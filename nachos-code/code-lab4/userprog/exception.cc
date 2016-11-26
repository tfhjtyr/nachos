// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "noff.h"
#include "memory.h"
#include "machine.h"
#include "bitmap.h"
#include "synch.h"
#include "thread.h"


static Semaphore *s = new Semaphore("suspend semaphore", 0);
Thread *suspendedThread = NULL;

TranslationEntry*
LRU()
{
	int min = 999999999;
	TranslationEntry *returnEntry = NULL;
	for(int i = 0; i < TLBSize; i++) {
		if(machine->tlb[i].valid == FALSE)
			return &machine->tlb[i];
		if(machine->tlb[i].lastUsedTime < min) {
			min = machine->tlb[i].lastUsedTime;
			returnEntry = &machine->tlb[i];
		}
	}
	ASSERT(returnEntry != NULL);
	return returnEntry;
}

TranslationEntry*
NFU()
{
	if(stats->totalTicks % 100 == 0)
		return &machine->tlb[0];
	if(stats->totalTicks % 101 == 0)
		return &machine->tlb[1];
	if(stats->totalTicks % 102 == 0)
		return &machine->tlb[2];
	if(stats->totalTicks % 103 == 0)
		return &machine->tlb[3];
	
	int min = 999999999;
	TranslationEntry *returnEntry = NULL;
	for(int i = 0; i < TLBSize; i++) {
		ASSERT(machine->tlb[i].numOfReference >= 0 && machine->tlb[i].numOfReference < 999999999);
		if(machine->tlb[i].valid == FALSE)
			return &machine->tlb[i];
		if(machine->tlb[i].numOfReference < min) {
			min = machine->tlb[i].numOfReference;
			returnEntry = &machine->tlb[i];
		}
	}
	ASSERT(returnEntry != NULL);
	return returnEntry;
}


TranslationEntry* 
findOneTLBToRelpace()
{
	/*for(int i = 0; i < TLBSize; i++)
		printf("\tTLB entry %d, vpn %d, reference %d\n", i, machine->tlb[i].virtualPage, machine->tlb[i].numOfReference);*/
	//return LRU();
	return NFU();
	//return &machine->tlb[0];
}

void
replaceTLB(TranslationEntry* entry)
{
	/*if(entry->valid == TRUE)
		printf("Kicking TLB with vpn %d\n", entry->virtualPage);
	else
		printf("Unused TLB");*/
	currentThread->space->pageTable[entry->virtualPage].dirty = entry->dirty;	//write back to page table
	
	entry->dirty = FALSE;
	entry->lastUsedTime = stats->totalTicks;
	entry->numOfReference = 1;
	entry->readOnly = FALSE;
	entry->use = FALSE;
	entry->valid = TRUE;
	entry->virtualPage = machine->registers[BadVAddrReg] / PageSize;

	AddrSpace *addrs = currentThread->space;
	if(addrs->pageTable[entry->virtualPage].valid == FALSE)
		ExceptionHandler(PageFaultException);
	entry->physicalPage = addrs->pageTable[entry->virtualPage].physicalPage;
}

int
findOnePageToRelpace()
{
	//DEBUG('a', "In findOnePageToReplace.\n");
	int ppn = memBitMap->findPage();  
	//write back
	if(memBitMap->myEntry[ppn] != NULL) {  //already mapped to an entry
		int vpn = memBitMap->myEntry[ppn]->virtualPage;
		Thread* t = (Thread*) memBitMap->myEntry[ppn]->thread;
		memcpy(t->space->diskSpace + vpn*PageSize, &(machine->mainMemory[ppn*PageSize]), PageSize);
		//doesn't neccessarily be currentThread!!!!!!!!!!!!!!!!!
		memBitMap->myEntry[ppn]->valid = FALSE;
		memBitMap->myEntry[ppn]->thread = NULL;

		for(int i = 0; i < TLBSize; i++)
			if(machine->tlb[i].virtualPage == memBitMap->myEntry[ppn]->virtualPage)
				machine->tlb[i].valid = FALSE;
	}
	if(ppn == -1) {
		ASSERT(FALSE);
	}

	return ppn;
}

void replacePage(int ppn)
{
	//DEBUG('a', "In replacePage.\n");
	int vpn = (machine->ReadRegister(BadVAddrReg)) / PageSize;
	//printf("Mapping vpn %d to ppn %d, ppn is last used at %d ticks\n", vpn, ppn, memBitMap->myEntry[ppn] == NULL ? 0 : memBitMap->myEntry[ppn]->lastUsedTime);
	//for(int i = 0; i < NumPhysPages; i++)
		//printf("ppn %d %d; ", i, memBitMap->myEntry[i] == NULL ? 0 : memBitMap->myEntry[i]->lastUsedTime);
	//printf("\n");
	TranslationEntry *entry = &currentThread->space->pageTable[vpn];
	entry->physicalPage = ppn;
	entry->dirty = FALSE;
	entry->lastUsedTime = stats->totalTicks;
	entry->numOfReference = 1;
	entry->readOnly = FALSE;
	entry->use = FALSE;
	entry->valid = TRUE;

	memBitMap->myEntry[ppn] = entry;
	memBitMap->myEntry[ppn]->thread = (void*) currentThread;
	memcpy(&(machine->mainMemory[ppn*PageSize]), currentThread->space->diskSpace + vpn*PageSize, PageSize);
	
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
	
	DEBUG('a', "In ExceptionHandler.\n");
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
		DEBUG('a', "Shutdown, initiated by user program.\n");
   		interrupt->Halt();
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
    } 
	else if((which == SyscallException) && (type == SC_Exit)) {
		DEBUG('a', "Exit, initiated by user program.\n");
		int exitCode = machine->registers[4];
		printf("Exit with %d\n", exitCode);
		//interrupt->Halt();
		currentThread->Finish();
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Yield)) {
		DEBUG('a', "Yield, initiated by user program.\n");
		printf("%s yields, initiated by user program.\n", currentThread->getName());
		currentThread->Yield();
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Read)) {
		DEBUG('a', "Read, initiated by user program.\n");
		printf("%s is blocked because of reading operation...\n", currentThread->getName());
		suspendedThread = currentThread;
		s->P();
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Write)) {
		DEBUG('a', "Write, initiated by user program.\n");
		while(suspendedThread == NULL)
			currentThread->Yield();
		
		printf("Suspending %s...\n", suspendedThread->getName());
		currentThread->suspend(suspendedThread);
		s->V();
		printf("%s is ready again...\n", suspendedThread->getName());
		suspendedThread = NULL;
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if(which == PageFaultException) {
		DEBUG('a', "PageFaultException, initiated by user program.\n");
		stats->numPageFaults++;
		int ppn = findOnePageToRelpace();
		replacePage(ppn);
	}
	else if(which == TLBMissException) {
		DEBUG('a', "TLBMissException, initiated by user program.\n");
		stats->numTLBmiss++;
		TranslationEntry *entry = findOneTLBToRelpace();
		replaceTLB(entry);
	}
	else {
		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
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

#include "stdlib.h"
#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "noff.h"
#include "memory.h"
#include "machine.h"
#include "bitmap.h"
#include "synch.h"
#include "thread.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "system.h"
#include "dirent.h"

extern bool StartProcess(char* filename);
extern Thread* threadList[MaxThreadNumber];

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

void func_Exec(int filename)
{
	//printf("%s executes a file named %s\n", currentThread->getName(), filename);
	StartProcess((char*)filename);
}

void func_Fork(int funcPointer)
{   
    currentThread->space = threadList[0]->space;
	currentThread->executable = threadList[0]->executable;
    //delete executable;			// close file

    machine->WriteRegister(PCReg, funcPointer);
	machine->WriteRegister(NextPCReg, funcPointer+4);

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

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
		//printf("%s exits with %d\n", currentThread->getName(), exitCode);
		//interrupt->Halt();
		currentThread->Finish();
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Yield)) {
		DEBUG('a', "Yield, initiated by user program.\n");
		//printf("%s yields, initiated by user program.\n", currentThread->getName());
		currentThread->Yield();
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Create)) {
		DEBUG('a', "Create, initiated by user program.\n");
		int nameAddr = machine->ReadRegister(4);
		char* name = new char[20];
		int index = 0;
		int value;
		while(1) {
			while(!machine->ReadMem(nameAddr, 1, &value));
			nameAddr++;
			name[index++] = *((char*)(&value));
			//printf("%c", name[index-1]);
			if(name[index-1] == '\0')
				break;
		}
		//printf("%s creates a new file named %s\n", currentThread->getName(), name);
		fileSystem->Create(name,1000);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_CreateDir)) {
		DEBUG('a', "Create, initiated by user program.\n");
		int nameAddr = machine->ReadRegister(4);
		char* name = new char[20];
		int index = 0;
		int value;
		while(1) {
			while(!machine->ReadMem(nameAddr, 1, &value));
			nameAddr++;
			name[index++] = *((char*)(&value));
			//printf("%c", name[index-1]);
			if(name[index-1] == '\0')
				break;
		}
		mkdir(name, S_IRUSR | S_IWUSR);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_RemoveDir)) {
		DEBUG('a', "Open, initiated by user program.\n");
		int nameAddr = machine->ReadRegister(4);
		char* name = new char[20];
		int index = 0;
		int value;
		while(1) {
			while(!machine->ReadMem(nameAddr, 1, &value));
			nameAddr++;
			name[index++] = *((char*)(&value));
			//printf("%c", name[index-1]);
			if(name[index-1] == '\0')
				break;
		}
		char* s = new char[100];
		strcpy(s, "rm -r ");
		strcpy(s+6, name);
		system(s);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Open)) {
		DEBUG('a', "Open, initiated by user program.\n");
		int nameAddr = machine->ReadRegister(4);
		char* name = new char[20];
		int index = 0;
		int value;
		while(1) {
			while(!machine->ReadMem(nameAddr, 1, &value));
			nameAddr++;
			name[index++] = *((char*)(&value));
			//printf("%c", name[index-1]);
			if(name[index-1] == '\0')
				break;
		}
		int id = OpenForReadWrite(name, FALSE);
		machine->WriteRegister(2, id);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Close)) {
		DEBUG('a', "Open, initiated by user program.\n");
		OpenFileId id = machine->ReadRegister(4);
		//printf("%s closes a file with fid %d\n", currentThread->getName(), 0);
		Close(id);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Remove)) {
		DEBUG('a', "Remove, initiated by user program.\n");
		int nameAddr = machine->ReadRegister(4);
		char* name = new char[20];
		int index = 0;
		int value;
		while(1) {
			while(!machine->ReadMem(nameAddr, 1, &value));
			nameAddr++;
			name[index++] = *((char*)(&value));
			//printf("%c", name[index-1]);
			if(name[index-1] == '\0')
				break;
		}
		Unlink(name);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Read)) {
		DEBUG('a', "Read, initiated by user program.\n");
		int buffer = machine->ReadRegister(4);
		int size = machine->ReadRegister(5);
		OpenFileId id = machine->ReadRegister(6);
		char* bf = new char[size];
		
		Read(id, bf, size);
		int readCount = size;
		//printf("%s reads %s\n", currentThread->getName(), bf);
		for(int i = 0; i < size; i++)
			while(!machine->WriteMem(buffer+i, 1, (int)bf[i]));
			
		machine->WriteRegister(2, readCount);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Write)) {
		DEBUG('a', "Write, initiated by user program.\n");
		int buffer = machine->ReadRegister(4);
		int size = machine->ReadRegister(5);
		OpenFileId id = machine->ReadRegister(6);
		char* bf = new char[size];
		for(int i = 0; i < size; i++)
			while(!machine->ReadMem(buffer+i, 1, (int*)(bf+i)));
		//printf("%s writes %s\n", currentThread->getName(), bf);
		//int writeCount = currentThread->openfile->WriteAt(bf, size, 0);
		WriteFile(id, bf, size);
		int writeCount = size;
		machine->WriteRegister(2, writeCount);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Seek)) {
		OpenFileId id = machine->ReadRegister(4);
		int size = machine->ReadRegister(5);
		Lseek(id, size, SEEK_CUR);
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Exec)) {
		DEBUG('a', "Exec, initiated by user program.\n");
		int nameAddr = machine->ReadRegister(4);
		char* filename = new char[20];
		int index = 0;
		int value;
		while(1) {
			while(!machine->ReadMem(nameAddr, 1, &value));
			nameAddr++;
			filename[index++] = *((char*)(&value));
			if(filename[index-1] == '\0')
				break;
		}
		
		Thread *t = new Thread("forked thread 2");
   		t->Fork(func_Exec, (int)filename);

		SpaceId id = t->getTID();
		machine->WriteRegister(2, id);			
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Join)) {
		DEBUG('a', "Join, initiated by user program.\n");
		SpaceId id = machine->ReadRegister(4);
		while(threadList[id] != NULL) {
			//printf("%s yields waiting for %s\n", currentThread->getName(), threadList[id]->getName());
			currentThread->Yield();
		}
		
		machine->registers[PCReg] = machine->registers[NextPCReg];
		machine->registers[NextPCReg] = machine->registers[PCReg] + 4; 
	}
	else if((which == SyscallException) && (type == SC_Fork)) {
		DEBUG('a', "Fork, initiated by user program.\n");
		int funcPointer = machine->ReadRegister(4);
		Thread *t = new Thread("forked thread 1");
		//printf("%s forks %s\n", currentThread->getName(), t->getName());
   		t->Fork(func_Fork, funcPointer);
		
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
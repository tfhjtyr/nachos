// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

/*void
Semaphore::P()			//must minus value first, otherwise if the waken thread is not scheduled on CPU, V() will increase value to an undesired positive number
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts

	value--; 					// semaphore available, 
						// consume its value
    while (value < 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}*/


//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) 
{
	name = debugName;
	lock = new Semaphore("lock semaphore", 1);
	holdingThread = NULL;
}
Lock::~Lock() 
{
	delete lock;
}
void Lock::Acquire() 
{
	lock->P();
	holdingThread = currentThread;
}
void Lock::Release() 
{
	if(isHeldByCurrentThread()) {	//in readwriteLock, lock must be able to be released by another thread.
		lock->V();
		holdingThread = NULL;
	}
}

bool Lock::isHeldByCurrentThread()
{
	if(holdingThread == currentThread)
		return true;
	else
		return false;
}


Condition::Condition(char* debugName)
{
	name = debugName;
	condition = new Semaphore("condition Semaphore", 0);
	waitingNumber = 0;
}
Condition::~Condition()
{
	delete condition;
}
void Condition::Wait(Lock* conditionLock) 
{
	waitingNumber++;
	conditionLock->Release();
	condition->P();
	conditionLock->Acquire();
}
void Condition::Signal(Lock* conditionLock)
{
	if(waitingNumber > 0) {
		condition->V();
		waitingNumber--;
	}
}
void Condition::Broadcast(Lock* conditionLock) 
{
	while(waitingNumber) {
		condition->V();
		waitingNumber--;
	}
}

Barrier::Barrier()
{
	expectedNum = 0;
	arrivedNum = 0;
	s = new Semaphore("Barrier Semaphore", 0);
	mutex = new Semaphore("Mutex Semaphore", 1);
}

Barrier::~Barrier()
{
	delete s;
}

void 
Barrier::setBarrier(int num)
{
	mutex->P();
	arrivedNum = 0;
	expectedNum = num;
	mutex->V();
}

void 
Barrier::waitOnBarrier()
{
	mutex->P();
	arrivedNum++;
	if(arrivedNum == expectedNum)
	{
		mutex->V();
		for(int i = 0; i < expectedNum - 1; i++) {
			s->V();
		}
	}
	else
	{
		mutex->V();
		s->P();
	}
}

ReadWriteLock::ReadWriteLock()
{
	mutex = new Lock("Mutex Lock");
	w = new Lock("write Lock");
	r = new Lock("Read Lock");
	rc = 0;
}

ReadWriteLock::~ReadWriteLock()
{
	delete mutex;
	delete w;
	delete r;
}

void
ReadWriteLock::prepareToRead()
{
	r->Acquire();
	mutex->Acquire();
	rc++;
	if(rc == 1)
		w->Acquire();
	mutex->Release();
	r->Release();
}
	
void
ReadWriteLock::finishRead()
{
	mutex->Acquire();
	rc--;
	if(rc == 0)
		w->Release();
	mutex->Release();
}
void
ReadWriteLock::prepareToWrite()
{
	r->Acquire();
	w->Acquire();
	r->Release();
}
void
ReadWriteLock::finishWrite()
{
	w->Release();
}


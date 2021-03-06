// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"

// testnum is set in main.cc
int testnum = 1;


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------
void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, 1);
	
    SimpleThread(0);
}


void 
myThreadTest()
{
	DEBUG('t', "Entering myThreadTest");
	
	Thread *t1 = new Thread("thread1");
    t1->Fork(SimpleThread, 1);
	Thread *t2 = new Thread("thread2");
    t2->Fork(SimpleThread, 2);
	Thread *t3 = new Thread("thread3");
    t3->Fork(SimpleThread, 3);
	TS();
	SimpleThread(0);
	currentThread->Yield();
	
	Thread *t4 = new Thread("thread4");
    t4->Fork(SimpleThread, 4);
	TS();
}



//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
		ThreadTest1();
		break;
	case 2:
		myThreadTest();
		break;
    default:
		printf("No test specified.\n");
		break;
    }
}


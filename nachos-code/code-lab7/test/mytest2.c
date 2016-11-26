#include "syscall.h"

int spaceId;
char* executable;

void func()
{
	spaceId = Exec(executable);
	Join(spaceId);	
	Exit(0);
}

int 
main()
{
	executable = "mytest";
	Fork(func);
	Yield();
	Exit(0);
}




/*int 
main()
{
	int spaceId;
	spaceId = Exec("mytest");	
	Join(spaceId);
	Exit(0);
}*/

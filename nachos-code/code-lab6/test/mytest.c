#include "syscall.h"

int 
main()
{
	int fid;
	char buffer[10];
	Create("testfile");
	fid = Open("testfile");
	Write("123", 3, fid);
	Read(buffer, 3, fid);
	Close(fid);
	Exit(0);
}

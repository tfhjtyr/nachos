#include "syscall.h"

int 
main()
{
	int shmid;
	char* addr;
	shmid = Shmget(0, 4);
	addr = Shmat(shmid);
	addr[0] = 'a';
	addr[1] = 'b';
	addr[2] = 'c';
	addr[3] = '\n';
	Shmdl(addr);
	Exit(0);
}

/*int fid;
	char buffer[10];
	Create("testfile");
	fid = Open("testfile");
	Write("123", 3, fid);
	Seek(fid, -3);
	Read(buffer, 3, fid);
	Close(fid);*/

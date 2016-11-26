#include "syscall.h"

char A[1024];

int 
main()
{
	/*int i;
	for(i = 0; i < 3; i++) {
		A[i] = 1;
		Yield();
	}

	Exit(2);*/

	int i = 0;
	while(i < 10) {
		i++;
	}
	Write(0, 0, 0);
	Yield();
	Exit(i-8);
}

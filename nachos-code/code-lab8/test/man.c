#include "syscall.h"

int
main()
{
	OpenFileId output = ConsoleOutput;
    	Write("type 'man' to see manual.\n", sizeof("type 'man' to see manual.\n"), output);
	Write("type './executable' to execute a file named executable.\n", sizeof("type './executable' to execute a file named executable.\n"), output);
	Write("type 'mf + filename' to create a file with name filename\n", sizeof("type 'mf + filename' to create a file with name filename\n"), output);
	Write("type 'df + filename' to delete a file with name filename.\n", sizeof("type 'df + filename' to delete a file with name filename\n"), output);
	Write("type 'md + dirname' to create a directory with name dirname\n", sizeof("type 'md + dirname' to create a directory with name dirname\n"), output);
	Write("type 'rd + dirname' to remove a directory with name dirname\n", sizeof("type 'rd + dirname' to remove a directory with name dirname\n"), output);
}

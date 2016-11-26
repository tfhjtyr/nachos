#include "syscall.h"

int
main()
{
    SpaceId newProc;
    OpenFileId input = ConsoleInput;
    OpenFileId output = ConsoleOutput;
    OpenFileId fid;
    char prompt[2], ch, buffer[60];
    int i;

    prompt[0] = '-';
    prompt[1] = '$';

    while( 1 )
    {
	Write(prompt, 2, output);

	i = 0;
	
	do {
	
	    Read(&buffer[i], 1, input); 

	} while( buffer[i++] != '\n' );
	
	buffer[--i] = '\0';

	if( i > 0 ) {
		if(buffer[0] == '.' && buffer[1] == '/') {
			newProc = Exec(&buffer[2]);
			Join(newProc);
		}
		if(buffer[0] == 'm' && buffer[1] == 'a' && buffer[2] == 'n') {
			newProc = Exec("man");
			Join(newProc);
		}
		else if(buffer[0] == 'm' && buffer[1] == 'f') {
			Create(&buffer[3]);
		}
		else if(buffer[0] == 'r' && buffer[1] == 'f') {
			Remove(&buffer[3]);
		}
		else if(buffer[0] == 'm' && buffer[1] == 'd') {
			CreateDir(&buffer[3]);
		}
		else if(buffer[0] == 'r' && buffer[1] == 'd') {
			RemoveDir(&buffer[3]);
		}
		else if(buffer[0] == 'e' && buffer[1] == 'c' && buffer[2] == 'h' && buffer[3] == 'o') {
			Write(&buffer[5], i-4, output);
			Write("\n", 1, output);
		}
		else {
			//Write("Command not found!\n", sizeof("Command not found!\n"), output);
		}
		
		for(i = 0; buffer[i] != '\0'; i++) {
			if(buffer[i] == ' ' && buffer[i+1] == '>') {
				buffer[i] = '\0';	
				Create(buffer+i+3);
				fid = Open(buffer+i+3);
				Write(buffer, i, fid);
			}
		}
	}
    }
}


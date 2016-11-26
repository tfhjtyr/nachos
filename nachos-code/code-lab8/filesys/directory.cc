// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"
#include "system.h"
#include "openfile.h"
//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++){
		table[i].inUse = FALSE;
		table[i].name = NULL;
    }
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
	//printf("In FetchFrom()\n");
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
	//printf("End reading\n");
	int offset = tableSize * sizeof(DirectoryEntry);
	for(int i = 0; i < tableSize; i++) {
		if(table[i].inUse == TRUE) {
			int len = (int)table[i].name;
			table[i].name = new char[len + 1];
			(void) file->ReadAt(table[i].name, len+1, offset);
			offset += len + 1;
		}
	}
	//printf("Out FetchFrom()\n");
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
	char** names = new char*[tableSize]; 
	for(int i = 0; i < tableSize; i++)
		names[i] = table[i].name;

	for(int i = 0; i < tableSize; i++) {
		if(table[i].inUse == TRUE) {
			int len = strlen(table[i].name);
			table[i].name = (char*)len;
		}
	}

	(void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
	
	int offset = tableSize * sizeof(DirectoryEntry);
	for(int i = 0; i < tableSize; i++) {
		if(table[i].inUse == TRUE) {
			file->WriteAt(names[i],strlen(names[i])+1, offset);
			//names[i] = new char[strlen(names[i])+1];
			offset += strlen(names[i])+1;
		}
	}  
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
    for (int i = 0; i < tableSize; i++) {
		if(table[i].inUse == TRUE) {
			ASSERT(table[i].name != NULL);
		}
        if (table[i].inUse && (strlen(name) == strlen(table[i].name)) && !strncmp(table[i].name, name, strlen(name))) {
			table[i].lastReferenced = stats->totalTicks;
	    	return i;
        }
    }
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
	return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector, bool isDirectory)
{ 
	printf("Adding %s %s\n", isDirectory?"Directory":"File", name);
    if (FindIndex(name) != -1)
	return FALSE;

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
			table[i].name = name;
            //strncpy(table[i].name, name, strlen(name)+1); 
            table[i].sector = newSector;
			if(isDirectory)
				table[i].fileType = DIRECTORY;
			else
				table[i].fileType = DATAFILE;
			
			table[i].FID = globalFID++;
		
			printf("File %s %s is added.\n", isDirectory?"Directory":"File", name);
        	return TRUE;
	}
    return FALSE;	// no space.  Fix when we have extensible files.
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{ 
	printf("Removing file %s\n", name);
    int i = FindIndex(name);

    if (i == -1)
	return FALSE; 		// name not in directory
    table[i].inUse = FALSE;
	table[i].name = NULL;
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
   for (int i = 0; i < tableSize; i++)
	if (table[i].inUse) {
	    printf("File name : %s\t", table[i].name);
		printf("File type: %d\t", table[i].fileType);
		printf("Date created : %d\t", table[i].dateCreated);
		printf("Last referenced : %d\t", table[i].lastReferenced);
		printf("Last modified : %d\n", table[i].lastModified);
		//printf("Path : /\n");

		if(table[i].fileType == DIRECTORY) {
			printf("-----------------------------------------\n");
			fileSystem->formerDirectory = fileSystem->directoryFile;
			Directory *directory = new Directory(NumDirEntries);
			directory->FetchFrom(fileSystem->directoryFile);
			int sector = directory->Find(table[i].name);
			ASSERT(sector != -1);
			fileSystem->directoryFile = new OpenFile(sector, table[i].name);
			directory->FetchFrom(fileSystem->directoryFile);
			directory->List();
			delete directory;
			//delete fileSystem->directoryFile; ???????????
			fileSystem->directoryFile = fileSystem->formerDirectory;
			printf("----------------------------------------\n");
		}
	}
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    for (int i = 0; i < tableSize; i++)
	if (table[i].inUse) {
	    printf("Name: %s, Sector: %d\n", table[i].name, table[i].sector);
	    hdr->FetchFrom(table[i].sector);
	    hdr->Print();
	}
    printf("\n");
    delete hdr;
}

void 
Directory::setDataCreated(char* name) 
{ 
	int i = FindIndex(name);
	ASSERT(i != -1);
	table[i].dateCreated = stats->totalTicks; 
}

void 
Directory::updateLastReferenced(char* name) 
{ 
	int i = FindIndex(name);
	ASSERT(i != -1);
	table[i].lastReferenced = stats->totalTicks; 
}

bool 
Directory::updateLastModified(char* name) 
{
	int i = FindIndex(name);
	if(i == -1)
		return false;
	table[i].lastModified = stats->totalTicks; 
	return true;
}


int
Directory::getFID(char *name)
{
    for (int i = 0; i < tableSize; i++) {
		if(table[i].inUse == TRUE) {
			ASSERT(table[i].name != NULL);
		}
        if (table[i].inUse && (strlen(name) == strlen(table[i].name)) && !strncmp(table[i].name, name, strlen(name))) {
			return table[i].FID;
        }
    }
	printf("getFID() returns -1!!!!!\n");
    return -1;
}





#define _GNU_SOURCE

#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
								// so we need to define what delimits our tokens.
								// In this case  white space
								// will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

#include<stdlib.h>
#include<stdint.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>
#include<time.h>
#include <assert.h>

#define NUM_BLOCKS 4226
#define BLOCK_SIZE 8192
#define NUM_FILES  128
#define MAX_FILE_SIZE 10240000

FILE * fd;
int data =0;

char *token[MAX_NUM_ARGUMENTS];

uint8_t blocks[NUM_BLOCKS][BLOCK_SIZE];


/*Structure to save file information*/
struct Directory_Entry
{
	uint8_t  valid;
	char     filename[255];
	uint32_t inode;
	char time[65];

};


/*Using inode to implement index allocation file system*/
struct Inode
{
	uint8_t valid;
	uint8_t attributes;
	uint32_t size;
	uint32_t blocks[1250];
	
};

struct Directory_Entry * dir;
struct Inode * inodes;

uint8_t *freeBlockList;
uint8_t *freeInodeList;


void initializeBlockList()
{
	int i;
	for(i=0; i<NUM_BLOCKS; i++)
	{
		
		freeBlockList[i]=1;
	}
	
}

void initializeInodes()
{
	int i;
	for(i=0; i<NUM_FILES; i++)
	{
		inodes[i].valid=0;
		inodes[i].attributes=0;
		inodes[i].size=0;
		
		int j;
		for (j=0; j<1250; j++)
		{
			inodes[i].blocks[j] = -1;
		}
	}
}

void initializeInodeList()
{
	int i;
	for(i=0; i<NUM_FILES; i++)
	{
		freeInodeList[i]=1;
	}
}

void initializeDirectory()
{
	int i;
	for(i=0; i<NUM_FILES; i++)
	{
		dir[i].valid = 0;
		dir[i].inode =-1;
		memset(dir[i].filename, 0, 255);
		memset(dir[i].time, 0, 64);
	}
	
} 

/*To diplay the available disk space starting from block 10 to avoid inodes*/
/*return value: available disk space*/
int df()
{
	int i;
	int free_space = 0;
	for(i=10; i<NUM_BLOCKS; i++)
	{
		if(freeBlockList[i]==1)
		{
			free_space = free_space + BLOCK_SIZE;
		}		
	}
	return free_space;
}
/*return value: index of freeblock found
sets free to not free after finding freeblock
if failed returns -1 */
int findFreeBlock()
{
	int i;
	int ret = -1;
	

	for(i=10; i<NUM_BLOCKS; i++)
	{
		if(freeBlockList[i]==1)
		{   
			freeBlockList[i]= 0;
			return i;
		}
	}
	
	return ret;

}

/*return value: index of freeInode found
sets validity(0) to not valid(1) after finding freeInode
if failed returns -1 */
int findFreeInode()
{
	int i;
	int ret = -1;

	for(i=0; i<NUM_FILES; i++)
	{
		if(inodes[i].valid==0)
		{
			inodes[i].valid = 1;
			return i;
		}
	}

	return ret;

}

/*arg: filename
return value: index for file by comapring existing files
			: index of next valid block
if failed returns -1 */
int findDirectoryEntry(char * filename)
{
	// check for existing entry

	int i;
	int ret = -1;
	for(i=0; i<NUM_FILES; i++)
	{
		if(strcmp( filename, dir[i].filename)==0)
		{
			return i;
		}
	}
	//check for space
	for(i=0; i<NUM_FILES; i++)
	{
		if( dir[i].valid == 0)
		{
			dir[i].valid = 1;
			return i;
		} 
	}

	return ret;

}
/*arg: filename
  return value: -1 if failed
  puts our file from current working directoey to the filesystem*/
int put(char * filename)
{

	struct stat buf;
	int ret;
	int lengthoffilename;

	/*timestamping file by using inbuilt functions time
	  ,localtime and struct tm provided by c
	  include header file time.h */
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	/* stat gives information about file
	   return value:-1 if failed */
	ret = stat(filename, &buf);


	if(ret == -1)
	{
		printf("put error: file not found\n");
		return -1;
	}

	int size = buf.st_size;
	lengthoffilename = strlen(filename);
	/*Filename should not exceed 255 characters*/
	if(lengthoffilename > 255)
	{
		printf("put error: FILE NAME TOO LONG\n");
		return -1;
	}
	
	
	
	if(size > MAX_FILE_SIZE)
		{
		printf("put error:file size too big\n");
		return -1;   
		}

	if (size > df() )
		{
			printf("put error: Not enough disk space\n");
			return -1;
		}

	//Put file into image

	int directoryIndex = findDirectoryEntry(filename);
	int inodeIndex = findFreeInode();
	
	/*Copying the filename from the commandline to the obtained directoryIndex
	  Copying the timestamp string into the file structure to the obtained directoryIndex*/
	strncpy(dir[directoryIndex].filename,token[1],sizeof(token[1]));
	assert(strftime(dir[directoryIndex].time, sizeof(dir[directoryIndex].time), "%c", tm));
	
	/*Saving filesize and its inode number*/
	dir[directoryIndex].inode=inodeIndex;
	inodes[inodeIndex].size= buf.st_size;
	
	/*Sorting the changes caused by deleting the file to print in list*/
	if(dir[directoryIndex].valid==0)
	{
		
		dir[directoryIndex].valid=1;
	}
	



  // If stat did not return -1 then we know the input file exists and we can use it.
  if( ret != -1 )
  {
 
	// Open the input file read-only 
	FILE *ifp = fopen ( token[1], "r" ); 
	printf("Reading %d bytes from %s\n", (int) buf . st_size, token[1] );
 
	// Save off the size of the input file since we'll use it in a couple of places and 
	// also initialize our index variables to zero. 
	//int copy_size   = buf . st_size;

	// We want to copy and write in chunks of BLOCK_SIZE. So to do this 
	// we are going to use fseek to move along our file stream in chunks of BLOCK_SIZE.
	// We will copy bytes, increment our file pointer by BLOCK_SIZE and repeat.
	int offset      = 0;               

	// We are going to copy and store our file in BLOCK_SIZE chunks instead of one big 
	// memory pool. Why? We are simulating the way the file system stores file data in
	// blocks of space on the disk. block_index will keep us pointing to the area of
	// the area that we will read from or write to.
	//int block_index = 0;
 
	// copy_size is initialized to the size of the input file so each loop iteration we
	// will copy BLOCK_SIZE bytes from the file then reduce our copy_size counter by
	// BLOCK_SIZE number of bytes. When copy_size is less than or equal to zero we know
	// we have copied all the data from the input file.
	while( size > 0 )
	{
		int freeblock = findFreeBlock();
		data = freeblock;
		


	  // Index into the input file by offset number of bytes.  Initially offset is set to
	  // zero so we copy BLOCK_SIZE number of bytes from the front of the file.  We 
	  // then increase the offset by BLOCK_SIZE and continue the process.  This will
	  // make us copy from offsets 0, BLOCK_SIZE, 2*BLOCK_SIZE, 3*BLOCK_SIZE, etc.
	  fseek( ifp, offset, SEEK_SET );
 
	  // Read BLOCK_SIZE number of bytes from the input file and store them in our
	  // data array. 
	  int bytes  = fread( blocks[freeblock], BLOCK_SIZE, 1, ifp );

	  // If bytes == 0 and we haven't reached the end of the file then something is 
	  // wrong. If 0 is returned and we also have the EOF flag set then that is OK.
	  // It means we've reached the end of our input file.
	  if( bytes == 0 && !feof( ifp ) )
	  {
		printf("An error occured reading from the input file.\n");
		return -1;
	  }

	  // Clear the EOF file flag.
	  clearerr( ifp );

	  // Reduce copy_size by the BLOCK_SIZE bytes.
	  size -= BLOCK_SIZE;
	  
	  // Increase the offset into our input file by BLOCK_SIZE.  This will allow
	  // the fseek at the top of the loop to position us to the correct spot.
	  offset    += BLOCK_SIZE;

	  
	}
	

	// We are done copying from the input file so close it out.
	fclose( ifp );
   }
}

/* gets the file from the filesystem and places in current working directory
   args : filename or newfilename to get output file as newfilename
   returns 0 if file found.
   returns -1 if fileopening fialed */


int get(char * filename,char * newfilename )
{
	struct stat buf;
	int offset;
	int test;
	int testsize;
	int geterror=1;
	int i;
	FILE *ofp;
	
	/*Handling the NULL tokens from commandline
	  to change the name of output file accordingly */
	if(token[1] !=NULL && token[2]==NULL)
		{
			ofp = fopen(token[1], "w");
		}
	else if(token[1]!=NULL &&token[2] !=NULL)
		{
			ofp = fopen(token[2], "w");
		}
		

	if( ofp == NULL )
	{
	  printf("Could not open output file: %s\n", token[1] );
	  perror("Opening output file returned");
	  return -1;
	}


	
	for(i=0; i<NUM_FILES; i++)
	{

		if(!strcmp(dir[i].filename,filename))
			{
				/* getting the directoryIndex for a file through i 
				  saving the InodeIndex in test and file size in test size 
				  to copy the exact number of bytes when get is performed. */
				test=dir[i].inode;
				testsize=inodes[test].size;
				geterror=0;

			}
				
	}
	/*Using geterror as a flag to proceed only if the filename exists */
	if(!geterror)
	{
		int size   = testsize;
		offset      = 0;
		if(newfilename!=NULL)
			{
				printf("Writing %d bytes to %s\n", testsize, token[2]);
			}
		else
			{
				printf("Writing %d bytes to %s\n", testsize, token[1] );
			}

	  // Using copy_size as a count to determine when we've copied enough bytes to the output file.
	  // Each time through the loop, except the last time, we will copy BLOCK_SIZE number of bytes from
	  // our stored data to the file fp, then we will increment the offset into the file we are writing to.
	  // On the last iteration of the loop, instead of copying BLOCK_SIZE number of bytes we just copy
	  // how ever much is remaining ( copy_size % BLOCK_SIZE ).  If we just copied BLOCK_SIZE on the
	  // last iteration we'd end up with gibberish at the end of our file. 
		while( size > 0 )
			{
				
				int num_bytes;

			   // If the remaining number of bytes we need to copy is less than BLOCK_SIZE then
			   // only copy the amount that remains. If we copied BLOCK_SIZE number of bytes we'd
			   // end up with garbage at the end of the file.
				if( size < BLOCK_SIZE )
					{
						num_bytes = size;
					}
				else
				{
					num_bytes = BLOCK_SIZE;
					
				}
			   // Write num_bytes number of bytes from our data array into our output file.
				fwrite( blocks[data], num_bytes, 1, ofp ); 
				// Reduce the amount of bytes remaining to copy, increase the offset into the file
				// and increment the block_index to move us to the next data block.
				size -= BLOCK_SIZE;
				offset    += BLOCK_SIZE;
				// Since we've copied from the point pointed to by our current file pointer, increment
				// offset number of bytes so we will be ready to copy to the next area of our output file.
				fseek( ofp, offset, SEEK_SET );
			}

			fclose(ofp);

   
	}
	else
	{
		printf("get error: File not found\n");
	}
}



/* lists the available files in the filesystem
   returns 1 for each time a file is found in the system
   returns 0 if file doesnt exist */
int list()
{   
	int i;
	int val=0;
	for( i=0; i<NUM_FILES; i++ )
	{
		if(dir[i].valid )
		{
			int inode_idx = dir[i].inode;
			if(inodes[inode_idx].attributes!=1)
			{
						   
				printf("%d  %s  %s\n",inodes[inode_idx].size,dir[i].time,dir[i].filename);
				val=1;

			}

			
		}


	}
	return val;
}
/* creates our filesystem 
   arg: filesystem name */
void createfs(char * filename)
{

	fd = fopen(filename, "w" );

	memset(&blocks[0],0,NUM_BLOCKS * BLOCK_SIZE);
	initializeDirectory();
	
	initializeBlockList();
	
	initializeInodeList();
	
	initializeInodes();

	fwrite (&blocks[0], NUM_BLOCKS,BLOCK_SIZE, fd);

	fclose(fd);
}

/* opens the filesystem and reads into it
   arg: existing filesystem name */
void open(char * filename)
{

	fd = fopen(filename,"r");
	if(fd==NULL)
	{
		printf("open: FILE NOT FOUND\n");
		return;
	}
	fread( &blocks[0],NUM_BLOCKS,BLOCK_SIZE,fd);
	fclose(fd);

}

/* closes the filesystem and writes into it
   arg: existing filesystem name */
void closeit(char * filename)
{
	
	fd = fopen(filename,"w");

	fwrite( &blocks[0],NUM_BLOCKS,BLOCK_SIZE,fd);
	fclose(fd);


}
/* Deletes the file from the filesystem by making it invalid
   arg: existing filename
   return value is 1 if successful or an attempt to delete a read-only file */
int delete(char * filename)
{
	int i;
	for( i=0; i<NUM_FILES; i++ )
	{
		if(dir[i].valid && !strcmp(filename,dir[i].filename))
		{
			int inode_idx = dir[i].inode;
			if(inodes[inode_idx].attributes==2)
			{
				printf("THAT FILE IS MARKED READ-ONLY\n");
				return 1;
			}
			else
			
			{
				printf("FILE DELETED\n");
				dir[i].valid=0;
				return 1;
			}

			
		}

	}
}
/* makes a file hidden or read only also undo changes
args: operation to perform '+h','-h','+r','-r' and existing filename */
void attribute(char* attr,char* filename)
{
	int i;
	int setattr;
	int readattrib;

	for( i=0; i<NUM_FILES; i++ )
	{
		
		if(dir[i].valid )
		{
			if(!strcmp(dir[i].filename,filename))
			{
				if(!strcmp(attr,"+h"))
					{
						setattr=dir[i].inode;
						printf(" FILE MARKED AS HIDDEN\n");
						/* using 1 for hidden */
						inodes[setattr].attributes= 1;
						break;  
				
					}

				else if(!strcmp(attr,"-h"))
				   {
					   printf("MARKED AS UNHIDDEN\n");
					   setattr=dir[i].inode;
					   /* using 0 for hidden */
					   inodes[setattr].attributes= 0;
					   break;

				   }

				else if(!strcmp(attr,"+r"))
				   {
					   printf("FILE MARKED AS READ-ONLY\n");
					   readattrib=dir[i].inode;
					   /* using 2 for hidden */
					   inodes[readattrib].attributes= 2;
					   break;

				   }
				else if(!strcmp(attr,"-r"))
				  {
					  printf("UNMARKED AS READ-ONLY\n");
					  readattrib=dir[i].inode;
					  /* using 3 for hidden */
					  inodes[readattrib].attributes= 3;
					  break;

				  }

			}
			
			
		}
		else
		{
			printf("attrib: FILE NOT FOUND\n");
			break;


		}
	}
}


int main()
{
	
	dir = (struct Directory_Entry * )&blocks[0];
	inodes = (struct Inode * )&blocks[9];

	freeInodeList = (uint8_t * )&blocks[7];
	freeBlockList = (uint8_t * )&blocks[8];

	initializeDirectory();
	
	initializeBlockList();
	
	initializeInodeList();
	
	initializeInodes();


	dir[0].valid = 1;
	dir[1].valid = 1;
	dir[127].valid = 1;

	 char forclose[100];
	 memset(forclose,0,100);

	
	

	char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );

	while( 1 )
  {
	// Print out the mfs prompt
	printf ("mfs> ");

	// Read the command from the commandline.  The
	// maximum command that will be read is MAX_COMMAND_SIZE
	// This while command will wait here until the user
	// inputs something since fgets returns NULL when there
	// is no input
	while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );

	/* Parse input */
	

	int   token_count = 0;                                 
														   
	// Pointer to point to the token
	// parsed by strsep
	char *arg_ptr;                                         
														   
	char *working_str  = strdup( cmd_str );                

	// we are going to move the working_str pointer so
	// keep track of its original value so we can deallocate
	// the correct amount at the end
	char *working_root = working_str;

	// Tokenize the input stringswith whitespace used as the delimiter
	while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) && 
			  (token_count < MAX_NUM_ARGUMENTS))
	{
	  token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
	  if( strlen( token[token_count] ) == 0 )
	  {
		token[token_count] = NULL;
		token_count--;
	  }
	  token_count++;
	}
	if (token_count==0) continue;

	if (!strcmp(token[0], "exit" )) exit(0); // returns 0, and exits
	if (!strcmp(token[0], "quit" )) exit(0);

	
   if(!strcmp(token[0],"put"))
	{

		  if(token[1] != NULL)
		  { 
			put(token[1]);
			
		  }

		  else
		  {
			printf("PLEASE ENTER THE FILENAME AFTER PUT\n");
			continue;

		  }

	}

	if(!strcmp(token[0],"list"))
	{
		 int validlist=list();
		 if(validlist==0)
		 {
			printf("NO FILES FOUND\n");
		 }
		
	}

	if(!strcmp(token[0],"open"))
	{
		if(token[1]!=NULL)
		{
		 
		strncpy(forclose,token[1],sizeof(forclose));    	

		open(token[1]);
		}
		else
		{
			printf("filesystem name not provided sorry\n");

		}

	}

	if(!strcmp(token[0],"close"))
	{
		
		if(forclose[0]==0)
		{
			printf("please open the file system first\n");

		}
		else
		{
			closeit(forclose);

		}
		
	}

	if(!strcmp(token[0],"createfs"))
	{
		if(!(token[1]==NULL))
		{
			createfs(token[1]);
		
		}
		else
		{
			printf("createfs: FILE NOT FOUND\n");
		}

	}

	if(!strcmp(token[0],"df"))
	{
		printf("%d bytes free\n",df() );
	}

	if(!strcmp(token[0],"del"))
	{
		int validdel=delete(token[1]);
		if(validdel==0)
		{
			printf("del error: FILE NOT FOUND\n");
		}
	}

	if(!strcmp(token[0],"get"))
	{ int getfile;
		if(token[1] !=NULL && token[2]==NULL)
			{
				 getfile =get(token[1],token[2]);
			}
		else if(token[1]!=NULL &&token[2] !=NULL)
			{
				getfile=get(token[1],token[2]);
			}

		if(getfile==1)
		{
			printf("get error: FILE NOT FOUND\n");
		}

	}

	if(!strcmp(token[0],"attrib"))
	{
		if((token[1]!=NULL && token[2]!=NULL))
			{
				
				attribute(token[1],token[2]);
			}

		else
		{
			printf("PLEASE PASS ATTRIBUTE +h <FILENAME> OR +r <FILENAME> TO PROCEED\n");
			
		}    	
	}

	free( working_root );

	}
	return 0;

}


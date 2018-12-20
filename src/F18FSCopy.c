#include "dyn_array.h"
#include "bitmap.h"
#include "block_store.h"
#include "F18FS.h"
#include <string.h>
#include "libgen.h"
#include "math.h"

// you can add more library functions for convenience
// remember to uncomment the tests for corresponding milestones. THIS IS IMPORTANT!
// remember to add pseudo-code for the next milestone
/*

typedef struct inode {
	char * mode;
	char *owner;
	size_t size;
	int time_modified;
	
	size_t inode_num[6];
	inode_t * indirect_block;
	inode_t ** double_indirect_block;
	
} inode_t;
typedef struct fileDescriptor {
	char * filename;
	size_t position;
} fileDescriptor_t;
typedef struct directoryFile {
	char filename[32];
	size_t inode_num;
} directoryFile_t;

typedef struct superblock{
	size_t num_blocks; //2^16
	size_t block_size; //512 bytes
	size_t num_blocks_free;
	Other File system details for metadata
	char fs_name[32];
	
} superblock_t;

typedef struct F18FS {
	superblock_t sb;
	fileDescriptor_t fd;
	inode_t * inode_table;
	Rest of File Data
	block_store_t *bs;
} F18FS_t;



F18FS_t *fs_format(const char *path){
	if(path!=NULL && *path!='\0'){
		//F18FS_t *fs = malloc(sizeof(F18FS_t));
		F18FS_t *fs = fs_mount(path);
		return fs;
	}
	return NULL;
}


F18FS_t *fs_mount(const char *path){
	Check if path to File System has previously been mounted?
	if(path!=NULL && *path!='\0'){
		F18FS_t *fs = malloc(sizeof(F18FS_t));
		fs->bs = block_store_create(path);
		fs->sb.num_blocks = 65536;
		fs->sb.block_size = 512;
		fs->sb.num_blocks_free = fs->sb.num_blocks;
		strcpy(fs->sb.fs_name, path);
		return fs;
	}
	return NULL;
}


int fs_unmount(F18FS_t *fs){
	if(fs!=NULL){
		block_store_destroy(fs->bs);
		free(fs);
		return 0;
	}
	return -1;
}

*/

/*
----------------------
PseudoCode for next milestone:

int fs_create(F18FS_t *fs, const char *path, file_t type){
	- if not(check parameters for null and invalid values){
		- check path and traverse across tree of directories to get
		to correct new file location. Make sure each directory exists,
		otherwise return 0
		- When valid directory is reached, allocate space for file,
		create an inode and add to inode_table, set inode_num in
		created file = inode_table size. Traverse back to parent directory
		and set pointer to new file in directory's inode.
		Return 0;
	}
	return -1
}


int fs_open(F18FS_t *fs, const char *path){
	- Check parameters for validity & null
	- Iterate through directory path given, go through inodes,
	find last file given, make sure it is a file and not a directory
	- Set new file descriptor.filename = path, and the position = 0.
	- Add new file descriptor to FileSystem's overarching file_descriptor array
	- Return file descriptor or -1 on failure of any above cases	
}

int fs_close(F18FS_t *fs, int fd){
	- Basic error checking to make sure fd is valid num & fs exists
	- Checks file descriptor given to make sure it exists in fs's
	file descriptor list.
	- Removes file descriptor from fs, returns 0 on success or -1 on failure
	of any above cases.
	
}

dyn_array_t *fs_get_dir(F18FS_t *fs, const char *path){
	- Error check fs and path for null & validity.
	- Parse through path and find directory by inode.
	- Call dyn_array_create(15, sizeof(directoryFile), NULL)
	- Store in dyn_array that was returned, the filenames of
	every entry in the given path's directory, 15 entries max.
	- Return back this array w/ NULL on error.
}


*/

// you can add more library functions for convenience
// remember to uncomment the tests for corresponding milestones. THIS IS IMPORTANT!
// remember to add pseudo-code for the next milestone


#define BLOCK_STORE_NUM_BLOCKS 65536   // 2^16 blocks.
#define BLOCK_STORE_AVAIL_BLOCKS 65520 // Last 16 blocks consumed by the FBM
#define BLOCK_SIZE_BITS 4096           // 2^9 BYTES per block * 2^3 BITS per BYTES
#define BLOCK_SIZE_BYTES 512           // 2^9 BYTES per block
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES)  // 2^16 blocks of 2^10 bytes.
#define DIRECT_TOTAL_BYTES 3072	
#define SINGLE_INDIRECT_TOTAL_BYTES 131072
#define DOUBLE_INDIRECT_TOTAL_BYTES 33554432
#define DIRECT_BLOCKS 6	
#define INDIRECT_BLOCKS 256
#define DOUBLE_INDIRECT_BLOCKS 65536
#define MAX_FILE_SIZE 33397248


#define number_inodes 256
#define inode_size 64

#define number_fd 256
#define fd_size 6	// any number as you see fit

#define folder_number_entries 15
#define number_pointers_per_block 256

// each inode represents a regular file or a directory file
struct inode {
	uint16_t vacantFile;			// this parameter is only for directory. Each bit denotes each file or sub-folder. 1 means taken, 0 means available.
	char owner[18];

	char fileType;				// 'r' denotes regular file, 'd' denotes directory file
	
	size_t inodeNumber;			// for F18FS, the range should be 0-255
	size_t fileSize; 			// the unit is in byte	
	size_t linkCount;
	
	// to realize the 16-bit addressing, pointers are acutally block numbers, rather than 'real' pointers.
	uint16_t directPointer[6];
	uint16_t indirectPointer;  // same as uint16_t indirectPointer
	uint16_t doubleIndirectPointer;
		
};


struct fileDescriptor {
	uint8_t inodeNum;	// the inode # of the fd
	uint8_t usage; 		// inode pointer usage info. Only the lower 3 digits is used. usage = 1 means direct, 2 means indirect, 4 means dbindirect
	// usage, locate_order and locate_offset together locate the exact byte at which the cursor is
	uint16_t locate_order;		// serial number of the blocks within direct, indirect, or dbindirect range
	uint16_t locate_offset;		// offset of the cursor within a block
};


struct directoryFile {
	char filename[32];
	uint8_t inodeNumber;
};

struct directoryBlock {
	directoryFile_t dentries[7];
	char padding[57];
};



struct F18FS {
	block_store_t * BlockStore_whole;
	block_store_t * BlockStore_inode;
	block_store_t * BlockStore_fd;
};


// initialize directoryFile to 0 or "";
directoryFile_t init_dirFile(void){
	directoryFile_t df;
	memset(df.filename,'\0',64);
	df.inodeNumber = 0x00;
	return df;
}

// initialize directoryBlock 
directoryBlock_t init_dirBlock(void){
	directoryBlock_t db;
	int i=0;
	for(; i<7; i++){
		db.dentries[i] = init_dirFile();
	}
	memset(db.padding,'\0',57);
	return db;
}


// check if the input filename is valid or not
bool isValidFileName(const char *filename)
{
    if(!filename || strlen(filename) == 0 || strlen(filename) > 32)
    {
        return false;
    }

    // define invalid characters might be contained in filenames
    char *invalidCharacters = "!@#$%^&*?\"";
    int i = 0;
    int len = strlen(invalidCharacters);
    for( ; i < len; i++)
    {
        if(strchr(filename, invalidCharacters[i]) != NULL)
        {
            return false;
        }
    }
    return true;
}


// use str_split to decompose the input string into filenames along the path, '/' as delimiter
char** str_split(char* a_str, const char a_delim, size_t * count)
{
	if(*a_str != '/')
	{
		return NULL;
	}
    char** result    = 0;
    char* tmp        = a_str;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = '\0';

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            (*count)++;
        }
        tmp++;
    }

    result = (char**)calloc(1, sizeof(char*) * (*count));
	for(size_t i = 0; i < (*count); i++)
	{
		*(result + i) = (char*)calloc(1, 200);
	}

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
			strcpy(*(result + idx++), token);
        //    *(result + idx++) = strdup(token);
            token = strtok(NULL, delim);
        }

    }
    return result;
}


size_t searchPath(F18FS_t *fs, char* dirPath){
	char *fn = strtok(dirPath,"/");
	// search and check if the directory name "fn" along the path are valid
	size_t iNum = 0; // inode number of the searched directory inode
	inode_t dirInode; // inode of the searched directory
	directoryBlock_t dirBlock; // file block of the searched directory
	while(fn != NULL){
		// find the inode
		if(0 == block_store_inode_read(fs->BlockStore_inode,iNum,&dirInode)){
			return SIZE_MAX;
		}
		// the inode must be of directory
		if(dirInode.fileType != 'd'){
			return SIZE_MAX;
		}
		// read the directory file block 	
		if(0 == block_store_read(fs->BlockStore_whole,dirInode.directPointer[0],&dirBlock)){
			return SIZE_MAX;
		}
		// search in the entries of the directory to see if the next directory name is found
		// use bitmap to jump over uninitialzied(unused) entries
		bitmap_t * dirBitmap = bitmap_overlay(8,&(dirInode.vacantFile));
		size_t j=0, found = 0;
		for(; j<7; j++){
			if(!bitmap_test(dirBitmap,j)){continue;}		
			if(strncmp(dirBlock.dentries[j].filename,fn,strlen(fn)) == 0 /* && (0 < dirBlock.dentries[j].inodeNumber)*/){
				inode_t nextInode; // inode whoes filename is fn
				// check if it is found and is dir  
				if((0 != block_store_inode_read(fs->BlockStore_inode,dirBlock.dentries[j].inodeNumber,&nextInode)) && (nextInode.fileType == 'd')){
					iNum = nextInode.inodeNumber;
					found = 1;
				}
			}
		}
		bitmap_destroy(dirBitmap);
		// if not found, exit on error
		if(found == 0){
			return SIZE_MAX;		
		}
		fn = strtok(NULL,"/");
	}
	return iNum;
} 


size_t getFileInodeID(F18FS_t *fs, size_t dirInodeID, char *filename){
	// file aready exists?? use iNum as the inode number of the parent dir to search if this directory already contains the file/dir to be created
	inode_t parentInode; // inode for the parent directory of the destinated file/dir
	directoryBlock_t parentDir; // directory file block of the parent directory
	if((0==block_store_inode_read(fs->BlockStore_inode,dirInodeID,&parentInode)) || (0==block_store_read(fs->BlockStore_whole, parentInode.directPointer[0],&parentDir))){
		return 0;
	}
	// use bitmap to jump over uninitialized(unused) entries
	bitmap_t *parentBitmap = bitmap_overlay(8, &(parentInode.vacantFile)); 	
	int k=0;
	for(; k<7; k++){
		if(bitmap_test(parentBitmap,k)){
			if(0 == strncmp(parentDir.dentries[k].filename,filename,strlen(filename)) /* && 0 < parentDir.dentries[k].inodeNumber*/){
				// DO NOT worry about same name but different file type.
				// files can't have same name, regardless of file type.
				//inode_t tempInode;
				//if((0 != block_store_inode_read(fs->BlockStore_inode,parentDir.dentries[k].inodeNumber,&tempInode)) && (tempInode.fileType == fileType)){
		        	// printf("path: %s\nfilename already exists: %s\n",path,parentDir.dentries[k].filename);	
				bitmap_destroy(parentBitmap);
				return parentDir.dentries[k].inodeNumber;
				//}
			}
		}
	}
	bitmap_destroy(parentBitmap);
	return 0;
}	




/// Formats (and mounts) an F18FS file for use
/// \param fname The file to format
/// \return Mounted F18FS object, NULL on error
///
F18FS_t *fs_format(const char *path)
{
	if(path != NULL && strlen(path) != 0)
	{
		F18FS_t * ptr_F18FS = (F18FS_t *)calloc(1, sizeof(F18FS_t));	// get started
		ptr_F18FS->BlockStore_whole = block_store_create(path);				// pointer to start of a large chunck of memory
		
		// reserve the 1st block for inode bitmap
		size_t bitmap_ID = block_store_allocate(ptr_F18FS->BlockStore_whole);
//		printf("bitmap_ID = %zu\n", bitmap_ID);
		
		// 2rd - 33th block for inodes, 32 blocks in total
		size_t inode_start_block = block_store_allocate(ptr_F18FS->BlockStore_whole);
//		printf("inode_start_block = %zu\n", inode_start_block);		
		for(int i = 0; i < 31; i++)
		{
			block_store_allocate(ptr_F18FS->BlockStore_whole);
//			printf("all the way with block %zu\n", block_store_allocate(ptr_F18FS->BlockStore_whole));
		}
		
    // the 34th block is wasted according to the assignment statement
		block_store_allocate(ptr_F18FS->BlockStore_whole);
    
		// install inode block store inside the whole block store
		ptr_F18FS->BlockStore_inode = block_store_inode_create(block_store_Data_location(ptr_F18FS->BlockStore_whole) + bitmap_ID * BLOCK_SIZE_BYTES, block_store_Data_location(ptr_F18FS->BlockStore_whole) + inode_start_block * BLOCK_SIZE_BYTES);

		// the first inode is reserved for root dir
		block_store_sub_allocate(ptr_F18FS->BlockStore_inode);
//		printf("first inode ID = %zu\n", block_store_sub_allocate(ptr_F18FS->BlockStore_inode));
		
		// update the root inode info.
		uint8_t root_inode_ID = 0;	// root inode is the first one in the inode table
		inode_t * root_inode = (inode_t *) calloc(1, sizeof(inode_t));
//		printf("size of inode_t = %zu\n", sizeof(inode_t));
		root_inode->vacantFile = 0x0000;
		root_inode->fileType = 'd';								
		root_inode->inodeNumber = root_inode_ID;
		root_inode->linkCount = 1;
//		root_inode->directPointer[0] = root_data_ID;	// not allocate date block for it until it has a sub-folder or file
		block_store_inode_write(ptr_F18FS->BlockStore_inode, root_inode_ID, root_inode);	
		//directoryBlock_t rootDataBlock = init_dirBlock();		
		//block_store_write(ptr_F18FS->BlockStore_whole, root_data_ID,&rootDataBlock );		
		free(root_inode);
		
		// now allocate space for the file descriptors
		ptr_F18FS->BlockStore_fd = block_store_fd_create();

		return ptr_F18FS;
	}
	
	return NULL;	
}



///
/// Mounts an F18FS object and prepares it for use
/// \param fname The file to mount

/// \return Mounted F18FS object, NULL on error

///
F18FS_t *fs_mount(const char *path)
{
	if(path != NULL && strlen(path) != 0)
	{
		F18FS_t * ptr_F18FS = (F18FS_t *)calloc(1, sizeof(F18FS_t));	// get started
		ptr_F18FS->BlockStore_whole = block_store_open(path);	// get the chunck of data	
		
		// the bitmap block should be the 1st one
		size_t bitmap_ID = 0;

		// the inode blocks start with the 2nd block, and goes around until the 33th block, 32 in total
		size_t inode_start_block = 1;
		
		// attach the bitmaps to their designated place
		ptr_F18FS->BlockStore_inode = block_store_inode_create(block_store_Data_location(ptr_F18FS->BlockStore_whole) + bitmap_ID * BLOCK_SIZE_BYTES, block_store_Data_location(ptr_F18FS->BlockStore_whole) + inode_start_block * BLOCK_SIZE_BYTES);
		
		// since file descriptors are allocated outside of the whole blocks, we can simply reallocate space for it.
		ptr_F18FS->BlockStore_fd = block_store_fd_create();
		
		return ptr_F18FS;
	}
	
	return NULL;		
}




///
/// Unmounts the given object and frees all related resources
/// \param fs The F18FS object to unmount
/// \return 0 on success, < 0 on failure
///
int fs_unmount(F18FS_t *fs)
{
	if(fs != NULL)
	{	
		block_store_inode_destroy(fs->BlockStore_inode);
		
		block_store_destroy(fs->BlockStore_whole);
		block_store_fd_destroy(fs->BlockStore_fd);
		
		free(fs);
		return 0;
	}
	return -1;
}





///
/// Creates a new file at the specified location
///   Directories along the path that do not exist are not created
/// \param fs The F18FS containing the file
/// \param path Absolute path to file to create
/// \param type Type of file to create (regular/directory)
/// \return 0 on success, < 0 on failure
///
int fs_create(F18FS_t *fs, const char *path, file_t type)
{
	if(fs != NULL && path != NULL && strlen(path) != 0 && (type == FS_REGULAR || type == FS_DIRECTORY))
	{
		char* copy_path = (char*)calloc(1, 65535);
		strcpy(copy_path, path);
		char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
		size_t count = 0;
		tokens = str_split(copy_path, '/', &count);
		free(copy_path);
		if(tokens == NULL)
		{
			return -1;
		}
		
		// let's check if the filenames are valid or not
		for(size_t n = 0; n < count; n++)
		{	
			if(isValidFileName(*(tokens + n)) == false)
			{
				// before any return, we need to free tokens, otherwise memory leakage
				for (size_t i = 0; i < count; i++)
				{
					free(*(tokens + i));
				}
				free(tokens);
				return -1;
			}
		}
				
		size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
		// first, let's find the parent dir
		size_t indicator = 0;
		
		// we declare parent_inode and parent_data here since it will be used after the for loop
		directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);
		inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));	
		
		for(size_t i = 0; i < count - 1; i++)
		{
			block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
			// in case file and dir has the same name
			if(parent_inode->fileType == 'd')
			{
				block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);

				for(int j = 0; j < folder_number_entries; j++)
				{
					if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
					{
						parent_inode_ID = (parent_data + j) -> inodeNumber;
						indicator++;
					}					
				}
			}					
		}
//		printf("indicator = %zu\n", indicator);
//		printf("parent_inode_ID = %lu\n", parent_inode_ID);
		
		// read out the parent inode
		block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);
		if(indicator == count - 1 && parent_inode->fileType == 'd')
		{
//			printf("before creation, parent_inode->vacantFile = %u\n", parent_inode->vacantFile);

			// same file or dir name in the same path is intolerable
			for(int m = 0; m < folder_number_entries; m++)
			{
				// rid out the case of existing same file or dir name
				if( ((parent_inode->vacantFile >> m) & 1) == 1)
				{
					// before read out parent_data, we need to make sure it does exist!
					block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
					if( strcmp((parent_data + m) -> filename, *(tokens + count - 1)) == 0 )
					{
						free(parent_data);
						free(parent_inode);	
						// before any return, we need to free tokens, otherwise memory leakage
						for (size_t i = 0; i < count; i++)
						{
							free(*(tokens + i));
						}
						free(tokens);
						return -1;											
					}
				}
			}	

			// cannot declare k inside for loop, since it will be used later.
			int k = 0;
			for( ; k < folder_number_entries; k++)
			{
				if( ((parent_inode->vacantFile >> k) & 1) == 0 )
					break;
			}
			
			// if k == 0, then we have to declare a new parent data block
//			printf("k = %d\n", k);
			if(k == 0)
			{
					size_t parent_data_ID = block_store_allocate(fs->BlockStore_whole);
//					printf("parent_data_ID = %zu\n", parent_data_ID);
					if(parent_data_ID < BLOCK_STORE_AVAIL_BLOCKS)
					{
						parent_inode->directPointer[0] = parent_data_ID;
					}
					else
					{
						free(parent_inode);
						free(parent_data);
						// before any return, we need to free tokens, otherwise memory leakage
						for (size_t i = 0; i < count; i++)
						{
							free(*(tokens + i));
						}
						free(tokens);
						return -1;												
					}
			}
			
			if(k < folder_number_entries)	// k == folder_number_entries means this directory is full
			{
				size_t child_inode_ID = block_store_sub_allocate(fs->BlockStore_inode);
				// printf("new child_inode_ID = %zu\n", child_inode_ID);
				// ugh, inodes are used up
				if(child_inode_ID == SIZE_MAX)
				{
					free(parent_data);
					free(parent_inode);
					// before any return, we need to free tokens, otherwise memory leakage
					for (size_t i = 0; i < count; i++)
					{
						free(*(tokens + i));
					}
					free(tokens);
					return -1;	
				}
				
				// wow, at last, we make it!				
				// update the parent inode
				parent_inode->vacantFile |= (1 << k);
				// in the following cases, we should allocate parent data first: 
				// 1)the parent dir is not the root dir; 
				// 2)the file or dir to create is to be the 1st in the parent dir

				block_store_inode_write(fs->BlockStore_inode, parent_inode_ID, parent_inode);	

				// update the parent directory file block
				block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
				strcpy((parent_data + k)->filename, *(tokens + count - 1));
//				printf("the newly created file's name is: %s\n", (parent_data + k)->filename);
				(parent_data + k)->inodeNumber = child_inode_ID;
				block_store_write(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
				
				// update the newly created inode
				inode_t * child_inode = (inode_t *) calloc(1, sizeof(inode_t));
				child_inode->vacantFile = 0;
				if(type == FS_REGULAR)
				{
					child_inode->fileType = 'r';
				}
				else if(type == FS_DIRECTORY)
				{
					child_inode->fileType = 'd';
				}	
				
				child_inode->inodeNumber = child_inode_ID;
				child_inode->fileSize = 0;
				child_inode->linkCount = 1;
				block_store_inode_write(fs->BlockStore_inode, child_inode_ID, child_inode);
			
//				printf("after creation, parent_inode->vacantFile = %d\n", parent_inode->vacantFile);
			
			
			
				// free the temp space
				free(parent_inode);
				free(parent_data);
				free(child_inode);
				// before any return, we need to free tokens, otherwise memory leakage
				for (size_t i = 0; i < count; i++)
				{
					free(*(tokens + i));
				}
				free(tokens);					
				return 0;
			}				
		}
		// before any return, we need to free tokens, otherwise memory leakage
		for (size_t i = 0; i < count; i++)
		{
			free(*(tokens + i));
		}
		free(tokens); 
		free(parent_inode);	
		free(parent_data);
	}
	return -1;
}



///
/// Opens the specified file for use
///   R/W position is set to the beginning of the file (BOF)
///   Directories cannot be opened
/// \param fs The F18FS containing the file
/// \param path path to the requested file
/// \return file descriptor to the requested file, < 0 on error
///
int fs_open(F18FS_t *fs, const char *path)
{
	if(fs != NULL && path != NULL && strlen(path) != 0)
	{
		char* copy_path = (char*)calloc(1, 65535);
		strcpy(copy_path, path);
		char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
		size_t count = 0;
		tokens = str_split(copy_path, '/', &count);
		free(copy_path);
		if(tokens == NULL)
		{
			return -1;
		}
		
		// let's check if the filenames are valid or not
		for(size_t n = 0; n < count; n++)
		{	
			if(isValidFileName(*(tokens + n)) == false)
			{
				// before any return, we need to free tokens, otherwise memory leakage
				for (size_t i = 0; i < count; i++)
				{
					free(*(tokens + i));
				}
				free(tokens);
				return -1;
			}
		}	
		
		size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
		// first, let's find the parent dir
		size_t indicator = 0;

		inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));
		directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);			
		
		// locate the file
		for(size_t i = 0; i < count; i++)
		{		
			block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
			if(parent_inode->fileType == 'd')
			{
				block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
				//printf("parent_inode->vacantFile = %d\n", parent_inode->vacantFile);
				for(int j = 0; j < folder_number_entries; j++)
				{
					//printf("(parent_data + j) -> filename = %s\n", (parent_data + j) -> filename);
					if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
					{
						parent_inode_ID = (parent_data + j) -> inodeNumber;
						indicator++;
					}					
				}
			}					
		}		
		free(parent_data);			
		free(parent_inode);	
		//printf("indicator = %zu\n", indicator);
		//printf("count = %zu\n", count);
		// now let's open the file
		if(indicator == count)
		{
			size_t fd_ID = block_store_sub_allocate(fs->BlockStore_fd);
			//printf("fd_ID = %zu\n", fd_ID);
			// it could be possible that fd runs out
			if(fd_ID < number_fd)
			{
				size_t file_inode_ID = parent_inode_ID;
				inode_t * file_inode = (inode_t *) calloc(1, sizeof(inode_t));
				block_store_inode_read(fs->BlockStore_inode, file_inode_ID, file_inode);	// read out the file inode	
				
				// it's too bad if file to be opened is a dir 
				if(file_inode->fileType == 'd')
				{
					free(file_inode);
					// before any return, we need to free tokens, otherwise memory leakage
					for (size_t i = 0; i < count; i++)
					{
						free(*(tokens + i));
					}
					free(tokens);
					return -1;
				}
				
				// assign a file descriptor ID to the open behavior
				fileDescriptor_t * fd = (fileDescriptor_t *)calloc(1, sizeof(fileDescriptor_t));
				fd->inodeNum = file_inode_ID;
				fd->usage = 1;
				fd->locate_order = 0; // R/W position is set to the beginning of the file (BOF)
				fd->locate_offset = 0;
				block_store_fd_write(fs->BlockStore_fd, fd_ID, fd);
				
				free(file_inode);
				free(fd);
				// before any return, we need to free tokens, otherwise memory leakage
				for (size_t i = 0; i < count; i++)
				{
					free(*(tokens + i));
				}
				free(tokens);			
				return fd_ID;
			}	
		}
		// before any return, we need to free tokens, otherwise memory leakage
		for (size_t i = 0; i < count; i++)
		{
			free(*(tokens + i));
		}
		free(tokens);
	}
	return -1;
}


///
/// Closes the given file descriptor
/// \param fs The F18FS containing the file
/// \param fd The file to close
/// \return 0 on success, < 0 on failure
///
int fs_close(F18FS_t *fs, int fd)
{
	if(fs != NULL && fd >=0 && fd < number_fd)
	{
		// first, make sure this fd is in use
		if(block_store_sub_test(fs->BlockStore_fd, fd))
		{
			block_store_sub_release(fs->BlockStore_fd, fd);
			return 0;
		}	
	}
	return -1;
}



///
/// Populates a dyn_array with information about the files in a directory
///   Array contains up to 15 file_record_t structures
/// \param fs The F18FS containing the file
/// \param path Absolute path to the directory to inspect
/// \return dyn_array of file records, NULL on error
///
dyn_array_t *fs_get_dir(F18FS_t *fs, const char *path)
{
	if(fs != NULL && path != NULL && strlen(path) != 0)
	{	
		char* copy_path = (char*)malloc(200);
		strcpy(copy_path, path);
		char** tokens;		// tokens are the directory names along the path. The last one is the name for the new file or dir
		size_t count = 0;
		tokens = str_split(copy_path, '/', &count);
		free(copy_path);

		if(strlen(*tokens) == 0)
		{
			// a spcial case: only a slash, no dir names
			count -= 1;
		}
		else
		{
			for(size_t n = 0; n < count; n++)
			{	
				if(isValidFileName(*(tokens + n)) == false)
				{
					// before any return, we need to free tokens, otherwise memory leakage
					for (size_t i = 0; i < count; i++)
					{
						free(*(tokens + i));
					}
					free(tokens);		
					return NULL;
				}
			}			
		}		

		// search along the path and find the deepest dir
		size_t parent_inode_ID = 0;	// start from the 1st inode, ie., the inode for root directory
		// first, let's find the parent dir
		size_t indicator = 0;

		inode_t * parent_inode = (inode_t *) calloc(1, sizeof(inode_t));
		directoryFile_t * parent_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);
		for(size_t i = 0; i < count; i++)
		{
			block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, parent_inode);	// read out the parent inode
			// in case file and dir has the same name. But from the test cases we can see, this case would not happen
			if(parent_inode->fileType == 'd')
			{			
				block_store_read(fs->BlockStore_whole, parent_inode->directPointer[0], parent_data);
				for(int j = 0; j < folder_number_entries; j++)
				{
					if( ((parent_inode->vacantFile >> j) & 1) == 1 && strcmp((parent_data + j) -> filename, *(tokens + i)) == 0 )
					{
						parent_inode_ID = (parent_data + j) -> inodeNumber;
						indicator++;
					}					
				}	
			}					
		}	
		free(parent_data);
		free(parent_inode);	
		
		// now let's enumerate the files/dir in it
		if(indicator == count)
		{
			inode_t * dir_inode = (inode_t *) calloc(1, sizeof(inode_t));
			block_store_inode_read(fs->BlockStore_inode, parent_inode_ID, dir_inode);	// read out the file inode			
			if(dir_inode->fileType == 'd')
			{
				// prepare the data to be read out
				directoryFile_t * dir_data = (directoryFile_t *)calloc(1, BLOCK_SIZE_BYTES);
				block_store_read(fs->BlockStore_whole, dir_inode->directPointer[0], dir_data);
				
				// prepare the dyn_array to hold the data
				dyn_array_t * dynArray = dyn_array_create(15, sizeof(file_record_t), NULL);

				for(int j = 0; j < folder_number_entries; j++)
				{
					if( ((dir_inode->vacantFile >> j) & 1) == 1 )
					{
						file_record_t* fileRec = (file_record_t *)calloc(1, sizeof(file_record_t));
						strcpy(fileRec->name, (dir_data + j) -> filename);
						
						// to know fileType of the member in this dir, we have to refer to its inode
						inode_t * member_inode = (inode_t *) calloc(1, sizeof(inode_t));
						block_store_inode_read(fs->BlockStore_inode, (dir_data + j) -> inodeNumber, member_inode);
						if(member_inode->fileType == 'd')
						{
							fileRec->type = FS_DIRECTORY;
						}
						else if(member_inode->fileType == 'f')
						{
							fileRec->type = FS_REGULAR;
						}
						
						// now insert the file record into the dyn_array
						dyn_array_push_front(dynArray, fileRec);
						free(fileRec);
						free(member_inode);
					}					
				}
				free(dir_data);
				free(dir_inode);
				// before any return, we need to free tokens, otherwise memory leakage
				if(strlen(*tokens) == 0)
				{
					// a spcial case: only a slash, no dir names
					count += 1;
				}
				for (size_t i = 0; i < count; i++)
				{
					free(*(tokens + i));
				}
				free(tokens);	
				return(dynArray);
			}
			free(dir_inode);
		}
		// before any return, we need to free tokens, otherwise memory leakage
		if(strlen(*tokens) == 0)
		{
			// a spcial case: only a slash, no dir names
			count += 1;
		}
		for (size_t i = 0; i < count; i++)
		{
			free(*(tokens + i));
		}
		free(tokens);	
	}
	return NULL;
}




// allocate and get the data block id
// \param fs The F18FS Filesystem
// \param fd_t The fileDescriptor object
// \param fileInode Inode of the file
// return the data block id, or 0 on error
uint16_t get_data_block_id(F18FS_t *fs, fileDescriptor_t *fd_t){
	if(fs==NULL || fd_t==NULL){
		//printf("fs or fd_t == NULL\n");
		return 0;
	} else {
		inode_t ino;
		if(0==block_store_inode_read(fs->BlockStore_inode,fd_t->inodeNum,&ino)){
			//printf("Cannot read inode\n");
			return 0;
		} else {
			size_t order = fd_t->locate_order; 
	
			if(fd_t->usage == 1){ // the block to be used is pointed by directPointer
				if(0x0000 == ino.directPointer[order]){ // if the block hasnt been allocated
					if(1<=block_store_get_free_blocks(fs->BlockStore_whole)){
						ino.directPointer[order] = block_store_allocate(fs->BlockStore_whole);
						if(block_store_inode_write(fs->BlockStore_inode,fd_t->inodeNum,&ino)){
							//printf("new, usage:%d, order:%lu,offset:%d\n",fd_t->usage,order,fd_t->locate_offset);
							return ino.directPointer[order];
						}
					}
				} else if(0x0000 != ino.directPointer[order] && block_store_test(fs->BlockStore_whole,ino.directPointer[order])){
					//printf("existed\n");
					return ino.directPointer[order]; // return the pointer, i.e., the address of the data block to write
				}
				//printf("error,order:%lu,offset:%d,direct addr:%d\n",order,fd_t->locate_offset,ino.directPointer[order]);
				return 0;
			} else if(fd_t->usage == 2){ // the block is pointed by indirectPointer
				uint16_t table[256]; // the index table of the indirectPointer
				memset(table,0x0000,2*256);
				if(0 == order && 0x0000 == ino.indirectPointer){ // the block hasnt been allocated
					// the block is the first indirectPointer pointed block
					// allocate a block for the index table pointed by the indirectPointer in the inode 
					if(2<=block_store_get_free_blocks(fs->BlockStore_whole)){
						ino.indirectPointer = block_store_allocate(fs->BlockStore_whole);
						table[0] = block_store_allocate(fs->BlockStore_whole); // allocate the data block pointed by an entry in the index table
						if(0!=block_store_write(fs->BlockStore_whole,ino.indirectPointer,table) && 0!=block_store_inode_write(fs->BlockStore_inode,fd_t->inodeNum,&ino)){
							return table[0]; 
						}
					} 
				} else if(0x0000 != ino.indirectPointer && block_store_test(fs->BlockStore_whole,ino.indirectPointer)){
				// when indirectPointer is alread allocated: (1) the the data block is not allocated (2) data block is allocated	
					if(block_store_read(fs->BlockStore_whole,ino.indirectPointer,table)){
						if(0x0000 == table[order]){
							if(1<=block_store_get_free_blocks(fs->BlockStore_whole)){
								table[order] = block_store_allocate(fs->BlockStore_whole);
								if(block_store_write(fs->BlockStore_whole,ino.indirectPointer,table)){
									return table[order];
								}
							}
						} else if(0x0000 != table[order] && block_store_test(fs->BlockStore_whole,table[order])){
							return table[order];
						}
					}
					//printf("indirectPointer set but cannot access\n");
				}
				//printf("indirectPointer %d\n",ino.indirectPointer);
				return 0;	
			} else { // the block is pointed by a doubleIndiretPointer
				uint16_t outerIndexTable[256];
				memset(outerIndexTable,0x0000,2*256);
				uint16_t innerIndexTable[256];
				memset(innerIndexTable,0x0000,2*256);
				//printf("order: %lu\n",order);
				if(0x0000 == ino.doubleIndirectPointer){ //the block hasnt been allocated yet
					//printf("outerIndexTable index: %lu,usedBlockCount: %lu\n",order/256,usedBlockCount);
					if(3<=block_store_get_free_blocks(fs->BlockStore_whole)){
						ino.doubleIndirectPointer = block_store_allocate(fs->BlockStore_whole);
						outerIndexTable[0] = block_store_allocate(fs->BlockStore_whole);
						innerIndexTable[0] = block_store_allocate(fs->BlockStore_whole);
						if(block_store_write(fs->BlockStore_whole,ino.doubleIndirectPointer,outerIndexTable) &&	
						   block_store_write(fs->BlockStore_whole,outerIndexTable[0],innerIndexTable) &&
						   block_store_inode_write(fs->BlockStore_inode,fd_t->inodeNum,&ino)){
							//printf("dbIndirect addr: %d, order: %lu\n",innerIndexTable[0],order);
							return innerIndexTable[0];
						}
					} 	
				} else if(0x0000 != ino.doubleIndirectPointer && block_store_test(fs->BlockStore_whole,ino.doubleIndirectPointer)){
					if(block_store_read(fs->BlockStore_whole,ino.doubleIndirectPointer,outerIndexTable)){
						if(0x0000 == outerIndexTable[order/256]){
						// when the new block is the first entry of a new innerIndexTable
							if(2<=block_store_get_free_blocks(fs->BlockStore_whole)){
							//printf("outerIndexTable index: %lu,usedBlockCount: %lu\n",order/256,usedBlockCount);
								outerIndexTable[order/256] = block_store_allocate(fs->BlockStore_whole);
								innerIndexTable[order%256] = block_store_allocate(fs->BlockStore_whole);
								if(0!=block_store_write(fs->BlockStore_whole,ino.doubleIndirectPointer,outerIndexTable) && 0!=block_store_write(fs->BlockStore_whole,outerIndexTable[order/256],innerIndexTable)){
									//printf("? dbIndirect addr: %d, order: %lu\n",innerIndexTable[order%256],order);
 									return innerIndexTable[order%256];
								}
							} 	
						} else if(0x0000 != outerIndexTable[order/256] && block_store_test(fs->BlockStore_whole,outerIndexTable[order/256])){
							if(block_store_read(fs->BlockStore_whole,outerIndexTable[order/256],innerIndexTable)){
								//printf("innerIndex %lu addr: %d\n",order%256,innerIndexTable[order%256]);
								if(0x0000 == innerIndexTable[order%256]){
									if(1<=block_store_get_free_blocks(fs->BlockStore_whole)){
										innerIndexTable[order%256] = block_store_allocate(fs->BlockStore_whole);
										if(block_store_write(fs->BlockStore_whole,outerIndexTable[order/256],innerIndexTable)){
											//printf("dbIndirect addr: %d, order: %lu\n",innerIndexTable[order%256],order);
											return innerIndexTable[order%256];
										}
									}	
								} else if(0x0000 != innerIndexTable[order%256] && block_store_test(fs->BlockStore_whole,innerIndexTable[order%256])){
									//printf("dbIndirect addr: %d, order: %lu\n",innerIndexTable[order%256],order);
									return innerIndexTable[order%256];
								}	
								//printf("Something happened to doubleIndirect 5");
							}
							//printf("Something happened to doubleIndirect 4");
						}
						//printf("Something happened to doubleIndirect 3");
					}
					//printf("Something happened to doubleIndirect 2");
				}
				//printf("Something happened to doubleIndirect");
				return 0;
			}
			//printf("Something happened to doubleIndirect -1");
		}		
		//printf("Something happened to doubleIndirect -2\n");
	} 
	//printf("Something happened to doubleIndirect -3\n");
}

//
// calculate the file size up until the location pointed by fileDescriptor usage, order and offset
// return size of the file
size_t getFileSize(fileDescriptor_t *fd_t){
		if(fd_t->usage == 1){
			return 512 * (15+fd_t->locate_order) + fd_t->locate_offset;
		} else if(fd_t->usage == 2){
			return 512 * (6 + fd_t->locate_order) + fd_t->locate_offset;	
		} else {
			return 512 * (15 + 6 + 256 + fd_t->locate_order) + fd_t->locate_offset;
		}
} 


/// Writes data from given buffer to the file linked to the descriptor
///   Writing past EOF extends the file
///   Writing inside a file overwrites existing data
///   R/W position is incremented by the number of bytes written
/// \param fs The F18FS containing the file/// \param fd The file to write to
/// \param dst The buffer to read from
/// \param nbyte The number of bytes to write
/// \return number of bytes written (< nbyte IF out of space), < 0 on error
///
ssize_t fs_write(F18FS_t *fs, int fd, const void *src, size_t nbyte){
	// check if fs,fd,src are valid
	if(fs==NULL || fd < 0 || !block_store_sub_test(fs->BlockStore_fd,fd) || src == NULL){
		return -1;
	} else {
		// if 0 byte is needed to write
		if(nbyte==0){
			return 0;
		} else {
			// get fd's corresponding fileDescriptor structure 
			// get inode number, usage, order and offset
			fileDescriptor_t fd_t;
			if(0==block_store_fd_read(fs->BlockStore_fd,fd,&fd_t)){
				return -2;
			} else {
				/*
				Actually implement the writing of the file
				*/				
				
				size_t locSize = getFileSize(&fd_t); 
				size_t writtenBytes = 0;
	
				while(nbyte - writtenBytes > 0){
					uint16_t blockID = get_data_block_id(fs,&fd_t); 
					
					if(0 < blockID){
						if(fd_t.locate_offset + nbyte - writtenBytes < BLOCK_SIZE_BYTES){ // the last block to write 
							if(0!=block_store_n_write(fs->BlockStore_whole,blockID,fd_t.locate_offset,src+writtenBytes,nbyte-writtenBytes)){
								fd_t.locate_offset += (nbyte - writtenBytes); // update locate_offset
								writtenBytes = nbyte;		
								break; // Done writing 
							}
						
							return -5;
						} else { 
							if(0!=block_store_n_write(fs->BlockStore_whole,blockID,fd_t.locate_offset,src+writtenBytes,BLOCK_SIZE_BYTES-fd_t.locate_offset)){
								writtenBytes += (BLOCK_SIZE_BYTES - fd_t.locate_offset);
								fd_t.locate_offset = 0;//update locate_offset
								// update locate_order
								if(fd_t.usage==1&&fd_t.locate_order== (DIRECT_BLOCKS-1)){ // directPointer space is full
									fd_t.locate_order = 0;
									fd_t.usage = 2;
									
								} else if(fd_t.usage==2&&fd_t.locate_order== (INDIRECT_BLOCKS-1)){ // indirectPointer space is full
									fd_t.locate_order = 0;
									fd_t.usage = 4;
									
								} else { // as long as not exceeding the double inidirect max, unlikely
									fd_t.locate_order += 1;
								}
							
								continue;
							} 
							return -6; 
						}
					} else {
					//printf("error on getting block id: %d,order:%d\n",blockID,fd_t.locate_order);
					break;
					}	
				} 
				inode_t fileInode;
				block_store_inode_read(fs->BlockStore_inode,fd_t.inodeNum,&fileInode);
				if(fileInode.fileSize < locSize + writtenBytes){ // Need to recalculate
					fileInode.fileSize = locSize + writtenBytes;
				}
				if(0!=block_store_fd_write(fs->BlockStore_fd,fd,&fd_t) && 0!=block_store_inode_write(fs->BlockStore_inode,fd_t.inodeNum,&fileInode)){
					//printf("Finish writing: %lu\n",writtenBytes);
					return writtenBytes;
				} else {return -8;}							
	
			}
		}
	}
}

//
// Deletes the specified file and closes all open descriptors to the file
//   Directories can only be removed when empty
// \param fs The F18FS containing the file
// \param path Absolute path to file to remove
// \return 0 on success, < 0 on error
//
int fs_remove(F18FS_t *fs, const char *path) {
	if(fs == NULL || path == NULL || strlen(path) == 0){
		return -1;
	}
	// Validate the path
	char firstChar = *path;
	if(firstChar != '/'){return -2;}
	char dirc[strlen(path)+1];
	char basec[strlen(path)+1];
	strcpy(dirc,path);
	strcpy(basec,path);
	char *dirPath =  dirname(dirc);
	char *baseFileName = basename(basec);

	size_t dirInodeID = searchPath(fs,dirPath);
	size_t fileInodeID;
	if(dirInodeID != SIZE_MAX){
		
		if(0==strcmp(baseFileName,"/") && 0==strcmp(dirPath,"/") ){// Cannot remove root directory
			return -4;
		} else { // If not root, need to validate the file's inode ID
			fileInodeID = getFileInodeID(fs,dirInodeID,baseFileName);
			if(fileInodeID == 0){
				return -4;
			}
		}
		
		
		fileInodeID = getFileInodeID(fs,dirInodeID,baseFileName);
		if(fileInodeID == 0){
			return -4;
		}
		
		
		inode_t fileInode;
		block_store_inode_read(fs->BlockStore_inode,fileInodeID,&fileInode);
		if(fileInode.fileType=='d'){

		//Delete empty dirs, vacantFile would be == 0x00
			if(fileInode.vacantFile == 0x00 || fileInode.linkCount > 1){
				// To delete a dir, remove its entry from the directory block of its parent inode, or the number of hardlinks > 1
				inode_t dirInode; // Parent directoy inode
				block_store_inode_read(fs->BlockStore_inode,dirInodeID,&dirInode);
				//printf("Dir %s before clear file,vacantFile: %d\n",path,dirInode.vacantFile); 
				bitmap_t *bmp = bitmap_overlay(8, &(dirInode.vacantFile));
				directoryBlock_t db_t;
				block_store_read(fs->BlockStore_whole,dirInode.directPointer[0],&db_t); 	
				int m=0;
				for(;m<7; m++){
					if(bitmap_test(bmp,m)){
						if(0 == strncmp(db_t.dentries[m].filename,baseFileName,FS_FNAME_MAX) /* && 0 < parentDir.dentries[k].inodeNumber*/){
							memset(db_t.dentries[m].filename,'\0',FS_FNAME_MAX);
							db_t.dentries[m].inodeNumber = 0x00;
							bitmap_reset(bmp,m);	
							break;
						}
					}	
				}
	
				bitmap_destroy(bmp);
				block_store_inode_write(fs->BlockStore_inode,dirInodeID,&dirInode);
				block_store_write(fs->BlockStore_whole,dirInode.directPointer[0],&db_t);
				// If the directory file inode is not hardlinked to any other file
				if(fileInode.linkCount <= 1) {
					// Remove its file block pointed by directPointer[0], then remove its inode from inode table
					block_store_release(fs->BlockStore_whole,fileInode.directPointer[0]);
					block_store_sub_release(fs->BlockStore_inode,fileInodeID);
					 
					return 0;
				} else {
					fileInode.linkCount -= 1;
					if(block_store_inode_write(fs->BlockStore_inode,fileInodeID,&fileInode)){
						
						return 0;
					}
				}
				return -8;
			}

			return -5;
		} else if(fileInode.fileType=='r') { // if the file to remove is file
			if(fileInode.linkCount > 1){ 
				// If the file inode is referenced by more than one link, then just decrement the linkCount by 1 and update the inode 
				fileInode.linkCount -= 1;
				// Update the file inode
				if(0 == block_store_inode_write(fs->BlockStore_inode,fileInodeID,&fileInode)){
					return -11;
				}	
			} else { // If the file inode is only referened by one link, delete the content of the inode
				int i=0;
				for(; i<6; i++){ // if the directPointer is allocated, release those memory addresses first
					if(0x0000 != fileInode.directPointer[i] && block_store_test(fs->BlockStore_whole,fileInode.directPointer[i])){
						block_store_release(fs->BlockStore_whole,fileInode.directPointer[i]);	
					}	
				}
				if(0x0000 != fileInode.indirectPointer && block_store_test(fs->BlockStore_whole,fileInode.indirectPointer)){// if the indirectPointer is allocated, 
					uint16_t indexTable[256];
					memset(indexTable,0x0000,512);
					if(block_store_read(fs->BlockStore_whole,fileInode.indirectPointer,indexTable)){
						int j=0;
						for(; j<256; j++){ // release all the secondary memory addresses 
							if(0x0000 != indexTable[j] && block_store_test(fs->BlockStore_whole,indexTable[j])){
								block_store_release(fs->BlockStore_whole,indexTable[j]);	
							}	
						}
					} else {return -11;}
					block_store_release(fs->BlockStore_whole,fileInode.indirectPointer);	
				}
				if(0x0000 != fileInode.doubleIndirectPointer && block_store_test(fs->BlockStore_whole,fileInode.doubleIndirectPointer)){// if the doubleIndirectPointer is allocated, 
					uint16_t outerIndexTable[256];
					memset(outerIndexTable,0x0000,512);
					uint16_t innerIndexTable[256];
					memset(innerIndexTable,0x0000,512);
					if(block_store_read(fs->BlockStore_whole,fileInode.doubleIndirectPointer,outerIndexTable)){
						int j=0;
						for(; j<256; j++){ // release all the secondary memory addresses 
							if(outerIndexTable[j]!=0x0000 && block_store_test(fs->BlockStore_whole,outerIndexTable[j])){			
								if(block_store_read(fs->BlockStore_whole,outerIndexTable[j],innerIndexTable)){
									int k=0;
									for(; k<256; k++){ // release all the secondary memory addresses 
										if(innerIndexTable[k]!=0x0000 && block_store_test(fs->BlockStore_whole,innerIndexTable[k])){
											block_store_release(fs->BlockStore_whole,innerIndexTable[k]);	
										} 
									}
								} else {return -10;}
								block_store_release(fs->BlockStore_whole,outerIndexTable[j]);	
							} 	
						}
					} else {return -9;}
					block_store_release(fs->BlockStore_whole,fileInode.doubleIndirectPointer);	
				}
				int fd_count=0;
				for(;fd_count<256;fd_count++){ // close all fd pointing to the file
					if(block_store_sub_test(fs->BlockStore_fd,fd_count)){
						fileDescriptor_t fd_t;
						if(block_store_fd_read(fs->BlockStore_fd,fd_count,&fd_t)){
							if(fd_t.inodeNum==fileInodeID){
								fs_close(fs,fd_count);
							}
						}
						return -7;
					}
				}
				block_store_sub_release(fs->BlockStore_inode,fileInodeID);
			}
			// Remove the file entry in the parent directory data block by reducing one bit from vacantFile
			inode_t dirInode; // The inode for the parent directory containing the file
			block_store_inode_read(fs->BlockStore_inode,dirInodeID,&dirInode); 
			bitmap_t *bmp = bitmap_overlay(8, &(dirInode.vacantFile));
			directoryBlock_t db_t;
			block_store_read(fs->BlockStore_whole,dirInode.directPointer[0],&db_t); 	
			int m=0;
			for(;m<7; m++){
				if(bitmap_test(bmp,m)){
					if(0 == strncmp(db_t.dentries[m].filename,baseFileName,FS_FNAME_MAX) /* && 0 < parentDir.dentries[k].inodeNumber*/){
						bitmap_reset(bmp,m);	
						strncmp(db_t.dentries[m].filename,"\0",FS_FNAME_MAX);
						db_t.dentries[m].inodeNumber = 0x0000;
						break;
					}
				}
			} 
			bitmap_destroy(bmp);
			if(block_store_write(fs->BlockStore_whole,dirInode.directPointer[0],&db_t) &&	block_store_inode_write(fs->BlockStore_inode,dirInodeID,&dirInode)) {
				
				return 0;
			}
			return -12;
		}
		return -6;	
	}
	return -3;
}

///
/// Moves the R/W position of the given descriptor to the given location
///   Files cannot be seeked past EOF or before BOF (beginning of file or offset==0)
///   Seeking past EOF will seek to EOF, seeking before BOF will seek to BOF
/// \param fs The F18FS containing the file
/// \param fd The descriptor to seek
/// \param offset Desired offset relative to whence
/// \param whence Position from which offset is applied
/// \return offset from BOF, < 0 on error
///
off_t fs_seek(F18FS_t *fs, int fd, off_t offset, seek_t whence){
	if(fs !=NULL && fd >= 0 && block_store_sub_test(fs->BlockStore_fd,fd)){
		fileDescriptor_t fd_t;
		inode_t fileInode;
		if(0 !=block_store_fd_read(fs->BlockStore_fd,fd,&fd_t) && 0 != block_store_inode_read(fs->BlockStore_inode,fd_t.inodeNum,&fileInode)){
			ssize_t currentOffset = getFileSize(&fd_t);
			ssize_t fileSize = fileInode.fileSize;
			off_t stdOffset; // Standardized offset, starting from BOF
			// Standardize the offset against the BOF from the three cases: FS_SEEK_SET, FS_SEEK_CUR, and FS_SEEK_END
			if(whence == FS_SEEK_SET){
				if(offset<0){
					stdOffset = 0;
				} else if(offset > fileSize){
					stdOffset = fileSize;
				} else {
					stdOffset = offset;
				}
			} else if(whence == FS_SEEK_CUR){
				if(0 > currentOffset + offset){
					stdOffset = 0;	
				} else if(fileSize < currentOffset + offset){
					stdOffset = fileSize;
				} else {
					stdOffset = currentOffset + offset;
				}
			} else if(whence == FS_SEEK_END){
				if(offset>0){
					stdOffset = fileSize;
				} else if(offset + fileSize < 0){
					stdOffset = 0;
				} else {
					stdOffset = offset + fileSize;
				}
			} else {
				return -4;
			}	
			// Calculate the relative offset, order, and usage	
			if((stdOffset/BLOCK_SIZE_BYTES) >= DIRECT_BLOCKS + INDIRECT_BLOCKS) {
				fd_t.usage = 4;
				fd_t.locate_offset = stdOffset % BLOCK_SIZE_BYTES;
				fd_t.locate_order = (stdOffset/BLOCK_SIZE_BYTES) - (DIRECT_BLOCKS + INDIRECT_BLOCKS);
			} else if (ceil(1.0*stdOffset/BLOCK_SIZE_BYTES) >= DIRECT_BLOCKS){
				fd_t.usage = 2;
				fd_t.locate_offset = stdOffset % BLOCK_SIZE_BYTES;
				fd_t.locate_order = (stdOffset/BLOCK_SIZE_BYTES) - DIRECT_BLOCKS; 
			} else {
				fd_t.usage = 1;
                		fd_t.locate_offset = stdOffset % BLOCK_SIZE_BYTES;
                		fd_t.locate_order = stdOffset/BLOCK_SIZE_BYTES;
			}
			if(0 != block_store_fd_write(fs->BlockStore_fd,fd,&fd_t)){
				return stdOffset;
			} 
			return -3;
		}
		return -2;	
	} 
	return -1;
}

///
/// Reads data from the file linked to the given descriptor
///   Reading past EOF returns data up to EOF
///   R/W position in incremented by the number of bytes read
/// \param fs The F18FS containing the file
/// \param fd The file to read from
/// \param dst The buffer to write to
/// \param nbyte The number of bytes to read
/// \return number of bytes read (< nbyte IFF read passes EOF), < 0 on error
//
ssize_t fs_read(F18FS_t *fs, int fd, void *dst, size_t nbyte){
	// check if fs,fd,src are valid
	if(fs !=NULL && fd >= 0 && block_store_sub_test(fs->BlockStore_fd,fd) && dst != NULL){
		if(nbyte != 0){
			// Open the fileDescriptor and file inode
			fileDescriptor_t fd_t;
			inode_t fileInode;
			if(0 != block_store_fd_read(fs->BlockStore_fd,fd,&fd_t) && 0 != block_store_inode_read(fs->BlockStore_inode,fd_t.inodeNum,&fileInode)){
				// Get the current offset and the file size
				size_t fileSize =  fileInode.fileSize;
				size_t currentOffset = getFileSize(&fd_t);
				// Calculate the maximum of bytes it can read (fileSize - current offset)
				size_t leftBytes = fileSize - currentOffset;
				// If the maximum of bytes to read is smaller than nbyte, then set nbyte to  maximum of bytes 
				if(nbyte > leftBytes){
					nbyte =  leftBytes;
				}
				size_t readBytes = 0;
				while(nbyte - readBytes > 0){
					uint16_t blockID = get_data_block_id(fs,&fd_t); // Get the data block to read	
					if(blockID > 0){
						if(fd_t.locate_offset + nbyte - readBytes < BLOCK_SIZE_BYTES){
							// Need a special read method here to write less than BLOCK_SIZE_BYTES
							if(0 != block_store_n_read(fs->BlockStore_whole,blockID,fd_t.locate_offset,dst+readBytes,nbyte-readBytes)){
								fd_t.locate_offset = fd_t.locate_offset + nbyte - readBytes;
								readBytes = nbyte;
								continue;
							}
							return -4;
						} else {
							if(0 != block_store_n_read(fs->BlockStore_whole,blockID,fd_t.locate_offset,dst+readBytes,BLOCK_SIZE_BYTES-fd_t.locate_offset)){
								readBytes += (BLOCK_SIZE_BYTES-fd_t.locate_offset);
								// increment usage and locate_order
								if(fd_t.usage==1&&fd_t.locate_order== (DIRECT_BLOCKS-1)){ // directPointer space is full
									fd_t.locate_order = 0;
									fd_t.usage = 2;
									//printf("dip into indirect blocks\n");
								} else if(fd_t.usage==2&&fd_t.locate_order== (INDIRECT_BLOCKS-1)){ // indirectPointer space is full
									fd_t.locate_order = 0;
									fd_t.usage = 4;
									//printf("dip into double indirect blocks\n");
								} else { // as long as not exceeding the double inidirect max, unlikely
									fd_t.locate_order += 1;
								}
								fd_t.locate_offset = 0;
								continue;
							}
							return -5;	
						}
					}
					return -3;
				}
				if(0 != block_store_fd_write(fs->BlockStore_fd,fd,&fd_t)){ // update the fd
					return readBytes;
				}
				return -6;					
			}
			return -2;
		}
		return 0;
	} 
	return -1;
}










#include "dyn_array.h"
#include "bitmap.h"
#include "block_store.h"
#include "F18FS.h"
#include <string.h>

// you can add more library functions for convenience
// remember to uncomment the tests for corresponding milestones. THIS IS IMPORTANT!
// remember to add pseudo-code for the next milestone


#define BLOCK_STORE_NUM_BLOCKS 65536   // 2^16 blocks.
#define BLOCK_STORE_AVAIL_BLOCKS 65520 // Last 16 blocks consumed by the FBM
#define BLOCK_SIZE_BITS 4096           // 2^9 BYTES per block * 2^3 BITS per BYTES
#define BLOCK_SIZE_BYTES 512           // 2^9 BYTES per block
#define BLOCK_STORE_NUM_BYTES (BLOCK_STORE_NUM_BLOCKS * BLOCK_SIZE_BYTES)  // 2^16 blocks of 2^10 bytes.

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
	uint16_t indirectPointer[1];  // same as uint16_t indirectPointer
	uint16_t doubleIndirectPointer;
		
};

struct F18FS {
	block_store_t * BlockStore_whole;
	block_store_t * BlockStore_inode;
	block_store_t * BlockStore_fd;
};

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

int fs_create(F18FS_t *fs, const char *path, file_t type){
	
	if(path!=NULL && strlen(path)!=0 && fs!=NULL && path[0]=='/' && (type==FS_REGULAR || type==FS_DIRECTORY)){
		/*Check if path exists*/
		//directory = fs_get_dir(fs, path);
		block_store_inode_write(fs->BlockStore_inode, block_store_sub_allocate(fs->BlockStore_fd), NULL);
		return -1;
	}
	return -1;
}


int fs_open(F18FS_t *fs, const char *path){
	if(path!=NULL && strlen(path)!=0 && fs!=NULL && path[0]=='/'){
		
		return -1;
	}
	return -1;
}

int fs_close(F18FS_t *fs, int fd){
	if(fs!=NULL && fd>=0){
		return -1;
	}
	return -1;
}


dyn_array_t *fs_get_dir(F18FS_t *fs, const char *path){
	//cur_dir = "";
	//last_dir = "";
	
	
	if(fs!=NULL && path!=NULL){
		/*
		cur_dir = strtok(path, "/");
		while(cur_dir!=NULL){
			if(cur_dir=="\0"){
				break;
			}
			else{
				last_dir = cur_dir;
			}
			//cur_dir = strtok(NULL, "/");
		}
		*/
	}
	
	return NULL;
}






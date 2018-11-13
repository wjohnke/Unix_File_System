#include "dyn_array.h"
#include "bitmap.h"
#include "block_store.h"
#include "F18FS.h"

// you can add more library functions for convenience
// remember to uncomment the tests for corresponding milestones. THIS IS IMPORTANT!
// remember to add pseudo-code for the next milestone

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
	/*Other File system details for metadata*/
	char fs_name[32];
	
} superblock_t;

typedef struct F18FS {
	superblock_t sb;
	fileDescriptor_t fd;
	inode_t * inode_table;
	/*Rest of File Data*/
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
	/*Check if path to File System has previously been mounted?*/
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









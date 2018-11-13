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









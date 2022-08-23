#include "simplefs-ops.h"
extern struct filehandle_t file_handle_array[MAX_OPEN_FILES]; // Array for storing opened files

int simplefs_create(char *filename)
{
	/*
		Create file with name `filename` from disk
	*/
	// struct superblock_t *superblock = (struct superblock_t *)malloc(sizeof(struct superblock_t));
	// simplefs_readSuperBlock(superblock);
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	for (int i = 0; i < NUM_INODES; i++)
	{
		simplefs_readInode(i, inode);
		if (inode->status == INODE_IN_USE)
		{
			if (strcmp(filename, inode->name) == 0)
			{
				// free(superblock);
				free(inode);
				return -1;
			}
		}
	}
	int fd = simplefs_allocInode();
	if (fd == -1)
		return -1;
	simplefs_readInode(fd, inode);
	inode->status = INODE_IN_USE;
	memcpy(inode->name, filename, MAX_NAME_STRLEN);
	simplefs_writeInode(fd,inode);
	free(inode);
	return fd;
}

void simplefs_delete(char *filename)
{
	/*
		delete file with name `filename` from disk
	*/
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	for (int i = 0; i < NUM_INODES; i++)
	{
		simplefs_readInode(i, inode);
		if (inode->status == INODE_IN_USE)
		{
			if (strcmp(filename, inode->name) == 0)
			{
				simplefs_freeInode(i);
				for (int j=0; j<NUM_INODE_BLOCKS; j++){
					if(inode->direct_blocks[j]!=-1)
					simplefs_freeDataBlock(inode->direct_blocks[j]);
				}
				free(inode);
				return;
			}
		}
	}
	free(inode);
	
}

int simplefs_open(char *filename)
{
	/*
		open file with name `filename`
	*/
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	for (int i = 0; i < NUM_INODES; i++)
	{
		simplefs_readInode(i, inode);
		if (inode->status == INODE_IN_USE)
		{
			if (strcmp(filename, inode->name) == 0)
			{

				free(inode);
				for (int j = 0; j < MAX_OPEN_FILES; j++)
				{
					if (file_handle_array[j].inode_number == -1)
					{
						file_handle_array[j].inode_number = i;
						file_handle_array[j].offset = 0;
						return j;
					}
				}
				return -1;
			}
		}
	}
	free(inode);
	return -1;
}

void simplefs_close(int file_handle)
{
	/*
		close file pointed by `file_handle`
	*/
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}

int simplefs_read(int file_handle, char *buf, int nbytes)
{
	/*
		read `nbytes` of data into `buf` from file pointed by `file_handle` starting at current offset
	*/
	if (nbytes == 0)
		return 0;
	int offset = file_handle_array[file_handle].offset;
	int inum = file_handle_array[file_handle].inode_number;
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	simplefs_readInode(inum, inode);
	if (offset + nbytes > inode->file_size){
		free(inode);
		return -1;
	}
	
	int stblock = offset / BLOCKSIZE;
	int endblock = (offset + nbytes - 1) / BLOCKSIZE;
	int curoff = 0, curoff1 = offset % BLOCKSIZE;
	char buff[BLOCKSIZE];

	for (int i = stblock; i <= endblock; i++)
	{
		simplefs_readDataBlock(inode->direct_blocks[i], buff);
		int size = BLOCKSIZE - curoff1;
		if (nbytes - curoff < size)
			size = nbytes - curoff;

		memcpy(buf + curoff, buff + curoff1, size);
		curoff1 = 0;
		curoff += size;
	}
	
	free(inode);
	return 0;
}

int simplefs_write(int file_handle, char *buf, int nbytes)
{
	/*
		write `nbytes` of data from `buf` to file pointed by `file_handle` starting at current offset
	*/
	int offset = file_handle_array[file_handle].offset;
	int inum = file_handle_array[file_handle].inode_number;
	if (nbytes == 0)
		return 0;
	
	if (offset + nbytes > MAX_FILE_SIZE*BLOCKSIZE)
		return -1;
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	simplefs_readInode(inum, inode);
	int stblock = (inode->file_size-1) / BLOCKSIZE;
	int endblock = (offset + nbytes - 1) / BLOCKSIZE;
	if(inode->file_size==0)stblock=-1;
	for (int i=stblock+1; i<=endblock; i++){
		int db = simplefs_allocDataBlock();
		if(db==-1){
			for (int j=i-1; j>=stblock+1; j--){
				simplefs_freeDataBlock(inode->direct_blocks[j]);
			}
			free(inode);
			return -1;
		}
		inode->direct_blocks[i]=db;
	}
	int curoff = 0, curoff1 = offset % BLOCKSIZE;
	char buff[BLOCKSIZE];
	stblock = offset / BLOCKSIZE;
	for (int i = stblock; i <= endblock; i++)
	{
		if(curoff1>0)
		simplefs_readDataBlock(inode->direct_blocks[i], buff);
		int size = BLOCKSIZE - curoff1;
		if (nbytes - curoff < size)
			size = nbytes - curoff;

		memcpy(buff + curoff1, buf + curoff, size);
		curoff1 = 0;
		curoff += size;
		simplefs_writeDataBlock(inode->direct_blocks[i], buff);
	}
	
	if(inode->file_size<offset+nbytes)inode->file_size = offset+nbytes;
	simplefs_writeInode(inum,inode);
	
	free(inode);
	return 0;
	
}

int simplefs_seek(int file_handle, int nseek)
{
	/*
	   increase `file_handle` offset by `nseek`
	*/
	int offset = file_handle_array[file_handle].offset;
	int inum = file_handle_array[file_handle].inode_number;
	struct inode_t *inode = (struct inode_t *)malloc(sizeof(struct inode_t));
	simplefs_readInode(inum, inode);
	int newoffset = offset + nseek;
	if (newoffset > inode->file_size || newoffset < 0)
		return -1;
	file_handle_array[file_handle].offset = newoffset;
	return 0;
}
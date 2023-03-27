#ifndef FISOPFS_H
#define FISOPFS_H

#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#define __USE_XOPEN2K8                                                         \
	1  // for strndup: SEE WHICH CFLAGS TO ADD IN ORDER TO AVOID USING THIS
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "bitmap.h"

#define BLOCK_SIZE 4096  // 4KB block size
#define BLOCKS_IN_GROUP 8

#define ALIGN4(s) (((((s) -1) >> 2) << 2) + 4)

#define NBLOCKS 10240  // Total amount of blocks in the filesystem

// INODE
#define NDATA_BLOCKS_PER_INODE                                                 \
	46  // max amount of references to datablocks for a single inode

#define NINODES_PER_BLOCK                                                      \
	BLOCK_SIZE /                                                           \
	        ALIGN4(sizeof(inode_t))  // how many inodes fit in a single block

#define NINODE_BLOCKS                                                          \
	NBLOCKS / (NDATA_BLOCKS_PER_INODE *                                    \
	           NINODES_PER_BLOCK)  // Total amount of inode blocks in the filesystem

#define NTOTAL_INODES                                                          \
	NINODE_BLOCKS *NINODES_PER_BLOCK  // Total amount of inodes in the filesystem

#define ROOT_INODE 1         // root inode number
#define ROOT_INODE_NAME "/"  // root inode name

// data
#define NDATA_BLOCKS                                                           \
	(NBLOCKS -                                                             \
	 (NINODE_BLOCKS +                                                      \
	  BLOCKS_IN_GROUP))  // Total amount of data blocks in the filesystem

// directory entries
#define DIRENT_NAME_LEN 252  // max length of directory entry name
#define NENTRIES_PER_BLOCK                                                     \
	(BLOCK_SIZE /                                                          \
	 sizeof(dirent))  // max amount of directory entries stored in a single data block

#define GET_INODE(ino) &inode_table[ino]
#define GET_DATA_BLOCK(blkno) (uint8_t *) data_blocks + (blkno * BLOCK_SIZE)


// bitmaps
#define NBITS_PER_BLOCK                                                        \
	BLOCK_SIZE *CHAR_BIT  // amount of bits for a bitmap of size BLOCK_SIZE

#define NWORDS_PER_BLOCK                                                       \
	BLOCK_SIZE /                                                           \
	        sizeof(word_t)  // amount of words for a bitmap of size BLOCK_SIZE

#define NINODE_BITMAP_BLOCKS (NTOTAL_INODES / NBITS_PER_BLOCK)


/*  -------------- FILESYSTEM STRUCTURES ----------------- */

// inode
typedef struct inode {
	uint32_t ino;       // inode number
	uint32_t nlinks;    // number of hard links that point to this inode
	uint32_t mode;      // inode (file or directory) mode flags:
	size_t file_size;   // real size of the file (or directory) in bytes
	uint8_t nblocks;    // size of the file (or directory) in blocks
	time_t d_changed;   // last time metadata related to the file changed
	time_t d_modified;  // last time the file's contents were modified
	time_t d_accessed;  // last access date
	uint32_t data_blocks[NDATA_BLOCKS_PER_INODE];  // direct data block pointers
	uid_t uid;  // user id of the owner of this inode
	gid_t gid;  // group id of the owner of this inode
	uint32_t nentries;  // if type==DIR, this is the number of directory entries of this inode
	uint32_t parent;  // inode number of the parent directory of this inode
} inode_t;                // 256 bytes


// inode operations

void print_inode(inode_t *inode);

inode_t *find_inode(const char *path, uint32_t type_flags);

int new_inode(const char *path, mode_t mode);

int unlink_inode(const char *path, inode_t *inode);


struct superblock {
	size_t used_blocks;         // numero total de bloques en uso
	size_t used_data_blocks;    // numero total de bloques de datos en uso
	size_t used_inodes;         // numero total de inodos en uso
	uint32_t first_free_inode;  // First free inode number
};


// directory entry
typedef struct dirent {
	uint32_t ino;                // inode number
	char name[DIRENT_NAME_LEN];  // entry name
} dirent;                            // 256 bytes


#endif  // FISOPFS_H
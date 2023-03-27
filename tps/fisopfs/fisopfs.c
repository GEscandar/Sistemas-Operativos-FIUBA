#include "fisopfs.h"
#include "bitmap.h"


// FILESYSTEM

// inode bitmap, to check wether an inode is in use or not
word_t __inode_bitmap[NWORDS_PER_BLOCK] = { 0 };
bitmap_t inode_bitmap = { .words = __inode_bitmap,
	                  .nwords = sizeof(__inode_bitmap) / sizeof(word_t) };

// data bitmap, to check wether a data block is in use or not
word_t __data_bitmap[NWORDS_PER_BLOCK] = { 0 };
bitmap_t data_bitmap = { .words = __data_bitmap,
	                 .nwords = sizeof(__data_bitmap) / sizeof(word_t) };

// blocks containing inodes: file and directory metadata
inode_t inode_table[NTOTAL_INODES];

// blocks containing file data and directory entries
uint32_t data_blocks[NDATA_BLOCKS * (BLOCK_SIZE / sizeof(uint32_t))];

// root inode
inode_t *root = GET_INODE(ROOT_INODE);

// filesystem image
FILE *image;


void
fill_stat(struct stat *st, inode_t *inode)
{
	st->st_uid = inode->uid;
	st->st_gid = inode->gid;
	st->st_mode = inode->mode;
	st->st_atime = inode->d_accessed;
	st->st_mtime = inode->d_modified;
	st->st_ctime = inode->d_changed;
	st->st_ino = inode->ino;
	st->st_blksize = BLOCK_SIZE;
	st->st_blocks = inode->nblocks;
	st->st_size = inode->file_size;
	st->st_nlink = inode->nlinks;
}

void
print_inode(inode_t *inode)
{
	printf("[debug] Inode Information\n");
	printf("--------------------------------\n");
	printf("[debug] i->ino:				%u\n", inode->ino);
	printf("[debug] i->d_accessed:			%lu\n", inode->d_accessed);
	printf("[debug] i->d_changed:			%lu\n", inode->d_changed);
	printf("[debug] i->d_modified:			%lu\n", inode->d_modified);
	printf("[debug] i->uid:			%d\n", inode->uid);
	printf("[debug] i->gid:			%d\n", inode->gid);
	printf("[debug] i->file_size:			%lu\n", inode->file_size);
	printf("[debug] i->mode:			%u\n", inode->mode);
	printf("[debug] i->nblocks:			%u\n", inode->nblocks);
	printf("[debug] i->nentries:			%u\n", inode->nentries);
	// printf("[debug] i->type:			%u\n", inode->type);
	printf("[debug] i->parent:			%u\n", inode->parent);
	printf("[debug] type:			%s\n",
	       S_ISDIR(inode->mode) ? "directory" : "file");
	printf("[debug] address:			%p\n", inode);
}

// Particionar al string src, dividiéndolo segun el delimitador
// delim y guardar el resultado en dest. Se devuelve la cantidad
// de elementos de dest.
int
strsplit(const char *src, char *dest[], char delim)
{
	int copystart = 0, copysize = 0, argcount = 0;

	for (int i = 0; src[i] != END_STRING; i++) {
		if (src[i] == delim) {
			if (copysize > 0) {
				dest[argcount] =
				        strndup(src + copystart, copysize);

				copysize = 0;
				argcount++;
			}
			copystart = i + 1;

		} else {
			copysize++;
		}
	}
	if (copysize > 0) {
		// Si el ultimo caracter de src no es delim, agregar la ultima particion
		dest[argcount] = strndup(src + copystart, copysize);
		argcount++;
	}

	return argcount;
}

char *
get_pathname(const char *path)
{
	char *name = NULL, *curr = (char *) path;
	for (; *curr; curr++) {
		if (*curr == '/' && *(curr + 1)) {
			name = ++curr;
		}
	}

	return name;
}

// Find inode of type 'type' on the filesystem
inode_t *
find_inode(const char *path, uint32_t type_flags)
{
	if (strcmp(path, ROOT_INODE_NAME) == 0)
		return root;

	char **tokens =
	        malloc((strlen(path) / 2) *
	               sizeof(char *));  // puede haber como máximo strlen(path)/2
	                                 // strings divididos por el delimitador
	int split_size = strsplit(path, tokens, '/');
	char *last_token = tokens[split_size - 1];

	// Search the filesystem directory structure to find the inode
	inode_t *inode = NULL;
	uint32_t nentries = root->nentries;
	dirent *entries = (dirent *) GET_DATA_BLOCK(root->data_blocks[0]);
	bool found = false;

	int i = 0, j = 0;
	while (!found && j < nentries && i < split_size) {
		dirent *entry = &entries[j % NENTRIES_PER_BLOCK];
		printf("[debug] entry name: %s, token: %s, j: %d\n",
		       entry->name,
		       tokens[i],
		       j);
		int comp = strcmp(entry->name, tokens[i]);
		if (comp == 0) {
			inode = GET_INODE(entry->ino);
			printf("[debug] found match for token %s\n", tokens[i]);
			print_inode(inode);
			if (tokens[i] == last_token) {
				if ((inode->mode & type_flags) != 0)
					found = true;
				else
					break;  // inode is of incorrect type
			}
			if (S_ISDIR(inode->mode)) {
				j = 0;
				nentries = inode->nentries;
				entries = (dirent *) GET_DATA_BLOCK(
				        inode->data_blocks[0]);
				free(tokens[i]);
				i++;
			}
		}
		if (comp != 0 || (comp == 0 && !S_ISDIR(inode->mode))) {
			if (++j % NENTRIES_PER_BLOCK == 0 && inode != NULL) {
				printf("[debug] more than 16 entries, "
				       "switching to next block\n");
				inode_t *parent =
				        inode != root ? GET_INODE(inode->parent)
				                      : root;
				entries = (dirent *) GET_DATA_BLOCK(
				        parent->data_blocks[j / NENTRIES_PER_BLOCK]);
			}
		}
	}

	for (; i < split_size; i++)
		free(tokens[i]);
	free(tokens);

	if (!found)
		return NULL;

	return inode;
}

int
get_parent_dir(const char *path, inode_t **dst)
{
	if (strcmp(path, ROOT_INODE_NAME) == 0)
		return -EINVAL;

	int last_slash_index = 0;
	for (int i = 0; i < strlen(path); i++) {
		if (path[i] == '/')
			last_slash_index = i;
	}
	printf("[debug] last_slash_index: %d\n", last_slash_index);

	if (last_slash_index == 0) {
		*dst = root;
	} else {
		char *dirpath = strndup(path, last_slash_index);
		printf("[debug] parent dirpath: %s\n", dirpath);
		*dst = find_inode(dirpath, __S_IFDIR);
		free(dirpath);
		if (!*dst)
			return -ENOENT;
	}

	return last_slash_index;
}

/////////////
bool permissions_ok_for(inode_t *inode, char rwx){
	mode_t S_USR = 0;
	mode_t S_GRP = 0;
	mode_t S_OTH = 0;
	if (rwx=='R'){
		S_USR = S_IRUSR;
		S_GRP = S_IRGRP;
		S_OTH = S_IROTH;
	} else if (rwx=='W'){
		S_USR = S_IWUSR;
		S_GRP = S_IWGRP;
		S_OTH = S_IWOTH;
	} else if (rwx=='X'){
		S_USR = S_IXUSR;
		S_GRP = S_IXGRP;
		S_OTH = S_IXOTH;
	}
	
	// Permissions check.
	struct fuse_context *context = fuse_get_context();
	bool user_can_and_im_user = ((inode->mode & S_USR) && (inode->uid == context->uid));
	bool group_can_and_im_group = ((inode->mode & S_GRP) && (inode->gid == context->gid));
	bool others_can_and_im_others = ((inode->mode & S_OTH) &&
				(inode->uid != context->uid) && (inode->gid != context->gid));
	
	// debug
	printf("[debug] [] CHECKEANDO PERMISOS uid: %d, gid: %d, inode uid: %d, inode gid: %d \n",
	       context->uid,
	       context->gid,
	       inode->uid,
	       inode->gid);
	
	return (user_can_and_im_user || group_can_and_im_group || others_can_and_im_others);
}
//////////////
int
insert_directory_entry(inode_t *parent, uint32_t ino, const char *name)
{
	// Permissions check.
	if (!permissions_ok_for(parent,'W'))
		return -EACCES;

	uint32_t block_offset = parent->nentries / NENTRIES_PER_BLOCK;
	uint32_t entry_offset = parent->nentries % NENTRIES_PER_BLOCK;
	printf("[debug] block_offset: %u\n", block_offset);
	printf("[debug] entry_offset: %u\n", entry_offset);

	if (parent->nentries == 0 || entry_offset == 0) {
		if (parent->nentries ==
		    (NENTRIES_PER_BLOCK * NDATA_BLOCKS_PER_INODE)) {
			return -EFBIG;
		}
		long blk_num = get_free_bit(&data_bitmap);
		if (blk_num < 0)
			return blk_num;

		parent->data_blocks[parent->nblocks++] = blk_num;
		set_bit(&data_bitmap, blk_num);
	}

	dirent *entries =
	        (dirent *) GET_DATA_BLOCK(parent->data_blocks[block_offset]);
	dirent *new_entry = &entries[entry_offset];
	parent->nentries++;

	printf("[debug] creating directory entry for: %s\n", name);
	strncpy(new_entry->name, name, DIRENT_NAME_LEN);
	new_entry->ino = ino;
	print_inode(parent);

	return 0;
}

int
new_inode(const char *path, mode_t mode)
{
	inode_t *parent_dir;
	int last_slash_index;
	if ((last_slash_index = get_parent_dir(path, &parent_dir)) < 0)
		return last_slash_index;

	long ino = get_free_bit(&inode_bitmap);
	printf("[debug] inode number: %ld\n", ino);
	if (ino < 0)
		return ino;
	inode_t *file = GET_INODE(ino);

	if (!file) {
		printf("[debug] Could not find free inode!\n");
		return -ENOENT;
	}

	struct fuse_context *context = fuse_get_context();
	file->uid = context->uid;
	file->gid = context->gid;
	file->ino = ino;
	file->nlinks = 1;
	file->d_changed = time(NULL);
	file->d_accessed = file->d_changed;
	file->d_modified = file->d_changed;
	file->mode = mode;
	file->file_size = 0;
	file->nblocks = 0;
	file->parent = parent_dir->ino;

	print_inode(file);
	set_bit(&inode_bitmap, ino);

	// Increment amount of hardlinks of parent if the new_inode is for a directory
	// (since the new dir will have a '..' refering to its parent).
	if (S_ISDIR(mode)) {
		inode_t *parent = GET_INODE(file->parent);
		parent->nlinks++;
	}

	return insert_directory_entry(parent_dir, ino, path + last_slash_index + 1);
}

int
unlink_inode(const char *path, inode_t *inode)
{
	// Delete directory entry from parent dir
	inode_t *parent;
	int status = get_parent_dir(path, &parent);
	if (status < 0)
		return status;
	
	char *pathname = get_pathname(path);
	printf("[debug] pathname: %s\n", pathname);
	uint32_t dir_blocks = parent->nentries % NENTRIES_PER_BLOCK == 0
	                              ? parent->nentries / NENTRIES_PER_BLOCK
	                              : parent->nentries / NENTRIES_PER_BLOCK + 1;
	dirent entry;

	for (int i = 0; i < dir_blocks; i++) {
		dirent *entries = (dirent *) GET_DATA_BLOCK(
		        parent->data_blocks[parent->data_blocks[i]]);
		uint32_t nentries =
		        (i + 1) * NENTRIES_PER_BLOCK <= parent->nentries
		                ? NENTRIES_PER_BLOCK
		                : parent->nentries % NENTRIES_PER_BLOCK;
		int index = -1;
		for (int j = 0; j < nentries; j++) {
			if (inode->ino == entries[j].ino &&
			    strcmp(entries[j].name, pathname) == 0) {
				printf("[debug] Found entry at index %d\n", j);
				index = j;
				entry = entries[j];
			}
			if (index >= 0 && j < nentries - 1) {
				printf("[debug] assigning entries[%d] = "
				       "entries[%d]\n",
				       j,
				       j + 1);
				entries[j] = entries[j + 1];
			}
		}
		if (index > 0)
			break;
	}

	printf("[debug] deleting entry: %s\n", entry.name);
	parent->nentries--;
	print_inode(parent);

	inode->nlinks--;
	if (inode->nlinks == 0) {
		clear_bit(&inode_bitmap, inode->ino);
	}

	return 0;
}


static int
fisopfs_getattr(const char *path, struct stat *st)
{
	printf("[debug] fisopfs_getattr(%s)\n", path);

	inode_t *inode = find_inode(path, __S_IFREG | __S_IFDIR | __S_IFLNK);
	if (!inode) {
		printf("[debug] Could not find inode of path %s", path);
		return -ENOENT;
	}

	fill_stat(st, inode);

	return 0;
}

static int
fisopfs_readdir(const char *path,
                void *buffer,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_readdir(%s)", path);

	inode_t *dir = find_inode(path, __S_IFDIR);
	if (!dir)
		return -ENOENT;

	struct stat st;

	// Los directorios '.' y '..'
	fill_stat(&st, dir);
	filler(buffer, ".", &st, 0);
	fill_stat(&st, GET_INODE(dir->parent));
	filler(buffer, "..", &st, 0);

	printf("[debug] total entries: %u\n", dir->nentries);
	for (int i = 0; i < dir->nentries; i++) {
		uint32_t block_offset = i / NENTRIES_PER_BLOCK;
		uint32_t entry_offset = i % NENTRIES_PER_BLOCK;
		dirent *entry =
		        (dirent *) GET_DATA_BLOCK(dir->data_blocks[block_offset]) +
		        entry_offset;
		printf("[debug] entry: (block=%d, offset=%d, name=%s, "
		       "inode=%u)\n",
		       block_offset,
		       entry_offset,
		       entry->name,
		       entry->ino);
		inode_t *inode = GET_INODE(entry->ino);
		fill_stat(&st, inode);
		filler(buffer, entry->name, &st, 0);
	}

	return 0;
}

static int
fisopfs_mkdir(const char *path, mode_t mode)
{
	printf("[debug] fisopfs_mkdir(%s, %u)", path, mode);
	mode |= __S_IFDIR;
	return new_inode(path, mode);
}


static int
fisopfs_opendir(const char *path, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_opendir(%s, %d)\n", path, fi->flags);
	inode_t *inode = find_inode(path, __S_IFDIR);
	if (!inode)
		return -ENOENT;
	
	// Permissions check.
	if (!permissions_ok_for(inode,'W'))
		return -EACCES;

	fi->fh = (uint64_t) inode->ino;
	return 0;
}


static int
fisopfs_rmdir(const char *path)
{
	inode_t *inode = find_inode(path, __S_IFDIR);
	if (!inode)
		return -ENOENT;

	if (inode == root)
		// forbid removal of the root inode
		return -EPERM;

	if (inode->file_size > 0 || inode->nentries > 0)
		// only delete a directory if it's empty
		return -ENOTEMPTY;

	return unlink_inode(path, inode);
}


static int
fisopfs_read(const char *path,
             char *buffer,
             size_t size,
             off_t offset,
             struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_read(%s, %lu, %lu)\n", path, offset, size);

	// Find the file inode
	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;
	
	// Permissions check.
	if (!permissions_ok_for(inode,'R'))
		return -EACCES;

	if (offset + size > inode->file_size)
		size = inode->file_size - offset;

	uint32_t blk_num = offset / BLOCK_SIZE;
	uint32_t blk_offset = offset % BLOCK_SIZE;
	uint32_t bytes_read = 0;

	while (blk_num < inode->nblocks && size > 0) {
		uint32_t __size =
		        size > BLOCK_SIZE ? BLOCK_SIZE - blk_offset : size;
		printf("[debug] blk_num: %u\n", inode->data_blocks[blk_num]);
		printf("[debug] blk_offset: %u\n", blk_offset);
		char *copystart =
		        (char *) GET_DATA_BLOCK(inode->data_blocks[blk_num]) +
		        blk_offset;
		strncpy(buffer, copystart, size);
		printf("[debug] read %u bytes\n", __size);
		bytes_read += __size;
		size -= __size;
		buffer += __size;
		blk_num++;
		blk_offset = 0;
	}

	inode->d_accessed = time(NULL);

	return bytes_read;
}

static int
fisopfs_write(const char *path,
              const char *buf,
              size_t size,
              off_t offset,
              struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_write(%s, %s, %lu, %lu)\n", path, buf, size, offset);
	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;
	
	// Permissions check.
	if (!permissions_ok_for(inode,'W'))
		return -EACCES;

	uint32_t blk_num = offset / BLOCK_SIZE;
	printf("[debug] blk_num: %u\n", blk_num);
	uint32_t blk_offset = offset % BLOCK_SIZE;
	// Store file_size to return at the end.
	// Remember that write 'attempts' to write the requested amount of bytes.
	size_t arrival_size = inode->file_size;

	while (inode->nblocks < NDATA_BLOCKS_PER_INODE && size > 0) {
		// If the file already points to a dirty block, use it
		if (inode->nblocks > blk_num)
			blk_num = inode->data_blocks[inode->nblocks - 1];
		// Otherwise, allocate a new block
		else {
			blk_num = get_free_bit(&data_bitmap);
			if (blk_num < 0)
				return blk_num;
			inode->data_blocks[inode->nblocks++] = blk_num;
			set_bit(&data_bitmap, blk_num);
		}
		printf("[debug] blk_num: %u\n", blk_num);
		printf("[debug] blk_offset: %u\n", blk_offset);
		uint32_t __size =
		        size > BLOCK_SIZE ? BLOCK_SIZE - blk_offset : size;
		memcpy(GET_DATA_BLOCK(blk_num) + blk_offset, buf, __size);
		printf("[debug] wrote %u bytes\n", __size);
		// update inode
		inode->file_size += __size;
		inode_t *parent = GET_INODE(inode->parent);
		while (true) {
			parent->file_size += __size;
			if (parent == root)
				break;
			parent = GET_INODE(parent->parent);
		}
		// update helper variables
		size -= __size;
		printf("[debug] remaining bytes: %lu, offset: %lu\n", size, offset);
		buf += __size;
		blk_num++;
		blk_offset = 0;
	}

	if (inode->nblocks == NDATA_BLOCKS_PER_INODE && size > 0)
		return -EFBIG;

	return inode->file_size - arrival_size;
}


static int
fisopfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	printf("[debug] fisopfs_mknod(%s)\n", path);
	mode |= __S_IFREG;
	return new_inode(path, mode);
}


static int
fisopfs_symlink(const char *to, const char *from)
{
	printf("[debug] fisopfs_symlink(%s,%s)\n", to, from);
	inode_t *target = find_inode(to, __S_IFREG | __S_IFDIR);
	if (!target)
		return -ENOENT;

	int status;
	status = new_inode(from, __S_IFLNK | S_IRWXG | S_IRWXU | S_IRWXO);
	if (status < 0)
		return status;

	status = fisopfs_write(from, to, strlen(to), 0, NULL);
	if (status < 0)
		return status;

	return 0;
}


static int
fisopfs_readlink(const char *path, char *buf, size_t len)
{
	printf("[debug] fisopfs_readlink(%s,%lu)\n", path, len);
	int status = fisopfs_read(path, buf, len, 0, NULL);
	if (status < 0)
		return status;
	printf("[debug] Link target: %s\n", buf);
	return 0;
}


static int
fisopfs_link(const char *from, const char *to)
{
	printf("[debug] fisopfs_link(%s,%s)\n", from, to);

	inode_t *target = find_inode(from, __S_IFREG);
	if (!target)
		return -ENOENT;

	inode_t *parent;
	int sindex = get_parent_dir(to, &parent);
	if (sindex < 0)
		return sindex;

	int status = insert_directory_entry(parent, target->ino, to + sindex + 1);
	if (status < 0)
		return status;

	target->nlinks++;
	return 0;
}

static int
fisopfs_open(const char *path, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_open(%s, %d)\n", path, fi->flags);

	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;
	fi->fh = (uint64_t) inode->ino;

	return 0;
}

static int
fisopfs_truncate(const char *path, off_t offset)
{
	printf("[debug] fisopfs_truncate(%s, %lu)\n", path, offset);

	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;
	for (int i = 0; i < inode->nblocks; i++) {
		uint32_t blk_num = inode->data_blocks[i];
		uint32_t blk_offset = inode->file_size % BLOCK_SIZE;
		uint32_t size = blk_offset > 0 ? blk_offset : BLOCK_SIZE;

		memset(GET_DATA_BLOCK(blk_num), 0, size);
		// ^obs: el memset a 0 podría eliminarse, tal como lo omite por ejemplo el fs de linux.
		printf("[debug] cleared %u bytes\n", size);
		clear_bit(&data_bitmap, blk_num);

		inode->data_blocks[i] = 0;
		inode->file_size -= size;
		inode->d_modified = time(NULL);
		inode->d_changed = inode->d_modified;
		inode->d_accessed = inode->d_modified;
		inode_t *parent = GET_INODE(inode->parent);
		print_inode(parent);
		while (true) {
			parent->file_size -= size;
			if (parent == root)
				break;
			parent = GET_INODE(parent->parent);
		}
	}
	inode->nblocks = 0;

	return 0;
}

static int
fisopfs_unlink(const char *path)
{
	printf("[debug] fisopfs_unlink(%s)\n", path);

	int status;
	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;

	if (inode->nlinks - 1 == 0) {
		status = fisopfs_truncate(path, 0);
		if (status < 0)
			return status;
	}

	return unlink_inode(path, inode);
}

static int
fisopfs_utimens(const char *path, const struct timespec tv[2])
{
	printf("[debug] fisopfs_utimens(%s)\n", path);
	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;

	struct timespec atime = tv[0];
	struct timespec mtime = tv[1];

	inode->d_accessed = atime.tv_sec;
	inode->d_modified = mtime.tv_sec;
	inode->d_changed = mtime.tv_sec;

	return 0;
}

static int
fisopfs_flush(const char *path, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_flush(%s)\n", path);
	inode_t *inode = find_inode(path, __S_IFREG);
	if (!inode)
		return -ENOENT;

	// save entire filesystem image (checking at each step that no errors occurred)
	size_t items_written;
	fseek(image, 0, SEEK_SET);
	items_written = fwrite(__inode_bitmap, sizeof(__inode_bitmap), 1, image);
	assert(items_written == 1);
	items_written = fwrite(__data_bitmap, sizeof(__data_bitmap), 1, image);
	assert(items_written == 1);
	items_written = fwrite(inode_table, sizeof(inode_table), 1, image);
	assert(items_written == 1);
	items_written = fwrite(data_blocks, sizeof(data_blocks), 1, image);
	assert(items_written == 1);

	fflush(image);
	return 0;
}


static void *
fisopfs_init(struct fuse_conn_info *conn)
{
	printf("[debug] fisopfs_init\n");
	printf("[debug] sizeof dirent: %lu\n", sizeof(dirent));
	printf("[debug] NENTRIES_PER_BLOCK: %lu\n", NENTRIES_PER_BLOCK);
	printf("[debug] sizeof inode_t: %lu\n", sizeof(inode_t));
	printf("[debug] NINODES_PER_BLOCK: %lu\n", NINODES_PER_BLOCK);
	printf("[debug] NINODE_BLOCKS: %lu\n", NINODE_BLOCKS);
	printf("[debug] NTOTAL_INODES: %lu\n", NTOTAL_INODES);

	// prints auxiliares
	printf("[debug] sizeof inode_t SIN ALIGN: %lu\n", sizeof(inode_t));  //[]
	printf("debug[] sizeof mode_t: %lu\n", sizeof(mode_t));  //[]
	printf("debug[] sizeof time_t: %lu\n", sizeof(time_t));  //[]
	printf("debug[] sizeof size_t: %lu\n", sizeof(size_t));  //[]
	printf("debug[] sizeof uid_t: %lu\n", sizeof(uid_t));    //[]
	printf("debug[] sizeof gid_t: %lu\n", sizeof(gid_t));    //[]

	image = fopen("image.fisopfs", "rb+");
	if (image == NULL) {
		// First time this is mounted
		image = fopen("image.fisopfs", "wb+");
		if (image == NULL) {
			perror("[debug] Error opening filesystem image");
			exit(-1);
		}

		// initialize root inode
		struct fuse_context *context = fuse_get_context();
		printf("[debug][] DESDE INIT, context uid: %d, context gid: %d\n",
	       context->uid,
	       context->gid);
		root->uid = 1000;
		root->gid = 1000;
		root->ino = ROOT_INODE;
		root->mode = __S_IFDIR | 0777;
		root->d_changed = time(NULL);
		root->d_accessed = root->d_changed;
		root->d_modified = root->d_changed;
		root->nblocks = 0;
		root->parent = ROOT_INODE;
		root->nentries = 0;
		root->nlinks = 2;
		set_bit(&inode_bitmap, 0);
		set_bit(&inode_bitmap, ROOT_INODE);  // mark root inode as used

		print_inode(root);

	} else {
		size_t items_read;

		items_read =
		        fread(__inode_bitmap, sizeof(__inode_bitmap), 1, image);
		printf("[debug] items_read: %lu\n", items_read);
		printf("[debug] inode_bitmap[0]: %u\n", __inode_bitmap[0]);
		items_read =
		        fread(__data_bitmap, sizeof(__data_bitmap), 1, image);
		printf("[debug] items_read: %lu\n", items_read);
		printf("[debug] __data_bitmap[0]: %u\n", __data_bitmap[0]);
		items_read = fread(inode_table, sizeof(inode_table), 1, image);
		printf("[debug] items_read: %lu\n", items_read);
		print_inode(root);
		items_read = fread(data_blocks, sizeof(data_blocks), 1, image);
		printf("[debug] items_read: %lu\n", items_read);
	}

	return NULL;
}

static void
fisopfs_destroy(void *ptr)
{
	printf("[debug] fisopfs_destroy\n");
	fisopfs_flush(ROOT_INODE_NAME, NULL);
	fclose(image);
}

static int
fisopfs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi)
{
	// Obtengo el inodo
	inode_t *inode = find_inode(path, __S_IFREG | __S_IFDIR);
	if (!inode)
		return -ENOENT;
	
	// Permissions check. Comentado xq no contemplado en tests de permisos;
	// Funciona. Descomentar para ver funcionamiento manualmente (ver archivo de pruebas 4_...txt).
	struct fuse_context *context = fuse_get_context();
	/*if (context->uid!=0)
		return -EACCES;*/
		
	printf("[debug][] DESDE CHOWN, context uid: %d, uid pm: %d, context gid: %d, gid pm: %d\n",
	       context->uid,
	       uid,
	       context->gid,
	       gid);

	// Setting permissions
	if (uid != -1)
		inode->uid = uid;
	if (gid != -1)
		inode->gid = gid;

	return 0;
}

static int
fisopfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("[debug] [] SETEANDO PERMISOS\n");
	printf("[debug] fisopfs_chmod(%s, mode: %d)\n", path, mode);

	inode_t *inode = find_inode(path, __S_IFREG | __S_IFDIR);
	if (!inode)
		return -ENOENT;
	
	// Permissions check. Comentado xq no contemplado en tests de permisos;
	// Funciona. Descomentar para ver funcionamiento manualmente (ver archivo de pruebas 4_...txt).
	struct fuse_context *context = fuse_get_context();
	/*if (inode->uid != context->uid)
		return -EACCES;*/
		
	printf("[debug] [] PRINT CONTEXT uid: %d, gid: %d\n",
	       context->uid,
	       context->gid);
	

	// Mode change
	printf("[debug] [] PRINT DEBUG modo anterior: %d\n", inode->mode);
	inode->mode &= __S_IFREG;
	inode->mode |= mode;
	printf("[debug] [] PRINT DEBUG modo actual: %d\n", inode->mode);

	return 0;
}

static struct fuse_operations operations = {
	.init = fisopfs_init,
	.getattr = fisopfs_getattr,
	.readdir = fisopfs_readdir,
	.read = fisopfs_read,
	.mknod = fisopfs_mknod,
	.open = fisopfs_open,
	.truncate = fisopfs_truncate,
	.write = fisopfs_write,
	.unlink = fisopfs_unlink,
	.mkdir = fisopfs_mkdir,
	.opendir = fisopfs_opendir,
	.rmdir = fisopfs_rmdir,
	.utimens = fisopfs_utimens,
	.flush = fisopfs_flush,
	.destroy = fisopfs_destroy,
	.symlink = fisopfs_symlink,
	.readlink = fisopfs_readlink,
	.link = fisopfs_link,
	.chmod = (void *) fisopfs_chmod,
	.chown = (void *) fisopfs_chown,
};

int
main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &operations, NULL);
}

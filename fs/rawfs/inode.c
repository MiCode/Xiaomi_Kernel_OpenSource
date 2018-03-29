/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#if defined(CONFIG_MT_ENG_BUILD)
#define DEBUG 1
#endif

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/crc32.h>
#include <linux/mm.h>
#include "rawfs.h"

#define CEILING(x, y) rawfs_div(((x)+(y)-1), (y))

/* file operations */
static const struct file_operations rawfs_file_operations = {
	.read		= do_sync_read,
	.write		= do_sync_write,
	.mmap		= generic_file_mmap,
	.llseek		= generic_file_llseek,
	.aio_read	= rawfs_reg_file_aio_read,
	.aio_write	= rawfs_reg_file_aio_write,
};

static const struct file_operations rawfs_block_file_operations = {
	.read		= do_sync_read,
	.aio_read	= rawfs_block_file_aio_read,
	.write		= do_sync_write,
	.aio_write	= rawfs_block_file_aio_write,
	.mmap		= generic_file_mmap,
	.llseek		= generic_file_llseek,
};

static const struct file_operations rawfs_dir_operations = {
	.open		= dcache_dir_open,
	.release	= dcache_dir_close,
	.llseek		= dcache_dir_lseek,
	.read		= generic_read_dir,
	.iterate	= rawfs_readdir,
	.fsync		= rawfs_file_sync,
};

/* dentry operations */
static const struct dentry_operations rawfs_dentry_ops = {
	.d_delete = rawfs_delete_dentry,
};

/* Case in-sensitive dentry operations */

static const struct dentry_operations rawfs_ci_dentry_ops = {
/*	.d_revalidate, Not necessary for rawfs */
	.d_hash = rawfs_ci_hash,
	.d_compare = rawfs_compare_dentry,
	.d_delete = rawfs_delete_dentry,
};

/* Address Space Operations: Block file & Normal file */
static const struct address_space_operations rawfs_aops = {
	.readpage	= rawfs_readpage,
	.write_begin	= rawfs_write_begin,
	.write_end	= rawfs_write_end,
};

static struct backing_dev_info rawfs_backing_dev_info = {
	.ra_pages	   = 0,	/* No readahead */
	.capabilities   = BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_WRITEBACK |
			  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
			  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP |
			  BDI_CAP_EXEC_MAP,
};


static unsigned long rawfs_hash(const char *name)
{
	unsigned long val;

	val = crc32(0, name, strlen(name));
	return hash_32(val, RAWFS_HASH_BITS);
}

static void rawfs_attach(struct inode *inode, const char *name)
{
	struct rawfs_sb_info *sbi = RAWFS_SB(inode->i_sb);
	struct hlist_head *head = sbi->inode_hashtable + rawfs_hash(name);

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_attach %s %lx to hlist[%d]=%lx\n", name,
		(unsigned long)(inode), (unsigned)rawfs_hash(name), (unsigned long)head);

	spin_lock(&sbi->inode_hash_lock);
	hlist_add_head(&RAWFS_I(inode)->i_rawfs_hash, head);
	spin_unlock(&sbi->inode_hash_lock);
}

static void rawfs_detach(struct inode *inode)
{
	struct rawfs_sb_info *sbi = RAWFS_SB(inode->i_sb);

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_detach %s %lx from hlist\n",
		RAWFS_I(inode)->i_name, (unsigned long)(inode));

	spin_lock(&sbi->inode_hash_lock);
	hlist_del_init(&RAWFS_I(inode)->i_rawfs_hash);
	spin_unlock(&sbi->inode_hash_lock);
}

/** Inode Cache */
static struct kmem_cache *rawfs_inode_cachep;

struct inode *rawfs_alloc_inode(struct super_block *sb)
{
	struct rawfs_inode_info *ei;

	ei = kmem_cache_alloc(rawfs_inode_cachep, GFP_NOFS);
	if (!ei) {
		RAWFS_PRINT(RAWFS_DBG_INODE,
			"rawfs_alloc_inode, allocation failed, out of memory\n");
		return NULL;
	}

	init_rwsem(&ei->truncate_lock);

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_alloc_inode, %lx\n", (unsigned long)(ei));

	return &ei->vfs_inode;
}
EXPORT_SYMBOL_GPL(rawfs_alloc_inode);

static void rawfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_i_callback, %lx\n",
		(unsigned long)(RAWFS_I(inode)));
	kmem_cache_free(rawfs_inode_cachep, RAWFS_I(inode));
}

void rawfs_destroy_inode(struct inode *inode)
{
	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_destroy_inode, %lx\n",
		(unsigned long)(inode));

	remove_inode_hash(inode);
	rawfs_detach(inode);

	call_rcu(&inode->i_rcu, rawfs_i_callback);
}
EXPORT_SYMBOL_GPL(rawfs_destroy_inode);

static void init_once(void *foo)
{
	struct rawfs_inode_info *ei = (struct rawfs_inode_info *)foo;

	spin_lock_init(&ei->cache_lru_lock);
	ei->nr_caches = 0;
	ei->cache_valid_id = RAWFS_CACHE_VALID + 1;
	INIT_LIST_HEAD(&ei->cache_lru);
	INIT_HLIST_NODE(&ei->i_rawfs_hash);
	inode_init_once(&ei->vfs_inode);
}

int __init rawfs_init_inodecache(void)
{
	rawfs_inode_cachep = kmem_cache_create("rawfs_inode_cache",
						 sizeof(struct rawfs_inode_info),
						 0, (SLAB_RECLAIM_ACCOUNT|
						SLAB_MEM_SPREAD),
						 init_once);

	if (rawfs_inode_cachep == NULL) {
		RAWFS_PRINT(RAWFS_DBG_INODE,
			"rawfs_init_inodecache, failed, -ENOMEM\n");
		return -ENOMEM;
	}

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_init_inodecache\n");
	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_init_inodecache);

void __exit rawfs_destroy_inodecache(void)
{
	kmem_cache_destroy(rawfs_inode_cachep);
}
EXPORT_SYMBOL_GPL(rawfs_destroy_inodecache);

/* ----------------------------------------------------------------------------- */
static unsigned long rawfs_unique_id(struct super_block *sb, umode_t mode,
	const char *name, int parent_folder)
{
	int result;
	int len = strlen(name);
	int seed = parent_folder;

	do {
		result = crc32(seed, name, len);
		seed++;
	} while (rawfs_file_list_get_by_id(sb, mode, result) != NULL);

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_unique_id: new %s %s, id = %X\n",
		(S_ISDIR(mode))?"folder":"file", name, result);

	return result;
}

void rawfs_hash_init(struct super_block *sb)
{
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	int i;

	spin_lock_init(&sbi->inode_hash_lock);
	for (i = 0; i < RAWFS_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&sbi->inode_hashtable[i]);
}
EXPORT_SYMBOL_GPL(rawfs_hash_init);

/* Get inode from inode cache */
struct inode *rawfs_iget(struct super_block *sb, const char *name, int folder)
{
	struct rawfs_sb_info	*sbi = RAWFS_SB(sb);
	struct hlist_head		*head = sbi->inode_hashtable + rawfs_hash(name);
/* struct hlist_node		*ptr; */
	struct rawfs_inode_info	*inode_info;
	struct inode			*inode = NULL;
	int index = 0;

	RAWFS_PRINT(RAWFS_DBG_INODE,
		"rawfs_iget, by name %s @ folder %X in inode_hashtable[%ld]=%lx\n",
		name, folder, rawfs_hash(name), (unsigned long)head);

	/* Search inode list for the file */
	spin_lock(&sbi->inode_hash_lock);

	hlist_for_each_entry(inode_info, head, i_rawfs_hash) {
		RAWFS_PRINT(RAWFS_DBG_INODE,
			"rawfs_iget, inode_hashtable[%ld][%d] %s @ folder %X, %lx -> %lx\n",
			rawfs_hash(name), index,
			inode_info->i_name,
			inode_info->i_parent_folder_id,
			(unsigned long)(&inode_info->vfs_inode),
			(unsigned long)(inode_info));

		index++;

		if (inode_info->i_parent_folder_id != folder)
			continue;

/* BUG_ON(i->vfs_inode.i_sb != sb); */
		if (strnicmp(inode_info->i_name, name, RAWFS_MAX_FILENAME_LEN+4) == 0) {
			RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_iget, igrab %lx\n",
				(unsigned long)(&inode_info->vfs_inode));
			inode = igrab(&inode_info->vfs_inode);
			if (inode)
				break;
		}
	}

	if (IS_ERR_OR_NULL(inode))
		RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_iget, %s was not found\n", name);
	else
		RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_iget, %s was found at %lx\n", name,
		(unsigned long)(inode));

	spin_unlock(&sbi->inode_hash_lock);

	return inode;
}
EXPORT_SYMBOL_GPL(rawfs_iget);

#if 0
static int rawfs_fill_inode_blk(struct inode *inode,
	struct rawfs_file_info *file_info, int block_no, int page_no)
{
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);

	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
	inode->i_uid  = current_uid();
	inode->i_gid  = current_gid();
	inode->i_size = RAWFS_SB(inode->i_sb)->block_size;
	inode->i_ino  = RAWFS_BLOCK0_INO + block_no;
	inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
	sprintf(inode_info->i_name, ".block%d", block_no);
	inode_info->i_location_block = block_no;
	inode_info->i_location_page = 0;
	inode_info->i_location_page_count = 0;
	inode_info->i_parent_folder_id = RAWFS_ROOT_DIR_ID;

	return 0;
}
#endif

static int rawfs_fill_inode_reg(struct inode *inode,
	struct rawfs_file_info *file_info, int block_no, int page_no)
{
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);
	int name_len;

	name_len = strlen(file_info->i_name);
	strncpy(inode_info->i_name, file_info->i_name, name_len+1);
	inode->i_atime = file_info->i_atime;
	inode->i_mtime = file_info->i_mtime;
	inode->i_ctime = file_info->i_ctime;
	inode->i_uid.val = file_info->i_uid;
	inode->i_gid.val = file_info->i_gid;
	inode->i_mode = file_info->i_mode;  /* Use file_info ones */
	inode->i_ino = iunique(inode->i_sb, RAWFS_MAX_RESERVED_INO);

	inode_info->i_id = file_info->i_id;
	inode_info->i_location_page_count = file_info->i_chunk_total;
	inode_info->i_parent_folder_id = file_info->i_parent_folder_id;
	inode_info->i_location_block = block_no;
	inode_info->i_location_page = page_no;

	return 0;
}

int rawfs_subdirs(struct inode *dir)
{
	struct rawfs_sb_info	*sbi = RAWFS_SB(dir->i_sb);
	struct rawfs_file_list_entry *ptr;

	int folder_id = RAWFS_I(dir)->i_id;
	int count = 0;

	mutex_lock(&sbi->file_list_lock);
	list_for_each_entry(ptr, &sbi->folder_list, list) {
		if (ptr->file_info.i_parent_folder_id == folder_id)
			count++;
	}
	mutex_unlock(&sbi->file_list_lock);

	return count;
}

static struct inode *rawfs_build_inode(struct super_block *sb,
	struct rawfs_file_info *file_info, int block_no, int page_no)
{
	struct inode *inode = NULL;
	umode_t mode = S_IFREG | 755;

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_build_inode");

	if (file_info) {
		inode = rawfs_iget(sb, file_info->i_name,
			file_info->i_parent_folder_id);

		if (inode) {
			iput(inode);
			goto out;
		} else { /* Not exist in inode cache, search in file list */

			struct rawfs_file_list_entry *entry;

			entry = rawfs_file_list_get(sb, file_info->i_name,
				file_info->i_parent_folder_id);

			if (entry) {
				memcpy(file_info, &entry->file_info,
					sizeof(struct rawfs_file_info));
				block_no = entry->i_location_block;
				page_no = entry->i_location_page;
			} else {   /* New File, or new folder */
				file_info->i_atime = file_info->i_mtime = file_info->i_ctime =
					CURRENT_TIME_SEC;
				file_info->i_uid  = current_uid().val;
				file_info->i_gid  = current_gid().val;
				file_info->i_size = 0;
				/* file_info->i_mode |= (S_IRUGO | S_IXUGO); */
				/* file_info->i_chunk_index; */
				file_info->i_chunk_total = 1;
				/* file_info->i_mode; assigned as S_IFREG */
				/* file_info->i_crc; */

				/* i_id will be the ino of the inode */
				file_info->i_id = rawfs_unique_id(sb, file_info->i_mode,
					file_info->i_name, file_info->i_parent_folder_id);
			}

			mode = file_info->i_mode;
		}
	}

	inode = new_inode(sb);

	if (IS_ERR_OR_NULL(inode))	{
		RAWFS_PRINT(RAWFS_DBG_INODE,
			"rawfs_build_inode: new_inode failed %lx\n", (unsigned long)(inode));
		goto out;
	}
	inode->i_version = 1;
	rawfs_fill_inode(inode, file_info, block_no, page_no, mode , 0);
		/* Default mode for Block file & Super block is S_IFREG | 755 */
	insert_inode_hash(inode);
	rawfs_attach(inode, RAWFS_I(inode)->i_name);

out:
	return inode;
}

static struct inode *rawfs_build_inode_from_dentry(struct super_block *sb,
	struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct rawfs_file_info file_info;
	struct inode *inode;

	strncpy(file_info.i_name, dentry->d_name.name, RAWFS_MAX_FILENAME_LEN+4);
	file_info.i_mode = mode;
	file_info.i_parent_folder_id = RAWFS_I(dir)->i_id;

	inode = rawfs_build_inode(sb, &file_info, -1, -1);
	return inode;
}

/*
 * Lookup the data, if the dentry didn't already exist, it must be
 * negative.  Set d_op to delete negative dentries to save memory
 * (and since it does not help performance for in memory filesystem).
 */

static struct dentry *rawfs_lookup(struct inode *dir, struct dentry *dentry,
				unsigned int flags)
{
	struct inode *inode = NULL;
	static struct dentry *result;
	struct super_block *sb = dir->i_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(dir->i_sb);
	struct rawfs_file_list_entry *entry;
/* struct nls_table *codepage = RAWFS_SB(dentry->d_inode->i_sb)->local_nls; */

	mutex_lock(&sbi->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_lookup: %s, len %d\n",
		dentry->d_name.name, dentry->d_name.len);

	if (sbi->flags & RAWFS_MNT_CASE)
		dentry->d_op = &rawfs_ci_dentry_ops;  /* case insensitive */
	else
		dentry->d_op = &rawfs_dentry_ops;	 /* case sensitive */

	entry = rawfs_file_list_get(sb, dentry->d_name.name, RAWFS_I(dir)->i_id);

	if (entry)
		inode = rawfs_build_inode_from_dentry(sb, dir, dentry,
			entry->file_info.i_mode);

	d_add(dentry, inode);

	mutex_unlock(&sbi->rawfs_lock);

	return result;
}

static int
rawfs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t dev)
{
	struct inode *inode;
	int error = -ENOSPC;

	inode = rawfs_build_inode_from_dentry(dir->i_sb, dir, dentry, mode);

	RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_mknod %s %X = %lx, i_nlink=%d\n",
		dentry->d_name.name, mode, (unsigned long)inode, inode->i_nlink);
	if (inode) {
			RAWFS_PRINT(RAWFS_DBG_INODE,
				"rawfs_mknod: parent dir ID=%X, ino=%X\n",
				RAWFS_I(dir)->i_id, (unsigned)dir->i_ino);
			if (dir->i_mode & S_ISGID) {
					inode->i_gid = dir->i_gid;
					if (S_ISDIR(mode))
							inode->i_mode |= S_ISGID;
			}
			d_instantiate(dentry, inode);
			dget(dentry);   /* Extra count - pin the dentry in core */
			error = 0;
			dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;

	/* real filesystems would normally use i_size_write function */
	dir->i_size += 0x20;  /* bogus small size for each dir entry */
	} else {
		RAWFS_PRINT(RAWFS_DBG_INODE, "mknod: inode is already exist NULL\n");
	}

	return error;
}


static int rawfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct super_block *sb = dentry->d_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct rawfs_file_info fi;
	struct rawfs_file_list_entry *entry;
	struct rawfs_inode_info *inode_info;
	int result = 0;

	mutex_lock(&sbi->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_DIR,
		"rawfs_mkdir: dentry name %s, parent id=%X, parent ino=%X\n", dentry->d_name.name,
		RAWFS_I(dir)->i_id,
		(unsigned)dir->i_ino);

	if (dir->i_ino != RAWFS_ROOT_INO) {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_mkdir: sub-folder only allowed in root dir\n");
		result = -EACCES;
		goto out;
	}

	if (dentry->d_name.len > RAWFS_MAX_FILENAME_LEN) {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_mkdir: foldername is too long -ENAMETOOLONG\n");
		result = -ENAMETOOLONG;
		goto out;
	}

	entry = rawfs_file_list_get(sb, dentry->d_name.name, RAWFS_I(dir)->i_id);

	if (entry) {
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_mkdir, target already exists\n");
		result = -EEXIST;
		goto out;
	}

	/* Do GC, if there's no free_pages */

	result = rawfs_reserve_space(sb, 1);
	if (result < 0)
		goto out;

	inc_nlink(dir);
	result = rawfs_mknod(dir, dentry, mode | S_IFDIR, 0);
	set_nlink(dentry->d_inode, 2);

	if (result < 0)
		goto out;

	result = rawfs_reg_file_create(dir, dentry, mode | S_IFDIR, 0);

	if (result < 0)
		goto out;

	/* rawfs_fill_file_info(dentry->d_inode, &fi); */
	rawfs_fill_fileinfo_by_dentry(dentry, &fi);
	inode_info = RAWFS_I(dentry->d_inode);
	rawfs_file_list_add(sb, &fi, inode_info->i_location_block,
		inode_info->i_location_page);

out:
	mutex_unlock(&sbi->rawfs_lock);
	return result;

}

static int rawfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr;
	struct rawfs_file_list_entry *entry = NULL;
	int result = 0;

	mutex_lock(&sbi->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_rmdir, dentry name %s, parent id=%X, parent ino=%X\n",
		dentry->d_name.name, RAWFS_I(dir)->i_id, (unsigned)dir->i_ino);

	/* Is the file exists ? */
	entry = rawfs_file_list_get(sb, dentry->d_name.name, RAWFS_I(dir)->i_id);

	if (!entry) {
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_rmdir, folder not exist\n");
		result = -ENOENT;
		goto out;
	}

	if (!S_ISDIR(entry->file_info.i_mode)) {
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_rmdir, target is not a folder\n");
		result = -ENOTDIR;
		goto out;
	}

	mutex_lock(&sbi->file_list_lock);

	/* Check the folder is empty */
	list_for_each_entry(ptr, &sbi->file_list, list) {
		if (ptr->file_info.i_parent_folder_id == RAWFS_I(dentry->d_inode)->i_id) {
			result = -ENOTEMPTY;
			break;
		}
	}
	mutex_unlock(&sbi->file_list_lock);

	if (result < 0)
		goto out;

	result = rawfs_reg_file_delete(dir, dentry);

	if (result < 0)
		goto out;

	/* detach from inode cache, if it exists */
	drop_nlink(dir);
	clear_nlink(dentry->d_inode);
	rawfs_detach(dentry->d_inode);

	rawfs_file_list_remove(sb, &entry->file_info);

out:
	mutex_unlock(&sbi->rawfs_lock);
	return result;
}

static int rawfs_dir_create(struct inode *dir, struct dentry *dentry,
	umode_t mode, bool excl)
{
	struct super_block *sb = dentry->d_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct rawfs_file_info fi;
	struct rawfs_file_list_entry *entry;
	struct rawfs_inode_info *inode_info;
	int result = 0;

	mutex_lock(&sbi->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_DIR,
		"rawfs_dir_create: %s, len %d\n",
		dentry->d_name.name, dentry->d_name.len);

	if (dentry->d_name.len > RAWFS_MAX_FILENAME_LEN) {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_create: filename is too long -ENAMETOOLONG\n");
		result = -ENAMETOOLONG;
		goto out;
	}

	entry = rawfs_file_list_get(sb, dentry->d_name.name, RAWFS_I(dir)->i_id);

	if (entry) {
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_dir_create, file already exists\n");
		result = -EEXIST;
		goto out;
	}

	result = rawfs_reserve_space(sb, 1);
	if (result < 0)
		goto out;

	result = rawfs_mknod(dir, dentry, mode | S_IFREG, 0);
	if (result < 0)
		goto out;

	result = rawfs_reg_file_create(dir, dentry, mode | S_IFREG, 0);

	if (result < 0)
		goto out;

	/* rawfs_fill_file_info(dentry->d_inode, &fi); */
	rawfs_fill_fileinfo_by_dentry(dentry, &fi);
	inode_info = RAWFS_I(dentry->d_inode);
	rawfs_file_list_add(sb, &fi, inode_info->i_location_block,
		inode_info->i_location_page);

out:
	mutex_unlock(&sbi->rawfs_lock);

	return result;
}

static int rawfs_dir_unlink(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dentry->d_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *entry;
	int result = 0;

	mutex_lock(&sbi->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_dir_unlink, dentry name %s\n",
		dentry->d_name.name);

	/* Is the file exists ? */
	entry = rawfs_file_list_get(sb, dentry->d_name.name, RAWFS_I(dir)->i_id);

	if (!entry) {
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_dir_unlink, file not exist\n");
		result = -ENOENT;
		goto out;
	}

	result = rawfs_reg_file_delete(dir, dentry);

	if (result < 0)
		goto out;

	/* detach from inode cache, if it exists */
	clear_nlink(dentry->d_inode);
	rawfs_detach(dentry->d_inode);

	rawfs_file_list_remove(sb, &entry->file_info);

out:
	mutex_unlock(&sbi->rawfs_lock);

	return result;
}

static int rawfs_dir_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	struct super_block *sb = old_dentry->d_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct inode *inode;
	struct inode *old_inode, *new_inode;
	struct rawfs_file_list_entry *entry;
	struct rawfs_inode_info *inode_info;
	struct timespec ts;
	int result = 0, is_dir, update_dotdot;

	mutex_lock(&sbi->rawfs_lock);

	old_inode = old_dentry->d_inode;
	new_inode = new_dentry->d_inode;
	is_dir = S_ISDIR(old_inode->i_mode);
	update_dotdot = (is_dir && old_dir != new_dir);

	RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_dir_rename, %s (%lx) -> %s (%lx)\n",
		old_dentry->d_name.name, (unsigned long)old_dentry,
		new_dentry->d_name.name, (unsigned long)new_dentry);

	if (new_dentry->d_name.len > RAWFS_MAX_FILENAME_LEN) {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_rename: filename is too long -ENAMETOOLONG, len %d\n",
			new_dentry->d_name.len);
		result = -ENAMETOOLONG;
		goto out;
	}

	/* Is target file already exist ? */
	entry = rawfs_file_list_get(sb, new_dentry->d_name.name, new_dir->i_ino);

	if (entry)	{
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_rename: target file already exists\n");
		result = -EEXIST;
		goto out;
	}

	/* Check source file existence */
	entry = rawfs_file_list_get(sb, old_dentry->d_name.name,
		RAWFS_I(old_dir)->i_id);

	if (!entry) {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_rename: source file not exists\n");
		result = -ENOENT;
		goto out;
	}

	ts = CURRENT_TIME_SEC;

	/* do GC, if the required space is not enough */
	result = rawfs_reserve_space(sb, 1);
	if (result < 0)
		goto out;

	/* No inodes will be created, */
	/* It will use original inode and dentry. */

	/* TODO: Copy inode properties */

	inode = rawfs_iget(sb, old_dentry->d_name.name, RAWFS_I(old_dir)->i_id);

	if (inode)	{
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_rename: found old inode %lx, expected %lx\n",
			(unsigned long)inode, (unsigned long)old_dentry->d_inode);
		iput(inode);
		rawfs_detach(old_dentry->d_inode);
		/* remove_inode_hash(old_dentry->d_inode); */
	}

	/* copy file with new name,
	   this function will add new entry to the file list. */
	result = rawfs_reg_file_copy(old_dir, old_dentry, new_dir, new_dentry);

	if (result < 0)
		goto out;

	/* delete the origianl copy, if renamed to new folder */
	if (old_dir->i_ino != new_dir->i_ino)
		result = rawfs_reg_file_delete(old_dir, old_dentry);

	if (result < 0)
		goto out;

	/* Note: Inode info may change after copy/delete, or during, */
	/* We need to get latest info from file list again. */
	entry = rawfs_file_list_get(sb, old_dentry->d_name.name, RAWFS_I(old_dir)->i_id);

	if (entry)
		rawfs_file_list_remove(sb, &entry->file_info);

	old_inode->i_ctime = ts;

	if (inode)  /* inode was detached, attached it back */
		rawfs_attach(inode, RAWFS_I(inode)->i_name);

	/* Update inode info */
	entry = rawfs_file_list_get(sb, new_dentry->d_name.name,
		RAWFS_I(new_dir)->i_id);

	if (entry) {
		inode_info = RAWFS_I(old_inode);
		/* inode_info = RAWFS_I(inode); */
		inode_info->i_location_block = entry->i_location_block;
		inode_info->i_location_page_count = entry->i_location_page_count;
		inode_info->i_location_page = entry->i_location_page;
		inode_info->i_parent_folder_id = entry->file_info.i_parent_folder_id;
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_rename: new location block %d, page %d\n",
			inode_info->i_location_block, inode_info->i_location_page);
	} else {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_dir_rename: new entry missing after rename\n");
	}

	if (update_dotdot) {
		drop_nlink(old_dir);
		if (!new_inode)
			inc_nlink(new_dir);
	}

	if (new_inode) {
		drop_nlink(new_inode);
		if (is_dir)
			drop_nlink(new_inode);
		new_inode->i_ctime = ts;
	}

out:
	mutex_unlock(&sbi->rawfs_lock);

	return result;
}

static int rawfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct super_block *sb = dentry->d_sb;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct inode *inode = dentry->d_inode;
	unsigned int size_limit;
	struct rawfs_file_list_entry *entry = NULL;
	int result = 0;
	bool do_add = false;

	mutex_lock(&sbi->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_setattr: %s %s, id = %X @ folder id %X, parent ino=%X\n",
		(S_ISDIR(dentry->d_inode->i_mode))?"folder":"file",
		dentry->d_name.name,
		RAWFS_I(dentry->d_inode)->i_id,
		RAWFS_I(dentry->d_parent->d_inode)->i_id,
		(unsigned)parent_ino(dentry));

	size_limit = ((sbi->pages_per_block-2) * (sbi->page_data_size));

	/* Fail if a requested resize >= 2GB */
	if (attr->ia_valid & ATTR_SIZE && (attr->ia_size > size_limit)) {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_setattr: set length %llx beyond block size limit %d\n",
			attr->ia_size, size_limit);
		result = -EINVAL;
		goto out;
	}

	entry = rawfs_file_list_get(sb, dentry->d_name.name, RAWFS_I(dentry->d_parent->d_inode)->i_id);

	if (entry)
		do_add = true;

	result = inode_change_ok(inode, attr);

	if (result)
		goto out;

	setattr_copy(inode, attr);

	if (attr->ia_valid & ATTR_SIZE) {
		truncate_setsize(inode, attr->ia_size);
		inode->i_blocks = (inode->i_size + 511) >> 9;
	}

	if (do_add)
		rawfs_reg_file_copy(dentry->d_parent->d_inode, dentry, NULL, NULL);

out:
	mutex_unlock(&sbi->rawfs_lock);

	return result;
}

int rawfs_getattr(struct vfsmount *mnt, struct dentry *dentry,
		struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;

	generic_fillattr(inode, stat);
	return 0;
}

struct inode_operations rawfs_file_inode_ops = {
	.getattr		= rawfs_getattr,
	.setattr		= rawfs_setattr,
};

struct inode_operations rawfs_dir_inode_ops = {
	.create		= rawfs_dir_create,
	.lookup		= rawfs_lookup,
	.unlink		= rawfs_dir_unlink, /* simple_unlink, */
	.mkdir		= rawfs_mkdir,
	.rmdir		= rawfs_rmdir,
	.mknod		= rawfs_mknod,
	.rename		= rawfs_dir_rename, /* simple_rename, */
	.getattr	= rawfs_getattr,
	.setattr	= rawfs_setattr,
};

int rawfs_fill_inode(struct inode *inode, struct rawfs_file_info *file_info,
	int block_no, int page_no, umode_t mode, dev_t dev)
{
	struct rawfs_sb_info	*sbi = RAWFS_SB(inode->i_sb);
	struct rawfs_inode_info *inode_info;
	int subfolders;

	inode->i_mode = mode;
	inode->i_blocks = 0;
	inode->i_mapping->a_ops = &rawfs_aops;
	inode->i_mapping->backing_dev_info = &rawfs_backing_dev_info;
	inode_info = RAWFS_I(inode);

	switch (mode & S_IFMT) {
	default:
		RAWFS_PRINT(RAWFS_DBG_INODE, KERN_INFO
			"rawfs_fill_inode: special inode\n");
		init_special_inode(inode, mode, dev);
		break;

	case S_IFREG:
#if 0
		if ((block_no >= 0) && (page_no < 0)) { /* Speical block file */
			RAWFS_PRINT(RAWFS_DBG_INODE,
				"rawfs_fill_inode: block file %d, %lx -> %lx\n",
				block_no, (unsigned long)inode,
				(unsigned long)inode_info);
			rawfs_fill_inode_blk(inode, file_info, block_no, page_no);
			inode->i_fop  = &rawfs_block_file_operations;
		} else
#endif
		if (file_info != NULL) { /* Regular file */
			rawfs_fill_inode_reg(inode, file_info, block_no, page_no);
			inode->i_fop =  &rawfs_file_operations;
			inode->i_blocks = CEILING((unsigned)inode->i_size,
				sbi->page_data_size) * sbi->page_size >> 9;
			inode->i_size = file_info->i_size;
			RAWFS_PRINT(RAWFS_DBG_INODE,
				"rawfs_fill_inode: reg file inode %s, size %lld bytes, %d blocks, i_blkbits = %d\n",
				file_info->i_name, inode->i_size,
				(unsigned)inode->i_blocks, inode->i_blkbits);
		}
		inode->i_op = &rawfs_file_inode_ops;
		break;

	case S_IFDIR:
		inode->i_op = &rawfs_dir_inode_ops;
		inode->i_fop = &rawfs_dir_operations;
		/* 2 for initial ".." and "." entries */
		subfolders = rawfs_subdirs(inode) + 2;
		RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_fill_inode: folder, nlink=%d\n",
			subfolders);
		set_nlink(inode, subfolders);

		if (file_info != NULL) { /* folder */
			RAWFS_PRINT(RAWFS_DBG_INODE,
				"rawfs_fill_inode: folder inode %s\n", file_info->i_name);
			inode->i_blocks = sbi->sectors_per_page;

			rawfs_fill_inode_reg(inode, file_info, block_no, page_no);
			inode->i_size = 0;
		} else { /* Super inode */
			RAWFS_PRINT(RAWFS_DBG_INODE, "rawfs_fill_inode: super inode\n");
			inode->i_ino = RAWFS_ROOT_INO;
			RAWFS_I(inode)->i_id = RAWFS_ROOT_DIR_ID;
		}

		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_fill_inode);


MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_DESCRIPTION("RAW file system for NAND flash");
MODULE_LICENSE("GPL");

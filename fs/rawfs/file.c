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
#include <linux/aio.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include "rawfs.h"

#define CEILING(x, y) rawfs_div(((x)+(y)-1), (y))
#define FLOOR(x, y)   rawfs_div((x), (y))
#define page_uptodate(page)	test_bit(PG_uptodate, &(page)->flags)

/* ----------------------------------------------------------------------------- */
/* Block File Operation */
/* ----------------------------------------------------------------------------- */
/**
 * rawfs_block_file_aio_read - read routine for block files
 * @iocb:	kernel I/O control block
 * @iov:	io vector request
 * @nr_segs:	number of segments in the iovec
 * @pos:	current file position
 */
ssize_t
rawfs_block_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	struct super_block *sb = filp->f_path.dentry->d_sb;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct inode *inode = filp->f_mapping->host;

	ssize_t retval;
	loff_t *ppos = &iocb->ki_pos;
	/* Always use direct I/O */
	loff_t size;
	int block_no;

	retval = iov_length(iov, nr_segs);

	mutex_lock(&rawfs_sb->rawfs_lock);

	RAWFS_PRINT(RAWFS_DBG_FILE,
		"rawfs_block_file_aio_read %s, pos %lld, len %ld\n",
		RAWFS_I(inode)->i_name, pos, (unsigned long)retval);

	size = i_size_read(inode);

	/* Get inode ID */
	block_no = filp->f_path.dentry->d_inode->i_ino - RAWFS_BLOCK0_INO;

	if ((retval + pos) >= size)
		retval = size - pos;

	if (pos < size) {
		rawfs_sb->dev.read_page_user(filp->f_path.dentry->d_inode->i_sb,
			block_no, pos, iov, nr_segs, retval);

		if (retval > 0)
			*ppos = pos + retval;

		if (retval < 0 || *ppos >= size) {
			file_accessed(filp);
			goto out;
		}
	}

out:

	mutex_unlock(&rawfs_sb->rawfs_lock);

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_block_file_aio_read);

/**
 * rawfs_block_file_aio_write - write data to a block file
 * @iocb:	IO state structure
 * @iov:	vector with data to write
 * @nr_segs:	number of segments in the vector
 * @pos:	position in file where to write
 */

ssize_t rawfs_block_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	ssize_t retval;
	loff_t *ppos = &iocb->ki_pos;

	retval = iov_length(iov, nr_segs);

	RAWFS_PRINT(RAWFS_DBG_FILE,
		"rawfs_block_file_aio_write %s, pos %lld, len %ld\n",
		RAWFS_I(filp->f_mapping->host)->i_name, pos, (unsigned long)retval);

	if (retval > 0)
		*ppos = pos + retval;

	file_accessed(filp);

	/* This function is not supported, data will not write to the device */

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_block_file_aio_write);

/* ----------------------------------------------------------------------------- */
/* Regular File Operation */
/* ----------------------------------------------------------------------------- */
ssize_t rawfs_reg_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	struct super_block *sb = filp->f_path.dentry->d_sb;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct inode *inode = filp->f_mapping->host;
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);
	ssize_t retval;
	loff_t *ppos = &iocb->ki_pos;
	loff_t size;
	unsigned int curr_file_pos = pos;
	unsigned int curr_buf_pos = 0;
	int remain_buf_size;
	const struct iovec *iv = &iov[0];  /* TODO: Process all io vectors */

	RAWFS_PRINT(RAWFS_DBG_FILE,
		"rawfs_reg_file_aio_read %s\n",	inode_info->i_name);

	mutex_lock(&rawfs_sb->rawfs_lock);

	retval = iov_length(iov, nr_segs);

	size = i_size_read(inode);

	RAWFS_PRINT(RAWFS_DBG_FILE,
		"rawfs_reg_file_aio_read %s, pos %lld, len %ld, filesize: %lld\n",
		inode_info->i_name, pos, (unsigned long)retval, size);

	if ((retval + pos) >= size)
		retval = size - pos;

	if (pos < size) {
		/* Read File */
		{
			int preceding_pages, rear_pages;
			struct rawfs_page *page_buf = NULL;
			int i;

			/* Prepare page buffer */
			page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

			if (page_buf == NULL) {
				retval = 0;
				goto out;
			}

			preceding_pages = FLOOR((unsigned)pos, rawfs_sb->page_data_size);
			rear_pages = CEILING((unsigned)pos + retval,
				rawfs_sb->page_data_size);
			remain_buf_size = retval;

			RAWFS_PRINT(RAWFS_DBG_FILE,
				"rawfs_reg_file_aio_read %s, preceding %d, rear %d, remain %d\n",
				inode_info->i_name, preceding_pages, rear_pages,
				remain_buf_size);

			/* Step 1: Copy preceding pages, if starting pos is not 0. */
			for (i = preceding_pages; i < rear_pages; i++) {
				__u32 crc;

				/* Read page */
				rawfs_sb->dev.read_page(sb,
					inode_info->i_location_block,
					inode_info->i_location_page+i,
					page_buf);

				/* TODO: skip this page, if unrecoverable error occurs */

				/* CRC error should not happen,
				   since we have already check them at bootup */
				crc = rawfs_page_crc_data(sb, page_buf);

				if (crc != page_buf->i_crc)
					RAWFS_PRINT(RAWFS_DBG_FILE,
						"rawfs_reg_file_aio_read: %s @ %X, crc fail %X, expected %X\n",
						page_buf->i_info.i_file_info.i_name,
						page_buf->i_info.i_file_info.i_parent_folder_id, crc,
						page_buf->i_crc);
				else
					RAWFS_PRINT(RAWFS_DBG_FILE,
						"rawfs_reg_file_aio_read: %s @ %X, crc %X\n",
						page_buf->i_info.i_file_info.i_name,
						page_buf->i_info.i_file_info.i_parent_folder_id,
						page_buf->i_crc);

				/* Copy required parts */
				{
					int start_in_buf;
					int copy_len;

					start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
					copy_len =  ((start_in_buf + remain_buf_size) >
						rawfs_sb->page_data_size) ?
						(rawfs_sb->page_data_size - start_in_buf) :
						remain_buf_size;

					if (copy_to_user((char *)iv->iov_base + curr_buf_pos,
						&page_buf->i_data[0] + start_in_buf, copy_len)) {
						retval = -EFAULT;
						goto out2;
					}

					RAWFS_PRINT(RAWFS_DBG_FILE,
						"rawfs_reg_file_aio_read %s, %d, curr %d, remain %d start %d copy %d starting %X\n",
						inode_info->i_name, i, curr_buf_pos, remain_buf_size,
						start_in_buf, copy_len,
						*(unsigned int *)(&page_buf->i_data[0] + start_in_buf));

					curr_buf_pos	+= copy_len;
					remain_buf_size -= copy_len;
				}
			}

out2:
			kfree(page_buf);
		}

		if (retval > 0)
			*ppos = pos + retval;

		if (retval < 0 || *ppos >= size) {
			file_accessed(filp);
			goto out;
		}
	} else {
		retval = 0;
	}

out:
	/* Release Lock */
	mutex_unlock(&rawfs_sb->rawfs_lock);

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_reg_file_aio_read);

void rawfs_fill_fileinfo_by_dentry(struct dentry *dentry,
	struct rawfs_file_info *file_info)
{
	rawfs_fill_file_info(dentry->d_inode, file_info);
	file_info->i_parent_folder_id = RAWFS_I(dentry->d_parent->d_inode)->i_id;

	/* Dentry only has 32 bytes, file it with iname */
	strncpy(file_info->i_name, dentry->d_name.name, RAWFS_MAX_FILENAME_LEN+4);
}
EXPORT_SYMBOL_GPL(rawfs_fill_fileinfo_by_dentry);

void rawfs_fill_file_info(struct inode *inode,
	struct rawfs_file_info *file_info)
{
	int name_len;
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);

	name_len = strlen(inode_info->i_name);
	file_info->i_atime = inode->i_atime;
	file_info->i_mtime = inode->i_mtime;
	file_info->i_ctime = inode->i_ctime;
	file_info->i_uid = inode->i_uid.val;
	file_info->i_gid = inode->i_gid.val;
	file_info->i_mode = inode->i_mode;
	file_info->i_size = inode->i_size;
	file_info->i_id = inode_info->i_id;
	file_info->i_parent_folder_id = RAWFS_ROOT_DIR_ID;

/* inode->i_ino = rawfs_file_unique_ino(file_info->i_name, name_len); */
/* inode->i_fop =  &rawfs_file_operations; */
	strncpy(file_info->i_name, inode_info->i_name, name_len+1);
/* inode_info->i_location_block = block_no; */
/* inode_info->i_location_page = page_no; */
/* inode_info->i_location_page_count = file_info->i_chunk_total; */
}
EXPORT_SYMBOL_GPL(rawfs_fill_file_info);

ssize_t rawfs_reg_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	struct super_block *sb = filp->f_path.dentry->d_sb;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
/* struct address_space *mapping=filp->f_mapping; */
	struct inode *inode = filp->f_mapping->host;
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);

	ssize_t retval;
/* size_t count; */
	loff_t *ppos = &iocb->ki_pos;
	struct rawfs_file_info *fi = NULL;
	unsigned int curr_file_pos = pos;
	unsigned int curr_buf_pos = 0;
	int starting_page, total_pages;
	int remain_buf_size;
	int result;
	struct rawfs_file_list_entry *entry;
	const struct iovec *iv = &iov[0];  /* TODO: Process all io vectors */

	/* Always use direct I/O */
	/* loff_t size; */
	/* int block_no; */

	/* Get Lock */
	mutex_lock(&rawfs_sb->rawfs_lock);

	retval = iov_length(iov, nr_segs);

	RAWFS_PRINT(RAWFS_DBG_FILE,
		"rawfs_reg_file_aio_write %s, pos %lld, len %ld\n",
		inode_info->i_name, pos, (unsigned long)retval);

	fi = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);

	if (!fi) {
		retval = 0;
		goto out;
	}

	/* rawfs_fill_file_info(inode , fi); */
	rawfs_fill_fileinfo_by_dentry(filp->f_path.dentry, fi);

	/* Update file_info */
	if ((pos+retval) > fi->i_size)
		fi->i_size = pos+retval;

	fi->i_chunk_index = 1;
	fi->i_chunk_total = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
		rawfs_sb->page_data_size);

	/* do GC, if the required space is not enough */
	result = rawfs_reserve_space(sb, fi->i_chunk_total);
	if (result < 0) {
		retval = result;
		goto out;
	}

	/* get entry from file list */
	entry = rawfs_file_list_get(sb, fi->i_name, fi->i_parent_folder_id);

	if (!entry) {
		RAWFS_PRINT(RAWFS_DBG_FILE,
			"rawfs_reg_file_aio_write: %s file list entry missing\n", fi->i_name);
		dump_stack();
		goto out;
	}

	/* Write File */
	{
		int preceding_pages, rear_pages;
		struct rawfs_page *page_buf = NULL;
		int i;

		/* Prepare page buffer */
		page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

		if (page_buf == NULL) {
			retval = 0;
			goto out;
		}

		starting_page = rawfs_sb->data_block_free_page_index;
		preceding_pages = FLOOR((unsigned)pos, rawfs_sb->page_data_size);
		total_pages = CEILING((unsigned)fi->i_size, rawfs_sb->page_data_size);
		rear_pages = CEILING((unsigned)pos + iv->iov_len,
			rawfs_sb->page_data_size);
		remain_buf_size = iv->iov_len;

		RAWFS_PRINT(RAWFS_DBG_FILE,
			"rawfs_reg_file_aio_write %s, preceding %d, rear %d, total %d, remain %d\n",
			 inode_info->i_name, preceding_pages, rear_pages, total_pages,
			 remain_buf_size);


		/* Step 1: Copy preceding pages, if starting pos is not 0. */
		for (i = 0; i < total_pages; i++)	{
			/* Read, if necessary, (no need for new files) */
			if ((i <= preceding_pages) || (i >= rear_pages))
				rawfs_sb->dev.read_page(sb,
					entry->i_location_block,
					entry->i_location_page+i,
					page_buf);

			/* Update info */
			memcpy(&page_buf->i_info.i_file_info, fi,
				sizeof(struct rawfs_file_info));

			/* Within Modify Range: Copy Modify */
			if ((i >= preceding_pages) && (i <= rear_pages)) {
				int start_in_buf;
				int copy_len;

				start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
				copy_len =  ((start_in_buf + remain_buf_size) >
							rawfs_sb->page_data_size) ?
							(rawfs_sb->page_data_size - start_in_buf) :
							remain_buf_size;

				if (copy_from_user(&page_buf->i_data[0] + start_in_buf,
					(char *)iv->iov_base + curr_buf_pos, copy_len))	{
					retval = -EFAULT;
					goto out2;
				}

				curr_buf_pos	+= copy_len;
				remain_buf_size -= copy_len;

				RAWFS_PRINT(RAWFS_DBG_FILE,
					"rawfs_reg_file_aio_write %s, %d, curr %d, remain %d start %d copy %d starting %X\n",
				inode_info->i_name, i, curr_buf_pos, remain_buf_size,
				start_in_buf, copy_len,
				*(unsigned int *)(&page_buf->i_data[0] + start_in_buf));
			}

			rawfs_page_signature(sb, page_buf);

			/* Write */
			rawfs_sb->dev.write_page(sb,
				rawfs_sb->data_block,
				rawfs_sb->data_block_free_page_index,
				page_buf);

			rawfs_sb->data_block_free_page_index++;
			fi->i_chunk_index++;
		}

out2:
		kfree(page_buf);
	}

	if (retval > 0)
		*ppos = pos + retval;

	file_accessed(filp);

	/* Update Inode: file size, block, page */
	i_size_write(inode, fi->i_size);
	/* TODO: Get inode lock when update */
	inode_info->i_location_block = rawfs_sb->data_block;
	inode_info->i_location_page = starting_page;
	inode_info->i_location_page_count = total_pages;

	/* update location */
	entry->i_location_block = rawfs_sb->data_block;
	entry->i_location_page = starting_page;
	entry->i_location_page_count = total_pages;

out:
	kfree(fi);

	/* Release Lock */
	mutex_unlock(&rawfs_sb->rawfs_lock);

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_reg_file_aio_write);

/* ----------------------------------------------------------------------------- */
/* Internal Functions */
/* ----------------------------------------------------------------------------- */
int rawfs_reg_file_create(struct inode *dir, struct dentry *dentry,
	umode_t mode, struct nameidata *nd)
{
	int result = 0;
	struct super_block *sb = dentry->d_sb;
	struct inode *inode = dentry->d_inode;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);
	struct rawfs_page *page_buf = NULL;

	RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_create: i_name=%s\n",
		inode_info->i_name);

	page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

	if (page_buf == NULL) {
		result = -ENOMEM;
		goto out;
	}

	result = rawfs_reserve_space(sb, 1);
	if (result < 0)
		goto out;

	/* rawfs_fill_file_info(inode, &page_buf->i_info.i_file_info); */
	rawfs_fill_fileinfo_by_dentry(dentry, &page_buf->i_info.i_file_info);

	page_buf->i_info.i_file_info.i_mode = mode;
	page_buf->i_info.i_file_info.i_size = 0;
	page_buf->i_info.i_file_info.i_chunk_total = 1;
	page_buf->i_info.i_file_info.i_chunk_index = 1;

	rawfs_page_signature(sb, page_buf);

	rawfs_sb->dev.write_page(sb,
		rawfs_sb->data_block,
		rawfs_sb->data_block_free_page_index,
		page_buf);


	/* Update inode_info */
	inode_info->i_location_block = rawfs_sb->data_block;
	inode_info->i_location_page = rawfs_sb->data_block_free_page_index;
	inode->i_size = 0;

	rawfs_sb->data_block_free_page_index++;

out:
	kfree(page_buf);
	return result;

}
EXPORT_SYMBOL_GPL(rawfs_reg_file_create);

int rawfs_reg_file_delete(struct inode *dir, struct dentry *dentry)
{
	int result = 0;
	struct super_block *sb = dentry->d_sb;
	struct inode *inode = dentry->d_inode;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);
	struct rawfs_page *page_buf = NULL;

	RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_delete %s\n",
		dentry->d_name.name);

	page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

	if (page_buf == NULL) {
		result = -ENOMEM;
		goto out;
	}

	/* Do GC, if there's no free_pages */
	if ((rawfs_sb->data_block_free_page_index+1) > rawfs_sb->pages_per_block) {
		int reclaimed_size;

		reclaimed_size = rawfs_dev_garbage_collection(sb);
		if (reclaimed_size <= 0) {
			RAWFS_PRINT(RAWFS_DBG_FILE,
				"rawfs_reg_file_delete: Disk full: %d\n", reclaimed_size);
			result = -ENOSPC;
			goto out;
		}
	}

	/* Read existing data */
	rawfs_sb->dev.read_page(sb,
		inode_info->i_location_block,
		inode_info->i_location_page,
		page_buf);

	/* Set i_chun_total as zero to mark delete. */
	page_buf->i_info.i_file_info.i_chunk_total = -1;
	page_buf->i_info.i_file_info.i_chunk_index = -1;
	page_buf->i_info.i_file_info.i_size		 = 0;

	rawfs_page_signature(sb, page_buf);

	rawfs_sb->dev.write_page(sb,
		rawfs_sb->data_block,
		rawfs_sb->data_block_free_page_index,
		page_buf);

	rawfs_sb->data_block_free_page_index++;

out:
	kfree(page_buf);

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_reg_file_delete);

/* This function is used by rawfs_dir_rename() & rawfs_setattr()
   This function will put the new copeid file into file list. */
int rawfs_reg_file_copy(struct inode *src_dir, struct dentry *src_dentry,
	struct inode *dest_dir, struct dentry *dest_dentry)
{
	int result = 0;
	int starting_page;
	struct inode *src_inode = src_dentry->d_inode;
	struct super_block *sb = src_dentry->d_sb;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_file_info *fi = NULL;
	struct rawfs_file_list_entry *src_info;

	if ((dest_dir == NULL) && (dest_dentry == NULL)) {
		RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_copy (setattr) %s\n",
			src_dentry->d_name.name);
		dest_dir = src_dir;
		dest_dentry = src_dentry;
	} else {
		RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_reg_file_copy (rename) %s -> %s\n",
			src_dentry->d_name.name, dest_dentry->d_name.name);
	}

	fi = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);

	if (!fi) {
		result = -ENOMEM;
		goto out;
	}

	rawfs_fill_file_info(src_inode , fi);
	fi->i_parent_folder_id = RAWFS_I(dest_dir)->i_id;

	strncpy(fi->i_name, dest_dentry->d_name.name,
		strlen(dest_dentry->d_name.name)+1);

	fi->i_chunk_index = 1;
	fi->i_chunk_total = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
			rawfs_sb->page_data_size);

	/* do GC, if the required space is not enough */
	result = rawfs_reserve_space(sb, fi->i_chunk_total);
	if (result < 0)
		goto out;

	/* Get soruce location & starting page again,
	   it might be change to new location after GC */
	src_info = rawfs_file_list_get(sb,
		src_dentry->d_name.name,
		RAWFS_I(src_dir)->i_id);

	starting_page = rawfs_sb->data_block_free_page_index;

	if (!src_info) {
		RAWFS_PRINT(RAWFS_DBG_FILE,
			"rawfs_reg_file_copy: src file %s @ %X missing after GC\n",
			src_dentry->d_name.name,
			RAWFS_I(src_dir)->i_id);
		goto out;
	}

	{
		int total_pages;
		struct rawfs_page *page_buf = NULL;
		int i;

		page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

		if (page_buf == NULL) {
			result = -ENOMEM;
			goto out;
		}

		total_pages = S_ISDIR(fi->i_mode) ? 1 : CEILING((unsigned)fi->i_size,
			rawfs_sb->page_data_size);

		for (i = 0; i < total_pages; i++) {
			RAWFS_PRINT(RAWFS_DBG_FILE,
				"rawfs_reg_file_copy copy %d,%d to %d,%d\n",
				src_info->i_location_block,
				src_info->i_location_page+i,
				rawfs_sb->data_block,
				rawfs_sb->data_block_free_page_index);

			rawfs_sb->dev.read_page(sb,
				src_info->i_location_block,
				src_info->i_location_page+i,
				page_buf);

			/* Update info */
			memcpy(&page_buf->i_info.i_file_info, fi,
				sizeof(struct rawfs_file_info));

			rawfs_page_signature(sb, page_buf);

			rawfs_sb->dev.write_page(sb,
				rawfs_sb->data_block,
				rawfs_sb->data_block_free_page_index,
				page_buf);

			rawfs_sb->data_block_free_page_index++;
			fi->i_chunk_index++;
		}

		kfree(page_buf);
	}

	/* Add to file list, so that rawf_dir_rename can get the new
		location of the file from list. */
	rawfs_file_list_add(sb, fi, rawfs_sb->data_block, starting_page);

out:
	kfree(fi);
	return result;
}
EXPORT_SYMBOL_GPL(rawfs_reg_file_copy);

/* Read page, by pass page cache, always read from device. */
static int rawfs_readpage_nolock(struct file *filp, struct page *page)
{
	struct super_block *sb = filp->f_path.dentry->d_sb;
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_inode_info *inode_info = RAWFS_I(inode);
	loff_t pos;
	unsigned int curr_file_pos;
	unsigned int curr_buf_pos = 0;
	int remain_buf_size;
	unsigned size;
	int retval;
	unsigned char *pg_buf;

/* pg_buf = kmap_atomic(page); */
	pg_buf = kmap(page);

	/* TODO: check pg_buf */

	size = i_size_read(inode);
	curr_file_pos = pos = page->index << PAGE_CACHE_SHIFT;
	retval = PAGE_CACHE_SIZE;

	RAWFS_PRINT(RAWFS_DBG_FILE,
		"rawfs_readpage %s @ folder %X, page %ld, pos %lld, len %d, filesize: %d, pg_buf %lx\n",
		inode_info->i_name,
		inode_info->i_parent_folder_id,
		page->index,
		pos, retval, size, (unsigned long)pg_buf);

	if ((retval + pos) >= size)
		retval = size - pos;

	if (pos < size) {
		{
			int preceding_pages, rear_pages;
			struct rawfs_page *page_buf = NULL;
			int i;

			/* Prepare page buffer */
			page_buf = kzalloc(rawfs_sb->page_size, GFP_NOFS);

			if (page_buf == NULL)	{
				retval = 0;
				goto out;
			}

			preceding_pages = FLOOR((unsigned)pos, rawfs_sb->page_data_size);
			rear_pages = CEILING((unsigned)pos + retval,
				rawfs_sb->page_data_size);
			remain_buf_size = retval;

			RAWFS_PRINT(RAWFS_DBG_FILE,
				"rawfs_readpage %s, preceding %d, rear %d, remain %d ",
				inode_info->i_name, preceding_pages, rear_pages,
				remain_buf_size);

			/* Step 1: Copy preceding pages, if starting pos is not 0. */
			for (i = preceding_pages; i < rear_pages; i++)	{
				/* Read page */
				rawfs_sb->dev.read_page(sb,
					inode_info->i_location_block,
					inode_info->i_location_page+i,
					page_buf);

				/* Copy required parts */
				{
					int start_in_buf;
					int copy_len;

					start_in_buf = (curr_file_pos % rawfs_sb->page_data_size);
					copy_len =  ((start_in_buf + remain_buf_size) >
								  rawfs_sb->page_data_size) ?
								(rawfs_sb->page_data_size - start_in_buf) :
								remain_buf_size;

					memcpy(pg_buf + curr_buf_pos,
						&page_buf->i_data[0] + start_in_buf, copy_len);

					RAWFS_PRINT(RAWFS_DBG_FILE,
						"rawfs_readpage %s, %d, curr %d, remain %d start %d copy %d pattern %X",
						inode_info->i_name, i, curr_buf_pos, remain_buf_size,
						start_in_buf, copy_len,
						*(unsigned int *)(&page_buf->i_data[0] + start_in_buf));

					curr_buf_pos	+= copy_len;
					remain_buf_size -= copy_len;
				}
			}

			kfree(page_buf);
		}
	} else {
		retval = 0;
	}

out:

	SetPageUptodate(page);
	ClearPageError(page);
	flush_dcache_page(page);

	/* kunmap_atomic(pg_buf); */
	kunmap(page);
	unlock_page(page);

	return 0;
}


int rawfs_readpage(struct file *filp, struct page *page)
{
	int result;

	result = rawfs_readpage_nolock(filp, page);
	unlock_page(page);

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_readpage);

int rawfs_write_begin(struct file *filp, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_begin: unexpected call !");
	BUG();
	return simple_write_begin(filp, mapping, pos, len, flags, pagep, fsdata);
}
EXPORT_SYMBOL_GPL(rawfs_write_begin);

int rawfs_write_end(struct file *filp, struct address_space *mapping,
			   loff_t pos, unsigned len, unsigned copied,
			   struct page *pg, void *fsdata)
{
	RAWFS_PRINT(RAWFS_DBG_FILE, "rawfs_write_end: unexpected call !");
	BUG();
	return simple_write_end(filp, mapping, pos, len, copied, pg, fsdata);
}
EXPORT_SYMBOL_GPL(rawfs_write_end);

MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_DESCRIPTION("RAW file system for NAND flash");
MODULE_LICENSE("GPL");

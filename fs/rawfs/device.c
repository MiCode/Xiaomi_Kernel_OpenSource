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
#include <linux/pagemap.h>
#include <linux/version.h>
#include <linux/nls.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/crc32.h>
#include "rawfs.h"

#define CEILING(x, y) rawfs_div(((x)+(y)-1), (y))
#define FLOOR(x, y)   rawfs_div((x), (y))

/* ----------------------------------------------------------------------------- */
/* Device Level Access */
/* ----------------------------------------------------------------------------- */
#define RAWFS_NAND_PAGE_FOOTER(sb, ptr) (*((unsigned int *) \
	(((char *)ptr)+(sb->page_size-4))))

/*---------------------------------------------------------------------------*/
/* Layer 0: Address translation functions */
/*---------------------------------------------------------------------------*/
static int rawfs_block_addr(struct super_block *sb, int block_no, int offset)
{
	struct rawfs_sb_info *rawfs_sb;
	int result;

	rawfs_sb = RAWFS_SB(sb);
	result = block_no * RAWFS_NAND_BLOCK_SIZE(rawfs_sb) + offset;

	RAWFS_PRINT(RAWFS_DBG_DEVICE, "rawfs_block_addr %d, %d, %d = %d\n",
		block_no, RAWFS_NAND_BLOCK_SIZE(rawfs_sb), offset, result);

	return result;
}

static int rawfs_page_addr(struct super_block *sb, int block_no, int page_no)
{
	struct rawfs_sb_info *rawfs_sb;

	rawfs_sb = RAWFS_SB(sb);
	return block_no*RAWFS_NAND_BLOCK_SIZE(rawfs_sb) +
		page_no*RAWFS_NAND_PAGE_SIZE(rawfs_sb);
}

/*---------------------------------------------------------------------------*/
/* Layer 1: Device functions: RAM disk */
/*---------------------------------------------------------------------------*/
#ifdef RAWFS_RAM_DISK

int rawfs_dev_ram_erase_block(struct super_block *sb, int block_no)
{

	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	char *fake_block = rawfs_sb->fake_block;

	RAWFS_PRINT(RAWFS_DBG_DEVICE, "rawfs_dev_ram_erase_block: %d\n", block_no);

	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb)) {
		memset(&fake_block[rawfs_page_addr(sb, block_no, 0)], 0xFF,
			RAWFS_NAND_BLOCK_SIZE(rawfs_sb));
	}
	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_dev_ram_erase_block);

int rawfs_dev_ram_read_page_user(struct super_block *sb, int block_no,
	int block_offset, const struct iovec *iov, unsigned long nr_segs, int size)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	char *fake_block = rawfs_sb->fake_block;

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_dev_ram_read_page_user: block %d, addr %d, size %d\n",
		block_no, block_offset, size);

	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb) &&
		(block_offset+size) <= RAWFS_NAND_BLOCK_SIZE(rawfs_sb)) {
		unsigned long   seg;
		unsigned long copied;

		RAWFS_PRINT(RAWFS_DBG_DEVICE, "iov_base %08X, ram buffer = %08X\n",
			(unsigned)iov->iov_base,
			(unsigned)&fake_block[rawfs_block_addr(
				sb, block_no, block_offset)]);

		for (seg = 0; seg < nr_segs && size > 0; seg++) {
			const struct iovec *iv = &iov[seg];
			/* if (access_ok(access_flags, iv->iov_base, iv->iov_len)) */

			RAWFS_PRINT(RAWFS_DBG_DEVICE,
				"iv_base %08X, iv_len = %08X, ram_offset = %08X\n",
				(unsigned int) iv->iov_base,
				(unsigned int) ((size > iv->iov_len) ? iv->iov_len : size),
				(unsigned int) &fake_block[rawfs_block_addr(
					sb, block_no, block_offset)]);

			copied = copy_to_user(iv->iov_base,
				fake_block + rawfs_block_addr(sb, block_no, block_offset),
				(size > iv->iov_len) ? iv->iov_len : size);
			size -= iv->iov_len;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_dev_ram_read_page_user);

int rawfs_dev_ram_write_page(struct super_block *sb, int block_no, int page_no,
	void *buffer)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	char *fake_block = rawfs_sb->fake_block;

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_dev_ram_write_page: block %d, page %d\n", block_no, page_no);
	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb) &&
		page_no < RAWFS_NAND_PAGES(rawfs_sb)) {
		memcpy(fake_block + rawfs_page_addr(sb, block_no, page_no), buffer,
			RAWFS_NAND_PAGE_SIZE(rawfs_sb));
	}
	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_dev_ram_write_page);

int rawfs_dev_ram_read_page(struct super_block *sb, int block_no, int page_no,
	void *buffer)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	char *fake_block = rawfs_sb->fake_block;

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_dev_ram_read_page: block %d, page %d\n", block_no, page_no);
	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb) &&
		page_no < RAWFS_NAND_PAGES(rawfs_sb)) {
		memcpy(buffer, fake_block + rawfs_page_addr(sb, block_no, page_no),
			RAWFS_NAND_PAGE_SIZE(rawfs_sb));
	}
	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_dev_ram_read_page);

#endif /* RAWFS_RAM_DISK */

/*---------------------------------------------------------------------------*/
/* Layer 1: Device functions: MTD */
/*---------------------------------------------------------------------------*/
int rawfs_dev_mtd_erase_block(struct super_block *sb, int block_no)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct mtd_info *mtd = RAWFS_MTD(sb);
	struct erase_info ei;
	int retval = 0;
	u32 addr;

	addr = ((loff_t) block_no) * RAWFS_NAND_BLOCK_SIZE(rawfs_sb);

	RAWFS_PRINT(RAWFS_DBG_DEVICE, "rawfs_dev_mtd_erase_block: %d @ 0x%X\n",
		block_no, addr);

	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb)) {
		ei.mtd = mtd;
		ei.addr = addr;
		ei.len = RAWFS_NAND_BLOCK_SIZE(rawfs_sb);
		ei.time = 1000;
		ei.retries = 2;
		ei.callback = NULL;
		ei.priv = (u_long) sb;

		retval = mtd_erase(mtd, &ei);

		if (retval)
			RAWFS_PRINT(RAWFS_DBG_DEVICE,
				"rawfs_dev_mtd_erase_block: mtd error %d\n", retval);
		else
			RAWFS_PRINT(RAWFS_DBG_DEVICE, "rawfs_dev_mtd_erase_block: done\n");
	}
	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_dev_mtd_erase_block);

int rawfs_dev_mtd_read_page_user(struct super_block *sb, int block_no,
	int block_offset, const struct iovec *iov, unsigned long nr_segs, int size)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct mtd_info *mtd = RAWFS_MTD(sb);
	loff_t addr = ((loff_t) rawfs_block_addr(sb, block_no, block_offset));
	int result = 0;
	void *page_buffer;
	size_t dummy;
	int retval = 0;

	page_buffer = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);

	if (!page_buffer) {
		result = -ENOMEM;
		goto out;
	}

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_dev_mtd_read_page_user: block %d, addr %lld, size %d\n",
		block_no, addr, size);

	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb) &&
		(block_offset+size) <= RAWFS_NAND_BLOCK_SIZE(rawfs_sb)) {
		unsigned long   seg;
		unsigned long copied;

		RAWFS_PRINT(RAWFS_DBG_DEVICE,
			"rawfs_dev_mtd_read_page_user: iov_base %lx, addr = %lx\n",
			(unsigned long)iov->iov_base,
			(unsigned long)addr);

		for (seg = 0; seg < nr_segs && size > 0; seg++) {
			const struct iovec *iv = &iov[seg];
			unsigned int read_length = 0;
			unsigned int remain_length = iv->iov_len;
			u8 *user_addr = iv->iov_base;

			RAWFS_PRINT(RAWFS_DBG_DEVICE,
				"rawfs_dev_mtd_read_page_user: seg %ld, base %lx, length %08X\n",
				seg, (unsigned long)iv->iov_base, (unsigned)iv->iov_len);

			/* if (access_ok(access_flags, iv->iov_base, iv->iov_len)) */

			while (remain_length > 0) {
				unsigned read = 0;

				read = (size > remain_length) ? remain_length : size;
				read = (read > RAWFS_NAND_PAGE_SIZE(rawfs_sb)) ?
					RAWFS_NAND_PAGE_SIZE(rawfs_sb) : read;

				RAWFS_PRINT(RAWFS_DBG_DEVICE,
					"user_addr %lx, phy_addr = %lx, read = %08X, reamin = %08X\n",
					(unsigned long) user_addr,
					(unsigned long) addr,
					(unsigned int) read_length,
					(unsigned int) remain_length);

				retval = mtd_read(mtd, addr, RAWFS_NAND_PAGE_SIZE(rawfs_sb),
					&dummy, page_buffer);

				if (retval) {
					RAWFS_PRINT(RAWFS_DBG_DEVICE,
						"rawfs_dev_mtd_read_page: block %d, page %d, offset %X, mtd error %d\n",
						block_no,
						rawfs_div(addr, RAWFS_NAND_PAGE_SIZE(rawfs_sb)),
						(unsigned)addr, retval);
				}

				copied = copy_to_user(user_addr, page_buffer, read);

				remain_length -= read;
				addr += read;
				user_addr += read;
				read_length += read;
				size -= read;
			}
		}
	}

out:
	kfree(page_buffer);

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_dev_mtd_read_page_user);

int rawfs_dev_mtd_write_page(struct super_block *sb, int block_no, int page_no,
	void *buffer)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct mtd_info *mtd = RAWFS_MTD(sb);
	loff_t addr = ((loff_t) rawfs_page_addr(sb, block_no, page_no));
	size_t dummy;
	int retval = 0;

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_dev_mtd_write_page: block %d, page %d\n", block_no, page_no);

	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb) &&
		page_no < RAWFS_NAND_PAGES(rawfs_sb)) {

		retval = mtd_write(mtd, addr, RAWFS_NAND_PAGE_SIZE(rawfs_sb), &dummy,
			buffer);

		if (retval) {
			RAWFS_PRINT(RAWFS_DBG_DEVICE,
				"rawfs_dev_mtd_write_page: block %d, page %d, mtd error %d\n",
				block_no, page_no, retval);
			dump_stack();
		} else { /* write succeed, read-back verify */
			int read_retval;
			void *read_buffer;

			read_buffer = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);
			if (read_buffer) {
				read_retval = mtd_read(mtd, addr,
					RAWFS_NAND_PAGE_SIZE(rawfs_sb), &dummy, read_buffer);
				if (read_retval) {  /* error case 1: MTD read error */
					RAWFS_PRINT(RAWFS_DBG_DEVICE,
						"rawfs_dev_mtd_write_page: read-verify failed, mtd error %d\n",
						read_retval);
					retval = read_retval;
				} else {
					/* error case 2: read-verify failed */
					if (memcmp(buffer, read_buffer,
						RAWFS_NAND_PAGE_SIZE(rawfs_sb))) {
						RAWFS_PRINT(RAWFS_DBG_DEVICE,
							"rawfs_dev_mtd_write_page: read-verify mismatch\n");
						retval = -EIO;
					} else {
						RAWFS_PRINT(RAWFS_DBG_DEVICE,
							"rawfs_dev_mtd_write_page: done\n");
					}
				}
				kfree(read_buffer);
			} else {
				RAWFS_PRINT(RAWFS_DBG_DEVICE,
					"rawfs_dev_mtd_write_page: read-verify abort, out of memory\n");
			}
		}
	}
	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_dev_mtd_write_page);

int rawfs_dev_mtd_read_page(struct super_block *sb, int block_no, int page_no,
	void *buffer)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct mtd_info *mtd = RAWFS_MTD(sb);
	loff_t addr = ((loff_t) rawfs_page_addr(sb, block_no, page_no));
	size_t dummy;
	int retval = 0;

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_dev_mtd_read_page: block %d, page %d\n", block_no, page_no);
	if (block_no < RAWFS_NAND_BLOCKS(rawfs_sb) &&
		page_no < RAWFS_NAND_PAGES(rawfs_sb)) {
		retval = mtd_read(mtd, addr, RAWFS_NAND_PAGE_SIZE(rawfs_sb),
			&dummy, buffer);
		if (retval) {
			RAWFS_PRINT(RAWFS_DBG_DEVICE,
				"rawfs_dev_mtd_read_page: block %d, page %d, read fail, mtd error %d\n",
				block_no, page_no,
				retval);
			dump_stack();
		}
	}
	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_dev_mtd_read_page);

static int rawfs_block_header_read(struct super_block *sb, int block_no,
	struct rawfs_block_header *block_head_out)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_block_header *block_head = NULL;
	int result = 0;

	block_head = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);

	if (!block_head) {
		result = -ENOMEM;
		goto end;
	}

	result = rawfs_sb->dev.read_page(sb, block_no, 0, block_head);

	if (result) {
		RAWFS_PRINT(RAWFS_DBG_DEVICE,
			"rawfs_read_block_header: block %d header, read error %d\n",
			block_no, result);
		result = -EIO;
		goto end;
	}

	if (block_head_out)
		memcpy(block_head_out, block_head, sizeof(struct rawfs_block_header));

end:

	kfree(block_head);

	return result;
}

/*---------------------------------------------------------------------------*/
/* Layer 2: Block/Page level application */
/*---------------------------------------------------------------------------*/
/* This function updates following two statistics which are used in
	block level analysis:
		rawfs_sb->sequence_number
		rawfs_sb->erase_count_max */
int rawfs_block_is_valid(struct super_block *sb, int block_no,
	struct rawfs_block_header *block_head_out,
	struct rawfs_gc_marker_page *gc_page_out)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_block_header block_head;
	struct rawfs_gc_marker_page *gc_page = NULL;
	__u32 crc;
	bool all_empty = true;
	int i;
	int result = 0;
	int retval = 0;
	void *page_buffer = NULL;

	result = rawfs_block_header_read(sb, block_no, &block_head);

	if (result) {
		if (result == -EIO)
			result = RAWFS_BLOCK_STAT_INVALID_HEAD;
		goto end;
	}

	page_buffer = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);

	if (!page_buffer) {
		result = -ENOMEM;
		goto end;
	}

	if (block_head.i_signature_head != RAWFS_NAND_BLOCK_SIG_HEAD) {
		RAWFS_PRINT(RAWFS_DBG_MOUNT, "block %d is invalid\n", block_no);
		result = RAWFS_BLOCK_STAT_INVALID_HEAD;
		goto end;
	}

	crc = crc32(0, &block_head,
			sizeof(struct rawfs_block_header) - sizeof(__u32));

	if (block_head.i_crc != crc) {
		RAWFS_PRINT(RAWFS_DBG_MOUNT, "block %d is invalid, header crc fail\n",
			block_no);
		result = RAWFS_BLOCK_STAT_INVALID_HEAD;
		goto end;
	}

	RAWFS_PRINT(RAWFS_DBG_MOUNT,
		"rawfs_is_valid_block: block %d, seq.no %d, last seq.no=%d, ec=%d\n",
		block_no,
		block_head.i_sequence_number,
		block_head.i_sequence_number_last,
		block_head.i_erase_count);

	/* Copy block head into output */
	if (block_head_out)
		memcpy(block_head_out, &block_head, sizeof(struct rawfs_block_header));

	/* Search for GC block */
	for (i = 1; i < RAWFS_NAND_PAGES(rawfs_sb); i++) {
		retval = rawfs_sb->dev.read_page(sb, block_no, i, page_buffer);
		if (retval) {
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"rawfs_is_valid_block: block %d, page %d, read error %d\n", block_no, i, retval);
			all_empty = false;
		}
		gc_page = (struct rawfs_gc_marker_page *)page_buffer;

		if (gc_page->i_signature_head != RAWFS_NAND_PAGE_SIG_EMPTY)
			all_empty = false;

		if (gc_page->i_signature_head != RAWFS_NAND_GC_MARKER_SIG_HEAD)
			continue;

		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"rawfs_is_valid_block: block %d is a data block, GC complete marker found @ page %d\n",
			block_no, i);

		crc = rawfs_page_crc_gcmarker(sb, gc_page);

		if (crc != gc_page->i_crc) {
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"rawfs_is_valid_block: page %d, GC maker, crc error, expected %X, calucated %X\n",
				i, gc_page->i_crc, crc);
			result = RAWFS_PAGE_STAT_INVALID;
		} else {
			if (gc_page_out != NULL)
				memcpy(gc_page_out, gc_page,
					sizeof(struct rawfs_gc_marker_page));
		}

		rawfs_sb->sequence_number = max(rawfs_sb->sequence_number,
			block_head.i_sequence_number);
		rawfs_sb->erase_count_max = max3(rawfs_sb->erase_count_max,
			block_head.i_erase_count, gc_page->i_src_block_erase_count);
		result = RAWFS_BLOCK_STAT_DATA;

		goto end;
	}

	if (all_empty)	{
		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"rawfs_is_valid_block: block %d is a empty block\n", block_no);
		result = RAWFS_BLOCK_STAT_EMPTY;
	} else {
		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"rawfs_is_valid_block: block %d is an invalid data block without GC complete marker\n",
			block_no);
		rawfs_sb->sequence_number = max(rawfs_sb->sequence_number,
			block_head.i_sequence_number);
		result = RAWFS_BLOCK_STAT_INVALID_DATA;
	}

end:

	kfree(page_buffer);
	return result;
}
EXPORT_SYMBOL_GPL(rawfs_block_is_valid);

__u32 rawfs_page_crc_data(struct super_block *sb, void *data_page)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_page *ptr;
	__u32 retval;

	ptr = (struct rawfs_page *) data_page;

	retval = crc32(0, &ptr->i_info.i_file_info, sizeof(struct rawfs_file_info));
	retval = crc32(retval, &ptr->i_data[0], rawfs_sb->page_data_size);

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_page_crc_data);

__u32 rawfs_page_crc_gcmarker(struct super_block *sb, void *gcmarker_page)
{
	 __u32 retval;

	retval = crc32(0, gcmarker_page,
		sizeof(struct rawfs_gc_marker_page) - sizeof(__u32));

	return retval;
}
EXPORT_SYMBOL_GPL(rawfs_page_crc_gcmarker);

int rawfs_page_get(struct super_block *sb, int block_no, int page_no,
	struct rawfs_file_info *file_info, void *data)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_page *page;
	int	result = RAWFS_PAGE_STAT_EMPTY;
	int	retval;
	unsigned signature_foot;
	void *page_buffer;
	__u32   crc;

	page_buffer = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);

	if (!page_buffer) {
		result = -ENOMEM;
		goto out;
	}

	retval = rawfs_sb->dev.read_page(sb, block_no, page_no, page_buffer);

	if (retval) {
		RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d, read error %d\n", page_no,
			retval);
		result = RAWFS_PAGE_STAT_UNCORRECTABLE;
		goto out;
	}

	page = (struct rawfs_page *)page_buffer;
	signature_foot = RAWFS_NAND_PAGE_FOOTER(rawfs_sb, page);

	if ((page->i_signature_head == RAWFS_NAND_PAGE_SIG_EMPTY) &&
		(signature_foot == RAWFS_NAND_PAGE_SIG_EMPTY)) {
		goto out;
	} else if (page->i_signature_head == RAWFS_NAND_GC_MARKER_SIG_HEAD) {
		/* Verify GC page CRC */
		struct rawfs_gc_marker_page *gcmarker;

		gcmarker = (struct rawfs_gc_marker_page *)page_buffer;

		crc = rawfs_page_crc_gcmarker(sb, page);
		if (crc != gcmarker->i_crc) {
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"page %d, GC maker, crc error, expected %X, calucated %X\n",
				page_no, gcmarker->i_crc, crc);
			result = RAWFS_PAGE_STAT_INVALID;
		} else {
			result = RAWFS_PAGE_STAT_GC_MARKER;
		}
		goto out;
	} else if (page->i_signature_head == RAWFS_NAND_BLOCK_SIG_HEAD) {
		result = RAWFS_PAGE_STAT_BLOCK_HEAD;
		goto out;
	} else if ((page->i_signature_head != RAWFS_NAND_PAGE_SIG_HEAD) ||
			 (signature_foot != RAWFS_NAND_PAGE_SIG_FOOT)) {
		result = RAWFS_PAGE_STAT_INVALID;  /* Invalid Page: 0 */
		goto out;
	}

	/* Verify Data Page CRC */
	crc = rawfs_page_crc_data(sb, page);

	if (crc != page->i_crc) {
		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"page %d, crc error, expected %X, calucated %X\n",
			page_no, page->i_crc, crc);
		result = RAWFS_PAGE_STAT_INVALID;
		goto out;
	}

	/* Copy Page Data */
	if (data != NULL)
		memcpy(data, page->i_data, RAWFS_NAND_PAGE_DATA_SIZE(rawfs_sb));

	if (file_info != NULL)
		memcpy(file_info, &page->i_info.i_file_info,
			sizeof(struct rawfs_file_info));

	if (page->i_info.i_file_info.i_chunk_total == -1)
		result = RAWFS_PAGE_STAT_DELETED;
	else if (page->i_info.i_file_info.i_chunk_total >=
			 page->i_info.i_file_info.i_chunk_index)
		result = RAWFS_PAGE_STAT_VALID;

	/* Free Page */
out:
	kfree(page_buffer);
	return result;
}
EXPORT_SYMBOL_GPL(rawfs_page_get);

/* Program Block Header */
static void rawfs_block_write_header(struct super_block *sb, int block_no,
	unsigned sequence_number, unsigned sequence_number_last,
	unsigned erase_count)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_block_header *bheader;

	bheader = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);
	bheader->i_rawfs_version = RAWFS_VERSION;
	bheader->i_signature_head = RAWFS_NAND_BLOCK_SIG_HEAD;
	bheader->i_erase_count = erase_count;
	bheader->i_sequence_number = sequence_number;
	bheader->i_sequence_number_last = sequence_number_last;
	bheader->i_crc = crc32(0, bheader,
		sizeof(struct rawfs_block_header) - sizeof(__u32));

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_block_write_header: block %d, seq.no %d, last_seq.no %d, ec: %d\n",
		block_no, sequence_number, sequence_number_last, erase_count);

	rawfs_sb->erase_count_max = max(rawfs_sb->erase_count_max, erase_count);

	rawfs_sb->dev.write_page(sb, block_no, 0, bheader);
	kfree(bheader);
}

static void rawfs_page_write_gc_marker(struct super_block *sb, int block_no,
	int page_no, unsigned src_block_index, unsigned src_block_seq,
	unsigned src_erase_count)
{
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_gc_marker_page *gc_marker;

	gc_marker = kzalloc(RAWFS_NAND_PAGE_SIZE(rawfs_sb), GFP_NOFS);
	gc_marker->i_signature_head = RAWFS_NAND_GC_MARKER_SIG_HEAD;
	gc_marker->i_src_block_index = src_block_index;
	gc_marker->i_src_block_sequence_number = src_block_seq;
	gc_marker->i_src_block_erase_count = src_erase_count;
	gc_marker->i_crc = crc32(0, gc_marker,
		sizeof(struct rawfs_gc_marker_page) - sizeof(__u32));

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_write_gc_marker_page: block %d, page %d, src_blk %d, src_seq.no: %d, src_blk_ec %d\n",
		block_no, page_no, src_block_index, src_block_seq, src_erase_count);

	rawfs_sb->dev.write_page(sb, block_no, page_no, gc_marker);

	rawfs_sb->data_block_gcmarker_page_index = page_no;

	kfree(gc_marker);
}


/* Set page head/foot signature, and crc */
void rawfs_page_signature(struct super_block *sb, void *buf)
{
	struct rawfs_page *page_buf = (struct rawfs_page *) buf;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);

	page_buf->i_signature_head = RAWFS_NAND_PAGE_SIG_HEAD;
	RAWFS_NAND_PAGE_FOOTER(rawfs_sb, buf) = RAWFS_NAND_PAGE_SIG_FOOT;

	page_buf->i_crc = rawfs_page_crc_data(sb, page_buf);

	RAWFS_PRINT(RAWFS_DBG_DEVICE,
		"rawfs_page_signature: %s @ %X (%d/%d), crc %X\n",
		page_buf->i_info.i_file_info.i_name,
		page_buf->i_info.i_file_info.i_parent_folder_id,
		page_buf->i_info.i_file_info.i_chunk_index,
		page_buf->i_info.i_file_info.i_chunk_total,
		page_buf->i_crc);
}
EXPORT_SYMBOL_GPL(rawfs_page_signature);

/* Mount: Block Level Analysis */
/* Assign data block & empty block in sb */
int rawfs_block_level_analysis(struct super_block *sb)
{
	/* For each block, Check its block header */
	int i;
	int result = 0;
	int *block_list = NULL;
	int block_list_entries = 0;
	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);
	struct rawfs_block_header block_header;
	struct rawfs_gc_marker_page gc_page;
	int data_block_seqno = 0;
	int gc_src_block_index =  -1;
	int gc_src_block_seqno =  -1;

	RAWFS_PRINT(RAWFS_DBG_MOUNT, "rawfs_block_level_analysis\n");

	rawfs_sb->data_block  = -1;
	rawfs_sb->empty_block = -1;
	rawfs_sb->sequence_number = 0;
	rawfs_sb->erase_count_max = 0;
	rawfs_sb->data_block_gcmarker_page_index = -1;

	block_list = kzalloc((1+RAWFS_NAND_BLOCKS(rawfs_sb))*sizeof(int), GFP_NOFS);

	if (!block_list) {
		result = -ENOMEM;
		goto out;
	}

	memset(&block_header, 0, sizeof(struct rawfs_block_header));

	for (i = 0; i < RAWFS_NAND_BLOCKS(rawfs_sb); i++)	{
		int block_stat;

		block_stat = rawfs_block_is_valid(sb, i, &block_header, &gc_page);

		/* erase all blocks if firstboot flag was set in protect.rc */
		if (rawfs_sb->flags & RAWFS_MNT_FIRSTBOOT)
			block_stat = RAWFS_BLOCK_STAT_INVALID_HEAD;

		switch (block_stat)	{
		case RAWFS_BLOCK_STAT_INVALID_HEAD:  /* Invalid Block Head */
			block_list[block_list_entries] = i;
			block_list_entries++;
			/* Add to list, the block header will program latter,
			   its header will be recovered from data block	   */
			break;
		case RAWFS_BLOCK_STAT_INVALID_DATA:
			/* Invalid Block Data, Keep orignal header */
			block_list[block_list_entries] = i;
			block_list_entries++;
			break;
		case RAWFS_BLOCK_STAT_EMPTY:   /* Empty Block */
			rawfs_sb->empty_block = i;
			break;
		case RAWFS_BLOCK_STAT_DATA:
			/* Data Block, with largest sequence number */
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"rawfs_block_level_analysis: data block %d, seq.no %d, last seq.no=%d, ec=%d\n",
				i,
				block_header.i_sequence_number,
				block_header.i_sequence_number_last,
				block_header.i_erase_count);

			if (block_header.i_sequence_number >= data_block_seqno) {
				rawfs_sb->data_block = i;
				data_block_seqno = block_header.i_sequence_number;
				gc_src_block_index = gc_page.i_src_block_index;
				gc_src_block_seqno = gc_page.i_src_block_sequence_number;
			}
			break;
		case -ENOMEM:
			result = -ENOMEM;
			goto out;
		default:
			break;
		}
	}

	RAWFS_PRINT(RAWFS_DBG_MOUNT,
		"rawfs_block_level_analysis: [Handling] data block %d, empty block %d\n",
		rawfs_sb->data_block, rawfs_sb->empty_block);

	/* Case A: There's no data block, this is First Boot */
	if (rawfs_sb->data_block < 0) {
		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"rawfs_block_level_analysis: A.1/A.2: Data block not exist: First Boot\n");
		rawfs_sb->data_block  = 0;
		rawfs_sb->empty_block = 1;
		for (i = 0; i < RAWFS_NAND_BLOCKS(rawfs_sb); i++) {
			rawfs_sb->dev.erase_block(sb, i);
			rawfs_block_write_header(sb, i, i+1, 0, 0);
		}
		rawfs_sb->sequence_number = i;
		/* gc_block=1, last_seq=-1, for first boot */
		rawfs_page_write_gc_marker(sb, 0, 1, 1, -1, 0);

		goto out;
	} else {
		if (rawfs_sb->empty_block < 0) { /* Case B: Empty block was not exist */
			/* case B.1: Both data blocks are valid, but there's no empty block
						 => erase the one that last gc marker indicates. */
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"rawfs_block_level_analysis: B.1 Empty block not exist, gc_block_index %d\n",
				gc_src_block_index);
			BUG_ON(gc_src_block_index < 0);  /* This should not happen,
				since data blocks already had been found. */

			/* add to list */
			for (i = 0; i < block_list_entries; i++) {
				if (block_list[i] == gc_src_block_index)
					break;
			}
			if (i == block_list_entries) {
				block_list[block_list_entries] = gc_src_block_index;
				block_list_entries++;
			}
			rawfs_sb->empty_block = gc_src_block_index;
			BUG_ON(rawfs_sb->empty_block == rawfs_sb->data_block);
		} else {
			/* Case B.2:  Both data block and empty block are valid
						  => Normal Case */
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"rawfs_block_level_analysis: B.2 Normal Boot\n");
		}
	}

	/* Handle blocks listed in erase list. */
	for (i = 0; i < block_list_entries; i++)	{
		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"rawfs_block_level_analysis: erase & restore block %d header, seq %d, ec %d\n",
			block_list[i], rawfs_sb->sequence_number,
			rawfs_sb->erase_count_max);
		rawfs_sb->dev.erase_block(sb, block_list[i]);
		rawfs_block_write_header(sb, block_list[i], rawfs_sb->sequence_number,
			0, rawfs_sb->erase_count_max);
		if (rawfs_sb->empty_block < 0)
			rawfs_sb->empty_block = block_list[i];
		rawfs_sb->sequence_number++;
	}

	RAWFS_PRINT(RAWFS_DBG_MOUNT,
		"rawfs_block_level_analysis: [Result] data block %d, empty block %d\n",
		rawfs_sb->data_block, rawfs_sb->empty_block);

	BUG_ON(rawfs_sb->empty_block < 0);
	BUG_ON(rawfs_sb->data_block < 0);

out:
	kfree(block_list);

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_block_level_analysis);

void rawfs_file_list_init(struct super_block *sb)
{
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);

	INIT_LIST_HEAD(&sbi->file_list);
	INIT_LIST_HEAD(&sbi->folder_list);
}
EXPORT_SYMBOL_GPL(rawfs_file_list_init);

void rawfs_file_list_destroy(struct super_block *sb)
{
	struct rawfs_sb_info   *sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr, *tmp;
	struct list_head *lists[2];
	int i;

	lists[0] =  &sbi->folder_list;
	lists[1] =  &sbi->file_list;

	RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_file_list_destroy()\n");

	mutex_lock(&sbi->file_list_lock);

	for (i = 0; i < 2; i++)	{
		list_for_each_entry_safe(ptr, tmp, lists[i], list) {
			RAWFS_PRINT(RAWFS_DBG_DIR,
				"rawfs_file_list_destroy: free %s @ folder %0X", ptr->file_info.i_name,
				ptr->file_info.i_parent_folder_id);
			list_del(&ptr->list);
			kfree(ptr);
		}
	}
	mutex_unlock(&sbi->file_list_lock);
}
EXPORT_SYMBOL_GPL(rawfs_file_list_destroy);

int rawfs_file_list_count(struct super_block *sb, unsigned int *entry_count,
	unsigned int *used_blocks, unsigned int *free_blocks)
{
	struct rawfs_sb_info	*sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr;
	struct list_head *lists[2];
	struct rawfs_file_info *fi;
	unsigned int entries = 0;
	unsigned int ublocks = 0;
	int fblocks;
	int i;

	lists[0] =  &sbi->folder_list; /* Folders will be listed before files */
	lists[1] =  &sbi->file_list;

	mutex_lock(&sbi->file_list_lock);

	for (i = 0; i < 2; i++)	{
		list_for_each_entry(ptr, lists[i], list) {
			if (ptr->i_location_page < 0)  /* skip block file */
				continue;
			fi = &ptr->file_info;
			RAWFS_PRINT(RAWFS_DBG_DIR,
				"rawfs_file_list_count() %s %s @ folder %X, size %d, pages %d\n",
				S_ISDIR(fi->i_mode)?"folder":"file",
				fi->i_name, fi->i_parent_folder_id, (unsigned)fi->i_size,
				ptr->i_location_page_count);

			entries++;
			ublocks += ptr->i_location_page_count;
		}
	}
	mutex_unlock(&sbi->file_list_lock);

	fblocks = (sbi->pages_per_block - 3 - ublocks);

	if (entry_count)
		*entry_count = entries;

	if (used_blocks)
		*used_blocks = ublocks;

	if (free_blocks)
		*free_blocks = (fblocks < 0) ? 0 : fblocks;

	RAWFS_PRINT(RAWFS_DBG_DIR,
		"rawfs_file_list_count() entries %d, used %d, free %d\n", entries, ublocks, fblocks);

	return 0;
}
EXPORT_SYMBOL_GPL(rawfs_file_list_count);

struct rawfs_file_list_entry *rawfs_file_list_get(struct super_block *sb,
	const char *name, int folder_id)
{
	/* Try to search in file list */
	struct rawfs_sb_info	*sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr;
	struct rawfs_file_list_entry *entry = NULL;
	struct list_head *lists[2];
	int i;

	lists[0] =  &sbi->folder_list; /* Folders will be listed before files */
	lists[1] =  &sbi->file_list;

	mutex_lock(&sbi->file_list_lock);

	for (i = 0; i < 2; i++) {   /* Check files on the same folder */
		list_for_each_entry(ptr, lists[i], list) {
			if (ptr->file_info.i_parent_folder_id != folder_id)
				continue;
			if (strnicmp(ptr->file_info.i_name, name,
				RAWFS_MAX_FILENAME_LEN+4) == 0) {
				entry = ptr;
				break;
			}
		}
	}
	mutex_unlock(&sbi->file_list_lock);

	if (entry)
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_file_list_get() %s, found @ %0lx",
			name, (unsigned long)entry);
	else
		RAWFS_PRINT(RAWFS_DBG_DIR, "rawfs_file_list_get() %s, not found", name);

	return entry;
}
EXPORT_SYMBOL_GPL(rawfs_file_list_get);

struct rawfs_file_list_entry *rawfs_file_list_get_by_id(struct super_block *sb,
	umode_t mode, int id)
{
	struct rawfs_sb_info	*sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr;
	struct rawfs_file_list_entry *entry = NULL;
	struct list_head *list_p;

	/* Determine entry type */
	if S_ISDIR(mode)
		list_p = &sbi->folder_list;
	else
		list_p = &sbi->file_list;

	mutex_lock(&sbi->file_list_lock);

	list_for_each_entry(ptr, &sbi->folder_list, list) {
		if (ptr->file_info.i_id == id) {
			entry = ptr;
			break;
		}
	}

	mutex_unlock(&sbi->file_list_lock);

	return entry;

}
EXPORT_SYMBOL_GPL(rawfs_file_list_get_by_id);

void rawfs_file_list_remove(struct super_block *sb, struct rawfs_file_info *fi)
{
	struct rawfs_sb_info   *sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr, *tmp;
	struct rawfs_file_list_entry *entry = NULL;
	struct list_head *list_p;

	/* Determine entry type */
	if S_ISDIR(fi->i_mode)
		list_p = &sbi->folder_list;
	else
		list_p = &sbi->file_list;

	RAWFS_PRINT(RAWFS_DBG_DIR,
		"rawfs_file_list_remove() %s %s from folder %X\n",
		S_ISDIR(fi->i_mode)?"folder":"file",
		fi->i_name, fi->i_parent_folder_id);

	mutex_lock(&sbi->file_list_lock);

	list_for_each_entry_safe(ptr, tmp, list_p, list) {
		/* Check files on the same folder */
		if (ptr->file_info.i_parent_folder_id != fi->i_parent_folder_id)
			continue;
		if (strnicmp(ptr->file_info.i_name, fi->i_name,
			RAWFS_MAX_FILENAME_LEN+4) == 0) {
			entry = ptr;
			break;
		}
	}

	if (entry) {
		list_del(&entry->list);
		kfree(entry);
	}

	mutex_unlock(&sbi->file_list_lock);
}
EXPORT_SYMBOL_GPL(rawfs_file_list_remove);

int rawfs_file_list_add(struct super_block *sb, struct rawfs_file_info *fi,
	int block_no, int page_no)
{
	int result = 0;
	struct rawfs_sb_info   *sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *ptr;
	struct rawfs_file_list_entry *entry = NULL;
	struct list_head *list_p;

	/* Determine entry type */
	if S_ISDIR(fi->i_mode)
		list_p = &sbi->folder_list;
	else
		list_p = &sbi->file_list;

	RAWFS_PRINT(RAWFS_DBG_DIR,
		"rawfs_file_list_add() add %s %s to folder %X @ block %d, page %d\n",
		S_ISDIR(fi->i_mode)?"folder":"file", fi->i_name,
		fi->i_parent_folder_id, block_no, page_no);

	mutex_lock(&sbi->file_list_lock);

	list_for_each_entry(ptr, list_p, list) {
		if (ptr->file_info.i_parent_folder_id != fi->i_parent_folder_id)
			continue;
		if ((strnicmp(ptr->file_info.i_name, fi->i_name,
			 RAWFS_MAX_FILENAME_LEN+4) == 0) ||
			 (ptr->file_info.i_id == fi->i_id)) {
			entry = ptr;
			break;
		}
	}

	if (!entry) { /* Entry not exist -> allocate new entry */
		entry = kzalloc(sizeof(struct rawfs_file_list_entry), GFP_NOFS);

		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_file_list_add() allocate new entry @ %lx\n", (unsigned long)entry);

		if (!entry) {
			result = -ENOMEM;
			goto out;
		}
		INIT_LIST_HEAD(&(entry->list));
		list_add_tail(&(entry->list), list_p);
	} else {
		RAWFS_PRINT(RAWFS_DBG_DIR,
			"rawfs_file_list_add() update existing entry @ %lx\n", (unsigned long)entry);
	}

	memcpy(&(entry->file_info), fi, sizeof(struct rawfs_file_info));
	entry->i_location_block = block_no;
	entry->i_location_page = page_no;
	entry->i_location_page_count = S_ISDIR(fi->i_mode) ? 1 :
		CEILING((unsigned)fi->i_size, sbi->page_data_size);

out:
	mutex_unlock(&sbi->file_list_lock);

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_file_list_add);

/* Inconsistent: 1, Consistent: 0 */
static int rawfs_page_check_file_consistency(struct rawfs_file_info *fi_head,
	struct rawfs_file_info *fi_ptr, int index)
{
	/* Except CRC & index, all info should indentical to that head. */
	int result;

	if (index != fi_ptr->i_chunk_index)
		return 1;

	fi_head->i_chunk_index = fi_ptr->i_chunk_index;

	if (memcmp(fi_head, fi_ptr, sizeof(struct rawfs_file_info)) == 0)
		result = 0;
	else
		result = 1;

	fi_head->i_chunk_index = 1;

	return result;
}


/***** Mount: Page Level Analysis (On data block)
   Build list of files
   Last free page in data block */
int rawfs_page_level_analysis(struct super_block *sb)
{
	int i;
	int result = 0;
	int page_status;
	int free_page_index = -1;
	int head_pos = -1;
	int uncorrectable_pages = 0;  /* uncorrectable pages among file chunks. */
	struct rawfs_sb_info *rawfs_sb;
	struct rawfs_file_info *fi_ptr;   /* Info of Current reading page. */
	struct rawfs_file_info *fi_head;  /* Info Current processing file. */

	rawfs_sb = RAWFS_SB(sb);
	RAWFS_PRINT(RAWFS_DBG_MOUNT, "page level analysis @ block %d\n",
		rawfs_sb->data_block);

	fi_ptr  = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);
	fi_head = kzalloc(sizeof(struct rawfs_file_info), GFP_NOFS);

	if ((!fi_head) || (!fi_ptr))	{
		result = -ENOMEM;
		goto out;
	}

	/* Search pages from data block begin (by order of update sequence). */
	for (i = 1; i < RAWFS_NAND_PAGES(rawfs_sb); i++)	{
		page_status = rawfs_page_get(sb, rawfs_sb->data_block, i, fi_ptr, NULL);

		switch (page_status)		{
		case RAWFS_PAGE_STAT_VALID:
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: Valid\n", i);
			/* Check file integrity */
			/* Search & delete redundant entries in list. (by name) */
			/* Insert new entry to list */
			free_page_index = -1;

			if (fi_ptr->i_chunk_index == 1) {   /* First chunk of file */
				memcpy(fi_head, fi_ptr, sizeof(struct rawfs_file_info));
				head_pos = i;
				uncorrectable_pages = 0;
			} else { /* Check current chunk is consistent to the head */
				/* No head chunk were found preceding this middle chunk,
				   it is invalid. */
				if (head_pos < 0)
					break;

				/* Expected index of current chunk is
					(i-head_pos+1+uncorrectable_pages) */
				if (rawfs_page_check_file_consistency(fi_head, fi_ptr,
					i-head_pos+1+uncorrectable_pages))	{
					head_pos = -1;
					break;
				}
			}

			/* Last chunk */
			if (fi_ptr->i_chunk_index == fi_ptr->i_chunk_total) {
				result = rawfs_file_list_add(sb, fi_head,
					rawfs_sb->data_block, head_pos);
				if (result)
					goto out;
			}
			break;
		case RAWFS_PAGE_STAT_DELETED:
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: Deleted\n", i);
			/* Search list for redundant entry (by name)
			   delete entry */
			free_page_index = -1;
			rawfs_file_list_remove(sb, fi_ptr);
			head_pos = -1;
			break;
		case RAWFS_PAGE_STAT_INVALID: /* Do nothing, skip it */
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: Invalid\n", i);
			free_page_index = -1;
			head_pos = -1;
			break;
		case RAWFS_PAGE_STAT_BLOCK_HEAD: /* Do nothing, skip it */
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: Block Head\n", i);
			free_page_index = -1;
			head_pos = -1;
			break;
		case RAWFS_PAGE_STAT_UNCORRECTABLE:
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: Uncorrectable\n", i);
			/* Ignore this page, and continue search next valid chunk. */
			uncorrectable_pages++;
			/* We shall skip uncorrectable pages */
			free_page_index = -1;
			break;
		case RAWFS_PAGE_STAT_GC_MARKER:
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: GC complete mark\n", i);
			head_pos = -1;
			free_page_index = -1;
			rawfs_sb->data_block_gcmarker_page_index = i;
			break;
		case RAWFS_PAGE_STAT_EMPTY:
			RAWFS_PRINT(RAWFS_DBG_MOUNT, "page %d: Empty\n", i);
			/* Set last free page,
			   Verify all pages are free, till end. */
			if (free_page_index < 0)
				free_page_index = i;
			head_pos = -1;
			break;
		case -ENOMEM:
			result = -ENOMEM;
			goto out;
		default:
			result = page_status;
			goto out;
		}
	}

out:
	kfree(fi_ptr);
	kfree(fi_head);

	if (free_page_index < 0)
		rawfs_dev_garbage_collection(sb);
	else
		rawfs_sb->data_block_free_page_index = free_page_index;

	RAWFS_PRINT(RAWFS_DBG_MOUNT,
		"rawfs_page_level_analysis, gc_marker %d, free_index %d\n",
		rawfs_sb->data_block_gcmarker_page_index,
		rawfs_sb->data_block_free_page_index);

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_page_level_analysis);

/* Mount: File Level Analysis (On data block) */
/* Search for files whose parent folders are not exist */
int rawfs_file_level_analysis(struct super_block *sb)
{
	int result = 0;
	struct rawfs_sb_info   *sbi = RAWFS_SB(sb);
	struct rawfs_file_list_entry *file_ptr, *folder_ptr, *tmp;
	struct rawfs_file_list_entry *entry;
	struct list_head *list_p;

	list_p = &sbi->file_list;

	RAWFS_PRINT(RAWFS_DBG_MOUNT, "rawfs_file_level_analysis.\n");

	mutex_lock(&sbi->file_list_lock);

	list_for_each_entry_safe(file_ptr, tmp, list_p, list) {
		RAWFS_PRINT(RAWFS_DBG_MOUNT,
			"rawfs_file_level_analysis: %s(%X)@%X, %d\n",
			file_ptr->file_info.i_name,
			file_ptr->file_info.i_id,
			file_ptr->file_info.i_parent_folder_id,
			(unsigned)file_ptr->file_info.i_size);

		if (file_ptr->file_info.i_parent_folder_id == RAWFS_ROOT_DIR_ID)
			continue;

		entry = NULL;

		/* Search for the parent folder in folder list. */
		list_for_each_entry(folder_ptr, &sbi->folder_list, list) {
			if (folder_ptr->file_info.i_id ==
				file_ptr->file_info.i_parent_folder_id)	{
				entry = folder_ptr;
				break;
			}
		}

		/* Parent folder not exist => Remove orphan files from file list */
		if (entry == NULL)	{
			RAWFS_PRINT(RAWFS_DBG_MOUNT,
				"rawfs_file_level_analysis: %s(%X)@%X, parent folder not exist.\n",
				file_ptr->file_info.i_name,
				file_ptr->file_info.i_id,
				file_ptr->file_info.i_parent_folder_id);

			list_del(&file_ptr->list);
			kfree(file_ptr);
		}
	}

	mutex_unlock(&sbi->file_list_lock);

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_file_level_analysis);

int rawfs_dev_free_space(struct super_block *sb)
{
	int result;

	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);

	/* -2: Except block header page,
		last page is reserved for deletion & rename */
	result = rawfs_sb->pages_per_block -
		rawfs_sb->data_block_free_page_index - 2;
	result = result * rawfs_sb->page_data_size;

	return result;
}
EXPORT_SYMBOL_GPL(rawfs_dev_free_space);

/* For all files in the gc data block, copy all valid data to empty block */
int rawfs_dev_garbage_collection(struct super_block *sb)
{
	int result = 0;
	int i, j;
	int empty_block;
	int empty_block_free_page_index = 1;
	int data_block;
	struct rawfs_sb_info *sbi = RAWFS_SB(sb);
	struct rawfs_page *page_buf = NULL;
	struct rawfs_file_list_entry *ptr;
	struct list_head *lists[2];
	struct inode *inode;
	struct rawfs_inode_info *inode_info;
	struct rawfs_block_header block_header;

	RAWFS_PRINT(RAWFS_DBG_GC, "rawfs_dev_garbage_collection()\n");

	if (sbi->data_block_gcmarker_page_index+1 ==
		sbi->data_block_free_page_index)	{
		RAWFS_PRINT(RAWFS_DBG_GC,
			"rawfs_dev_garbage_collection: disk is full, gcmarker index %d, free index %d, pages per block %d\n",
			sbi->data_block_gcmarker_page_index,
			sbi->data_block_free_page_index,
			sbi->pages_per_block);
		result = -ENOSPC;
		goto out;
	}


	/* Clear the list */
	page_buf = kzalloc(sbi->page_size, GFP_NOFS);

	if (!page_buf) {
		result = -ENOMEM;
		goto out;
	}

	empty_block = sbi->empty_block;
	data_block = sbi->data_block;

	result = rawfs_block_header_read(sb, data_block, &block_header);

	if (result)
		goto out;

	/* Try to search in file list */
	lists[0] =  &sbi->folder_list; /* Folders will be listed before files */
	lists[1] =  &sbi->file_list;

	mutex_lock(&sbi->file_list_lock);

	for (i = 0; i < 2; i++)	{
		list_for_each_entry(ptr, lists[i], list) {
			int starting_page;
			/* skip block file */
			if (ptr->i_location_page < 0) {
				RAWFS_PRINT(RAWFS_DBG_GC,
					"rawfs_dev_garbage_collection: skip block file %s",
					ptr->file_info.i_name);
				continue;
			}
			if (ptr->i_location_block != data_block) {
				RAWFS_PRINT(RAWFS_DBG_GC,
					"rawfs_dev_garbage_collection: skip %s @ block %d",
					ptr->file_info.i_name,
					ptr->i_location_block);
				continue;
			}
			/* Copy Content */
			starting_page = empty_block_free_page_index;

			RAWFS_PRINT(RAWFS_DBG_GC,
				"rawfs_dev_garbage_collection: Moving %s %s @ folder %X, Block %d Page %d, Count %d\n",
				S_ISDIR(ptr->file_info.i_mode)?"folder":"file",
				ptr->file_info.i_name, ptr->file_info.i_parent_folder_id,
				ptr->i_location_block, ptr->i_location_page,
				ptr->i_location_page_count);

			for (j = 0; j < ptr->i_location_page_count; j++)	{
				RAWFS_PRINT(RAWFS_DBG_GC,
					"rawfs_dev_garbage_collection: Block %d Page %d > Block %d Page %d\n",
					data_block, ptr->i_location_page + j,
					empty_block, empty_block_free_page_index);

			   sbi->dev.read_page(sb, data_block, ptr->i_location_page + j,
					page_buf);
			   sbi->dev.write_page(sb, empty_block, empty_block_free_page_index,
					page_buf);
			   empty_block_free_page_index++;
			}

			/* Update file list */
			ptr->i_location_block = empty_block;
			ptr->i_location_page = starting_page;
			/* ptr->i_location_page_count = ptr->file_info.i_chunk_total; */

			/* Update inode info */
			inode = rawfs_iget(sb, ptr->file_info.i_name,
				ptr->file_info.i_parent_folder_id);
			if (inode) {
				/* TODO: get inode lock first */
				inode_info = RAWFS_I(inode);
				inode_info->i_location_block = empty_block;
				inode_info->i_location_page = starting_page;
				inode_info->i_location_page_count = ptr->i_location_page_count;

				RAWFS_PRINT(RAWFS_DBG_GC,
					"rawfs_dev_garbage_collection: Update inode info %s %s @ folder %X, Block %d Page %d, Count %d\n",
					S_ISDIR(ptr->file_info.i_mode)?"folder":"file",
					ptr->file_info.i_name, ptr->file_info.i_parent_folder_id,
					inode_info->i_location_block, inode_info->i_location_page,
					inode_info->i_location_page_count);

				iput(inode);
			}
		}
	}

	mutex_unlock(&sbi->file_list_lock);

	/* write GC complete marker page */
	rawfs_page_write_gc_marker(sb,
		empty_block,
		empty_block_free_page_index,
		data_block,
		block_header.i_sequence_number,
		block_header.i_erase_count);

	empty_block_free_page_index++;

	/* Erase data block */
	sbi->dev.erase_block(sb, data_block);

	/* Write block header to new data block */
	rawfs_block_write_header(sb, data_block,
		sbi->sequence_number,
		block_header.i_sequence_number, /* last seq no */
		block_header.i_erase_count+1);

	sbi->sequence_number++;

	sbi->data_block = empty_block;
	sbi->empty_block = data_block;
	sbi->data_block_free_page_index = empty_block_free_page_index;

	result = (RAWFS_NAND_PAGES(sbi) - empty_block_free_page_index) *
		RAWFS_NAND_PAGE_DATA_SIZE(sbi);

	RAWFS_PRINT(RAWFS_DBG_GC,
		"rawfs_dev_garbage_collection: empty_blk = %d, data_blk = %d, free index = %d, reclaimed: %d bytes\n",
		sbi->empty_block, sbi->data_block, sbi->data_block_free_page_index,
		result);

out:
	kfree(page_buf);
	return result;
}
EXPORT_SYMBOL_GPL(rawfs_dev_garbage_collection);

/* Reserve space for create, write, copy, and rename */
int rawfs_reserve_space(struct super_block *sb, int chunks)
{
	int result = 0;
	int required_size;

	struct rawfs_sb_info *rawfs_sb = RAWFS_SB(sb);

	required_size = chunks * rawfs_sb->page_data_size;

	if (required_size > rawfs_dev_free_space(sb))	{
		int reclaimed_size;

		reclaimed_size = rawfs_dev_garbage_collection(sb);
		if ((required_size > reclaimed_size) || (reclaimed_size < 0)) {
			RAWFS_PRINT(RAWFS_DBG_GC,
				"rawfs_reg_reserve_space: disk full, reclaimed %d, required %d\n",
				reclaimed_size, required_size);
			result = -ENOSPC;
		}
	}
	return result;

}
EXPORT_SYMBOL_GPL(rawfs_reserve_space);

MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_DESCRIPTION("RAW file system for NAND flash");
MODULE_LICENSE("GPL");

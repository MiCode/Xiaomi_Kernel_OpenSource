/*
 * fs/mpage.c
 *
 * Copyright (C) 2002, Linus Torvalds.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Contains functions related to preparing and submitting BIOs which contain
 * multiple pagecache pages.
 *
 * 15May2002	Andrew Morton
 *		Initial version
 * 27Jun2002	axboe@suse.de
 *		use bio_add_page() to build bio's just the right size
 */

/*
 *  Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : core.c                                                    */
/*  PURPOSE : sdFAT glue layer for supporting VFS                       */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/exportfs.h>
#include <linux/mount.h>
#include <linux/vfs.h>
#include <linux/parser.h>
#include <linux/uio.h>
#include <linux/writeback.h>
#include <linux/log2.h>
#include <linux/hash.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/swap.h> /* for mark_page_accessed() */
#include <asm/current.h>
#include <asm/unaligned.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#include <linux/aio.h>
#endif

#include "sdfat.h"

#ifdef CONFIG_SDFAT_ALIGNED_MPAGE_WRITE

/*************************************************************************
 * INNER FUNCTIONS FOR FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
static void __mpage_write_end_io(struct bio *bio, int err);

/*************************************************************************
 * FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
       /* EMPTY */
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0) */
static inline void bio_set_dev(struct bio *bio, struct block_device *bdev)
{
	bio->bi_bdev = bdev;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
static inline void __sdfat_clean_bdev_aliases(struct block_device *bdev, sector_t block)
{
	clean_bdev_aliases(bdev, block, 1);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) */
static inline void __sdfat_clean_bdev_aliases(struct block_device *bdev, sector_t block)
{
	unmap_underlying_metadata(bdev, block);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static inline void __sdfat_submit_bio_write2(int flags, struct bio *bio)
{
	bio_set_op_attrs(bio, REQ_OP_WRITE, flags);
	submit_bio(bio);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0) */
static inline void __sdfat_submit_bio_write2(int flags, struct bio *bio)
{
	submit_bio(WRITE | flags, bio);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
static inline int bio_get_nr_vecs(struct block_device *bdev)
{
	return BIO_MAX_PAGES;
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,1,0) */
	/* EMPTY */
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static inline sector_t __sdfat_bio_sector(struct bio *bio)
{
	return bio->bi_iter.bi_sector;
}

static inline void __sdfat_set_bio_sector(struct bio *bio, sector_t sector)
{
	bio->bi_iter.bi_sector = sector;
}

static inline unsigned int __sdfat_bio_size(struct bio *bio)
{
	return bio->bi_iter.bi_size;
}

static inline void __sdfat_set_bio_size(struct bio *bio, unsigned int size)
{
	bio->bi_iter.bi_size = size;
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) */
static inline sector_t __sdfat_bio_sector(struct bio *bio)
{
	return bio->bi_sector;
}

static inline void __sdfat_set_bio_sector(struct bio *bio, sector_t sector)
{
	bio->bi_sector = sector;
}

static inline unsigned int __sdfat_bio_size(struct bio *bio)
{
	return bio->bi_size;
}

static inline void __sdfat_set_bio_size(struct bio *bio, unsigned int size)
{
	bio->bi_size = size;
}
#endif

/*************************************************************************
 * MORE FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
static void  mpage_write_end_io(struct bio *bio)
{
	__mpage_write_end_io(bio, blk_status_to_errno(bio->bi_status));
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
static void  mpage_write_end_io(struct bio *bio)
{
	__mpage_write_end_io(bio, bio->bi_error);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4,3,0) */
static void mpage_write_end_io(struct bio *bio, int err)
{
	if (test_bit(BIO_UPTODATE, &bio->bi_flags))
		err = 0;
	__mpage_write_end_io(bio, err);
}
#endif

/* __check_dfr_on() and __dfr_writepage_end_io() functions
 * are copied from sdfat.c
 * Each function should be same perfectly
 */
static inline int __check_dfr_on(struct inode *inode, loff_t start, loff_t end, const char *fname)
{
#ifdef	CONFIG_SDFAT_DFR
	struct defrag_info *ino_dfr = &(SDFAT_I(inode)->dfr_info);

	if ((atomic_read(&ino_dfr->stat) == DFR_INO_STAT_REQ) &&
			fsapi_dfr_check_dfr_on(inode, start, end, 0, fname))
		return 1;
#endif
	return 0;
}

static inline int __dfr_writepage_end_io(struct page *page)
{
#ifdef	CONFIG_SDFAT_DFR
	struct defrag_info *ino_dfr = &(SDFAT_I(page->mapping->host)->dfr_info);

	if (atomic_read(&ino_dfr->stat) == DFR_INO_STAT_REQ)
		fsapi_dfr_writepage_endio(page);
#endif
	return 0;
}


static inline unsigned int __calc_size_to_align(struct super_block *sb)
{
	struct block_device *bdev = sb->s_bdev;
	struct gendisk *disk;
	struct request_queue *queue;
	struct queue_limits *limit;
	unsigned int max_sectors;
	unsigned int aligned = 0;

	disk = bdev->bd_disk;
	if (!disk)
		goto out;

	queue = disk->queue;
	if (!queue)
		goto out;

	limit = &queue->limits;
	max_sectors = limit->max_sectors;
	aligned = 1 << ilog2(max_sectors);

	if (aligned && (max_sectors & (aligned - 1)))
		aligned = 0;
out:
	return aligned;
}

struct mpage_data {
	struct bio *bio;
	sector_t last_block_in_bio;
	get_block_t *get_block;
	unsigned int use_writepage;
	unsigned int size_to_align;
};

/*
 * I/O completion handler for multipage BIOs.
 *
 * The mpage code never puts partial pages into a BIO (except for end-of-file).
 * If a page does not map to a contiguous run of blocks then it simply falls
 * back to block_read_full_page().
 *
 * Why is this?  If a page's completion depends on a number of different BIOs
 * which can complete in any order (or at the same time) then determining the
 * status of that page is hard.  See end_buffer_async_read() for the details.
 * There is no point in duplicating all that complexity.
 */
static void __mpage_write_end_io(struct bio *bio, int err)
{
	struct bio_vec *bvec = bio->bi_io_vec + bio->bi_vcnt - 1;

	ASSERT(bio_data_dir(bio) == WRITE); /* only write */

	do {
		struct page *page = bvec->bv_page;

		if (--bvec >= bio->bi_io_vec)
			prefetchw(&bvec->bv_page->flags);
		if (err) {
			SetPageError(page);
			if (page->mapping)
				mapping_set_error(page->mapping, err);
		}

		__dfr_writepage_end_io(page);

		end_page_writeback(page);
	} while (bvec >= bio->bi_io_vec);
	bio_put(bio);
}

static struct bio *mpage_bio_submit_write(int flags, struct bio *bio)
{
	bio->bi_end_io = mpage_write_end_io;
	__sdfat_submit_bio_write2(flags, bio);
	return NULL;
}

static struct bio *
mpage_alloc(struct block_device *bdev,
		sector_t first_sector, int nr_vecs,
		gfp_t gfp_flags)
{
	struct bio *bio;

	bio = bio_alloc(gfp_flags, nr_vecs);

	if (bio == NULL && (current->flags & PF_MEMALLOC)) {
		while (!bio && (nr_vecs /= 2))
			bio = bio_alloc(gfp_flags, nr_vecs);
	}

	if (bio) {
		bio_set_dev(bio, bdev);
		__sdfat_set_bio_sector(bio, first_sector);
	}
	return bio;
}

static int sdfat_mpage_writepage(struct page *page,
		struct writeback_control *wbc, void *data)
{
	struct mpage_data *mpd = data;
	struct bio *bio = mpd->bio;
	struct address_space *mapping = page->mapping;
	struct inode *inode = page->mapping->host;
	const unsigned int blkbits = inode->i_blkbits;
	const unsigned int blocks_per_page = PAGE_SIZE >> blkbits;
	sector_t last_block;
	sector_t block_in_file;
	sector_t blocks[MAX_BUF_PER_PAGE];
	unsigned int page_block;
	unsigned int first_unmapped = blocks_per_page;
	struct block_device *bdev = NULL;
	int boundary = 0;
	sector_t boundary_block = 0;
	struct block_device *boundary_bdev = NULL;
	int length;
	struct buffer_head map_bh;
	loff_t i_size = i_size_read(inode);
	unsigned long end_index = i_size >> PAGE_SHIFT;
	int ret = 0;

	if (page_has_buffers(page)) {
		struct buffer_head *head = page_buffers(page);
		struct buffer_head *bh = head;

		/* If they're all mapped and dirty, do it */
		page_block = 0;
		do {
			BUG_ON(buffer_locked(bh));
			if (!buffer_mapped(bh)) {
				/*
				 * unmapped dirty buffers are created by
				 * __set_page_dirty_buffers -> mmapped data
				 */
				if (buffer_dirty(bh))
					goto confused;
				if (first_unmapped == blocks_per_page)
					first_unmapped = page_block;
				continue;
			}

			if (first_unmapped != blocks_per_page)
				goto confused;	/* hole -> non-hole */

			if (!buffer_dirty(bh) || !buffer_uptodate(bh))
				goto confused;

			/* bh should be mapped if delay is set */
			if (buffer_delay(bh)) {
				sector_t blk_in_file =
					(sector_t)(page->index << (PAGE_SHIFT - blkbits)) + page_block;

				BUG_ON(bh->b_size != (1 << blkbits));
				if (page->index > end_index) {
					MMSG("%s(inode:%p) "
						"over end with delayed buffer"
						"(page_idx:%u, end_idx:%u)\n",
						__func__, inode,
						(u32)page->index,
						(u32)end_index);
					goto confused;
				}

				ret = mpd->get_block(inode, blk_in_file, bh, 1);
				if (ret) {
					MMSG("%s(inode:%p) "
						"failed to getblk(ret:%d)\n",
						__func__, inode, ret);
					goto confused;
				}

				BUG_ON(buffer_delay(bh));

				if (buffer_new(bh)) {
					clear_buffer_new(bh);
					__sdfat_clean_bdev_aliases(bh->b_bdev, bh->b_blocknr);
				}
			}

			if (page_block) {
				if (bh->b_blocknr != blocks[page_block-1] + 1) {
					MMSG("%s(inode:%p) pblk(%d) "
						"no_seq(prev:%lld, new:%lld)\n",
						__func__, inode, page_block,
						(u64)blocks[page_block-1],
						(u64)bh->b_blocknr);
					goto confused;
				}
			}
			blocks[page_block++] = bh->b_blocknr;
			boundary = buffer_boundary(bh);
			if (boundary) {
				boundary_block = bh->b_blocknr;
				boundary_bdev = bh->b_bdev;
			}
			bdev = bh->b_bdev;
		} while ((bh = bh->b_this_page) != head);

		if (first_unmapped)
			goto page_is_mapped;

		/*
		 * Page has buffers, but they are all unmapped. The page was
		 * created by pagein or read over a hole which was handled by
		 * block_read_full_page().  If this address_space is also
		 * using mpage_readpages then this can rarely happen.
		 */
		goto confused;
	}

	/*
	 * The page has no buffers: map it to disk
	 */
	BUG_ON(!PageUptodate(page));
	block_in_file = (sector_t)page->index << (PAGE_SHIFT - blkbits);
	last_block = (i_size - 1) >> blkbits;
	map_bh.b_page = page;
	for (page_block = 0; page_block < blocks_per_page; ) {

		map_bh.b_state = 0;
		map_bh.b_size = 1 << blkbits;
		if (mpd->get_block(inode, block_in_file, &map_bh, 1))
			goto confused;

		if (buffer_new(&map_bh))
			__sdfat_clean_bdev_aliases(map_bh.b_bdev, map_bh.b_blocknr);
		if (buffer_boundary(&map_bh)) {
			boundary_block = map_bh.b_blocknr;
			boundary_bdev = map_bh.b_bdev;
		}

		if (page_block) {
			if (map_bh.b_blocknr != blocks[page_block-1] + 1)
				goto confused;
		}
		blocks[page_block++] = map_bh.b_blocknr;
		boundary = buffer_boundary(&map_bh);
		bdev = map_bh.b_bdev;
		if (block_in_file == last_block)
			break;
		block_in_file++;
	}
	BUG_ON(page_block == 0);

	first_unmapped = page_block;

page_is_mapped:
	if (page->index >= end_index) {
		/*
		 * The page straddles i_size.  It must be zeroed out on each
		 * and every writepage invocation because it may be mmapped.
		 * "A file is mapped in multiples of the page size.  For a file
		 * that is not a multiple of the page size, the remaining memory
		 * is zeroed when mapped, and writes to that region are not
		 * written out to the file."
		 */
		unsigned int offset = i_size & (PAGE_SIZE - 1);

		if (page->index > end_index || !offset) {
			MMSG("%s(inode:%p) over end "
				"(page_idx:%u, end_idx:%u off:%u)\n",
				__func__, inode, (u32)page->index,
				(u32)end_index, (u32)offset);
			goto confused;
		}
		zero_user_segment(page, offset, PAGE_SIZE);
	}

	/*
	 * This page will go to BIO.  Do we need to send this BIO off first?
	 *
	 * REMARK : added ELSE_IF for ALIGNMENT_MPAGE_WRITE of SDFAT
	 */
	if (bio) {
		if (mpd->last_block_in_bio != blocks[0] - 1) {
			bio = mpage_bio_submit_write(0, bio);
		} else if (mpd->size_to_align) {
			unsigned int mask = mpd->size_to_align - 1;
			sector_t max_end_block =
				(__sdfat_bio_sector(bio) & ~(mask)) + mask;

			if ((__sdfat_bio_size(bio) != (1 << (mask + 1))) &&
				(mpd->last_block_in_bio == max_end_block)) {
				MMSG("%s(inode:%p) alignment mpage_bio_submit"
					"(start:%u, len:%u aligned:%u)\n",
					__func__, inode,
					(unsigned int)__sdfat_bio_sector(bio),
					(unsigned int)(mpd->last_block_in_bio -
						__sdfat_bio_sector(bio) + 1),
					(unsigned int)mpd->size_to_align);
				bio = mpage_bio_submit_write(REQ_NOMERGE, bio);
			}
		}
	}

alloc_new:
	if (!bio) {
		bio = mpage_alloc(bdev, blocks[0] << (blkbits - 9),
				bio_get_nr_vecs(bdev), GFP_NOFS|__GFP_HIGH);
		if (!bio)
			goto confused;
	}

	/*
	 * Must try to add the page before marking the buffer clean or
	 * the confused fail path above (OOM) will be very confused when
	 * it finds all bh marked clean (i.e. it will not write anything)
	 */
	length = first_unmapped << blkbits;
	if (bio_add_page(bio, page, length, 0) < length) {
		bio = mpage_bio_submit_write(0, bio);
		goto alloc_new;
	}

	/*
	 * OK, we have our BIO, so we can now mark the buffers clean.  Make
	 * sure to only clean buffers which we know we'll be writing.
	 */
	if (page_has_buffers(page)) {
		struct buffer_head *head = page_buffers(page);
		struct buffer_head *bh = head;
		unsigned int buffer_counter = 0;

		do {
			if (buffer_counter++ == first_unmapped)
				break;
			clear_buffer_dirty(bh);
			bh = bh->b_this_page;
		} while (bh != head);

		/*
		 * we cannot drop the bh if the page is not uptodate
		 * or a concurrent readpage would fail to serialize with the bh
		 * and it would read from disk before we reach the platter.
		 */
		if (buffer_heads_over_limit && PageUptodate(page))
			try_to_free_buffers(page);
	}

	BUG_ON(PageWriteback(page));
	set_page_writeback(page);

	/*
	 * FIXME FOR DEFRAGMENTATION : CODE REVIEW IS REQUIRED
	 *
	 * Turn off MAPPED flag in victim's bh if defrag on.
	 * Another write_begin can starts after get_block for defrag victims
	 * called.
	 * In this case, write_begin calls get_block and get original block
	 * number and previous defrag will be canceled.
	 */
	if (unlikely(__check_dfr_on(inode, (loff_t)(page->index << PAGE_SHIFT),
			(loff_t)((page->index + 1) << PAGE_SHIFT), __func__))) {
		struct buffer_head *head = page_buffers(page);
		struct buffer_head *bh = head;

		do {
			clear_buffer_mapped(bh);
			bh = bh->b_this_page;
		} while (bh != head);
	}

	unlock_page(page);
	if (boundary || (first_unmapped != blocks_per_page)) {
		bio = mpage_bio_submit_write(0, bio);
		if (boundary_block) {
			write_boundary_block(boundary_bdev,
					boundary_block, 1 << blkbits);
		}
	} else {
		mpd->last_block_in_bio = blocks[blocks_per_page - 1];
	}

	goto out;

confused:
	if (bio)
		bio = mpage_bio_submit_write(0, bio);

	if (mpd->use_writepage) {
		ret = mapping->a_ops->writepage(page, wbc);
	} else {
		ret = -EAGAIN;
		goto out;
	}
	/*
	 * The caller has a ref on the inode, so *mapping is stable
	 */
	mapping_set_error(mapping, ret);
out:
	mpd->bio = bio;
	return ret;
}

int sdfat_mpage_writepages(struct address_space *mapping,
			struct writeback_control *wbc, get_block_t *get_block)
{
	struct blk_plug plug;
	int ret;
	struct mpage_data mpd = {
		.bio = NULL,
		.last_block_in_bio = 0,
		.get_block = get_block,
		.use_writepage = 1,
		.size_to_align = __calc_size_to_align(mapping->host->i_sb),
	};

	BUG_ON(!get_block);
	blk_start_plug(&plug);
	ret = write_cache_pages(mapping, wbc, sdfat_mpage_writepage, &mpd);
	if (mpd.bio)
		mpage_bio_submit_write(0, mpd.bio);
	blk_finish_plug(&plug);
	return ret;
}

#endif /* CONFIG_SDFAT_ALIGNED_MPAGE_WRITE */


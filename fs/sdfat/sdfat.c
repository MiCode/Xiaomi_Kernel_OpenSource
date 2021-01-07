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
#include <linux/init.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/pagemap.h>
#include <linux/mpage.h>
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
#include <linux/vmalloc.h>
#include <asm/current.h>
#include <asm/unaligned.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#include <linux/aio.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 0)
#error SDFAT only supports linux kernel version 3.0 or higher
#endif

#include "sdfat.h"
#include "version.h"

/* skip iterating emit_dots when dir is empty */
#define ITER_POS_FILLED_DOTS	(2)

/* type index declare at sdfat.h */
const char *FS_TYPE_STR[] = {
	"auto",
	"exfat",
	"vfat"
};

static struct kset *sdfat_kset;
static struct kmem_cache *sdfat_inode_cachep;


static int sdfat_default_codepage = CONFIG_SDFAT_DEFAULT_CODEPAGE;
static char sdfat_default_iocharset[] = CONFIG_SDFAT_DEFAULT_IOCHARSET;
static const char sdfat_iocharset_with_utf8[] = "iso8859-1";

#ifdef CONFIG_SDFAT_TRACE_SB_LOCK
static unsigned long __lock_jiffies;
#endif

static void sdfat_truncate(struct inode *inode, loff_t old_size);
static int sdfat_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create);

static struct inode *sdfat_iget(struct super_block *sb, loff_t i_pos);
static struct inode *sdfat_build_inode(struct super_block *sb, const FILE_ID_T *fid, loff_t i_pos);
static void sdfat_detach(struct inode *inode);
static void sdfat_attach(struct inode *inode, loff_t i_pos);
static inline unsigned long sdfat_hash(loff_t i_pos);
static int __sdfat_write_inode(struct inode *inode, int sync);
static int sdfat_sync_inode(struct inode *inode);
static int sdfat_write_inode(struct inode *inode, struct writeback_control *wbc);
static void sdfat_write_super(struct super_block *sb);
static void sdfat_write_failed(struct address_space *mapping, loff_t to);

static void sdfat_init_namebuf(DENTRY_NAMEBUF_T *nb);
static int sdfat_alloc_namebuf(DENTRY_NAMEBUF_T *nb);
static void sdfat_free_namebuf(DENTRY_NAMEBUF_T *nb);

/*************************************************************************
 * INNER FUNCTIONS FOR FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
static int __sdfat_getattr(struct inode *inode, struct kstat *stat);
static void __sdfat_writepage_end_io(struct bio *bio, int err);
static inline void __lock_super(struct super_block *sb);
static inline void __unlock_super(struct super_block *sb);
static int __sdfat_create(struct inode *dir, struct dentry *dentry);
static int __sdfat_revalidate(struct dentry *dentry);
static int __sdfat_revalidate_ci(struct dentry *dentry, unsigned int flags);
static int __sdfat_file_fsync(struct file *filp, loff_t start, loff_t end, int datasync);
static struct dentry *__sdfat_lookup(struct inode *dir, struct dentry *dentry);
static int __sdfat_mkdir(struct inode *dir, struct dentry *dentry);
static int __sdfat_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry);
static int __sdfat_show_options(struct seq_file *m, struct super_block *sb);
static inline ssize_t __sdfat_blkdev_direct_IO(int rw, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		unsigned long nr_segs);
static inline ssize_t __sdfat_direct_IO(int rw, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		loff_t count, unsigned long nr_segs);
static int __sdfat_d_hash(const struct dentry *dentry, struct qstr *qstr);
static int __sdfat_d_hashi(const struct dentry *dentry, struct qstr *qstr);
static int __sdfat_cmp(const struct dentry *dentry, unsigned int len,
		const char *str, const struct qstr *name);
static int __sdfat_cmpi(const struct dentry *dentry, unsigned int len,
		const char *str, const struct qstr *name);

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


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
#define CURRENT_TIME_SEC	timespec_trunc(current_kernel_time(), NSEC_PER_SEC)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static int sdfat_getattr(const struct path *path, struct kstat *stat,
			u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_backing_inode(path->dentry);

	return __sdfat_getattr(inode, stat);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0) */
static int sdfat_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;

	return __sdfat_getattr(inode, stat);
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


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
static int sdfat_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry,
		unsigned int flags)
{
	/*
	 * The VFS already checks for existence, so for local filesystems
	 * the RENAME_NOREPLACE implementation is equivalent to plain rename.
	 * Don't support any other flags
	 */
	if (flags & ~RENAME_NOREPLACE)
		return -EINVAL;
	return __sdfat_rename(old_dir, old_dentry, new_dir, new_dentry);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0) */
static int sdfat_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	return __sdfat_rename(old_dir, old_dentry, new_dir, new_dentry);
}

static int setattr_prepare(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;

	return inode_change_ok(inode, attr);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static inline void __sdfat_submit_bio_write(struct bio *bio)
{
	bio_set_op_attrs(bio, REQ_OP_WRITE, 0);
	submit_bio(bio);
}

static inline unsigned int __sdfat_full_name_hash(const struct dentry *dentry, const char *name, unsigned int len)
{
	return full_name_hash(dentry, name, len);
}

static inline unsigned long __sdfat_init_name_hash(const struct dentry *dentry)
{
	return init_name_hash(dentry);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0) */
static inline void __sdfat_submit_bio_write(struct bio *bio)
{
	submit_bio(WRITE, bio);
}

static inline unsigned int __sdfat_full_name_hash(const struct dentry *unused, const char *name, unsigned int len)
{
	return full_name_hash(name, len);
}

static inline unsigned long __sdfat_init_name_hash(const struct dentry *unused)
{
	return init_name_hash();
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 21)
       /* EMPTY */
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 21) */
static inline void inode_lock(struct inode *inode)
{
	       mutex_lock(&inode->i_mutex);
}

static inline void inode_unlock(struct inode *inode)
{
	       mutex_unlock(&inode->i_mutex);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static inline int sdfat_remount_syncfs(struct super_block *sb)
{
	sync_filesystem(sb);
	return 0;
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0) */
static inline int sdfat_remount_syncfs(struct super_block *sb)
{
	/*
	 * We don`t need to call sync_filesystem(sb),
	 * Because VFS calls it.
	 */
	return 0;
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static inline sector_t __sdfat_bio_sector(struct bio *bio)
{
	return bio->bi_iter.bi_sector;
}

static inline void __sdfat_set_bio_iterate(struct bio *bio, sector_t sector,
		unsigned int size, unsigned int idx, unsigned int done)
{
	struct bvec_iter *iter = &(bio->bi_iter);

	iter->bi_sector = sector;
	iter->bi_size = size;
	iter->bi_idx = idx;
	iter->bi_bvec_done = done;
}

static void __sdfat_truncate_pagecache(struct inode *inode,
					loff_t to, loff_t newsize)
{
	truncate_pagecache(inode, newsize);
}

static int sdfat_d_hash(const struct dentry *dentry, struct qstr *qstr)
{
	return __sdfat_d_hash(dentry, qstr);
}

static int sdfat_d_hashi(const struct dentry *dentry, struct qstr *qstr)
{
	return __sdfat_d_hashi(dentry, qstr);
}

//instead of sdfat_readdir
static int sdfat_iterate(struct file *filp, struct dir_context *ctx)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	DIR_ENTRY_T de;
	DENTRY_NAMEBUF_T *nb = &(de.NameBuf);
	unsigned long inum;
	loff_t cpos;
	int err = 0, fake_offset = 0;

	sdfat_init_namebuf(nb);
	__lock_super(sb);

	cpos = ctx->pos;
	if ((fsi->vol_type == EXFAT) || (inode->i_ino == SDFAT_ROOT_INO)) {
		if (!dir_emit_dots(filp, ctx))
			goto out;
		if (ctx->pos == ITER_POS_FILLED_DOTS) {
			cpos = 0;
			fake_offset = 1;
		}
	}
	if (cpos & (DENTRY_SIZE - 1)) {
		err = -ENOENT;
		goto out;
	}

	/* name buffer should be allocated before use */
	err = sdfat_alloc_namebuf(nb);
	if (err)
		goto out;
get_new:
	SDFAT_I(inode)->fid.size = i_size_read(inode);
	SDFAT_I(inode)->fid.rwoffset = cpos >> DENTRY_SIZE_BITS;

	if (cpos >= SDFAT_I(inode)->fid.size)
		goto end_of_dir;

	err = fsapi_readdir(inode, &de);
	if (err) {
		// at least we tried to read a sector
		// move cpos to next sector position (should be aligned)
		if (err == -EIO) {
			cpos += 1 << (sb->s_blocksize_bits);
			cpos &= ~((u32)sb->s_blocksize-1);
		}

		err = -EIO;
		goto end_of_dir;
	}

	cpos = SDFAT_I(inode)->fid.rwoffset << DENTRY_SIZE_BITS;

	if (!nb->lfn[0])
		goto end_of_dir;

	if (!memcmp(nb->sfn, DOS_CUR_DIR_NAME, DOS_NAME_LENGTH)) {
		inum = inode->i_ino;
	} else if (!memcmp(nb->sfn, DOS_PAR_DIR_NAME, DOS_NAME_LENGTH)) {
		inum = parent_ino(filp->f_path.dentry);
	} else {
		loff_t i_pos = ((loff_t) SDFAT_I(inode)->fid.start_clu << 32) |
			((SDFAT_I(inode)->fid.rwoffset-1) & 0xffffffff);
		struct inode *tmp = sdfat_iget(sb, i_pos);

		if (tmp) {
			inum = tmp->i_ino;
			iput(tmp);
		} else {
			inum = iunique(sb, SDFAT_ROOT_INO);
		}
	}

	/* Before calling dir_emit(), sb_lock should be released.
	 * Because page fault can occur in dir_emit() when the size of buffer given
	 * from user is larger than one page size
	 */
	__unlock_super(sb);
	if (!dir_emit(ctx, nb->lfn, strlen(nb->lfn), inum,
			(de.Attr & ATTR_SUBDIR) ? DT_DIR : DT_REG))
		goto out_unlocked;
	__lock_super(sb);

	ctx->pos = cpos;
	goto get_new;

end_of_dir:
	if (!cpos && fake_offset)
		cpos = ITER_POS_FILLED_DOTS;
	ctx->pos = cpos;
out:
	__unlock_super(sb);
out_unlocked:
	/*
	 * To improve performance, free namebuf after unlock sb_lock.
	 * If namebuf is not allocated, this function do nothing
	 */
	sdfat_free_namebuf(nb);
	return err;
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) */
static inline sector_t __sdfat_bio_sector(struct bio *bio)
{
	return bio->bi_sector;
}

static inline void __sdfat_set_bio_iterate(struct bio *bio, sector_t sector,
		unsigned int size, unsigned int idx, unsigned int done)
{
	bio->bi_sector = sector;
	bio->bi_idx = idx;
	bio->bi_size = size; //PAGE_SIZE;
}

static void __sdfat_truncate_pagecache(struct inode *inode,
					loff_t to, loff_t newsize)
{
	truncate_pagecache(inode, to, newsize);
}

static int sdfat_d_hash(const struct dentry *dentry,
		const struct inode *inode, struct qstr *qstr)
{
	return __sdfat_d_hash(dentry, qstr);
}

static int sdfat_d_hashi(const struct dentry *dentry,
		const struct inode *inode, struct qstr *qstr)
{
	return __sdfat_d_hashi(dentry, qstr);
}

static int sdfat_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	DIR_ENTRY_T de;
	DENTRY_NAMEBUF_T *nb = &(de.NameBuf);
	unsigned long inum;
	loff_t cpos;
	int err = 0, fake_offset = 0;

	sdfat_init_namebuf(nb);
	__lock_super(sb);

	cpos = filp->f_pos;
	/* Fake . and .. for the root directory. */
	if ((fsi->vol_type == EXFAT) || (inode->i_ino == SDFAT_ROOT_INO)) {
		while (cpos < ITER_POS_FILLED_DOTS) {
			if (inode->i_ino == SDFAT_ROOT_INO)
				inum = SDFAT_ROOT_INO;
			else if (cpos == 0)
				inum = inode->i_ino;
			else /* (cpos == 1) */
				inum = parent_ino(filp->f_path.dentry);

			if (filldir(dirent, "..", cpos+1, cpos, inum, DT_DIR) < 0)
				goto out;
			cpos++;
			filp->f_pos++;
		}
		if (cpos == ITER_POS_FILLED_DOTS) {
			cpos = 0;
			fake_offset = 1;
		}
	}
	if (cpos & (DENTRY_SIZE - 1)) {
		err = -ENOENT;
		goto out;
	}

	/* name buffer should be allocated before use */
	err = sdfat_alloc_namebuf(nb);
	if (err)
		goto out;
get_new:
	SDFAT_I(inode)->fid.size = i_size_read(inode);
	SDFAT_I(inode)->fid.rwoffset = cpos >> DENTRY_SIZE_BITS;

	if (cpos >= SDFAT_I(inode)->fid.size)
		goto end_of_dir;

	err = fsapi_readdir(inode, &de);
	if (err) {
		// at least we tried to read a sector
		// move cpos to next sector position (should be aligned)
		if (err == -EIO) {
			cpos += 1 << (sb->s_blocksize_bits);
			cpos &= ~((u32)sb->s_blocksize-1);
		}

		err = -EIO;
		goto end_of_dir;
	}

	cpos = SDFAT_I(inode)->fid.rwoffset << DENTRY_SIZE_BITS;

	if (!nb->lfn[0])
		goto end_of_dir;

	if (!memcmp(nb->sfn, DOS_CUR_DIR_NAME, DOS_NAME_LENGTH)) {
		inum = inode->i_ino;
	} else if (!memcmp(nb->sfn, DOS_PAR_DIR_NAME, DOS_NAME_LENGTH)) {
		inum = parent_ino(filp->f_path.dentry);
	} else {
		loff_t i_pos = ((loff_t) SDFAT_I(inode)->fid.start_clu << 32) |
			   ((SDFAT_I(inode)->fid.rwoffset-1) & 0xffffffff);
		struct inode *tmp = sdfat_iget(sb, i_pos);

		if (tmp) {
			inum = tmp->i_ino;
			iput(tmp);
		} else {
			inum = iunique(sb, SDFAT_ROOT_INO);
		}
	}

	/* Before calling dir_emit(), sb_lock should be released.
	 * Because page fault can occur in dir_emit() when the size of buffer given
	 * from user is larger than one page size
	 */
	__unlock_super(sb);
	if (filldir(dirent, nb->lfn, strlen(nb->lfn), cpos, inum,
				(de.Attr & ATTR_SUBDIR) ? DT_DIR : DT_REG) < 0)
		goto out_unlocked;
	__lock_super(sb);

	filp->f_pos = cpos;
	goto get_new;

end_of_dir:
	if (!cpos && fake_offset)
		cpos = ITER_POS_FILLED_DOTS;
	filp->f_pos = cpos;
out:
	__unlock_super(sb);
out_unlocked:
	/*
	 * To improve performance, free namebuf after unlock sb_lock.
	 * If namebuf is not allocated, this function do nothing
	 */
	sdfat_free_namebuf(nb);
	return err;
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
	/* EMPTY */
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0) */
static inline struct inode *file_inode(const struct file *f)
{
	return f->f_dentry->d_inode;
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
static inline int __is_sb_dirty(struct super_block *sb)
{
	return SDFAT_SB(sb)->s_dirt;
}

static inline void __set_sb_clean(struct super_block *sb)
{
	SDFAT_SB(sb)->s_dirt = 0;
}

/* Workqueue wrapper for sdfat_write_super () */
static void __write_super_delayed(struct work_struct *work)
{
	struct sdfat_sb_info *sbi;
	struct super_block *sb;

	sbi = container_of(work, struct sdfat_sb_info, write_super_work.work);
	sb = sbi->host_sb;

	/* XXX: Is this needed? */
	if (!sb || !down_read_trylock(&sb->s_umount)) {
		DMSG("%s: skip delayed work(write_super).\n", __func__);
		return;
	}

	DMSG("%s: do delayed_work(write_super).\n", __func__);

	spin_lock(&sbi->work_lock);
	sbi->write_super_queued = 0;
	spin_unlock(&sbi->work_lock);

	sdfat_write_super(sb);

	up_read(&sb->s_umount);
}

static void setup_sdfat_sync_super_wq(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	mutex_init(&sbi->s_lock);
	spin_lock_init(&sbi->work_lock);
	INIT_DELAYED_WORK(&sbi->write_super_work, __write_super_delayed);
	sbi->host_sb = sb;
}

static inline bool __cancel_delayed_work_sync(struct sdfat_sb_info *sbi)
{
	return cancel_delayed_work_sync(&sbi->write_super_work);
}

static inline void lock_super(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	mutex_lock(&sbi->s_lock);
}

static inline void unlock_super(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	mutex_unlock(&sbi->s_lock);
}

static int sdfat_revalidate(struct dentry *dentry, unsigned int flags)
{
	if (flags & LOOKUP_RCU)
		return -ECHILD;

	return __sdfat_revalidate(dentry);
}

static int sdfat_revalidate_ci(struct dentry *dentry, unsigned int flags)
{
	if (flags & LOOKUP_RCU)
		return -ECHILD;

	return __sdfat_revalidate_ci(dentry, flags);
}

static struct inode *sdfat_iget(struct super_block *sb, loff_t i_pos)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct sdfat_inode_info *info;
	struct hlist_head *head = sbi->inode_hashtable + sdfat_hash(i_pos);
	struct inode *inode = NULL;

	spin_lock(&sbi->inode_hash_lock);
	hlist_for_each_entry(info, head, i_hash_fat) {
		BUG_ON(info->vfs_inode.i_sb != sb);

		if (i_pos != info->i_pos)
			continue;
		inode = igrab(&info->vfs_inode);
		if (inode)
			break;
	}
	spin_unlock(&sbi->inode_hash_lock);
	return inode;
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0) */
static inline int __is_sb_dirty(struct super_block *sb)
{
	return sb->s_dirt;
}

static inline void __set_sb_clean(struct super_block *sb)
{
	sb->s_dirt = 0;
}

static void setup_sdfat_sync_super_wq(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	sbi->host_sb = sb;
}

static inline bool __cancel_delayed_work_sync(struct sdfat_sb_info *sbi)
{
	/* DO NOTHING */
	return 0;
}

static inline void clear_inode(struct inode *inode)
{
	end_writeback(inode);
}

static int sdfat_revalidate(struct dentry *dentry, struct nameidata *nd)
{
	if (nd && nd->flags & LOOKUP_RCU)
		return -ECHILD;

	return __sdfat_revalidate(dentry);
}

static int sdfat_revalidate_ci(struct dentry *dentry, struct nameidata *nd)
{
	if (nd && nd->flags & LOOKUP_RCU)
		return -ECHILD;

	return __sdfat_revalidate_ci(dentry, nd ? nd->flags : 0);

}

static struct inode *sdfat_iget(struct super_block *sb, loff_t i_pos)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct sdfat_inode_info *info;
	struct hlist_node *node;
	struct hlist_head *head = sbi->inode_hashtable + sdfat_hash(i_pos);
	struct inode *inode = NULL;

	spin_lock(&sbi->inode_hash_lock);
	hlist_for_each_entry(info, node, head, i_hash_fat) {
		BUG_ON(info->vfs_inode.i_sb != sb);

		if (i_pos != info->i_pos)
			continue;
		inode = igrab(&info->vfs_inode);
		if (inode)
			break;
	}
	spin_unlock(&sbi->inode_hash_lock);
	return inode;
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
static struct dentry *sdfat_lookup(struct inode *dir, struct dentry *dentry,
						   unsigned int flags)
{
	return __sdfat_lookup(dir, dentry);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0) */
static struct dentry *sdfat_lookup(struct inode *dir, struct dentry *dentry,
						   struct nameidata *nd)
{
	return __sdfat_lookup(dir, dentry);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	/* NOTHING NOW */
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0) */
#define GLOBAL_ROOT_UID (0)
#define GLOBAL_ROOT_GID (0)

static inline bool uid_eq(uid_t left, uid_t right)
{
	return left == right;
}

static inline bool gid_eq(gid_t left, gid_t right)
{
	return left == right;
}

static inline uid_t from_kuid_munged(struct user_namespace *to, uid_t kuid)
{
	return kuid;
}

static inline gid_t from_kgid_munged(struct user_namespace *to, gid_t kgid)
{
	return kgid;
}

static inline uid_t make_kuid(struct user_namespace *from, uid_t uid)
{
	return uid;
}

static inline gid_t make_kgid(struct user_namespace *from, gid_t gid)
{
	return gid;
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static struct dentry *__d_make_root(struct inode *root_inode)
{
	return d_make_root(root_inode);
}

static void __sdfat_do_truncate(struct inode *inode, loff_t old, loff_t new)
{
	down_write(&SDFAT_I(inode)->truncate_lock);
	truncate_setsize(inode, new);
	sdfat_truncate(inode, old);
	up_write(&SDFAT_I(inode)->truncate_lock);
}

static sector_t sdfat_aop_bmap(struct address_space *mapping, sector_t block)
{
	sector_t blocknr;

	/* sdfat_get_cluster() assumes the requested blocknr isn't truncated. */
	down_read(&SDFAT_I(mapping->host)->truncate_lock);
	blocknr = generic_block_bmap(mapping, block, sdfat_get_block);
	up_read(&SDFAT_I(mapping->host)->truncate_lock);
	return blocknr;
}

static int sdfat_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	return __sdfat_mkdir(dir, dentry);
}

static int sdfat_show_options(struct seq_file *m, struct dentry *root)
{
	return __sdfat_show_options(m, root->d_sb);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0) */
static inline void set_nlink(struct inode *inode, unsigned int nlink)
{
	inode->i_nlink = nlink;
}

static struct dentry *__d_make_root(struct inode *root_inode)
{
	return d_alloc_root(root_inode);
}

static void __sdfat_do_truncate(struct inode *inode, loff_t old, loff_t new)
{
		truncate_setsize(inode, new);
		sdfat_truncate(inode, old);
}

static sector_t sdfat_aop_bmap(struct address_space *mapping, sector_t block)
{
	sector_t blocknr;

	/* sdfat_get_cluster() assumes the requested blocknr isn't truncated. */
	down_read(&mapping->host->i_alloc_sem);
	blocknr = generic_block_bmap(mapping, block, sdfat_get_block);
	up_read(&mapping->host->i_alloc_sem);
	return blocknr;
}

static int sdfat_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	return __sdfat_mkdir(dir, dentry);
}

static int sdfat_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	return __sdfat_show_options(m, mnt->mnt_sb);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 1, 0)
#define __sdfat_generic_file_fsync(filp, start, end, datasync) \
		generic_file_fsync(filp, start, end, datasync)

static int sdfat_file_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	return __sdfat_file_fsync(filp, start, end, datasync);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 0) */
#define __sdfat_generic_file_fsync(filp, start, end, datasync) \
		generic_file_fsync(filp, datasync)
static int sdfat_file_fsync(struct file *filp, int datasync)
{
	return __sdfat_file_fsync(filp, 0, 0, datasync);
}
#endif

/*************************************************************************
 * MORE FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
static void sdfat_writepage_end_io(struct bio *bio)
{
	__sdfat_writepage_end_io(bio, blk_status_to_errno(bio->bi_status));
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
static void sdfat_writepage_end_io(struct bio *bio)
{
	__sdfat_writepage_end_io(bio, bio->bi_error);
}
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0) */
static void sdfat_writepage_end_io(struct bio *bio, int err)
{
	if (test_bit(BIO_UPTODATE, &bio->bi_flags))
		err = 0;
	__sdfat_writepage_end_io(bio, err);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static int sdfat_cmp(const struct dentry *dentry,
		unsigned int len, const char *str, const struct qstr *name)
{
	return __sdfat_cmp(dentry, len, str, name);
}

static int sdfat_cmpi(const struct dentry *dentry,
		unsigned int len, const char *str, const struct qstr *name)
{
	return __sdfat_cmpi(dentry, len, str, name);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
static int sdfat_cmp(const struct dentry *parent, const struct dentry *dentry,
		unsigned int len, const char *str, const struct qstr *name)
{
	return __sdfat_cmp(dentry, len, str, name);
}

static int sdfat_cmpi(const struct dentry *parent, const struct dentry *dentry,
		unsigned int len, const char *str, const struct qstr *name)
{
	return __sdfat_cmpi(dentry, len, str, name);
}
#else
static int sdfat_cmp(const struct dentry *parent, const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	return __sdfat_cmp(dentry, len, str, name);
}

static int sdfat_cmpi(const struct dentry *parent, const struct inode *pinode,
		const struct dentry *dentry, const struct inode *inode,
		unsigned int len, const char *str, const struct qstr *name)
{
	return __sdfat_cmpi(dentry, len, str, name);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
static ssize_t sdfat_direct_IO(struct kiocb *iocb, struct iov_iter *iter)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	size_t count = iov_iter_count(iter);
	int rw = iov_iter_rw(iter);
	loff_t offset = iocb->ki_pos;

	return __sdfat_direct_IO(rw, iocb, inode,
			(void *)iter, offset, count, 0 /* UNUSED */);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
static ssize_t sdfat_direct_IO(struct kiocb *iocb,
		struct iov_iter *iter,
		loff_t offset)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	size_t count = iov_iter_count(iter);
	int rw = iov_iter_rw(iter);

	return __sdfat_direct_IO(rw, iocb, inode,
			(void *)iter, offset, count, 0 /* UNUSED */);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t sdfat_direct_IO(int rw, struct kiocb *iocb,
		struct iov_iter *iter,
		loff_t offset)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	size_t count = iov_iter_count(iter);

	return __sdfat_direct_IO(rw, iocb, inode,
			(void *)iter, offset, count, 0 /* UNUSED */);
}
#else
static ssize_t sdfat_direct_IO(int rw, struct kiocb *iocb,
	   const struct iovec *iov, loff_t offset, unsigned long nr_segs)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	size_t count = iov_length(iov, nr_segs);

	return __sdfat_direct_IO(rw, iocb, inode,
			(void *)iov, offset, count, nr_segs);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
static inline ssize_t __sdfat_blkdev_direct_IO(int unused, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t unused_1,
		unsigned long nr_segs)
{
	struct iov_iter *iter = (struct iov_iter *)iov_u;

	return blockdev_direct_IO(iocb, inode, iter, sdfat_get_block);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
static inline ssize_t __sdfat_blkdev_direct_IO(int unused, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		unsigned long nr_segs)
{
	struct iov_iter *iter = (struct iov_iter *)iov_u;

	return blockdev_direct_IO(iocb, inode, iter, offset, sdfat_get_block);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static inline ssize_t __sdfat_blkdev_direct_IO(int rw, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		unsigned long nr_segs)
{
	struct iov_iter *iter = (struct iov_iter *)iov_u;

	return blockdev_direct_IO(rw, iocb, inode, iter,
					offset, sdfat_get_block);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static inline ssize_t __sdfat_blkdev_direct_IO(int rw, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		unsigned long nr_segs)
{
	const struct iovec *iov = (const struct iovec *)iov_u;

	return blockdev_direct_IO(rw, iocb, inode, iov,
					offset, nr_segs, sdfat_get_block);
}
#else
static inline ssize_t __sdfat_blkdev_direct_IO(int rw, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		unsigned long nr_segs)
{
	const struct iovec *iov = (const struct iovec *)iov_u;

	return blockdev_direct_IO(rw, iocb, inode, inode->i_sb->s_bdev, iov,
					offset, nr_segs, sdfat_get_block, NULL);
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
static const char *sdfat_follow_link(struct dentry *dentry, struct inode *inode, struct delayed_call *done)
{
	struct sdfat_inode_info *ei = SDFAT_I(inode);

	return (char *)(ei->target);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
static const char *sdfat_follow_link(struct dentry *dentry, void **cookie)
{
	struct sdfat_inode_info *ei = SDFAT_I(dentry->d_inode);

	return *cookie = (char *)(ei->target);
}
#else
static void *sdfat_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct sdfat_inode_info *ei = SDFAT_I(dentry->d_inode);

	nd_set_link(nd, (char *)(ei->target));
	return NULL;
}
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
static int sdfat_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			 bool excl)
{
	return __sdfat_create(dir, dentry);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
static int sdfat_create(struct inode *dir, struct dentry *dentry, umode_t mode,
			struct nameidata *nd)
{
	return __sdfat_create(dir, dentry);
}
#else
static int sdfat_create(struct inode *dir, struct dentry *dentry, int mode,
			struct nameidata *nd)
{
	return __sdfat_create(dir, dentry);
}
#endif


/*************************************************************************
 * WRAP FUNCTIONS FOR DEBUGGING
 *************************************************************************/
#ifdef CONFIG_SDFAT_TRACE_SB_LOCK
static inline void __lock_super(struct super_block *sb)
{
	lock_super(sb);
	__lock_jiffies = jiffies;
}

static inline void __unlock_super(struct super_block *sb)
{
	int time = ((jiffies - __lock_jiffies) * 1000 / HZ);
	/* FIXME : error message should be modified */
	if (time > 10)
		EMSG("lock_super in %s (%d ms)\n", __func__, time);

	unlock_super(sb);
}
#else /* CONFIG_SDFAT_TRACE_SB_LOCK */
static inline void __lock_super(struct super_block *sb)
{
	lock_super(sb);
}

static inline void __unlock_super(struct super_block *sb)
{
	unlock_super(sb);
}
#endif /* CONFIG_SDFAT_TRACE_SB_LOCK */

/*************************************************************************
 * NORMAL FUNCTIONS
 *************************************************************************/
static inline loff_t sdfat_make_i_pos(FILE_ID_T *fid)
{
	return ((loff_t) fid->dir.dir << 32) | (fid->entry & 0xffffffff);
}

/*======================================================================*/
/*  Directory Entry Name Buffer Operations                              */
/*======================================================================*/
static void sdfat_init_namebuf(DENTRY_NAMEBUF_T *nb)
{
	nb->lfn = NULL;
	nb->sfn = NULL;
	nb->lfnbuf_len = 0;
	nb->sfnbuf_len = 0;
}

static int sdfat_alloc_namebuf(DENTRY_NAMEBUF_T *nb)
{
	nb->lfn = __getname();
	if (!nb->lfn)
		return -ENOMEM;
	nb->sfn = nb->lfn + MAX_VFSNAME_BUF_SIZE;
	nb->lfnbuf_len = MAX_VFSNAME_BUF_SIZE;
	nb->sfnbuf_len = MAX_VFSNAME_BUF_SIZE;
	return 0;
}

static void sdfat_free_namebuf(DENTRY_NAMEBUF_T *nb)
{
	if (!nb->lfn)
		return;

	__putname(nb->lfn);
	sdfat_init_namebuf(nb);
}

/*======================================================================*/
/*  Directory Entry Operations                                          */
/*======================================================================*/
#define SDFAT_DSTATE_LOCKED	(void *)(0xCAFE2016)
#define SDFAT_DSTATE_UNLOCKED	(void *)(0x00000000)

static inline void __lock_d_revalidate(struct dentry *dentry)
{
	spin_lock(&dentry->d_lock);
	dentry->d_fsdata = SDFAT_DSTATE_LOCKED;
	spin_unlock(&dentry->d_lock);
}

static inline void __unlock_d_revalidate(struct dentry *dentry)
{
	spin_lock(&dentry->d_lock);
	dentry->d_fsdata = SDFAT_DSTATE_UNLOCKED;
	spin_unlock(&dentry->d_lock);
}

/* __check_dstate_locked requires dentry->d_lock */
static inline int __check_dstate_locked(struct dentry *dentry)
{
	if (dentry->d_fsdata == SDFAT_DSTATE_LOCKED)
		return 1;

	return 0;
}

/*
 * If new entry was created in the parent, it could create the 8.3
 * alias (the shortname of logname).  So, the parent may have the
 * negative-dentry which matches the created 8.3 alias.
 *
 * If it happened, the negative dentry isn't actually negative
 * anymore.  So, drop it.
 */
static int __sdfat_revalidate_common(struct dentry *dentry)
{
	int ret = 1;

	spin_lock(&dentry->d_lock);
	if ((!dentry->d_inode) && (!__check_dstate_locked(dentry) &&
		(dentry->d_time != dentry->d_parent->d_inode->i_version))) {
		ret = 0;
	}
	spin_unlock(&dentry->d_lock);
	return ret;
}

static int __sdfat_revalidate(struct dentry *dentry)
{
	/* This is not negative dentry. Always valid. */
	if (dentry->d_inode)
		return 1;
	return __sdfat_revalidate_common(dentry);
}

static int __sdfat_revalidate_ci(struct dentry *dentry, unsigned int flags)
{
	/*
	 * This is not negative dentry. Always valid.
	 *
	 * Note, rename() to existing directory entry will have ->d_inode,
	 * and will use existing name which isn't specified name by user.
	 *
	 * We may be able to drop this positive dentry here. But dropping
	 * positive dentry isn't good idea. So it's unsupported like
	 * rename("filename", "FILENAME") for now.
	 */
	if (dentry->d_inode)
		return 1;
#if 0	/* Blocked below code for lookup_one_len() called by stackable FS */
	/*
	 * This may be nfsd (or something), anyway, we can't see the
	 * intent of this. So, since this can be for creation, drop it.
	 */
	if (!flags)
		return 0;
#endif
	/*
	 * Drop the negative dentry, in order to make sure to use the
	 * case sensitive name which is specified by user if this is
	 * for creation.
	 */
	if (flags & (LOOKUP_CREATE | LOOKUP_RENAME_TARGET))
		return 0;
	return __sdfat_revalidate_common(dentry);
}


/* returns the length of a struct qstr, ignoring trailing dots */
static unsigned int __sdfat_striptail_len(unsigned int len, const char *name)
{
	while (len && name[len - 1] == '.')
		len--;
	return len;
}

static unsigned int sdfat_striptail_len(const struct qstr *qstr)
{
	return __sdfat_striptail_len(qstr->len, qstr->name);
}

/*
 * Compute the hash for the sdfat name corresponding to the dentry.
 * Note: if the name is invalid, we leave the hash code unchanged so
 * that the existing dentry can be used. The sdfat fs routines will
 * return ENOENT or EINVAL as appropriate.
 */
static int __sdfat_d_hash(const struct dentry *dentry, struct qstr *qstr)
{
	unsigned int len = sdfat_striptail_len(qstr);

	qstr->hash = __sdfat_full_name_hash(dentry, qstr->name, len);
	return 0;
}

/*
 * Compute the hash for the sdfat name corresponding to the dentry.
 * Note: if the name is invalid, we leave the hash code unchanged so
 * that the existing dentry can be used. The sdfat fs routines will
 * return ENOENT or EINVAL as appropriate.
 */
static int __sdfat_d_hashi(const struct dentry *dentry, struct qstr *qstr)
{
	struct nls_table *t = SDFAT_SB(dentry->d_sb)->nls_io;
	const unsigned char *name;
	unsigned int len;
	unsigned long hash;

	name = qstr->name;
	len = sdfat_striptail_len(qstr);

	hash = __sdfat_init_name_hash(dentry);
	while (len--)
		hash = partial_name_hash(nls_tolower(t, *name++), hash);
	qstr->hash = end_name_hash(hash);

	return 0;
}

/*
 * Case sensitive compare of two sdfat names.
 */
static int __sdfat_cmp(const struct dentry *dentry, unsigned int len,
		const char *str, const struct qstr *name)
{
	unsigned int alen, blen;

	/* A filename cannot end in '.' or we treat it like it has none */
	alen = sdfat_striptail_len(name);
	blen = __sdfat_striptail_len(len, str);
	if (alen == blen) {
		if (strncmp(name->name, str, alen) == 0)
			return 0;
	}
	return 1;
}

/*
 * Case insensitive compare of two sdfat names.
 */
static int __sdfat_cmpi(const struct dentry *dentry, unsigned int len,
		const char *str, const struct qstr *name)
{
	struct nls_table *t = SDFAT_SB(dentry->d_sb)->nls_io;
	unsigned int alen, blen;

	/* A filename cannot end in '.' or we treat it like it has none */
	alen = sdfat_striptail_len(name);
	blen = __sdfat_striptail_len(len, str);
	if (alen == blen) {
		if (nls_strnicmp(t, name->name, str, alen) == 0)
			return 0;
	}
	return 1;
}

static const struct dentry_operations sdfat_dentry_ops = {
	.d_revalidate   = sdfat_revalidate,
	.d_hash         = sdfat_d_hash,
	.d_compare      = sdfat_cmp,
};

static const struct dentry_operations sdfat_ci_dentry_ops = {
	.d_revalidate   = sdfat_revalidate_ci,
	.d_hash         = sdfat_d_hashi,
	.d_compare      = sdfat_cmpi,
};

#ifdef	CONFIG_SDFAT_DFR
/*----------------------------------------------------------------------*/
/*  Defragmentation related                                             */
/*----------------------------------------------------------------------*/
/**
 * @fn		defrag_cleanup_reqs
 * @brief	clean-up defrag info depending on error flag
 * @return	void
 * @param	sb		super block
 * @param	error	error flag
 */
static void defrag_cleanup_reqs(INOUT struct super_block *sb, IN int error)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct defrag_info *sb_dfr = &(sbi->dfr_info);
	struct defrag_info *ino_dfr = NULL, *tmp = NULL;
	/* sdfat patch 0.96 : sbi->dfr_info crash problem */
	__lock_super(sb);

	/* Clean-up ino_dfr */
	if (!error) {
		list_for_each_entry_safe(ino_dfr, tmp, &sb_dfr->entry, entry) {
			struct inode *inode = &(container_of(ino_dfr, struct sdfat_inode_info, dfr_info)->vfs_inode);

			mutex_lock(&ino_dfr->lock);

			atomic_set(&ino_dfr->stat, DFR_INO_STAT_IDLE);

			list_del(&ino_dfr->entry);

			ino_dfr->chunks = NULL;
			ino_dfr->nr_chunks = 0;
			INIT_LIST_HEAD(&ino_dfr->entry);

			BUG_ON(!mutex_is_locked(&ino_dfr->lock));
			mutex_unlock(&ino_dfr->lock);

			iput(inode);
		}
	}

	/* Clean-up sb_dfr */
	sb_dfr->chunks = NULL;
	sb_dfr->nr_chunks = 0;
	INIT_LIST_HEAD(&sb_dfr->entry);

	/* Clear dfr_new_clus page */
	memset(sbi->dfr_new_clus, 0, PAGE_SIZE);
	sbi->dfr_new_idx = 1;
	memset(sbi->dfr_page_wb, 0, PAGE_SIZE);

	sbi->dfr_hint_clus = sbi->dfr_hint_idx = sbi->dfr_reserved_clus = 0;

	__unlock_super(sb);
}

/**
 * @fn		defrag_validate_pages
 * @brief	validate and mark dirty for victiim pages
 * @return	0 on success, -errno otherwise
 * @param	inode	inode
 * @param	chunk	given chunk
 * @remark	protected by inode_lock and super_lock
 */
static int
defrag_validate_pages(
	IN struct inode *inode,
	IN struct defrag_chunk_info *chunk)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct page *page = NULL;
	unsigned int i_size = 0, page_off = 0, page_nr = 0;
	int buf_i = 0, i = 0, err = 0;

	i_size = i_size_read(inode);
	page_off = chunk->f_clus * PAGES_PER_CLUS(sb);
	page_nr = (i_size / PAGE_SIZE) + ((i_size % PAGE_SIZE) ? 1 : 0);
	if ((i_size <= 0) || (page_nr <= 0)) {
		dfr_err("inode %p, i_size %d, page_nr %d", inode, i_size, page_nr);
		return -EINVAL;
	}

	/* Get victim pages
	 * and check its dirty/writeback/mapped state
	 */
	for (i = 0;
		i < min((int)(page_nr - page_off), (int)(chunk->nr_clus * PAGES_PER_CLUS(sb)));
		i++) {
		page = find_get_page(inode->i_mapping, page_off + i);
		if (page)
			if (!trylock_page(page)) {
				put_page(page);
				page = NULL;
			}

		if (!page) {
			dfr_debug("get/lock_page() failed, index %d", i);
			err = -EINVAL;
			goto error;
		}

		sbi->dfr_pagep[buf_i++] = page;
		if (PageError(page) || !PageUptodate(page) || PageDirty(page) ||
			PageWriteback(page) || page_mapped(page)) {
			dfr_debug("page %p, err %d, uptodate %d, "
				"dirty %d, wb %d, mapped %d",
				page, PageError(page), PageUptodate(page),
				PageDirty(page), PageWriteback(page),
				page_mapped(page));
			err = -EINVAL;
			goto error;
		}

		set_bit((page->index & (PAGES_PER_CLUS(sb) - 1)),
			(volatile unsigned long *)&(sbi->dfr_page_wb[chunk->new_idx + i / PAGES_PER_CLUS(sb)]));

		page = NULL;
	}

	/**
	 * All pages in the chunks are valid.
	 */
	i_size -= (chunk->f_clus * (sbi->fsi.cluster_size));
	BUG_ON(((i_size / PAGE_SIZE) + ((i_size % PAGE_SIZE) ? 1 : 0)) != (page_nr - page_off));

	for (i = 0; i < buf_i; i++) {
		struct buffer_head *bh = NULL, *head = NULL;
		int bh_idx = 0;

		page = sbi->dfr_pagep[i];
		BUG_ON(!page);

		/* Mark dirty in page */
		set_page_dirty(page);
		mark_page_accessed(page);

		/* Attach empty BHs */
		if (!page_has_buffers(page))
			create_empty_buffers(page, 1 << inode->i_blkbits, 0);

		/* Mark dirty in BHs */
		bh = head = page_buffers(page);
		BUG_ON(!bh && !i_size);
		do {
			if ((bh_idx >= 1) && (bh_idx >= (i_size >> inode->i_blkbits))) {
				clear_buffer_dirty(bh);
			} else {
				if (PageUptodate(page))
					if (!buffer_uptodate(bh))
						set_buffer_uptodate(bh);

				/* Set this bh as delay */
				set_buffer_new(bh);
				set_buffer_delay(bh);

				mark_buffer_dirty(bh);
			}

			bh_idx++;
			bh = bh->b_this_page;
		} while (bh != head);

		/* Mark this page accessed */
		mark_page_accessed(page);

		i_size -= PAGE_SIZE;
	}

error:
	/* Unlock and put refs for pages */
	for (i = 0; i < buf_i; i++) {
		BUG_ON(!sbi->dfr_pagep[i]);
		unlock_page(sbi->dfr_pagep[i]);
		put_page(sbi->dfr_pagep[i]);
	}
	memset(sbi->dfr_pagep, 0, sizeof(PAGE_SIZE));

	return err;
}


/**
 * @fn		defrag_validate_reqs
 * @brief	validate defrag requests
 * @return	negative if all requests not valid, 0 otherwise
 * @param	sb		super block
 * @param	chunks	given chunks
 */
static int
defrag_validate_reqs(
		IN struct super_block *sb,
		INOUT struct defrag_chunk_info *chunks)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct defrag_info *sb_dfr = &(sbi->dfr_info);
	int i = 0, err = 0, err_cnt = 0;

	/* Validate all reqs */
	for (i = REQ_HEADER_IDX + 1; i < sb_dfr->nr_chunks; i++) {
		struct defrag_chunk_info *chunk = NULL;
		struct inode *inode = NULL;
		struct defrag_info *ino_dfr = NULL;

		chunk = &chunks[i];

		/* Check inode */
		__lock_super(sb);
		inode = sdfat_iget(sb, chunk->i_pos);
		if (!inode) {
			dfr_debug("inode not found, i_pos %08llx", chunk->i_pos);
			chunk->stat = DFR_CHUNK_STAT_ERR;
			err_cnt++;
			__unlock_super(sb);
			continue;
		}
		__unlock_super(sb);

		dfr_debug("req[%d] inode %p, i_pos %08llx, f_clus %d, "
			  "d_clus %08x, nr %d, prev %08x, next %08x",
			i, inode, chunk->i_pos, chunk->f_clus, chunk->d_clus,
			chunk->nr_clus, chunk->prev_clus, chunk->next_clus);
		/**
		 * Lock ordering: inode_lock -> lock_super
		 */
		inode_lock(inode);
		__lock_super(sb);

		/* Check if enough buffers exist for chunk->new_idx */
		if ((sbi->dfr_new_idx + chunk->nr_clus) >= (PAGE_SIZE / sizeof(int))) {
			dfr_err("dfr_new_idx %d, chunk->nr_clus %d",
					sbi->dfr_new_idx, chunk->nr_clus);
			err = -ENOSPC;
			goto unlock;
		}

		/* Reserve clusters for defrag with DA */
		err = fsapi_dfr_reserve_clus(sb, chunk->nr_clus);
		if (err)
			goto unlock;

		/* Check clusters */
		err = fsapi_dfr_validate_clus(inode, chunk, 0);
		if (err) {
			fsapi_dfr_reserve_clus(sb, 0 - chunk->nr_clus);
			dfr_debug("Cluster validation: err %d", err);
			goto unlock;
		}

		/* Check pages */
		err = defrag_validate_pages(inode, chunk);
		if (err) {
			fsapi_dfr_reserve_clus(sb, 0 - chunk->nr_clus);
			dfr_debug("Page validation: err %d", err);
			goto unlock;
		}

		/* Mark IGNORE flag to victim AU */
		if (sbi->options.improved_allocation & SDFAT_ALLOC_SMART)
			fsapi_dfr_mark_ignore(sb, chunk->d_clus);

		ino_dfr = &(SDFAT_I(inode)->dfr_info);
		mutex_lock(&ino_dfr->lock);

		/* Update chunk info */
		chunk->stat = DFR_CHUNK_STAT_REQ;
		chunk->new_idx = sbi->dfr_new_idx;

		/* Update ino_dfr info */
		if (list_empty(&(ino_dfr->entry))) {
			list_add_tail(&ino_dfr->entry, &sb_dfr->entry);
			ino_dfr->chunks = chunk;
			igrab(inode);
		}
		ino_dfr->nr_chunks++;

		atomic_set(&ino_dfr->stat, DFR_INO_STAT_REQ);

		BUG_ON(!mutex_is_locked(&ino_dfr->lock));
		mutex_unlock(&ino_dfr->lock);

		/* Reserved buffers for chunk->new_idx */
		sbi->dfr_new_idx += chunk->nr_clus;

unlock:
		if (err) {
			chunk->stat = DFR_CHUNK_STAT_ERR;
			err_cnt++;
		}
		iput(inode);
		__unlock_super(sb);
		inode_unlock(inode);
	}

	/* Return error if all chunks are invalid */
	if (err_cnt == sb_dfr->nr_chunks - 1) {
		dfr_debug("%s failed (err_cnt %d)", __func__, err_cnt);
		return -ENXIO;
	}

	return 0;
}


/**
 * @fn		defrag_check_fs_busy
 * @brief	check if this module busy
 * @return	0 when idle, 1 otherwise
 * @param	sb				super block
 * @param	reserved_clus	# of reserved clusters
 * @param	queued_pages	# of queued pages
 */
static int
defrag_check_fs_busy(
	IN struct super_block *sb,
	OUT int *reserved_clus,
	OUT int *queued_pages)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	int err = 0;

	*reserved_clus = *queued_pages = 0;

	__lock_super(sb);
	*reserved_clus = fsi->reserved_clusters;
	*queued_pages = atomic_read(&SDFAT_SB(sb)->stat_n_pages_queued);

	if (*reserved_clus || *queued_pages)
		err = 1;
	__unlock_super(sb);

	return err;
}


/**
 * @fn		sdfat_ioctl_defrag_req
 * @brief	ioctl to send defrag requests
 * @return	0 on success, -errno otherwise
 * @param	inode		inode
 * @param	uarg		given requests
 */
static int
sdfat_ioctl_defrag_req(
	IN struct inode *inode,
	INOUT unsigned int *uarg)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct defrag_info *sb_dfr = &(sbi->dfr_info);
	struct defrag_chunk_header head;
	struct defrag_chunk_info *chunks = NULL;
	unsigned int len = 0;
	int err = 0;
	unsigned long timeout = 0;

	/* Check overlapped defrag */
	if (atomic_cmpxchg(&sb_dfr->stat, DFR_SB_STAT_IDLE, DFR_SB_STAT_REQ)) {
		dfr_debug("sb_dfr->stat %d", atomic_read(&sb_dfr->stat));
		return -EBUSY;
	}

	/* Check if defrag required */
	__lock_super(sb);
	if (!fsapi_dfr_check_dfr_required(sb, NULL, NULL, NULL)) {
		dfr_debug("Not enough space left for defrag (err %d)", -ENOSPC);
		atomic_set(&sb_dfr->stat, DFR_SB_STAT_IDLE);
		__unlock_super(sb);
		return -ENOSPC;
	}
	__unlock_super(sb);

	/* Copy args */
	memset(&head, 0, sizeof(struct defrag_chunk_header));
	err = copy_from_user(&head, uarg, sizeof(struct defrag_chunk_info));
	ERR_HANDLE(err);

	/* If FS busy, cancel defrag */
	if (!(head.mode == DFR_MODE_TEST)) {
		int reserved_clus = 0, queued_pages = 0;

		err = defrag_check_fs_busy(sb, &reserved_clus, &queued_pages);
		if (err) {
			dfr_debug("FS busy, cancel defrag (reserved_clus %d, queued_pages %d)",
					reserved_clus, queued_pages);
			err = -EBUSY;
			goto error;
		}
	}

	/* Total length is saved in the chunk header's nr_chunks field */
	len = head.nr_chunks;
	ERR_HANDLE2(!len, err, -EINVAL);

	dfr_debug("IOC_DFR_REQ started (mode %d, nr_req %d)", head.mode, len - 1);
	if (get_order(len * sizeof(struct defrag_chunk_info)) > MAX_ORDER) {
		dfr_debug("len %d, sizeof(struct defrag_chunk_info) %lu, MAX_ORDER %d",
				len, sizeof(struct defrag_chunk_info), MAX_ORDER);
		err = -EINVAL;
		goto error;
	}
	chunks = alloc_pages_exact(len * sizeof(struct defrag_chunk_info),
								GFP_KERNEL | __GFP_ZERO);
	ERR_HANDLE2(!chunks, err, -ENOMEM)

	err = copy_from_user(chunks, uarg, len * sizeof(struct defrag_chunk_info));
	ERR_HANDLE(err);

	/* Initialize sb_dfr */
	sb_dfr->chunks = chunks;
	sb_dfr->nr_chunks = len;

	/* Validate reqs & mark defrag/dirty */
	err = defrag_validate_reqs(sb, sb_dfr->chunks);
	ERR_HANDLE(err);

	atomic_set(&sb_dfr->stat, DFR_SB_STAT_VALID);

	/* Wait for defrag completion */
	if (head.mode == DFR_MODE_ONESHOT)
		timeout = 0;
	else if (head.mode & DFR_MODE_BACKGROUND)
		timeout = DFR_DEFAULT_TIMEOUT;
	else
		timeout = DFR_MIN_TIMEOUT;

	dfr_debug("Wait for completion (timeout %ld)", timeout);
	init_completion(&sbi->dfr_complete);
	timeout = wait_for_completion_timeout(&sbi->dfr_complete, timeout);

	if (!timeout) {
		/* Force defrag_updat_fat() after timeout. */
		dfr_debug("Force sync(), mode %d, left-timeout %ld", head.mode, timeout);

		down_read(&sb->s_umount);

		sync_inodes_sb(sb);

		__lock_super(sb);
		fsapi_dfr_update_fat_next(sb);

		fsapi_sync_fs(sb, 1);

#ifdef	CONFIG_SDFAT_DFR_DEBUG
		/* SPO test */
		fsapi_dfr_spo_test(sb, DFR_SPO_FAT_NEXT, __func__);
#endif

		fsapi_dfr_update_fat_prev(sb, 1);
		fsapi_sync_fs(sb, 1);

		__unlock_super(sb);

		up_read(&sb->s_umount);
	}

#ifdef	CONFIG_SDFAT_DFR_DEBUG
		/* SPO test */
		fsapi_dfr_spo_test(sb, DFR_SPO_NORMAL, __func__);
#endif

	__lock_super(sb);
	/* Send DISCARD to clean-ed AUs */
	fsapi_dfr_check_discard(sb);

#ifdef	CONFIG_SDFAT_DFR_DEBUG
	/* SPO test */
	fsapi_dfr_spo_test(sb, DFR_SPO_DISCARD, __func__);
#endif

	/* Unmark IGNORE flag to all victim AUs */
	fsapi_dfr_unmark_ignore_all(sb);
	__unlock_super(sb);

	err = copy_to_user(uarg, sb_dfr->chunks, sizeof(struct defrag_chunk_info) * len);
	ERR_HANDLE(err);

error:
	/* Clean-up sb_dfr & ino_dfr */
	defrag_cleanup_reqs(sb, err);

	if (chunks)
		free_pages_exact(chunks, len * sizeof(struct defrag_chunk_info));

	/* Set sb_dfr's state as IDLE */
	atomic_set(&sb_dfr->stat, DFR_SB_STAT_IDLE);

	dfr_debug("IOC_DFR_REQ done (err %d)", err);
	return err;
}

/**
 * @fn		sdfat_ioctl_defrag_trav
 * @brief	ioctl to traverse given directory for defrag
 * @return	0 on success, -errno otherwise
 * @param	inode		inode
 * @param	uarg		output buffer
 */
static int
sdfat_ioctl_defrag_trav(
	IN struct inode *inode,
	INOUT unsigned int *uarg)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct defrag_info *sb_dfr = &(sbi->dfr_info);
	struct defrag_trav_arg *args = (struct defrag_trav_arg *) sbi->dfr_pagep;
	struct defrag_trav_header *header = (struct defrag_trav_header *) args;
	int err = 0;

	/* Check overlapped defrag */
	if (atomic_cmpxchg(&sb_dfr->stat, DFR_SB_STAT_IDLE, DFR_SB_STAT_REQ)) {
		dfr_debug("sb_dfr->stat %d", atomic_read(&sb_dfr->stat));
		return -EBUSY;
	}

	/* Check if defrag required */
	__lock_super(sb);
	if (!fsapi_dfr_check_dfr_required(sb, NULL, NULL, NULL)) {
		dfr_debug("Not enough space left for defrag (err %d)", -ENOSPC);
		atomic_set(&sb_dfr->stat, DFR_SB_STAT_IDLE);
		__unlock_super(sb);
		return -ENOSPC;
	}
	__unlock_super(sb);

	/* Copy args */
	err = copy_from_user(args, uarg, PAGE_SIZE);
	ERR_HANDLE(err);

	/**
	 * Check args.
	 * ROOT directory has i_pos = 0 and start_clus = 0 .
	 */
	if (!(header->type & DFR_TRAV_TYPE_HEADER)) {
		err = -EINVAL;
		dfr_debug("type %d, i_pos %08llx, start_clus %08x",
				header->type, header->i_pos, header->start_clus);
		goto error;
	}

	/* If FS busy, cancel defrag */
	if (!(header->type & DFR_TRAV_TYPE_TEST)) {
		unsigned int reserved_clus = 0, queued_pages = 0;

		err = defrag_check_fs_busy(sb, &reserved_clus, &queued_pages);
		if (err) {
			dfr_debug("FS busy, cancel defrag (reserved_clus %d, queued_pages %d)",
					reserved_clus, queued_pages);
			err = -EBUSY;
			goto error;
		}
	}

	/* Scan given directory and gather info */
	inode_lock(inode);
	__lock_super(sb);
	err = fsapi_dfr_scan_dir(sb, (void *)args);
	__unlock_super(sb);
	inode_unlock(inode);
	ERR_HANDLE(err);

	/* Copy the result to user */
	err = copy_to_user(uarg, args, PAGE_SIZE);
	ERR_HANDLE(err);

error:
	memset(sbi->dfr_pagep, 0, PAGE_SIZE);

	atomic_set(&sb_dfr->stat, DFR_SB_STAT_IDLE);
	return err;
}

/**
 * @fn		sdfat_ioctl_defrag_info
 * @brief	ioctl to get HW param info
 * @return	0 on success, -errno otherwise
 * @param	sb		super block
 * @param	uarg	output buffer
 */
static int
sdfat_ioctl_defrag_info(
	IN struct super_block *sb,
	OUT unsigned int *uarg)
{
	struct defrag_info_arg info_arg;
	int err = 0;

	memset(&info_arg, 0, sizeof(struct defrag_info_arg));

	__lock_super(sb);
	err = fsapi_dfr_get_info(sb, &info_arg);
	__unlock_super(sb);
	ERR_HANDLE(err);
	dfr_debug("IOC_DFR_INFO: sec_per_au %d, hidden_sectors %d",
				info_arg.sec_per_au, info_arg.hidden_sectors);

	err = copy_to_user(uarg, &info_arg, sizeof(struct defrag_info_arg));
error:
	return err;
}

#endif	/* CONFIG_SDFAT_DFR */

static inline int __do_dfr_map_cluster(struct inode *inode, u32 clu_offset, unsigned int *clus_ptr)
{
#ifdef	CONFIG_SDFAT_DFR
	return fsapi_dfr_map_clus(inode, clu_offset, clus_ptr);
#else
	return 0;
#endif
}

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

static inline int __cancel_dfr_work(struct inode *inode, loff_t start, loff_t end, const char *fname)
{
#ifdef	CONFIG_SDFAT_DFR
	struct defrag_info *ino_dfr = &(SDFAT_I(inode)->dfr_info);
	/* Cancel DEFRAG */
	if (atomic_read(&ino_dfr->stat) == DFR_INO_STAT_REQ)
		fsapi_dfr_check_dfr_on(inode, start, end, 1, fname);
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

static inline void __init_dfr_info(struct inode *inode)
{
#ifdef	CONFIG_SDFAT_DFR
	memset(&(SDFAT_I(inode)->dfr_info), 0, sizeof(struct defrag_info));
	INIT_LIST_HEAD(&(SDFAT_I(inode)->dfr_info.entry));
	mutex_init(&(SDFAT_I(inode)->dfr_info.lock));
#endif
}

static inline int __alloc_dfr_mem_if_required(struct super_block *sb)
{
#ifdef	CONFIG_SDFAT_DFR
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	if (!sbi->options.defrag)
		return 0;

	memset(&sbi->dfr_info, 0, sizeof(struct defrag_info));
	INIT_LIST_HEAD(&(sbi->dfr_info.entry));
	mutex_init(&(sbi->dfr_info.lock));

	sbi->dfr_new_clus = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!sbi->dfr_new_clus) {
		dfr_debug("error %d", -ENOMEM);
		return -ENOMEM;
	}
	sbi->dfr_new_idx = 1;

	sbi->dfr_page_wb = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!sbi->dfr_page_wb) {
		dfr_debug("error %d", -ENOMEM);
		return -ENOMEM;
	}

	sbi->dfr_pagep = alloc_pages_exact(sizeof(struct page *) *
			PAGES_PER_AU(sb), GFP_KERNEL | __GFP_ZERO);
	if (!sbi->dfr_pagep) {
		dfr_debug("error %d", -ENOMEM);
		return -ENOMEM;
	}
#endif
	return 0;
}

static void __free_dfr_mem_if_required(struct super_block *sb)
{
#ifdef	CONFIG_SDFAT_DFR
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	if (sbi->dfr_pagep) {
		free_pages_exact(sbi->dfr_pagep, sizeof(struct page *) * PAGES_PER_AU(sb));
		sbi->dfr_pagep = NULL;
	}

	/* thanks for kfree */
	kfree(sbi->dfr_page_wb);
	sbi->dfr_page_wb = NULL;

	kfree(sbi->dfr_new_clus);
	sbi->dfr_new_clus = NULL;
#endif
}


static int sdfat_file_mmap(struct file *file, struct vm_area_struct *vm_struct)
{
	__cancel_dfr_work(file->f_mapping->host,
			(loff_t)vm_struct->vm_start,
			(loff_t)(vm_struct->vm_end - 1),
			__func__);

	return generic_file_mmap(file, vm_struct);
}

static int sdfat_ioctl_volume_id(struct inode *dir)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(dir->i_sb);
	FS_INFO_T *fsi = &(sbi->fsi);

	return fsi->vol_id;
}

static int sdfat_dfr_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long arg)
{
#ifdef	CONFIG_SDFAT_DFR
	switch (cmd) {
	case SDFAT_IOCTL_DFR_INFO: {
		struct super_block *sb = inode->i_sb;
		FS_INFO_T *fsi = &SDFAT_SB(sb)->fsi;
		unsigned int __user *uarg = (unsigned int __user *) arg;

		__lock_super(sb);
		/* Check FS type (FAT32 only) */
		if (fsi->vol_type != FAT32) {
			dfr_err("Defrag not supported, vol_type %d", fsi->vol_type);
			__unlock_super(sb);
			return -EPERM;

		}

		/* Check if SB's defrag option enabled */
		if (!(SDFAT_SB(sb)->options.defrag)) {
			dfr_err("Defrag not supported, sbi->options.defrag %d", SDFAT_SB(sb)->options.defrag);
			__unlock_super(sb);
			return -EPERM;
		}

		/* Only IOCTL on mount-point allowed */
		if (filp->f_path.mnt->mnt_root != filp->f_path.dentry) {
			dfr_err("IOC_DFR_INFO only allowed on ROOT, root %p, dentry %p",
					filp->f_path.mnt->mnt_root, filp->f_path.dentry);
			__unlock_super(sb);
			return -EPERM;
		}
		__unlock_super(sb);

		return sdfat_ioctl_defrag_info(sb, uarg);
	}
	case SDFAT_IOCTL_DFR_TRAV: {
		struct super_block *sb = inode->i_sb;
		FS_INFO_T *fsi = &SDFAT_SB(sb)->fsi;
		unsigned int __user *uarg = (unsigned int __user *) arg;

		__lock_super(sb);
		/* Check FS type (FAT32 only) */
		if (fsi->vol_type != FAT32) {
			dfr_err("Defrag not supported, vol_type %d", fsi->vol_type);
			__unlock_super(sb);
			return -EPERM;

		}

		/* Check if SB's defrag option enabled */
		if (!(SDFAT_SB(sb)->options.defrag)) {
			dfr_err("Defrag not supported, sbi->options.defrag %d", SDFAT_SB(sb)->options.defrag);
			__unlock_super(sb);
			return -EPERM;
		}
		__unlock_super(sb);

		return sdfat_ioctl_defrag_trav(inode, uarg);
	}
	case SDFAT_IOCTL_DFR_REQ: {
		struct super_block *sb = inode->i_sb;
		FS_INFO_T *fsi = &SDFAT_SB(sb)->fsi;
		unsigned int __user *uarg = (unsigned int __user *) arg;

		__lock_super(sb);

		/* Check if FS_ERROR occurred */
		if (sb->s_flags & MS_RDONLY) {
			dfr_err("RDONLY partition (err %d)", -EPERM);
			__unlock_super(sb);
			return -EPERM;
		}

		/* Check FS type (FAT32 only) */
		if (fsi->vol_type != FAT32) {
			dfr_err("Defrag not supported, vol_type %d", fsi->vol_type);
			__unlock_super(sb);
			return -EINVAL;

		}

		/* Check if SB's defrag option enabled */
		if (!(SDFAT_SB(sb)->options.defrag)) {
			dfr_err("Defrag not supported, sbi->options.defrag %d", SDFAT_SB(sb)->options.defrag);
			__unlock_super(sb);
			return -EPERM;
		}

		/* Only IOCTL on mount-point allowed */
		if (filp->f_path.mnt->mnt_root != filp->f_path.dentry) {
			dfr_err("IOC_DFR_INFO only allowed on ROOT, root %p, dentry %p",
					filp->f_path.mnt->mnt_root, filp->f_path.dentry);
			__unlock_super(sb);
			return -EINVAL;
		}
		__unlock_super(sb);

		return sdfat_ioctl_defrag_req(inode, uarg);
	}
#ifdef	CONFIG_SDFAT_DFR_DEBUG
	case SDFAT_IOCTL_DFR_SPO_FLAG: {
		struct sdfat_sb_info *sbi = SDFAT_SB(inode->i_sb);
		int ret = 0;

		ret = get_user(sbi->dfr_spo_flag, (int __user *)arg);
		dfr_debug("dfr_spo_flag %d", sbi->dfr_spo_flag);

		return ret;
	}
#endif	/* CONFIG_SDFAT_DFR_DEBUG */
	}
#endif /* CONFIG_SDFAT_DFR */

	/* Inappropriate ioctl for device */
	return -ENOTTY;
}

static int sdfat_dbg_ioctl(struct inode *inode, struct file *filp,
					unsigned int cmd, unsigned long arg)
{
#ifdef CONFIG_SDFAT_DBG_IOCTL
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	unsigned int flags;

	switch (cmd) {
	case SDFAT_IOC_GET_DEBUGFLAGS:
		flags = sbi->debug_flags;
		return put_user(flags, (int __user *)arg);
	case SDFAT_IOC_SET_DEBUGFLAGS:
		flags = 0;
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;

		if (get_user(flags, (int __user *) arg))
			return -EFAULT;

		__lock_super(sb);
		sbi->debug_flags = flags;
		__unlock_super(sb);
		return 0;
	case SDFAT_IOCTL_PANIC:
		panic("ioctl panic for test");

		/* COULD NOT REACH HEAR */
		return 0;
	}
#endif /* CONFIG_SDFAT_DBG_IOCTL */
	return -ENOTTY;
}

static long sdfat_generic_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(filp);
	int err;

	if (cmd == SDFAT_IOCTL_GET_VOLUME_ID)
		return sdfat_ioctl_volume_id(inode);

	err = sdfat_dfr_ioctl(inode, filp, cmd, arg);
	if (err != -ENOTTY)
		return err;

	/* -ENOTTY if inappropriate ioctl for device */
	return sdfat_dbg_ioctl(inode, filp, cmd, arg);
}

static int __sdfat_getattr(struct inode *inode, struct kstat *stat)
{
	TMSG("%s entered\n", __func__);

	generic_fillattr(inode, stat);
	stat->blksize = SDFAT_SB(inode->i_sb)->fsi.cluster_size;

	TMSG("%s exited\n", __func__);
	return 0;
}

static void __sdfat_writepage_end_io(struct bio *bio, int err)
{
	struct page *page = bio->bi_io_vec->bv_page;
	struct super_block *sb = page->mapping->host->i_sb;

	ASSERT(bio->bi_vcnt == 1); /* Single page endio */
	ASSERT(bio_data_dir(bio)); /* Write */

	if (err) {
		SetPageError(page);
		mapping_set_error(page->mapping, err);
	}

	__dfr_writepage_end_io(page);

#ifdef CONFIG_SDFAT_TRACE_IO
	{
		//struct sdfat_sb_info *sbi = SDFAT_SB(bio->bi_bdev->bd_super);
		struct sdfat_sb_info *sbi = SDFAT_SB(sb);

		sbi->stat_n_pages_written++;
		if (page->mapping->host == sb->s_bdev->bd_inode)
			sbi->stat_n_bdev_pages_written++;

		/* 4 MB = 1024 pages => 0.4 sec (approx.)
		 * 32 KB =  64 pages => 0.025 sec
		 * Min. average latency b/w msgs. ~= 0.025 sec
		 */
		if ((sbi->stat_n_pages_written & 63) == 0) {
			DMSG("STAT:%u, %u, %u, %u (Sector #: %u)\n",
			sbi->stat_n_pages_added, sbi->stat_n_pages_written,
			sbi->stat_n_bdev_pages_witten,
			sbi->stat_n_pages_confused,
			(unsigned int)__sdfat_bio_sector(bio));
		}
	}
#endif
	end_page_writeback(page);
	bio_put(bio);

	// Update trace info.
	atomic_dec(&SDFAT_SB(sb)->stat_n_pages_queued);
}


static int __support_write_inode_sync(struct super_block *sb)
{
#ifdef CONFIG_SDFAT_SUPPORT_DIR_SYNC
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	if (sbi->fsi.vol_type != EXFAT)
		return 0;
#endif
	return 1;
#endif
	return 0;
}


static int __sdfat_file_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = filp->f_mapping->host;
	struct super_block *sb = inode->i_sb;
	int res, err = 0;

	res = __sdfat_generic_file_fsync(filp, start, end, datasync);

	if (!__support_write_inode_sync(sb))
		err = fsapi_sync_fs(sb, 1);

	return res ? res : err;
}


static const struct file_operations sdfat_dir_operations = {
	.llseek     = generic_file_llseek,
	.read       = generic_read_dir,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
	.iterate    = sdfat_iterate,
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0) */
	.readdir    = sdfat_readdir,
#endif
	.fsync      = sdfat_file_fsync,
	.unlocked_ioctl = sdfat_generic_ioctl,
};

static int __sdfat_create(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	struct timespec ts;
	FILE_ID_T fid;
	loff_t i_pos;
	int err;

	__lock_super(sb);

	TMSG("%s entered\n", __func__);

	ts = CURRENT_TIME_SEC;

	err = fsapi_create(dir, (u8 *) dentry->d_name.name, FM_REGULAR, &fid);
	if (err)
		goto out;

	__lock_d_revalidate(dentry);

	dir->i_version++;
	dir->i_ctime = dir->i_mtime = dir->i_atime = ts;
	if (IS_DIRSYNC(dir))
		(void) sdfat_sync_inode(dir);
	else
		mark_inode_dirty(dir);

	i_pos = sdfat_make_i_pos(&fid);
	inode = sdfat_build_inode(sb, &fid, i_pos);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out;
	}
	inode->i_version++;
	inode->i_mtime = inode->i_atime = inode->i_ctime = ts;
	/* timestamp is already written, so mark_inode_dirty() is unneeded. */

	d_instantiate(dentry, inode);
out:
	__unlock_d_revalidate(dentry);
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	if (!err)
		sdfat_statistics_set_create(fid.flags);
	return err;
}


static int sdfat_find(struct inode *dir, struct qstr *qname, FILE_ID_T *fid)
{
	int err;

	if (qname->len == 0)
		return -ENOENT;

	err = fsapi_lookup(dir, (u8 *) qname->name, fid);
	if (err)
		return -ENOENT;

	return 0;
}

static int sdfat_d_anon_disconn(struct dentry *dentry)
{
	return IS_ROOT(dentry) && (dentry->d_flags & DCACHE_DISCONNECTED);
}

static struct dentry *__sdfat_lookup(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	struct dentry *alias;
	int err;
	FILE_ID_T fid;
	loff_t i_pos;
	u64 ret;
	mode_t i_mode;

	__lock_super(sb);
	TMSG("%s entered\n", __func__);
	err = sdfat_find(dir, &dentry->d_name, &fid);
	if (err) {
		if (err == -ENOENT) {
			inode = NULL;
			goto out;
		}
		goto error;
	}

	i_pos = sdfat_make_i_pos(&fid);
	inode = sdfat_build_inode(sb, &fid, i_pos);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto error;
	}

	i_mode = inode->i_mode;
	if (S_ISLNK(i_mode) && !SDFAT_I(inode)->target) {
		SDFAT_I(inode)->target = kmalloc((i_size_read(inode)+1), GFP_KERNEL);
		if (!SDFAT_I(inode)->target) {
			err = -ENOMEM;
			goto error;
		}
		fsapi_read_link(dir, &fid, SDFAT_I(inode)->target, i_size_read(inode), &ret);
		*(SDFAT_I(inode)->target + i_size_read(inode)) = '\0';
	}

	alias = d_find_alias(inode);

	/*
	 * Checking "alias->d_parent == dentry->d_parent" to make sure
	 * FS is not corrupted (especially double linked dir).
	 */
	if (alias && alias->d_parent == dentry->d_parent &&
	    !sdfat_d_anon_disconn(alias)) {

		/*
		 * Unhashed alias is able to exist because of revalidate()
		 * called by lookup_fast. You can easily make this status
		 * by calling create and lookup concurrently
		 * In such case, we reuse an alias instead of new dentry
		 */
		if (d_unhashed(alias)) {
			BUG_ON(alias->d_name.hash_len != dentry->d_name.hash_len);
			sdfat_msg(sb, KERN_INFO, "rehashed a dentry(%p) "
				"in read lookup", alias);
			d_drop(dentry);
			d_rehash(alias);
		} else if (!S_ISDIR(i_mode)) {
			/*
			 * This inode has non anonymous-DCACHE_DISCONNECTED
			 * dentry. This means, the user did ->lookup() by an
			 * another name (longname vs 8.3 alias of it) in past.
			 *
			 * Switch to new one for reason of locality if possible.
			 */
			d_move(alias, dentry);
		}
		iput(inode);
		__unlock_super(sb);
		TMSG("%s exited\n", __func__);
		return alias;
	}
	dput(alias);
out:
	/* initialize d_time even though it is positive dentry */
	dentry->d_time = dir->i_version;
	__unlock_super(sb);

	dentry = d_splice_alias(inode, dentry);

	TMSG("%s exited\n", __func__);
	return dentry;
error:
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	return ERR_PTR(err);
}


static int sdfat_unlink(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = dir->i_sb;
	struct timespec ts;
	int err;

	__lock_super(sb);

	TMSG("%s entered\n", __func__);

	ts = CURRENT_TIME_SEC;

	SDFAT_I(inode)->fid.size = i_size_read(inode);

	__cancel_dfr_work(inode, 0, SDFAT_I(inode)->fid.size, __func__);

	err = fsapi_unlink(dir, &(SDFAT_I(inode)->fid));
	if (err)
		goto out;

	__lock_d_revalidate(dentry);

	dir->i_version++;
	dir->i_mtime = dir->i_atime = ts;
	if (IS_DIRSYNC(dir))
		(void) sdfat_sync_inode(dir);
	else
		mark_inode_dirty(dir);

	clear_nlink(inode);
	inode->i_mtime = inode->i_atime = ts;
	sdfat_detach(inode);
	dentry->d_time = dir->i_version;
out:
	__unlock_d_revalidate(dentry);
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	return err;
}

static int sdfat_symlink(struct inode *dir, struct dentry *dentry, const char *target)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	struct timespec ts;
	FILE_ID_T fid;
	loff_t i_pos;
	int err;
	u64 len = (u64) strlen(target);
	u64 ret;

	/* symlink option check */
	if (!SDFAT_SB(sb)->options.symlink)
		return -ENOTSUPP;

	__lock_super(sb);

	TMSG("%s entered\n", __func__);

	ts = CURRENT_TIME_SEC;

	err = fsapi_create(dir, (u8 *) dentry->d_name.name, FM_SYMLINK, &fid);
	if (err)
		goto out;

	err = fsapi_write_link(dir, &fid, (char *) target, len, &ret);

	if (err) {
		fsapi_remove(dir, &fid);
		goto out;
	}

	__lock_d_revalidate(dentry);

	dir->i_version++;
	dir->i_ctime = dir->i_mtime = dir->i_atime = ts;
	if (IS_DIRSYNC(dir))
		(void) sdfat_sync_inode(dir);
	else
		mark_inode_dirty(dir);

	i_pos = sdfat_make_i_pos(&fid);
	inode = sdfat_build_inode(sb, &fid, i_pos);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out;
	}
	inode->i_version++;
	inode->i_mtime = inode->i_atime = inode->i_ctime = ts;
	/* timestamp is already written, so mark_inode_dirty() is unneeded. */

	SDFAT_I(inode)->target = kmalloc((len+1), GFP_KERNEL);
	if (!SDFAT_I(inode)->target) {
		err = -ENOMEM;
		goto out;
	}
	memcpy(SDFAT_I(inode)->target, target, len+1);

	d_instantiate(dentry, inode);
out:
	__unlock_d_revalidate(dentry);
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	return err;
}


static int __sdfat_mkdir(struct inode *dir, struct dentry *dentry)
{
	struct super_block *sb = dir->i_sb;
	struct inode *inode;
	struct timespec ts;
	FILE_ID_T fid;
	loff_t i_pos;
	int err;

	__lock_super(sb);

	TMSG("%s entered\n", __func__);

	ts = CURRENT_TIME_SEC;

	err = fsapi_mkdir(dir, (u8 *) dentry->d_name.name, &fid);
	if (err)
		goto out;

	__lock_d_revalidate(dentry);

	dir->i_version++;
	dir->i_ctime = dir->i_mtime = dir->i_atime = ts;
	if (IS_DIRSYNC(dir))
		(void) sdfat_sync_inode(dir);
	else
		mark_inode_dirty(dir);
	inc_nlink(dir);

	i_pos = sdfat_make_i_pos(&fid);
	inode = sdfat_build_inode(sb, &fid, i_pos);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out;
	}
	inode->i_version++;
	inode->i_mtime = inode->i_atime = inode->i_ctime = ts;
	/* timestamp is already written, so mark_inode_dirty() is unneeded. */

	d_instantiate(dentry, inode);

out:
	__unlock_d_revalidate(dentry);
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	if (!err)
		sdfat_statistics_set_mkdir(fid.flags);
	return err;
}


static int sdfat_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	struct super_block *sb = dir->i_sb;
	struct timespec ts;
	int err;

	__lock_super(sb);

	TMSG("%s entered\n", __func__);

	ts = CURRENT_TIME_SEC;

	SDFAT_I(inode)->fid.size = i_size_read(inode);

	err = fsapi_rmdir(dir, &(SDFAT_I(inode)->fid));
	if (err)
		goto out;

	__lock_d_revalidate(dentry);

	dir->i_version++;
	dir->i_mtime = dir->i_atime = ts;
	if (IS_DIRSYNC(dir))
		(void) sdfat_sync_inode(dir);
	else
		mark_inode_dirty(dir);
	drop_nlink(dir);

	clear_nlink(inode);
	inode->i_mtime = inode->i_atime = ts;
	sdfat_detach(inode);
	dentry->d_time = dir->i_version;
out:
	__unlock_d_revalidate(dentry);
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	return err;
}

static int __sdfat_rename(struct inode *old_dir, struct dentry *old_dentry,
		struct inode *new_dir, struct dentry *new_dentry)
{
	struct inode *old_inode, *new_inode;
	struct super_block *sb = old_dir->i_sb;
	struct timespec ts;
	loff_t i_pos;
	int err;

	__lock_super(sb);

	TMSG("%s entered\n", __func__);

	old_inode = old_dentry->d_inode;
	new_inode = new_dentry->d_inode;

	ts = CURRENT_TIME_SEC;

	SDFAT_I(old_inode)->fid.size = i_size_read(old_inode);

	__cancel_dfr_work(old_inode, 0, 1, __func__);

	err = fsapi_rename(old_dir, &(SDFAT_I(old_inode)->fid), new_dir, new_dentry);
	if (err)
		goto out;

	__lock_d_revalidate(old_dentry);
	__lock_d_revalidate(new_dentry);

	new_dir->i_version++;
	new_dir->i_ctime = new_dir->i_mtime = new_dir->i_atime = ts;
	if (IS_DIRSYNC(new_dir))
		(void) sdfat_sync_inode(new_dir);
	else
		mark_inode_dirty(new_dir);

	i_pos = sdfat_make_i_pos(&(SDFAT_I(old_inode)->fid));
	sdfat_detach(old_inode);
	sdfat_attach(old_inode, i_pos);
	if (IS_DIRSYNC(new_dir))
		(void) sdfat_sync_inode(old_inode);
	else
		mark_inode_dirty(old_inode);

	if ((S_ISDIR(old_inode->i_mode)) && (old_dir != new_dir)) {
		drop_nlink(old_dir);
		if (!new_inode)
			inc_nlink(new_dir);
	}

	old_dir->i_version++;
	old_dir->i_ctime = old_dir->i_mtime = ts;
	if (IS_DIRSYNC(old_dir))
		(void) sdfat_sync_inode(old_dir);
	else
		mark_inode_dirty(old_dir);

	if (new_inode) {
		sdfat_detach(new_inode);

		/* skip drop_nlink if new_inode already has been dropped */
		if (new_inode->i_nlink) {
			drop_nlink(new_inode);
			if (S_ISDIR(new_inode->i_mode))
				drop_nlink(new_inode);
		} else {
			EMSG("%s : abnormal access to an inode dropped\n",
				__func__);
			WARN_ON(new_inode->i_nlink == 0);
		}
		new_inode->i_ctime = ts;
#if 0
		(void) sdfat_sync_inode(new_inode);
#endif
	}

out:
	__unlock_d_revalidate(old_dentry);
	__unlock_d_revalidate(new_dentry);
	__unlock_super(sb);
	TMSG("%s exited with err(%d)\n", __func__, err);
	return err;
}

static int sdfat_cont_expand(struct inode *inode, loff_t size)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t start = i_size_read(inode), count = size - i_size_read(inode);
	int err, err2;

	err = generic_cont_expand_simple(inode, size);
	if (err)
		return err;

	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	mark_inode_dirty(inode);

	if (!IS_SYNC(inode))
		return 0;

	err = filemap_fdatawrite_range(mapping, start, start + count - 1);
	err2 = sync_mapping_buffers(mapping);
	err = (err)?(err):(err2);
	err2 = write_inode_now(inode, 1);
	err = (err)?(err):(err2);
	if (err)
		return err;

	return filemap_fdatawait_range(mapping, start, start + count - 1);
}

static int sdfat_allow_set_time(struct sdfat_sb_info *sbi, struct inode *inode)
{
	mode_t allow_utime = sbi->options.allow_utime;

	if (!uid_eq(current_fsuid(), inode->i_uid)) {
		if (in_group_p(inode->i_gid))
			allow_utime >>= 3;
		if (allow_utime & MAY_WRITE)
			return 1;
	}

	/* use a default check */
	return 0;
}

static int sdfat_sanitize_mode(const struct sdfat_sb_info *sbi,
				   struct inode *inode, umode_t *mode_ptr)
{
	mode_t i_mode, mask, perm;

	i_mode = inode->i_mode;

	if (S_ISREG(i_mode) || S_ISLNK(i_mode))
		mask = sbi->options.fs_fmask;
	else
		mask = sbi->options.fs_dmask;

	perm = *mode_ptr & ~(S_IFMT | mask);

	/* Of the r and x bits, all (subject to umask) must be present.*/
	if ((perm & (S_IRUGO | S_IXUGO)) != (i_mode & (S_IRUGO | S_IXUGO)))
		return -EPERM;

	if (sdfat_mode_can_hold_ro(inode)) {
		/* Of the w bits, either all (subject to umask) or none must be present. */
		if ((perm & S_IWUGO) && ((perm & S_IWUGO) != (S_IWUGO & ~mask)))
			return -EPERM;
	} else {
		/* If sdfat_mode_can_hold_ro(inode) is false, can't change w bits. */
		if ((perm & S_IWUGO) != (S_IWUGO & ~mask))
			return -EPERM;
	}

	*mode_ptr &= S_IFMT | perm;

	return 0;
}

static int sdfat_setattr(struct dentry *dentry, struct iattr *attr)
{

	struct sdfat_sb_info *sbi = SDFAT_SB(dentry->d_sb);
	struct inode *inode = dentry->d_inode;
	unsigned int ia_valid;
	int error;
	loff_t old_size;

	TMSG("%s entered\n", __func__);

	if ((attr->ia_valid & ATTR_SIZE)
		&& (attr->ia_size > i_size_read(inode))) {
		error = sdfat_cont_expand(inode, attr->ia_size);
		if (error || attr->ia_valid == ATTR_SIZE)
			return error;
		attr->ia_valid &= ~ATTR_SIZE;
	}

	/* Check for setting the inode time. */
	ia_valid = attr->ia_valid;
	if ((ia_valid & (ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET))
		&& sdfat_allow_set_time(sbi, inode)) {
		attr->ia_valid &= ~(ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET);
	}

	error = setattr_prepare(dentry, attr);
	attr->ia_valid = ia_valid;
	if (error)
		return error;

	if (((attr->ia_valid & ATTR_UID) &&
		 (!uid_eq(attr->ia_uid, sbi->options.fs_uid))) ||
		((attr->ia_valid & ATTR_GID) &&
		 (!gid_eq(attr->ia_gid, sbi->options.fs_gid))) ||
		((attr->ia_valid & ATTR_MODE) &&
		 (attr->ia_mode & ~(S_IFREG | S_IFLNK | S_IFDIR | S_IRWXUGO)))) {
		return -EPERM;
	}

	/*
	 * We don't return -EPERM here. Yes, strange, but this is too
	 * old behavior.
	 */
	if (attr->ia_valid & ATTR_MODE) {
		if (sdfat_sanitize_mode(sbi, inode, &attr->ia_mode) < 0)
			attr->ia_valid &= ~ATTR_MODE;
	}

	SDFAT_I(inode)->fid.size = i_size_read(inode);

	/* patch 1.2.0 : fixed the problem of size mismatch. */
	if (attr->ia_valid & ATTR_SIZE) {
		old_size = i_size_read(inode);

		/* TO CHECK evicting directory works correctly */
		MMSG("%s: inode(%p) truncate size (%llu->%llu)\n", __func__,
			inode, (u64)old_size, (u64)attr->ia_size);
		__sdfat_do_truncate(inode, old_size, attr->ia_size);
	}
	setattr_copy(inode, attr);
	mark_inode_dirty(inode);


	TMSG("%s exited with err(%d)\n", __func__, error);
	return error;
}

static const struct inode_operations sdfat_dir_inode_operations = {
	.create        = sdfat_create,
	.lookup        = sdfat_lookup,
	.unlink        = sdfat_unlink,
	.symlink       = sdfat_symlink,
	.mkdir         = sdfat_mkdir,
	.rmdir         = sdfat_rmdir,
	.rename        = sdfat_rename,
	.setattr       = sdfat_setattr,
	.getattr       = sdfat_getattr,
#ifdef CONFIG_SDFAT_VIRTUAL_XATTR
	.listxattr      = sdfat_listxattr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	.setxattr       = sdfat_setxattr,
	.getxattr       = sdfat_getxattr,
	.removexattr    = sdfat_removexattr,
#endif
#endif
};

/*======================================================================*/
/*  File Operations                                                     */
/*======================================================================*/
static const struct inode_operations sdfat_symlink_inode_operations = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	.readlink    = generic_readlink,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	.get_link = sdfat_follow_link,
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0) */
	.follow_link = sdfat_follow_link,
#endif
#ifdef CONFIG_SDFAT_VIRTUAL_XATTR
	.listxattr      = sdfat_listxattr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	.setxattr       = sdfat_setxattr,
	.getxattr       = sdfat_getxattr,
	.removexattr    = sdfat_removexattr,
#endif
#endif
};

static int sdfat_file_release(struct inode *inode, struct file *filp)
{
	struct super_block *sb = inode->i_sb;

	/* Moved below code from sdfat_write_inode
	 * TO FIX size-mismatch problem.
	 */
	/* FIXME : Added bug_on to confirm that there is no size mismatch */
	sdfat_debug_bug_on(SDFAT_I(inode)->fid.size != i_size_read(inode));
	SDFAT_I(inode)->fid.size = i_size_read(inode);
	fsapi_sync_fs(sb, 0);
	return 0;
}

static const struct file_operations sdfat_file_operations = {
	.llseek      = generic_file_llseek,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	.read_iter   = generic_file_read_iter,
	.write_iter  = generic_file_write_iter,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	.read        = new_sync_read,
	.write       = new_sync_write,
	.read_iter   = generic_file_read_iter,
	.write_iter  = generic_file_write_iter,
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0) */
	.read        = do_sync_read,
	.write       = do_sync_write,
	.aio_read    = generic_file_aio_read,
	.aio_write   = generic_file_aio_write,
#endif
	.mmap        = sdfat_file_mmap,
	.release     = sdfat_file_release,
	.unlocked_ioctl  = sdfat_generic_ioctl,
	.fsync       = sdfat_file_fsync,
	.splice_read = generic_file_splice_read,
};

static const struct address_space_operations sdfat_da_aops;
static const struct address_space_operations sdfat_aops;

static void sdfat_truncate(struct inode *inode, loff_t old_size)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	unsigned int blocksize = 1 << inode->i_blkbits;
	loff_t aligned_size;
	int err;

	__lock_super(sb);

	if (SDFAT_I(inode)->fid.start_clu == 0) {
		/* Stange statement:
		 * Empty start_clu != ~0 (not allocated)
		 */
		sdfat_fs_error(sb, "tried to truncate zeroed cluster.");
		goto out;
	}

	sdfat_debug_check_clusters(inode);

	__cancel_dfr_work(inode, (loff_t)i_size_read(inode), (loff_t)old_size, __func__);

	err = fsapi_truncate(inode, old_size, i_size_read(inode));
	if (err)
		goto out;

	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
	if (IS_DIRSYNC(inode))
		(void) sdfat_sync_inode(inode);
	else
		mark_inode_dirty(inode);

	// FIXME: 확인 요망
	// inode->i_blocks = ((SDFAT_I(inode)->i_size_ondisk + (fsi->cluster_size - 1))
	inode->i_blocks = ((i_size_read(inode) + (fsi->cluster_size - 1)) &
			~((loff_t)fsi->cluster_size - 1)) >> inode->i_blkbits;
out:
	/*
	 * This protects against truncating a file bigger than it was then
	 * trying to write into the hole.
	 *
	 * comment by sh.hong:
	 * This seems to mean 'intra page/block' truncate and writing.
	 * I couldn't find a reason to change the values prior to fsapi_truncate
	 * Therefore, I switched the order of operations
	 * so that it's possible to utilize i_size_ondisk in fsapi_truncate
	 */

	aligned_size = i_size_read(inode);
	if (aligned_size & (blocksize - 1)) {
		aligned_size |= (blocksize - 1);
		aligned_size++;
	}

	if (SDFAT_I(inode)->i_size_ondisk > i_size_read(inode))
		SDFAT_I(inode)->i_size_ondisk = aligned_size;

	sdfat_debug_check_clusters(inode);

	if (SDFAT_I(inode)->i_size_aligned > i_size_read(inode))
		SDFAT_I(inode)->i_size_aligned = aligned_size;

	/* After truncation :
	 * 1) Delayed allocation is OFF
	 *    i_size = i_size_ondisk <= i_size_aligned
	 *    (useless size var.)
	 *    (block-aligned)
	 * 2) Delayed allocation is ON
	 *    i_size = i_size_ondisk = i_size_aligned
	 *    (will be block-aligned after write)
	 *    or
	 *    i_size_ondisk < i_size <= i_size_aligned (block_aligned)
	 *    (will be block-aligned after write)
	 */

	__unlock_super(sb);
}

static const struct inode_operations sdfat_file_inode_operations = {
	.setattr     = sdfat_setattr,
	.getattr     = sdfat_getattr,
#ifdef CONFIG_SDFAT_VIRTUAL_XATTR
	.listxattr      = sdfat_listxattr,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
	.setxattr       = sdfat_setxattr,
	.getxattr       = sdfat_getxattr,
	.removexattr    = sdfat_removexattr,
#endif
#endif
};

/*======================================================================*/
/*  Address Space Operations                                            */
/*======================================================================*/
/* 2-level option flag */
#define BMAP_NOT_CREATE				0
#define BMAP_ADD_BLOCK				1
#define BMAP_ADD_CLUSTER			2
#define BLOCK_ADDED(bmap_ops)	(bmap_ops)
static int sdfat_bmap(struct inode *inode, sector_t sector, sector_t *phys,
				  unsigned long *mapped_blocks, int *create)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	const unsigned long blocksize = sb->s_blocksize;
	const unsigned char blocksize_bits = sb->s_blocksize_bits;
	sector_t last_block;
	unsigned int cluster, clu_offset, sec_offset;
	int err = 0;

	*phys = 0;
	*mapped_blocks = 0;

	/* core code should handle EIO */
#if 0
	if (fsi->prev_eio && BLOCK_ADDED(*create))
		return -EIO;
#endif

	if (((fsi->vol_type == FAT12) || (fsi->vol_type == FAT16)) &&
					(inode->i_ino == SDFAT_ROOT_INO)) {
		if (sector < (fsi->dentries_in_root >>
				(sb->s_blocksize_bits - DENTRY_SIZE_BITS))) {
			*phys = sector + fsi->root_start_sector;
			*mapped_blocks = 1;
		}
		return 0;
	}

	last_block = (i_size_read(inode) + (blocksize - 1)) >> blocksize_bits;
	if ((sector >= last_block) && (*create == BMAP_NOT_CREATE))
		return 0;

	/* Is this block already allocated? */
	clu_offset = sector >> fsi->sect_per_clus_bits;  /* cluster offset */

	SDFAT_I(inode)->fid.size = i_size_read(inode);


	if (unlikely(__check_dfr_on(inode,
		(loff_t)((loff_t)clu_offset << fsi->cluster_size_bits),
		(loff_t)((loff_t)(clu_offset + 1) << fsi->cluster_size_bits),
			__func__))) {
		err = __do_dfr_map_cluster(inode, clu_offset, &cluster);
	} else {
		if (*create & BMAP_ADD_CLUSTER)
			err = fsapi_map_clus(inode, clu_offset, &cluster, 1);
		else
			err = fsapi_map_clus(inode, clu_offset, &cluster, ALLOC_NOWHERE);
	}

	if (err) {
		if (err != -ENOSPC)
			return -EIO;
		return err;
	}

	/* FOR BIGDATA */
	sdfat_statistics_set_rw(SDFAT_I(inode)->fid.flags,
				clu_offset, *create & BMAP_ADD_CLUSTER);

	if (!IS_CLUS_EOF(cluster)) {
		/* sector offset in cluster */
		sec_offset = sector & (fsi->sect_per_clus - 1);

		*phys = CLUS_TO_SECT(fsi, cluster) + sec_offset;
		*mapped_blocks = fsi->sect_per_clus - sec_offset;
	}
#if 0
	else {
		/* Debug purpose (new clu needed) */
		ASSERT((*create & BMAP_ADD_CLUSTER) == 0);
		ASSERT(sector >= last_block);
	}
#endif

	if (sector < last_block)
		*create = BMAP_NOT_CREATE;
#if 0
	else if (sector >= last_block)
		*create = non-zero;

	if (iblock <= last mapped-block)
		*phys != 0
		*create = BMAP_NOT_CREATE
	else if (iblock <= last cluster)
		*phys != 0
		*create = non-zero
#endif
	return 0;
}

static int sdfat_da_prep_block(struct inode *inode, sector_t iblock,
				struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	unsigned long max_blocks = bh_result->b_size >> inode->i_blkbits;
	unsigned long mapped_blocks;
	sector_t phys;
	loff_t pos;
	int sec_offset;
	int bmap_create = create ? BMAP_ADD_BLOCK : BMAP_NOT_CREATE;
	int err = 0;

	__lock_super(sb);

	/* FAT32 only */
	ASSERT(fsi->vol_type == FAT32);

	err = sdfat_bmap(inode, iblock, &phys, &mapped_blocks, &bmap_create);
	if (err) {
		if (err != -ENOSPC)
			sdfat_fs_error_ratelimit(sb, "%s: failed to bmap "
					"(iblock:%u, err:%d)", __func__,
					(u32)iblock, err);
		goto unlock_ret;
	}

	sec_offset = iblock & (fsi->sect_per_clus - 1);

	if (phys) {
		/* the block in in the mapped cluster boundary */
		max_blocks = min(mapped_blocks, max_blocks);
		map_bh(bh_result, sb, phys);

		BUG_ON(BLOCK_ADDED(bmap_create) && (sec_offset == 0));

	} else if (create == 1) {
		/* Not exist: new cluster needed */
		if (!BLOCK_ADDED(bmap_create)) {
			sector_t last_block;
			last_block = (i_size_read(inode) + (sb->s_blocksize - 1))
						>> sb->s_blocksize_bits;
			sdfat_fs_error(sb, "%s: new cluster need, but "
				"bmap_create == BMAP_NOT_CREATE(iblock:%lld, "
				"last_block:%lld)", __func__,
				(s64)iblock, (s64)last_block);
			err = -EIO;
			goto unlock_ret;
		}

		// Reserved Cluster (only if iblock is the first sector in a clu)
		if (sec_offset == 0) {
			err = fsapi_reserve_clus(inode);
			if (err) {
				if (err != -ENOSPC)
					sdfat_fs_error_ratelimit(sb,
						"%s: failed to bmap "
						"(iblock:%u, err:%d)", __func__,
						(u32)iblock, err);

				goto unlock_ret;
			}
		}

		// Delayed mapping
		map_bh(bh_result, sb, ~((sector_t) 0xffff));
		set_buffer_new(bh_result);
		set_buffer_delay(bh_result);

	} else {
		/* get_block on non-existing addr. with create==0 */
		/*
		 * CHECKME:
		 * i_size_aligned 보다 작으면 delay 매핑을 일단
		 * 켜줘야되는 게 아닌가?
		 * - 0-fill 을 항상 하기에, FAT 에서는 문제 없음.
		 *   중간에 영역이 꽉 찼으면, 디스크에 내려가지 않고는
		 *   invalidate 될 일이 없음
		 */
		goto unlock_ret;
	}


	/* Newly added blocks */
	if (BLOCK_ADDED(bmap_create)) {
		set_buffer_new(bh_result);

		SDFAT_I(inode)->i_size_aligned += max_blocks << sb->s_blocksize_bits;
		if (phys) {
			/* i_size_ondisk changes if a block added in the existing cluster */
			#define num_clusters(value) ((value) ? (s32)((value - 1) >> fsi->cluster_size_bits) + 1 : 0)

			/* FOR GRACEFUL ERROR HANDLING */
			if (num_clusters(SDFAT_I(inode)->i_size_aligned) !=
				num_clusters(SDFAT_I(inode)->i_size_ondisk)) {
				EMSG("%s: inode(%p) invalid size (create(%d) "
				"bmap_create(%d) phys(%lld) aligned(%lld) "
				"on_disk(%lld) iblock(%u) sec_off(%d))\n",
				__func__, inode, create, bmap_create, (s64)phys,
				(s64)SDFAT_I(inode)->i_size_aligned,
				(s64)SDFAT_I(inode)->i_size_ondisk,
				(u32)iblock,
				(s32)sec_offset);
				sdfat_debug_bug_on(1);
			}
			SDFAT_I(inode)->i_size_ondisk = SDFAT_I(inode)->i_size_aligned;
		}

		pos = (iblock + 1) << sb->s_blocksize_bits;
		/* Debug purpose - defensive coding */
		ASSERT(SDFAT_I(inode)->i_size_aligned == pos);
		if (SDFAT_I(inode)->i_size_aligned < pos)
			SDFAT_I(inode)->i_size_aligned = pos;
		/* Debug end */

#ifdef CONFIG_SDFAT_TRACE_IO
		/* New page added (ASSERTION: 8 blocks per page) */
		if ((sec_offset & 7) == 0)
			sbi->stat_n_pages_added++;
#endif
	}

	/* FOR GRACEFUL ERROR HANDLING */
	if (i_size_read(inode) > SDFAT_I(inode)->i_size_aligned) {
		sdfat_fs_error_ratelimit(sb, "%s: invalid size (inode(%p), "
			"size(%llu) > aligned(%llu)\n", __func__, inode,
			i_size_read(inode), SDFAT_I(inode)->i_size_aligned);
		sdfat_debug_bug_on(1);
	}

	bh_result->b_size = max_blocks << sb->s_blocksize_bits;

unlock_ret:
	__unlock_super(sb);
	return err;
}

static int sdfat_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	unsigned long max_blocks = bh_result->b_size >> inode->i_blkbits;
	int err = 0;
	unsigned long mapped_blocks;
	sector_t phys;
	loff_t pos;
	int bmap_create = create ? BMAP_ADD_CLUSTER : BMAP_NOT_CREATE;

	__lock_super(sb);
	err = sdfat_bmap(inode, iblock, &phys, &mapped_blocks, &bmap_create);
	if (err) {
		if (err != -ENOSPC)
			sdfat_fs_error_ratelimit(sb, "%s: failed to bmap "
					"(inode:%p iblock:%u, err:%d)",
					__func__, inode, (u32)iblock, err);
		goto unlock_ret;
	}

	if (phys) {
		max_blocks = min(mapped_blocks, max_blocks);

		/* Treat newly added block / cluster */
		if (BLOCK_ADDED(bmap_create) || buffer_delay(bh_result)) {

			/* Update i_size_ondisk */
			pos = (iblock + 1) << sb->s_blocksize_bits;
			if (SDFAT_I(inode)->i_size_ondisk < pos) {
				/* Debug purpose */
				if ((pos - SDFAT_I(inode)->i_size_ondisk) > bh_result->b_size) {
					/* This never happens without DA */
					MMSG("Jumping get_block\n");
				}

				SDFAT_I(inode)->i_size_ondisk = pos;
				sdfat_debug_check_clusters(inode);
			}

			if (BLOCK_ADDED(bmap_create)) {
				/* Old way (w/o DA)
				 * create == 1 only if iblock > i_size
				 * (in block unit)
				 */

				/* 20130723 CHECK
				 * Truncate와 동시에 발생할 경우,
				 * i_size < (i_block 위치) 면서 buffer_delay()가
				 * 켜져있을 수 있다.
				 *
				 * 기존에 할당된 영역을 다시 쓸 뿐이므로 큰 문제
				 * 없지만, 그 경우, 미리 i_size_aligned 가 확장된
				 * 영역이어야 한다.
				 */

				/* FOR GRACEFUL ERROR HANDLING */
				if (buffer_delay(bh_result) &&
					(pos > SDFAT_I(inode)->i_size_aligned)) {
					sdfat_fs_error(sb, "requested for bmap "
						"out of range(pos:(%llu)>i_size_aligned(%llu)\n",
						pos, SDFAT_I(inode)->i_size_aligned);
					sdfat_debug_bug_on(1);
					err = -EIO;
					goto unlock_ret;
				}
				set_buffer_new(bh_result);

				/*
				 * adjust i_size_aligned if i_size_ondisk is
				 * bigger than it. (i.e. non-DA)
				 */
				if (SDFAT_I(inode)->i_size_ondisk >
					SDFAT_I(inode)->i_size_aligned) {
					SDFAT_I(inode)->i_size_aligned =
						SDFAT_I(inode)->i_size_ondisk;
				}
			}

			if (buffer_delay(bh_result))
				clear_buffer_delay(bh_result);

#if 0
			/* Debug purpose */
			if (SDFAT_I(inode)->i_size_ondisk >
					SDFAT_I(inode)->i_size_aligned) {
				/* Only after truncate
				 * and the two size variables should indicate
				 * same i_block
				 */
				unsigned int blocksize = 1 << inode->i_blkbits;
				BUG_ON(SDFAT_I(inode)->i_size_ondisk -
					SDFAT_I(inode)->i_size_aligned >= blocksize);
			}
#endif
		}
		map_bh(bh_result, sb, phys);
	}

	bh_result->b_size = max_blocks << sb->s_blocksize_bits;
unlock_ret:
	__unlock_super(sb);
	return err;
}

static int sdfat_readpage(struct file *file, struct page *page)
{
	int ret;

	ret =  mpage_readpage(page, sdfat_get_block);
	return ret;
}

static int sdfat_readpages(struct file *file, struct address_space *mapping,
			   struct list_head *pages, unsigned int nr_pages)
{
	int ret;

	ret =  mpage_readpages(mapping, pages, nr_pages, sdfat_get_block);
	return ret;
}

static inline void sdfat_submit_fullpage_bio(struct block_device *bdev,
			sector_t sector, unsigned int length, struct page *page)
{
	/* Single page bio submit */
	struct bio *bio;

	BUG_ON((length > PAGE_SIZE) || (length == 0));

	/*
	 * If __GFP_WAIT is set, then bio_alloc will always be able to allocate
	 * a bio. This is due to the mempool guarantees. To make this work, callers
	 * must never allocate more than 1 bio at a time from this pool.
	 *
	 * #define GFP_NOIO	(__GFP_WAIT)
	 */
	bio = bio_alloc(GFP_NOIO, 1);

	bio_set_dev(bio, bdev);
	bio->bi_vcnt = 1;
	bio->bi_io_vec[0].bv_page = page;	/* Inline vec */
	bio->bi_io_vec[0].bv_len = length;	/* PAGE_SIZE */
	bio->bi_io_vec[0].bv_offset = 0;
	__sdfat_set_bio_iterate(bio, sector, length, 0, 0);

	bio->bi_end_io = sdfat_writepage_end_io;
	__sdfat_submit_bio_write(bio);
}

static int sdfat_writepage(struct page *page, struct writeback_control *wbc)
{
	struct inode * const inode = page->mapping->host;
	struct super_block *sb = inode->i_sb;
	loff_t i_size = i_size_read(inode);
	const pgoff_t end_index = i_size >> PAGE_SHIFT;
	const unsigned int blocks_per_page = PAGE_SIZE >> inode->i_blkbits;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	struct buffer_head *bh, *head;
	sector_t block, block_0, last_phys;
	int ret;
	unsigned int nr_blocks_towrite = blocks_per_page;

	/* Don't distinguish 0-filled/clean block.
	 * Just write back the whole page
	 */
	if (fsi->cluster_size < PAGE_SIZE)
		goto confused;

	if (!PageUptodate(page)) {
		MMSG("%s: Not up-to-date page -> block_write_full_page\n",
				__func__);
		goto confused;
	}

	if (page->index >= end_index) {
		/* last page or outside i_size */
		unsigned int offset = i_size & (PAGE_SIZE-1);

		/* If a truncation is in progress */
		if (page->index > end_index || !offset)
			goto confused;

		/* 0-fill after i_size */
		zero_user_segment(page, offset, PAGE_SIZE);
	}

	if (!page_has_buffers(page)) {
		MMSG("WP: No buffers -> block_write_full_page\n");
		goto confused;
	}

	block = (sector_t)page->index << (PAGE_SHIFT - inode->i_blkbits);
	block_0 = block; /* first block */
	head = page_buffers(page);
	bh = head;

	last_phys = 0;
	do {
		BUG_ON(buffer_locked(bh));

		if (!buffer_dirty(bh) || !buffer_uptodate(bh)) {
			if (nr_blocks_towrite == blocks_per_page)
				nr_blocks_towrite = (unsigned int) (block - block_0);

			BUG_ON(nr_blocks_towrite >= blocks_per_page);

			// !uptodate but dirty??
			if (buffer_dirty(bh))
				goto confused;

			// Nothing to writeback in this block
			bh = bh->b_this_page;
			block++;
			continue;
		}

		if (nr_blocks_towrite != blocks_per_page)
			// Dirty -> Non-dirty -> Dirty again case
			goto confused;

		/* Map if needed */
		if (!buffer_mapped(bh) || buffer_delay(bh)) {
			BUG_ON(bh->b_size != (1 << (inode->i_blkbits)));
			ret = sdfat_get_block(inode, block, bh, 1);
			if (ret)
				goto confused;

			if (buffer_new(bh)) {
				clear_buffer_new(bh);
				__sdfat_clean_bdev_aliases(bh->b_bdev, bh->b_blocknr);
			}
		}

		/* continuity check */
		if (((last_phys + 1) != bh->b_blocknr) && (last_phys != 0)) {
			DMSG("Non-contiguous block mapping in single page");
			goto confused;
		}

		last_phys = bh->b_blocknr;
		bh = bh->b_this_page;
		block++;
	} while (bh != head);

	if (nr_blocks_towrite == 0) {
		DMSG("Page dirty but no dirty bh? alloc_208\n");
		goto confused;
	}


	/* Write-back */
	do {
		clear_buffer_dirty(bh);
		bh = bh->b_this_page;
	} while (bh != head);

	BUG_ON(PageWriteback(page));
	set_page_writeback(page);

	/**
	 * Turn off MAPPED flag in victim's bh if defrag on.
	 * Another write_begin can starts after get_block for defrag victims called.
	 * In this case, write_begin calls get_block and get original block number
	 * and previous defrag will be canceled.
	 */
	if (unlikely(__check_dfr_on(inode,
		(loff_t)(page->index << PAGE_SHIFT),
		(loff_t)((page->index + 1) << PAGE_SHIFT),
		__func__))) {
		do {
			clear_buffer_mapped(bh);
			bh = bh->b_this_page;
		} while (bh != head);
	}

	// Trace # of pages queued (Approx.)
	atomic_inc(&SDFAT_SB(sb)->stat_n_pages_queued);

	sdfat_submit_fullpage_bio(head->b_bdev,
		head->b_blocknr << (sb->s_blocksize_bits - SECTOR_SIZE_BITS),
		nr_blocks_towrite << inode->i_blkbits,
		page);

	unlock_page(page);

	return 0;

confused:
#ifdef CONFIG_SDFAT_TRACE_IO
	SDFAT_SB(sb)->stat_n_pages_confused++;
#endif
	ret = block_write_full_page(page, sdfat_get_block, wbc);
	return ret;
}

static int sdfat_da_writepages(struct address_space *mapping,
				struct writeback_control *wbc)
{
	MMSG("%s(inode:%p) with nr_to_write = 0x%08lx "
		"(ku %d, bg %d, tag %d, rc %d )\n",
		__func__, mapping->host, wbc->nr_to_write,
		wbc->for_kupdate, wbc->for_background, wbc->tagged_writepages,
		wbc->for_reclaim);

	ASSERT(mapping->a_ops == &sdfat_da_aops);

#ifdef CONFIG_SDFAT_ALIGNED_MPAGE_WRITE
	if (SDFAT_SB(mapping->host->i_sb)->options.adj_req)
		return sdfat_mpage_writepages(mapping, wbc, sdfat_get_block);
#endif
	return generic_writepages(mapping, wbc);
}

static int sdfat_writepages(struct address_space *mapping,
				struct writeback_control *wbc)
{
	MMSG("%s(inode:%p) with nr_to_write = 0x%08lx "
		"(ku %d, bg %d, tag %d, rc %d )\n",
		__func__, mapping->host, wbc->nr_to_write,
		wbc->for_kupdate, wbc->for_background, wbc->tagged_writepages,
		wbc->for_reclaim);

	ASSERT(mapping->a_ops == &sdfat_aops);

#ifdef CONFIG_SDFAT_ALIGNED_MPAGE_WRITE
	if (SDFAT_SB(mapping->host->i_sb)->options.adj_req)
		return sdfat_mpage_writepages(mapping, wbc, sdfat_get_block);
#endif
	return mpage_writepages(mapping, wbc, sdfat_get_block);
}

static void sdfat_write_failed(struct address_space *mapping, loff_t to)
{
	struct inode *inode = mapping->host;

	if (to > i_size_read(inode)) {
		__sdfat_truncate_pagecache(inode, to, i_size_read(inode));
		sdfat_truncate(inode, SDFAT_I(inode)->i_size_aligned);
	}
}

static int sdfat_check_writable(struct super_block *sb)
{
	if (fsapi_check_bdi_valid(sb))
		return -EIO;

	if (sb->s_flags & MS_RDONLY)
		return -EROFS;

	return 0;
}

static int __sdfat_write_begin(struct file *file, struct address_space *mapping,
				 loff_t pos, unsigned int len,
				 unsigned int flags, struct page **pagep,
				 void **fsdata, get_block_t *get_block,
				 loff_t *bytes, const char *fname)
{
	struct super_block *sb = mapping->host->i_sb;
	int ret;

	__cancel_dfr_work(mapping->host, pos, (loff_t)(pos + len), fname);

	ret = sdfat_check_writable(sb);
	if (unlikely(ret < 0))
		return ret;

	*pagep = NULL;
	ret = cont_write_begin(file, mapping, pos, len, flags, pagep, fsdata,
					get_block, bytes);

	if (ret < 0)
		sdfat_write_failed(mapping, pos+len);

	return ret;
}


static int sdfat_da_write_begin(struct file *file, struct address_space *mapping,
				 loff_t pos, unsigned int len, unsigned int flags,
				 struct page **pagep, void **fsdata)
{
	return __sdfat_write_begin(file, mapping, pos, len, flags,
				pagep, fsdata, sdfat_da_prep_block,
				&SDFAT_I(mapping->host)->i_size_aligned,
				__func__);
}


static int sdfat_write_begin(struct file *file, struct address_space *mapping,
				 loff_t pos, unsigned int len, unsigned int flags,
				 struct page **pagep, void **fsdata)
{
	return __sdfat_write_begin(file, mapping, pos, len, flags,
				pagep, fsdata, sdfat_get_block,
				&SDFAT_I(mapping->host)->i_size_ondisk,
				__func__);
}

static int sdfat_write_end(struct file *file, struct address_space *mapping,
				   loff_t pos, unsigned int len, unsigned int copied,
				   struct page *pagep, void *fsdata)
{
	struct inode *inode = mapping->host;
	FILE_ID_T *fid = &(SDFAT_I(inode)->fid);
	int err;

	err = generic_write_end(file, mapping, pos, len, copied, pagep, fsdata);

	/* FOR GRACEFUL ERROR HANDLING */
	if (SDFAT_I(inode)->i_size_aligned < i_size_read(inode)) {
		sdfat_fs_error(inode->i_sb, "invalid size(size(%llu) "
			"> aligned(%llu)\n", i_size_read(inode),
			SDFAT_I(inode)->i_size_aligned);
		sdfat_debug_bug_on(1);
	}

	if (err < len)
		sdfat_write_failed(mapping, pos+len);

	if (!(err < 0) && !(fid->attr & ATTR_ARCHIVE)) {
		inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
		fid->attr |= ATTR_ARCHIVE;
		mark_inode_dirty(inode);
	}

	return err;
}

static inline ssize_t __sdfat_direct_IO(int rw, struct kiocb *iocb,
		struct inode *inode, void *iov_u, loff_t offset,
		loff_t count, unsigned long nr_segs)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t size = offset + count;
	ssize_t ret;

	if (rw == WRITE) {
		/*
		 * FIXME: blockdev_direct_IO() doesn't use ->write_begin(),
		 * so we need to update the ->i_size_aligned to block boundary.
		 *
		 * But we must fill the remaining area or hole by nul for
		 * updating ->i_size_aligned
		 *
		 * Return 0, and fallback to normal buffered write.
		 */
		if (SDFAT_I(inode)->i_size_aligned < size)
			return 0;
	}

	/*
	 * sdFAT need to use the DIO_LOCKING for avoiding the race
	 * condition of sdfat_get_block() and ->truncate().
	 */
	ret = __sdfat_blkdev_direct_IO(rw, iocb, inode, iov_u, offset, nr_segs);
	if (ret < 0 && (rw & WRITE))
		sdfat_write_failed(mapping, size);

	return ret;
}

static const struct address_space_operations sdfat_aops = {
	.readpage    = sdfat_readpage,
	.readpages   = sdfat_readpages,
	.writepage   = sdfat_writepage,
	.writepages  = sdfat_writepages,
	.write_begin = sdfat_write_begin,
	.write_end   = sdfat_write_end,
	.direct_IO   = sdfat_direct_IO,
	.bmap        = sdfat_aop_bmap
};

static const struct address_space_operations sdfat_da_aops = {
	.readpage    = sdfat_readpage,
	.readpages   = sdfat_readpages,
	.writepage   = sdfat_writepage,
	.writepages  = sdfat_da_writepages,
	.write_begin = sdfat_da_write_begin,
	.write_end   = sdfat_write_end,
	.direct_IO   = sdfat_direct_IO,
	.bmap        = sdfat_aop_bmap
};

/*======================================================================*/
/*  Super Operations                                                    */
/*======================================================================*/

static inline unsigned long sdfat_hash(loff_t i_pos)
{
	return hash_32(i_pos, SDFAT_HASH_BITS);
}

static void sdfat_attach(struct inode *inode, loff_t i_pos)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(inode->i_sb);
	struct hlist_head *head = sbi->inode_hashtable + sdfat_hash(i_pos);

	spin_lock(&sbi->inode_hash_lock);
	SDFAT_I(inode)->i_pos = i_pos;
	hlist_add_head(&SDFAT_I(inode)->i_hash_fat, head);
	spin_unlock(&sbi->inode_hash_lock);
}

static void sdfat_detach(struct inode *inode)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(inode->i_sb);

	spin_lock(&sbi->inode_hash_lock);
	hlist_del_init(&SDFAT_I(inode)->i_hash_fat);
	SDFAT_I(inode)->i_pos = 0;
	spin_unlock(&sbi->inode_hash_lock);
}


/* doesn't deal with root inode */
static int sdfat_fill_inode(struct inode *inode, const FILE_ID_T *fid)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(inode->i_sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	DIR_ENTRY_T info;
	u64 size = fid->size;

	memcpy(&(SDFAT_I(inode)->fid), fid, sizeof(FILE_ID_T));

	SDFAT_I(inode)->i_pos = 0;
	SDFAT_I(inode)->target = NULL;
	inode->i_uid = sbi->options.fs_uid;
	inode->i_gid = sbi->options.fs_gid;
	inode->i_version++;
	inode->i_generation = get_seconds();

	if (fsapi_read_inode(inode, &info) < 0) {
		MMSG("%s: failed to read stat!\n", __func__);
		return -EIO;
	}

	if (info.Attr & ATTR_SUBDIR) { /* directory */
		inode->i_generation &= ~1;
		inode->i_mode = sdfat_make_mode(sbi, info.Attr, S_IRWXUGO);
		inode->i_op = &sdfat_dir_inode_operations;
		inode->i_fop = &sdfat_dir_operations;

		set_nlink(inode, info.NumSubdirs);
	} else if (info.Attr & ATTR_SYMLINK) { /* symbolic link */
		inode->i_op = &sdfat_symlink_inode_operations;
		inode->i_generation |= 1;
		inode->i_mode = sdfat_make_mode(sbi, info.Attr, S_IRWXUGO);
	} else { /* regular file */
		inode->i_generation |= 1;
		inode->i_mode = sdfat_make_mode(sbi, info.Attr, S_IRWXUGO);
		inode->i_op = &sdfat_file_inode_operations;
		inode->i_fop = &sdfat_file_operations;

		if (sbi->options.improved_allocation & SDFAT_ALLOC_DELAY)
			inode->i_mapping->a_ops = &sdfat_da_aops;
		else
			inode->i_mapping->a_ops = &sdfat_aops;

		inode->i_mapping->nrpages = 0;

	}

	/*
	 * Use fid->size instead of info.Size
	 * because info.Size means the value saved on disk
	 */
	i_size_write(inode, size);

	/* ondisk and aligned size should be aligned with block size */
	if (size & (inode->i_sb->s_blocksize - 1)) {
		size |= (inode->i_sb->s_blocksize - 1);
		size++;
	}

	SDFAT_I(inode)->i_size_aligned = size;
	SDFAT_I(inode)->i_size_ondisk = size;
	sdfat_debug_check_clusters(inode);

	sdfat_save_attr(inode, info.Attr);

	inode->i_blocks = ((i_size_read(inode) + (fsi->cluster_size - 1))
		   & ~((loff_t)fsi->cluster_size - 1)) >> inode->i_blkbits;

	sdfat_time_fat2unix(sbi, &inode->i_mtime, &info.ModifyTimestamp);
	sdfat_time_fat2unix(sbi, &inode->i_ctime, &info.CreateTimestamp);
	sdfat_time_fat2unix(sbi, &inode->i_atime, &info.AccessTimestamp);

	__init_dfr_info(inode);

	return 0;
}

static struct inode *sdfat_build_inode(struct super_block *sb, const FILE_ID_T *fid, loff_t i_pos)
{
	struct inode *inode;
	int err;

	inode = sdfat_iget(sb, i_pos);
	if (inode)
		goto out;
	inode = new_inode(sb);
	if (!inode) {
		inode = ERR_PTR(-ENOMEM);
		goto out;
	}
	inode->i_ino = iunique(sb, SDFAT_ROOT_INO);
	inode->i_version = 1;
	err = sdfat_fill_inode(inode, fid);
	if (err) {
		iput(inode);
		inode = ERR_PTR(err);
		goto out;
	}
	sdfat_attach(inode, i_pos);
	insert_inode_hash(inode);
out:
	return inode;
}

static struct inode *sdfat_alloc_inode(struct super_block *sb)
{
	struct sdfat_inode_info *ei;

	ei = kmem_cache_alloc(sdfat_inode_cachep, GFP_NOFS);
	if (!ei)
		return NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	init_rwsem(&ei->truncate_lock);
#endif
	return &ei->vfs_inode;
}

static void sdfat_destroy_inode(struct inode *inode)
{
	if (SDFAT_I(inode)->target) {
		kfree(SDFAT_I(inode)->target);
		SDFAT_I(inode)->target = NULL;
	}

	kmem_cache_free(sdfat_inode_cachep, SDFAT_I(inode));
}

static int __sdfat_write_inode(struct inode *inode, int sync)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	DIR_ENTRY_T info;

	if (inode->i_ino == SDFAT_ROOT_INO)
		return 0;

	info.Attr = sdfat_make_attr(inode);
	info.Size = i_size_read(inode);

	sdfat_time_unix2fat(sbi, &inode->i_mtime, &info.ModifyTimestamp);
	sdfat_time_unix2fat(sbi, &inode->i_ctime, &info.CreateTimestamp);
	sdfat_time_unix2fat(sbi, &inode->i_atime, &info.AccessTimestamp);

	if (!__support_write_inode_sync(sb))
		sync = 0;

	/* FIXME : Do we need handling error? */
	return fsapi_write_inode(inode, &info, sync);
}

static int sdfat_sync_inode(struct inode *inode)
{
	return __sdfat_write_inode(inode, 1);
}

static int sdfat_write_inode(struct inode *inode, struct writeback_control *wbc)
{
	return __sdfat_write_inode(inode, wbc->sync_mode == WB_SYNC_ALL);
}

static void sdfat_evict_inode(struct inode *inode)
{
	truncate_inode_pages(&inode->i_data, 0);

	if (!inode->i_nlink) {
		loff_t old_size = i_size_read(inode);

		i_size_write(inode, 0);

		SDFAT_I(inode)->fid.size = old_size;

		__cancel_dfr_work(inode, 0, (loff_t)old_size, __func__);

		/* TO CHECK evicting directory works correctly */
		MMSG("%s: inode(%p) evict %s (size(%llu) to zero)\n",
				__func__, inode,
				S_ISDIR(inode->i_mode) ? "directory" : "file",
				(u64)old_size);
		fsapi_truncate(inode, old_size, 0);
	}

	invalidate_inode_buffers(inode);
	clear_inode(inode);
	fsapi_invalidate_extent(inode);
	sdfat_detach(inode);

	/* after end of this function, caller will remove inode hash */
	/* remove_inode_hash(inode); */
}



static void sdfat_put_super(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	int err;

	sdfat_log_msg(sb, KERN_INFO, "trying to unmount...");

	__cancel_delayed_work_sync(sbi);

	if (__is_sb_dirty(sb))
		sdfat_write_super(sb);

	__free_dfr_mem_if_required(sb);
	err = fsapi_umount(sb);

	if (sbi->nls_disk) {
		unload_nls(sbi->nls_disk);
		sbi->nls_disk = NULL;
		sbi->options.codepage = sdfat_default_codepage;
	}
	if (sbi->nls_io) {
		unload_nls(sbi->nls_io);
		sbi->nls_io = NULL;
	}
	if (sbi->options.iocharset != sdfat_default_iocharset) {
		kfree(sbi->options.iocharset);
		sbi->options.iocharset = sdfat_default_iocharset;
	}

	sb->s_fs_info = NULL;

	kobject_del(&sbi->sb_kobj);
	kobject_put(&sbi->sb_kobj);
	if (!sbi->use_vmalloc)
		kfree(sbi);
	else
		vfree(sbi);

	sdfat_log_msg(sb, KERN_INFO, "unmounted successfully! %s",
				err ? "(with previous I/O errors)" : "");
}

static inline void __flush_delayed_meta(struct super_block *sb, s32 sync)
{
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	fsapi_cache_flush(sb, sync);
#else
	/* DO NOTHING */
#endif
}

static void sdfat_write_super(struct super_block *sb)
{
	int time = 0;

	__lock_super(sb);

	__set_sb_clean(sb);

#ifdef CONFIG_SDFAT_DFR
	if (atomic_read(&(SDFAT_SB(sb)->dfr_info.stat)) == DFR_SB_STAT_VALID)
		fsapi_dfr_update_fat_next(sb);
#endif

	/* flush delayed FAT/DIR dirty */
	__flush_delayed_meta(sb, 0);

	if (!(sb->s_flags & MS_RDONLY))
		fsapi_sync_fs(sb, 0);

	__unlock_super(sb);

	time = jiffies;

	/* Issuing bdev requests is needed
	 * to guarantee DIR updates in time
	 * whether w/ or w/o delayed DIR dirty feature.
	 * (otherwise DIR updates could be delayed for 5 + 5 secs at max.)
	 */
	sync_blockdev(sb->s_bdev);

#if  (defined(CONFIG_SDFAT_DFR) && defined(CONFIG_SDFAT_DFR_DEBUG))
	/* SPO test */
	fsapi_dfr_spo_test(sb, DFR_SPO_FAT_NEXT, __func__);
#endif
	MMSG("BD: sdfat_write_super (bdev_sync for %ld ms)\n",
			(jiffies - time) * 1000 / HZ);
}


static void __dfr_update_fat_next(struct super_block *sb)
{
#ifdef	CONFIG_SDFAT_DFR
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	if (sbi->options.defrag &&
		(atomic_read(&sbi->dfr_info.stat) == DFR_SB_STAT_VALID)) {
		fsapi_dfr_update_fat_next(sb);
	}
#endif
}

static void __dfr_update_fat_prev(struct super_block *sb, int wait)
{
#ifdef	CONFIG_SDFAT_DFR
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct defrag_info *sb_dfr = &sbi->dfr_info;
	/* static time available? */
	static int time; /* initialized by zero */
	int uevent = 0, total = 0, clean = 0, full = 0;
	int spent = jiffies - time;

	if (!(sbi->options.defrag && wait))
		return;

	__lock_super(sb);
	/* Update FAT for defrag */
	if (atomic_read(&(sbi->dfr_info.stat)) == DFR_SB_STAT_VALID) {

		fsapi_dfr_update_fat_prev(sb, 0);

		/* flush delayed FAT/DIR dirty */
		__flush_delayed_meta(sb, 0);

		/* Complete defrag req */
		fsapi_sync_fs(sb, 1);
		atomic_set(&sb_dfr->stat, DFR_SB_STAT_REQ);
		complete_all(&sbi->dfr_complete);
	} else if (((spent < 0) ||  (spent > DFR_DEFAULT_TIMEOUT)) &&
		(atomic_read(&(sbi->dfr_info.stat)) == DFR_SB_STAT_IDLE)) {
		uevent = fsapi_dfr_check_dfr_required(sb, &total, &clean, &full);
		time = jiffies;
	}
	__unlock_super(sb);

	if (uevent) {
		kobject_uevent(&SDFAT_SB(sb)->sb_kobj, KOBJ_CHANGE);
		dfr_debug("uevent for defrag_daemon, total_au %d, "
				"clean_au %d, full_au %d", total, clean, full);
	}
#endif
}

static int sdfat_sync_fs(struct super_block *sb, int wait)
{
	int err = 0;

	/* If there are some dirty buffers in the bdev inode */
	if (__is_sb_dirty(sb)) {
		__lock_super(sb);
		__set_sb_clean(sb);

		__dfr_update_fat_next(sb);

		err = fsapi_sync_fs(sb, 1);

#if (defined(CONFIG_SDFAT_DFR) && defined(CONFIG_SDFAT_DFR_DEBUG))
		/* SPO test */
		fsapi_dfr_spo_test(sb, DFR_SPO_FAT_NEXT, __func__);
#endif

		__unlock_super(sb);
	}

	__dfr_update_fat_prev(sb, wait);

	return err;
}

static int sdfat_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	/*
	 * patch 1.2.2 :
	 * fixed the slow-call problem because of volume-lock contention.
	 */
	struct super_block *sb = dentry->d_sb;
	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	VOL_INFO_T info;

	/* fsapi_statfs will try to get a volume lock if needed */
	if (fsapi_statfs(sb, &info))
		return -EIO;

	if (fsi->prev_eio)
		sdfat_msg(sb, KERN_INFO, "called statfs with previous"
				" I/O error(0x%02X).", fsi->prev_eio);

	buf->f_type = sb->s_magic;
	buf->f_bsize = info.ClusterSize;
	buf->f_blocks = info.NumClusters;
	buf->f_bfree = info.FreeClusters;
	buf->f_bavail = info.FreeClusters;
	buf->f_fsid.val[0] = (u32)id;
	buf->f_fsid.val[1] = (u32)(id >> 32);
	buf->f_namelen = 260;

	return 0;
}

static int sdfat_remount(struct super_block *sb, int *flags, char *data)
{
	unsigned long prev_sb_flags;
	char *orig_data = kstrdup(data, GFP_KERNEL);
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);

	*flags |= MS_NODIRATIME;

	prev_sb_flags = sb->s_flags;

	sdfat_remount_syncfs(sb);

	fsapi_set_vol_flags(sb, VOL_CLEAN, 1);

	sdfat_log_msg(sb, KERN_INFO, "re-mounted(%s->%s), eio=0x%x, Opts: %s",
		(prev_sb_flags & MS_RDONLY) ? "ro" : "rw",
		(*flags & MS_RDONLY) ? "ro" : "rw",
		fsi->prev_eio, orig_data);
	kfree(orig_data);
	return 0;
}

static int __sdfat_show_options(struct seq_file *m, struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct sdfat_mount_options *opts = &sbi->options;
	FS_INFO_T *fsi = &(sbi->fsi);

	/* Show partition info */
	seq_printf(m, ",fs=%s", sdfat_get_vol_type_str(fsi->vol_type));
	if (fsi->prev_eio)
		seq_printf(m, ",eio=0x%x", fsi->prev_eio);
	if (!uid_eq(opts->fs_uid, GLOBAL_ROOT_UID))
		seq_printf(m, ",uid=%u",
				from_kuid_munged(&init_user_ns, opts->fs_uid));
	if (!gid_eq(opts->fs_gid, GLOBAL_ROOT_GID))
		seq_printf(m, ",gid=%u",
				from_kgid_munged(&init_user_ns, opts->fs_gid));
	seq_printf(m, ",fmask=%04o", opts->fs_fmask);
	seq_printf(m, ",dmask=%04o", opts->fs_dmask);
	if (opts->allow_utime)
		seq_printf(m, ",allow_utime=%04o", opts->allow_utime);
	if (sbi->nls_disk)
		seq_printf(m, ",codepage=%s", sbi->nls_disk->charset);
	if (sbi->nls_io)
		seq_printf(m, ",iocharset=%s", sbi->nls_io->charset);
	if (opts->utf8)
		seq_puts(m, ",utf8");
	if (sbi->fsi.vol_type != EXFAT)
		seq_puts(m, ",shortname=winnt");
	seq_printf(m, ",namecase=%u", opts->casesensitive);
	if (opts->tz_utc)
		seq_puts(m, ",tz=UTC");
	if (opts->improved_allocation & SDFAT_ALLOC_DELAY)
		seq_puts(m, ",delay");
	if (opts->improved_allocation & SDFAT_ALLOC_SMART)
		seq_printf(m, ",smart,ausize=%u", opts->amap_opt.sect_per_au);
	if (opts->defrag)
		seq_puts(m, ",defrag");
	if (opts->adj_hidsect)
		seq_puts(m, ",adj_hid");
	if (opts->adj_req)
		seq_puts(m, ",adj_req");
	seq_printf(m, ",symlink=%u", opts->symlink);
	seq_printf(m, ",bps=%ld", sb->s_blocksize);
	if (opts->errors == SDFAT_ERRORS_CONT)
		seq_puts(m, ",errors=continue");
	else if (opts->errors == SDFAT_ERRORS_PANIC)
		seq_puts(m, ",errors=panic");
	else
		seq_puts(m, ",errors=remount-ro");
	if (opts->discard)
		seq_puts(m, ",discard");

	return 0;
}

static const struct super_operations sdfat_sops = {
	.alloc_inode   = sdfat_alloc_inode,
	.destroy_inode = sdfat_destroy_inode,
	.write_inode   = sdfat_write_inode,
	.evict_inode  = sdfat_evict_inode,
	.put_super     = sdfat_put_super,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
	.write_super   = sdfat_write_super,
#endif
	.sync_fs       = sdfat_sync_fs,
	.statfs        = sdfat_statfs,
	.remount_fs    = sdfat_remount,
	.show_options  = sdfat_show_options,
};

/*======================================================================*/
/*  SYSFS Operations                                                    */
/*======================================================================*/
#define SDFAT_ATTR(name, mode, show, store) \
static struct sdfat_attr sdfat_attr_##name = __ATTR(name, mode, show, store)

struct sdfat_attr {
	struct attribute attr;
	ssize_t (*show)(struct sdfat_sb_info *, char *);
	ssize_t (*store)(struct sdfat_sb_info *, const char *, size_t);
};

static ssize_t sdfat_attr_show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct sdfat_sb_info *sbi = container_of(kobj, struct sdfat_sb_info, sb_kobj);
	struct sdfat_attr *a = container_of(attr, struct sdfat_attr, attr);

	return a->show ? a->show(sbi, buf) : 0;
}

static ssize_t sdfat_attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t len)
{
	struct sdfat_sb_info *sbi = container_of(kobj, struct sdfat_sb_info, sb_kobj);
	struct sdfat_attr *a = container_of(attr, struct sdfat_attr, attr);

	return a->store ? a->store(sbi, buf, len) : len;
}

static const struct sysfs_ops sdfat_attr_ops = {
	.show  = sdfat_attr_show,
	.store = sdfat_attr_store,
};


static ssize_t type_show(struct sdfat_sb_info *sbi, char *buf)
{
	FS_INFO_T *fsi = &(sbi->fsi);

	return snprintf(buf, PAGE_SIZE, "%s\n", sdfat_get_vol_type_str(fsi->vol_type));
}
SDFAT_ATTR(type, 0444, type_show, NULL);

static ssize_t eio_show(struct sdfat_sb_info *sbi, char *buf)
{
	FS_INFO_T *fsi = &(sbi->fsi);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", fsi->prev_eio);
}
SDFAT_ATTR(eio, 0444, eio_show, NULL);

static ssize_t fratio_show(struct sdfat_sb_info *sbi, char *buf)
{
	unsigned int n_total_au = 0;
	unsigned int n_clean_au = 0;
	unsigned int n_full_au = 0;
	unsigned int n_dirty_au = 0;
	unsigned int fr = 0;

	n_total_au = fsapi_get_au_stat(sbi->host_sb, VOL_AU_STAT_TOTAL);
	n_clean_au = fsapi_get_au_stat(sbi->host_sb, VOL_AU_STAT_CLEAN);
	n_full_au = fsapi_get_au_stat(sbi->host_sb, VOL_AU_STAT_FULL);
	n_dirty_au = n_total_au - (n_full_au + n_clean_au);

	if (!n_dirty_au)
		fr = 0;
	else if (!n_clean_au)
		fr = 100;
	else
		fr = (n_dirty_au * 100) / (n_clean_au + n_dirty_au);

	return snprintf(buf, PAGE_SIZE, "%u\n", fr);
}
SDFAT_ATTR(fratio, 0444, fratio_show, NULL);

static ssize_t totalau_show(struct sdfat_sb_info *sbi, char *buf)
{
	unsigned int n_au = 0;

	n_au = fsapi_get_au_stat(sbi->host_sb, VOL_AU_STAT_TOTAL);
	return snprintf(buf, PAGE_SIZE, "%u\n", n_au);
}
SDFAT_ATTR(totalau, 0444, totalau_show, NULL);

static ssize_t cleanau_show(struct sdfat_sb_info *sbi, char *buf)
{
	unsigned int n_clean_au = 0;

	n_clean_au = fsapi_get_au_stat(sbi->host_sb, VOL_AU_STAT_CLEAN);
	return snprintf(buf, PAGE_SIZE, "%u\n", n_clean_au);
}
SDFAT_ATTR(cleanau, 0444, cleanau_show, NULL);

static ssize_t fullau_show(struct sdfat_sb_info *sbi, char *buf)
{
	unsigned int n_full_au = 0;

	n_full_au = fsapi_get_au_stat(sbi->host_sb, VOL_AU_STAT_FULL);
	return snprintf(buf, PAGE_SIZE, "%u\n", n_full_au);
}
SDFAT_ATTR(fullau, 0444, fullau_show, NULL);

static struct attribute *sdfat_attrs[] = {
	&sdfat_attr_type.attr,
	&sdfat_attr_eio.attr,
	&sdfat_attr_fratio.attr,
	&sdfat_attr_totalau.attr,
	&sdfat_attr_cleanau.attr,
	&sdfat_attr_fullau.attr,
	NULL,
};

static struct kobj_type sdfat_ktype = {
	.default_attrs = sdfat_attrs,
	.sysfs_ops     = &sdfat_attr_ops,
};

static ssize_t version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buff)
{
	return snprintf(buff, PAGE_SIZE, "FS Version %s\n", SDFAT_VERSION);
}

static struct kobj_attribute version_attr = __ATTR_RO(version);

static struct attribute *attributes[] = {
	&version_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attributes,
};

/*======================================================================*/
/*  Super Block Read Operations                                         */
/*======================================================================*/

enum {
	Opt_uid,
	Opt_gid,
	Opt_umask,
	Opt_dmask,
	Opt_fmask,
	Opt_allow_utime,
	Opt_codepage,
	Opt_charset,
	Opt_utf8,
	Opt_namecase,
	Opt_tz_utc,
	Opt_adj_hidsect,
	Opt_delay,
	Opt_smart,
	Opt_ausize,
	Opt_packing,
	Opt_defrag,
	Opt_symlink,
	Opt_debug,
	Opt_err_cont,
	Opt_err_panic,
	Opt_err_ro,
	Opt_err,
	Opt_discard,
	Opt_fs,
	Opt_adj_req,
#ifdef CONFIG_SDFAT_USE_FOR_VFAT
	Opt_shortname_lower,
	Opt_shortname_win95,
	Opt_shortname_winnt,
	Opt_shortname_mixed,
#endif /* CONFIG_SDFAT_USE_FOR_VFAT */
};

static const match_table_t sdfat_tokens = {
	{Opt_uid, "uid=%u"},
	{Opt_gid, "gid=%u"},
	{Opt_umask, "umask=%o"},
	{Opt_dmask, "dmask=%o"},
	{Opt_fmask, "fmask=%o"},
	{Opt_allow_utime, "allow_utime=%o"},
	{Opt_codepage, "codepage=%u"},
	{Opt_charset, "iocharset=%s"},
	{Opt_utf8, "utf8"},
	{Opt_namecase, "namecase=%u"},
	{Opt_tz_utc, "tz=UTC"},
	{Opt_adj_hidsect, "adj_hid"},
	{Opt_delay, "delay"},
	{Opt_smart, "smart"},
	{Opt_ausize, "ausize=%u"},
	{Opt_packing, "packing=%u"},
	{Opt_defrag, "defrag"},
	{Opt_symlink, "symlink=%u"},
	{Opt_debug, "debug"},
	{Opt_err_cont, "errors=continue"},
	{Opt_err_panic, "errors=panic"},
	{Opt_err_ro, "errors=remount-ro"},
	{Opt_discard, "discard"},
	{Opt_fs, "fs=%s"},
	{Opt_adj_req, "adj_req"},
#ifdef CONFIG_SDFAT_USE_FOR_VFAT
	{Opt_shortname_lower, "shortname=lower"},
	{Opt_shortname_win95, "shortname=win95"},
	{Opt_shortname_winnt, "shortname=winnt"},
	{Opt_shortname_mixed, "shortname=mixed"},
#endif /* CONFIG_SDFAT_USE_FOR_VFAT */
	{Opt_err, NULL}
};

static int parse_options(struct super_block *sb, char *options, int silent,
				int *debug, struct sdfat_mount_options *opts)
{
	char *p;
	substring_t args[MAX_OPT_ARGS];
	int option, i;
	char *tmpstr;

	opts->fs_uid = current_uid();
	opts->fs_gid = current_gid();
	opts->fs_fmask = opts->fs_dmask = current->fs->umask;
	opts->allow_utime = (unsigned short) -1;
	opts->codepage = sdfat_default_codepage;
	opts->iocharset = sdfat_default_iocharset;
	opts->casesensitive = 0;
	opts->utf8 = 0;
	opts->adj_hidsect = 0;
	opts->tz_utc = 0;
	opts->improved_allocation = 0;
	opts->amap_opt.pack_ratio = 0;	// Default packing
	opts->amap_opt.sect_per_au = 0;
	opts->amap_opt.misaligned_sect = 0;
	opts->symlink = 0;
	opts->errors = SDFAT_ERRORS_RO;
	opts->discard = 0;
	*debug = 0;

	if (!options)
		goto out;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;

		if (!*p)
			continue;
		token = match_token(p, sdfat_tokens, args);
		switch (token) {
		case Opt_uid:
			if (match_int(&args[0], &option))
				return 0;
			opts->fs_uid = make_kuid(current_user_ns(), option);
			break;
		case Opt_gid:
			if (match_int(&args[0], &option))
				return 0;
			opts->fs_gid = make_kgid(current_user_ns(), option);
			break;
		case Opt_umask:
		case Opt_dmask:
		case Opt_fmask:
			if (match_octal(&args[0], &option))
				return 0;
			if (token != Opt_dmask)
				opts->fs_fmask = option;
			if (token != Opt_fmask)
				opts->fs_dmask = option;
			break;
		case Opt_allow_utime:
			if (match_octal(&args[0], &option))
				return 0;
			opts->allow_utime = option & (S_IWGRP | S_IWOTH);
			break;
		case Opt_codepage:
			if (match_int(&args[0], &option))
				return 0;
			opts->codepage = option;
			break;
		case Opt_charset:
			if (opts->iocharset != sdfat_default_iocharset)
				kfree(opts->iocharset);
			tmpstr = match_strdup(&args[0]);
			if (!tmpstr)
				return -ENOMEM;
			opts->iocharset = tmpstr;
			break;
		case Opt_namecase:
			if (match_int(&args[0], &option))
				return 0;
			opts->casesensitive = (option > 0) ? 1:0;
			break;
		case Opt_utf8:
			opts->utf8 = 1;
			break;
		case Opt_adj_hidsect:
			opts->adj_hidsect = 1;
			break;
		case Opt_tz_utc:
			opts->tz_utc = 1;
			break;
		case Opt_symlink:
			if (match_int(&args[0], &option))
				return 0;
			opts->symlink = option > 0 ? 1 : 0;
			break;
		case Opt_delay:
			opts->improved_allocation |= SDFAT_ALLOC_DELAY;
			break;
		case Opt_smart:
			opts->improved_allocation |= SDFAT_ALLOC_SMART;
			break;
		case Opt_ausize:
			if (match_int(&args[0], &option))
				return -EINVAL;
			if (!is_power_of_2(option))
				return -EINVAL;
			opts->amap_opt.sect_per_au = option;
			IMSG("set AU size by option : %u sectors\n", option);
			break;
		case Opt_packing:
			if (match_int(&args[0], &option))
				return 0;
			opts->amap_opt.pack_ratio = option;
			break;
		case Opt_defrag:
#ifdef	CONFIG_SDFAT_DFR
			opts->defrag = 1;
#else
			IMSG("defragmentation config is not enabled. ignore\n");
#endif
			break;
		case Opt_err_cont:
			opts->errors = SDFAT_ERRORS_CONT;
			break;
		case Opt_err_panic:
			opts->errors = SDFAT_ERRORS_PANIC;
			break;
		case Opt_err_ro:
			opts->errors = SDFAT_ERRORS_RO;
			break;
		case Opt_debug:
			*debug = 1;
			break;
		case Opt_discard:
			opts->discard = 1;
			break;
		case Opt_fs:
			tmpstr = match_strdup(&args[0]);
			if (!tmpstr)
				return -ENOMEM;
			for (i = 0; i < FS_TYPE_MAX; i++) {
				if (!strcmp(tmpstr, FS_TYPE_STR[i])) {
					opts->fs_type = (unsigned char)i;
					sdfat_log_msg(sb, KERN_ERR,
						"set fs-type by option    : %s",
						FS_TYPE_STR[i]);
					break;
				}
			}
			kfree(tmpstr);
			if (i == FS_TYPE_MAX) {
				sdfat_log_msg(sb, KERN_ERR,
					"invalid fs-type, "
					"only allow auto, exfat, vfat");
				return -EINVAL;
			}
			break;
		case Opt_adj_req:
#ifdef CONFIG_SDFAT_ALIGNED_MPAGE_WRITE
			opts->adj_req = 1;
#else
			IMSG("adjust request config is not enabled. ignore\n");
#endif
			break;
#ifdef CONFIG_SDFAT_USE_FOR_VFAT
		case Opt_shortname_lower:
		case Opt_shortname_win95:
		case Opt_shortname_mixed:
			pr_warn("[SDFAT] DRAGONS AHEAD! sdFAT only understands \"shortname=winnt\"!\n");
		case Opt_shortname_winnt:
			break;
#endif /* CONFIG_SDFAT_USE_FOR_VFAT */
		default:
			if (!silent) {
				sdfat_msg(sb, KERN_ERR,
					"unrecognized mount option \"%s\" "
					"or missing value", p);
			}
			return -EINVAL;
		}
	}

out:
	if (opts->allow_utime == (unsigned short) -1)
		opts->allow_utime = ~opts->fs_dmask & (S_IWGRP | S_IWOTH);

	if (opts->utf8 && strcmp(opts->iocharset,  sdfat_iocharset_with_utf8)) {
		sdfat_msg(sb, KERN_WARNING,
			"utf8 enabled, \"iocharset=%s\" is recommended",
			sdfat_iocharset_with_utf8);
	}

	if (opts->discard) {
		struct request_queue *q = bdev_get_queue(sb->s_bdev);

		if (!blk_queue_discard(q))
			sdfat_msg(sb, KERN_WARNING,
				"mounting with \"discard\" option, but "
				"the device does not support discard");
		opts->discard = 0;
	}

	return 0;
}

static void sdfat_hash_init(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	int i;

	spin_lock_init(&sbi->inode_hash_lock);
	for (i = 0; i < SDFAT_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&sbi->inode_hashtable[i]);
}

static int sdfat_read_root(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct timespec ts;
	FS_INFO_T *fsi = &(sbi->fsi);
	DIR_ENTRY_T info;

	ts = CURRENT_TIME_SEC;

	SDFAT_I(inode)->fid.dir.dir = fsi->root_dir;
	SDFAT_I(inode)->fid.dir.flags = 0x01;
	SDFAT_I(inode)->fid.entry = -1;
	SDFAT_I(inode)->fid.start_clu = fsi->root_dir;
	SDFAT_I(inode)->fid.flags = 0x01;
	SDFAT_I(inode)->fid.type = TYPE_DIR;
	SDFAT_I(inode)->fid.version = 0;
	SDFAT_I(inode)->fid.rwoffset = 0;
	SDFAT_I(inode)->fid.hint_bmap.off = CLUS_EOF;
	SDFAT_I(inode)->fid.hint_stat.eidx = 0;
	SDFAT_I(inode)->fid.hint_stat.clu = fsi->root_dir;
	SDFAT_I(inode)->fid.hint_femp.eidx = -1;

	SDFAT_I(inode)->target = NULL;

	if (fsapi_read_inode(inode, &info) < 0)
		return -EIO;

	inode->i_uid = sbi->options.fs_uid;
	inode->i_gid = sbi->options.fs_gid;
	inode->i_version++;
	inode->i_generation = 0;
	inode->i_mode = sdfat_make_mode(sbi, ATTR_SUBDIR, S_IRWXUGO);
	inode->i_op = &sdfat_dir_inode_operations;
	inode->i_fop = &sdfat_dir_operations;

	i_size_write(inode, info.Size);
	SDFAT_I(inode)->fid.size = info.Size;
	inode->i_blocks = ((i_size_read(inode) + (fsi->cluster_size - 1))
		& ~((loff_t)fsi->cluster_size - 1)) >> inode->i_blkbits;
	SDFAT_I(inode)->i_pos = ((loff_t) fsi->root_dir << 32) | 0xffffffff;
	SDFAT_I(inode)->i_size_aligned = i_size_read(inode);
	SDFAT_I(inode)->i_size_ondisk = i_size_read(inode);

	sdfat_save_attr(inode, ATTR_SUBDIR);
	inode->i_mtime = inode->i_atime = inode->i_ctime = ts;
	set_nlink(inode, info.NumSubdirs + 2);
	return 0;
}



static void setup_dops(struct super_block *sb)
{
	if (SDFAT_SB(sb)->options.casesensitive == 0)
		sb->s_d_op = &sdfat_ci_dentry_ops;
	else
		sb->s_d_op = &sdfat_dentry_ops;
}

static int sdfat_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *root_inode = NULL;
	struct sdfat_sb_info *sbi;
	int debug;
	int err;
	char buf[50];
	struct block_device *bdev = sb->s_bdev;
	dev_t bd_dev = bdev ? bdev->bd_dev : 0;

	sdfat_log_msg(sb, KERN_INFO, "trying to mount...");

	/*
	 * GFP_KERNEL is ok here, because while we do hold the
	 * supeblock lock, memory pressure can't call back into
	 * the filesystem, since we're only just about to mount
	 * it and have no inodes etc active!
	 */
	sbi = kzalloc(sizeof(struct sdfat_sb_info), GFP_KERNEL);
	if (!sbi) {
		sdfat_log_msg(sb, KERN_INFO,
			"trying to alloc sbi with vzalloc()");
		sbi = vzalloc(sizeof(struct sdfat_sb_info));
		if (!sbi) {
			sdfat_log_msg(sb, KERN_ERR, "failed to mount! (ENOMEM)");
			return -ENOMEM;
		}
		sbi->use_vmalloc = 1;
	}

	mutex_init(&sbi->s_vlock);
	sb->s_fs_info = sbi;
	sb->s_flags |= MS_NODIRATIME;
	sb->s_magic = SDFAT_SUPER_MAGIC;
	sb->s_op = &sdfat_sops;
	ratelimit_state_init(&sbi->ratelimit, DEFAULT_RATELIMIT_INTERVAL,
						DEFAULT_RATELIMIT_BURST);
	err = parse_options(sb, data, silent, &debug, &sbi->options);
	if (err) {
		sdfat_log_msg(sb, KERN_ERR, "failed to parse options");
		goto failed_mount;
	}

	setup_sdfat_xattr_handler(sb);
	setup_sdfat_sync_super_wq(sb);
	setup_dops(sb);

	err = fsapi_mount(sb);
	if (err) {
		sdfat_log_msg(sb, KERN_ERR, "failed to recognize fat type");
		goto failed_mount;
	}

	/* set up enough so that it can read an inode */
	sdfat_hash_init(sb);

	/*
	 * The low byte of FAT's first entry must have same value with
	 * media-field.  But in real world, too many devices is
	 * writing wrong value.  So, removed that validity check.
	 *
	 * if (FAT_FIRST_ENT(sb, media) != first)
	 */

	err = -EINVAL;
	sprintf(buf, "cp%d", sbi->options.codepage);
	sbi->nls_disk = load_nls(buf);
	if (!sbi->nls_disk) {
		sdfat_log_msg(sb, KERN_ERR, "codepage %s not found", buf);
		goto failed_mount2;
	}

	sbi->nls_io = load_nls(sbi->options.iocharset);
	if (!sbi->nls_io) {
		sdfat_log_msg(sb, KERN_ERR, "IO charset %s not found",
				sbi->options.iocharset);
		goto failed_mount2;
	}

	err = __alloc_dfr_mem_if_required(sb);
	if (err) {
		sdfat_log_msg(sb, KERN_ERR, "failed to initialize a memory for "
				"defragmentation");
		goto failed_mount3;
	}

	err = -ENOMEM;
	root_inode = new_inode(sb);
	if (!root_inode) {
		sdfat_log_msg(sb, KERN_ERR, "failed to allocate root inode.");
		goto failed_mount3;
	}

	root_inode->i_ino = SDFAT_ROOT_INO;
	root_inode->i_version = 1;

	err = sdfat_read_root(root_inode);
	if (err) {
		sdfat_log_msg(sb, KERN_ERR, "failed to initialize root inode.");
		goto failed_mount3;
	}

	sdfat_attach(root_inode, SDFAT_I(root_inode)->i_pos);
	insert_inode_hash(root_inode);

	err = -ENOMEM;
	sb->s_root = __d_make_root(root_inode);
	if (!sb->s_root) {
		sdfat_msg(sb, KERN_ERR, "failed to get the root dentry");
		goto failed_mount3;
	}

	/*
	 * Initialize filesystem attributes (for sysfs)
	 * ex: /sys/fs/sdfat/mmcblk1[179:17]
	 */
	sbi->sb_kobj.kset = sdfat_kset;
	err = kobject_init_and_add(&sbi->sb_kobj, &sdfat_ktype, NULL,
			"%s[%d:%d]", sb->s_id, MAJOR(bd_dev), MINOR(bd_dev));
	if (err) {
		sdfat_msg(sb, KERN_ERR, "Unable to create sdfat attributes for"
					" %s[%d:%d](%d)", sb->s_id,
					MAJOR(bd_dev), MINOR(bd_dev), err);
		goto failed_mount3;
	}

	sdfat_log_msg(sb, KERN_INFO, "mounted successfully!");
	/* FOR BIGDATA */
	sdfat_statistics_set_mnt(&sbi->fsi);
	sdfat_statistics_set_vol_size(sb);
	return 0;

failed_mount3:
	__free_dfr_mem_if_required(sb);
failed_mount2:
	fsapi_umount(sb);
failed_mount:
	sdfat_log_msg(sb, KERN_INFO, "failed to mount! (%d)", err);

	if (root_inode)
		iput(root_inode);
	sb->s_root = NULL;

	if (sbi->nls_io)
		unload_nls(sbi->nls_io);
	if (sbi->nls_disk)
		unload_nls(sbi->nls_disk);
	if (sbi->options.iocharset != sdfat_default_iocharset)
		kfree(sbi->options.iocharset);
	sb->s_fs_info = NULL;
	if (!sbi->use_vmalloc)
		kfree(sbi);
	else
		vfree(sbi);
	return err;
}

static struct dentry *sdfat_fs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, sdfat_fill_super);
}

static void init_once(void *foo)
{
	struct sdfat_inode_info *ei = (struct sdfat_inode_info *)foo;

	INIT_HLIST_NODE(&ei->i_hash_fat);
	inode_init_once(&ei->vfs_inode);
}

static int __init sdfat_init_inodecache(void)
{
	sdfat_inode_cachep = kmem_cache_create("sdfat_inode_cache",
				sizeof(struct sdfat_inode_info),
				0, (SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD),
				init_once);
	if (!sdfat_inode_cachep)
		return -ENOMEM;
	return 0;
}

static void sdfat_destroy_inodecache(void)
{
	kmem_cache_destroy(sdfat_inode_cachep);
}

#ifdef CONFIG_SDFAT_DBG_IOCTL
static void sdfat_debug_kill_sb(struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct block_device *bdev = sb->s_bdev;

	long flags;

	if (sbi) {
		flags = sbi->debug_flags;

		if (flags & SDFAT_DEBUGFLAGS_INVALID_UMOUNT) {
			/* invalidate_bdev drops all device cache include dirty.
			 * we use this to simulate device removal
			 */
			fsapi_cache_release(sb);
			invalidate_bdev(bdev);
		}
	}

	kill_block_super(sb);
}
#endif /* CONFIG_SDFAT_DBG_IOCTL */

static struct file_system_type sdfat_fs_type = {
	.owner       = THIS_MODULE,
	.name        = "sdfat",
	.mount       = sdfat_fs_mount,
#ifdef CONFIG_SDFAT_DBG_IOCTL
	.kill_sb    = sdfat_debug_kill_sb,
#else
	.kill_sb    = kill_block_super,
#endif /* CONFIG_SDFAT_DBG_IOCTL */
	.fs_flags    = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("sdfat");

#ifdef CONFIG_SDFAT_USE_FOR_EXFAT
static struct file_system_type exfat_fs_type = {
	.owner       = THIS_MODULE,
	.name        = "exfat",
	.mount       = sdfat_fs_mount,
#ifdef CONFIG_SDFAT_DBG_IOCTL
	.kill_sb    = sdfat_debug_kill_sb,
#else
	.kill_sb    = kill_block_super,
#endif /* CONFIG_SDFAT_DBG_IOCTL */
	.fs_flags    = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("exfat");
#endif /* CONFIG_SDFAT_USE_FOR_EXFAT */

#ifdef CONFIG_SDFAT_USE_FOR_VFAT
static struct file_system_type vfat_fs_type = {
	.owner       = THIS_MODULE,
	.name        = "vfat",
	.mount       = sdfat_fs_mount,
#ifdef CONFIG_SDFAT_DBG_IOCTL
	.kill_sb    = sdfat_debug_kill_sb,
#else
	.kill_sb    = kill_block_super,
#endif /* CONFIG_SDFAT_DBG_IOCTL */
	.fs_flags    = FS_REQUIRES_DEV,
};
MODULE_ALIAS_FS("vfat");
#endif /* CONFIG_SDFAT_USE_FOR_VFAT */

static int __init init_sdfat_fs(void)
{
	int err;

	sdfat_log_version();
	err = fsapi_init();
	if (err)
		goto error;

	sdfat_kset = kset_create_and_add("sdfat", NULL, fs_kobj);
	if (!sdfat_kset) {
		pr_err("[SDFAT] failed to create fs_kobj\n");
		err = -ENOMEM;
		goto error;
	}

	err = sysfs_create_group(&sdfat_kset->kobj, &attr_group);
	if (err) {
		pr_err("[SDFAT] failed to create sdfat version attributes\n");
		goto error;
	}

	err = sdfat_statistics_init(sdfat_kset);
	if (err)
		goto error;

	err = sdfat_init_inodecache();
	if (err) {
		pr_err("[SDFAT] failed to initialize inode cache\n");
		goto error;
	}

	err = register_filesystem(&sdfat_fs_type);
	if (err) {
		pr_err("[SDFAT] failed to register filesystem\n");
		goto error;
	}

#ifdef CONFIG_SDFAT_USE_FOR_EXFAT
	err = register_filesystem(&exfat_fs_type);
	if (err) {
		pr_err("[SDFAT] failed to register for exfat filesystem\n");
		goto error;
	}
#endif /* CONFIG_SDFAT_USE_FOR_EXFAT */

#ifdef CONFIG_SDFAT_USE_FOR_VFAT
	err = register_filesystem(&vfat_fs_type);
	if (err) {
		pr_err("[SDFAT] failed to register for vfat filesystem\n");
		goto error;
	}
#endif /* CONFIG_SDFAT_USE_FOR_VFAT */

	return 0;
error:
	sdfat_statistics_uninit();

	if (sdfat_kset) {
		sysfs_remove_group(&sdfat_kset->kobj, &attr_group);
		kset_unregister(sdfat_kset);
		sdfat_kset = NULL;
	}

	sdfat_destroy_inodecache();
	fsapi_shutdown();

	pr_err("[SDFAT] failed to initialize FS driver(err:%d)\n", err);
	return err;
}

static void __exit exit_sdfat_fs(void)
{
	sdfat_statistics_uninit();

	if (sdfat_kset) {
		sysfs_remove_group(&sdfat_kset->kobj, &attr_group);
		kset_unregister(sdfat_kset);
		sdfat_kset = NULL;
	}

	sdfat_destroy_inodecache();
	unregister_filesystem(&sdfat_fs_type);
#ifdef CONFIG_SDFAT_USE_FOR_EXFAT
	unregister_filesystem(&exfat_fs_type);
#endif /* CONFIG_SDFAT_USE_FOR_EXFAT */
#ifdef CONFIG_SDFAT_USE_FOR_VFAT
	unregister_filesystem(&vfat_fs_type);
#endif /* CONFIG_SDFAT_USE_FOR_VFAT */
	fsapi_shutdown();
}

module_init(init_sdfat_fs);
module_exit(exit_sdfat_fs);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FAT/exFAT filesystem support");
MODULE_AUTHOR("Samsung Electronics Co., Ltd.");


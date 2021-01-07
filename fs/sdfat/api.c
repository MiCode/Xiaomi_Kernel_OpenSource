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
/*  FILE    : sdfat_api.c                                               */
/*  PURPOSE : sdFAT volume lock layer                                   */
/*                                                                      */
/************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mutex.h>

#include "version.h"
#include "config.h"

#include "sdfat.h"
#include "core.h"

/*----------------------------------------------------------------------*/
/*  Internal structures                                                 */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*  Constant & Macro Definitions                                        */
/*----------------------------------------------------------------------*/
static DEFINE_MUTEX(_lock_core);

/*----------------------------------------------------------------------*/
/*  Global Variable Definitions                                         */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*  Local Variable Definitions                                          */
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*  Local Function Declarations                                         */
/*----------------------------------------------------------------------*/

/*======================================================================*/
/*  Global Function Definitions                                         */
/*    - All functions for global use have same return value format,     */
/*      that is, 0 on success and minus error number on                 */
/*      various error condition.                                        */
/*======================================================================*/

/*----------------------------------------------------------------------*/
/*  sdFAT Filesystem Init & Exit Functions                              */
/*----------------------------------------------------------------------*/

s32 fsapi_init(void)
{
	return fscore_init();
}

s32 fsapi_shutdown(void)
{
	return fscore_shutdown();
}

/*----------------------------------------------------------------------*/
/*  Volume Management Functions                                         */
/*----------------------------------------------------------------------*/

/* mount the file system volume */
s32 fsapi_mount(struct super_block *sb)
{
	s32 err;

	/* acquire the core lock for file system ccritical section */
	mutex_lock(&_lock_core);

	err = meta_cache_init(sb);
	if (err)
		goto out;

	err = fscore_mount(sb);
out:
	if (err)
		meta_cache_shutdown(sb);

	/* release the core lock for file system critical section */
	mutex_unlock(&_lock_core);

	return err;
}
EXPORT_SYMBOL(fsapi_mount);

/* unmount the file system volume */
s32 fsapi_umount(struct super_block *sb)
{
	s32 err;

	/* acquire the core lock for file system ccritical section */
	mutex_lock(&_lock_core);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_umount(sb);
	meta_cache_shutdown(sb);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));

	/* release the core lock for file system critical section */
	mutex_unlock(&_lock_core);

	return err;
}
EXPORT_SYMBOL(fsapi_umount);

/* get the information of a file system volume */
s32 fsapi_statfs(struct super_block *sb, VOL_INFO_T *info)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	/* check the validity of pointer parameters */
	ASSERT(info);

	if (fsi->used_clusters == (u32) ~0) {
		s32 err;

		mutex_lock(&(SDFAT_SB(sb)->s_vlock));
		err = fscore_statfs(sb, info);
		mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
		return err;
	}

	info->FatType = fsi->vol_type;
	info->ClusterSize = fsi->cluster_size;
	info->NumClusters = fsi->num_clusters - 2; /* clu 0 & 1 */
	info->UsedClusters = fsi->used_clusters + fsi->reserved_clusters;
	info->FreeClusters = info->NumClusters - info->UsedClusters;

	return 0;
}
EXPORT_SYMBOL(fsapi_statfs);

/* synchronize a file system volume */
s32 fsapi_sync_fs(struct super_block *sb, s32 do_sync)
{
	s32 err;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_sync_fs(sb, do_sync);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_sync_fs);

s32 fsapi_set_vol_flags(struct super_block *sb, u16 new_flag, s32 always_sync)
{
	s32 err;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_set_vol_flags(sb, new_flag, always_sync);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_set_vol_flags);

/*----------------------------------------------------------------------*/
/*  File Operation Functions                                            */
/*----------------------------------------------------------------------*/

/* lookup */
s32 fsapi_lookup(struct inode *inode, u8 *path, FILE_ID_T *fid)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid && path);

	if (unlikely(!strlen(path)))
		return -EINVAL;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_lookup(inode, path, fid);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_lookup);

/* create a file */
s32 fsapi_create(struct inode *inode, u8 *path, u8 mode, FILE_ID_T *fid)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid && path);

	if (unlikely(!strlen(path)))
		return -EINVAL;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_create(inode, path, mode, fid);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_create);

/* read the target string of symlink */
s32 fsapi_read_link(struct inode *inode, FILE_ID_T *fid, void *buffer, u64 count, u64 *rcount)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid && buffer);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_read_link(inode, fid, buffer, count, rcount);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_read_link);

/* write the target string of symlink */
s32 fsapi_write_link(struct inode *inode, FILE_ID_T *fid, void *buffer, u64 count, u64 *wcount)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid && buffer);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_write_link(inode, fid, buffer, count, wcount);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_write_link);

/* resize the file length */
s32 fsapi_truncate(struct inode *inode, u64 old_size, u64 new_size)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	TMSG("%s entered (inode %p size %llu)\n", __func__, inode, new_size);
	err = fscore_truncate(inode, old_size, new_size);
	TMSG("%s exitted (%d)\n", __func__, err);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_truncate);

/* rename or move a old file into a new file */
s32 fsapi_rename(struct inode *old_parent_inode, FILE_ID_T *fid,
		struct inode *new_parent_inode, struct dentry *new_dentry)
{
	s32 err;
	struct super_block *sb = old_parent_inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_rename(old_parent_inode, fid, new_parent_inode, new_dentry);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_rename);

/* remove a file */
s32 fsapi_remove(struct inode *inode, FILE_ID_T *fid)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_remove(inode, fid);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_remove);

/* get the information of a given file */
s32 fsapi_read_inode(struct inode *inode, DIR_ENTRY_T *info)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	TMSG("%s entered (inode %p info %p\n", __func__, inode, info);
	err = fscore_read_inode(inode, info);
	TMSG("%s exited (err:%d)\n", __func__, err);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_read_inode);

/* set the information of a given file */
s32 fsapi_write_inode(struct inode *inode, DIR_ENTRY_T *info, int sync)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	TMSG("%s entered (inode %p info %p sync:%d\n",
			__func__, inode, info, sync);
	err = fscore_write_inode(inode, info, sync);
	TMSG("%s exited (err:%d)\n", __func__, err);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_write_inode);

/* return the cluster number in the given cluster offset */
s32 fsapi_map_clus(struct inode *inode, u32 clu_offset, u32 *clu, int dest)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(clu);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	TMSG("%s entered (inode:%p clus:%08x dest:%d\n",
				__func__, inode, *clu, dest);
	err = fscore_map_clus(inode, clu_offset, clu, dest);
	TMSG("%s exited (clu:%08x err:%d)\n", __func__, *clu, err);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_map_clus);

/* reserve a cluster */
s32 fsapi_reserve_clus(struct inode *inode)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	TMSG("%s entered (inode:%p)\n", __func__, inode);
	err = fscore_reserve_clus(inode);
	TMSG("%s exited (err:%d)\n", __func__, err);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_reserve_clus);

/*----------------------------------------------------------------------*/
/*  Directory Operation Functions                                       */
/*----------------------------------------------------------------------*/

/* create(make) a directory */
s32 fsapi_mkdir(struct inode *inode, u8 *path, FILE_ID_T *fid)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid && path);

	if (unlikely(!strlen(path)))
		return -EINVAL;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_mkdir(inode, path, fid);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_mkdir);

/* read a directory entry from the opened directory */
s32 fsapi_readdir(struct inode *inode, DIR_ENTRY_T *dir_entry)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(dir_entry);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_readdir(inode, dir_entry);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_readdir);

/* remove a directory */
s32 fsapi_rmdir(struct inode *inode, FILE_ID_T *fid)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_rmdir(inode, fid);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_rmdir);

/* unlink a file.
 * that is, remove an entry from a directory. BUT don't truncate
 */
s32 fsapi_unlink(struct inode *inode, FILE_ID_T *fid)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(fid);
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = fscore_unlink(inode, fid);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_unlink);

/* reflect the internal dirty flags to VFS bh dirty flags */
s32 fsapi_cache_flush(struct super_block *sb, int do_sync)
{
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	fcache_flush(sb, do_sync);
	dcache_flush(sb, do_sync);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return 0;
}
EXPORT_SYMBOL(fsapi_cache_flush);

/* release FAT & buf cache */
s32 fsapi_cache_release(struct super_block *sb)
{
#ifdef CONFIG_SDFAT_DEBUG
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));

	fcache_release_all(sb);
	dcache_release_all(sb);

	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
#endif /* CONFIG_SDFAT_DEBUG */
	return 0;
}
EXPORT_SYMBOL(fsapi_cache_release);

u32 fsapi_get_au_stat(struct super_block *sb, s32 mode)
{
	/* volume lock is not required */
	return fscore_get_au_stat(sb, mode);
}
EXPORT_SYMBOL(fsapi_get_au_stat);

/* clear extent cache */
void fsapi_invalidate_extent(struct inode *inode)
{
	/* Volume lock is not required,
	 * because it is only called by evict_inode.
	 * If any other function can call it,
	 * you should check whether volume lock is needed or not.
	 */
	extent_cache_inval_inode(inode);
}
EXPORT_SYMBOL(fsapi_invalidate_extent);

/* check device is ejected */
s32 fsapi_check_bdi_valid(struct super_block *sb)
{
	return fscore_check_bdi_valid(sb);
}
EXPORT_SYMBOL(fsapi_check_bdi_valid);



#ifdef	CONFIG_SDFAT_DFR
/*----------------------------------------------------------------------*/
/*  Defragmentation related                                             */
/*----------------------------------------------------------------------*/
s32 fsapi_dfr_get_info(struct super_block *sb, void *arg)
{
	/* volume lock is not required */
	return defrag_get_info(sb, (struct defrag_info_arg *)arg);
}
EXPORT_SYMBOL(fsapi_dfr_get_info);

s32 fsapi_dfr_scan_dir(struct super_block *sb, void *args)
{
	s32 err;

	/* check the validity of pointer parameters */
	ASSERT(args);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = defrag_scan_dir(sb, (struct defrag_trav_arg *)args);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_dfr_scan_dir);

s32 fsapi_dfr_validate_clus(struct inode *inode, void *chunk, int skip_prev)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = defrag_validate_cluster(inode,
		(struct defrag_chunk_info *)chunk, skip_prev);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_dfr_validate_clus);

s32 fsapi_dfr_reserve_clus(struct super_block *sb, s32 nr_clus)
{
	s32 err;

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = defrag_reserve_clusters(sb, nr_clus);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
	return err;
}
EXPORT_SYMBOL(fsapi_dfr_reserve_clus);

s32 fsapi_dfr_mark_ignore(struct super_block *sb, unsigned int clus)
{
	/* volume lock is not required */
	return defrag_mark_ignore(sb, clus);
}
EXPORT_SYMBOL(fsapi_dfr_mark_ignore);

void fsapi_dfr_unmark_ignore_all(struct super_block *sb)
{
	/* volume lock is not required */
	defrag_unmark_ignore_all(sb);
}
EXPORT_SYMBOL(fsapi_dfr_unmark_ignore_all);

s32 fsapi_dfr_map_clus(struct inode *inode, u32 clu_offset, u32 *clu)
{
	s32 err;
	struct super_block *sb = inode->i_sb;

	/* check the validity of pointer parameters */
	ASSERT(clu);

	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	err = defrag_map_cluster(inode, clu_offset, clu);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));

	return err;
}
EXPORT_SYMBOL(fsapi_dfr_map_clus);

void fsapi_dfr_writepage_endio(struct page *page)
{
	/* volume lock is not required */
	defrag_writepage_end_io(page);
}
EXPORT_SYMBOL(fsapi_dfr_writepage_endio);

void fsapi_dfr_update_fat_prev(struct super_block *sb, int force)
{
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	defrag_update_fat_prev(sb, force);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
}
EXPORT_SYMBOL(fsapi_dfr_update_fat_prev);

void fsapi_dfr_update_fat_next(struct super_block *sb)
{
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	defrag_update_fat_next(sb);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
}
EXPORT_SYMBOL(fsapi_dfr_update_fat_next);

void fsapi_dfr_check_discard(struct super_block *sb)
{
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	defrag_check_discard(sb);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
}
EXPORT_SYMBOL(fsapi_dfr_check_discard);

void fsapi_dfr_free_clus(struct super_block *sb, u32 clus)
{
	mutex_lock(&(SDFAT_SB(sb)->s_vlock));
	defrag_free_cluster(sb, clus);
	mutex_unlock(&(SDFAT_SB(sb)->s_vlock));
}
EXPORT_SYMBOL(fsapi_dfr_free_clus);

s32 fsapi_dfr_check_dfr_required(struct super_block *sb, int *totalau, int *cleanau, int *fullau)
{
	/* volume lock is not required */
	return defrag_check_defrag_required(sb, totalau, cleanau, fullau);
}
EXPORT_SYMBOL(fsapi_dfr_check_dfr_required);

s32 fsapi_dfr_check_dfr_on(struct inode *inode, loff_t start, loff_t end, s32 cancel, const char *caller)
{
	/* volume lock is not required */
	return defrag_check_defrag_on(inode, start, end, cancel, caller);
}
EXPORT_SYMBOL(fsapi_dfr_check_dfr_on);



#ifdef CONFIG_SDFAT_DFR_DEBUG
void fsapi_dfr_spo_test(struct super_block *sb, int flag, const char *caller)
{
	/* volume lock is not required */
	defrag_spo_test(sb, flag, caller);
}
EXPORT_SYMBOL(fsapi_dfr_spo_test);
#endif


#endif	/* CONFIG_SDFAT_DFR */

/* end of sdfat_api.c */

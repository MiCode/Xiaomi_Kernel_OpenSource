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

#ifndef _SDFAT_CORE_H
#define _SDFAT_CORE_H

#include <asm/byteorder.h>

#include "config.h"
#include "api.h"
#include "upcase.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*----------------------------------------------------------------------*/
/*  Constant & Macro Definitions                                        */
/*----------------------------------------------------------------------*/
#define get_next_clus(sb, pclu)		fat_ent_get(sb, *(pclu), pclu)
#define get_next_clus_safe(sb, pclu)	fat_ent_get_safe(sb, *(pclu), pclu)

/* file status */
/* this prevents
 * fscore_write_inode, fscore_map_clus, ... with the unlinked inodes
 * from corrupting on-disk dentry data.
 *
 * The fid->dir value of unlinked inode will be DIR_DELETED
 * and those functions must check if fid->dir is valid prior to
 * the calling of get_dentry_in_dir()
 */
#define DIR_DELETED				0xFFFF0321

/*----------------------------------------------------------------------*/
/*  Type Definitions                                                    */
/*----------------------------------------------------------------------*/
#define ES_2_ENTRIES		2
#define ES_3_ENTRIES		3
#define ES_ALL_ENTRIES	0

typedef struct {
	u64	sector;		// sector number that contains file_entry
	u32	offset;		// byte offset in the sector
	s32	alloc_flag;	// flag in stream entry. 01 for cluster chain, 03 for contig. clusters.
	u32	num_entries;
	void	*__buf;		// __buf should be the last member
} ENTRY_SET_CACHE_T;



/*----------------------------------------------------------------------*/
/*  External Function Declarations                                      */
/*----------------------------------------------------------------------*/

/* file system initialization & shutdown functions */
s32 fscore_init(void);
s32 fscore_shutdown(void);

/* bdev management */
s32 fscore_check_bdi_valid(struct super_block *sb);

/* chain management */
s32 chain_cont_cluster(struct super_block *sb, u32 chain, u32 len);

/* volume management functions */
s32 fscore_mount(struct super_block *sb);
s32 fscore_umount(struct super_block *sb);
s32 fscore_statfs(struct super_block *sb, VOL_INFO_T *info);
s32 fscore_sync_fs(struct super_block *sb, s32 do_sync);
s32 fscore_set_vol_flags(struct super_block *sb, u16 new_flag, s32 always_sync);
u32 fscore_get_au_stat(struct super_block *sb, s32 mode);

/* file management functions */
s32 fscore_lookup(struct inode *inode, u8 *path, FILE_ID_T *fid);
s32 fscore_create(struct inode *inode, u8 *path, u8 mode, FILE_ID_T *fid);
s32 fscore_read_link(struct inode *inode, FILE_ID_T *fid, void *buffer, u64 count, u64 *rcount);
s32 fscore_write_link(struct inode *inode, FILE_ID_T *fid, void *buffer, u64 count, u64 *wcount);
s32 fscore_truncate(struct inode *inode, u64 old_size, u64 new_size);
s32 fscore_rename(struct inode *old_parent_inode, FILE_ID_T *fid,
		struct inode *new_parent_inode, struct dentry *new_dentry);
s32 fscore_remove(struct inode *inode, FILE_ID_T *fid);
s32 fscore_read_inode(struct inode *inode, DIR_ENTRY_T *info);
s32 fscore_write_inode(struct inode *inode, DIR_ENTRY_T *info, int sync);
s32 fscore_map_clus(struct inode *inode, u32 clu_offset, u32 *clu, int dest);
s32 fscore_reserve_clus(struct inode *inode);
s32 fscore_unlink(struct inode *inode, FILE_ID_T *fid);

/* directory management functions */
s32 fscore_mkdir(struct inode *inode, u8 *path, FILE_ID_T *fid);
s32 fscore_readdir(struct inode *inode, DIR_ENTRY_T *dir_ent);
s32 fscore_rmdir(struct inode *inode, FILE_ID_T *fid);


/*----------------------------------------------------------------------*/
/*  External Function Declarations (NOT TO UPPER LAYER)                 */
/*----------------------------------------------------------------------*/

/* core.c : core code for common */
/* dir entry management functions */
DENTRY_T *get_dentry_in_dir(struct super_block *sb, CHAIN_T *p_dir, s32 entry, u64 *sector);

/* name conversion functions */
void get_uniname_from_dos_entry(struct super_block *sb, DOS_DENTRY_T *ep, UNI_NAME_T *p_uniname, u8 mode);

/* file operation functions */
s32 walk_fat_chain(struct super_block *sb, CHAIN_T *p_dir, u32 byte_offset, u32 *clu);

/* sdfat/cache.c */
s32  meta_cache_init(struct super_block *sb);
s32  meta_cache_shutdown(struct super_block *sb);
u8 *fcache_getblk(struct super_block *sb, u64 sec);
s32  fcache_modify(struct super_block *sb, u64 sec);
s32  fcache_release_all(struct super_block *sb);
s32  fcache_flush(struct super_block *sb, u32 sync);

u8 *dcache_getblk(struct super_block *sb, u64 sec);
s32   dcache_modify(struct super_block *sb, u64 sec);
s32   dcache_lock(struct super_block *sb, u64 sec);
s32   dcache_unlock(struct super_block *sb, u64 sec);
s32   dcache_release(struct super_block *sb, u64 sec);
s32   dcache_release_all(struct super_block *sb);
s32   dcache_flush(struct super_block *sb, u32 sync);
s32   dcache_readahead(struct super_block *sb, u64 sec);


/* fatent.c */
s32 fat_ent_ops_init(struct super_block *sb);
s32 fat_ent_get(struct super_block *sb, u32 loc, u32 *content);
s32 fat_ent_set(struct super_block *sb, u32 loc, u32 content);
s32 fat_ent_get_safe(struct super_block *sb, u32 loc, u32 *content);

/* core_fat.c : core code for fat */
s32 fat_generate_dos_name_new(struct super_block *sb, CHAIN_T *p_dir, DOS_NAME_T *p_dosname, s32 n_entries);
s32  mount_fat16(struct super_block *sb, pbr_t *p_pbr);
s32  mount_fat32(struct super_block *sb, pbr_t *p_pbr);

/* core_exfat.c : core code for exfat */

s32 load_alloc_bmp(struct super_block *sb);
void free_alloc_bmp(struct super_block *sb);
ENTRY_SET_CACHE_T *get_dentry_set_in_dir(struct super_block *sb,
		CHAIN_T *p_dir, s32 entry, u32 type, DENTRY_T **file_ep);
void release_dentry_set(ENTRY_SET_CACHE_T *es);
s32 update_dir_chksum(struct super_block *sb, CHAIN_T *p_dir, s32 entry);
s32 update_dir_chksum_with_entry_set(struct super_block *sb, ENTRY_SET_CACHE_T *es);
bool is_dir_empty(struct super_block *sb, CHAIN_T *p_dir);
s32  mount_exfat(struct super_block *sb, pbr_t *p_pbr);

/* amap_smart.c :  creation on mount / destroy on umount */
int amap_create(struct super_block *sb, u32 pack_ratio, u32 sect_per_au, u32 hidden_sect);
void amap_destroy(struct super_block *sb);

/* amap_smart.c : (de)allocation functions */
s32 amap_fat_alloc_cluster(struct super_block *sb, u32 num_alloc, CHAIN_T *p_chain, s32 dest);
s32 amap_free_cluster(struct super_block *sb, CHAIN_T *p_chain, s32 do_relse);/* Not impelmented */
s32 amap_release_cluster(struct super_block *sb, u32 clu); /* Only update AMAP */

/* amap_smart.c : misc (for defrag) */
s32 amap_mark_ignore(struct super_block *sb, u32 clu);
s32 amap_unmark_ignore(struct super_block *sb, u32 clu);
s32 amap_unmark_ignore_all(struct super_block *sb);
s32 amap_check_working(struct super_block *sb, u32 clu);
s32 amap_get_freeclus(struct super_block *sb, u32 clu);

/* amap_smart.c : stat AU */
u32 amap_get_au_stat(struct super_block *sb, s32 mode);


/* blkdev.c */
s32 bdev_open_dev(struct super_block *sb);
s32 bdev_close_dev(struct super_block *sb);
s32 bdev_check_bdi_valid(struct super_block *sb);
s32 bdev_readahead(struct super_block *sb, u64 secno, u64 num_secs);
s32 bdev_mread(struct super_block *sb, u64 secno, struct buffer_head **bh, u64 num_secs, s32 read);
s32 bdev_mwrite(struct super_block *sb, u64 secno, struct buffer_head *bh, u64 num_secs, s32 sync);
s32 bdev_sync_all(struct super_block *sb);

/* blkdev.c : sector read/write functions */
s32 read_sect(struct super_block *sb, u64 sec, struct buffer_head **bh, s32 read);
s32 write_sect(struct super_block *sb, u64 sec, struct buffer_head *bh, s32 sync);
s32 read_msect(struct super_block *sb, u64 sec, struct buffer_head **bh, s64 num_secs, s32 read);
s32 write_msect(struct super_block *sb, u64 sec, struct buffer_head *bh, s64 num_secs, s32 sync);
s32 write_msect_zero(struct super_block *sb, u64 sec, u64 num_secs);

/* misc.c */
u8  calc_chksum_1byte(void *data, s32 len, u8 chksum);
u16 calc_chksum_2byte(void *data, s32 len, u16 chksum, s32 type);

/* extent.c */
s32 extent_cache_init(void);
void extent_cache_shutdown(void);
void extent_cache_init_inode(struct inode *inode);
void extent_cache_inval_inode(struct inode *inode);
s32 extent_get_clus(struct inode *inode, u32 cluster, u32 *fclus,
		u32 *dclus, u32 *last_dclus, s32 allow_eof);
/*----------------------------------------------------------------------*/
/*  Wrapper Function                                                    */
/*----------------------------------------------------------------------*/
void	set_sb_dirty(struct super_block *sb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _SDFAT_CORE_H */

/* end of core.h */

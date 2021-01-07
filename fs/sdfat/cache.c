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
/*  FILE    : cache.c                                                   */
/*  PURPOSE : sdFAT Cache Manager                                       */
/*            (FAT Cache & Buffer Cache)                                */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/swap.h> /* for mark_page_accessed() */
#include <asm/unaligned.h>

#include "sdfat.h"
#include "core.h"

#define DEBUG_HASH_LIST
#define DEBUG_HASH_PREV	(0xAAAA5555)
#define DEBUG_HASH_NEXT	(0x5555AAAA)

/*----------------------------------------------------------------------*/
/*  Global Variable Definitions                                         */
/*----------------------------------------------------------------------*/
/* All buffer structures are protected w/ fsi->v_sem */

/*----------------------------------------------------------------------*/
/*  Local Variable Definitions                                          */
/*----------------------------------------------------------------------*/
#define LOCKBIT         (0x01)
#define DIRTYBIT        (0x02)
#define KEEPBIT         (0x04)

/*----------------------------------------------------------------------*/
/*  Cache handling function declarations                                */
/*----------------------------------------------------------------------*/
static cache_ent_t *__fcache_find(struct super_block *sb, u64 sec);
static cache_ent_t *__fcache_get(struct super_block *sb);
static void __fcache_insert_hash(struct super_block *sb, cache_ent_t *bp);
static void __fcache_remove_hash(cache_ent_t *bp);

static cache_ent_t *__dcache_find(struct super_block *sb, u64 sec);
static cache_ent_t *__dcache_get(struct super_block *sb);
static void __dcache_insert_hash(struct super_block *sb, cache_ent_t *bp);
static void __dcache_remove_hash(cache_ent_t *bp);

/*----------------------------------------------------------------------*/
/*  Static functions                                                    */
/*----------------------------------------------------------------------*/
static void push_to_mru(cache_ent_t *bp, cache_ent_t *list)
{
	bp->next = list->next;
	bp->prev = list;
	list->next->prev = bp;
	list->next = bp;
}

static void push_to_lru(cache_ent_t *bp, cache_ent_t *list)
{
	bp->prev = list->prev;
	bp->next = list;
	list->prev->next = bp;
	list->prev = bp;
}

static void move_to_mru(cache_ent_t *bp, cache_ent_t *list)
{
	bp->prev->next = bp->next;
	bp->next->prev = bp->prev;
	push_to_mru(bp, list);
}

static void move_to_lru(cache_ent_t *bp, cache_ent_t *list)
{
	bp->prev->next = bp->next;
	bp->next->prev = bp->prev;
	push_to_lru(bp, list);
}

static inline s32 __check_hash_valid(cache_ent_t *bp)
{
#ifdef DEBUG_HASH_LIST
	if ((bp->hash.next == (cache_ent_t *)DEBUG_HASH_NEXT) ||
		(bp->hash.prev == (cache_ent_t *)DEBUG_HASH_PREV)) {
		return -EINVAL;
	}
#endif
	if ((bp->hash.next == bp) || (bp->hash.prev == bp))
		return -EINVAL;

	return 0;
}

static inline void __remove_from_hash(cache_ent_t *bp)
{
	(bp->hash.prev)->hash.next = bp->hash.next;
	(bp->hash.next)->hash.prev = bp->hash.prev;
	bp->hash.next = bp;
	bp->hash.prev = bp;
#ifdef DEBUG_HASH_LIST
	bp->hash.next = (cache_ent_t *)DEBUG_HASH_NEXT;
	bp->hash.prev = (cache_ent_t *)DEBUG_HASH_PREV;
#endif
}

/* Do FAT mirroring (don't sync)
 * sec: sector No. in FAT1
 * bh:  bh of sec.
 */
static inline s32 __fat_copy(struct super_block *sb, u64 sec, struct buffer_head *bh, int sync)
{
#ifdef CONFIG_SDFAT_FAT_MIRRORING
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	u64 sec2;

	if (fsi->FAT2_start_sector != fsi->FAT1_start_sector) {
		sec2 = sec - fsi->FAT1_start_sector + fsi->FAT2_start_sector;
		BUG_ON(sec2 != (sec + (u64)fsi->num_FAT_sectors));

		MMSG("BD: fat mirroring (%llu in FAT1, %llu in FAT2)\n", sec, sec2);
		if (write_sect(sb, sec2, bh, sync))
			return -EIO;
	}
#else
	/* DO NOTHING */
#endif
	return 0;
} /* end of __fat_copy */

/*
 * returns 1, if bp is flushed
 * returns 0, if bp is not dirty
 * returns -1, if error occurs
 */
static s32 __fcache_ent_flush(struct super_block *sb, cache_ent_t *bp, u32 sync)
{
	if (!(bp->flag & DIRTYBIT))
		return 0;
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	// Make buffer dirty (XXX: Naive impl.)
	if (write_sect(sb, bp->sec, bp->bh, 0))
		return -EIO;

	if (__fat_copy(sb, bp->sec, bp->bh, 0))
		return -EIO;
#endif
	bp->flag &= ~(DIRTYBIT);

	if (sync)
		sync_dirty_buffer(bp->bh);

	return 1;
}

static s32 __fcache_ent_discard(struct super_block *sb, cache_ent_t *bp)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	__fcache_remove_hash(bp);
	bp->sec = ~0;
	bp->flag = 0;

	if (bp->bh) {
		__brelse(bp->bh);
		bp->bh = NULL;
	}
	move_to_lru(bp, &fsi->fcache.lru_list);
	return 0;
}

u8 *fcache_getblk(struct super_block *sb, u64 sec)
{
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	u32 page_ra_count = FCACHE_MAX_RA_SIZE >> sb->s_blocksize_bits;

	bp = __fcache_find(sb, sec);
	if (bp) {
		if (bdev_check_bdi_valid(sb)) {
			__fcache_ent_flush(sb, bp, 0);
			__fcache_ent_discard(sb, bp);
			return NULL;
		}
		move_to_mru(bp, &fsi->fcache.lru_list);
		return bp->bh->b_data;
	}

	bp = __fcache_get(sb);
	if (!__check_hash_valid(bp))
		__fcache_remove_hash(bp);

	bp->sec = sec;
	bp->flag = 0;
	__fcache_insert_hash(sb, bp);

	/* Naive FAT read-ahead (increase I/O unit to page_ra_count) */
	if ((sec & (page_ra_count - 1)) == 0)
		bdev_readahead(sb, sec, (u64)page_ra_count);

	/*
	 * patch 1.2.4 : buffer_head null pointer exception problem.
	 *
	 * When read_sect is failed, fcache should be moved to
	 * EMPTY hash_list and the first of lru_list.
	 */
	if (read_sect(sb, sec, &(bp->bh), 1)) {
		__fcache_ent_discard(sb, bp);
		return NULL;
	}

	return bp->bh->b_data;
}

static inline int __mark_delayed_dirty(struct super_block *sb, cache_ent_t *bp)
{
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	if (fsi->vol_type == EXFAT)
		return -ENOTSUPP;

	bp->flag |= DIRTYBIT;
	return 0;
#else
	return -ENOTSUPP;
#endif
}



s32 fcache_modify(struct super_block *sb, u64 sec)
{
	cache_ent_t *bp;

	bp = __fcache_find(sb, sec);
	if (!bp) {
		sdfat_fs_error(sb, "Can`t find fcache (sec 0x%016llx)", sec);
		return -EIO;
	}

	if (!__mark_delayed_dirty(sb, bp))
		return 0;

	if (write_sect(sb, sec, bp->bh, 0))
		return -EIO;

	if (__fat_copy(sb, sec, bp->bh, 0))
		return -EIO;

	return 0;
}

/*======================================================================*/
/*  Cache Initialization Functions                                      */
/*======================================================================*/
s32 meta_cache_init(struct super_block *sb)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 i;

	/* LRU list */
	fsi->fcache.lru_list.next = &fsi->fcache.lru_list;
	fsi->fcache.lru_list.prev = fsi->fcache.lru_list.next;

	for (i = 0; i < FAT_CACHE_SIZE; i++) {
		fsi->fcache.pool[i].sec = ~0;
		fsi->fcache.pool[i].flag = 0;
		fsi->fcache.pool[i].bh = NULL;
		fsi->fcache.pool[i].prev = NULL;
		fsi->fcache.pool[i].next = NULL;
		push_to_mru(&(fsi->fcache.pool[i]), &fsi->fcache.lru_list);
	}

	fsi->dcache.lru_list.next = &fsi->dcache.lru_list;
	fsi->dcache.lru_list.prev = fsi->dcache.lru_list.next;
	fsi->dcache.keep_list.next = &fsi->dcache.keep_list;
	fsi->dcache.keep_list.prev = fsi->dcache.keep_list.next;

	// Initially, all the BUF_CACHEs are in the LRU list
	for (i = 0; i < BUF_CACHE_SIZE; i++) {
		fsi->dcache.pool[i].sec = ~0;
		fsi->dcache.pool[i].flag = 0;
		fsi->dcache.pool[i].bh = NULL;
		fsi->dcache.pool[i].prev = NULL;
		fsi->dcache.pool[i].next = NULL;
		push_to_mru(&(fsi->dcache.pool[i]), &fsi->dcache.lru_list);
	}

	/* HASH list */
	for (i = 0; i < FAT_CACHE_HASH_SIZE; i++) {
		fsi->fcache.hash_list[i].sec = ~0;
		fsi->fcache.hash_list[i].hash.next = &(fsi->fcache.hash_list[i]);
;
		fsi->fcache.hash_list[i].hash.prev = fsi->fcache.hash_list[i].hash.next;
	}

	for (i = 0; i < FAT_CACHE_SIZE; i++)
		__fcache_insert_hash(sb, &(fsi->fcache.pool[i]));

	for (i = 0; i < BUF_CACHE_HASH_SIZE; i++) {
		fsi->dcache.hash_list[i].sec = ~0;
		fsi->dcache.hash_list[i].hash.next = &(fsi->dcache.hash_list[i]);

		fsi->dcache.hash_list[i].hash.prev = fsi->dcache.hash_list[i].hash.next;
	}

	for (i = 0; i < BUF_CACHE_SIZE; i++)
		__dcache_insert_hash(sb, &(fsi->dcache.pool[i]));

	return 0;
}

s32 meta_cache_shutdown(struct super_block *sb)
{
	return 0;
}

/*======================================================================*/
/*  FAT Read/Write Functions                                            */
/*======================================================================*/
s32 fcache_release_all(struct super_block *sb)
{
	s32 ret = 0;
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 dirtycnt = 0;

	bp = fsi->fcache.lru_list.next;
	while (bp != &fsi->fcache.lru_list) {
		s32 ret_tmp = __fcache_ent_flush(sb, bp, 0);

		if (ret_tmp < 0)
			ret = ret_tmp;
		else
			dirtycnt += ret_tmp;

		bp->sec = ~0;
		bp->flag = 0;

		if (bp->bh) {
			__brelse(bp->bh);
			bp->bh = NULL;
		}
		bp = bp->next;
	}

	DMSG("BD:Release / dirty fat cache: %d (err:%d)\n", dirtycnt, ret);
	return ret;
}


/* internal DIRTYBIT marked => bh dirty */
s32 fcache_flush(struct super_block *sb, u32 sync)
{
	s32 ret = 0;
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 dirtycnt = 0;

	bp = fsi->fcache.lru_list.next;
	while (bp != &fsi->fcache.lru_list) {
		ret = __fcache_ent_flush(sb, bp, sync);
		if (ret < 0)
			break;

		dirtycnt += ret;
		bp = bp->next;
	}

	MMSG("BD: flush / dirty fat cache: %d (err:%d)\n", dirtycnt, ret);
	return ret;
}

static cache_ent_t *__fcache_find(struct super_block *sb, u64 sec)
{
	s32 off;
	cache_ent_t *bp, *hp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	off = (sec + (sec >> fsi->sect_per_clus_bits)) & (FAT_CACHE_HASH_SIZE - 1);
	hp = &(fsi->fcache.hash_list[off]);
	for (bp = hp->hash.next; bp != hp; bp = bp->hash.next) {
		if (bp->sec == sec) {
			/*
			 * patch 1.2.4 : for debugging
			 */
			WARN(!bp->bh, "[SDFAT] fcache has no bh. "
					  "It will make system panic.\n");

			touch_buffer(bp->bh);
			return bp;
		}
	}
	return NULL;
}

static cache_ent_t *__fcache_get(struct super_block *sb)
{
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	bp = fsi->fcache.lru_list.prev;
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	while (bp->flag & DIRTYBIT) {
		cache_ent_t *bp_prev = bp->prev;

		bp = bp_prev;
		if (bp == &fsi->fcache.lru_list) {
			DMSG("BD: fat cache flooding\n");
			fcache_flush(sb, 0);	// flush all dirty FAT caches
			bp = fsi->fcache.lru_list.prev;
		}
	}
#endif
//	if (bp->flag & DIRTYBIT)
//       sync_dirty_buffer(bp->bh);

	move_to_mru(bp, &fsi->fcache.lru_list);
	return bp;
}

static void __fcache_insert_hash(struct super_block *sb, cache_ent_t *bp)
{
	s32 off;
	cache_ent_t *hp;
	FS_INFO_T *fsi;

	fsi = &(SDFAT_SB(sb)->fsi);
	off = (bp->sec + (bp->sec >> fsi->sect_per_clus_bits)) & (FAT_CACHE_HASH_SIZE-1);

	hp = &(fsi->fcache.hash_list[off]);
	bp->hash.next = hp->hash.next;
	bp->hash.prev = hp;
	hp->hash.next->hash.prev = bp;
	hp->hash.next = bp;
}


static void __fcache_remove_hash(cache_ent_t *bp)
{
#ifdef DEBUG_HASH_LIST
	if ((bp->hash.next == (cache_ent_t *)DEBUG_HASH_NEXT) ||
		(bp->hash.prev == (cache_ent_t *)DEBUG_HASH_PREV)) {
		EMSG("%s: FATAL: tried to remove already-removed-cache-entry"
			"(bp:%p)\n", __func__, bp);
		return;
	}
#endif
	WARN_ON(bp->flag & DIRTYBIT);
	__remove_from_hash(bp);
}

/*======================================================================*/
/*  Buffer Read/Write Functions                                         */
/*======================================================================*/
/* Read-ahead a cluster */
s32 dcache_readahead(struct super_block *sb, u64 sec)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	struct buffer_head *bh;
	u32 max_ra_count = DCACHE_MAX_RA_SIZE >> sb->s_blocksize_bits;
	u32 page_ra_count = PAGE_SIZE >> sb->s_blocksize_bits;
	u32 adj_ra_count = max(fsi->sect_per_clus, page_ra_count);
	u32 ra_count = min(adj_ra_count, max_ra_count);

	/* Read-ahead is not required */
	if (fsi->sect_per_clus == 1)
		return 0;

	if (sec < fsi->data_start_sector) {
		EMSG("BD: %s: requested sector is invalid(sect:%llu, root:%llu)\n",
				__func__, sec, fsi->data_start_sector);
		return -EIO;
	}

	/* Not sector aligned with ra_count, resize ra_count to page size */
	if ((sec - fsi->data_start_sector) & (ra_count - 1))
		ra_count = page_ra_count;

	bh = sb_find_get_block(sb, sec);
	if (!bh || !buffer_uptodate(bh))
		bdev_readahead(sb, sec, (u64)ra_count);

	brelse(bh);

	return 0;
}

/*
 * returns 1, if bp is flushed
 * returns 0, if bp is not dirty
 * returns -1, if error occurs
 */
static s32 __dcache_ent_flush(struct super_block *sb, cache_ent_t *bp, u32 sync)
{
	if (!(bp->flag & DIRTYBIT))
		return 0;
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	// Make buffer dirty (XXX: Naive impl.)
	if (write_sect(sb, bp->sec, bp->bh, 0))
		return -EIO;
#endif
	bp->flag &= ~(DIRTYBIT);

	if (sync)
		sync_dirty_buffer(bp->bh);

	return 1;
}

static s32 __dcache_ent_discard(struct super_block *sb, cache_ent_t *bp)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	MMSG("%s : bp[%p] (sec:%016llx flag:%08x bh:%p) list(prev:%p next:%p) "
		"hash(prev:%p next:%p)\n", __func__,
		bp, bp->sec, bp->flag, bp->bh, bp->prev, bp->next,
		bp->hash.prev, bp->hash.next);

	__dcache_remove_hash(bp);
	bp->sec = ~0;
	bp->flag = 0;

	if (bp->bh) {
		__brelse(bp->bh);
		bp->bh = NULL;
	}

	move_to_lru(bp, &fsi->dcache.lru_list);
	return 0;
}

u8 *dcache_getblk(struct super_block *sb, u64 sec)
{
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	bp = __dcache_find(sb, sec);
	if (bp) {
		if (bdev_check_bdi_valid(sb)) {
			MMSG("%s: found cache(%p, sect:%llu). But invalid BDI\n"
				, __func__, bp, sec);
			__dcache_ent_flush(sb, bp, 0);
			__dcache_ent_discard(sb, bp);
			return NULL;
		}

		if (!(bp->flag & KEEPBIT))	// already in keep list
			move_to_mru(bp, &fsi->dcache.lru_list);

		return bp->bh->b_data;
	}

	bp = __dcache_get(sb);

	if (!__check_hash_valid(bp))
		__dcache_remove_hash(bp);

	bp->sec = sec;
	bp->flag = 0;
	__dcache_insert_hash(sb, bp);

	if (read_sect(sb, sec, &(bp->bh), 1)) {
		__dcache_ent_discard(sb, bp);
		return NULL;
	}

	return bp->bh->b_data;

}

s32 dcache_modify(struct super_block *sb, u64 sec)
{
	s32 ret = -EIO;
	cache_ent_t *bp;

	set_sb_dirty(sb);

	bp = __dcache_find(sb, sec);
	if (unlikely(!bp)) {
		sdfat_fs_error(sb, "Can`t find dcache (sec 0x%016llx)", sec);
		return -EIO;
	}
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	if (SDFAT_SB(sb)->fsi.vol_type != EXFAT) {
		bp->flag |= DIRTYBIT;
		return 0;
	}
#endif
	ret = write_sect(sb, sec, bp->bh, 0);

	if (ret) {
		DMSG("%s : failed to modify buffer(err:%d, sec:%llu, bp:0x%p)\n",
			__func__, ret, sec, bp);
	}

	return ret;
}

s32 dcache_lock(struct super_block *sb, u64 sec)
{
	cache_ent_t *bp;

	bp = __dcache_find(sb, sec);
	if (likely(bp)) {
		bp->flag |= LOCKBIT;
		return 0;
	}

	EMSG("%s : failed to lock buffer(sec:%llu, bp:0x%p)\n", __func__, sec, bp);
	return -EIO;
}

s32 dcache_unlock(struct super_block *sb, u64 sec)
{
	cache_ent_t *bp;

	bp = __dcache_find(sb, sec);
	if (likely(bp))  {
		bp->flag &= ~(LOCKBIT);
		return 0;
	}

	EMSG("%s : failed to unlock buffer (sec:%llu, bp:0x%p)\n", __func__, sec, bp);
	return -EIO;
}

s32 dcache_release(struct super_block *sb, u64 sec)
{
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	bp = __dcache_find(sb, sec);
	if (unlikely(!bp))
		return -ENOENT;

#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	if (bp->flag & DIRTYBIT) {
		if (write_sect(sb, bp->sec, bp->bh, 0))
			return -EIO;
	}
#endif
	bp->sec = ~0;
	bp->flag = 0;

	if (bp->bh) {
		__brelse(bp->bh);
		bp->bh = NULL;
	}

	move_to_lru(bp, &fsi->dcache.lru_list);
	return 0;
}

s32 dcache_release_all(struct super_block *sb)
{
	s32 ret = 0;
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 dirtycnt = 0;

	/* Connect list elements:
	 * LRU list : (A - B - ... - bp_front) + (bp_first + ... + bp_last)
	 */
	while (fsi->dcache.keep_list.prev != &fsi->dcache.keep_list) {
		cache_ent_t *bp_keep = fsi->dcache.keep_list.prev;
		// bp_keep->flag &= ~(KEEPBIT);		// Will be 0-ed later
		move_to_mru(bp_keep, &fsi->dcache.lru_list);
	}

	bp = fsi->dcache.lru_list.next;
	while (bp != &fsi->dcache.lru_list) {
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
		if (bp->flag & DIRTYBIT) {
			dirtycnt++;
			if (write_sect(sb, bp->sec, bp->bh, 0))
				ret = -EIO;
		}
#endif
		bp->sec = ~0;
		bp->flag = 0;

		if (bp->bh) {
			__brelse(bp->bh);
			bp->bh = NULL;
		}
		bp = bp->next;
	}

	DMSG("BD:Release / dirty buf cache: %d (err:%d)", dirtycnt, ret);
	return ret;
}


s32 dcache_flush(struct super_block *sb, u32 sync)
{
	s32 ret = 0;
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	s32 dirtycnt = 0;
	s32 keepcnt = 0;

	/* Connect list elements:
	 * LRU list : (A - B - ... - bp_front) + (bp_first + ... + bp_last)
	 */
	while (fsi->dcache.keep_list.prev != &fsi->dcache.keep_list) {
		cache_ent_t *bp_keep = fsi->dcache.keep_list.prev;

		bp_keep->flag &= ~(KEEPBIT);		// Will be 0-ed later
		move_to_mru(bp_keep, &fsi->dcache.lru_list);
		keepcnt++;
	}

	bp = fsi->dcache.lru_list.next;
	while (bp != &fsi->dcache.lru_list) {
		if (bp->flag & DIRTYBIT) {
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
			// Make buffer dirty (XXX: Naive impl.)
			if (write_sect(sb, bp->sec, bp->bh, 0)) {
				ret = -EIO;
				break;
			}

#endif
			bp->flag &= ~(DIRTYBIT);
			dirtycnt++;

			if (sync != 0)
				sync_dirty_buffer(bp->bh);
		}
		bp = bp->next;
	}

	MMSG("BD: flush / dirty dentry cache: %d (%d from keeplist, err:%d)\n",
						dirtycnt, keepcnt, ret);
	return ret;
}

static cache_ent_t *__dcache_find(struct super_block *sb, u64 sec)
{
	s32 off;
	cache_ent_t *bp, *hp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	off = (sec + (sec >> fsi->sect_per_clus_bits)) & (BUF_CACHE_HASH_SIZE - 1);

	hp = &(fsi->dcache.hash_list[off]);
	for (bp = hp->hash.next; bp != hp; bp = bp->hash.next) {
		if (bp->sec == sec) {
			touch_buffer(bp->bh);
			return bp;
		}
	}
	return NULL;
}

static cache_ent_t *__dcache_get(struct super_block *sb)
{
	cache_ent_t *bp;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	bp = fsi->dcache.lru_list.prev;
#ifdef CONFIG_SDFAT_DELAYED_META_DIRTY
	while (bp->flag & (DIRTYBIT | LOCKBIT)) {
		cache_ent_t *bp_prev = bp->prev; // hold prev

		if (bp->flag & DIRTYBIT) {
			MMSG("BD: Buf cache => Keep list\n");
			bp->flag |= KEEPBIT;
			move_to_mru(bp, &fsi->dcache.keep_list);
		}
		bp = bp_prev;

		/* If all dcaches are dirty */
		if (bp == &fsi->dcache.lru_list) {
			DMSG("BD: buf cache flooding\n");
			dcache_flush(sb, 0);
			bp = fsi->dcache.lru_list.prev;
		}
	}
#else
	while (bp->flag & LOCKBIT)
		bp = bp->prev;
#endif
//	if (bp->flag & DIRTYBIT)
//       sync_dirty_buffer(bp->bh);

	move_to_mru(bp, &fsi->dcache.lru_list);
	return bp;
}

static void __dcache_insert_hash(struct super_block *sb, cache_ent_t *bp)
{
	s32 off;
	cache_ent_t *hp;
	FS_INFO_T *fsi;

	fsi = &(SDFAT_SB(sb)->fsi);
	off = (bp->sec + (bp->sec >> fsi->sect_per_clus_bits)) & (BUF_CACHE_HASH_SIZE-1);

	hp = &(fsi->dcache.hash_list[off]);
	bp->hash.next = hp->hash.next;
	bp->hash.prev = hp;
	hp->hash.next->hash.prev = bp;
	hp->hash.next = bp;
}

static void __dcache_remove_hash(cache_ent_t *bp)
{
#ifdef DEBUG_HASH_LIST
	if ((bp->hash.next == (cache_ent_t *)DEBUG_HASH_NEXT) ||
		(bp->hash.prev == (cache_ent_t *)DEBUG_HASH_PREV)) {
		EMSG("%s: FATAL: tried to remove already-removed-cache-entry"
			"(bp:%p)\n", __func__, bp);
		return;
	}
#endif
	WARN_ON(bp->flag & DIRTYBIT);
	__remove_from_hash(bp);
}


/* end of cache.c */

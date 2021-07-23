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
/* @PROJECT : exFAT & FAT12/16/32 File System                           */
/* @FILE    : dfr.c                                                     */
/* @PURPOSE : Defragmentation support for SDFAT32                       */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/version.h>
#include <linux/list.h>
#include <linux/blkdev.h>

#include "sdfat.h"
#include "core.h"
#include "amap_smart.h"

#ifdef CONFIG_SDFAT_DFR
/**
 * @fn		defrag_get_info
 * @brief	get HW params for defrag daemon
 * @return	0 on success, -errno otherwise
 * @param	sb		super block
 * @param	arg		defrag info arguments
 * @remark	protected by super_block
 */
int
defrag_get_info(
	IN struct super_block *sb,
	OUT struct defrag_info_arg *arg)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;

	if (!arg)
		return -EINVAL;

	arg->sec_sz = sb->s_blocksize;
	arg->clus_sz = fsi->cluster_size;
	arg->total_sec = fsi->num_sectors;
	arg->fat_offset_sec = fsi->FAT1_start_sector;
	arg->fat_sz_sec = fsi->num_FAT_sectors;
	arg->n_fat = (fsi->FAT1_start_sector == fsi->FAT2_start_sector) ? 1 : 2;

	arg->sec_per_au = amap->option.au_size;
	arg->hidden_sectors = amap->option.au_align_factor % amap->option.au_size;

	return 0;
}


static int
__defrag_scan_dir(
	IN struct super_block *sb,
	IN DOS_DENTRY_T *dos_ep,
	IN loff_t i_pos,
	OUT struct defrag_trav_arg *arg)
{
	FS_INFO_T *fsi = NULL;
	UNI_NAME_T uniname;
	unsigned int type = 0, start_clus = 0;
	int err = -EPERM;

	/* Check params */
	ERR_HANDLE2((!sb || !dos_ep || !i_pos || !arg), err, -EINVAL);
	fsi = &(SDFAT_SB(sb)->fsi);

	/* Get given entry's type */
	type = fsi->fs_func->get_entry_type((DENTRY_T *) dos_ep);

	/* Check dos_ep */
	if (!strncmp(dos_ep->name, DOS_CUR_DIR_NAME, DOS_NAME_LENGTH)) {
		;
	} else if (!strncmp(dos_ep->name, DOS_PAR_DIR_NAME, DOS_NAME_LENGTH)) {
		;
	} else if ((type == TYPE_DIR) || (type == TYPE_FILE)) {

		/* Set start_clus */
		SET32_HI(start_clus, le16_to_cpu(dos_ep->start_clu_hi));
		SET32_LO(start_clus, le16_to_cpu(dos_ep->start_clu_lo));
		arg->start_clus = start_clus;

		/* Set type & i_pos */
		if (type == TYPE_DIR)
			arg->type = DFR_TRAV_TYPE_DIR;
		else
			arg->type = DFR_TRAV_TYPE_FILE;

		arg->i_pos = i_pos;

		/* Set name */
		memset(&uniname, 0, sizeof(UNI_NAME_T));
		get_uniname_from_dos_entry(sb, dos_ep, &uniname, 0x1);
		/* FIXME :
		 * we should think that whether the size of arg->name
		 * is enough or not
		 */
		nls_uni16s_to_vfsname(sb, &uniname,
			arg->name, sizeof(arg->name));

		err = 0;
	/* End case */
	} else if (type == TYPE_UNUSED) {
		err = -ENOENT;
	} else {
		;
	}

error:
	return err;
}


/**
 * @fn		defrag_scan_dir
 * @brief	scan given directory
 * @return	0 on success, -errno otherwise
 * @param	sb		super block
 * @param	args	traverse args
 * @remark	protected by inode_lock, super_block and volume lock
 */
int
defrag_scan_dir(
	IN struct super_block *sb,
	INOUT struct defrag_trav_arg *args)
{
	struct sdfat_sb_info *sbi = NULL;
	FS_INFO_T *fsi = NULL;
	struct defrag_trav_header *header = NULL;
	DOS_DENTRY_T *dos_ep;
	CHAIN_T chain;
	int dot_found = 0, args_idx = DFR_TRAV_HEADER_IDX + 1, clus = 0, index = 0;
	int err = 0, j = 0;

	/* Check params */
	ERR_HANDLE2((!sb || !args), err, -EINVAL);
	sbi = SDFAT_SB(sb);
	fsi = &(sbi->fsi);
	header = (struct defrag_trav_header *) args;

	/* Exceptional case for ROOT */
	if (header->i_pos == DFR_TRAV_ROOT_IPOS) {
		header->start_clus = fsi->root_dir;
		dfr_debug("IOC_DFR_TRAV for ROOT: start_clus %08x", header->start_clus);
		dot_found = 1;
	}

	chain.dir = header->start_clus;
	chain.size = 0;
	chain.flags = 0;

	/* Check if this is directory */
	if (!dot_found) {
		FAT32_CHECK_CLUSTER(fsi, chain.dir, err);
		ERR_HANDLE(err);
		dos_ep = (DOS_DENTRY_T *) get_dentry_in_dir(sb, &chain, 0, NULL);
		ERR_HANDLE2(!dos_ep, err, -EIO);

		if (strncmp(dos_ep->name, DOS_CUR_DIR_NAME, DOS_NAME_LENGTH)) {
			err = -EINVAL;
			dfr_err("Scan: Not a directory, err %d", err);
			goto error;
		}
	}

	/* For more-scan case */
	if ((header->stat == DFR_TRAV_STAT_MORE) &&
		(header->start_clus == sbi->dfr_hint_clus) &&
		(sbi->dfr_hint_idx > 0)) {

		index = sbi->dfr_hint_idx;
		for (j = 0; j < (sbi->dfr_hint_idx / fsi->dentries_per_clu); j++) {
			/* Follow FAT-chain */
			FAT32_CHECK_CLUSTER(fsi, chain.dir, err);
			ERR_HANDLE(err);
			err = fat_ent_get(sb, chain.dir, &(chain.dir));
			ERR_HANDLE(err);

			if (!IS_CLUS_EOF(chain.dir)) {
				clus++;
				index -= fsi->dentries_per_clu;
			} else {
				/**
				 * This directory modified. Stop scanning.
				 */
				err = -EINVAL;
				dfr_err("Scan: SCAN_MORE failed, err %d", err);
				goto error;
			}
		}

	/* For first-scan case */
	} else {
		clus = 0;
		index = 0;
	}

scan_fat_chain:
	/* Scan given directory and get info of children */
	for ( ; index < fsi->dentries_per_clu; index++) {
		DOS_DENTRY_T *dos_ep = NULL;
		loff_t i_pos = 0;

		/* Get dos_ep */
		FAT32_CHECK_CLUSTER(fsi, chain.dir, err);
		ERR_HANDLE(err);
		dos_ep = (DOS_DENTRY_T *) get_dentry_in_dir(sb, &chain, index, NULL);
		ERR_HANDLE2(!dos_ep, err, -EIO);

		/* Make i_pos for this entry */
		SET64_HI(i_pos, header->start_clus);
		SET64_LO(i_pos, clus * fsi->dentries_per_clu + index);

		err = __defrag_scan_dir(sb, dos_ep, i_pos, &args[args_idx]);
		if (!err) {
			/* More-scan case */
			if (++args_idx >= (PAGE_SIZE / sizeof(struct defrag_trav_arg))) {
				sbi->dfr_hint_clus = header->start_clus;
				sbi->dfr_hint_idx = clus * fsi->dentries_per_clu + index + 1;

				header->stat = DFR_TRAV_STAT_MORE;
				header->nr_entries = args_idx;
				goto error;
			}
		/* Error case */
		} else if (err == -EINVAL) {
			sbi->dfr_hint_clus = sbi->dfr_hint_idx = 0;
			dfr_err("Scan: err %d", err);
			goto error;
		/* End case */
		} else if (err == -ENOENT) {
			sbi->dfr_hint_clus = sbi->dfr_hint_idx = 0;
			err = 0;
			goto done;
		} else {
			/* DO NOTHING */
		}
		err = 0;
	}

	/* Follow FAT-chain */
	FAT32_CHECK_CLUSTER(fsi, chain.dir, err);
	ERR_HANDLE(err);
	err = fat_ent_get(sb, chain.dir, &(chain.dir));
	ERR_HANDLE(err);

	if (!IS_CLUS_EOF(chain.dir)) {
		index = 0;
		clus++;
		goto scan_fat_chain;
	}

done:
	/* Update header */
	header->stat = DFR_TRAV_STAT_DONE;
	header->nr_entries = args_idx;

error:
	return err;
}


static int
__defrag_validate_cluster_prev(
	IN struct super_block *sb,
	IN struct defrag_chunk_info *chunk)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	CHAIN_T dir;
	DENTRY_T *ep = NULL;
	unsigned int entry = 0, clus = 0;
	int err = 0;

	if (chunk->prev_clus == 0) {
		/* For the first cluster of a file */
		dir.dir = GET64_HI(chunk->i_pos);
		dir.flags = 0x1;	// Assume non-continuous

		entry = GET64_LO(chunk->i_pos);

		FAT32_CHECK_CLUSTER(fsi, dir.dir, err);
		ERR_HANDLE(err);
		ep = get_dentry_in_dir(sb, &dir, entry, NULL);
		if (!ep) {
			err = -EPERM;
			goto error;
		}

		/* should call fat_get_entry_clu0(ep) */
		clus = fsi->fs_func->get_entry_clu0(ep);
		if (clus != chunk->d_clus) {
			err = -ENXIO;
			goto error;
		}
	} else {
		/* Normal case */
		FAT32_CHECK_CLUSTER(fsi, chunk->prev_clus, err);
		ERR_HANDLE(err);
		err = fat_ent_get(sb, chunk->prev_clus, &clus);
		if (err)
			goto error;
		if (chunk->d_clus != clus)
			err = -ENXIO;
	}

error:
	return err;
}


static int
__defrag_validate_cluster_next(
	IN struct super_block *sb,
	IN struct defrag_chunk_info *chunk)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	unsigned int clus = 0;
	int err = 0;

	/* Check next_clus */
	FAT32_CHECK_CLUSTER(fsi, (chunk->d_clus + chunk->nr_clus - 1), err);
	ERR_HANDLE(err);
	err = fat_ent_get(sb, (chunk->d_clus + chunk->nr_clus - 1), &clus);
	if (err)
		goto error;
	if (chunk->next_clus != (clus & FAT32_EOF))
		err = -ENXIO;

error:
	return err;
}


/**
 * @fn		__defrag_check_au
 * @brief	check if this AU is in use
 * @return	0 if idle, 1 if busy
 * @param	sb		super block
 * @param	clus	physical cluster num
 * @param	limit	# of used clusters from daemon
 */
static int
__defrag_check_au(
	struct super_block *sb,
	u32 clus,
	u32 limit)
{
	unsigned int nr_free = amap_get_freeclus(sb, clus);

#if defined(CONFIG_SDFAT_DFR_DEBUG) && defined(CONFIG_SDFAT_DBG_MSG)
	if (nr_free < limit) {
		AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
		AU_INFO_T *au = GET_AU(amap, i_AU_of_CLU(amap, clus));

		dfr_debug("AU[%d] nr_free %d, limit %d", au->idx, nr_free, limit);
	}
#endif
	return ((nr_free < limit) ? 1 : 0);
}


/**
 * @fn		defrag_validate_cluster
 * @brief	validate cluster info of given chunk
 * @return	0 on success, -errno otherwise
 * @param	inode	inode of given chunk
 * @param	chunk	given chunk
 * @param	skip_prev	flag to skip checking previous cluster info
 * @remark	protected by super_block and volume lock
 */
int
defrag_validate_cluster(
	IN struct inode *inode,
	IN struct defrag_chunk_info *chunk,
	IN int skip_prev)
{
	struct super_block *sb = inode->i_sb;
	FILE_ID_T *fid = &(SDFAT_I(inode)->fid);
	unsigned int clus = 0;
	int err = 0, i = 0;

	/* If this inode is unlink-ed, skip it */
	if (fid->dir.dir == DIR_DELETED)
		return -ENOENT;

	/* Skip working-AU */
	err = amap_check_working(sb, chunk->d_clus);
	if (err)
		return -EBUSY;

	/* Check # of free_clus of belonged AU */
	err = __defrag_check_au(inode->i_sb, chunk->d_clus, CLUS_PER_AU(sb) - chunk->au_clus);
	if (err)
		return -EINVAL;

	/* Check chunk's clusters */
	for (i = 0; i < chunk->nr_clus; i++) {
		err = fsapi_map_clus(inode, chunk->f_clus + i, &clus, ALLOC_NOWHERE);
		if (err || (chunk->d_clus + i != clus)) {
			if (!err)
				err = -ENXIO;
			goto error;
		}
	}

	/* Check next_clus */
	err = __defrag_validate_cluster_next(sb, chunk);
	ERR_HANDLE(err);

	if (!skip_prev) {
		/* Check prev_clus */
		err = __defrag_validate_cluster_prev(sb, chunk);
		ERR_HANDLE(err);
	}

error:
	return err;
}


/**
 * @fn		defrag_reserve_clusters
 * @brief	reserve clusters for defrag
 * @return	0 on success, -errno otherwise
 * @param	sb			super block
 * @param	nr_clus		# of clusters to reserve
 * @remark	protected by super_block and volume lock
 */
int
defrag_reserve_clusters(
	INOUT struct super_block *sb,
	IN int nr_clus)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);

	if (!(sbi->options.improved_allocation & SDFAT_ALLOC_DELAY))
		/* Nothing to do */
		return 0;

	/* Check error case */
	if (fsi->used_clusters + fsi->reserved_clusters + nr_clus >= fsi->num_clusters - 2)  {
		return -ENOSPC;
	} else if (fsi->reserved_clusters + nr_clus < 0) {
		dfr_err("Reserve count: reserved_clusters %d, nr_clus %d",
				fsi->reserved_clusters, nr_clus);
		BUG_ON(fsi->reserved_clusters + nr_clus < 0);
	}

	sbi->dfr_reserved_clus += nr_clus;
	fsi->reserved_clusters += nr_clus;

	return 0;
}


/**
 * @fn		defrag_mark_ignore
 * @brief	mark corresponding AU to be ignored
 * @return	0 on success, -errno otherwise
 * @param	sb		super block
 * @param	clus	given cluster num
 * @remark	protected by super_block
 */
int
defrag_mark_ignore(
	INOUT struct super_block *sb,
	IN unsigned int clus)
{
	int err = 0;

	if (SDFAT_SB(sb)->options.improved_allocation & SDFAT_ALLOC_SMART)
		err = amap_mark_ignore(sb, clus);

	if (err)
		dfr_debug("err %d", err);
	return err;
}


/**
 * @fn		defrag_unmark_ignore_all
 * @brief	unmark all ignored AUs
 * @return	void
 * @param	sb		super block
 * @remark	protected by super_block
 */
void
defrag_unmark_ignore_all(struct super_block *sb)
{
	if (SDFAT_SB(sb)->options.improved_allocation & SDFAT_ALLOC_SMART)
		amap_unmark_ignore_all(sb);
}


/**
 * @fn		defrag_map_cluster
 * @brief	get_block function for defrag dests
 * @return	0 on success, -errno otherwise
 * @param	inode		inode
 * @param	clu_offset	logical cluster offset
 * @param	clu			mapped cluster (physical)
 * @remark	protected by super_block and volume lock
 */
int
defrag_map_cluster(
	struct inode *inode,
	unsigned int clu_offset,
	unsigned int *clu)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
#ifdef CONFIG_SDFAT_DFR_PACKING
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
#endif
	FILE_ID_T *fid = &(SDFAT_I(inode)->fid);
	struct defrag_info *ino_dfr = &(SDFAT_I(inode)->dfr_info);
	struct defrag_chunk_info *chunk = NULL;
	CHAIN_T new_clu;
	int i = 0, nr_new = 0, err = 0;

	/* Get corresponding chunk */
	for (i = 0; i < ino_dfr->nr_chunks; i++) {
		chunk = &(ino_dfr->chunks[i]);

		if ((chunk->f_clus <= clu_offset) && (clu_offset < chunk->f_clus + chunk->nr_clus)) {
			/* For already allocated new_clus */
			if (sbi->dfr_new_clus[chunk->new_idx + clu_offset - chunk->f_clus]) {
				*clu = sbi->dfr_new_clus[chunk->new_idx + clu_offset - chunk->f_clus];
				return 0;
			}
			break;
		}
	}
	BUG_ON(!chunk);

	fscore_set_vol_flags(sb, VOL_DIRTY, 0);

	new_clu.dir = CLUS_EOF;
	new_clu.size = 0;
	new_clu.flags = fid->flags;

	/* Allocate new cluster */
#ifdef CONFIG_SDFAT_DFR_PACKING
	if (amap->n_clean_au * DFR_FULL_RATIO <= amap->n_au * DFR_DEFAULT_PACKING_RATIO)
		err = fsi->fs_func->alloc_cluster(sb, 1, &new_clu, ALLOC_COLD_PACKING);
	else
		err = fsi->fs_func->alloc_cluster(sb, 1, &new_clu, ALLOC_COLD_ALIGNED);
#else
		err = fsi->fs_func->alloc_cluster(sb, 1, &new_clu, ALLOC_COLD_ALIGNED);
#endif

	if (err) {
		dfr_err("Map: 1 %d", 0);
		return err;
	}

	/* Decrease reserved cluster count */
	defrag_reserve_clusters(sb, -1);

	/* Add new_clus info in ino_dfr */
	sbi->dfr_new_clus[chunk->new_idx + clu_offset - chunk->f_clus] = new_clu.dir;

	/* Make FAT-chain for new_clus */
	for (i = 0; i < chunk->nr_clus; i++) {
#if 0
		if (sbi->dfr_new_clus[chunk->new_idx + i])
			nr_new++;
		else
			break;
#else
		if (!sbi->dfr_new_clus[chunk->new_idx + i])
			break;
		nr_new++;
#endif
	}
	if (nr_new == chunk->nr_clus) {
		for (i = 0; i < chunk->nr_clus - 1; i++) {
			FAT32_CHECK_CLUSTER(fsi, sbi->dfr_new_clus[chunk->new_idx + i], err);
			BUG_ON(err);
			if (fat_ent_set(sb,
				sbi->dfr_new_clus[chunk->new_idx + i],
				sbi->dfr_new_clus[chunk->new_idx + i + 1]))
				return -EIO;
		}
	}

	*clu = new_clu.dir;
	return 0;
}


/**
 * @fn		defrag_writepage_end_io
 * @brief	check WB status of requested page
 * @return	void
 * @param	page		page
 */
void
defrag_writepage_end_io(
	INOUT struct page *page)
{
	struct super_block *sb = page->mapping->host->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	struct defrag_info *ino_dfr = &(SDFAT_I(page->mapping->host)->dfr_info);
	unsigned int clus_start = 0, clus_end = 0;
	int i = 0;

	/* Check if this inode is on defrag */
	if (atomic_read(&ino_dfr->stat) != DFR_INO_STAT_REQ)
		return;

	clus_start = page->index / PAGES_PER_CLUS(sb);
	clus_end = clus_start + 1;

	/* Check each chunk in given inode */
	for (i = 0; i < ino_dfr->nr_chunks; i++) {
		struct defrag_chunk_info *chunk = &(ino_dfr->chunks[i]);
		unsigned int chunk_start = 0, chunk_end = 0;

		chunk_start = chunk->f_clus;
		chunk_end = chunk->f_clus + chunk->nr_clus;

		if ((clus_start >= chunk_start) && (clus_end <= chunk_end)) {
			int off = clus_start - chunk_start;

			clear_bit((page->index & (PAGES_PER_CLUS(sb) - 1)),
					(volatile unsigned long *)&(sbi->dfr_page_wb[chunk->new_idx + off]));
		}
	}
}


/**
 * @fn		__defrag_check_wb
 * @brief	check if WB for given chunk completed
 * @return	0 on success, -errno otherwise
 * @param	sbi		super block info
 * @param	chunk	given chunk
 */
static int
__defrag_check_wb(
	IN struct sdfat_sb_info *sbi,
	IN struct defrag_chunk_info *chunk)
{
	int err = 0, wb_i = 0, i = 0, nr_new = 0;

	if (!sbi || !chunk)
		return -EINVAL;

	/* Check WB complete status first */
	for (wb_i = 0; wb_i < chunk->nr_clus; wb_i++) {
		if (atomic_read((atomic_t *)&(sbi->dfr_page_wb[chunk->new_idx + wb_i]))) {
			err = -EBUSY;
			break;
		}
	}

	/**
	 * Check NEW_CLUS status.
	 * writepage_end_io cannot check whole WB complete status,
	 *	so we need to check NEW_CLUS status.
	 */
	for (i = 0; i < chunk->nr_clus; i++)
		if (sbi->dfr_new_clus[chunk->new_idx + i])
			nr_new++;

	if (nr_new == chunk->nr_clus) {
		err = 0;
		if ((wb_i != chunk->nr_clus) && (wb_i != chunk->nr_clus - 1))
			dfr_debug("submit_fullpage_bio() called on a page (nr_clus %d, wb_i %d)",
				chunk->nr_clus, wb_i);

		BUG_ON(nr_new > chunk->nr_clus);
	} else {
		dfr_debug("nr_new %d, nr_clus %d", nr_new, chunk->nr_clus);
		err = -EBUSY;
	}

	/* Update chunk's state */
	if (!err)
		chunk->stat |= DFR_CHUNK_STAT_WB;

	return err;
}


static void
__defrag_check_fat_old(
	IN struct super_block *sb,
	IN struct inode *inode,
	IN struct defrag_chunk_info *chunk)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	unsigned int clus = 0;
	int err = 0, idx = 0, max_idx = 0;

	/* Get start_clus */
	clus = SDFAT_I(inode)->fid.start_clu;

	/* Follow FAT-chain */
	#define num_clusters(val) ((val) ? (s32)((val - 1) >> fsi->cluster_size_bits) + 1 : 0)
	max_idx = num_clusters(SDFAT_I(inode)->i_size_ondisk);
	for (idx = 0; idx < max_idx; idx++) {

		FAT32_CHECK_CLUSTER(fsi, clus, err);
		ERR_HANDLE(err);
		err = fat_ent_get(sb, clus, &clus);
		ERR_HANDLE(err);

		if ((idx < max_idx - 1) && (IS_CLUS_EOF(clus) || IS_CLUS_FREE(clus))) {
			dfr_err("FAT: inode %p, max_idx %d, idx %d, clus %08x, "
				"f_clus %d, nr_clus %d", inode, max_idx,
				idx, clus, chunk->f_clus, chunk->nr_clus);
			BUG_ON(idx < max_idx - 1);
			goto error;
		}
	}

error:
	return;
}


static void
__defrag_check_fat_new(
	IN struct super_block *sb,
	IN struct inode *inode,
	IN struct defrag_chunk_info *chunk)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	unsigned int clus = 0;
	int i = 0, err = 0;

	/* Check start of FAT-chain */
	if (chunk->prev_clus) {
		FAT32_CHECK_CLUSTER(fsi, chunk->prev_clus, err);
		BUG_ON(err);
		err = fat_ent_get(sb, chunk->prev_clus, &clus);
		BUG_ON(err);
	} else {
		clus = SDFAT_I(inode)->fid.start_clu;
	}
	if (sbi->dfr_new_clus[chunk->new_idx] != clus) {
		dfr_err("FAT: inode %p, start_clus %08x, read_clus %08x",
				inode, sbi->dfr_new_clus[chunk->new_idx], clus);
		err = EIO;
		goto error;
	}

	/* Check inside of FAT-chain */
	if (chunk->nr_clus > 1) {
		for (i = 0; i < chunk->nr_clus - 1; i++) {
			FAT32_CHECK_CLUSTER(fsi, sbi->dfr_new_clus[chunk->new_idx + i], err);
			BUG_ON(err);
			err = fat_ent_get(sb, sbi->dfr_new_clus[chunk->new_idx + i], &clus);
			BUG_ON(err);
			if (sbi->dfr_new_clus[chunk->new_idx + i + 1] != clus) {
				dfr_err("FAT: inode %p, new_clus %08x, read_clus %08x",
							inode, sbi->dfr_new_clus[chunk->new_idx], clus);
				err = EIO;
				goto error;
			}
		}
		clus = 0;
	}

	/* Check end of FAT-chain */
	FAT32_CHECK_CLUSTER(fsi, sbi->dfr_new_clus[chunk->new_idx + chunk->nr_clus - 1], err);
	BUG_ON(err);
	err = fat_ent_get(sb, sbi->dfr_new_clus[chunk->new_idx + chunk->nr_clus - 1], &clus);
	BUG_ON(err);
	if ((chunk->next_clus & 0x0FFFFFFF) != (clus & 0x0FFFFFFF)) {
		dfr_err("FAT: inode %p, next_clus %08x, read_clus %08x", inode, chunk->next_clus, clus);
		err = EIO;
	}

error:
	BUG_ON(err);
}


/**
 * @fn		__defrag_update_dirent
 * @brief	update DIR entry for defrag req
 * @return	void
 * @param	sb		super block
 * @param	chunk	given chunk
 */
static void
__defrag_update_dirent(
	struct super_block *sb,
	struct defrag_chunk_info *chunk)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &SDFAT_SB(sb)->fsi;
	CHAIN_T dir;
	DOS_DENTRY_T *dos_ep;
	unsigned int entry = 0;
	unsigned long long sector = 0;
	unsigned short hi = 0, lo = 0;
	int err = 0;

	dir.dir = GET64_HI(chunk->i_pos);
	dir.flags = 0x1;	// Assume non-continuous

	entry = GET64_LO(chunk->i_pos);

	FAT32_CHECK_CLUSTER(fsi, dir.dir, err);
	BUG_ON(err);
	dos_ep = (DOS_DENTRY_T *) get_dentry_in_dir(sb, &dir, entry, &sector);

	hi = GET32_HI(sbi->dfr_new_clus[chunk->new_idx]);
	lo = GET32_LO(sbi->dfr_new_clus[chunk->new_idx]);

	dos_ep->start_clu_hi = cpu_to_le16(hi);
	dos_ep->start_clu_lo = cpu_to_le16(lo);

	dcache_modify(sb, sector);
}


/**
 * @fn		defrag_update_fat_prev
 * @brief	update FAT chain for defrag requests
 * @return	void
 * @param	sb		super block
 * @param	force	flag to force FAT update
 * @remark	protected by super_block and volume lock
 */
void
defrag_update_fat_prev(
	struct super_block *sb,
	int force)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	struct defrag_info *sb_dfr = &sbi->dfr_info, *ino_dfr = NULL;
	int skip = 0, done = 0;

	/* Check if FS_ERROR occurred */
	if (sb->s_flags & MS_RDONLY) {
		dfr_err("RDONLY partition (err %d)", -EPERM);
		goto out;
	}

	list_for_each_entry(ino_dfr, &sb_dfr->entry, entry) {
		struct inode *inode = &(container_of(ino_dfr, struct sdfat_inode_info, dfr_info)->vfs_inode);
		struct sdfat_inode_info *ino_info = SDFAT_I(inode);
		struct defrag_chunk_info *chunk_prev = NULL;
		int i = 0, j = 0;

		mutex_lock(&ino_dfr->lock);
		BUG_ON(atomic_read(&ino_dfr->stat) != DFR_INO_STAT_REQ);
		for (i = 0; i < ino_dfr->nr_chunks; i++) {
			struct defrag_chunk_info *chunk = NULL;
			int err = 0;

			chunk = &(ino_dfr->chunks[i]);
			BUG_ON(!chunk);

			/* Do nothing for already passed chunk */
			if (chunk->stat == DFR_CHUNK_STAT_PASS) {
				done++;
				continue;
			}

			/* Handle error case */
			if (chunk->stat == DFR_CHUNK_STAT_ERR) {
				err = -EINVAL;
				goto error;
			}

			/* Double-check clusters */
			if (chunk_prev &&
				(chunk->f_clus == chunk_prev->f_clus + chunk_prev->nr_clus) &&
				(chunk_prev->stat == DFR_CHUNK_STAT_PASS)) {

				err = defrag_validate_cluster(inode, chunk, 1);

				/* Handle continuous chunks in a file */
				if (!err) {
					chunk->prev_clus =
						sbi->dfr_new_clus[chunk_prev->new_idx + chunk_prev->nr_clus - 1];
					dfr_debug("prev->f_clus %d, prev->nr_clus %d, chunk->f_clus %d",
							chunk_prev->f_clus, chunk_prev->nr_clus, chunk->f_clus);
				}
			} else {
				err = defrag_validate_cluster(inode, chunk, 0);
			}

			if (err) {
				dfr_err("Cluster validation: inode %p, chunk->f_clus %d, err %d",
						inode, chunk->f_clus, err);
				goto error;
			}

			/**
			 * Skip update_fat_prev if WB or update_fat_next not completed.
			 * Go to error case if FORCE set.
			 */
			if (__defrag_check_wb(sbi, chunk) || (chunk->stat != DFR_CHUNK_STAT_PREP)) {
				if (force) {
					err = -EPERM;
					dfr_err("Skip case: inode %p, stat %x, f_clus %d, err %d",
							inode, chunk->stat, chunk->f_clus, err);
					goto error;
				}
				skip++;
				continue;
			}

#ifdef	CONFIG_SDFAT_DFR_DEBUG
			/* SPO test */
			defrag_spo_test(sb, DFR_SPO_RANDOM, __func__);
#endif

			/* Update chunk's previous cluster */
			if (chunk->prev_clus == 0) {
				/* For the first cluster of a file */
				/* Update ino_info->fid.start_clu */
				ino_info->fid.start_clu = sbi->dfr_new_clus[chunk->new_idx];
				__defrag_update_dirent(sb, chunk);
			} else {
				FAT32_CHECK_CLUSTER(fsi, chunk->prev_clus, err);
				BUG_ON(err);
				if (fat_ent_set(sb,
					chunk->prev_clus,
					sbi->dfr_new_clus[chunk->new_idx])) {
					err = -EIO;
					goto error;
				}
			}

			/* Clear extent cache */
			extent_cache_inval_inode(inode);

			/* Update FID info */
			ino_info->fid.hint_bmap.off = CLUS_EOF;
			ino_info->fid.hint_bmap.clu = 0;

			/* Clear old FAT-chain */
			for (j = 0; j < chunk->nr_clus; j++)
				defrag_free_cluster(sb, chunk->d_clus + j);

			/* Mark this chunk PASS */
			chunk->stat = DFR_CHUNK_STAT_PASS;
			__defrag_check_fat_new(sb, inode, chunk);

			done++;

error:
			if (err) {
				/**
				 * chunk->new_idx != 0 means this chunk needs to be cleaned up
				 */
				if (chunk->new_idx) {
					/* Free already allocated clusters */
					for (j = 0; j < chunk->nr_clus; j++) {
						if (sbi->dfr_new_clus[chunk->new_idx + j]) {
							defrag_free_cluster(sb, sbi->dfr_new_clus[chunk->new_idx + j]);
							sbi->dfr_new_clus[chunk->new_idx + j] = 0;
						}
					}

					__defrag_check_fat_old(sb, inode, chunk);
				}

				/**
				 * chunk->new_idx == 0 means this chunk already cleaned up
				 */
				chunk->new_idx = 0;
				chunk->stat = DFR_CHUNK_STAT_ERR;
			}

			chunk_prev = chunk;
		}
		BUG_ON(!mutex_is_locked(&ino_dfr->lock));
		mutex_unlock(&ino_dfr->lock);
	}

out:
	if (skip) {
		dfr_debug("%s skipped (nr_reqs %d, done %d, skip %d)",
					__func__, sb_dfr->nr_chunks - 1, done, skip);
	} else {
		/* Make dfr_reserved_clus zero */
		if (sbi->dfr_reserved_clus > 0) {
			if (fsi->reserved_clusters < sbi->dfr_reserved_clus) {
				dfr_err("Reserved count: reserved_clus %d, dfr_reserved_clus %d",
						fsi->reserved_clusters, sbi->dfr_reserved_clus);
				BUG_ON(fsi->reserved_clusters < sbi->dfr_reserved_clus);
			}

			defrag_reserve_clusters(sb, 0 - sbi->dfr_reserved_clus);
		}

		dfr_debug("%s done (nr_reqs %d, done %d)", __func__, sb_dfr->nr_chunks - 1, done);
	}
}


/**
 * @fn		defrag_update_fat_next
 * @brief	update FAT chain for defrag requests
 * @return	void
 * @param	sb		super block
 * @remark	protected by super_block and volume lock
 */
void
defrag_update_fat_next(
	struct super_block *sb)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	struct defrag_info *sb_dfr = &sbi->dfr_info, *ino_dfr = NULL;
	struct defrag_chunk_info *chunk = NULL;
	int done = 0, i = 0, j = 0, err = 0;

	/* Check if FS_ERROR occurred */
	if (sb->s_flags & MS_RDONLY) {
		dfr_err("RDONLY partition (err %d)", -EROFS);
		goto out;
	}

	list_for_each_entry(ino_dfr, &sb_dfr->entry, entry) {

		for (i = 0; i < ino_dfr->nr_chunks; i++) {
			int skip = 0;

			chunk = &(ino_dfr->chunks[i]);

			/* Do nothing if error occurred or update_fat_next already passed */
			if (chunk->stat == DFR_CHUNK_STAT_ERR)
				continue;
			if (chunk->stat & DFR_CHUNK_STAT_FAT) {
				done++;
				continue;
			}

			/* Ship this chunk if get_block not passed for this chunk */
			for (j = 0; j < chunk->nr_clus; j++) {
				if (sbi->dfr_new_clus[chunk->new_idx + j] == 0) {
					skip = 1;
					break;
				}
			}
			if (skip)
				continue;

			/* Update chunk's next cluster */
			FAT32_CHECK_CLUSTER(fsi,
				sbi->dfr_new_clus[chunk->new_idx + chunk->nr_clus - 1], err);
			BUG_ON(err);
			if (fat_ent_set(sb,
				sbi->dfr_new_clus[chunk->new_idx + chunk->nr_clus - 1],
				chunk->next_clus))
				goto out;

#ifdef	CONFIG_SDFAT_DFR_DEBUG
			/* SPO test */
			defrag_spo_test(sb, DFR_SPO_RANDOM, __func__);
#endif

			/* Update chunk's state */
			chunk->stat |= DFR_CHUNK_STAT_FAT;
			done++;
		}
	}

out:
	dfr_debug("%s done (nr_reqs %d, done %d)", __func__, sb_dfr->nr_chunks - 1, done);
}


/**
 * @fn		defrag_check_discard
 * @brief	check if we can send discard for this AU, if so, send discard
 * @return	void
 * @param	sb		super block
 * @remark	protected by super_block and volume lock
 */
void
defrag_check_discard(
	IN struct super_block *sb)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	AU_INFO_T *au = NULL;
	struct defrag_info *sb_dfr = &(SDFAT_SB(sb)->dfr_info);
	unsigned int tmp[DFR_MAX_AU_MOVED];
	int i = 0, j = 0;

	BUG_ON(!amap);

	if (!(SDFAT_SB(sb)->options.discard) ||
		!(SDFAT_SB(sb)->options.improved_allocation & SDFAT_ALLOC_SMART))
		return;

	memset(tmp, 0, sizeof(int) * DFR_MAX_AU_MOVED);

	for (i = REQ_HEADER_IDX + 1; i < sb_dfr->nr_chunks; i++) {
		struct defrag_chunk_info *chunk = &(sb_dfr->chunks[i]);
		int skip = 0;

		au = GET_AU(amap, i_AU_of_CLU(amap, chunk->d_clus));

		/* Send DISCARD for free AU */
		if ((IS_AU_IGNORED(au, amap)) &&
			(amap_get_freeclus(sb, chunk->d_clus) == CLUS_PER_AU(sb))) {
			sector_t blk = 0, nr_blks = 0;
			unsigned int au_align_factor = amap->option.au_align_factor % amap->option.au_size;

			BUG_ON(au->idx == 0);

			/* Avoid multiple DISCARD */
			for (j = 0; j < DFR_MAX_AU_MOVED; j++) {
				if (tmp[j] == au->idx) {
					skip = 1;
					break;
				}
			}
			if (skip == 1)
				continue;

			/* Send DISCARD cmd */
			blk = (sector_t) (((au->idx * CLUS_PER_AU(sb)) << fsi->sect_per_clus_bits)
						- au_align_factor);
			nr_blks = ((sector_t)CLUS_PER_AU(sb)) << fsi->sect_per_clus_bits;

			dfr_debug("Send DISCARD for AU[%d] (blk %08zx)", au->idx, blk);
			sb_issue_discard(sb, blk, nr_blks, GFP_NOFS, 0);

			/* Save previous AU's index */
			for (j = 0; j < DFR_MAX_AU_MOVED; j++) {
				if (!tmp[j]) {
					tmp[j] = au->idx;
					break;
				}
			}
		}
	}
}


/**
 * @fn		defrag_free_cluster
 * @brief	free uneccessary cluster
 * @return	void
 * @param	sb		super block
 * @param	clus	physical cluster num
 * @remark	protected by super_block and volume lock
 */
int
defrag_free_cluster(
	struct super_block *sb,
	unsigned int clus)
{
	FS_INFO_T *fsi = &SDFAT_SB(sb)->fsi;
	unsigned int val = 0;
	s32 err = 0;

	FAT32_CHECK_CLUSTER(fsi, clus, err);
	BUG_ON(err);
	if (fat_ent_get(sb, clus, &val))
		return -EIO;
	if (val) {
		if (fat_ent_set(sb, clus, 0))
			return -EIO;
	} else {
		dfr_err("Free: Already freed, clus %08x, val %08x", clus, val);
		BUG_ON(!val);
	}

	set_sb_dirty(sb);
	fsi->used_clusters--;
	if (fsi->amap)
		amap_release_cluster(sb, clus);

	return 0;
}


/**
 * @fn		defrag_check_defrag_required
 * @brief	check if defrag required
 * @return	1 if required, 0 otherwise
 * @param	sb			super block
 * @param	totalau		# of total AUs
 * @param	cleanau		# of clean AUs
 * @param	fullau		# of full AUs
 * @remark	protected by super_block
 */
int
defrag_check_defrag_required(
	IN struct super_block *sb,
	OUT int *totalau,
	OUT int *cleanau,
	OUT int *fullau)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	AMAP_T *amap = NULL;
	int clean_ratio = 0, frag_ratio = 0;
	int ret = 0;

	if (!sb || !(SDFAT_SB(sb)->options.defrag))
		return 0;

	/* Check DFR_DEFAULT_STOP_RATIO first */
	fsi = &(SDFAT_SB(sb)->fsi);
	if (fsi->used_clusters == (unsigned int)(~0)) {
		if (fsi->fs_func->count_used_clusters(sb, &fsi->used_clusters))
			return -EIO;
	}
	if (fsi->used_clusters * DFR_FULL_RATIO >= fsi->num_clusters * DFR_DEFAULT_STOP_RATIO) {
		dfr_debug("used_clusters %d, num_clusters %d", fsi->used_clusters, fsi->num_clusters);
		return 0;
	}

	/* Check clean/frag ratio */
	amap = SDFAT_SB(sb)->fsi.amap;
	BUG_ON(!amap);

	clean_ratio = (amap->n_clean_au * 100) / amap->n_au;
	if (amap->n_full_au)
		frag_ratio = ((amap->n_au - amap->n_clean_au) * 100) / amap->n_full_au;
	else
		frag_ratio = ((amap->n_au - amap->n_clean_au) * 100) /
					(fsi->used_clusters * CLUS_PER_AU(sb));

	/*
	 * Wake-up defrag_daemon:
	 * when # of clean AUs too small, or frag_ratio exceeds the limit
	 */
	if ((clean_ratio < DFR_DEFAULT_WAKEUP_RATIO) ||
		((clean_ratio < DFR_DEFAULT_CLEAN_RATIO) && (frag_ratio >= DFR_DEFAULT_FRAG_RATIO))) {

		if (totalau)
			*totalau = amap->n_au;
		if (cleanau)
			*cleanau = amap->n_clean_au;
		if (fullau)
			*fullau = amap->n_full_au;
		ret = 1;
	}

	return ret;
}


/**
 * @fn		defrag_check_defrag_required
 * @brief	check defrag status on inode
 * @return	1 if defrag in on, 0 otherwise
 * @param	inode	inode
 * @param	start	logical start addr
 * @param	end		logical end addr
 * @param	cancel	flag to cancel defrag
 * @param	caller	caller info
 */
int
defrag_check_defrag_on(
	INOUT struct inode *inode,
	IN loff_t start,
	IN loff_t end,
	IN int cancel,
	IN const char *caller)
{
	struct super_block *sb = inode->i_sb;
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);
	FS_INFO_T *fsi = &(sbi->fsi);
	struct defrag_info *ino_dfr = &(SDFAT_I(inode)->dfr_info);
	unsigned int clus_start = 0, clus_end = 0;
	int ret = 0, i = 0;

	if (!inode || (start == end))
		return 0;

	mutex_lock(&ino_dfr->lock);
	/* Check if this inode is on defrag */
	if (atomic_read(&ino_dfr->stat) == DFR_INO_STAT_REQ) {

		clus_start = start >> (fsi->cluster_size_bits);
		clus_end = (end >> (fsi->cluster_size_bits)) +
				((end & (fsi->cluster_size - 1)) ? 1 : 0);

		if (!ino_dfr->chunks)
			goto error;

		/* Check each chunk in given inode */
		for (i = 0; i < ino_dfr->nr_chunks; i++) {
			struct defrag_chunk_info *chunk = &(ino_dfr->chunks[i]);
			unsigned int chunk_start = 0, chunk_end = 0;

			/* Skip this chunk when error occurred or it already passed defrag process */
			if ((chunk->stat == DFR_CHUNK_STAT_ERR) || (chunk->stat == DFR_CHUNK_STAT_PASS))
				continue;

			chunk_start = chunk->f_clus;
			chunk_end = chunk->f_clus + chunk->nr_clus;

			if (((clus_start >= chunk_start) && (clus_start < chunk_end)) ||
				((clus_end > chunk_start) && (clus_end <= chunk_end)) ||
				((clus_start < chunk_start) && (clus_end > chunk_end)))  {
				ret = 1;
				if (cancel) {
					chunk->stat =  DFR_CHUNK_STAT_ERR;
					dfr_debug("Defrag canceled: inode %p, start %08x, end %08x, caller %s",
							inode, clus_start, clus_end, caller);
				}
			}
		}
	}

error:
	BUG_ON(!mutex_is_locked(&ino_dfr->lock));
	mutex_unlock(&ino_dfr->lock);
	return ret;
}


#ifdef CONFIG_SDFAT_DFR_DEBUG
/**
 * @fn		defrag_spo_test
 * @brief	test SPO while defrag running
 * @return	void
 * @param	sb		super block
 * @param	flag	SPO debug flag
 * @param	caller	caller info
 */
void
defrag_spo_test(
	struct super_block *sb,
	int flag,
	const char *caller)
{
	struct sdfat_sb_info *sbi = SDFAT_SB(sb);

	if (!sb || !(SDFAT_SB(sb)->options.defrag))
		return;

	if (flag == sbi->dfr_spo_flag) {
		dfr_err("Defrag SPO test (flag %d, caller %s)", flag, caller);
		panic("Defrag SPO test");
	}
}
#endif	/* CONFIG_SDFAT_DFR_DEBUG */


#endif /* CONFIG_SDFAT_DFR */

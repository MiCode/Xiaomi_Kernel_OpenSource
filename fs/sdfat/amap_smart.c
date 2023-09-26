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
/*  FILE    : amap_smart.c                                              */
/*  PURPOSE : FAT32 Smart allocation code for sdFAT                     */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "sdfat.h"
#include "core.h"
#include "amap_smart.h"

/* AU list related functions */
static inline void amap_list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);

	/* Will be used to check if the entry is a single entry(selected) */
	entry->prev = NULL;
	entry->next = NULL;
}

static inline int amap_insert_to_list(AU_INFO_T *au, struct slist_head *shead)
{
	struct slist_head *entry = &au->shead;

	ASSERT(!entry->head);

	entry->next = shead->next;
	entry->head = shead;

	shead->next = entry;

	return 0;
}

static inline int amap_remove_from_list(AU_INFO_T *au, struct slist_head *shead)
{
	struct slist_head *entry = &au->shead;
	struct slist_head *iter;

	BUG_ON(entry->head != shead);

	iter = shead;

	while (iter->next) {
		if (iter->next == entry) {
			// iter->next = iter->next->next
			iter->next = entry->next;

			entry->next = NULL;
			entry->head = NULL;
			return 0;
		}
		iter = iter->next;
	}

	BUG_ON("Not reachable");
}

/* Full-linear serach => Find AU with max. number of fclu */
static inline AU_INFO_T *amap_find_hot_au_largest(struct slist_head *shead)
{
	struct slist_head *iter;
	uint16_t max_fclu = 0;
	AU_INFO_T *entry, *ret = NULL;

	ASSERT(shead->head == shead);	/* Singly-list condition */
	ASSERT(shead->next != shead);

	iter = shead->next;

	while (iter) {
		entry = list_entry(iter, AU_INFO_T, shead);

		if (entry->free_clusters > max_fclu) {
			max_fclu = entry->free_clusters;
			ret = entry;
		}

		iter = iter->next;
	}

	return ret;
}

/* Find partially used AU with max. number of fclu.
 * If there is no partial AU available, pick a clean one
 */
static inline AU_INFO_T *amap_find_hot_au_partial(AMAP_T *amap)
{
	struct slist_head *iter;
	uint16_t max_fclu = 0;
	AU_INFO_T *entry, *ret = NULL;

	iter = &amap->slist_hot;
	ASSERT(iter->head == iter);	/* Singly-list condition */
	ASSERT(iter->next != iter);

	iter = iter->next;

	while (iter) {
		entry = list_entry(iter, AU_INFO_T, shead);

		if (entry->free_clusters > max_fclu) {
			if (entry->free_clusters < amap->clusters_per_au) {
				max_fclu = entry->free_clusters;
				ret = entry;
			} else {
				if (!ret)
					ret = entry;
			}
		}

		iter = iter->next;
	}

	return ret;
}




/*
 * Size-base AU management functions
 */

/*
 * Add au into cold AU MAP
 * au: an isolated (not in a list) AU data structure
 */
int amap_add_cold_au(AMAP_T *amap, AU_INFO_T *au)
{
	FCLU_NODE_T *fclu_node = NULL;

	/* Check if a single entry */
	BUG_ON(au->head.prev);

	/* Ignore if the au is full */
	if (!au->free_clusters)
		return 0;

	/* Find entry */
	fclu_node = NODE(au->free_clusters, amap);

	/* Insert to the list */
	list_add_tail(&(au->head), &(fclu_node->head));

	/* Update fclu_hint (Increase) */
	if (au->free_clusters > amap->fclu_hint)
		amap->fclu_hint = au->free_clusters;

	return 0;
}

/*
 * Remove an AU from AU MAP
 */
int amap_remove_cold_au(AMAP_T *amap, AU_INFO_T *au)
{
	struct list_head *prev = au->head.prev;

	/* Single entries are not managed in lists */
	if (!prev) {
		BUG_ON(au->free_clusters > 0);
		return 0;
	}

	/* remove from list */
	amap_list_del(&(au->head));

	return 0;
}


/* "Find" best fit AU
 * returns NULL if there is no AU w/ enough free space.
 *
 * This function doesn't change AU status.
 * The caller should call amap_remove_cold_au() if needed.
 */
AU_INFO_T *amap_find_cold_au_bestfit(AMAP_T *amap, uint16_t free_clusters)
{
	AU_INFO_T *au = NULL;
	FCLU_NODE_T *fclu_iter;

	if (free_clusters <= 0 || free_clusters > amap->clusters_per_au) {
		EMSG("AMAP: amap_find_cold_au_bestfit / unexpected arg. (%d)\n",
				free_clusters);
		return NULL;
	}

	fclu_iter = NODE(free_clusters, amap);

	if (amap->fclu_hint < free_clusters) {
		/* There is no AUs with enough free_clusters */
		return NULL;
	}

	/* Naive Hash management (++) */
	do {
		if (!list_empty(&fclu_iter->head)) {
			struct list_head *first = fclu_iter->head.next;

			au = list_entry(first, AU_INFO_T, head);

			break;
		}

		fclu_iter++;
	} while (fclu_iter < (amap->fclu_nodes + amap->clusters_per_au));


	// BUG_ON(au->free_clusters < 0);
	BUG_ON(au && (au->free_clusters > amap->clusters_per_au));

	return au;
}


/* "Pop" best fit AU
 *
 * returns NULL if there is no AU w/ enough free space.
 * The returned AU will not be in the list anymore.
 */
AU_INFO_T *amap_pop_cold_au_bestfit(AMAP_T *amap, uint16_t free_clusters)
{
	/* Naive implementation */
	AU_INFO_T *au;

	au = amap_find_cold_au_bestfit(amap, free_clusters);
	if (au)
		amap_remove_cold_au(amap, au);

	return au;
}



/* Pop the AU with the largest free space
 *
 * search from 'start_fclu' to 0
 * (target freecluster : -1 for each step)
 * start_fclu = 0 means to search from the max. value
 */
AU_INFO_T *amap_pop_cold_au_largest(AMAP_T *amap, uint16_t start_fclu)
{
	AU_INFO_T *au = NULL;
	FCLU_NODE_T *fclu_iter;

	if (!start_fclu)
		start_fclu = amap->clusters_per_au;
	if (start_fclu > amap->clusters_per_au)
		start_fclu = amap->clusters_per_au;

	/* Use hint (search start point) */
	if (amap->fclu_hint < start_fclu)
		fclu_iter = NODE(amap->fclu_hint, amap);
	else
		fclu_iter = NODE(start_fclu, amap);

	/* Naive Hash management */
	do {
		if (!list_empty(&fclu_iter->head)) {
			struct list_head *first = fclu_iter->head.next;

			au = list_entry(first, AU_INFO_T, head);
			// BUG_ON((au < amap->entries) || ((amap->entries + amap->n_au) <= au));

			amap_list_del(first);

			// (Hint) Possible maximum value of free clusters (among cold)
			/* if it wasn't the whole search, don't update fclu_hint */
			if (start_fclu == amap->clusters_per_au)
				amap->fclu_hint = au->free_clusters;

			break;
		}

		fclu_iter--;
	} while (amap->fclu_nodes <= fclu_iter);

	return au;
}



/*
 * ===============================================
 * Allocation Map related functions
 * ===============================================
 */

/* Create AMAP related data structure (mount time) */
int amap_create(struct super_block *sb, u32 pack_ratio, u32 sect_per_au, u32 hidden_sect)
{
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	AMAP_T *amap;
	int total_used_clusters;
	int n_au_table = 0;
	int i, i_clu, i_au;
	int i_au_root = -1, i_au_hot_from = INT_MAX;
	u32 misaligned_sect = hidden_sect;

	BUG_ON(!fsi->bd_opened);

	if (fsi->amap)
		return -EEXIST;

	/* Check conditions */
	if (fsi->vol_type != FAT32) {
		sdfat_msg(sb, KERN_ERR, "smart allocation is only available "
					"with fat32-fs");
		return -ENOTSUPP;
	}

	if (fsi->num_sectors < AMAP_MIN_SUPPORT_SECTORS) {
		sdfat_msg(sb, KERN_ERR, "smart allocation is only available "
			"with sectors above %d", AMAP_MIN_SUPPORT_SECTORS);
		return -ENOTSUPP;
	}

	/* AU size must be a multiple of clu_size */
	if ((sect_per_au <= 0) || (sect_per_au & (fsi->sect_per_clus - 1))) {
		sdfat_msg(sb, KERN_ERR,
			"invalid AU size (sect_per_au : %u, "
			"sect_per_clus : %u) "
			"please re-format for performance.",
			sect_per_au, fsi->sect_per_clus);
		return -EINVAL;
	}

	/* the start sector of this partition must be a multiple of clu_size */
	if (misaligned_sect & (fsi->sect_per_clus - 1)) {
		sdfat_msg(sb, KERN_ERR,
			"misaligned part (start sect : %u, "
			"sect_per_clus : %u) "
			"please re-format for performance.",
			misaligned_sect, fsi->sect_per_clus);
		return -EINVAL;
	}

	/* data start sector must be a multiple of clu_size */
	if (fsi->data_start_sector & (fsi->sect_per_clus - 1)) {
		sdfat_msg(sb, KERN_ERR,
			"misaligned data area (start sect : %llu, "
			"sect_per_clus : %u) "
			"please re-format for performance.",
			fsi->data_start_sector, fsi->sect_per_clus);
		return -EINVAL;
	}

	misaligned_sect &= (sect_per_au - 1);

	/* Allocate data structrues */
	amap = kzalloc(sizeof(AMAP_T), GFP_NOIO);
	if (!amap)
		return -ENOMEM;

	amap->sb = sb;

	amap->n_au = (fsi->num_sectors + misaligned_sect + sect_per_au - 1) / sect_per_au;
	amap->n_clean_au = 0;
	amap->n_full_au = 0;

	/* Reflect block-partition align first,
	 * then partition-data_start align
	 */
	amap->clu_align_bias = (misaligned_sect / fsi->sect_per_clus);
	amap->clu_align_bias += (fsi->data_start_sector >> fsi->sect_per_clus_bits) - CLUS_BASE;
	amap->clusters_per_au = sect_per_au / fsi->sect_per_clus;

	/* That is,
	 * the size of cluster is at least 4KB if the size of AU is 4MB
	 */
	if (amap->clusters_per_au > MAX_CLU_PER_AU) {
		sdfat_log_msg(sb, KERN_INFO,
			"too many clusters per AU (clus/au:%d > %d).",
		       amap->clusters_per_au,
		       MAX_CLU_PER_AU);
	}

	/* is it needed? why here? */
	// set_sb_dirty(sb);

	spin_lock_init(&amap->amap_lock);

	amap->option.packing_ratio = pack_ratio;
	amap->option.au_size = sect_per_au;
	amap->option.au_align_factor = hidden_sect;


	/* Allocate AU info table */
	n_au_table = (amap->n_au + N_AU_PER_TABLE - 1) / N_AU_PER_TABLE;
	amap->au_table = kmalloc(sizeof(AU_INFO_T *) * n_au_table, GFP_NOIO);
	if (!amap->au_table) {
		sdfat_msg(sb, KERN_ERR,
			"failed to alloc amap->au_table\n");
		kfree(amap);
		return -ENOMEM;
	}

	for (i = 0; i < n_au_table; i++)
		amap->au_table[i] = (AU_INFO_T *)get_zeroed_page(GFP_NOIO);

	/* Allocate buckets indexed by # of free clusters */
	amap->fclu_order = get_order(sizeof(FCLU_NODE_T) * amap->clusters_per_au);

	// XXX: amap->clusters_per_au limitation is 512 (w/ 8 byte list_head)
	sdfat_log_msg(sb, KERN_INFO, "page orders for AU nodes : %d "
			"(clus_per_au : %d, node_size : %lu)",
			amap->fclu_order,
			amap->clusters_per_au,
			(unsigned long)sizeof(FCLU_NODE_T));

	if (!amap->fclu_order)
		amap->fclu_nodes = (FCLU_NODE_T *)get_zeroed_page(GFP_NOIO);
	else
		amap->fclu_nodes = vzalloc(PAGE_SIZE << amap->fclu_order);

	amap->fclu_hint = amap->clusters_per_au;

	/* Hot AU list, ignored AU list */
	amap->slist_hot.next = NULL;
	amap->slist_hot.head = &amap->slist_hot;
	amap->total_fclu_hot = 0;

	amap->slist_ignored.next = NULL;
	amap->slist_ignored.head = &amap->slist_ignored;

	/* Strategy related vars. */
	amap->cur_cold.au = NULL;
	amap->cur_hot.au = NULL;
	amap->n_need_packing = 0;


	/* Build AMAP info */
	total_used_clusters = 0;		// Count # of used clusters

	i_au_root = i_AU_of_CLU(amap, fsi->root_dir);
	i_au_hot_from = amap->n_au - (SMART_ALLOC_N_HOT_AU - 1);

	for (i = 0; i < amap->clusters_per_au; i++)
		INIT_LIST_HEAD(&amap->fclu_nodes[i].head);

	/*
	 * Thanks to kzalloc()
	 * amap->entries[i_au].free_clusters = 0;
	 * amap->entries[i_au].head.prev = NULL;
	 * amap->entries[i_au].head.next = NULL;
	 */

	/* Parse FAT table */
	for (i_clu = CLUS_BASE; i_clu < fsi->num_clusters; i_clu++) {
		u32 clu_data;
		AU_INFO_T *au;

		if (fat_ent_get(sb, i_clu, &clu_data)) {
			sdfat_msg(sb, KERN_ERR,
				"failed to read fat entry(%u)\n", i_clu);
			goto free_and_eio;
		}

		if (IS_CLUS_FREE(clu_data)) {
			au = GET_AU(amap, i_AU_of_CLU(amap, i_clu));
			au->free_clusters++;
		} else
			total_used_clusters++;
	}

	/* Build AU list */
	for (i_au = 0; i_au < amap->n_au; i_au++) {
		AU_INFO_T *au = GET_AU(amap, i_au);

		au->idx = i_au;
		BUG_ON(au->free_clusters > amap->clusters_per_au);

		if (au->free_clusters == amap->clusters_per_au)
			amap->n_clean_au++;
		else if (au->free_clusters == 0)
			amap->n_full_au++;

		/* If hot, insert to the hot list */
		if (i_au >= i_au_hot_from) {
			amap_add_hot_au(amap, au);
			amap->total_fclu_hot += au->free_clusters;
		} else if (i_au != i_au_root || SMART_ALLOC_N_HOT_AU == 0) {
		/* Otherwise, insert to the free cluster hash */
			amap_add_cold_au(amap, au);
		}
	}

	/* Hot list -> (root) -> (last) -> (last - 1) -> ... */
	if (i_au_root >= 0 && SMART_ALLOC_N_HOT_AU > 0) {
		amap_add_hot_au(amap, GET_AU(amap, i_au_root));
		amap->total_fclu_hot += GET_AU(amap, i_au_root)->free_clusters;
	}

	fsi->amap = amap;
	fsi->used_clusters = total_used_clusters;

	sdfat_msg(sb, KERN_INFO,
			"AMAP: Smart allocation enabled (opt : %u / %u / %u)",
			amap->option.au_size, amap->option.au_align_factor,
			amap->option.packing_ratio);

	/* Debug purpose - check */
	//{
	//u32 used_clusters;
	//fat_count_used_clusters(sb, &used_clusters)
	//ASSERT(used_clusters == total_used_clusters);
	//}

	return 0;


free_and_eio:
	if (amap) {
		if (amap->au_table) {
			for (i = 0; i < n_au_table; i++)
				free_page((unsigned long)amap->au_table[i]);
			kfree(amap->au_table);
		}
		if (amap->fclu_nodes) {
			if (!amap->fclu_order)
				free_page((unsigned long)amap->fclu_nodes);
			else
				vfree(amap->fclu_nodes);
		}
		kfree(amap);
	}
	return -EIO;
}


/* Free AMAP related structure */
void amap_destroy(struct super_block *sb)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	int n_au_table;

	if (!amap)
		return;

	DMSG("%s\n", __func__);

	n_au_table = (amap->n_au + N_AU_PER_TABLE - 1) / N_AU_PER_TABLE;

	if (amap->au_table) {
		int i;

		for (i = 0; i < n_au_table; i++)
			free_page((unsigned long)amap->au_table[i]);

		kfree(amap->au_table);
	}
	if (!amap->fclu_order)
		free_page((unsigned long)amap->fclu_nodes);
	else
		vfree(amap->fclu_nodes);
	kfree(amap);
	SDFAT_SB(sb)->fsi.amap = NULL;
}


/*
 * Check status of FS
 * and change destination if needed to disable AU-aligned alloc.
 * (from ALLOC_COLD_ALIGNED to ALLOC_COLD_SEQ)
 */
static inline int amap_update_dest(AMAP_T *amap, int ori_dest)
{
	FS_INFO_T *fsi = &(SDFAT_SB(amap->sb)->fsi);
	int n_partial_au, n_partial_freeclus;

	if (ori_dest != ALLOC_COLD_ALIGNED)
		return ori_dest;

	/* # of partial AUs and # of clusters in those AUs */
	n_partial_au = amap->n_au - amap->n_clean_au - amap->n_full_au;
	n_partial_freeclus = fsi->num_clusters - fsi->used_clusters -
				amap->clusters_per_au * amap->n_clean_au;

	/* Status of AUs : Full / Partial / Clean
	 * If there are many partial (and badly fragmented) AUs,
	 * the throughput will decrease extremly.
	 *
	 * The follow code will treat those worst cases.
	 */

	/* XXX: AMAP heuristics */
	if ((amap->n_clean_au * 50 <= amap->n_au) &&
		(n_partial_freeclus*2) < (n_partial_au*amap->clusters_per_au)) {
		/* If clean AUs are fewer than 2% of n_au (80 AUs per 16GB)
		 * and fragment ratio is more than 2 (AVG free_clusters=half AU)
		 *
		 * disable clean-first allocation
		 * enable VFAT-like sequential allocation
		 */
		return ALLOC_COLD_SEQ;
	}

	return ori_dest;
}


#define PACKING_SOFTLIMIT      (amap->option.packing_ratio)
#define PACKING_HARDLIMIT      (amap->option.packing_ratio * 4)
/*
 * Pick a packing AU if needed.
 * Otherwise just return NULL
 *
 * This function includes some heuristics.
 */
static inline AU_INFO_T *amap_get_packing_au(AMAP_T *amap, int dest, int num_to_wb, int *clu_to_skip)
{
	AU_INFO_T *au = NULL;

	if (dest == ALLOC_COLD_PACKING) {
		/* ALLOC_COLD_PACKING:
		 * Packing-first mode for defrag.
		 * Optimized to save clean AU
		 *
		 * 1) best-fit AU
		 * 2) Smallest AU (w/ minimum free clusters)
		 */
		if (num_to_wb >= amap->clusters_per_au)
			num_to_wb = num_to_wb % amap->clusters_per_au;

		/* 이거 주석처리하면, AU size 딱 맞을때는 clean, 나머지는 작은거부터 */
		if (num_to_wb == 0)
			num_to_wb = 1;		// Don't use clean AUs

		au = amap_find_cold_au_bestfit(amap, num_to_wb);
		if (au && au->free_clusters == amap->clusters_per_au && num_to_wb > 1) {
			/* if au is clean then get a new partial one */
			au = amap_find_cold_au_bestfit(amap, 1);
		}

		if (au) {
			amap->n_need_packing = 0;
			amap_remove_cold_au(amap, au);
			return au;
		}
	}


	/* Heuristic packing:
	 * This will improve QoS greatly.
	 *
	 * Count # of AU_ALIGNED allocation.
	 * If the number exceeds the specific threshold,
	 * allocate on a partial AU or generate random I/O.
	 */
	if ((PACKING_SOFTLIMIT > 0) &&
		(amap->n_need_packing >= PACKING_SOFTLIMIT) &&
		(num_to_wb < (int)amap->clusters_per_au)) {
		/* Best-fit packing:
		 * If num_to_wb (expected number to be allocated) is smaller
		 * than AU_SIZE, find a best-fit AU.
		 */

		/* Back margin (heuristics) */
		if (num_to_wb < amap->clusters_per_au / 4)
			num_to_wb = amap->clusters_per_au / 4;

		au = amap_find_cold_au_bestfit(amap, num_to_wb);
		if (au != NULL) {
			amap_remove_cold_au(amap, au);

			MMSG("AMAP: packing (cnt: %d) / softlimit, "
			     "best-fit (num_to_wb: %d))\n",
				amap->n_need_packing, num_to_wb);

			if (au->free_clusters > num_to_wb) { // Best-fit search: if 문 무조건 hit
				*clu_to_skip = au->free_clusters - num_to_wb;
				/* otherwise don't skip */
			}
			amap->n_need_packing = 0;
			return au;
		}
	}

	if ((PACKING_HARDLIMIT) && amap->n_need_packing >= PACKING_HARDLIMIT) {
		/* Compulsory SLC flushing:
		 * If there was no chance to do best-fit packing
		 * and the # of AU-aligned allocation exceeds HARD threshold,
		 * then pick a clean AU and generate a compulsory random I/O.
		 */
		au = amap_pop_cold_au_largest(amap, amap->clusters_per_au);
		if (au) {
			MMSG("AMAP: packing (cnt: %d) / hard-limit, largest)\n",
				amap->n_need_packing);

			if (au->free_clusters >= 96) {
				*clu_to_skip = au->free_clusters / 2;
				MMSG("AMAP: cluster idx re-position\n");
			}
			amap->n_need_packing = 0;
			return au;
		}
	}

	/* Update # of clean AU allocation */
	amap->n_need_packing++;
	return NULL;
}


/* Pick a target AU:
 * This function should be called
 * only if there are one or more free clusters in the bdev.
 */
TARGET_AU_T *amap_get_target_au(AMAP_T *amap, int dest, int num_to_wb)
{
	int loop_count = 0;

retry:
	if (++loop_count >= 3) {
		/* No space available (or AMAP consistency error)
		 * This could happen because of the ignored AUs but not likely
		 * (because the defrag daemon will not work if there is no enough space)
		 */
		BUG_ON(amap->slist_ignored.next == NULL);
		return NULL;
	}

	/* Hot clusters (DIR) */
	if (dest == ALLOC_HOT) {

		/* Working hot AU exist? */
		if (amap->cur_hot.au == NULL || amap->cur_hot.au->free_clusters == 0) {
			AU_INFO_T *au;

			if (amap->total_fclu_hot == 0) {
				/* No more hot AU avaialbe */
				dest = ALLOC_COLD;

				goto retry;
			}

			au = amap_find_hot_au_partial(amap);

			BUG_ON(au == NULL);
			BUG_ON(au->free_clusters <= 0);

			amap->cur_hot.au = au;
			amap->cur_hot.idx = 0;
			amap->cur_hot.clu_to_skip = 0;
		}

		/* Now allocate on a hot AU */
		return &amap->cur_hot;
	}

	/* Cold allocation:
	 * If amap->cur_cold.au has one or more free cluster(s),
	 * then just return amap->cur_cold
	 */
	if ((!amap->cur_cold.au)
		|| (amap->cur_cold.idx == amap->clusters_per_au)
		|| (amap->cur_cold.au->free_clusters == 0)) {

		AU_INFO_T *au = NULL;
		const AU_INFO_T *old_au = amap->cur_cold.au;
		int n_clu_to_skip = 0;

		if (old_au) {
			ASSERT(!IS_AU_WORKING(old_au, amap));
			/* must be NOT WORKING AU.
			 * (only for information gathering)
			 */
		}

		/* Next target AU is needed:
		 * There are 3 possible ALLOC options for cold AU
		 *
		 * ALLOC_COLD_ALIGNED: Clean AU first, but heuristic packing is ON
		 * ALLOC_COLD_PACKING: Packing AU first (usually for defrag)
		 * ALLOC_COLD_SEQ    : Sequential AU allocation (VFAT-like)
		 */

		/* Experimental: Modify allocation destination if needed (ALIGNED => SEQ) */
		// dest = amap_update_dest(amap, dest);

		if ((dest == ALLOC_COLD_SEQ) && old_au) {
			int i_au = old_au->idx + 1;

			while (i_au != old_au->idx) {
				au = GET_AU(amap, i_au);

				if ((au->free_clusters > 0) &&
					!IS_AU_HOT(au, amap) &&
					!IS_AU_IGNORED(au, amap)) {
					MMSG("AMAP: new cold AU(%d) with %d "
					     "clusters (seq)\n",
						au->idx, au->free_clusters);

					amap_remove_cold_au(amap, au);
					goto ret_new_cold;
				}
				i_au++;
				if (i_au >= amap->n_au)
					i_au = 0;
			}

			// no cold AUs are available => Hot allocation
			dest = ALLOC_HOT;
			goto retry;
		}


		/*
		 * Check if packing is needed
		 * (ALLOC_COLD_PACKING is treated by this function)
		 */
		au = amap_get_packing_au(amap, dest, num_to_wb, &n_clu_to_skip);
		if (au) {
			MMSG("AMAP: new cold AU(%d) with %d clusters "
				"(packing)\n", au->idx, au->free_clusters);
			goto ret_new_cold;
		}

		/* ALLOC_COLD_ALIGNED */
		/* Check if the adjacent AU is clean */
		if (old_au && ((old_au->idx + 1) < amap->n_au)) {
			au = GET_AU(amap, old_au->idx + 1);
			if ((au->free_clusters == amap->clusters_per_au) &&
						!IS_AU_HOT(au, amap) &&
						!IS_AU_IGNORED(au, amap)) {
				MMSG("AMAP: new cold AU(%d) with %d clusters "
					"(adjacent)\n", au->idx, au->free_clusters);
				amap_remove_cold_au(amap, au);
				goto ret_new_cold;
			}
		}

		/* Clean or largest AU */
		au = amap_pop_cold_au_largest(amap, 0);
		if (!au) {
			//ASSERT(amap->total_fclu_hot == (fsi->num_clusters - fsi->used_clusters - 2));
			dest = ALLOC_HOT;
			goto retry;
		}

		MMSG("AMAP: New cold AU (%d) with %d clusters\n",
				au->idx, au->free_clusters);

ret_new_cold:
		SET_AU_WORKING(au);

		amap->cur_cold.au = au;
		amap->cur_cold.idx = 0;
		amap->cur_cold.clu_to_skip = n_clu_to_skip;
	}

	return &amap->cur_cold;
}

/* Put and update target AU */
void amap_put_target_au(AMAP_T *amap, TARGET_AU_T *cur, unsigned int num_allocated)
{
	/* Update AMAP info vars. */
	if (num_allocated > 0 &&
		(cur->au->free_clusters + num_allocated) == amap->clusters_per_au) {
		/* if the target AU was a clean AU before this allocation ... */
		amap->n_clean_au--;
	}
	if (num_allocated > 0 &&
		cur->au->free_clusters == 0)
		amap->n_full_au++;

	if (IS_AU_HOT(cur->au, amap)) {
		/* Hot AU */
		MMSG("AMAP: hot allocation at AU %d\n", cur->au->idx);
		amap->total_fclu_hot -= num_allocated;

		/* Intra-AU round-robin */
		if (cur->idx >= amap->clusters_per_au)
			cur->idx = 0;

		/* No more space available */
		if (cur->au->free_clusters == 0)
			cur->au = NULL;

	} else {
		/* non-hot AU */
		ASSERT(IS_AU_WORKING(cur->au, amap));

		if (cur->idx >= amap->clusters_per_au || cur->au->free_clusters == 0) {
			/* It should be inserted back to AU MAP */
			cur->au->shead.head = NULL;		// SET_AU_NOT_WORKING
			amap_add_cold_au(amap, cur->au);

			// cur->au = NULL;	// This value will be used for the next AU selection
			cur->idx = amap->clusters_per_au;	// AU closing
		}
	}

}


/* Reposition target->idx for packing (Heuristics):
 * Skip (num_to_skip) free clusters in (cur->au)
 */
static inline int amap_skip_cluster(struct super_block *sb, TARGET_AU_T *cur, int num_to_skip)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	u32 clu, read_clu;
	MMSG_VAR(int num_to_skip_orig = num_to_skip);

	if (num_to_skip >= cur->au->free_clusters) {
		EMSG("AMAP(%s): skip mis-use. amap_566\n", __func__);
		return -EIO;
	}

	clu = CLU_of_i_AU(amap, cur->au->idx, cur->idx);
	while (num_to_skip > 0) {
		if (clu >= CLUS_BASE) {
			/* Cf.
			 * If AMAP's integrity is okay,
			 * we don't need to check if (clu < fsi->num_clusters)
			 */

			if (fat_ent_get(sb, clu, &read_clu))
				return -EIO;

			if (IS_CLUS_FREE(read_clu))
				num_to_skip--;
		}

		// Move clu->idx
		clu++;
		(cur->idx)++;

		if (cur->idx >= amap->clusters_per_au) {
			/* End of AU (Not supposed) */
			EMSG("AMAP: Skip - End of AU?! (amap_596)\n");
			cur->idx = 0;
			return -EIO;
		}
	}

	MMSG("AMAP: Skip_clusters (%d skipped => %d, among %d free clus)\n",
			num_to_skip_orig, cur->idx, cur->au->free_clusters);

	return 0;
}


/* AMAP-based allocation function for FAT32 */
s32 amap_fat_alloc_cluster(struct super_block *sb, u32 num_alloc, CHAIN_T *p_chain, s32 dest)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	TARGET_AU_T *cur = NULL;
	AU_INFO_T *target_au = NULL;				/* Allocation target AU */
	s32 ret = -ENOSPC;
	u32 last_clu = CLUS_EOF, read_clu;
	u32 new_clu, total_cnt;
	u32 num_allocated = 0, num_allocated_each = 0;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);

	BUG_ON(!amap);
	BUG_ON(IS_CLUS_EOF(fsi->used_clusters));

	total_cnt = fsi->num_clusters - CLUS_BASE;

	if (unlikely(total_cnt < fsi->used_clusters)) {
		sdfat_fs_error_ratelimit(sb,
				"AMAP(%s): invalid used clusters(t:%u,u:%u)\n",
				__func__, total_cnt, fsi->used_clusters);
		return -EIO;
	}

	if (num_alloc > total_cnt - fsi->used_clusters)
		return -ENOSPC;

	p_chain->dir = CLUS_EOF;

	set_sb_dirty(sb);

	// spin_lock(&amap->amap_lock);

retry_alloc:
	/* Allocation strategy implemented */
	cur = amap_get_target_au(amap, dest, fsi->reserved_clusters);
	if (unlikely(!cur)) {
		// There is no available AU (only ignored-AU are left)
		sdfat_msg(sb, KERN_ERR, "AMAP Allocator: no avaialble AU.");
		goto error;
	}

	/* If there are clusters to skip */
	if (cur->clu_to_skip > 0) {
		if (amap_skip_cluster(sb, &amap->cur_cold, cur->clu_to_skip)) {
			ret = -EIO;
			goto error;
		}
		cur->clu_to_skip = 0;
	}

	target_au = cur->au;

	/*
	 * cur->au  : target AU info pointer
	 * cur->idx : the intra-cluster idx in the AU to start from
	 */
	BUG_ON(!cur->au);
	BUG_ON(!cur->au->free_clusters);
	BUG_ON(cur->idx >= amap->clusters_per_au);

	num_allocated_each = 0;
	new_clu = CLU_of_i_AU(amap, target_au->idx, cur->idx);

	do {
		/* Allocate at the target AU */
		if ((new_clu >= CLUS_BASE) && (new_clu < fsi->num_clusters)) {
			if (fat_ent_get(sb, new_clu, &read_clu)) {
				// spin_unlock(&amap->amap_lock);
				ret = -EIO;
				goto error;
			}

			if (IS_CLUS_FREE(read_clu)) {
				BUG_ON(GET_AU(amap, i_AU_of_CLU(amap, new_clu)) != target_au);

				/* Free cluster found */
				if (fat_ent_set(sb, new_clu, CLUS_EOF)) {
					ret = -EIO;
					goto error;
				}

				num_allocated_each++;

				if (IS_CLUS_EOF(p_chain->dir)) {
					p_chain->dir = new_clu;
				} else {
					if (fat_ent_set(sb, last_clu, new_clu)) {
						ret = -EIO;
						goto error;
					}
				}
				last_clu = new_clu;

				/* Update au info */
				target_au->free_clusters--;
			}

		}

		new_clu++;
		(cur->idx)++;

		/* End of the AU */
		if ((cur->idx >= amap->clusters_per_au) || !(target_au->free_clusters))
			break;
	} while (num_allocated_each < num_alloc);

	/* Update strategy info */
	amap_put_target_au(amap, cur, num_allocated_each);


	num_allocated += num_allocated_each;
	fsi->used_clusters += num_allocated_each;
	num_alloc -= num_allocated_each;


	if (num_alloc > 0)
		goto retry_alloc;

	// spin_unlock(&amap->amap_lock);
	return 0;
error:
	if (num_allocated)
		fsi->fs_func->free_cluster(sb, p_chain, 0);
	return ret;
}


/* Free cluster for FAT32 (not implemented yet) */
s32 amap_free_cluster(struct super_block *sb, CHAIN_T *p_chain, s32 do_relse)
{
	return -ENOTSUPP;
}


/*
 * This is called by fat_free_cluster()
 * to update AMAP info.
 */
s32 amap_release_cluster(struct super_block *sb, u32 clu)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	AU_INFO_T *au;
	int i_au;

	// spin_lock(&amap->amap_lock);

	/* Update AU info */
	i_au = i_AU_of_CLU(amap, clu);
	BUG_ON(i_au >= amap->n_au);
	au = GET_AU(amap, i_au);
	if (au->free_clusters >= amap->clusters_per_au) {
		sdfat_fs_error(sb, "%s, au->free_clusters(%hd) is "
			"greater than or equal to amap->clusters_per_au(%hd)",
			__func__, au->free_clusters, amap->clusters_per_au);
		return -EIO;
	}

	if (IS_AU_HOT(au, amap)) {
		MMSG("AMAP: Hot cluster freed\n");
		au->free_clusters++;
		amap->total_fclu_hot++;
	} else if (!IS_AU_WORKING(au, amap) && !IS_AU_IGNORED(au, amap)) {
		/* Ordinary AU - update AU tree */
		// Can be optimized by implementing amap_update_au
		amap_remove_cold_au(amap, au);
		au->free_clusters++;
		amap_add_cold_au(amap, au);
	} else
		au->free_clusters++;


	/* Update AMAP info */
	if (au->free_clusters == amap->clusters_per_au)
		amap->n_clean_au++;
	if (au->free_clusters == 1)
		amap->n_full_au--;

	// spin_unlock(&amap->amap_lock);
	return 0;
}


/*
 * Check if the cluster is in a working AU
 * The caller should hold sb lock.
 * This func. should be used only if smart allocation is on
 */
s32 amap_check_working(struct super_block *sb, u32 clu)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	AU_INFO_T *au;

	BUG_ON(!amap);
	au = GET_AU(amap, i_AU_of_CLU(amap, clu));
	return IS_AU_WORKING(au, amap);
}


/*
 * Return the # of free clusters in that AU
 */
s32 amap_get_freeclus(struct super_block *sb, u32 clu)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	AU_INFO_T *au;

	BUG_ON(!amap);
	au = GET_AU(amap, i_AU_of_CLU(amap, clu));
	return (s32)au->free_clusters;
}


/*
 * Add the AU containing 'clu' to the ignored AU list.
 * The AU will not be used by the allocator.
 *
 * XXX: Ignored counter needed
 */
s32 amap_mark_ignore(struct super_block *sb, u32 clu)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	AU_INFO_T *au;

	BUG_ON(!amap);
	au = GET_AU(amap, i_AU_of_CLU(amap, clu));

	if (IS_AU_HOT(au, amap)) {
		/* Doesn't work with hot AUs */
		return -EPERM;
	} else if (IS_AU_WORKING(au, amap)) {
		return -EBUSY;
	}

	//BUG_ON(IS_AU_IGNORED(au, amap) && (GET_IGN_CNT(au) == 0));
	if (IS_AU_IGNORED(au, amap))
		return 0;

	amap_remove_cold_au(amap, au);
	amap_insert_to_list(au, &amap->slist_ignored);

	BUG_ON(!IS_AU_IGNORED(au, amap));

	//INC_IGN_CNT(au);
	MMSG("AMAP: Mark ignored AU (%d)\n", au->idx);
	return 0;
}


/*
 * This function could be used only on IGNORED AUs.
 * The caller should care whether it's ignored or not before using this func.
 */
s32 amap_unmark_ignore(struct super_block *sb, u32 clu)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	AU_INFO_T *au;

	BUG_ON(!amap);

	au = GET_AU(amap, i_AU_of_CLU(amap, clu));

	BUG_ON(!IS_AU_IGNORED(au, amap));
	// BUG_ON(GET_IGN_CNT(au) == 0);

	amap_remove_from_list(au, &amap->slist_ignored);
	amap_add_cold_au(amap, au);

	BUG_ON(IS_AU_IGNORED(au, amap));

	//DEC_IGN_CNT(au);

	MMSG("AMAP: Unmark ignored AU (%d)\n", au->idx);

	return 0;
}

/*
 * Unmark all ignored AU
 * This will return # of unmarked AUs
 */
s32 amap_unmark_ignore_all(struct super_block *sb)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;
	struct slist_head *entry;
	AU_INFO_T *au;
	int n = 0;

	BUG_ON(!amap);
	entry = amap->slist_ignored.next;
	while (entry) {
		au = list_entry(entry, AU_INFO_T, shead);

		BUG_ON(au != GET_AU(amap, au->idx));
		BUG_ON(!IS_AU_IGNORED(au, amap));

		//CLEAR_IGN_CNT(au);
		amap_remove_from_list(au, &amap->slist_ignored);
		amap_add_cold_au(amap, au);

		MMSG("AMAP: Unmark ignored AU (%d)\n", au->idx);
		n++;

		entry = amap->slist_ignored.next;
	}

	BUG_ON(amap->slist_ignored.next != NULL);
	MMSG("AMAP: unmark_ignore_all, total %d AUs\n", n);

	return n;
}

/**
 * @fn          amap_get_au_stat
 * @brief       report AUs status depending on mode
 * @return      positive on success, 0 otherwise
 * @param       sbi             super block info
 * @param       mode    TOTAL, CLEAN and FULL
 */
u32 amap_get_au_stat(struct super_block *sb, s32 mode)
{
	AMAP_T *amap = SDFAT_SB(sb)->fsi.amap;

	if (!amap)
		return 0;

	if (mode == VOL_AU_STAT_TOTAL)
		return amap->n_au;
	else if (mode == VOL_AU_STAT_CLEAN)
		return amap->n_clean_au;
	else if (mode == VOL_AU_STAT_FULL)
		return amap->n_full_au;

	return 0;
}


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

#ifndef _SDFAT_AMAP_H
#define _SDFAT_AMAP_H

#include <linux/fs.h>
#include <linux/list.h>
#include <linux/rbtree.h>

/* AMAP Configuration Variable */
#define SMART_ALLOC_N_HOT_AU    (5)

/* Allocating Destination (for smart allocator):
 * moved to sdfat.h
 */
/*
 * #define ALLOC_COLD_ALIGNED	(1)
 * #define ALLOC_COLD_PACKING	(2)
 * #define ALLOC_COLD_SEQ	(4)
 */

/* Minimum sectors for support AMAP create */
#define AMAP_MIN_SUPPORT_SECTORS	(1048576)

#define amap_add_hot_au(amap, au) amap_insert_to_list(au, &amap->slist_hot)

/* singly linked list */
struct slist_head {
	struct slist_head *next;
	struct slist_head *head;
};

/* AU entry type */
typedef struct __AU_INFO_T {
	uint16_t idx;			/* the index of the AU (0, 1, 2, ... ) */
	uint16_t free_clusters;		/* # of available cluster */
	union {
		struct list_head head;
		struct slist_head shead;/* singly linked list head for hot list */
	};
} AU_INFO_T;


/* Allocation Target AU */
typedef struct __TARGET_AU_T {
	AU_INFO_T *au;			/* Working AU */
	uint16_t idx;			/* Intra-AU cluster index */
	uint16_t clu_to_skip;		/* Clusters to skip */
} TARGET_AU_T;


/* AMAP free-clusters-based node */
typedef struct {
	struct list_head head;		/* the list of AUs */
} FCLU_NODE_T;


/* AMAP options */
typedef struct {
	unsigned int packing_ratio;	/* Tunable packing ratio */
	unsigned int au_size;		/* AU size in sectors */
	unsigned int au_align_factor;	/* Hidden sectors % au_size */
} AMAP_OPT_T;

typedef struct __AMAP_T {
	spinlock_t amap_lock;		/* obsolete */
	struct super_block *sb;

	int n_au;
	int n_clean_au, n_full_au;
	int clu_align_bias;
	uint16_t clusters_per_au;
	AU_INFO_T **au_table;		/* An array of AU_INFO entries */
	AMAP_OPT_T option;

	/* Size-based AU management pool (cold) */
	FCLU_NODE_T *fclu_nodes;	/* An array of listheads */
	int fclu_order;			/* Page order that fclu_nodes needs */
	int fclu_hint;			/* maximum # of free clusters in an AU */

	/* Hot AU list */
	unsigned int total_fclu_hot;	/* Free clusters in hot list */
	struct slist_head slist_hot;	/* Hot AU list */

	/* Ignored AU list */
	struct slist_head slist_ignored;

	/* Allocator variables (keep 2 AUs at maximum) */
	TARGET_AU_T cur_cold;
	TARGET_AU_T cur_hot;
	int n_need_packing;
} AMAP_T;


/* AU table */
#define N_AU_PER_TABLE		(int)(PAGE_SIZE / sizeof(AU_INFO_T))
#define GET_AU(amap, i_AU)	(amap->au_table[(i_AU) / N_AU_PER_TABLE] + ((i_AU) % N_AU_PER_TABLE))
//#define MAX_CLU_PER_AU		(int)(PAGE_SIZE / sizeof(FCLU_NODE_T))
#define MAX_CLU_PER_AU		(1024)

/* Cold AU bucket <-> # of freeclusters */
#define NODE_CLEAN(amap) (&amap->fclu_nodes[amap->clusters_per_au - 1])
#define NODE(fclu, amap) (&amap->fclu_nodes[fclu - 1])
#define FREE_CLUSTERS(node, amap) ((int)(node - amap->fclu_nodes) + 1)

/* AU status */
#define MAGIC_WORKING	((struct slist_head *)0xFFFF5091)
#define IS_AU_HOT(au, amap)	(au->shead.head == &amap->slist_hot)
#define IS_AU_IGNORED(au, amap)	(au->shead.head == &amap->slist_ignored)
#define IS_AU_WORKING(au, amap)	(au->shead.head == MAGIC_WORKING)
#define SET_AU_WORKING(au)	(au->shead.head = MAGIC_WORKING)

/* AU <-> cluster */
#define i_AU_of_CLU(amap, clu)	((amap->clu_align_bias + clu) / amap->clusters_per_au)
#define CLU_of_i_AU(amap, i_au, idx)	\
	((uint32_t)(i_au) * (uint32_t)amap->clusters_per_au + (idx) - amap->clu_align_bias)

/*
 * NOTE : AMAP internal functions are moved to core.h
 */

#endif /* _SDFAT_AMAP_H */

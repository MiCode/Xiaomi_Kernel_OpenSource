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

/*
 *  linux/fs/fat/cache.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *
 *  Mar 1999. AV. Changed cache, so that it uses the starting cluster instead
 *	of inode number.
 *  May 1999. AV. Fixed the bogosity with FAT32 (read "FAT28"). Fscking lusers.
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : extent.c                                                  */
/*  PURPOSE : Improve the performance of traversing fat chain.          */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/slab.h>
#include "sdfat.h"
#include "core.h"

#define EXTENT_CACHE_VALID	0
/* this must be > 0. */
#define EXTENT_MAX_CACHE	16

struct extent_cache {
	struct list_head cache_list;
	u32 nr_contig;	/* number of contiguous clusters */
	u32 fcluster;	/* cluster number in the file. */
	u32 dcluster;	/* cluster number on disk. */
};

struct extent_cache_id {
	u32 id;
	u32 nr_contig;
	u32 fcluster;
	u32 dcluster;
};

static struct kmem_cache *extent_cache_cachep;

static void init_once(void *c)
{
	struct extent_cache *cache = (struct extent_cache *)c;

	INIT_LIST_HEAD(&cache->cache_list);
}

s32 extent_cache_init(void)
{
	extent_cache_cachep = kmem_cache_create("sdfat_extent_cache",
				sizeof(struct extent_cache),
				0, SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD,
				init_once);
	if (!extent_cache_cachep)
		return -ENOMEM;
	return 0;
}

void extent_cache_shutdown(void)
{
	if (!extent_cache_cachep)
		return;
	kmem_cache_destroy(extent_cache_cachep);
}

void extent_cache_init_inode(struct inode *inode)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);

	spin_lock_init(&extent->cache_lru_lock);
	extent->nr_caches = 0;
	extent->cache_valid_id = EXTENT_CACHE_VALID + 1;
	INIT_LIST_HEAD(&extent->cache_lru);
}

static inline struct extent_cache *extent_cache_alloc(void)
{
	return kmem_cache_alloc(extent_cache_cachep, GFP_NOFS);
}

static inline void extent_cache_free(struct extent_cache *cache)
{
	BUG_ON(!list_empty(&cache->cache_list));
	kmem_cache_free(extent_cache_cachep, cache);
}

static inline void extent_cache_update_lru(struct inode *inode,
					struct extent_cache *cache)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);

	if (extent->cache_lru.next != &cache->cache_list)
		list_move(&cache->cache_list, &extent->cache_lru);
}

static u32 extent_cache_lookup(struct inode *inode, u32 fclus,
			    struct extent_cache_id *cid,
			    u32 *cached_fclus, u32 *cached_dclus)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);

	static struct extent_cache nohit = { .fcluster = 0, };

	struct extent_cache *hit = &nohit, *p;
	u32 offset = CLUS_EOF;

	spin_lock(&extent->cache_lru_lock);
	list_for_each_entry(p, &extent->cache_lru, cache_list) {
		/* Find the cache of "fclus" or nearest cache. */
		if (p->fcluster <= fclus && hit->fcluster < p->fcluster) {
			hit = p;
			if ((hit->fcluster + hit->nr_contig) < fclus) {
				offset = hit->nr_contig;
			} else {
				offset = fclus - hit->fcluster;
				break;
			}
		}
	}
	if (hit != &nohit) {
		extent_cache_update_lru(inode, hit);

		cid->id = extent->cache_valid_id;
		cid->nr_contig = hit->nr_contig;
		cid->fcluster = hit->fcluster;
		cid->dcluster = hit->dcluster;
		*cached_fclus = cid->fcluster + offset;
		*cached_dclus = cid->dcluster + offset;
	}
	spin_unlock(&extent->cache_lru_lock);

	return offset;
}

static struct extent_cache *extent_cache_merge(struct inode *inode,
					 struct extent_cache_id *new)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);

	struct extent_cache *p;

	list_for_each_entry(p, &extent->cache_lru, cache_list) {
		/* Find the same part as "new" in cluster-chain. */
		if (p->fcluster == new->fcluster) {
			ASSERT(p->dcluster == new->dcluster);
			if (new->nr_contig > p->nr_contig)
				p->nr_contig = new->nr_contig;
			return p;
		}
	}
	return NULL;
}

static void extent_cache_add(struct inode *inode, struct extent_cache_id *new)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);

	struct extent_cache *cache, *tmp;

	if (new->fcluster == -1) /* dummy cache */
		return;

	spin_lock(&extent->cache_lru_lock);
	if (new->id != EXTENT_CACHE_VALID &&
	    new->id != extent->cache_valid_id)
		goto out;	/* this cache was invalidated */

	cache = extent_cache_merge(inode, new);
	if (cache == NULL) {
		if (extent->nr_caches < EXTENT_MAX_CACHE) {
			extent->nr_caches++;
			spin_unlock(&extent->cache_lru_lock);

			tmp = extent_cache_alloc();
			if (!tmp) {
				spin_lock(&extent->cache_lru_lock);
				extent->nr_caches--;
				spin_unlock(&extent->cache_lru_lock);
				return;
			}

			spin_lock(&extent->cache_lru_lock);
			cache = extent_cache_merge(inode, new);
			if (cache != NULL) {
				extent->nr_caches--;
				extent_cache_free(tmp);
				goto out_update_lru;
			}
			cache = tmp;
		} else {
			struct list_head *p = extent->cache_lru.prev;
			cache = list_entry(p, struct extent_cache, cache_list);
		}
		cache->fcluster = new->fcluster;
		cache->dcluster = new->dcluster;
		cache->nr_contig = new->nr_contig;
	}
out_update_lru:
	extent_cache_update_lru(inode, cache);
out:
	spin_unlock(&extent->cache_lru_lock);
}

/*
 * Cache invalidation occurs rarely, thus the LRU chain is not updated. It
 * fixes itself after a while.
 */
static void __extent_cache_inval_inode(struct inode *inode)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);
	struct extent_cache *cache;

	while (!list_empty(&extent->cache_lru)) {
		cache = list_entry(extent->cache_lru.next,
				   struct extent_cache, cache_list);
		list_del_init(&cache->cache_list);
		extent->nr_caches--;
		extent_cache_free(cache);
	}
	/* Update. The copy of caches before this id is discarded. */
	extent->cache_valid_id++;
	if (extent->cache_valid_id == EXTENT_CACHE_VALID)
		extent->cache_valid_id++;
}

void extent_cache_inval_inode(struct inode *inode)
{
	EXTENT_T *extent = &(SDFAT_I(inode)->fid.extent);

	spin_lock(&extent->cache_lru_lock);
	__extent_cache_inval_inode(inode);
	spin_unlock(&extent->cache_lru_lock);
}

static inline s32 cache_contiguous(struct extent_cache_id *cid, u32 dclus)
{
	cid->nr_contig++;
	return ((cid->dcluster + cid->nr_contig) == dclus);
}

static inline void cache_init(struct extent_cache_id *cid, u32 fclus, u32 dclus)
{
	cid->id = EXTENT_CACHE_VALID;
	cid->fcluster = fclus;
	cid->dcluster = dclus;
	cid->nr_contig = 0;
}

s32 extent_get_clus(struct inode *inode, u32 cluster, u32 *fclus,
		u32 *dclus, u32 *last_dclus, s32 allow_eof)
{
	struct super_block *sb = inode->i_sb;
	FS_INFO_T *fsi = &(SDFAT_SB(sb)->fsi);
	u32 limit = fsi->num_clusters;
	FILE_ID_T *fid = &(SDFAT_I(inode)->fid);
	struct extent_cache_id cid;
	u32 content;

	/* FOR GRACEFUL ERROR HANDLING */
	if (IS_CLUS_FREE(fid->start_clu)) {
		sdfat_fs_error(sb, "invalid access to "
			"extent cache (entry 0x%08x)", fid->start_clu);
		ASSERT(0);
		return -EIO;
	}

	*fclus = 0;
	*dclus = fid->start_clu;
	*last_dclus = *dclus;

	/*
	 * Don`t use extent_cache if zero offset or non-cluster allocation
	 */
	if ((cluster == 0) || IS_CLUS_EOF(*dclus))
		return 0;

	cache_init(&cid, CLUS_EOF, CLUS_EOF);

	if (extent_cache_lookup(inode, cluster, &cid, fclus, dclus) == CLUS_EOF) {
		/*
		 * dummy, always not contiguous
		 * This is reinitialized by cache_init(), later.
		 */
		ASSERT((cid.id == EXTENT_CACHE_VALID)
			&& (cid.fcluster == CLUS_EOF)
			&& (cid.dcluster == CLUS_EOF)
			&& (cid.nr_contig == 0));
	}

	if (*fclus == cluster)
		return 0;

	while (*fclus < cluster) {
		/* prevent the infinite loop of cluster chain */
		if (*fclus > limit) {
			sdfat_fs_error(sb,
				"%s: detected the cluster chain loop"
				" (i_pos %u)", __func__,
				(*fclus));
			return -EIO;
		}

		if (fat_ent_get_safe(sb, *dclus, &content))
			return -EIO;

		*last_dclus = *dclus;
		*dclus = content;
		(*fclus)++;

		if (IS_CLUS_EOF(content)) {
			if (!allow_eof) {
				sdfat_fs_error(sb,
				       "%s: invalid cluster chain (i_pos %u,"
				       "last_clus 0x%08x is EOF)",
				       __func__, *fclus, (*last_dclus));
				return -EIO;
			}

			break;
		}

		if (!cache_contiguous(&cid, *dclus))
			cache_init(&cid, *fclus, *dclus);
	}

	extent_cache_add(inode, &cid);
	return 0;
}

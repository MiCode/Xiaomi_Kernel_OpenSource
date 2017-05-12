/*
 * Copyright (C) 2017 Joonsoo Kim.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/vmalloc.h>
#include <linux/jhash.h>
#include <linux/highmem.h>

#include "zram_drv.h"

/* One slot will contain 128 pages theoretically */
#define ZRAM_HASH_SHIFT		7
#define ZRAM_HASH_SIZE_MIN	(1 << 10)
#define ZRAM_HASH_SIZE_MAX	(1 << 31)

u64 zram_dedup_dup_size(struct zram *zram)
{
	return (u64)atomic64_read(&zram->stats.dup_data_size);
}

u64 zram_dedup_meta_size(struct zram *zram)
{
	return (u64)atomic64_read(&zram->stats.meta_data_size);
}

static u32 zram_dedup_checksum(unsigned char *mem)
{
	return jhash(mem, PAGE_SIZE, 0);
}

void zram_dedup_insert(struct zram *zram, struct zram_entry *new,
				u32 checksum)
{
	struct zram_hash *hash;
	struct rb_root *rb_root;
	struct rb_node **rb_node, *parent = NULL;
	struct zram_entry *entry;

	if (!zram_dedup_enabled(zram))
		return;

	new->checksum = checksum;
	hash = &zram->hash[checksum % zram->hash_size];
	rb_root = &hash->rb_root;

	spin_lock(&hash->lock);
	rb_node = &rb_root->rb_node;
	while (*rb_node) {
		parent = *rb_node;
		entry = rb_entry(parent, struct zram_entry, rb_node);
		if (checksum < entry->checksum)
			rb_node = &parent->rb_left;
		else if (checksum > entry->checksum)
			rb_node = &parent->rb_right;
		else
			rb_node = &parent->rb_left;
	}

	rb_link_node(&new->rb_node, parent, rb_node);
	rb_insert_color(&new->rb_node, rb_root);
	spin_unlock(&hash->lock);
}

static bool zram_dedup_match(struct zram *zram, struct zram_entry *entry,
				unsigned char *mem)
{
	bool match = false;
	unsigned char *cmem;
	struct zcomp_strm *zstrm;

	cmem = zs_map_object(zram->mem_pool, entry->handle, ZS_MM_RO);
	if (entry->len == PAGE_SIZE) {
		match = !memcmp(mem, cmem, PAGE_SIZE);
	} else {
		zstrm = zcomp_stream_get(zram->comp);
		if (!zcomp_decompress(zstrm, cmem, entry->len, zstrm->buffer))
			match = !memcmp(mem, zstrm->buffer, PAGE_SIZE);
		zcomp_stream_put(zram->comp);
	}
	zs_unmap_object(zram->mem_pool, entry->handle);

	return match;
}

static unsigned long zram_dedup_put(struct zram *zram,
				struct zram_entry *entry)
{
	struct zram_hash *hash;
	u32 checksum;

	checksum = entry->checksum;
	hash = &zram->hash[checksum % zram->hash_size];

	spin_lock(&hash->lock);

	entry->refcount--;
	if (!entry->refcount)
		rb_erase(&entry->rb_node, &hash->rb_root);
	else
		atomic64_sub(entry->len, &zram->stats.dup_data_size);

	spin_unlock(&hash->lock);

	return entry->refcount;
}

static struct zram_entry *zram_dedup_get(struct zram *zram,
				unsigned char *mem, u32 checksum)
{
	struct zram_hash *hash;
	struct zram_entry *entry;
	struct rb_node *rb_node;

	hash = &zram->hash[checksum % zram->hash_size];

	spin_lock(&hash->lock);
	rb_node = hash->rb_root.rb_node;
	while (rb_node) {
		entry = rb_entry(rb_node, struct zram_entry, rb_node);
		if (checksum == entry->checksum) {
			entry->refcount++;
			atomic64_add(entry->len, &zram->stats.dup_data_size);
			spin_unlock(&hash->lock);

			if (zram_dedup_match(zram, entry, mem))
				return entry;

			zram_entry_free(zram, entry);

			return NULL;
		}

		if (checksum < entry->checksum)
			rb_node = rb_node->rb_left;
		else
			rb_node = rb_node->rb_right;
	}
	spin_unlock(&hash->lock);

	return NULL;
}

struct zram_entry *zram_dedup_find(struct zram *zram, struct page *page,
				u32 *checksum)
{
	void *mem;
	struct zram_entry *entry;

	if (!zram_dedup_enabled(zram))
		return NULL;

	mem = kmap_atomic(page);
	*checksum = zram_dedup_checksum(mem);

	entry = zram_dedup_get(zram, mem, *checksum);
	kunmap_atomic(mem);

	return entry;
}

void zram_dedup_init_entry(struct zram *zram, struct zram_entry *entry,
				unsigned long handle, unsigned int len)
{
	if (!zram_dedup_enabled(zram))
		return;

	entry->handle = handle;
	entry->refcount = 1;
	entry->len = len;
}

bool zram_dedup_put_entry(struct zram *zram, struct zram_entry *entry)
{
	if (!zram_dedup_enabled(zram))
		return true;

	if (zram_dedup_put(zram, entry))
		return false;

	return true;
}

int zram_dedup_init(struct zram *zram, size_t num_pages)
{
	int i;
	struct zram_hash *hash;

	if (!zram_dedup_enabled(zram))
		return 0;

	zram->hash_size = num_pages >> ZRAM_HASH_SHIFT;
	zram->hash_size = min_t(size_t, ZRAM_HASH_SIZE_MAX, zram->hash_size);
	zram->hash_size = max_t(size_t, ZRAM_HASH_SIZE_MIN, zram->hash_size);
	zram->hash = vzalloc(zram->hash_size * sizeof(struct zram_hash));
	if (!zram->hash) {
		pr_err("Error allocating zram entry hash\n");
		return -ENOMEM;
	}

	for (i = 0; i < zram->hash_size; i++) {
		hash = &zram->hash[i];
		spin_lock_init(&hash->lock);
		hash->rb_root = RB_ROOT;
	}

	return 0;
}

void zram_dedup_fini(struct zram *zram)
{
	vfree(zram->hash);
	zram->hash = NULL;
	zram->hash_size = 0;
}

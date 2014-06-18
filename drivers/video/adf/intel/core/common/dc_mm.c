/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>

#include "core/common/dc_mm.h"

static inline void dc_mm_insert_pages(struct dc_mm *mm, u32 offset,
	struct page **pages, size_t n_pages)
{
	mm->insert(mm, offset, pages, n_pages);
}

static inline void dc_mm_clear_pages(struct dc_mm *mm, u32 offset,
	size_t n_pages)
{
	struct page *pages[1];
	size_t i;

	pages[0] = mm->blank_page;
	for (i = 0; i < n_pages; i++)
		mm->insert(mm, (offset + i), pages, 1);
}

static int dc_mm_find_ht_locked(struct dc_mm *mm, u32 tgid,
	struct dc_mm_per_thread_entry **hentry)
{
	struct drm_hash_item *entry;
	int err;

	err = drm_ht_find_item(&mm->ht, tgid, &entry);
	if (err)
		return err;

	*hentry = container_of(entry, struct dc_mm_per_thread_entry, item);

	return 0;
}

static int dc_mm_insert_ht_locked(struct dc_mm *mm, u32 tgid,
	struct dc_mm_per_thread_entry *hentry)
{
	struct drm_hash_item *item;
	int err;

	if (!hentry)
		return -EINVAL;

	item = &hentry->item;
	item->key = tgid;

	/**
	 * NOTE: drm_ht_insert_item will perform such a check
	ret = psb_gtt_mm_get_ht_by_pid(mm, tgid, &tmp);
	if (!ret) {
		DRM_DEBUG("Entry already exists for pid %ld\n", tgid);
		return -EAGAIN;
	}
	*/

	/*Insert the given entry */
	err = drm_ht_insert_item(&mm->ht, item);
	if (err)
		return err;
	mm->n_entries++;
	return 0;
}

static int dc_mm_alloc_insert_ht_locked(struct dc_mm *mm,
	u32 tgid, struct dc_mm_per_thread_entry **entry)
{
	struct dc_mm_per_thread_entry *hentry;
	int err;

	/*if the hentry for this tgid exists, just get it and return */
	err = dc_mm_find_ht_locked(mm, tgid, &hentry);
	if (!err) {
		kref_get(&hentry->ref);
		*entry = hentry;
		return 0;
	}

	hentry = kzalloc(sizeof(struct dc_mm_per_thread_entry), GFP_KERNEL);
	if (!hentry) {
		err = -ENOMEM;
		goto err_out0;
	}

	err = drm_ht_create(&hentry->ht, PAGE_SHIFT);
	if (err)
		goto err_out1;

	err = dc_mm_insert_ht_locked(mm, tgid, hentry);
	if (err)
		goto err_out2;

	kref_init(&hentry->ref);
	hentry->parent = &mm->ht;
	*entry = hentry;

	return 0;
err_out2:
	drm_ht_remove(&hentry->ht);
err_out1:
	kfree(hentry);
err_out0:
	return err;
}

static int dc_mm_get_mem_mapping_locked(struct drm_open_hash *ht,
	u32 key, struct dc_mm_buffer_mem_mapping **hentry)
{
	struct drm_hash_item *entry;
	struct dc_mm_buffer_mem_mapping *mapping;
	int ret;

	ret = drm_ht_find_item(ht, key, &entry);
	if (ret)
		return ret;

	mapping = container_of(entry, struct dc_mm_buffer_mem_mapping, item);
	if (!mapping)
		return -EINVAL;

	*hentry = mapping;
	return 0;
}

static int dc_mm_insert_mem_mapping_locked(
	struct dc_mm_per_thread_entry *hentry, u32 key,
	struct dc_mm_buffer_mem_mapping *mapping)
{
	struct drm_hash_item *item;
	int err;

	item = &mapping->item;
	item->key = key;

	err = drm_ht_insert_item(&hentry->ht, item);
	if (err)
		return err;
	hentry->n_entries++;

	return 0;
}

static int dc_mm_alloc_insert_mem_mapping_locked(struct dc_mm *mm,
	struct dc_mm_per_thread_entry *hentry, u32 key,
	struct drm_mm_node *node, struct dc_mm_buffer_mem_mapping **entry)
{
	struct dc_mm_buffer_mem_mapping *mapping;
	int err;

	/*try to get this mem_map */
	err = dc_mm_get_mem_mapping_locked(&hentry->ht, key, &mapping);
	if (!err) {
		kref_get(&mapping->ref);
		*entry = mapping;
		return 0;
	}

	mapping = kzalloc(sizeof(struct dc_mm_buffer_mem_mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	mapping->node = node;

	err = dc_mm_insert_mem_mapping_locked(hentry, key, mapping);
	if (err)
		goto out_err0;

	kref_init(&mapping->ref);
	mapping->parent = &hentry->ht;
	*entry = mapping;

	return 0;
out_err0:
	kfree(mapping);
	return err;
}

static int dc_mm_add_node_locked(struct dc_mm *mm, u32 tgid, u32 key,
	struct drm_mm_node *node, struct dc_mm_buffer_mem_mapping **entry)
{
	struct dc_mm_per_thread_entry *hentry;
	struct dc_mm_buffer_mem_mapping *mapping;
	int ret;

	ret = dc_mm_alloc_insert_ht_locked(mm, tgid, &hentry);
	if (ret)
		return ret;

	ret = dc_mm_alloc_insert_mem_mapping_locked(mm, hentry, key, node,
		&mapping);
	if (ret)
		return ret;

	*entry = mapping;

	return 0;
}

static int dc_mm_alloc_mem_locked(struct dc_mm *mm, uint32_t pages,
	uint32_t align, struct drm_mm_node **node)
{
	struct drm_mm_node *tmp_node;
	int ret;

	do {
		ret = drm_mm_pre_get(&mm->base);
		if (unlikely(ret))
			return ret;

		tmp_node = drm_mm_search_free(&mm->base, pages, align, 1);
		if (unlikely(!tmp_node))
			break;

		tmp_node = drm_mm_get_block_atomic(tmp_node, pages, align);
	} while (!tmp_node);

	if (!tmp_node)
		return -ENOMEM;

	*node = tmp_node;
	return 0;
}

static void dc_mm_destroy_per_thread_entry_locked(struct kref *ref)
{
	struct dc_mm_per_thread_entry *hentry =
		container_of(ref, struct dc_mm_per_thread_entry, ref);
	struct dc_mm *mm = container_of(hentry->parent, struct dc_mm, ht);

	/*remove it from mm*/
	drm_ht_remove_item(&mm->ht, &hentry->item);
	/*remove hash table*/
	drm_ht_remove(&hentry->ht);
	/*free this entry*/
	kfree(hentry);

	--mm->n_entries;
}

static void dc_mm_destroy_mem_mapping_locked(struct kref *ref)
{
	struct dc_mm_buffer_mem_mapping *mapping =
		container_of(ref, struct dc_mm_buffer_mem_mapping, ref);
	struct dc_mm_per_thread_entry *hentry =
		container_of(mapping->parent, struct dc_mm_per_thread_entry,
			ht);
	struct drm_mm_node *node = mapping->node;
	struct dc_mm *mm = container_of(hentry->parent, struct dc_mm, ht);

	/*remove this mapping from per thread table*/
	drm_ht_remove_item(&hentry->ht, &mapping->item);
	/*remove from MMU*/
	dc_mm_clear_pages(mm, node->start, node->size);
	/*release this node*/
	drm_mm_put_block(node);
	/*free this mapping*/
	kfree(mapping);

	/**
	 * there's no mapping for this thread, continue destroying
	 * per thread table
	 */
	if (--hentry->n_entries)
		kref_put(&hentry->ref,
			dc_mm_destroy_per_thread_entry_locked);
}

static int dc_mm_remove_free_mem_mapping_locked(struct drm_open_hash *ht,
	u32 key)
{
	struct dc_mm_buffer_mem_mapping *mapping;
	int err;

	err = dc_mm_get_mem_mapping_locked(ht, key, &mapping);
	if (err)
		return err;

	kref_put(&mapping->ref, dc_mm_destroy_mem_mapping_locked);

	return 0;
}

static int dc_mm_remove_node_locked(struct dc_mm *mm, u32 tgid, u32 key)
{
	struct dc_mm_per_thread_entry *hentry;
	int err;

	err = dc_mm_find_ht_locked(mm, tgid, &hentry);
	if (err)
		return err;

	/*remove mapping entry */
	return dc_mm_remove_free_mem_mapping_locked(&hentry->ht, key);
}

int dc_mm_buffer_import(struct dc_mm *mm, u32 handle, struct page **pages,
	size_t n_pages, u32 *addr)
{
	struct drm_mm_node *node;
	struct dc_mm_buffer_mem_mapping *mapping;
	u32 offset_pages;
	int err = 0;

	if (!mm || !mm->insert || !pages || !n_pages || !addr)
		return -EINVAL;

	mutex_lock(&mm->lock);

	/*alloc memory in TT apeture */
	err = dc_mm_alloc_mem_locked(mm, n_pages, 0, &node);
	if (err) {
		pr_info("%s: alloc TT memory error\n", __func__);
		goto out_err0;
	}

	/*update psb_gtt_mm */
	err = dc_mm_add_node_locked(mm, task_tgid_nr(current), handle,
		node, &mapping);
	if (err) {
		pr_info("%s: add_node failed\n", __func__);
		goto out_err1;
	}

	node = mapping->node;
	offset_pages = node->start;

	/*insert pages*/
	dc_mm_insert_pages(mm, offset_pages, pages, n_pages);

	*addr = offset_pages;

	mutex_unlock(&mm->lock);
	return 0;
out_err1:
	drm_mm_put_block(node);
out_err0:
	mutex_unlock(&mm->lock);
	return err;
}

void dc_mm_buffer_free(struct dc_mm *mm, u32 handle)
{

	if (!mm)
		return;

	mutex_lock(&mm->lock);

	dc_mm_remove_node_locked(mm, task_tgid_nr(current), handle);

	mutex_unlock(&mm->lock);
}

int dc_mm_init(struct dc_mm *mm, u32 start, size_t n_pages,
	page_table_insert_t insert_func)
{
	struct page *blank_page;
	int err;

	if (!mm || !insert_func)
		return -EINVAL;

	memset(mm, 0, sizeof(*mm));

	blank_page = alloc_page(GFP_DMA32 | __GFP_ZERO);
	if (!blank_page)
		return -ENOMEM;
	get_page(blank_page);
	set_pages_uc(blank_page, 1);

	mm->insert = insert_func;
	mm->blank_page = blank_page;

	err = drm_ht_create(&mm->ht, PAGE_SHIFT);
	if (err)
		goto out_err0;

	drm_mm_init(&mm->base, start, n_pages);

	mutex_init(&mm->lock);

	dc_mm_clear_pages(mm, start, n_pages);
	return 0;
out_err0:
	put_page(blank_page);
	__free_page(blank_page);
	return err;
}

void dc_mm_destroy(struct dc_mm *mm)
{
	if (mm) {
		put_page(mm->blank_page);
		__free_page(mm->blank_page);
		drm_mm_takedown(&mm->base);
		drm_ht_remove(&mm->ht);
	}
}


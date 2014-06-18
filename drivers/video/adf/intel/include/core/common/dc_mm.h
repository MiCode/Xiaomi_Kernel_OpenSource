/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#ifndef DC_MM_H_
#define DC_MM_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <drm/drm_mm.h>
#include <drm/drm_hashtab.h>

struct dc_mm;

typedef void (*page_table_insert_t)(struct dc_mm *mm, u32 offset_pages,
	struct page **pages, size_t n_pages);

struct dc_mm {
	struct drm_mm base;
	struct drm_open_hash ht;
	size_t n_entries;
	page_table_insert_t insert;
	struct page *blank_page;
	struct mutex lock;
};

struct dc_mm_per_thread_entry {
	struct kref ref;
	struct drm_open_hash ht;
	size_t n_entries;
	struct drm_hash_item item;
	struct drm_open_hash *parent;
};

struct dc_mm_buffer_mem_mapping {
	struct kref ref;
	struct drm_mm_node *node;
	struct drm_hash_item item;
	struct drm_open_hash *parent;
};

extern int dc_mm_buffer_import(struct dc_mm *mm, u32 handle,
	struct page **pages, size_t n_pages, u32 *addr);
extern void dc_mm_buffer_free(struct dc_mm *mm, u32 handle);
extern int dc_mm_init(struct dc_mm *mm, u32 start, size_t n_pages,
	page_table_insert_t insert_func);
extern void dc_mm_destroy(struct dc_mm *mm);

#endif /* DC_MM_H_ */

/*
 * Copyright (C) 2016 Google, Inc.
 *
 * Based on, but no longer compatible with, the original
 * OpenBinder.org binder driver interface, which is:
 *
 * Copyright (c) 2005 Palmsource, Inc.
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

#ifndef _LINUX_BINDER_ALLOC_H
#define _LINUX_BINDER_ALLOC_H

#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/rtmutex.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

struct binder_transaction;

struct binder_buffer {
	struct list_head entry; /* free and allocated entries by address */
	struct rb_node rb_node; /* free entry by size or allocated entry */
				/* by address */
	unsigned free:1;
	unsigned allow_user_free:1;
	unsigned async_transaction:1;
	unsigned debug_id:29;

	struct binder_transaction *transaction;

	struct binder_node *target_node;
	size_t data_size;
	size_t offsets_size;
	size_t extra_buffers_size;
	uint8_t data[0];
};

struct binder_alloc {
	struct mutex mutex;
	struct task_struct *tsk;
	struct vm_area_struct *vma;
	struct mm_struct *vma_vm_mm;
	void *buffer;
	ptrdiff_t user_buffer_offset;
	struct list_head buffers;
	struct rb_root free_buffers;
	struct rb_root allocated_buffers;
	size_t free_async_space;
	struct page **pages;
	size_t buffer_size;
	uint32_t buffer_free;
	int pid;
};

extern struct binder_buffer *binder_alloc_new_buf(struct binder_alloc *alloc,
						  size_t data_size,
						  size_t offsets_size,
						  size_t extra_buffers_size,
						  int is_async);
extern void binder_alloc_init(struct binder_alloc *alloc);
extern void binder_alloc_vma_close(struct binder_alloc *alloc);
extern struct binder_buffer *
binder_alloc_buffer_lookup(struct binder_alloc *alloc,
			   uintptr_t user_ptr);
extern void binder_alloc_free_buf(struct binder_alloc *alloc,
				  struct binder_buffer *buffer);
extern int binder_alloc_mmap_handler(struct binder_alloc *alloc,
				     struct vm_area_struct *vma);
extern void binder_alloc_deferred_release(struct binder_alloc *alloc);
extern int binder_alloc_get_allocated_count(struct binder_alloc *alloc);
extern void binder_alloc_print_allocated(struct seq_file *m,
					 struct binder_alloc *alloc);

static inline size_t
binder_alloc_get_free_async_space(struct binder_alloc *alloc)
{
	size_t free_async_space;

	mutex_lock(&alloc->mutex);
	free_async_space = alloc->free_async_space;
	mutex_unlock(&alloc->mutex);
	return free_async_space;
}

static inline ptrdiff_t
binder_alloc_get_user_buffer_offset(struct binder_alloc *alloc)
{
	/*
	 * user_buffer_offset is constant if vma is set and
	 * undefined if vma is not set
	 */
	BUG_ON(!alloc->vma);
	return READ_ONCE(alloc->user_buffer_offset);
}

#endif /* _LINUX_BINDER_ALLOC_H */


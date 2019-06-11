/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/stackdepot.h>
#include <linux/stacktrace.h>
#include <linux/seq_file.h>

#define DMA_BUF_STACK_DEPTH (32)

struct dma_buf_ref {
	struct list_head list;
	depot_stack_handle_t handle;
	int count;
};

void dma_buf_ref_init(struct dma_buf *dmabuf)
{
	INIT_LIST_HEAD(&dmabuf->refs);
}

void dma_buf_ref_destroy(struct dma_buf *dmabuf)
{
	struct dma_buf_ref *r, *n;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry_safe(r, n, &dmabuf->refs, list) {
		list_del(&r->list);
		kfree(r);
	}
	mutex_unlock(&dmabuf->lock);
}

static void dma_buf_ref_insert_handle(struct dma_buf *dmabuf,
				      depot_stack_handle_t handle,
				      int count)
{
	struct dma_buf_ref *r;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry(r, &dmabuf->refs, list) {
		if (r->handle == handle) {
			r->count += count;
			goto out;
		}
	}

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		goto out;

	INIT_LIST_HEAD(&r->list);
	r->handle = handle;
	r->count = count;
	list_add(&r->list, &dmabuf->refs);

out:
	mutex_unlock(&dmabuf->lock);
}

void dma_buf_ref_mod(struct dma_buf *dmabuf, int nr)
{
	unsigned long entries[DMA_BUF_STACK_DEPTH];
	struct stack_trace trace = {
		.nr_entries = 0,
		.entries = entries,
#ifdef CONFIG_USER_STACKTRACE_SUPPORT
		.max_entries = DMA_BUF_STACK_DEPTH/2,
#else
		.max_entries = DMA_BUF_STACK_DEPTH,
#endif
		.skip = 1
	};
	depot_stack_handle_t handle;

	save_stack_trace(&trace);
#ifdef CONFIG_USER_STACKTRACE_SUPPORT
	trace.max_entries = DMA_BUF_STACK_DEPTH;
	save_stack_trace_user(&trace);
#endif
	if (trace.nr_entries != 0 &&
	    trace.entries[trace.nr_entries-1] == ULONG_MAX)
		trace.nr_entries--;

	handle = depot_save_stack(&trace, GFP_KERNEL, current->pid);
	if (!handle)
		return;

	dma_buf_ref_insert_handle(dmabuf, handle, nr);
}

/**
 * Called with dmabuf->lock held
 */
int dma_buf_ref_show(struct seq_file *s, struct dma_buf *dmabuf)
{
	char *buf;
	struct dma_buf_ref *ref;
	int count = 0;
	struct stack_trace trace;

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	list_for_each_entry(ref, &dmabuf->refs, list) {
		count += ref->count;

		seq_printf(s, "References: %d\n", ref->count);
		depot_fetch_stack(ref->handle, &trace);
		snprint_stack_trace(buf, PAGE_SIZE, &trace, 0);
		seq_puts(s, buf);
		seq_putc(s, '\n');
	}

	seq_printf(s, "Total references: %d\n\n\n", count);
	free_page((unsigned long)buf);

	return 0;
}

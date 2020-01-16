// SPDX-License-Identifier: GPL-2.0-only

#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/stackdepot.h>
#include <linux/stacktrace.h>
#include <linux/seq_file.h>

#define DMA_BUF_STACK_DEPTH (16)

struct dma_buf_ref {
	struct list_head list;
	depot_stack_handle_t handle;
	int count;
};

void dma_buf_ref_init(struct msm_dma_buf *msm_dma_buf)
{
	INIT_LIST_HEAD(&msm_dma_buf->refs);
}

void dma_buf_ref_destroy(struct msm_dma_buf *msm_dma_buf)
{
	struct dma_buf_ref *r, *n;
	struct dma_buf *dmabuf = &msm_dma_buf->dma_buf;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry_safe(r, n, &msm_dma_buf->refs, list) {
		list_del(&r->list);
		kfree(r);
	}
	mutex_unlock(&dmabuf->lock);
}

static void dma_buf_ref_insert_handle(struct msm_dma_buf *msm_dma_buf,
				      depot_stack_handle_t handle,
				      int count)
{
	struct dma_buf_ref *r;
	struct dma_buf *dmabuf = &msm_dma_buf->dma_buf;

	mutex_lock(&dmabuf->lock);
	list_for_each_entry(r, &msm_dma_buf->refs, list) {
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
	list_add(&r->list, &msm_dma_buf->refs);

out:
	mutex_unlock(&dmabuf->lock);
}

void dma_buf_ref_mod(struct msm_dma_buf *msm_dma_buf, int nr)
{
	unsigned long entries[DMA_BUF_STACK_DEPTH];
	int nr_entries;
	depot_stack_handle_t handle;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 1);

	if (nr_entries != 0 && entries[nr_entries - 1] == ULONG_MAX)
		nr_entries--;

	handle = stack_depot_save(entries, nr_entries, GFP_KERNEL);
	if (!handle)
		return;

	dma_buf_ref_insert_handle(msm_dma_buf, handle, nr);
}
EXPORT_SYMBOL(dma_buf_ref_mod);

/**
 * Called with dmabuf->lock held
 */
int dma_buf_ref_show(struct seq_file *s, struct msm_dma_buf *msm_dma_buf)
{
	char *buf;
	struct dma_buf_ref *ref;
	int count = 0;

	buf = (void *)__get_free_page(GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	list_for_each_entry(ref, &msm_dma_buf->refs, list) {
		unsigned long *entries;
		int nr_entries;

		count += ref->count;

		seq_printf(s, "References: %d\n", ref->count);
		nr_entries = stack_depot_fetch(ref->handle, &entries);

		stack_trace_snprint(buf, PAGE_SIZE, entries, nr_entries, 0);
		seq_puts(s, buf);
		seq_putc(s, '\n');
	}

	seq_printf(s, "Total references: %d\n\n\n", count);
	free_page((unsigned long)buf);

	return 0;
}

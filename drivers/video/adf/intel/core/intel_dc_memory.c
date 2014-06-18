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
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/rwlock.h>
#include <linux/slab.h>

#include "core/intel_dc_config.h"

struct intel_dc_buffer *intel_dc_memory_import(struct intel_dc_memory *mem,
	u32 handle, struct page **pages, size_t n_pages)
{
	struct intel_dc_buffer *buf;
	u32 addr = 0;
	size_t i;
	int err;

	if (!mem || !mem->ops || !pages || !n_pages)
		return ERR_PTR(-EINVAL);

	if (!mem->ops->import || !mem->ops->free)
		return ERR_PTR(-EOPNOTSUPP);

	err = mem->ops->import(mem, handle, pages, n_pages, &addr);
	if (err) {
		dev_err(mem->dev, "%s: failed to import pages\n", __func__);
		goto out_err0;
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		dev_err(mem->dev, "%s: failed to allocate buffer\n", __func__);
		err = -ENOMEM;
		goto out_err1;
	}

	buf->handle = handle;
	buf->dc_mem_addr = addr;
	buf->pages = pages;
	buf->n_pages = n_pages;
	INIT_LIST_HEAD(&buf->list);

	for (i = 0; i < n_pages; i++)
		get_page(pages[i]);

	write_lock(&mem->lock);

	mem->alloc_pages += n_pages;
	mem->free_pages -= n_pages;
	list_add_tail(&buf->list, &mem->buf_list);
	mem->n_bufs++;

	write_unlock(&mem->lock);

	return buf;
out_err1:
	mem->ops->free(mem, handle);
out_err0:
	return ERR_PTR(err);
}

void intel_dc_memory_free(struct intel_dc_memory *mem,
	struct intel_dc_buffer *buf)
{
	size_t i;

	if (!mem || !buf)
		return;

	if (!mem->ops || !mem->ops->free)
		return;

	mem->ops->free(mem, buf->handle);

	for (i = 0; i < buf->n_pages; i++)
		put_page(buf->pages[i]);

	kfree(buf->pages);

	write_lock(&mem->lock);

	mem->alloc_pages -= buf->n_pages;
	mem->free_pages += buf->n_pages;
	list_del(&buf->list);
	mem->n_bufs--;

	write_unlock(&mem->lock);

	kfree(buf);
}

int intel_dc_memory_status(struct intel_dc_memory *mem, size_t *n_total,
	size_t *n_alloc, size_t *n_free, size_t *n_bufs)
{
	if (!mem || !n_total || !n_alloc || !n_free || !n_bufs)
		return -EINVAL;

	read_lock(&mem->lock);

	*n_total = mem->total_pages;
	*n_alloc = mem->alloc_pages;
	*n_free = mem->free_pages;
	*n_bufs = mem->n_bufs;

	read_unlock(&mem->lock);
	return 0;
}

int intel_dc_memory_init(struct intel_dc_memory *mem, struct device *dev,
	size_t total_pages, const struct intel_dc_memory_ops *ops)
{
	if (!mem || !dev || !total_pages)
		return -EINVAL;

	memset(mem, 0, sizeof(*mem));

	mem->dev = dev;
	mem->total_pages = total_pages;
	mem->free_pages = total_pages;
	mem->ops = ops;
	INIT_LIST_HEAD(&mem->buf_list);
	rwlock_init(&mem->lock);

	return 0;
}

void intel_dc_memory_destroy(struct intel_dc_memory *mem)
{
	struct intel_dc_buffer *buf, *tmp;

	if (!mem || !mem->ops)
		return;

	write_lock(&mem->lock);

	if (!list_empty(&mem->buf_list)) {
		list_for_each_entry_safe(buf, tmp, &mem->buf_list, list) {
			if (mem->ops->free)
				mem->ops->free(mem, buf->handle);
			/*free buffer*/
			kfree(buf);
			list_del(&buf->list);
		}
		mem->total_pages = 0;
		mem->alloc_pages = 0;
		mem->free_pages = 0;
		mem->n_bufs = 0;
		INIT_LIST_HEAD(&mem->buf_list);
	}

	write_unlock(&mem->lock);
}




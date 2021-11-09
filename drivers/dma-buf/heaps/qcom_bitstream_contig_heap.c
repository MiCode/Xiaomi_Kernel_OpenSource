// SPDX-License-Identifier: GPL-2.0-only
/*
 * DMABUF Contiguous Bitstream Allocator
 *
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/moduleparam.h>

#include <linux/mem-buf.h>
#include <soc/qcom/secure_buffer.h>
#include "qcom_bitstream_contig_heap.h"

static bool enable_bitstream_contig_heap;
static struct dma_heap *display_heap;

static struct dma_buf *bitstream_contig_heap_allocate(struct dma_heap *heap,
				  unsigned long len,
				  unsigned long fd_flags,
				  unsigned long heap_flags)
{
	struct dma_buf *dmabuf;
	int ret;
	struct mem_buf_lend_kernel_arg karg = {0};
	int vmid = VMID_CP_BITSTREAM;
	int perms = PERM_READ | PERM_WRITE;

	dmabuf = dma_heap_buffer_alloc(display_heap, len, fd_flags, heap_flags);
	if (IS_ERR(dmabuf)) {
		pr_err("%s: Failed to allocate from the display heap: %d\n",
		       __func__, PTR_ERR(dmabuf));
		return dmabuf;
	}

	karg.nr_acl_entries = 1;
	karg.vmids = &vmid;
	karg.perms = &perms;

	ret = mem_buf_lend(dmabuf, &karg);
	if (ret) {
		pr_err("%s: Failed to lend dmabuf: %d\n", __func__, ret);
		goto free_dmabuf;
	}

	return dmabuf;

free_dmabuf:
	dma_buf_put(dmabuf);

	return ERR_PTR(ret);
}

static const struct dma_heap_ops bitstream_contig_heap_ops = {
	.allocate = bitstream_contig_heap_allocate,
};

int qcom_add_bitstream_contig_heap(char *name)
{
	struct dma_heap_export_info exp_info;
	struct dma_heap *heap;

	if (!enable_bitstream_contig_heap)
		return 0;

	display_heap = dma_heap_find("qcom,display");
	if (!display_heap) {
		pr_err("%s: qcom,display heap doesn't exist, can't create %s heap\n",
		       __func__, name);
		return -EINVAL;
	}

	exp_info.name = name;
	exp_info.ops = &bitstream_contig_heap_ops;

	heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap)) {
		pr_err("%s: Failed to add system-secure heap: %d\n",
		       __func__, PTR_ERR(heap));
		return PTR_ERR(heap);
	}

	return 0;
}

module_param(enable_bitstream_contig_heap, bool, 0);

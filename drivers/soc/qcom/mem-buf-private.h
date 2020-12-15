/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef MEM_BUF_PRIVATE_H
#define MEM_BUF_PRIVATE_H

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/haven/hh_rm_drv.h>
#include <linux/slab.h>

/**
 * strcut mem_buf_import: Represents a memory buffer that was imported from
 * another VM.
 * @memparcel_hdl: The handle associated with the memparcel that represents the
 * memory that was imported from another VM.
 * @size: The size of the buffer.
 * @sgl_desc: The SG descriptor that represents the memory buffer.
 * @dmabuf: The dma-buf that corresponds to the buffer.
 * @kmap_cnt: The number of kernel mapping references associated with the buffer
 * @vaddr: The virtual address for the buffer after it has been mapped into a
 * contiguous range in the kernel virtual address space.
 * @lock: protects accesses to attachments.
 * @attachments: a list of attachments for the buffer.
 */
struct mem_buf_import {
	hh_memparcel_handle_t memparcel_hdl;
	size_t size;
	struct hh_sgl_desc *sgl_desc;
	struct dma_buf *dmabuf;
	int kmap_cnt;
	void *vaddr;
	struct mutex lock;
	struct list_head attachments;
};

void mem_buf_unimport_dma_buf(struct mem_buf_import *import_buf);

extern const struct dma_buf_ops mem_buf_dma_buf_ops;
#endif


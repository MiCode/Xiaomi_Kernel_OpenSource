/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef MEM_BUF_PRIVATE_H
#define MEM_BUF_PRIVATE_H

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/haven/hh_rm_drv.h>
#include <linux/mem-buf.h>
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

/*
 * @vmid - id assigned by hypervisor to uniquely identify a VM
 * @hh_id - id used to request the real vmid from the kernel
 * haven driver. This is a legacy field which should eventually be
 * removed once a better design is present.
 * @peripheral - certain operations, such as retrieveing a hh_handle,
 * are not supported for peripheral vms (yet).
 */
struct mem_buf_vm {
	const char *name;
	u16 vmid;
	enum hh_vm_names hh_id;
	bool peripheral;
	struct cdev cdev;
	struct device dev;
};

extern int current_vmid;
int mem_buf_vm_init(struct device *dev);
void mem_buf_vm_exit(void);
/*
 * Handles are only supported for CPU VMs & for physically contiguous memory
 * regions.
 * Returns a negative number for invalid arguments, 0 for handle not supported
 * and a positive number for handle supported.
 */
int mem_buf_vm_supports_handle(struct sg_table *sgt, int *vmids,
		unsigned int nr_acl_entries);
/* @Return: A negative number on failure, or vmid on success */
int mem_buf_fd_to_vmid(int fd);
#endif


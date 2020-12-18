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
#include <linux/dma-heap.h>

#define MEM_BUF_CAP_SUPPLIER	BIT(0)
#define MEM_BUF_CAP_CONSUMER	BIT(1)
#define MEM_BUF_CAP_DUAL (MEM_BUF_CAP_SUPPLIER | MEM_BUF_CAP_CONSUMER)
extern unsigned char mem_buf_capability;
extern struct device *mem_buf_dev;

int mem_buf_assign_mem(struct sg_table *sgt, int *dst_vmids,
			      int *dst_perms, unsigned int nr_acl_entries);
int mem_buf_unassign_mem(struct sg_table *sgt, int *src_vmids,
				unsigned int nr_acl_entries);
int mem_buf_retrieve_memparcel_hdl(struct sg_table *sgt,
					  int *dst_vmids, int *dst_perms,
					  u32 nr_acl_entries,
					  hh_memparcel_handle_t *memparcel_hdl);
struct hh_sgl_desc *mem_buf_map_mem_s2(hh_memparcel_handle_t memparcel_hdl,
					struct hh_acl_desc *acl_desc);
int mem_buf_map_mem_s1(struct hh_sgl_desc *sgl_desc);

int mem_buf_unmap_mem_s2(hh_memparcel_handle_t memparcel_hdl);
int mem_buf_unmap_mem_s1(struct hh_sgl_desc *sgl_desc);
size_t mem_buf_get_sgl_buf_size(struct hh_sgl_desc *sgl_desc);
struct sg_table *dup_hh_sgl_desc_to_sgt(struct hh_sgl_desc *sgl_desc);
struct hh_sgl_desc *dup_sgt_to_hh_sgl_desc(struct sg_table *sgt);
struct hh_acl_desc *mem_buf_vmid_perm_list_to_hh_acl(int *vmids, int *perms,
		unsigned int nr_acl_entries);

/*
 * Deltas from original qcom_sg_buffer:
 * Removed heap & secure fields
 * Added vmperm
 * Changed sg_tablee to pointer.
 */
struct qcom_sg_buffer {
	struct list_head attachments;
	struct mutex lock;
	unsigned long len;
	struct sg_table *sg_table;
	int vmap_cnt;
	void *vaddr;
	void (*free)(struct qcom_sg_buffer *buffer);
	struct mem_buf_vmperm *vmperm;
};

struct dma_heap_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped;
};

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


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

/* Hypervisor Interface */
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
int mem_buf_unmap_mem_s2(hh_memparcel_handle_t memparcel_hdl);

/* Memory Hotplug */
int mem_buf_map_mem_s1(struct hh_sgl_desc *sgl_desc);
int mem_buf_unmap_mem_s1(struct hh_sgl_desc *sgl_desc);

#define MEM_BUF_API_HYP_ASSIGN BIT(0)
#define MEM_BUF_API_HAVEN BIT(1)

/*
 * @vmid - id assigned by hypervisor to uniquely identify a VM
 * @hh_id - id used to request the real vmid from the kernel
 * haven driver. This is a legacy field which should eventually be
 * removed once a better design is present.
 * @allowed_api - Some vms may use a different hypervisor interface.
 */
struct mem_buf_vm {
	const char *name;
	u16 vmid;
	enum hh_vm_names hh_id;
	u32 allowed_api;
	struct cdev cdev;
	struct device dev;
};

extern int current_vmid;
int mem_buf_vm_init(struct device *dev);
void mem_buf_vm_exit(void);
/*
 * Returns a negative number for invalid arguments, otherwise a MEM_BUF_API
 * which is supported by all vmids in the array.
 */
int mem_buf_vm_get_backend_api(int *vmids, unsigned int nr_acl_entries);
/* @Return: A negative number on failure, or vmid on success */
int mem_buf_fd_to_vmid(int fd);

/* Functions from mem_buf_dma_buf.c */
int mem_buf_lend_internal(struct dma_buf *dmabuf,
			struct mem_buf_lend_kernel_arg *arg,
			bool is_lend);
#endif


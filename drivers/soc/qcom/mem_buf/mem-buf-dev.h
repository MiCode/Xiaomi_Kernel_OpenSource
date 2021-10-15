/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef MEM_BUF_PRIVATE_H
#define MEM_BUF_PRIVATE_H

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/mem-buf.h>
#include <linux/slab.h>

#define MEM_BUF_MEMPARCEL_INVALID (U32_MAX)

#define MEM_BUF_CAP_SUPPLIER	BIT(0)
#define MEM_BUF_CAP_CONSUMER	BIT(1)
#define MEM_BUF_CAP_DUAL (MEM_BUF_CAP_SUPPLIER | MEM_BUF_CAP_CONSUMER)
extern unsigned char mem_buf_capability;
extern struct device *mem_buf_dev;

struct gh_acl_desc *mem_buf_vmid_perm_list_to_gh_acl(int *vmids, int *perms,
		unsigned int nr_acl_entries);
struct gh_sgl_desc *mem_buf_sgt_to_gh_sgl_desc(struct sg_table *sgt);
int mem_buf_gh_acl_desc_to_vmid_perm_list(struct gh_acl_desc *acl_desc,
						 int **vmids, int **perms);
struct sg_table *dup_gh_sgl_desc_to_sgt(struct gh_sgl_desc *sgl_desc);

/* Hypervisor Interface */
int mem_buf_assign_mem(int op, struct sg_table *sgt,
		       struct mem_buf_lend_kernel_arg *arg);
int mem_buf_unassign_mem(struct sg_table *sgt, int *src_vmids,
			 unsigned int nr_acl_entries,
			 gh_memparcel_handle_t hdl);
struct gh_sgl_desc *mem_buf_map_mem_s2(int op, gh_memparcel_handle_t *memparcel_hdl,
					struct gh_acl_desc *acl_desc, int src_vmid);
int mem_buf_unmap_mem_s2(gh_memparcel_handle_t memparcel_hdl);

/* Memory Hotplug */
int mem_buf_map_mem_s1(struct gh_sgl_desc *sgl_desc);
int mem_buf_unmap_mem_s1(struct gh_sgl_desc *sgl_desc);

#endif

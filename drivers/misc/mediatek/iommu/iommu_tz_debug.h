/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef M4U_DEBUG_H
#define M4U_DEBUG_H

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/proc_fs.h>

#define DEFINE_PROC_ATTRIBUTE(__fops, __get, __set, __fmt)		  \
static int __fops ## _open(struct inode *inode, struct file *file)	  \
{									  \
	struct inode local_inode = *inode;				  \
									  \
	local_inode.i_private = PDE_DATA(inode);			  \
	__simple_attr_check_format(__fmt, 0ull);			  \
	return simple_attr_open(&local_inode, file, __get, __set, __fmt); \
}									  \
static const struct proc_ops __fops = {				  \
	.proc_open	 = __fops ## _open,				  \
	.proc_release = simple_attr_release,				  \
	.proc_read	 = simple_attr_read,				  \
	.proc_write	 = simple_attr_write,				  \
	.proc_lseek	 = generic_file_llseek,				  \
}

typedef int (*mtk_iommu_fault_callback_t)(int port,
				dma_addr_t mva, void *cb_data);

bool report_custom_iommu_fault(
	u32 fault_iova,
	u32 fault_pa,
	u32 fault_id, bool is_vpu);

int mtk_iommu_register_fault_callback(int port,
			       mtk_iommu_fault_callback_t fn,
			       void *cb_data);

/* port: comes from "include/dt-binding/memort/mtxxx-larb-port.h" */
int mtk_iommu_unregister_fault_callback(int port);

void mtk_iova_dbg_alloc(struct device *dev, dma_addr_t iova, size_t size);

void mtk_iova_dbg_free(dma_addr_t iova, size_t size);

void mtk_iova_dbg_dump(struct seq_file *s);

#endif

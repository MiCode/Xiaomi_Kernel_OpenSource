/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Qti (or) Qualcomm Technologies Inc CE device driver.
 *
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DRIVERS_CRYPTO_PARSE_H_
#define _DRIVERS_CRYPTO_PARSE_H_

#include <linux/dma-iommu.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/iommu.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <linux/msm_ion.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/types.h>

struct context_bank_info {
	struct list_head list;
	const char *name;
	u32 buffer_type;
	u32 start_addr;
	u32 size;
	bool is_secure;
	struct device *dev;
	struct dma_iommu_mapping *mapping;
};

enum qcedev_mem_type {
	MEM_ION,
};

struct qcedev_mem_client {
	enum qcedev_mem_type mtype;
};

struct dma_mapping_info {
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct dma_buf *buf;
};

struct qcedev_ion_buf_info {
	struct dma_mapping_info mapping_info;
	dma_addr_t iova;
	unsigned long mapped_buf_size;
	int ion_fd;
};

struct qcedev_reg_buf_info {
	struct list_head list;
	union {
		struct qcedev_ion_buf_info ion_buf;
	};
	atomic_t ref_count;
};

struct qcedev_buffer_list {
	struct list_head list;
	struct mutex lock;
};

int qcedev_parse_context_bank(struct platform_device *pdev);
struct qcedev_mem_client *qcedev_mem_new_client(enum qcedev_mem_type mtype);
void qcedev_mem_delete_client(struct qcedev_mem_client *mem_client);
int qcedev_check_and_map_buffer(void *qce_hndl,
		int fd, unsigned int offset, unsigned int fd_size,
		unsigned long long *vaddr);
int qcedev_check_and_unmap_buffer(void *handle, int fd);
int qcedev_unmap_all_buffers(void *handle);

extern struct qcedev_reg_buf_info *global_binfo_in;
extern struct qcedev_reg_buf_info *global_binfo_out;
extern struct qcedev_reg_buf_info *global_binfo_res;
#endif


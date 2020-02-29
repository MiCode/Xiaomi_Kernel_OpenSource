/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_CORE_H_
#define _MSM_CVP_CORE_H_

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include <linux/refcount.h>
#include <media/msm_cvp_private.h>
#include <media/msm_cvp_vidc.h>

enum smem_prop {
	SMEM_UNCACHED = 0x1,
	SMEM_CACHED = 0x2,
	SMEM_SECURE = 0x4,
	SMEM_ADSP = 0x8,
	SMEM_NON_PIXEL = 0x10
};

struct cvp_dma_mapping_info {
	struct device *dev;
	struct iommu_domain *domain;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct dma_buf *buf;
	void *cb_info;
};

struct msm_cvp_smem {
	struct list_head list;
	atomic_t refcount;
	struct dma_buf *dma_buf;
	void *kvaddr;
	u32 device_addr;
	dma_addr_t dma_handle;
	u32 size;
	u32 flags;
	u32 ion_flags;
	struct cvp_dma_mapping_info mapping_info;
};

enum smem_cache_ops {
	SMEM_CACHE_CLEAN,
	SMEM_CACHE_INVALIDATE,
	SMEM_CACHE_CLEAN_INVALIDATE,
};

enum core_id {
	MSM_CORE_CVP = 0,
	MSM_CVP_CORES_MAX,
};

enum session_type {
	MSM_CVP_USER = 1,
	MSM_CVP_KERNEL,
	MSM_CVP_BOOT,
	MSM_CVP_UNKNOWN,
	MSM_CVP_MAX_DEVICES = MSM_CVP_UNKNOWN,
};

void *msm_cvp_open(int core_id, int session_type);
int msm_cvp_close(void *instance);
int msm_cvp_suspend(int core_id);
int msm_cvp_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
int msm_cvp_private(void *cvp_inst, unsigned int cmd,
		struct cvp_kmd_arg *arg);
int msm_cvp_est_cycles(struct cvp_kmd_usecase_desc *cvp_desc,
		struct cvp_kmd_request_power *cvp_voting);

#endif

/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_CORE_H_
#define _MSM_CVP_CORE_H_

#include <linux/poll.h>
#include <linux/videodev2.h>
#include <linux/types.h>
#include <linux/msm_ion.h>
#include <media/msm_cvp_private.h>
#include <media/msm_cvp_utils.h>

#define HAL_BUFFER_MAX 0xe

enum smem_type {
	SMEM_DMA = 1,
};

enum smem_prop {
	SMEM_UNCACHED = 0x1,
	SMEM_CACHED = 0x2,
	SMEM_SECURE = 0x4,
	SMEM_ADSP = 0x8,
};

/* NOTE: if you change this enum you MUST update the
 * "buffer-type-tz-usage-table" for any affected target
 * in arch/arm/boot/dts/<arch>.dtsi
 */
enum hal_buffer {
	HAL_BUFFER_NONE = 0x0,
	HAL_BUFFER_INPUT = 0x1,
	HAL_BUFFER_OUTPUT = 0x2,
	HAL_BUFFER_OUTPUT2 = 0x4,
	HAL_BUFFER_EXTRADATA_INPUT = 0x8,
	HAL_BUFFER_EXTRADATA_OUTPUT = 0x10,
	HAL_BUFFER_EXTRADATA_OUTPUT2 = 0x20,
	HAL_BUFFER_INTERNAL_SCRATCH = 0x40,
	HAL_BUFFER_INTERNAL_SCRATCH_1 = 0x80,
	HAL_BUFFER_INTERNAL_SCRATCH_2 = 0x100,
	HAL_BUFFER_INTERNAL_PERSIST = 0x200,
	HAL_BUFFER_INTERNAL_PERSIST_1 = 0x400,
	HAL_BUFFER_INTERNAL_CMD_QUEUE = 0x800,
	HAL_BUFFER_INTERNAL_RECON = 0x1000,
};

struct dma_mapping_info {
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	struct dma_buf *buf;
	void *cb_info;
};

struct msm_smem {
	u32 refcount;
	int fd;
	void *dma_buf;
	void *kvaddr;
	u32 device_addr;
	dma_addr_t dma_handle;
	unsigned int offset;
	unsigned int size;
	unsigned long flags;
	enum hal_buffer buffer_type;
	struct dma_mapping_info mapping_info;
};

enum smem_cache_ops {
	SMEM_CACHE_CLEAN,
	SMEM_CACHE_INVALIDATE,
	SMEM_CACHE_CLEAN_INVALIDATE,
};

enum core_id {
	MSM_CORE_CVP = 0,
	MSM_CVP_CORE_Q6,
	MSM_CVP_CORES_MAX,
};
enum session_type {
	MSM_CVP_ENCODER = 0,
	MSM_CVP_DECODER,
	MSM_CVP_CORE,
	MSM_CVP_UNKNOWN,
	MSM_CVP_MAX_DEVICES = MSM_CVP_UNKNOWN,
};

union msm_v4l2_cmd {
	struct v4l2_decoder_cmd dec;
	struct v4l2_encoder_cmd enc;
};

void *msm_cvp_open(int core_id, int session_type);
int msm_cvp_close(void *instance);
int msm_cvp_suspend(int core_id);
int msm_cvp_g_fmt(void *instance, struct v4l2_format *f);
int msm_cvp_reqbufs(void *instance, struct v4l2_requestbuffers *b);
int msm_cvp_release_buffer(void *instance, int buffer_type,
		unsigned int buffer_index);
int msm_cvp_comm_cmd(void *instance, union msm_v4l2_cmd *cmd);
int msm_cvp_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
int msm_cvp_subscribe_event(void *instance,
		const struct v4l2_event_subscription *sub);
int msm_cvp_unsubscribe_event(void *instance,
		const struct v4l2_event_subscription *sub);
int msm_cvp_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize);
int msm_cvp_private(void *cvp_inst, unsigned int cmd,
		struct cvp_kmd_arg *arg);
#endif

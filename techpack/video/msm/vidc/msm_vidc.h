/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_VIDC_H_
#define _MSM_VIDC_H_

#include <linux/videodev2.h>
#include <linux/msm_ion.h>
#include "vidc/media/msm_vidc_utils.h"
#include <media/media-device.h>

#define HAL_BUFFER_MAX 0xe
#define CVP_FRAME_RATE_MAX (60)
#define MEMORY_REGIONS_MAX 30

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

enum msm_vidc_blur_type {
	MSM_VIDC_BLUR_INTERNAL = 0x0,
	MSM_VIDC_BLUR_EXTERNAL_DYNAMIC = 0x1,
	MSM_VIDC_BLUR_DISABLE = 0x2,
};

struct dma_mapping_info {
	struct device *dev;
	struct iommu_domain *domain;
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

struct memory_regions {
	u32 num_regions;
	struct {
		u64 size;
		u32 vmid;
	} region[MEMORY_REGIONS_MAX];
};

enum memory_ops {
	MEMORY_PREFETCH = 1,
	MEMORY_DRAIN,
};

enum core_id {
	MSM_VIDC_CORE_VENUS = 0,
	MSM_VIDC_CORE_Q6,
	MSM_VIDC_CORES_MAX,
};
enum session_type {
	MSM_VIDC_ENCODER = 0,
	MSM_VIDC_DECODER,
	MSM_VIDC_UNKNOWN,
	MSM_VIDC_MAX_DEVICES = MSM_VIDC_UNKNOWN,
};

enum load_type {
	MSM_VIDC_VIDEO = 0,
	MSM_VIDC_IMAGE,
};

union msm_v4l2_cmd {
	struct v4l2_decoder_cmd dec;
	struct v4l2_encoder_cmd enc;
};

void *msm_vidc_open(int core_id, int session_type);
int msm_vidc_close(void *instance);
int msm_vidc_suspend(int core_id);
int msm_vidc_querycap(void *instance, struct v4l2_capability *cap);
int msm_vidc_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_vidc_s_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_g_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_s_ctrl(void *instance, struct v4l2_control *a);
int msm_vidc_s_ext_ctrl(void *instance, struct v4l2_ext_controls *a);
int msm_vidc_g_ext_ctrl(void *instance, struct v4l2_ext_controls *a);
int msm_vidc_g_ctrl(void *instance, struct v4l2_control *a);
int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b);
int msm_vidc_release_buffer(void *instance, int buffer_type,
		unsigned int buffer_index);
int msm_vidc_qbuf(void *instance, struct media_device *mdev,
		struct v4l2_buffer *b);
int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b);
int msm_vidc_streamon(void *instance, enum v4l2_buf_type i);
int msm_vidc_query_ctrl(void *instance, struct v4l2_queryctrl *ctrl);
int msm_vidc_query_menu(void *instance, struct v4l2_querymenu *qmenu);
int msm_vidc_streamoff(void *instance, enum v4l2_buf_type i);
int msm_vidc_comm_cmd(void *instance, union msm_v4l2_cmd *cmd);
int msm_vidc_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
int msm_vidc_subscribe_event(void *instance,
		const struct v4l2_event_subscription *sub);
int msm_vidc_unsubscribe_event(void *instance,
		const struct v4l2_event_subscription *sub);
int msm_vidc_dqevent(void *instance, struct v4l2_event *event);
int msm_vidc_g_crop(void *instance, struct v4l2_crop *a);
int msm_vidc_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize);
#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis-YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef __MTK_MML_H__
#define __MTK_MML_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ktime.h>

#define MML_MAX_OUTPUTS		2
#define MML_MAX_PLANES		3

struct mml_job {
	uint32_t jobid;
	int32_t fence;
};

enum mml_mode {
	MML_MODE_NOT_SUPPORT = -1,
	MML_MODE_UNKNOWN = 0,
	MML_MODE_DIRECT_LINK,
	MML_MODE_RACING,
	MML_MODE_MML_DECOUPLE,
	MML_MODE_MDP_DECOUPLE,
};

/* Combine colorspace, xfer_func, ycbcr_encoding, and quantization */
enum mml_ycbcr_profile {
	/* V4L2_YCBCR_ENC_601 and V4L2_QUANTIZATION_LIM_RANGE */
	MML_YCBCR_PROFILE_BT601,
	/* V4L2_YCBCR_ENC_709 and V4L2_QUANTIZATION_LIM_RANGE */
	MML_YCBCR_PROFILE_BT709,
	/* V4L2_YCBCR_ENC_601 and V4L2_QUANTIZATION_FULL_RANGE */
	MML_YCBCR_PROFILE_JPEG,
	MML_YCBCR_PROFILE_FULL_BT601 = MML_YCBCR_PROFILE_JPEG,

	/* Colorspaces not support for destination */
	/* V4L2_YCBCR_ENC_BT2020 and V4L2_QUANTIZATION_LIM_RANGE */
	MML_YCBCR_PROFILE_BT2020,
	/* V4L2_YCBCR_ENC_709 and V4L2_QUANTIZATION_FULL_RANGE */
	MML_YCBCR_PROFILE_FULL_BT709,
	/* V4L2_YCBCR_ENC_BT2020 and V4L2_QUANTIZATION_FULL_RANGE */
	MML_YCBCR_PROFILE_FULL_BT2020,
};

enum mml_orientation {
	MML_ROT_0 = 0,
	MML_ROT_90,
	MML_ROT_180,
	MML_ROT_270
};

struct mml_pq_config {
	bool en;
};

struct mml_pq_param {
	int32_t ion_handle;
	uint32_t enhance_pos;
	uint32_t enhance_dir;
};

struct mml_frame_data {
	uint32_t width;
	uint32_t height;
	uint32_t y_stride;
	uint32_t uv_stride;
	uint32_t vert_stride;
	uint32_t format;
	uint16_t profile;
	uint32_t plane_offset[MML_MAX_PLANES];
	uint8_t plane_cnt;
	bool secure;
};

struct mml_rect {
	uint32_t left;
	uint32_t top;
	uint32_t width;
	uint32_t height;
};

struct mml_crop {
	struct mml_rect r;
	uint32_t x_sub_px;
	uint32_t y_sub_px;
	uint32_t w_sub_px;
	uint32_t h_sub_px;
};

struct mml_frame_dest {
	struct mml_frame_data data;
	struct mml_crop crop;
	struct mml_rect compose;
	uint16_t rotate;
	bool flip;
	struct mml_pq_config pq_config;
};

struct mml_frame_info {
	struct mml_frame_data src;
	struct mml_frame_dest dest[MML_MAX_OUTPUTS];
	uint8_t dest_cnt;	/* should be < MML_MAX_OUTPUTS */
	int8_t mode;	/* one of mml_mode */
	uint8_t layer_id;
};

struct mml_buffer {
	int32_t fd[MML_MAX_PLANES];
	uint32_t size[MML_MAX_PLANES];
	uint8_t cnt;
	int32_t fence;
	uint32_t usage;
};

struct mml_frame_buffer {
	struct mml_buffer src;
	struct mml_buffer dest[MML_MAX_OUTPUTS];
	uint8_t dest_cnt;
};

/**
 * struct mml_submit - submit mml task from user
 * @job:	[in/out] The mml task serial number and fence for user to wait.
 *		job->jobid must provide if update flag set to true and
 *		mml adaptor will try to match previous mml_frame_config to
 *		reuse commands.
 * @info:	Frame configs which not change between frame-to-frame.
 *		MML try to match same info in cache and reuse same commands.
 * @buffer:	Buffer fd and related parameters.
 * @sec:	End-Time for time value second
 * @usec:	End-Time for time value usecond
 * @pq_param:	PQ parameters pointer. Leave empty also disable PQ.
 * @update:	Flag to enable partial update.
 *		Turn on to force mml driver find same job->jobid in cache to
 *		reuse previous configs and commands.
 */
struct mml_submit {
	struct mml_job *job;
	struct mml_frame_info info;
	struct mml_frame_buffer buffer;
	struct timeval_t {
		uint64_t sec;
		uint64_t nsec;
	} end;
	struct mml_pq_param *pq_param[MML_MAX_OUTPUTS];
	bool update;
};


#endif	/* __MTK_MML_H__ */

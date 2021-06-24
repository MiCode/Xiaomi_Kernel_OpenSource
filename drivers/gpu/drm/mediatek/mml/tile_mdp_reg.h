/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __TILE_MDP_REG_H__
#define __TILE_MDP_REG_H__

#include "DpTileScaler.h"
#include "mtk-mml.h"

/* error enum */
#define MDP_TILE_ERROR_MESSAGE_ENUM(n, CMD) \
	/* Ring Buffer control */\
	CMD(n, MDP_MESSAGE_BUFFER_EMPTY, ISP_TPIPE_MESSAGE_FAIL)\
	/* RDMA check */\
	CMD(n, MDP_MESSAGE_RDMA_NULL_DATA, ISP_TPIPE_MESSAGE_FAIL)\
	/* HDR check */\
	CMD(n, MDP_MESSAGE_HDR_NULL_DATA, ISP_TPIPE_MESSAGE_FAIL)\
	/* AAL check */\
	CMD(n, MDP_MESSAGE_AAL_NULL_DATA, ISP_TPIPE_MESSAGE_FAIL)\
	/* PRZ check */\
	CMD(n, MDP_MESSAGE_PRZ_NULL_DATA, ISP_TPIPE_MESSAGE_FAIL)\
	CMD(n, MDP_MESSAGE_RESIZER_SCALING_ERROR, ISP_TPIPE_MESSAGE_FAIL)\
	/* TDSHP check */\
	CMD(n, MDP_MESSAGE_TDSHP_BACK_LT_FORWARD, ISP_TPIPE_MESSAGE_FAIL)\
	/* WROT check */\
	CMD(n, MDP_MESSAGE_WROT_NULL_DATA, ISP_TPIPE_MESSAGE_FAIL)\
	CMD(n, MDP_MESSAGE_WROT_INVALID_FORMAT, ISP_TPIPE_MESSAGE_FAIL)\
	/* WDMA check */\
	CMD(n, MDP_MESSAGE_WDMA_NULL_DATA, ISP_TPIPE_MESSAGE_FAIL)\
	/* General status */\
	CMD(n, MDP_MESSAGE_INVALID_STATE, ISP_TPIPE_MESSAGE_FAIL)\
	CMD(n, MDP_MESSAGE_UNKNOWN_ERROR, ISP_TPIPE_MESSAGE_FAIL)\

#define USE_DP_TILE_SCALER  1
#define RDMA_CROP_LEFT_TOP  0

struct rdma_tile_data {
	enum mml_color src_fmt;
	u32 blk_shift_w;
	u32 blk_shift_h;
	struct mml_rect crop;
	bool alpharot;
	u32 max_width;
};

struct hdr_tile_data {
	bool relay_mode;
	u32 min_width;
};

struct aal_tile_data{
	u32 min_width;
	u32 max_width;
	u32 min_hist_width;
};

struct rsz_tile_data {
	bool use_121filter;
	u32 coef_step_x;
	u32 coef_step_y;
	u32 precision_x;
	u32 precision_y;
	struct mml_crop crop;
	bool hor_scale;
	enum scaler_algo hor_algo;
	bool vir_scale;
	enum scaler_algo ver_algo;
	s32 c42_out_frame_w;
	s32 c24_in_frame_w;
	s32 out_tile_w;
	s32 back_xs;
	s32 back_xe;
	u32 ver_first;
	u32 ver_cubic_trunc;
	u32 max_width;
};

struct tdshp_tile_data {
	u32 max_width;
};

struct wrot_tile_data {
	enum mml_color dest_fmt;
	u32 rotate;
	bool flip;
	bool alpharot;
	bool enable_crop;
	s32 crop_left;
	s32 crop_width;
	u32 max_width;
};

struct mml_tile_data {
	struct rdma_tile_data rdma_data;
	struct hdr_tile_data hdr_data;
	struct aal_tile_data aal_data;
	struct rsz_tile_data rsz_data;
	struct tdshp_tile_data tdshp_data;
	struct wrot_tile_data wrot_data;
};

#endif

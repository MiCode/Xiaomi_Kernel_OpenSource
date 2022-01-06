/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_MML_TILE_H__
#define __MTK_MML_TILE_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include "mtk-mml.h"
#include "mtk-mml-core.h"
#include "DpTileScaler.h"

struct rdma_tile_data {
	enum mml_color src_fmt;
	u32 blk_shift_w;
	u32 blk_shift_h;
	struct mml_rect crop;
	u32 max_width;
};

struct hdr_tile_data {
	bool relay_mode;
	u32 min_width;
};

struct aal_tile_data {
	u32 min_width;
	u32 max_width;
	u32 min_hist_width;
};

struct rsz_tile_data {
	bool use_121filter;
	u32 coeff_step_x;
	u32 coeff_step_y;
	u32 precision_x;
	u32 precision_y;
	struct mml_crop crop;
	bool hor_scale;
	enum scaler_algo hor_algo;
	bool ver_scale;
	enum scaler_algo ver_algo;
	s32 c42_out_frame_w;
	s32 c24_in_frame_w;
	s32 prz_out_tile_w;
	s32 prz_back_xs;
	s32 prz_back_xe;
	bool ver_first;
	bool ver_cubic_trunc;
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
	bool racing;
	bool enable_x_crop;
	bool enable_y_crop;
	struct mml_rect crop;
	u32 max_width;
	u8 racing_h;
};

struct dlo_tile_data {
	bool enable_x_crop;
	struct mml_rect crop;
};

union mml_tile_data {
	struct rdma_tile_data rdma;
	struct hdr_tile_data hdr;
	struct aal_tile_data aal;
	struct rsz_tile_data rsz;
	struct tdshp_tile_data tdshp;
	struct wrot_tile_data wrot;
	struct dlo_tile_data dlo;
};

s32 calc_tile(struct mml_task *task, u32 pipe, struct mml_tile_cache *tile_cache);
void destroy_tile_output(struct mml_tile_output *output);
void dump_tile_output(struct mml_tile_output *output);

#endif	/* __MTK_MML_TILE_H__ */

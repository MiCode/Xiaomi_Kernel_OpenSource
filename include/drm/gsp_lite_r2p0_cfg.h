/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_LITE_R2P0_CFG_H
#define _GSP_LITE_R2P0_CFG_H

#include <linux/types.h>
#include <uapi/drm/gsp_cfg.h>
#include <uapi/drm/gsp_lite_r2p0_cfg.h>
#include <drm/gsp_cfg.h>

struct gsp_lite_r2p0_img_layer {
	struct gsp_layer		common;
	struct gsp_lite_r2p0_img_layer_params	params;
};

struct gsp_lite_r2p0_osd_layer {
	struct gsp_layer		common;
	struct gsp_lite_r2p0_osd_layer_params	params;
};

struct gsp_lite_r2p0_des_layer {
	struct gsp_layer		common;
	struct gsp_lite_r2p0_des_layer_params	params;
};

struct gsp_lite_r2p0_misc_cfg {
	uint8_t gsp_gap;
	uint8_t cmd_cnt;
	uint8_t run_mod;
	uint8_t scale_seq;
	uint8_t pmargb_en;
	struct gsp_rect		workarea1_src_rect;
	struct gsp_rect		workarea2_src_rect;
	struct gsp_pos		workarea2_des_pos;
	struct gsp_scale_para	scale_para;
};

struct gsp_lite_r2p0_cfg {
	struct gsp_cfg common;
	struct gsp_lite_r2p0_img_layer limg[LITE_R2P0_IMGL_NUM];
	struct gsp_lite_r2p0_osd_layer losd[LITE_R2P0_OSDL_NUM];
	struct gsp_lite_r2p0_des_layer ld1;
	struct gsp_lite_r2p0_misc_cfg misc;
};

#endif

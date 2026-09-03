/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_R6P0_CFG_H
#define _GSP_R6P0_CFG_H
/**---------------------------------------------------------------------------*
 **                             Indepence                                     *
 **---------------------------------------------------------------------------*
 */
#include <linux/types.h>
#include <uapi/drm/gsp_cfg.h>
#include <uapi/drm/gsp_r6p0_cfg.h>
#include <drm/gsp_cfg.h>

struct gsp_r6p0_img_layer {
	struct gsp_layer		common;
	struct gsp_r6p0_img_layer_params	params;
};

struct gsp_r6p0_osd_layer {
	struct gsp_layer		common;
	struct gsp_r6p0_osd_layer_params	params;
};

struct gsp_r6p0_des_layer {
	struct gsp_layer		common;
	struct gsp_r6p0_des_layer_params	params;
};

struct gsp_r6p0_misc_cfg {
	u8 gsp_gap;
	u8 core_num;
	u8 co_work0;
	u8 co_work1;
	u8 work_mod;
	u8 pmargb_en;
	struct gsp_rect workarea_src_rect;
	struct gsp_pos workarea_des_pos;
};

struct gsp_r6p0_cfg {
	struct gsp_cfg common;
	struct gsp_r6p0_img_layer limg[R6P0_IMGL_NUM];
	struct gsp_r6p0_osd_layer losd[R6P0_OSDL_NUM];
	struct gsp_r6p0_des_layer ld1;
	struct gsp_r6p0_misc_cfg misc;
};

#endif

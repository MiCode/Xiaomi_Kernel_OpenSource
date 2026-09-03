/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_R9P0_CFG_H
#define _GSP_R9P0_CFG_H
/**---------------------------------------------------------------------------*
 **                             Indepence                                     *
 **---------------------------------------------------------------------------*
 */
#include <linux/types.h>
#include <uapi/drm/gsp_cfg.h>
#include <uapi/drm/gsp_r9p0_cfg.h>
#include <drm/gsp_cfg.h>

struct gsp_r9p0_img_layer {
	struct gsp_layer			common;
	struct gsp_r9p0_img_layer_params	params;
};

struct gsp_r9p0_osd_layer {
	struct gsp_layer			common;
	struct gsp_r9p0_osd_layer_params	params;
};

struct gsp_r9p0_des_layer {
	struct gsp_layer			common;
	struct gsp_r9p0_des_layer_params	params;
};

struct gsp_r9p0_misc_cfg {
	u8 gsp_gap;
	u8 core_num;
	u8 co_work0;
	u8 co_work1;
	u8 work_mod;
	u8 pmargb_en;
	u8 secure_en;
	bool hdr_flag[R9P0_IMGL_NUM];
	bool first10bit_frame[R9P0_IMGL_NUM];
	bool hdr10plus_update[R9P0_IMGL_NUM];
	u32 work_freq;
	struct gsp_rect workarea_src_rect;
	struct gsp_pos workarea_des_pos;
	struct gsp_r9p0_hdr10_cfg hdr10_para[R9P0_IMGL_NUM];
};

struct gsp_r9p0_cfg {
	struct gsp_cfg common;
	struct gsp_r9p0_img_layer limg[R9P0_IMGL_NUM];
	struct gsp_r9p0_osd_layer losd[R9P0_OSDL_NUM];
	struct gsp_r9p0_des_layer ld1;
	struct gsp_r9p0_misc_cfg misc;
};

#endif

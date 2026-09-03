/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _UAPI_VIDEO_GSP_R6P0_CFG_H_
#define _UAPI_VIDEO_GSP_R6P0_CFG_H_

#include <linux/ioctl.h>
#include <linux/types.h>
#include "gsp_cfg.h"

#define R6P0_IMGL_NUM 2
#define R6P0_OSDL_NUM 2
#define R6P0_IMGSEC_NUM 0
#define R6P0_OSDSEC_NUM 1

/*Original: B3B2B1B0*/
enum gsp_r6p0_word_endian {
	GSP_R6P0_WORD_ENDN_0 = 0x00,	 /*B3B2B1B0*/
	GSP_R6P0_WORD_ENDN_1,			/*B0B1B2B3*/
	GSP_R6P0_WORD_ENDN_2,			/*B1B0B3B2*/
	GSP_R6P0_WORD_ENDN_3,			/*B2B3B0B1*/
	GSP_R6P0_WORD_ENDN_MAX_NUM,
};

enum gsp_r6p0_dword_endian {
	GSP_R6P0_DWORD_ENDN_0 = 0x00,	 /*B7B6B5B4_B3B2B1B0*/
	GSP_R6P0_DWORD_ENDN_1,			/*B3B2B1B0_B7B6B5B4*/
	GSP_R6P0_DWORD_ENDN_MAX_NUM,
};

enum gsp_r6p0_qword_endian {
	/*B15B14B13B12_B11B10B9B8_B7B6B5B4_B3B2B1B0*/
	GSP_R6P0_QWORD_ENDN_0 = 0x00,
	/*B7B6B5B4_B3B2B1B0_B15B14B13B12_B11B10B9B8*/
	GSP_R6P0_QWORD_ENDN_1,
	GSP_R6P0_QWORD_ENDN_MAX_NUM,
};

enum gsp_r6p0_rgb_swap_mod {
	GSP_R6P0_RGB_SWP_RGB = 0x00,
	GSP_R6P0_RGB_SWP_RBG,
	GSP_R6P0_RGB_SWP_GRB,
	GSP_R6P0_RGB_SWP_GBR,
	GSP_R6P0_RGB_SWP_BGR,
	GSP_R6P0_RGB_SWP_BRG,
	GSP_R6P0_RGB_SWP_MAX,
};

enum gsp_r6p0_a_swap_mod {
	GSP_R6P0_A_SWAP_ARGB,
	GSP_R6P0_A_SWAP_RGBA,
	GSP_R6P0_A_SWAP_MAX,
};

enum gsp_r6p0_img_layer_format {
	GSP_R6P0_IMG_FMT_ARGB888 = 0x00,
	GSP_R6P0_IMG_FMT_RGB888,
	GSP_R6P0_IMG_FMT_YUV422_2P,
	GSP_R6P0_IMG_FMT_RESERVED,
	GSP_R6P0_IMG_FMT_YUV420_2P,
	GSP_R6P0_IMG_FMT_YUV420_3P,
	GSP_R6P0_IMG_FMT_RGB565,
	GSP_R6P0_IMG_FMT_YV12,
	GSP_R6P0_IMG_FMT_MAX_NUM,
};

enum gsp_r6p0_osd_layer_format {
	GSP_R6P0_OSD_FMT_ARGB888 = 0x00,
	GSP_R6P0_OSD_FMT_RGB888,
	GSP_R6P0_OSD_FMT_RGB565,
	GSP_R6P0_OSD_FMT_MAX_NUM,
};

enum gsp_r6p0_des_layer_format {
	GSP_R6P0_DST_FMT_ARGB888 = 0x00,
	GSP_R6P0_DST_FMT_RGB888,
	GSP_R6P0_DST_FMT_RGB565,
	GSP_R6P0_DST_FMT_YUV420_2P,
	GSP_R6P0_DST_FMT_YUV420_3P,
	GSP_R6P0_DST_FMT_YUV422_2P,
	GSP_R6P0_DST_FMT_RGB666,
	GSP_R6P0_DST_FMT_MAX_NUM,
};

struct gsp_r6p0_endian {
	__u32 y_rgb_word_endn;
	__u32 y_rgb_dword_endn;
	__u32 y_rgb_qword_endn;
	__u32 uv_word_endn;
	__u32 uv_dword_endn;
	__u32 uv_qword_endn;
	__u32 rgb_swap_mode;
	__u32 a_swap_mode;
};

struct gsp_r6p0_img_layer_params {
	struct gsp_rect				clip_rect;
	struct gsp_rect				des_rect;
	struct gsp_rgb				grey;
	struct gsp_rgb				colorkey;
	struct gsp_rgb				pallet;
	struct gsp_r6p0_endian			endian;
	__u32						img_format;
	__u32						pitch;
	__u32						height;
	__u32						rot_angle;
	__u8						alpha;
	__u8						colorkey_en;
	__u8						pallet_en;
	__u8						fbcd_mod;
	__u8						pmargb_en;
	__u8						scaling_en;
	__u8						pmargb_mod;
	__u8						zorder;
	__u8						y2r_mod;
	__u8						y2y_mod;
	struct gsp_yuv_adjust_para		yuv_adjust;
	struct gsp_scale_para			scale_para;
	__u32   header_size_r;
};

struct gsp_r6p0_img_layer_user {
	struct gsp_layer_user				common;
	struct gsp_r6p0_img_layer_params	params;
};

struct gsp_r6p0_osd_layer_params {
	struct gsp_rect				clip_rect;
	struct gsp_pos				des_pos;
	struct gsp_rgb				grey;
	struct gsp_rgb				colorkey;
	struct gsp_rgb				pallet;
	struct gsp_r6p0_endian		endian;
	__u32						osd_format;
	__u32						pitch;
	__u32						height;
	__u8						alpha;
	__u8						colorkey_en;
	__u8						pallet_en;
	__u8						fbcd_mod;
	__u8						pmargb_en;
	__u8						pmargb_mod;
	__u8						zorder;
	__u32   header_size_r;
};

struct gsp_r6p0_osd_layer_user {
	struct gsp_layer_user				common;
	struct gsp_r6p0_osd_layer_params	params;
};

struct gsp_r6p0_des_layer_params {
	__u32						pitch;
	__u32						height;
	struct gsp_r6p0_endian		endian;
	__u32						img_format;
	__u32						rot_angle;
	__u8						r2y_mod;
	__u8						fbc_mod;
	__u8						dither_en;
	struct gsp_background_para	bk_para;
	__u32   header_size_r;
};

struct gsp_r6p0_des_layer_user {
	struct gsp_layer_user				common;
	struct gsp_r6p0_des_layer_params	params;
};

struct gsp_r6p0_misc_cfg_user {
	__u8 gsp_gap;
	__u8 core_num;
	__u8 co_work0;
	__u8 co_work1;
	__u8 work_mod;
	__u8 pmargb_en;
	struct gsp_rect workarea_src_rect;
	struct gsp_pos workarea_des_pos;
};

struct gsp_r6p0_cfg_user {
	struct gsp_r6p0_img_layer_user limg[R6P0_IMGL_NUM];
	struct gsp_r6p0_osd_layer_user losd[R6P0_OSDL_NUM];
	struct gsp_r6p0_des_layer_user ld1;
	struct gsp_r6p0_misc_cfg_user  misc;
};

struct drm_gsp_r6p0_cfg_user {
	__u8 gsp_id;
	bool async;
	__u32 size;
	__u32 num;
	bool split;
	char version[32];
	struct gsp_r6p0_cfg_user *config;
};

struct gsp_r6p0_capability {
	struct gsp_capability common;
	/* 1: means 1/16, 64 means 4*/
	__u32 scale_range_up;
	/* 1: means 1/16, 64 means 4*/
	__u32 scale_range_down;
	__u32 yuv_xywh_even;
	__u32 max_video_size;
	__u32 video_need_copy;
	__u32 blend_video_with_OSD;
	__u32 OSD_scaling;
	__u32 scale_updown_sametime;
	__u32 max_yuvLayer_cnt;
	__u32 max_scaleLayer_cnt;
	__u32 seq0_scale_range_up;
	__u32 seq0_scale_range_down;
	__u32 seq1_scale_range_up;
	__u32 seq1_scale_range_down;
	__u32 src_yuv_xywh_even_limit;
	__u32 csc_matrix_in;
	__u32 csc_matrix_out;

	__u32 block_alpha_limit;
	__u32 max_throughput;

	__u32 max_gspmmu_size;
	__u32 max_gsp_bandwidth;
};

#endif

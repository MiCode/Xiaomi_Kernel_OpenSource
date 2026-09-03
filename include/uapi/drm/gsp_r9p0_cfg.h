/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _UAPI_VIDEO_GSP_R9P0_CFG_H_
#define _UAPI_VIDEO_GSP_R9P0_CFG_H_

#include <linux/ioctl.h>
#include <linux/types.h>
#include "gsp_cfg.h"

#define R9P0_IMGL_NUM 2
#define R9P0_OSDL_NUM 2
#define R9P0_IMGSEC_NUM 0
#define R9P0_OSDSEC_NUM 1
#define HDR_TM_LUT_SIZE (1024 + 1)
#define GSP_R9P0_FREQ_256M 256000000
#define GSP_R9P0_FREQ_307_2M 307200000
#define GSP_R9P0_FREQ_384M 384000000
#define GSP_R9P0_FREQ_512M 512000000
#define GSP_R9P0_FREQ_614_4M 614400000

/*Original: B3B2B1B0*/
enum gsp_r9p0_word_endian {
	GSP_R9P0_WORD_ENDN_0 = 0x00,	/*B3B2B1B0*/
	GSP_R9P0_WORD_ENDN_1,		/*B0B1B2B3*/
	GSP_R9P0_WORD_ENDN_2,		/*B1B0B3B2*/
	GSP_R9P0_WORD_ENDN_3,		/*B2B3B0B1*/
	GSP_R9P0_WORD_ENDN_MAX_NUM,
};

enum gsp_r9p0_dword_endian {
	GSP_R9P0_DWORD_ENDN_0 = 0x00,	/*B7B6B5B4_B3B2B1B0*/
	GSP_R9P0_DWORD_ENDN_1,		/*B3B2B1B0_B7B6B5B4*/
	GSP_R9P0_DWORD_ENDN_MAX_NUM,
};

enum gsp_r9p0_qword_endian {
	/*B15B14B13B12_B11B10B9B8_B7B6B5B4_B3B2B1B0*/
	GSP_R9P0_QWORD_ENDN_0 = 0x00,
	/*B7B6B5B4_B3B2B1B0_B15B14B13B12_B11B10B9B8*/
	GSP_R9P0_QWORD_ENDN_1,
	GSP_R9P0_QWORD_ENDN_MAX_NUM,
};

enum gsp_r9p0_rgb_swap_mod {
	GSP_R9P0_RGB_SWP_RGB = 0x00,
	GSP_R9P0_RGB_SWP_RBG,
	GSP_R9P0_RGB_SWP_GRB,
	GSP_R9P0_RGB_SWP_GBR,
	GSP_R9P0_RGB_SWP_BGR,
	GSP_R9P0_RGB_SWP_BRG,
	GSP_R9P0_RGB_SWP_MAX,
};

enum gsp_r9p0_a_swap_mod {
	GSP_R9P0_A_SWAP_ARGB,
	GSP_R9P0_A_SWAP_RGBA,
	GSP_R9P0_A_SWAP_MAX,
};

enum gsp_r9p0_img_layer_format {
	GSP_R9P0_IMG_FMT_ARGB888 = 0x00,
	GSP_R9P0_IMG_FMT_RGB888,
	GSP_R9P0_IMG_FMT_YUV422_2P,
	GSP_R9P0_IMG_FMT_RESERVED,
	GSP_R9P0_IMG_FMT_YUV420_2P,
	GSP_R9P0_IMG_FMT_YUV420_3P,
	GSP_R9P0_IMG_FMT_RGB565,
	GSP_R9P0_IMG_FMT_YV12,
	GSP_R9P0_IMG_FMT_YCBCR_P016,
	GSP_R9P0_IMG_FMT_MAX_NUM,
};

enum gsp_r9p0_osd_layer_format {
	GSP_R9P0_OSD_FMT_ARGB888 = 0x00,
	GSP_R9P0_OSD_FMT_RGB888,
	GSP_R9P0_OSD_FMT_RGB565,
	GSP_R9P0_OSD_FMT_MAX_NUM,
};

enum gsp_r9p0_des_layer_format {
	GSP_R9P0_DST_FMT_ARGB888 = 0x00,
	GSP_R9P0_DST_FMT_RGB888,
	GSP_R9P0_DST_FMT_RGB565,
	GSP_R9P0_DST_FMT_YUV420_2P,
	GSP_R9P0_DST_FMT_YUV420_3P,
	GSP_R9P0_DST_FMT_YUV422_2P,
	GSP_R9P0_DST_FMT_RGB666,
	GSP_R9P0_DST_FMT_MAX_NUM,
};

struct gsp_r9p0_endian {
	__u32 y_rgb_word_endn;
	__u32 y_rgb_dword_endn;
	__u32 y_rgb_qword_endn;
	__u32 uv_word_endn;
	__u32 uv_dword_endn;
	__u32 uv_qword_endn;
	__u32 rgb_swap_mode;
	__u32 a_swap_mode;
};

struct gsp_r9p0_img_layer_params {
	struct gsp_rect				clip_rect;
	struct gsp_rect				des_rect;
	struct gsp_rgb				grey;
	struct gsp_rgb				colorkey;
	struct gsp_rgb				pallet;
	struct gsp_r9p0_endian			endian;
	__u32					img_format;
	__u32					pitch;
	__u32					height;
	__u32					rot_angle;
	__u8					alpha;
	__u8					colorkey_en;
	__u8					pallet_en;
	__u8					fbcd_mod;
	__u8					pmargb_en;
	__u8					scaling_en;
	__u8					pmargb_mod;
	__u8					zorder;
	__u8					y2r_mod;
	__u8					y2y_mod;
	struct gsp_yuv_adjust_para		yuv_adjust;
	struct gsp_scale_para			scale_para;
	__u32   				header_size_r;
	__u8					secure_en;
	__u8					hdr2rgb_mod;
};

struct gsp_r9p0_img_layer_user {
	struct gsp_layer_user			common;
	struct gsp_r9p0_img_layer_params	params;
};

struct gsp_r9p0_osd_layer_params {
	struct gsp_rect				clip_rect;
	struct gsp_pos				des_pos;
	struct gsp_rgb				grey;
	struct gsp_rgb				colorkey;
	struct gsp_rgb				pallet;
	struct gsp_r9p0_endian			endian;
	__u32					osd_format;
	__u32					pitch;
	__u32					height;
	__u8					alpha;
	__u8					colorkey_en;
	__u8					pallet_en;
	__u8					fbcd_mod;
	__u8					pmargb_en;
	__u8					pmargb_mod;
	__u8					zorder;
	__u32   				header_size_r;
	__u8					secure_en;
};

struct gsp_r9p0_osd_layer_user {
	struct gsp_layer_user			common;
	struct gsp_r9p0_osd_layer_params	params;
};

struct gsp_r9p0_des_layer_params {
	__u32					pitch;
	__u32					height;
	struct gsp_r9p0_endian			endian;
	__u32					img_format;
	__u32					rot_angle;
	__u8					r2y_mod;
	__u8					fbc_mod;
	__u8					dither_en;
	struct gsp_background_para		bk_para;
	__u32   				header_size_r;
};

struct gsp_r9p0_des_layer_user {
	struct gsp_layer_user			common;
	struct gsp_r9p0_des_layer_params	params;
};

struct gsp_r9p0_hdr10_cfg {
	int video_range; /* 0: narrow range, 1: full range */
	int transfer_char;

	int maxcll;
	int maxscl[3];
	int max_maxscl;

	double max_master_luminance;
	double min_master_luminance;
	double manual_max_luminance;

	double csc2_ratio;
	double beta_ratio;
	double norm_ratio;

	double norm_ratio_step2;
	double beta_ratio_step2;

	bool reg_hdr_slp;
	bool reg_hdr_bypass_csc1;
	bool reg_hdr_bypass_degamma;
	bool reg_hdr_bypass_csc2;
	bool reg_hdr_csc2_gain_bypass;
	bool reg_hdr_bypass_gamma;
	bool reg_hdr_bypass_csc3;
	bool reg_hdr_force_in_range_csc1;
	bool reg_hdr_force_in_range_csc3;
	bool reg_hdr_gamut_map_en;
	bool reg_hdr_csc1_cl_en;
	bool reg_hdr_avg_en;
	int reg_hdr_alpha_gain;
	int reg_hdr_sat_thr;

	int reg_hdr_csc1_ycr;
	int reg_hdr_csc1_ucr;
	int reg_hdr_csc1_vcr;
	int reg_hdr_csc1_ycg;
	int reg_hdr_csc1_ucg;
	int reg_hdr_csc1_vcg;
	int reg_hdr_csc1_ycb;
	int reg_hdr_csc1_ucb;
	int reg_hdr_csc1_vcb;
	int reg_hdr_csc1_ucb2;
	int reg_hdr_csc1_vcr2;
	int reg_hdr_csc1_yls;
	int reg_hdr_csc1_uls;
	int reg_hdr_csc1_vls;

	int reg_hdr_csc2_c11;
	int reg_hdr_csc2_c12;
	int reg_hdr_csc2_c13;
	int reg_hdr_csc2_c21;
	int reg_hdr_csc2_c22;
	int reg_hdr_csc2_c23;
	int reg_hdr_csc2_c31;
	int reg_hdr_csc2_c32;
	int reg_hdr_csc2_c33;
	int reg_hdr_csc2_c11_2;
	int reg_hdr_csc2_c12_2;
	int reg_hdr_csc2_c13_2;
	int reg_hdr_csc2_c21_2;
	int reg_hdr_csc2_c22_2;
	int reg_hdr_csc2_c23_2;
	int reg_hdr_csc2_c31_2;
	int reg_hdr_csc2_c32_2;
	int reg_hdr_csc2_c33_2;
	int reg_hdr_csc2_offset_r;
	int reg_hdr_csc2_offset_g;
	int reg_hdr_csc2_offset_b;
	int reg_hdr_csc2_gain;

	int reg_hdr_rg_step1;
	int reg_hdr_rg_step2;
	int reg_hdr_rg_step3;
	int reg_hdr_rg_step4;
	int reg_hdr_diff_sat_thr;
	int reg_hdr_csc3_a11;
	int reg_hdr_csc3_a12;
	int reg_hdr_csc3_a13;
	int reg_hdr_csc3_a21;
	int reg_hdr_csc3_a22;
	int reg_hdr_csc3_a23;
	int reg_hdr_csc3_a31;
	int reg_hdr_csc3_a32;
	int reg_hdr_csc3_a33;

	bool reg_hdr_tm_bypass;
	bool reg_hdr_tm_force_in_range;
	int reg_hdr_tm_rw_sel;
	int reg_hdr_tm_use_sel;
	bool reg_hdr_tm_force_rw_cur;
	bool reg_hdr_tm1_en;
	int reg_hdr_tm_step1;
	int reg_hdr_tm_step2;
	int reg_hdr_tm_step3;
	int reg_hdr_tm_step4;
	int reg_hdr_tm_norm_gain;
	int reg_hdr_tm1_beta_gain;
	int reg_hdr_tm2_beta_gain;
	int reg_hdr_tm3_beta_gain;

	__u32 hdr_degamma_lut_type;
	__u32 hdr_tone_mapping_lut_table[HDR_TM_LUT_SIZE];
};

struct gsp_r9p0_misc_cfg_user {
	__u8 gsp_gap;
	__u8 core_num;
	__u8 co_work0;
	__u8 co_work1;
	__u8 work_mod;
	__u8 pmargb_en;
	__u8 secure_en;
	bool hdr_flag[R9P0_IMGL_NUM];
	bool first10bit_frame[R9P0_IMGL_NUM];
	bool hdr10plus_update[R9P0_IMGL_NUM];
	__u32 work_freq;
	struct gsp_rect workarea_src_rect;
	struct gsp_pos workarea_des_pos;
	struct gsp_r9p0_hdr10_cfg hdr10_para[R9P0_IMGL_NUM];
};

struct gsp_r9p0_cfg_user {
	struct gsp_r9p0_img_layer_user limg[R9P0_IMGL_NUM];
	struct gsp_r9p0_osd_layer_user losd[R9P0_OSDL_NUM];
	struct gsp_r9p0_des_layer_user ld1;
	struct gsp_r9p0_misc_cfg_user  misc;
};

struct drm_gsp_r9p0_cfg_user {
	__u8 gsp_id;
	bool async;
	__u32 size;
	__u32 num;
	bool split;
	char version[32];
	struct gsp_r9p0_cfg_user *config;
};

struct gsp_r9p0_capability {
	struct gsp_capability common;
	char board[32];
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

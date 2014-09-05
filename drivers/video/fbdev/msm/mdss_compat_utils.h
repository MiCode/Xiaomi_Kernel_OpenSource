/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_COMPAT_UTILS_H
#define MDSS_COMPAT_UTILS_H

struct mdp_buf_sync32 {
	u32		flags;
	u32		acq_fen_fd_cnt;
	u32		session_id;
	compat_caddr_t	acq_fen_fd;
	compat_caddr_t	rel_fen_fd;
	compat_caddr_t  retire_fen_fd;
};

struct fb_cmap32 {
	u32		start;
	u32		len;
	compat_caddr_t	red;
	compat_caddr_t	green;
	compat_caddr_t	blue;
	compat_caddr_t	transp;
};

struct fb_image32 {
	u32 dx;
	u32 dy;
	u32 width;
	u32 height;
	u32 fg_color;
	u32 bg_color;
	u8 depth;
	compat_caddr_t data;
	struct fb_cmap32 cmap;
};

struct fb_cursor32 {
	u16 set;
	u16 enable;
	u16 rop;
	compat_caddr_t mask;
	struct fbcurpos	hot;
	struct fb_image32 image;
};

struct mdp_ccs32 {
};

struct msmfb_overlay_blt32 {
};

struct msmfb_overlay_3d32 {
};

struct msmfb_mixer_info_req32 {
};

struct msmfb_metadata32 {
	uint32_t op;
	uint32_t flags;
	union {
		struct mdp_misr misr_request;
		struct mdp_blend_cfg blend_cfg;
		struct mdp_mixer_cfg mixer_cfg;
		uint32_t panel_frame_rate;
		uint32_t video_info_code;
		struct mdss_hw_caps caps;
		uint8_t secure_en;
	} data;
};

struct mdp_histogram_start_req32 {
	uint32_t block;
	uint8_t frame_cnt;
	uint8_t bit_mask;
	uint16_t num_bins;
};

struct mdp_histogram_data32 {
	uint32_t block;
	uint32_t bin_cnt;
	compat_caddr_t c0;
	compat_caddr_t c1;
	compat_caddr_t c2;
	compat_caddr_t extra_info;
};

struct mdp_pcc_coeff32 {
	uint32_t c, r, g, b, rr, gg, bb, rg, gb, rb, rgb_0, rgb_1;
};

struct mdp_pcc_cfg_data32 {
	uint32_t block;
	uint32_t ops;
	struct mdp_pcc_coeff32 r, g, b;
};

struct mdp_csc_cfg32 {
	/* flags for enable CSC, toggling RGB,YUV input/output */
	uint32_t flags;
	uint32_t csc_mv[9];
	uint32_t csc_pre_bv[3];
	uint32_t csc_post_bv[3];
	uint32_t csc_pre_lv[6];
	uint32_t csc_post_lv[6];
};

struct mdp_csc_cfg_data32 {
	uint32_t block;
	struct mdp_csc_cfg32 csc_data;
};

struct mdp_bl_scale_data32 {
	uint32_t min_lvl;
	uint32_t scale;
};

struct mdp_pa_mem_col_cfg32 {
	uint32_t color_adjust_p0;
	uint32_t color_adjust_p1;
	uint32_t hue_region;
	uint32_t sat_region;
	uint32_t val_region;
};

struct mdp_pa_v2_data32 {
	/* Mask bits for PA features */
	uint32_t flags;
	uint32_t global_hue_adj;
	uint32_t global_sat_adj;
	uint32_t global_val_adj;
	uint32_t global_cont_adj;
	struct mdp_pa_mem_col_cfg32 skin_cfg;
	struct mdp_pa_mem_col_cfg32 sky_cfg;
	struct mdp_pa_mem_col_cfg32 fol_cfg;
	uint32_t six_zone_len;
	uint32_t six_zone_thresh;
	compat_caddr_t six_zone_curve_p0;
	compat_caddr_t six_zone_curve_p1;
};

struct mdp_pa_v2_cfg_data32 {
	uint32_t block;
	struct mdp_pa_v2_data32 pa_v2_data;
};

struct mdp_pa_cfg32 {
	uint32_t flags;
	uint32_t hue_adj;
	uint32_t sat_adj;
	uint32_t val_adj;
	uint32_t cont_adj;
};

struct mdp_pa_cfg_data32 {
	uint32_t block;
	struct mdp_pa_cfg32 pa_data;
};

struct mdp_igc_lut_data32 {
	uint32_t block;
	uint32_t len, ops;
	compat_caddr_t c0_c1_data;
	compat_caddr_t c2_data;
};

struct mdp_hist_lut_data32 {
	uint32_t block;
	uint32_t ops;
	uint32_t len;
	compat_caddr_t data;
};

struct mdp_ar_gc_lut_data32 {
	uint32_t x_start;
	uint32_t slope;
	uint32_t offset;
};

struct mdp_pgc_lut_data32 {
	uint32_t block;
	uint32_t flags;
	uint8_t num_r_stages;
	uint8_t num_g_stages;
	uint8_t num_b_stages;
	compat_caddr_t r_data;
	compat_caddr_t g_data;
	compat_caddr_t b_data;
};

struct mdp_lut_cfg_data32 {
	uint32_t lut_type;
	union {
		struct mdp_igc_lut_data32 igc_lut_data;
		struct mdp_pgc_lut_data32 pgc_lut_data;
		struct mdp_hist_lut_data32 hist_lut_data;
	} data;
};

struct mdp_qseed_cfg32 {
	uint32_t table_num;
	uint32_t ops;
	uint32_t len;
	compat_caddr_t data;
};

struct mdp_qseed_cfg_data32 {
	uint32_t block;
	struct mdp_qseed_cfg32 qseed_data;
};

struct mdp_dither_cfg_data32 {
	uint32_t block;
	uint32_t flags;
	uint32_t g_y_depth;
	uint32_t r_cr_depth;
	uint32_t b_cb_depth;
};

struct mdp_gamut_cfg_data32 {
	uint32_t block;
	uint32_t flags;
	uint32_t gamut_first;
	uint32_t tbl_size[MDP_GAMUT_TABLE_NUM];
	compat_caddr_t r_tbl[MDP_GAMUT_TABLE_NUM];
	compat_caddr_t g_tbl[MDP_GAMUT_TABLE_NUM];
	compat_caddr_t b_tbl[MDP_GAMUT_TABLE_NUM];
};

struct mdp_calib_config_data32 {
	uint32_t ops;
	uint32_t addr;
	uint32_t data;
};

struct mdp_calib_config_buffer32 {
	uint32_t ops;
	uint32_t size;
	compat_caddr_t buffer;
};

struct mdp_calib_dcm_state32 {
	uint32_t ops;
	uint32_t dcm_state;
};

struct mdss_ad_init32 {
	uint32_t asym_lut[33];
	uint32_t color_corr_lut[33];
	uint8_t i_control[2];
	uint16_t black_lvl;
	uint16_t white_lvl;
	uint8_t var;
	uint8_t limit_ampl;
	uint8_t i_dither;
	uint8_t slope_max;
	uint8_t slope_min;
	uint8_t dither_ctl;
	uint8_t format;
	uint8_t auto_size;
	uint16_t frame_w;
	uint16_t frame_h;
	uint8_t logo_v;
	uint8_t logo_h;
	uint32_t alpha;
	uint32_t alpha_base;
	uint32_t bl_lin_len;
	uint32_t bl_att_len;
	compat_caddr_t bl_lin;
	compat_caddr_t bl_lin_inv;
	compat_caddr_t bl_att_lut;
};

struct mdss_ad_cfg32 {
	uint32_t mode;
	uint32_t al_calib_lut[33];
	uint16_t backlight_min;
	uint16_t backlight_max;
	uint16_t backlight_scale;
	uint16_t amb_light_min;
	uint16_t filter[2];
	uint16_t calib[4];
	uint8_t strength_limit;
	uint8_t t_filter_recursion;
	uint16_t stab_itr;
	uint32_t bl_ctrl_mode;
};

/* ops uses standard MDP_PP_* flags */
struct mdss_ad_init_cfg32 {
	uint32_t ops;
	union {
		struct mdss_ad_init32 init;
		struct mdss_ad_cfg32 cfg;
	} params;
};

struct mdss_ad_input32 {
	uint32_t mode;
	union {
		uint32_t amb_light;
		uint32_t strength;
		uint32_t calib_bl;
	} in;
	uint32_t output;
};

struct mdss_calib_cfg32 {
	uint32_t ops;
	uint32_t calib_mask;
};

struct mdp_pp_init_data32 {
	uint32_t init_request;
};

struct mdp_histogram_cfg32 {
	uint32_t ops;
	uint32_t block;
	uint8_t frame_cnt;
	uint8_t bit_mask;
	uint16_t num_bins;
};

struct mdp_sharp_cfg32 {
	uint32_t flags;
	uint32_t strength;
	uint32_t edge_thr;
	uint32_t smooth_thr;
	uint32_t noise_thr;
};

struct mdp_overlay_pp_params32 {
	uint32_t config_ops;
	struct mdp_csc_cfg32 csc_cfg;
	struct mdp_qseed_cfg32 qseed_cfg[2];
	struct mdp_pa_cfg32 pa_cfg;
	struct mdp_pa_v2_data32 pa_v2_cfg;
	struct mdp_igc_lut_data32 igc_cfg;
	struct mdp_sharp_cfg32 sharp_cfg;
	struct mdp_histogram_cfg32 hist_cfg;
	struct mdp_hist_lut_data32 hist_lut_cfg;
};

struct msmfb_mdp_pp32 {
	uint32_t op;
	union {
		struct mdp_pcc_cfg_data32 pcc_cfg_data;
		struct mdp_csc_cfg_data32 csc_cfg_data;
		struct mdp_lut_cfg_data32 lut_cfg_data;
		struct mdp_qseed_cfg_data32 qseed_cfg_data;
		struct mdp_bl_scale_data32 bl_scale_data;
		struct mdp_pa_cfg_data32 pa_cfg_data;
		struct mdp_pa_v2_cfg_data32 pa_v2_cfg_data;
		struct mdp_dither_cfg_data32 dither_cfg_data;
		struct mdp_gamut_cfg_data32 gamut_cfg_data;
		struct mdp_calib_config_data32 calib_cfg;
		struct mdss_ad_init_cfg32 ad_init_cfg;
		struct mdss_calib_cfg32 mdss_calib_cfg;
		struct mdss_ad_input32 ad_input;
		struct mdp_calib_config_buffer32 calib_buffer;
		struct mdp_calib_dcm_state32 calib_dcm;
		struct mdp_pp_init_data32 init_data;
	} data;
};

struct mdp_overlay32 {
	struct msmfb_img src;
	struct mdp_rect src_rect;
	struct mdp_rect dst_rect;
	uint32_t z_order;	/* stage number */
	uint32_t is_fg;	/* control alpha & transp */
	uint32_t alpha;
	uint32_t blend_op;
	uint32_t transp_mask;
	uint32_t flags;
	uint32_t pipe_type;
	uint32_t id;
	uint8_t priority;
	uint32_t user_data[6];
	uint32_t bg_color;
	uint8_t horz_deci;
	uint8_t vert_deci;
	struct mdp_overlay_pp_params32 overlay_pp_cfg;
	struct mdp_scale_data scale;
};

struct mdp_overlay_list32 {
	uint32_t num_overlays;
	compat_caddr_t overlay_list;
	uint32_t flags;
	uint32_t processed_overlays;
};

#endif

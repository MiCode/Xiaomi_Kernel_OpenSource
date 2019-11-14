/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_PANEL_EXT_H__
#define __MTK_PANEL_EXT_H__

#include <drm/drm_panel.h>

#define RT_MAX_NUM 10
#define ESD_CHECK_NUM 3

struct mtk_dsi;
struct cmdq_pkt;
typedef void (*dcs_write_gce) (struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				  const void *data, size_t len);
enum MTK_PANEL_OUTPUT_MODE {
	MTK_PANEL_SINGLE_PORT = 0x0,
	MTK_PANEL_DSC_SINGLE_PORT,
	MTK_PANEL_DUAL_PORT,
};

struct esd_check_item {
	unsigned char cmd;
	unsigned char count;
	unsigned char para_list[RT_MAX_NUM];
	unsigned char mask_list[RT_MAX_NUM];
};

enum MTK_PANEL_MODE_SWITCH_STAGE {
	BEFORE_DSI_POWERDOWN,
	AFTER_DSI_POWERON,
};

struct mtk_panel_funcs {
	int (*set_backlight_cmdq)(void *dsi_drv, dcs_write_gce cb,
		void *handle, unsigned int level);
	int (*reset)(struct drm_panel *panel, int on);
	int (*ata_check)(struct drm_panel *panel);
	int (*ext_param_set)(struct drm_panel *panel, unsigned int mode);
	int (*mode_switch)(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage);
	/**
	 * @doze_enable:
	 *
	 * Call the @doze_enable before starting AOD mode. The LCM off may add
	 * at the beginning of this function to avoid panel show unexpected
	 * content when switching to specific panel low power mode.
	 */
	int (*doze_enable)(struct drm_panel *panel);

	/**
	 * @doze_enable:
	 *
	 * Call the @doze_enable before starting AOD mode. The LCM off may add
	 * at the beginning of this function to avoid panel show unexpected
	 * content when switching back to normal mode.
	 */
	int (*doze_disable)(struct drm_panel *panel);

	/**
	 * @doze_post_disp_on:
	 *
	 * In some situation, the LCM off may set in @doze_enable & @disable.
	 * After LCM switch to the new mode stable, system call
	 * @doze_post_disp_on to turn on panel.
	 */
	int (*doze_post_disp_on)(struct drm_panel *panel);

	/**
	 * @doze_area:
	 *
	 * Send the panel area in command here.
	 */
	int (*doze_area)(struct drm_panel *panel);

	/**
	 * @doze_get_mode_flags:
	 *
	 * If CV switch is needed for doze mode, fill the mode_flags in this
	 * function for both CMD and VDO mode.
	 */
	unsigned long (*doze_get_mode_flags)(
		struct drm_panel *panel, int aod_en);
};

enum MIPITX_PHY_PORT {
	MIPITX_PHY_PORT_0 = 0,
	MIPITX_PHY_PORT_1,
	MIPITX_PHY_PORT_NUM
};

enum MIPITX_PHY_LANE_SWAP {
	MIPITX_PHY_LANE_0 = 0,
	MIPITX_PHY_LANE_1,
	MIPITX_PHY_LANE_2,
	MIPITX_PHY_LANE_3,
	MIPITX_PHY_LANE_CK,
	MIPITX_PHY_LANE_RX,
	MIPITX_PHY_LANE_NUM
};

struct mtk_panel_dsc_params {
	unsigned int enable;
	unsigned int ver; /* [7:4] major [3:0] minor */
	unsigned int slice_mode;
	unsigned int rgb_swap;
	unsigned int dsc_cfg;
	unsigned int rct_on;
	unsigned int bit_per_channel;
	unsigned int dsc_line_buf_depth;
	unsigned int bp_enable;
	unsigned int bit_per_pixel;
	unsigned int pic_height; /* need to check */
	unsigned int pic_width;  /* need to check */
	unsigned int slice_height;
	unsigned int slice_width;
	unsigned int chunk_size;
	unsigned int xmit_delay;
	unsigned int dec_delay;
	unsigned int scale_value;
	unsigned int increment_interval;
	unsigned int decrement_interval;
	unsigned int line_bpg_offset;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;
	unsigned int flatness_minqp;
	unsigned int flatness_maxqp;
	unsigned int rc_model_size;
	unsigned int rc_edge_factor;
	unsigned int rc_quant_incr_limit0;
	unsigned int rc_quant_incr_limit1;
	unsigned int rc_tgt_offset_hi;
	unsigned int rc_tgt_offset_lo;
};
struct mtk_dsi_phy_timcon {
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int clk_trail;
	unsigned int cont_det;
	unsigned int da_hs_sync;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;
};

struct dynamic_mipi_params {
	unsigned int switch_en;
	unsigned int pll_clk;
	unsigned int data_rate;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;

	unsigned int hsa;
	unsigned int hbp;
	unsigned int hfp;
};

struct mtk_panel_params {
	unsigned int pll_clk;
	unsigned int data_rate;
	struct mtk_dsi_phy_timcon phy_timcon;
	unsigned int vfp_low_power;
	struct dynamic_mipi_params dyn;
	unsigned int cust_esd_check;
	unsigned int esd_check_enable;
	struct esd_check_item lcm_esd_check_table[ESD_CHECK_NUM];
	unsigned int ssc_disable;
	unsigned int ssc_range;
	int lcm_color_mode;
	unsigned int min_luminance;
	unsigned int average_luminance;
	unsigned int max_luminance;
	unsigned int round_corner_en;
	unsigned int corner_pattern_height;
	unsigned int corner_pattern_height_bot;
	unsigned int corner_pattern_tp_size;
	void *corner_pattern_lt_addr;
	unsigned int physical_width_um;
	unsigned int physical_height_um;
	unsigned int lane_swap_en;
	enum MIPITX_PHY_LANE_SWAP
		lane_swap[MIPITX_PHY_PORT_NUM][MIPITX_PHY_LANE_NUM];
	struct mtk_panel_dsc_params dsc_params;
	unsigned int output_mode;
};

struct mtk_panel_ext {
	struct mtk_panel_funcs *funcs;
	struct mtk_panel_params *params;
};

struct mtk_panel_ctx {
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;

	struct list_head list;
};

void mtk_panel_init(struct mtk_panel_ctx *ctx);
void mtk_panel_add(struct mtk_panel_ctx *ctx);
void mtk_panel_remove(struct mtk_panel_ctx *ctx);
int mtk_panel_attach(struct mtk_panel_ctx *ctx, struct drm_panel *panel);
int mtk_panel_detach(struct mtk_panel_ctx *ctx);
struct mtk_panel_ext *find_panel_ext(struct drm_panel *panel);
int mtk_panel_ext_create(struct device *dev,
			 struct mtk_panel_params *ext_params,
			 struct mtk_panel_funcs *ext_funcs,
			 struct drm_panel *panel);


#endif

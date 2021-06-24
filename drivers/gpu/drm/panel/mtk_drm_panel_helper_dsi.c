// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include "mtk_drm_panel_helper.h"

/* parsing of struct mtk_panel_dsc_params */
static void parse_lcm_dsi_dsc_mode(struct device_node *np,
			struct mtk_panel_dsc_params *dsc_params)
{
	if (IS_ERR_OR_NULL(dsc_params))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_enable",
			&dsc_params->enable);

	if (dsc_params->enable == 0)
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_ver",
			&dsc_params->ver);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_slice_mode",
			&dsc_params->slice_mode);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rgb_swap",
			&dsc_params->rgb_swap);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_cfg",
			&dsc_params->dsc_cfg);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rct_on",
			&dsc_params->rct_on);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_bit_per_channel",
			&dsc_params->bit_per_channel);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_line_buf_depth",
			&dsc_params->dsc_line_buf_depth);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_bp_enable",
			&dsc_params->bp_enable);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_bit_per_pixel",
			&dsc_params->bit_per_pixel);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_pic_height",
			&dsc_params->pic_height);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_pic_width",
			&dsc_params->pic_width);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_slice_height",
			&dsc_params->slice_height);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_slice_width",
			&dsc_params->slice_width);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_chunk_size",
			&dsc_params->chunk_size);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_xmit_delay",
			&dsc_params->xmit_delay);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_dec_delay",
			&dsc_params->dec_delay);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_scale_value",
			&dsc_params->scale_value);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_increment_interval",
			&dsc_params->increment_interval);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_decrement_interval",
			&dsc_params->decrement_interval);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_line_bpg_offset",
			&dsc_params->line_bpg_offset);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_nfl_bpg_offset",
			&dsc_params->nfl_bpg_offset);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_slice_bpg_offset",
			&dsc_params->slice_bpg_offset);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_initial_offset",
			&dsc_params->initial_offset);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_final_offset",
			&dsc_params->final_offset);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_flatness_minqp",
			&dsc_params->flatness_minqp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_flatness_maxqp",
			&dsc_params->flatness_maxqp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rc_model_size",
			&dsc_params->rc_model_size);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rc_edge_factor",
			&dsc_params->rc_edge_factor);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rc_quant_incr_limit0",
			&dsc_params->rc_quant_incr_limit0);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rc_quant_incr_limit1",
			&dsc_params->rc_quant_incr_limit1);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rc_tgt_offset_hi",
			&dsc_params->rc_tgt_offset_hi);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dsc_rc_tgt_offset_lo",
			&dsc_params->rc_tgt_offset_lo);
}

/* parsing of struct mtk_dsi_phy_timcon */
static void parse_lcm_dsi_phy_timcon(struct device_node *np,
			struct mtk_dsi_phy_timcon *phy_timcon)
{
	if (IS_ERR_OR_NULL(phy_timcon))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_hs_trail",
			&phy_timcon->hs_trail);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_hs_prpr",
			&phy_timcon->hs_prpr);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_hs_zero",
			&phy_timcon->hs_zero);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_lpx",
			&phy_timcon->lpx);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_ta_get",
			&phy_timcon->ta_get);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_ta_sure",
			&phy_timcon->ta_sure);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_ta_go",
			&phy_timcon->ta_go);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_da_hs_exit",
			&phy_timcon->da_hs_exit);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_clk_trail",
			&phy_timcon->clk_trail);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_cont_det",
			&phy_timcon->cont_det);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_da_hs_sync",
			&phy_timcon->da_hs_sync);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_clk_zero",
			&phy_timcon->clk_zero);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_clk_prpr",
			&phy_timcon->clk_hs_prpr);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_clk_exit",
			&phy_timcon->clk_hs_exit);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-phy_timcon_clk_post",
			&phy_timcon->clk_hs_post);
}

/* parsing of struct dynamic_mipi_params */
static void parse_lcm_dsi_dyn(struct device_node *np,
			struct dynamic_mipi_params *dyn)
{
	if (IS_ERR_OR_NULL(dyn) || IS_ERR_OR_NULL(np))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_switch_en",
			&dyn->switch_en);
	if (dyn->switch_en == 0)
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_pll_clk",
			&dyn->pll_clk);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_data_rate",
			&dyn->data_rate);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_vsa",
			&dyn->vsa);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_vbp",
			&dyn->vbp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_vfp",
			&dyn->vfp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_vfp_lp_dyn",
			&dyn->vfp_lp_dyn);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_hsa",
			&dyn->hsa);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_hbp",
			&dyn->hbp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_hfp",
			&dyn->hfp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_max_vfp_for_msync_dyn",
			&dyn->max_vfp_for_msync_dyn);
}

/* parsing of struct dynamic_fps_params */
static void parse_lcm_dsi_dyn_fps(struct device_node *np,
			struct dynamic_fps_params *dyn_fps)
{
	unsigned int temp[sizeof(struct dfps_switch_cmd)] = {0};
	int i = 0, j = 0, len = 0;
	char node[128] = { 0 };

	if (IS_ERR_OR_NULL(dyn_fps))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_fps_switch_en",
			&dyn_fps->switch_en);

	if (dyn_fps->switch_en == 0)
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_fps_vact_timing_fps",
			&dyn_fps->vact_timing_fps);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_fps_data_rate",
			&dyn_fps->data_rate);
	for (i = 0; i < MAX_DYN_CMD_NUM; i++) {
		snprintf(node, sizeof(node),
			 "lcm-params-dsi-dyn_fps_dfps_cmd_table%u", i);
		len = mtk_lcm_dts_read_u32_array(np, node, &temp[0], 0, 2);
		if (len != 2) {
			DDPMSG("%s, %d: the %d dyn fps of invalid cmd:%d\n",
				__func__, __LINE__, i, len);
			continue;
		}
		dyn_fps->dfps_cmd_table[i].src_fps = temp[0];
		dyn_fps->dfps_cmd_table[i].cmd_num = temp[1];
		if (dyn_fps->dfps_cmd_table[i].cmd_num == 0)
			continue;
		len = mtk_lcm_dts_read_u32_array(np, node, &temp[0], 0,
				dyn_fps->dfps_cmd_table[i].cmd_num + 2);
		if (len != dyn_fps->dfps_cmd_table[i].cmd_num + 2) {
			DDPMSG("%s, %d: the %d dyn fps of invalid cmd length:%d\n",
				__func__, __LINE__, i, len);
			dyn_fps->dfps_cmd_table[i].cmd_num = 0;
			continue;
		}
		for (j = 0; j < 64; j++)
			dyn_fps->dfps_cmd_table[i].para_list[j] =
					(unsigned char)temp[j + 2];
	}
}

/* parse dsi fps mode: drm_display_mode */
static void parse_lcm_dsi_fps_mode(struct device_node *np,
			struct drm_display_mode *mode)
{
	u32 hac = 0, hfp = 0, hsa = 0, hbp = 0;
	u32 vac = 0, vfp = 0, vsa = 0, vbp = 0;
	u32 hskew = 0, vscan = 0;

	if (IS_ERR_OR_NULL(mode))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-vertical_sync_active",
			&vsa);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-vertical_backporch",
			&vbp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-vertical_frontporch",
			&vfp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-vertical_active_line",
			&vac);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-horizontal_sync_active",
			&hsa);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-horizontal_backporch",
			&hbp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-horizontal_frontporch",
			&hfp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-horizontal_active_pixel",
			&hac);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-pixel_clock",
			&mode->clock);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-hskew",
			&hskew);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-vscan",
			&vscan);

	mode->hdisplay = (u16)hac;
	mode->hsync_start = (u16)(hac + hfp);
	mode->hsync_end = (u16)(hac + hfp + hsa);
	mode->htotal = (u16)(hac + hfp + hsa + hbp);
	mode->vdisplay = (u16)vac;
	mode->vsync_start = (u16)(vac + vfp);
	mode->vsync_end = (u16)(vac + vfp + vsa);
	mode->vtotal = (u16)(vac + vfp + vsa + vbp);
	mode->hskew = (u16)hskew;
	mode->vscan = (u16)vscan;
}

/* parse dsi fps ext params: mtk_panel_params */
static void parse_lcm_dsi_fps_ext_param(struct device_node *np,
			struct mtk_panel_params *ext_param)
{
	char prop[128] = { 0 };
	u8 temp[RT_MAX_NUM * 2 + 2] = {0};
	unsigned int i = 0, j = 0;
	int len = 0;

	if (IS_ERR_OR_NULL(ext_param) || IS_ERR_OR_NULL(np))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-pll_clock",
			&ext_param->pll_clk);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-data_rate",
			&ext_param->data_rate);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-vfp_for_low_power",
			&ext_param->vfp_low_power);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-ssc_disable",
			&ext_param->ssc_disable);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-ssc_range",
			&ext_param->ssc_range);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lcm_color_mode",
			&ext_param->lcm_color_mode);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-min_luminance",
			&ext_param->min_luminance);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-average_luminance",
			&ext_param->average_luminance);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-max_luminance",
			&ext_param->max_luminance);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-round_corner_en",
			&ext_param->round_corner_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_height",
			&ext_param->corner_pattern_height);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_height_bot",
			&ext_param->corner_pattern_height_bot);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_tp_size",
			&ext_param->corner_pattern_tp_size);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_tp_size_left",
			&ext_param->corner_pattern_tp_size_l);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_tp_size_right",
			&ext_param->corner_pattern_tp_size_r);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_lt_addr",
			(u32 *) (&(ext_param->corner_pattern_lt_addr)));
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_lt_addr_left",
			(u32 *) (&(ext_param->corner_pattern_lt_addr_l)));
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-corner_pattern_lt_addr_right",
			(u32 *) (&(ext_param->corner_pattern_lt_addr_r)));
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-physical_width_um",
			&ext_param->physical_width_um);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-physical_height_um",
			&ext_param->physical_height_um);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lane_swap_en",
			&ext_param->lane_swap_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-output_mode",
			&ext_param->output_mode);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lcm_cmd_if",
			&ext_param->lcm_cmd_if);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-hbm_en_time",
			&ext_param->hbm_en_time);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-hbm_dis_time",
			&ext_param->hbm_dis_time);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lcm_index",
			&ext_param->lcm_index);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-wait_sof_before_dec_vfp",
			&ext_param->wait_sof_before_dec_vfp);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-doze_delay",
			&ext_param->doze_delay);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lfr_enable",
			&ext_param->lfr_enable);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lfr_minimum_fps",
			&ext_param->lfr_minimum_fps);

	/* esd check */
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cust_esd_check",
			&ext_param->cust_esd_check);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-esd_check_enable",
			&ext_param->esd_check_enable);
	if (ext_param->esd_check_enable != 0) {
		for (i = 0; i < ESD_CHECK_NUM; i++) {
			snprintf(prop, sizeof(prop),
				 "lcm-params-dsi-lcm_esd_check_table%u", i);
			len = mtk_lcm_dts_read_u8_array(np,
					prop, temp, 0, sizeof(struct esd_check_item));
			if (len <= 0 || len > sizeof(struct esd_check_item)) {
				DDPMSG("%s, %d: the %d esd table of invalid cmd:%d\n",
					__func__, __LINE__, i, len);
				continue;
			}

			ext_param->lcm_esd_check_table[i].cmd = temp[0];
			ext_param->lcm_esd_check_table[i].count = temp[1];
			if (ext_param->lcm_esd_check_table[i].count == 0)
				continue;
			for (j = 0;
			     j < ext_param->lcm_esd_check_table[i].count; j++)
				ext_param->lcm_esd_check_table[i].para_list[j] =
					temp[j + 2];
			for (j = 0;
			     j < ext_param->lcm_esd_check_table[i].count; j++)
				ext_param->lcm_esd_check_table[i].mask_list[j] =
					temp[j + 2 +
					ext_param->lcm_esd_check_table[i].count];
			}
	}
	/* lane swap */
	if (ext_param->lane_swap_en != 0) {
		for (i = 0; i < MIPITX_PHY_PORT_NUM; i++) {
			snprintf(prop, sizeof(prop),
				 "lcm-params-dsi-lane_swap%u", i);
			mtk_lcm_dts_read_u32_array(np, prop,
					(u32 *)&ext_param->lane_swap[i][0],
					0, MIPITX_PHY_LANE_NUM);
		}
	}
}

static void parse_lcm_dsi_fps_setting(struct device_node *np,
	unsigned int fps, unsigned int phy_type,
	struct drm_display_mode *mode,
	struct mtk_panel_params *ext_param)
{
	struct device_node *child_np = NULL;
	char child[128] = { 0 };

	parse_lcm_dsi_fps_mode(np, mode);
	parse_lcm_dsi_fps_ext_param(np, ext_param);

	if (phy_type == MTK_LCM_MIPI_CPHY)
		ext_param->is_cphy = 1;
	else
		ext_param->is_cphy = 0;

	for_each_available_child_of_node(np, child_np) {
		/* dsc params */
		snprintf(child, sizeof(child) - 1,
			"mediatek,lcm_params-dsi-fps%u-dsc-params", fps);
		if (of_device_is_compatible(child_np, child)) {
			DDPMSG("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_dsc_mode(child_np,
					&ext_param->dsc_params);
		}
		/* phy timcon */
		snprintf(child, sizeof(child) - 1,
			"mediatek,lcm_params-dsi-fps%u-phy-timcon", fps);
		if (of_device_is_compatible(child_np, child)) {
			DDPMSG("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_phy_timcon(child_np,
					&ext_param->phy_timcon);
		}
		/* dyn */
		snprintf(child, sizeof(child) - 1,
			"mediatek,lcm_params-dsi-fps%u-dyn", fps);
		if (of_device_is_compatible(child_np, child)) {
			DDPMSG("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_dyn(child_np,
					&ext_param->dyn);
		}
		/* dyn fps */
		snprintf(child, sizeof(child) - 1,
			"mediatek,lcm_params-dsi-fps%u-dyn-fps", fps);
		if (of_device_is_compatible(child_np, child)) {
			DDPMSG("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_dyn_fps(child_np,
					&ext_param->dyn_fps);
		}
	}
}

int parse_lcm_params_dsi(struct device_node *np,
		struct mtk_lcm_params_dsi *params)
{
	unsigned int i = 0, phy_type = 0, len = 0;
	unsigned int flag[64] = { 0 };
	char mode_node[128] = { 0 };
	struct device_node *mode_np = NULL;
	struct platform_device *pdev = NULL;

	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np))
		return -EINVAL;

	mtk_lcm_dts_read_u32(np, "lcm-params-dsi-density",
			&params->density);
	mtk_lcm_dts_read_u32(np, "lcm-params-dsi-phy_type",
			&params->phy_type);
	mtk_lcm_dts_read_u32(np, "lcm-params-dsi-lanes",
			&params->lanes);
	mtk_lcm_dts_read_u32(np, "lcm-params-dsi-format",
			&params->format);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-need_fake_resolution",
			&params->need_fake_resolution);
	if (params->need_fake_resolution != 0)
		mtk_lcm_dts_read_u32_array(np,
				"lcm-params-dsi-fake_resolution",
				(u32 *)&params->fake_resolution[0], 0, 2);

	len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-mode_flags",
			&flag[0], 0, 32);
	params->mode_flags = 0UL;
	if (len > 0 && len <= 32) {
		for (i = 0; i < len; i++)
			params->mode_flags |= (unsigned long)flag[i];
	}

	len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-mode_flags_doze_on",
			&flag[0], 0, 32);
	params->mode_flags_doze_on = 0UL;
	if (len > 0 && len <= 32) {
		for (i = 0; i < len; i++)
			params->mode_flags_doze_on |= (unsigned long)flag[i];
	}

	len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-mode_flags_doze_off",
			&flag[0], 0, 32);
	params->mode_flags_doze_off = 0UL;
	if (len > 0 && len <= 32) {
		for (i = 0; i < len; i++)
			params->mode_flags_doze_off |= (unsigned long)flag[i];
	}

	pdev = of_find_device_by_node(np);
	if (IS_ERR_OR_NULL(pdev))
		DDPPR_ERR("%s, %d, failed to get dsi pdev\n",
			__func__, __LINE__);
	else
		memcpy(&params->lcm_gpio_dev, &pdev->dev,
				sizeof(struct device));
{
	struct device_node *temp_node = of_get_parent(np);

	if (IS_ERR_OR_NULL(temp_node))
		DDPPR_ERR("%s, %d, failed to get parent node\n", __func__, __LINE__);
	else {
		pdev = of_find_device_by_node(temp_node);
		if (IS_ERR_OR_NULL(pdev))
			DDPPR_ERR("%s, %d, failed to get parent pdev\n",
				__func__, __LINE__);
		else
			memcpy(&params->lcm_gpio_dev, &pdev->dev,
				sizeof(struct device));
	}
}

	len = of_property_count_strings(np, "pinctrl-names");
	if (len > 0) {
		LCM_KZALLOC(params->lcm_pinctrl_name, (len + 1) *
				MTK_LCM_NAME_LENGTH, GFP_KERNEL);
		if (IS_ERR_OR_NULL(params->lcm_pinctrl_name)) {
			DDPPR_ERR("%s, %d, failed to allocate lcm_pinctrl_names\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		memset(params->lcm_pinctrl_name, 0, len * MTK_LCM_NAME_LENGTH);
		len = of_property_read_string_array(np, "pinctrl-names",
				params->lcm_pinctrl_name, len);
		if (len < 0) {
			DDPPR_ERR("%s, %d, failed to get pinctrl-names\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		params->lcm_pinctrl_count = len;
	} else if (len == 0)
		DDPPR_ERR("%s, %d, lcm_pinctrl_names is empty, %d\n",
			__func__, __LINE__, len);
	else
		DDPPR_ERR("%s, %d, failed to get lcm_pinctrl_names, %d\n",
			__func__, __LINE__, len);

	mtk_lcm_dts_read_u32(np, "lcm-params-dsi-default_mode",
			(u32 *) (&params->default_mode));
	len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-mode_list",
			(u32 *) (&params->mode_list[0]),
			0, MTK_DSI_FPS_MODE_COUNT);
	if (len > 0 && len <= MTK_DSI_FPS_MODE_COUNT) {
		for (i = 0; i < len; i++) {
			if (params->mode_list[i] == 0)
				break;
			snprintf(mode_node, sizeof(mode_node),
				 "mediatek,lcm-dsi-fps-%u", params->mode_list[i]);
			for_each_available_child_of_node(np, mode_np) {
				if (of_device_is_compatible(mode_np, mode_node)) {
					DDPMSG("parsing LCM fps mode: %s\n", mode_node);
					parse_lcm_dsi_fps_setting(mode_np,
							params->mode_list[i], phy_type,
							&params->mode[i],
							&params->ext_param[i]);
				}
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(parse_lcm_params_dsi);

int parse_lcm_ops_dsi(struct device_node *np,
		struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		struct mtk_panel_cust *cust)
{
	struct device_node *mode_np = NULL;
	char mode_node[128] = {0};
	int i = 0, len = 0;

	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np) || IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return -EINVAL;
	}
	memset(ops, 0, sizeof(struct mtk_lcm_ops_dsi));

	mtk_lcm_dts_read_u32(np, "prepare_size",
				&ops->prepare_size);
	if (ops->prepare_size > 0) {
		LCM_KZALLOC(ops->prepare, sizeof(struct mtk_lcm_ops_data) *
				ops->prepare_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->prepare)) {
			DDPMSG("%s,%d: failed to allocate table\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->prepare_size = parse_lcm_ops_func(np,
					ops->prepare, "prepare_table",
					ops->prepare_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "unprepare_size",
				&ops->unprepare_size);
	if (ops->unprepare_size > 0) {
		LCM_KZALLOC(ops->unprepare, sizeof(struct mtk_lcm_ops_data) *
				ops->unprepare_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->unprepare)) {
			DDPMSG("%s,%d: failed to allocate table\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->unprepare_size = parse_lcm_ops_func(np,
					ops->unprepare, "unprepare_table",
					ops->unprepare_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	mtk_lcm_dts_read_u32(np, "compare_id_size",
				&ops->compare_id_size);
	if (ops->compare_id_size > 0) {
		mtk_lcm_dts_read_u32(np, "compare_id_value_length",
				&ops->compare_id_value_length);

		if (ops->compare_id_value_length > 0 &&
		    ops->compare_id_value_length <= MTK_PANEL_COMPARE_ID_LENGTH) {
			LCM_KZALLOC(ops->compare_id_value_data,
				ops->compare_id_value_length, GFP_KERNEL);
			if (IS_ERR_OR_NULL(ops->compare_id_value_data)) {
				DDPPR_ERR("%s,%d: failed to allocate compare id data\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
			len = mtk_lcm_dts_read_u8_array(np,
					"compare_id_value_data",
					&ops->compare_id_value_data[0], 0,
					ops->compare_id_value_length);
			if (len != ops->compare_id_value_length) {
				DDPPR_ERR("%s,%d: warn parse compare id data, len:%d, expect:%u\n",
					__func__, __LINE__, len, ops->compare_id_value_length);
			}

			LCM_KZALLOC(ops->compare_id, sizeof(struct mtk_lcm_ops_data) *
					ops->compare_id_size, GFP_KERNEL);
			if (IS_ERR_OR_NULL(ops->compare_id)) {
				DDPMSG("%s,%d: failed to allocate table\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
			ops->compare_id_size = parse_lcm_ops_func(np,
						ops->compare_id, "compare_id_table",
						ops->compare_id_size,
						MTK_LCM_FUNC_DSI,  cust,
					MTK_LCM_PHASE_KERNEL);
		}
	}
#endif

	mtk_lcm_dts_read_u32(np, "set_backlight_cmdq_size",
				&ops->set_backlight_cmdq_size);
	if (ops->set_backlight_cmdq_size > 0) {
		LCM_KZALLOC(ops->set_backlight_cmdq, sizeof(struct mtk_lcm_ops_data) *
				ops->set_backlight_cmdq_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->set_backlight_cmdq)) {
			DDPMSG("%s,%d: failed to allocate table\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->set_backlight_cmdq_size = parse_lcm_ops_func(np,
					ops->set_backlight_cmdq,
					"set_backlight_cmdq_table",
					ops->set_backlight_cmdq_size,
					MTK_LCM_FUNC_DSI,  cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "set_backlight_grp_cmdq_size",
				&ops->set_backlight_grp_cmdq_size);
	if (ops->set_backlight_grp_cmdq_size > 0) {
		LCM_KZALLOC(ops->set_backlight_grp_cmdq, sizeof(struct mtk_lcm_ops_data) *
				ops->set_backlight_grp_cmdq_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->set_backlight_grp_cmdq)) {
			DDPMSG("%s,%d: failed to allocate table\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->set_backlight_grp_cmdq_size = parse_lcm_ops_func(np,
					ops->set_backlight_grp_cmdq,
					"set_backlight_grp_cmdq_table",
					ops->set_backlight_grp_cmdq_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "ata_check_size",
				&ops->ata_check_size);
	if (ops->ata_check_size > 0) {
		mtk_lcm_dts_read_u32(np, "ata_id_value_length",
				&ops->ata_id_value_length);
		if (ops->ata_id_value_length > 0 &&
		    ops->ata_id_value_length <= MTK_PANEL_ATA_ID_LENGTH) {
			LCM_KZALLOC(ops->ata_id_value_data,
					ops->ata_id_value_length, GFP_KERNEL);
			if (IS_ERR_OR_NULL(ops->ata_id_value_data)) {
				DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
			len = mtk_lcm_dts_read_u8_array(np,
					"ata_id_value_data",
					&ops->ata_id_value_data[0], 0,
					ops->ata_id_value_length);
			if (len != ops->ata_id_value_length) {
				DDPPR_ERR("%s,%d: failed to parse ata id data, len:%d, expect:%u\n",
					__func__, __LINE__, len,
					ops->ata_id_value_length);
			}

			LCM_KZALLOC(ops->ata_check, sizeof(struct mtk_lcm_ops_data) *
					ops->ata_check_size, GFP_KERNEL);
			if (IS_ERR_OR_NULL(ops->ata_check)) {
				DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
					__func__, __LINE__);
				return -ENOMEM;
			}
			ops->ata_check_size = parse_lcm_ops_func(np,
						ops->ata_check, "ata_check_table",
						ops->ata_check_size,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
		}
	}

	mtk_lcm_dts_read_u32(np, "set_aod_light_high_size",
				&ops->set_aod_light_high_size);
	if (ops->set_aod_light_high_size > 0) {
		LCM_KZALLOC(ops->set_aod_light_high, sizeof(struct mtk_lcm_ops_data) *
				ops->set_aod_light_high_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->set_aod_light_high)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->set_aod_light_high_size = parse_lcm_ops_func(np,
					ops->set_aod_light_high,
					"set_aod_light_high",
					ops->set_aod_light_high_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "set_aod_light_low_size",
				&ops->set_aod_light_low_size);
	if (ops->set_aod_light_low_size > 0) {
		LCM_KZALLOC(ops->set_aod_light_low, sizeof(struct mtk_lcm_ops_data) *
				ops->set_aod_light_low_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->set_aod_light_low)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->set_aod_light_low_size = parse_lcm_ops_func(np,
					ops->set_aod_light_low,
					"set_aod_light_low",
					ops->set_aod_light_low_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "doze_enable_size",
				&ops->doze_enable_size);
	if (ops->doze_enable_size > 0) {
		LCM_KZALLOC(ops->doze_enable, sizeof(struct mtk_lcm_ops_data) *
				ops->doze_enable_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->doze_enable)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->doze_enable_size = parse_lcm_ops_func(np,
					ops->doze_enable,
					"doze_enable_table",
					ops->doze_enable_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "doze_disable_size",
				&ops->doze_disable_size);
	if (ops->doze_disable_size > 0) {
		LCM_KZALLOC(ops->doze_disable, sizeof(struct mtk_lcm_ops_data) *
				ops->doze_disable_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->doze_disable)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->doze_disable_size = parse_lcm_ops_func(np,
					ops->doze_disable,
					"doze_disable_table",
					ops->doze_disable_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "doze_enable_start_size",
				&ops->doze_enable_start_size);
	if (ops->doze_enable_start_size > 0) {
		LCM_KZALLOC(ops->doze_enable_start, sizeof(struct mtk_lcm_ops_data) *
				ops->doze_enable_start_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->doze_enable_start)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->doze_enable_start_size = parse_lcm_ops_func(np,
					ops->doze_enable_start,
					"doze_enable_start_table",
					ops->doze_enable_start_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "doze_area_size",
				&ops->doze_area_size);
	if (ops->doze_area_size > 0) {
		LCM_KZALLOC(ops->doze_area, sizeof(struct mtk_lcm_ops_data) *
				ops->doze_area_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->doze_area)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->doze_area_size = parse_lcm_ops_func(np,
					ops->doze_area, "doze_area_table",
					ops->doze_area_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "doze_post_disp_on_size",
				&ops->doze_post_disp_on_size);
	if (ops->doze_post_disp_on_size > 0) {
		LCM_KZALLOC(ops->doze_post_disp_on, sizeof(struct mtk_lcm_ops_data) *
				ops->doze_post_disp_on_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->doze_post_disp_on)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->doze_post_disp_on_size = parse_lcm_ops_func(np,
					ops->doze_post_disp_on,
					"doze_post_disp_on_table",
					ops->doze_post_disp_on_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	mtk_lcm_dts_read_u32(np, "hbm_set_cmdq_size",
				&ops->hbm_set_cmdq_size);
	if (ops->hbm_set_cmdq_size > 0) {
		mtk_lcm_dts_read_u32(np, "hbm_set_cmdq_switch_on",
					&ops->hbm_set_cmdq_switch_on);
		mtk_lcm_dts_read_u32(np, "hbm_set_cmdq_switch_off",
					&ops->hbm_set_cmdq_switch_off);

		LCM_KZALLOC(ops->hbm_set_cmdq, sizeof(struct mtk_lcm_ops_data) *
				ops->hbm_set_cmdq_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->hbm_set_cmdq)) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->hbm_set_cmdq_size = parse_lcm_ops_func(np,
					ops->hbm_set_cmdq, "hbm_set_cmdq_table",
					ops->hbm_set_cmdq_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	for_each_available_child_of_node(np, mode_np) {
		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-fps-switch-before-powerdown")) {
			for (i = 0; i < MTK_DSI_FPS_MODE_COUNT; i++) {
				if (params->mode_list[i] == 0)
					break;
				snprintf(mode_node, sizeof(mode_node),
					 "fps_switch_%u_size", params->mode_list[i]);
				mtk_lcm_dts_read_u32(mode_np, mode_node,
						&ops->fps_switch_bfoff_size[i]);

				if (ops->fps_switch_bfoff_size[i] > 0) {
					snprintf(mode_node, sizeof(mode_node),
						 "fps_switch_%u", params->mode_list[i]);

					LCM_KZALLOC(ops->fps_switch_bfoff[i],
						sizeof(struct mtk_lcm_ops_data) *
						ops->fps_switch_bfoff_size[i], GFP_KERNEL);
					if (IS_ERR_OR_NULL(ops->fps_switch_bfoff[i])) {
						DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
							__func__, __LINE__);
						return -ENOMEM;
					}
					DDPMSG("parsing LCM fps switch before power down ops: %s\n",
						mode_node);
					ops->fps_switch_bfoff_size[i] =
							parse_lcm_ops_func(mode_np,
							ops->fps_switch_bfoff[i], mode_node,
							ops->fps_switch_bfoff_size[i],
							MTK_LCM_FUNC_DSI, cust,
							MTK_LCM_PHASE_KERNEL);
				}
			}
		}

		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-fps-switch-after-poweron")) {
			for (i = 0; i < MTK_DSI_FPS_MODE_COUNT; i++) {
				if (params->mode_list[i] == 0)
					break;
				snprintf(mode_node, sizeof(mode_node),
					 "fps_switch_%u_size", params->mode_list[i]);
				mtk_lcm_dts_read_u32(mode_np, mode_node,
						&ops->fps_switch_afon_size[i]);

				if (ops->fps_switch_afon_size[i] > 0) {
					snprintf(mode_node, sizeof(mode_node),
						 "fps_switch_%u", params->mode_list[i]);
					LCM_KZALLOC(ops->fps_switch_afon[i],
						sizeof(struct mtk_lcm_ops_data) *
						ops->fps_switch_afon_size[i], GFP_KERNEL);
					if (IS_ERR_OR_NULL(ops->fps_switch_afon[i])) {
						DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
							__func__, __LINE__);
						return -ENOMEM;
					}
					DDPMSG("parsing LCM fps switch after power on ops: %s\n",
						mode_node);
					ops->fps_switch_afon_size[i] =
							parse_lcm_ops_func(mode_np,
							ops->fps_switch_afon[i], mode_node,
							ops->fps_switch_afon_size[i],
							MTK_LCM_FUNC_DSI, cust,
							MTK_LCM_PHASE_KERNEL);
				}
			}
		}
	}

	mtk_lcm_dts_read_u32(np, "gpio_test_size",
				&ops->gpio_test_size);
	if (ops->gpio_test_size > 0) {
		LCM_KZALLOC(ops->gpio_test, sizeof(struct mtk_lcm_ops_data) *
				ops->gpio_test_size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(ops->gpio_test)) {
			DDPMSG("%s,%d: failed to allocate table\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->gpio_test_size = parse_lcm_ops_func(np,
					ops->gpio_test, "gpio_test_table",
					ops->gpio_test_size,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
	}

	return 0;
}
EXPORT_SYMBOL(parse_lcm_ops_dsi);

/* dump dsi dsc mode settings */
static void dump_lcm_dsi_dsc_mode(struct mtk_panel_dsc_params *dsc_params, unsigned int fps)
{
	DDPDUMP("-------------  fps%u dsc_params -------------\n", fps);
	if (dsc_params->enable == 0) {
		DDPDUMP("%s: dsc off\n", __func__);
		return;
	}

	DDPDUMP("enable:%u, ver:%u, slice:%u, rgb_swap:%u, cfg:%u\n",
		dsc_params->enable,
		dsc_params->ver,
		dsc_params->slice_mode,
		dsc_params->rgb_swap,
		dsc_params->dsc_cfg);
	DDPDUMP("rct_on:%u, bit_ch:%u, depth:%u, bp_enable:%u, bit_pixel:%u\n",
		dsc_params->rct_on,
		dsc_params->bit_per_channel,
		dsc_params->dsc_line_buf_depth,
		dsc_params->bp_enable,
		dsc_params->bit_per_pixel);
	DDPDUMP("pic_h=%u, pic_w=%u, slice_h=%u, slice_w=%u, size=%u\n",
		dsc_params->pic_height,
		dsc_params->pic_width,
		dsc_params->slice_height,
		dsc_params->slice_width,
		dsc_params->chunk_size);
	DDPDUMP("xmi_delay=%u, dec_delay=%u, scale=%u, inc=%u, dec=%u\n",
		dsc_params->xmit_delay,
		dsc_params->dec_delay,
		dsc_params->scale_value,
		dsc_params->increment_interval,
		dsc_params->decrement_interval);
	DDPDUMP("offset:(line=%u, nfl=%u, slice=%u, init=%u, final=%u)\n",
		dsc_params->line_bpg_offset,
		dsc_params->nfl_bpg_offset,
		dsc_params->slice_bpg_offset,
		dsc_params->initial_offset,
		dsc_params->final_offset);
	DDPDUMP("flatness:(min=%u, max=%u), rc:(size=%u, edge=%u)\n",
		dsc_params->flatness_minqp,
		dsc_params->flatness_maxqp,
		dsc_params->rc_model_size,
		dsc_params->rc_edge_factor);
	DDPDUMP("quant:(limit0=%u, limit1=%u), tgt:(hi=%u, lo=%u)\n",
		dsc_params->rc_quant_incr_limit0,
		dsc_params->rc_quant_incr_limit1,
		dsc_params->rc_tgt_offset_hi,
		dsc_params->rc_tgt_offset_lo);
}

/* dump dsi phy_timcon settings */
static void dump_lcm_dsi_phy_timcon(struct mtk_dsi_phy_timcon *phy_timcon, unsigned int fps)
{
	DDPDUMP("------------- fps%u phy_timcon -------------\n", fps);
	DDPDUMP("hs:(trial=0x%x, prpr=0x%x, zero=0x%x)\n",
		phy_timcon->hs_trail,
		phy_timcon->hs_prpr,
		phy_timcon->hs_zero);
	DDPDUMP("lpx=0x%x, ta:(get=0x%x, sure=0x%x, go=0x%x)\n",
		phy_timcon->lpx,
		phy_timcon->ta_get,
		phy_timcon->ta_sure,
		phy_timcon->ta_go);
	DDPDUMP("da_hs:(exit=0x%x, sync=0x%x), cont_det=0x%x\n",
		phy_timcon->da_hs_exit,
		phy_timcon->da_hs_sync,
		phy_timcon->cont_det);
	DDPDUMP("clk:(trail=0x%x, zero=0x%x, prpr=0x%x, exit=0x%x, post=0x%x)\n",
		phy_timcon->clk_trail,
		phy_timcon->clk_zero,
		phy_timcon->clk_hs_prpr,
		phy_timcon->clk_hs_exit,
		phy_timcon->clk_hs_post);
}

/* dump dsi dyn settings */
static void dump_lcm_dsi_dyn(struct dynamic_mipi_params *dyn, unsigned int fps)
{
	DDPDUMP("------------- fps%u dyn -------------\n", fps);
	if (dyn->switch_en == 0) {
		DDPDUMP("%s: dyn off\n", __func__);
		return;
	}
	DDPDUMP("enable=%u, pll_clk=%u, data_rate=%u\n",
		dyn->switch_en,
		dyn->pll_clk,
		dyn->data_rate);
	DDPDUMP("vertical:(vsa=%u, vbp=%u, vfp=%u, vfp_lp=%u)\n",
		dyn->vsa,
		dyn->vbp,
		dyn->vfp,
		dyn->vfp_lp_dyn);
	DDPDUMP("horizontal:(hsa=%u, hbp=%u, hfp=%u)\n",
		dyn->hsa,
		dyn->hbp,
		dyn->hfp);
	DDPDUMP("msync:(max_vfp=%u)\n",
		dyn->max_vfp_for_msync_dyn);
}

/* dump dsi dyn_fps settings */
static void dump_lcm_dsi_dyn_fps(struct dynamic_fps_params *dyn_fps, unsigned int fps)
{
	int i = 0, j = 0;

	DDPDUMP("----------------- fps%u dyn_fps -------------------\n", fps);
	if (dyn_fps->switch_en == 0) {
		DDPDUMP("%s: dyn_fps off\n", __func__);
		return;
	}

	DDPDUMP("enable:%u, vact_fps=%u, data_rate:%u\n",
		dyn_fps->switch_en,
		dyn_fps->vact_timing_fps,
		dyn_fps->data_rate);
	for (i = 0; i < MAX_DYN_CMD_NUM; i++) {
		DDPDUMP("cmd%d: fps:%u, num:%u\n",
			i, dyn_fps->dfps_cmd_table[i].src_fps,
			dyn_fps->dfps_cmd_table[i].cmd_num);
		for (j = 0; j < dyn_fps->dfps_cmd_table[i].cmd_num; j++) {
			DDPDUMP("para%d:0x%x\n", j,
				dyn_fps->dfps_cmd_table[i].para_list[j]);
		}
	}
}

/* dump dsi fps mode settings */
static void dump_lcm_dsi_fps_mode(struct drm_display_mode *mode, unsigned int fps)
{
	DDPDUMP("-------------  fps%u params -------------\n", fps);
	DDPDUMP("horizontal:(hac=%u, h_start=%u, h_end=%u, htotal=%u)\n",
	mode->hdisplay, mode->hsync_start,
	mode->hsync_end, mode->htotal);
	DDPDUMP("vertical:(vac=%u, v_start=%u, v_end=%u, vtotal=%u)\n",
	mode->vdisplay, mode->vsync_start,
	mode->vsync_end, mode->vtotal);
	DDPDUMP("clock=%d, skew=%u, vscan=%u)\n",
	mode->clock, mode->hskew, mode->vscan);
}

/* dump dsi fps ext_param */
static void dump_lcm_dsi_fps_ext_param(struct mtk_panel_params *ext_param, unsigned int fps)
{
	unsigned int i = 0, j = 0;

	DDPDUMP("--------------- fps%u ext_param ---------------\n", fps);
	DDPDUMP("pll_clk=%u, data_rate=%u, vfp_lp=%u, ssc:(dis=%u, range=%u)\n",
		ext_param->pll_clk,
		ext_param->data_rate,
		ext_param->vfp_low_power,
		ext_param->ssc_disable,
		ext_param->ssc_range);
	DDPDUMP("color_mode=%u, luminance:(min=%u, average=%u, max=%u)\n",
		ext_param->lcm_color_mode,
		ext_param->min_luminance,
		ext_param->average_luminance,
		ext_param->max_luminance);
	DDPDUMP("round_corner:(enable=%u, patt_h=(%u,%u))\n",
		ext_param->round_corner_en,
		ext_param->corner_pattern_height,
		ext_param->corner_pattern_height_bot);
	DDPDUMP("round_corner:(tp_size=(%u,%u,%u), tp_addr=(0x%x,0x%x,0x%x))\n",
		ext_param->corner_pattern_tp_size,
		ext_param->corner_pattern_tp_size_l,
		ext_param->corner_pattern_tp_size_r,
		ext_param->corner_pattern_lt_addr,
		ext_param->corner_pattern_lt_addr_l,
		ext_param->corner_pattern_lt_addr_r);
	DDPDUMP("physical:(%u,%u), lane_swap_en=%u, output_mode=%u, cmdif=0x%x\n",
		ext_param->physical_width_um,
		ext_param->physical_height_um,
		ext_param->lane_swap_en,
		ext_param->output_mode,
		ext_param->lcm_cmd_if);
	DDPDUMP("hbm:(t_en=%u, t_dis=%u), lcm_id=%u, wait_sof=%u, doze_delay=%u\n",
		ext_param->hbm_en_time,
		ext_param->hbm_dis_time,
		ext_param->lcm_index,
		ext_param->wait_sof_before_dec_vfp,
		ext_param->doze_delay);
	DDPDUMP("lfr:(enable=%u, fps=%u), msync:(enable=%u, vfp=%u)\n",
		ext_param->lfr_enable,
		ext_param->lfr_minimum_fps,
		ext_param->msync2_enable,
		ext_param->max_vfp_for_msync);
	DDPDUMP("is_cphy:%u, esd_check(cust=%u, enable=%u)\n",
		ext_param->is_cphy,
		ext_param->cust_esd_check,
		ext_param->esd_check_enable);

	for (i = 0; i < ESD_CHECK_NUM; i++) {
		DDPDUMP(">>>>esd_check_table%d:(cmd=%u, count=%u)\n", i,
			ext_param->lcm_esd_check_table[i].cmd,
			ext_param->lcm_esd_check_table[i].count);
		for (j = 0; j < ext_param->lcm_esd_check_table[i].count; j += 5)
			DDPDUMP("para%d~%d(0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", j, j + 4,
				ext_param->lcm_esd_check_table[i].para_list[j],
				ext_param->lcm_esd_check_table[i].para_list[j + 1],
				ext_param->lcm_esd_check_table[i].para_list[j + 2],
				ext_param->lcm_esd_check_table[i].para_list[j + 3],
				ext_param->lcm_esd_check_table[i].para_list[j + 4]);
		for (j = 0; j < ext_param->lcm_esd_check_table[i].count; j += 5)
			DDPDUMP("mask%d~%d(0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", j, j + 4,
				ext_param->lcm_esd_check_table[i].mask_list[j],
				ext_param->lcm_esd_check_table[i].mask_list[j + 1],
				ext_param->lcm_esd_check_table[i].mask_list[j + 2],
				ext_param->lcm_esd_check_table[i].mask_list[j + 3],
				ext_param->lcm_esd_check_table[i].mask_list[j + 4]);
	}
	/* lane swap */
	if (ext_param->lane_swap_en != 0) {
		for (i = 0; i < MIPITX_PHY_PORT_NUM; i++) {
			DDPDUMP("lane_swap0~5(%u, %u, %u, %u, %u,%u)\n",
				ext_param->lane_swap[i][MIPITX_PHY_LANE_0],
				ext_param->lane_swap[i][MIPITX_PHY_LANE_1],
				ext_param->lane_swap[i][MIPITX_PHY_LANE_2],
				ext_param->lane_swap[i][MIPITX_PHY_LANE_3],
				ext_param->lane_swap[i][MIPITX_PHY_LANE_CK],
				ext_param->lane_swap[i][MIPITX_PHY_LANE_RX]);
		}
	}
}

/* dump dsi fps settings*/
void dump_lcm_dsi_fps_settings(struct mtk_lcm_params_dsi *params, int id)
{
	unsigned int fps = 0;

	if (id < 0 || id >= MTK_DSI_FPS_MODE_COUNT) {
		DDPPR_ERR("%s, %d, invalid mode index:%d\n",
			__func__, __LINE__, id);
		return;
	}

	if (IS_ERR_OR_NULL(params)) {
		DDPPR_ERR("%s, %d, invalid params\n",
			__func__, __LINE__);
		return;
	}

	fps = drm_mode_vrefresh(&params->mode[id]);
	if (fps == 0)
		return;

	DDPDUMP("---------------- mode:%u - fps:%u-------------------\n",
		id, fps);
	dump_lcm_dsi_fps_mode(&params->mode[id], fps);
	dump_lcm_dsi_fps_ext_param(&params->ext_param[id], fps);
	dump_lcm_dsi_dsc_mode(&params->ext_param[id].dsc_params, fps);
	dump_lcm_dsi_phy_timcon(&params->ext_param[id].phy_timcon, fps);
	dump_lcm_dsi_dyn(&params->ext_param[id].dyn, fps);
	dump_lcm_dsi_dyn_fps(&params->ext_param[id].dyn_fps, fps);
}
EXPORT_SYMBOL(dump_lcm_dsi_fps_settings);

/* dump dsi settings*/
void dump_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
	struct mtk_panel_cust *cust)
{
	int i = 0;

	if (IS_ERR_OR_NULL(params)) {
		DDPPR_ERR("%s, %d: invalid params\n", __func__, __LINE__);
		return;
	}
	DDPDUMP("=========== LCM DSI DUMP ==============\n");
	DDPDUMP("phy:%u, lanes:%u, density:%u, format:%u\n",
		params->phy_type, params->lanes,
		params->density, params->format);
	DDPDUMP("default_flag:0x%lx, doze_on_flag:0x%lx, doze_off_flag:0x%lx\n",
		params->mode_flags, params->mode_flags_doze_on,
		params->mode_flags_doze_off);

	for (i = 0; i < params->lcm_pinctrl_count; i++)
		DDPDUMP(" pinctrl%d: %s\n", i, params->lcm_pinctrl_name[i]);

	for (i = 0; i < MTK_DSI_FPS_MODE_COUNT; i++)
		dump_lcm_dsi_fps_settings(params, i);
	DDPDUMP("=============================================\n");

	if (IS_ERR_OR_NULL(cust) ||
	    atomic_read(&cust->cust_enabled) == 0 ||
	    IS_ERR_OR_NULL(cust->dump_params))
		return;

	DDPDUMP("=========== LCM CUSTOMIZATION DUMP ==============\n");
	cust->dump_params();
	DDPDUMP("=============================================\n");
}
EXPORT_SYMBOL(dump_lcm_params_dsi);

void dump_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		struct mtk_panel_cust *cust)
{
	char mode_node[128] = {0};
	int i = 0;

	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, %d: invalid params\n", __func__, __LINE__);
		return;
	}

	DDPDUMP("=========== LCM DUMP of DSI ops:0x%lx-0x%lx ==============\n",
		(unsigned long)ops, (unsigned long)params);
	dump_lcm_ops_func(ops->prepare,
		ops->prepare_size, cust, "prepare");
	dump_lcm_ops_func(ops->unprepare,
		ops->unprepare_size, cust, "unprepare");
	dump_lcm_ops_func(ops->enable,
		ops->enable_size, cust, "enable");
	dump_lcm_ops_func(ops->disable,
		ops->disable_size, cust, "disable");

	dump_lcm_ops_func(ops->set_backlight_cmdq,
		ops->set_backlight_cmdq_size,
		cust, "set_backlight_cmdq");
	dump_lcm_ops_func(ops->set_backlight_grp_cmdq,
		ops->set_backlight_grp_cmdq_size,
		cust, "set_backlight_grp_cmdq");

	DDPDUMP("ata_id_value_length=%u\n",
		ops->ata_id_value_length);
	DDPDUMP("ata_id_value_data:");
	for (i = 0; i < ops->ata_id_value_length; i += 4)
		DDPDUMP("[ata_id_value_data+%u]>>> 0x%x, 0x%x, 0x%x, 0x%x\n",
			i, ops->ata_id_value_data[i],
			ops->ata_id_value_data[i + 1],
			ops->ata_id_value_data[i + 2],
			ops->ata_id_value_data[i + 3]);
	dump_lcm_ops_func(ops->ata_check,
		ops->ata_check_size, cust, "ata_check");

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	DDPDUMP("compare_id_value_length=%u\n",
		ops->compare_id_value_length);
	DDPDUMP("compare_id_value_data=",
	for (i = 0; i < ops->compare_id_value_length; i++)
		DDPDUMP("%u,", ops->compare_id_value_data[i]);
	DDPDUMP("\n");
	dump_lcm_ops_func(ops->compare_id,
		ops->compare_id_size, cust, "compare_id");
#endif

	dump_lcm_ops_func(ops->doze_enable_start,
		ops->doze_enable_start_size, cust, "doze_enable_start");
	dump_lcm_ops_func(ops->doze_enable,
		ops->doze_enable_size, cust, "doze_enable");
	dump_lcm_ops_func(ops->doze_disable,
		ops->doze_disable_size, cust, "doze_disable");
	dump_lcm_ops_func(ops->doze_area,
		ops->doze_area_size, cust, "doze_area");
	dump_lcm_ops_func(ops->doze_post_disp_on,
		ops->doze_post_disp_on_size, cust, "doze_post_disp_on");

	DDPDUMP("hbm_set_cmdq_switch: on=0x%x,off=0x%x\n",
		ops->hbm_set_cmdq_switch_on,
		ops->hbm_set_cmdq_switch_off);
	dump_lcm_ops_func(ops->hbm_set_cmdq,
		ops->hbm_set_cmdq_size, cust, "hbm_set_cmdq");

	if (params != NULL) {
		for (i = 0; i < MTK_DSI_FPS_MODE_COUNT; i++) {
			if (params->mode_list[i] == 0)
				break;

			snprintf(mode_node, sizeof(mode_node),
				 "fps_switch_bfoff_%uhz",
				 params->mode_list[i]);
			dump_lcm_ops_func(ops->fps_switch_bfoff[i],
				ops->fps_switch_bfoff_size[i],
				cust, mode_node);

			snprintf(mode_node, sizeof(mode_node),
				 "fps_switch_afon_%uhz",
				 params->mode_list[i]);
			dump_lcm_ops_func(ops->fps_switch_afon[i],
				ops->fps_switch_afon_size[i],
				cust, mode_node);
		}
	}

	dump_lcm_ops_func(ops->gpio_test,
			ops->gpio_test_size,
			cust, "gpio_test");

	DDPDUMP("=============================================\n");
}
EXPORT_SYMBOL(dump_lcm_ops_dsi);

void free_lcm_params_dsi(struct mtk_lcm_params_dsi *params)
{
	if (IS_ERR_OR_NULL(params)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return;
	}

	DDPMSG("%s: LCM free dsi params:0x%lx\n",
		__func__, (unsigned long)params);
}
EXPORT_SYMBOL(free_lcm_params_dsi);

void free_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops)
{
	unsigned int i = 0;

	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return;
	}

	DDPMSG("%s: LCM free dsi ops:0x%lx\n",
		__func__, (unsigned long)ops);

	if (ops->prepare_size > 0 && ops->prepare != NULL) {
		kfree(ops->prepare);
		ops->prepare = NULL;
		ops->prepare_size = 0;
	}

	if (ops->unprepare_size > 0 && ops->unprepare != NULL) {
		kfree(ops->unprepare);
		ops->unprepare = NULL;
		ops->unprepare_size = 0;
	}

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	if (ops->compare_id_value_length > 0 &&
	    ops->compare_id_value_data != NULL) {
		kfree(ops->compare_id_value_data);
		ops->compare_id_value_data = NULL;
		ops->compare_id_value_length = 0;
	}

	if (ops->compare_id_size > 0 &&
	    ops->compare_id != NULL) {
		kfree(ops->compare_id);
		ops->compare_id = NULL;
		ops->compare_id_size = 0;
	}
#endif

	if (ops->set_backlight_cmdq_size > 0 &&
	    ops->set_backlight_cmdq != NULL) {
		kfree(ops->set_backlight_cmdq);
		ops->set_backlight_cmdq = NULL;
		ops->set_backlight_cmdq_size = 0;
	}
	if (ops->set_backlight_grp_cmdq_size > 0 &&
	    ops->set_backlight_grp_cmdq != NULL) {
		kfree(ops->set_backlight_grp_cmdq);
		ops->set_backlight_grp_cmdq = NULL;
		ops->set_backlight_grp_cmdq_size = 0;
	}
	if (ops->ata_id_value_length > 0 &&
	    ops->ata_id_value_data != NULL) {
		kfree(ops->ata_id_value_data);
		ops->ata_id_value_data = NULL;
		ops->ata_id_value_length = 0;
	}
	if (ops->ata_check_size > 0 &&
	    ops->ata_check != NULL) {
		kfree(ops->ata_check);
		ops->ata_check = NULL;
		ops->ata_check_size = 0;
	}
	if (ops->set_aod_light_high_size > 0 &&
	    ops->set_aod_light_high != NULL) {
		kfree(ops->set_aod_light_high);
		ops->set_aod_light_high = NULL;
		ops->set_aod_light_high_size = 0;
	}
	if (ops->set_aod_light_low_size > 0 &&
	    ops->set_aod_light_low != NULL) {
		kfree(ops->set_aod_light_low);
		ops->set_aod_light_low = NULL;
		ops->set_aod_light_low_size = 0;
	}
	if (ops->doze_enable_size > 0 &&
	    ops->doze_enable != NULL) {
		kfree(ops->doze_enable);
		ops->doze_enable = NULL;
		ops->doze_enable_size = 0;
	}
	if (ops->doze_disable_size > 0 &&
	    ops->doze_disable != NULL) {
		kfree(ops->doze_disable);
		ops->doze_disable = NULL;
		ops->doze_disable_size = 0;
	}
	if (ops->doze_enable_start_size > 0 &&
	    ops->doze_enable_start != NULL) {
		kfree(ops->doze_enable_start);
		ops->doze_enable_start = NULL;
		ops->doze_enable_start_size = 0;
	}
	if (ops->doze_area_size > 0 &&
	    ops->doze_area != NULL) {
		kfree(ops->doze_area);
		ops->doze_area = NULL;
		ops->doze_area_size = 0;
	}
	if (ops->doze_post_disp_on_size > 0 &&
	    ops->doze_post_disp_on != NULL) {
		kfree(ops->doze_post_disp_on);
		ops->doze_post_disp_on = NULL;
		ops->doze_post_disp_on_size = 0;
	}
	if (ops->hbm_set_cmdq_size > 0 &&
	    ops->hbm_set_cmdq != NULL) {
		kfree(ops->hbm_set_cmdq);
		ops->hbm_set_cmdq = NULL;
		ops->hbm_set_cmdq_size = 0;
	}
	for (i = 0; i < MTK_DSI_FPS_MODE_COUNT; i++) {
		if (ops->fps_switch_bfoff_size[i] > 0 &&
		    ops->fps_switch_bfoff[i] != NULL) {
			kfree(ops->fps_switch_bfoff[i]);
			ops->fps_switch_bfoff[i] = NULL;
			ops->fps_switch_bfoff_size[i] = 0;
		}
	}
	for (i = 0; i < MTK_DSI_FPS_MODE_COUNT; i++) {
		if (ops->fps_switch_afon_size[i] > 0 &&
		    ops->fps_switch_afon[i] != NULL) {
			kfree(ops->fps_switch_afon[i]);
			ops->fps_switch_afon[i] = NULL;
			ops->fps_switch_afon_size[i] = 0;
		}
	}
	if (ops->gpio_test_size > 0 &&
	    ops->gpio_test != NULL) {
		kfree(ops->gpio_test);
		ops->gpio_test = NULL;
		ops->gpio_test_size = 0;
	}

	kfree(ops);
	ops = NULL;
}
EXPORT_SYMBOL(free_lcm_ops_dsi);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dsi helper");
MODULE_LICENSE("GPL v2");

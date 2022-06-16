// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/sched.h>
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
	u8 temp[sizeof(struct dfps_switch_cmd)] = {0};
	int i = 0, j = 0, len = 0, ret = 0;
	char node[128] = { 0 };

	if (IS_ERR_OR_NULL(dyn_fps))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_fps_switch_en",
			&dyn_fps->switch_en);

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_fps_vact_timing_fps",
			&dyn_fps->vact_timing_fps);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-dyn_fps_data_rate",
			&dyn_fps->data_rate);
	for (i = 0; i < MAX_DYN_CMD_NUM; i++) {
		ret = snprintf(node, sizeof(node),
			 "lcm-params-dsi-dyn_fps_dfps_cmd_table%u",
			 (unsigned int)i);
		if (ret < 0 || (size_t)ret >= sizeof(node))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

		len = mtk_lcm_dts_read_u8_array(np, node, &temp[0], 0,
					sizeof(struct dfps_switch_cmd));
		if (len < 0 || len > sizeof(struct dfps_switch_cmd)) {
			DDPMSG("%s, %d: the %d dyn fps of invalid cmd:%d\n",
				__func__, __LINE__, i, len);
			continue;
		} else if (len == 0)
			continue;

		dyn_fps->dfps_cmd_table[i].src_fps = temp[0];
		dyn_fps->dfps_cmd_table[i].cmd_num = temp[1];
		if (dyn_fps->dfps_cmd_table[i].cmd_num == 0)
			continue;
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
	const char *pattern;
	u8 temp[RT_MAX_NUM * 2 + 2] = {0};
	unsigned int i = 0, j = 0;
	int len = 0, ret = 0;

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
			"lcm-params-dsi-ssc_enable",
			&ext_param->ssc_enable);
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

	if (ext_param->round_corner_en == 1) {
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

		ret = of_property_read_string(np, "lcm-params-dsi-corner_pattern_name",
				&pattern);
		if (ret < 0 || strlen(pattern) == 0) {
			DDPMSG("%s,%d: invalid pattern, ret:%d\n",
				__func__, __LINE__, ret);
			return;
		}

		DDPMSG("%s, %d, rc pattern:%s\n", __func__, __LINE__, pattern);
		ext_param->corner_pattern_lt_addr =
			mtk_lcm_get_rc_addr(pattern,
				RC_LEFT_TOP, &ext_param->corner_pattern_tp_size);

		ext_param->corner_pattern_lt_addr_l =
			mtk_lcm_get_rc_addr(pattern,
				RC_LEFT_TOP_LEFT, &ext_param->corner_pattern_tp_size_l);

		ext_param->corner_pattern_lt_addr_r =
			mtk_lcm_get_rc_addr(pattern,
				RC_LEFT_TOP_RIGHT, &ext_param->corner_pattern_tp_size_r);

	}

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
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-msync2_enable",
			&ext_param->msync2_enable);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-max_vfp_for_msync",
			&ext_param->max_vfp_for_msync);

	/* esd check */
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cust_esd_check",
			&ext_param->cust_esd_check);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-esd_check_enable",
			&ext_param->esd_check_enable);
	if (ext_param->esd_check_enable != 0) {
		for (i = 0; i < ESD_CHECK_NUM; i++) {
			ret = snprintf(prop, sizeof(prop),
				 "lcm-params-dsi-lcm_esd_check_table%u", i);
			if (ret < 0 || (size_t)ret >= sizeof(prop))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

			len = mtk_lcm_dts_read_u8_array(np,
					prop, temp, 0, sizeof(struct esd_check_item));
			if (len < 0 || len > sizeof(struct esd_check_item)) {
				DDPMSG("%s, %d: the %d esd table of invalid cmd:%d\n",
					__func__, __LINE__, i, len);
				continue;
			} else if (len == 0)
				continue;

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
			ret = snprintf(prop, sizeof(prop),
				 "lcm-params-dsi-lane_swap%u", i);
			if (ret < 0 || (size_t)ret >= sizeof(prop))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

			mtk_lcm_dts_read_u32_array(np, prop,
					(u32 *)&ext_param->lane_swap[i][0],
					0, MIPITX_PHY_LANE_NUM);
		}
	}
}

static void parse_lcm_dsi_fpga_settings(struct device_node *np,
	struct mtk_lcm_mode_dsi *mode_node)
{
#ifdef CONFIG_FPGA_EARLY_PORTING
	/* currently not used in kernel, but applied at LK parameters,
	 * just keep for potential development
	 * we can replace the normal settings with FPGA settings
	 */
#endif
}

#define LCM_DSI_MSYNC_FPS_DTS_COUNT (2)
static int parse_lcm_dsi_msync_min_fps_list(struct device_node *np,
	struct mtk_lcm_mode_dsi *mode_node)
{
	struct mtk_lcm_msync_min_fps_switch *node = NULL;
	u8 *table_dts_buf = NULL, *tmp = NULL;
	unsigned int table_dts_size = 512, tmp_len = 0;
	int ret = 0, len = 0;

	INIT_LIST_HEAD(&mode_node->msync_min_fps_switch);
	mode_node->msync_min_fps_count = 0;

	LCM_KZALLOC(table_dts_buf, table_dts_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(table_dts_buf)) {
		DDPPR_ERR("%s, %d, failed to allocate dts buffer\n", __func__, __LINE__);
		return -ENOMEM;
	}

	len = mtk_lcm_dts_read_u8_array(np, "lcm-params-dsi-msync_min_fps_list",
				table_dts_buf, 0, table_dts_size - 1);
	if (len == 0) {
		ret = 0;
		goto end;
	} else if (len < 0) {
		DDPINFO("%s, failed to get msync min fps list dts, len:%d\n",
			__func__, len);
		ret = 0;
		goto end;
	} else if ((unsigned int)len < table_dts_size) {
		table_dts_buf[len] = '\0';
		DDPINFO("%s: start to parse msync min fps list, dts_len:%u\n",
			__func__, len);
	} else {
		table_dts_buf[table_dts_size - 1] = '\0';
		DDPMSG("%s: start to parse msync min fps list, len:%u has out of size:%u\n",
			__func__, len, table_dts_size);
		len = table_dts_size;
	}

	tmp = table_dts_buf;
	while (len > LCM_DSI_MSYNC_FPS_DTS_COUNT) {
		LCM_KZALLOC(node,
			sizeof(struct mtk_lcm_msync_min_fps_switch),
			GFP_KERNEL);
		if (IS_ERR_OR_NULL(node)) {
			DDPPR_ERR("%s, %d, failed to allocate msync min fps node\n",
				__func__, __LINE__);
			ret = -ENOMEM;
			goto end;
		}

		node->fps = tmp[0];
		node->count = tmp[1];
		tmp_len = LCM_DSI_MSYNC_FPS_DTS_COUNT + node->count;
		if (len < tmp_len) {
			DDPPR_ERR("%s, %d, invalid fps data count:%u, total:%u\n",
				__func__, __LINE__, tmp_len, len);
			LCM_KFREE(node, sizeof(struct mtk_lcm_msync_min_fps_switch));
			ret = -ENOMEM;
			goto end;
		}

		LCM_KZALLOC(node->data, node->count, GFP_KERNEL);
		if (IS_ERR_OR_NULL(node->data)) {
			DDPPR_ERR("%s, %d, failed to allocate msync min fps data\n",
				__func__, __LINE__);
			LCM_KFREE(node, sizeof(struct mtk_lcm_msync_min_fps_switch));
			ret = -ENOMEM;
			goto end;
		}
		memcpy(node->data, &tmp[LCM_DSI_MSYNC_FPS_DTS_COUNT], node->count);

		list_add_tail(&node->list, &mode_node->msync_min_fps_switch);
		mode_node->msync_min_fps_count++;

		if (tmp_len <= len) {
			tmp = tmp + tmp_len;
			len = len - tmp_len;
		} else {
			DDPMSG("%s: parsing warning of msync min fps list, len:%d, tmp:%d\n",
				__func__, len, tmp_len);
			break;
		}
	}

end:
	LCM_KFREE(table_dts_buf, table_dts_size);
	if (mode_node->msync_min_fps_count > 0 ||
	    ret < 0)
		DDPINFO("%s, %d, count:%u, ret:%d\n", __func__, __LINE__,
			mode_node->msync_min_fps_count, ret);

	return ret;
}

/* parse dsi msync cmd table: mtk_panel_params */
#define MTK_LCM_MSYNC_MAX_LEVEL (20)
static void parse_lcm_dsi_msync_cmd_table(struct device_node *np,
			struct mtk_panel_params *ext_param)
{
	unsigned int i = 0, multi_te_tb_count = 0;
	int len = 0;
	unsigned int *level_id = NULL;
	unsigned int *level_fps = NULL;
	unsigned int *max_fps = NULL;
	unsigned int *min_fps = NULL;

	if (IS_ERR_OR_NULL(ext_param) || IS_ERR_OR_NULL(np))
		return;

	if (ext_param->msync2_enable == 0)
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-te_type",
			&ext_param->msync_cmd_table.te_type);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-msync_max_fps",
			&ext_param->msync_cmd_table.msync_max_fps);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-msync_min_fps",
			&ext_param->msync_cmd_table.msync_min_fps);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-msync_level_num",
			&ext_param->msync_cmd_table.msync_level_num);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-multi_te_tb_count",
			&multi_te_tb_count);

	if (multi_te_tb_count > 0) {
		struct msync_level_table *table =
			&ext_param->msync_cmd_table.multi_te_tb.multi_te_level[0];

		multi_te_tb_count = (multi_te_tb_count < MTK_LCM_MSYNC_MAX_LEVEL) ?
				multi_te_tb_count : MTK_LCM_MSYNC_MAX_LEVEL;
		DDPINFO("%s, %d, multi_te_tb_count:%u\n",
			__func__, __LINE__, multi_te_tb_count);

		/* read level_id */
		LCM_KZALLOC(level_id, multi_te_tb_count * sizeof(unsigned int),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(level_id)) {
			DDPPR_ERR("%s, %d, failed to allocate level_id\n",
				__func__, __LINE__);
			goto end;
		}
		len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-multi_te_level_id",
			&level_id[0], 0, multi_te_tb_count);
		if (len < 0 || len > multi_te_tb_count)
			DDPPR_ERR("%s, %d, failed to get level_id\n",
				__func__, __LINE__);

		/* read level_fps */
		LCM_KZALLOC(level_fps, multi_te_tb_count * sizeof(unsigned int),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(level_fps)) {
			DDPPR_ERR("%s, %d, failed to allocate level_fps\n",
				__func__, __LINE__);
			goto end;
		}
		len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-multi_te_level_fps",
			&level_fps[0], 0, multi_te_tb_count);
		if (len < 0 || len > multi_te_tb_count)
			DDPPR_ERR("%s, %d, failed to get level_fps\n",
				__func__, __LINE__);

		/* read max_fps */
		LCM_KZALLOC(max_fps, multi_te_tb_count * sizeof(unsigned int),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(max_fps)) {
			DDPPR_ERR("%s, %d, failed to allocate max_fps\n",
				__func__, __LINE__);
			goto end;
		}
		len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-multi_te_max_fps",
			&max_fps[0], 0, multi_te_tb_count);
		if (len < 0 || len > multi_te_tb_count)
			DDPPR_ERR("%s, %d, failed to get max_fps\n",
				__func__, __LINE__);

		/* read min_fps */
		LCM_KZALLOC(min_fps, multi_te_tb_count * sizeof(unsigned int),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(min_fps)) {
			DDPPR_ERR("%s, %d, failed to allocate min_fps\n",
				__func__, __LINE__);
			goto end;
		}
		len = mtk_lcm_dts_read_u32_array(np,
			"lcm-params-dsi-multi_te_min_fps",
			&min_fps[0], 0, multi_te_tb_count);
		if (len < 0 || len > multi_te_tb_count)
			DDPPR_ERR("%s, %d, failed to get min_fps\n",
				__func__, __LINE__);

		/* save parsing result */
		for (i = 0; i < multi_te_tb_count; i++) {
			table[i].level_id = level_id[i];
			table[i].level_fps = level_fps[i];
			table[i].max_fps = max_fps[i];
			table[i].min_fps = min_fps[i];
			DDPMSG("%s, %d >>> table%u, id:%u, fps:%u, max:%u, min:%u\n",
				__func__, __LINE__, i,
				table[i].level_id, table[i].level_fps,
				table[i].max_fps, table[i].min_fps);
		}
	}

end:
	if (level_id != NULL)
		LCM_KFREE(level_id, multi_te_tb_count * sizeof(unsigned int));
	if (level_fps != NULL)
		LCM_KFREE(level_fps, multi_te_tb_count * sizeof(unsigned int));
	if (max_fps != NULL)
		LCM_KFREE(max_fps, multi_te_tb_count * sizeof(unsigned int));
	if (min_fps != NULL)
		LCM_KFREE(min_fps, multi_te_tb_count * sizeof(unsigned int));
}

static void parse_lcm_dsi_fps_setting(struct device_node *np,
	struct mtk_lcm_mode_dsi *mode_node, unsigned int phy_type)
{
	struct device_node *child_np = NULL;
	char child[128] = { 0 };
	struct drm_display_mode *mode = &mode_node->mode;
	struct mtk_panel_params *ext_param = &mode_node->ext_param;
	int ret = 0;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-voltage",
			&mode_node->voltage);

	parse_lcm_dsi_fps_mode(np, mode);
	parse_lcm_dsi_fps_ext_param(np, ext_param);
	if (mode_node->ext_param.msync2_enable != 0)
		parse_lcm_dsi_msync_min_fps_list(np, mode_node);

	if (phy_type == MTK_LCM_MIPI_CPHY)
		ext_param->is_cphy = 1;
	else
		ext_param->is_cphy = 0;

	for_each_available_child_of_node(np, child_np) {
		/* dsc params */
		ret = snprintf(child, sizeof(child) - 1,
			"mediatek,lcm-params-dsi-dsc-params");
		if (ret < 0 || (size_t)ret >= sizeof(child))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

		if (of_device_is_compatible(child_np, child)) {
			DDPINFO("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_dsc_mode(child_np,
					&ext_param->dsc_params);
		}
		/* phy timcon */
		ret = snprintf(child, sizeof(child) - 1,
			"mediatek,lcm-params-dsi-phy-timcon");
		if (ret < 0 || (size_t)ret >= sizeof(child))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

		if (of_device_is_compatible(child_np, child)) {
			DDPINFO("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_phy_timcon(child_np,
					&ext_param->phy_timcon);
		}
		/* dyn */
		ret = snprintf(child, sizeof(child) - 1,
			"mediatek,lcm-params-dsi-dyn");
		if (ret < 0 || (size_t)ret >= sizeof(child))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

		if (of_device_is_compatible(child_np, child)) {
			DDPINFO("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_dyn(child_np,
					&ext_param->dyn);
		}
		/* dyn fps */
		ret = snprintf(child, sizeof(child) - 1,
			"mediatek,lcm-params-dsi-dyn-fps");
		if (ret < 0 || (size_t)ret >= sizeof(child))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

		if (of_device_is_compatible(child_np, child)) {
			DDPINFO("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_dyn_fps(child_np,
					&ext_param->dyn_fps);
		}

		/* msync cmd table */
		if (mode_node->ext_param.msync2_enable != 0) {
			ret = snprintf(child, sizeof(child) - 1,
				"mediatek,lcm-params-dsi-msync-cmd-table");
			if (ret < 0 || (size_t)ret >= sizeof(child))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			if (of_device_is_compatible(child_np, child)) {
				DDPINFO("%s, parsing child:%s\n",
					__func__, child);
				parse_lcm_dsi_msync_cmd_table(child_np, ext_param);
			}
		}

		/* fpga settings */
		ret = snprintf(child, sizeof(child) - 1,
			"mediatek,lcm-dsi-fpga-params");
		if (ret < 0 || (size_t)ret >= sizeof(child))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
		if (of_device_is_compatible(child_np, child)) {
			DDPINFO("%s, parsing child:%s\n",
				__func__, child);
			parse_lcm_dsi_fpga_settings(child_np, mode_node);
		}
	}
}

int parse_lcm_params_dsi(struct device_node *np,
		struct mtk_lcm_params_dsi *params)
{
	int len = 0;
	unsigned int i = 0;
	unsigned int default_mode = 0;
	unsigned int flag[64] = { 0 };
	u32 *mode = NULL;
	char mode_name[128] = { 0 };
	struct device_node *mode_np = NULL;
	int ret = 0;
#if MTK_LCM_DEBUG_DUMP
	struct platform_device *pdev = NULL;
#endif

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
#if MTK_LCM_DEBUG_DUMP
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
#endif

	len = of_property_count_strings(np, "pinctrl-names");
	if (len > 0) {
		LCM_KZALLOC(params->lcm_pinctrl_name, len *
				MTK_LCM_NAME_LENGTH, GFP_KERNEL);
		if (IS_ERR_OR_NULL(params->lcm_pinctrl_name)) {
			DDPPR_ERR("%s, %d, failed to allocate lcm_pinctrl_names\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

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
			&default_mode);
	mtk_lcm_dts_read_u32(np, "lcm-params-dsi-mode_count",
			(u32 *) (&params->mode_count));

	INIT_LIST_HEAD(&params->mode_list);
	if (params->mode_count == 0) {
		DDPMSG("%s, invalid mode count:%u\n", __func__, params->mode_count);
		return -EFAULT;
	}
	LCM_KZALLOC(mode, params->mode_count *
		MTK_LCM_MODE_UNIT * sizeof(u32), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mode)) {
		DDPMSG("%s, failed to allocate mode buffer\n", __func__);
		return -ENOMEM;
	}

	len = mtk_lcm_dts_read_u32_array(np, "lcm-params-dsi-mode_list",
			mode, 0, params->mode_count * MTK_LCM_MODE_UNIT);
	if (len != params->mode_count * MTK_LCM_MODE_UNIT) {
		DDPMSG("%s: invalid dsi mode list, len:%d, count:%u",
			__func__, len, params->mode_count);
		kfree(mode);
		return -EINVAL;
	}
	for (i = 0; i < params->mode_count; i++) {
		unsigned int id = mode[i * MTK_LCM_MODE_UNIT];
		unsigned int width = mode[i * MTK_LCM_MODE_UNIT + 1];
		unsigned int height = mode[i * MTK_LCM_MODE_UNIT + 2];
		unsigned int fps = mode[i * MTK_LCM_MODE_UNIT + 3];

		ret = snprintf(mode_name, sizeof(mode_name),
			 "mediatek,lcm-dsi-fps-%u-%u-%u-%u",
			 id, width, height, fps);
		if (ret < 0 || (size_t)ret >= sizeof(mode_name))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

		for_each_available_child_of_node(np, mode_np) {
			if (of_device_is_compatible(mode_np, mode_name)) {
				struct mtk_lcm_mode_dsi *mode_node = NULL;

				LCM_KZALLOC(mode_node,
					sizeof(struct mtk_lcm_mode_dsi),
					GFP_KERNEL);
				mode_node->id = id;
				mode_node->width = width;
				mode_node->height = height;
				mode_node->fps = fps;
				list_add_tail(&mode_node->list, &params->mode_list);
				if (default_mode == id)
					params->default_mode = mode_node;

				parse_lcm_dsi_fps_setting(mode_np, mode_node,
						params->phy_type);
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
	struct mtk_lcm_mode_dsi *mode_node;
	char mode_name[128] = {0};
	int len = 0, ret = 0;

	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np) || IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return -EINVAL;
	}
	memset(ops, 0, sizeof(struct mtk_lcm_ops_dsi));

	ret = parse_lcm_ops_func(np,
				&ops->prepare, "prepare_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing prepare_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->unprepare, "unprepare_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing unprepare_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	LCM_KZALLOC(ops->compare_id_value_data,
		MTK_PANEL_COMPARE_ID_LENGTH, GFP_KERNEL);
	if (IS_ERR_OR_NULL(ops->compare_id_value_data)) {
		DDPPR_ERR("%s,%d: failed to allocate compare id data\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	len = mtk_lcm_dts_read_u8_array(np,
				"compare_id_value_data",
				&ops->compare_id_value_data[0], 0,
				MTK_PANEL_COMPARE_ID_LENGTH);
	if (len > 0 &&
	    len < MTK_PANEL_COMPARE_ID_LENGTH) {
		ops->compare_id_value_length = len;

		ret = parse_lcm_ops_func(np,
					&ops->compare_id, "compare_id_table",
					MTK_LCM_FUNC_DSI,  cust,
					MTK_LCM_PHASE_KERNEL);
		if (ret < 0) {
			DDPMSG("%s, %d failed to parsing compare_id_table, ret:%d\n",
				__func__, __LINE__, ret);
			return ret;
		}
	} else {
		DDPMSG("%s, %d, failed to get compare id,len%d\n",
			__func__, __LINE__, len);
		ops->compare_id_value_length = 0;
	}
#endif

	mtk_lcm_dts_read_u32(np, "set_backlight_mask",
				&ops->set_backlight_mask);
	ret = parse_lcm_ops_func(np,
				&ops->set_backlight_cmdq,
				"set_backlight_cmdq_table",
				MTK_LCM_FUNC_DSI,  cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing set_backlight_cmdq_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	LCM_KZALLOC(ops->ata_id_value_data,
			MTK_PANEL_ATA_ID_LENGTH, GFP_KERNEL);
	if (IS_ERR_OR_NULL(ops->ata_id_value_data)) {
		DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	len = mtk_lcm_dts_read_u8_array(np,
				"ata_id_value_data",
				&ops->ata_id_value_data[0], 0,
				MTK_PANEL_ATA_ID_LENGTH);
	if (len > 0 &&
	    len < MTK_PANEL_ATA_ID_LENGTH) {
		ops->ata_id_value_length = len;

		ret = parse_lcm_ops_func(np,
					&ops->ata_check, "ata_check_table",
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
		if (ret < 0) {
			DDPMSG("%s, %d failed to parsing ata_check_table, ret:%d\n",
				__func__, __LINE__, ret);
			return ret;
		}
	} else {
		DDPMSG("%s, %d, failed to get ata id,len%d\n",
			__func__, __LINE__, len);
		ops->ata_id_value_length = 0;
	}

	mtk_lcm_dts_read_u32(np, "set_aod_light_mask",
				&ops->set_aod_light_mask);
	ret = parse_lcm_ops_func(np,
				&ops->set_aod_light,
				"set_aod_light_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing set_aod_light, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->doze_enable,
				"doze_enable_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing doze_enable_table, ret:%d\n",
				__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->doze_disable,
				"doze_disable_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing doze_disable_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->doze_enable_start,
				"doze_enable_start_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing doze_enable_start_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->doze_area, "doze_area_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing doze_area_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->doze_post_disp_on,
				"doze_post_disp_on_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing doze_post_disp_on_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	mtk_lcm_dts_read_u32(np, "hbm_set_cmdq_switch_on",
				&ops->hbm_set_cmdq_switch_on);
	mtk_lcm_dts_read_u32(np, "hbm_set_cmdq_switch_off",
				&ops->hbm_set_cmdq_switch_off);
	ret = parse_lcm_ops_func(np,
				&ops->hbm_set_cmdq, "hbm_set_cmdq_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing hbm_set_cmdq, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->msync_set_min_fps, "msync_set_min_fps_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing msync_set_min_fps, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->msync_close_mte, "msync_close_mte_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing msync_close_mte, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->msync_default_mte, "msync_default_mte_table",
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing msync_default_mte, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	for_each_available_child_of_node(np, mode_np) {
		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-fps-switch-before-powerdown")) {
			list_for_each_entry(mode_node, &params->mode_list, list) {
				ret = snprintf(mode_name, sizeof(mode_name),
					"fps-switch-%u-%u-%u-%u_table",
					mode_node->id, mode_node->width,
					mode_node->height, mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(mode_name))
					DDPMSG("%s, %d, snprintf failed, ret:%d\n",
						__func__, __LINE__, ret);
				ret = 0;

#if MTK_LCM_DEBUG_DUMP
				DDPMSG("parsing LCM fps switch before power down ops: %s\n",
					mode_name);
#endif
				ret = parse_lcm_ops_func(mode_np,
						&mode_node->fps_switch_bfoff, mode_name,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
				if (ret < 0) {
					DDPMSG("%s, %d failed to parsing %s, ret:%d\n",
						__func__, __LINE__, mode_name, ret);
					return ret;
				}
			}
		}

		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-fps-switch-after-poweron")) {
			list_for_each_entry(mode_node, &params->mode_list, list) {
				ret = snprintf(mode_name, sizeof(mode_name),
					"fps-switch-%u-%u-%u-%u_table",
					mode_node->id, mode_node->width,
					mode_node->height, mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(mode_name))
					DDPMSG("%s, %d, snprintf failed, ret:%d\n",
						__func__, __LINE__, ret);
				ret = 0;

#if MTK_LCM_DEBUG_DUMP
				DDPMSG("parsing LCM fps switch after power on ops: %s\n",
					mode_name);
#endif
				ret = parse_lcm_ops_func(mode_np,
						&mode_node->fps_switch_afon, mode_name,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
				if (ret < 0) {
					DDPMSG("%s, %d failed to parsing %s, ret:%d\n",
						__func__, __LINE__, mode_name, ret);
					return ret;
				}
			}
		}

		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-msync-switch-mte")) {
			list_for_each_entry(mode_node, &params->mode_list, list) {
				if (mode_node->ext_param.msync2_enable == 0)
					continue;

				ret = snprintf(mode_name, sizeof(mode_name),
					"msync-switch-mte-%u-%u-%u-%u_table",
					mode_node->id, mode_node->width,
					mode_node->height, mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(mode_name))
					DDPMSG("%s, %d, snprintf failed, ret:%d\n",
						__func__, __LINE__, ret);
				ret = 0;

#if MTK_LCM_DEBUG_DUMP
				DDPMSG("parsing msync level switch ops: %s\n",
					mode_name);
#endif
				ret = parse_lcm_ops_func(mode_np,
						&mode_node->msync_switch_mte, mode_name,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
				if (ret < 0) {
					DDPMSG("%s, %d failed to parsing %s, ret:%d\n",
						__func__, __LINE__, mode_name, ret);
					return ret;
				}
			}
		}
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

	DDPDUMP("enable:%u, vact_fps=%u, data_rate:%u\n",
		dyn_fps->switch_en,
		dyn_fps->vact_timing_fps,
		dyn_fps->data_rate);
	for (i = 0; i < MAX_DYN_CMD_NUM; i++) {
		DDPDUMP("cmd%d: fps:%u, num:%u\n",
			i, dyn_fps->dfps_cmd_table[i].src_fps,
			dyn_fps->dfps_cmd_table[i].cmd_num);
		for (j = 0; j < dyn_fps->dfps_cmd_table[i].cmd_num; j++) {
			DDPDUMP("    para%d:0x%x\n", j,
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
		ext_param->ssc_enable,
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
	DDPDUMP("round_corner:(tp_size=(%u,%u,%u), tp_addr=(0x%lx,0x%lx,0x%lx))\n",
		ext_param->corner_pattern_tp_size,
		ext_param->corner_pattern_tp_size_l,
		ext_param->corner_pattern_tp_size_r,
		(unsigned long)ext_param->corner_pattern_lt_addr,
		(unsigned long)ext_param->corner_pattern_lt_addr_l,
		(unsigned long)ext_param->corner_pattern_lt_addr_r);
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
void dump_lcm_dsi_fps_settings(struct mtk_lcm_mode_dsi *mode)
{
	struct mtk_lcm_msync_min_fps_switch *node = NULL;
	struct mtk_lcm_msync_min_fps_switch *tmp = NULL;

	if (IS_ERR_OR_NULL(mode)) {
		DDPPR_ERR("%s, %d, invalid mode\n",
			__func__, __LINE__);
		return;
	}

	DDPDUMP("---------------- mode:%u (%u-%u-%u)-------------------\n",
		mode->id, mode->width, mode->height, mode->fps);
	dump_lcm_dsi_fps_mode(&mode->mode, mode->fps);
	dump_lcm_dsi_fps_ext_param(&mode->ext_param, mode->fps);
	dump_lcm_dsi_dsc_mode(&mode->ext_param.dsc_params, mode->fps);
	dump_lcm_dsi_phy_timcon(&mode->ext_param.phy_timcon, mode->fps);
	dump_lcm_dsi_dyn(&mode->ext_param.dyn, mode->fps);
	dump_lcm_dsi_dyn_fps(&mode->ext_param.dyn_fps, mode->fps);

	DDPDUMP("msync_min_fps_count:%u\n", mode->msync_min_fps_count);
	if (mode->msync_min_fps_count > 0) {
		list_for_each_entry_safe(node, tmp, &mode->msync_min_fps_switch, list) {
			DDPDUMP("msync_min_fps_list: fps:%u,count:%u,cmd:0x%x,0x%x,0x%x,0x%x\n",
				node->fps, node->count,
				IS_ERR_OR_NULL(node->data) ? 0 : *(node->data),
				IS_ERR_OR_NULL(node->data + 1) ? 0 : *(node->data + 1),
				IS_ERR_OR_NULL(node->data + 2) ? 0 : *(node->data + 2),
				IS_ERR_OR_NULL(node->data + 3) ? 0 : *(node->data + 3));
		}
	}
}
EXPORT_SYMBOL(dump_lcm_dsi_fps_settings);

/* dump dsi settings*/
void dump_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
	struct mtk_panel_cust *cust)
{
	struct mtk_lcm_mode_dsi *mode_node;
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

	if (params->mode_count > 0) {
		list_for_each_entry(mode_node, &params->mode_list, list)
			dump_lcm_dsi_fps_settings(mode_node);
	}
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
	char mode_name[128] = {0};
	struct mtk_lcm_mode_dsi *mode_node;
	int i = 0, ret = 0;

	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, %d: invalid params\n", __func__, __LINE__);
		return;
	}

	DDPDUMP("=========== LCM DUMP of DSI ops:0x%lx-0x%lx ==============\n",
		(unsigned long)ops, (unsigned long)params);
	dump_lcm_ops_func(&ops->prepare, cust, "prepare");
	dump_lcm_ops_func(&ops->unprepare, cust, "unprepare");
	dump_lcm_ops_func(&ops->enable, cust, "enable");
	dump_lcm_ops_func(&ops->disable, cust, "disable");
	dump_lcm_ops_func(&ops->set_backlight_cmdq,
		cust, "set_backlight_cmdq");

	DDPDUMP("ata_id_value_length=%u\n",
		ops->ata_id_value_length);
	DDPDUMP("ata_id_value_data:");
	for (i = 0; i < ops->ata_id_value_length; i += 4)
		DDPDUMP("[ata_id_value_data+%u]>>> 0x%x, 0x%x, 0x%x, 0x%x\n",
			i, ops->ata_id_value_data[i],
			ops->ata_id_value_data[i + 1],
			ops->ata_id_value_data[i + 2],
			ops->ata_id_value_data[i + 3]);
	dump_lcm_ops_func(&ops->ata_check, cust, "ata_check");

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	DDPDUMP("compare_id_value_length=%u\n",
		ops->compare_id_value_length);
	DDPDUMP("compare_id_value_data=",
	for (i = 0; i < ops->compare_id_value_length; i++)
		DDPDUMP("%u,", ops->compare_id_value_data[i]);
	DDPDUMP("\n");
	dump_lcm_ops_func(&ops->compare_id, cust, "compare_id");
#endif

	dump_lcm_ops_func(&ops->doze_enable_start,
		cust, "doze_enable_start");
	dump_lcm_ops_func(&ops->doze_enable, cust, "doze_enable");
	dump_lcm_ops_func(&ops->doze_disable, cust, "doze_disable");
	dump_lcm_ops_func(&ops->doze_area, cust, "doze_area");
	dump_lcm_ops_func(&ops->doze_post_disp_on,
		cust, "doze_post_disp_on");
	dump_lcm_ops_func(&ops->set_aod_light, cust, "set_aod_light");

	DDPDUMP("hbm_set_cmdq_switch: on=0x%x,off=0x%x\n",
		ops->hbm_set_cmdq_switch_on,
		ops->hbm_set_cmdq_switch_off);
	dump_lcm_ops_func(&ops->hbm_set_cmdq, cust, "hbm_set_cmdq");

	dump_lcm_ops_func(&ops->msync_set_min_fps, cust, "msync_set_min_fps");
	dump_lcm_ops_func(&ops->msync_close_mte, cust, "msync_close_mte");
	dump_lcm_ops_func(&ops->msync_default_mte, cust, "msync_default_mte");

	if (params != NULL && params->mode_count > 0) {
		list_for_each_entry(mode_node, &params->mode_list, list) {
			ret = snprintf(mode_name, sizeof(mode_name),
				 "fps_switch_bfoff_%u_%u_%u", mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			dump_lcm_ops_func(&mode_node->fps_switch_bfoff,
				cust, mode_name);

			ret = snprintf(mode_name, sizeof(mode_name),
				 "fps_switch_afon_%u_%u_%u", mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			dump_lcm_ops_func(&mode_node->fps_switch_afon,
				cust, mode_name);

			ret = snprintf(mode_name, sizeof(mode_name),
				 "msync_switch_mte_%u_%u_%u", mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			dump_lcm_ops_func(&mode_node->msync_switch_mte,
				cust, mode_name);
		}
	}

	DDPDUMP("=============================================\n");
}
EXPORT_SYMBOL(dump_lcm_ops_dsi);

void free_lcm_msync_min_fps_list(struct list_head *msync_fps_list)
{
	struct mtk_lcm_msync_min_fps_switch *node = NULL;
	struct mtk_lcm_msync_min_fps_switch *tmp = NULL;

	list_for_each_entry_safe(node, tmp, msync_fps_list, list) {
		if (node->count > 0) {
			LCM_KFREE(node->data, node->count);
			node->count = 0;
		}
		list_del(&node->list);
		LCM_KFREE(node, sizeof(struct mtk_lcm_msync_min_fps_switch));
	}
}

void free_lcm_params_dsi(struct mtk_lcm_params_dsi *params)
{
	struct mtk_lcm_mode_dsi *mode_node = NULL, *tmp = NULL;

	if (IS_ERR_OR_NULL(params) || params->mode_count == 0) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return;
	}

	list_for_each_entry_safe(mode_node, tmp, &params->mode_list, list) {
		if (mode_node->fps_switch_bfoff.size > 0) {
			free_lcm_ops_table(&mode_node->fps_switch_bfoff);
			mode_node->fps_switch_bfoff.size = 0;
		}
		if (mode_node->fps_switch_afon.size > 0) {
			free_lcm_ops_table(&mode_node->fps_switch_afon);
			mode_node->fps_switch_afon.size = 0;
		}
		if (mode_node->msync_switch_mte.size > 0) {
			free_lcm_ops_table(&mode_node->msync_switch_mte);
			mode_node->msync_switch_mte.size = 0;
		}
		if (mode_node->msync_min_fps_count > 0) {
			free_lcm_msync_min_fps_list(&mode_node->msync_min_fps_switch);
			mode_node->msync_min_fps_count = 0;
		}
		list_del(&mode_node->list);
		LCM_KFREE(mode_node, sizeof(struct mtk_lcm_mode_dsi));
	}
	params->default_mode = NULL;
	memset(params, 0, sizeof(struct mtk_lcm_params_dsi));

	DDPMSG("%s: LCM free dsi params:0x%lx\n",
		__func__, (unsigned long)params);
}
EXPORT_SYMBOL(free_lcm_params_dsi);

void free_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops)
{
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return;
	}

	DDPMSG("%s: LCM free dsi ops:0x%lx\n",
		__func__, (unsigned long)ops);

	free_lcm_ops_table(&ops->prepare);
	free_lcm_ops_table(&ops->unprepare);

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	if (ops->compare_id_value_length > 0 &&
	    ops->compare_id_value_data != NULL) {
		LCM_KFREE(ops->compare_id_value_data,
				ops->compare_id_value_length);
		ops->compare_id_value_length = 0;
	}

	free_lcm_ops_table(&ops->compare_id);
#endif

	free_lcm_ops_table(&ops->set_backlight_cmdq);
	if (ops->ata_id_value_length > 0 &&
	    ops->ata_id_value_data != NULL) {
		LCM_KFREE(ops->ata_id_value_data,
				ops->ata_id_value_length);
		ops->ata_id_value_length = 0;
	}
	free_lcm_ops_table(&ops->ata_check);
	free_lcm_ops_table(&ops->set_aod_light);
	free_lcm_ops_table(&ops->doze_enable);
	free_lcm_ops_table(&ops->doze_disable);
	free_lcm_ops_table(&ops->doze_enable_start);
	free_lcm_ops_table(&ops->doze_area);
	free_lcm_ops_table(&ops->doze_post_disp_on);
	free_lcm_ops_table(&ops->hbm_set_cmdq);
	free_lcm_ops_table(&ops->msync_set_min_fps);
	free_lcm_ops_table(&ops->msync_close_mte);
	free_lcm_ops_table(&ops->msync_default_mte);

	LCM_KFREE(ops, sizeof(struct mtk_lcm_ops_dsi));
}
EXPORT_SYMBOL(free_lcm_ops_dsi);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dsi helper");
MODULE_LICENSE("GPL v2");

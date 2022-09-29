// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include "mtk_drm_panel_helper.h"

/* parsing of struct mtk_panel_cm_params */
static void parse_lcm_dsi_cm_params(struct device_node *np,
			struct mtk_panel_cm_params *cm_params)
{
	if (IS_ERR_OR_NULL(cm_params))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_enable",
			&cm_params->enable);

	if (cm_params->enable == 0)
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_relay",
			&cm_params->relay);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c00",
			&cm_params->cm_c00);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c01",
			&cm_params->cm_c01);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c02",
			&cm_params->cm_c02);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c10",
			&cm_params->cm_c10);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c11",
			&cm_params->cm_c11);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c12",
			&cm_params->cm_c12);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c20",
			&cm_params->cm_c20);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c21",
			&cm_params->cm_c21);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_c22",
			&cm_params->cm_c22);

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_coeff_round_en",
			&cm_params->cm_coeff_round_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_precision_mask",
			&cm_params->cm_precision_mask);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_bits_switch",
			&cm_params->bits_switch);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cm_gray_en",
			&cm_params->cm_gray_en);
}

/* parsing of struct mtk_panel_spr_params */
static void parse_lcm_dsi_spr_params(struct device_node *np,
			struct mtk_panel_spr_params *spr_params)
{
	struct property *pp = NULL;
	struct device_node *spr_np = NULL;
	int ret = 0, len = 0;

	if (IS_ERR_OR_NULL(spr_params))
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_enable",
			&spr_params->enable);

	if (spr_params->enable == 0)
		return;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_relay",
			&spr_params->relay);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_rgb_swap",
			&spr_params->rgb_swap);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_bypass_dither",
			&spr_params->bypass_dither);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_postalign_en",
			&spr_params->postalign_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_wrap_mode",
			&spr_params->wrap_mode);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_specialcaseen",
			&spr_params->specialcaseen);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_indata_res_sel",
			&spr_params->indata_res_sel);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_outdata_res_sel",
			&spr_params->outdata_res_sel);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_padding_repeat_en",
			&spr_params->padding_repeat_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_postalign_6type_mode_en",
			&spr_params->postalign_6type_mode_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_custom_header_en",
			&spr_params->custom_header_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_custom_header",
			&spr_params->custom_header);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_format_type",
			&spr_params->spr_format_type);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_rg_xy_swap",
			&spr_params->rg_xy_swap);

	spr_np = of_parse_phandle(np,
			"lcm-params-dsi-spr_ip_params", 0);
	if (!IS_ERR_OR_NULL(spr_np)) {
		pp = of_find_property(spr_np, "spr_ip_cfg", &len);
		if (pp != NULL && len > 0) {
			LCM_KZALLOC(spr_params->spr_ip_params, len + 1, GFP_KERNEL);
			if (spr_params->spr_ip_params == NULL) {
				DDPPR_ERR("%s, %d, failed to allocate spr_ip_params, %d\n",
					__func__, __LINE__, len);
				return;
			}
			spr_params->spr_ip_params_len = len;
			ret = mtk_lcm_dts_read_u32_array(spr_np,
					"spr_ip_cfg",
					spr_params->spr_ip_params, 0, len + 1);
			if (ret <= 0) {
				DDPPR_ERR("%s, %d,failed to get spr_ip_cfg, ret:%d\n",
					__func__, __LINE__, ret);
				LCM_KFREE(spr_params->spr_ip_params, len + 1);
				spr_params->spr_ip_params = NULL;
				spr_params->spr_ip_params_len = 0;
				return;
			}
		}
	}

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_color_param0_type",
			&spr_params->spr_color_params[0].spr_color_params_type);
	ret = mtk_lcm_dts_read_u8_array(np,
			"lcm-params-dsi-spr_color_param0_param_list",
			spr_params->spr_color_params[0].para_list, 0, 80);
	if (ret > 0)
		spr_params->spr_color_params[0].count = ret;
	ret = mtk_lcm_dts_read_u8_array(np,
			"lcm-params-dsi-spr_color_param0_tune_list",
			spr_params->spr_color_params[0].tune_list, 0, 80);

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_color_param1_type",
			&spr_params->spr_color_params[1].spr_color_params_type);
	ret = mtk_lcm_dts_read_u8_array(np,
			"lcm-params-dsi-spr_color_param1_param_list",
			spr_params->spr_color_params[1].para_list, 0, 80);
	if (ret > 0)
		spr_params->spr_color_params[1].count = ret;
	ret = mtk_lcm_dts_read_u8_array(np,
			"lcm-params-dsi-spr_color_param1_tune_list",
			spr_params->spr_color_params[1].tune_list, 0, 80);

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-spr_color_param2_type",
			&spr_params->spr_color_params[2].spr_color_params_type);
	ret = mtk_lcm_dts_read_u8_array(np,
			"lcm-params-dsi-spr_color_param2_param_list",
			spr_params->spr_color_params[2].para_list, 0, 80);
	if (ret > 0)
		spr_params->spr_color_params[2].count = ret;
	ret = mtk_lcm_dts_read_u8_array(np,
			"lcm-params-dsi-spr_color_param2_tune_list",
			spr_params->spr_color_params[2].tune_list, 0, 80);
}

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
			DDPDBG("%s, %d: the %d dyn fps of invalid cmd:%d\n",
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

static void parse_lcm_dsi_round_corner_dtsi(
			struct device_node *rc_np,
			struct mtk_panel_params *ext_param)
{
	struct property *pp = NULL;
	int rc_len = 0, ret = 0;
#if MTK_LCM_DEBUG_DUMP
	unsigned int i = 0;
	char *addr = NULL;
#endif

	DDPDUMP("%s, %d, round corner pattern from dtsi\n",
		__func__, __LINE__);
	mtk_lcm_dts_read_u32(rc_np, "pattern_height",
			&ext_param->corner_pattern_height);
	mtk_lcm_dts_read_u32(rc_np, "pattern_height_bot",
			&ext_param->corner_pattern_height_bot);

	pp = of_find_property(rc_np, "left_top", &rc_len);
	if (pp != NULL && rc_len > 0) {
		LCM_KZALLOC(ext_param->corner_pattern_lt_addr, rc_len + 1, GFP_KERNEL);
		if (ext_param->corner_pattern_lt_addr == NULL) {
			DDPPR_ERR("%s, %d, failed to allocate rc_lt, %d\n",
				__func__, __LINE__, rc_len);
			return;
		}
		ext_param->corner_pattern_tp_size = rc_len;
		ret = mtk_lcm_dts_read_u8_array(rc_np, "left_top",
				ext_param->corner_pattern_lt_addr, 0, rc_len + 1);
		if (ret <= 0) {
			DDPPR_ERR("%s, failed to parsing rc_tp, %d\n",
				__func__, ext_param->corner_pattern_tp_size);
			LCM_KFREE(ext_param->corner_pattern_lt_addr, rc_len + 1);
			ext_param->corner_pattern_lt_addr = NULL;
			ext_param->corner_pattern_tp_size = 0;
			return;
		}

#if MTK_LCM_DEBUG_DUMP
		DDPMSG("====round corner top pattern size:%u =====\n",
			ext_param->corner_pattern_tp_size);
		addr = (char *)ext_param->corner_pattern_lt_addr;
		for (i = 0; i < ext_param->corner_pattern_tp_size - 12;
			i += 12)
			DDPMSG(
				"rc_tp: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				 *(addr + i),
				 *(addr + i + 1),
				 *(addr + i + 2),
				 *(addr + i + 3),
				 *(addr + i + 4),
				 *(addr + i + 5),
				 *(addr + i + 6),
				 *(addr + i + 7),
				 *(addr + i + 8),
				 *(addr + i + 9),
				 *(addr + i + 10),
				 *(addr + i + 11));
		DDPMSG("=============================\n");
#endif
	}

	pp = of_find_property(rc_np, "left_top_left", &rc_len);
	if (pp != NULL && rc_len > 0) {
		LCM_KZALLOC(ext_param->corner_pattern_lt_addr_l,
				rc_len + 1, GFP_KERNEL);
		if (ext_param->corner_pattern_lt_addr_l == NULL) {
			DDPPR_ERR("%s, %d, failed to allocate rc_lt_l, %d\n",
				__func__, __LINE__, rc_len);
			return;
		}
		ext_param->corner_pattern_tp_size_l = rc_len;
		ret = mtk_lcm_dts_read_u8_array(rc_np, "left_top_left",
				ext_param->corner_pattern_lt_addr_l, 0, rc_len + 1);
		if (ret <= 0) {
			DDPPR_ERR("%s, failed to parsing rc_tp_l, %d\n",
				__func__, ext_param->corner_pattern_tp_size_l);
			LCM_KFREE(ext_param->corner_pattern_lt_addr_l,
				rc_len + 1);
			ext_param->corner_pattern_lt_addr_l = NULL;
			ext_param->corner_pattern_tp_size_l = 0;
			return;
		}
	}

	pp = of_find_property(rc_np, "left_top_right", &rc_len);
	if (pp != NULL && rc_len > 0) {
		LCM_KZALLOC(ext_param->corner_pattern_lt_addr_r,
				rc_len + 1, GFP_KERNEL);
		if (ext_param->corner_pattern_lt_addr_r == NULL) {
			DDPPR_ERR("%s, %d, failed to allocate rc_lt_r, %d\n",
				__func__, __LINE__, rc_len);
			return;
		}
		ext_param->corner_pattern_tp_size_r = rc_len;
		ret = mtk_lcm_dts_read_u8_array(rc_np, "left_top_right",
				ext_param->corner_pattern_lt_addr_r, 0, rc_len + 1);
		if (ret <= 0) {
			DDPPR_ERR("%s, failed to parsing rc_tp_r, %d\n",
				__func__, ext_param->corner_pattern_tp_size_r);
			LCM_KFREE(ext_param->corner_pattern_lt_addr_r,
				rc_len + 1);
			ext_param->corner_pattern_lt_addr_r = NULL;
			ext_param->corner_pattern_tp_size_r = 0;
			return;
		}
	}
}

static void parse_lcm_dsi_round_corner_header(
			struct device_node *np,
			struct mtk_panel_params *ext_param)
{
	const char *pattern;
	int ret = 0;

	DDPDUMP("%s, %d, round corner pattern from header\n",
		__func__, __LINE__);
	ret = of_property_read_string(np,
			"lcm-params-dsi-corner_pattern_name",
			&pattern);
	if (ret < 0 || strlen(pattern) == 0) {
		DDPMSG("%s,%d: invalid pattern, ret:%d\n",
			__func__, __LINE__, ret);
		return;
	}

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

/* parse dsi fps ext params: mtk_panel_params */
static void parse_lcm_dsi_fps_ext_param(struct device_node *np,
			struct mtk_panel_params *ext_param)
{
	char prop[128] = { 0 };
	u8 temp[RT_MAX_NUM * 2 + 2] = {0};
	unsigned int i = 0, j = 0;
	int len = 0, ret = 0;
	struct device_node *rc_np = NULL;

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
		rc_np = of_parse_phandle(np,
				"lcm-params-dsi-round_corner_pattern", 0);

		if (!IS_ERR_OR_NULL(rc_np))
			parse_lcm_dsi_round_corner_dtsi(rc_np, ext_param);
		else
			parse_lcm_dsi_round_corner_header(np, ext_param);
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
			"lcm-params-dsi-lp_perline_en",
			&ext_param->lp_perline_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cmd_null_pkt_en",
			&ext_param->cmd_null_pkt_en);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-cmd_null_pkt_len",
			&ext_param->cmd_null_pkt_len);
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

	ret = 0;
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lcm_is_support_od", &ret);
	if (ret <= 0)
		ext_param->is_support_od = false;
	else
		ext_param->is_support_od = true;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-lcm_is_support_dmr", &ret);
	if (ret <= 0)
		ext_param->is_support_dmr = false;
	else
		ext_param->is_support_dmr = true;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-skip_vblank",
			&ext_param->skip_vblank);
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

/* parse dsi msync cmd table: mtk_panel_params */
#define MTK_LCM_MSYNC_MAX_LEVEL (20)
#define MTK_LCM_MSYNC_MAX_CMD_NUM (20)
static void parse_lcm_dsi_msync_te_level_table(struct device_node *np,
	unsigned int count, char *func, struct msync_level_table *table)
{
	unsigned int i = 0;
	char name[128] = { 0 };
	int len = 0, ret = 0;
	unsigned int *level_id = NULL;
	unsigned int *level_fps = NULL;
	unsigned int *max_fps = NULL;
	unsigned int *min_fps = NULL;

	count = (count < MTK_LCM_MSYNC_MAX_LEVEL) ?
			count : MTK_LCM_MSYNC_MAX_LEVEL;
	if (count == 0 || func == NULL ||
		table == NULL)
		return;

	/* read level_id */
	LCM_KZALLOC(level_id, count * sizeof(unsigned int),
			GFP_KERNEL);
	if (level_id == NULL) {
		DDPPR_ERR("%s, %d, failed to allocate level_id\n",
			__func__, __LINE__);
		goto end;
	}
	ret = snprintf(name, sizeof(name),
			"lcm-params-dsi-%s_level_id", func);
	if (ret < 0 || (size_t)ret >= sizeof(name))
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
	len = mtk_lcm_dts_read_u32_array(np, name,
		&level_id[0], 0, count);
	if (len < 0 || len > count)
		DDPPR_ERR("%s, %d, failed to get %s\n",
			__func__, __LINE__, name);

	/* read level_fps */
	LCM_KZALLOC(level_fps, count * sizeof(unsigned int),
			GFP_KERNEL);
	if (level_fps == NULL) {
		DDPPR_ERR("%s, %d, failed to allocate level_fps\n",
			__func__, __LINE__);
		goto end;
	}
	ret = snprintf(name, sizeof(name),
			"lcm-params-dsi-%s_level_fps", func);
	if (ret < 0 || (size_t)ret >= sizeof(name))
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
	len = mtk_lcm_dts_read_u32_array(np, name,
		&level_fps[0], 0, count);
	if (len < 0 || len > count)
		DDPPR_ERR("%s, %d, failed to get %s\n",
			__func__, __LINE__, name);

	/* read max_fps */
	LCM_KZALLOC(max_fps, count * sizeof(unsigned int),
			GFP_KERNEL);
	if (max_fps == NULL) {
		DDPPR_ERR("%s, %d, failed to allocate max_fps\n",
			__func__, __LINE__);
		goto end;
	}
	ret = snprintf(name, sizeof(name),
			"lcm-params-dsi-%s_max_fps", func);
	if (ret < 0 || (size_t)ret >= sizeof(name))
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
	len = mtk_lcm_dts_read_u32_array(np, name,
		&max_fps[0], 0, count);
	if (len < 0 || len > count)
		DDPPR_ERR("%s, %d, failed to get %s\n",
			__func__, __LINE__, name);

	/* read min_fps */
	LCM_KZALLOC(min_fps, count * sizeof(unsigned int),
			GFP_KERNEL);
	if (min_fps == NULL) {
		DDPPR_ERR("%s, %d, failed to allocate min_fps\n",
			__func__, __LINE__);
		goto end;
	}
	ret = snprintf(name, sizeof(name),
			"lcm-params-dsi-%s_min_fps", func);
	if (ret < 0 || (size_t)ret >= sizeof(name))
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
	len = mtk_lcm_dts_read_u32_array(np, name,
		&min_fps[0], 0, count);
	if (len < 0 || len > count)
		DDPPR_ERR("%s, %d, failed to get %s\n",
			__func__, __LINE__, name);

	/* save parsing result */
	for (i = 0; i < count; i++) {
		table[i].level_id = level_id[i];
		table[i].level_fps = level_fps[i];
		table[i].max_fps = max_fps[i];
		table[i].min_fps = min_fps[i];
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s, %d >>> table%u, id:%u, fps:%u, max:%u, min:%u\n",
			__func__, __LINE__, i,
			table[i].level_id, table[i].level_fps,
			table[i].max_fps, table[i].min_fps);
#endif
	}

end:
	if (level_id != NULL)
		LCM_KFREE(level_id, count * sizeof(unsigned int));
	if (level_fps != NULL)
		LCM_KFREE(level_fps, count * sizeof(unsigned int));
	if (max_fps != NULL)
		LCM_KFREE(max_fps, count * sizeof(unsigned int));
	if (min_fps != NULL)
		LCM_KFREE(min_fps, count * sizeof(unsigned int));
}

static void parse_lcm_dsi_msync_cmd_list(struct device_node *np,
	unsigned int count, struct msync_cmd_list *list, char *func)
{
	unsigned int i = 0;
	int ret = 0;
	char name[128] = { 0 };

	count = (count < MTK_LCM_MSYNC_MAX_CMD_NUM) ?
			count : MTK_LCM_MSYNC_MAX_CMD_NUM;
	if (count == 0 || func == NULL ||
		list == NULL)
		return;

	for (i = 0; i < count; i++) {
		/*parse rte_cmd_list->cmd_num*/
		ret = snprintf(name, sizeof(name),
				"lcm-params-dsi-%s_cmd_list%d_num", func, i);
		if (ret < 0 || (size_t)ret >= sizeof(name))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
		mtk_lcm_dts_read_u32(np, name, &list[i].cmd_num);
		if (list[i].cmd_num == 0)
			break;

		/*parse rte_cmd_list->para_list*/
		ret = snprintf(name, sizeof(name),
				"lcm-params-dsi-%s_cmd_list%d_para", func, i);
		if (ret < 0 || (size_t)ret >= sizeof(name))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
		ret = mtk_lcm_dts_read_u8_array(np, name,
				(u8 *)&list[i].para_list, 0, list[i].cmd_num + 1);
		if (ret <= 0)
			DDPPR_ERR("%s,%d: failed to parse %s\n",
				__func__, __LINE__, name);
	}
}

static void parse_lcm_dsi_msync_mte(struct device_node *np,
		struct msync_multi_te_table *table)
{
	unsigned int count = 0;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-mte_tb_count",
			&count);
	if (count > 0)
		parse_lcm_dsi_msync_te_level_table(np,
					count, "mte", &table->multi_te_level[0]);
}

static void parse_lcm_dsi_msync_tte(struct device_node *np,
		struct msync_trigger_level_te_table *table)
{
	unsigned int count = 0;
	int ret = 0;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-tte_level_te_count",
			&count);
	if (count > 0) {
		char name[128] = { 0 };
		int i = 0, num = 0;

		for (i = 0; i < count; i++) {
			/*parse trigger_level_te_level->id*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-tte_level%d_id", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->trigger_level_te_level[i].id);

			/*parse trigger_level_te_level->level_fps*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-tte_level%d_level_fps", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->trigger_level_te_level[i].level_fps);

			/*parse trigger_level_te_level->max_fps*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-tte_level%d_max_fps", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->trigger_level_te_level[i].max_fps);

			/*parse trigger_level_te_level->min_fps*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-tte_level%d_min_fps", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->trigger_level_te_level[i].min_fps);

			/*parse trigger_level_te_level->cmd_list */
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-tte_level%d_cmd_num", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name, &num);

			if (num > 0) {
				ret = snprintf(name, sizeof(name), "tte_level%d", i);
				if (ret < 0 || (size_t)ret >= sizeof(name))
					DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
				parse_lcm_dsi_msync_cmd_list(np, num,
						&table->trigger_level_te_level[i].cmd_list[0],
						name);
			}
		}
	}
}

static void parse_lcm_dsi_msync_rte(struct device_node *np,
		struct msync_request_te_table *table)
{
	int ret = 0;
	unsigned int count = 0;

	if (table == NULL)
		return;

	/*parse rte common params */
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_msync_ctrl_idx",
			(u32 *)&table->msync_ctrl_idx);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_msync_rte_idx",
			(u32 *)&table->msync_rte_idx);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_msync_valid_te_idx",
			(u32 *)&table->msync_valid_te_idx);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_msync_max_vfp_idx",
			(u32 *)&table->msync_max_vfp_idx);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_msync_en_byte",
			(u32 *)&table->msync_en_byte);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_msync_en_mask",
			(u32 *)&table->msync_en_mask);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_delay_mode_byte",
			(u32 *)&table->delay_mode_byte);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_delay_mode_mask",
			(u32 *)&table->delay_mode_mask);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_start1_byte",
			(u32 *)&table->valid_te_start_1_byte);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_start1_mask",
			(u32 *)&table->valid_te_start_1_mask);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_start2_byte",
			(u32 *)&table->valid_te_start_2_byte);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_start2_mask",
			(u32 *)&table->valid_te_start_2_mask);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_end1_byte",
			(u32 *)&table->valid_te_end_1_byte);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_end1_mask",
			(u32 *)&table->valid_te_end_1_mask);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_end2_byte",
			(u32 *)&table->valid_te_end_2_byte);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_valid_te_end2_mask",
			(u32 *)&table->valid_te_end_2_mask);

	/*parse rte_cmd_list */
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_cmd_num", &count);
	parse_lcm_dsi_msync_cmd_list(np, count,
				&table->rte_cmd_list[0], "rte");

	/*parse request_te_level */
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_request_te_count",
			&count);
	if (count > 0) {
		char name[128] = { 0 };
		int i = 0, num = 0;

		for (i = 0; i < count; i++) {
			/*parse request_te_level->id*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-rte_request%d_id", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->request_te_level[i].id);

			/*parse request_te_level->level_fps*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-rte_request%d_level_fps", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->request_te_level[i].level_fps);

			/*parse request_te_level->max_fps*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-rte_request%d_max_fps", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->request_te_level[i].max_fps);

			/*parse request_te_level->min_fps*/
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-rte_request%d_min_fps", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name,
					&table->request_te_level[i].min_fps);

			/*parse request_te_level->cmd_list */
			ret = snprintf(name, sizeof(name),
					"lcm-params-dsi-rte_request%d_cmd_num", i);
			if (ret < 0 || (size_t)ret >= sizeof(name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			mtk_lcm_dts_read_u32(np, name, &num);

			if (num > 0) {
				ret = snprintf(name, sizeof(name), "rte_request%d", i);
				if (ret < 0 || (size_t)ret >= sizeof(name))
					DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
				parse_lcm_dsi_msync_cmd_list(np, num,
						&table->request_te_level[i].cmd_list[0],
						name);
			}
		}
	}

	/*parse rte_te_level */
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-rte_tb_count",
			&count);
	if (count > 0)
		parse_lcm_dsi_msync_te_level_table(np, count, "rte",
				&table->rte_te_level[0]);
}

static void parse_lcm_dsi_msync_cmd_table(struct device_node *np,
			struct mtk_panel_params *ext_param)
{
	if (IS_ERR_OR_NULL(ext_param) || IS_ERR_OR_NULL(np))
		return;

	if (ext_param->msync2_enable == 0)
		return;

	/* parsing common params */
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
			"lcm-params-dsi-delay_frame_num",
			&ext_param->msync_cmd_table.delay_frame_num);

	/* parsing RTE params */
	parse_lcm_dsi_msync_rte(np,
				&ext_param->msync_cmd_table.request_te_tb);

	/* parsing MTE params*/
	parse_lcm_dsi_msync_mte(np,
				&ext_param->msync_cmd_table.multi_te_tb);

	/* parsing TTE params */
	parse_lcm_dsi_msync_tte(np,
				&ext_param->msync_cmd_table.trigger_level_te_tb);

}

static void parse_lcm_dsi_fps_setting(struct device_node *np,
	struct mtk_lcm_mode_dsi *mode_node, unsigned int phy_type)
{
	struct device_node *child_np = NULL;
	struct drm_display_mode *mode = &mode_node->mode;
	struct mtk_panel_params *ext_param = &mode_node->ext_param;

	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-voltage",
			&mode_node->voltage);
	mtk_lcm_dts_read_u32(np,
			"lcm-params-dsi-fake",
			&mode_node->fake);

	parse_lcm_dsi_fps_mode(np, mode);
	parse_lcm_dsi_fps_ext_param(np, ext_param);

	if (phy_type == MTK_LCM_MIPI_CPHY)
		ext_param->is_cphy = 1;
	else
		ext_param->is_cphy = 0;

	for_each_available_child_of_node(np, child_np) {
		/* cm params */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-params-dsi-cm-params")) {
			DDPINFO("%s, parsing cm-params\n", __func__);
			parse_lcm_dsi_cm_params(child_np,
					&ext_param->cm_params);
			continue;
		}

		/* spr params */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-params-dsi-spr-params")) {
			DDPINFO("%s, parsing spr-params\n", __func__);
			parse_lcm_dsi_spr_params(child_np,
					&ext_param->spr_params);
			continue;
		}

		/* dsc params */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-params-dsi-dsc-params")) {
			DDPINFO("%s, parsing dsc-params\n", __func__);
			parse_lcm_dsi_dsc_mode(child_np,
					&ext_param->dsc_params);
			continue;
		}

		/* phy timcon */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-params-dsi-phy-timcon")) {
			DDPINFO("%s, parsing phy-timcon\n", __func__);
			parse_lcm_dsi_phy_timcon(child_np,
					&ext_param->phy_timcon);
			continue;
		}
		/* dyn */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-params-dsi-dyn")) {
			DDPINFO("%s, parsing dyn\n", __func__);
			parse_lcm_dsi_dyn(child_np,
					&ext_param->dyn);
			continue;
		}
		/* dyn fps */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-params-dsi-dyn-fps")) {
			DDPINFO("%s, parsing dyn-fps\n", __func__);
			parse_lcm_dsi_dyn_fps(child_np,
					&ext_param->dyn_fps);
			continue;
		}

		/* msync cmd table */
		if (mode_node->ext_param.msync2_enable != 0) {
			if (of_device_is_compatible(child_np,
				"mediatek,lcm-params-dsi-msync-cmd-table")) {
				DDPINFO("%s, parsing msync-cmd-table\n", __func__);
				parse_lcm_dsi_msync_cmd_table(child_np, ext_param);
				continue;
			}
		}

		/* fpga settings */
		if (of_device_is_compatible(child_np,
			"mediatek,lcm-dsi-fpga-params")) {
			DDPINFO("%s, parsing fpga-params\n", __func__);
			parse_lcm_dsi_fpga_settings(child_np, mode_node);
			continue;
		}
	}
}

int parse_lcm_params_dsi(struct device_node *np,
		struct mtk_lcm_params_dsi *params)
{
	unsigned int i = 0;
	unsigned int default_mode = 0;
	unsigned int flag[64] = { 0 };
	u32 *mode = NULL;
	char mode_name[128] = { 0 };
	struct device_node *mode_np = NULL;
	int ret = 0, len = 0;
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
		if (params->lcm_pinctrl_name == NULL) {
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
		DDPDBG("%s, %d, lcm_pinctrl_names is empty, %d\n",
			__func__, __LINE__, len);
	else
		DDPDBG("%s, %d, failed to get lcm_pinctrl_names, %d\n",
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
	if (mode == NULL) {
		DDPMSG("%s, failed to allocate mode buffer\n", __func__);
		return -ENOMEM;
	}

	len = mtk_lcm_dts_read_u32_array(np, "lcm-params-dsi-mode_list",
			mode, 0, params->mode_count * MTK_LCM_MODE_UNIT);
	if (len != params->mode_count * MTK_LCM_MODE_UNIT) {
		DDPMSG("%s: invalid dsi mode list, len:%d, count:%u",
			__func__, len, params->mode_count);
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
				if (mode_node == NULL) {
					DDPMSG("%s, failed to allocate mode buffer\n", __func__);
					return -ENOMEM;
				}

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
		const struct mtk_panel_cust *cust)
{
	struct device_node *mode_np = NULL;
	struct property *pp = NULL;
	struct mtk_lcm_mode_dsi *mode_node;
	char mode_name[128] = {0};
	int len = 0, ret = 0;

	if (IS_ERR_OR_NULL(params) || IS_ERR_OR_NULL(np) || IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return -EINVAL;
	}
	memset(ops, 0, sizeof(struct mtk_lcm_ops_dsi));
	mtk_lcm_dts_read_u32(np, "dsi_flag_length",
			&ops->flag_len);

	ret = parse_lcm_ops_func(np,
				&ops->prepare, "prepare_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing prepare_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->unprepare, "unprepare_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing unprepare_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	pp = of_find_property(np, "compare_id_value_data", &len);
	if (pp != NULL && len > 0) {
		LCM_KZALLOC(ops->compare_id_value_data, len + 1, GFP_KERNEL);
		if (ops->compare_id_value_data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate compare id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->compare_id_value_length = len;
		ret = mtk_lcm_dts_read_u8_array(np,
					"compare_id_value_data",
					&ops->compare_id_value_data[0], 0, len + 1);
		if (ret <= 0) {
			DDPPR_ERR("%s,%d: failed to parse compare id data\n",
				__func__, __LINE__);
			LCM_KFREE(ops->compare_id_value_data, len + 1);
			ops->compare_id_value_data = NULL;
			ops->compare_id_value_length = 0;
			return -EFAULT;
		}

		ret = parse_lcm_ops_func(np,
					&ops->compare_id, "compare_id_table",
					ops->flag_len,
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
		ops->compare_id_value_data = NULL;
	}
#endif

	mtk_lcm_dts_read_u32(np, "set_backlight_mask",
				&ops->set_backlight_mask);
	ret = parse_lcm_ops_func(np,
				&ops->set_backlight_cmdq,
				"set_backlight_cmdq_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI,  cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing set_backlight_cmdq_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->set_elvss_cmdq,
				"set_elvss_cmdq_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI,  cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing set_elvss_cmdq_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->set_backlight_elvss_cmdq,
				"set_backlight_elvss_cmdq_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI,  cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing set_backlight_elvss_cmdq_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	pp = of_find_property(np, "aod_mode_value_data", &len);
	if (pp != NULL && len > 0) {
		LCM_KZALLOC(ops->aod_mode_value_data, len + 1, GFP_KERNEL);
		if (ops->aod_mode_value_data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate aod mode\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		ops->aod_mode_value_length = len;
		ret = mtk_lcm_dts_read_u8_array(np,
					"aod_mode_value_data",
					&ops->aod_mode_value_data[0], 0, len + 1);
		if (ret <= 0) {
			DDPPR_ERR("%s,%d: failed to parse aod mode\n",
				__func__, __LINE__);
			LCM_KFREE(ops->aod_mode_value_data, len + 1);
			ops->aod_mode_value_data = NULL;
			ops->aod_mode_value_length = 0;
			return -EFAULT;
		}

		ret = parse_lcm_ops_func(np,
					&ops->aod_mode_check, "aod_mode_check_table",
					ops->flag_len,
					MTK_LCM_FUNC_DSI, cust,
					MTK_LCM_PHASE_KERNEL);
		if (ret < 0) {
			DDPMSG("%s, %d failed to parsing aod_mode_check_table, ret:%d\n",
				__func__, __LINE__, ret);
			return ret;
		}
	} else {
		DDPMSG("%s, %d, failed to get aod mode,len%d\n",
			__func__, __LINE__, len);
		ops->aod_mode_value_length = 0;
		ops->aod_mode_value_data = NULL;
	}

	pp = of_find_property(np, "ata_id_value_data", &len);
	if (pp != NULL && len > 0) {
		LCM_KZALLOC(ops->ata_id_value_data, len + 1, GFP_KERNEL);
		if (ops->ata_id_value_data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate ata id data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		ops->ata_id_value_length = len;
		ret = mtk_lcm_dts_read_u8_array(np,
					"ata_id_value_data",
					&ops->ata_id_value_data[0], 0, len + 1);
		if (ret <= 0) {
			DDPPR_ERR("%s,%d: failed to parse ata id\n",
				__func__, __LINE__);
			LCM_KFREE(ops->ata_id_value_data, len + 1);
			ops->ata_id_value_data = NULL;
			ops->ata_id_value_length = 0;
			return -EFAULT;
		}

		ret = parse_lcm_ops_func(np,
					&ops->ata_check, "ata_check_table",
					ops->flag_len,
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
		ops->ata_id_value_data = NULL;
	}

	mtk_lcm_dts_read_u32(np, "set_aod_light_mask",
				&ops->set_aod_light_mask);
	ret = parse_lcm_ops_func(np,
				&ops->set_aod_light,
				"set_aod_light_table",
				ops->flag_len,
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
				ops->flag_len,
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
				ops->flag_len,
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
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing doze_enable_start_table, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->doze_area, "doze_area_table",
				ops->flag_len,
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
				ops->flag_len,
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
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing hbm_set_cmdq, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->msync_request_mte, "msync_request_mte_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing msync_request_mte, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->default_msync_close_mte, "msync_close_mte_default_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing msync_close_mte, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = parse_lcm_ops_func(np,
				&ops->msync_default_mte, "msync_default_mte_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing msync_default_mte, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	mtk_lcm_dts_read_u32(np, "read_panelid_len",
				&ops->read_panelid_len);
	ret = parse_lcm_ops_func(np,
				&ops->read_panelid, "read_panelid_table",
				ops->flag_len,
				MTK_LCM_FUNC_DSI, cust,
				MTK_LCM_PHASE_KERNEL);
	if (ret < 0) {
		DDPMSG("%s, %d failed to parsing read_panelid, ret:%d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	for_each_available_child_of_node(np, mode_np) {
		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-fps-switch-before-powerdown")) {
			ret = parse_lcm_common_ops_func_u8(mode_np,
					&ops->fps_switch_bfoff_mode,
					&ops->fps_switch_bfoff_mode_count,
					"fps_switch_mode_list",
					&ops->default_fps_switch_bfoff,
					"default_fps_switch_table",
					ops->flag_len, MTK_LCM_FUNC_DSI,
					cust, MTK_LCM_PHASE_KERNEL);
			if (ret < 0 || ops->fps_switch_bfoff_mode_count <= 0) {
				ops->fps_switch_bfoff_mode = NULL;
				ops->fps_switch_bfoff_mode_count = 0;
			}

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
						ops->flag_len,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
				if (ret < 0) {
					DDPMSG("%s, %d failed to parsing %s, ret:%d\n",
						__func__, __LINE__, mode_name, ret);
					mode_node->fps_switch_bfoff.size = 0;
					return ret;
				}
			}
			continue;
		}

		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-fps-switch-after-poweron")) {
			/* parse mode list */
			ret = parse_lcm_common_ops_func_u8(mode_np,
					&ops->fps_switch_afon_mode,
					&ops->fps_switch_afon_mode_count,
					"fps_switch_mode_list",
					&ops->default_fps_switch_afon,
					"default_fps_switch_table",
					ops->flag_len, MTK_LCM_FUNC_DSI,
					cust, MTK_LCM_PHASE_KERNEL);
			if (ret < 0 || ops->fps_switch_afon_mode_count <= 0) {
				ops->fps_switch_afon_mode = NULL;
				ops->fps_switch_afon_mode_count = 0;
			}

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
						ops->flag_len,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
				if (ret < 0) {
					DDPMSG("%s, %d failed to parsing %s, ret:%d\n",
						__func__, __LINE__, mode_name, ret);
					mode_node->fps_switch_afon.size = 0;
					return ret;
				}
			}
			continue;
		}

		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-msync-switch-mte")) {
			ret = parse_lcm_common_ops_func_u8(mode_np,
					&ops->msync_switch_mte_mode,
					&ops->msync_switch_mte_mode_count,
					"msync_switch_mte_mode_list",
					&ops->default_msync_switch_mte,
					"msync_switch_mte_default_table",
					ops->flag_len, MTK_LCM_FUNC_DSI,
					cust, MTK_LCM_PHASE_KERNEL);
			if (ret < 0 || ops->msync_switch_mte_mode_count <= 0) {
				ops->msync_switch_mte_mode = NULL;
				ops->msync_switch_mte_mode_count = 0;
			}

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
						ops->flag_len,
						MTK_LCM_FUNC_DSI, cust,
						MTK_LCM_PHASE_KERNEL);
				if (ret < 0) {
					DDPMSG("%s, %d failed to parsing %s, ret:%d\n",
						__func__, __LINE__, mode_name, ret);
					return ret;
				}
			}
			continue;
		}

		if (of_device_is_compatible(mode_np,
				"mediatek,lcm-ops-dsi-msync-set-min-fps")) {
			char list_name[128] = {0};
			char table_name[128] = {0};

			list_for_each_entry(mode_node, &params->mode_list, list) {
				if (mode_node->ext_param.msync2_enable == 0)
					continue;

				ret = snprintf(list_name, sizeof(list_name),
					"msync_min_fps_list_%u_%u_%u_%u",
					mode_node->id, mode_node->width,
					mode_node->height, mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(list_name))
					DDPMSG("%s, %d, snprintf failed, ret:%d\n",
						__func__, __LINE__, ret);

				ret = snprintf(table_name, sizeof(table_name),
					"msync_set_min_fps_table_%u_%u_%u_%u",
					mode_node->id, mode_node->width,
					mode_node->height, mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(table_name))
					DDPMSG("%s, %d, snprintf failed, ret:%d\n",
						__func__, __LINE__, ret);

				ret = parse_lcm_common_ops_func_u32(mode_np,
						&mode_node->msync_set_min_fps_list,
						&mode_node->msync_set_min_fps_list_length,
						list_name,
						&mode_node->msync_set_min_fps,
						table_name,
						ops->flag_len, MTK_LCM_FUNC_DSI,
						cust, MTK_LCM_PHASE_KERNEL);
				if (ret < 0 || mode_node->msync_set_min_fps_list_length <= 0) {
					mode_node->msync_set_min_fps_list = NULL;
					mode_node->msync_set_min_fps_list_length = 0;
				}
			}
			continue;
		}
	}

	return 0;
}
EXPORT_SYMBOL(parse_lcm_ops_dsi);

/* dump dsi cm params */
static void dump_lcm_dsi_cm_params(struct mtk_panel_cm_params *cm_params, unsigned int fps)
{
	DDPDUMP("-------------  fps%u cm_params -------------\n", fps);
	if (cm_params->enable == 0) {
		DDPDUMP("%s: cm off\n", __func__);
		return;
	}

	DDPDUMP("enable:%u, relay:%u, coeff_round:%u, precision:%u, bits_switch:%u, gray:%u\n",
		cm_params->enable,
		cm_params->relay,
		cm_params->cm_coeff_round_en,
		cm_params->cm_precision_mask,
		cm_params->bits_switch,
		cm_params->cm_gray_en);
	DDPDUMP("c0:%u %u %u\n",
		cm_params->cm_c00, cm_params->cm_c01, cm_params->cm_c02);
	DDPDUMP("c1:%u %u %u\n",
		cm_params->cm_c10, cm_params->cm_c11, cm_params->cm_c12);
	DDPDUMP("c2:%u %u %u\n",
		cm_params->cm_c20, cm_params->cm_c21, cm_params->cm_c22);
}

/* dump dsi spr params */
static void dump_lcm_dsi_spr_params(struct mtk_panel_spr_params *spr_params, unsigned int fps)
{
	int i = 0, j = 0;

	DDPDUMP("-------------  fps%u spr_params -------------\n", fps);
	if (spr_params->enable == 0) {
		DDPDUMP("%s: spr off\n", __func__);
		return;
	}

	DDPDUMP("enable:%u, relay:%u, rgbswap:%u, bypass_dither:%u, postalign_en:%u\n",
		spr_params->enable,
		spr_params->relay,
		spr_params->rgb_swap,
		spr_params->bypass_dither,
		spr_params->postalign_en);
	DDPDUMP("wrap_mode:%u, special:%u, indata:%u, outdata:%u, padding_repeat:%u\n",
		spr_params->wrap_mode,
		spr_params->specialcaseen,
		spr_params->indata_res_sel,
		spr_params->outdata_res_sel,
		spr_params->padding_repeat_en);
	DDPDUMP("postalign_6type:%u, custom_en:%u, custom:%u, format_type:%u, rg_xy_swap:%u\n",
		spr_params->postalign_6type_mode_en,
		spr_params->custom_header_en,
		spr_params->custom_header,
		spr_params->spr_format_type,
		spr_params->rg_xy_swap);

	for (i = 0; i < SPR_COLOR_PARAMS_TYPE_NUM; i++) {
		DDPDUMP("color_params%d: type:%d, count:%u\n", i,
			spr_params->spr_color_params[i].spr_color_params_type,
			spr_params->spr_color_params[i].count);
		for (j = 0; j < 80; j += 10) {
			DDPDUMP("para%d: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", j,
				 spr_params->spr_color_params[0].para_list[j],
				 spr_params->spr_color_params[0].para_list[j + 1],
				 spr_params->spr_color_params[0].para_list[j + 2],
				 spr_params->spr_color_params[0].para_list[j + 3],
				 spr_params->spr_color_params[0].para_list[j + 4],
				 spr_params->spr_color_params[0].para_list[j + 5],
				 spr_params->spr_color_params[0].para_list[j + 6],
				 spr_params->spr_color_params[0].para_list[j + 7],
				 spr_params->spr_color_params[0].para_list[j + 8],
				 spr_params->spr_color_params[0].para_list[j + 9]);
		}
		for (j = 0; j < 80; j += 10) {
			DDPDUMP("tune%d: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", j,
				 spr_params->spr_color_params[0].tune_list[j],
				 spr_params->spr_color_params[0].tune_list[j + 1],
				 spr_params->spr_color_params[0].tune_list[j + 2],
				 spr_params->spr_color_params[0].tune_list[j + 3],
				 spr_params->spr_color_params[0].tune_list[j + 4],
				 spr_params->spr_color_params[0].tune_list[j + 5],
				 spr_params->spr_color_params[0].tune_list[j + 6],
				 spr_params->spr_color_params[0].tune_list[j + 7],
				 spr_params->spr_color_params[0].tune_list[j + 8],
				 spr_params->spr_color_params[0].tune_list[j + 9]);
		}
	}
}

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
		if (dyn_fps->dfps_cmd_table[i].src_fps == 0)
			break;
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

static void dump_lcm_dsi_msync_tte_param(unsigned int fps,
	struct msync_trigger_level_te_table *ttable)
{
	int i = 0, j = 0, r = 0;
	__u8 *para_list = NULL;

	DDPDUMP("----------fps%u TTE-----------\n", fps);
	/*dump trigger_level_te_level*/
	for (i = 0; i < MTK_LCM_MSYNC_MAX_LEVEL; i++) {
		if (ttable->trigger_level_te_level[i].level_fps == 0)
			break;

		DDPDUMP("TTE:trigger_level_te_level[%d]: level(id=%u,fps=%u),fps(%u~%u)\n",
			i, ttable->trigger_level_te_level[i].id,
			ttable->trigger_level_te_level[i].level_fps,
			ttable->trigger_level_te_level[i].min_fps,
			ttable->trigger_level_te_level[i].max_fps);

		for (j = 0; j < MTK_LCM_MSYNC_MAX_CMD_NUM; j++) {
			if (ttable->trigger_level_te_level[i].cmd_list[j].cmd_num == 0)
				break;

			DDPDUMP("TTE:trigger_level_te_level[%d]->cmd_list[%d]:num=%u",
				i, j,
				ttable->trigger_level_te_level[i].cmd_list[j].cmd_num);
			para_list =
				&ttable->trigger_level_te_level[i].cmd_list[j].para_list[0];
			for (r = 0; r <
				ttable->trigger_level_te_level[i].cmd_list[j].cmd_num;
				r += 4) {
				DDPDUMP("TTE:   para[%d]:0x%x,0x%x,0x%x,0x%x\n", r,
					para_list[r], para_list[r + 1],
					para_list[r + 2], para_list[r + 3]);
			}
		}
	}
	DDPDUMP("------------------------\n");
}

static void dump_lcm_dsi_msync_mte_param(unsigned int fps,
	struct msync_multi_te_table *mtable)
{
	int i = 0;
	struct msync_level_table *te_tb =
			&mtable->multi_te_level[0];

	DDPDUMP("----------fps:%u MTE-----------\n", fps);
	for (i = 0; i < MTK_LCM_MSYNC_MAX_LEVEL; i++) {
		if (te_tb[i].level_fps == 0)
			break;

		DDPDUMP("MTE[%d]: level(id=%u,fps=%u),fps(%u~%u)\n",
			i, te_tb[i].level_id, te_tb[i].level_fps,
			te_tb[i].min_fps, te_tb[i].max_fps);
	}
	DDPDUMP("-----------------------\n");
}

static void dump_lcm_dsi_msync_rte_param(unsigned int fps,
	struct msync_request_te_table *rtable)
{
	int i = 0, j = 0, r = 0;
	struct msync_level_table *te_tb =
			&rtable->rte_te_level[0];
	__u8 *para_list = NULL;

	DDPDUMP("----------fps:%u RTE-----------\n", fps);
	DDPDUMP("RTE: ctrl_id:0x%x,rte_id:0x%x,valid_te_id:0x%x,max_vfp_id:0x%x\n",
		rtable->msync_ctrl_idx, rtable->msync_rte_idx,
		rtable->msync_valid_te_idx, rtable->msync_max_vfp_idx);
	DDPDUMP("RTE: en_byte:0x%x, en_mask:0x%x, delay_mode(byte:0x%x, mask:0x%x)\n",
		rtable->msync_en_byte, rtable->msync_en_mask,
		rtable->delay_mode_byte, rtable->delay_mode_mask);
	DDPDUMP("RTE: start1(byte:0x%x,mask:0x%x),end1(byte:0x%x,mask:0x%x)",
		rtable->valid_te_start_1_byte, rtable->valid_te_start_1_mask,
		rtable->valid_te_end_1_byte, rtable->valid_te_end_1_mask);
	DDPDUMP("RTE: start2(byte:0x%x,mask:0x%x),end2(byte:0x%x,mask:0x%x)",
		rtable->valid_te_start_2_byte, rtable->valid_te_start_2_mask,
		rtable->valid_te_end_2_byte, rtable->valid_te_end_2_mask);

	/*dump rte_cmd_list*/
	for (i = 0; i < MTK_LCM_MSYNC_MAX_CMD_NUM; i++) {
		if (rtable->rte_cmd_list[i].cmd_num == 0)
			break;

		DDPDUMP("RTE:rte_cmd_list[%d]:num=%u", i,
			rtable->rte_cmd_list[i].cmd_num);
		para_list = &rtable->rte_cmd_list[i].para_list[0];
		for (j = 0; j < rtable->rte_cmd_list[i].cmd_num; j += 4) {
			DDPDUMP("RTE:   para[%d]:0x%x,0x%x,0x%x,0x%x\n", j,
				para_list[j], para_list[j + 1],
				para_list[j + 2], para_list[j + 3]);
		}
	}

	/*dump request_te_level*/
	for (i = 0; i < MTK_LCM_MSYNC_MAX_LEVEL; i++) {
		if (rtable->request_te_level[i].level_fps == 0)
			break;

		DDPDUMP("RTE:request_te_level[%d]: level(id=%u,fps=%u),fps(%u~%u)\n",
			i, rtable->request_te_level[i].id,
			rtable->request_te_level[i].level_fps,
			rtable->request_te_level[i].min_fps,
			rtable->request_te_level[i].max_fps);

		for (j = 0; j < MTK_LCM_MSYNC_MAX_CMD_NUM; j++) {
			if (rtable->request_te_level[i].cmd_list[j].cmd_num == 0)
				break;

			DDPDUMP("RTE:request_te_level[%d]->cmd_list[%d]:num=%u",
				i, j,
				rtable->request_te_level[i].cmd_list[j].cmd_num);
			para_list =
				&rtable->request_te_level[i].cmd_list[j].para_list[0];
			for (r = 0; r <
				rtable->request_te_level[i].cmd_list[j].cmd_num;
				r += 4) {
				DDPDUMP("RTE:   para[%d]:0x%x,0x%x,0x%x,0x%x\n", r,
					para_list[r], para_list[r + 1],
					para_list[r + 2], para_list[r + 3]);
			}
		}
	}

	/*dump rte_te_level*/
	for (i = 0; i < MTK_LCM_MSYNC_MAX_LEVEL; i++) {
		if (te_tb[i].level_fps == 0)
			break;

		DDPDUMP("rte_te_level[%d]: level(id=%u,fps=%u),fps(%u~%u)\n",
			i, te_tb[i].level_id, te_tb[i].level_fps,
			te_tb[i].min_fps, te_tb[i].max_fps);
	}
	DDPDUMP("------------------------\n");
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
	DDPDUMP("lp_perline_en=%u, cmd_null_pkt(en=%u, len=%u)\n",
		ext_param->lp_perline_en,
		ext_param->cmd_null_pkt_en,
		ext_param->cmd_null_pkt_len);
	DDPDUMP("is_cphy:%u, esd_check(cust=%u, enable=%u)\n",
		ext_param->is_cphy,
		ext_param->cust_esd_check,
		ext_param->esd_check_enable);
	DDPDUMP("od:%d, de-mura:%d\n",
		ext_param->is_support_od,
		ext_param->is_support_dmr);
	DDPDUMP("lfr:(enable=%u, fps=%u), msync:(enable=%u, vfp=%u)\n",
		ext_param->lfr_enable,
		ext_param->lfr_minimum_fps,
		ext_param->msync2_enable,
		ext_param->max_vfp_for_msync);

	/* dump msync2 settings */
	if (ext_param->msync2_enable != 0) {
		/*dump msync2 common settings*/
		DDPDUMP("MSYNC20: te_type:%u, fps(%u~%u),level_num:%u,delay_frame:%u\n",
			ext_param->msync_cmd_table.te_type,
			ext_param->msync_cmd_table.msync_min_fps,
			ext_param->msync_cmd_table.msync_max_fps,
			ext_param->msync_cmd_table.msync_level_num,
			ext_param->msync_cmd_table.delay_frame_num);

		/*dump RTE settings*/
		dump_lcm_dsi_msync_rte_param(fps,
			&ext_param->msync_cmd_table.request_te_tb);

		/*dump MTE settings*/
		dump_lcm_dsi_msync_mte_param(fps,
				&ext_param->msync_cmd_table.multi_te_tb);

		/*dump TTE settings*/
		dump_lcm_dsi_msync_tte_param(fps,
				&ext_param->msync_cmd_table.trigger_level_te_tb);
	}

	/* dump esd check table */
	if (ext_param->esd_check_enable != 0) {
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
	}

	/* dump lane swap */
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
	if (IS_ERR_OR_NULL(mode)) {
		DDPPR_ERR("%s, %d, invalid mode\n",
			__func__, __LINE__);
		return;
	}

	DDPDUMP("---------------- mode:%u (%u-%u-%u)-------------------\n",
		mode->id, mode->width, mode->height, mode->fps);
	dump_lcm_dsi_fps_mode(&mode->mode, mode->fps);
	dump_lcm_dsi_fps_ext_param(&mode->ext_param, mode->fps);
	dump_lcm_dsi_cm_params(&mode->ext_param.cm_params, mode->fps);
	dump_lcm_dsi_spr_params(&mode->ext_param.spr_params, mode->fps);
	dump_lcm_dsi_dsc_mode(&mode->ext_param.dsc_params, mode->fps);
	dump_lcm_dsi_phy_timcon(&mode->ext_param.phy_timcon, mode->fps);
	dump_lcm_dsi_dyn(&mode->ext_param.dyn, mode->fps);
	dump_lcm_dsi_dyn_fps(&mode->ext_param.dyn_fps, mode->fps);
}
EXPORT_SYMBOL(dump_lcm_dsi_fps_settings);

/* dump dsi settings*/
void dump_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
	const struct mtk_panel_cust *cust)
{
	struct mtk_lcm_mode_dsi *mode_node;
	int i = 0;

	if (IS_ERR_OR_NULL(params)) {
		DDPPR_ERR("%s, %d: invalid params\n", __func__, __LINE__);
		return;
	}
	DDPDUMP("=========== LCM DSI DUMP ==============\n");
	DDPDUMP("phy:%u,lanes:%u,density:%u,format:%u\n",
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
	    IS_ERR_OR_NULL(cust->dump_params))
		return;

	DDPDUMP("=========== LCM CUSTOMIZATION DUMP ==============\n");
	cust->dump_params();
	DDPDUMP("=============================================\n");
}
EXPORT_SYMBOL(dump_lcm_params_dsi);

void dump_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops,
		struct mtk_lcm_params_dsi *params,
		const struct mtk_panel_cust *cust)
{
	char mode_name[128] = {0};
	struct mtk_lcm_mode_dsi *mode_node;
	int i = 0, ret = 0;

	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, %d: invalid params\n", __func__, __LINE__);
		return;
	}

	DDPDUMP("=========== LCM DUMP of DSI ops:0x%lx-0x%lx flag_len:%u ==============\n",
		(unsigned long)ops, (unsigned long)params,
		ops->flag_len);
	dump_lcm_ops_table(&ops->prepare, cust, "prepare");
	dump_lcm_ops_table(&ops->unprepare, cust, "unprepare");
	dump_lcm_ops_table(&ops->enable, cust, "enable");
	dump_lcm_ops_table(&ops->disable, cust, "disable");
	dump_lcm_ops_table(&ops->set_backlight_cmdq,
		cust, "set_backlight_cmdq");
	dump_lcm_ops_table(&ops->set_elvss_cmdq,
		cust, "set_elvss_cmdq");
	dump_lcm_ops_table(&ops->set_backlight_elvss_cmdq,
		cust, "set_backlight_elvss_cmdq");

	/*dump ata check*/
	DDPDUMP("ata_id_value_length=%u\n",
		ops->ata_id_value_length);
	DDPDUMP("ata_id_value_data:");
	for (i = 0; i < ops->ata_id_value_length; i += 4)
		DDPDUMP("[ata_id_value_data+%u]>>> 0x%x, 0x%x, 0x%x, 0x%x\n",
			i, ops->ata_id_value_data[i],
			ops->ata_id_value_data[i + 1],
			ops->ata_id_value_data[i + 2],
			ops->ata_id_value_data[i + 3]);
	dump_lcm_ops_table(&ops->ata_check, cust, "ata_check");

	/*dump aod mode check*/
	DDPDUMP("aod_mode_value_length=%u\n",
		ops->aod_mode_value_length);
	DDPDUMP("aod_mode_value_data:");
	for (i = 0; i < ops->aod_mode_value_length; i += 4)
		DDPDUMP("[aod_mode_value_data+%u]>>> 0x%x, 0x%x, 0x%x, 0x%x\n",
			i, ops->aod_mode_value_data[i],
			ops->aod_mode_value_data[i + 1],
			ops->aod_mode_value_data[i + 2],
			ops->aod_mode_value_data[i + 3]);
	dump_lcm_ops_table(&ops->aod_mode_check, cust, "aod_mode_check");

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	DDPDUMP("compare_id_value_length=%u\n",
		ops->compare_id_value_length);
	DDPDUMP("compare_id_value_data=",
	for (i = 0; i < ops->compare_id_value_length; i += 4)
		DDPDUMP("[compare_id_value_data+%u]>>> 0x%x, 0x%x, 0x%x, 0x%x\n",
			i, ops->compare_id_value_data[i],
			ops->compare_id_value_data[i + 1],
			ops->compare_id_value_data[i + 2],
			ops->compare_id_value_data[i + 3]);
	DDPDUMP("\n");
	dump_lcm_ops_table(&ops->compare_id, cust, "compare_id");
#endif

	dump_lcm_ops_table(&ops->doze_enable_start,
		cust, "doze_enable_start");
	dump_lcm_ops_table(&ops->doze_enable, cust, "doze_enable");
	dump_lcm_ops_table(&ops->doze_disable, cust, "doze_disable");
	dump_lcm_ops_table(&ops->doze_area, cust, "doze_area");
	dump_lcm_ops_table(&ops->doze_post_disp_on,
		cust, "doze_post_disp_on");
	dump_lcm_ops_table(&ops->set_aod_light, cust, "set_aod_light");

	DDPDUMP("hbm_set_cmdq_switch: on=0x%x,off=0x%x\n",
		ops->hbm_set_cmdq_switch_on,
		ops->hbm_set_cmdq_switch_off);
	dump_lcm_ops_table(&ops->hbm_set_cmdq, cust, "hbm_set_cmdq");
	dump_lcm_ops_table(&ops->msync_request_mte, cust, "msync_request_mte");
	dump_lcm_ops_table(&ops->default_msync_close_mte, cust, "msync_close_mte");
	dump_lcm_ops_table(&ops->msync_default_mte, cust, "msync_default_mte");

	DDPDUMP("read_panelid_len: %u\n", ops->read_panelid_len);
	dump_lcm_ops_table(&ops->read_panelid, cust, "read_panelid");

	/* dump msync switch mte default ops*/
	if (ops->msync_switch_mte_mode != NULL) {
		for (i = 0; i < ops->msync_switch_mte_mode_count;
			i += 4) {
			DDPDUMP("msync_switch_mte_mode[%d]: %d %d %d %d\n", i,
				ops->msync_switch_mte_mode[i],
				ops->msync_switch_mte_mode[i + 1],
				ops->msync_switch_mte_mode[i + 2],
				ops->msync_switch_mte_mode[i + 3]);
		}
	}
	dump_lcm_ops_table(&ops->default_msync_switch_mte,
			cust, "default_msync_switch_mte");

	/* dump fps switch default ops*/
	if (ops->fps_switch_bfoff_mode != NULL) {
		for (i = 0; i < ops->fps_switch_bfoff_mode_count;
			i += 4) {
			DDPDUMP("fps_switch_bfoff_mode: %d %d %d %d\n",
				ops->fps_switch_bfoff_mode[i],
				ops->fps_switch_bfoff_mode[i + 1],
				ops->fps_switch_bfoff_mode[i + 2],
				ops->fps_switch_bfoff_mode[i + 3]);
		}
	}
	dump_lcm_ops_table(&ops->default_fps_switch_bfoff,
			cust, "default_fps_switch_bfoff");

	if (ops->fps_switch_afon_mode != NULL) {
		for (i = 0; i < ops->fps_switch_afon_mode_count;
			i += 4) {
			DDPDUMP("fps_switch_afon_mode: %d %d %d %d\n",
				ops->fps_switch_afon_mode[i],
				ops->fps_switch_afon_mode[i + 1],
				ops->fps_switch_afon_mode[i + 2],
				ops->fps_switch_afon_mode[i + 3]);
		}
	}
	dump_lcm_ops_table(&ops->default_fps_switch_afon,
			cust, "default_fps_switch_afon");
	if (params != NULL && params->mode_count > 0) {
		list_for_each_entry(mode_node, &params->mode_list, list) {
			/* dump fps switch private ops*/
			ret = snprintf(mode_name, sizeof(mode_name),
				 "fps_switch_bfoff_%u_%u_%u_%u",
				 mode_node->id, mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			dump_lcm_ops_table(&mode_node->fps_switch_bfoff,
				cust, mode_name);

			ret = snprintf(mode_name, sizeof(mode_name),
				 "fps_switch_afon_%u_%u_%u_%u",
				 mode_node->id, mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			dump_lcm_ops_table(&mode_node->fps_switch_afon,
				cust, mode_name);

			/* dump msync switch mte private ops*/
			ret = snprintf(mode_name, sizeof(mode_name),
				 "msync_switch_mte_%u_%u_%u_%u",
				 mode_node->id, mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			dump_lcm_ops_table(&mode_node->msync_switch_mte,
				cust, mode_name);

			/* dump msync set min fps ops*/
			ret = snprintf(mode_name, sizeof(mode_name),
				 "msync_set_min_fps_%u_%u_%u_%u",
				 mode_node->id, mode_node->width,
				 mode_node->height, mode_node->fps);
			if (ret < 0 || (size_t)ret >= sizeof(mode_name))
				DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			for (i = 0; i < mode_node->msync_set_min_fps_list_length; i += 4) {
				DDPDUMP("%s-[%d]:%d,%d,%d,%d\n", mode_name, i,
					mode_node->msync_set_min_fps_list[i],
					mode_node->msync_set_min_fps_list[i + 1],
					mode_node->msync_set_min_fps_list[i + 2],
					mode_node->msync_set_min_fps_list[i + 3]);
			}
			dump_lcm_ops_table(&mode_node->msync_set_min_fps,
					cust, mode_name);
		}
	}

	if (cust != NULL &&
		cust->dump_ops_table != NULL) {
		cust->dump_ops_table(__func__, MTK_LCM_FUNC_DSI);
	}

	DDPDUMP("=============================================\n");
}
EXPORT_SYMBOL(dump_lcm_ops_dsi);

void free_lcm_params_dsi_round_corner(
		struct mtk_panel_params *ext_param)
{
	if (IS_ERR_OR_NULL(ext_param) ||
		ext_param->round_corner_en == 0 ||
		mtk_lcm_rc_need_free() == false)
		return;

	if (ext_param->corner_pattern_lt_addr != NULL &&
	    ext_param->corner_pattern_tp_size > 0) {
		LCM_KFREE(ext_param->corner_pattern_lt_addr,
				ext_param->corner_pattern_tp_size + 1);
		ext_param->corner_pattern_tp_size = 0;
		ext_param->corner_pattern_lt_addr = NULL;
	}

	if (ext_param->corner_pattern_lt_addr_l != NULL &&
	    ext_param->corner_pattern_tp_size_l > 0) {
		LCM_KFREE(ext_param->corner_pattern_lt_addr_l,
				ext_param->corner_pattern_tp_size_l + 1);
		ext_param->corner_pattern_tp_size_l = 0;
		ext_param->corner_pattern_lt_addr_l = NULL;
	}

	if (ext_param->corner_pattern_lt_addr_r != NULL &&
	    ext_param->corner_pattern_tp_size_r > 0) {
		LCM_KFREE(ext_param->corner_pattern_lt_addr_r,
				ext_param->corner_pattern_tp_size_r + 1);
		ext_param->corner_pattern_tp_size_r = 0;
		ext_param->corner_pattern_lt_addr_r = NULL;
	}
}

void free_lcm_params_dsi(struct mtk_lcm_params_dsi *params,
	const struct mtk_panel_cust *cust)
{
	struct mtk_lcm_mode_dsi *mode_node = NULL, *tmp = NULL;
	struct mtk_panel_spr_params *spr_params = NULL;

	if (IS_ERR_OR_NULL(params) || params->mode_count == 0) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return;
	}

	list_for_each_entry_safe(mode_node, tmp, &params->mode_list, list) {
		/* free fps switch ops*/
		if (mode_node->fps_switch_bfoff.size > 0) {
			free_lcm_ops_table(&mode_node->fps_switch_bfoff, cust);
			mode_node->fps_switch_bfoff.size = 0;
		}
		if (mode_node->fps_switch_afon.size > 0) {
			free_lcm_ops_table(&mode_node->fps_switch_afon, cust);
			mode_node->fps_switch_afon.size = 0;
		}

		/* free msync switch mte ops*/
		if (mode_node->msync_switch_mte.size > 0) {
			free_lcm_ops_table(&mode_node->msync_switch_mte, cust);
			mode_node->msync_switch_mte.size = 0;
		}

		/* free msync set min fps ops*/
		if (mode_node->msync_set_min_fps_list != NULL) {
			LCM_KFREE(mode_node->msync_set_min_fps_list,
				mode_node->msync_set_min_fps_list_length + 1);
			mode_node->msync_set_min_fps_list = NULL;
			mode_node->msync_set_min_fps_list_length = 0;
		}
		if (mode_node->msync_set_min_fps.size > 0) {
			free_lcm_ops_table(&mode_node->msync_set_min_fps, cust);
			mode_node->msync_set_min_fps.size = 0;
		}

		/* free spr ip params*/
		spr_params = &mode_node->ext_param.spr_params;
		if (spr_params->spr_ip_params_len > 0 &&
			spr_params->spr_ip_params != NULL) {
			LCM_KFREE(spr_params->spr_ip_params,
				spr_params->spr_ip_params_len + 1);
			spr_params->spr_ip_params = NULL;
			spr_params->spr_ip_params_len = 0;
		}

		/* free round corner params*/
		free_lcm_params_dsi_round_corner(&mode_node->ext_param);

		list_del(&mode_node->list);
		LCM_KFREE(mode_node, sizeof(struct mtk_lcm_mode_dsi));
	}
	params->default_mode = NULL;
	memset(params, 0, sizeof(struct mtk_lcm_params_dsi));

	DDPMSG("%s: LCM free dsi params:0x%lx\n",
		__func__, (unsigned long)params);
}
EXPORT_SYMBOL(free_lcm_params_dsi);

void free_lcm_ops_dsi(struct mtk_lcm_ops_dsi *ops,
	const struct mtk_panel_cust *cust)
{
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s:%d, ERROR: invalid params/ops\n",
			__FILE__, __LINE__);
		return;
	}

	DDPMSG("%s: LCM free dsi ops:0x%lx\n",
		__func__, (unsigned long)ops);

	free_lcm_ops_table(&ops->prepare, cust);
	free_lcm_ops_table(&ops->unprepare, cust);

#ifdef MTK_PANEL_SUPPORT_COMPARE_ID
	if (ops->compare_id_value_length > 0 &&
	    ops->compare_id_value_data != NULL) {
		LCM_KFREE(ops->compare_id_value_data,
				sizeof(ops->compare_id_value_data));
		ops->compare_id_value_length = 0;
		ops->compare_id_value_data = NULL;
	}

	free_lcm_ops_table(&ops->compare_id, cust);
#endif

	free_lcm_ops_table(&ops->set_backlight_cmdq, cust);
	free_lcm_ops_table(&ops->set_elvss_cmdq, cust);
	free_lcm_ops_table(&ops->set_backlight_elvss_cmdq, cust);
	free_lcm_ops_table(&ops->set_aod_light, cust);
	free_lcm_ops_table(&ops->doze_enable, cust);
	free_lcm_ops_table(&ops->doze_disable, cust);
	free_lcm_ops_table(&ops->doze_enable_start, cust);
	free_lcm_ops_table(&ops->doze_area, cust);
	free_lcm_ops_table(&ops->doze_post_disp_on, cust);
	free_lcm_ops_table(&ops->hbm_set_cmdq, cust);
	free_lcm_ops_table(&ops->msync_request_mte, cust);
	free_lcm_ops_table(&ops->default_msync_close_mte, cust);
	free_lcm_ops_table(&ops->msync_default_mte, cust);
	free_lcm_ops_table(&ops->read_panelid, cust);

	if (ops->msync_switch_mte_mode != NULL) {
		LCM_KFREE(ops->msync_switch_mte_mode,
			ops->msync_switch_mte_mode_count + 1);
		ops->msync_switch_mte_mode = NULL;
		ops->msync_switch_mte_mode_count = 0;
	}
	free_lcm_ops_table(&ops->default_msync_switch_mte, cust);

	if (ops->fps_switch_bfoff_mode != NULL) {
		LCM_KFREE(ops->fps_switch_bfoff_mode,
			ops->fps_switch_bfoff_mode_count + 1);
		ops->fps_switch_bfoff_mode = NULL;
	}
	free_lcm_ops_table(&ops->default_fps_switch_bfoff, cust);

	if (ops->fps_switch_afon_mode != NULL) {
		LCM_KFREE(ops->fps_switch_afon_mode,
			ops->fps_switch_afon_mode_count + 1);
		ops->fps_switch_afon_mode = NULL;
		ops->fps_switch_afon_mode_count = 0;
	}
	free_lcm_ops_table(&ops->default_fps_switch_afon, cust);

	/* free ata check table */
	if (ops->ata_id_value_length > 0 &&
	    ops->ata_id_value_data != NULL) {
		LCM_KFREE(ops->ata_id_value_data,
				ops->ata_id_value_length + 1);
		ops->ata_id_value_length = 0;
		ops->ata_id_value_data = NULL;
	}
	free_lcm_ops_table(&ops->ata_check, cust);

	/* free aod check table */
	if (ops->aod_mode_value_length > 0 &&
	    ops->aod_mode_value_data != NULL) {
		LCM_KFREE(ops->aod_mode_value_data,
				ops->aod_mode_value_length + 1);
		ops->aod_mode_value_length = 0;
		ops->aod_mode_value_data = NULL;
	}
	free_lcm_ops_table(&ops->aod_mode_check, cust);

	LCM_KFREE(ops, sizeof(struct mtk_lcm_ops_dsi));
}
EXPORT_SYMBOL(free_lcm_ops_dsi);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dsi helper");
MODULE_LICENSE("GPL v2");

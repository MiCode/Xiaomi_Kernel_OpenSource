/*
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <core/common/dsi/dsi_config.h>
#include <core/common/dsi/dsi_panel.h>
#include <core/intel_dc_config.h>

#ifndef CONFIG_ADF_INTEL_VLV
static void dsi_regs_init(struct dsi_config *config, int idx)
{
	struct dsi_registers *regs;

	regs = &config->regs;

	regs->vgacntr_reg = VGACNTRL;
	regs->dpll_reg = DPLL_CTRL_A;

	regs->ovaadd_reg = OVAADD;
	regs->ovcadd_reg = OVCADD;
	regs->ddl1_reg = DDL1;
	regs->ddl2_reg = DDL2;
	regs->ddl3_reg = DDL3;
	regs->ddl4_reg = DDL4;
	regs->histogram_intr_ctrl_reg = HIST_THRESHOLD_GD;
	regs->histogram_logic_ctrl_reg = A_BLK_HISTOGRAM_LOGIC_CTRL;
	regs->aimg_enhance_bin_reg = A_IMG_ENHANCE_BIN;

	switch (idx) {
	case 0:
		regs->dspcntr_reg = DSPACNTR;
		regs->dspsize_reg = DSPASIZE;
		regs->dspsurf_reg = DSPASURF;
		regs->dsplinoff_reg = DSPALINOFF;
		regs->dsppos_reg = DSPAPOS;
		regs->dspstride_reg = DSPASTRIDE;
		regs->color_coef_reg = PIPEA_COLOR_COEF0;
		regs->htotal_reg = HTOTAL_A;
		regs->hblank_reg = HBLANK_A;
		regs->hsync_reg = HSYNC_A;
		regs->vtotal_reg = VTOTAL_A;
		regs->vblank_reg = VBLANK_A;
		regs->vsync_reg = VSYNC_A;
		regs->pipesrc_reg = SRCSZ_A;
		regs->pipeconf_reg = PIPEACONF;
		regs->pipestat_reg = PIPEASTAT;
		regs->pipeframehigh_reg = PIPEAFRAMEHIGH;
		regs->pipeframepixel_reg = PIPEAFRAMEPIXEL;
		regs->mipi_reg = MIPIA_PORT_CTRL;
		regs->palette_reg = PIPE_A_PALETTE;
		regs->gamma_red_max_reg = GAMMA_RED_MAX_A;
		regs->gamma_green_max_reg = GAMMA_GREEN_MAX_A;
		regs->gamma_blue_max_reg = GAMMA_BLUE_MAX_A;

		regs->device_ready_reg = MIPIA_DEVICE_READY;
		regs->intr_stat_reg = MIPIA_INTR_STAT_REG;
		regs->intr_en_reg = MIPIA_INTR_EN_REG;
		regs->dsi_func_prg_reg = MIPIA_DSI_FUNC_PRG_REG;
		regs->hs_tx_timeout_reg = MIPIA_HS_TX_TIMEOUT_REG;
		regs->lp_rx_timeout_reg = MIPIA_LP_RX_TIMEOUT_REG;
		regs->turn_around_timeout_reg = MIPIA_TURN_AROUND_TIMEOUT_REG;
		regs->device_reset_timer_reg = MIPIA_DEVICE_RESET_TIMER;
		regs->dpi_resolution_reg = MIPIA_DPI_RESOLUTION_REG;
		regs->hsync_count_reg = MIPIA_HORIZ_SYNC_PADDING_CNT;
		regs->hbp_count_reg = MIPIA_HORIZ_BACK_PORCH_CNT;
		regs->hfp_count_reg = MIPIA_HORIZ_FRONT_PORCH_CNT;
		regs->hactive_count_reg = MIPIA_HORIZ_ACTIVE_AREA_CNT;
		regs->vsync_count_reg = MIPIA_VERT_SYNC_PADDING_CNT;
		regs->vbp_count_reg = MIPIA_VERT_BACK_PORCH_CNT;
		regs->vfp_count_reg = MIPIA_VERT_FRONT_PORCH_CNT;
		regs->high_low_switch_count_reg = MIPIA_HIGH_LOW_SWITCH_CNT;
		regs->dpi_control_reg = MIPIA_DPI_CTRL_REG;
		regs->dpi_data_reg = MIPIA_DPI_DATA_REG;
		regs->init_count_reg = MIPIA_INIT_COUNT_REG;
		regs->max_return_pack_size_reg = MIPIA_MAX_RETURN_PKT_SIZE_REG;
		regs->video_mode_format_reg = MIPIA_VIDEO_MODE_FORMAT_REG;
		regs->eot_disable_reg = MIPIA_EOT_DISABLE_REG;
		regs->lp_byteclk_reg = MIPIA_LP_BYTECLK_REG;
		regs->lp_gen_data_reg = MIPIA_LP_GEN_DATA_REG;
		regs->hs_gen_data_reg = MIPIA_HS_GEN_DATA_REG;
		regs->lp_gen_ctrl_reg = MIPIA_LP_GEN_CTRL_REG;
		regs->hs_gen_ctrl_reg = MIPIA_HS_GEN_CTRL_REG;
		regs->gen_fifo_stat_reg = MIPIA_GEN_FIFO_STAT_REG;
		regs->hs_ls_dbi_enable_reg = MIPIA_HS_LS_DBI_ENABLE_REG;
		regs->dphy_param_reg = MIPIA_DPHY_PARAM_REG;
		regs->dbi_bw_ctrl_reg = MIPIA_DBI_BW_CTRL_REG;
		regs->clk_lane_switch_time_cnt_reg =
					MIPIA_CLK_LANE_SWITCHING_TIME_CNT;
		regs->mipi_control_reg = MIPIA_CTRL;
		regs->mipi_data_addr_reg = MIPIA_DATA_ADD;
		regs->mipi_data_len_reg = MIPIA_DATA_LEN;
		regs->mipi_cmd_addr_reg = MIPIA_CMD_ADD;
		regs->mipi_cmd_len_reg = MIPIA_CMD_LEN;
		break;
	case 2:
		regs->dspcntr_reg = DSPCCNTR;
		regs->dspsize_reg = DSPCSIZE;
		regs->dspsurf_reg = DSPCSURF;
		regs->dsplinoff_reg = DSPCLINOFF;
		regs->dsppos_reg = DSPCPOS;
		regs->dspstride_reg = DSPCSTRIDE;
		regs->color_coef_reg = PIPEC_COLOR_COEF0;
		regs->htotal_reg = HTOTAL_C;
		regs->hblank_reg = HBLANK_C;
		regs->hsync_reg = HSYNC_C;
		regs->vtotal_reg = VTOTAL_C;
		regs->vblank_reg = VBLANK_C;
		regs->vsync_reg = VSYNC_C;
		regs->pipesrc_reg = SRCSZ_C;
		regs->pipeconf_reg = PIPECCONF;
		regs->pipestat_reg = PIPECSTAT;
		regs->pipeframehigh_reg = PIPECFRAMEHIGH;
		regs->pipeframepixel_reg = PIPECFRAMEPIXEL;
		regs->mipi_reg = MIPIC_PORT_CTRL;
		regs->palette_reg = PIPE_C_PALETTE;
		regs->gamma_red_max_reg = GAMMA_RED_MAX_C;
		regs->gamma_green_max_reg = GAMMA_GREEN_MAX_C;
		regs->gamma_blue_max_reg = GAMMA_BLUE_MAX_C;

		regs->device_ready_reg = MIPIC_DEVICE_READY;
		regs->intr_stat_reg = MIPIC_INTR_STAT_REG;
		regs->intr_en_reg = MIPIC_INTR_EN_REG;
		regs->dsi_func_prg_reg = MIPIC_DSI_FUNC_PRG_REG;
		regs->hs_tx_timeout_reg = MIPIC_HS_TX_TIMEOUT_REG;
		regs->lp_rx_timeout_reg = MIPIC_LP_RX_TIMEOUT_REG;
		regs->turn_around_timeout_reg = MIPIC_TURN_AROUND_TIMEOUT_REG;
		regs->device_reset_timer_reg = MIPIC_DEVICE_RESET_TIMER;
		regs->dpi_resolution_reg = MIPIC_DPI_RESOLUTION_REG;
		regs->hsync_count_reg = MIPIC_HORIZ_SYNC_PADDING_CNT;
		regs->hbp_count_reg = MIPIC_HORIZ_BACK_PORCH_CNT;
		regs->hfp_count_reg = MIPIC_HORIZ_FRONT_PORCH_CNT;
		regs->hactive_count_reg = MIPIC_HORIZ_ACTIVE_AREA_CNT;
		regs->vsync_count_reg = MIPIC_VERT_SYNC_PADDING_CNT;
		regs->vbp_count_reg = MIPIC_VERT_BACK_PORCH_CNT;
		regs->vfp_count_reg = MIPIC_VERT_FRONT_PORCH_CNT;
		regs->high_low_switch_count_reg = MIPIC_HIGH_LOW_SWITCH_CNT;
		regs->dpi_control_reg = MIPIC_DPI_CTRL_REG;
		regs->dpi_data_reg = MIPIC_DPI_DATA_REG;
		regs->init_count_reg = MIPIC_INIT_COUNT_REG;
		regs->max_return_pack_size_reg = MIPIC_MAX_RETURN_PKT_SIZE_REG;
		regs->video_mode_format_reg = MIPIC_VIDEO_MODE_FORMAT_REG;
		regs->eot_disable_reg = MIPIC_EOT_DISABLE_REG;
		regs->lp_byteclk_reg = MIPIC_LP_BYTECLK_REG;
		regs->lp_gen_data_reg = MIPIC_LP_GEN_DATA_REG;
		regs->hs_gen_data_reg = MIPIC_HS_GEN_DATA_REG;
		regs->lp_gen_ctrl_reg = MIPIC_LP_GEN_CTRL_REG;
		regs->hs_gen_ctrl_reg = MIPIC_HS_GEN_CTRL_REG;
		regs->gen_fifo_stat_reg = MIPIC_GEN_FIFO_STAT_REG;
		regs->hs_ls_dbi_enable_reg = MIPIC_HS_LS_DBI_ENABLE_REG;
		regs->dphy_param_reg = MIPIC_DPHY_PARAM_REG;
		regs->dbi_bw_ctrl_reg = MIPIC_DBI_BW_CTRL_REG;
		regs->clk_lane_switch_time_cnt_reg =
					MIPIC_CLK_LANE_SWITCHING_TIME_CNT;
		regs->mipi_control_reg = MIPIC_CTRL;
		regs->mipi_data_addr_reg = MIPIC_DATA_ADD;
		regs->mipi_data_len_reg = MIPIC_DATA_LEN;
		regs->mipi_cmd_addr_reg = MIPIC_CMD_ADD;
		regs->mipi_cmd_len_reg = MIPIC_CMD_LEN;
		break;
	default:
		pr_err("%s: invalid index %d\n", __func__, idx);
		return;
	}
}

#endif

int dsi_config_init(struct dsi_config *config,
			struct dsi_panel *panel, u8 idx)
{
	int err = 0;
	struct panel_info pi;
	struct drm_mode_modeinfo mode;

	pr_info("ADF: %s\n", __func__);
	if (!config || !panel) {
		pr_err("%s: invalid parameter\n", __func__);
		err = -EINVAL;
		goto out_err0;
	}

	if (!panel->ops) {
		pr_err("%s: no panel ops found\n", __func__);
		err = -EINVAL;
		goto out_err0;
	}

	if (!panel->ops->get_panel_info) {
		pr_err("%s: failed to get panel info\n", __func__);
		err = -ENODEV;
		goto out_err0;
	}

	if (!panel->ops->get_config_mode) {
		pr_err("%s: panel doesn't have configured mode\n", __func__);
		err = -ENODEV;
		goto out_err0;
	}

	err = panel->ops->get_config_mode(config, &mode);
	if (err) {
		pr_err("%s: failed to get configured mode\n", __func__);
		goto out_err0;
	}

	panel->ops->get_panel_info(config, &pi);

	memcpy(&config->perferred_mode, &mode, sizeof(mode));

	config->pipe = idx;
	config->changed = 0;
	config->drv_ic_inited = 0;
	config->bpp = pi.bpp;
	config->dual_link = pi.dual_link;

#ifndef CONFIG_ADF_INTEL_VLV
	config->channel_num = 0;
	config->enable_gamma_csc = 0;
	config->video_mode = DSI_VIDEO_BURST_MODE;
	config->lane_count = pi.lane_num;
	config->type = pi.dsi_type;

	/*init regs*/
	dsi_regs_init(config, idx);
#endif

	/*init context lock*/
	mutex_init(&config->ctx_lock);

	/*init DSR for DBI panels*/
	/*
	 if (config->type == DSI_DBI) {
		dsi_dsr_init();
	}
	*/
	return 0;
out_err0:
	return err;
}

void dsi_config_destroy(struct dsi_config *config)
{

}

/*
 * Copyright (C) 2015 MediaTek Inc.
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

/* weiping fix */
#if 0
#include <mach/mt_chip.h>
#endif

#include "mt_sd.h"
#include "msdc_hw_ett.h"

#ifdef CONFIG_ARCH_MT6735M
#ifdef MSDC_SUPPORT_SANDISK_COMBO_ETT
struct msdc_ett_settings msdc0_ett_hs200_settings_for_sandisk[] = {
	{ 0xb0,  (0x7 << 7), 0 }, /* PATCH_BIT0[MSDC_PB0_INT_DAT_LATCH_CK_SEL] */
	{ 0xb0,  (0x1f << 10), 0 }, /* PATCH_BIT0[MSDC_PB0_CKGEN_MSDC_DLY_SEL] */

	/* command & resp ett settings */
	{ 0xb4,  (0x7 << 3), 1 }, /* PATCH_BIT1[MSDC_PB1_CMD_RSP_TA_CNTR] */
	{ 0x4,   (0x1 << 1), 1 }, /* MSDC_IOCON[MSDC_IOCON_RSPL] */
	{ 0xf0,  (0x1f << 16), 0 }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRDLY] */
	{ 0xf0,  (0x1f << 22), 6 }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRRDLY] */

	/* write ett settings */
	{ 0xb4,  (0x7 << 0), 1 }, /* PATCH_BIT1[MSDC_PB1_WRDAT_CRCS_TA_CNTR] */
	{ 0xf0,  (0x1f << 0), 15 }, /* PAD_TUNE[MSDC_PAD_TUNE_DATWRDLY] */
	{ 0x4,   (0x1 << 10), 1 }, /* MSDC_IOCON[MSDC_IOCON_W_D0SPL] */
	{ 0xf8,  (0x1f << 24), 5 }, /* DAT_RD_DLY0[MSDC_DAT_RDDLY0_D0] */

	/* read ett settings */
	{ 0xf0,  (0x1f << 8), 18}, /* PAD_TUNE[MSDC_PAD_TUNE_DATRRDLY] */
	{ 0x4,   (0x1 << 2), 3 }, /* MSDC_IOCON[MSDC_IOCON_R_D_SMPL] */
};
struct msdc_ett_settings msdc0_ett_hs400_settings_for_sandisk[] = {
	{ 0xb0,  (0x7 << 7), 0 }, /* PATCH_BIT0[MSDC_PB0_INT_DAT_LATCH_CK_SEL] */
	{ 0xb0,  (0x1f << 10), 0 }, /* PATCH_BIT0[MSDC_PB0_CKGEN_MSDC_DLY_SEL] */
	{ 0x188, (0x1f << 2), 2 /*0x0*/ }, /* EMMC50_PAD_DS_TUNE[MSDC_EMMC50_PAD_DS_TUNE_DLY1] */
	{ 0x188, (0x1f << 12), 18 /*0x13*/}, /* EMMC50_PAD_DS_TUNE[MSDC_EMMC50_PAD_DS_TUNE_DLY3] */

	{ 0xb4,  (0x7 << 3), 1 }, /* PATCH_BIT1[MSDC_PB1_CMD_RSP_TA_CNTR] */
	{ 0x4,   (0x1 << 1), 1 }, /* MSDC_IOCON[MSDC_IOCON_RSPL] */
	{ 0xf0,  (0x1f << 16), 0 }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRDLY] */
	{ 0xf0,  (0x1f << 22), 11 /*0x0*/ }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRRDLY] */
};
#endif /* mt6735m MSDC_SUPPORT_SANDISK_COMBO_ETT */
#ifdef MSDC_SUPPORT_SAMSUNG_COMBO_ETT
struct msdc_ett_settings msdc0_ett_hs200_settings_for_samsung[] = {
	{ 0xb0,  (0x7 << 7), 0 }, /* PATCH_BIT0[MSDC_PB0_INT_DAT_LATCH_CK_SEL] */
	{ 0xb0,  (0x1f << 10), 0 }, /* PATCH_BIT0[MSDC_PB0_CKGEN_MSDC_DLY_SEL] */

	/* command & resp ett settings */
	{ 0xb4,  (0x7 << 3), 1 }, /* PATCH_BIT1[MSDC_PB1_CMD_RSP_TA_CNTR] */
	{ 0x4,   (0x1 << 1), 1 }, /* MSDC_IOCON[MSDC_IOCON_RSPL] */
	{ 0xf0,  (0x1f << 16), 0 }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRDLY] */
	{ 0xf0,  (0x1f << 22), 6 }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRRDLY] */

	/* write ett settings */
	{ 0xb4,  (0x7 << 0), 1 }, /* PATCH_BIT1[MSDC_PB1_WRDAT_CRCS_TA_CNTR] */
	{ 0xf0,  (0x1f << 0), 15 }, /* PAD_TUNE[MSDC_PAD_TUNE_DATWRDLY] */
	{ 0x4,   (0x1 << 10), 1 }, /* MSDC_IOCON[MSDC_IOCON_W_D0SPL] */
	{ 0xf8,  (0x1f << 24), 5 }, /* DAT_RD_DLY0[MSDC_DAT_RDDLY0_D0] */

	/* read ett settings */
	{ 0xf0,  (0x1f << 8), 18}, /* PAD_TUNE[MSDC_PAD_TUNE_DATRRDLY] */
	{ 0x4,   (0x1 << 2), 4 }, /* MSDC_IOCON[MSDC_IOCON_R_D_SMPL] */
};

struct msdc_ett_settings msdc0_ett_hs400_settings_for_samsung[] = {
	{ 0xb0,  (0x7 << 7), 0 }, /* PATCH_BIT0[MSDC_PB0_INT_DAT_LATCH_CK_SEL] */
	{ 0xb0,  (0x1f << 10), 0 }, /* PATCH_BIT0[MSDC_PB0_CKGEN_MSDC_DLY_SEL] */
	{ 0x188, (0x1f << 2), 2 /*0x0*/ }, /* EMMC50_PAD_DS_TUNE[MSDC_EMMC50_PAD_DS_TUNE_DLY1] */
	{ 0x188, (0x1f << 12), 18 /*0x13*/}, /* EMMC50_PAD_DS_TUNE[MSDC_EMMC50_PAD_DS_TUNE_DLY3] */

	{ 0xb4,  (0x7 << 3), 1 }, /* PATCH_BIT1[MSDC_PB1_CMD_RSP_TA_CNTR] */
	{ 0x4,   (0x1 << 1), 1 }, /* MSDC_IOCON[MSDC_IOCON_RSPL] */
	{ 0xf0,  (0x1f << 16), 0 }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRDLY] */
	{ 0xf0,  (0x1f << 22), 11 /*0x0*/ }, /* PAD_TUNE[MSDC_PAD_TUNE_CMDRRDLY] */
};
#endif				/* mt6735m MSDC_SUPPORT_SAMSUNG_COMBO_ETT */

#endif

int msdc_setting_parameter(struct msdc_hw *hw, unsigned int *para)
{
	struct tag_msdc_hw_para *msdc_para_hw_datap = (struct tag_msdc_hw_para *)para;

	if (NULL == hw)
		return -1;

	if ((msdc_para_hw_datap != NULL) &&
	    ((msdc_para_hw_datap->host_function == MSDC_SD) || (msdc_para_hw_datap->host_function == MSDC_EMMC)) &&
	    (msdc_para_hw_datap->version == 0x5A01) && (msdc_para_hw_datap->end_flag == 0x5a5a5a5a)) {
		hw->clk_src = msdc_para_hw_datap->clk_src;
		hw->cmd_edge = msdc_para_hw_datap->cmd_edge;
		hw->rdata_edge = msdc_para_hw_datap->rdata_edge;
		hw->wdata_edge = msdc_para_hw_datap->wdata_edge;
		hw->clk_drv = msdc_para_hw_datap->clk_drv;
		hw->cmd_drv = msdc_para_hw_datap->cmd_drv;
		hw->dat_drv = msdc_para_hw_datap->dat_drv;
		hw->rst_drv = msdc_para_hw_datap->rst_drv;
		hw->ds_drv = msdc_para_hw_datap->ds_drv;
		hw->clk_drv_sd_18 = msdc_para_hw_datap->clk_drv_sd_18;
		hw->cmd_drv_sd_18 = msdc_para_hw_datap->cmd_drv_sd_18;
		hw->dat_drv_sd_18 = msdc_para_hw_datap->dat_drv_sd_18;
		hw->clk_drv_sd_18_sdr50 = msdc_para_hw_datap->clk_drv_sd_18_sdr50;
		hw->cmd_drv_sd_18_sdr50 = msdc_para_hw_datap->cmd_drv_sd_18_sdr50;
		hw->dat_drv_sd_18_sdr50 = msdc_para_hw_datap->dat_drv_sd_18_sdr50;
		hw->clk_drv_sd_18_ddr50 = msdc_para_hw_datap->clk_drv_sd_18_ddr50;
		hw->cmd_drv_sd_18_ddr50 = msdc_para_hw_datap->cmd_drv_sd_18_ddr50;
		hw->dat_drv_sd_18_ddr50 = msdc_para_hw_datap->dat_drv_sd_18_ddr50;
		hw->flags = msdc_para_hw_datap->flags;
		hw->data_pins = msdc_para_hw_datap->data_pins;
		hw->data_offset = msdc_para_hw_datap->data_offset;

		hw->ddlsel = msdc_para_hw_datap->ddlsel;
		hw->rdsplsel = msdc_para_hw_datap->rdsplsel;
		hw->wdsplsel = msdc_para_hw_datap->wdsplsel;

		hw->dat0rddly = msdc_para_hw_datap->dat0rddly;
		hw->dat1rddly = msdc_para_hw_datap->dat1rddly;
		hw->dat2rddly = msdc_para_hw_datap->dat2rddly;
		hw->dat3rddly = msdc_para_hw_datap->dat3rddly;
		hw->dat4rddly = msdc_para_hw_datap->dat4rddly;
		hw->dat5rddly = msdc_para_hw_datap->dat5rddly;
		hw->dat6rddly = msdc_para_hw_datap->dat6rddly;
		hw->dat7rddly = msdc_para_hw_datap->dat7rddly;
		hw->datwrddly = msdc_para_hw_datap->datwrddly;
		hw->cmdrrddly = msdc_para_hw_datap->cmdrrddly;
		hw->cmdrddly = msdc_para_hw_datap->cmdrddly;
		hw->host_function = msdc_para_hw_datap->host_function;
		hw->boot = msdc_para_hw_datap->boot;
		hw->cd_level = msdc_para_hw_datap->cd_level;
		return 0;
	}
	return -1;
}

/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/gpio.h>
#include <linux/delay.h>

#include "mtk_sd.h"
#include <mmc/core/core.h>
#include <mmc/core/card.h>
#include "dbg.h"
#include "autok.h"
#include "autok_dvfs.h"

void msdc_sdio_restore_after_resume(struct msdc_host *host)
{
}

void msdc_save_timing_setting(struct msdc_host *host)
{
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base, *base_top;
	int i;

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, hw->cmd_edge);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, hw->rdata_edge);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, hw->wdata_edge);

	// this is for suspend only
	host->saved_para.sdc_cfg = MSDC_READ32(SDC_CFG);
	host->saved_para.iocon = MSDC_READ32(MSDC_IOCON);
	host->saved_para.emmc50_cfg0 = MSDC_READ32(EMMC50_CFG0);

	host->saved_para.pb0 = MSDC_READ32(MSDC_PATCH_BIT0);
	host->saved_para.pb1 = MSDC_READ32(MSDC_PATCH_BIT1);
	host->saved_para.pb2 = MSDC_READ32(MSDC_PATCH_BIT2);
	host->saved_para.sdc_fifo_cfg = MSDC_READ32(SDC_FIFO_CFG);
	host->saved_para.sdc_adv_cfg0 = MSDC_READ32(SDC_ADV_CFG0);


	if (host->base_top) {
		base_top = host->base_top;
		host->saved_para.emmc_top_control
			= MSDC_READ32(EMMC_TOP_CONTROL);
		host->saved_para.emmc_top_cmd
			= MSDC_READ32(EMMC_TOP_CMD);
		host->saved_para.top_emmc50_pad_ctl0
			= MSDC_READ32(TOP_EMMC50_PAD_CTL0);
		host->saved_para.top_emmc50_pad_ds_tune
			= MSDC_READ32(TOP_EMMC50_PAD_DS_TUNE);
		for (i = 0; i < 8; i++) {
			host->saved_para.top_emmc50_pad_dat_tune[i]
				= MSDC_READ32(TOP_EMMC50_PAD_DAT0_TUNE + i * 4);
		}
	} else {
		host->saved_para.pad_tune0 = MSDC_READ32(MSDC_PAD_TUNE0);
		host->saved_para.pad_tune1 = MSDC_READ32(MSDC_PAD_TUNE1);
	}
}

void msdc_set_bad_card_and_remove(struct msdc_host *host)
{
	unsigned long flags;

	if (host == NULL) {
		pr_info("WARN: host is NULL");
		return;
	}
	if (host->card_inserted) {
		host->block_bad_card = 1;
		host->card_inserted = 0;
	}

	if ((host->mmc == NULL) || (host->mmc->card == NULL)) {
		ERR_MSG("WARN: mmc or card is NULL");
		return;
	}

	if (host->mmc->card) {
		spin_lock_irqsave(&host->remove_bad_card, flags);
		mmc_card_set_removed(host->mmc->card);
		spin_unlock_irqrestore(&host->remove_bad_card, flags);

#ifndef CONFIG_GPIOLIB
		ERR_MSG("Cannot get gpio %d level", cd_gpio);
#else
		if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
			ERR_MSG("Schedule mmc_rescan");
			mmc_detect_change(host->mmc, msecs_to_jiffies(200));
		} else
#endif
		{
			/*
			 * prevent from calling device_del with mmcqd/X,
			 * it will cause dead lock
			 */
			ERR_MSG("Schedule msdc_remove_card");
			schedule_delayed_work(&host->remove_card,
				msecs_to_jiffies(200));
		}

		if (host->block_bad_card)
			ERR_MSG(
			"Remove the bad card, block_bad_card=%d, card_inserted=%d",
				host->block_bad_card, host->card_inserted);
	}
}

void msdc_ops_set_bad_card_and_remove(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	msdc_set_bad_card_and_remove(host);
}

void msdc_remove_card(struct work_struct *work)
{
	struct msdc_host *host =
		container_of(work, struct msdc_host, remove_card.work);

	if (!host->mmc || !host->mmc->card) {
		ERR_MSG("WARN: mmc or card is NULL");
		return;
	}

	ERR_MSG("Remove card");
	mmc_claim_host(host->mmc);
	mmc_remove_card(host->mmc->card);
	host->mmc->card = NULL;
	mmc_detach_bus(host->mmc);
	mmc_power_off(host->mmc);
	mmc_release_host(host->mmc);
}

int msdc_data_timeout_cont_chk(struct msdc_host *host)
{
	if ((host->hw->host_function == MSDC_SD) &&
		(host->data_timeout_cont >= MSDC_MAX_DATA_TIMEOUT_CONTINUOUS)) {
		ERR_MSG("force remove bad card, data timeout continuous %d",
			host->data_timeout_cont);
		msdc_set_bad_card_and_remove(host);
		return 1;
	}

	return 0;
}

int emmc_reinit_tuning(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	u32 div = 0;
	u32 mode = 0;
	unsigned int caps_hw_reset = 0;

	if (!mmc->card) {
		pr_notice("mmc card = NULL, skip reset tuning\n");
		return -1;
	}

	/* Switch to DDR/HS mode */
	if (mmc->card->mmc_avail_type
	  & (EXT_CSD_CARD_TYPE_HS200 | EXT_CSD_CARD_TYPE_HS400)) {
		mmc->card->mmc_avail_type &=
			~(EXT_CSD_CARD_TYPE_HS200|EXT_CSD_CARD_TYPE_HS400);
		pr_notice("msdc%d: switch to DDR/HS mode, reinit card\n",
			host->id);
		if (mmc->caps & MMC_CAP_HW_RESET) {
			caps_hw_reset = 1;
		} else {
			caps_hw_reset = 0;
			mmc->caps |= MMC_CAP_HW_RESET;
		}
		mmc->ios.timing = MMC_TIMING_LEGACY;
		mmc->ios.clock = 260000;
		msdc_ops_set_ios(mmc, &mmc->ios);
		host->hs400_mode = false;
		if (mmc_hw_reset(mmc))
			pr_notice("msdc%d fail to switch to DDR/HS mode\n",
				host->id);
		/* restore MMC_CAP_HW_RESET */
		if (!caps_hw_reset)
			mmc->caps &= ~MMC_CAP_HW_RESET;
		goto done;
	}

	/* Reduce to lower frequency */
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, div);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
	div += 1;
	if (div > EMMC_MAX_FREQ_DIV) {
		pr_notice("msdc%d: max lower freq: %d\n", host->id, div);
		return 1;
	}
	msdc_clk_stable(host, mode, div, 0);
	host->sclk = (div == 0) ? host->hclk / 4 : host->hclk / (4 * div);
	pr_notice("msdc%d: reduce frequence to %dMhz\n",
		host->id, host->sclk / 1000000);

done:
	return 0;

}

int sdcard_hw_reset(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;

	int level = 1;

#ifdef CONFIG_GPIOLIB
	level = __gpio_get_value(cd_gpio);
#endif
	host->card_inserted = (host->hw->cd_level == level) ? 1 : 0;

	if (!(host->card_inserted)) {
		pr_notice("card is not inserted!\n");
		msdc_set_bad_card_and_remove(host);
		ret = -1;
		return ret;
	}

	/* power reset sdcard */
	mmc->ios.timing = MMC_TIMING_LEGACY;
	/* do not set same as HOST_MIN_MCLK
	 * or else it will be set as block_bad_card when power off
	 */
	mmc->ios.clock = 300000;
	msdc_ops_set_ios(mmc, &mmc->ios);
	ret = mmc_hw_reset(mmc);
	if (ret) {
		if (++host->power_cycle_cnt
			> MSDC_MAX_POWER_CYCLE_FAIL_CONTINUOUS)
			msdc_set_bad_card_and_remove(host);
		pr_notice(
			"msdc%d power reset (%d) failed, block_bad_card = %d\n",
			host->id, host->power_cycle_cnt, host->block_bad_card);
	} else {
		host->power_cycle_cnt = 0;
		pr_notice("msdc%d power reset success\n", host->id);
	}

	return ret;
}

/* SDcard will change speed mode and power reset
 * UHS card
 *    UHS_SDR104 --> UHS_DDR50 --> UHS_SDR50 --> UHS_SDR25
 * HS card
 *    50MHz --> 25MHz --> 12.5MHz --> 6.25MHz
 */
int sdcard_reset_tuning(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);
	char *remove_cap;
	int ret = 0;

	if (!mmc->card) {
		pr_notice("mmc card = NULL, skip reset tuning\n");
		return -1;
	}

	if (mmc_card_uhs(mmc->card)) {
		if (mmc->card->sw_caps.sd3_bus_mode & SD_MODE_UHS_SDR104) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR104;
			remove_cap = "UHS_SDR104";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_DDR50) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_DDR50;
			remove_cap = "UHS_DDR50";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_SDR50) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR50;
			remove_cap = "UHS_SDR50";
		} else if (mmc->card->sw_caps.sd3_bus_mode
			& SD_MODE_UHS_SDR25) {
			mmc->card->sw_caps.sd3_bus_mode &= ~SD_MODE_UHS_SDR25;
			remove_cap = "UHS_SDR25";
		} else {
			remove_cap = "none";
		}
		pr_notice("msdc%d: remove %s mode then reinit card\n", host->id,
			remove_cap);
	} else if (mmc_card_hs(mmc->card)) {
		if (mmc->card->sw_caps.hs_max_dtr >= HIGH_SPEED_MAX_DTR / 4)
			mmc->card->sw_caps.hs_max_dtr /= 2;
		pr_notice("msdc%d: set hs speed %dhz then reinit card\n",
			host->id, mmc->card->sw_caps.hs_max_dtr);
	} else {
		pr_notice("msdc%d: ds card just reinit card\n", host->id);
	}

	/* force remove card for continuous data timeout */
	ret = msdc_data_timeout_cont_chk(host);
	if (ret) {
		ret = -1;
		goto done;
	}

	/* power cycle sdcard */
	ret = sdcard_hw_reset(mmc);
	if (ret) {
		ret = -1;
		goto done;
	}

done:
	return ret;
}

void msdc_restore_timing_setting(struct msdc_host *host)
{
	void __iomem *base = host->base, *base_top = host->base_top;
	int emmc = (host->hw->host_function == MSDC_EMMC) ? 1 : 0;
	int i;

	autok_path_sel(host);

	MSDC_WRITE32(SDC_CFG, host->saved_para.sdc_cfg);

	MSDC_WRITE32(MSDC_IOCON, host->saved_para.iocon);

	if (!host->base_top) {
		MSDC_WRITE32(MSDC_PAD_TUNE0, host->saved_para.pad_tune0);
		MSDC_WRITE32(MSDC_PAD_TUNE1, host->saved_para.pad_tune1);
	}

	MSDC_WRITE32(MSDC_PATCH_BIT0, host->saved_para.pb0);
	MSDC_WRITE32(MSDC_PATCH_BIT1, host->saved_para.pb1);
	MSDC_WRITE32(MSDC_PATCH_BIT2, host->saved_para.pb2);
	MSDC_WRITE32(SDC_FIFO_CFG, host->saved_para.sdc_fifo_cfg);
	MSDC_WRITE32(SDC_ADV_CFG0, host->saved_para.sdc_adv_cfg0);


	if (emmc && !host->base_top) {
		/* FIX ME: sdio shall add extra check for sdio3.0+ */
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			host->saved_para.ds_dly1);
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
			host->saved_para.ds_dly3);
		MSDC_WRITE32(EMMC50_PAD_CMD_TUNE,
			host->saved_para.emmc50_pad_cmd_tune);
		MSDC_WRITE32(EMMC50_PAD_DAT01_TUNE,
			host->saved_para.emmc50_dat01);
		MSDC_WRITE32(EMMC50_PAD_DAT23_TUNE,
			host->saved_para.emmc50_dat23);
		MSDC_WRITE32(EMMC50_PAD_DAT45_TUNE,
			host->saved_para.emmc50_dat45);
		MSDC_WRITE32(EMMC50_PAD_DAT67_TUNE,
			host->saved_para.emmc50_dat67);
	}

	if (emmc)
		MSDC_WRITE32(EMMC50_CFG0, host->saved_para.emmc50_cfg0);

	if (host->base_top) {
		MSDC_WRITE32(EMMC_TOP_CONTROL,
			host->saved_para.emmc_top_control);
		MSDC_WRITE32(EMMC_TOP_CMD,
			host->saved_para.emmc_top_cmd);
		MSDC_WRITE32(TOP_EMMC50_PAD_CTL0,
			host->saved_para.top_emmc50_pad_ctl0);
		MSDC_WRITE32(TOP_EMMC50_PAD_DS_TUNE,
			host->saved_para.top_emmc50_pad_ds_tune);
		for (i = 0; i < 8; i++) {
			MSDC_WRITE32(TOP_EMMC50_PAD_DAT0_TUNE + i * 4,
				host->saved_para.top_emmc50_pad_dat_tune[i]);
		}
	}

	if (host->use_hw_dvfs == 1)
		msdc_dvfs_reg_restore(host);
}

void msdc_init_tune_path(struct msdc_host *host, unsigned char timing)
{
	void __iomem *base = host->base, *base_top = host->base_top;

	MSDC_WRITE32(MSDC_PAD_TUNE0, 0x00000000);

	if (host->base_top) {
		/* FIX ME: toggle these fields accroding to timing */
		/* FIX ME: maybe unnecessary if autok can take care */
		MSDC_CLR_BIT32(EMMC_TOP_CONTROL, DATA_K_VALUE_SEL);
		MSDC_CLR_BIT32(EMMC_TOP_CONTROL, DELAY_EN);
		MSDC_CLR_BIT32(EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY);
		MSDC_CLR_BIT32(EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
		MSDC_CLR_BIT32(EMMC_TOP_CONTROL, PAD_RXDLY_SEL);
		MSDC_CLR_BIT32(EMMC_TOP_CMD, PAD_CMD_RXDLY);
		MSDC_CLR_BIT32(EMMC_TOP_CMD, PAD_CMD_RD_RXDLY_SEL);
		MSDC_CLR_BIT32(TOP_EMMC50_PAD_CTL0, PAD_CLK_TXDLY);
	}

	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_DDLSEL);
	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL);
	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_R_D_SMPL);
	if (timing == MMC_TIMING_MMC_HS400) {
		MSDC_CLR_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
		MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
		if (host->base_top) {
			/* FIX ME: maybe unnecessary if autok can take care */
			MSDC_CLR_BIT32(EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
			MSDC_CLR_BIT32(EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2_SEL);
		}
	} else {
		MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
		MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
		if (host->base_top) {
			/* FIX ME: maybe unnecessary if autok can take care */
			MSDC_SET_BIT32(EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY_SEL);
			MSDC_CLR_BIT32(EMMC_TOP_CONTROL, PAD_DAT_RD_RXDLY2_SEL);
		}
	}

	if (timing == MMC_TIMING_MMC_HS400)
		MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);
	else
		MSDC_SET_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);

	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL);

	MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
	MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL);

	if ((timing == MMC_TIMING_MMC_HS400) && (host->base_top)) {
		/* FIX ME: maybe unnecessary if autok can take care */
		MSDC_SET_BIT32(EMMC_TOP_CMD, PAD_CMD_RD_RXDLY_SEL);
		MSDC_CLR_BIT32(EMMC_TOP_CMD, PAD_CMD_RD_RXDLY2_SEL);
	}

	MSDC_CLR_BIT32(EMMC50_CFG0, MSDC_EMMC50_CFG_CMD_RESP_SEL);

	autok_path_sel(host);
}

void msdc_init_tune_setting(struct msdc_host *host)
{
	void __iomem *base = host->base, *base_top = host->base_top;
	u32 val;

	/* FIX ME: check if always convered by autok */
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY,
		MSDC_CLKTXDLY);
	if (host->base_top) {
		MSDC_SET_FIELD(TOP_EMMC50_PAD_CTL0, PAD_CLK_TXDLY,
			MSDC_CLKTXDLY);
		MSDC_SET_FIELD(EMMC_TOP_CONTROL,
			(PAD_DAT_RD_RXDLY2 | PAD_DAT_RD_RXDLY), 0);
		MSDC_SET_FIELD(EMMC_TOP_CMD,
			(PAD_CMD_RXDLY2 | PAD_CMD_RXDLY), 0);
		MSDC_SET_FIELD(TOP_EMMC50_PAD_DS_TUNE,
			(PAD_DS_DLY3 | PAD_DS_DLY2 | PAD_DS_DLY1), 0);
	}

	/* Reserve MSDC_IOCON_DDR50CKD bit, clear all other bits */
	val = MSDC_READ32(MSDC_IOCON) & MSDC_IOCON_DDR50CKD;
	MSDC_WRITE32(MSDC_IOCON, val);

	MSDC_WRITE32(MSDC_DAT_RDDLY0, 0x00000000);
	MSDC_WRITE32(MSDC_DAT_RDDLY1, 0x00000000);

	MSDC_WRITE32(MSDC_PATCH_BIT0, MSDC_PB0_DEFAULT_VAL);
	MSDC_WRITE32(MSDC_PATCH_BIT1, MSDC_PB1_DEFAULT_VAL);

	/* Fix HS400 mode */
	MSDC_CLR_BIT32(EMMC50_CFG0, MSDC_EMMC50_CFG_TXSKEW_SEL);
	MSDC_SET_BIT32(MSDC_PATCH_BIT1, MSDC_PB1_DDR_CMD_FIX_SEL);

	/* DDR50 mode */
	MSDC_SET_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_DDR50SEL);

	/* 64T + 48T cmd <-> resp */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT,
		MSDC_PB2_DEFAULT_RESPWAITCNT);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL,
		MSDC_PB2_DEFAULT_RESPSTENSEL);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL,
		MSDC_PB2_DEFAULT_CRCSTSENSEL);

	/* FIX ME: check if can be moved to msdc_cust.c */
#ifdef CONFIG_MACH_MT6799
	if (CHIP_IS_VER2())
		SET_EMMC50_CFG_END_BIT_CHK_CNT(EMMC50_CFG_END_BIT_CHK_CNT);
#endif

	autok_path_sel(host);
}

void msdc_ios_tune_setting(struct msdc_host *host, struct mmc_ios *ios)
{
	autok_msdc_tx_setting(host, ios);
}

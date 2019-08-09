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

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/gpio.h>
#include <linux/delay.h>

#include "mt_sd.h"
#include <core/core.h>
#include "dbg.h"
#include "autok.h"

/* FIX ME: better low freq trigger condition is continual 2 or more times crc or tmo error */
#define CMD_TUNE_SMPL_MAX_TIME          (4)
#define READ_TUNE_SMPL_MAX_TIME         (4)
#define WRITE_TUNE_SMPL_MAX_TIME        (4)

#define CMD_TUNE_HS_MAX_TIME            (2*63)
#define READ_DATA_TUNE_MAX_TIME         (2*63)
#define WRITE_DATA_TUNE_MAX_TIME        (2*63)

#define MSDC_LOWER_FREQ
#define MSDC_MAX_FREQ_DIV               (2)
#define MSDC_MAX_TIMEOUT_RETRY          (1)
#define MSDC_MAX_TIMEOUT_RETRY_EMMC     (2)
#define MSDC_MAX_W_TIMEOUT_TUNE         (5)
#define MSDC_MAX_W_TIMEOUT_TUNE_EMMC    (64)
#define MSDC_MAX_R_TIMEOUT_TUNE         (3)
#define MSDC_MAX_POWER_CYCLE            (5)

#define MSDC_MAX_CONTINUOUS_FAIL_REQUEST_COUNT (50)

#define MAX_HS400_TUNE_COUNT            (576) /*(32*18)*/

#define CMD_SET_FOR_MMC_TUNE_CASE1      (0x00000000FB260140ULL)
#define CMD_SET_FOR_MMC_TUNE_CASE2      (0x0000000000000080ULL)
#define CMD_SET_FOR_MMC_TUNE_CASE3      (0x0000000000001000ULL)
#define CMD_SET_FOR_MMC_TUNE_CASE4      (0x0000000000000020ULL)
/*#define CMD_SET_FOR_MMC_TUNE_CASE5      (0x0000000000084000ULL)*/

#define CMD_SET_FOR_SD_TUNE_CASE1       (0x000000007B060040ULL)
#define CMD_SET_FOR_APP_TUNE_CASE1      (0x0008000000402000ULL)

#define IS_IN_CMD_SET(cmd_num, set)     ((0x1ULL << cmd_num) & (set))

#define MSDC_VERIFY_NEED_TUNE           (0)
#define MSDC_VERIFY_ERROR               (1)
#define MSDC_VERIFY_NEED_NOT_TUNE       (2)

u8 emmc_id; /* FIX ME: check if it can be removed */

/* FIX ME: check if it can be removed since it is set
 *         but referenced
 */
u32 sdio_tune_flag;

void msdc_reset_pwr_cycle_counter(struct msdc_host *host)
{
	host->power_cycle = 0;
	host->power_cycle_enable = 1;
}

void msdc_reset_tmo_tune_counter(struct msdc_host *host, unsigned int index)
{
	switch (index) {
	case all_counter:
	case cmd_counter:
		if (host->rwcmd_time_tune != 0)
			ERR_MSG("TMO TUNE CMD Times(%d)",
				host->rwcmd_time_tune);
		host->rwcmd_time_tune = 0;
		if (index == cmd_counter)
			break;
	case read_counter:
		if (host->read_time_tune != 0)
			ERR_MSG("TMO TUNE READ Times(%d)",
				host->read_time_tune);
		host->read_time_tune = 0;
		if (index == read_counter)
			break;
	case write_counter:
		if (host->write_time_tune != 0)
			ERR_MSG("TMO TUNE WRITE Times(%d)",
				host->write_time_tune);
		host->write_time_tune = 0;
		break;
	default:
		ERR_MSG("msdc%d ==> reset tmo counter index(%d) error!\n",
			host->id, index);
		break;
	}
}

void msdc_reset_crc_tune_counter(struct msdc_host *host, unsigned int index)
{
	/*void __iomem *base = host->base;*/
	struct tune_counter *t_counter = &host->t_counter;

	switch (index) {
	case all_counter:
		/* FIX ME: This part might be removed if HS400 AUTOK work fine*/
		if (t_counter->time_hs400 != 0)
			ERR_MSG("TUNE HS400 Times(%d)", t_counter->time_hs400);
		t_counter->time_hs400 = 0;
		/* fallthrough*/
	case cmd_counter:
		if (t_counter->time_cmd != 0)
			ERR_MSG("CRC TUNE CMD Times(%d)", t_counter->time_cmd);
		t_counter->time_cmd = 0;
		if (index == cmd_counter)
			break;
	case read_counter:
		if (t_counter->time_read != 0) {
			ERR_MSG("CRC TUNE READ Times(%d)",
				t_counter->time_read);
		}
		t_counter->time_read = 0;
		if (index == read_counter)
			break;
	case write_counter:
		if (t_counter->time_write != 0) {
			ERR_MSG("CRC TUNE WRITE Times(%d)",
				t_counter->time_write);
		}
		t_counter->time_write = 0;
		break;
	default:
		ERR_MSG("msdc%d ==> reset crc counter index(%d) error!\n",
			host->id, index);
		break;
	}
}

void msdc_save_timing_setting(struct msdc_host *host, int save_mode)
{
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;
	/* save_mode: 1 emmc_suspend
	 *	      2 sdio_suspend
	 *	      3 power_tuning
	 *	      4 power_off
	 */

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, hw->cmd_edge);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_R_D_SMPL, hw->rdata_edge);
	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_W_D_SMPL, hw->wdata_edge);

	if ((save_mode == 1) || (save_mode == 2)) {
		host->saved_para.hz = host->mclk;
		host->saved_para.sdc_cfg = MSDC_READ32(SDC_CFG);
	}

	if (((save_mode == 1) || (save_mode == 4)) && (hw->host_function == MSDC_EMMC)) {
		MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
			host->saved_para.ds_dly1);
		MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
			host->saved_para.ds_dly3);
		host->saved_para.emmc50_pad_cmd_tune =
			MSDC_READ32(EMMC50_PAD_CMD_TUNE);
		host->saved_para.emmc50_dat01 =
			MSDC_READ32(EMMC50_PAD_DAT01_TUNE);
		host->saved_para.emmc50_dat23 =
			MSDC_READ32(EMMC50_PAD_DAT23_TUNE);
		host->saved_para.emmc50_dat45 =
			MSDC_READ32(EMMC50_PAD_DAT45_TUNE);
		host->saved_para.emmc50_dat67 =
			MSDC_READ32(EMMC50_PAD_DAT67_TUNE);
	}

	if (save_mode == 1) {
		host->saved_para.timing = host->timing;
		host->saved_para.msdc_cfg = MSDC_READ32(MSDC_CFG);
		host->saved_para.iocon = MSDC_READ32(MSDC_IOCON);
	}

	if (save_mode == 2) {
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, host->saved_para.mode);
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, host->saved_para.div);
		MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			host->saved_para.int_dat_latch_ck_sel);
		MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
			host->saved_para.ckgen_msdc_dly_sel);
		MSDC_GET_FIELD(MSDC_INTEN, MSDC_INT_SDIOIRQ,
			host->saved_para.inten_sdio_irq);
		host->saved_para.msdc_cfg = MSDC_READ32(MSDC_CFG);

		host->saved_para.iocon = MSDC_READ32(MSDC_IOCON);

		host->saved_para.timing = host->timing;
	}

	host->saved_para.pad_tune0 = MSDC_READ32(MSDC_PAD_TUNE0);
	host->saved_para.pad_tune1 = MSDC_READ32(MSDC_PAD_TUNE1);

	host->saved_para.ddly0 = MSDC_READ32(MSDC_DAT_RDDLY0);
	host->saved_para.ddly1 = MSDC_READ32(MSDC_DAT_RDDLY1);

	host->saved_para.pb1 = MSDC_READ32(MSDC_PATCH_BIT1);
	host->saved_para.pb2 = MSDC_READ32(MSDC_PATCH_BIT2);
	/*msdc_dump_register(host);*/
}

void msdc_set_bad_card_and_remove(struct msdc_host *host)
{
	unsigned long flags;

	if (host == NULL) {
		ERR_MSG("WARN: host is NULL");
		return;
	}
	host->card_inserted = 0;

	if ((host->mmc == NULL) || (host->mmc->card == NULL)) {
		ERR_MSG("WARN: mmc or card is NULL");
		return;
	}
	if (host->mmc->card) {
		spin_lock_irqsave(&host->remove_bad_card, flags);
		host->block_bad_card = 1;

		mmc_card_set_removed(host->mmc->card);
		spin_unlock_irqrestore(&host->remove_bad_card, flags);

		if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)
		 && (host->hw->cd_level == __gpio_get_value(cd_gpio))) {
			/* do nothing*/
			/*tasklet_hi_schedule(&host->card_tasklet);*/
		} else {
			mmc_remove_card(host->mmc->card);
			host->mmc->card = NULL;
			mmc_detach_bus(host->mmc);
			mmc_power_off(host->mmc);
		}

		ERR_MSG("Remove the bad card, block_bad_card=%d, card_inserted=%d",
			host->block_bad_card, host->card_inserted);
	}
}

u32 mmc_sd_power_cycle(struct mmc_host *mmc, u32 ocr, struct mmc_card *card)
{
	pr_err("This function is for sd card only!!");
	return 0;
}

/* 0 means pass */
u32 msdc_power_tuning(struct msdc_host *host)
{
	struct mmc_host *mmc = host->mmc;
	struct mmc_card *card;
	struct mmc_request *mrq;
	u32 power_cycle = 0;
	int read_timeout_tune = 0;
	int write_timeout_tune = 0;
	u32 rwcmd_timeout_tune = 0;
	u32 read_timeout_tune_uhs104 = 0;
	u32 write_timeout_tune_uhs104 = 0;
	u32 sw_timeout = 0;
	u32 ret = 1;
	u32 host_err = 0;
	u32 sclk = host->sclk;

	if (!mmc)
		return 1;

	card = mmc->card;
	if (card == NULL) {
		ERR_MSG("mmc->card is NULL");
		return 1;
	}
	/* eMMC first */
#ifdef CONFIG_MTK_EMMC_SUPPORT
	if (mmc_card_mmc(card) && (host->hw->host_function == MSDC_EMMC))
		return 1;
#endif

	if (!host->error_tune_enable)
		return 1;

	if ((host->sd_30_busy > 0)
	 && (host->sd_30_busy <= MSDC_MAX_POWER_CYCLE)) {
		host->power_cycle_enable = 1;
	}

	if (!mmc_card_sd(card) || (host->hw->host_function != MSDC_SD))
		return ret;

	if (host->power_cycle == MSDC_MAX_POWER_CYCLE) {
		if (host->error_tune_enable) {
			ERR_MSG("do disable error tune flow of bad SD card");
			host->error_tune_enable = 0;
		}
		return ret;
	}

	if ((host->power_cycle > MSDC_MAX_POWER_CYCLE) ||
	    (!host->power_cycle_enable))
		return ret;

	/* power cycle */
	ERR_MSG("the %d time, Power cycle start", host->power_cycle);
	spin_unlock(&host->lock);

	if (host->power_control)
		host->power_control(host, 0);

	mdelay(10);

	if (host->power_control)
		host->power_control(host, 1);

	spin_lock(&host->lock);
	/*msdc_save_timing_setting(host, 3);*/
	msdc_init_tune_setting(host);
	if ((sclk > 100000000) && (host->power_cycle >= 1))
		mmc->caps &= ~MMC_CAP_UHS_SDR104;
	if (((sclk <= 100000000) &&
	     ((sclk > 50000000) || (host->timing == MMC_TIMING_UHS_DDR50)))
	 && (host->power_cycle >= 1)) {
		mmc->caps &= ~(MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104
			| MMC_CAP_UHS_DDR50);
	}

	msdc_host_mode[host->id] = mmc->caps;
	msdc_host_mode2[host->id] = mmc->caps2;

	/* clock should set to 260K*/
	mmc->ios.clock = HOST_MIN_MCLK;
	mmc->ios.bus_width = MMC_BUS_WIDTH_1;
	mmc->ios.timing = MMC_TIMING_LEGACY;
	msdc_set_mclk(host, MMC_TIMING_LEGACY, HOST_MIN_MCLK);

	/* re-init the card!*/
	mrq = host->mrq;
	host->mrq = NULL;
	power_cycle = host->power_cycle;
	host->power_cycle = MSDC_MAX_POWER_CYCLE;
	read_timeout_tune = host->read_time_tune;
	write_timeout_tune = host->write_time_tune;
	rwcmd_timeout_tune = host->rwcmd_time_tune;
	read_timeout_tune_uhs104 = host->read_timeout_uhs104;
	write_timeout_tune_uhs104 = host->write_timeout_uhs104;
	sw_timeout = host->sw_timeout;
	host_err = host->error;
	spin_unlock(&host->lock);
	ret = mmc_sd_power_cycle(mmc, card->ocr, card);
	spin_lock(&host->lock);
	host->mrq = mrq;
	host->power_cycle = power_cycle;
	host->read_time_tune = read_timeout_tune;
	host->write_time_tune = write_timeout_tune;
	host->rwcmd_time_tune = rwcmd_timeout_tune;
	if (sclk > 100000000) {
		host->write_timeout_uhs104 = write_timeout_tune_uhs104;
	} else {
		host->read_timeout_uhs104 = 0;
		host->write_timeout_uhs104 = 0;
	}
	host->sw_timeout = sw_timeout;
	host->error = host_err;
	if (!ret)
		host->power_cycle_enable = 0;
	ERR_MSG("the %d time, Power cycle Done, host->error(0x%x), ret(%d)",
		host->power_cycle, host->error, ret);
	(host->power_cycle)++;

	return ret;
}

static int msdc_lower_freq(struct msdc_host *host)
{
	u32 div, mode, hs400_div_dis;
	void __iomem *base = host->base;
	u32 *hclks;

	ERR_MSG("need to lower freq");
	msdc_reset_crc_tune_counter(host, all_counter);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKDIV, div);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD_HS400, hs400_div_dis);

	hclks = msdc_get_hclks(host->id);

	if (div >= MSDC_MAX_FREQ_DIV) {
		ERR_MSG("div<%d> too large, change to power tuning", div);
		return msdc_power_tuning(host);
	} else if ((mode == 3) && (host->id == 0)) {
		mode = 1; /* change to HS200 */
		div = 0;
		while (!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB))
			cpu_relax();
		host->sclk = hclks[host->hw->clk_src] / 2;
	} else if (mode == 1) {
		mode = 0;
		while (!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB))
			cpu_relax();
		host->sclk = (div == 0) ?
			hclks[host->hw->clk_src]/2 :
			hclks[host->hw->clk_src]/(4*div);
	} else {
		while (!(MSDC_READ32(MSDC_CFG) & MSDC_CFG_CKSTB))
			cpu_relax();
		host->sclk = (mode == 2) ?
			hclks[host->hw->clk_src]/(2*4*(div+1)) :
			hclks[host->hw->clk_src]/(4*(div+1));
	}

	ERR_MSG("new div<%d>, mode<%d> new freq.<%dKHz>",
		(mode == 1) ? div : div + 1, mode, host->sclk / 1000);
	return 0;
}

/* FIX ME: remove it if autok works fine */
/*
 *  2013-12-09
 *  HS400 error tune flow of read/write data error
 *  HS400 error tune flow of cmd error is same as eMMC4.5 backward speed mode.
 */
int emmc_hs400_tune_rw(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int cur_ds_dly1 = 0, cur_ds_dly3 = 0;
	int orig_ds_dly1 = 0, orig_ds_dly3 = 0;
	int err = 0;

	if (host->timing != MMC_TIMING_MMC_HS400)
		return err;

	MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY1,
		orig_ds_dly1);
	MSDC_GET_FIELD(EMMC50_PAD_DS_TUNE, MSDC_EMMC50_PAD_DS_TUNE_DLY3,
		orig_ds_dly3);

	cur_ds_dly1 = orig_ds_dly1 - 1;
	cur_ds_dly3 = orig_ds_dly3;
	if (cur_ds_dly1 < 0) {
		cur_ds_dly1 = 17;
		cur_ds_dly3 = orig_ds_dly3 + 1;
		if (cur_ds_dly3 >= 32)
			cur_ds_dly3 = 0;
	}

	if (++host->t_counter.time_hs400 == MAX_HS400_TUNE_COUNT) {
		ERR_MSG("Failed to update EMMC50_PAD_DS_TUNE_DLY, cur_ds_dly3=0x%x, cur_ds_dly1=0x%x",
			cur_ds_dly3, cur_ds_dly1);
#ifdef MSDC_LOWER_FREQ
		err = msdc_lower_freq(host);
#else
		err = 1;
#endif
		goto out;
	} else {
		MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
			MSDC_EMMC50_PAD_DS_TUNE_DLY1, cur_ds_dly1);
		if (cur_ds_dly3 != orig_ds_dly3) {
			MSDC_SET_FIELD(EMMC50_PAD_DS_TUNE,
				MSDC_EMMC50_PAD_DS_TUNE_DLY3, cur_ds_dly3);
		}
		INIT_MSG("HS400_TUNE: orig_ds_dly1<0x%x>, orig_ds_dly3<0x%x>, cur_ds_dly1<0x%x>, cur_ds_dly3<0x%x>",
			orig_ds_dly1, orig_ds_dly3, cur_ds_dly1, cur_ds_dly3);
	}

 out:
	return err;
}

/*
 * register as callback function of WIFI(combo_sdio_register_pm)
 * can called by msdc_drv_suspend/resume too.
 */
void msdc_restore_timing_setting(struct msdc_host *host)
{
	void __iomem *base = host->base;
	int retry = 3;
	int emmc = (host->hw->host_function == MSDC_EMMC) ? 1 : 0;
	int sdio = (host->hw->host_function == MSDC_SDIO) ? 1 : 0;

	if (sdio) {
		msdc_reset_hw(host->id); /* force bit5(BV18SDT) to 0 */
		host->saved_para.msdc_cfg =
			host->saved_para.msdc_cfg & 0xFFFFFFDF;
		MSDC_WRITE32(MSDC_CFG, host->saved_para.msdc_cfg);
	}

	do {
		msdc_set_mclk(host, host->saved_para.timing,
			host->saved_para.hz);
		if ((MSDC_READ32(MSDC_CFG) & 0xFFFFFF9F) ==
		    (host->saved_para.msdc_cfg & 0xFFFFFF9F))
			break;
		ERR_MSG("msdc set_mclk is unstable (cur_cfg=%x, save_cfg=%x, cur_hz=%d, save_hz=%d)",
			MSDC_READ32(MSDC_CFG),
			host->saved_para.msdc_cfg, host->mclk,
			host->saved_para.hz);
	} while (retry--);

	MSDC_WRITE32(SDC_CFG, host->saved_para.sdc_cfg);

	MSDC_WRITE32(MSDC_IOCON, host->saved_para.iocon);

	MSDC_WRITE32(MSDC_PAD_TUNE0, host->saved_para.pad_tune0);
	MSDC_WRITE32(MSDC_PAD_TUNE1, host->saved_para.pad_tune1);

	MSDC_WRITE32(MSDC_DAT_RDDLY0, host->saved_para.ddly0);
	MSDC_WRITE32(MSDC_DAT_RDDLY1, host->saved_para.ddly1);

	MSDC_WRITE32(MSDC_PATCH_BIT1, host->saved_para.pb1);
	MSDC_WRITE32(MSDC_PATCH_BIT2, host->saved_para.pb2);

	if (sdio) {
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_INT_DAT_LATCH_CK_SEL,
			host->saved_para.int_dat_latch_ck_sel);
		MSDC_SET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_CKGEN_MSDC_DLY_SEL,
			host->saved_para.ckgen_msdc_dly_sel);
		MSDC_SET_FIELD(MSDC_INTEN, MSDC_INT_SDIOIRQ,
			host->saved_para.inten_sdio_irq);

		host->mmc->pm_flags |= MMC_PM_KEEP_POWER;
		host->mmc->rescan_entered = 0;
	}

	if (emmc) {
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

	/*msdc_dump_register(host);*/
}

void msdc_init_tune_path(struct msdc_host *host, unsigned char timing)
{
	void __iomem *base = host->base;

	if (host->id == 0)
		MSDC_WRITE32(MSDC_PAD_TUNE0, 0x00000000);
	else
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, 0x7FFFFFF, 0x0000000);

	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_DDLSEL);
	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_R_D_SMPL_SEL);
	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_R_D_SMPL);
	if (timing == MMC_TIMING_MMC_HS400) {
		MSDC_CLR_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
		MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
	} else {
		MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL);
		MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL);
	}

	if (timing == MMC_TIMING_MMC_HS400)
		MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);
	else
		MSDC_SET_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);

	MSDC_CLR_BIT32(MSDC_IOCON, MSDC_IOCON_W_D_SMPL_SEL);

	MSDC_CLR_BIT32(MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
	MSDC_SET_BIT32(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL);
	MSDC_CLR_BIT32(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL);

	MSDC_CLR_BIT32(EMMC50_CFG0, MSDC_EMMC50_CFG_CMD_RESP_SEL);

	/* tune path and related key hw fixed setting should be wrappered follow interface */
	autok_path_sel(host);

}

void msdc_init_tune_setting(struct msdc_host *host)
{
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;

	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY,
		MSDC_CLKTXDLY);
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATWRDLY,
		hw->datwrddly);
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLY,
		hw->cmdrrddly);
	MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY,
		hw->cmdrddly);

	MSDC_WRITE32(MSDC_IOCON, 0x00000000);

	MSDC_WRITE32(MSDC_DAT_RDDLY0, 0x00000000);
	MSDC_WRITE32(MSDC_DAT_RDDLY1, 0x00000000);

	MSDC_WRITE32(MSDC_PATCH_BIT0, 0x403C000F);
	MSDC_WRITE32(MSDC_PATCH_BIT1, 0xFFFE00C9);

	/* 64T + 48T cmd <-> resp */
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPWAITCNT, 3);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_RESPSTENSEL, 0);
	MSDC_SET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CRCSTSENSEL, 0);

	/* low speed mode should be switch data tune path too even if not covered by autok */
	autok_path_sel(host);
}

int msdc_tune_cmdrsp(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 rsmpl;
	u32 dly, dly1, dly2, dly1_sel, dly2_sel;
	u32 clkmode, hs400;

	MSDC_GET_FIELD(MSDC_IOCON, MSDC_IOCON_RSPL, rsmpl);
	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;

	rsmpl++;
	msdc_set_smpl(host, hs400, rsmpl % 2, TYPE_CMD_RESP_EDGE, NULL);

	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL, dly1_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, dly1);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL, dly2_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRDLY2, dly2);

	if (rsmpl >= 2) {
		dly = ((dly1_sel ? dly1 : 0) + (dly2_sel ? dly2 : 0) + 1) % 63;

		dly1_sel = 1;
		if (dly < 32) {
			dly1 = dly;
			dly2_sel = 0;
			dly2 = 0;
		} else {
			dly1 = 31;
			dly2_sel = 1;
			dly2 = dly - 31;
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRRDLYSEL,
			dly1_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRRDLY2SEL,
			dly2_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CMDRDLY, dly1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_CMDRDLY2, dly2);
	}

	++(host->t_counter.time_cmd);
	if (host->t_counter.time_cmd == CMD_TUNE_HS_MAX_TIME) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_cmd = 0;
	}

	INIT_MSG("TUNE_CMD: rsmpl<%d> dly1<%d> dly2<%d> sfreq.<%d>",
		rsmpl & 0x1, dly1, dly2, host->sclk);

	return result;
}

int msdc_tune_read(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 clkmode, hs400, ddr;
	u32 dsmpl;
	u32 dly, dly1, dly2, dly1_sel, dly2_sel;
	int tune_times_max;

	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;
	if (clkmode == 2 || clkmode == 3)
		ddr = 1;
	else
		ddr = 0;

	MSDC_SET_FIELD(MSDC_IOCON, MSDC_IOCON_DDLSEL, 0);
	MSDC_GET_FIELD(MSDC_PATCH_BIT0, MSDC_PB0_RD_DAT_SEL, dsmpl);

	if (host->id != 0 && ddr == 0) {
		dsmpl++;
		msdc_set_smpl(host, hs400, dsmpl % 2, TYPE_READ_DATA_EDGE,
			NULL);
		tune_times_max = READ_DATA_TUNE_MAX_TIME;
	} else {
		dsmpl = 2;
		tune_times_max = READ_DATA_TUNE_MAX_TIME/2;
	}

	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL, dly1_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL, dly2_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);

	if (dsmpl >= 2) {
		dly = ((dly1_sel ? dly1 : 0) + (dly2_sel ? dly2 : 0) + 1) % 63;

		dly1_sel = 1;
		if (dly < 32) {
			dly1 = dly;
			dly2_sel = 0;
			dly2 = 0;
		} else {
			dly1 = 31;
			dly2_sel = 1;
			dly2 = dly - 31;
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL,
			dly1_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL,
			dly2_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);
	}

	++(host->t_counter.time_read);
	if (host->t_counter.time_read == tune_times_max) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_read = 0;
	}

	INIT_MSG("TUNE_READ: dsmpl<%d> dly1<0x%x> dly2<0x%x> sfreq.<%d>",
		dsmpl & 0x1, dly1, dly2, host->sclk);

	return result;
}

int msdc_tune_write(struct msdc_host *host)
{
	int result = 0;
	void __iomem *base = host->base;
	u32 dsmpl;
	u32 dly, dly1, dly2, dly1_sel, dly2_sel;
	int clkmode, hs400, ddr;
	int tune_times_max;

	MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, clkmode);
	hs400 = (clkmode == 3) ? 1 : 0;
	if (clkmode == 2 || clkmode == 3)
		ddr = 1;
	else
		ddr = 0;

	if (host->id == 0 && hs400) {
		MSDC_GET_FIELD(EMMC50_CFG0, MSDC_EMMC50_CFG_CRC_STS_EDGE,
			dsmpl);
	} else {
		MSDC_GET_FIELD(MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTSEDGE,
			dsmpl);
	}

	if (host->id != 0 && ddr == 0) {
		dsmpl++;
		msdc_set_smpl(host, hs400, dsmpl % 2, TYPE_WRITE_CRC_EDGE,
			NULL);
		tune_times_max = WRITE_DATA_TUNE_MAX_TIME;
	} else {
		dsmpl = 2;
		tune_times_max = WRITE_DATA_TUNE_MAX_TIME/2;
	}

	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL, dly1_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL, dly2_sel);
	MSDC_GET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);

	if (dsmpl >= 2) {
		dly = ((dly1_sel ? dly1 : 0) + (dly2_sel ? dly2 : 0) + 1) % 63;

		dly1_sel = 1;
		if (dly < 32) {
			dly1 = dly;
			dly2_sel = 0;
			dly2 = 0;
		} else {
			dly1 = 31;
			dly2_sel = 1;
			dly2 = dly - 31;
		}
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLYSEL,
			dly1_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2SEL,
			dly2_sel);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_DATRRDLY, dly1);
		MSDC_SET_FIELD(MSDC_PAD_TUNE1, MSDC_PAD_TUNE1_DATRRDLY2, dly2);
	}

	++(host->t_counter.time_write);
	if (host->t_counter.time_write == tune_times_max) {
#ifdef MSDC_LOWER_FREQ
		result = msdc_lower_freq(host);
#else
		result = 1;
#endif
		host->t_counter.time_write = 0;
	}

	INIT_MSG("TUNE_WRITE: dsmpl<%d> dly1<0x%x> dly2<0x%x> sfreq.<%d>",
		dsmpl & 0x1, dly1, dly2, host->sclk);

	return result;
}



#ifndef MSDC_WQ_ERROR_TUNE
int msdc_tuning_wo_autok(struct msdc_host *host)
{
	#if 0
	if (host->error & REQ_DAT_ERR) {
		if (host->err_mrq_dir & MMC_DATA_WRITE)
			return msdc_tune_write_smpl(host);
		else if (host->err_mrq_dir & MMC_DATA_WRITE)
			return msdc_tune_read_smpl(host);
	} else if (host->error & REQ_CMD_EIO)
		return msdc_tune_cmdrsp_smpl(host);
	/* other error */
	#else
	if (host->error & REQ_DAT_ERR) {
		if (host->err_mrq_dir & MMC_DATA_WRITE)
			return msdc_tune_write(host);
		else if (host->err_mrq_dir & MMC_DATA_READ)
			return msdc_tune_read(host);
	} else if (host->error & REQ_CMD_EIO) {
		return msdc_tune_cmdrsp(host);
	}
	#endif
	return 0;
}
#endif

#define MSDC0_TX_SETTING_NUM	(12)
static u32 msdc0_tx_setting_reg[MSDC0_TX_SETTING_NUM][2] = {
	{OFFSET_MSDC_IOCON, MSDC_IOCON_DDR50CKD},
	{OFFSET_EMMC50_CFG0, MSDC_EMMC50_CFG_TXSKEW_SEL},
	{OFFSET_MSDC_PAD_TUNE0, MSDC_PAD_TUNE0_CLKTXDLY},
	{OFFSET_EMMC50_PAD_CMD_TUNE, MSDC_EMMC50_PAD_CMD_TUNE_TXDLY},
	{OFFSET_EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT0_TXDLY},
	{OFFSET_EMMC50_PAD_DAT01_TUNE, MSDC_EMMC50_PAD_DAT1_TXDLY},
	{OFFSET_EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT2_TXDLY},
	{OFFSET_EMMC50_PAD_DAT23_TUNE, MSDC_EMMC50_PAD_DAT3_TXDLY},
	{OFFSET_EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT4_TXDLY},
	{OFFSET_EMMC50_PAD_DAT45_TUNE, MSDC_EMMC50_PAD_DAT5_TXDLY},
	{OFFSET_EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT6_TXDLY},
	{OFFSET_EMMC50_PAD_DAT67_TUNE, MSDC_EMMC50_PAD_DAT7_TXDLY}
};

static u8 msdc0_hs400_tx_setting_default_val[MSDC0_TX_SETTING_NUM] = {
	MSDC_DDRCKD,
	MSDC0_HS400_TXSKEW,
	MSDC0_HS400_CLKTXDLY,
	MSDC0_HS400_CMDTXDLY,
	MSDC0_HS400_DAT0TXDLY,
	MSDC0_HS400_DAT1TXDLY,
	MSDC0_HS400_DAT2TXDLY,
	MSDC0_HS400_DAT3TXDLY,
	MSDC0_HS400_DAT4TXDLY,
	MSDC0_HS400_DAT5TXDLY,
	MSDC0_HS400_DAT6TXDLY,
	MSDC0_HS400_DAT7TXDLY
};

/* for hs and hs200 */
static u8 msdc0_hs_tx_setting_default_val[MSDC0_TX_SETTING_NUM] = {
	MSDC_DDRCKD,
	MSDC0_CLKTXDLY,
	MSDC0_TXSKEW,
	MSDC0_CMDTXDLY,
	MSDC0_DAT0TXDLY,
	MSDC0_DAT1TXDLY,
	MSDC0_DAT2TXDLY,
	MSDC0_DAT3TXDLY,
	MSDC0_DAT4TXDLY,
	MSDC0_DAT5TXDLY,
	MSDC0_DAT6TXDLY,
	MSDC0_DAT7TXDLY
};

static u8 msdc0_ddr52_tx_setting_default_val[MSDC0_TX_SETTING_NUM] = {
	MSDC0_DDR50_DDRCKD,
	MSDC0_CLKTXDLY,
	MSDC0_TXSKEW,
	MSDC0_CMDTXDLY,
	MSDC0_DAT0TXDLY,
	MSDC0_DAT1TXDLY,
	MSDC0_DAT2TXDLY,
	MSDC0_DAT3TXDLY,
	MSDC0_DAT4TXDLY,
	MSDC0_DAT5TXDLY,
	MSDC0_DAT6TXDLY,
	MSDC0_DAT7TXDLY
};

static void msdc_init_tx_setting(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	void __iomem *base = host->base;
	u8 *tx_setting_default;
	int i;

	if (host->hw->host_function == MSDC_EMMC) {
		if ((ios->timing == MMC_TIMING_MMC_HS400) &&
		    (ios->clock >= 100000000))
			tx_setting_default = msdc0_hs400_tx_setting_default_val;
		else if (ios->timing == MMC_TIMING_MMC_DDR52)
			tx_setting_default = msdc0_ddr52_tx_setting_default_val;
		else
			tx_setting_default = msdc0_hs_tx_setting_default_val;

		for (i = 0; i < MSDC0_TX_SETTING_NUM; i++) {
			MSDC_SET_FIELD(base + msdc0_tx_setting_reg[i][0],
				msdc0_tx_setting_reg[i][1],
				tx_setting_default[i]);
		}
	} else if (host->hw->host_function == MSDC_SD) {
		MSDC_SET_FIELD(MSDC_IOCON,
			MSDC_IOCON_DDR50CKD, MSDC_DDRCKD);
		if ((ios->timing == MMC_TIMING_UHS_SDR104) &&
		    (ios->clock >= 200000000)) {
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				MSDC1_CLK_SDR104_TX_VALUE);
		} else {
			MSDC_SET_FIELD(MSDC_PAD_TUNE0,
				MSDC_PAD_TUNE0_CLKTXDLY,
				MSDC1_CLK_TX_VALUE);
		}
	} else if (host->hw->host_function == MSDC_SDIO) {
		MSDC_SET_FIELD(MSDC_IOCON,
			MSDC_IOCON_DDR50CKD, MSDC_DDRCKD);
		MSDC_SET_FIELD(MSDC_PAD_TUNE0,
			MSDC_PAD_TUNE0_CLKTXDLY,
			MSDC2_CLK_TX_VALUE);
	}
}


/*For msdc_ios_set_ios()*/
void msdc_ios_tune_setting(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct msdc_host *host = mmc_priv(mmc);
	struct msdc_hw *hw = host->hw;
	void __iomem *base = host->base;
	u32 mode;

	msdc_set_mclk(host, ios->timing, ios->clock);
	if ((ios->timing == MMC_TIMING_MMC_HS400) &&
	    (host->first_tune_done == 0))
		host->first_tune_done = 1;

	if (ios->clock >= 25000000) {
		MSDC_GET_FIELD(MSDC_CFG, MSDC_CFG_CKMOD, mode);
		msdc_set_smpl_all(host, mode);

		msdc_init_tx_setting(mmc, ios);
	}

	if ((hw->host_function == MSDC_EMMC) && (host->first_tune_done == 1)) {
		/*msdc_set_mclk(host, ios->timing, ios->clock);*/
		if (ios->timing == MMC_TIMING_MMC_HS400) {
			if (host->mclk >= 100000000)
				msdc_execute_tuning(host->mmc,
					MMC_SEND_TUNING_BLOCK_HS200);
		} else if (ios->timing == MMC_TIMING_MMC_HS200) {
			if (host->mclk >= 100000000)
				msdc_execute_tuning(host->mmc,
					MMC_SEND_TUNING_BLOCK_HS200);
		} else {
			msdc_init_tune_setting(host);
			host->first_tune_done = 0;
		}
	}
}

static u32 msdc_status_verify_case1(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;
	unsigned long tmo = jiffies + POLLING_BUSY;
	void __iomem *base = host->base;

	while (state != 4) { /* until status to "tran" */
		msdc_reset_hw(host->id);
		while ((err = msdc_get_card_status(mmc, host, &status))) {
			ERR_MSG("CMD13 ERR<%d>", err);
			if (err != (unsigned int)-EILSEQ) {
				return msdc_power_tuning(host);
			} else if (msdc_tune_cmdrsp(host)) {
				ERR_MSG("update cmd para failed");
				return MSDC_VERIFY_ERROR;
			}
		}

		state = R1_CURRENT_STATE(status);
		ERR_MSG("check card state<%d>", state);
		if (state == 5 || state == 6) {
			ERR_MSG("state<%d> need cmd12 to stop", state);
			msdc_send_stop(host); /* don't tuning */
		} else if (state == 7) { /* busy in programing */
			ERR_MSG("state<%d> card is busy", state);
			spin_unlock(&host->lock);
			msleep(100);
			spin_lock(&host->lock);
		} else if (state != 4) {
			ERR_MSG("state<%d> ??? ", state);
			return msdc_power_tuning(host);
		}

		if (time_after(jiffies, tmo)) {
			ERR_MSG("abort timeout. Do power cycle");
			if ((host->hw->host_function == MSDC_SD)
				&& (host->sclk >= 100000000
				|| (host->timing == MMC_TIMING_UHS_DDR50)))
				host->sd_30_busy++;
			return msdc_power_tuning(host);
		}
	}

	msdc_reset_hw(host->id);
	return MSDC_VERIFY_NEED_TUNE;
}

static u32 msdc_status_verify_case2(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: err hapen; 2: tune pass;*/
	struct mmc_card *card = host->mmc->card;
	void __iomem *base = host->base;

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EILSEQ) {
			ERR_MSG("CMD13 ERR<%d>", err);
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);

	/*whether is right RCA*/
	if (cmd->arg == card->rca << 16) {
		return (3 == state || 8 == state) ? MSDC_VERIFY_NEED_TUNE :
			MSDC_VERIFY_NEED_NOT_TUNE;
	} else {
		return (4 == state || 5 == state || 6 == state || 7 == state)
			? MSDC_VERIFY_NEED_TUNE : MSDC_VERIFY_NEED_NOT_TUNE;
	}
}

static u32 msdc_status_verify_case3(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: tune pass;*/
	void __iomem *base = host->base;

	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EILSEQ) {
			ERR_MSG("CMD13 ERR<%d>", err);
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);
	return (5 == state || 6 == state) ? MSDC_VERIFY_NEED_TUNE :
		MSDC_VERIFY_NEED_NOT_TUNE;
}

static u32 msdc_status_verify_case4(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: tune pass;*/
	void __iomem *base = host->base;

	if (cmd->arg & (0x1UL << 15))
		return MSDC_VERIFY_NEED_NOT_TUNE;
	while (1) {
		msdc_reset_hw(host->id);
		err = msdc_get_card_status(mmc, host, &status);
		if (!err) {
			break;
		} else if (err != (unsigned int)-EILSEQ) {
			ERR_MSG("CMD13 ERR<%d>", err);
			break;
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
		ERR_MSG("CMD13 ERR<%d>", err);
	}
	state = R1_CURRENT_STATE(status);
	return state == 3 ? MSDC_VERIFY_NEED_NOT_TUNE :
		MSDC_VERIFY_NEED_TUNE;
}

#if 0
static u32 msdc_status_verify_case5(struct msdc_host *host,
	struct mmc_command *cmd)
{
	struct mmc_host *mmc = host->mmc;
	u32 status = 0;
	u32 state = 0;
	u32 err = 0;/*0: can tune normaly; 1: tune pass;*/
	struct mmc_card *card = host->mmc->card;
	struct mmc_command cmd_bus_test = { 0 };
	struct mmc_request mrq_bus_sest = { 0 };

	while ((err = msdc_get_card_status(mmc, host, &status))) {
		ERR_MSG("CMD13 ERR<%d>", err);
		if (err != (unsigned int)-EILSEQ) {
			return msdc_power_tuning(host);
		} else if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("update cmd para failed");
			return MSDC_VERIFY_ERROR;
		}
	}
	state = R1_CURRENT_STATE(status);

	if (cmd->opcode == MMC_SEND_TUNING_BLOCK) {
		if (state == 9) {
			/*send cmd14*/
			/*u32 err = -1;*/
			cmd_bus_test.opcode = MMC_BUS_TEST_R;
			cmd_bus_test.arg = 0;
			cmd_bus_test.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

			mrq_bus_sest.cmd = &cmd_bus_test;
			cmd_bus_test.mrq = &mrq_bus_sest;
			cmd_bus_test.data = NULL;
			msdc_do_command(host, &cmd_bus_test, 0, CMD_TIMEOUT);
		}
		return MSDC_VERIFY_NEED_TUNE;
	}

	if (state == 4) {
		/*send cmd19*/
		/*u32 err = -1;*/
		cmd_bus_test.opcode = MMC_BUS_TEST_W;
		cmd_bus_test.arg = 0;
		cmd_bus_test.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		mrq_bus_sest.cmd = &cmd_bus_test;
		cmd_bus_test.mrq = &mrq_bus_sest;
		cmd_bus_test.data = NULL;
		msdc_do_command(host, &cmd_bus_test, 0, CMD_TIMEOUT);
	}
	return MSDC_VERIFY_NEED_TUNE;
}
#endif

static u32 msdc_status_verify(struct msdc_host *host, struct mmc_command *cmd)
{
	if (!host->mmc || !host->mmc->card || !host->mmc->card->rca) {
		/*card is not identify*/
		return MSDC_VERIFY_NEED_TUNE;
	}

	if (((host->hw->host_function == MSDC_EMMC) &&
	      IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE1))
	 || ((host->hw->host_function == MSDC_SD) &&
	      IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_SD_TUNE_CASE1))
	 || (host->app_cmd &&
	      IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_APP_TUNE_CASE1))) {
		return msdc_status_verify_case1(host, cmd);
	} else if (IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE2)) {
		return msdc_status_verify_case2(host, cmd);
	} else if (IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE3)) {
		return msdc_status_verify_case3(host, cmd);
	} else if ((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE4)) {
		return msdc_status_verify_case4(host, cmd);
#if 0
	} else if ((host->hw->host_function == MSDC_EMMC)
		&& IS_IN_CMD_SET(cmd->opcode, CMD_SET_FOR_MMC_TUNE_CASE5)) {
		return msdc_status_verify_case5(host, cmd);
#endif
	} else {
		return MSDC_VERIFY_NEED_TUNE;
	}

}

int msdc_crc_tune(struct msdc_host *host, struct mmc_command *cmd,
	struct mmc_data *data, struct mmc_command *stop,
	struct mmc_command *sbc)
{
	u32 status_verify = 0;

#ifdef MTK_MSDC_USE_CMD23
	/* cmd->error also set when autocmd23 crc error */
	if ((cmd->error == (unsigned int)-EILSEQ)
	 || (stop && (stop->error == (unsigned int)-EILSEQ))
	 || (sbc && (sbc->error == (unsigned int)-EILSEQ))) {
#else
	if ((cmd->error == (unsigned int)-EILSEQ)
	 || (stop && (stop->error == (unsigned int)-EILSEQ))) {
#endif
		if (msdc_tune_cmdrsp(host)) {
			ERR_MSG("failed to updata cmd para");
			return 1;
		}
	}

	if (data && (data->error == (unsigned int)-EILSEQ)) {
		if ((host->id == 0) &&
		    (host->timing == MMC_TIMING_MMC_HS400)) {
			if (emmc_hs400_tune_rw(host)) {
				ERR_MSG("failed to updata write para");
				return 1;
			}
		} else if (data->flags & MMC_DATA_READ) {      /* read */
			if (msdc_tune_read(host)) {
				ERR_MSG("failed to updata read para");
				return 1;
			}
		} else {
			if (msdc_tune_write(host)) {
				ERR_MSG("failed to updata write para");
				return 1;
			}
		}
	}

	status_verify = msdc_status_verify(host, cmd);
	if (status_verify == MSDC_VERIFY_ERROR) {
		ERR_MSG("status verify failed");
		/* data_abort = 1; */
		if (host->hw->host_function == MSDC_SD) {
			if (host->error_tune_enable) {
				ERR_MSG("disable error tune of bad SD card");
				host->error_tune_enable = 0;
			}
			return 1;
		}
	} else if (status_verify == MSDC_VERIFY_NEED_NOT_TUNE) {
		/* clear the error condition. */
		ERR_MSG("need not error tune");
		cmd->error = 0;
		return 1;
	}

	return 0;
}

#define CMD_TMO_MSG "exceed max r/w cmd timeout tune times(%d) or SW tmo(%d), Power cycle"
#define RD_TMO_MSG "exceed max read timeout retry times(%d) or SW tmo(%d) or read tmo tuning times(%d), Power cycle"
#define WR_TMO_MSG "exceed max write timeout retry times(%d) or SW tmo(%d) or write tmo tuning time(%d), Power cycle"

int msdc_cmd_timeout_tune(struct msdc_host *host, struct mmc_command *cmd)
{
	/* CMD TO -> not tuning.
	 * cmd->error also set when autocmd23 TO error
	 */
	if (cmd->error != (unsigned int)-ETIMEDOUT)
		return 0;

	if (check_mmc_cmd1718(cmd->opcode) ||
	    check_mmc_cmd2425(cmd->opcode)) {
		if ((host->sw_timeout) ||
		    (++(host->rwcmd_time_tune) > MSDC_MAX_TIMEOUT_RETRY)) {
			ERR_MSG(CMD_TMO_MSG,
				host->rwcmd_time_tune, host->sw_timeout);
			if (!(host->sd_30_busy) && msdc_power_tuning(host))
				return 1;
		}
	} else {
		return 1;
	}

	return 0;
}

int msdc_data_timeout_tune(struct msdc_host *host, struct mmc_data *data)
{
	if (!data || (data->error != (unsigned int)-ETIMEDOUT))
		return 0;

	/* [ALPS114710] Patch for data timeout issue. */
	if (data->flags & MMC_DATA_READ) {
		if (!(host->sw_timeout) &&
		    (host->hw->host_function == MSDC_SD) &&
		    (host->sclk > 100000000) &&
		    (host->read_timeout_uhs104 < MSDC_MAX_R_TIMEOUT_TUNE)) {
			if (host->t_counter.time_read)
				host->t_counter.time_read--;
			host->read_timeout_uhs104++;
			msdc_tune_read(host);
		} else if ((host->sw_timeout) ||
			   (host->read_timeout_uhs104
				>= MSDC_MAX_R_TIMEOUT_TUNE) ||
			   (++(host->read_time_tune)
				> MSDC_MAX_TIMEOUT_RETRY)) {
			ERR_MSG(RD_TMO_MSG,
				host->read_time_tune,
				host->sw_timeout, host->read_timeout_uhs104);
			if (msdc_power_tuning(host))
				return 1;
		}
	} else if (data->flags & MMC_DATA_WRITE) {
		if (!(host->sw_timeout)
		 && (host->hw->host_function == MSDC_SD)
		 && (host->sclk > 100000000)
		 && (host->write_timeout_uhs104 < MSDC_MAX_W_TIMEOUT_TUNE)) {
			if (host->t_counter.time_write)
				host->t_counter.time_write--;
			host->write_timeout_uhs104++;
			msdc_tune_write(host);
		} else if (!(host->sw_timeout) &&
			   (host->hw->host_function == MSDC_EMMC) &&
			   (host->write_timeout_emmc
				< MSDC_MAX_W_TIMEOUT_TUNE_EMMC)) {
			if (host->t_counter.time_write)
				host->t_counter.time_write--;
			host->write_timeout_emmc++;

			if ((host->id == 0) &&
			    (host->timing == MMC_TIMING_MMC_HS400))
				emmc_hs400_tune_rw(host);
			else
				msdc_tune_write(host);

		} else if ((host->hw->host_function == MSDC_SD)
			&& ((host->sw_timeout) ||
			    (host->write_timeout_uhs104
				>= MSDC_MAX_W_TIMEOUT_TUNE) ||
			    (++(host->write_time_tune)
				> MSDC_MAX_TIMEOUT_RETRY))) {
			ERR_MSG(WR_TMO_MSG,
				host->write_time_tune,
				host->sw_timeout, host->write_timeout_uhs104);
			if (!(host->sd_30_busy) && msdc_power_tuning(host))
				return 1;
		} else if ((host->hw->host_function == MSDC_EMMC)
			&& ((host->sw_timeout) ||
			    (++(host->write_time_tune)
				> MSDC_MAX_TIMEOUT_RETRY_EMMC))) {
			ERR_MSG(WR_TMO_MSG,
				host->write_time_tune,
				host->sw_timeout, host->write_timeout_emmc);
			host->write_timeout_emmc = 0;
			return 1;
		}
	}

	return 0;
}

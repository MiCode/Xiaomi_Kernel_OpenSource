/*
 * drivers/mmc/host/sdhci-msm.c - Qualcomm MSM SDHCI Platform
 * driver source file
 *
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/gfp.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/pinctrl/consumer.h>
#include <linux/msm-bus.h>
#include <linux/pm_runtime.h>
#include <trace/events/mmc.h>

#include "sdhci-msm.h"
#include "sdhci-msm-ice.h"
#include "cmdq_hci.h"

#define QOS_REMOVE_DELAY_MS	10
#define CORE_POWER		0x0
#define CORE_SW_RST		(1 << 7)

#define SDHCI_VER_100		0x2B
#define CORE_MCI_DATA_CNT	0x30
#define CORE_MCI_STATUS		0x34
#define CORE_MCI_FIFO_CNT	0x44

#define CORE_MCI_VERSION		0x050
#define CORE_VERSION_STEP_MASK		0x0000FFFF
#define CORE_VERSION_MINOR_MASK		0x0FFF0000
#define CORE_VERSION_MINOR_SHIFT	16
#define CORE_VERSION_MAJOR_MASK		0xF0000000
#define CORE_VERSION_MAJOR_SHIFT	28
#define CORE_VERSION_TARGET_MASK	0x000000FF
#define SDHCI_MSM_VER_420               0x49

#define CORE_GENERICS			0x70
#define SWITCHABLE_SIGNALLING_VOL	(1 << 29)

#define CORE_HC_MODE		0x78
#define HC_MODE_EN		0x1
#define FF_CLK_SW_RST_DIS	(1 << 13)

#define CORE_TESTBUS_CONFIG	0x0CC
#define CORE_TESTBUS_SEL2_BIT	4
#define CORE_TESTBUS_ENA	(1 << 3)
#define CORE_TESTBUS_SEL2	(1 << CORE_TESTBUS_SEL2_BIT)

#define CORE_PWRCTL_STATUS	0xDC
#define CORE_PWRCTL_MASK	0xE0
#define CORE_PWRCTL_CLEAR	0xE4
#define CORE_PWRCTL_CTL		0xE8

#define CORE_PWRCTL_BUS_OFF	0x01
#define CORE_PWRCTL_BUS_ON	(1 << 1)
#define CORE_PWRCTL_IO_LOW	(1 << 2)
#define CORE_PWRCTL_IO_HIGH	(1 << 3)

#define CORE_PWRCTL_BUS_SUCCESS	0x01
#define CORE_PWRCTL_BUS_FAIL	(1 << 1)
#define CORE_PWRCTL_IO_SUCCESS	(1 << 2)
#define CORE_PWRCTL_IO_FAIL	(1 << 3)

#define INT_MASK		0xF
#define MAX_PHASES		16

#define CORE_DLL_CONFIG		0x100
#define CORE_CMD_DAT_TRACK_SEL	(1 << 0)
#define CORE_DLL_EN		(1 << 16)
#define CORE_CDR_EN		(1 << 17)
#define CORE_CK_OUT_EN		(1 << 18)
#define CORE_CDR_EXT_EN		(1 << 19)
#define CORE_DLL_PDN		(1 << 29)
#define CORE_DLL_RST		(1 << 30)

#define CORE_DLL_STATUS		0x108
#define CORE_DLL_LOCK		(1 << 7)
#define CORE_DDR_DLL_LOCK	(1 << 11)

#define CORE_VENDOR_SPEC	0x10C
#define CORE_CLK_PWRSAVE		(1 << 1)
#define CORE_HC_MCLK_SEL_DFLT		(2 << 8)
#define CORE_HC_MCLK_SEL_HS400		(3 << 8)
#define CORE_HC_MCLK_SEL_MASK		(3 << 8)
#define CORE_HC_AUTO_CMD21_EN		(1 << 6)
#define CORE_IO_PAD_PWR_SWITCH_EN	(1 << 15)
#define CORE_IO_PAD_PWR_SWITCH	(1 << 16)
#define CORE_HC_SELECT_IN_EN	(1 << 18)
#define CORE_HC_SELECT_IN_HS400	(6 << 19)
#define CORE_HC_SELECT_IN_MASK	(7 << 19)
#define CORE_VENDOR_SPEC_POR_VAL	0xA1C

#define CORE_VENDOR_SPEC_ADMA_ERR_ADDR0	0x114
#define CORE_VENDOR_SPEC_ADMA_ERR_ADDR1	0x118

#define CORE_VENDOR_SPEC_FUNC2 0x110
#define HC_SW_RST_WAIT_IDLE_DIS	(1 << 20)
#define HC_SW_RST_REQ (1 << 21)
#define CORE_ONE_MID_EN     (1 << 25)

#define CORE_VENDOR_SPEC_CAPABILITIES0	0x11C
#define CORE_8_BIT_SUPPORT		(1 << 18)
#define CORE_3_3V_SUPPORT		(1 << 24)
#define CORE_3_0V_SUPPORT		(1 << 25)
#define CORE_1_8V_SUPPORT		(1 << 26)
#define CORE_SYS_BUS_SUPPORT_64_BIT	BIT(28)

#define CORE_SDCC_DEBUG_REG		0x124

#define CORE_CSR_CDC_CTLR_CFG0		0x130
#define CORE_SW_TRIG_FULL_CALIB		(1 << 16)
#define CORE_HW_AUTOCAL_ENA		(1 << 17)

#define CORE_CSR_CDC_CTLR_CFG1		0x134
#define CORE_CSR_CDC_CAL_TIMER_CFG0	0x138
#define CORE_TIMER_ENA			(1 << 16)

#define CORE_CSR_CDC_CAL_TIMER_CFG1	0x13C
#define CORE_CSR_CDC_REFCOUNT_CFG	0x140
#define CORE_CSR_CDC_COARSE_CAL_CFG	0x144
#define CORE_CDC_OFFSET_CFG		0x14C
#define CORE_CSR_CDC_DELAY_CFG		0x150
#define CORE_CDC_SLAVE_DDA_CFG		0x160
#define CORE_CSR_CDC_STATUS0		0x164
#define CORE_CALIBRATION_DONE		(1 << 0)

#define CORE_CDC_ERROR_CODE_MASK	0x7000000

#define CQ_CMD_DBG_RAM	                0x110
#define CQ_CMD_DBG_RAM_WA               0x150
#define CQ_CMD_DBG_RAM_OL               0x154

#define CORE_CSR_CDC_GEN_CFG		0x178
#define CORE_CDC_SWITCH_BYPASS_OFF	(1 << 0)
#define CORE_CDC_SWITCH_RC_EN		(1 << 1)

#define CORE_DDR_200_CFG		0x184
#define CORE_CDC_T4_DLY_SEL		(1 << 0)
#define CORE_CMDIN_RCLK_EN		(1 << 1)
#define CORE_START_CDC_TRAFFIC		(1 << 6)

#define CORE_VENDOR_SPEC3	0x1B0
#define CORE_PWRSAVE_DLL	(1 << 3)
#define CORE_CMDEN_HS400_INPUT_MASK_CNT (1 << 13)

#define CORE_DLL_CONFIG_2	0x1B4
#define CORE_DDR_CAL_EN		(1 << 0)
#define CORE_FLL_CYCLE_CNT	(1 << 18)
#define CORE_DLL_CLOCK_DISABLE	(1 << 21)

#define CORE_DDR_CONFIG			0x1B8
#define DDR_CONFIG_POR_VAL		0x80040853
#define DDR_CONFIG_PRG_RCLK_DLY_MASK	0x1FF
#define DDR_CONFIG_PRG_RCLK_DLY		115
#define CORE_DDR_CONFIG_2		0x1BC
#define DDR_CONFIG_2_POR_VAL		0x80040873

/* 512 descriptors */
#define SDHCI_MSM_MAX_SEGMENTS  (1 << 9)
#define SDHCI_MSM_MMC_CLK_GATE_DELAY	200 /* msecs */

#define CORE_FREQ_100MHZ	(100 * 1000 * 1000)
#define TCXO_FREQ		19200000

#define INVALID_TUNING_PHASE	-1
#define sdhci_is_valid_gpio_wakeup_int(_h) ((_h)->pdata->sdiowakeup_irq >= 0)

#define NUM_TUNING_PHASES		16
#define MAX_DRV_TYPES_SUPPORTED_HS200	4
#define MSM_AUTOSUSPEND_DELAY_MS 100

/* Timeout value to avoid infinite waiting for pwr_irq */
#define MSM_PWR_IRQ_TIMEOUT_MS 5000

static const u32 tuning_block_64[] = {
	0x00FF0FFF, 0xCCC3CCFF, 0xFFCC3CC3, 0xEFFEFFFE,
	0xDDFFDFFF, 0xFBFFFBFF, 0xFF7FFFBF, 0xEFBDF777,
	0xF0FFF0FF, 0x3CCCFC0F, 0xCFCC33CC, 0xEEFFEFFF,
	0xFDFFFDFF, 0xFFBFFFDF, 0xFFF7FFBB, 0xDE7B7FF7
};

static const u32 tuning_block_128[] = {
	0xFF00FFFF, 0x0000FFFF, 0xCCCCFFFF, 0xCCCC33CC,
	0xCC3333CC, 0xFFFFCCCC, 0xFFFFEEFF, 0xFFEEEEFF,
	0xFFDDFFFF, 0xDDDDFFFF, 0xBBFFFFFF, 0xBBFFFFFF,
	0xFFFFFFBB, 0xFFFFFF77, 0x77FF7777, 0xFFEEDDBB,
	0x00FFFFFF, 0x00FFFFFF, 0xCCFFFF00, 0xCC33CCCC,
	0x3333CCCC, 0xFFCCCCCC, 0xFFEEFFFF, 0xEEEEFFFF,
	0xDDFFFFFF, 0xDDFFFFFF, 0xFFFFFFDD, 0xFFFFFFBB,
	0xFFFFBBBB, 0xFFFF77FF, 0xFF7777FF, 0xEEDDBB77
};

/* global to hold each slot instance for debug */
static struct sdhci_msm_host *sdhci_slot[2];

static int disable_slots;
/* root can write, others read */
module_param(disable_slots, int, S_IRUGO|S_IWUSR);

static bool nocmdq;
module_param(nocmdq, bool, S_IRUGO|S_IWUSR);

enum vdd_io_level {
	/* set vdd_io_data->low_vol_level */
	VDD_IO_LOW,
	/* set vdd_io_data->high_vol_level */
	VDD_IO_HIGH,
	/*
	 * set whatever there in voltage_level (third argument) of
	 * sdhci_msm_set_vdd_io_vol() function.
	 */
	VDD_IO_SET_LEVEL,
};

/* MSM platform specific tuning */
static inline int msm_dll_poll_ck_out_en(struct sdhci_host *host,
						u8 poll)
{
	int rc = 0;
	u32 wait_cnt = 50;
	u8 ck_out_en = 0;
	struct mmc_host *mmc = host->mmc;

	/* poll for CK_OUT_EN bit.  max. poll time = 50us */
	ck_out_en = !!(readl_relaxed(host->ioaddr + CORE_DLL_CONFIG) &
			CORE_CK_OUT_EN);

	while (ck_out_en != poll) {
		if (--wait_cnt == 0) {
			pr_err("%s: %s: CK_OUT_EN bit is not %d\n",
				mmc_hostname(mmc), __func__, poll);
			rc = -ETIMEDOUT;
			goto out;
		}
		udelay(1);

		ck_out_en = !!(readl_relaxed(host->ioaddr +
				CORE_DLL_CONFIG) & CORE_CK_OUT_EN);
	}
out:
	return rc;
}

/*
 * Enable CDR to track changes of DAT lines and adjust sampling
 * point according to voltage/temperature variations
 */
static int msm_enable_cdr_cm_sdc4_dll(struct sdhci_host *host)
{
	int rc = 0;
	u32 config;

	config = readl_relaxed(host->ioaddr + CORE_DLL_CONFIG);
	config |= CORE_CDR_EN;
	config &= ~(CORE_CDR_EXT_EN | CORE_CK_OUT_EN);
	writel_relaxed(config, host->ioaddr + CORE_DLL_CONFIG);

	rc = msm_dll_poll_ck_out_en(host, 0);
	if (rc)
		goto err;

	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG) |
			CORE_CK_OUT_EN), host->ioaddr + CORE_DLL_CONFIG);

	rc = msm_dll_poll_ck_out_en(host, 1);
	if (rc)
		goto err;
	goto out;
err:
	pr_err("%s: %s: failed\n", mmc_hostname(host->mmc), __func__);
out:
	return rc;
}

static ssize_t store_auto_cmd21(struct device *dev, struct device_attribute
				*attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u32 tmp;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &tmp)) {
		spin_lock_irqsave(&host->lock, flags);
		msm_host->en_auto_cmd21 = !!tmp;
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static ssize_t show_auto_cmd21(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%d\n", msm_host->en_auto_cmd21);
}

/* MSM auto-tuning handler */
static int sdhci_msm_config_auto_tuning_cmd(struct sdhci_host *host,
					    bool enable,
					    u32 type)
{
	int rc = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u32 val = 0;

	if (!msm_host->en_auto_cmd21)
		return 0;

	if (type == MMC_SEND_TUNING_BLOCK_HS200)
		val = CORE_HC_AUTO_CMD21_EN;
	else
		return 0;

	if (enable) {
		rc = msm_enable_cdr_cm_sdc4_dll(host);
		writel_relaxed(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) |
			       val, host->ioaddr + CORE_VENDOR_SPEC);
	} else {
		writel_relaxed(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) &
			       ~val, host->ioaddr + CORE_VENDOR_SPEC);
	}
	return rc;
}

static int msm_config_cm_dll_phase(struct sdhci_host *host, u8 phase)
{
	int rc = 0;
	u8 grey_coded_phase_table[] = {0x0, 0x1, 0x3, 0x2, 0x6, 0x7, 0x5, 0x4,
					0xC, 0xD, 0xF, 0xE, 0xA, 0xB, 0x9,
					0x8};
	unsigned long flags;
	u32 config;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	spin_lock_irqsave(&host->lock, flags);

	config = readl_relaxed(host->ioaddr + CORE_DLL_CONFIG);
	config &= ~(CORE_CDR_EN | CORE_CK_OUT_EN);
	config |= (CORE_CDR_EXT_EN | CORE_DLL_EN);
	writel_relaxed(config, host->ioaddr + CORE_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of DLL_CONFIG register becomes '0' */
	rc = msm_dll_poll_ck_out_en(host, 0);
	if (rc)
		goto err_out;

	/*
	 * Write the selected DLL clock output phase (0 ... 15)
	 * to CDR_SELEXT bit field of DLL_CONFIG register.
	 */
	writel_relaxed(((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			& ~(0xF << 20))
			| (grey_coded_phase_table[phase] << 20)),
			host->ioaddr + CORE_DLL_CONFIG);

	/* Set CK_OUT_EN bit of DLL_CONFIG register to 1. */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_CK_OUT_EN), host->ioaddr + CORE_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of DLL_CONFIG register becomes '1' */
	rc = msm_dll_poll_ck_out_en(host, 1);
	if (rc)
		goto err_out;

	config = readl_relaxed(host->ioaddr + CORE_DLL_CONFIG);
	config |= CORE_CDR_EN;
	config &= ~CORE_CDR_EXT_EN;
	writel_relaxed(config, host->ioaddr + CORE_DLL_CONFIG);
	goto out;

err_out:
	pr_err("%s: %s: Failed to set DLL phase: %d\n",
		mmc_hostname(mmc), __func__, phase);
out:
	spin_unlock_irqrestore(&host->lock, flags);
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return rc;
}

/*
 * Find out the greatest range of consecuitive selected
 * DLL clock output phases that can be used as sampling
 * setting for SD3.0 UHS-I card read operation (in SDR104
 * timing mode) or for eMMC4.5 card read operation (in
 * HS400/HS200 timing mode).
 * Select the 3/4 of the range and configure the DLL with the
 * selected DLL clock output phase.
 */

static int msm_find_most_appropriate_phase(struct sdhci_host *host,
				u8 *phase_table, u8 total_phases)
{
	int ret;
	u8 ranges[MAX_PHASES][MAX_PHASES] = { {0}, {0} };
	u8 phases_per_row[MAX_PHASES] = {0};
	int row_index = 0, col_index = 0, selected_row_index = 0, curr_max = 0;
	int i, cnt, phase_0_raw_index = 0, phase_15_raw_index = 0;
	bool phase_0_found = false, phase_15_found = false;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	if (!total_phases || (total_phases > MAX_PHASES)) {
		pr_err("%s: %s: invalid argument: total_phases=%d\n",
			mmc_hostname(mmc), __func__, total_phases);
		return -EINVAL;
	}

	for (cnt = 0; cnt < total_phases; cnt++) {
		ranges[row_index][col_index] = phase_table[cnt];
		phases_per_row[row_index] += 1;
		col_index++;

		if ((cnt + 1) == total_phases) {
			continue;
		/* check if next phase in phase_table is consecutive or not */
		} else if ((phase_table[cnt] + 1) != phase_table[cnt + 1]) {
			row_index++;
			col_index = 0;
		}
	}

	if (row_index >= MAX_PHASES)
		return -EINVAL;

	/* Check if phase-0 is present in first valid window? */
	if (!ranges[0][0]) {
		phase_0_found = true;
		phase_0_raw_index = 0;
		/* Check if cycle exist between 2 valid windows */
		for (cnt = 1; cnt <= row_index; cnt++) {
			if (phases_per_row[cnt]) {
				for (i = 0; i < phases_per_row[cnt]; i++) {
					if (ranges[cnt][i] == 15) {
						phase_15_found = true;
						phase_15_raw_index = cnt;
						break;
					}
				}
			}
		}
	}

	/* If 2 valid windows form cycle then merge them as single window */
	if (phase_0_found && phase_15_found) {
		/* number of phases in raw where phase 0 is present */
		u8 phases_0 = phases_per_row[phase_0_raw_index];
		/* number of phases in raw where phase 15 is present */
		u8 phases_15 = phases_per_row[phase_15_raw_index];

		if (phases_0 + phases_15 >= MAX_PHASES)
			/*
			 * If there are more than 1 phase windows then total
			 * number of phases in both the windows should not be
			 * more than or equal to MAX_PHASES.
			 */
			return -EINVAL;

		/* Merge 2 cyclic windows */
		i = phases_15;
		for (cnt = 0; cnt < phases_0; cnt++) {
			ranges[phase_15_raw_index][i] =
				ranges[phase_0_raw_index][cnt];
			if (++i >= MAX_PHASES)
				break;
		}

		phases_per_row[phase_0_raw_index] = 0;
		phases_per_row[phase_15_raw_index] = phases_15 + phases_0;
	}

	for (cnt = 0; cnt <= row_index; cnt++) {
		if (phases_per_row[cnt] > curr_max) {
			curr_max = phases_per_row[cnt];
			selected_row_index = cnt;
		}
	}

	i = ((curr_max * 3) / 4);
	if (i)
		i--;

	ret = (int)ranges[selected_row_index][i];

	if (ret >= MAX_PHASES) {
		ret = -EINVAL;
		pr_err("%s: %s: invalid phase selected=%d\n",
			mmc_hostname(mmc), __func__, ret);
	}

	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return ret;
}

static inline void msm_cm_dll_set_freq(struct sdhci_host *host)
{
	u32 mclk_freq = 0;

	/* Program the MCLK value to MCLK_FREQ bit field */
	if (host->clock <= 112000000)
		mclk_freq = 0;
	else if (host->clock <= 125000000)
		mclk_freq = 1;
	else if (host->clock <= 137000000)
		mclk_freq = 2;
	else if (host->clock <= 150000000)
		mclk_freq = 3;
	else if (host->clock <= 162000000)
		mclk_freq = 4;
	else if (host->clock <= 175000000)
		mclk_freq = 5;
	else if (host->clock <= 187000000)
		mclk_freq = 6;
	else if (host->clock <= 200000000)
		mclk_freq = 7;

	writel_relaxed(((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			& ~(7 << 24)) | (mclk_freq << 24)),
			host->ioaddr + CORE_DLL_CONFIG);
}

/* Initialize the DLL (Programmable Delay Line ) */
static int msm_init_cm_dll(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct mmc_host *mmc = host->mmc;
	int rc = 0;
	unsigned long flags;
	u32 wait_cnt;
	bool prev_pwrsave, curr_pwrsave;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	spin_lock_irqsave(&host->lock, flags);
	prev_pwrsave = !!(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) &
			  CORE_CLK_PWRSAVE);
	curr_pwrsave = prev_pwrsave;
	/*
	 * Make sure that clock is always enabled when DLL
	 * tuning is in progress. Keeping PWRSAVE ON may
	 * turn off the clock. So let's disable the PWRSAVE
	 * here and re-enable it once tuning is completed.
	 */
	if (prev_pwrsave) {
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
				& ~CORE_CLK_PWRSAVE),
				host->ioaddr + CORE_VENDOR_SPEC);
		curr_pwrsave = false;
	}

	if (msm_host->use_updated_dll_reset) {
		/* Disable the DLL clock */
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
				& ~CORE_CK_OUT_EN),
				host->ioaddr + CORE_DLL_CONFIG);

		writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG_2)
				| CORE_DLL_CLOCK_DISABLE),
				host->ioaddr + CORE_DLL_CONFIG_2);
	}

	/* Write 1 to DLL_RST bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_DLL_RST), host->ioaddr + CORE_DLL_CONFIG);

	/* Write 1 to DLL_PDN bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_DLL_PDN), host->ioaddr + CORE_DLL_CONFIG);
	msm_cm_dll_set_freq(host);

	if (msm_host->use_updated_dll_reset) {
		u32 mclk_freq = 0;

		if ((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG_2)
					& CORE_FLL_CYCLE_CNT))
			mclk_freq = (u32) ((host->clock / TCXO_FREQ) * 8);
		else
			mclk_freq = (u32) ((host->clock / TCXO_FREQ) * 4);

		writel_relaxed(((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG_2)
				& ~(0xFF << 10)) | (mclk_freq << 10)),
				host->ioaddr + CORE_DLL_CONFIG_2);
		/* wait for 5us before enabling DLL clock */
		udelay(5);
	}

	/* Write 0 to DLL_RST bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			& ~CORE_DLL_RST), host->ioaddr + CORE_DLL_CONFIG);

	/* Write 0 to DLL_PDN bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			& ~CORE_DLL_PDN), host->ioaddr + CORE_DLL_CONFIG);

	if (msm_host->use_updated_dll_reset) {
		msm_cm_dll_set_freq(host);
		/* Enable the DLL clock */
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG_2)
				& ~CORE_DLL_CLOCK_DISABLE),
				host->ioaddr + CORE_DLL_CONFIG_2);
	}

	/* Set DLL_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_DLL_EN), host->ioaddr + CORE_DLL_CONFIG);

	/* Set CK_OUT_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_CK_OUT_EN), host->ioaddr + CORE_DLL_CONFIG);

	wait_cnt = 50;
	/* Wait until DLL_LOCK bit of DLL_STATUS register becomes '1' */
	while (!(readl_relaxed(host->ioaddr + CORE_DLL_STATUS) &
		CORE_DLL_LOCK)) {
		/* max. wait for 50us sec for LOCK bit to be set */
		if (--wait_cnt == 0) {
			pr_err("%s: %s: DLL failed to LOCK\n",
				mmc_hostname(mmc), __func__);
			rc = -ETIMEDOUT;
			goto out;
		}
		/* wait for 1us before polling again */
		udelay(1);
	}

out:
	/* Restore the correct PWRSAVE state */
	if (prev_pwrsave ^ curr_pwrsave) {
		u32 reg = readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC);

		if (prev_pwrsave)
			reg |= CORE_CLK_PWRSAVE;
		else
			reg &= ~CORE_CLK_PWRSAVE;

		writel_relaxed(reg, host->ioaddr + CORE_VENDOR_SPEC);
	}

	spin_unlock_irqrestore(&host->lock, flags);
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return rc;
}

static int sdhci_msm_cdclp533_calibration(struct sdhci_host *host)
{
	u32 calib_done;
	int ret = 0;
	int cdc_err = 0;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	/* Write 0 to CDC_T4_DLY_SEL field in VENDOR_SPEC_DDR200_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DDR_200_CFG)
			& ~CORE_CDC_T4_DLY_SEL),
			host->ioaddr + CORE_DDR_200_CFG);

	/* Write 0 to CDC_SWITCH_BYPASS_OFF field in CORE_CSR_CDC_GEN_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_GEN_CFG)
			& ~CORE_CDC_SWITCH_BYPASS_OFF),
			host->ioaddr + CORE_CSR_CDC_GEN_CFG);

	/* Write 1 to CDC_SWITCH_RC_EN field in CORE_CSR_CDC_GEN_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_GEN_CFG)
			| CORE_CDC_SWITCH_RC_EN),
			host->ioaddr + CORE_CSR_CDC_GEN_CFG);

	/* Write 0 to START_CDC_TRAFFIC field in CORE_DDR200_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DDR_200_CFG)
			& ~CORE_START_CDC_TRAFFIC),
			host->ioaddr + CORE_DDR_200_CFG);

	/*
	 * Perform CDC Register Initialization Sequence
	 *
	 * CORE_CSR_CDC_CTLR_CFG0	0x11800EC
	 * CORE_CSR_CDC_CTLR_CFG1	0x3011111
	 * CORE_CSR_CDC_CAL_TIMER_CFG0	0x1201000
	 * CORE_CSR_CDC_CAL_TIMER_CFG1	0x4
	 * CORE_CSR_CDC_REFCOUNT_CFG	0xCB732020
	 * CORE_CSR_CDC_COARSE_CAL_CFG	0xB19
	 * CORE_CSR_CDC_DELAY_CFG	0x3AC
	 * CORE_CDC_OFFSET_CFG		0x0
	 * CORE_CDC_SLAVE_DDA_CFG	0x16334
	 */

	writel_relaxed(0x11800EC, host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);
	writel_relaxed(0x3011111, host->ioaddr + CORE_CSR_CDC_CTLR_CFG1);
	writel_relaxed(0x1201000, host->ioaddr + CORE_CSR_CDC_CAL_TIMER_CFG0);
	writel_relaxed(0x4, host->ioaddr + CORE_CSR_CDC_CAL_TIMER_CFG1);
	writel_relaxed(0xCB732020, host->ioaddr + CORE_CSR_CDC_REFCOUNT_CFG);
	writel_relaxed(0xB19, host->ioaddr + CORE_CSR_CDC_COARSE_CAL_CFG);
	writel_relaxed(0x4E2, host->ioaddr + CORE_CSR_CDC_DELAY_CFG);
	writel_relaxed(0x0, host->ioaddr + CORE_CDC_OFFSET_CFG);
	writel_relaxed(0x16334, host->ioaddr + CORE_CDC_SLAVE_DDA_CFG);

	/* CDC HW Calibration */

	/* Write 1 to SW_TRIG_FULL_CALIB field in CORE_CSR_CDC_CTLR_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_CTLR_CFG0)
			| CORE_SW_TRIG_FULL_CALIB),
			host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);

	/* Write 0 to SW_TRIG_FULL_CALIB field in CORE_CSR_CDC_CTLR_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_CTLR_CFG0)
			& ~CORE_SW_TRIG_FULL_CALIB),
			host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);

	/* Write 1 to HW_AUTOCAL_ENA field in CORE_CSR_CDC_CTLR_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_CTLR_CFG0)
			| CORE_HW_AUTOCAL_ENA),
			host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);

	/* Write 1 to TIMER_ENA field in CORE_CSR_CDC_CAL_TIMER_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr +
			CORE_CSR_CDC_CAL_TIMER_CFG0) | CORE_TIMER_ENA),
			host->ioaddr + CORE_CSR_CDC_CAL_TIMER_CFG0);

	mb();

	/* Poll on CALIBRATION_DONE field in CORE_CSR_CDC_STATUS0 to be 1 */
	ret = readl_poll_timeout(host->ioaddr + CORE_CSR_CDC_STATUS0,
		 calib_done, (calib_done & CORE_CALIBRATION_DONE), 1, 50);

	if (ret == -ETIMEDOUT) {
		pr_err("%s: %s: CDC Calibration was not completed\n",
				mmc_hostname(host->mmc), __func__);
		goto out;
	}

	/* Verify CDC_ERROR_CODE field in CORE_CSR_CDC_STATUS0 is 0 */
	cdc_err = readl_relaxed(host->ioaddr + CORE_CSR_CDC_STATUS0)
			& CORE_CDC_ERROR_CODE_MASK;
	if (cdc_err) {
		pr_err("%s: %s: CDC Error Code %d\n",
			mmc_hostname(host->mmc), __func__, cdc_err);
		ret = -EINVAL;
		goto out;
	}

	/* Write 1 to START_CDC_TRAFFIC field in CORE_DDR200_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DDR_200_CFG)
			| CORE_START_CDC_TRAFFIC),
			host->ioaddr + CORE_DDR_200_CFG);
out:
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static int sdhci_msm_cm_dll_sdc4_calibration(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u32 dll_status, ddr_config;
	int ret = 0;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	/*
	 * Reprogramming the value in case it might have been modified by
	 * bootloaders.
	 */
	if (msm_host->rclk_delay_fix) {
		writel_relaxed(DDR_CONFIG_2_POR_VAL,
				host->ioaddr + CORE_DDR_CONFIG_2);
	} else {
		ddr_config = DDR_CONFIG_POR_VAL &
				~DDR_CONFIG_PRG_RCLK_DLY_MASK;
		ddr_config |= DDR_CONFIG_PRG_RCLK_DLY;
		writel_relaxed(ddr_config, host->ioaddr + CORE_DDR_CONFIG);
	}

	if (msm_host->enhanced_strobe && mmc_card_strobe(msm_host->mmc->card))
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_DDR_200_CFG)
				| CORE_CMDIN_RCLK_EN),
				host->ioaddr + CORE_DDR_200_CFG);

	/* Write 1 to DDR_CAL_EN field in CORE_DLL_CONFIG_2 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG_2)
			| CORE_DDR_CAL_EN),
			host->ioaddr + CORE_DLL_CONFIG_2);

	/* Poll on DDR_DLL_LOCK bit in CORE_DLL_STATUS to be set */
	ret = readl_poll_timeout(host->ioaddr + CORE_DLL_STATUS,
		 dll_status, (dll_status & CORE_DDR_DLL_LOCK), 10, 1000);

	if (ret == -ETIMEDOUT) {
		pr_err("%s: %s: CM_DLL_SDC4 Calibration was not completed\n",
				mmc_hostname(host->mmc), __func__);
		goto out;
	}

	/*
	 * set CORE_PWRSAVE_DLL bit in CORE_VENDOR_SPEC3.
	 * when MCLK is gated OFF, it is not gated for less than 0.5us
	 * and MCLK must be switched on for at-least 1us before DATA
	 * starts coming. Controllers with 14lpp tech DLL cannot
	 * guarantee above requirement. So PWRSAVE_DLL should not be
	 * turned on for host controllers using this DLL.
	 */
	if (!msm_host->use_14lpp_dll)
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC3)
				| CORE_PWRSAVE_DLL),
				host->ioaddr + CORE_VENDOR_SPEC3);
	mb();
out:
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static int sdhci_msm_enhanced_strobe(struct sdhci_host *host)
{
	int ret = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	if (!msm_host->enhanced_strobe || !mmc_card_strobe(mmc->card)) {
		pr_debug("%s: host/card does not support hs400 enhanced strobe\n",
				mmc_hostname(mmc));
		return -EINVAL;
	}

	if (msm_host->calibration_done ||
		!(mmc->ios.timing == MMC_TIMING_MMC_HS400)) {
		return 0;
	}

	/*
	 * Reset the tuning block.
	 */
	ret = msm_init_cm_dll(host);
	if (ret)
		goto out;

	ret = sdhci_msm_cm_dll_sdc4_calibration(host);
out:
	if (!ret)
		msm_host->calibration_done = true;
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static int sdhci_msm_hs400_dll_calibration(struct sdhci_host *host)
{
	int ret = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	/*
	 * Retuning in HS400 (DDR mode) will fail, just reset the
	 * tuning block and restore the saved tuning phase.
	 */
	ret = msm_init_cm_dll(host);
	if (ret)
		goto out;

	/* Set the selected phase in delay line hw block */
	ret = msm_config_cm_dll_phase(host, msm_host->saved_tuning_phase);
	if (ret)
		goto out;

	/* Write 1 to CMD_DAT_TRACK_SEL field in DLL_CONFIG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
				| CORE_CMD_DAT_TRACK_SEL),
				host->ioaddr + CORE_DLL_CONFIG);

	if (msm_host->use_cdclp533)
		/* Calibrate CDCLP533 DLL HW */
		ret = sdhci_msm_cdclp533_calibration(host);
	else
		/* Calibrate CM_DLL_SDC4 HW */
		ret = sdhci_msm_cm_dll_sdc4_calibration(host);
out:
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static void sdhci_msm_set_mmc_drv_type(struct sdhci_host *host, u32 opcode,
		u8 drv_type)
{
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	struct mmc_host *mmc = host->mmc;
	u8 val = ((drv_type << 4) | 2);

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		(EXT_CSD_HS_TIMING << 16) |
		(val << 8) |
		EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_CMD_AC | MMC_RSP_R1B;
	/* 1 sec */
	cmd.busy_timeout = 1000 * 1000;

	memset(cmd.resp, 0, sizeof(cmd.resp));
	cmd.retries = 3;

	mrq.cmd = &cmd;
	cmd.data = NULL;

	mmc_wait_for_req(mmc, &mrq);
	pr_debug("%s: %s: set card drive type to %d\n",
			mmc_hostname(mmc), __func__,
			drv_type);
}

int sdhci_msm_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned long flags;
	int tuning_seq_cnt = 3;
	u8 phase, *data_buf, tuned_phases[NUM_TUNING_PHASES], tuned_phase_cnt;
	const u32 *tuning_block_pattern = tuning_block_64;
	int size = sizeof(tuning_block_64); /* Tuning pattern size in bytes */
	int rc;
	struct mmc_host *mmc = host->mmc;
	struct mmc_ios	ios = host->mmc->ios;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u8 drv_type = 0;
	bool drv_type_changed = false;
	struct mmc_card *card = host->mmc->card;
	int sts_retry;
	u8 last_good_phase = 0;

	/*
	 * Tuning is required for SDR104, HS200 and HS400 cards and
	 * if clock frequency is greater than 100MHz in these modes.
	 */
	if (host->clock <= CORE_FREQ_100MHZ ||
		!((ios.timing == MMC_TIMING_MMC_HS400) ||
		(ios.timing == MMC_TIMING_MMC_HS200) ||
		(ios.timing == MMC_TIMING_UHS_SDR104)))
		return 0;

	/*
	 * Don't allow re-tuning for CRC errors observed for any commands
	 * that are sent during tuning sequence itself.
	 */
	if (msm_host->tuning_in_progress)
		return 0;
	msm_host->tuning_in_progress = true;
	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);

	/* CDC/SDC4 DLL HW calibration is only required for HS400 mode*/
	if (msm_host->tuning_done && !msm_host->calibration_done &&
		(mmc->ios.timing == MMC_TIMING_MMC_HS400)) {
		rc = sdhci_msm_hs400_dll_calibration(host);
		spin_lock_irqsave(&host->lock, flags);
		if (!rc)
			msm_host->calibration_done = true;
		spin_unlock_irqrestore(&host->lock, flags);
		goto out;
	}

	spin_lock_irqsave(&host->lock, flags);

	if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) &&
		(mmc->ios.bus_width == MMC_BUS_WIDTH_8)) {
		tuning_block_pattern = tuning_block_128;
		size = sizeof(tuning_block_128);
	}
	spin_unlock_irqrestore(&host->lock, flags);

	data_buf = kmalloc(size, GFP_KERNEL);
	if (!data_buf) {
		rc = -ENOMEM;
		goto out;
	}

retry:
	tuned_phase_cnt = 0;

	/* first of all reset the tuning block */
	rc = msm_init_cm_dll(host);
	if (rc)
		goto kfree;

	phase = 0;
	do {
		struct mmc_command cmd = {0};
		struct mmc_data data = {0};
		struct mmc_request mrq = {
			.cmd = &cmd,
			.data = &data
		};
		struct scatterlist sg;
		struct mmc_command sts_cmd = {0};

		/* set the phase in delay line hw block */
		rc = msm_config_cm_dll_phase(host, phase);
		if (rc)
			goto kfree;

		cmd.opcode = opcode;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = size;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.timeout_ns = 1000 * 1000 * 1000; /* 1 sec */

		data.sg = &sg;
		data.sg_len = 1;
		sg_init_one(&sg, data_buf, size);
		memset(data_buf, 0, size);
		mmc_wait_for_req(mmc, &mrq);

		if (card && (cmd.error || data.error)) {
			/*
			 * Set the dll to last known good phase while sending
			 * status command to ensure that status command won't
			 * fail due to bad phase.
			 */
			if (tuned_phase_cnt)
				last_good_phase =
					tuned_phases[tuned_phase_cnt-1];
			else if (msm_host->saved_tuning_phase !=
					INVALID_TUNING_PHASE)
				last_good_phase = msm_host->saved_tuning_phase;

			rc = msm_config_cm_dll_phase(host, last_good_phase);
			if (rc)
				goto kfree;

			sts_cmd.opcode = MMC_SEND_STATUS;
			sts_cmd.arg = card->rca << 16;
			sts_cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
			sts_retry = 5;
			while (sts_retry) {
				mmc_wait_for_cmd(mmc, &sts_cmd, 0);

				if (sts_cmd.error ||
				   (R1_CURRENT_STATE(sts_cmd.resp[0])
				   != R1_STATE_TRAN)) {
					sts_retry--;
					/*
					 * wait for at least 146 MCLK cycles for
					 * the card to move to TRANS state. As
					 * the MCLK would be min 200MHz for
					 * tuning, we need max 0.73us delay. To
					 * be on safer side 1ms delay is given.
					 */
					usleep_range(1000, 1200);
					pr_debug("%s: phase %d sts cmd err %d resp 0x%x\n",
						mmc_hostname(mmc), phase,
						sts_cmd.error, sts_cmd.resp[0]);
					continue;
				}
				break;
			};
		}

		if (!cmd.error && !data.error &&
			!memcmp(data_buf, tuning_block_pattern, size)) {
			/* tuning is successful at this tuning point */
			tuned_phases[tuned_phase_cnt++] = phase;
			pr_debug("%s: %s: found *** good *** phase = %d\n",
				mmc_hostname(mmc), __func__, phase);
		} else {
			pr_debug("%s: %s: found ## bad ## phase = %d\n",
				mmc_hostname(mmc), __func__, phase);
		}
	} while (++phase < 16);

	if ((tuned_phase_cnt == NUM_TUNING_PHASES) &&
			card && mmc_card_mmc(card)) {
		/*
		 * If all phases pass then its a problem. So change the card's
		 * drive type to a different value, if supported and repeat
		 * tuning until at least one phase fails. Then set the original
		 * drive type back.
		 *
		 * If all the phases still pass after trying all possible
		 * drive types, then one of those 16 phases will be picked.
		 * This is no different from what was going on before the
		 * modification to change drive type and retune.
		 */
		pr_debug("%s: tuned phases count: %d\n", mmc_hostname(mmc),
				tuned_phase_cnt);

		/* set drive type to other value . default setting is 0x0 */
		while (++drv_type <= MAX_DRV_TYPES_SUPPORTED_HS200) {
			pr_debug("%s: trying different drive strength (%d)\n",
				mmc_hostname(mmc), drv_type);
			if (card->ext_csd.raw_drive_strength &
					(1 << drv_type)) {
				sdhci_msm_set_mmc_drv_type(host, opcode,
						drv_type);
				if (!drv_type_changed)
					drv_type_changed = true;
				goto retry;
			}
		}
	}

	/* reset drive type to default (50 ohm) if changed */
	if (drv_type_changed)
		sdhci_msm_set_mmc_drv_type(host, opcode, 0);

	if (tuned_phase_cnt) {
		rc = msm_find_most_appropriate_phase(host, tuned_phases,
							tuned_phase_cnt);
		if (rc < 0)
			goto kfree;
		else
			phase = (u8)rc;

		/*
		 * Finally set the selected phase in delay
		 * line hw block.
		 */
		rc = msm_config_cm_dll_phase(host, phase);
		if (rc)
			goto kfree;
		msm_host->saved_tuning_phase = phase;
		pr_debug("%s: %s: finally setting the tuning phase to %d\n",
				mmc_hostname(mmc), __func__, phase);
	} else {
		if (--tuning_seq_cnt)
			goto retry;
		/* tuning failed */
		pr_err("%s: %s: no tuning point found\n",
			mmc_hostname(mmc), __func__);
		rc = -EIO;
	}

kfree:
	kfree(data_buf);
out:
	spin_lock_irqsave(&host->lock, flags);
	if (!rc)
		msm_host->tuning_done = true;
	spin_unlock_irqrestore(&host->lock, flags);
	msm_host->tuning_in_progress = false;
	pr_debug("%s: Exit %s, err(%d)\n", mmc_hostname(mmc), __func__, rc);
	return rc;
}

static int sdhci_msm_setup_gpio(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	struct sdhci_msm_gpio_data *curr;
	int i, ret = 0;

	curr = pdata->pin_data->gpio_data;
	for (i = 0; i < curr->size; i++) {
		if (!gpio_is_valid(curr->gpio[i].no)) {
			ret = -EINVAL;
			pr_err("%s: Invalid gpio = %d\n", __func__,
					curr->gpio[i].no);
			goto free_gpios;
		}
		if (enable) {
			ret = gpio_request(curr->gpio[i].no,
						curr->gpio[i].name);
			if (ret) {
				pr_err("%s: gpio_request(%d, %s) failed %d\n",
					__func__, curr->gpio[i].no,
					curr->gpio[i].name, ret);
				goto free_gpios;
			}
			curr->gpio[i].is_enabled = true;
		} else {
			gpio_free(curr->gpio[i].no);
			curr->gpio[i].is_enabled = false;
		}
	}
	return ret;

free_gpios:
	for (i--; i >= 0; i--) {
		gpio_free(curr->gpio[i].no);
		curr->gpio[i].is_enabled = false;
	}
	return ret;
}

static int sdhci_msm_setup_pinctrl(struct sdhci_msm_pltfm_data *pdata,
		bool enable)
{
	int ret = 0;

	if (enable)
		ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
			pdata->pctrl_data->pins_active);
	else
		ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
			pdata->pctrl_data->pins_sleep);

	if (ret < 0)
		pr_err("%s state for pinctrl failed with %d\n",
			enable ? "Enabling" : "Disabling", ret);

	return ret;
}

static int sdhci_msm_setup_pins(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	int ret = 0;

	if  (pdata->pin_cfg_sts == enable) {
		return 0;
	} else if (pdata->pctrl_data) {
		ret = sdhci_msm_setup_pinctrl(pdata, enable);
		goto out;
	} else if (!pdata->pin_data) {
		return 0;
	}
	if (pdata->pin_data->is_gpio)
		ret = sdhci_msm_setup_gpio(pdata, enable);
out:
	if (!ret)
		pdata->pin_cfg_sts = enable;

	return ret;
}

static int sdhci_msm_dt_get_array(struct device *dev, const char *prop_name,
				 u32 **out, int *len, u32 size)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	size_t sz;
	u32 *arr = NULL;

	if (!of_get_property(np, prop_name, len)) {
		ret = -EINVAL;
		goto out;
	}
	sz = *len = *len / sizeof(*arr);
	if (sz <= 0 || (size > 0 && (sz > size))) {
		dev_err(dev, "%s invalid size\n", prop_name);
		ret = -EINVAL;
		goto out;
	}

	arr = devm_kzalloc(dev, sz * sizeof(*arr), GFP_KERNEL);
	if (!arr) {
		dev_err(dev, "%s failed allocating memory\n", prop_name);
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, prop_name, arr, sz);
	if (ret < 0) {
		dev_err(dev, "%s failed reading array %d\n", prop_name, ret);
		goto out;
	}
	*out = arr;
out:
	if (ret)
		*len = 0;
	return ret;
}

#define MAX_PROP_SIZE 32
static int sdhci_msm_dt_parse_vreg_info(struct device *dev,
		struct sdhci_msm_reg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct sdhci_msm_reg_data *vreg;
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "No vreg data found for %s\n", vreg_name);
		return ret;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		dev_err(dev, "No memory for vreg: %s\n", vreg_name);
		ret = -ENOMEM;
		return ret;
	}

	vreg->name = vreg_name;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-always-on", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->is_always_on = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-lpm-sup", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->lpm_sup = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-voltage-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->low_vol_level = be32_to_cpup(&prop[0]);
		vreg->high_vol_level = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-current-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->lpm_uA = be32_to_cpup(&prop[0]);
		vreg->hpm_uA = be32_to_cpup(&prop[1]);
	}

	*vreg_data = vreg;
	dev_dbg(dev, "%s: %s %s vol=[%d %d]uV, curr=[%d %d]uA\n",
		vreg->name, vreg->is_always_on ? "always_on," : "",
		vreg->lpm_sup ? "lpm_sup," : "", vreg->low_vol_level,
		vreg->high_vol_level, vreg->lpm_uA, vreg->hpm_uA);

	return ret;
}

/* GPIO/Pad data extraction */
static int sdhci_msm_parse_pinctrl_info(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	struct sdhci_pinctrl_data *pctrl_data;
	struct pinctrl *pctrl;
	int ret = 0;

	/* Try to obtain pinctrl handle */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		ret = PTR_ERR(pctrl);
		goto out;
	}
	pctrl_data = devm_kzalloc(dev, sizeof(*pctrl_data), GFP_KERNEL);
	if (!pctrl_data) {
		dev_err(dev, "No memory for sdhci_pinctrl_data\n");
		ret = -ENOMEM;
		goto out;
	}
	pctrl_data->pctrl = pctrl;
	/* Look-up and keep the states handy to be used later */
	pctrl_data->pins_active = pinctrl_lookup_state(
			pctrl_data->pctrl, "active");
	if (IS_ERR(pctrl_data->pins_active)) {
		ret = PTR_ERR(pctrl_data->pins_active);
		dev_err(dev, "Could not get active pinstates, err:%d\n", ret);
		goto out;
	}
	pctrl_data->pins_sleep = pinctrl_lookup_state(
			pctrl_data->pctrl, "sleep");
	if (IS_ERR(pctrl_data->pins_sleep)) {
		ret = PTR_ERR(pctrl_data->pins_sleep);
		dev_err(dev, "Could not get sleep pinstates, err:%d\n", ret);
		goto out;
	}
	pdata->pctrl_data = pctrl_data;
out:
	return ret;
}

#define GPIO_NAME_MAX_LEN 32
static int sdhci_msm_dt_parse_gpio_info(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	int ret = 0, cnt, i;
	struct sdhci_msm_pin_data *pin_data;
	struct device_node *np = dev->of_node;

	ret = sdhci_msm_parse_pinctrl_info(dev, pdata);
	if (!ret) {
		goto out;
	} else if (ret == -EPROBE_DEFER) {
		dev_err(dev, "Pinctrl framework not registered, err:%d\n", ret);
		goto out;
	} else {
		dev_err(dev, "Parsing Pinctrl failed with %d, falling back on GPIO lib\n",
			ret);
		ret = 0;
	}
	pin_data = devm_kzalloc(dev, sizeof(*pin_data), GFP_KERNEL);
	if (!pin_data) {
		dev_err(dev, "No memory for pin_data\n");
		ret = -ENOMEM;
		goto out;
	}

	cnt = of_gpio_count(np);
	if (cnt > 0) {
		pin_data->is_gpio = true;
		pin_data->gpio_data = devm_kzalloc(dev,
				sizeof(struct sdhci_msm_gpio_data), GFP_KERNEL);
		if (!pin_data->gpio_data) {
			dev_err(dev, "No memory for gpio_data\n");
			ret = -ENOMEM;
			goto out;
		}
		pin_data->gpio_data->size = cnt;
		pin_data->gpio_data->gpio = devm_kzalloc(dev, cnt *
				sizeof(struct sdhci_msm_gpio), GFP_KERNEL);

		if (!pin_data->gpio_data->gpio) {
			dev_err(dev, "No memory for gpio\n");
			ret = -ENOMEM;
			goto out;
		}
		for (i = 0; i < cnt; i++) {
			const char *name = NULL;
			char result[GPIO_NAME_MAX_LEN];
			pin_data->gpio_data->gpio[i].no = of_get_gpio(np, i);
			of_property_read_string_index(np,
					"qcom,gpio-names", i, &name);

			snprintf(result, GPIO_NAME_MAX_LEN, "%s-%s",
					dev_name(dev), name ? name : "?");
			pin_data->gpio_data->gpio[i].name = result;
			dev_dbg(dev, "%s: gpio[%s] = %d\n", __func__,
				pin_data->gpio_data->gpio[i].name,
				pin_data->gpio_data->gpio[i].no);
		}
	}
	pdata->pin_data = pin_data;
out:
	if (ret)
		dev_err(dev, "%s failed with err %d\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_SMP
static inline void parse_affine_irq(struct sdhci_msm_pltfm_data *pdata)
{
	pdata->pm_qos_data.irq_req_type = PM_QOS_REQ_AFFINE_IRQ;
}
#else
static inline void parse_affine_irq(struct sdhci_msm_pltfm_data *pdata) { }
#endif

static int sdhci_msm_pm_qos_parse_irq(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	struct device_node *np = dev->of_node;
	const char *str;
	u32 cpu;
	int ret = 0;
	int i;

	pdata->pm_qos_data.irq_valid = false;
	pdata->pm_qos_data.irq_req_type = PM_QOS_REQ_AFFINE_CORES;
	if (!of_property_read_string(np, "qcom,pm-qos-irq-type", &str) &&
		!strcmp(str, "affine_irq")) {
		parse_affine_irq(pdata);
	}

	/* must specify cpu for "affine_cores" type */
	if (pdata->pm_qos_data.irq_req_type == PM_QOS_REQ_AFFINE_CORES) {
		pdata->pm_qos_data.irq_cpu = -1;
		ret = of_property_read_u32(np, "qcom,pm-qos-irq-cpu", &cpu);
		if (ret) {
			dev_err(dev, "%s: error %d reading irq cpu\n", __func__,
				ret);
			goto out;
		}
		if (cpu < 0 || cpu >= num_possible_cpus()) {
			dev_err(dev, "%s: invalid irq cpu %d (NR_CPUS=%d)\n",
				__func__, cpu, num_possible_cpus());
			ret = -EINVAL;
			goto out;
		}
		pdata->pm_qos_data.irq_cpu = cpu;
	}

	if (of_property_count_u32_elems(np, "qcom,pm-qos-irq-latency") !=
		SDHCI_POWER_POLICY_NUM) {
		dev_err(dev, "%s: could not read %d values for 'qcom,pm-qos-irq-latency'\n",
			__func__, SDHCI_POWER_POLICY_NUM);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < SDHCI_POWER_POLICY_NUM; i++)
		of_property_read_u32_index(np, "qcom,pm-qos-irq-latency", i,
			&pdata->pm_qos_data.irq_latency.latency[i]);

	pdata->pm_qos_data.irq_valid = true;
out:
	return ret;
}

static int sdhci_msm_pm_qos_parse_cpu_groups(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 mask;
	int nr_groups;
	int ret;
	int i;

	/* Read cpu group mapping */
	nr_groups = of_property_count_u32_elems(np, "qcom,pm-qos-cpu-groups");
	if (nr_groups <= 0) {
		ret = -EINVAL;
		goto out;
	}
	pdata->pm_qos_data.cpu_group_map.nr_groups = nr_groups;
	pdata->pm_qos_data.cpu_group_map.mask =
		kcalloc(nr_groups, sizeof(cpumask_t), GFP_KERNEL);
	if (!pdata->pm_qos_data.cpu_group_map.mask) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < nr_groups; i++) {
		of_property_read_u32_index(np, "qcom,pm-qos-cpu-groups",
			i, &mask);

		pdata->pm_qos_data.cpu_group_map.mask[i].bits[0] = mask;
		if (!cpumask_subset(&pdata->pm_qos_data.cpu_group_map.mask[i],
			cpu_possible_mask)) {
			dev_err(dev, "%s: invalid mask 0x%x of cpu group #%d\n",
				__func__, mask, i);
			ret = -EINVAL;
			goto free_res;
		}
	}
	return 0;

free_res:
	kfree(pdata->pm_qos_data.cpu_group_map.mask);
out:
	return ret;
}

static int sdhci_msm_pm_qos_parse_latency(struct device *dev, const char *name,
		int nr_groups, struct sdhci_msm_pm_qos_latency **latency)
{
	struct device_node *np = dev->of_node;
	struct sdhci_msm_pm_qos_latency *values;
	int ret;
	int i;
	int group;
	int cfg;

	ret = of_property_count_u32_elems(np, name);
	if (ret > 0 && ret != SDHCI_POWER_POLICY_NUM * nr_groups) {
		dev_err(dev, "%s: invalid number of values for property %s: expected=%d actual=%d\n",
			__func__, name,	SDHCI_POWER_POLICY_NUM * nr_groups,
			ret);
		return -EINVAL;
	} else if (ret < 0) {
		return ret;
	}

	values = kcalloc(nr_groups, sizeof(struct sdhci_msm_pm_qos_latency),
			GFP_KERNEL);
	if (!values)
		return -ENOMEM;

	for (i = 0; i < SDHCI_POWER_POLICY_NUM * nr_groups; i++) {
		group = i / SDHCI_POWER_POLICY_NUM;
		cfg = i % SDHCI_POWER_POLICY_NUM;
		of_property_read_u32_index(np, name, i,
				&(values[group].latency[cfg]));
	}

	*latency = values;
	return 0;
}

static void sdhci_msm_pm_qos_parse(struct device *dev,
				struct sdhci_msm_pltfm_data *pdata)
{
	if (sdhci_msm_pm_qos_parse_irq(dev, pdata))
		dev_notice(dev, "%s: PM QoS voting for IRQ will be disabled\n",
			__func__);

	if (!sdhci_msm_pm_qos_parse_cpu_groups(dev, pdata)) {
		pdata->pm_qos_data.cmdq_valid =
			!sdhci_msm_pm_qos_parse_latency(dev,
				"qcom,pm-qos-cmdq-latency-us",
				pdata->pm_qos_data.cpu_group_map.nr_groups,
				&pdata->pm_qos_data.cmdq_latency);
		pdata->pm_qos_data.legacy_valid =
			!sdhci_msm_pm_qos_parse_latency(dev,
				"qcom,pm-qos-legacy-latency-us",
				pdata->pm_qos_data.cpu_group_map.nr_groups,
				&pdata->pm_qos_data.latency);
		if (!pdata->pm_qos_data.cmdq_valid &&
			!pdata->pm_qos_data.legacy_valid) {
			/* clean-up previously allocated arrays */
			kfree(pdata->pm_qos_data.latency);
			kfree(pdata->pm_qos_data.cmdq_latency);
			dev_err(dev, "%s: invalid PM QoS latency values. Voting for cpu group will be disabled\n",
				__func__);
		}
	} else {
		dev_notice(dev, "%s: PM QoS voting for cpu group will be disabled\n",
			__func__);
	}
}

/* Parse platform data */
static
struct sdhci_msm_pltfm_data *sdhci_msm_populate_pdata(struct device *dev,
						struct sdhci_msm_host *msm_host)
{
	struct sdhci_msm_pltfm_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	u32 bus_width = 0;
	int len, i;
	int clk_table_len;
	u32 *clk_table = NULL;
	int ice_clk_table_len;
	u32 *ice_clk_table = NULL;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	const char *lower_bus_speed = NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate memory for platform data\n");
		goto out;
	}

	pdata->status_gpio = of_get_named_gpio_flags(np, "cd-gpios", 0, &flags);
	if (gpio_is_valid(pdata->status_gpio) & !(flags & OF_GPIO_ACTIVE_LOW))
		pdata->caps2 |= MMC_CAP2_CD_ACTIVE_HIGH;

	of_property_read_u32(np, "qcom,bus-width", &bus_width);
	if (bus_width == 8)
		pdata->mmc_bus_width = MMC_CAP_8_BIT_DATA;
	else if (bus_width == 4)
		pdata->mmc_bus_width = MMC_CAP_4_BIT_DATA;
	else {
		dev_notice(dev, "invalid bus-width, default to 1-bit mode\n");
		pdata->mmc_bus_width = 0;
	}

	if (sdhci_msm_dt_get_array(dev, "qcom,devfreq,freq-table",
			&msm_host->mmc->clk_scaling.freq_table,
			&msm_host->mmc->clk_scaling.freq_table_sz, 0))
		pr_debug("%s: no clock scaling frequencies were supplied\n",
			dev_name(dev));
	else if (!msm_host->mmc->clk_scaling.freq_table ||
			!msm_host->mmc->clk_scaling.freq_table_sz)
			dev_err(dev, "bad dts clock scaling frequencies\n");

	/*
	 * Few hosts can support DDR52 mode at the same lower
	 * system voltage corner as high-speed mode. In such cases,
	 * it is always better to put it in DDR mode which will
	 * improve the performance without any power impact.
	 */
	if (!of_property_read_string(np, "qcom,scaling-lower-bus-speed-mode",
				&lower_bus_speed)) {
		if (!strcmp(lower_bus_speed, "DDR52"))
			msm_host->mmc->clk_scaling.lower_bus_speed_mode |=
				MMC_SCALING_LOWER_DDR52_MODE;
	}

	if (sdhci_msm_dt_get_array(dev, "qcom,clk-rates",
			&clk_table, &clk_table_len, 0)) {
		dev_err(dev, "failed parsing supported clock rates\n");
		goto out;
	}
	if (!clk_table || !clk_table_len) {
		dev_err(dev, "Invalid clock table\n");
		goto out;
	}
	pdata->sup_clk_table = clk_table;
	pdata->sup_clk_cnt = clk_table_len;

	if (msm_host->ice.pdev) {
		if (sdhci_msm_dt_get_array(dev, "qcom,ice-clk-rates",
				&ice_clk_table, &ice_clk_table_len, 0)) {
			dev_err(dev, "failed parsing supported ice clock rates\n");
			goto out;
		}
		if (!ice_clk_table || !ice_clk_table_len) {
			dev_err(dev, "Invalid clock table\n");
			goto out;
		}
		if (ice_clk_table_len != 2) {
			dev_err(dev, "Need max and min frequencies in the table\n");
			goto out;
		}
		pdata->sup_ice_clk_table = ice_clk_table;
		pdata->sup_ice_clk_cnt = ice_clk_table_len;
		pdata->ice_clk_max = pdata->sup_ice_clk_table[0];
		pdata->ice_clk_min = pdata->sup_ice_clk_table[1];
		dev_dbg(dev, "supported ICE clock rates (Hz): max: %u min: %u\n",
				pdata->ice_clk_max, pdata->ice_clk_min);
	}

	pdata->vreg_data = devm_kzalloc(dev, sizeof(struct
						    sdhci_msm_slot_reg_data),
					GFP_KERNEL);
	if (!pdata->vreg_data) {
		dev_err(dev, "failed to allocate memory for vreg data\n");
		goto out;
	}

	if (sdhci_msm_dt_parse_vreg_info(dev, &pdata->vreg_data->vdd_data,
					 "vdd")) {
		dev_err(dev, "failed parsing vdd data\n");
		goto out;
	}
	if (sdhci_msm_dt_parse_vreg_info(dev,
					 &pdata->vreg_data->vdd_io_data,
					 "vdd-io")) {
		dev_err(dev, "failed parsing vdd-io data\n");
		goto out;
	}

	if (sdhci_msm_dt_parse_gpio_info(dev, pdata)) {
		dev_err(dev, "failed parsing gpio data\n");
		goto out;
	}

	len = of_property_count_strings(np, "qcom,bus-speed-mode");

	for (i = 0; i < len; i++) {
		const char *name = NULL;

		of_property_read_string_index(np,
			"qcom,bus-speed-mode", i, &name);
		if (!name)
			continue;

		if (!strncmp(name, "HS400_1p8v", sizeof("HS400_1p8v")))
			pdata->caps2 |= MMC_CAP2_HS400_1_8V;
		else if (!strncmp(name, "HS400_1p2v", sizeof("HS400_1p2v")))
			pdata->caps2 |= MMC_CAP2_HS400_1_2V;
		else if (!strncmp(name, "HS200_1p8v", sizeof("HS200_1p8v")))
			pdata->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
		else if (!strncmp(name, "HS200_1p2v", sizeof("HS200_1p2v")))
			pdata->caps2 |= MMC_CAP2_HS200_1_2V_SDR;
		else if (!strncmp(name, "DDR_1p8v", sizeof("DDR_1p8v")))
			pdata->caps |= MMC_CAP_1_8V_DDR
						| MMC_CAP_UHS_DDR50;
		else if (!strncmp(name, "DDR_1p2v", sizeof("DDR_1p2v")))
			pdata->caps |= MMC_CAP_1_2V_DDR
						| MMC_CAP_UHS_DDR50;
	}

	if (of_get_property(np, "qcom,nonremovable", NULL))
		pdata->nonremovable = true;

	if (of_get_property(np, "qcom,nonhotplug", NULL))
		pdata->nonhotplug = true;

	pdata->largeaddressbus =
		of_property_read_bool(np, "qcom,large-address-bus");

	if (of_property_read_bool(np, "qcom,wakeup-on-idle"))
		msm_host->mmc->wakeup_on_idle = true;

	sdhci_msm_pm_qos_parse(dev, pdata);

	if (of_get_property(np, "qcom,core_3_0v_support", NULL))
		msm_host->core_3_0v_support = true;

	pdata->sdr104_wa = of_property_read_bool(np, "qcom,sdr104-wa");

	return pdata;
out:
	return NULL;
}

/* Returns required bandwidth in Bytes per Sec */
static unsigned int sdhci_get_bw_required(struct sdhci_host *host,
					struct mmc_ios *ios)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	unsigned int bw;

	bw = msm_host->clk_rate;
	/*
	 * For DDR mode, SDCC controller clock will be at
	 * the double rate than the actual clock that goes to card.
	 */
	if (ios->bus_width == MMC_BUS_WIDTH_4)
		bw /= 2;
	else if (ios->bus_width == MMC_BUS_WIDTH_1)
		bw /= 8;

	return bw;
}

static int sdhci_msm_bus_get_vote_for_bw(struct sdhci_msm_host *host,
					   unsigned int bw)
{
	unsigned int *table = host->pdata->voting_data->bw_vecs;
	unsigned int size = host->pdata->voting_data->bw_vecs_size;
	int i;

	if (host->msm_bus_vote.is_max_bw_needed && bw)
		return host->msm_bus_vote.max_bw_vote;

	for (i = 0; i < size; i++) {
		if (bw <= table[i])
			break;
	}

	if (i && (i == size))
		i--;

	return i;
}

/*
 * This function must be called with host lock acquired.
 * Caller of this function should also ensure that msm bus client
 * handle is not null.
 */
static inline int sdhci_msm_bus_set_vote(struct sdhci_msm_host *msm_host,
					     int vote,
					     unsigned long *flags)
{
	struct sdhci_host *host =  platform_get_drvdata(msm_host->pdev);
	int rc = 0;

	BUG_ON(!flags);

	if (vote != msm_host->msm_bus_vote.curr_vote) {
		spin_unlock_irqrestore(&host->lock, *flags);
		rc = msm_bus_scale_client_update_request(
				msm_host->msm_bus_vote.client_handle, vote);
		spin_lock_irqsave(&host->lock, *flags);
		if (rc) {
			pr_err("%s: msm_bus_scale_client_update_request() failed: bus_client_handle=0x%x, vote=%d, err=%d\n",
				mmc_hostname(host->mmc),
				msm_host->msm_bus_vote.client_handle, vote, rc);
			goto out;
		}
		msm_host->msm_bus_vote.curr_vote = vote;
	}
out:
	return rc;
}

/*
 * Internal work. Work to set 0 bandwidth for msm bus.
 */
static void sdhci_msm_bus_work(struct work_struct *work)
{
	struct sdhci_msm_host *msm_host;
	struct sdhci_host *host;
	unsigned long flags;

	msm_host = container_of(work, struct sdhci_msm_host,
				msm_bus_vote.vote_work.work);
	host =  platform_get_drvdata(msm_host->pdev);

	if (!msm_host->msm_bus_vote.client_handle)
		return;

	spin_lock_irqsave(&host->lock, flags);
	/* don't vote for 0 bandwidth if any request is in progress */
	if (!host->mrq) {
		sdhci_msm_bus_set_vote(msm_host,
			msm_host->msm_bus_vote.min_bw_vote, &flags);
	} else
		pr_warning("%s: %s: Transfer in progress. skipping bus voting to 0 bandwidth\n",
			   mmc_hostname(host->mmc), __func__);
	spin_unlock_irqrestore(&host->lock, flags);
}

/*
 * This function cancels any scheduled delayed work and sets the bus
 * vote based on bw (bandwidth) argument.
 */
static void sdhci_msm_bus_cancel_work_and_set_vote(struct sdhci_host *host,
						unsigned int bw)
{
	int vote;
	unsigned long flags;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	cancel_delayed_work_sync(&msm_host->msm_bus_vote.vote_work);
	spin_lock_irqsave(&host->lock, flags);
	vote = sdhci_msm_bus_get_vote_for_bw(msm_host, bw);
	sdhci_msm_bus_set_vote(msm_host, vote, &flags);
	spin_unlock_irqrestore(&host->lock, flags);
}

#define MSM_MMC_BUS_VOTING_DELAY	200 /* msecs */

/* This function queues a work which will set the bandwidth requiement to 0 */
static void sdhci_msm_bus_queue_work(struct sdhci_host *host)
{
	unsigned long flags;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	spin_lock_irqsave(&host->lock, flags);
	if (msm_host->msm_bus_vote.min_bw_vote !=
		msm_host->msm_bus_vote.curr_vote)
		queue_delayed_work(system_wq,
				   &msm_host->msm_bus_vote.vote_work,
				   msecs_to_jiffies(MSM_MMC_BUS_VOTING_DELAY));
	spin_unlock_irqrestore(&host->lock, flags);
}

static int sdhci_msm_bus_register(struct sdhci_msm_host *host,
				struct platform_device *pdev)
{
	int rc = 0;
	struct msm_bus_scale_pdata *bus_pdata;

	struct sdhci_msm_bus_voting_data *data;
	struct device *dev = &pdev->dev;

	data = devm_kzalloc(dev,
		sizeof(struct sdhci_msm_bus_voting_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev,
			"%s: failed to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto out;
	}
	data->bus_pdata = msm_bus_cl_get_pdata(pdev);
	if (data->bus_pdata) {
		rc = sdhci_msm_dt_get_array(dev, "qcom,bus-bw-vectors-bps",
				&data->bw_vecs, &data->bw_vecs_size, 0);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Failed to get bus-bw-vectors-bps\n",
				__func__);
			goto out;
		}
		host->pdata->voting_data = data;
	}
	if (host->pdata->voting_data &&
		host->pdata->voting_data->bus_pdata &&
		host->pdata->voting_data->bw_vecs &&
		host->pdata->voting_data->bw_vecs_size) {

		bus_pdata = host->pdata->voting_data->bus_pdata;
		host->msm_bus_vote.client_handle =
				msm_bus_scale_register_client(bus_pdata);
		if (!host->msm_bus_vote.client_handle) {
			dev_err(&pdev->dev, "msm_bus_scale_register_client()\n");
			rc = -EFAULT;
			goto out;
		}
		/* cache the vote index for minimum and maximum bandwidth */
		host->msm_bus_vote.min_bw_vote =
				sdhci_msm_bus_get_vote_for_bw(host, 0);
		host->msm_bus_vote.max_bw_vote =
				sdhci_msm_bus_get_vote_for_bw(host, UINT_MAX);
	} else {
		devm_kfree(dev, data);
	}

out:
	return rc;
}

static void sdhci_msm_bus_unregister(struct sdhci_msm_host *host)
{
	if (host->msm_bus_vote.client_handle)
		msm_bus_scale_unregister_client(
			host->msm_bus_vote.client_handle);
}

static void sdhci_msm_bus_voting(struct sdhci_host *host, u32 enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct mmc_ios *ios = &host->mmc->ios;
	unsigned int bw;

	if (!msm_host->msm_bus_vote.client_handle)
		return;

	bw = sdhci_get_bw_required(host, ios);
	if (enable) {
		sdhci_msm_bus_cancel_work_and_set_vote(host, bw);
	} else {
		/*
		 * If clock gating is enabled, then remove the vote
		 * immediately because clocks will be disabled only
		 * after SDHCI_MSM_MMC_CLK_GATE_DELAY and thus no
		 * additional delay is required to remove the bus vote.
		 */
		if (host->mmc->clkgate_delay)
			sdhci_msm_bus_cancel_work_and_set_vote(host, 0);
		else
			sdhci_msm_bus_queue_work(host);
	}
}

/* Regulator utility functions */
static int sdhci_msm_vreg_init_reg(struct device *dev,
					struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* check if regulator is already initialized? */
	if (vreg->reg)
		goto out;

	/* Get the regulator handle */
	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		pr_err("%s: devm_regulator_get(%s) failed. ret=%d\n",
			__func__, vreg->name, ret);
		goto out;
	}

	if (regulator_count_voltages(vreg->reg) > 0) {
		vreg->set_voltage_sup = true;
		/* sanity check */
		if (!vreg->high_vol_level || !vreg->hpm_uA) {
			pr_err("%s: %s invalid constraints specified\n",
			       __func__, vreg->name);
			ret = -EINVAL;
		}
	}

out:
	return ret;
}

static void sdhci_msm_vreg_deinit_reg(struct sdhci_msm_reg_data *vreg)
{
	if (vreg->reg)
		devm_regulator_put(vreg->reg);
}

static int sdhci_msm_vreg_set_optimum_mode(struct sdhci_msm_reg_data
						  *vreg, int uA_load)
{
	int ret = 0;

	/*
	 * regulators that do not support regulator_set_voltage also
	 * do not support regulator_set_optimum_mode
	 */
	if (vreg->set_voltage_sup) {
		ret = regulator_set_optimum_mode(vreg->reg, uA_load);
		if (ret < 0)
			pr_err("%s: regulator_set_optimum_mode(reg=%s,uA_load=%d) failed. ret=%d\n",
			       __func__, vreg->name, uA_load, ret);
		else
			/*
			 * regulator_set_optimum_mode() can return non zero
			 * value even for success case.
			 */
			ret = 0;
	}
	return ret;
}

static int sdhci_msm_vreg_set_voltage(struct sdhci_msm_reg_data *vreg,
					int min_uV, int max_uV)
{
	int ret = 0;
	if (vreg->set_voltage_sup) {
		ret = regulator_set_voltage(vreg->reg, min_uV, max_uV);
		if (ret) {
			pr_err("%s: regulator_set_voltage(%s)failed. min_uV=%d,max_uV=%d,ret=%d\n",
			       __func__, vreg->name, min_uV, max_uV, ret);
		}
	}

	return ret;
}

static int sdhci_msm_vreg_enable(struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* Put regulator in HPM (high power mode) */
	ret = sdhci_msm_vreg_set_optimum_mode(vreg, vreg->hpm_uA);
	if (ret < 0)
		return ret;

	if (!vreg->is_enabled) {
		/* Set voltage level */
		ret = sdhci_msm_vreg_set_voltage(vreg, vreg->high_vol_level,
						vreg->high_vol_level);
		if (ret)
			return ret;
	}
	ret = regulator_enable(vreg->reg);
	if (ret) {
		pr_err("%s: regulator_enable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
		return ret;
	}
	vreg->is_enabled = true;
	return ret;
}

static int sdhci_msm_vreg_disable(struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* Never disable regulator marked as always_on */
	if (vreg->is_enabled && !vreg->is_always_on) {
		ret = regulator_disable(vreg->reg);
		if (ret) {
			pr_err("%s: regulator_disable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
			goto out;
		}
		vreg->is_enabled = false;

		ret = sdhci_msm_vreg_set_optimum_mode(vreg, 0);
		if (ret < 0)
			goto out;

		/* Set min. voltage level to 0 */
		ret = sdhci_msm_vreg_set_voltage(vreg, 0, vreg->high_vol_level);
		if (ret)
			goto out;
	} else if (vreg->is_enabled && vreg->is_always_on) {
		if (vreg->lpm_sup) {
			/* Put always_on regulator in LPM (low power mode) */
			ret = sdhci_msm_vreg_set_optimum_mode(vreg,
							      vreg->lpm_uA);
			if (ret < 0)
				goto out;
		}
	}
out:
	return ret;
}

static int sdhci_msm_setup_vreg(struct sdhci_msm_pltfm_data *pdata,
			bool enable, bool is_init)
{
	int ret = 0, i;
	struct sdhci_msm_slot_reg_data *curr_slot;
	struct sdhci_msm_reg_data *vreg_table[2];

	curr_slot = pdata->vreg_data;
	if (!curr_slot) {
		pr_debug("%s: vreg info unavailable,assuming the slot is powered by always on domain\n",
			 __func__);
		goto out;
	}

	vreg_table[0] = curr_slot->vdd_data;
	vreg_table[1] = curr_slot->vdd_io_data;

	for (i = 0; i < ARRAY_SIZE(vreg_table); i++) {
		if (vreg_table[i]) {
			if (enable)
				ret = sdhci_msm_vreg_enable(vreg_table[i]);
			else
				ret = sdhci_msm_vreg_disable(vreg_table[i]);
			if (ret)
				goto out;
		}
	}
out:
	return ret;
}

/* This init function should be called only once for each SDHC slot */
static int sdhci_msm_vreg_init(struct device *dev,
				struct sdhci_msm_pltfm_data *pdata,
				bool is_init)
{
	int ret = 0;
	struct sdhci_msm_slot_reg_data *curr_slot;
	struct sdhci_msm_reg_data *curr_vdd_reg, *curr_vdd_io_reg;

	curr_slot = pdata->vreg_data;
	if (!curr_slot)
		goto out;

	curr_vdd_reg = curr_slot->vdd_data;
	curr_vdd_io_reg = curr_slot->vdd_io_data;

	if (!is_init)
		/* Deregister all regulators from regulator framework */
		goto vdd_io_reg_deinit;

	/*
	 * Get the regulator handle from voltage regulator framework
	 * and then try to set the voltage level for the regulator
	 */
	if (curr_vdd_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_reg);
		if (ret)
			goto out;
	}
	if (curr_vdd_io_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_io_reg);
		if (ret)
			goto vdd_reg_deinit;
	}

	if (ret)
		dev_err(dev, "vreg reset failed (%d)\n", ret);
	goto out;

vdd_io_reg_deinit:
	if (curr_vdd_io_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_io_reg);
vdd_reg_deinit:
	if (curr_vdd_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_reg);
out:
	return ret;
}


static int sdhci_msm_set_vdd_io_vol(struct sdhci_msm_pltfm_data *pdata,
			enum vdd_io_level level,
			unsigned int voltage_level)
{
	int ret = 0;
	int set_level;
	struct sdhci_msm_reg_data *vdd_io_reg;

	if (!pdata->vreg_data)
		return ret;

	vdd_io_reg = pdata->vreg_data->vdd_io_data;
	if (vdd_io_reg && vdd_io_reg->is_enabled) {
		switch (level) {
		case VDD_IO_LOW:
			set_level = vdd_io_reg->low_vol_level;
			break;
		case VDD_IO_HIGH:
			set_level = vdd_io_reg->high_vol_level;
			break;
		case VDD_IO_SET_LEVEL:
			set_level = voltage_level;
			break;
		default:
			pr_err("%s: invalid argument level = %d",
					__func__, level);
			ret = -EINVAL;
			return ret;
		}
		ret = sdhci_msm_vreg_set_voltage(vdd_io_reg, set_level,
				set_level);
	}
	return ret;
}

/*
 * Acquire spin-lock host->lock before calling this function
 */
static void sdhci_msm_cfg_sdiowakeup_gpio_irq(struct sdhci_host *host,
					      bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (enable && !msm_host->is_sdiowakeup_enabled)
		enable_irq(msm_host->pdata->sdiowakeup_irq);
	else if (!enable && msm_host->is_sdiowakeup_enabled)
		disable_irq_nosync(msm_host->pdata->sdiowakeup_irq);
	else
		dev_warn(&msm_host->pdev->dev, "%s: wakeup to config: %d curr: %d\n",
			__func__, enable, msm_host->is_sdiowakeup_enabled);
	msm_host->is_sdiowakeup_enabled = enable;
}

static irqreturn_t sdhci_msm_sdiowakeup_irq(int irq, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	unsigned long flags;

	pr_debug("%s: irq (%d) received\n", __func__, irq);

	spin_lock_irqsave(&host->lock, flags);
	sdhci_msm_cfg_sdiowakeup_gpio_irq(host, false);
	spin_unlock_irqrestore(&host->lock, flags);
	msm_host->sdio_pending_processing = true;

	return IRQ_HANDLED;
}

void sdhci_msm_dump_pwr_ctrl_regs(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	pr_err("%s: PWRCTL_STATUS: 0x%08x | PWRCTL_MASK: 0x%08x | PWRCTL_CTL: 0x%08x\n",
		mmc_hostname(host->mmc),
		readl_relaxed(msm_host->core_mem + CORE_PWRCTL_STATUS),
		readl_relaxed(msm_host->core_mem + CORE_PWRCTL_MASK),
		readl_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL));
}

static irqreturn_t sdhci_msm_pwr_irq(int irq, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u8 irq_status = 0;
	u8 irq_ack = 0;
	int ret = 0;
	int pwr_state = 0, io_level = 0;
	unsigned long flags;
	int retry = 10;

	irq_status = readb_relaxed(msm_host->core_mem + CORE_PWRCTL_STATUS);
	pr_debug("%s: Received IRQ(%d), status=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, irq_status);

	/* Clear the interrupt */
	writeb_relaxed(irq_status, (msm_host->core_mem + CORE_PWRCTL_CLEAR));
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();
	/*
	 * There is a rare HW scenario where the first clear pulse could be
	 * lost when actual reset and clear/read of status register is
	 * happening at a time. Hence, retry for at least 10 times to make
	 * sure status register is cleared. Otherwise, this will result in
	 * a spurious power IRQ resulting in system instability.
	 */
	while (irq_status &
		readb_relaxed(msm_host->core_mem + CORE_PWRCTL_STATUS)) {
		if (retry == 0) {
			pr_err("%s: Timedout clearing (0x%x) pwrctl status register\n",
				mmc_hostname(host->mmc), irq_status);
			sdhci_msm_dump_pwr_ctrl_regs(host);
			BUG_ON(1);
		}
		writeb_relaxed(irq_status,
				(msm_host->core_mem + CORE_PWRCTL_CLEAR));
		retry--;
		udelay(10);
	}
	if (likely(retry < 10))
		pr_debug("%s: success clearing (0x%x) pwrctl status register, retries left %d\n",
				mmc_hostname(host->mmc), irq_status, retry);

	/* Handle BUS ON/OFF*/
	if (irq_status & CORE_PWRCTL_BUS_ON) {
		ret = sdhci_msm_setup_vreg(msm_host->pdata, true, false);
		if (!ret) {
			ret = sdhci_msm_setup_pins(msm_host->pdata, true);
			ret |= sdhci_msm_set_vdd_io_vol(msm_host->pdata,
					VDD_IO_HIGH, 0);
		}
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;

		pwr_state = REQ_BUS_ON;
		io_level = REQ_IO_HIGH;
	}
	if (irq_status & CORE_PWRCTL_BUS_OFF) {
		if (msm_host->pltfm_init_done)
			ret = sdhci_msm_setup_vreg(msm_host->pdata,
					false, false);
		if (!ret) {
			ret = sdhci_msm_setup_pins(msm_host->pdata, false);
			ret |= sdhci_msm_set_vdd_io_vol(msm_host->pdata,
					VDD_IO_LOW, 0);
		}
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;

		pwr_state = REQ_BUS_OFF;
		io_level = REQ_IO_LOW;
	}
	/* Handle IO LOW/HIGH */
	if (irq_status & CORE_PWRCTL_IO_LOW) {
		/* Switch voltage Low */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_LOW, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;

		io_level = REQ_IO_LOW;
	}
	if (irq_status & CORE_PWRCTL_IO_HIGH) {
		/* Switch voltage High */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_HIGH, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;

		io_level = REQ_IO_HIGH;
	}

	/* ACK status to the core */
	writeb_relaxed(irq_ack, (msm_host->core_mem + CORE_PWRCTL_CTL));
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();

	if ((io_level & REQ_IO_HIGH) &&
			(msm_host->caps_0 & CORE_3_0V_SUPPORT) &&
			!msm_host->core_3_0v_support)
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) &
				~CORE_IO_PAD_PWR_SWITCH),
				host->ioaddr + CORE_VENDOR_SPEC);
	else if ((io_level & REQ_IO_LOW) ||
			(msm_host->caps_0 & CORE_1_8V_SUPPORT))
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) |
				CORE_IO_PAD_PWR_SWITCH),
				host->ioaddr + CORE_VENDOR_SPEC);
	mb();

	pr_debug("%s: Handled IRQ(%d), ret=%d, ack=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, ret, irq_ack);
	spin_lock_irqsave(&host->lock, flags);
	if (pwr_state)
		msm_host->curr_pwr_state = pwr_state;
	if (io_level)
		msm_host->curr_io_level = io_level;
	complete(&msm_host->pwr_irq_completion);
	spin_unlock_irqrestore(&host->lock, flags);

	return IRQ_HANDLED;
}

static ssize_t
show_polling(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	int poll;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	poll = !!(host->mmc->caps & MMC_CAP_NEEDS_POLL);
	spin_unlock_irqrestore(&host->lock, flags);

	return snprintf(buf, PAGE_SIZE, "%d\n", poll);
}

static ssize_t
store_polling(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	int value;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &value)) {
		spin_lock_irqsave(&host->lock, flags);
		if (value) {
			host->mmc->caps |= MMC_CAP_NEEDS_POLL;
			mmc_detect_change(host->mmc, 0);
		} else {
			host->mmc->caps &= ~MMC_CAP_NEEDS_POLL;
		}
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static ssize_t
show_sdhci_max_bus_bw(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			msm_host->msm_bus_vote.is_max_bw_needed);
}

static ssize_t
store_sdhci_max_bus_bw(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	uint32_t value;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &value)) {
		spin_lock_irqsave(&host->lock, flags);
		msm_host->msm_bus_vote.is_max_bw_needed = !!value;
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static void sdhci_msm_check_power_status(struct sdhci_host *host, u32 req_type)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	unsigned long flags;
	bool done = false;
	u32 io_sig_sts;

	spin_lock_irqsave(&host->lock, flags);
	pr_debug("%s: %s: request %d curr_pwr_state %x curr_io_level %x\n",
			mmc_hostname(host->mmc), __func__, req_type,
			msm_host->curr_pwr_state, msm_host->curr_io_level);
	io_sig_sts = readl_relaxed(msm_host->core_mem + CORE_GENERICS);
	/*
	 * The IRQ for request type IO High/Low will be generated when -
	 * 1. SWITCHABLE_SIGNALLING_VOL is enabled in HW.
	 * 2. If 1 is true and when there is a state change in 1.8V enable
	 * bit (bit 3) of SDHCI_HOST_CONTROL2 register. The reset state of
	 * that bit is 0 which indicates 3.3V IO voltage. So, when MMC core
	 * layer tries to set it to 3.3V before card detection happens, the
	 * IRQ doesn't get triggered as there is no state change in this bit.
	 * The driver already handles this case by changing the IO voltage
	 * level to high as part of controller power up sequence. Hence, check
	 * for host->pwr to handle a case where IO voltage high request is
	 * issued even before controller power up.
	 */
	if (req_type & (REQ_IO_HIGH | REQ_IO_LOW)) {
		if (!(io_sig_sts & SWITCHABLE_SIGNALLING_VOL) ||
				((req_type & REQ_IO_HIGH) && !host->pwr)) {
			pr_debug("%s: do not wait for power IRQ that never comes\n",
					mmc_hostname(host->mmc));
			spin_unlock_irqrestore(&host->lock, flags);
			return;
		}
	}

	if ((req_type & msm_host->curr_pwr_state) ||
			(req_type & msm_host->curr_io_level))
		done = true;
	spin_unlock_irqrestore(&host->lock, flags);

	/*
	 * This is needed here to hanlde a case where IRQ gets
	 * triggered even before this function is called so that
	 * x->done counter of completion gets reset. Otherwise,
	 * next call to wait_for_completion returns immediately
	 * without actually waiting for the IRQ to be handled.
	 */
	if (done)
		init_completion(&msm_host->pwr_irq_completion);
	else if (!wait_for_completion_timeout(&msm_host->pwr_irq_completion,
				msecs_to_jiffies(MSM_PWR_IRQ_TIMEOUT_MS))) {
		__WARN_printf("%s: request(%d) timed out waiting for pwr_irq\n",
					mmc_hostname(host->mmc), req_type);
		MMC_TRACE(host->mmc,
			"request(%d) timed out waiting for pwr_irq, 0xDC: 0x%08x | 0xE0: 0x%08x | 0xE8: 0x%08x\n",
			req_type,
			readl_relaxed(msm_host->core_mem + CORE_PWRCTL_STATUS),
			readl_relaxed(msm_host->core_mem + CORE_PWRCTL_MASK),
			readl_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL));
		mmc_stop_tracing(host->mmc);
		}

	pr_debug("%s: %s: request %d done\n", mmc_hostname(host->mmc),
			__func__, req_type);
}

static void sdhci_msm_toggle_cdr(struct sdhci_host *host, bool enable)
{
	u32 config = readl_relaxed(host->ioaddr + CORE_DLL_CONFIG);

	if (enable) {
		config |= CORE_CDR_EN;
		config &= ~CORE_CDR_EXT_EN;
		writel_relaxed(config, host->ioaddr + CORE_DLL_CONFIG);
	} else {
		config &= ~CORE_CDR_EN;
		config |= CORE_CDR_EXT_EN;
		writel_relaxed(config, host->ioaddr + CORE_DLL_CONFIG);
	}
}

static unsigned int sdhci_msm_max_segs(void)
{
	return SDHCI_MSM_MAX_SEGMENTS;
}

static unsigned int sdhci_msm_get_min_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return msm_host->pdata->sup_clk_table[0];
}

static unsigned int sdhci_msm_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int max_clk_index = msm_host->pdata->sup_clk_cnt;

	return msm_host->pdata->sup_clk_table[max_clk_index - 1];
}

static unsigned int sdhci_msm_get_sup_clk_rate(struct sdhci_host *host,
						u32 req_clk)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	unsigned int sel_clk = -1;
	unsigned char cnt;

	if (req_clk < sdhci_msm_get_min_clock(host)) {
		sel_clk = sdhci_msm_get_min_clock(host);
		return sel_clk;
	}

	for (cnt = 0; cnt < msm_host->pdata->sup_clk_cnt; cnt++) {
		if (msm_host->pdata->sup_clk_table[cnt] > req_clk) {
			break;
		} else if (msm_host->pdata->sup_clk_table[cnt] == req_clk) {
			sel_clk = msm_host->pdata->sup_clk_table[cnt];
			break;
		} else {
			sel_clk = msm_host->pdata->sup_clk_table[cnt];
		}
	}
	return sel_clk;
}

static int sdhci_msm_enable_controller_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int rc = 0;

	if (atomic_read(&msm_host->controller_clock))
		return 0;

	sdhci_msm_bus_voting(host, 1);

	if (!IS_ERR(msm_host->pclk)) {
		rc = clk_prepare_enable(msm_host->pclk);
		if (rc) {
			pr_err("%s: %s: failed to enable the pclk with error %d\n",
			       mmc_hostname(host->mmc), __func__, rc);
			goto remove_vote;
		}
	}

	rc = clk_prepare_enable(msm_host->clk);
	if (rc) {
		pr_err("%s: %s: failed to enable the host-clk with error %d\n",
		       mmc_hostname(host->mmc), __func__, rc);
		goto disable_pclk;
	}

	if (!IS_ERR(msm_host->ice_clk)) {
		rc = clk_prepare_enable(msm_host->ice_clk);
		if (rc) {
			pr_err("%s: %s: failed to enable the ice-clk with error %d\n",
				mmc_hostname(host->mmc), __func__, rc);
			goto disable_host_clk;
		}
	}
	atomic_set(&msm_host->controller_clock, 1);
	pr_debug("%s: %s: enabled controller clock\n",
			mmc_hostname(host->mmc), __func__);
	goto out;

disable_host_clk:
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
disable_pclk:
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
remove_vote:
	if (msm_host->msm_bus_vote.client_handle)
		sdhci_msm_bus_cancel_work_and_set_vote(host, 0);
out:
	return rc;
}

static void sdhci_msm_disable_controller_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (atomic_read(&msm_host->controller_clock)) {
		if (!IS_ERR(msm_host->clk))
			clk_disable_unprepare(msm_host->clk);
		if (!IS_ERR(msm_host->pclk))
			clk_disable_unprepare(msm_host->pclk);
		if (!IS_ERR(msm_host->ice_clk))
			clk_disable_unprepare(msm_host->ice_clk);
		sdhci_msm_bus_voting(host, 0);
		atomic_set(&msm_host->controller_clock, 0);
		pr_debug("%s: %s: disabled controller clock\n",
			mmc_hostname(host->mmc), __func__);
	}
}

static int sdhci_msm_prepare_clocks(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int rc = 0;

	if (enable && !atomic_read(&msm_host->clks_on)) {
		pr_debug("%s: request to enable clocks\n",
				mmc_hostname(host->mmc));

		/*
		 * The bus-width or the clock rate might have changed
		 * after controller clocks are enbaled, update bus vote
		 * in such case.
		 */
		if (atomic_read(&msm_host->controller_clock))
			sdhci_msm_bus_voting(host, 1);

		rc = sdhci_msm_enable_controller_clock(host);
		if (rc)
			goto remove_vote;

		if (!IS_ERR_OR_NULL(msm_host->bus_clk)) {
			rc = clk_prepare_enable(msm_host->bus_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the bus-clock with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_controller_clk;
			}
		}
		if (!IS_ERR(msm_host->ff_clk)) {
			rc = clk_prepare_enable(msm_host->ff_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the ff_clk with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_bus_clk;
			}
		}
		if (!IS_ERR(msm_host->sleep_clk)) {
			rc = clk_prepare_enable(msm_host->sleep_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the sleep_clk with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_ff_clk;
			}
		}
		mb();

	} else if (!enable && atomic_read(&msm_host->clks_on)) {
		sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
		mb();
		/*
		 * During 1.8V signal switching the clock source must
		 * still be ON as it requires accessing SDHC
		 * registers (SDHCi host control2 register bit 3 must
		 * be written and polled after stopping the SDCLK).
		 */
		if (host->mmc->card_clock_off)
			return 0;
		pr_debug("%s: request to disable clocks\n",
				mmc_hostname(host->mmc));
		if (!IS_ERR_OR_NULL(msm_host->sleep_clk))
			clk_disable_unprepare(msm_host->sleep_clk);
		if (!IS_ERR_OR_NULL(msm_host->ff_clk))
			clk_disable_unprepare(msm_host->ff_clk);
		clk_disable_unprepare(msm_host->clk);
		if (!IS_ERR(msm_host->ice_clk))
			clk_disable_unprepare(msm_host->ice_clk);
		if (!IS_ERR(msm_host->pclk))
			clk_disable_unprepare(msm_host->pclk);
		if (!IS_ERR_OR_NULL(msm_host->bus_clk))
			clk_disable_unprepare(msm_host->bus_clk);

		atomic_set(&msm_host->controller_clock, 0);
		sdhci_msm_bus_voting(host, 0);
	}
	atomic_set(&msm_host->clks_on, enable);
	goto out;
disable_ff_clk:
	if (!IS_ERR_OR_NULL(msm_host->ff_clk))
		clk_disable_unprepare(msm_host->ff_clk);
disable_bus_clk:
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
disable_controller_clk:
	if (!IS_ERR_OR_NULL(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
	if (!IS_ERR(msm_host->ice_clk))
		clk_disable_unprepare(msm_host->ice_clk);
	if (!IS_ERR_OR_NULL(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
	atomic_set(&msm_host->controller_clock, 0);
remove_vote:
	if (msm_host->msm_bus_vote.client_handle)
		sdhci_msm_bus_cancel_work_and_set_vote(host, 0);
out:
	return rc;
}

static void sdhci_msm_set_clock(struct sdhci_host *host, unsigned int clock)
{
	int rc;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct mmc_ios	curr_ios = host->mmc->ios;
	u32 sup_clock, ddr_clock, dll_lock;
	bool curr_pwrsave;

	if (!clock) {
		/*
		 * disable pwrsave to ensure clock is not auto-gated until
		 * the rate is >400KHz (initialization complete).
		 */
		writel_relaxed(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) &
			~CORE_CLK_PWRSAVE, host->ioaddr + CORE_VENDOR_SPEC);
		sdhci_msm_prepare_clocks(host, false);
		host->clock = clock;
		goto out;
	}

	rc = sdhci_msm_prepare_clocks(host, true);
	if (rc)
		goto out;

	curr_pwrsave = !!(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) &
			  CORE_CLK_PWRSAVE);
	if ((clock > 400000) &&
	    !curr_pwrsave && mmc_host_may_gate_card(host->mmc->card))
		writel_relaxed(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
				| CORE_CLK_PWRSAVE,
				host->ioaddr + CORE_VENDOR_SPEC);
	/*
	 * Disable pwrsave for a newly added card if doesn't allow clock
	 * gating.
	 */
	else if (curr_pwrsave && !mmc_host_may_gate_card(host->mmc->card))
		writel_relaxed(readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
				& ~CORE_CLK_PWRSAVE,
				host->ioaddr + CORE_VENDOR_SPEC);

	sup_clock = sdhci_msm_get_sup_clk_rate(host, clock);
	if ((curr_ios.timing == MMC_TIMING_UHS_DDR50) ||
		(curr_ios.timing == MMC_TIMING_MMC_DDR52) ||
		(curr_ios.timing == MMC_TIMING_MMC_HS400)) {
		/*
		 * The SDHC requires internal clock frequency to be double the
		 * actual clock that will be set for DDR mode. The controller
		 * uses the faster clock(100/400MHz) for some of its parts and
		 * send the actual required clock (50/200MHz) to the card.
		 */
		ddr_clock = clock * 2;
		sup_clock = sdhci_msm_get_sup_clk_rate(host,
				ddr_clock);
	}

	/*
	 * In general all timing modes are controlled via UHS mode select in
	 * Host Control2 register. eMMC specific HS200/HS400 doesn't have
	 * their respective modes defined here, hence we use these values.
	 *
	 * HS200 - SDR104 (Since they both are equivalent in functionality)
	 * HS400 - This involves multiple configurations
	 *		Initially SDR104 - when tuning is required as HS200
	 *		Then when switching to DDR @ 400MHz (HS400) we use
	 *		the vendor specific HC_SELECT_IN to control the mode.
	 *
	 * In addition to controlling the modes we also need to select the
	 * correct input clock for DLL depending on the mode.
	 *
	 * HS400 - divided clock (free running MCLK/2)
	 * All other modes - default (free running MCLK)
	 */
	if (curr_ios.timing == MMC_TIMING_MMC_HS400) {
		/* Select the divided clock (free running MCLK/2) */
		writel_relaxed(((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
					& ~CORE_HC_MCLK_SEL_MASK)
					| CORE_HC_MCLK_SEL_HS400),
					host->ioaddr + CORE_VENDOR_SPEC);
		/*
		 * Select HS400 mode using the HC_SELECT_IN from VENDOR SPEC
		 * register
		 */
		if ((msm_host->tuning_done ||
				(mmc_card_strobe(msm_host->mmc->card) &&
				 msm_host->enhanced_strobe)) &&
				!msm_host->calibration_done) {
			/*
			 * Write 0x6 to HC_SELECT_IN and 1 to HC_SELECT_IN_EN
			 * field in VENDOR_SPEC_FUNC
			 */
			writel_relaxed((readl_relaxed(host->ioaddr + \
					CORE_VENDOR_SPEC)
					| CORE_HC_SELECT_IN_HS400
					| CORE_HC_SELECT_IN_EN),
					host->ioaddr + CORE_VENDOR_SPEC);
		}
		if (!host->mmc->ios.old_rate && !msm_host->use_cdclp533) {
			/*
			 * Poll on DLL_LOCK and DDR_DLL_LOCK bits in
			 * CORE_DLL_STATUS to be set.  This should get set
			 * with in 15 us at 200 MHz.
			 */
			rc = readl_poll_timeout(host->ioaddr + CORE_DLL_STATUS,
					dll_lock, (dll_lock & (CORE_DLL_LOCK |
					CORE_DDR_DLL_LOCK)), 10, 1000);
			if (rc == -ETIMEDOUT)
				pr_err("%s: Unable to get DLL_LOCK/DDR_DLL_LOCK, dll_status: 0x%08x\n",
						mmc_hostname(host->mmc),
						dll_lock);
		}
	} else {
		if (!msm_host->use_cdclp533)
			/* set CORE_PWRSAVE_DLL bit in CORE_VENDOR_SPEC3 */
			writel_relaxed((readl_relaxed(host->ioaddr +
					CORE_VENDOR_SPEC3) & ~CORE_PWRSAVE_DLL),
					host->ioaddr + CORE_VENDOR_SPEC3);

		/* Select the default clock (free running MCLK) */
		writel_relaxed(((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
					& ~CORE_HC_MCLK_SEL_MASK)
					| CORE_HC_MCLK_SEL_DFLT),
					host->ioaddr + CORE_VENDOR_SPEC);

		/*
		 * Disable HC_SELECT_IN to be able to use the UHS mode select
		 * configuration from Host Control2 register for all other
		 * modes.
		 *
		 * Write 0 to HC_SELECT_IN and HC_SELECT_IN_EN field
		 * in VENDOR_SPEC_FUNC
		 */
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
				& ~CORE_HC_SELECT_IN_EN
				& ~CORE_HC_SELECT_IN_MASK),
				host->ioaddr + CORE_VENDOR_SPEC);
	}
	mb();

	if (sup_clock != msm_host->clk_rate) {
		pr_debug("%s: %s: setting clk rate to %u\n",
				mmc_hostname(host->mmc), __func__, sup_clock);
		rc = clk_set_rate(msm_host->clk, sup_clock);
		if (rc) {
			pr_err("%s: %s: Failed to set rate %u for host-clk : %d\n",
					mmc_hostname(host->mmc), __func__,
					sup_clock, rc);
			goto out;
		}
		msm_host->clk_rate = sup_clock;
		host->clock = clock;
		/*
		 * Update the bus vote in case of frequency change due to
		 * clock scaling.
		 */
		sdhci_msm_bus_voting(host, 1);
	}
out:
	sdhci_set_clock(host, clock);
}

static void sdhci_msm_set_uhs_signaling(struct sdhci_host *host,
					unsigned int uhs)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if ((uhs == MMC_TIMING_MMC_HS400) ||
		(uhs == MMC_TIMING_MMC_HS200) ||
		(uhs == MMC_TIMING_UHS_SDR104))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (uhs == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if (uhs == MMC_TIMING_UHS_SDR25)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (uhs == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((uhs == MMC_TIMING_UHS_DDR50) ||
		 (uhs == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	/*
	 * When clock frquency is less than 100MHz, the feedback clock must be
	 * provided and DLL must not be used so that tuning can be skipped. To
	 * provide feedback clock, the mode selection can be any value less
	 * than 3'b011 in bits [2:0] of HOST CONTROL2 register.
	 */
	if (host->clock <= CORE_FREQ_100MHZ) {
		if ((uhs == MMC_TIMING_MMC_HS400) ||
		    (uhs == MMC_TIMING_MMC_HS200) ||
		    (uhs == MMC_TIMING_UHS_SDR104))
			ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;

		/*
		 * Make sure DLL is disabled when not required
		 *
		 * Write 1 to DLL_RST bit of DLL_CONFIG register
		 */
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
				| CORE_DLL_RST),
				host->ioaddr + CORE_DLL_CONFIG);

		/* Write 1 to DLL_PDN bit of DLL_CONFIG register */
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
				| CORE_DLL_PDN),
				host->ioaddr + CORE_DLL_CONFIG);
		mb();

		/*
		 * The DLL needs to be restored and CDCLP533 recalibrated
		 * when the clock frequency is set back to 400MHz.
		 */
		msm_host->calibration_done = false;
	}

	pr_debug("%s: %s-clock:%u uhs mode:%u ctrl_2:0x%x\n",
		mmc_hostname(host->mmc), __func__, host->clock, uhs, ctrl_2);
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

}

#define MAX_TEST_BUS 60
#define DRV_NAME "cmdq-host"
static void sdhci_msm_cmdq_dump_debug_ram(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int i = 0;
	struct cmdq_host *cq_host = host->cq_host;

	u32 version = readl_relaxed(msm_host->core_mem + CORE_MCI_VERSION);
	u16 minor = version & CORE_VERSION_TARGET_MASK;
	/* registers offset changed starting from 4.2.0 */
	int offset = minor >= SDHCI_MSM_VER_420 ? 0 : 0x48;

	pr_err("---- Debug RAM dump ----\n");
	pr_err(DRV_NAME ": Debug RAM wrap-around: 0x%08x | Debug RAM overlap: 0x%08x\n",
	       cmdq_readl(cq_host, CQ_CMD_DBG_RAM_WA + offset),
	       cmdq_readl(cq_host, CQ_CMD_DBG_RAM_OL + offset));

	while (i < 16) {
		pr_err(DRV_NAME ": Debug RAM dump [%d]: 0x%08x\n", i,
		       cmdq_readl(cq_host, CQ_CMD_DBG_RAM + offset + (4 * i)));
		i++;
	}
	pr_err("-------------------------\n");
}

void sdhci_msm_dump_vendor_regs(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int tbsel, tbsel2;
	int i, index = 0;
	u32 test_bus_val = 0;
	u32 debug_reg[MAX_TEST_BUS] = {0};
	u32 sts = 0;

	pr_info("----------- VENDOR REGISTER DUMP -----------\n");
	if (host->cq_host)
		sdhci_msm_cmdq_dump_debug_ram(host);

	MMC_TRACE(host->mmc, "Data cnt: 0x%08x | Fifo cnt: 0x%08x\n",
		readl_relaxed(msm_host->core_mem + CORE_MCI_DATA_CNT),
		readl_relaxed(msm_host->core_mem + CORE_MCI_FIFO_CNT));
	pr_info("Data cnt: 0x%08x | Fifo cnt: 0x%08x | Int sts: 0x%08x\n",
		readl_relaxed(msm_host->core_mem + CORE_MCI_DATA_CNT),
		readl_relaxed(msm_host->core_mem + CORE_MCI_FIFO_CNT),
		readl_relaxed(msm_host->core_mem + CORE_MCI_STATUS));
	pr_info("DLL cfg:  0x%08x | DLL sts:  0x%08x | SDCC ver: 0x%08x\n",
		readl_relaxed(host->ioaddr + CORE_DLL_CONFIG),
		readl_relaxed(host->ioaddr + CORE_DLL_STATUS),
		readl_relaxed(msm_host->core_mem + CORE_MCI_VERSION));
	pr_info("Vndr func: 0x%08x | Vndr adma err : addr0: 0x%08x addr1: 0x%08x\n",
		readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC),
		readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC_ADMA_ERR_ADDR0),
		readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC_ADMA_ERR_ADDR1));
	pr_info("Vndr func2: 0x%08x\n",
		readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC_FUNC2));

	/*
	 * tbsel indicates [2:0] bits and tbsel2 indicates [7:4] bits
	 * of CORE_TESTBUS_CONFIG register.
	 *
	 * To select test bus 0 to 7 use tbsel and to select any test bus
	 * above 7 use (tbsel2 | tbsel) to get the test bus number. For eg,
	 * to select test bus 14, write 0x1E to CORE_TESTBUS_CONFIG register
	 * i.e., tbsel2[7:4] = 0001, tbsel[2:0] = 110.
	 */
	for (tbsel2 = 0; tbsel2 < 7; tbsel2++) {
		for (tbsel = 0; tbsel < 8; tbsel++) {
			if (index >= MAX_TEST_BUS)
				break;
			test_bus_val = (tbsel2 << CORE_TESTBUS_SEL2_BIT) |
					tbsel | CORE_TESTBUS_ENA;
			writel_relaxed(test_bus_val,
				msm_host->core_mem + CORE_TESTBUS_CONFIG);
			debug_reg[index++] = readl_relaxed(msm_host->core_mem +
							CORE_SDCC_DEBUG_REG);
		}
	}
	for (i = 0; i < MAX_TEST_BUS; i = i + 4)
		pr_info(" Test bus[%d to %d]: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				i, i + 3, debug_reg[i], debug_reg[i+1],
				debug_reg[i+2], debug_reg[i+3]);
	if (host->is_crypto_en) {
		sdhci_msm_ice_get_status(host, &sts);
		pr_info("%s: ICE status %x\n", mmc_hostname(host->mmc), sts);
		sdhci_msm_ice_print_regs(host);
	}
}

void sdhci_msm_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	/* Set ICE core to be reset in sync with SDHC core */
	if (msm_host->ice.pdev)
		writel_relaxed(1, host->ioaddr + CORE_VENDOR_SPEC_ICE_CTRL);

	sdhci_reset(host, mask);
}

/*
 * sdhci_msm_enhanced_strobe_mask :-
 * Before running CMDQ transfers in HS400 Enhanced Strobe mode,
 * SW should write 3 to
 * HC_VENDOR_SPECIFIC_FUNC3.CMDEN_HS400_INPUT_MASK_CNT register.
 * The default reset value of this register is 2.
 */
static void sdhci_msm_enhanced_strobe_mask(struct sdhci_host *host, bool set)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (!msm_host->enhanced_strobe ||
			!mmc_card_strobe(msm_host->mmc->card)) {
		pr_debug("%s: host/card does not support hs400 enhanced strobe\n",
				mmc_hostname(host->mmc));
		return;
	}

	if (set) {
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC3)
				| CORE_CMDEN_HS400_INPUT_MASK_CNT),
				host->ioaddr + CORE_VENDOR_SPEC3);
	} else {
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC3)
				& ~CORE_CMDEN_HS400_INPUT_MASK_CNT),
				host->ioaddr + CORE_VENDOR_SPEC3);
	}
}

static void sdhci_msm_clear_set_dumpregs(struct sdhci_host *host, bool set)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (set) {
		writel_relaxed(CORE_TESTBUS_ENA,
			       msm_host->core_mem + CORE_TESTBUS_CONFIG);
	} else {
		u32 value;
		value = readl_relaxed(msm_host->core_mem + CORE_TESTBUS_CONFIG);
		value &= ~CORE_TESTBUS_ENA;
		writel_relaxed(value, msm_host->core_mem + CORE_TESTBUS_CONFIG);
	}
}

int sdhci_msm_notify_load(struct sdhci_host *host, enum mmc_load state)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;
	u32 clk_rate = 0;

	if (!IS_ERR(msm_host->ice_clk)) {
		clk_rate = (state == MMC_LOAD_LOW) ?
			msm_host->pdata->ice_clk_min :
			msm_host->pdata->ice_clk_max;
		if (msm_host->ice_clk_rate == clk_rate)
			return 0;
		pr_debug("%s: changing ICE clk rate to %u\n",
				mmc_hostname(host->mmc), clk_rate);
		ret = clk_set_rate(msm_host->ice_clk, clk_rate);
		if (ret) {
			pr_err("%s: ICE_CLK rate set failed (%d) for %u\n",
				mmc_hostname(host->mmc), ret, clk_rate);
			return ret;
		}
		msm_host->ice_clk_rate = clk_rate;
	}
	return 0;
}

void sdhci_msm_reset_workaround(struct sdhci_host *host, u32 enable)
{
	u32 vendor_func2;
	unsigned long timeout;

	vendor_func2 = readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC_FUNC2);

	if (enable) {
		writel_relaxed(vendor_func2 | HC_SW_RST_REQ, host->ioaddr +
				CORE_VENDOR_SPEC_FUNC2);
		timeout = 10000;
		while (readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC_FUNC2) &
				HC_SW_RST_REQ) {
			if (timeout == 0) {
				pr_info("%s: Applying wait idle disable workaround\n",
					mmc_hostname(host->mmc));
				/*
				 * Apply the reset workaround to not wait for
				 * pending data transfers on AXI before
				 * resetting the controller. This could be
				 * risky if the transfers were stuck on the
				 * AXI bus.
				 */
				vendor_func2 = readl_relaxed(host->ioaddr +
						CORE_VENDOR_SPEC_FUNC2);
				writel_relaxed(vendor_func2 |
					HC_SW_RST_WAIT_IDLE_DIS,
					host->ioaddr + CORE_VENDOR_SPEC_FUNC2);
				host->reset_wa_t = ktime_get();
				return;
			}
			timeout--;
			udelay(10);
		}
		pr_info("%s: waiting for SW_RST_REQ is successful\n",
				mmc_hostname(host->mmc));
	} else {
		writel_relaxed(vendor_func2 & ~HC_SW_RST_WAIT_IDLE_DIS,
				host->ioaddr + CORE_VENDOR_SPEC_FUNC2);
	}
}

static void sdhci_msm_pm_qos_irq_unvote_work(struct work_struct *work)
{
	struct sdhci_msm_pm_qos_irq *pm_qos_irq =
		container_of(work, struct sdhci_msm_pm_qos_irq,
			     unvote_work.work);

	if (atomic_read(&pm_qos_irq->counter))
		return;

	pm_qos_irq->latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&pm_qos_irq->req, pm_qos_irq->latency);
}

void sdhci_msm_pm_qos_irq_vote(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_latency *latency =
		&msm_host->pdata->pm_qos_data.irq_latency;
	int counter;

	if (!msm_host->pm_qos_irq.enabled)
		return;

	counter = atomic_inc_return(&msm_host->pm_qos_irq.counter);
	/* Make sure to update the voting in case power policy has changed */
	if (msm_host->pm_qos_irq.latency == latency->latency[host->power_policy]
		&& counter > 1)
		return;

	cancel_delayed_work_sync(&msm_host->pm_qos_irq.unvote_work);
	msm_host->pm_qos_irq.latency = latency->latency[host->power_policy];
	pm_qos_update_request(&msm_host->pm_qos_irq.req,
				msm_host->pm_qos_irq.latency);
}

void sdhci_msm_pm_qos_irq_unvote(struct sdhci_host *host, bool async)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int counter;

	if (!msm_host->pm_qos_irq.enabled)
		return;

	if (atomic_read(&msm_host->pm_qos_irq.counter)) {
		counter = atomic_dec_return(&msm_host->pm_qos_irq.counter);
	} else {
		WARN(1, "attempt to decrement pm_qos_irq.counter when it's 0");
		return;
	}

	if (counter)
		return;

	if (async) {
		schedule_delayed_work(&msm_host->pm_qos_irq.unvote_work,
				      msecs_to_jiffies(QOS_REMOVE_DELAY_MS));
		return;
	}

	msm_host->pm_qos_irq.latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&msm_host->pm_qos_irq.req,
			msm_host->pm_qos_irq.latency);
}

static ssize_t
sdhci_msm_pm_qos_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_irq *irq = &msm_host->pm_qos_irq;

	return snprintf(buf, PAGE_SIZE,
		"IRQ PM QoS: enabled=%d, counter=%d, latency=%d\n",
		irq->enabled, atomic_read(&irq->counter), irq->latency);
}

static ssize_t
sdhci_msm_pm_qos_irq_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%u\n", msm_host->pm_qos_irq.enabled);
}

static ssize_t
sdhci_msm_pm_qos_irq_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	uint32_t value;
	bool enable;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		goto out;
	enable = !!value;

	if (enable == msm_host->pm_qos_irq.enabled)
		goto out;

	msm_host->pm_qos_irq.enabled = enable;
	if (!enable) {
		cancel_delayed_work_sync(&msm_host->pm_qos_irq.unvote_work);
		atomic_set(&msm_host->pm_qos_irq.counter, 0);
		msm_host->pm_qos_irq.latency = PM_QOS_DEFAULT_VALUE;
		pm_qos_update_request(&msm_host->pm_qos_irq.req,
				msm_host->pm_qos_irq.latency);
	}

out:
	return count;
}

#ifdef CONFIG_SMP
static inline void set_affine_irq(struct sdhci_msm_host *msm_host,
				struct sdhci_host *host)
{
	msm_host->pm_qos_irq.req.irq = host->irq;
}
#else
static inline void set_affine_irq(struct sdhci_msm_host *msm_host,
				struct sdhci_host *host) { }
#endif

void sdhci_msm_pm_qos_irq_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_latency *irq_latency;
	int ret;

	if (!msm_host->pdata->pm_qos_data.irq_valid)
		return;

	/* Initialize only once as this gets called per partition */
	if (msm_host->pm_qos_irq.enabled)
		return;

	atomic_set(&msm_host->pm_qos_irq.counter, 0);
	msm_host->pm_qos_irq.req.type =
			msm_host->pdata->pm_qos_data.irq_req_type;
	if ((msm_host->pm_qos_irq.req.type != PM_QOS_REQ_AFFINE_CORES) &&
		(msm_host->pm_qos_irq.req.type != PM_QOS_REQ_ALL_CORES))
		set_affine_irq(msm_host, host);
	else
		cpumask_copy(&msm_host->pm_qos_irq.req.cpus_affine,
			cpumask_of(msm_host->pdata->pm_qos_data.irq_cpu));

	INIT_DELAYED_WORK(&msm_host->pm_qos_irq.unvote_work,
		sdhci_msm_pm_qos_irq_unvote_work);
	/* For initialization phase, set the performance latency */
	irq_latency = &msm_host->pdata->pm_qos_data.irq_latency;
	msm_host->pm_qos_irq.latency =
		irq_latency->latency[SDHCI_PERFORMANCE_MODE];
	pm_qos_add_request(&msm_host->pm_qos_irq.req, PM_QOS_CPU_DMA_LATENCY,
			msm_host->pm_qos_irq.latency);
	msm_host->pm_qos_irq.enabled = true;

	/* sysfs */
	msm_host->pm_qos_irq.enable_attr.show =
		sdhci_msm_pm_qos_irq_enable_show;
	msm_host->pm_qos_irq.enable_attr.store =
		sdhci_msm_pm_qos_irq_enable_store;
	sysfs_attr_init(&msm_host->pm_qos_irq.enable_attr.attr);
	msm_host->pm_qos_irq.enable_attr.attr.name = "pm_qos_irq_enable";
	msm_host->pm_qos_irq.enable_attr.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&msm_host->pdev->dev,
		&msm_host->pm_qos_irq.enable_attr);
	if (ret)
		pr_err("%s: fail to create pm_qos_irq_enable (%d)\n",
			__func__, ret);

	msm_host->pm_qos_irq.status_attr.show = sdhci_msm_pm_qos_irq_show;
	msm_host->pm_qos_irq.status_attr.store = NULL;
	sysfs_attr_init(&msm_host->pm_qos_irq.status_attr.attr);
	msm_host->pm_qos_irq.status_attr.attr.name = "pm_qos_irq_status";
	msm_host->pm_qos_irq.status_attr.attr.mode = S_IRUGO;
	ret = device_create_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_irq.status_attr);
	if (ret)
		pr_err("%s: fail to create pm_qos_irq_status (%d)\n",
			__func__, ret);
}

static ssize_t sdhci_msm_pm_qos_group_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_group *group;
	int i;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	int offset = 0;

	for (i = 0; i < nr_groups; i++) {
		group = &msm_host->pm_qos[i];
		offset += snprintf(&buf[offset], PAGE_SIZE,
			"Group #%d (mask=0x%lx) PM QoS: enabled=%d, counter=%d, latency=%d\n",
			i, group->req.cpus_affine.bits[0],
			msm_host->pm_qos_group_enable,
			atomic_read(&group->counter),
			group->latency);
	}

	return offset;
}

static ssize_t sdhci_msm_pm_qos_group_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%s\n",
		msm_host->pm_qos_group_enable ? "enabled" : "disabled");
}

static ssize_t sdhci_msm_pm_qos_group_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	uint32_t value;
	bool enable;
	int ret;
	int i;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		goto out;
	enable = !!value;

	if (enable == msm_host->pm_qos_group_enable)
		goto out;

	msm_host->pm_qos_group_enable = enable;
	if (!enable) {
		for (i = 0; i < nr_groups; i++) {
			cancel_delayed_work_sync(
				&msm_host->pm_qos[i].unvote_work);
			atomic_set(&msm_host->pm_qos[i].counter, 0);
			msm_host->pm_qos[i].latency = PM_QOS_DEFAULT_VALUE;
			pm_qos_update_request(&msm_host->pm_qos[i].req,
				msm_host->pm_qos[i].latency);
		}
	}

out:
	return count;
}

static int sdhci_msm_get_cpu_group(struct sdhci_msm_host *msm_host, int cpu)
{
	int i;
	struct sdhci_msm_cpu_group_map *map =
			&msm_host->pdata->pm_qos_data.cpu_group_map;

	if (cpu < 0)
		goto not_found;

	for (i = 0; i < map->nr_groups; i++)
		if (cpumask_test_cpu(cpu, &map->mask[i]))
			return i;

not_found:
	return -EINVAL;
}

void sdhci_msm_pm_qos_cpu_vote(struct sdhci_host *host,
		struct sdhci_msm_pm_qos_latency *latency, int cpu)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int group = sdhci_msm_get_cpu_group(msm_host, cpu);
	struct sdhci_msm_pm_qos_group *pm_qos_group;
	int counter;

	if (!msm_host->pm_qos_group_enable || group < 0)
		return;

	pm_qos_group = &msm_host->pm_qos[group];
	counter = atomic_inc_return(&pm_qos_group->counter);

	/* Make sure to update the voting in case power policy has changed */
	if (pm_qos_group->latency == latency->latency[host->power_policy]
		&& counter > 1)
		return;

	cancel_delayed_work_sync(&pm_qos_group->unvote_work);

	pm_qos_group->latency = latency->latency[host->power_policy];
	pm_qos_update_request(&pm_qos_group->req, pm_qos_group->latency);
}

static void sdhci_msm_pm_qos_cpu_unvote_work(struct work_struct *work)
{
	struct sdhci_msm_pm_qos_group *group =
		container_of(work, struct sdhci_msm_pm_qos_group,
			     unvote_work.work);

	if (atomic_read(&group->counter))
		return;

	group->latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&group->req, group->latency);
}

bool sdhci_msm_pm_qos_cpu_unvote(struct sdhci_host *host, int cpu, bool async)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int group = sdhci_msm_get_cpu_group(msm_host, cpu);

	if (!msm_host->pm_qos_group_enable || group < 0 ||
		atomic_dec_return(&msm_host->pm_qos[group].counter))
		return false;

	if (async) {
		schedule_delayed_work(&msm_host->pm_qos[group].unvote_work,
				      msecs_to_jiffies(QOS_REMOVE_DELAY_MS));
		return true;
	}

	msm_host->pm_qos[group].latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&msm_host->pm_qos[group].req,
				msm_host->pm_qos[group].latency);
	return true;
}

void sdhci_msm_pm_qos_cpu_init(struct sdhci_host *host,
		struct sdhci_msm_pm_qos_latency *latency)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	struct sdhci_msm_pm_qos_group *group;
	int i;
	int ret;

	if (msm_host->pm_qos_group_enable)
		return;

	msm_host->pm_qos = kcalloc(nr_groups, sizeof(*msm_host->pm_qos),
			GFP_KERNEL);
	if (!msm_host->pm_qos)
		return;

	for (i = 0; i < nr_groups; i++) {
		group = &msm_host->pm_qos[i];
		INIT_DELAYED_WORK(&group->unvote_work,
			sdhci_msm_pm_qos_cpu_unvote_work);
		atomic_set(&group->counter, 0);
		group->req.type = PM_QOS_REQ_AFFINE_CORES;
		cpumask_copy(&group->req.cpus_affine,
			&msm_host->pdata->pm_qos_data.cpu_group_map.mask[i]);
		/* For initialization phase, set the performance mode latency */
		group->latency = latency[i].latency[SDHCI_PERFORMANCE_MODE];
		pm_qos_add_request(&group->req, PM_QOS_CPU_DMA_LATENCY,
			group->latency);
		pr_info("%s (): voted for group #%d (mask=0x%lx) latency=%d (0x%p)\n",
			__func__, i,
			group->req.cpus_affine.bits[0],
			group->latency,
			&latency[i].latency[SDHCI_PERFORMANCE_MODE]);
	}
	msm_host->pm_qos_prev_cpu = -1;
	msm_host->pm_qos_group_enable = true;

	/* sysfs */
	msm_host->pm_qos_group_status_attr.show = sdhci_msm_pm_qos_group_show;
	msm_host->pm_qos_group_status_attr.store = NULL;
	sysfs_attr_init(&msm_host->pm_qos_group_status_attr.attr);
	msm_host->pm_qos_group_status_attr.attr.name =
			"pm_qos_cpu_groups_status";
	msm_host->pm_qos_group_status_attr.attr.mode = S_IRUGO;
	ret = device_create_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_group_status_attr);
	if (ret)
		dev_err(&msm_host->pdev->dev, "%s: fail to create pm_qos_group_status_attr (%d)\n",
			__func__, ret);
	msm_host->pm_qos_group_enable_attr.show =
			sdhci_msm_pm_qos_group_enable_show;
	msm_host->pm_qos_group_enable_attr.store =
			sdhci_msm_pm_qos_group_enable_store;
	sysfs_attr_init(&msm_host->pm_qos_group_enable_attr.attr);
	msm_host->pm_qos_group_enable_attr.attr.name =
			"pm_qos_cpu_groups_enable";
	msm_host->pm_qos_group_enable_attr.attr.mode = S_IRUGO;
	ret = device_create_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_group_enable_attr);
	if (ret)
		dev_err(&msm_host->pdev->dev, "%s: fail to create pm_qos_group_enable_attr (%d)\n",
			__func__, ret);
}

static void sdhci_msm_pre_req(struct sdhci_host *host,
		struct mmc_request *mmc_req)
{
	int cpu;
	int group;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int prev_group = sdhci_msm_get_cpu_group(msm_host,
			msm_host->pm_qos_prev_cpu);

	sdhci_msm_pm_qos_irq_vote(host);

	cpu = get_cpu();
	put_cpu();
	group = sdhci_msm_get_cpu_group(msm_host, cpu);
	if (group < 0)
		return;

	if (group != prev_group && prev_group >= 0) {
		sdhci_msm_pm_qos_cpu_unvote(host,
				msm_host->pm_qos_prev_cpu, false);
		prev_group = -1; /* make sure to vote for new group */
	}

	if (prev_group < 0) {
		sdhci_msm_pm_qos_cpu_vote(host,
				msm_host->pdata->pm_qos_data.latency, cpu);
		msm_host->pm_qos_prev_cpu = cpu;
	}
}

static void sdhci_msm_post_req(struct sdhci_host *host,
				struct mmc_request *mmc_req)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	sdhci_msm_pm_qos_irq_unvote(host, false);

	if (sdhci_msm_pm_qos_cpu_unvote(host, msm_host->pm_qos_prev_cpu, false))
			msm_host->pm_qos_prev_cpu = -1;
}

static void sdhci_msm_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	sdhci_msm_pm_qos_irq_init(host);

	if (msm_host->pdata->pm_qos_data.legacy_valid)
		sdhci_msm_pm_qos_cpu_init(host,
				msm_host->pdata->pm_qos_data.latency);
}

static unsigned int sdhci_msm_get_current_limit(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_slot_reg_data *curr_slot = msm_host->pdata->vreg_data;
	u32 max_curr = 0;

	if (curr_slot && curr_slot->vdd_data)
		max_curr = curr_slot->vdd_data->hpm_uA;

	return max_curr;
}

static struct sdhci_ops sdhci_msm_ops = {
	.crypto_engine_cfg = sdhci_msm_ice_cfg,
	.crypto_cfg_reset = sdhci_msm_ice_cfg_reset,
	.crypto_engine_reset = sdhci_msm_ice_reset,
	.set_uhs_signaling = sdhci_msm_set_uhs_signaling,
	.check_power_status = sdhci_msm_check_power_status,
	.platform_execute_tuning = sdhci_msm_execute_tuning,
	.enhanced_strobe = sdhci_msm_enhanced_strobe,
	.toggle_cdr = sdhci_msm_toggle_cdr,
	.get_max_segments = sdhci_msm_max_segs,
	.set_clock = sdhci_msm_set_clock,
	.get_min_clock = sdhci_msm_get_min_clock,
	.get_max_clock = sdhci_msm_get_max_clock,
	.dump_vendor_regs = sdhci_msm_dump_vendor_regs,
	.config_auto_tuning_cmd = sdhci_msm_config_auto_tuning_cmd,
	.enable_controller_clock = sdhci_msm_enable_controller_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_msm_reset,
	.clear_set_dumpregs = sdhci_msm_clear_set_dumpregs,
	.enhanced_strobe_mask = sdhci_msm_enhanced_strobe_mask,
	.notify_load = sdhci_msm_notify_load,
	.reset_workaround = sdhci_msm_reset_workaround,
	.init = sdhci_msm_init,
	.pre_req = sdhci_msm_pre_req,
	.post_req = sdhci_msm_post_req,
	.get_current_limit = sdhci_msm_get_current_limit,
};

static void sdhci_set_default_hw_caps(struct sdhci_msm_host *msm_host,
		struct sdhci_host *host)
{
	u32 version, caps = 0;
	u16 minor;
	u8 major;
	u32 val;

	version = readl_relaxed(msm_host->core_mem + CORE_MCI_VERSION);
	major = (version & CORE_VERSION_MAJOR_MASK) >>
			CORE_VERSION_MAJOR_SHIFT;
	minor = version & CORE_VERSION_TARGET_MASK;

	caps = readl_relaxed(host->ioaddr + SDHCI_CAPABILITIES);

	/*
	 * Starting with SDCC 5 controller (core major version = 1)
	 * controller won't advertise 3.0v, 1.8v and 8-bit features
	 * except for some targets.
	 */
	if (major >= 1 && minor != 0x11 && minor != 0x12) {
		struct sdhci_msm_reg_data *vdd_io_reg;
		/*
		 * Enable 1.8V support capability on controllers that
		 * support dual voltage
		 */
		vdd_io_reg = msm_host->pdata->vreg_data->vdd_io_data;
		if (vdd_io_reg && (vdd_io_reg->high_vol_level > 2700000))
			caps |= CORE_3_0V_SUPPORT;
		if (vdd_io_reg && (vdd_io_reg->low_vol_level < 1950000))
			caps |= CORE_1_8V_SUPPORT;
		if (msm_host->pdata->mmc_bus_width == MMC_CAP_8_BIT_DATA)
			caps |= CORE_8_BIT_SUPPORT;
	}

	/*
	 * Enable one MID mode for SDCC5 (major 1) on 8916/8939 (minor 0x2e) and
	 * on 8992 (minor 0x3e) as a workaround to reset for data stuck issue.
	 */
	if (major == 1 && (minor == 0x2e || minor == 0x3e)) {
		host->quirks2 |= SDHCI_QUIRK2_USE_RESET_WORKAROUND;
		val = readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC_FUNC2);
		writel_relaxed((val | CORE_ONE_MID_EN),
			host->ioaddr + CORE_VENDOR_SPEC_FUNC2);
	}
	/*
	 * SDCC 5 controller with major version 1, minor version 0x34 and later
	 * with HS 400 mode support will use CM DLL instead of CDC LP 533 DLL.
	 */
	if ((major == 1) && (minor < 0x34))
		msm_host->use_cdclp533 = true;

	/*
	 * SDCC 5 controller with major version 1, minor version 0x42 and later
	 * will require additional steps when resetting DLL.
	 * It also supports HS400 enhanced strobe mode.
	 */
	if ((major == 1) && (minor >= 0x42)) {
		msm_host->use_updated_dll_reset = true;
		msm_host->enhanced_strobe = true;
	}

	/*
	 * SDCC 5 controller with major version 1 and minor version 0x42,
	 * 0x46 and 0x49 currently uses 14lpp tech DLL whose internal
	 * gating cannot guarantee MCLK timing requirement i.e.
	 * when MCLK is gated OFF, it is not gated for less than 0.5us
	 * and MCLK must be switched on for at-least 1us before DATA
	 * starts coming.
	 */
	if ((major == 1) && ((minor == 0x42) || (minor == 0x46) ||
				(minor == 0x49)))
		msm_host->use_14lpp_dll = true;

	/* Fake 3.0V support for SDIO devices which requires such voltage */
	if (msm_host->core_3_0v_support) {
		caps |= CORE_3_0V_SUPPORT;
			writel_relaxed(
			(readl_relaxed(host->ioaddr + SDHCI_CAPABILITIES) |
			caps), host->ioaddr + CORE_VENDOR_SPEC_CAPABILITIES0);
	}

	if ((major == 1) && (minor >= 0x49))
		msm_host->rclk_delay_fix = true;
	/*
	 * Mask 64-bit support for controller with 32-bit address bus so that
	 * smaller descriptor size will be used and improve memory consumption.
	 */
	if (!msm_host->pdata->largeaddressbus)
		caps &= ~CORE_SYS_BUS_SUPPORT_64_BIT;

	writel_relaxed(caps, host->ioaddr + CORE_VENDOR_SPEC_CAPABILITIES0);
	/* keep track of the value in SDHCI_CAPABILITIES */
	msm_host->caps_0 = caps;
}

#ifdef CONFIG_MMC_CQ_HCI
static void sdhci_msm_cmdq_init(struct sdhci_host *host,
				struct platform_device *pdev)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (nocmdq) {
		dev_dbg(&pdev->dev, "CMDQ disabled via cmdline\n");
		return;
	}

	host->cq_host = cmdq_pltfm_init(pdev);
	if (IS_ERR(host->cq_host)) {
		dev_dbg(&pdev->dev, "cmdq-pltfm init: failed: %ld\n",
			PTR_ERR(host->cq_host));
		host->cq_host = NULL;
	} else {
		msm_host->mmc->caps2 |= MMC_CAP2_CMD_QUEUE;
	}
}
#else
static void sdhci_msm_cmdq_init(struct sdhci_host *host,
				struct platform_device *pdev)
{

}
#endif

static bool sdhci_msm_is_bootdevice(struct device *dev)
{
	if (strnstr(saved_command_line, "androidboot.bootdevice=",
		    strlen(saved_command_line))) {
		char search_string[50];

		snprintf(search_string, ARRAY_SIZE(search_string),
			"androidboot.bootdevice=%s", dev_name(dev));
		if (strnstr(saved_command_line, search_string,
		    strlen(saved_command_line)))
			return true;
		else
			return false;
	}

	/*
	 * "androidboot.bootdevice=" argument is not present then
	 * return true as we don't know the boot device anyways.
	 */
	return true;
}

static int sdhci_msm_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_msm_host *msm_host;
	struct resource *core_memres = NULL;
	int ret = 0, dead = 0;
	u16 host_version;
	u32 irq_status, irq_ctl;
	struct resource *tlmm_memres = NULL;
	void __iomem *tlmm_mem;
	unsigned long flags;

	pr_debug("%s: Enter %s\n", dev_name(&pdev->dev), __func__);
	msm_host = devm_kzalloc(&pdev->dev, sizeof(struct sdhci_msm_host),
				GFP_KERNEL);
	if (!msm_host) {
		ret = -ENOMEM;
		goto out;
	}

	msm_host->sdhci_msm_pdata.ops = &sdhci_msm_ops;
	host = sdhci_pltfm_init(pdev, &msm_host->sdhci_msm_pdata, 0);
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		goto out_host_free;
	}

	pltfm_host = sdhci_priv(host);
	pltfm_host->priv = msm_host;
	msm_host->mmc = host->mmc;
	msm_host->pdev = pdev;

	/* get the ice device vops if present */
	ret = sdhci_msm_ice_get_dev(host);
	if (ret == -EPROBE_DEFER) {
		/*
		 * SDHCI driver might be probed before ICE driver does.
		 * In that case we would like to return EPROBE_DEFER code
		 * in order to delay its probing.
		 */
		dev_err(&pdev->dev, "%s: required ICE device not probed yet err = %d\n",
			__func__, ret);
		goto pltfm_free;

	} else if (ret == -ENODEV) {
		/*
		 * ICE device is not enabled in DTS file. No need for further
		 * initialization of ICE driver.
		 */
		dev_warn(&pdev->dev, "%s: ICE device is not enabled",
			__func__);
	} else if (ret) {
		dev_err(&pdev->dev, "%s: sdhci_msm_ice_get_dev failed %d\n",
			__func__, ret);
		goto pltfm_free;
	}

	/* Extract platform data */
	if (pdev->dev.of_node) {
		ret = of_alias_get_id(pdev->dev.of_node, "sdhc");
		if (ret <= 0) {
			dev_err(&pdev->dev, "Failed to get slot index %d\n",
				ret);
			goto pltfm_free;
		}

		/* skip the probe if eMMC isn't a boot device */
		if ((ret == 1) && !sdhci_msm_is_bootdevice(&pdev->dev)) {
			ret = -ENODEV;
			goto pltfm_free;
		}

		if (disable_slots & (1 << (ret - 1))) {
			dev_info(&pdev->dev, "%s: Slot %d disabled\n", __func__,
				ret);
			ret = -ENODEV;
			goto pltfm_free;
		}

		if (ret <= 2)
			sdhci_slot[ret-1] = msm_host;

		msm_host->pdata = sdhci_msm_populate_pdata(&pdev->dev,
							   msm_host);
		if (!msm_host->pdata) {
			dev_err(&pdev->dev, "DT parsing error\n");
			goto pltfm_free;
		}
	} else {
		dev_err(&pdev->dev, "No device tree node\n");
		goto pltfm_free;
	}

	/* Setup Clocks */

	/* Setup SDCC bus voter clock. */
	msm_host->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (!IS_ERR_OR_NULL(msm_host->bus_clk)) {
		/* Vote for max. clk rate for max. performance */
		ret = clk_set_rate(msm_host->bus_clk, INT_MAX);
		if (ret)
			goto pltfm_free;
		ret = clk_prepare_enable(msm_host->bus_clk);
		if (ret)
			goto pltfm_free;
	}

	/* Setup main peripheral bus clock */
	msm_host->pclk = devm_clk_get(&pdev->dev, "iface_clk");
	if (!IS_ERR(msm_host->pclk)) {
		ret = clk_prepare_enable(msm_host->pclk);
		if (ret)
			goto bus_clk_disable;
	}
	atomic_set(&msm_host->controller_clock, 1);

	if (msm_host->ice.pdev) {
		/* Setup SDC ICE clock */
		msm_host->ice_clk = devm_clk_get(&pdev->dev, "ice_core_clk");
		if (!IS_ERR(msm_host->ice_clk)) {
			/* ICE core has only one clock frequency for now */
			ret = clk_set_rate(msm_host->ice_clk,
					msm_host->pdata->ice_clk_max);
			if (ret) {
				dev_err(&pdev->dev, "ICE_CLK rate set failed (%d) for %u\n",
					ret,
					msm_host->pdata->ice_clk_max);
				goto pclk_disable;
			}
			ret = clk_prepare_enable(msm_host->ice_clk);
			if (ret)
				goto pclk_disable;

			msm_host->ice_clk_rate =
				msm_host->pdata->ice_clk_max;
		}
	}

	/* Setup SDC MMC clock */
	msm_host->clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(msm_host->clk)) {
		ret = PTR_ERR(msm_host->clk);
		goto pclk_disable;
	}

	/* Set to the minimum supported clock frequency */
	ret = clk_set_rate(msm_host->clk, sdhci_msm_get_min_clock(host));
	if (ret) {
		dev_err(&pdev->dev, "MClk rate set failed (%d)\n", ret);
		goto pclk_disable;
	}
	ret = clk_prepare_enable(msm_host->clk);
	if (ret)
		goto pclk_disable;

	msm_host->clk_rate = sdhci_msm_get_min_clock(host);
	atomic_set(&msm_host->clks_on, 1);

	/* Setup CDC calibration fixed feedback clock */
	msm_host->ff_clk = devm_clk_get(&pdev->dev, "cal_clk");
	if (!IS_ERR(msm_host->ff_clk)) {
		ret = clk_prepare_enable(msm_host->ff_clk);
		if (ret)
			goto clk_disable;
	}

	/* Setup CDC calibration sleep clock */
	msm_host->sleep_clk = devm_clk_get(&pdev->dev, "sleep_clk");
	if (!IS_ERR(msm_host->sleep_clk)) {
		ret = clk_prepare_enable(msm_host->sleep_clk);
		if (ret)
			goto ff_clk_disable;
	}

	msm_host->saved_tuning_phase = INVALID_TUNING_PHASE;

	ret = sdhci_msm_bus_register(msm_host, pdev);
	if (ret)
		goto sleep_clk_disable;

	if (msm_host->msm_bus_vote.client_handle)
		INIT_DELAYED_WORK(&msm_host->msm_bus_vote.vote_work,
				  sdhci_msm_bus_work);
	sdhci_msm_bus_voting(host, 1);

	/* Setup regulators */
	ret = sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, true);
	if (ret) {
		dev_err(&pdev->dev, "Regulator setup failed (%d)\n", ret);
		goto bus_unregister;
	}

	/* Reset the core and Enable SDHC mode */
	core_memres = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "core_mem");
	if (!core_memres) {
		dev_err(&pdev->dev, "Failed to get iomem resource\n");
		goto vreg_deinit;
	}
	msm_host->core_mem = devm_ioremap(&pdev->dev, core_memres->start,
					resource_size(core_memres));

	if (!msm_host->core_mem) {
		dev_err(&pdev->dev, "Failed to remap registers\n");
		ret = -ENOMEM;
		goto vreg_deinit;
	}

	tlmm_memres = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tlmm_mem");
	if (tlmm_memres) {
		tlmm_mem = devm_ioremap(&pdev->dev, tlmm_memres->start,
						resource_size(tlmm_memres));

		if (!tlmm_mem) {
			dev_err(&pdev->dev, "Failed to remap tlmm registers\n");
			ret = -ENOMEM;
			goto vreg_deinit;
		}
		writel_relaxed(readl_relaxed(tlmm_mem) | 0x2, tlmm_mem);
		dev_dbg(&pdev->dev, "tlmm reg %pa value 0x%08x\n",
				&tlmm_memres->start, readl_relaxed(tlmm_mem));
	}

	/*
	 * Reset the vendor spec register to power on reset state.
	 */
	writel_relaxed(CORE_VENDOR_SPEC_POR_VAL,
			host->ioaddr + CORE_VENDOR_SPEC);

	/* Set HC_MODE_EN bit in HC_MODE register */
	writel_relaxed(HC_MODE_EN, (msm_host->core_mem + CORE_HC_MODE));

	/* Set FF_CLK_SW_RST_DIS bit in HC_MODE register */
	writel_relaxed(readl_relaxed(msm_host->core_mem + CORE_HC_MODE) |
			FF_CLK_SW_RST_DIS, msm_host->core_mem + CORE_HC_MODE);

	sdhci_set_default_hw_caps(msm_host, host);

	/*
	 * Set the PAD_PWR_SWTICH_EN bit so that the PAD_PWR_SWITCH bit can
	 * be used as required later on.
	 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) |
			CORE_IO_PAD_PWR_SWITCH_EN),
			host->ioaddr + CORE_VENDOR_SPEC);
	/*
	 * CORE_SW_RST above may trigger power irq if previous status of PWRCTL
	 * was either BUS_ON or IO_HIGH_V. So before we enable the power irq
	 * interrupt in GIC (by registering the interrupt handler), we need to
	 * ensure that any pending power irq interrupt status is acknowledged
	 * otherwise power irq interrupt handler would be fired prematurely.
	 */
	irq_status = readl_relaxed(msm_host->core_mem + CORE_PWRCTL_STATUS);
	writel_relaxed(irq_status, (msm_host->core_mem + CORE_PWRCTL_CLEAR));
	irq_ctl = readl_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL);
	if (irq_status & (CORE_PWRCTL_BUS_ON | CORE_PWRCTL_BUS_OFF))
		irq_ctl |= CORE_PWRCTL_BUS_SUCCESS;
	if (irq_status & (CORE_PWRCTL_IO_HIGH | CORE_PWRCTL_IO_LOW))
		irq_ctl |= CORE_PWRCTL_IO_SUCCESS;
	writel_relaxed(irq_ctl, (msm_host->core_mem + CORE_PWRCTL_CTL));

	/*
	 * Ensure that above writes are propogated before interrupt enablement
	 * in GIC.
	 */
	mb();

	/*
	 * Following are the deviations from SDHC spec v3.0 -
	 * 1. Card detection is handled using separate GPIO.
	 * 2. Bus power control is handled by interacting with PMIC.
	 */
	host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
	host->quirks |= SDHCI_QUIRK_SINGLE_POWER_WRITE;
	host->quirks |= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN;
	host->quirks |= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC;
	host->quirks2 |= SDHCI_QUIRK2_ALWAYS_USE_BASE_CLOCK;
	host->quirks2 |= SDHCI_QUIRK2_IGNORE_DATATOUT_FOR_R1BCMD;
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_PRESET_VALUE;
	host->quirks2 |= SDHCI_QUIRK2_USE_RESERVED_MAX_TIMEOUT;
	host->quirks2 |= SDHCI_QUIRK2_NON_STANDARD_TUNING;
	host->quirks2 |= SDHCI_QUIRK2_USE_PIO_FOR_EMMC_TUNING;

	if (host->quirks2 & SDHCI_QUIRK2_ALWAYS_USE_BASE_CLOCK)
		host->quirks2 |= SDHCI_QUIRK2_DIVIDE_TOUT_BY_4;

	host_version = readw_relaxed((host->ioaddr + SDHCI_HOST_VERSION));
	dev_dbg(&pdev->dev, "Host Version: 0x%x Vendor Version 0x%x\n",
		host_version, ((host_version & SDHCI_VENDOR_VER_MASK) >>
		  SDHCI_VENDOR_VER_SHIFT));
	if (((host_version & SDHCI_VENDOR_VER_MASK) >>
		SDHCI_VENDOR_VER_SHIFT) == SDHCI_VER_100) {
		/*
		 * Add 40us delay in interrupt handler when
		 * operating at initialization frequency(400KHz).
		 */
		host->quirks2 |= SDHCI_QUIRK2_SLOW_INT_CLR;
		/*
		 * Set Software Reset for DAT line in Software
		 * Reset Register (Bit 2).
		 */
		host->quirks2 |= SDHCI_QUIRK2_RDWR_TX_ACTIVE_EOT;
	}

	host->quirks2 |= SDHCI_QUIRK2_IGN_DATA_END_BIT_ERROR;
	host->quirks2 |= SDHCI_QUIRK2_ADMA_SKIP_DATA_ALIGNMENT;

	/* Setup PWRCTL irq */
	msm_host->pwr_irq = platform_get_irq_byname(pdev, "pwr_irq");
	if (msm_host->pwr_irq < 0) {
		dev_err(&pdev->dev, "Failed to get pwr_irq by name (%d)\n",
				msm_host->pwr_irq);
		goto vreg_deinit;
	}
	ret = devm_request_threaded_irq(&pdev->dev, msm_host->pwr_irq, NULL,
					sdhci_msm_pwr_irq, IRQF_ONESHOT,
					dev_name(&pdev->dev), host);
	if (ret) {
		dev_err(&pdev->dev, "Request threaded irq(%d) failed (%d)\n",
				msm_host->pwr_irq, ret);
		goto vreg_deinit;
	}

	/* Enable pwr irq interrupts */
	writel_relaxed(INT_MASK, (msm_host->core_mem + CORE_PWRCTL_MASK));

	/* Set clock gating delay to be used when CONFIG_MMC_CLKGATE is set */
	msm_host->mmc->clkgate_delay = SDHCI_MSM_MMC_CLK_GATE_DELAY;

	/* Set host capabilities */
	msm_host->mmc->caps |= msm_host->pdata->mmc_bus_width;
	msm_host->mmc->caps |= msm_host->pdata->caps;
	msm_host->mmc->caps |= MMC_CAP_AGGRESSIVE_PM;
	msm_host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;
	msm_host->mmc->caps2 |= msm_host->pdata->caps2;
	msm_host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC;
	msm_host->mmc->caps2 |= MMC_CAP2_HS400_POST_TUNING;
	msm_host->mmc->caps2 |= MMC_CAP2_CLK_SCALE;
	msm_host->mmc->caps2 |= MMC_CAP2_SANITIZE;
	msm_host->mmc->caps2 |= MMC_CAP2_MAX_DISCARD_SIZE;
	msm_host->mmc->caps2 |= MMC_CAP2_SLEEP_AWAKE;
	msm_host->mmc->pm_caps |= MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;

	if (msm_host->pdata->nonremovable)
		msm_host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	if (msm_host->pdata->nonhotplug)
		msm_host->mmc->caps2 |= MMC_CAP2_NONHOTPLUG;

	msm_host->mmc->sdr104_wa = msm_host->pdata->sdr104_wa;

	/* Initialize ICE if present */
	if (msm_host->ice.pdev) {
		ret = sdhci_msm_ice_init(host);
		if (ret) {
			dev_err(&pdev->dev, "%s: SDHCi ICE init failed (%d)\n",
					mmc_hostname(host->mmc), ret);
			ret = -EINVAL;
			goto vreg_deinit;
		}
		host->is_crypto_en = true;
		/* Packed commands cannot be encrypted/decrypted using ICE */
		msm_host->mmc->caps2 &= ~(MMC_CAP2_PACKED_WR |
				MMC_CAP2_PACKED_WR_CONTROL);
	}

	init_completion(&msm_host->pwr_irq_completion);

	if (gpio_is_valid(msm_host->pdata->status_gpio)) {
		/*
		 * Set up the card detect GPIO in active configuration before
		 * configuring it as an IRQ. Otherwise, it can be in some
		 * weird/inconsistent state resulting in flood of interrupts.
		 */
		sdhci_msm_setup_pins(msm_host->pdata, true);

		/*
		 * This delay is needed for stabilizing the card detect GPIO
		 * line after changing the pull configs.
		 */
		usleep_range(10000, 10500);
		ret = mmc_gpio_request_cd(msm_host->mmc,
				msm_host->pdata->status_gpio, 0);
		if (ret) {
			dev_err(&pdev->dev, "%s: Failed to request card detection IRQ %d\n",
					__func__, ret);
			goto vreg_deinit;
		}
	}

	if ((sdhci_readl(host, SDHCI_CAPABILITIES) & SDHCI_CAN_64BIT) &&
		(dma_supported(mmc_dev(host->mmc), DMA_BIT_MASK(64)))) {
		host->dma_mask = DMA_BIT_MASK(64);
		mmc_dev(host->mmc)->dma_mask = &host->dma_mask;
		mmc_dev(host->mmc)->coherent_dma_mask  = host->dma_mask;
	} else if (dma_supported(mmc_dev(host->mmc), DMA_BIT_MASK(32))) {
		host->dma_mask = DMA_BIT_MASK(32);
		mmc_dev(host->mmc)->dma_mask = &host->dma_mask;
		mmc_dev(host->mmc)->coherent_dma_mask  = host->dma_mask;
	} else {
		dev_err(&pdev->dev, "%s: Failed to set dma mask\n", __func__);
	}

	msm_host->pdata->sdiowakeup_irq = platform_get_irq_byname(pdev,
							  "sdiowakeup_irq");
	if (sdhci_is_valid_gpio_wakeup_int(msm_host)) {
		dev_info(&pdev->dev, "%s: sdiowakeup_irq = %d\n", __func__,
				msm_host->pdata->sdiowakeup_irq);
		msm_host->is_sdiowakeup_enabled = true;
		ret = request_irq(msm_host->pdata->sdiowakeup_irq,
				  sdhci_msm_sdiowakeup_irq,
				  IRQF_SHARED | IRQF_TRIGGER_HIGH,
				  "sdhci-msm sdiowakeup", host);
		if (ret) {
			dev_err(&pdev->dev, "%s: request sdiowakeup IRQ %d: failed: %d\n",
				__func__, msm_host->pdata->sdiowakeup_irq, ret);
			msm_host->pdata->sdiowakeup_irq = -1;
			msm_host->is_sdiowakeup_enabled = false;
			goto free_cd_gpio;
		} else {
			spin_lock_irqsave(&host->lock, flags);
			sdhci_msm_cfg_sdiowakeup_gpio_irq(host, false);
			msm_host->sdio_pending_processing = false;
			spin_unlock_irqrestore(&host->lock, flags);
		}
	}

	sdhci_msm_cmdq_init(host, pdev);
	ret = sdhci_add_host(host);
	if (ret) {
		dev_err(&pdev->dev, "Add host failed (%d)\n", ret);
		goto free_cd_gpio;
	}

	msm_host->pltfm_init_done = true;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, MSM_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(&pdev->dev);

	msm_host->msm_bus_vote.max_bus_bw.show = show_sdhci_max_bus_bw;
	msm_host->msm_bus_vote.max_bus_bw.store = store_sdhci_max_bus_bw;
	sysfs_attr_init(&msm_host->msm_bus_vote.max_bus_bw.attr);
	msm_host->msm_bus_vote.max_bus_bw.attr.name = "max_bus_bw";
	msm_host->msm_bus_vote.max_bus_bw.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev,
			&msm_host->msm_bus_vote.max_bus_bw);
	if (ret)
		goto remove_host;

	if (!gpio_is_valid(msm_host->pdata->status_gpio)) {
		msm_host->polling.show = show_polling;
		msm_host->polling.store = store_polling;
		sysfs_attr_init(&msm_host->polling.attr);
		msm_host->polling.attr.name = "polling";
		msm_host->polling.attr.mode = S_IRUGO | S_IWUSR;
		ret = device_create_file(&pdev->dev, &msm_host->polling);
		if (ret)
			goto remove_max_bus_bw_file;
	}

	msm_host->auto_cmd21_attr.show = show_auto_cmd21;
	msm_host->auto_cmd21_attr.store = store_auto_cmd21;
	sysfs_attr_init(&msm_host->auto_cmd21_attr.attr);
	msm_host->auto_cmd21_attr.attr.name = "enable_auto_cmd21";
	msm_host->auto_cmd21_attr.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(&pdev->dev, &msm_host->auto_cmd21_attr);
	if (ret) {
		pr_err("%s: %s: failed creating auto-cmd21 attr: %d\n",
		       mmc_hostname(host->mmc), __func__, ret);
		device_remove_file(&pdev->dev, &msm_host->auto_cmd21_attr);
	}
	/* Successful initialization */
	goto out;

remove_max_bus_bw_file:
	device_remove_file(&pdev->dev, &msm_host->msm_bus_vote.max_bus_bw);
remove_host:
	dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);
	pm_runtime_disable(&pdev->dev);
	sdhci_remove_host(host, dead);
free_cd_gpio:
	if (gpio_is_valid(msm_host->pdata->status_gpio))
		mmc_gpio_free_cd(msm_host->mmc);
vreg_deinit:
	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);
bus_unregister:
	if (msm_host->msm_bus_vote.client_handle)
		sdhci_msm_bus_cancel_work_and_set_vote(host, 0);
	sdhci_msm_bus_unregister(msm_host);
sleep_clk_disable:
	if (!IS_ERR(msm_host->sleep_clk))
		clk_disable_unprepare(msm_host->sleep_clk);
ff_clk_disable:
	if (!IS_ERR(msm_host->ff_clk))
		clk_disable_unprepare(msm_host->ff_clk);
clk_disable:
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
pclk_disable:
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
bus_clk_disable:
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
pltfm_free:
	sdhci_pltfm_free(pdev);
out_host_free:
	devm_kfree(&pdev->dev, msm_host);
out:
	pr_debug("%s: Exit %s\n", dev_name(&pdev->dev), __func__);
	return ret;
}

static int sdhci_msm_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pltfm_data *pdata = msm_host->pdata;
	int dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) ==
			0xffffffff);

	pr_debug("%s: %s\n", dev_name(&pdev->dev), __func__);
	if (!gpio_is_valid(msm_host->pdata->status_gpio))
		device_remove_file(&pdev->dev, &msm_host->polling);
	device_remove_file(&pdev->dev, &msm_host->msm_bus_vote.max_bus_bw);
	pm_runtime_disable(&pdev->dev);
	sdhci_remove_host(host, dead);
	sdhci_pltfm_free(pdev);

	if (gpio_is_valid(msm_host->pdata->status_gpio))
		mmc_gpio_free_cd(msm_host->mmc);

	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);

	sdhci_msm_setup_pins(pdata, true);
	sdhci_msm_setup_pins(pdata, false);

	if (msm_host->msm_bus_vote.client_handle) {
		sdhci_msm_bus_cancel_work_and_set_vote(host, 0);
		sdhci_msm_bus_unregister(msm_host);
	}
	return 0;
}

#ifdef CONFIG_PM
static int sdhci_msm_cfg_sdio_wakeup(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	unsigned long flags;
	int ret = 0;

	if (!(host->mmc->card && mmc_card_sdio(host->mmc->card) &&
	      sdhci_is_valid_gpio_wakeup_int(msm_host) &&
	      mmc_card_wake_sdio_irq(host->mmc))) {
		msm_host->sdio_pending_processing = false;
		return 1;
	}

	spin_lock_irqsave(&host->lock, flags);
	if (enable) {
		/* configure DAT1 gpio if applicable */
		if (sdhci_is_valid_gpio_wakeup_int(msm_host)) {
			msm_host->sdio_pending_processing = false;
			ret = enable_irq_wake(msm_host->pdata->sdiowakeup_irq);
			if (!ret)
				sdhci_msm_cfg_sdiowakeup_gpio_irq(host, true);
			goto out;
		} else {
			pr_err("%s: sdiowakeup_irq(%d) invalid\n",
					mmc_hostname(host->mmc), enable);
		}
	} else {
		if (sdhci_is_valid_gpio_wakeup_int(msm_host)) {
			ret = disable_irq_wake(msm_host->pdata->sdiowakeup_irq);
			sdhci_msm_cfg_sdiowakeup_gpio_irq(host, false);
			msm_host->sdio_pending_processing = false;
		} else {
			pr_err("%s: sdiowakeup_irq(%d)invalid\n",
					mmc_hostname(host->mmc), enable);

		}
	}
out:
	if (ret)
		pr_err("%s: %s: %sable wakeup: failed: %d gpio: %d\n",
		       mmc_hostname(host->mmc), __func__, enable ? "en" : "dis",
		       ret, msm_host->pdata->sdiowakeup_irq);
	spin_unlock_irqrestore(&host->lock, flags);
	return ret;
}


static int sdhci_msm_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	ktime_t start = ktime_get();
	int ret;

	if (host->mmc->card && mmc_card_sdio(host->mmc->card))
		goto defer_disable_host_irq;

	sdhci_cfg_irq(host, false, true);

defer_disable_host_irq:
	disable_irq(msm_host->pwr_irq);

	/*
	 * Remove the vote immediately only if clocks are off in which
	 * case we might have queued work to remove vote but it may not
	 * be completed before runtime suspend or system suspend.
	 */
	if (!atomic_read(&msm_host->clks_on)) {
		if (msm_host->msm_bus_vote.client_handle)
			sdhci_msm_bus_cancel_work_and_set_vote(host, 0);
	}

	if (host->is_crypto_en) {
		ret = sdhci_msm_ice_suspend(host);
		if (ret < 0)
			pr_err("%s: failed to suspend crypto engine %d\n",
					mmc_hostname(host->mmc), ret);
	}
	trace_sdhci_msm_runtime_suspend(mmc_hostname(host->mmc), 0,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return 0;
}

static int sdhci_msm_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	ktime_t start = ktime_get();
	int ret;

	if (host->is_crypto_en) {
		ret = sdhci_msm_enable_controller_clock(host);
		if (ret) {
			pr_err("%s: Failed to enable reqd clocks\n",
					mmc_hostname(host->mmc));
			goto skip_ice_resume;
		}
		ret = sdhci_msm_ice_resume(host);
		if (ret)
			pr_err("%s: failed to resume crypto engine %d\n",
					mmc_hostname(host->mmc), ret);
	}
skip_ice_resume:
	if (host->mmc->card && mmc_card_sdio(host->mmc->card))
		goto defer_enable_host_irq;

	sdhci_cfg_irq(host, true, true);

defer_enable_host_irq:
	enable_irq(msm_host->pwr_irq);

	trace_sdhci_msm_runtime_resume(mmc_hostname(host->mmc), 0,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return 0;
}

static int sdhci_msm_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;
	int sdio_cfg = 0;
	ktime_t start = ktime_get();

	if (gpio_is_valid(msm_host->pdata->status_gpio) &&
		(msm_host->mmc->slot.cd_irq >= 0))
			disable_irq(msm_host->mmc->slot.cd_irq);

	if (pm_runtime_suspended(dev)) {
		pr_debug("%s: %s: already runtime suspended\n",
		mmc_hostname(host->mmc), __func__);
		goto out;
	}
	ret = sdhci_msm_runtime_suspend(dev);
out:
	sdhci_msm_disable_controller_clock(host);
	if (host->mmc->card && mmc_card_sdio(host->mmc->card)) {
		sdio_cfg = sdhci_msm_cfg_sdio_wakeup(host, true);
		if (sdio_cfg)
			sdhci_cfg_irq(host, false, true);
	}

	trace_sdhci_msm_suspend(mmc_hostname(host->mmc), ret,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return ret;
}

static int sdhci_msm_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;
	int sdio_cfg = 0;
	ktime_t start = ktime_get();

	if (gpio_is_valid(msm_host->pdata->status_gpio) &&
		(msm_host->mmc->slot.cd_irq >= 0))
			enable_irq(msm_host->mmc->slot.cd_irq);

	if (pm_runtime_suspended(dev)) {
		pr_debug("%s: %s: runtime suspended, defer system resume\n",
		mmc_hostname(host->mmc), __func__);
		goto out;
	}

	ret = sdhci_msm_runtime_resume(dev);
out:
	if (host->mmc->card && mmc_card_sdio(host->mmc->card)) {
		sdio_cfg = sdhci_msm_cfg_sdio_wakeup(host, false);
		if (sdio_cfg)
			sdhci_cfg_irq(host, true, true);
	}

	trace_sdhci_msm_resume(mmc_hostname(host->mmc), ret,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return ret;
}

static int sdhci_msm_suspend_noirq(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;

	/*
	 * ksdioirqd may be running, hence retry
	 * suspend in case the clocks are ON
	 */
	if (atomic_read(&msm_host->clks_on)) {
		pr_warn("%s: %s: clock ON after suspend, aborting suspend\n",
			mmc_hostname(host->mmc), __func__);
		ret = -EAGAIN;
	}

	if (host->mmc->card && mmc_card_sdio(host->mmc->card))
		if (msm_host->sdio_pending_processing)
			ret = -EBUSY;

	return ret;
}

static const struct dev_pm_ops sdhci_msm_pmops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sdhci_msm_suspend, sdhci_msm_resume)
	SET_RUNTIME_PM_OPS(sdhci_msm_runtime_suspend, sdhci_msm_runtime_resume,
			   NULL)
	.suspend_noirq = sdhci_msm_suspend_noirq,
};

#define SDHCI_MSM_PMOPS (&sdhci_msm_pmops)

#else
#define SDHCI_MSM_PMOPS NULL
#endif
static const struct of_device_id sdhci_msm_dt_match[] = {
	{.compatible = "qcom,sdhci-msm"},
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_msm_dt_match);

static struct platform_driver sdhci_msm_driver = {
	.probe		= sdhci_msm_probe,
	.remove		= sdhci_msm_remove,
	.driver		= {
		.name	= "sdhci_msm",
		.owner	= THIS_MODULE,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_msm_dt_match,
		.pm	= SDHCI_MSM_PMOPS,
	},
};

module_platform_driver(sdhci_msm_driver);

MODULE_DESCRIPTION("Qualcomm Secure Digital Host Controller Interface driver");
MODULE_LICENSE("GPL v2");

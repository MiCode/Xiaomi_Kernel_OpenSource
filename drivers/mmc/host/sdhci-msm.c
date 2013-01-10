/*
 * drivers/mmc/host/sdhci-msm.c - Qualcomm MSM SDHCI Platform
 * driver source file
 *
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/mmc/mmc.h>
#include <mach/gpio.h>

#include "sdhci-pltfm.h"

#define SDHCI_VER_100		0x2B
#define CORE_HC_MODE		0x78
#define HC_MODE_EN		0x1

#define CORE_POWER		0x0
#define CORE_SW_RST		(1 << 7)

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

#define CORE_DLL_LOCK		(1 << 7)
#define CORE_DLL_EN		(1 << 16)
#define CORE_CDR_EN		(1 << 17)
#define CORE_CK_OUT_EN		(1 << 18)
#define CORE_CDR_EXT_EN		(1 << 19)
#define CORE_DLL_PDN		(1 << 29)
#define CORE_DLL_RST		(1 << 30)
#define CORE_DLL_CONFIG		0x100
#define CORE_DLL_TEST_CTL	0x104
#define CORE_DLL_STATUS		0x108

#define CORE_VENDOR_SPEC	0x10C
#define CORE_CLK_PWRSAVE	(1 << 1)
#define CORE_IO_PAD_PWR_SWITCH	(1 << 16)

/* 8KB descriptors */
#define SDHCI_MSM_MAX_SEGMENTS  (1 << 13)

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

/* This structure keeps information per regulator */
struct sdhci_msm_reg_data {
	/* voltage regulator handle */
	struct regulator *reg;
	/* regulator name */
	const char *name;
	/* voltage level to be set */
	u32 low_vol_level;
	u32 high_vol_level;
	/* Load values for low power and high power mode */
	u32 lpm_uA;
	u32 hpm_uA;

	/* is this regulator enabled? */
	bool is_enabled;
	/* is this regulator needs to be always on? */
	bool is_always_on;
	/* is low power mode setting required for this regulator? */
	bool lpm_sup;
};

/*
 * This structure keeps information for all the
 * regulators required for a SDCC slot.
 */
struct sdhci_msm_slot_reg_data {
	/* keeps VDD/VCC regulator info */
	struct sdhci_msm_reg_data *vdd_data;
	 /* keeps VDD IO regulator info */
	struct sdhci_msm_reg_data *vdd_io_data;
};

struct sdhci_msm_gpio {
	u32 no;
	const char *name;
	bool is_enabled;
};

struct sdhci_msm_gpio_data {
	struct sdhci_msm_gpio *gpio;
	u8 size;
};

struct sdhci_msm_pad_pull {
	enum msm_tlmm_pull_tgt no;
	u32 val;
};

struct sdhci_msm_pad_pull_data {
	struct sdhci_msm_pad_pull *on;
	struct sdhci_msm_pad_pull *off;
	u8 size;
};

struct sdhci_msm_pad_drv {
	enum msm_tlmm_hdrive_tgt no;
	u32 val;
};

struct sdhci_msm_pad_drv_data {
	struct sdhci_msm_pad_drv *on;
	struct sdhci_msm_pad_drv *off;
	u8 size;
};

struct sdhci_msm_pad_data {
	struct sdhci_msm_pad_pull_data *pull;
	struct sdhci_msm_pad_drv_data *drv;
};


struct sdhci_msm_pin_data {
	/*
	 * = 1 if controller pins are using gpios
	 * = 0 if controller has dedicated MSM pads
	 */
	u8 is_gpio;
	bool cfg_sts;
	struct sdhci_msm_gpio_data *gpio_data;
	struct sdhci_msm_pad_data *pad_data;
};

struct sdhci_msm_pltfm_data {
	/* Supported UHS-I Modes */
	u32 caps;

	/* More capabilities */
	u32 caps2;

	unsigned long mmc_bus_width;
	u32 max_clk;
	struct sdhci_msm_slot_reg_data *vreg_data;
	bool nonremovable;
	struct sdhci_msm_pin_data *pin_data;
};

struct sdhci_msm_host {
	void __iomem *core_mem;    /* MSM SDCC mapped address */
	struct clk	 *clk;     /* main SD/MMC bus clock */
	struct clk	 *pclk;    /* SDHC peripheral bus clock */
	struct clk	 *bus_clk; /* SDHC bus voter clock */
	struct sdhci_msm_pltfm_data *pdata;
	struct mmc_host  *mmc;
	struct sdhci_pltfm_data sdhci_msm_pdata;
	wait_queue_head_t pwr_irq_wait;
};

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
 * timing mode) or for eMMC4.5 card read operation (in HS200
 * timing mode).
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
	struct mmc_host *mmc = host->mmc;
	int rc = 0;
	unsigned long flags;
	u32 wait_cnt;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	spin_lock_irqsave(&host->lock, flags);

	/*
	 * Make sure that clock is always enabled when DLL
	 * tuning is in progress. Keeping PWRSAVE ON may
	 * turn off the clock. So let's disable the PWRSAVE
	 * here and re-enable it once tuning is completed.
	 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC)
			& ~CORE_CLK_PWRSAVE),
			host->ioaddr + CORE_VENDOR_SPEC);

	/* Write 1 to DLL_RST bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_DLL_RST), host->ioaddr + CORE_DLL_CONFIG);

	/* Write 1 to DLL_PDN bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			| CORE_DLL_PDN), host->ioaddr + CORE_DLL_CONFIG);
	msm_cm_dll_set_freq(host);

	/* Write 0 to DLL_RST bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			& ~CORE_DLL_RST), host->ioaddr + CORE_DLL_CONFIG);

	/* Write 0 to DLL_PDN bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_DLL_CONFIG)
			& ~CORE_DLL_PDN), host->ioaddr + CORE_DLL_CONFIG);

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
	/* re-enable PWRSAVE */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) |
			CORE_CLK_PWRSAVE),
			host->ioaddr + CORE_VENDOR_SPEC);
	spin_unlock_irqrestore(&host->lock, flags);
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return rc;
}

int sdhci_msm_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned long flags;
	u8 phase, *data_buf, tuned_phases[16], tuned_phase_cnt = 0;
	const u32 *tuning_block_pattern = tuning_block_64;
	int size = sizeof(tuning_block_64); /* Tuning pattern size in bytes */
	int rc;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	/* Tuning is only required for SDR104 modes */
	spin_lock_irqsave(&host->lock, flags);

	if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) &&
		(mmc->ios.bus_width == MMC_BUS_WIDTH_8)) {
		tuning_block_pattern = tuning_block_128;
		size = sizeof(tuning_block_128);
	}
	spin_unlock_irqrestore(&host->lock, flags);

	/* first of all reset the tuning block */
	rc = msm_init_cm_dll(host);
	if (rc)
		goto out;

	data_buf = kmalloc(size, GFP_KERNEL);
	if (!data_buf) {
		rc = -ENOMEM;
		goto out;
	}

	phase = 0;
	do {
		struct mmc_command cmd = {0};
		struct mmc_data data = {0};
		struct mmc_request mrq = {
			.cmd = &cmd,
			.data = &data
		};
		struct scatterlist sg;

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

		if (!cmd.error && !data.error &&
			!memcmp(data_buf, tuning_block_pattern, size)) {
			/* tuning is successful at this tuning point */
			tuned_phases[tuned_phase_cnt++] = phase;
			pr_debug("%s: %s: found good phase = %d\n",
				mmc_hostname(mmc), __func__, phase);
		}
	} while (++phase < 16);

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
		pr_debug("%s: %s: finally setting the tuning phase to %d\n",
				mmc_hostname(mmc), __func__, phase);
	} else {
		/* tuning failed */
		pr_err("%s: %s: no tuning point found\n",
			mmc_hostname(mmc), __func__);
		rc = -EAGAIN;
	}

kfree:
	kfree(data_buf);
out:
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
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

static int sdhci_msm_setup_pad(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	struct sdhci_msm_pad_data *curr;
	int i;

	curr = pdata->pin_data->pad_data;
	for (i = 0; i < curr->drv->size; i++) {
		if (enable)
			msm_tlmm_set_hdrive(curr->drv->on[i].no,
				curr->drv->on[i].val);
		else
			msm_tlmm_set_hdrive(curr->drv->off[i].no,
				curr->drv->off[i].val);
	}

	for (i = 0; i < curr->pull->size; i++) {
		if (enable)
			msm_tlmm_set_pull(curr->pull->on[i].no,
				curr->pull->on[i].val);
		else
			msm_tlmm_set_pull(curr->pull->off[i].no,
				curr->pull->off[i].val);
	}

	return 0;
}

static int sdhci_msm_setup_pins(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	int ret = 0;

	if (!pdata->pin_data || (pdata->pin_data->cfg_sts == enable))
		return 0;
	if (pdata->pin_data->is_gpio)
		ret = sdhci_msm_setup_gpio(pdata, enable);
	else
		ret = sdhci_msm_setup_pad(pdata, enable);

	if (!ret)
		pdata->pin_data->cfg_sts = enable;

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
	if (sz <= 0 || (size > 0 && (sz != size))) {
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
		dev_err(dev, "No vreg data found for %s\n", vreg_name);
		ret = -EINVAL;
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
static int sdhci_msm_dt_get_pad_pull_info(struct device *dev, int id,
		struct sdhci_msm_pad_pull_data **pad_pull_data)
{
	int ret = 0, base = 0, len, i;
	u32 *tmp;
	struct sdhci_msm_pad_pull_data *pull_data;
	struct sdhci_msm_pad_pull *pull;

	switch (id) {
	case 1:
		base = TLMM_PULL_SDC1_CLK;
		break;
	case 2:
		base = TLMM_PULL_SDC2_CLK;
		break;
	case 3:
		base = TLMM_PULL_SDC3_CLK;
		break;
	case 4:
		base = TLMM_PULL_SDC4_CLK;
		break;
	default:
		dev_err(dev, "%s: Invalid slot id\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	pull_data = devm_kzalloc(dev, sizeof(struct sdhci_msm_pad_pull_data),
			GFP_KERNEL);
	if (!pull_data) {
		dev_err(dev, "No memory for msm_mmc_pad_pull_data\n");
		ret = -ENOMEM;
		goto out;
	}
	pull_data->size = 3; /* array size for clk, cmd, data */

	/* Allocate on, off configs for clk, cmd, data */
	pull = devm_kzalloc(dev, 2 * pull_data->size *\
			sizeof(struct sdhci_msm_pad_pull), GFP_KERNEL);
	if (!pull) {
		dev_err(dev, "No memory for msm_mmc_pad_pull\n");
		ret = -ENOMEM;
		goto out;
	}
	pull_data->on = pull;
	pull_data->off = pull + pull_data->size;

	ret = sdhci_msm_dt_get_array(dev, "qcom,pad-pull-on",
			&tmp, &len, pull_data->size);
	if (ret)
		goto out;

	for (i = 0; i < len; i++) {
		pull_data->on[i].no = base + i;
		pull_data->on[i].val = tmp[i];
		dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
				i, pull_data->on[i].val);
	}

	ret = sdhci_msm_dt_get_array(dev, "qcom,pad-pull-off",
			&tmp, &len, pull_data->size);
	if (ret)
		goto out;

	for (i = 0; i < len; i++) {
		pull_data->off[i].no = base + i;
		pull_data->off[i].val = tmp[i];
		dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
				i, pull_data->off[i].val);
	}

	*pad_pull_data = pull_data;
out:
	return ret;
}

static int sdhci_msm_dt_get_pad_drv_info(struct device *dev, int id,
		struct sdhci_msm_pad_drv_data **pad_drv_data)
{
	int ret = 0, base = 0, len, i;
	u32 *tmp;
	struct sdhci_msm_pad_drv_data *drv_data;
	struct sdhci_msm_pad_drv *drv;

	switch (id) {
	case 1:
		base = TLMM_HDRV_SDC1_CLK;
		break;
	case 2:
		base = TLMM_HDRV_SDC2_CLK;
		break;
	case 3:
		base = TLMM_HDRV_SDC3_CLK;
		break;
	case 4:
		base = TLMM_HDRV_SDC4_CLK;
		break;
	default:
		dev_err(dev, "%s: Invalid slot id\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	drv_data = devm_kzalloc(dev, sizeof(struct sdhci_msm_pad_drv_data),
			GFP_KERNEL);
	if (!drv_data) {
		dev_err(dev, "No memory for msm_mmc_pad_drv_data\n");
		ret = -ENOMEM;
		goto out;
	}
	drv_data->size = 3; /* array size for clk, cmd, data */

	/* Allocate on, off configs for clk, cmd, data */
	drv = devm_kzalloc(dev, 2 * drv_data->size *\
			sizeof(struct sdhci_msm_pad_drv), GFP_KERNEL);
	if (!drv) {
		dev_err(dev, "No memory msm_mmc_pad_drv\n");
		ret = -ENOMEM;
		goto out;
	}
	drv_data->on = drv;
	drv_data->off = drv + drv_data->size;

	ret = sdhci_msm_dt_get_array(dev, "qcom,pad-drv-on",
			&tmp, &len, drv_data->size);
	if (ret)
		goto out;

	for (i = 0; i < len; i++) {
		drv_data->on[i].no = base + i;
		drv_data->on[i].val = tmp[i];
		dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
				i, drv_data->on[i].val);
	}

	ret = sdhci_msm_dt_get_array(dev, "qcom,pad-drv-off",
			&tmp, &len, drv_data->size);
	if (ret)
		goto out;

	for (i = 0; i < len; i++) {
		drv_data->off[i].no = base + i;
		drv_data->off[i].val = tmp[i];
		dev_dbg(dev, "%s: val[%d]=0x%x\n", __func__,
				i, drv_data->off[i].val);
	}

	*pad_drv_data = drv_data;
out:
	return ret;
}

#define GPIO_NAME_MAX_LEN 32
static int sdhci_msm_dt_parse_gpio_info(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	int ret = 0, id = 0, cnt, i;
	struct sdhci_msm_pin_data *pin_data;
	struct device_node *np = dev->of_node;

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
	} else {
		pin_data->pad_data =
			devm_kzalloc(dev,
				     sizeof(struct sdhci_msm_pad_data),
				     GFP_KERNEL);
		if (!pin_data->pad_data) {
			dev_err(dev,
				"No memory for pin_data->pad_data\n");
			ret = -ENOMEM;
			goto out;
		}

		ret = of_alias_get_id(np, "sdhc");
		if (ret < 0) {
			dev_err(dev, "Failed to get slot index %d\n", ret);
			goto out;
		}
		id = ret;

		ret = sdhci_msm_dt_get_pad_pull_info(
			dev, id, &pin_data->pad_data->pull);
		if (ret)
			goto out;
		ret = sdhci_msm_dt_get_pad_drv_info(
			dev, id, &pin_data->pad_data->drv);
		if (ret)
			goto out;

	}
	pdata->pin_data = pin_data;
out:
	if (ret)
		dev_err(dev, "%s failed with err %d\n", __func__, ret);
	return ret;
}

/* Parse platform data */
static struct sdhci_msm_pltfm_data *sdhci_msm_populate_pdata(struct device *dev)
{
	struct sdhci_msm_pltfm_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	u32 bus_width = 0;
	int len, i;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate memory for platform data\n");
		goto out;
	}

	of_property_read_u32(np, "qcom,bus-width", &bus_width);
	if (bus_width == 8)
		pdata->mmc_bus_width = MMC_CAP_8_BIT_DATA;
	else if (bus_width == 4)
		pdata->mmc_bus_width = MMC_CAP_4_BIT_DATA;
	else {
		dev_notice(dev, "invalid bus-width, default to 1-bit mode\n");
		pdata->mmc_bus_width = 0;
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

	of_property_read_u32(np, "qcom,max-clk-rate", &pdata->max_clk);

	len = of_property_count_strings(np, "qcom,bus-speed-mode");

	for (i = 0; i < len; i++) {
		const char *name = NULL;

		of_property_read_string_index(np,
			"qcom,bus-speed-mode", i, &name);
		if (!name)
			continue;

		if (!strncmp(name, "HS200_1p8v", sizeof("HS200_1p8v")))
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

	return pdata;
out:
	return NULL;
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

	/* sanity check */
	if (!vreg->high_vol_level || !vreg->hpm_uA) {
		pr_err("%s: %s invalid constraints specified\n",
		       __func__, vreg->name);
		ret = -EINVAL;
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
	return ret;
}

static int sdhci_msm_vreg_set_voltage(struct sdhci_msm_reg_data *vreg,
					int min_uV, int max_uV)
{
	int ret = 0;

	ret = regulator_set_voltage(vreg->reg, min_uV, max_uV);
	if (ret) {
		pr_err("%s: regulator_set_voltage(%s)failed. min_uV=%d,max_uV=%d,ret=%d\n",
			       __func__, vreg->name, min_uV, max_uV, ret);
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

/*
 * Reset vreg by ensuring it is off during probe. A call
 * to enable vreg is needed to balance disable vreg
 */
static int sdhci_msm_vreg_reset(struct sdhci_msm_pltfm_data *pdata)
{
	int ret;

	ret = sdhci_msm_setup_vreg(pdata, 1, true);
	if (ret)
		return ret;
	ret = sdhci_msm_setup_vreg(pdata, 0, true);
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
	ret = sdhci_msm_vreg_reset(pdata);
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

static irqreturn_t sdhci_msm_pwr_irq(int irq, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u8 irq_status = 0;
	u8 irq_ack = 0;
	int ret = 0;

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

	/* Handle BUS ON/OFF*/
	if (irq_status & CORE_PWRCTL_BUS_ON) {
		ret = sdhci_msm_setup_vreg(msm_host->pdata, true, false);
		if (!ret)
			ret = sdhci_msm_setup_pins(msm_host->pdata, true);
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;
	}
	if (irq_status & CORE_PWRCTL_BUS_OFF) {
		ret = sdhci_msm_setup_vreg(msm_host->pdata, false, false);
		if (!ret)
			ret = sdhci_msm_setup_pins(msm_host->pdata, false);
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;
	}
	/* Handle IO LOW/HIGH */
	if (irq_status & CORE_PWRCTL_IO_LOW) {
		/* Switch voltage Low */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_LOW, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;
	}
	if (irq_status & CORE_PWRCTL_IO_HIGH) {
		/* Switch voltage High */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_HIGH, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;
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

	if (irq_status & CORE_PWRCTL_IO_HIGH)
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) &
				~CORE_IO_PAD_PWR_SWITCH),
				host->ioaddr + CORE_VENDOR_SPEC);
	if (irq_status & CORE_PWRCTL_IO_LOW)
		writel_relaxed((readl_relaxed(host->ioaddr + CORE_VENDOR_SPEC) |
				CORE_IO_PAD_PWR_SWITCH),
				host->ioaddr + CORE_VENDOR_SPEC);
	mb();

	pr_debug("%s: Handled IRQ(%d), ret=%d, ack=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, ret, irq_ack);
	wake_up_interruptible(&msm_host->pwr_irq_wait);
	return IRQ_HANDLED;
}

/* This function returns the max. current supported by VDD rail in mA */
static unsigned int sdhci_msm_get_vreg_vdd_max_current(struct sdhci_msm_host
						       *host)
{
	struct sdhci_msm_slot_reg_data *curr_slot = host->pdata->vreg_data;
	if (!curr_slot)
		return 0;
	if (curr_slot->vdd_data)
		return curr_slot->vdd_data->hpm_uA / 1000;
	else
		return 0;
}

static void sdhci_msm_check_power_status(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;

	pr_debug("%s: %s: power status before waiting 0x%x\n",
		mmc_hostname(host->mmc), __func__,
		readb_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL));

	ret = wait_event_interruptible(msm_host->pwr_irq_wait,
				       (readb_relaxed(msm_host->core_mem +
						      CORE_PWRCTL_CTL)) != 0x0);
	if (ret)
		pr_warning("%s: %s: returned due to error %d\n",
				mmc_hostname(host->mmc), __func__, ret);
	pr_debug("%s: %s: ret %d power status after handling power IRQ 0x%x\n",
		mmc_hostname(host->mmc), __func__, ret,
		readb_relaxed(msm_host->core_mem + CORE_PWRCTL_CTL));
}

static void sdhci_msm_toggle_cdr(struct sdhci_host *host, bool enable)
{
	if (enable)
		writel_relaxed((readl_relaxed(host->ioaddr +
					      CORE_DLL_CONFIG) | CORE_CDR_EN),
			       host->ioaddr + CORE_DLL_CONFIG);
	else
		writel_relaxed((readl_relaxed(host->ioaddr +
					      CORE_DLL_CONFIG) & ~CORE_CDR_EN),
			       host->ioaddr + CORE_DLL_CONFIG);
}

static unsigned int sdhci_msm_max_segs(void)
{
	return SDHCI_MSM_MAX_SEGMENTS;
}

static struct sdhci_ops sdhci_msm_ops = {
	.check_power_status = sdhci_msm_check_power_status,
	.execute_tuning = sdhci_msm_execute_tuning,
	.toggle_cdr = sdhci_msm_toggle_cdr,
	.get_max_segments = sdhci_msm_max_segs,
};

static int __devinit sdhci_msm_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_msm_host *msm_host;
	struct resource *core_memres = NULL;
	int ret = 0, pwr_irq = 0, dead = 0;
	u32 vdd_max_current;
	u32 host_version;

	pr_debug("%s: Enter %s\n", dev_name(&pdev->dev), __func__);
	msm_host = devm_kzalloc(&pdev->dev, sizeof(struct sdhci_msm_host),
				GFP_KERNEL);
	if (!msm_host) {
		ret = -ENOMEM;
		goto out;
	}
	init_waitqueue_head(&msm_host->pwr_irq_wait);

	msm_host->sdhci_msm_pdata.ops = &sdhci_msm_ops;
	host = sdhci_pltfm_init(pdev, &msm_host->sdhci_msm_pdata);
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		goto out;
	}

	pltfm_host = sdhci_priv(host);
	pltfm_host->priv = msm_host;
	msm_host->mmc = host->mmc;

	/* Extract platform data */
	if (pdev->dev.of_node) {
		msm_host->pdata = sdhci_msm_populate_pdata(&pdev->dev);
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

	/* Setup SDC MMC clock */
	msm_host->clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(msm_host->clk)) {
		ret = PTR_ERR(msm_host->clk);
		goto pclk_disable;
	}

	ret = clk_prepare_enable(msm_host->clk);
	if (ret)
		goto pclk_disable;

	/* Setup regulators */
	ret = sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, true);
	if (ret) {
		dev_err(&pdev->dev, "Regulator setup failed (%d)\n", ret);
		goto clk_disable;
	}

	/* Reset the core and Enable SDHC mode */
	core_memres = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "core_mem");
	msm_host->core_mem = devm_ioremap(&pdev->dev, core_memres->start,
					resource_size(core_memres));

	if (!msm_host->core_mem) {
		dev_err(&pdev->dev, "Failed to remap registers\n");
		ret = -ENOMEM;
		goto vreg_deinit;
	}

	/* Set SW_RST bit in POWER register (Offset 0x0) */
	writel_relaxed(CORE_SW_RST, msm_host->core_mem + CORE_POWER);
	/* Set HC_MODE_EN bit in HC_MODE register */
	writel_relaxed(HC_MODE_EN, (msm_host->core_mem + CORE_HC_MODE));

	/*
	 * Following are the deviations from SDHC spec v3.0 -
	 * 1. Card detection is handled using separate GPIO.
	 * 2. Bus power control is handled by interacting with PMIC.
	 */
	host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
	host->quirks |= SDHCI_QUIRK_SINGLE_POWER_WRITE;
	host->quirks2 |= SDHCI_QUIRK2_IGNORE_CMDCRC_FOR_TUNING;

	host_version = readl_relaxed((host->ioaddr + SDHCI_HOST_VERSION));
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

	/* Setup PWRCTL irq */
	pwr_irq = platform_get_irq_byname(pdev, "pwr_irq");
	if (pwr_irq < 0) {
		dev_err(&pdev->dev, "Failed to get pwr_irq by name (%d)\n",
				pwr_irq);
		goto vreg_deinit;
	}
	ret = devm_request_threaded_irq(&pdev->dev, pwr_irq, NULL,
					sdhci_msm_pwr_irq, IRQF_ONESHOT,
					dev_name(&pdev->dev), host);
	if (ret) {
		dev_err(&pdev->dev, "Request threaded irq(%d) failed (%d)\n",
				pwr_irq, ret);
		goto vreg_deinit;
	}

	/* Enable pwr irq interrupts */
	writel_relaxed(INT_MASK, (msm_host->core_mem + CORE_PWRCTL_MASK));

	/* Set host capabilities */
	msm_host->mmc->caps |= msm_host->pdata->mmc_bus_width;
	msm_host->mmc->caps |= msm_host->pdata->caps;

	vdd_max_current = sdhci_msm_get_vreg_vdd_max_current(msm_host);
	if (vdd_max_current >= 800)
		msm_host->mmc->caps |= MMC_CAP_MAX_CURRENT_800;
	else if (vdd_max_current >= 600)
		msm_host->mmc->caps |= MMC_CAP_MAX_CURRENT_600;
	else if (vdd_max_current >= 400)
		msm_host->mmc->caps |= MMC_CAP_MAX_CURRENT_400;
	else
		msm_host->mmc->caps |= MMC_CAP_MAX_CURRENT_200;

	if (vdd_max_current > 150)
		msm_host->mmc->caps |= MMC_CAP_SET_XPC_180 |
					MMC_CAP_SET_XPC_300|
					MMC_CAP_SET_XPC_330;

	msm_host->mmc->caps |= MMC_CAP_HW_RESET;
	msm_host->mmc->caps2 |= msm_host->pdata->caps2;
	msm_host->mmc->caps2 |= MMC_CAP2_PACKED_WR;
	msm_host->mmc->caps2 |= MMC_CAP2_PACKED_WR_CONTROL;
	msm_host->mmc->caps2 |= (MMC_CAP2_BOOTPART_NOACC |
				MMC_CAP2_DETECT_ON_ERR);
	msm_host->mmc->caps2 |= MMC_CAP2_SANITIZE;
	msm_host->mmc->caps2 |= MMC_CAP2_CACHE_CTRL;
	msm_host->mmc->caps2 |= MMC_CAP2_INIT_BKOPS;
	msm_host->mmc->caps2 |= MMC_CAP2_POWEROFF_NOTIFY;

	if (msm_host->pdata->nonremovable)
		msm_host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	ret = sdhci_add_host(host);
	if (ret) {
		dev_err(&pdev->dev, "Add host failed (%d)\n", ret);
		goto vreg_deinit;
	}

	 /* Set core clk rate, optionally override from dts */
	if (msm_host->pdata->max_clk)
		host->max_clk = msm_host->pdata->max_clk;
	ret = clk_set_rate(msm_host->clk, host->max_clk);
	if (ret) {
		dev_err(&pdev->dev, "MClk rate set failed (%d)\n", ret);
		goto remove_host;
	}

	/* Successful initialization */
	goto out;

remove_host:
	dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);
	sdhci_remove_host(host, dead);
vreg_deinit:
	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);
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
out:
	pr_debug("%s: Exit %s\n", dev_name(&pdev->dev), __func__);
	return ret;
}

static int __devexit sdhci_msm_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pltfm_data *pdata = msm_host->pdata;
	int dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) ==
			0xffffffff);

	pr_debug("%s: %s\n", dev_name(&pdev->dev), __func__);
	sdhci_remove_host(host, dead);
	sdhci_pltfm_free(pdev);
	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
	if (pdata->pin_data)
		sdhci_msm_setup_pins(pdata, false);
	return 0;
}

static const struct of_device_id sdhci_msm_dt_match[] = {
	{.compatible = "qcom,sdhci-msm"},
};
MODULE_DEVICE_TABLE(of, sdhci_msm_dt_match);

static struct platform_driver sdhci_msm_driver = {
	.probe		= sdhci_msm_probe,
	.remove		= __devexit_p(sdhci_msm_remove),
	.driver		= {
		.name	= "sdhci_msm",
		.owner	= THIS_MODULE,
		.of_match_table = sdhci_msm_dt_match,
	},
};

module_platform_driver(sdhci_msm_driver);

MODULE_DESCRIPTION("Qualcomm Secure Digital Host Controller Interface driver");
MODULE_LICENSE("GPL v2");

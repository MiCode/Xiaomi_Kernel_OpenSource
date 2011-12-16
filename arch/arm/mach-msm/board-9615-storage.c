/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/mach/mmc.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include "devices.h"

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT) \
		|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT))

#define GPIO_SDC1_HW_DET	80
#define GPIO_SDC2_DAT1_WAKEUP	26

/* MDM9x15 has 2 SDCC controllers */
enum sdcc_controllers {
	SDCC1,
	SDCC2,
	MAX_SDCC_CONTROLLER
};

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
/* All SDCC controllers requires VDD/VCC voltage */
static struct msm_mmc_reg_data mmc_vdd_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : External card slot connected */
	[SDCC1] = {
		.name = "sdc_vdd",
		/*
		 * This is a gpio-regulator and does not support
		 * regulator_set_voltage and regulator_set_optimum_mode
		 */
		.high_vol_level = 2950000,
		.low_vol_level = 2950000,
		.hpm_uA = 600000, /* 600mA */
	}
};

/* All SDCC controllers may require voting for VDD PAD voltage */
static struct msm_mmc_reg_data mmc_vddp_reg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : External card slot connected */
	[SDCC1] = {
		.name = "sdc_vddp",
		.high_vol_level = 2950000,
		.low_vol_level = 1850000,
		.always_on = true,
		.lpm_sup = true,
		/* Max. Active current required is 16 mA */
		.hpm_uA = 16000,
		/*
		 * Sleep current required is ~300 uA. But min. vote can be
		 * in terms of mA (min. 1 mA). So let's vote for 2 mA
		 * during sleep.
		 */
		.lpm_uA = 2000,
	}
};

static struct msm_mmc_slot_reg_data mmc_slot_vreg_data[MAX_SDCC_CONTROLLER] = {
	/* SDCC1 : External card slot connected */
	[SDCC1] = {
		.vdd_data = &mmc_vdd_reg_data[SDCC1],
		.vddp_data = &mmc_vddp_reg_data[SDCC1],
	}
};

/* SDC1 pad data */
static struct msm_mmc_pad_drv sdc1_pad_drv_on_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_16MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_10MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_10MA}
};

static struct msm_mmc_pad_drv sdc1_pad_drv_off_cfg[] = {
	{TLMM_HDRV_SDC1_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC1_DATA, GPIO_CFG_2MA}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_on_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_mmc_pad_pull sdc1_pad_pull_off_cfg[] = {
	{TLMM_PULL_SDC1_CLK, GPIO_CFG_NO_PULL},
	{TLMM_PULL_SDC1_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC1_DATA, GPIO_CFG_PULL_DOWN}
};

static struct msm_mmc_pad_pull_data mmc_pad_pull_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_pull_on_cfg,
		.off = sdc1_pad_pull_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_pull_on_cfg)
	},
};

static struct msm_mmc_pad_drv_data mmc_pad_drv_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.on = sdc1_pad_drv_on_cfg,
		.off = sdc1_pad_drv_off_cfg,
		.size = ARRAY_SIZE(sdc1_pad_drv_on_cfg)
	},
};

static struct msm_mmc_pad_data mmc_pad_data[MAX_SDCC_CONTROLLER] = {
	[SDCC1] = {
		.pull = &mmc_pad_pull_data[SDCC1],
		.drv = &mmc_pad_drv_data[SDCC1]
	},
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT

static struct msm_mmc_gpio sdc2_gpio_cfg[] = {
	{25, "sdc2_dat_0"},
	{26, "sdc2_dat_1"},
	{27, "sdc2_dat_2"},
	{28, "sdc2_dat_3"},
	{29, "sdc2_cmd"},
	{30, "sdc2_clk"},
};

static struct msm_mmc_gpio_data mmc_gpio_data[MAX_SDCC_CONTROLLER] = {
	[SDCC2] = {
		.gpio = sdc2_gpio_cfg,
		.size = ARRAY_SIZE(sdc2_gpio_cfg),
	},
};
#endif

static struct msm_mmc_pin_data mmc_slot_pin_data[MAX_SDCC_CONTROLLER] = {
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	[SDCC1] = {
		.is_gpio = 0,
		.pad_data = &mmc_pad_data[SDCC1],
	},
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	[SDCC2] = {
		.is_gpio = 1,
		.gpio_data = &mmc_gpio_data[SDCC2],
	},
#endif
};

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static unsigned int sdc1_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc1_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc1_sup_clk_rates),
	.pclk_src_dfab	= true,
	.vreg_data	= &mmc_slot_vreg_data[SDCC1],
	.pin_data	= &mmc_slot_pin_data[SDCC1],
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
	.status_gpio	= GPIO_SDC1_HW_DET,
	.status_irq	= MSM_GPIO_TO_INT(GPIO_SDC1_HW_DET),
	.irq_flags	= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
#endif
	.xpc_cap	= 1,
	.uhs_caps	= (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			   MMC_CAP_MAX_CURRENT_400)
};
static struct mmc_platform_data *msm9615_sdc1_pdata = &sdc1_data;
#else
static struct mmc_platform_data *msm9615_sdc1_pdata;
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static unsigned int sdc2_sup_clk_rates[] = {
	400000, 24000000, 48000000
};

static struct mmc_platform_data sdc2_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.sup_clk_table	= sdc2_sup_clk_rates,
	.sup_clk_cnt	= ARRAY_SIZE(sdc2_sup_clk_rates),
	.pclk_src_dfab	= 1,
	.pin_data	= &mmc_slot_pin_data[SDCC2],
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
	.sdiowakeup_irq = MSM_GPIO_TO_INT(GPIO_SDC2_DAT1_WAKEUP),
#endif
};
static struct mmc_platform_data *msm9615_sdc2_pdata = &sdc2_data;
#else
static struct mmc_platform_data *msm9615_sdc2_pdata;
#endif

void __init msm9615_init_mmc(void)
{
	if (msm9615_sdc1_pdata) {
		/* SDC1: External card slot for SD/MMC cards */
		msm_add_sdcc(1, msm9615_sdc1_pdata);
	}

	if (msm9615_sdc2_pdata) {
		/* SDC2: External card slot used for WLAN */
		msm_add_sdcc(2, msm9615_sdc2_pdata);
	}
}
#else
void __init msm9615_init_mmc(void)
{
}
#endif

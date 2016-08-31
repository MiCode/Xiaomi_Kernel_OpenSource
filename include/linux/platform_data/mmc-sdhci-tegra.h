/*
 * Copyright (C) 2009 Palm, Inc.
 * Author: Yvonne Yip <y@palm.com>
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __PLATFORM_DATA_TEGRA_SDHCI_H
#define __PLATFORM_DATA_TEGRA_SDHCI_H

#include <linux/mmc/host.h>
#include <asm/mach/mmc.h>

/*
 * MMC_OCR_1V8_MASK will be used in board sdhci file
 * Example for cardhu it will be used in board-cardhu-sdhci.c
 * for built_in = 0 devices enabling ocr_mask to MMC_OCR_1V8_MASK
 * sets the voltage to 1.8V
 */
#define MMC_OCR_1V8_MASK    0x00000008
#define MMC_OCR_2V8_MASK    0x00010000
#define MMC_OCR_3V2_MASK    0x00100000
#define MMC_OCR_3V3_MASK    0x00200000

/* uhs mask can be used to mask any of the UHS modes support */
#define MMC_UHS_MASK_SDR12	0x1
#define MMC_UHS_MASK_SDR25	0x2
#define MMC_UHS_MASK_SDR50	0x4
#define MMC_UHS_MASK_DDR50	0x8
#define MMC_UHS_MASK_SDR104	0x10
#define MMC_MASK_HS200		0x20

struct tegra_sdhci_platform_data {
	int cd_gpio;
	int wp_gpio;
	int power_gpio;
	int is_8bit;
	int pm_flags;
	int pm_caps;
	int nominal_vcore_mv;
	int min_vcore_override_mv;
	int boot_vcore_mv;
	unsigned int max_clk_limit;
	unsigned int ddr_clk_limit;
	unsigned int tap_delay;
	unsigned int trim_delay;
	unsigned int ddr_trim_delay;
	unsigned int uhs_mask;
	struct mmc_platform_data mmc_data;
	bool power_off_rail;
	bool en_freq_scaling;
	bool cd_wakeup_incapable;
	bool en_nominal_vcore_tuning;
	unsigned int calib_3v3_offsets;	/* Format to be filled: 0xXXXXPDPU */
	unsigned int calib_1v8_offsets;	/* Format to be filled: 0xXXXXPDPU */
	bool disable_clock_gate; /* no clock gate when true */
	u32 cpu_speedo;
};

#endif

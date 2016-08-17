/*
 * Copyright (C) 2010-2012, NVIDIA Corporation. All rights reserved.
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
 * include/linux/platform_data/tegra_nor.h
 *
 * Author:
 *	Raghavendra V K <rvk@nvidia.com>
 *
 */

#ifndef __MACH_TEGRA_NOR_PDATA_H
#define __MACH_TEGRA_NOR_PDATA_H

#include <asm/mach/flash.h>

typedef enum {
	NorMuxMode_ADNonMux,
	NorMuxMode_ADMux,
}NorMuxMode;

typedef enum {
	NorPageLength_Unsupported,
	NorPageLength_4Word,
	NorPageLength_8Word,
}NorPageLength;

typedef enum {
	NorBurstLength_CntBurst,
	NorBurstLength_8Word,
	NorBurstLength_16Word,
	NorBurstLength_32Word,
}NorBurstLength;

typedef enum {
	NorReadMode_Async,
	NorReadMode_Page,
	NorReadMode_Burst,
}NorReadMode;

typedef enum {
	NorReadyActive_WithData,
	NorReadyActive_BeforeData,
}NorReadyActive;

struct tegra_nor_chip_parms {
	struct {
		uint32_t timing0;
		uint32_t timing1;
	} timing_default, timing_read;
	NorMuxMode MuxMode;
	NorReadMode ReadMode;
	NorPageLength PageLength;
	NorBurstLength BurstLength;
	NorReadyActive ReadyActive;
};

struct tegra_nor_platform_data {
	struct tegra_nor_chip_parms chip_parms;
	struct flash_platform_data flash;
};

#endif /* __MACH_TEGRA_NOR_PDATA_H */

/*
 * Copyright (C) 2010-2013, NVIDIA Corporation. All rights reserved.
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
} NorMuxMode;

typedef enum {
	NorPageLength_Unsupported,
	NorPageLength_4Word,
	NorPageLength_8Word,
	NorPageLength_16Word,
} NorPageLength;

typedef enum {
	NorBurstLength_CntBurst,
	NorBurstLength_8Word,
	NorBurstLength_16Word,
	NorBurstLength_32Word,
} NorBurstLength;

typedef enum {
	NorReadMode_Async,
	NorReadMode_Page,
	NorReadMode_Burst,
} NorReadMode;

typedef enum {
	NorReadyActive_WithData,
	NorReadyActive_BeforeData,
} NorReadyActive;

/* Signal values*/
enum SIGNAL {
	LOW,
	HIGH
};

/* Mapping of CS and signal*/
struct gpio_state {
	char *label;
	int gpio_num;		/* GPIO number which is CS*/
	enum SIGNAL value;	/* Signal value of CS pin to set */
};

enum CS {
	CS_0,
	CS_1,
	CS_2,
	CS_3,
	CS_4,
	CS_5,
	CS_6,
	CS_7
};

/* All params required for map_info passed*/
struct cs_info {
	enum CS cs;
	struct gpio_state gpio_cs;
	int num_cs_gpio;
	void __iomem *virt;	/* Virtual address of chip select window */
	resource_size_t phys;	/* Physical address of chip select window */
	unsigned int size;	/* size of chip select window */
};
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
	unsigned int BusWidth;
	struct cs_info csinfo;
};
struct flash_info {
	struct cs_info *cs;
	unsigned int num_chips;
};

/* Container having cs_info and number of such mappings*/
struct gpio_addr {
	int gpio_num;           /* GPIO number of address line */
	int line_num;           /* address line number starting from 0 */
};

struct gpio_addr_info {
	struct gpio_addr *addr;
	unsigned int num_gpios;
};

struct tegra_nor_platform_data {
	struct tegra_nor_chip_parms chip_parms;
	struct flash_platform_data flash;
	struct flash_info info;
	struct gpio_addr_info addr;
};

#endif /* __MACH_TEGRA_NOR_PDATA_H */

/*
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * Author:
 * Bharath H S <bhs@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * include/linux/tegra_snor.h
 *
 * MTD mapping driver for the internal SNOR controller in Tegra SoCs
 *
 */

#ifndef TEGRA_SNOR
#define TGERA_SNOR

#include<linux/mtd/map.h>


#define __BITMASK0(len)                 (BIT(len) - 1)
#define REG_FIELD(val, start, len)      (((val) & __BITMASK0(len)) << (start))
#define REG_GET_FIELD(val, start, len)  (((val) >> (start)) & __BITMASK0(len))

/* tegra gmi registers... */
#define TEGRA_SNOR_CONFIG_REG                   0x00
#define TEGRA_SNOR_NOR_ADDR_PTR_REG             0x08
#define TEGRA_SNOR_AHB_ADDR_PTR_REG             0x0C
#define TEGRA_SNOR_TIMING0_REG                  0x10
#define TEGRA_SNOR_TIMING1_REG                  0x14
#define TEGRA_SNOR_DMA_CFG_REG                  0x20

/* config register */
#define TEGRA_SNOR_CONFIG_GO                    BIT(31)
#define TEGRA_SNOR_CONFIG_WORDWIDE              BIT(30)
#define TEGRA_SNOR_CONFIG_DEVICE_TYPE           BIT(29)
#define TEGRA_SNOR_CONFIG_MUX_MODE              BIT(28)
#define TEGRA_SNOR_CONFIG_BURST_LEN(val)        REG_FIELD((val), 26, 2)
#define TEGRA_SNOR_CONFIG_RDY_ACTIVE            BIT(24)
#define TEGRA_SNOR_CONFIG_RDY_POLARITY          BIT(23)
#define TEGRA_SNOR_CONFIG_ADV_POLARITY          BIT(22)
#define TEGRA_SNOR_CONFIG_OE_WE_POLARITY        BIT(21)
#define TEGRA_SNOR_CONFIG_CS_POLARITY           BIT(20)
#define TEGRA_SNOR_CONFIG_NOR_DPD               BIT(19)
#define TEGRA_SNOR_CONFIG_WP                    BIT(15)
#define TEGRA_SNOR_CONFIG_PAGE_SZ(val)          REG_FIELD((val), 8, 2)
#define TEGRA_SNOR_CONFIG_MST_ENB               BIT(7)
#define TEGRA_SNOR_CONFIG_SNOR_CS(val)          REG_FIELD((val), 4, 3)
#define TEGRA_SNOR_CONFIG_CE_LAST               REG_FIELD(3)
#define TEGRA_SNOR_CONFIG_CE_FIRST              REG_FIELD(2)
#define TEGRA_SNOR_CONFIG_DEVICE_MODE(val)      REG_FIELD((val), 0, 2)

/* dma config register */
#define TEGRA_SNOR_DMA_CFG_GO                   BIT(31)
#define TEGRA_SNOR_DMA_CFG_BSY                  BIT(30)
#define TEGRA_SNOR_DMA_CFG_DIR                  BIT(29)
#define TEGRA_SNOR_DMA_CFG_INT_ENB              BIT(28)
#define TEGRA_SNOR_DMA_CFG_INT_STA              BIT(27)
#define TEGRA_SNOR_DMA_CFG_BRST_SZ(val)         REG_FIELD((val), 24, 3)
#define TEGRA_SNOR_DMA_CFG_WRD_CNT(val)         REG_FIELD((val), 2, 14)

/* timing 0 register */
#define TEGRA_SNOR_TIMING0_PG_RDY(val)          REG_FIELD((val), 28, 4)
#define TEGRA_SNOR_TIMING0_PG_SEQ(val)          REG_FIELD((val), 20, 4)
#define TEGRA_SNOR_TIMING0_MUX(val)             REG_FIELD((val), 12, 4)
#define TEGRA_SNOR_TIMING0_HOLD(val)            REG_FIELD((val), 8, 4)
#define TEGRA_SNOR_TIMING0_ADV(val)             REG_FIELD((val), 4, 4)
#define TEGRA_SNOR_TIMING0_CE(val)              REG_FIELD((val), 0, 4)

/* timing 1 register */
#define TEGRA_SNOR_TIMING1_WE(val)              REG_FIELD((val), 16, 8)
#define TEGRA_SNOR_TIMING1_OE(val)              REG_FIELD((val), 8, 8)
#define TEGRA_SNOR_TIMING1_WAIT(val)            REG_FIELD((val), 0, 8)

/* SNOR DMA supports 2^14 AHB (32-bit words)
 * Maximum data in one transfer = 2^16 bytes
 */
#define TEGRA_SNOR_DMA_LIMIT           0x10000
#define TEGRA_SNOR_DMA_LIMIT_WORDS     (TEGRA_SNOR_DMA_LIMIT >> 2)

/* Even if BW is 1 MB/s, maximum time to
 * transfer SNOR_DMA_LIMIT bytes is 66 ms
 */
#define TEGRA_SNOR_DMA_TIMEOUT_MS       67

#define SNOR_CONFIG_MASK 0xFFFFFF8F
#define SNOR_WINDOW_SIZE 0x07FFFFFF

void tegra_nor_copy_from(struct map_info *map, void *to,
		unsigned long from, ssize_t len);
void tegra_nor_write(struct map_info *map,
				map_word datum, unsigned long ofs);
map_word tegra_nor_read(struct map_info *map,
				unsigned long ofs);
struct map_info *get_map_info(unsigned int bank_index);
int get_maps_no(void);
unsigned long long getflashsize(void);

#endif

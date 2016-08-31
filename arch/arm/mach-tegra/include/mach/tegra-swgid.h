/*
 * arch/arm/mach-tegra/include/mach/tegra-swgid.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

/*
 * This header provides constants for binding nvidia,swgroup ID
 */

#ifndef DT_BINDINGS_IOMMU_TEGRA_SWGID_H
#define DT_BINDINGS_IOMMU_TEGRA_SWGID_H

#define SWGID_AFI	0	/* 0x238 */
#define SWGID_AVPC	1	/* 0x23c */
#define SWGID_DC	2	/* 0x240 */
#define SWGID_DCB	3	/* 0x244 */
#define SWGID_EPP	4	/* 0x248 */
#define SWGID_G2	5	/* 0x24c */
#define SWGID_HC	6	/* 0x250 */
#define SWGID_HDA	7	/* 0x254 */
#define SWGID_ISP	8	/* 0x258 */
#define SWGID_ISP2	SWGID_ISP
#define SWGID_DC14	9	/* 0x490 *//* 150: Exceptionally non-linear */
#define SWGID_DC12	10	/* 0xa88 *//* 532: Exceptionally non-linear */
#define SWGID_MPE	11	/* 0x264 */
#define SWGID_MSENC	SWGID_MPE
#define SWGID_NV	12	/* 0x268 */
#define SWGID_NV2	13	/* 0x26c */
#define SWGID_PPCS	14	/* 0x270 */
#define SWGID_SATA2	15	/* 0x274 */
#define SWGID_SATA	16	/* 0x278 */
#define SWGID_VDE	17	/* 0x27c */
#define SWGID_VI	18	/* 0x280 */
#define SWGID_VIC	19	/* 0x284 */
#define SWGID_XUSB_HOST	20	/* 0x288 */
#define SWGID_XUSB_DEV	21	/* 0x28c */
#define SWGID_A9AVP	22	/* 0x290 */
#define SWGID_TSEC	23	/* 0x294 */
#define SWGID_PPCS1	24	/* 0x298 */
#define SWGID_SDMMC1A	25	/* 0xa94 *//* Linear shift starts here */
#define SWGID_SDMMC2A	26	/* 0xa98 */
#define SWGID_SDMMC3A	27	/* 0xa9c */
#define SWGID_SDMMC4A	28	/* 0xaa0 */
#define SWGID_ISP2B	29	/* 0xaa4 */
#define SWGID_GPU	30	/* 0xaa8 */
#define SWGID_GPUB	31	/* 0xaac */
#define SWGID_PPCS2	32	/* 0xab0 */

#define SWGID(x)	(1ULL << SWGID_##x)

#endif /* DT_BINDINGS_IOMMU_TEGRA_SWGID_H */

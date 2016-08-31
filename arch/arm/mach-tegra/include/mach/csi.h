/*
 * arch/arm/mach-tegra/include/mach/csi.h
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation.
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

#ifndef __MACH_TEGRA_CSI_H
#define __MACH_TEGRA_CSI_H

#define CSI_CILA_MIPI_CAL_CONFIG_0 0x22a
#define  MIPI_CAL_TERMOSA(x)		(((x) & 0x1f) << 0)

#define CSI_CILB_MIPI_CAL_CONFIG_0 0x22b
#define  MIPI_CAL_TERMOSB(x)		(((x) & 0x1f) << 0)

#define CSI_CIL_PAD_CONFIG 0x229
#define  PAD_CIL_PDVREG(x)		(((x) & 0x01) << 1)

#define CSI_DSI_MIPI_CAL_CONFIG	0x234
#define  MIPI_CAL_HSPDOSD(x)		(((x) & 0x1f) << 16)
#define  MIPI_CAL_HSPUOSD(x)		(((x) & 0x1f) << 8)

#define CSI_MIPIBIAS_PAD_CONFIG	0x235
#define  PAD_DRIV_DN_REF(x)		(((x) & 0x7) << 16)
#define  PAD_DRIV_UP_REF(x)		(((x) & 0x7) << 8)

int tegra_vi_csi_readl(u32 offset, u32 *val);
int tegra_vi_csi_writel(u32 value, u32 offset);

#endif

/*
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

#ifndef _TEGRA_USB_PAD_CTRL_INTERFACE_H_
#define _TEGRA_USB_PAD_CTRL_INTERFACE_H_

#define UTMIPLL_HW_PWRDN_CFG0			0x52c
#define   UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE  (1<<1)
#define   UTMIPLL_HW_PWRDN_CFG0_IDDQ_SWCTL     (1<<0)

#define UTMIP_BIAS_CFG0		0x80c
#define   UTMIP_OTGPD			(1 << 11)
#define   UTMIP_BIASPD			(1 << 10)
#define   UTMIP_HSSQUELCH_LEVEL(x)	(((x) & 0x3) << 0)
#define   UTMIP_HSDISCON_LEVEL(x)	(((x) & 0x3) << 2)
#define   UTMIP_HSDISCON_LEVEL_MSB	(1 << 24)

int utmi_phy_pad_disable(void);
int utmi_phy_pad_enable(void);
int utmi_phy_iddq_override(bool set);
#endif

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>

const struct mpm_pin mpm_qcs405_gic_chip_data[] = {
	{2, 184},
	{35, 318}, /* dmse_hv, usb20 -> hs_phy_irq */
	{36, 318}, /* dpse_hv, usb20 -> hs_phy_irq */
	{38, 319}, /* dmse_hv, usb30 -> hs_phy_irq */
	{39, 319}, /* dpse_hv, usb30 -> hs_phy_irq */
	{62, 190}, /* mpm_wake,spmi_m */
	{-1},
};

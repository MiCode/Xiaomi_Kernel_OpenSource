/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "mpm.h"

const struct mpm_pin mpm_msm8953_gic_chip_data[] = {
	{2, 216}, /* tsens_upper_lower_int */
	{37, 252}, /* qmp_usb3_lfps_rxterm_irq -> ss_phy_irq */
	{49, 168}, /* qusb2phy_dpse_hv -> hs_phy_irq*/
	{53, 104}, /* mdss_irq */
	{58, 168}, /* qusb2phy_dmse_hv -> hs_phy_irq*/
	{88, 222}, /* ee0_krait_hlos_spmi_periph_irq */
	{-1},
};

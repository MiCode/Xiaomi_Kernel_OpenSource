// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>
const struct mpm_pin mpm_sdxnightjar_gic_chip_data[] = {
	{2, 184}, /* tsens_upper_lower_int */
	{88, 190},   /* ee0_krait_hlos_spmi_periph_irq */
	{54, 203},   /* qmp_usb3_lfps_rxterm_irq */
	{49, 202},   /* qusb2_phy_dpse_hv qusb2phy_intr */
	{-1},
};

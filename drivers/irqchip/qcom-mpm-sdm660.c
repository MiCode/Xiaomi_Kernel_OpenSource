// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>

const struct mpm_pin mpm_sdm660_gic_chip_data[] = {
	{2, 216}, /* tsens1_tsens_upper_lower_int */
	{52, 275}, /* qmp_usb3_lfps_rxterm_irq_cx */
	{61, 209}, /* lpi_dir_conn_irq_apps[1] */
	{79, 379}, /* qusb2phy_intr for Dm */
	{80, 380}, /* qusb2phy_intr for Dm for secondary PHY */
	{81, 379}, /* qusb2phy_intr for Dp */
	{82, 380}, /* qusb2phy_intr for Dp for secondary PHY */
	{87, 358}, /* ee0_apps_hlos_spmi_periph_irq */
	{91, 519}, /* lpass_pmu_tmr_timeout_irq_cx */
	{-1},
};

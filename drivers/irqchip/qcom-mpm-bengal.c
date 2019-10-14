// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>
const struct mpm_pin mpm_bengal_gic_chip_data[] = {
	{2, 222},
	{12, 454}, /* b3_lfps_rxterm_irq */
	{86, 215}, /* mpm_wake,spmi_m */
	{90, 292}, /* eud_p0_dpse_int_mx */
	{91, 292}, /* eud_p0_dmse_int_mx */
	{-1},
};

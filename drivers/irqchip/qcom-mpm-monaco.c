// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>
const struct mpm_pin mpm_monaco_gic_chip_data[] = {
	{5, 296}, /* lpass_irq_out_sdc */
	{8, 260}, /* eud_p0_dpse_int_mx */
	{86, 183}, /* mpm_wake,spmi_m */
	{89, 422}, /* tsens0_tsens_0C_int */
	{91, 260}, /* eud_p0_dmse_int_mx */
	{-1},
};

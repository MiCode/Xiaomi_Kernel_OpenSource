// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>
const struct mpm_pin mpm_khaje_gic_chip_data[] = {
	{2, 222},
	{12, 454}, /* b3_lfps_rxterm_irq */
	{86, 215}, /* mpm_wake,spmi_m */
	{91, 216}, /* eud_p0_dpse_int_mx */
	{90, 220}, /* eud_p0_dmse_int_mx */
	{5, 328}, /* lpass_irq_out_sdc */
	{24, 111}, /* bi_px_lpi_1_aoss_mx */
	{-1},
};

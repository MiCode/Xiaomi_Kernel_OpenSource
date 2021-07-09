// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>
const struct mpm_pin mpm_blair_gic_chip_data[] = {
	{5, 296}, /* lpass_irq_out_sdc */
	{12, 422}, /* qmp_usb3_lfps_rxterm_irq_cx */
	{86, 183}, /* mpm_wake,spmi_m */
	{89, 314}, /* tsens0_tsens_0C_int */
	{90, 315}, /* tsens1_tsens_0C_int */
	{93, 164}, /* eud_p0_dmse_int_mx */
	{94, 165}, /* eud_p0_dpse_int_mx */
	{-1},
};

// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <soc/qcom/mpm.h>
const struct mpm_pin mpm_holi_gic_chip_data[] = {
	{2, 222}, /* tsens0_tsens_critical_int */
	{5, 328}, /* lpass_irq_out_sdc */
	{12, 454}, /* qmp_usb3_lfps_rxterm_irq_cx */
	{86, 215}, /* mpm_wake,spmi_m */
	{93, 292}, /* eud_p0_dpse_int_mx */
	{94, 292}, /* eud_p0_dmse_int_mx */
	{-1},
};

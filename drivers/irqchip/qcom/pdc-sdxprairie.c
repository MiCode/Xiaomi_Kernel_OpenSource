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
 */

#include <linux/irqchip.h>
#include "pdc.h"

static struct pdc_pin sdxprairie_data[] = {
	{0, 179}, /* rpmh_wake */
	{1, 180}, /* ee0_apps_hlos_spmi_periph_irq */
	{2, 181}, /* ee1_apps_trustzone_spmi_periph_irq */
	{3, 182}, /* secure_wdog_expired */
	{4, 183}, /* secure_wdog_bark_irq */
	{5, 184}, /* aop_wdog_expired_irq */
	{6, 185}, /* spmi_vgis_irq_to_APPS */
	{7, 186}, /* not-connected */
	{8, 187}, /* aoss_pmic_arb_mpu_xpu_summary_irq */
	{9, 188}, /* rpmh_wake */
	{10, 189}, /* eud_p1_dpse_int_mx */
	{11, 190}, /* eud_p1_dmse_int_mx */
	{12, 191}, /* pdc_apps_epcb_timeout_summary_irq */
	{13, 192}, /* spmi_protocol_irq */
	{14, 193}, /* tsense0_tsense_max_min_int */
	{15, 194}, /* not-connected */
	{16, 195}, /* tsense0_tsense_upper_lower_int */
	{17, 196}, /* not-connected */
	{18, 197}, /* tsense0_tsense_critical_int */
	{19, 198}, /* eud_int_mx[4] */
	{20, 199}, /* apps_pdc.gp_irq_mux[0] */
	{21, 200}, /* apps_pdc.gp_irq_mux[1] */
	{22, 201}, /* apps_pdc.gp_irq_mux[2] */
	{23, 202}, /* apps_pdc.gp_irq_mux[3] */
	{24, 203}, /* apps_pdc.gp_irq_mux[4] */
	{25, 204}, /* apps_pdc.gp_irq_mux[5] */
	{26, 205}, /* apps_pdc.gp_irq_mux[6] */
	{27, 206}, /* apps_pdc.gp_irq_mux[7] */
	{28, 207}, /* apps_pdc.gp_irq_mux[8] */
	{29, 208}, /* apps_pdc.gp_irq_mux[9] */
	{30, 209}, /* apps_pdc.gp_irq_mux[10] */
	{31, 210}, /* apps_pdc.gp_irq_mux[11] */
	{32, 211}, /* apps_pdc.gp_irq_mux[12] */
	{33, 212}, /* apps_pdc.gp_irq_mux[13] */
	{34, 213}, /* apps_pdc.gp_irq_mux[14] */
	{35, 214}, /* apps_pdc.gp_irq_mux[15] */
	{36, 215}, /* apps_pdc.gp_irq_mux[16] */
	{37, 216}, /* apps_pdc.gp_irq_mux[17] */
	{38, 217}, /* apps_pdc.gp_irq_mux[18] */
	{39, 218}, /* apps_pdc.gp_irq_mux[19] */
	{40, 219}, /* apps_pdc.gp_irq_mux[20] */
	{41, 220}, /* apps_pdc.gp_irq_mux[21] */
	{42, 221}, /* apps_pdc.gp_irq_mux[22] */
	{43, 222}, /* apps_pdc.gp_irq_mux[23] */
	{44, 223}, /* apps_pdc.gp_irq_mux[24] */
	{45, 224}, /* apps_pdc.gp_irq_mux[25] */
	{46, 225}, /* apps_pdc.gp_irq_mux[26] */
	{47, 226}, /* apps_pdc.gp_irq_mux[27] */
	{48, 227}, /* apps_pdc.gp_irq_mux[28] */
	{49, 228}, /* apps_pdc.gp_irq_mux[29] */
	{50, 229}, /* apps_pdc.gp_irq_mux[30] */
	{51, 230}, /* apps_pdc.gp_irq_mux[31] */
	{-1},
};

static int __init qcom_pdc_gic_init(struct device_node *node,
				struct device_node *parent)
{
	return qcom_pdc_init(node, parent, sdxprairie_data);
}

IRQCHIP_DECLARE(pdc_sdxprairie, "qcom,pdc-sdxprairie", qcom_pdc_gic_init);

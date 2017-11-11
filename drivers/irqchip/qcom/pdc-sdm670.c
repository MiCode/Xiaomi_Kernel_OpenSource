/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/irqchip.h>
#include "pdc.h"

static struct pdc_pin sdm670_data[] = {
	{0, 512}, /* rpmh_wake */
	{1, 513}, /* ee0_apps_hlos_spmi_periph_irq */
	{2, 514}, /* ee1_apps_trustzone_spmi_periph_irq */
	{3, 515}, /* secure_wdog_expired */
	{4, 516}, /* secure_wdog_bark_irq */
	{5, 517}, /* aop_wdog_expired_irq */
	{6, 518}, /* qmp_usb3_lfps_rxterm_irq */
	{7, 519}, /* not-connected */
	{8, 520}, /* eud_p0_dmse_int_mx */
	{9, 521}, /* eud_p0_dpse_int_mx */
	{10, 522}, /* not-connected */
	{11, 523}, /* not-connected */
	{12, 524}, /* eud_int_mx[1] */
	{13, 525}, /* ssc_xpu_irq_summary */
	{14, 526}, /* wd_bite_apps */
	{15, 527}, /* ssc_vmidmt_irq_summary */
	{16, 528}, /* q6ss_irq_out_apps_ipc[4] */
	{17, 529}, /* not-connected */
	{18, 530}, /* aoss_pmic_arb_mpu_xpu_summary_irq */
	{19, 531}, /* apps_pdc_irq_in_19 */
	{20, 532}, /* apps_pdc_irq_in_20 */
	{21, 533}, /* apps_pdc_irq_in_21 */
	{22, 534}, /* pdc_apps_epcb_timeout_summary_irq */
	{23, 535}, /* spmi_protocol_irq */
	{24, 536}, /* tsense0_tsense_max_min_int */
	{25, 537}, /* tsense1_tsense_max_min_int */
	{26, 538}, /* tsense0_upper_lower_intr */
	{27, 539}, /* tsense1_upper_lower_intr */
	{28, 540}, /* tsense0_critical_intr */
	{29, 541}, /* tsense1_critical_intr */
	{30, 542}, /* core_bi_px_gpio_1 */
	{31, 543}, /* core_bi_px_gpio_3 */
	{32, 544}, /* core_bi_px_gpio_5 */
	{33, 545}, /* core_bi_px_gpio_10 */
	{34, 546}, /* core_bi_px_gpio_11 */
	{35, 547}, /* core_bi_px_gpio_20 */
	{36, 548}, /* core_bi_px_gpio_22 */
	{37, 549}, /* core_bi_px_gpio_24 */
	{38, 550}, /* core_bi_px_gpio_26 */
	{39, 551}, /* core_bi_px_gpio_30 */
	{41, 553}, /* core_bi_px_gpio_32 */
	{42, 554}, /* core_bi_px_gpio_34 */
	{43, 555}, /* core_bi_px_gpio_36 */
	{44, 556}, /* core_bi_px_gpio_37 */
	{45, 557}, /* core_bi_px_gpio_38 */
	{46, 558}, /* core_bi_px_gpio_39 */
	{47, 559}, /* core_bi_px_gpio_40 */
	{49, 561}, /* core_bi_px_gpio_43 */
	{50, 562}, /* core_bi_px_gpio_44 */
	{51, 563}, /* core_bi_px_gpio_46 */
	{52, 564}, /* core_bi_px_gpio_48 */
	{54, 566}, /* core_bi_px_gpio_52 */
	{55, 567}, /* core_bi_px_gpio_53 */
	{56, 568}, /* core_bi_px_gpio_54 */
	{57, 569}, /* core_bi_px_gpio_56 */
	{58, 570}, /* core_bi_px_gpio_57 */
	{59, 571}, /* bi_px_ssc_23 */
	{60, 572}, /* bi_px_ssc_24 */
	{61, 573}, /* bi_px_ssc_25 */
	{62, 574}, /* bi_px_ssc_26 */
	{63, 575}, /* bi_px_ssc_27 */
	{64, 576}, /* bi_px_ssc_28 */
	{65, 577}, /* bi_px_ssc_29 */
	{66, 578}, /* core_bi_px_gpio_66 */
	{67, 579}, /* core_bi_px_gpio_68 */
	{68, 580}, /* bi_px_ssc_20 */
	{69, 581}, /* bi_px_ssc_30 */
	{70, 582}, /* core_bi_px_gpio_77 */
	{71, 583}, /* core_bi_px_gpio_78 */
	{72, 584}, /* core_bi_px_gpio_79 */
	{73, 585}, /* core_bi_px_gpio_80 */
	{74, 586}, /* core_bi_px_gpio_84 */
	{75, 587}, /* core_bi_px_gpio_85 */
	{76, 588}, /* core_bi_px_gpio_86 */
	{77, 589}, /* core_bi_px_gpio_88 */
	{79, 591}, /* core_bi_px_gpio_91 */
	{80, 592}, /* core_bi_px_gpio_92 */
	{81, 593}, /* core_bi_px_gpio_95 */
	{82, 594}, /* core_bi_px_gpio_96 */
	{83, 595}, /* core_bi_px_gpio_97 */
	{84, 596}, /* core_bi_px_gpio_101 */
	{85, 597}, /* core_bi_px_gpio_103 */
	{86, 598}, /* bi_px_ssc_22 */
	{87, 599}, /* core_bi_px_to_mpm[6] */
	{88, 600}, /* core_bi_px_to_mpm[0] */
	{89, 601}, /* core_bi_px_to_mpm[1] */
	{90, 602}, /* core_bi_px_gpio_115 */
	{91, 603}, /* core_bi_px_gpio_116 */
	{92, 604}, /* core_bi_px_gpio_117 */
	{93, 605}, /* core_bi_px_gpio_118 */
	{94, 641}, /* core_bi_px_gpio_119 */
	{95, 642}, /* core_bi_px_gpio_120 */
	{96, 643}, /* core_bi_px_gpio_121 */
	{97, 644}, /* core_bi_px_gpio_122 */
	{98, 645}, /* core_bi_px_gpio_123 */
	{99, 646}, /* core_bi_px_gpio_124 */
	{100, 647}, /* core_bi_px_gpio_125 */
	{101, 648}, /* core_bi_px_to_mpm[5] */
	{102, 649}, /* core_bi_px_gpio_127 */
	{103, 650}, /* core_bi_px_gpio_128 */
	{104, 651}, /* core_bi_px_gpio_129 */
	{105, 652}, /* core_bi_px_gpio_130 */
	{106, 653}, /* core_bi_px_gpio_132 */
	{107, 654}, /* core_bi_px_gpio_133 */
	{108, 655}, /* core_bi_px_gpio_145 */
	{115, 662}, /* core_bi_px_gpio_41 */
	{116, 663}, /* core_bi_px_gpio_89 */
	{117, 664}, /* core_bi_px_gpio_31 */
	{118, 665}, /* core_bi_px_gpio_49 */
	{119, 666}, /* core_bi_px_to_mpm[2] */
	{120, 667}, /* core_bi_px_to_mpm[3] */
	{121, 668}, /* core_bi_px_to_mpm[4] */
	{-1}
};

static int __init qcom_pdc_gic_init(struct device_node *node,
		struct device_node *parent)
{
	return qcom_pdc_init(node, parent, sdm670_data);
}

IRQCHIP_DECLARE(pdc_sdm670, "qcom,pdc-sdm670", qcom_pdc_gic_init);

/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

static struct pdc_pin atoll_data[] = {
	{0, 512}, /* rpmh_wake */
	{1, 513}, /* ee0_apps_hlos_spmi_periph_irq */
	{2, 514}, /* ee1_apps_trustzone_spmi_periph_irq */
	{3, 515}, /* secure_wdog_expired */
	{4, 516}, /* secure_wdog_bark_irq */
	{5, 517}, /* aop_wdog_expired_irq */
	{6, 518}, /* qmp_usb3_lfps_rxterm_irq */
	{7, 519}, /* core_bi_px_gpio_42 */
	{8, 520}, /* eud_p0_dmse_int_mx */
	{9, 521}, /* eud_p0_dpse_int_mx */
	{10, 522}, /* lpass2aop_ipc_0 */
	{11, 523}, /* lpass2aop_ipc_1 */
	{12, 524}, /* eud_int_mx[1] */
	{13, 525}, /* va_dma_irq_aoss */
	{14, 526}, /* wd_bite_apps */
	{15, 527}, /* not-connected */
	{16, 528}, /* not-connected */
	{17, 529}, /* core_bi_px_gpio_49 */
	{18, 530}, /* aoss_pmic_arb_mpu_xpu_summary_irq */
	{19, 531}, /* rpmh_wake_2 */
	{20, 532}, /* core_bi_px_gpio_16 */
	{21, 533}, /* core_bi_px_gpio_23 */
	{22, 534}, /* pdc_apps_epcb_timeout_summary_irq */
	{23, 535}, /* spmi_protocol_irq */
	{24, 536}, /* tsense0_tsense_max_min_int */
	{25, 537}, /* tsense1_tsense_max_min_int */
	{26, 538}, /* tsense0_upper_lower_intr */
	{27, 539}, /* tsense1_upper_lower_intr */
	{28, 540}, /* tsense0_critical_intr */
	{29, 541}, /* tsense1_critical_intr */
	{30, 542}, /* core_bi_mx_to_aoss[0] */
	{31, 543}, /* core_bi_mx_to_aoss[2] */
	{32, 544}, /* core_bi_px_gpio_69 */
	{33, 545}, /* core_bi_px_gpio_31 */
	{34, 546}, /* core_bi_px_gpio_43 */
	{35, 547}, /* core_bi_px_gpio_9 */
	{36, 548}, /* core_bi_px_gpio_28 */
	{37, 549}, /* core_bi_px_gpio_59 */
	{38, 550}, /* core_bi_px_gpio_86 */
	{39, 551}, /* core_bi_px_gpio_87 */
	{40, 552}, /* core_bi_px_gpio_0 */
	{41, 553}, /* core_bi_px_gpio_6 */
	{42, 554}, /* core_bi_px_gpio_4 */
	{43, 555}, /* core_bi_px_gpio_34 */
	{44, 556}, /* core_bi_px_gpio_65 */
	{45, 557}, /* core_bi_px_gpio_88 */
	{46, 558}, /* core_bi_px_gpio_89 */
	{47, 559}, /* core_bi_px_gpio_90 */
	{48, 560}, /* core_bi_px_gpio_91 */
	{49, 561}, /* core_bi_px_gpio_93 */
	{50, 562}, /* core_bi_px_gpio_3 */
	{51, 563}, /* core_bi_px_gpio_11 */
	{52, 564}, /* core_bi_px_gpio_26 */
	{53, 565}, /* core_bi_px_gpio_37 */
	{54, 566}, /* core_bi_px_gpio_70 */
	{55, 567}, /* core_bi_px_gpio_21 */
	{56, 568}, /* core_bi_px_gpio_56 */
	{57, 569}, /* core_bi_px_gpio_57 */
	{58, 570}, /* core_bi_px_gpio_67 */
	{59, 571}, /* core_bi_px_gpio_72 */
	{60, 572}, /* core_bi_px_gpio_92 */
	{61, 573}, /* core_bi_px_gpio_24 */
	{62, 574}, /* gp_irq_hvm[32] */
	{63, 575}, /* gp_irq_hvm[33] */
	{64, 576}, /* gp_irq_hvm[34] */
	{65, 577}, /* gp_irq_hvm[35] */
	{66, 578}, /* gp_irq_hvm[36] */
	{67, 579}, /* gp_irq_hvm[37] */
	{68, 580}, /* gp_irq_hvm[38] */
	{69, 581}, /* gp_irq_hvm[39] */
	{70, 582}, /* core_bi_px_gpio_5 */
	{71, 583}, /* core_bi_px_gpio_74 */
	{72, 584}, /* core_bi_px_gpio_39 */
	{73, 585}, /* core_bi_px_gpio_45 */
	{74, 586}, /* core_bi_px_gpio_64 */
	{75, 587}, /* mgpi_mx_lpi_1 */
	{76, 588}, /* mgpi_mx_lpi_10 */
	{77, 589}, /* mgpi_mx_lpi_11 */
	{78, 590}, /* mgpi_mx_lpi_12 */
	{79, 591}, /* mgpi_mx_lpi_13 */
	{80, 592}, /* core_bi_px_gpio_10 */
	{81, 593}, /* core_bi_px_gpio_32 */
	{82, 594}, /* core_bi_px_gpio_47 */
	{83, 595}, /* core_bi_px_gpio_58 */
	{84, 596}, /* core_bi_px_gpio_94 */
	{85, 597}, /* mgpi_mx_lpi_2 */
	{86, 598}, /* mgpi_mx_lpi_4 */
	{87, 599}, /* mgpi_mx_lpi_6 */
	{88, 600}, /* mgpi_mx_lpi_7 */
	{89, 601}, /* mgpi_mx_lpi_9 */
	{90, 602}, /* core_bi_px_gpio_22 */
	{91, 603}, /* core_bi_px_gpio_36 */
	{92, 604}, /* core_bi_px_gpio_55 */
	{93, 605}, /* core_bi_px_gpio_66 */
	{94, 641}, /* core_bi_px_gpio_95 */
	{95, 642}, /* core_bi_px_sdc1_data_1 */
	{96, 643}, /* core_bi_px_sdc2_data_1 */
	{97, 644}, /* core_bi_px_sdc2_data_3 */
	{98, 645}, /* core_bi_px_sdc2_cmd */
	{99, 646}, /* mgpi_mx_ssc_13 */
	{100, 647}, /* core_bi_px_gpio_30 */
	{101, 648}, /* core_bi_px_gpio_41 */
	{102, 649}, /* core_bi_px_gpio_53 */
	{103, 650}, /* core_bi_mx_to_aoss[1] */
	{104, 651}, /* core_bi_px_gpio_109 */
	{105, 652}, /* mgpi_mx_ssc_15 */
	{106, 653}, /* mgpi_mx_ssc_4 */
	{107, 654}, /* mgpi_mx_ssc_7 */
	{108, 655}, /* core_bi_px_gpio_115 */
	{109, 656}, /* core_bi_px_gpio_52 */
	{110, 657}, /* core_bi_px_gpio_62 */
	{111, 658}, /* core_bi_px_gpio_63 */
	{112, 659}, /* core_bi_px_gpio_68 */
	{113, 660}, /* core_bi_px_gpio_114 */
	{114, 661}, /* core_bi_px_gpio_117 */
	{115, 662}, /* gp_irq_hvm[85] */
	{116, 663}, /* gp_irq_hvm[86] */
	{117, 664}, /* gp_irq_hvm[87] */
	{118, 665}, /* gp_irq_hvm[88] */
	{119, 666}, /* core_bi_px_gpio_118 */
	{120, 667}, /* mgpi_mx_ssc_0 */
	{121, 668}, /* core_bi_px_gpio_116 */
	{122, 669}, /* core_bi_px_gpio_34 */
	{123, 670}, /* gp_irq_hvm[93] */
	{124, 671}, /* core_bi_px_gpio_59 */
	{125, 95}, /* gp_irq_hvm[95] */
	{-1}
};

static int __init qcom_pdc_gic_init(struct device_node *node,
					struct device_node *parent)
{
	return qcom_pdc_init(node, parent, atoll_data);
}

IRQCHIP_DECLARE(pdc_atoll, "qcom,pdc-atoll", qcom_pdc_gic_init);

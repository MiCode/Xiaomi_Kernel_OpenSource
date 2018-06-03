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
 *
 */

#include <linux/irqchip.h>
#include "pdc.h"

static struct pdc_pin sm8150_data[] = {
	{0, 512},/*rpmh_wake*/
	{1, 513},/*ee0_apps_hlos_spmi_periph_irq*/
	{2, 514},/*ee1_apps_trustzone_spmi_periph_irq*/
	{3, 515},/*secure_wdog_expired*/
	{4, 516},/*secure_wdog_bark_irq*/
	{5, 517},/*aop_wdog_expired_irq*/
	{6, 518},/*qmp_usb3_lfps_rxterm_irq*/
	{7, 519},/*qmp_usb3_lfps_rxterm_irq*/
	{8, 520},/*eud_p0_dmse_int_mx*/
	{9, 521},/*eud_p0_dpse_int_mx*/
	{10, 522},/*eud_p1_dmse_int_mx*/
	{11, 523},/*eud_p1_dpse_int_mx*/
	{12, 524},/*eud_int_mx[1]*/
	{13, 525},/*xpu_irq_summary*/
	{14, 526},/*wd_bite_apps*/
	{15, 527},/*vmidmt_irq_summary*/
	{16, 528},/*q6ss_irq_out_apps_ipc[4]*/
	{17, 529},/*not-connected*/
	{18, 530},/*aoss_pmic_arb_mpu_xpu_summary_irq*/
	{19, 531},/*rpmh_wake_2*/
	{20, 532},/*apps_pdc_irq_in_20*/
	{21, 533},/*apps_pdc_irq_in_21*/
	{22, 534},/*pdc_apps_epcb_timeout_summary_irq*/
	{23, 535},/*spmi_protocol_irq*/
	{24, 536},/*tsense0_tsense_max_min_int*/
	{25, 537},/*tsense1_tsense_max_min_int*/
	{26, 538},/*tsense0_upper_lower_intr*/
	{27, 539},/*tsense1_upper_lower_intr*/
	{28, 540},/*tsense0_critical_intr*/
	{29, 541},/*tsense1_critical_intr*/
	{30, 542},/*core_bi_px_core_in_mx_gpio_38*/
	{31, 543},/*core_bi_px_core_in_mx_gpio_3*/
	{32, 544},/*core_bi_px_core_in_mx_gpio_5*/
	{33, 545},/*core_bi_px_core_in_mx_gpio_8*/
	{34, 546},/*core_bi_px_core_in_mx_gpio_9*/
	{35, 547},/*gp_irq_hvm[5]*/
	{36, 548},/*core_bi_px_core_in_mx_gpio_134*/
	{37, 549},/*core_bi_px_core_in_mx_gpio_24*/
	{38, 550},/*core_bi_px_core_in_mx_gpio_26*/
	{39, 551},/*core_bi_px_core_in_mx_gpio_30*/
	{40, 552},/*core_bi_px_core_in_mx_gpio_101*/
	{41, 553},/*core_bi_px_core_in_mx_gpio_27*/
	{42, 554},/*core_bi_px_core_in_mx_gpio_28*/
	{43, 555},/*core_bi_px_core_in_mx_gpio_36*/
	{44, 556},/*core_bi_px_core_in_mx_gpio_37*/
	{45, 557},/*gp_irq_hvm[15]*/
	{46, 558},/*gp_irq_hvm[16]*/
	{47, 559},/*core_bi_px_core_in_mx_gpio_41*/
	{48, 560},/*core_bi_px_core_in_mx_gpio_42*/
	{49, 561},/*core_bi_px_core_in_mx_gpio_47*/
	{50, 562},/*core_bi_px_core_in_mx_gpio_46*/
	{51, 563},/*core_bi_px_core_in_mx_gpio_48*/
	{52, 564},/*core_bi_px_core_in_mx_gpio_50*/
	{53, 565},/*core_bi_px_core_in_mx_gpio_49*/
	{54, 566},/*core_bi_px_core_in_mx_gpio_53*/
	{55, 567},/*core_bi_px_core_in_mx_gpio_54*/
	{56, 568},/*core_bi_px_core_in_mx_gpio_55*/
	{57, 569},/*core_bi_px_core_in_mx_gpio_56*/
	{58, 570},/*core_bi_px_core_in_mx_gpio_58*/
	{59, 571},/*gp_irq_hvm[29]*/
	{60, 572},/*core_bi_px_core_in_mx_gpio_60*/
	{61, 573},/*core_bi_px_core_in_mx_gpio_61_from_and_gate_to_mpm*/
	{62, 574},/*core_bi_px_core_in_mx_gpio_68*/
	{63, 575},/*core_bi_px_core_in_mx_gpio_70*/
	{64, 576},/*core_bi_px_core_in_mx_gpio_81*/
	{65, 577},/*core_bi_px_core_in_mx_gpio_83*/
	{66, 578},/*core_bi_px_core_in_mx_gpio_77*/
	{67, 579},/*core_bi_px_core_in_mx_gpio_86*/
	{68, 580},/*gp_irq_hvm[38]*/
	{69, 581},/*core_bi_px_core_in_mx_gpio_90*/
	{70, 582},/*core_bi_px_core_in_mx_gpio_91*/
	{71, 583},/*core_bi_px_core_in_mx_gpio_76*/
	{72, 584},/*core_bi_px_core_in_mx_gpio_95*/
	{73, 585},/*core_bi_px_core_in_mx_gpio_96_from_and_gate_to_mpm*/
	{74, 586},/*core_bi_px_core_in_mx_gpio_97*/
	{75, 587},/*core_bi_px_core_in_mx_gpio_93*/
	{76, 588},/*gp_irq_hvm[46]*/
	{77, 589},/*core_bi_px_core_in_mx_gpio_103*/
	{78, 590},/*core_bi_px_core_in_mx_gpio_104*/
	{79, 591},/*core_bi_px_core_in_mx_gpio_108_from_and_gate_to_mpm*/
	{80, 592},/*core_bi_px_core_in_mx_gpio_112_from_and_gate_to_mpm*/
	{81, 593},/*core_bi_px_core_in_mx_gpio_113_from_and_gate_to_mpm*/
	{82, 594},/*core_bi_px_core_in_mx_gpio_114*/
	{83, 595},/*core_bi_px_core_in_mx_gpio_133*/
	{84, 596},/*core_bi_px_core_in_mx_gpio_87*/
	{85, 597},/*core_bi_px_core_in_mx_gpio_117*/
	{86, 598},/*gp_irq_hvm[56]*/
	{87, 599},/*core_bi_px_core_in_mx_gpio_119*/
	{88, 600},/*core_bi_px_core_in_mx_gpio_120*/
	{89, 601},/*core_bi_px_core_in_mx_gpio_121*/
	{90, 602},/*core_bi_px_core_in_mx_gpio_122*/
	{91, 603},/*core_bi_px_core_in_mx_gpio_123*/
	{92, 604},/*core_bi_px_core_in_mx_gpio_124*/
	{93, 605},/*core_bi_px_core_in_mx_gpio_125*/
	{94, 641},/*core_bi_px_core_in_mx_gpio_129*/
	{95, 642},/*gp_irq_hvm[65]*/
	{96, 643},/*gp_irq_hvm[66]*/
	{97, 644},/*core_bi_px_core_in_mx_gpio_136*/
	{98, 645},/*gp_irq_hvm[68]*/
	{99, 646},/*gp_irq_hvm[69]*/
	{100, 647},/*core_bi_px_core_in_mx_gpio_10*/
	{101, 648},/*core_bi_px_core_in_mx_gpio_118*/
	{102, 649},/*core_bi_px_core_in_mx_gpio_147*/
	{103, 650},/*core_bi_px_core_in_mx_gpio_142*/
	{104, 651},/*core_bi_px_core_in_mx_gpio_12*/
	{105, 652},/*core_bi_px_core_in_mx_gpio_132*/
	{106, 653},/*gp_irq_hvm[76]*/
	{107, 654},/*core_bi_px_core_in_mx_gpio_150*/
	{108, 655},/*core_bi_px_core_in_mx_gpio_152*/
	{109, 656},/*core_bi_px_core_in_mx_gpio_153*/
	{110, 657},/*gp_irq_hvm[80]*/
	{111, 658},/*gp_irq_hvm[81]*/
	{112, 659},/*gp_irq_hvm[82]*/
	{113, 660},/*gp_irq_hvm[83]*/
	{114, 661},/*gp_irq_hvm[84]*/
	{115, 662},/*core_bi_px_core_in_mx_gpio_144*/
	{116, 663},/*core_bi_px_core_in_mx_gpio_51*/
	{117, 664},/*core_bi_px_core_in_mx_gpio_88*/
	{118, 665},/*core_bi_px_core_in_mx_gpio_39*/
	{119, 666},/*core_bi_px_core_in_mx_gpio_sdc2_data_1_from_and_to_mpm*/
	{120, 667},/*core_bi_px_core_in_mx_gpio_sdc2_data_3_from_and__to_mpm*/
	{121, 668},/*core_bi_px_core_in_mx_gpio_sdc2_cmd_from_and_gate_to_mpm*/
	{122, 669},/*core_bi_px_core_in_mx_gpio_144*/
	{123, 670},/*core_bi_px_core_in_mx_gpio_51*/
	{124, 671},/*core_bi_px_core_in_mx_gpio_88*/
	{125, 95},/*core_bi_px_core_in_mx_gpio_39*/
	{-1},
};

static int __init qcom_pdc_gic_init(struct device_node *node,
		struct device_node *parent)
{
	pr_info("PDC SM8150 initialized\n");
	return qcom_pdc_init(node, parent, sm8150_data);
}

IRQCHIP_DECLARE(pdc_sm8150, "qcom,pdc-sm8150", qcom_pdc_gic_init);

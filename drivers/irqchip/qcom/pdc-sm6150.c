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

static struct pdc_pin sm6150_data[] = {
	{0, 512},/*rpmh_wake*/
	{1, 513},/*ee0_apps_hlos_spmi_periph_irq*/
	{2, 514},/*ee1_apps_trustzone_spmi_periph_irq*/
	{3, 515},/*secure_wdog_expired*/
	{4, 516},/*secure_wdog_bark_irq*/
	{5, 517},/*aop_wdog_expired_irq*/
	{6, 518},/*not-connected*/
	{7, 519},/*not-connected*/
	{8, 520},/*eud_p0_dmse_int_mx*/
	{9, 521},/*eud_p0_dpse_int_mx*/
	{10, 522},/*eud_p1_dmse_int_mx*/
	{11, 523},/*eud_p1_dpse_int_mx*/
	{12, 524},/*eud_int_mx[1]*/
	{13, 525},/*ssc_xpu_irq_summary*/
	{14, 526},/*wd_bite_apps*/
	{15, 527},/*ssc_vmidmt_irq_summary*/
	{16, 528},/*sdc_gpo[0]*/
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
	{30, 542},/*gp_irq_hvm[0]*/
	{31, 543},/*core_bi_px_gpio_3*/
	{32, 544},/*gp_irq_hvm[2]*/
	{33, 545},/*core_bi_px_gpio_13*/
	{34, 546},/*core_bi_px_gpio_11*/
	{35, 547},/*core_bi_px_gpio_14*/
	{36, 548},/*core_bi_px_gpio_22*/
	{37, 549},/*core_bi_px_gpio_35*/
	{38, 550},/*core_bi_px_gpio_26*/
	{39, 551},/*gp_irq_hvm[9]*/
	{40, 552},/*core_bi_px_gpio_101*/
	{41, 553},/*gp_irq_hvm[11]*/
	{42, 554},/*gp_irq_hvm[12]*/
	{43, 555},/*gp_irq_hvm[13]*/
	{44, 556},/*bi_px_ssc_29_px_to_mx*/
	{45, 557},/*core_bi_px_gpio_1*/
	{46, 558},/*core_bi_px_gpio_17*/
	{47, 559},/*core_bi_px_gpio_41*/
	{48, 560},/*core_bi_px_gpio_19*/
	{49, 561},/*core_bi_px_gpio_47*/
	{50, 562},/*core_bi_px_gpio_82*/
	{51, 563},/*core_bi_px_gpio_48*/
	{52, 564},/*core_bi_px_gpio_50*/
	{53, 565},/*bi_px_ssc_28_px_to_mx*/
	{54, 566},/*core_bi_px_gpio_71*/
	{55, 567},/*core_bi_px_gpio_7*/
	{56, 568},/*core_bi_px_gpio_55*/
	{57, 569},/*core_bi_px_gpio_56*/
	{58, 570},/*core_bi_px_gpio_57*/
	{59, 571},/*bi_px_ssc_23_px_to_mx*/
	{60, 572},/*core_bi_px_gpio_60*/
	{61, 573},/*bi_px_ssc_25_px_to_mx*/
	{62, 574},/*bi_px_ssc_24_px_to_mx*/
	{63, 575},/*bi_px_ssc_27_px_to_mx*/
	{64, 576},/*core_bi_px_gpio_81*/
	{65, 577},/*core_bi_px_gpio_83*/
	{66, 578},/*gp_irq_hvm[36]*/
	{67, 579},/*core_bi_px_gpio_86*/
	{68, 580},/*bi_px_ssc_20_px_to_mx*/
	{69, 581},/*core_bi_px_gpio_90*/
	{70, 582},/*bi_px_ssc_30_px_to_mx*/
	{71, 583},/*gp_irq_hvm[41]*/
	{72, 584},/*core_bi_px_gpio_95*/
	{73, 585},/*core_bi_px_gpio_80*/
	{74, 586},/*core_bi_px_gpio_97*/
	{75, 587},/*core_bi_px_gpio_93*/
	{76, 588},/*bi_px_ssc_26_px_to_mx*/
	{77, 589},/*core_bi_px_gpio_103*/
	{78, 590},/*core_bi_px_gpio_104*/
	{79, 591},/*gp_irq_hvm[49]*/
	{80, 592},/*gp_irq_hvm[50]*/
	{81, 593},/*gp_irq_hvm[51]*/
	{82, 594},/*core_bi_px_gpio_96*/
	{83, 595},/*core_bi_px_gpio_21*/
	{84, 596},/*core_bi_px_gpio_87*/
	{85, 597},/*core_bi_px_gpio_117*/
	{86, 598},/*bi_px_ssc_22_px_to_mx*/
	{87, 599},/*core_bi_px_gpio_119*/
	{88, 600},/*core_bi_px_gpio_92*/
	{89, 601},/*core_bi_px_gpio_121*/
	{90, 602},/*core_bi_px_gpio_122*/
	{91, 603},/*core_bi_px_gpio_94*/
	{92, 604},/*core_bi_px_gpio_84*/
	{93, 605},/*core_bi_px_gpio_102*/
	{94, 641},/*core_bi_px_gpio_99*/
	{95, 642},/*core_bi_px_gpio_98*/
	{96, 643},/*core_bi_px_gpio_105*/
	{97, 644},/*core_bi_px_gpio_107*/
	{98, 645},/*gp_irq_hvm[68]*/
	{99, 646},/*core_bi_px_gpio_85*/
	{100, 647},/*core_bi_px_gpio_100*/
	{101, 648},/*gp_irq_hvm[71]*/
	{102, 649},/*core_bi_px_gpio_118*/
	{103, 650},/*gp_irq_hvm[73]*/
	{104, 651},/*gp_irq_hvm[74]*/
	{105, 652},/*gp_irq_hvm[75]*/
	{106, 653},/*gp_irq_hvm[76]*/
	{107, 654},/*gp_irq_hvm[77]*/
	{108, 655},/*gp_irq_hvm[78]*/
	{109, 656},/*core_bi_px_sdc1_data_1_px_to_mx*/
	{110, 657},/*core_bi_px_gpio_9*/
	{111, 658},/*core_bi_px_gpio_108*/
	{112, 659},/*core_bi_px_gpio_112*/
	{113, 660},/*core_bi_px_gpio_113*/
	{114, 661},/*core_bi_px_gpio_120*/
	{115, 662},/*core_bi_px_gpio_89*/
	{116, 663},/*core_bi_px_gpio_51*/
	{117, 664},/*core_bi_px_gpio_88*/
	{118, 665},/*core_bi_px_gpio_39*/
	{119, 666},/*gp_irq_hvm[89]*/
	{120, 667},/*gp_irq_hvm[90]*/
	{121, 668},/*gp_irq_hvm[91]*/
	{122, 669},/*core_bi_px_gpio_89*/
	{123, 670},/*core_bi_px_gpio_51*/
	{124, 671},/*core_bi_px_gpio_88*/
	{125, 672},/*core_bi_px_gpio_39*/
	{-1},
};

static int __init qcom_pdc_gic_init(struct device_node *node,
		struct device_node *parent)
{
	return qcom_pdc_init(node, parent, sm6150_data);
}

IRQCHIP_DECLARE(pdc_sm6150, "qcom,pdc-sm6150", qcom_pdc_gic_init);

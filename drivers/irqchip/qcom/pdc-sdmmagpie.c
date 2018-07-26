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

static struct pdc_pin sdmmagpie_data[] = {
	{0, 512},/*rpmh_wake*/
	{1, 513},/*ee0_apps_hlos_spmi_periph_irq*/
	{2, 514},/*ee1_apps_trustzone_spmi_periph_irq*/
	{3, 515},/*secure_wdog_expired*/
	{4, 516},/*secure_wdog_bark_irq*/
	{5, 517},/*aop_wdog_expired_irq*/
	{6, 518},/*qmp_usb3_lfps_rxterm_irq*/
	{7, 519},/*not-connected*/
	{8, 520},/*eud_p0_dmse_int_mx*/
	{9, 521},/*eud_p0_dpse_int_mx*/
	{10, 522},/*not-connected*/
	{11, 523},/*not-connected*/
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
	{30, 542},/*core_bi_px_gpio_82*/
	{31, 543},/*core_bi_px_gpio_78*/
	{32, 544},/*core_bi_px_gpio_69*/
	{33, 545},/*core_bi_px_gpio_31*/
	{34, 546},/*core_bi_px_gpio_43*/
	{35, 547},/*core_bi_px_gpio_42*/
	{36, 548},/*core_bi_px_gpio_48*/
	{37, 549},/*core_bi_px_gpio_49*/
	{38, 550},/*core_bi_px_gpio_50*/
	{39, 551},/*core_bi_px_gpio_52*/
	{40, 552},/*core_bi_px_gpio_0*/
	{41, 553},/*core_bi_px_gpio_6*/
	{42, 554},/*core_bi_px_gpio_4*/
	{43, 555},/*core_bi_px_gpio_34*/
	{44, 556},/*core_bi_px_gpio_65*/
	{45, 557},/*core_bi_px_gpio_56*/
	{46, 558},/*core_bi_px_gpio_57*/
	{47, 559},/*core_bi_px_gpio_59*/
	{48, 560},/*core_bi_px_gpio_62*/
	{49, 561},/*core_bi_px_gpio_67*/
	{50, 562},/*core_bi_px_gpio_3*/
	{51, 563},/*core_bi_px_gpio_11*/
	{52, 564},/*core_bi_px_gpio_26*/
	{53, 565},/*core_bi_px_gpio_37*/
	{54, 566},/*core_bi_px_gpio_70*/
	{55, 567},/*core_bi_px_gpio_68*/
	{56, 568},/*core_bi_px_gpio_30*/
	{57, 569},/*core_bi_px_gpio_9*/
	{58, 570},/*core_bi_px_gpio_84*/
	{59, 571},/*core_bi_px_gpio_86*/
	{60, 572},/*core_bi_px_gpio_87*/
	{61, 573},/*core_bi_px_gpio_24*/
	{62, 574},/*core_bi_px_gpio_33*/
	{63, 575},/*core_bi_px_gpio_38*/
	{64, 576},/*core_bi_px_gpio_73*/
	{65, 577},/*core_bi_px_gpio_88*/
	{66, 578},/*core_bi_px_gpio_89*/
	{67, 579},/*core_bi_px_gpio_90*/
	{68, 580},/*core_bi_px_gpio_91*/
	{69, 581},/*core_bi_px_gpio_92*/
	{70, 582},/*core_bi_px_gpio_5*/
	{71, 583},/*core_bi_px_gpio_74*/
	{72, 584},/*core_bi_px_gpio_39*/
	{73, 585},/*core_bi_px_gpio_45*/
	{74, 586},/*core_bi_px_gpio_64*/
	{75, 587},/*core_bi_px_gpio_93*/
	{76, 588},/*core_bi_px_gpio_96*/
	{77, 589},/*core_bi_px_gpio_98*/
	{78, 590},/*core_bi_px_gpio_101*/
	{79, 591},/*core_bi_px_gpio_110*/
	{80, 592},/*core_bi_px_gpio_10*/
	{81, 593},/*core_bi_px_gpio_32*/
	{82, 594},/*core_bi_px_gpio_47*/
	{83, 595},/*core_bi_px_gpio_58*/
	{84, 596},/*core_bi_px_gpio_94*/
	{85, 597},/*core_bi_px_gpio_113*/
	{86, 598},/*gp_irq_hvm[56]*/
	{87, 599},/*gp_irq_hvm[57]*/
	{88, 600},/*gp_irq_hvm[58]*/
	{89, 601},/*gp_irq_hvm[59]*/
	{90, 602},/*core_bi_px_gpio_22*/
	{91, 603},/*core_bi_px_gpio_36*/
	{92, 604},/*core_bi_px_gpio_55*/
	{93, 605},/*core_bi_px_gpio_66*/
	{94, 641},/*core_bi_px_gpio_95*/
	{95, 642},/*bi_px_ssc_27_mx*/
	{96, 643},/*bi_px_ssc_28_mx*/
	{97, 644},/*bi_px_ssc_29_mx*/
	{98, 645},/*bi_px_ssc_30_mx*/
	{99, 646},/*core_bi_px_gpio_104*/
	{100, 647},/*core_bi_px_gpio_30*/
	{101, 648},/*core_bi_px_gpio_41*/
	{102, 649},/*core_bi_px_gpio_53*/
	{103, 650},/*core_bi_px_gpio_85*/
	{104, 651},/*core_bi_px_gpio_109*/
	{105, 652},/*bi_px_ssc_18_mx*/
	{106, 653},/*bi_px_ssc_20_mx*/
	{107, 654},/*bi_px_ssc_23_mx*/
	{108, 655},/*bi_px_ssc_24_mx*/
	{109, 656},/*bi_px_ssc_25_mx*/
	{110, 657},/*bi_px_ssc_26_mx*/
	{111, 658},/*core_bi_px_sdc1_data_1*/
	{112, 659},/*core_bi_px_sdc2_data[1]*/
	{113, 660},/*core_bi_px_sdc2_data[3]*/
	{114, 661},/*core_bi_px_sdc2_cmd*/
	{115, 662},/*gp_irq_hvm[85]*/
	{116, 663},/*gp_irq_hvm[86]*/
	{117, 664},/*gp_irq_hvm[87]*/
	{118, 665},/*gp_irq_hvm[88]*/
	{119, 666},/*gp_irq_hvm[89]*/
	{120, 667},/*gp_irq_hvm[90]*/
	{121, 668},/*gp_irq_hvm[91]*/
	{122, 669},/*gp_irq_hvm[92]*/
	{123, 670},/*core_bi_px_gpio_0*/
	{124, 671},/*core_bi_px_gpio_59*/
	{125, 95},/*core_bi_px_gpio_6*/
	{-1}
};

static int __init qcom_pdc_gic_init(struct device_node *node,
		struct device_node *parent)
{
	return qcom_pdc_init(node, parent, sdmmagpie_data);
}

IRQCHIP_DECLARE(pdc_sdmmagpie, "qcom,pdc-sdmmagpie", qcom_pdc_gic_init);

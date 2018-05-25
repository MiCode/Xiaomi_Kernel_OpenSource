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
	{30, 542},/*apps_pdc.gp_irq_mux[0]*/
	{31, 543},/*apps_pdc.gp_irq_mux[1]*/
	{32, 544},/*apps_pdc.gp_irq_mux[2]*/
	{33, 545},/*apps_pdc.gp_irq_mux[3]*/
	{34, 546},/*apps_pdc.gp_irq_mux[4]*/
	{35, 547},/*apps_pdc.gp_irq_mux[5]*/
	{36, 548},/*apps_pdc.gp_irq_mux[6]*/
	{37, 549},/*apps_pdc.gp_irq_mux[7]*/
	{38, 550},/*apps_pdc.gp_irq_mux[8]*/
	{39, 551},/*apps_pdc.gp_irq_mux[9]*/
	{40, 552},/*apps_pdc.gp_irq_mux[10]*/
	{41, 553},/*apps_pdc.gp_irq_mux[11]*/
	{42, 554},/*apps_pdc.gp_irq_mux[12]*/
	{43, 555},/*apps_pdc.gp_irq_mux[13]*/
	{44, 556},/*apps_pdc.gp_irq_mux[14]*/
	{45, 557},/*apps_pdc.gp_irq_mux[15]*/
	{46, 558},/*apps_pdc.gp_irq_mux[16]*/
	{47, 559},/*apps_pdc.gp_irq_mux[17]*/
	{48, 560},/*apps_pdc.gp_irq_mux[18]*/
	{49, 561},/*apps_pdc.gp_irq_mux[19]*/
	{50, 562},/*apps_pdc.gp_irq_mux[20]*/
	{51, 563},/*apps_pdc.gp_irq_mux[21]*/
	{52, 564},/*apps_pdc.gp_irq_mux[22]*/
	{53, 565},/*apps_pdc.gp_irq_mux[23]*/
	{54, 566},/*apps_pdc.gp_irq_mux[24]*/
	{55, 567},/*apps_pdc.gp_irq_mux[25]*/
	{56, 568},/*apps_pdc.gp_irq_mux[26]*/
	{57, 569},/*apps_pdc.gp_irq_mux[27]*/
	{58, 570},/*apps_pdc.gp_irq_mux[28]*/
	{59, 571},/*apps_pdc.gp_irq_mux[29]*/
	{60, 572},/*apps_pdc.gp_irq_mux[30]*/
	{61, 573},/*apps_pdc.gp_irq_mux[31]*/
	{62, 574},/*apps_pdc.gp_irq_mux[32]*/
	{63, 575},/*apps_pdc.gp_irq_mux[33]*/
	{64, 576},/*apps_pdc.gp_irq_mux[34]*/
	{65, 577},/*apps_pdc.gp_irq_mux[35]*/
	{66, 578},/*apps_pdc.gp_irq_mux[36]*/
	{67, 579},/*apps_pdc.gp_irq_mux[37]*/
	{68, 580},/*apps_pdc.gp_irq_mux[38]*/
	{69, 581},/*apps_pdc.gp_irq_mux[39]*/
	{70, 582},/*apps_pdc.gp_irq_mux[40]*/
	{71, 583},/*apps_pdc.gp_irq_mux[41]*/
	{72, 584},/*apps_pdc.gp_irq_mux[42]*/
	{73, 585},/*apps_pdc.gp_irq_mux[43]*/
	{74, 586},/*apps_pdc.gp_irq_mux[44]*/
	{75, 587},/*apps_pdc.gp_irq_mux[45]*/
	{76, 588},/*apps_pdc.gp_irq_mux[46]*/
	{77, 589},/*apps_pdc.gp_irq_mux[47]*/
	{78, 590},/*apps_pdc.gp_irq_mux[48]*/
	{79, 591},/*apps_pdc.gp_irq_mux[49]*/
	{80, 592},/*apps_pdc.gp_irq_mux[50]*/
	{81, 593},/*apps_pdc.gp_irq_mux[51]*/
	{82, 594},/*apps_pdc.gp_irq_mux[52]*/
	{83, 595},/*apps_pdc.gp_irq_mux[53]*/
	{84, 596},/*apps_pdc.gp_irq_mux[54]*/
	{85, 597},/*apps_pdc.gp_irq_mux[55]*/
	{86, 598},/*apps_pdc.gp_irq_mux[56]*/
	{87, 599},/*apps_pdc.gp_irq_mux[57]*/
	{88, 600},/*apps_pdc.gp_irq_mux[58]*/
	{89, 601},/*apps_pdc.gp_irq_mux[59]*/
	{90, 602},/*apps_pdc.gp_irq_mux[60]*/
	{91, 603},/*apps_pdc.gp_irq_mux[61]*/
	{92, 604},/*apps_pdc.gp_irq_mux[62]*/
	{93, 605},/*apps_pdc.gp_irq_mux[63]*/
	{94, 641},/*apps_pdc.gp_irq_mux[64]*/
	{95, 642},/*apps_pdc.gp_irq_mux[65]*/
	{96, 643},/*apps_pdc.gp_irq_mux[66]*/
	{97, 644},/*apps_pdc.gp_irq_mux[67]*/
	{98, 645},/*apps_pdc.gp_irq_mux[68]*/
	{99, 646},/*apps_pdc.gp_irq_mux[69]*/
	{100, 647},/*apps_pdc.gp_irq_mux[70]*/
	{101, 648},/*apps_pdc.gp_irq_mux[71]*/
	{102, 649},/*apps_pdc.gp_irq_mux[72]*/
	{103, 650},/*apps_pdc.gp_irq_mux[73]*/
	{104, 651},/*apps_pdc.gp_irq_mux[74]*/
	{105, 652},/*apps_pdc.gp_irq_mux[75]*/
	{106, 653},/*apps_pdc.gp_irq_mux[76]*/
	{107, 654},/*apps_pdc.gp_irq_mux[77]*/
	{108, 655},/*apps_pdc.gp_irq_mux[78]*/
	{109, 656},/*apps_pdc.gp_irq_mux[79]*/
	{110, 657},/*apps_pdc.gp_irq_mux[80]*/
	{111, 658},/*apps_pdc.gp_irq_mux[81]*/
	{112, 659},/*apps_pdc.gp_irq_mux[82]*/
	{113, 660},/*apps_pdc.gp_irq_mux[83]*/
	{114, 661},/*apps_pdc.gp_irq_mux[84]*/
	{115, 662},/*apps_pdc.gp_irq_mux[85]*/
	{116, 663},/*apps_pdc.gp_irq_mux[86]*/
	{117, 664},/*apps_pdc.gp_irq_mux[87]*/
	{118, 665},/*apps_pdc.gp_irq_mux[88]*/
	{119, 666},/*apps_pdc.gp_irq_mux[89]*/
	{120, 667},/*apps_pdc.gp_irq_mux[90]*/
	{121, 668},/*apps_pdc.gp_irq_mux[91]*/
	{122, 669},/*apps_pdc.gp_irq_mux[92]*/
	{123, 670},/*apps_pdc.gp_irq_mux[93]*/
	{124, 671},/*apps_pdc.gp_irq_mux[94]*/
	{125, 95},/*apps_pdc.gp_irq_mux[95]*/
	{-1}
};

static int __init qcom_pdc_gic_init(struct device_node *node,
		struct device_node *parent)
{
	return qcom_pdc_init(node, parent, sdmmagpie_data);
}

IRQCHIP_DECLARE(pdc_sdmmagpie, "qcom,pdc-sdmmagpie", qcom_pdc_gic_init);

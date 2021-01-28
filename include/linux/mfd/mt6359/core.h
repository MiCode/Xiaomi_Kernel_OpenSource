/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MFD_MT6359_CORE_H__
#define __MFD_MT6359_CORE_H__

#define MT6359_REG_WIDTH 16

enum mt6359_irq_top_status_shift {
	MT6359_BUCK_TOP = 0,
	MT6359_LDO_TOP = 1,
	MT6359_PSC_TOP = 2,
	MT6359_SCK_TOP = 3,
	MT6359_BM_TOP = 4,
	MT6359_HK_TOP = 5,
	MT6359_AUD_TOP = 7,
	MT6359_MISC_TOP = 8,
};

enum mt6359_irq_numbers {
	MT6359_IRQ_VPU_OC = 0,
	MT6359_IRQ_VCORE_OC = 1,
	MT6359_IRQ_VGPU11_OC = 2,
	MT6359_IRQ_VGPU12_OC = 3,
	MT6359_IRQ_VMODEM_OC = 4,
	MT6359_IRQ_VPROC1_OC = 5,
	MT6359_IRQ_VPROC2_OC = 6,
	MT6359_IRQ_VS1_OC = 7,
	MT6359_IRQ_VS2_OC = 8,
	MT6359_IRQ_VPA_OC = 9,
	MT6359_IRQ_VFE28_OC = 16,
	MT6359_IRQ_VXO22_OC = 17,
	MT6359_IRQ_VRF18_OC = 18,
	MT6359_IRQ_VRF12_OC = 19,
	MT6359_IRQ_VEFUSE_OC = 20,
	MT6359_IRQ_VCN33_1_OC = 21,
	MT6359_IRQ_VCN33_2_OC = 22,
	MT6359_IRQ_VCN13_OC = 23,
	MT6359_IRQ_VCN18_OC = 24,
	MT6359_IRQ_VA09_OC = 25,
	MT6359_IRQ_VCAMIO_OC = 26,
	MT6359_IRQ_VA12_OC = 27,
	MT6359_IRQ_VAUX18_OC = 28,
	MT6359_IRQ_VAUD18_OC = 29,
	MT6359_IRQ_VIO18_OC = 30,
	MT6359_IRQ_VSRAM_PROC1_OC = 31,
	MT6359_IRQ_VSRAM_PROC2_OC = 32,
	MT6359_IRQ_VSRAM_OTHERS_OC = 33,
	MT6359_IRQ_VSRAM_MD_OC = 34,
	MT6359_IRQ_VEMC_OC = 35,
	MT6359_IRQ_VSIM1_OC = 36,
	MT6359_IRQ_VSIM2_OC = 37,
	MT6359_IRQ_VUSB_OC = 38,
	MT6359_IRQ_VRFCK_OC = 39,
	MT6359_IRQ_VBBCK_OC = 40,
	MT6359_IRQ_VBIF28_OC = 41,
	MT6359_IRQ_VIBR_OC = 42,
	MT6359_IRQ_VIO28_OC = 43,
	MT6359_IRQ_VM18_OC = 44,
	MT6359_IRQ_VUFS_OC = 45,
	MT6359_IRQ_PWRKEY = 48,
	MT6359_IRQ_HOMEKEY = 49,
	MT6359_IRQ_PWRKEY_R = 50,
	MT6359_IRQ_HOMEKEY_R = 51,
	MT6359_IRQ_NI_LBAT_INT = 52,
	MT6359_IRQ_CHRDET_EDGE = 53,
	MT6359_IRQ_RTC = 64,
	MT6359_IRQ_FG_BAT_H = 80,
	MT6359_IRQ_FG_BAT_L = 81,
	MT6359_IRQ_FG_CUR_H = 82,
	MT6359_IRQ_FG_CUR_L = 83,
	MT6359_IRQ_FG_ZCV = 84,
	MT6359_IRQ_FG_N_CHARGE_L = 87,
	MT6359_IRQ_FG_IAVG_H = 88,
	MT6359_IRQ_FG_IAVG_L = 89,
	MT6359_IRQ_FG_DISCHARGE = 91,
	MT6359_IRQ_FG_CHARGE = 92,
	MT6359_IRQ_BATON_LV = 96,
	MT6359_IRQ_BATON_BAT_IN = 98,
	MT6359_IRQ_BATON_BAT_OUT = 99,
	MT6359_IRQ_BIF = 100,
	MT6359_IRQ_BAT_H = 112,
	MT6359_IRQ_BAT_L = 113,
	MT6359_IRQ_BAT2_H = 114,
	MT6359_IRQ_BAT2_L = 115,
	MT6359_IRQ_BAT_TEMP_H = 116,
	MT6359_IRQ_BAT_TEMP_L = 117,
	MT6359_IRQ_THR_H = 118,
	MT6359_IRQ_THR_L = 119,
	MT6359_IRQ_AUXADC_IMP = 120,
	MT6359_IRQ_NAG_C_DLTV = 121,
	MT6359_IRQ_AUDIO = 128,
	MT6359_IRQ_ACCDET = 133,
	MT6359_IRQ_ACCDET_EINT0 = 134,
	MT6359_IRQ_ACCDET_EINT1 = 135,
	MT6359_IRQ_SPI_CMD_ALERT = 144,
	MT6359_IRQ_NR = 145,
};

#define MT6359_IRQ_BUCK_BASE MT6359_IRQ_VPU_OC
#define MT6359_IRQ_LDO_BASE MT6359_IRQ_VFE28_OC
#define MT6359_IRQ_PSC_BASE MT6359_IRQ_PWRKEY
#define MT6359_IRQ_SCK_BASE MT6359_IRQ_RTC
#define MT6359_IRQ_BM_BASE MT6359_IRQ_FG_BAT_H
#define MT6359_IRQ_HK_BASE MT6359_IRQ_BAT_H
#define MT6359_IRQ_AUD_BASE MT6359_IRQ_AUDIO
#define MT6359_IRQ_MISC_BASE MT6359_IRQ_SPI_CMD_ALERT

#define MT6359_IRQ_BUCK_BITS \
	(MT6359_IRQ_VPA_OC - MT6359_IRQ_BUCK_BASE)
#define MT6359_IRQ_LDO_BITS \
	(MT6359_IRQ_VUFS_OC - MT6359_IRQ_LDO_BASE)
#define MT6359_IRQ_PSC_BITS \
	(MT6359_IRQ_CHRDET_EDGE - MT6359_IRQ_PSC_BASE)
#define MT6359_IRQ_SCK_BITS \
	(MT6359_IRQ_RTC - MT6359_IRQ_SCK_BASE)
#define MT6359_IRQ_BM_BITS \
	(MT6359_IRQ_BIF - MT6359_IRQ_BM_BASE)
#define MT6359_IRQ_HK_BITS \
	(MT6359_IRQ_NAG_C_DLTV - MT6359_IRQ_HK_BASE)
#define MT6359_IRQ_AUD_BITS \
	(MT6359_IRQ_ACCDET_EINT1 - MT6359_IRQ_AUD_BASE)
#define MT6359_IRQ_MISC_BITS \
	(MT6359_IRQ_SPI_CMD_ALERT - MT6359_IRQ_MISC_BASE)

#define MT6359_TOP_GEN(sp)	\
{	\
	.hwirq_base = MT6359_IRQ_##sp##_BASE,	\
	.num_int_regs =	\
		(MT6359_IRQ_##sp##_BITS / MT6359_REG_WIDTH) + 1,	\
	.en_reg = MT6359_##sp##_TOP_INT_CON0,		\
	.en_reg_shift = 0x6,	\
	.sta_reg = MT6359_##sp##_TOP_INT_STATUS0,		\
	.sta_reg_shift = 0x2,	\
	.top_offset = MT6359_##sp##_TOP,	\
}

#define MT6359_IRQ_NAME_GEN()	\
{	\
	[MT6359_IRQ_VPU_OC] = {.name = "vpu_oc"},	\
	[MT6359_IRQ_VCORE_OC] = {.name = "vcore_oc"},	\
	[MT6359_IRQ_VGPU11_OC] = {.name = "vgpu11_oc"},	\
	[MT6359_IRQ_VGPU12_OC] = {.name = "vgpu12_oc"},	\
	[MT6359_IRQ_VMODEM_OC] = {.name = "vmodem_oc"},	\
	[MT6359_IRQ_VPROC1_OC] = {.name = "vproc1_oc"},	\
	[MT6359_IRQ_VPROC2_OC] = {.name = "vproc2_oc"},	\
	[MT6359_IRQ_VS1_OC] = {.name = "vs1_oc"},	\
	[MT6359_IRQ_VS2_OC] = {.name = "vs2_oc"},	\
	[MT6359_IRQ_VPA_OC] = {.name = "vpa_oc"},	\
	[MT6359_IRQ_VFE28_OC] = {.name = "vfe28_oc"},	\
	[MT6359_IRQ_VXO22_OC] = {.name = "vxo22_oc"},	\
	[MT6359_IRQ_VRF18_OC] = {.name = "vrf18_oc"},	\
	[MT6359_IRQ_VRF12_OC] = {.name = "vrf12_oc"},	\
	[MT6359_IRQ_VEFUSE_OC] = {.name = "vefuse_oc"},	\
	[MT6359_IRQ_VCN33_1_OC] = {.name = "vcn33_1_oc"},	\
	[MT6359_IRQ_VCN33_2_OC] = {.name = "vcn33_2_oc"},	\
	[MT6359_IRQ_VCN13_OC] = {.name = "vcn13_oc"},	\
	[MT6359_IRQ_VCN18_OC] = {.name = "vcn18_oc"},	\
	[MT6359_IRQ_VA09_OC] = {.name = "va09_oc"},	\
	[MT6359_IRQ_VCAMIO_OC] = {.name = "vcamio_oc"},	\
	[MT6359_IRQ_VA12_OC] = {.name = "va12_oc"},	\
	[MT6359_IRQ_VAUX18_OC] = {.name = "vaux18_oc"},	\
	[MT6359_IRQ_VAUD18_OC] = {.name = "vaud18_oc"},	\
	[MT6359_IRQ_VIO18_OC] = {.name = "vio18_oc"},	\
	[MT6359_IRQ_VSRAM_PROC1_OC] = {.name = "vsram_proc1_oc"},	\
	[MT6359_IRQ_VSRAM_PROC2_OC] = {.name = "vsram_proc2_oc"},	\
	[MT6359_IRQ_VSRAM_OTHERS_OC] = {.name = "vsram_others_oc"},	\
	[MT6359_IRQ_VSRAM_MD_OC] = {.name = "vsram_md_oc"},	\
	[MT6359_IRQ_VEMC_OC] = {.name = "vemc_oc"},	\
	[MT6359_IRQ_VSIM1_OC] = {.name = "vsim1_oc"},	\
	[MT6359_IRQ_VSIM2_OC] = {.name = "vsim2_oc"},	\
	[MT6359_IRQ_VUSB_OC] = {.name = "vusb_oc"},	\
	[MT6359_IRQ_VRFCK_OC] = {.name = "vrfck_oc"},	\
	[MT6359_IRQ_VBBCK_OC] = {.name = "vbbck_oc"},	\
	[MT6359_IRQ_VBIF28_OC] = {.name = "vbif28_oc"},	\
	[MT6359_IRQ_VIBR_OC] = {.name = "vibr_oc"},	\
	[MT6359_IRQ_VIO28_OC] = {.name = "vio28_oc"},	\
	[MT6359_IRQ_VM18_OC] = {.name = "vm18_oc"},	\
	[MT6359_IRQ_VUFS_OC] = {.name = "vufs_oc"},	\
	[MT6359_IRQ_PWRKEY] = {.name = "pwrkey"},	\
	[MT6359_IRQ_HOMEKEY] = {.name = "homekey"},	\
	[MT6359_IRQ_PWRKEY_R] = {.name = "pwrkey_r"},	\
	[MT6359_IRQ_HOMEKEY_R] = {.name = "homekey_r"},	\
	[MT6359_IRQ_NI_LBAT_INT] = {.name = "ni_lbat_int"},	\
	[MT6359_IRQ_CHRDET_EDGE] = {.name = "chrdet_edge"},	\
	[MT6359_IRQ_RTC] = {.name = "rtc"},	\
	[MT6359_IRQ_FG_BAT_H] = {.name = "fg_bat_h"},	\
	[MT6359_IRQ_FG_BAT_L] = {.name = "fg_bat_l"},	\
	[MT6359_IRQ_FG_CUR_H] = {.name = "fg_cur_h"},	\
	[MT6359_IRQ_FG_CUR_L] = {.name = "fg_cur_l"},	\
	[MT6359_IRQ_FG_ZCV] = {.name = "fg_zcv"},	\
	[MT6359_IRQ_FG_N_CHARGE_L] = {.name = "fg_n_charge_l"},	\
	[MT6359_IRQ_FG_IAVG_H] = {.name = "fg_iavg_h"},	\
	[MT6359_IRQ_FG_IAVG_L] = {.name = "fg_iavg_l"},	\
	[MT6359_IRQ_FG_DISCHARGE] = {.name = "fg_discharge"},	\
	[MT6359_IRQ_FG_CHARGE] = {.name = "fg_charge"},	\
	[MT6359_IRQ_BATON_LV] = {.name = "baton_lv"},	\
	[MT6359_IRQ_BATON_BAT_IN] = {.name = "baton_bat_in"},	\
	[MT6359_IRQ_BATON_BAT_OUT] = {.name = "baton_bat_out"},	\
	[MT6359_IRQ_BIF] = {.name = "bif"},	\
	[MT6359_IRQ_BAT_H] = {.name = "bat_h"},	\
	[MT6359_IRQ_BAT_L] = {.name = "bat_l"},	\
	[MT6359_IRQ_BAT2_H] = {.name = "bat2_h"},	\
	[MT6359_IRQ_BAT2_L] = {.name = "bat2_l"},	\
	[MT6359_IRQ_BAT_TEMP_H] = {.name = "bat_temp_h"},	\
	[MT6359_IRQ_BAT_TEMP_L] = {.name = "bat_temp_l"},	\
	[MT6359_IRQ_THR_H] = {.name = "thr_h"},	\
	[MT6359_IRQ_THR_L] = {.name = "thr_l"},	\
	[MT6359_IRQ_AUXADC_IMP] = {.name = "auxadc_imp"},	\
	[MT6359_IRQ_NAG_C_DLTV] = {.name = "nag_c_dltv"},	\
	[MT6359_IRQ_AUDIO] = {.name = "audio"},	\
	[MT6359_IRQ_ACCDET] = {.name = "accdet"},	\
	[MT6359_IRQ_ACCDET_EINT0] = {.name = "accdet_eint0"},	\
	[MT6359_IRQ_ACCDET_EINT1] = {.name = "accdet_eint1"},	\
	[MT6359_IRQ_SPI_CMD_ALERT] = {.name = "spi_cmd_alert"},	\
}

#endif /* __MFD_MT6359_CORE_H__ */

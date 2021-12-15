/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MFD_MT6358_CORE_H__
#define __MFD_MT6358_CORE_H__

#define MT6358_REG_WIDTH 16

enum mt6358_irq_top_status_shift {
	MT6358_BUCK_TOP = 0,
	MT6358_LDO_TOP,
	MT6358_PSC_TOP,
	MT6358_SCK_TOP,
	MT6358_BM_TOP,
	MT6358_HK_TOP,
	MT6358_AUD_TOP = 7,
	MT6358_MISC_TOP,
};

enum mt6358_irq_numbers {
	MT6358_IRQ_VPROC11_OC = 0,
	MT6358_IRQ_VPROC12_OC,
	MT6358_IRQ_VCORE_OC,
	MT6358_IRQ_VGPU_OC,
	MT6358_IRQ_VMODEM_OC,
	MT6358_IRQ_VDRAM1_OC,
	MT6358_IRQ_VS1_OC,
	MT6358_IRQ_VS2_OC,
	MT6358_IRQ_VPA_OC,
	MT6358_IRQ_VCORE_PREOC,
	MT6358_IRQ_VFE28_OC = 16,
	MT6358_IRQ_VXO22_OC,
	MT6358_IRQ_VRF18_OC,
	MT6358_IRQ_VRF12_OC,
	MT6358_IRQ_VEFUSE_OC,
	MT6358_IRQ_VCN33_OC,
	MT6358_IRQ_VCN28_OC,
	MT6358_IRQ_VCN18_OC,
	MT6358_IRQ_VCAMA1_OC,
	MT6358_IRQ_VCAMA2_OC,
	MT6358_IRQ_VCAMD_OC,
	MT6358_IRQ_VCAMIO_OC,
	MT6358_IRQ_VLDO28_OC,
	MT6358_IRQ_VA12_OC,
	MT6358_IRQ_VAUX18_OC,
	MT6358_IRQ_VAUD28_OC,
	MT6358_IRQ_VIO28_OC,
	MT6358_IRQ_VIO18_OC,
	MT6358_IRQ_VSRAM_PROC11_OC,
	MT6358_IRQ_VSRAM_PROC12_OC,
	MT6358_IRQ_VSRAM_OTHERS_OC,
	MT6358_IRQ_VSRAM_GPU_OC,
	MT6358_IRQ_VDRAM2_OC,
	MT6358_IRQ_VMC_OC,
	MT6358_IRQ_VMCH_OC,
	MT6358_IRQ_VEMC_OC,
	MT6358_IRQ_VSIM1_OC,
	MT6358_IRQ_VSIM2_OC,
	MT6358_IRQ_VIBR_OC,
	MT6358_IRQ_VUSB_OC,
	MT6358_IRQ_VBIF28_OC,
	MT6358_IRQ_PWRKEY = 48,
	MT6358_IRQ_HOMEKEY,
	MT6358_IRQ_PWRKEY_R,
	MT6358_IRQ_HOMEKEY_R,
	MT6358_IRQ_NI_LBAT_INT,
	MT6358_IRQ_CHRDET,
	MT6358_IRQ_CHRDET_EDGE,
	MT6358_IRQ_VCDT_HV_DET,
	MT6358_IRQ_RTC = 64,
	MT6358_IRQ_FG_BAT0_H = 80,
	MT6358_IRQ_FG_BAT0_L,
	MT6358_IRQ_FG_CUR_H,
	MT6358_IRQ_FG_CUR_L,
	MT6358_IRQ_FG_ZCV,
	MT6358_IRQ_FG_BAT1_H,
	MT6358_IRQ_FG_BAT1_L,
	MT6358_IRQ_FG_N_CHARGE_L,
	MT6358_IRQ_FG_IAVG_H,
	MT6358_IRQ_FG_IAVG_L,
	MT6358_IRQ_FG_TIME_H,
	MT6358_IRQ_FG_DISCHARGE,
	MT6358_IRQ_FG_CHARGE,
	MT6358_IRQ_BATON_LV = 96,
	MT6358_IRQ_BATON_HT,
	MT6358_IRQ_BATON_BAT_IN,
	MT6358_IRQ_BATON_BAT_OUT,
	MT6358_IRQ_BIF,
	MT6358_IRQ_BAT_H = 112,
	MT6358_IRQ_BAT_L,
	MT6358_IRQ_BAT2_H,
	MT6358_IRQ_BAT2_L,
	MT6358_IRQ_BAT_TEMP_H,
	MT6358_IRQ_BAT_TEMP_L,
	MT6358_IRQ_AUXADC_IMP,
	MT6358_IRQ_NAG_C_DLTV,
	MT6358_IRQ_AUDIO = 128,
	MT6358_IRQ_ACCDET = 133,
	MT6358_IRQ_ACCDET_EINT0,
	MT6358_IRQ_ACCDET_EINT1,
	MT6358_IRQ_SPI_CMD_ALERT = 144,
	MT6358_IRQ_NR,
};

#define MT6358_IRQ_BUCK_BASE MT6358_IRQ_VPROC11_OC
#define MT6358_IRQ_LDO_BASE MT6358_IRQ_VFE28_OC
#define MT6358_IRQ_PSC_BASE MT6358_IRQ_PWRKEY
#define MT6358_IRQ_SCK_BASE MT6358_IRQ_RTC
#define MT6358_IRQ_BM_BASE MT6358_IRQ_FG_BAT0_H
#define MT6358_IRQ_HK_BASE MT6358_IRQ_BAT_H
#define MT6358_IRQ_AUD_BASE MT6358_IRQ_AUDIO
#define MT6358_IRQ_MISC_BASE MT6358_IRQ_SPI_CMD_ALERT

#define MT6358_IRQ_BUCK_BITS (MT6358_IRQ_VCORE_PREOC - MT6358_IRQ_BUCK_BASE)
#define MT6358_IRQ_LDO_BITS (MT6358_IRQ_VBIF28_OC - MT6358_IRQ_LDO_BASE)
#define MT6358_IRQ_PSC_BITS (MT6358_IRQ_VCDT_HV_DET - MT6358_IRQ_PSC_BASE)
#define MT6358_IRQ_SCK_BITS (MT6358_IRQ_RTC - MT6358_IRQ_SCK_BASE)
#define MT6358_IRQ_BM_BITS (MT6358_IRQ_BIF - MT6358_IRQ_BM_BASE)
#define MT6358_IRQ_HK_BITS (MT6358_IRQ_NAG_C_DLTV - MT6358_IRQ_HK_BASE)
#define MT6358_IRQ_AUD_BITS (MT6358_IRQ_ACCDET_EINT1 - MT6358_IRQ_AUD_BASE)
#define MT6358_IRQ_MISC_BITS	\
	(MT6358_IRQ_SPI_CMD_ALERT - MT6358_IRQ_MISC_BASE)

#define MT6358_TOP_GEN(sp)	\
{	\
	.hwirq_base = MT6358_IRQ_##sp##_BASE,	\
	.num_int_regs =	\
		(MT6358_IRQ_##sp##_BITS / MT6358_REG_WIDTH) + 1,	\
	.en_reg = MT6358_##sp##_TOP_INT_CON0,		\
	.en_reg_shift = 0x6,	\
	.sta_reg = MT6358_##sp##_TOP_INT_STATUS0,		\
	.sta_reg_shift = 0x2,	\
	.top_offset = MT6358_##sp##_TOP,	\
}

#define MT6358_IRQ_NAME_GEN()	\
{	\
	[MT6358_IRQ_VPROC11_OC] = {.name = "vproc11_oc"},	\
	[MT6358_IRQ_VPROC12_OC] = {.name = "vproc12_oc"},	\
	[MT6358_IRQ_VCORE_OC] = {.name = "vcore_oc"},	\
	[MT6358_IRQ_VGPU_OC] = {.name = "vgpu_oc"},	\
	[MT6358_IRQ_VMODEM_OC] = {.name = "vmodem_oc"},	\
	[MT6358_IRQ_VDRAM1_OC] = {.name = "vdram1_oc"},	\
	[MT6358_IRQ_VS1_OC] = {.name = "vs1_oc"},	\
	[MT6358_IRQ_VS2_OC] = {.name = "vs2_oc"},	\
	[MT6358_IRQ_VPA_OC] = {.name = "vpa_oc"},	\
	[MT6358_IRQ_VCORE_PREOC] = {.name = "vcore_preoc"},	\
	[MT6358_IRQ_VFE28_OC] = {.name = "vfe28_oc"},	\
	[MT6358_IRQ_VXO22_OC] = {.name = "vxo22_oc"},	\
	[MT6358_IRQ_VRF18_OC] = {.name = "vrf18_oc"},	\
	[MT6358_IRQ_VRF12_OC] = {.name = "vrf12_oc"},	\
	[MT6358_IRQ_VEFUSE_OC] = {.name = "vefuse_oc"},	\
	[MT6358_IRQ_VCN33_OC] = {.name = "vcn33_oc"},	\
	[MT6358_IRQ_VCN28_OC] = {.name = "vcn28_oc"},	\
	[MT6358_IRQ_VCN18_OC] = {.name = "vcn18_oc"},	\
	[MT6358_IRQ_VCAMA1_OC] = {.name = "vcama1_oc"},	\
	[MT6358_IRQ_VCAMA2_OC] = {.name = "vcama2_oc"},	\
	[MT6358_IRQ_VCAMD_OC] = {.name = "vcamd_oc"},	\
	[MT6358_IRQ_VCAMIO_OC] = {.name = "vcamio_oc"},	\
	[MT6358_IRQ_VLDO28_OC] = {.name = "vldo28_oc"},	\
	[MT6358_IRQ_VA12_OC] = {.name = "va12_oc"},	\
	[MT6358_IRQ_VAUX18_OC] = {.name = "vaux18_oc"},	\
	[MT6358_IRQ_VAUD28_OC] = {.name = "vaud28_oc"},	\
	[MT6358_IRQ_VIO28_OC] = {.name = "vio28_oc"},	\
	[MT6358_IRQ_VIO18_OC] = {.name = "vio18_oc"},	\
	[MT6358_IRQ_VSRAM_PROC11_OC] = {.name = "vsram_proc11_oc"},	\
	[MT6358_IRQ_VSRAM_PROC12_OC] = {.name = "vsram_proc12_oc"},	\
	[MT6358_IRQ_VSRAM_OTHERS_OC] = {.name = "vsram_others_oc"},	\
	[MT6358_IRQ_VSRAM_GPU_OC] = {.name = "vsram_gpu_oc"},	\
	[MT6358_IRQ_VDRAM2_OC] = {.name = "vdram2_oc"},	\
	[MT6358_IRQ_VMC_OC] = {.name = "vmc_oc"},	\
	[MT6358_IRQ_VMCH_OC] = {.name = "vmch_oc"},	\
	[MT6358_IRQ_VEMC_OC] = {.name = "vemc_oc"},	\
	[MT6358_IRQ_VSIM1_OC] = {.name = "vsim1_oc"},	\
	[MT6358_IRQ_VSIM2_OC] = {.name = "vsim2_oc"},	\
	[MT6358_IRQ_VIBR_OC] = {.name = "vibr_oc"},	\
	[MT6358_IRQ_VUSB_OC] = {.name = "vusb_oc"},	\
	[MT6358_IRQ_VBIF28_OC] = {.name = "vbif28_oc"},	\
	[MT6358_IRQ_PWRKEY] = {.name = "pwrkey"},	\
	[MT6358_IRQ_HOMEKEY] = {.name = "homekey"},	\
	[MT6358_IRQ_PWRKEY_R] = {.name = "pwrkey_r"},	\
	[MT6358_IRQ_HOMEKEY_R] = {.name = "homekey_r"},	\
	[MT6358_IRQ_NI_LBAT_INT] = {.name = "ni_lbat_int"},	\
	[MT6358_IRQ_CHRDET] = {.name = "chrdet"},	\
	[MT6358_IRQ_CHRDET_EDGE] = {.name = "chrdet_edge"},	\
	[MT6358_IRQ_VCDT_HV_DET] = {.name = "vcdt_hv_det"},	\
	[MT6358_IRQ_RTC] = {.name = "rtc"},	\
	[MT6358_IRQ_FG_BAT0_H] = {.name = "fg_bat0_h"},	\
	[MT6358_IRQ_FG_BAT0_L] = {.name = "fg_bat0_l"},	\
	[MT6358_IRQ_FG_CUR_H] = {.name = "fg_cur_h"},	\
	[MT6358_IRQ_FG_CUR_L] = {.name = "fg_cur_l"},	\
	[MT6358_IRQ_FG_ZCV] = {.name = "fg_zcv"},	\
	[MT6358_IRQ_FG_BAT1_H] = {.name = "fg_bat1_h"},	\
	[MT6358_IRQ_FG_BAT1_L] = {.name = "fg_bat1_l"},	\
	[MT6358_IRQ_FG_N_CHARGE_L] = {.name = "fg_n_charge_l"},	\
	[MT6358_IRQ_FG_IAVG_H] = {.name = "fg_iavg_h"},	\
	[MT6358_IRQ_FG_IAVG_L] = {.name = "fg_iavg_l"},	\
	[MT6358_IRQ_FG_TIME_H] = {.name = "fg_time_h"},	\
	[MT6358_IRQ_FG_DISCHARGE] = {.name = "fg_discharge"},	\
	[MT6358_IRQ_FG_CHARGE] = {.name = "fg_charge"},	\
	[MT6358_IRQ_BATON_LV] = {.name = "baton_lv"},	\
	[MT6358_IRQ_BATON_HT] = {.name = "baton_ht"},	\
	[MT6358_IRQ_BATON_BAT_IN] = {.name = "baton_bat_in"},	\
	[MT6358_IRQ_BATON_BAT_OUT] = {.name = "baton_bat_out"},	\
	[MT6358_IRQ_BIF] = {.name = "bif"},	\
	[MT6358_IRQ_BAT_H] = {.name = "bat_h"},	\
	[MT6358_IRQ_BAT_L] = {.name = "bat_l"},	\
	[MT6358_IRQ_BAT2_H] = {.name = "bat2_h"},	\
	[MT6358_IRQ_BAT2_L] = {.name = "bat2_l"},	\
	[MT6358_IRQ_BAT_TEMP_H] = {.name = "bat_temp_h"},	\
	[MT6358_IRQ_BAT_TEMP_L] = {.name = "bat_temp_l"},	\
	[MT6358_IRQ_AUXADC_IMP] = {.name = "auxadc_imp"},	\
	[MT6358_IRQ_NAG_C_DLTV] = {.name = "nag_c_dltv"},	\
	[MT6358_IRQ_AUDIO] = {.name = "audio"},	\
	[MT6358_IRQ_ACCDET] = {.name = "accdet"},	\
	[MT6358_IRQ_ACCDET_EINT0] = {.name = "accdet_eint0"},	\
	[MT6358_IRQ_ACCDET_EINT1] = {.name = "accdet_eint1"},	\
	[MT6358_IRQ_SPI_CMD_ALERT] = {.name = "spi_cmd_alert"},	\
}

struct mt6358_chip {
	struct device *dev;
	struct regmap *regmap;
	int irq;
	struct irq_domain *irq_domain;
	struct mutex irqlock;
	unsigned int num_sps;
	unsigned int num_pmic_irqs;
	unsigned short top_int_status_reg;
};

extern unsigned int mt6358_irq_get_virq(struct device *dev, unsigned int hwirq);
extern const char *mt6358_irq_get_name(struct device *dev, unsigned int hwirq);

#endif /* __MFD_MT6358_CORE_H__ */

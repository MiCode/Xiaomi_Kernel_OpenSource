/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __MFD_MT6363_REGISTERS_H__
#define __MFD_MT6363_REGISTERS_H__

/* PMIC Registers */
#define MT6363_TOPSTATUS		   (0x1e)
#define MT6363_MISC_TOP_INT_CON0           (0x37)
#define MT6363_MISC_TOP_INT_STATUS0        (0x43)
#define MT6363_TOP_INT_STATUS1             (0x4e)
#define MT6363_PSC_TOP_INT_CON0            (0x90f)
#define MT6363_PSC_TOP_INT_STATUS0         (0x91b)
#define MT6363_STRUP_CON11		   (0xa0e)
#define MT6363_STRUP_CON12		   (0xa0f)
#define MT6363_PCHR_VREF_ANA_CON2          (0xa8a)
#define MT6363_BM_TOP_INT_CON0             (0xc24)
#define MT6363_BM_TOP_INT_STATUS0          (0xc36)
#define MT6363_HK_TOP_INT_CON0             (0xf92)
#define MT6363_HK_TOP_INT_STATUS0          (0xf9e)
#define MT6363_BUCK_TOP_INT_CON0           (0x1411)
#define MT6363_BUCK_TOP_INT_STATUS0        (0x141d)
#define MT6363_LDO_TOP_INT_CON0            (0x1b11)
#define MT6363_LDO_TOP_INT_STATUS0         (0x1b29)

#define MT6363_CHRDET_DEB_ADDR             MT6363_TOPSTATUS
#define MT6363_CHRDET_DEB_MASK             (0x1)
#define MT6363_CHRDET_DEB_SHIFT            (2)
#define MT6363_RG_VSYS_UVLO_VTHL_ADDR      MT6363_PCHR_VREF_ANA_CON2
#define MT6363_RG_VSYS_UVLO_VTHL_MASK      (0xF)
#define MT6363_RG_VSYS_UVLO_VTHL_SHIFT     (0)

#define MT6363_AUXADC_ADC0_L               (0x1088)
#define MT6363_AUXADC_ADC3_L               (0x108e)
#define MT6363_AUXADC_ADC4_L               (0x1090)
#define MT6363_AUXADC_ADC11_L              (0x109e)
#define MT6363_AUXADC_ADC38_L              (0x10c4)
#define MT6363_AUXADC_ADC39_L              (0x10c6)
#define MT6363_AUXADC_ADC40_L              (0x10c8)
#define MT6363_AUXADC_ADC_CH12_L           (0x10d2)
#define MT6363_AUXADC_ADC_CH14_L           (0x10d8)
#define MT6363_AUXADC_ADC42_L              (0x10dc)
#define MT6363_AUXADC_RQST0                (0x1108)
#define MT6363_AUXADC_RQST1                (0x1109)
#define MT6363_AUXADC_RQST3                (0x110c)
#define MT6363_SDMADC_RQST0                (0x110e)
#define MT6363_SDMADC_CON0                 (0x11c4)
#define MT6363_AUXADC_IMP0                 (0x1208)
#define MT6363_AUXADC_IMP1                 (0x1209)

#endif /* __MFD_MT6363_REGISTERS_H__ */


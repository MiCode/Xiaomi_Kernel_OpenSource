/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */


#ifndef __MFD_MT6377_REGISTERS_H__
#define __MFD_MT6377_REGISTERS_H__

/* PMIC Registers */
#define MT6377_TOPSTATUS		   (0x22)
#define MT6377_MISC_TOP_INT_CON0           (0x3e)
#define MT6377_MISC_TOP_INT_STATUS0        (0x4a)
#define MT6377_TOP_INT_STATUS1             (0x55)
#define MT6377_SCK_TOP_INT_STATUS0         (0x534)
#define MT6377_SCK_TOP_INT_CON0            (0x528)
#define MT6377_PSC_TOP_INT_CON0            (0x90f)
#define MT6377_PSC_TOP_INT_STATUS0         (0x915)
#define MT6377_BM_TOP_INT_CON0             (0xc24)
#define MT6377_BM_TOP_INT_STATUS0          (0xc36)
#define MT6377_HK_TOP_INT_CON0             (0xf92)
#define MT6377_HK_TOP_INT_STATUS0          (0xf9e)
#define MT6377_BUCK_TOP_INT_CON0           (0x1411)
#define MT6377_BUCK_TOP_INT_STATUS0        (0x1419)
#define MT6377_LDO_TOP_INT_CON0            (0x1b11)
#define MT6377_LDO_TOP_INT_STATUS0         (0x1b2f)
#define MT6377_AUD_TOP_INT_CON0            (0x231d)
#define MT6377_AUD_TOP_INT_STATUS0         (0x2323)
#define MT6377_STRUP_CON12		   (0xa10)
#define MT6377_PCHR_VREF_ANA_CON1          (0xa89)

#define MT6377_CHRDET_DEB_ADDR             MT6377_TOPSTATUS
#define MT6377_CHRDET_DEB_MASK             (0x1)
#define MT6377_CHRDET_DEB_SHIFT            (2)
#define MT6377_RG_VSYS_UVLO_VTHL_ADDR      MT6377_PCHR_VREF_ANA_CON1
#define MT6377_RG_VSYS_UVLO_VTHL_MASK      (0x1F)
#define MT6377_RG_VSYS_UVLO_VTHL_SHIFT     (2)

#define MT6377_FGADC_CUR_CON1_L            (0xd8b)
#define MT6377_FGADC_CUR_CON2_L            (0xd8d)
#define MT6377_AUXADC_ADC0_L               (0x1088)
#define MT6377_AUXADC_ADC2_L               (0x108c)
#define MT6377_AUXADC_ADC3_L               (0x108e)
#define MT6377_AUXADC_ADC4_L               (0x1090)
#define MT6377_AUXADC_ADC5_L               (0x1092)
#define MT6377_AUXADC_ADC9_L               (0x109a)
#define MT6377_AUXADC_ADC11_L              (0x109e)
#define MT6377_AUXADC_ADC32_L              (0x10c2)
#define MT6377_AUXADC_ADC33_L              (0x10c4)
#define MT6377_AUXADC_ADC34_L              (0x10c6)
#define MT6377_AUXADC_ADC_AUTO1_L          (0x10d2)
#define MT6377_AUXADC_ADC_AUTO3_L          (0x10d6)
#define MT6377_AUXADC_RQST0                (0x1108)
#define MT6377_AUXADC_RQST1                (0x110a)
#define MT6377_AUXADC_RQST3                (0x110d)
#define MT6377_AUXADC_IMP0                 (0x1208)
#define MT6377_AUXADC_IMP1                 (0x1209)
#define MT6377_AUXADC_LBAT0                (0x120d)
#define MT6377_AUXADC_LBAT1                (0x120e)
#define MT6377_AUXADC_LBAT2                (0x120f)
#define MT6377_AUXADC_LBAT3                (0x1210)
#define MT6377_AUXADC_LBAT5                (0x1212)
#define MT6377_AUXADC_LBAT6                (0x1213)

#endif /* __MFD_MT6377_REGISTERS_H__ */


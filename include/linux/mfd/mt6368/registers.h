/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __MFD_MT6368_REGISTERS_H__
#define __MFD_MT6368_REGISTERS_H__

/* PMIC Registers */
#define MT6368_MISC_TOP_INT_CON0             0x3b
#define MT6368_MISC_TOP_INT_CON1             0x3e
#define MT6368_MISC_TOP_INT_STATUS0          0x47
#define MT6368_MISC_TOP_INT_STATUS1          0x48
#define MT6368_TOP_INT_MASK_CON0             0x4b
#define MT6368_TOP_INT_MASK_CON0_SET         0x4c
#define MT6368_TOP_INT_MASK_CON0_CLR         0x4d
#define MT6368_TOP_INT_MASK_CON1             0x4e
#define MT6368_TOP_INT_MASK_CON1_SET         0x4f
#define MT6368_TOP_INT_MASK_CON1_CLR         0x50
#define MT6368_TOP_INT_STATUS0               0x51
#define MT6368_TOP_INT_STATUS1               0x52
#define MT6368_HK_TOP_INT_CON0               0xf92
#define MT6368_HK_TOP_INT_CON1               0xf95
#define MT6368_HK_TOP_INT_STATUS0            0xf9e
#define MT6368_HK_TOP_INT_STATUS1            0xf9f
#define MT6368_AUXADC_ADC4_L                 0x1090
#define MT6368_AUXADC_ADC5_L                 0x1092
#define MT6368_AUXADC_ADC9_L                 0x109a
#define MT6368_AUXADC_ADC38_L                0x10c4
#define MT6368_AUXADC_ADC39_L                0x10c6
#define MT6368_AUXADC_ADC40_L                0x10c8
#define MT6368_AUXADC_ADC_CH12_L             0x10d2
#define MT6368_AUXADC_ADC_CH12_H             0x10d3
#define MT6368_AUXADC_RQST0                  0x1108
#define MT6368_AUXADC_RQST1                  0x1109
#define MT6368_AUXADC_RQST3                  0x110c
#define MT6368_SDMADC_RQST0                  0x110e
#define MT6368_SDMADC_CON0                   0x11c4
#define MT6368_BUCK_TOP_INT_CON0             0x1411
#define MT6368_BUCK_TOP_INT_STATUS0          0x1417
#define MT6368_LDO_TOP_INT_CON0              0x1b12
#define MT6368_LDO_TOP_INT_CON1              0x1b15
#define MT6368_LDO_TOP_INT_CON2              0x1b18
#define MT6368_LDO_TOP_INT_STATUS0           0x1b24
#define MT6368_LDO_TOP_INT_STATUS1           0x1b25
#define MT6368_LDO_TOP_INT_STATUS2           0x1b26
#define MT6368_LDO_VMCH_CON0                 0x1c95
#define MT6368_LDO_VMCH_CON1                 0x1c96
#define MT6368_LDO_VMCH_CON2                 0x1c97
#define MT6368_AUD_TOP_INT_CON0              0x231d
#define MT6368_AUD_TOP_INT_STATUS0           0x2323

#endif /* __MFD_MT6368_REGISTERS_H__ */

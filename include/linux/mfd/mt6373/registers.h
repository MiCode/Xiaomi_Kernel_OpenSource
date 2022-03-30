/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __MFD_MT6373_REGISTERS_H__
#define __MFD_MT6373_REGISTERS_H__

/* PMIC Registers */
#define MT6373_TOPSTATUS                       0x1e
#define MT6373_MISC_TOP_INT_CON0               0x3c
#define MT6373_MISC_TOP_INT_CON1               0x3f
#define MT6373_MISC_TOP_INT_STATUS0            0x48
#define MT6373_MISC_TOP_INT_STATUS1            0x49
#define MT6373_TOP_INT_MASK_CON0               0x4c
#define MT6373_TOP_INT_MASK_CON0_SET           0x4d
#define MT6373_TOP_INT_MASK_CON0_CLR           0x4e
#define MT6373_TOP_INT_MASK_CON1               0x4f
#define MT6373_TOP_INT_MASK_CON1_SET           0x50
#define MT6373_TOP_INT_MASK_CON1_CLR           0x51
#define MT6373_TOP_INT_STATUS0                 0x52
#define MT6373_TOP_INT_STATUS1                 0x53
#define MT6373_HK_TOP_INT_CON0                 0xf92
#define MT6373_HK_TOP_INT_CON1                 0xf95
#define MT6373_HK_TOP_INT_STATUS0              0xf9e
#define MT6373_HK_TOP_INT_STATUS1              0xf9f
#define MT6373_AUXADC_ADC4_L                   0x1090
#define MT6373_AUXADC_ADC38_L                  0x10c4
#define MT6373_AUXADC_ADC39_L                  0x10c6
#define MT6373_AUXADC_ADC40_L                  0x10c8
#define MT6373_AUXADC_ADC_CH12_L               0x10d2
#define MT6373_AUXADC_ADC_CH12_H               0x10d3
#define MT6373_AUXADC_RQST0                    0x1108
#define MT6373_AUXADC_RQST1                    0x1109
#define MT6373_AUXADC_RQST3                    0x110c
#define MT6373_SDMADC_RQST0                    0x110e
#define MT6373_SDMADC_CON0                     0x11c4
#define MT6373_BUCK_TOP_INT_CON0               0x1411
#define MT6373_BUCK_TOP_INT_CON1               0x1414
#define MT6373_BUCK_TOP_INT_STATUS0            0x141d
#define MT6373_BUCK_TOP_INT_STATUS1            0x141e
#define MT6373_LDO_TOP_INT_CON0                0x1b10
#define MT6373_LDO_TOP_INT_CON1                0x1b13
#define MT6373_LDO_TOP_INT_CON2                0x1b16
#define MT6373_LDO_TOP_INT_STATUS0             0x1b22
#define MT6373_LDO_TOP_INT_STATUS1             0x1b23
#define MT6373_LDO_TOP_INT_STATUS2             0x1b24
#define MT6373_LDO_VMCH_CON0                   0x1cb1
#define MT6373_LDO_VMCH_CON1                   0x1cb2
#define MT6373_LDO_VMCH_CON2                   0x1cb3

#endif /* __MFD_MT6373_REGISTERS_H__ */

/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_PMIC_6337_UPMU_HW_H_
#define _MT_PMIC_6337_UPMU_HW_H_

#define MT6337_PMIC_REG_BASE (0x8000)

#define MT6337_HWCID                 ((unsigned int)(MT6337_PMIC_REG_BASE+0x0000))
#define MT6337_SWCID                 ((unsigned int)(MT6337_PMIC_REG_BASE+0x0002))
#define MT6337_ANACID                ((unsigned int)(MT6337_PMIC_REG_BASE+0x0004))
#define MT6337_TOP_CON               ((unsigned int)(MT6337_PMIC_REG_BASE+0x0006))
#define MT6337_TEST_OUT              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0008))
#define MT6337_TEST_CON0             ((unsigned int)(MT6337_PMIC_REG_BASE+0x000A))
#define MT6337_TEST_CON1             ((unsigned int)(MT6337_PMIC_REG_BASE+0x000C))
#define MT6337_TESTMODE_SW           ((unsigned int)(MT6337_PMIC_REG_BASE+0x000E))
#define MT6337_TOPSTATUS0            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0010))
#define MT6337_TOPSTATUS1            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0012))
#define MT6337_TDSEL_CON             ((unsigned int)(MT6337_PMIC_REG_BASE+0x0014))
#define MT6337_RDSEL_CON             ((unsigned int)(MT6337_PMIC_REG_BASE+0x0016))
#define MT6337_SMT_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0018))
#define MT6337_SMT_CON1              ((unsigned int)(MT6337_PMIC_REG_BASE+0x001A))
#define MT6337_SMT_CON2              ((unsigned int)(MT6337_PMIC_REG_BASE+0x001C))
#define MT6337_DRV_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x001E))
#define MT6337_DRV_CON1              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0020))
#define MT6337_DRV_CON2              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0022))
#define MT6337_DRV_CON3              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0024))
#define MT6337_FILTER_CON0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0026))
#define MT6337_FILTER_CON1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0028))
#define MT6337_DLY_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x002A))
#define MT6337_TOP_STATUS            ((unsigned int)(MT6337_PMIC_REG_BASE+0x002C))
#define MT6337_TOP_STATUS_SET        ((unsigned int)(MT6337_PMIC_REG_BASE+0x002E))
#define MT6337_TOP_STATUS_CLR        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0030))
#define MT6337_TOP_SPI_CON0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0032))
#define MT6337_TOP_CKPDN_CON0        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0200))
#define MT6337_TOP_CKPDN_CON0_SET    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0202))
#define MT6337_TOP_CKPDN_CON0_CLR    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0204))
#define MT6337_TOP_CKPDN_CON1        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0206))
#define MT6337_TOP_CKPDN_CON1_SET    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0208))
#define MT6337_TOP_CKPDN_CON1_CLR    ((unsigned int)(MT6337_PMIC_REG_BASE+0x020A))
#define MT6337_TOP_CKSEL_CON0        ((unsigned int)(MT6337_PMIC_REG_BASE+0x020C))
#define MT6337_TOP_CKSEL_CON0_SET    ((unsigned int)(MT6337_PMIC_REG_BASE+0x020E))
#define MT6337_TOP_CKSEL_CON0_CLR    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0210))
#define MT6337_TOP_CKDIVSEL_CON0     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0212))
#define MT6337_TOP_CKDIVSEL_CON0_SET ((unsigned int)(MT6337_PMIC_REG_BASE+0x0214))
#define MT6337_TOP_CKDIVSEL_CON0_CLR ((unsigned int)(MT6337_PMIC_REG_BASE+0x0216))
#define MT6337_TOP_CKHWEN_CON0       ((unsigned int)(MT6337_PMIC_REG_BASE+0x0218))
#define MT6337_TOP_CKHWEN_CON0_SET   ((unsigned int)(MT6337_PMIC_REG_BASE+0x021A))
#define MT6337_TOP_CKHWEN_CON0_CLR   ((unsigned int)(MT6337_PMIC_REG_BASE+0x021C))
#define MT6337_TOP_CKHWEN_CON1       ((unsigned int)(MT6337_PMIC_REG_BASE+0x021E))
#define MT6337_TOP_CKTST_CON0        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0220))
#define MT6337_TOP_CKTST_CON1        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0222))
#define MT6337_TOP_CLKSQ             ((unsigned int)(MT6337_PMIC_REG_BASE+0x0224))
#define MT6337_TOP_CLKSQ_SET         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0226))
#define MT6337_TOP_CLKSQ_CLR         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0228))
#define MT6337_TOP_CLKSQ_RTC         ((unsigned int)(MT6337_PMIC_REG_BASE+0x022A))
#define MT6337_TOP_CLKSQ_RTC_SET     ((unsigned int)(MT6337_PMIC_REG_BASE+0x022C))
#define MT6337_TOP_CLKSQ_RTC_CLR     ((unsigned int)(MT6337_PMIC_REG_BASE+0x022E))
#define MT6337_TOP_CLK_TRIM          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0230))
#define MT6337_TOP_CLK_CON0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0232))
#define MT6337_TOP_CLK_CON0_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0234))
#define MT6337_TOP_CLK_CON0_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0236))
#define MT6337_BUCK_ALL_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0238))
#define MT6337_BUCK_ALL_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x023A))
#define MT6337_BUCK_ALL_CON2         ((unsigned int)(MT6337_PMIC_REG_BASE+0x023C))
#define MT6337_BUCK_ALL_CON3         ((unsigned int)(MT6337_PMIC_REG_BASE+0x023E))
#define MT6337_BUCK_ALL_CON4         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0240))
#define MT6337_BUCK_ALL_CON5         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0242))
#define MT6337_TOP_RST_CON0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0400))
#define MT6337_TOP_RST_CON0_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0402))
#define MT6337_TOP_RST_CON0_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0404))
#define MT6337_TOP_RST_CON1          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0406))
#define MT6337_TOP_RST_CON1_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0408))
#define MT6337_TOP_RST_CON1_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x040A))
#define MT6337_TOP_RST_CON2          ((unsigned int)(MT6337_PMIC_REG_BASE+0x040C))
#define MT6337_TOP_RST_MISC          ((unsigned int)(MT6337_PMIC_REG_BASE+0x040E))
#define MT6337_TOP_RST_MISC_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0410))
#define MT6337_TOP_RST_MISC_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0412))
#define MT6337_TOP_RST_STATUS        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0414))
#define MT6337_TOP_RST_STATUS_SET    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0416))
#define MT6337_TOP_RST_STATUS_CLR    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0418))
#define MT6337_INT_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0600))
#define MT6337_INT_CON0_SET          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0602))
#define MT6337_INT_CON0_CLR          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0604))
#define MT6337_INT_CON1              ((unsigned int)(MT6337_PMIC_REG_BASE+0x0606))
#define MT6337_INT_CON1_SET          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0608))
#define MT6337_INT_CON1_CLR          ((unsigned int)(MT6337_PMIC_REG_BASE+0x060A))
#define MT6337_INT_CON2              ((unsigned int)(MT6337_PMIC_REG_BASE+0x060C))
#define MT6337_INT_CON2_SET          ((unsigned int)(MT6337_PMIC_REG_BASE+0x060E))
#define MT6337_INT_CON2_CLR          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0610))
#define MT6337_INT_MISC_CON          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0612))
#define MT6337_INT_MISC_CON_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0614))
#define MT6337_INT_MISC_CON_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0616))
#define MT6337_INT_STATUS0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0618))
#define MT6337_STRUP_CON0            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0800))
#define MT6337_STRUP_CON1            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0802))
#define MT6337_STRUP_CON4            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0804))
#define MT6337_STRUP_CON5            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0806))
#define MT6337_TSBG_ANA_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0808))
#define MT6337_TSBG_ANA_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x080A))
#define MT6337_IVGEN_ANA_CON0        ((unsigned int)(MT6337_PMIC_REG_BASE+0x080C))
#define MT6337_IVGEN_ANA_CON1        ((unsigned int)(MT6337_PMIC_REG_BASE+0x080E))
#define MT6337_STRUP_ANA_CON0        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0810))
#define MT6337_STRUP_ANA_CON1        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0812))
#define MT6337_RG_SPI_CON            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A00))
#define MT6337_DEW_DIO_EN            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A02))
#define MT6337_DEW_READ_TEST         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A04))
#define MT6337_DEW_WRITE_TEST        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A06))
#define MT6337_DEW_CRC_SWRST         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A08))
#define MT6337_DEW_CRC_EN            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A0A))
#define MT6337_DEW_CRC_VAL           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A0C))
#define MT6337_DEW_DBG_MON_SEL       ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A0E))
#define MT6337_DEW_CIPHER_KEY_SEL    ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A10))
#define MT6337_DEW_CIPHER_IV_SEL     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A12))
#define MT6337_DEW_CIPHER_EN         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A14))
#define MT6337_DEW_CIPHER_RDY        ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A16))
#define MT6337_DEW_CIPHER_MODE       ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A18))
#define MT6337_DEW_CIPHER_SWRST      ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A1A))
#define MT6337_DEW_RDDMY_NO          ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A1C))
#define MT6337_INT_TYPE_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A1E))
#define MT6337_INT_TYPE_CON0_SET     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A20))
#define MT6337_INT_TYPE_CON0_CLR     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A22))
#define MT6337_INT_TYPE_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A24))
#define MT6337_INT_TYPE_CON1_SET     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A26))
#define MT6337_INT_TYPE_CON1_CLR     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A28))
#define MT6337_INT_TYPE_CON2         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A2A))
#define MT6337_INT_TYPE_CON2_SET     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A2C))
#define MT6337_INT_TYPE_CON2_CLR     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A2E))
#define MT6337_INT_TYPE_CON3         ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A30))
#define MT6337_INT_TYPE_CON3_SET     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A32))
#define MT6337_INT_TYPE_CON3_CLR     ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A34))
#define MT6337_INT_STA               ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A36))
#define MT6337_RG_SPI_CON1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A38))
#define MT6337_RG_SPI_CON2           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0A3A))
#define MT6337_FQMTR_CON0            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0C00))
#define MT6337_FQMTR_CON1            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0C02))
#define MT6337_FQMTR_CON2            ((unsigned int)(MT6337_PMIC_REG_BASE+0x0C04))
#define MT6337_BUCK_K_CON0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0E00))
#define MT6337_BUCK_K_CON1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0E02))
#define MT6337_BUCK_K_CON2           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0E04))
#define MT6337_BUCK_K_CON3           ((unsigned int)(MT6337_PMIC_REG_BASE+0x0E06))
#define MT6337_LDO_RSV_CON0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1000))
#define MT6337_LDO_RSV_CON1          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1002))
#define MT6337_LDO_OCFB0             ((unsigned int)(MT6337_PMIC_REG_BASE+0x1004))
#define MT6337_LDO_VA18_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1006))
#define MT6337_LDO_VA18_OP_EN        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1008))
#define MT6337_LDO_VA18_OP_EN_SET    ((unsigned int)(MT6337_PMIC_REG_BASE+0x100A))
#define MT6337_LDO_VA18_OP_EN_CLR    ((unsigned int)(MT6337_PMIC_REG_BASE+0x100C))
#define MT6337_LDO_VA18_OP_CFG       ((unsigned int)(MT6337_PMIC_REG_BASE+0x100E))
#define MT6337_LDO_VA18_OP_CFG_SET   ((unsigned int)(MT6337_PMIC_REG_BASE+0x1010))
#define MT6337_LDO_VA18_OP_CFG_CLR   ((unsigned int)(MT6337_PMIC_REG_BASE+0x1012))
#define MT6337_LDO_VA18_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1014))
#define MT6337_LDO_VA18_CON2         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1016))
#define MT6337_LDO_VA18_CON3         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1018))
#define MT6337_LDO_VA25_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x101A))
#define MT6337_LDO_VA25_OP_EN        ((unsigned int)(MT6337_PMIC_REG_BASE+0x101C))
#define MT6337_LDO_VA25_OP_EN_SET    ((unsigned int)(MT6337_PMIC_REG_BASE+0x101E))
#define MT6337_LDO_VA25_OP_EN_CLR    ((unsigned int)(MT6337_PMIC_REG_BASE+0x1020))
#define MT6337_LDO_VA25_OP_CFG       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1022))
#define MT6337_LDO_VA25_OP_CFG_SET   ((unsigned int)(MT6337_PMIC_REG_BASE+0x1024))
#define MT6337_LDO_VA25_OP_CFG_CLR   ((unsigned int)(MT6337_PMIC_REG_BASE+0x1026))
#define MT6337_LDO_VA25_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1028))
#define MT6337_LDO_VA25_CON2         ((unsigned int)(MT6337_PMIC_REG_BASE+0x102A))
#define MT6337_LDO_DCM               ((unsigned int)(MT6337_PMIC_REG_BASE+0x102C))
#define MT6337_LDO_VA18_CG0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x102E))
#define MT6337_LDO_VA25_CG0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1030))
#define MT6337_VA25_ANA_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1032))
#define MT6337_VA25_ANA_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1034))
#define MT6337_VA18_ANA_CON0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1036))
#define MT6337_VA18_ANA_CON1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1038))
#define MT6337_OTP_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1200))
#define MT6337_OTP_CON1              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1202))
#define MT6337_OTP_CON2              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1204))
#define MT6337_OTP_CON3              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1206))
#define MT6337_OTP_CON4              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1208))
#define MT6337_OTP_CON5              ((unsigned int)(MT6337_PMIC_REG_BASE+0x120A))
#define MT6337_OTP_CON6              ((unsigned int)(MT6337_PMIC_REG_BASE+0x120C))
#define MT6337_OTP_CON7              ((unsigned int)(MT6337_PMIC_REG_BASE+0x120E))
#define MT6337_OTP_CON8              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1210))
#define MT6337_OTP_CON9              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1212))
#define MT6337_OTP_CON10             ((unsigned int)(MT6337_PMIC_REG_BASE+0x1214))
#define MT6337_OTP_CON11             ((unsigned int)(MT6337_PMIC_REG_BASE+0x1216))
#define MT6337_OTP_CON13             ((unsigned int)(MT6337_PMIC_REG_BASE+0x1218))
#define MT6337_OTP_DOUT_0_15         ((unsigned int)(MT6337_PMIC_REG_BASE+0x121A))
#define MT6337_OTP_DOUT_16_31        ((unsigned int)(MT6337_PMIC_REG_BASE+0x121C))
#define MT6337_OTP_DOUT_32_47        ((unsigned int)(MT6337_PMIC_REG_BASE+0x121E))
#define MT6337_OTP_DOUT_48_63        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1220))
#define MT6337_OTP_DOUT_64_79        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1222))
#define MT6337_OTP_DOUT_80_95        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1224))
#define MT6337_OTP_DOUT_96_111       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1226))
#define MT6337_OTP_DOUT_112_127      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1228))
#define MT6337_OTP_DOUT_128_143      ((unsigned int)(MT6337_PMIC_REG_BASE+0x122A))
#define MT6337_OTP_DOUT_144_159      ((unsigned int)(MT6337_PMIC_REG_BASE+0x122C))
#define MT6337_OTP_DOUT_160_175      ((unsigned int)(MT6337_PMIC_REG_BASE+0x122E))
#define MT6337_OTP_DOUT_176_191      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1230))
#define MT6337_OTP_DOUT_192_207      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1232))
#define MT6337_OTP_DOUT_208_223      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1234))
#define MT6337_OTP_DOUT_224_239      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1236))
#define MT6337_OTP_DOUT_240_255      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1238))
#define MT6337_OTP_DOUT_256_271      ((unsigned int)(MT6337_PMIC_REG_BASE+0x123A))
#define MT6337_OTP_DOUT_272_287      ((unsigned int)(MT6337_PMIC_REG_BASE+0x123C))
#define MT6337_OTP_DOUT_288_303      ((unsigned int)(MT6337_PMIC_REG_BASE+0x123E))
#define MT6337_OTP_DOUT_304_319      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1240))
#define MT6337_OTP_DOUT_320_335      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1242))
#define MT6337_OTP_DOUT_336_351      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1244))
#define MT6337_OTP_DOUT_352_367      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1246))
#define MT6337_OTP_DOUT_368_383      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1248))
#define MT6337_OTP_DOUT_384_399      ((unsigned int)(MT6337_PMIC_REG_BASE+0x124A))
#define MT6337_OTP_DOUT_400_415      ((unsigned int)(MT6337_PMIC_REG_BASE+0x124C))
#define MT6337_OTP_DOUT_416_431      ((unsigned int)(MT6337_PMIC_REG_BASE+0x124E))
#define MT6337_OTP_DOUT_432_447      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1250))
#define MT6337_OTP_DOUT_448_463      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1252))
#define MT6337_OTP_DOUT_464_479      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1254))
#define MT6337_OTP_DOUT_480_495      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1256))
#define MT6337_OTP_DOUT_496_511      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1258))
#define MT6337_OTP_VAL_0_15          ((unsigned int)(MT6337_PMIC_REG_BASE+0x125A))
#define MT6337_OTP_VAL_16_31         ((unsigned int)(MT6337_PMIC_REG_BASE+0x125C))
#define MT6337_OTP_VAL_32_47         ((unsigned int)(MT6337_PMIC_REG_BASE+0x125E))
#define MT6337_OTP_VAL_48_63         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1260))
#define MT6337_OTP_VAL_64_79         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1262))
#define MT6337_OTP_VAL_80_95         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1264))
#define MT6337_OTP_VAL_208_223       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1274))
#define MT6337_OTP_VAL_224_239       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1276))
#define MT6337_OTP_VAL_240_255       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1278))
#define MT6337_OTP_VAL_496_511       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1298))
#define MT6337_AUXADC_ADC0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1400))
#define MT6337_AUXADC_ADC1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1402))
#define MT6337_AUXADC_ADC2           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1404))
#define MT6337_AUXADC_ADC3           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1406))
#define MT6337_AUXADC_ADC4           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1408))
#define MT6337_AUXADC_ADC5           ((unsigned int)(MT6337_PMIC_REG_BASE+0x140A))
#define MT6337_AUXADC_ADC6           ((unsigned int)(MT6337_PMIC_REG_BASE+0x140C))
#define MT6337_AUXADC_ADC7           ((unsigned int)(MT6337_PMIC_REG_BASE+0x140E))
#define MT6337_AUXADC_ADC8           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1410))
#define MT6337_AUXADC_ADC9           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1412))
#define MT6337_AUXADC_ADC10          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1414))
#define MT6337_AUXADC_ADC11          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1416))
#define MT6337_AUXADC_ADC12          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1418))
#define MT6337_AUXADC_ADC13          ((unsigned int)(MT6337_PMIC_REG_BASE+0x141A))
#define MT6337_AUXADC_ADC14          ((unsigned int)(MT6337_PMIC_REG_BASE+0x141C))
#define MT6337_AUXADC_ADC15          ((unsigned int)(MT6337_PMIC_REG_BASE+0x141E))
#define MT6337_AUXADC_ADC16          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1420))
#define MT6337_AUXADC_ADC17          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1422))
#define MT6337_AUXADC_STA0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1424))
#define MT6337_AUXADC_STA1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1426))
#define MT6337_AUXADC_RQST0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1428))
#define MT6337_AUXADC_RQST0_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x142A))
#define MT6337_AUXADC_RQST0_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x142C))
#define MT6337_AUXADC_RQST1          ((unsigned int)(MT6337_PMIC_REG_BASE+0x142E))
#define MT6337_AUXADC_RQST1_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1430))
#define MT6337_AUXADC_RQST1_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1432))
#define MT6337_AUXADC_CON0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1434))
#define MT6337_AUXADC_CON0_SET       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1436))
#define MT6337_AUXADC_CON0_CLR       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1438))
#define MT6337_AUXADC_CON1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x143A))
#define MT6337_AUXADC_CON2           ((unsigned int)(MT6337_PMIC_REG_BASE+0x143C))
#define MT6337_AUXADC_CON3           ((unsigned int)(MT6337_PMIC_REG_BASE+0x143E))
#define MT6337_AUXADC_CON4           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1440))
#define MT6337_AUXADC_CON5           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1442))
#define MT6337_AUXADC_CON6           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1444))
#define MT6337_AUXADC_CON7           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1446))
#define MT6337_AUXADC_CON8           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1448))
#define MT6337_AUXADC_CON9           ((unsigned int)(MT6337_PMIC_REG_BASE+0x144A))
#define MT6337_AUXADC_CON10          ((unsigned int)(MT6337_PMIC_REG_BASE+0x144C))
#define MT6337_AUXADC_CON11          ((unsigned int)(MT6337_PMIC_REG_BASE+0x144E))
#define MT6337_AUXADC_CON12          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1450))
#define MT6337_AUXADC_CON13          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1452))
#define MT6337_AUXADC_CON14          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1454))
#define MT6337_AUXADC_CON15          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1456))
#define MT6337_AUXADC_AUTORPT0       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1458))
#define MT6337_AUXADC_ACCDET         ((unsigned int)(MT6337_PMIC_REG_BASE+0x145A))
#define MT6337_AUXADC_THR0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x145C))
#define MT6337_AUXADC_THR1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x145E))
#define MT6337_AUXADC_THR2           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1460))
#define MT6337_AUXADC_THR3           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1462))
#define MT6337_AUXADC_THR4           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1464))
#define MT6337_AUXADC_THR5           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1466))
#define MT6337_AUXADC_THR6           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1468))
#define MT6337_AUXADC_EFUSE0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x146A))
#define MT6337_AUXADC_EFUSE1         ((unsigned int)(MT6337_PMIC_REG_BASE+0x146C))
#define MT6337_AUXADC_EFUSE2         ((unsigned int)(MT6337_PMIC_REG_BASE+0x146E))
#define MT6337_AUXADC_EFUSE3         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1470))
#define MT6337_AUXADC_EFUSE4         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1472))
#define MT6337_AUXADC_EFUSE5         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1474))
#define MT6337_AUXADC_DBG0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1476))
#define MT6337_AUXADC_ANA_CON0       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1478))
#define MT6337_AUXADC_ANA_CON1       ((unsigned int)(MT6337_PMIC_REG_BASE+0x147A))
#define MT6337_ACCDET_CON0           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1600))
#define MT6337_ACCDET_CON1           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1602))
#define MT6337_ACCDET_CON2           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1604))
#define MT6337_ACCDET_CON3           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1606))
#define MT6337_ACCDET_CON4           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1608))
#define MT6337_ACCDET_CON5           ((unsigned int)(MT6337_PMIC_REG_BASE+0x160A))
#define MT6337_ACCDET_CON6           ((unsigned int)(MT6337_PMIC_REG_BASE+0x160C))
#define MT6337_ACCDET_CON7           ((unsigned int)(MT6337_PMIC_REG_BASE+0x160E))
#define MT6337_ACCDET_CON8           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1610))
#define MT6337_ACCDET_CON9           ((unsigned int)(MT6337_PMIC_REG_BASE+0x1612))
#define MT6337_ACCDET_CON10          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1614))
#define MT6337_ACCDET_CON11          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1616))
#define MT6337_ACCDET_CON12          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1618))
#define MT6337_ACCDET_CON13          ((unsigned int)(MT6337_PMIC_REG_BASE+0x161A))
#define MT6337_ACCDET_CON14          ((unsigned int)(MT6337_PMIC_REG_BASE+0x161C))
#define MT6337_ACCDET_CON15          ((unsigned int)(MT6337_PMIC_REG_BASE+0x161E))
#define MT6337_ACCDET_CON16          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1620))
#define MT6337_ACCDET_CON17          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1622))
#define MT6337_ACCDET_CON18          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1624))
#define MT6337_ACCDET_CON19          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1626))
#define MT6337_ACCDET_CON20          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1628))
#define MT6337_ACCDET_CON21          ((unsigned int)(MT6337_PMIC_REG_BASE+0x162A))
#define MT6337_ACCDET_CON22          ((unsigned int)(MT6337_PMIC_REG_BASE+0x162C))
#define MT6337_ACCDET_CON23          ((unsigned int)(MT6337_PMIC_REG_BASE+0x162E))
#define MT6337_ACCDET_CON24          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1630))
#define MT6337_ACCDET_CON25          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1632))
#define MT6337_ACCDET_CON26          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1634))
#define MT6337_ACCDET_CON27          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1636))
#define MT6337_ACCDET_CON28          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1638))
#define MT6337_ACCDET_CON29          ((unsigned int)(MT6337_PMIC_REG_BASE+0x163A))
#define MT6337_ACCDET_CON30          ((unsigned int)(MT6337_PMIC_REG_BASE+0x163C))
#define MT6337_ACCDET_CON31          ((unsigned int)(MT6337_PMIC_REG_BASE+0x163E))
#define MT6337_ACCDET_CON32          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1640))
#define MT6337_ACCDET_CON33          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1642))
#define MT6337_ACCDET_CON34          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1644))
#define MT6337_ACCDET_CON35          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1646))
#define MT6337_ACCDET_CON36          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1648))
#define MT6337_AUDDEC_ANA_CON0       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1800))
#define MT6337_AUDDEC_ANA_CON1       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1802))
#define MT6337_AUDDEC_ANA_CON2       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1804))
#define MT6337_AUDDEC_ANA_CON3       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1806))
#define MT6337_AUDDEC_ANA_CON4       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1808))
#define MT6337_AUDDEC_ANA_CON5       ((unsigned int)(MT6337_PMIC_REG_BASE+0x180A))
#define MT6337_AUDDEC_ANA_CON6       ((unsigned int)(MT6337_PMIC_REG_BASE+0x180C))
#define MT6337_AUDDEC_ANA_CON7       ((unsigned int)(MT6337_PMIC_REG_BASE+0x180E))
#define MT6337_AUDDEC_ANA_CON8       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1810))
#define MT6337_AUDDEC_ANA_CON9       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1812))
#define MT6337_AUDDEC_ANA_CON10      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1814))
#define MT6337_AUDDEC_ANA_CON11      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1816))
#define MT6337_AUDDEC_ANA_CON12      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1818))
#define MT6337_AUDDEC_ANA_CON13      ((unsigned int)(MT6337_PMIC_REG_BASE+0x181A))
#define MT6337_AUDDEC_ANA_CON14      ((unsigned int)(MT6337_PMIC_REG_BASE+0x181C))
#define MT6337_AUDENC_ANA_CON0       ((unsigned int)(MT6337_PMIC_REG_BASE+0x181E))
#define MT6337_AUDENC_ANA_CON1       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1820))
#define MT6337_AUDENC_ANA_CON2       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1822))
#define MT6337_AUDENC_ANA_CON3       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1824))
#define MT6337_AUDENC_ANA_CON4       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1826))
#define MT6337_AUDENC_ANA_CON5       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1828))
#define MT6337_AUDENC_ANA_CON6       ((unsigned int)(MT6337_PMIC_REG_BASE+0x182A))
#define MT6337_AUDENC_ANA_CON7       ((unsigned int)(MT6337_PMIC_REG_BASE+0x182C))
#define MT6337_AUDENC_ANA_CON8       ((unsigned int)(MT6337_PMIC_REG_BASE+0x182E))
#define MT6337_AUDENC_ANA_CON9       ((unsigned int)(MT6337_PMIC_REG_BASE+0x1830))
#define MT6337_AUDENC_ANA_CON10      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1832))
#define MT6337_AUDENC_ANA_CON11      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1834))
#define MT6337_AUDENC_ANA_CON12      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1836))
#define MT6337_AUDENC_ANA_CON13      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1838))
#define MT6337_AUDENC_ANA_CON14      ((unsigned int)(MT6337_PMIC_REG_BASE+0x183A))
#define MT6337_AUDENC_ANA_CON15      ((unsigned int)(MT6337_PMIC_REG_BASE+0x183C))
#define MT6337_AUDENC_ANA_CON16      ((unsigned int)(MT6337_PMIC_REG_BASE+0x183E))
#define MT6337_AUDENC_ANA_CON17      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1840))
#define MT6337_AUDENC_ANA_CON18      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1842))
#define MT6337_AUDENC_ANA_CON19      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1844))
#define MT6337_AUDENC_ANA_CON20      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1846))
#define MT6337_AUDENC_ANA_CON21      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1848))
#define MT6337_AUDENC_ANA_CON22      ((unsigned int)(MT6337_PMIC_REG_BASE+0x184A))
#define MT6337_AUDENC_ANA_CON23      ((unsigned int)(MT6337_PMIC_REG_BASE+0x184C))
#define MT6337_AUDENC_ANA_CON24      ((unsigned int)(MT6337_PMIC_REG_BASE+0x184E))
#define MT6337_AUDENC_ANA_CON25      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1850))
#define MT6337_AUDENC_ANA_CON26      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1852))
#define MT6337_ZCD_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1A00))
#define MT6337_ZCD_CON1              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1A02))
#define MT6337_ZCD_CON2              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1A04))
#define MT6337_ZCD_CON3              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1A06))
#define MT6337_ZCD_CON4              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1A08))
#define MT6337_ZCD_CON5              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1A0A))
#define MT6337_GPIO_DIR0             ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C00))
#define MT6337_GPIO_DIR0_SET         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C02))
#define MT6337_GPIO_DIR0_CLR         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C04))
#define MT6337_GPIO_PULLEN0          ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C06))
#define MT6337_GPIO_PULLEN0_SET      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C08))
#define MT6337_GPIO_PULLEN0_CLR      ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C0A))
#define MT6337_GPIO_PULLSEL0         ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C0C))
#define MT6337_GPIO_PULLSEL0_SET     ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C0E))
#define MT6337_GPIO_PULLSEL0_CLR     ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C10))
#define MT6337_GPIO_DINV0            ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C12))
#define MT6337_GPIO_DINV0_SET        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C14))
#define MT6337_GPIO_DINV0_CLR        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C16))
#define MT6337_GPIO_DOUT0            ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C18))
#define MT6337_GPIO_DOUT0_SET        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C1A))
#define MT6337_GPIO_DOUT0_CLR        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C1C))
#define MT6337_GPIO_PI0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C1E))
#define MT6337_GPIO_POE0             ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C20))
#define MT6337_GPIO_MODE0            ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C22))
#define MT6337_GPIO_MODE0_SET        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C24))
#define MT6337_GPIO_MODE0_CLR        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C26))
#define MT6337_GPIO_MODE1            ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C28))
#define MT6337_GPIO_MODE1_SET        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C2A))
#define MT6337_GPIO_MODE1_CLR        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C2C))
#define MT6337_GPIO_MODE2            ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C2E))
#define MT6337_GPIO_MODE2_SET        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C30))
#define MT6337_GPIO_MODE2_CLR        ((unsigned int)(MT6337_PMIC_REG_BASE+0x1C32))
#define MT6337_RSV_CON0              ((unsigned int)(MT6337_PMIC_REG_BASE+0x1E00))
/*---mask is HEX;  shift is Integer---*/
#define MT6337_PMIC_HWCID_ADDR                                MT6337_HWCID
#define MT6337_PMIC_HWCID_MASK                                0xFFFF
#define MT6337_PMIC_HWCID_SHIFT                               0
#define MT6337_PMIC_SWCID_ADDR                                MT6337_SWCID
#define MT6337_PMIC_SWCID_MASK                                0xFFFF
#define MT6337_PMIC_SWCID_SHIFT                               0
#define MT6337_PMIC_ANACID_ADDR                               MT6337_ANACID
#define MT6337_PMIC_ANACID_MASK                               0x7
#define MT6337_PMIC_ANACID_SHIFT                              0
#define MT6337_PMIC_RG_SRCLKEN_IN0_EN_ADDR                    MT6337_TOP_CON
#define MT6337_PMIC_RG_SRCLKEN_IN0_EN_MASK                    0x1
#define MT6337_PMIC_RG_SRCLKEN_IN0_EN_SHIFT                   0
#define MT6337_PMIC_RG_VOWEN_EN_ADDR                          MT6337_TOP_CON
#define MT6337_PMIC_RG_VOWEN_EN_MASK                          0x1
#define MT6337_PMIC_RG_VOWEN_EN_SHIFT                         1
#define MT6337_PMIC_RG_OSC_SEL_ADDR                           MT6337_TOP_CON
#define MT6337_PMIC_RG_OSC_SEL_MASK                           0x1
#define MT6337_PMIC_RG_OSC_SEL_SHIFT                          2
#define MT6337_PMIC_RG_SRCLKEN_IN0_HW_MODE_ADDR               MT6337_TOP_CON
#define MT6337_PMIC_RG_SRCLKEN_IN0_HW_MODE_MASK               0x1
#define MT6337_PMIC_RG_SRCLKEN_IN0_HW_MODE_SHIFT              4
#define MT6337_PMIC_RG_VOWEN_HW_MODE_ADDR                     MT6337_TOP_CON
#define MT6337_PMIC_RG_VOWEN_HW_MODE_MASK                     0x1
#define MT6337_PMIC_RG_VOWEN_HW_MODE_SHIFT                    5
#define MT6337_PMIC_RG_OSC_SEL_HW_MODE_ADDR                   MT6337_TOP_CON
#define MT6337_PMIC_RG_OSC_SEL_HW_MODE_MASK                   0x1
#define MT6337_PMIC_RG_OSC_SEL_HW_MODE_SHIFT                  6
#define MT6337_PMIC_RG_SRCLKEN_IN_SYNC_EN_ADDR                MT6337_TOP_CON
#define MT6337_PMIC_RG_SRCLKEN_IN_SYNC_EN_MASK                0x1
#define MT6337_PMIC_RG_SRCLKEN_IN_SYNC_EN_SHIFT               8
#define MT6337_PMIC_RG_OSC_EN_AUTO_OFF_ADDR                   MT6337_TOP_CON
#define MT6337_PMIC_RG_OSC_EN_AUTO_OFF_MASK                   0x1
#define MT6337_PMIC_RG_OSC_EN_AUTO_OFF_SHIFT                  9
#define MT6337_PMIC_TEST_OUT_ADDR                             MT6337_TEST_OUT
#define MT6337_PMIC_TEST_OUT_MASK                             0xFF
#define MT6337_PMIC_TEST_OUT_SHIFT                            0
#define MT6337_PMIC_RG_MON_FLAG_SEL_ADDR                      MT6337_TEST_CON0
#define MT6337_PMIC_RG_MON_FLAG_SEL_MASK                      0xFF
#define MT6337_PMIC_RG_MON_FLAG_SEL_SHIFT                     0
#define MT6337_PMIC_RG_MON_GRP_SEL_ADDR                       MT6337_TEST_CON0
#define MT6337_PMIC_RG_MON_GRP_SEL_MASK                       0x1F
#define MT6337_PMIC_RG_MON_GRP_SEL_SHIFT                      8
#define MT6337_PMIC_RG_NANDTREE_MODE_ADDR                     MT6337_TEST_CON1
#define MT6337_PMIC_RG_NANDTREE_MODE_MASK                     0x1
#define MT6337_PMIC_RG_NANDTREE_MODE_SHIFT                    0
#define MT6337_PMIC_RG_TEST_AUXADC_ADDR                       MT6337_TEST_CON1
#define MT6337_PMIC_RG_TEST_AUXADC_MASK                       0x1
#define MT6337_PMIC_RG_TEST_AUXADC_SHIFT                      1
#define MT6337_PMIC_RG_EFUSE_MODE_ADDR                        MT6337_TEST_CON1
#define MT6337_PMIC_RG_EFUSE_MODE_MASK                        0x1
#define MT6337_PMIC_RG_EFUSE_MODE_SHIFT                       2
#define MT6337_PMIC_RG_TEST_STRUP_ADDR                        MT6337_TEST_CON1
#define MT6337_PMIC_RG_TEST_STRUP_MASK                        0x1
#define MT6337_PMIC_RG_TEST_STRUP_SHIFT                       3
#define MT6337_PMIC_TESTMODE_SW_ADDR                          MT6337_TESTMODE_SW
#define MT6337_PMIC_TESTMODE_SW_MASK                          0x1
#define MT6337_PMIC_TESTMODE_SW_SHIFT                         0
#define MT6337_PMIC_VA18_PG_DEB_ADDR                          MT6337_TOPSTATUS0
#define MT6337_PMIC_VA18_PG_DEB_MASK                          0x1
#define MT6337_PMIC_VA18_PG_DEB_SHIFT                         0
#define MT6337_PMIC_VA18_OC_STATUS_ADDR                       MT6337_TOPSTATUS0
#define MT6337_PMIC_VA18_OC_STATUS_MASK                       0x1
#define MT6337_PMIC_VA18_OC_STATUS_SHIFT                      1
#define MT6337_PMIC_VA25_OC_STATUS_ADDR                       MT6337_TOPSTATUS0
#define MT6337_PMIC_VA25_OC_STATUS_MASK                       0x1
#define MT6337_PMIC_VA25_OC_STATUS_SHIFT                      2
#define MT6337_PMIC_PMU_THR_DEB_ADDR                          MT6337_TOPSTATUS0
#define MT6337_PMIC_PMU_THR_DEB_MASK                          0x1
#define MT6337_PMIC_PMU_THR_DEB_SHIFT                         3
#define MT6337_PMIC_PMU_TEST_MODE_SCAN_ADDR                   MT6337_TOPSTATUS1
#define MT6337_PMIC_PMU_TEST_MODE_SCAN_MASK                   0x1
#define MT6337_PMIC_PMU_TEST_MODE_SCAN_SHIFT                  0
#define MT6337_PMIC_RG_PMU_TDSEL_ADDR                         MT6337_TDSEL_CON
#define MT6337_PMIC_RG_PMU_TDSEL_MASK                         0x1
#define MT6337_PMIC_RG_PMU_TDSEL_SHIFT                        0
#define MT6337_PMIC_RG_SPI_TDSEL_ADDR                         MT6337_TDSEL_CON
#define MT6337_PMIC_RG_SPI_TDSEL_MASK                         0x1
#define MT6337_PMIC_RG_SPI_TDSEL_SHIFT                        1
#define MT6337_PMIC_RG_AUD_TDSEL_ADDR                         MT6337_TDSEL_CON
#define MT6337_PMIC_RG_AUD_TDSEL_MASK                         0x1
#define MT6337_PMIC_RG_AUD_TDSEL_SHIFT                        2
#define MT6337_PMIC_RG_E32CAL_TDSEL_ADDR                      MT6337_TDSEL_CON
#define MT6337_PMIC_RG_E32CAL_TDSEL_MASK                      0x1
#define MT6337_PMIC_RG_E32CAL_TDSEL_SHIFT                     3
#define MT6337_PMIC_RG_PMU_RDSEL_ADDR                         MT6337_RDSEL_CON
#define MT6337_PMIC_RG_PMU_RDSEL_MASK                         0x1
#define MT6337_PMIC_RG_PMU_RDSEL_SHIFT                        0
#define MT6337_PMIC_RG_SPI_RDSEL_ADDR                         MT6337_RDSEL_CON
#define MT6337_PMIC_RG_SPI_RDSEL_MASK                         0x1
#define MT6337_PMIC_RG_SPI_RDSEL_SHIFT                        1
#define MT6337_PMIC_RG_AUD_RDSEL_ADDR                         MT6337_RDSEL_CON
#define MT6337_PMIC_RG_AUD_RDSEL_MASK                         0x1
#define MT6337_PMIC_RG_AUD_RDSEL_SHIFT                        2
#define MT6337_PMIC_RG_E32CAL_RDSEL_ADDR                      MT6337_RDSEL_CON
#define MT6337_PMIC_RG_E32CAL_RDSEL_MASK                      0x1
#define MT6337_PMIC_RG_E32CAL_RDSEL_SHIFT                     3
#define MT6337_PMIC_RG_SMT_WDTRSTB_IN_ADDR                    MT6337_SMT_CON0
#define MT6337_PMIC_RG_SMT_WDTRSTB_IN_MASK                    0x1
#define MT6337_PMIC_RG_SMT_WDTRSTB_IN_SHIFT                   0
#define MT6337_PMIC_RG_SMT_SRCLKEN_IN0_ADDR                   MT6337_SMT_CON0
#define MT6337_PMIC_RG_SMT_SRCLKEN_IN0_MASK                   0x1
#define MT6337_PMIC_RG_SMT_SRCLKEN_IN0_SHIFT                  2
#define MT6337_PMIC_RG_SMT_SPI_CLK_ADDR                       MT6337_SMT_CON1
#define MT6337_PMIC_RG_SMT_SPI_CLK_MASK                       0x1
#define MT6337_PMIC_RG_SMT_SPI_CLK_SHIFT                      0
#define MT6337_PMIC_RG_SMT_SPI_CSN_ADDR                       MT6337_SMT_CON1
#define MT6337_PMIC_RG_SMT_SPI_CSN_MASK                       0x1
#define MT6337_PMIC_RG_SMT_SPI_CSN_SHIFT                      1
#define MT6337_PMIC_RG_SMT_SPI_MOSI_ADDR                      MT6337_SMT_CON1
#define MT6337_PMIC_RG_SMT_SPI_MOSI_MASK                      0x1
#define MT6337_PMIC_RG_SMT_SPI_MOSI_SHIFT                     2
#define MT6337_PMIC_RG_SMT_SPI_MISO_ADDR                      MT6337_SMT_CON1
#define MT6337_PMIC_RG_SMT_SPI_MISO_MASK                      0x1
#define MT6337_PMIC_RG_SMT_SPI_MISO_SHIFT                     3
#define MT6337_PMIC_RG_SMT_AUD_CLK_MOSI_ADDR                  MT6337_SMT_CON2
#define MT6337_PMIC_RG_SMT_AUD_CLK_MOSI_MASK                  0x1
#define MT6337_PMIC_RG_SMT_AUD_CLK_MOSI_SHIFT                 0
#define MT6337_PMIC_RG_SMT_AUD_DAT_MOSI1_ADDR                 MT6337_SMT_CON2
#define MT6337_PMIC_RG_SMT_AUD_DAT_MOSI1_MASK                 0x1
#define MT6337_PMIC_RG_SMT_AUD_DAT_MOSI1_SHIFT                1
#define MT6337_PMIC_RG_SMT_AUD_DAT_MOSI2_ADDR                 MT6337_SMT_CON2
#define MT6337_PMIC_RG_SMT_AUD_DAT_MOSI2_MASK                 0x1
#define MT6337_PMIC_RG_SMT_AUD_DAT_MOSI2_SHIFT                2
#define MT6337_PMIC_RG_SMT_AUD_DAT_MISO1_ADDR                 MT6337_SMT_CON2
#define MT6337_PMIC_RG_SMT_AUD_DAT_MISO1_MASK                 0x1
#define MT6337_PMIC_RG_SMT_AUD_DAT_MISO1_SHIFT                3
#define MT6337_PMIC_RG_SMT_AUD_DAT_MISO2_ADDR                 MT6337_SMT_CON2
#define MT6337_PMIC_RG_SMT_AUD_DAT_MISO2_MASK                 0x1
#define MT6337_PMIC_RG_SMT_AUD_DAT_MISO2_SHIFT                4
#define MT6337_PMIC_RG_SMT_VOW_CLK_MISO_ADDR                  MT6337_SMT_CON2
#define MT6337_PMIC_RG_SMT_VOW_CLK_MISO_MASK                  0x1
#define MT6337_PMIC_RG_SMT_VOW_CLK_MISO_SHIFT                 5
#define MT6337_PMIC_RG_OCTL_SRCLKEN_IN0_ADDR                  MT6337_DRV_CON0
#define MT6337_PMIC_RG_OCTL_SRCLKEN_IN0_MASK                  0xF
#define MT6337_PMIC_RG_OCTL_SRCLKEN_IN0_SHIFT                 0
#define MT6337_PMIC_RG_OCTL_SPI_CLK_ADDR                      MT6337_DRV_CON1
#define MT6337_PMIC_RG_OCTL_SPI_CLK_MASK                      0xF
#define MT6337_PMIC_RG_OCTL_SPI_CLK_SHIFT                     0
#define MT6337_PMIC_RG_OCTL_SPI_CSN_ADDR                      MT6337_DRV_CON1
#define MT6337_PMIC_RG_OCTL_SPI_CSN_MASK                      0xF
#define MT6337_PMIC_RG_OCTL_SPI_CSN_SHIFT                     4
#define MT6337_PMIC_RG_OCTL_SPI_MOSI_ADDR                     MT6337_DRV_CON1
#define MT6337_PMIC_RG_OCTL_SPI_MOSI_MASK                     0xF
#define MT6337_PMIC_RG_OCTL_SPI_MOSI_SHIFT                    8
#define MT6337_PMIC_RG_OCTL_SPI_MISO_ADDR                     MT6337_DRV_CON1
#define MT6337_PMIC_RG_OCTL_SPI_MISO_MASK                     0xF
#define MT6337_PMIC_RG_OCTL_SPI_MISO_SHIFT                    12
#define MT6337_PMIC_RG_OCTL_AUD_CLK_MOSI_ADDR                 MT6337_DRV_CON2
#define MT6337_PMIC_RG_OCTL_AUD_CLK_MOSI_MASK                 0xF
#define MT6337_PMIC_RG_OCTL_AUD_CLK_MOSI_SHIFT                0
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI1_ADDR                MT6337_DRV_CON2
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI1_MASK                0xF
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI1_SHIFT               4
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI2_ADDR                MT6337_DRV_CON2
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI2_MASK                0xF
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI2_SHIFT               8
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MISO1_ADDR                MT6337_DRV_CON3
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MISO1_MASK                0xF
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MISO1_SHIFT               0
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MISO2_ADDR                MT6337_DRV_CON3
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MISO2_MASK                0xF
#define MT6337_PMIC_RG_OCTL_AUD_DAT_MISO2_SHIFT               4
#define MT6337_PMIC_RG_OCTL_VOW_CLK_MISO_ADDR                 MT6337_DRV_CON3
#define MT6337_PMIC_RG_OCTL_VOW_CLK_MISO_MASK                 0xF
#define MT6337_PMIC_RG_OCTL_VOW_CLK_MISO_SHIFT                12
#define MT6337_PMIC_RG_FILTER_SPI_CLK_ADDR                    MT6337_FILTER_CON0
#define MT6337_PMIC_RG_FILTER_SPI_CLK_MASK                    0x1
#define MT6337_PMIC_RG_FILTER_SPI_CLK_SHIFT                   0
#define MT6337_PMIC_RG_FILTER_SPI_CSN_ADDR                    MT6337_FILTER_CON0
#define MT6337_PMIC_RG_FILTER_SPI_CSN_MASK                    0x1
#define MT6337_PMIC_RG_FILTER_SPI_CSN_SHIFT                   1
#define MT6337_PMIC_RG_FILTER_SPI_MOSI_ADDR                   MT6337_FILTER_CON0
#define MT6337_PMIC_RG_FILTER_SPI_MOSI_MASK                   0x1
#define MT6337_PMIC_RG_FILTER_SPI_MOSI_SHIFT                  2
#define MT6337_PMIC_RG_FILTER_SPI_MISO_ADDR                   MT6337_FILTER_CON0
#define MT6337_PMIC_RG_FILTER_SPI_MISO_MASK                   0x1
#define MT6337_PMIC_RG_FILTER_SPI_MISO_SHIFT                  3
#define MT6337_PMIC_RG_FILTER_AUD_CLK_MOSI_ADDR               MT6337_FILTER_CON1
#define MT6337_PMIC_RG_FILTER_AUD_CLK_MOSI_MASK               0x1
#define MT6337_PMIC_RG_FILTER_AUD_CLK_MOSI_SHIFT              0
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI1_ADDR              MT6337_FILTER_CON1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI1_MASK              0x1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI1_SHIFT             1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI2_ADDR              MT6337_FILTER_CON1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI2_MASK              0x1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI2_SHIFT             2
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MISO1_ADDR              MT6337_FILTER_CON1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MISO1_MASK              0x1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MISO1_SHIFT             3
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MISO2_ADDR              MT6337_FILTER_CON1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MISO2_MASK              0x1
#define MT6337_PMIC_RG_FILTER_AUD_DAT_MISO2_SHIFT             4
#define MT6337_PMIC_RG_FILTER_VOW_CLK_MISO_ADDR               MT6337_FILTER_CON1
#define MT6337_PMIC_RG_FILTER_VOW_CLK_MISO_MASK               0x1
#define MT6337_PMIC_RG_FILTER_VOW_CLK_MISO_SHIFT              5
#define MT6337_PMIC_RG_SPI_MOSI_DLY_ADDR                      MT6337_DLY_CON0
#define MT6337_PMIC_RG_SPI_MOSI_DLY_MASK                      0xF
#define MT6337_PMIC_RG_SPI_MOSI_DLY_SHIFT                     0
#define MT6337_PMIC_RG_SPI_MOSI_OE_DLY_ADDR                   MT6337_DLY_CON0
#define MT6337_PMIC_RG_SPI_MOSI_OE_DLY_MASK                   0xF
#define MT6337_PMIC_RG_SPI_MOSI_OE_DLY_SHIFT                  4
#define MT6337_PMIC_RG_SPI_MISO_DLY_ADDR                      MT6337_DLY_CON0
#define MT6337_PMIC_RG_SPI_MISO_DLY_MASK                      0xF
#define MT6337_PMIC_RG_SPI_MISO_DLY_SHIFT                     8
#define MT6337_PMIC_RG_SPI_MISO_OE_DLY_ADDR                   MT6337_DLY_CON0
#define MT6337_PMIC_RG_SPI_MISO_OE_DLY_MASK                   0xF
#define MT6337_PMIC_RG_SPI_MISO_OE_DLY_SHIFT                  12
#define MT6337_PMIC_TOP_STATUS_ADDR                           MT6337_TOP_STATUS
#define MT6337_PMIC_TOP_STATUS_MASK                           0xF
#define MT6337_PMIC_TOP_STATUS_SHIFT                          0
#define MT6337_PMIC_TOP_STATUS_SET_ADDR                       MT6337_TOP_STATUS_SET
#define MT6337_PMIC_TOP_STATUS_SET_MASK                       0x3
#define MT6337_PMIC_TOP_STATUS_SET_SHIFT                      0
#define MT6337_PMIC_TOP_STATUS_CLR_ADDR                       MT6337_TOP_STATUS_CLR
#define MT6337_PMIC_TOP_STATUS_CLR_MASK                       0x3
#define MT6337_PMIC_TOP_STATUS_CLR_SHIFT                      0
#define MT6337_PMIC_RG_SRCLKEN_IN2_EN_ADDR                    MT6337_TOP_SPI_CON0
#define MT6337_PMIC_RG_SRCLKEN_IN2_EN_MASK                    0x1
#define MT6337_PMIC_RG_SRCLKEN_IN2_EN_SHIFT                   0
#define MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_ADDR                  MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_MASK                  0x1
#define MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_SHIFT                 0
#define MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_ADDR                 MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_MASK                 0x1
#define MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_SHIFT                1
#define MT6337_PMIC_RG_SMPS_AO_1M_CK_PDN_ADDR                 MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_SMPS_AO_1M_CK_PDN_MASK                 0x1
#define MT6337_PMIC_RG_SMPS_AO_1M_CK_PDN_SHIFT                2
#define MT6337_PMIC_RG_STRUP_75K_CK_PDN_ADDR                  MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_STRUP_75K_CK_PDN_MASK                  0x1
#define MT6337_PMIC_RG_STRUP_75K_CK_PDN_SHIFT                 3
#define MT6337_PMIC_RG_TRIM_75K_CK_PDN_ADDR                   MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_TRIM_75K_CK_PDN_MASK                   0x1
#define MT6337_PMIC_RG_TRIM_75K_CK_PDN_SHIFT                  4
#define MT6337_PMIC_RG_VOW32K_CK_PDN_ADDR                     MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_VOW32K_CK_PDN_MASK                     0x1
#define MT6337_PMIC_RG_VOW32K_CK_PDN_SHIFT                    5
#define MT6337_PMIC_RG_VOW12M_CK_PDN_ADDR                     MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_VOW12M_CK_PDN_MASK                     0x1
#define MT6337_PMIC_RG_VOW12M_CK_PDN_SHIFT                    6
#define MT6337_PMIC_RG_AUXADC_CK_PDN_ADDR                     MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_AUXADC_CK_PDN_MASK                     0x1
#define MT6337_PMIC_RG_AUXADC_CK_PDN_SHIFT                    7
#define MT6337_PMIC_RG_AUXADC_1M_CK_PDN_ADDR                  MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_AUXADC_1M_CK_PDN_MASK                  0x1
#define MT6337_PMIC_RG_AUXADC_1M_CK_PDN_SHIFT                 8
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_ADDR                MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_MASK                0x1
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_SHIFT               9
#define MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_ADDR                 MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_MASK                 0x1
#define MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_SHIFT                10
#define MT6337_PMIC_RG_AUDIF_CK_PDN_ADDR                      MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_AUDIF_CK_PDN_MASK                      0x1
#define MT6337_PMIC_RG_AUDIF_CK_PDN_SHIFT                     11
#define MT6337_PMIC_RG_AUD_CK_PDN_ADDR                        MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_AUD_CK_PDN_MASK                        0x1
#define MT6337_PMIC_RG_AUD_CK_PDN_SHIFT                       12
#define MT6337_PMIC_RG_ZCD13M_CK_PDN_ADDR                     MT6337_TOP_CKPDN_CON0
#define MT6337_PMIC_RG_ZCD13M_CK_PDN_MASK                     0x1
#define MT6337_PMIC_RG_ZCD13M_CK_PDN_SHIFT                    13
#define MT6337_PMIC_TOP_CKPDN_CON0_SET_ADDR                   MT6337_TOP_CKPDN_CON0_SET
#define MT6337_PMIC_TOP_CKPDN_CON0_SET_MASK                   0xFFFF
#define MT6337_PMIC_TOP_CKPDN_CON0_SET_SHIFT                  0
#define MT6337_PMIC_TOP_CKPDN_CON0_CLR_ADDR                   MT6337_TOP_CKPDN_CON0_CLR
#define MT6337_PMIC_TOP_CKPDN_CON0_CLR_MASK                   0xFFFF
#define MT6337_PMIC_TOP_CKPDN_CON0_CLR_SHIFT                  0
#define MT6337_PMIC_RG_BUCK_9M_CK_PDN_ADDR                    MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_BUCK_9M_CK_PDN_MASK                    0x1
#define MT6337_PMIC_RG_BUCK_9M_CK_PDN_SHIFT                   0
#define MT6337_PMIC_RG_BUCK_32K_CK_PDN_ADDR                   MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_BUCK_32K_CK_PDN_MASK                   0x1
#define MT6337_PMIC_RG_BUCK_32K_CK_PDN_SHIFT                  1
#define MT6337_PMIC_RG_INTRP_CK_PDN_ADDR                      MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_INTRP_CK_PDN_MASK                      0x1
#define MT6337_PMIC_RG_INTRP_CK_PDN_SHIFT                     2
#define MT6337_PMIC_RG_INTRP_PRE_OC_CK_PDN_ADDR               MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_INTRP_PRE_OC_CK_PDN_MASK               0x1
#define MT6337_PMIC_RG_INTRP_PRE_OC_CK_PDN_SHIFT              3
#define MT6337_PMIC_RG_EFUSE_CK_PDN_ADDR                      MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_EFUSE_CK_PDN_MASK                      0x1
#define MT6337_PMIC_RG_EFUSE_CK_PDN_SHIFT                     4
#define MT6337_PMIC_RG_FQMTR_32K_CK_PDN_ADDR                  MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_FQMTR_32K_CK_PDN_MASK                  0x1
#define MT6337_PMIC_RG_FQMTR_32K_CK_PDN_SHIFT                 5
#define MT6337_PMIC_RG_FQMTR_26M_CK_PDN_ADDR                  MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_FQMTR_26M_CK_PDN_MASK                  0x1
#define MT6337_PMIC_RG_FQMTR_26M_CK_PDN_SHIFT                 6
#define MT6337_PMIC_RG_FQMTR_CK_PDN_ADDR                      MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_FQMTR_CK_PDN_MASK                      0x1
#define MT6337_PMIC_RG_FQMTR_CK_PDN_SHIFT                     7
#define MT6337_PMIC_RG_LDO_CALI_75K_CK_PDN_ADDR               MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_LDO_CALI_75K_CK_PDN_MASK               0x1
#define MT6337_PMIC_RG_LDO_CALI_75K_CK_PDN_SHIFT              8
#define MT6337_PMIC_RG_STB_1M_CK_PDN_ADDR                     MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_STB_1M_CK_PDN_MASK                     0x1
#define MT6337_PMIC_RG_STB_1M_CK_PDN_SHIFT                    9
#define MT6337_PMIC_RG_SMPS_CK_DIV_PDN_ADDR                   MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_SMPS_CK_DIV_PDN_MASK                   0x1
#define MT6337_PMIC_RG_SMPS_CK_DIV_PDN_SHIFT                  10
#define MT6337_PMIC_RG_ACCDET_CK_PDN_ADDR                     MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_ACCDET_CK_PDN_MASK                     0x1
#define MT6337_PMIC_RG_ACCDET_CK_PDN_SHIFT                    11
#define MT6337_PMIC_RG_BGR_TEST_CK_PDN_ADDR                   MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_BGR_TEST_CK_PDN_MASK                   0x1
#define MT6337_PMIC_RG_BGR_TEST_CK_PDN_SHIFT                  12
#define MT6337_PMIC_RG_REG_CK_PDN_ADDR                        MT6337_TOP_CKPDN_CON1
#define MT6337_PMIC_RG_REG_CK_PDN_MASK                        0x1
#define MT6337_PMIC_RG_REG_CK_PDN_SHIFT                       13
#define MT6337_PMIC_TOP_CKPDN_CON1_SET_ADDR                   MT6337_TOP_CKPDN_CON1_SET
#define MT6337_PMIC_TOP_CKPDN_CON1_SET_MASK                   0xFFFF
#define MT6337_PMIC_TOP_CKPDN_CON1_SET_SHIFT                  0
#define MT6337_PMIC_TOP_CKPDN_CON1_CLR_ADDR                   MT6337_TOP_CKPDN_CON1_CLR
#define MT6337_PMIC_TOP_CKPDN_CON1_CLR_MASK                   0xFFFF
#define MT6337_PMIC_TOP_CKPDN_CON1_CLR_SHIFT                  0
#define MT6337_PMIC_RG_AUDIF_CK_CKSEL_ADDR                    MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_AUDIF_CK_CKSEL_MASK                    0x1
#define MT6337_PMIC_RG_AUDIF_CK_CKSEL_SHIFT                   0
#define MT6337_PMIC_RG_AUD_CK_CKSEL_ADDR                      MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_AUD_CK_CKSEL_MASK                      0x1
#define MT6337_PMIC_RG_AUD_CK_CKSEL_SHIFT                     1
#define MT6337_PMIC_RG_STRUP_75K_CK_CKSEL_ADDR                MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_STRUP_75K_CK_CKSEL_MASK                0x1
#define MT6337_PMIC_RG_STRUP_75K_CK_CKSEL_SHIFT               2
#define MT6337_PMIC_RG_BGR_TEST_CK_CKSEL_ADDR                 MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_BGR_TEST_CK_CKSEL_MASK                 0x1
#define MT6337_PMIC_RG_BGR_TEST_CK_CKSEL_SHIFT                3
#define MT6337_PMIC_RG_FQMTR_CK_CKSEL_ADDR                    MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_FQMTR_CK_CKSEL_MASK                    0x3
#define MT6337_PMIC_RG_FQMTR_CK_CKSEL_SHIFT                   4
#define MT6337_PMIC_RG_OSC_SEL_HW_SRC_SEL_ADDR                MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_OSC_SEL_HW_SRC_SEL_MASK                0x3
#define MT6337_PMIC_RG_OSC_SEL_HW_SRC_SEL_SHIFT               6
#define MT6337_PMIC_RG_TOP_CKSEL_CON0_RSV_ADDR                MT6337_TOP_CKSEL_CON0
#define MT6337_PMIC_RG_TOP_CKSEL_CON0_RSV_MASK                0xFF
#define MT6337_PMIC_RG_TOP_CKSEL_CON0_RSV_SHIFT               8
#define MT6337_PMIC_TOP_CKSEL_CON_SET_ADDR                    MT6337_TOP_CKSEL_CON0_SET
#define MT6337_PMIC_TOP_CKSEL_CON_SET_MASK                    0xFFFF
#define MT6337_PMIC_TOP_CKSEL_CON_SET_SHIFT                   0
#define MT6337_PMIC_TOP_CKSEL_CON_CLR_ADDR                    MT6337_TOP_CKSEL_CON0_CLR
#define MT6337_PMIC_TOP_CKSEL_CON_CLR_MASK                    0xFFFF
#define MT6337_PMIC_TOP_CKSEL_CON_CLR_SHIFT                   0
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_DIVSEL_ADDR             MT6337_TOP_CKDIVSEL_CON0
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_DIVSEL_MASK             0x3
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_DIVSEL_SHIFT            0
#define MT6337_PMIC_RG_REG_CK_DIVSEL_ADDR                     MT6337_TOP_CKDIVSEL_CON0
#define MT6337_PMIC_RG_REG_CK_DIVSEL_MASK                     0x3
#define MT6337_PMIC_RG_REG_CK_DIVSEL_SHIFT                    2
#define MT6337_PMIC_RG_BUCK_9M_CK_DIVSEL_ADDR                 MT6337_TOP_CKDIVSEL_CON0
#define MT6337_PMIC_RG_BUCK_9M_CK_DIVSEL_MASK                 0x1
#define MT6337_PMIC_RG_BUCK_9M_CK_DIVSEL_SHIFT                4
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_RSV_ADDR                MT6337_TOP_CKDIVSEL_CON0
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_RSV_MASK                0xFF
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_RSV_SHIFT               8
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_SET_ADDR                MT6337_TOP_CKDIVSEL_CON0_SET
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_SET_MASK                0xFFFF
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_SET_SHIFT               0
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_CLR_ADDR                MT6337_TOP_CKDIVSEL_CON0_CLR
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_CLR_MASK                0xFFFF
#define MT6337_PMIC_TOP_CKDIVSEL_CON0_CLR_SHIFT               0
#define MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_HWEN_ADDR             MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_HWEN_MASK             0x1
#define MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_HWEN_SHIFT            0
#define MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN_ADDR            MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN_MASK            0x1
#define MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN_SHIFT           1
#define MT6337_PMIC_RG_AUXADC_CK_PDN_HWEN_ADDR                MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_AUXADC_CK_PDN_HWEN_MASK                0x1
#define MT6337_PMIC_RG_AUXADC_CK_PDN_HWEN_SHIFT               2
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN_ADDR           MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN_MASK           0x1
#define MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN_SHIFT          3
#define MT6337_PMIC_RG_EFUSE_CK_PDN_HWEN_ADDR                 MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_EFUSE_CK_PDN_HWEN_MASK                 0x1
#define MT6337_PMIC_RG_EFUSE_CK_PDN_HWEN_SHIFT                4
#define MT6337_PMIC_RG_STB_1M_CK_PDN_HWEN_ADDR                MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_STB_1M_CK_PDN_HWEN_MASK                0x1
#define MT6337_PMIC_RG_STB_1M_CK_PDN_HWEN_SHIFT               5
#define MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_HWEN_ADDR            MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_HWEN_MASK            0x1
#define MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_HWEN_SHIFT           6
#define MT6337_PMIC_RG_REG_CK_PDN_HWEN_ADDR                   MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_RG_REG_CK_PDN_HWEN_MASK                   0x1
#define MT6337_PMIC_RG_REG_CK_PDN_HWEN_SHIFT                  7
#define MT6337_PMIC_TOP_CKHWEN_CON0_RSV_ADDR                  MT6337_TOP_CKHWEN_CON0
#define MT6337_PMIC_TOP_CKHWEN_CON0_RSV_MASK                  0xF
#define MT6337_PMIC_TOP_CKHWEN_CON0_RSV_SHIFT                 12
#define MT6337_PMIC_TOP_CKHWEN_CON0_SET_ADDR                  MT6337_TOP_CKHWEN_CON0_SET
#define MT6337_PMIC_TOP_CKHWEN_CON0_SET_MASK                  0xFFFF
#define MT6337_PMIC_TOP_CKHWEN_CON0_SET_SHIFT                 0
#define MT6337_PMIC_TOP_CKHWEN_CON0_CLR_ADDR                  MT6337_TOP_CKHWEN_CON0_CLR
#define MT6337_PMIC_TOP_CKHWEN_CON0_CLR_MASK                  0xFFFF
#define MT6337_PMIC_TOP_CKHWEN_CON0_CLR_SHIFT                 0
#define MT6337_PMIC_TOP_CKHWEN_CON1_RSV_ADDR                  MT6337_TOP_CKHWEN_CON1
#define MT6337_PMIC_TOP_CKHWEN_CON1_RSV_MASK                  0xF
#define MT6337_PMIC_TOP_CKHWEN_CON1_RSV_SHIFT                 12
#define MT6337_PMIC_RG_PMU75K_CK_TST_DIS_ADDR                 MT6337_TOP_CKTST_CON0
#define MT6337_PMIC_RG_PMU75K_CK_TST_DIS_MASK                 0x1
#define MT6337_PMIC_RG_PMU75K_CK_TST_DIS_SHIFT                0
#define MT6337_PMIC_RG_SMPS_CK_TST_DIS_ADDR                   MT6337_TOP_CKTST_CON0
#define MT6337_PMIC_RG_SMPS_CK_TST_DIS_MASK                   0x1
#define MT6337_PMIC_RG_SMPS_CK_TST_DIS_SHIFT                  1
#define MT6337_PMIC_RG_AUD26M_CK_TST_DIS_ADDR                 MT6337_TOP_CKTST_CON0
#define MT6337_PMIC_RG_AUD26M_CK_TST_DIS_MASK                 0x1
#define MT6337_PMIC_RG_AUD26M_CK_TST_DIS_SHIFT                2
#define MT6337_PMIC_RG_CLK32K_CK_TST_DIS_ADDR                 MT6337_TOP_CKTST_CON0
#define MT6337_PMIC_RG_CLK32K_CK_TST_DIS_MASK                 0x1
#define MT6337_PMIC_RG_CLK32K_CK_TST_DIS_SHIFT                3
#define MT6337_PMIC_RG_VOW12M_CK_TST_DIS_ADDR                 MT6337_TOP_CKTST_CON0
#define MT6337_PMIC_RG_VOW12M_CK_TST_DIS_MASK                 0x1
#define MT6337_PMIC_RG_VOW12M_CK_TST_DIS_SHIFT                4
#define MT6337_PMIC_TOP_CKTST_CON0_RSV_ADDR                   MT6337_TOP_CKTST_CON0
#define MT6337_PMIC_TOP_CKTST_CON0_RSV_MASK                   0x1
#define MT6337_PMIC_TOP_CKTST_CON0_RSV_SHIFT                  15
#define MT6337_PMIC_RG_VOW12M_CK_TSTSEL_ADDR                  MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_VOW12M_CK_TSTSEL_MASK                  0x1
#define MT6337_PMIC_RG_VOW12M_CK_TSTSEL_SHIFT                 0
#define MT6337_PMIC_RG_AUD26M_CK_TSTSEL_ADDR                  MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_AUD26M_CK_TSTSEL_MASK                  0x1
#define MT6337_PMIC_RG_AUD26M_CK_TSTSEL_SHIFT                 1
#define MT6337_PMIC_RG_AUDIF_CK_TSTSEL_ADDR                   MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_AUDIF_CK_TSTSEL_MASK                   0x1
#define MT6337_PMIC_RG_AUDIF_CK_TSTSEL_SHIFT                  2
#define MT6337_PMIC_RG_AUD_CK_TSTSEL_ADDR                     MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_AUD_CK_TSTSEL_MASK                     0x1
#define MT6337_PMIC_RG_AUD_CK_TSTSEL_SHIFT                    3
#define MT6337_PMIC_RG_FQMTR_CK_TSTSEL_ADDR                   MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_FQMTR_CK_TSTSEL_MASK                   0x1
#define MT6337_PMIC_RG_FQMTR_CK_TSTSEL_SHIFT                  4
#define MT6337_PMIC_RG_PMU75K_CK_TSTSEL_ADDR                  MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_PMU75K_CK_TSTSEL_MASK                  0x1
#define MT6337_PMIC_RG_PMU75K_CK_TSTSEL_SHIFT                 5
#define MT6337_PMIC_RG_SMPS_CK_TSTSEL_ADDR                    MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_SMPS_CK_TSTSEL_MASK                    0x1
#define MT6337_PMIC_RG_SMPS_CK_TSTSEL_SHIFT                   6
#define MT6337_PMIC_RG_STRUP_75K_CK_TSTSEL_ADDR               MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_STRUP_75K_CK_TSTSEL_MASK               0x1
#define MT6337_PMIC_RG_STRUP_75K_CK_TSTSEL_SHIFT              7
#define MT6337_PMIC_RG_CLK32K_CK_TSTSEL_ADDR                  MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_CLK32K_CK_TSTSEL_MASK                  0x1
#define MT6337_PMIC_RG_CLK32K_CK_TSTSEL_SHIFT                 8
#define MT6337_PMIC_RG_BGR_TEST_CK_TSTSEL_ADDR                MT6337_TOP_CKTST_CON1
#define MT6337_PMIC_RG_BGR_TEST_CK_TSTSEL_MASK                0x1
#define MT6337_PMIC_RG_BGR_TEST_CK_TSTSEL_SHIFT               15
#define MT6337_PMIC_RG_CLKSQ_EN_AUD_ADDR                      MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_EN_AUD_MASK                      0x1
#define MT6337_PMIC_RG_CLKSQ_EN_AUD_SHIFT                     0
#define MT6337_PMIC_RG_CLKSQ_EN_FQR_ADDR                      MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_EN_FQR_MASK                      0x1
#define MT6337_PMIC_RG_CLKSQ_EN_FQR_SHIFT                     1
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_ADDR                   MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MASK                   0x1
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_SHIFT                  2
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_RSV_ADDR                  MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_RSV_MASK                  0x1
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_RSV_SHIFT                 5
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MODE_ADDR              MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MODE_MASK              0x1
#define MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MODE_SHIFT             8
#define MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_ADDR                 MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_MASK                 0x1
#define MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SHIFT                10
#define MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SWCTRL_ADDR          MT6337_TOP_CLKSQ
#define MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SWCTRL_MASK          0x1
#define MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SWCTRL_SHIFT         11
#define MT6337_PMIC_TOP_CLKSQ_RSV_ADDR                        MT6337_TOP_CLKSQ
#define MT6337_PMIC_TOP_CLKSQ_RSV_MASK                        0x7
#define MT6337_PMIC_TOP_CLKSQ_RSV_SHIFT                       12
#define MT6337_PMIC_DA_CLKSQ_EN_VA18_ADDR                     MT6337_TOP_CLKSQ
#define MT6337_PMIC_DA_CLKSQ_EN_VA18_MASK                     0x1
#define MT6337_PMIC_DA_CLKSQ_EN_VA18_SHIFT                    15
#define MT6337_PMIC_TOP_CLKSQ_SET_ADDR                        MT6337_TOP_CLKSQ_SET
#define MT6337_PMIC_TOP_CLKSQ_SET_MASK                        0xFFFF
#define MT6337_PMIC_TOP_CLKSQ_SET_SHIFT                       0
#define MT6337_PMIC_TOP_CLKSQ_CLR_ADDR                        MT6337_TOP_CLKSQ_CLR
#define MT6337_PMIC_TOP_CLKSQ_CLR_MASK                        0xFFFF
#define MT6337_PMIC_TOP_CLKSQ_CLR_SHIFT                       0
#define MT6337_PMIC_TOP_CLKSQ_RTC_RSV0_ADDR                   MT6337_TOP_CLKSQ_RTC
#define MT6337_PMIC_TOP_CLKSQ_RTC_RSV0_MASK                   0xF
#define MT6337_PMIC_TOP_CLKSQ_RTC_RSV0_SHIFT                  0
#define MT6337_PMIC_RG_CLKSQ_EN_6336_ADDR                     MT6337_TOP_CLKSQ_RTC
#define MT6337_PMIC_RG_CLKSQ_EN_6336_MASK                     0x1
#define MT6337_PMIC_RG_CLKSQ_EN_6336_SHIFT                    8
#define MT6337_PMIC_TOP_CLKSQ_RTC_SET_ADDR                    MT6337_TOP_CLKSQ_RTC_SET
#define MT6337_PMIC_TOP_CLKSQ_RTC_SET_MASK                    0xFFFF
#define MT6337_PMIC_TOP_CLKSQ_RTC_SET_SHIFT                   0
#define MT6337_PMIC_TOP_CLKSQ_RTC_CLR_ADDR                    MT6337_TOP_CLKSQ_RTC_CLR
#define MT6337_PMIC_TOP_CLKSQ_RTC_CLR_MASK                    0xFFFF
#define MT6337_PMIC_TOP_CLKSQ_RTC_CLR_SHIFT                   0
#define MT6337_PMIC_OSC_75K_TRIM_ADDR                         MT6337_TOP_CLK_TRIM
#define MT6337_PMIC_OSC_75K_TRIM_MASK                         0x1F
#define MT6337_PMIC_OSC_75K_TRIM_SHIFT                        0
#define MT6337_PMIC_RG_OSC_75K_TRIM_EN_ADDR                   MT6337_TOP_CLK_TRIM
#define MT6337_PMIC_RG_OSC_75K_TRIM_EN_MASK                   0x1
#define MT6337_PMIC_RG_OSC_75K_TRIM_EN_SHIFT                  5
#define MT6337_PMIC_RG_OSC_75K_TRIM_RATE_ADDR                 MT6337_TOP_CLK_TRIM
#define MT6337_PMIC_RG_OSC_75K_TRIM_RATE_MASK                 0x3
#define MT6337_PMIC_RG_OSC_75K_TRIM_RATE_SHIFT                6
#define MT6337_PMIC_DA_OSC_75K_TRIM_ADDR                      MT6337_TOP_CLK_TRIM
#define MT6337_PMIC_DA_OSC_75K_TRIM_MASK                      0x1F
#define MT6337_PMIC_DA_OSC_75K_TRIM_SHIFT                     8
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_EN_ADDR         MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_EN_MASK         0x1
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_EN_SHIFT        0
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_DLY_EN_ADDR     MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_DLY_EN_MASK     0x1
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_DLY_EN_SHIFT    1
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN2_EN_ADDR         MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN2_EN_MASK         0x1
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN2_EN_SHIFT        2
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_BUCK_OSC_SEL_EN_ADDR     MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_BUCK_OSC_SEL_EN_MASK     0x1
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_BUCK_OSC_SEL_EN_SHIFT    3
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_VOWEN_EN_ADDR            MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_VOWEN_EN_MASK            0x1
#define MT6337_PMIC_RG_G_SMPS_CK_PDN_VOWEN_EN_SHIFT           4
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_EN_ADDR               MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_EN_MASK               0x1
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_EN_SHIFT              5
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_DLY_EN_ADDR           MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_DLY_EN_MASK           0x1
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_DLY_EN_SHIFT          6
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN2_EN_ADDR               MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN2_EN_MASK               0x1
#define MT6337_PMIC_RG_OSC_SEL_SRCLKEN2_EN_SHIFT              7
#define MT6337_PMIC_RG_OSC_SEL_BUCK_LDO_EN_ADDR               MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_OSC_SEL_BUCK_LDO_EN_MASK               0x1
#define MT6337_PMIC_RG_OSC_SEL_BUCK_LDO_EN_SHIFT              8
#define MT6337_PMIC_RG_OSC_SEL_VOWEN_EN_ADDR                  MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_OSC_SEL_VOWEN_EN_MASK                  0x1
#define MT6337_PMIC_RG_OSC_SEL_VOWEN_EN_SHIFT                 9
#define MT6337_PMIC_RG_OSC_SEL_SPI_EN_ADDR                    MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_OSC_SEL_SPI_EN_MASK                    0x1
#define MT6337_PMIC_RG_OSC_SEL_SPI_EN_SHIFT                   10
#define MT6337_PMIC_RG_CLK_RSV_ADDR                           MT6337_TOP_CLK_CON0
#define MT6337_PMIC_RG_CLK_RSV_MASK                           0x1F
#define MT6337_PMIC_RG_CLK_RSV_SHIFT                          11
#define MT6337_PMIC_TOP_CLK_CON0_SET_ADDR                     MT6337_TOP_CLK_CON0_SET
#define MT6337_PMIC_TOP_CLK_CON0_SET_MASK                     0xFFFF
#define MT6337_PMIC_TOP_CLK_CON0_SET_SHIFT                    0
#define MT6337_PMIC_TOP_CLK_CON0_CLR_ADDR                     MT6337_TOP_CLK_CON0_CLR
#define MT6337_PMIC_TOP_CLK_CON0_CLR_MASK                     0xFFFF
#define MT6337_PMIC_TOP_CLK_CON0_CLR_SHIFT                    0
#define MT6337_PMIC_BUCK_LDO_FT_TESTMODE_EN_ADDR              MT6337_BUCK_ALL_CON0
#define MT6337_PMIC_BUCK_LDO_FT_TESTMODE_EN_MASK              0x1
#define MT6337_PMIC_BUCK_LDO_FT_TESTMODE_EN_SHIFT             0
#define MT6337_PMIC_BUCK_ALL_CON0_RSV1_ADDR                   MT6337_BUCK_ALL_CON0
#define MT6337_PMIC_BUCK_ALL_CON0_RSV1_MASK                   0x7F
#define MT6337_PMIC_BUCK_ALL_CON0_RSV1_SHIFT                  1
#define MT6337_PMIC_BUCK_ALL_CON0_RSV0_ADDR                   MT6337_BUCK_ALL_CON0
#define MT6337_PMIC_BUCK_ALL_CON0_RSV0_MASK                   0xFF
#define MT6337_PMIC_BUCK_ALL_CON0_RSV0_SHIFT                  8
#define MT6337_PMIC_BUCK_BUCK_RSV_ADDR                        MT6337_BUCK_ALL_CON1
#define MT6337_PMIC_BUCK_BUCK_RSV_MASK                        0xFFFF
#define MT6337_PMIC_BUCK_BUCK_RSV_SHIFT                       0
#define MT6337_PMIC_RG_OSC_SEL_SW_ADDR                        MT6337_BUCK_ALL_CON2
#define MT6337_PMIC_RG_OSC_SEL_SW_MASK                        0x1
#define MT6337_PMIC_RG_OSC_SEL_SW_SHIFT                       0
#define MT6337_PMIC_RG_OSC_SEL_LDO_MODE_ADDR                  MT6337_BUCK_ALL_CON2
#define MT6337_PMIC_RG_OSC_SEL_LDO_MODE_MASK                  0x1
#define MT6337_PMIC_RG_OSC_SEL_LDO_MODE_SHIFT                 1
#define MT6337_PMIC_BUCK_BUCK_VSLEEP_SRCLKEN_SEL_ADDR         MT6337_BUCK_ALL_CON2
#define MT6337_PMIC_BUCK_BUCK_VSLEEP_SRCLKEN_SEL_MASK         0x3
#define MT6337_PMIC_BUCK_BUCK_VSLEEP_SRCLKEN_SEL_SHIFT        8
#define MT6337_PMIC_BUCK_ALL_CON2_RSV0_ADDR                   MT6337_BUCK_ALL_CON2
#define MT6337_PMIC_BUCK_ALL_CON2_RSV0_MASK                   0x1F
#define MT6337_PMIC_BUCK_ALL_CON2_RSV0_SHIFT                  11
#define MT6337_PMIC_BUCK_VSLEEP_SRC0_ADDR                     MT6337_BUCK_ALL_CON3
#define MT6337_PMIC_BUCK_VSLEEP_SRC0_MASK                     0x1FF
#define MT6337_PMIC_BUCK_VSLEEP_SRC0_SHIFT                    0
#define MT6337_PMIC_BUCK_VSLEEP_SRC1_ADDR                     MT6337_BUCK_ALL_CON3
#define MT6337_PMIC_BUCK_VSLEEP_SRC1_MASK                     0xF
#define MT6337_PMIC_BUCK_VSLEEP_SRC1_SHIFT                    12
#define MT6337_PMIC_BUCK_R2R_SRC0_ADDR                        MT6337_BUCK_ALL_CON4
#define MT6337_PMIC_BUCK_R2R_SRC0_MASK                        0x1FF
#define MT6337_PMIC_BUCK_R2R_SRC0_SHIFT                       0
#define MT6337_PMIC_BUCK_R2R_SRC1_ADDR                        MT6337_BUCK_ALL_CON4
#define MT6337_PMIC_BUCK_R2R_SRC1_MASK                        0xF
#define MT6337_PMIC_BUCK_R2R_SRC1_SHIFT                       12
#define MT6337_PMIC_RG_OSC_SEL_DLY_MAX_ADDR                   MT6337_BUCK_ALL_CON5
#define MT6337_PMIC_RG_OSC_SEL_DLY_MAX_MASK                   0x1FF
#define MT6337_PMIC_RG_OSC_SEL_DLY_MAX_SHIFT                  0
#define MT6337_PMIC_BUCK_SRCLKEN_DLY_SRC1_ADDR                MT6337_BUCK_ALL_CON5
#define MT6337_PMIC_BUCK_SRCLKEN_DLY_SRC1_MASK                0xF
#define MT6337_PMIC_BUCK_SRCLKEN_DLY_SRC1_SHIFT               12
#define MT6337_PMIC_RG_EFUSE_MAN_RST_ADDR                     MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_EFUSE_MAN_RST_MASK                     0x1
#define MT6337_PMIC_RG_EFUSE_MAN_RST_SHIFT                    0
#define MT6337_PMIC_RG_AUXADC_RST_ADDR                        MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_AUXADC_RST_MASK                        0x1
#define MT6337_PMIC_RG_AUXADC_RST_SHIFT                       1
#define MT6337_PMIC_RG_AUXADC_REG_RST_ADDR                    MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_AUXADC_REG_RST_MASK                    0x1
#define MT6337_PMIC_RG_AUXADC_REG_RST_SHIFT                   2
#define MT6337_PMIC_RG_AUDIO_RST_ADDR                         MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_AUDIO_RST_MASK                         0x1
#define MT6337_PMIC_RG_AUDIO_RST_SHIFT                        3
#define MT6337_PMIC_RG_ACCDET_RST_ADDR                        MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_ACCDET_RST_MASK                        0x1
#define MT6337_PMIC_RG_ACCDET_RST_SHIFT                       4
#define MT6337_PMIC_RG_INTCTL_RST_ADDR                        MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_INTCTL_RST_MASK                        0x1
#define MT6337_PMIC_RG_INTCTL_RST_SHIFT                       5
#define MT6337_PMIC_RG_BUCK_CALI_RST_ADDR                     MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_BUCK_CALI_RST_MASK                     0x1
#define MT6337_PMIC_RG_BUCK_CALI_RST_SHIFT                    6
#define MT6337_PMIC_RG_CLK_TRIM_RST_ADDR                      MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_CLK_TRIM_RST_MASK                      0x1
#define MT6337_PMIC_RG_CLK_TRIM_RST_SHIFT                     7
#define MT6337_PMIC_RG_FQMTR_RST_ADDR                         MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_FQMTR_RST_MASK                         0x1
#define MT6337_PMIC_RG_FQMTR_RST_SHIFT                        8
#define MT6337_PMIC_RG_LDO_CALI_RST_ADDR                      MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_LDO_CALI_RST_MASK                      0x1
#define MT6337_PMIC_RG_LDO_CALI_RST_SHIFT                     9
#define MT6337_PMIC_RG_ZCD_RST_ADDR                           MT6337_TOP_RST_CON0
#define MT6337_PMIC_RG_ZCD_RST_MASK                           0x1
#define MT6337_PMIC_RG_ZCD_RST_SHIFT                          10
#define MT6337_PMIC_TOP_RST_CON0_SET_ADDR                     MT6337_TOP_RST_CON0_SET
#define MT6337_PMIC_TOP_RST_CON0_SET_MASK                     0xFFFF
#define MT6337_PMIC_TOP_RST_CON0_SET_SHIFT                    0
#define MT6337_PMIC_TOP_RST_CON0_CLR_ADDR                     MT6337_TOP_RST_CON0_CLR
#define MT6337_PMIC_TOP_RST_CON0_CLR_MASK                     0xFFFF
#define MT6337_PMIC_TOP_RST_CON0_CLR_SHIFT                    0
#define MT6337_PMIC_RG_STRUP_LONG_PRESS_RST_ADDR              MT6337_TOP_RST_CON1
#define MT6337_PMIC_RG_STRUP_LONG_PRESS_RST_MASK              0x1
#define MT6337_PMIC_RG_STRUP_LONG_PRESS_RST_SHIFT             0
#define MT6337_PMIC_TOP_RST_CON1_RSV_ADDR                     MT6337_TOP_RST_CON1
#define MT6337_PMIC_TOP_RST_CON1_RSV_MASK                     0x1F
#define MT6337_PMIC_TOP_RST_CON1_RSV_SHIFT                    1
#define MT6337_PMIC_TOP_RST_CON1_SET_ADDR                     MT6337_TOP_RST_CON1_SET
#define MT6337_PMIC_TOP_RST_CON1_SET_MASK                     0xFFFF
#define MT6337_PMIC_TOP_RST_CON1_SET_SHIFT                    0
#define MT6337_PMIC_TOP_RST_CON1_CLR_ADDR                     MT6337_TOP_RST_CON1_CLR
#define MT6337_PMIC_TOP_RST_CON1_CLR_MASK                     0xFFFF
#define MT6337_PMIC_TOP_RST_CON1_CLR_SHIFT                    0
#define MT6337_PMIC_TOP_RST_CON2_RSV_ADDR                     MT6337_TOP_RST_CON2
#define MT6337_PMIC_TOP_RST_CON2_RSV_MASK                     0xF
#define MT6337_PMIC_TOP_RST_CON2_RSV_SHIFT                    4
#define MT6337_PMIC_RG_WDTRSTB_EN_ADDR                        MT6337_TOP_RST_MISC
#define MT6337_PMIC_RG_WDTRSTB_EN_MASK                        0x1
#define MT6337_PMIC_RG_WDTRSTB_EN_SHIFT                       0
#define MT6337_PMIC_RG_WDTRSTB_MODE_ADDR                      MT6337_TOP_RST_MISC
#define MT6337_PMIC_RG_WDTRSTB_MODE_MASK                      0x1
#define MT6337_PMIC_RG_WDTRSTB_MODE_SHIFT                     1
#define MT6337_PMIC_WDTRSTB_STATUS_ADDR                       MT6337_TOP_RST_MISC
#define MT6337_PMIC_WDTRSTB_STATUS_MASK                       0x1
#define MT6337_PMIC_WDTRSTB_STATUS_SHIFT                      2
#define MT6337_PMIC_WDTRSTB_STATUS_CLR_ADDR                   MT6337_TOP_RST_MISC
#define MT6337_PMIC_WDTRSTB_STATUS_CLR_MASK                   0x1
#define MT6337_PMIC_WDTRSTB_STATUS_CLR_SHIFT                  3
#define MT6337_PMIC_RG_WDTRSTB_FB_EN_ADDR                     MT6337_TOP_RST_MISC
#define MT6337_PMIC_RG_WDTRSTB_FB_EN_MASK                     0x1
#define MT6337_PMIC_RG_WDTRSTB_FB_EN_SHIFT                    4
#define MT6337_PMIC_RG_WDTRSTB_DEB_ADDR                       MT6337_TOP_RST_MISC
#define MT6337_PMIC_RG_WDTRSTB_DEB_MASK                       0x1
#define MT6337_PMIC_RG_WDTRSTB_DEB_SHIFT                      5
#define MT6337_PMIC_TOP_RST_MISC_RSV_ADDR                     MT6337_TOP_RST_MISC
#define MT6337_PMIC_TOP_RST_MISC_RSV_MASK                     0x3
#define MT6337_PMIC_TOP_RST_MISC_RSV_SHIFT                    14
#define MT6337_PMIC_TOP_RST_MISC_SET_ADDR                     MT6337_TOP_RST_MISC_SET
#define MT6337_PMIC_TOP_RST_MISC_SET_MASK                     0xFFFF
#define MT6337_PMIC_TOP_RST_MISC_SET_SHIFT                    0
#define MT6337_PMIC_TOP_RST_MISC_CLR_ADDR                     MT6337_TOP_RST_MISC_CLR
#define MT6337_PMIC_TOP_RST_MISC_CLR_MASK                     0xFFFF
#define MT6337_PMIC_TOP_RST_MISC_CLR_SHIFT                    0
#define MT6337_PMIC_VPWRIN_RSTB_STATUS_ADDR                   MT6337_TOP_RST_STATUS
#define MT6337_PMIC_VPWRIN_RSTB_STATUS_MASK                   0x1
#define MT6337_PMIC_VPWRIN_RSTB_STATUS_SHIFT                  0
#define MT6337_PMIC_TOP_RST_STATUS_RSV_ADDR                   MT6337_TOP_RST_STATUS
#define MT6337_PMIC_TOP_RST_STATUS_RSV_MASK                   0x7
#define MT6337_PMIC_TOP_RST_STATUS_RSV_SHIFT                  1
#define MT6337_PMIC_TOP_RST_STATUS_SET_ADDR                   MT6337_TOP_RST_STATUS_SET
#define MT6337_PMIC_TOP_RST_STATUS_SET_MASK                   0xFFFF
#define MT6337_PMIC_TOP_RST_STATUS_SET_SHIFT                  0
#define MT6337_PMIC_TOP_RST_STATUS_CLR_ADDR                   MT6337_TOP_RST_STATUS_CLR
#define MT6337_PMIC_TOP_RST_STATUS_CLR_MASK                   0xFFFF
#define MT6337_PMIC_TOP_RST_STATUS_CLR_SHIFT                  0
#define MT6337_PMIC_RG_INT_EN_THR_H_ADDR                      MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_THR_H_MASK                      0x1
#define MT6337_PMIC_RG_INT_EN_THR_H_SHIFT                     0
#define MT6337_PMIC_RG_INT_EN_THR_L_ADDR                      MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_THR_L_MASK                      0x1
#define MT6337_PMIC_RG_INT_EN_THR_L_SHIFT                     1
#define MT6337_PMIC_RG_INT_EN_AUDIO_ADDR                      MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_AUDIO_MASK                      0x1
#define MT6337_PMIC_RG_INT_EN_AUDIO_SHIFT                     2
#define MT6337_PMIC_RG_INT_EN_MAD_ADDR                        MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_MAD_MASK                        0x1
#define MT6337_PMIC_RG_INT_EN_MAD_SHIFT                       3
#define MT6337_PMIC_RG_INT_EN_ACCDET_ADDR                     MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_ACCDET_MASK                     0x1
#define MT6337_PMIC_RG_INT_EN_ACCDET_SHIFT                    4
#define MT6337_PMIC_RG_INT_EN_ACCDET_EINT_ADDR                MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_ACCDET_EINT_MASK                0x1
#define MT6337_PMIC_RG_INT_EN_ACCDET_EINT_SHIFT               5
#define MT6337_PMIC_RG_INT_EN_ACCDET_EINT1_ADDR               MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_ACCDET_EINT1_MASK               0x1
#define MT6337_PMIC_RG_INT_EN_ACCDET_EINT1_SHIFT              6
#define MT6337_PMIC_RG_INT_EN_ACCDET_NEGV_ADDR                MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_ACCDET_NEGV_MASK                0x1
#define MT6337_PMIC_RG_INT_EN_ACCDET_NEGV_SHIFT               7
#define MT6337_PMIC_RG_INT_EN_PMU_THR_ADDR                    MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_PMU_THR_MASK                    0x1
#define MT6337_PMIC_RG_INT_EN_PMU_THR_SHIFT                   8
#define MT6337_PMIC_RG_INT_EN_LDO_VA18_OC_ADDR                MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_LDO_VA18_OC_MASK                0x1
#define MT6337_PMIC_RG_INT_EN_LDO_VA18_OC_SHIFT               9
#define MT6337_PMIC_RG_INT_EN_LDO_VA25_OC_ADDR                MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_LDO_VA25_OC_MASK                0x1
#define MT6337_PMIC_RG_INT_EN_LDO_VA25_OC_SHIFT               10
#define MT6337_PMIC_RG_INT_EN_LDO_VA18_PG_ADDR                MT6337_INT_CON0
#define MT6337_PMIC_RG_INT_EN_LDO_VA18_PG_MASK                0x1
#define MT6337_PMIC_RG_INT_EN_LDO_VA18_PG_SHIFT               11
#define MT6337_PMIC_INT_CON0_SET_ADDR                         MT6337_INT_CON0_SET
#define MT6337_PMIC_INT_CON0_SET_MASK                         0xFFFF
#define MT6337_PMIC_INT_CON0_SET_SHIFT                        0
#define MT6337_PMIC_INT_CON0_CLR_ADDR                         MT6337_INT_CON0_CLR
#define MT6337_PMIC_INT_CON0_CLR_MASK                         0xFFFF
#define MT6337_PMIC_INT_CON0_CLR_SHIFT                        0
#define MT6337_PMIC_RG_INT_MASK_B_THR_H_ADDR                  MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_THR_H_MASK                  0x1
#define MT6337_PMIC_RG_INT_MASK_B_THR_H_SHIFT                 0
#define MT6337_PMIC_RG_INT_MASK_B_THR_L_ADDR                  MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_THR_L_MASK                  0x1
#define MT6337_PMIC_RG_INT_MASK_B_THR_L_SHIFT                 1
#define MT6337_PMIC_RG_INT_MASK_B_AUDIO_ADDR                  MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_AUDIO_MASK                  0x1
#define MT6337_PMIC_RG_INT_MASK_B_AUDIO_SHIFT                 2
#define MT6337_PMIC_RG_INT_MASK_B_MAD_ADDR                    MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_MAD_MASK                    0x1
#define MT6337_PMIC_RG_INT_MASK_B_MAD_SHIFT                   3
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_ADDR                 MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_MASK                 0x1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_SHIFT                4
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT_ADDR            MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT_MASK            0x1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT_SHIFT           5
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT1_ADDR           MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT1_MASK           0x1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT1_SHIFT          6
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_NEGV_ADDR            MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_NEGV_MASK            0x1
#define MT6337_PMIC_RG_INT_MASK_B_ACCDET_NEGV_SHIFT           7
#define MT6337_PMIC_RG_INT_MASK_B_PMU_THR_ADDR                MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_PMU_THR_MASK                0x1
#define MT6337_PMIC_RG_INT_MASK_B_PMU_THR_SHIFT               8
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_OC_ADDR            MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_OC_MASK            0x1
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_OC_SHIFT           9
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA25_OC_ADDR            MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA25_OC_MASK            0x1
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA25_OC_SHIFT           10
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_PG_ADDR            MT6337_INT_CON1
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_PG_MASK            0x1
#define MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_PG_SHIFT           11
#define MT6337_PMIC_INT_CON1_SET_ADDR                         MT6337_INT_CON1_SET
#define MT6337_PMIC_INT_CON1_SET_MASK                         0xFFFF
#define MT6337_PMIC_INT_CON1_SET_SHIFT                        0
#define MT6337_PMIC_INT_CON1_CLR_ADDR                         MT6337_INT_CON1_CLR
#define MT6337_PMIC_INT_CON1_CLR_MASK                         0xFFFF
#define MT6337_PMIC_INT_CON1_CLR_SHIFT                        0
#define MT6337_PMIC_RG_INT_SEL_THR_H_ADDR                     MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_THR_H_MASK                     0x1
#define MT6337_PMIC_RG_INT_SEL_THR_H_SHIFT                    0
#define MT6337_PMIC_RG_INT_SEL_THR_L_ADDR                     MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_THR_L_MASK                     0x1
#define MT6337_PMIC_RG_INT_SEL_THR_L_SHIFT                    1
#define MT6337_PMIC_RG_INT_SEL_AUDIO_ADDR                     MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_AUDIO_MASK                     0x1
#define MT6337_PMIC_RG_INT_SEL_AUDIO_SHIFT                    2
#define MT6337_PMIC_RG_INT_SEL_MAD_ADDR                       MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_MAD_MASK                       0x1
#define MT6337_PMIC_RG_INT_SEL_MAD_SHIFT                      3
#define MT6337_PMIC_RG_INT_SEL_ACCDET_ADDR                    MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_ACCDET_MASK                    0x1
#define MT6337_PMIC_RG_INT_SEL_ACCDET_SHIFT                   4
#define MT6337_PMIC_RG_INT_SEL_ACCDET_EINT_ADDR               MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_ACCDET_EINT_MASK               0x1
#define MT6337_PMIC_RG_INT_SEL_ACCDET_EINT_SHIFT              5
#define MT6337_PMIC_RG_INT_SEL_ACCDET_EINT1_ADDR              MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_ACCDET_EINT1_MASK              0x1
#define MT6337_PMIC_RG_INT_SEL_ACCDET_EINT1_SHIFT             6
#define MT6337_PMIC_RG_INT_SEL_ACCDET_NEGV_ADDR               MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_ACCDET_NEGV_MASK               0x1
#define MT6337_PMIC_RG_INT_SEL_ACCDET_NEGV_SHIFT              7
#define MT6337_PMIC_RG_INT_SEL_PMU_THR_ADDR                   MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_PMU_THR_MASK                   0x1
#define MT6337_PMIC_RG_INT_SEL_PMU_THR_SHIFT                  8
#define MT6337_PMIC_RG_INT_SEL_LDO_VA18_OC_ADDR               MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_LDO_VA18_OC_MASK               0x1
#define MT6337_PMIC_RG_INT_SEL_LDO_VA18_OC_SHIFT              9
#define MT6337_PMIC_RG_INT_SEL_LDO_VA25_OC_ADDR               MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_LDO_VA25_OC_MASK               0x1
#define MT6337_PMIC_RG_INT_SEL_LDO_VA25_OC_SHIFT              10
#define MT6337_PMIC_RG_INT_SEL_LDO_VA18_PG_ADDR               MT6337_INT_CON2
#define MT6337_PMIC_RG_INT_SEL_LDO_VA18_PG_MASK               0x1
#define MT6337_PMIC_RG_INT_SEL_LDO_VA18_PG_SHIFT              11
#define MT6337_PMIC_INT_CON2_SET_ADDR                         MT6337_INT_CON2_SET
#define MT6337_PMIC_INT_CON2_SET_MASK                         0xFFFF
#define MT6337_PMIC_INT_CON2_SET_SHIFT                        0
#define MT6337_PMIC_INT_CON2_CLR_ADDR                         MT6337_INT_CON2_CLR
#define MT6337_PMIC_INT_CON2_CLR_MASK                         0xFFFF
#define MT6337_PMIC_INT_CON2_CLR_SHIFT                        0
#define MT6337_PMIC_POLARITY_ADDR                             MT6337_INT_MISC_CON
#define MT6337_PMIC_POLARITY_MASK                             0x1
#define MT6337_PMIC_POLARITY_SHIFT                            0
#define MT6337_PMIC_RG_PCHR_CM_VDEC_POLARITY_RSV_ADDR         MT6337_INT_MISC_CON
#define MT6337_PMIC_RG_PCHR_CM_VDEC_POLARITY_RSV_MASK         0x1
#define MT6337_PMIC_RG_PCHR_CM_VDEC_POLARITY_RSV_SHIFT        5
#define MT6337_PMIC_INT_MISC_CON_SET_ADDR                     MT6337_INT_MISC_CON_SET
#define MT6337_PMIC_INT_MISC_CON_SET_MASK                     0xFFFF
#define MT6337_PMIC_INT_MISC_CON_SET_SHIFT                    0
#define MT6337_PMIC_INT_MISC_CON_CLR_ADDR                     MT6337_INT_MISC_CON_CLR
#define MT6337_PMIC_INT_MISC_CON_CLR_MASK                     0xFFFF
#define MT6337_PMIC_INT_MISC_CON_CLR_SHIFT                    0
#define MT6337_PMIC_RG_INT_STATUS_THR_H_ADDR                  MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_THR_H_MASK                  0x1
#define MT6337_PMIC_RG_INT_STATUS_THR_H_SHIFT                 0
#define MT6337_PMIC_RG_INT_STATUS_THR_L_ADDR                  MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_THR_L_MASK                  0x1
#define MT6337_PMIC_RG_INT_STATUS_THR_L_SHIFT                 1
#define MT6337_PMIC_RG_INT_STATUS_AUDIO_ADDR                  MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_AUDIO_MASK                  0x1
#define MT6337_PMIC_RG_INT_STATUS_AUDIO_SHIFT                 2
#define MT6337_PMIC_RG_INT_STATUS_MAD_ADDR                    MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_MAD_MASK                    0x1
#define MT6337_PMIC_RG_INT_STATUS_MAD_SHIFT                   3
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_ADDR                 MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_MASK                 0x1
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_SHIFT                4
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT_ADDR            MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT_MASK            0x1
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT_SHIFT           5
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT1_ADDR           MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT1_MASK           0x1
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT1_SHIFT          6
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_NEGV_ADDR            MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_NEGV_MASK            0x1
#define MT6337_PMIC_RG_INT_STATUS_ACCDET_NEGV_SHIFT           7
#define MT6337_PMIC_RG_INT_STATUS_PMU_THR_ADDR                MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_PMU_THR_MASK                0x1
#define MT6337_PMIC_RG_INT_STATUS_PMU_THR_SHIFT               8
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA18_OC_ADDR            MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA18_OC_MASK            0x1
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA18_OC_SHIFT           9
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA25_OC_ADDR            MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA25_OC_MASK            0x1
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA25_OC_SHIFT           10
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA18_PG_ADDR            MT6337_INT_STATUS0
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA18_PG_MASK            0x1
#define MT6337_PMIC_RG_INT_STATUS_LDO_VA18_PG_SHIFT           11
#define MT6337_PMIC_RG_THR_DET_DIS_ADDR                       MT6337_STRUP_CON0
#define MT6337_PMIC_RG_THR_DET_DIS_MASK                       0x1
#define MT6337_PMIC_RG_THR_DET_DIS_SHIFT                      0
#define MT6337_PMIC_RG_THR_TEST_ADDR                          MT6337_STRUP_CON0
#define MT6337_PMIC_RG_THR_TEST_MASK                          0x3
#define MT6337_PMIC_RG_THR_TEST_SHIFT                         1
#define MT6337_PMIC_THR_HWPDN_EN_ADDR                         MT6337_STRUP_CON0
#define MT6337_PMIC_THR_HWPDN_EN_MASK                         0x1
#define MT6337_PMIC_THR_HWPDN_EN_SHIFT                        5
#define MT6337_PMIC_RG_THERMAL_DEB_SEL_ADDR                   MT6337_STRUP_CON0
#define MT6337_PMIC_RG_THERMAL_DEB_SEL_MASK                   0x3
#define MT6337_PMIC_RG_THERMAL_DEB_SEL_SHIFT                  14
#define MT6337_PMIC_RG_VA18_PG_DEB_SEL_ADDR                   MT6337_STRUP_CON1
#define MT6337_PMIC_RG_VA18_PG_DEB_SEL_MASK                   0x3
#define MT6337_PMIC_RG_VA18_PG_DEB_SEL_SHIFT                  0
#define MT6337_PMIC_STRUP_CON4_RSV_ADDR                       MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_CON4_RSV_MASK                       0x3
#define MT6337_PMIC_STRUP_CON4_RSV_SHIFT                      0
#define MT6337_PMIC_STRUP_OSC_EN_ADDR                         MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_OSC_EN_MASK                         0x1
#define MT6337_PMIC_STRUP_OSC_EN_SHIFT                        2
#define MT6337_PMIC_STRUP_OSC_EN_SEL_ADDR                     MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_OSC_EN_SEL_MASK                     0x1
#define MT6337_PMIC_STRUP_OSC_EN_SEL_SHIFT                    3
#define MT6337_PMIC_STRUP_FT_CTRL_ADDR                        MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_FT_CTRL_MASK                        0x3
#define MT6337_PMIC_STRUP_FT_CTRL_SHIFT                       4
#define MT6337_PMIC_STRUP_PWRON_FORCE_ADDR                    MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_PWRON_FORCE_MASK                    0x1
#define MT6337_PMIC_STRUP_PWRON_FORCE_SHIFT                   6
#define MT6337_PMIC_BIAS_GEN_EN_FORCE_ADDR                    MT6337_STRUP_CON4
#define MT6337_PMIC_BIAS_GEN_EN_FORCE_MASK                    0x1
#define MT6337_PMIC_BIAS_GEN_EN_FORCE_SHIFT                   7
#define MT6337_PMIC_STRUP_PWRON_ADDR                          MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_PWRON_MASK                          0x1
#define MT6337_PMIC_STRUP_PWRON_SHIFT                         8
#define MT6337_PMIC_STRUP_PWRON_SEL_ADDR                      MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_PWRON_SEL_MASK                      0x1
#define MT6337_PMIC_STRUP_PWRON_SEL_SHIFT                     9
#define MT6337_PMIC_BIAS_GEN_EN_ADDR                          MT6337_STRUP_CON4
#define MT6337_PMIC_BIAS_GEN_EN_MASK                          0x1
#define MT6337_PMIC_BIAS_GEN_EN_SHIFT                         10
#define MT6337_PMIC_BIAS_GEN_EN_SEL_ADDR                      MT6337_STRUP_CON4
#define MT6337_PMIC_BIAS_GEN_EN_SEL_MASK                      0x1
#define MT6337_PMIC_BIAS_GEN_EN_SEL_SHIFT                     11
#define MT6337_PMIC_VBGSW_ENB_EN_ADDR                         MT6337_STRUP_CON4
#define MT6337_PMIC_VBGSW_ENB_EN_MASK                         0x1
#define MT6337_PMIC_VBGSW_ENB_EN_SHIFT                        12
#define MT6337_PMIC_VBGSW_ENB_EN_SEL_ADDR                     MT6337_STRUP_CON4
#define MT6337_PMIC_VBGSW_ENB_EN_SEL_MASK                     0x1
#define MT6337_PMIC_VBGSW_ENB_EN_SEL_SHIFT                    13
#define MT6337_PMIC_VBGSW_ENB_FORCE_ADDR                      MT6337_STRUP_CON4
#define MT6337_PMIC_VBGSW_ENB_FORCE_MASK                      0x1
#define MT6337_PMIC_VBGSW_ENB_FORCE_SHIFT                     14
#define MT6337_PMIC_STRUP_DIG_IO_PG_FORCE_ADDR                MT6337_STRUP_CON4
#define MT6337_PMIC_STRUP_DIG_IO_PG_FORCE_MASK                0x1
#define MT6337_PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT               15
#define MT6337_PMIC_RG_STRUP_VA18_EN_ADDR                     MT6337_STRUP_CON5
#define MT6337_PMIC_RG_STRUP_VA18_EN_MASK                     0x1
#define MT6337_PMIC_RG_STRUP_VA18_EN_SHIFT                    2
#define MT6337_PMIC_RG_STRUP_VA18_EN_SEL_ADDR                 MT6337_STRUP_CON5
#define MT6337_PMIC_RG_STRUP_VA18_EN_SEL_MASK                 0x1
#define MT6337_PMIC_RG_STRUP_VA18_EN_SEL_SHIFT                3
#define MT6337_PMIC_RG_STRUP_VA18_STB_ADDR                    MT6337_STRUP_CON5
#define MT6337_PMIC_RG_STRUP_VA18_STB_MASK                    0x1
#define MT6337_PMIC_RG_STRUP_VA18_STB_SHIFT                   4
#define MT6337_PMIC_RG_STRUP_VA18_STB_SEL_ADDR                MT6337_STRUP_CON5
#define MT6337_PMIC_RG_STRUP_VA18_STB_SEL_MASK                0x1
#define MT6337_PMIC_RG_STRUP_VA18_STB_SEL_SHIFT               5
#define MT6337_PMIC_RG_VA18_PGST_ADDR                         MT6337_STRUP_CON5
#define MT6337_PMIC_RG_VA18_PGST_MASK                         0x1
#define MT6337_PMIC_RG_VA18_PGST_SHIFT                        6
#define MT6337_PMIC_RG_VA18_PGST_SEL_ADDR                     MT6337_STRUP_CON5
#define MT6337_PMIC_RG_VA18_PGST_SEL_MASK                     0x1
#define MT6337_PMIC_RG_VA18_PGST_SEL_SHIFT                    7
#define MT6337_PMIC_RG_VA18_PGST_AUDENC_ADDR                  MT6337_STRUP_CON5
#define MT6337_PMIC_RG_VA18_PGST_AUDENC_MASK                  0x1
#define MT6337_PMIC_RG_VA18_PGST_AUDENC_SHIFT                 8
#define MT6337_PMIC_RG_VA18_PGST_AUDENC_SEL_ADDR              MT6337_STRUP_CON5
#define MT6337_PMIC_RG_VA18_PGST_AUDENC_SEL_MASK              0x1
#define MT6337_PMIC_RG_VA18_PGST_AUDENC_SEL_SHIFT             9
#define MT6337_PMIC_DA_QI_OSC_EN_ADDR                         MT6337_STRUP_CON5
#define MT6337_PMIC_DA_QI_OSC_EN_MASK                         0x1
#define MT6337_PMIC_DA_QI_OSC_EN_SHIFT                        15
#define MT6337_PMIC_RG_BGR_UNCHOP_ADDR                        MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_UNCHOP_MASK                        0x1
#define MT6337_PMIC_RG_BGR_UNCHOP_SHIFT                       0
#define MT6337_PMIC_RG_BGR_UNCHOP_PH_ADDR                     MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_UNCHOP_PH_MASK                     0x1
#define MT6337_PMIC_RG_BGR_UNCHOP_PH_SHIFT                    1
#define MT6337_PMIC_RG_BGR_RSEL_ADDR                          MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_RSEL_MASK                          0x7
#define MT6337_PMIC_RG_BGR_RSEL_SHIFT                         2
#define MT6337_PMIC_RG_BGR_TRIM_ADDR                          MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_TRIM_MASK                          0x1F
#define MT6337_PMIC_RG_BGR_TRIM_SHIFT                         5
#define MT6337_PMIC_RG_BGR_TRIM_EN_ADDR                       MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_TRIM_EN_MASK                       0x1
#define MT6337_PMIC_RG_BGR_TRIM_EN_SHIFT                      10
#define MT6337_PMIC_RG_BGR_TEST_RSTB_ADDR                     MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_TEST_RSTB_MASK                     0x1
#define MT6337_PMIC_RG_BGR_TEST_RSTB_SHIFT                    11
#define MT6337_PMIC_RG_BGR_TEST_EN_ADDR                       MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_TEST_EN_MASK                       0x1
#define MT6337_PMIC_RG_BGR_TEST_EN_SHIFT                      12
#define MT6337_PMIC_RG_BYPASSMODESEL_ADDR                     MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BYPASSMODESEL_MASK                     0x1
#define MT6337_PMIC_RG_BYPASSMODESEL_SHIFT                    13
#define MT6337_PMIC_RG_BYPASSMODEEN_ADDR                      MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BYPASSMODEEN_MASK                      0x1
#define MT6337_PMIC_RG_BYPASSMODEEN_SHIFT                     14
#define MT6337_PMIC_RG_BGR_OSC_EN_TEST_ADDR                   MT6337_TSBG_ANA_CON0
#define MT6337_PMIC_RG_BGR_OSC_EN_TEST_MASK                   0x1
#define MT6337_PMIC_RG_BGR_OSC_EN_TEST_SHIFT                  15
#define MT6337_PMIC_RG_TSENS_EN_ADDR                          MT6337_TSBG_ANA_CON1
#define MT6337_PMIC_RG_TSENS_EN_MASK                          0x1
#define MT6337_PMIC_RG_TSENS_EN_SHIFT                         0
#define MT6337_PMIC_RG_SPAREBGR_ADDR                          MT6337_TSBG_ANA_CON1
#define MT6337_PMIC_RG_SPAREBGR_MASK                          0xF
#define MT6337_PMIC_RG_SPAREBGR_SHIFT                         1
#define MT6337_PMIC_RG_STRUP_IREF_TRIM_ADDR                   MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_STRUP_IREF_TRIM_MASK                   0x1F
#define MT6337_PMIC_RG_STRUP_IREF_TRIM_SHIFT                  0
#define MT6337_PMIC_RG_VREF_BG_ADDR                           MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_VREF_BG_MASK                           0x7
#define MT6337_PMIC_RG_VREF_BG_SHIFT                          5
#define MT6337_PMIC_RG_STRUP_THR_SEL_ADDR                     MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_STRUP_THR_SEL_MASK                     0x3
#define MT6337_PMIC_RG_STRUP_THR_SEL_SHIFT                    8
#define MT6337_PMIC_RG_THR_TMODE_ADDR                         MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_THR_TMODE_MASK                         0x1
#define MT6337_PMIC_RG_THR_TMODE_SHIFT                        10
#define MT6337_PMIC_RG_THRDET_SEL_ADDR                        MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_THRDET_SEL_MASK                        0x1
#define MT6337_PMIC_RG_THRDET_SEL_SHIFT                       11
#define MT6337_PMIC_RG_VTHR_POL_ADDR                          MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_VTHR_POL_MASK                          0x7
#define MT6337_PMIC_RG_VTHR_POL_SHIFT                         12
#define MT6337_PMIC_RG_SPAREIVGEN_ADDR                        MT6337_IVGEN_ANA_CON0
#define MT6337_PMIC_RG_SPAREIVGEN_MASK                        0x1
#define MT6337_PMIC_RG_SPAREIVGEN_SHIFT                       15
#define MT6337_PMIC_RG_OVT_EN_ADDR                            MT6337_IVGEN_ANA_CON1
#define MT6337_PMIC_RG_OVT_EN_MASK                            0x1
#define MT6337_PMIC_RG_OVT_EN_SHIFT                           0
#define MT6337_PMIC_RG_IVGEN_EXT_TEST_EN_ADDR                 MT6337_IVGEN_ANA_CON1
#define MT6337_PMIC_RG_IVGEN_EXT_TEST_EN_MASK                 0x1
#define MT6337_PMIC_RG_IVGEN_EXT_TEST_EN_SHIFT                1
#define MT6337_PMIC_RG_TOPSPAREVA18_ADDR                      MT6337_STRUP_ANA_CON0
#define MT6337_PMIC_RG_TOPSPAREVA18_MASK                      0xFFFF
#define MT6337_PMIC_RG_TOPSPAREVA18_SHIFT                     0
#define MT6337_PMIC_RGS_ANA_CHIP_ID_ADDR                      MT6337_STRUP_ANA_CON1
#define MT6337_PMIC_RGS_ANA_CHIP_ID_MASK                      0x7
#define MT6337_PMIC_RGS_ANA_CHIP_ID_SHIFT                     0
#define MT6337_PMIC_RG_SLP_RW_EN_ADDR                         MT6337_RG_SPI_CON
#define MT6337_PMIC_RG_SLP_RW_EN_MASK                         0x1
#define MT6337_PMIC_RG_SLP_RW_EN_SHIFT                        0
#define MT6337_PMIC_RG_SPI_RSV_ADDR                           MT6337_RG_SPI_CON
#define MT6337_PMIC_RG_SPI_RSV_MASK                           0x7FFF
#define MT6337_PMIC_RG_SPI_RSV_SHIFT                          1
#define MT6337_PMIC_DEW_DIO_EN_ADDR                           MT6337_DEW_DIO_EN
#define MT6337_PMIC_DEW_DIO_EN_MASK                           0x1
#define MT6337_PMIC_DEW_DIO_EN_SHIFT                          0
#define MT6337_PMIC_DEW_READ_TEST_ADDR                        MT6337_DEW_READ_TEST
#define MT6337_PMIC_DEW_READ_TEST_MASK                        0xFFFF
#define MT6337_PMIC_DEW_READ_TEST_SHIFT                       0
#define MT6337_PMIC_DEW_WRITE_TEST_ADDR                       MT6337_DEW_WRITE_TEST
#define MT6337_PMIC_DEW_WRITE_TEST_MASK                       0xFFFF
#define MT6337_PMIC_DEW_WRITE_TEST_SHIFT                      0
#define MT6337_PMIC_DEW_CRC_SWRST_ADDR                        MT6337_DEW_CRC_SWRST
#define MT6337_PMIC_DEW_CRC_SWRST_MASK                        0x1
#define MT6337_PMIC_DEW_CRC_SWRST_SHIFT                       0
#define MT6337_PMIC_DEW_CRC_EN_ADDR                           MT6337_DEW_CRC_EN
#define MT6337_PMIC_DEW_CRC_EN_MASK                           0x1
#define MT6337_PMIC_DEW_CRC_EN_SHIFT                          0
#define MT6337_PMIC_DEW_CRC_VAL_ADDR                          MT6337_DEW_CRC_VAL
#define MT6337_PMIC_DEW_CRC_VAL_MASK                          0xFF
#define MT6337_PMIC_DEW_CRC_VAL_SHIFT                         0
#define MT6337_PMIC_DEW_DBG_MON_SEL_ADDR                      MT6337_DEW_DBG_MON_SEL
#define MT6337_PMIC_DEW_DBG_MON_SEL_MASK                      0xF
#define MT6337_PMIC_DEW_DBG_MON_SEL_SHIFT                     0
#define MT6337_PMIC_DEW_CIPHER_KEY_SEL_ADDR                   MT6337_DEW_CIPHER_KEY_SEL
#define MT6337_PMIC_DEW_CIPHER_KEY_SEL_MASK                   0x3
#define MT6337_PMIC_DEW_CIPHER_KEY_SEL_SHIFT                  0
#define MT6337_PMIC_DEW_CIPHER_IV_SEL_ADDR                    MT6337_DEW_CIPHER_IV_SEL
#define MT6337_PMIC_DEW_CIPHER_IV_SEL_MASK                    0x3
#define MT6337_PMIC_DEW_CIPHER_IV_SEL_SHIFT                   0
#define MT6337_PMIC_DEW_CIPHER_EN_ADDR                        MT6337_DEW_CIPHER_EN
#define MT6337_PMIC_DEW_CIPHER_EN_MASK                        0x1
#define MT6337_PMIC_DEW_CIPHER_EN_SHIFT                       0
#define MT6337_PMIC_DEW_CIPHER_RDY_ADDR                       MT6337_DEW_CIPHER_RDY
#define MT6337_PMIC_DEW_CIPHER_RDY_MASK                       0x1
#define MT6337_PMIC_DEW_CIPHER_RDY_SHIFT                      0
#define MT6337_PMIC_DEW_CIPHER_MODE_ADDR                      MT6337_DEW_CIPHER_MODE
#define MT6337_PMIC_DEW_CIPHER_MODE_MASK                      0x1
#define MT6337_PMIC_DEW_CIPHER_MODE_SHIFT                     0
#define MT6337_PMIC_DEW_CIPHER_SWRST_ADDR                     MT6337_DEW_CIPHER_SWRST
#define MT6337_PMIC_DEW_CIPHER_SWRST_MASK                     0x1
#define MT6337_PMIC_DEW_CIPHER_SWRST_SHIFT                    0
#define MT6337_PMIC_DEW_RDDMY_NO_ADDR                         MT6337_DEW_RDDMY_NO
#define MT6337_PMIC_DEW_RDDMY_NO_MASK                         0xF
#define MT6337_PMIC_DEW_RDDMY_NO_SHIFT                        0
#define MT6337_PMIC_INT_TYPE_CON0_ADDR                        MT6337_INT_TYPE_CON0
#define MT6337_PMIC_INT_TYPE_CON0_MASK                        0xFFFF
#define MT6337_PMIC_INT_TYPE_CON0_SHIFT                       0
#define MT6337_PMIC_INT_TYPE_CON0_SET_ADDR                    MT6337_INT_TYPE_CON0_SET
#define MT6337_PMIC_INT_TYPE_CON0_SET_MASK                    0xFFFF
#define MT6337_PMIC_INT_TYPE_CON0_SET_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON0_CLR_ADDR                    MT6337_INT_TYPE_CON0_CLR
#define MT6337_PMIC_INT_TYPE_CON0_CLR_MASK                    0xFFFF
#define MT6337_PMIC_INT_TYPE_CON0_CLR_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON1_ADDR                        MT6337_INT_TYPE_CON1
#define MT6337_PMIC_INT_TYPE_CON1_MASK                        0xFFFF
#define MT6337_PMIC_INT_TYPE_CON1_SHIFT                       0
#define MT6337_PMIC_INT_TYPE_CON1_SET_ADDR                    MT6337_INT_TYPE_CON1_SET
#define MT6337_PMIC_INT_TYPE_CON1_SET_MASK                    0xFFFF
#define MT6337_PMIC_INT_TYPE_CON1_SET_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON1_CLR_ADDR                    MT6337_INT_TYPE_CON1_CLR
#define MT6337_PMIC_INT_TYPE_CON1_CLR_MASK                    0xFFFF
#define MT6337_PMIC_INT_TYPE_CON1_CLR_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON2_ADDR                        MT6337_INT_TYPE_CON2
#define MT6337_PMIC_INT_TYPE_CON2_MASK                        0xFFFF
#define MT6337_PMIC_INT_TYPE_CON2_SHIFT                       0
#define MT6337_PMIC_INT_TYPE_CON2_SET_ADDR                    MT6337_INT_TYPE_CON2_SET
#define MT6337_PMIC_INT_TYPE_CON2_SET_MASK                    0xFFFF
#define MT6337_PMIC_INT_TYPE_CON2_SET_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON2_CLR_ADDR                    MT6337_INT_TYPE_CON2_CLR
#define MT6337_PMIC_INT_TYPE_CON2_CLR_MASK                    0xFFFF
#define MT6337_PMIC_INT_TYPE_CON2_CLR_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON3_ADDR                        MT6337_INT_TYPE_CON3
#define MT6337_PMIC_INT_TYPE_CON3_MASK                        0x1F
#define MT6337_PMIC_INT_TYPE_CON3_SHIFT                       0
#define MT6337_PMIC_INT_TYPE_CON3_SET_ADDR                    MT6337_INT_TYPE_CON3_SET
#define MT6337_PMIC_INT_TYPE_CON3_SET_MASK                    0x1F
#define MT6337_PMIC_INT_TYPE_CON3_SET_SHIFT                   0
#define MT6337_PMIC_INT_TYPE_CON3_CLR_ADDR                    MT6337_INT_TYPE_CON3_CLR
#define MT6337_PMIC_INT_TYPE_CON3_CLR_MASK                    0x1F
#define MT6337_PMIC_INT_TYPE_CON3_CLR_SHIFT                   0
#define MT6337_PMIC_CPU_INT_STA_ADDR                          MT6337_INT_STA
#define MT6337_PMIC_CPU_INT_STA_MASK                          0x1
#define MT6337_PMIC_CPU_INT_STA_SHIFT                         0
#define MT6337_PMIC_MD32_INT_STA_ADDR                         MT6337_INT_STA
#define MT6337_PMIC_MD32_INT_STA_MASK                         0x1
#define MT6337_PMIC_MD32_INT_STA_SHIFT                        1
#define MT6337_PMIC_RG_SRCLKEN_IN2_SMPS_CLK_MODE_ADDR         MT6337_RG_SPI_CON1
#define MT6337_PMIC_RG_SRCLKEN_IN2_SMPS_CLK_MODE_MASK         0x1
#define MT6337_PMIC_RG_SRCLKEN_IN2_SMPS_CLK_MODE_SHIFT        0
#define MT6337_PMIC_RG_SRCLKEN_IN2_EN_SMPS_TEST_ADDR          MT6337_RG_SPI_CON1
#define MT6337_PMIC_RG_SRCLKEN_IN2_EN_SMPS_TEST_MASK          0x1
#define MT6337_PMIC_RG_SRCLKEN_IN2_EN_SMPS_TEST_SHIFT         1
#define MT6337_PMIC_RG_SPI_DLY_SEL_ADDR                       MT6337_RG_SPI_CON2
#define MT6337_PMIC_RG_SPI_DLY_SEL_MASK                       0x7
#define MT6337_PMIC_RG_SPI_DLY_SEL_SHIFT                      0
#define MT6337_PMIC_FQMTR_TCKSEL_ADDR                         MT6337_FQMTR_CON0
#define MT6337_PMIC_FQMTR_TCKSEL_MASK                         0x7
#define MT6337_PMIC_FQMTR_TCKSEL_SHIFT                        0
#define MT6337_PMIC_FQMTR_BUSY_ADDR                           MT6337_FQMTR_CON0
#define MT6337_PMIC_FQMTR_BUSY_MASK                           0x1
#define MT6337_PMIC_FQMTR_BUSY_SHIFT                          3
#define MT6337_PMIC_FQMTR_EN_ADDR                             MT6337_FQMTR_CON0
#define MT6337_PMIC_FQMTR_EN_MASK                             0x1
#define MT6337_PMIC_FQMTR_EN_SHIFT                            15
#define MT6337_PMIC_FQMTR_WINSET_ADDR                         MT6337_FQMTR_CON1
#define MT6337_PMIC_FQMTR_WINSET_MASK                         0xFFFF
#define MT6337_PMIC_FQMTR_WINSET_SHIFT                        0
#define MT6337_PMIC_FQMTR_DATA_ADDR                           MT6337_FQMTR_CON2
#define MT6337_PMIC_FQMTR_DATA_MASK                           0xFFFF
#define MT6337_PMIC_FQMTR_DATA_SHIFT                          0
#define MT6337_PMIC_BUCK_K_RST_DONE_ADDR                      MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_RST_DONE_MASK                      0x1
#define MT6337_PMIC_BUCK_K_RST_DONE_SHIFT                     0
#define MT6337_PMIC_BUCK_K_MAP_SEL_ADDR                       MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_MAP_SEL_MASK                       0x1
#define MT6337_PMIC_BUCK_K_MAP_SEL_SHIFT                      1
#define MT6337_PMIC_BUCK_K_ONCE_EN_ADDR                       MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_ONCE_EN_MASK                       0x1
#define MT6337_PMIC_BUCK_K_ONCE_EN_SHIFT                      2
#define MT6337_PMIC_BUCK_K_ONCE_ADDR                          MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_ONCE_MASK                          0x1
#define MT6337_PMIC_BUCK_K_ONCE_SHIFT                         3
#define MT6337_PMIC_BUCK_K_START_MANUAL_ADDR                  MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_START_MANUAL_MASK                  0x1
#define MT6337_PMIC_BUCK_K_START_MANUAL_SHIFT                 4
#define MT6337_PMIC_BUCK_K_SRC_SEL_ADDR                       MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_SRC_SEL_MASK                       0x1
#define MT6337_PMIC_BUCK_K_SRC_SEL_SHIFT                      5
#define MT6337_PMIC_BUCK_K_AUTO_EN_ADDR                       MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_AUTO_EN_MASK                       0x1
#define MT6337_PMIC_BUCK_K_AUTO_EN_SHIFT                      6
#define MT6337_PMIC_BUCK_K_INV_ADDR                           MT6337_BUCK_K_CON0
#define MT6337_PMIC_BUCK_K_INV_MASK                           0x1
#define MT6337_PMIC_BUCK_K_INV_SHIFT                          7
#define MT6337_PMIC_BUCK_K_CONTROL_SMPS_ADDR                  MT6337_BUCK_K_CON1
#define MT6337_PMIC_BUCK_K_CONTROL_SMPS_MASK                  0x3F
#define MT6337_PMIC_BUCK_K_CONTROL_SMPS_SHIFT                 8
#define MT6337_PMIC_K_RESULT_ADDR                             MT6337_BUCK_K_CON2
#define MT6337_PMIC_K_RESULT_MASK                             0x1
#define MT6337_PMIC_K_RESULT_SHIFT                            0
#define MT6337_PMIC_K_DONE_ADDR                               MT6337_BUCK_K_CON2
#define MT6337_PMIC_K_DONE_MASK                               0x1
#define MT6337_PMIC_K_DONE_SHIFT                              1
#define MT6337_PMIC_K_CONTROL_ADDR                            MT6337_BUCK_K_CON2
#define MT6337_PMIC_K_CONTROL_MASK                            0x3F
#define MT6337_PMIC_K_CONTROL_SHIFT                           2
#define MT6337_PMIC_DA_QI_SMPS_OSC_CAL_ADDR                   MT6337_BUCK_K_CON2
#define MT6337_PMIC_DA_QI_SMPS_OSC_CAL_MASK                   0x3F
#define MT6337_PMIC_DA_QI_SMPS_OSC_CAL_SHIFT                  8
#define MT6337_PMIC_BUCK_K_BUCK_CK_CNT_ADDR                   MT6337_BUCK_K_CON3
#define MT6337_PMIC_BUCK_K_BUCK_CK_CNT_MASK                   0x3FF
#define MT6337_PMIC_BUCK_K_BUCK_CK_CNT_SHIFT                  0
#define MT6337_PMIC_RG_LDO_RSV1_ADDR                          MT6337_LDO_RSV_CON0
#define MT6337_PMIC_RG_LDO_RSV1_MASK                          0x3FF
#define MT6337_PMIC_RG_LDO_RSV1_SHIFT                         0
#define MT6337_PMIC_RG_LDO_RSV0_ADDR                          MT6337_LDO_RSV_CON0
#define MT6337_PMIC_RG_LDO_RSV0_MASK                          0x3F
#define MT6337_PMIC_RG_LDO_RSV0_SHIFT                         10
#define MT6337_PMIC_RG_LDO_RSV2_ADDR                          MT6337_LDO_RSV_CON1
#define MT6337_PMIC_RG_LDO_RSV2_MASK                          0xFFFF
#define MT6337_PMIC_RG_LDO_RSV2_SHIFT                         0
#define MT6337_PMIC_LDO_DEGTD_SEL_ADDR                        MT6337_LDO_OCFB0
#define MT6337_PMIC_LDO_DEGTD_SEL_MASK                        0x3
#define MT6337_PMIC_LDO_DEGTD_SEL_SHIFT                       14
#define MT6337_PMIC_RG_VA18_SW_EN_ADDR                        MT6337_LDO_VA18_CON0
#define MT6337_PMIC_RG_VA18_SW_EN_MASK                        0x1
#define MT6337_PMIC_RG_VA18_SW_EN_SHIFT                       0
#define MT6337_PMIC_RG_VA18_SW_LP_ADDR                        MT6337_LDO_VA18_CON0
#define MT6337_PMIC_RG_VA18_SW_LP_MASK                        0x1
#define MT6337_PMIC_RG_VA18_SW_LP_SHIFT                       1
#define MT6337_PMIC_RG_VA18_SW_OP_EN_ADDR                     MT6337_LDO_VA18_OP_EN
#define MT6337_PMIC_RG_VA18_SW_OP_EN_MASK                     0x1
#define MT6337_PMIC_RG_VA18_SW_OP_EN_SHIFT                    0
#define MT6337_PMIC_RG_VA18_HW0_OP_EN_ADDR                    MT6337_LDO_VA18_OP_EN
#define MT6337_PMIC_RG_VA18_HW0_OP_EN_MASK                    0x1
#define MT6337_PMIC_RG_VA18_HW0_OP_EN_SHIFT                   1
#define MT6337_PMIC_RG_VA18_HW2_OP_EN_ADDR                    MT6337_LDO_VA18_OP_EN
#define MT6337_PMIC_RG_VA18_HW2_OP_EN_MASK                    0x1
#define MT6337_PMIC_RG_VA18_HW2_OP_EN_SHIFT                   2
#define MT6337_PMIC_RG_VA18_OP_EN_SET_ADDR                    MT6337_LDO_VA18_OP_EN_SET
#define MT6337_PMIC_RG_VA18_OP_EN_SET_MASK                    0xFFFF
#define MT6337_PMIC_RG_VA18_OP_EN_SET_SHIFT                   0
#define MT6337_PMIC_RG_VA18_OP_EN_CLR_ADDR                    MT6337_LDO_VA18_OP_EN_CLR
#define MT6337_PMIC_RG_VA18_OP_EN_CLR_MASK                    0xFFFF
#define MT6337_PMIC_RG_VA18_OP_EN_CLR_SHIFT                   0
#define MT6337_PMIC_RG_VA18_HW0_OP_CFG_ADDR                   MT6337_LDO_VA18_OP_CFG
#define MT6337_PMIC_RG_VA18_HW0_OP_CFG_MASK                   0x1
#define MT6337_PMIC_RG_VA18_HW0_OP_CFG_SHIFT                  1
#define MT6337_PMIC_RG_VA18_HW2_OP_CFG_ADDR                   MT6337_LDO_VA18_OP_CFG
#define MT6337_PMIC_RG_VA18_HW2_OP_CFG_MASK                   0x1
#define MT6337_PMIC_RG_VA18_HW2_OP_CFG_SHIFT                  2
#define MT6337_PMIC_RG_VA18_GO_ON_OP_ADDR                     MT6337_LDO_VA18_OP_CFG
#define MT6337_PMIC_RG_VA18_GO_ON_OP_MASK                     0x1
#define MT6337_PMIC_RG_VA18_GO_ON_OP_SHIFT                    8
#define MT6337_PMIC_RG_VA18_GO_LP_OP_ADDR                     MT6337_LDO_VA18_OP_CFG
#define MT6337_PMIC_RG_VA18_GO_LP_OP_MASK                     0x1
#define MT6337_PMIC_RG_VA18_GO_LP_OP_SHIFT                    9
#define MT6337_PMIC_RG_VA18_OP_CFG_SET_ADDR                   MT6337_LDO_VA18_OP_CFG_SET
#define MT6337_PMIC_RG_VA18_OP_CFG_SET_MASK                   0xFFFF
#define MT6337_PMIC_RG_VA18_OP_CFG_SET_SHIFT                  0
#define MT6337_PMIC_RG_VA18_OP_CFG_CLR_ADDR                   MT6337_LDO_VA18_OP_CFG_CLR
#define MT6337_PMIC_RG_VA18_OP_CFG_CLR_MASK                   0xFFFF
#define MT6337_PMIC_RG_VA18_OP_CFG_CLR_SHIFT                  0
#define MT6337_PMIC_DA_QI_VA18_MODE_ADDR                      MT6337_LDO_VA18_CON1
#define MT6337_PMIC_DA_QI_VA18_MODE_MASK                      0x1
#define MT6337_PMIC_DA_QI_VA18_MODE_SHIFT                     8
#define MT6337_PMIC_RG_VA18_STBTD_ADDR                        MT6337_LDO_VA18_CON1
#define MT6337_PMIC_RG_VA18_STBTD_MASK                        0x3
#define MT6337_PMIC_RG_VA18_STBTD_SHIFT                       9
#define MT6337_PMIC_DA_QI_VA18_STB_ADDR                       MT6337_LDO_VA18_CON1
#define MT6337_PMIC_DA_QI_VA18_STB_MASK                       0x1
#define MT6337_PMIC_DA_QI_VA18_STB_SHIFT                      14
#define MT6337_PMIC_DA_QI_VA18_EN_ADDR                        MT6337_LDO_VA18_CON1
#define MT6337_PMIC_DA_QI_VA18_EN_MASK                        0x1
#define MT6337_PMIC_DA_QI_VA18_EN_SHIFT                       15
#define MT6337_PMIC_RG_VA18_OCFB_EN_ADDR                      MT6337_LDO_VA18_CON2
#define MT6337_PMIC_RG_VA18_OCFB_EN_MASK                      0x1
#define MT6337_PMIC_RG_VA18_OCFB_EN_SHIFT                     9
#define MT6337_PMIC_DA_QI_VA18_OCFB_EN_ADDR                   MT6337_LDO_VA18_CON2
#define MT6337_PMIC_DA_QI_VA18_OCFB_EN_MASK                   0x1
#define MT6337_PMIC_DA_QI_VA18_OCFB_EN_SHIFT                  10
#define MT6337_PMIC_RG_VA18_DUMMY_LOAD_ADDR                   MT6337_LDO_VA18_CON3
#define MT6337_PMIC_RG_VA18_DUMMY_LOAD_MASK                   0x7
#define MT6337_PMIC_RG_VA18_DUMMY_LOAD_SHIFT                  4
#define MT6337_PMIC_DA_QI_VA18_DUMMY_LOAD_ADDR                MT6337_LDO_VA18_CON3
#define MT6337_PMIC_DA_QI_VA18_DUMMY_LOAD_MASK                0x1F
#define MT6337_PMIC_DA_QI_VA18_DUMMY_LOAD_SHIFT               11
#define MT6337_PMIC_RG_VA25_SW_EN_ADDR                        MT6337_LDO_VA25_CON0
#define MT6337_PMIC_RG_VA25_SW_EN_MASK                        0x1
#define MT6337_PMIC_RG_VA25_SW_EN_SHIFT                       0
#define MT6337_PMIC_RG_VA25_SW_LP_ADDR                        MT6337_LDO_VA25_CON0
#define MT6337_PMIC_RG_VA25_SW_LP_MASK                        0x1
#define MT6337_PMIC_RG_VA25_SW_LP_SHIFT                       1
#define MT6337_PMIC_RG_VA25_SW_OP_EN_ADDR                     MT6337_LDO_VA25_OP_EN
#define MT6337_PMIC_RG_VA25_SW_OP_EN_MASK                     0x1
#define MT6337_PMIC_RG_VA25_SW_OP_EN_SHIFT                    0
#define MT6337_PMIC_RG_VA25_HW0_OP_EN_ADDR                    MT6337_LDO_VA25_OP_EN
#define MT6337_PMIC_RG_VA25_HW0_OP_EN_MASK                    0x1
#define MT6337_PMIC_RG_VA25_HW0_OP_EN_SHIFT                   1
#define MT6337_PMIC_RG_VA25_HW2_OP_EN_ADDR                    MT6337_LDO_VA25_OP_EN
#define MT6337_PMIC_RG_VA25_HW2_OP_EN_MASK                    0x1
#define MT6337_PMIC_RG_VA25_HW2_OP_EN_SHIFT                   2
#define MT6337_PMIC_RG_VA25_OP_EN_SET_ADDR                    MT6337_LDO_VA25_OP_EN_SET
#define MT6337_PMIC_RG_VA25_OP_EN_SET_MASK                    0xFFFF
#define MT6337_PMIC_RG_VA25_OP_EN_SET_SHIFT                   0
#define MT6337_PMIC_RG_VA25_OP_EN_CLR_ADDR                    MT6337_LDO_VA25_OP_EN_CLR
#define MT6337_PMIC_RG_VA25_OP_EN_CLR_MASK                    0xFFFF
#define MT6337_PMIC_RG_VA25_OP_EN_CLR_SHIFT                   0
#define MT6337_PMIC_RG_VA25_HW0_OP_CFG_ADDR                   MT6337_LDO_VA25_OP_CFG
#define MT6337_PMIC_RG_VA25_HW0_OP_CFG_MASK                   0x1
#define MT6337_PMIC_RG_VA25_HW0_OP_CFG_SHIFT                  1
#define MT6337_PMIC_RG_VA25_HW2_OP_CFG_ADDR                   MT6337_LDO_VA25_OP_CFG
#define MT6337_PMIC_RG_VA25_HW2_OP_CFG_MASK                   0x1
#define MT6337_PMIC_RG_VA25_HW2_OP_CFG_SHIFT                  2
#define MT6337_PMIC_RG_VA25_GO_ON_OP_ADDR                     MT6337_LDO_VA25_OP_CFG
#define MT6337_PMIC_RG_VA25_GO_ON_OP_MASK                     0x1
#define MT6337_PMIC_RG_VA25_GO_ON_OP_SHIFT                    8
#define MT6337_PMIC_RG_VA25_GO_LP_OP_ADDR                     MT6337_LDO_VA25_OP_CFG
#define MT6337_PMIC_RG_VA25_GO_LP_OP_MASK                     0x1
#define MT6337_PMIC_RG_VA25_GO_LP_OP_SHIFT                    9
#define MT6337_PMIC_RG_VA25_OP_CFG_SET_ADDR                   MT6337_LDO_VA25_OP_CFG_SET
#define MT6337_PMIC_RG_VA25_OP_CFG_SET_MASK                   0xFFFF
#define MT6337_PMIC_RG_VA25_OP_CFG_SET_SHIFT                  0
#define MT6337_PMIC_RG_VA25_OP_CFG_CLR_ADDR                   MT6337_LDO_VA25_OP_CFG_CLR
#define MT6337_PMIC_RG_VA25_OP_CFG_CLR_MASK                   0xFFFF
#define MT6337_PMIC_RG_VA25_OP_CFG_CLR_SHIFT                  0
#define MT6337_PMIC_DA_QI_VA25_MODE_ADDR                      MT6337_LDO_VA25_CON1
#define MT6337_PMIC_DA_QI_VA25_MODE_MASK                      0x1
#define MT6337_PMIC_DA_QI_VA25_MODE_SHIFT                     8
#define MT6337_PMIC_RG_VA25_STBTD_ADDR                        MT6337_LDO_VA25_CON1
#define MT6337_PMIC_RG_VA25_STBTD_MASK                        0x3
#define MT6337_PMIC_RG_VA25_STBTD_SHIFT                       9
#define MT6337_PMIC_DA_QI_VA25_STB_ADDR                       MT6337_LDO_VA25_CON1
#define MT6337_PMIC_DA_QI_VA25_STB_MASK                       0x1
#define MT6337_PMIC_DA_QI_VA25_STB_SHIFT                      14
#define MT6337_PMIC_DA_QI_VA25_EN_ADDR                        MT6337_LDO_VA25_CON1
#define MT6337_PMIC_DA_QI_VA25_EN_MASK                        0x1
#define MT6337_PMIC_DA_QI_VA25_EN_SHIFT                       15
#define MT6337_PMIC_RG_VA25_OCFB_EN_ADDR                      MT6337_LDO_VA25_CON2
#define MT6337_PMIC_RG_VA25_OCFB_EN_MASK                      0x1
#define MT6337_PMIC_RG_VA25_OCFB_EN_SHIFT                     9
#define MT6337_PMIC_DA_QI_VA25_OCFB_EN_ADDR                   MT6337_LDO_VA25_CON2
#define MT6337_PMIC_DA_QI_VA25_OCFB_EN_MASK                   0x1
#define MT6337_PMIC_DA_QI_VA25_OCFB_EN_SHIFT                  10
#define MT6337_PMIC_RG_DCM_MODE_ADDR                          MT6337_LDO_DCM
#define MT6337_PMIC_RG_DCM_MODE_MASK                          0x1
#define MT6337_PMIC_RG_DCM_MODE_SHIFT                         0
#define MT6337_PMIC_RG_VA18_CK_SW_MODE_ADDR                   MT6337_LDO_VA18_CG0
#define MT6337_PMIC_RG_VA18_CK_SW_MODE_MASK                   0x1
#define MT6337_PMIC_RG_VA18_CK_SW_MODE_SHIFT                  0
#define MT6337_PMIC_RG_VA18_CK_SW_EN_ADDR                     MT6337_LDO_VA18_CG0
#define MT6337_PMIC_RG_VA18_CK_SW_EN_MASK                     0x1
#define MT6337_PMIC_RG_VA18_CK_SW_EN_SHIFT                    1
#define MT6337_PMIC_RG_VA25_CK_SW_MODE_ADDR                   MT6337_LDO_VA25_CG0
#define MT6337_PMIC_RG_VA25_CK_SW_MODE_MASK                   0x1
#define MT6337_PMIC_RG_VA25_CK_SW_MODE_SHIFT                  0
#define MT6337_PMIC_RG_VA25_CK_SW_EN_ADDR                     MT6337_LDO_VA25_CG0
#define MT6337_PMIC_RG_VA25_CK_SW_EN_MASK                     0x1
#define MT6337_PMIC_RG_VA25_CK_SW_EN_SHIFT                    1
#define MT6337_PMIC_RG_VA25_CAL_ADDR                          MT6337_VA25_ANA_CON0
#define MT6337_PMIC_RG_VA25_CAL_MASK                          0xF
#define MT6337_PMIC_RG_VA25_CAL_SHIFT                         0
#define MT6337_PMIC_RG_VA25_VOSEL_ADDR                        MT6337_VA25_ANA_CON0
#define MT6337_PMIC_RG_VA25_VOSEL_MASK                        0x3
#define MT6337_PMIC_RG_VA25_VOSEL_SHIFT                       8
#define MT6337_PMIC_RG_VA25_NDIS_EN_ADDR                      MT6337_VA25_ANA_CON1
#define MT6337_PMIC_RG_VA25_NDIS_EN_MASK                      0x1
#define MT6337_PMIC_RG_VA25_NDIS_EN_SHIFT                     1
#define MT6337_PMIC_RG_VA25_FBSEL_ADDR                        MT6337_VA25_ANA_CON1
#define MT6337_PMIC_RG_VA25_FBSEL_MASK                        0x3
#define MT6337_PMIC_RG_VA25_FBSEL_SHIFT                       2
#define MT6337_PMIC_RG_VA18_CAL_ADDR                          MT6337_VA18_ANA_CON0
#define MT6337_PMIC_RG_VA18_CAL_MASK                          0xF
#define MT6337_PMIC_RG_VA18_CAL_SHIFT                         0
#define MT6337_PMIC_RG_VA18_VOSEL_ADDR                        MT6337_VA18_ANA_CON0
#define MT6337_PMIC_RG_VA18_VOSEL_MASK                        0x7
#define MT6337_PMIC_RG_VA18_VOSEL_SHIFT                       8
#define MT6337_PMIC_RG_VA18_PG_STATUS_EN_ADDR                 MT6337_VA18_ANA_CON1
#define MT6337_PMIC_RG_VA18_PG_STATUS_EN_MASK                 0x1
#define MT6337_PMIC_RG_VA18_PG_STATUS_EN_SHIFT                0
#define MT6337_PMIC_RG_VA18_NDIS_EN_ADDR                      MT6337_VA18_ANA_CON1
#define MT6337_PMIC_RG_VA18_NDIS_EN_MASK                      0x1
#define MT6337_PMIC_RG_VA18_NDIS_EN_SHIFT                     1
#define MT6337_PMIC_RG_VA18_STB_SEL_ADDR                      MT6337_VA18_ANA_CON1
#define MT6337_PMIC_RG_VA18_STB_SEL_MASK                      0x1
#define MT6337_PMIC_RG_VA18_STB_SEL_SHIFT                     2
#define MT6337_PMIC_RG_OTP_PA_ADDR                            MT6337_OTP_CON0
#define MT6337_PMIC_RG_OTP_PA_MASK                            0x3F
#define MT6337_PMIC_RG_OTP_PA_SHIFT                           0
#define MT6337_PMIC_RG_OTP_PDIN_ADDR                          MT6337_OTP_CON1
#define MT6337_PMIC_RG_OTP_PDIN_MASK                          0xFF
#define MT6337_PMIC_RG_OTP_PDIN_SHIFT                         0
#define MT6337_PMIC_RG_OTP_PTM_ADDR                           MT6337_OTP_CON2
#define MT6337_PMIC_RG_OTP_PTM_MASK                           0x3
#define MT6337_PMIC_RG_OTP_PTM_SHIFT                          0
#define MT6337_PMIC_RG_OTP_PWE_ADDR                           MT6337_OTP_CON3
#define MT6337_PMIC_RG_OTP_PWE_MASK                           0x1
#define MT6337_PMIC_RG_OTP_PWE_SHIFT                          0
#define MT6337_PMIC_RG_OTP_PPROG_ADDR                         MT6337_OTP_CON4
#define MT6337_PMIC_RG_OTP_PPROG_MASK                         0x1
#define MT6337_PMIC_RG_OTP_PPROG_SHIFT                        0
#define MT6337_PMIC_RG_OTP_PWE_SRC_ADDR                       MT6337_OTP_CON5
#define MT6337_PMIC_RG_OTP_PWE_SRC_MASK                       0x1
#define MT6337_PMIC_RG_OTP_PWE_SRC_SHIFT                      0
#define MT6337_PMIC_RG_OTP_PROG_PKEY_ADDR                     MT6337_OTP_CON6
#define MT6337_PMIC_RG_OTP_PROG_PKEY_MASK                     0xFFFF
#define MT6337_PMIC_RG_OTP_PROG_PKEY_SHIFT                    0
#define MT6337_PMIC_RG_OTP_RD_PKEY_ADDR                       MT6337_OTP_CON7
#define MT6337_PMIC_RG_OTP_RD_PKEY_MASK                       0xFFFF
#define MT6337_PMIC_RG_OTP_RD_PKEY_SHIFT                      0
#define MT6337_PMIC_RG_OTP_RD_TRIG_ADDR                       MT6337_OTP_CON8
#define MT6337_PMIC_RG_OTP_RD_TRIG_MASK                       0x1
#define MT6337_PMIC_RG_OTP_RD_TRIG_SHIFT                      0
#define MT6337_PMIC_RG_RD_RDY_BYPASS_ADDR                     MT6337_OTP_CON9
#define MT6337_PMIC_RG_RD_RDY_BYPASS_MASK                     0x1
#define MT6337_PMIC_RG_RD_RDY_BYPASS_SHIFT                    0
#define MT6337_PMIC_RG_SKIP_OTP_OUT_ADDR                      MT6337_OTP_CON10
#define MT6337_PMIC_RG_SKIP_OTP_OUT_MASK                      0x1
#define MT6337_PMIC_RG_SKIP_OTP_OUT_SHIFT                     0
#define MT6337_PMIC_RG_OTP_RD_SW_ADDR                         MT6337_OTP_CON11
#define MT6337_PMIC_RG_OTP_RD_SW_MASK                         0x1
#define MT6337_PMIC_RG_OTP_RD_SW_SHIFT                        0
#define MT6337_PMIC_RG_OTP_RD_BUSY_ADDR                       MT6337_OTP_CON13
#define MT6337_PMIC_RG_OTP_RD_BUSY_MASK                       0x1
#define MT6337_PMIC_RG_OTP_RD_BUSY_SHIFT                      0
#define MT6337_PMIC_RG_OTP_RD_ACK_ADDR                        MT6337_OTP_CON13
#define MT6337_PMIC_RG_OTP_RD_ACK_MASK                        0x1
#define MT6337_PMIC_RG_OTP_RD_ACK_SHIFT                       2
#define MT6337_PMIC_RG_OTP_DOUT_0_15_ADDR                     MT6337_OTP_DOUT_0_15
#define MT6337_PMIC_RG_OTP_DOUT_0_15_MASK                     0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_0_15_SHIFT                    0
#define MT6337_PMIC_RG_OTP_DOUT_16_31_ADDR                    MT6337_OTP_DOUT_16_31
#define MT6337_PMIC_RG_OTP_DOUT_16_31_MASK                    0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_16_31_SHIFT                   0
#define MT6337_PMIC_RG_OTP_DOUT_32_47_ADDR                    MT6337_OTP_DOUT_32_47
#define MT6337_PMIC_RG_OTP_DOUT_32_47_MASK                    0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_32_47_SHIFT                   0
#define MT6337_PMIC_RG_OTP_DOUT_48_63_ADDR                    MT6337_OTP_DOUT_48_63
#define MT6337_PMIC_RG_OTP_DOUT_48_63_MASK                    0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_48_63_SHIFT                   0
#define MT6337_PMIC_RG_OTP_DOUT_64_79_ADDR                    MT6337_OTP_DOUT_64_79
#define MT6337_PMIC_RG_OTP_DOUT_64_79_MASK                    0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_64_79_SHIFT                   0
#define MT6337_PMIC_RG_OTP_DOUT_80_95_ADDR                    MT6337_OTP_DOUT_80_95
#define MT6337_PMIC_RG_OTP_DOUT_80_95_MASK                    0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_80_95_SHIFT                   0
#define MT6337_PMIC_RG_OTP_DOUT_96_111_ADDR                   MT6337_OTP_DOUT_96_111
#define MT6337_PMIC_RG_OTP_DOUT_96_111_MASK                   0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_96_111_SHIFT                  0
#define MT6337_PMIC_RG_OTP_DOUT_112_127_ADDR                  MT6337_OTP_DOUT_112_127
#define MT6337_PMIC_RG_OTP_DOUT_112_127_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_112_127_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_128_143_ADDR                  MT6337_OTP_DOUT_128_143
#define MT6337_PMIC_RG_OTP_DOUT_128_143_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_128_143_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_144_159_ADDR                  MT6337_OTP_DOUT_144_159
#define MT6337_PMIC_RG_OTP_DOUT_144_159_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_144_159_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_160_175_ADDR                  MT6337_OTP_DOUT_160_175
#define MT6337_PMIC_RG_OTP_DOUT_160_175_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_160_175_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_176_191_ADDR                  MT6337_OTP_DOUT_176_191
#define MT6337_PMIC_RG_OTP_DOUT_176_191_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_176_191_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_192_207_ADDR                  MT6337_OTP_DOUT_192_207
#define MT6337_PMIC_RG_OTP_DOUT_192_207_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_192_207_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_208_223_ADDR                  MT6337_OTP_DOUT_208_223
#define MT6337_PMIC_RG_OTP_DOUT_208_223_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_208_223_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_224_239_ADDR                  MT6337_OTP_DOUT_224_239
#define MT6337_PMIC_RG_OTP_DOUT_224_239_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_224_239_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_240_255_ADDR                  MT6337_OTP_DOUT_240_255
#define MT6337_PMIC_RG_OTP_DOUT_240_255_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_240_255_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_256_271_ADDR                  MT6337_OTP_DOUT_256_271
#define MT6337_PMIC_RG_OTP_DOUT_256_271_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_256_271_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_272_287_ADDR                  MT6337_OTP_DOUT_272_287
#define MT6337_PMIC_RG_OTP_DOUT_272_287_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_272_287_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_288_303_ADDR                  MT6337_OTP_DOUT_288_303
#define MT6337_PMIC_RG_OTP_DOUT_288_303_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_288_303_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_304_319_ADDR                  MT6337_OTP_DOUT_304_319
#define MT6337_PMIC_RG_OTP_DOUT_304_319_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_304_319_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_320_335_ADDR                  MT6337_OTP_DOUT_320_335
#define MT6337_PMIC_RG_OTP_DOUT_320_335_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_320_335_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_336_351_ADDR                  MT6337_OTP_DOUT_336_351
#define MT6337_PMIC_RG_OTP_DOUT_336_351_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_336_351_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_352_367_ADDR                  MT6337_OTP_DOUT_352_367
#define MT6337_PMIC_RG_OTP_DOUT_352_367_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_352_367_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_368_383_ADDR                  MT6337_OTP_DOUT_368_383
#define MT6337_PMIC_RG_OTP_DOUT_368_383_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_368_383_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_384_399_ADDR                  MT6337_OTP_DOUT_384_399
#define MT6337_PMIC_RG_OTP_DOUT_384_399_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_384_399_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_400_415_ADDR                  MT6337_OTP_DOUT_400_415
#define MT6337_PMIC_RG_OTP_DOUT_400_415_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_400_415_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_416_431_ADDR                  MT6337_OTP_DOUT_416_431
#define MT6337_PMIC_RG_OTP_DOUT_416_431_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_416_431_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_432_447_ADDR                  MT6337_OTP_DOUT_432_447
#define MT6337_PMIC_RG_OTP_DOUT_432_447_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_432_447_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_448_463_ADDR                  MT6337_OTP_DOUT_448_463
#define MT6337_PMIC_RG_OTP_DOUT_448_463_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_448_463_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_464_479_ADDR                  MT6337_OTP_DOUT_464_479
#define MT6337_PMIC_RG_OTP_DOUT_464_479_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_464_479_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_480_495_ADDR                  MT6337_OTP_DOUT_480_495
#define MT6337_PMIC_RG_OTP_DOUT_480_495_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_480_495_SHIFT                 0
#define MT6337_PMIC_RG_OTP_DOUT_496_511_ADDR                  MT6337_OTP_DOUT_496_511
#define MT6337_PMIC_RG_OTP_DOUT_496_511_MASK                  0xFFFF
#define MT6337_PMIC_RG_OTP_DOUT_496_511_SHIFT                 0
#define MT6337_PMIC_RG_OTP_VAL_0_15_ADDR                      MT6337_OTP_VAL_0_15
#define MT6337_PMIC_RG_OTP_VAL_0_15_MASK                      0xFFFF
#define MT6337_PMIC_RG_OTP_VAL_0_15_SHIFT                     0
#define MT6337_PMIC_RG_OTP_VAL_16_31_ADDR                     MT6337_OTP_VAL_16_31
#define MT6337_PMIC_RG_OTP_VAL_16_31_MASK                     0xFFFF
#define MT6337_PMIC_RG_OTP_VAL_16_31_SHIFT                    0
#define MT6337_PMIC_RG_OTP_VAL_32_47_ADDR                     MT6337_OTP_VAL_32_47
#define MT6337_PMIC_RG_OTP_VAL_32_47_MASK                     0xFFFF
#define MT6337_PMIC_RG_OTP_VAL_32_47_SHIFT                    0
#define MT6337_PMIC_RG_OTP_VAL_48_63_ADDR                     MT6337_OTP_VAL_48_63
#define MT6337_PMIC_RG_OTP_VAL_48_63_MASK                     0xFFFF
#define MT6337_PMIC_RG_OTP_VAL_48_63_SHIFT                    0
#define MT6337_PMIC_RG_OTP_VAL_64_79_ADDR                     MT6337_OTP_VAL_64_79
#define MT6337_PMIC_RG_OTP_VAL_64_79_MASK                     0xFFFF
#define MT6337_PMIC_RG_OTP_VAL_64_79_SHIFT                    0
#define MT6337_PMIC_RG_OTP_VAL_80_90_ADDR                     MT6337_OTP_VAL_80_95
#define MT6337_PMIC_RG_OTP_VAL_80_90_MASK                     0x7FF
#define MT6337_PMIC_RG_OTP_VAL_80_90_SHIFT                    0
#define MT6337_PMIC_RG_OTP_VAL_222_223_ADDR                   MT6337_OTP_VAL_208_223
#define MT6337_PMIC_RG_OTP_VAL_222_223_MASK                   0x3
#define MT6337_PMIC_RG_OTP_VAL_222_223_SHIFT                  14
#define MT6337_PMIC_RG_OTP_VAL_224_239_ADDR                   MT6337_OTP_VAL_224_239
#define MT6337_PMIC_RG_OTP_VAL_224_239_MASK                   0xFFFF
#define MT6337_PMIC_RG_OTP_VAL_224_239_SHIFT                  0
#define MT6337_PMIC_RG_OTP_VAL_240_252_ADDR                   MT6337_OTP_VAL_240_255
#define MT6337_PMIC_RG_OTP_VAL_240_252_MASK                   0x1FFF
#define MT6337_PMIC_RG_OTP_VAL_240_252_SHIFT                  0
#define MT6337_PMIC_RG_OTP_VAL_511_ADDR                       MT6337_OTP_VAL_496_511
#define MT6337_PMIC_RG_OTP_VAL_511_MASK                       0x1
#define MT6337_PMIC_RG_OTP_VAL_511_SHIFT                      15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_ADDR                   MT6337_AUXADC_ADC0
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_MASK                   0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_ADDR                   MT6337_AUXADC_ADC0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH1_ADDR                   MT6337_AUXADC_ADC1
#define MT6337_PMIC_AUXADC_ADC_OUT_CH1_MASK                   0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH1_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH1_ADDR                   MT6337_AUXADC_ADC1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH1_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH1_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH2_ADDR                   MT6337_AUXADC_ADC2
#define MT6337_PMIC_AUXADC_ADC_OUT_CH2_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH2_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH2_ADDR                   MT6337_AUXADC_ADC2
#define MT6337_PMIC_AUXADC_ADC_RDY_CH2_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH2_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH3_ADDR                   MT6337_AUXADC_ADC3
#define MT6337_PMIC_AUXADC_ADC_OUT_CH3_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH3_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH3_ADDR                   MT6337_AUXADC_ADC3
#define MT6337_PMIC_AUXADC_ADC_RDY_CH3_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH3_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH4_ADDR                   MT6337_AUXADC_ADC4
#define MT6337_PMIC_AUXADC_ADC_OUT_CH4_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH4_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH4_ADDR                   MT6337_AUXADC_ADC4
#define MT6337_PMIC_AUXADC_ADC_RDY_CH4_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH4_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH5_ADDR                   MT6337_AUXADC_ADC5
#define MT6337_PMIC_AUXADC_ADC_OUT_CH5_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH5_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH5_ADDR                   MT6337_AUXADC_ADC5
#define MT6337_PMIC_AUXADC_ADC_RDY_CH5_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH5_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH6_ADDR                   MT6337_AUXADC_ADC6
#define MT6337_PMIC_AUXADC_ADC_OUT_CH6_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH6_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH6_ADDR                   MT6337_AUXADC_ADC6
#define MT6337_PMIC_AUXADC_ADC_RDY_CH6_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH6_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH7_ADDR                   MT6337_AUXADC_ADC7
#define MT6337_PMIC_AUXADC_ADC_OUT_CH7_MASK                   0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH7_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH7_ADDR                   MT6337_AUXADC_ADC7
#define MT6337_PMIC_AUXADC_ADC_RDY_CH7_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH7_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH8_ADDR                   MT6337_AUXADC_ADC8
#define MT6337_PMIC_AUXADC_ADC_OUT_CH8_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH8_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH8_ADDR                   MT6337_AUXADC_ADC8
#define MT6337_PMIC_AUXADC_ADC_RDY_CH8_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH8_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH9_ADDR                   MT6337_AUXADC_ADC9
#define MT6337_PMIC_AUXADC_ADC_OUT_CH9_MASK                   0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH9_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH9_ADDR                   MT6337_AUXADC_ADC9
#define MT6337_PMIC_AUXADC_ADC_RDY_CH9_MASK                   0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH9_SHIFT                  15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH10_ADDR                  MT6337_AUXADC_ADC10
#define MT6337_PMIC_AUXADC_ADC_OUT_CH10_MASK                  0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH10_SHIFT                 0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH10_ADDR                  MT6337_AUXADC_ADC10
#define MT6337_PMIC_AUXADC_ADC_RDY_CH10_MASK                  0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH10_SHIFT                 15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH11_ADDR                  MT6337_AUXADC_ADC11
#define MT6337_PMIC_AUXADC_ADC_OUT_CH11_MASK                  0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH11_SHIFT                 0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH11_ADDR                  MT6337_AUXADC_ADC11
#define MT6337_PMIC_AUXADC_ADC_RDY_CH11_MASK                  0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH11_SHIFT                 15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH12_15_ADDR               MT6337_AUXADC_ADC12
#define MT6337_PMIC_AUXADC_ADC_OUT_CH12_15_MASK               0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH12_15_SHIFT              0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH12_15_ADDR               MT6337_AUXADC_ADC12
#define MT6337_PMIC_AUXADC_ADC_RDY_CH12_15_MASK               0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH12_15_SHIFT              15
#define MT6337_PMIC_AUXADC_ADC_OUT_THR_HW_ADDR                MT6337_AUXADC_ADC13
#define MT6337_PMIC_AUXADC_ADC_OUT_THR_HW_MASK                0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_THR_HW_SHIFT               0
#define MT6337_PMIC_AUXADC_ADC_RDY_THR_HW_ADDR                MT6337_AUXADC_ADC13
#define MT6337_PMIC_AUXADC_ADC_RDY_THR_HW_MASK                0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_THR_HW_SHIFT               15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH4_BY_MD_ADDR             MT6337_AUXADC_ADC14
#define MT6337_PMIC_AUXADC_ADC_OUT_CH4_BY_MD_MASK             0xFFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH4_BY_MD_SHIFT            0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH4_BY_MD_ADDR             MT6337_AUXADC_ADC14
#define MT6337_PMIC_AUXADC_ADC_RDY_CH4_BY_MD_MASK             0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH4_BY_MD_SHIFT            15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_MD_ADDR             MT6337_AUXADC_ADC15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_MD_MASK             0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_MD_SHIFT            0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_MD_ADDR             MT6337_AUXADC_ADC15
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_MD_MASK             0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_MD_SHIFT            15
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_AP_ADDR             MT6337_AUXADC_ADC16
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_AP_MASK             0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_AP_SHIFT            0
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_AP_ADDR             MT6337_AUXADC_ADC16
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_AP_MASK             0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_AP_SHIFT            15
#define MT6337_PMIC_AUXADC_ADC_OUT_RAW_ADDR                   MT6337_AUXADC_ADC17
#define MT6337_PMIC_AUXADC_ADC_OUT_RAW_MASK                   0x7FFF
#define MT6337_PMIC_AUXADC_ADC_OUT_RAW_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_ADDR                   MT6337_AUXADC_STA0
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHIFT                  0
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHARE_ADDR             MT6337_AUXADC_STA1
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHARE_MASK             0x1
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHARE_SHIFT            7
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_HW_ADDR            MT6337_AUXADC_STA1
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_HW_MASK            0x1
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_HW_SHIFT           14
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_MD_ADDR            MT6337_AUXADC_STA1
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_MD_MASK            0x1
#define MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_MD_SHIFT           15
#define MT6337_PMIC_AUXADC_RQST_CH0_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH0_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH0_SHIFT                     0
#define MT6337_PMIC_AUXADC_RQST_CH1_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH1_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH1_SHIFT                     1
#define MT6337_PMIC_AUXADC_RQST_CH2_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH2_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH2_SHIFT                     2
#define MT6337_PMIC_AUXADC_RQST_CH3_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH3_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH3_SHIFT                     3
#define MT6337_PMIC_AUXADC_RQST_CH4_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH4_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH4_SHIFT                     4
#define MT6337_PMIC_AUXADC_RQST_CH5_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH5_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH5_SHIFT                     5
#define MT6337_PMIC_AUXADC_RQST_CH6_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH6_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH6_SHIFT                     6
#define MT6337_PMIC_AUXADC_RQST_CH7_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH7_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH7_SHIFT                     7
#define MT6337_PMIC_AUXADC_RQST_CH8_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH8_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH8_SHIFT                     8
#define MT6337_PMIC_AUXADC_RQST_CH9_ADDR                      MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH9_MASK                      0x1
#define MT6337_PMIC_AUXADC_RQST_CH9_SHIFT                     9
#define MT6337_PMIC_AUXADC_RQST_CH10_ADDR                     MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH10_MASK                     0x1
#define MT6337_PMIC_AUXADC_RQST_CH10_SHIFT                    10
#define MT6337_PMIC_AUXADC_RQST_CH11_ADDR                     MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH11_MASK                     0x1
#define MT6337_PMIC_AUXADC_RQST_CH11_SHIFT                    11
#define MT6337_PMIC_AUXADC_RQST_CH12_ADDR                     MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH12_MASK                     0x1
#define MT6337_PMIC_AUXADC_RQST_CH12_SHIFT                    12
#define MT6337_PMIC_AUXADC_RQST_CH13_ADDR                     MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH13_MASK                     0x1
#define MT6337_PMIC_AUXADC_RQST_CH13_SHIFT                    13
#define MT6337_PMIC_AUXADC_RQST_CH14_ADDR                     MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH14_MASK                     0x1
#define MT6337_PMIC_AUXADC_RQST_CH14_SHIFT                    14
#define MT6337_PMIC_AUXADC_RQST_CH15_ADDR                     MT6337_AUXADC_RQST0
#define MT6337_PMIC_AUXADC_RQST_CH15_MASK                     0x1
#define MT6337_PMIC_AUXADC_RQST_CH15_SHIFT                    15
#define MT6337_PMIC_AUXADC_RQST0_SET_ADDR                     MT6337_AUXADC_RQST0_SET
#define MT6337_PMIC_AUXADC_RQST0_SET_MASK                     0xFFFF
#define MT6337_PMIC_AUXADC_RQST0_SET_SHIFT                    0
#define MT6337_PMIC_AUXADC_RQST0_CLR_ADDR                     MT6337_AUXADC_RQST0_CLR
#define MT6337_PMIC_AUXADC_RQST0_CLR_MASK                     0xFFFF
#define MT6337_PMIC_AUXADC_RQST0_CLR_SHIFT                    0
#define MT6337_PMIC_AUXADC_RQST_CH0_BY_MD_ADDR                MT6337_AUXADC_RQST1
#define MT6337_PMIC_AUXADC_RQST_CH0_BY_MD_MASK                0x1
#define MT6337_PMIC_AUXADC_RQST_CH0_BY_MD_SHIFT               0
#define MT6337_PMIC_AUXADC_RQST_RSV0_ADDR                     MT6337_AUXADC_RQST1
#define MT6337_PMIC_AUXADC_RQST_RSV0_MASK                     0x3
#define MT6337_PMIC_AUXADC_RQST_RSV0_SHIFT                    2
#define MT6337_PMIC_AUXADC_RQST_CH4_BY_MD_ADDR                MT6337_AUXADC_RQST1
#define MT6337_PMIC_AUXADC_RQST_CH4_BY_MD_MASK                0x1
#define MT6337_PMIC_AUXADC_RQST_CH4_BY_MD_SHIFT               4
#define MT6337_PMIC_AUXADC_RQST_RSV1_ADDR                     MT6337_AUXADC_RQST1
#define MT6337_PMIC_AUXADC_RQST_RSV1_MASK                     0x1F
#define MT6337_PMIC_AUXADC_RQST_RSV1_SHIFT                    11
#define MT6337_PMIC_AUXADC_RQST1_SET_ADDR                     MT6337_AUXADC_RQST1_SET
#define MT6337_PMIC_AUXADC_RQST1_SET_MASK                     0xFFFF
#define MT6337_PMIC_AUXADC_RQST1_SET_SHIFT                    0
#define MT6337_PMIC_AUXADC_RQST1_CLR_ADDR                     MT6337_AUXADC_RQST1_CLR
#define MT6337_PMIC_AUXADC_RQST1_CLR_MASK                     0xFFFF
#define MT6337_PMIC_AUXADC_RQST1_CLR_SHIFT                    0
#define MT6337_PMIC_AUXADC_CK_ON_EXTD_ADDR                    MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_CK_ON_EXTD_MASK                    0x3F
#define MT6337_PMIC_AUXADC_CK_ON_EXTD_SHIFT                   0
#define MT6337_PMIC_AUXADC_SRCLKEN_SRC_SEL_ADDR               MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_SRCLKEN_SRC_SEL_MASK               0x3
#define MT6337_PMIC_AUXADC_SRCLKEN_SRC_SEL_SHIFT              6
#define MT6337_PMIC_AUXADC_ADC_PWDB_ADDR                      MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_ADC_PWDB_MASK                      0x1
#define MT6337_PMIC_AUXADC_ADC_PWDB_SHIFT                     8
#define MT6337_PMIC_AUXADC_ADC_PWDB_SWCTRL_ADDR               MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_ADC_PWDB_SWCTRL_MASK               0x1
#define MT6337_PMIC_AUXADC_ADC_PWDB_SWCTRL_SHIFT              9
#define MT6337_PMIC_AUXADC_STRUP_CK_ON_ENB_ADDR               MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_STRUP_CK_ON_ENB_MASK               0x1
#define MT6337_PMIC_AUXADC_STRUP_CK_ON_ENB_SHIFT              10
#define MT6337_PMIC_AUXADC_ADC_RDY_WAKEUP_CLR_ADDR            MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_ADC_RDY_WAKEUP_CLR_MASK            0x1
#define MT6337_PMIC_AUXADC_ADC_RDY_WAKEUP_CLR_SHIFT           11
#define MT6337_PMIC_AUXADC_SRCLKEN_CK_EN_ADDR                 MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_SRCLKEN_CK_EN_MASK                 0x1
#define MT6337_PMIC_AUXADC_SRCLKEN_CK_EN_SHIFT                12
#define MT6337_PMIC_AUXADC_CK_AON_GPS_ADDR                    MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_CK_AON_GPS_MASK                    0x1
#define MT6337_PMIC_AUXADC_CK_AON_GPS_SHIFT                   13
#define MT6337_PMIC_AUXADC_CK_AON_MD_ADDR                     MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_CK_AON_MD_MASK                     0x1
#define MT6337_PMIC_AUXADC_CK_AON_MD_SHIFT                    14
#define MT6337_PMIC_AUXADC_CK_AON_ADDR                        MT6337_AUXADC_CON0
#define MT6337_PMIC_AUXADC_CK_AON_MASK                        0x1
#define MT6337_PMIC_AUXADC_CK_AON_SHIFT                       15
#define MT6337_PMIC_AUXADC_CON0_SET_ADDR                      MT6337_AUXADC_CON0_SET
#define MT6337_PMIC_AUXADC_CON0_SET_MASK                      0xFFFF
#define MT6337_PMIC_AUXADC_CON0_SET_SHIFT                     0
#define MT6337_PMIC_AUXADC_CON0_CLR_ADDR                      MT6337_AUXADC_CON0_CLR
#define MT6337_PMIC_AUXADC_CON0_CLR_MASK                      0xFFFF
#define MT6337_PMIC_AUXADC_CON0_CLR_SHIFT                     0
#define MT6337_PMIC_AUXADC_AVG_NUM_SMALL_ADDR                 MT6337_AUXADC_CON1
#define MT6337_PMIC_AUXADC_AVG_NUM_SMALL_MASK                 0x7
#define MT6337_PMIC_AUXADC_AVG_NUM_SMALL_SHIFT                0
#define MT6337_PMIC_AUXADC_AVG_NUM_LARGE_ADDR                 MT6337_AUXADC_CON1
#define MT6337_PMIC_AUXADC_AVG_NUM_LARGE_MASK                 0x7
#define MT6337_PMIC_AUXADC_AVG_NUM_LARGE_SHIFT                3
#define MT6337_PMIC_AUXADC_SPL_NUM_ADDR                       MT6337_AUXADC_CON1
#define MT6337_PMIC_AUXADC_SPL_NUM_MASK                       0x3FF
#define MT6337_PMIC_AUXADC_SPL_NUM_SHIFT                      6
#define MT6337_PMIC_AUXADC_AVG_NUM_SEL_ADDR                   MT6337_AUXADC_CON2
#define MT6337_PMIC_AUXADC_AVG_NUM_SEL_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHIFT                  0
#define MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHARE_ADDR             MT6337_AUXADC_CON2
#define MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHARE_MASK             0x1
#define MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHARE_SHIFT            12
#define MT6337_PMIC_AUXADC_SPL_NUM_LARGE_ADDR                 MT6337_AUXADC_CON3
#define MT6337_PMIC_AUXADC_SPL_NUM_LARGE_MASK                 0x3FF
#define MT6337_PMIC_AUXADC_SPL_NUM_LARGE_SHIFT                0
#define MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_ADDR                 MT6337_AUXADC_CON4
#define MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_MASK                 0x3FF
#define MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SHIFT                0
#define MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SEL_ADDR             MT6337_AUXADC_CON4
#define MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SEL_MASK             0x1
#define MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SEL_SHIFT            15
#define MT6337_PMIC_AUXADC_SPL_NUM_SEL_ADDR                   MT6337_AUXADC_CON5
#define MT6337_PMIC_AUXADC_SPL_NUM_SEL_MASK                   0xFFF
#define MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHIFT                  0
#define MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHARE_ADDR             MT6337_AUXADC_CON5
#define MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHARE_MASK             0x1
#define MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHARE_SHIFT            12
#define MT6337_PMIC_AUXADC_TRIM_CH0_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH0_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH0_SEL_SHIFT                 0
#define MT6337_PMIC_AUXADC_TRIM_CH1_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH1_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH1_SEL_SHIFT                 2
#define MT6337_PMIC_AUXADC_TRIM_CH2_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH2_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH2_SEL_SHIFT                 4
#define MT6337_PMIC_AUXADC_TRIM_CH3_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH3_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH3_SEL_SHIFT                 6
#define MT6337_PMIC_AUXADC_TRIM_CH4_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH4_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH4_SEL_SHIFT                 8
#define MT6337_PMIC_AUXADC_TRIM_CH5_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH5_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH5_SEL_SHIFT                 10
#define MT6337_PMIC_AUXADC_TRIM_CH6_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH6_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH6_SEL_SHIFT                 12
#define MT6337_PMIC_AUXADC_TRIM_CH7_SEL_ADDR                  MT6337_AUXADC_CON6
#define MT6337_PMIC_AUXADC_TRIM_CH7_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH7_SEL_SHIFT                 14
#define MT6337_PMIC_AUXADC_TRIM_CH8_SEL_ADDR                  MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_TRIM_CH8_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH8_SEL_SHIFT                 0
#define MT6337_PMIC_AUXADC_TRIM_CH9_SEL_ADDR                  MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_TRIM_CH9_SEL_MASK                  0x3
#define MT6337_PMIC_AUXADC_TRIM_CH9_SEL_SHIFT                 2
#define MT6337_PMIC_AUXADC_TRIM_CH10_SEL_ADDR                 MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_TRIM_CH10_SEL_MASK                 0x3
#define MT6337_PMIC_AUXADC_TRIM_CH10_SEL_SHIFT                4
#define MT6337_PMIC_AUXADC_TRIM_CH11_SEL_ADDR                 MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_TRIM_CH11_SEL_MASK                 0x3
#define MT6337_PMIC_AUXADC_TRIM_CH11_SEL_SHIFT                6
#define MT6337_PMIC_AUXADC_TRIM_CH12_SEL_ADDR                 MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_TRIM_CH12_SEL_MASK                 0x3
#define MT6337_PMIC_AUXADC_TRIM_CH12_SEL_SHIFT                8
#define MT6337_PMIC_AUXADC_ADC_2S_COMP_ENB_ADDR               MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_ADC_2S_COMP_ENB_MASK               0x1
#define MT6337_PMIC_AUXADC_ADC_2S_COMP_ENB_SHIFT              14
#define MT6337_PMIC_AUXADC_ADC_TRIM_COMP_ADDR                 MT6337_AUXADC_CON7
#define MT6337_PMIC_AUXADC_ADC_TRIM_COMP_MASK                 0x1
#define MT6337_PMIC_AUXADC_ADC_TRIM_COMP_SHIFT                15
#define MT6337_PMIC_AUXADC_SW_GAIN_TRIM_ADDR                  MT6337_AUXADC_CON8
#define MT6337_PMIC_AUXADC_SW_GAIN_TRIM_MASK                  0x7FFF
#define MT6337_PMIC_AUXADC_SW_GAIN_TRIM_SHIFT                 0
#define MT6337_PMIC_AUXADC_SW_OFFSET_TRIM_ADDR                MT6337_AUXADC_CON9
#define MT6337_PMIC_AUXADC_SW_OFFSET_TRIM_MASK                0x7FFF
#define MT6337_PMIC_AUXADC_SW_OFFSET_TRIM_SHIFT               0
#define MT6337_PMIC_AUXADC_RNG_EN_ADDR                        MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_RNG_EN_MASK                        0x1
#define MT6337_PMIC_AUXADC_RNG_EN_SHIFT                       0
#define MT6337_PMIC_AUXADC_DATA_REUSE_SEL_ADDR                MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_DATA_REUSE_SEL_MASK                0x3
#define MT6337_PMIC_AUXADC_DATA_REUSE_SEL_SHIFT               1
#define MT6337_PMIC_AUXADC_TEST_MODE_ADDR                     MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_TEST_MODE_MASK                     0x1
#define MT6337_PMIC_AUXADC_TEST_MODE_SHIFT                    3
#define MT6337_PMIC_AUXADC_BIT_SEL_ADDR                       MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_BIT_SEL_MASK                       0x1
#define MT6337_PMIC_AUXADC_BIT_SEL_SHIFT                      4
#define MT6337_PMIC_AUXADC_START_SW_ADDR                      MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_START_SW_MASK                      0x1
#define MT6337_PMIC_AUXADC_START_SW_SHIFT                     5
#define MT6337_PMIC_AUXADC_START_SWCTRL_ADDR                  MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_START_SWCTRL_MASK                  0x1
#define MT6337_PMIC_AUXADC_START_SWCTRL_SHIFT                 6
#define MT6337_PMIC_AUXADC_TS_VBE_SEL_ADDR                    MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_TS_VBE_SEL_MASK                    0x1
#define MT6337_PMIC_AUXADC_TS_VBE_SEL_SHIFT                   7
#define MT6337_PMIC_AUXADC_TS_VBE_SEL_SWCTRL_ADDR             MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_TS_VBE_SEL_SWCTRL_MASK             0x1
#define MT6337_PMIC_AUXADC_TS_VBE_SEL_SWCTRL_SHIFT            8
#define MT6337_PMIC_AUXADC_VBUF_EN_ADDR                       MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_VBUF_EN_MASK                       0x1
#define MT6337_PMIC_AUXADC_VBUF_EN_SHIFT                      9
#define MT6337_PMIC_AUXADC_VBUF_EN_SWCTRL_ADDR                MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_VBUF_EN_SWCTRL_MASK                0x1
#define MT6337_PMIC_AUXADC_VBUF_EN_SWCTRL_SHIFT               10
#define MT6337_PMIC_AUXADC_OUT_SEL_ADDR                       MT6337_AUXADC_CON10
#define MT6337_PMIC_AUXADC_OUT_SEL_MASK                       0x1
#define MT6337_PMIC_AUXADC_OUT_SEL_SHIFT                      11
#define MT6337_PMIC_AUXADC_DA_DAC_ADDR                        MT6337_AUXADC_CON11
#define MT6337_PMIC_AUXADC_DA_DAC_MASK                        0xFFF
#define MT6337_PMIC_AUXADC_DA_DAC_SHIFT                       0
#define MT6337_PMIC_AUXADC_DA_DAC_SWCTRL_ADDR                 MT6337_AUXADC_CON11
#define MT6337_PMIC_AUXADC_DA_DAC_SWCTRL_MASK                 0x1
#define MT6337_PMIC_AUXADC_DA_DAC_SWCTRL_SHIFT                12
#define MT6337_PMIC_AD_AUXADC_COMP_ADDR                       MT6337_AUXADC_CON11
#define MT6337_PMIC_AD_AUXADC_COMP_MASK                       0x1
#define MT6337_PMIC_AD_AUXADC_COMP_SHIFT                      15
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_EN_ADDR                 MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_EN_MASK                 0x1
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_EN_SHIFT                0
#define MT6337_PMIC_AUXADC_ADCIN_VBAT_EN_ADDR                 MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ADCIN_VBAT_EN_MASK                 0x1
#define MT6337_PMIC_AUXADC_ADCIN_VBAT_EN_SHIFT                1
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_MUX_EN_ADDR             MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_MUX_EN_MASK             0x1
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_MUX_EN_SHIFT            2
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_EXT_BATON_EN_ADDR       MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_EXT_BATON_EN_MASK       0x1
#define MT6337_PMIC_AUXADC_ADCIN_VSEN_EXT_BATON_EN_SHIFT      3
#define MT6337_PMIC_AUXADC_ADCIN_CHR_EN_ADDR                  MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ADCIN_CHR_EN_MASK                  0x1
#define MT6337_PMIC_AUXADC_ADCIN_CHR_EN_SHIFT                 4
#define MT6337_PMIC_AUXADC_ADCIN_BATON_TDET_EN_ADDR           MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ADCIN_BATON_TDET_EN_MASK           0x1
#define MT6337_PMIC_AUXADC_ADCIN_BATON_TDET_EN_SHIFT          5
#define MT6337_PMIC_AUXADC_ACCDET_ANASWCTRL_EN_ADDR           MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_ACCDET_ANASWCTRL_EN_MASK           0x1
#define MT6337_PMIC_AUXADC_ACCDET_ANASWCTRL_EN_SHIFT          6
#define MT6337_PMIC_AUXADC_DIG0_RSV0_ADDR                     MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_DIG0_RSV0_MASK                     0xF
#define MT6337_PMIC_AUXADC_DIG0_RSV0_SHIFT                    7
#define MT6337_PMIC_AUXADC_CHSEL_ADDR                         MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_CHSEL_MASK                         0xF
#define MT6337_PMIC_AUXADC_CHSEL_SHIFT                        11
#define MT6337_PMIC_AUXADC_SWCTRL_EN_ADDR                     MT6337_AUXADC_CON12
#define MT6337_PMIC_AUXADC_SWCTRL_EN_MASK                     0x1
#define MT6337_PMIC_AUXADC_SWCTRL_EN_SHIFT                    15
#define MT6337_PMIC_AUXADC_SOURCE_LBAT_SEL_ADDR               MT6337_AUXADC_CON13
#define MT6337_PMIC_AUXADC_SOURCE_LBAT_SEL_MASK               0x1
#define MT6337_PMIC_AUXADC_SOURCE_LBAT_SEL_SHIFT              0
#define MT6337_PMIC_AUXADC_SOURCE_LBAT2_SEL_ADDR              MT6337_AUXADC_CON13
#define MT6337_PMIC_AUXADC_SOURCE_LBAT2_SEL_MASK              0x1
#define MT6337_PMIC_AUXADC_SOURCE_LBAT2_SEL_SHIFT             1
#define MT6337_PMIC_AUXADC_DIG0_RSV2_ADDR                     MT6337_AUXADC_CON13
#define MT6337_PMIC_AUXADC_DIG0_RSV2_MASK                     0x7
#define MT6337_PMIC_AUXADC_DIG0_RSV2_SHIFT                    2
#define MT6337_PMIC_AUXADC_DIG1_RSV2_ADDR                     MT6337_AUXADC_CON13
#define MT6337_PMIC_AUXADC_DIG1_RSV2_MASK                     0xF
#define MT6337_PMIC_AUXADC_DIG1_RSV2_SHIFT                    5
#define MT6337_PMIC_AUXADC_DAC_EXTD_ADDR                      MT6337_AUXADC_CON13
#define MT6337_PMIC_AUXADC_DAC_EXTD_MASK                      0xF
#define MT6337_PMIC_AUXADC_DAC_EXTD_SHIFT                     11
#define MT6337_PMIC_AUXADC_DAC_EXTD_EN_ADDR                   MT6337_AUXADC_CON13
#define MT6337_PMIC_AUXADC_DAC_EXTD_EN_MASK                   0x1
#define MT6337_PMIC_AUXADC_DAC_EXTD_EN_SHIFT                  15
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_SW_ADDR                MT6337_AUXADC_CON14
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_SW_MASK                0x1
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_SW_SHIFT               10
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_SEL_ADDR               MT6337_AUXADC_CON14
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_SEL_MASK               0x1
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_SEL_SHIFT              11
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_STATUS_ADDR            MT6337_AUXADC_CON14
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_STATUS_MASK            0x1
#define MT6337_PMIC_AUXADC_PMU_THR_PDN_STATUS_SHIFT           12
#define MT6337_PMIC_AUXADC_DIG0_RSV1_ADDR                     MT6337_AUXADC_CON14
#define MT6337_PMIC_AUXADC_DIG0_RSV1_MASK                     0x7
#define MT6337_PMIC_AUXADC_DIG0_RSV1_SHIFT                    13
#define MT6337_PMIC_AUXADC_START_SHADE_NUM_ADDR               MT6337_AUXADC_CON15
#define MT6337_PMIC_AUXADC_START_SHADE_NUM_MASK               0x3FF
#define MT6337_PMIC_AUXADC_START_SHADE_NUM_SHIFT              0
#define MT6337_PMIC_AUXADC_START_SHADE_EN_ADDR                MT6337_AUXADC_CON15
#define MT6337_PMIC_AUXADC_START_SHADE_EN_MASK                0x1
#define MT6337_PMIC_AUXADC_START_SHADE_EN_SHIFT               14
#define MT6337_PMIC_AUXADC_START_SHADE_SEL_ADDR               MT6337_AUXADC_CON15
#define MT6337_PMIC_AUXADC_START_SHADE_SEL_MASK               0x1
#define MT6337_PMIC_AUXADC_START_SHADE_SEL_SHIFT              15
#define MT6337_PMIC_AUXADC_AUTORPT_PRD_ADDR                   MT6337_AUXADC_AUTORPT0
#define MT6337_PMIC_AUXADC_AUTORPT_PRD_MASK                   0x3FF
#define MT6337_PMIC_AUXADC_AUTORPT_PRD_SHIFT                  0
#define MT6337_PMIC_AUXADC_AUTORPT_EN_ADDR                    MT6337_AUXADC_AUTORPT0
#define MT6337_PMIC_AUXADC_AUTORPT_EN_MASK                    0x1
#define MT6337_PMIC_AUXADC_AUTORPT_EN_SHIFT                   15
#define MT6337_PMIC_AUXADC_ACCDET_AUTO_SPL_ADDR               MT6337_AUXADC_ACCDET
#define MT6337_PMIC_AUXADC_ACCDET_AUTO_SPL_MASK               0x1
#define MT6337_PMIC_AUXADC_ACCDET_AUTO_SPL_SHIFT              0
#define MT6337_PMIC_AUXADC_ACCDET_AUTO_RQST_CLR_ADDR          MT6337_AUXADC_ACCDET
#define MT6337_PMIC_AUXADC_ACCDET_AUTO_RQST_CLR_MASK          0x1
#define MT6337_PMIC_AUXADC_ACCDET_AUTO_RQST_CLR_SHIFT         1
#define MT6337_PMIC_AUXADC_ACCDET_DIG1_RSV0_ADDR              MT6337_AUXADC_ACCDET
#define MT6337_PMIC_AUXADC_ACCDET_DIG1_RSV0_MASK              0x3F
#define MT6337_PMIC_AUXADC_ACCDET_DIG1_RSV0_SHIFT             2
#define MT6337_PMIC_AUXADC_ACCDET_DIG0_RSV0_ADDR              MT6337_AUXADC_ACCDET
#define MT6337_PMIC_AUXADC_ACCDET_DIG0_RSV0_MASK              0xFF
#define MT6337_PMIC_AUXADC_ACCDET_DIG0_RSV0_SHIFT             8
#define MT6337_PMIC_AUXADC_THR_DEBT_MAX_ADDR                  MT6337_AUXADC_THR0
#define MT6337_PMIC_AUXADC_THR_DEBT_MAX_MASK                  0xFF
#define MT6337_PMIC_AUXADC_THR_DEBT_MAX_SHIFT                 0
#define MT6337_PMIC_AUXADC_THR_DEBT_MIN_ADDR                  MT6337_AUXADC_THR0
#define MT6337_PMIC_AUXADC_THR_DEBT_MIN_MASK                  0xFF
#define MT6337_PMIC_AUXADC_THR_DEBT_MIN_SHIFT                 8
#define MT6337_PMIC_AUXADC_THR_DET_PRD_15_0_ADDR              MT6337_AUXADC_THR1
#define MT6337_PMIC_AUXADC_THR_DET_PRD_15_0_MASK              0xFFFF
#define MT6337_PMIC_AUXADC_THR_DET_PRD_15_0_SHIFT             0
#define MT6337_PMIC_AUXADC_THR_DET_PRD_19_16_ADDR             MT6337_AUXADC_THR2
#define MT6337_PMIC_AUXADC_THR_DET_PRD_19_16_MASK             0xF
#define MT6337_PMIC_AUXADC_THR_DET_PRD_19_16_SHIFT            0
#define MT6337_PMIC_AUXADC_THR_VOLT_MAX_ADDR                  MT6337_AUXADC_THR3
#define MT6337_PMIC_AUXADC_THR_VOLT_MAX_MASK                  0xFFF
#define MT6337_PMIC_AUXADC_THR_VOLT_MAX_SHIFT                 0
#define MT6337_PMIC_AUXADC_THR_IRQ_EN_MAX_ADDR                MT6337_AUXADC_THR3
#define MT6337_PMIC_AUXADC_THR_IRQ_EN_MAX_MASK                0x1
#define MT6337_PMIC_AUXADC_THR_IRQ_EN_MAX_SHIFT               12
#define MT6337_PMIC_AUXADC_THR_EN_MAX_ADDR                    MT6337_AUXADC_THR3
#define MT6337_PMIC_AUXADC_THR_EN_MAX_MASK                    0x1
#define MT6337_PMIC_AUXADC_THR_EN_MAX_SHIFT                   13
#define MT6337_PMIC_AUXADC_THR_MAX_IRQ_B_ADDR                 MT6337_AUXADC_THR3
#define MT6337_PMIC_AUXADC_THR_MAX_IRQ_B_MASK                 0x1
#define MT6337_PMIC_AUXADC_THR_MAX_IRQ_B_SHIFT                15
#define MT6337_PMIC_AUXADC_THR_VOLT_MIN_ADDR                  MT6337_AUXADC_THR4
#define MT6337_PMIC_AUXADC_THR_VOLT_MIN_MASK                  0xFFF
#define MT6337_PMIC_AUXADC_THR_VOLT_MIN_SHIFT                 0
#define MT6337_PMIC_AUXADC_THR_IRQ_EN_MIN_ADDR                MT6337_AUXADC_THR4
#define MT6337_PMIC_AUXADC_THR_IRQ_EN_MIN_MASK                0x1
#define MT6337_PMIC_AUXADC_THR_IRQ_EN_MIN_SHIFT               12
#define MT6337_PMIC_AUXADC_THR_EN_MIN_ADDR                    MT6337_AUXADC_THR4
#define MT6337_PMIC_AUXADC_THR_EN_MIN_MASK                    0x1
#define MT6337_PMIC_AUXADC_THR_EN_MIN_SHIFT                   13
#define MT6337_PMIC_AUXADC_THR_MIN_IRQ_B_ADDR                 MT6337_AUXADC_THR4
#define MT6337_PMIC_AUXADC_THR_MIN_IRQ_B_MASK                 0x1
#define MT6337_PMIC_AUXADC_THR_MIN_IRQ_B_SHIFT                15
#define MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MAX_ADDR        MT6337_AUXADC_THR5
#define MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MAX_MASK        0x1FF
#define MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MAX_SHIFT       0
#define MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN_ADDR        MT6337_AUXADC_THR6
#define MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN_MASK        0x1FF
#define MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN_SHIFT       0
#define MT6337_PMIC_EFUSE_GAIN_CH4_TRIM_ADDR                  MT6337_AUXADC_EFUSE0
#define MT6337_PMIC_EFUSE_GAIN_CH4_TRIM_MASK                  0xFFF
#define MT6337_PMIC_EFUSE_GAIN_CH4_TRIM_SHIFT                 0
#define MT6337_PMIC_EFUSE_OFFSET_CH4_TRIM_ADDR                MT6337_AUXADC_EFUSE1
#define MT6337_PMIC_EFUSE_OFFSET_CH4_TRIM_MASK                0x7FF
#define MT6337_PMIC_EFUSE_OFFSET_CH4_TRIM_SHIFT               0
#define MT6337_PMIC_EFUSE_GAIN_CH0_TRIM_ADDR                  MT6337_AUXADC_EFUSE2
#define MT6337_PMIC_EFUSE_GAIN_CH0_TRIM_MASK                  0xFFF
#define MT6337_PMIC_EFUSE_GAIN_CH0_TRIM_SHIFT                 0
#define MT6337_PMIC_EFUSE_OFFSET_CH0_TRIM_ADDR                MT6337_AUXADC_EFUSE3
#define MT6337_PMIC_EFUSE_OFFSET_CH0_TRIM_MASK                0x7FF
#define MT6337_PMIC_EFUSE_OFFSET_CH0_TRIM_SHIFT               0
#define MT6337_PMIC_EFUSE_GAIN_CH7_TRIM_ADDR                  MT6337_AUXADC_EFUSE4
#define MT6337_PMIC_EFUSE_GAIN_CH7_TRIM_MASK                  0xFFF
#define MT6337_PMIC_EFUSE_GAIN_CH7_TRIM_SHIFT                 0
#define MT6337_PMIC_EFUSE_OFFSET_CH7_TRIM_ADDR                MT6337_AUXADC_EFUSE5
#define MT6337_PMIC_EFUSE_OFFSET_CH7_TRIM_MASK                0x7FF
#define MT6337_PMIC_EFUSE_OFFSET_CH7_TRIM_SHIFT               0
#define MT6337_PMIC_AUXADC_DBG_DIG0_RSV2_ADDR                 MT6337_AUXADC_DBG0
#define MT6337_PMIC_AUXADC_DBG_DIG0_RSV2_MASK                 0x3F
#define MT6337_PMIC_AUXADC_DBG_DIG0_RSV2_SHIFT                4
#define MT6337_PMIC_AUXADC_DBG_DIG1_RSV2_ADDR                 MT6337_AUXADC_DBG0
#define MT6337_PMIC_AUXADC_DBG_DIG1_RSV2_MASK                 0x3F
#define MT6337_PMIC_AUXADC_DBG_DIG1_RSV2_SHIFT                10
#define MT6337_PMIC_RG_AUXADC_CALI_ADDR                       MT6337_AUXADC_ANA_CON0
#define MT6337_PMIC_RG_AUXADC_CALI_MASK                       0xF
#define MT6337_PMIC_RG_AUXADC_CALI_SHIFT                      0
#define MT6337_PMIC_RG_AUX_RSV_ADDR                           MT6337_AUXADC_ANA_CON0
#define MT6337_PMIC_RG_AUX_RSV_MASK                           0x7
#define MT6337_PMIC_RG_AUX_RSV_SHIFT                          4
#define MT6337_PMIC_RG_VBUF_BYP_ADDR                          MT6337_AUXADC_ANA_CON0
#define MT6337_PMIC_RG_VBUF_BYP_MASK                          0x1
#define MT6337_PMIC_RG_VBUF_BYP_SHIFT                         8
#define MT6337_PMIC_RG_VBUF_CALEN_ADDR                        MT6337_AUXADC_ANA_CON0
#define MT6337_PMIC_RG_VBUF_CALEN_MASK                        0x1
#define MT6337_PMIC_RG_VBUF_CALEN_SHIFT                       9
#define MT6337_PMIC_RG_VBUF_EXTEN_ADDR                        MT6337_AUXADC_ANA_CON0
#define MT6337_PMIC_RG_VBUF_EXTEN_MASK                        0x1
#define MT6337_PMIC_RG_VBUF_EXTEN_SHIFT                       10
#define MT6337_PMIC_RG_RNG_MOD_ADDR                           MT6337_AUXADC_ANA_CON1
#define MT6337_PMIC_RG_RNG_MOD_MASK                           0x1
#define MT6337_PMIC_RG_RNG_MOD_SHIFT                          0
#define MT6337_PMIC_RG_RNG_ANA_EN_ADDR                        MT6337_AUXADC_ANA_CON1
#define MT6337_PMIC_RG_RNG_ANA_EN_MASK                        0x1
#define MT6337_PMIC_RG_RNG_ANA_EN_SHIFT                       1
#define MT6337_PMIC_RG_RNG_SEL_ADDR                           MT6337_AUXADC_ANA_CON1
#define MT6337_PMIC_RG_RNG_SEL_MASK                           0x1
#define MT6337_PMIC_RG_RNG_SEL_SHIFT                          2
#define MT6337_PMIC_AD_AUDACCDETCMPOC_ADDR                    MT6337_ACCDET_CON0
#define MT6337_PMIC_AD_AUDACCDETCMPOC_MASK                    0x1
#define MT6337_PMIC_AD_AUDACCDETCMPOC_SHIFT                   0
#define MT6337_PMIC_RG_AUDACCDETANASWCTRLENB_ADDR             MT6337_ACCDET_CON0
#define MT6337_PMIC_RG_AUDACCDETANASWCTRLENB_MASK             0x1
#define MT6337_PMIC_RG_AUDACCDETANASWCTRLENB_SHIFT            2
#define MT6337_PMIC_RG_ACCDETSEL_ADDR                         MT6337_ACCDET_CON0
#define MT6337_PMIC_RG_ACCDETSEL_MASK                         0x1
#define MT6337_PMIC_RG_ACCDETSEL_SHIFT                        3
#define MT6337_PMIC_RG_AUDACCDETSWCTRL_ADDR                   MT6337_ACCDET_CON0
#define MT6337_PMIC_RG_AUDACCDETSWCTRL_MASK                   0x7
#define MT6337_PMIC_RG_AUDACCDETSWCTRL_SHIFT                  4
#define MT6337_PMIC_RG_AUDACCDETTVDET_ADDR                    MT6337_ACCDET_CON0
#define MT6337_PMIC_RG_AUDACCDETTVDET_MASK                    0x1
#define MT6337_PMIC_RG_AUDACCDETTVDET_SHIFT                   8
#define MT6337_PMIC_AUDACCDETAUXADCSWCTRL_ADDR                MT6337_ACCDET_CON0
#define MT6337_PMIC_AUDACCDETAUXADCSWCTRL_MASK                0x1
#define MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SHIFT               10
#define MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SEL_ADDR            MT6337_ACCDET_CON0
#define MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SEL_MASK            0x1
#define MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SEL_SHIFT           11
#define MT6337_PMIC_RG_AUDACCDETRSV_ADDR                      MT6337_ACCDET_CON0
#define MT6337_PMIC_RG_AUDACCDETRSV_MASK                      0x3
#define MT6337_PMIC_RG_AUDACCDETRSV_SHIFT                     13
#define MT6337_PMIC_ACCDET_SW_EN_ADDR                         MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_SW_EN_MASK                         0x1
#define MT6337_PMIC_ACCDET_SW_EN_SHIFT                        0
#define MT6337_PMIC_ACCDET_SEQ_INIT_ADDR                      MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_SEQ_INIT_MASK                      0x1
#define MT6337_PMIC_ACCDET_SEQ_INIT_SHIFT                     1
#define MT6337_PMIC_ACCDET_EINT_EN_ADDR                       MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_EINT_EN_MASK                       0x1
#define MT6337_PMIC_ACCDET_EINT_EN_SHIFT                      2
#define MT6337_PMIC_ACCDET_EINT_SEQ_INIT_ADDR                 MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_EINT_SEQ_INIT_MASK                 0x1
#define MT6337_PMIC_ACCDET_EINT_SEQ_INIT_SHIFT                3
#define MT6337_PMIC_ACCDET_EINT1_EN_ADDR                      MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_EINT1_EN_MASK                      0x1
#define MT6337_PMIC_ACCDET_EINT1_EN_SHIFT                     4
#define MT6337_PMIC_ACCDET_EINT1_SEQ_INIT_ADDR                MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_EINT1_SEQ_INIT_MASK                0x1
#define MT6337_PMIC_ACCDET_EINT1_SEQ_INIT_SHIFT               5
#define MT6337_PMIC_ACCDET_NEGVDET_EN_ADDR                    MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_NEGVDET_EN_MASK                    0x1
#define MT6337_PMIC_ACCDET_NEGVDET_EN_SHIFT                   6
#define MT6337_PMIC_ACCDET_NEGVDET_EN_CTRL_ADDR               MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_NEGVDET_EN_CTRL_MASK               0x1
#define MT6337_PMIC_ACCDET_NEGVDET_EN_CTRL_SHIFT              7
#define MT6337_PMIC_ACCDET_ANASWCTRL_SEL_ADDR                 MT6337_ACCDET_CON1
#define MT6337_PMIC_ACCDET_ANASWCTRL_SEL_MASK                 0x1
#define MT6337_PMIC_ACCDET_ANASWCTRL_SEL_SHIFT                8
#define MT6337_PMIC_ACCDET_CMP_PWM_EN_ADDR                    MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_CMP_PWM_EN_MASK                    0x1
#define MT6337_PMIC_ACCDET_CMP_PWM_EN_SHIFT                   0
#define MT6337_PMIC_ACCDET_VTH_PWM_EN_ADDR                    MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_VTH_PWM_EN_MASK                    0x1
#define MT6337_PMIC_ACCDET_VTH_PWM_EN_SHIFT                   1
#define MT6337_PMIC_ACCDET_MBIAS_PWM_EN_ADDR                  MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_MBIAS_PWM_EN_MASK                  0x1
#define MT6337_PMIC_ACCDET_MBIAS_PWM_EN_SHIFT                 2
#define MT6337_PMIC_ACCDET_CMP1_PWM_EN_ADDR                   MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_CMP1_PWM_EN_MASK                   0x1
#define MT6337_PMIC_ACCDET_CMP1_PWM_EN_SHIFT                  3
#define MT6337_PMIC_ACCDET_EINT_PWM_EN_ADDR                   MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_EINT_PWM_EN_MASK                   0x1
#define MT6337_PMIC_ACCDET_EINT_PWM_EN_SHIFT                  4
#define MT6337_PMIC_ACCDET_EINT1_PWM_EN_ADDR                  MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_EINT1_PWM_EN_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT1_PWM_EN_SHIFT                 5
#define MT6337_PMIC_ACCDET_CMP_PWM_IDLE_ADDR                  MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_CMP_PWM_IDLE_MASK                  0x1
#define MT6337_PMIC_ACCDET_CMP_PWM_IDLE_SHIFT                 6
#define MT6337_PMIC_ACCDET_VTH_PWM_IDLE_ADDR                  MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_VTH_PWM_IDLE_MASK                  0x1
#define MT6337_PMIC_ACCDET_VTH_PWM_IDLE_SHIFT                 7
#define MT6337_PMIC_ACCDET_MBIAS_PWM_IDLE_ADDR                MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_MBIAS_PWM_IDLE_MASK                0x1
#define MT6337_PMIC_ACCDET_MBIAS_PWM_IDLE_SHIFT               8
#define MT6337_PMIC_ACCDET_CMP1_PWM_IDLE_ADDR                 MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_CMP1_PWM_IDLE_MASK                 0x1
#define MT6337_PMIC_ACCDET_CMP1_PWM_IDLE_SHIFT                9
#define MT6337_PMIC_ACCDET_EINT_PWM_IDLE_ADDR                 MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_EINT_PWM_IDLE_MASK                 0x1
#define MT6337_PMIC_ACCDET_EINT_PWM_IDLE_SHIFT                10
#define MT6337_PMIC_ACCDET_EINT1_PWM_IDLE_ADDR                MT6337_ACCDET_CON2
#define MT6337_PMIC_ACCDET_EINT1_PWM_IDLE_MASK                0x1
#define MT6337_PMIC_ACCDET_EINT1_PWM_IDLE_SHIFT               11
#define MT6337_PMIC_ACCDET_PWM_WIDTH_ADDR                     MT6337_ACCDET_CON3
#define MT6337_PMIC_ACCDET_PWM_WIDTH_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_PWM_WIDTH_SHIFT                    0
#define MT6337_PMIC_ACCDET_PWM_THRESH_ADDR                    MT6337_ACCDET_CON4
#define MT6337_PMIC_ACCDET_PWM_THRESH_MASK                    0xFFFF
#define MT6337_PMIC_ACCDET_PWM_THRESH_SHIFT                   0
#define MT6337_PMIC_ACCDET_RISE_DELAY_ADDR                    MT6337_ACCDET_CON5
#define MT6337_PMIC_ACCDET_RISE_DELAY_MASK                    0x7FFF
#define MT6337_PMIC_ACCDET_RISE_DELAY_SHIFT                   0
#define MT6337_PMIC_ACCDET_FALL_DELAY_ADDR                    MT6337_ACCDET_CON5
#define MT6337_PMIC_ACCDET_FALL_DELAY_MASK                    0x1
#define MT6337_PMIC_ACCDET_FALL_DELAY_SHIFT                   15
#define MT6337_PMIC_ACCDET_DEBOUNCE0_ADDR                     MT6337_ACCDET_CON6
#define MT6337_PMIC_ACCDET_DEBOUNCE0_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE0_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE1_ADDR                     MT6337_ACCDET_CON7
#define MT6337_PMIC_ACCDET_DEBOUNCE1_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE1_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE2_ADDR                     MT6337_ACCDET_CON8
#define MT6337_PMIC_ACCDET_DEBOUNCE2_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE2_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE3_ADDR                     MT6337_ACCDET_CON9
#define MT6337_PMIC_ACCDET_DEBOUNCE3_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE3_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE4_ADDR                     MT6337_ACCDET_CON10
#define MT6337_PMIC_ACCDET_DEBOUNCE4_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE4_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE5_ADDR                     MT6337_ACCDET_CON11
#define MT6337_PMIC_ACCDET_DEBOUNCE5_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE5_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE6_ADDR                     MT6337_ACCDET_CON12
#define MT6337_PMIC_ACCDET_DEBOUNCE6_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE6_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE7_ADDR                     MT6337_ACCDET_CON13
#define MT6337_PMIC_ACCDET_DEBOUNCE7_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE7_SHIFT                    0
#define MT6337_PMIC_ACCDET_DEBOUNCE8_ADDR                     MT6337_ACCDET_CON14
#define MT6337_PMIC_ACCDET_DEBOUNCE8_MASK                     0xFFFF
#define MT6337_PMIC_ACCDET_DEBOUNCE8_SHIFT                    0
#define MT6337_PMIC_ACCDET_IVAL_CUR_IN_ADDR                   MT6337_ACCDET_CON15
#define MT6337_PMIC_ACCDET_IVAL_CUR_IN_MASK                   0x7
#define MT6337_PMIC_ACCDET_IVAL_CUR_IN_SHIFT                  0
#define MT6337_PMIC_ACCDET_IVAL_SAM_IN_ADDR                   MT6337_ACCDET_CON15
#define MT6337_PMIC_ACCDET_IVAL_SAM_IN_MASK                   0x7
#define MT6337_PMIC_ACCDET_IVAL_SAM_IN_SHIFT                  3
#define MT6337_PMIC_ACCDET_IVAL_MEM_IN_ADDR                   MT6337_ACCDET_CON15
#define MT6337_PMIC_ACCDET_IVAL_MEM_IN_MASK                   0x7
#define MT6337_PMIC_ACCDET_IVAL_MEM_IN_SHIFT                  6
#define MT6337_PMIC_ACCDET_IVAL_SEL_ADDR                      MT6337_ACCDET_CON15
#define MT6337_PMIC_ACCDET_IVAL_SEL_MASK                      0x1
#define MT6337_PMIC_ACCDET_IVAL_SEL_SHIFT                     15
#define MT6337_PMIC_ACCDET_EINT1_IVAL_CUR_IN_ADDR             MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT1_IVAL_CUR_IN_MASK             0x1
#define MT6337_PMIC_ACCDET_EINT1_IVAL_CUR_IN_SHIFT            0
#define MT6337_PMIC_ACCDET_EINT1_IVAL_SAM_IN_ADDR             MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT1_IVAL_SAM_IN_MASK             0x1
#define MT6337_PMIC_ACCDET_EINT1_IVAL_SAM_IN_SHIFT            1
#define MT6337_PMIC_ACCDET_EINT1_IVAL_MEM_IN_ADDR             MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT1_IVAL_MEM_IN_MASK             0x1
#define MT6337_PMIC_ACCDET_EINT1_IVAL_MEM_IN_SHIFT            2
#define MT6337_PMIC_ACCDET_EINT1_IVAL_SEL_ADDR                MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT1_IVAL_SEL_MASK                0x1
#define MT6337_PMIC_ACCDET_EINT1_IVAL_SEL_SHIFT               3
#define MT6337_PMIC_ACCDET_EINT_IVAL_CUR_IN_ADDR              MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT_IVAL_CUR_IN_MASK              0x1
#define MT6337_PMIC_ACCDET_EINT_IVAL_CUR_IN_SHIFT             12
#define MT6337_PMIC_ACCDET_EINT_IVAL_SAM_IN_ADDR              MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT_IVAL_SAM_IN_MASK              0x1
#define MT6337_PMIC_ACCDET_EINT_IVAL_SAM_IN_SHIFT             13
#define MT6337_PMIC_ACCDET_EINT_IVAL_MEM_IN_ADDR              MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT_IVAL_MEM_IN_MASK              0x1
#define MT6337_PMIC_ACCDET_EINT_IVAL_MEM_IN_SHIFT             14
#define MT6337_PMIC_ACCDET_EINT_IVAL_SEL_ADDR                 MT6337_ACCDET_CON16
#define MT6337_PMIC_ACCDET_EINT_IVAL_SEL_MASK                 0x1
#define MT6337_PMIC_ACCDET_EINT_IVAL_SEL_SHIFT                15
#define MT6337_PMIC_ACCDET_IRQ_ADDR                           MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_IRQ_MASK                           0x1
#define MT6337_PMIC_ACCDET_IRQ_SHIFT                          0
#define MT6337_PMIC_ACCDET_NEGV_IRQ_ADDR                      MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_NEGV_IRQ_MASK                      0x1
#define MT6337_PMIC_ACCDET_NEGV_IRQ_SHIFT                     1
#define MT6337_PMIC_ACCDET_EINT_IRQ_ADDR                      MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_EINT_IRQ_MASK                      0x1
#define MT6337_PMIC_ACCDET_EINT_IRQ_SHIFT                     2
#define MT6337_PMIC_ACCDET_EINT1_IRQ_ADDR                     MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_EINT1_IRQ_MASK                     0x1
#define MT6337_PMIC_ACCDET_EINT1_IRQ_SHIFT                    3
#define MT6337_PMIC_ACCDET_IRQ_CLR_ADDR                       MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_IRQ_CLR_MASK                       0x1
#define MT6337_PMIC_ACCDET_IRQ_CLR_SHIFT                      8
#define MT6337_PMIC_ACCDET_NEGV_IRQ_CLR_ADDR                  MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_NEGV_IRQ_CLR_MASK                  0x1
#define MT6337_PMIC_ACCDET_NEGV_IRQ_CLR_SHIFT                 9
#define MT6337_PMIC_ACCDET_EINT_IRQ_CLR_ADDR                  MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_EINT_IRQ_CLR_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT_IRQ_CLR_SHIFT                 10
#define MT6337_PMIC_ACCDET_EINT1_IRQ_CLR_ADDR                 MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_EINT1_IRQ_CLR_MASK                 0x1
#define MT6337_PMIC_ACCDET_EINT1_IRQ_CLR_SHIFT                11
#define MT6337_PMIC_ACCDET_EINT_IRQ_POLARITY_ADDR             MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_EINT_IRQ_POLARITY_MASK             0x1
#define MT6337_PMIC_ACCDET_EINT_IRQ_POLARITY_SHIFT            14
#define MT6337_PMIC_ACCDET_EINT1_IRQ_POLARITY_ADDR            MT6337_ACCDET_CON17
#define MT6337_PMIC_ACCDET_EINT1_IRQ_POLARITY_MASK            0x1
#define MT6337_PMIC_ACCDET_EINT1_IRQ_POLARITY_SHIFT           15
#define MT6337_PMIC_ACCDET_TEST_MODE0_ADDR                    MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_TEST_MODE0_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE0_SHIFT                   0
#define MT6337_PMIC_ACCDET_CMP_SWSEL_ADDR                     MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_CMP_SWSEL_MASK                     0x1
#define MT6337_PMIC_ACCDET_CMP_SWSEL_SHIFT                    1
#define MT6337_PMIC_ACCDET_VTH_SWSEL_ADDR                     MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_VTH_SWSEL_MASK                     0x1
#define MT6337_PMIC_ACCDET_VTH_SWSEL_SHIFT                    2
#define MT6337_PMIC_ACCDET_MBIAS_SWSEL_ADDR                   MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_MBIAS_SWSEL_MASK                   0x1
#define MT6337_PMIC_ACCDET_MBIAS_SWSEL_SHIFT                  3
#define MT6337_PMIC_ACCDET_CMP1_SWSEL_ADDR                    MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_CMP1_SWSEL_MASK                    0x1
#define MT6337_PMIC_ACCDET_CMP1_SWSEL_SHIFT                   4
#define MT6337_PMIC_ACCDET_PWM_SEL_ADDR                       MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_PWM_SEL_MASK                       0x3
#define MT6337_PMIC_ACCDET_PWM_SEL_SHIFT                      9
#define MT6337_PMIC_ACCDET_CMP1_EN_SW_ADDR                    MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_CMP1_EN_SW_MASK                    0x1
#define MT6337_PMIC_ACCDET_CMP1_EN_SW_SHIFT                   11
#define MT6337_PMIC_ACCDET_CMP_EN_SW_ADDR                     MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_CMP_EN_SW_MASK                     0x1
#define MT6337_PMIC_ACCDET_CMP_EN_SW_SHIFT                    12
#define MT6337_PMIC_ACCDET_VTH_EN_SW_ADDR                     MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_VTH_EN_SW_MASK                     0x1
#define MT6337_PMIC_ACCDET_VTH_EN_SW_SHIFT                    13
#define MT6337_PMIC_ACCDET_MBIAS_EN_SW_ADDR                   MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_MBIAS_EN_SW_MASK                   0x1
#define MT6337_PMIC_ACCDET_MBIAS_EN_SW_SHIFT                  14
#define MT6337_PMIC_ACCDET_PWM_EN_SW_ADDR                     MT6337_ACCDET_CON18
#define MT6337_PMIC_ACCDET_PWM_EN_SW_MASK                     0x1
#define MT6337_PMIC_ACCDET_PWM_EN_SW_SHIFT                    15
#define MT6337_PMIC_ACCDET_MBIAS_CLK_ADDR                     MT6337_ACCDET_CON19
#define MT6337_PMIC_ACCDET_MBIAS_CLK_MASK                     0x1
#define MT6337_PMIC_ACCDET_MBIAS_CLK_SHIFT                    0
#define MT6337_PMIC_ACCDET_VTH_CLK_ADDR                       MT6337_ACCDET_CON19
#define MT6337_PMIC_ACCDET_VTH_CLK_MASK                       0x1
#define MT6337_PMIC_ACCDET_VTH_CLK_SHIFT                      1
#define MT6337_PMIC_ACCDET_CMP_CLK_ADDR                       MT6337_ACCDET_CON19
#define MT6337_PMIC_ACCDET_CMP_CLK_MASK                       0x1
#define MT6337_PMIC_ACCDET_CMP_CLK_SHIFT                      2
#define MT6337_PMIC_ACCDET2_CMP_CLK_ADDR                      MT6337_ACCDET_CON19
#define MT6337_PMIC_ACCDET2_CMP_CLK_MASK                      0x1
#define MT6337_PMIC_ACCDET2_CMP_CLK_SHIFT                     3
#define MT6337_PMIC_DA_NI_AUDACCDETAUXADCSWCTRL_ADDR          MT6337_ACCDET_CON19
#define MT6337_PMIC_DA_NI_AUDACCDETAUXADCSWCTRL_MASK          0x1
#define MT6337_PMIC_DA_NI_AUDACCDETAUXADCSWCTRL_SHIFT         15
#define MT6337_PMIC_ACCDET_CMPC_ADDR                          MT6337_ACCDET_CON20
#define MT6337_PMIC_ACCDET_CMPC_MASK                          0x1
#define MT6337_PMIC_ACCDET_CMPC_SHIFT                         0
#define MT6337_PMIC_ACCDET_IN_ADDR                            MT6337_ACCDET_CON20
#define MT6337_PMIC_ACCDET_IN_MASK                            0x3
#define MT6337_PMIC_ACCDET_IN_SHIFT                           1
#define MT6337_PMIC_ACCDET_CUR_IN_ADDR                        MT6337_ACCDET_CON20
#define MT6337_PMIC_ACCDET_CUR_IN_MASK                        0x7
#define MT6337_PMIC_ACCDET_CUR_IN_SHIFT                       3
#define MT6337_PMIC_ACCDET_SAM_IN_ADDR                        MT6337_ACCDET_CON20
#define MT6337_PMIC_ACCDET_SAM_IN_MASK                        0x7
#define MT6337_PMIC_ACCDET_SAM_IN_SHIFT                       6
#define MT6337_PMIC_ACCDET_MEM_IN_ADDR                        MT6337_ACCDET_CON20
#define MT6337_PMIC_ACCDET_MEM_IN_MASK                        0x7
#define MT6337_PMIC_ACCDET_MEM_IN_SHIFT                       9
#define MT6337_PMIC_ACCDET_STATE_ADDR                         MT6337_ACCDET_CON20
#define MT6337_PMIC_ACCDET_STATE_MASK                         0x7
#define MT6337_PMIC_ACCDET_STATE_SHIFT                        13
#define MT6337_PMIC_ACCDET_EINT_DEB_SEL_ADDR                  MT6337_ACCDET_CON21
#define MT6337_PMIC_ACCDET_EINT_DEB_SEL_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT_DEB_SEL_SHIFT                 0
#define MT6337_PMIC_ACCDET_EINT_DEBOUNCE_ADDR                 MT6337_ACCDET_CON21
#define MT6337_PMIC_ACCDET_EINT_DEBOUNCE_MASK                 0xF
#define MT6337_PMIC_ACCDET_EINT_DEBOUNCE_SHIFT                3
#define MT6337_PMIC_ACCDET_EINT_PWM_THRESH_ADDR               MT6337_ACCDET_CON21
#define MT6337_PMIC_ACCDET_EINT_PWM_THRESH_MASK               0x7
#define MT6337_PMIC_ACCDET_EINT_PWM_THRESH_SHIFT              8
#define MT6337_PMIC_ACCDET_EINT_PWM_WIDTH_ADDR                MT6337_ACCDET_CON21
#define MT6337_PMIC_ACCDET_EINT_PWM_WIDTH_MASK                0x3
#define MT6337_PMIC_ACCDET_EINT_PWM_WIDTH_SHIFT               12
#define MT6337_PMIC_ACCDET_EINT1_DEB_SEL_ADDR                 MT6337_ACCDET_CON22
#define MT6337_PMIC_ACCDET_EINT1_DEB_SEL_MASK                 0x1
#define MT6337_PMIC_ACCDET_EINT1_DEB_SEL_SHIFT                0
#define MT6337_PMIC_ACCDET_EINT1_DEBOUNCE_ADDR                MT6337_ACCDET_CON22
#define MT6337_PMIC_ACCDET_EINT1_DEBOUNCE_MASK                0xF
#define MT6337_PMIC_ACCDET_EINT1_DEBOUNCE_SHIFT               3
#define MT6337_PMIC_ACCDET_EINT1_PWM_THRESH_ADDR              MT6337_ACCDET_CON22
#define MT6337_PMIC_ACCDET_EINT1_PWM_THRESH_MASK              0x7
#define MT6337_PMIC_ACCDET_EINT1_PWM_THRESH_SHIFT             8
#define MT6337_PMIC_ACCDET_EINT1_PWM_WIDTH_ADDR               MT6337_ACCDET_CON22
#define MT6337_PMIC_ACCDET_EINT1_PWM_WIDTH_MASK               0x3
#define MT6337_PMIC_ACCDET_EINT1_PWM_WIDTH_SHIFT              12
#define MT6337_PMIC_ACCDET_NEGV_THRESH_ADDR                   MT6337_ACCDET_CON23
#define MT6337_PMIC_ACCDET_NEGV_THRESH_MASK                   0x1F
#define MT6337_PMIC_ACCDET_NEGV_THRESH_SHIFT                  0
#define MT6337_PMIC_ACCDET_EINT_PWM_FALL_DELAY_ADDR           MT6337_ACCDET_CON23
#define MT6337_PMIC_ACCDET_EINT_PWM_FALL_DELAY_MASK           0x1
#define MT6337_PMIC_ACCDET_EINT_PWM_FALL_DELAY_SHIFT          5
#define MT6337_PMIC_ACCDET_EINT_PWM_RISE_DELAY_ADDR           MT6337_ACCDET_CON23
#define MT6337_PMIC_ACCDET_EINT_PWM_RISE_DELAY_MASK           0x3FF
#define MT6337_PMIC_ACCDET_EINT_PWM_RISE_DELAY_SHIFT          6
#define MT6337_PMIC_ACCDET_EINT1_PWM_FALL_DELAY_ADDR          MT6337_ACCDET_CON24
#define MT6337_PMIC_ACCDET_EINT1_PWM_FALL_DELAY_MASK          0x1
#define MT6337_PMIC_ACCDET_EINT1_PWM_FALL_DELAY_SHIFT         5
#define MT6337_PMIC_ACCDET_EINT1_PWM_RISE_DELAY_ADDR          MT6337_ACCDET_CON24
#define MT6337_PMIC_ACCDET_EINT1_PWM_RISE_DELAY_MASK          0x3FF
#define MT6337_PMIC_ACCDET_EINT1_PWM_RISE_DELAY_SHIFT         6
#define MT6337_PMIC_ACCDET_NVDETECTOUT_SW_ADDR                MT6337_ACCDET_CON25
#define MT6337_PMIC_ACCDET_NVDETECTOUT_SW_MASK                0x1
#define MT6337_PMIC_ACCDET_NVDETECTOUT_SW_SHIFT               0
#define MT6337_PMIC_ACCDET_EINT_CMPOUT_SW_ADDR                MT6337_ACCDET_CON25
#define MT6337_PMIC_ACCDET_EINT_CMPOUT_SW_MASK                0x1
#define MT6337_PMIC_ACCDET_EINT_CMPOUT_SW_SHIFT               1
#define MT6337_PMIC_ACCDET_EINT1_CMPOUT_SW_ADDR               MT6337_ACCDET_CON25
#define MT6337_PMIC_ACCDET_EINT1_CMPOUT_SW_MASK               0x1
#define MT6337_PMIC_ACCDET_EINT1_CMPOUT_SW_SHIFT              2
#define MT6337_PMIC_ACCDET_AUXADC_CTRL_SW_ADDR                MT6337_ACCDET_CON25
#define MT6337_PMIC_ACCDET_AUXADC_CTRL_SW_MASK                0x1
#define MT6337_PMIC_ACCDET_AUXADC_CTRL_SW_SHIFT               3
#define MT6337_PMIC_ACCDET_EINT_CMP_EN_SW_ADDR                MT6337_ACCDET_CON25
#define MT6337_PMIC_ACCDET_EINT_CMP_EN_SW_MASK                0x1
#define MT6337_PMIC_ACCDET_EINT_CMP_EN_SW_SHIFT               14
#define MT6337_PMIC_ACCDET_EINT1_CMP_EN_SW_ADDR               MT6337_ACCDET_CON25
#define MT6337_PMIC_ACCDET_EINT1_CMP_EN_SW_MASK               0x1
#define MT6337_PMIC_ACCDET_EINT1_CMP_EN_SW_SHIFT              15
#define MT6337_PMIC_ACCDET_TEST_MODE13_ADDR                   MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE13_MASK                   0x1
#define MT6337_PMIC_ACCDET_TEST_MODE13_SHIFT                  0
#define MT6337_PMIC_ACCDET_TEST_MODE12_ADDR                   MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE12_MASK                   0x1
#define MT6337_PMIC_ACCDET_TEST_MODE12_SHIFT                  1
#define MT6337_PMIC_ACCDET_TEST_MODE11_ADDR                   MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE11_MASK                   0x1
#define MT6337_PMIC_ACCDET_TEST_MODE11_SHIFT                  2
#define MT6337_PMIC_ACCDET_TEST_MODE10_ADDR                   MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE10_MASK                   0x1
#define MT6337_PMIC_ACCDET_TEST_MODE10_SHIFT                  3
#define MT6337_PMIC_ACCDET_TEST_MODE9_ADDR                    MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE9_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE9_SHIFT                   4
#define MT6337_PMIC_ACCDET_TEST_MODE8_ADDR                    MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE8_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE8_SHIFT                   5
#define MT6337_PMIC_ACCDET_TEST_MODE7_ADDR                    MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE7_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE7_SHIFT                   6
#define MT6337_PMIC_ACCDET_TEST_MODE6_ADDR                    MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE6_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE6_SHIFT                   7
#define MT6337_PMIC_ACCDET_TEST_MODE5_ADDR                    MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE5_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE5_SHIFT                   8
#define MT6337_PMIC_ACCDET_TEST_MODE4_ADDR                    MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_TEST_MODE4_MASK                    0x1
#define MT6337_PMIC_ACCDET_TEST_MODE4_SHIFT                   9
#define MT6337_PMIC_ACCDET_IN_SW_ADDR                         MT6337_ACCDET_CON26
#define MT6337_PMIC_ACCDET_IN_SW_MASK                         0x7
#define MT6337_PMIC_ACCDET_IN_SW_SHIFT                        10
#define MT6337_PMIC_ACCDET_EINT_STATE_ADDR                    MT6337_ACCDET_CON27
#define MT6337_PMIC_ACCDET_EINT_STATE_MASK                    0x7
#define MT6337_PMIC_ACCDET_EINT_STATE_SHIFT                   0
#define MT6337_PMIC_ACCDET_AUXADC_DEBOUNCE_END_ADDR           MT6337_ACCDET_CON27
#define MT6337_PMIC_ACCDET_AUXADC_DEBOUNCE_END_MASK           0x1
#define MT6337_PMIC_ACCDET_AUXADC_DEBOUNCE_END_SHIFT          3
#define MT6337_PMIC_ACCDET_AUXADC_CONNECT_PRE_ADDR            MT6337_ACCDET_CON27
#define MT6337_PMIC_ACCDET_AUXADC_CONNECT_PRE_MASK            0x1
#define MT6337_PMIC_ACCDET_AUXADC_CONNECT_PRE_SHIFT           4
#define MT6337_PMIC_ACCDET_EINT_CUR_IN_ADDR                   MT6337_ACCDET_CON27
#define MT6337_PMIC_ACCDET_EINT_CUR_IN_MASK                   0x1
#define MT6337_PMIC_ACCDET_EINT_CUR_IN_SHIFT                  8
#define MT6337_PMIC_ACCDET_EINT_SAM_IN_ADDR                   MT6337_ACCDET_CON27
#define MT6337_PMIC_ACCDET_EINT_SAM_IN_MASK                   0x1
#define MT6337_PMIC_ACCDET_EINT_SAM_IN_SHIFT                  9
#define MT6337_PMIC_ACCDET_EINT_MEM_IN_ADDR                   MT6337_ACCDET_CON27
#define MT6337_PMIC_ACCDET_EINT_MEM_IN_MASK                   0x1
#define MT6337_PMIC_ACCDET_EINT_MEM_IN_SHIFT                  10
#define MT6337_PMIC_AD_NVDETECTOUT_ADDR                       MT6337_ACCDET_CON27
#define MT6337_PMIC_AD_NVDETECTOUT_MASK                       0x1
#define MT6337_PMIC_AD_NVDETECTOUT_SHIFT                      13
#define MT6337_PMIC_AD_EINT1CMPOUT_ADDR                       MT6337_ACCDET_CON27
#define MT6337_PMIC_AD_EINT1CMPOUT_MASK                       0x1
#define MT6337_PMIC_AD_EINT1CMPOUT_SHIFT                      14
#define MT6337_PMIC_DA_NI_EINT1CMPEN_ADDR                     MT6337_ACCDET_CON27
#define MT6337_PMIC_DA_NI_EINT1CMPEN_MASK                     0x1
#define MT6337_PMIC_DA_NI_EINT1CMPEN_SHIFT                    15
#define MT6337_PMIC_ACCDET_EINT1_STATE_ADDR                   MT6337_ACCDET_CON28
#define MT6337_PMIC_ACCDET_EINT1_STATE_MASK                   0x7
#define MT6337_PMIC_ACCDET_EINT1_STATE_SHIFT                  0
#define MT6337_PMIC_ACCDET_EINT1_CUR_IN_ADDR                  MT6337_ACCDET_CON28
#define MT6337_PMIC_ACCDET_EINT1_CUR_IN_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT1_CUR_IN_SHIFT                 8
#define MT6337_PMIC_ACCDET_EINT1_SAM_IN_ADDR                  MT6337_ACCDET_CON28
#define MT6337_PMIC_ACCDET_EINT1_SAM_IN_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT1_SAM_IN_SHIFT                 9
#define MT6337_PMIC_ACCDET_EINT1_MEM_IN_ADDR                  MT6337_ACCDET_CON28
#define MT6337_PMIC_ACCDET_EINT1_MEM_IN_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT1_MEM_IN_SHIFT                 10
#define MT6337_PMIC_AD_EINT2CMPOUT_ADDR                       MT6337_ACCDET_CON28
#define MT6337_PMIC_AD_EINT2CMPOUT_MASK                       0x1
#define MT6337_PMIC_AD_EINT2CMPOUT_SHIFT                      14
#define MT6337_PMIC_DA_NI_EINT2CMPEN_ADDR                     MT6337_ACCDET_CON28
#define MT6337_PMIC_DA_NI_EINT2CMPEN_MASK                     0x1
#define MT6337_PMIC_DA_NI_EINT2CMPEN_SHIFT                    15
#define MT6337_PMIC_ACCDET_NEGV_COUNT_IN_ADDR                 MT6337_ACCDET_CON29
#define MT6337_PMIC_ACCDET_NEGV_COUNT_IN_MASK                 0x3F
#define MT6337_PMIC_ACCDET_NEGV_COUNT_IN_SHIFT                0
#define MT6337_PMIC_ACCDET_NEGV_EN_FINAL_ADDR                 MT6337_ACCDET_CON29
#define MT6337_PMIC_ACCDET_NEGV_EN_FINAL_MASK                 0x1
#define MT6337_PMIC_ACCDET_NEGV_EN_FINAL_SHIFT                6
#define MT6337_PMIC_ACCDET_NEGV_COUNT_END_ADDR                MT6337_ACCDET_CON29
#define MT6337_PMIC_ACCDET_NEGV_COUNT_END_MASK                0x1
#define MT6337_PMIC_ACCDET_NEGV_COUNT_END_SHIFT               12
#define MT6337_PMIC_ACCDET_NEGV_MINU_ADDR                     MT6337_ACCDET_CON29
#define MT6337_PMIC_ACCDET_NEGV_MINU_MASK                     0x1
#define MT6337_PMIC_ACCDET_NEGV_MINU_SHIFT                    13
#define MT6337_PMIC_ACCDET_NEGV_ADD_ADDR                      MT6337_ACCDET_CON29
#define MT6337_PMIC_ACCDET_NEGV_ADD_MASK                      0x1
#define MT6337_PMIC_ACCDET_NEGV_ADD_SHIFT                     14
#define MT6337_PMIC_ACCDET_NEGV_CMP_ADDR                      MT6337_ACCDET_CON29
#define MT6337_PMIC_ACCDET_NEGV_CMP_MASK                      0x1
#define MT6337_PMIC_ACCDET_NEGV_CMP_SHIFT                     15
#define MT6337_PMIC_ACCDET_CUR_DEB_ADDR                       MT6337_ACCDET_CON30
#define MT6337_PMIC_ACCDET_CUR_DEB_MASK                       0xFFFF
#define MT6337_PMIC_ACCDET_CUR_DEB_SHIFT                      0
#define MT6337_PMIC_ACCDET_EINT_CUR_DEB_ADDR                  MT6337_ACCDET_CON31
#define MT6337_PMIC_ACCDET_EINT_CUR_DEB_MASK                  0x7FFF
#define MT6337_PMIC_ACCDET_EINT_CUR_DEB_SHIFT                 0
#define MT6337_PMIC_ACCDET_EINT1_CUR_DEB_ADDR                 MT6337_ACCDET_CON32
#define MT6337_PMIC_ACCDET_EINT1_CUR_DEB_MASK                 0x7FFF
#define MT6337_PMIC_ACCDET_EINT1_CUR_DEB_SHIFT                0
#define MT6337_PMIC_ACCDET_RSV_CON0_ADDR                      MT6337_ACCDET_CON33
#define MT6337_PMIC_ACCDET_RSV_CON0_MASK                      0xFFFF
#define MT6337_PMIC_ACCDET_RSV_CON0_SHIFT                     0
#define MT6337_PMIC_ACCDET_RSV_CON1_ADDR                      MT6337_ACCDET_CON34
#define MT6337_PMIC_ACCDET_RSV_CON1_MASK                      0xFFFF
#define MT6337_PMIC_ACCDET_RSV_CON1_SHIFT                     0
#define MT6337_PMIC_ACCDET_AUXADC_CONNECT_TIME_ADDR           MT6337_ACCDET_CON35
#define MT6337_PMIC_ACCDET_AUXADC_CONNECT_TIME_MASK           0xFFFF
#define MT6337_PMIC_ACCDET_AUXADC_CONNECT_TIME_SHIFT          0
#define MT6337_PMIC_ACCDET_HWEN_SEL_ADDR                      MT6337_ACCDET_CON36
#define MT6337_PMIC_ACCDET_HWEN_SEL_MASK                      0x3
#define MT6337_PMIC_ACCDET_HWEN_SEL_SHIFT                     0
#define MT6337_PMIC_ACCDET_EINT_REVERSE_ADDR                  MT6337_ACCDET_CON36
#define MT6337_PMIC_ACCDET_EINT_REVERSE_MASK                  0x1
#define MT6337_PMIC_ACCDET_EINT_REVERSE_SHIFT                 2
#define MT6337_PMIC_ACCDET_EINT1_REVERSE_ADDR                 MT6337_ACCDET_CON36
#define MT6337_PMIC_ACCDET_EINT1_REVERSE_MASK                 0x1
#define MT6337_PMIC_ACCDET_EINT1_REVERSE_SHIFT                3
#define MT6337_PMIC_ACCDET_HWMODE_SEL_ADDR                    MT6337_ACCDET_CON36
#define MT6337_PMIC_ACCDET_HWMODE_SEL_MASK                    0x1
#define MT6337_PMIC_ACCDET_HWMODE_SEL_SHIFT                   4
#define MT6337_PMIC_ACCDET_EN_CMPC_ADDR                       MT6337_ACCDET_CON36
#define MT6337_PMIC_ACCDET_EN_CMPC_MASK                       0x1
#define MT6337_PMIC_ACCDET_EN_CMPC_SHIFT                      15
#define MT6337_PMIC_RG_AUDDACLPWRUP_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDDACLPWRUP_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDDACLPWRUP_VAUDP32_SHIFT             0
#define MT6337_PMIC_RG_AUDDACRPWRUP_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDDACRPWRUP_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDDACRPWRUP_VAUDP32_SHIFT             1
#define MT6337_PMIC_RG_AUD_DAC_PWR_UP_VA32_ADDR               MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUD_DAC_PWR_UP_VA32_MASK               0x1
#define MT6337_PMIC_RG_AUD_DAC_PWR_UP_VA32_SHIFT              2
#define MT6337_PMIC_RG_AUD_DAC_PWL_UP_VA32_ADDR               MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUD_DAC_PWL_UP_VA32_MASK               0x1
#define MT6337_PMIC_RG_AUD_DAC_PWL_UP_VA32_SHIFT              3
#define MT6337_PMIC_RG_AUDHPLPWRUP_VAUDP32_ADDR               MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPLPWRUP_VAUDP32_MASK               0x1
#define MT6337_PMIC_RG_AUDHPLPWRUP_VAUDP32_SHIFT              4
#define MT6337_PMIC_RG_AUDHPRPWRUP_VAUDP32_ADDR               MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPRPWRUP_VAUDP32_MASK               0x1
#define MT6337_PMIC_RG_AUDHPRPWRUP_VAUDP32_SHIFT              5
#define MT6337_PMIC_RG_AUDHPLPWRUP_IBIAS_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPLPWRUP_IBIAS_VAUDP32_MASK         0x1
#define MT6337_PMIC_RG_AUDHPLPWRUP_IBIAS_VAUDP32_SHIFT        6
#define MT6337_PMIC_RG_AUDHPRPWRUP_IBIAS_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPRPWRUP_IBIAS_VAUDP32_MASK         0x1
#define MT6337_PMIC_RG_AUDHPRPWRUP_IBIAS_VAUDP32_SHIFT        7
#define MT6337_PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP32_MASK         0x3
#define MT6337_PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP32_SHIFT        8
#define MT6337_PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP32_MASK         0x3
#define MT6337_PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP32_SHIFT        10
#define MT6337_PMIC_RG_AUDHPLSCDISABLE_VAUDP32_ADDR           MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPLSCDISABLE_VAUDP32_MASK           0x1
#define MT6337_PMIC_RG_AUDHPLSCDISABLE_VAUDP32_SHIFT          12
#define MT6337_PMIC_RG_AUDHPRSCDISABLE_VAUDP32_ADDR           MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPRSCDISABLE_VAUDP32_MASK           0x1
#define MT6337_PMIC_RG_AUDHPRSCDISABLE_VAUDP32_SHIFT          13
#define MT6337_PMIC_RG_AUDHPLBSCCURRENT_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPLBSCCURRENT_VAUDP32_MASK          0x1
#define MT6337_PMIC_RG_AUDHPLBSCCURRENT_VAUDP32_SHIFT         14
#define MT6337_PMIC_RG_AUDHPRBSCCURRENT_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON0
#define MT6337_PMIC_RG_AUDHPRBSCCURRENT_VAUDP32_MASK          0x1
#define MT6337_PMIC_RG_AUDHPRBSCCURRENT_VAUDP32_SHIFT         15
#define MT6337_PMIC_RG_AUDHPLOUTPWRUP_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_AUDHPLOUTPWRUP_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_AUDHPLOUTPWRUP_VAUDP32_SHIFT           0
#define MT6337_PMIC_RG_AUDHPROUTPWRUP_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_AUDHPROUTPWRUP_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_AUDHPROUTPWRUP_VAUDP32_SHIFT           1
#define MT6337_PMIC_RG_AUDHPLOUTAUXPWRUP_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_AUDHPLOUTAUXPWRUP_VAUDP32_MASK         0x1
#define MT6337_PMIC_RG_AUDHPLOUTAUXPWRUP_VAUDP32_SHIFT        2
#define MT6337_PMIC_RG_AUDHPROUTAUXPWRUP_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_AUDHPROUTAUXPWRUP_VAUDP32_MASK         0x1
#define MT6337_PMIC_RG_AUDHPROUTAUXPWRUP_VAUDP32_SHIFT        3
#define MT6337_PMIC_RG_HPLAUXFBRSW_EN_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_HPLAUXFBRSW_EN_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_HPLAUXFBRSW_EN_VAUDP32_SHIFT           4
#define MT6337_PMIC_RG_HPRAUXFBRSW_EN_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_HPRAUXFBRSW_EN_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_HPRAUXFBRSW_EN_VAUDP32_SHIFT           5
#define MT6337_PMIC_RG_HPLSHORT2HPLAUX_EN_VAUDP32_ADDR        MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_HPLSHORT2HPLAUX_EN_VAUDP32_MASK        0x1
#define MT6337_PMIC_RG_HPLSHORT2HPLAUX_EN_VAUDP32_SHIFT       6
#define MT6337_PMIC_RG_HPRSHORT2HPRAUX_EN_VAUDP32_ADDR        MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_HPRSHORT2HPRAUX_EN_VAUDP32_MASK        0x1
#define MT6337_PMIC_RG_HPRSHORT2HPRAUX_EN_VAUDP32_SHIFT       7
#define MT6337_PMIC_RG_HPPOUTSTGCTRL_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_HPPOUTSTGCTRL_VAUDP32_MASK             0x7
#define MT6337_PMIC_RG_HPPOUTSTGCTRL_VAUDP32_SHIFT            8
#define MT6337_PMIC_RG_HPNOUTSTGCTRL_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON1
#define MT6337_PMIC_RG_HPNOUTSTGCTRL_VAUDP32_MASK             0x7
#define MT6337_PMIC_RG_HPNOUTSTGCTRL_VAUDP32_SHIFT            11
#define MT6337_PMIC_RG_HPPOUTPUTSTBENH_VAUDP32_ADDR           MT6337_AUDDEC_ANA_CON2
#define MT6337_PMIC_RG_HPPOUTPUTSTBENH_VAUDP32_MASK           0x7
#define MT6337_PMIC_RG_HPPOUTPUTSTBENH_VAUDP32_SHIFT          0
#define MT6337_PMIC_RG_HPNOUTPUTSTBENH_VAUDP32_ADDR           MT6337_AUDDEC_ANA_CON2
#define MT6337_PMIC_RG_HPNOUTPUTSTBENH_VAUDP32_MASK           0x7
#define MT6337_PMIC_RG_HPNOUTPUTSTBENH_VAUDP32_SHIFT          3
#define MT6337_PMIC_RG_AUDHPSTARTUP_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON2
#define MT6337_PMIC_RG_AUDHPSTARTUP_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDHPSTARTUP_VAUDP32_SHIFT             13
#define MT6337_PMIC_RG_AUDREFN_DERES_EN_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON2
#define MT6337_PMIC_RG_AUDREFN_DERES_EN_VAUDP32_MASK          0x1
#define MT6337_PMIC_RG_AUDREFN_DERES_EN_VAUDP32_SHIFT         14
#define MT6337_PMIC_RG_HPPSHORT2VCM_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON2
#define MT6337_PMIC_RG_HPPSHORT2VCM_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_HPPSHORT2VCM_VAUDP32_SHIFT             15
#define MT6337_PMIC_RG_AUDHPTRIM_EN_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_AUDHPTRIM_EN_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDHPTRIM_EN_VAUDP32_SHIFT             0
#define MT6337_PMIC_RG_AUDHPLTRIM_VAUDP32_ADDR                MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_AUDHPLTRIM_VAUDP32_MASK                0xF
#define MT6337_PMIC_RG_AUDHPLTRIM_VAUDP32_SHIFT               1
#define MT6337_PMIC_RG_AUDHPLFINETRIM_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_AUDHPLFINETRIM_VAUDP32_MASK            0x3
#define MT6337_PMIC_RG_AUDHPLFINETRIM_VAUDP32_SHIFT           5
#define MT6337_PMIC_RG_AUDHPRTRIM_VAUDP32_ADDR                MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_AUDHPRTRIM_VAUDP32_MASK                0xF
#define MT6337_PMIC_RG_AUDHPRTRIM_VAUDP32_SHIFT               7
#define MT6337_PMIC_RG_AUDHPRFINETRIM_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_AUDHPRFINETRIM_VAUDP32_MASK            0x3
#define MT6337_PMIC_RG_AUDHPRFINETRIM_VAUDP32_SHIFT           11
#define MT6337_PMIC_RG_HPINPUTSTBENH_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_HPINPUTSTBENH_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_HPINPUTSTBENH_VAUDP32_SHIFT            13
#define MT6337_PMIC_RG_HPINPUTRESET0_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_HPINPUTRESET0_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_HPINPUTRESET0_VAUDP32_SHIFT            14
#define MT6337_PMIC_RG_HPOUTPUTRESET0_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON3
#define MT6337_PMIC_RG_HPOUTPUTRESET0_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_HPOUTPUTRESET0_VAUDP32_SHIFT           15
#define MT6337_PMIC_RG_AUDHPCOMP_EN_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON4
#define MT6337_PMIC_RG_AUDHPCOMP_EN_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDHPCOMP_EN_VAUDP32_SHIFT             0
#define MT6337_PMIC_RG_AUDHPDIFFINPBIASADJ_VAUDP32_ADDR       MT6337_AUDDEC_ANA_CON4
#define MT6337_PMIC_RG_AUDHPDIFFINPBIASADJ_VAUDP32_MASK       0x7
#define MT6337_PMIC_RG_AUDHPDIFFINPBIASADJ_VAUDP32_SHIFT      1
#define MT6337_PMIC_RG_AUDHPLFCOMPRESSEL_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON4
#define MT6337_PMIC_RG_AUDHPLFCOMPRESSEL_VAUDP32_MASK         0x7
#define MT6337_PMIC_RG_AUDHPLFCOMPRESSEL_VAUDP32_SHIFT        4
#define MT6337_PMIC_RG_AUDHPHFCOMPRESSEL_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON4
#define MT6337_PMIC_RG_AUDHPHFCOMPRESSEL_VAUDP32_MASK         0x7
#define MT6337_PMIC_RG_AUDHPHFCOMPRESSEL_VAUDP32_SHIFT        7
#define MT6337_PMIC_RG_AUDHPHFCOMPBUFGAINSEL_VAUDP32_ADDR     MT6337_AUDDEC_ANA_CON4
#define MT6337_PMIC_RG_AUDHPHFCOMPBUFGAINSEL_VAUDP32_MASK     0x3
#define MT6337_PMIC_RG_AUDHPHFCOMPBUFGAINSEL_VAUDP32_SHIFT    10
#define MT6337_PMIC_RG_AUDHPDECMGAINADJ_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON5
#define MT6337_PMIC_RG_AUDHPDECMGAINADJ_VAUDP32_MASK          0x7
#define MT6337_PMIC_RG_AUDHPDECMGAINADJ_VAUDP32_SHIFT         0
#define MT6337_PMIC_RG_AUDHPDEDMGAINADJ_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON5
#define MT6337_PMIC_RG_AUDHPDEDMGAINADJ_VAUDP32_MASK          0x7
#define MT6337_PMIC_RG_AUDHPDEDMGAINADJ_VAUDP32_SHIFT         3
#define MT6337_PMIC_RG_AUDHSPWRUP_VAUDP32_ADDR                MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_AUDHSPWRUP_VAUDP32_MASK                0x1
#define MT6337_PMIC_RG_AUDHSPWRUP_VAUDP32_SHIFT               0
#define MT6337_PMIC_RG_AUDHSPWRUP_IBIAS_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_AUDHSPWRUP_IBIAS_VAUDP32_MASK          0x1
#define MT6337_PMIC_RG_AUDHSPWRUP_IBIAS_VAUDP32_SHIFT         1
#define MT6337_PMIC_RG_AUDHSMUXINPUTSEL_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_AUDHSMUXINPUTSEL_VAUDP32_MASK          0x3
#define MT6337_PMIC_RG_AUDHSMUXINPUTSEL_VAUDP32_SHIFT         2
#define MT6337_PMIC_RG_AUDHSSCDISABLE_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_AUDHSSCDISABLE_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_AUDHSSCDISABLE_VAUDP32_SHIFT           4
#define MT6337_PMIC_RG_AUDHSBSCCURRENT_VAUDP32_ADDR           MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_AUDHSBSCCURRENT_VAUDP32_MASK           0x1
#define MT6337_PMIC_RG_AUDHSBSCCURRENT_VAUDP32_SHIFT          5
#define MT6337_PMIC_RG_AUDHSSTARTUP_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_AUDHSSTARTUP_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDHSSTARTUP_VAUDP32_SHIFT             6
#define MT6337_PMIC_RG_HSOUTPUTSTBENH_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_HSOUTPUTSTBENH_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_HSOUTPUTSTBENH_VAUDP32_SHIFT           7
#define MT6337_PMIC_RG_HSINPUTSTBENH_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_HSINPUTSTBENH_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_HSINPUTSTBENH_VAUDP32_SHIFT            8
#define MT6337_PMIC_RG_HSINPUTRESET0_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_HSINPUTRESET0_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_HSINPUTRESET0_VAUDP32_SHIFT            9
#define MT6337_PMIC_RG_HSOUTPUTRESET0_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_HSOUTPUTRESET0_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_HSOUTPUTRESET0_VAUDP32_SHIFT           10
#define MT6337_PMIC_RG_HSOUT_SHORTVCM_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON6
#define MT6337_PMIC_RG_HSOUT_SHORTVCM_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_HSOUT_SHORTVCM_VAUDP32_SHIFT           11
#define MT6337_PMIC_RG_AUDLOLPWRUP_VAUDP32_ADDR               MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_AUDLOLPWRUP_VAUDP32_MASK               0x1
#define MT6337_PMIC_RG_AUDLOLPWRUP_VAUDP32_SHIFT              0
#define MT6337_PMIC_RG_AUDLOLPWRUP_IBIAS_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_AUDLOLPWRUP_IBIAS_VAUDP32_MASK         0x1
#define MT6337_PMIC_RG_AUDLOLPWRUP_IBIAS_VAUDP32_SHIFT        1
#define MT6337_PMIC_RG_AUDLOLMUXINPUTSEL_VAUDP32_ADDR         MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_AUDLOLMUXINPUTSEL_VAUDP32_MASK         0x3
#define MT6337_PMIC_RG_AUDLOLMUXINPUTSEL_VAUDP32_SHIFT        2
#define MT6337_PMIC_RG_AUDLOLSCDISABLE_VAUDP32_ADDR           MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_AUDLOLSCDISABLE_VAUDP32_MASK           0x1
#define MT6337_PMIC_RG_AUDLOLSCDISABLE_VAUDP32_SHIFT          4
#define MT6337_PMIC_RG_AUDLOLBSCCURRENT_VAUDP32_ADDR          MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_AUDLOLBSCCURRENT_VAUDP32_MASK          0x1
#define MT6337_PMIC_RG_AUDLOLBSCCURRENT_VAUDP32_SHIFT         5
#define MT6337_PMIC_RG_AUDLOSTARTUP_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_AUDLOSTARTUP_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDLOSTARTUP_VAUDP32_SHIFT             6
#define MT6337_PMIC_RG_LOINPUTSTBENH_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_LOINPUTSTBENH_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_LOINPUTSTBENH_VAUDP32_SHIFT            7
#define MT6337_PMIC_RG_LOOUTPUTSTBENH_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_LOOUTPUTSTBENH_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_LOOUTPUTSTBENH_VAUDP32_SHIFT           8
#define MT6337_PMIC_RG_LOINPUTRESET0_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_LOINPUTRESET0_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_LOINPUTRESET0_VAUDP32_SHIFT            9
#define MT6337_PMIC_RG_LOOUTPUTRESET0_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_LOOUTPUTRESET0_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_LOOUTPUTRESET0_VAUDP32_SHIFT           10
#define MT6337_PMIC_RG_LOOUT_SHORTVCM_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON7
#define MT6337_PMIC_RG_LOOUT_SHORTVCM_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_LOOUT_SHORTVCM_VAUDP32_SHIFT           11
#define MT6337_PMIC_RG_AUDTRIMBUF_EN_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON8
#define MT6337_PMIC_RG_AUDTRIMBUF_EN_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_AUDTRIMBUF_EN_VAUDP32_SHIFT            0
#define MT6337_PMIC_RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP32_ADDR    MT6337_AUDDEC_ANA_CON8
#define MT6337_PMIC_RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP32_MASK    0xF
#define MT6337_PMIC_RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP32_SHIFT   1
#define MT6337_PMIC_RG_AUDTRIMBUF_GAINSEL_VAUDP32_ADDR        MT6337_AUDDEC_ANA_CON8
#define MT6337_PMIC_RG_AUDTRIMBUF_GAINSEL_VAUDP32_MASK        0x3
#define MT6337_PMIC_RG_AUDTRIMBUF_GAINSEL_VAUDP32_SHIFT       5
#define MT6337_PMIC_RG_AUDHPSPKDET_EN_VAUDP32_ADDR            MT6337_AUDDEC_ANA_CON8
#define MT6337_PMIC_RG_AUDHPSPKDET_EN_VAUDP32_MASK            0x1
#define MT6337_PMIC_RG_AUDHPSPKDET_EN_VAUDP32_SHIFT           8
#define MT6337_PMIC_RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP32_ADDR   MT6337_AUDDEC_ANA_CON8
#define MT6337_PMIC_RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP32_MASK   0x3
#define MT6337_PMIC_RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP32_SHIFT  9
#define MT6337_PMIC_RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP32_ADDR  MT6337_AUDDEC_ANA_CON8
#define MT6337_PMIC_RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP32_MASK  0x3
#define MT6337_PMIC_RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP32_SHIFT 11
#define MT6337_PMIC_RG_ABIDEC_RSVD0_VA32_ADDR                 MT6337_AUDDEC_ANA_CON9
#define MT6337_PMIC_RG_ABIDEC_RSVD0_VA32_MASK                 0xFF
#define MT6337_PMIC_RG_ABIDEC_RSVD0_VA32_SHIFT                0
#define MT6337_PMIC_RG_ABIDEC_RSVD0_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON9
#define MT6337_PMIC_RG_ABIDEC_RSVD0_VAUDP32_MASK              0xFF
#define MT6337_PMIC_RG_ABIDEC_RSVD0_VAUDP32_SHIFT             8
#define MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON10
#define MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP32_MASK              0x7
#define MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP32_SHIFT             0
#define MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON10
#define MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP32_MASK              0x1
#define MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP32_SHIFT             3
#define MT6337_PMIC_RG_AUDBIASADJ_0_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON10
#define MT6337_PMIC_RG_AUDBIASADJ_0_VAUDP32_MASK              0x1FF
#define MT6337_PMIC_RG_AUDBIASADJ_0_VAUDP32_SHIFT             7
#define MT6337_PMIC_RG_AUDBIASADJ_1_VAUDP32_ADDR              MT6337_AUDDEC_ANA_CON11
#define MT6337_PMIC_RG_AUDBIASADJ_1_VAUDP32_MASK              0xFF
#define MT6337_PMIC_RG_AUDBIASADJ_1_VAUDP32_SHIFT             7
#define MT6337_PMIC_RG_AUDIBIASPWRDN_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON11
#define MT6337_PMIC_RG_AUDIBIASPWRDN_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_AUDIBIASPWRDN_VAUDP32_SHIFT            15
#define MT6337_PMIC_RG_RSTB_DECODER_VA32_ADDR                 MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_RSTB_DECODER_VA32_MASK                 0x1
#define MT6337_PMIC_RG_RSTB_DECODER_VA32_SHIFT                0
#define MT6337_PMIC_RG_SEL_DECODER_96K_VA32_ADDR              MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_SEL_DECODER_96K_VA32_MASK              0x1
#define MT6337_PMIC_RG_SEL_DECODER_96K_VA32_SHIFT             1
#define MT6337_PMIC_RG_SEL_DELAY_VCORE_ADDR                   MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_SEL_DELAY_VCORE_MASK                   0x1
#define MT6337_PMIC_RG_SEL_DELAY_VCORE_SHIFT                  2
#define MT6337_PMIC_RG_AUDGLB_PWRDN_VA32_ADDR                 MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_AUDGLB_PWRDN_VA32_MASK                 0x1
#define MT6337_PMIC_RG_AUDGLB_PWRDN_VA32_SHIFT                4
#define MT6337_PMIC_RG_AUDGLB_LP_VOW_EN_VA32_ADDR             MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_AUDGLB_LP_VOW_EN_VA32_MASK             0x1
#define MT6337_PMIC_RG_AUDGLB_LP_VOW_EN_VA32_SHIFT            5
#define MT6337_PMIC_RG_AUDGLB_LP2_VOW_EN_VA32_ADDR            MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_AUDGLB_LP2_VOW_EN_VA32_MASK            0x1
#define MT6337_PMIC_RG_AUDGLB_LP2_VOW_EN_VA32_SHIFT           6
#define MT6337_PMIC_RG_AUDGLB_NVREG_L_VA32_ADDR               MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_AUDGLB_NVREG_L_VA32_MASK               0x3
#define MT6337_PMIC_RG_AUDGLB_NVREG_L_VA32_SHIFT              7
#define MT6337_PMIC_RG_AUDGLB_NVREG_R_VA32_ADDR               MT6337_AUDDEC_ANA_CON12
#define MT6337_PMIC_RG_AUDGLB_NVREG_R_VA32_MASK               0x3
#define MT6337_PMIC_RG_AUDGLB_NVREG_R_VA32_SHIFT              9
#define MT6337_PMIC_RG_LCLDO_DECL_EN_VA32_ADDR                MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_LCLDO_DECL_EN_VA32_MASK                0x1
#define MT6337_PMIC_RG_LCLDO_DECL_EN_VA32_SHIFT               0
#define MT6337_PMIC_RG_LCLDO_DECL_PDDIS_EN_VA18_ADDR          MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_LCLDO_DECL_PDDIS_EN_VA18_MASK          0x1
#define MT6337_PMIC_RG_LCLDO_DECL_PDDIS_EN_VA18_SHIFT         1
#define MT6337_PMIC_RG_LCLDO_DECL_REMOTE_SENSE_VA18_ADDR      MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_LCLDO_DECL_REMOTE_SENSE_VA18_MASK      0x1
#define MT6337_PMIC_RG_LCLDO_DECL_REMOTE_SENSE_VA18_SHIFT     2
#define MT6337_PMIC_RG_LCLDO_DECR_EN_VA32_ADDR                MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_LCLDO_DECR_EN_VA32_MASK                0x1
#define MT6337_PMIC_RG_LCLDO_DECR_EN_VA32_SHIFT               3
#define MT6337_PMIC_RG_LCLDO_DECR_PDDIS_EN_VA18_ADDR          MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_LCLDO_DECR_PDDIS_EN_VA18_MASK          0x1
#define MT6337_PMIC_RG_LCLDO_DECR_PDDIS_EN_VA18_SHIFT         4
#define MT6337_PMIC_RG_LCLDO_DECR_REMOTE_SENSE_VA18_ADDR      MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_LCLDO_DECR_REMOTE_SENSE_VA18_MASK      0x1
#define MT6337_PMIC_RG_LCLDO_DECR_REMOTE_SENSE_VA18_SHIFT     5
#define MT6337_PMIC_RG_AUDPMU_RSVD_VA18_ADDR                  MT6337_AUDDEC_ANA_CON13
#define MT6337_PMIC_RG_AUDPMU_RSVD_VA18_MASK                  0xFF
#define MT6337_PMIC_RG_AUDPMU_RSVD_VA18_SHIFT                 8
#define MT6337_PMIC_RG_NVREGL_EN_VAUDP32_ADDR                 MT6337_AUDDEC_ANA_CON14
#define MT6337_PMIC_RG_NVREGL_EN_VAUDP32_MASK                 0x1
#define MT6337_PMIC_RG_NVREGL_EN_VAUDP32_SHIFT                0
#define MT6337_PMIC_RG_NVREGR_EN_VAUDP32_ADDR                 MT6337_AUDDEC_ANA_CON14
#define MT6337_PMIC_RG_NVREGR_EN_VAUDP32_MASK                 0x1
#define MT6337_PMIC_RG_NVREGR_EN_VAUDP32_SHIFT                1
#define MT6337_PMIC_RG_NVREGL_PULL0V_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON14
#define MT6337_PMIC_RG_NVREGL_PULL0V_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_NVREGL_PULL0V_VAUDP32_SHIFT            2
#define MT6337_PMIC_RG_NVREGR_PULL0V_VAUDP32_ADDR             MT6337_AUDDEC_ANA_CON14
#define MT6337_PMIC_RG_NVREGR_PULL0V_VAUDP32_MASK             0x1
#define MT6337_PMIC_RG_NVREGR_PULL0V_VAUDP32_SHIFT            3
#define MT6337_PMIC_RG_AUDPREAMPCH0_01ON_ADDR                 MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01ON_MASK                 0x1
#define MT6337_PMIC_RG_AUDPREAMPCH0_01ON_SHIFT                0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01DCCEN_ADDR              MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01DCCEN_MASK              0x1
#define MT6337_PMIC_RG_AUDPREAMPCH0_01DCCEN_SHIFT             1
#define MT6337_PMIC_RG_AUDPREAMPCH0_01DCPRECHARGE_ADDR        MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01DCPRECHARGE_MASK        0x1
#define MT6337_PMIC_RG_AUDPREAMPCH0_01DCPRECHARGE_SHIFT       2
#define MT6337_PMIC_RG_AUDPREAMPCH0_01HPMODEEN_ADDR           MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01HPMODEEN_MASK           0x1
#define MT6337_PMIC_RG_AUDPREAMPCH0_01HPMODEEN_SHIFT          3
#define MT6337_PMIC_RG_AUDPREAMPCH0_01INPUTSEL_ADDR           MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01INPUTSEL_MASK           0x3
#define MT6337_PMIC_RG_AUDPREAMPCH0_01INPUTSEL_SHIFT          4
#define MT6337_PMIC_RG_AUDPREAMPCH0_01VSCALE_ADDR             MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01VSCALE_MASK             0x3
#define MT6337_PMIC_RG_AUDPREAMPCH0_01VSCALE_SHIFT            6
#define MT6337_PMIC_RG_AUDPREAMPCH0_01GAIN_ADDR               MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01GAIN_MASK               0x7
#define MT6337_PMIC_RG_AUDPREAMPCH0_01GAIN_SHIFT              8
#define MT6337_PMIC_RG_AUDPREAMPCH0_01PGATEST_ADDR            MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDPREAMPCH0_01PGATEST_MASK            0x1
#define MT6337_PMIC_RG_AUDPREAMPCH0_01PGATEST_SHIFT           11
#define MT6337_PMIC_RG_AUDADCCH0_01PWRUP_ADDR                 MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDADCCH0_01PWRUP_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCCH0_01PWRUP_SHIFT                12
#define MT6337_PMIC_RG_AUDADCCH0_01INPUTSEL_ADDR              MT6337_AUDENC_ANA_CON0
#define MT6337_PMIC_RG_AUDADCCH0_01INPUTSEL_MASK              0x3
#define MT6337_PMIC_RG_AUDADCCH0_01INPUTSEL_SHIFT             13
#define MT6337_PMIC_RG_AUDPREAMPCH1_23ON_ADDR                 MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23ON_MASK                 0x1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23ON_SHIFT                0
#define MT6337_PMIC_RG_AUDPREAMPCH1_23DCCEN_ADDR              MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23DCCEN_MASK              0x1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23DCCEN_SHIFT             1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23DCPRECHARGE_ADDR        MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23DCPRECHARGE_MASK        0x1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23DCPRECHARGE_SHIFT       2
#define MT6337_PMIC_RG_AUDPREAMPCH1_23HPMODEEN_ADDR           MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23HPMODEEN_MASK           0x1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23HPMODEEN_SHIFT          3
#define MT6337_PMIC_RG_AUDPREAMPCH1_23INPUTSEL_ADDR           MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23INPUTSEL_MASK           0x3
#define MT6337_PMIC_RG_AUDPREAMPCH1_23INPUTSEL_SHIFT          4
#define MT6337_PMIC_RG_AUDPREAMPCH1_23VSCALE_ADDR             MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23VSCALE_MASK             0x3
#define MT6337_PMIC_RG_AUDPREAMPCH1_23VSCALE_SHIFT            6
#define MT6337_PMIC_RG_AUDPREAMPCH1_23GAIN_ADDR               MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23GAIN_MASK               0x7
#define MT6337_PMIC_RG_AUDPREAMPCH1_23GAIN_SHIFT              8
#define MT6337_PMIC_RG_AUDPREAMPCH1_23PGATEST_ADDR            MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23PGATEST_MASK            0x1
#define MT6337_PMIC_RG_AUDPREAMPCH1_23PGATEST_SHIFT           11
#define MT6337_PMIC_RG_AUDADCCH1_23PWRUP_ADDR                 MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDADCCH1_23PWRUP_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCCH1_23PWRUP_SHIFT                12
#define MT6337_PMIC_RG_AUDADCCH1_23INPUTSEL_ADDR              MT6337_AUDENC_ANA_CON1
#define MT6337_PMIC_RG_AUDADCCH1_23INPUTSEL_MASK              0x3
#define MT6337_PMIC_RG_AUDADCCH1_23INPUTSEL_SHIFT             13
#define MT6337_PMIC_RG_AUDPREAMPCH2_45ON_ADDR                 MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45ON_MASK                 0x1
#define MT6337_PMIC_RG_AUDPREAMPCH2_45ON_SHIFT                0
#define MT6337_PMIC_RG_AUDPREAMPCH2_45DCCEN_ADDR              MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45DCCEN_MASK              0x1
#define MT6337_PMIC_RG_AUDPREAMPCH2_45DCCEN_SHIFT             1
#define MT6337_PMIC_RG_AUDPREAMPCH2_45DCPRECHARGE_ADDR        MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45DCPRECHARGE_MASK        0x1
#define MT6337_PMIC_RG_AUDPREAMPCH2_45DCPRECHARGE_SHIFT       2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45HPMODEEN_ADDR           MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45HPMODEEN_MASK           0x1
#define MT6337_PMIC_RG_AUDPREAMPCH2_45HPMODEEN_SHIFT          3
#define MT6337_PMIC_RG_AUDPREAMPCH2_45INPUTSEL_ADDR           MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45INPUTSEL_MASK           0x3
#define MT6337_PMIC_RG_AUDPREAMPCH2_45INPUTSEL_SHIFT          4
#define MT6337_PMIC_RG_AUDPREAMPCH2_45VSCALE_ADDR             MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45VSCALE_MASK             0x3
#define MT6337_PMIC_RG_AUDPREAMPCH2_45VSCALE_SHIFT            6
#define MT6337_PMIC_RG_AUDPREAMPCH2_45GAIN_ADDR               MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45GAIN_MASK               0x7
#define MT6337_PMIC_RG_AUDPREAMPCH2_45GAIN_SHIFT              8
#define MT6337_PMIC_RG_AUDPREAMPCH2_45PGATEST_ADDR            MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDPREAMPCH2_45PGATEST_MASK            0x1
#define MT6337_PMIC_RG_AUDPREAMPCH2_45PGATEST_SHIFT           11
#define MT6337_PMIC_RG_AUDADCCH2_45PWRUP_ADDR                 MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDADCCH2_45PWRUP_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCCH2_45PWRUP_SHIFT                12
#define MT6337_PMIC_RG_AUDADCCH2_45INPUTSEL_ADDR              MT6337_AUDENC_ANA_CON2
#define MT6337_PMIC_RG_AUDADCCH2_45INPUTSEL_MASK              0x3
#define MT6337_PMIC_RG_AUDADCCH2_45INPUTSEL_SHIFT             13
#define MT6337_PMIC_RG_AUDPREAMPCH3_6ON_ADDR                  MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6ON_MASK                  0x1
#define MT6337_PMIC_RG_AUDPREAMPCH3_6ON_SHIFT                 0
#define MT6337_PMIC_RG_AUDPREAMPCH3_6DCCEN_ADDR               MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6DCCEN_MASK               0x1
#define MT6337_PMIC_RG_AUDPREAMPCH3_6DCCEN_SHIFT              1
#define MT6337_PMIC_RG_AUDPREAMPCH3_6DCPRECHARGE_ADDR         MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6DCPRECHARGE_MASK         0x1
#define MT6337_PMIC_RG_AUDPREAMPCH3_6DCPRECHARGE_SHIFT        2
#define MT6337_PMIC_RG_AUDPREAMPCH3_6HPMODEEN_ADDR            MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6HPMODEEN_MASK            0x1
#define MT6337_PMIC_RG_AUDPREAMPCH3_6HPMODEEN_SHIFT           3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6INPUTSEL_ADDR            MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6INPUTSEL_MASK            0x3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6INPUTSEL_SHIFT           4
#define MT6337_PMIC_RG_AUDPREAMPCH3_6VSCALE_ADDR              MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6VSCALE_MASK              0x3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6VSCALE_SHIFT             6
#define MT6337_PMIC_RG_AUDPREAMPCH3_6GAIN_ADDR                MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6GAIN_MASK                0x7
#define MT6337_PMIC_RG_AUDPREAMPCH3_6GAIN_SHIFT               8
#define MT6337_PMIC_RG_AUDPREAMPCH3_6PGATEST_ADDR             MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDPREAMPCH3_6PGATEST_MASK             0x1
#define MT6337_PMIC_RG_AUDPREAMPCH3_6PGATEST_SHIFT            11
#define MT6337_PMIC_RG_AUDADCCH3_6PWRUP_ADDR                  MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDADCCH3_6PWRUP_MASK                  0x1
#define MT6337_PMIC_RG_AUDADCCH3_6PWRUP_SHIFT                 12
#define MT6337_PMIC_RG_AUDADCCH3_6INPUTSEL_ADDR               MT6337_AUDENC_ANA_CON3
#define MT6337_PMIC_RG_AUDADCCH3_6INPUTSEL_MASK               0x3
#define MT6337_PMIC_RG_AUDADCCH3_6INPUTSEL_SHIFT              13
#define MT6337_PMIC_RG_AUDULHALFBIAS_CH01_ADDR                MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDULHALFBIAS_CH01_MASK                0x1
#define MT6337_PMIC_RG_AUDULHALFBIAS_CH01_SHIFT               0
#define MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH01_ADDR               MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH01_MASK               0x1
#define MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH01_SHIFT              1
#define MT6337_PMIC_RG_AUDPREAMPLPEN_CH01_ADDR                MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDPREAMPLPEN_CH01_MASK                0x1
#define MT6337_PMIC_RG_AUDPREAMPLPEN_CH01_SHIFT               2
#define MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH01_ADDR           MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH01_MASK           0x1
#define MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH01_SHIFT          3
#define MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH01_ADDR           MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH01_MASK           0x1
#define MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH01_SHIFT          4
#define MT6337_PMIC_RG_AUDADCFLASHLPEN_CH01_ADDR              MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADCFLASHLPEN_CH01_MASK              0x1
#define MT6337_PMIC_RG_AUDADCFLASHLPEN_CH01_SHIFT             5
#define MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH01_ADDR             MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH01_MASK             0x3
#define MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH01_SHIFT            6
#define MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH01_ADDR        MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH01_MASK        0x3
#define MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH01_SHIFT       8
#define MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH01_ADDR        MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH01_MASK        0x3
#define MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH01_SHIFT       10
#define MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH01_ADDR          MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH01_MASK          0x3
#define MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH01_SHIFT         12
#define MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH01_ADDR           MT6337_AUDENC_ANA_CON4
#define MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH01_MASK           0x3
#define MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH01_SHIFT          14
#define MT6337_PMIC_RG_AUDULHALFBIAS_CH23_ADDR                MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDULHALFBIAS_CH23_MASK                0x1
#define MT6337_PMIC_RG_AUDULHALFBIAS_CH23_SHIFT               0
#define MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH23_ADDR               MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH23_MASK               0x1
#define MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH23_SHIFT              1
#define MT6337_PMIC_RG_AUDPREAMPLPEN_CH23_ADDR                MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDPREAMPLPEN_CH23_MASK                0x1
#define MT6337_PMIC_RG_AUDPREAMPLPEN_CH23_SHIFT               2
#define MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH23_ADDR           MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH23_MASK           0x1
#define MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH23_SHIFT          3
#define MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH23_ADDR           MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH23_MASK           0x1
#define MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH23_SHIFT          4
#define MT6337_PMIC_RG_AUDADCFLASHLPEN_CH23_ADDR              MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADCFLASHLPEN_CH23_MASK              0x1
#define MT6337_PMIC_RG_AUDADCFLASHLPEN_CH23_SHIFT             5
#define MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH23_ADDR             MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH23_MASK             0x3
#define MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH23_SHIFT            6
#define MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH23_ADDR        MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH23_MASK        0x3
#define MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH23_SHIFT       8
#define MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH23_ADDR        MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH23_MASK        0x3
#define MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH23_SHIFT       10
#define MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH23_ADDR          MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH23_MASK          0x3
#define MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH23_SHIFT         12
#define MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH23_ADDR           MT6337_AUDENC_ANA_CON5
#define MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH23_MASK           0x3
#define MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH23_SHIFT          14
#define MT6337_PMIC_RG_AUDADCDAC0P25FS_CH01_ADDR              MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDADCDAC0P25FS_CH01_MASK              0x1
#define MT6337_PMIC_RG_AUDADCDAC0P25FS_CH01_SHIFT             0
#define MT6337_PMIC_RG_AUDADCCLKSEL_CH01_ADDR                 MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDADCCLKSEL_CH01_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCCLKSEL_CH01_SHIFT                1
#define MT6337_PMIC_RG_AUDADCCLKSOURCE_CH01_ADDR              MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDADCCLKSOURCE_CH01_MASK              0x3
#define MT6337_PMIC_RG_AUDADCCLKSOURCE_CH01_SHIFT             2
#define MT6337_PMIC_RG_AUDADCCLKGENMODE_CH01_ADDR             MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDADCCLKGENMODE_CH01_MASK             0x3
#define MT6337_PMIC_RG_AUDADCCLKGENMODE_CH01_SHIFT            4
#define MT6337_PMIC_RG_AUDADCCLKRSTB_CH01_ADDR                MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDADCCLKRSTB_CH01_MASK                0x1
#define MT6337_PMIC_RG_AUDADCCLKRSTB_CH01_SHIFT               6
#define MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH01_ADDR           MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH01_MASK           0x1
#define MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH01_SHIFT          8
#define MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH01_ADDR             MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH01_MASK             0x1
#define MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH01_SHIFT            9
#define MT6337_PMIC_RG_AUDPREAMPAAFEN_CH01_ADDR               MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDPREAMPAAFEN_CH01_MASK               0x1
#define MT6337_PMIC_RG_AUDPREAMPAAFEN_CH01_SHIFT              10
#define MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH01_ADDR            MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH01_MASK            0x1
#define MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH01_SHIFT           11
#define MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH01_ADDR              MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH01_MASK              0x1
#define MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH01_SHIFT             12
#define MT6337_PMIC_RG_AUDSPAREPGA_CH01_ADDR                  MT6337_AUDENC_ANA_CON6
#define MT6337_PMIC_RG_AUDSPAREPGA_CH01_MASK                  0x7
#define MT6337_PMIC_RG_AUDSPAREPGA_CH01_SHIFT                 13
#define MT6337_PMIC_RG_AUDADCDAC0P25FS_CH23_ADDR              MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDADCDAC0P25FS_CH23_MASK              0x1
#define MT6337_PMIC_RG_AUDADCDAC0P25FS_CH23_SHIFT             0
#define MT6337_PMIC_RG_AUDADCCLKSEL_CH23_ADDR                 MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDADCCLKSEL_CH23_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCCLKSEL_CH23_SHIFT                1
#define MT6337_PMIC_RG_AUDADCCLKSOURCE_CH23_ADDR              MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDADCCLKSOURCE_CH23_MASK              0x3
#define MT6337_PMIC_RG_AUDADCCLKSOURCE_CH23_SHIFT             2
#define MT6337_PMIC_RG_AUDADCCLKGENMODE_CH23_ADDR             MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDADCCLKGENMODE_CH23_MASK             0x3
#define MT6337_PMIC_RG_AUDADCCLKGENMODE_CH23_SHIFT            4
#define MT6337_PMIC_RG_AUDADCCLKRSTB_CH23_ADDR                MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDADCCLKRSTB_CH23_MASK                0x1
#define MT6337_PMIC_RG_AUDADCCLKRSTB_CH23_SHIFT               6
#define MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH23_ADDR           MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH23_MASK           0x1
#define MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH23_SHIFT          8
#define MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH23_ADDR             MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH23_MASK             0x1
#define MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH23_SHIFT            9
#define MT6337_PMIC_RG_AUDPREAMPAAFEN_CH23_ADDR               MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDPREAMPAAFEN_CH23_MASK               0x1
#define MT6337_PMIC_RG_AUDPREAMPAAFEN_CH23_SHIFT              10
#define MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH23_ADDR            MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH23_MASK            0x1
#define MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH23_SHIFT           11
#define MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH23_ADDR              MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH23_MASK              0x1
#define MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH23_SHIFT             12
#define MT6337_PMIC_RG_AUDSPAREPGA_CH23_ADDR                  MT6337_AUDENC_ANA_CON7
#define MT6337_PMIC_RG_AUDSPAREPGA_CH23_MASK                  0x7
#define MT6337_PMIC_RG_AUDSPAREPGA_CH23_SHIFT                 13
#define MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH01_ADDR          MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH01_MASK          0x1
#define MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH01_SHIFT         0
#define MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH01_ADDR          MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH01_MASK          0x1
#define MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH01_SHIFT         1
#define MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH01_ADDR          MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH01_MASK          0x1
#define MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH01_SHIFT         2
#define MT6337_PMIC_RG_AUDADCFSRESET_CH01_ADDR                MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCFSRESET_CH01_MASK                0x1
#define MT6337_PMIC_RG_AUDADCFSRESET_CH01_SHIFT               3
#define MT6337_PMIC_RG_AUDADCWIDECM_CH01_ADDR                 MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCWIDECM_CH01_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCWIDECM_CH01_SHIFT                4
#define MT6337_PMIC_RG_AUDADCNOPATEST_CH01_ADDR               MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCNOPATEST_CH01_MASK               0x1
#define MT6337_PMIC_RG_AUDADCNOPATEST_CH01_SHIFT              5
#define MT6337_PMIC_RG_AUDADCBYPASS_CH01_ADDR                 MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCBYPASS_CH01_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCBYPASS_CH01_SHIFT                6
#define MT6337_PMIC_RG_AUDADCFFBYPASS_CH01_ADDR               MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCFFBYPASS_CH01_MASK               0x1
#define MT6337_PMIC_RG_AUDADCFFBYPASS_CH01_SHIFT              7
#define MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH01_ADDR           MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH01_MASK           0x1
#define MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH01_SHIFT          8
#define MT6337_PMIC_RG_AUDADCDACIDDTEST_CH01_ADDR             MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCDACIDDTEST_CH01_MASK             0x3
#define MT6337_PMIC_RG_AUDADCDACIDDTEST_CH01_SHIFT            9
#define MT6337_PMIC_RG_AUDADCDACNRZ_CH01_ADDR                 MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCDACNRZ_CH01_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCDACNRZ_CH01_SHIFT                11
#define MT6337_PMIC_RG_AUDADCNODEM_CH01_ADDR                  MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCNODEM_CH01_MASK                  0x1
#define MT6337_PMIC_RG_AUDADCNODEM_CH01_SHIFT                 12
#define MT6337_PMIC_RG_AUDADCDACTEST_CH01_ADDR                MT6337_AUDENC_ANA_CON8
#define MT6337_PMIC_RG_AUDADCDACTEST_CH01_MASK                0x1
#define MT6337_PMIC_RG_AUDADCDACTEST_CH01_SHIFT               13
#define MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH23_ADDR          MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH23_MASK          0x1
#define MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH23_SHIFT         0
#define MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH23_ADDR          MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH23_MASK          0x1
#define MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH23_SHIFT         1
#define MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH23_ADDR          MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH23_MASK          0x1
#define MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH23_SHIFT         2
#define MT6337_PMIC_RG_AUDADCFSRESET_CH23_ADDR                MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCFSRESET_CH23_MASK                0x1
#define MT6337_PMIC_RG_AUDADCFSRESET_CH23_SHIFT               3
#define MT6337_PMIC_RG_AUDADCWIDECM_CH23_ADDR                 MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCWIDECM_CH23_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCWIDECM_CH23_SHIFT                4
#define MT6337_PMIC_RG_AUDADCNOPATEST_CH23_ADDR               MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCNOPATEST_CH23_MASK               0x1
#define MT6337_PMIC_RG_AUDADCNOPATEST_CH23_SHIFT              5
#define MT6337_PMIC_RG_AUDADCBYPASS_CH23_ADDR                 MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCBYPASS_CH23_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCBYPASS_CH23_SHIFT                6
#define MT6337_PMIC_RG_AUDADCFFBYPASS_CH23_ADDR               MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCFFBYPASS_CH23_MASK               0x1
#define MT6337_PMIC_RG_AUDADCFFBYPASS_CH23_SHIFT              7
#define MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH23_ADDR           MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH23_MASK           0x1
#define MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH23_SHIFT          8
#define MT6337_PMIC_RG_AUDADCDACIDDTEST_CH23_ADDR             MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCDACIDDTEST_CH23_MASK             0x3
#define MT6337_PMIC_RG_AUDADCDACIDDTEST_CH23_SHIFT            9
#define MT6337_PMIC_RG_AUDADCDACNRZ_CH23_ADDR                 MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCDACNRZ_CH23_MASK                 0x1
#define MT6337_PMIC_RG_AUDADCDACNRZ_CH23_SHIFT                11
#define MT6337_PMIC_RG_AUDADCNODEM_CH23_ADDR                  MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCNODEM_CH23_MASK                  0x1
#define MT6337_PMIC_RG_AUDADCNODEM_CH23_SHIFT                 12
#define MT6337_PMIC_RG_AUDADCDACTEST_CH23_ADDR                MT6337_AUDENC_ANA_CON9
#define MT6337_PMIC_RG_AUDADCDACTEST_CH23_MASK                0x1
#define MT6337_PMIC_RG_AUDADCDACTEST_CH23_SHIFT               13
#define MT6337_PMIC_RG_AUDRCTUNECH0_01_ADDR                   MT6337_AUDENC_ANA_CON10
#define MT6337_PMIC_RG_AUDRCTUNECH0_01_MASK                   0x1F
#define MT6337_PMIC_RG_AUDRCTUNECH0_01_SHIFT                  0
#define MT6337_PMIC_RG_AUDRCTUNECH0_01SEL_ADDR                MT6337_AUDENC_ANA_CON10
#define MT6337_PMIC_RG_AUDRCTUNECH0_01SEL_MASK                0x1
#define MT6337_PMIC_RG_AUDRCTUNECH0_01SEL_SHIFT               5
#define MT6337_PMIC_RG_AUDRCTUNECH1_23_ADDR                   MT6337_AUDENC_ANA_CON10
#define MT6337_PMIC_RG_AUDRCTUNECH1_23_MASK                   0x1F
#define MT6337_PMIC_RG_AUDRCTUNECH1_23_SHIFT                  8
#define MT6337_PMIC_RG_AUDRCTUNECH1_23SEL_ADDR                MT6337_AUDENC_ANA_CON10
#define MT6337_PMIC_RG_AUDRCTUNECH1_23SEL_MASK                0x1
#define MT6337_PMIC_RG_AUDRCTUNECH1_23SEL_SHIFT               13
#define MT6337_PMIC_RG_AUDRCTUNECH2_45_ADDR                   MT6337_AUDENC_ANA_CON11
#define MT6337_PMIC_RG_AUDRCTUNECH2_45_MASK                   0x1F
#define MT6337_PMIC_RG_AUDRCTUNECH2_45_SHIFT                  0
#define MT6337_PMIC_RG_AUDRCTUNECH2_45SEL_ADDR                MT6337_AUDENC_ANA_CON11
#define MT6337_PMIC_RG_AUDRCTUNECH2_45SEL_MASK                0x1
#define MT6337_PMIC_RG_AUDRCTUNECH2_45SEL_SHIFT               5
#define MT6337_PMIC_RG_AUDRCTUNECH3_6_ADDR                    MT6337_AUDENC_ANA_CON11
#define MT6337_PMIC_RG_AUDRCTUNECH3_6_MASK                    0x1F
#define MT6337_PMIC_RG_AUDRCTUNECH3_6_SHIFT                   8
#define MT6337_PMIC_RG_AUDRCTUNECH3_6SEL_ADDR                 MT6337_AUDENC_ANA_CON11
#define MT6337_PMIC_RG_AUDRCTUNECH3_6SEL_MASK                 0x1
#define MT6337_PMIC_RG_AUDRCTUNECH3_6SEL_SHIFT                13
#define MT6337_PMIC_RG_AUDSPAREVA30_ADDR                      MT6337_AUDENC_ANA_CON12
#define MT6337_PMIC_RG_AUDSPAREVA30_MASK                      0xFF
#define MT6337_PMIC_RG_AUDSPAREVA30_SHIFT                     0
#define MT6337_PMIC_RG_AUDSPAREVA18_ADDR                      MT6337_AUDENC_ANA_CON12
#define MT6337_PMIC_RG_AUDSPAREVA18_MASK                      0xFF
#define MT6337_PMIC_RG_AUDSPAREVA18_SHIFT                     8
#define MT6337_PMIC_RG_AUDDIGMIC0EN_ADDR                      MT6337_AUDENC_ANA_CON13
#define MT6337_PMIC_RG_AUDDIGMIC0EN_MASK                      0x1
#define MT6337_PMIC_RG_AUDDIGMIC0EN_SHIFT                     0
#define MT6337_PMIC_RG_AUDDIGMIC0BIAS_ADDR                    MT6337_AUDENC_ANA_CON13
#define MT6337_PMIC_RG_AUDDIGMIC0BIAS_MASK                    0x3
#define MT6337_PMIC_RG_AUDDIGMIC0BIAS_SHIFT                   1
#define MT6337_PMIC_RG_DMIC0HPCLKEN_ADDR                      MT6337_AUDENC_ANA_CON13
#define MT6337_PMIC_RG_DMIC0HPCLKEN_MASK                      0x1
#define MT6337_PMIC_RG_DMIC0HPCLKEN_SHIFT                     3
#define MT6337_PMIC_RG_AUDDIGMIC0PDUTY_ADDR                   MT6337_AUDENC_ANA_CON13
#define MT6337_PMIC_RG_AUDDIGMIC0PDUTY_MASK                   0x3
#define MT6337_PMIC_RG_AUDDIGMIC0PDUTY_SHIFT                  4
#define MT6337_PMIC_RG_AUDDIGMIC0NDUTY_ADDR                   MT6337_AUDENC_ANA_CON13
#define MT6337_PMIC_RG_AUDDIGMIC0NDUTY_MASK                   0x3
#define MT6337_PMIC_RG_AUDDIGMIC0NDUTY_SHIFT                  6
#define MT6337_PMIC_RG_AUDDIGMIC1EN_ADDR                      MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_AUDDIGMIC1EN_MASK                      0x1
#define MT6337_PMIC_RG_AUDDIGMIC1EN_SHIFT                     0
#define MT6337_PMIC_RG_AUDDIGMIC1BIAS_ADDR                    MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_AUDDIGMIC1BIAS_MASK                    0x3
#define MT6337_PMIC_RG_AUDDIGMIC1BIAS_SHIFT                   1
#define MT6337_PMIC_RG_DMIC1HPCLKEN_ADDR                      MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_DMIC1HPCLKEN_MASK                      0x1
#define MT6337_PMIC_RG_DMIC1HPCLKEN_SHIFT                     3
#define MT6337_PMIC_RG_AUDDIGMIC1PDUTY_ADDR                   MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_AUDDIGMIC1PDUTY_MASK                   0x3
#define MT6337_PMIC_RG_AUDDIGMIC1PDUTY_SHIFT                  4
#define MT6337_PMIC_RG_AUDDIGMIC1NDUTY_ADDR                   MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_AUDDIGMIC1NDUTY_MASK                   0x3
#define MT6337_PMIC_RG_AUDDIGMIC1NDUTY_SHIFT                  6
#define MT6337_PMIC_RG_DMIC1MONEN_ADDR                        MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_DMIC1MONEN_MASK                        0x1
#define MT6337_PMIC_RG_DMIC1MONEN_SHIFT                       8
#define MT6337_PMIC_RG_DMIC1MONSEL_ADDR                       MT6337_AUDENC_ANA_CON14
#define MT6337_PMIC_RG_DMIC1MONSEL_MASK                       0x7
#define MT6337_PMIC_RG_DMIC1MONSEL_SHIFT                      9
#define MT6337_PMIC_RG_AUDDIGMIC2EN_ADDR                      MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_AUDDIGMIC2EN_MASK                      0x1
#define MT6337_PMIC_RG_AUDDIGMIC2EN_SHIFT                     0
#define MT6337_PMIC_RG_AUDDIGMIC2BIAS_ADDR                    MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_AUDDIGMIC2BIAS_MASK                    0x3
#define MT6337_PMIC_RG_AUDDIGMIC2BIAS_SHIFT                   1
#define MT6337_PMIC_RG_DMIC2HPCLKEN_ADDR                      MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_DMIC2HPCLKEN_MASK                      0x1
#define MT6337_PMIC_RG_DMIC2HPCLKEN_SHIFT                     3
#define MT6337_PMIC_RG_AUDDIGMIC2PDUTY_ADDR                   MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_AUDDIGMIC2PDUTY_MASK                   0x3
#define MT6337_PMIC_RG_AUDDIGMIC2PDUTY_SHIFT                  4
#define MT6337_PMIC_RG_AUDDIGMIC2NDUTY_ADDR                   MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_AUDDIGMIC2NDUTY_MASK                   0x3
#define MT6337_PMIC_RG_AUDDIGMIC2NDUTY_SHIFT                  6
#define MT6337_PMIC_RG_DMIC2MONEN_ADDR                        MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_DMIC2MONEN_MASK                        0x1
#define MT6337_PMIC_RG_DMIC2MONEN_SHIFT                       8
#define MT6337_PMIC_RG_DMIC2MONSEL_ADDR                       MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_DMIC2MONSEL_MASK                       0x3
#define MT6337_PMIC_RG_DMIC2MONSEL_SHIFT                      9
#define MT6337_PMIC_RG_DMIC2RDATASEL_ADDR                     MT6337_AUDENC_ANA_CON15
#define MT6337_PMIC_RG_DMIC2RDATASEL_MASK                     0x1
#define MT6337_PMIC_RG_DMIC2RDATASEL_SHIFT                    12
#define MT6337_PMIC_RG_AUDPWDBMICBIAS0_ADDR                   MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDPWDBMICBIAS0_MASK                   0x1
#define MT6337_PMIC_RG_AUDPWDBMICBIAS0_SHIFT                  0
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0P1EN_ADDR              MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0P1EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0P1EN_SHIFT             1
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0P2EN_ADDR              MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0P2EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0P2EN_SHIFT             2
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0NEN_ADDR               MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS0DCSW0NEN_SHIFT              3
#define MT6337_PMIC_RG_AUDMICBIAS0VREF_ADDR                   MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS0VREF_MASK                   0x7
#define MT6337_PMIC_RG_AUDMICBIAS0VREF_SHIFT                  4
#define MT6337_PMIC_RG_AUDMICBIAS0LOWPEN_ADDR                 MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS0LOWPEN_MASK                 0x1
#define MT6337_PMIC_RG_AUDMICBIAS0LOWPEN_SHIFT                7
#define MT6337_PMIC_RG_AUDPWDBMICBIAS2_ADDR                   MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDPWDBMICBIAS2_MASK                   0x1
#define MT6337_PMIC_RG_AUDPWDBMICBIAS2_SHIFT                  8
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2P1EN_ADDR              MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2P1EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2P1EN_SHIFT             9
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2P2EN_ADDR              MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2P2EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2P2EN_SHIFT             10
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2NEN_ADDR               MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW2NEN_SHIFT              11
#define MT6337_PMIC_RG_AUDMICBIAS2VREF_ADDR                   MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS2VREF_MASK                   0x7
#define MT6337_PMIC_RG_AUDMICBIAS2VREF_SHIFT                  12
#define MT6337_PMIC_RG_AUDMICBIAS2LOWPEN_ADDR                 MT6337_AUDENC_ANA_CON16
#define MT6337_PMIC_RG_AUDMICBIAS2LOWPEN_MASK                 0x1
#define MT6337_PMIC_RG_AUDMICBIAS2LOWPEN_SHIFT                15
#define MT6337_PMIC_RG_AUDPWDBMICBIAS1_ADDR                   MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDPWDBMICBIAS1_MASK                   0x1
#define MT6337_PMIC_RG_AUDPWDBMICBIAS1_SHIFT                  0
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW1PEN_ADDR               MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW1PEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW1PEN_SHIFT              1
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW1NEN_ADDR               MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW1NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW1NEN_SHIFT              2
#define MT6337_PMIC_RG_AUDMICBIAS1VREF_ADDR                   MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1VREF_MASK                   0x7
#define MT6337_PMIC_RG_AUDMICBIAS1VREF_SHIFT                  4
#define MT6337_PMIC_RG_AUDMICBIAS1LOWPEN_ADDR                 MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1LOWPEN_MASK                 0x1
#define MT6337_PMIC_RG_AUDMICBIAS1LOWPEN_SHIFT                7
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW3PEN_ADDR               MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW3PEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW3PEN_SHIFT              8
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW3NEN_ADDR               MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW3NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS1DCSW3NEN_SHIFT              9
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW3PEN_ADDR               MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW3PEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW3PEN_SHIFT              10
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW3NEN_ADDR               MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW3NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS2DCSW3NEN_SHIFT              11
#define MT6337_PMIC_RG_AUDMICBIAS1HVEN_ADDR                   MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1HVEN_MASK                   0x1
#define MT6337_PMIC_RG_AUDMICBIAS1HVEN_SHIFT                  12
#define MT6337_PMIC_RG_AUDMICBIAS1HVVREF_ADDR                 MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_AUDMICBIAS1HVVREF_MASK                 0x1
#define MT6337_PMIC_RG_AUDMICBIAS1HVVREF_SHIFT                13
#define MT6337_PMIC_RG_BANDGAPGEN_ADDR                        MT6337_AUDENC_ANA_CON17
#define MT6337_PMIC_RG_BANDGAPGEN_MASK                        0x1
#define MT6337_PMIC_RG_BANDGAPGEN_SHIFT                       14
#define MT6337_PMIC_RG_AUDPWDBMICBIAS3_ADDR                   MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDPWDBMICBIAS3_MASK                   0x1
#define MT6337_PMIC_RG_AUDPWDBMICBIAS3_SHIFT                  0
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4P1EN_ADDR              MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4P1EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4P1EN_SHIFT             1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4P2EN_ADDR              MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4P2EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4P2EN_SHIFT             2
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4NEN_ADDR               MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW4NEN_SHIFT              3
#define MT6337_PMIC_RG_AUDMICBIAS3VREF_ADDR                   MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3VREF_MASK                   0x7
#define MT6337_PMIC_RG_AUDMICBIAS3VREF_SHIFT                  4
#define MT6337_PMIC_RG_AUDMICBIAS3LOWPEN_ADDR                 MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3LOWPEN_MASK                 0x1
#define MT6337_PMIC_RG_AUDMICBIAS3LOWPEN_SHIFT                7
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5P1EN_ADDR              MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5P1EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5P1EN_SHIFT             8
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5P2EN_ADDR              MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5P2EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5P2EN_SHIFT             9
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5NEN_ADDR               MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW5NEN_SHIFT              10
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6P1EN_ADDR              MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6P1EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6P1EN_SHIFT             12
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6P2EN_ADDR              MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6P2EN_MASK              0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6P2EN_SHIFT             13
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6NEN_ADDR               MT6337_AUDENC_ANA_CON18
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6NEN_MASK               0x1
#define MT6337_PMIC_RG_AUDMICBIAS3DCSW6NEN_SHIFT              14
#define MT6337_PMIC_RG_AUDACCDETMICBIAS0PULLLOW_ADDR          MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETMICBIAS0PULLLOW_MASK          0x1
#define MT6337_PMIC_RG_AUDACCDETMICBIAS0PULLLOW_SHIFT         0
#define MT6337_PMIC_RG_AUDACCDETMICBIAS1PULLLOW_ADDR          MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETMICBIAS1PULLLOW_MASK          0x1
#define MT6337_PMIC_RG_AUDACCDETMICBIAS1PULLLOW_SHIFT         1
#define MT6337_PMIC_RG_AUDACCDETMICBIAS2PULLLOW_ADDR          MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETMICBIAS2PULLLOW_MASK          0x1
#define MT6337_PMIC_RG_AUDACCDETMICBIAS2PULLLOW_SHIFT         2
#define MT6337_PMIC_RG_AUDACCDETMICBIAS3PULLLOW_ADDR          MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETMICBIAS3PULLLOW_MASK          0x1
#define MT6337_PMIC_RG_AUDACCDETMICBIAS3PULLLOW_SHIFT         3
#define MT6337_PMIC_RG_AUDACCDETVIN1PULLLOW_ADDR              MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETVIN1PULLLOW_MASK              0x1
#define MT6337_PMIC_RG_AUDACCDETVIN1PULLLOW_SHIFT             4
#define MT6337_PMIC_RG_AUDACCDETVTHACAL_ADDR                  MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETVTHACAL_MASK                  0x1
#define MT6337_PMIC_RG_AUDACCDETVTHACAL_SHIFT                 5
#define MT6337_PMIC_RG_AUDACCDETVTHBCAL_ADDR                  MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_AUDACCDETVTHBCAL_MASK                  0x1
#define MT6337_PMIC_RG_AUDACCDETVTHBCAL_SHIFT                 6
#define MT6337_PMIC_RG_ACCDET1SEL_ADDR                        MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_ACCDET1SEL_MASK                        0x1
#define MT6337_PMIC_RG_ACCDET1SEL_SHIFT                       7
#define MT6337_PMIC_RG_ACCDET2SEL_ADDR                        MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_ACCDET2SEL_MASK                        0x1
#define MT6337_PMIC_RG_ACCDET2SEL_SHIFT                       8
#define MT6337_PMIC_RG_SWBUFMODSEL_ADDR                       MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_SWBUFMODSEL_MASK                       0x1
#define MT6337_PMIC_RG_SWBUFMODSEL_SHIFT                      9
#define MT6337_PMIC_RG_SWBUFSWEN_ADDR                         MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_SWBUFSWEN_MASK                         0x1
#define MT6337_PMIC_RG_SWBUFSWEN_SHIFT                        10
#define MT6337_PMIC_RG_EINTCOMPVTH_ADDR                       MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_EINTCOMPVTH_MASK                       0x1
#define MT6337_PMIC_RG_EINTCOMPVTH_SHIFT                      11
#define MT6337_PMIC_RG_EINT1CONFIGACCDET1_ADDR                MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_EINT1CONFIGACCDET1_MASK                0x1
#define MT6337_PMIC_RG_EINT1CONFIGACCDET1_SHIFT               12
#define MT6337_PMIC_RG_EINT2CONFIGACCDET2_ADDR                MT6337_AUDENC_ANA_CON19
#define MT6337_PMIC_RG_EINT2CONFIGACCDET2_MASK                0x1
#define MT6337_PMIC_RG_EINT2CONFIGACCDET2_SHIFT               13
#define MT6337_PMIC_RG_ACCDETSPARE_ADDR                       MT6337_AUDENC_ANA_CON20
#define MT6337_PMIC_RG_ACCDETSPARE_MASK                       0xFF
#define MT6337_PMIC_RG_ACCDETSPARE_SHIFT                      0
#define MT6337_PMIC_RG_AUDENCSPAREVA30_ADDR                   MT6337_AUDENC_ANA_CON21
#define MT6337_PMIC_RG_AUDENCSPAREVA30_MASK                   0xFF
#define MT6337_PMIC_RG_AUDENCSPAREVA30_SHIFT                  0
#define MT6337_PMIC_RG_AUDENCSPAREVA18_ADDR                   MT6337_AUDENC_ANA_CON21
#define MT6337_PMIC_RG_AUDENCSPAREVA18_MASK                   0xFF
#define MT6337_PMIC_RG_AUDENCSPAREVA18_SHIFT                  8
#define MT6337_PMIC_RG_PLL_EN_ADDR                            MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLL_EN_MASK                            0x1
#define MT6337_PMIC_RG_PLL_EN_SHIFT                           0
#define MT6337_PMIC_RG_PLLBS_RST_ADDR                         MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLLBS_RST_MASK                         0x1
#define MT6337_PMIC_RG_PLLBS_RST_SHIFT                        1
#define MT6337_PMIC_RG_PLL_DCKO_SEL_ADDR                      MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLL_DCKO_SEL_MASK                      0x3
#define MT6337_PMIC_RG_PLL_DCKO_SEL_SHIFT                     2
#define MT6337_PMIC_RG_PLL_DIV1_ADDR                          MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLL_DIV1_MASK                          0x3F
#define MT6337_PMIC_RG_PLL_DIV1_SHIFT                         4
#define MT6337_PMIC_RG_PLL_RLATCH_EN_ADDR                     MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLL_RLATCH_EN_MASK                     0x1
#define MT6337_PMIC_RG_PLL_RLATCH_EN_SHIFT                    10
#define MT6337_PMIC_RG_PLL_PDIV1_EN_ADDR                      MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLL_PDIV1_EN_MASK                      0x1
#define MT6337_PMIC_RG_PLL_PDIV1_EN_SHIFT                     11
#define MT6337_PMIC_RG_PLL_PDIV1_ADDR                         MT6337_AUDENC_ANA_CON22
#define MT6337_PMIC_RG_PLL_PDIV1_MASK                         0xF
#define MT6337_PMIC_RG_PLL_PDIV1_SHIFT                        12
#define MT6337_PMIC_RG_PLL_BC_ADDR                            MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_BC_MASK                            0x3
#define MT6337_PMIC_RG_PLL_BC_SHIFT                           0
#define MT6337_PMIC_RG_PLL_BP_ADDR                            MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_BP_MASK                            0x3
#define MT6337_PMIC_RG_PLL_BP_SHIFT                           2
#define MT6337_PMIC_RG_PLL_BR_ADDR                            MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_BR_MASK                            0x3
#define MT6337_PMIC_RG_PLL_BR_SHIFT                           4
#define MT6337_PMIC_RG_CKO_SEL_ADDR                           MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_CKO_SEL_MASK                           0x3
#define MT6337_PMIC_RG_CKO_SEL_SHIFT                          6
#define MT6337_PMIC_RG_PLL_IBSEL_ADDR                         MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_IBSEL_MASK                         0x3
#define MT6337_PMIC_RG_PLL_IBSEL_SHIFT                        8
#define MT6337_PMIC_RG_PLL_CKT_SEL_ADDR                       MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_CKT_SEL_MASK                       0x3
#define MT6337_PMIC_RG_PLL_CKT_SEL_SHIFT                      10
#define MT6337_PMIC_RG_PLL_VCT_EN_ADDR                        MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_VCT_EN_MASK                        0x1
#define MT6337_PMIC_RG_PLL_VCT_EN_SHIFT                       12
#define MT6337_PMIC_RG_PLL_CKT_EN_ADDR                        MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_CKT_EN_MASK                        0x1
#define MT6337_PMIC_RG_PLL_CKT_EN_SHIFT                       13
#define MT6337_PMIC_RG_PLL_HPM_EN_ADDR                        MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_HPM_EN_MASK                        0x1
#define MT6337_PMIC_RG_PLL_HPM_EN_SHIFT                       14
#define MT6337_PMIC_RG_PLL_DCHP_EN_ADDR                       MT6337_AUDENC_ANA_CON23
#define MT6337_PMIC_RG_PLL_DCHP_EN_MASK                       0x1
#define MT6337_PMIC_RG_PLL_DCHP_EN_SHIFT                      15
#define MT6337_PMIC_RG_PLL_CDIV_ADDR                          MT6337_AUDENC_ANA_CON24
#define MT6337_PMIC_RG_PLL_CDIV_MASK                          0x7
#define MT6337_PMIC_RG_PLL_CDIV_SHIFT                         0
#define MT6337_PMIC_RG_VCOBAND_ADDR                           MT6337_AUDENC_ANA_CON24
#define MT6337_PMIC_RG_VCOBAND_MASK                           0x7
#define MT6337_PMIC_RG_VCOBAND_SHIFT                          3
#define MT6337_PMIC_RG_CKDRV_EN_ADDR                          MT6337_AUDENC_ANA_CON24
#define MT6337_PMIC_RG_CKDRV_EN_MASK                          0x1
#define MT6337_PMIC_RG_CKDRV_EN_SHIFT                         6
#define MT6337_PMIC_RG_PLL_DCHP_AEN_ADDR                      MT6337_AUDENC_ANA_CON24
#define MT6337_PMIC_RG_PLL_DCHP_AEN_MASK                      0x1
#define MT6337_PMIC_RG_PLL_DCHP_AEN_SHIFT                     7
#define MT6337_PMIC_RG_PLL_RSVA_ADDR                          MT6337_AUDENC_ANA_CON24
#define MT6337_PMIC_RG_PLL_RSVA_MASK                          0xFF
#define MT6337_PMIC_RG_PLL_RSVA_SHIFT                         8
#define MT6337_PMIC_RGS_AUDRCTUNECH0_01READ_ADDR              MT6337_AUDENC_ANA_CON25
#define MT6337_PMIC_RGS_AUDRCTUNECH0_01READ_MASK              0x1F
#define MT6337_PMIC_RGS_AUDRCTUNECH0_01READ_SHIFT             0
#define MT6337_PMIC_RGS_AUDRCTUNECH1_23READ_ADDR              MT6337_AUDENC_ANA_CON25
#define MT6337_PMIC_RGS_AUDRCTUNECH1_23READ_MASK              0x1F
#define MT6337_PMIC_RGS_AUDRCTUNECH1_23READ_SHIFT             8
#define MT6337_PMIC_RGS_AUDRCTUNECH2_45READ_ADDR              MT6337_AUDENC_ANA_CON26
#define MT6337_PMIC_RGS_AUDRCTUNECH2_45READ_MASK              0x1F
#define MT6337_PMIC_RGS_AUDRCTUNECH2_45READ_SHIFT             0
#define MT6337_PMIC_RGS_AUDRCTUNECH3_6READ_ADDR               MT6337_AUDENC_ANA_CON26
#define MT6337_PMIC_RGS_AUDRCTUNECH3_6READ_MASK               0x1F
#define MT6337_PMIC_RGS_AUDRCTUNECH3_6READ_SHIFT              8
#define MT6337_PMIC_RG_AUDZCDENABLE_ADDR                      MT6337_ZCD_CON0
#define MT6337_PMIC_RG_AUDZCDENABLE_MASK                      0x1
#define MT6337_PMIC_RG_AUDZCDENABLE_SHIFT                     0
#define MT6337_PMIC_RG_AUDZCDGAINSTEPTIME_ADDR                MT6337_ZCD_CON0
#define MT6337_PMIC_RG_AUDZCDGAINSTEPTIME_MASK                0x7
#define MT6337_PMIC_RG_AUDZCDGAINSTEPTIME_SHIFT               1
#define MT6337_PMIC_RG_AUDZCDGAINSTEPSIZE_ADDR                MT6337_ZCD_CON0
#define MT6337_PMIC_RG_AUDZCDGAINSTEPSIZE_MASK                0x3
#define MT6337_PMIC_RG_AUDZCDGAINSTEPSIZE_SHIFT               4
#define MT6337_PMIC_RG_AUDZCDTIMEOUTMODESEL_ADDR              MT6337_ZCD_CON0
#define MT6337_PMIC_RG_AUDZCDTIMEOUTMODESEL_MASK              0x1
#define MT6337_PMIC_RG_AUDZCDTIMEOUTMODESEL_SHIFT             6
#define MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP15_ADDR              MT6337_ZCD_CON0
#define MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP15_MASK              0x1
#define MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP15_SHIFT             7
#define MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP15_ADDR              MT6337_ZCD_CON0
#define MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP15_MASK              0x7
#define MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP15_SHIFT             8
#define MT6337_PMIC_RG_AUDLOLGAIN_ADDR                        MT6337_ZCD_CON1
#define MT6337_PMIC_RG_AUDLOLGAIN_MASK                        0x1F
#define MT6337_PMIC_RG_AUDLOLGAIN_SHIFT                       0
#define MT6337_PMIC_RG_AUDLORGAIN_ADDR                        MT6337_ZCD_CON1
#define MT6337_PMIC_RG_AUDLORGAIN_MASK                        0x1F
#define MT6337_PMIC_RG_AUDLORGAIN_SHIFT                       7
#define MT6337_PMIC_RG_AUDHPLGAIN_ADDR                        MT6337_ZCD_CON2
#define MT6337_PMIC_RG_AUDHPLGAIN_MASK                        0x1F
#define MT6337_PMIC_RG_AUDHPLGAIN_SHIFT                       0
#define MT6337_PMIC_RG_AUDHPRGAIN_ADDR                        MT6337_ZCD_CON2
#define MT6337_PMIC_RG_AUDHPRGAIN_MASK                        0x1F
#define MT6337_PMIC_RG_AUDHPRGAIN_SHIFT                       7
#define MT6337_PMIC_RG_AUDHSGAIN_ADDR                         MT6337_ZCD_CON3
#define MT6337_PMIC_RG_AUDHSGAIN_MASK                         0x1F
#define MT6337_PMIC_RG_AUDHSGAIN_SHIFT                        0
#define MT6337_PMIC_RG_AUDIVLGAIN_ADDR                        MT6337_ZCD_CON4
#define MT6337_PMIC_RG_AUDIVLGAIN_MASK                        0x7
#define MT6337_PMIC_RG_AUDIVLGAIN_SHIFT                       0
#define MT6337_PMIC_RG_AUDIVRGAIN_ADDR                        MT6337_ZCD_CON4
#define MT6337_PMIC_RG_AUDIVRGAIN_MASK                        0x7
#define MT6337_PMIC_RG_AUDIVRGAIN_SHIFT                       8
#define MT6337_PMIC_RG_AUDINTGAIN1_ADDR                       MT6337_ZCD_CON5
#define MT6337_PMIC_RG_AUDINTGAIN1_MASK                       0x3F
#define MT6337_PMIC_RG_AUDINTGAIN1_SHIFT                      0
#define MT6337_PMIC_RG_AUDINTGAIN2_ADDR                       MT6337_ZCD_CON5
#define MT6337_PMIC_RG_AUDINTGAIN2_MASK                       0x3F
#define MT6337_PMIC_RG_AUDINTGAIN2_SHIFT                      8
#define MT6337_PMIC_GPIO_DIR0_ADDR                            MT6337_GPIO_DIR0
#define MT6337_PMIC_GPIO_DIR0_MASK                            0x7FF
#define MT6337_PMIC_GPIO_DIR0_SHIFT                           0
#define MT6337_PMIC_GPIO_DIR0_SET_ADDR                        MT6337_GPIO_DIR0_SET
#define MT6337_PMIC_GPIO_DIR0_SET_MASK                        0xFFFF
#define MT6337_PMIC_GPIO_DIR0_SET_SHIFT                       0
#define MT6337_PMIC_GPIO_DIR0_CLR_ADDR                        MT6337_GPIO_DIR0_CLR
#define MT6337_PMIC_GPIO_DIR0_CLR_MASK                        0xFFFF
#define MT6337_PMIC_GPIO_DIR0_CLR_SHIFT                       0
#define MT6337_PMIC_GPIO_PULLEN0_ADDR                         MT6337_GPIO_PULLEN0
#define MT6337_PMIC_GPIO_PULLEN0_MASK                         0x7FF
#define MT6337_PMIC_GPIO_PULLEN0_SHIFT                        0
#define MT6337_PMIC_GPIO_PULLEN0_SET_ADDR                     MT6337_GPIO_PULLEN0_SET
#define MT6337_PMIC_GPIO_PULLEN0_SET_MASK                     0xFFFF
#define MT6337_PMIC_GPIO_PULLEN0_SET_SHIFT                    0
#define MT6337_PMIC_GPIO_PULLEN0_CLR_ADDR                     MT6337_GPIO_PULLEN0_CLR
#define MT6337_PMIC_GPIO_PULLEN0_CLR_MASK                     0xFFFF
#define MT6337_PMIC_GPIO_PULLEN0_CLR_SHIFT                    0
#define MT6337_PMIC_GPIO_PULLSEL0_ADDR                        MT6337_GPIO_PULLSEL0
#define MT6337_PMIC_GPIO_PULLSEL0_MASK                        0x7FF
#define MT6337_PMIC_GPIO_PULLSEL0_SHIFT                       0
#define MT6337_PMIC_GPIO_PULLSEL0_SET_ADDR                    MT6337_GPIO_PULLSEL0_SET
#define MT6337_PMIC_GPIO_PULLSEL0_SET_MASK                    0xFFFF
#define MT6337_PMIC_GPIO_PULLSEL0_SET_SHIFT                   0
#define MT6337_PMIC_GPIO_PULLSEL0_CLR_ADDR                    MT6337_GPIO_PULLSEL0_CLR
#define MT6337_PMIC_GPIO_PULLSEL0_CLR_MASK                    0xFFFF
#define MT6337_PMIC_GPIO_PULLSEL0_CLR_SHIFT                   0
#define MT6337_PMIC_GPIO_DINV0_ADDR                           MT6337_GPIO_DINV0
#define MT6337_PMIC_GPIO_DINV0_MASK                           0x7FF
#define MT6337_PMIC_GPIO_DINV0_SHIFT                          0
#define MT6337_PMIC_GPIO_DINV0_SET_ADDR                       MT6337_GPIO_DINV0_SET
#define MT6337_PMIC_GPIO_DINV0_SET_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_DINV0_SET_SHIFT                      0
#define MT6337_PMIC_GPIO_DINV0_CLR_ADDR                       MT6337_GPIO_DINV0_CLR
#define MT6337_PMIC_GPIO_DINV0_CLR_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_DINV0_CLR_SHIFT                      0
#define MT6337_PMIC_GPIO_DOUT0_ADDR                           MT6337_GPIO_DOUT0
#define MT6337_PMIC_GPIO_DOUT0_MASK                           0x7FF
#define MT6337_PMIC_GPIO_DOUT0_SHIFT                          0
#define MT6337_PMIC_GPIO_DOUT0_SET_ADDR                       MT6337_GPIO_DOUT0_SET
#define MT6337_PMIC_GPIO_DOUT0_SET_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_DOUT0_SET_SHIFT                      0
#define MT6337_PMIC_GPIO_DOUT0_CLR_ADDR                       MT6337_GPIO_DOUT0_CLR
#define MT6337_PMIC_GPIO_DOUT0_CLR_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_DOUT0_CLR_SHIFT                      0
#define MT6337_PMIC_GPIO_PI0_ADDR                             MT6337_GPIO_PI0
#define MT6337_PMIC_GPIO_PI0_MASK                             0x7FF
#define MT6337_PMIC_GPIO_PI0_SHIFT                            0
#define MT6337_PMIC_GPIO_POE0_ADDR                            MT6337_GPIO_POE0
#define MT6337_PMIC_GPIO_POE0_MASK                            0x7FF
#define MT6337_PMIC_GPIO_POE0_SHIFT                           0
#define MT6337_PMIC_GPIO0_MODE_ADDR                           MT6337_GPIO_MODE0
#define MT6337_PMIC_GPIO0_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO0_MODE_SHIFT                          0
#define MT6337_PMIC_GPIO1_MODE_ADDR                           MT6337_GPIO_MODE0
#define MT6337_PMIC_GPIO1_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO1_MODE_SHIFT                          3
#define MT6337_PMIC_GPIO2_MODE_ADDR                           MT6337_GPIO_MODE0
#define MT6337_PMIC_GPIO2_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO2_MODE_SHIFT                          6
#define MT6337_PMIC_GPIO3_MODE_ADDR                           MT6337_GPIO_MODE0
#define MT6337_PMIC_GPIO3_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO3_MODE_SHIFT                          9
#define MT6337_PMIC_GPIO4_MODE_ADDR                           MT6337_GPIO_MODE0
#define MT6337_PMIC_GPIO4_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO4_MODE_SHIFT                          12
#define MT6337_PMIC_GPIO_MODE0_SET_ADDR                       MT6337_GPIO_MODE0_SET
#define MT6337_PMIC_GPIO_MODE0_SET_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_MODE0_SET_SHIFT                      0
#define MT6337_PMIC_GPIO_MODE0_CLR_ADDR                       MT6337_GPIO_MODE0_CLR
#define MT6337_PMIC_GPIO_MODE0_CLR_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_MODE0_CLR_SHIFT                      0
#define MT6337_PMIC_GPIO5_MODE_ADDR                           MT6337_GPIO_MODE1
#define MT6337_PMIC_GPIO5_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO5_MODE_SHIFT                          0
#define MT6337_PMIC_GPIO6_MODE_ADDR                           MT6337_GPIO_MODE1
#define MT6337_PMIC_GPIO6_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO6_MODE_SHIFT                          3
#define MT6337_PMIC_GPIO7_MODE_ADDR                           MT6337_GPIO_MODE1
#define MT6337_PMIC_GPIO7_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO7_MODE_SHIFT                          6
#define MT6337_PMIC_GPIO8_MODE_ADDR                           MT6337_GPIO_MODE1
#define MT6337_PMIC_GPIO8_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO8_MODE_SHIFT                          9
#define MT6337_PMIC_GPIO9_MODE_ADDR                           MT6337_GPIO_MODE1
#define MT6337_PMIC_GPIO9_MODE_MASK                           0x7
#define MT6337_PMIC_GPIO9_MODE_SHIFT                          12
#define MT6337_PMIC_GPIO_MODE1_SET_ADDR                       MT6337_GPIO_MODE1_SET
#define MT6337_PMIC_GPIO_MODE1_SET_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_MODE1_SET_SHIFT                      0
#define MT6337_PMIC_GPIO_MODE1_CLR_ADDR                       MT6337_GPIO_MODE1_CLR
#define MT6337_PMIC_GPIO_MODE1_CLR_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_MODE1_CLR_SHIFT                      0
#define MT6337_PMIC_GPIO10_MODE_ADDR                          MT6337_GPIO_MODE2
#define MT6337_PMIC_GPIO10_MODE_MASK                          0x7
#define MT6337_PMIC_GPIO10_MODE_SHIFT                         0
#define MT6337_PMIC_GPIO_MODE2_SET_ADDR                       MT6337_GPIO_MODE2_SET
#define MT6337_PMIC_GPIO_MODE2_SET_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_MODE2_SET_SHIFT                      0
#define MT6337_PMIC_GPIO_MODE2_CLR_ADDR                       MT6337_GPIO_MODE2_CLR
#define MT6337_PMIC_GPIO_MODE2_CLR_MASK                       0xFFFF
#define MT6337_PMIC_GPIO_MODE2_CLR_SHIFT                      0
#define MT6337_PMIC_RG_RSV_CON0_ADDR                          MT6337_RSV_CON0
#define MT6337_PMIC_RG_RSV_CON0_MASK                          0x1
#define MT6337_PMIC_RG_RSV_CON0_SHIFT                         0

typedef enum {
	MT6337_PMIC_HWCID,
	MT6337_PMIC_SWCID,
	MT6337_PMIC_ANACID,
	MT6337_PMIC_RG_SRCLKEN_IN0_EN,
	MT6337_PMIC_RG_VOWEN_EN,
	MT6337_PMIC_RG_OSC_SEL,
	MT6337_PMIC_RG_SRCLKEN_IN0_HW_MODE,
	MT6337_PMIC_RG_VOWEN_HW_MODE,
	MT6337_PMIC_RG_OSC_SEL_HW_MODE,
	MT6337_PMIC_RG_SRCLKEN_IN_SYNC_EN,
	MT6337_PMIC_RG_OSC_EN_AUTO_OFF,
	MT6337_PMIC_TEST_OUT,
	MT6337_PMIC_RG_MON_FLAG_SEL,
	MT6337_PMIC_RG_MON_GRP_SEL,
	MT6337_PMIC_RG_NANDTREE_MODE,
	MT6337_PMIC_RG_TEST_AUXADC,
	MT6337_PMIC_RG_EFUSE_MODE,
	MT6337_PMIC_RG_TEST_STRUP,
	MT6337_PMIC_TESTMODE_SW,
	MT6337_PMIC_VA18_PG_DEB,
	MT6337_PMIC_VA18_OC_STATUS,
	MT6337_PMIC_VA25_OC_STATUS,
	MT6337_PMIC_PMU_THR_DEB,
	MT6337_PMIC_PMU_TEST_MODE_SCAN,
	MT6337_PMIC_RG_PMU_TDSEL,
	MT6337_PMIC_RG_SPI_TDSEL,
	MT6337_PMIC_RG_AUD_TDSEL,
	MT6337_PMIC_RG_E32CAL_TDSEL,
	MT6337_PMIC_RG_PMU_RDSEL,
	MT6337_PMIC_RG_SPI_RDSEL,
	MT6337_PMIC_RG_AUD_RDSEL,
	MT6337_PMIC_RG_E32CAL_RDSEL,
	MT6337_PMIC_RG_SMT_WDTRSTB_IN,
	MT6337_PMIC_RG_SMT_SRCLKEN_IN0,
	MT6337_PMIC_RG_SMT_SPI_CLK,
	MT6337_PMIC_RG_SMT_SPI_CSN,
	MT6337_PMIC_RG_SMT_SPI_MOSI,
	MT6337_PMIC_RG_SMT_SPI_MISO,
	MT6337_PMIC_RG_SMT_AUD_CLK_MOSI,
	MT6337_PMIC_RG_SMT_AUD_DAT_MOSI1,
	MT6337_PMIC_RG_SMT_AUD_DAT_MOSI2,
	MT6337_PMIC_RG_SMT_AUD_DAT_MISO1,
	MT6337_PMIC_RG_SMT_AUD_DAT_MISO2,
	MT6337_PMIC_RG_SMT_VOW_CLK_MISO,
	MT6337_PMIC_RG_OCTL_SRCLKEN_IN0,
	MT6337_PMIC_RG_OCTL_SPI_CLK,
	MT6337_PMIC_RG_OCTL_SPI_CSN,
	MT6337_PMIC_RG_OCTL_SPI_MOSI,
	MT6337_PMIC_RG_OCTL_SPI_MISO,
	MT6337_PMIC_RG_OCTL_AUD_CLK_MOSI,
	MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI1,
	MT6337_PMIC_RG_OCTL_AUD_DAT_MOSI2,
	MT6337_PMIC_RG_OCTL_AUD_DAT_MISO1,
	MT6337_PMIC_RG_OCTL_AUD_DAT_MISO2,
	MT6337_PMIC_RG_OCTL_VOW_CLK_MISO,
	MT6337_PMIC_RG_FILTER_SPI_CLK,
	MT6337_PMIC_RG_FILTER_SPI_CSN,
	MT6337_PMIC_RG_FILTER_SPI_MOSI,
	MT6337_PMIC_RG_FILTER_SPI_MISO,
	MT6337_PMIC_RG_FILTER_AUD_CLK_MOSI,
	MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI1,
	MT6337_PMIC_RG_FILTER_AUD_DAT_MOSI2,
	MT6337_PMIC_RG_FILTER_AUD_DAT_MISO1,
	MT6337_PMIC_RG_FILTER_AUD_DAT_MISO2,
	MT6337_PMIC_RG_FILTER_VOW_CLK_MISO,
	MT6337_PMIC_RG_SPI_MOSI_DLY,
	MT6337_PMIC_RG_SPI_MOSI_OE_DLY,
	MT6337_PMIC_RG_SPI_MISO_DLY,
	MT6337_PMIC_RG_SPI_MISO_OE_DLY,
	MT6337_PMIC_TOP_STATUS,
	MT6337_PMIC_TOP_STATUS_SET,
	MT6337_PMIC_TOP_STATUS_CLR,
	MT6337_PMIC_RG_SRCLKEN_IN2_EN,
	MT6337_PMIC_RG_G_SMPS_PD_CK_PDN,
	MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN,
	MT6337_PMIC_RG_SMPS_AO_1M_CK_PDN,
	MT6337_PMIC_RG_STRUP_75K_CK_PDN,
	MT6337_PMIC_RG_TRIM_75K_CK_PDN,
	MT6337_PMIC_RG_VOW32K_CK_PDN,
	MT6337_PMIC_RG_VOW12M_CK_PDN,
	MT6337_PMIC_RG_AUXADC_CK_PDN,
	MT6337_PMIC_RG_AUXADC_1M_CK_PDN,
	MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN,
	MT6337_PMIC_RG_AUXADC_RNG_CK_PDN,
	MT6337_PMIC_RG_AUDIF_CK_PDN,
	MT6337_PMIC_RG_AUD_CK_PDN,
	MT6337_PMIC_RG_ZCD13M_CK_PDN,
	MT6337_PMIC_TOP_CKPDN_CON0_SET,
	MT6337_PMIC_TOP_CKPDN_CON0_CLR,
	MT6337_PMIC_RG_BUCK_9M_CK_PDN,
	MT6337_PMIC_RG_BUCK_32K_CK_PDN,
	MT6337_PMIC_RG_INTRP_CK_PDN,
	MT6337_PMIC_RG_INTRP_PRE_OC_CK_PDN,
	MT6337_PMIC_RG_EFUSE_CK_PDN,
	MT6337_PMIC_RG_FQMTR_32K_CK_PDN,
	MT6337_PMIC_RG_FQMTR_26M_CK_PDN,
	MT6337_PMIC_RG_FQMTR_CK_PDN,
	MT6337_PMIC_RG_LDO_CALI_75K_CK_PDN,
	MT6337_PMIC_RG_STB_1M_CK_PDN,
	MT6337_PMIC_RG_SMPS_CK_DIV_PDN,
	MT6337_PMIC_RG_ACCDET_CK_PDN,
	MT6337_PMIC_RG_BGR_TEST_CK_PDN,
	MT6337_PMIC_RG_REG_CK_PDN,
	MT6337_PMIC_TOP_CKPDN_CON1_SET,
	MT6337_PMIC_TOP_CKPDN_CON1_CLR,
	MT6337_PMIC_RG_AUDIF_CK_CKSEL,
	MT6337_PMIC_RG_AUD_CK_CKSEL,
	MT6337_PMIC_RG_STRUP_75K_CK_CKSEL,
	MT6337_PMIC_RG_BGR_TEST_CK_CKSEL,
	MT6337_PMIC_RG_FQMTR_CK_CKSEL,
	MT6337_PMIC_RG_OSC_SEL_HW_SRC_SEL,
	MT6337_PMIC_RG_TOP_CKSEL_CON0_RSV,
	MT6337_PMIC_TOP_CKSEL_CON_SET,
	MT6337_PMIC_TOP_CKSEL_CON_CLR,
	MT6337_PMIC_RG_AUXADC_SMPS_CK_DIVSEL,
	MT6337_PMIC_RG_REG_CK_DIVSEL,
	MT6337_PMIC_RG_BUCK_9M_CK_DIVSEL,
	MT6337_PMIC_TOP_CKDIVSEL_CON0_RSV,
	MT6337_PMIC_TOP_CKDIVSEL_CON0_SET,
	MT6337_PMIC_TOP_CKDIVSEL_CON0_CLR,
	MT6337_PMIC_RG_G_SMPS_PD_CK_PDN_HWEN,
	MT6337_PMIC_RG_G_SMPS_AUD_CK_PDN_HWEN,
	MT6337_PMIC_RG_AUXADC_CK_PDN_HWEN,
	MT6337_PMIC_RG_AUXADC_SMPS_CK_PDN_HWEN,
	MT6337_PMIC_RG_EFUSE_CK_PDN_HWEN,
	MT6337_PMIC_RG_STB_1M_CK_PDN_HWEN,
	MT6337_PMIC_RG_AUXADC_RNG_CK_PDN_HWEN,
	MT6337_PMIC_RG_REG_CK_PDN_HWEN,
	MT6337_PMIC_TOP_CKHWEN_CON0_RSV,
	MT6337_PMIC_TOP_CKHWEN_CON0_SET,
	MT6337_PMIC_TOP_CKHWEN_CON0_CLR,
	MT6337_PMIC_TOP_CKHWEN_CON1_RSV,
	MT6337_PMIC_RG_PMU75K_CK_TST_DIS,
	MT6337_PMIC_RG_SMPS_CK_TST_DIS,
	MT6337_PMIC_RG_AUD26M_CK_TST_DIS,
	MT6337_PMIC_RG_CLK32K_CK_TST_DIS,
	MT6337_PMIC_RG_VOW12M_CK_TST_DIS,
	MT6337_PMIC_TOP_CKTST_CON0_RSV,
	MT6337_PMIC_RG_VOW12M_CK_TSTSEL,
	MT6337_PMIC_RG_AUD26M_CK_TSTSEL,
	MT6337_PMIC_RG_AUDIF_CK_TSTSEL,
	MT6337_PMIC_RG_AUD_CK_TSTSEL,
	MT6337_PMIC_RG_FQMTR_CK_TSTSEL,
	MT6337_PMIC_RG_PMU75K_CK_TSTSEL,
	MT6337_PMIC_RG_SMPS_CK_TSTSEL,
	MT6337_PMIC_RG_STRUP_75K_CK_TSTSEL,
	MT6337_PMIC_RG_CLK32K_CK_TSTSEL,
	MT6337_PMIC_RG_BGR_TEST_CK_TSTSEL,
	MT6337_PMIC_RG_CLKSQ_EN_AUD,
	MT6337_PMIC_RG_CLKSQ_EN_FQR,
	MT6337_PMIC_RG_CLKSQ_EN_AUX_AP,
	MT6337_PMIC_RG_CLKSQ_EN_AUX_RSV,
	MT6337_PMIC_RG_CLKSQ_EN_AUX_AP_MODE,
	MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18,
	MT6337_PMIC_RG_CLKSQ_IN_SEL_VA18_SWCTRL,
	MT6337_PMIC_TOP_CLKSQ_RSV,
	MT6337_PMIC_DA_CLKSQ_EN_VA18,
	MT6337_PMIC_TOP_CLKSQ_SET,
	MT6337_PMIC_TOP_CLKSQ_CLR,
	MT6337_PMIC_TOP_CLKSQ_RTC_RSV0,
	MT6337_PMIC_RG_CLKSQ_EN_6336,
	MT6337_PMIC_TOP_CLKSQ_RTC_SET,
	MT6337_PMIC_TOP_CLKSQ_RTC_CLR,
	MT6337_PMIC_OSC_75K_TRIM,
	MT6337_PMIC_RG_OSC_75K_TRIM_EN,
	MT6337_PMIC_RG_OSC_75K_TRIM_RATE,
	MT6337_PMIC_DA_OSC_75K_TRIM,
	MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_EN,
	MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN0_DLY_EN,
	MT6337_PMIC_RG_G_SMPS_CK_PDN_SRCLKEN2_EN,
	MT6337_PMIC_RG_G_SMPS_CK_PDN_BUCK_OSC_SEL_EN,
	MT6337_PMIC_RG_G_SMPS_CK_PDN_VOWEN_EN,
	MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_EN,
	MT6337_PMIC_RG_OSC_SEL_SRCLKEN0_DLY_EN,
	MT6337_PMIC_RG_OSC_SEL_SRCLKEN2_EN,
	MT6337_PMIC_RG_OSC_SEL_BUCK_LDO_EN,
	MT6337_PMIC_RG_OSC_SEL_VOWEN_EN,
	MT6337_PMIC_RG_OSC_SEL_SPI_EN,
	MT6337_PMIC_RG_CLK_RSV,
	MT6337_PMIC_TOP_CLK_CON0_SET,
	MT6337_PMIC_TOP_CLK_CON0_CLR,
	MT6337_PMIC_BUCK_LDO_FT_TESTMODE_EN,
	MT6337_PMIC_BUCK_ALL_CON0_RSV1,
	MT6337_PMIC_BUCK_ALL_CON0_RSV0,
	MT6337_PMIC_BUCK_BUCK_RSV,
	MT6337_PMIC_RG_OSC_SEL_SW,
	MT6337_PMIC_RG_OSC_SEL_LDO_MODE,
	MT6337_PMIC_BUCK_BUCK_VSLEEP_SRCLKEN_SEL,
	MT6337_PMIC_BUCK_ALL_CON2_RSV0,
	MT6337_PMIC_BUCK_VSLEEP_SRC0,
	MT6337_PMIC_BUCK_VSLEEP_SRC1,
	MT6337_PMIC_BUCK_R2R_SRC0,
	MT6337_PMIC_BUCK_R2R_SRC1,
	MT6337_PMIC_RG_OSC_SEL_DLY_MAX,
	MT6337_PMIC_BUCK_SRCLKEN_DLY_SRC1,
	MT6337_PMIC_RG_EFUSE_MAN_RST,
	MT6337_PMIC_RG_AUXADC_RST,
	MT6337_PMIC_RG_AUXADC_REG_RST,
	MT6337_PMIC_RG_AUDIO_RST,
	MT6337_PMIC_RG_ACCDET_RST,
	MT6337_PMIC_RG_INTCTL_RST,
	MT6337_PMIC_RG_BUCK_CALI_RST,
	MT6337_PMIC_RG_CLK_TRIM_RST,
	MT6337_PMIC_RG_FQMTR_RST,
	MT6337_PMIC_RG_LDO_CALI_RST,
	MT6337_PMIC_RG_ZCD_RST,
	MT6337_PMIC_TOP_RST_CON0_SET,
	MT6337_PMIC_TOP_RST_CON0_CLR,
	MT6337_PMIC_RG_STRUP_LONG_PRESS_RST,
	MT6337_PMIC_TOP_RST_CON1_RSV,
	MT6337_PMIC_TOP_RST_CON1_SET,
	MT6337_PMIC_TOP_RST_CON1_CLR,
	MT6337_PMIC_TOP_RST_CON2_RSV,
	MT6337_PMIC_RG_WDTRSTB_EN,
	MT6337_PMIC_RG_WDTRSTB_MODE,
	MT6337_PMIC_WDTRSTB_STATUS,
	MT6337_PMIC_WDTRSTB_STATUS_CLR,
	MT6337_PMIC_RG_WDTRSTB_FB_EN,
	MT6337_PMIC_RG_WDTRSTB_DEB,
	MT6337_PMIC_TOP_RST_MISC_RSV,
	MT6337_PMIC_TOP_RST_MISC_SET,
	MT6337_PMIC_TOP_RST_MISC_CLR,
	MT6337_PMIC_VPWRIN_RSTB_STATUS,
	MT6337_PMIC_TOP_RST_STATUS_RSV,
	MT6337_PMIC_TOP_RST_STATUS_SET,
	MT6337_PMIC_TOP_RST_STATUS_CLR,
	MT6337_PMIC_RG_INT_EN_THR_H,
	MT6337_PMIC_RG_INT_EN_THR_L,
	MT6337_PMIC_RG_INT_EN_AUDIO,
	MT6337_PMIC_RG_INT_EN_MAD,
	MT6337_PMIC_RG_INT_EN_ACCDET,
	MT6337_PMIC_RG_INT_EN_ACCDET_EINT,
	MT6337_PMIC_RG_INT_EN_ACCDET_EINT1,
	MT6337_PMIC_RG_INT_EN_ACCDET_NEGV,
	MT6337_PMIC_RG_INT_EN_PMU_THR,
	MT6337_PMIC_RG_INT_EN_LDO_VA18_OC,
	MT6337_PMIC_RG_INT_EN_LDO_VA25_OC,
	MT6337_PMIC_RG_INT_EN_LDO_VA18_PG,
	MT6337_PMIC_INT_CON0_SET,
	MT6337_PMIC_INT_CON0_CLR,
	MT6337_PMIC_RG_INT_MASK_B_THR_H,
	MT6337_PMIC_RG_INT_MASK_B_THR_L,
	MT6337_PMIC_RG_INT_MASK_B_AUDIO,
	MT6337_PMIC_RG_INT_MASK_B_MAD,
	MT6337_PMIC_RG_INT_MASK_B_ACCDET,
	MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT,
	MT6337_PMIC_RG_INT_MASK_B_ACCDET_EINT1,
	MT6337_PMIC_RG_INT_MASK_B_ACCDET_NEGV,
	MT6337_PMIC_RG_INT_MASK_B_PMU_THR,
	MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_OC,
	MT6337_PMIC_RG_INT_MASK_B_LDO_VA25_OC,
	MT6337_PMIC_RG_INT_MASK_B_LDO_VA18_PG,
	MT6337_PMIC_INT_CON1_SET,
	MT6337_PMIC_INT_CON1_CLR,
	MT6337_PMIC_RG_INT_SEL_THR_H,
	MT6337_PMIC_RG_INT_SEL_THR_L,
	MT6337_PMIC_RG_INT_SEL_AUDIO,
	MT6337_PMIC_RG_INT_SEL_MAD,
	MT6337_PMIC_RG_INT_SEL_ACCDET,
	MT6337_PMIC_RG_INT_SEL_ACCDET_EINT,
	MT6337_PMIC_RG_INT_SEL_ACCDET_EINT1,
	MT6337_PMIC_RG_INT_SEL_ACCDET_NEGV,
	MT6337_PMIC_RG_INT_SEL_PMU_THR,
	MT6337_PMIC_RG_INT_SEL_LDO_VA18_OC,
	MT6337_PMIC_RG_INT_SEL_LDO_VA25_OC,
	MT6337_PMIC_RG_INT_SEL_LDO_VA18_PG,
	MT6337_PMIC_INT_CON2_SET,
	MT6337_PMIC_INT_CON2_CLR,
	MT6337_PMIC_POLARITY,
	MT6337_PMIC_RG_PCHR_CM_VDEC_POLARITY_RSV,
	MT6337_PMIC_INT_MISC_CON_SET,
	MT6337_PMIC_INT_MISC_CON_CLR,
	MT6337_PMIC_RG_INT_STATUS_THR_H,
	MT6337_PMIC_RG_INT_STATUS_THR_L,
	MT6337_PMIC_RG_INT_STATUS_AUDIO,
	MT6337_PMIC_RG_INT_STATUS_MAD,
	MT6337_PMIC_RG_INT_STATUS_ACCDET,
	MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT,
	MT6337_PMIC_RG_INT_STATUS_ACCDET_EINT1,
	MT6337_PMIC_RG_INT_STATUS_ACCDET_NEGV,
	MT6337_PMIC_RG_INT_STATUS_PMU_THR,
	MT6337_PMIC_RG_INT_STATUS_LDO_VA18_OC,
	MT6337_PMIC_RG_INT_STATUS_LDO_VA25_OC,
	MT6337_PMIC_RG_INT_STATUS_LDO_VA18_PG,
	MT6337_PMIC_RG_THR_DET_DIS,
	MT6337_PMIC_RG_THR_TEST,
	MT6337_PMIC_THR_HWPDN_EN,
	MT6337_PMIC_RG_THERMAL_DEB_SEL,
	MT6337_PMIC_RG_VA18_PG_DEB_SEL,
	MT6337_PMIC_STRUP_CON4_RSV,
	MT6337_PMIC_STRUP_OSC_EN,
	MT6337_PMIC_STRUP_OSC_EN_SEL,
	MT6337_PMIC_STRUP_FT_CTRL,
	MT6337_PMIC_STRUP_PWRON_FORCE,
	MT6337_PMIC_BIAS_GEN_EN_FORCE,
	MT6337_PMIC_STRUP_PWRON,
	MT6337_PMIC_STRUP_PWRON_SEL,
	MT6337_PMIC_BIAS_GEN_EN,
	MT6337_PMIC_BIAS_GEN_EN_SEL,
	MT6337_PMIC_VBGSW_ENB_EN,
	MT6337_PMIC_VBGSW_ENB_EN_SEL,
	MT6337_PMIC_VBGSW_ENB_FORCE,
	MT6337_PMIC_STRUP_DIG_IO_PG_FORCE,
	MT6337_PMIC_RG_STRUP_VA18_EN,
	MT6337_PMIC_RG_STRUP_VA18_EN_SEL,
	MT6337_PMIC_RG_STRUP_VA18_STB,
	MT6337_PMIC_RG_STRUP_VA18_STB_SEL,
	MT6337_PMIC_RG_VA18_PGST,
	MT6337_PMIC_RG_VA18_PGST_SEL,
	MT6337_PMIC_RG_VA18_PGST_AUDENC,
	MT6337_PMIC_RG_VA18_PGST_AUDENC_SEL,
	MT6337_PMIC_DA_QI_OSC_EN,
	MT6337_PMIC_RG_BGR_UNCHOP,
	MT6337_PMIC_RG_BGR_UNCHOP_PH,
	MT6337_PMIC_RG_BGR_RSEL,
	MT6337_PMIC_RG_BGR_TRIM,
	MT6337_PMIC_RG_BGR_TRIM_EN,
	MT6337_PMIC_RG_BGR_TEST_RSTB,
	MT6337_PMIC_RG_BGR_TEST_EN,
	MT6337_PMIC_RG_BYPASSMODESEL,
	MT6337_PMIC_RG_BYPASSMODEEN,
	MT6337_PMIC_RG_BGR_OSC_EN_TEST,
	MT6337_PMIC_RG_TSENS_EN,
	MT6337_PMIC_RG_SPAREBGR,
	MT6337_PMIC_RG_STRUP_IREF_TRIM,
	MT6337_PMIC_RG_VREF_BG,
	MT6337_PMIC_RG_STRUP_THR_SEL,
	MT6337_PMIC_RG_THR_TMODE,
	MT6337_PMIC_RG_THRDET_SEL,
	MT6337_PMIC_RG_VTHR_POL,
	MT6337_PMIC_RG_SPAREIVGEN,
	MT6337_PMIC_RG_OVT_EN,
	MT6337_PMIC_RG_IVGEN_EXT_TEST_EN,
	MT6337_PMIC_RG_TOPSPAREVA18,
	MT6337_PMIC_RGS_ANA_CHIP_ID,
	MT6337_PMIC_RG_SLP_RW_EN,
	MT6337_PMIC_RG_SPI_RSV,
	MT6337_PMIC_DEW_DIO_EN,
	MT6337_PMIC_DEW_READ_TEST,
	MT6337_PMIC_DEW_WRITE_TEST,
	MT6337_PMIC_DEW_CRC_SWRST,
	MT6337_PMIC_DEW_CRC_EN,
	MT6337_PMIC_DEW_CRC_VAL,
	MT6337_PMIC_DEW_DBG_MON_SEL,
	MT6337_PMIC_DEW_CIPHER_KEY_SEL,
	MT6337_PMIC_DEW_CIPHER_IV_SEL,
	MT6337_PMIC_DEW_CIPHER_EN,
	MT6337_PMIC_DEW_CIPHER_RDY,
	MT6337_PMIC_DEW_CIPHER_MODE,
	MT6337_PMIC_DEW_CIPHER_SWRST,
	MT6337_PMIC_DEW_RDDMY_NO,
	MT6337_PMIC_INT_TYPE_CON0,
	MT6337_PMIC_INT_TYPE_CON0_SET,
	MT6337_PMIC_INT_TYPE_CON0_CLR,
	MT6337_PMIC_INT_TYPE_CON1,
	MT6337_PMIC_INT_TYPE_CON1_SET,
	MT6337_PMIC_INT_TYPE_CON1_CLR,
	MT6337_PMIC_INT_TYPE_CON2,
	MT6337_PMIC_INT_TYPE_CON2_SET,
	MT6337_PMIC_INT_TYPE_CON2_CLR,
	MT6337_PMIC_INT_TYPE_CON3,
	MT6337_PMIC_INT_TYPE_CON3_SET,
	MT6337_PMIC_INT_TYPE_CON3_CLR,
	MT6337_PMIC_CPU_INT_STA,
	MT6337_PMIC_MD32_INT_STA,
	MT6337_PMIC_RG_SRCLKEN_IN2_SMPS_CLK_MODE,
	MT6337_PMIC_RG_SRCLKEN_IN2_EN_SMPS_TEST,
	MT6337_PMIC_RG_SPI_DLY_SEL,
	MT6337_PMIC_FQMTR_TCKSEL,
	MT6337_PMIC_FQMTR_BUSY,
	MT6337_PMIC_FQMTR_EN,
	MT6337_PMIC_FQMTR_WINSET,
	MT6337_PMIC_FQMTR_DATA,
	MT6337_PMIC_BUCK_K_RST_DONE,
	MT6337_PMIC_BUCK_K_MAP_SEL,
	MT6337_PMIC_BUCK_K_ONCE_EN,
	MT6337_PMIC_BUCK_K_ONCE,
	MT6337_PMIC_BUCK_K_START_MANUAL,
	MT6337_PMIC_BUCK_K_SRC_SEL,
	MT6337_PMIC_BUCK_K_AUTO_EN,
	MT6337_PMIC_BUCK_K_INV,
	MT6337_PMIC_BUCK_K_CONTROL_SMPS,
	MT6337_PMIC_K_RESULT,
	MT6337_PMIC_K_DONE,
	MT6337_PMIC_K_CONTROL,
	MT6337_PMIC_DA_QI_SMPS_OSC_CAL,
	MT6337_PMIC_BUCK_K_BUCK_CK_CNT,
	MT6337_PMIC_RG_LDO_RSV1,
	MT6337_PMIC_RG_LDO_RSV0,
	MT6337_PMIC_RG_LDO_RSV2,
	MT6337_PMIC_LDO_DEGTD_SEL,
	MT6337_PMIC_RG_VA18_SW_EN,
	MT6337_PMIC_RG_VA18_SW_LP,
	MT6337_PMIC_RG_VA18_SW_OP_EN,
	MT6337_PMIC_RG_VA18_HW0_OP_EN,
	MT6337_PMIC_RG_VA18_HW2_OP_EN,
	MT6337_PMIC_RG_VA18_OP_EN_SET,
	MT6337_PMIC_RG_VA18_OP_EN_CLR,
	MT6337_PMIC_RG_VA18_HW0_OP_CFG,
	MT6337_PMIC_RG_VA18_HW2_OP_CFG,
	MT6337_PMIC_RG_VA18_GO_ON_OP,
	MT6337_PMIC_RG_VA18_GO_LP_OP,
	MT6337_PMIC_RG_VA18_OP_CFG_SET,
	MT6337_PMIC_RG_VA18_OP_CFG_CLR,
	MT6337_PMIC_DA_QI_VA18_MODE,
	MT6337_PMIC_RG_VA18_STBTD,
	MT6337_PMIC_DA_QI_VA18_STB,
	MT6337_PMIC_DA_QI_VA18_EN,
	MT6337_PMIC_RG_VA18_OCFB_EN,
	MT6337_PMIC_DA_QI_VA18_OCFB_EN,
	MT6337_PMIC_RG_VA18_DUMMY_LOAD,
	MT6337_PMIC_DA_QI_VA18_DUMMY_LOAD,
	MT6337_PMIC_RG_VA25_SW_EN,
	MT6337_PMIC_RG_VA25_SW_LP,
	MT6337_PMIC_RG_VA25_SW_OP_EN,
	MT6337_PMIC_RG_VA25_HW0_OP_EN,
	MT6337_PMIC_RG_VA25_HW2_OP_EN,
	MT6337_PMIC_RG_VA25_OP_EN_SET,
	MT6337_PMIC_RG_VA25_OP_EN_CLR,
	MT6337_PMIC_RG_VA25_HW0_OP_CFG,
	MT6337_PMIC_RG_VA25_HW2_OP_CFG,
	MT6337_PMIC_RG_VA25_GO_ON_OP,
	MT6337_PMIC_RG_VA25_GO_LP_OP,
	MT6337_PMIC_RG_VA25_OP_CFG_SET,
	MT6337_PMIC_RG_VA25_OP_CFG_CLR,
	MT6337_PMIC_DA_QI_VA25_MODE,
	MT6337_PMIC_RG_VA25_STBTD,
	MT6337_PMIC_DA_QI_VA25_STB,
	MT6337_PMIC_DA_QI_VA25_EN,
	MT6337_PMIC_RG_VA25_OCFB_EN,
	MT6337_PMIC_DA_QI_VA25_OCFB_EN,
	MT6337_PMIC_RG_DCM_MODE,
	MT6337_PMIC_RG_VA18_CK_SW_MODE,
	MT6337_PMIC_RG_VA18_CK_SW_EN,
	MT6337_PMIC_RG_VA25_CK_SW_MODE,
	MT6337_PMIC_RG_VA25_CK_SW_EN,
	MT6337_PMIC_RG_VA25_CAL,
	MT6337_PMIC_RG_VA25_VOSEL,
	MT6337_PMIC_RG_VA25_NDIS_EN,
	MT6337_PMIC_RG_VA25_FBSEL,
	MT6337_PMIC_RG_VA18_CAL,
	MT6337_PMIC_RG_VA18_VOSEL,
	MT6337_PMIC_RG_VA18_PG_STATUS_EN,
	MT6337_PMIC_RG_VA18_NDIS_EN,
	MT6337_PMIC_RG_VA18_STB_SEL,
	MT6337_PMIC_RG_OTP_PA,
	MT6337_PMIC_RG_OTP_PDIN,
	MT6337_PMIC_RG_OTP_PTM,
	MT6337_PMIC_RG_OTP_PWE,
	MT6337_PMIC_RG_OTP_PPROG,
	MT6337_PMIC_RG_OTP_PWE_SRC,
	MT6337_PMIC_RG_OTP_PROG_PKEY,
	MT6337_PMIC_RG_OTP_RD_PKEY,
	MT6337_PMIC_RG_OTP_RD_TRIG,
	MT6337_PMIC_RG_RD_RDY_BYPASS,
	MT6337_PMIC_RG_SKIP_OTP_OUT,
	MT6337_PMIC_RG_OTP_RD_SW,
	MT6337_PMIC_RG_OTP_RD_BUSY,
	MT6337_PMIC_RG_OTP_RD_ACK,
	MT6337_PMIC_RG_OTP_DOUT_0_15,
	MT6337_PMIC_RG_OTP_DOUT_16_31,
	MT6337_PMIC_RG_OTP_DOUT_32_47,
	MT6337_PMIC_RG_OTP_DOUT_48_63,
	MT6337_PMIC_RG_OTP_DOUT_64_79,
	MT6337_PMIC_RG_OTP_DOUT_80_95,
	MT6337_PMIC_RG_OTP_DOUT_96_111,
	MT6337_PMIC_RG_OTP_DOUT_112_127,
	MT6337_PMIC_RG_OTP_DOUT_128_143,
	MT6337_PMIC_RG_OTP_DOUT_144_159,
	MT6337_PMIC_RG_OTP_DOUT_160_175,
	MT6337_PMIC_RG_OTP_DOUT_176_191,
	MT6337_PMIC_RG_OTP_DOUT_192_207,
	MT6337_PMIC_RG_OTP_DOUT_208_223,
	MT6337_PMIC_RG_OTP_DOUT_224_239,
	MT6337_PMIC_RG_OTP_DOUT_240_255,
	MT6337_PMIC_RG_OTP_DOUT_256_271,
	MT6337_PMIC_RG_OTP_DOUT_272_287,
	MT6337_PMIC_RG_OTP_DOUT_288_303,
	MT6337_PMIC_RG_OTP_DOUT_304_319,
	MT6337_PMIC_RG_OTP_DOUT_320_335,
	MT6337_PMIC_RG_OTP_DOUT_336_351,
	MT6337_PMIC_RG_OTP_DOUT_352_367,
	MT6337_PMIC_RG_OTP_DOUT_368_383,
	MT6337_PMIC_RG_OTP_DOUT_384_399,
	MT6337_PMIC_RG_OTP_DOUT_400_415,
	MT6337_PMIC_RG_OTP_DOUT_416_431,
	MT6337_PMIC_RG_OTP_DOUT_432_447,
	MT6337_PMIC_RG_OTP_DOUT_448_463,
	MT6337_PMIC_RG_OTP_DOUT_464_479,
	MT6337_PMIC_RG_OTP_DOUT_480_495,
	MT6337_PMIC_RG_OTP_DOUT_496_511,
	MT6337_PMIC_RG_OTP_VAL_0_15,
	MT6337_PMIC_RG_OTP_VAL_16_31,
	MT6337_PMIC_RG_OTP_VAL_32_47,
	MT6337_PMIC_RG_OTP_VAL_48_63,
	MT6337_PMIC_RG_OTP_VAL_64_79,
	MT6337_PMIC_RG_OTP_VAL_80_90,
	MT6337_PMIC_RG_OTP_VAL_222_223,
	MT6337_PMIC_RG_OTP_VAL_224_239,
	MT6337_PMIC_RG_OTP_VAL_240_252,
	MT6337_PMIC_RG_OTP_VAL_511,
	MT6337_PMIC_AUXADC_ADC_OUT_CH0,
	MT6337_PMIC_AUXADC_ADC_RDY_CH0,
	MT6337_PMIC_AUXADC_ADC_OUT_CH1,
	MT6337_PMIC_AUXADC_ADC_RDY_CH1,
	MT6337_PMIC_AUXADC_ADC_OUT_CH2,
	MT6337_PMIC_AUXADC_ADC_RDY_CH2,
	MT6337_PMIC_AUXADC_ADC_OUT_CH3,
	MT6337_PMIC_AUXADC_ADC_RDY_CH3,
	MT6337_PMIC_AUXADC_ADC_OUT_CH4,
	MT6337_PMIC_AUXADC_ADC_RDY_CH4,
	MT6337_PMIC_AUXADC_ADC_OUT_CH5,
	MT6337_PMIC_AUXADC_ADC_RDY_CH5,
	MT6337_PMIC_AUXADC_ADC_OUT_CH6,
	MT6337_PMIC_AUXADC_ADC_RDY_CH6,
	MT6337_PMIC_AUXADC_ADC_OUT_CH7,
	MT6337_PMIC_AUXADC_ADC_RDY_CH7,
	MT6337_PMIC_AUXADC_ADC_OUT_CH8,
	MT6337_PMIC_AUXADC_ADC_RDY_CH8,
	MT6337_PMIC_AUXADC_ADC_OUT_CH9,
	MT6337_PMIC_AUXADC_ADC_RDY_CH9,
	MT6337_PMIC_AUXADC_ADC_OUT_CH10,
	MT6337_PMIC_AUXADC_ADC_RDY_CH10,
	MT6337_PMIC_AUXADC_ADC_OUT_CH11,
	MT6337_PMIC_AUXADC_ADC_RDY_CH11,
	MT6337_PMIC_AUXADC_ADC_OUT_CH12_15,
	MT6337_PMIC_AUXADC_ADC_RDY_CH12_15,
	MT6337_PMIC_AUXADC_ADC_OUT_THR_HW,
	MT6337_PMIC_AUXADC_ADC_RDY_THR_HW,
	MT6337_PMIC_AUXADC_ADC_OUT_CH4_BY_MD,
	MT6337_PMIC_AUXADC_ADC_RDY_CH4_BY_MD,
	MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_MD,
	MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_MD,
	MT6337_PMIC_AUXADC_ADC_OUT_CH0_BY_AP,
	MT6337_PMIC_AUXADC_ADC_RDY_CH0_BY_AP,
	MT6337_PMIC_AUXADC_ADC_OUT_RAW,
	MT6337_PMIC_AUXADC_ADC_BUSY_IN,
	MT6337_PMIC_AUXADC_ADC_BUSY_IN_SHARE,
	MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_HW,
	MT6337_PMIC_AUXADC_ADC_BUSY_IN_THR_MD,
	MT6337_PMIC_AUXADC_RQST_CH0,
	MT6337_PMIC_AUXADC_RQST_CH1,
	MT6337_PMIC_AUXADC_RQST_CH2,
	MT6337_PMIC_AUXADC_RQST_CH3,
	MT6337_PMIC_AUXADC_RQST_CH4,
	MT6337_PMIC_AUXADC_RQST_CH5,
	MT6337_PMIC_AUXADC_RQST_CH6,
	MT6337_PMIC_AUXADC_RQST_CH7,
	MT6337_PMIC_AUXADC_RQST_CH8,
	MT6337_PMIC_AUXADC_RQST_CH9,
	MT6337_PMIC_AUXADC_RQST_CH10,
	MT6337_PMIC_AUXADC_RQST_CH11,
	MT6337_PMIC_AUXADC_RQST_CH12,
	MT6337_PMIC_AUXADC_RQST_CH13,
	MT6337_PMIC_AUXADC_RQST_CH14,
	MT6337_PMIC_AUXADC_RQST_CH15,
	MT6337_PMIC_AUXADC_RQST0_SET,
	MT6337_PMIC_AUXADC_RQST0_CLR,
	MT6337_PMIC_AUXADC_RQST_CH0_BY_MD,
	MT6337_PMIC_AUXADC_RQST_RSV0,
	MT6337_PMIC_AUXADC_RQST_CH4_BY_MD,
	MT6337_PMIC_AUXADC_RQST_RSV1,
	MT6337_PMIC_AUXADC_RQST1_SET,
	MT6337_PMIC_AUXADC_RQST1_CLR,
	MT6337_PMIC_AUXADC_CK_ON_EXTD,
	MT6337_PMIC_AUXADC_SRCLKEN_SRC_SEL,
	MT6337_PMIC_AUXADC_ADC_PWDB,
	MT6337_PMIC_AUXADC_ADC_PWDB_SWCTRL,
	MT6337_PMIC_AUXADC_STRUP_CK_ON_ENB,
	MT6337_PMIC_AUXADC_ADC_RDY_WAKEUP_CLR,
	MT6337_PMIC_AUXADC_SRCLKEN_CK_EN,
	MT6337_PMIC_AUXADC_CK_AON_GPS,
	MT6337_PMIC_AUXADC_CK_AON_MD,
	MT6337_PMIC_AUXADC_CK_AON,
	MT6337_PMIC_AUXADC_CON0_SET,
	MT6337_PMIC_AUXADC_CON0_CLR,
	MT6337_PMIC_AUXADC_AVG_NUM_SMALL,
	MT6337_PMIC_AUXADC_AVG_NUM_LARGE,
	MT6337_PMIC_AUXADC_SPL_NUM,
	MT6337_PMIC_AUXADC_AVG_NUM_SEL,
	MT6337_PMIC_AUXADC_AVG_NUM_SEL_SHARE,
	MT6337_PMIC_AUXADC_SPL_NUM_LARGE,
	MT6337_PMIC_AUXADC_SPL_NUM_SLEEP,
	MT6337_PMIC_AUXADC_SPL_NUM_SLEEP_SEL,
	MT6337_PMIC_AUXADC_SPL_NUM_SEL,
	MT6337_PMIC_AUXADC_SPL_NUM_SEL_SHARE,
	MT6337_PMIC_AUXADC_TRIM_CH0_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH1_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH2_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH3_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH4_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH5_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH6_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH7_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH8_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH9_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH10_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH11_SEL,
	MT6337_PMIC_AUXADC_TRIM_CH12_SEL,
	MT6337_PMIC_AUXADC_ADC_2S_COMP_ENB,
	MT6337_PMIC_AUXADC_ADC_TRIM_COMP,
	MT6337_PMIC_AUXADC_SW_GAIN_TRIM,
	MT6337_PMIC_AUXADC_SW_OFFSET_TRIM,
	MT6337_PMIC_AUXADC_RNG_EN,
	MT6337_PMIC_AUXADC_DATA_REUSE_SEL,
	MT6337_PMIC_AUXADC_TEST_MODE,
	MT6337_PMIC_AUXADC_BIT_SEL,
	MT6337_PMIC_AUXADC_START_SW,
	MT6337_PMIC_AUXADC_START_SWCTRL,
	MT6337_PMIC_AUXADC_TS_VBE_SEL,
	MT6337_PMIC_AUXADC_TS_VBE_SEL_SWCTRL,
	MT6337_PMIC_AUXADC_VBUF_EN,
	MT6337_PMIC_AUXADC_VBUF_EN_SWCTRL,
	MT6337_PMIC_AUXADC_OUT_SEL,
	MT6337_PMIC_AUXADC_DA_DAC,
	MT6337_PMIC_AUXADC_DA_DAC_SWCTRL,
	MT6337_PMIC_AD_AUXADC_COMP,
	MT6337_PMIC_AUXADC_ADCIN_VSEN_EN,
	MT6337_PMIC_AUXADC_ADCIN_VBAT_EN,
	MT6337_PMIC_AUXADC_ADCIN_VSEN_MUX_EN,
	MT6337_PMIC_AUXADC_ADCIN_VSEN_EXT_BATON_EN,
	MT6337_PMIC_AUXADC_ADCIN_CHR_EN,
	MT6337_PMIC_AUXADC_ADCIN_BATON_TDET_EN,
	MT6337_PMIC_AUXADC_ACCDET_ANASWCTRL_EN,
	MT6337_PMIC_AUXADC_DIG0_RSV0,
	MT6337_PMIC_AUXADC_CHSEL,
	MT6337_PMIC_AUXADC_SWCTRL_EN,
	MT6337_PMIC_AUXADC_SOURCE_LBAT_SEL,
	MT6337_PMIC_AUXADC_SOURCE_LBAT2_SEL,
	MT6337_PMIC_AUXADC_DIG0_RSV2,
	MT6337_PMIC_AUXADC_DIG1_RSV2,
	MT6337_PMIC_AUXADC_DAC_EXTD,
	MT6337_PMIC_AUXADC_DAC_EXTD_EN,
	MT6337_PMIC_AUXADC_PMU_THR_PDN_SW,
	MT6337_PMIC_AUXADC_PMU_THR_PDN_SEL,
	MT6337_PMIC_AUXADC_PMU_THR_PDN_STATUS,
	MT6337_PMIC_AUXADC_DIG0_RSV1,
	MT6337_PMIC_AUXADC_START_SHADE_NUM,
	MT6337_PMIC_AUXADC_START_SHADE_EN,
	MT6337_PMIC_AUXADC_START_SHADE_SEL,
	MT6337_PMIC_AUXADC_AUTORPT_PRD,
	MT6337_PMIC_AUXADC_AUTORPT_EN,
	MT6337_PMIC_AUXADC_ACCDET_AUTO_SPL,
	MT6337_PMIC_AUXADC_ACCDET_AUTO_RQST_CLR,
	MT6337_PMIC_AUXADC_ACCDET_DIG1_RSV0,
	MT6337_PMIC_AUXADC_ACCDET_DIG0_RSV0,
	MT6337_PMIC_AUXADC_THR_DEBT_MAX,
	MT6337_PMIC_AUXADC_THR_DEBT_MIN,
	MT6337_PMIC_AUXADC_THR_DET_PRD_15_0,
	MT6337_PMIC_AUXADC_THR_DET_PRD_19_16,
	MT6337_PMIC_AUXADC_THR_VOLT_MAX,
	MT6337_PMIC_AUXADC_THR_IRQ_EN_MAX,
	MT6337_PMIC_AUXADC_THR_EN_MAX,
	MT6337_PMIC_AUXADC_THR_MAX_IRQ_B,
	MT6337_PMIC_AUXADC_THR_VOLT_MIN,
	MT6337_PMIC_AUXADC_THR_IRQ_EN_MIN,
	MT6337_PMIC_AUXADC_THR_EN_MIN,
	MT6337_PMIC_AUXADC_THR_MIN_IRQ_B,
	MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MAX,
	MT6337_PMIC_AUXADC_THR_DEBOUNCE_COUNT_MIN,
	MT6337_PMIC_EFUSE_GAIN_CH4_TRIM,
	MT6337_PMIC_EFUSE_OFFSET_CH4_TRIM,
	MT6337_PMIC_EFUSE_GAIN_CH0_TRIM,
	MT6337_PMIC_EFUSE_OFFSET_CH0_TRIM,
	MT6337_PMIC_EFUSE_GAIN_CH7_TRIM,
	MT6337_PMIC_EFUSE_OFFSET_CH7_TRIM,
	MT6337_PMIC_AUXADC_DBG_DIG0_RSV2,
	MT6337_PMIC_AUXADC_DBG_DIG1_RSV2,
	MT6337_PMIC_RG_AUXADC_CALI,
	MT6337_PMIC_RG_AUX_RSV,
	MT6337_PMIC_RG_VBUF_BYP,
	MT6337_PMIC_RG_VBUF_CALEN,
	MT6337_PMIC_RG_VBUF_EXTEN,
	MT6337_PMIC_RG_RNG_MOD,
	MT6337_PMIC_RG_RNG_ANA_EN,
	MT6337_PMIC_RG_RNG_SEL,
	MT6337_PMIC_AD_AUDACCDETCMPOC,
	MT6337_PMIC_RG_AUDACCDETANASWCTRLENB,
	MT6337_PMIC_RG_ACCDETSEL,
	MT6337_PMIC_RG_AUDACCDETSWCTRL,
	MT6337_PMIC_RG_AUDACCDETTVDET,
	MT6337_PMIC_AUDACCDETAUXADCSWCTRL,
	MT6337_PMIC_AUDACCDETAUXADCSWCTRL_SEL,
	MT6337_PMIC_RG_AUDACCDETRSV,
	MT6337_PMIC_ACCDET_SW_EN,
	MT6337_PMIC_ACCDET_SEQ_INIT,
	MT6337_PMIC_ACCDET_EINT_EN,
	MT6337_PMIC_ACCDET_EINT_SEQ_INIT,
	MT6337_PMIC_ACCDET_EINT1_EN,
	MT6337_PMIC_ACCDET_EINT1_SEQ_INIT,
	MT6337_PMIC_ACCDET_NEGVDET_EN,
	MT6337_PMIC_ACCDET_NEGVDET_EN_CTRL,
	MT6337_PMIC_ACCDET_ANASWCTRL_SEL,
	MT6337_PMIC_ACCDET_CMP_PWM_EN,
	MT6337_PMIC_ACCDET_VTH_PWM_EN,
	MT6337_PMIC_ACCDET_MBIAS_PWM_EN,
	MT6337_PMIC_ACCDET_CMP1_PWM_EN,
	MT6337_PMIC_ACCDET_EINT_PWM_EN,
	MT6337_PMIC_ACCDET_EINT1_PWM_EN,
	MT6337_PMIC_ACCDET_CMP_PWM_IDLE,
	MT6337_PMIC_ACCDET_VTH_PWM_IDLE,
	MT6337_PMIC_ACCDET_MBIAS_PWM_IDLE,
	MT6337_PMIC_ACCDET_CMP1_PWM_IDLE,
	MT6337_PMIC_ACCDET_EINT_PWM_IDLE,
	MT6337_PMIC_ACCDET_EINT1_PWM_IDLE,
	MT6337_PMIC_ACCDET_PWM_WIDTH,
	MT6337_PMIC_ACCDET_PWM_THRESH,
	MT6337_PMIC_ACCDET_RISE_DELAY,
	MT6337_PMIC_ACCDET_FALL_DELAY,
	MT6337_PMIC_ACCDET_DEBOUNCE0,
	MT6337_PMIC_ACCDET_DEBOUNCE1,
	MT6337_PMIC_ACCDET_DEBOUNCE2,
	MT6337_PMIC_ACCDET_DEBOUNCE3,
	MT6337_PMIC_ACCDET_DEBOUNCE4,
	MT6337_PMIC_ACCDET_DEBOUNCE5,
	MT6337_PMIC_ACCDET_DEBOUNCE6,
	MT6337_PMIC_ACCDET_DEBOUNCE7,
	MT6337_PMIC_ACCDET_DEBOUNCE8,
	MT6337_PMIC_ACCDET_IVAL_CUR_IN,
	MT6337_PMIC_ACCDET_IVAL_SAM_IN,
	MT6337_PMIC_ACCDET_IVAL_MEM_IN,
	MT6337_PMIC_ACCDET_IVAL_SEL,
	MT6337_PMIC_ACCDET_EINT1_IVAL_CUR_IN,
	MT6337_PMIC_ACCDET_EINT1_IVAL_SAM_IN,
	MT6337_PMIC_ACCDET_EINT1_IVAL_MEM_IN,
	MT6337_PMIC_ACCDET_EINT1_IVAL_SEL,
	MT6337_PMIC_ACCDET_EINT_IVAL_CUR_IN,
	MT6337_PMIC_ACCDET_EINT_IVAL_SAM_IN,
	MT6337_PMIC_ACCDET_EINT_IVAL_MEM_IN,
	MT6337_PMIC_ACCDET_EINT_IVAL_SEL,
	MT6337_PMIC_ACCDET_IRQ,
	MT6337_PMIC_ACCDET_NEGV_IRQ,
	MT6337_PMIC_ACCDET_EINT_IRQ,
	MT6337_PMIC_ACCDET_EINT1_IRQ,
	MT6337_PMIC_ACCDET_IRQ_CLR,
	MT6337_PMIC_ACCDET_NEGV_IRQ_CLR,
	MT6337_PMIC_ACCDET_EINT_IRQ_CLR,
	MT6337_PMIC_ACCDET_EINT1_IRQ_CLR,
	MT6337_PMIC_ACCDET_EINT_IRQ_POLARITY,
	MT6337_PMIC_ACCDET_EINT1_IRQ_POLARITY,
	MT6337_PMIC_ACCDET_TEST_MODE0,
	MT6337_PMIC_ACCDET_CMP_SWSEL,
	MT6337_PMIC_ACCDET_VTH_SWSEL,
	MT6337_PMIC_ACCDET_MBIAS_SWSEL,
	MT6337_PMIC_ACCDET_CMP1_SWSEL,
	MT6337_PMIC_ACCDET_PWM_SEL,
	MT6337_PMIC_ACCDET_CMP1_EN_SW,
	MT6337_PMIC_ACCDET_CMP_EN_SW,
	MT6337_PMIC_ACCDET_VTH_EN_SW,
	MT6337_PMIC_ACCDET_MBIAS_EN_SW,
	MT6337_PMIC_ACCDET_PWM_EN_SW,
	MT6337_PMIC_ACCDET_MBIAS_CLK,
	MT6337_PMIC_ACCDET_VTH_CLK,
	MT6337_PMIC_ACCDET_CMP_CLK,
	MT6337_PMIC_ACCDET2_CMP_CLK,
	MT6337_PMIC_DA_NI_AUDACCDETAUXADCSWCTRL,
	MT6337_PMIC_ACCDET_CMPC,
	MT6337_PMIC_ACCDET_IN,
	MT6337_PMIC_ACCDET_CUR_IN,
	MT6337_PMIC_ACCDET_SAM_IN,
	MT6337_PMIC_ACCDET_MEM_IN,
	MT6337_PMIC_ACCDET_STATE,
	MT6337_PMIC_ACCDET_EINT_DEB_SEL,
	MT6337_PMIC_ACCDET_EINT_DEBOUNCE,
	MT6337_PMIC_ACCDET_EINT_PWM_THRESH,
	MT6337_PMIC_ACCDET_EINT_PWM_WIDTH,
	MT6337_PMIC_ACCDET_EINT1_DEB_SEL,
	MT6337_PMIC_ACCDET_EINT1_DEBOUNCE,
	MT6337_PMIC_ACCDET_EINT1_PWM_THRESH,
	MT6337_PMIC_ACCDET_EINT1_PWM_WIDTH,
	MT6337_PMIC_ACCDET_NEGV_THRESH,
	MT6337_PMIC_ACCDET_EINT_PWM_FALL_DELAY,
	MT6337_PMIC_ACCDET_EINT_PWM_RISE_DELAY,
	MT6337_PMIC_ACCDET_EINT1_PWM_FALL_DELAY,
	MT6337_PMIC_ACCDET_EINT1_PWM_RISE_DELAY,
	MT6337_PMIC_ACCDET_NVDETECTOUT_SW,
	MT6337_PMIC_ACCDET_EINT_CMPOUT_SW,
	MT6337_PMIC_ACCDET_EINT1_CMPOUT_SW,
	MT6337_PMIC_ACCDET_AUXADC_CTRL_SW,
	MT6337_PMIC_ACCDET_EINT_CMP_EN_SW,
	MT6337_PMIC_ACCDET_EINT1_CMP_EN_SW,
	MT6337_PMIC_ACCDET_TEST_MODE13,
	MT6337_PMIC_ACCDET_TEST_MODE12,
	MT6337_PMIC_ACCDET_TEST_MODE11,
	MT6337_PMIC_ACCDET_TEST_MODE10,
	MT6337_PMIC_ACCDET_TEST_MODE9,
	MT6337_PMIC_ACCDET_TEST_MODE8,
	MT6337_PMIC_ACCDET_TEST_MODE7,
	MT6337_PMIC_ACCDET_TEST_MODE6,
	MT6337_PMIC_ACCDET_TEST_MODE5,
	MT6337_PMIC_ACCDET_TEST_MODE4,
	MT6337_PMIC_ACCDET_IN_SW,
	MT6337_PMIC_ACCDET_EINT_STATE,
	MT6337_PMIC_ACCDET_AUXADC_DEBOUNCE_END,
	MT6337_PMIC_ACCDET_AUXADC_CONNECT_PRE,
	MT6337_PMIC_ACCDET_EINT_CUR_IN,
	MT6337_PMIC_ACCDET_EINT_SAM_IN,
	MT6337_PMIC_ACCDET_EINT_MEM_IN,
	MT6337_PMIC_AD_NVDETECTOUT,
	MT6337_PMIC_AD_EINT1CMPOUT,
	MT6337_PMIC_DA_NI_EINT1CMPEN,
	MT6337_PMIC_ACCDET_EINT1_STATE,
	MT6337_PMIC_ACCDET_EINT1_CUR_IN,
	MT6337_PMIC_ACCDET_EINT1_SAM_IN,
	MT6337_PMIC_ACCDET_EINT1_MEM_IN,
	MT6337_PMIC_AD_EINT2CMPOUT,
	MT6337_PMIC_DA_NI_EINT2CMPEN,
	MT6337_PMIC_ACCDET_NEGV_COUNT_IN,
	MT6337_PMIC_ACCDET_NEGV_EN_FINAL,
	MT6337_PMIC_ACCDET_NEGV_COUNT_END,
	MT6337_PMIC_ACCDET_NEGV_MINU,
	MT6337_PMIC_ACCDET_NEGV_ADD,
	MT6337_PMIC_ACCDET_NEGV_CMP,
	MT6337_PMIC_ACCDET_CUR_DEB,
	MT6337_PMIC_ACCDET_EINT_CUR_DEB,
	MT6337_PMIC_ACCDET_EINT1_CUR_DEB,
	MT6337_PMIC_ACCDET_RSV_CON0,
	MT6337_PMIC_ACCDET_RSV_CON1,
	MT6337_PMIC_ACCDET_AUXADC_CONNECT_TIME,
	MT6337_PMIC_ACCDET_HWEN_SEL,
	MT6337_PMIC_ACCDET_EINT_REVERSE,
	MT6337_PMIC_ACCDET_EINT1_REVERSE,
	MT6337_PMIC_ACCDET_HWMODE_SEL,
	MT6337_PMIC_ACCDET_EN_CMPC,
	MT6337_PMIC_RG_AUDDACLPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDDACRPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUD_DAC_PWR_UP_VA32,
	MT6337_PMIC_RG_AUD_DAC_PWL_UP_VA32,
	MT6337_PMIC_RG_AUDHPLPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDHPRPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDHPLPWRUP_IBIAS_VAUDP32,
	MT6337_PMIC_RG_AUDHPRPWRUP_IBIAS_VAUDP32,
	MT6337_PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPLSCDISABLE_VAUDP32,
	MT6337_PMIC_RG_AUDHPRSCDISABLE_VAUDP32,
	MT6337_PMIC_RG_AUDHPLBSCCURRENT_VAUDP32,
	MT6337_PMIC_RG_AUDHPRBSCCURRENT_VAUDP32,
	MT6337_PMIC_RG_AUDHPLOUTPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDHPROUTPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDHPLOUTAUXPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDHPROUTAUXPWRUP_VAUDP32,
	MT6337_PMIC_RG_HPLAUXFBRSW_EN_VAUDP32,
	MT6337_PMIC_RG_HPRAUXFBRSW_EN_VAUDP32,
	MT6337_PMIC_RG_HPLSHORT2HPLAUX_EN_VAUDP32,
	MT6337_PMIC_RG_HPRSHORT2HPRAUX_EN_VAUDP32,
	MT6337_PMIC_RG_HPPOUTSTGCTRL_VAUDP32,
	MT6337_PMIC_RG_HPNOUTSTGCTRL_VAUDP32,
	MT6337_PMIC_RG_HPPOUTPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_HPNOUTPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_AUDHPSTARTUP_VAUDP32,
	MT6337_PMIC_RG_AUDREFN_DERES_EN_VAUDP32,
	MT6337_PMIC_RG_HPPSHORT2VCM_VAUDP32,
	MT6337_PMIC_RG_AUDHPTRIM_EN_VAUDP32,
	MT6337_PMIC_RG_AUDHPLTRIM_VAUDP32,
	MT6337_PMIC_RG_AUDHPLFINETRIM_VAUDP32,
	MT6337_PMIC_RG_AUDHPRTRIM_VAUDP32,
	MT6337_PMIC_RG_AUDHPRFINETRIM_VAUDP32,
	MT6337_PMIC_RG_HPINPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_HPINPUTRESET0_VAUDP32,
	MT6337_PMIC_RG_HPOUTPUTRESET0_VAUDP32,
	MT6337_PMIC_RG_AUDHPCOMP_EN_VAUDP32,
	MT6337_PMIC_RG_AUDHPDIFFINPBIASADJ_VAUDP32,
	MT6337_PMIC_RG_AUDHPLFCOMPRESSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPHFCOMPRESSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPHFCOMPBUFGAINSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPDECMGAINADJ_VAUDP32,
	MT6337_PMIC_RG_AUDHPDEDMGAINADJ_VAUDP32,
	MT6337_PMIC_RG_AUDHSPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDHSPWRUP_IBIAS_VAUDP32,
	MT6337_PMIC_RG_AUDHSMUXINPUTSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHSSCDISABLE_VAUDP32,
	MT6337_PMIC_RG_AUDHSBSCCURRENT_VAUDP32,
	MT6337_PMIC_RG_AUDHSSTARTUP_VAUDP32,
	MT6337_PMIC_RG_HSOUTPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_HSINPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_HSINPUTRESET0_VAUDP32,
	MT6337_PMIC_RG_HSOUTPUTRESET0_VAUDP32,
	MT6337_PMIC_RG_HSOUT_SHORTVCM_VAUDP32,
	MT6337_PMIC_RG_AUDLOLPWRUP_VAUDP32,
	MT6337_PMIC_RG_AUDLOLPWRUP_IBIAS_VAUDP32,
	MT6337_PMIC_RG_AUDLOLMUXINPUTSEL_VAUDP32,
	MT6337_PMIC_RG_AUDLOLSCDISABLE_VAUDP32,
	MT6337_PMIC_RG_AUDLOLBSCCURRENT_VAUDP32,
	MT6337_PMIC_RG_AUDLOSTARTUP_VAUDP32,
	MT6337_PMIC_RG_LOINPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_LOOUTPUTSTBENH_VAUDP32,
	MT6337_PMIC_RG_LOINPUTRESET0_VAUDP32,
	MT6337_PMIC_RG_LOOUTPUTRESET0_VAUDP32,
	MT6337_PMIC_RG_LOOUT_SHORTVCM_VAUDP32,
	MT6337_PMIC_RG_AUDTRIMBUF_EN_VAUDP32,
	MT6337_PMIC_RG_AUDTRIMBUF_INPUTMUXSEL_VAUDP32,
	MT6337_PMIC_RG_AUDTRIMBUF_GAINSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPSPKDET_EN_VAUDP32,
	MT6337_PMIC_RG_AUDHPSPKDET_INPUTMUXSEL_VAUDP32,
	MT6337_PMIC_RG_AUDHPSPKDET_OUTPUTMUXSEL_VAUDP32,
	MT6337_PMIC_RG_ABIDEC_RSVD0_VA32,
	MT6337_PMIC_RG_ABIDEC_RSVD0_VAUDP32,
	MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP32,
	MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP32,
	MT6337_PMIC_RG_AUDBIASADJ_0_VAUDP32,
	MT6337_PMIC_RG_AUDBIASADJ_1_VAUDP32,
	MT6337_PMIC_RG_AUDIBIASPWRDN_VAUDP32,
	MT6337_PMIC_RG_RSTB_DECODER_VA32,
	MT6337_PMIC_RG_SEL_DECODER_96K_VA32,
	MT6337_PMIC_RG_SEL_DELAY_VCORE,
	MT6337_PMIC_RG_AUDGLB_PWRDN_VA32,
	MT6337_PMIC_RG_AUDGLB_LP_VOW_EN_VA32,
	MT6337_PMIC_RG_AUDGLB_LP2_VOW_EN_VA32,
	MT6337_PMIC_RG_AUDGLB_NVREG_L_VA32,
	MT6337_PMIC_RG_AUDGLB_NVREG_R_VA32,
	MT6337_PMIC_RG_LCLDO_DECL_EN_VA32,
	MT6337_PMIC_RG_LCLDO_DECL_PDDIS_EN_VA18,
	MT6337_PMIC_RG_LCLDO_DECL_REMOTE_SENSE_VA18,
	MT6337_PMIC_RG_LCLDO_DECR_EN_VA32,
	MT6337_PMIC_RG_LCLDO_DECR_PDDIS_EN_VA18,
	MT6337_PMIC_RG_LCLDO_DECR_REMOTE_SENSE_VA18,
	MT6337_PMIC_RG_AUDPMU_RSVD_VA18,
	MT6337_PMIC_RG_NVREGL_EN_VAUDP32,
	MT6337_PMIC_RG_NVREGR_EN_VAUDP32,
	MT6337_PMIC_RG_NVREGL_PULL0V_VAUDP32,
	MT6337_PMIC_RG_NVREGR_PULL0V_VAUDP32,
	MT6337_PMIC_RG_AUDPREAMPCH0_01ON,
	MT6337_PMIC_RG_AUDPREAMPCH0_01DCCEN,
	MT6337_PMIC_RG_AUDPREAMPCH0_01DCPRECHARGE,
	MT6337_PMIC_RG_AUDPREAMPCH0_01HPMODEEN,
	MT6337_PMIC_RG_AUDPREAMPCH0_01INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH0_01VSCALE,
	MT6337_PMIC_RG_AUDPREAMPCH0_01GAIN,
	MT6337_PMIC_RG_AUDPREAMPCH0_01PGATEST,
	MT6337_PMIC_RG_AUDADCCH0_01PWRUP,
	MT6337_PMIC_RG_AUDADCCH0_01INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH1_23ON,
	MT6337_PMIC_RG_AUDPREAMPCH1_23DCCEN,
	MT6337_PMIC_RG_AUDPREAMPCH1_23DCPRECHARGE,
	MT6337_PMIC_RG_AUDPREAMPCH1_23HPMODEEN,
	MT6337_PMIC_RG_AUDPREAMPCH1_23INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH1_23VSCALE,
	MT6337_PMIC_RG_AUDPREAMPCH1_23GAIN,
	MT6337_PMIC_RG_AUDPREAMPCH1_23PGATEST,
	MT6337_PMIC_RG_AUDADCCH1_23PWRUP,
	MT6337_PMIC_RG_AUDADCCH1_23INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH2_45ON,
	MT6337_PMIC_RG_AUDPREAMPCH2_45DCCEN,
	MT6337_PMIC_RG_AUDPREAMPCH2_45DCPRECHARGE,
	MT6337_PMIC_RG_AUDPREAMPCH2_45HPMODEEN,
	MT6337_PMIC_RG_AUDPREAMPCH2_45INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH2_45VSCALE,
	MT6337_PMIC_RG_AUDPREAMPCH2_45GAIN,
	MT6337_PMIC_RG_AUDPREAMPCH2_45PGATEST,
	MT6337_PMIC_RG_AUDADCCH2_45PWRUP,
	MT6337_PMIC_RG_AUDADCCH2_45INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH3_6ON,
	MT6337_PMIC_RG_AUDPREAMPCH3_6DCCEN,
	MT6337_PMIC_RG_AUDPREAMPCH3_6DCPRECHARGE,
	MT6337_PMIC_RG_AUDPREAMPCH3_6HPMODEEN,
	MT6337_PMIC_RG_AUDPREAMPCH3_6INPUTSEL,
	MT6337_PMIC_RG_AUDPREAMPCH3_6VSCALE,
	MT6337_PMIC_RG_AUDPREAMPCH3_6GAIN,
	MT6337_PMIC_RG_AUDPREAMPCH3_6PGATEST,
	MT6337_PMIC_RG_AUDADCCH3_6PWRUP,
	MT6337_PMIC_RG_AUDADCCH3_6INPUTSEL,
	MT6337_PMIC_RG_AUDULHALFBIAS_CH01,
	MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH01,
	MT6337_PMIC_RG_AUDPREAMPLPEN_CH01,
	MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH01,
	MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH01,
	MT6337_PMIC_RG_AUDADCFLASHLPEN_CH01,
	MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH01,
	MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH01,
	MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH01,
	MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH01,
	MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH01,
	MT6337_PMIC_RG_AUDULHALFBIAS_CH23,
	MT6337_PMIC_RG_AUDGLBVOWLPWEN_CH23,
	MT6337_PMIC_RG_AUDPREAMPLPEN_CH23,
	MT6337_PMIC_RG_AUDADC1STSTAGELPEN_CH23,
	MT6337_PMIC_RG_AUDADC2NDSTAGELPEN_CH23,
	MT6337_PMIC_RG_AUDADCFLASHLPEN_CH23,
	MT6337_PMIC_RG_AUDPREAMPIDDTEST_CH23,
	MT6337_PMIC_RG_AUDADC1STSTAGEIDDTEST_CH23,
	MT6337_PMIC_RG_AUDADC2NDSTAGEIDDTEST_CH23,
	MT6337_PMIC_RG_AUDADCREFBUFIDDTEST_CH23,
	MT6337_PMIC_RG_AUDADCFLASHIDDTEST_CH23,
	MT6337_PMIC_RG_AUDADCDAC0P25FS_CH01,
	MT6337_PMIC_RG_AUDADCCLKSEL_CH01,
	MT6337_PMIC_RG_AUDADCCLKSOURCE_CH01,
	MT6337_PMIC_RG_AUDADCCLKGENMODE_CH01,
	MT6337_PMIC_RG_AUDADCCLKRSTB_CH01,
	MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH01,
	MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH01,
	MT6337_PMIC_RG_AUDPREAMPAAFEN_CH01,
	MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH01,
	MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH01,
	MT6337_PMIC_RG_AUDSPAREPGA_CH01,
	MT6337_PMIC_RG_AUDADCDAC0P25FS_CH23,
	MT6337_PMIC_RG_AUDADCCLKSEL_CH23,
	MT6337_PMIC_RG_AUDADCCLKSOURCE_CH23,
	MT6337_PMIC_RG_AUDADCCLKGENMODE_CH23,
	MT6337_PMIC_RG_AUDADCCLKRSTB_CH23,
	MT6337_PMIC_RG_AUDPREAMPCASCADEEN_CH23,
	MT6337_PMIC_RG_AUDPREAMPFBCAPEN_CH23,
	MT6337_PMIC_RG_AUDPREAMPAAFEN_CH23,
	MT6337_PMIC_RG_DCCVCMBUFLPMODSEL_CH23,
	MT6337_PMIC_RG_DCCVCMBUFLPSWEN_CH23,
	MT6337_PMIC_RG_AUDSPAREPGA_CH23,
	MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH01,
	MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH01,
	MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH01,
	MT6337_PMIC_RG_AUDADCFSRESET_CH01,
	MT6337_PMIC_RG_AUDADCWIDECM_CH01,
	MT6337_PMIC_RG_AUDADCNOPATEST_CH01,
	MT6337_PMIC_RG_AUDADCBYPASS_CH01,
	MT6337_PMIC_RG_AUDADCFFBYPASS_CH01,
	MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH01,
	MT6337_PMIC_RG_AUDADCDACIDDTEST_CH01,
	MT6337_PMIC_RG_AUDADCDACNRZ_CH01,
	MT6337_PMIC_RG_AUDADCNODEM_CH01,
	MT6337_PMIC_RG_AUDADCDACTEST_CH01,
	MT6337_PMIC_RG_AUDADC1STSTAGESDENB_CH23,
	MT6337_PMIC_RG_AUDADC2NDSTAGERESET_CH23,
	MT6337_PMIC_RG_AUDADC3RDSTAGERESET_CH23,
	MT6337_PMIC_RG_AUDADCFSRESET_CH23,
	MT6337_PMIC_RG_AUDADCWIDECM_CH23,
	MT6337_PMIC_RG_AUDADCNOPATEST_CH23,
	MT6337_PMIC_RG_AUDADCBYPASS_CH23,
	MT6337_PMIC_RG_AUDADCFFBYPASS_CH23,
	MT6337_PMIC_RG_AUDADCDACFBCURRENT_CH23,
	MT6337_PMIC_RG_AUDADCDACIDDTEST_CH23,
	MT6337_PMIC_RG_AUDADCDACNRZ_CH23,
	MT6337_PMIC_RG_AUDADCNODEM_CH23,
	MT6337_PMIC_RG_AUDADCDACTEST_CH23,
	MT6337_PMIC_RG_AUDRCTUNECH0_01,
	MT6337_PMIC_RG_AUDRCTUNECH0_01SEL,
	MT6337_PMIC_RG_AUDRCTUNECH1_23,
	MT6337_PMIC_RG_AUDRCTUNECH1_23SEL,
	MT6337_PMIC_RG_AUDRCTUNECH2_45,
	MT6337_PMIC_RG_AUDRCTUNECH2_45SEL,
	MT6337_PMIC_RG_AUDRCTUNECH3_6,
	MT6337_PMIC_RG_AUDRCTUNECH3_6SEL,
	MT6337_PMIC_RG_AUDSPAREVA30,
	MT6337_PMIC_RG_AUDSPAREVA18,
	MT6337_PMIC_RG_AUDDIGMIC0EN,
	MT6337_PMIC_RG_AUDDIGMIC0BIAS,
	MT6337_PMIC_RG_DMIC0HPCLKEN,
	MT6337_PMIC_RG_AUDDIGMIC0PDUTY,
	MT6337_PMIC_RG_AUDDIGMIC0NDUTY,
	MT6337_PMIC_RG_AUDDIGMIC1EN,
	MT6337_PMIC_RG_AUDDIGMIC1BIAS,
	MT6337_PMIC_RG_DMIC1HPCLKEN,
	MT6337_PMIC_RG_AUDDIGMIC1PDUTY,
	MT6337_PMIC_RG_AUDDIGMIC1NDUTY,
	MT6337_PMIC_RG_DMIC1MONEN,
	MT6337_PMIC_RG_DMIC1MONSEL,
	MT6337_PMIC_RG_AUDDIGMIC2EN,
	MT6337_PMIC_RG_AUDDIGMIC2BIAS,
	MT6337_PMIC_RG_DMIC2HPCLKEN,
	MT6337_PMIC_RG_AUDDIGMIC2PDUTY,
	MT6337_PMIC_RG_AUDDIGMIC2NDUTY,
	MT6337_PMIC_RG_DMIC2MONEN,
	MT6337_PMIC_RG_DMIC2MONSEL,
	MT6337_PMIC_RG_DMIC2RDATASEL,
	MT6337_PMIC_RG_AUDPWDBMICBIAS0,
	MT6337_PMIC_RG_AUDMICBIAS0DCSW0P1EN,
	MT6337_PMIC_RG_AUDMICBIAS0DCSW0P2EN,
	MT6337_PMIC_RG_AUDMICBIAS0DCSW0NEN,
	MT6337_PMIC_RG_AUDMICBIAS0VREF,
	MT6337_PMIC_RG_AUDMICBIAS0LOWPEN,
	MT6337_PMIC_RG_AUDPWDBMICBIAS2,
	MT6337_PMIC_RG_AUDMICBIAS2DCSW2P1EN,
	MT6337_PMIC_RG_AUDMICBIAS2DCSW2P2EN,
	MT6337_PMIC_RG_AUDMICBIAS2DCSW2NEN,
	MT6337_PMIC_RG_AUDMICBIAS2VREF,
	MT6337_PMIC_RG_AUDMICBIAS2LOWPEN,
	MT6337_PMIC_RG_AUDPWDBMICBIAS1,
	MT6337_PMIC_RG_AUDMICBIAS1DCSW1PEN,
	MT6337_PMIC_RG_AUDMICBIAS1DCSW1NEN,
	MT6337_PMIC_RG_AUDMICBIAS1VREF,
	MT6337_PMIC_RG_AUDMICBIAS1LOWPEN,
	MT6337_PMIC_RG_AUDMICBIAS1DCSW3PEN,
	MT6337_PMIC_RG_AUDMICBIAS1DCSW3NEN,
	MT6337_PMIC_RG_AUDMICBIAS2DCSW3PEN,
	MT6337_PMIC_RG_AUDMICBIAS2DCSW3NEN,
	MT6337_PMIC_RG_AUDMICBIAS1HVEN,
	MT6337_PMIC_RG_AUDMICBIAS1HVVREF,
	MT6337_PMIC_RG_BANDGAPGEN,
	MT6337_PMIC_RG_AUDPWDBMICBIAS3,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW4P1EN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW4P2EN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW4NEN,
	MT6337_PMIC_RG_AUDMICBIAS3VREF,
	MT6337_PMIC_RG_AUDMICBIAS3LOWPEN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW5P1EN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW5P2EN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW5NEN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW6P1EN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW6P2EN,
	MT6337_PMIC_RG_AUDMICBIAS3DCSW6NEN,
	MT6337_PMIC_RG_AUDACCDETMICBIAS0PULLLOW,
	MT6337_PMIC_RG_AUDACCDETMICBIAS1PULLLOW,
	MT6337_PMIC_RG_AUDACCDETMICBIAS2PULLLOW,
	MT6337_PMIC_RG_AUDACCDETMICBIAS3PULLLOW,
	MT6337_PMIC_RG_AUDACCDETVIN1PULLLOW,
	MT6337_PMIC_RG_AUDACCDETVTHACAL,
	MT6337_PMIC_RG_AUDACCDETVTHBCAL,
	MT6337_PMIC_RG_ACCDET1SEL,
	MT6337_PMIC_RG_ACCDET2SEL,
	MT6337_PMIC_RG_SWBUFMODSEL,
	MT6337_PMIC_RG_SWBUFSWEN,
	MT6337_PMIC_RG_EINTCOMPVTH,
	MT6337_PMIC_RG_EINT1CONFIGACCDET1,
	MT6337_PMIC_RG_EINT2CONFIGACCDET2,
	MT6337_PMIC_RG_ACCDETSPARE,
	MT6337_PMIC_RG_AUDENCSPAREVA30,
	MT6337_PMIC_RG_AUDENCSPAREVA18,
	MT6337_PMIC_RG_PLL_EN,
	MT6337_PMIC_RG_PLLBS_RST,
	MT6337_PMIC_RG_PLL_DCKO_SEL,
	MT6337_PMIC_RG_PLL_DIV1,
	MT6337_PMIC_RG_PLL_RLATCH_EN,
	MT6337_PMIC_RG_PLL_PDIV1_EN,
	MT6337_PMIC_RG_PLL_PDIV1,
	MT6337_PMIC_RG_PLL_BC,
	MT6337_PMIC_RG_PLL_BP,
	MT6337_PMIC_RG_PLL_BR,
	MT6337_PMIC_RG_CKO_SEL,
	MT6337_PMIC_RG_PLL_IBSEL,
	MT6337_PMIC_RG_PLL_CKT_SEL,
	MT6337_PMIC_RG_PLL_VCT_EN,
	MT6337_PMIC_RG_PLL_CKT_EN,
	MT6337_PMIC_RG_PLL_HPM_EN,
	MT6337_PMIC_RG_PLL_DCHP_EN,
	MT6337_PMIC_RG_PLL_CDIV,
	MT6337_PMIC_RG_VCOBAND,
	MT6337_PMIC_RG_CKDRV_EN,
	MT6337_PMIC_RG_PLL_DCHP_AEN,
	MT6337_PMIC_RG_PLL_RSVA,
	MT6337_PMIC_RGS_AUDRCTUNECH0_01READ,
	MT6337_PMIC_RGS_AUDRCTUNECH1_23READ,
	MT6337_PMIC_RGS_AUDRCTUNECH2_45READ,
	MT6337_PMIC_RGS_AUDRCTUNECH3_6READ,
	MT6337_PMIC_RG_AUDZCDENABLE,
	MT6337_PMIC_RG_AUDZCDGAINSTEPTIME,
	MT6337_PMIC_RG_AUDZCDGAINSTEPSIZE,
	MT6337_PMIC_RG_AUDZCDTIMEOUTMODESEL,
	MT6337_PMIC_RG_AUDZCDCLKSEL_VAUDP15,
	MT6337_PMIC_RG_AUDZCDMUXSEL_VAUDP15,
	MT6337_PMIC_RG_AUDLOLGAIN,
	MT6337_PMIC_RG_AUDLORGAIN,
	MT6337_PMIC_RG_AUDHPLGAIN,
	MT6337_PMIC_RG_AUDHPRGAIN,
	MT6337_PMIC_RG_AUDHSGAIN,
	MT6337_PMIC_RG_AUDIVLGAIN,
	MT6337_PMIC_RG_AUDIVRGAIN,
	MT6337_PMIC_RG_AUDINTGAIN1,
	MT6337_PMIC_RG_AUDINTGAIN2,
	MT6337_PMIC_GPIO_DIR0,
	MT6337_PMIC_GPIO_DIR0_SET,
	MT6337_PMIC_GPIO_DIR0_CLR,
	MT6337_PMIC_GPIO_PULLEN0,
	MT6337_PMIC_GPIO_PULLEN0_SET,
	MT6337_PMIC_GPIO_PULLEN0_CLR,
	MT6337_PMIC_GPIO_PULLSEL0,
	MT6337_PMIC_GPIO_PULLSEL0_SET,
	MT6337_PMIC_GPIO_PULLSEL0_CLR,
	MT6337_PMIC_GPIO_DINV0,
	MT6337_PMIC_GPIO_DINV0_SET,
	MT6337_PMIC_GPIO_DINV0_CLR,
	MT6337_PMIC_GPIO_DOUT0,
	MT6337_PMIC_GPIO_DOUT0_SET,
	MT6337_PMIC_GPIO_DOUT0_CLR,
	MT6337_PMIC_GPIO_PI0,
	MT6337_PMIC_GPIO_POE0,
	MT6337_PMIC_GPIO0_MODE,
	MT6337_PMIC_GPIO1_MODE,
	MT6337_PMIC_GPIO2_MODE,
	MT6337_PMIC_GPIO3_MODE,
	MT6337_PMIC_GPIO4_MODE,
	MT6337_PMIC_GPIO_MODE0_SET,
	MT6337_PMIC_GPIO_MODE0_CLR,
	MT6337_PMIC_GPIO5_MODE,
	MT6337_PMIC_GPIO6_MODE,
	MT6337_PMIC_GPIO7_MODE,
	MT6337_PMIC_GPIO8_MODE,
	MT6337_PMIC_GPIO9_MODE,
	MT6337_PMIC_GPIO_MODE1_SET,
	MT6337_PMIC_GPIO_MODE1_CLR,
	MT6337_PMIC_GPIO10_MODE,
	MT6337_PMIC_GPIO_MODE2_SET,
	MT6337_PMIC_GPIO_MODE2_CLR,
	MT6337_PMIC_RG_RSV_CON0,
	MT6337_PMU_COMMAND_MAX
} MT6337_PMU_FLAGS_LIST_ENUM;

typedef struct {
	MT6337_PMU_FLAGS_LIST_ENUM flagname;
	unsigned short offset;
	unsigned short mask;
	unsigned char shift;
} MT6337_PMU_FLAG_TABLE_ENTRY;

#endif				/* _MT_PMIC_6337_UPMU_HW_H_ */

/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LINUX_MT6311_I2C_H
#define __LINUX_MT6311_I2C_H

#include <linux/device.h>

#define IPIMB_MT6311

#define MT6311_DEBUG 1

#if MT6311_DEBUG
#define MT6311TAG		"[MT6311] "
#define MT6311LOG(fmt, arg...)   pr_notice(MT6311TAG fmt, ##arg)
#else
#define MT6311LOG(...) do { } while (0)
#endif /* MT6311_DEBUG */

#define MT6311_CID_CODE		(0x01)

extern int mt6311_read_byte(unsigned char addr, unsigned char *data);
extern int mt6311_write_byte(unsigned char addr, unsigned char data);
extern int mt6311_assign_bit(unsigned char reg, unsigned char mask,
			     unsigned char data);
extern int mt6311_config_interface(unsigned char reg, unsigned char data,
	unsigned char mask, unsigned char shift);
extern int mt6311_read_interface(unsigned char reg, unsigned char *data,
	unsigned char mask, unsigned char shift);
extern int mt6311_regulator_init(struct device *dev);
extern int mt6311_regulator_deinit(void);

extern int is_mt6311_sw_ready(void);
extern int is_mt6311_exist(void);
extern int mt6311_vdvfs11_set_mode(unsigned char mode);

/*---------------------- AUTO GEN ---------------------------*/
#define PMIC_REG_BASE (0x00000000)

#define MT6311_CID                ((unsigned int)(PMIC_REG_BASE+0x00))
#define MT6311_SWCID              ((unsigned int)(PMIC_REG_BASE+0x01))
#define MT6311_HWCID              ((unsigned int)(PMIC_REG_BASE+0x02))
#define MT6311_GPIO_CFG           ((unsigned int)(PMIC_REG_BASE+0x03))
#define MT6311_GPIO_MODE          ((unsigned int)(PMIC_REG_BASE+0x04))
#define MT6311_TEST_OUT           ((unsigned int)(PMIC_REG_BASE+0x05))
#define MT6311_TEST_CON0          ((unsigned int)(PMIC_REG_BASE+0x06))
#define MT6311_TEST_CON1          ((unsigned int)(PMIC_REG_BASE+0x07))
#define MT6311_TEST_CON2          ((unsigned int)(PMIC_REG_BASE+0x08))
#define MT6311_TEST_CON3          ((unsigned int)(PMIC_REG_BASE+0x09))
#define MT6311_TOP_CON            ((unsigned int)(PMIC_REG_BASE+0x0A))
#define MT6311_TOP_CKTST_CON      ((unsigned int)(PMIC_REG_BASE+0x0B))
#define MT6311_TOP_CKPDN_CON1     ((unsigned int)(PMIC_REG_BASE+0x0C))
#define MT6311_TOP_CKPDN_CON1_SET ((unsigned int)(PMIC_REG_BASE+0x0D))
#define MT6311_TOP_CKPDN_CON1_CLR ((unsigned int)(PMIC_REG_BASE+0x0E))
#define MT6311_TOP_CKPDN_CON2     ((unsigned int)(PMIC_REG_BASE+0x0F))
#define MT6311_TOP_CKPDN_CON2_SET ((unsigned int)(PMIC_REG_BASE+0x10))
#define MT6311_TOP_CKPDN_CON2_CLR ((unsigned int)(PMIC_REG_BASE+0x11))
#define MT6311_TOP_CKHWEN_CON     ((unsigned int)(PMIC_REG_BASE+0x12))
#define MT6311_TOP_CKHWEN_CON_SET ((unsigned int)(PMIC_REG_BASE+0x13))
#define MT6311_TOP_CKHWEN_CON_CLR ((unsigned int)(PMIC_REG_BASE+0x14))
#define MT6311_TOP_RST_CON        ((unsigned int)(PMIC_REG_BASE+0x15))
#define MT6311_TOP_RST_CON_SET    ((unsigned int)(PMIC_REG_BASE+0x16))
#define MT6311_TOP_RST_CON_CLR    ((unsigned int)(PMIC_REG_BASE+0x17))
#define MT6311_TOP_INT_CON        ((unsigned int)(PMIC_REG_BASE+0x18))
#define MT6311_TOP_INT_MON        ((unsigned int)(PMIC_REG_BASE+0x19))
#define MT6311_STRUP_CON0         ((unsigned int)(PMIC_REG_BASE+0x1A))
#define MT6311_STRUP_CON1         ((unsigned int)(PMIC_REG_BASE+0x1B))
#define MT6311_STRUP_CON2         ((unsigned int)(PMIC_REG_BASE+0x1C))
#define MT6311_STRUP_CON3         ((unsigned int)(PMIC_REG_BASE+0x1D))
#define MT6311_STRUP_CON4         ((unsigned int)(PMIC_REG_BASE+0x1E))
#define MT6311_STRUP_CON5         ((unsigned int)(PMIC_REG_BASE+0x1F))
#define MT6311_STRUP_CON6         ((unsigned int)(PMIC_REG_BASE+0x20))
#define MT6311_STRUP_CON7         ((unsigned int)(PMIC_REG_BASE+0x21))
#define MT6311_STRUP_CON8         ((unsigned int)(PMIC_REG_BASE+0x22))
#define MT6311_STRUP_CON9         ((unsigned int)(PMIC_REG_BASE+0x23))
#define MT6311_STRUP_CON10        ((unsigned int)(PMIC_REG_BASE+0x24))
#define MT6311_STRUP_CON11        ((unsigned int)(PMIC_REG_BASE+0x25))
#define MT6311_STRUP_CON12        ((unsigned int)(PMIC_REG_BASE+0x26))
#define MT6311_STRUP_CON13        ((unsigned int)(PMIC_REG_BASE+0x27))
#define MT6311_STRUP_CON14        ((unsigned int)(PMIC_REG_BASE+0x28))
#define MT6311_TOP_CLK_TRIM0      ((unsigned int)(PMIC_REG_BASE+0x29))
#define MT6311_TOP_CLK_TRIM1      ((unsigned int)(PMIC_REG_BASE+0x2A))
#define MT6311_EFUSE_CON0         ((unsigned int)(PMIC_REG_BASE+0x2B))
#define MT6311_EFUSE_CON1         ((unsigned int)(PMIC_REG_BASE+0x2C))
#define MT6311_EFUSE_CON2         ((unsigned int)(PMIC_REG_BASE+0x2D))
#define MT6311_EFUSE_CON3         ((unsigned int)(PMIC_REG_BASE+0x2E))
#define MT6311_EFUSE_CON4         ((unsigned int)(PMIC_REG_BASE+0x2F))
#define MT6311_EFUSE_CON5         ((unsigned int)(PMIC_REG_BASE+0x30))
#define MT6311_EFUSE_CON6         ((unsigned int)(PMIC_REG_BASE+0x31))
#define MT6311_EFUSE_CON7         ((unsigned int)(PMIC_REG_BASE+0x32))
#define MT6311_EFUSE_CON8         ((unsigned int)(PMIC_REG_BASE+0x33))
#define MT6311_EFUSE_CON9         ((unsigned int)(PMIC_REG_BASE+0x34))
#define MT6311_EFUSE_CON10        ((unsigned int)(PMIC_REG_BASE+0x35))
#define MT6311_EFUSE_CON11        ((unsigned int)(PMIC_REG_BASE+0x36))
#define MT6311_EFUSE_CON12        ((unsigned int)(PMIC_REG_BASE+0x37))
#define MT6311_EFUSE_CON13        ((unsigned int)(PMIC_REG_BASE+0x38))
#define MT6311_EFUSE_DOUT_0_7     ((unsigned int)(PMIC_REG_BASE+0x39))
#define MT6311_EFUSE_DOUT_8_15    ((unsigned int)(PMIC_REG_BASE+0x3A))
#define MT6311_EFUSE_DOUT_16_23   ((unsigned int)(PMIC_REG_BASE+0x3B))
#define MT6311_EFUSE_DOUT_24_31   ((unsigned int)(PMIC_REG_BASE+0x3C))
#define MT6311_EFUSE_DOUT_32_39   ((unsigned int)(PMIC_REG_BASE+0x3D))
#define MT6311_EFUSE_DOUT_40_47   ((unsigned int)(PMIC_REG_BASE+0x3E))
#define MT6311_EFUSE_DOUT_48_55   ((unsigned int)(PMIC_REG_BASE+0x3F))
#define MT6311_EFUSE_DOUT_56_63   ((unsigned int)(PMIC_REG_BASE+0x40))
#define MT6311_EFUSE_DOUT_64_71   ((unsigned int)(PMIC_REG_BASE+0x41))
#define MT6311_EFUSE_DOUT_72_79   ((unsigned int)(PMIC_REG_BASE+0x42))
#define MT6311_EFUSE_DOUT_80_87   ((unsigned int)(PMIC_REG_BASE+0x43))
#define MT6311_EFUSE_DOUT_88_95   ((unsigned int)(PMIC_REG_BASE+0x44))
#define MT6311_EFUSE_DOUT_96_103  ((unsigned int)(PMIC_REG_BASE+0x45))
#define MT6311_EFUSE_DOUT_104_111 ((unsigned int)(PMIC_REG_BASE+0x46))
#define MT6311_EFUSE_DOUT_112_119 ((unsigned int)(PMIC_REG_BASE+0x47))
#define MT6311_EFUSE_DOUT_120_127 ((unsigned int)(PMIC_REG_BASE+0x48))
#define MT6311_EFUSE_VAL_0_7      ((unsigned int)(PMIC_REG_BASE+0x49))
#define MT6311_EFUSE_VAL_8_15     ((unsigned int)(PMIC_REG_BASE+0x4A))
#define MT6311_EFUSE_VAL_16_23    ((unsigned int)(PMIC_REG_BASE+0x4B))
#define MT6311_EFUSE_VAL_24_31    ((unsigned int)(PMIC_REG_BASE+0x4C))
#define MT6311_EFUSE_VAL_32_39    ((unsigned int)(PMIC_REG_BASE+0x4D))
#define MT6311_EFUSE_VAL_40_47    ((unsigned int)(PMIC_REG_BASE+0x4E))
#define MT6311_EFUSE_VAL_48_55    ((unsigned int)(PMIC_REG_BASE+0x4F))
#define MT6311_EFUSE_VAL_56_63    ((unsigned int)(PMIC_REG_BASE+0x50))
#define MT6311_EFUSE_VAL_64_71    ((unsigned int)(PMIC_REG_BASE+0x51))
#define MT6311_EFUSE_VAL_72_79    ((unsigned int)(PMIC_REG_BASE+0x52))
#define MT6311_EFUSE_VAL_80_87    ((unsigned int)(PMIC_REG_BASE+0x53))
#define MT6311_EFUSE_VAL_88_95    ((unsigned int)(PMIC_REG_BASE+0x54))
#define MT6311_EFUSE_VAL_96_103   ((unsigned int)(PMIC_REG_BASE+0x55))
#define MT6311_EFUSE_VAL_104_111  ((unsigned int)(PMIC_REG_BASE+0x56))
#define MT6311_EFUSE_VAL_112_119  ((unsigned int)(PMIC_REG_BASE+0x57))
#define MT6311_EFUSE_VAL_120_127  ((unsigned int)(PMIC_REG_BASE+0x58))
#define MT6311_BUCK_ALL_CON0      ((unsigned int)(PMIC_REG_BASE+0x59))
#define MT6311_BUCK_ALL_CON1      ((unsigned int)(PMIC_REG_BASE+0x5A))
#define MT6311_BUCK_ALL_CON2      ((unsigned int)(PMIC_REG_BASE+0x5B))
#define MT6311_BUCK_ALL_CON3      ((unsigned int)(PMIC_REG_BASE+0x5C))
#define MT6311_BUCK_ALL_CON4      ((unsigned int)(PMIC_REG_BASE+0x5D))
#define MT6311_BUCK_ALL_CON5      ((unsigned int)(PMIC_REG_BASE+0x5E))
#define MT6311_BUCK_ALL_CON6      ((unsigned int)(PMIC_REG_BASE+0x5F))
#define MT6311_BUCK_ALL_CON7      ((unsigned int)(PMIC_REG_BASE+0x60))
#define MT6311_BUCK_ALL_CON8      ((unsigned int)(PMIC_REG_BASE+0x61))
#define MT6311_BUCK_ALL_CON9      ((unsigned int)(PMIC_REG_BASE+0x62))
#define MT6311_BUCK_ALL_CON10     ((unsigned int)(PMIC_REG_BASE+0x63))
#define MT6311_BUCK_ALL_CON18     ((unsigned int)(PMIC_REG_BASE+0x64))
#define MT6311_BUCK_ALL_CON19     ((unsigned int)(PMIC_REG_BASE+0x65))
#define MT6311_BUCK_ALL_CON20     ((unsigned int)(PMIC_REG_BASE+0x66))
#define MT6311_BUCK_ALL_CON21     ((unsigned int)(PMIC_REG_BASE+0x67))
#define MT6311_BUCK_ALL_CON22     ((unsigned int)(PMIC_REG_BASE+0x68))
#define MT6311_BUCK_ALL_CON23     ((unsigned int)(PMIC_REG_BASE+0x69))
#define MT6311_BUCK_ALL_CON24     ((unsigned int)(PMIC_REG_BASE+0x6A))
#define MT6311_ANA_RSV_CON0       ((unsigned int)(PMIC_REG_BASE+0x6B))
#define MT6311_STRUP_ANA_CON0     ((unsigned int)(PMIC_REG_BASE+0x6C))
#define MT6311_STRUP_ANA_CON1     ((unsigned int)(PMIC_REG_BASE+0x6D))
#define MT6311_STRUP_ANA_CON2     ((unsigned int)(PMIC_REG_BASE+0x6E))
#define MT6311_STRUP_ANA_CON3     ((unsigned int)(PMIC_REG_BASE+0x6F))
#define MT6311_STRUP_ANA_CON4     ((unsigned int)(PMIC_REG_BASE+0x70))
#define MT6311_STRUP_ANA_CON5     ((unsigned int)(PMIC_REG_BASE+0x71))
#define MT6311_STRUP_ANA_CON6     ((unsigned int)(PMIC_REG_BASE+0x72))
#define MT6311_STRUP_ANA_CON7     ((unsigned int)(PMIC_REG_BASE+0x73))
#define MT6311_STRUP_ANA_CON8     ((unsigned int)(PMIC_REG_BASE+0x74))
#define MT6311_STRUP_ANA_CON9     ((unsigned int)(PMIC_REG_BASE+0x75))
#define MT6311_STRUP_ANA_CON10    ((unsigned int)(PMIC_REG_BASE+0x76))
#define MT6311_STRUP_ANA_CON11    ((unsigned int)(PMIC_REG_BASE+0x77))
#define MT6311_STRUP_ANA_CON12    ((unsigned int)(PMIC_REG_BASE+0x78))
#define MT6311_VBIASN_ANA_CON0    ((unsigned int)(PMIC_REG_BASE+0x79))
#define MT6311_VDVFS1_ANA_CON0    ((unsigned int)(PMIC_REG_BASE+0x7A))
#define MT6311_VDVFS1_ANA_CON1    ((unsigned int)(PMIC_REG_BASE+0x7B))
#define MT6311_VDVFS1_ANA_CON2    ((unsigned int)(PMIC_REG_BASE+0x7C))
#define MT6311_VDVFS1_ANA_CON3    ((unsigned int)(PMIC_REG_BASE+0x7D))
#define MT6311_VDVFS1_ANA_CON4    ((unsigned int)(PMIC_REG_BASE+0x7E))
#define MT6311_VDVFS1_ANA_CON5    ((unsigned int)(PMIC_REG_BASE+0x7F))
#define MT6311_VDVFS1_ANA_CON6    ((unsigned int)(PMIC_REG_BASE+0x80))
#define MT6311_VDVFS1_ANA_CON7    ((unsigned int)(PMIC_REG_BASE+0x81))
#define MT6311_VDVFS1_ANA_CON8    ((unsigned int)(PMIC_REG_BASE+0x82))
#define MT6311_VDVFS1_ANA_CON9    ((unsigned int)(PMIC_REG_BASE+0x83))
#define MT6311_VDVFS1_ANA_CON10   ((unsigned int)(PMIC_REG_BASE+0x84))
#define MT6311_VDVFS1_ANA_CON11   ((unsigned int)(PMIC_REG_BASE+0x85))
#define MT6311_VDVFS1_ANA_CON12   ((unsigned int)(PMIC_REG_BASE+0x86))
#define MT6311_VDVFS11_CON0       ((unsigned int)(PMIC_REG_BASE+0x87))
#define MT6311_VDVFS11_CON7       ((unsigned int)(PMIC_REG_BASE+0x88))
#define MT6311_VDVFS11_CON8       ((unsigned int)(PMIC_REG_BASE+0x89))
#define MT6311_VDVFS11_CON9       ((unsigned int)(PMIC_REG_BASE+0x8A))
#define MT6311_VDVFS11_CON10      ((unsigned int)(PMIC_REG_BASE+0x8B))
#define MT6311_VDVFS11_CON11      ((unsigned int)(PMIC_REG_BASE+0x8C))
#define MT6311_VDVFS11_CON12      ((unsigned int)(PMIC_REG_BASE+0x8D))
#define MT6311_VDVFS11_CON13      ((unsigned int)(PMIC_REG_BASE+0x8E))
#define MT6311_VDVFS11_CON14      ((unsigned int)(PMIC_REG_BASE+0x8F))
#define MT6311_VDVFS11_CON15      ((unsigned int)(PMIC_REG_BASE+0x90))
#define MT6311_VDVFS11_CON16      ((unsigned int)(PMIC_REG_BASE+0x91))
#define MT6311_VDVFS11_CON17      ((unsigned int)(PMIC_REG_BASE+0x92))
#define MT6311_VDVFS11_CON18      ((unsigned int)(PMIC_REG_BASE+0x93))
#define MT6311_VDVFS11_CON19      ((unsigned int)(PMIC_REG_BASE+0x94))
#define MT6311_VDVFS12_CON0       ((unsigned int)(PMIC_REG_BASE+0x95))
#define MT6311_VDVFS12_CON7       ((unsigned int)(PMIC_REG_BASE+0x96))
#define MT6311_VDVFS12_CON8       ((unsigned int)(PMIC_REG_BASE+0x97))
#define MT6311_VDVFS12_CON9       ((unsigned int)(PMIC_REG_BASE+0x98))
#define MT6311_VDVFS12_CON10      ((unsigned int)(PMIC_REG_BASE+0x99))
#define MT6311_VDVFS12_CON11      ((unsigned int)(PMIC_REG_BASE+0x9A))
#define MT6311_VDVFS12_CON12      ((unsigned int)(PMIC_REG_BASE+0x9B))
#define MT6311_VDVFS12_CON13      ((unsigned int)(PMIC_REG_BASE+0x9C))
#define MT6311_VDVFS12_CON14      ((unsigned int)(PMIC_REG_BASE+0x9D))
#define MT6311_VDVFS12_CON15      ((unsigned int)(PMIC_REG_BASE+0x9E))
#define MT6311_VDVFS12_CON16      ((unsigned int)(PMIC_REG_BASE+0x9F))
#define MT6311_VDVFS12_CON17      ((unsigned int)(PMIC_REG_BASE+0xA0))
#define MT6311_VDVFS12_CON18      ((unsigned int)(PMIC_REG_BASE+0xA1))
#define MT6311_VDVFS12_CON19      ((unsigned int)(PMIC_REG_BASE+0xA2))
#define MT6311_BUCK_K_CON0        ((unsigned int)(PMIC_REG_BASE+0xA3))
#define MT6311_BUCK_K_CON1        ((unsigned int)(PMIC_REG_BASE+0xA4))
#define MT6311_BUCK_K_CON2        ((unsigned int)(PMIC_REG_BASE+0xA5))
#define MT6311_BUCK_K_CON3        ((unsigned int)(PMIC_REG_BASE+0xA6))
#define MT6311_BUCK_K_CON4        ((unsigned int)(PMIC_REG_BASE+0xA7))
#define MT6311_BUCK_K_CON5        ((unsigned int)(PMIC_REG_BASE+0xA8))
#define MT6311_AUXADC_ADC0        ((unsigned int)(PMIC_REG_BASE+0xA9))
#define MT6311_AUXADC_ADC1        ((unsigned int)(PMIC_REG_BASE+0xAA))
#define MT6311_AUXADC_ADC2        ((unsigned int)(PMIC_REG_BASE+0xAB))
#define MT6311_AUXADC_ADC3        ((unsigned int)(PMIC_REG_BASE+0xAC))
#define MT6311_AUXADC_STA0        ((unsigned int)(PMIC_REG_BASE+0xAD))
#define MT6311_AUXADC_RQST0       ((unsigned int)(PMIC_REG_BASE+0xAE))
#define MT6311_AUXADC_CON0        ((unsigned int)(PMIC_REG_BASE+0xAF))
#define MT6311_AUXADC_CON1        ((unsigned int)(PMIC_REG_BASE+0xB0))
#define MT6311_AUXADC_CON2        ((unsigned int)(PMIC_REG_BASE+0xB1))
#define MT6311_AUXADC_CON3        ((unsigned int)(PMIC_REG_BASE+0xB2))
#define MT6311_AUXADC_CON4        ((unsigned int)(PMIC_REG_BASE+0xB3))
#define MT6311_AUXADC_CON5        ((unsigned int)(PMIC_REG_BASE+0xB4))
#define MT6311_AUXADC_CON6        ((unsigned int)(PMIC_REG_BASE+0xB5))
#define MT6311_AUXADC_CON7        ((unsigned int)(PMIC_REG_BASE+0xB6))
#define MT6311_AUXADC_CON8        ((unsigned int)(PMIC_REG_BASE+0xB7))
#define MT6311_AUXADC_CON9        ((unsigned int)(PMIC_REG_BASE+0xB8))
#define MT6311_AUXADC_CON10       ((unsigned int)(PMIC_REG_BASE+0xB9))
#define MT6311_AUXADC_CON11       ((unsigned int)(PMIC_REG_BASE+0xBA))
#define MT6311_AUXADC_CON12       ((unsigned int)(PMIC_REG_BASE+0xBB))
#define MT6311_AUXADC_CON13       ((unsigned int)(PMIC_REG_BASE+0xBC))
#define MT6311_AUXADC_CON14       ((unsigned int)(PMIC_REG_BASE+0xBD))
#define MT6311_AUXADC_CON15       ((unsigned int)(PMIC_REG_BASE+0xBE))
#define MT6311_AUXADC_CON16       ((unsigned int)(PMIC_REG_BASE+0xBF))
#define MT6311_AUXADC_CON17       ((unsigned int)(PMIC_REG_BASE+0xC0))
#define MT6311_AUXADC_CON18       ((unsigned int)(PMIC_REG_BASE+0xC1))
#define MT6311_AUXADC_CON19       ((unsigned int)(PMIC_REG_BASE+0xC2))
#define MT6311_AUXADC_CON20       ((unsigned int)(PMIC_REG_BASE+0xC3))
#define MT6311_AUXADC_CON21       ((unsigned int)(PMIC_REG_BASE+0xC4))
#define MT6311_AUXADC_CON22       ((unsigned int)(PMIC_REG_BASE+0xC5))
#define MT6311_AUXADC_CON23       ((unsigned int)(PMIC_REG_BASE+0xC6))
#define MT6311_AUXADC_CON24       ((unsigned int)(PMIC_REG_BASE+0xC7))
#define MT6311_AUXADC_CON25       ((unsigned int)(PMIC_REG_BASE+0xC8))
#define MT6311_AUXADC_CON26       ((unsigned int)(PMIC_REG_BASE+0xC9))
#define MT6311_AUXADC_CON27       ((unsigned int)(PMIC_REG_BASE+0xCA))
#define MT6311_AUXADC_CON28       ((unsigned int)(PMIC_REG_BASE+0xCB))
#define MT6311_LDO_CON0           ((unsigned int)(PMIC_REG_BASE+0xCC))
#define MT6311_LDO_OCFB0          ((unsigned int)(PMIC_REG_BASE+0xCD))
#define MT6311_LDO_CON2           ((unsigned int)(PMIC_REG_BASE+0xCE))
#define MT6311_LDO_CON3           ((unsigned int)(PMIC_REG_BASE+0xCF))
#define MT6311_LDO_CON4           ((unsigned int)(PMIC_REG_BASE+0xD0))
#define MT6311_FQMTR_CON0         ((unsigned int)(PMIC_REG_BASE+0xD1))
#define MT6311_FQMTR_CON1         ((unsigned int)(PMIC_REG_BASE+0xD2))
#define MT6311_FQMTR_CON2         ((unsigned int)(PMIC_REG_BASE+0xD3))
#define MT6311_FQMTR_CON3         ((unsigned int)(PMIC_REG_BASE+0xD4))
#define MT6311_FQMTR_CON4         ((unsigned int)(PMIC_REG_BASE+0xD5))
/* mask is HEX;  shift is Integer */
#define MT6311_PMIC_CID_ADDR                             MT6311_CID
#define MT6311_PMIC_CID_MASK                             0xFF
#define MT6311_PMIC_CID_SHIFT                            0
#define MT6311_PMIC_SWCID_ADDR                           MT6311_SWCID
#define MT6311_PMIC_SWCID_MASK                           0xFF
#define MT6311_PMIC_SWCID_SHIFT                          0
#define MT6311_PMIC_HWCID_ADDR                           MT6311_HWCID
#define MT6311_PMIC_HWCID_MASK                           0xFF
#define MT6311_PMIC_HWCID_SHIFT                          0
#define MT6311_PMIC_GPIO0_DIR_ADDR                       MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO0_DIR_MASK                       0x1
#define MT6311_PMIC_GPIO0_DIR_SHIFT                      0
#define MT6311_PMIC_GPIO1_DIR_ADDR                       MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO1_DIR_MASK                       0x1
#define MT6311_PMIC_GPIO1_DIR_SHIFT                      1
#define MT6311_PMIC_GPIO0_DINV_ADDR                      MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO0_DINV_MASK                      0x1
#define MT6311_PMIC_GPIO0_DINV_SHIFT                     2
#define MT6311_PMIC_GPIO1_DINV_ADDR                      MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO1_DINV_MASK                      0x1
#define MT6311_PMIC_GPIO1_DINV_SHIFT                     3
#define MT6311_PMIC_GPIO0_DOUT_ADDR                      MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO0_DOUT_MASK                      0x1
#define MT6311_PMIC_GPIO0_DOUT_SHIFT                     4
#define MT6311_PMIC_GPIO1_DOUT_ADDR                      MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO1_DOUT_MASK                      0x1
#define MT6311_PMIC_GPIO1_DOUT_SHIFT                     5
#define MT6311_PMIC_GPIO0_DIN_ADDR                       MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO0_DIN_MASK                       0x1
#define MT6311_PMIC_GPIO0_DIN_SHIFT                      6
#define MT6311_PMIC_GPIO1_DIN_ADDR                       MT6311_GPIO_CFG
#define MT6311_PMIC_GPIO1_DIN_MASK                       0x1
#define MT6311_PMIC_GPIO1_DIN_SHIFT                      7
#define MT6311_PMIC_GPIO0_MODE_ADDR                      MT6311_GPIO_MODE
#define MT6311_PMIC_GPIO0_MODE_MASK                      0x7
#define MT6311_PMIC_GPIO0_MODE_SHIFT                     0
#define MT6311_PMIC_GPIO1_MODE_ADDR                      MT6311_GPIO_MODE
#define MT6311_PMIC_GPIO1_MODE_MASK                      0x7
#define MT6311_PMIC_GPIO1_MODE_SHIFT                     3
#define MT6311_PMIC_TEST_OUT_ADDR                        MT6311_TEST_OUT
#define MT6311_PMIC_TEST_OUT_MASK                        0x3
#define MT6311_PMIC_TEST_OUT_SHIFT                       0
#define MT6311_PMIC_RG_MON_GRP_SEL_ADDR                  MT6311_TEST_CON0
#define MT6311_PMIC_RG_MON_GRP_SEL_MASK                  0xF
#define MT6311_PMIC_RG_MON_GRP_SEL_SHIFT                 0
#define MT6311_PMIC_RG_MON_FLAG_SEL_ADDR                 MT6311_TEST_CON1
#define MT6311_PMIC_RG_MON_FLAG_SEL_MASK                 0xFF
#define MT6311_PMIC_RG_MON_FLAG_SEL_SHIFT                0
#define MT6311_PMIC_DIG_TESTMODE_ADDR                    MT6311_TEST_CON2
#define MT6311_PMIC_DIG_TESTMODE_MASK                    0x1
#define MT6311_PMIC_DIG_TESTMODE_SHIFT                   0
#define MT6311_PMIC_PMU_TESTMODE_ADDR                    MT6311_TEST_CON3
#define MT6311_PMIC_PMU_TESTMODE_MASK                    0x1
#define MT6311_PMIC_PMU_TESTMODE_SHIFT                   0
#define MT6311_PMIC_RG_SRCLKEN_IN_HW_MODE_ADDR           MT6311_TOP_CON
#define MT6311_PMIC_RG_SRCLKEN_IN_HW_MODE_MASK           0x1
#define MT6311_PMIC_RG_SRCLKEN_IN_HW_MODE_SHIFT          0
#define MT6311_PMIC_RG_SRCLKEN_IN_EN_ADDR                MT6311_TOP_CON
#define MT6311_PMIC_RG_SRCLKEN_IN_EN_MASK                0x1
#define MT6311_PMIC_RG_SRCLKEN_IN_EN_SHIFT               1
#define MT6311_PMIC_RG_BUCK_LP_HW_MODE_ADDR              MT6311_TOP_CON
#define MT6311_PMIC_RG_BUCK_LP_HW_MODE_MASK              0x1
#define MT6311_PMIC_RG_BUCK_LP_HW_MODE_SHIFT             2
#define MT6311_PMIC_RG_BUCK_LP_EN_ADDR                   MT6311_TOP_CON
#define MT6311_PMIC_RG_BUCK_LP_EN_MASK                   0x1
#define MT6311_PMIC_RG_BUCK_LP_EN_SHIFT                  3
#define MT6311_PMIC_RG_OSC_EN_ADDR                       MT6311_TOP_CON
#define MT6311_PMIC_RG_OSC_EN_MASK                       0x1
#define MT6311_PMIC_RG_OSC_EN_SHIFT                      4
#define MT6311_PMIC_RG_OSC_EN_HW_MODE_ADDR               MT6311_TOP_CON
#define MT6311_PMIC_RG_OSC_EN_HW_MODE_MASK               0x1
#define MT6311_PMIC_RG_OSC_EN_HW_MODE_SHIFT              5
#define MT6311_PMIC_RG_SRCLKEN_IN_SYNC_EN_ADDR           MT6311_TOP_CON
#define MT6311_PMIC_RG_SRCLKEN_IN_SYNC_EN_MASK           0x1
#define MT6311_PMIC_RG_SRCLKEN_IN_SYNC_EN_SHIFT          6
#define MT6311_PMIC_RG_STRUP_RSV_HW_MODE_ADDR            MT6311_TOP_CON
#define MT6311_PMIC_RG_STRUP_RSV_HW_MODE_MASK            0x1
#define MT6311_PMIC_RG_STRUP_RSV_HW_MODE_SHIFT           7
#define MT6311_PMIC_RG_BUCK_REF_CK_TSTSEL_ADDR           MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_BUCK_REF_CK_TSTSEL_MASK           0x1
#define MT6311_PMIC_RG_BUCK_REF_CK_TSTSEL_SHIFT          0
#define MT6311_PMIC_RG_FQMTR_CK_TSTSEL_ADDR              MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_FQMTR_CK_TSTSEL_MASK              0x1
#define MT6311_PMIC_RG_FQMTR_CK_TSTSEL_SHIFT             1
#define MT6311_PMIC_RG_SMPS_CK_TSTSEL_ADDR               MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_SMPS_CK_TSTSEL_MASK               0x1
#define MT6311_PMIC_RG_SMPS_CK_TSTSEL_SHIFT              2
#define MT6311_PMIC_RG_PMU75K_CK_TSTSEL_ADDR             MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_PMU75K_CK_TSTSEL_MASK             0x1
#define MT6311_PMIC_RG_PMU75K_CK_TSTSEL_SHIFT            3
#define MT6311_PMIC_RG_SMPS_CK_TST_DIS_ADDR              MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_SMPS_CK_TST_DIS_MASK              0x1
#define MT6311_PMIC_RG_SMPS_CK_TST_DIS_SHIFT             4
#define MT6311_PMIC_RG_PMU75K_CK_TST_DIS_ADDR            MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_PMU75K_CK_TST_DIS_MASK            0x1
#define MT6311_PMIC_RG_PMU75K_CK_TST_DIS_SHIFT           5
#define MT6311_PMIC_RG_BUCK_ANA_AUTO_OFF_DIS_ADDR        MT6311_TOP_CKTST_CON
#define MT6311_PMIC_RG_BUCK_ANA_AUTO_OFF_DIS_MASK        0x1
#define MT6311_PMIC_RG_BUCK_ANA_AUTO_OFF_DIS_SHIFT       6
#define MT6311_PMIC_RG_BUCK_REF_CK_PDN_ADDR              MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_BUCK_REF_CK_PDN_MASK              0x1
#define MT6311_PMIC_RG_BUCK_REF_CK_PDN_SHIFT             0
#define MT6311_PMIC_RG_BUCK_CK_PDN_ADDR                  MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_BUCK_CK_PDN_MASK                  0x1
#define MT6311_PMIC_RG_BUCK_CK_PDN_SHIFT                 1
#define MT6311_PMIC_RG_BUCK_1M_CK_PDN_ADDR               MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_BUCK_1M_CK_PDN_MASK               0x1
#define MT6311_PMIC_RG_BUCK_1M_CK_PDN_SHIFT              2
#define MT6311_PMIC_RG_INTRP_CK_PDN_ADDR                 MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_INTRP_CK_PDN_MASK                 0x1
#define MT6311_PMIC_RG_INTRP_CK_PDN_SHIFT                3
#define MT6311_PMIC_RG_EFUSE_CK_PDN_ADDR                 MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_EFUSE_CK_PDN_MASK                 0x1
#define MT6311_PMIC_RG_EFUSE_CK_PDN_SHIFT                4
#define MT6311_PMIC_RG_STRUP_75K_CK_PDN_ADDR             MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_STRUP_75K_CK_PDN_MASK             0x1
#define MT6311_PMIC_RG_STRUP_75K_CK_PDN_SHIFT            5
#define MT6311_PMIC_RG_BUCK_ANA_CK_PDN_ADDR              MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_BUCK_ANA_CK_PDN_MASK              0x1
#define MT6311_PMIC_RG_BUCK_ANA_CK_PDN_SHIFT             6
#define MT6311_PMIC_RG_TRIM_75K_CK_PDN_ADDR              MT6311_TOP_CKPDN_CON1
#define MT6311_PMIC_RG_TRIM_75K_CK_PDN_MASK              0x1
#define MT6311_PMIC_RG_TRIM_75K_CK_PDN_SHIFT             7
#define MT6311_PMIC_TOP_CKPDN_CON1_SET_ADDR          MT6311_TOP_CKPDN_CON1_SET
#define MT6311_PMIC_TOP_CKPDN_CON1_SET_MASK          0xFF
#define MT6311_PMIC_TOP_CKPDN_CON1_SET_SHIFT         0
#define MT6311_PMIC_TOP_CKPDN_CON1_CLR_ADDR          MT6311_TOP_CKPDN_CON1_CLR
#define MT6311_PMIC_TOP_CKPDN_CON1_CLR_MASK          0xFF
#define MT6311_PMIC_TOP_CKPDN_CON1_CLR_SHIFT         0
#define MT6311_PMIC_RG_AUXADC_CK_PDN_ADDR                MT6311_TOP_CKPDN_CON2
#define MT6311_PMIC_RG_AUXADC_CK_PDN_MASK                0x1
#define MT6311_PMIC_RG_AUXADC_CK_PDN_SHIFT               0
#define MT6311_PMIC_RG_AUXADC_1M_CK_PDN_ADDR             MT6311_TOP_CKPDN_CON2
#define MT6311_PMIC_RG_AUXADC_1M_CK_PDN_MASK             0x1
#define MT6311_PMIC_RG_AUXADC_1M_CK_PDN_SHIFT            1
#define MT6311_PMIC_RG_STB_75K_CK_PDN_ADDR               MT6311_TOP_CKPDN_CON2
#define MT6311_PMIC_RG_STB_75K_CK_PDN_MASK               0x1
#define MT6311_PMIC_RG_STB_75K_CK_PDN_SHIFT              2
#define MT6311_PMIC_RG_FQMTR_CK_PDN_ADDR                 MT6311_TOP_CKPDN_CON2
#define MT6311_PMIC_RG_FQMTR_CK_PDN_MASK                 0x1
#define MT6311_PMIC_RG_FQMTR_CK_PDN_SHIFT                3
#define MT6311_PMIC_TOP_CKPDN_CON2_RSV_ADDR              MT6311_TOP_CKPDN_CON2
#define MT6311_PMIC_TOP_CKPDN_CON2_RSV_MASK              0xF
#define MT6311_PMIC_TOP_CKPDN_CON2_RSV_SHIFT             4
#define MT6311_PMIC_TOP_CKPDN_CON2_SET_ADDR     MT6311_TOP_CKPDN_CON2_SET
#define MT6311_PMIC_TOP_CKPDN_CON2_SET_MASK     0xFF
#define MT6311_PMIC_TOP_CKPDN_CON2_SET_SHIFT    0
#define MT6311_PMIC_TOP_CKPDN_CON2_CLR_ADDR     MT6311_TOP_CKPDN_CON2_CLR
#define MT6311_PMIC_TOP_CKPDN_CON2_CLR_MASK     0xFF
#define MT6311_PMIC_TOP_CKPDN_CON2_CLR_SHIFT    0
#define MT6311_PMIC_RG_BUCK_1M_CK_PDN_HWEN_ADDR          MT6311_TOP_CKHWEN_CON
#define MT6311_PMIC_RG_BUCK_1M_CK_PDN_HWEN_MASK          0x1
#define MT6311_PMIC_RG_BUCK_1M_CK_PDN_HWEN_SHIFT         0
#define MT6311_PMIC_RG_EFUSE_CK_PDN_HWEN_ADDR            MT6311_TOP_CKHWEN_CON
#define MT6311_PMIC_RG_EFUSE_CK_PDN_HWEN_MASK            0x1
#define MT6311_PMIC_RG_EFUSE_CK_PDN_HWEN_SHIFT           1
#define MT6311_PMIC_TOP_CKHWEN_CON_SET_ADDR     MT6311_TOP_CKHWEN_CON_SET
#define MT6311_PMIC_TOP_CKHWEN_CON_SET_MASK     0x3
#define MT6311_PMIC_TOP_CKHWEN_CON_SET_SHIFT    0
#define MT6311_PMIC_TOP_CKHWEN_CON_CLR_ADDR     MT6311_TOP_CKHWEN_CON_CLR
#define MT6311_PMIC_TOP_CKHWEN_CON_CLR_MASK     0x3
#define MT6311_PMIC_TOP_CKHWEN_CON_CLR_SHIFT    0
#define MT6311_PMIC_RG_AUXADC_RST_ADDR                   MT6311_TOP_RST_CON
#define MT6311_PMIC_RG_AUXADC_RST_MASK                   0x1
#define MT6311_PMIC_RG_AUXADC_RST_SHIFT                  0
#define MT6311_PMIC_RG_FQMTR_RST_ADDR                    MT6311_TOP_RST_CON
#define MT6311_PMIC_RG_FQMTR_RST_MASK                    0x1
#define MT6311_PMIC_RG_FQMTR_RST_SHIFT                   1
#define MT6311_PMIC_RG_CLK_TRIM_RST_ADDR                 MT6311_TOP_RST_CON
#define MT6311_PMIC_RG_CLK_TRIM_RST_MASK                 0x1
#define MT6311_PMIC_RG_CLK_TRIM_RST_SHIFT                2
#define MT6311_PMIC_RG_EFUSE_MAN_RST_ADDR                MT6311_TOP_RST_CON
#define MT6311_PMIC_RG_EFUSE_MAN_RST_MASK                0x1
#define MT6311_PMIC_RG_EFUSE_MAN_RST_SHIFT               3
#define MT6311_PMIC_RG_WDTRSTB_MODE_ADDR                 MT6311_TOP_RST_CON
#define MT6311_PMIC_RG_WDTRSTB_MODE_MASK                 0x1
#define MT6311_PMIC_RG_WDTRSTB_MODE_SHIFT                4
#define MT6311_PMIC_RG_WDTRSTB_EN_ADDR                   MT6311_TOP_RST_CON
#define MT6311_PMIC_RG_WDTRSTB_EN_MASK                   0x1
#define MT6311_PMIC_RG_WDTRSTB_EN_SHIFT                  5
#define MT6311_PMIC_WDTRSTB_STATUS_CLR_ADDR              MT6311_TOP_RST_CON
#define MT6311_PMIC_WDTRSTB_STATUS_CLR_MASK              0x1
#define MT6311_PMIC_WDTRSTB_STATUS_CLR_SHIFT             6
#define MT6311_PMIC_WDTRSTB_STATUS_ADDR                  MT6311_TOP_RST_CON
#define MT6311_PMIC_WDTRSTB_STATUS_MASK                  0x1
#define MT6311_PMIC_WDTRSTB_STATUS_SHIFT                 7
#define MT6311_PMIC_TOP_RST_CON_SET_ADDR                 MT6311_TOP_RST_CON_SET
#define MT6311_PMIC_TOP_RST_CON_SET_MASK                 0xFF
#define MT6311_PMIC_TOP_RST_CON_SET_SHIFT                0
#define MT6311_PMIC_TOP_RST_CON_CLR_ADDR                 MT6311_TOP_RST_CON_CLR
#define MT6311_PMIC_TOP_RST_CON_CLR_MASK                 0xFF
#define MT6311_PMIC_TOP_RST_CON_CLR_SHIFT                0
#define MT6311_PMIC_RG_INT_POL_ADDR                      MT6311_TOP_INT_CON
#define MT6311_PMIC_RG_INT_POL_MASK                      0x1
#define MT6311_PMIC_RG_INT_POL_SHIFT                     0
#define MT6311_PMIC_RG_INT_EN_ADDR                       MT6311_TOP_INT_CON
#define MT6311_PMIC_RG_INT_EN_MASK                       0x1
#define MT6311_PMIC_RG_INT_EN_SHIFT                      1
#define MT6311_PMIC_I2C_CONFIG_ADDR                      MT6311_TOP_INT_CON
#define MT6311_PMIC_I2C_CONFIG_MASK                      0x1
#define MT6311_PMIC_I2C_CONFIG_SHIFT                     2
#define MT6311_PMIC_RG_LBAT_MIN_INT_STATUS_ADDR          MT6311_TOP_INT_MON
#define MT6311_PMIC_RG_LBAT_MIN_INT_STATUS_MASK          0x1
#define MT6311_PMIC_RG_LBAT_MIN_INT_STATUS_SHIFT         0
#define MT6311_PMIC_RG_LBAT_MAX_INT_STATUS_ADDR          MT6311_TOP_INT_MON
#define MT6311_PMIC_RG_LBAT_MAX_INT_STATUS_MASK          0x1
#define MT6311_PMIC_RG_LBAT_MAX_INT_STATUS_SHIFT         1
#define MT6311_PMIC_RG_THR_L_INT_STATUS_ADDR             MT6311_TOP_INT_MON
#define MT6311_PMIC_RG_THR_L_INT_STATUS_MASK             0x1
#define MT6311_PMIC_RG_THR_L_INT_STATUS_SHIFT            2
#define MT6311_PMIC_RG_THR_H_INT_STATUS_ADDR             MT6311_TOP_INT_MON
#define MT6311_PMIC_RG_THR_H_INT_STATUS_MASK             0x1
#define MT6311_PMIC_RG_THR_H_INT_STATUS_SHIFT            3
#define MT6311_PMIC_RG_BUCK_OC_INT_STATUS_ADDR           MT6311_TOP_INT_MON
#define MT6311_PMIC_RG_BUCK_OC_INT_STATUS_MASK           0x1
#define MT6311_PMIC_RG_BUCK_OC_INT_STATUS_SHIFT          4
#define MT6311_PMIC_THR_DET_DIS_ADDR                     MT6311_STRUP_CON0
#define MT6311_PMIC_THR_DET_DIS_MASK                     0x1
#define MT6311_PMIC_THR_DET_DIS_SHIFT                    0
#define MT6311_PMIC_THR_HWPDN_EN_ADDR                    MT6311_STRUP_CON0
#define MT6311_PMIC_THR_HWPDN_EN_MASK                    0x1
#define MT6311_PMIC_THR_HWPDN_EN_SHIFT                   1
#define MT6311_PMIC_STRUP_DIG0_RSV0_ADDR                 MT6311_STRUP_CON0
#define MT6311_PMIC_STRUP_DIG0_RSV0_MASK                 0x3F
#define MT6311_PMIC_STRUP_DIG0_RSV0_SHIFT                2
#define MT6311_PMIC_RG_USBDL_EN_ADDR                     MT6311_STRUP_CON1
#define MT6311_PMIC_RG_USBDL_EN_MASK                     0x1
#define MT6311_PMIC_RG_USBDL_EN_SHIFT                    0
#define MT6311_PMIC_RG_TEST_STRUP_ADDR                   MT6311_STRUP_CON1
#define MT6311_PMIC_RG_TEST_STRUP_MASK                   0x1
#define MT6311_PMIC_RG_TEST_STRUP_SHIFT                  1
#define MT6311_PMIC_RG_TEST_STRUP_THR_IN_ADDR            MT6311_STRUP_CON1
#define MT6311_PMIC_RG_TEST_STRUP_THR_IN_MASK            0x1
#define MT6311_PMIC_RG_TEST_STRUP_THR_IN_SHIFT           2
#define MT6311_PMIC_STRUP_DIG1_RSV0_ADDR                 MT6311_STRUP_CON1
#define MT6311_PMIC_STRUP_DIG1_RSV0_MASK                 0x1F
#define MT6311_PMIC_STRUP_DIG1_RSV0_SHIFT                3
#define MT6311_PMIC_THR_TEST_ADDR                        MT6311_STRUP_CON2
#define MT6311_PMIC_THR_TEST_MASK                        0x3
#define MT6311_PMIC_THR_TEST_SHIFT                       0
#define MT6311_PMIC_PMU_THR_DEB_ADDR                     MT6311_STRUP_CON2
#define MT6311_PMIC_PMU_THR_DEB_MASK                     0x7
#define MT6311_PMIC_PMU_THR_DEB_SHIFT                    2
#define MT6311_PMIC_PMU_THR_STATUS_ADDR                  MT6311_STRUP_CON2
#define MT6311_PMIC_PMU_THR_STATUS_MASK                  0x7
#define MT6311_PMIC_PMU_THR_STATUS_SHIFT                 5
#define MT6311_PMIC_STRUP_PWRON_ADDR                     MT6311_STRUP_CON3
#define MT6311_PMIC_STRUP_PWRON_MASK                     0x1
#define MT6311_PMIC_STRUP_PWRON_SHIFT                    0
#define MT6311_PMIC_STRUP_PWRON_SEL_ADDR                 MT6311_STRUP_CON3
#define MT6311_PMIC_STRUP_PWRON_SEL_MASK                 0x1
#define MT6311_PMIC_STRUP_PWRON_SEL_SHIFT                1
#define MT6311_PMIC_BIAS_GEN_EN_ADDR                     MT6311_STRUP_CON3
#define MT6311_PMIC_BIAS_GEN_EN_MASK                     0x1
#define MT6311_PMIC_BIAS_GEN_EN_SHIFT                    2
#define MT6311_PMIC_BIAS_GEN_EN_SEL_ADDR                 MT6311_STRUP_CON3
#define MT6311_PMIC_BIAS_GEN_EN_SEL_MASK                 0x1
#define MT6311_PMIC_BIAS_GEN_EN_SEL_SHIFT                3
#define MT6311_PMIC_RTC_XOSC32_ENB_SW_ADDR               MT6311_STRUP_CON3
#define MT6311_PMIC_RTC_XOSC32_ENB_SW_MASK               0x1
#define MT6311_PMIC_RTC_XOSC32_ENB_SW_SHIFT              4
#define MT6311_PMIC_RTC_XOSC32_ENB_SEL_ADDR              MT6311_STRUP_CON3
#define MT6311_PMIC_RTC_XOSC32_ENB_SEL_MASK              0x1
#define MT6311_PMIC_RTC_XOSC32_ENB_SEL_SHIFT             5
#define MT6311_PMIC_STRUP_DIG_IO_PG_FORCE_ADDR           MT6311_STRUP_CON3
#define MT6311_PMIC_STRUP_DIG_IO_PG_FORCE_MASK           0x1
#define MT6311_PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT          6
#define MT6311_PMIC_DDUVLO_DEB_EN_ADDR                   MT6311_STRUP_CON4
#define MT6311_PMIC_DDUVLO_DEB_EN_MASK                   0x1
#define MT6311_PMIC_DDUVLO_DEB_EN_SHIFT                  0
#define MT6311_PMIC_PWRBB_DEB_EN_ADDR                    MT6311_STRUP_CON4
#define MT6311_PMIC_PWRBB_DEB_EN_MASK                    0x1
#define MT6311_PMIC_PWRBB_DEB_EN_SHIFT                   1
#define MT6311_PMIC_STRUP_OSC_EN_ADDR                    MT6311_STRUP_CON4
#define MT6311_PMIC_STRUP_OSC_EN_MASK                    0x1
#define MT6311_PMIC_STRUP_OSC_EN_SHIFT                   2
#define MT6311_PMIC_STRUP_OSC_EN_SEL_ADDR                MT6311_STRUP_CON4
#define MT6311_PMIC_STRUP_OSC_EN_SEL_MASK                0x1
#define MT6311_PMIC_STRUP_OSC_EN_SEL_SHIFT               3
#define MT6311_PMIC_STRUP_FT_CTRL_ADDR                   MT6311_STRUP_CON4
#define MT6311_PMIC_STRUP_FT_CTRL_MASK                   0x3
#define MT6311_PMIC_STRUP_FT_CTRL_SHIFT                  4
#define MT6311_PMIC_STRUP_PWRON_FORCE_ADDR               MT6311_STRUP_CON4
#define MT6311_PMIC_STRUP_PWRON_FORCE_MASK               0x1
#define MT6311_PMIC_STRUP_PWRON_FORCE_SHIFT              6
#define MT6311_PMIC_BIAS_GEN_EN_FORCE_ADDR               MT6311_STRUP_CON4
#define MT6311_PMIC_BIAS_GEN_EN_FORCE_MASK               0x1
#define MT6311_PMIC_BIAS_GEN_EN_FORCE_SHIFT              7
#define MT6311_PMIC_VDVFS11_PG_H2L_EN_ADDR               MT6311_STRUP_CON5
#define MT6311_PMIC_VDVFS11_PG_H2L_EN_MASK               0x1
#define MT6311_PMIC_VDVFS11_PG_H2L_EN_SHIFT              0
#define MT6311_PMIC_VDVFS12_PG_H2L_EN_ADDR               MT6311_STRUP_CON5
#define MT6311_PMIC_VDVFS12_PG_H2L_EN_MASK               0x1
#define MT6311_PMIC_VDVFS12_PG_H2L_EN_SHIFT              1
#define MT6311_PMIC_VBIASN_PG_H2L_EN_ADDR                MT6311_STRUP_CON5
#define MT6311_PMIC_VBIASN_PG_H2L_EN_MASK                0x1
#define MT6311_PMIC_VBIASN_PG_H2L_EN_SHIFT               2
#define MT6311_PMIC_VDVFS11_PG_ENB_ADDR                  MT6311_STRUP_CON6
#define MT6311_PMIC_VDVFS11_PG_ENB_MASK                  0x1
#define MT6311_PMIC_VDVFS11_PG_ENB_SHIFT                 0
#define MT6311_PMIC_VDVFS12_PG_ENB_ADDR                  MT6311_STRUP_CON6
#define MT6311_PMIC_VDVFS12_PG_ENB_MASK                  0x1
#define MT6311_PMIC_VDVFS12_PG_ENB_SHIFT                 1
#define MT6311_PMIC_VBIASN_PG_ENB_ADDR                   MT6311_STRUP_CON6
#define MT6311_PMIC_VBIASN_PG_ENB_MASK                   0x1
#define MT6311_PMIC_VBIASN_PG_ENB_SHIFT                  2
#define MT6311_PMIC_RG_EXT_PMIC_EN_PG_ENB_ADDR           MT6311_STRUP_CON6
#define MT6311_PMIC_RG_EXT_PMIC_EN_PG_ENB_MASK           0x1
#define MT6311_PMIC_RG_EXT_PMIC_EN_PG_ENB_SHIFT          3
#define MT6311_PMIC_RG_PRE_PWRON_EN_ADDR                 MT6311_STRUP_CON7
#define MT6311_PMIC_RG_PRE_PWRON_EN_MASK                 0x1
#define MT6311_PMIC_RG_PRE_PWRON_EN_SHIFT                0
#define MT6311_PMIC_RG_PRE_PWRON_SWCTRL_ADDR             MT6311_STRUP_CON7
#define MT6311_PMIC_RG_PRE_PWRON_SWCTRL_MASK             0x1
#define MT6311_PMIC_RG_PRE_PWRON_SWCTRL_SHIFT            1
#define MT6311_PMIC_CLR_JUST_RST_ADDR                    MT6311_STRUP_CON7
#define MT6311_PMIC_CLR_JUST_RST_MASK                    0x1
#define MT6311_PMIC_CLR_JUST_RST_SHIFT                   4
#define MT6311_PMIC_UVLO_L2H_DEB_EN_ADDR                 MT6311_STRUP_CON7
#define MT6311_PMIC_UVLO_L2H_DEB_EN_MASK                 0x1
#define MT6311_PMIC_UVLO_L2H_DEB_EN_SHIFT                5
#define MT6311_PMIC_RG_BGR_TEST_CKIN_EN_ADDR             MT6311_STRUP_CON7
#define MT6311_PMIC_RG_BGR_TEST_CKIN_EN_MASK             0x1
#define MT6311_PMIC_RG_BGR_TEST_CKIN_EN_SHIFT            6
#define MT6311_PMIC_QI_OSC_EN_ADDR                       MT6311_STRUP_CON7
#define MT6311_PMIC_QI_OSC_EN_MASK                       0x1
#define MT6311_PMIC_QI_OSC_EN_SHIFT                      7
#define MT6311_PMIC_RG_STRUP_PMU_PWRON_SEL_ADDR          MT6311_STRUP_CON8
#define MT6311_PMIC_RG_STRUP_PMU_PWRON_SEL_MASK          0x1
#define MT6311_PMIC_RG_STRUP_PMU_PWRON_SEL_SHIFT         0
#define MT6311_PMIC_RG_STRUP_PMU_PWRON_EN_ADDR           MT6311_STRUP_CON8
#define MT6311_PMIC_RG_STRUP_PMU_PWRON_EN_MASK           0x1
#define MT6311_PMIC_RG_STRUP_PMU_PWRON_EN_SHIFT          1
#define MT6311_PMIC_STRUP_AUXADC_START_SW_ADDR           MT6311_STRUP_CON9
#define MT6311_PMIC_STRUP_AUXADC_START_SW_MASK           0x1
#define MT6311_PMIC_STRUP_AUXADC_START_SW_SHIFT          4
#define MT6311_PMIC_STRUP_AUXADC_RSTB_SW_ADDR            MT6311_STRUP_CON9
#define MT6311_PMIC_STRUP_AUXADC_RSTB_SW_MASK            0x1
#define MT6311_PMIC_STRUP_AUXADC_RSTB_SW_SHIFT           5
#define MT6311_PMIC_STRUP_AUXADC_START_SEL_ADDR          MT6311_STRUP_CON9
#define MT6311_PMIC_STRUP_AUXADC_START_SEL_MASK          0x1
#define MT6311_PMIC_STRUP_AUXADC_START_SEL_SHIFT         6
#define MT6311_PMIC_STRUP_AUXADC_RSTB_SEL_ADDR           MT6311_STRUP_CON9
#define MT6311_PMIC_STRUP_AUXADC_RSTB_SEL_MASK           0x1
#define MT6311_PMIC_STRUP_AUXADC_RSTB_SEL_SHIFT          7
#define MT6311_PMIC_STRUP_PWROFF_PREOFF_EN_ADDR          MT6311_STRUP_CON10
#define MT6311_PMIC_STRUP_PWROFF_PREOFF_EN_MASK          0x1
#define MT6311_PMIC_STRUP_PWROFF_PREOFF_EN_SHIFT         0
#define MT6311_PMIC_STRUP_PWROFF_SEQ_EN_ADDR             MT6311_STRUP_CON10
#define MT6311_PMIC_STRUP_PWROFF_SEQ_EN_MASK             0x1
#define MT6311_PMIC_STRUP_PWROFF_SEQ_EN_SHIFT            1
#define MT6311_PMIC_RG_SYS_LATCH_EN_SWCTRL_ADDR          MT6311_STRUP_CON10
#define MT6311_PMIC_RG_SYS_LATCH_EN_SWCTRL_MASK          0x1
#define MT6311_PMIC_RG_SYS_LATCH_EN_SWCTRL_SHIFT         2
#define MT6311_PMIC_RG_SYS_LATCH_EN_ADDR                 MT6311_STRUP_CON10
#define MT6311_PMIC_RG_SYS_LATCH_EN_MASK                 0x1
#define MT6311_PMIC_RG_SYS_LATCH_EN_SHIFT                3
#define MT6311_PMIC_RG_ONOFF_EN_SWCTRL_ADDR              MT6311_STRUP_CON10
#define MT6311_PMIC_RG_ONOFF_EN_SWCTRL_MASK              0x1
#define MT6311_PMIC_RG_ONOFF_EN_SWCTRL_SHIFT             4
#define MT6311_PMIC_RG_ONOFF_EN_ADDR                     MT6311_STRUP_CON10
#define MT6311_PMIC_RG_ONOFF_EN_MASK                     0x1
#define MT6311_PMIC_RG_ONOFF_EN_SHIFT                    5
#define MT6311_PMIC_RG_STRUP_PWRON_COND_SEL_ADDR         MT6311_STRUP_CON10
#define MT6311_PMIC_RG_STRUP_PWRON_COND_SEL_MASK         0x1
#define MT6311_PMIC_RG_STRUP_PWRON_COND_SEL_SHIFT        6
#define MT6311_PMIC_RG_STRUP_PWRON_COND_EN_ADDR          MT6311_STRUP_CON10
#define MT6311_PMIC_RG_STRUP_PWRON_COND_EN_MASK          0x1
#define MT6311_PMIC_RG_STRUP_PWRON_COND_EN_SHIFT         7
#define MT6311_PMIC_STRUP_PG_STATUS_ADDR                 MT6311_STRUP_CON11
#define MT6311_PMIC_STRUP_PG_STATUS_MASK                 0x1
#define MT6311_PMIC_STRUP_PG_STATUS_SHIFT                0
#define MT6311_PMIC_STRUP_PG_STATUS_CLR_ADDR             MT6311_STRUP_CON11
#define MT6311_PMIC_STRUP_PG_STATUS_CLR_MASK             0x1
#define MT6311_PMIC_STRUP_PG_STATUS_CLR_SHIFT            1
#define MT6311_PMIC_RG_RSV_SWREG_ADDR                    MT6311_STRUP_CON12
#define MT6311_PMIC_RG_RSV_SWREG_MASK                    0xFF
#define MT6311_PMIC_RG_RSV_SWREG_SHIFT                   0
#define MT6311_PMIC_VDVFS11_PG_DEB_ADDR                  MT6311_STRUP_CON13
#define MT6311_PMIC_VDVFS11_PG_DEB_MASK                  0x1
#define MT6311_PMIC_VDVFS11_PG_DEB_SHIFT                 0
#define MT6311_PMIC_VDVFS12_PG_DEB_ADDR                  MT6311_STRUP_CON13
#define MT6311_PMIC_VDVFS12_PG_DEB_MASK                  0x1
#define MT6311_PMIC_VDVFS12_PG_DEB_SHIFT                 1
#define MT6311_PMIC_VBIASN_PG_DEB_ADDR                   MT6311_STRUP_CON13
#define MT6311_PMIC_VBIASN_PG_DEB_MASK                   0x1
#define MT6311_PMIC_VBIASN_PG_DEB_SHIFT                  2
#define MT6311_PMIC_STRUP_RO_RSV0_ADDR                   MT6311_STRUP_CON13
#define MT6311_PMIC_STRUP_RO_RSV0_MASK                   0x1F
#define MT6311_PMIC_STRUP_RO_RSV0_SHIFT                  3
#define MT6311_PMIC_RG_STRUP_THR_110_CLR_ADDR            MT6311_STRUP_CON14
#define MT6311_PMIC_RG_STRUP_THR_110_CLR_MASK            0x1
#define MT6311_PMIC_RG_STRUP_THR_110_CLR_SHIFT           0
#define MT6311_PMIC_RG_STRUP_THR_125_CLR_ADDR            MT6311_STRUP_CON14
#define MT6311_PMIC_RG_STRUP_THR_125_CLR_MASK            0x1
#define MT6311_PMIC_RG_STRUP_THR_125_CLR_SHIFT           1
#define MT6311_PMIC_RG_STRUP_THR_110_IRQ_EN_ADDR         MT6311_STRUP_CON14
#define MT6311_PMIC_RG_STRUP_THR_110_IRQ_EN_MASK         0x1
#define MT6311_PMIC_RG_STRUP_THR_110_IRQ_EN_SHIFT        2
#define MT6311_PMIC_RG_STRUP_THR_125_IRQ_EN_ADDR         MT6311_STRUP_CON14
#define MT6311_PMIC_RG_STRUP_THR_125_IRQ_EN_MASK         0x1
#define MT6311_PMIC_RG_STRUP_THR_125_IRQ_EN_SHIFT        3
#define MT6311_PMIC_RG_STRUP_THR_110_IRQ_STATUS_ADDR     MT6311_STRUP_CON14
#define MT6311_PMIC_RG_STRUP_THR_110_IRQ_STATUS_MASK     0x1
#define MT6311_PMIC_RG_STRUP_THR_110_IRQ_STATUS_SHIFT    4
#define MT6311_PMIC_RG_STRUP_THR_125_IRQ_STATUS_ADDR     MT6311_STRUP_CON14
#define MT6311_PMIC_RG_STRUP_THR_125_IRQ_STATUS_MASK     0x1
#define MT6311_PMIC_RG_STRUP_THR_125_IRQ_STATUS_SHIFT    5
#define MT6311_PMIC_RG_THERMAL_EN_ADDR                   MT6311_STRUP_CON14
#define MT6311_PMIC_RG_THERMAL_EN_MASK                   0x1
#define MT6311_PMIC_RG_THERMAL_EN_SHIFT                  6
#define MT6311_PMIC_RG_THERMAL_EN_SEL_ADDR               MT6311_STRUP_CON14
#define MT6311_PMIC_RG_THERMAL_EN_SEL_MASK               0x1
#define MT6311_PMIC_RG_THERMAL_EN_SEL_SHIFT              7
#define MT6311_PMIC_RG_OSC_75K_TRIM_ADDR                 MT6311_TOP_CLK_TRIM0
#define MT6311_PMIC_RG_OSC_75K_TRIM_MASK                 0x1F
#define MT6311_PMIC_RG_OSC_75K_TRIM_SHIFT                0
#define MT6311_PMIC_OSC_75K_TRIM_ADDR                    MT6311_TOP_CLK_TRIM1
#define MT6311_PMIC_OSC_75K_TRIM_MASK                    0x1F
#define MT6311_PMIC_OSC_75K_TRIM_SHIFT                   0
#define MT6311_PMIC_RG_OSC_75K_TRIM_EN_ADDR              MT6311_TOP_CLK_TRIM1
#define MT6311_PMIC_RG_OSC_75K_TRIM_EN_MASK              0x1
#define MT6311_PMIC_RG_OSC_75K_TRIM_EN_SHIFT             5
#define MT6311_PMIC_RG_OSC_75K_TRIM_RATE_ADDR            MT6311_TOP_CLK_TRIM1
#define MT6311_PMIC_RG_OSC_75K_TRIM_RATE_MASK            0x3
#define MT6311_PMIC_RG_OSC_75K_TRIM_RATE_SHIFT           6
#define MT6311_PMIC_RG_EFUSE_ADDR_ADDR                   MT6311_EFUSE_CON0
#define MT6311_PMIC_RG_EFUSE_ADDR_MASK                   0x7F
#define MT6311_PMIC_RG_EFUSE_ADDR_SHIFT                  0
#define MT6311_PMIC_RG_EFUSE_DIN_ADDR                    MT6311_EFUSE_CON1
#define MT6311_PMIC_RG_EFUSE_DIN_MASK                    0x1
#define MT6311_PMIC_RG_EFUSE_DIN_SHIFT                   0
#define MT6311_PMIC_RG_EFUSE_DM_ADDR                     MT6311_EFUSE_CON2
#define MT6311_PMIC_RG_EFUSE_DM_MASK                     0x1
#define MT6311_PMIC_RG_EFUSE_DM_SHIFT                    0
#define MT6311_PMIC_RG_EFUSE_PGM_ADDR                    MT6311_EFUSE_CON3
#define MT6311_PMIC_RG_EFUSE_PGM_MASK                    0x1
#define MT6311_PMIC_RG_EFUSE_PGM_SHIFT                   0
#define MT6311_PMIC_RG_EFUSE_PGM_EN_ADDR                 MT6311_EFUSE_CON4
#define MT6311_PMIC_RG_EFUSE_PGM_EN_MASK                 0x1
#define MT6311_PMIC_RG_EFUSE_PGM_EN_SHIFT                0
#define MT6311_PMIC_RG_EFUSE_PROG_PKEY_ADDR              MT6311_EFUSE_CON5
#define MT6311_PMIC_RG_EFUSE_PROG_PKEY_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_PROG_PKEY_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_RD_PKEY_ADDR                MT6311_EFUSE_CON6
#define MT6311_PMIC_RG_EFUSE_RD_PKEY_MASK                0xFF
#define MT6311_PMIC_RG_EFUSE_RD_PKEY_SHIFT               0
#define MT6311_PMIC_RG_EFUSE_PGM_SRC_ADDR                MT6311_EFUSE_CON7
#define MT6311_PMIC_RG_EFUSE_PGM_SRC_MASK                0x1
#define MT6311_PMIC_RG_EFUSE_PGM_SRC_SHIFT               0
#define MT6311_PMIC_RG_EFUSE_DIN_SRC_ADDR                MT6311_EFUSE_CON8
#define MT6311_PMIC_RG_EFUSE_DIN_SRC_MASK                0x1
#define MT6311_PMIC_RG_EFUSE_DIN_SRC_SHIFT               0
#define MT6311_PMIC_RG_EFUSE_RD_TRIG_ADDR                MT6311_EFUSE_CON9
#define MT6311_PMIC_RG_EFUSE_RD_TRIG_MASK                0x1
#define MT6311_PMIC_RG_EFUSE_RD_TRIG_SHIFT               0
#define MT6311_PMIC_RG_RD_RDY_BYPASS_ADDR                MT6311_EFUSE_CON10
#define MT6311_PMIC_RG_RD_RDY_BYPASS_MASK                0x1
#define MT6311_PMIC_RG_RD_RDY_BYPASS_SHIFT               0
#define MT6311_PMIC_RG_SKIP_EFUSE_OUT_ADDR               MT6311_EFUSE_CON11
#define MT6311_PMIC_RG_SKIP_EFUSE_OUT_MASK               0x1
#define MT6311_PMIC_RG_SKIP_EFUSE_OUT_SHIFT              0
#define MT6311_PMIC_RG_EFUSE_RD_ACK_ADDR                 MT6311_EFUSE_CON12
#define MT6311_PMIC_RG_EFUSE_RD_ACK_MASK                 0x1
#define MT6311_PMIC_RG_EFUSE_RD_ACK_SHIFT                0
#define MT6311_PMIC_RG_EFUSE_RD_BUSY_ADDR                MT6311_EFUSE_CON12
#define MT6311_PMIC_RG_EFUSE_RD_BUSY_MASK                0x1
#define MT6311_PMIC_RG_EFUSE_RD_BUSY_SHIFT               2
#define MT6311_PMIC_RG_EFUSE_WRITE_MODE_ADDR             MT6311_EFUSE_CON13
#define MT6311_PMIC_RG_EFUSE_WRITE_MODE_MASK             0x1
#define MT6311_PMIC_RG_EFUSE_WRITE_MODE_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_0_7_ADDR               MT6311_EFUSE_DOUT_0_7
#define MT6311_PMIC_RG_EFUSE_DOUT_0_7_MASK               0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_0_7_SHIFT              0
#define MT6311_PMIC_RG_EFUSE_DOUT_8_15_ADDR              MT6311_EFUSE_DOUT_8_15
#define MT6311_PMIC_RG_EFUSE_DOUT_8_15_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_8_15_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_DOUT_16_23_ADDR             MT6311_EFUSE_DOUT_16_23
#define MT6311_PMIC_RG_EFUSE_DOUT_16_23_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_16_23_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_24_31_ADDR             MT6311_EFUSE_DOUT_24_31
#define MT6311_PMIC_RG_EFUSE_DOUT_24_31_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_24_31_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_32_39_ADDR             MT6311_EFUSE_DOUT_32_39
#define MT6311_PMIC_RG_EFUSE_DOUT_32_39_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_32_39_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_40_47_ADDR             MT6311_EFUSE_DOUT_40_47
#define MT6311_PMIC_RG_EFUSE_DOUT_40_47_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_40_47_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_48_55_ADDR             MT6311_EFUSE_DOUT_48_55
#define MT6311_PMIC_RG_EFUSE_DOUT_48_55_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_48_55_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_56_63_ADDR             MT6311_EFUSE_DOUT_56_63
#define MT6311_PMIC_RG_EFUSE_DOUT_56_63_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_56_63_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_64_71_ADDR             MT6311_EFUSE_DOUT_64_71
#define MT6311_PMIC_RG_EFUSE_DOUT_64_71_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_64_71_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_72_79_ADDR             MT6311_EFUSE_DOUT_72_79
#define MT6311_PMIC_RG_EFUSE_DOUT_72_79_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_72_79_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_80_87_ADDR             MT6311_EFUSE_DOUT_80_87
#define MT6311_PMIC_RG_EFUSE_DOUT_80_87_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_80_87_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_88_95_ADDR             MT6311_EFUSE_DOUT_88_95
#define MT6311_PMIC_RG_EFUSE_DOUT_88_95_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_88_95_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_DOUT_96_103_ADDR       MT6311_EFUSE_DOUT_96_103
#define MT6311_PMIC_RG_EFUSE_DOUT_96_103_MASK       0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_96_103_SHIFT      0
#define MT6311_PMIC_RG_EFUSE_DOUT_104_111_ADDR      MT6311_EFUSE_DOUT_104_111
#define MT6311_PMIC_RG_EFUSE_DOUT_104_111_MASK      0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_104_111_SHIFT     0
#define MT6311_PMIC_RG_EFUSE_DOUT_112_119_ADDR      MT6311_EFUSE_DOUT_112_119
#define MT6311_PMIC_RG_EFUSE_DOUT_112_119_MASK      0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_112_119_SHIFT     0
#define MT6311_PMIC_RG_EFUSE_DOUT_120_127_ADDR      MT6311_EFUSE_DOUT_120_127
#define MT6311_PMIC_RG_EFUSE_DOUT_120_127_MASK      0xFF
#define MT6311_PMIC_RG_EFUSE_DOUT_120_127_SHIFT     0
#define MT6311_PMIC_RG_EFUSE_VAL_0_7_ADDR                MT6311_EFUSE_VAL_0_7
#define MT6311_PMIC_RG_EFUSE_VAL_0_7_MASK                0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_0_7_SHIFT               0
#define MT6311_PMIC_RG_EFUSE_VAL_8_15_ADDR               MT6311_EFUSE_VAL_8_15
#define MT6311_PMIC_RG_EFUSE_VAL_8_15_MASK               0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_8_15_SHIFT              0
#define MT6311_PMIC_RG_EFUSE_VAL_16_23_ADDR              MT6311_EFUSE_VAL_16_23
#define MT6311_PMIC_RG_EFUSE_VAL_16_23_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_16_23_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_24_31_ADDR              MT6311_EFUSE_VAL_24_31
#define MT6311_PMIC_RG_EFUSE_VAL_24_31_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_24_31_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_32_39_ADDR              MT6311_EFUSE_VAL_32_39
#define MT6311_PMIC_RG_EFUSE_VAL_32_39_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_32_39_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_40_47_ADDR              MT6311_EFUSE_VAL_40_47
#define MT6311_PMIC_RG_EFUSE_VAL_40_47_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_40_47_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_48_55_ADDR              MT6311_EFUSE_VAL_48_55
#define MT6311_PMIC_RG_EFUSE_VAL_48_55_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_48_55_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_56_63_ADDR              MT6311_EFUSE_VAL_56_63
#define MT6311_PMIC_RG_EFUSE_VAL_56_63_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_56_63_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_64_71_ADDR              MT6311_EFUSE_VAL_64_71
#define MT6311_PMIC_RG_EFUSE_VAL_64_71_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_64_71_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_72_79_ADDR              MT6311_EFUSE_VAL_72_79
#define MT6311_PMIC_RG_EFUSE_VAL_72_79_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_72_79_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_80_87_ADDR              MT6311_EFUSE_VAL_80_87
#define MT6311_PMIC_RG_EFUSE_VAL_80_87_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_80_87_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_88_95_ADDR              MT6311_EFUSE_VAL_88_95
#define MT6311_PMIC_RG_EFUSE_VAL_88_95_MASK              0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_88_95_SHIFT             0
#define MT6311_PMIC_RG_EFUSE_VAL_96_103_ADDR             MT6311_EFUSE_VAL_96_103
#define MT6311_PMIC_RG_EFUSE_VAL_96_103_MASK             0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_96_103_SHIFT            0
#define MT6311_PMIC_RG_EFUSE_VAL_104_111_ADDR       MT6311_EFUSE_VAL_104_111
#define MT6311_PMIC_RG_EFUSE_VAL_104_111_MASK       0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_104_111_SHIFT      0
#define MT6311_PMIC_RG_EFUSE_VAL_112_119_ADDR       MT6311_EFUSE_VAL_112_119
#define MT6311_PMIC_RG_EFUSE_VAL_112_119_MASK       0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_112_119_SHIFT      0
#define MT6311_PMIC_RG_EFUSE_VAL_120_127_ADDR       MT6311_EFUSE_VAL_120_127
#define MT6311_PMIC_RG_EFUSE_VAL_120_127_MASK       0xFF
#define MT6311_PMIC_RG_EFUSE_VAL_120_127_SHIFT      0
#define MT6311_PMIC_BUCK_DIG0_RSV0_ADDR                  MT6311_BUCK_ALL_CON0
#define MT6311_PMIC_BUCK_DIG0_RSV0_MASK                  0xFF
#define MT6311_PMIC_BUCK_DIG0_RSV0_SHIFT                 0
#define MT6311_PMIC_VSLEEP_SRC0_8_ADDR                   MT6311_BUCK_ALL_CON1
#define MT6311_PMIC_VSLEEP_SRC0_8_MASK                   0x1
#define MT6311_PMIC_VSLEEP_SRC0_8_SHIFT                  0
#define MT6311_PMIC_VSLEEP_SRC1_ADDR                     MT6311_BUCK_ALL_CON1
#define MT6311_PMIC_VSLEEP_SRC1_MASK                     0xF
#define MT6311_PMIC_VSLEEP_SRC1_SHIFT                    1
#define MT6311_PMIC_VSLEEP_SRC0_7_0_ADDR                 MT6311_BUCK_ALL_CON2
#define MT6311_PMIC_VSLEEP_SRC0_7_0_MASK                 0xFF
#define MT6311_PMIC_VSLEEP_SRC0_7_0_SHIFT                0
#define MT6311_PMIC_R2R_SRC0_8_ADDR                      MT6311_BUCK_ALL_CON3
#define MT6311_PMIC_R2R_SRC0_8_MASK                      0x1
#define MT6311_PMIC_R2R_SRC0_8_SHIFT                     0
#define MT6311_PMIC_R2R_SRC1_ADDR                        MT6311_BUCK_ALL_CON3
#define MT6311_PMIC_R2R_SRC1_MASK                        0xF
#define MT6311_PMIC_R2R_SRC1_SHIFT                       1
#define MT6311_PMIC_R2R_SRC0_7_0_ADDR                    MT6311_BUCK_ALL_CON4
#define MT6311_PMIC_R2R_SRC0_7_0_MASK                    0xFF
#define MT6311_PMIC_R2R_SRC0_7_0_SHIFT                   0
#define MT6311_PMIC_BUCK_OSC_SEL_SRC0_8_ADDR             MT6311_BUCK_ALL_CON5
#define MT6311_PMIC_BUCK_OSC_SEL_SRC0_8_MASK             0x1
#define MT6311_PMIC_BUCK_OSC_SEL_SRC0_8_SHIFT            0
#define MT6311_PMIC_SRCLKEN_DLY_SRC1_ADDR                MT6311_BUCK_ALL_CON5
#define MT6311_PMIC_SRCLKEN_DLY_SRC1_MASK                0xF
#define MT6311_PMIC_SRCLKEN_DLY_SRC1_SHIFT               1
#define MT6311_PMIC_BUCK_OSC_SEL_SRC0_7_0_ADDR           MT6311_BUCK_ALL_CON6
#define MT6311_PMIC_BUCK_OSC_SEL_SRC0_7_0_MASK           0xFF
#define MT6311_PMIC_BUCK_OSC_SEL_SRC0_7_0_SHIFT          0
#define MT6311_PMIC_QI_VDVFS12_DIG_MON_ADDR              MT6311_BUCK_ALL_CON7
#define MT6311_PMIC_QI_VDVFS12_DIG_MON_MASK              0xFF
#define MT6311_PMIC_QI_VDVFS12_DIG_MON_SHIFT             0
#define MT6311_PMIC_QI_VDVFS11_DIG_MON_ADDR              MT6311_BUCK_ALL_CON8
#define MT6311_PMIC_QI_VDVFS11_DIG_MON_MASK              0xFF
#define MT6311_PMIC_QI_VDVFS11_DIG_MON_SHIFT             0
#define MT6311_PMIC_VDVFS11_OC_EN_ADDR                   MT6311_BUCK_ALL_CON9
#define MT6311_PMIC_VDVFS11_OC_EN_MASK                   0x1
#define MT6311_PMIC_VDVFS11_OC_EN_SHIFT                  0
#define MT6311_PMIC_VDVFS11_OC_DEG_EN_ADDR               MT6311_BUCK_ALL_CON9
#define MT6311_PMIC_VDVFS11_OC_DEG_EN_MASK               0x1
#define MT6311_PMIC_VDVFS11_OC_DEG_EN_SHIFT              1
#define MT6311_PMIC_VDVFS11_OC_WND_ADDR                  MT6311_BUCK_ALL_CON9
#define MT6311_PMIC_VDVFS11_OC_WND_MASK                  0x3
#define MT6311_PMIC_VDVFS11_OC_WND_SHIFT                 2
#define MT6311_PMIC_VDVFS11_OC_THD_ADDR                  MT6311_BUCK_ALL_CON9
#define MT6311_PMIC_VDVFS11_OC_THD_MASK                  0x3
#define MT6311_PMIC_VDVFS11_OC_THD_SHIFT                 6
#define MT6311_PMIC_VDVFS12_OC_EN_ADDR                   MT6311_BUCK_ALL_CON10
#define MT6311_PMIC_VDVFS12_OC_EN_MASK                   0x1
#define MT6311_PMIC_VDVFS12_OC_EN_SHIFT                  0
#define MT6311_PMIC_VDVFS12_OC_DEG_EN_ADDR               MT6311_BUCK_ALL_CON10
#define MT6311_PMIC_VDVFS12_OC_DEG_EN_MASK               0x1
#define MT6311_PMIC_VDVFS12_OC_DEG_EN_SHIFT              1
#define MT6311_PMIC_VDVFS12_OC_WND_ADDR                  MT6311_BUCK_ALL_CON10
#define MT6311_PMIC_VDVFS12_OC_WND_MASK                  0x3
#define MT6311_PMIC_VDVFS12_OC_WND_SHIFT                 2
#define MT6311_PMIC_VDVFS12_OC_THD_ADDR                  MT6311_BUCK_ALL_CON10
#define MT6311_PMIC_VDVFS12_OC_THD_MASK                  0x3
#define MT6311_PMIC_VDVFS12_OC_THD_SHIFT                 6
#define MT6311_PMIC_VDVFS11_OC_FLAG_CLR_ADDR             MT6311_BUCK_ALL_CON18
#define MT6311_PMIC_VDVFS11_OC_FLAG_CLR_MASK             0x1
#define MT6311_PMIC_VDVFS11_OC_FLAG_CLR_SHIFT            0
#define MT6311_PMIC_VDVFS12_OC_FLAG_CLR_ADDR             MT6311_BUCK_ALL_CON18
#define MT6311_PMIC_VDVFS12_OC_FLAG_CLR_MASK             0x1
#define MT6311_PMIC_VDVFS12_OC_FLAG_CLR_SHIFT            1
#define MT6311_PMIC_VDVFS11_OC_RG_STATUS_CLR_ADDR        MT6311_BUCK_ALL_CON18
#define MT6311_PMIC_VDVFS11_OC_RG_STATUS_CLR_MASK        0x1
#define MT6311_PMIC_VDVFS11_OC_RG_STATUS_CLR_SHIFT       2
#define MT6311_PMIC_VDVFS12_OC_RG_STATUS_CLR_ADDR        MT6311_BUCK_ALL_CON18
#define MT6311_PMIC_VDVFS12_OC_RG_STATUS_CLR_MASK        0x1
#define MT6311_PMIC_VDVFS12_OC_RG_STATUS_CLR_SHIFT       3
#define MT6311_PMIC_VDVFS11_OC_FLAG_CLR_SEL_ADDR         MT6311_BUCK_ALL_CON19
#define MT6311_PMIC_VDVFS11_OC_FLAG_CLR_SEL_MASK         0x1
#define MT6311_PMIC_VDVFS11_OC_FLAG_CLR_SEL_SHIFT        0
#define MT6311_PMIC_VDVFS12_OC_FLAG_CLR_SEL_ADDR         MT6311_BUCK_ALL_CON19
#define MT6311_PMIC_VDVFS12_OC_FLAG_CLR_SEL_MASK         0x1
#define MT6311_PMIC_VDVFS12_OC_FLAG_CLR_SEL_SHIFT        1
#define MT6311_PMIC_VDVFS11_OC_STATUS_ADDR               MT6311_BUCK_ALL_CON20
#define MT6311_PMIC_VDVFS11_OC_STATUS_MASK               0x1
#define MT6311_PMIC_VDVFS11_OC_STATUS_SHIFT              0
#define MT6311_PMIC_VDVFS12_OC_STATUS_ADDR               MT6311_BUCK_ALL_CON20
#define MT6311_PMIC_VDVFS12_OC_STATUS_MASK               0x1
#define MT6311_PMIC_VDVFS12_OC_STATUS_SHIFT              1
#define MT6311_PMIC_VDVFS11_OC_INT_EN_ADDR               MT6311_BUCK_ALL_CON21
#define MT6311_PMIC_VDVFS11_OC_INT_EN_MASK               0x1
#define MT6311_PMIC_VDVFS11_OC_INT_EN_SHIFT              0
#define MT6311_PMIC_VDVFS12_OC_INT_EN_ADDR               MT6311_BUCK_ALL_CON21
#define MT6311_PMIC_VDVFS12_OC_INT_EN_MASK               0x1
#define MT6311_PMIC_VDVFS12_OC_INT_EN_SHIFT              1
#define MT6311_PMIC_VDVFS11_EN_OC_SDN_SEL_ADDR           MT6311_BUCK_ALL_CON22
#define MT6311_PMIC_VDVFS11_EN_OC_SDN_SEL_MASK           0x1
#define MT6311_PMIC_VDVFS11_EN_OC_SDN_SEL_SHIFT          0
#define MT6311_PMIC_VDVFS12_EN_OC_SDN_SEL_ADDR           MT6311_BUCK_ALL_CON22
#define MT6311_PMIC_VDVFS12_EN_OC_SDN_SEL_MASK           0x1
#define MT6311_PMIC_VDVFS12_EN_OC_SDN_SEL_SHIFT          1
#define MT6311_PMIC_BUCK_TEST_MODE_ADDR                  MT6311_BUCK_ALL_CON23
#define MT6311_PMIC_BUCK_TEST_MODE_MASK                  0x1
#define MT6311_PMIC_BUCK_TEST_MODE_SHIFT                 0
#define MT6311_PMIC_BUCK_DIG1_RSV0_ADDR                  MT6311_BUCK_ALL_CON23
#define MT6311_PMIC_BUCK_DIG1_RSV0_MASK                  0x7F
#define MT6311_PMIC_BUCK_DIG1_RSV0_SHIFT                 1
#define MT6311_PMIC_QI_VDVFS11_VSLEEP_ADDR               MT6311_BUCK_ALL_CON24
#define MT6311_PMIC_QI_VDVFS11_VSLEEP_MASK               0x7
#define MT6311_PMIC_QI_VDVFS11_VSLEEP_SHIFT              0
#define MT6311_PMIC_QI_VDVFS12_VSLEEP_ADDR               MT6311_BUCK_ALL_CON24
#define MT6311_PMIC_QI_VDVFS12_VSLEEP_MASK               0x7
#define MT6311_PMIC_QI_VDVFS12_VSLEEP_SHIFT              3
#define MT6311_PMIC_BUCK_ANA_DIG0_RSV0_ADDR              MT6311_ANA_RSV_CON0
#define MT6311_PMIC_BUCK_ANA_DIG0_RSV0_MASK              0xFF
#define MT6311_PMIC_BUCK_ANA_DIG0_RSV0_SHIFT             0
#define MT6311_PMIC_RG_THRDET_SEL_ADDR                   MT6311_STRUP_ANA_CON0
#define MT6311_PMIC_RG_THRDET_SEL_MASK                   0x1
#define MT6311_PMIC_RG_THRDET_SEL_SHIFT                  0
#define MT6311_PMIC_RG_STRUP_THR_SEL_ADDR                MT6311_STRUP_ANA_CON0
#define MT6311_PMIC_RG_STRUP_THR_SEL_MASK                0x3
#define MT6311_PMIC_RG_STRUP_THR_SEL_SHIFT               1
#define MT6311_PMIC_RG_THR_TMODE_ADDR                    MT6311_STRUP_ANA_CON0
#define MT6311_PMIC_RG_THR_TMODE_MASK                    0x1
#define MT6311_PMIC_RG_THR_TMODE_SHIFT                   3
#define MT6311_PMIC_RG_STRUP_IREF_TRIM_ADDR              MT6311_STRUP_ANA_CON1
#define MT6311_PMIC_RG_STRUP_IREF_TRIM_MASK              0x1F
#define MT6311_PMIC_RG_STRUP_IREF_TRIM_SHIFT             0
#define MT6311_PMIC_RG_UVLO_VTHL_ADDR                    MT6311_STRUP_ANA_CON1
#define MT6311_PMIC_RG_UVLO_VTHL_MASK                    0x3
#define MT6311_PMIC_RG_UVLO_VTHL_SHIFT                   5
#define MT6311_PMIC_RG_UVLO_VTHH_ADDR                    MT6311_STRUP_ANA_CON2
#define MT6311_PMIC_RG_UVLO_VTHH_MASK                    0x3
#define MT6311_PMIC_RG_UVLO_VTHH_SHIFT                   0
#define MT6311_PMIC_RG_BGR_UNCHOP_ADDR                   MT6311_STRUP_ANA_CON2
#define MT6311_PMIC_RG_BGR_UNCHOP_MASK                   0x1
#define MT6311_PMIC_RG_BGR_UNCHOP_SHIFT                  2
#define MT6311_PMIC_RG_BGR_UNCHOP_PH_ADDR                MT6311_STRUP_ANA_CON2
#define MT6311_PMIC_RG_BGR_UNCHOP_PH_MASK                0x1
#define MT6311_PMIC_RG_BGR_UNCHOP_PH_SHIFT               3
#define MT6311_PMIC_RG_BGR_RSEL_ADDR                     MT6311_STRUP_ANA_CON2
#define MT6311_PMIC_RG_BGR_RSEL_MASK                     0x7
#define MT6311_PMIC_RG_BGR_RSEL_SHIFT                    4
#define MT6311_PMIC_RG_BGR_TRIM_ADDR                     MT6311_STRUP_ANA_CON3
#define MT6311_PMIC_RG_BGR_TRIM_MASK                     0x1F
#define MT6311_PMIC_RG_BGR_TRIM_SHIFT                    0
#define MT6311_PMIC_RG_BGR_TEST_EN_ADDR                  MT6311_STRUP_ANA_CON3
#define MT6311_PMIC_RG_BGR_TEST_EN_MASK                  0x1
#define MT6311_PMIC_RG_BGR_TEST_EN_SHIFT                 5
#define MT6311_PMIC_RG_BGR_TEST_RSTB_ADDR                MT6311_STRUP_ANA_CON3
#define MT6311_PMIC_RG_BGR_TEST_RSTB_MASK                0x1
#define MT6311_PMIC_RG_BGR_TEST_RSTB_SHIFT               6
#define MT6311_PMIC_RG_VDVFS11_TRIMH_ADDR                MT6311_STRUP_ANA_CON4
#define MT6311_PMIC_RG_VDVFS11_TRIMH_MASK                0x1F
#define MT6311_PMIC_RG_VDVFS11_TRIMH_SHIFT               0
#define MT6311_PMIC_RG_VDVFS11_TRIML_ADDR                MT6311_STRUP_ANA_CON5
#define MT6311_PMIC_RG_VDVFS11_TRIML_MASK                0x1F
#define MT6311_PMIC_RG_VDVFS11_TRIML_SHIFT               0
#define MT6311_PMIC_RG_VDVFS12_TRIMH_ADDR                MT6311_STRUP_ANA_CON6
#define MT6311_PMIC_RG_VDVFS12_TRIMH_MASK                0x1F
#define MT6311_PMIC_RG_VDVFS12_TRIMH_SHIFT               0
#define MT6311_PMIC_RG_VDVFS12_TRIML_ADDR                MT6311_STRUP_ANA_CON7
#define MT6311_PMIC_RG_VDVFS12_TRIML_MASK                0x1F
#define MT6311_PMIC_RG_VDVFS12_TRIML_SHIFT               0
#define MT6311_PMIC_RG_VDVFS11_VSLEEP_ADDR               MT6311_STRUP_ANA_CON7
#define MT6311_PMIC_RG_VDVFS11_VSLEEP_MASK               0x7
#define MT6311_PMIC_RG_VDVFS11_VSLEEP_SHIFT              5
#define MT6311_PMIC_RG_VDVFS12_VSLEEP_ADDR               MT6311_STRUP_ANA_CON8
#define MT6311_PMIC_RG_VDVFS12_VSLEEP_MASK               0x7
#define MT6311_PMIC_RG_VDVFS12_VSLEEP_SHIFT              0
#define MT6311_PMIC_RG_BGR_OSC_CAL_ADDR                  MT6311_STRUP_ANA_CON9
#define MT6311_PMIC_RG_BGR_OSC_CAL_MASK                  0x3F
#define MT6311_PMIC_RG_BGR_OSC_CAL_SHIFT                 0
#define MT6311_PMIC_RG_STRUP_RSV_ADDR                    MT6311_STRUP_ANA_CON10
#define MT6311_PMIC_RG_STRUP_RSV_MASK                    0xFF
#define MT6311_PMIC_RG_STRUP_RSV_SHIFT                   0
#define MT6311_PMIC_RG_VREF_LP_MODE_ADDR                 MT6311_STRUP_ANA_CON11
#define MT6311_PMIC_RG_VREF_LP_MODE_MASK                 0x1
#define MT6311_PMIC_RG_VREF_LP_MODE_SHIFT                0
#define MT6311_PMIC_RG_TESTMODE_SWEN_ADDR                MT6311_STRUP_ANA_CON11
#define MT6311_PMIC_RG_TESTMODE_SWEN_MASK                0x1
#define MT6311_PMIC_RG_TESTMODE_SWEN_SHIFT               1
#define MT6311_PMIC_RG_VDIG18_VOSEL_ADDR                 MT6311_STRUP_ANA_CON11
#define MT6311_PMIC_RG_VDIG18_VOSEL_MASK                 0x7
#define MT6311_PMIC_RG_VDIG18_VOSEL_SHIFT                2
#define MT6311_PMIC_RG_VDIG18_CAL_ADDR                   MT6311_STRUP_ANA_CON12
#define MT6311_PMIC_RG_VDIG18_CAL_MASK                   0xF
#define MT6311_PMIC_RG_VDIG18_CAL_SHIFT                  0
#define MT6311_PMIC_RG_OSC_SEL_ADDR                      MT6311_STRUP_ANA_CON12
#define MT6311_PMIC_RG_OSC_SEL_MASK                      0x1
#define MT6311_PMIC_RG_OSC_SEL_SHIFT                     4
#define MT6311_PMIC_RG_VBIASN_NDIS_EN_ADDR               MT6311_VBIASN_ANA_CON0
#define MT6311_PMIC_RG_VBIASN_NDIS_EN_MASK               0x1
#define MT6311_PMIC_RG_VBIASN_NDIS_EN_SHIFT              0
#define MT6311_PMIC_RG_VBIASN_VOSEL_ADDR                 MT6311_VBIASN_ANA_CON0
#define MT6311_PMIC_RG_VBIASN_VOSEL_MASK                 0x1F
#define MT6311_PMIC_RG_VBIASN_VOSEL_SHIFT                1
#define MT6311_PMIC_RG_VDVFS11_RC_ADDR                   MT6311_VDVFS1_ANA_CON0
#define MT6311_PMIC_RG_VDVFS11_RC_MASK                   0xF
#define MT6311_PMIC_RG_VDVFS11_RC_SHIFT                  0
#define MT6311_PMIC_RG_VDVFS12_RC_ADDR                   MT6311_VDVFS1_ANA_CON0
#define MT6311_PMIC_RG_VDVFS12_RC_MASK                   0xF
#define MT6311_PMIC_RG_VDVFS12_RC_SHIFT                  4
#define MT6311_PMIC_RG_VDVFS11_CSR_ADDR                  MT6311_VDVFS1_ANA_CON1
#define MT6311_PMIC_RG_VDVFS11_CSR_MASK                  0x3
#define MT6311_PMIC_RG_VDVFS11_CSR_SHIFT                 0
#define MT6311_PMIC_RG_VDVFS12_CSR_ADDR                  MT6311_VDVFS1_ANA_CON1
#define MT6311_PMIC_RG_VDVFS12_CSR_MASK                  0x3
#define MT6311_PMIC_RG_VDVFS12_CSR_SHIFT                 2
#define MT6311_PMIC_RG_VDVFS11_PFM_CSR_ADDR              MT6311_VDVFS1_ANA_CON1
#define MT6311_PMIC_RG_VDVFS11_PFM_CSR_MASK              0x3
#define MT6311_PMIC_RG_VDVFS11_PFM_CSR_SHIFT             4
#define MT6311_PMIC_RG_VDVFS12_PFM_CSR_ADDR              MT6311_VDVFS1_ANA_CON1
#define MT6311_PMIC_RG_VDVFS12_PFM_CSR_MASK              0x3
#define MT6311_PMIC_RG_VDVFS12_PFM_CSR_SHIFT             6
#define MT6311_PMIC_RG_VDVFS11_SLP_ADDR                  MT6311_VDVFS1_ANA_CON2
#define MT6311_PMIC_RG_VDVFS11_SLP_MASK                  0x3
#define MT6311_PMIC_RG_VDVFS11_SLP_SHIFT                 0
#define MT6311_PMIC_RG_VDVFS12_SLP_ADDR                  MT6311_VDVFS1_ANA_CON2
#define MT6311_PMIC_RG_VDVFS12_SLP_MASK                  0x3
#define MT6311_PMIC_RG_VDVFS12_SLP_SHIFT                 2
#define MT6311_PMIC_RG_VDVFS11_UVP_EN_ADDR               MT6311_VDVFS1_ANA_CON2
#define MT6311_PMIC_RG_VDVFS11_UVP_EN_MASK               0x1
#define MT6311_PMIC_RG_VDVFS11_UVP_EN_SHIFT              4
#define MT6311_PMIC_RG_VDVFS12_UVP_EN_ADDR               MT6311_VDVFS1_ANA_CON2
#define MT6311_PMIC_RG_VDVFS12_UVP_EN_MASK               0x1
#define MT6311_PMIC_RG_VDVFS12_UVP_EN_SHIFT              5
#define MT6311_PMIC_RG_VDVFS11_MODESET_ADDR              MT6311_VDVFS1_ANA_CON2
#define MT6311_PMIC_RG_VDVFS11_MODESET_MASK              0x1
#define MT6311_PMIC_RG_VDVFS11_MODESET_SHIFT             6
#define MT6311_PMIC_RG_VDVFS12_MODESET_ADDR              MT6311_VDVFS1_ANA_CON2
#define MT6311_PMIC_RG_VDVFS12_MODESET_MASK              0x1
#define MT6311_PMIC_RG_VDVFS12_MODESET_SHIFT             7
#define MT6311_PMIC_RG_VDVFS11_NDIS_EN_ADDR              MT6311_VDVFS1_ANA_CON3
#define MT6311_PMIC_RG_VDVFS11_NDIS_EN_MASK              0x1
#define MT6311_PMIC_RG_VDVFS11_NDIS_EN_SHIFT             0
#define MT6311_PMIC_RG_VDVFS12_NDIS_EN_ADDR              MT6311_VDVFS1_ANA_CON3
#define MT6311_PMIC_RG_VDVFS12_NDIS_EN_MASK              0x1
#define MT6311_PMIC_RG_VDVFS12_NDIS_EN_SHIFT             1
#define MT6311_PMIC_RG_VDVFS11_TRANS_BST_ADDR            MT6311_VDVFS1_ANA_CON4
#define MT6311_PMIC_RG_VDVFS11_TRANS_BST_MASK            0xFF
#define MT6311_PMIC_RG_VDVFS11_TRANS_BST_SHIFT           0
#define MT6311_PMIC_RG_VDVFS12_TRANS_BST_ADDR            MT6311_VDVFS1_ANA_CON5
#define MT6311_PMIC_RG_VDVFS12_TRANS_BST_MASK            0xFF
#define MT6311_PMIC_RG_VDVFS12_TRANS_BST_SHIFT           0
#define MT6311_PMIC_RG_VDVFS11_CSM_N_ADDR                MT6311_VDVFS1_ANA_CON6
#define MT6311_PMIC_RG_VDVFS11_CSM_N_MASK                0xF
#define MT6311_PMIC_RG_VDVFS11_CSM_N_SHIFT               0
#define MT6311_PMIC_RG_VDVFS11_CSM_P_ADDR                MT6311_VDVFS1_ANA_CON6
#define MT6311_PMIC_RG_VDVFS11_CSM_P_MASK                0xF
#define MT6311_PMIC_RG_VDVFS11_CSM_P_SHIFT               4
#define MT6311_PMIC_RG_VDVFS12_CSM_N_ADDR                MT6311_VDVFS1_ANA_CON7
#define MT6311_PMIC_RG_VDVFS12_CSM_N_MASK                0xF
#define MT6311_PMIC_RG_VDVFS12_CSM_N_SHIFT               0
#define MT6311_PMIC_RG_VDVFS12_CSM_P_ADDR                MT6311_VDVFS1_ANA_CON7
#define MT6311_PMIC_RG_VDVFS12_CSM_P_MASK                0xF
#define MT6311_PMIC_RG_VDVFS12_CSM_P_SHIFT               4
#define MT6311_PMIC_RG_VDVFS11_ZXOS_TRIM_ADDR            MT6311_VDVFS1_ANA_CON8
#define MT6311_PMIC_RG_VDVFS11_ZXOS_TRIM_MASK            0x3F
#define MT6311_PMIC_RG_VDVFS11_ZXOS_TRIM_SHIFT           0
#define MT6311_PMIC_RG_VDVFS12_ZXOS_TRIM_ADDR            MT6311_VDVFS1_ANA_CON9
#define MT6311_PMIC_RG_VDVFS12_ZXOS_TRIM_MASK            0x3F
#define MT6311_PMIC_RG_VDVFS12_ZXOS_TRIM_SHIFT           0
#define MT6311_PMIC_RG_VDVFS11_OC_OFF_ADDR               MT6311_VDVFS1_ANA_CON9
#define MT6311_PMIC_RG_VDVFS11_OC_OFF_MASK               0x1
#define MT6311_PMIC_RG_VDVFS11_OC_OFF_SHIFT              6
#define MT6311_PMIC_RG_VDVFS12_OC_OFF_ADDR               MT6311_VDVFS1_ANA_CON9
#define MT6311_PMIC_RG_VDVFS12_OC_OFF_MASK               0x1
#define MT6311_PMIC_RG_VDVFS12_OC_OFF_SHIFT              7
#define MT6311_PMIC_RG_VDVFS11_PHS_SHED_TRIM_ADDR        MT6311_VDVFS1_ANA_CON10
#define MT6311_PMIC_RG_VDVFS11_PHS_SHED_TRIM_MASK        0xF
#define MT6311_PMIC_RG_VDVFS11_PHS_SHED_TRIM_SHIFT       0
#define MT6311_PMIC_RG_VDVFS11_F2PHS_ADDR                MT6311_VDVFS1_ANA_CON10
#define MT6311_PMIC_RG_VDVFS11_F2PHS_MASK                0x1
#define MT6311_PMIC_RG_VDVFS11_F2PHS_SHIFT               4
#define MT6311_PMIC_RG_VDVFS11_RS_FORCE_OFF_ADDR         MT6311_VDVFS1_ANA_CON10
#define MT6311_PMIC_RG_VDVFS11_RS_FORCE_OFF_MASK         0x1
#define MT6311_PMIC_RG_VDVFS11_RS_FORCE_OFF_SHIFT        5
#define MT6311_PMIC_RG_VDVFS12_RS_FORCE_OFF_ADDR         MT6311_VDVFS1_ANA_CON10
#define MT6311_PMIC_RG_VDVFS12_RS_FORCE_OFF_MASK         0x1
#define MT6311_PMIC_RG_VDVFS12_RS_FORCE_OFF_SHIFT        6
#define MT6311_PMIC_RG_VDVFS11_TM_EN_ADDR                MT6311_VDVFS1_ANA_CON10
#define MT6311_PMIC_RG_VDVFS11_TM_EN_MASK                0x1
#define MT6311_PMIC_RG_VDVFS11_TM_EN_SHIFT               7
#define MT6311_PMIC_RG_VDVFS11_TM_UGSNS_ADDR             MT6311_VDVFS1_ANA_CON11
#define MT6311_PMIC_RG_VDVFS11_TM_UGSNS_MASK             0x1
#define MT6311_PMIC_RG_VDVFS11_TM_UGSNS_SHIFT            0
#define MT6311_PMIC_RG_VDVFS1_FBN_SEL_ADDR               MT6311_VDVFS1_ANA_CON11
#define MT6311_PMIC_RG_VDVFS1_FBN_SEL_MASK               0x1
#define MT6311_PMIC_RG_VDVFS1_FBN_SEL_SHIFT              1
#define MT6311_PMIC_RGS_VDVFS11_ENPWM_STATUS_ADDR        MT6311_VDVFS1_ANA_CON12
#define MT6311_PMIC_RGS_VDVFS11_ENPWM_STATUS_MASK        0x1
#define MT6311_PMIC_RGS_VDVFS11_ENPWM_STATUS_SHIFT       0
#define MT6311_PMIC_RGS_VDVFS12_ENPWM_STATUS_ADDR        MT6311_VDVFS1_ANA_CON12
#define MT6311_PMIC_RGS_VDVFS12_ENPWM_STATUS_MASK        0x1
#define MT6311_PMIC_RGS_VDVFS12_ENPWM_STATUS_SHIFT       1
#define MT6311_PMIC_NI_VDVFS1_COUNT_ADDR                 MT6311_VDVFS1_ANA_CON12
#define MT6311_PMIC_NI_VDVFS1_COUNT_MASK                 0x1
#define MT6311_PMIC_NI_VDVFS1_COUNT_SHIFT                2
#define MT6311_PMIC_VDVFS11_DIG0_RSV0_ADDR               MT6311_VDVFS11_CON0
#define MT6311_PMIC_VDVFS11_DIG0_RSV0_MASK               0xFF
#define MT6311_PMIC_VDVFS11_DIG0_RSV0_SHIFT              0
#define MT6311_PMIC_VDVFS11_EN_CTRL_ADDR                 MT6311_VDVFS11_CON7
#define MT6311_PMIC_VDVFS11_EN_CTRL_MASK                 0x1
#define MT6311_PMIC_VDVFS11_EN_CTRL_SHIFT                0
#define MT6311_PMIC_VDVFS11_VOSEL_CTRL_ADDR              MT6311_VDVFS11_CON7
#define MT6311_PMIC_VDVFS11_VOSEL_CTRL_MASK              0x1
#define MT6311_PMIC_VDVFS11_VOSEL_CTRL_SHIFT             1
#define MT6311_PMIC_VDVFS11_DIG0_RSV1_ADDR               MT6311_VDVFS11_CON7
#define MT6311_PMIC_VDVFS11_DIG0_RSV1_MASK               0x1
#define MT6311_PMIC_VDVFS11_DIG0_RSV1_SHIFT              2
#define MT6311_PMIC_VDVFS11_BURST_CTRL_ADDR              MT6311_VDVFS11_CON7
#define MT6311_PMIC_VDVFS11_BURST_CTRL_MASK              0x1
#define MT6311_PMIC_VDVFS11_BURST_CTRL_SHIFT             3
#define MT6311_PMIC_VDVFS11_EN_SEL_ADDR                  MT6311_VDVFS11_CON8
#define MT6311_PMIC_VDVFS11_EN_SEL_MASK                  0x3
#define MT6311_PMIC_VDVFS11_EN_SEL_SHIFT                 0
#define MT6311_PMIC_VDVFS11_VOSEL_SEL_ADDR               MT6311_VDVFS11_CON8
#define MT6311_PMIC_VDVFS11_VOSEL_SEL_MASK               0x3
#define MT6311_PMIC_VDVFS11_VOSEL_SEL_SHIFT              2
#define MT6311_PMIC_VDVFS11_DIG0_RSV2_ADDR               MT6311_VDVFS11_CON8
#define MT6311_PMIC_VDVFS11_DIG0_RSV2_MASK               0x3
#define MT6311_PMIC_VDVFS11_DIG0_RSV2_SHIFT              4
#define MT6311_PMIC_VDVFS11_BURST_SEL_ADDR               MT6311_VDVFS11_CON8
#define MT6311_PMIC_VDVFS11_BURST_SEL_MASK               0x3
#define MT6311_PMIC_VDVFS11_BURST_SEL_SHIFT              6
#define MT6311_PMIC_VDVFS11_EN_ADDR                      MT6311_VDVFS11_CON9
#define MT6311_PMIC_VDVFS11_EN_MASK                      0x1
#define MT6311_PMIC_VDVFS11_EN_SHIFT                     0
#define MT6311_PMIC_VDVFS11_STBTD_ADDR                   MT6311_VDVFS11_CON9
#define MT6311_PMIC_VDVFS11_STBTD_MASK                   0x3
#define MT6311_PMIC_VDVFS11_STBTD_SHIFT                  1
#define MT6311_PMIC_QI_VDVFS11_STB_ADDR                  MT6311_VDVFS11_CON9
#define MT6311_PMIC_QI_VDVFS11_STB_MASK                  0x1
#define MT6311_PMIC_QI_VDVFS11_STB_SHIFT                 3
#define MT6311_PMIC_QI_VDVFS11_EN_ADDR                   MT6311_VDVFS11_CON9
#define MT6311_PMIC_QI_VDVFS11_EN_MASK                   0x1
#define MT6311_PMIC_QI_VDVFS11_EN_SHIFT                  4
#define MT6311_PMIC_QI_VDVFS11_OC_STATUS_ADDR            MT6311_VDVFS11_CON9
#define MT6311_PMIC_QI_VDVFS11_OC_STATUS_MASK            0x1
#define MT6311_PMIC_QI_VDVFS11_OC_STATUS_SHIFT           7
#define MT6311_PMIC_VDVFS11_SFCHG_RRATE_ADDR             MT6311_VDVFS11_CON10
#define MT6311_PMIC_VDVFS11_SFCHG_RRATE_MASK             0x7F
#define MT6311_PMIC_VDVFS11_SFCHG_RRATE_SHIFT            0
#define MT6311_PMIC_VDVFS11_SFCHG_REN_ADDR               MT6311_VDVFS11_CON10
#define MT6311_PMIC_VDVFS11_SFCHG_REN_MASK               0x1
#define MT6311_PMIC_VDVFS11_SFCHG_REN_SHIFT              7
#define MT6311_PMIC_VDVFS11_SFCHG_FRATE_ADDR             MT6311_VDVFS11_CON11
#define MT6311_PMIC_VDVFS11_SFCHG_FRATE_MASK             0x7F
#define MT6311_PMIC_VDVFS11_SFCHG_FRATE_SHIFT            0
#define MT6311_PMIC_VDVFS11_SFCHG_FEN_ADDR               MT6311_VDVFS11_CON11
#define MT6311_PMIC_VDVFS11_SFCHG_FEN_MASK               0x1
#define MT6311_PMIC_VDVFS11_SFCHG_FEN_SHIFT              7
#define MT6311_PMIC_VDVFS11_VOSEL_ADDR                   MT6311_VDVFS11_CON12
#define MT6311_PMIC_VDVFS11_VOSEL_MASK                   0x7F
#define MT6311_PMIC_VDVFS11_VOSEL_SHIFT                  0
#define MT6311_PMIC_VDVFS11_VOSEL_ON_ADDR                MT6311_VDVFS11_CON13
#define MT6311_PMIC_VDVFS11_VOSEL_ON_MASK                0x7F
#define MT6311_PMIC_VDVFS11_VOSEL_ON_SHIFT               0
#define MT6311_PMIC_VDVFS11_VOSEL_SLEEP_ADDR             MT6311_VDVFS11_CON14
#define MT6311_PMIC_VDVFS11_VOSEL_SLEEP_MASK             0x7F
#define MT6311_PMIC_VDVFS11_VOSEL_SLEEP_SHIFT            0
#define MT6311_PMIC_NI_VDVFS11_VOSEL_ADDR                MT6311_VDVFS11_CON15
#define MT6311_PMIC_NI_VDVFS11_VOSEL_MASK                0x7F
#define MT6311_PMIC_NI_VDVFS11_VOSEL_SHIFT               0
#define MT6311_PMIC_VDVFS11_BURST_SLEEP_ADDR             MT6311_VDVFS11_CON16
#define MT6311_PMIC_VDVFS11_BURST_SLEEP_MASK             0x7
#define MT6311_PMIC_VDVFS11_BURST_SLEEP_SHIFT            0
#define MT6311_PMIC_QI_VDVFS11_BURST_ADDR                MT6311_VDVFS11_CON16
#define MT6311_PMIC_QI_VDVFS11_BURST_MASK                0x7
#define MT6311_PMIC_QI_VDVFS11_BURST_SHIFT               4
#define MT6311_PMIC_VDVFS11_BURST_ADDR                   MT6311_VDVFS11_CON17
#define MT6311_PMIC_VDVFS11_BURST_MASK                   0x7
#define MT6311_PMIC_VDVFS11_BURST_SHIFT                  0
#define MT6311_PMIC_VDVFS11_BURST_ON_ADDR                MT6311_VDVFS11_CON17
#define MT6311_PMIC_VDVFS11_BURST_ON_MASK                0x7
#define MT6311_PMIC_VDVFS11_BURST_ON_SHIFT               4
#define MT6311_PMIC_VDVFS11_VSLEEP_EN_ADDR               MT6311_VDVFS11_CON18
#define MT6311_PMIC_VDVFS11_VSLEEP_EN_MASK               0x1
#define MT6311_PMIC_VDVFS11_VSLEEP_EN_SHIFT              0
#define MT6311_PMIC_VDVFS11_R2R_PDN_ADDR                 MT6311_VDVFS11_CON18
#define MT6311_PMIC_VDVFS11_R2R_PDN_MASK                 0x1
#define MT6311_PMIC_VDVFS11_R2R_PDN_SHIFT                1
#define MT6311_PMIC_VDVFS11_VSLEEP_SEL_ADDR              MT6311_VDVFS11_CON18
#define MT6311_PMIC_VDVFS11_VSLEEP_SEL_MASK              0x1
#define MT6311_PMIC_VDVFS11_VSLEEP_SEL_SHIFT             2
#define MT6311_PMIC_NI_VDVFS11_R2R_PDN_ADDR              MT6311_VDVFS11_CON18
#define MT6311_PMIC_NI_VDVFS11_R2R_PDN_MASK              0x1
#define MT6311_PMIC_NI_VDVFS11_R2R_PDN_SHIFT             3
#define MT6311_PMIC_NI_VDVFS11_VSLEEP_SEL_ADDR           MT6311_VDVFS11_CON18
#define MT6311_PMIC_NI_VDVFS11_VSLEEP_SEL_MASK           0x1
#define MT6311_PMIC_NI_VDVFS11_VSLEEP_SEL_SHIFT          4
#define MT6311_PMIC_VDVFS11_TRANS_TD_ADDR                MT6311_VDVFS11_CON19
#define MT6311_PMIC_VDVFS11_TRANS_TD_MASK                0x3
#define MT6311_PMIC_VDVFS11_TRANS_TD_SHIFT               0
#define MT6311_PMIC_VDVFS11_TRANS_CTRL_ADDR              MT6311_VDVFS11_CON19
#define MT6311_PMIC_VDVFS11_TRANS_CTRL_MASK              0x3
#define MT6311_PMIC_VDVFS11_TRANS_CTRL_SHIFT             4
#define MT6311_PMIC_VDVFS11_TRANS_ONCE_ADDR              MT6311_VDVFS11_CON19
#define MT6311_PMIC_VDVFS11_TRANS_ONCE_MASK              0x1
#define MT6311_PMIC_VDVFS11_TRANS_ONCE_SHIFT             6
#define MT6311_PMIC_NI_VDVFS11_VOSEL_TRANS_ADDR          MT6311_VDVFS11_CON19
#define MT6311_PMIC_NI_VDVFS11_VOSEL_TRANS_MASK          0x1
#define MT6311_PMIC_NI_VDVFS11_VOSEL_TRANS_SHIFT         7
#define MT6311_PMIC_VDVFS12_DIG0_RSV0_ADDR               MT6311_VDVFS12_CON0
#define MT6311_PMIC_VDVFS12_DIG0_RSV0_MASK               0xFF
#define MT6311_PMIC_VDVFS12_DIG0_RSV0_SHIFT              0
#define MT6311_PMIC_VDVFS12_EN_CTRL_ADDR                 MT6311_VDVFS12_CON7
#define MT6311_PMIC_VDVFS12_EN_CTRL_MASK                 0x1
#define MT6311_PMIC_VDVFS12_EN_CTRL_SHIFT                0
#define MT6311_PMIC_VDVFS12_VOSEL_CTRL_ADDR              MT6311_VDVFS12_CON7
#define MT6311_PMIC_VDVFS12_VOSEL_CTRL_MASK              0x1
#define MT6311_PMIC_VDVFS12_VOSEL_CTRL_SHIFT             1
#define MT6311_PMIC_VDVFS12_DIG0_RSV1_ADDR               MT6311_VDVFS12_CON7
#define MT6311_PMIC_VDVFS12_DIG0_RSV1_MASK               0x1
#define MT6311_PMIC_VDVFS12_DIG0_RSV1_SHIFT              2
#define MT6311_PMIC_VDVFS12_BURST_CTRL_ADDR              MT6311_VDVFS12_CON7
#define MT6311_PMIC_VDVFS12_BURST_CTRL_MASK              0x1
#define MT6311_PMIC_VDVFS12_BURST_CTRL_SHIFT             3
#define MT6311_PMIC_VDVFS12_EN_SEL_ADDR                  MT6311_VDVFS12_CON8
#define MT6311_PMIC_VDVFS12_EN_SEL_MASK                  0x3
#define MT6311_PMIC_VDVFS12_EN_SEL_SHIFT                 0
#define MT6311_PMIC_VDVFS12_VOSEL_SEL_ADDR               MT6311_VDVFS12_CON8
#define MT6311_PMIC_VDVFS12_VOSEL_SEL_MASK               0x3
#define MT6311_PMIC_VDVFS12_VOSEL_SEL_SHIFT              2
#define MT6311_PMIC_VDVFS12_DIG0_RSV2_ADDR               MT6311_VDVFS12_CON8
#define MT6311_PMIC_VDVFS12_DIG0_RSV2_MASK               0x3
#define MT6311_PMIC_VDVFS12_DIG0_RSV2_SHIFT              4
#define MT6311_PMIC_VDVFS12_BURST_SEL_ADDR               MT6311_VDVFS12_CON8
#define MT6311_PMIC_VDVFS12_BURST_SEL_MASK               0x3
#define MT6311_PMIC_VDVFS12_BURST_SEL_SHIFT              6
#define MT6311_PMIC_VDVFS12_EN_ADDR                      MT6311_VDVFS12_CON9
#define MT6311_PMIC_VDVFS12_EN_MASK                      0x1
#define MT6311_PMIC_VDVFS12_EN_SHIFT                     0
#define MT6311_PMIC_VDVFS12_STBTD_ADDR                   MT6311_VDVFS12_CON9
#define MT6311_PMIC_VDVFS12_STBTD_MASK                   0x3
#define MT6311_PMIC_VDVFS12_STBTD_SHIFT                  1
#define MT6311_PMIC_QI_VDVFS12_STB_ADDR                  MT6311_VDVFS12_CON9
#define MT6311_PMIC_QI_VDVFS12_STB_MASK                  0x1
#define MT6311_PMIC_QI_VDVFS12_STB_SHIFT                 3
#define MT6311_PMIC_QI_VDVFS12_EN_ADDR                   MT6311_VDVFS12_CON9
#define MT6311_PMIC_QI_VDVFS12_EN_MASK                   0x1
#define MT6311_PMIC_QI_VDVFS12_EN_SHIFT                  4
#define MT6311_PMIC_QI_VDVFS12_OC_STATUS_ADDR            MT6311_VDVFS12_CON9
#define MT6311_PMIC_QI_VDVFS12_OC_STATUS_MASK            0x1
#define MT6311_PMIC_QI_VDVFS12_OC_STATUS_SHIFT           7
#define MT6311_PMIC_VDVFS12_SFCHG_RRATE_ADDR             MT6311_VDVFS12_CON10
#define MT6311_PMIC_VDVFS12_SFCHG_RRATE_MASK             0x7F
#define MT6311_PMIC_VDVFS12_SFCHG_RRATE_SHIFT            0
#define MT6311_PMIC_VDVFS12_SFCHG_REN_ADDR               MT6311_VDVFS12_CON10
#define MT6311_PMIC_VDVFS12_SFCHG_REN_MASK               0x1
#define MT6311_PMIC_VDVFS12_SFCHG_REN_SHIFT              7
#define MT6311_PMIC_VDVFS12_SFCHG_FRATE_ADDR             MT6311_VDVFS12_CON11
#define MT6311_PMIC_VDVFS12_SFCHG_FRATE_MASK             0x7F
#define MT6311_PMIC_VDVFS12_SFCHG_FRATE_SHIFT            0
#define MT6311_PMIC_VDVFS12_SFCHG_FEN_ADDR               MT6311_VDVFS12_CON11
#define MT6311_PMIC_VDVFS12_SFCHG_FEN_MASK               0x1
#define MT6311_PMIC_VDVFS12_SFCHG_FEN_SHIFT              7
#define MT6311_PMIC_VDVFS12_VOSEL_ADDR                   MT6311_VDVFS12_CON12
#define MT6311_PMIC_VDVFS12_VOSEL_MASK                   0x7F
#define MT6311_PMIC_VDVFS12_VOSEL_SHIFT                  0
#define MT6311_PMIC_VDVFS12_VOSEL_ON_ADDR                MT6311_VDVFS12_CON13
#define MT6311_PMIC_VDVFS12_VOSEL_ON_MASK                0x7F
#define MT6311_PMIC_VDVFS12_VOSEL_ON_SHIFT               0
#define MT6311_PMIC_VDVFS12_VOSEL_SLEEP_ADDR             MT6311_VDVFS12_CON14
#define MT6311_PMIC_VDVFS12_VOSEL_SLEEP_MASK             0x7F
#define MT6311_PMIC_VDVFS12_VOSEL_SLEEP_SHIFT            0
#define MT6311_PMIC_NI_VDVFS12_VOSEL_ADDR                MT6311_VDVFS12_CON15
#define MT6311_PMIC_NI_VDVFS12_VOSEL_MASK                0x7F
#define MT6311_PMIC_NI_VDVFS12_VOSEL_SHIFT               0
#define MT6311_PMIC_VDVFS12_BURST_SLEEP_ADDR             MT6311_VDVFS12_CON16
#define MT6311_PMIC_VDVFS12_BURST_SLEEP_MASK             0x7
#define MT6311_PMIC_VDVFS12_BURST_SLEEP_SHIFT            0
#define MT6311_PMIC_QI_VDVFS12_BURST_ADDR                MT6311_VDVFS12_CON16
#define MT6311_PMIC_QI_VDVFS12_BURST_MASK                0x7
#define MT6311_PMIC_QI_VDVFS12_BURST_SHIFT               4
#define MT6311_PMIC_VDVFS12_BURST_ADDR                   MT6311_VDVFS12_CON17
#define MT6311_PMIC_VDVFS12_BURST_MASK                   0x7
#define MT6311_PMIC_VDVFS12_BURST_SHIFT                  0
#define MT6311_PMIC_VDVFS12_BURST_ON_ADDR                MT6311_VDVFS12_CON17
#define MT6311_PMIC_VDVFS12_BURST_ON_MASK                0x7
#define MT6311_PMIC_VDVFS12_BURST_ON_SHIFT               4
#define MT6311_PMIC_VDVFS12_VSLEEP_EN_ADDR               MT6311_VDVFS12_CON18
#define MT6311_PMIC_VDVFS12_VSLEEP_EN_MASK               0x1
#define MT6311_PMIC_VDVFS12_VSLEEP_EN_SHIFT              0
#define MT6311_PMIC_VDVFS12_R2R_PDN_ADDR                 MT6311_VDVFS12_CON18
#define MT6311_PMIC_VDVFS12_R2R_PDN_MASK                 0x1
#define MT6311_PMIC_VDVFS12_R2R_PDN_SHIFT                1
#define MT6311_PMIC_VDVFS12_VSLEEP_SEL_ADDR              MT6311_VDVFS12_CON18
#define MT6311_PMIC_VDVFS12_VSLEEP_SEL_MASK              0x1
#define MT6311_PMIC_VDVFS12_VSLEEP_SEL_SHIFT             2
#define MT6311_PMIC_NI_VDVFS12_R2R_PDN_ADDR              MT6311_VDVFS12_CON18
#define MT6311_PMIC_NI_VDVFS12_R2R_PDN_MASK              0x1
#define MT6311_PMIC_NI_VDVFS12_R2R_PDN_SHIFT             3
#define MT6311_PMIC_NI_VDVFS12_VSLEEP_SEL_ADDR           MT6311_VDVFS12_CON18
#define MT6311_PMIC_NI_VDVFS12_VSLEEP_SEL_MASK           0x1
#define MT6311_PMIC_NI_VDVFS12_VSLEEP_SEL_SHIFT          4
#define MT6311_PMIC_VDVFS12_TRANS_TD_ADDR                MT6311_VDVFS12_CON19
#define MT6311_PMIC_VDVFS12_TRANS_TD_MASK                0x3
#define MT6311_PMIC_VDVFS12_TRANS_TD_SHIFT               0
#define MT6311_PMIC_VDVFS12_TRANS_CTRL_ADDR              MT6311_VDVFS12_CON19
#define MT6311_PMIC_VDVFS12_TRANS_CTRL_MASK              0x3
#define MT6311_PMIC_VDVFS12_TRANS_CTRL_SHIFT             4
#define MT6311_PMIC_VDVFS12_TRANS_ONCE_ADDR              MT6311_VDVFS12_CON19
#define MT6311_PMIC_VDVFS12_TRANS_ONCE_MASK              0x1
#define MT6311_PMIC_VDVFS12_TRANS_ONCE_SHIFT             6
#define MT6311_PMIC_NI_VDVFS12_VOSEL_TRANS_ADDR          MT6311_VDVFS12_CON19
#define MT6311_PMIC_NI_VDVFS12_VOSEL_TRANS_MASK          0x1
#define MT6311_PMIC_NI_VDVFS12_VOSEL_TRANS_SHIFT         7
#define MT6311_PMIC_K_RST_DONE_ADDR                      MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_RST_DONE_MASK                      0x1
#define MT6311_PMIC_K_RST_DONE_SHIFT                     0
#define MT6311_PMIC_K_MAP_SEL_ADDR                       MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_MAP_SEL_MASK                       0x1
#define MT6311_PMIC_K_MAP_SEL_SHIFT                      1
#define MT6311_PMIC_K_ONCE_EN_ADDR                       MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_ONCE_EN_MASK                       0x1
#define MT6311_PMIC_K_ONCE_EN_SHIFT                      2
#define MT6311_PMIC_K_ONCE_ADDR                          MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_ONCE_MASK                          0x1
#define MT6311_PMIC_K_ONCE_SHIFT                         3
#define MT6311_PMIC_K_START_MANUAL_ADDR                  MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_START_MANUAL_MASK                  0x1
#define MT6311_PMIC_K_START_MANUAL_SHIFT                 4
#define MT6311_PMIC_K_SRC_SEL_ADDR                       MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_SRC_SEL_MASK                       0x1
#define MT6311_PMIC_K_SRC_SEL_SHIFT                      5
#define MT6311_PMIC_K_AUTO_EN_ADDR                       MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_AUTO_EN_MASK                       0x1
#define MT6311_PMIC_K_AUTO_EN_SHIFT                      6
#define MT6311_PMIC_K_INV_ADDR                           MT6311_BUCK_K_CON0
#define MT6311_PMIC_K_INV_MASK                           0x1
#define MT6311_PMIC_K_INV_SHIFT                          7
#define MT6311_PMIC_K_CONTROL_SMPS_ADDR                  MT6311_BUCK_K_CON1
#define MT6311_PMIC_K_CONTROL_SMPS_MASK                  0x3F
#define MT6311_PMIC_K_CONTROL_SMPS_SHIFT                 0
#define MT6311_PMIC_QI_SMPS_OSC_CAL_ADDR                 MT6311_BUCK_K_CON2
#define MT6311_PMIC_QI_SMPS_OSC_CAL_MASK                 0x3F
#define MT6311_PMIC_QI_SMPS_OSC_CAL_SHIFT                0
#define MT6311_PMIC_K_RESULT_ADDR                        MT6311_BUCK_K_CON3
#define MT6311_PMIC_K_RESULT_MASK                        0x1
#define MT6311_PMIC_K_RESULT_SHIFT                       0
#define MT6311_PMIC_K_DONE_ADDR                          MT6311_BUCK_K_CON3
#define MT6311_PMIC_K_DONE_MASK                          0x1
#define MT6311_PMIC_K_DONE_SHIFT                         1
#define MT6311_PMIC_K_CONTROL_ADDR                       MT6311_BUCK_K_CON3
#define MT6311_PMIC_K_CONTROL_MASK                       0x3F
#define MT6311_PMIC_K_CONTROL_SHIFT                      2
#define MT6311_PMIC_K_BUCK_CK_CNT_8_ADDR                 MT6311_BUCK_K_CON4
#define MT6311_PMIC_K_BUCK_CK_CNT_8_MASK                 0x1
#define MT6311_PMIC_K_BUCK_CK_CNT_8_SHIFT                0
#define MT6311_PMIC_K_BUCK_CK_CNT_7_0_ADDR               MT6311_BUCK_K_CON5
#define MT6311_PMIC_K_BUCK_CK_CNT_7_0_MASK               0xFF
#define MT6311_PMIC_K_BUCK_CK_CNT_7_0_SHIFT              0
#define MT6311_PMIC_AUXADC_ADC_OUT_CH0_ADDR              MT6311_AUXADC_ADC0
#define MT6311_PMIC_AUXADC_ADC_OUT_CH0_MASK              0x3F
#define MT6311_PMIC_AUXADC_ADC_OUT_CH0_SHIFT             0
#define MT6311_PMIC_AUXADC_ADC_RDY_CH0_ADDR              MT6311_AUXADC_ADC0
#define MT6311_PMIC_AUXADC_ADC_RDY_CH0_MASK              0x1
#define MT6311_PMIC_AUXADC_ADC_RDY_CH0_SHIFT             7
#define MT6311_PMIC_AUXADC_ADC_OUT_CH1_ADDR              MT6311_AUXADC_ADC1
#define MT6311_PMIC_AUXADC_ADC_OUT_CH1_MASK              0x3F
#define MT6311_PMIC_AUXADC_ADC_OUT_CH1_SHIFT             0
#define MT6311_PMIC_AUXADC_ADC_RDY_CH1_ADDR              MT6311_AUXADC_ADC1
#define MT6311_PMIC_AUXADC_ADC_RDY_CH1_MASK              0x1
#define MT6311_PMIC_AUXADC_ADC_RDY_CH1_SHIFT             7
#define MT6311_PMIC_AUXADC_ADC_OUT_CSM_ADDR              MT6311_AUXADC_ADC2
#define MT6311_PMIC_AUXADC_ADC_OUT_CSM_MASK              0x3F
#define MT6311_PMIC_AUXADC_ADC_OUT_CSM_SHIFT             0
#define MT6311_PMIC_AUXADC_ADC_RDY_CSM_ADDR              MT6311_AUXADC_ADC2
#define MT6311_PMIC_AUXADC_ADC_RDY_CSM_MASK              0x1
#define MT6311_PMIC_AUXADC_ADC_RDY_CSM_SHIFT             7
#define MT6311_PMIC_AUXADC_ADC_OUT_DIV2_ADDR             MT6311_AUXADC_ADC3
#define MT6311_PMIC_AUXADC_ADC_OUT_DIV2_MASK             0x3F
#define MT6311_PMIC_AUXADC_ADC_OUT_DIV2_SHIFT            0
#define MT6311_PMIC_AUXADC_ADC_RDY_DIV2_ADDR             MT6311_AUXADC_ADC3
#define MT6311_PMIC_AUXADC_ADC_RDY_DIV2_MASK             0x1
#define MT6311_PMIC_AUXADC_ADC_RDY_DIV2_SHIFT            7
#define MT6311_PMIC_AUXADC_ADC_BUSY_IN_ADDR              MT6311_AUXADC_STA0
#define MT6311_PMIC_AUXADC_ADC_BUSY_IN_MASK              0xFF
#define MT6311_PMIC_AUXADC_ADC_BUSY_IN_SHIFT             0
#define MT6311_PMIC_AUXADC_RQST_CH0_ADDR                 MT6311_AUXADC_RQST0
#define MT6311_PMIC_AUXADC_RQST_CH0_MASK                 0x1
#define MT6311_PMIC_AUXADC_RQST_CH0_SHIFT                0
#define MT6311_PMIC_AUXADC_RQST_CH1_ADDR                 MT6311_AUXADC_RQST0
#define MT6311_PMIC_AUXADC_RQST_CH1_MASK                 0x1
#define MT6311_PMIC_AUXADC_RQST_CH1_SHIFT                1
#define MT6311_PMIC_AUXADC_RQST_CH2_ADDR                 MT6311_AUXADC_RQST0
#define MT6311_PMIC_AUXADC_RQST_CH2_MASK                 0x1
#define MT6311_PMIC_AUXADC_RQST_CH2_SHIFT                2
#define MT6311_PMIC_AUXADC_EN_CSM_SW_ADDR                MT6311_AUXADC_CON0
#define MT6311_PMIC_AUXADC_EN_CSM_SW_MASK                0x1
#define MT6311_PMIC_AUXADC_EN_CSM_SW_SHIFT               0
#define MT6311_PMIC_AUXADC_EN_CSM_SEL_ADDR               MT6311_AUXADC_CON0
#define MT6311_PMIC_AUXADC_EN_CSM_SEL_MASK               0x1
#define MT6311_PMIC_AUXADC_EN_CSM_SEL_SHIFT              1
#define MT6311_PMIC_RG_TEST_AUXADC_ADDR                  MT6311_AUXADC_CON0
#define MT6311_PMIC_RG_TEST_AUXADC_MASK                  0x1
#define MT6311_PMIC_RG_TEST_AUXADC_SHIFT                 2
#define MT6311_PMIC_AUXADC_CK_AON_GPS_ADDR               MT6311_AUXADC_CON0
#define MT6311_PMIC_AUXADC_CK_AON_GPS_MASK               0x1
#define MT6311_PMIC_AUXADC_CK_AON_GPS_SHIFT              3
#define MT6311_PMIC_AUXADC_CK_AON_MD_ADDR                MT6311_AUXADC_CON0
#define MT6311_PMIC_AUXADC_CK_AON_MD_MASK                0x1
#define MT6311_PMIC_AUXADC_CK_AON_MD_SHIFT               4
#define MT6311_PMIC_AUXADC_CK_AON_ADDR                   MT6311_AUXADC_CON0
#define MT6311_PMIC_AUXADC_CK_AON_MASK                   0x1
#define MT6311_PMIC_AUXADC_CK_AON_SHIFT                  5
#define MT6311_PMIC_AUXADC_CK_ON_EXTD_ADDR               MT6311_AUXADC_CON1
#define MT6311_PMIC_AUXADC_CK_ON_EXTD_MASK               0x3F
#define MT6311_PMIC_AUXADC_CK_ON_EXTD_SHIFT              0
#define MT6311_PMIC_AUXADC_SPL_NUM_ADDR                  MT6311_AUXADC_CON2
#define MT6311_PMIC_AUXADC_SPL_NUM_MASK                  0xFF
#define MT6311_PMIC_AUXADC_SPL_NUM_SHIFT                 0
#define MT6311_PMIC_AUXADC_AVG_NUM_SMALL_ADDR            MT6311_AUXADC_CON3
#define MT6311_PMIC_AUXADC_AVG_NUM_SMALL_MASK            0x7
#define MT6311_PMIC_AUXADC_AVG_NUM_SMALL_SHIFT           0
#define MT6311_PMIC_AUXADC_AVG_NUM_LARGE_ADDR            MT6311_AUXADC_CON3
#define MT6311_PMIC_AUXADC_AVG_NUM_LARGE_MASK            0x7
#define MT6311_PMIC_AUXADC_AVG_NUM_LARGE_SHIFT           3
#define MT6311_PMIC_AUXADC_AVG_NUM_SEL_ADDR              MT6311_AUXADC_CON4
#define MT6311_PMIC_AUXADC_AVG_NUM_SEL_MASK              0xFF
#define MT6311_PMIC_AUXADC_AVG_NUM_SEL_SHIFT             0
#define MT6311_PMIC_AUXADC_TRIM_CH0_SEL_ADDR             MT6311_AUXADC_CON5
#define MT6311_PMIC_AUXADC_TRIM_CH0_SEL_MASK             0x3
#define MT6311_PMIC_AUXADC_TRIM_CH0_SEL_SHIFT            0
#define MT6311_PMIC_AUXADC_TRIM_CH1_SEL_ADDR             MT6311_AUXADC_CON5
#define MT6311_PMIC_AUXADC_TRIM_CH1_SEL_MASK             0x3
#define MT6311_PMIC_AUXADC_TRIM_CH1_SEL_SHIFT            2
#define MT6311_PMIC_AUXADC_TRIM_CH2_SEL_ADDR             MT6311_AUXADC_CON5
#define MT6311_PMIC_AUXADC_TRIM_CH2_SEL_MASK             0x3
#define MT6311_PMIC_AUXADC_TRIM_CH2_SEL_SHIFT            4
#define MT6311_PMIC_AUXADC_TRIM_CH3_SEL_ADDR             MT6311_AUXADC_CON5
#define MT6311_PMIC_AUXADC_TRIM_CH3_SEL_MASK             0x3
#define MT6311_PMIC_AUXADC_TRIM_CH3_SEL_SHIFT            6
#define MT6311_PMIC_AUXADC_CON6_RSV0_ADDR                MT6311_AUXADC_CON6
#define MT6311_PMIC_AUXADC_CON6_RSV0_MASK                0x1
#define MT6311_PMIC_AUXADC_CON6_RSV0_SHIFT               0
#define MT6311_PMIC_RG_ADC_2S_COMP_ENB_ADDR              MT6311_AUXADC_CON6
#define MT6311_PMIC_RG_ADC_2S_COMP_ENB_MASK              0x1
#define MT6311_PMIC_RG_ADC_2S_COMP_ENB_SHIFT             1
#define MT6311_PMIC_RG_ADC_TRIM_COMP_ADDR                MT6311_AUXADC_CON6
#define MT6311_PMIC_RG_ADC_TRIM_COMP_MASK                0x1
#define MT6311_PMIC_RG_ADC_TRIM_COMP_SHIFT               2
#define MT6311_PMIC_AUXADC_OUT_SEL_ADDR                  MT6311_AUXADC_CON6
#define MT6311_PMIC_AUXADC_OUT_SEL_MASK                  0x1
#define MT6311_PMIC_AUXADC_OUT_SEL_SHIFT                 3
#define MT6311_PMIC_AUXADC_ADC_PWDB_SWCTRL_ADDR          MT6311_AUXADC_CON6
#define MT6311_PMIC_AUXADC_ADC_PWDB_SWCTRL_MASK          0x1
#define MT6311_PMIC_AUXADC_ADC_PWDB_SWCTRL_SHIFT         4
#define MT6311_PMIC_AUXADC_QI_VDVFS1_CSM_EN_SW_ADDR      MT6311_AUXADC_CON6
#define MT6311_PMIC_AUXADC_QI_VDVFS1_CSM_EN_SW_MASK      0x1
#define MT6311_PMIC_AUXADC_QI_VDVFS1_CSM_EN_SW_SHIFT     5
#define MT6311_PMIC_AUXADC_QI_VDVFS11_CSM_EN_ADDR        MT6311_AUXADC_CON6
#define MT6311_PMIC_AUXADC_QI_VDVFS11_CSM_EN_MASK        0x1
#define MT6311_PMIC_AUXADC_QI_VDVFS11_CSM_EN_SHIFT       6
#define MT6311_PMIC_AUXADC_QI_VDVFS12_CSM_EN_ADDR        MT6311_AUXADC_CON6
#define MT6311_PMIC_AUXADC_QI_VDVFS12_CSM_EN_MASK        0x1
#define MT6311_PMIC_AUXADC_QI_VDVFS12_CSM_EN_SHIFT       7
#define MT6311_PMIC_AUXADC_SW_GAIN_TRIM_ADDR             MT6311_AUXADC_CON7
#define MT6311_PMIC_AUXADC_SW_GAIN_TRIM_MASK             0xFF
#define MT6311_PMIC_AUXADC_SW_GAIN_TRIM_SHIFT            0
#define MT6311_PMIC_AUXADC_SW_OFFSET_TRIM_ADDR           MT6311_AUXADC_CON8
#define MT6311_PMIC_AUXADC_SW_OFFSET_TRIM_MASK           0xFF
#define MT6311_PMIC_AUXADC_SW_OFFSET_TRIM_SHIFT          0
#define MT6311_PMIC_AUXADC_RNG_EN_ADDR                   MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_RNG_EN_MASK                   0x1
#define MT6311_PMIC_AUXADC_RNG_EN_SHIFT                  0
#define MT6311_PMIC_AUXADC_DATA_REUSE_SEL_ADDR           MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_DATA_REUSE_SEL_MASK           0x3
#define MT6311_PMIC_AUXADC_DATA_REUSE_SEL_SHIFT          1
#define MT6311_PMIC_AUXADC_TEST_MODE_ADDR                MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_TEST_MODE_MASK                0x1
#define MT6311_PMIC_AUXADC_TEST_MODE_SHIFT               3
#define MT6311_PMIC_AUXADC_BIT_SEL_ADDR                  MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_BIT_SEL_MASK                  0x1
#define MT6311_PMIC_AUXADC_BIT_SEL_SHIFT                 4
#define MT6311_PMIC_AUXADC_START_SW_ADDR                 MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_START_SW_MASK                 0x1
#define MT6311_PMIC_AUXADC_START_SW_SHIFT                5
#define MT6311_PMIC_AUXADC_START_SWCTRL_ADDR             MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_START_SWCTRL_MASK             0x1
#define MT6311_PMIC_AUXADC_START_SWCTRL_SHIFT            6
#define MT6311_PMIC_AUXADC_ADC_PWDB_ADDR                 MT6311_AUXADC_CON9
#define MT6311_PMIC_AUXADC_ADC_PWDB_MASK                 0x1
#define MT6311_PMIC_AUXADC_ADC_PWDB_SHIFT                7
#define MT6311_PMIC_AD_AUXADC_COMP_ADDR                  MT6311_AUXADC_CON10
#define MT6311_PMIC_AD_AUXADC_COMP_MASK                  0x1
#define MT6311_PMIC_AD_AUXADC_COMP_SHIFT                 0
#define MT6311_PMIC_AUXADC_DA_DAC_SWCTRL_ADDR            MT6311_AUXADC_CON10
#define MT6311_PMIC_AUXADC_DA_DAC_SWCTRL_MASK            0x1
#define MT6311_PMIC_AUXADC_DA_DAC_SWCTRL_SHIFT           1
#define MT6311_PMIC_AUXADC_DA_DAC_ADDR                   MT6311_AUXADC_CON11
#define MT6311_PMIC_AUXADC_DA_DAC_MASK                   0xFF
#define MT6311_PMIC_AUXADC_DA_DAC_SHIFT                  0
#define MT6311_PMIC_AUXADC_SWCTRL_EN_ADDR                MT6311_AUXADC_CON12
#define MT6311_PMIC_AUXADC_SWCTRL_EN_MASK                0x1
#define MT6311_PMIC_AUXADC_SWCTRL_EN_SHIFT               0
#define MT6311_PMIC_AUXADC_CHSEL_ADDR                    MT6311_AUXADC_CON12
#define MT6311_PMIC_AUXADC_CHSEL_MASK                    0xF
#define MT6311_PMIC_AUXADC_CHSEL_SHIFT                   1
#define MT6311_PMIC_AUXADC_ADCIN_BATON_TED_EN_ADDR       MT6311_AUXADC_CON12
#define MT6311_PMIC_AUXADC_ADCIN_BATON_TED_EN_MASK       0x1
#define MT6311_PMIC_AUXADC_ADCIN_BATON_TED_EN_SHIFT      5
#define MT6311_PMIC_AUXADC_ADCIN_CHRIN_EN_ADDR           MT6311_AUXADC_CON13
#define MT6311_PMIC_AUXADC_ADCIN_CHRIN_EN_MASK           0x1
#define MT6311_PMIC_AUXADC_ADCIN_CHRIN_EN_SHIFT          0
#define MT6311_PMIC_AUXADC_ADCIN_BATSNS_EN_ADDR          MT6311_AUXADC_CON13
#define MT6311_PMIC_AUXADC_ADCIN_BATSNS_EN_MASK          0x1
#define MT6311_PMIC_AUXADC_ADCIN_BATSNS_EN_SHIFT         1
#define MT6311_PMIC_AUXADC_ADCIN_CS_EN_ADDR              MT6311_AUXADC_CON13
#define MT6311_PMIC_AUXADC_ADCIN_CS_EN_MASK              0x1
#define MT6311_PMIC_AUXADC_ADCIN_CS_EN_SHIFT             2
#define MT6311_PMIC_AUXADC_DAC_EXTD_EN_ADDR              MT6311_AUXADC_CON14
#define MT6311_PMIC_AUXADC_DAC_EXTD_EN_MASK              0x1
#define MT6311_PMIC_AUXADC_DAC_EXTD_EN_SHIFT             0
#define MT6311_PMIC_AUXADC_DAC_EXTD_ADDR                 MT6311_AUXADC_CON14
#define MT6311_PMIC_AUXADC_DAC_EXTD_MASK                 0xF
#define MT6311_PMIC_AUXADC_DAC_EXTD_SHIFT                1
#define MT6311_PMIC_AUXADC_DIG1_RSV1_ADDR                MT6311_AUXADC_CON14
#define MT6311_PMIC_AUXADC_DIG1_RSV1_MASK                0x7
#define MT6311_PMIC_AUXADC_DIG1_RSV1_SHIFT               5
#define MT6311_PMIC_AUXADC_DIG0_RSV1_ADDR                MT6311_AUXADC_CON15
#define MT6311_PMIC_AUXADC_DIG0_RSV1_MASK                0xF
#define MT6311_PMIC_AUXADC_DIG0_RSV1_SHIFT               0
#define MT6311_PMIC_AUXADC_RO_RSV1_ADDR                  MT6311_AUXADC_CON15
#define MT6311_PMIC_AUXADC_RO_RSV1_MASK                  0x1
#define MT6311_PMIC_AUXADC_RO_RSV1_SHIFT                 4
#define MT6311_PMIC_LBAT_MAX_IRQ_ADDR                    MT6311_AUXADC_CON15
#define MT6311_PMIC_LBAT_MAX_IRQ_MASK                    0x1
#define MT6311_PMIC_LBAT_MAX_IRQ_SHIFT                   5
#define MT6311_PMIC_LBAT_MIN_IRQ_ADDR                    MT6311_AUXADC_CON15
#define MT6311_PMIC_LBAT_MIN_IRQ_MASK                    0x1
#define MT6311_PMIC_LBAT_MIN_IRQ_SHIFT                   6
#define MT6311_PMIC_AUXADC_AUTORPT_EN_ADDR               MT6311_AUXADC_CON15
#define MT6311_PMIC_AUXADC_AUTORPT_EN_MASK               0x1
#define MT6311_PMIC_AUXADC_AUTORPT_EN_SHIFT              7
#define MT6311_PMIC_AUXADC_AUTORPT_PRD_ADDR              MT6311_AUXADC_CON16
#define MT6311_PMIC_AUXADC_AUTORPT_PRD_MASK              0xFF
#define MT6311_PMIC_AUXADC_AUTORPT_PRD_SHIFT             0
#define MT6311_PMIC_AUXADC_LBAT_DEBT_MIN_ADDR            MT6311_AUXADC_CON17
#define MT6311_PMIC_AUXADC_LBAT_DEBT_MIN_MASK            0xFF
#define MT6311_PMIC_AUXADC_LBAT_DEBT_MIN_SHIFT           0
#define MT6311_PMIC_AUXADC_LBAT_DEBT_MAX_ADDR            MT6311_AUXADC_CON18
#define MT6311_PMIC_AUXADC_LBAT_DEBT_MAX_MASK            0xFF
#define MT6311_PMIC_AUXADC_LBAT_DEBT_MAX_SHIFT           0
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_7_0_ADDR         MT6311_AUXADC_CON19
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_7_0_MASK         0xFF
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_7_0_SHIFT        0
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_15_8_ADDR        MT6311_AUXADC_CON20
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_15_8_MASK        0xFF
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_15_8_SHIFT       0
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_19_16_ADDR       MT6311_AUXADC_CON21
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_19_16_MASK       0xF
#define MT6311_PMIC_AUXADC_LBAT_DET_PRD_19_16_SHIFT      0
#define MT6311_PMIC_AUXADC_LBAT_MAX_IRQ_B_ADDR           MT6311_AUXADC_CON22
#define MT6311_PMIC_AUXADC_LBAT_MAX_IRQ_B_MASK           0x1
#define MT6311_PMIC_AUXADC_LBAT_MAX_IRQ_B_SHIFT          0
#define MT6311_PMIC_AUXADC_LBAT_EN_MAX_ADDR              MT6311_AUXADC_CON22
#define MT6311_PMIC_AUXADC_LBAT_EN_MAX_MASK              0x1
#define MT6311_PMIC_AUXADC_LBAT_EN_MAX_SHIFT             1
#define MT6311_PMIC_AUXADC_LBAT_IRQ_EN_MAX_ADDR          MT6311_AUXADC_CON22
#define MT6311_PMIC_AUXADC_LBAT_IRQ_EN_MAX_MASK          0x1
#define MT6311_PMIC_AUXADC_LBAT_IRQ_EN_MAX_SHIFT         2
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MAX_0_ADDR          MT6311_AUXADC_CON22
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MAX_0_MASK          0xF
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MAX_0_SHIFT         3
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MAX_1_ADDR          MT6311_AUXADC_CON23
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MAX_1_MASK          0xFF
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MAX_1_SHIFT         0
#define MT6311_PMIC_AUXADC_LBAT_MIN_IRQ_B_ADDR           MT6311_AUXADC_CON24
#define MT6311_PMIC_AUXADC_LBAT_MIN_IRQ_B_MASK           0x1
#define MT6311_PMIC_AUXADC_LBAT_MIN_IRQ_B_SHIFT          0
#define MT6311_PMIC_AUXADC_LBAT_EN_MIN_ADDR              MT6311_AUXADC_CON24
#define MT6311_PMIC_AUXADC_LBAT_EN_MIN_MASK              0x1
#define MT6311_PMIC_AUXADC_LBAT_EN_MIN_SHIFT             1
#define MT6311_PMIC_AUXADC_LBAT_IRQ_EN_MIN_ADDR          MT6311_AUXADC_CON24
#define MT6311_PMIC_AUXADC_LBAT_IRQ_EN_MIN_MASK          0x1
#define MT6311_PMIC_AUXADC_LBAT_IRQ_EN_MIN_SHIFT         2
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MIN_0_ADDR          MT6311_AUXADC_CON24
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MIN_0_MASK          0xF
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MIN_0_SHIFT         3
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MIN_1_ADDR          MT6311_AUXADC_CON25
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MIN_1_MASK          0xFF
#define MT6311_PMIC_AUXADC_LBAT_VOLT_MIN_1_SHIFT         0
#define MT6311_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX_ADDR  MT6311_AUXADC_CON26
#define MT6311_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX_MASK  0xFF
#define MT6311_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX_SHIFT 0
#define MT6311_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN_ADDR  MT6311_AUXADC_CON27
#define MT6311_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN_MASK  0xFF
#define MT6311_PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN_SHIFT 0
#define MT6311_PMIC_AUXADC_ENPWM1_SEL_ADDR               MT6311_AUXADC_CON28
#define MT6311_PMIC_AUXADC_ENPWM1_SEL_MASK               0x1
#define MT6311_PMIC_AUXADC_ENPWM1_SEL_SHIFT              0
#define MT6311_PMIC_AUXADC_ENPWM1_SW_ADDR                MT6311_AUXADC_CON28
#define MT6311_PMIC_AUXADC_ENPWM1_SW_MASK                0x1
#define MT6311_PMIC_AUXADC_ENPWM1_SW_SHIFT               1
#define MT6311_PMIC_AUXADC_ENPWM2_SEL_ADDR               MT6311_AUXADC_CON28
#define MT6311_PMIC_AUXADC_ENPWM2_SEL_MASK               0x1
#define MT6311_PMIC_AUXADC_ENPWM2_SEL_SHIFT              2
#define MT6311_PMIC_AUXADC_ENPWM2_SW_ADDR                MT6311_AUXADC_CON28
#define MT6311_PMIC_AUXADC_ENPWM2_SW_MASK                0x1
#define MT6311_PMIC_AUXADC_ENPWM2_SW_SHIFT               3
#define MT6311_PMIC_QI_VBIASN_OC_STATUS_ADDR             MT6311_LDO_CON0
#define MT6311_PMIC_QI_VBIASN_OC_STATUS_MASK             0x1
#define MT6311_PMIC_QI_VBIASN_OC_STATUS_SHIFT            0
#define MT6311_PMIC_RG_VBIASN_ON_CTRL_ADDR               MT6311_LDO_CON0
#define MT6311_PMIC_RG_VBIASN_ON_CTRL_MASK               0x1
#define MT6311_PMIC_RG_VBIASN_ON_CTRL_SHIFT              1
#define MT6311_PMIC_RG_VBIASN_MODE_SET_ADDR              MT6311_LDO_CON0
#define MT6311_PMIC_RG_VBIASN_MODE_SET_MASK              0x1
#define MT6311_PMIC_RG_VBIASN_MODE_SET_SHIFT             2
#define MT6311_PMIC_RG_VBIASN_MODE_CTRL_ADDR             MT6311_LDO_CON0
#define MT6311_PMIC_RG_VBIASN_MODE_CTRL_MASK             0x1
#define MT6311_PMIC_RG_VBIASN_MODE_CTRL_SHIFT            3
#define MT6311_PMIC_RG_VBIASN_STBTD_ADDR                 MT6311_LDO_CON0
#define MT6311_PMIC_RG_VBIASN_STBTD_MASK                 0x3
#define MT6311_PMIC_RG_VBIASN_STBTD_SHIFT                4
#define MT6311_PMIC_QI_VBIASN_MODE_ADDR                  MT6311_LDO_CON0
#define MT6311_PMIC_QI_VBIASN_MODE_MASK                  0x1
#define MT6311_PMIC_QI_VBIASN_MODE_SHIFT                 6
#define MT6311_PMIC_QI_VBIASN_EN_ADDR                    MT6311_LDO_CON0
#define MT6311_PMIC_QI_VBIASN_EN_MASK                    0x1
#define MT6311_PMIC_QI_VBIASN_EN_SHIFT                   7
#define MT6311_PMIC_QI_VBIASN_OCFB_EN_ADDR               MT6311_LDO_OCFB0
#define MT6311_PMIC_QI_VBIASN_OCFB_EN_MASK               0x1
#define MT6311_PMIC_QI_VBIASN_OCFB_EN_SHIFT              3
#define MT6311_PMIC_RG_VBIASN_OCFB_EN_ADDR               MT6311_LDO_OCFB0
#define MT6311_PMIC_RG_VBIASN_OCFB_EN_MASK               0x1
#define MT6311_PMIC_RG_VBIASN_OCFB_EN_SHIFT              5
#define MT6311_PMIC_LDO_DEGTD_SEL_ADDR                   MT6311_LDO_OCFB0
#define MT6311_PMIC_LDO_DEGTD_SEL_MASK                   0x3
#define MT6311_PMIC_LDO_DEGTD_SEL_SHIFT                  6
#define MT6311_PMIC_RG_VBIASN_DIS_SEL_ADDR               MT6311_LDO_CON2
#define MT6311_PMIC_RG_VBIASN_DIS_SEL_MASK               0x3
#define MT6311_PMIC_RG_VBIASN_DIS_SEL_SHIFT              0
#define MT6311_PMIC_RG_VBIASN_TRANS_EN_ADDR              MT6311_LDO_CON2
#define MT6311_PMIC_RG_VBIASN_TRANS_EN_MASK              0x1
#define MT6311_PMIC_RG_VBIASN_TRANS_EN_SHIFT             2
#define MT6311_PMIC_RG_VBIASN_TRANS_CTRL_ADDR            MT6311_LDO_CON2
#define MT6311_PMIC_RG_VBIASN_TRANS_CTRL_MASK            0x3
#define MT6311_PMIC_RG_VBIASN_TRANS_CTRL_SHIFT           4
#define MT6311_PMIC_RG_VBIASN_TRANS_ONCE_ADDR            MT6311_LDO_CON2
#define MT6311_PMIC_RG_VBIASN_TRANS_ONCE_MASK            0x1
#define MT6311_PMIC_RG_VBIASN_TRANS_ONCE_SHIFT           6
#define MT6311_PMIC_QI_VBIASN_CHR_ADDR                   MT6311_LDO_CON2
#define MT6311_PMIC_QI_VBIASN_CHR_MASK                   0x1
#define MT6311_PMIC_QI_VBIASN_CHR_SHIFT                  7
#define MT6311_PMIC_RG_VBIASN_EN_ADDR                    MT6311_LDO_CON3
#define MT6311_PMIC_RG_VBIASN_EN_MASK                    0x1
#define MT6311_PMIC_RG_VBIASN_EN_SHIFT                   0
#define MT6311_PMIC_LDO_RSV_ADDR                         MT6311_LDO_CON4
#define MT6311_PMIC_LDO_RSV_MASK                         0xFF
#define MT6311_PMIC_LDO_RSV_SHIFT                        0
#define MT6311_PMIC_FQMTR_TCKSEL_ADDR                    MT6311_FQMTR_CON0
#define MT6311_PMIC_FQMTR_TCKSEL_MASK                    0x7
#define MT6311_PMIC_FQMTR_TCKSEL_SHIFT                   0
#define MT6311_PMIC_FQMTR_BUSY_ADDR                      MT6311_FQMTR_CON0
#define MT6311_PMIC_FQMTR_BUSY_MASK                      0x1
#define MT6311_PMIC_FQMTR_BUSY_SHIFT                     3
#define MT6311_PMIC_FQMTR_EN_ADDR                        MT6311_FQMTR_CON0
#define MT6311_PMIC_FQMTR_EN_MASK                        0x1
#define MT6311_PMIC_FQMTR_EN_SHIFT                       7
#define MT6311_PMIC_FQMTR_WINSET_1_ADDR                  MT6311_FQMTR_CON1
#define MT6311_PMIC_FQMTR_WINSET_1_MASK                  0xFF
#define MT6311_PMIC_FQMTR_WINSET_1_SHIFT                 0
#define MT6311_PMIC_FQMTR_WINSET_0_ADDR                  MT6311_FQMTR_CON2
#define MT6311_PMIC_FQMTR_WINSET_0_MASK                  0xFF
#define MT6311_PMIC_FQMTR_WINSET_0_SHIFT                 0
#define MT6311_PMIC_FQMTR_DATA_1_ADDR                    MT6311_FQMTR_CON3
#define MT6311_PMIC_FQMTR_DATA_1_MASK                    0xFF
#define MT6311_PMIC_FQMTR_DATA_1_SHIFT                   0
#define MT6311_PMIC_FQMTR_DATA_0_ADDR                    MT6311_FQMTR_CON4
#define MT6311_PMIC_FQMTR_DATA_0_MASK                    0xFF
#define MT6311_PMIC_FQMTR_DATA_0_SHIFT                   0

#endif /* __LINUX_MT6311_I2C_H */

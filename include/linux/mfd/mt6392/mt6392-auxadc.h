/*
 * Copyright (C) 2016 MediaTek, Inc.
 *
 * Author: Dongdong Cheng <dongdong.cheng@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MFD_MT6392_AUXADC_H__
#define __MFD_MT6392_AUXADC_H__


#define AUXADC_TIME_OUT	100
#define VOLTAGE_FULL_RANGE	1800
#define MT6392_ADC_PRECISE	32768

/* AUXADC  register mask */
/*STRUP*/
#define MT6392_AUXADC_START_SW_MASK		(1 << 4)
#define MT6392_AUXADC_START_SW_VAL(x)		((x) << 4)
#define MT6392_AUXADC_START_SEL_MASK		(1 << 6)
#define MT6392_AUXADC_START_SEL_VAL(x)		((x) << 6)
#define MT6392_AUXADC_RSTB_SW_MASK		(1 << 5)
#define MT6392_AUXADC_RSTB_SW_VAL(x)		((x) << 5)
#define MT6392_AUXADC_RSTB_SEL_MASK		(1 << 7)
#define MT6392_AUXADC_RSTB_SEL_VAL(x)		((x) << 7)
#define MT6392_AUXADC_RPCNT_MAN_MASK		(0x7f << 8)
#define MT6392_AUXADC_RPCNT_MAN_VAL(x)		((x) << 8)

/*TOP*/
#define MT6392_AUXADC_RNG_CK_PDN_HWEN_MASK	(1 << 3)
#define MT6392_AUXADC_RNG_CK_PDN_HWEN_VAL(x)	((x) << 3)
#define MT6392_AUXADC_CK_PDN_HWEN_MASK		(1 << 4)
#define MT6392_AUXADC_CK_PDN_HWEN_VAL(x)	((x) << 4)
#define MT6392_AUXADC_CK_PDN_MASK		(1 << 5)
#define MT6392_AUXADC_CK_PDN_VAL(x)		((x) << 5)
#define MT6392_AUXADC_1M_CK_PDN_MASK		(1 << 6)
#define MT6392_AUXADC_1M_CK_PDN_VAL(x)		((x) << 6)
#define MT6392_AUXADC_32K_CK_PDN_MASK		(1 << 10)
#define MT6392_AUXADC_32K_CK_PDN_VAL(x)		((x) << 10)
#define MT6392_AUXADC_RNG_CK_MASK		(1 << 11)
#define MT6392_AUXADC_RNG_CK_VAL(x)		((x) << 11)
#define MT6392_AUXADC_RNG_HW_MODE_MASK		(1 << 12)
#define MT6392_AUXADC_RNG_HW_MODE_VAL(x)	((x) << 12)
#define MT6392_AUXADC_REG_RST_MASK		(1 << 9)
#define MT6392_AUXADC_REG_RST_VAL(x)		((x) << 9)
#define MT6392_AUXADC_RST_MASK			(1 << 1)
#define MT6392_AUXADC_RST_VAL(x)		((x) << 1)
#define MT6392_AUXADC_CK_SRC_SEL_MASK		(0x3 << 10)
#define MT6392_AUXADC_CK_SRC_SEL_VAL(x)		((x) << 10)
#define MT6392_AUXADC_CK_TSTSEL_MASK		(1 << 8)
#define MT6392_AUXADC_CK_TSTSEL_VAL(x)		((x) << 8)
#define MT6392_AUXADC_RG_TEST_MASK		(1 << 8)
#define MT6392_AUXADC_RG_TEST_VAL(x)		((x) << 8)

/*AUXADC*/
#define MT6392_AUXADC_RQST0_SET_MASK		(0xffff << 0)
#define MT6392_AUXADC_RQST0_SET_VAL(x)		((x) << 0)

#define MT6392_AUXADC_CK_AON_MASK		(0x1 << 15)
#define MT6392_AUXADC_CK_AON_VAL(x)		((x) << 15)
#define MT6392_AUXADC_12M_CK_AON_MASK		(0x1 << 15)
#define MT6392_AUXADC_12M_CK_AON_VAL(x)		((x) << 15)
#define MT6392_AUXADC_AVG_NUM_LARGE_MASK	(0x7 << 3)
#define MT6392_AUXADC_AVG_NUM_LARGE_VAL(x)	((x) << 3)
#define MT6392_AUXADC_AVG_NUM_SMALL_MASK	(0x7 << 0)
#define MT6392_AUXADC_AVG_NUM_SMALL_VAL(x)	((x) << 0)
#define MT6392_AUXADC_AVG_NUM_SEL_MASK		(0xffff << 0)
#define MT6392_AUXADC_AVG_NUM_SEL_VAL(x)	((x) << 0)
#define MT6392_AUXADC_VBUF_EN_MASK		(0x1 << 9)
#define MT6392_AUXADC_VBUF_EN_VAL(x)		((x) << 9)

#define MT6392_AUXADC_TRIM_CH0_SEL_MASK		(0x3 << 0)
#define MT6392_AUXADC_TRIM_CH0_SEL_VAL(x)	((x) << 0)
#define MT6392_AUXADC_TRIM_CH1_SEL_MASK		(0x3 << 2)
#define MT6392_AUXADC_TRIM_CH1_SEL_VAL(x)	((x) << 2)
#define MT6392_AUXADC_TRIM_CH2_SEL_MASK		(0x3 << 4)
#define MT6392_AUXADC_TRIM_CH2_SEL_VAL(x)	((x) << 4)
#define MT6392_AUXADC_TRIM_CH3_SEL_MASK		(0x3 << 6)
#define MT6392_AUXADC_TRIM_CH3_SEL_VAL(x)	((x) << 6)
#define MT6392_AUXADC_TRIM_CH4_SEL_MASK		(0x3 << 8)
#define MT6392_AUXADC_TRIM_CH4_SEL_VAL(x)	((x) << 8)
#define MT6392_AUXADC_TRIM_CH5_SEL_MASK		(0x3 << 10)
#define MT6392_AUXADC_TRIM_CH5_SEL_VAL(x)	((x) << 10)
#define MT6392_AUXADC_TRIM_CH6_SEL_MASK		(0x3 << 12)
#define MT6392_AUXADC_TRIM_CH6_SEL_VAL(x)	((x) << 12)
#define MT6392_AUXADC_VBUF_EN_SWCTRL_MASK	(0x1 << 10)
#define MT6392_AUXADC_VBUF_EN_SWCTRL_VAL(x)	((x) << 10)
#define MT6392_AUXADC_VBUF_EN_MASK		(0x1 << 9)
#define MT6392_AUXADC_VBUF_EN_VAL(x)		((x) << 9)

extern int PMIC_IMM_GetOneChannelValue(unsigned int dwChannel, int deCount, int trimd);

#endif /* __MFD_MT6392_AUXADC_H__ */

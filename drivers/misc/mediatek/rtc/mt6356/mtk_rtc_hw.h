/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_RTC_HW_H__
#define __MT_RTC_HW_H__

#include <mach/upmu_hw.h>
extern unsigned int pmic_config_interface_nolock(unsigned int RegNum,
						 unsigned int val,
						 unsigned int MASK,
						 unsigned int SHIFT);

#define RTC_BASE		(0x0980)

/* RTC registers */
#define MT_PMIC_REG_BASE (0x0000)
#define RTC_BBPU            (RTC_BASE + 0x0088)
#define RTC_BBPU_PWREN            (1U << 0)	/* BBPU = 1 when alarm occurs */
#define RTC_BBPU_BBPU            (1U << 2)	/* 1: power on, 0: power down */
/* BBPU = 0 when xreset_rstb goes low */
#define RTC_BBPU_AUTO            (1U << 3)
#define RTC_BBPU_CLRPKY        (1U << 4)
#define RTC_BBPU_RELOAD        (1U << 5)
#define RTC_BBPU_CBUSY            (1U << 6)
#define RTC_BBPU_KEY            (0x43 << 8)

#define RTC_IRQ_STA			(RTC_BASE + 0x008A)
#define RTC_IRQ_STA_AL            (1U << 0)
#define RTC_IRQ_STA_TC            (1U << 1)
#define RTC_IRQ_STA_LP            (1U << 3)

#define RTC_IRQ_EN			(RTC_BASE + 0x008C)
#define RTC_IRQ_EN_AL            (1U << 0)
#define RTC_IRQ_EN_TC            (1U << 1)
#define RTC_IRQ_EN_ONESHOT        (1U << 2)
#define RTC_IRQ_EN_LP            (1U << 3)
#define RTC_IRQ_EN_ONESHOT_AL        (RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_CII_EN			(RTC_BASE + 0x008E)
#define RTC_CII_SEC             (1U << 0)
#define RTC_CII_MIN             (1U << 1)
#define RTC_CII_HOU             (1U << 2)
#define RTC_CII_DOM             (1U << 3)
#define RTC_CII_DOW             (1U << 4)
#define RTC_CII_MTH             (1U << 5)
#define RTC_CII_YEA             (1U << 6)
#define RTC_CII_1_2_SEC            (1U << 7)
#define RTC_CII_1_4_SEC            (1U << 8)
#define RTC_CII_1_8_SEC            (1U << 9)

#define RTC_AL_MASK			(RTC_BASE + 0x0090)
#define RTC_AL_MASK_SEC        (1U << 0)
#define RTC_AL_MASK_MIN        (1U << 1)
#define RTC_AL_MASK_HOU        (1U << 2)
#define RTC_AL_MASK_DOM        (1U << 3)
#define RTC_AL_MASK_DOW        (1U << 4)
#define RTC_AL_MASK_MTH        (1U << 5)
#define RTC_AL_MASK_YEA        (1U << 6)

#define RTC_TC_SEC			(RTC_BASE + 0x0092)
#define RTC_TC_SEC_MASK 0x003f

#define RTC_TC_MIN			(RTC_BASE + 0x0094)
#define RTC_TC_MIN_MASK 0x003f

#define RTC_TC_HOU			(RTC_BASE + 0x0096)
#define RTC_TC_HOU_MASK 0x001f

#define RTC_TC_DOM			(RTC_BASE + 0x0098)
#define RTC_TC_DOM_MASK 0x001f

#define RTC_TC_DOW			(RTC_BASE + 0x009A)
#define RTC_TC_DOW_MASK 0x0007

#define RTC_TC_MTH			(RTC_BASE + 0x009C)
#define RTC_TC_MTH_MASK 0x000f

#define RTC_TC_YEA			(RTC_BASE + 0x009E)
#define RTC_TC_YEA_MASK 0x007f

#define RTC_AL_SEC			(RTC_BASE + 0x00A0)
#define RTC_K_EOSC32_VTCXO_ON_SEL    (1U << 15)
#define RTC_LPD_OPT_SHIFT        13
#define RTC_LPD_OPT            (1U << RTC_LPD_OPT_SHIFT)
#define RTC_BBPU_2SEC_STAT_CLEAR    (1U << 11)
#define RTC_BBPU_2SEC_MODE        (3U << 9)
#define RTC_BBPU_2SEC_EN        (1U << 8)
#define RTC_BBPU_2SEC_CK_SEL        (1U << 7)
#define RTC_BBPU_AUTO_PDN_SEL		(1U << 6)
#define RTC_AL_SEC_MASK 0x003f

#define RTC_AL_MIN			(RTC_BASE + 0x00A2)
#define RTC_AL_MIN_MASK 0x003f

/*
 * RTC_NEW_SPARE0: RTC_AL_HOU bit0~4
 * bit 8 ~ 15 : For design used, can't be overwrited.
 */
#define RTC_AL_HOU			(RTC_BASE + 0x00A4)
#define RTC_NEW_SPARE0            0xff00
#define RTC_AL_HOU_MASK         0x001f
#define RTC_AL_HOU_FG_SHIFT		8
#define RTC_AL_HOU_FG_MASK		0xff00

/*
 * RTC_NEW_SPARE1: RTC_AL_DOM bit0~4
 * bit 8 ~ 15 : for 2 second reboot desing used,
 * can't be overwrited.
 */
#define RTC_AL_DOM			(RTC_BASE + 0x00A6)
#define RTC_NEW_SPARE1    0xff00
#define RTC_AL_DOM_MASK 0x001f

/*
 * RTC_NEW_SPARE2: RTC_AL_DOW bit0~2
 *        bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_DOW			(RTC_BASE + 0x00A8)
#define RTC_NEW_SPARE2    0xff00
#define RTC_AL_DOW_MASK 0x0007

/*
 * RTC_NEW_SPARE3: RTC_AL_MTH bit0~3
 * bit 8 ~ 15 : Fuel Gauge
 */
#define RTC_AL_MTH			(RTC_BASE + 0x00AA)
#define RTC_NEW_SPARE3    0xff00
#define RTC_AL_MTH_MASK 0x000f
#define RTC_AL_MTH_FG_SHIFT	8
#define RTC_AL_MTH_FG_MASK	0xff00

#define RTC_AL_YEA			(RTC_BASE + 0x00AC)
#define RTC_K_EOSC_RSV_7        (1U << 15)
#define RTC_K_EOSC_RSV_6        (1U << 14)
#define RTC_K_EOSC_RSV_5        (1U << 13)
#define RTC_K_EOSC_RSV_4        (1U << 12)
#define RTC_K_EOSC_RSV_3        (1U << 11)
#define RTC_K_EOSC_RSV_2        (1U << 10)
#define RTC_K_EOSC_RSV_1        (1U << 9)
#define RTC_K_EOSC_RSV_0        (1U << 8)
#define RTC_AL_YEA_MASK 0x007f

#define RTC_OSC32CON			(RTC_BASE + 0x00AE)
#define RTC_OSC32CON_UNLOCK1        0x1a57
#define RTC_OSC32CON_UNLOCK2        0x2b68
#define RTC_REG_XOSC32_ENB        (1U << 15)
#define RTC_GP_OSC32_CON		(2U << 13)
#define RTC_EOSC32_CHOP_EN         (1U << 12)
#define RTC_EOSC32_VCT_EN_SHIFT    11
#define RTC_EOSC32_VCT_EN        (1U << RTC_EOSC32_VCT_EN_SHIFT)
#define RTC_EOSC32_OPT          (1U << 10)
/*
 * 0: embedded clock switch back to dcxo decided by
 *    (eosc32_ck_alive & powerkey_match)
 * 1: embedded clock switch back to dcxo decided by (powerkey_match)
 */
#define RTC_EMBCK_SEL_OPTION        (1U << 9)
#define RTC_EMBCK_SRC_SEL        (1U << 8)
#define RTC_EMBCK_SEL_MODE        (3U << 6)
/*	0:dcxo_ck	1:eosc32_ck	2:emb_k_eosc_32	3: emb_hw	*/
#define RTC_EMBCK_SEL_DCXO         (0 << 6)
#define RTC_EMBCK_SEL_EOSC         (1U << 6)
#define RTC_EMBCK_SEL_K_EOSC         (2U << 6)
#define RTC_EMBCK_SEL_HW         (3U << 6)
/* 0 (32k crystal exist)    1 (32k crystal doesn't exist)*/
#define RTC_XOSC32_ENB           (1U << 5)
/*Default 4'b0111, 2nd step suggest to set
 *to 4'b0000 EOSC_CALI = charging cap calibration
 */
#define RTC_XOSCCALI_MASK        0x001f

#define OSC32CON_ANALOG_SETTING	(RTC_GP_OSC32_CON | \
	RTC_EOSC32_VCT_EN | RTC_EOSC32_CHOP_EN & \
	(~RTC_REG_XOSC32_ENB) | RTC_EMBCK_SEL_MODE | \
	RTC_EMBCK_SEL_OPTION | RTC_EMBCK_SRC_SEL)

#define RTC_XOSCCALI_START        0x0000
#define RTC_XOSCCALI_END        0x001f

#define RTC_POWERKEY1			(RTC_BASE + 0x00B0)
#define RTC_POWERKEY2			(RTC_BASE + 0x00B2)
#define RTC_POWERKEY1_KEY        0xa357
#define RTC_POWERKEY2_KEY        0x67d2
/*
 * RTC_PDN1:
 *     bit 0 - 3  : Android bits
 *     bit 4 - 5  : Recovery bits (0x10: factory data reset)
 *     bit 6      : Bypass PWRKEY bit
 *     bit 7      : Power-On Time bit
 *     bit 8      : RTC_GPIO_USER_WIFI bit
 *     bit 9      : RTC_GPIO_USER_GPS bit
 *     bit 10     : RTC_GPIO_USER_BT bit
 *     bit 11     : RTC_GPIO_USER_FM bit
 *     bit 12     : RTC_GPIO_USER_PMIC bit
 *     bit 13     : Fast Boot
 *     bit 14      : Kernel Power Off Charging
 *     bit 15     : Debug bit
 */
#define RTC_PDN1			(RTC_BASE + 0x00B4)
#define RTC_PDN1_ANDROID_MASK        0x000f
#define RTC_PDN1_RECOVERY_MASK        0x0030
#define RTC_PDN1_FAC_RESET        (1U << 4)
#define RTC_PDN1_BYPASS_PWR        (1U << 6)
#define RTC_PDN1_PWRON_TIME        (1U << 7)
#define RTC_PDN1_GPIO_WIFI        (1U << 8)
#define RTC_PDN1_GPIO_GPS        (1U << 9)
#define RTC_PDN1_GPIO_BT        (1U << 10)
#define RTC_PDN1_GPIO_FM        (1U << 11)
#define RTC_PDN1_GPIO_PMIC        (1U << 12)
#define RTC_PDN1_FAST_BOOT        (1U << 13)
#define RTC_PDN1_KPOC            (1U << 14)
#define RTC_PDN1_DEBUG            (1U << 15)

#define RTC_GPIO_USER_MASK	(RTC_PDN1_GPIO_WIFI | RTC_PDN1_GPIO_GPS\
	| RTC_PDN1_GPIO_BT | RTC_PDN1_GPIO_FM | RTC_PDN1_GPIO_PMIC)

/*
 * RTC_PDN2:
 *     bit 0 - 3 : MTH in power-on time
 *     bit 4     : Power-On Alarm bit
 *     bit 5 - 6 : UART bits
 *     bit 7     : autoboot bit
 *     bit 8 - 14: YEA in power-on time
 *     bit 15    : Power-On Logo bit
 */
#define RTC_PDN2                (RTC_BASE + 0x00B6)
#define RTC_PDN2_PWRON_ALARM             (1U << 4)
#define RTC_PDN2_UART_MASK            0x0060
#define RTC_PDN2_UART_SHIFT            5
#define RTC_AUTOBOOT_MASK            0x1
#define RTC_AUTOBOOT_SHIFT            7
#define RTC_PDN2_PWRON_LOGO            (1U << 15)

/*
 * RTC_SPAR0:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6      : 32K less bit. True:with 32K, False:Without 32K
 *     bit 7 - 15: reserved bits
 */
#define RTC_SPAR0                (RTC_BASE + 0x00B8)
#define RTC_SPAR0_32K_LESS             (1U << 6)
#define RTC_SPAR0_LP_DET            (1U << 7)

/*
 * RTC_SPAR1:
 *     bit 0 - 5  : MIN in power-on time
 *     bit 6 - 10 : HOU in power-on time
 *     bit 11 - 15: DOM in power-on time
 */
#define RTC_SPAR1            (RTC_BASE + 0x00BA)

#define RTC_PROT            (RTC_BASE + 0x00BC)
#define RTC_PROT_UNLOCK1         0x586a
#define RTC_PROT_UNLOCK2         0x9136

#define RTC_DIFF            (RTC_BASE + 0x00BE)
#define RTC_CALI_RD_SEL_SHIFT        15
#define RTC_CALI_RD_SEL         (1U << RTC_CALI_RD_SEL_SHIFT)
#define RTC_K_EOSC32_RSV        (1U << 14)
#define RTC_DIFF_MASK            0xFFF
#define RTC_DIFF_SHIFT            0

#define RTC_CALI			(RTC_BASE + 0x00C0)
#define RTC_K_EOSC32_OVERFLOW        (1U << 15)
#define RTC_CALI_WR_SEL_SHIFT        14
#define RTC_CALI_WR_SEL_MASK        1
#define RTC_CALI_XOSC            0
#define RTC_CALI_K_EOSC_32        1
#define RTC_CALI_MASK            0x3FFF
#define RTC_CALI_SHIFT            0

#define RTC_WRTGR			(RTC_BASE + 0x00C2)

#define RTC_CON				(RTC_BASE + 0x00C4)
#define RTC_VBAT_LPSTA_RAW        (1U << 0)
#define RTC_EOSC32_LPEN            (1U << 1)
#define RTC_XOSC32_LPEN            (1U << 2)
#define RTC_CON_LPRST            (1U << 3)
#define RTC_CON_CDBO            (1U << 4)
#define RTC_CON_F32KOB            (1U << 5)	/* 0: RTC_GPIO exports 32K */
#define RTC_CON_GPO            (1U << 6)
#define RTC_CON_GOE            (1U << 7)	/* 1: GPO mode, 0: GPI mode */
#define RTC_CON_GSR            (1U << 8)
#define RTC_CON_GSMT            (1U << 9)
#define RTC_CON_GPEN            (1U << 10)
#define RTC_CON_GPU            (1U << 11)
#define RTC_CON_GE4            (1U << 12)
#define RTC_CON_GE8            (1U << 13)
#define RTC_CON_GPI            (1U << 14)
#define RTC_CON_LPSTA_RAW        (1U << 15)	/* 32K was stopped */

/* power on alarm time setting */

#define RTC_PWRON_YEA        RTC_PDN2
#define RTC_PWRON_YEA_MASK     0x7f00
#define RTC_PWRON_YEA_SHIFT     8

#define RTC_PWRON_MTH        RTC_PDN2
#define RTC_PWRON_MTH_MASK     0x000f
#define RTC_PWRON_MTH_SHIFT     0

#define RTC_PWRON_SEC        RTC_SPAR0
#define RTC_PWRON_SEC_MASK     0x003f
#define RTC_PWRON_SEC_SHIFT     0

#define RTC_PWRON_MIN        RTC_SPAR1
#define RTC_PWRON_MIN_MASK     0x003f
#define RTC_PWRON_MIN_SHIFT     0

#define RTC_PWRON_HOU        RTC_SPAR1
#define RTC_PWRON_HOU_MASK     0x07c0
#define RTC_PWRON_HOU_SHIFT     6

#define RTC_PWRON_DOM        RTC_SPAR1
#define RTC_PWRON_DOM_MASK     0xf800
#define RTC_PWRON_DOM_SHIFT     11

#define RTC_INT_CNT            (RTC_BASE + 0x00C8)
#define RTC_INT_CNT_MASK        0x7FF
#define RTC_INT_CNT_SHIFT       0

extern u16 rtc_spare_reg[][3];

#endif				/* __MT_RTC_HW_H__ */

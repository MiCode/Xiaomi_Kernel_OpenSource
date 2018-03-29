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

/* #include <typedefs.h> */
/* #include <platform.h> */
/*
#define TOP_CKPDN		0x004E
#define TOP_CKTST2      0x012E
#define FQMTR_CON0      0x0188
#define FQMTR_CON1      0x018A
#define FQMTR_CON2      0x018C
*/
/* RTC registers */
/* #define       RTC_BASE        (0x8000) */
/* RTC registers */
#define RTC_BBPU					(RTC_BASE + 0x0000)
#define RTC_BBPU_PWREN		(1U << 0)	/* BBPU = 1 when alarm occurs */
#define RTC_BBPU_BBPU			(1U << 2)	/* 1: power on, 0: power down */
#define RTC_BBPU_AUTO			(1U << 3)	/* BBPU = 0 when xreset_rstb goes low */
#define RTC_BBPU_CLRPKY		(1U << 4)
#define RTC_BBPU_RELOAD		(1U << 5)
#define RTC_BBPU_CBUSY		(1U << 6)
#define RTC_BBPU_KEY			(0x43 << 8)

#define RTC_IRQ_STA				(RTC_BASE + 0x0002)
#define RTC_IRQ_STA_AL		(1U << 0)
#define RTC_IRQ_STA_TC		(1U << 1)
#define RTC_IRQ_STA_LP		(1U << 3)

#define RTC_IRQ_EN					(RTC_BASE + 0x0004)
#define RTC_IRQ_EN_AL				(1U << 0)
#define RTC_IRQ_EN_TC		(1U << 1)
#define RTC_IRQ_EN_ONESHOT	(1U << 2)
#define RTC_IRQ_EN_LP				(1U << 3)
#define RTC_IRQ_EN_ONESHOT_AL	(RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_CII_EN			(RTC_BASE + 0x0006)
#define RTC_CII_SEC		(1U << 0)
#define RTC_CII_MIN		(1U << 1)
#define RTC_CII_HOU		(1U << 2)
#define RTC_CII_DOM		(1U << 3)
#define RTC_CII_DOW		(1U << 4)
#define RTC_CII_MTH		(1U << 5)
#define RTC_CII_YEA		(1U << 6)
#define RTC_CII_1_2_SEC (1U << 7)
#define RTC_CII_1_4_SEC (1U << 8)
#define RTC_CII_1_8_SEC (1U << 9)

#define RTC_AL_MASK	(RTC_BASE + 0x0008)
#define RTC_AL_MASK_SEC	(1U << 0)
#define RTC_AL_MASK_MIN	(1U << 1)
#define RTC_AL_MASK_HOU	(1U << 2)
#define RTC_AL_MASK_DOM	(1U << 3)
#define RTC_AL_MASK_DOW	(1U << 4)
#define RTC_AL_MASK_MTH	(1U << 5)
#define RTC_AL_MASK_YEA	(1U << 6)

#define RTC_TC_SEC			(RTC_BASE + 0x000a)
#define RTC_TC_SEC_MASK 0x003f

#define RTC_TC_MIN			(RTC_BASE + 0x000c)
#define RTC_TC_MIN_MASK 0x003f

#define RTC_TC_HOU			(RTC_BASE + 0x000e)
#define RTC_TC_HOU_MASK 0x001f

#define RTC_TC_DOM			(RTC_BASE + 0x0010)
#define RTC_TC_DOM_MASK 0x001f

#define RTC_TC_DOW			(RTC_BASE + 0x0012)
#define RTC_TC_DOW_MASK 0x0007

#define RTC_TC_MTH			(RTC_BASE + 0x0014)
#define RTC_TC_MTH_MASK 0x000f

#define RTC_TC_YEA			(RTC_BASE + 0x0016)
#define RTC_TC_YEA_MASK 0x007f

#define RTC_AL_SEC			(RTC_BASE + 0x0018)
#define RTC_AL_SEC_MASK 0x003f

#define RTC_AL_MIN			(RTC_BASE + 0x001a)
#define RTC_AL_MIN_MASK 0x003f

/*
 * RTC_NEW_SPARE0: RTC_AL_HOU bit0~4
 *	   bit 8 ~ 14 : Fuel Gauge
 *     bit 15     : reserved bits
 */
#define RTC_AL_HOU							(RTC_BASE + 0x001c)
#define RTC_NEW_SPARE_FG_MASK		0xff00
#define RTC_NEW_SPARE_FG_SHIFT	8
#define RTC_AL_HOU_MASK				0x001f

/*
 * RTC_NEW_SPARE1: RTC_AL_DOM bit0~4
 *	   bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_DOM			(RTC_BASE + 0x001e)
#define RTC_NEW_SPARE1	0xff00
#define RTC_AL_DOM_MASK 0x001f

/*
 * RTC_NEW_SPARE2: RTC_AL_DOW bit0~2
 *	   bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_DOW			(RTC_BASE + 0x0020)
#define RTC_NEW_SPARE2	0xff00
#define RTC_AL_DOW_MASK 0x0007

/*
 * RTC_NEW_SPARE3: RTC_AL_MTH bit0~3
 *	   bit 8 ~ 15 : reserved bits
 */
#define RTC_AL_MTH			(RTC_BASE + 0x0022)
#define RTC_NEW_SPARE3	0xff00
#define RTC_AL_MTH_MASK 0x000f

#define RTC_AL_YEA			(RTC_BASE + 0x0024)
#define RTC_AL_YEA_MASK 0x007f

#define RTC_OSC32CON			(RTC_BASE + 0x0026)
#define RTC_OSC32CON_UNLOCK1		0x1a57
#define RTC_OSC32CON_UNLOCK2		0x2b68
#define RTC_OSC32CON_XOSC32_ENB		(1U << 13)
#define RTC_OSC32CON_LNBUFEN		(1U << 11)	/* ungate 32K to ABB */
#define RTC_OSC32CON_EOSC32_CHOP_EN	(1U << 10)
#define RTC_OSC32CON_EMBCK_SEL			(1U << 7)
#define RTC_OSC32CON_EMBCK_SEL_MODE	(1U << 6)
#define RTC_OSC32CON_XOSCCALI_MASK	0x001f

#define RTC_XOSCCALI_START		0x0000
#define RTC_XOSCCALI_END		0x001f

#define RTC_POWERKEY1	(RTC_BASE + 0x0028)
#define RTC_POWERKEY2	(RTC_BASE + 0x002a)
#define RTC_POWERKEY1_KEY       0xa357
#define RTC_POWERKEY2_KEY       0x67d2
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
 *     bit 14	  : Kernel Power Off Charging
 *     bit 15     : Debug bit
 */
#define RTC_PDN1								(RTC_BASE + 0x002c)
#define RTC_PDN1_ANDROID_MASK		0x000f
#define RTC_PDN1_RECOVERY_MASK	0x0030
#define RTC_PDN1_FAC_RESET			(1U << 4)
#define RTC_PDN1_BYPASS_PWR			(1U << 6)
#define RTC_PDN1_PWRON_TIME			(1U << 7)
#define RTC_PDN1_GPIO_WIFI			(1U << 8)
#define RTC_PDN1_GPIO_GPS				(1U << 9)
#define RTC_PDN1_GPIO_BT				(1U << 10)
#define RTC_PDN1_GPIO_FM				(1U << 11)
#define RTC_PDN1_GPIO_PMIC			(1U << 12)
#define RTC_PDN1_FAST_BOOT			(1U << 13)
#define RTC_PDN1_KPOC						(1U << 14)
#define RTC_PDN1_DEBUG					(1U << 15)

/*
 * RTC_PDN2:
 *     bit 0 - 3 : MTH in power-on time
 *     bit 4     : Power-On Alarm bit
 *     bit 5 - 6 : UART bits
 *     bit 7     : reserved bit
 *     bit 8 - 14: YEA in power-on time
 *     bit 15    : Power-On Logo bit
 */
#define RTC_PDN2									(RTC_BASE + 0x002e)
#define RTC_PDN2_PWRON_MTH_MASK	0x000f
#define RTC_PDN2_PWRON_MTH_SHIFT	0
#define RTC_PDN2_PWRON_ALARM			(1U << 4)
#define RTC_PDN2_UART_MASK				0x0060
#define RTC_PDN2_UART_SHIFT				5
#define RTC_PDN2_PWRON_YEA_MASK	0x7f00
#define RTC_PDN2_PWRON_YEA_SHIFT	8
#define RTC_PDN2_PWRON_LOGO				(1U << 15)

/*
 * RTC_SPAR0:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6	 : 32K less bit. True:with 32K, False:Without 32K
 *     bit 7 - 15: reserved bits
 */
#define RTC_SPAR0									(RTC_BASE + 0x0030)
#define RTC_SPAR0_PWRON_SEC_MASK	0x003f
#define RTC_SPAR0_PWRON_SEC_SHIFT	0
#define RTC_SPAR0_32K_LESS				(1U << 6)
#define RTC_SPAR0_LP_DET					(1U << 7)
#define RTC_SPAR0_ENTER_KPOC				(1U << 8)
#define RTC_SPAR0_EMERGENCY_DL_MODE		(1U << 9)
#define RTC_SPAR0_LONG_PRESS_RST			(1U << 10)	/* mask for long press reset */

/*
 * RTC_SPAR1:
 *     bit 0 - 5  : MIN in power-on time
 *     bit 6 - 10 : HOU in power-on time
 *     bit 11 - 15: DOM in power-on time
 */
#define RTC_SPAR1									(RTC_BASE + 0x0032)
#define RTC_SPAR1_PWRON_MIN_MASK	0x003f
#define RTC_SPAR1_PWRON_MIN_SHIFT	0
#define RTC_SPAR1_PWRON_HOU_MASK	0x07c0
#define RTC_SPAR1_PWRON_HOU_SHIFT	6
#define RTC_SPAR1_PWRON_DOM_MASK	0xf800
#define RTC_SPAR1_PWRON_DOM_SHIFT	11

#define RTC_PROT					(RTC_BASE + 0x0036)
#define RTC_PROT_UNLOCK1	0x586a
#define RTC_PROT_UNLOCK2	0x9136

#define RTC_DIFF	(RTC_BASE + 0x0038)
#define RTC_CALI	(RTC_BASE + 0x003a)
#define RTC_WRTGR	(RTC_BASE + 0x003c)

#define RTC_CON					(RTC_BASE + 0x003e)
#define RTC_CON_LPEN		(1U << 2)
#define RTC_CON_LPRST		(1U << 3)
#define RTC_CON_CDBO		(1U << 4)
#define RTC_CON_F32KOB		(1U << 5)	/* 0: RTC_GPIO exports 32K */
#define RTC_CON_GPO		(1U << 6)
#define RTC_CON_GOE		(1U << 7)	/* 1: GPO mode, 0: GPI mode */
#define RTC_CON_GSR		(1U << 8)
#define RTC_CON_GSMT		(1U << 9)
#define RTC_CON_GPEN		(1U << 10)
#define RTC_CON_GPU		(1U << 11)
#define RTC_CON_GE4		(1U << 12)
#define RTC_CON_GE8		(1U << 13)
#define RTC_CON_GPI		(1U << 14)
#define RTC_CON_LPSTA_RAW	(1U << 15)	/* 32K was stopped */

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

extern u16 rtc_spare_reg[][3];

#endif				/* __MT_RTC_HW_H__ */

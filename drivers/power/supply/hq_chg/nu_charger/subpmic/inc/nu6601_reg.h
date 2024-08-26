/*
 *  Copyright (C) 2017 MediaTek Inc.
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

#ifndef __LINUX_NU6601_REG_H
#define __LINUX_NU6601_REG_H

/* intr regs */
#define NU6601_REG_INTMNGR_STAT0 (0x0E)
#define NU6601_REG_INTMNGR_STAT1 (0x0F)

#define NU6601_REG_INFRAINT_STAT (0x10)
#define NU6601_REG_INFRAINT_FLAG (0x11)
#define NU6601_REG_INFRAINT_MASK (0x12)
#define NU6601_REG_INFRAINT_FE_EN (0x13)


#define NU6601_REG_ADCINT_STAT (0x40)
#define NU6601_REG_ADCINT_FLAG (0x41)
#define NU6601_REG_ADCINT_MASK (0x42)
#define NU6601_REG_ADCINT_FE_EN (0x43)

#define NU6601_REG_QCINT_STAT (0xB6)
#define NU6601_REG_QCINT_FLAG (0xB7)
#define NU6601_REG_QCINT_MASK (0xB8)
#define NU6601_REG_QCINT_FE_EN (0xB9)

#define NU6601_REG_BC12INT_STAT (0xB0)
#define NU6601_REG_BC12INT_FLAG (0xB1)
#define NU6601_REG_BC12INT_MASK (0xB2)
#define NU6601_REG_BC12INT_FE_EN (0xB3)

#define NU6601_REG_UFCSINT0_STAT (0x14)
#define NU6601_REG_UFCSINT0_FLAG (0x15)
#define NU6601_REG_UFCSINT0_MASK (0x16)
#define NU6601_REG_UFCSINT0_FE_EN (0x17)

#define NU6601_REG_UFCSINT1_STAT (0x14)
#define NU6601_REG_UFCSINT1_FLAG (0x15)
#define NU6601_REG_UFCSINT1_MASK (0x16)
#define NU6601_REG_UFCSINT1_FE_EN (0x17)

#define NU6601_REG_UFCSINT2_STAT (0x18)
#define NU6601_REG_UFCSINT2_FLAG (0x19)
#define NU6601_REG_UFCSINT2_MASK (0x1a)
#define NU6601_REG_UFCSINT2_FE_EN (0x1b)

#define NU6601_REG_LEDINT1_STAT (0x80)
#define NU6601_REG_LEDINT1_FLAG (0x81)
#define NU6601_REG_LEDINT1_MASK (0x82)
#define NU6601_REG_LEDINT1_FE_EN (0x83)

#define NU6601_REG_LEDINT2_STAT (0x84)
#define NU6601_REG_LEDINT2_FLAG (0x85)
#define NU6601_REG_LEDINT2_MASK (0x86)
#define NU6601_REG_LEDINT2_FE_EN (0x87)

#define NU6601_REG_BATTINT_STAT (0x70)
#define NU6601_REG_BATTINT_FLAG (0x71)
#define NU6601_REG_BATTINT_MASK (0x72)
#define NU6601_REG_BATTINT_FE_EN (0x73)

#define NU6601_REG_BOBULOOPINT_STAT (0x1C)
#define NU6601_REG_BOBULOOPINT_FLAG (0x1D)
#define NU6601_REG_BOBULOOPINT_MASK (0x1E)
#define NU6601_REG_BOBULOOPINT_FE_EN (0x1F)

#define NU6601_REG_BOBUINT_STAT (0x18)
#define NU6601_REG_BOBUINT_FLAG (0x19)
#define NU6601_REG_BOBUINT_MASK (0x1A)
#define NU6601_REG_BOBUINT_FE_EN (0x1B)

#define NU6601_REG_USBINT_STAT (0x14)
#define NU6601_REG_USBINT_FLAG (0x15)
#define NU6601_REG_USBINT_MASK (0x16)
#define NU6601_REG_USBINT_FE_EN (0x17)

#define NU6601_REG_CHGINT_STAT (0x10)
#define NU6601_REG_CHGINT_FLAG (0x11)
#define NU6601_REG_CHGINT_MASK (0x12)
#define NU6601_REG_CHGINT_FE_EN (0x13)

/* ========== 0x30 HK_ADC Trim Registers =============== */
/*General Registers*/

/* Register 0x04h */
#define NU6601_REG_DEV_ID					(0x04)
#define NU6601_DEV_ID			(0x61)

/* Register 0x05h */
#define NU6601_REG_REV_ID					(0x05)
#define NU6601_REV_ID_A0			(0x00)
#define NU6601_REV_ID_A1			(0x01)
#define NU6601_REV_ID_A2			(0x02)

/* Register 0x22h */
#define NU6601_REG_I2C_RST					(0x22)
	#define I2C_LONG_RST_EN_MASK		(0x80)
	#define I2C_LONG_RST_EN_SHIFT		(7)
	#define I2C_LONG_RST_ENABLE			(1)
	#define I2C_LONG_RST_DISABLE		(0)

	#define EN_I2C_IN_SHIPMODE_MASK		(0x40)
	#define EN_I2C_IN_SHIPMODE_SHIFT	(6)
	#define I2C_IN_SHIPMODE_ENABLE		(1)
	#define I2C_IN_SHIPMODE_DISABLE		(0)

	#define PWR_KEY_LONG_RST_ENMASK		(0x20)
	#define PWR_KEY_LONG_RST_SHIFT		(5)
	#define PWR_KEY_LONG_RST_ENABLE		(1)
	#define PWR_KEY_LONG_RST_DISABLE	(0)

	#define WD_TIMER_RST_MASK			(0x10)
	#define WD_TIMER_RST_SHIFT			(4)
	#define WD_TIMER_NORMAL				(0)
	#define WD_TIMER_RESET				(1)

	#define WD_TIMEOUT_MASK				(0x0F)
	#define WD_TIMEOUT_SHIFT			(0)
	#define WD_TIMEOUT_DISABLE			(0x00)
	#define WD_TIMEOUT_500MS			(0x01)
	#define WD_TIMEOUT_1S				(0x02)
	#define WD_TIMEOUT_2S				(0x03)
	#define WD_TIMEOUT_3S				(0x04)
	#define WD_TIMEOUT_4S				(0x05)
	#define WD_TIMEOUT_5S				(0x06)
	#define WD_TIMEOUT_6S				(0x07)
	#define WD_TIMEOUT_7S				(0x08)
	#define WD_TIMEOUT_8S				(0x09)
	#define WD_TIMEOUT_9S				(0x0A)
	#define WD_TIMEOUT_10S				(0x0B)
	#define WD_TIMEOUT_20S				(0x0C)
	#define WD_TIMEOUT_40S				(0x0D)
	#define WD_TIMEOUT_80S				(0x0E)
	#define WD_TIMEOUT_160S				(0x0F)

/* Register 0x23h */
#define NU6601_REG_SW_RST					(0x23)
	#define SW_RST_MASK		(0xFF)
	#define SW_RST_SHIFT	(0)
	#define SW_RST_ALL		(0xFF)

/*ADC Registers*/
/* Register 0x44h */
#define NU6601_REG_ADC_CTRL					(0x44)
	#define ADC_EN_MASK					(0x80)
	#define ADC_EN_SHIFT			    (7)
	#define ADC_ENABLE					(1)
	#define ADC_DISABLE					(0)

	#define ADC_MODE_MASK				(0x40)
	#define ADC_MODE_SHIFT				(6)
	#define ADC_MODE_CONTINUE			(0)
	#define ADC_MODE_ONT_SHOT			(1)
/*
 *    #define ADC_AVG_MASK				(0x20)
 *    #define ADC_AVG_SHIFT				(5)
 *    #define ADC_AVG_CLOSE				(0)
 *    #define ADC_AVG_OPEN				(1)
 */
	#define ADC_FAST_MODE_MASK			(0x20)
	#define ADC_FAST_MODE_SHIFT			(5)
	#define ADC_FAST_MODE				(1)
	#define ADC_NORMAL_MODE				(0)
/*
 *
 *    #define ADC_AVG_INIT_MASK			(0x08)
 *    #define ADC_AVG_INIT_SHIFT			(3)
 *
 */
	#define CONV_REQ_MASK				(0x10)
	#define CONV_REQ_SHIFT				(4)
	#define CONV_REQ_START				(1)
	#define CONV_REQ_DONE				(0)

	#define CIC_SUM_TIME_MASK			(0x0C)
	#define CIC_SUM_TIME_SHIFT			(2)
	#define CIC_SUM_TIME_0P5MS			(0)
	#define CIC_SUM_TIME_1MS			(1)
	#define CIC_SUM_TIME_2MS			(2)
	#define CIC_SUM_TIME_4MS			(3)

	#define ADC_SMP_CLK_MASK			(0x03)
	#define ADC_SMP_CLK_SHIFT			(0)
	#define ADC_SMP_CLK_482K			(0)
	#define ADC_SMP_CLK_859K			(1)
	#define ADC_SMP_CLK_1040K			(2)
	#define ADC_SMP_CLK_1410K			(3)

#define NU6601_REG_ADC_CH_EN					(0x47)
#define NU6601_REG_ADC_CH_EN1					(0x48)
	#define TDIE_ADC_EN		BIT(11)
	#define TSBUS_ADC_EN	BIT(10)
	#define TSBAT_ADC_EN	BIT(9)

/* Register 50h 51h*/
#define NU6601_REG_VAC_ADC0					(0x51)
#define NU6601_REG_VAC_ADC1					(0x50)
	#define VAC_ADC_LSB         (2500) /* uV */

/* Register 52h 53h*/
#define NU6601_REG_VBUS_ADC0					(0x53)
#define NU6601_REG_VBUS_ADC1					(0x52)
	#define VBUS_ADC_LSB         (2500) /* uV */

/* Register 54h 55h*/
#define NU6601_REG_VPMID_ADC0					(0x53)
#define NU6601_REG_VPMID_ADC1					(0x52)
	#define VPMID_ADC_LSB         (0) /* uV */

/* Register 56h 57h*/
#define NU6601_REG_VSYS_ADC0					(0x57)
#define NU6601_REG_VSYS_ADC1					(0x56)
	#define VSYS_ADC_LSB         (667)

/* Register 58h 59h*/
#define NU6601_REG_VBAT_ADC0					(0x59)
#define NU6601_REG_VBAT_ADC1					(0x58)
	#define VBAT_ADC_LSB         (667)

/* Register 5Ah 5Bh*/
#define NU6601_REG_VBATPN_ADC0					(0x5B)
#define NU6601_REG_VBATPN_ADC1					(0x5A)
	#define VBATPN_ADC_LSB         (667)

/* Register 5Ch 5Dh*/
#define NU6601_REG_IBUS_ADC0					(0x5D)
#define NU6601_REG_IBUS_ADC1					(0x5C)
	#define IBUS_ADC_LSB         (667)

/* Register 5Eh 5Fh*/
#define NU6601_REG_IBAT_ADC0					(0x5F)
#define NU6601_REG_IBAT_ADC1					(0x5E)
	#define IBAT_ADC_LSB         (667)

/* Register 60h 61h*/
#define NU6601_REG_TDIE_ADC0					(0x61)
#define NU6601_REG_TDIE_ADC1					(0x60)
	#define TDIE_ADC_LSB         (267)

/* Register 62h 63h*/
#define NU6601_REG_TSBUS_ADC0					(0x63)
#define NU6601_REG_TSBUS_ADC1					(0x62)
	#define TSBUS_ADC_LSB         (1)

/* Register 64h 65h*/
#define NU6601_REG_TSBAT_ADC0					(0x65)
#define NU6601_REG_TSBAT_ADC1					(0x64)
	#define TSBAT_ADC_LSB         (1)

/* Register 66h 67h*/
#define NU6601_REG_BATID_ADC0					(0x67)
#define NU6601_REG_BATID_ADC1					(0x66)
	#define BATID_ADC_LSB         (1)


/* ========== HK_ADC Trim Registers END=============== */

/* ========== Charger LED Registers =============== */
/* charge Registers*/
/* Register 10h */
#define NU6601_REG_CHGINT_STAT					(0x10)
	#define CHG_OK_INT_STAT				BIT(7)
	#define CHG_FSM_INT_STAT			BIT(6)
	#define CHG_GD_INT_STAT				BIT(5)
	#define CHG_0V_INT_STAT				BIT(4)
	#define CHG_DET_DONE_INT_STAT		BIT(3)

#define NU6601_REG_CHGINT_FLAG					(0x11)
	#define CHG_OK_INT_FLAG				BIT(7)
	#define CHG_FSM_INT_FLAG			BIT(6)
	#define CHG_GD_INT_FLAG				BIT(5)
	#define CHG_0V_INT_FLAG				BIT(4)
	#define CHG_DET_DONE_INT_FLAG		BIT(3)

#define NU6601_REG_CHGINT_MASK					(0x12)
	#define CHG_OK_INT_MASK				BIT(7)
	#define CHG_FSM_INT_MASK			BIT(6)
	#define CHG_GD_INT_MASK				BIT(5)
	#define CHG_0V_INT_MASK				BIT(4)
	#define CHG_DET_DONE_INT_MASK		BIT(3)

#define NU6601_REG_CHGINT_FE_EN					(0x13)
	#define CHG_OK_INT_FE_EN			BIT(7)
	#define CHG_FSM_INT_FE_EN			BIT(6)
	#define CHG_GD_INT_FE_EN			BIT(5)
	#define CHG_0V_INT_FE_EN			BIT(4)
	#define CHG_DET_DONE_INT_FE_EN		BIT(3)

#define NU6601_REG_CHG_STATUS					(0x20)
	#define CHG_FSM_MASK				(0xF0)
	#define CHG_FSM_SHIFT				(4)
	#define VSYS_RESET					(0x01)
	#define IN_SHIP_MODE 				(0x02)
	#define BFET_SOFT_START				(0x03)
	#define SYSTEM_ON_BATTERY			(0x04)
	#define CHARGER_BUCK_SOFT_START1	(0x05)
	#define CHARGER_BUCK_SOFT_START2	(0x06)
	#define SYSTEM_ON_CHARGER			(0x07)
	#define CHARGER_PAUSED				(0x08)
	#define TRICKLE_CHARGING			(0x09)
	#define PRE_CHARGING				(0x0A)
	#define LINEAR_FAST_CHARGING		(0x0B)
	#define CC_CHARGING					(0x0C)
	#define CV_CHARGING					(0x0D)
	#define CHARGE_TERMINATION			(0x0E)
	#define VSYS_IS_OFF					(0x0F)

	#define INPUT_OK				BIT(3)
	#define BUCK_OK					BIT(2)
	#define BATT_OK					BIT(1)
	#define CHG_DIG_OK				BIT(0)
	
#define NU6601_REG_CHG_OK_FLAG					(0x21)
	#define INPUT_OK_FLAG				BIT(3)
	#define BUCK_OK_FLAG				BIT(2)
	#define BATT_OK_FLAG				BIT(1)
	#define CHG_DIG_OK_FLAG				BIT(0)

#define NU6601_SHIFT_BST_OLPI		7
#define NU6601_SHIFT_BST_MIDOVI		6
#define NU6601_SHIFT_BST_BATUVI		5
#define NU6601_SHIFT_PUMPX_DONEI	1
#define NU6601_SHIFT_ADC_DONEI		0

#define NU6601_MASK_BST_OLPI	(1 << NU6601_SHIFT_BST_OLPI)
#define NU6601_MASK_BST_MIDOVI	(1 << NU6601_SHIFT_BST_MIDOVI)
#define NU6601_MASK_BST_BATUVI	(1 << NU6601_SHIFT_BST_BATUVI)
#define NU6601_MASK_PUMPX_DONEI	(1 << NU6601_SHIFT_PUMPX_DONEI)
#define NU6601_MASK_ADC_DONEI	(1 << NU6601_SHIFT_ADC_DONEI)

/* Register 30h */
#define NU6601_REG_PRE_CHG_CFG					(0x30)
	#define MIN_TRKL_CHG_TIME_MASK		(0xC0)
	#define MIN_TRKL_CHG_TIME_SHIFT		(6)
	#define MIN_TRKL_CHG_TIME_100MS		(0x00)
	#define MIN_TRKL_CHG_TIME_2S		(0x01)
	#define MIN_TRKL_CHG_TIME_4S		(0x02)
	#define MIN_TRKL_CHG_TIME_6S		(0x03)

	#define VPRE_THRESHOLD_MASK		(0x30)
	#define VPRE_THRESHOLD_SHIFT	(4)
	#define VPRE_THRESHOLD_2_9V		(0x00)
	#define VPRE_THRESHOLD_3V		(0x01)
	#define VPRE_THRESHOLD_3_1V		(0x02)
	#define VPRE_THRESHOLD_3_2V		(0x03)

	#define IPRE_LIMIT_MASK			(0x0F)
	#define IPRE_LIMIT_SHIFT		(0)
	#define IPRE_LIMIT_LSB_MA		(50)
	#define IPRE_LIMIT_MIN		(0)
	#define IPRE_LIMIT_MAX		(500)

/* Register 31h */
#define NU6601_REG_VSYS_MIN					(0x31)
	#define VBAT_TRACK_MASK		(0x30)
	#define VBAT_TRACK_SHIFT		(4)
	#define VBAT_TRACK_50MV			(0x00)
	#define VBAT_TRACK_100MV		(0x01)
	#define VBAT_TRACK_150MV		(0x02)
	#define VBAT_TRACK_200MV		(0x03)

	#define VSYS_MIN_MASK		(0x07)
	#define VSYS_MIN_SHIFT		(0x00)
	#define VSYS_MIN_BASE		(2600)
	#define VSYS_MIN_LV1_LSB	(200)
	#define VSYS_MIN_LV1		(4)
	#define VSYS_MIN_LV2_LSB	(100)

/* Register 32h */
#define NU6601_REG_ICHG_CC					(0x32)
	#define ICHG_CC_SEL_MASK		(0x7F)
	#define ICHG_CC_SEL_SHIFT		(0)
	#define ICHG_CC_SEL_LSB_MA		(50)
	#define ICHG_CC_MAX_MA		(4000)

/* Register 34h */
#define NU6601_REG_VBAT_REG					(0x34)
	#define BATSNS_EN_MASK		(0x80)
	#define BATSNS_EN_SHIFT		(7)
	#define BATSNS_EN_ENABLE	(1)
	#define BATSNS_EN_DISABLE	(0)

	#define VBAT_REG_SEL_MASK			(0x7F)
	#define VBAT_REG_SEL_SHIFT			(0)
	#define VBAT_REG_SEL_FIXED_OFFSET	(3840)
	#define VBAT_REG_SEL_LSB_MV			(8)
	#define VBAT_MIN		(3840)
	#define VBAT_MAX		(4856)

/* Register 36h */
#define NU6601_REG_AFVC_CFG					(0x36)
	#define AFVC_RES_SEL_MASK 		(0xE0)
	#define AFVC_RES_SEL_SHIFT 		(5)
	#define AFVC_RES_SEL_LSB 		(10)

	#define AFVC_VCLAMP_MASK 		(0x1C)
	#define AFVC_VCLAMP_SHIFT 		(2)
	#define AFVC_VCLAMP_LSB 		(16)

	#define AFVC_ISTOP_MASK 		(0x3)
	#define AFVC_ISTOP_SHIFT 		(0)
	#define AFVC_ISTOP_0MA			(0x00)
	#define AFVC_ISTOP_200MA 		(0x01)
	#define AFVC_ISTOP_300MA 		(0x10)
	#define AFVC_ISTOP_400MA 		(0x11)

/* Register 37h */
#define NU6601_REG_CHG_TERM_CFG				(0x37)
	#define TERM_EN_MASK		(0x80)
	#define TERM_EN_SHIFT		(7)
	#define TERM_ENABLE			(1)
	#define TERM_DISABLE		(0)

	#define ITERM_DGL_MASK		(0x60)
	#define ITERM_DGL_SHIFT		(5)
	#define ITERM_DGL_64MS		(0x00)
	#define ITERM_DGL_256MS		(0x01)
	#define ITERM_DGL_512MS		(0x02)
	#define ITERM_DGL_1024MS	(0x03)

	#define ITERM_MASK			(0x1F)
	#define ITERM_SHIFT			(0)
	#define ITERM_FIXED_OFFSET	(100)
	#define ITERM_LSB_MA		(50)
	#define ITERM_MIN		(100)
	#define ITERM_MAX		(1650)

/* Register 38h */
#define NU6601_REG_RECHG_CFG				(0x38)
	#define RECHG_DIS_MASK			(0x08)
	#define RECHG_DIS_SHIFT			(3)
	#define RECHG_DIS_ENABLE		(0)
	#define RECHG_DIS_DISABLE		(1)

	#define VBAT_RECHG_MASK			(0x04)
	#define VBAT_RECHG_SHIFT			(2)

/* Register 3Ah */
#define NU6601_REG_CHG_CTRL						(0x3A)
	#define CHG_EN_CMD_MASK		(0x80)
	#define CHG_EN_CMD_SHIFT	(7)
	#define CHG_ENABLE			(1)
	#define CHG_DISABLE			(0)

	#define CHG_PAUSE_CMD_MASK		(0x40)
	#define CHG_PAUSE_CMD_SHIFT		(6)
	#define CHG_PAUSE				(1)
	#define CHG_CONTINUE			(0)

	#define CHG_EN_CMD_SEL_MASK		(0x80)
	#define CHG_EN_CMD_SEL_SHIFT	(7)

/* Register 3Fh */
#define NU6601_REG_QRB_CTRL						(0x3F)

	#define QRB_OK_MASK		(0x80)
	#define QRB_OK_SHIFT	(7)
	#define QRB_OK_ENABLE			(1)

	#define QRB_AUTO_EN_MASK		(0x04)
	#define QRB_AUTO_EN_SHIFT	(2)
	#define QRB_AUTO_EN_ENABLE			(1)
	#define QRB_AUTO_EN_DISABLE			(0)

	#define QRB_FORCE_MASK		(0x03)
	#define QRB_FORCE_SHIFT		(0)
	#define QRB_FORCE_ON			(0x11)
	#define QRB_FORCE_OFF			(0x10)

/*USB Input Registers*/

/* Register 0x40h */
#define NU6601_REG_VAC_OVP					(0x40)
	#define VAC_OVP_THD_MASK				(0x70)
	#define VAC_OVP_THD_SHIFT				(4)

	#define VAC_OVP_IGNORE_UV_MASK				(0x02)
	#define VAC_OVP_IGNORE_UV_SHIFT				(1)

/* Register 0x41h */
#define NU6601_REG_VBUS_OVP_CTRL			(0x41)
	#define VBUS_OVP_THD_MASK				(0xC0)
	#define VBUS_OVP_THD_SHIFT				(6)

/* Register 0x42h */
#define NU6601_REG_IPD_CTRL					(0x42)
	#define FORCE_ON_IPD_VAC_MASK		(0x20)
	#define FORCE_ON_IPD_VAC_SHIFT		(5)

	#define FORCE_ON_IPD_VBUS_MASK		(0x10)
	#define FORCE_ON_IPD_VBUS_SHIFT		(4)

	#define FORCE_ON_IPD_VSYS_MASK		(0x02)
	#define FORCE_ON_IPD_VSYS_SHIFT		(1)

/* Register 0x45h */
#define NU6601_REG_VBUS_DET_STATUS				(0x45)
	#define VBUS_GD_MASK		(0x80)
	#define VBUS_GD_SHIFT		(7)

/* Register 0x48h */
#define NU6601_REG_VINDPM					(0x48)
	#define VINDPM_DIS_MASK				(0x80)
	#define VINDPM_DIS_SHIFT			(7)
	#define VINDPM_ENABLE				(0)
	#define VINDPM_DISABLE				(1)

	#define VINDPM_TRK_MASK				(0x60)
	#define VINDPM_TRK_SHIFT			(5)
	#define VINDPM_TRK_0MV				(0)
	#define VINDPM_TRK_150MV			(1)
	#define VINDPM_TRK_200MV			(2)
	#define VINDPM_TRK_250MV			(3)

	#define VINDPM_ABS_MASK				(0x1F)
	#define VINDPM_ABS_SHIFT			(0)
	#define VINDPM_ABS_INTER_VALUE		(10)
	#define VINDPM_ABS_OFFSET			(4000) //mV
	#define VINDPM_ABS_OFFSET1			(7000)
	#define VINDPM_ABS_LSBL				(100)
	#define VINDPM_ABS_LSBH				(200) 
	#define VINDPM_ABS_MIN				(4000) //mV
	#define VINDPM_ABS_MAX				(11000) //mV

/* Register 0x49h */
#define NU6601_REG_IINDPM					(0x49)
	#define IINDPM_DIS_MASK				(0x80)
	#define IINDPM_DIS_SHIFT			(7)
	#define IINDPM_ENABLE				(0)
	#define IINDPM_DISABLE				(1)

	#define USB_SUSPEND_MASK			(0x40)
	#define USB_SUSPEND_SHIFT			(6)
	#define USB_NORMAL					(0)
	#define USB_SUSPEND					(1)

	#define IINDPM_SW_MASK			(0x3F)
	#define IINDPM_SW_SHIFT			(0)
	#define IINDPM_SW_OFFSET		(100)
	#define IINDPM_SW_LSB			(50) 
	#define IINDPM_MIN				100
	#define IINDPM_MAX				3250

/* Register 0x4Ah */
#define NU6601_REG_IIN_MAX					(0x4A)
	#define IIN_MAX_OVRD_MASK		(0x80)
	#define IIN_MAX_OVRD_SHIFT		(7)
	#define IIN_MAX_OVRD			(1)

	#define IIN_MAX_STATUS_MASK		(0x3F)
	#define IIN_MAX_STATUS_SHIFT	(0)
	#define IIN_MAX_STATUS_OFFSET	(100)
	#define IIN_MAX_STATUS_LSB		(50) 

/* Register 0x4Bh */
#define NU6601_REG_IIN_FINAL					(0x4B)
	#define IIN_FINAL_OVRD_MASK		(0x80)
	#define IIN_FINAL_OVRD_SHIFT	(7)
	#define IIN_FINAL_OVRD			(1)

/* Register 0x4Ch */
#define NU6601_REG_ICO_CTRL					(0x4C)
	#define ICO_EN_MASK				(0x80)
	#define ICO_EN_SHIFT			(7)
	#define ICO_ENABLE				(1)
	#define ICO_DISABLE				(0)

	#define ICO_RESTART_MASK		(0x10)
	#define ICO_RESTART_SHIFT		(4)
	#define ICO_RESTART				(1)

/*Buck Boost Registers*/
/* Register 50h */
#define NU6601_REG_BUCK_CTRL					(0x50)
	#define BUCK_HS_IPEAK_MASK			(0xF0)
	#define BUCK_HS_IPEAK_SHIFT			(4)
	#define BUCK_HS_IPEAK_BASE			(500)
	#define BUCK_HS_IPEAK_LSB			(500)
	#define BUCK_HS_IPEAK_MAX_8A		(8000)

	#define BUCK_FSW_SEL_MASK			(0x03)
	#define BUCK_FSW_SEL_SHIFT			(0)
	#define BUCK_FSW_SEL_500K			(0)
	#define BUCK_FSW_SEL_1000K			(1)
	#define BUCK_FSW_SEL_1500K			(2)
	#define BUCK_FSW_SEL_2000K			(3)

/* Register 51h */
#define NU6601_REG_VSYS_OVP						(0x51)
	#define VSYS_OVP_THD_MASK				(0x70)
	#define VSYS_OVP_THD_SHIFT				(4)

/* Register 54h */
#define NU6601_REG_BOOST_CTRL					(0x54)
	#define BOOST_EN_MASK				(0x80)
	#define BOOST_EN_SHIFT				(7)
	#define BOOST_ENABLE				(1)
	#define BOOST_DISABLE				(0)

	#define BOOST_STOP_CLR_MASK			(0x40)
	#define BOOST_STOP_CLR_SHIFT		(6)
	#define BOOST_STOP_CLR				(1)
	
	#define BOOST_STOP_STAT_MASK		(0x20)
	#define BOOST_STOP_STAT_SHIFT		(5)

	#define BOOST_VOUT_MASK				(0x0F)
	#define BOOST_VOUT_SHIFT			(0)
	#define BOOST_VOUT_BASE				(3900)
	#define BOOST_VOUT_LSB				(100)
	#define BOOST_VOUT_MIN				(3900)
	#define BOOST_VOUT_MAX				(5400)

/* Register 55h */
#define NU6601_REG_BOOST_ILIM					(0x55)
	#define BOOST_IOUT_LIM_STAT_MASK	(0x80)
	#define BOOST_IOUT_LIM_STAT_SHFIT	(7)
	#define BOOST_IOUT_LIM_BY_IOUT		(1)
	#define BOOST_IOUT_LIM_BY_VBUS		(0)

	#define BOOST_IOUT_LIM_MASK			(0x70)
	#define BOOST_IOUT_LIM_SHIFT		(4)
	#define BOOST_IOUT_LIM_500MA		(0)
	#define BOOST_IOUT_LIM_1000MA		(1)
	#define BOOST_IOUT_LIM_1500MA		(2)
	#define BOOST_IOUT_LIM_2000MA		(3)
	#define BOOST_IOUT_LIM_2500MA		(4)
	#define BOOST_IOUT_LIM_3000MA		(5)

	#define BOOST_LS_IPEAK_MASK			(0x0F)
	#define BOOST_LS_IPEAK_SHIFT		(0)
	#define BOOST_LS_IPEAK_BASE			(500)
	#define BOOST_LS_IPEAK_LSB			(500)
	#define BOOST_LS_IPEAK_MAX_8A		(8000)

/* Register 57h */
#define NU6601_REG_BOOST_OP_STAT				(0x57)
	#define BOOST_FSM_MASK				(0x1c)
	#define BOOST_FSM_SHIFT				(2)
	#define BOOST_GOOD					(4)

/* Register 5Ah */
#define NU6601_REG_BOOST_PORT_EN				(0x5A)
	#define LFET_OCP_EN_MASK			(0x10)
	#define LFET_OCP_EN_SHIFT			(4)
	#define LFET_OCP_EN					(1)
	#define LFET_OCP_DISABLE			(0)

/* Register 5Bh */
#define NU6601_REG_BOOST_PORT_CFG			(0x5B)
	#define	VBAT_LOW_MASK			(0x03)
	#define VBAT_LOW_SHIFT			(0)
	#define VBAT_LOW_2P6V			(0)
	#define VBAT_LOW_2P8V			(1)
	#define VBAT_LOW_3V				(2)
	#define VBAT_LOW_3P2V			(3)


/* Register 5Eh */
#define NU6601_REG_BUBO_DRV_CFG				(0x5E)
	#define HS_DRV_MASK			(0x0C)
	#define	HS_DRV_SHIFT		(2)
	#define HS_DRV_25P		(0)
	#define HS_DRV_50P		(1)
	#define HS_DRV_75P		(2)
	#define HS_DRV_100P		(3)

	#define LS_DRV_MASK			(0x03)
	#define	LS_DRV_SHIFT		(0)
	#define LS_DRV_25P		(0)
	#define LS_DRV_50P		(1)
	#define LS_DRV_75P		(2)
	#define LS_DRV_100P		(3)

/* Register 6Ch */
#define NU6601_REG_CID_EN				(0x6c)
	#define RID_CID_SEL_MASK	(0x2)
	#define RID_CID_SEL_SHIFT	(1)
	#define SEL_CID				(1)
	#define SEL_RID				(0)

	#define CID_EN_MASK			(0x1)
	#define CID_EN_SHIFT		(0)
	#define CID_EN				(1)
	#define CID_DIS				(0)


/*Battery inferface Registers*/
/* Register 74h */
#define NU6601_REG_BATFET_STATUS				(0x74)
	#define BSM_ACTIVE_MASK			(0x80)
	#define BSM_ACTIVE_SHIFT		(7)

/* Register 75h */
#define NU6601_REG_QBAT_SS						(0x75)
	#define BFET_SS_FSM_MASK			(0x7F)
	#define BFET_SS_FSM_SHIFT			(0)

/* Register 78h */
#define NU6601_REG_BSM_CFG3					(0x78)
	#define BSM_SLOW_OFF_DGL_MASK		(0x0c)
	#define BSM_SLOW_OFF_DGL_SHIFT		(2)
	#define BSM_SLOW_OFF_DGL_10US		(0)
	#define BSM_SLOW_OFF_DGL_20US		(1)
	#define BSM_SLOW_OFF_DGL_250US		(2)
	#define BSM_SLOW_OFF_DGL_500US		(3)

/* Register 79h */
#define NU6601_REG_SHIP_MODE_CTRL				(0x79)
	#define BATFET_DIS_MASK			(0x80)
	#define BATFET_DIS_SHIFT		(7)
	#define BATFET_DIS			(1)
	#define BATFET_DIS_DLY_MASK		(0x40)
	#define BATFET_DIS_DLY_SHIFT		(6)
	#define BATFET_DIS_DLY			(0)

/* Register 7Ah */
#define NU6601_REG_BFET_CFG				(0x7A)
	#define BFET_MAX_VGS_CLAMP_EN_MASK			(0x08)
	#define BFET_MAX_VGS_CLAMP_EN_SHIFT			(3)

	#define BFET_MIN_VGS_CLAMP_EN_MASK			(0x04)
	#define BFET_MIN_VGS_CLAMP_EN_SHIFT			(2)

/*LED Registers*/
#define LED_OFF						(0)
/* Register 88h */
#define NU6601_LED_FAULT_ACT_EN				(0x88)
	#define CLR_LED_STOP_MASK			(0x80)
	#define CLR_LED_STOP_SHIFT			(7)
	#define CLR_LED_STOP				(1)

	#define NBOOST_DET_ENABLE_MASK			(0x40)
	#define NBOOST_DET_ENABLE_SHIFT			(6)
	#define NBOOST_DET_ENABLE			(1)
	#define NBOOST_DET_DISABLE			(0)

	#define VSYS_LVP_ENABLE_MASK			(0x20)
	#define VSYS_LVP_ENABLE_SHIFT			(5)
	#define VSYS_LVP_ENABLE				(1)
	#define VSYS_LVP_DISABLE			(0)

	#define VDD_OVP_ENABLE_MASK			(0x10)
	#define VDD_OVP_ENABLE_SHIFT			(4)
	#define VDD_OVP_ENABLE				(1)
	#define VDD_OVP_DISABLE				(0)

	#define VDD_UVLO_ENABLE_MASK			(0x08)
	#define VDD_UVLO_ENABLE_SHIFT			(3)
	#define VDD_UVLO_ENABLE				(1)
	#define VDD_UVLO_DISABLE			(0)

	#define LED_SHORT_ENABLE_MASK			(0x04)
	#define LED_SHORT_ENABLE_SHIFT			(2)
	#define LED_SHORT_ENABLE			(1)
	#define LED_SHORT_DISABLE			(0)

	#define THERMAL_ALARM_ENABLE_MASK		(0x20)
	#define THERMAL_ALARM_ENABLE_SHIFT		(1)
	#define THERMAL_ALARM_ENABLE			(1)
	#define THERMAL_ALARM_DISABLE			(0)

	#define THERMAL_SHUTDOWN_ENABLE_MASK		(0x01)
	#define THERMAL_SHUTDOWN_ENABLE_SHIFT		(0)
	#define THERMAL_SHUTDOWN_ENABLE			(1)
	#define THERMAL_SHUTDOWN_DISABLE		(0)

/* Register 90h */
#define NU6601_REG_LED_CTRL				(0x90)
	#define LED_FUNCTION_EN_MASK			(0x80)
	#define LED_FUNCTION_EN_SHIFT			(7)
	#define LED_FUNCTION_ENABLE			(1)
	#define LED_FUNCTION_DISABLE			(0)

	#define STROBE_EN_MASK				(0x40)
	#define STROBE_EN_SHIFT				(6)
	#define STROBE_ENABLE				(1)
	#define STROBE_DISABLE				(0)

	#define TORCH_EN_MASK				(0x20)
	#define TORCH_EN_SHIFT				(5)
	#define TORCH_ENABLE				(1)
	#define TORCH_DISABLE				(0)

	#define FT_TX_EN_MASK				(0x10)
	#define FT_TX_EN_SHIFT				(4)
	#define FT_TX_ENABLE				(1)
	#define FT_TX_DISABLE				(0)

	#define FT_TX_CURRENT_RATIO_MASK		(0x0C)
	#define FT_TX_CURRENT_RATIO_SHIFT		(2)
	#define FT_TX_RATIO_80				(0)
	#define FT_TX_RATIO_60				(1)
	#define FT_TX_RATIO_40				(2)
	#define FT_TX_RATIO_20				(3)

	#define LED1_EN_MASK				(0x02)
	#define LED1_EN_SHIFT				(1)
	#define LED1_ENABLE				(1)
	#define LED1_DISABLE				(0)

	#define LED2_EN_MASK				(0x01)
	#define LED2_EN_SHIFT				(0)
	#define LED2_ENABLE				(1)
	#define LED2_DISABLE				(0)

	#define LED_STROBE_CURRENT_MASK			(0x7F)

/* Register 91h */
	#define NU6601_REG_FLED1_BR			(0x91)
	#define CLEAR_STOP_BAK_MASK			(0x80)
	#define CLEAR_STOP_BAK_EN_SHIFT			(7)
	#define CLEAR_STOP_BAK_ENABLE			(1)

	#define LED_STROBE_CURRENT_BASE			(50000) /* uA */
	#define LED_STROBE_CURRENT_LSB			(12500)
	#define LED_STROBE_CURRENT_MIN			(50) /* mA */
	#define LED_STROBE_CURRENT_MAX			(1500) /* mA */

/* Register 92h */
#define NU6601_REG_FLED2_BR				(0x92)

/* Register 93h */
#define NU6601_REG_LED_TIME_CFG_A			(0x93)
	#define LED1_SHORT_DET_TIME_MASK		(0xC0)
	#define LED1_SHORT_DET_TIME_SHIFT		(7)
	#define LED1_SHORT_DET_TIME_1MS			(0)
	#define LED1_SHORT_DET_TIME_2MS			(1)
	#define LED1_SHORT_DET_TIME_5MS			(2)
	#define LED1_SHORT_DET_TIME_10MS		(3)

	#define LED_STROBE_TIMEOUT_MASK			(0x1F)
	#define LED_STROBE_TIMEOUT_SHIFT		(0)
	#define LED_STROBE_TIMEOUT_BASE			(10)
	#define LED_STROBE_TIMEOUT_LEVEL1		(100) //ms
	#define LED_STROBE_TIMEOUT_LEVEL1_LSB		(10)
	#define LED_STROBE_TIMEOUT_LEVEL2		(800) //ms
	#define LED_STROBE_TIMEOUT_LEVEL2_LSB		(50)
	#define LED_STROBE_TIMEOUT_MIN			(50)
	#define LED_STROBE_TIMEOUT_MAX			(800)

/* Register 94h */
#define NU6601_REG_LED_TIME_CFG_B			(0x94)
	#define LED1_RAMP_STEP_TIME_MASK		(0xC0)
	#define LED1_RAMP_STEP_TIME_SHIFT		(6)
	#define LED1_RAMP_STEP_TIME_10US		(0)
	#define LED1_RAMP_STEP_TIME_50US		(1)
	#define LED1_RAMP_STEP_TIME_100US		(2)
	#define LED1_RAMP_STEP_TIME_200US		(3)

	#define LED2_RAMP_STEP_TIME_MASK		(0x30)
	#define LED2_RAMP_STEP_TIME_SHIFT		(4)
	#define LED2_RAMP_STEP_TIME_10US		(0)
	#define LED2_RAMP_STEP_TIME_50US		(1)

/* Register 95h */
#define NU6601_REG_TLED1_BR				(0x95)
	#define LED1_TORCH_CURRENT_MASK			(0x7F)
	#define LED_TROCH_CURRENT_BASE			(50000) /* uA */
	#define LED_TROCH_CURRENT_LSB			(12500) /*uA */
	#define LED_TROCH_CURRENT_MIN			(50) /* mA */
	#define LED_TROCH_CURRENT_MAX			(500) /* mA */

/* Register 96h */
#define NU6601_REG_TLED2_BR				(0x96)
	#define LED2_TORCH_CURRENT_MASK			(0x7F)

/* Register 97h */
#define NU6601_REG_LED_PRO_CFG_A			(0x97)
	#define TEMP_ALARM_DEB_MASK			(0x40)
	#define TEMP_ALARM_DEB_SHIFT			(6)
	#define TEMP_ALARM_DEB_100US			(1)
	#define TEMP_ALARM_DEB_2MS			(0)

	#define BOOST_HDRM_EN_MASK			(0x20)
	#define BOOST_HDRM_EN_SHIFT			(5)
	#define BOOST_HDRM_ENABLE			(1)
	#define BOOST_HDRM_DISABLE			(1)

	#define BOOST_HDRM_SEL_MASK			(0x18)
	#define BOOST_HDRM_SEL_SHIFT			(3)
	#define BOOST_HDRM_SEL_300MV			(0)
	#define BOOST_HDRM_SEL_400MV			(1)
	#define BOOST_HDRM_SEL_500MV			(2)
	#define BOOST_HDRM_SEL_600MV			(3)

	#define LED_LVP_THR_MASK			(0x7)
	#define LED_LVP_THR_SHIFT			(0)
	#define LED_LVP_THR_BASE			(2800) //mV
	#define LED_LVP_THR_LSB				(100)


/* ========== Charger LED Register End ============ */

/* ========== DPDM Register =============== */

/*bc1.2*/
#define NU6601_REG_BC12INT_STAT					(0xB0)
	#define BC12_DET_DONE_INT_STAT		BIT(7)
	#define DP_OV_INT_STAT				BIT(6)
	#define DM_OV_INT_STAT				BIT(5)
	#define RID_DETED_INT_STAT			BIT(4)
	#define DCD_TIMEOUT_INT_STAT		BIT(3)

#define NU6601_REG_BC12INT_FLAG					(0xB1)
	#define BC12_DET_DONE_INT_FLAG		BIT(7)
	#define DP_OV_INT_FLAG				BIT(6)
	#define DM_OV_INT_FLAG				BIT(5)
	#define RID_DETED_INT_FLAG			BIT(4)
	#define DCD_TIMEOUT_INT_FLAG		BIT(3)

#define NU6601_REG_BC12INT_MASK (0xB2)
	#define BC12_DET_DONE_INT_MASK		BIT(7)
	#define DP_OV_INT_MASK				BIT(6)
	#define DM_OV_INT_MASK				BIT(5)
	#define RID_DETED_INT_MASK			BIT(4)
	#define DCD_TIMEOUT_INT_MASK		BIT(3)

#define NU6601_REG_BC12INT_FE_EN					(0xB3)
	#define BC12_DET_DONE_INT_FE_EN	BIT(7)
	#define DP_OV_INT_FE_EN				BIT(6)
	#define DM_OV_INT_FE_EN				BIT(5)
	#define RID_DETED_INT_FE_EN			BIT(4)
	#define DCD_TIMEOUT_INT_FE_EN		BIT(3)

#define NU6601_REG_BC12_TYPE						(0xB4)
	#define BC12_TYPE_MASK				(0x70)
	#define BC12_TYPE_SHIFT			(4)
	#define NORESULT_OR_DETECTERROR		(0x0)
	

	#define UNSTANDARD_TYPE_SHIT			(0)
	#define UNSTANDARD_TYPE_MASK			(0x07)

#define NU6601_REG_BC12_CTRL						(0xB5)
	#define DPDM_EN_MASK		(0x20)
	#define DPDM_EN_SHIFT		(5)
	#define DPDM_ENABLE			(1)
	#define DPDM_DISABLE		(0)

	#define BC12_EN_MASK				(0x02)
	#define BC12_EN_SHIFT				(1)
	#define BC12_ENABLE					(1)
	#define BC12_DISABLE				(0)

/* QC35 */
#define NU6601_REG_QC_DPDM_CTRL			(0xBA)
	#define QC_EN_MASK				(0x80)
	#define QC_EN_SHIFT				(7)
	#define QC_ENABLE				(0x1)
	#define QC_DISABLE				(0x0)

	#define QC_COMMAND_MASK			(0x40)
	#define QC_COMMAND_SHIFT		(6)
	#define QC_COMMAND				(1)

	#define QC_MODE_MASK			(0x30)
	#define QC_MODE_SHIFT			(4)
	#define QC20_5V					(3)
	#define QC20_9V					(1)
	#define QC20_12V				(0)
	#define QC30_5V					(2)

	#define QC3P5_PULSE_MASK			(0x0E)
	#define QC3P5_PULSE_SHIFT			(1)
/*
 *    #define NO_ACTION			(0)
 *    #define DP_16PULSE			(1)
 *    #define DM_16PULSE			(2)
 *    #define DPDM_3PULSE			(3)
 *    #define DPDM_2PULSE			(4)
 *    #define DP_COT_PULSE		(5)
 *    #define DM_COT_PULSE		(6)
 *
 */
	#define QC_FALLING_DET_MASK		(0x01)
	#define QC_FALLING_DET_SHIFT	(0)
	#define DET_TIME_1P5S			(0)
	#define DET_TIME_2P5S			(1)


/* ========== DPDM Register END=============== */


#endif /* __LINUX_NU6601_REG_H */

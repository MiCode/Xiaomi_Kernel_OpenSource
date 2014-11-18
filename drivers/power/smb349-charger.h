/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SMB349_CHARGER_H
#define SMB349_CHARGER_H

/* Config/Control registers */
#define CHG_CURRENT_CTRL_REG		0x0
#define CHG_OTH_CURRENT_CTRL_REG	0x1
#define VARIOUS_FUNC_REG		0x2
#define VFLOAT_REG			0x3
#define CHG_CTRL_REG			0x4
#define CHG_PIN_EN_CTRL_REG		0x6
#define THERM_A_CTRL_REG		0x7
#define OTG_TLIM_THERM_CTRL_REG		0xA
#define HARD_SOFT_TLIM_CTRL_REG		0xB
#define FAULT_INT_REG			0xC
#define STATUS_INT_REG			0xD

/* Command registers */
#define CMD_A_REG			0x30
#define CMD_B_REG			0x31

/* Revision register */
#define CHG_REVISION_REG		0x34

/* IRQ status registers */
#define IRQ_A_REG			0x35
#define IRQ_B_REG			0x36
#define IRQ_C_REG			0x37
#define IRQ_D_REG			0x38
#define IRQ_E_REG			0x39
#define IRQ_F_REG			0x3A

/* Status registers */
#define STATUS_C_REG			0x3D
#define STATUS_D_REG			0x3E
#define STATUS_E_REG			0x3F

/* Config bits */
#define CMD_A_CHG_ENABLE_BIT			BIT(1)
#define CMD_A_VOLATILE_W_PERM_BIT		BIT(7)
#define CMD_A_CHG_SUSP_EN_BIT			BIT(2)
#define CMD_A_CHG_SUSP_EN_MASK			BIT(2)
#define CMD_A_OTG_ENABLE_BIT			BIT(4)
#define CMD_A_OTG_ENABLE_MASK			BIT(4)
#define CMD_B_CHG_HC_ENABLE_BIT			BIT(0)
#define CMD_B_CHG_USB3_ENABLE_BIT		BIT(2)
#define CMD_B_CHG_USB_500_900_ENABLE_BIT	BIT(1)
#define CHG_CTRL_AUTO_RECHARGE_ENABLE_BIT	0x0
#define CHG_CTRL_CURR_TERM_END_CHG_BIT		0x0
#define CHG_CTRL_BATT_MISSING_DET_THERM_IO	(BIT(5) | BIT(4))
#define CHG_CTRL_AUTO_RECHARGE_MASK		BIT(7)
#define CHG_CTRL_CURR_TERM_END_MASK		BIT(6)
#define CHG_CTRL_BATT_MISSING_DET_MASK		(BIT(5) | BIT(4))
#define CHG_CTRL_RECHG_100MV_BIT		BIT(3)
#define CHG_CTRL_RECHG_50_100_MASK		BIT(3)
#define CHG_OTH_CTRL_USB_2_3_REG_CTRL_BIT	0
#define CHG_OTH_CTRL_USB_2_3_PIN_REG_MASK	BIT(1)
#define CHG_ITERM_MASK				0x1C
#define CHG_PIN_CTRL_USBCS_REG_BIT		0x0
#define CHG_PIN_CTRL_CHG_EN_LOW_PIN_BIT		(BIT(5) | BIT(6))
#define CHG_PIN_CTRL_CHG_EN_LOW_REG_BIT		0x0
#define CHG_PIN_CTRL_CHG_EN_MASK		(BIT(5) | BIT(6))
#define CHG_PIN_CTRL_USBCS_REG_MASK		BIT(4)
#define CHG_PIN_CTRL_APSD_IRQ_BIT		BIT(1)
#define CHG_PIN_CTRL_APSD_IRQ_MASK		BIT(1)
#define CHG_PIN_CTRL_CHG_ERR_IRQ_BIT		BIT(2)
#define CHG_PIN_CTRL_CHG_ERR_IRQ_MASK		BIT(2)
#define VARIOUS_FUNC_USB_SUSP_EN_REG_BIT	BIT(7)
#define VARIOUS_FUNC_USB_SUSP_MASK		BIT(7)
#define VARIOUS_FUNC_APSD_EN_BIT		BIT(2)
#define VARIOUS_FUNC_APSD_MASK			BIT(2)
#define FAULT_INT_HOT_COLD_HARD_BIT		BIT(7)
#define FAULT_INT_HOT_COLD_SOFT_BIT		BIT(6)
#define FAULT_INT_INPUT_UV_BIT			BIT(2)
#define FAULT_INT_AICL_COMPLETE_BIT		BIT(1)
#define STATUS_INT_CHG_TIMEOUT_BIT		BIT(7)
#define STATUS_INT_OTG_DETECT_BIT		BIT(6)
#define STATUS_INT_BATT_OV_BIT			BIT(5)
#define STATUS_INT_TERM_TAPER_BIT		BIT(4)
#define STATUS_INT_FAST_CHG_BIT			BIT(3)
#define STATUS_INT_MISSING_BATT_BIT		BIT(1)
#define STATUS_INT_LOW_BATT_BIT			BIT(0)
#define THERM_A_THERM_MONITOR_EN_BIT		0x0
#define THERM_A_THERM_MONITOR_EN_MASK		BIT(4)
#define VFLOAT_MASK				0x3F
#define SMB349_REV_MASK				0x0F
#define SMB349_REV_A4				0x4

/* IRQ status bits */
#define IRQ_A_HOT_HARD_BIT			BIT(6)
#define IRQ_A_COLD_HARD_BIT			BIT(4)
#define IRQ_A_HOT_SOFT_BIT			BIT(2)
#define IRQ_A_COLD_SOFT_BIT			BIT(0)
#define IRQ_B_BATT_MISSING_BIT			BIT(4)
#define IRQ_B_BATT_LOW_BIT			BIT(2)
#define IRQ_B_BATT_OV_BIT			BIT(6)
#define IRQ_B_PRE_FAST_CHG_BIT			BIT(0)
#define IRQ_C_TAPER_CHG_BIT			BIT(2)
#define IRQ_C_TERM_BIT				BIT(0)
#define IRQ_C_INT_OVER_TEMP_BIT			BIT(6)
#define IRQ_D_CHG_TIMEOUT_BIT			(BIT(0) | BIT(2))
#define IRQ_D_AICL_DONE_BIT			BIT(4)
#define IRQ_D_APSD_COMPLETE			BIT(6)
#define IRQ_E_INPUT_UV_BIT			BIT(0)
#define IRQ_E_INPUT_OV_BIT			BIT(2)
#define IRQ_E_AFVC_ACTIVE                       BIT(4)
#define IRQ_F_OTG_VALID_BIT			BIT(2)
#define IRQ_F_OTG_BATT_FAIL_BIT			BIT(4)
#define IRQ_F_OTG_OC_BIT			BIT(6)
#define IRQ_F_POWER_OK				BIT(0)

/* Status  bits */
#define STATUS_C_CHARGING_MASK			(BIT(1) | BIT(2))
#define STATUS_C_FAST_CHARGING			BIT(2)
#define STATUS_C_PRE_CHARGING			BIT(1)
#define STATUS_C_TAPER_CHARGING			(BIT(2) | BIT(1))
#define STATUS_C_CHG_ERR_STATUS_BIT		BIT(6)
#define STATUS_C_CHG_ENABLE_STATUS_BIT		BIT(0)
#define STATUS_C_CHG_HOLD_OFF_BIT		BIT(3)
#define STATUS_D_PORT_OTHER			BIT(0)
#define STATUS_D_PORT_SDP			BIT(1)
#define STATUS_D_PORT_DCP			BIT(2)
#define STATUS_D_PORT_CDP			BIT(3)
#define STATUS_D_PORT_ACA_A			BIT(4)
#define STATUS_D_PORT_ACA_B			BIT(5)
#define STATUS_D_PORT_ACA_C			BIT(6)
#define STATUS_D_PORT_ACA_DOCK			BIT(7)

/* Constants */
#define USB2_MIN_CURRENT_MA		100
#define USB2_MAX_CURRENT_MA		500
#define USB3_MAX_CURRENT_MA		900
#define AC_CHG_CURRENT_MASK		0x0F
#define SMB349_IRQ_REG_COUNT		6
#define SMB349_FAST_CHG_MIN_MA		1000
#define SMB349_FAST_CHG_STEP_MA		200
#define SMB349_FAST_CHG_MAX_MA		4000
#define SMB349_FAST_CHG_SHIFT		4
#define SMB_FAST_CHG_CURRENT_MASK	0xF0
#define SMB349_DEFAULT_BATT_CAPACITY	50

#define MIN_FLOAT_MV		3460
#define MAX_FLOAT_MV		4720
#define VFLOAT_STEP_MV		20

/* Termination currents */
#define CHG_ITERM_100MA			0x18
#define CHG_ITERM_200MA			0x0
#define CHG_ITERM_300MA			0x04
#define CHG_ITERM_400MA			0x08
#define CHG_ITERM_500MA			0x0C
#define CHG_ITERM_600MA			0x10
#define CHG_ITERM_700MA			0x14

#define IRQ_LATCHED_MASK	0x02
#define IRQ_STATUS_MASK		0x01
#define BITS_PER_IRQ		2

#define LAST_CNFG_REG		0x13
#define FIRST_CMD_REG		0x30
#define LAST_CMD_REG		0x33
#define FIRST_STATUS_REG	0x35
#define LAST_STATUS_REG		0x3F

#endif /* SMB349_CHARGER_H */

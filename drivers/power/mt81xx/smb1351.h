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

#define I2C_TRANSFER_RETRY 10

#define CONFIG_REG			0x0
#define OTHER_CURRENT_REG	0x1
#define VARIOUS_FUNC		0x2
#define FLOAT_VOLT_REG		0x3
#define CHARGE_CTRL_REG	0x4
#define STAT_CTRL_REG		0x5
#define ENABLE_CTRL_REG	0x6
#define THERM_CTRL_A_REG	0x7
#define WDT_TIMER_CTRL_REG 0x8
#define OTG_TLIM_CTRL_REG	0xA
#define TEMP_MONITOR_REG	0xB
#define STATUS_INT_REG		0xD
#define COMP_REG			0xE
#define FLEXCHARGER_REG	0x10
#define OTG_POWER_REG		0x14
#define CMD_REG_I2C			0x30
#define CMD_REG_IL			0x31
#define STATUS_REG			0x36
#define STATUS_REG_4		0x3A
#define IRQ_C_REG			0x42

#define SWITCHING_FREQ_MASK	0xC0
#define AUTO_AICL_LIMIT_MASK	0x10
#define BQ_CONFIG_ACCESS_MASK	0x40
#define FASTCHARGE_COMP_MASK	0x20
#define CURRENT_TERMINATION_MASK 0x40
#define INPUT_CURRENT_MODE_MASK	0x8
#define RECHARGE_STATE_MASK		0x10
#define TERMINATION_STATE_MASK	0x1
#define SOFTCOLD_LIMIT_MASK	0xC
#define SOFTHOT_LIMIT_MASK		0x3
#define USB_AC_MODE_MASK		0x1
#define USB_CTRL_MODE_MASK	0x3
#define ADAPTER_ID_MODE_MASK	0x3
#define CHARGER_CONFIG_MASK	0x70
#define ADAPTER_CONFIG_MASK	0x80
#define STAT_OUTPUT_CTRL_MASK	0x20
#define HOT_ALARM_MASK	0x30
#define ENABLE_PIN_MASK	0x60
#define CHARGE_TYPE_MASK	0x10
#define CHARGE_DONE_MASK	0x20
#define FLOAT_VOLT_MASK	0x3F
#define THERM_MON_MASK	0x10
#define FASTCHARGE_MASK	0xF0
#define PRECHARGE_MASK		0xE0
#define TERMINATION_MASK	0x1C
#define AC_IN_LIMIT_MASK	0xF
#define AICL_DONE_MASK		0x80
#define DCIN_LIMIT_MASK	0xF
#define SUSPEND_MODE_MASK	0x40
#define OTG_DCIN_CURRENT_MASK		0xC
#define CHARGING_STATE_MASK	0x6
#define WATCHDOG_TIMER_MASK	0x60
#define WATCHDOG_EN_MASK		0x1

extern int hw_charger_type_detection(void);
extern int slp_get_wake_reason(void);


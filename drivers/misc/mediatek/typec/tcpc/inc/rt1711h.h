/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LINUX_RT1711H_H
#define __LINUX_RT1711H_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_RT1711_DBG	0

/* RT1711H Private RegMap */

#define RT1711H_REG_CONFIG_GPIO0			(0x71)

#define RT1711H_REG_CONFIG_GPIO0			(0x71)

#define RT1711H_REG_PHY_CTRL1				(0x80)

#define RT1711H_REG_CLK_CTRL2				(0x87)
#define RT1711H_REG_CLK_CTRL3				(0x88)

#define RT1711H_REG_PRL_FSM_RESET			(0x8D)

#define RT1711H_REG_BMC_CTRL				(0x90)
#define RT1711H_REG_BMCIO_RXDZSEL			(0x93)
#define RT1711H_REG_VCONN_CLIMITEN			(0x95)

#define RT1711H_REG_RT_STATUS				(0x97)
#define RT1711H_REG_RT_INT					(0x98)
#define RT1711H_REG_RT_MASK					(0x99)

#define RT1711H_REG_IDLE_CTRL				(0x9B)
#define RT1711H_REG_INTRST_CTRL				(0x9C)
#define RT1711H_REG_WATCHDOG_CTRL			(0x9D)
#define RT1711H_REG_I2CRST_CTRL				(0X9E)

#define RT1711H_REG_SWRESET				(0xA0)
#define RT1711H_REG_TTCPC_FILTER			(0xA1)
#define RT1711H_REG_DRP_TOGGLE_CYCLE		(0xA2)
#define RT1711H_REG_DRP_DUTY_CTRL			(0xA3)
#define RT1711H_REG_BMCIO_RXDZEN			(0xAF)

#define RT1711H_REG_UNLOCK_PW_2				(0xF0)
#define RT1711H_REG_UNLOCK_PW_1				(0xF1)
#define RT1711H_REG_EFUSE5				(0xF6)

/*HUSB311 REGMAP*/

#define HUSB311_TCPC_V10_REG_VID					(0x00)
#define HUSB311_TCPC_V10_REG_PID					(0x02)
#define HUSB311_TCPC_V10_REG_DID					(0x04)
#define HUSB311_TCPC_V10_REG_TYPEC_REV				(0x06)
#define HUSB311_TCPC_V10_REG_PD_REV					(0x08)
#define HUSB311_TCPC_V10_REG_PDIF_REV				(0x0A)
#define HUSB311_TCPC_V10_REG_ALERT					(0x10)
#define HUSB311_TCPC_V10_REG_ALERT_MASK				(0x12)
#define HUSB311_TCPC_V10_REG_POWER_STATUS_MASK		(0x14)
#define HUSB311_TCPC_V10_REG_FAULT_STATUS_MASK		(0x15)
#define HUSB311_TCPC_V10_REG_HUSB311_TCPC_CTRL		(0x19)
#define HUSB311_TCPC_V10_REG_ROLE_CTRL				(0x1A)
#define HUSB311_TCPC_V10_REG_FAULT_CTRL				(0x1B)
#define HUSB311_TCPC_V10_REG_POWER_CTRL				(0x1C)
#define HUSB311_TCPC_V10_REG_CC_STATUS				(0x1D)
#define HUSB311_TCPC_V10_REG_POWER_STATUS			(0x1E)
#define HUSB311_TCPC_V10_REG_FAULT_STATUS			(0x1F)
#define HUSB311_TCPC_V10_REG_COMMAND				(0x23)
#define HUSB311_TCPC_V10_REG_MSG_HDR_INFO			(0x2e)
#define HUSB311_TCPC_V10_REG_RX_DETECT				(0x2f)
#define HUSB311_TCPC_V10_REG_RX_BYTE_CNT			(0x30)
#define HUSB311_TCPC_V10_REG_RX_BUF_FRAME_TYPE		(0x31)
#define HUSB311_TCPC_V10_REG_RX_HDR					(0x32)
#define HUSB311_TCPC_V10_REG_RX_DATA				(0x34)
#define HUSB311_TCPC_V10_REG_TRANSMIT				(0x50)
#define HUSB311_TCPC_V10_REG_TX_BYTE_CNT			(0x51)
#define HUSB311_TCPC_V10_REG_TX_HDR					(0x52)
#define HUSB311_TCPC_V10_REG_TX_DATA				(0x54)

/*HUSB311 XXX*/
#define HUSB311_REG_CONFIG_GPIO0			(0x71)
#define HUSB311_REG_PHY_CTRL1				(0x80)
#define HUSB311_REG_CLK_CTRL2				(0x87)
#define HUSB311_REG_CLK_CTRL3				(0x88)
#define HUSB311_REG_PRL_FSM_RESET			(0x8D)
#define HUSB311_REG_BMCIO_RXDZSEL			(0x93)
#define HUSB311_REG_VCONN_CLIMITEN			(0x95)
#define HUSB311_REG_IDLE_CTRL				(0x9B)
#define HUSB311_REG_BMCIO_RXDZEN			(0xAF)
#define HUSB311_REG_UNLOCK_PW_2				(0xF0)
#define HUSB311_REG_EFUSE5					(0xF6)

/*HUSB311 private*/
#define HUSB311_REG_BMC_CTRL				(0x90)
#define HUSB311_REG_RT_STATUS				(0x97)
#define HUSB311_REG_RT_INT					(0x98)
#define HUSB311_REG_RT_MASK					(0x99)
#define HUSB311_REG_INTRST_CTRL				(0x9C)
#define HUSB311_REG_WATCHDOG_CTRL			(0x9D)
#define HUSB311_REG_I2CRST_CTRL				(0X9E)
#define HUSB311_REG_SWRESET					(0xA0)
#define HUSB311_REG_TTCPC_FILTER			(0xA1)
#define HUSB311_REG_DRP_TOGGLE_CYCLE		(0xA2)
#define HUSB311_REG_DRP_DUTY_CTRL			(0xA3)
#define HUSB311_REG_CF					(0xcf)
/*
 * Device ID
 */

#define RT1711H_DID_A		0x2170
#define RT1711H_DID_B		0x2171
#define RT1711H_DID_C		0x2172

#define RT1715_DID_D			0x2173

/*
 * RT1711H_REG_PHY_CTRL1			(0x80)
 */

#define RT1711H_REG_PHY_CTRL1_SET( \
	retry_discard, toggle_cnt, bus_idle_cnt, rx_filter) \
	((retry_discard << 7) | (toggle_cnt << 4) | \
	(bus_idle_cnt << 2) | (rx_filter & 0x03))

/*
 * RT1711H_REG_CLK_CTRL2			(0x87)
 */

#define RT1711H_REG_CLK_DIV_600K_EN		(1<<7)
#define RT1711H_REG_CLK_BCLK2_EN		(1<<6)
#define RT1711H_REG_CLK_BCLK2_TG_EN		(1<<5)
#define RT1711H_REG_CLK_DIV_300K_EN		(1<<3)
#define RT1711H_REG_CLK_CK_300K_EN		(1<<2)
#define RT1711H_REG_CLK_BCLK_EN			(1<<1)
#define RT1711H_REG_CLK_BCLK_TH_EN		(1<<0)

/*
 * RT1711H_REG_CLK_CTRL3			(0x88)
 */

#define RT1711H_REG_CLK_OSCMUX_RG_EN	(1<<7)
#define RT1711H_REG_CLK_CK_24M_EN		(1<<6)
#define RT1711H_REG_CLK_OSC_RG_EN		(1<<5)
#define RT1711H_REG_CLK_DIV_2P4M_EN		(1<<4)
#define RT1711H_REG_CLK_CK_2P4M_EN		(1<<3)
#define RT1711H_REG_CLK_PCLK_EN			(1<<2)
#define RT1711H_REG_CLK_PCLK_RG_EN		(1<<1)
#define RT1711H_REG_CLK_PCLK_TG_EN		(1<<0)

/*
 * RT1711H_REG_BMC_CTRL				(0x90)
 */

#define RT1711H_REG_IDLE_EN				(1<<6)
#define RT1711H_REG_DISCHARGE_EN			(1<<5)
#define RT1711H_REG_BMCIO_LPRPRD			(1<<4)
#define RT1711H_REG_BMCIO_LPEN				(1<<3)
#define RT1711H_REG_BMCIO_BG_EN				(1<<2)
#define RT1711H_REG_VBUS_DET_EN				(1<<1)
#define RT1711H_REG_BMCIO_OSC_EN			(1<<0)

/*
 * RT1711H_REG_RT_STATUS				(0x97)
 */

#define RT1711H_REG_RA_DETACH				(1<<5)
#define RT1711H_REG_VBUS_80				(1<<1)

/*
 * RT1711H_REG_RT_INT				(0x98)
 */

#define RT1711H_REG_INT_RA_DETACH			(1<<5)
#define RT1711H_REG_INT_WATCHDOG			(1<<2)
#define RT1711H_REG_INT_VBUS_80				(1<<1)
#define RT1711H_REG_INT_WAKEUP				(1<<0)

/*
 * RT1711H_REG_RT_MASK				(0x99)
 */

#define RT1711H_REG_M_RA_DETACH				(1<<5)
#define RT1711H_REG_M_WATCHDOG				(1<<2)
#define RT1711H_REG_M_VBUS_80				(1<<1)
#define RT1711H_REG_M_WAKEUP				(1<<0)

/*
 * RT1711H_REG_IDLE_CTRL				(0x9B)
 */

#define RT1711H_REG_CK_300K_SEL				(1<<7)
#define RT1711H_REG_SHIPPING_OFF			(1<<5)
#define RT1711H_REG_ENEXTMSG				(1<<4)
#define RT1711H_REG_AUTOIDLE_EN				(1<<3)

/*
 * RT1711H_REG_EFUSE5					(0xF6)
 */

#define RT1711H_REG_M_VBUS_CAL				GENMASK(7, 5)
#define RT1711H_REG_S_VBUS_CAL				5
#define RT1711H_REG_MIN_VBUS_CAL			-4

/* timeout = (tout*2+1) * 6.4ms */

#ifdef CONFIG_USB_PD_REV30
#define RT1711H_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) \
	| (tout & 0x07) | RT1711H_REG_ENEXTMSG)
#else
#define RT1711H_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))
#endif

/*
 * RT1711H_REG_INTRST_CTRL			(0x9C)
 */

#define RT1711H_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define RT1711H_REG_INTRST_SET(en, tout) \
	((en << 7) | (tout & 0x03))

/*
 * RT1711H_REG_WATCHDOG_CTRL		(0x9D)
 */

#define RT1711H_REG_WATCHDOG_EN				(1<<7)

/* timeout = (tout+1) * 0.4sec */
#define RT1711H_REG_WATCHDOG_CTRL_SET(en, tout)	\
	((en << 7) | (tout & 0x07))

/*
 * RT1711H_REG_I2CRST_CTRL		(0x9E)
 */

#define RT1711H_REG_I2CRST_EN				(1<<7)

/* timeout = (tout+1) * 12.5ms */
#define RT1711H_REG_I2CRST_SET(en, tout)	\
	((en << 7) | (tout & 0x0f))

#if ENABLE_RT1711_DBG
#define RT1711_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define RT1711_INFO(foramt, args...)
#endif

enum husb311_version {
	HUSB311_B,
	HUSB311_C,
};
#endif /* #ifndef __LINUX_RT1711H_H */

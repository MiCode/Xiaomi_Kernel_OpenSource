/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __LINUX_MT6370_H
#define __LINUX_MT6370_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_MT6370_DBG	0

/* MT6370 Private RegMap */

#define MT6370_REG_PHY_CTRL1				(0x80)
#define MT6370_REG_PHY_CTRL2				(0x81)
#define MT6370_REG_PHY_CTRL3				(0x82)
#define MT6370_REG_CLK_CTRL2				(0x87)
#define MT6370_REG_CLK_CTRL3				(0x88)

#define MT6370_REG_PRL_FSM_RESET			(0x8D)

#define MT6370_REG_BMC_CTRL				(0x90)
#define MT6370_REG_BMCIO_RXDZSEL			(0x93)
#define MT6370_REG_VCONN_CLIMITEN			(0x95)

#define MT6370_REG_MT_STATUS				(0x97)
#define MT6370_REG_MT_INT					(0x98)
#define MT6370_REG_MT_MASK					(0x99)

#define MT6370_REG_BMCIO_RXDZEN			(0x9A)
#define MT6370_REG_IDLE_CTRL				(0x9B)
#define MT6370_REG_INTRST_CTRL				(0x9C)
#define MT6370_REG_WATCHDOG_CTRL			(0x9D)
#define MT6370_REG_I2CRST_CTRL				(0X9E)

#define MT6370_REG_SWRESET				(0xA0)
#define MT6370_REG_TTCPC_FILTER			(0xA1)
#define MT6370_REG_DRP_TOGGLE_CYCLE		(0xA2)
#define MT6370_REG_DRP_DUTY_CTRL			(0xA3)

#define MT6370_REG_PHY_CTRL11				(0xBA)
#define MT6370_REG_PHY_CTRL12				(0xBB)

/*
 * Device ID
 */

#define MT6370_DID_A		0x2170
#define MT6370_DID_B		0x2171
#define MT6370_DID_C		0x2172

#define MT6370_DID_D            0x2173

/*
 * MT6370_REG_PHY_CTRL1            (0x80)
 */

#define MT6370_REG_PHY_CTRL1_SET( \
	retry_discard, toggle_cnt, bus_idle_cnt, rx_filter) \
	((retry_discard << 7) | (toggle_cnt << 4) | \
	(bus_idle_cnt << 2) | (rx_filter & 0x03))

/*
 * MT6370_REG_CLK_CTRL2			(0x87)
 */

#define MT6370_REG_CLK_DIV_600K_EN		(1<<7)
#define MT6370_REG_CLK_BCLK2_EN		(1<<6)
#define MT6370_REG_CLK_BCLK2_TG_EN		(1<<5)
#define MT6370_REG_CLK_DIV_300K_EN		(1<<3)
#define MT6370_REG_CLK_CK_300K_EN		(1<<2)
#define MT6370_REG_CLK_BCLK_EN			(1<<1)
#define MT6370_REG_CLK_BCLK_TH_EN		(1<<0)

/*
 * MT6370_REG_CLK_CTRL3			(0x88)
 */

#define MT6370_REG_CLK_OSCMUX_RG_EN	(1<<7)
#define MT6370_REG_CLK_CK_24M_EN		(1<<6)
#define MT6370_REG_CLK_OSC_RG_EN		(1<<5)
#define MT6370_REG_CLK_DIV_2P4M_EN		(1<<4)
#define MT6370_REG_CLK_CK_2P4M_EN		(1<<3)
#define MT6370_REG_CLK_PCLK_EN			(1<<2)
#define MT6370_REG_CLK_PCLK_RG_EN		(1<<1)
#define MT6370_REG_CLK_PCLK_TG_EN		(1<<0)

/*
 * MT6370_REG_BMC_CTRL				(0x90)
 */

#define MT6370_REG_IDLE_EN				(1<<6)
#define MT6370_REG_DISCHARGE_EN			(1<<5)
#define MT6370_REG_BMCIO_LPRPRD			(1<<4)
#define MT6370_REG_BMCIO_LPEN				(1<<3)
#define MT6370_REG_BMCIO_BG_EN				(1<<2)
#define MT6370_REG_VBUS_DET_EN				(1<<1)
#define MT6370_REG_BMCIO_OSC_EN			(1<<0)

/*
 * MT6370_REG_MT_STATUS				(0x97)
 */

#define MT6370_REG_RA_DETACH				(1<<5)
#define MT6370_REG_VBUS_80				(1<<1)

/*
 * MT6370_REG_MT_INT				(0x98)
 */

#define MT6370_REG_INT_RA_DETACH			(1<<5)
#define MT6370_REG_INT_WATCHDOG			(1<<2)
#define MT6370_REG_INT_VBUS_80				(1<<1)
#define MT6370_REG_INT_WAKEUP				(1<<0)

/*
 * MT6370_REG_MT_MASK				(0x99)
 */

#define MT6370_REG_M_RA_DETACH				(1<<5)
#define MT6370_REG_M_WATCHDOG				(1<<2)
#define MT6370_REG_M_VBUS_80				(1<<1)
#define MT6370_REG_M_WAKEUP				(1<<0)

/*
 * MT6370_REG_IDLE_CTRL				(0x9B)
 */

#define MT6370_REG_CK_300K_SEL				(1<<7)
#define MT6370_REG_SHIPPING_OFF			(1<<5)
#define MT6370_REG_ENEXTMSG			(1<<4)
#define MT6370_REG_AUTOIDLE_EN				(1<<3)

/* timeout = (tout*2+1) * 6.4ms */
#ifdef CONFIG_USB_PD_REV30
#define MT6370_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) |\
	 (auto_idle << 3) | (tout & 0x07) | MT6370_REG_ENEXTMSG)
#else
#define MT6370_REG_IDLE_SET(ck300, ship_dis, auto_idle, tout) \
	((ck300 << 7) | (ship_dis << 5) | (auto_idle << 3) | (tout & 0x07))
#endif

/*
 * MT6370_REG_INTRST_CTRL			(0x9C)
 */

#define MT6370_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define MT6370_REG_INTRST_SET(en, tout) \
	((en << 7) | (tout & 0x03))

/*
 * MT6370_REG_WATCHDOG_CTRL		(0x9D)
 */

#define MT6370_REG_WATCHDOG_EN				(1<<7)

/* timeout = (tout+1) * 0.4sec */
#define MT6370_REG_WATCHDOG_CTRL_SET(en, tout)	\
	((en << 7) | (tout & 0x07))

/*
 * MT6370_REG_I2CRST_CTRL		(0x9E)
 */

#define MT6370_REG_I2CRST_EN				(1<<7)

/* timeout = (tout+1) * 12.5ms */
#define MT6370_REG_I2CRST_SET(en, tout)	\
	((en << 7) | (tout & 0x0f))

#if ENABLE_MT6370_DBG
#define MT6370_INFO(format, args...) \
	pd_dbg_info("%s() line-%d: " format,\
	__func__, __LINE__, ##args)
#else
#define MT6370_INFO(foramt, args...)
#endif

#endif /* #ifndef __LINUX_MT6370_H */

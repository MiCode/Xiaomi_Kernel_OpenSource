/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Author: TH <tsunghan_tasi@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_RT1711H_H
#define __LINUX_RT1711H_H

#include "std_tcpci_v10.h"
#include "pd_dbg_info.h"

/*show debug message or not */
#define ENABLE_RT1711_DBG	0

/* RT1711H Private RegMap */

#define RT1711H_REG_PHY_CTRL1				(0x80)
#define RT1711H_REG_PHY_CTRL2				(0x81)
#define RT1711H_REG_PHY_CTRL3				(0x82)
#define RT1711H_REG_PHY_CTRL6				(0x85)

#define RT1711H_REG_CLK_CTRL2				(0x87)
#define RT1711H_REG_CLK_CTRL3				(0x88)

#define RT1711H_REG_RX_TX_DBG				(0x8b)
#define RT1711H_REG_BMC_CTRL				(0x90)
#define RT1711H_REG_BMCIO_RXDZSEL			(0x93)

#define RT1711H_REG_RT_STATUS				(0x97)
#define RT1711H_REG_RT_INT					(0x98)
#define RT1711H_REG_RT_MASK					(0x99)

#define RT1711H_REG_IDLE_CTRL				(0x9B)
#define RT1711H_REG_INTRST_CTRL				(0x9C)
#define RT1711H_REG_WATCHDOG_CTRL			(0x9D)
#define RT1711H_REG_I2CRST_CTRL				(0x9E)

#define RT1711H_REG_SWRESET				(0xA0)
#define RT1711H_REG_TTCPC_FILTER			(0xA1)
#define RT1711H_REG_DRP_TOGGLE_CYCLE		(0xA2)
#define RT1711H_REG_DRP_DUTY_CTRL			(0xA3)

#define RT1711H_REG_UNLOCK_PW2				(0xF0)
#define RT1711H_REG_UNLOCK_PW1				(0xF1)

/*
 * Device ID
 */

#define RT1711H_DID_A		0x2170
#define RT1711H_DID_B		0x2171
#define RT1711H_DID_C		0x2172

/*
 * RT1711H_REG_RX_TX_DBG			(0x8b)
 */

#define RT1711H_REG_RX_TX_DBG_RX_BUSY		(1<<7)
#define RT1711H_REG_RX_TX_DBG_TX_BUSY		(1<<6)

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
 * RT1711H_REG_INTRST_CTRL	(0x9C)
 */

#define RT1711H_REG_INTRST_EN				(1<<7)

/* timeout = (tout+1) * 0.2sec */
#define RT1711H_REG_INTRST_SET(en, tout)	((en << 7)|(tout&0x03))

/*
 * RT1711H_REG_WATCHDOG_CTRL	(0x9D)
 */

#define RT1711H_REG_WATCHDOG_EN				(1<<7)

/* timeout = (tout+1) * 0.4sec */
#define RT1711H_REG_WATCHDOG_CTRL_SET(en, tout)	((en << 7)|(tout&0x07))


#if ENABLE_RT1711_DBG
#define RT1711H_INFO(format, args...)	\
	pd_dbg_info("%s() line-%d: "format, __func__, __LINE__, ##args)
#else
#define RT1711_INFO(foramt, args...)
#endif

#endif /* #ifndef __LINUX_RT1711H_H */

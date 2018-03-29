/*
 * ICUSB - for MUSB Host Driver defines
 *
 * Copyright 2015 Mediatek Inc.
 *	Marvin Lin <marvin.lin@mediatek.com>
 *	Arvin Wang <arvin.wang@mediatek.com>
 *	Vincent Fan <vincent.fan@mediatek.com>
 *	Bryant Lu <bryant.lu@mediatek.com>
 *	Yu-Chang Wang <yu-chang.wang@mediatek.com>
 *	Macpaul Lin <macpaul.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MUSBFSH_ICUSB_H
#define _MUSBFSH_ICUSB_H

enum PHY_VOLTAGE_TYPE {
	VOL_18 = 0,
	VOL_33,
	VOL_50,
};

enum SESSION_CONTROL_ACTION {
	STOP_SESSION = 0,
	START_SESSION,
};

enum WAIT_DISCONNECT_DONE_ACTION {
	WAIT_DISCONNECT_DONE_DFT_ACTION = 0,
};

#define IC_USB_CMD_LEN 255
struct IC_USB_CMD {
	unsigned char type;
	unsigned char length;
	unsigned char data[IC_USB_CMD_LEN];
};

enum IC_USB_CMD_TYPE {
	USB11_SESSION_CONTROL = 0,
	USB11_INIT_PHY_BY_VOLTAGE,
	USB11_WAIT_DISCONNECT_DONE,
};

/* ICUSB feature list */
/* --- sysfs controlable feature --- */
#define MTK_ICUSB_POWER_AND_RESUME_TIME_NEOGO_SUPPORT
#define MTK_ICUSB_SKIP_SESSION_REQ
#define MTK_ICUSB_SKIP_ENABLE_SESSION
#define MTK_ICUSB_SKIP_MAC_INIT
#define MTK_ICUSB_RESISTOR_CONTROL
#define MTK_ICUSB_HW_DBG
/* #define MTK_ICUSB_SKIP_PORT_PM */

/* --- non sysfs controlable feature --- */
/* #define MTK_ICUSB_TAKE_WAKE_LOCK */
/* #define MTK_ICUSB_BABBLE_RECOVER */

struct my_attr {
	struct attribute attr;
	int value;
};

/* power neogo */
#define IC_USB_REQ_TYPE_GET_IFACE_POWER 0xC0	/* Get interface power */
#define IC_USB_REQ_TYPE_SET_IFACE_POWER 0x40	/* Set interface power */
#define IC_USB_REQ_GET_IFACE_POWER 0x01		/* Get interface power */
#define IC_USB_REQ_SET_IFACE_POWER 0x02		/* Set interface power */
#define IC_USB_WVALUE_POWER_NEGOTIATION 0
#define IC_USB_WINDEX_POWER_NEGOTIATION 0
#define IC_USB_LEN_POWER_NEGOTIATION 2
#define IC_USB_PREFER_CLASSB_ENABLE_BIT 0x80
#define IC_USB_RETRIES_POWER_NEGOTIATION 3
#define IC_USB_CLASSB (1<<1)
#define IC_USB_CLASSC (1<<2)
#define IC_USB_CURRENT 100	/* in 2 mA unit, 100 denotes 200 mA */

/* resume_time neogo */
#define IC_USB_REQ_TYPE_GET_INTERFACE_RESUME_TIME  0xC0
#define IC_USB_REQ_GET_INTERFACE_RESUME_TIME 0x03
#define IC_USB_WVALUE_RESUME_TIME_NEGOTIATION 0
#define IC_USB_WINDEX_RESUME_TIME_NEGOTIATION 0
#define IC_USB_LEN_RESUME_TIME_NEGOTIATION 3
#define IC_USB_RETRIES_RESUME_TIME_NEGOTIATION 3

/* == =================== */
/* ic_usb_status : */
/* Byte4 : wait disconnect status */
/* Byte3 Byte2 : get interface power reqest data field */
/* Byte1 : power negotiation result */
/*  */
/* ===================== */

#define PREFER_VOL_STS_SHIFT (0)
#define PREFER_VOL_STS_MSK (0x3)

#define PREFER_VOL_NOT_INITED  0x0
#define PREFER_VOL_PWR_NEG_FAIL 0x1
#define PREFER_VOL_PWR_NEG_OK 0x2

#define PREFER_VOL_CLASS_SHIFT (8)
#define PREFER_VOL_CLASS_MSK (0xff)

#define USB_PORT1_STS_SHIFT (24)
#define USB_PORT1_STS_MSK (0xf)

#define USB_PORT1_DISCONNECTING 0x0
#define USB_PORT1_DISCONNECT_DONE 0x1
#define USB_PORT1_CONNECT 0x2

extern struct my_attr power_resume_time_neogo_attr;
extern struct my_attr skip_session_req_attr;
extern struct my_attr skip_enable_session_attr;
extern struct my_attr skip_mac_init_attr;
extern struct my_attr resistor_control_attr;
extern struct my_attr hw_dbg_attr;
extern struct my_attr skip_port_pm_attr;

extern void musbfsh_start_session(void);
extern void musbfsh_start_session_pure(void);
extern void musbfsh_stop_session(void);
extern void musbfsh_init_phy_by_voltage(enum PHY_VOLTAGE_TYPE);
extern enum PHY_VOLTAGE_TYPE get_usb11_phy_voltage(void);
extern void mt65xx_usb11_mac_reset_and_phy_stress_set(void);
extern int is_usb11_enabled(void);

#define MYDBG(fmt, args...) pr_warn("MTK_ICUSB [DBG], <%s(), %d> " fmt, \
				__func__, __LINE__, ## args)
#endif

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

/*
 * This is used to for host and peripheral modes of the driver for
 * super speed Dual-Role Controllers (MediaTek).
 */

#ifndef _LINUX_USB_SSUSB_H_
#define _LINUX_USB_SSUSB_H_

/*
* copy from musb/musb.h temp. modify later
*/

/* The USB role is defined by the connector used on the board, so long as
 * standards are being followed.  (Developer boards sometimes won't.)
 */
enum musb_mode {
	MUSB_UNDEFINED = 0,
	MUSB_HOST,		/* A or Mini-A connector */
	MUSB_PERIPHERAL,	/* B or Mini-B connector */
	MUSB_OTG		/* Mini-AB connector */
};

/* struct clk; */

enum musb_fifo_style {
	FIFO_RXTX,		/* add MUSB_ prefix to avoid confilicts with musbfsh.h, gang */
	FIFO_TX,
	FIFO_RX
};


enum musb_ep_mode {
	EP_CONT,
	EP_INT,
	EP_BULK,
	EP_ISO
};

struct musb_fifo_cfg {
	u8 hw_ep_num;
	enum musb_fifo_style style;
	u16 maxpacket;
	enum musb_ep_mode ep_mode;
};

#define MUSB_EP_FIFO(ep, st, m, pkt)		\
{						\
	.hw_ep_num	= ep,			\
	.style		= st,			\
	.mode		= m,			\
	.maxpacket	= pkt,			\
}


struct musb_hdrc_config {
	struct musb_fifo_cfg *fifo_cfg;	/* board fifo configuration */
	u32 fifo_cfg_size;	/* size of the fifo configuration */
	u32 num_eps;		/* number of endpoints _with_ ep0 */
	u32 dyn_fifo_size;	/* dynamic size in bytes */
	u32 ram_bits;		/* ram address size */

};

struct musb_hdrc_platform_data {
	int port_num;
	/* MUSB_HOST, MUSB_PERIPHERAL, or MUSB_OTG */
	int drv_mode;
	int otg_mode;
	int str_mode;
	int otg_init_as_host;  /* init as device or host? */
	int is_u3_otg;    /* is usb3 or usb2? */
	int eint_num;
	int p0_vbus_mode;
	int p0_gpio_num;
	int p0_gpio_active_low;
	int p1_vbus_mode;
	int p1_gpio_num;
	int p1_gpio_active_low;
	int wakeup_src;
	/* MUSB configuration-specific details */
	struct musb_hdrc_config *config;

	/* Architecture specific board data     */
	void *board_data;

	/* Platform specific struct musb_ops pointer */
	const void *platform_ops;
};


struct ssusb_xhci_pdata {
	int	need_str;
};

extern void mtk_xhci_set(void *xhci);

#endif				/* __LINUX_USB_MUSB_H */

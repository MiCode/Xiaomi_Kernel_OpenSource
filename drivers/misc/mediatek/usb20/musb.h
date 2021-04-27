/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __LINUX_USB_MUSB_H
#define __LINUX_USB_MUSB_H

#include <linux/usb/role.h>

/* The USB role is defined by the connector used on the board, so long as
 * standards are being followed.  (Developer boards sometimes won't.)
 */
enum musb_mode {
	MUSB_UNDEFINED = 0,
	MUSB_HOST,		/* A or Mini-A connector */
	MUSB_PERIPHERAL,	/* B or Mini-B connector */
	MUSB_OTG		/* Mini-AB connector */
};

enum musb_fifo_style {
	FIFO_RXTX,
	FIFO_TX,
	FIFO_RX
} __attribute__ ((packed));

enum musb_buf_mode {
	BUF_SINGLE,
	BUF_DOUBLE
} __attribute__ ((packed));

enum musb_ep_mode {
	EP_CONT,
	EP_INT,
	EP_BULK,
	EP_ISO
} __packed;

struct musb_fifo_cfg {
	u8 hw_ep_num;
	enum musb_fifo_style style;
	enum musb_buf_mode mode;
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

#define MUSB_EP_FIFO_SINGLE(ep, st, pkt)	\
	MUSB_EP_FIFO(ep, st, BUF_SINGLE, pkt)

#define MUSB_EP_FIFO_DOUBLE(ep, st, pkt)	\
	MUSB_EP_FIFO(ep, st, BUF_DOUBLE, pkt)

struct musb_hdrc_eps_bits {
	const char name[16];
	u8 bits;
};

struct musb_hdrc_config {
	struct musb_fifo_cfg *fifo_cfg;	/* board fifo configuration */
	unsigned int fifo_cfg_size;/* size of the fifo configuration */

	/* MUSB configuration-specific details */
	unsigned multipoint:1;	/* multipoint device */
	unsigned dyn_fifo:1 __deprecated;/* supports dynamic fifo sizing */
	unsigned soft_con:1 __deprecated;/* soft connect required */
	unsigned utm_16:1 __deprecated;	/* utm data witdh is 16 bits */
	unsigned big_endian:1;	/* true if CPU uses big-endian */
	unsigned mult_bulk_tx:1;	/* Tx ep required for multbulk pkts */
	unsigned mult_bulk_rx:1;	/* Rx ep required for multbulk pkts */
	unsigned high_iso_tx:1;	/* Tx ep required for HB iso */
	unsigned high_iso_rx:1;	/* Rx ep required for HD iso */
	unsigned dma:1 __deprecated;	/* supports DMA */
	unsigned vendor_req:1 __deprecated;	/* vendor registers required */

	u8 num_eps;		/* number of endpoints _with_ ep0 */

	u8 dma_channels __deprecated;	/* number of dma channels */
	u8 dyn_fifo_size;	/* dynamic size in bytes */

	u8 vendor_ctrl __deprecated;	/* vendor control reg width */
	u8 vendor_stat __deprecated;	/* vendor status reg witdh */
	u8 dma_req_chan __deprecated;	/* bitmask for required dma channels */
	u8 ram_bits;		/* ram address size */

	struct musb_hdrc_eps_bits *eps_bits __deprecated;
};

struct musb_hdrc_platform_data {
	/* MUSB_HOST, MUSB_PERIPHERAL, or MUSB_OTG */
	u8 mode;

	/* for clk_get() */
	const char *clock;

	/* (HOST or OTG) switch VBUS on/off */
	int (*set_vbus)(struct device *dev, int is_on);

	/* (HOST or OTG) mA/2 power supplied on (default = 8mA) */
	u8 power;

	/* (PERIPHERAL) mA/2 max power consumed (default = 100mA) */
	u8 min_power;

	/* (HOST or OTG) msec/2 after VBUS on till power good */
	u8 potpgt;

	/* (HOST or OTG) program PHY for external Vbus */
	unsigned extvbus:1;

	/* Power the device on or off */
	int (*set_power)(int state);

	/* MUSB configuration-specific details */
	struct musb_hdrc_config *config;

	/* Architecture specific board data     */
	void *board_data;

	/* Platform specific struct musb_ops pointer */
	const void *platform_ops;
};

/**
 * MUSB_DR_FORCE_NONE: automatically switch host and periperal mode
 *		by IDPIN signal.
 * MUSB_DR_FORCE_HOST: force to enter host mode and override OTG
 *		IDPIN signal.
 * MUSB_DR_FORCE_DEVICE: force to enter peripheral mode.
 */
enum mt_usb_dr_force_mode {
	MUSB_DR_FORCE_NONE = 0,
	MUSB_DR_FORCE_HOST,
	MUSB_DR_FORCE_DEVICE,
};

/**
 * MUSB_DR_OPERATION_NONE: force to tun off usb
 * MUSB_DR_OPERATION_NORMAL: automatically switch host and
 *   periperal mode by usb role switch.
 * MUSB_DR_OPERATION_HOST: force to enter host mode.
 * MUSB_DR_OPERATION_DEVICE: force to enter peripheral mode.
 */
enum mt_usb_dr_operation_mode {
	MUSB_DR_OPERATION_NONE = 0,
	MUSB_DR_OPERATION_NORMAL,
	MUSB_DR_OPERATION_HOST,
	MUSB_DR_OPERATION_DEVICE,
};

/**
 * @vbus: vbus 5V used by host mode
 * @edev: external connector used to detect vbus and iddig changes
 * @vbus_nb: notifier for vbus detection
 * @vbus_work : work of vbus detection notifier, used to avoid sleep in
 *		notifier callback which is atomic context
 * @vbus_event : event of vbus detecion notifier
 * @id_nb : notifier for iddig(idpin) detection
 * @id_work : work of iddig detection notifier
 * @id_event : event of iddig detecion notifier
 * @role_sw : use USB Role Switch to support dual-role switch, can't use
 *		extcon at the same time, and extcon is deprecated.
 * @role_sw_used : true when the USB Role Switch is used.
 * @manual_drd_enabled: it's true when supports dual-role device by debugfs
 *		to switch host/device modes depending on user input.
 */
struct otg_switch_mtk {
	struct regulator *vbus;
	struct extcon_dev *edev;
	struct notifier_block vbus_nb;
	struct work_struct vbus_work;
	unsigned long vbus_event;
	struct notifier_block id_nb;
	struct work_struct id_work;
	unsigned long id_event;
	struct usb_role_switch *role_sw;
	bool role_sw_used;
	bool manual_drd_enabled;
	u32 sw_state;
	enum usb_role latest_role;
	enum mt_usb_dr_operation_mode op_mode;
};
#endif				/* __LINUX_USB_MUSB_H */

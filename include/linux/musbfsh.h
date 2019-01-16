/*
 * This is used to for host and peripheral modes of the driver for
 * Inventra (Multidrop) Highspeed Dual-Role Controllers:  (M)HDRC.
 *
 * Board initialization should put one of these into dev->platform_data,
 * probably on some platform_device named "musbfsh_hdrc".  It encapsulates
 * key configuration differences between boards.
 */

#ifndef __LINUX_USB_MUSBFSH_H
#define __LINUX_USB_MUSBFSH_H

/* The USB role is defined by the connector used on the board, so long as
 * standards are being followed.  (Developer boards sometimes won't.)
 */
enum musbfsh_mode {
	MUSBFSH_UNDEFINED = 0,
	MUSBFSH_HOST,		/* A or Mini-A connector */
	MUSBFSH_PERIPHERAL,	/* B or Mini-B connector */
	MUSBFSH_OTG		/* Mini-AB connector */
};

struct clk;
enum musbfsh_fifo_style {
	FIFO_RXTX,
	FIFO_TX,
	FIFO_RX
} __attribute__ ((packed));

enum musbfsh_buf_mode {
	BUF_SINGLE,
	BUF_DOUBLE
} __attribute__ ((packed));

struct musbfsh_fifo_cfg {
	u8 hw_ep_num;
	enum musbfsh_fifo_style style;
	enum musbfsh_buf_mode mode;
	u16 maxpacket;
};

#define MUSBFSH_EP_FIFO(ep, st, m, pkt)		\
{						\
	.hw_ep_num	= ep,			\
	.style		= st,			\
	.mode		= m,			\
	.maxpacket	= pkt,			\
}

#define MUSBFSH_EP_FIFO_SINGLE(ep, st, pkt)	\
	MUSBFSH_EP_FIFO(ep, st, BUF_SINGLE, pkt)

#define MUSBFSH_EP_FIFO_DOUBLE(ep, st, pkt)	\
	MUSBFSH_EP_FIFO(ep, st, BUF_DOUBLE, pkt)

struct musbfsh_hdrc_eps_bits {
	const char name[16];
	u8 bits;
};

struct musbfsh_hdrc_config {
	struct musbfsh_fifo_cfg *fifo_cfg;	/* board fifo configuration */
	unsigned fifo_cfg_size;	/* size of the fifo configuration */

	/* MUSB configuration-specific details */
	unsigned multipoint:1;	/* multipoint device */
	unsigned dyn_fifo:1 __deprecated;	/* supports dynamic fifo sizing */
	unsigned soft_con:1 __deprecated;	/* soft connect required */
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

	struct musbfsh_hdrc_eps_bits *eps_bits __deprecated;

};

struct musbfsh_hdrc_platform_data {
	/* MUSBFSH_HOST, MUSBFSH_PERIPHERAL, or MUSBFSH_OTG */
	u8 mode;

	const char *clock;
	/* (HOST or OTG) switch VBUS on/off */
	int (*set_vbus) (struct device *dev, int is_on);

	/* (HOST or OTG) mA/2 power supplied on (default = 8mA) */
	u8 power;

	u8 min_power;
	u8 potpgt;
	/* (HOST or OTG) program PHY for external Vbus */
	unsigned extvbus:1;

	/* Power the device on or off */
	int (*set_power) (int state);

	/* MUSB configuration-specific details */
	struct musbfsh_hdrc_config *config;

	void *board_data;
	const void *platform_ops;
};

#endif				/* __LINUX_USB_MUSBFSH_H */

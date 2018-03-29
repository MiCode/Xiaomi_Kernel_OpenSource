/*
 * MUSB OTG driver defines
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
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
 *
 */

#ifndef __MUSB_CORE_H__
#define __MUSB_CORE_H__

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb.h>
#include <linux/usb/otg.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>

struct musb;
struct musb_hw_ep;
struct musb_ep;

#include "ssusb_sysfs.h"
/* #include "musb_io.h" */
#include "musb_gadget.h"
#include "ssusb.h"


/* #define CONFIG_MUSB_PIO_ONLY */
#ifdef SUPPORT_U3
#define USB_GADGET_SUPERSPEED
#else
#define USB_GADGET_DUALSPEED
#endif

#define MUSB_DRIVER_NAME "musb-hdrc"

#define	is_peripheral_enabled(musb)	((musb)->board_mode != MUSB_HOST)
#define	is_host_enabled(musb)		((musb)->board_mode != MUSB_PERIPHERAL)
#define	is_otg_enabled(musb)		((musb)->board_mode == MUSB_OTG)

#define	DMA_ADDR_INVALID	(~(dma_addr_t)0)

#ifdef USE_SSUSB_QMU
#define	is_dma_capable()	(1)
#else
#define	is_dma_capable()	(0)
#endif

#define SSUSB_XHCI_RSCS_NUM	2
#define SSUSB_MU3D_RSCS_NUM	4

#define K_QMU	(1<<7)
#define K_ALET		(1<<6)
#define K_CRIT		(1<<5)
#define K_ERR		(1<<4)
#define K_WARNIN	(1<<3)
#define K_NOTICE	(1<<2)
#define K_INFO		(1<<1)
#define K_DEBUG	(1<<0)

/*Set the debug level at musb_core.c*/
extern u32 debug_level;
extern struct musb *_mu3d_musb;
extern struct musb_fifo_cfg ep0_cfg_u3;
extern struct musb_fifo_cfg ep0_cfg_u2;

#ifdef USE_SSUSB_QMU
#define qmu_dbg(level, fmt, args...) do { \
		if (debug_level & (level|K_QMU)) { \
			pr_err("[MU3D][Q]" fmt, ## args); \
		} \
	} while (0)
#endif

#define mu3d_dbg(level, fmt, args...) do { \
		if (debug_level & level) { \
			pr_err("[MU3D]" fmt, ## args); \
		} \
	} while (0)

/* NOTE:  otg and peripheral-only state machines start at B_IDLE.
 * OTG or host-only go to A_IDLE when ID is sensed.
 */
/* #define is_peripheral_active(m)               (!(m)->is_host) */
/* #define is_host_active(m)             ((m)->is_host) */

#if 0				/* ndef CONFIG_HAVE_CLK */
/* Dummy stub for clk framework */
#define clk_get(dev, id)	NULL
#define clk_put(clock)		do {} while (0)
#define clk_enable(clock)	do {} while (0)
#define clk_disable(clock)	do {} while (0)
#endif

#ifdef CONFIG_PROC_FS
#include <linux/fs.h>
#define MUSB_CONFIG_PROC_FS
#endif

/* xtal clock related */
/* #define UAP_PLL_CONX_BASE	0x10209000 */
/* #define UAP_PLL_CONX_LEN	0x1000 */
#define UAP_PLL_CON0		0x00
#define CON0_RG_LTECLKSQ_EN	(0x1 << 0)
#define CON0_RG_LTECLKSQ_LPF_EN	(0x1 << 1)

#define UAP_PLL_CON1		0x04
#define UAP_PLL_CON2		0x08
#define CON2_DA_REF2USB_TX_EN	(0x1 << 0)
#define CON2_DA_REF2USB_TX_LPF_EN	(0x1 << 1)
#define CON2_DA_REF2USB_TX_OUT_EN	(0x1 << 2)
#define CON2_DA_REF2USB_TX_MASK \
	(CON2_DA_REF2USB_TX_EN | CON2_DA_REF2USB_TX_LPF_EN | CON2_DA_REF2USB_TX_OUT_EN)

/* usb wakeup control related (pericfg doc) */
/* #define PERIFSYS_BASE 0x10003000 */
/* #define PERIFSYS_LEN 0x1000 */

#define PERI_WK_CTRL0 0x400
#define UWK_CTL1_1P_LS_E	(0x1 << 0)
#define UWK_CTL1_1P_LS_C(x) (((x) & 0xf) << 1)
#define UWK_CTR0_0P_LS_NE	(0x1 << 7)	/* negedge for 0p linestate */
#define UWK_CTR0_0P_LS_PE	(0x1 << 8)	/* posedge */

#define PERI_WK_CTRL1 0x404
#define UWK_CTL1_IS_P		(0x1 << 6)	/* polarity for ip sleep */
#define UWK_CTL1_0P_LS_P	(0x1 << 7)
#define UWK_CTL1_IDDIG_P	(0x1 << 9)	/* polarity */
#define UWK_CTL1_IDDIG_E	(0x1 << 10)	/* enable debounce */
#define UWK_CTL1_IDDIG_C(x) (((x) & 0xf) << 11)	/* cycle of debounce */
#define UWK_CTL1_0P_LS_E	(0x1 << 20)
#define UWK_CTL1_0P_LS_C(x) (((x) & 0xf) << 21)
#define UWK_CTL1_IS_E	(0x1 << 25)
#define UWK_CTL1_IS_C(x) (((x) & 0xf) << 26)



/****************************** PERIPHERAL ROLE *****************************/

/* #define       is_peripheral_capable() (1) */

extern irqreturn_t musb_g_ep0_irq(struct musb *);
extern void musb_g_tx(struct musb *, u8);
extern void musb_g_rx(struct musb *, u8);
extern void musb_g_reset(struct musb *);
extern void musb_g_suspend(struct musb *);
extern void musb_g_resume(struct musb *);
extern void musb_g_wakeup(struct musb *);
extern void musb_g_disconnect(struct musb *);

/****************************** HOST ROLE ***********************************/

/* #define       is_host_capable()       (1) */

/* extern irqreturn_t musb_h_ep0_irq(struct musb *); */
/* extern void musb_host_tx(struct musb *, u8); */
/* extern void musb_host_rx(struct musb *, u8); */

/****************************** CONSTANTS ********************************/
#define MUSB_EP0_FIFOSIZE	64	/* This is non-configurable */

#ifndef MUSB_C_NUM_EPS
#define MUSB_C_NUM_EPS ((u8)9)
#endif

#ifndef MUSB_MAX_END0_PACKET
#define MUSB_MAX_END0_PACKET ((u16)MUSB_EP0_FIFOSIZE)
#endif

/* host side ep0 states */
enum musb_h_ep0_state {
	MUSB_EP0_IDLE,
	MUSB_EP0_START,		/* expect ack of setup */
	MUSB_EP0_IN,		/* expect IN DATA */
	MUSB_EP0_OUT,		/* expect ack of OUT DATA */
	MUSB_EP0_STATUS,	/* expect ack of STATUS */
};

/* peripheral side ep0 states */
enum musb_g_ep0_state {
	MUSB_EP0_STAGE_IDLE,	/* idle, waiting for SETUP */
	MUSB_EP0_STAGE_SETUP,	/* received SETUP */
	MUSB_EP0_STAGE_TX,	/* IN data */
	MUSB_EP0_STAGE_RX,	/* OUT data */
	MUSB_EP0_STAGE_STATUSIN,	/* (after OUT data) */
	MUSB_EP0_STAGE_STATUSOUT,	/* (after IN data) */
	MUSB_EP0_STAGE_ACKWAIT,	/* after zlp, before statusin */
};

enum ssusb_vbus_mode {
	SSUSB_VBUS_DEF_ON,	/* turn on already by default */
	SSUSB_VBUS_CHARGER,	/* turn on/off by charger driver api */
	SSUSB_VBUS_GPIO,	/* turn on/off by GPIO */
};

/*
* @ SSUSB_OTG_STANDARD: by SRP and HNP, don't realize it temp.;
* @ SSUSB_OTG_CHEINT: device mode is turned on/off by charge driver & idpin eint
*		is used to switch between device/host mode. (on tablet)
* @ SSUSB_OTG_MANUAL: device and host is switch by sysfs interface (on box with type A)
* @ SSUSB_OTG_IDPIN: device and host mode is switched by idpin eint, but device should turn
*		on by default which is almost the same as SSUSB_OTG_MANUAL; (on box with micro
*		port and inpin, but not charger ic and a gpio to detect vbus)
*/
enum ssusb_otg_mode {
	SSUSB_OTG_STANDARD,	/* by SRP or HNP */
	SSUSB_OTG_CHEINT,	/* by charger & EINT */
	SSUSB_OTG_MANUAL,	/* by sysfs interface */
	SSUSB_OTG_IDPIN,	/* by iddig instead of sysfs interface */
};

/* same as musb_mode temp */
enum ssusb_mode {
	SSUSB_MODE_UNDEFINED = 0,
	SSUSB_MODE_HOST,
	SSUSB_MODE_DEVICE,
	SSUSB_MODE_DRD,
};

/*
* @SSUSB_STR_NONE: especially used by tablet: clocks, ports, mu3d and xhci
*		are all power down and phy also enter low-power mode when no cable is plugged in.
*		but keep LDOs on to let charger detect cable type. when plug in cable or OTG
*		device or host is enabled again, at the same time system never enter suspend
*		mode, so it doesn't need to support suspend/resume  at the case.
* @SSUSB_STR_ALIVE: mainly used by box: when system suspend, usb should keep
*		on link with devices such as wifi, bt and ethernet which  layed out on board;
*		that means only close clocks and force phy enter low-power mode
*		when resume, usb should not enum device again but make use of resume-signal
*		to wakeup device.
* @SSUSB_STR_DEEP: maily used by box: when systen suspend, close all LDO, clocks,
*		mu3d, xhci, and force phy enter low-power mode; when resume, mu3d or xhci
*		is reset and enabled again.
*/
enum ssusb_str_mode {
	SSUSB_STR_NONE = 0,
	SSUSB_STR_ALIVE,
	SSUSB_STR_DEEP,
};

/*
* bit definition for usb wakeup source:
* @SSUSB_WK_IDDIG: wakeup system when plug-in OTG cable; it's only for OTG port
*		with id-pin; it is not affected by host/device mode which port is working on.
* @SSUSB_WK_LINESTATE_0P: wakeup system when plug-in/out device for port0,
*		it's not necessary to enable it for port0 which supports OTG, but instead of
*		SSUSB_WK_IDDIG; and it works only when port0 is host mode.
* @SSUSB_WK_LINESTATE_1P: wakeup system when plug-in/out device for port1 which
*		works only on host mode.
* @SSUSB_WK_IP_SLEEP: wakeup system when device sends remote wakeup request.
*/
enum ssusb_wakeup_src {
	SSUSB_WK_IDDIG = 1,
	SSUSB_WK_LINESTATE_0P = 2,
	SSUSB_WK_LINESTATE_1P = 4,
	SSUSB_WK_IP_SLEEP = 8,
};

/* only for annonce usb status to battery */
enum usb_state_enum {
	USB_SUSPEND = 0,
	USB_UNCONFIGURED,
	USB_CONFIGURED
};


enum cable_mode {
	CABLE_MODE_CHRG_ONLY = 0,
	CABLE_MODE_NORMAL,
	CABLE_MODE_HOST_ONLY,
	CABLE_MODE_MAX
};

/*
 * OTG protocol constants.  See USB OTG 1.3 spec,
 * sections 5.5 "Device Timings" and 6.6.5 "Timers".
 */
#define OTG_TIME_A_WAIT_VRISE	100	/* msec (max) */
#define OTG_TIME_A_WAIT_BCON	1100	/* min 1 second */
#define OTG_TIME_A_AIDL_BDIS	200	/* min 200 msec */
#define OTG_TIME_B_ASE0_BRST	100	/* min 3.125 ms */


/*************************** REGISTER ACCESS ********************************/

/* Endpoint registers (other than dynfifo setup) can be accessed either
 * directly with the "flat" model, or after setting up an index register.
 */
#define MU3D_FIFO_OFFSET(epnum)	(U3D_FIFO0 + ((epnum) * 0x10))


#define	MU3D_EP_TXCR0_OFFSET(_epnum, _offset)	\
	(U3D_TX1CSR0 + ((_epnum - 1)*0x10) + (_offset))

#define	MU3D_EP_TXCR1_OFFSET(_epnum, _offset)	\
	(U3D_TX1CSR1 + ((_epnum - 1)*0x10) + (_offset))

#define	MU3D_EP_TXCR2_OFFSET(_epnum, _offset)	\
	(U3D_TX1CSR2 + ((_epnum - 1)*0x10) + (_offset))

/* #define       SSUSB_EP_TXMAXP_OFFSET(_epnum, _offset) */
/* (U3D_TX1CSR0 + ((_epnum - 1)*0x10) + (_offset)) */

#define	MU3D_EP_RXCR0_OFFSET(_epnum, _offset)	\
	(U3D_RX1CSR0 + ((_epnum - 1)*0x10) + (_offset))

#define	MU3D_EP_RXCR1_OFFSET(_epnum, _offset)	\
	(U3D_RX1CSR1 + ((_epnum - 1)*0x10) + (_offset))

#define	MU3D_EP_RXCR2_OFFSET(_epnum, _offset)	\
	(U3D_RX1CSR2 + ((_epnum - 1)*0x10) + (_offset))

#define	MU3D_EP_RXCR3_OFFSET(_epnum, _offset)	\
	(U3D_RX1CSR3 + ((_epnum - 1)*0x10) + (_offset))



/****************************** FUNCTIONS ********************************/

/* #define MUSB_HST_MODE(_musb)  { (_musb)->is_host = true; } */
/* #define MUSB_DEV_MODE(_musb)  { (_musb)->is_host = false; } */

/* #define test_devctl_hst_mode(_x) */
/* (musb_readb((_x)->mregs, MUSB_DEVCTL)&MUSB_DEVCTL_HM) */

/* #define MUSB_MODE(musb) ((musb)->is_host ? "Host" : "Peripheral") */

/******************************** TYPES *************************************/

/* #define SSUSB_MODE_DEVICE	0 */
/* #define SSUSB_MODE_HOST		1 */
/* #define SSUSB_MODE_DRD		2 */

struct vbus_ctrl_info {
	enum ssusb_vbus_mode vbus_mode;
	int vbus_gpio_num;
	int gpio_active_low;
};

/**
* @is_iddig_registered: whether iddig eint ISR is registered or not.
* @iddig_reg_dwork: delay work for iddig eint register, waiting for charger initialized to
*		provide vbus for p0(it's necessary for bootup system when plug in OTG cable ).
* @switch_dwork: iddig eint delay work, process OTG switch.
* @is_init_as_host: it is only used by SSUSB_OTG_MANUAL; for other otg mode,
*		its default value shold be zero.
* @port0_u2 : only port0 supports OTG, @port0_u2 indicats whether it support OTG or not.
*		0: no, 1: do switch if exist.
* @port0_u3 : is the same as port0_u2.
*/
struct otg_switch_mtk {
	struct switch_dev otg_state;
	struct vbus_ctrl_info p0_vbus;
	int is_iddig_registered;
	struct delayed_work iddig_reg_dwork;
	struct delayed_work switch_dwork;
	struct wake_lock xhci_wakelock;
	enum ssusb_otg_mode otg_mode;
	int is_init_as_host;
	int iddig_eint_num;
	int next_idpin_state;
	int vbus_gpio_num;
	enum ssusb_vbus_mode vbus_mode;
	int port0_u2;
	int port0_u3;
};


/**
 * @ pdata: for mu3d platform
 * @ mac_base: only mu3d device-mac regs, exclude xhci's
 * @ sif_base: include sif & sif2
 * @ scp_sys: it is usb-mtcmos power in fact
 * @ str_mode: if it is SSUSB_STR_NONE, then its otg mode should be SSUSB_OTG_CHENT.
 * @ usbnet33:  only some box use it, maybe it is better to put in usbnet
 * @ start_mu3d:  0: use iddig to start/stop mu3d,
 *		1: need call musb_start/stop() in gadget_start/stop()
 * @ is_power_saving_mode: if there is only one port which supports OTG, usb power
 *		and clocks can be closed when plug out cable, so the mode is what
 *		tablet/phone wanted. (on mt8173, should close all LDO, clocks and
 *		powerdown & disconnect all ports.)
 * @ u2_ports: number of usb2.0 host ports
 * @ u3_ports: number of usb3.0 host ports
 * @vi_dev: virtual power-key input device, only for usb host wakeup
 */
struct ssusb_mtk {
	struct device *dev;
	struct musb *mu3di;
	/* struct platform_device *mu3d; */
	struct platform_device *xhci;
	struct resource xhci_rscs[SSUSB_XHCI_RSCS_NUM];
	struct resource mu3d_rscs[SSUSB_MU3D_RSCS_NUM];
	struct musb_hdrc_platform_data *pdata;
	void __iomem *mac_base;
	void __iomem *sif_base;
	/* power & clock */
	struct regulator *vusb33;
	struct regulator *usbnet33;
	void __iomem *uap_pll_con;
	struct clk *scp_sys;
	struct clk *peri_usb0;
	struct clk *peri_usb1;
	struct mutex power_mutex;
	int is_power_on;
	/* otg */
	int mu3d_irq;
	int p1_exist;
	int p1_vbus_gpio_num;
	enum ssusb_vbus_mode p1_vbus_mode;
	struct vbus_ctrl_info p1_vbus;
	struct otg_switch_mtk otg_switch;
	enum ssusb_mode drv_mode;
	enum ssusb_str_mode str_mode;
	int start_mu3d;
	int is_power_saving_mode;
	int u2_ports;
	int u3_ports;
	int ic_version;
	/* usb wakeup */
	void __iomem *perisys;
	struct input_dev *vi_dev;
	int wakeup_src;
};

#define glue_to_musb(g)		platform_get_drvdata(g->mu3d)


/**
 * struct musb_platform_ops - Operations passed to musb_core by HW glue layer
 * @init:	turns on clocks, sets up platform-specific registers, etc
 * @exit:	undoes @init
 * @set_mode:	forcefully changes operating mode
 * @try_ilde:	tries to idle the IP
 * @vbus_status: returns vbus status if possible
 * @set_vbus:	forces vbus status
 * @adjust_channel_params: pre check for standard dma channel_program func
 */
struct musb_platform_ops {
	int (*init)(struct musb *musb);
	int (*exit)(struct musb *musb);

	void (*enable)(struct musb *musb);
	void (*disable)(struct musb *musb);

	int (*set_mode)(struct musb *musb, u8 mode);
	void (*try_idle)(struct musb *musb, unsigned long timeout);

	int (*vbus_status)(struct musb *musb);
	void (*set_vbus)(struct musb *musb, int on);

	/* int (*adjust_channel_params) (struct dma_channel *channel, */
	/* u16 packet_sz, u8 *mode, dma_addr_t *dma_addr, u32 *len); */
};

/*
 * struct musb_hw_ep - endpoint hardware (bidirectional)
 *
 * Ordered slightly for better cacheline locality.
 */
struct musb_hw_ep {
	struct musb *musb;
	void __iomem *fifo;
	void __iomem *regs;

	/* For ssusb+ , only for non-ep0 */
	void __iomem *addr_txcsr0;
	void __iomem *addr_txcsr1;
	void __iomem *addr_txcsr2;
	/* void __iomem          *addr_txmaxpktsz; */

	void __iomem *addr_rxcsr0;
	void __iomem *addr_rxcsr1;
	void __iomem *addr_rxcsr2;
	void __iomem *addr_rxcsr3;
	void __iomem *addr_rxmaxpktsz;
	/* For ssusb- */

	/* index in musb->endpoints[]  */
	u8 epnum;

	/* hardware configuration, possibly dynamic */
	bool is_shared_fifo;
	/* bool                  tx_double_buffered; */
	/* bool                  rx_double_buffered; */
	u16 max_packet_sz_tx;
	u16 max_packet_sz_rx;

	/* For ssusb+ */
	u32 fifoaddr_tx;
	u32 fifoaddr_rx;

	u8 mult_tx;
	u8 mult_rx;

	u8 interval_tx;
	u8 interval_rx;
	/* For ssusb- */

	/* struct dma_channel *tx_channel; */
	/* struct dma_channel *rx_channel; */

	/* void __iomem *target_regs; */

	/* currently scheduled peripheral endpoint */
	struct musb_qh *in_qh;
	struct musb_qh *out_qh;

	/* u8 rx_reinit; */
	/* u8 tx_reinit; */

	/* peripheral side */
	struct musb_ep ep_in;	/* TX */
	struct musb_ep ep_out;	/* RX */
};

static inline struct musb_request *next_in_request(struct musb_hw_ep *hw_ep)
{
	return next_request(&hw_ep->ep_in);
}

static inline struct musb_request *next_out_request(struct musb_hw_ep *hw_ep)
{
	return next_request(&hw_ep->ep_out);
}

static inline struct ssusb_mtk *dev_to_ssusb(struct device *dev)
{
	return dev_get_drvdata(dev);
}


#ifdef NEVER
struct musb_csr_regs {
	/* FIFO registers */
	u16 txmaxp, txcsr, rxmaxp, rxcsr;
	u16 rxfifoadd, txfifoadd;
	u8 txtype, txinterval, rxtype, rxinterval;
	u8 rxfifosz, txfifosz;
	u8 txfunaddr, txhubaddr, txhubport;
	u8 rxfunaddr, rxhubaddr, rxhubport;
};

struct musb_context_registers {

	u8 power;
	u16 intrtxe, intrrxe;
	u8 intrusbe;
	u16 frame;
	u8 index, testmode;

	u8 devctl, busctl, misc;

	struct musb_csr_regs index_regs[MUSB_C_NUM_EPS];
};
#endif				/* NEVER */

#ifdef CONFIG_SSUSB_DRV
struct musb_csr_regs {
	/* FIFO registers */
	/* u32 txcsr0, txcsr1, txcsr2; */
	/* u32 rxcsr0, rxcsr1, rxcsr2; */
#ifdef USE_SSUSB_QMU
	u32 txqmuaddr, rxqmuaddr;
#endif
	/* u16 txmaxp, txcsr, rxmaxp, rxcsr; */
	/* u16 rxfifoadd, txfifoadd; */
	/* u8 txtype, txinterval, rxtype, rxinterval; */
	/* u8 rxfifosz, txfifosz; */

	/* u8 txfunaddr, txhubaddr, txhubport; */
	/* u8 rxfunaddr, rxhubaddr, rxhubport; */
};

struct musb_context_registers {

	/* u8 power; */
	/* u16 intrtxe, intrrxe; */
	/* u8 intrusbe; */
	/* u16 frame; */
	/* u8 index, testmode; */

	/* u8 devctl, busctl, misc; */
	/* u32 intr_ep; */
	/* u32 ep0_csr; */
	/* u32 qmu_crs, intr_qmu_done; */
	struct musb_csr_regs index_regs[MUSB_C_NUM_EPS];
};
#endif

/*
 * struct musb - Driver instance data.
 */
struct musb {
	/* device lock */
	spinlock_t lock;

	const struct musb_platform_ops *ops;
	struct musb_context_registers context;

	 irqreturn_t (*isr)(int, void *);

	u32 hwvers;

/* this hub status bit is reserved by USB 2.0 and not seen by usbcore */
/* #define MUSB_PORT_STAT_RESUME (1 << 31) */

/* u32 port1_status; */

/* unsigned long rh_timer; */

	enum musb_h_ep0_state ep0_stage;

	/* bulk traffic normally dedicates endpoint hardware, and each
	 * direction has its own ring of host side endpoints.
	 * we try to progress the transfer at the head of each endpoint's
	 * queue until it completes or NAKs too much; then we try the next
	 * endpoint.
	 */

	int start_mu3d;
	struct ssusb_mtk *ssusb;

	struct device *controller;

	void __iomem *mac_base;
	void __iomem *sif_base;



	/* passed down from chip/board specific irq handlers */
	u32 int_usb;
	u16 int_rx;
	u16 int_tx;

	struct usb_phy *xceiv;

	int irq;
	unsigned irq_wake:1;

	struct musb_hw_ep endpoints[MUSB_C_NUM_EPS];
/* #define control_ep            endpoints */

	u16 epmask;
	u8 nr_endpoints;

	u8 board_mode;		/* enum musb_mode */
	/* active means connected and not suspended */
	unsigned is_active:1;	/* maybe some wrong to set to 1, TODO modify it later. yun */

	/* is_suspended means USB B_PERIPHERAL suspend */
	unsigned is_suspended:1;

	/* may_wakeup means remote wakeup is enabled */
	unsigned may_wakeup:1;

	/* is_self_powered is reported in device status and the
	 * config descriptor.  is_bus_powered means B_PERIPHERAL
	 * draws some VBUS current; both can be true.
	 */
	unsigned is_self_powered:1;
	unsigned is_bus_powered:1;

	unsigned set_address:1;
	unsigned test_mode:1;
	unsigned softconnect:1;
	unsigned bU1Enabled:1;
	unsigned bU2Enabled:1;

	u8 address;
	u8 test_mode_nr;
	u32 ackpend;		/* ep0 We don't maintain Max Packet size in it. */
	enum musb_g_ep0_state ep0_state;
	struct usb_gadget g;	/* the gadget */
	struct usb_gadget_driver *gadget_driver;	/* its driver */

	struct musb_hdrc_config *config;

#ifdef MUSB_CONFIG_PROC_FS
	struct proc_dir_entry *proc_entry;
#endif

	u32 txfifoadd_offset;
	u32 rxfifoadd_offset;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
#endif

	unsigned is_clk_on;	/* deprecated */
	unsigned usb_mode;
	unsigned active_ep;
	struct work_struct suspend_work;
	struct work_struct otg_event_work;
	struct delayed_work connection_work;
	struct workqueue_struct *wq;
	struct wake_lock usb_wakelock;

#ifdef USE_SSUSB_QMU
	struct tasklet_struct qmu_done;
	u32 qmu_done_intr;
#endif
};

static inline struct musb *gadget_to_musb(struct usb_gadget *g)
{
	return container_of(g, struct musb, g);
}


static inline void musb_configure_ep0(struct musb *musb)
{
	musb->endpoints[0].max_packet_sz_tx = MUSB_EP0_FIFOSIZE;
	musb->endpoints[0].max_packet_sz_rx = MUSB_EP0_FIFOSIZE;
	musb->endpoints[0].is_shared_fifo = true;
}

static inline int is_first_entry(const struct list_head *list, const struct list_head *head)
{
	return list_is_last(head, list);
}

void musb_dev_on_off(struct musb *musb, int is_on);
int ssusb_gadget_init(struct ssusb_mtk *ssusb);
void ssusb_gadget_exit(struct ssusb_mtk *ssusb);


/***************************** Glue it together *****************************/

extern const char musb_driver_name[];

extern void musb_start(struct musb *musb);
extern void musb_stop(struct musb *musb);

extern void musb_write_fifo(struct musb_hw_ep *ep, u16 len, const u8 *src);
extern void musb_read_fifo(struct musb_hw_ep *ep, u16 len, u8 *dst);

extern void musb_load_testpacket(struct musb *);

extern irqreturn_t musb_interrupt(struct musb *);

/* extern void musb_hnp_stop(struct musb *musb); */

static inline void musb_platform_set_vbus(struct musb *musb, int is_on)
{
	if (musb->ops->set_vbus)
		musb->ops->set_vbus(musb, is_on);
}

static inline void musb_platform_enable(struct musb *musb)
{
	if (musb->ops->enable)
		musb->ops->enable(musb);
}

static inline void musb_platform_disable(struct musb *musb)
{
	if (musb->ops->disable)
		musb->ops->disable(musb);
}

static inline int musb_platform_set_mode(struct musb *musb, u8 mode)
{
	if (!musb->ops->set_mode)
		return 0;

	return musb->ops->set_mode(musb, mode);
}

static inline void musb_platform_try_idle(struct musb *musb, unsigned long timeout)
{
	if (musb->ops->try_idle)
		musb->ops->try_idle(musb, timeout);
}

static inline int musb_platform_get_vbus_status(struct musb *musb)
{
	if (!musb->ops->vbus_status)
		return 0;

	return musb->ops->vbus_status(musb);
}

static inline int musb_platform_init(struct musb *musb)
{
	if (!musb->ops->init)
		return -EINVAL;

	return musb->ops->init(musb);
}

static inline int musb_platform_exit(struct musb *musb)
{
	if (!musb->ops->exit)
		return -EINVAL;

	return musb->ops->exit(musb);
}

/*
* treat SSUSB_OTG_IDPIN as SSUSB_OTG_MANUAL;
*/
static inline int is_maual_otg(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;

	return ((SSUSB_MODE_DRD == ssusb->drv_mode) &&
		(SSUSB_OTG_MANUAL == otg_switch->otg_mode)) ||
	    (SSUSB_OTG_IDPIN == otg_switch->otg_mode);
}

static inline int is_eint_used(enum ssusb_otg_mode otg_mode)
{
	return (SSUSB_OTG_CHEINT == otg_mode) || (SSUSB_OTG_IDPIN == otg_mode);
}

static inline int need_vbus_chg_int(struct musb *musb)
{
	struct ssusb_mtk *ssusb = musb->ssusb;
	struct otg_switch_mtk *otg_switch = &ssusb->otg_switch;

	return SSUSB_OTG_IDPIN == otg_switch->otg_mode;
}


bool usb_cable_connected(void);
void mt_usb_disconnect(void);
void mt_usb_connect(void);
void musb_sync_with_bat(struct musb *musb, int usb_state);
/* extern void usb_phy_savecurrent(unsigned int clk_on); */
/* extern void usb_phy_recover(unsigned int clk_on); */
void connection_work(struct work_struct *data);
bool mtk_is_host_mode(void);	/* remove later */
u32 get_devinfo_with_index(u32 index);
int ssusb_power_save(struct ssusb_mtk *ssusb);
int ssusb_power_restore(struct ssusb_mtk *ssusb);
void ssusb_otg_iddig_en(struct ssusb_mtk *ssusb);
void ssusb_otg_iddig_dis(struct ssusb_mtk *ssusb);
void ssusb_otg_plug_in(struct ssusb_mtk *ssusb);
void ssusb_otg_plug_out(struct ssusb_mtk *ssusb);
int is_init_host_for_manual_otg(struct ssusb_mtk *ssusb);
int is_ssusb_connected_to_pc(struct ssusb_mtk *ssusb);
void __iomem *get_xhci_base(void);
void ep0_setup(struct musb *musb, struct musb_hw_ep *hw_ep0, const struct musb_fifo_cfg *cfg);
extern void bat_charger_update_usb_state(int usb_state);
extern int bat_charger_type_detection(void);

extern bool upmu_is_chr_det(void);
extern void BATTERY_SetUSBState(int usb_state);
extern void upmu_interrupt_chrdet_int_en(u32 val);
extern u32 upmu_get_rgs_chrdet(void);

#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2017 MediaTek Inc.
 */

#ifndef __MUSB_CORE_H__
#define __MUSB_CORE_H__

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb.h>
#include <linux/usb/otg.h>
#include "musb.h"
#include <linux/pm_wakeup.h>
#include <linux/version.h>
#include <linux/clk.h>
#if defined(CONFIG_MTK_CHARGER)
extern enum charger_type mt_get_charger_type(void);
#endif

/* to prevent 32 bit project misuse */
#if defined(CONFIG_MTK_MUSB_DRV_36BIT) && !defined(CONFIG_64BIT)
#error
#endif

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
#include "mtk_qmu.h"
#endif

#define SHARE_IRQ -1

struct musb;
struct musb_hw_ep;
struct musb_ep;
extern int musb_fake_CDP;
extern int kernel_init_done;
extern int musb_force_on;
extern int musb_host_dynamic_fifo;
extern int musb_host_dynamic_fifo_usage_msk;
extern unsigned int musb_uart_debug;
extern bool musb_host_db_enable;
extern bool musb_host_db_workaround1;
extern bool musb_host_db_workaround2;
extern long musb_host_db_delay_ns;
extern long musb_host_db_workaround_cnt;
extern struct musb *mtk_musb;
extern bool mtk_usb_power;
extern ktime_t ktime_ready;
extern int ep_config_from_table_for_host(struct musb *musb);
extern int polling_vbus_value(void *data);

#if defined(CONFIG_USBIF_COMPLIANCE)
extern bool polling_vbus;
extern struct task_struct *vbus_polling_tsk;
extern void musb_set_host_request_flag(struct musb *musb, unsigned int value);
extern void pmic_bvalid_det_int_en(int i);
#define CONFIG_USBIF_COMPLIANCE_PMIC
#if defined(CONFIG_USBIF_COMPLIANCE_PMIC)
extern int PMIC_IMM_GetOneChannelValue(int dwChannel, int deCount, int trimd);
#define MT6328_AUX_VCDT_AP 2	/* include/mach/upmu_sw.h */
#define AUX_VCDT_AP MT6328_AUX_VCDT_AP	/* include/mach/upmu_sw.h */
#define R_CHARGER_1 330
#define R_CHARGER_2 39
#else
extern signed_int battery_meter_get_charger_voltage(void);
#endif
extern void send_otg_event(enum usb_otg_event event);
#endif
extern void musb_bug(void);

/* Helper defines for struct musb->hwvers */
#define MUSB_HWVERS_MAJOR(x)	((x >> 10) & 0x1f)
#define MUSB_HWVERS_MINOR(x)	(x & 0x3ff)
#define MUSB_HWVERS_RC		0x8000
#define MUSB_HWVERS_1300	0x52C
#define MUSB_HWVERS_1400	0x590
#define MUSB_HWVERS_1800	0x720
#define MUSB_HWVERS_1900	0x784
#define MUSB_HWVERS_2000	0x800
#define MUSB_HUB_SUPPORT    0x4000
#include "mtk_musb.h"

#include "musb_debug.h"
#include "musb_dma.h"

#include "musb_io.h"
#include "mtk_musb_reg.h"

#include "musb_gadget.h"
#include <linux/usb/hcd.h>
#include "musb_host.h"

#ifdef CONFIG_DUAL_ROLE_USB_INTF
#include <linux/usb/class-dual-role.h>
#endif

/* NOTE:  otg and peripheral-only state machines start at B_IDLE.
 * OTG or host-only go to A_IDLE when ID is sensed.
 */
#define is_peripheral_active(m)		(!(m)->is_host)
#define is_host_active(m)		((m)->is_host)

#ifdef CONFIG_PROC_FS
#include <linux/fs.h>
#define MUSB_CONFIG_PROC_FS
#endif

/****************************** PERIPHERAL ROLE *****************************/

extern irqreturn_t musb_g_ep0_irq(struct musb *musb);
extern void musb_g_tx(struct musb *musb, u8 epnum);
extern void musb_g_rx(struct musb *musb, u8 epnum);
extern void musb_g_reset(struct musb *musb);
extern void musb_g_suspend(struct musb *musb);
extern void musb_g_resume(struct musb *musb);
extern void musb_g_wakeup(struct musb *musb);
extern void musb_g_disconnect(struct musb *musb);

/****************************** HOST ROLE ***********************************/
extern irqreturn_t musb_h_ep0_irq(struct musb *musb);
extern void musb_host_tx(struct musb *musb, u8 epnum);
extern void musb_host_rx(struct musb *musb, u8 epnum);

/****************************** CONSTANTS ********************************/

#ifdef CONFIG_MTK_MUSB_CARPLAY_SUPPORT
extern bool apple;
extern void musb_id_pin_work_device(void);
extern void musb_id_pin_work_host(struct work_struct *data);
extern int send_switch_cmd(void);
#endif



#ifndef MUSB_C_NUM_EPS
#define MUSB_C_NUM_EPS ((u8)16)
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
} __packed;

/* peripheral side ep0 states */
enum musb_g_ep0_state {
	MUSB_EP0_STAGE_IDLE,	/* idle, waiting for SETUP */
	MUSB_EP0_STAGE_SETUP,	/* received SETUP */
	MUSB_EP0_STAGE_TX,	/* IN data */
	MUSB_EP0_STAGE_RX,	/* OUT data */
	MUSB_EP0_STAGE_STATUSIN,	/* (after OUT data) */
	MUSB_EP0_STAGE_STATUSOUT,	/* (after IN data) */
	MUSB_EP0_STAGE_ACKWAIT,	/* after zlp, before statusin */
} __packed;

/*
 * OTG protocol constants.  See USB OTG 1.3 spec,
 * sections 5.5 "Device Timings" and 6.6.5 "Timers".
 */
#define OTG_TIME_A_WAIT_VRISE	100	/* msec (max) */
/* when switch host to device within min 1 second, the otg state can't*/
/* switch to b-idle successfully, then connect to host and can't run */
/* gadget rest in BUS_RESET, so need to decrease min 1 second to 600ms*/
#define OTG_TIME_A_WAIT_BCON	600	/* min 1 second */
#define OTG_TIME_A_AIDL_BDIS	200	/* min 200 msec */
#if defined(CONFIG_USBIF_COMPLIANCE)
#define OTG_TIME_B_ASE0_BRST	155	/* min 3.125 ms */
#else
#define OTG_TIME_B_ASE0_BRST	100	/* min 3.125 ms */
#endif



/*************************** REGISTER ACCESS ********************************/

#define musb_ep_select(_mbase, _epnum) \
	musb_writeb((_mbase), MUSB_INDEX, (_epnum))
#define	MUSB_EP_OFFSET			MUSB_INDEXED_OFFSET

/****************************** FUNCTIONS ********************************/

#define MUSB_HST_MODE(_musb)\
	{ (_musb)->is_host = true; }
#define MUSB_DEV_MODE(_musb) \
	{ (_musb)->is_host = false; }

#define test_devctl_hst_mode(_x) \
	(musb_readb((_x)->mregs, MUSB_DEVCTL)&MUSB_DEVCTL_HM)

#define MUSB_MODE(musb) ((musb)->is_host ? "Host" : "Peripheral")

enum writeFunc_enum {
	funcWriteb = 0,
	funcWritew,
	funcWritel,
	funcInterrupt
};

void dumpTime(enum writeFunc_enum func, int epnum);

/******************************** TYPES *************************************/

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

	int (*adjust_channel_params)(struct dma_channel *channel,
		u16 packet_sz, u8 *mode, dma_addr_t *dma_addr, u32 *len);

	void (*enable_clk)(struct musb *musb);
	void (*disable_clk)(struct musb *musb);
	void (*prepare_clk)(struct musb *musb);
	void (*unprepare_clk)(struct musb *musb);
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

	/* index in musb->endpoints[]  */
	u8 epnum;
	enum musb_ep_mode ep_mode;

	/* hardware configuration, possibly dynamic */
	bool is_shared_fifo;
	bool tx_double_buffered;
	bool rx_double_buffered;
	u16 max_packet_sz_tx;
	u16 max_packet_sz_rx;

	struct dma_channel *tx_channel;
	struct dma_channel *rx_channel;

	void __iomem *target_regs;

	/* currently scheduled peripheral endpoint */
	struct musb_qh *in_qh;
	struct musb_qh *out_qh;

	u8 rx_reinit;
	u8 tx_reinit;

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
	u8 intrusbe;
	u16 frame;
	u8 index, testmode;

	u8 devctl, busctl, misc;
	u32 otg_interfsel;
	u32 l1_int;

	struct musb_csr_regs index_regs[MUSB_C_NUM_EPS];
};

/*
 * struct musb - Driver instance data.
 */
struct musb {
	struct semaphore musb_lock;
	/* device lock */
	spinlock_t lock;
	const struct musb_platform_ops *ops;
	struct musb_context_registers context;

	irqreturn_t (*isr)(int irq, void *private_data);
	struct work_struct irq_work;
	struct work_struct otg_notifier_work;
	u16 hwvers;
	struct delayed_work id_pin_work;
#ifdef CONFIG_MTK_MUSB_CARPLAY_SUPPORT
	struct delayed_work carplay_work;
#endif
	struct musb_fifo_cfg *fifo_cfg;
	unsigned int fifo_cfg_size;
	struct musb_fifo_cfg *fifo_cfg_host;
	unsigned int fifo_cfg_host_size;
	u32 fifo_size;

	u16 intrrxe;
	u16 intrtxe;
/* this hub status bit is reserved by USB 2.0 and not seen by usbcore */
#define MUSB_PORT_STAT_RESUME	(1 << 31)

	u32 port1_status;

	unsigned long rh_timer;

	enum musb_h_ep0_state ep0_stage;

	/* bulk traffic normally dedicates endpoint hardware, and each
	 * direction has its own ring of host side endpoints.
	 * we try to progress the transfer at the head of each endpoint's
	 * queue until it completes or NAKs too much; then we try the next
	 * endpoint.
	 */
	struct musb_hw_ep *bulk_ep;

	struct list_head control;	/* of musb_qh */
	struct list_head in_bulk;	/* of musb_qh */
	struct list_head out_bulk;	/* of musb_qh */

	struct timer_list otg_timer;
	struct timer_list idle_timer;
#if defined(CONFIG_USBIF_COMPLIANCE)
	struct timer_list otg_vbus_polling_timer;
#endif
	struct notifier_block nb;

	struct dma_controller *dma_controller;

	struct device *controller;
	void __iomem *ctrl_base;
	void __iomem *mregs;

	/* passed down from chip/board specific irq handlers */
	u8 int_usb;
	u16 int_rx;
	u16 int_tx;

#ifdef CONFIG_MTK_MUSB_QMU_SUPPORT
	u32 int_queue;
#endif

	struct usb_phy *xceiv;
	unsigned int efuse_val;
	u8 xceiv_event;

	int nIrq;
	int dma_irq;
	unsigned irq_wake:1;

	struct musb_hw_ep endpoints[MUSB_C_NUM_EPS];
#define control_ep		endpoints

#define VBUSERR_RETRY_COUNT	3
	u16 vbuserr_retry;
	u16 epmask;
	u8 nr_endpoints;

	int (*board_set_power)(int state);
	void (*usb_rev6_setting)(int value);

	u8 min_power;		/* vbus for periph, in mA/2 */

	bool is_host;
	bool in_ipo_off;

	int a_wait_bcon;	/* VBUS timeout in msecs */
	unsigned long idle_timeout;	/* Next timeout in jiffies */

	/* active means connected and not suspended */
	unsigned is_active:1;

	unsigned is_multipoint:1;
	unsigned ignore_disconnect:1;	/* during bus resets */

	unsigned hb_iso_rx:1;	/* high bandwidth iso rx? */
	unsigned hb_iso_tx:1;	/* high bandwidth iso tx? */
	unsigned dyn_fifo:1;	/* dynamic FIFO supported? */

	unsigned bulk_split:1;
#define	can_bulk_split(musb, type) \
	(((type) == USB_ENDPOINT_XFER_BULK) && (musb)->bulk_split)

	unsigned bulk_combine:1;
#define	can_bulk_combine(musb, type) \
	(((type) == USB_ENDPOINT_XFER_BULK) && (musb)->bulk_combine)

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

	u8 address;
	u8 test_mode_nr;
	u16 ackpend;		/* ep0 */
	enum musb_g_ep0_state ep0_state;
	struct usb_gadget g;	/* the gadget */
	struct usb_gadget_driver *gadget_driver;	/* its driver */
	struct wakeup_source *usb_lock;

	/*
	 * FIXME: Remove this flag.
	 *
	 * This is only added to allow Blackfin to work
	 * with current driver. For some unknown reason
	 * Blackfin doesn't work with double buffering
	 * and that's enabled by default.
	 *
	 * We added this flag to forcefully disable double
	 * buffering until we get it working.
	 */
	unsigned double_buffer_not_ok:1;

	struct musb_hdrc_config *config;

#ifdef MUSB_CONFIG_PROC_FS
	struct proc_dir_entry *proc_entry;
#endif
	int xceiv_old_state;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
#endif
	bool power;
	unsigned is_ready:1;
	bool usb_if;
	u16 fifo_addr;
#if defined(CONFIG_USBIF_COMPLIANCE)
	bool srp_drvvbus;
	enum usb_otg_event otg_event;
#endif
	struct workqueue_struct *st_wq;
#ifdef CONFIG_DUAL_ROLE_USB_INTF
	struct dual_role_phy_instance *dr_usb;
#endif /* CONFIG_DUAL_ROLE_USB_INTF */
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;
};

static inline struct musb *gadget_to_musb(struct usb_gadget *g)
{
	return container_of(g, struct musb, g);
}

static inline int musb_read_fifosize
	(struct musb *musb, struct musb_hw_ep *hw_ep, u8 epnum)
{
	void __iomem *mbase = musb->mregs;
	u8 reg = 0;

	/* read from core using indexed model */
	reg = musb_readb(mbase, MUSB_EP_OFFSET(epnum, MUSB_FIFOSIZE));
	/* 0's returned when no more endpoints */
	if (!reg)
		return -ENODEV;

	musb->nr_endpoints++;
	musb->epmask |= (1 << epnum);

	hw_ep->max_packet_sz_tx = 1 << (reg & 0x0f);

	/* shared TX/RX FIFO? */
	if ((reg & 0xf0) == 0xf0) {
		hw_ep->max_packet_sz_rx = hw_ep->max_packet_sz_tx;
		hw_ep->is_shared_fifo = true;
		return 0;
	}
	hw_ep->max_packet_sz_rx = 1 << ((reg & 0xf0) >> 4);
	hw_ep->is_shared_fifo = false;

	return 0;
}

static inline void musb_configure_ep0(struct musb *musb)
{
	musb->endpoints[0].max_packet_sz_tx = MUSB_EP0_FIFOSIZE;
	musb->endpoints[0].max_packet_sz_rx = MUSB_EP0_FIFOSIZE;
	musb->endpoints[0].is_shared_fifo = true;
}


/***************************** Glue it together *****************************/

extern const char musb_driver_name[];

extern void musb_start(struct musb *musb);
extern void musb_stop(struct musb *musb);
extern int musb_get_id(struct device *dev, gfp_t gfp_mask);
extern void musb_put_id(struct device *dev, int id);

extern void musb_write_fifo(struct musb_hw_ep *ep, u16 len, const u8 *src);
extern void musb_read_fifo(struct musb_hw_ep *ep, u16 len, u8 *dst);

extern void musb_load_testpacket(struct musb *musb);
extern void musb_generic_disable(struct musb *musb);
extern irqreturn_t musb_interrupt(struct musb *musb);
extern irqreturn_t dma_controller_irq(int irq, void *private_data);

extern void musb_hnp_stop(struct musb *musb);

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

static inline void
	musb_platform_try_idle(struct musb *musb, unsigned long timeout)
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

static inline void musb_platform_enable_clk(struct musb *musb)
{
	if (musb->ops->enable_clk)
		musb->ops->enable_clk(musb);
}

static inline void musb_platform_disable_clk(struct musb *musb)
{
	if (musb->ops->disable_clk)
		musb->ops->disable_clk(musb);
}

static inline void musb_platform_prepare_clk(struct musb *musb)
{
	if (musb->ops->prepare_clk)
		musb->ops->prepare_clk(musb);
}

static inline void musb_platform_unprepare_clk(struct musb *musb)
{
	if (musb->ops->unprepare_clk)
		musb->ops->unprepare_clk(musb);
}

/* #if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0) */
static inline const char *otg_state_string(enum usb_otg_state state)
{
	return usb_otg_state_string(state);
}

enum {
	USB_DPIDLE_ALLOWED = 0,
	USB_DPIDLE_FORBIDDEN,
	USB_DPIDLE_SRAM,
	USB_DPIDLE_TIMER
};
extern void usb_hal_dpidle_request(int mode);
extern void register_usb_hal_dpidle_request(void (*function)(int));
extern void register_usb_hal_disconnect_check(void (*function)(void));
extern void wake_up_bat(void);
extern void wait_tx_done(u8 epnum, unsigned int timeout_ns);
extern int host_tx_refcnt_inc(int epnum);
extern int host_tx_refcnt_dec(int epnum);
extern void host_tx_refcnt_reset(int epnum);
extern void dump_tx_ops(u8 ep_num);
#endif				/* __MUSB_CORE_H__ */

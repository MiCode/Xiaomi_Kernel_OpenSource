/*
 * Intel Shared Transport Driver
 * Copyright (C) 2013 -2014, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/lbf_ldisc.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/bitops.h>

struct intel_bt_gpio_data {
	struct gpio_desc	*reg_on_gpio;
	int	host_wake_irq;
};

struct intel_bt_gpio_desc {
	int			reg_on_idx;
	int			host_wake_idx;
};
static struct intel_bt_gpio_desc acpi_default_bluetooth = {
	.reg_on_idx = 1,
	.host_wake_idx = 0,
};


static struct acpi_device_id intel_id_table[] = {
	/* ACPI IDs here */
	{ "INT33E1", (kernel_ulong_t)&acpi_default_bluetooth },
	{ },
};

MODULE_DEVICE_TABLE(acpi, intel_id_table);


struct intel_bt_lpm_platform_data {
	int gpio_wake;          /* CPU -> BCM wakeup gpio */
	int gpio_host_wake;     /* BCM -> CPU wakeup gpio */
	int int_host_wake;      /* BCM -> CPU wakeup irq */
	int gpio_enable;        /* GPIO enable/disable BT/FM */
	int port;               /* UART port to use with BT/FM */
};

DECLARE_WAIT_QUEUE_HEAD(waitFordxexit);
DECLARE_WAIT_QUEUE_HEAD(waitFord0exit);

struct intel_bt_lpm {

	unsigned int gpio_host_wake;
	unsigned int int_host_wake;
	unsigned int bt_module_state;
	unsigned int fm_module_state;
	unsigned int bt_fmr_state;
	unsigned int module_refcount;
	unsigned char hostwake;
	int dx_packet_pending;
	int d0_packet_pending;
	struct gpio_desc *gpio_enable_bt;
	int host_wake;
	int lpm_enable;
	struct mutex lpmtxlock;
	struct mutex idleupdatelock;
	struct mutex lpmenable;
	struct platform_device *pdev;
	struct device *tty_dev;
	spinlock_t rxref_lock;
	spinlock_t interrupt_lock;
	spinlock_t txref_lock;
	spinlock_t devicest_lock;
	spinlock_t ackrsp_lock;
	spinlock_t wakeuprsp_lock;
	spinlock_t host_wake_lock;
	spinlock_t lpm_modulestate;
	int tx_refcount;
	int device_state;
	int rx_refcount;
	int port;
} intel_lbf_lpm;


struct lbf_uart {

	unsigned long rx_state;
	unsigned long rx_count;
	char *read_buf;
	int read_head;
	int read_cnt;
	int read_tail;
	wait_queue_t wait;
	wait_queue_head_t read_wait;
	struct mutex atomic_read_lock;
	spinlock_t tx_lock;
	spinlock_t tx_update_lock;
	struct tty_struct *tty;
	spinlock_t rx_lock;
	struct sk_buff *rx_skb;
	unsigned short rx_chnl;
	unsigned short type;
	unsigned short bytes_pending;
	unsigned short len;

};

struct lbf_q_tx {
	struct sk_buff_head txq, tx_waitq;
	struct sk_buff *tx_skb;
	spinlock_t lock;
	struct mutex writelock;
	struct mutex fwdownloadlock;
	struct work_struct tx_wakeup_work;
	struct work_struct hostwake_update_work;
	struct work_struct host_enable_work;
	struct work_struct host_wake_high_work;
	struct mutex tx_wakeup;
	int tbusy;
	int woke_up;
	struct tty_struct *tty;
};

struct st_proto_s {
	unsigned char chnl_id;
	unsigned char hdr_len;
	unsigned char offset_len_in_hdr;
	unsigned char len_size;
	unsigned char reserve;
};

enum {
	MODULE_INVALID,
	MODULE_FM,
	MODULE_BT,
};

enum {
	LOW,
	HIGH
};

enum ioctl_status {
	DO_FW_DL,
	DO_STACK_INIT,
	FW_FAILED,
	FW_SUCCESS
};

#define DX_HCI_CMD_SIZE	4
#define D0_HCI_CMD_SIZE	1
#define WAKEUP_RSP_SIZE	3
#define RECEIVE_ROOM	4096
#define IGNORE	-3
#define FAILED	4
#define HOSTWAKE_HIGH		BIT(0)  /* 2^0, bit 0 */
#define HOSTWAKE_LOW		BIT(1)  /* 2^1, bit 1 */
#define HOSTWAKE_D0		BIT(2)  /* 2^2, bit 2 */
#define HOSTWAKE_D0_TO_D2	BIT(3)  /* 2^3, bit 3 */
#define HOSTWAKE_D2_TO_D0	BIT(4) /* 2^4, bit 4 */
#define HOSTWAKE_D2		BIT(5) /* and so on...*/



/*------ Forward Declaration-----*/
struct sk_buff *lbf_dequeue(void);
static int lbf_tty_write(struct tty_struct *tty, const unsigned char *data,
				int count);
static void lbf_enqueue(struct sk_buff *skb);
static void bt_rfkill_set_power(unsigned long blocked);
static void lbf_update_set_room(struct tty_struct *tty, signed int cnt);
static void lbf_ldisc_fw_download_complete(unsigned long);
static void activate_irq_handler(void);
static int lbf_ldisc_fw_download_init(void);
static int lpm_tx_update(int module);
static int enqueue_dx_packet(uint8_t *hci_cmd, int size);
static void lbf_tx_wakeup(struct lbf_uart *lbf_uart);
static int wait_dx_exit(void);
static void lbf_ldisc_lpm_enable_cleanup(void);
static int send_d0_dx_packet(void);

static struct st_proto_s lbf_st_proto[6] = {
		{ .chnl_id = PM_PKT, /* PM_PACKET */
		.hdr_len = HCI_EVENT_HDR_SIZE, .offset_len_in_hdr = 2,
				.len_size = 1, .reserve = 8, },
		{ .chnl_id = INVALID, /* ACL */
		.hdr_len = INVALID, .offset_len_in_hdr = INVALID,
				.len_size = INVALID, .reserve = INVALID, },
		{ .chnl_id = HCI_ACLDATA_PKT, /* ACL */
		.hdr_len = HCI_ACL_HDR_SIZE, .offset_len_in_hdr = 3,
				.len_size = 2, .reserve = 8, },
		{ .chnl_id = HCI_SCODATA_PKT, /* SCO */
		.hdr_len = HCI_SCO_HDR_SIZE, .offset_len_in_hdr = 3,
				.len_size = 1, .reserve = 8, },
		{ .chnl_id = HCI_EVENT_PKT, /* HCI Events */
		.hdr_len = HCI_EVENT_HDR_SIZE, .offset_len_in_hdr = 2,
				.len_size = 1, .reserve = 8, },
		{ .chnl_id = HCI_EVENT_PKT, /* HCI Events */
		.hdr_len = HCI_EVENT_HDR_SIZE, .offset_len_in_hdr = 2,
				.len_size = 1, .reserve = 8, }, };

/* Static variable */

static int fw_isregister;
static int fw_download_state;
static unsigned int lbf_ldisc_running = INVALID;
static int bt_enable_state = DISABLE;
static struct lbf_q_tx *lbf_tx;
static struct fm_ld_drv_register *fm;

/*Dx  Entry Request  form host*/
/*Wake Enabled*/
static uint8_t hci_dx_request[] = {0xF1, 0x01, 0x01, 0x01};
/*Wake Enabled response*/
static uint8_t hci_controlerwake_req_rsp[] = {0xF1, 0x03, 0x00};
static uint8_t hci_dx_wake[] = {0xF0}; /*Wakeup BT packet - HIGH*/
static unsigned int ignore_ack;
static unsigned int ignore_wakeuprsp;


/* lbf_set_ackign_st
 *
 * set the ACK RSP processing state.
 *
 * Arguments:
 * state to set
 *
 * Return Value:
 * void
 */
static inline void lbf_set_ackign_st(unsigned int state)
{
	spin_lock(&intel_lbf_lpm.ackrsp_lock);
	ignore_ack = state;
	spin_unlock(&intel_lbf_lpm.ackrsp_lock);
	pr_debug("%s:ignore_ack: %d\n", __func__, ignore_ack);
}

/* lbf_get_ackigne_st
 *
 * get the ACK RSP processing state.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * state of ackrsp processing
 */
static inline unsigned int lbf_get_ackigne_st(void)
{
	unsigned int state;
	spin_lock(&intel_lbf_lpm.ackrsp_lock);
	state = ignore_ack;
	spin_unlock(&intel_lbf_lpm.ackrsp_lock);
	return state;
}

/* lbf_set_wakeupign_st
 *
 * set the WAKEUP RSP processing state.
 *
 * Arguments:
 * state to set
 *
 * Return Value:
 * void
 */
static inline void lbf_set_wakeupign_st(unsigned int state)
{
	spin_lock(&intel_lbf_lpm.wakeuprsp_lock);
	ignore_wakeuprsp = state;
	spin_unlock(&intel_lbf_lpm.wakeuprsp_lock);
	pr_debug("%s:ignore_wakeuprsp: %d\n", __func__, ignore_wakeuprsp);
}

/* lbf_get_wakeupign_st
 *
 * get the wake_up RSP processing state.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * state of wakeup rsp
 */
static inline unsigned int lbf_get_wakeupign_st(void)
{
	unsigned int state;
	spin_lock(&intel_lbf_lpm.wakeuprsp_lock);
	state = ignore_wakeuprsp;
	spin_unlock(&intel_lbf_lpm.wakeuprsp_lock);
	return state;
}

/* lbf_set_device_state
 *
 * set the device state.
 *
 * Arguments:
 * state to set
 *
 * Return Value:
 * void
 */

static inline void lbf_set_device_state(unsigned int state)
{
	spin_lock(&intel_lbf_lpm.devicest_lock);
	intel_lbf_lpm.device_state = state;
	spin_unlock(&intel_lbf_lpm.devicest_lock);
	pr_debug("%s:device_st: %d\n", __func__, intel_lbf_lpm.device_state);
}

/* lbf_get_device_state
 *
 * get the device state.
 *
 * Arguments:
 * void
 *
 * Return Value:
 *	device state
 *
 */

static inline unsigned int lbf_get_device_state(void)
{
	unsigned int device_st;
	spin_lock(&intel_lbf_lpm.devicest_lock);
	device_st = intel_lbf_lpm.device_state;
	spin_unlock(&intel_lbf_lpm.devicest_lock);
	pr_info("%s:device_st: %d\n", __func__, device_st);
	return device_st;
}

/* lbf_set_bt_fmr_state
 *
 * set the bt fmr state.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * void
 */

static inline void lbf_set_bt_fmr_state(void)
{
	spin_lock(&intel_lbf_lpm.lpm_modulestate);
	if (!intel_lbf_lpm.bt_module_state && !intel_lbf_lpm.fm_module_state)
		intel_lbf_lpm.bt_fmr_state = IDLE;
	else
		intel_lbf_lpm.bt_fmr_state = ACTIVE;
	spin_unlock(&intel_lbf_lpm.lpm_modulestate);
	pr_debug("%s:bt_fmr_state: %d\n", __func__, intel_lbf_lpm.bt_fmr_state);
}

/* lbf_get_bt_fmr_state
 *
 * get the bt and fmr  state.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * module state
 */

static inline int lbf_get_bt_fmr_state(void)
{
	int bt_fmr_state;
	spin_lock(&intel_lbf_lpm.lpm_modulestate);
	bt_fmr_state = intel_lbf_lpm.bt_fmr_state;
	spin_unlock(&intel_lbf_lpm.lpm_modulestate);
	pr_debug("%s:bt_fmr_state: %d\n", __func__, bt_fmr_state);
	return bt_fmr_state;
}

/* lbf_serial_get
 *
 * resume UART driver
 *
 * Arguments:
 * tty_dev
 *
 * Return Value:
 * void
 */
static inline void lbf_serial_get(void)
{
	pr_debug("%s\n", __func__);
	/*WARN_ON(!intel_lbf_lpm.tty_dev);
	 For cht refersh: pm_runtime is not supported*/
	/*pm_runtime_get_sync(intel_lbf_lpm.tty_dev);*/
}

static void lbf_host_enable_work(struct work_struct *work)
{
	pr_debug("%s\n", __func__);
	lbf_serial_get();
}

/* lbf_serial_put
 *
 * suspend UART Driver
 *
 * Arguments:
 * tty_dev
 *
 * Return Value:
 * void
 */

static inline void lbf_serial_put(void)
{
	pr_debug("%s\n", __func__);
	/*WARN_ON(!intel_lbf_lpm.tty_dev);
	 For cht refersh: pm_runtime is not supported*/
	/*pm_runtime_put(intel_lbf_lpm.tty_dev);*/
}

/* lbf_set_host_wake_state
 *
 * set the host wake state.
 *
 * Arguments:
 * host_wake state
 *
 * Return Value:
 * void
 */

static inline void lbf_set_host_wake_state(int host_wake)
{
	spin_lock(&intel_lbf_lpm.host_wake_lock);
	intel_lbf_lpm.host_wake = host_wake;
	spin_unlock(&intel_lbf_lpm.host_wake_lock);
	pr_debug("%s:host_wake: %d\n", __func__, intel_lbf_lpm.host_wake);
}

/* lbf_get_tx_ref_count
 *
 * get the tx reference count.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * tx reference count
 */

static inline int lbf_get_tx_ref_count(void)
{
	int ref_count;
	spin_lock(&intel_lbf_lpm.txref_lock);
	ref_count = intel_lbf_lpm.tx_refcount;
	spin_unlock(&intel_lbf_lpm.txref_lock);
	pr_debug("%s:Tx_ref_cnt: %d\n", __func__, intel_lbf_lpm.tx_refcount);

	return ref_count;
}

/* lbf_get_rx_ref_count
 *
 * get the rx reference count.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * rx reference count
 */
static inline int lbf_get_rx_ref_count(void)
{
	int ref_count;
	spin_lock(&intel_lbf_lpm.rxref_lock);
	ref_count = intel_lbf_lpm.rx_refcount;
	spin_unlock(&intel_lbf_lpm.rxref_lock);
	pr_debug("%s:rx_ref_cnt: %d\n", __func__, intel_lbf_lpm.rx_refcount);

	return ref_count;
}

/* lbf_maintain_tx_refcnt
 *
 * set the rx reference count.
 *
 * Arguments:
 * count
 *
 * Return Value:
 * void
 */

static inline void lbf_maintain_tx_refcnt(signed int count)
{
	spin_lock(&intel_lbf_lpm.txref_lock);
	intel_lbf_lpm.tx_refcount = intel_lbf_lpm.tx_refcount + count;
	spin_unlock(&intel_lbf_lpm.txref_lock);
	pr_debug("%s:Tx_ref_cnt: %d\n", __func__, intel_lbf_lpm.tx_refcount);

}

/* lbf_maintain_rx_refcnt
 *
 * set the tx reference count.
 *
 * Arguments:
 * count
 *
 * Return Value:
 * void
 */

static inline void lbf_maintain_rx_refcnt(signed int count)
{
	spin_lock(&intel_lbf_lpm.rxref_lock);
	intel_lbf_lpm.rx_refcount = intel_lbf_lpm.rx_refcount + count;
	spin_unlock(&intel_lbf_lpm.rxref_lock);
	pr_debug("%s:rx_ref_cnt: %d\n", __func__, intel_lbf_lpm.rx_refcount);

}

/* lbf_get_host_wake_state
 *
 * get the host wake state.
 *
 * Arguments:
 * void
 *
 * Return Value:
 * host_wake state
 */

static inline int lbf_get_host_wake_state(void)
{
	int host_wake;
	spin_lock(&intel_lbf_lpm.host_wake_lock);
	host_wake = intel_lbf_lpm.host_wake;
	spin_unlock(&intel_lbf_lpm.host_wake_lock);
	pr_debug("%s:host_wake: %d\n", __func__, host_wake);

	return host_wake;
}



static void lbf_hostwake_low_processing(void)
{
	pr_debug("->%s\n", __func__);

	if (lbf_get_device_state() == D2_TO_D0)
		wait_dx_exit();
	if (lbf_get_device_state() == D0)
		lbf_tx_wakeup(NULL);

	pr_info("<-%s\n", __func__);
}


static void lbf_hostwake_high_processing(void)
{
	lbf_tty_write((void *) lbf_tx->tty, hci_controlerwake_req_rsp,
		WAKEUP_RSP_SIZE);

	switch (lbf_get_device_state()) {
	case D0_TO_D2:
		lbf_set_ackign_st(true);
		intel_lbf_lpm.hostwake |= HOSTWAKE_D0_TO_D2;
		break;
	case D2_TO_D0:
		lbf_set_wakeupign_st(true);
		intel_lbf_lpm.hostwake |= HOSTWAKE_D2_TO_D0;
		break;
	default:
		intel_lbf_lpm.hostwake |= HOSTWAKE_D2;
		break;
	}

	lbf_set_device_state(D0);

	if (!lbf_get_tx_ref_count()) {
		lbf_maintain_tx_refcnt(1);
		pm_runtime_get(intel_lbf_lpm.tty_dev);
	}

	wake_up_interruptible(&waitFord0exit);
	wake_up_interruptible(&waitFordxexit);
	pr_info("<-%s\n", __func__);
}


static void lbf_hostwake_update_work(struct work_struct *work)
{
	lbf_hostwake_low_processing();
}

/* lbf_update_host_wake
 *
 * update the host_wake_locked
 *
 * Arguments:
 * host_wake state
 *
 * Return Value:
 * void
 */

static void lbf_update_host_wake(int host_wake)
{

	if (host_wake == lbf_get_host_wake_state() &&
					host_wake == IDLE)
		return;

	if (host_wake == lbf_get_host_wake_state() &&
				lbf_get_device_state() == D0)
		return;

	lbf_set_host_wake_state(host_wake);
	intel_lbf_lpm.hostwake = 0;

	if (host_wake) {
		intel_lbf_lpm.hostwake |= HOSTWAKE_HIGH;
		spin_lock(&intel_lbf_lpm.interrupt_lock);
		lbf_hostwake_high_processing();
		spin_unlock(&intel_lbf_lpm.interrupt_lock);

	} else {
		intel_lbf_lpm.hostwake |= HOSTWAKE_LOW;
		switch (lbf_get_device_state()) {
		case D2:
			intel_lbf_lpm.hostwake |= HOSTWAKE_D2;
			break;
		case D0_TO_D2:
			intel_lbf_lpm.hostwake |= HOSTWAKE_D0_TO_D2;
			pr_debug("Ignore tx idle notification\n");
			break;
		case D0:
			intel_lbf_lpm.hostwake |= HOSTWAKE_D0;
		case D2_TO_D0:
			intel_lbf_lpm.hostwake |= HOSTWAKE_D2_TO_D0;
			schedule_work(&lbf_tx->hostwake_update_work);
			break;
		default:
			break;
		}

	}
	pr_info("<-%s ht_wk:%d\n", __func__, intel_lbf_lpm.hostwake);
}

/* irqreturn_t host_wake_isr
 *
 * callback called when irq is received
 *
 * Arguments:
 *
 * Return Value:
 * irqreturn_t
 */

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;

	host_wake = gpio_get_value(intel_lbf_lpm.gpio_host_wake);

	host_wake = host_wake ? 0 : 1;

	if (!intel_lbf_lpm.tty_dev) {
		lbf_set_host_wake_state(host_wake);
		return IRQ_HANDLED;
	}

	if (host_wake) {
		if (!lbf_get_rx_ref_count()) {
			lbf_maintain_rx_refcnt(1);
			pm_runtime_get(intel_lbf_lpm.tty_dev);
		}
		lbf_update_host_wake(host_wake);
	}

	return IRQ_HANDLED;
}


/* activate_irq_handler
 *
 * Activaes the irq and registers the irq handler
 *
 * Arguments:
 * void
 * Return Value:
 * void
 */

static void activate_irq_handler(void)
{
	int ret;
	int irqf = IRQF_TRIGGER_RISING | IRQF_NO_SUSPEND;
	pr_info("%s\n", __func__);
	ret = devm_request_threaded_irq(&intel_lbf_lpm.pdev->dev,
					intel_lbf_lpm.int_host_wake,
					NULL,
					host_wake_isr,
					irqf, "host_wake",
					NULL);

	if (ret < 0) {
		pr_err("Error lpm request IRQ");
		gpio_free(intel_lbf_lpm.gpio_host_wake);
	}
}

/* check_unthrottle
 *
 * Unthrottle the tty buffer
 *
 * Arguments:
 * tty structure
 * Return Value:
 * void
 */

static inline void check_unthrottle(struct tty_struct *tty)
{
	if (tty->count)
		tty_unthrottle(tty);
}

/* send_d0_dx_packet
 *
 * send Dx request packet
 *
 * Arguments:
 * void
 * Return Value:
 * status of Dx Request
 */

static int send_d0_dx_packet(void)
{
	int len;
	int ret;
	pr_debug("-> %s\n", __func__);


	if (lbf_tx->tx_skb == NULL && !(skb_queue_len(&lbf_tx->txq)) &&
		!lbf_get_bt_fmr_state() && !lbf_get_host_wake_state() &&
			(lbf_get_device_state() == D0)) {

		lbf_set_device_state(D0_TO_D2);
		disable_irq_nosync(intel_lbf_lpm.int_host_wake);
		len = lbf_tty_write((void *) lbf_tx->tty, hci_dx_request,
			DX_HCI_CMD_SIZE);
		pr_info("BT CORE SEND D2 REQ len:%d\n", len);
		if (len != DX_HCI_CMD_SIZE) {
			ret = enqueue_dx_packet(&hci_dx_request[len],
					(DX_HCI_CMD_SIZE - len));
			intel_lbf_lpm.dx_packet_pending =
				(DX_HCI_CMD_SIZE - len);
		}
	}
	pr_debug("<- %s\n", __func__);
	return ret;
}

/* reset_buffer_flags
 *
 * Resets the st driver's buffer
 *
 * Arguments:
 * void
 * Return Value:
 * void
 */

static void reset_buffer_flags(struct tty_struct *tty)
{
	struct lbf_uart *lbf_ldisc;
	pr_debug("-> %s\n", __func__);
	lbf_ldisc = tty->disc_data;
	spin_lock(&lbf_ldisc->tx_lock);
	lbf_ldisc->read_head = 0;
	lbf_ldisc->read_cnt = 0;
	lbf_ldisc->read_tail = 0;
	spin_unlock(&lbf_ldisc->tx_lock);
	tty->receive_room = RECEIVE_ROOM;
	lbf_update_set_room(tty, 0);
	check_unthrottle(tty);

}

/* dx_pending_packet
 *
 * Handles if Dx packet couldnot be sent in one go
 *
 * Arguments:
 * len
 * Return Value:
 * void
 */

static void dx_pending_packet(int len)
{
	pr_debug("-> %s\n", __func__);
	if (intel_lbf_lpm.dx_packet_pending == len) {
		intel_lbf_lpm.dx_packet_pending = 0;
		lbf_set_device_state(D0_TO_D2);
	} else
		intel_lbf_lpm.dx_packet_pending =
			intel_lbf_lpm.dx_packet_pending - len;

}

/* d0_pending_packet
 *
 * Handles if D0 packet couldnot be sent in one go
 *
 * Arguments:
 * len
 * Return Value:
 * void
 */

static void d0_pending_packet(int len)
{
	pr_debug("-> %s\n", __func__);
	if (intel_lbf_lpm.d0_packet_pending == len) {
		intel_lbf_lpm.d0_packet_pending = 0;
		lbf_set_device_state(D2_TO_D0);
	} else
		intel_lbf_lpm.d0_packet_pending =
			intel_lbf_lpm.d0_packet_pending - len;

}

/* pending_dx_packet
 *
 * checks for Dx pending packet and update the dx pending packet
 *
 * Arguments:
 * len
 * Return Value:
 * void
 */

static inline void pending_dx_packet(int len)
{
	pr_debug("-> %s\n", __func__);

	if (intel_lbf_lpm.dx_packet_pending)
		dx_pending_packet(len);
	if (intel_lbf_lpm.d0_packet_pending)
		d0_pending_packet(len);
}

/* lbf_tx_wakeup
 *
 * wakes up and does Tx operation
 *
 * Arguments:
 * Driver's internal structure
 * Return Value:
 * void
 */

static void lbf_tx_wakeup(struct lbf_uart *lbf_uart)
{
	struct sk_buff *skb;
	int len = 0;
	pr_debug("-> %s\n", __func__);

	mutex_lock(&lbf_tx->tx_wakeup);
	if (lbf_tx->tbusy) {
		lbf_tx->woke_up = 1;
		mutex_unlock(&lbf_tx->tx_wakeup);
		return;
	}
	lbf_tx->tbusy = 1; /* tx write is busy.*/

check_again:
	lbf_tx->woke_up = 0;
	mutex_unlock(&lbf_tx->tx_wakeup);


	while ((skb = lbf_dequeue())) {

		if (lbf_tx->tty) {
			set_bit(TTY_DO_WRITE_WAKEUP, &lbf_tx->tty->flags);
			len = lbf_tty_write((void *) lbf_tx->tty, skb->data,
					skb->len);
			if (intel_lbf_lpm.lpm_enable == ENABLE)
				pending_dx_packet(len);
			skb_pull(skb, len);

			if (skb->len) {
				lbf_tx->tx_skb = skb;
				pr_debug("-> %s added to pending buffer:%d\n",
					__func__, skb->len);
				break;
			}
			kfree_skb(skb);
		}
	}

	mutex_lock(&lbf_tx->tx_wakeup);
	if (lbf_tx->woke_up)
		goto check_again;

	if (intel_lbf_lpm.lpm_enable == ENABLE)
		send_d0_dx_packet();
	lbf_tx->tbusy = 0; /* Done with Tx.*/
	mutex_unlock(&lbf_tx->tx_wakeup);


	pr_debug("<- %s\n", __func__);

}

static void lbf_tx_wakeup_work(struct work_struct *work)
{
	lbf_tx_wakeup(NULL);
}


/* This is the internal write function - a wrapper
 * to tty->ops->write
 */
static int lbf_tty_write(struct tty_struct *tty, const unsigned char *data,
			int count)
{
	struct lbf_uart *lbf_uart;
	lbf_uart = tty->disc_data;
	return lbf_uart->tty->ops->write(tty, data, count);
}

static long lbf_write(struct sk_buff *skb)
{
	long len = 0;
	int ret = 0;
	pr_info("-> %s\n", __func__);

	print_hex_dump_debug("<FMR out<", DUMP_PREFIX_NONE, 16, 1, skb->data,
			skb->len, 0);

	if (intel_lbf_lpm.lpm_enable == ENABLE)
		ret = lpm_tx_update(MODULE_FM);
	/*ST to decide where to enqueue the skb */
	if (!ret) {

		lbf_enqueue(skb);
		/* wake up */
		len = skb->len;
		lbf_tx_wakeup(NULL);
	}

	pr_debug("<- %s len: %ld\n", __func__, len);

	/* return number of bytes written */
	return len;
}

/* for protocols making use of shared transport */

long unregister_fmdrv_from_ld_driv(struct fm_ld_drv_register *fm_ld_drv_reg)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&lbf_tx->lock, flags);
	fw_isregister = 0;
	spin_unlock_irqrestore(&lbf_tx->lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(unregister_fmdrv_from_ld_driv);

long register_fmdrv_to_ld_driv(struct fm_ld_drv_register *fm_ld_drv_reg)
{
	unsigned long flags;
	long err = 0;
	fm = NULL;
	pr_debug("-> %s\n", __func__);

	if (lbf_ldisc_running == 1) {

		if (fm_ld_drv_reg == NULL || fm_ld_drv_reg->fm_cmd_handler
				== NULL
				|| fm_ld_drv_reg->ld_drv_reg_complete_cb
						== NULL) {
			pr_debug("fmr func handler not initialised");
			return -EINVAL;
		}
		spin_lock_irqsave(&lbf_tx->lock, flags);
		fm_ld_drv_reg->fm_cmd_write = lbf_write;
		fm = fm_ld_drv_reg;
		fw_isregister = 1;
		spin_unlock_irqrestore(&lbf_tx->lock, flags);

		if (fm) {
			if (likely(fm->ld_drv_reg_complete_cb != NULL)) {
				fm->ld_drv_reg_complete_cb(fm->priv_data,
						0);
				pr_debug("Registration called");

			}
		}
	} else
		err = -EINVAL;

	return err;
}
EXPORT_SYMBOL_GPL(register_fmdrv_to_ld_driv);

static void lbf_ldisc_flush_buffer(struct tty_struct *tty)
{
	/* clear everything and unthrottle the driver */

	unsigned long flags;
	pr_debug("-> %s\n", __func__);
	reset_buffer_flags(tty);

	if (!tty->link)
		return;

	if (tty->link) {
		spin_lock_irqsave(&tty->ctrl_lock, flags);
		if (tty->link->packet) {
			tty->ctrl_status |= TIOCPKT_FLUSHREAD;
			wake_up_interruptible(&tty->link->read_wait);
		}
		spin_unlock_irqrestore(&tty->ctrl_lock, flags);
	}
	pr_debug("<- %s\n", __func__);
}

/**
 * lbf_dequeue - internal de-Q function.
 * If the previous data set was not written
 * completely, return that skb which has the pending data.
 * In normal cases, return top of txq.
 */
struct sk_buff *lbf_dequeue(void)
{
	struct sk_buff *returning_skb;

	if (lbf_tx->tx_skb != NULL) {
		returning_skb = lbf_tx->tx_skb;
		lbf_tx->tx_skb = NULL;
		return returning_skb;
	}
	return skb_dequeue(&lbf_tx->txq);
}

/**
 * lbf_enqueue - internal Q-ing function.
 * Will either Q the skb to txq or the tx_waitq
 * depending on the ST LL state.
 * txq needs protection since the other contexts
 * may be sending data, waking up chip.
 */
void lbf_enqueue(struct sk_buff *skb)
{
	mutex_lock(&lbf_tx->writelock);
	skb_queue_tail(&lbf_tx->txq, skb);
	mutex_unlock(&lbf_tx->writelock);
}

/* lbf_ldisc_open
 *
 * Called when line discipline changed to HCI_UART.
 *
 * Arguments:
 * tty pointer to tty info structure
 * Return Value:
 * 0 if success, otherwise error code
 */
static int lbf_ldisc_open(struct tty_struct *tty)
{

	struct lbf_uart *lbf_uart;
	pr_debug("-> %s\n", __func__);
	if (tty->disc_data) {
		pr_debug("%s ldiscdata exist\n ", __func__);
		return -EEXIST;
	}
	/* Error if the tty has no write op instead of leaving an exploitable
	 hole */
	if (tty->ops->write == NULL) {
		pr_debug("%s write = NULL\n ", __func__);
		return -EOPNOTSUPP;
	}

	lbf_uart = kzalloc(sizeof(struct lbf_uart), GFP_KERNEL);
	if (!lbf_uart) {
		pr_debug(" kzalloc for lbf_uart failed\n ");
		tty_unregister_ldisc(N_INTEL_LDISC);
		return -ENOMEM;
	}

	lbf_tx = kzalloc(sizeof(struct lbf_q_tx), GFP_KERNEL);
	if (!lbf_tx) {
		pr_debug(" kzalloc for lbf_tx failed\n ");
		kfree(lbf_uart);

		tty_unregister_ldisc(N_INTEL_LDISC);
		return -ENOMEM;
	}

	tty->disc_data = lbf_uart;
	lbf_uart->tty = tty;
	lbf_tx->tty = tty;
	skb_queue_head_init(&lbf_tx->txq);
	skb_queue_head_init(&lbf_tx->tx_waitq);
	/* don't do an wakeup for now */
	clear_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	if (likely(!lbf_uart->read_buf)) {
		lbf_uart->read_buf
				= kzalloc(N_INTEL_LDISC_BUF_SIZE, GFP_KERNEL);

		if (!lbf_uart->read_buf) {
			kfree(lbf_uart);

			kfree(lbf_tx);

			tty_unregister_ldisc(N_INTEL_LDISC);
			return -ENOMEM;
		}
	}

	tty->receive_room = RECEIVE_ROOM;
	spin_lock_init(&lbf_uart->rx_lock);
	spin_lock_init(&lbf_tx->lock);
	spin_lock_init(&lbf_uart->tx_lock);
	spin_lock_init(&lbf_uart->tx_update_lock);
	mutex_init(&lbf_tx->writelock);
	mutex_init(&lbf_tx->fwdownloadlock);
	mutex_init(&lbf_tx->tx_wakeup);
	mutex_init(&intel_lbf_lpm.lpmenable);
	spin_lock_init(&intel_lbf_lpm.lpm_modulestate);
	mutex_init(&lbf_uart->atomic_read_lock);

	INIT_WORK(&lbf_tx->tx_wakeup_work, lbf_tx_wakeup_work);
	INIT_WORK(&lbf_tx->hostwake_update_work, lbf_hostwake_update_work);
	INIT_WORK(&lbf_tx->host_enable_work, lbf_host_enable_work);

	init_waitqueue_head(&lbf_uart->read_wait);

	lbf_ldisc_running = 1;
	intel_lbf_lpm.device_state = D0;
	fw_download_state = 0;

	/* Flush any pending characters in the driver and line discipline. */
	reset_buffer_flags(tty);

	if (tty->ldisc->ops->flush_buffer)
		tty->ldisc->ops->flush_buffer(tty);
	tty_driver_flush_buffer(tty);
	intel_lbf_lpm.lpm_enable = DISABLE;
	lbf_serial_get();
	intel_lbf_lpm.rx_refcount = 1;
	lbf_serial_get();
	intel_lbf_lpm.tx_refcount = 1;

	pr_info("<- %s\n", __func__);

	return 0;
}

/* lbf_ldisc_close()
 *
 * Called when the line discipline is changed to something
 * else, the tty is closed, or the tty detects a hangup.
 */

static void lbf_ldisc_close(struct tty_struct *tty)
{
	struct lbf_uart *lbf_uart = (void *) tty->disc_data;
	tty->disc_data = NULL; /* Detach from the tty */

	pr_info("-> %s\n", __func__);

	if (intel_lbf_lpm.rx_refcount > 0) {
		lbf_serial_put();
		intel_lbf_lpm.rx_refcount = 0;
	}
	if (intel_lbf_lpm.tx_refcount > 0) {
		lbf_serial_put();
		intel_lbf_lpm.tx_refcount = 0;
	}

	if (intel_lbf_lpm.lpm_enable == ENABLE &&
		intel_lbf_lpm.module_refcount > 0) {
		intel_lbf_lpm.module_refcount = 0;
		lbf_ldisc_lpm_enable_cleanup();
		intel_lbf_lpm.lpm_enable = DISABLE;
	}

	flush_work(&lbf_tx->tx_wakeup_work);
	flush_work(&lbf_tx->hostwake_update_work);
	flush_work(&lbf_tx->host_enable_work);
	skb_queue_purge(&lbf_tx->txq);
	skb_queue_purge(&lbf_tx->tx_waitq);
	lbf_uart->rx_count = 0;
	lbf_uart->rx_state = LBF_W4_H4_HDR;

	kfree_skb(lbf_uart->rx_skb);
	kfree(lbf_uart->read_buf);
	lbf_uart->rx_skb = NULL;
	lbf_ldisc_running = -1;
	bt_rfkill_set_power(DISABLE);

	kfree(lbf_tx);
	kfree(lbf_uart);
}


/* lbf_ldisc_wakeup()
 *
 * Callback for transmit wakeup. Called when low level
 * device driver can accept more send data.
 *
 * Arguments: tty pointer to associated tty instance data
 * Return Value: None
 */
static void lbf_ldisc_wakeup(struct tty_struct *tty)
{

	struct lbf_uart *lbf_uart = (void *) tty->disc_data;
	pr_debug("-> %s\n", __func__);

	clear_bit(TTY_DO_WRITE_WAKEUP, &lbf_uart->tty->flags);

	schedule_work(&lbf_tx->tx_wakeup_work);

	pr_debug("<- %s\n", __func__);


}

/* is_valid_proto()
 * Note the HCI Header type
 * Arguments : type: the 1st byte in the packet
 * Return Type: The Corresponding type in the array defined
 * for all headers
 */

static inline int is_valid_proto(int type)
{
	return (type >= 2 && type <= 6) ? type : INVALID;
}

/* st_send_frame()
 * push the skb received to relevant
 * protocol stacks
 * Arguments : tty pointer to associated tty instance data
 * lbf_uart : Disc data for tty
 * Return Type: int
 */
static int st_send_frame(struct tty_struct *tty, struct lbf_uart *lbf_uart)
{
	unsigned int i = 0;
	int ret = 0;
	int chnl_id = MODULE_BT;
	unsigned char *buff;
	unsigned int opcode = 0;
	unsigned int count = 0;

	if (unlikely(lbf_uart == NULL || lbf_uart->rx_skb == NULL)) {
		pr_debug(" No channel registered, no data to send?");
		return ret;
	}

	buff = &lbf_uart->rx_skb->data[0];
	if (intel_lbf_lpm.lpm_enable == ENABLE) {
		if (lbf_uart->rx_skb->data[0] == LPM_PKT) {
			if (lbf_uart->rx_skb->data[1] == TX_HOST_NOTIFICATION) {
				if (lbf_uart->rx_skb->data[3] == IDLE) {
					lbf_update_host_wake(IDLE);
					pr_info("Tx Idle Notification\n");
				} else {
					lbf_set_host_wake_state(ACTIVE);
					pr_info("Tx Active Notification\n");
				}
				lbf_update_set_room(tty, DX_HCI_CMD_SIZE);
				goto lpm_packet;
			}
		}
	}

	count = lbf_uart->rx_skb->len;
	STREAM_TO_UINT16(opcode, buff);
	pr_debug("opcode : 0x%x event code: 0x%x registered", opcode,
			 lbf_uart->rx_skb->data[1]);
	/*if (lbf_uart->rx_skb->data[0] == LPM_PKT)
		for (i = lbf_uart->rx_skb->len, j = 0; i > 0; i--, j++)
			 pr_err("%d:0x%x", j, lbf_uart->rx_skb->data[j]);*/

	if (HCI_COMMAND_COMPLETE_EVENT == lbf_uart->rx_skb->data[1]) {
		switch (opcode) {
		case FMR_IRQ_CONFIRM:
		case FMR_SET_POWER:
		case FMR_READ:
		case FMR_WRITE:
		case FMR_SET_AUDIO:
		case FMR_TOP_READ:
		case FMR_TOP_WRITE:
			chnl_id = MODULE_FM;
			pr_debug("Its FM event\n");
			break;
		default:
			chnl_id = MODULE_BT;
			pr_debug("Its BT event\n");
			break;
		}
	}
	if (HCI_INTERRUPT_EVENT == lbf_uart->rx_skb->data[1]
			&& (lbf_uart->rx_skb->data[3] == FMR_DEBUG_EVENT))
		chnl_id = MODULE_FM;

	if (chnl_id == MODULE_FM) {
		if (fm) {
			if (fm->fm_cmd_handler != NULL) {
				if (fm->fm_cmd_handler(fm->
					priv_data, lbf_uart->rx_skb) != 0)
					pr_debug("proto stack %d recv failed"
						, chnl_id);
				else
					ret = lbf_uart->rx_skb->len;
			}
		}
	else
		pr_debug("fm is NULL\n");
	} else if (chnl_id == MODULE_BT) {
		pr_debug(" Added in buffer count %d rdhd %d rdcnt: %d\n"
			, count, lbf_uart->read_head, lbf_uart->read_cnt);
		spin_lock(&lbf_uart->tx_lock);
		i = min3((N_INTEL_LDISC_BUF_SIZE - lbf_uart->read_cnt),
				N_INTEL_LDISC_BUF_SIZE - lbf_uart->read_head,
				(int)lbf_uart->rx_skb->len);
		spin_unlock(&lbf_uart->tx_lock);
		memcpy(lbf_uart->read_buf + lbf_uart->read_head,
			 &lbf_uart->rx_skb->data[0], i);
		spin_lock(&lbf_uart->tx_lock);
		lbf_uart->read_head =
			(lbf_uart->read_head + i) & (N_INTEL_LDISC_BUF_SIZE-1);
		lbf_uart->read_cnt += i;
		spin_unlock(&lbf_uart->tx_lock);
		count = count - i;

		if (unlikely(count)) {
			pr_debug(" Added in buffer inside count %d readhead %d\n"
				, count , lbf_uart->read_head);
			spin_lock(&lbf_uart->tx_lock);
			i = min3((N_INTEL_LDISC_BUF_SIZE - lbf_uart->read_cnt),
				(N_INTEL_LDISC_BUF_SIZE - lbf_uart->read_head),
				(int)count);
			spin_unlock(&lbf_uart->tx_lock);
			memcpy(lbf_uart->read_buf + lbf_uart->read_head,
			&lbf_uart->rx_skb->data[lbf_uart->rx_skb->len - count],
				i);
			spin_lock(&lbf_uart->tx_lock);
			lbf_uart->read_head =
			(lbf_uart->read_head + i) & (N_INTEL_LDISC_BUF_SIZE-1);
			lbf_uart->read_cnt += i;
			spin_unlock(&lbf_uart->tx_lock);
		}
	}

lpm_packet:

	if (lbf_uart->rx_skb) {
		kfree_skb(lbf_uart->rx_skb);
		lbf_uart->rx_skb = NULL;
	}
	lbf_uart->rx_state = LBF_W4_H4_HDR;
	lbf_uart->rx_count = 0;
	lbf_uart->rx_chnl = 0;
	lbf_uart->bytes_pending = 0;

	return ret;
}

/* lbf_ldisc_fw_download_init()
 * lock the Mutex to block the IOCTL on 2nd call
 * Return Type: void
 */
static int lbf_ldisc_fw_download_init(void)
{
	unsigned long ret = -1;
	pr_debug("->B%s\n", __func__);
	if (mutex_lock_interruptible(&lbf_tx->fwdownloadlock))
		return -ERESTARTSYS;
	pr_debug("->A%s fw_download_state: %d\n", __func__ , fw_download_state);
	if (bt_enable_state == DISABLE) {
		bt_rfkill_set_power(ENABLE);
		ret = DO_FW_DL;
	} else {
		if (fw_download_state == FW_FAILED)
			ret = FW_FAILED;
		else if (fw_download_state == FW_SUCCESS)
			ret = DO_STACK_INIT;
	}
	pr_debug("<- %s\n", __func__);
	return ret;
}

/* wait_dx_exit()
 * wait until Dx to D0 transition is completed
 * Return Type: status
 * Dx wake status
 */

static int wait_dx_exit(void)
{
	int ret = 0;
	DEFINE_WAIT(dxwait);

	pr_info("-> %s\n", __func__);

	while (lbf_get_device_state() == D2_TO_D0) {
		prepare_to_wait(&waitFordxexit, &dxwait, TASK_INTERRUPTIBLE);

		if (lbf_get_device_state() == D2_TO_D0)
			ret = schedule_timeout(2*HZ);

		finish_wait(&waitFordxexit, &dxwait);
		pr_debug("->finish_wait%s\n", __func__);

		if (ret == 0 && lbf_get_device_state() == D2_TO_D0) {
			ret = FAILED;
			lbf_set_device_state(D2);
			break;
		} else
			ret = 0;

		if (signal_pending(current))
			return -ERESTARTSYS;


	}

	pr_info("<- ret: %d %s\n", ret, __func__);
	return ret;
}

/* wait_d0_exit()
 * wait until D0 to Dx transition is completed
 * Return Type: status
 * D0 exit status
 */

static int wait_d0_exit(void)
{
	int ret = 0;
	pr_info("-> %s\n", __func__);

	while (lbf_get_device_state() == D0_TO_D2) {

		DEFINE_WAIT(d0wait);

		prepare_to_wait(&waitFord0exit, &d0wait, TASK_INTERRUPTIBLE);

		if (lbf_get_device_state() == D0_TO_D2) {
			ret = schedule_timeout(2*HZ);
			/* the remaining time in jiffies will be returned,
			 or 0 if the timer expired in time*/
			pr_debug("%s\n", __func__);
		}

		finish_wait(&waitFord0exit, &d0wait);
		pr_debug("->finish%s\n", __func__);

		if (ret == 0 && lbf_get_device_state() == D0_TO_D2) {
			ret = FAILED;
			lbf_set_device_state(D0);
			break;
		} else
			ret = 0;
		if (signal_pending(current))
			return -ERESTARTSYS;

	}

	pr_info("<- %s ret: %d\n", __func__, ret);
	return ret;
}

/* enqueue_dx_packet()
 * Enqueue the Dx packet
 * Return Type: status
 * enqueue status
 */

static int enqueue_dx_packet(uint8_t *hci_cmd, int size)
{
	struct sk_buff *skb;
	pr_debug("--> send_dx_packet: SKIP");

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	memcpy(skb_put(skb, size), hci_cmd, size);
	lbf_enqueue(skb);
	return 0;
}
/* send_d0_packet()
 * Send the D0 packet
 * Return Type: status
 * sending the d0 packet sending status
 */

static int send_d0_packet(void)
{
	int len = 0;
	int dev_st;
	pr_debug("-> %s\n", __func__);
	dev_st = lbf_get_device_state();
	if (dev_st == D0 || lbf_get_host_wake_state() == HIGH)
		return IGNORE;
	lbf_set_device_state(D2_TO_D0);
	len = lbf_tty_write((void *) lbf_tx->tty, hci_dx_wake,
			D0_HCI_CMD_SIZE);
	pr_info("BT CORE SEND D0 REQ len:%d\n", len);
	if (len != D0_HCI_CMD_SIZE) {
		lbf_set_device_state(dev_st);
		len = enqueue_dx_packet(&hci_dx_wake[len],
				(D0_HCI_CMD_SIZE - len));
		intel_lbf_lpm.d0_packet_pending = (D0_HCI_CMD_SIZE - len);
	} else
		len = 0;

	pr_debug("-> %s, len:%d\n", __func__, len);
	return len;
}

/* lpm_tx_update()
 * update the device and module states on tx  being received
 * Return Type: status
 * D0 packet sending status
 */

static int lpm_tx_update(int module)
{
	struct lbf_uart *lbf_ldisc;
	int ret = 0;
	pr_debug("-> %s\n", __func__);
	return ret;
	/* Disabling LPM from driver side*/
	lbf_ldisc = (struct lbf_uart *) lbf_tx->tty->disc_data;

	mutex_lock(&intel_lbf_lpm.lpmtxlock);

check_refcount:
	switch (lbf_get_device_state()) {
	case D2:
	case D3:
		if (!lbf_get_tx_ref_count()) {
			lbf_maintain_tx_refcnt(1);
			lbf_serial_get();
		}
		disable_irq_nosync(intel_lbf_lpm.int_host_wake);
		ret = send_d0_packet();
		enable_irq(intel_lbf_lpm.int_host_wake);
		if (ret != IGNORE)
			ret = wait_dx_exit();
		else
			ret = 0;
		break;
	case D0_TO_D2:
		ret = wait_d0_exit();
		if (ret == 0)
			goto check_refcount;
		break;
	case D0:
		break;
	default:
		break;
	}


	if (ret == 0) {
		spin_lock(&intel_lbf_lpm.lpm_modulestate);
		switch (module) {
		case  MODULE_FM:
			intel_lbf_lpm.fm_module_state = ACTIVE;
			intel_lbf_lpm.bt_fmr_state = ACTIVE;
			break;
		case MODULE_BT:
			intel_lbf_lpm.bt_module_state = ACTIVE;
			intel_lbf_lpm.bt_fmr_state = ACTIVE;
			break;
		default:
			break;
		}

		pr_info("->BT_FMR st:%d\n", intel_lbf_lpm.bt_fmr_state);
		spin_unlock(&intel_lbf_lpm.lpm_modulestate);
	}

	mutex_unlock(&intel_lbf_lpm.lpmtxlock);
	pr_debug("<-%s ret:%d\n", __func__, ret);
	return ret;

}

/* lbf_ldisc_lpm_enable_cleanup()
 * Reset all the device and module states
 * Return Type: void
 */

static void lbf_ldisc_lpm_enable_cleanup(void)
{
	pr_debug("-> %s\n", __func__);

	if (lbf_get_rx_ref_count()) {
		lbf_serial_put();
		lbf_maintain_rx_refcnt(-1);
	}
	if (lbf_get_tx_ref_count()) {
		lbf_serial_put();
		lbf_maintain_tx_refcnt(-1);
	}

	if (intel_lbf_lpm.gpio_host_wake > 0) {
		free_irq(intel_lbf_lpm.int_host_wake, NULL);
		gpio_free(intel_lbf_lpm.gpio_host_wake);
		pr_debug("<- Free the  %s\n", __func__);
	}

	pr_debug("<- %s\n", __func__);

}

/* lbf_ldisc_lpm_enable_init()
 * Initializzes all the device and module states
 * Return Type: void
 */

static void lbf_ldisc_lpm_enable_init(void)
{
	pr_debug("-> %s\n", __func__);

	if (!lbf_get_rx_ref_count()) {
		lbf_serial_get();
		lbf_maintain_rx_refcnt(1);
	}
	if (!lbf_get_tx_ref_count()) {
		lbf_maintain_tx_refcnt(1);
		lbf_serial_get();
	}
	lbf_set_device_state(D0);

	intel_lbf_lpm.host_wake = HIGH;
	intel_lbf_lpm.bt_module_state = ACTIVE;
	intel_lbf_lpm.fm_module_state = ACTIVE;
	intel_lbf_lpm.bt_fmr_state = ACTIVE;

	pr_debug("<- %s\n", __func__);
}


/* lbf_ldisc_lpm_enable()
 * IOCTL to enable LPM in st driver
 * Return Type: int
 * status of LPM initialization
 */

static int lbf_ldisc_lpm_enable(unsigned long arg)
{
	int ret = -EINVAL;
	pr_debug("-> %s\n", __func__);

	mutex_lock(&intel_lbf_lpm.lpmenable);

	if (arg == ENABLE) {
		++intel_lbf_lpm.module_refcount;
		pr_info("-> %s module refcount: %d\n", __func__,
				intel_lbf_lpm.module_refcount);
		if (arg == intel_lbf_lpm.lpm_enable) {
			mutex_unlock(&intel_lbf_lpm.lpmenable);
			return 0;
		}

		spin_lock_init(&intel_lbf_lpm.devicest_lock);
		spin_lock_init(&intel_lbf_lpm.host_wake_lock);
		spin_lock_init(&intel_lbf_lpm.ackrsp_lock);
		spin_lock_init(&intel_lbf_lpm.wakeuprsp_lock);
		mutex_init(&intel_lbf_lpm.lpmtxlock);
		mutex_init(&intel_lbf_lpm.idleupdatelock);
		spin_lock_init(&intel_lbf_lpm.rxref_lock);
		spin_lock_init(&intel_lbf_lpm.interrupt_lock);
		spin_lock_init(&intel_lbf_lpm.txref_lock);

		pr_info("%s: gpio_enable=%d\n", __func__,
				desc_to_gpio(intel_lbf_lpm.gpio_enable_bt));
		intel_lbf_lpm.lpm_enable = ENABLE;
		activate_irq_handler();
		pr_info("LPM enabled success\n");
	} else {

		--intel_lbf_lpm.module_refcount;
		pr_info("-> %s module refcount: %d\n", __func__,
						intel_lbf_lpm.module_refcount);
		if (!intel_lbf_lpm.module_refcount) {
			lbf_ldisc_lpm_enable_cleanup();
			intel_lbf_lpm.lpm_enable = DISABLE;
		}
	}

	mutex_unlock(&intel_lbf_lpm.lpmenable);
	pr_debug("<- %s\n", __func__);

	return ret;
}

/* lbf_ldisc_lpm_idle()
 * IOCTL to send module IDLE State from userspace
 * Return Type: void
 */

static int lbf_ldisc_lpm_idle(unsigned long arg)
{
	int ret = INVALID;
	return 0;
	/*Disabling LPM from Driver side for cht PO*/
	pr_debug("-> %s\n", __func__);
	mutex_lock(&intel_lbf_lpm.idleupdatelock);
	spin_lock(&intel_lbf_lpm.lpm_modulestate);

	switch (arg) {
	case  MODULE_FM:
		intel_lbf_lpm.fm_module_state = IDLE;
		break;
	case MODULE_BT:
		intel_lbf_lpm.bt_module_state = IDLE;
		break;
	default:
		break;
	}

	if (!intel_lbf_lpm.bt_module_state && !intel_lbf_lpm.fm_module_state) {
		intel_lbf_lpm.bt_fmr_state = IDLE;
		pr_info("-> %sBoth BT & FMR Idle\n", __func__);
	} else
		intel_lbf_lpm.bt_fmr_state = ACTIVE;

	spin_unlock(&intel_lbf_lpm.lpm_modulestate);

try_again:
	if (lbf_get_device_state() == D0) {
		lbf_tx_wakeup(NULL);
		pr_debug("-> %s Try to send idle\n", __func__);
	} else if (lbf_get_device_state() == D2_TO_D0) {
		pr_info("-> %s wait until D0\n", __func__);
		ret = wait_dx_exit();
		goto try_again;
	}

	mutex_unlock(&intel_lbf_lpm.idleupdatelock);
	pr_debug("<- %s\n", __func__);
	return ret;
}


/* lbf_ldisc_fw_download_complete()
 * unlock the Mutex to unblock the IOCTL on 2nd call
 * Return Type: void
 */
static void lbf_ldisc_fw_download_complete(unsigned long arg)
{
	if (arg == FW_SUCCESS)
		fw_download_state = FW_SUCCESS;
	else if (arg == FW_FAILED) {
		fw_download_state = FW_FAILED;
		bt_rfkill_set_power(DISABLE);
		bt_rfkill_set_power(ENABLE);
	}

	intel_lbf_lpm.bt_module_state = IDLE;
	intel_lbf_lpm.fm_module_state = IDLE;
	intel_lbf_lpm.bt_fmr_state = IDLE;

	mutex_unlock(&lbf_tx->fwdownloadlock);
}

/* st_wakeup_packet_processing()
 * Processes all the LPM packet
 * Return Type: void
 */

static void st_wakeup_packet_processing(struct lbf_uart *lbf_uart)
{
	spin_lock(&intel_lbf_lpm.interrupt_lock);
	if (lbf_uart->rx_skb->data[0] != LPM_PKT) {
		spin_unlock(&intel_lbf_lpm.interrupt_lock);
		return;
	}
	switch (lbf_uart->rx_skb->data[1]) {
	case ACK_RSP:
		pr_info("BT CORE IN D2 STATE\n");
		if (lbf_get_ackigne_st() == false &&
				lbf_get_device_state() != D0) {
			lbf_set_host_wake_state(IDLE);
			/*Work around for not receiving Idle
				notification before interrupt*/
			lbf_set_device_state(D2);
			if (lbf_get_tx_ref_count()) {
				lbf_maintain_tx_refcnt(-1);
				lbf_serial_put();
			}
			if (lbf_get_rx_ref_count()) {
				lbf_maintain_rx_refcnt(-1);
				lbf_serial_put();
			}
		}
		lbf_set_ackign_st(false);
		enable_irq(intel_lbf_lpm.int_host_wake);
		wake_up_interruptible(&waitFord0exit);
		break;
	case WAKE_UP_RSP:
		pr_info("BT CORE IN D0 STATE\n");
		if (lbf_get_wakeupign_st() == false) {
			lbf_set_device_state(D0);
			if (!lbf_get_rx_ref_count()) {
				lbf_maintain_rx_refcnt(1);
				schedule_work(&lbf_tx->host_enable_work);
			}
			wake_up_interruptible(&waitFordxexit);
		}
		lbf_set_wakeupign_st(false);
		break;
	default:
		break;
	}
	spin_unlock(&intel_lbf_lpm.interrupt_lock);
	lbf_update_set_room(lbf_tx->tty, WAKEUP_RSP_SIZE);

}
/* st_check_data_len()
 * push the skb received to relevant
 * protocol stacks
 * Arguments : tty pointer to associated tty instance data
 *lbf_uart : Disc data for tty
 *len : lenn of data
 * Return Type: void
 */

static inline int st_check_data_len(struct lbf_uart *lbf_uart, int len)
{
	int room = skb_tailroom(lbf_uart->rx_skb);

	if (!len) {
		/* Received packet has only packet header and
		 * has zero length payload. So, ask ST CORE to
		 * forward the packet to protocol driver (BT/FM/GPS)
		 */
		 if (intel_lbf_lpm.lpm_enable == ENABLE) {
			st_wakeup_packet_processing(lbf_uart);
			goto pm_packet;
		} else {
			st_send_frame(lbf_uart->tty, lbf_uart);
			pr_info("Should not be hit\n");
		}

	} else if (len > room) {
		/* Received packet's payload length is larger.
		 * We can't accommodate it in created skb.
		 */
		pr_debug("Data length is too large len %d room %d", len, room);
		kfree_skb(lbf_uart->rx_skb);
	} else {
		/* Packet header has non-zero payload length and
		 * we have enough space in created skb. Lets read
		 * payload data */

		lbf_uart->rx_state = LBF_W4_DATA;
		lbf_uart->rx_count = len;

		return len;
	}

	/* Change LDISC state to continue to process next
	 * packet */
pm_packet:
	if (lbf_uart->rx_skb) {
		kfree_skb(lbf_uart->rx_skb);
		lbf_uart->rx_skb = NULL;
	}

	lbf_uart->rx_state = LBF_W4_H4_HDR;
	lbf_uart->rx_count = 0;
	lbf_uart->rx_chnl = 0;
	lbf_uart->bytes_pending = 0;

	return 0;
}

/**
 * lbf_update_set_room - receive space
 * @tty: terminal
 *
 * Called by the driver to find out how much data it is
 * permitted to feed to the line discipline without any being lost
 * and thus to manage flow control. Not serialized. Answers for the
 * "instant".
 */

static void lbf_update_set_room(struct tty_struct *tty, signed int cnt)
{
	struct lbf_uart *lbf_uart = (struct lbf_uart *) tty->disc_data;
	int left;
	int old_left;

	spin_lock(&lbf_uart->tx_update_lock);
	left = tty->receive_room + cnt;

	if (left < 0)
		left = 0;
	old_left = tty->receive_room;
	tty->receive_room = left;
	spin_unlock(&lbf_uart->tx_update_lock);

	pr_debug("-> %s, tty_receive:%d\n", __func__, tty->receive_room);

	if (left && !old_left) {
		WARN_RATELIMIT(tty->port->itty == NULL,
				"scheduling with invalid itty\n");
		/* see if ldisc has been killed - if so, this means that
		 * even though the ldisc has been halted and ->buf.work
		 * cancelled, ->buf.work is about to be rescheduled
		 */
		WARN_RATELIMIT(test_bit(TTY_LDISC_HALTED, &tty->flags),
				"scheduling buffer work for halted ldisc\n");
		schedule_work(&tty->port->buf.work);
	}
}

static inline int packet_state(int exp_count, int recv_count)
{
	int status = 0;
	if (exp_count > recv_count)
		status = exp_count - recv_count;

	return status;
}

/* lbf_ldisc_receive()
 *
 * Called by tty low level driver when receive data is
 * available.
 *
 * Arguments: tty pointer to tty isntance data
 * data pointer to received data
 * flags pointer to flags for data
 * count count of received data in bytes
 *
 * Return Value: None
 */
static void lbf_ldisc_receive(struct tty_struct *tty, const u8 *cp, char *fp,
		int count)
{

	unsigned char *ptr;
	int proto = INVALID;
	int pkt_status;
	unsigned short payload_len = 0;
	int len = 0, type = 0, i = 0;
	unsigned char *plen;
	int st_ret = 0;
	struct lbf_uart *lbf_uart = (struct lbf_uart *) tty->disc_data;
	unsigned long flags;

	pr_info("-> %s\n", __func__);
	print_hex_dump_debug(">in>", DUMP_PREFIX_NONE, 16, 1, cp, count,
			0);
	ptr = (unsigned char *) cp;
	if (unlikely(ptr == NULL) || unlikely(lbf_uart == NULL)) {
		pr_debug(" received null from TTY ");
		return;
	}

	lbf_update_set_room(tty, (-1)*count);

	spin_lock_irqsave(&lbf_uart->rx_lock, flags);

	pr_debug("-> %s count: %d lbf_uart->rx_state: %ld\n", __func__ , count,
		 lbf_uart->rx_state);

	while (count) {
		if (likely(lbf_uart->bytes_pending)) {
			len = min_t(unsigned int, lbf_uart->bytes_pending,
				 count);
			lbf_uart->rx_count = lbf_uart->bytes_pending;
			memcpy(skb_put(lbf_uart->rx_skb, len),
					ptr, len);
		} else if ((lbf_uart->rx_count)) {
			len = min_t(unsigned int, lbf_uart->rx_count, count);
			memcpy(skb_put(lbf_uart->rx_skb, len), ptr, len);
		}

		switch (lbf_uart->rx_state) {
		case LBF_W4_H4_HDR:
			if (*ptr != 0x01 && *ptr != 0x02 && *ptr != 0x03 &&
				*ptr != 0x04 && *ptr != LPM_PKT) {
				pr_info(" Discard a byte 0x%x\n" , *ptr);
				ptr++;
				count = count - 1;
				continue;
			}
			switch (*ptr) {
			default:
				type = *ptr;
				proto = is_valid_proto(type);
				if (type == LPM_PKT)
					proto = 0;
				if (proto != INVALID) {
					lbf_uart->rx_skb = alloc_skb(1700,
						GFP_ATOMIC);
					if (!lbf_uart->rx_skb)
						return;
					lbf_uart->rx_chnl = proto;
					lbf_uart->rx_state = LBF_W4_PKT_HDR;
					lbf_uart->rx_count =
				lbf_st_proto[lbf_uart->rx_chnl].hdr_len + 1;
				} else {
					pr_debug("Invalid header type: 0x%x\n",
					 type);
					count = 0;
					break;
				}
				continue;
			}
			break;

		case LBF_W4_PKT_HDR:
			pkt_status = packet_state(lbf_uart->rx_count, count);
			pr_debug("%s: lbf_uart->rx_count %ld\n", __func__,
				lbf_uart->rx_count);
			count -= len;
			ptr += len;
			lbf_uart->rx_count -= len;
			if (pkt_status) {
				lbf_uart->bytes_pending = pkt_status;
				count = 0;
			} else {
				plen = &lbf_uart->rx_skb->data
			[lbf_st_proto[lbf_uart->rx_chnl].offset_len_in_hdr];
				lbf_uart->bytes_pending = 0;
				if (lbf_st_proto[lbf_uart->rx_chnl].len_size
					== 1)
					payload_len = *(unsigned char *)plen;
				else if (lbf_st_proto[lbf_uart->rx_chnl].
						len_size == 2)
					payload_len =
					__le16_to_cpu(*(unsigned short *)plen);
				else
					pr_debug("%s invalid len for id %d\n",
							__func__, proto);

				st_check_data_len(lbf_uart, payload_len);
				}
			continue;

		case LBF_W4_DATA:
			pkt_status = packet_state(lbf_uart->rx_count, count);
			count -= len;
			ptr += len;
			i = 0;
			lbf_uart->rx_count -= len;
			if (!pkt_status) {
				pr_debug("\n---------Complete pkt received----\n");
				lbf_uart->rx_state = LBF_W4_H4_HDR;
				st_ret = st_send_frame(tty, lbf_uart);
				lbf_uart->bytes_pending = 0;
				lbf_uart->rx_count = 0;
				lbf_uart->rx_skb = NULL;
			} else {
				lbf_uart->bytes_pending = pkt_status;
				count = 0;
			}

			if (st_ret > 0)
				lbf_update_set_room(tty, st_ret);
			continue;

		} /* end of switch rx_state*/
	} /* end of while*/

	spin_unlock_irqrestore(&lbf_uart->rx_lock, flags);

	if (waitqueue_active(&lbf_uart->read_wait))
		wake_up_interruptible(&lbf_uart->read_wait);


	if (tty->receive_room < TTY_THRESHOLD_THROTTLE)
		tty_throttle(tty);

	pr_info("<- %s\n", __func__);
}

/* lbf_ldisc_ioctl()
 *
 * Process IOCTL system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 * cmd IOCTL command code
 * arg argument for IOCTL call (cmd dependent)
 *
 * Return Value: Command dependent
 */
static int lbf_ldisc_ioctl(struct tty_struct *tty, struct file *file,
		unsigned int cmd, unsigned long arg)
{

	int err = 0;
	pr_debug("-> %s\n", __func__);
	switch (cmd) {
	case BT_FW_DOWNLOAD_INIT:
		pr_info("BT_FW_DOWNLOAD_INIT\n");
		err = lbf_ldisc_fw_download_init();
		break;
	case BT_FW_DOWNLOAD_COMPLETE:
		pr_info("BT_FW_DOWNLOAD_COMPLETE\n");
		lbf_ldisc_fw_download_complete(arg);
		break;
	case BT_FMR_LPM_ENABLE:
		pr_info("BT_FMR_LPM_ENABLE\n");
		lbf_ldisc_lpm_enable(arg);
		break;
	case BT_FMR_IDLE:
		pr_info("BT_FMR_IDLE\n");
		lbf_ldisc_lpm_idle(arg);
		break;
	default:
		err = n_tty_ioctl_helper(tty, file, cmd, arg);
	}
	pr_info("<- %s\n", __func__);
	return err;
}

/* lbf_ldisc_compat_ioctl()
 *
 * Process IOCTL system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 * cmd IOCTL command code
 * arg argument for IOCTL call (cmd dependent)
 *
 * Return Value: Command dependent
 */
static long lbf_ldisc_compat_ioctl(struct tty_struct *tty, struct file *file,
		unsigned int cmd, unsigned long arg)
{

	int err = 0;
	pr_debug("-> %s\n", __func__);
	switch (cmd) {
	case BT_FW_DOWNLOAD_INIT:
		pr_info("BT_FW_DOWNLOAD_INIT");
		err = lbf_ldisc_fw_download_init();
		break;
	case BT_FW_DOWNLOAD_COMPLETE:
		pr_info("BT_FW_DOWNLOAD_COMPLETED");
		lbf_ldisc_fw_download_complete(arg);
		break;
	case BT_FMR_LPM_ENABLE:
		pr_info("BT_FMR_LPM_ENABLE\n");
		lbf_ldisc_lpm_enable(arg);
		break;
	case BT_FMR_IDLE:
		pr_info("BT_FMR_IDLE\n");
		lbf_ldisc_lpm_idle(arg);
		break;
	default:
		err = n_tty_ioctl_helper(tty, file, cmd, arg);
	}
	pr_info("<- %s\n", __func__);
	return err;
}



/* copy_from_read_buf()
 *
 * Internal function for lbf_ldisc_read().
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * buf buf to copy to user space
 * nr number of bytes to be read
 *
 * Return Value: number of bytes copy to user spcae succesfully
 */
static int copy_from_read_buf(struct tty_struct *tty,
		unsigned char __user **b,
		size_t *nr)
{
	int retval = 0;
	size_t n;
	struct lbf_uart *lbf_ldisc = (struct lbf_uart *)tty->disc_data;
	spin_lock(&lbf_ldisc->tx_lock);
	n = min3(lbf_ldisc->read_cnt,
		 (N_INTEL_LDISC_BUF_SIZE - lbf_ldisc->read_tail), (int)*nr);
	spin_unlock(&lbf_ldisc->tx_lock);
	if (n) {
		retval =
		copy_to_user(*b, &lbf_ldisc->read_buf[lbf_ldisc->read_tail],
			n);
		n -= retval;
		spin_lock(&lbf_ldisc->tx_lock);
		lbf_ldisc->read_tail =
		(lbf_ldisc->read_tail + n) & (N_INTEL_LDISC_BUF_SIZE-1);
		lbf_ldisc->read_cnt -= n;
		spin_unlock(&lbf_ldisc->tx_lock);
		*b += n;
	}
	return n;
}

/* lbf_ldisc_get_read_cnt()
 *
 * Gets the read count available currently
 *
 * Arguments:
 *
 * void
 *
 * Return Value: number of bytes available to be copied to user space
 *
 */
static unsigned int lbf_ldisc_get_read_cnt(void)
{
	int read_cnt = 0;
	struct lbf_uart *lbf_ldisc = (struct lbf_uart *)lbf_tx->tty->disc_data;
	spin_lock(&lbf_ldisc->tx_lock);
	read_cnt = lbf_ldisc->read_cnt;
	spin_unlock(&lbf_ldisc->tx_lock);
	return read_cnt;
}

/* lbf_ldisc_read()
 *
 * Process read system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance datan
 * file pointer to open file object for device
 * buf buf to copy to user space
 * nr number of bytes to be read
 *
 * Return Value: number of bytes read copied to user space
 * succesfully
 */
static ssize_t lbf_ldisc_read(struct tty_struct *tty, struct file *file,
		unsigned char __user *buf, size_t nr)
{
	unsigned char __user *b = buf;
	ssize_t size;
	int retval = 0;
	int state = -1;
	struct lbf_uart *lbf_uart = (struct lbf_uart *)tty->disc_data;
	pr_info("-> %s\n", __func__);
	init_waitqueue_entry(&lbf_uart->wait, current);

	if (!lbf_ldisc_get_read_cnt()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		else {
			if (mutex_lock_interruptible(
				&lbf_uart->atomic_read_lock))
				return -ERESTARTSYS;
			else
				state = 0;
			}
	}



	add_wait_queue(&lbf_uart->read_wait, &lbf_uart->wait);
	while (nr) {
		if (lbf_ldisc_get_read_cnt() > 0) {
			int copied;
			__set_current_state(TASK_RUNNING);
			/* The copy function takes the read lock and handles
			 locking internally for this case */
			copied = copy_from_read_buf(tty, &b, &nr);
			copied += copy_from_read_buf(tty, &b, &nr);
			if (copied) {
				retval = 0;
				if (lbf_uart->read_cnt <=
					TTY_THRESHOLD_UNTHROTTLE)
					check_unthrottle(tty);
			break;
			}
		} else {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}
	}

	if (state == 0)
		mutex_unlock(&lbf_uart->atomic_read_lock);


	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&lbf_uart->read_wait, &lbf_uart->wait);

	size = b - buf;
	if (size)
		retval = size;

	lbf_update_set_room(tty, retval);
	pr_debug("<- %s\n", __func__);
	return retval;
}

/* lbf_ldisc_write()
 *
 * Process Write system call from user Space.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 * buf data to pass down
 * nr number of bytes to be written down
 *
 * Return Value: no of bytes written succesfully
 */
static ssize_t lbf_ldisc_write(struct tty_struct *tty, struct file *file,
		const unsigned char *data, size_t count)
{
	struct sk_buff *skb = alloc_skb(count + 1, GFP_ATOMIC);
	int len = 0;
	int ret = 0;
	pr_info("-> %s\n", __func__);

	/*print_hex_dump(KERN_DEBUG, "<BT out<", DUMP_PREFIX_NONE, 16, 1, data,
			count, 0);*/

	if (!skb)
		return -ENOMEM;

	memcpy(skb_put(skb, count), data, count);
	len = count;

	if (intel_lbf_lpm.lpm_enable == ENABLE)
		ret = lpm_tx_update(MODULE_BT);

	if (!ret) {
		lbf_enqueue(skb);
		len = count;
		lbf_tx_wakeup(NULL);
	}

	pr_debug("<- %s\n", __func__);
	return len;
}

/* lbf_ldisc_poll()
 *
 * Process POLL system call for the tty device.
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 *
 * Return Value: mask of events registered with this poll call
 */
static unsigned int lbf_ldisc_poll(struct tty_struct *tty, struct file *file,
		poll_table *wait)
{
	unsigned int mask = 0;
	struct lbf_uart *lbf_uart = (struct lbf_uart *) tty->disc_data;

	pr_debug("-> %s\n", __func__);

	if (lbf_uart->read_cnt > 0)
		mask |= POLLIN | POLLRDNORM;
	else {
		poll_wait(file, &lbf_uart->read_wait, wait);
		if (lbf_uart->read_cnt > 0)
			mask |= POLLIN | POLLRDNORM;
		if (tty->packet && tty->link->ctrl_status)
			mask |= POLLPRI | POLLIN | POLLRDNORM;
		if (test_bit(TTY_OTHER_CLOSED, &tty->flags))
			mask |= POLLHUP;
		if (tty_hung_up_p(file))
			mask |= POLLHUP;
		if (tty->ops->write && !tty_is_writelocked(tty)
				&& tty_chars_in_buffer(tty) < 256
				&& tty_write_room(tty) > 0)
			mask |= POLLOUT | POLLWRNORM;
	}
	pr_debug("<- %s\n", __func__);
	return mask;
}

/* lbf_ldisc_set_termios()
 *
 * It notifies the line discpline that a change has been made to the
 * termios structure
 *
 * Arguments:
 *
 * tty pointer to tty instance data
 * file pointer to open file object for device
 *
 * Return Value: void
 */
static void lbf_ldisc_set_termios(struct tty_struct *tty, struct ktermios *old)
{
	unsigned int cflag;
	cflag = tty->termios.c_cflag;
	pr_debug("-> %s\n", __func__);

	if (cflag & CRTSCTS)
		pr_debug(" - RTS/CTS is enabled\n");
	else
		pr_debug(" - RTS/CTS is disabled\n");
}


static int bt_gpio_init(struct device *dev, struct intel_bt_gpio_desc *desc)
{

	int ret = 0;
	struct gpio_desc *gpio;

	if (!desc)
		gpio = devm_gpiod_get_index(dev, "bt_reg_on", 1);
	else if (desc->reg_on_idx >= 0)
		gpio = devm_gpiod_get_index(dev, "bt_reg_on", desc->reg_on_idx);
	else
		gpio = NULL;


	if (gpio && !IS_ERR(gpio)) {
		pr_info("bt_enable:%d", desc_to_gpio(gpio));
		ret = gpiod_direction_output(gpio, 0);
		if (ret) {
			pr_info("didn't get reg_on");
			return ret;
		}
		intel_lbf_lpm.gpio_enable_bt = gpio;
	} else
		pr_info("Failed to get bt_reg_on gpio\n");

#if 0
	Not used for CHT PO

	if (desc && (desc->host_wake_idx >= 0)) {
		gpio = devm_gpiod_get_index(dev, "bt_host_wake",
					    desc->host_wake_idx);
		pr_info("bt_host_wake:%d\n", desc_to_gpio(gpio));
		if (!IS_ERR(gpio)) {
			ret = gpiod_direction_input(gpio);
			if (ret)
				return ret;

			intel_lbf_lpm.int_host_wake = gpiod_to_irq(gpio);

			if (ret)
				return ret;

		}
	}
#endif
	return 0;
}
static int intel_bt_acpi_probe(struct device *dev,
				  struct intel_bt_gpio_data *bt_gpio)
{

	const struct acpi_device_id *id;
	struct intel_bt_gpio_desc *desc;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!id) {
		pr_info("acpi id didn't match");
		return -ENODEV;
	}

	desc = (struct intel_bt_gpio_desc *)id->driver_data;
	if (!desc)
		return -ENODEV;

	return bt_gpio_init(dev, desc);
}



static void bt_rfkill_set_power(unsigned long blocked)
{
	pr_debug("%s blocked: %lu\n", __func__, blocked);
	if (ENABLE == blocked) {
		gpiod_set_value(intel_lbf_lpm.gpio_enable_bt, 1);
		pr_err("%s: turn BT on\n", __func__);
		pr_info("BT CORE IN D0 STATE\n");
		bt_enable_state = ENABLE;
		if (intel_lbf_lpm.lpm_enable == ENABLE)
			lbf_ldisc_lpm_enable_init();
	} else if (DISABLE == blocked) {
		gpiod_set_value(intel_lbf_lpm.gpio_enable_bt, 0);
		pr_err("%s: turn BT off\n", __func__);
		pr_info("BT CORE IN D3 STATE\n");
		bt_enable_state = DISABLE;
	}
}


static int intel_bluetooth_probe(struct platform_device *pdev)
{

	int default_state = 0;	/* off */
	int ret = 0;
	struct intel_bt_gpio_data *bt_gpio;
	pr_info("-> P%s\n", __func__);
	bt_gpio = devm_kzalloc(&pdev->dev, sizeof(*bt_gpio), GFP_KERNEL);
	if (!bt_gpio)
		return -ENOMEM;

	platform_set_drvdata(pdev, bt_gpio);
	intel_lbf_lpm.pdev = pdev;

	if (ACPI_HANDLE(&pdev->dev)) {
		ret = intel_bt_acpi_probe(&pdev->dev, bt_gpio);
		if (ret)
			return ret;

		bt_rfkill_set_power(default_state);

	} else
		ret = -ENODEV;

	pr_info("%s ret: %d\n", __func__, ret);
	return ret;
}

static int intel_bluetooth_remove(struct platform_device *pdev)
{
	pr_err("%s\n", __func__);
	return 0;
}



/*Line discipline op*/
static struct tty_ldisc_ops lbf_ldisc = {
	.magic = TTY_LDISC_MAGIC,
	.name = "n_ld_intel",
	.open = lbf_ldisc_open,
	.close = lbf_ldisc_close,
	.read = lbf_ldisc_read,
	.write = lbf_ldisc_write,
	.ioctl = lbf_ldisc_ioctl,
	.compat_ioctl = lbf_ldisc_compat_ioctl,
	.poll = lbf_ldisc_poll,
	.receive_buf = lbf_ldisc_receive,
	.write_wakeup = lbf_ldisc_wakeup,
	.flush_buffer = lbf_ldisc_flush_buffer,
	.set_termios = lbf_ldisc_set_termios,
	.owner = THIS_MODULE
};

static struct platform_driver intel_bluetooth_platform_driver = {
	.probe = intel_bluetooth_probe,
	.remove = intel_bluetooth_remove,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "intel_bt",
		.owner = THIS_MODULE,
		.acpi_match_table = ACPI_PTR(intel_id_table),
		},
};

static int __init lbf_uart_init(void)
{
	int err = 0;

	pr_debug("-> %s\n", __func__);

	err = platform_driver_register(&intel_bluetooth_platform_driver);
	if (err) {
		pr_err("Registration failed. (%d)", err);
		goto err_platform_register;
	} else
		pr_debug("%s: Plarform registration succeded\n", __func__);

	/* Register the tty discipline */

	err = tty_register_ldisc(N_INTEL_LDISC, &lbf_ldisc);
	if (err) {
		pr_err("Line Discipline Registration failed. (%d)", err);
		goto err_tty_register;
	} else
		pr_info("%s: N_INTEL_LDISC registration succeded\n", __func__);

	pr_debug("<- %s\n", __func__);

	return err;

err_tty_register:
	platform_driver_unregister(&intel_bluetooth_platform_driver);
err_platform_register:
	return err;
}

static void __exit lbf_uart_exit(void)
{
	int err;
	pr_debug("-> %s\n", __func__);

	/* Release tty registration of line discipline */
	err = tty_unregister_ldisc(N_INTEL_LDISC);
	if (err)
		pr_err("Can't unregister N_INTEL_LDISC line discipline (%d)",
			 err);
	platform_driver_unregister(&intel_bluetooth_platform_driver);

	pr_debug("<- %s\n", __func__);
}

module_init(lbf_uart_init);
module_exit(lbf_uart_exit);
MODULE_LICENSE("GPL");

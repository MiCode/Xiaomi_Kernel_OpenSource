/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (c) 2001-2004 by David Brownell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * Root HUB management and Asynchronous scheduling traversal
 * Based on ehci-hub.c and ehci-q.c
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/firmware.h>
#include <linux/spi/spi.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ch9.h>
#include <linux/usb/ch11.h>

#include <asm/unaligned.h>
#include <mach/gpiomux.h>

#define CREATE_TRACE_POINTS
#include <trace/events/ice40.h>

#define FADDR_REG 0x00 /* R/W: Device address */
#define HCMD_REG 0x01 /* R/W: Host transfer command */
#define XFRST_REG 0x02 /* R: Transfer status */
#define IRQ_REG 0x03 /* R/C: IRQ status */
#define IEN_REG 0x04 /* R/W: IRQ enable */
#define CTRL0_REG 0x05 /* R/W: Host control command */
#define CTRL1_REG 0x06 /* R/W: Host control command */
#define WBUF0_REG 0x10 /* W: Tx fifo 0 */
#define WBUF1_REG 0x11 /* W: Tx fifo 1 */
#define SUBUF_REG 0x12 /* W: SETUP fifo */
#define WBLEN_REG 0x13 /* W: Tx fifo size */
#define RBUF0_REG 0x18 /* R: Rx fifo 0 */
#define RBUF1_REG 0x19 /* R: Rx fifo 1 */
#define RBLEN_REG 0x1B /* R: Rx fifo size */

#define WRITE_CMD(addr) ((addr << 3) | 1)
#define READ_CMD(addr) ((addr << 3) | 0)

/* Host controller command register definitions */
#define HCMD_EP(ep) (ep & 0xF)
#define HCMD_BSEL(sel) (sel << 4)
#define HCMD_TOGV(toggle) (toggle << 5)
#define HCMD_PT(token) (token << 6)

/* Transfer status register definitions */
#define XFR_MASK(xfr) (xfr & 0xF)
#define XFR_SUCCESS 0x0
#define XFR_BUSY 0x1
#define XFR_PKTERR 0x2
#define XFR_PIDERR 0x3
#define XFR_NAK 0x4
#define XFR_STALL 0x5
#define XFR_WRONGPID 0x6
#define XFR_CRCERR 0x7
#define XFR_TOGERR 0x8
#define XFR_BADLEN 0x9
#define XFR_TIMEOUT 0xA

#define LINE_STATE(xfr) ((xfr & 0x30) >> 4) /* D+, D- */
#define DPST	BIT(5)
#define DMST	BIT(4)
#define PLLOK	BIT(6)
#define R64B	BIT(7)

/* Interrupt enable/status register definitions */
#define RESET_IRQ BIT(0)
#define RESUME_IRQ BIT(1)
#define SUSP_IRQ BIT(3)
#define DISCONNECT_IRQ BIT(4)
#define CONNECT_IRQ BIT(5)
#define FRAME_IRQ BIT(6)
#define XFR_IRQ BIT(7)

/* Control 0 register definitions */
#define RESET_CTRL BIT(0)
#define FRAME_RESET_CTRL BIT(1)
#define DET_BUS_CTRL BIT(2)
#define RESUME_CTRL BIT(3)
#define SOFEN_CTRL BIT(4)
#define DM_PD_CTRL BIT(6)
#define DP_PD_CTRL BIT(7)
#define HRST_CTRL  BIT(5)

/* Control 1 register definitions */
#define INT_EN_CTRL BIT(0)

enum ice40_xfr_type {
	FIRMWARE_XFR,
	REG_WRITE_XFR,
	REG_READ_XFR,
	SETUP_XFR,
	DATA_IN_XFR,
	DATA_OUT_XFR,
};

enum ice40_ep_phase {
	SETUP_PHASE = 1,
	DATA_PHASE,
	STATUS_PHASE,
};

struct ice40_ep {
	u8 xcat_err;
	bool unlinking;
	bool halted;
	struct usb_host_endpoint *ep;
	struct list_head ep_list;
};

struct ice40_hcd {
	spinlock_t lock;

	struct mutex wlock;
	struct mutex rlock;

	u8 devnum;
	u32 port_flags;
	u8 ctrl0;
	u8 wblen0;

	enum ice40_ep_phase ep0_state;
	struct usb_hcd *hcd;

	struct list_head async_list;
	struct workqueue_struct *wq;
	struct work_struct async_work;

	int reset_gpio;
	int slave_select_gpio;
	int config_done_gpio;
	int vcc_en_gpio;
	int clk_en_gpio;

	struct regulator *core_vcc;
	struct regulator *spi_vcc;
	struct regulator *gpio_vcc;
	bool powered;

	struct dentry *dbg_root;
	bool pcd_pending;

	/* SPI stuff later */
	struct spi_device *spi;

	struct spi_message *fmsg;
	struct spi_transfer *fmsg_xfr; /* size 1 */

	struct spi_message *wmsg;
	struct spi_transfer *wmsg_xfr; /* size 1 */
	u8 *w_tx_buf;
	u8 *w_rx_buf;

	struct spi_message *rmsg;
	struct spi_transfer *rmsg_xfr; /* size 1 */
	u8 *r_tx_buf;
	u8 *r_rx_buf;

	struct spi_message *setup_msg;
	struct spi_transfer *setup_xfr; /* size 2 */
	u8 *setup_buf; /* size 1 for SUBUF */

	struct spi_message *in_msg;
	struct spi_transfer *in_xfr; /* size 2 */
	u8 *in_buf; /* size 2 for reading from RBUF0 */

	struct spi_message *out_msg;
	struct spi_transfer *out_xfr; /* size 2 */
	u8 *out_buf; /* size 1 for writing WBUF0 */
};

static char fw_name[16] = "ice40.bin";
module_param_string(fw, fw_name, sizeof(fw_name), S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(fw, "firmware blob file name");

static bool debugger;
module_param(debugger, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debugger, "true to use the debug port");

static inline struct ice40_hcd *hcd_to_ihcd(struct usb_hcd *hcd)
{
	return *((struct ice40_hcd **) hcd->hcd_priv);
}

static void ice40_spi_reg_write(struct ice40_hcd *ihcd, u8 val, u8 addr)
{
	int ret;

	/*
	 * Register Write Pattern:
	 * TX: 1st byte is CMD (register + write), 2nd byte is value
	 * RX: Ignore
	 *
	 * The Mutex is to protect concurrent register writes as
	 * we have only 1 SPI message struct.
	 */

	mutex_lock(&ihcd->wlock);

	ihcd->w_tx_buf[0] = WRITE_CMD(addr);
	ihcd->w_tx_buf[1] = val;
	ret = spi_sync(ihcd->spi, ihcd->wmsg);
	if (ret < 0) /* should not happen */
		pr_err("failed. val = %d addr = %d\n", val, addr);

	trace_ice40_reg_write(addr, val, ihcd->w_tx_buf[0],
			ihcd->w_tx_buf[1], ret);

	mutex_unlock(&ihcd->wlock);
}

static int ice40_spi_reg_read(struct ice40_hcd *ihcd, u8 addr)
{
	int ret;

	/*
	 * Register Read Pattern:
	 * TX: 1st byte is CMD (register + read)
	 * RX: 1st, 2nd byte Ignore, 3rd byte value.
	 *
	 * The Mutex is to protect concurrent register reads as
	 * we have only 1 SPI message struct.
	 */

	mutex_lock(&ihcd->rlock);

	ihcd->r_tx_buf[0] = READ_CMD(addr);
	ret = spi_sync(ihcd->spi, ihcd->rmsg);
	if (ret < 0)
		pr_err("failed. addr = %d\n", addr);
	else
		ret = ihcd->r_rx_buf[2];

	trace_ice40_reg_read(addr, ihcd->r_tx_buf[0], ret);

	mutex_unlock(&ihcd->rlock);

	return ret;
}

static int ice40_poll_xfer(struct ice40_hcd *ihcd, int usecs)
{
	ktime_t start = ktime_get();
	u8 val, retry = 0;
	u8 ret = ~0; /* time out */

again:

	/*
	 * The SPI transaction may take tens of usec. Use ktime
	 * based checks rather than loop count.
	 */
	do {
		val = ice40_spi_reg_read(ihcd, XFRST_REG);

		if (XFR_MASK(val) != XFR_BUSY)
			return val;

	} while (ktime_us_delta(ktime_get(), start) < usecs);

	/*
	 * The SPI transaction involves a context switch. For any
	 * reason, if we are scheduled out more than usecs after
	 * the 1st read, this extra read will help.
	 */
	if (!retry) {
		retry = 1;
		goto again;
	}

	return ret;
}

static int
ice40_handshake(struct ice40_hcd *ihcd, u8 reg, u8 mask, u8 done, int usecs)
{
	ktime_t start = ktime_get();
	u8 val, retry = 0;

again:
	do {
		val = ice40_spi_reg_read(ihcd, reg);
		val &= mask;

		if (val == done)
			return 0;

	} while (ktime_us_delta(ktime_get(), start) < usecs);

	if (!retry) {
		retry = 1;
		goto again;
	}

	return -ETIMEDOUT;
}


static const char hcd_name[] = "ice40-hcd";

static int ice40_reset(struct usb_hcd *hcd)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);
	u8 ctrl, status;
	int ret = 0;

	/*
	 * Program the defualt address 0. The device address is
	 * re-programmed after SET_ADDRESS in URB handling path.
	 */
	ihcd->devnum = 0;
	ice40_spi_reg_write(ihcd, 0, FADDR_REG);

	ihcd->wblen0 = ~0;
	/*
	 * Read the line state. This driver is loaded after the
	 * UICC card insertion. So the line state should indicate
	 * that a Full-speed device is connected. Return error
	 * if there is no device connected.
	 *
	 * There can be no device connected during debug. A debugfs
	 * file is provided to sample the bus line and update the
	 * port flags accordingly.
	 */

	if (debugger)
		goto out;

	ctrl = ice40_spi_reg_read(ihcd, CTRL0_REG);
	ice40_spi_reg_write(ihcd, ctrl | DET_BUS_CTRL, CTRL0_REG);

	ret = ice40_handshake(ihcd, CTRL0_REG, DET_BUS_CTRL, 0, 5000);
	if (ret) {
		pr_err("bus detection failed\n");
		goto out;
	}

	status = ice40_spi_reg_read(ihcd, XFRST_REG);
	pr_debug("line state (D+, D-) is %d\n", LINE_STATE(status));

	if (status & DPST) {
		pr_debug("Full speed device connected\n");
		ihcd->port_flags |= USB_PORT_STAT_CONNECTION;
	} else {
		pr_err("No device connected\n");
		ret = -ENODEV;
	}
out:
	return ret;
}

static int ice40_run(struct usb_hcd *hcd)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);

	/*
	 * HCD_FLAG_POLL_RH flag is not set by us. Core will not poll
	 * for the port status periodically. This uses_new_polling
	 * flag tells core that this hcd will call usb_hcd_poll_rh_status
	 * upon port change.
	 */
	hcd->uses_new_polling = 1;

	/*
	 * Cache the ctrl0 register to avoid multiple reads. This register
	 * is written during reset and resume.
	 */
	ihcd->ctrl0 = ice40_spi_reg_read(ihcd, CTRL0_REG);
	ihcd->ctrl0 |= SOFEN_CTRL;
	ice40_spi_reg_write(ihcd, ihcd->ctrl0, CTRL0_REG);

	return 0;
}

static void ice40_stop(struct usb_hcd *hcd)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);

	cancel_work_sync(&ihcd->async_work);
}

/*
 * The _Error looks odd. But very helpful when looking for
 * any errors in logs.
 */
static char __maybe_unused *xfr_status_string(int status)
{
	switch (XFR_MASK(status)) {
	case XFR_SUCCESS:
		return "Ack";
	case XFR_BUSY:
		return "Busy_Error";
	case XFR_PKTERR:
		return "Pkt_Error";
	case XFR_PIDERR:
		return "PID_Error";
	case XFR_NAK:
		return "Nak";
	case XFR_STALL:
		return "Stall_Error";
	case XFR_WRONGPID:
		return "WrongPID_Error";
	case XFR_CRCERR:
		return "CRC_Error";
	case XFR_TOGERR:
		return "Togg_Error";
	case XFR_BADLEN:
		return "BadLen_Error";
	case XFR_TIMEOUT:
		return "Timeout_Error";
	default:
		return "Unknown_Error";
	}
}

static int ice40_xfer_setup(struct ice40_hcd *ihcd, struct urb *urb)
{
	struct usb_host_endpoint *ep = urb->ep;
	struct ice40_ep *iep = ep->hcpriv;
	void *buf = urb->setup_packet;
	int ret, status;
	u8 cmd;

	/*
	 * SETUP transaction Handling:
	 * - copy the setup buffer to SUBUF fifo
	 * - Program HCMD register to initiate the SETP transaction.
	 * - poll for completion by reading XFRST register.
	 * - Interpret the result.
	 */

	ihcd->setup_buf[0] = WRITE_CMD(SUBUF_REG);
	ihcd->setup_xfr[1].tx_buf = buf;
	ihcd->setup_xfr[1].len = sizeof(struct usb_ctrlrequest);

	ret = spi_sync(ihcd->spi, ihcd->setup_msg);
	if (ret < 0) {
		pr_err("SPI transfer failed\n");
		status = ret = -EIO;
		goto out;
	}

	cmd = HCMD_PT(2) | HCMD_TOGV(0) | HCMD_BSEL(0) | HCMD_EP(0);
	ice40_spi_reg_write(ihcd, cmd, HCMD_REG);

	status = ice40_poll_xfer(ihcd, 1000);
	switch (XFR_MASK(status)) {
	case XFR_SUCCESS:
		iep->xcat_err = 0;
		ret = 0;
		break;
	case XFR_NAK: /* Device should not return Nak for SETUP */
	case XFR_STALL:
		iep->xcat_err = 0;
		ret = -EPIPE;
		break;
	case XFR_PKTERR:
	case XFR_PIDERR:
	case XFR_WRONGPID:
	case XFR_CRCERR:
	case XFR_TIMEOUT:
		if (++iep->xcat_err < 8)
			ret = -EINPROGRESS;
		else
			ret = -EPROTO;
		break;
	default:
		pr_err("transaction timed out\n");
		ret = -EIO;
	}

out:
	trace_ice40_setup(xfr_status_string(status), ret);
	return ret;
}

static int ice40_xfer_in(struct ice40_hcd *ihcd, struct urb *urb)
{
	struct usb_host_endpoint *ep = urb->ep;
	struct usb_device *udev = urb->dev;
	u32 total_len = urb->transfer_buffer_length;
	u16 maxpacket = usb_endpoint_maxp(&ep->desc);
	u8 epnum = usb_pipeendpoint(urb->pipe);
	bool is_out = usb_pipeout(urb->pipe);
	struct ice40_ep *iep = ep->hcpriv;
	u8 cmd, status, len = 0, t, expected_len;
	void *buf;
	int ret;
	bool short_packet = true;

	if (epnum == 0 && ihcd->ep0_state == STATUS_PHASE) {
		expected_len = 0;
		buf = NULL;
		t = 1; /* STATUS PHASE is always DATA1 */
	} else {
		expected_len = min_t(u32, maxpacket,
				total_len - urb->actual_length);
		buf = urb->transfer_buffer + urb->actual_length;
		t = usb_gettoggle(udev, epnum, is_out);
	}

	/*
	 * IN transaction Handling:
	 * - Program HCMD register to initiate the IN transaction.
	 * - poll for completion by reading XFRST register.
	 * - Interpret the result.
	 * - If ACK is received and we expect some data, read RBLEN
	 * - Read the data from RBUF
	 */

	cmd = HCMD_PT(0) | HCMD_TOGV(t) | HCMD_BSEL(0) | HCMD_EP(epnum);
	ice40_spi_reg_write(ihcd, cmd, HCMD_REG);

	status = ice40_poll_xfer(ihcd, 1000);
	switch (XFR_MASK(status)) {
	case XFR_SUCCESS:
		usb_dotoggle(udev, epnum, is_out);
		iep->xcat_err = 0;
		ret = 0;
		if ((expected_len == 64) && (status & R64B))
			short_packet = false;
		break;
	case XFR_NAK:
		iep->xcat_err = 0;
		ret = -EINPROGRESS;
		break;
	case XFR_TOGERR:
		/*
		 * Peripheral had missed the previous Ack and sent
		 * the same packet again. Ack is sent by the hardware.
		 * As the data is received already, ignore this
		 * event.
		 */
		ret = -EINPROGRESS;
		break;
	case XFR_PKTERR:
	case XFR_PIDERR:
	case XFR_WRONGPID:
	case XFR_CRCERR:
	case XFR_TIMEOUT:
		if (++iep->xcat_err < 8)
			ret = -EINPROGRESS;
		else
			ret = -EPROTO;
		break;
	case XFR_STALL:
		ret = -EPIPE;
		break;
	case XFR_BADLEN:
		ret = -EOVERFLOW;
		break;
	default:
		pr_err("transaction timed out\n");
		ret = -EIO;
	}

	/*
	 * Proceed further only if Ack is received and
	 * we are expecting some data.
	 */
	if (ret || !expected_len)
		goto out;

	if (short_packet)
		len = ice40_spi_reg_read(ihcd, RBLEN_REG);
	else
		len = 64;

	/* babble condition */
	if (len > expected_len) {
		pr_err("overflow condition\n");
		ret = -EOVERFLOW;
		goto out;
	}

	/*
	 * zero len packet received. nothing to read from
	 * FIFO.
	 */
	if (len == 0) {
		ret = 0;
		goto out;
	}

	ihcd->in_buf[0] = READ_CMD(RBUF0_REG);

	ihcd->in_xfr[1].rx_buf = buf;
	ihcd->in_xfr[1].len = len;

	ret = spi_sync(ihcd->spi, ihcd->in_msg);
	if (ret < 0) {
		pr_err("SPI transfer failed\n");
		ret = -EIO;
		goto out;
	}

	urb->actual_length += len;
	if ((urb->actual_length == total_len) ||
			(len < expected_len))
		ret = 0; /* URB completed */
	else
		ret = -EINPROGRESS; /* still pending */
out:
	trace_ice40_in(epnum, xfr_status_string(status), len,
			expected_len, ret);
	return ret;
}

static int ice40_xfer_out(struct ice40_hcd *ihcd, struct urb *urb)
{
	struct usb_host_endpoint *ep = urb->ep;
	struct usb_device *udev = urb->dev;
	u32 total_len = urb->transfer_buffer_length;
	u16 maxpacket = usb_endpoint_maxp(&ep->desc);
	u8 epnum = usb_pipeendpoint(urb->pipe);
	bool is_out = usb_pipeout(urb->pipe);
	struct ice40_ep *iep = ep->hcpriv;
	u8 cmd, status, len, t;
	void *buf;
	int ret;

	if (epnum == 0 && ihcd->ep0_state == STATUS_PHASE) {
		len = 0;
		buf = NULL;
		t = 1; /* STATUS PHASE is always DATA1 */
	} else {
		len = min_t(u32, maxpacket, total_len - urb->actual_length);
		buf = urb->transfer_buffer + urb->actual_length;
		t = usb_gettoggle(udev, epnum, is_out);
	}

	/*
	 * OUT transaction Handling:
	 * - If we need to send data, write the data to WBUF Fifo
	 * - Program the WBLEN register
	 * - Program HCMD register to initiate the OUT transaction.
	 * - poll for completion by reading XFRST register.
	 * - Interpret the result.
	 */


	if (!len)
		goto no_data;

	ihcd->out_buf[0] = WRITE_CMD(WBUF0_REG);

	ihcd->out_xfr[1].tx_buf = buf;
	ihcd->out_xfr[1].len = len;

	ret = spi_sync(ihcd->spi, ihcd->out_msg);
	if (ret < 0) {
		pr_err("SPI transaction failed\n");
		status = ret = -EIO;
		goto out;
	}

no_data:
	/*
	 * Cache the WBLEN register and update it only if it
	 * is changed from the previous value.
	 */
	if (len != ihcd->wblen0) {
		ice40_spi_reg_write(ihcd, len, WBLEN_REG);
		ihcd->wblen0 = len;
	}

	cmd = HCMD_PT(1) | HCMD_TOGV(t) | HCMD_BSEL(0) | HCMD_EP(epnum);
	ice40_spi_reg_write(ihcd, cmd, HCMD_REG);

	status = ice40_poll_xfer(ihcd, 1000);
	switch (XFR_MASK(status)) {
	case XFR_SUCCESS:
		usb_dotoggle(udev, epnum, is_out);
		urb->actual_length += len;
		iep->xcat_err = 0;
		if (!len || (urb->actual_length == total_len))
			ret = 0; /* URB completed */
		else
			ret = -EINPROGRESS; /* pending */
		break;
	case XFR_NAK:
		iep->xcat_err = 0;
		ret = -EINPROGRESS;
		break;
	case XFR_PKTERR:
	case XFR_PIDERR:
	case XFR_WRONGPID:
	case XFR_CRCERR:
	case XFR_TIMEOUT:
		if (++iep->xcat_err < 8)
			ret = -EINPROGRESS;
		else
			ret = -EPROTO;
		break;
	case XFR_STALL:
		ret = -EPIPE;
		break;
	case XFR_BADLEN:
		ret = -EOVERFLOW;
		break;
	default:
		pr_err("transaction timed out\n");
		ret = -EIO;
	}

out:
	trace_ice40_out(epnum, xfr_status_string(status), len, ret);
	return ret;
}

static int ice40_process_urb(struct ice40_hcd *ihcd, struct urb *urb)
{
	struct usb_device *udev = urb->dev;
	u8 devnum = usb_pipedevice(urb->pipe);
	bool is_out = usb_pipeout(urb->pipe);
	u32 total_len = urb->transfer_buffer_length;
	int ret = 0;

	/*
	 * The USB device address can be reset to 0 by core temporarily
	 * during reset recovery process. Don't assume anything about
	 * device address. The device address is programmed as 0 by
	 * default. If the device address is different to the previous
	 * cached value, re-program it here before proceeding. The device
	 * address register (FADDR) holds the value across multiple
	 * transactions and we support only one device.
	 */
	if (ihcd->devnum != devnum) {
		ice40_spi_reg_write(ihcd, devnum, FADDR_REG);
		ihcd->devnum = devnum;
	}

	switch (usb_pipetype(urb->pipe)) {
	case PIPE_CONTROL:
		switch (ihcd->ep0_state) {
		case SETUP_PHASE:
			trace_ice40_ep0("SETUP");
			ret = ice40_xfer_setup(ihcd, urb);
			if (ret)
				break;
			if (total_len) {
				ihcd->ep0_state = DATA_PHASE;
				/*
				 * Data stage always begin with
				 * DATA1 PID.
				 */
				usb_settoggle(udev, 0, is_out, 1);
			} else {
				ihcd->ep0_state = STATUS_PHASE;
				goto do_status;
			}
			/* fall through */
		case DATA_PHASE:
			trace_ice40_ep0("DATA");
			if (is_out)
				ret = ice40_xfer_out(ihcd, urb);
			else
				ret = ice40_xfer_in(ihcd, urb);
			if (ret)
				break;
			/* DATA Phase is completed successfully */
			ihcd->ep0_state = STATUS_PHASE;
			/* fall through */
		case STATUS_PHASE:
do_status:
			trace_ice40_ep0("STATUS");
			/* zero len DATA transfers have IN status */
			if (!total_len || is_out)
				ret = ice40_xfer_in(ihcd, urb);
			else
				ret = ice40_xfer_out(ihcd, urb);
			if (ret)
				break;
			ihcd->ep0_state = SETUP_PHASE;
			break;
		default:
			pr_err("unknown stage for a control transfer\n");
			break;
		}
		break;
	case PIPE_BULK:
		if (is_out)
			ret = ice40_xfer_out(ihcd, urb);
		else
			ret = ice40_xfer_in(ihcd, urb);
		/*
		 * We may have to support zero len packet terminations
		 * for URB_ZERO_PACKET URBs.
		 */
		break;
	default:
		pr_err("IN/ISO transfers not supported\n");
		break;
	}

	return ret;
}

/* Must be called with spin lock and interrupts disabled */
static void ice40_complete_urb(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);
	struct usb_host_endpoint *ep = urb->ep;
	struct ice40_ep *iep = ep->hcpriv;
	struct urb *first_urb;
	bool needs_update = false;
	bool control = usb_pipecontrol(urb->pipe);

	/*
	 * If the active URB i.e the first URB in the ep list is being
	 * removed, clear the transaction error count. If it is a control
	 * URB ep0_state needs to be reset to SETUP_PHASE.
	 */
	first_urb = list_first_entry(&ep->urb_list, struct urb, urb_list);
	if (urb == first_urb)
		needs_update = true;

	usb_hcd_unlink_urb_from_ep(hcd, urb);
	spin_unlock(&ihcd->lock);
	trace_ice40_urb_done(urb, status);
	usb_hcd_giveback_urb(ihcd->hcd, urb, status);
	spin_lock(&ihcd->lock);

	if (needs_update) {
		iep->xcat_err = 0;
		if (control)
			ihcd->ep0_state = SETUP_PHASE;
	}
}

static void ice40_async_work(struct work_struct *work)
{
	struct ice40_hcd *ihcd = container_of(work,
			struct ice40_hcd, async_work);
	struct usb_hcd *hcd = ihcd->hcd;
	struct list_head *tmp, *uent, *utmp;
	struct ice40_ep *iep;
	struct usb_host_endpoint *ep;
	struct urb *urb;
	unsigned long flags;
	int status;

	/*
	 * Traverse the active endpoints circularly and process URBs.
	 * If any endpoint is marked for unlinking, the URBs are
	 * completed here. The endpoint is removed from active list
	 * if a URB is retired with -EPIPE/-EPROTO errors.
	 */

	spin_lock_irqsave(&ihcd->lock, flags);

	if (list_empty(&ihcd->async_list))
		goto out;

	iep = list_first_entry(&ihcd->async_list, struct ice40_ep, ep_list);
	while (1) {
		ep = iep->ep;

		urb = list_first_entry(&ep->urb_list, struct urb, urb_list);
		if (urb->unlinked) {
			status = urb->unlinked;
		} else {
			spin_unlock_irqrestore(&ihcd->lock, flags);
			status = ice40_process_urb(ihcd, urb);
			spin_lock_irqsave(&ihcd->lock, flags);
		}

		if ((status == -EPIPE) || (status == -EPROTO))
			iep->halted = true;

		if (status != -EINPROGRESS)
			ice40_complete_urb(hcd, urb, status);

		if (iep->unlinking) {
			list_for_each_safe(uent, utmp, &ep->urb_list) {
				urb = list_entry(uent, struct urb, urb_list);
				if (urb->unlinked)
					ice40_complete_urb(hcd, urb, 0);
			}
			iep->unlinking = false;
		}

		tmp = iep->ep_list.next;
		if (list_empty(&ep->urb_list) || iep->halted) {
			list_del_init(&iep->ep_list);

			if (list_empty(&ihcd->async_list))
				break;
		}

		if (tmp == &ihcd->async_list)
			tmp = tmp->next;
		iep = list_entry(tmp, struct ice40_ep, ep_list);
	}
out:
	spin_unlock_irqrestore(&ihcd->lock, flags);
}

static int
ice40_urb_enqueue(struct usb_hcd *hcd, struct urb *urb, gfp_t mem_flags)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);
	struct usb_device *udev = urb->dev;
	struct usb_host_endpoint *ep = urb->ep;
	bool is_out = usb_pipeout(urb->pipe);
	u8 epnum = usb_pipeendpoint(urb->pipe);
	struct ice40_ep *iep;
	unsigned long flags;
	int ret;

	/*
	 * This bridge chip supports only Full-speed. So ISO is not
	 * supported. Interrupt support is not implemented as there
	 * is no use case.
	 */
	if (usb_pipeisoc(urb->pipe) || usb_pipeint(urb->pipe)) {
		pr_debug("iso and int xfers not supported\n");
		ret = -ENOTSUPP;
		goto out;
	}

	spin_lock_irqsave(&ihcd->lock, flags);

	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	if (ret)
		goto rel_lock;

	trace_ice40_urb_enqueue(urb);

	iep = ep->hcpriv;
	if (!iep) {
		iep = kzalloc(sizeof(struct ice40_ep), GFP_ATOMIC);
		if (!iep) {
			pr_debug("fail to allocate iep\n");
			ret = -ENOMEM;
			goto unlink;
		}
		ep->hcpriv = iep;
		INIT_LIST_HEAD(&iep->ep_list);
		iep->ep = ep;
		usb_settoggle(udev, epnum, is_out, 0);
		if (usb_pipecontrol(urb->pipe))
			ihcd->ep0_state = SETUP_PHASE;
	}

	/*
	 * We expect the interface driver to clear the stall condition
	 * before queueing another URB. For example mass storage
	 * device may STALL a bulk endpoint for un-supported command.
	 * The storage driver clear the STALL condition before queueing
	 * another URB.
	 */
	iep->halted = false;
	if (list_empty(&iep->ep_list))
		list_add_tail(&iep->ep_list, &ihcd->async_list);

	queue_work(ihcd->wq, &ihcd->async_work);

	spin_unlock_irqrestore(&ihcd->lock, flags);

	return 0;
unlink:
	usb_hcd_unlink_urb_from_ep(hcd, urb);
rel_lock:
	spin_unlock_irqrestore(&ihcd->lock, flags);
out:
	return ret;
}

static int
ice40_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);
	struct usb_host_endpoint *ep = urb->ep;
	struct ice40_ep *iep;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&ihcd->lock, flags);

	ret = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (ret)
		goto rel_lock;

	trace_ice40_urb_dequeue(urb);
	iep = ep->hcpriv;

	/*
	 * If the endpoint is not in asynchronous schedule, complete
	 * the URB immediately. Otherwise mark it as being unlinked.
	 * The asynchronous schedule work will take care of completing
	 * the URB when this endpoint is encountered during traversal.
	 */
	if (list_empty(&iep->ep_list))
		ice40_complete_urb(hcd, urb, status);
	else
		iep->unlinking = true;

rel_lock:
	spin_unlock_irqrestore(&ihcd->lock, flags);
	return ret;
}

static void
ice40_endpoint_disable(struct usb_hcd *hcd, struct usb_host_endpoint *ep)
{
	struct ice40_ep	*iep = ep->hcpriv;

	/*
	 * If there is no I/O on this endpoint before, ep->hcpriv
	 * will be NULL. nothing to do in this case.
	 */
	if (!iep)
		return;

	if (!list_empty(&ep->urb_list))
		pr_err("trying to disable an non-empty endpoint\n");

	kfree(iep);
	ep->hcpriv = NULL;
}


static int ice40_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);
	int ret = 0;

	/*
	 * core calls hub_status_method during suspend/resume.
	 * return 0 if there is no port change. pcd_pending
	 * is set to true when a device is connected and line
	 * state is sampled via debugfs command. clear this
	 * flag after returning the port change status.
	 */
	if (ihcd->pcd_pending) {
		*buf = (1 << 1);
		ret = 1;
		ihcd->pcd_pending = false;
	}

	return ret;
}

static void ice40_hub_descriptor(struct usb_hub_descriptor *desc)
{
	/* There is nothing special about us!! */
	desc->bDescLength = 9;
	desc->bDescriptorType = 0x29;
	desc->bNbrPorts = 1;
	desc->wHubCharacteristics = cpu_to_le16(HUB_CHAR_NO_LPSM |
				HUB_CHAR_NO_OCPM);
	desc->bPwrOn2PwrGood = 0;
	desc->bHubContrCurrent = 0;
	desc->u.hs.DeviceRemovable[0] = 0;
	desc->u.hs.DeviceRemovable[1] = ~0;
}

static int
ice40_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
			u16 wIndex, char *buf, u16 wLength)
{
	int ret = 0;
	u8 ctrl;
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);

	/*
	 * We have only 1 port. No special locking is required while
	 * handling root hub commands. The bridge chip does not maintain
	 * any port states. Maintain different port states in software.
	 */
	switch (typeReq) {
	case ClearPortFeature:
		if (wIndex != 1 || wLength != 0)
			goto error;
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			/*
			 * The device is resumed as part of the root hub
			 * resume to simplify the resume sequence. so
			 * we may simply return from here. If device is
			 * resumed before root hub is suspended, this
			 * flags will be cleared here.
			 */
			if (!(ihcd->port_flags & USB_PORT_STAT_SUSPEND))
				break;
			ihcd->port_flags &= ~USB_PORT_STAT_SUSPEND;
			break;
		case USB_PORT_FEAT_ENABLE:
			ihcd->port_flags &= ~USB_PORT_STAT_ENABLE;
			break;
		case USB_PORT_FEAT_POWER:
			ihcd->port_flags &= ~USB_PORT_STAT_POWER;
			break;
		case USB_PORT_FEAT_C_CONNECTION:
			ihcd->port_flags &= ~(USB_PORT_STAT_C_CONNECTION << 16);
			break;
		case USB_PORT_FEAT_C_ENABLE:
		case USB_PORT_FEAT_C_SUSPEND:
		case USB_PORT_FEAT_C_OVER_CURRENT:
		case USB_PORT_FEAT_C_RESET:
			/* nothing special here */
			break;
		default:
			goto error;
		}
		break;
	case GetHubDescriptor:
		ice40_hub_descriptor((struct usb_hub_descriptor *) buf);
		break;
	case GetHubStatus:
		put_unaligned_le32(0, buf);
		break;
	case GetPortStatus:
		if (wIndex != 1)
			goto error;

		/*
		 * Core resets the device and requests port status to
		 * stop the reset signaling. If there is a reset in
		 * progress, finish it here.
		 */
		ctrl = ice40_spi_reg_read(ihcd, CTRL0_REG);
		if (!(ctrl & RESET_CTRL))
			ihcd->port_flags &= ~USB_PORT_STAT_RESET;

		put_unaligned_le32(ihcd->port_flags, buf);
		break;
	case SetPortFeature:
		if (wIndex != 1 || wLength != 0)
			goto error;
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			if (ihcd->port_flags & USB_PORT_STAT_RESET)
				goto error;
			if (!(ihcd->port_flags & USB_PORT_STAT_ENABLE))
				goto error;
			/* SOFs will be stopped during root hub suspend */
			ihcd->port_flags |= USB_PORT_STAT_SUSPEND;
			break;
		case USB_PORT_FEAT_POWER:
			ihcd->port_flags |= USB_PORT_STAT_POWER;
			break;
		case USB_PORT_FEAT_RESET:
			/* Good time to enable the port */
			ice40_spi_reg_write(ihcd, ihcd->ctrl0 |
					RESET_CTRL, CTRL0_REG);
			ihcd->port_flags |= USB_PORT_STAT_RESET;
			ihcd->port_flags |= USB_PORT_STAT_ENABLE;
			break;
		default:
			goto error;
		}
		break;
	default:
error:
		/* "protocol stall" on error */
		ret = -EPIPE;
	}

	trace_ice40_hub_control(typeReq, wValue, wIndex, wLength, ret);
	return ret;
}

static void ice40_spi_power_off(struct ice40_hcd *ihcd);
static int ice40_bus_suspend(struct usb_hcd *hcd)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);

	trace_ice40_bus_suspend(0); /* start */

	/* This happens only during debugging */
	if (!ihcd->devnum) {
		pr_debug("device still not connected. abort suspend\n");
		trace_ice40_bus_suspend(2); /* failure */
		return -EAGAIN;
	}
	/*
	 * Stop sending the SOFs on downstream port. The device
	 * finds the bus idle and enter suspend. The device
	 * takes ~3 msec to enter suspend.
	 */
	ihcd->ctrl0 &= ~SOFEN_CTRL;
	ice40_spi_reg_write(ihcd, ihcd->ctrl0, CTRL0_REG);
	usleep_range(4500, 5000);

	/*
	 * Power collapse the bridge chip to avoid the leakage
	 * current.
	 */
	ice40_spi_power_off(ihcd);

	trace_ice40_bus_suspend(1); /* successful */
	pm_relax(&ihcd->spi->dev);
	return 0;
}

static int ice40_spi_load_fw(struct ice40_hcd *ihcd);
static int ice40_bus_resume(struct usb_hcd *hcd)
{
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);
	u8 ctrl0;
	int ret;

	pm_stay_awake(&ihcd->spi->dev);
	trace_ice40_bus_resume(0); /* start */
	/*
	 * Power up the bridge chip and load the configuration file.
	 * Re-program the previous settings. For now we need to
	 * update the device address only.
	 */
	ice40_spi_load_fw(ihcd);
	ice40_spi_reg_write(ihcd, ihcd->devnum, FADDR_REG);
	ihcd->wblen0 = ~0;

	/*
	 * Program the bridge chip to drive resume signaling. The SOFs
	 * are automatically transmitted after resume completion. It
	 * will take ~20 msec for resume completion.
	 */
	ice40_spi_reg_write(ihcd, ihcd->ctrl0 | RESUME_CTRL, CTRL0_REG);
	usleep_range(20000, 21000);
	ret = ice40_handshake(ihcd, CTRL0_REG, RESUME_CTRL, 0, 5000);
	if (ret) {
		pr_err("resume failed\n");
		trace_ice40_bus_resume(2); /* failure */
		return -ENODEV;
	}

	ctrl0 = ice40_spi_reg_read(ihcd, CTRL0_REG);
	if (!(ctrl0 & SOFEN_CTRL)) {
		pr_err("SOFs are not transmitted after resume\n");
		trace_ice40_bus_resume(3); /* failure */
		return -ENODEV;
	}

	ihcd->port_flags &= ~USB_PORT_STAT_SUSPEND;
	ihcd->ctrl0 |= SOFEN_CTRL;

	trace_ice40_bus_resume(1); /* success */
	return 0;
}

static void ice40_set_autosuspend_delay(struct usb_device *dev)
{
	/*
	 * Immediate suspend for root hub and 500 msec auto-suspend
	 * timeout for the card.
	 */
	if (!dev->parent)
		pm_runtime_set_autosuspend_delay(&dev->dev, 0);
	else
		pm_runtime_set_autosuspend_delay(&dev->dev, 500);
}

static const struct hc_driver ice40_hc_driver = {
	.description = hcd_name,
	.product_desc = "ICE40 SPI Host Controller",
	.hcd_priv_size = sizeof(struct ice40_hcd *),
	.flags = HCD_USB11,

	/* setup and clean up */
	.reset = ice40_reset,
	.start = ice40_run,
	.stop = ice40_stop,

	/* endpoint and I/O routines */
	.urb_enqueue = ice40_urb_enqueue,
	.urb_dequeue = ice40_urb_dequeue,
	.endpoint_disable = ice40_endpoint_disable,

	/* Root hub operations */
	.hub_status_data = ice40_hub_status_data,
	.hub_control = ice40_hub_control,
	.bus_suspend = ice40_bus_suspend,
	.bus_resume = ice40_bus_resume,

	.set_autosuspend_delay = ice40_set_autosuspend_delay,
};

static int ice40_spi_parse_dt(struct ice40_hcd *ihcd)
{
	struct device_node *node = ihcd->spi->dev.of_node;
	int ret = 0;

	if (!node) {
		pr_err("device specific info missing\n");
		ret = -ENODEV;
		goto out;
	}

	ihcd->reset_gpio = of_get_named_gpio(node, "lattice,reset-gpio", 0);
	if (ihcd->reset_gpio < 0) {
		pr_err("reset gpio is missing\n");
		ret = ihcd->reset_gpio;
		goto out;
	}

	ihcd->slave_select_gpio = of_get_named_gpio(node,
				"lattice,slave-select-gpio", 0);
	if (ihcd->slave_select_gpio < 0) {
		pr_err("slave select gpio is missing\n");
		ret = ihcd->slave_select_gpio;
		goto out;
	}

	ihcd->config_done_gpio = of_get_named_gpio(node,
				"lattice,config-done-gpio", 0);
	if (ihcd->config_done_gpio < 0) {
		pr_err("config done gpio is missing\n");
		ret = ihcd->config_done_gpio;
		goto out;
	}

	ihcd->vcc_en_gpio = of_get_named_gpio(node, "lattice,vcc-en-gpio", 0);
	if (ihcd->vcc_en_gpio < 0) {
		pr_err("vcc enable gpio is missing\n");
		ret = ihcd->vcc_en_gpio;
		goto out;
	}

	/*
	 * When clk-en-gpio is present, it is used to enable the 19.2 MHz
	 * clock from MSM to the bridge chip. Otherwise on-board clock
	 * is used.
	 */
	ihcd->clk_en_gpio = of_get_named_gpio(node, "lattice,clk-en-gpio", 0);
	if (ihcd->clk_en_gpio < 0)
		ihcd->clk_en_gpio = 0;
out:
	return ret;
}

static void ice40_spi_power_off(struct ice40_hcd *ihcd)
{
	if (!ihcd->powered)
		return;

	gpio_direction_output(ihcd->vcc_en_gpio, 0);
	regulator_disable(ihcd->core_vcc);
	regulator_disable(ihcd->spi_vcc);
	if (ihcd->gpio_vcc)
		regulator_disable(ihcd->gpio_vcc);
	if (ihcd->clk_en_gpio)
		gpio_direction_output(ihcd->clk_en_gpio, 0);

	ihcd->powered = false;
}

static int ice40_spi_power_up(struct ice40_hcd *ihcd)
{
	int ret;

	if (ihcd->clk_en_gpio) {
		ret = gpio_direction_output(ihcd->clk_en_gpio, 1);
		if (ret < 0) {
			pr_err("fail to enabel clk %d\n", ret);
			goto out;
		}
	}

	if (ihcd->gpio_vcc) {
		ret = regulator_enable(ihcd->gpio_vcc); /* 1.8 V */
		if (ret < 0) {
			pr_err("fail to enable gpio vcc\n");
			goto disable_clk;
		}
	}

	ret = regulator_enable(ihcd->spi_vcc); /* 1.8 V */
	if (ret < 0) {
		pr_err("fail to enable spi vcc\n");
		goto disable_gpio_vcc;
	}

	ret = regulator_enable(ihcd->core_vcc); /* 1.2 V */
	if (ret < 0) {
		pr_err("fail to enable core vcc\n");
		goto disable_spi_vcc;
	}

	ret = gpio_direction_output(ihcd->vcc_en_gpio, 1);
	if (ret < 0) {
		pr_err("fail to assert vcc gpio\n");
		goto disable_core_vcc;
	}

	ihcd->powered = true;

	return 0;

disable_core_vcc:
	regulator_disable(ihcd->core_vcc);
disable_spi_vcc:
	regulator_disable(ihcd->spi_vcc);
disable_gpio_vcc:
	if (ihcd->gpio_vcc)
		regulator_disable(ihcd->gpio_vcc);
disable_clk:
	if (ihcd->clk_en_gpio)
		gpio_direction_output(ihcd->clk_en_gpio, 0);
out:
	return ret;
}

static struct gpiomux_setting slave_select_setting = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_NONE,
	.dir = GPIOMUX_OUT_LOW,
};

static int ice40_spi_cache_fw(struct ice40_hcd *ihcd)
{
	const struct firmware *fw;
	void *buf;
	size_t buf_len;
	int ret;

	ret = request_firmware(&fw, fw_name, &ihcd->spi->dev);
	if (ret < 0) {
		pr_err("fail to get the firmware\n");
		goto out;
	}

	pr_debug("received firmware size = %zu\n", fw->size);

	/*
	 * The bridge expects additional clock cycles after
	 * receiving the configuration data. We don't have a
	 * direct control over SPI clock. Add extra bytes
	 * to the confiration data.
	 */
	buf_len = fw->size + 16;
	buf = devm_kzalloc(&ihcd->spi->dev, buf_len, GFP_KERNEL);
	if (!buf) {
		pr_err("fail to allocate firmware buffer\n");
		ret = -ENOMEM;
		goto release;
	}

	/*
	 * The firmware buffer can not be used for DMA as it
	 * is not physically contiguous. We copy the data
	 * in kmalloc buffer. This buffer will be freed only
	 * during unbind or rmmod.
	 */
	memcpy(buf, fw->data, fw->size);
	release_firmware(fw);

	/*
	 * The bridge supports only 25 MHz during configuration
	 * file loading.
	 */
	ihcd->fmsg_xfr[0].tx_buf = buf;
	ihcd->fmsg_xfr[0].len = buf_len;
	ihcd->fmsg_xfr[0].speed_hz = 25000000;

	return 0;

release:
	release_firmware(fw);
out:
	return ret;
}

static int ice40_spi_load_fw(struct ice40_hcd *ihcd)
{
	int ret, i;
	struct gpiomux_setting active_old_setting, suspend_old_setting;

	ret = gpio_direction_output(ihcd->reset_gpio, 0);
	if (ret  < 0) {
		pr_err("fail to assert reset %d\n", ret);
		goto out;
	}

	ret = gpio_direction_output(ihcd->vcc_en_gpio, 0);
	if (ret < 0) {
		pr_err("fail to de-assert vcc_en gpio %d\n", ret);
		goto out;
	}

	/*
	 * The bridge chip samples the chip select signal during
	 * power-up. If it is low, it enters SPI slave mode and
	 * accepts the configuration data from us. The chip
	 * select signal is managed by the SPI controller driver.
	 * We temporarily override the chip select config to
	 * drive it low. The SPI bus needs to be locked down during
	 * this period to avoid other slave data going to our
	 * bridge chip. Disable the SPI runtime suspend for exclusive
	 * chip select access.
	 */
	pm_runtime_get_sync(ihcd->spi->master->dev.parent);

	spi_bus_lock(ihcd->spi->master);

	ret = msm_gpiomux_write(ihcd->slave_select_gpio, GPIOMUX_SUSPENDED,
			&slave_select_setting, &suspend_old_setting);
	if (ret < 0) {
		pr_err("fail to override suspend setting and select slave %d\n",
				ret);
		spi_bus_unlock(ihcd->spi->master);
		pm_runtime_put_noidle(ihcd->spi->master->dev.parent);
		goto out;
	}

	ret = msm_gpiomux_write(ihcd->slave_select_gpio, GPIOMUX_ACTIVE,
			&slave_select_setting, &active_old_setting);
	if (ret < 0) {
		pr_err("fail to override active setting and select slave %d\n",
				ret);
		spi_bus_unlock(ihcd->spi->master);
		pm_runtime_put_noidle(ihcd->spi->master->dev.parent);
		goto out;
	}

	ret = ice40_spi_power_up(ihcd);
	if (ret < 0) {
		pr_err("fail to power up the chip\n");
		spi_bus_unlock(ihcd->spi->master);
		pm_runtime_put_noidle(ihcd->spi->master->dev.parent);
		goto out;
	}


	/*
	 * The databook says 1200 usec is required before the
	 * chip becomes ready for the SPI transfer.
	 */
	usleep_range(1200, 1250);

	ret = msm_gpiomux_write(ihcd->slave_select_gpio, GPIOMUX_SUSPENDED,
			&suspend_old_setting, NULL);
	if (ret < 0) {
		pr_err("fail to rewrite suspend setting %d\n", ret);
		spi_bus_unlock(ihcd->spi->master);
		pm_runtime_put_noidle(ihcd->spi->master->dev.parent);
		goto power_off;
	}

	ret = msm_gpiomux_write(ihcd->slave_select_gpio, GPIOMUX_ACTIVE,
			&active_old_setting, NULL);
	if (ret < 0) {
		pr_err("fail to rewrite active setting %d\n", ret);
		spi_bus_unlock(ihcd->spi->master);
		pm_runtime_put_noidle(ihcd->spi->master->dev.parent);
		goto power_off;
	}

	pm_runtime_put_noidle(ihcd->spi->master->dev.parent);

	ret = spi_sync_locked(ihcd->spi, ihcd->fmsg);

	spi_bus_unlock(ihcd->spi->master);

	if (ret < 0) {
		pr_err("spi write failed\n");
		goto power_off;
	}

	for (i = 0; i < 1000; i++) {
		ret = gpio_get_value(ihcd->config_done_gpio);
		if (ret) {
			pr_debug("config done asserted %d\n", i);
			break;
		}
		udelay(1);
	}

	if (ret <= 0) {
		pr_err("config done not asserted\n");
		ret = -ENODEV;
		goto power_off;
	}

	ret = gpio_direction_output(ihcd->reset_gpio, 1);
	if (ret  < 0) {
		pr_err("fail to assert reset %d\n", ret);
		goto power_off;
	}
	udelay(50);

	ret = ice40_spi_reg_read(ihcd, XFRST_REG);
	pr_debug("XFRST val is %x\n", ret);
	if (!(ret & PLLOK)) {
		pr_err("The PLL2 is not synchronized\n");
		goto power_off;
	}

	pr_info("Firmware load success\n");

	return 0;

power_off:
	ice40_spi_power_off(ihcd);
out:
	return ret;
}

static int ice40_spi_init_regulators(struct ice40_hcd *ihcd)
{
	int ret;

	ihcd->spi_vcc = devm_regulator_get(&ihcd->spi->dev, "spi-vcc");
	if (IS_ERR(ihcd->spi_vcc)) {
		ret = PTR_ERR(ihcd->spi_vcc);
		if (ret != -EPROBE_DEFER)
			pr_err("fail to get spi-vcc %d\n", ret);
		goto out;
	}

	ret = regulator_set_voltage(ihcd->spi_vcc, 1800000, 1800000);
	if (ret < 0) {
		pr_err("fail to set spi-vcc %d\n", ret);
		goto out;
	}

	ihcd->core_vcc = devm_regulator_get(&ihcd->spi->dev, "core-vcc");
	if (IS_ERR(ihcd->core_vcc)) {
		ret = PTR_ERR(ihcd->core_vcc);
		if (ret != -EPROBE_DEFER)
			pr_err("fail to get core-vcc %d\n", ret);
		goto out;
	}

	ret = regulator_set_voltage(ihcd->core_vcc, 1200000, 1200000);
	if (ret < 0) {
		pr_err("fail to set core-vcc %d\n", ret);
		goto out;
	}

	if (!of_get_property(ihcd->spi->dev.of_node, "gpio-supply", NULL))
		goto out;

	ihcd->gpio_vcc = devm_regulator_get(&ihcd->spi->dev, "gpio");
	if (IS_ERR(ihcd->gpio_vcc)) {
		ret = PTR_ERR(ihcd->gpio_vcc);
		if (ret != -EPROBE_DEFER)
			pr_err("fail to get gpio_vcc %d\n", ret);
		goto out;
	}

	ret = regulator_set_voltage(ihcd->gpio_vcc, 1800000, 1800000);
	if (ret < 0) {
		pr_err("fail to set gpio_vcc %d\n", ret);
		goto out;
	}

out:
	return ret;
}

static int ice40_spi_request_gpios(struct ice40_hcd *ihcd)
{
	int ret;

	ret = devm_gpio_request(&ihcd->spi->dev, ihcd->reset_gpio,
				"ice40_reset");
	if (ret < 0) {
		pr_err("fail to request reset gpio\n");
		goto out;
	}

	ret = devm_gpio_request(&ihcd->spi->dev, ihcd->config_done_gpio,
				"ice40_config_done");
	if (ret < 0) {
		pr_err("fail to request config_done gpio\n");
		goto out;
	}

	ret = devm_gpio_request(&ihcd->spi->dev, ihcd->vcc_en_gpio,
				"ice40_vcc_en");
	if (ret < 0) {
		pr_err("fail to request vcc_en gpio\n");
		goto out;
	}

	if (ihcd->clk_en_gpio) {

		ret = devm_gpio_request(&ihcd->spi->dev, ihcd->clk_en_gpio,
					"ice40_clk_en");
		if (ret < 0)
			pr_err("fail to request clk_en gpio\n");
	}

out:
	return ret;
}

static int
ice40_spi_init_one_xfr(struct ice40_hcd *ihcd, enum ice40_xfr_type type)
{
	struct spi_message **m;
	struct spi_transfer **t;
	int n;

	switch (type) {
	case FIRMWARE_XFR:
		m = &ihcd->fmsg;
		t = &ihcd->fmsg_xfr;
		n = 1;
		break;
	case REG_WRITE_XFR:
		m = &ihcd->wmsg;
		t = &ihcd->wmsg_xfr;
		n = 1;
		break;
	case REG_READ_XFR:
		m = &ihcd->rmsg;
		t = &ihcd->rmsg_xfr;
		n = 1;
		break;
	case SETUP_XFR:
		m = &ihcd->setup_msg;
		t = &ihcd->setup_xfr;
		n = 2;
		break;
	case DATA_IN_XFR:
		m = &ihcd->in_msg;
		t = &ihcd->in_xfr;
		n = 2;
		break;
	case DATA_OUT_XFR:
		m = &ihcd->out_msg;
		t = &ihcd->out_xfr;
		n = 2;
		break;
	default:
		return -EINVAL;
	}

	*m = devm_kzalloc(&ihcd->spi->dev, sizeof(**m), GFP_KERNEL);
	if (*m == NULL)
		goto out;

	*t = devm_kzalloc(&ihcd->spi->dev, n * sizeof(**t), GFP_KERNEL);
	if (*t == NULL)
		goto out;

	spi_message_init_with_transfers(*m, *t, n);

	return 0;
out:
	return -ENOMEM;
}

static int ice40_spi_init_xfrs(struct ice40_hcd *ihcd)
{
	int ret = -ENOMEM;

	ret = ice40_spi_init_one_xfr(ihcd, FIRMWARE_XFR);
	if (ret < 0)
		goto out;

	ret = ice40_spi_init_one_xfr(ihcd, REG_WRITE_XFR);
	if (ret < 0)
		goto out;

	ihcd->w_tx_buf = devm_kzalloc(&ihcd->spi->dev, 2, GFP_KERNEL);
	if (!ihcd->w_tx_buf)
		goto out;

	ihcd->w_rx_buf = devm_kzalloc(&ihcd->spi->dev, 2, GFP_KERNEL);
	if (!ihcd->w_rx_buf)
		goto out;

	ihcd->wmsg_xfr[0].tx_buf = ihcd->w_tx_buf;
	ihcd->wmsg_xfr[0].rx_buf = ihcd->w_rx_buf;
	ihcd->wmsg_xfr[0].len = 2;

	ret = ice40_spi_init_one_xfr(ihcd, REG_READ_XFR);
	if (ret < 0)
		goto out;

	ihcd->r_tx_buf = devm_kzalloc(&ihcd->spi->dev, 3, GFP_KERNEL);
	if (!ihcd->r_tx_buf)
		goto out;

	ihcd->r_rx_buf = devm_kzalloc(&ihcd->spi->dev, 3, GFP_KERNEL);
	if (!ihcd->r_rx_buf)
		goto out;

	ihcd->rmsg_xfr[0].tx_buf = ihcd->r_tx_buf;
	ihcd->rmsg_xfr[0].rx_buf = ihcd->r_rx_buf;
	ihcd->rmsg_xfr[0].len = 3;

	ret = ice40_spi_init_one_xfr(ihcd, SETUP_XFR);
	if (ret < 0)
		goto out;

	ihcd->setup_buf = devm_kzalloc(&ihcd->spi->dev, 1, GFP_KERNEL);
	if (!ihcd->setup_buf)
		goto out;
	ihcd->setup_xfr[0].tx_buf = ihcd->setup_buf;
	ihcd->setup_xfr[0].len = 1;

	ret = ice40_spi_init_one_xfr(ihcd, DATA_IN_XFR);
	if (ret < 0)
		goto out;
	ihcd->in_buf = devm_kzalloc(&ihcd->spi->dev, 2, GFP_KERNEL);
	if (!ihcd->in_buf)
		goto out;
	ihcd->in_xfr[0].tx_buf = ihcd->in_buf;
	ihcd->in_xfr[0].len = 2;

	ret = ice40_spi_init_one_xfr(ihcd, DATA_OUT_XFR);
	if (ret < 0)
		goto out;
	ihcd->out_buf = devm_kzalloc(&ihcd->spi->dev, 1, GFP_KERNEL);
	if (!ihcd->out_buf)
		goto out;
	ihcd->out_xfr[0].tx_buf = ihcd->out_buf;
	ihcd->out_xfr[0].len = 1;

	return 0;

out:
	return -ENOMEM;
}

static int ice40_dbg_cmd_open(struct inode *inode, struct file *file)
{
	return single_open(file, NULL, inode->i_private);
}

static ssize_t ice40_dbg_cmd_write(struct file *file, const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct ice40_hcd *ihcd = s->private;
	char buf[32];
	int ret;
	u8 status, addr;

	memset(buf, 0x00, sizeof(buf));

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count))) {
		ret = -EFAULT;
		goto out;
	}

	if (!strcmp(buf, "poll")) {
		if (!HCD_RH_RUNNING(ihcd->hcd)) {
			ret = -EAGAIN;
			goto out;
		}
		/*
		 * The bridge chip supports interrupt for device
		 * connect and disconnect. We don;t have a real
		 * use case of connect/disconnect. This debugfs
		 * interface provides a way to enumerate the
		 * attached device.
		 */
		ice40_spi_reg_write(ihcd, ihcd->ctrl0 |
				DET_BUS_CTRL, CTRL0_REG);
		ice40_handshake(ihcd, CTRL0_REG, DET_BUS_CTRL, 0, 5000);
		status = ice40_spi_reg_read(ihcd, XFRST_REG);
		if ((status & DPST)) {
			ihcd->port_flags |= USB_PORT_STAT_CONNECTION;
			ihcd->port_flags |= USB_PORT_STAT_C_CONNECTION << 16;
			ihcd->pcd_pending = true;
			usb_hcd_poll_rh_status(ihcd->hcd);
		} else if (ihcd->port_flags & USB_PORT_STAT_CONNECTION) {
			ihcd->port_flags &= ~USB_PORT_STAT_ENABLE;
			ihcd->port_flags &= ~USB_PORT_STAT_CONNECTION;
			ihcd->port_flags |= (USB_PORT_STAT_C_CONNECTION << 16);
			ihcd->pcd_pending = true;
			usb_hcd_poll_rh_status(ihcd->hcd);
		}
	} else if (!strcmp(buf, "rwtest")) {
		ihcd->devnum = 1;
		ice40_spi_reg_write(ihcd, 0x1, FADDR_REG);
		addr = ice40_spi_reg_read(ihcd, FADDR_REG);
		pr_info("addr written was 0x1 read as %x\n", addr);
	} else if (!strcmp(buf, "force_disconnect")) {
		if (!HCD_RH_RUNNING(ihcd->hcd)) {
			ret = -EAGAIN;
			goto out;
		}
		/*
		 * Forcfully disconnect the device. This is required
		 * for simulating the disconnect on a USB port which
		 * does not have pull-down resistors.
		 */
		ihcd->port_flags &= ~USB_PORT_STAT_ENABLE;
		ihcd->port_flags &= ~USB_PORT_STAT_CONNECTION;
		ihcd->port_flags |= (USB_PORT_STAT_C_CONNECTION << 16);
		ihcd->pcd_pending = true;
		usb_hcd_poll_rh_status(ihcd->hcd);
	} else if (!strcmp(buf, "config_test")) {
		ice40_spi_power_off(ihcd);
		ret = ice40_spi_load_fw(ihcd);
		if (ret) {
			pr_err("config load failed\n");
			goto out;
		}
	} else {
		ret = -EINVAL;
		goto out;
	}

	ret = count;
out:
	return ret;
}

const struct file_operations ice40_dbg_cmd_ops = {
	.open = ice40_dbg_cmd_open,
	.write = ice40_dbg_cmd_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ice40_debugfs_init(struct ice40_hcd *ihcd)
{
	struct dentry *dir;
	int ret = 0;

	dir = debugfs_create_dir("ice40_hcd", NULL);

	if (!dir || IS_ERR(dir)) {
		ret = -ENODEV;
		goto out;
	}

	ihcd->dbg_root = dir;

	dir = debugfs_create_file("command", S_IWUSR, ihcd->dbg_root, ihcd,
			&ice40_dbg_cmd_ops);

	if (!dir) {
		debugfs_remove_recursive(ihcd->dbg_root);
		ihcd->dbg_root = NULL;
		ret = -ENODEV;
	}

out:
	return ret;
}

static int ice40_spi_probe(struct spi_device *spi)
{
	struct ice40_hcd *ihcd;
	int ret;

	ihcd = devm_kzalloc(&spi->dev, sizeof(*ihcd), GFP_KERNEL);
	if (!ihcd) {
		pr_err("fail to allocate ihcd\n");
		ret = -ENOMEM;
		goto out;
	}
	ihcd->spi = spi;

	ret = ice40_spi_parse_dt(ihcd);
	if (ret) {
		pr_err("fail to parse dt node\n");
		goto out;
	}

	ret = ice40_spi_init_regulators(ihcd);
	if (ret) {
		pr_err("fail to init regulators\n");
		goto out;
	}

	ret = ice40_spi_request_gpios(ihcd);
	if (ret) {
		pr_err("fail to request gpios\n");
		goto out;
	}

	spin_lock_init(&ihcd->lock);
	INIT_LIST_HEAD(&ihcd->async_list);
	INIT_WORK(&ihcd->async_work, ice40_async_work);
	mutex_init(&ihcd->wlock);
	mutex_init(&ihcd->rlock);

	/*
	 * Enable all our trace points. Useful in debugging card
	 * enumeration issues.
	 */
	ret = trace_set_clr_event(__stringify(TRACE_SYSTEM), NULL, 1);
	if (ret < 0)
		pr_err("fail to enable trace points with %d\n", ret);

	ihcd->wq = create_singlethread_workqueue("ice40_wq");
	if (!ihcd->wq) {
		pr_err("fail to create workqueue\n");
		ret = -ENOMEM;
		goto destroy_mutex;
	}

	ret = ice40_spi_init_xfrs(ihcd);
	if (ret) {
		pr_err("fail to init spi xfrs %d\n", ret);
		goto destroy_wq;
	}

	ret = ice40_spi_cache_fw(ihcd);
	if (ret) {
		pr_err("fail to cache fw %d\n", ret);
		goto destroy_wq;
	}

	ret = ice40_spi_load_fw(ihcd);
	if (ret) {
		pr_err("fail to load fw %d\n", ret);
		goto destroy_wq;
	}

	ihcd->hcd = usb_create_hcd(&ice40_hc_driver, &spi->dev, "ice40");
	if (!ihcd->hcd) {
		pr_err("fail to alloc hcd\n");
		ret = -ENOMEM;
		goto power_off;
	}
	*((struct ice40_hcd **) ihcd->hcd->hcd_priv) = ihcd;

	ret = usb_add_hcd(ihcd->hcd, 0, 0);

	if (ret < 0) {
		pr_err("fail to add HCD\n");
		goto put_hcd;
	}

	ice40_debugfs_init(ihcd);

	/*
	 * We manage the power states of the bridge chip
	 * as part of root hub suspend/resume. We don't
	 * need to implement any additional runtime PM
	 * methods.
	 */
	pm_runtime_no_callbacks(&spi->dev);
	pm_runtime_set_active(&spi->dev);
	pm_runtime_enable(&spi->dev);

	/*
	 * This does not mean bridge chip can wakeup the
	 * system from sleep. It's activity can prevent
	 * or abort the system sleep. The device_init_wakeup
	 * creates the wakeup source for us which we will
	 * use to control system sleep.
	 */
	device_init_wakeup(&spi->dev, 1);
	pm_stay_awake(&spi->dev);

	pr_debug("success\n");

	return 0;

put_hcd:
	usb_put_hcd(ihcd->hcd);
power_off:
	ice40_spi_power_off(ihcd);
destroy_wq:
	destroy_workqueue(ihcd->wq);
destroy_mutex:
	mutex_destroy(&ihcd->rlock);
	mutex_destroy(&ihcd->wlock);
out:
	pr_info("ice40_spi_probe failed\n");
	return ret;
}

static int ice40_spi_remove(struct spi_device *spi)
{
	struct usb_hcd *hcd = spi_get_drvdata(spi);
	struct ice40_hcd *ihcd = hcd_to_ihcd(hcd);

	debugfs_remove_recursive(ihcd->dbg_root);

	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);
	destroy_workqueue(ihcd->wq);
	ice40_spi_power_off(ihcd);

	pm_runtime_disable(&spi->dev);
	pm_relax(&spi->dev);

	return 0;
}

static struct of_device_id ice40_spi_of_match_table[] = {
	{ .compatible = "lattice,ice40-spi-usb", },
	{},
};

static struct spi_driver ice40_spi_driver = {
	.driver = {
		.name =		"ice40_spi",
		.owner =	THIS_MODULE,
		.of_match_table = ice40_spi_of_match_table,
	},
	.probe =	ice40_spi_probe,
	.remove =	ice40_spi_remove,
};

module_spi_driver(ice40_spi_driver);

MODULE_DESCRIPTION("ICE40 FPGA based SPI-USB bridge HCD");
MODULE_LICENSE("GPL v2");

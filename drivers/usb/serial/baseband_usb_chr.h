/*
 * baseband_usb_chr.h
 *
 * USB character driver to communicate with baseband modems.
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __BASEBAND_USB_CHR_H__
#define __BASEBAND_USB_CHR_H__

#define BASEBAND_USB_CHR_DEV_NAME		"baseband_usb_chr"
#define BASEBAND_USB_CHR_DEV_MAJOR		66

#ifndef USB_CHR_RX_BUFSIZ
#define USB_CHR_RX_BUFSIZ			(32*1024)
#endif  /* USB_CHR_RX_BUFSIZ */

#ifndef USB_CHR_TX_BUFSIZ
#define USB_CHR_TX_BUFSIZ			(32*1024)
#endif  /* USB_CHR_TX_BUFSIZ */

#ifndef USB_CHR_TIMEOUT
#define USB_CHR_TIMEOUT				5000 /* ms */
#endif  /* USB_CHR_TIMEOUT */

#ifndef BASEBAND_IPC_NUM_RX_BUF
#define BASEBAND_IPC_NUM_RX_BUF			1
#endif  /* BASEBAND_IPC_NUM_RX_BUF */

#ifndef BASEBAND_IPC_NUM_TX_BUF
#define BASEBAND_IPC_NUM_TX_BUF			1
#endif  /* BASEBAND_IPC_NUM_TX_BUF */

#ifndef BASEBAND_IPC_BUFSIZ
#define BASEBAND_IPC_BUFSIZ			65536
#endif  /* BASEBAND_IPC_BUFSIZ */

struct baseband_ipc {
	/* rx / tx data */
	struct semaphore buf_sem;
	struct {
		/* linked list of data buffers */
		struct list_head buf;
		/* wait queue of processes trying to access data buffers */
		wait_queue_head_t wait;
	} rx, tx, rx_free, tx_free;
	unsigned char *ipc_rx;
	unsigned char *ipc_tx;
	/* work queue
	 * - queued per ipc transaction
	 * - initiated by either:
	 *   = interrupt on gpio line (rx data available)
	 *   = tx data packet being added to tx linked list
	 */
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct work_struct rx_work;
	struct work_struct tx_work;
};

struct baseband_ipc_buf {
	struct list_head list;
	/* data buffer */
	unsigned char data[BASEBAND_IPC_BUFSIZ];
	/* offset of first data byte */
	size_t offset;
	/* number of valid data bytes */
	size_t count;
};

struct baseband_usb {
	struct baseband_ipc *ipc;
	unsigned int ref;
	struct {
		struct usb_driver *driver;
		struct usb_device *device;
		struct usb_interface *interface;
		struct {
			struct {
				unsigned int in;
				unsigned int out;
			} isoch, bulk, interrupt;
		} pipe;
		/* currently active rx urb */
		struct urb *rx_urb;
		/* currently active tx urb */
		struct urb *tx_urb;
	} usb;
};

#endif  /* __BASEBAND_USB_CHR_H__ */


/*
 * baseband_usb_chr.c
 *
 * USB character driver to communicate with baseband modems.
 *
 * Copyright (c) 2012, NVIDIA Corporation.  All rights reserved.
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/ioctls.h>
#include "baseband_usb_chr.h"

MODULE_LICENSE("GPL");

/* To add new usb devices, update
 * (1) baseband_usb_driver_id_table
 *     - usb vendor id / product id
 * (2) baseband_usb_driver_intf_table
 *     - usb interface number
 */

static struct usb_device_id baseband_usb_driver_id_table[] = {
	/* XMM modem #1 BOOT ROM */
	{ USB_DEVICE(0x058b, 0x0041), },
	/* XMM modem #2 BOOT ROM */
	{ USB_DEVICE(0x8087, 0x0716), },
	/* empty entry required to terminate list */
	{ },
};

static unsigned int baseband_usb_driver_intf_table[] = {
	/* XMM modem #1 BOOT ROM */
	0x01,
	/* XMM modem #2 BOOT ROM */
	0x00,
	/* empty entry required to terminate list */
	0x00,
};

MODULE_DEVICE_TABLE(usb, baseband_usb_driver_id_table);

static struct baseband_usb *baseband_usb_chr;
static struct usb_interface *probe_usb_intf;
static bool usb_device_connection;
static struct workqueue_struct *chr_ipc_wq;

static atomic_t g_rx_count = ATOMIC_INIT(0);

struct usb_chr_resource {
	unsigned char *ipc_rx;
	unsigned char *ipc_tx;
	unsigned char *rx_buf;
};
static struct usb_chr_resource usb_chr_res;

/* baseband ipc functions */

static void baseband_ipc_dump(const char *prefix, unsigned long int offset,
	const void *buf, size_t bufsiz)
{
	size_t i;

	for (i = 0; i < bufsiz; i += 16) {
		pr_debug("%s"
			"[%lx+%x] %p "
			"%02x %02x %02x %02x "
			"%02x %02x %02x %02x "
			"%02x %02x %02x %02x "
			"%02x %02x %02x %02x\n",
			prefix,
			offset,
			i,
			((const unsigned char *) buf) + i,
			(i + 0 < bufsiz) ? ((const unsigned char *) buf)[i+0]
				: 0xff,
			(i + 1 < bufsiz) ? ((const unsigned char *) buf)[i+1]
				: 0xff,
			(i + 2 < bufsiz) ? ((const unsigned char *) buf)[i+2]
				: 0xff,
			(i + 3 < bufsiz) ? ((const unsigned char *) buf)[i+3]
				: 0xff,
			(i + 4 < bufsiz) ? ((const unsigned char *) buf)[i+4]
				: 0xff,
			(i + 5 < bufsiz) ? ((const unsigned char *) buf)[i+5]
				: 0xff,
			(i + 6 < bufsiz) ? ((const unsigned char *) buf)[i+6]
				: 0xff,
			(i + 7 < bufsiz) ? ((const unsigned char *) buf)[i+7]
				: 0xff,
			(i + 8 < bufsiz) ? ((const unsigned char *) buf)[i+8]
				: 0xff,
			(i + 9 < bufsiz) ? ((const unsigned char *) buf)[i+9]
				: 0xff,
			(i + 10 < bufsiz) ? ((const unsigned char *) buf)[i+10]
				: 0xff,
			(i + 11 < bufsiz) ? ((const unsigned char *) buf)[i+11]
				: 0xff,
			(i + 12 < bufsiz) ? ((const unsigned char *) buf)[i+12]
				: 0xff,
			(i + 13 < bufsiz) ? ((const unsigned char *) buf)[i+13]
				: 0xff,
			(i + 14 < bufsiz) ? ((const unsigned char *) buf)[i+14]
				: 0xff,
			(i + 15 < bufsiz) ? ((const unsigned char *) buf)[i+15]
				: 0xff);
	}

}

static size_t peek_ipc_tx_bufsiz(struct baseband_ipc *ipc,
	size_t bufsiz)
{
	struct baseband_ipc_buf *ipc_buf, *ipc_buf_next;
	size_t tx_bufsiz;

	pr_debug("peek_ipc_tx_bufsiz\n");

	/* check input */
	if (!ipc) {
		pr_err("%s: !ipc\n", __func__);
		return 0;
	}

	/* acquire tx buffer semaphores */
	if (down_interruptible(&ipc->buf_sem)) {
		pr_err("peek_ipc_tx_bufsiz - "
			"cannot acquire buffer semaphore\n");
		return -ERESTARTSYS;
	}

	/* calculate maximum number of tx buffers which can be sent */
	tx_bufsiz = 0;
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->tx.buf, list)
	{
		pr_debug("peek_ipc_tx_bufsiz - "
			"ipc_buf %p ipc_buf->offset %x ipc_buf->count %x\n",
			ipc_buf, ipc_buf->offset, ipc_buf->count);
		if (ipc_buf->count > bufsiz - tx_bufsiz)
			break;
		else
			tx_bufsiz += ipc_buf->count;
	}

	/* release tx buffer semaphores */
	up(&ipc->buf_sem);

	return tx_bufsiz;
}

static size_t get_ipc_tx_buf(struct baseband_ipc *ipc,
	void *buf, size_t bufsiz)
{
	struct baseband_ipc_buf *ipc_buf, *ipc_buf_next;
	size_t tx_bufsiz;

	pr_debug("get_ipc_tx_buf\n");

	/* check input */
	if (!ipc || !buf) {
		pr_err("%s: !ipc || !buf\n", __func__);
		return 0;
	}
	if (!bufsiz)
		return 0;

	/* acquire tx buffer semaphores */
	if (down_interruptible(&ipc->buf_sem)) {
		pr_err("get_ipc_tx_buf - "
			"cannot acquire buffer semaphore\n");
		return -ERESTARTSYS;
	}

	/* get tx data from tx linked list */
	tx_bufsiz = 0;
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->tx.buf, list)
	{
		pr_debug("get_ipc_tx_buf - "
			"ipc_buf %p ipc_buf->offset %x ipc_buf->count %x\n",
			ipc_buf, ipc_buf->offset, ipc_buf->count);
		pr_debug("get_ipc_tx_buf - "
			"ipc_buf->data [0] %x [1] %x [2] %x [3] %x\n",
			ipc_buf->data[0],
			ipc_buf->data[1],
			ipc_buf->data[2],
			ipc_buf->data[3]);
		if (ipc_buf->count > bufsiz - tx_bufsiz) {
			/* copy part of tx buffer */
			memcpy(buf + tx_bufsiz,
				ipc_buf->data + ipc_buf->offset,
				bufsiz - tx_bufsiz);
			ipc_buf->offset += bufsiz - tx_bufsiz;
			ipc_buf->count -= bufsiz - tx_bufsiz;
			tx_bufsiz = bufsiz;
		} else {
			/* copy all data from tx buffer */
			memcpy(buf + tx_bufsiz,
				ipc_buf->data + ipc_buf->offset,
				ipc_buf->count);
			tx_bufsiz += ipc_buf->count;
			ipc_buf->offset = 0;
			ipc_buf->count = 0;
			/* add tx buffer to tx free list */
			list_move_tail(&ipc_buf->list, &ipc->tx_free.buf);
			wake_up(&ipc->tx_free.wait);
		}
		/* check if done */
		if (tx_bufsiz == bufsiz)
			break;
	}

	/* release tx buffer semaphores */
	up(&ipc->buf_sem);

	return tx_bufsiz;
}

static size_t put_ipc_rx_buf(struct baseband_ipc *ipc,
	const void *buf, size_t bufsiz)
{
	struct baseband_ipc_buf *ipc_buf, *ipc_buf_next;
	size_t rx_bufsiz;
	int ret;

	pr_debug("put_ipc_rx_buf\n");

	/* check input */
	if (!ipc || !buf) {
		pr_err("%s: !ipc || !buf\n", __func__);
		return 0;
	}
	if (!bufsiz)
		return 0;

	/* acquire rx buffer semaphores */
retry:
	if (down_interruptible(&ipc->buf_sem)) {
		pr_err("put_ipc_rx_buf - "
			"cannot acquire buffer semaphore\n");
		return -ERESTARTSYS;
	}

	/* put rx data in rx linked list */
	rx_bufsiz = 0;
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->rx_free.buf, list)
	{
		pr_debug("put_ipc_rx_buf - "
			"ipc_buf %p ipc_buf->offset %x ipc_buf->count %x\n",
			ipc_buf, ipc_buf->offset, ipc_buf->count);
		if (sizeof(ipc_buf->data) > bufsiz - rx_bufsiz) {
			/* partially fill rx free buffer */
			memcpy(ipc_buf->data,
				buf + rx_bufsiz,
				bufsiz - rx_bufsiz);
			ipc_buf->offset = 0;
			ipc_buf->count = bufsiz - rx_bufsiz;
			rx_bufsiz = bufsiz;
		} else {
			/* fill entire rx free buffer */
			memcpy(ipc_buf->data,
				buf + rx_bufsiz,
				sizeof(ipc_buf->data));
			ipc_buf->offset = 0;
			ipc_buf->count = sizeof(ipc_buf->data);
			rx_bufsiz += sizeof(ipc_buf->data);
		}
		/* add filled rx free buffer to rx linked list */
		list_move_tail(&ipc_buf->list, &ipc->rx.buf);
		wake_up(&ipc->rx.wait);
		/* check if done */
		if (rx_bufsiz == bufsiz)
			break;
	}

	/* release rx buffer semaphores */
	up(&ipc->buf_sem);

	/* wait for rx free buffer available */
	if (!rx_bufsiz) {
		ret = wait_event_interruptible_timeout(ipc->rx_free.wait,
			!list_empty(&ipc->rx_free.buf), HZ*2);
		if (ret == 0) {
			pr_err("%s timeout occured no wait\n", __func__);
			return -ETIMEDOUT;
		}
		if (ret == -ERESTARTSYS) {
			pr_err("put_ipc_rx_buf - "
				"interrupted wait\n");
			return -ERESTARTSYS;
		}
		goto retry;
	}

	return rx_bufsiz;

}

static ssize_t baseband_ipc_file_read(struct baseband_ipc *ipc,
	struct file *file, char *buf, size_t count, loff_t *pos)
{
	struct baseband_ipc_buf *ipc_buf, *ipc_buf_next;
	size_t read_count;

	pr_debug("baseband_ipc_file_read\n");

	/* check input */
	if (!ipc || !buf) {
		pr_err("%s: !ipc || !buf\n", __func__);
		return -EIO;
	}

	/* acquire rx buffer semaphores */
retry:
	if (down_interruptible(&ipc->buf_sem)) {
		pr_err("baseband_ipc_file_read - "
			"cannot acquire buffer semaphore\n");
		return -ERESTARTSYS;
	}

	/* get read data from rx linked list */
	read_count = 0;
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->rx.buf, list)
	{
		pr_debug("baseband_ipc_file_read - "
			"ipc_buf %p ipc_buf->offset %x ipc_buf->count %x\n",
			ipc_buf, ipc_buf->offset, ipc_buf->count);
		pr_debug("baseband_ipc_file_read - "
			"ipc_buf->data [0] %x [1] %x [2] %x [3] %x\n",
			ipc_buf->data[0],
			ipc_buf->data[1],
			ipc_buf->data[2],
			ipc_buf->data[3]);
		if (ipc_buf->count > count - read_count) {
			/* copy part of rx buffer */
			if (copy_to_user(buf + read_count,
				ipc_buf->data + ipc_buf->offset,
				count - read_count)) {
				pr_err("copy_to_user failed\n");
				up(&ipc->buf_sem);
				return -EFAULT;
			}
			ipc_buf->offset += count - read_count;
			ipc_buf->count -= count - read_count;
			read_count = count;
		} else {
			/* copy all data from rx buffer */
			if (copy_to_user(buf + read_count,
				ipc_buf->data + ipc_buf->offset,
				ipc_buf->count)) {
				pr_err("copy_to_user failed\n");
				up(&ipc->buf_sem);
				return -EFAULT;
			}
			read_count += ipc_buf->count;
			ipc_buf->offset = 0;
			ipc_buf->count = 0;
			/* add rx buffer to rx free list */
			list_move_tail(&ipc_buf->list, &ipc->rx_free.buf);
			wake_up(&ipc->rx_free.wait);
		}
		/* check if done */
		if (read_count == count)
			break;
	}

	/* release rx buffer semaphores */
	up(&ipc->buf_sem);

	/* wait for rx buffer available */
	if (!read_count) {
		if (wait_event_interruptible(ipc->rx.wait,
			!list_empty(&ipc->rx.buf))) {
			pr_err("baseband_ipc_file_read - "
				"interrupted wait\n");
			return -ERESTARTSYS;
		}
		goto retry;
	}

	return read_count;
}

static ssize_t baseband_ipc_file_write(struct baseband_ipc *ipc,
	struct file *file, const char *buf, size_t count, loff_t *pos)
{
	struct baseband_ipc_buf *ipc_buf, *ipc_buf_next;
	size_t write_count;

	pr_debug("baseband_ipc_file_write\n");

	/* check input */
	if (!ipc || !buf) {
		pr_err("%s: !ipc || !buf\n", __func__);
		return -EIO;
	}

	/* do not accept write if previous tx not finished */
	if (peek_ipc_tx_bufsiz(ipc, USB_CHR_TX_BUFSIZ) != 0) {
		pr_debug("%s: not accepting write of %u bytes"
			" - previous tx not finished\n",
			__func__, count);
		return 0;
	}

	/* acquire tx buffer semaphores */
retry:
	if (down_interruptible(&ipc->buf_sem)) {
		pr_err("baseband_ipc_file_write - "
			"cannot acquire buffer semaphore\n");
		return -ERESTARTSYS;
	}

	/* put write data in tx linked list */
	write_count = 0;
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->tx_free.buf, list)
	{
		pr_debug("baseband_ipc_file_write - "
			"ipc_buf %p ipc_buf->offset %x ipc_buf->count %x\n",
			ipc_buf, ipc_buf->offset, ipc_buf->count);
		if (sizeof(ipc_buf->data) > count - write_count) {
			/* partially fill tx free buffer */
			if (copy_from_user(ipc_buf->data,
				buf + write_count,
				count - write_count)) {
				pr_err("copy_from_user failed\n");
				up(&ipc->buf_sem);
				return -EFAULT;
			}
			ipc_buf->offset = 0;
			ipc_buf->count = count - write_count;
			write_count = count;
		} else {
			/* fill entire tx free buffer */
			if (copy_from_user(ipc_buf->data,
				buf + write_count,
				sizeof(ipc_buf->data))) {
				pr_err("copy_from_user failed\n");
				up(&ipc->buf_sem);
				return -EFAULT;
			}
			ipc_buf->offset = 0;
			ipc_buf->count = sizeof(ipc_buf->data);
			write_count += sizeof(ipc_buf->data);
		}
		/* add filled tx free buffer to tx linked list */
		pr_debug("baseband_ipc_file_write - "
			"ipc_buf->data [0] %x [1] %x [2] %x [3] %x\n",
			ipc_buf->data[0],
			ipc_buf->data[1],
			ipc_buf->data[2],
			ipc_buf->data[3]);
		list_move_tail(&ipc_buf->list, &ipc->tx.buf);
		wake_up(&ipc->tx.wait);
		/* check if done */
		if (write_count == count)
			break;
	}

	/* release tx buffer semaphores */
	up(&ipc->buf_sem);

	/* wait for tx buffer available */
	if (!write_count) {
		if (wait_event_interruptible(ipc->tx_free.wait,
			!list_empty(&ipc->tx_free.buf))) {
			pr_err("baseband_ipc_file_write - "
				"interrupted wait\n");
			return -ERESTARTSYS;
		}
		goto retry;
	}

	/* queue ipc transaction work */
	queue_work(ipc->workqueue, &ipc->work);

	return write_count;
}

static void baseband_ipc_close(struct baseband_ipc *ipc)
{
	struct baseband_ipc_buf *ipc_buf, *ipc_buf_next;

	pr_debug("baseband_ipc_close {\n");

	/* check input */
	if (!ipc)
		return;

	/* cancel work queue */
	if (ipc->workqueue) {
		pr_debug("destroy workqueue {\n");
		cancel_work_sync(&ipc->work);
		pr_debug("destroy workqueue }\n");
	}
	memset(&ipc->work, 0, sizeof(ipc->work));

	/* destroy wait queues */
	memset(&ipc->tx_free.wait, 0, sizeof(ipc->tx_free.wait));
	memset(&ipc->rx_free.wait, 0, sizeof(ipc->rx_free.wait));
	memset(&ipc->tx.wait, 0, sizeof(ipc->tx.wait));
	memset(&ipc->rx.wait, 0, sizeof(ipc->rx.wait));

	/* destroy data buffers */
	ipc->ipc_tx = (unsigned char *) 0;
	ipc->ipc_rx = (unsigned char *) 0;
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->tx_free.buf, list)
	{
		vfree(ipc_buf);
	}
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->rx_free.buf, list)
	{
		vfree(ipc_buf);
	}
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->tx.buf, list)
	{
		vfree(ipc_buf);
	}
	list_for_each_entry_safe(ipc_buf, ipc_buf_next, &ipc->rx.buf, list)
	{
		vfree(ipc_buf);
	}

	/* destroy semaphores */
	memset(&ipc->buf_sem, 0, sizeof(ipc->buf_sem));

	/* free baseband ipc structure */
	vfree(ipc);

	pr_debug("baseband_ipc_close }\n");
}

static struct baseband_ipc *baseband_ipc_open(work_func_t work_func,
	work_func_t rx_work_func,
	work_func_t tx_work_func)
{
	struct baseband_ipc *ipc;
	struct baseband_ipc_buf *ipc_buf;
	int i;

	pr_debug("baseband_ipc_open {\n");

	/* allocate baseband ipc structure */
	ipc = vmalloc(sizeof(struct baseband_ipc));
	if (!ipc)
		return (struct baseband_ipc *) 0;

	memset(ipc, 0 , sizeof(struct baseband_ipc));
	/* create semaphores */
	sema_init(&ipc->buf_sem, 1);

	/* create data buffers */
	INIT_LIST_HEAD(&ipc->rx.buf);
	INIT_LIST_HEAD(&ipc->tx.buf);
	INIT_LIST_HEAD(&ipc->rx_free.buf);
	INIT_LIST_HEAD(&ipc->tx_free.buf);
	for (i = 0; i < BASEBAND_IPC_NUM_RX_BUF; i++) {
		ipc_buf = (struct baseband_ipc_buf *)
			vmalloc(sizeof(struct baseband_ipc_buf));
		if (!ipc_buf) {
			pr_err("cannot allocate baseband ipc rx buffer #%d\n",
				i);
			goto error_exit;
		}
		pr_debug("baseband_ipc_open - "
			"rx_free: ipc_buf %p\n",
			ipc_buf);
		list_add_tail(&ipc_buf->list, &ipc->rx_free.buf);
	}
	for (i = 0; i < BASEBAND_IPC_NUM_TX_BUF; i++) {
		ipc_buf = (struct baseband_ipc_buf *)
			vmalloc(sizeof(struct baseband_ipc_buf));
		if (!ipc_buf) {
			pr_err("cannot allocate baseband ipc tx buffer #%d\n",
				i);
			goto error_exit;
		}
		pr_debug("baseband_ipc_open - "
			"tx_free: ipc_buf %p\n",
			ipc_buf);
		list_add_tail(&ipc_buf->list, &ipc->tx_free.buf);
	}
	ipc->ipc_rx = usb_chr_res.ipc_rx;
	if (!ipc->ipc_rx) {
		pr_err("%s: cannot find ipc->ipc_rx\n", __func__);
		goto error_exit;
	}
	ipc->ipc_tx = usb_chr_res.ipc_tx;
	if (!ipc->ipc_tx) {
		pr_err("%s: cannot find ipc->ipc_tx\n", __func__);
		goto error_exit;
	}

	/* create wait queues */
	init_waitqueue_head(&ipc->rx.wait);
	init_waitqueue_head(&ipc->tx.wait);
	init_waitqueue_head(&ipc->rx_free.wait);
	init_waitqueue_head(&ipc->tx_free.wait);

	/* init work queue */
	if (chr_ipc_wq)
		ipc->workqueue = chr_ipc_wq;
	else {
		pr_err("%s: no workqueue found\n", __func__);
		goto error_exit;
	}
	if (work_func)
		INIT_WORK(&ipc->work, work_func);
	if (rx_work_func)
		INIT_WORK(&ipc->rx_work, rx_work_func);
	if (tx_work_func)
		INIT_WORK(&ipc->tx_work, tx_work_func);

	pr_debug("baseband_ipc_open }\n");
	return ipc;

error_exit:
	baseband_ipc_close(ipc);
	return (struct baseband_ipc *) 0;
}

/* usb rx */

static void baseband_usb_chr_rx_urb_comp(struct urb *urb)
{
	struct baseband_usb *usb = (struct baseband_usb *) urb->context;

	pr_debug("baseband_usb_chr_rx_urb_comp { urb %p\n", urb);

	/* check input */
	if (!usb) {
		pr_err("%s: !usb\n", __func__);
		return;
	}
	if (!usb->ipc) {
		pr_err("%s: !usb->ipc\n", __func__);
		return;
	}
	if (!usb->ipc->workqueue) {
		pr_err("%s: !usb->ipc->rx_work\n", __func__);
		return;
	}

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ENOENT:
	case -ESHUTDOWN:
	case -EPROTO:
		pr_info("%s: link down\n", __func__);
	default:
		pr_err("%s: urb error status %d\n", __func__, urb->status);
		return;
	}

	/* queue rx urb completion work */
	queue_work(usb->ipc->workqueue, &usb->ipc->rx_work);

	pr_debug("baseband_usb_chr_rx_urb_comp }\n");
}

static int baseband_usb_chr_rx_urb_submit(struct baseband_usb *usb)
{
	struct urb *urb;
	void *buf;
	int err;

	pr_debug("baseband_usb_chr_rx_urb_submit { usb %p\n", usb);

	/* check input */
	if (!usb_device_connection) {
		pr_err("%s: no usb device connection\n", __func__);
		return -1;
	}
	if (!usb->usb.rx_urb) {
		pr_err("%s: no rx urb!\n", __func__);
		return -1;
	}

	/* fill rx urb */
	urb = usb->usb.rx_urb;
	buf = usb->usb.rx_urb->transfer_buffer;
	usb_fill_bulk_urb(urb, usb->usb.device, usb->usb.pipe.bulk.in,
		buf, USB_CHR_RX_BUFSIZ,
		baseband_usb_chr_rx_urb_comp,
		usb);
	urb->transfer_flags = 0;

	/* submit rx urb */
	usb->usb.rx_urb = urb;
	err = usb_submit_urb(urb, GFP_KERNEL);
	if (err < 0) {
		pr_err("usb_submit_urb() failed - err %d\n", err);
		return err;
	}

	pr_debug("baseband_usb_chr_rx_urb_submit }\n");
	return err;
}

static void baseband_usb_chr_rx_urb_comp_work(struct work_struct *work)
{
	struct baseband_usb *usb = baseband_usb_chr;
	struct urb *urb = usb->usb.rx_urb;
	size_t len;

	pr_debug("baseband_usb_chr_rx_urb_comp_work { work %p\n", work);

	if (usb_device_connection == false) {
		/* device is closing or disconnect - nothing to read */
		pr_info("%s: device is disconnected\n", __func__);
		return;
	}
	/* put rx urb data in rx buffer */
	if (urb->actual_length) {
		pr_debug("baseband_usb_chr_rx_urb_comp_work - "
			"urb->actual_length %d\n", urb->actual_length);
		len = put_ipc_rx_buf(usb->ipc,
			urb->transfer_buffer, urb->actual_length);
		if (len == -ETIMEDOUT) {
			/* device closed */
			pr_info("%s: device closed\n", __func__);
			return;
		}
		baseband_ipc_dump("baseband_usb_chr_rx_urb_comp_work"
			" - rx buf ", 0,
			urb->transfer_buffer, len > 16 ? 16 : len);
		if (len != urb->actual_length) {
			pr_err("baseband_usb_chr_rx_urb_comp_work - "
				"put_ipx_rx_buf() only put %d/%d bytes\n",
				len, urb->actual_length);
		}
		/* increment count of available rx bytes */
		atomic_add(len, &g_rx_count);
	}

	/* submit next rx urb */
	baseband_usb_chr_rx_urb_submit(usb);

	pr_debug("baseband_usb_chr_rx_urb_comp_work }\n");
}

/* usb functions */

static void find_usb_pipe(struct baseband_usb *usb)
{
	struct usb_device *usbdev = usb->usb.device;
	struct usb_interface *intf = usb->usb.interface;
	unsigned char numendpoint = intf->cur_altsetting->desc.bNumEndpoints;
	struct usb_host_endpoint *endpoint = intf->cur_altsetting->endpoint;
	unsigned char n;

	for (n = 0; n < numendpoint; n++) {
		if (usb_endpoint_is_isoc_in(&endpoint[n].desc)) {
			pr_debug("endpoint[%d] isochronous in\n", n);
			usb->usb.pipe.isoch.in = usb_rcvisocpipe(usbdev,
				endpoint[n].desc.bEndpointAddress);
		} else if (usb_endpoint_is_isoc_out(&endpoint[n].desc)) {
			pr_debug("endpoint[%d] isochronous out\n", n);
			usb->usb.pipe.isoch.out = usb_sndisocpipe(usbdev,
				endpoint[n].desc.bEndpointAddress);
		} else if (usb_endpoint_is_bulk_in(&endpoint[n].desc)) {
			pr_debug("endpoint[%d] bulk in\n", n);
			usb->usb.pipe.bulk.in = usb_rcvbulkpipe(usbdev,
				endpoint[n].desc.bEndpointAddress);
		} else if (usb_endpoint_is_bulk_out(&endpoint[n].desc)) {
			pr_debug("endpoint[%d] bulk out\n", n);
			usb->usb.pipe.bulk.out = usb_sndbulkpipe(usbdev,
				endpoint[n].desc.bEndpointAddress);
		} else if (usb_endpoint_is_int_in(&endpoint[n].desc)) {
			pr_debug("endpoint[%d] interrupt in\n", n);
			usb->usb.pipe.interrupt.in = usb_rcvintpipe(usbdev,
				endpoint[n].desc.bEndpointAddress);
		} else if (usb_endpoint_is_int_out(&endpoint[n].desc)) {
			pr_debug("endpoint[%d] interrupt out\n", n);
			usb->usb.pipe.interrupt.out = usb_sndintpipe(usbdev,
				endpoint[n].desc.bEndpointAddress);
		} else {
			pr_debug("endpoint[%d] skipped\n", n);
		}
	}
}

static int baseband_usb_driver_probe(struct usb_interface *intf,
	const struct usb_device_id *id)
{
	int i;

	pr_debug("%s(%d) { intf %p id %p\n", __func__, __LINE__, intf, id);

	pr_debug("intf->cur_altsetting->desc.bInterfaceNumber %02x\n",
		intf->cur_altsetting->desc.bInterfaceNumber);
	pr_debug("intf->cur_altsetting->desc.bAlternateSetting %02x\n",
		intf->cur_altsetting->desc.bAlternateSetting);
	pr_debug("intf->cur_altsetting->desc.bNumEndpoints %02x\n",
		intf->cur_altsetting->desc.bNumEndpoints);
	pr_debug("intf->cur_altsetting->desc.bInterfaceClass %02x\n",
		intf->cur_altsetting->desc.bInterfaceClass);
	pr_debug("intf->cur_altsetting->desc.bInterfaceSubClass %02x\n",
		intf->cur_altsetting->desc.bInterfaceSubClass);
	pr_debug("intf->cur_altsetting->desc.bInterfaceProtocol %02x\n",
		intf->cur_altsetting->desc.bInterfaceProtocol);
	pr_debug("intf->cur_altsetting->desc.iInterface %02x\n",
		intf->cur_altsetting->desc.iInterface);

	/* usb interface mismatch */
	for (i = 0; baseband_usb_driver_id_table[i].match_flags; i++) {
		if (id == &baseband_usb_driver_id_table[i]) {
			if (baseband_usb_driver_intf_table[i] !=
				intf->cur_altsetting->desc.bInterfaceNumber) {
				pr_debug("%s(%d) } -ENODEV\n", __func__, __LINE__);
				return -ENODEV;
			}
		}
	}

	/* usb interface match */
	probe_usb_intf = intf;
	usb_device_connection = true;

	pr_debug("%s(%d) }\n", __func__, __LINE__);
	return 0;
}

static void baseband_usb_driver_disconnect(struct usb_interface *intf)
{
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	pr_debug("%s(%d) { intf %p\n", __func__, __LINE__, intf);

	if (!baseband_usb_chr) {
		pr_err("%s: no baseband_usb_chr\n", __func__);
		return;
	}

	if (baseband_usb_chr->usb.interface != intf) {
		pr_info("%s(%d) -ENODEV\n", __func__, __LINE__);
		return;
	}
	if (baseband_usb_chr->usb.device == usb_dev) {
		pr_info("%s: Matching usb device: Flush workqueue\n", __func__);
		/* flush queued ipc transaction work */
		if (baseband_usb_chr && baseband_usb_chr->ipc
			&& baseband_usb_chr->ipc->workqueue)
			flush_workqueue(baseband_usb_chr->ipc->workqueue);
		usb_device_connection = false;
		probe_usb_intf = NULL;
	}
	pr_debug("%s(%d) }\n", __func__, __LINE__);
}

static struct usb_driver baseband_usb_driver = {
	.name = "bb_usb_chr",
	.probe = baseband_usb_driver_probe,
	.disconnect = baseband_usb_driver_disconnect,
	.id_table = baseband_usb_driver_id_table,
};

static void baseband_usb_chr_work(struct work_struct *work)
{
	struct baseband_usb *usb = baseband_usb_chr;
	struct {
		unsigned char *buf;
		unsigned int bufsiz_byte;
	} rx, tx;
	int ipc_tx_byte;
	int err;

	pr_debug("baseband_usb_chr_work {\n");

	/* check input */
	if (!usb || !usb->ipc) {
		pr_err("baseband_usb_chr_work - "
			"usb not open\n");
		return;
	}
	if (!usb->usb.device) {
		pr_err("baseband_usb_chr_work - "
			"usb device not probed yet\n");
		mdelay(10);
		queue_work(usb->ipc->workqueue, &usb->ipc->work);
		return;
	}
	if (!usb->ipc->ipc_rx) {
		pr_err("baseband_usb_chr_work - "
			"null usb->ipc->ipc_rx\n");
		return;
	}
	if (!usb->ipc->ipc_tx) {
		pr_err("baseband_usb_chr_work - "
			"null usb->ipc->ipc_tx\n");
		return;
	}

	/* usb transaction loop */
	rx.buf = usb->ipc->ipc_rx;
	tx.buf = usb->ipc->ipc_tx;
	while ((tx.bufsiz_byte = peek_ipc_tx_bufsiz(usb->ipc,
		USB_CHR_TX_BUFSIZ)) != 0) {
		get_ipc_tx_buf(usb->ipc, tx.buf, tx.bufsiz_byte);
		err = usb_bulk_msg(usb->usb.device, usb->usb.pipe.bulk.out,
			tx.buf, tx.bufsiz_byte, &ipc_tx_byte, USB_CHR_TIMEOUT);
		if (err < 0) {
			pr_err("baseband_usb_chr_work - "
				"usb_bulk_msg err %d\n", err);
			continue;
		}
		if (tx.bufsiz_byte != ipc_tx_byte) {
			pr_err("tx.bufsiz_byte %d != ipc_tx_byte %d\n",
				tx.bufsiz_byte, ipc_tx_byte);
			continue;
		}
	}

	pr_debug("baseband_usb_chr_work }\n");
}

/* usb device driver functions */

static void baseband_usb_close(struct baseband_usb *usb)
{
	pr_debug("baseband_usb_close {\n");

	/* check input */
	if (!usb)
		return;

	/* we need proper lock, maybe...*/
	usb_device_connection = false;

	/* free re-usable rx urb + rx urb transfer buffer */
	if (usb->usb.rx_urb) {
		pr_debug("%s: free rx urb\n", __func__);
		usb_kill_urb(usb->usb.rx_urb);
		if (usb->usb.rx_urb->transfer_buffer) {
			pr_debug("%s: free rx urb transfer buffer\n", __func__);
			usb->usb.rx_urb->transfer_buffer = (void *) 0;
		}
	}

	if (usb->ipc) {
		flush_work(&usb->ipc->work);
		flush_work(&usb->ipc->rx_work);
	}


	/* close baseband ipc */
	if (usb->ipc) {
		baseband_ipc_close(usb->ipc);
		usb_free_urb(usb->usb.rx_urb);
		usb->usb.rx_urb = NULL;
		usb->ipc = NULL;
	}

	/* free baseband usb structure */
	vfree(usb);

	pr_debug("baseband_usb_close }\n");
}

static struct baseband_usb *baseband_usb_open(work_func_t work_func,
	work_func_t rx_work_func,
	work_func_t tx_work_func)
{
	struct baseband_usb *usb;
	int err, i;
	struct urb *urb;
	void *buf;

	pr_debug("baseband_usb_open {\n");

	/* allocate baseband usb structure */
	usb = vmalloc(sizeof(struct baseband_usb));
	if (!usb)
		return (struct baseband_usb *) 0;
	memset(usb, 0, sizeof(struct baseband_usb));

	/* open baseband ipc */
	usb->ipc = baseband_ipc_open(work_func,
		rx_work_func,
		tx_work_func);
	if (!usb->ipc) {
		pr_err("open baseband ipc failed\n");
		goto error_exit;
	}

	usb->usb.driver = &baseband_usb_driver;

	for (i = 0; i < 5 * 50; i++) {
		if (probe_usb_intf && usb_device_connection)
			break;
		/* wait for probe */
		pr_debug("%s: waiting for usb probe...\n", __func__);
		msleep(20);
	}
	if (!probe_usb_intf || !usb_device_connection) {
		pr_err("%s: probe timed out!\n", __func__);
		goto error_exit;
	}

	/* get probed usb device information */
	usb->usb.device = interface_to_usbdev(probe_usb_intf);
	usb->usb.interface = probe_usb_intf;
	find_usb_pipe(usb);
	usb->usb.rx_urb = (struct urb *) 0;
	usb->usb.tx_urb = (struct urb *) 0;
	pr_debug("usb->usb.driver->name %s\n",
		usb->usb.driver->name);
	pr_debug("usb->usb.device %p\n",
		usb->usb.device);
	pr_debug("usb->usb.interface %p\n",
		usb->usb.interface);
	pr_debug("usb->usb.pipe.isoch.in %x\n",
		usb->usb.pipe.isoch.in);
	pr_debug("usb->usb.pipe.isoch.out %x\n",
		usb->usb.pipe.isoch.out);
	pr_debug("usb->usb.pipe.bulk.in %x\n",
		usb->usb.pipe.bulk.in);
	pr_debug("usb->usb.pipe.bulk.out %x\n",
		usb->usb.pipe.bulk.out);
	pr_debug("usb->usb.pipe.interrupt.in %x\n",
		usb->usb.pipe.interrupt.in);
	pr_debug("usb->usb.pipe.interrupt.out %x\n",
		usb->usb.pipe.interrupt.out);

	/* allocate re-usable rx urb + rx urb transfer buffer */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		pr_err("usb_alloc_urb() failed\n");
		goto error_exit;
	}
	buf = usb_chr_res.rx_buf;
	if (!buf) {
		pr_err("%s: usb rx buffer not found\n", __func__);
		usb_free_urb(urb);
		goto error_exit;
	}
	urb->transfer_buffer = buf;
	usb->usb.rx_urb = urb;

	/* start usb rx */
	err = baseband_usb_chr_rx_urb_submit(usb);
	if (err < 0) {
		pr_err("submit rx failed - err %d\n", err);
		goto error_exit;
	}

	pr_debug("baseband_usb_open }\n");
	return usb;

error_exit:
	baseband_usb_close(usb);
	baseband_usb_chr = (struct baseband_usb *) 0;
	return (struct baseband_usb *) 0;
}


/* usb character file operations */

static int baseband_usb_chr_open(struct inode *inode, struct file *file)
{
	pr_debug("baseband_usb_chr_open {\n");

	if (baseband_usb_chr) {
		pr_err("%s: device is already open\n", __func__);
		/* application uses two fd opens for download*/
		baseband_usb_chr->ref++;
		return 0;
	}

	/* open baseband usb */
	baseband_usb_chr = baseband_usb_open(baseband_usb_chr_work,
				baseband_usb_chr_rx_urb_comp_work,
				(work_func_t) 0);
	if (!baseband_usb_chr) {
		pr_err("cannot open baseband usb chr\n");
		return -ENODEV;
	}
	baseband_usb_chr->ref++;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	pr_debug("baseband_usb_chr_open }\n");
	return 0;
}

static int baseband_usb_chr_release(struct inode *inode, struct file *file)
{
	pr_debug("baseband_usb_chr_release\n");
	pr_info("baseband_usb_chr_release {\n");

	if (baseband_usb_chr) {
		baseband_usb_chr->ref--;
		if (baseband_usb_chr->ref)
			return 0;

		/* close baseband usb */
		baseband_usb_close(baseband_usb_chr);
		baseband_usb_chr = (struct baseband_usb *) 0;
	}

	module_put(THIS_MODULE);
	pr_info("baseband_usb_chr_release }\n");

	return 0;
}

static ssize_t baseband_usb_chr_read(struct file *file, char *buf,
	size_t count, loff_t *pos)
{
	ssize_t ret;

	pr_debug("baseband_usb_chr_read\n");

	if (!baseband_usb_chr || !baseband_usb_chr->ipc) {
		pr_err("%s: -ENODEV\n", __func__);
		return -ENODEV;
	}
	ret = baseband_ipc_file_read(baseband_usb_chr->ipc,
		file, buf, count, pos);
	if (ret > 0) {
		/* decrement count of available rx bytes */
		int val = atomic_read(&g_rx_count);
		pr_debug("baseband_usb_chr_read - read %d unread %d\n",
			ret, val - ret);
		atomic_sub(ret, &g_rx_count);
	}
	return ret;
}

static ssize_t baseband_usb_chr_write(struct file *file, const char *buf,
	size_t count, loff_t *pos)
{
	pr_debug("baseband_usb_chr_write\n");
	if (!baseband_usb_chr || !baseband_usb_chr->ipc) {
		pr_err("%s: -ENODEV\n", __func__);
		return -ENODEV;
	}
	return baseband_ipc_file_write(baseband_usb_chr->ipc,
		file, buf, count, pos);
}

static long baseband_usb_chr_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	pr_debug("baseband_usb_chr_ioctl\n");
	switch (cmd) {
	case TCFLSH:
		pr_debug("TCFLSH\n");
		/* flush queued ipc transaction work */
		if (!baseband_usb_chr || !baseband_usb_chr->ipc
			|| !baseband_usb_chr->ipc->workqueue) {
			pr_err("%s: no workqueue!\n", __func__);
			return -ENODEV;
		}
		flush_workqueue(baseband_usb_chr->ipc->workqueue);
		return 0;
	case FIONREAD:
		pr_debug("FIONREAD\n");
		/* return count of available rx bytes */
		{
			int __user *p = (int __user *) arg;
			int val = atomic_read(&g_rx_count);
			if (put_user(val, p))
				break;
		}
		return 0;
	default:
		pr_err("unsupported ioctl cmd %x\n", cmd);
		return 0;
	}
	return -ENODEV;
}

static const struct file_operations baseband_usb_chr_fops = {
	.open = baseband_usb_chr_open,
	.release = baseband_usb_chr_release,
	.read = baseband_usb_chr_read,
	.write = baseband_usb_chr_write,
	.unlocked_ioctl = baseband_usb_chr_ioctl,
};

/* module init / exit functions */

static int baseband_usb_chr_init(void)
{
	int err = -ENOMEM;

	pr_debug("baseband_usb_chr_init {\n");

	usb_chr_res.ipc_rx = kmalloc(USB_CHR_RX_BUFSIZ, GFP_KERNEL);
	if (!usb_chr_res.ipc_rx) {
		pr_err("cannot allocate ipc_rx\n");
		goto error;
	}
	usb_chr_res.ipc_tx = kmalloc(USB_CHR_TX_BUFSIZ, GFP_KERNEL);
	if (!usb_chr_res.ipc_tx) {
		pr_err("cannot allocate ipc_tx\n");
		goto error;
	}
	usb_chr_res.rx_buf = kmalloc(USB_CHR_RX_BUFSIZ, GFP_KERNEL);
	if (!usb_chr_res.rx_buf) {
		pr_err("%s: usb buffer kmalloc() failed\n", __func__);
		goto error;
	}

	/* register character device */
	err = register_chrdev(BASEBAND_USB_CHR_DEV_MAJOR,
		BASEBAND_USB_CHR_DEV_NAME,
		&baseband_usb_chr_fops);
	if (err < 0) {
		pr_err("cannot register character device - %d\n", err);
		goto error;
	}
	pr_debug("registered baseband usb character device - major %d\n",
		BASEBAND_USB_CHR_DEV_MAJOR);

	/* create workqueue thread */
	chr_ipc_wq = create_singlethread_workqueue("baseband_chr_wq");
	if (chr_ipc_wq == NULL) {
		pr_err("cannot create workqueue\n");
		unregister_chrdev(BASEBAND_USB_CHR_DEV_MAJOR,
			BASEBAND_USB_CHR_DEV_NAME);
		err = -ENODEV;
		goto error;
	}

	/* register usb driver */
	err = usb_register(&baseband_usb_driver);
	if (err < 0) {
		pr_err("%s: cannot register usb driver %d\n", __func__, err);
		goto error2;
	}

	pr_debug("baseband_usb_chr_init }\n");
	return 0;

error2:
	unregister_chrdev(BASEBAND_USB_CHR_DEV_MAJOR,
			BASEBAND_USB_CHR_DEV_NAME);
	destroy_workqueue(chr_ipc_wq);
	chr_ipc_wq = NULL;
error:
	kfree(usb_chr_res.ipc_rx);
	kfree(usb_chr_res.ipc_tx);
	kfree(usb_chr_res.rx_buf);
	return err;
}

static void baseband_usb_chr_exit(void)
{
	pr_debug("baseband_usb_chr_exit {\n");

	/* unregister character device */
	unregister_chrdev(BASEBAND_USB_CHR_DEV_MAJOR,
		BASEBAND_USB_CHR_DEV_NAME);

	if (chr_ipc_wq) {
		destroy_workqueue(chr_ipc_wq);
		chr_ipc_wq = NULL;
	}

	/* close usb driver */
	usb_deregister(&baseband_usb_driver);

	kfree(usb_chr_res.ipc_rx);
	kfree(usb_chr_res.ipc_tx);
	kfree(usb_chr_res.rx_buf);

	pr_debug("baseband_usb_chr_exit }\n");
}

module_init(baseband_usb_chr_init)
module_exit(baseband_usb_chr_exit)


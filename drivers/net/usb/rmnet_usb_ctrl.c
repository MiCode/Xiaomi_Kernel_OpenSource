/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/termios.h>
#include <linux/poll.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include "rmnet_usb.h"

static char *rmnet_dev_names[MAX_RMNET_DEVS] = {"hsicctl"};
module_param_array(rmnet_dev_names, charp, NULL, S_IRUGO | S_IWUSR);

#define DEFAULT_READ_URB_LENGTH		0x1000
#define UNLINK_TIMEOUT_MS		500 /*random value*/

/*Output control lines.*/
#define ACM_CTRL_DTR		BIT(0)
#define ACM_CTRL_RTS		BIT(1)


/*Input control lines.*/
#define ACM_CTRL_DSR		BIT(0)
#define ACM_CTRL_CTS		BIT(1)
#define ACM_CTRL_RI		BIT(2)
#define ACM_CTRL_CD		BIT(3)

/*echo modem_wait > /sys/class/hsicctl/hsicctlx/modem_wait*/
static ssize_t modem_wait_store(struct device *d, struct device_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int		mdm_wait;
	struct rmnet_ctrl_dev	*dev = dev_get_drvdata(d);

	if (!dev)
		return -ENODEV;

	sscanf(buf, "%u", &mdm_wait);

	dev->mdm_wait_timeout = mdm_wait;

	return n;
}

static ssize_t modem_wait_show(struct device *d, struct device_attribute *attr,
		char *buf)
{
	struct rmnet_ctrl_dev	*dev = dev_get_drvdata(d);

	if (!dev)
		return -ENODEV;

	return snprintf(buf, PAGE_SIZE, "%u\n", dev->mdm_wait_timeout);
}

static DEVICE_ATTR(modem_wait, 0664, modem_wait_show, modem_wait_store);

static int	ctl_msg_dbg_mask;
module_param_named(dump_ctrl_msg, ctl_msg_dbg_mask, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

enum {
	MSM_USB_CTL_DEBUG = 1U << 0,
	MSM_USB_CTL_DUMP_BUFFER = 1U << 1,
};

#define DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (ctl_msg_dbg_mask & MSM_USB_CTL_DUMP_BUFFER) \
			print_hex_dump(KERN_INFO, prestr, DUMP_PREFIX_NONE, \
					16, 1, buf, cnt, false); \
} while (0)

#define DBG(x...) \
		do { \
			if (ctl_msg_dbg_mask & MSM_USB_CTL_DEBUG) \
				pr_info(x); \
		} while (0)

/* passed in rmnet_usb_ctrl_init */
static int num_devs;
static int insts_per_dev;

/* dynamically allocated 2-D array of num_devs*insts_per_dev ctrl_devs */
static struct rmnet_ctrl_dev **ctrl_devs;
static struct class	*ctrldev_classp[MAX_RMNET_DEVS];
static dev_t		ctrldev_num[MAX_RMNET_DEVS];

struct ctrl_pkt {
	size_t	data_size;
	void	*data;
	void	*ctxt;
};

struct ctrl_pkt_list_elem {
	struct list_head	list;
	struct ctrl_pkt		cpkt;
};

static void resp_avail_cb(struct urb *);

static int rmnet_usb_ctrl_dmux(struct ctrl_pkt_list_elem *clist)
{
	struct mux_hdr	*hdr;
	size_t		pad_len;
	size_t		total_len;
	unsigned int	mux_id;

	hdr = (struct mux_hdr *)clist->cpkt.data;
	pad_len = hdr->padding_info & MUX_CTRL_PADLEN_MASK;
	if (pad_len > MAX_PAD_BYTES(4)) {
		pr_err_ratelimited("%s: Invalid pad len %d\n", __func__,
				pad_len);
		return -EINVAL;
	}

	mux_id = hdr->mux_id;
	if (!mux_id || mux_id > insts_per_dev) {
		pr_err_ratelimited("%s: Invalid mux id %d\n", __func__, mux_id);
		return -EINVAL;
	}

	total_len = ntohs(hdr->pkt_len_w_padding);
	if (!total_len || !(total_len - pad_len)) {
		pr_err_ratelimited("%s: Invalid pkt length %d\n", __func__,
				total_len);
		return -EINVAL;
	}

	clist->cpkt.data_size = total_len - pad_len;

	return mux_id - 1;
}

static void rmnet_usb_ctrl_mux(unsigned int id, struct ctrl_pkt *cpkt)
{
	struct mux_hdr	*hdr;
	size_t		len;
	size_t		pad_len = 0;

	hdr = (struct mux_hdr *)cpkt->data;
	hdr->mux_id = id + 1;
	len = cpkt->data_size - sizeof(struct mux_hdr) - MAX_PAD_BYTES(4);

	/*add padding if len is not 4 byte aligned*/
	pad_len =  ALIGN(len, 4) - len;

	hdr->pkt_len_w_padding = htons(len + pad_len);
	hdr->padding_info = (pad_len &  MUX_CTRL_PADLEN_MASK) | MUX_CTRL_MASK;

	cpkt->data_size = sizeof(struct mux_hdr) +
		ntohs(hdr->pkt_len_w_padding);
}

static void get_encap_work(struct work_struct *w)
{
	struct usb_device	*udev;
	struct rmnet_ctrl_udev	*dev =
			container_of(w, struct rmnet_ctrl_udev, get_encap_work);
	int			status;

	if (!test_bit(RMNET_CTRL_DEV_READY, &dev->status))
		return;

	if (dev->rcvurb->anchor) {
		dev->ignore_encap_work++;
		return;
	}

	udev = interface_to_usbdev(dev->intf);

	status = usb_autopm_get_interface(dev->intf);
	if (status < 0 && status != -EAGAIN && status != -EACCES) {
		dev->get_encap_failure_cnt++;
		return;
	}

	usb_fill_control_urb(dev->rcvurb, udev,
				usb_rcvctrlpipe(udev, 0),
				(unsigned char *)dev->in_ctlreq,
				dev->rcvbuf,
				DEFAULT_READ_URB_LENGTH,
				resp_avail_cb, dev);


	usb_anchor_urb(dev->rcvurb, &dev->rx_submitted);
	status = usb_submit_urb(dev->rcvurb, GFP_KERNEL);
	if (status) {
		dev->get_encap_failure_cnt++;
		usb_unanchor_urb(dev->rcvurb);
		usb_autopm_put_interface(dev->intf);
		if (status != -ENODEV)
			pr_err("%s: Error submitting Read URB %d\n",
			__func__, status);
		goto resubmit_int_urb;
	}

	return;

resubmit_int_urb:
	/*check if it is already submitted in resume*/
	if (!dev->inturb->anchor) {
		usb_anchor_urb(dev->inturb, &dev->rx_submitted);
		status = usb_submit_urb(dev->inturb, GFP_KERNEL);
		if (status) {
			usb_unanchor_urb(dev->inturb);
			if (status != -ENODEV)
				pr_err("%s: Error re-submitting Int URB %d\n",
				__func__, status);
		}
	}
}

static void notification_available_cb(struct urb *urb)
{
	int				status;
	struct usb_cdc_notification	*ctrl;
	struct usb_device		*udev;
	struct rmnet_ctrl_udev		*dev = urb->context;
	struct rmnet_ctrl_dev		*cdev;

	/*usb device disconnect*/
	if (urb->dev->state == USB_STATE_NOTATTACHED)
		return;

	udev = interface_to_usbdev(dev->intf);

	switch (urb->status) {
	case 0:
	/*if non zero lenght of data received while unlink*/
	case -ENOENT:
		/*success*/
		break;

	/*do not resubmit*/
	case -ESHUTDOWN:
	case -ECONNRESET:
	case -EPROTO:
		return;
	case -EPIPE:
		pr_err_ratelimited("%s: Stall on int endpoint\n", __func__);
		/* TBD : halt to be cleared in work */
		return;

	/*resubmit*/
	case -EOVERFLOW:
		pr_err_ratelimited("%s: Babble error happened\n", __func__);
	default:
		 pr_debug_ratelimited("%s: Non zero urb status = %d\n",
			__func__, urb->status);
		goto resubmit_int_urb;
	}

	if (!urb->actual_length)
		return;

	ctrl = urb->transfer_buffer;

	switch (ctrl->bNotificationType) {
	case USB_CDC_NOTIFY_RESPONSE_AVAILABLE:
		dev->resp_avail_cnt++;
		/* If MUX is not enabled, wakeup up the open process
		 * upon first notify response available.
		 */
		if (!test_bit(RMNET_CTRL_DEV_READY, &dev->status)) {
			set_bit(RMNET_CTRL_DEV_READY, &dev->status);

			cdev = &ctrl_devs[dev->rdev_num][dev->ctrldev_id];
			wake_up(&cdev->open_wait_queue);
		}

		usb_mark_last_busy(udev);
		queue_work(dev->wq, &dev->get_encap_work);

		return;
	default:
		 dev_err(&dev->intf->dev,
			"%s:Command not implemented\n", __func__);
	}

resubmit_int_urb:
	usb_anchor_urb(urb, &dev->rx_submitted);
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status) {
		usb_unanchor_urb(urb);
		if (status != -ENODEV)
			pr_err("%s: Error re-submitting Int URB %d\n",
			__func__, status);
	}

	return;
}

static void resp_avail_cb(struct urb *urb)
{
	struct usb_device		*udev;
	struct ctrl_pkt_list_elem	*list_elem = NULL;
	struct rmnet_ctrl_udev		*dev = urb->context;
	struct rmnet_ctrl_dev		*rx_dev;
	void				*cpkt;
	int					status = 0;
	int					ch_id = -EINVAL;
	size_t				cpkt_size = 0;

	/*usb device disconnect*/
	if (urb->dev->state == USB_STATE_NOTATTACHED)
		return;

	udev = interface_to_usbdev(dev->intf);

	usb_autopm_put_interface_async(dev->intf);

	switch (urb->status) {
	case 0:
		/*success*/
		break;

	/*do not resubmit*/
	case -ESHUTDOWN:
	case -ENOENT:
	case -ECONNRESET:
	case -EPROTO:
		return;

	/*resubmit*/
	case -EOVERFLOW:
		pr_err_ratelimited("%s: Babble error happened\n", __func__);
	default:
		pr_debug_ratelimited("%s: Non zero urb status = %d\n",
				__func__, urb->status);
		goto resubmit_int_urb;
	}

	cpkt = urb->transfer_buffer;
	cpkt_size = urb->actual_length;
	if (!cpkt_size) {
		dev->zlp_cnt++;
		dev_dbg(&dev->intf->dev, "%s: zero length pkt received\n",
				__func__);
		goto resubmit_int_urb;
	}

	list_elem = kmalloc(sizeof(struct ctrl_pkt_list_elem), GFP_ATOMIC);
	if (!list_elem) {
		dev_err(&dev->intf->dev, "%s: list_elem alloc failed\n",
				__func__);
		return;
	}
	list_elem->cpkt.data = kmalloc(cpkt_size, GFP_ATOMIC);
	if (!list_elem->cpkt.data) {
		dev_err(&dev->intf->dev, "%s: list_elem->data alloc failed\n",
			__func__);
		kfree(list_elem);
		return;
	}
	memcpy(list_elem->cpkt.data, cpkt, cpkt_size);
	list_elem->cpkt.data_size = cpkt_size;

	ch_id = dev->ctrldev_id;

	if (test_bit(RMNET_CTRL_DEV_MUX_EN, &dev->status)) {
		ch_id = rmnet_usb_ctrl_dmux(list_elem);
		if (ch_id < 0) {
			dev->invalid_mux_id_cnt++;
			kfree(list_elem->cpkt.data);
			kfree(list_elem);
			goto resubmit_int_urb;
		}
	}

	rx_dev = &ctrl_devs[dev->rdev_num][ch_id];

	dev->get_encap_resp_cnt++;
	dev_dbg(&dev->intf->dev, "Read %d bytes for %s\n",
		urb->actual_length, rx_dev->name);

	spin_lock(&rx_dev->rx_lock);
	list_add_tail(&list_elem->list, &rx_dev->rx_list);
	spin_unlock(&rx_dev->rx_lock);

	wake_up(&rx_dev->read_wait_queue);

resubmit_int_urb:
	/*check if it is already submitted in resume*/
	if (!dev->inturb->anchor) {
		usb_mark_last_busy(udev);
		usb_anchor_urb(dev->inturb, &dev->rx_submitted);
		status = usb_submit_urb(dev->inturb, GFP_ATOMIC);
		if (status) {
			usb_unanchor_urb(dev->inturb);
			if (status != -ENODEV)
				pr_err("%s: Error re-submitting Int URB %d\n",
				__func__, status);
		}
	}
}

int rmnet_usb_ctrl_start_rx(struct rmnet_ctrl_udev *dev)
{
	int	retval = 0;

	usb_anchor_urb(dev->inturb, &dev->rx_submitted);
	retval = usb_submit_urb(dev->inturb, GFP_KERNEL);
	if (retval < 0) {
		usb_unanchor_urb(dev->inturb);
		if (retval != -ENODEV)
			pr_err("%s Intr submit %d\n", __func__, retval);
	}

	return retval;
}

static int rmnet_usb_ctrl_alloc_rx(struct rmnet_ctrl_udev *dev)
{
	dev->rcvurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->rcvurb) {
		pr_err("%s: Error allocating read urb\n", __func__);
		goto nomem;
	}

	dev->rcvbuf = kmalloc(DEFAULT_READ_URB_LENGTH, GFP_KERNEL);
	if (!dev->rcvbuf) {
		pr_err("%s: Error allocating read buffer\n", __func__);
		goto nomem;
	}

	dev->in_ctlreq = kmalloc(sizeof(*dev->in_ctlreq), GFP_KERNEL);
	if (!dev->in_ctlreq) {
		pr_err("%s: Error allocating setup packet buffer\n", __func__);
		goto nomem;
	}

	return 0;

nomem:
	usb_free_urb(dev->rcvurb);
	kfree(dev->rcvbuf);
	kfree(dev->in_ctlreq);

	return -ENOMEM;

}
static int rmnet_usb_ctrl_write_cmd(struct rmnet_ctrl_udev *dev, u8 req,
		u16 val, void *data, u16 size)
{
	struct usb_device	*udev;
	int			ret;

	if (!test_bit(RMNET_CTRL_DEV_READY, &dev->status))
		return -ENETRESET;

	ret = usb_autopm_get_interface(dev->intf);
	if (ret < 0) {
		pr_debug("%s: Unable to resume interface: %d\n",
			__func__, ret);
		return ret;
	}

	udev = interface_to_usbdev(dev->intf);
	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
		req,
		(USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE),
		val,
		dev->intf->cur_altsetting->desc.bInterfaceNumber,
		data, size, USB_CTRL_SET_TIMEOUT);
	if (ret < 0)
		dev->tx_ctrl_err_cnt++;

	/* if we are here after device disconnect
	 * usb_unbind_interface() takes care of
	 * residual pm_autopm_get_interface_* calls
	 */
	if (test_bit(RMNET_CTRL_DEV_READY, &dev->status))
		usb_autopm_put_interface(dev->intf);

	return ret;
}

static void rmnet_usb_ctrl_free_rx_list(struct rmnet_ctrl_dev *dev)
{
	unsigned long flag;
	struct ctrl_pkt_list_elem *list_elem = NULL;

	spin_lock_irqsave(&dev->rx_lock, flag);
	while (!list_empty(&dev->rx_list)) {
		list_elem = list_first_entry(&dev->rx_list,
				struct ctrl_pkt_list_elem, list);
		list_del(&list_elem->list);
		kfree(list_elem->cpkt.data);
		kfree(list_elem);
	}
	spin_unlock_irqrestore(&dev->rx_lock, flag);
}

static int rmnet_ctl_open(struct inode *inode, struct file *file)
{
	int				retval = 0;
	struct rmnet_ctrl_dev		*dev =
		container_of(inode->i_cdev, struct rmnet_ctrl_dev, cdev);

	if (!dev)
		return -ENODEV;

	if (test_bit(RMNET_CTRL_DEV_OPEN, &dev->status))
		goto already_opened;

	if (dev->mdm_wait_timeout &&
			!test_bit(RMNET_CTRL_DEV_READY, &dev->cudev->status)) {
		retval = wait_event_interruptible_timeout(
				dev->open_wait_queue,
				test_bit(RMNET_CTRL_DEV_READY,
					&dev->cudev->status),
				msecs_to_jiffies(dev->mdm_wait_timeout * 1000));
		if (retval == 0) {
			dev_err(dev->devicep, "%s: Timeout opening %s\n",
						__func__, dev->name);
			return -ETIMEDOUT;
		} else if (retval < 0) {
			dev_err(dev->devicep, "%s: Error waiting for %s\n",
						__func__, dev->name);
			return retval;
		}
	}

	if (!test_bit(RMNET_CTRL_DEV_READY, &dev->cudev->status)) {
		dev_dbg(dev->devicep, "%s: Connection timedout opening %s\n",
					__func__, dev->name);
		return -ETIMEDOUT;
	}

	/* clear stale data if device close called but channel was ready */
	rmnet_usb_ctrl_free_rx_list(dev);

	set_bit(RMNET_CTRL_DEV_OPEN, &dev->status);

	file->private_data = dev;

already_opened:
	DBG("%s: Open called for %s\n", __func__, dev->name);

	return 0;
}

static int rmnet_ctl_release(struct inode *inode, struct file *file)
{
	struct rmnet_ctrl_dev		*dev;

	dev = file->private_data;
	if (!dev)
		return -ENODEV;

	DBG("%s Called on %s device\n", __func__, dev->name);

	clear_bit(RMNET_CTRL_DEV_OPEN, &dev->status);

	file->private_data = NULL;

	return 0;
}

static unsigned int rmnet_ctl_poll(struct file *file, poll_table *wait)
{
	unsigned int		mask = 0;
	struct rmnet_ctrl_dev	*dev;
	unsigned long		flags;

	dev = file->private_data;
	if (!dev)
		return POLLERR;

	poll_wait(file, &dev->read_wait_queue, wait);
	if (!dev->poll_err &&
			!test_bit(RMNET_CTRL_DEV_READY, &dev->cudev->status)) {
		dev_dbg(dev->devicep, "%s: Device not connected\n", __func__);
		dev->poll_err = true;
		return POLLERR;
	}

	if (dev->poll_err)
		dev->poll_err = false;

	spin_lock_irqsave(&dev->rx_lock, flags);
	if (!list_empty(&dev->rx_list))
		mask |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&dev->rx_lock, flags);

	return mask;
}

static ssize_t rmnet_ctl_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	int				retval = 0;
	int				bytes_to_read;
	unsigned int			hdr_len = 0;
	struct rmnet_ctrl_dev		*dev;
	struct ctrl_pkt_list_elem	*list_elem = NULL;
	unsigned long			flags;

	dev = file->private_data;
	if (!dev)
		return -ENODEV;

	DBG("%s: Read from %s\n", __func__, dev->name);

ctrl_read:
	if (!test_bit(RMNET_CTRL_DEV_READY, &dev->cudev->status)) {
		dev_dbg(dev->devicep, "%s: Device not connected\n",
			__func__);
		return -ENETRESET;
	}
	spin_lock_irqsave(&dev->rx_lock, flags);
	if (list_empty(&dev->rx_list)) {
		spin_unlock_irqrestore(&dev->rx_lock, flags);

		retval = wait_event_interruptible(dev->read_wait_queue,
				!list_empty(&dev->rx_list) ||
				!test_bit(RMNET_CTRL_DEV_READY,
					&dev->cudev->status));
		if (retval < 0)
			return retval;

		goto ctrl_read;
	}

	list_elem = list_first_entry(&dev->rx_list,
				     struct ctrl_pkt_list_elem, list);
	bytes_to_read = (uint32_t)(list_elem->cpkt.data_size);
	if (bytes_to_read > count) {
		spin_unlock_irqrestore(&dev->rx_lock, flags);
		dev_err(dev->devicep, "%s: Packet size %d > buf size %d\n",
			__func__, bytes_to_read, count);
		return -ENOMEM;
	}
	spin_unlock_irqrestore(&dev->rx_lock, flags);

	if (test_bit(RMNET_CTRL_DEV_MUX_EN, &dev->status))
		hdr_len = sizeof(struct mux_hdr);

	if (copy_to_user(buf, list_elem->cpkt.data + hdr_len, bytes_to_read)) {
			dev_err(dev->devicep,
				"%s: copy_to_user failed for %s\n",
				__func__, dev->name);
		return -EFAULT;
	}
	spin_lock_irqsave(&dev->rx_lock, flags);
	list_del(&list_elem->list);
	spin_unlock_irqrestore(&dev->rx_lock, flags);

	kfree(list_elem->cpkt.data);
	kfree(list_elem);
	DBG("%s: Returning %d bytes to %s\n", __func__, bytes_to_read,
			dev->name);
	DUMP_BUFFER("Read: ", bytes_to_read, buf);

	return bytes_to_read;
}

static ssize_t rmnet_ctl_write(struct file *file, const char __user * buf,
		size_t size, loff_t *pos)
{
	int			status;
	size_t			total_len;
	void			*wbuf;
	void			*actual_data;
	struct ctrl_pkt		*cpkt;
	struct rmnet_ctrl_dev	*dev = file->private_data;

	if (!dev)
		return -ENODEV;

	if (size <= 0)
		return -EINVAL;

	if (!test_bit(RMNET_CTRL_DEV_READY, &dev->cudev->status))
		return -ENETRESET;

	DBG("%s: Writing %i bytes on %s\n", __func__, size, dev->name);

	total_len = size;

	if (test_bit(RMNET_CTRL_DEV_MUX_EN, &dev->status))
		total_len += sizeof(struct mux_hdr) + MAX_PAD_BYTES(4);

	wbuf = kmalloc(total_len , GFP_KERNEL);
	if (!wbuf)
		return -ENOMEM;

	cpkt = kmalloc(sizeof(struct ctrl_pkt), GFP_KERNEL);
	if (!cpkt) {
		kfree(wbuf);
		return -ENOMEM;
	}
	actual_data = cpkt->data = wbuf;
	cpkt->data_size = total_len;
	cpkt->ctxt = dev;

	if (test_bit(RMNET_CTRL_DEV_MUX_EN, &dev->status)) {
		actual_data = wbuf + sizeof(struct mux_hdr);
		rmnet_usb_ctrl_mux(dev->ch_id, cpkt);
	}

	status = copy_from_user(actual_data, buf, size);
	if (status) {
		dev_err(dev->devicep,
		"%s: Unable to copy data from userspace %d\n",
		__func__, status);
		kfree(wbuf);
		kfree(cpkt);
		return status;
	}
	DUMP_BUFFER("Write: ", size, buf);

	status = rmnet_usb_ctrl_write_cmd(dev->cudev,
			USB_CDC_SEND_ENCAPSULATED_COMMAND, 0, cpkt->data,
			cpkt->data_size);

	kfree(cpkt->data);
	kfree(cpkt);

	if (status > 0) {
		dev->cudev->snd_encap_cmd_cnt++;
		return size;
	}

	return status;
}

static int rmnet_ctrl_tiocmset(struct rmnet_ctrl_dev *dev, unsigned int set,
		unsigned int clear)
{
	int retval;

	mutex_lock(&dev->dev_lock);
	if (set & TIOCM_DTR)
		dev->cbits_tomdm |= ACM_CTRL_DTR;

	/*
	 * TBD if (set & TIOCM_RTS)
	 *	dev->cbits_tomdm |= ACM_CTRL_RTS;
	 */

	if (clear & TIOCM_DTR)
		dev->cbits_tomdm &= ~ACM_CTRL_DTR;

	/*
	 * (clear & TIOCM_RTS)
	 *	dev->cbits_tomdm &= ~ACM_CTRL_RTS;
	 */

	mutex_unlock(&dev->dev_lock);

	retval = rmnet_usb_ctrl_write_cmd(dev->cudev,
			USB_CDC_REQ_SET_CONTROL_LINE_STATE, 0, NULL, 0);
	if (!retval)
		dev->cudev->set_ctrl_line_state_cnt++;

	return retval;
}

static int rmnet_ctrl_tiocmget(struct rmnet_ctrl_dev *dev)
{
	int	ret;

	mutex_lock(&dev->dev_lock);
	ret =
	/*
	 * TBD(dev->cbits_tolocal & ACM_CTRL_DSR ? TIOCM_DSR : 0) |
	 * (dev->cbits_tolocal & ACM_CTRL_CTS ? TIOCM_CTS : 0) |
	 */
	(dev->cbits_tolocal & ACM_CTRL_CD ? TIOCM_CD : 0) |
	/*
	 * TBD (dev->cbits_tolocal & ACM_CTRL_RI ? TIOCM_RI : 0) |
	 *(dev->cbits_tomdm & ACM_CTRL_RTS ? TIOCM_RTS : 0) |
	*/
	(dev->cbits_tomdm & ACM_CTRL_DTR ? TIOCM_DTR : 0);
	mutex_unlock(&dev->dev_lock);

	return ret;
}

static long rmnet_ctrl_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int			ret;
	struct rmnet_ctrl_dev	*dev;

	dev = file->private_data;
	if (!dev)
		return -ENODEV;

	switch (cmd) {
	case TIOCMGET:

		ret = rmnet_ctrl_tiocmget(dev);
		break;
	case TIOCMSET:
		ret = rmnet_ctrl_tiocmset(dev, arg, ~arg);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations ctrldev_fops = {
	.owner = THIS_MODULE,
	.read  = rmnet_ctl_read,
	.write = rmnet_ctl_write,
	.unlocked_ioctl = rmnet_ctrl_ioctl,
	.open  = rmnet_ctl_open,
	.release = rmnet_ctl_release,
	.poll = rmnet_ctl_poll,
};

int rmnet_usb_ctrl_probe(struct usb_interface *intf,
			 struct usb_host_endpoint *int_in,
			 unsigned long rmnet_devnum,
			 unsigned long *data)
{
	struct rmnet_ctrl_udev		*cudev;
	struct rmnet_ctrl_dev		*dev = NULL;
	u16				wMaxPacketSize;
	struct usb_endpoint_descriptor	*ep;
	struct usb_device		*udev = interface_to_usbdev(intf);
	int				interval;
	int				ret = 0, n;

	/* Find next available ctrl_dev */
	for (n = 0; n < insts_per_dev; n++) {
		dev = &ctrl_devs[rmnet_devnum][n];
		if (!dev->claimed)
			break;
	}

	if (!dev || n == insts_per_dev) {
		pr_err("%s: No available ctrl devices for %lu\n", __func__,
			rmnet_devnum);
		return -ENODEV;
	}

	cudev = dev->cudev;

	cudev->int_pipe = usb_rcvintpipe(udev,
		int_in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);

	cudev->intf = intf;

	cudev->inturb = usb_alloc_urb(0, GFP_KERNEL);
	if (!cudev->inturb) {
		dev_err(&intf->dev, "Error allocating int urb\n");
		kfree(cudev);
		return -ENOMEM;
	}

	/*use max pkt size from ep desc*/
	ep = &cudev->intf->cur_altsetting->endpoint[0].desc;
	wMaxPacketSize = le16_to_cpu(ep->wMaxPacketSize);

	cudev->intbuf = kmalloc(wMaxPacketSize, GFP_KERNEL);
	if (!cudev->intbuf) {
		usb_free_urb(cudev->inturb);
		kfree(cudev);
		dev_err(&intf->dev, "Error allocating int buffer\n");
		return -ENOMEM;
	}

	cudev->in_ctlreq->bRequestType =
		(USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE);
	cudev->in_ctlreq->bRequest  = USB_CDC_GET_ENCAPSULATED_RESPONSE;
	cudev->in_ctlreq->wValue = 0;
	cudev->in_ctlreq->wIndex =
		cudev->intf->cur_altsetting->desc.bInterfaceNumber;
	cudev->in_ctlreq->wLength = cpu_to_le16(DEFAULT_READ_URB_LENGTH);

	interval = int_in->desc.bInterval;

	usb_fill_int_urb(cudev->inturb, udev,
			 cudev->int_pipe,
			 cudev->intbuf, wMaxPacketSize,
			 notification_available_cb, cudev, interval);

	usb_mark_last_busy(udev);
	ret = rmnet_usb_ctrl_start_rx(cudev);
	if (ret) {
		usb_free_urb(cudev->inturb);
		kfree(cudev->intbuf);
		kfree(cudev);
		return ret;
	}

	*data = (unsigned long)cudev;


	/* If MUX is enabled, wakeup the open process here */
	if (test_bit(RMNET_CTRL_DEV_MUX_EN, &cudev->status)) {
		set_bit(RMNET_CTRL_DEV_READY, &cudev->status);
		for (n = 0; n < insts_per_dev; n++) {
			dev = &ctrl_devs[rmnet_devnum][n];
			wake_up(&dev->open_wait_queue);
		}
	} else {
		cudev->ctrldev_id = n;
		dev->claimed = true;
	}

	return 0;
}

void rmnet_usb_ctrl_disconnect(struct rmnet_ctrl_udev *dev)
{
	struct rmnet_ctrl_dev *cdev;
	int n;

	clear_bit(RMNET_CTRL_DEV_READY, &dev->status);

	if (test_bit(RMNET_CTRL_DEV_MUX_EN, &dev->status)) {
		for (n = 0; n < insts_per_dev; n++) {
			cdev = &ctrl_devs[dev->rdev_num][n];
			rmnet_usb_ctrl_free_rx_list(cdev);
			wake_up(&cdev->read_wait_queue);
			mutex_lock(&cdev->dev_lock);
			cdev->cbits_tolocal = ~ACM_CTRL_CD;
			cdev->cbits_tomdm = ~ACM_CTRL_DTR;
			mutex_unlock(&cdev->dev_lock);
		}
	} else {
		cdev = &ctrl_devs[dev->rdev_num][dev->ctrldev_id];
		cdev->claimed = false;
		rmnet_usb_ctrl_free_rx_list(cdev);
		wake_up(&cdev->read_wait_queue);
		mutex_lock(&cdev->dev_lock);
		cdev->cbits_tolocal = ~ACM_CTRL_CD;
		cdev->cbits_tomdm = ~ACM_CTRL_DTR;
		mutex_unlock(&cdev->dev_lock);
	}

	cancel_work_sync(&dev->get_encap_work);

	usb_kill_anchored_urbs(&dev->tx_submitted);
	usb_kill_anchored_urbs(&dev->rx_submitted);

	usb_free_urb(dev->inturb);
	dev->inturb = NULL;

	kfree(dev->intbuf);
	dev->intbuf = NULL;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	4096
static ssize_t rmnet_usb_ctrl_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct rmnet_ctrl_udev	*dev;
	struct rmnet_ctrl_dev	*cdev;
	char			*buf;
	int			ret;
	int			i, n;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < num_devs; i++) {
		for (n = 0; n < insts_per_dev; n++) {
			cdev = &ctrl_devs[i][n];
			dev = cdev->cudev;
			temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
					"\n#ctrl_dev: %p     Name: %s#\n"
					"snd encap cmd cnt         %u\n"
					"resp avail cnt:           %u\n"
					"get encap resp cnt:       %u\n"
					"set ctrl line state cnt:  %u\n"
					"tx_err_cnt:               %u\n"
					"cbits_tolocal:            %d\n"
					"cbits_tomdm:              %d\n"
					"mdm_wait_timeout:         %u\n"
					"zlp_cnt:                  %u\n"
					"get_encap_failure_cnt     %u\n"
					"ignore_encap_work         %u\n"
					"invalid mux id cnt        %u\n"
					"RMNET_CTRL_DEV_MUX_EN:    %d\n"
					"RMNET_CTRL_DEV_OPEN:      %d\n"
					"RMNET_CTRL_DEV_READY:     %d\n",
					cdev, cdev->name,
					dev->snd_encap_cmd_cnt,
					dev->resp_avail_cnt,
					dev->get_encap_resp_cnt,
					dev->set_ctrl_line_state_cnt,
					dev->tx_ctrl_err_cnt,
					cdev->cbits_tolocal,
					cdev->cbits_tomdm,
					cdev->mdm_wait_timeout,
					dev->zlp_cnt,
					dev->get_encap_failure_cnt,
					dev->ignore_encap_work,
					dev->invalid_mux_id_cnt,
					test_bit(RMNET_CTRL_DEV_MUX_EN,
							&dev->status),
					test_bit(RMNET_CTRL_DEV_OPEN,
							&dev->status),
					test_bit(RMNET_CTRL_DEV_READY,
							&dev->status));
		}
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return ret;
}

static ssize_t rmnet_usb_ctrl_reset_stats(struct file *file, const char __user *
		buf, size_t count, loff_t *ppos)
{
	struct rmnet_ctrl_udev	*dev;
	struct rmnet_ctrl_dev	*cdev;
	int			i, n;

	for (i = 0; i < num_devs; i++) {
		for (n = 0; n < insts_per_dev; n++) {
			cdev = &ctrl_devs[i][n];
			dev = cdev->cudev;

			dev->snd_encap_cmd_cnt = 0;
			dev->resp_avail_cnt = 0;
			dev->get_encap_resp_cnt = 0;
			dev->set_ctrl_line_state_cnt = 0;
			dev->tx_ctrl_err_cnt = 0;
			dev->zlp_cnt = 0;
			dev->invalid_mux_id_cnt = 0;
			dev->ignore_encap_work = 0;
		}
	}
	return count;
}

const struct file_operations rmnet_usb_ctrl_stats_ops = {
	.read = rmnet_usb_ctrl_read_stats,
	.write = rmnet_usb_ctrl_reset_stats,
};

struct dentry	*usb_ctrl_dent;
struct dentry	*usb_ctrl_dfile;
static void rmnet_usb_ctrl_debugfs_init(void)
{
	usb_ctrl_dent = debugfs_create_dir("rmnet_usb_ctrl", 0);
	if (IS_ERR(usb_ctrl_dent))
		return;

	usb_ctrl_dfile = debugfs_create_file("status", 0644, usb_ctrl_dent, 0,
			&rmnet_usb_ctrl_stats_ops);
	if (!usb_ctrl_dfile || IS_ERR(usb_ctrl_dfile))
		debugfs_remove(usb_ctrl_dent);
}

static void rmnet_usb_ctrl_debugfs_exit(void)
{
	debugfs_remove(usb_ctrl_dfile);
	debugfs_remove(usb_ctrl_dent);
}

#else
static void rmnet_usb_ctrl_debugfs_init(void) { }
static void rmnet_usb_ctrl_debugfs_exit(void) { }
#endif

static void free_rmnet_ctrl_udev(struct rmnet_ctrl_udev *cudev)
{
	kfree(cudev->in_ctlreq);
	kfree(cudev->rcvbuf);
	kfree(cudev->intbuf);
	usb_free_urb(cudev->rcvurb);
	usb_free_urb(cudev->inturb);
	destroy_workqueue(cudev->wq);
	kfree(cudev);
}

int rmnet_usb_ctrl_init(int no_rmnet_devs, int no_rmnet_insts_per_dev,
		unsigned long mux_info)
{
	struct rmnet_ctrl_dev	*dev;
	struct rmnet_ctrl_udev	*cudev;
	int			i, n;
	int			status;
	int			cmux_enabled;

	num_devs = no_rmnet_devs;
	insts_per_dev = no_rmnet_insts_per_dev;

	ctrl_devs = kzalloc(num_devs * sizeof(*ctrl_devs), GFP_KERNEL);
	if (!ctrl_devs)
		return -ENOMEM;

	for (i = 0; i < num_devs; i++) {
		ctrl_devs[i] = kzalloc(insts_per_dev * sizeof(*ctrl_devs[i]),
				       GFP_KERNEL);
		if (!ctrl_devs[i])
			return -ENOMEM;

		status = alloc_chrdev_region(&ctrldev_num[i], 0, insts_per_dev,
					     rmnet_dev_names[i]);
		if (IS_ERR_VALUE(status)) {
			pr_err("ERROR:%s: alloc_chrdev_region() ret %i.\n",
				__func__, status);
			return status;
		}

		ctrldev_classp[i] = class_create(THIS_MODULE,
						 rmnet_dev_names[i]);
		if (IS_ERR(ctrldev_classp[i])) {
			pr_err("ERROR:%s: class_create() ENOMEM\n", __func__);
			status = PTR_ERR(ctrldev_classp[i]);
			return status;
		}

		for (n = 0; n < insts_per_dev; n++) {
			dev = &ctrl_devs[i][n];

			/*for debug purpose*/
			snprintf(dev->name, CTRL_DEV_MAX_LEN, "%s%d",
				 rmnet_dev_names[i], n);

			/* ctrl usb dev inits */
			cmux_enabled = test_bit(i, &mux_info);
			if (n && cmux_enabled)
				/* for mux config one cudev maps to n dev */
				goto skip_cudev_init;

			cudev = kzalloc(sizeof(*cudev), GFP_KERNEL);
			if (!cudev) {
				pr_err("Error allocating rmnet usb ctrl dev\n");
				kfree(dev);
				return -ENOMEM;
			}

			cudev->rdev_num = i;
			cudev->wq = create_singlethread_workqueue(dev->name);
			if (!cudev->wq) {
				pr_err("unable to allocate workqueue");
				kfree(cudev);
				kfree(dev);
				return -ENOMEM;
			}

			init_usb_anchor(&cudev->tx_submitted);
			init_usb_anchor(&cudev->rx_submitted);
			INIT_WORK(&cudev->get_encap_work, get_encap_work);

			status = rmnet_usb_ctrl_alloc_rx(cudev);
			if (status) {
				destroy_workqueue(cudev->wq);
				kfree(cudev);
				kfree(dev);
				return status;
			}

skip_cudev_init:
			/* ctrl dev inits */
			dev->cudev = cudev;

			if (cmux_enabled) {
				set_bit(RMNET_CTRL_DEV_MUX_EN, &dev->status);
				set_bit(RMNET_CTRL_DEV_MUX_EN,
						&dev->cudev->status);
			}

			dev->ch_id = n;

			mutex_init(&dev->dev_lock);
			spin_lock_init(&dev->rx_lock);
			init_waitqueue_head(&dev->read_wait_queue);
			init_waitqueue_head(&dev->open_wait_queue);
			INIT_LIST_HEAD(&dev->rx_list);

			cdev_init(&dev->cdev, &ctrldev_fops);
			dev->cdev.owner = THIS_MODULE;

			status = cdev_add(&dev->cdev, (ctrldev_num[i] + n), 1);
			if (status) {
				pr_err("%s: cdev_add() ret %i\n", __func__,
					status);
				free_rmnet_ctrl_udev(dev->cudev);
				kfree(dev);
				return status;
			}

			dev->devicep = device_create(ctrldev_classp[i], NULL,
						     (ctrldev_num[i] + n), NULL,
						     "%s%d", rmnet_dev_names[i],
						     n);
			if (IS_ERR(dev->devicep)) {
				pr_err("%s: device_create() returned %ld\n",
					__func__, PTR_ERR(dev->devicep));
				cdev_del(&dev->cdev);
				free_rmnet_ctrl_udev(dev->cudev);
				kfree(dev);
				return PTR_ERR(dev->devicep);
			}

			/*create /sys/class/hsicctl/hsicctlx/modem_wait*/
			status = device_create_file(dev->devicep,
						    &dev_attr_modem_wait);
			if (status) {
				device_destroy(dev->devicep->class,
					       dev->devicep->devt);
				cdev_del(&dev->cdev);
				free_rmnet_ctrl_udev(dev->cudev);
				kfree(dev);
				return status;
			}
			dev_set_drvdata(dev->devicep, dev);
		}
	}

	rmnet_usb_ctrl_debugfs_init();
	pr_info("rmnet usb ctrl Initialized.\n");
	return 0;
}

static void free_rmnet_ctrl_dev(struct rmnet_ctrl_dev *dev)
{
	device_remove_file(dev->devicep, &dev_attr_modem_wait);
	cdev_del(&dev->cdev);
	device_destroy(dev->devicep->class,
		       dev->devicep->devt);
}

void rmnet_usb_ctrl_exit(int no_rmnet_devs, int no_rmnet_insts_per_dev,
	unsigned long mux_info)
{
	int i, n;

	for (i = 0; i < no_rmnet_devs; i++) {
		for (n = 0; n < no_rmnet_insts_per_dev; n++) {
			free_rmnet_ctrl_dev(&ctrl_devs[i][n]);
			if (n && test_bit(i, &mux_info))
				continue;
			free_rmnet_ctrl_udev((&ctrl_devs[i][n])->cudev);
		}

		kfree(ctrl_devs[i]);

		class_destroy(ctrldev_classp[i]);
		if (ctrldev_num[i])
			unregister_chrdev_region(ctrldev_num[i], insts_per_dev);
	}

	kfree(ctrl_devs);
	rmnet_usb_ctrl_debugfs_exit();
}

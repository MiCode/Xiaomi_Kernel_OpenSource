/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include "rmnet_usb_ctrl.h"

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

/* polling interval for Interrupt ep */
#define HS_INTERVAL		7
#define FS_LS_INTERVAL		3

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
};

struct ctrl_pkt_list_elem {
	struct list_head	list;
	struct ctrl_pkt		cpkt;
};

static void resp_avail_cb(struct urb *);

static int is_dev_connected(struct rmnet_ctrl_dev *dev)
{
	if (dev) {
		mutex_lock(&dev->dev_lock);
		if (!dev->is_connected) {
			mutex_unlock(&dev->dev_lock);
			return 0;
		}
		mutex_unlock(&dev->dev_lock);
		return 1;
	}
	return 0;
}

static void get_encap_work(struct work_struct *w)
{
	struct usb_device	*udev;
	struct rmnet_ctrl_dev	*dev =
			container_of(w, struct rmnet_ctrl_dev, get_encap_work);
	int			status;

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
		dev_err(dev->devicep,
		"%s: Error submitting Read URB %d\n", __func__, status);
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
			dev_err(dev->devicep, "%s: Error re-submitting Int URB %d\n",
				__func__, status);
		}
	}
}

static void notification_available_cb(struct urb *urb)
{
	int				status;
	struct usb_cdc_notification	*ctrl;
	struct usb_device		*udev;
	struct rmnet_ctrl_dev		*dev = urb->context;

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

		usb_mark_last_busy(udev);
		queue_work(dev->wq, &dev->get_encap_work);

		if (!dev->resp_available) {
			dev->resp_available = true;
			wake_up(&dev->open_wait_queue);
		}

		return;
	default:
		 dev_err(dev->devicep,
			"%s:Command not implemented\n", __func__);
	}

resubmit_int_urb:
	usb_anchor_urb(urb, &dev->rx_submitted);
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status) {
		usb_unanchor_urb(urb);
		dev_err(dev->devicep, "%s: Error re-submitting Int URB %d\n",
		__func__, status);
	}

	return;
}

static void resp_avail_cb(struct urb *urb)
{
	struct usb_device		*udev;
	struct ctrl_pkt_list_elem	*list_elem = NULL;
	struct rmnet_ctrl_dev		*dev = urb->context;
	void				*cpkt;
	int				status = 0;
	size_t				cpkt_size = 0;

	udev = interface_to_usbdev(dev->intf);

	usb_autopm_put_interface_async(dev->intf);

	switch (urb->status) {
	case 0:
		/*success*/
		dev->get_encap_resp_cnt++;
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

	dev_dbg(dev->devicep, "Read %d bytes for %s\n",
		urb->actual_length, dev->name);

	cpkt = urb->transfer_buffer;
	cpkt_size = urb->actual_length;
	if (!cpkt_size) {
		dev->zlp_cnt++;
		dev_dbg(dev->devicep, "%s: zero length pkt received\n",
				__func__);
		goto resubmit_int_urb;
	}

	list_elem = kmalloc(sizeof(struct ctrl_pkt_list_elem), GFP_ATOMIC);
	if (!list_elem) {
		dev_err(dev->devicep, "%s: list_elem alloc failed\n", __func__);
		return;
	}
	list_elem->cpkt.data = kmalloc(cpkt_size, GFP_ATOMIC);
	if (!list_elem->cpkt.data) {
		dev_err(dev->devicep, "%s: list_elem->data alloc failed\n",
			__func__);
		kfree(list_elem);
		return;
	}
	memcpy(list_elem->cpkt.data, cpkt, cpkt_size);
	list_elem->cpkt.data_size = cpkt_size;
	spin_lock(&dev->rx_lock);
	list_add_tail(&list_elem->list, &dev->rx_list);
	spin_unlock(&dev->rx_lock);

	wake_up(&dev->read_wait_queue);

resubmit_int_urb:
	/*check if it is already submitted in resume*/
	if (!dev->inturb->anchor) {
		usb_mark_last_busy(udev);
		usb_anchor_urb(dev->inturb, &dev->rx_submitted);
		status = usb_submit_urb(dev->inturb, GFP_ATOMIC);
		if (status) {
			usb_unanchor_urb(dev->inturb);
			dev_err(dev->devicep, "%s: Error re-submitting Int URB %d\n",
				__func__, status);
		}
	}
}

int rmnet_usb_ctrl_start_rx(struct rmnet_ctrl_dev *dev)
{
	int	retval = 0;

	usb_anchor_urb(dev->inturb, &dev->rx_submitted);
	retval = usb_submit_urb(dev->inturb, GFP_KERNEL);
	if (retval < 0) {
		usb_unanchor_urb(dev->inturb);
		dev_err(dev->devicep, "%s Intr submit %d\n", __func__,
				retval);
	}

	return retval;
}

static int rmnet_usb_ctrl_alloc_rx(struct rmnet_ctrl_dev *dev)
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
static int rmnet_usb_ctrl_write_cmd(struct rmnet_ctrl_dev *dev)
{
	struct usb_device	*udev;

	if (!is_dev_connected(dev))
		return -ENODEV;

	udev = interface_to_usbdev(dev->intf);
	dev->set_ctrl_line_state_cnt++;
	return usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
		USB_CDC_REQ_SET_CONTROL_LINE_STATE,
		(USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE),
		dev->cbits_tomdm,
		dev->intf->cur_altsetting->desc.bInterfaceNumber,
		NULL, 0, USB_CTRL_SET_TIMEOUT);
}

static void ctrl_write_callback(struct urb *urb)
{
	struct rmnet_ctrl_dev	*dev = urb->context;

	if (urb->status) {
		dev->tx_ctrl_err_cnt++;
		pr_debug_ratelimited("Write status/size %d/%d\n",
				urb->status, urb->actual_length);
	}

	kfree(urb->setup_packet);
	kfree(urb->transfer_buffer);
	usb_free_urb(urb);
	usb_autopm_put_interface_async(dev->intf);
}

static int rmnet_usb_ctrl_write(struct rmnet_ctrl_dev *dev, char *buf,
		size_t size)
{
	int			result;
	struct urb		*sndurb;
	struct usb_ctrlrequest	*out_ctlreq;
	struct usb_device	*udev;

	if (!is_dev_connected(dev))
		return -ENETRESET;

	udev = interface_to_usbdev(dev->intf);

	sndurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!sndurb) {
		dev_err(dev->devicep, "Error allocating read urb\n");
		return -ENOMEM;
	}

	out_ctlreq = kmalloc(sizeof(*out_ctlreq), GFP_KERNEL);
	if (!out_ctlreq) {
		usb_free_urb(sndurb);
		dev_err(dev->devicep, "Error allocating setup packet buffer\n");
		return -ENOMEM;
	}

	/* CDC Send Encapsulated Request packet */
	out_ctlreq->bRequestType = (USB_DIR_OUT | USB_TYPE_CLASS |
			     USB_RECIP_INTERFACE);
	out_ctlreq->bRequest = USB_CDC_SEND_ENCAPSULATED_COMMAND;
	out_ctlreq->wValue = 0;
	out_ctlreq->wIndex = dev->intf->cur_altsetting->desc.bInterfaceNumber;
	out_ctlreq->wLength = cpu_to_le16(size);

	usb_fill_control_urb(sndurb, udev,
			     usb_sndctrlpipe(udev, 0),
			     (unsigned char *)out_ctlreq, (void *)buf, size,
			     ctrl_write_callback, dev);

	result = usb_autopm_get_interface(dev->intf);
	if (result < 0) {
		dev_dbg(dev->devicep, "%s: Unable to resume interface: %d\n",
			__func__, result);

		/*
		* Revisit:  if (result == -EPERM)
		*		rmnet_usb_suspend(dev->intf, PMSG_SUSPEND);
		*/

		usb_free_urb(sndurb);
		kfree(out_ctlreq);
		return result;
	}

	usb_anchor_urb(sndurb, &dev->tx_submitted);
	dev->snd_encap_cmd_cnt++;
	result = usb_submit_urb(sndurb, GFP_KERNEL);
	if (result < 0) {
		dev_err(dev->devicep, "%s: Submit URB error %d\n",
			__func__, result);
		dev->snd_encap_cmd_cnt--;
		usb_autopm_put_interface(dev->intf);
		usb_unanchor_urb(sndurb);
		usb_free_urb(sndurb);
		kfree(out_ctlreq);
		return result;
	}

	return size;
}

static int rmnet_ctl_open(struct inode *inode, struct file *file)
{
	int			retval = 0;
	struct rmnet_ctrl_dev	*dev =
		container_of(inode->i_cdev, struct rmnet_ctrl_dev, cdev);

	if (!dev)
		return -ENODEV;

	if (dev->is_opened)
		goto already_opened;

	/*block open to get first response available from mdm*/
	if (dev->mdm_wait_timeout && !dev->resp_available) {
		retval = wait_event_interruptible_timeout(
					dev->open_wait_queue,
					dev->resp_available,
					msecs_to_jiffies(dev->mdm_wait_timeout *
									1000));
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

	if (!dev->resp_available) {
		dev_dbg(dev->devicep, "%s: Connection timedout opening %s\n",
					__func__, dev->name);
		return -ETIMEDOUT;
	}

	mutex_lock(&dev->dev_lock);
	dev->is_opened = 1;
	mutex_unlock(&dev->dev_lock);

	file->private_data = dev;

already_opened:
	DBG("%s: Open called for %s\n", __func__, dev->name);

	return 0;
}

static int rmnet_ctl_release(struct inode *inode, struct file *file)
{
	struct ctrl_pkt_list_elem	*list_elem = NULL;
	struct rmnet_ctrl_dev		*dev;
	unsigned long			flag;
	int				time;

	dev = file->private_data;
	if (!dev)
		return -ENODEV;

	DBG("%s Called on %s device\n", __func__, dev->name);

	spin_lock_irqsave(&dev->rx_lock, flag);
	while (!list_empty(&dev->rx_list)) {
		list_elem = list_first_entry(
				&dev->rx_list,
				struct ctrl_pkt_list_elem,
				list);
		list_del(&list_elem->list);
		kfree(list_elem->cpkt.data);
		kfree(list_elem);
	}
	spin_unlock_irqrestore(&dev->rx_lock, flag);

	mutex_lock(&dev->dev_lock);
	dev->is_opened = 0;
	mutex_unlock(&dev->dev_lock);

	time = usb_wait_anchor_empty_timeout(&dev->tx_submitted,
			UNLINK_TIMEOUT_MS);
	if (!time)
		usb_kill_anchored_urbs(&dev->tx_submitted);

	file->private_data = NULL;

	return 0;
}

static unsigned int rmnet_ctl_poll(struct file *file, poll_table *wait)
{
	unsigned int		mask = 0;
	struct rmnet_ctrl_dev	*dev;

	dev = file->private_data;
	if (!dev)
		return POLLERR;

	poll_wait(file, &dev->read_wait_queue, wait);
	if (!is_dev_connected(dev)) {
		dev_dbg(dev->devicep, "%s: Device not connected\n",
			__func__);
		return POLLERR;
	}

	if (!list_empty(&dev->rx_list))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t rmnet_ctl_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	int				retval = 0;
	int				bytes_to_read;
	struct rmnet_ctrl_dev		*dev;
	struct ctrl_pkt_list_elem	*list_elem = NULL;
	unsigned long			flags;

	dev = file->private_data;
	if (!dev)
		return -ENODEV;

	DBG("%s: Read from %s\n", __func__, dev->name);

ctrl_read:
	if (!is_dev_connected(dev)) {
		dev_dbg(dev->devicep, "%s: Device not connected\n",
			__func__);
		return -ENETRESET;
	}
	spin_lock_irqsave(&dev->rx_lock, flags);
	if (list_empty(&dev->rx_list)) {
		spin_unlock_irqrestore(&dev->rx_lock, flags);

		retval = wait_event_interruptible(dev->read_wait_queue,
					!list_empty(&dev->rx_list) ||
					!is_dev_connected(dev));
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

	if (copy_to_user(buf, list_elem->cpkt.data, bytes_to_read)) {
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
	void			*wbuf;
	struct rmnet_ctrl_dev	*dev = file->private_data;

	if (!dev)
		return -ENODEV;

	if (size <= 0)
		return -EINVAL;

	if (!is_dev_connected(dev))
		return -ENETRESET;

	DBG("%s: Writing %i bytes on %s\n", __func__, size, dev->name);

	wbuf = kmalloc(size , GFP_KERNEL);
	if (!wbuf)
		return -ENOMEM;

	status = copy_from_user(wbuf , buf, size);
	if (status) {
		dev_err(dev->devicep,
		"%s: Unable to copy data from userspace %d\n",
		__func__, status);
		kfree(wbuf);
		return status;
	}
	DUMP_BUFFER("Write: ", size, buf);

	status = rmnet_usb_ctrl_write(dev, wbuf, size);
	if (status == size)
		return size;

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

	retval = usb_autopm_get_interface(dev->intf);
	if (retval < 0) {
		dev_dbg(dev->devicep, "%s: Unable to resume interface: %d\n",
			__func__, retval);
		return retval;
	}

	retval = rmnet_usb_ctrl_write_cmd(dev);

	usb_autopm_put_interface(dev->intf);
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
			 struct rmnet_ctrl_dev **ctrldev)
{
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

	dev->int_pipe = usb_rcvintpipe(udev,
		int_in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);

	mutex_lock(&dev->dev_lock);
	dev->intf = intf;

	/*TBD: for now just update CD status*/
	dev->cbits_tolocal = ACM_CTRL_CD;

	/*send DTR high to modem*/
	dev->cbits_tomdm = ACM_CTRL_DTR;
	mutex_unlock(&dev->dev_lock);

	dev->resp_available = false;
	dev->snd_encap_cmd_cnt = 0;
	dev->get_encap_resp_cnt = 0;
	dev->resp_avail_cnt = 0;
	dev->tx_ctrl_err_cnt = 0;
	dev->set_ctrl_line_state_cnt = 0;

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			USB_CDC_REQ_SET_CONTROL_LINE_STATE,
			(USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE),
			dev->cbits_tomdm,
			dev->intf->cur_altsetting->desc.bInterfaceNumber,
			NULL, 0, USB_CTRL_SET_TIMEOUT);
	if (ret < 0)
		return ret;

	dev->set_ctrl_line_state_cnt++;

	dev->inturb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->inturb) {
		dev_err(dev->devicep, "Error allocating int urb\n");
		return -ENOMEM;
	}

	/*use max pkt size from ep desc*/
	ep = &dev->intf->cur_altsetting->endpoint[0].desc;
	wMaxPacketSize = le16_to_cpu(ep->wMaxPacketSize);

	dev->intbuf = kmalloc(wMaxPacketSize, GFP_KERNEL);
	if (!dev->intbuf) {
		usb_free_urb(dev->inturb);
		dev_err(dev->devicep, "Error allocating int buffer\n");
		return -ENOMEM;
	}

	dev->in_ctlreq->bRequestType =
		(USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE);
	dev->in_ctlreq->bRequest  = USB_CDC_GET_ENCAPSULATED_RESPONSE;
	dev->in_ctlreq->wValue = 0;
	dev->in_ctlreq->wIndex =
		dev->intf->cur_altsetting->desc.bInterfaceNumber;
	dev->in_ctlreq->wLength = cpu_to_le16(DEFAULT_READ_URB_LENGTH);

	interval = max((int)int_in->desc.bInterval,
			(udev->speed == USB_SPEED_HIGH) ? HS_INTERVAL
							: FS_LS_INTERVAL);

	usb_fill_int_urb(dev->inturb, udev,
			 dev->int_pipe,
			 dev->intbuf, wMaxPacketSize,
			 notification_available_cb, dev, interval);

	usb_mark_last_busy(udev);
	ret = rmnet_usb_ctrl_start_rx(dev);
	if (!ret)
		dev->is_connected = true;

	dev->claimed = true;
	*ctrldev = dev;
	return 0;
}

void rmnet_usb_ctrl_disconnect(struct rmnet_ctrl_dev *dev)
{
	dev->claimed = false;

	mutex_lock(&dev->dev_lock);
	/*TBD: for now just update CD status*/
	dev->cbits_tolocal = ~ACM_CTRL_CD;
	dev->cbits_tomdm = ~ACM_CTRL_DTR;
	dev->is_connected = false;
	mutex_unlock(&dev->dev_lock);

	wake_up(&dev->read_wait_queue);

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
	struct rmnet_ctrl_dev	*dev;
	char			*buf;
	int			ret;
	int			i, n;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < num_devs; i++) {
		for (n = 0; n < insts_per_dev; n++) {
			dev = &ctrl_devs[i][n];
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
					"dev opened:               %s\n",
					dev, dev->name,
					dev->snd_encap_cmd_cnt,
					dev->resp_avail_cnt,
					dev->get_encap_resp_cnt,
					dev->set_ctrl_line_state_cnt,
					dev->tx_ctrl_err_cnt,
					dev->cbits_tolocal,
					dev->cbits_tomdm,
					dev->mdm_wait_timeout,
					dev->zlp_cnt,
					dev->get_encap_failure_cnt,
					dev->is_opened ? "OPEN" : "CLOSE");
		}
	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);
	kfree(buf);
	return ret;
}

static ssize_t rmnet_usb_ctrl_reset_stats(struct file *file, const char __user *
		buf, size_t count, loff_t *ppos)
{
	struct rmnet_ctrl_dev	*dev;
	int			i, n;

	for (i = 0; i < num_devs; i++) {
		for (n = 0; n < insts_per_dev; n++) {
			dev = &ctrl_devs[i][n];

			dev->snd_encap_cmd_cnt = 0;
			dev->resp_avail_cnt = 0;
			dev->get_encap_resp_cnt = 0;
			dev->set_ctrl_line_state_cnt = 0;
			dev->tx_ctrl_err_cnt = 0;
			dev->zlp_cnt = 0;
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

int rmnet_usb_ctrl_init(int no_rmnet_devs, int no_rmnet_insts_per_dev)
{
	struct rmnet_ctrl_dev	*dev;
	int			i, n;
	int			status;

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

			dev->wq = create_singlethread_workqueue(dev->name);
			if (!dev->wq) {
				pr_err("unable to allocate workqueue");
				kfree(dev);
				return -ENOMEM;
			}

			mutex_init(&dev->dev_lock);
			spin_lock_init(&dev->rx_lock);
			init_waitqueue_head(&dev->read_wait_queue);
			init_waitqueue_head(&dev->open_wait_queue);
			INIT_LIST_HEAD(&dev->rx_list);
			init_usb_anchor(&dev->tx_submitted);
			init_usb_anchor(&dev->rx_submitted);
			INIT_WORK(&dev->get_encap_work, get_encap_work);

			cdev_init(&dev->cdev, &ctrldev_fops);
			dev->cdev.owner = THIS_MODULE;

			status = cdev_add(&dev->cdev, (ctrldev_num[i] + n), 1);
			if (status) {
				pr_err("%s: cdev_add() ret %i\n", __func__,
					status);
				destroy_workqueue(dev->wq);
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
				destroy_workqueue(dev->wq);
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
				destroy_workqueue(dev->wq);
				kfree(dev);
				return status;
			}
			dev_set_drvdata(dev->devicep, dev);

			status = rmnet_usb_ctrl_alloc_rx(dev);
			if (status) {
				device_remove_file(dev->devicep,
						   &dev_attr_modem_wait);
				device_destroy(dev->devicep->class,
					       dev->devicep->devt);
				cdev_del(&dev->cdev);
				destroy_workqueue(dev->wq);
				kfree(dev);
				return status;
			}
		}
	}

	rmnet_usb_ctrl_debugfs_init();
	pr_info("rmnet usb ctrl Initialized.\n");
	return 0;
}

static void free_rmnet_ctrl_dev(struct rmnet_ctrl_dev *dev)
{
	kfree(dev->in_ctlreq);
	kfree(dev->rcvbuf);
	kfree(dev->intbuf);
	usb_free_urb(dev->rcvurb);
	usb_free_urb(dev->inturb);
	device_remove_file(dev->devicep, &dev_attr_modem_wait);
	cdev_del(&dev->cdev);
	destroy_workqueue(dev->wq);
	device_destroy(dev->devicep->class,
		       dev->devicep->devt);
}

void rmnet_usb_ctrl_exit(int no_rmnet_devs, int no_rmnet_insts_per_dev)
{
	int i, n;

	for (i = 0; i < no_rmnet_devs; i++) {
		for (n = 0; n < no_rmnet_insts_per_dev; n++)
			free_rmnet_ctrl_dev(&ctrl_devs[i][n]);

		kfree(ctrl_devs[i]);

		class_destroy(ctrldev_classp[i]);
		if (ctrldev_num[i])
			unregister_chrdev_region(ctrldev_num[i], insts_per_dev);
	}

	kfree(ctrl_devs);
	rmnet_usb_ctrl_debugfs_exit();
}

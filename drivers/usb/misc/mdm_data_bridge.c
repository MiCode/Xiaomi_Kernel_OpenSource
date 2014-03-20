/* Copyright (c) 2011-2014, Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/ratelimit.h>
#include <linux/usb/usb_bridge.h>

#define MAX_RX_URBS			100
#define RMNET_RX_BUFSIZE		2048

#define STOP_SUBMIT_URB_LIMIT		500
#define FLOW_CTRL_EN_THRESHOLD		500
#define FLOW_CTRL_DISABLE		300
#define FLOW_CTRL_SUPPORT		1

#define BRIDGE_DATA_IDX		0
#define BRIDGE_CTRL_IDX		1

/*for xport : HSIC*/
static const char * const serial_hsic_bridge_names[] = {
	"serial_hsic_data",
	"serial_hsic_ctrl",
};

static const char * const rmnet_hsic_bridge_names[] = {
	"rmnet_hsic_data",
	"rmnet_hsic_ctrl",
};

static const char * const qdss_hsic_bridge_names[] = {
	"qdss_hsic_data",
};

/*for xport : HSUSB*/
static const char * const serial_hsusb_bridge_names[] = {
	"serial_hsusb_data",
	"serial_hsusb_ctrl",
};

static const char * const rmnet_hsusb_bridge_names[] = {
	"rmnet_hsusb_data",
	"rmnet_hsusb_ctrl",
};

/* since driver supports multiple instances, on smp systems
 * probe might get called from multiple cores, hence use lock
 * to identify unclaimed bridge device instance
 */
static DEFINE_MUTEX(brdg_claim_lock);

static struct workqueue_struct	*bridge_wq;

static unsigned int	fctrl_support = FLOW_CTRL_SUPPORT;
module_param(fctrl_support, uint, S_IRUGO | S_IWUSR);

static unsigned int	fctrl_en_thld = FLOW_CTRL_EN_THRESHOLD;
module_param(fctrl_en_thld, uint, S_IRUGO | S_IWUSR);

static unsigned int	fctrl_dis_thld = FLOW_CTRL_DISABLE;
module_param(fctrl_dis_thld, uint, S_IRUGO | S_IWUSR);

unsigned int	max_rx_urbs = MAX_RX_URBS;
module_param(max_rx_urbs, uint, S_IRUGO | S_IWUSR);

unsigned int	stop_submit_urb_limit = STOP_SUBMIT_URB_LIMIT;
module_param(stop_submit_urb_limit, uint, S_IRUGO | S_IWUSR);

static unsigned tx_urb_mult = 20;
module_param(tx_urb_mult, uint, S_IRUGO|S_IWUSR);

static unsigned int rx_rmnet_buffer_size = RMNET_RX_BUFSIZE;
module_param(rx_rmnet_buffer_size, uint, S_IRUGO | S_IWUSR);

#define TX_HALT   0
#define RX_HALT   1
#define SUSPENDED 2
#define CLAIMED   3

struct data_bridge {
	struct usb_interface		*intf;
	struct usb_device		*udev;
	int				id;
	char				*name;

	unsigned int			bulk_in;
	unsigned int			bulk_out;
	int				err;

	/* Support INT IN instead of BULK IN */
	bool				use_int_in_pipe;
	unsigned int			period;

	/* keep track of in-flight URBs */
	struct usb_anchor		tx_active;
	struct usb_anchor		rx_active;

	struct list_head		rx_idle;
	struct sk_buff_head		rx_done;

	struct workqueue_struct		*wq;
	struct work_struct		process_rx_w;

	struct bridge			*brdg;

	/* work queue function for handling halt conditions */
	struct work_struct		kevent;

	unsigned long			flags;

	struct platform_device		*pdev;

	/* counters */
	atomic_t			pending_txurbs;
	unsigned int			txurb_drp_cnt;
	unsigned long			to_host;
	unsigned long			to_modem;
	unsigned int			tx_throttled_cnt;
	unsigned int			tx_unthrottled_cnt;
	unsigned int			rx_throttled_cnt;
	unsigned int			rx_unthrottled_cnt;
	unsigned long long		tx_num_of_bytes;
	unsigned long long		rx_num_of_bytes;
};

static struct data_bridge	*__dev[MAX_BRIDGE_DEVICES];

static unsigned int get_timestamp(void);
static void dbg_timestamp(char *, struct sk_buff *);
static int submit_rx_urb(struct data_bridge *dev, struct urb *urb,
		gfp_t flags);

/* Find an unclaimed bridge device instance */
static int get_bridge_dev_idx(void)
{
	struct data_bridge	*dev;
	int			i;

	mutex_lock(&brdg_claim_lock);
	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		dev = __dev[i];
		if (!test_bit(CLAIMED, &dev->flags)) {
			set_bit(CLAIMED, &dev->flags);
			mutex_unlock(&brdg_claim_lock);
			return i;
		}
	}
	mutex_unlock(&brdg_claim_lock);

	return -ENODEV;
}

static int get_data_bridge_chid(char *xport_name)
{
	struct data_bridge	*dev;
	int			i;

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		dev = __dev[i];
		if (!strncmp(dev->name, xport_name, BRIDGE_NAME_MAX_LEN))
			return i;
	}

	return -ENODEV;
}

static inline  bool rx_halted(struct data_bridge *dev)
{
	return test_bit(RX_HALT, &dev->flags);
}

static inline bool rx_throttled(struct bridge *brdg)
{
	return test_bit(RX_THROTTLED, &brdg->flags);
}

static void free_rx_urbs(struct data_bridge *dev)
{
	struct list_head	*head;
	struct urb		*rx_urb;
	unsigned long		flags;

	head = &dev->rx_idle;
	spin_lock_irqsave(&dev->rx_done.lock, flags);
	while (!list_empty(head)) {
		rx_urb = list_entry(head->next, struct urb, urb_list);
		list_del(&rx_urb->urb_list);
		usb_free_urb(rx_urb);
	}
	spin_unlock_irqrestore(&dev->rx_done.lock, flags);
}

int data_bridge_unthrottle_rx(unsigned int id)
{
	struct data_bridge	*dev;

	if (id >= MAX_BRIDGE_DEVICES)
		return -EINVAL;

	dev = __dev[id];
	if (!dev || !dev->brdg)
		return -ENODEV;

	dev->rx_unthrottled_cnt++;
	queue_work(dev->wq, &dev->process_rx_w);

	return 0;
}
EXPORT_SYMBOL(data_bridge_unthrottle_rx);

static void data_bridge_process_rx(struct work_struct *work)
{
	int			retval;
	unsigned long		flags;
	struct urb		*rx_idle;
	struct sk_buff		*skb;
	struct timestamp_info	*info;
	struct data_bridge	*dev =
		container_of(work, struct data_bridge, process_rx_w);

	struct bridge		*brdg = dev->brdg;

	if (!brdg || !brdg->ops.send_pkt || rx_halted(dev))
		return;

	while (!rx_throttled(brdg) && (skb = skb_dequeue(&dev->rx_done))) {
		dev->to_host++;
		dev->rx_num_of_bytes += skb->len;
		info = (struct timestamp_info *)skb->cb;
		info->rx_done_sent = get_timestamp();
		/* hand off sk_buff to client,they'll need to free it */
		retval = brdg->ops.send_pkt(brdg->ctx, skb, skb->len);
		if (retval == -ENOTCONN || retval == -EINVAL) {
			return;
		} else if (retval == -EBUSY) {
			dev->rx_throttled_cnt++;
			break;
		}
	}

	spin_lock_irqsave(&dev->rx_done.lock, flags);
	while (!list_empty(&dev->rx_idle)) {
		if (dev->rx_done.qlen > stop_submit_urb_limit)
			break;

		rx_idle = list_first_entry(&dev->rx_idle, struct urb, urb_list);
		list_del(&rx_idle->urb_list);
		spin_unlock_irqrestore(&dev->rx_done.lock, flags);
		retval = submit_rx_urb(dev, rx_idle, GFP_KERNEL);
		spin_lock_irqsave(&dev->rx_done.lock, flags);
		if (retval) {
			list_add_tail(&rx_idle->urb_list, &dev->rx_idle);
			break;
		}
	}
	spin_unlock_irqrestore(&dev->rx_done.lock, flags);
}

static void data_bridge_read_cb(struct urb *urb)
{
	struct bridge		*brdg;
	struct sk_buff		*skb = urb->context;
	struct timestamp_info	*info = (struct timestamp_info *)skb->cb;
	struct data_bridge	*dev = info->dev;
	bool			queue = 0;

	/*usb device disconnect*/
	if (urb->dev->state == USB_STATE_NOTATTACHED)
		urb->status = -ECONNRESET;

	brdg = dev->brdg;
	skb_put(skb, urb->actual_length);

	switch (urb->status) {
	case 0: /* success */
		queue = 1;
		info->rx_done = get_timestamp();
		spin_lock(&dev->rx_done.lock);
		__skb_queue_tail(&dev->rx_done, skb);
		spin_unlock(&dev->rx_done.lock);
		break;

	/*do not resubmit*/
	case -EPIPE:
		set_bit(RX_HALT, &dev->flags);
		dev_err(&dev->intf->dev, "%s: epout halted\n", __func__);
		schedule_work(&dev->kevent);
		/* FALLTHROUGH */
	case -ESHUTDOWN:
	case -ENOENT: /* suspended */
	case -ECONNRESET: /* unplug */
	case -EPROTO:
		dev_kfree_skb_any(skb);
		break;

	/*resubmit */
	case -EOVERFLOW: /*babble error*/
	default:
		queue = 1;
		dev_kfree_skb_any(skb);
		pr_debug_ratelimited("%s: non zero urb status = %d\n",
			__func__, urb->status);
		break;
	}

	spin_lock(&dev->rx_done.lock);
	list_add_tail(&urb->urb_list, &dev->rx_idle);
	spin_unlock(&dev->rx_done.lock);

	if (queue)
		queue_work(dev->wq, &dev->process_rx_w);
}

static int submit_rx_urb(struct data_bridge *dev, struct urb *rx_urb,
	gfp_t flags)
{
	struct sk_buff		*skb;
	struct timestamp_info	*info;
	int			retval = -EINVAL;
	unsigned int		created;

	created = get_timestamp();
	skb = alloc_skb(rx_rmnet_buffer_size, flags);
	if (!skb)
		return -ENOMEM;

	info = (struct timestamp_info *)skb->cb;
	info->dev = dev;
	info->created = created;

	if (dev->use_int_in_pipe)
		usb_fill_int_urb(rx_urb, dev->udev, dev->bulk_in,
				skb->data, rx_rmnet_buffer_size,
				data_bridge_read_cb, skb, dev->period);
	else
		usb_fill_bulk_urb(rx_urb, dev->udev, dev->bulk_in,
				skb->data, rx_rmnet_buffer_size,
				data_bridge_read_cb, skb);

	if (test_bit(SUSPENDED, &dev->flags))
		goto suspended;

	usb_anchor_urb(rx_urb, &dev->rx_active);
	info->rx_queued = get_timestamp();
	retval = usb_submit_urb(rx_urb, flags);
	if (retval)
		goto fail;

	usb_mark_last_busy(dev->udev);
	return 0;
fail:
	usb_unanchor_urb(rx_urb);
suspended:
	dev_kfree_skb_any(skb);

	return retval;
}

static int data_bridge_prepare_rx(struct data_bridge *dev)
{
	int		i;
	struct urb	*rx_urb;
	int		retval = 0;

	for (i = 0; i < max_rx_urbs; i++) {
		rx_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!rx_urb) {
			retval = -ENOMEM;
			goto free_urbs;
		}

		list_add_tail(&rx_urb->urb_list, &dev->rx_idle);
	}

	return 0;

free_urbs:
	 free_rx_urbs(dev);
	 return retval;
}

int data_bridge_open(struct bridge *brdg)
{
	struct data_bridge	*dev;
	int			ch_id;

	if (!brdg) {
		pr_err("bridge is null\n");
		return -EINVAL;
	}

	ch_id = get_data_bridge_chid(brdg->name);
	if (ch_id < 0 || ch_id >= MAX_BRIDGE_DEVICES) {
		pr_err("%s: %s dev not found\n", __func__, brdg->name);
		return ch_id;
	}

	brdg->ch_id = ch_id;

	dev = __dev[ch_id];

	dev_dbg(&dev->intf->dev, "%s: dev:%p\n", __func__, dev);

	dev->brdg = brdg;
	dev->err = 0;
	atomic_set(&dev->pending_txurbs, 0);
	dev->to_host = 0;
	dev->to_modem = 0;
	dev->txurb_drp_cnt = 0;
	dev->tx_throttled_cnt = 0;
	dev->tx_unthrottled_cnt = 0;
	dev->rx_throttled_cnt = 0;
	dev->rx_unthrottled_cnt = 0;
	dev->tx_num_of_bytes = 0;
	dev->rx_num_of_bytes = 0;

	queue_work(dev->wq, &dev->process_rx_w);

	return 0;
}
EXPORT_SYMBOL(data_bridge_open);

void data_bridge_close(unsigned int id)
{
	struct data_bridge	*dev;
	struct sk_buff		*skb;
	unsigned long		flags;

	if (id >= MAX_BRIDGE_DEVICES)
		return;

	dev  = __dev[id];
	if (!dev || !dev->brdg)
		return;

	dev_dbg(&dev->intf->dev, "%s:\n", __func__);

	cancel_work_sync(&dev->kevent);
	cancel_work_sync(&dev->process_rx_w);

	usb_kill_anchored_urbs(&dev->tx_active);
	usb_kill_anchored_urbs(&dev->rx_active);

	spin_lock_irqsave(&dev->rx_done.lock, flags);
	while ((skb = __skb_dequeue(&dev->rx_done)))
		dev_kfree_skb_any(skb);
	spin_unlock_irqrestore(&dev->rx_done.lock, flags);

	dev->brdg = NULL;
}
EXPORT_SYMBOL(data_bridge_close);

static void defer_kevent(struct work_struct *work)
{
	int			status;
	struct data_bridge	*dev =
		container_of(work, struct data_bridge, kevent);

	if (!dev)
		return;

	if (test_bit(TX_HALT, &dev->flags)) {
		usb_unlink_anchored_urbs(&dev->tx_active);

		status = usb_autopm_get_interface(dev->intf);
		if (status < 0) {
			dev_dbg(&dev->intf->dev,
				"can't acquire interface, status %d\n", status);
			return;
		}

		status = usb_clear_halt(dev->udev, dev->bulk_out);
		usb_autopm_put_interface(dev->intf);
		if (status < 0 && status != -EPIPE && status != -ESHUTDOWN)
			dev_err(&dev->intf->dev,
				"can't clear tx halt, status %d\n", status);
		else
			clear_bit(TX_HALT, &dev->flags);
	}

	if (test_bit(RX_HALT, &dev->flags)) {
		usb_unlink_anchored_urbs(&dev->rx_active);

		status = usb_autopm_get_interface(dev->intf);
		if (status < 0) {
			dev_dbg(&dev->intf->dev,
				"can't acquire interface, status %d\n", status);
			return;
		}

		status = usb_clear_halt(dev->udev, dev->bulk_in);
		usb_autopm_put_interface(dev->intf);
		if (status < 0 && status != -EPIPE && status != -ESHUTDOWN)
			dev_err(&dev->intf->dev,
				"can't clear rx halt, status %d\n", status);
		else {
			clear_bit(RX_HALT, &dev->flags);
			if (dev->brdg)
				queue_work(dev->wq, &dev->process_rx_w);
		}
	}
}

static void data_bridge_write_cb(struct urb *urb)
{
	struct sk_buff		*skb = urb->context;
	struct timestamp_info	*info = (struct timestamp_info *)skb->cb;
	struct data_bridge	*dev = info->dev;
	struct bridge		*brdg = dev->brdg;
	int			pending;

	pr_debug("%s: dev:%p\n", __func__, dev);

	switch (urb->status) {
	case 0: /*success*/
		dev->to_modem++;
		dev->tx_num_of_bytes += skb->len;
		dbg_timestamp("UL", skb);
		break;
	case -EPROTO:
		dev->err = -EPROTO;
		break;
	case -EPIPE:
		set_bit(TX_HALT, &dev->flags);
		dev_err(&dev->intf->dev, "%s: epout halted\n", __func__);
		schedule_work(&dev->kevent);
		/* FALLTHROUGH */
	case -ESHUTDOWN:
	case -ENOENT: /* suspended */
	case -ECONNRESET: /* unplug */
	case -EOVERFLOW: /*babble error*/
		/* FALLTHROUGH */
	default:
		pr_debug_ratelimited("%s: non zero urb status = %d\n",
					__func__, urb->status);
	}

	usb_free_urb(urb);
	dev_kfree_skb_any(skb);

	pending = atomic_dec_return(&dev->pending_txurbs);

	/*flow ctrl*/
	if (brdg && fctrl_support && pending <= fctrl_dis_thld &&
		test_and_clear_bit(TX_THROTTLED, &brdg->flags)) {
		pr_debug_ratelimited("%s: disable flow ctrl: pend urbs:%u\n",
			__func__, pending);
		dev->tx_unthrottled_cnt++;
		if (brdg->ops.unthrottle_tx)
			brdg->ops.unthrottle_tx(brdg->ctx);
	}

	/* if we are here after device disconnect
	 * usb_unbind_interface() takes care of
	 * residual pm_autopm_get_interface_* calls
	 */
	if (urb->dev->state != USB_STATE_NOTATTACHED)
		usb_autopm_put_interface_async(dev->intf);
}

int data_bridge_write(unsigned int id, struct sk_buff *skb)
{
	int			result;
	int			size = skb->len;
	int			pending;
	struct urb		*txurb;
	struct timestamp_info	*info = (struct timestamp_info *)skb->cb;
	struct data_bridge	*dev = __dev[id];
	struct bridge		*brdg;

	if (!dev || !dev->brdg || dev->err || !usb_get_intfdata(dev->intf))
		return -ENODEV;

	brdg = dev->brdg;
	if (!brdg)
		return -ENODEV;

	dev_dbg(&dev->intf->dev, "%s: write (%d bytes)\n", __func__, skb->len);

	result = usb_autopm_get_interface(dev->intf);
	if (result < 0) {
		dev_dbg(&dev->intf->dev, "%s: resume failure\n", __func__);
		goto pm_error;
	}

	txurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!txurb) {
		dev_err(&dev->intf->dev, "%s: error allocating read urb\n",
			__func__);
		result = -ENOMEM;
		goto error;
	}

	/* store dev pointer in skb */
	info->dev = dev;
	info->tx_queued = get_timestamp();

	usb_fill_bulk_urb(txurb, dev->udev, dev->bulk_out,
			skb->data, skb->len, data_bridge_write_cb, skb);

	txurb->transfer_flags |= URB_ZERO_PACKET;

	pending = atomic_inc_return(&dev->pending_txurbs);
	usb_anchor_urb(txurb, &dev->tx_active);

	if (atomic_read(&dev->pending_txurbs) % tx_urb_mult)
		txurb->transfer_flags |= URB_NO_INTERRUPT;

	result = usb_submit_urb(txurb, GFP_KERNEL);
	if (result < 0) {
		usb_unanchor_urb(txurb);
		atomic_dec(&dev->pending_txurbs);
		dev_err(&dev->intf->dev, "%s: submit URB error %d\n",
			__func__, result);
		goto free_urb;
	}

	dev_dbg(&dev->intf->dev, "%s: pending_txurbs: %u\n", __func__, pending);

	/* flow control: last urb submitted but return -EBUSY */
	if (fctrl_support && pending > fctrl_en_thld) {
		set_bit(TX_THROTTLED, &brdg->flags);
		dev->tx_throttled_cnt++;
		pr_debug_ratelimited("%s: enable flow ctrl pend txurbs:%u\n",
					__func__, pending);
		return -EBUSY;
	}

	return size;

free_urb:
	usb_free_urb(txurb);
error:
	dev->txurb_drp_cnt++;
	usb_autopm_put_interface(dev->intf);
pm_error:
	return result;
}
EXPORT_SYMBOL(data_bridge_write);

static int bridge_resume(struct usb_interface *iface)
{
	int			retval = 0;
	struct data_bridge	*dev = usb_get_intfdata(iface);

	clear_bit(SUSPENDED, &dev->flags);

	if (dev->brdg)
		queue_work(dev->wq, &dev->process_rx_w);

	retval = ctrl_bridge_resume(dev->id);

	return retval;
}

static int bridge_suspend(struct usb_interface *intf, pm_message_t message)
{
	int			retval;
	struct data_bridge	*dev = usb_get_intfdata(intf);

	if (atomic_read(&dev->pending_txurbs))
		return -EBUSY;

	retval = ctrl_bridge_suspend(dev->id);
	if (retval)
		return retval;

	set_bit(SUSPENDED, &dev->flags);
	usb_kill_anchored_urbs(&dev->rx_active);

	return 0;
}

static int data_bridge_probe(struct usb_interface *iface,
		struct usb_host_endpoint *bulk_in,
		struct usb_host_endpoint *bulk_out, char *name, int id)
{
	struct data_bridge	*dev;
	int			retval;

	dev = __dev[id];
	if (!dev) {
		pr_err("%s: device not found\n", __func__);
		return -ENODEV;
	}

	dev->pdev = platform_device_alloc(name, -1);
	if (!dev->pdev) {
		pr_err("%s: unable to allocate platform device\n", __func__);
		kfree(dev);
		return -ENOMEM;
	}

	/*clear all bits except claimed bit*/
	clear_bit(RX_HALT, &dev->flags);
	clear_bit(TX_HALT, &dev->flags);
	clear_bit(SUSPENDED, &dev->flags);

	dev->id = id;
	dev->name = name;
	dev->udev = interface_to_usbdev(iface);
	dev->intf = iface;

	if (dev->use_int_in_pipe)
		dev->bulk_in = usb_rcvintpipe(dev->udev,
			bulk_in->desc.bEndpointAddress &
			USB_ENDPOINT_NUMBER_MASK);
	else
		dev->bulk_in = usb_rcvbulkpipe(dev->udev,
			bulk_in->desc.bEndpointAddress &
			USB_ENDPOINT_NUMBER_MASK);

	if (bulk_out)
		dev->bulk_out = usb_sndbulkpipe(dev->udev,
		bulk_out->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);

	usb_set_intfdata(iface, dev);

	/*allocate list of rx urbs*/
	retval = data_bridge_prepare_rx(dev);
	if (retval) {
		platform_device_put(dev->pdev);
		return retval;
	}

	platform_device_add(dev->pdev);

	return 0;
}

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_BUF_SIZE	4096

static unsigned int	record_timestamp;
module_param(record_timestamp, uint, S_IRUGO | S_IWUSR);

static struct timestamp_buf dbg_data = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck)
};

/*get_timestamp - returns time of day in us */
static unsigned int get_timestamp(void)
{
	struct timeval	tval;
	unsigned int	stamp;

	if (!record_timestamp)
		return 0;

	do_gettimeofday(&tval);
	/* 2^32 = 4294967296. Limit to 4096s. */
	stamp = tval.tv_sec & 0xFFF;
	stamp = stamp * 1000000 + tval.tv_usec;
	return stamp;
}

static void dbg_inc(unsigned *idx)
{
	*idx = (*idx + 1) & (DBG_DATA_MAX-1);
}

/**
* dbg_timestamp - Stores timestamp values of a SKB life cycle
*	to debug buffer
* @event: "UL": Uplink Data
* @skb: SKB used to store timestamp values to debug buffer
*/
static void dbg_timestamp(char *event, struct sk_buff * skb)
{
	unsigned long		flags;
	struct timestamp_info	*info = (struct timestamp_info *)skb->cb;

	if (!record_timestamp)
		return;

	write_lock_irqsave(&dbg_data.lck, flags);

	scnprintf(dbg_data.buf[dbg_data.idx], DBG_DATA_MSG,
		  "%p %u[%s] %u %u %u %u %u %u\n",
		  skb, skb->len, event, info->created, info->rx_queued,
		  info->rx_done, info->rx_done_sent, info->tx_queued,
		  get_timestamp());

	dbg_inc(&dbg_data.idx);

	write_unlock_irqrestore(&dbg_data.lck, flags);
}

/* show_timestamp: displays the timestamp buffer */
static ssize_t show_timestamp(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	unsigned long	flags;
	unsigned	i;
	unsigned	j = 0;
	char		*buf;
	int		ret = 0;

	if (!record_timestamp)
		return 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	read_lock_irqsave(&dbg_data.lck, flags);

	i = dbg_data.idx;
	for (dbg_inc(&i); i != dbg_data.idx; dbg_inc(&i)) {
		if (!strnlen(dbg_data.buf[i], DBG_DATA_MSG))
			continue;
		j += scnprintf(buf + j, DEBUG_BUF_SIZE - j,
			       "%s\n", dbg_data.buf[i]);
	}

	read_unlock_irqrestore(&dbg_data.lck, flags);

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, j);

	kfree(buf);

	return ret;
}

const struct file_operations data_timestamp_ops = {
	.read = show_timestamp,
};

static ssize_t data_bridge_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct data_bridge	*dev;
	char			*buf;
	int			ret;
	int			i;
	int			temp = 0;

	buf = kzalloc(sizeof(char) * DEBUG_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		dev = __dev[i];
		if (!dev)
			continue;

		temp += scnprintf(buf + temp, DEBUG_BUF_SIZE - temp,
				"\nName#%s dev %p\n"
				"pending tx urbs:    %u\n"
				"tx urb drp cnt:     %u\n"
				"to host:            %lu\n"
				"to mdm:             %lu\n"
				"rx number of bytes: %llu\n"
				"tx number of bytes: %llu\n"
				"tx throttled cnt:   %u\n"
				"tx unthrottled cnt: %u\n"
				"rx throttled cnt:   %u\n"
				"rx unthrottled cnt: %u\n"
				"rx done skb qlen:   %u\n"
				"dev err:            %d\n"
				"suspended:          %d\n"
				"TX_HALT:            %d\n"
				"RX_HALT:            %d\n",
				dev->name, dev,
				atomic_read(&dev->pending_txurbs),
				dev->txurb_drp_cnt,
				dev->to_host,
				dev->to_modem,
				dev->rx_num_of_bytes,
				dev->tx_num_of_bytes,
				dev->tx_throttled_cnt,
				dev->tx_unthrottled_cnt,
				dev->rx_throttled_cnt,
				dev->rx_unthrottled_cnt,
				dev->rx_done.qlen,
				dev->err,
				test_bit(SUSPENDED, &dev->flags),
				test_bit(TX_HALT, &dev->flags),
				test_bit(RX_HALT, &dev->flags));

	}

	ret = simple_read_from_buffer(ubuf, count, ppos, buf, temp);

	kfree(buf);

	return ret;
}

static ssize_t data_bridge_reset_stats(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	struct data_bridge	*dev;
	int			i;

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		dev = __dev[i];
		if (!dev)
			continue;

		dev->to_host = 0;
		dev->to_modem = 0;
		dev->txurb_drp_cnt = 0;
		dev->tx_throttled_cnt = 0;
		dev->tx_unthrottled_cnt = 0;
		dev->rx_throttled_cnt = 0;
		dev->rx_unthrottled_cnt = 0;
		dev->tx_num_of_bytes = 0;
		dev->rx_num_of_bytes = 0;
	}
	return count;
}

const struct file_operations data_stats_ops = {
	.read = data_bridge_read_stats,
	.write = data_bridge_reset_stats,
};

static struct dentry	*data_dent;
static struct dentry	*data_dfile_stats;
static struct dentry	*data_dfile_tstamp;

static void data_bridge_debugfs_init(void)
{
	data_dent = debugfs_create_dir("data_hsic_bridge", 0);
	if (IS_ERR(data_dent))
		return;

	data_dfile_stats = debugfs_create_file("status", 0644, data_dent, 0,
				&data_stats_ops);
	if (!data_dfile_stats || IS_ERR(data_dfile_stats)) {
		debugfs_remove(data_dent);
		return;
	}

	data_dfile_tstamp = debugfs_create_file("timestamp", 0644, data_dent,
				0, &data_timestamp_ops);
	if (!data_dfile_tstamp || IS_ERR(data_dfile_tstamp))
		debugfs_remove(data_dent);
}

static void data_bridge_debugfs_exit(void)
{
	debugfs_remove(data_dfile_stats);
	debugfs_remove(data_dfile_tstamp);
	debugfs_remove(data_dent);
}

#else
static void data_bridge_debugfs_init(void) { }
static void data_bridge_debugfs_exit(void) { }
static void dbg_timestamp(char *event, struct sk_buff * skb)
{
	return;
}

static unsigned int get_timestamp(void)
{
	return 0;
}

#endif

static int 
bridge_probe(struct usb_interface *iface, const struct usb_device_id *id)
{
	struct usb_host_endpoint	*endpoint = NULL;
	struct usb_host_endpoint	*bulk_in = NULL;
	struct usb_host_endpoint	*bulk_out = NULL;
	struct usb_host_endpoint	*int_in = NULL;
	struct usb_host_endpoint	*data_int_in = NULL;
	struct usb_device		*udev;
	int				i;
	int				status = 0;
	int				numends;
	int				ch_id;
	char				**bname = (char **)id->driver_info;

	if (iface->num_altsetting != 1) {
		pr_err("%s invalid num_altsetting %u\n",
				__func__, iface->num_altsetting);
		return -EINVAL;
	}

	udev = interface_to_usbdev(iface);
	usb_get_dev(udev);

	numends = iface->cur_altsetting->desc.bNumEndpoints;
	for (i = 0; i < numends; i++) {
		endpoint = iface->cur_altsetting->endpoint + i;
		if (!endpoint) {
			dev_err(&iface->dev, "%s: invalid endpoint %u\n",
					__func__, i);
			status = -EINVAL;
			goto out;
		}

		if (usb_endpoint_is_bulk_in(&endpoint->desc))
			bulk_in = endpoint;
		else if (usb_endpoint_is_bulk_out(&endpoint->desc))
			bulk_out = endpoint;
		else if (usb_endpoint_is_int_in(&endpoint->desc)) {
			if (int_in != 0)
				data_int_in = endpoint;
			else
				int_in = endpoint;
		}
	}
	if (((numends == 3)
	&& ((!bulk_in && !data_int_in) || !bulk_out || !int_in))
	|| ((numends == 1) && !bulk_in)) {
		dev_err(&iface->dev, "%s: invalid endpoints\n", __func__);
		status = -EINVAL;
		goto out;
	}

	ch_id = get_bridge_dev_idx();
	if (ch_id < 0) {
		pr_err("%s all bridge channels claimed. Probe failed\n",
				__func__);
		return -ENODEV;
	}
	if (data_int_in) {
		__dev[ch_id]->use_int_in_pipe = true;
		__dev[ch_id]->period = data_int_in->desc.bInterval;
		status = data_bridge_probe(iface, data_int_in, bulk_out,
				bname[BRIDGE_DATA_IDX], ch_id);
	} else {
		status = data_bridge_probe(iface, bulk_in, bulk_out,
			bname[BRIDGE_DATA_IDX], ch_id);
	}
	if (status < 0) {
		dev_err(&iface->dev, "data_bridge_probe failed %d\n", status);
		goto out;
	}

	status = ctrl_bridge_probe(iface,
				int_in,
				bname[BRIDGE_CTRL_IDX],
				ch_id);
	if (status < 0) {
		dev_err(&iface->dev, "ctrl_bridge_probe failed %d\n",
			status);
		goto error;
	}
	return 0;

error:
	platform_device_unregister(__dev[ch_id]->pdev);
	free_rx_urbs(__dev[ch_id]);
	usb_set_intfdata(iface, NULL);
out:
	usb_put_dev(udev);

	return status;
}

static void bridge_disconnect(struct usb_interface *intf)
{
	struct data_bridge	*dev = usb_get_intfdata(intf);

	if (!dev) {
		pr_err("%s: data device not found\n", __func__);
		return;
	}

	/*set device name to none to get correct channel id
	 * at the time of bridge open
	 */
	dev->name = "none";

	ctrl_bridge_disconnect(dev->id);
	platform_device_unregister(dev->pdev);
	usb_set_intfdata(intf, NULL);

	free_rx_urbs(dev);

	usb_put_dev(dev->udev);

	clear_bit(CLAIMED, &dev->flags);
}

/*driver info stores data/ctrl bridge name used to match bridge xport name*/
static const struct usb_device_id bridge_ids[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9001, 2),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9001, 3),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9034, 2),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9034, 3),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9048, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9048, 4),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x904c, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x904c, 5),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9075, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9075, 5),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9079, 3),
	.driver_info = (unsigned long)serial_hsusb_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9079, 4),
	.driver_info = (unsigned long)rmnet_hsusb_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908A, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908A, 5),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	/* this PID supports QDSS-MDM trace*/
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908E, 4),
	.driver_info = (unsigned long)qdss_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908E, 5),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908E, 7),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909C, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909D, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909E, 5),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	/* this PID supports QDSS-MDM trace*/
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909E, 4),
	.driver_info = (unsigned long)qdss_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909F, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909F, 5),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A0, 3),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A0, 5),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},
	/* this PID supports QDSS-MDM trace*/
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A4, 4),
	.driver_info = (unsigned long)qdss_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A4, 5),
	.driver_info = (unsigned long)serial_hsic_bridge_names,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A4, 7),
	.driver_info = (unsigned long)rmnet_hsic_bridge_names,
	},


	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, bridge_ids);

static struct usb_driver bridge_driver = {
	.name =			"mdm_bridge",
	.probe =		bridge_probe,
	.disconnect =		bridge_disconnect,
	.id_table =		bridge_ids,
	.suspend =		bridge_suspend,
	.resume =		bridge_resume,
	.reset_resume =		bridge_resume,
	.supports_autosuspend =	1,
};

static int __init bridge_init(void)
{
	struct data_bridge	*dev;
	int			ret;
	int			i = 0;

	ret = ctrl_bridge_init();
	if (ret)
		return ret;

	bridge_wq  = create_singlethread_workqueue("mdm_bridge");
	if (!bridge_wq) {
		pr_err("%s: Unable to create workqueue:bridge\n", __func__);
		ret = -ENOMEM;
		goto free_ctrl;
	}

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {

		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (!dev) {
			pr_err("%s: unable to allocate dev\n", __func__);
			ret = -ENOMEM;
			goto error;
		}

		dev->wq = bridge_wq;

		/*transport name will be set during probe*/
		dev->name = "none";

		init_usb_anchor(&dev->tx_active);
		init_usb_anchor(&dev->rx_active);

		INIT_LIST_HEAD(&dev->rx_idle);

		skb_queue_head_init(&dev->rx_done);

		INIT_WORK(&dev->kevent, defer_kevent);
		INIT_WORK(&dev->process_rx_w, data_bridge_process_rx);

		__dev[i] = dev;
	}

	ret = usb_register(&bridge_driver);
	if (ret) {
		pr_err("%s: unable to register mdm_bridge driver", __func__);
		goto error;
	}

	data_bridge_debugfs_init();

	return 0;

error:
	while (--i >= 0) {
		kfree(__dev[i]);
		__dev[i] = NULL;
	}
	destroy_workqueue(bridge_wq);
free_ctrl:
	ctrl_bridge_exit();
	return ret;
}

static void __exit bridge_exit(void)
{
	int	i;

	usb_deregister(&bridge_driver);
	data_bridge_debugfs_exit();
	destroy_workqueue(bridge_wq);

	for (i = 0; i < MAX_BRIDGE_DEVICES; i++) {
		kfree(__dev[i]);
		__dev[i] = NULL;
	}

	ctrl_bridge_exit();
}

module_init(bridge_init);
module_exit(bridge_exit);

MODULE_DESCRIPTION("Qualcomm modem data bridge driver");
MODULE_LICENSE("GPL v2");

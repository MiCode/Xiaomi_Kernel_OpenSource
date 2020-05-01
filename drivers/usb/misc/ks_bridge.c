/*
 * Copyright (c) 2012-2014, 2017-2020, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/kobject.h>

#define DRIVER_DESC	"USB host ks bridge driver"
#define DEVICE_NAME	"ks_usb_bridge"

struct data_pkt {
	int			n_read;
	char			*buf;
	size_t			len;
	struct list_head	list;
	void			*ctxt;
};

#define FILE_OPENED		BIT(0)
#define USB_DEV_CONNECTED	BIT(1)
#define NO_RX_REQS		10
#define NO_BRIDGE_INSTANCES	4
#define MAX_DATA_PKT_SIZE	16384
#define PENDING_URB_TIMEOUT	10

struct ks_bridge {
	char			name[sizeof(DEVICE_NAME) + 3];
	spinlock_t		lock;
	struct workqueue_struct	*wq;
	struct work_struct	to_mdm_work;
	struct work_struct	start_rx_work;
	struct list_head	to_mdm_list;
	struct list_head	to_ks_list;
	wait_queue_head_t	ks_wait_q;
	wait_queue_head_t	pending_urb_wait;
	atomic_t		tx_pending_cnt;
	atomic_t		rx_pending_cnt;

	/* cdev interface */
	dev_t			cdev_start_no;
	struct cdev		cdev;
	struct class		*class;
	struct device		*device;

	/* usb specific */
	struct usb_device	*udev;
	struct usb_interface	*ifc;
	unsigned int		in_pipe;
	unsigned int		out_pipe;
	struct usb_anchor	submitted;

	unsigned long		flags;
};

static struct ks_bridge *__ksb[NO_BRIDGE_INSTANCES];

#define DBG_MSG_LEN   40
#define DBG_MAX_MSG   500

static char (dbgbuf[DBG_MAX_MSG])[DBG_MSG_LEN];   /* buffer */
static unsigned int dbg_idx;
static rwlock_t dbg_lock = __RW_LOCK_UNLOCKED(lck);

/* by default debugging is enabled */
static unsigned int enable_dbg = 1;
module_param(enable_dbg, uint, 0644);

static bool prevent_edl_probe;
module_param(prevent_edl_probe, bool, 0644);

/*get_timestamp - returns time of day in us */
static unsigned int get_timestamp(void)
{
	struct timeval	tval;
	unsigned int	stamp;

	do_gettimeofday(&tval);
	/* 2^32 = 4294967296. Limit to 4096s. */
	stamp = tval.tv_sec & 0xFFF;
	stamp = stamp * 1000000 + tval.tv_usec;
	return stamp;
}

static void dbg_inc(unsigned int *idx)
{
	*idx = (*idx + 1) % (DBG_MAX_MSG - 1);
}

static void
dbg_log_event(struct ks_bridge *ksb, char *event, int d1, int d2)
{
	unsigned long flags;

	if (!enable_dbg)
		return;

	write_lock_irqsave(&dbg_lock, flags);
	scnprintf(dbgbuf[dbg_idx], DBG_MSG_LEN, "%u:%s:%x:%x",
			get_timestamp(), event, d1, d2);

	dbg_inc(&dbg_idx);
	write_unlock_irqrestore(&dbg_lock, flags);
}

static
struct data_pkt *ksb_alloc_data_pkt(size_t count, gfp_t flags, void *ctxt)
{
	struct data_pkt *pkt;

	pkt = kzalloc(sizeof(struct data_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(count, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}

	pkt->len = count;
	INIT_LIST_HEAD(&pkt->list);
	pkt->ctxt = ctxt;

	return pkt;
}

static void ksb_free_data_pkt(struct data_pkt *pkt)
{
	kfree(pkt->buf);
	kfree(pkt);
}

static void
submit_one_urb(struct ks_bridge *ksb, gfp_t flags, struct data_pkt *pkt);
static ssize_t ksb_fs_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	int ret;
	unsigned long flags;
	struct ks_bridge *ksb = fp->private_data;
	struct data_pkt *pkt = NULL;
	size_t space, copied;

read_start:
	if (!test_bit(USB_DEV_CONNECTED, &ksb->flags))
		return -ENODEV;

	spin_lock_irqsave(&ksb->lock, flags);
	if (list_empty(&ksb->to_ks_list)) {
		spin_unlock_irqrestore(&ksb->lock, flags);
		ret = wait_event_interruptible(ksb->ks_wait_q,
				!list_empty(&ksb->to_ks_list) ||
				!test_bit(USB_DEV_CONNECTED, &ksb->flags));
		if (ret < 0)
			return ret;

		goto read_start;
	}

	space = count;
	copied = 0;
	while (!list_empty(&ksb->to_ks_list) && space &&
			test_bit(USB_DEV_CONNECTED, &ksb->flags)) {
		size_t len;

		pkt = list_first_entry(&ksb->to_ks_list, struct data_pkt, list);
		list_del_init(&pkt->list);
		len = min_t(size_t, space, pkt->len - pkt->n_read);
		spin_unlock_irqrestore(&ksb->lock, flags);

		ret = copy_to_user(buf + copied, pkt->buf + pkt->n_read, len);
		if (ret) {
			dev_err(ksb->device, "%s: copy_to_user failed err:%d\n",
						__func__, ret);
			ksb_free_data_pkt(pkt);
			return -EFAULT;
		}

		pkt->n_read += len;
		space -= len;
		copied += len;

		if (pkt->n_read == pkt->len) {
			/*
			 * re-init the packet and queue it
			 * for more data.
			 */
			pkt->n_read = 0;
			pkt->len = MAX_DATA_PKT_SIZE;
			submit_one_urb(ksb, GFP_KERNEL, pkt);
			pkt = NULL;
		}
		spin_lock_irqsave(&ksb->lock, flags);
	}

	/* put the partial packet back in the list */
	if (!space && pkt && pkt->n_read != pkt->len) {
		if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
			list_add(&pkt->list, &ksb->to_ks_list);
		else
			ksb_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ksb->lock, flags);

	dbg_log_event(ksb, "KS_READ", copied, 0);

	dev_dbg(ksb->device, "%s: count:%zu space:%zu copied:%zu\n",
			__func__, count, space, copied);

	return copied;
}

static void ksb_tx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ks_bridge *ksb = pkt->ctxt;

	dbg_log_event(ksb, "C TX_URB", urb->status, 0);
	dev_dbg(&ksb->udev->dev, "%s: status:%d\n", __func__, urb->status);

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		usb_autopm_put_interface_async(ksb->ifc);

	if (urb->status < 0)
		pr_err_ratelimited("%s: urb failed with err:%d\n",
				ksb->name, urb->status);

	ksb_free_data_pkt(pkt);

	atomic_dec(&ksb->tx_pending_cnt);
	wake_up(&ksb->pending_urb_wait);
}

static void ksb_tomdm_work(struct work_struct *w)
{
	struct ks_bridge *ksb = container_of(w, struct ks_bridge, to_mdm_work);
	struct data_pkt	*pkt;
	unsigned long flags;
	struct urb *urb;
	int ret;

	spin_lock_irqsave(&ksb->lock, flags);
	while (!list_empty(&ksb->to_mdm_list)
			&& test_bit(USB_DEV_CONNECTED, &ksb->flags)) {
		pkt = list_first_entry(&ksb->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		spin_unlock_irqrestore(&ksb->lock, flags);

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			dbg_log_event(ksb, "TX_URB_MEM_FAIL", -ENOMEM, 0);
			pr_err_ratelimited("%s: unable to allocate urb\n",
				ksb->name);
			ksb_free_data_pkt(pkt);
			return;
		}

		ret = usb_autopm_get_interface(ksb->ifc);
		if (ret < 0 && ret != -EAGAIN && ret != -EACCES) {
			dbg_log_event(ksb, "TX_URB_AUTOPM_FAIL", ret, 0);
			pr_err_ratelimited("%s: autopm_get failed:%d\n",
					ksb->name, ret);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			return;
		}
		usb_fill_bulk_urb(urb, ksb->udev, ksb->out_pipe,
				pkt->buf, pkt->len, ksb_tx_cb, pkt);
		urb->transfer_flags |= URB_ZERO_PACKET;
		usb_anchor_urb(urb, &ksb->submitted);

		dbg_log_event(ksb, "S TX_URB", pkt->len, 0);

		atomic_inc(&ksb->tx_pending_cnt);
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			dev_err(&ksb->udev->dev, "%s: out urb submission failed\n",
					__func__);
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			usb_autopm_put_interface(ksb->ifc);
			atomic_dec(&ksb->tx_pending_cnt);
			wake_up(&ksb->pending_urb_wait);
			return;
		}

		usb_free_urb(urb);

		spin_lock_irqsave(&ksb->lock, flags);
	}
	spin_unlock_irqrestore(&ksb->lock, flags);
}

static ssize_t ksb_fs_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	int			ret;
	struct data_pkt		*pkt;
	unsigned long		flags;
	struct ks_bridge	*ksb = fp->private_data;

	if (!test_bit(USB_DEV_CONNECTED, &ksb->flags))
		return -ENODEV;

	if (count > MAX_DATA_PKT_SIZE)
		count = MAX_DATA_PKT_SIZE;

	pkt = ksb_alloc_data_pkt(count, GFP_KERNEL, ksb);
	if (IS_ERR(pkt)) {
		dev_err(ksb->device, "%s: unable to allocate data packet\n",
							__func__);
		return PTR_ERR(pkt);
	}

	ret = copy_from_user(pkt->buf, buf, count);
	if (ret) {
		dev_err(ksb->device, "%s: copy_from_user failed: err:%d\n",
							__func__, ret);
		ksb_free_data_pkt(pkt);
		return ret;
	}

	spin_lock_irqsave(&ksb->lock, flags);
	list_add_tail(&pkt->list, &ksb->to_mdm_list);
	spin_unlock_irqrestore(&ksb->lock, flags);

	queue_work(ksb->wq, &ksb->to_mdm_work);

	dbg_log_event(ksb, "KS_WRITE", count, 0);

	return count;
}

static int ksb_fs_open(struct inode *ip, struct file *fp)
{
	struct ks_bridge *ksb =
			container_of(ip->i_cdev, struct ks_bridge, cdev);

	if (IS_ERR(ksb)) {
		pr_err("%s: ksb device not found\n", __func__);
		return -ENODEV;
	}

	dev_dbg(ksb->device, "%s\n", ksb->name);
	dbg_log_event(ksb, "FS-OPEN", 0, 0);

	fp->private_data = ksb;
	set_bit(FILE_OPENED, &ksb->flags);

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		queue_work(ksb->wq, &ksb->start_rx_work);

	return 0;
}

static unsigned int ksb_fs_poll(struct file *file, poll_table *wait)
{
	struct ks_bridge	*ksb = file->private_data;
	unsigned long		flags;
	int			ret = 0;

	if (!test_bit(USB_DEV_CONNECTED, &ksb->flags))
		return POLLERR;

	poll_wait(file, &ksb->ks_wait_q, wait);
	if (!test_bit(USB_DEV_CONNECTED, &ksb->flags))
		return POLLERR;

	spin_lock_irqsave(&ksb->lock, flags);
	if (!list_empty(&ksb->to_ks_list))
		ret = POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&ksb->lock, flags);

	return ret;
}

static int ksb_fs_release(struct inode *ip, struct file *fp)
{
	struct ks_bridge	*ksb = fp->private_data;
	struct data_pkt		*pkt;
	unsigned long		flags;

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		dev_dbg(ksb->device, "%s\n", ksb->name);
	dbg_log_event(ksb, "FS-RELEASE", 0, 0);

	spin_lock_irqsave(&ksb->lock, flags);
	while (!list_empty(&ksb->to_ks_list)) {
		pkt = list_first_entry(&ksb->to_ks_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ksb_free_data_pkt(pkt);
	}
	while (!list_empty(&ksb->to_mdm_list)) {
		pkt = list_first_entry(&ksb->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ksb_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ksb->lock, flags);
	usb_kill_anchored_urbs(&ksb->submitted);
	cancel_work_sync(&ksb->start_rx_work);
	wait_event_interruptible_timeout(
					ksb->pending_urb_wait,
					!atomic_read(&ksb->tx_pending_cnt) &&
					!atomic_read(&ksb->rx_pending_cnt),
					msecs_to_jiffies(PENDING_URB_TIMEOUT));
	clear_bit(FILE_OPENED, &ksb->flags);
	fp->private_data = NULL;

	return 0;
}

static const struct file_operations ksb_fops = {
	.owner = THIS_MODULE,
	.read = ksb_fs_read,
	.write = ksb_fs_write,
	.open = ksb_fs_open,
	.release = ksb_fs_release,
	.poll = ksb_fs_poll,
};

static void ksb_rx_cb(struct urb *urb);
static void
submit_one_urb(struct ks_bridge *ksb, gfp_t flags, struct data_pkt *pkt)
{
	struct urb *urb;
	int ret;

	urb = usb_alloc_urb(0, flags);
	if (!urb) {
		dev_err(&ksb->udev->dev, "%s: unable to allocate urb\n",
							__func__);
		ksb_free_data_pkt(pkt);
		return;
	}

	usb_fill_bulk_urb(urb, ksb->udev, ksb->in_pipe, pkt->buf, pkt->len,
							ksb_rx_cb, pkt);

	usb_anchor_urb(urb, &ksb->submitted);

	if (!test_bit(USB_DEV_CONNECTED, &ksb->flags)) {
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		ksb_free_data_pkt(pkt);
		return;
	}

	atomic_inc(&ksb->rx_pending_cnt);
	ret = usb_submit_urb(urb, flags);
	if (ret) {
		dev_err(&ksb->udev->dev, "%s: in urb submission failed\n",
							__func__);
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		ksb_free_data_pkt(pkt);
		atomic_dec(&ksb->rx_pending_cnt);
		wake_up(&ksb->pending_urb_wait);
		return;
	}

	dbg_log_event(ksb, "S RX_URB", pkt->len, 0);

	usb_free_urb(urb);
}

static void ksb_rx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ks_bridge *ksb = pkt->ctxt;
	bool wakeup = true;

	dbg_log_event(ksb, "C RX_URB", urb->status, urb->actual_length);

	dev_dbg(&ksb->udev->dev, "%s: status:%d actual:%d\n",
			__func__, urb->status, urb->actual_length);

	/*non zero len of data received while unlinking urb*/
	if (urb->status == -ENOENT && (urb->actual_length > 0)) {
		/*
		 * If we wakeup the reader process now, it may
		 * queue the URB before its reject flag gets
		 * cleared.
		 */
		wakeup = false;
		goto add_to_list;
	}

	if (urb->status < 0) {
		if (urb->status != -ESHUTDOWN && urb->status != -ENOENT
				&& urb->status != -EPROTO)
			pr_err_ratelimited("%s: urb failed with err:%d\n",
					ksb->name, urb->status);

		if (!urb->actual_length) {
			ksb_free_data_pkt(pkt);
			goto done;
		}
	}

	usb_mark_last_busy(ksb->udev);

	if (urb->actual_length == 0) {
		submit_one_urb(ksb, GFP_ATOMIC, pkt);
		goto done;
	}

add_to_list:
	spin_lock(&ksb->lock);
	pkt->len = urb->actual_length;
	list_add_tail(&pkt->list, &ksb->to_ks_list);
	spin_unlock(&ksb->lock);
	/* wake up read thread */
	if (wakeup)
		wake_up(&ksb->ks_wait_q);
done:
	atomic_dec(&ksb->rx_pending_cnt);
	wake_up(&ksb->pending_urb_wait);
}

static void ksb_start_rx_work(struct work_struct *w)
{
	struct ks_bridge *ksb =
			container_of(w, struct ks_bridge, start_rx_work);
	struct data_pkt	*pkt;
	struct urb *urb;
	int i = 0;
	int ret;
	bool put = true;

	ret = usb_autopm_get_interface(ksb->ifc);
	if (ret < 0) {
		if (ret != -EAGAIN && ret != -EACCES) {
			pr_err_ratelimited("%s: autopm_get failed:%d\n",
					ksb->name, ret);
			return;
		}
		put = false;
	}
	for (i = 0; i < NO_RX_REQS; i++) {

		if (!test_bit(USB_DEV_CONNECTED, &ksb->flags))
			break;

		pkt = ksb_alloc_data_pkt(MAX_DATA_PKT_SIZE, GFP_KERNEL, ksb);
		if (IS_ERR(pkt)) {
			dev_err(&ksb->udev->dev,
				 "%s: unable to allocate data pkt\n", __func__);
			break;
		}

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			dev_err(&ksb->udev->dev,
				 "%s: unable to allocate urb\n", __func__);
			ksb_free_data_pkt(pkt);
			break;
		}

		usb_fill_bulk_urb(urb, ksb->udev, ksb->in_pipe, pkt->buf,
					pkt->len, ksb_rx_cb, pkt);

		usb_anchor_urb(urb, &ksb->submitted);

		dbg_log_event(ksb, "S RX_URB", pkt->len, 0);

		atomic_inc(&ksb->rx_pending_cnt);
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			dev_err(&ksb->udev->dev,
				 "%s: in urb submission failed\n", __func__);
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			atomic_dec(&ksb->rx_pending_cnt);
			wake_up(&ksb->pending_urb_wait);
			break;
		}

		usb_free_urb(urb);
	}
	if (put)
		usb_autopm_put_interface_async(ksb->ifc);
}

static void ks_bridge_notify_status(struct kobject *kobj,
						const struct usb_device_id *id)
{
	char product_info[32];
	char *envp[2] = { product_info, NULL };

	snprintf(product_info, sizeof(product_info), "PRODUCT=%x/%x/%x",
			id->idVendor, id->idProduct, id->bDeviceProtocol);
	kobject_uevent_env(kobj, KOBJ_ONLINE, envp);
}

static int
ksb_usb_probe(struct usb_interface *ifc, const struct usb_device_id *id)
{
	struct usb_host_endpoint	*endpoint = NULL;
	struct usb_host_endpoint	*bulk_in = NULL;
	struct usb_host_endpoint	*bulk_out = NULL;
	int				i;
	struct ks_bridge		*ksb;
	unsigned long			flags;
	struct data_pkt			*pkt;
	struct usb_device		*udev;
	int				ret;
	int				devid;

	pr_debug("%s: id: %lu\n", __func__, id->driver_info);

	if (prevent_edl_probe && (le16_to_cpu(id->idProduct) == 0x9008)) {
		pr_err("%s: Preventing EDL device enumeration\n", __func__);
		return -EINVAL;
	}

	udev = interface_to_usbdev(ifc);
	if (udev->actconfig->desc.bNumInterfaces > 1) {
		pr_err("%s: Invalid configuration: More than 1 interface\n",
				__func__);
		return -EINVAL;
	}

	devid = id->driver_info & 0xFF;
	if (devid < 0 || devid >= NO_BRIDGE_INSTANCES) {
		pr_err("%s: Invalid device ID: %d\n", __func__, devid);
		return -EINVAL;
	}

	for (i = 0; i < ifc->cur_altsetting->desc.bNumEndpoints; i++) {
		endpoint = ifc->cur_altsetting->endpoint + i;
		if (!endpoint) {
			dev_err(&ifc->dev, "%s: invalid endpoint %u\n",
					__func__, i);
			return -EINVAL;
		}

		if (!bulk_in && usb_endpoint_is_bulk_in(&endpoint->desc))
			bulk_in = endpoint;
		else if (!bulk_out && usb_endpoint_is_bulk_out(&endpoint->desc))
			bulk_out = endpoint;
	}

	if (!(bulk_in && bulk_out)) {
		dev_err(&ifc->dev, "%s: could not find IN and OUT bulk EPs\n",
					__func__);
		return -EINVAL;
	}

	ksb = __ksb[devid];
	if (ksb->ifc) {
		dev_err(&ifc->dev, "%s: Port already in use\n", __func__);
		return -ENODEV;
	}

	ksb->udev = usb_get_dev(udev);
	ksb->ifc = usb_get_intf(ifc);
	ksb->in_pipe = usb_rcvbulkpipe(ksb->udev,
					bulk_in->desc.bEndpointAddress);
	ksb->out_pipe = usb_sndbulkpipe(ksb->udev,
					bulk_out->desc.bEndpointAddress);
	usb_set_intfdata(ifc, ksb);
	snprintf(ksb->name, sizeof(ksb->name), "%s.%d", DEVICE_NAME, devid);
	spin_lock_init(&ksb->lock);
	INIT_LIST_HEAD(&ksb->to_mdm_list);
	INIT_LIST_HEAD(&ksb->to_ks_list);
	init_waitqueue_head(&ksb->ks_wait_q);
	init_waitqueue_head(&ksb->pending_urb_wait);
	ksb->wq = create_singlethread_workqueue(ksb->name);
	if (!ksb->wq) {
		pr_err("%s: unable to allocate workqueue\n", __func__);
		ret = -ENOMEM;
		goto clean_dev;
	}

	INIT_WORK(&ksb->to_mdm_work, ksb_tomdm_work);
	INIT_WORK(&ksb->start_rx_work, ksb_start_rx_work);
	init_usb_anchor(&ksb->submitted);
	set_bit(USB_DEV_CONNECTED, &ksb->flags);
	atomic_set(&ksb->tx_pending_cnt, 0);
	atomic_set(&ksb->rx_pending_cnt, 0);

	dbg_log_event(ksb, "PID-ATT", id->idProduct, 0);

	/*free up stale buffers if any from previous disconnect*/
	spin_lock_irqsave(&ksb->lock, flags);
	while (!list_empty(&ksb->to_ks_list)) {
		pkt = list_first_entry(&ksb->to_ks_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ksb_free_data_pkt(pkt);
	}
	while (!list_empty(&ksb->to_mdm_list)) {
		pkt = list_first_entry(&ksb->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ksb_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ksb->lock, flags);

	ret = alloc_chrdev_region(&ksb->cdev_start_no, 0, 1, ksb->name);
	if (ret < 0) {
		dbg_log_event(ksb, "chr reg failed", ret, 0);
		goto fail_chrdev_region;
	}

	ksb->class = class_create(THIS_MODULE, ksb->name);
	if (IS_ERR(ksb->class)) {
		dbg_log_event(ksb, "clscr failed", PTR_ERR(ksb->class), 0);
		ret = PTR_ERR(ksb->class);
		goto fail_class_create;
	}

	cdev_init(&ksb->cdev, &ksb_fops);
	ksb->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ksb->cdev, ksb->cdev_start_no, 1);
	if (ret < 0) {
		dbg_log_event(ksb, "cdev_add failed", ret, 0);
		goto fail_cdev_add;
	}

	ksb->device = device_create(ksb->class, &udev->dev, ksb->cdev_start_no,
				NULL, ksb->name);
	if (IS_ERR(ksb->device)) {
		dbg_log_event(ksb, "devcrfailed", PTR_ERR(ksb->device), 0);
		ret = PTR_ERR(ksb->device);
		goto fail_device_create;
	}

	if (device_can_wakeup(&ksb->udev->dev))
		ifc->needs_remote_wakeup = 1;

	ks_bridge_notify_status(&ksb->device->kobj, id);
	dev_dbg(&ifc->dev, "%s: usb dev connected\n", __func__);

	return 0;

fail_device_create:
	cdev_del(&ksb->cdev);
fail_cdev_add:
	class_destroy(ksb->class);
fail_class_create:
	unregister_chrdev_region(ksb->cdev_start_no, 1);
fail_chrdev_region:
	clear_bit(USB_DEV_CONNECTED, &ksb->flags);
	destroy_workqueue(ksb->wq);
clean_dev:
	usb_set_intfdata(ifc, NULL);
	usb_put_intf(ifc);
	ksb->ifc = NULL;
	usb_put_dev(ksb->udev);

	return ret;
}

static void ksb_usb_disconnect(struct usb_interface *ifc)
{
	struct ks_bridge *ksb = usb_get_intfdata(ifc);
	unsigned long flags;
	struct data_pkt *pkt;

	dev_dbg(&ksb->ifc->dev, "%s\n", __func__);
	dbg_log_event(ksb, "PID-DETACH", 0, 0);

	kobject_uevent(&ksb->device->kobj, KOBJ_OFFLINE);
	ifc->needs_remote_wakeup = 0;
	device_destroy(ksb->class, ksb->cdev_start_no);
	cdev_del(&ksb->cdev);
	class_destroy(ksb->class);
	unregister_chrdev_region(ksb->cdev_start_no, 1);
	spin_lock_irqsave(&ksb->lock, flags);
	while (!list_empty(&ksb->to_ks_list)) {
		pkt = list_first_entry(&ksb->to_ks_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ksb_free_data_pkt(pkt);
	}
	while (!list_empty(&ksb->to_mdm_list)) {
		pkt = list_first_entry(&ksb->to_mdm_list,
				struct data_pkt, list);
		list_del_init(&pkt->list);
		ksb_free_data_pkt(pkt);
	}
	spin_unlock_irqrestore(&ksb->lock, flags);
	clear_bit(USB_DEV_CONNECTED, &ksb->flags);
	usb_kill_anchored_urbs(&ksb->submitted);
	cancel_work_sync(&ksb->to_mdm_work);
	cancel_work_sync(&ksb->start_rx_work);
	destroy_workqueue(ksb->wq);
	wait_event_interruptible_timeout(
					ksb->pending_urb_wait,
					!atomic_read(&ksb->tx_pending_cnt) &&
					!atomic_read(&ksb->rx_pending_cnt),
					msecs_to_jiffies(PENDING_URB_TIMEOUT));
	wake_up(&ksb->ks_wait_q);
	usb_set_intfdata(ifc, NULL);
	usb_put_intf(ifc);
	ksb->ifc = NULL;
	usb_put_dev(ksb->udev);
}

static int ksb_usb_suspend(struct usb_interface *ifc, pm_message_t message)
{
	struct ks_bridge *ksb = usb_get_intfdata(ifc);
	unsigned long flags;

	dbg_log_event(ksb, "SUSPEND", 0, 0);

	if (pm_runtime_autosuspend_expiration(&ksb->udev->dev)) {
		dbg_log_event(ksb, "SUSP ABORT-TimeCheck", 0, 0);
		return -EBUSY;
	}

	usb_kill_anchored_urbs(&ksb->submitted);

	spin_lock_irqsave(&ksb->lock, flags);
	if (!list_empty(&ksb->to_ks_list)) {
		spin_unlock_irqrestore(&ksb->lock, flags);
		dbg_log_event(ksb, "SUSPEND ABORT", 0, 0);
		/*
		 * Now wakeup the reader process and queue
		 * Rx URBs for more data.
		 */
		wake_up(&ksb->ks_wait_q);
		queue_work(ksb->wq, &ksb->start_rx_work);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&ksb->lock, flags);

	return 0;
}

static int ksb_usb_resume(struct usb_interface *ifc)
{
	struct ks_bridge *ksb = usb_get_intfdata(ifc);

	dbg_log_event(ksb, "RESUME", 0, 0);

	if (test_bit(FILE_OPENED, &ksb->flags))
		queue_work(ksb->wq, &ksb->start_rx_work);

	return 0;
}

#if defined(CONFIG_DEBUG_FS)

static int ksb_debug_show(struct seq_file *s, void *unused)
{
	unsigned long		flags;
	unsigned int		i;

	read_lock_irqsave(&dbg_lock, flags);
	i = dbg_idx;
	for (dbg_inc(&i); i != dbg_idx; dbg_inc(&i)) {
		if (!strnlen(dbgbuf[i], DBG_MSG_LEN))
			continue;
		seq_printf(s, "%s\n", dbgbuf[i]);
	}

	read_unlock_irqrestore(&dbg_lock, flags);

	return 0;
}

static int ksb_debug_open(struct inode *ip, struct file *fp)
{
	return single_open(fp, ksb_debug_show, ip->i_private);
}

static const struct file_operations dbg_fops = {
	.open = ksb_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *dbg_dir;

static void ksb_debugfs_init(void)
{
	struct dentry *dbg_file;

	dbg_dir = debugfs_create_dir("ks_bridge", NULL);
	if (IS_ERR(dbg_dir))
		return;

	dbg_file = debugfs_create_file("log", 0444, dbg_dir, NULL, &dbg_fops);
	if (!dbg_file || IS_ERR(dbg_file)) {
		debugfs_remove_recursive(dbg_dir);
		dbg_dir = NULL;
	}

	return;
}

static void ksb_debugfs_exit(void)
{
	debugfs_remove_recursive(dbg_dir);
	dbg_dir = NULL;
}

#else

static void ksb_debugfs_init(void) { }
static void ksb_debugfs_exit(void) { }

#endif

#define DEV_ID(n)		(n)

static const struct usb_device_id ksb_usb_ids[] = {

	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9008, 0),
	.driver_info = DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x900E, 0),
	.driver_info = DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90F3, 0),
	.driver_info =	DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90FD, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9102, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9103, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9104, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9105, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9106, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9107, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x910A, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x910B, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x910C, 0),
	.driver_info =  DEV_ID(0), },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x910D, 0),
	.driver_info =  DEV_ID(0), },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, ksb_usb_ids);

static struct usb_driver ksb_usb_driver = {
	.name =		"ks_bridge",
	.probe =	ksb_usb_probe,
	.disconnect =	ksb_usb_disconnect,
	.suspend =	ksb_usb_suspend,
	.resume =	ksb_usb_resume,
	.reset_resume =	ksb_usb_resume,
	.id_table =	ksb_usb_ids,
	.supports_autosuspend = 1,
};

static int __init ksb_init(void)
{
	struct ks_bridge *ksb;
	int ret;
	int i;
	int num_instances = 0;

	for (i = 0; i < NO_BRIDGE_INSTANCES; i++) {
		ksb = kzalloc(sizeof(*ksb), GFP_KERNEL);
		if (!ksb) {
			ret = -ENOMEM;
			goto dev_free;
		}

		num_instances++;
		__ksb[i] = ksb;
	}

	ret = usb_register(&ksb_usb_driver);
	if (ret) {
		pr_err("%s: unable to register ks bridge driver\n", __func__);
		goto dev_free;
	}

	ksb_debugfs_init();

	return 0;

dev_free:
	for (i = 0; i < num_instances; i++) {
		ksb = __ksb[i];
		kfree(ksb);
		__ksb[i] = NULL;
	}

	return ret;
}

static void __exit ksb_exit(void)
{
	struct ks_bridge *ksb;
	int i;

	ksb_debugfs_exit();
	usb_deregister(&ksb_usb_driver);
	for (i = 0; i < NO_BRIDGE_INSTANCES; i++) {
		ksb = __ksb[i];
		kfree(ksb);
		__ksb[i] = NULL;
	}
}

module_init(ksb_init);
module_exit(ksb_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

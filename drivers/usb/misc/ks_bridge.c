/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/* add additional information to our printk's */
#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/wait.h>

#define DRIVER_DESC	"USB host ks bridge driver"
#define DRIVER_VERSION	"1.0"

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
#define NO_BRIDGE_INSTANCES	2
#define BOOT_BRIDGE_INDEX	0
#define EFS_BRIDGE_INDEX	1
#define MAX_DATA_PKT_SIZE	16384

struct ks_bridge {
	char			*name;
	spinlock_t		lock;
	struct workqueue_struct	*wq;
	struct work_struct	to_mdm_work;
	struct work_struct	start_rx_work;
	struct list_head	to_mdm_list;
	struct list_head	to_ks_list;
	wait_queue_head_t	ks_wait_q;
	struct miscdevice	*fs_dev;

	/* usb specific */
	struct usb_device	*udev;
	struct usb_interface	*ifc;
	__u8			in_epAddr;
	__u8			out_epAddr;
	unsigned int		in_pipe;
	unsigned int		out_pipe;
	struct usb_anchor	submitted;

	unsigned long		flags;
	unsigned int		alloced_read_pkts;

#define DBG_MSG_LEN   40
#define DBG_MAX_MSG   500
	unsigned int	dbg_idx;
	rwlock_t	dbg_lock;
	char     (dbgbuf[DBG_MAX_MSG])[DBG_MSG_LEN];   /* buffer */
};
struct ks_bridge *__ksb[NO_BRIDGE_INSTANCES];

/* by default debugging is enabled */
static unsigned int enable_dbg = 1;
module_param(enable_dbg, uint, S_IRUGO | S_IWUSR);

static void
dbg_log_event(struct ks_bridge *ksb, char *event, int d1, int d2)
{
	unsigned long flags;
	unsigned long long t;
	unsigned long nanosec;

	if (!enable_dbg)
		return;

	write_lock_irqsave(&ksb->dbg_lock, flags);
	t = cpu_clock(smp_processor_id());
	nanosec = do_div(t, 1000000000)/1000;
	scnprintf(ksb->dbgbuf[ksb->dbg_idx], DBG_MSG_LEN, "%5lu.%06lu:%s:%x:%x",
			(unsigned long)t, nanosec, event, d1, d2);

	ksb->dbg_idx++;
	ksb->dbg_idx = ksb->dbg_idx % DBG_MAX_MSG;
	write_unlock_irqrestore(&ksb->dbg_lock, flags);
}

static
struct data_pkt *ksb_alloc_data_pkt(size_t count, gfp_t flags, void *ctxt)
{
	struct data_pkt *pkt;

	pkt = kzalloc(sizeof(struct data_pkt), flags);
	if (!pkt) {
		pr_err("failed to allocate data packet\n");
		return ERR_PTR(-ENOMEM);
	}

	pkt->buf = kmalloc(count, flags);
	if (!pkt->buf) {
		pr_err("failed to allocate data buffer\n");
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


static ssize_t ksb_fs_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	int ret;
	unsigned long flags;
	struct ks_bridge *ksb = fp->private_data;
	struct data_pkt *pkt;
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
	while (!list_empty(&ksb->to_ks_list) && space) {
		size_t len;

		pkt = list_first_entry(&ksb->to_ks_list, struct data_pkt, list);
		len = min_t(size_t, space, pkt->len);
		pkt->n_read += len;
		spin_unlock_irqrestore(&ksb->lock, flags);

		ret = copy_to_user(buf + copied, pkt->buf, len);
		if (ret) {
			pr_err("copy_to_user failed err:%d\n", ret);
			ksb_free_data_pkt(pkt);
			ksb->alloced_read_pkts--;
			return ret;
		}

		space -= len;
		copied += len;

		spin_lock_irqsave(&ksb->lock, flags);
		if (pkt->n_read == pkt->len) {
			list_del_init(&pkt->list);
			ksb_free_data_pkt(pkt);
			ksb->alloced_read_pkts--;
		}
	}
	spin_unlock_irqrestore(&ksb->lock, flags);

	dbg_log_event(ksb, "KS_READ", copied, 0);

	pr_debug("count:%d space:%d copied:%d", count, space, copied);

	return copied;
}

static void ksb_tx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ks_bridge *ksb = pkt->ctxt;

	dbg_log_event(ksb, "C TX_URB", urb->status, 0);
	pr_debug("status:%d", urb->status);

	if (ksb->ifc)
		usb_autopm_put_interface_async(ksb->ifc);

	if (urb->status < 0)
		pr_err_ratelimited("urb failed with err:%d", urb->status);

	ksb_free_data_pkt(pkt);
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
			pr_err_ratelimited("unable to allocate urb");
			ksb_free_data_pkt(pkt);
			return;
		}

		ret = usb_autopm_get_interface(ksb->ifc);
		if (ret < 0 && ret != -EAGAIN && ret != -EACCES) {
			pr_err_ratelimited("autopm_get failed:%d", ret);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			return;
		}
		usb_fill_bulk_urb(urb, ksb->udev, ksb->out_pipe,
				pkt->buf, pkt->len, ksb_tx_cb, pkt);
		usb_anchor_urb(urb, &ksb->submitted);

		dbg_log_event(ksb, "S TX_URB", pkt->len, 0);

		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			pr_err("out urb submission failed");
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			usb_autopm_put_interface(ksb->ifc);
			return;
		}

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

	pkt = ksb_alloc_data_pkt(count, GFP_KERNEL, ksb);
	if (IS_ERR(pkt)) {
		pr_err("unable to allocate data packet");
		return PTR_ERR(pkt);
	}

	ret = copy_from_user(pkt->buf, buf, count);
	if (ret) {
		pr_err("copy_from_user failed: err:%d", ret);
		ksb_free_data_pkt(pkt);
		return ret;
	}

	spin_lock_irqsave(&ksb->lock, flags);
	list_add_tail(&pkt->list, &ksb->to_mdm_list);
	spin_unlock_irqrestore(&ksb->lock, flags);

	queue_work(ksb->wq, &ksb->to_mdm_work);

	return count;
}

static int efs_fs_open(struct inode *ip, struct file *fp)
{
	struct ks_bridge *ksb = __ksb[EFS_BRIDGE_INDEX];

	pr_debug(":%s", ksb->name);
	dbg_log_event(ksb, "EFS-FS-OPEN", 0, 0);

	if (!ksb) {
		pr_err("ksb is being removed");
		return -ENODEV;
	}

	fp->private_data = ksb;
	set_bit(FILE_OPENED, &ksb->flags);

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		queue_work(ksb->wq, &ksb->start_rx_work);

	return 0;
}

static int ksb_fs_open(struct inode *ip, struct file *fp)
{
	struct ks_bridge *ksb = __ksb[BOOT_BRIDGE_INDEX];

	pr_debug(":%s", ksb->name);
	dbg_log_event(ksb, "KS-FS-OPEN", 0, 0);

	if (!ksb) {
		pr_err("ksb is being removed");
		return -ENODEV;
	}

	fp->private_data = ksb;
	set_bit(FILE_OPENED, &ksb->flags);

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		queue_work(ksb->wq, &ksb->start_rx_work);

	return 0;
}

static int ksb_fs_release(struct inode *ip, struct file *fp)
{
	struct ks_bridge	*ksb = fp->private_data;

	pr_debug(":%s", ksb->name);
	dbg_log_event(ksb, "FS-RELEASE", 0, 0);

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
};

static struct miscdevice ksb_fboot_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ks_bridge",
	.fops = &ksb_fops,
};

static const struct file_operations efs_fops = {
	.owner = THIS_MODULE,
	.read = ksb_fs_read,
	.write = ksb_fs_write,
	.open = efs_fs_open,
	.release = ksb_fs_release,
};

static struct miscdevice ksb_efs_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "efs_bridge",
	.fops = &efs_fops,
};

static const struct usb_device_id ksb_usb_ids[] = {
	{ USB_DEVICE(0x5c6, 0x9008),
	.driver_info = (unsigned long)&ksb_fboot_dev, },
	{ USB_DEVICE(0x5c6, 0x9048),
	.driver_info = (unsigned long)&ksb_efs_dev, },
	{ USB_DEVICE(0x5c6, 0x904C),
	.driver_info = (unsigned long)&ksb_efs_dev, },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, ksb_usb_ids);

static void ksb_rx_cb(struct urb *urb);
static void submit_one_urb(struct ks_bridge *ksb)
{
	struct data_pkt	*pkt;
	struct urb *urb;
	int ret;

	pkt = ksb_alloc_data_pkt(MAX_DATA_PKT_SIZE, GFP_ATOMIC, ksb);
	if (IS_ERR(pkt)) {
		pr_err("unable to allocate data pkt");
		return;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		pr_err("unable to allocate urb");
		ksb_free_data_pkt(pkt);
		return;
	}
	ksb->alloced_read_pkts++;

	usb_fill_bulk_urb(urb, ksb->udev, ksb->in_pipe,
			pkt->buf, pkt->len,
			ksb_rx_cb, pkt);
	usb_anchor_urb(urb, &ksb->submitted);

	dbg_log_event(ksb, "S RX_URB", pkt->len, 0);

	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret) {
		pr_err("in urb submission failed");
		usb_unanchor_urb(urb);
		usb_free_urb(urb);
		ksb_free_data_pkt(pkt);
		ksb->alloced_read_pkts--;
		return;
	}

	usb_free_urb(urb);
}
static void ksb_rx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ks_bridge *ksb = pkt->ctxt;

	dbg_log_event(ksb, "C RX_URB", urb->status, urb->actual_length);

	pr_debug("status:%d actual:%d", urb->status, urb->actual_length);

	if (urb->status < 0) {
		if (urb->status != -ESHUTDOWN && urb->status != -ENOENT)
			pr_err_ratelimited("urb failed with err:%d",
					urb->status);
		ksb_free_data_pkt(pkt);
		ksb->alloced_read_pkts--;
		return;
	}

	if (urb->actual_length == 0) {
		ksb_free_data_pkt(pkt);
		ksb->alloced_read_pkts--;
		goto resubmit_urb;
	}

	spin_lock(&ksb->lock);
	pkt->len = urb->actual_length;
	list_add_tail(&pkt->list, &ksb->to_ks_list);
	spin_unlock(&ksb->lock);

	/* wake up read thread */
	wake_up(&ksb->ks_wait_q);

resubmit_urb:
	submit_one_urb(ksb);

}

static void ksb_start_rx_work(struct work_struct *w)
{
	struct ks_bridge *ksb =
			container_of(w, struct ks_bridge, start_rx_work);
	struct data_pkt	*pkt;
	struct urb *urb;
	int i = 0;
	int ret;

	for (i = 0; i < NO_RX_REQS; i++) {
		pkt = ksb_alloc_data_pkt(MAX_DATA_PKT_SIZE, GFP_KERNEL, ksb);
		if (IS_ERR(pkt)) {
			pr_err("unable to allocate data pkt");
			return;
		}

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			pr_err("unable to allocate urb");
			ksb_free_data_pkt(pkt);
			return;
		}

		ret = usb_autopm_get_interface(ksb->ifc);
		if (ret < 0 && ret != -EAGAIN && ret != -EACCES) {
			pr_err_ratelimited("autopm_get failed:%d", ret);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			return;
		}
		ksb->alloced_read_pkts++;

		usb_fill_bulk_urb(urb, ksb->udev, ksb->in_pipe,
				pkt->buf, pkt->len,
				ksb_rx_cb, pkt);
		usb_anchor_urb(urb, &ksb->submitted);

		dbg_log_event(ksb, "S RX_URB", pkt->len, 0);

		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			pr_err("in urb submission failed");
			usb_unanchor_urb(urb);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			ksb->alloced_read_pkts--;
			usb_autopm_put_interface(ksb->ifc);
			return;
		}

		usb_autopm_put_interface_async(ksb->ifc);
		usb_free_urb(urb);
	}
}

static int
ksb_usb_probe(struct usb_interface *ifc, const struct usb_device_id *id)
{
	__u8				ifc_num;
	struct usb_host_interface	*ifc_desc;
	struct usb_endpoint_descriptor	*ep_desc;
	int				i;
	struct ks_bridge		*ksb;

	ifc_num = ifc->cur_altsetting->desc.bInterfaceNumber;

	switch (id->idProduct) {
	case 0x9008:
		if (ifc_num != 0)
			return -ENODEV;
		ksb = __ksb[BOOT_BRIDGE_INDEX];
		break;
	case 0x9048:
	case 0x904C:
		if (ifc_num != 2)
			return -ENODEV;
		ksb = __ksb[EFS_BRIDGE_INDEX];
		break;
	default:
		return -ENODEV;
	}

	if (!ksb) {
		pr_err("ksb is not initialized");
		return -ENODEV;
	}

	ksb->udev = usb_get_dev(interface_to_usbdev(ifc));
	ksb->ifc = ifc;
	ifc_desc = ifc->cur_altsetting;

	for (i = 0; i < ifc_desc->desc.bNumEndpoints; i++) {
		ep_desc = &ifc_desc->endpoint[i].desc;

		if (!ksb->in_epAddr && usb_endpoint_is_bulk_in(ep_desc))
			ksb->in_epAddr = ep_desc->bEndpointAddress;

		if (!ksb->out_epAddr && usb_endpoint_is_bulk_out(ep_desc))
			ksb->out_epAddr = ep_desc->bEndpointAddress;
	}

	if (!(ksb->in_epAddr && ksb->out_epAddr)) {
		pr_err("could not find bulk in and bulk out endpoints");
		usb_put_dev(ksb->udev);
		ksb->ifc = NULL;
		return -ENODEV;
	}

	ksb->in_pipe = usb_rcvbulkpipe(ksb->udev, ksb->in_epAddr);
	ksb->out_pipe = usb_sndbulkpipe(ksb->udev, ksb->out_epAddr);

	usb_set_intfdata(ifc, ksb);
	set_bit(USB_DEV_CONNECTED, &ksb->flags);

	dbg_log_event(ksb, "PID-ATT", id->idProduct, 0);

	ksb->fs_dev = (struct miscdevice *)id->driver_info;
	misc_register(ksb->fs_dev);

	usb_enable_autosuspend(ksb->udev);

	pr_debug("usb dev connected");

	return 0;
}

static int ksb_usb_suspend(struct usb_interface *ifc, pm_message_t message)
{
	struct ks_bridge *ksb = usb_get_intfdata(ifc);

	dbg_log_event(ksb, "SUSPEND", 0, 0);

	pr_info("read cnt: %d", ksb->alloced_read_pkts);

	usb_kill_anchored_urbs(&ksb->submitted);

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

static void ksb_usb_disconnect(struct usb_interface *ifc)
{
	struct ks_bridge *ksb = usb_get_intfdata(ifc);
	unsigned long flags;
	struct data_pkt *pkt;

	dbg_log_event(ksb, "PID-DETACH", 0, 0);

	clear_bit(USB_DEV_CONNECTED, &ksb->flags);
	wake_up(&ksb->ks_wait_q);
	cancel_work_sync(&ksb->to_mdm_work);

	usb_kill_anchored_urbs(&ksb->submitted);

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

	misc_deregister(ksb->fs_dev);
	usb_put_dev(ksb->udev);
	ksb->ifc = NULL;
	usb_set_intfdata(ifc, NULL);

	return;
}

static struct usb_driver ksb_usb_driver = {
	.name =		"ks_bridge",
	.probe =	ksb_usb_probe,
	.disconnect =	ksb_usb_disconnect,
	.suspend =	ksb_usb_suspend,
	.resume =	ksb_usb_resume,
	.id_table =	ksb_usb_ids,
	.supports_autosuspend = 1,
};

static ssize_t ksb_debug_show(struct seq_file *s, void *unused)
{
	unsigned long		flags;
	struct ks_bridge	*ksb = s->private;
	int			i;

	read_lock_irqsave(&ksb->dbg_lock, flags);
	for (i = 0; i < DBG_MAX_MSG; i++) {
		if (i == (ksb->dbg_idx - 1))
			seq_printf(s, "-->%s\n", ksb->dbgbuf[i]);
		else
			seq_printf(s, "%s\n", ksb->dbgbuf[i]);
	}
	read_unlock_irqrestore(&ksb->dbg_lock, flags);

	return 0;
}

static int ksb_debug_open(struct inode *ip, struct file *fp)
{
	return single_open(fp, ksb_debug_show, ip->i_private);

	return 0;
}

static const struct file_operations dbg_fops = {
	.open = ksb_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
static struct dentry *dbg_dir;
static int __init ksb_init(void)
{
	struct ks_bridge *ksb;
	int num_instances = 0;
	int ret = 0;
	int i;

	dbg_dir = debugfs_create_dir("ks_bridge", NULL);
	if (IS_ERR(dbg_dir))
		pr_err("unable to create debug dir");

	for (i = 0; i < NO_BRIDGE_INSTANCES; i++) {
		ksb = kzalloc(sizeof(struct ks_bridge), GFP_KERNEL);
		if (!ksb) {
			pr_err("unable to allocat mem for ks_bridge");
			ret =  -ENOMEM;
			goto dev_free;
		}
		__ksb[i] = ksb;

		ksb->name = kasprintf(GFP_KERNEL, "ks_bridge:%i", i + 1);
		if (!ksb->name) {
			pr_info("unable to allocate name");
			kfree(ksb);
			ret = -ENOMEM;
			goto dev_free;
		}

		spin_lock_init(&ksb->lock);
		INIT_LIST_HEAD(&ksb->to_mdm_list);
		INIT_LIST_HEAD(&ksb->to_ks_list);
		init_waitqueue_head(&ksb->ks_wait_q);
		ksb->wq = create_singlethread_workqueue(ksb->name);
		if (!ksb->wq) {
			pr_err("unable to allocate workqueue");
			kfree(ksb->name);
			kfree(ksb);
			ret = -ENOMEM;
			goto dev_free;
		}

		INIT_WORK(&ksb->to_mdm_work, ksb_tomdm_work);
		INIT_WORK(&ksb->start_rx_work, ksb_start_rx_work);
		init_usb_anchor(&ksb->submitted);

		ksb->dbg_idx = 0;
		ksb->dbg_lock = __RW_LOCK_UNLOCKED(lck);

		if (!IS_ERR(dbg_dir))
			debugfs_create_file(ksb->name, S_IRUGO, dbg_dir,
					ksb, &dbg_fops);

		num_instances++;
	}

	ret = usb_register(&ksb_usb_driver);
	if (ret) {
		pr_err("unable to register ks bridge driver");
		goto dev_free;
	}

	pr_info("init done");

	return 0;

dev_free:
	if (!IS_ERR(dbg_dir))
		debugfs_remove_recursive(dbg_dir);

	for (i = 0; i < num_instances; i++) {
		ksb = __ksb[i];

		destroy_workqueue(ksb->wq);
		kfree(ksb->name);
		kfree(ksb);
	}

	return ret;

}

static void __exit ksb_exit(void)
{
	struct ks_bridge *ksb;
	int i;

	if (!IS_ERR(dbg_dir))
		debugfs_remove_recursive(dbg_dir);

	usb_deregister(&ksb_usb_driver);

	for (i = 0; i < NO_BRIDGE_INSTANCES; i++) {
		ksb = __ksb[i];

		destroy_workqueue(ksb->wq);
		kfree(ksb->name);
		kfree(ksb);
	}
}

module_init(ksb_init);
module_exit(ksb_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");

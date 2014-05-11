/*
 * Copyright (c) 2012-2014, Linux Foundation. All rights reserved.
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
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define DRIVER_DESC	"USB host ks bridge driver"
#define DRIVER_VERSION	"1.0"

enum bus_id {
	BUS_HSIC,
	BUS_USB,
	BUS_UNDEF,
};

#define BUSNAME_LEN	20

static enum bus_id str_to_busid(const char *name)
{
	if (!strncasecmp("msm_hsic_host", name, BUSNAME_LEN))
		return BUS_HSIC;
	if (!strncasecmp("msm_ehci_host.0", name, BUSNAME_LEN))
		return BUS_USB;

	return BUS_UNDEF;
}

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
#define EFS_HSIC_BRIDGE_INDEX	2
#define EFS_USB_BRIDGE_INDEX	3
#define MAX_DATA_PKT_SIZE	16384
#define PENDING_URB_TIMEOUT	10

struct ksb_dev_info {
	const char *name;
};

struct ks_bridge {
	char			*name;
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

	struct ksb_dev_info	id_info;

	/* cdev interface */
	dev_t			cdev_start_no;
	struct cdev		cdev;
	struct class		*class;
	struct device		*device;

	/* usb specific */
	struct usb_device	*udev;
	struct usb_interface	*ifc;
	__u8			in_epAddr;
	__u8			out_epAddr;
	unsigned int		in_pipe;
	unsigned int		out_pipe;
	struct usb_anchor	submitted;

	unsigned long		flags;

	/* to handle INT IN ep */
	unsigned int		period;

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
			dev_err(ksb->device,
					"copy_to_user failed err:%d\n", ret);
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

	dev_dbg(ksb->device, "count:%d space:%d copied:%d", count,
			space, copied);

	return copied;
}

static void ksb_tx_cb(struct urb *urb)
{
	struct data_pkt *pkt = urb->context;
	struct ks_bridge *ksb = pkt->ctxt;

	dbg_log_event(ksb, "C TX_URB", urb->status, 0);
	dev_dbg(&ksb->udev->dev, "status:%d", urb->status);

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		usb_autopm_put_interface_async(ksb->ifc);

	if (urb->status < 0)
		pr_err_ratelimited("%s: urb failed with err:%d",
				ksb->id_info.name, urb->status);

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
			pr_err_ratelimited("%s: unable to allocate urb",
					ksb->id_info.name);
			ksb_free_data_pkt(pkt);
			return;
		}

		ret = usb_autopm_get_interface(ksb->ifc);
		if (ret < 0 && ret != -EAGAIN && ret != -EACCES) {
			dbg_log_event(ksb, "TX_URB_AUTOPM_FAIL", ret, 0);
			pr_err_ratelimited("%s: autopm_get failed:%d",
					ksb->id_info.name, ret);
			usb_free_urb(urb);
			ksb_free_data_pkt(pkt);
			return;
		}
		usb_fill_bulk_urb(urb, ksb->udev, ksb->out_pipe,
				pkt->buf, pkt->len, ksb_tx_cb, pkt);
		usb_anchor_urb(urb, &ksb->submitted);

		dbg_log_event(ksb, "S TX_URB", pkt->len, 0);

		atomic_inc(&ksb->tx_pending_cnt);
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			dev_err(&ksb->udev->dev, "out urb submission failed");
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
		dev_err(ksb->device,
				"unable to allocate data packet");
		return PTR_ERR(pkt);
	}

	ret = copy_from_user(pkt->buf, buf, count);
	if (ret) {
		dev_err(ksb->device,
				"copy_from_user failed: err:%d", ret);
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
		pr_err("ksb device not found");
		return -ENODEV;
	}

	dev_dbg(ksb->device, ":%s", ksb->id_info.name);
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

	if (test_bit(USB_DEV_CONNECTED, &ksb->flags))
		dev_dbg(ksb->device, ":%s", ksb->id_info.name);
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
	.poll = ksb_fs_poll,
};

static struct ksb_dev_info ksb_fboot_dev[] = {
	{
		.name = "ks_hsic_bridge",
	},
	{
		.name = "ks_usb_bridge",
	},
};

static struct ksb_dev_info ksb_efs_hsic_dev = {
	.name = "efs_hsic_bridge",
};

static struct ksb_dev_info ksb_efs_usb_dev = {
	.name = "efs_usb_bridge",
};
static const struct usb_device_id ksb_usb_ids[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9008, 0),
	.driver_info = (unsigned long)&ksb_fboot_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9048, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x904C, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9075, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x9079, 2),
	.driver_info = (unsigned long)&ksb_efs_usb_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908A, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x908E, 3),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909C, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909D, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909E, 3),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x909F, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A0, 2),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },
	{ USB_DEVICE_INTERFACE_NUMBER(0x5c6, 0x90A4, 3),
	.driver_info = (unsigned long)&ksb_efs_hsic_dev, },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, ksb_usb_ids);

static void ksb_rx_cb(struct urb *urb);
static void
submit_one_urb(struct ks_bridge *ksb, gfp_t flags, struct data_pkt *pkt)
{
	struct urb *urb;
	int ret;

	urb = usb_alloc_urb(0, flags);
	if (!urb) {
		dev_err(&ksb->udev->dev, "unable to allocate urb");
		ksb_free_data_pkt(pkt);
		return;
	}

	if (ksb->period)
		usb_fill_int_urb(urb, ksb->udev, ksb->in_pipe,
				 pkt->buf, pkt->len,
				 ksb_rx_cb, pkt, ksb->period);
	else
		usb_fill_bulk_urb(urb, ksb->udev, ksb->in_pipe,
				pkt->buf, pkt->len,
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
		dev_err(&ksb->udev->dev, "in urb submission failed");
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

	dev_dbg(&ksb->udev->dev, "status:%d actual:%d", urb->status,
			urb->actual_length);

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
			pr_err_ratelimited("%s: urb failed with err:%d",
					ksb->id_info.name, urb->status);

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
			pr_err_ratelimited("%s: autopm_get failed:%d",
					ksb->id_info.name, ret);
			return;
		}
		put = false;
	}
	for (i = 0; i < NO_RX_REQS; i++) {

		if (!test_bit(USB_DEV_CONNECTED, &ksb->flags))
			break;

		pkt = ksb_alloc_data_pkt(MAX_DATA_PKT_SIZE, GFP_KERNEL, ksb);
		if (IS_ERR(pkt)) {
			dev_err(&ksb->udev->dev, "unable to allocate data pkt");
			break;
		}

		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			dev_err(&ksb->udev->dev, "unable to allocate urb");
			ksb_free_data_pkt(pkt);
			break;
		}

		if (ksb->period)
			usb_fill_int_urb(urb, ksb->udev, ksb->in_pipe,
					pkt->buf, pkt->len,
					ksb_rx_cb, pkt, ksb->period);
		else
			usb_fill_bulk_urb(urb, ksb->udev, ksb->in_pipe,
					pkt->buf, pkt->len,
					ksb_rx_cb, pkt);

		usb_anchor_urb(urb, &ksb->submitted);

		dbg_log_event(ksb, "S RX_URB", pkt->len, 0);

		atomic_inc(&ksb->rx_pending_cnt);
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			dev_err(&ksb->udev->dev, "in urb submission failed");
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

static int
ksb_usb_probe(struct usb_interface *ifc, const struct usb_device_id *id)
{
	__u8				ifc_num;
	struct usb_host_interface	*ifc_desc;
	struct usb_endpoint_descriptor	*ep_desc;
	int				i;
	struct ks_bridge		*ksb;
	unsigned long			flags;
	struct data_pkt			*pkt;
	struct ksb_dev_info		*mdev, *fbdev;
	struct usb_device		*udev;
	unsigned int			bus_id;
	int ret;

	ifc_num = ifc->cur_altsetting->desc.bInterfaceNumber;

	udev = interface_to_usbdev(ifc);
	fbdev = mdev = (struct ksb_dev_info *)id->driver_info;

	bus_id = str_to_busid(udev->bus->bus_name);
	if (bus_id == BUS_UNDEF) {
		dev_err(&udev->dev, "unknown usb bus %s, probe failed\n",
				udev->bus->bus_name);
		return -ENODEV;
	}

	switch (id->idProduct) {
	case 0x9008:
		ksb = __ksb[bus_id];
		mdev = &fbdev[bus_id];
		break;
	case 0x9048:
	case 0x904C:
	case 0x9075:
	case 0x908A:
	case 0x908E:
	case 0x90A0:
	case 0x909C:
	case 0x909D:
	case 0x909E:
	case 0x909F:
	case 0x90A4:
		ksb = __ksb[EFS_HSIC_BRIDGE_INDEX];
		break;
	case 0x9079:
		if (ifc_num != 2)
			return -ENODEV;
		ksb = __ksb[EFS_USB_BRIDGE_INDEX];
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
	ksb->id_info = *mdev;

	for (i = 0; i < ifc_desc->desc.bNumEndpoints; i++) {
		ep_desc = &ifc_desc->endpoint[i].desc;

		if (!ksb->in_epAddr && (usb_endpoint_is_bulk_in(ep_desc))) {
			ksb->in_epAddr = ep_desc->bEndpointAddress;
			ksb->period = 0;
		}

		if (!ksb->in_epAddr && (usb_endpoint_is_int_in(ep_desc))) {
			ksb->in_epAddr = ep_desc->bEndpointAddress;
			ksb->period = ep_desc->bInterval;
		}

		if (!ksb->out_epAddr && usb_endpoint_is_bulk_out(ep_desc))
			ksb->out_epAddr = ep_desc->bEndpointAddress;
	}

	if (!(ksb->in_epAddr && ksb->out_epAddr)) {
		dev_err(&udev->dev,
			"could not find bulk in and bulk out endpoints");
		usb_put_dev(ksb->udev);
		ksb->ifc = NULL;
		return -ENODEV;
	}

	ksb->in_pipe = ksb->period ?
		usb_rcvintpipe(ksb->udev, ksb->in_epAddr) :
		usb_rcvbulkpipe(ksb->udev, ksb->in_epAddr);

	ksb->out_pipe = usb_sndbulkpipe(ksb->udev, ksb->out_epAddr);

	usb_set_intfdata(ifc, ksb);
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

	ret = alloc_chrdev_region(&ksb->cdev_start_no, 0, 1, mdev->name);
	if (ret < 0) {
		dbg_log_event(ksb, "chr reg failed", ret, 0);
		goto fail_chrdev_region;
	}

	ksb->class = class_create(THIS_MODULE, mdev->name);
	if (IS_ERR(ksb->class)) {
		dbg_log_event(ksb, "clscr failed", PTR_ERR(ksb->class), 0);
		goto fail_class_create;
	}

	cdev_init(&ksb->cdev, &ksb_fops);
	ksb->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ksb->cdev, ksb->cdev_start_no, 1);
	if (ret < 0) {
		dbg_log_event(ksb, "cdev_add failed", ret, 0);
		goto fail_class_create;
	}

	ksb->device = device_create(ksb->class, NULL, ksb->cdev_start_no,
				NULL, mdev->name);
	if (IS_ERR(ksb->device)) {
		dbg_log_event(ksb, "devcrfailed", PTR_ERR(ksb->device), 0);
		goto fail_device_create;
	}

	if (device_can_wakeup(&ksb->udev->dev)) {
		ifc->needs_remote_wakeup = 1;
		usb_enable_autosuspend(ksb->udev);
	}

	dev_dbg(&udev->dev, "usb dev connected");

	return 0;

fail_device_create:
	cdev_del(&ksb->cdev);
fail_class_create:
	unregister_chrdev_region(ksb->cdev_start_no, 1);
fail_chrdev_region:
	usb_set_intfdata(ifc, NULL);
	clear_bit(USB_DEV_CONNECTED, &ksb->flags);

	return -ENODEV;

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

static void ksb_usb_disconnect(struct usb_interface *ifc)
{
	struct ks_bridge *ksb = usb_get_intfdata(ifc);
	unsigned long flags;
	struct data_pkt *pkt;

	dbg_log_event(ksb, "PID-DETACH", 0, 0);

	clear_bit(USB_DEV_CONNECTED, &ksb->flags);
	wake_up(&ksb->ks_wait_q);
	cancel_work_sync(&ksb->to_mdm_work);
	cancel_work_sync(&ksb->start_rx_work);

	device_destroy(ksb->class, ksb->cdev_start_no);
	cdev_del(&ksb->cdev);
	class_destroy(ksb->class);
	unregister_chrdev_region(ksb->cdev_start_no, 1);

	usb_kill_anchored_urbs(&ksb->submitted);

	wait_event_interruptible_timeout(
					ksb->pending_urb_wait,
					!atomic_read(&ksb->tx_pending_cnt) &&
					!atomic_read(&ksb->rx_pending_cnt),
					msecs_to_jiffies(PENDING_URB_TIMEOUT));

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

	ifc->needs_remote_wakeup = 0;
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
	.reset_resume =	ksb_usb_resume,
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
		init_waitqueue_head(&ksb->pending_urb_wait);
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

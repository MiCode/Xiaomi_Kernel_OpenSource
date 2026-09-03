// SPDX-License-Identifier: GPL-2.0+
/*
 * Vendor specific serial Gadget Driver use for put modem log.
 * From the Gadget side, it is a misc device(No TTY),
 * from PC side, sprd's usb2serial driver will enumerate the
 * device as a serial device.
 * This driver basicly from android adb driver, many thanks to
 * Android guys.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/configfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>

#define VSER_BULK_BUFFER_SIZE	(4096 * 16)
#define MAX_INST_NAME_LEN	40
/* number of tx requests to allocate */
#define TX_REQ_MAX		4
#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
#define TX_REQ_BYPASS_MAX		100
#define MAX_PKTS_PER_XFER		32
#define TX_STOP_THRESHOLD		1000
#endif
/* Device --> Host */
#define SET_IN_SIZE		0x00
/* Host --> Device */
#define SET_OUT_SIZE		0x01
#define START_TEST		0x10
#define STOP_TEST		0x20
#define UPLINK_TEST		0x01
#define DOWNLINK_TEST		0x02
#define LOOP_TEST		0x03

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
static void (*bulk_in_complete_function)(char *buffer,
					 unsigned int length, void *p);
static void *s_callback_data;
static bool s_in_bypass_mode;

struct pass_buf {
	char				*buf;
	int				len;
	struct list_head		list;
};

struct sg_ctx {
	struct list_head		list;
	ktime_t				q_time;
};

static struct workqueue_struct	*vser_tx_wq;
#endif

static const char vser_shortname[] = "vser";
static int tx_req_count;

struct vser_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t	lock;
	spinlock_t	req_lock;	/* guard {rx,tx}_reqs */

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int online;
	int rd_error;
	int wr_error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;
	int open_count;

	struct list_head tx_idle;
#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	struct list_head tx_pass_idle;

	/* statistic */
	u32			dl_rx_packet_count;
	u32			dl_sent_packet_count;
	u32			dl_sent_failed_packet_count;
	u64			dl_rx_bytes;
	u64			dl_sent_bytes;

	u64			max_cb_time_per_xfer;

	u64			min_time_per_xfer;
	u64			max_time_per_xfer;

	struct timer_list	log_print_timer;

	/* sg mode */
	struct list_head	tx_pass_buf_q;
	bool			sg_enabled;

	u32		dl_tx_pass_buf_qlen;
	u32		dl_tx_stop_threshold;
	u32		dl_max_pkts_per_xfer;
	int		dl_tx_work_status;
	int		dl_tx_req_status;

	struct work_struct	tx_work;
#endif

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
	int test_mode;
	int size_in;
	int size_out;
	int total_size;
	struct work_struct test_work;
	struct workqueue_struct *test_wq;
};

struct vser_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct vser_dev *dev;
};

static struct vser_dev *_vser_dev;

/* interface descriptor */
static struct usb_interface_descriptor vser_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = 0,
	.bInterfaceProtocol     = 0,
};

/* super speed support */
static struct usb_endpoint_descriptor vser_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor vser_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor vser_ss_bulk_comp_desc = {
	.bLength =              sizeof(vser_ss_bulk_comp_desc),
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_descriptor_header *vser_ss_function[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_ss_in_desc,
	(struct usb_descriptor_header *) &vser_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &vser_ss_out_desc,
	(struct usb_descriptor_header *) &vser_ss_bulk_comp_desc,
	NULL,
};

/* high speed support */
static struct usb_endpoint_descriptor vser_hs_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor vser_hs_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = cpu_to_le16(512),
};

static struct usb_descriptor_header *vser_hs_function[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_hs_in_desc,
	(struct usb_descriptor_header *) &vser_hs_out_desc,
	NULL,
};

/* full speed support */
static struct usb_endpoint_descriptor vser_fs_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor vser_fs_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *vser_fs_function[] = {
	(struct usb_descriptor_header *) &vser_interface_desc,
	(struct usb_descriptor_header *) &vser_fs_in_desc,
	(struct usb_descriptor_header *) &vser_fs_out_desc,
	NULL,
};

static inline struct vser_dev *func_to_vser(struct usb_function *f)
{
	return container_of(f, struct vser_dev, function);
}

static struct usb_request *vser_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);

	if (!req)
		return NULL;

	/* now allocate buffers for the request */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void vser_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int vser_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1)
		return 0;

	atomic_dec(excl);
	return -1;
}

static inline void vser_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
static void vser_req_put(struct vser_dev *dev, struct list_head *head,
			 struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->req_lock, flags);
	list_add_tail(&req->list, head);
	tx_req_count++;
	spin_unlock_irqrestore(&dev->req_lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *vser_req_get(struct vser_dev *dev,
					struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->req_lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
		tx_req_count--;
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);

	return req;
}

static void vser_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	if (req->status != 0)
		dev->wr_error = 1;

	vser_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
/* add a pbuf to the tail of a list */
static void vser_pbuf_put(struct vser_dev *dev, struct list_head *head,
			 struct pass_buf *pbuf)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&pbuf->list, head);
	if (&dev->tx_pass_buf_q == head)
		dev->dl_tx_pass_buf_qlen++;
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a pbuf from the head of a list */
static struct pass_buf *vser_pbuf_get(struct vser_dev *dev,
					struct list_head *head)
{
	unsigned long flags;
	struct pass_buf *pbuf;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		pbuf = 0;
	} else {
		pbuf = list_first_entry(head, struct pass_buf, list);
		list_del(&pbuf->list);
		if (&dev->tx_pass_buf_q == head)
			dev->dl_tx_pass_buf_qlen--;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	return pbuf;
}

static struct pass_buf *vser_alloc_pass_buf(unsigned int len, gfp_t flags)
{
	struct pass_buf *pbuf;

	pbuf = kzalloc(sizeof(struct pass_buf), flags);
	if (!pbuf)
		return NULL;

	pbuf->buf = NULL;
	pbuf->len = len;
	INIT_LIST_HEAD(&pbuf->list);

	return pbuf;
}

static void vser_pass_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	if (req->status != 0)
		dev->wr_error = 1;

	vser_req_put(dev, &dev->tx_pass_idle, req);

	if (bulk_in_complete_function != NULL)
		bulk_in_complete_function(req->buf, req->length,
						s_callback_data);

	wake_up(&dev->write_wq);
}
#endif

static void vser_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev *dev = _vser_dev;

	dev->rx_done = 1;
	if (req->status != 0)
		dev->rd_error = 1;

	wake_up(&dev->read_wq);
}

static void vser_free_all_request(struct vser_dev *dev)
{
	struct usb_request *req;

	vser_request_free(dev->rx_req, dev->ep_out);
	while ((req = vser_req_get(dev, &dev->tx_idle)))
		vser_request_free(req, dev->ep_in);
}

static int vser_alloc_all_request(struct vser_dev *dev)
{
	struct usb_request *req;
	int i;

	req = vser_request_new(dev->ep_out, dev->size_out);
	if (!req)
		return -ENOMEM;

	req->complete = vser_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = vser_request_new(dev->ep_in, dev->size_in);
		if (!req) {
			vser_free_all_request(dev);
			return -ENOMEM;
		}

		req->complete = vser_complete_in;
		vser_req_put(dev, &dev->tx_idle, req);
	}

	tx_req_count = TX_REQ_MAX;
	return 0;
}

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
static void tx_complete(struct usb_ep *ep, struct usb_request *req);
static void process_tx_w(struct work_struct *work);
static int prealloc_sg(struct list_head *list,
	 struct usb_ep *ep, unsigned int n, bool sg_enabled)
{
	struct vser_dev	*dev = ep->driver_data;
	unsigned int		i;
	struct usb_request	*req;
	struct sg_ctx		*sg_ctx;

	if (!n)
		return -EINVAL;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}
	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_KERNEL);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;

		vser_req_put(dev, &dev->tx_pass_idle, req);

		if (!sg_enabled) {
			req->buf = NULL;
			req->complete = vser_pass_complete_in;
			continue;
		}

		req->complete = tx_complete;
		req->sg = kcalloc(MAX_PKTS_PER_XFER,
				sizeof(struct scatterlist), GFP_KERNEL);
		if (!req->sg)
			goto extra;

		sg_ctx = kmalloc(sizeof(*sg_ctx), GFP_KERNEL);
		if (!sg_ctx)
			goto extra;
		req->context = sg_ctx;
		req->buf = NULL;
	}
	return 0;

extra:
	/* free extras */
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);
		tx_req_count--;

		kfree(req->buf);
		kfree(req->sg);
		kfree(req->context);

		usb_ep_free_request(ep, req);

		if (next == list)
			break;
		req = container_of(next, struct usb_request, list);
	}
	return -ENOMEM;
}
#endif

static int vser_create_bulk_endpoints(struct vser_dev *dev,
				      struct usb_endpoint_descriptor *in_desc,
				      struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_ep *ep;
	int ret = 0;

	DBG(cdev, "%s dev: %p\n", __func__, dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}

	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}

	DBG(cdev, "usb_ep_autoconfig for vser ep_out got %s\n", ep->name);
	ep->driver_data = dev;
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	ret = vser_alloc_all_request(dev);
	if (ret)
		goto fail;

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	ret = prealloc_sg(&dev->tx_pass_idle, dev->ep_in, TX_REQ_BYPASS_MAX, dev->sg_enabled);
	if (ret)
		goto fail;
#endif

	return 0;

fail:
	DBG(cdev, "Allocate requests failed.\n");
	return ret;
}

static ssize_t vser_read(struct file *fp, char __user *buf,
			 size_t count, loff_t *pos)
{
	struct vser_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	int r = count, xfer, ret;

	DBG(cdev, "vser read %d.\n", r);

	if (dev->test_mode)
		return -EINVAL;

	if (count > dev->size_out)
		return -EINVAL;

	if (vser_lock(&dev->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(dev->online || dev->rd_error)) {
		DBG(cdev, "%s: waiting for online state\n", __func__);
		ret = wait_event_interruptible(dev->read_wq,
				(dev->online || dev->rd_error));
		if (ret < 0) {
			vser_unlock(&dev->read_excl);
			return ret;
		}
	}

	if (dev->rd_error) {
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	/* Make bulk-out requests be divisible by the maxpacket size */
	if (dev->cdev->gadget->quirk_ep_out_aligned_size)
		req->length = ALIGN(count, dev->ep_out->maxpacket);
	else
		req->length = count;

	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		DBG(cdev, "%s: failed to queue req %p (%d)\n", __func__, req, ret);
		r = -EIO;
		dev->rd_error = 1;
		goto done;
	} else {
		DBG(cdev, "rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		usb_ep_dequeue(dev->ep_out, req);
		dev->rd_error = 1;
		r = ret;
		goto done;
	}

	if (!dev->rd_error) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		DBG(cdev, "rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else {
		r = -EIO;
	}

done:
	vser_unlock(&dev->read_excl);
	DBG(cdev, "vser read returning %d\n", r);
	return r;
}

static ssize_t vser_write(struct file *fp, const char __user *buf,
			  size_t count, loff_t *pos)
{
	struct vser_dev *dev = fp->private_data;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req = 0;
	int r = count, xfer, ret;

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	if (s_in_bypass_mode) {
		msleep(100);
		return count;
	}
#endif

	if (dev->test_mode)
		return -EBUSY;

	if (vser_lock(&dev->write_excl))
		return -EBUSY;

	DBG(cdev, "vser write = %d.\n", r);

	/* we will block until we're online */
	while (!(dev->online || dev->wr_error)) {
		DBG(cdev, "%s: waiting for online state\n", __func__);
		ret = wait_event_interruptible(dev->write_wq,
				(dev->online || dev->wr_error));
		if (ret < 0) {
			vser_unlock(&dev->write_excl);
			return ret;
		}
	}

	while (count > 0) {
		if (dev->wr_error) {
			DBG(cdev, "%s dev->wr_error\n", __func__);
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;

		if (list_empty(&dev->tx_idle))
			DBG(cdev, "%s: tx buffer is full!\n", __func__);

		ret = wait_event_interruptible(dev->write_wq,
			((req = vser_req_get(dev, &dev->tx_idle)) ||
			 dev->wr_error));
		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > dev->size_in)
				xfer = dev->size_in;
			else
				xfer = count;

			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				DBG(cdev, "%s: xfer error %d\n", __func__, ret);
				dev->wr_error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		vser_req_put(dev, &dev->tx_idle, req);

	vser_unlock(&dev->write_excl);
	DBG(cdev, "vser write returning %d\n", r);

	return r;
}

static int vser_open(struct inode *ip, struct file *fp)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	fp->private_data = dev;

	dev->open_count++;
	if (dev->open_count > 2) {
		dev->open_count--;
		DBG(cdev, "too many user open vser.\n");
		spin_unlock_irqrestore(&dev->lock, flags);
		return -EBUSY;
	}

	/* clear the error latch */
	dev->wr_error = 0;
	dev->rd_error = 0;
	spin_unlock_irqrestore(&dev->lock, flags);
	DBG(cdev, "%s: open %d times.\n", __func__, dev->open_count);

	return 0;
}

static int vser_release(struct inode *ip, struct file *fp)
{
	struct vser_dev *dev = _vser_dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	dev->open_count--;
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

/* file operations for ADB device /dev/android_vser */
static const struct file_operations vser_fops = {
	.owner = THIS_MODULE,
	.read = vser_read,
	.write = vser_write,
	.open = vser_open,
	.release = vser_release,
};

static struct miscdevice vser_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = vser_shortname,
	.fops = &vser_fops,
};

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
static ssize_t vser_statistics_show(struct device *dev,
	 struct device_attribute *attr, char *buf)
{
	struct vser_dev *vdev = _vser_dev;

	return snprintf(buf, PAGE_SIZE, "u32 dl_rx_packet_count= %u;\nu64 dl_rx_bytes= %llu;\n"
	"u32 dl_sent_packet_count= %u;\nu64 dl_sent_bytes= %llu;\nu32 dl_sent_failed_packet_count= %u;\n"
	"u64 min_time_per_xfer= %llu(ns);\nu64 max_time_per_xfer= %llu(ns);\n"
	"u64 max_cb_time_per_xfer= %llu(ns);\n",
	vdev->dl_rx_packet_count,
	vdev->dl_rx_bytes,
	vdev->dl_sent_packet_count,
	vdev->dl_sent_bytes,
	vdev->dl_sent_failed_packet_count,
	vdev->min_time_per_xfer,
	vdev->max_time_per_xfer,
	vdev->max_cb_time_per_xfer
	);
}

static DEVICE_ATTR_RO(vser_statistics);

static void vser_log_print_timer_func(struct timer_list *timer)
{
	struct vser_dev *dev = from_timer(dev, timer, log_print_timer);

	pr_info("u32 dl_rx_packet_count= %u; u64 dl_rx_bytes= %llu; "
	"u32 dl_sent_packet_count= %u; u64 dl_sent_bytes= %llu; u32 dl_sent_failed_packet_count= %u; "
	"u64 min_time_per_xfer= %llu(ns); u64 max_time_per_xfer= %llu(ns); "
	"u64 max_cb_time_per_xfer= %llu(ns);\n",
	dev->dl_rx_packet_count,
	dev->dl_rx_bytes,
	dev->dl_sent_packet_count,
	dev->dl_sent_bytes,
	dev->dl_sent_failed_packet_count,
	dev->min_time_per_xfer,
	dev->max_time_per_xfer,
	dev->max_cb_time_per_xfer
	);

	mod_timer(&dev->log_print_timer, jiffies + msecs_to_jiffies(5000));
}
#endif

static void vser_test_works(struct work_struct *work);
static int vser_init(struct vser_instance *fi_vser)
{
	struct vser_dev *dev;
	int ret = 0;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	_vser_dev = dev;
	if (fi_vser != NULL)
		fi_vser->dev = dev;

	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);
	dev->open_count = 0;
	dev->size_in = VSER_BULK_BUFFER_SIZE;
	dev->size_out = VSER_BULK_BUFFER_SIZE;
	tx_req_count = 0;

	INIT_LIST_HEAD(&dev->tx_idle);
#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	INIT_LIST_HEAD(&dev->tx_pass_idle);
	timer_setup(&dev->log_print_timer, vser_log_print_timer_func, 0);
	INIT_LIST_HEAD(&dev->tx_pass_buf_q);
	INIT_WORK(&dev->tx_work, process_tx_w);
	dev->sg_enabled = true;
	dev->dl_max_pkts_per_xfer = MAX_PKTS_PER_XFER;
#endif

	INIT_WORK(&dev->test_work, vser_test_works);
	dev->test_wq = create_singlethread_workqueue("VserTestWq");
	if (!dev->test_wq) {
		ret = -ENOMEM;
		goto err1;
	}

	ret = misc_register(&vser_device);
	if (ret)
		goto err2;

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	ret = device_create_file(vser_device.this_device, &dev_attr_vser_statistics);
	if (ret)
		goto err2;
#endif

	return 0;

err2:
	destroy_workqueue(dev->test_wq);
err1:
	kfree(dev);
	return ret;
}

static void vser_cleanup(void)
{
#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	device_remove_file(vser_device.this_device, &dev_attr_vser_statistics);
#endif
	misc_deregister(&vser_device);

	kfree(_vser_dev);
	_vser_dev = NULL;
}

static int vser_function_bind(struct usb_configuration *c,
			      struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct vser_dev	*dev = func_to_vser(f);
	int id, ret;

	dev->cdev = cdev;

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	dev->dl_tx_stop_threshold = TX_STOP_THRESHOLD;
	dev->dl_tx_pass_buf_qlen = 0;
	dev->dl_tx_req_status = 0;
	dev->dl_tx_work_status = 0;
#endif

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;

	vser_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = vser_create_bulk_endpoints(dev, &vser_fs_in_desc,
					 &vser_fs_out_desc);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	dev->min_time_per_xfer = ULONG_MAX;
#endif

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	vser_hs_in_desc.bEndpointAddress = vser_fs_in_desc.bEndpointAddress;
	vser_hs_out_desc.bEndpointAddress = vser_fs_out_desc.bEndpointAddress;
	vser_ss_in_desc.bEndpointAddress = vser_fs_in_desc.bEndpointAddress;
	vser_ss_out_desc.bEndpointAddress = vser_fs_out_desc.bEndpointAddress;

	DBG(cdev, "%s speed %s IN/%s OUT/%s\n",
	    f->name,
	    gadget_is_superspeed(c->cdev->gadget) ? "super" :
	    gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
	    dev->ep_in->name, dev->ep_out->name);

	return 0;
}

static void vser_function_unbind(struct usb_configuration *c,
				 struct usb_function *f)
{
	struct vser_dev	*dev = func_to_vser(f);

	dev->online = 0;
	vser_free_all_request(dev);

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	do {
		struct usb_request *req;
		struct pass_buf	*pbuf;

		while ((req = vser_req_get(dev, &dev->tx_pass_idle))) {
			if (dev->sg_enabled) {
				kfree(req->buf);
				kfree(req->context);
				kfree(req->sg);
			}
			usb_ep_free_request(dev->ep_in, req);
		}

		if (dev->sg_enabled) {
			while ((pbuf = vser_pbuf_get(dev, &dev->tx_pass_buf_q)))
				kfree(pbuf);
		}
	} while (0);

	dev->dl_rx_packet_count = 0;
	dev->dl_rx_bytes = 0;
	dev->dl_sent_packet_count = 0;
	dev->dl_sent_bytes = 0;
	dev->dl_sent_failed_packet_count = 0;

	dev->max_time_per_xfer = 0;
	dev->min_time_per_xfer = 0;
	dev->max_cb_time_per_xfer = 0;

	del_timer(&dev->log_print_timer);
#endif

	dev->wr_error = 1;
	dev->rd_error = 1;
}

static int vser_function_set_alt(struct usb_function *f,
				 unsigned int intf,
				 unsigned int alt)
{
	struct vser_dev	*dev = func_to_vser(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "%s: intf: %d alt: %d dev:%p\n", __func__, intf, alt, dev);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	return 0;
}

static void vser_function_disable(struct usb_function *f)
{
	struct vser_dev	*dev = func_to_vser(f);
	struct usb_composite_dev *cdev = dev->cdev;

	dev->online = 0;
	dev->wr_error = 1;
	dev->rd_error = 1;

	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);
	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	wake_up(&dev->write_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int vser_setup(struct usb_function *f,
		      const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev *cdev = f->config->cdev;
	struct vser_dev	*dev = _vser_dev;
	u16 w_length = le16_to_cpu(ctrl->wLength);
	u16 w_value = le16_to_cpu(ctrl->wValue);
	int value = -EOPNOTSUPP;
	bool port_open = false;

	VDBG(cdev,
	     "request :%x request type: %x, wValue %x, wIndex %x, wLength %x\n",
	     ctrl->bRequest, ctrl->bRequestType, w_value,
	     le16_to_cpu(ctrl->wIndex), w_length);

	/* Handle Bulk-only class-specific requests */
	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		switch (ctrl->bRequest) {
		case 0x22:
			value = 0;
			if ((w_value & 0xff) == 1)
				port_open = true;
			break;
		}
	} else if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		u32 parameter;

		value = 0;
		parameter = (ctrl->wValue << 16) | ctrl->wIndex;
		switch (ctrl->bRequest) {
		case SET_IN_SIZE:
			dev->size_in = parameter;
			dev->test_mode |= 1;
			break;
		case SET_OUT_SIZE:
			dev->size_out = parameter;
			dev->test_mode |= 2;
			break;
		case START_TEST:
			vser_free_all_request(dev);
			if (vser_alloc_all_request(dev) < 0)
				VDBG(cdev, "Vser allocate memory failed!\n");
			dev->test_mode |= 0x80;
			dev->total_size = parameter;
			VDBG(cdev, "total size = 0x%x\n", parameter);

			queue_work(dev->test_wq, &dev->test_work);
			break;
		case STOP_TEST:
			vser_free_all_request(dev);
			break;
		default:
			VDBG(cdev, "wrong request.\n");
			break;
		}
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		int rc;

		cdev->req->zero = value < w_length;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0)
			VDBG(cdev, "%s setup response queue error\n", __func__);
	}

	if (value == -EOPNOTSUPP) {
		VDBG(cdev,
		     "unknown class-specific control req %02x.%02x v%04x i%04x l%u\n",
		     ctrl->bRequestType, ctrl->bRequest,
		     le16_to_cpu(ctrl->wValue), le16_to_cpu(ctrl->wIndex),
		     le16_to_cpu(ctrl->wLength));
	}

	if (port_open) {
		DBG(cdev, "cmd for port open\n");
		dev->online = 1;
		wake_up(&dev->read_wq);
		wake_up(&dev->write_wq);
#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
		mod_timer(&dev->log_print_timer, jiffies);
#endif
	}
	return value;
}

static void vser_free(struct usb_function *f)
{
	/* No need to do */
}

static struct vser_instance *to_vser_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct vser_instance,
		func_inst.group);
}

static void vser_attr_release(struct config_item *item)
{
	struct vser_instance *fi_vser = to_vser_instance(item);

	usb_put_function_instance(&fi_vser->func_inst);
}

static struct configfs_item_operations vser_item_ops = {
	.release        = vser_attr_release,
};

static struct config_item_type vser_func_type = {
	.ct_item_ops    = &vser_item_ops,
	.ct_owner       = THIS_MODULE,
};

static struct vser_instance *to_fi_vser(struct usb_function_instance *fi)
{
	return container_of(fi, struct vser_instance, func_inst);
}

static int vser_set_inst_name(struct usb_function_instance *fi,
			      const char *name)
{
	struct vser_instance *fi_vser = to_fi_vser(fi);
	char *ptr;
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	fi_vser->name = ptr;
	return 0;
}

static void vser_free_inst(struct usb_function_instance *fi)
{
	struct vser_instance *fi_vser = to_fi_vser(fi);

	kfree(fi_vser->name);
	kfree(fi_vser);

	destroy_workqueue(vser_tx_wq);

	vser_cleanup();
}

static struct usb_function_instance *vser_alloc_inst(void)
{
	struct vser_instance *fi_vser;
	int ret;

	fi_vser = kzalloc(sizeof(*fi_vser), GFP_KERNEL);
	if (!fi_vser)
		return ERR_PTR(-ENOMEM);

	fi_vser->func_inst.set_inst_name = vser_set_inst_name;
	fi_vser->func_inst.free_func_inst = vser_free_inst;

	ret = vser_init(fi_vser);
	if (ret) {
		kfree(fi_vser);
		return ERR_PTR(ret);
	}

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
	vser_tx_wq = alloc_workqueue("vser_tx",
		 WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);
	if (!vser_tx_wq) {
		kfree(fi_vser);
		pr_err("%s: Unable to create workqueue: vser_tx\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
#endif

	config_group_init_type_name(&fi_vser->func_inst.group,
				    "", &vser_func_type);

	return &fi_vser->func_inst;
}

static struct usb_function *vser_alloc(struct usb_function_instance *fi)
{
	struct vser_instance *fi_vser = to_fi_vser(fi);
	struct vser_dev *dev;

	pr_err("%s\n", __func__);
	if (!fi_vser->dev) {
		pr_err("Error: Create vser function failed.\n");
		return NULL;
	}

	dev = fi_vser->dev;
	dev->function.name = "vser";
	dev->function.fs_descriptors = vser_fs_function;
	dev->function.hs_descriptors = vser_hs_function;
	dev->function.ss_descriptors = vser_ss_function;
	dev->function.ssp_descriptors = vser_ss_function;
	dev->function.bind = vser_function_bind;
	dev->function.unbind = vser_function_unbind;
	dev->function.set_alt = vser_function_set_alt;
	dev->function.setup = vser_setup;
	dev->function.disable = vser_function_disable;
	dev->function.free_func = vser_free;

	return &dev->function;
}

DECLARE_USB_FUNCTION_INIT(vser, vser_alloc_inst, vser_alloc);

#if IS_ENABLED(CONFIG_USB_F_VSERIAL_BYPASS_USER)
void kernel_vser_register_callback(void *function, void *p)
{
	bulk_in_complete_function = function;
	s_callback_data = p;
}
EXPORT_SYMBOL(kernel_vser_register_callback);

void kernel_vser_set_pass_mode(bool pass)
{
	s_in_bypass_mode = pass;
}
EXPORT_SYMBOL(kernel_vser_set_pass_mode);

void kernel_vser_enable_sg(bool enable)
{
	struct vser_dev *dev = _vser_dev;

	dev->sg_enabled = enable;
}
EXPORT_SYMBOL(kernel_vser_enable_sg);

static void do_tx_queue_work(struct vser_dev *dev)
{
	unsigned long		flags;
	u32			max_pkts_per_xfer;

	spin_lock_irqsave(&dev->lock, flags);
	max_pkts_per_xfer = dev->dl_max_pkts_per_xfer;
	if (!max_pkts_per_xfer)
		max_pkts_per_xfer = 1;

	if ((!list_empty(&dev->tx_pass_buf_q) && !dev->dl_tx_req_status)
		|| (!dev->dl_tx_work_status
		&& (dev->dl_tx_pass_buf_qlen) >= max_pkts_per_xfer)) {
		spin_unlock_irqrestore(&dev->lock, flags);
		queue_work(vser_tx_wq, &dev->tx_work);
		return;
	}
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct vser_dev		*dev = _vser_dev;
	unsigned long		flags;
	struct sg_ctx		*sg_ctx = req->context;
	ktime_t			tv_end;
	ktime_t			tv_begin = sg_ctx->q_time;
	u64			transfer_time;

	if (req->status != 0) {
		pr_err("tx err %d\n", req->status);
		dev->wr_error = 1;
	}

	tv_end = ktime_get();
	transfer_time = ktime_to_ns(ktime_sub(tv_end, tv_begin));
	if (transfer_time < dev->min_time_per_xfer)
		dev->min_time_per_xfer = transfer_time;
	if (transfer_time > dev->max_time_per_xfer)
		dev->max_time_per_xfer = transfer_time;

	dev->dl_sent_packet_count += req->num_sgs;
	dev->dl_sent_bytes += req->length;

	if (req->num_sgs) {
		ktime_t			cb_begin;
		ktime_t			cb_end;
		u64			cb_time = 0;
		struct pass_buf		*pbuf = NULL;

		do {
			pbuf = vser_pbuf_get(dev, &sg_ctx->list);
			if (!pbuf)
				break;

			cb_begin = ktime_get();
			if (bulk_in_complete_function != NULL)
				bulk_in_complete_function(pbuf->buf, pbuf->len,
								s_callback_data);
			cb_end = ktime_get();

			cb_time = ktime_to_ns(ktime_sub(cb_end, cb_begin));
			if (cb_time > dev->max_cb_time_per_xfer)
				dev->max_cb_time_per_xfer = cb_time;

			kfree(pbuf);
		} while (1);
	}

	vser_req_put(dev, &dev->tx_pass_idle, req);

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->dl_tx_req_status > 0)
		dev->dl_tx_req_status--;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!req->status)
		do_tx_queue_work(dev);
}

static void process_tx_w(struct work_struct *work)
{
	struct vser_dev *dev = container_of(work, struct vser_dev, tx_work);
	unsigned long flags;
	struct usb_request *req;
	struct pass_buf *pbuf = NULL;
	struct sg_ctx *sg_ctx;
	int count, ret = 0;

	if (!dev || !dev->online)
		return;
	spin_lock_irqsave(&dev->lock, flags);
	dev->dl_tx_work_status = 1;
	spin_unlock_irqrestore(&dev->lock, flags);

	spin_lock_irqsave(&dev->req_lock, flags);
	while (dev->online && !list_empty(&dev->tx_pass_idle) &&
		(pbuf = vser_pbuf_get(dev, &dev->tx_pass_buf_q))) {
		req = list_first_entry(&dev->tx_pass_idle, struct usb_request, list);
		list_del(&req->list);
		tx_req_count--;
		spin_unlock_irqrestore(&dev->req_lock, flags);

		req->num_sgs = 0;
		req->zero = 0;
		req->length = 0;
		sg_ctx = req->context;
		INIT_LIST_HEAD(&sg_ctx->list);
		sg_init_table(req->sg, dev->dl_max_pkts_per_xfer);

		count = 1;
		do {
			if ((req->length + pbuf->len) > dev->size_in ||
					count > dev->dl_max_pkts_per_xfer) {
				spin_lock_irqsave(&dev->lock, flags);
				list_add(&pbuf->list, &dev->tx_pass_buf_q);
				dev->dl_tx_pass_buf_qlen++;
				spin_unlock_irqrestore(&dev->lock, flags);
				break;
			}

			sg_set_buf(&req->sg[req->num_sgs], pbuf->buf, pbuf->len);
			req->num_sgs++;
			req->length += pbuf->len;
			vser_pbuf_put(dev, &sg_ctx->list, pbuf);
			count++;
		} while (dev->online && (pbuf = vser_pbuf_get(dev, &dev->tx_pass_buf_q)));

		if (!dev->online) {
			while (!list_empty(&sg_ctx->list)) {
				pbuf = list_first_entry(&sg_ctx->list, struct pass_buf, list);
				list_del(&pbuf->list);
				kfree(pbuf);
				pbuf = NULL;
			}
			kfree(req->buf);
			kfree(req->context);
			kfree(req->sg);
			usb_ep_free_request(dev->ep_in, req);
			dev->dl_tx_work_status = 0;
			return;
		}
		sg_mark_end(&req->sg[req->num_sgs - 1]);

		if ((req->length % dev->ep_in->maxpacket) == 0)
			req->zero = 1;

		spin_lock_irqsave(&dev->req_lock, flags);
		dev->dl_tx_req_status++;
		spin_unlock_irqrestore(&dev->req_lock, flags);

		sg_ctx->q_time = ktime_get();
		ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);

		spin_lock_irqsave(&dev->req_lock, flags);
		if (ret) {
			pr_err("tx usb_ep_queue ERROR!!!, ret = %d\n", ret);

			dev->dl_sent_failed_packet_count++;
			dev->wr_error = 1;
			if (dev->dl_tx_req_status > 0)
				dev->dl_tx_req_status--;

			while (!list_empty(&sg_ctx->list)) {
				pbuf = list_first_entry(&sg_ctx->list, struct pass_buf, list);
				list_del(&pbuf->list);
				kfree(pbuf);
				pbuf = NULL;
			}

			if (!dev->online) {
				kfree(req->buf);
				kfree(req->context);
				kfree(req->sg);
				usb_ep_free_request(dev->ep_in, req);
			} else {
				list_add_tail(&req->list,  &dev->tx_pass_idle);
				tx_req_count++;
			}
			break;
		}
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
	dev->dl_tx_work_status = 0;
}

ssize_t vser_pass_user_write(char *buf, size_t count)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_request *req = 0;
	struct pass_buf *pbuf = NULL;
	int r = count, xfer, ret;

	if (!dev || !dev->online)
		return -ENODEV;

	pr_debug("%s: buf:0x%p, count:0x%llx, epname:%s\n",
		__func__, buf, (long long)count, dev->ep_in->name);

	if (!dev->ep_in->enabled) {
		pr_err("vser deactivated\n");
		return -ENODEV;
	}

	if (vser_lock(&dev->write_excl))
		return -EBUSY;

	if (dev->sg_enabled) {
		dev->dl_rx_packet_count++;
		dev->dl_rx_bytes += count;

		if (dev->dl_tx_pass_buf_qlen >= dev->dl_tx_stop_threshold) {
			pr_debug("%s stop, qlen:%d stop_threshold:%d\n",
				__func__, dev->dl_tx_pass_buf_qlen, dev->dl_tx_stop_threshold);
			vser_unlock(&dev->write_excl);
			return -EBUSY;
		}

		pbuf = vser_alloc_pass_buf(count, GFP_KERNEL);
		if (!pbuf) {
			vser_unlock(&dev->write_excl);
			return -ENOMEM;
		}
		pbuf->buf = buf;
		vser_pbuf_put(dev, &dev->tx_pass_buf_q, pbuf);

		do_tx_queue_work(dev);
		vser_unlock(&dev->write_excl);
		return r;
	}

	while (count > 0) {
		/* get an idle tx request to use */
		ret = wait_event_interruptible(dev->write_wq,
			(req = vser_req_get(dev, &dev->tx_pass_idle)));

		if (ret < 0) {
			r = ret;
			pr_debug("%s:line %d wait event failed.\n",
				__func__, __LINE__);
			break;
		}

		xfer = count;
		if (req != 0) {
			ktime_t	*tv_begin = req->context;
			req->buf = buf;
			req->length = xfer;

			*tv_begin = ktime_get();
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("%s: xfer error %d\n", __func__, ret);
				dev->wr_error = 1;
				r = -EIO;
				break;
			}
			count -= xfer;
			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		vser_req_put(dev, &dev->tx_pass_idle, req);

	vser_unlock(&dev->write_excl);
	pr_debug("%s: returning %x\n", __func__, r);
	return r;
}
EXPORT_SYMBOL(vser_pass_user_write);

ssize_t vser_iq_write(char *buf, size_t count)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_composite_dev *cdev;
	struct usb_request *req = 0;
	int r = count, xfer, ret;

	if (!dev || !dev->online)
		return -ENODEV;

	cdev = dev->cdev;

	DBG(cdev, "%s: buf:0x%p, count:0x%llx, epname:%s\n",
		__func__, buf, (long long)count, dev->ep_in->name);

	if (!dev->ep_in->enabled) {
		ERROR(cdev, "vser deactivated\n");
		return -ENODEV;
	}

	if (vser_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		/* get an idle tx request to use */
		ret = wait_event_interruptible(dev->write_wq,
			(req = vser_req_get(dev, &dev->tx_pass_idle)));

		if (ret < 0) {
			r = ret;
			DBG(cdev, "%s:line %d wait event failed.\n",
				__func__, __LINE__);
			break;
		}

		xfer = count;
		if (req != 0) {
			req->buf = buf;
			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				DBG(cdev, "%s: xfer error %d\n", __func__, ret);
				dev->wr_error = 1;
				r = -EIO;
				break;
			}
			count -= xfer;
			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req)
		vser_req_put(dev, &dev->tx_pass_idle, req);

	vser_unlock(&dev->write_excl);
	DBG(cdev, "%s: returning %x\n", __func__, r);
	return r;
}
EXPORT_SYMBOL(vser_iq_write);
#endif

static int wait_vser_event(struct vser_dev *dev, struct usb_request *req_sent)
{
	int ret;
	int free_req_count = 0;

	do {
		ret = wait_event_interruptible(dev->write_wq,
			((req_sent = vser_req_get(dev, &dev->tx_idle)) ||
			dev->wr_error));
		if (req_sent == NULL)
			continue;
		free_req_count++;
		vser_request_free(req_sent, dev->ep_in);
	} while (free_req_count < TX_REQ_MAX);
	return ret;
}
static void vser_test_works(struct work_struct *work)
{
	struct vser_dev *dev = _vser_dev;
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req_recv = NULL, *req_sent = NULL;
	int count, offset, total, ret;
	int test_mode = dev->test_mode & 0x03;
	int free_req_count = 0;
	unsigned long start_times = jiffies;

	total = dev->total_size;
	req_recv = dev->rx_req;

	DBG(cdev, "%s: start (%d, 0x%x)\n", __func__, dev->test_mode, total);
	if (!req_recv) {
		DBG(cdev, "receive request is NULL!\n");
		return;
	}

	while (dev->test_mode) {
		if (dev->test_mode & 0x80) {
			if (test_mode == LOOP_TEST) { /* loop back test */
				if (total > dev->size_out)
					req_recv->length = dev->size_out;
				else
					req_recv->length = total;

				req_recv->length =
					ALIGN(req_recv->length,
					      dev->ep_out->maxpacket);
				dev->rx_done = 0;
				ret = usb_ep_queue(dev->ep_out,
						   req_recv, GFP_ATOMIC);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				/* wait for a request to complete */
				ret = wait_event_interruptible(dev->read_wq,
							       dev->rx_done);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				if (!dev->test_mode)
					break;

				offset = 0;
				count = req_recv->actual;
				do {
					ret = wait_event_interruptible(dev->write_wq,
						((req_sent = vser_req_get(dev, &dev->tx_idle)) ||
						 dev->wr_error));
					if (!req_sent) {
						DBG(cdev, "send request is NULL!\n");
						continue;
					}

					if (count > dev->size_in) {
						memcpy(req_sent->buf,
						       req_recv->buf,
						       dev->size_in);
						count -= dev->size_in;
						offset += dev->size_in;
						req_sent->length = dev->size_in;
					} else {
						memcpy(req_sent->buf,
						       req_recv->buf, count);
						req_sent->length = count;
						offset += count;
						count = 0;
					}

					ret = usb_ep_queue(dev->ep_in,
							   req_sent,
							   GFP_ATOMIC);
					if (ret < 0) {
						dev->wr_error = 1;
						break;
					}
				} while (count > 0);

				total -= req_recv->actual;
				if (total == 0) {
					vser_request_free(req_recv,
							  dev->ep_out);
					wait_vser_event(dev, req_sent);
					break;
				}
			} else if (test_mode == DOWNLINK_TEST) {
				/* Device-->Host 0x00 UL */
				if (total > dev->size_out)
					req_recv->length = dev->size_out;
				else {
					req_recv->length = total;
					req_recv->length = ALIGN(req_recv->length,
								 dev->ep_out->maxpacket);
				}

				dev->rx_done = 0;
				ret = usb_ep_queue(dev->ep_out, req_recv,
						   GFP_ATOMIC);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				/* wait for a request to complete */
				ret = wait_event_interruptible(dev->read_wq,
							       dev->rx_done);
				if (ret < 0) {
					dev->rd_error = 1;
					break;
				}

				total -= req_recv->actual;
				if (total == 0) {
					vser_free_all_request(dev);
					break;
				}
			} else if (test_mode  == UPLINK_TEST) {
				/* Host-->Device 0x01 DL */
				ret = wait_event_interruptible(dev->write_wq,
					((req_sent = vser_req_get(dev, &dev->tx_idle)) ||
					 dev->wr_error));
				if (!req_sent)
					continue;

				if (!total) {
					vser_request_free(req_sent, dev->ep_in);
					free_req_count++;
					if (free_req_count == TX_REQ_MAX) {
						vser_request_free(req_recv,
								  dev->ep_out);
						break;
					}
					continue;
				}

				if (total > dev->size_in)
					req_sent->length = dev->size_in;
				else
					req_sent->length = total;

				ret = usb_ep_queue(dev->ep_in, req_sent,
						   GFP_ATOMIC);
				if (ret < 0) {
					dev->wr_error = 1;
					break;
				}
				total -= req_sent->length;
			}
		}
		req_sent = NULL;
	}

	DBG(cdev, "%s test time = %d\n", __func__, (int)(jiffies - start_times));
	dev->size_in = VSER_BULK_BUFFER_SIZE;
	dev->size_out = VSER_BULK_BUFFER_SIZE;
	if (vser_alloc_all_request(dev) < 0)
		DBG(cdev, "Vser allocate memory failed!!!\n");
	dev->test_mode = 0;
}
MODULE_DESCRIPTION("SPRD USB VSER");
MODULE_LICENSE("GPL v2");

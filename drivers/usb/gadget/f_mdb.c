/*
 * Gadget Driver for Android MDB
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2017 XiaoMi, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#define MDB_BULK_BUFFER_SIZE           4096
#define MDB_DATA_BULK_BUFFER_SIZE      16384
#define MDB_INTR_BUFFER_SIZE   16

/* number of tx requests to allocate */
#define TX_REQ_MAX 4
/* only for data pipe */
#define RX_DATA_REQ_MAX 3
#define INTER_REQ_MAX   3

#define STATUS_OFFLINE    0
#define STATUS_READY      1
#define STATUS_ERROR      2
#define STATUS_CANCELED   3
#define STATUS_BUSY       4

static const char mdb_shortname[] = "android_mdb";

struct mdb_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
	struct usb_ep *data_ep_in;
	struct usb_ep *data_ep_out;
	struct usb_ep *ep_intr;

	atomic_t online;
	atomic_t error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;
	atomic_t ioctl_excl;

	struct list_head tx_idle;
	struct list_head data_tx_idle;
	struct list_head intr_idle;
	struct usb_request *data_rx_req[RX_DATA_REQ_MAX];

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
	bool notify_close;
	bool close_notified;

	/* data pipe */
	wait_queue_head_t data_read_wq;
	wait_queue_head_t data_write_wq;
	wait_queue_head_t intr_wq;
	int data_rx_done;
	atomic_t data_status;
	/* for processing MDB_SEND_FILE, MDB_RECEIVE_FILE, MDB_QUERY_DATA_PORT
	 * ioctls on a work queue
	 */
	struct workqueue_struct *wq;
	struct work_struct send_file_work;
	struct work_struct receive_file_work;
	struct file *xfer_file;
	int64_t xfer_file_length;
	int xfer_result;
	unsigned transfered;
};

struct mdb_file_range {
	/* file descriptor for file to transfer */
	int			fd;
	/* number of bytes to transfer */
	int64_t		length;
};

/* event code */
#define EVENT_MDB_FAIL  0x4c494146

/* ioctl  command */
/* Sends the specified file range to the host */
#define MDB_RECEIVE_FILE        _IOW('M', 0, struct mdb_file_range)
/* Receives data from the host and writes it to a file.
 * The file is created if it does not exist.
 */
#define MDB_SEND_FILE           _IOW('M', 1, struct mdb_file_range)

#define MDB_QUERY_DATA_PORT     _IOW('M', 3, int)
#define MDB_CANCEL_TASK         _IOW('M', 4, int)
static struct usb_interface_descriptor mdb_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 5,
	.bInterfaceClass        = 0xFF,
	.bInterfaceSubClass     = 0x43,
	.bInterfaceProtocol     = 1,
};

static struct usb_endpoint_descriptor mdb_superspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor mdb_superspeed_in_comp_desc = {
	.bLength =		sizeof mdb_superspeed_in_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_endpoint_descriptor mdb_superspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor mdb_superspeed_out_comp_desc = {
	.bLength =		sizeof mdb_superspeed_out_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_endpoint_descriptor mdb_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mdb_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mdb_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mdb_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

/* data pipe */
static struct usb_endpoint_descriptor mdb_superspeed_data_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor mdb_superspeed_data_in_comp_desc = {
	.bLength =		sizeof mdb_superspeed_data_in_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_endpoint_descriptor mdb_superspeed_data_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor mdb_superspeed_data_out_comp_desc = {
	.bLength =		sizeof mdb_superspeed_data_out_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_ss_ep_comp_descriptor mdb_superspeed_intr_comp_desc = {
	.bLength =		sizeof mdb_superspeed_intr_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};
static struct usb_endpoint_descriptor mdb_highspeed_data_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mdb_highspeed_data_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mdb_fullspeed_data_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mdb_fullspeed_data_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mdb_intr_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize         = __constant_cpu_to_le16(MDB_INTR_BUFFER_SIZE),
	.bInterval              = 6,
};

static struct usb_descriptor_header *fs_mdb_descs[] = {
	(struct usb_descriptor_header *) &mdb_interface_desc,
	(struct usb_descriptor_header *) &mdb_fullspeed_in_desc,
	(struct usb_descriptor_header *) &mdb_fullspeed_out_desc,
	(struct usb_descriptor_header *) &mdb_fullspeed_data_in_desc,
	(struct usb_descriptor_header *) &mdb_fullspeed_data_out_desc,
	(struct usb_descriptor_header *) &mdb_intr_desc,
	NULL,
};

static struct usb_descriptor_header *hs_mdb_descs[] = {
	(struct usb_descriptor_header *) &mdb_interface_desc,
	(struct usb_descriptor_header *) &mdb_highspeed_in_desc,
	(struct usb_descriptor_header *) &mdb_highspeed_out_desc,
	(struct usb_descriptor_header *) &mdb_highspeed_data_in_desc,
	(struct usb_descriptor_header *) &mdb_highspeed_data_out_desc,
	(struct usb_descriptor_header *) &mdb_intr_desc,
	NULL,
};

static struct usb_descriptor_header *ss_mdb_descs[] = {
	(struct usb_descriptor_header *) &mdb_interface_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_in_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_in_comp_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_out_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_out_comp_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_data_in_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_data_in_comp_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_data_out_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_data_out_comp_desc,
	(struct usb_descriptor_header *) &mdb_intr_desc,
	(struct usb_descriptor_header *) &mdb_superspeed_intr_comp_desc,
	NULL,
};

static void mdb_ready_callback(void);
static void mdb_closed_callback(void);

/* temporary variable used between mdb_open() and mdb_gadget_bind() */
static struct mdb_dev *_mdb_dev;

static inline struct mdb_dev *func_to_mdb(struct usb_function *f)
{
	return container_of(f, struct mdb_dev, function);
}


static struct usb_request *mdb_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void mdb_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int mdb_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -EPERM;
	}
}

static inline void mdb_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
void mdb_req_put(struct mdb_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *mdb_req_get(struct mdb_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void mdb_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;

	if (req->status != 0)
		atomic_set(&dev->error, 1);

	mdb_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void mdb_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;

	dev->rx_done = 1;
	if (req->status != 0 && req->status != -ECONNRESET)
		atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);
}

static void mdb_data_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;
	if (req->status != 0)
		atomic_set(&dev->data_status, STATUS_ERROR);

	mdb_req_put(dev, &dev->data_tx_idle, req);

	wake_up(&dev->data_write_wq);
}

static void mdb_data_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;

	dev->data_rx_done = 1;
	if (req->status != 0 && req->status != -ECONNRESET)
		atomic_set(&dev->data_status, STATUS_ERROR);
	wake_up(&dev->data_read_wq);
}

static void mdb_complete_intr(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;

	mdb_req_put(dev, &dev->intr_idle, req);

	wake_up(&dev->intr_wq);
}

static int mdb_create_bulk_endpoints(struct mdb_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc,
				struct usb_endpoint_descriptor *data_in_desc,
				struct usb_endpoint_descriptor *data_out_desc,
				struct usb_endpoint_descriptor *intr_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	pr_debug("--create bulk endpoints--\n");
	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for mdb ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_out = ep;

	/* data pipe */
	ep = usb_ep_autoconfig(cdev->gadget, data_in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for data_ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for data_ep_in got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->data_ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, data_out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for data_ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for mdb data_ep_out got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->data_ep_out = ep;

	ep = usb_ep_autoconfig(cdev->gadget, intr_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_intr failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for mdb ep_intr got %s\n", ep->name);
	ep->driver_data = dev;		/* claim the endpoint */
	dev->ep_intr = ep;

	/* now allocate requests for our endpoints */
	req = mdb_request_new(dev->ep_out, MDB_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = mdb_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = mdb_request_new(dev->ep_in, MDB_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mdb_complete_in;
		mdb_req_put(dev, &dev->tx_idle, req);
	}

	/* data pipe */
	for (i = 0; i < RX_DATA_REQ_MAX; i++) {
		req = mdb_request_new(dev->data_ep_out, MDB_DATA_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mdb_data_complete_out;
		dev->data_rx_req[i] = req;
	}

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = mdb_request_new(dev->data_ep_in, MDB_DATA_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mdb_data_complete_in;
		mdb_req_put(dev, &dev->data_tx_idle, req);
	}

	for (i = 0; i < INTER_REQ_MAX; i++) {
		req = mdb_request_new(dev->ep_intr, MDB_INTR_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mdb_complete_intr;
		mdb_req_put(dev, &dev->intr_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "mdb_bind() could not allocate requests\n");
	return -EPERM;
}

static int mdb_send_event(struct mdb_dev *dev, int event)
{
	struct usb_request *req = NULL;
	int ret;

	DBG(dev->cdev, "mdb_send_event: %d\n", event);

	ret = wait_event_interruptible_timeout(dev->intr_wq,
			(req = mdb_req_get(dev, &dev->intr_idle)),
			msecs_to_jiffies(1000));
	if (!req)
		return -ETIME;

	memcpy(req->buf, &event, sizeof(int));
	req->length = sizeof(int);
	ret = usb_ep_queue(dev->ep_intr, req, GFP_KERNEL);
	if (ret)
		mdb_req_put(dev, &dev->intr_idle, req);

	return ret;
}


/* read from a local file and write to USB */
static void mdb_send_file_work(struct work_struct *data)
{
	struct mdb_dev *dev = container_of(data, struct mdb_dev,
						send_file_work);
	struct usb_request *req = 0;
	struct file *filp;
	loff_t offset;
	int64_t count;
	int xfer, ret;
	int r = 0;

	/* read our parameters */
	smp_rmb();
	filp = dev->xfer_file;
	count = dev->xfer_file_length;
	offset = 0;

	while (count > 0) {
		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->data_write_wq,
			(req = mdb_req_get(dev, &dev->data_tx_idle)) ||
			(atomic_read(&dev->data_status) != STATUS_BUSY));

		if (atomic_read(&dev->data_status) == STATUS_CANCELED) {
			r = -ECANCELED;
			break;
		}

		if (ret < 0) {
			pr_err("send_file_work: wait event error\n");
			r = ret;
			break;
		}
		if (!req) {
			pr_err("send_file_work: alloc req error\n");
			r = ret;
			break;
		}

		if (count > MDB_DATA_BULK_BUFFER_SIZE)
			xfer = MDB_DATA_BULK_BUFFER_SIZE;
		else
			xfer = count;

		ret = vfs_read(filp, req->buf, xfer, &offset);
		if (ret < 0) {
			pr_err("send_file_work: vfs_read error\n");
			r = ret;
			break;
		}
		xfer = ret;

		req->length = xfer;
		ret = usb_ep_queue(dev->data_ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			pr_err("send_file_work: xfer error %d\n", ret);
			r = -EIO;
			break;
		}

		count -= xfer;

		/* zero this so we don't try to free it on error exit */
		req = 0;
	}

	if (req)
		mdb_req_put(dev, &dev->data_tx_idle, req);

	pr_debug("send_file_work: returning %d\n", r);
	/* write the result */
	dev->xfer_result = r;
	smp_wmb();
}

/* read from USB and write to a local file */
static void mdb_receive_file_work(struct work_struct *data)
{
	struct mdb_dev *dev = container_of(data, struct mdb_dev,
						receive_file_work);
	struct usb_request *read_req = NULL, *write_req = NULL;
	struct file *filp;
	loff_t offset;
	int64_t count;
	int ret = 0, cur = 0;
	int r = 0;
	int canceled = 0;

	/* read our parameters */
	smp_rmb();
	filp = dev->xfer_file;
	count = dev->xfer_file_length;
	offset = 0;

	while (count > 0 || write_req) {
		if (count > 0) {
			read_req = dev->data_rx_req[cur];
			cur = (cur + 1)%RX_DATA_REQ_MAX;
			/* queue a request */
			read_req->length = MDB_DATA_BULK_BUFFER_SIZE;
			dev->data_rx_done = 0;
			ret = usb_ep_queue(dev->data_ep_out, read_req, GFP_KERNEL);
			if (ret < 0) {
				pr_err("receive_file_work: ep queue error\n");
				r = -EIO;
				break;
			}
		}

		if (write_req) {
			pr_debug("receive_file_work: rx %p %d\n", write_req, write_req->actual);
			ret = vfs_write(filp, write_req->buf, write_req->actual,
				&offset);
			pr_debug("receive_file_work: vfs_write %d\n", ret);
			if (ret != write_req->actual) {
				pr_err("receive_file_work: vfs_write error\n");
				r = -EIO;
				break;
			}
			write_req = NULL;
		}
		if (read_req) {
wait_event:
			/* wait for our last read to complete */
			ret = wait_event_interruptible(dev->data_read_wq,
				dev->data_rx_done || (atomic_read(&dev->data_status) != STATUS_BUSY));

			if (atomic_read(&dev->data_status) == STATUS_CANCELED) {
				/* clear pipe if needed */
				if ((dev->xfer_file_length - count) < dev->transfered) {
					atomic_set(&dev->data_status, STATUS_BUSY);
					canceled = 1;
					count = dev->transfered - (dev->xfer_file_length - count);
					goto wait_event;
				}
				r = -ECANCELED;
				break;
			}

			if (ret < 0) {
				pr_err("receive_file_work: wait data rx done error\n");
				r = ret;
				if (!dev->data_rx_done)
					usb_ep_dequeue(dev->data_ep_out, read_req);
				break;
			}
			/* if xfer_file_length is 0xFFFFFFFF, then we read until
			 * we get a zero length packet
			 */
			if (count != 0xFFFFFFFF)
				count -= read_req->actual;
			if (read_req->actual < read_req->length) {
				/*
				 * short packet is used to signal EOF for
				 * sizes > 4 gig
				 */
				pr_debug("receive_file_work: got short packet\n");
				count = 0;
			}

			write_req = read_req;
			read_req = NULL;
		}
	}

	if (read_req)
		usb_ep_dequeue(dev->data_ep_out, read_req);
	if (canceled)
		r = -ECANCELED;

	pr_debug("receive_file_work: returning %d\n", r);
	/* write the result */
	dev->xfer_result = r;
	smp_wmb();
}

static long mdb_ioctl(struct file *fp, unsigned code, unsigned long value)
{
	struct mdb_dev *dev = fp->private_data;
	struct file *filp = NULL;
	int ret = -EINVAL;
	int support_data = 1;

	switch (code) {
	case MDB_SEND_FILE:
	case MDB_RECEIVE_FILE:
	{
		struct mdb_file_range	mfr;
		struct work_struct *work;
		if (mdb_lock(&dev->ioctl_excl)) {
			pr_err("mdb_ioctl: device busy\n");
			return -EBUSY;
		}

		if (copy_from_user(&mfr, (void __user *)value, sizeof(mfr))) {
			pr_err("mdb_ioctl: copy data from user error\n");
			mdb_unlock(&dev->ioctl_excl);
			ret = -EFAULT;
			goto out;
		}
		/* hold a reference to the file while we are working with it */
		filp = fget(mfr.fd);
		if (!filp) {
			pr_err("mdb_ioctl: file fd invalid\n");
			mdb_unlock(&dev->ioctl_excl);
			ret = -EBADF;
			goto out;
		}

		if (atomic_read(&dev->data_status) == STATUS_OFFLINE) {
			ret = -ENODEV;
			goto out;
		}
		atomic_set(&dev->data_status, STATUS_BUSY);
		/* write the parameters */
		dev->xfer_file = filp;
		dev->xfer_file_length = mfr.length;
		dev->xfer_result = 0;
		dev->transfered = 0;
		smp_wmb();

		if (code == MDB_SEND_FILE) {
			pr_debug("mdb_ioctl: send request, file_length= %llu\n", dev->xfer_file_length);
			work = &dev->send_file_work;
		} else {
			pr_debug("mdb_ioctl: receive request, file_length= %llu\n", dev->xfer_file_length);
			work = &dev->receive_file_work;
		}

		/* We do the file transfer on a work queue so it will run
		 * in kernel context, which is necessary for vfs_read and
		 * vfs_write to use our buffers in the kernel address space.
		 */
		queue_work(dev->wq, work);
		/* wait for operation to complete */
		flush_workqueue(dev->wq);
		fput(filp);

		/* read the result */
		smp_rmb();
		ret = dev->xfer_result;
		if (atomic_read(&dev->data_status) != STATUS_OFFLINE)
			atomic_set(&dev->data_status, STATUS_READY);
		mdb_unlock(&dev->ioctl_excl);
		break;
	}
	case  MDB_QUERY_DATA_PORT:
		ret = 0;
		if (copy_to_user((void __user *)value, &support_data, 4))
			ret = EFAULT;
		break;
	case  MDB_CANCEL_TASK:
		ret = 0;
		if (copy_from_user(&dev->transfered, (void __user *)value, sizeof(unsigned long))) {
			pr_err("mdb_ioctl: failed to copy data from user");
			ret = -EFAULT;
			goto out;
		}
		atomic_set(&dev->data_status, STATUS_CANCELED);
		wake_up(&dev->data_write_wq);
		wake_up(&dev->data_read_wq);
		pr_debug("mdb_ioctl: cancel task\n");
		break;
	default:
		pr_err("mdb_ioctl case error\n");

	}

out:
	if (ret != 0 && (ret != -ECANCELED) && (code == MDB_SEND_FILE || code == MDB_RECEIVE_FILE)) {
		pr_err("mdb_ioctl: send event, ret=%d\n", ret);
		mdb_send_event(dev, EVENT_MDB_FAIL);
	}

	pr_debug("mdb_ioctl returning %d\n", ret);
	return ret;
}

static ssize_t mdb_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct mdb_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	pr_debug("mdb_read(%d)\n", count);
	if (!_mdb_dev)
		return -ENODEV;

	if (count > MDB_BULK_BUFFER_SIZE)
		return -EINVAL;

	if (mdb_lock(&dev->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(atomic_read(&dev->online) || atomic_read(&dev->error))) {
		pr_debug("mdb_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
			(atomic_read(&dev->online) ||
			atomic_read(&dev->error)));
		if (ret < 0) {
			mdb_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (atomic_read(&dev->error)) {
		r = -EIO;
		goto done;
	}

requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = MDB_BULK_BUFFER_SIZE;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		pr_debug("mdb_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		atomic_set(&dev->error, 1);
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done ||
				atomic_read(&dev->error));
	if (ret < 0) {
		if (ret != -ERESTARTSYS)
		atomic_set(&dev->error, 1);
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (!atomic_read(&dev->error)) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;

	} else
		r = -EIO;

done:
	if (atomic_read(&dev->error))
		wake_up(&dev->write_wq);

	mdb_unlock(&dev->read_excl);
	pr_debug("mdb_read returning %d\n", r);
	return r;
}

static ssize_t mdb_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct mdb_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	if (!_mdb_dev)
		return -ENODEV;
	pr_debug("mdb_write(%d)\n", count);

	if (mdb_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		if (atomic_read(&dev->error)) {
			pr_debug("mdb_write dev->error\n");
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = mdb_req_get(dev, &dev->tx_idle)) ||
			 atomic_read(&dev->error)));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > MDB_BULK_BUFFER_SIZE)
				xfer = MDB_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("mdb_write: xfer error %d\n", ret);
				atomic_set(&dev->error, 1);
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
		mdb_req_put(dev, &dev->tx_idle, req);

	if (atomic_read(&dev->error))
		wake_up(&dev->read_wq);

	mdb_unlock(&dev->write_excl);
	pr_debug("mdb_write returning %d\n", r);
	return r;
}


static int mdb_open(struct inode *ip, struct file *fp)
{
	static DEFINE_RATELIMIT_STATE(rl, 10*HZ, 1);

	pr_debug("--mdb open--\n");
	if (__ratelimit(&rl))
	    pr_info("mdb_open\n");
	if (!_mdb_dev)
		return -ENODEV;

	if (mdb_lock(&_mdb_dev->open_excl))
		return -EBUSY;

	fp->private_data = _mdb_dev;

	/* clear the error latch */
	atomic_set(&_mdb_dev->error, 0);
	atomic_set(&_mdb_dev->data_status, STATUS_READY);
	if (_mdb_dev->close_notified) {
		_mdb_dev->close_notified = false;
		mdb_ready_callback();
	}

	_mdb_dev->notify_close = true;
	return 0;
}

static int mdb_release(struct inode *ip, struct file *fp)
{
	static DEFINE_RATELIMIT_STATE(rl, 10*HZ, 1);

	pr_debug("--mdb release--\n");
	if (__ratelimit(&rl))
	    pr_info("mdb_release\n");

	/*
	 * MDB daemon closes the device file after I/O error.  The
	 * I/O error happen when Rx requests are flushed during
	 * cable disconnect or bus reset in configured state.  Disabling
	 * USB configuration and pull-up during these scenarios are
	 * undesired.  We want to force bus reset only for certain
	 * commands like "mdb root" and "mdb usb".
	 */
	if (_mdb_dev->notify_close) {
		mdb_closed_callback();
		_mdb_dev->close_notified = true;
	}

	mdb_unlock(&_mdb_dev->open_excl);
	return 0;
}

/* file operations for MDB device /dev/android_mdb */
static const struct file_operations mdb_fops = {
	.owner = THIS_MODULE,
	.read = mdb_read,
	.write = mdb_write,
	.unlocked_ioctl = mdb_ioctl,
	.open = mdb_open,
	.release = mdb_release,
};

static struct miscdevice mdb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = mdb_shortname,
	.fops = &mdb_fops,
};




static int
mdb_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct mdb_dev	*dev = func_to_mdb(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "mdb_function_bind dev: %p\n", dev);

	pr_debug("--mdb function bind--\n");
	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	mdb_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = mdb_create_bulk_endpoints(dev, &mdb_fullspeed_in_desc,
			&mdb_fullspeed_out_desc,
			&mdb_fullspeed_data_in_desc,
			&mdb_fullspeed_data_out_desc,
			&mdb_intr_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		pr_debug("-- is dualspeed --\n");
		mdb_highspeed_in_desc.bEndpointAddress =
			mdb_fullspeed_in_desc.bEndpointAddress;
		mdb_highspeed_out_desc.bEndpointAddress =
			mdb_fullspeed_out_desc.bEndpointAddress;
		mdb_highspeed_data_in_desc.bEndpointAddress =
			mdb_fullspeed_data_in_desc.bEndpointAddress;
		mdb_highspeed_data_out_desc.bEndpointAddress =
			mdb_fullspeed_data_out_desc.bEndpointAddress;
	}

	/* support super speed hardware */
	if (gadget_is_superspeed(c->cdev->gadget)) {
		pr_debug("--is superspeed--\n");
		mdb_superspeed_in_desc.bEndpointAddress =
			mdb_fullspeed_in_desc.bEndpointAddress;
		mdb_superspeed_out_desc.bEndpointAddress =
			mdb_fullspeed_out_desc.bEndpointAddress;
		mdb_superspeed_data_in_desc.bEndpointAddress =
			mdb_fullspeed_data_in_desc.bEndpointAddress;
		mdb_superspeed_data_out_desc.bEndpointAddress =
			mdb_fullspeed_data_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
mdb_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct mdb_dev	*dev = func_to_mdb(f);
	struct usb_request *req;
	int i = 0;

	pr_debug("--mdb function unbind--\n");
	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);
	atomic_set(&dev->data_status, STATUS_OFFLINE);

	wake_up(&dev->read_wq);

	mdb_request_free(dev->rx_req, dev->ep_out);
	while ((req = mdb_req_get(dev, &dev->tx_idle)))
		mdb_request_free(req, dev->ep_in);

	/* data pipe */
	while ((req = mdb_req_get(dev, &dev->data_tx_idle)))
		mdb_request_free(req, dev->data_ep_in);
	for (i = 0; i < RX_DATA_REQ_MAX; ++i)
		mdb_request_free(dev->data_rx_req[i], dev->data_ep_out);
	while ((req = mdb_req_get(dev, &dev->intr_idle)))
		mdb_request_free(req, dev->ep_intr);
}

static int mdb_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct mdb_dev	*dev = func_to_mdb(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "mdb_function_set_alt intf: %d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret) {
		dev->ep_in->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
				dev->ep_in->name, ret);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_in);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
			dev->ep_in->name, ret);
		return ret;
	}

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret) {
		dev->ep_out->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
			dev->ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
				dev->ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	/* data pipe */
	ret = config_ep_by_speed(cdev->gadget, f, dev->data_ep_in);
	if (ret) {
		dev->data_ep_in->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
				dev->data_ep_in->name, ret);
		usb_ep_disable(dev->ep_in);
		usb_ep_disable(dev->ep_out);
		return ret;
	}
	ret = usb_ep_enable(dev->data_ep_in);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
			dev->data_ep_in->name, ret);
		usb_ep_disable(dev->ep_in);
		usb_ep_disable(dev->ep_out);
		return ret;
	}

	ret = config_ep_by_speed(cdev->gadget, f, dev->data_ep_out);
	if (ret) {
		dev->data_ep_out->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
			dev->data_ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->data_ep_in);
		return ret;
	}
	ret = usb_ep_enable(dev->data_ep_out);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
				dev->data_ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->data_ep_in);
		return ret;
	}
	dev->ep_intr->desc = &mdb_intr_desc;
	ret = usb_ep_enable(dev->ep_intr);
	if (ret) {
		usb_ep_disable(dev->ep_out);
		usb_ep_disable(dev->ep_in);
		usb_ep_disable(dev->data_ep_out);
		usb_ep_disable(dev->data_ep_in);
		return ret;
	}
	atomic_set(&dev->online, 1);
	atomic_set(&dev->data_status, STATUS_READY);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void mdb_function_disable(struct usb_function *f)
{
	struct mdb_dev	*dev = func_to_mdb(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	DBG(cdev, "mdb_function_disable cdev %p\n", cdev);
	/*
	 * Bus reset happened or cable disconnected.  No
	 * need to disable the configuration now.  We will
	 * set noify_close to true when device file is re-opened.
	 */
	dev->notify_close = false;
	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	atomic_set(&dev->data_status, STATUS_OFFLINE);
	usb_ep_disable(dev->data_ep_in);
	usb_ep_disable(dev->data_ep_out);
	usb_ep_disable(dev->ep_intr);
	wake_up(&dev->data_read_wq);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int mdb_bind_config(struct usb_configuration *c)
{
	struct mdb_dev *dev = _mdb_dev;

	pr_debug("--mdb_bind_config--\n");

	dev->cdev = c->cdev;
	dev->function.name = "mdb";
	dev->function.descriptors = fs_mdb_descs;
	dev->function.hs_descriptors = hs_mdb_descs;

	if (gadget_is_superspeed(c->cdev->gadget)) {
		dev->function.ss_descriptors = ss_mdb_descs;
	}

	dev->function.bind = mdb_function_bind;
	dev->function.unbind = mdb_function_unbind;
	dev->function.set_alt = mdb_function_set_alt;
	dev->function.disable = mdb_function_disable;

	return usb_add_function(c, &dev->function);
}

static int mdb_setup(void)
{
	struct mdb_dev *dev;
	int ret;

	pr_debug("--mdb_setup--\n");
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	init_waitqueue_head(&dev->data_read_wq);
	init_waitqueue_head(&dev->data_write_wq);
	init_waitqueue_head(&dev->intr_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);
	atomic_set(&dev->ioctl_excl, 0);

	/* config is disabled by default if mdb is present. */
	dev->close_notified = true;

	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->data_tx_idle);
	INIT_LIST_HEAD(&dev->intr_idle);

	dev->wq = create_singlethread_workqueue("f_mdb_data");
	if (!dev->wq) {
		ret = -ENOMEM;
		goto err;
	}
	INIT_WORK(&dev->send_file_work, mdb_send_file_work);
	INIT_WORK(&dev->receive_file_work, mdb_receive_file_work);

	_mdb_dev = dev;

	ret = misc_register(&mdb_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	printk(KERN_ERR "mdb gadget driver failed to initialize\n");
	return ret;
}

static void mdb_cleanup(void)
{
	misc_deregister(&mdb_device);
	destroy_workqueue(_mdb_dev->wq);

	pr_debug("--mdb_clean--\n");
	kfree(_mdb_dev);
	_mdb_dev = NULL;
}

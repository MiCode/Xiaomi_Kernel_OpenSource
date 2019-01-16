/*
 * Gadget Driver for Android ADB
 *
 * Copyright (C) 2008 Google, Inc.
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
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

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
#include <linux/ratelimit.h>

#define ADB_BULK_BUFFER_SIZE           4096

/* number of tx requests to allocate */
#define TX_REQ_MAX 4

static const char adb_shortname[] = "android_adb";

struct adb_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int online;
	int error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
};

/* __ADB_DEBUG__ start */
struct usb_ep *ep_in = NULL;
struct usb_ep *ep_out = NULL;
int bitdebug_enabled;
unsigned bitdebug_writeCnt = 1;
unsigned bitdebug_readCnt = 0;

struct amessage {
	unsigned command;	/* command identifier constant      */
	unsigned arg0;		/* first argument                   */
	unsigned arg1;		/* second argument                  */
	unsigned data_length;	/* length of payload (0 is allowed) */
	unsigned data_check;	/* checksum of data payload         */
	unsigned magic;		/* command ^ 0xffffffff             */
};

struct debuginfo {
	unsigned headtoken;
	unsigned command;	/* command identifier constant      */
	unsigned msg_check;
	unsigned data_check;
	unsigned count;
	unsigned dummy;
	unsigned tailtoken;
};


typedef struct amessage amessage;
typedef struct debuginfo debuginfo;

#define A_SYNC 0x434e5953
#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257
#define A_AUTH 0x48545541
#define A_DBUG 0x41424a42

#define DBGHEADTOKEN 0x13579bdf
#define DBGTAILTOKEN 0xdca86420

/* __ADB_DEBUG__ end */

static struct usb_interface_descriptor adb_interface_desc = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = 0xFF,
	.bInterfaceSubClass = 0x42,
	.bInterfaceProtocol = 1,
};

static struct usb_endpoint_descriptor adb_superspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor adb_superspeed_in_comp_desc = {
	.bLength = sizeof(adb_superspeed_in_comp_desc),
	.bDescriptorType = USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
};

static struct usb_endpoint_descriptor adb_superspeed_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor adb_superspeed_out_comp_desc = {
	.bLength = sizeof(adb_superspeed_out_comp_desc),
	.bDescriptorType = USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =         0, */
	/* .bmAttributes =      0, */
};

static struct usb_endpoint_descriptor adb_highspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor adb_highspeed_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor adb_fullspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor adb_fullspeed_out_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_OUT,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_adb_descs[] = {
	(struct usb_descriptor_header *)&adb_interface_desc,
	(struct usb_descriptor_header *)&adb_fullspeed_in_desc,
	(struct usb_descriptor_header *)&adb_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_adb_descs[] = {
	(struct usb_descriptor_header *)&adb_interface_desc,
	(struct usb_descriptor_header *)&adb_highspeed_in_desc,
	(struct usb_descriptor_header *)&adb_highspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *ss_adb_descs[] = {
	(struct usb_descriptor_header *)&adb_interface_desc,
	(struct usb_descriptor_header *)&adb_superspeed_in_desc,
	(struct usb_descriptor_header *)&adb_superspeed_in_comp_desc,
	(struct usb_descriptor_header *)&adb_superspeed_out_desc,
	(struct usb_descriptor_header *)&adb_superspeed_out_comp_desc,
	NULL,
};

static void adb_debug_read_copy_from_user(char __user *buf, struct usb_request *req)
{
	if (sizeof(debuginfo) == req->length) {
		unsigned long ret;
		ret = copy_from_user(req->buf, buf, req->length);
		if (ret != 0) {
			pr_err("copy_from_user fail\n");
		}
	}
}

static void adb_debug_read_copy_to_user(char __user *buf, struct usb_request *req)
{
	debuginfo *dbg = (debuginfo *) req->buf;

	if (dbg != NULL && dbg->command == A_DBUG && dbg->headtoken == DBGHEADTOKEN
	    && dbg->tailtoken == DBGTAILTOKEN) {
		unsigned long ret;
		ret = copy_to_user(buf, req->buf, req->length);
		if (ret != 0) {
			pr_err("copy_to_user fail\n");
		}
		printk(KERN_INFO "adb_read A_DBUG (0x%x) (0x%x) (0x%x)\n", dbg->command,
		       dbg->msg_check, dbg->data_check);
	}
}

static void adb_ready_callback(void);
static void adb_closed_callback(void);

/* temporary variable used between adb_open() and adb_gadget_bind() */
static struct adb_dev *_adb_dev;

static inline struct adb_dev *func_to_adb(struct usb_function *f)
{
	return container_of(f, struct adb_dev, function);
}


static struct usb_request *adb_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	/* now allocate buffers for the requests */
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		pr_err("%s %s %d: kmalloc failed\n", __FILE__, __func__, __LINE__);
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void adb_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int adb_lock(atomic_t *excl)
{
	/*printk(KERN_DEBUG "%s %s %d: excl: %d\n", __FILE__, __func__, __LINE__, atomic_read(excl)); */
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void adb_unlock(atomic_t *excl)
{
	/*printk(KERN_DEBUG "%s %s %d: excl: %d\n", __FILE__, __func__, __LINE__, atomic_read(excl)); */
	atomic_dec(excl);
}

/* add a request to the tail of a list */
void adb_req_put(struct adb_dev *dev, struct list_head *head, struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *adb_req_get(struct adb_dev *dev, struct list_head *head)
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

static void adb_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct adb_dev *dev = _adb_dev;

	if (req->status != 0)
		dev->error = 1;

	adb_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void adb_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct adb_dev *dev = _adb_dev;

	dev->rx_done = 1;
	if (req->status != 0 && req->status != -ECONNRESET)
		dev->error = 1;

	wake_up(&dev->read_wq);
}

static int adb_create_bulk_endpoints(struct adb_dev *dev,
				     struct usb_endpoint_descriptor *in_desc,
				     struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "%s %s %d: create_bulk_endpoints dev: %p\n", __FILE__, __func__, __LINE__, dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	/* __ADB_DEBUG__ start */
	ep_in = ep;
	/* __ADB_DEBUG__ end */
	if (!ep) {
		DBG(cdev, "%s %s %d: usb_ep_autoconfig for ep_in failed\n", __FILE__, __func__,
		    __LINE__);
		return -ENODEV;
	}
	DBG(cdev, "%s %s %d: usb_ep_autoconfig for ep_in got %s\n", __FILE__, __func__, __LINE__,
	    ep->name);
	ep->driver_data = dev;	/* claim the endpoint */
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	/* __ADB_DEBUG__ start */
	ep_out = ep;
	/* __ADB_DEBUG__ end */
	if (!ep) {
		DBG(cdev, "%s %s %d: usb_ep_autoconfig for ep_out failed\n", __FILE__, __func__,
		    __LINE__);
		return -ENODEV;
	}
	DBG(cdev, "%s %s %d: usb_ep_autoconfig for adb ep_out got %s\n", __FILE__, __func__,
	    __LINE__, ep->name);
	ep->driver_data = dev;	/* claim the endpoint */
	dev->ep_out = ep;

	/* now allocate requests for our endpoints */
	req = adb_request_new(dev->ep_out, ADB_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = adb_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = adb_request_new(dev->ep_in, ADB_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = adb_complete_in;
		adb_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

 fail:
	printk(KERN_ERR "%s %s %d: adb_bind() could not allocate requests\n", __FILE__, __func__,
	       __LINE__);
	return -1;
}

static ssize_t adb_read(struct file *fp, char __user *buf, size_t count, loff_t *pos)
{
	struct adb_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int ret;
	static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 10);

	pr_debug("%s %s %d: (%d)\n", __FILE__, __func__, __LINE__, (int)count);
	if (!_adb_dev)
		return -ENODEV;

	if (count > ADB_BULK_BUFFER_SIZE) {
		pr_err("%s %s %d: count > ADB_BULK_BUFFER_SIZE\n", __FILE__, __func__, __LINE__);
		return -EINVAL;
	}


	if (adb_lock(&dev->read_excl)) {
		if (__ratelimit(&ratelimit)) {
			pr_err("%s %s %d: Failed due to lock busy\n", __FILE__, __func__, __LINE__);
		}
		return -EBUSY;
	}

	/* we will block until we're online */
	while (!(dev->online || dev->error)) {
		pr_debug("%s %s %d: waiting for online state\n", __FILE__, __func__, __LINE__);
		ret = wait_event_interruptible(dev->read_wq, (dev->online || dev->error));
		if (ret < 0) {
			adb_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (dev->error) {
		r = -EIO;
		goto done;
	}

 requeue_req:
	/* queue a request */
	req = dev->rx_req;
	req->length = count;
	dev->rx_done = 0;

	/*
	 * The MAX_PAYLOAD of adb is 4096bytes defined in system/core/adb/adb.h
	 * So when meet the request has to read 4096bytes long payload,
	 * set short_not_ok is 1. Use musb dma mode 1 to speed up the write
	 * throughput.
	 */
	if (count == 4096)
		req->short_not_ok = 1;
	else
		req->short_not_ok = 0;

	if (bitdebug_enabled == 1) {
		adb_debug_read_copy_from_user(buf, req);
	}

	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		/* FIXME */
		/* Process adbd would try to reconnect when usb has been reset. */
		/* It should not send data after endpoint has shutdown. */
		/* It is a workaround to reduce adb retry times. */
		if (ret == -ESHUTDOWN) {
			msleep(150);
		}

		if (bitdebug_enabled == 1) {
			if (ret == -EINPROGRESS) {
				adb_debug_read_copy_to_user(buf, req);
				goto done;
			}
		}

		pr_err("%s %s %d: failed to queue req %p (%d)\n",
		       __FILE__, __func__, __LINE__, req, ret);
		r = -EIO;
		dev->error = 1;
		goto done;
	} else {
		pr_debug("%s %s %d: rx %p queue\n", __FILE__, __func__, __LINE__, req);
	}

	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		if (ret != -ERESTARTSYS)
			dev->error = 1;
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (!dev->error) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("%s %s %d: rx %p %d\n", __FILE__, __func__, __LINE__, req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;

		/* __ADB_DEBUG__ start */
		if (bitdebug_enabled == 1) {
			if (count == sizeof(amessage)) {
				amessage *msg = (amessage *) req->buf;
				if (msg != NULL) {
					switch (msg->command) {
					case A_SYNC:
					case A_CNXN:
					case A_OPEN:
					case A_OKAY:
					case A_CLSE:
					case A_WRTE:
					case A_AUTH:
						/* printk(KERN_INFO "adb: adb_read (0x%x) (0x%x) (0x%x) (0x%x) (0x%x) (0x%x)\n", msg->command, msg->arg0, msg->arg1, */
						/* msg->data_length, msg->data_check, msg->magic); */
						break;
					default:
						/* printk(KERN_INFO "adb_read msg A_DATA\n"); */
						break;
					}
				}
			}
		}
		/* __ADB_DEBUG__ end */

		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;

	} else
		r = -EIO;

 done:
	adb_unlock(&dev->read_excl);
	pr_debug("%s %s %d: returning %d\n", __FILE__, __func__, __LINE__, r);

	return r;
}

static ssize_t adb_write(struct file *fp, const char __user *buf, size_t count, loff_t *pos)
{
	struct adb_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;
	static int flow_state;
	bool data;

	if (!_adb_dev)
		return -ENODEV;
	pr_debug("%s %s %d:(%d)\n", __FILE__, __func__, __LINE__, (int)count);

	if (adb_lock(&dev->write_excl)) {
		return -EBUSY;
	}

	while (count > 0) {
		if (dev->error) {
			pr_debug("%s %s %d: dev->error\n", __FILE__, __func__, __LINE__);
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
					       (req = adb_req_get(dev, &dev->tx_idle))
					       || dev->error);

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > ADB_BULK_BUFFER_SIZE)
				xfer = ADB_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}
			/* __ADB_DEBUG__ start */
			data = true;
			if (bitdebug_enabled == 1) {
				if (count == sizeof(amessage)) {
					amessage *msg = (amessage *) req->buf;
					if (msg != NULL) {
						switch (msg->command) {
						case A_SYNC:
						case A_CNXN:
						case A_OPEN:
						case A_OKAY:
						case A_CLSE:
						case A_WRTE:
						case A_AUTH:
							/* printk(KERN_INFO "adb_write msg (0x%x) (0x%x) (0x%x) (0x%x) (0x%x) (0x%x)\n", msg->command, msg->arg0, msg->arg1, */
							/* msg->data_length, msg->data_check, msg->magic); */
							if (flow_state == 0) {
								flow_state = 1;
								/* no data packet */
								if (msg->data_length == 0) {
									flow_state = 2;
								}
							} else {
								pr_debug
								    ("adb_write flow state msg warning\n");
								pr_debug
								    ("adb_write msg (0x%x) (0x%x) (0x%x) (0x%x) (0x%x) (0x%x)\n",
								     msg->command, msg->arg0,
								     msg->arg1, msg->data_length,
								     msg->data_check, msg->magic);
							}
							data = false;
							break;
						}
					}
				} else {
					data = true;
				}

				if (count == sizeof(debuginfo)) {
					debuginfo *dbg = (debuginfo *) req->buf;
					if (dbg != NULL && dbg->command == A_DBUG
					    && dbg->headtoken == DBGHEADTOKEN
					    && dbg->tailtoken == DBGTAILTOKEN) {
						/* printk(KERN_INFO "adb_write dbg (0x%x) (0x%x) (0x%x)\n", dbg->command, dbg->msg_check, dbg->data_check); */
						if (flow_state == 2) {
							flow_state = 0;
						} else {
							pr_debug
							    ("adb_write flow state debug warning\n");
							pr_debug
							    ("adb_write dbg (0x%x) (0x%x) (0x%x)\n",
							     dbg->command, dbg->msg_check,
							     dbg->data_check);
						}
						data = false;
						if (dbg->count == -1) {
							bitdebug_enabled = 0;
							bitdebug_writeCnt = 1;
							bitdebug_readCnt = 0;
							msleep(150);
							break;
						}
					}
				}

				if (data == true && bitdebug_enabled == 1) {
					if (flow_state == 1) {
						flow_state = 2;
					} else {
						pr_debug("adb_write flow state data warning\n");
					}
					/* printk(KERN_INFO "adb_write data\n"); */
				}
			} else {
				if (count == sizeof(debuginfo)) {
					debuginfo *dbg = (debuginfo *) req->buf;
					if (dbg != NULL && dbg->command == A_DBUG
					    && dbg->headtoken == DBGHEADTOKEN
					    && dbg->tailtoken == DBGTAILTOKEN) {
						if (dbg->count == 0) {
							bitdebug_enabled = 1;
							flow_state = 0;
							/* req->length = sizeof(debuginfo); */
							msleep(150);
							break;
						}
					}
				}
			}
			/* __ADB_DEBUG__ end */

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("%s %s %d: xfer error %d\n", __FILE__, __func__, __LINE__,
					 ret);
				dev->error = 1;
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
		adb_req_put(dev, &dev->tx_idle, req);

	adb_unlock(&dev->write_excl);
	pr_debug("%s %s %d: returning %d\n", __FILE__, __func__, __LINE__, r);

	return r;
}

static int adb_open(struct inode *ip, struct file *fp);
static int adb_release(struct inode *ip, struct file *fp);

/* file operations for ADB device /dev/android_adb */
static struct file_operations adb_fops = {
	.owner = THIS_MODULE,
	.read = adb_read,
	.write = adb_write,
	.open = adb_open,
	.release = adb_release,
};

static struct miscdevice adb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = adb_shortname,
	.fops = &adb_fops,
};

static int open_release_pair;

static spinlock_t open_lock;

static int adb_open(struct inode *ip, struct file *fp)
{
	int ret = 0;
	unsigned long flags;
	bitdebug_enabled = 0;

	spin_lock_irqsave(&open_lock, flags);

	pr_debug("[adb]adb_open start, adb_open: %p check adb_release %p, open_release_pair: %d\n",
		 adb_fops.open, adb_fops.release, open_release_pair);

	if (!_adb_dev) {
		pr_err("[adb]adb_open _adb_dev is NULL, open_release_pair: %d\n",
		       open_release_pair);
		open_release_pair = 0;
		ret = ENODEV;
		goto OPEN_END;
	}

	/*Workaround for being unable to call adb_release from adbd */
	if (open_release_pair > 0) {
		pr_debug("[XLOG_WARN][adb]open twice, %s %s %d: open_release_pair count: %d\n",
			 __FILE__, __func__, __LINE__, open_release_pair);
		ret = 0;
		fp->private_data = _adb_dev;
		goto OPEN_END;
	}

	open_release_pair++;
	fp->private_data = _adb_dev;
	/* clear the error latch */
	_adb_dev->error = 0;

	adb_ready_callback();

 OPEN_END:
	pr_debug("[adb]adb_open end, open_release_pair: %d", open_release_pair);

	spin_unlock_irqrestore(&open_lock, flags);

	return ret;
}

static int adb_release(struct inode *ip, struct file *fp)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&open_lock, flags);

	pr_debug
	    ("[adb]adb_release start, adb_open: %p check adb_release %p, open_release_pair: %d\n",
	     adb_fops.open, adb_fops.release, open_release_pair);

	if (open_release_pair < 1) {
		pr_debug
		    ("[XLOG_WARN][adb][adb] close an unopened device, %s %s %d: open_release_pair count: %d\n",
		     __FILE__, __func__, __LINE__, open_release_pair);
		ret = -1;
		goto RELEASE_END;
	}

	adb_closed_callback();

	open_release_pair--;

 RELEASE_END:

	pr_debug("[adb]adb_release end, open_release_pair: %d", open_release_pair);

	spin_unlock_irqrestore(&open_lock, flags);

	return ret;
}

static int adb_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct adb_dev *dev = func_to_adb(f);
	int id;
	int ret;

	dev->cdev = cdev;
	DBG(cdev, "%s %s %d: dev: %p\n", __FILE__, __func__, __LINE__, dev);

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	adb_interface_desc.bInterfaceNumber = id;

	/* allocate endpoints */
	ret = adb_create_bulk_endpoints(dev, &adb_fullspeed_in_desc, &adb_fullspeed_out_desc);
	if (ret)
		return ret;

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		adb_highspeed_in_desc.bEndpointAddress = adb_fullspeed_in_desc.bEndpointAddress;
		adb_highspeed_out_desc.bEndpointAddress = adb_fullspeed_out_desc.bEndpointAddress;
	}
	/* support super speed hardware */
	if (gadget_is_superspeed(c->cdev->gadget)) {
		adb_superspeed_in_desc.bEndpointAddress = adb_fullspeed_in_desc.bEndpointAddress;
		adb_superspeed_out_desc.bEndpointAddress = adb_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s %s %d: %s speed %s: IN/%s, OUT/%s\n", __FILE__, __func__, __LINE__,
	    gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
	    f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void adb_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct adb_dev *dev = func_to_adb(f);
	struct usb_request *req;


	dev->online = 0;
	dev->error = 1;

	wake_up(&dev->read_wq);

	adb_request_free(dev->rx_req, dev->ep_out);
	while ((req = adb_req_get(dev, &dev->tx_idle)))
		adb_request_free(req, dev->ep_in);
}

static int adb_function_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct adb_dev *dev = func_to_adb(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "adb_function_set_alt intf: %d alt: %d\n", intf, alt);

#ifdef CONFIG_USBIF_COMPLIANCE
	if (dev->online){
		return 0 ;
	}
#endif

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret)
		return ret;

	ret = usb_ep_enable(dev->ep_in);
	if (ret)
		return ret;

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret) {
		pr_err("%s %s %d: usb_ep_enable in failed\n", __FILE__, __func__, __LINE__);
		return ret;
	}

	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		usb_ep_disable(dev->ep_in);
		pr_err("%s %s %d: usb_ep_enable out failed\n", __FILE__, __func__, __LINE__);
		return ret;
	}
	dev->online = 1;

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void adb_function_disable(struct usb_function *f)
{
	struct adb_dev *dev = func_to_adb(f);
	struct usb_composite_dev *cdev = dev->cdev;

	DBG(cdev, "%s %s %d: cdev %p\n", __FILE__, __func__, __LINE__, cdev);
	dev->online = 0;
	dev->error = 1;
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(cdev, "%s %s %d: %s disabled\n", __FILE__, __func__, __LINE__, dev->function.name);
}

static int adb_bind_config(struct usb_configuration *c)
{
	struct adb_dev *dev = _adb_dev;

	printk(KERN_INFO "%s %s %d\n", __FILE__, __func__, __LINE__);

	dev->cdev = c->cdev;
	dev->function.name = "adb";
	dev->function.fs_descriptors = fs_adb_descs;
	dev->function.hs_descriptors = hs_adb_descs;
	if (gadget_is_superspeed(c->cdev->gadget)) {
		pr_debug("[ADB] SS ADB DESCS!!\n");
		dev->function.ss_descriptors = ss_adb_descs;
	}
	dev->function.bind = adb_function_bind;
	dev->function.unbind = adb_function_unbind;
	dev->function.set_alt = adb_function_set_alt;
	dev->function.disable = adb_function_disable;

	return usb_add_function(c, &dev->function);
}

static int adb_setup(void)
{
	struct adb_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	spin_lock_init(&open_lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	INIT_LIST_HEAD(&dev->tx_idle);

	_adb_dev = dev;

	ret = misc_register(&adb_device);
	if (ret)
		goto err;

	return 0;

 err:
	kfree(dev);
	printk(KERN_ERR "%s %s %d: adb gadget driver failed to initialize\n", __FILE__, __func__,
	       __LINE__);
	return ret;
}

static void adb_cleanup(void)
{
	misc_deregister(&adb_device);

	kfree(_adb_dev);
	_adb_dev = NULL;
}

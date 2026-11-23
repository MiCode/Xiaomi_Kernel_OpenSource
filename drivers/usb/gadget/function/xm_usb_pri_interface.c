
// SPDX-License-Identifier: GPL-2.0
/*
 * Gadget Function Driver for Xiaomi ScreenShare Interface
 *
 * Copyright 2026-2030 Xiaomi Technology Co., Ltd.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/kref.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/hid.h>
#include <linux/hiddev.h>
#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/android_accessory.h>
#include <uapi/linux/usb/android_accessory.h>
#include <linux/configfs.h>
#include <linux/usb/composite.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
#include <linux/bug.h>
#define MAX_INST_NAME_LEN 40
#define BULK_BUFFER_SIZE 16384
#define PROTOCOL_VERSION 2
/* String IDs */
#define INTERFACE_STRING_INDEX 0
/* number of tx and rx requests to allocate */
#define TX_REQ_MAX 4
#define RX_REQ_MAX 2
#define XIAOMI_SEND_STRING		88
struct xm_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;
	struct kref kref;
	struct usb_ep *ep_in;
	struct usb_ep *ep_out;
	/*
	 * online indicates state of function_set_alt & function_unbind
	 * set to true when we connect
	 */
	bool online;
	/*
	 * disconnected indicates state of open & release
	 * Set to true when we disconnect.
	 */
	bool disconnected;
	/* strings sent by the host */
	char manufacturer[ACC_STRING_SIZE];
	char model[ACC_STRING_SIZE];
	char description[ACC_STRING_SIZE];
	char version[ACC_STRING_SIZE];
	char uri[ACC_STRING_SIZE];
	char serial[ACC_STRING_SIZE];
	/* for xm_complete_set_string */
	int string_index;
	/* set to 1 if we have a pending start request */
	int start_requested;
	int audio_mode;
	struct list_head tx_idle;
	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req[RX_REQ_MAX];
	int rx_done;
	/* delayed work for handling ACCESSORY_START */
	struct delayed_work start_work;
	/* work for handling ACCESSORY GET PROTOCOL */
	struct work_struct getprotocol_work;
	/* work for handling ACCESSORY SEND STRING */
	struct work_struct sendstring_work;
	/* worker for registering and unregistering hid devices */
	struct work_struct hid_work;
	/* list of active HID devices */
	struct list_head hid_list;
	/* list of new HID devices to register */
	struct list_head new_hid_list;
	/* list of dead HID devices to unregister */
	struct list_head dead_hid_list;
};
static struct usb_interface_descriptor xm_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 0,
	.bInterfaceClass        = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass     = USB_SUBCLASS_VENDOR_SPEC,
	.bInterfaceProtocol     = 0,
};
static struct usb_descriptor_header *fs_xm_descs[] = {
	(struct usb_descriptor_header *) &xm_interface_desc,
	NULL,
};
static struct usb_descriptor_header *hs_xm_descs[] = {
	(struct usb_descriptor_header *) &xm_interface_desc,
	NULL,
};
static struct usb_descriptor_header *ss_xm_descs[] = {
	(struct usb_descriptor_header *) &xm_interface_desc,
	NULL,
};
static struct usb_descriptor_header *ssp_xm_descs[] = {
	(struct usb_descriptor_header *) &xm_interface_desc,
	NULL,
};
static struct usb_string xm_string_defs[] = {
	[INTERFACE_STRING_INDEX].s = "Xiaomi usb interface",
	{  }, /* end of list */
};
static struct usb_gadget_strings xm_string_table = {
	.language		= 0x0409, /* en-US */
	.strings		= xm_string_defs,
};
static struct usb_gadget_strings *xm_strings[] = {
	&xm_string_table,
	NULL,
};
static DEFINE_SPINLOCK(xm_dev_instance_lock);
static struct xm_dev *xm_dev_instance;
struct xm_instance {
	struct usb_function_instance func_inst;
	const char *name;
};
static struct xm_dev *get_xm_dev(void)
{
	unsigned long flags;
	spin_lock_irqsave(&xm_dev_instance_lock, flags);
	if (xm_dev_instance)
		kref_get(&xm_dev_instance->kref);
	spin_unlock_irqrestore(&xm_dev_instance_lock, flags);
	return xm_dev_instance;
}
static void __xm_dev_instance_release(struct kref *kref)
{
	struct xm_dev *dev = container_of(kref, struct xm_dev, kref);
	xm_dev_instance = NULL;
	/* Cancel any async work */
	cancel_delayed_work_sync(&dev->start_work);
	cancel_work_sync(&dev->getprotocol_work);
	cancel_work_sync(&dev->sendstring_work);
	cancel_work_sync(&dev->hid_work);
	kfree(dev);
}
static bool refcount_dec_not_one_local(refcount_t *r)
{
	unsigned int new, val = atomic_read(&r->refs);
	do {
		if (unlikely(val == REFCOUNT_SATURATED))
			return true;
		if (val == 1)
			return false;
		new = val - 1;
		if (new > val) {
			WARN_ONCE(new > val, "refcount_t: underflow; use-after-free.\n");
			return true;
		}
	} while (!atomic_try_cmpxchg_release(&r->refs, &val, new));
	return true;
}
static bool refcount_dec_and_lock_irqsave_local(refcount_t *r, spinlock_t *lock,
				   unsigned long *flags)
{
	if (refcount_dec_not_one_local(r))
		return false;
	spin_lock_irqsave(lock, *flags);
	if (!refcount_dec_and_test(r)) {
		spin_unlock_irqrestore(lock, *flags);
		return false;
	}

	return true;
}
static void put_xm_dev(struct xm_dev *dev)
{
	unsigned long flags;
	/*
	 * This is not best engineering practice, and does cause coupling with
	 * kref internal structure. It might cause an issue if kref internal
	 * refcount structure is changed. We will remove this implementation
	 * in the next kernel version.
	 */
	if (refcount_dec_and_lock_irqsave_local(&xm_dev_instance->kref.refcount,
		&xm_dev_instance_lock, &flags)) {
		__xm_dev_instance_release(&xm_dev_instance->kref);
		spin_unlock_irqrestore(&xm_dev_instance_lock, flags);
        }
}
static inline struct xm_dev *func_to_dev(struct usb_function *f)
{
	return container_of(f, struct xm_dev, function);
}
static void xm_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}
/* add a request to the tail of a list */
static void req_put(struct xm_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;
	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}
/* remove a request from the head of a list */
static struct usb_request *req_get(struct xm_dev *dev, struct list_head *head)
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
static void xm_free_all_requests(struct xm_dev *dev)
{
	struct usb_request *req;
	int i;
	while ((req = req_get(dev, &dev->tx_idle)))
		xm_request_free(req, dev->ep_in);
	for (i = 0; i < RX_REQ_MAX; i++) {
		xm_request_free(dev->rx_req[i], dev->ep_out);
		dev->rx_req[i] = NULL;
	}
}

static struct miscdevice xm_device;

struct xm_uevent_work {
    struct work_struct work;
    char *envp[2];
};

static void xm_uevent_work_fn(struct work_struct *work)
{
    struct xm_uevent_work *w =
        container_of(work, struct xm_uevent_work, work);

    printk("xm_uevent_work_fn\n");

    kobject_uevent_env(&xm_device.this_device->kobj, KOBJ_CHANGE, w->envp);

    kfree(w->envp[0]);
    kfree(w);
}

static void xm_complete_set_string(struct usb_ep *ep, struct usb_request *req)
{
	struct xm_dev *dev = ep->driver_data;
	char *string_dest = NULL;
	int length = req->actual;
	struct xm_uevent_work *w;

	if (!dev) {
		printk("xm_complete_set_string: dev is NULL\n");
		return ;
	}
	if (req->status != 0) {
		printk("err %d\n", req->status);
		return;
	}
	switch (dev->string_index) {
		case ACCESSORY_STRING_MANUFACTURER:
			string_dest = dev->manufacturer;
			break;
		case ACCESSORY_STRING_MODEL:
			string_dest = dev->model;
			break;
		case ACCESSORY_STRING_DESCRIPTION:
			string_dest = dev->description;
			break;
		case ACCESSORY_STRING_VERSION:
			string_dest = dev->version;
			break;
		case ACCESSORY_STRING_URI:
			string_dest = dev->uri;
			break;
		case ACCESSORY_STRING_SERIAL:
			string_dest = dev->serial;
			break;
		default:
			printk("unknown accessory string index %d\n",
				dev->string_index);
			break;
	}
	printk("%s:  %d \n", __func__, __LINE__);
	if (string_dest) {
		unsigned long flags;
		if (length >= ACC_STRING_SIZE)
			length = ACC_STRING_SIZE - 1;
		spin_lock_irqsave(&dev->lock, flags);
		memcpy(string_dest, req->buf, length);
		/* ensure zero termination */
		string_dest[length] = 0;
		printk("%s: string_dest = %s \n", __func__, string_dest);
		spin_unlock_irqrestore(&dev->lock, flags);

		/*
		envp[0] = (char *)kmalloc(length+12+1, GFP_ATOMIC);
		if (!envp[0]) {
			mca_log_info("%s: no mem \n", __func__);
			return ;
		}
		strcpy(envp[0], "SCREENSHARE=");
		printk("%s: envp[0] = %s \n", __func__, envp[0]);
		strcat(envp[0], string_dest);
		memcpy(envp[0]+12, req->buf, length);
		printk("%s: envp[0] = %s \n", __func__, envp[0]);
		*/

		w = kzalloc(sizeof(*w), GFP_ATOMIC);
		if (!w) {
			printk("%s: no mem \n", __func__);
			return ;
		}
		INIT_WORK(&w->work, xm_uevent_work_fn);
		w->envp[0] = kasprintf(GFP_ATOMIC, "SCREENSHARE=%s", string_dest);
		if (!w->envp[0]) {
			printk("%s: no mem error\n", __func__);
			kfree(w);
			return;
		}
		w->envp[1] = NULL;
		schedule_work(&w->work);
	} else {
		printk("unknown accessory string index %d\n",
			dev->string_index);
	}
}
static ssize_t xm_read(struct file *fp, char __user *buf,
	size_t count, loff_t *pos)
{
	struct xm_dev *dev = fp->private_data;
	struct usb_request *req;
	ssize_t r = count;
	ssize_t data_length;
	unsigned int xfer;
	int ret = 0;
	printk("xm_read %d \n", __LINE__);
	if (!dev) {
		printk("xm_read: dev is NULL\n");
		return -EINVAL;
	}
	if (dev->disconnected) {
		printk("disconnected\n");
		return -ENODEV;
	}
	if (count > BULK_BUFFER_SIZE)
		count = BULK_BUFFER_SIZE;
	/* we will block until we're online */
	printk("waiting for online\n");
	ret = wait_event_interruptible(dev->read_wq, dev->online);
	if (ret < 0) {
		r = ret;
		goto done;
	}
	if (!dev->rx_req[0]) {
		printk("USB request already handled/freed\n");
		r = -EINVAL;
		goto done;
	}
	/*
	 * Calculate the data length by considering termination character.
	 * Then compansite the difference of rounding up to
	 * integer multiple of maxpacket size.
	 */
	data_length = count;
	data_length += dev->ep_out->maxpacket - 1;
	data_length -= data_length % dev->ep_out->maxpacket;
	if (dev->rx_done) {
		// last req cancelled. try to get it.
		req = dev->rx_req[0];
		goto copy_data;
	}
requeue_req:
	/* queue a request */
	req = dev->rx_req[0];
	req->length = data_length;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_KERNEL);
	if (ret < 0) {
		r = -EIO;
		goto done;
	} else {
		printk("rx %p queue\n", req);
	}
	/* wait for a request to complete */
	ret = wait_event_interruptible(dev->read_wq, dev->rx_done);
	if (ret < 0) {
		r = ret;
		ret = usb_ep_dequeue(dev->ep_out, req);
		if (ret != 0) {
			// cancel failed. There can be a data already received.
			// it will be retrieved in the next read.
			printk("cancelling failed %d\n", ret);
		}
		goto done;
	}
copy_data:
	dev->rx_done = 0;
	if (dev->online) {
		/* If we got a 0-len packet, throw it back and try again. */
		if (req->actual == 0)
			goto requeue_req;
		printk("rx %p %u\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		r = xfer;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;
	} else
		r = -EIO;
done:
	printk("returning %zd\n", r);
	return r;
}
static ssize_t xm_write(struct file *fp, const char __user *buf,
	size_t count, loff_t *pos)
{
	struct xm_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	ssize_t r = count;
	unsigned int xfer;
	int ret;
	printk("xm_write %d \n", __LINE__);
	if (!dev) {
		printk("xm_write: dev is NULL\n");
		return -EINVAL;
	}
	if (!dev->online || dev->disconnected) {
		printk("disconnected or not online\n");
		return -ENODEV;
	}
	while (count > 0) {
		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = req_get(dev, &dev->tx_idle)) || !dev->online));
		if (!dev->online || dev->disconnected) {
			printk("dev->error\n");
			r = -EIO;
			break;
		}
		if (!req) {
			r = ret;
			break;
		}
		if (count > BULK_BUFFER_SIZE) {
			xfer = BULK_BUFFER_SIZE;
			/* ZLP, They will be more TX requests so not yet. */
			req->zero = 0;
		} else {
			xfer = count;
			/* If the data length is a multiple of the
			 * maxpacket size then send a zero length packet(ZLP).
			 */
			req->zero = ((xfer % dev->ep_in->maxpacket) == 0);
		}
		if (copy_from_user(req->buf, buf, xfer)) {
			r = -EFAULT;
			break;
		}
		req->length = xfer;
		ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
		if (ret < 0) {
			printk("xfer error %d\n", ret);
			r = -EIO;
			break;
		}
		buf += xfer;
		count -= xfer;
		/* zero this so we don't try to free it on error exit */
		req = 0;
	}
	if (req)
		req_put(dev, &dev->tx_idle, req);
	printk("returning %zd\n", r);
	return r;
}
static long xm_ioctl(struct file *fp, unsigned int code, unsigned long value)
{
	struct xm_dev *dev = fp->private_data;
	char *src = NULL;
	int ret;
	printk("xm_ioctl %d \n", __LINE__);
	if (!dev) {
		printk("xm_ioctl: dev is NULL\n");
		return -EINVAL;
	}
	switch (code) {
		case ACCESSORY_GET_STRING_MANUFACTURER:
			src = dev->manufacturer;
			break;
		case ACCESSORY_GET_STRING_MODEL:
			src = dev->model;
			break;
		case ACCESSORY_GET_STRING_DESCRIPTION:
			src = dev->description;
			break;
		case ACCESSORY_GET_STRING_VERSION:
			src = dev->version;
			break;
		case ACCESSORY_GET_STRING_URI:
			src = dev->uri;
			break;
		case ACCESSORY_GET_STRING_SERIAL:
			src = dev->serial;
			break;
		case ACCESSORY_IS_START_REQUESTED:
			return dev->start_requested;
		case ACCESSORY_GET_AUDIO_MODE:
			return dev->audio_mode;
		default:
			return -ENOTTY;
	}
	if (!src)
		return -ENOTTY;
	ret = strlen(src) + 1;
	if (copy_to_user((void __user *)value, src, ret))
		ret = -EFAULT;
	return ret;
}
static int xm_open(struct inode *ip, struct file *fp)
{
	struct xm_dev *dev = get_xm_dev();
	printk("xm_open %d \n", __LINE__);
	if (!dev) {
		printk("xm_open: dev is NULL\n");
		return -ENODEV;
	}
	dev->disconnected = false;
	fp->private_data = dev;
	return 0;
}
static int xm_release(struct inode *ip, struct file *fp)
{
	struct xm_dev *dev = fp->private_data;
	if (!dev)
		return -ENOENT;
	/* indicate that we are disconnected
	 * still could be online so don't touch online flag
	 */
	dev->disconnected = true;
	fp->private_data = NULL;
	put_xm_dev(dev);
	return 0;
}
/* file operations for /dev/usb_accessory */
static const struct file_operations xm_fops = {
	.owner = THIS_MODULE,
	.read = xm_read,
	.write = xm_write,
	.unlocked_ioctl = xm_ioctl,
	.compat_ioctl = xm_ioctl,
	.open = xm_open,
	.release = xm_release,
};
static struct miscdevice xm_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "usb_xiaomitest",
	.fops = &xm_fops,
};
static void xm_complete_setup_noop(struct usb_ep *ep, struct usb_request *req)
{
	/*
	 * Default no-op function when nothing needs to be done for the
	 * setup request
	 */
}
static int __xm_function_bind(struct usb_configuration *c,
		struct usb_function *f, bool configfs)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct usb_string *us;
	struct xm_dev *dev = func_to_dev(f);
	int id;
	int ret;
	printk("__xm_function_bind \n");
	if (!dev) {
		printk(" dev is NULL\n");
		return -EINVAL;
	}
	if (configfs) {
		us = usb_gstrings_attach(cdev, xm_strings,
				ARRAY_SIZE(xm_string_defs));
		if (IS_ERR(us))
			return PTR_ERR(us);
		ret = us[INTERFACE_STRING_INDEX].id;
		xm_interface_desc.iInterface = ret;
		dev->cdev = c->cdev;
	}
	dev->start_requested = 0;
	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	xm_interface_desc.bInterfaceNumber = id;
	return 0;
}
static int xm_function_bind_configfs(struct usb_configuration *c,
			struct usb_function *f)
{
	printk("xm_function_bind_configfs \n");
	return __xm_function_bind(c, f, true);
}
static void xm_function_unbind(struct usb_configuration *c,
		struct usb_function *f)
{
	struct xm_dev *dev = func_to_dev(f);
	if (!dev) {
		printk(" dev is NULL\n");
		return ;
	}
	dev->online = false;		/* clear online flag */
	wake_up(&dev->read_wq);		/* unblock reads on closure */
	wake_up(&dev->write_wq);	/* likewise for writes */
	xm_free_all_requests(dev);
	//xm_hid_unbind(dev);
}
static void xm_getprotocol_work(struct work_struct *data)
{
	char *envp[2] = { "SCREENSHARE=XMPCSCREENSHARE", NULL };
	kobject_uevent_env(&xm_device.this_device->kobj, KOBJ_CHANGE, envp);
}
static void xm_sendstring_work(struct work_struct *data)
{
	char *envp[2] = { "ACCESSORY=SENDSTRING", NULL };
	kobject_uevent_env(&xm_device.this_device->kobj, KOBJ_CHANGE, envp);
}
static void xm_start_work(struct work_struct *data)
{
	char *envp[2] = { "ACCESSORY=START", NULL };
	kobject_uevent_env(&xm_device.this_device->kobj, KOBJ_CHANGE, envp);
}
static int xm_function_set_alt(struct usb_function *f,
		unsigned int intf, unsigned int alt)
{
	struct xm_dev *dev = func_to_dev(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	//int ret;
	if (!dev) {
		printk(" dev is NULL\n");
		return -EINVAL;
	}
	DBG(cdev, "intf: %d alt: %d\n", intf, alt);
	dev->online = true;
	dev->disconnected = false; /* if online then not disconnected */
	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}
static void xm_function_disable(struct usb_function *f)
{
	struct xm_dev *dev = func_to_dev(f);
	struct usb_composite_dev *cdev = NULL;
	if (!dev) {
		printk(" dev is NULL\n");
		return ;
	}
	cdev = dev->cdev;
	dev->disconnected = true;
	dev->online = false; /* so now need to clear online flag here too */
	//usb_ep_disable(dev->ep_in);
	//usb_ep_disable(dev->ep_out);
	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	DBG(cdev, "%s disabled\n", dev->function.name);
}
static int xm_init(void)
{
	struct xm_dev *dev;
	int ret;
	unsigned long flags;
	printk("xm_init \n");
	spin_lock_irqsave(&xm_dev_instance_lock, flags);
	if (xm_dev_instance) {
		spin_unlock_irqrestore(&xm_dev_instance_lock, flags);
		return -EBUSY;
	}
	dev = kzalloc(sizeof(*dev), GFP_ATOMIC);
	if (!dev) {
		spin_unlock_irqrestore(&xm_dev_instance_lock, flags);
		return -ENOMEM;
	}
	printk("xm_init %d \n", __LINE__);
	spin_lock_init(&dev->lock);
	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_LIST_HEAD(&dev->hid_list);
	INIT_LIST_HEAD(&dev->new_hid_list);
	INIT_LIST_HEAD(&dev->dead_hid_list);
	INIT_DELAYED_WORK(&dev->start_work, xm_start_work);
	//INIT_WORK(&dev->hid_work, xm_hid_work);
	INIT_WORK(&dev->getprotocol_work, xm_getprotocol_work);
	INIT_WORK(&dev->sendstring_work, xm_sendstring_work);
	kref_init(&dev->kref);
	xm_dev_instance = dev;
	spin_unlock_irqrestore(&xm_dev_instance_lock, flags);
	ret = misc_register(&xm_device);
	if (ret)
		goto err_free_dev;
	return 0;
err_free_dev:
	/*
	 * Multiple threads might try to access the xm_dev_instance
	 * therefore protect the failure path with spinlock to avoid race
	 * conditions.
	 */
	spin_lock_irqsave(&xm_dev_instance_lock, flags);
	xm_dev_instance = NULL;
	spin_unlock_irqrestore(&xm_dev_instance_lock, flags);
	kfree(dev);
	printk("USB accessory gadget driver failed to initialize\n");
	return ret;
}
void android_xm_disconnect(void)
{
	struct xm_dev *dev = get_xm_dev();
	if (!dev)
		return;
	/* unregister all HID devices if USB is disconnected */
	//kill_all_hid_devices(dev);
	put_xm_dev(dev);
}
static void xm_cleanup(void)
{
	struct xm_dev *dev = get_xm_dev();
	misc_deregister(&xm_device);
	put_xm_dev(dev);
	put_xm_dev(dev); /* Pairs with kref_init() in xm_init() */
}
static struct xm_instance *to_xm_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct xm_instance,
		func_inst.group);
}
static void xm_attr_release(struct config_item *item)
{
	struct xm_instance *fi_xm = to_xm_instance(item);
	usb_put_function_instance(&fi_xm->func_inst);
}
static struct configfs_item_operations xm_item_ops = {
	.release        = xm_attr_release,
};
static struct config_item_type xm_func_type = {
	.ct_item_ops    = &xm_item_ops,
	.ct_owner       = THIS_MODULE,
};
static struct xm_instance *to_fi_xm(struct usb_function_instance *fi)
{
	return container_of(fi, struct xm_instance, func_inst);
}
static int xm_set_inst_name(struct usb_function_instance *fi,
		const char *name)
{
	struct xm_instance *fi_xm;
	char *ptr;
	int name_len;
	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;
	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;
	fi_xm = to_fi_xm(fi);
	fi_xm->name = ptr;
	return 0;
}
static void xm_free_inst(struct usb_function_instance *fi)
{
	struct xm_instance *fi_xm;
	fi_xm = to_fi_xm(fi);
	kfree(fi_xm->name);
	kfree(fi_xm);
	xm_cleanup();
}
static struct usb_function_instance *xmpriusbinterface_alloc_inst(void)
{
	struct xm_instance *fi_xm;
	int err;
	printk("xmpriusbinterface_alloc_inst \n");
	fi_xm = kzalloc(sizeof(*fi_xm), GFP_KERNEL);
	if (!fi_xm)
		return ERR_PTR(-ENOMEM);
	fi_xm->func_inst.set_inst_name = xm_set_inst_name;
	fi_xm->func_inst.free_func_inst = xm_free_inst;
	err = xm_init();
	if (err) {
		kfree(fi_xm);
		return ERR_PTR(err);
	}
	config_group_init_type_name(&fi_xm->func_inst.group,
					"", &xm_func_type);

	return  &fi_xm->func_inst;
}
static void xm_free(struct usb_function *f)
{
	struct xm_dev *dev = func_to_dev(f);
	put_xm_dev(dev);
}
bool __xm_req_match(const struct usb_ctrlrequest *ctrl)
{
	struct xm_dev *dev = get_xm_dev();
	u8 bRequestType = ctrl->bRequestType;
	u8 bRequest = ctrl->bRequest;
	bool ret = false;
	/*
	 * If instance is not created which is the case in power off charging
	 * mode, dev will be NULL. Hence return error if it is the case.
	 */
	if (!dev)
		return false;
	if (bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {
		switch (bRequest) {
		case ACCESSORY_START:
		case ACCESSORY_SEND_STRING:
		case ACCESSORY_SET_AUDIO_MODE:
		case ACCESSORY_REGISTER_HID:
		case ACCESSORY_UNREGISTER_HID:
		case ACCESSORY_SET_HID_REPORT_DESC:
		case ACCESSORY_SEND_HID_EVENT:
			ret = true;
			break;
		default:
			ret = false;
			break;
		}
	} else if ((bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) &&
			(bRequest == ACCESSORY_GET_PROTOCOL))
		ret = true;
	put_xm_dev(dev);
	return ret;
}
static bool xm_req_match(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl, bool config)
{
	return __xm_req_match(ctrl);
}
bool android_xm_req_match_composite(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl)
{
	return __xm_req_match(ctrl);
}
static int __xm_setup(struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *ctrl)
{
	struct xm_dev *dev = get_xm_dev();
	int value = -EOPNOTSUPP;
	//struct xm_hid_dev *hid;
	//int offset;
	u8 bRequestType = ctrl->bRequestType;
	u8 bRequest = ctrl->bRequest;
	u16 wIndex = le16_to_cpu(ctrl->wIndex);
	u16 wValue = le16_to_cpu(ctrl->wValue);
	u16 wLength = le16_to_cpu(ctrl->wLength);
	//unsigned long flags;
	printk("%s: bRequestType = 0x%x, bRequest = 0x%x \n", __func__, bRequestType, bRequest);
	printk("%s: wIndex = 0x%x, wValue = 0x%x \n", __func__, wIndex, wValue);
	printk("%s: wLength = 0x%x \n", __func__, wLength);
	/*
	 * If instance is not created which is the case in power off charging
	 * mode, dev will be NULL. Hence return error if it is the case.
	 */
	if (!dev)
		return -ENODEV;
	if (bRequestType == (USB_DIR_OUT | USB_TYPE_VENDOR)) {
		if (bRequest == ACCESSORY_START) {
			dev->start_requested = 1;
			schedule_delayed_work(
				&dev->start_work, msecs_to_jiffies(10));
			value = 0;
			cdev->req->complete = xm_complete_setup_noop;
		} else if (bRequest == ACCESSORY_SEND_STRING) {
			schedule_work(&dev->sendstring_work);
			dev->string_index = wIndex;
			cdev->gadget->ep0->driver_data = dev;
			cdev->req->complete = xm_complete_set_string;
			value = wLength;
		} else if (bRequest == ACCESSORY_SET_AUDIO_MODE &&
				wIndex == 0 && wLength == 0) {
			dev->audio_mode = wValue;
			cdev->req->complete = xm_complete_setup_noop;
			value = 0;
		}
	} else if (bRequestType == (USB_DIR_IN | USB_TYPE_VENDOR)) {
		if (bRequest == ACCESSORY_GET_PROTOCOL) {
			schedule_work(&dev->getprotocol_work);
			*((u16 *)cdev->req->buf) = PROTOCOL_VERSION;
			value = sizeof(u16);
			cdev->req->complete = xm_complete_setup_noop;
			/* clear strings left over from a previous session */
			memset(dev->manufacturer, 0,
					sizeof(dev->manufacturer));
			memset(dev->model, 0, sizeof(dev->model));
			memset(dev->description, 0, sizeof(dev->description));
			memset(dev->version, 0, sizeof(dev->version));
			memset(dev->uri, 0, sizeof(dev->uri));
			memset(dev->serial, 0, sizeof(dev->serial));
			dev->start_requested = 0;
			dev->audio_mode = 0;
		}
	}
	if (value >= 0) {
		cdev->req->zero = 0;
		cdev->req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "setup response queue error\n");
	}
//err:
	if (value == -EOPNOTSUPP)
		DBG(cdev,
		"unknown class-specific ctrl req %02x.%02x v%04x i%04x l%u\n",
			ctrl->bRequestType, ctrl->bRequest,
			wValue, wIndex, wLength);
	put_xm_dev(dev);
	return value;
}
int android_xm_setup_composite(struct usb_composite_dev *cdev,
			      const struct usb_ctrlrequest *ctrl)
{
	u16 w_length = le16_to_cpu(ctrl->wLength);
	printk("android_xm_setup_composite \n");
	if (w_length > USB_COMP_EP0_BUFSIZ) {
		if (ctrl->bRequestType & USB_DIR_IN) {
			/*
			 * Cast away the const, we are going to overwrite on
			 * purpose.
			 */
			__le16 *temp = (__le16 *)&ctrl->wLength;
			*temp = cpu_to_le16(USB_COMP_EP0_BUFSIZ);
			w_length = USB_COMP_EP0_BUFSIZ;
		} else {
			return -EINVAL;
		}
	}
	return __xm_setup(cdev, ctrl);
}
static int xm_setup(struct usb_function *f,
			const struct usb_ctrlrequest *ctrl)
{
	printk("xm_setup \n");
	if (f->config != NULL && f->config->cdev != NULL)
		return __xm_setup(f->config->cdev, ctrl);
	else
		return -1;
}
static struct usb_function *xmpriusbinterface_alloc(struct usb_function_instance *fi)
{
	struct xm_dev *dev = get_xm_dev();
	printk("xmpriusbinterface_alloc \n");
	if (!dev) {
		printk(" dev is NULL\n");
		return ERR_PTR(-EINVAL);
	}
	dev->function.name = "xiaomitest";
	dev->function.strings = xm_strings;
	dev->function.fs_descriptors = fs_xm_descs;
	dev->function.hs_descriptors = hs_xm_descs;
	dev->function.ss_descriptors = ss_xm_descs;
	dev->function.ssp_descriptors = ssp_xm_descs;
	dev->function.bind = xm_function_bind_configfs;
	dev->function.unbind = xm_function_unbind;
	dev->function.set_alt = xm_function_set_alt;
	dev->function.disable = xm_function_disable;
	dev->function.free_func = xm_free;
	dev->function.req_match = xm_req_match;
	dev->function.setup = xm_setup;
	return &dev->function;
}
DECLARE_USB_FUNCTION_INIT(xmpriusbinterface, xmpriusbinterface_alloc_inst, xmpriusbinterface_alloc);
MODULE_LICENSE("GPL");

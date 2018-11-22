/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/usb/ipc_bridge.h>
#include "f_ipc.h"

#define MAX_INST_NAME_LEN	40

#define IPC_BRIDGE_MAX_READ_SZ	(8 * 1024)
#define IPC_BRIDGE_MAX_WRITE_SZ	(8 * 1024)

/* for configfs support */
struct ipc_opts {
	struct usb_function_instance func_inst;
	struct ipc_context *ctxt;
};

static inline struct ipc_opts *to_ipc_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct ipc_opts,
			    func_inst.group);
}

static struct usb_interface_descriptor intf_desc = {
	.bLength            =	sizeof(intf_desc),
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bNumEndpoints      =	2,
	.bInterfaceClass    =	0xFF,
	.bInterfaceSubClass =	0xFF,
	.bInterfaceProtocol =	0x30,
};

static struct usb_endpoint_descriptor hs_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(512),
	.bInterval        =	0,
};
static struct usb_endpoint_descriptor fs_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor hs_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(512),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor fs_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor ss_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_bulk_in_comp_desc = {
	.bLength =		sizeof(ss_bulk_in_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_endpoint_descriptor ss_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_bulk_out_comp_desc = {
	.bLength =		sizeof(ss_bulk_out_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
};

static struct usb_descriptor_header *fs_ipc_desc[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fs_bulk_out_desc,
	NULL,
	};
static struct usb_descriptor_header *hs_ipc_desc[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &hs_bulk_in_desc,
	(struct usb_descriptor_header *) &hs_bulk_out_desc,
	NULL,
};

static struct usb_descriptor_header *ss_ipc_desc[] = {
	(struct usb_descriptor_header *) &intf_desc,
	(struct usb_descriptor_header *) &ss_bulk_in_desc,
	(struct usb_descriptor_header *) &ss_bulk_in_comp_desc,
	(struct usb_descriptor_header *) &ss_bulk_out_desc,
	(struct usb_descriptor_header *) &ss_bulk_out_comp_desc,
	NULL,
};

/* String descriptors */

static struct usb_string ipc_string_defs[] = {
	[0] = { .s = "IPC" },
	{  } /* end of list */
};

static struct usb_gadget_strings ipc_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		ipc_string_defs,
};

static struct usb_gadget_strings *ipc_strings[] = {
	&ipc_string_table,
	NULL,
};

enum current_state_type {
	IPC_DISCONNECTED,
	IPC_CONNECTED,
};

/*
 * struct ipc_context - USB IPC router function driver private structure
 * @function: function structure for USB interface
 * @out: USB OUT endpoint struct
 * @in: USB IN endpoint struct
 * @in_req: USB IN endpoint request
 * @out_req: USB OUT endpoint request
 * @lock: Spinlock to protect structure members
 * @state_wq: Waitqueue to wait on online and disconnected states
 * @read_done: Denote OUT endpoint request completion
 * @write_done: Denote IN endpoint request completion
 * @online: If true, function is ready to send and receive data
 * @connected: If true, set_alt issued by the host
 * @opened: If true, IPC router platform device has opened this route
 * @cdev: USB composite device struct
 * @pdev: Platform device to register with IPC router
 * @func_work: Work item to register pdev with IPC router and update states
 * @current_state: Current status of the interface
 */
struct ipc_context {
	struct usb_function function;
	struct usb_ep *out;
	struct usb_ep *in;
	struct usb_request *in_req;
	struct usb_request *out_req;
	spinlock_t lock;
	wait_queue_head_t state_wq;
	struct completion read_done;
	struct completion write_done;
	unsigned int online;
	unsigned int connected;
	bool opened;
	struct usb_composite_dev *cdev;
	struct platform_device *pdev;
	struct work_struct func_work;
	enum current_state_type current_state;

	/* pkt counters */
	unsigned long bytes_to_host;
	unsigned long bytes_to_mdm;
	unsigned int pending_writes;
	unsigned int pending_reads;
};

static struct ipc_context *ipc_dev;

static inline struct ipc_context *func_to_ipc(struct usb_function *f)
{
	return container_of(f, struct ipc_context, function);
}

static void ipc_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	complete(&ipc_dev->write_done);
	ipc_dev->bytes_to_host += req->actual;
	ipc_dev->pending_writes--;
}

/*
 * ipc_write() - Write IPC data from IPC router
 * @pdev: IPC router USB transport platform device
 * @buf: Data buffer from IPC core
 * @count: Data buffer size
 *
 * Enqueue a request on IN endpoint of the interface corresponding to this
 * channel. This function returns proper error code if the interface or data
 * buffer is not configured properly. If ep_queue fails because interface is
 * suspended, then it waits for interface to be online or get disconnected.
 *
 * This function operates asynchronously. WRITE_DONE event is notified after
 * completion of IN request.
 */
static int ipc_write(struct platform_device *pdev, char *buf,
							unsigned int count)
{
	unsigned long flags;
	struct usb_request *req;
	struct usb_ep *in;

	if (!ipc_dev)
		return -ENODEV;
	if (ipc_dev->pdev != pdev)
		return -EINVAL;
	if (!ipc_dev->opened)
		return -EPERM;
	if (count > IPC_BRIDGE_MAX_WRITE_SZ)
		return -ENOSPC;

	spin_lock_irqsave(&ipc_dev->lock, flags);
	in = ipc_dev->in;
	req = ipc_dev->in_req;
	req->buf = buf;
	req->length = count;
	ipc_dev->pending_writes++;
	spin_unlock_irqrestore(&ipc_dev->lock, flags);

retry_write:
	if (ipc_dev->current_state == IPC_DISCONNECTED) {
		pr_err("%s: Interface disconnected, cannot queue req\n",
								__func__);
		ipc_dev->pending_writes--;
		return -EINVAL;
	}

	if (usb_ep_queue(in, req, GFP_KERNEL)) {
		wait_event_interruptible(ipc_dev->state_wq, ipc_dev->online ||
				ipc_dev->current_state == IPC_DISCONNECTED);
		pr_debug("%s: Interface ready, Retry IN request\n", __func__);
		goto retry_write;
	}

	if (unlikely(wait_for_completion_interruptible(&ipc_dev->write_done))) {
		usb_ep_dequeue(in, req);
		return -EINTR;
	}

	return !req->status ? req->actual : req->status;
}

static void ipc_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	complete(&ipc_dev->read_done);
	ipc_dev->bytes_to_mdm += req->actual;
	ipc_dev->pending_reads--;
}

/*
 * ipc_read() - Read IPC data from USB
 * @pdev: IPC router USB transport platform device
 * @buf: Data buffer to be populated
 * @count: Data buffer size
 *
 * Enqueue a request on OUT endpoint of the interface corresponding to this
 * channel. This function returns proper error code if the interface or data
 * buffer is not configured properly. If ep_queue fails because interface is
 * suspended, then it waits for interface to be online or get disconnected.
 *
 * This function operates asynchronously. READ_DONE event is notified after
 * completion of OUT request.
 */
static int ipc_read(struct platform_device *pdev, char *buf, unsigned int count)
{
	unsigned long flags;
	struct usb_request *req;
	struct usb_ep *out;

	if (!ipc_dev)
		return -ENODEV;
	if (ipc_dev->pdev != pdev)
		return -EINVAL;
	if (!ipc_dev->opened)
		return -EPERM;
	if (count > IPC_BRIDGE_MAX_READ_SZ)
		return -ENOSPC;

	spin_lock_irqsave(&ipc_dev->lock, flags);
	out = ipc_dev->out;
	req = ipc_dev->out_req;
	req->buf = buf;
	req->length = count;
	ipc_dev->pending_reads++;
	spin_unlock_irqrestore(&ipc_dev->lock, flags);

retry_read:
	if (ipc_dev->current_state == IPC_DISCONNECTED) {
		pr_err("%s: Interface disconnected, cannot queue req\n",
							__func__);
		ipc_dev->pending_reads--;
		return -EINVAL;
	}

	if (usb_ep_queue(out, req, GFP_KERNEL)) {
		wait_event_interruptible(ipc_dev->state_wq, ipc_dev->online ||
				ipc_dev->current_state == IPC_DISCONNECTED);
		pr_debug("%s: Interface ready, Retry OUT request\n", __func__);
		goto retry_read;
	}

	if (unlikely(wait_for_completion_interruptible(&ipc_dev->read_done))) {
		usb_ep_dequeue(out, req);
		return -EINTR;
	}

	return !req->status ? req->actual : req->status;
}

static int ipc_open(struct platform_device *pdev)
{
	unsigned long flags;

	if (ipc_dev->pdev != pdev)
		return -EINVAL;

	pr_debug("%s: Trying to open IPC bridge\n", __func__);
	spin_lock_irqsave(&ipc_dev->lock, flags);
	if (ipc_dev->opened) {
		spin_unlock_irqrestore(&ipc_dev->lock, flags);
		pr_err("%s: Bridge already opened\n", __func__);
		return -EBUSY;
	}

	ipc_dev->opened = true;
	spin_unlock_irqrestore(&ipc_dev->lock, flags);

	return 0;
}

static void ipc_close(struct platform_device *pdev)
{
	unsigned long flags;

	WARN_ON(ipc_dev->pdev != pdev);

	pr_debug("%s: Trying to close IPC bridge\n", __func__);
	spin_lock_irqsave(&ipc_dev->lock, flags);
	if (!ipc_dev->opened) {
		spin_unlock_irqrestore(&ipc_dev->lock, flags);
		pr_err("%s: Bridge already closed\n", __func__);
		return;
	}

	ipc_dev->opened = false;
	spin_unlock_irqrestore(&ipc_dev->lock, flags);
}

static const struct ipc_bridge_platform_data ipc_pdata = {
	.max_read_size = IPC_BRIDGE_MAX_READ_SZ,
	.max_write_size = IPC_BRIDGE_MAX_WRITE_SZ,
	.open = ipc_open,
	.read = ipc_read,
	.write = ipc_write,
	.close = ipc_close,
};

static void ipc_function_work(struct work_struct *w)
{
	struct ipc_context *ctxt = container_of(w, struct ipc_context,
								func_work);
	int ret;

	switch (ctxt->current_state) {
	case IPC_DISCONNECTED:
		if (!ctxt->connected)
			break;

		ctxt->current_state = IPC_CONNECTED;
		ctxt->pdev = platform_device_alloc("ipc_bridge", -1);
		if (!ctxt->pdev)
			goto pdev_fail;

		ret = platform_device_add_data(ctxt->pdev, &ipc_pdata,
				sizeof(struct ipc_bridge_platform_data));
		if (ret) {
			platform_device_put(ctxt->pdev);
			pr_err("%s: fail to add pdata\n", __func__);
			goto pdev_fail;
		}

		ret = platform_device_add(ctxt->pdev);
		if (ret) {
			platform_device_put(ctxt->pdev);
			pr_err("%s: fail to add pdev\n", __func__);
			goto pdev_fail;
		}
		break;
	case IPC_CONNECTED:
		if (ctxt->connected)
			break;

		ctxt->current_state = IPC_DISCONNECTED;
		wake_up(&ctxt->state_wq);
		platform_device_unregister(ctxt->pdev);
		break;
	default:
		pr_debug("%s: Unknown current state\n", __func__);
	}

	return;

pdev_fail:
	ctxt->current_state = IPC_DISCONNECTED;
	return;
}

static int ipc_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct ipc_context *ctxt = func_to_ipc(f);
	struct usb_ep *ep;
	int status;

	pr_debug("%s: start binding\n", __func__);
	ctxt->cdev = c->cdev;

	if (ipc_string_defs[0].id == 0) {
		status = usb_string_id(cdev);
		if (status < 0)
			return status;
		ipc_string_defs[0].id = status;
	}

	intf_desc.bInterfaceNumber =  usb_interface_id(c, f);

	status = -ENODEV;
	ep = usb_ep_autoconfig(cdev->gadget, &fs_bulk_in_desc);
	if (!ep)
		goto fail;
	ctxt->in = ep;
	ep->driver_data = ctxt;

	ep = usb_ep_autoconfig(cdev->gadget, &fs_bulk_out_desc);
	if (!ep)
		goto fail;
	ctxt->out = ep;
	ep->driver_data = ctxt;

	status = -ENOMEM;
	ctxt->in_req = usb_ep_alloc_request(ctxt->in, GFP_KERNEL);
	if (!ctxt->in_req)
		goto fail;

	ctxt->in_req->complete = ipc_in_complete;
	ctxt->out_req = usb_ep_alloc_request(ctxt->out, GFP_KERNEL);
	if (!ctxt->out_req)
		goto fail;

	ctxt->out_req->complete = ipc_out_complete;
	/* copy descriptors, and track endpoint copies */
	f->fs_descriptors = usb_copy_descriptors(fs_ipc_desc);
	if (!f->fs_descriptors)
		goto fail;

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		hs_bulk_in_desc.bEndpointAddress =
				fs_bulk_in_desc.bEndpointAddress;
		hs_bulk_out_desc.bEndpointAddress =
				fs_bulk_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(hs_ipc_desc);
		if (!f->hs_descriptors)
			goto fail;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		ss_bulk_in_desc.bEndpointAddress =
				fs_bulk_in_desc.bEndpointAddress;
		ss_bulk_out_desc.bEndpointAddress =
				fs_bulk_out_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors = usb_copy_descriptors(ss_ipc_desc);
		if (!f->ss_descriptors)
			goto fail;
	}

	return 0;
fail:
	if (f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
	if (f->fs_descriptors)
		usb_free_descriptors(f->fs_descriptors);
	if (ctxt->out_req)
		usb_ep_free_request(ctxt->out, ctxt->out_req);
	if (ctxt->in_req)
		usb_ep_free_request(ctxt->in, ctxt->in_req);
	if (ctxt->out)
		ctxt->out->driver_data = NULL;
	if (ctxt->in)
		ctxt->in->driver_data = NULL;

	pr_err("%s: can't bind, err %d\n", __func__, status);
	return status;
}

static void ipc_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct ipc_context *ctxt = func_to_ipc(f);

	pr_debug("%s: start unbinding\nclear_desc\n", __func__);
	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);

	usb_free_descriptors(f->fs_descriptors);

	if (ctxt->out_req)
		usb_ep_free_request(ctxt->out, ctxt->out_req);
	if (ctxt->in_req)
		usb_ep_free_request(ctxt->in, ctxt->in_req);
}

static int ipc_set_alt(struct usb_function *f, unsigned int intf,
				unsigned int alt)
{
	struct ipc_context *ctxt = func_to_ipc(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	unsigned long flags;
	int rc = 0;

	pr_debug("%s: ipc_dev: %pK\n", __func__, ctxt);
	if (config_ep_by_speed(cdev->gadget, f, ctxt->in) ||
	    config_ep_by_speed(cdev->gadget, f, ctxt->out)) {
		ctxt->in->desc = NULL;
		ctxt->out->desc = NULL;
		return -EINVAL;
	}

	ctxt->in->driver_data = ctxt;
	rc = usb_ep_enable(ctxt->in);
	if (rc) {
		ERROR(ctxt->cdev, "can't enable %s, result %d\n",
						ctxt->in->name, rc);
		return rc;
	}

	ctxt->out->driver_data = ctxt;
	rc = usb_ep_enable(ctxt->out);
	if (rc) {
		ERROR(ctxt->cdev, "can't enable %s, result %d\n",
						ctxt->out->name, rc);
		usb_ep_disable(ctxt->in);
		return rc;
	}

	spin_lock_irqsave(&ctxt->lock, flags);
	ctxt->connected = 1;
	ctxt->online = 1;
	spin_unlock_irqrestore(&ctxt->lock, flags);
	schedule_work(&ctxt->func_work);

	return rc;
}

static void ipc_disable(struct usb_function *f)
{
	struct ipc_context *ctxt = func_to_ipc(f);
	unsigned long flags;

	pr_debug("%s: Disabling\n", __func__);
	spin_lock_irqsave(&ctxt->lock, flags);
	ctxt->online = 0;
	ctxt->connected = 0;
	spin_unlock_irqrestore(&ctxt->lock, flags);
	schedule_work(&ctxt->func_work);

	usb_ep_disable(ctxt->in);
	ctxt->in->driver_data = NULL;

	usb_ep_disable(ctxt->out);
	ctxt->out->driver_data = NULL;
}

static void ipc_suspend(struct usb_function *f)
{
	unsigned long flags;

	spin_lock_irqsave(&ipc_dev->lock, flags);
	ipc_dev->online = 0;
	spin_unlock_irqrestore(&ipc_dev->lock, flags);
}

static void ipc_resume(struct usb_function *f)
{
	unsigned long flags;

	spin_lock_irqsave(&ipc_dev->lock, flags);
	ipc_dev->online = 1;
	spin_unlock_irqrestore(&ipc_dev->lock, flags);
	wake_up(&ipc_dev->state_wq);
}

static void ipc_free(struct usb_function *f) {}

struct usb_function *ipc_bind_config(struct usb_function_instance *fi)
{
	struct ipc_opts *opts;
	struct ipc_context *ctxt;
	struct usb_function *f;

	opts = container_of(fi, struct ipc_opts, func_inst);
	ctxt = opts->ctxt;

	f = &ctxt->function;
	f->name = "ipc";
	f->strings = ipc_strings;
	f->bind = ipc_bind;
	f->unbind = ipc_unbind;
	f->set_alt = ipc_set_alt;
	f->disable = ipc_disable;
	f->suspend = ipc_suspend;
	f->resume = ipc_resume;
	f->free_func = ipc_free;

	pr_debug("%s: complete\n", __func__);

	return f;
}

#if defined(CONFIG_DEBUG_FS)
static char ipc_debug_buffer[PAGE_SIZE];

static ssize_t debug_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	char *buf = ipc_debug_buffer;
	int temp = 0;
	unsigned long flags;

	if (ipc_dev) {
		spin_lock_irqsave(&ipc_dev->lock, flags);
		temp += scnprintf(buf + temp, PAGE_SIZE - temp,
				"endpoints: %s, %s\n"
				"bytes to host: %lu\n"
				"bytes to mdm:  %lu\n"
				"pending writes:  %u\n"
				"pending reads: %u\n",
				ipc_dev->in->name, ipc_dev->out->name,
				ipc_dev->bytes_to_host,
				ipc_dev->bytes_to_mdm,
				ipc_dev->pending_writes,
				ipc_dev->pending_reads);
		spin_unlock_irqrestore(&ipc_dev->lock, flags);
	}

	return simple_read_from_buffer(ubuf, count, ppos, buf, temp);
}

static ssize_t debug_reset_stats(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	unsigned long flags;

	if (ipc_dev) {
		spin_lock_irqsave(&ipc_dev->lock, flags);
		ipc_dev->bytes_to_host = 0;
		ipc_dev->bytes_to_mdm = 0;
		spin_unlock_irqrestore(&ipc_dev->lock, flags);
	}

	return count;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations debug_fipc_ops = {
	.open = debug_open,
	.read = debug_read_stats,
	.write = debug_reset_stats,
};

static struct dentry *dent_ipc;
static void fipc_debugfs_init(void)
{
	struct dentry *dent_ipc_status;

	dent_ipc = debugfs_create_dir("usb_ipc", NULL);
	if (!dent_ipc || IS_ERR(dent_ipc))
		return;

	dent_ipc_status = debugfs_create_file("status", 0444, dent_ipc, NULL,
			&debug_fipc_ops);

	if (!dent_ipc_status || IS_ERR(dent_ipc_status)) {
		debugfs_remove(dent_ipc);
		dent_ipc = NULL;
		return;
	}
}

static void fipc_debugfs_remove(void)
{
	debugfs_remove_recursive(dent_ipc);
}
#else
static inline void fipc_debugfs_init(void) {}
static inline void fipc_debugfs_remove(void) {}
#endif

static void ipc_opts_release(struct config_item *item)
{
	struct ipc_opts *opts = to_ipc_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations ipc_item_ops = {
	.release	= ipc_opts_release,
};

static struct config_item_type ipc_func_type = {
	.ct_item_ops	= &ipc_item_ops,
	.ct_owner	= THIS_MODULE,
};

static int ipc_set_inst_name(struct usb_function_instance *fi,
	const char *name)
{
	struct ipc_opts *opts = container_of(fi, struct ipc_opts, func_inst);
	int name_len;

	name_len = strlen(name) + 1;
	if (name_len > MAX_INST_NAME_LEN)
		return -ENAMETOOLONG;

	ipc_dev = kzalloc(sizeof(*ipc_dev), GFP_KERNEL);
	if (!ipc_dev)
		return -ENOMEM;

	spin_lock_init(&ipc_dev->lock);
	init_waitqueue_head(&ipc_dev->state_wq);
	init_completion(&ipc_dev->read_done);
	init_completion(&ipc_dev->write_done);
	INIT_WORK(&ipc_dev->func_work, ipc_function_work);

	opts->ctxt = ipc_dev;

	return 0;
}

static void ipc_free_inst(struct usb_function_instance *f)
{
	struct ipc_opts *opts = container_of(f, struct ipc_opts, func_inst);

	kfree(opts->ctxt);
	kfree(opts);
}

static struct usb_function_instance *ipc_alloc_inst(void)
{
	struct ipc_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	opts->func_inst.set_inst_name = ipc_set_inst_name;
	opts->func_inst.free_func_inst = ipc_free_inst;
	config_group_init_type_name(&opts->func_inst.group, "",
				    &ipc_func_type);

	return &opts->func_inst;
}

static struct usb_function *ipc_alloc(struct usb_function_instance *fi)
{
	return ipc_bind_config(fi);
}

DECLARE_USB_FUNCTION(ipc, ipc_alloc_inst, ipc_alloc);

void *ipc_setup(void)
{
	struct ipc_opts *opts;

	fipc_debugfs_init();

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		goto err;

	ipc_dev = kzalloc(sizeof(*ipc_dev), GFP_KERNEL);
	if (!ipc_dev)
		goto dev_err;

	spin_lock_init(&ipc_dev->lock);
	init_waitqueue_head(&ipc_dev->state_wq);
	init_completion(&ipc_dev->read_done);
	init_completion(&ipc_dev->write_done);
	INIT_WORK(&ipc_dev->func_work, ipc_function_work);

	opts->ctxt = ipc_dev;

	return (void *)&opts->func_inst;

dev_err:
	kfree(opts);
err:
	return ERR_PTR(-ENOMEM);
}

void ipc_cleanup(void *fi)
{
	struct usb_function_instance *func_inst = NULL;

	func_inst = (struct usb_function_instance *)fi;

	ipc_free_inst(func_inst);
	fipc_debugfs_remove();
}

static int __init ipc_init(void)
{
	int ret;

	ret = usb_function_register(&ipcusb_func);
	if (ret)
		pr_err("%s: failed to register ipc %d\n", __func__, ret);

	fipc_debugfs_init();

	return ret;
}

static void __exit ipc_exit(void)
{
	fipc_debugfs_remove();
	usb_function_unregister(&ipcusb_func);
}

module_init(ipc_init);
module_exit(ipc_exit);

MODULE_DESCRIPTION("IPC function driver");
MODULE_LICENSE("GPL v2");

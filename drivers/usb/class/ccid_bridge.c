/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt "\n", __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/cdev.h>

#include <linux/usb/ccid_bridge.h>

#define CCID_CLASS_DECRIPTOR_TYPE 0x21
#define CCID_NOTIFY_SLOT_CHANGE	0x50
#define CCID_NOTIFY_HARDWARE_ERROR 0x51
#define CCID_ABORT_REQ 0x1
#define CCID_GET_CLK_FREQ_REQ 0x2
#define CCID_GET_DATA_RATES 0x3

#define CCID_BRIDGE_MSG_SZ 512
#define CCID_BRIDGE_OPEN_TIMEOUT 500 /* msec */
#define CCID_CONTROL_TIMEOUT 500 /* msec */
#define CCID_BRIDGE_MSG_TIMEOUT 500 /* msec */

struct ccid_bridge {
	struct usb_device *udev;
	struct usb_interface *intf;
	unsigned int in_pipe;
	unsigned int out_pipe;
	unsigned int int_pipe;
	struct urb *inturb;
	struct urb *readurb;
	struct urb *writeurb;

	bool opened;
	bool events_supported;
	bool is_suspended;
	struct mutex open_mutex;
	struct mutex write_mutex;
	struct mutex read_mutex;
	struct mutex event_mutex;
	int write_result;
	int read_result;
	int event_result;
	wait_queue_head_t open_wq;
	wait_queue_head_t write_wq;
	wait_queue_head_t read_wq;
	wait_queue_head_t event_wq;
	struct usb_ccid_event cur_event;
	void *intbuf;

	dev_t chrdev;
	struct cdev cdev;
	struct class *class;
	struct device *device;
};

static struct ccid_bridge *__ccid_bridge_dev;

static void ccid_bridge_out_cb(struct urb *urb)
{
	struct ccid_bridge *ccid = urb->context;

	if (urb->dev->state == USB_STATE_NOTATTACHED)
		ccid->write_result = -ENODEV;
	else
		ccid->write_result = urb->status ? : urb->actual_length;

	pr_debug("write result = %d", ccid->write_result);
	wake_up(&ccid->write_wq);
}

static void ccid_bridge_in_cb(struct urb *urb)
{
	struct ccid_bridge *ccid = urb->context;

	if (urb->dev->state == USB_STATE_NOTATTACHED)
		ccid->read_result = -ENODEV;
	else
		ccid->read_result = urb->status ? : urb->actual_length;

	pr_debug("read result = %d", ccid->read_result);
	wake_up(&ccid->read_wq);
}

static void ccid_bridge_int_cb(struct urb *urb)
{
	struct ccid_bridge *ccid = urb->context;
	u8 *msg_type;
	bool wakeup = true;

	if (urb->dev->state == USB_STATE_NOTATTACHED || (urb->status &&
				urb->status != -ENOENT)) {
		ccid->event_result = -ENODEV;
		wakeup = true;
		goto out;
	}

	/*
	 * Don't wakeup the event ioctl process during suspend.
	 * The suspend state is not visible to user space.
	 * we wake up the process after resume to send RESUME
	 * event if the device supports remote wakeup.
	 */
	if (urb->status == -ENOENT && !urb->actual_length) {
		ccid->event_result = -ENOENT;
		wakeup = false;
		goto out;
	}

	ccid->event_result = 0;
	msg_type = urb->transfer_buffer;
	switch (*msg_type) {
	case CCID_NOTIFY_SLOT_CHANGE:
		pr_debug("NOTIFY_SLOT_CHANGE event arrived");
		ccid->cur_event.event = USB_CCID_NOTIFY_SLOT_CHANGE_EVENT;
		ccid->cur_event.u.notify.slot_icc_state = *(++msg_type);
		break;
	case CCID_NOTIFY_HARDWARE_ERROR:
		pr_debug("NOTIFY_HARDWARE_ERROR event arrived");
		ccid->cur_event.event = USB_CCID_HARDWARE_ERROR_EVENT;
		ccid->cur_event.u.error.slot = *(++msg_type);
		ccid->cur_event.u.error.seq = *(++msg_type);
		ccid->cur_event.u.error.error_code = *(++msg_type);
		break;
	default:
		pr_err("UNKNOWN event arrived\n");
		ccid->event_result = -EINVAL;
	}

out:
	pr_debug("returning %d", ccid->event_result);
	if (wakeup)
		wake_up(&ccid->event_wq);
}

static int ccid_bridge_submit_inturb(struct ccid_bridge *ccid)
{
	int ret = 0;

	/*
	 * Don't resume the bus to submit an interrupt URB.
	 * We submit the URB in resume path.  This is important.
	 * Because the device will be in suspend state during
	 * multiple system suspend/resume cycles.  The user space
	 * process comes here during system resume after it is
	 * unfrozen.
	 */
	if (!ccid->int_pipe || ccid->is_suspended)
		goto out;

	ret = usb_autopm_get_interface(ccid->intf);
	if (ret < 0) {
		pr_debug("fail to get autopm with %d\n", ret);
		goto out;
	}
	ret = usb_submit_urb(ccid->inturb, GFP_KERNEL);
	if (ret < 0)
		pr_err("fail to submit int urb with %d\n", ret);
	usb_autopm_put_interface(ccid->intf);

out:
	pr_debug("returning %d", ret);
	return ret;
}

static int ccid_bridge_get_event(struct ccid_bridge *ccid)
{
	int ret = 0;

	/*
	 * The first event returned after the device resume
	 * will be RESUME event.  This event is set by
	 * the resume.
	 */
	if (ccid->cur_event.event)
		goto out;

	ccid->event_result = -EINPROGRESS;

	ret = ccid_bridge_submit_inturb(ccid);
	if (ret < 0)
		goto out;

	/*
	 * Wait for the notification on interrupt endpoint
	 * or remote wakeup event from the resume.  The
	 * int urb completion handler and resume callback
	 * take care of setting the current event.
	 */
	mutex_unlock(&ccid->event_mutex);
	ret = wait_event_interruptible(ccid->event_wq,
			(ccid->event_result != -EINPROGRESS));
	mutex_lock(&ccid->event_mutex);

	if (ret == -ERESTARTSYS) /* interrupted */
		usb_kill_urb(ccid->inturb);
	else
		ret = ccid->event_result;
out:
	pr_debug("returning %d", ret);
	return ret;
}

static int ccid_bridge_open(struct inode *ip, struct file *fp)
{
	struct ccid_bridge *ccid = container_of(ip->i_cdev,
				struct ccid_bridge, cdev);
	int ret;

	pr_debug("called");

	mutex_lock(&ccid->open_mutex);
	if (ccid->opened) {
		ret = -EBUSY;
		goto out;
	}
	mutex_unlock(&ccid->open_mutex);

	ret = wait_event_interruptible_timeout(ccid->open_wq,
			ccid->intf != NULL, msecs_to_jiffies(
				CCID_BRIDGE_OPEN_TIMEOUT));

	mutex_lock(&ccid->open_mutex);

	if (ret != -ERESTARTSYS && ccid->intf) {
		fp->private_data = ccid;
		ccid->opened = true;
		ret = 0;
	} else if (!ret) { /* timed out */
		ret = -ENODEV;
	}
out:
	mutex_unlock(&ccid->open_mutex);
	pr_debug("returning %d", ret);
	return ret;
}

static ssize_t ccid_bridge_write(struct file *fp, const char __user *ubuf,
				 size_t count, loff_t *pos)
{
	struct ccid_bridge *ccid = fp->private_data;
	int ret;
	char *kbuf;

	pr_debug("called with %d", count);

	if (!ccid->intf) {
		pr_debug("intf is not active");
		return -ENODEV;
	}

	mutex_lock(&ccid->write_mutex);

	if (!count || count > CCID_BRIDGE_MSG_SZ) {
		pr_err("invalid count");
		ret = -EINVAL;
		goto out;
	}

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf) {
		pr_err("fail to allocate memory");
		ret = -ENOMEM;
		goto out;
	}

	ret = copy_from_user(kbuf, ubuf, count);
	if (ret) {
		pr_err("fail to copy user buf");
		ret = -EFAULT;
		goto free_kbuf;
	}

	ret = usb_autopm_get_interface(ccid->intf);
	if (ret) {
		pr_err("fail to get autopm with %d", ret);
		goto free_kbuf;
	}

	ccid->write_result = 0;

	usb_fill_bulk_urb(ccid->writeurb, ccid->udev, ccid->out_pipe,
			kbuf, count, ccid_bridge_out_cb, ccid);
	ret = usb_submit_urb(ccid->writeurb, GFP_KERNEL);
	if (ret < 0) {
		pr_err("urb submit fail with %d", ret);
		goto put_pm;
	}

	ret = wait_event_interruptible_timeout(ccid->write_wq,
			ccid->write_result != 0,
			msecs_to_jiffies(CCID_BRIDGE_MSG_TIMEOUT));
	if (!ret || ret == -ERESTARTSYS) { /* timedout or interrupted */
		usb_kill_urb(ccid->writeurb);
		if (!ret)
			ret = -ETIMEDOUT;
	} else {
		ret = ccid->write_result;
	}

	pr_debug("returning %d", ret);

put_pm:
	if (ret != -ENODEV)
		usb_autopm_put_interface(ccid->intf);
free_kbuf:
	kfree(kbuf);
out:
	mutex_unlock(&ccid->write_mutex);
	return ret;

}

static ssize_t ccid_bridge_read(struct file *fp, char __user *ubuf,
				 size_t count, loff_t *pos)
{
	struct ccid_bridge *ccid = fp->private_data;
	int ret;
	char *kbuf;

	pr_debug("called with %d", count);
	if (!ccid->intf) {
		pr_debug("intf is not active");
		return -ENODEV;
	}

	mutex_lock(&ccid->read_mutex);

	if (!count || count > CCID_BRIDGE_MSG_SZ) {
		pr_err("invalid count");
		ret = -EINVAL;
		goto out;
	}

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf) {
		pr_err("fail to allocate memory");
		ret = -ENOMEM;
		goto out;
	}

	ret = usb_autopm_get_interface(ccid->intf);
	if (ret) {
		pr_err("fail to get autopm with %d", ret);
		goto free_kbuf;
	}

	ccid->read_result = 0;

	usb_fill_bulk_urb(ccid->readurb, ccid->udev, ccid->in_pipe,
			kbuf, count, ccid_bridge_in_cb, ccid);
	ret = usb_submit_urb(ccid->readurb, GFP_KERNEL);
	if (ret < 0) {
		pr_err("urb submit fail with %d", ret);
		if (ret != -ENODEV)
			usb_autopm_put_interface(ccid->intf);
		goto free_kbuf;
	}


	ret = wait_event_interruptible_timeout(ccid->read_wq,
			ccid->read_result != 0,
			msecs_to_jiffies(CCID_BRIDGE_MSG_TIMEOUT));
	if (!ret || ret == -ERESTARTSYS) { /* timedout or interrupted */
		usb_kill_urb(ccid->readurb);
		if (!ret)
			ret = -ETIMEDOUT;
	} else {
		ret = ccid->read_result;
	}


	if (ret > 0) {
		if (copy_to_user(ubuf, kbuf, ret))
			ret = -EFAULT;
	}

	usb_autopm_put_interface(ccid->intf);
	pr_debug("returning %d", ret);

free_kbuf:
	kfree(kbuf);
out:
	mutex_unlock(&ccid->read_mutex);
	return ret;
}

static long
ccid_bridge_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct ccid_bridge *ccid = fp->private_data;
	char *buf;
	struct usb_ccid_data data;
	struct usb_ccid_abort abort;
	struct usb_descriptor_header *header;
	int ret;
	struct usb_device *udev = ccid->udev;
	__u8 intf = ccid->intf->cur_altsetting->desc.bInterfaceNumber;
	__u8 breq = 0;

	if (!ccid->intf) {
		pr_debug("intf is not active");
		return -ENODEV;
	}

	mutex_lock(&ccid->event_mutex);
	switch (cmd) {
	case USB_CCID_GET_CLASS_DESC:
		pr_debug("GET_CLASS_DESC ioctl called");
		ret = copy_from_user(&data, (void __user *)arg, sizeof(data));
		if (ret) {
			ret = -EFAULT;
			break;
		}
		ret = __usb_get_extra_descriptor(udev->rawdescriptors[0],
				le16_to_cpu(udev->config[0].desc.wTotalLength),
				CCID_CLASS_DECRIPTOR_TYPE, (void **) &buf);
		if (ret) {
			ret = -ENOENT;
			break;
		}
		header = (struct usb_descriptor_header *) buf;
		if (data.length != header->bLength) {
			ret = -EINVAL;
			break;
		}
		ret = copy_to_user((void __user *)data.data, buf, data.length);
		if (ret)
			ret = -EFAULT;
		break;
	case USB_CCID_GET_CLOCK_FREQUENCIES:
		pr_debug("GET_CLOCK_FREQUENCIES ioctl called");
		breq = CCID_GET_CLK_FREQ_REQ;
		/* fall through */
	case USB_CCID_GET_DATA_RATES:
		if (!breq) {
			pr_debug("GET_DATA_RATES ioctl called");
			breq = CCID_GET_DATA_RATES;
		}
		ret = copy_from_user(&data, (void __user *)arg, sizeof(data));
		if (ret) {
			ret = -EFAULT;
			break;
		}
		buf = kmalloc(data.length, GFP_KERNEL);
		if (!buf) {
			ret = -ENOMEM;
			break;
		}
		ret = usb_autopm_get_interface(ccid->intf);
		if (ret < 0) {
			pr_debug("fail to get autopm with %d", ret);
			break;
		}
		ret = usb_control_msg(ccid->udev,
				usb_rcvctrlpipe(ccid->udev, 0),
				breq, (USB_DIR_IN | USB_TYPE_CLASS |
				 USB_RECIP_INTERFACE), 0, intf, buf,
				data.length, CCID_CONTROL_TIMEOUT);
		usb_autopm_put_interface(ccid->intf);
		if (ret == data.length) {
			ret = copy_to_user((void __user *)data.data, buf,
					data.length);
			if (ret)
				ret = -EFAULT;
		} else {
			if (ret > 0)
				ret = -EPIPE;
		}
		kfree(buf);
		break;
	case USB_CCID_ABORT:
		pr_debug("ABORT ioctl called");
		breq = CCID_ABORT_REQ;
		ret = copy_from_user(&abort, (void __user *)arg, sizeof(abort));
		if (ret) {
			ret = -EFAULT;
			break;
		}
		ret = usb_autopm_get_interface(ccid->intf);
		if (ret < 0) {
			pr_debug("fail to get autopm with %d", ret);
			break;
		}
		ret = usb_control_msg(ccid->udev,
				usb_sndctrlpipe(ccid->udev, 0),
				breq, (USB_DIR_OUT | USB_TYPE_CLASS |
				 USB_RECIP_INTERFACE),
				(abort.seq << 8) | abort.slot, intf, NULL,
				0, CCID_CONTROL_TIMEOUT);
		if (ret < 0)
			pr_err("abort request failed with err %d\n", ret);
		usb_autopm_put_interface(ccid->intf);
		break;
	case USB_CCID_GET_EVENT:
		pr_debug("GET_EVENT ioctl called");
		if (!ccid->events_supported) {
			ret = -ENOENT;
			break;
		}
		ret = ccid_bridge_get_event(ccid);
		if (ret == 0) {
			ret = copy_to_user((void __user *)arg, &ccid->cur_event,
					sizeof(ccid->cur_event));
			if (ret)
				ret = -EFAULT;
		}
		ccid->cur_event.event = 0;
		break;
	default:
		pr_err("UNKNOWN ioctl called");
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&ccid->event_mutex);
	pr_debug("returning %d", ret);
	return ret;
}

static int ccid_bridge_release(struct inode *ip, struct file *fp)
{
	struct ccid_bridge *ccid = fp->private_data;

	pr_debug("called");

	usb_kill_urb(ccid->writeurb);
	usb_kill_urb(ccid->readurb);
	if (ccid->int_pipe)
		usb_kill_urb(ccid->inturb);

	ccid->event_result = -EIO;
	wake_up(&ccid->event_wq);

	mutex_lock(&ccid->open_mutex);
	ccid->opened = false;
	mutex_unlock(&ccid->open_mutex);
	return 0;
}

static const struct file_operations ccid_bridge_fops = {
	.owner = THIS_MODULE,
	.open = ccid_bridge_open,
	.write = ccid_bridge_write,
	.read = ccid_bridge_read,
	.unlocked_ioctl = ccid_bridge_ioctl,
	.release = ccid_bridge_release,
};

static int ccid_bridge_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct ccid_bridge *ccid = usb_get_intfdata(intf);
	int ret = 0;

	pr_debug("called");

	if (!ccid->opened)
		goto out;

	mutex_lock(&ccid->event_mutex);
	if (ccid->int_pipe) {
		usb_kill_urb(ccid->inturb);
		if (ccid->event_result != -ENOENT) {
			ret = -EBUSY;
			goto rel_mutex;
		}
	}

	ccid->is_suspended = true;
rel_mutex:
	mutex_unlock(&ccid->event_mutex);
out:
	pr_debug("returning %d", ret);
	return ret;
}

static int ccid_bridge_resume(struct usb_interface *intf)
{
	struct ccid_bridge *ccid = usb_get_intfdata(intf);
	int ret;

	pr_debug("called");

	if (!ccid->opened)
		goto out;

	mutex_lock(&ccid->event_mutex);

	ccid->is_suspended = false;

	if (device_can_wakeup(&ccid->udev->dev)) {
		ccid->event_result = 0;
		ccid->cur_event.event = USB_CCID_RESUME_EVENT;
		wake_up(&ccid->event_wq);
	} else if (ccid->int_pipe) {
		ccid->event_result = -EINPROGRESS;
		ret = usb_submit_urb(ccid->inturb, GFP_KERNEL);
		if (ret < 0)
			pr_debug("fail to submit inturb with %d\n", ret);
	}

	mutex_unlock(&ccid->event_mutex);
out:
	return 0;
}

static int
ccid_bridge_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct ccid_bridge *ccid = __ccid_bridge_dev;
	struct usb_host_interface *intf_desc;
	struct usb_endpoint_descriptor *ep_desc;
	struct usb_host_endpoint *ep;
	__u8 epin_addr = 0, epout_addr = 0, epint_addr = 0;
	int i, ret;

	intf_desc = intf->cur_altsetting;

	if (intf_desc->desc.bNumEndpoints > 3)
		return -ENODEV;

	for (i = 0; i < intf_desc->desc.bNumEndpoints; i++) {
		ep_desc = &intf_desc->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(ep_desc))
			epin_addr = ep_desc->bEndpointAddress;
		else if (usb_endpoint_is_bulk_out(ep_desc))
			epout_addr = ep_desc->bEndpointAddress;
		else if (usb_endpoint_is_int_in(ep_desc))
			epint_addr = ep_desc->bEndpointAddress;
		else
			return -ENODEV;
	}

	if (!epin_addr || !epout_addr)
		return -ENODEV;

	ccid->udev = usb_get_dev(interface_to_usbdev(intf));
	ccid->in_pipe = usb_rcvbulkpipe(ccid->udev, epin_addr);
	ccid->out_pipe = usb_sndbulkpipe(ccid->udev, epout_addr);
	if (epint_addr)
		ccid->int_pipe = usb_rcvbulkpipe(ccid->udev, epint_addr);

	ccid->writeurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ccid->writeurb) {
		pr_err("fail to allocate write urb");
		ret = -ENOMEM;
		goto put_udev;
	}
	ccid->readurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!ccid->readurb) {
		pr_err("fail to allocate read urb");
		ret = -ENOMEM;
		goto free_writeurb;
	}

	if (ccid->int_pipe) {
		pr_debug("interrupt endpoint is present");
		ep = usb_pipe_endpoint(ccid->udev, ccid->int_pipe);
		ccid->inturb = usb_alloc_urb(0, GFP_KERNEL);
		if (!ccid->inturb) {
			pr_err("fail to allocate int urb");
			ret = -ENOMEM;
			goto free_readurb;
		}
		ccid->intbuf = kmalloc(usb_endpoint_maxp(&ep->desc),
				GFP_KERNEL);
		if (!ccid->intbuf) {
			pr_err("fail to allocated int buf");
			ret = -ENOMEM;
			goto free_inturb;
		}
		usb_fill_int_urb(ccid->inturb, ccid->udev,
				usb_rcvintpipe(ccid->udev, epint_addr),
				ccid->intbuf, usb_endpoint_maxp(&ep->desc),
				ccid_bridge_int_cb, ccid,
				ep->desc.bInterval);
	}

	if (ccid->int_pipe || device_can_wakeup(&ccid->udev->dev)) {
		pr_debug("event support is present");
		ccid->events_supported = true;
	}

	usb_set_intfdata(intf, ccid);

	mutex_lock(&ccid->open_mutex);
	ccid->intf = intf;
	wake_up(&ccid->open_wq);
	mutex_unlock(&ccid->open_mutex);

	pr_info("success");
	return 0;

free_inturb:
	if (ccid->int_pipe)
		usb_free_urb(ccid->inturb);
free_readurb:
	usb_free_urb(ccid->readurb);
free_writeurb:
	usb_free_urb(ccid->writeurb);
put_udev:
	usb_put_dev(ccid->udev);
	return ret;
}

static void ccid_bridge_disconnect(struct usb_interface *intf)
{
	struct ccid_bridge *ccid = usb_get_intfdata(intf);

	pr_debug("called");

	usb_kill_urb(ccid->writeurb);
	usb_kill_urb(ccid->readurb);
	if (ccid->int_pipe)
		usb_kill_urb(ccid->inturb);

	ccid->event_result = -ENODEV;
	wake_up(&ccid->event_wq);

	/*
	 * This would synchronize any ongoing read/write/ioctl.
	 * After acquiring the mutex, we can safely set
	 * intf to NULL.
	 */
	mutex_lock(&ccid->open_mutex);
	mutex_lock(&ccid->write_mutex);
	mutex_lock(&ccid->read_mutex);
	mutex_lock(&ccid->event_mutex);

	usb_free_urb(ccid->writeurb);
	usb_free_urb(ccid->readurb);
	if (ccid->int_pipe) {
		usb_free_urb(ccid->inturb);
		kfree(ccid->intbuf);
		ccid->int_pipe = 0;
	}

	ccid->intf = NULL;

	mutex_unlock(&ccid->event_mutex);
	mutex_unlock(&ccid->read_mutex);
	mutex_unlock(&ccid->write_mutex);
	mutex_unlock(&ccid->open_mutex);

}

static const struct usb_device_id ccid_bridge_ids[] = {
	{ USB_INTERFACE_INFO(USB_CLASS_CSCID, 0, 0) },

	{} /* terminating entry */
};
MODULE_DEVICE_TABLE(usb, ccid_bridge_ids);

static struct usb_driver ccid_bridge_driver = {
	.name = "ccid_bridge",
	.probe = ccid_bridge_probe,
	.disconnect = ccid_bridge_disconnect,
	.suspend = ccid_bridge_suspend,
	.resume = ccid_bridge_resume,
	.id_table = ccid_bridge_ids,
	.supports_autosuspend = 1,
};

static int __init ccid_bridge_init(void)
{
	int ret;
	struct ccid_bridge *ccid;

	ccid = kzalloc(sizeof(*ccid), GFP_KERNEL);
	if (!ccid) {
		pr_err("Fail to allocate ccid");
		ret = -ENOMEM;
		goto out;
	}
	__ccid_bridge_dev = ccid;

	mutex_init(&ccid->open_mutex);
	mutex_init(&ccid->write_mutex);
	mutex_init(&ccid->read_mutex);
	mutex_init(&ccid->event_mutex);

	init_waitqueue_head(&ccid->open_wq);
	init_waitqueue_head(&ccid->write_wq);
	init_waitqueue_head(&ccid->read_wq);
	init_waitqueue_head(&ccid->event_wq);

	ret = usb_register(&ccid_bridge_driver);
	if (ret < 0) {
		pr_err("Fail to register ccid usb driver with %d", ret);
		goto free_ccid;
	}

	ret = alloc_chrdev_region(&ccid->chrdev, 0, 1, "ccid_bridge");
	if (ret < 0) {
		pr_err("Fail to allocate ccid char dev region with %d", ret);
		goto unreg_driver;
	}
	ccid->class = class_create(THIS_MODULE, "ccid_bridge");
	if (IS_ERR(ccid->class)) {
		ret = PTR_ERR(ccid->class);
		pr_err("Fail to create ccid class with %d", ret);
		goto unreg_chrdev;
	}
	cdev_init(&ccid->cdev, &ccid_bridge_fops);
	ccid->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ccid->cdev, ccid->chrdev, 1);
	if (ret < 0) {
		pr_err("Fail to add ccid cdev with %d", ret);
		goto destroy_class;
	}
	ccid->device = device_create(ccid->class,
					NULL, ccid->chrdev, NULL,
					"ccid_bridge");
	if (IS_ERR(ccid->device)) {
		ret = PTR_ERR(ccid->device);
		pr_err("Fail to create ccid device with %d", ret);
		goto del_cdev;
	}

	pr_info("success");

	return 0;

del_cdev:
	cdev_del(&ccid->cdev);
destroy_class:
	class_destroy(ccid->class);
unreg_chrdev:
	unregister_chrdev_region(ccid->chrdev, 1);
unreg_driver:
	usb_deregister(&ccid_bridge_driver);
free_ccid:
	mutex_destroy(&ccid->open_mutex);
	mutex_destroy(&ccid->write_mutex);
	mutex_destroy(&ccid->read_mutex);
	mutex_destroy(&ccid->event_mutex);
	kfree(ccid);
	__ccid_bridge_dev = NULL;
out:
	return ret;
}

static void __exit ccid_bridge_exit(void)
{
	struct ccid_bridge *ccid = __ccid_bridge_dev;

	pr_debug("called");
	device_destroy(ccid->class, ccid->chrdev);
	cdev_del(&ccid->cdev);
	class_destroy(ccid->class);
	unregister_chrdev_region(ccid->chrdev, 1);

	usb_deregister(&ccid_bridge_driver);

	mutex_destroy(&ccid->open_mutex);
	mutex_destroy(&ccid->write_mutex);
	mutex_destroy(&ccid->read_mutex);
	mutex_destroy(&ccid->event_mutex);

	kfree(ccid);
	__ccid_bridge_dev = NULL;
}

module_init(ccid_bridge_init);
module_exit(ccid_bridge_exit);

MODULE_DESCRIPTION("USB CCID bridge driver");
MODULE_LICENSE("GPL v2");

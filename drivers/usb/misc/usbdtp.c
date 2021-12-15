/*
 * USB Driver for DTP
 *
 * Copyright (C) 2020 xiaomi, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Author: Deng yong jian <dengyongjian@xiaomi.com>
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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/file.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/usb/f_dtp.h>

#define MAX_INTERFACE_NUM 64
#define MAX_USB_CONCURRENT_WRITES 5
#define MAX_TRANSMISSION_SIZE (48*1024*1024) /*48M*/
#define DRIVER_NAME "dtp"

/*class request */
#define REQUEST_SET_DEVICE_STATUS	0x13
#define REQUEST_QUERY_DEVICE_STATUS	0x14

#define log_dbg(fmt, ...) do {\
	pr_debug("%s: {%s} " fmt, DRIVER_NAME, __func__, ##__VA_ARGS__);\
}while (0)

#define log_info(fmt, ...) do {\
	pr_info("%s: {%s} " fmt, DRIVER_NAME, __func__, ##__VA_ARGS__);\
}while (0)

#define log_err(fmt, ...) do {\
	pr_err("%s: {%s} " fmt, DRIVER_NAME, __func__, ##__VA_ARGS__);\
}while (0)

/*enum value for device state */
enum {
	e_offline = 0,
	e_ready,
	e_busy,
	e_cancel,
	e_error,
};

struct usb_dtp {
	struct usb_device *udev;/*Represent a usb device*/
	struct usb_interface *interface;/*Represent a usb interface*/
	struct usb_endpoint_descriptor *bulk_in;/*Represent usb endpoints*/
	struct usb_endpoint_descriptor *bulk_out;
	unsigned char *bulk_in_buffer;	/*the buffer to receive data*/
	unsigned char *ctrl_ep_buffer;
	size_t bulk_in_size;/*the length of bulk in buffer*/
	size_t bulk_out_size;/*the lenght of bulk out buffer*/
	struct mutex pm_mutex;  /*serialize access to open/suspend*/
	atomic_t refcnt;
	atomic_t ioctl_refcnt;

	struct workqueue_struct *wq;
	struct work_struct send_file_work;
	struct work_struct receive_file_work;

	struct file *xfer_file;
	loff_t xfer_file_offset;
	int64_t xfer_file_length;
	int xfer_result;

	struct semaphore	limit_sem;/*to stop writes at full throttle from using up all RAM*/
	struct usb_anchor	submitted;/*URBs to wait for before suspend*/
	struct rw_semaphore	io_rwsem;
	int disconnected:1;
};

static struct usb_dtp *_usb_dtp = NULL;
static const char device_name[] = "usb_" DRIVER_NAME;

static const struct usb_device_id id_tables[] = {
	{.match_flags = USB_DEVICE_ID_MATCH_INT_NUMBER|USB_DEVICE_ID_MATCH_INT_CLASS|USB_DEVICE_ID_MATCH_INT_SUBCLASS,
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = USB_SUBCLASS_VENDOR_SPEC,
		.bInterfaceNumber = MAX_INTERFACE_NUM,
	},
	{ }
};

static inline struct usb_dtp *get_usb_dtp(void)
{
	return _usb_dtp;
}

static int send_ctrlrequest(struct usb_device *udev, __u8 request, __u8 type, __u16 value, void *data, __u16 size)
{
	log_dbg("value %d\n", value);
	return usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			request,		/* __u8 request      */
			type,			/* __u8 request type */
			value,			/* __u16 value       */
			0x0000,			/* __u16 index       */
			data,			/* void *data        */
			size,			/* __u16 size 	     */
			USB_CTRL_SET_TIMEOUT);	/* int timeout       */
}

static int set_devicet_status(struct usb_dtp *drv, u16 status)
{
	int *data= (int*)drv->ctrl_ep_buffer;
	int size = sizeof(int);
	int ret = 0;

	ret = send_ctrlrequest(drv->udev, REQUEST_SET_DEVICE_STATUS,  USB_TYPE_VENDOR | USB_DIR_IN, status, data, size);
	log_dbg("ret %d, data 0x%x\n", ret, *data);

	if (ret == size)
		return *data;

	return -1;
}

static inline int atomic_try_down(atomic_t *refcnt)
{
	if (atomic_inc_return(refcnt) == 1) {
		return 0;
	} else {
		atomic_dec(refcnt);
		return -1;
	}
}

static inline void atomic_up(atomic_t *refcnt)
{
	atomic_dec(refcnt);
}

static ssize_t usb_read(struct file *fp, char __user *userbuf, size_t count, loff_t *pos)
{
	struct usb_dtp *drv = fp->private_data;
	int bytes_read = 0;
	int max_loop = 0;
	int recvZLP = 1;
	int xfer_copy = 0;
	int ret = 0;
	int xfer = 0;


	down_read(&drv->io_rwsem);

	if (drv->disconnected) {
		ret = -ENODEV;
		goto fail;
	}

	log_dbg("count %d\n", count);

	max_loop = MAX_TRANSMISSION_SIZE / drv->bulk_in_size;
	log_dbg("bulk in size: %d, max loop: %d\n", drv->bulk_in_size, max_loop);

	while(((count > 0) && (max_loop -- > 0)) || recvZLP) {

		/* do a blocking bulk read to get data from the device */
		bytes_read = 0;
		xfer = drv->bulk_in_size;

		if (count <= 0)
			recvZLP = 0;
		else if (count < drv->bulk_in_size)
			xfer = count;

		ret = usb_bulk_msg(drv->udev,
				      usb_rcvbulkpipe(drv->udev, drv->bulk_in->bEndpointAddress),
				      drv->bulk_in_buffer,
				      xfer,
				      &bytes_read,
				      5000);//wait for 5s, 0 for wait forever

		if (ret < 0) {
			log_err("bulk msg failed, ret(%d)\n", ret);
			break;
		}

		/* if the read was successful, copy the data to userspace */
		if (!ret) {
			if (!bytes_read) {
				log_dbg("got EOF sig\n");
				break;
			}
			if (copy_to_user(userbuf + xfer_copy, drv->bulk_in_buffer, bytes_read)) {
				log_err("copy to user failed\n");
				ret = -EFAULT;
				break;
			} else {
				count -= bytes_read;
				xfer_copy += bytes_read;
			}
		}


	}

fail:
	up_read(&drv->io_rwsem);

	log_dbg("returning %d, ret %d\n", xfer_copy, ret);

	return (ret < 0) ? ret : xfer_copy;
}

static void usb_dtp_ioctl_write_bulk_callback(struct urb *urb)
{
	int status = urb->status;

	/*sync/async unlink faults aren't errors*/
	if (status &&
	    !(status == -ENOENT ||
	      status == -ECONNRESET ||
	      status == -ESHUTDOWN)) {
		log_dbg("nonzero bulk status received: %d\n", status);
	}

	log_dbg("bulk status received: %d\n", status);

	/*free up our allocated buffer */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
}
static void usb_dtp_write_bulk_callback(struct urb *urb)
{
	/*struct usb_dtp *drv = urb->context;*/
	int status = urb->status;

	/*sync/async unlink faults aren't errors*/
	if (status &&
	    !(status == -ENOENT ||
	      status == -ECONNRESET ||
	      status == -ESHUTDOWN)) {
		log_dbg("nonzero bulk status received: %d\n", status);
	}

	log_dbg("bulk status received: %d\n", status);

	/*free up our allocated buffer*/
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);
	/*up(&drv->limit_sem);*/
}

static ssize_t usb_write(struct file *fp, const char __user *user_buf, size_t count, loff_t *pos)
{
	struct usb_dtp *drv = fp->private_data;
	struct urb *urb = NULL;
	size_t writed_count = 0;
	size_t bulk_size = 0;
	char *buf = NULL;
	int max_loop = 0;
	int xfer = 0;
	int ret = 0;
	ktime_t start_time;
	unsigned long delta;

	start_time = ktime_get();

	if (count == 0)
		return -EINVAL;

	log_dbg("(%zu)\n", count);

	if (down_interruptible(&drv->limit_sem) < 0) {
		log_err("limited staff, please try again later\n");
		return -EINTR;
	}

	down_read(&drv->io_rwsem);

	if (drv->disconnected) {
		ret = -ENODEV;
		log_err("drv is disconnected\n");
		goto err1;
	}

	/*bulk size*/
	bulk_size = drv->bulk_out_size;
	max_loop = MAX_TRANSMISSION_SIZE / bulk_size;
	log_dbg("bulk size: %d, max loop: %d\n", bulk_size, max_loop);

	while((writed_count < count) && (max_loop-- > 0)) {

		if ((count - writed_count) > bulk_size)
			xfer = bulk_size;
		else
			xfer = count- writed_count;

		/*create a urb, and a buffer for it, and copy the data to the urb */
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			ret = -ENOMEM;
			log_err("alloc urb failed\n");
			goto err1;
		}

		buf = usb_alloc_coherent(drv->udev, xfer, GFP_KERNEL, &urb->transfer_dma);
		if (!buf) {
			ret = -ENOMEM;
			log_err("alloc buf failed\n");
			goto err2;
		}

		if (copy_from_user(buf, user_buf + writed_count, xfer)) {
			ret = -EFAULT;
			log_err("copy failed\n");
			goto err3;
		}

		/*initialize the urb properly*/
		usb_fill_bulk_urb(urb, drv->udev,
				  usb_sndbulkpipe(drv->udev, drv->bulk_out->bEndpointAddress),
				  buf, xfer, usb_dtp_write_bulk_callback, drv);
		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		usb_anchor_urb(urb, &drv->submitted);

		/*send the data out the bulk port*/
		ret = usb_submit_urb(urb, GFP_KERNEL);
		if (ret) {
			log_err("submit urb failed, ret(%d)\n", ret);
			goto err4;
		}

		/*release our reference to this urb,
		   the USB core will eventually free it entirely */
		usb_free_urb(urb);
		writed_count += xfer;

	}

	up_read(&drv->io_rwsem);
	up(&drv->limit_sem);

	delta = ktime_to_ms(ktime_sub(ktime_get(), start_time));
	log_dbg("count:%d, (delta: %dms)\n", writed_count, delta);
	return writed_count;
err4:
	usb_unanchor_urb(urb);
err3:
	usb_free_coherent(drv->udev, xfer, buf, urb->transfer_dma);
err2:
	usb_free_urb(urb);
err1:
	up_read(&drv->io_rwsem);
	up(&drv->limit_sem);
	return ret;
}

static void send_file_work(struct work_struct *data)
{
	struct usb_dtp *drv = container_of(data, struct usb_dtp, send_file_work);
	struct file *filp = NULL;
	struct urb *urb = NULL;
	int64_t count = 0;
	char *buf = NULL;
	int retval = 0;
	int xfer = 0;
	int ret = 0;
	int r = 0;
	loff_t offset = 0;

	/*read our parameters*/
	smp_rmb();
	filp = drv->xfer_file;
	offset = drv->xfer_file_offset;
	count = drv->xfer_file_length;

	log_dbg("(%lld %lld)\n", offset, count);

	if (count == 0) {
		retval = -EINVAL;
		goto out;
	}

	r = down_interruptible(&drv->limit_sem);
	if (r < 0) {
		retval = -EINTR;
		log_err("limited staff, please try later!\n");
		goto out;
	}

	down_read(&drv->io_rwsem);

	if (drv->disconnected) {
		retval = -ENODEV;
		log_err("drv is disconnected\n");
		goto err1;
	}


	while (count > 0) {

		xfer = (drv->bulk_out_size < count)?drv->bulk_out_size:count;

		/*create a urb, and a buffer for it, and copy the data to the urb*/
		urb = usb_alloc_urb(0, GFP_KERNEL);
		if (!urb) {
			retval = -ENOMEM;
			log_err("Unable to alloc urb\n");
			goto err1;
		}

		buf = usb_alloc_coherent(drv->udev, xfer, GFP_KERNEL, &urb->transfer_dma);
		if (!buf) {
			retval = -ENOMEM;
			log_err("Unable to alloc buf\n");
			goto err2;
		}

		/*ret = vfs_read(filp, buf, xfer, &offset);*/
		ret = -1;/*no used*/
		if (ret < 0) {
			retval = ret;
			goto err3;
		}

		xfer = ret;


		/*initialize the urb properly*/
		usb_fill_bulk_urb(urb, drv->udev,
				  usb_sndbulkpipe(drv->udev,
						  drv->bulk_out->bEndpointAddress),
				  buf, xfer, usb_dtp_ioctl_write_bulk_callback, drv);
		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		usb_anchor_urb(urb, &drv->submitted);

		/*send the data out the bulk port*/
		retval = usb_submit_urb(urb, GFP_KERNEL);
		if (retval) {
			log_err("submit urb failed, ret(%d)\n", retval);
			goto err4;
		}

		/*release our reference to this urb,
		   the USB core will eventually free it entirely*/
		usb_free_urb(urb);
		count -= xfer;


	}
	up_read(&drv->io_rwsem);
	up(&drv->limit_sem);
out:
	drv->xfer_result = retval;
	smp_wmb();
	log_dbg("send_file_work count=%d\n", count);
	return;
err4:
	usb_unanchor_urb(urb);
err3:
	usb_free_coherent(drv->udev, count, buf, urb->transfer_dma);
err2:
	usb_free_urb(urb);
err1:
	up_read(&drv->io_rwsem);
	up(&drv->limit_sem);
	drv->xfer_result = retval;
	smp_wmb();
	return;
}

static void receive_file_work(struct work_struct *data)
{
	struct usb_dtp *drv = container_of(data, struct usb_dtp, receive_file_work);
	struct file *filp = NULL;
	size_t count = 0;
	int bytes_read = 0;
	int retval = 0;
	int ret = 0;
	int r = 0;
	loff_t offset = 0;

	/*read our parameters*/
	smp_rmb();

	filp = drv->xfer_file;
	offset = drv->xfer_file_offset;
	count = drv->xfer_file_length;

	log_dbg( "(%lld)\n", count);

	down_read(&drv->io_rwsem);

	if (drv->disconnected) {
		retval = -ENODEV;
		goto fail;
	}

	while (count > 0) {
		/*do a blocking bulk read to get data from the device*/
		retval = usb_bulk_msg(drv->udev,
				      usb_rcvbulkpipe(drv->udev, drv->bulk_in->bEndpointAddress),
				      drv->bulk_in_buffer,
				      min(drv->bulk_in_size, count),
				      &bytes_read,
				      /*10000*/0);//wait forever

		/*if the read was successful, copy the data to userspace*/
		if (!retval) {
			/*ret = vfs_write(filp, drv->bulk_in_buffer, bytes_read, &offset);*/
			ret = -1;/*no used*/
			if (ret != bytes_read) {
				retval = -EIO;
				break;
			} else
				retval = bytes_read;
		}
		count -= bytes_read;

	}

fail:
	up_read(&drv->io_rwsem);
	drv->xfer_result = r;
	smp_wmb();
}

static long send_receive_ioctl(struct file *fp, unsigned int code, struct dtp_file_desc *fdesc)
{
	struct usb_dtp *drv = fp->private_data;
	struct work_struct *work = NULL;
	struct file *filp = NULL;
	int ret = -EINVAL;


	if (atomic_try_down(&drv->ioctl_refcnt)) {
		log_err("ioctl is busy\n");
		return -EBUSY;
	}


	/*hold a reference to the file while we are working with it*/
	filp = fget(fdesc->fd);
	if (!filp) {
		ret = -EBADF;
		log_err("get file failed\n");
		goto fail;
	}

	/*write the parameters*/
	drv->xfer_file = filp;
	drv->xfer_file_offset = fdesc->offset;
	drv->xfer_file_length = fdesc->length;
	/*make sure write is done before parameters are read*/
	smp_wmb();

	if (code == DTP_SEND_FILE) {
		work = &drv->send_file_work;
	} else {
		work = &drv->receive_file_work;
	}

	/* We do the file transfer on a work queue so it will run
	 * in kernel context, which is necessary for vfs_read and
	 * vfs_write to use our buffers in the kernel address space.
	 */
	queue_work(drv->wq, work);
	/*wait for operation to complete*/
	flush_workqueue(drv->wq);
	fput(filp);

	/*read the result*/
	smp_rmb();
	ret = drv->xfer_result;

fail:
	atomic_up(&drv->ioctl_refcnt);
	log_dbg("ioctl returning %d\n", ret);
	return ret;
}

static long usb_ioctl(struct file *fp, unsigned int code, unsigned long value)
{
	struct dtp_file_desc fdesc;
	int ret = -EINVAL;

	switch (code) {
	case DTP_SEND_FILE:
	case DTP_RECEIVE_FILE:
		if (copy_from_user(&fdesc, (void __user *)value, sizeof(fdesc))) {
			ret = -EFAULT;
			log_err("copy fdesc failed\n");
			goto fail;
		}
		ret = send_receive_ioctl(fp, code, &fdesc);
		break;
	default:
		log_dbg( "unknown ioctl code: %d\n", code);
		break;
	}
fail:
	return ret;
}

#ifdef CONFIG_COMPAT
static long compat_usb_ioctl(struct file *fp, unsigned int code, unsigned long value)
{
	/*no used*/
	return 0;
}
#endif

static int usb_open(struct inode *ip, struct file *fp)
{
	struct usb_dtp *drv = get_usb_dtp();

	log_dbg("open\n");
	if (atomic_try_down(&drv->refcnt)) {
		log_info("drv is busy! please check! Is it exited abnormally or not closed?\n");
	}

	fp->private_data = drv;
	return 0;
}

static int usb_release(struct inode *ip, struct file *fp)
{
	struct usb_dtp *drv = get_usb_dtp();

	log_dbg("release\n");
	atomic_up(&drv->refcnt);
	return 0;
}

static struct file_operations usb_fops = {
	.owner = THIS_MODULE,
	.read = usb_read,
	.write = usb_write,
	.unlocked_ioctl = usb_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_usb_ioctl,
#endif
	.open = usb_open,
	.release = usb_release,
};

static struct miscdevice usb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = device_name,
	.fops = &usb_fops,
};


static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id)
{

	struct usb_dtp *drv = NULL;
	int ret = -ENOMEM;



	drv = kzalloc(sizeof(*drv), GFP_KERNEL);
	if (!drv) {
		ret = -ENOMEM;
		log_err( "alloc drv failed\n");
		goto err1;
	}

	ret = usb_find_common_endpoints(interface->cur_altsetting, &drv->bulk_in, &drv->bulk_out, NULL, NULL);

	if (ret) {
		log_err( "cant find both bulk in and bulk out endpoints, ret(%d)\n", ret);
		goto err2;
	}

	sema_init(&drv->limit_sem, MAX_USB_CONCURRENT_WRITES);
	init_usb_anchor(&drv->submitted);
	init_rwsem(&drv->io_rwsem);
	mutex_init(&drv->pm_mutex);
	atomic_set(&drv->refcnt, 0);
	atomic_set(&drv->ioctl_refcnt, 0);

	drv->disconnected = 0;
	drv->interface = interface;
	drv->udev = usb_get_dev(interface_to_usbdev(interface));
	drv->bulk_in_size = usb_endpoint_maxp(drv->bulk_in);
	drv->bulk_out_size = usb_endpoint_maxp(drv->bulk_out);

	log_dbg("bulk_in_size(%d), bulk_out_size(%d)\n", drv->bulk_in_size, drv->bulk_out_size);

	drv->bulk_in_buffer = kmalloc(drv->bulk_in_size, GFP_KERNEL);

	if (!drv->bulk_in_buffer) {
		ret = -ENOMEM;
		log_err( "alloc bulk in buffer failed\n");
		goto err3;
	}

	drv->ctrl_ep_buffer = kmalloc(drv->bulk_in_size, GFP_KERNEL);

	if (!drv->ctrl_ep_buffer) {
		ret = -ENOMEM;
		log_err("alloc ctrl ep buffer failed\n");
		goto  err4;
	}

	drv->wq = create_singlethread_workqueue("usb_dtp");
	if (!drv->wq) {
		ret = -ENOMEM;
		log_err( "create work queue failed!\n");
		goto err5;
	}

	INIT_WORK(&drv->send_file_work, send_file_work);
	INIT_WORK(&drv->receive_file_work, receive_file_work);

	_usb_dtp = drv;

	if (set_devicet_status(drv, e_ready) != e_ready) {
		log_err("binding gadget device failed\n");
		drv->disconnected = 1;
	} else
		log_dbg("binding gadget device successful\n");

	ret = misc_register(&usb_device);
	if (ret) {
		log_err( "register misc device failed, ret(%d)\n", ret);
		goto err6;
	}

	log_info( "usb dtp probe successful\n");
	return 0;
err6:
	destroy_workqueue(drv->wq);
err5:
	kfree(drv->ctrl_ep_buffer);
err4:
	kfree(drv->bulk_in_buffer);
err3:
err2:
	kfree(drv);
err1:
	return ret;
}

static void usb_disconnect(struct usb_interface *intf)
{
	struct usb_dtp *drv = get_usb_dtp();

	log_dbg( "disconnect\n");

	down_write(&drv->io_rwsem);
	drv->disconnected = 1;
	up_write(&drv->io_rwsem);
	usb_kill_anchored_urbs(&drv->submitted);
	misc_deregister(&usb_device);
	destroy_workqueue(drv->wq);
	_usb_dtp = NULL;
	return;
}

static int usb_suspend (struct usb_interface *intf, pm_message_t message)
{
	struct usb_dtp *drv = get_usb_dtp();
	int time = 0;

	log_dbg("usb_suspend\n");

	if (!drv)
		return 0;

	time = usb_wait_anchor_empty_timeout(&drv->submitted, 1000);
	if (!time)
		usb_kill_anchored_urbs(&drv->submitted);
	return 0;
}

static int usb_resume(struct usb_interface *intf)
{

	log_dbg("resume\n");
	return 0;
}

MODULE_DEVICE_TABLE(usb, id_tables);

static struct usb_driver usb_dtp_driver = {
	.name		= device_name,
	.probe		= usb_probe,
	.disconnect	= usb_disconnect,
	.suspend	= usb_suspend,
	.resume		= usb_resume,
	.id_table	= id_tables,
	.supports_autosuspend = 1,
};

module_usb_driver(usb_dtp_driver);

MODULE_AUTHOR("Deng yongjian <dengyongjian@xiaomi.com>");
MODULE_DESCRIPTION("USB DTP Host Driver");
MODULE_LICENSE("GPL");


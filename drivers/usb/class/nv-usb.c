/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/time.h>

#include "nv-usb.h"

#ifdef DEBUG
#define INFO(stuff...)	pr_info("nv-usb: " stuff)
#else
#define INFO(stuff...)	do {} while (0)
#endif


#define NV_USB_BULK_VENDOR_ID 0x0955
#define NV_USB_BULK_PRODUCT_ID 0xffff

/* table of devices that work with this driver */
static const struct usb_device_id nv_usb_table[] = {
	{ USB_DEVICE(NV_USB_BULK_VENDOR_ID, NV_USB_BULK_PRODUCT_ID) },
	{}
};

MODULE_DEVICE_TABLE(usb, nv_usb_table);

/* Need to check if the below minor id is ok */
#define NV_USB_BULK_MINOR_BASE	192

static struct usb_driver nv_usb_driver;

static void nv_usb_delete(struct kref *kref)
{
	struct nv_usb *dev = to_nv_usb_dev(kref);

	usb_put_dev(dev->udev);
}

static int nv_usb_open(struct inode *inode, struct file *file)
{
	struct nv_usb *dev;
	struct usb_interface *interface;
	int subminor;

	subminor = iminor(inode);

	interface = usb_find_interface(&nv_usb_driver, subminor);
	if (interface == NULL) {
		INFO("%s(%d) failed to get interface\n", __func__, __LINE__);
		return -ENODEV;
	}

	dev = usb_get_intfdata(interface);
	if (dev == NULL) {
		INFO("%s(%d) failed to get interface\n", __func__, __LINE__);
		return -ENODEV;
	}

	kref_get(&dev->kref);

	mutex_lock(&dev->mutex);
	file->private_data = dev;
	mutex_unlock(&dev->mutex);

	return 0;
}

static int nv_usb_release(struct inode *inode, struct file *file)
{
	struct nv_usb *dev;

	dev = file->private_data;
	if (dev == NULL) {
		INFO("%s(%d) failed to get interface\n", __func__, __LINE__);
		return -ENODEV;
	}

	kref_put(&dev->kref, nv_usb_delete);
	return 0;
}

static int nv_usb_flush(struct file *file, fl_owner_t id)
{
	return 0;
}

static int nv_usb_transfer(struct bulk_data *data);

static ssize_t nv_usb_read(struct file *file, char *buffer, size_t count,
		loff_t *ppos)
{
	return 0;
}

unsigned long
nv_usb_calc_time(struct timeval g_sttime, struct timeval g_entime)
{

	unsigned int num_sec = g_entime.tv_sec - g_sttime.tv_sec;

	if (num_sec)
		return (g_entime.tv_usec +
			(1000000 - g_sttime.tv_usec) +
			((num_sec - 1) * 1000000));
	else
		return g_entime.tv_usec - g_sttime.tv_usec;

}

static void nv_usb_callback(struct urb *urb)
{
	struct completion *urb_done_ptr = urb->context;

	complete(urb_done_ptr);
}

static int nv_usb_transfer(struct bulk_data *data)
{
	struct nv_usb *dev = data->dev;
	struct nvusb_cb_wrap *bcb = NULL;
	struct nvusb_cs_wrap *bcs = NULL;
	struct urb *urb = NULL;
	unsigned int transfer_length = data->length;
	struct completion urb_done;
	int retval = 0;
	unsigned int cbwlen = US_BULK_CB_WRAP_LEN;
	unsigned int cswlen = US_BULK_CS_WRAP_LEN;
	char *buf = NULL;
	struct timeval g_sttime, g_entime;
	unsigned long data_transfer_time = 0;

	init_completion(&urb_done);

	mutex_lock(&dev->mutex);

	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		INFO("%s(%d) failed to alloc urb\n", __func__, __LINE__);
		return -ENOMEM;
	}

	bcb = usb_alloc_coherent(dev->udev, cbwlen, GFP_KERNEL,
			&urb->transfer_dma);

	if (!bcb) {
		INFO("%s(%d) failed to alloc bcb\n", __func__, __LINE__);
		retval = -ENOMEM;
		goto out;
	}

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->DataTransferLength = cpu_to_le32(transfer_length);
	/*0 --> write 1 --> read*/
	bcb->Flags = data->data_direction ? 1 << 7 : 0;
	bcb->Tag = ++dev->tag;
	bcb->Length = data->sub_cmd_length;

	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, data->sub_cmd, bcb->Length);

	if (!dev->interface) {
		INFO("%s(%d) interface disconnected\n", __func__, __LINE__);
		usb_free_coherent(dev->udev, cbwlen, bcb, urb->transfer_dma);
		retval = -ENODEV;
		goto out;
	}

	INFO("cbwlen = %d\n", cbwlen);
	usb_fill_bulk_urb(urb, dev->udev,
		usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
		 bcb, cbwlen, nv_usb_callback, dev);

	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	urb->context = &urb_done;
	retval = usb_submit_urb(urb, GFP_KERNEL);

	if (retval) {
		usb_free_coherent(dev->udev, cbwlen, bcb, urb->transfer_dma);
		INFO("%s(%d) urb submit failed\n", __func__, __LINE__);
		goto out;
	}

	wait_for_completion_interruptible_timeout(
		&urb_done, 20000);


	usb_free_coherent(dev->udev, cbwlen, bcb, urb->transfer_dma);
	INFO("%s(%d)\n", __func__, __LINE__);


	if (transfer_length) {
		INFO("%s(%d) Transfering Data = %d\n",
			__func__, __LINE__, transfer_length);
		buf = usb_alloc_coherent(dev->udev, transfer_length, GFP_KERNEL,
			&urb->transfer_dma);

		if (!buf) {
			INFO("%s(%d)failed to alloc buffer for data transfer\n",
			__func__, __LINE__);
			retval = -ENOMEM;
			goto out;
		}

		if (!data->data_direction) {
			memset(buf, data->write_char, transfer_length);
			usb_fill_bulk_urb(urb, dev->udev,
			usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			buf, transfer_length, nv_usb_callback, dev);
		} else {
			memset(buf, 0, transfer_length);
			usb_fill_bulk_urb(urb, dev->udev,
			usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
			buf, transfer_length, nv_usb_callback, dev);
		}

		urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		urb->context = &urb_done;

		do_gettimeofday(&g_sttime);
		retval = usb_submit_urb(urb, GFP_KERNEL);
		if (retval) {
			usb_free_coherent(dev->udev, transfer_length, buf,
				urb->transfer_dma);
			INFO("%s(%d) urb submit failed\n", __func__, __LINE__);
			goto out;
		}

		wait_for_completion_interruptible_timeout(
			&urb_done, 20000);
		do_gettimeofday(&g_entime);
		if (data->data_direction)
			memcpy(data->buf, buf , transfer_length);

		usb_free_coherent(dev->udev,
			transfer_length, buf, urb->transfer_dma);
		data_transfer_time = nv_usb_calc_time(g_sttime, g_entime);
	}

	bcs = usb_alloc_coherent(dev->udev, cswlen, GFP_KERNEL,
		&urb->transfer_dma);
	if (!bcs) {
		INFO("%s(%d) failed to alloc bcs\n", __func__, __LINE__);
		retval = -ENOMEM;
		goto out;
	}
	memset(bcs, 0, cswlen);
	usb_fill_bulk_urb(urb, dev->udev,
		usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
		bcs, cswlen, nv_usb_callback, dev);

	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	urb->context = &urb_done;
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		usb_free_coherent(dev->udev, cswlen, bcs, urb->transfer_dma);
		INFO("%s(%d) urb submit failed\n", __func__, __LINE__);
		goto out;
	}

	wait_for_completion_interruptible_timeout(
		&urb_done, 20000);

	INFO(
	"%s(%d) dCSWSignature = %x dCSWTag = %x Dataresidur = %d status = %d\n",
				__func__, __LINE__, bcs->Signature,
				bcs->Tag, bcs->Residue, bcs->Status);

	if (bcs->Status)
		data_transfer_time = 0;

	INFO("time taken is  %lu\n", data_transfer_time);

	data->data_transfer_time = data_transfer_time;
	data->g_data_transfer_time = bcs->Residue;

	usb_free_coherent(dev->udev, cswlen, bcs, urb->transfer_dma);
	usb_free_urb(urb);
	mutex_unlock(&dev->mutex);
	return 0;
out:
	usb_free_urb(urb);
	mutex_unlock(&dev->mutex);
	return retval;
}

static ssize_t nv_usb_write(struct file *file, const char *user_buffer,
		size_t count, loff_t *ppos)
{
	return count;
}

static long nv_usb_ioctl(struct file *file, unsigned int cmd_in,
		unsigned long arg, struct nv_usb *dev)
{
	void __user *p = (void __user *)arg;
	struct bulk_data *data = NULL;
	struct user_bulk_data *user_data = NULL;
	int ret = 0;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		INFO("%s(%d) No Memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	user_data = kzalloc(sizeof(*user_data), GFP_KERNEL);
	if (!user_data) {
		INFO("%s(%d) No Memory\n", __func__, __LINE__);
		kfree(data);
		return -ENOMEM;
	}

	if (__copy_from_user(user_data, p, sizeof(*user_data))) {
		INFO("%s(%d) copy from user failed\n", __func__, __LINE__);
		kfree(user_data);
		kfree(data);
		return -EFAULT;
	}

	data->dev = file->private_data;
	data->sub_cmd_length = user_data->sub_cmd_length;

	data->sub_cmd = kzalloc(data->sub_cmd_length, GFP_KERNEL);
	if (!data->sub_cmd) {
		INFO("%s(%d) No Memory\n", __func__, __LINE__);
		kfree(user_data);
		kfree(data);
		return -ENOMEM;
	}
	if (__copy_from_user(data->sub_cmd,
		user_data->sub_cmd, data->sub_cmd_length)) {
		INFO("%s(%d) copy from user failed\n", __func__, __LINE__);
		kfree(data->sub_cmd);
		kfree(user_data);
		kfree(data);
		return -EFAULT;
	}

	data->length = user_data->length;
	data->buf = kzalloc(data->length, GFP_KERNEL);
	if (!data->buf) {
		INFO("%s(%d) No Memory\n", __func__, __LINE__);
		kfree(data->sub_cmd);
		kfree(user_data);
		kfree(data);
		return -ENOMEM;
	}

	data->write_char = user_data->write_char;

	switch (cmd_in) {
	case NVUSB_BULK_WRITE:
		if (__copy_from_user(data->buf, user_data->buf, data->length)) {
			INFO("%s(%d) copy from user failed\n",
						__func__, __LINE__);
			ret = -EFAULT;
			goto error;
		}
		data->data_direction = 0;
		ret = nv_usb_transfer(data);
		if (ret) {
			INFO("%s(%d) nv_usb_transfer failed\n",
						__func__, __LINE__);
			goto error;
		}
		break;
	case NVUSB_BULK_READ:
		data->data_direction = 1;
		INFO("%s(%d)\n", __func__, __LINE__);
		ret = nv_usb_transfer(data);
		if (ret) {
			INFO("%s(%d) nv_usb_transfer failed\n",
						__func__, __LINE__);
			goto error;
		}

		if (__copy_to_user(user_data->buf, data->buf, data->length)) {
			INFO("%s(%d) copy to user failed\n",
						__func__, __LINE__);
			ret = -EFAULT;
			goto error;
		}
		break;
	default:
		INFO("%s(%d) Invalid ioctl command %x\n",
					__func__, __LINE__, cmd_in);
		ret = -EINVAL;
		goto error;
	}

	user_data->data_transfer_time = data->data_transfer_time;
	user_data->g_data_transfer_time = data->g_data_transfer_time;

	if (copy_to_user(p, user_data, sizeof(*user_data))) {
		INFO("%s(%d) copy to user failed\n", __func__, __LINE__);
		ret = -EFAULT;
	}

error:
	kfree(data->buf);
	kfree(data->sub_cmd);
	kfree(user_data);
	kfree(data);

	return ret;

}
static long nv_usb_unlocked_ioctl(struct file *file, unsigned int cmd_in,
		unsigned long arg)
{
	int ret;
	struct nv_usb *dev;

	dev = file->private_data;

	ret = nv_usb_ioctl(file, cmd_in, arg, dev);

	return ret;
}

static const struct file_operations nv_usb_fops = {
	.owner		= THIS_MODULE,
	.read		= nv_usb_read,
	.write		= nv_usb_write,
	.open		= nv_usb_open,
	.unlocked_ioctl = nv_usb_unlocked_ioctl,
	.release	= nv_usb_release,
	.flush		= nv_usb_flush,
	.llseek		= noop_llseek,
};

static struct usb_class_driver nv_usb_class = {
	.name		= "nvbulk%d",
	.fops		= &nv_usb_fops,
	.minor_base	= NV_USB_BULK_MINOR_BASE,
};

static int nv_usb_probe(struct usb_interface *interface,
		const struct usb_device_id *id)
{
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	struct nv_usb *dev;
	struct usb_device *udev;
	int retval = 0;
	int i;

	udev = usb_get_dev(interface_to_usbdev(interface));
	/* allocate memory for our device state and initialize it */
	dev = devm_kzalloc(&udev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		INFO("%s(%d) Out of Memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	kref_init(&dev->kref);
	init_usb_anchor(&dev->submitted);
	mutex_init(&dev->mutex);

	dev->udev = udev;
	dev->interface = interface;

	/*Set up the Endpoint Information */
	/* use only the first bulk-in and bulk-out endpoints*/
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(endpoint)) {
			/*We found a bulk_in endpoint */
			dev->bulk_in_size = usb_endpoint_maxp(endpoint);
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
		}

		if (usb_endpoint_is_bulk_out(endpoint)) {
			/*We found a buld_out endpoint */
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}

	if (dev->bulk_in_endpointAddr == 0
		|| dev->bulk_out_endpointAddr == 0) {
		INFO("%s(%d) Count not find bulk point infomations\n",
							__func__, __LINE__);
		goto error;
	}

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &nv_usb_class);

	if (retval) {
		INFO("%s(%d) usb_register_dev failed\n", __func__, __LINE__);
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	INFO("%s(%d) device now attached to USB\n", __func__, __LINE__);
	return 0;
error:
	if (dev) {
		kref_put(&dev->kref, nv_usb_delete);
		retval = -EFAULT;
	}

	return retval;

}

static void nv_usb_disconnect(struct usb_interface *interface)
{
	struct nv_usb *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_deregister_dev(interface, &nv_usb_class);

	mutex_lock(&dev->mutex);
	dev->interface = NULL;
	mutex_unlock(&dev->mutex);

	usb_kill_anchored_urbs(&dev->submitted);

	kref_put(&dev->kref, nv_usb_delete);

	INFO("%s(%d) disconnected minor = %d\n", __func__, __LINE__, minor);

}

static struct usb_driver nv_usb_driver = {
	.name		= "nv-usb",
	.probe		= nv_usb_probe,
	.disconnect	= nv_usb_disconnect,
	.id_table	= nv_usb_table,
};

module_usb_driver(nv_usb_driver);
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL");

/*
 * User-mode HECI API
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/aio.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/uuid.h>
#include <linux/compat.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include "heci-api.h"
#include "heci_dev.h"
#include "client.h"
#include "platform-config.h"

#ifdef dev_dbg
#undef dev_dbg
#endif
static  void no_dev_dbg(void *v, char *s, ...)
{
}
#define dev_dbg no_dev_dbg
/*#define dev_dbg dev_err*/

/**
 * heci_open - the open function
 *
 * @inode: pointer to inode structure
 * @file: pointer to file structure
 e
 * returns 0 on success, <0 on error
 */
static int heci_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct pci_dev *pdev;
	struct heci_cl *cl;
	struct heci_device *dev;
	int err;

	/* Non-blocking semantics are not supported */
	if (file->f_flags & O_NONBLOCK)
		return	-EINVAL;

	err = -ENODEV;
	if (!misc->parent)
		goto out;

	pdev = container_of(misc->parent, struct pci_dev, dev);

	dev = pci_get_drvdata(pdev);
	if (!dev)
		goto out;

	err = -ENOMEM;
	cl = heci_cl_allocate(dev);
	if (!cl)
		goto out_free;

	/*
	 * We may have a case of issued open() with
	 * dev->dev_state == HECI_DEV_DISABLED, as part of re-enabling path
	 */
#if 0
	err = -ENODEV;
	if (dev->dev_state != HECI_DEV_ENABLED) {
		dev_dbg(&dev->pdev->dev, "dev_state != HECI_ENABLED  dev_state = %s\n",
		    heci_dev_state_str(dev->dev_state));
		goto out_free;
	}
#endif

	err = heci_cl_link(cl, HECI_HOST_CLIENT_ID_ANY);
	if (err)
		goto out_free;

	file->private_data = cl;

	return nonseekable_open(inode, file);

out_free:
	kfree(cl);
out:
	return err;
}

/**
 * heci_release - the release function
 *
 * @inode: pointer to inode structure
 * @file: pointer to file structure
 *
 * returns 0 on success, <0 on error
 */
static int heci_release(struct inode *inode, struct file *file)
{
	struct heci_cl *cl = file->private_data;
	struct heci_device *dev;
	int rets = 0;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	/*
	 * May happen if device sent FW reset or was intentionally
	 * halted by host SW. The client is then invalid
	 */
	if (dev->dev_state != HECI_DEV_ENABLED)
		return	0;

	if (cl->state == HECI_CL_CONNECTED) {
		cl->state = HECI_CL_DISCONNECTING;
		dev_dbg(&dev->pdev->dev, "disconnecting client host client = %d, ME client = %d\n",
			cl->host_client_id, cl->me_client_id);
		rets = heci_cl_disconnect(cl);
	}
	heci_cl_flush_queues(cl);
	dev_dbg(&dev->pdev->dev, "remove client host client = %d, ME client = %d\n",
	    cl->host_client_id,
	    cl->me_client_id);

	heci_cl_unlink(cl);

	file->private_data = NULL;

	/* disband and free all Tx and Rx client-level rings */
	heci_cl_free(cl);
	return rets;
}


/**
 * heci_read - the read function.
 *
 * @file: pointer to file structure
 * @ubuf: pointer to user buffer
 * @length: buffer length
 * @offset: data offset in buffer
 *
 * returns >=0 data length on success , <0 on error
 */
static ssize_t heci_read(struct file *file, char __user *ubuf,
			size_t length, loff_t *offset)
{
	struct heci_cl *cl = file->private_data;
	struct heci_cl_rb *rb = NULL;
	struct heci_device *dev;
	int rets;
	unsigned long flags;

	/* Non-blocking semantics are not supported */
	if (file->f_flags & O_NONBLOCK)
		return	-EINVAL;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;
	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

/*
 * EXPLAINME: handle reading message by fragments smaller than
 * actual message size. Why needed? Reportedly, doesn't work: why?
 */
#if 0
	if (cl->read_rb && cl->read_rb->buf_idx > *offset) {
		rb = cl->read_rb;
		goto copy_buffer;
	} else if (cl->read_rb && cl->read_rb->buf_idx > 0 &&
		   cl->read_rb->buf_idx <= *offset) {
		rb = cl->read_rb;
		rets = 0;
		goto free;
	} else if ((!cl->read_rb || !cl->read_rb->buf_idx) && *offset > 0) {
		/*Offset needs to be cleaned for contiguous reads*/
		*offset = 0;
		rets = 0;
		goto out;
	}
#endif

	spin_lock_irqsave(&cl->in_process_spinlock, flags);
	if (!list_empty(&cl->in_process_list.list)) {
		rb = list_entry(cl->in_process_list.list.next,
			struct heci_cl_rb, list);
		list_del_init(&rb->list);
		spin_unlock_irqrestore(&cl->in_process_spinlock, flags);
		goto copy_buffer;
	}
	spin_unlock_irqrestore(&cl->in_process_spinlock, flags);

	if (waitqueue_active(&cl->rx_wait)) {
		rets = -EBUSY;
		goto out;
	}

	if (wait_event_interruptible(cl->rx_wait,
			(dev->dev_state == HECI_DEV_ENABLED &&
			(cl->read_rb || HECI_CL_INITIALIZING == cl->state ||
			HECI_CL_DISCONNECTED == cl->state ||
			HECI_CL_DISCONNECTING == cl->state)))) {
		dev_err(&dev->pdev->dev, "%s(): woke up not in success; sig. pending = %d signal = %08lX\n",
			__func__, signal_pending(current),
			current->pending.signal.sig[0]);
		return	-ERESTARTSYS;
	}

	/*
	 * If FW reset arrived, this will happen. Don't check cl->,
	 * as 'cl' may be freed already
	 */
	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -ENODEV;
		goto	out;
	}

	if (HECI_CL_INITIALIZING == cl->state ||
	    HECI_CL_DISCONNECTED == cl->state ||
	    HECI_CL_DISCONNECTING == cl->state) {
		rets = -EBUSY;
		goto out;
	}

	rb = cl->read_rb;
	if (!rb) {
		rets = -ENODEV;
		goto out;
	}

	/* now copy the data to user space */
copy_buffer:
	dev_dbg(&dev->pdev->dev, "buf.size = %d buf.idx= %ld\n",
	    rb->buffer.size, rb->buf_idx);
	if (length == 0 || ubuf == NULL || *offset > rb->buf_idx) {
		rets = -EMSGSIZE;
		goto free;
	}

	/* length is being truncated to PAGE_SIZE,
	 * however buf_idx may point beyond that */
	length = min_t(size_t, length, rb->buf_idx - *offset);

	if (copy_to_user(ubuf, rb->buffer.data + *offset, length)) {
		rets = -EFAULT;
		goto free;
	}

	rets = length;
	*offset += length;
	if ((unsigned long)*offset < rb->buf_idx)
		goto out;

free:
	heci_io_rb_recycle(rb);

	cl->read_rb = NULL;
	*offset = 0;
out:
	dev_dbg(&dev->pdev->dev, "end heci read rets= %d\n", rets);
	return rets;
}


/**
 * heci_write - the write function.
 *
 * @file: pointer to file structure
 * @ubuf: pointer to user buffer
 * @length: buffer length
 * @offset: data offset in buffer
 *
 * returns >=0 data length on success , <0 on error
 */
static ssize_t heci_write(struct file *file, const char __user *ubuf,
	size_t length, loff_t *offset)
{
	struct heci_cl *cl = file->private_data;

	/*
	 * TODO: we may further optimize write path by obtaining and directly
	 * copy_from_user'ing to tx_ring's buffer
	 */
	void *write_buf = NULL;
	struct heci_device *dev;
	int rets;

	/* Non-blocking semantics are not supported */
	if (file->f_flags & O_NONBLOCK)
		return	-EINVAL;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	if (cl->state != HECI_CL_CONNECTED) {
		dev_err(&dev->pdev->dev, "host client = %d,  is not connected to ME client = %d",
			cl->host_client_id, cl->me_client_id);
		rets = -ENODEV;
		goto out;
	}

	if (length <= 0) {
		rets = -EMSGSIZE;
		goto out;
	}

	/* FIXME: check for DMA size for clients that accept DMA transfers */
	if (length > cl->device->fw_client->props.max_msg_length) {
		/* If the client supports DMA, try to use it */
		if (!(host_dma_enabled &&
				cl->device->fw_client->props.dma_hdr_len &
				HECI_CLIENT_DMA_ENABLED)) {
			rets = -EMSGSIZE;
			goto out;
		}
	}

	write_buf = kmalloc(length, GFP_KERNEL);
	if (!write_buf) {
		dev_err(&dev->pdev->dev, "write buffer allocation failed\n");
		rets = -ENOMEM;
		goto	out;
	}

	rets = copy_from_user(write_buf, ubuf, length);
	if (rets)
		goto out;
	rets = heci_cl_send(cl, write_buf, length);
	if (!rets)
		rets = length;
	else
		rets = -EIO;
out:
	kfree(write_buf);
	return rets;
}

/**
 * heci_ioctl_connect_client - the connect to fw client IOCTL function
 *
 * @dev: the device structure
 * @data: IOCTL connect data, input and output parameters
 * @file: private data of the file object
 *
 * Locking: called under "dev->device_lock" lock
 *
 * returns 0 on success, <0 on failure.
 */
static int heci_ioctl_connect_client(struct file *file,
	struct heci_connect_client_data *data)
{
	struct heci_device *dev;
	struct heci_client *client;
	struct heci_cl *cl;
	int i;
	int rets;

	ISH_DBG_PRINT(KERN_ALERT "%s(): +++\n", __func__);
	cl = file->private_data;
	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -ENODEV;
		goto end;
	}

	if (cl->state != HECI_CL_INITIALIZING &&
	    cl->state != HECI_CL_DISCONNECTED) {
		rets = -EBUSY;
		goto end;
	}

	/* find ME client we're trying to connect to */
	i = heci_me_cl_by_uuid(dev, &data->in_client_uuid);
	if (i < 0 || dev->me_clients[i].props.fixed_address) {
		dev_dbg(&dev->pdev->dev, "Cannot connect to FW Client UUID = %pUl\n",
				&data->in_client_uuid);
		rets = -ENODEV;
		goto end;
	}

	/* Check if there's driver attached to this UUID */
	if (!heci_can_client_connect(dev, &data->in_client_uuid))
		return	-EBUSY;

	cl->me_client_id = dev->me_clients[i].client_id;
	cl->state = HECI_CL_CONNECTING;

	dev_dbg(&dev->pdev->dev, "Connect to FW Client ID = %d\n",
			cl->me_client_id);
	dev_dbg(&dev->pdev->dev, "FW Client - Protocol Version = %d\n",
			dev->me_clients[i].props.protocol_version);
	dev_dbg(&dev->pdev->dev, "FW Client - Max Msg Len = %d\n",
			dev->me_clients[i].props.max_msg_length);

	/* prepare the output buffer */
	client = &data->out_client_properties;
	client->max_msg_length = dev->me_clients[i].props.max_msg_length;
	client->protocol_version = dev->me_clients[i].props.protocol_version;
	dev_dbg(&dev->pdev->dev, "Can connect?\n");

	rets = heci_cl_connect(cl);

end:
	ISH_DBG_PRINT(KERN_ALERT "%s(): --- (%d)\n", __func__, rets);
	return	rets;
}


/**
 * heci_ioctl - the IOCTL function
 *
 * @file: pointer to file structure
 * @cmd: ioctl command
 * @data: pointer to heci message structure
 *
 * returns 0 on success , <0 on error
 */
static long heci_ioctl(struct file *file, unsigned int cmd, unsigned long data)
{
	struct heci_device *dev;
	struct heci_cl *cl = file->private_data;
	struct heci_connect_client_data *connect_data = NULL;
	int rets;
	unsigned	ring_size;
	char fw_stat_buf[20];

	if (!cl)
		return -EINVAL;

	dev = cl->dev;
	dev_dbg(&dev->pdev->dev, "IOCTL cmd = 0x%x", cmd);

	/* Test API for triggering host-initiated IPC reset to ISS */
	if (cmd == 0x12345678) {
		ISH_DBG_PRINT(KERN_ALERT "%s(): ISS FW reset is requested\n",
			__func__);
		/* Re-init */
		dev->dev_state = HECI_DEV_INITIALIZING;
		heci_reset(dev, 1);

		if (heci_hbm_start_wait(dev)) {
			dev_err(&dev->pdev->dev, "HBM haven't started");
			goto err;
		}

		if (!heci_host_is_ready(dev)) {
			dev_err(&dev->pdev->dev, "host is not ready.\n");
			goto err;
		}

		if (!heci_hw_is_ready(dev)) {
			dev_err(&dev->pdev->dev, "ME is not ready.\n");
			goto err;
		}

		return	0;
err:
		dev_err(&dev->pdev->dev, "link layer initialization failed.\n");
		dev->dev_state = HECI_DEV_DISABLED;
		return -ENODEV;
	}

	/* Test API for triggering host disabling */
	if (cmd == 0xAA55AA55) {
		ISH_DBG_PRINT(KERN_ALERT "%s(): ISS host stop is requested\n",
			__func__);
		/* Handle ISS reset against upper layers */

		/* Remove all client devices */
		heci_bus_remove_all_clients(dev);
		dev->dev_state = HECI_DEV_DISABLED;
		return	0;
	}

	if (cmd == IOCTL_HECI_SET_RX_FIFO_SIZE) {
		ring_size = data;
		if (ring_size > CL_MAX_RX_RING_SIZE)
			return	-EINVAL;
		if (cl->state != HECI_CL_INITIALIZING)
			return	-EBUSY;
		cl->rx_ring_size = ring_size;
		return	0;
	}

	if (cmd == IOCTL_HECI_SET_TX_FIFO_SIZE) {
		ring_size = data;
		if (ring_size > CL_MAX_TX_RING_SIZE)
			return	-EINVAL;
		if (cl->state != HECI_CL_INITIALIZING)
			return	-EBUSY;
		cl->tx_ring_size = ring_size;
		return	0;
	}

	if (cmd == IOCTL_GET_FW_STATUS) {
		sprintf(fw_stat_buf, "%08X\n", dev->ops->get_fw_status(dev));
		copy_to_user((char __user *)data, fw_stat_buf,
			strlen(fw_stat_buf));
		return strlen(fw_stat_buf);
	}

	if (cmd != IOCTL_HECI_CONNECT_CLIENT)
		return -EINVAL;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	if (dev->dev_state != HECI_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	dev_dbg(&dev->pdev->dev, ": IOCTL_HECI_CONNECT_CLIENT.\n");

	connect_data = kzalloc(sizeof(struct heci_connect_client_data),
							GFP_KERNEL);
	if (!connect_data) {
		rets = -ENOMEM;
		goto out;
	}
	dev_dbg(&dev->pdev->dev, "copy connect data from user\n");
	if (copy_from_user(connect_data, (char __user *)data,
			sizeof(struct heci_connect_client_data))) {
		dev_dbg(&dev->pdev->dev, "failed to copy data from userland\n");
		rets = -EFAULT;
		goto out;
	}

	rets = heci_ioctl_connect_client(file, connect_data);

	/* if all is ok, copying the data back to user. */
	if (rets)
		goto out;

	dev_dbg(&dev->pdev->dev, "copy connect data to user\n");
	if (copy_to_user((char __user *)data, connect_data,
				sizeof(struct heci_connect_client_data))) {
		dev_dbg(&dev->pdev->dev, "failed to copy data to userland\n");
		rets = -EFAULT;
		goto out;
	}

out:
	kfree(connect_data);
	return rets;
}

/**
 * heci_compat_ioctl - the compat IOCTL function
 *
 * @file: pointer to file structure
 * @cmd: ioctl command
 * @data: pointer to heci message structure
 *
 * returns 0 on success , <0 on error
 */
#ifdef CONFIG_COMPAT
static long heci_compat_ioctl(struct file *file,
			unsigned int cmd, unsigned long data)
{
	return heci_ioctl(file, cmd, (unsigned long)compat_ptr(data));
}
#endif /*CONFIG_COMPAT*/


/*
 * file operations structure will be used for heci char device.
 */
static const struct file_operations heci_fops = {
	.owner = THIS_MODULE,
	.read = heci_read,
	.unlocked_ioctl = heci_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = heci_compat_ioctl,
#endif /*CONFIG_COMPAT*/
	.open = heci_open,
	.release = heci_release,
	.write = heci_write,
	.llseek = no_llseek
};

/*
 * Misc Device Struct
 */
static struct miscdevice  heci_misc_device = {
		.name = "ish",		/*"heci" changed to "ish", stuff it #2*/
		.fops = &heci_fops,
		.minor = MISC_DYNAMIC_MINOR,
};


int heci_register(struct heci_device *dev)
{
	int ret;
	heci_misc_device.parent = &dev->pdev->dev;
	ret = misc_register(&heci_misc_device);
	if (ret)
		return ret;

	if (heci_dbgfs_register(dev, heci_misc_device.name))
		dev_err(&dev->pdev->dev, "cannot register debugfs\n");

	return 0;
}
EXPORT_SYMBOL_GPL(heci_register);

void heci_deregister(struct heci_device *dev)
{
	if (heci_misc_device.parent == NULL)
		return;

	heci_dbgfs_deregister(dev);
	misc_deregister(&heci_misc_device);
	heci_misc_device.parent = NULL;
}
EXPORT_SYMBOL_GPL(heci_deregister);

static int __init heci_init(void)
{
	return heci_cl_bus_init();
}

static void __exit heci_exit(void)
{
	heci_cl_bus_exit();
}

module_init(heci_init);
module_exit(heci_exit);

MODULE_AUTHOR("Intel Corporation");
MODULE_DESCRIPTION("Intel(R) Management Engine Interface");
MODULE_LICENSE("GPL v2");


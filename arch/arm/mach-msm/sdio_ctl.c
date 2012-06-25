/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

/*
 * SDIO Control Driver -- Provides a binary SDIO muxed control port
 *                       interface.
 */

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <asm/ioctls.h>
#include <linux/platform_device.h>
#include <mach/msm_smd.h>
#include <mach/sdio_al.h>
#include <mach/sdio_cmux.h>
#include "modem_notifier.h"
#include <linux/slab.h>

#define MAX_WRITE_RETRY 5
#define MAGIC_NO_V1 0x33FC
#define NUM_SDIO_CTL_PORTS 10
#define DEVICE_NAME "sdioctl"
#define MAX_BUF_SIZE 2048
#define DEBUG

static int msm_sdio_ctl_debug_mask;
module_param_named(debug_mask, msm_sdio_ctl_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

struct sdio_ctl_dev {
	int id;
	char name[9];
	struct cdev cdev;
	struct device *devicep;
	struct mutex dev_lock;
	int ref_count;

	struct mutex rx_lock;
	uint32_t read_avail;
	struct list_head rx_list;

	wait_queue_head_t read_wait_queue;
	wait_queue_head_t write_wait_queue;
} *sdio_ctl_devp[NUM_SDIO_CTL_PORTS];

struct sdio_ctl_pkt {
	int data_size;
	void *data;
};

struct sdio_ctl_list_elem {
	struct list_head list;
	struct sdio_ctl_pkt ctl_pkt;
};

struct class *sdio_ctl_classp;
static dev_t sdio_ctl_number;
static uint32_t sdio_ctl_inited;

enum {
	MSM_SDIO_CTL_DEBUG = 1U << 0,
	MSM_SDIO_CTL_DUMP_BUFFER = 1U << 1,
};

#if defined(DEBUG)
#define D_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (msm_sdio_ctl_debug_mask & MSM_SDIO_CTL_DUMP_BUFFER) { \
		int i; \
		pr_info("%s", prestr); \
		for (i = 0; i < cnt; i++) \
			pr_info("%.2x", buf[i]); \
		pr_info("\n"); \
	} \
} while (0)

#define D(x...) \
do { \
	if (msm_sdio_ctl_debug_mask & MSM_SDIO_CTL_DEBUG) \
		pr_info(x); \
} while (0)

#else
#define D_DUMP_BUFFER(prestr, cnt, buf) do {} while (0)
#define D(x...) do {} while (0)
#endif

static uint32_t cmux_ch_id[] = {
	SDIO_CMUX_DATA_CTL_0,
	SDIO_CMUX_DATA_CTL_1,
	SDIO_CMUX_DATA_CTL_2,
	SDIO_CMUX_DATA_CTL_3,
	SDIO_CMUX_DATA_CTL_4,
	SDIO_CMUX_DATA_CTL_5,
	SDIO_CMUX_DATA_CTL_6,
	SDIO_CMUX_DATA_CTL_7,
	SDIO_CMUX_USB_CTL_0,
	SDIO_CMUX_CSVT_CTL_0
};

static int get_ctl_dev_index(int id)
{
	int dev_index;
	for (dev_index = 0; dev_index < NUM_SDIO_CTL_PORTS; dev_index++) {
		if (cmux_ch_id[dev_index] == id)
			return dev_index;
	}
	return -ENODEV;
}

static void sdio_ctl_receive_cb(void *data, int size, void *priv)
{
	struct sdio_ctl_list_elem *list_elem = NULL;
	int id = ((struct sdio_ctl_dev *)(priv))->id;
	int dev_index;

	if (id < 0 || id > cmux_ch_id[NUM_SDIO_CTL_PORTS - 1])
		return;
	dev_index = get_ctl_dev_index(id);
	if (dev_index < 0) {
		pr_err("%s: Ch%d is not exported to user-space\n",
			__func__, id);
		return;
	}

	if (!data || size <= 0) {
		wake_up(&sdio_ctl_devp[dev_index]->read_wait_queue);
		return;
	}

	list_elem = kmalloc(sizeof(struct sdio_ctl_list_elem), GFP_KERNEL);
	if (!list_elem) {
		pr_err("%s: list_elem alloc failed\n", __func__);
		return;
	}

	list_elem->ctl_pkt.data = kmalloc(size, GFP_KERNEL);
	if (!list_elem->ctl_pkt.data) {
		pr_err("%s: list_elem->data alloc failed\n", __func__);
		kfree(list_elem);
		return;
	}
	memcpy(list_elem->ctl_pkt.data, data, size);
	list_elem->ctl_pkt.data_size = size;
	mutex_lock(&sdio_ctl_devp[dev_index]->rx_lock);
	list_add_tail(&list_elem->list, &sdio_ctl_devp[dev_index]->rx_list);
	sdio_ctl_devp[dev_index]->read_avail += size;
	mutex_unlock(&sdio_ctl_devp[dev_index]->rx_lock);
	wake_up(&sdio_ctl_devp[dev_index]->read_wait_queue);
}

static void sdio_ctl_write_done(void *data, int size, void *priv)
{
	int id = ((struct sdio_ctl_dev *)(priv))->id;
	int dev_index;
	if (id < 0 || id > cmux_ch_id[NUM_SDIO_CTL_PORTS - 1])
		return;

	dev_index = get_ctl_dev_index(id);
	if (dev_index < 0) {
		pr_err("%s: Ch%d is not exported to user-space\n",
			__func__, id);
		return;
	}
	wake_up(&sdio_ctl_devp[dev_index]->write_wait_queue);
}

static long sdio_ctl_ioctl(struct file *file, unsigned int cmd,
					      unsigned long arg)
{
	int ret;
	struct sdio_ctl_dev *sdio_ctl_devp;

	sdio_ctl_devp = file->private_data;
	if (!sdio_ctl_devp)
		return -ENODEV;

	switch (cmd) {
	case TIOCMGET:
		ret = sdio_cmux_tiocmget(sdio_ctl_devp->id);
		break;
	case TIOCMSET:
		ret = sdio_cmux_tiocmset(sdio_ctl_devp->id, arg, ~arg);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static unsigned int sdio_ctl_poll(struct file *file, poll_table *wait)
{
	struct sdio_ctl_dev *sdio_ctl_devp;
	unsigned int mask = 0;

	sdio_ctl_devp = file->private_data;
	if (!sdio_ctl_devp) {
		pr_err("%s: on a NULL device\n", __func__);
		return POLLERR;
	}

	poll_wait(file, &sdio_ctl_devp->read_wait_queue, wait);
	mutex_lock(&sdio_ctl_devp->rx_lock);
	if (sdio_cmux_is_channel_reset(sdio_ctl_devp->id)) {
		mutex_unlock(&sdio_ctl_devp->rx_lock);
		pr_err("%s notifying reset for sdio_ctl_dev id:%d\n",
			__func__, sdio_ctl_devp->id);
		return POLLERR;
	}

	if (sdio_ctl_devp->read_avail > 0)
		mask |= POLLIN | POLLRDNORM;

	mutex_unlock(&sdio_ctl_devp->rx_lock);

	return mask;
}

ssize_t sdio_ctl_read(struct file *file,
		      char __user *buf,
		      size_t count,
		      loff_t *ppos)
{
	int r = 0, id, bytes_to_read;
	struct sdio_ctl_dev *sdio_ctl_devp;
	struct sdio_ctl_list_elem *list_elem = NULL;

	sdio_ctl_devp = file->private_data;

	if (!sdio_ctl_devp)
		return -ENODEV;

	D("%s: read from ch%d\n", __func__, sdio_ctl_devp->id);

	id = sdio_ctl_devp->id;
	mutex_lock(&sdio_ctl_devp->rx_lock);
	while (sdio_ctl_devp->read_avail <= 0) {
		mutex_unlock(&sdio_ctl_devp->rx_lock);
		r = wait_event_interruptible(sdio_ctl_devp->read_wait_queue,
					     sdio_ctl_devp->read_avail > 0 ||
					     !is_remote_open(id));
		if (sdio_cmux_is_channel_reset(id))
			return -ENETRESET;

		if (!is_remote_open(id))
			return -ENODEV;

		if (r < 0) {
			/* qualify error message */
			/* we get this anytime a signal comes in */
			if (r != -ERESTARTSYS)
				pr_err("ERROR:%s: wait_event_interruptible "
				       "ret %i\n", __func__, r);
			return r;
		}
		mutex_lock(&sdio_ctl_devp->rx_lock);
	}

	if (list_empty(&sdio_ctl_devp->rx_list)) {
		mutex_unlock(&sdio_ctl_devp->rx_lock);
		D("%s: Nothing in ch%d's rx_list\n", __func__,
		  sdio_ctl_devp->id);
		return -EAGAIN;
	}

	list_elem = list_first_entry(&sdio_ctl_devp->rx_list,
				     struct sdio_ctl_list_elem, list);
	bytes_to_read = (uint32_t)(list_elem->ctl_pkt.data_size);
	if (bytes_to_read > count) {
		mutex_unlock(&sdio_ctl_devp->rx_lock);
		pr_err("%s: Packet size %d > buf size %d\n", __func__,
			bytes_to_read, count);
		return -ENOMEM;
	}

	if (copy_to_user(buf, list_elem->ctl_pkt.data, bytes_to_read)) {
		mutex_unlock(&sdio_ctl_devp->rx_lock);
		pr_err("%s: copy_to_user failed for ch%d\n", __func__,
			sdio_ctl_devp->id);
		return -EFAULT;
	}
	sdio_ctl_devp->read_avail -= bytes_to_read;
	list_del(&list_elem->list);
	kfree(list_elem->ctl_pkt.data);
	kfree(list_elem);
	mutex_unlock(&sdio_ctl_devp->rx_lock);
	D("%s: Returning %d bytes to ch%d\n", __func__,
			bytes_to_read, sdio_ctl_devp->id);
	return bytes_to_read;
}


ssize_t sdio_ctl_write(struct file *file,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int r = 0, id;
	char *temp_buf;
	struct sdio_ctl_dev *sdio_ctl_devp;

	if (count <= 0)
		return -EINVAL;

	sdio_ctl_devp = file->private_data;
	if (!sdio_ctl_devp)
		return -ENODEV;

	D("%s: writing %i bytes on ch%d\n",
	  __func__, count, sdio_ctl_devp->id);
	id = sdio_ctl_devp->id;
	mutex_lock(&sdio_ctl_devp->dev_lock);
	while (sdio_cmux_write_avail(id) < count) {
		mutex_unlock(&sdio_ctl_devp->dev_lock);
		r = wait_event_interruptible(sdio_ctl_devp->write_wait_queue,
					     sdio_cmux_write_avail(id) >= count
					     || !is_remote_open(id));

		if (sdio_cmux_is_channel_reset(id))
			return -ENETRESET;

		if (!is_remote_open(id))
			return -ENODEV;

		if (r < 0) {
			/* qualify error message */
			/* we get this anytime a signal comes in */
			if (r != -ERESTARTSYS)
				pr_err("ERROR:%s: wait_event_interruptible "
				       "ret %i\n", __func__, r);
			return r;
		}
		mutex_lock(&sdio_ctl_devp->dev_lock);
	}

	temp_buf = kmalloc(count, GFP_KERNEL);
	if (!temp_buf) {
		mutex_unlock(&sdio_ctl_devp->dev_lock);
		pr_err("%s: temp_buf alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(temp_buf, buf, count)) {
		mutex_unlock(&sdio_ctl_devp->dev_lock);
		pr_err("%s: copy_from_user failed\n", __func__);
		kfree(temp_buf);
		return -EFAULT;
	}

	r = sdio_cmux_write(id, (void *)temp_buf, count);
	kfree(temp_buf);
	mutex_unlock(&sdio_ctl_devp->dev_lock);
	return r;
}


int sdio_ctl_open(struct inode *inode, struct file *file)
{
	int r = 0;
	struct sdio_ctl_dev *sdio_ctl_devp;

	if (!sdio_ctl_inited)
		return -EIO;

	sdio_ctl_devp = container_of(inode->i_cdev, struct sdio_ctl_dev, cdev);

	if (!sdio_ctl_devp)
		return -ENODEV;

	D("%s called on sdioctl%d device\n", __func__, sdio_ctl_devp->id);
	r = sdio_cmux_open(sdio_ctl_devp->id, sdio_ctl_receive_cb,
			   sdio_ctl_write_done, NULL,
			   sdio_ctl_devp);
	if (r < 0) {
		pr_err("ERROR %s: sdio_cmux_open failed with rc %d\n",
			__func__, r);
		return r;
	}

	mutex_lock(&sdio_ctl_devp->dev_lock);
	sdio_ctl_devp->ref_count++;
	mutex_unlock(&sdio_ctl_devp->dev_lock);

	file->private_data = sdio_ctl_devp;
	return 0;
}

int sdio_ctl_release(struct inode *inode, struct file *file)
{
	struct sdio_ctl_dev *sdio_ctl_devp;
	struct sdio_ctl_list_elem *list_elem = NULL;

	sdio_ctl_devp = file->private_data;
	if (!sdio_ctl_devp)
		return -EINVAL;

	D("%s called on sdioctl%d device\n", __func__, sdio_ctl_devp->id);

	mutex_lock(&sdio_ctl_devp->dev_lock);
	if (sdio_ctl_devp->ref_count > 0) {
		sdio_ctl_devp->ref_count--;
		if (!sdio_ctl_devp->ref_count) {
			mutex_lock(&sdio_ctl_devp->rx_lock);
			while (!list_empty(&sdio_ctl_devp->rx_list)) {
				list_elem = list_first_entry(
						&sdio_ctl_devp->rx_list,
						struct sdio_ctl_list_elem,
						list);
				list_del(&list_elem->list);
				kfree(list_elem->ctl_pkt.data);
				kfree(list_elem);
			}
			sdio_ctl_devp->read_avail = 0;
			mutex_unlock(&sdio_ctl_devp->rx_lock);
			sdio_cmux_close(sdio_ctl_devp->id);
		}
	}
	mutex_unlock(&sdio_ctl_devp->dev_lock);

	file->private_data = NULL;
	return 0;
}

static const struct file_operations sdio_ctl_fops = {
	.owner = THIS_MODULE,
	.open = sdio_ctl_open,
	.release = sdio_ctl_release,
	.read = sdio_ctl_read,
	.write = sdio_ctl_write,
	.poll = sdio_ctl_poll,
	.unlocked_ioctl = sdio_ctl_ioctl,
};

static int sdio_ctl_probe(struct platform_device *pdev)
{
	int i;
	int r;

	pr_info("%s Begins\n", __func__);
	for (i = 0; i < NUM_SDIO_CTL_PORTS; ++i) {
		sdio_ctl_devp[i] = kzalloc(sizeof(struct sdio_ctl_dev),
					GFP_KERNEL);
		if (IS_ERR(sdio_ctl_devp[i])) {
			pr_err("ERROR:%s kmalloc() ENOMEM\n", __func__);
			r = -ENOMEM;
			goto error0;
		}

		sdio_ctl_devp[i]->id = cmux_ch_id[i];
		sdio_ctl_devp[i]->ref_count = 0;

		mutex_init(&sdio_ctl_devp[i]->dev_lock);
		init_waitqueue_head(&sdio_ctl_devp[i]->read_wait_queue);
		init_waitqueue_head(&sdio_ctl_devp[i]->write_wait_queue);
		mutex_init(&sdio_ctl_devp[i]->rx_lock);
		INIT_LIST_HEAD(&sdio_ctl_devp[i]->rx_list);
		sdio_ctl_devp[i]->read_avail = 0;
	}

	r = alloc_chrdev_region(&sdio_ctl_number, 0, NUM_SDIO_CTL_PORTS,
				DEVICE_NAME);
	if (IS_ERR_VALUE(r)) {
		pr_err("ERROR:%s: alloc_chrdev_region() ret %i.\n",
		       __func__, r);
		goto error0;
	}

	sdio_ctl_classp = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(sdio_ctl_classp)) {
		pr_err("ERROR:%s: class_create() ENOMEM\n", __func__);
		r = -ENOMEM;
		goto error1;
	}

	for (i = 0; i < NUM_SDIO_CTL_PORTS; ++i) {
		cdev_init(&sdio_ctl_devp[i]->cdev, &sdio_ctl_fops);
		sdio_ctl_devp[i]->cdev.owner = THIS_MODULE;

		r = cdev_add(&sdio_ctl_devp[i]->cdev, (sdio_ctl_number + i),
			     1);

		if (IS_ERR_VALUE(r)) {
			pr_err("%s: cdev_add() ret %i\n", __func__, r);
			kfree(sdio_ctl_devp[i]);
			goto error2;
		}

		sdio_ctl_devp[i]->devicep =
				device_create(sdio_ctl_classp, NULL,
				      (sdio_ctl_number + i), NULL,
				      DEVICE_NAME "%d", cmux_ch_id[i]);

		if (IS_ERR(sdio_ctl_devp[i]->devicep)) {
			pr_err("%s: device_create() ENOMEM\n", __func__);
			r = -ENOMEM;
			cdev_del(&sdio_ctl_devp[i]->cdev);
			kfree(sdio_ctl_devp[i]);
			goto error2;
		}
	}

	sdio_ctl_inited = 1;
	D("SDIO Control Port Driver Initialized.\n");
	return 0;

error2:
	while (--i >= 0) {
		cdev_del(&sdio_ctl_devp[i]->cdev);
		device_destroy(sdio_ctl_classp,
			       MKDEV(MAJOR(sdio_ctl_number), i));
	}

	class_destroy(sdio_ctl_classp);
	i = NUM_SDIO_CTL_PORTS;
error1:
	unregister_chrdev_region(MAJOR(sdio_ctl_number), NUM_SDIO_CTL_PORTS);
error0:
	while (--i >= 0)
		kfree(sdio_ctl_devp[i]);
	return r;
}

static int sdio_ctl_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_SDIO_CTL_PORTS; ++i) {
		cdev_del(&sdio_ctl_devp[i]->cdev);
		kfree(sdio_ctl_devp[i]);
		device_destroy(sdio_ctl_classp,
			       MKDEV(MAJOR(sdio_ctl_number), i));
	}
	class_destroy(sdio_ctl_classp);
	unregister_chrdev_region(MAJOR(sdio_ctl_number), NUM_SDIO_CTL_PORTS);

	return 0;
}

static struct platform_driver sdio_ctl_driver = {
	.probe		= sdio_ctl_probe,
	.remove		= sdio_ctl_remove,
	.driver		= {
			.name	= "SDIO_CTL",
			.owner	= THIS_MODULE,
	},
};

static int __init sdio_ctl_init(void)
{
	msm_sdio_ctl_debug_mask = 0;
	return platform_driver_register(&sdio_ctl_driver);
}

module_init(sdio_ctl_init);
MODULE_DESCRIPTION("MSM SDIO Control Port");
MODULE_LICENSE("GPL v2");

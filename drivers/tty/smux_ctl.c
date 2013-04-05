/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 */

/*
 * Serial Mux Control Driver -- Provides a binary serial muxed control
 * port interface.
 */

#define DEBUG

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <linux/smux.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/poll.h>

#include <asm/ioctls.h>

#define MAX_WRITE_RETRY 5
#define MAGIC_NO_V1 0x33FC
#define DEVICE_NAME "smuxctl"
#define SMUX_CTL_MAX_BUF_SIZE 2048
#define SMUX_CTL_MODULE_NAME "smux_ctl"
#define DEBUG

static int msm_smux_ctl_debug_mask;
module_param_named(debug_mask, msm_smux_ctl_debug_mask,
	int, S_IRUGO | S_IWUSR | S_IWGRP);

static uint32_t smux_ctl_ch_id[] = {
	SMUX_DATA_CTL_0,
};

#define SMUX_CTL_NUM_CHANNELS ARRAY_SIZE(smux_ctl_ch_id)
#define DEFAULT_OPEN_TIMEOUT 5

struct smux_ctl_dev {
	int id;
	char name[10];
	struct cdev cdev;
	struct device *devicep;
	struct mutex dev_lock;
	atomic_t ref_count;
	int state;
	int is_channel_reset;
	int is_high_wm;
	int write_pending;
	unsigned open_timeout_val;

	struct mutex rx_lock;
	uint32_t read_avail;
	struct list_head rx_list;

	int abort_wait;
	wait_queue_head_t read_wait_queue;
	wait_queue_head_t write_wait_queue;

	struct {
		uint32_t bytes_tx;
		uint32_t bytes_rx;
		uint32_t pkts_tx;
		uint32_t pkts_rx;
		uint32_t cnt_ssr;
		uint32_t cnt_read_fail;
		uint32_t cnt_write_fail;
		uint32_t cnt_high_wm_hit;
	} stats;

} *smux_ctl_devp[SMUX_CTL_NUM_CHANNELS];

struct smux_ctl_pkt {
	int data_size;
	void *data;
};

struct smux_ctl_list_elem {
	struct list_head list;
	struct smux_ctl_pkt ctl_pkt;
};

struct class *smux_ctl_classp;
static dev_t smux_ctl_number;
static uint32_t smux_ctl_inited;

enum {
	MSM_SMUX_CTL_DEBUG = 1U << 0,
	MSM_SMUX_CTL_DUMP_BUFFER = 1U << 1,
};

#if defined(DEBUG)

static const char *smux_ctl_event_str[] = {
	"SMUX_CONNECTED",
	"SMUX_DISCONNECTED",
	"SMUX_READ_DONE",
	"SMUX_READ_FAIL",
	"SMUX_WRITE_DONE",
	"SMUX_WRITE_FAIL",
	"SMUX_TIOCM_UPDATE",
	"SMUX_LOW_WM_HIT",
	"SMUX_HIGH_WM_HIT",
};

#define SMUXCTL_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (msm_smux_ctl_debug_mask & MSM_SMUX_CTL_DUMP_BUFFER) { \
		int i; \
		pr_err("%s", prestr); \
		for (i = 0; i < cnt; i++) \
			pr_err("%.2x", buf[i]); \
		pr_err("\n"); \
	} \
} while (0)

#define SMUXCTL_DBG(x...) \
do { \
	if (msm_smux_ctl_debug_mask & MSM_SMUX_CTL_DEBUG) \
		pr_err(x); \
} while (0)


#else
#define SMUXCTL_DUMP_BUFFER(prestr, cnt, buf) do {} while (0)
#define SMUXCTL_DBG(x...) do {} while (0)
#endif

#if defined(DEBUG_LOOPBACK)
#define SMUXCTL_SET_LOOPBACK(lcid) \
	msm_smux_set_ch_option(lcid, SMUX_CH_OPTION_LOCAL_LOOPBACK, 0)
#else
#define SMUXCTL_SET_LOOPBACK(lcid) do {} while (0)
#endif

static ssize_t open_timeout_store(struct device *d,
		struct device_attribute *attr,
		const char *buf,
		size_t n)
{
	int i;
	unsigned long tmp;
	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		if (smux_ctl_devp[i]->devicep == d)
			break;
	}
	if (i >= SMUX_CTL_NUM_CHANNELS) {
		pr_err("%s: unable to match device to valid smux ctl port\n",
				__func__);
		return -EINVAL;
	}
	if (!kstrtoul(buf, 10, &tmp)) {
		smux_ctl_devp[i]->open_timeout_val = tmp;
		return n;
	} else {
		pr_err("%s: unable to convert: %s to an int\n", __func__,
				buf);
		return -EINVAL;
	}
}

static ssize_t open_timeout_show(struct device *d,
		struct device_attribute *attr,
		char *buf)
{
	int i;
	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		if (smux_ctl_devp[i]->devicep == d)
			break;
	}
	if (i >= SMUX_CTL_NUM_CHANNELS) {
		pr_err("%s: unable to match device to valid smux ctl port\n",
				__func__);
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",
			smux_ctl_devp[i]->open_timeout_val);
}

static DEVICE_ATTR(open_timeout, 0664, open_timeout_show, open_timeout_store);

static int get_ctl_dev_index(int id)
{
	int dev_index;
	for (dev_index = 0; dev_index < SMUX_CTL_NUM_CHANNELS; dev_index++) {
		if (smux_ctl_ch_id[dev_index] == id)
			return dev_index;
	}
	return -ENODEV;
}

static int smux_ctl_get_rx_buf_cb(void *priv, void **pkt_priv,
		void **buffer, int size)
{
	void *buf = NULL;
	int id = ((struct smux_ctl_dev *)(priv))->id;
	int dev_index;

	if (id < 0 || id > smux_ctl_ch_id[SMUX_CTL_NUM_CHANNELS - 1])
		return -ENODEV;

	if (!buffer || 0 >= size)
		return -EINVAL;

	dev_index = get_ctl_dev_index(id);
	if (dev_index < 0) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: Ch%d is not "
					"exported to user-space\n",
				__func__, id);
		return -ENODEV;
	}

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: Allocating Rx buf size %d "
			"for ch%d\n",
			__func__, size, smux_ctl_devp[dev_index]->id);

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: buffer allocation failed: "
				"Ch%d, size %d ", __func__, id, size);
		return -ENOMEM;
	}

	*buffer = buf;
	*pkt_priv = NULL;
	return 0;

}

void smux_ctl_notify_cb(void *priv, int event_type, const void *metadata)
{
	int id = ((struct smux_ctl_dev *)(priv))->id;
	struct smux_ctl_list_elem *list_elem = NULL;
	int dev_index;
	void *data;
	int len;

	if (id < 0 || id > smux_ctl_ch_id[SMUX_CTL_NUM_CHANNELS - 1])
		return;

	dev_index = get_ctl_dev_index(id);
	if (dev_index < 0) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: Ch%d is not exported "
				"to user-space\n", __func__, id);
		return;
	}

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: Ch%d, Event %d (%s)\n",
			__func__, smux_ctl_devp[dev_index]->id,
				event_type, smux_ctl_event_str[event_type]);


	switch (event_type) {
	case SMUX_CONNECTED:
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->state = SMUX_CONNECTED;
		smux_ctl_devp[dev_index]->is_high_wm = 0;
		smux_ctl_devp[dev_index]->is_channel_reset = 0;
		smux_ctl_devp[dev_index]->read_avail = 0;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		wake_up(&smux_ctl_devp[dev_index]->write_wait_queue);
		break;

	case SMUX_DISCONNECTED:
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->state = SMUX_DISCONNECTED;
		smux_ctl_devp[dev_index]->is_channel_reset =
			((struct smux_meta_disconnected *)metadata)->is_ssr;
		if (smux_ctl_devp[dev_index]->is_channel_reset)
			smux_ctl_devp[dev_index]->stats.cnt_ssr++;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		wake_up(&smux_ctl_devp[dev_index]->write_wait_queue);
		wake_up(&smux_ctl_devp[dev_index]->read_wait_queue);
		break;

	case SMUX_READ_FAIL:
		data = ((struct smux_meta_read *)metadata)->buffer;
		kfree(data);
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->stats.cnt_read_fail++;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		wake_up(&smux_ctl_devp[dev_index]->read_wait_queue);
		break;

	case SMUX_READ_DONE:
		data = ((struct smux_meta_read *)metadata)->buffer;
		len = ((struct smux_meta_read *)metadata)->len;

		if (data && len > 0) {
			list_elem = kmalloc(sizeof(struct smux_ctl_list_elem),
							GFP_KERNEL);
			if (list_elem) {
				list_elem->ctl_pkt.data = data;
				list_elem->ctl_pkt.data_size = len;

				mutex_lock(&smux_ctl_devp[dev_index]->rx_lock);
				list_add_tail(&list_elem->list,
					&smux_ctl_devp[dev_index]->rx_list);
				smux_ctl_devp[dev_index]->read_avail += len;
				mutex_unlock(
					&smux_ctl_devp[dev_index]->rx_lock);
			} else {
				kfree(data);
			}
		}

		wake_up(&smux_ctl_devp[dev_index]->read_wait_queue);
		break;

	case SMUX_WRITE_DONE:
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->write_pending = 0;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		data = ((struct smux_meta_write *)metadata)->buffer;
		kfree(data);
		wake_up(&smux_ctl_devp[dev_index]->write_wait_queue);
		break;

	case SMUX_WRITE_FAIL:
		data = ((struct smux_meta_write *)metadata)->buffer;
		kfree(data);
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->stats.cnt_write_fail++;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		wake_up(&smux_ctl_devp[dev_index]->write_wait_queue);
		break;

	case SMUX_LOW_WM_HIT:
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->is_high_wm = 0;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		wake_up(&smux_ctl_devp[dev_index]->write_wait_queue);
		break;

	case SMUX_HIGH_WM_HIT:
		mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);
		smux_ctl_devp[dev_index]->is_high_wm = 1;
		smux_ctl_devp[dev_index]->stats.cnt_high_wm_hit++;
		mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);
		break;

	case SMUX_TIOCM_UPDATE:
	default:
		pr_err(SMUX_CTL_MODULE_NAME ": %s: Event %d not supported\n",
				__func__, event_type);
		break;

	}

}

int smux_ctl_open(struct inode *inode, struct file *file)
{
	int r = 0;
	struct smux_ctl_dev *devp;
	unsigned wait_time = DEFAULT_OPEN_TIMEOUT * HZ;

	if (!smux_ctl_inited)
		return -EIO;

	devp = container_of(inode->i_cdev, struct smux_ctl_dev, cdev);
	if (!devp)
		return -ENODEV;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s called on smuxctl%d device\n",
			__func__, devp->id);

	if (1 == atomic_add_return(1, &devp->ref_count)) {

		SMUXCTL_SET_LOOPBACK(devp->id);
		r = msm_smux_open(devp->id,
				devp,
				smux_ctl_notify_cb,
				smux_ctl_get_rx_buf_cb);
		if (r < 0) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s: smux_open failed "
					"for smuxctl%d with rc %d\n",
					__func__, devp->id, r);
			atomic_dec(&devp->ref_count);
			return r;
		}

		if (devp->open_timeout_val)
			wait_time = devp->open_timeout_val * HZ;

		r = wait_event_interruptible_timeout(
				devp->write_wait_queue,
				(devp->state == SMUX_CONNECTED ||
				devp->abort_wait),
				wait_time);
		if (r == 0)
			r = -ETIMEDOUT;

		if (r < 0) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s: "
				"SMUX open timed out: %d, LCID %d\n",
			       __func__, r, devp->id);
			atomic_dec(&devp->ref_count);
			msm_smux_close(devp->id);
			return r;

		} else if (devp->abort_wait) {
			pr_err("%s: %s: Open command aborted\n",
					SMUX_CTL_MODULE_NAME, __func__);
			r = -EIO;
			atomic_dec(&devp->ref_count);
			msm_smux_close(devp->id);
			return r;
		} else if (devp->state != SMUX_CONNECTED) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s: "
				"Invalid open notification\n", __func__);
			r = -ENODEV;
			atomic_dec(&devp->ref_count);
			msm_smux_close(devp->id);
			return r;
		}
	}

	file->private_data = devp;
	return 0;
}

int smux_ctl_release(struct inode *inode, struct file *file)
{
	struct smux_ctl_dev *devp;
	struct smux_ctl_list_elem *list_elem = NULL;

	devp = file->private_data;
	if (!devp)
		return -EINVAL;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s called on smuxctl%d device\n",
			__func__, devp->id);

	mutex_lock(&devp->dev_lock);
	if (atomic_dec_and_test(&devp->ref_count)) {
		mutex_lock(&devp->rx_lock);
		while (!list_empty(&devp->rx_list)) {
			list_elem = list_first_entry(
					&devp->rx_list,
					struct smux_ctl_list_elem,
					list);
			list_del(&list_elem->list);
			kfree(list_elem->ctl_pkt.data);
			kfree(list_elem);
		}
		devp->read_avail = 0;
		mutex_unlock(&devp->rx_lock);
		msm_smux_close(devp->id);
	}
	mutex_unlock(&devp->dev_lock);
	file->private_data = NULL;

	return 0;
}

static int smux_ctl_readable(int id)
{
	int r;
	int dev_index;

	if (id < 0 || id > smux_ctl_ch_id[SMUX_CTL_NUM_CHANNELS - 1])
		return -ENODEV;

	dev_index = get_ctl_dev_index(id);
	if (dev_index < 0) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: Ch%d "
				"is not exported to user-space\n",
			__func__, id);
		return -ENODEV;
	}

	mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);

	if (signal_pending(current))
		r = -ERESTARTSYS;
	else if (smux_ctl_devp[dev_index]->abort_wait)
		r = -ENETRESET;
	else if (smux_ctl_devp[dev_index]->state == SMUX_DISCONNECTED &&
	    smux_ctl_devp[dev_index]->is_channel_reset != 0)
		r = -ENETRESET;

	else if (smux_ctl_devp[dev_index]->state != SMUX_CONNECTED)
		r = -ENODEV;

	else
		r = smux_ctl_devp[dev_index]->read_avail;


	mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);

	return r;

}

ssize_t smux_ctl_read(struct file *file,
			char __user *buf,
			size_t count,
			loff_t *ppos)
{
	int r = 0, id, bytes_to_read, read_err;
	struct smux_ctl_dev *devp;
	struct smux_ctl_list_elem *list_elem = NULL;

	devp = file->private_data;

	if (!devp)
		return -ENODEV;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: read from ch%d\n",
			__func__, devp->id);

	id = devp->id;
	mutex_lock(&devp->rx_lock);
	while (devp->read_avail <= 0) {
		mutex_unlock(&devp->rx_lock);
		r = wait_event_interruptible(devp->read_wait_queue,
				0 != (read_err = smux_ctl_readable(id)));

		if (r < 0) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s:"
					"wait_event_interruptible "
					"ret %i\n", __func__, r);
			return r;
		}

		if (read_err < 0) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s:"
				" Read block failed for Ch%d, err %d\n",
					__func__, devp->id, read_err);
			return read_err;
		}

		mutex_lock(&devp->rx_lock);
	}

	if (list_empty(&devp->rx_list)) {
		mutex_unlock(&devp->rx_lock);
		SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: "
			"Nothing in ch%d's rx_list\n", __func__,
			devp->id);
		return -EAGAIN;
	}

	list_elem = list_first_entry(&devp->rx_list,
			struct smux_ctl_list_elem, list);
	bytes_to_read = (uint32_t)(list_elem->ctl_pkt.data_size);
	if (bytes_to_read > count) {
		mutex_unlock(&devp->rx_lock);
		pr_err(SMUX_CTL_MODULE_NAME ": %s: "
			"Packet size %d > buf size %d\n", __func__,
			bytes_to_read, count);
		return -ENOMEM;
	}

	if (copy_to_user(buf, list_elem->ctl_pkt.data, bytes_to_read)) {
		mutex_unlock(&devp->rx_lock);
		pr_err(SMUX_CTL_MODULE_NAME ": %s: "
			"copy_to_user failed for ch%d\n", __func__,
			devp->id);
		return -EFAULT;
	}

	devp->read_avail -= bytes_to_read;
	list_del(&list_elem->list);
	kfree(list_elem->ctl_pkt.data);
	kfree(list_elem);
	devp->stats.pkts_rx++;
	devp->stats.bytes_rx += bytes_to_read;
	mutex_unlock(&devp->rx_lock);

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: "
		"Returning %d bytes to ch%d\n", __func__,
			bytes_to_read, devp->id);
	return bytes_to_read;
}

static int smux_ctl_writeable(int id)
{
	int r;
	int dev_index;

	if (id < 0 || id > smux_ctl_ch_id[SMUX_CTL_NUM_CHANNELS - 1])
		return -ENODEV;

	dev_index = get_ctl_dev_index(id);
	if (dev_index < 0) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: "
			"Ch%d is not exported to user-space\n",
			__func__, id);
		return -ENODEV;
	}

	mutex_lock(&smux_ctl_devp[dev_index]->dev_lock);

	if (signal_pending(current))
		r = -ERESTARTSYS;

	else if (smux_ctl_devp[dev_index]->abort_wait)
		r = -ENETRESET;
	else if (smux_ctl_devp[dev_index]->state == SMUX_DISCONNECTED &&
	    smux_ctl_devp[dev_index]->is_channel_reset != 0)
		r = -ENETRESET;
	else if (smux_ctl_devp[dev_index]->state != SMUX_CONNECTED)
		r = -ENODEV;
	else if (smux_ctl_devp[dev_index]->is_high_wm ||
			smux_ctl_devp[dev_index]->write_pending)
		r = 0;
	else
		r = SMUX_CTL_MAX_BUF_SIZE;

	mutex_unlock(&smux_ctl_devp[dev_index]->dev_lock);

	return r;

}

ssize_t smux_ctl_write(struct file *file,
		const char __user *buf,
		size_t count,
		loff_t *ppos)
{
	int r = 0, id, write_err;
	char *temp_buf;
	struct smux_ctl_dev *devp;

	if (count <= 0)
		return -EINVAL;

	devp = file->private_data;
	if (!devp)
		return -ENODEV;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: writing %i bytes on ch%d\n",
			__func__, count, devp->id);

	id = devp->id;
	r = wait_event_interruptible(devp->write_wait_queue,
			0 != (write_err = smux_ctl_writeable(id)));

	if (r < 0) {
		pr_err(SMUX_CTL_MODULE_NAME
				": %s: wait_event_interruptible "
				"ret %i\n", __func__, r);
		return r;
	}

	if (write_err < 0) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s:"
				"Write block failed for Ch%d, err %d\n",
				__func__, devp->id, write_err);
		return write_err;
	}

	temp_buf = kmalloc(count, GFP_KERNEL);
	if (!temp_buf) {
		pr_err(SMUX_CTL_MODULE_NAME
				": %s: temp_buf alloc failed\n", __func__);
		return -ENOMEM;
	}

	if (copy_from_user(temp_buf, buf, count)) {
		pr_err(SMUX_CTL_MODULE_NAME
				": %s: copy_from_user failed\n", __func__);
		kfree(temp_buf);
		return -EFAULT;
	}

	mutex_lock(&devp->dev_lock);
	devp->write_pending = 1;
	mutex_unlock(&devp->dev_lock);

	r = msm_smux_write(id, NULL, (void *)temp_buf, count);
	if (r < 0) {
		pr_err(SMUX_CTL_MODULE_NAME
			": %s: smux_write on Ch%dfailed, err %d\n",
				__func__, id, r);
		mutex_lock(&devp->dev_lock);
		devp->write_pending = 0;
		mutex_unlock(&devp->dev_lock);
		return r;
	}

	r = wait_event_interruptible(devp->write_wait_queue,
			0 != (write_err = smux_ctl_writeable(id)));

	if (-EIO == r) {
		pr_err("%s: %s: wait_event_interruptible ret %i\n",
				SMUX_CTL_MODULE_NAME, __func__, r);
		return -EIO;
	}

	if (r < 0) {
		pr_err(SMUX_CTL_MODULE_NAME " :%s: wait_event_interruptible "
				"ret %i\n", __func__, r);
		mutex_lock(&devp->dev_lock);
		devp->write_pending = 0;
		mutex_unlock(&devp->dev_lock);
		return r;
	}

	mutex_lock(&devp->dev_lock);
	devp->write_pending = 0;
	devp->stats.pkts_tx++;
	devp->stats.bytes_tx += count;
	mutex_unlock(&devp->dev_lock);
	return count;
}

static long smux_ctl_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	int ret;
	struct smux_ctl_dev *devp;

	devp = file->private_data;
	if (!devp)
		return -ENODEV;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s called on smuxctl%d device\n",
			__func__, devp->id);

	switch (cmd) {
	case TIOCMGET:
		ret = msm_smux_tiocm_get(devp->id);
		break;
	case TIOCMSET:
		ret = msm_smux_tiocm_set(devp->id, arg, ~arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int smux_ctl_poll(struct file *file, poll_table *wait)
{
	struct smux_ctl_dev *devp;
	unsigned int mask = 0;
	int readable;

	devp = file->private_data;
	if (!devp)
		return -ENODEV;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s called on smuxctl%d\n",
			__func__, devp->id);

	poll_wait(file, &devp->read_wait_queue, wait);

	readable = smux_ctl_readable(devp->id);
	if (readable > 0) {
		mask = POLLIN | POLLRDNORM;
	} else if ((readable < 0) && (readable != -ERESTARTSYS)) {
		/* error case (non-signal) received */
		pr_err(SMUX_CTL_MODULE_NAME ": %s err%d during poll for smuxctl%d\n",
			__func__, readable, devp->id);
		mask = POLLERR;
	}

	return mask;
}

static const struct file_operations smux_ctl_fops = {
	.owner = THIS_MODULE,
	.open = smux_ctl_open,
	.release = smux_ctl_release,
	.read = smux_ctl_read,
	.write = smux_ctl_write,
	.unlocked_ioctl = smux_ctl_ioctl,
	.poll = smux_ctl_poll,
};

static void smux_ctl_reset_channel(struct smux_ctl_dev *devp)
{
	devp->is_high_wm = 0;
	devp->write_pending = 0;
	devp->is_channel_reset = 0;
	devp->state = SMUX_DISCONNECTED;
	devp->read_avail = 0;

	devp->stats.bytes_tx = 0;
	devp->stats.bytes_rx = 0;
	devp->stats.pkts_tx = 0;
	devp->stats.pkts_rx = 0;
	devp->stats.cnt_ssr = 0;
	devp->stats.cnt_read_fail = 0;
	devp->stats.cnt_write_fail = 0;
	devp->stats.cnt_high_wm_hit = 0;
	devp->abort_wait = 0;
}

static int smux_ctl_probe(struct platform_device *pdev)
{
	int i;
	int r;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s Begins\n", __func__);

	if (smux_ctl_inited) {
		/* Already loaded once - reinitialize channels */
		for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
			struct smux_ctl_dev *devp = smux_ctl_devp[i];

			smux_ctl_reset_channel(devp);

			if (atomic_read(&devp->ref_count)) {
				r = msm_smux_open(devp->id,
						devp,
						smux_ctl_notify_cb,
						smux_ctl_get_rx_buf_cb);
				if (r)
					pr_err("%s: unable to reopen ch %d, ret %d\n",
							__func__, devp->id, r);
			}
		}
		return 0;
	}

	/* Create character devices */
	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		smux_ctl_devp[i] = kzalloc(sizeof(struct smux_ctl_dev),
							GFP_KERNEL);
		if (IS_ERR(smux_ctl_devp[i])) {
			pr_err(SMUX_CTL_MODULE_NAME
				 ": %s kmalloc() ENOMEM\n", __func__);
			r = -ENOMEM;
			goto error0;
		}

		smux_ctl_devp[i]->id = smux_ctl_ch_id[i];
		atomic_set(&smux_ctl_devp[i]->ref_count, 0);

		mutex_init(&smux_ctl_devp[i]->dev_lock);
		init_waitqueue_head(&smux_ctl_devp[i]->read_wait_queue);
		init_waitqueue_head(&smux_ctl_devp[i]->write_wait_queue);
		mutex_init(&smux_ctl_devp[i]->rx_lock);
		INIT_LIST_HEAD(&smux_ctl_devp[i]->rx_list);
		smux_ctl_reset_channel(smux_ctl_devp[i]);
	}

	r = alloc_chrdev_region(&smux_ctl_number, 0, SMUX_CTL_NUM_CHANNELS,
							DEVICE_NAME);
	if (IS_ERR_VALUE(r)) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: "
				"alloc_chrdev_region() ret %i.\n",
					 __func__, r);
		goto error0;
	}

	smux_ctl_classp = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(smux_ctl_classp)) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: "
				"class_create() ENOMEM\n", __func__);
		r = -ENOMEM;
		goto error1;
	}

	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		cdev_init(&smux_ctl_devp[i]->cdev, &smux_ctl_fops);
		smux_ctl_devp[i]->cdev.owner = THIS_MODULE;

		r = cdev_add(&smux_ctl_devp[i]->cdev,
				(smux_ctl_number + i), 1);

		if (IS_ERR_VALUE(r)) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s: "
					"cdev_add() ret %i\n", __func__, r);
			kfree(smux_ctl_devp[i]);
			goto error2;
		}

		smux_ctl_devp[i]->devicep =
				device_create(smux_ctl_classp, NULL,
					(smux_ctl_number + i), NULL,
					DEVICE_NAME "%d", smux_ctl_ch_id[i]);

		if (IS_ERR(smux_ctl_devp[i]->devicep)) {
			pr_err(SMUX_CTL_MODULE_NAME ": %s: "
					"device_create() ENOMEM\n", __func__);
			r = -ENOMEM;
			cdev_del(&smux_ctl_devp[i]->cdev);
			kfree(smux_ctl_devp[i]);
			goto error2;
		}
		if (device_create_file(smux_ctl_devp[i]->devicep,
				&dev_attr_open_timeout))
			pr_err("%s: unable to create device attr for" \
				" smux ctl dev id:%d\n", __func__, i);

	}

	smux_ctl_inited = 1;
	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s: "
		"SMUX Control Port Driver Initialized.\n", __func__);
	return 0;

error2:
	while (--i >= 0) {
		cdev_del(&smux_ctl_devp[i]->cdev);
		device_destroy(smux_ctl_classp,
			MKDEV(MAJOR(smux_ctl_number), i));
	}

	class_destroy(smux_ctl_classp);
	i = SMUX_CTL_NUM_CHANNELS;

error1:
	unregister_chrdev_region(MAJOR(smux_ctl_number),
			SMUX_CTL_NUM_CHANNELS);

error0:
	while (--i >= 0)
		kfree(smux_ctl_devp[i]);

	return r;
}

static int smux_ctl_remove(struct platform_device *pdev)
{
	int i;
	int ret;

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s Begins\n", __func__);

	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		struct smux_ctl_dev *devp = smux_ctl_devp[i];

		mutex_lock(&devp->dev_lock);
		devp->abort_wait = 1;
		wake_up(&devp->write_wait_queue);
		wake_up(&devp->read_wait_queue);

		if (atomic_read(&devp->ref_count)) {
			ret = msm_smux_close(devp->id);
			if (ret)
				pr_err("%s: unable to close ch %d, ret %d\n",
						__func__, devp->id, ret);
		}
		mutex_unlock(&devp->dev_lock);

		/* Empty RX queue */
		mutex_lock(&devp->rx_lock);
		while (!list_empty(&devp->rx_list)) {
			struct smux_ctl_list_elem *list_elem;

			list_elem = list_first_entry(
					&devp->rx_list,
					struct smux_ctl_list_elem,
					list);
			list_del(&list_elem->list);
			kfree(list_elem->ctl_pkt.data);
			kfree(list_elem);
		}
		devp->read_avail = 0;
		mutex_unlock(&devp->rx_lock);
	}

	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s Ends\n", __func__);
	return 0;
}

static struct platform_driver smux_ctl_driver = {
	.probe = smux_ctl_probe,
	.remove = smux_ctl_remove,
	.driver = {
		.name = "SMUX_CTL",
		.owner = THIS_MODULE,
	},
};

static int __init smux_ctl_init(void)
{
	SMUXCTL_DBG(SMUX_CTL_MODULE_NAME ": %s Begins\n", __func__);
	return platform_driver_register(&smux_ctl_driver);
}


#if defined(CONFIG_DEBUG_FS)

#define DEBUG_BUFMAX 4096
static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
				size_t count, loff_t *ppos)
{
	int bsize = 0;
	int i;
	if (!smux_ctl_inited) {
		pr_err(SMUX_CTL_MODULE_NAME ": %s: SMUX_CTL not yet inited\n",
				__func__);
		return -EIO;
	}

	bsize += scnprintf(debug_buffer + bsize, DEBUG_BUFMAX - bsize,
				"SMUX_CTL Channel States:\n");

	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		bsize += scnprintf(debug_buffer + bsize, DEBUG_BUFMAX - bsize,
		"Ch%02d %s RefCnt=%01d State=%02d "
		"SSR=%02d HighWM=%02d ReadAvail=%04d WritePending=%02d\n",
		smux_ctl_devp[i]->id,
		smux_ctl_devp[i]->name,
		atomic_read(&smux_ctl_devp[i]->ref_count),
		smux_ctl_devp[i]->state,
		smux_ctl_devp[i]->is_channel_reset,
		smux_ctl_devp[i]->is_high_wm,
		smux_ctl_devp[i]->read_avail,
		smux_ctl_devp[i]->write_pending);
	}

	bsize += scnprintf(debug_buffer + bsize, DEBUG_BUFMAX - bsize,
				"\nSMUX_CTL Channel Statistics:\n");
	for (i = 0; i < SMUX_CTL_NUM_CHANNELS; ++i) {
		bsize += scnprintf(debug_buffer + bsize, DEBUG_BUFMAX - bsize,
			"Ch%02d %s BytesTX=%08d "
				"BytesRx=%08d PktsTx=%04d PktsRx=%04d"
			"CntSSR=%02d CntHighWM=%02d "
				"CntReadFail%02d CntWriteFailed=%02d\n",
			smux_ctl_devp[i]->id,
			smux_ctl_devp[i]->name,
			smux_ctl_devp[i]->stats.bytes_tx,
			smux_ctl_devp[i]->stats.bytes_rx,
			smux_ctl_devp[i]->stats.pkts_tx,
			smux_ctl_devp[i]->stats.pkts_rx,
			smux_ctl_devp[i]->stats.cnt_ssr,
			smux_ctl_devp[i]->stats.cnt_high_wm_hit,
			smux_ctl_devp[i]->stats.cnt_read_fail,
			smux_ctl_devp[i]->stats.cnt_write_fail);
	}

	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static int __init smux_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smux_ctl", 0);
	if (!IS_ERR(dent))
		debugfs_create_file("smux_ctl_state", 0444, dent,
			NULL, &debug_ops);

	return 0;
}

late_initcall(smux_debugfs_init);
#endif

module_init(smux_ctl_init);
MODULE_DESCRIPTION("MSM SMUX Control Port");
MODULE_LICENSE("GPL v2");



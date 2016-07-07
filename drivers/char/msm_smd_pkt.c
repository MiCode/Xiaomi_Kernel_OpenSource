/* Copyright (c) 2008-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * SMD Packet Driver -- Provides a binary SMD non-muxed packet port
 *                       interface.
 */

#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/msm_smd_pkt.h>
#include <linux/poll.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/smsm.h>
#include <soc/qcom/subsystem_restart.h>
#include <asm/ioctls.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/ipc_logging.h>

#define MODULE_NAME "msm_smdpkt"
#define DEVICE_NAME "smdpkt"
#define WAKEUPSOURCE_TIMEOUT (2000) /* two seconds */

struct smd_pkt_dev {
	struct list_head dev_list;
	char dev_name[SMD_MAX_CH_NAME_LEN];
	char ch_name[SMD_MAX_CH_NAME_LEN];
	uint32_t edge;

	struct cdev cdev;
	struct device *devicep;
	void *pil;

	struct smd_channel *ch;
	struct mutex ch_lock;
	struct mutex rx_lock;
	struct mutex tx_lock;
	wait_queue_head_t ch_read_wait_queue;
	wait_queue_head_t ch_write_wait_queue;
	wait_queue_head_t ch_opened_wait_queue;

	int i;
	int ref_cnt;

	int blocking_write;
	int is_open;
	int poll_mode;
	unsigned ch_size;
	uint open_modem_wait;

	int has_reset;
	int do_reset_notification;
	struct completion ch_allocated;
	struct wakeup_source pa_ws;	/* Packet Arrival Wakeup Source */
	struct work_struct packet_arrival_work;
	spinlock_t pa_spinlock;
	int ws_locked;
};


struct smd_pkt_driver {
	struct list_head list;
	int ref_cnt;
	char pdriver_name[SMD_MAX_CH_NAME_LEN];
	struct platform_driver driver;
};

static DEFINE_MUTEX(smd_pkt_driver_lock_lha1);
static LIST_HEAD(smd_pkt_driver_list);

struct class *smd_pkt_classp;
static dev_t smd_pkt_number;
static struct delayed_work loopback_work;
static void check_and_wakeup_reader(struct smd_pkt_dev *smd_pkt_devp);
static void check_and_wakeup_writer(struct smd_pkt_dev *smd_pkt_devp);
static uint32_t is_modem_smsm_inited(void);

static DEFINE_MUTEX(smd_pkt_dev_lock_lha1);
static LIST_HEAD(smd_pkt_dev_list);
static int num_smd_pkt_ports;

#define SMD_PKT_IPC_LOG_PAGE_CNT 2
static void *smd_pkt_ilctxt;

static int msm_smd_pkt_debug_mask;
module_param_named(debug_mask, msm_smd_pkt_debug_mask,
		int, S_IRUGO | S_IWUSR | S_IWGRP);

enum {
	SMD_PKT_STATUS = 1U << 0,
	SMD_PKT_READ = 1U << 1,
	SMD_PKT_WRITE = 1U << 2,
	SMD_PKT_POLL = 1U << 5,
};

#define DEBUG

#ifdef DEBUG

#define SMD_PKT_LOG_STRING(x...) \
do { \
	if (smd_pkt_ilctxt) \
		ipc_log_string(smd_pkt_ilctxt, "<SMD_PKT>: "x); \
} while (0)

#define D_STATUS(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_STATUS) \
		pr_info("Status: "x); \
	SMD_PKT_LOG_STRING(x); \
} while (0)

#define D_READ(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_READ) \
		pr_info("Read: "x); \
	SMD_PKT_LOG_STRING(x); \
} while (0)

#define D_WRITE(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_WRITE) \
		pr_info("Write: "x); \
	SMD_PKT_LOG_STRING(x); \
} while (0)

#define D_POLL(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_POLL) \
		pr_info("Poll: "x); \
	SMD_PKT_LOG_STRING(x); \
} while (0)

#define E_SMD_PKT_SSR(x) \
do { \
	if (x->do_reset_notification) \
		pr_err("%s notifying reset for smd_pkt_dev id:%d\n", \
			__func__, x->i); \
} while (0)
#else
#define D_STATUS(x...) do {} while (0)
#define D_READ(x...) do {} while (0)
#define D_WRITE(x...) do {} while (0)
#define D_POLL(x...) do {} while (0)
#define E_SMD_PKT_SSR(x) do {} while (0)
#endif

static ssize_t open_timeout_store(struct device *d,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned long tmp;

	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_for_each_entry(smd_pkt_devp, &smd_pkt_dev_list, dev_list) {
		if (smd_pkt_devp->devicep == d) {
			if (!kstrtoul(buf, 10, &tmp)) {
				smd_pkt_devp->open_modem_wait = tmp;
				mutex_unlock(&smd_pkt_dev_lock_lha1);
				return n;
			} else {
				mutex_unlock(&smd_pkt_dev_lock_lha1);
				pr_err("%s: unable to convert: %s to an int\n",
						__func__, buf);
				return -EINVAL;
			}
		}
	}
	mutex_unlock(&smd_pkt_dev_lock_lha1);

	pr_err("%s: unable to match device to valid smd_pkt port\n", __func__);
	return -EINVAL;
}

static ssize_t open_timeout_show(struct device *d,
				 struct device_attribute *attr,
				 char *buf)
{
	struct smd_pkt_dev *smd_pkt_devp;

	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_for_each_entry(smd_pkt_devp, &smd_pkt_dev_list, dev_list) {
		if (smd_pkt_devp->devicep == d) {
			mutex_unlock(&smd_pkt_dev_lock_lha1);
			return snprintf(buf, PAGE_SIZE, "%d\n",
					smd_pkt_devp->open_modem_wait);
		}
	}
	mutex_unlock(&smd_pkt_dev_lock_lha1);
	pr_err("%s: unable to match device to valid smd_pkt port\n", __func__);
	return -EINVAL;

}

static DEVICE_ATTR(open_timeout, 0664, open_timeout_show, open_timeout_store);

/**
 * loopback_edge_store() - Set the edge type for loopback device
 * @d:		Linux device structure
 * @attr:	Device attribute structure
 * @buf:	Input string
 * @n:		Length of the input string
 *
 * This function is used to set the loopback device edge runtime
 * by writing to the loopback_edge node.
 */
static ssize_t loopback_edge_store(struct device *d,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned long tmp;

	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_for_each_entry(smd_pkt_devp, &smd_pkt_dev_list, dev_list) {
		if (smd_pkt_devp->devicep == d) {
			if (!kstrtoul(buf, 10, &tmp)) {
				smd_pkt_devp->edge = tmp;
				mutex_unlock(&smd_pkt_dev_lock_lha1);
				return n;
			} else {
				mutex_unlock(&smd_pkt_dev_lock_lha1);
				pr_err("%s: unable to convert: %s to an int\n",
						__func__, buf);
				return -EINVAL;
			}
		}
	}
	mutex_unlock(&smd_pkt_dev_lock_lha1);
	pr_err("%s: unable to match device to valid smd_pkt port\n", __func__);
	return -EINVAL;
}

/**
 * loopback_edge_show() - Get the edge type for loopback device
 * @d:		Linux device structure
 * @attr:	Device attribute structure
 * @buf:	Output buffer
 *
 * This function is used to get the loopback device edge runtime
 * by reading the loopback_edge node.
 */
static ssize_t loopback_edge_show(struct device *d,
				 struct device_attribute *attr,
				 char *buf)
{
	struct smd_pkt_dev *smd_pkt_devp;

	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_for_each_entry(smd_pkt_devp, &smd_pkt_dev_list, dev_list) {
		if (smd_pkt_devp->devicep == d) {
			mutex_unlock(&smd_pkt_dev_lock_lha1);
			return snprintf(buf, PAGE_SIZE, "%d\n",
					smd_pkt_devp->edge);
		}
	}
	mutex_unlock(&smd_pkt_dev_lock_lha1);
	pr_err("%s: unable to match device to valid smd_pkt port\n", __func__);
	return -EINVAL;

}

static DEVICE_ATTR(loopback_edge, 0664, loopback_edge_show,
						loopback_edge_store);

static int notify_reset(struct smd_pkt_dev *smd_pkt_devp)
{
	smd_pkt_devp->do_reset_notification = 0;

	return -ENETRESET;
}

static void clean_and_signal(struct smd_pkt_dev *smd_pkt_devp)
{
	smd_pkt_devp->do_reset_notification = 1;
	smd_pkt_devp->has_reset = 1;

	smd_pkt_devp->is_open = 0;

	wake_up(&smd_pkt_devp->ch_read_wait_queue);
	wake_up(&smd_pkt_devp->ch_write_wait_queue);
	wake_up_interruptible(&smd_pkt_devp->ch_opened_wait_queue);
	D_STATUS("%s smd_pkt_dev id:%d\n", __func__, smd_pkt_devp->i);
}

static void loopback_probe_worker(struct work_struct *work)
{

	/* Wait for the modem SMSM to be inited for the SMD
	** Loopback channel to be allocated at the modem. Since
	** the wait need to be done atmost once, using msleep
	** doesn't degrade the performance. */
	if (!is_modem_smsm_inited())
		schedule_delayed_work(&loopback_work, msecs_to_jiffies(1000));
	else
		smsm_change_state(SMSM_APPS_STATE,
			  0, SMSM_SMD_LOOPBACK);

}

static void packet_arrival_worker(struct work_struct *work)
{
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned long flags;

	smd_pkt_devp = container_of(work, struct smd_pkt_dev,
				    packet_arrival_work);
	mutex_lock(&smd_pkt_devp->ch_lock);
	spin_lock_irqsave(&smd_pkt_devp->pa_spinlock, flags);
	if (smd_pkt_devp->ch && smd_pkt_devp->ws_locked) {
		D_READ("%s locking smd_pkt_dev id:%d wakeup source\n",
			__func__, smd_pkt_devp->i);
		/*
		 * Keep system awake long enough to allow userspace client
		 * to process the packet.
		 */
		__pm_wakeup_event(&smd_pkt_devp->pa_ws, WAKEUPSOURCE_TIMEOUT);
	}
	spin_unlock_irqrestore(&smd_pkt_devp->pa_spinlock, flags);
	mutex_unlock(&smd_pkt_devp->ch_lock);
}

static long smd_pkt_ioctl(struct file *file, unsigned int cmd,
					     unsigned long arg)
{
	int ret;
	struct smd_pkt_dev *smd_pkt_devp;
	uint32_t val;

	smd_pkt_devp = file->private_data;
	if (!smd_pkt_devp)
		return -EINVAL;

	mutex_lock(&smd_pkt_devp->ch_lock);
	switch (cmd) {
	case TIOCMGET:
		D_STATUS("%s TIOCMGET command on smd_pkt_dev id:%d\n",
			 __func__, smd_pkt_devp->i);
		ret = smd_tiocmget(smd_pkt_devp->ch);
		break;
	case TIOCMSET:
		ret = get_user(val, (uint32_t *)arg);
		if (ret) {
			pr_err("Error getting TIOCMSET value\n");
			mutex_unlock(&smd_pkt_devp->ch_lock);
			return ret;
		}
		D_STATUS("%s TIOCSET command on smd_pkt_dev id:%d arg[0x%x]\n",
			 __func__, smd_pkt_devp->i, val);
		ret = smd_tiocmset(smd_pkt_devp->ch, val, ~val);
		break;
	case SMD_PKT_IOCTL_BLOCKING_WRITE:
		ret = get_user(smd_pkt_devp->blocking_write, (int *)arg);
		break;
	default:
		pr_err_ratelimited("%s: Unrecognized ioctl command %d\n",
			__func__, cmd);
		ret = -ENOIOCTLCMD;
	}
	mutex_unlock(&smd_pkt_devp->ch_lock);

	return ret;
}

ssize_t smd_pkt_read(struct file *file,
		       char __user *_buf,
		       size_t count,
		       loff_t *ppos)
{
	int r;
	int bytes_read;
	int pkt_size;
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned long flags;
	void *buf;

	smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp) {
		pr_err_ratelimited("%s on NULL smd_pkt_dev\n", __func__);
		return -EINVAL;
	}

	if (!smd_pkt_devp->ch) {
		pr_err_ratelimited("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return -EINVAL;
	}

	if (smd_pkt_devp->do_reset_notification) {
		/* notify client that a reset occurred */
		E_SMD_PKT_SSR(smd_pkt_devp);
		return notify_reset(smd_pkt_devp);
	}
	D_READ("Begin %s on smd_pkt_dev id:%d buffer_size %zu\n",
		__func__, smd_pkt_devp->i, count);

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

wait_for_packet:
	r = wait_event_interruptible(smd_pkt_devp->ch_read_wait_queue,
				     !smd_pkt_devp->ch ||
				     (smd_cur_packet_size(smd_pkt_devp->ch) > 0
				      && smd_read_avail(smd_pkt_devp->ch)) ||
				     smd_pkt_devp->has_reset);

	mutex_lock(&smd_pkt_devp->rx_lock);
	if (smd_pkt_devp->has_reset) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		E_SMD_PKT_SSR(smd_pkt_devp);
		kfree(buf);
		return notify_reset(smd_pkt_devp);
	}

	if (!smd_pkt_devp->ch) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		pr_err_ratelimited("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		kfree(buf);
		return -EINVAL;
	}

	if (r < 0) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		/* qualify error message */
		if (r != -ERESTARTSYS) {
			/* we get this anytime a signal comes in */
			pr_err_ratelimited("%s: wait_event_interruptible on smd_pkt_dev id:%d ret %i\n",
				__func__, smd_pkt_devp->i, r);
		}
		kfree(buf);
		return r;
	}

	/* Here we have a whole packet waiting for us */
	pkt_size = smd_cur_packet_size(smd_pkt_devp->ch);

	if (!pkt_size) {
		pr_err_ratelimited("%s: No data on smd_pkt_dev id:%d, False wakeup\n",
			__func__, smd_pkt_devp->i);
		mutex_unlock(&smd_pkt_devp->rx_lock);
		goto wait_for_packet;
	}

	if (pkt_size < 0) {
		pr_err_ratelimited("%s: Error %d obtaining packet size for Channel %s",
				__func__, pkt_size, smd_pkt_devp->ch_name);
		kfree(buf);
		return pkt_size;
	}

	if ((uint32_t)pkt_size > count) {
		pr_err_ratelimited("%s: failure on smd_pkt_dev id: %d - packet size %d > buffer size %zu,",
			__func__, smd_pkt_devp->i,
			pkt_size, count);
		mutex_unlock(&smd_pkt_devp->rx_lock);
		kfree(buf);
		return -ETOOSMALL;
	}

	bytes_read = 0;
	do {
		r = smd_read(smd_pkt_devp->ch,
					 (buf + bytes_read),
					 (pkt_size - bytes_read));
		if (r < 0) {
			mutex_unlock(&smd_pkt_devp->rx_lock);
			if (smd_pkt_devp->has_reset) {
				E_SMD_PKT_SSR(smd_pkt_devp);
				return notify_reset(smd_pkt_devp);
			}
			pr_err_ratelimited("%s Error while reading %d\n",
				__func__, r);
			kfree(buf);
			return r;
		}
		bytes_read += r;
		if (pkt_size != bytes_read)
			wait_event(smd_pkt_devp->ch_read_wait_queue,
				   smd_read_avail(smd_pkt_devp->ch) ||
				   smd_pkt_devp->has_reset);
		if (smd_pkt_devp->has_reset) {
			mutex_unlock(&smd_pkt_devp->rx_lock);
			E_SMD_PKT_SSR(smd_pkt_devp);
			kfree(buf);
			return notify_reset(smd_pkt_devp);
		}
	} while (pkt_size != bytes_read);
	mutex_unlock(&smd_pkt_devp->rx_lock);

	mutex_lock(&smd_pkt_devp->ch_lock);
	spin_lock_irqsave(&smd_pkt_devp->pa_spinlock, flags);
	if (smd_pkt_devp->poll_mode &&
	    !smd_cur_packet_size(smd_pkt_devp->ch)) {
		__pm_relax(&smd_pkt_devp->pa_ws);
		smd_pkt_devp->ws_locked = 0;
		smd_pkt_devp->poll_mode = 0;
		D_READ("%s unlocked smd_pkt_dev id:%d wakeup_source\n",
			__func__, smd_pkt_devp->i);
	}
	spin_unlock_irqrestore(&smd_pkt_devp->pa_spinlock, flags);
	mutex_unlock(&smd_pkt_devp->ch_lock);

	r = copy_to_user(_buf, buf, bytes_read);
	if (r) {
		kfree(buf);
		return -EFAULT;
	}
	D_READ("Finished %s on smd_pkt_dev id:%d  %d bytes\n",
		__func__, smd_pkt_devp->i, bytes_read);
	kfree(buf);

	/* check and wakeup read threads waiting on this device */
	check_and_wakeup_reader(smd_pkt_devp);

	return bytes_read;
}

ssize_t smd_pkt_write(struct file *file,
		       const char __user *_buf,
		       size_t count,
		       loff_t *ppos)
{
	int r = 0, bytes_written;
	struct smd_pkt_dev *smd_pkt_devp;
	DEFINE_WAIT(write_wait);
	void *buf;

	smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp) {
		pr_err_ratelimited("%s on NULL smd_pkt_dev\n", __func__);
		return -EINVAL;
	}

	if (!smd_pkt_devp->ch) {
		pr_err_ratelimited("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return -EINVAL;
	}

	if (smd_pkt_devp->do_reset_notification || smd_pkt_devp->has_reset) {
		E_SMD_PKT_SSR(smd_pkt_devp);
		/* notify client that a reset occurred */
		return notify_reset(smd_pkt_devp);
	}
	D_WRITE("Begin %s on smd_pkt_dev id:%d data_size %zu\n",
		__func__, smd_pkt_devp->i, count);

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	r = copy_from_user(buf, _buf, count);
	if (r) {
		kfree(buf);
		return -EFAULT;
	}

	mutex_lock(&smd_pkt_devp->tx_lock);
	if (!smd_pkt_devp->blocking_write) {
		if (smd_write_avail(smd_pkt_devp->ch) < count) {
			pr_err_ratelimited("%s: Not enough space in smd_pkt_dev id:%d\n",
				   __func__, smd_pkt_devp->i);
			mutex_unlock(&smd_pkt_devp->tx_lock);
			kfree(buf);
			return -ENOMEM;
		}
	}

	r = smd_write_start(smd_pkt_devp->ch, count);
	if (r < 0) {
		mutex_unlock(&smd_pkt_devp->tx_lock);
		pr_err_ratelimited("%s: Error:%d in smd_pkt_dev id:%d @ smd_write_start\n",
			__func__, r, smd_pkt_devp->i);
		kfree(buf);
		return r;
	}

	bytes_written = 0;
	do {
		prepare_to_wait(&smd_pkt_devp->ch_write_wait_queue,
				&write_wait, TASK_UNINTERRUPTIBLE);
		if (!smd_write_segment_avail(smd_pkt_devp->ch) &&
		    !smd_pkt_devp->has_reset) {
			smd_enable_read_intr(smd_pkt_devp->ch);
			schedule();
		}
		finish_wait(&smd_pkt_devp->ch_write_wait_queue, &write_wait);
		smd_disable_read_intr(smd_pkt_devp->ch);

		if (smd_pkt_devp->has_reset) {
			mutex_unlock(&smd_pkt_devp->tx_lock);
			E_SMD_PKT_SSR(smd_pkt_devp);
			kfree(buf);
			return notify_reset(smd_pkt_devp);
		} else {
			r = smd_write_segment(smd_pkt_devp->ch,
					      (void *)(buf + bytes_written),
					      (count - bytes_written));
			if (r < 0) {
				mutex_unlock(&smd_pkt_devp->tx_lock);
				if (smd_pkt_devp->has_reset) {
					E_SMD_PKT_SSR(smd_pkt_devp);
					return notify_reset(smd_pkt_devp);
				}
				pr_err_ratelimited("%s on smd_pkt_dev id:%d failed r:%d\n",
					__func__, smd_pkt_devp->i, r);
				kfree(buf);
				return r;
			}
			bytes_written += r;
		}
	} while (bytes_written != count);
	smd_write_end(smd_pkt_devp->ch);
	mutex_unlock(&smd_pkt_devp->tx_lock);
	D_WRITE("Finished %s on smd_pkt_dev id:%d %zu bytes\n",
		__func__, smd_pkt_devp->i, count);

	kfree(buf);
	return count;
}

static unsigned int smd_pkt_poll(struct file *file, poll_table *wait)
{
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned int mask = 0;

	smd_pkt_devp = file->private_data;
	if (!smd_pkt_devp) {
		pr_err_ratelimited("%s on a NULL device\n", __func__);
		return POLLERR;
	}

	smd_pkt_devp->poll_mode = 1;
	poll_wait(file, &smd_pkt_devp->ch_read_wait_queue, wait);
	mutex_lock(&smd_pkt_devp->ch_lock);
	if (smd_pkt_devp->has_reset || !smd_pkt_devp->ch) {
		mutex_unlock(&smd_pkt_devp->ch_lock);
		return POLLERR;
	}

	if (smd_read_avail(smd_pkt_devp->ch)) {
		mask |= POLLIN | POLLRDNORM;
		D_POLL("%s sets POLLIN for smd_pkt_dev id: %d\n",
			__func__, smd_pkt_devp->i);
	}
	mutex_unlock(&smd_pkt_devp->ch_lock);

	return mask;
}

static void check_and_wakeup_reader(struct smd_pkt_dev *smd_pkt_devp)
{
	int sz;
	unsigned long flags;

	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return;
	}

	if (!smd_pkt_devp->ch) {
		pr_err("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return;
	}

	sz = smd_cur_packet_size(smd_pkt_devp->ch);
	if (sz == 0) {
		D_READ("%s: No packet in smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return;
	}
	if (!smd_read_avail(smd_pkt_devp->ch)) {
		D_READ(
			"%s: packet size is %d in smd_pkt_dev id:%d - but the data isn't here\n",
			__func__, sz, smd_pkt_devp->i);
		return;
	}

	/* here we have a packet of size sz ready */
	spin_lock_irqsave(&smd_pkt_devp->pa_spinlock, flags);
	__pm_stay_awake(&smd_pkt_devp->pa_ws);
	smd_pkt_devp->ws_locked = 1;
	spin_unlock_irqrestore(&smd_pkt_devp->pa_spinlock, flags);
	wake_up(&smd_pkt_devp->ch_read_wait_queue);
	schedule_work(&smd_pkt_devp->packet_arrival_work);
	D_READ("%s: wake_up smd_pkt_dev id:%d\n", __func__, smd_pkt_devp->i);
}

static void check_and_wakeup_writer(struct smd_pkt_dev *smd_pkt_devp)
{
	int sz;

	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return;
	}

	if (!smd_pkt_devp->ch) {
		pr_err("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return;
	}

	sz = smd_write_segment_avail(smd_pkt_devp->ch);
	if (sz) {
		D_WRITE("%s: %d bytes write space in smd_pkt_dev id:%d\n",
			__func__, sz, smd_pkt_devp->i);
		smd_disable_read_intr(smd_pkt_devp->ch);
		wake_up(&smd_pkt_devp->ch_write_wait_queue);
	}
}

static void ch_notify(void *priv, unsigned event)
{
	struct smd_pkt_dev *smd_pkt_devp = priv;

	if (smd_pkt_devp->ch == 0) {
		if (event != SMD_EVENT_CLOSE)
			pr_err("%s on a closed smd_pkt_dev id:%d\n",
					__func__, smd_pkt_devp->i);
		return;
	}

	switch (event) {
	case SMD_EVENT_DATA: {
		D_STATUS("%s: DATA event in smd_pkt_dev id:%d\n",
			 __func__, smd_pkt_devp->i);
		check_and_wakeup_reader(smd_pkt_devp);
		if (smd_pkt_devp->blocking_write)
			check_and_wakeup_writer(smd_pkt_devp);
		break;
	}
	case SMD_EVENT_OPEN:
		D_STATUS("%s: OPEN event in smd_pkt_dev id:%d\n",
			  __func__, smd_pkt_devp->i);
		smd_pkt_devp->has_reset = 0;
		smd_pkt_devp->is_open = 1;
		wake_up_interruptible(&smd_pkt_devp->ch_opened_wait_queue);
		break;
	case SMD_EVENT_CLOSE:
		D_STATUS("%s: CLOSE event in smd_pkt_dev id:%d\n",
			  __func__, smd_pkt_devp->i);
		smd_pkt_devp->is_open = 0;
		/* put port into reset state */
		clean_and_signal(smd_pkt_devp);
		if (!strcmp(smd_pkt_devp->ch_name, "LOOPBACK"))
			schedule_delayed_work(&loopback_work,
					msecs_to_jiffies(1000));
		break;
	}
}

static int smd_pkt_dummy_probe(struct platform_device *pdev)
{
	struct smd_pkt_dev *smd_pkt_devp;

	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_for_each_entry(smd_pkt_devp, &smd_pkt_dev_list, dev_list) {
		if (smd_pkt_devp->edge == pdev->id
		    && !strcmp(pdev->name, smd_pkt_devp->ch_name)) {
			complete_all(&smd_pkt_devp->ch_allocated);
			D_STATUS("%s allocated SMD ch for smd_pkt_dev id:%d\n",
				 __func__, smd_pkt_devp->i);
			break;
		}
	}
	mutex_unlock(&smd_pkt_dev_lock_lha1);
	return 0;
}

static uint32_t is_modem_smsm_inited(void)
{
	uint32_t modem_state;
	uint32_t ready_state = (SMSM_INIT | SMSM_SMDINIT);

	modem_state = smsm_get_state(SMSM_MODEM_STATE);
	return (modem_state & ready_state) == ready_state;
}

/**
 * smd_pkt_add_driver() - Add platform drivers for smd pkt device
 *
 * @smd_pkt_devp: pointer to the smd pkt device structure
 *
 * @returns:	0 for success, standard Linux error code otherwise
 *
 * This function is used to register platform driver once for all
 * smd pkt devices which have same names and increment the reference
 * count for 2nd to nth devices.
 */
static int smd_pkt_add_driver(struct smd_pkt_dev *smd_pkt_devp)
{
	int r = 0;
	struct smd_pkt_driver *smd_pkt_driverp;
	struct smd_pkt_driver *item;

	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return -EINVAL;
	}
	D_STATUS("Begin %s on smd_pkt_ch[%s]\n", __func__,
					smd_pkt_devp->ch_name);

	mutex_lock(&smd_pkt_driver_lock_lha1);
	list_for_each_entry(item, &smd_pkt_driver_list, list) {
		if (!strcmp(item->pdriver_name, smd_pkt_devp->ch_name)) {
			D_STATUS("%s:%s Already Platform driver reg. cnt:%d\n",
				__func__, smd_pkt_devp->ch_name, item->ref_cnt);
			++item->ref_cnt;
			goto exit;
		}
	}

	smd_pkt_driverp = kzalloc(sizeof(*smd_pkt_driverp), GFP_KERNEL);
	if (IS_ERR_OR_NULL(smd_pkt_driverp)) {
		pr_err("%s: kzalloc() failed for smd_pkt_driver[%s]\n",
			__func__, smd_pkt_devp->ch_name);
		r = -ENOMEM;
		goto exit;
	}

	smd_pkt_driverp->driver.probe = smd_pkt_dummy_probe;
	scnprintf(smd_pkt_driverp->pdriver_name, SMD_MAX_CH_NAME_LEN,
		  "%s", smd_pkt_devp->ch_name);
	smd_pkt_driverp->driver.driver.name = smd_pkt_driverp->pdriver_name;
	smd_pkt_driverp->driver.driver.owner = THIS_MODULE;
	r = platform_driver_register(&smd_pkt_driverp->driver);
	if (r) {
		pr_err("%s: %s Platform driver reg. failed\n",
			__func__, smd_pkt_devp->ch_name);
		kfree(smd_pkt_driverp);
		goto exit;
	}
	++smd_pkt_driverp->ref_cnt;
	list_add(&smd_pkt_driverp->list, &smd_pkt_driver_list);

exit:
	D_STATUS("End %s on smd_pkt_ch[%s]\n", __func__, smd_pkt_devp->ch_name);
	mutex_unlock(&smd_pkt_driver_lock_lha1);
	return r;
}

/**
 * smd_pkt_remove_driver() - Remove the platform drivers for smd pkt device
 *
 * @smd_pkt_devp: pointer to the smd pkt device structure
 *
 * This function is used to decrement the reference count on
 * platform drivers for smd pkt devices and removes the drivers
 * when the reference count becomes zero.
 */
static void smd_pkt_remove_driver(struct smd_pkt_dev *smd_pkt_devp)
{
	struct smd_pkt_driver *smd_pkt_driverp;
	bool found_item = false;

	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return;
	}

	D_STATUS("Begin %s on smd_pkt_ch[%s]\n", __func__,
					smd_pkt_devp->ch_name);
	mutex_lock(&smd_pkt_driver_lock_lha1);
	list_for_each_entry(smd_pkt_driverp, &smd_pkt_driver_list, list) {
		if (!strcmp(smd_pkt_driverp->pdriver_name,
					smd_pkt_devp->ch_name)) {
			found_item = true;
			D_STATUS("%s:%s Platform driver cnt:%d\n",
				__func__, smd_pkt_devp->ch_name,
				smd_pkt_driverp->ref_cnt);
			if (smd_pkt_driverp->ref_cnt > 0)
				--smd_pkt_driverp->ref_cnt;
			else
				pr_warn("%s reference count <= 0\n", __func__);
			break;
		}
	}
	if (!found_item)
		pr_err("%s:%s No item found in list.\n",
				__func__, smd_pkt_devp->ch_name);

	if (found_item && smd_pkt_driverp->ref_cnt == 0) {
		platform_driver_unregister(&smd_pkt_driverp->driver);
		smd_pkt_driverp->driver.probe = NULL;
		list_del(&smd_pkt_driverp->list);
		kfree(smd_pkt_driverp);
	}
	mutex_unlock(&smd_pkt_driver_lock_lha1);
	D_STATUS("End %s on smd_pkt_ch[%s]\n", __func__, smd_pkt_devp->ch_name);
}

int smd_pkt_open(struct inode *inode, struct file *file)
{
	int r = 0;
	struct smd_pkt_dev *smd_pkt_devp;
	const char *peripheral = NULL;

	smd_pkt_devp = container_of(inode->i_cdev, struct smd_pkt_dev, cdev);

	if (!smd_pkt_devp) {
		pr_err_ratelimited("%s on a NULL device\n", __func__);
		return -EINVAL;
	}
	D_STATUS("Begin %s on smd_pkt_dev id:%d\n", __func__, smd_pkt_devp->i);

	file->private_data = smd_pkt_devp;

	mutex_lock(&smd_pkt_devp->ch_lock);
	if (smd_pkt_devp->ch == 0) {
		unsigned open_wait_rem = smd_pkt_devp->open_modem_wait * 1000;

		reinit_completion(&smd_pkt_devp->ch_allocated);

		r = smd_pkt_add_driver(smd_pkt_devp);
		if (r) {
			pr_err_ratelimited("%s: %s Platform driver reg. failed\n",
				__func__, smd_pkt_devp->ch_name);
			goto out;
		}

		peripheral = smd_edge_to_pil_str(smd_pkt_devp->edge);
		if (!IS_ERR_OR_NULL(peripheral)) {
			smd_pkt_devp->pil = subsystem_get(peripheral);
			if (IS_ERR(smd_pkt_devp->pil)) {
				r = PTR_ERR(smd_pkt_devp->pil);
				pr_err_ratelimited("%s failed on smd_pkt_dev id:%d - subsystem_get failed for %s\n",
					__func__, smd_pkt_devp->i, peripheral);
				/*
				 * Sleep inorder to reduce the frequency of
				 * retry by user-space modules and to avoid
				 * possible watchdog bite.
				 */
				msleep(open_wait_rem);
				goto release_pd;
			}
		}

		/* Wait for the modem SMSM to be inited for the SMD
		** Loopback channel to be allocated at the modem. Since
		** the wait need to be done atmost once, using msleep
		** doesn't degrade the performance. */
		if (!strcmp(smd_pkt_devp->ch_name, "LOOPBACK")) {
			if (!is_modem_smsm_inited())
				msleep(5000);
			smsm_change_state(SMSM_APPS_STATE,
					  0, SMSM_SMD_LOOPBACK);
			msleep(100);
		}

		/*
		 * Wait for a packet channel to be allocated so we know
		 * the modem is ready enough.
		 */
		if (open_wait_rem) {
			r = wait_for_completion_interruptible_timeout(
				&smd_pkt_devp->ch_allocated,
				msecs_to_jiffies(open_wait_rem));
			if (r >= 0)
				open_wait_rem = jiffies_to_msecs(r);
			if (r == 0)
				r = -ETIMEDOUT;
			if (r == -ERESTARTSYS) {
				pr_info_ratelimited("%s: wait on smd_pkt_dev id:%d allocation interrupted\n",
					__func__, smd_pkt_devp->i);
				goto release_pil;
			}
			if (r < 0) {
				pr_err_ratelimited("%s: wait on smd_pkt_dev id:%d allocation failed rc:%d\n",
					__func__, smd_pkt_devp->i, r);
				goto release_pil;
			}
		}

		r = smd_named_open_on_edge(smd_pkt_devp->ch_name,
					   smd_pkt_devp->edge,
					   &smd_pkt_devp->ch,
					   smd_pkt_devp,
					   ch_notify);
		if (r < 0) {
			pr_err_ratelimited("%s: %s open failed %d\n", __func__,
			       smd_pkt_devp->ch_name, r);
			goto release_pil;
		}

		open_wait_rem = max_t(unsigned, 2000, open_wait_rem);
		r = wait_event_interruptible_timeout(
				smd_pkt_devp->ch_opened_wait_queue,
				smd_pkt_devp->is_open,
				msecs_to_jiffies(open_wait_rem));
		if (r == 0)
			r = -ETIMEDOUT;

		if (r < 0) {
			/* close the ch to sync smd's state with smd_pkt */
			smd_close(smd_pkt_devp->ch);
			smd_pkt_devp->ch = NULL;
		}

		if (r == -ERESTARTSYS) {
			pr_info_ratelimited("%s: wait on smd_pkt_dev id:%d OPEN interrupted\n",
				__func__, smd_pkt_devp->i);
		} else if (r < 0) {
			pr_err_ratelimited("%s: wait on smd_pkt_dev id:%d OPEN event failed rc:%d\n",
				__func__, smd_pkt_devp->i, r);
		} else if (!smd_pkt_devp->is_open) {
			pr_err_ratelimited("%s: Invalid OPEN event on smd_pkt_dev id:%d\n",
				__func__, smd_pkt_devp->i);
			r = -ENODEV;
		} else {
			smd_disable_read_intr(smd_pkt_devp->ch);
			smd_pkt_devp->ch_size =
				smd_write_avail(smd_pkt_devp->ch);
			r = 0;
			smd_pkt_devp->ref_cnt++;
			D_STATUS("Finished %s on smd_pkt_dev id:%d\n",
				 __func__, smd_pkt_devp->i);
		}
	} else {
		smd_pkt_devp->ref_cnt++;
	}
release_pil:
	if (peripheral && (r < 0)) {
		subsystem_put(smd_pkt_devp->pil);
		smd_pkt_devp->pil = NULL;
	}

release_pd:
	if (r < 0)
		smd_pkt_remove_driver(smd_pkt_devp);
out:
	mutex_unlock(&smd_pkt_devp->ch_lock);


	return r;
}

int smd_pkt_release(struct inode *inode, struct file *file)
{
	int r = 0;
	struct smd_pkt_dev *smd_pkt_devp = file->private_data;
	unsigned long flags;

	if (!smd_pkt_devp) {
		pr_err_ratelimited("%s on a NULL device\n", __func__);
		return -EINVAL;
	}
	D_STATUS("Begin %s on smd_pkt_dev id:%d\n",
		 __func__, smd_pkt_devp->i);

	mutex_lock(&smd_pkt_devp->ch_lock);
	mutex_lock(&smd_pkt_devp->rx_lock);
	mutex_lock(&smd_pkt_devp->tx_lock);
	if (smd_pkt_devp->ref_cnt > 0)
		smd_pkt_devp->ref_cnt--;

	if (smd_pkt_devp->ch != 0 && smd_pkt_devp->ref_cnt == 0) {
		clean_and_signal(smd_pkt_devp);
		r = smd_close(smd_pkt_devp->ch);
		smd_pkt_devp->ch = 0;
		smd_pkt_devp->blocking_write = 0;
		smd_pkt_devp->poll_mode = 0;
		smd_pkt_remove_driver(smd_pkt_devp);
		if (smd_pkt_devp->pil)
			subsystem_put(smd_pkt_devp->pil);
		smd_pkt_devp->has_reset = 0;
		smd_pkt_devp->do_reset_notification = 0;
		spin_lock_irqsave(&smd_pkt_devp->pa_spinlock, flags);
		if (smd_pkt_devp->ws_locked) {
			__pm_relax(&smd_pkt_devp->pa_ws);
			smd_pkt_devp->ws_locked = 0;
		}
		spin_unlock_irqrestore(&smd_pkt_devp->pa_spinlock, flags);
	}
	mutex_unlock(&smd_pkt_devp->tx_lock);
	mutex_unlock(&smd_pkt_devp->rx_lock);
	mutex_unlock(&smd_pkt_devp->ch_lock);

	if (flush_work(&smd_pkt_devp->packet_arrival_work))
		D_STATUS("%s: Flushed work for smd_pkt_dev id:%d\n", __func__,
				smd_pkt_devp->i);

	D_STATUS("Finished %s on smd_pkt_dev id:%d\n",
		 __func__, smd_pkt_devp->i);

	return r;
}

static const struct file_operations smd_pkt_fops = {
	.owner = THIS_MODULE,
	.open = smd_pkt_open,
	.release = smd_pkt_release,
	.read = smd_pkt_read,
	.write = smd_pkt_write,
	.poll = smd_pkt_poll,
	.unlocked_ioctl = smd_pkt_ioctl,
	.compat_ioctl = smd_pkt_ioctl,
};

static int smd_pkt_init_add_device(struct smd_pkt_dev *smd_pkt_devp, int i)
{
	int r = 0;

	smd_pkt_devp->i = i;

	init_waitqueue_head(&smd_pkt_devp->ch_read_wait_queue);
	init_waitqueue_head(&smd_pkt_devp->ch_write_wait_queue);
	smd_pkt_devp->is_open = 0;
	smd_pkt_devp->poll_mode = 0;
	smd_pkt_devp->ws_locked = 0;
	init_waitqueue_head(&smd_pkt_devp->ch_opened_wait_queue);

	spin_lock_init(&smd_pkt_devp->pa_spinlock);
	mutex_init(&smd_pkt_devp->ch_lock);
	mutex_init(&smd_pkt_devp->rx_lock);
	mutex_init(&smd_pkt_devp->tx_lock);
	wakeup_source_init(&smd_pkt_devp->pa_ws, smd_pkt_devp->dev_name);
	INIT_WORK(&smd_pkt_devp->packet_arrival_work, packet_arrival_worker);
	init_completion(&smd_pkt_devp->ch_allocated);

	cdev_init(&smd_pkt_devp->cdev, &smd_pkt_fops);
	smd_pkt_devp->cdev.owner = THIS_MODULE;

	r = cdev_add(&smd_pkt_devp->cdev, (smd_pkt_number + i), 1);
	if (IS_ERR_VALUE(r)) {
		pr_err("%s: cdev_add() failed for smd_pkt_dev id:%d ret:%i\n",
			__func__, i, r);
		return r;
	}

	smd_pkt_devp->devicep =
		device_create(smd_pkt_classp,
			      NULL,
			      (smd_pkt_number + i),
			      NULL,
			      smd_pkt_devp->dev_name);

	if (IS_ERR_OR_NULL(smd_pkt_devp->devicep)) {
		pr_err("%s: device_create() failed for smd_pkt_dev id:%d\n",
			__func__, i);
		r = -ENOMEM;
		cdev_del(&smd_pkt_devp->cdev);
		wakeup_source_trash(&smd_pkt_devp->pa_ws);
		return r;
	}
	if (device_create_file(smd_pkt_devp->devicep,
				&dev_attr_open_timeout))
		pr_err("%s: unable to create device attr for smd_pkt_dev id:%d\n",
			__func__, i);

	if (!strcmp(smd_pkt_devp->ch_name, "LOOPBACK")) {
		if (device_create_file(smd_pkt_devp->devicep,
					&dev_attr_loopback_edge))
			pr_err("%s: unable to create device attr for smd_pkt_dev id:%d\n",
				__func__, i);
	}
	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_add(&smd_pkt_devp->dev_list, &smd_pkt_dev_list);
	mutex_unlock(&smd_pkt_dev_lock_lha1);

	return r;
}

static void smd_pkt_core_deinit(void)
{
	struct smd_pkt_dev *smd_pkt_devp;
	struct smd_pkt_dev *index;

	mutex_lock(&smd_pkt_dev_lock_lha1);
	list_for_each_entry_safe(smd_pkt_devp, index, &smd_pkt_dev_list,
							dev_list) {
		cdev_del(&smd_pkt_devp->cdev);
		list_del(&smd_pkt_devp->dev_list);
		device_destroy(smd_pkt_classp,
			       MKDEV(MAJOR(smd_pkt_number), smd_pkt_devp->i));
		kfree(smd_pkt_devp);
	}
	mutex_unlock(&smd_pkt_dev_lock_lha1);

	if (!IS_ERR_OR_NULL(smd_pkt_classp))
		class_destroy(smd_pkt_classp);

	unregister_chrdev_region(MAJOR(smd_pkt_number), num_smd_pkt_ports);
}

static int smd_pkt_alloc_chrdev_region(void)
{
	int r = alloc_chrdev_region(&smd_pkt_number,
			       0,
			       num_smd_pkt_ports,
			       DEVICE_NAME);

	if (IS_ERR_VALUE(r)) {
		pr_err("%s: alloc_chrdev_region() failed ret:%i\n",
			__func__, r);
		return r;
	}

	smd_pkt_classp = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(smd_pkt_classp)) {
		pr_err("%s: class_create() failed ENOMEM\n", __func__);
		r = -ENOMEM;
		unregister_chrdev_region(MAJOR(smd_pkt_number),
						num_smd_pkt_ports);
		return r;
	}

	return 0;
}

static int parse_smdpkt_devicetree(struct device_node *node,
					struct smd_pkt_dev *smd_pkt_devp)
{
	int edge;
	char *key;
	const char *ch_name;
	const char *dev_name;
	const char *remote_ss;

	key = "qcom,smdpkt-remote";
	remote_ss = of_get_property(node, key, NULL);
	if (!remote_ss)
		goto error;

	edge = smd_remote_ss_to_edge(remote_ss);
	if (edge < 0)
		goto error;

	smd_pkt_devp->edge = edge;
	D_STATUS("%s: %s = %d", __func__, key, edge);

	key = "qcom,smdpkt-port-name";
	ch_name = of_get_property(node, key, NULL);
	if (!ch_name)
		goto error;

	strlcpy(smd_pkt_devp->ch_name, ch_name, SMD_MAX_CH_NAME_LEN);
	D_STATUS("%s ch_name = %s\n", __func__, ch_name);

	key = "qcom,smdpkt-dev-name";
	dev_name = of_get_property(node, key, NULL);
	if (!dev_name)
		goto error;

	strlcpy(smd_pkt_devp->dev_name, dev_name, SMD_MAX_CH_NAME_LEN);
	D_STATUS("%s dev_name = %s\n", __func__, dev_name);

	return 0;

error:
	pr_err("%s: missing key: %s\n", __func__, key);
	return -ENODEV;

}

static int smd_pkt_devicetree_init(struct platform_device *pdev)
{
	int ret;
	int i = 0;
	struct device_node *node;
	struct smd_pkt_dev *smd_pkt_devp;
	int subnode_num = 0;

	for_each_child_of_node(pdev->dev.of_node, node)
		++subnode_num;

	num_smd_pkt_ports = subnode_num;

	ret = smd_pkt_alloc_chrdev_region();
	if (ret) {
		pr_err("%s: smd_pkt_alloc_chrdev_region() failed ret:%i\n",
			__func__, ret);
		return ret;
	}

	for_each_child_of_node(pdev->dev.of_node, node) {
		smd_pkt_devp = kzalloc(sizeof(struct smd_pkt_dev), GFP_KERNEL);
		if (IS_ERR_OR_NULL(smd_pkt_devp)) {
			pr_err("%s: kzalloc() failed for smd_pkt_dev id:%d\n",
				__func__, i);
			ret = -ENOMEM;
			goto error_destroy;
		}

		ret = parse_smdpkt_devicetree(node, smd_pkt_devp);
		if (ret) {
			pr_err(" failed to parse_smdpkt_devicetree %d\n", i);
			kfree(smd_pkt_devp);
			goto error_destroy;
		}

		ret = smd_pkt_init_add_device(smd_pkt_devp, i);
		if (ret < 0) {
			pr_err("add device failed for idx:%d ret=%d\n", i, ret);
			kfree(smd_pkt_devp);
			goto error_destroy;
		}
		i++;
	}

	INIT_DELAYED_WORK(&loopback_work, loopback_probe_worker);

	D_STATUS("SMD Packet Port Driver Initialized.\n");
	return 0;

error_destroy:
	smd_pkt_core_deinit();
	return ret;
}

static int msm_smd_pkt_probe(struct platform_device *pdev)
{
	int ret;

	if (pdev) {
		if (pdev->dev.of_node) {
			D_STATUS("%s device tree implementation\n", __func__);
			ret = smd_pkt_devicetree_init(pdev);
			if (ret)
				pr_err("%s: device tree init failed\n",
					__func__);
		}
	}

	return 0;
}

static struct of_device_id msm_smd_pkt_match_table[] = {
	{ .compatible = "qcom,smdpkt" },
	{},
};

static struct platform_driver msm_smd_pkt_driver = {
	.probe = msm_smd_pkt_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_smd_pkt_match_table,
	 },
};

static int __init smd_pkt_init(void)
{
	int rc;

	INIT_LIST_HEAD(&smd_pkt_dev_list);
	INIT_LIST_HEAD(&smd_pkt_driver_list);
	rc = platform_driver_register(&msm_smd_pkt_driver);
	if (rc) {
		pr_err("%s: msm_smd_driver register failed %d\n",
			 __func__, rc);
		return rc;
	}

	smd_pkt_ilctxt = ipc_log_context_create(SMD_PKT_IPC_LOG_PAGE_CNT,
						"smd_pkt", 0);
	return 0;
}

static void __exit smd_pkt_cleanup(void)
{
	smd_pkt_core_deinit();
}

module_init(smd_pkt_init);
module_exit(smd_pkt_cleanup);

MODULE_DESCRIPTION("MSM Shared Memory Packet Port");
MODULE_LICENSE("GPL v2");

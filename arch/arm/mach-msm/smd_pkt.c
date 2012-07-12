/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#include <asm/ioctls.h>
#include <linux/wakelock.h>

#include <mach/msm_smd.h>
#include <mach/peripheral-loader.h>

#include "smd_private.h"
#ifdef CONFIG_ARCH_FSM9XXX
#define NUM_SMD_PKT_PORTS 4
#else
#define NUM_SMD_PKT_PORTS 15
#endif

#define PDRIVER_NAME_MAX_SIZE 32
#define LOOPBACK_INX (NUM_SMD_PKT_PORTS - 1)

#define DEVICE_NAME "smdpkt"
#define WAKELOCK_TIMEOUT (2*HZ)

struct smd_pkt_dev {
	struct cdev cdev;
	struct device *devicep;
	void *pil;
	char pdriver_name[PDRIVER_NAME_MAX_SIZE];
	struct platform_driver driver;

	struct smd_channel *ch;
	struct mutex ch_lock;
	struct mutex rx_lock;
	struct mutex tx_lock;
	wait_queue_head_t ch_read_wait_queue;
	wait_queue_head_t ch_write_wait_queue;
	wait_queue_head_t ch_opened_wait_queue;

	int i;

	int blocking_write;
	int is_open;
	int poll_mode;
	unsigned ch_size;
	uint open_modem_wait;

	int has_reset;
	int do_reset_notification;
	struct completion ch_allocated;
	struct wake_lock pa_wake_lock;		/* Packet Arrival Wake lock*/
	struct work_struct packet_arrival_work;
	struct spinlock pa_spinlock;
	int wakelock_locked;
} *smd_pkt_devp[NUM_SMD_PKT_PORTS];

struct class *smd_pkt_classp;
static dev_t smd_pkt_number;
static struct delayed_work loopback_work;
static void check_and_wakeup_reader(struct smd_pkt_dev *smd_pkt_devp);
static void check_and_wakeup_writer(struct smd_pkt_dev *smd_pkt_devp);
static uint32_t is_modem_smsm_inited(void);

static int msm_smd_pkt_debug_mask;
module_param_named(debug_mask, msm_smd_pkt_debug_mask,
		int, S_IRUGO | S_IWUSR | S_IWGRP);

enum {
	SMD_PKT_STATUS = 1U << 0,
	SMD_PKT_READ = 1U << 1,
	SMD_PKT_WRITE = 1U << 2,
	SMD_PKT_READ_DUMP_BUFFER = 1U << 3,
	SMD_PKT_WRITE_DUMP_BUFFER = 1U << 4,
	SMD_PKT_POLL = 1U << 5,
};

#define DEBUG

#ifdef DEBUG
#define D_STATUS(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_STATUS) \
		pr_info("Status: "x); \
} while (0)

#define D_READ(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_READ) \
		pr_info("Read: "x); \
} while (0)

#define D_WRITE(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_WRITE) \
		pr_info("Write: "x); \
} while (0)

#define D_READ_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_READ_DUMP_BUFFER) \
		print_hex_dump(KERN_INFO, prestr, \
			       DUMP_PREFIX_NONE, 16, 1, \
			       buf, cnt, 1); \
} while (0)

#define D_WRITE_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_WRITE_DUMP_BUFFER) \
		print_hex_dump(KERN_INFO, prestr, \
			       DUMP_PREFIX_NONE, 16, 1, \
			       buf, cnt, 1); \
} while (0)

#define D_POLL(x...) \
do { \
	if (msm_smd_pkt_debug_mask & SMD_PKT_POLL) \
		pr_info("Poll: "x); \
} while (0)
#else
#define D_STATUS(x...) do {} while (0)
#define D_READ(x...) do {} while (0)
#define D_WRITE(x...) do {} while (0)
#define D_READ_DUMP_BUFFER(prestr, cnt, buf) do {} while (0)
#define D_WRITE_DUMP_BUFFER(prestr, cnt, buf) do {} while (0)
#define D_POLL(x...) do {} while (0)
#endif

static ssize_t open_timeout_store(struct device *d,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t n)
{
	int i;
	unsigned long tmp;
	for (i = 0; i < NUM_SMD_PKT_PORTS; ++i) {
		if (smd_pkt_devp[i]->devicep == d)
			break;
	}
	if (i >= NUM_SMD_PKT_PORTS) {
		pr_err("%s: unable to match device to valid smd_pkt port\n",
			__func__);
		return -EINVAL;
	}
	if (!strict_strtoul(buf, 10, &tmp)) {
		smd_pkt_devp[i]->open_modem_wait = tmp;
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
	for (i = 0; i < NUM_SMD_PKT_PORTS; ++i) {
		if (smd_pkt_devp[i]->devicep == d)
			break;
	}
	if (i >= NUM_SMD_PKT_PORTS) {
		pr_err("%s: unable to match device to valid smd_pkt port\n",
			__func__);
		return -EINVAL;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",
			smd_pkt_devp[i]->open_modem_wait);
}

static DEVICE_ATTR(open_timeout, 0664, open_timeout_show, open_timeout_store);

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
	if (smd_pkt_devp->ch && smd_pkt_devp->wakelock_locked) {
		D_READ("%s locking smd_pkt_dev id:%d wakelock\n",
			__func__, smd_pkt_devp->i);
		wake_lock_timeout(&smd_pkt_devp->pa_wake_lock,
				  WAKELOCK_TIMEOUT);
	}
	spin_unlock_irqrestore(&smd_pkt_devp->pa_spinlock, flags);
	mutex_unlock(&smd_pkt_devp->ch_lock);
}

static long smd_pkt_ioctl(struct file *file, unsigned int cmd,
					     unsigned long arg)
{
	int ret;
	struct smd_pkt_dev *smd_pkt_devp;

	smd_pkt_devp = file->private_data;
	if (!smd_pkt_devp)
		return -EINVAL;

	switch (cmd) {
	case TIOCMGET:
		D_STATUS("%s TIOCMGET command on smd_pkt_dev id:%d\n",
			 __func__, smd_pkt_devp->i);
		ret = smd_tiocmget(smd_pkt_devp->ch);
		break;
	case TIOCMSET:
		D_STATUS("%s TIOCSET command on smd_pkt_dev id:%d\n",
			 __func__, smd_pkt_devp->i);
		ret = smd_tiocmset(smd_pkt_devp->ch, arg, ~arg);
		break;
	case SMD_PKT_IOCTL_BLOCKING_WRITE:
		ret = get_user(smd_pkt_devp->blocking_write, (int *)arg);
		break;
	default:
		pr_err("%s: Unrecognized ioctl command %d\n", __func__, cmd);
		ret = -1;
	}

	return ret;
}

ssize_t smd_pkt_read(struct file *file,
		       char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int r;
	int bytes_read;
	int pkt_size;
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned long flags;

	smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp) {
		pr_err("%s on NULL smd_pkt_dev\n", __func__);
		return -EINVAL;
	}

	if (!smd_pkt_devp->ch) {
		pr_err("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return -EINVAL;
	}

	if (smd_pkt_devp->do_reset_notification) {
		/* notify client that a reset occurred */
		pr_err("%s notifying reset for smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return notify_reset(smd_pkt_devp);
	}
	D_READ("Begin %s on smd_pkt_dev id:%d buffer_size %d\n",
		__func__, smd_pkt_devp->i, count);

wait_for_packet:
	r = wait_event_interruptible(smd_pkt_devp->ch_read_wait_queue,
				     !smd_pkt_devp->ch ||
				     (smd_cur_packet_size(smd_pkt_devp->ch) > 0
				      && smd_read_avail(smd_pkt_devp->ch)) ||
				     smd_pkt_devp->has_reset);

	mutex_lock(&smd_pkt_devp->rx_lock);
	if (smd_pkt_devp->has_reset) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		pr_err("%s notifying reset for smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return notify_reset(smd_pkt_devp);
	}

	if (!smd_pkt_devp->ch) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		pr_err("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return -EINVAL;
	}

	if (r < 0) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		/* qualify error message */
		if (r != -ERESTARTSYS) {
			/* we get this anytime a signal comes in */
			pr_err("%s: wait_event_interruptible on smd_pkt_dev"
			       " id:%d ret %i\n",
				__func__, smd_pkt_devp->i, r);
		}
		return r;
	}

	/* Here we have a whole packet waiting for us */
	pkt_size = smd_cur_packet_size(smd_pkt_devp->ch);

	if (!pkt_size) {
		pr_err("%s: No data on smd_pkt_dev id:%d, False wakeup\n",
			__func__, smd_pkt_devp->i);
		mutex_unlock(&smd_pkt_devp->rx_lock);
		goto wait_for_packet;
	}

	if (pkt_size > count) {
		pr_err("%s: failure on smd_pkt_dev id: %d - packet size %d"
		       " > buffer size %d,", __func__, smd_pkt_devp->i,
			pkt_size, count);
		mutex_unlock(&smd_pkt_devp->rx_lock);
		return -ETOOSMALL;
	}

	bytes_read = 0;
	do {
		r = smd_read_user_buffer(smd_pkt_devp->ch,
					 (buf + bytes_read),
					 (pkt_size - bytes_read));
		if (r < 0) {
			mutex_unlock(&smd_pkt_devp->rx_lock);
			if (smd_pkt_devp->has_reset) {
				pr_err("%s notifying reset for smd_pkt_dev"
				       " id:%d\n", __func__, smd_pkt_devp->i);
				return notify_reset(smd_pkt_devp);
			}
			pr_err("%s Error while reading %d\n", __func__, r);
			return r;
		}
		bytes_read += r;
		if (pkt_size != bytes_read)
			wait_event(smd_pkt_devp->ch_read_wait_queue,
				   smd_read_avail(smd_pkt_devp->ch) ||
				   smd_pkt_devp->has_reset);
		if (smd_pkt_devp->has_reset) {
			mutex_unlock(&smd_pkt_devp->rx_lock);
			pr_err("%s notifying reset for smd_pkt_dev  id:%d\n",
				__func__, smd_pkt_devp->i);
			return notify_reset(smd_pkt_devp);
		}
	} while (pkt_size != bytes_read);
	D_READ_DUMP_BUFFER("Read: ", (bytes_read > 16 ? 16 : bytes_read), buf);
	mutex_unlock(&smd_pkt_devp->rx_lock);

	mutex_lock(&smd_pkt_devp->ch_lock);
	spin_lock_irqsave(&smd_pkt_devp->pa_spinlock, flags);
	if (smd_pkt_devp->poll_mode &&
	    !smd_cur_packet_size(smd_pkt_devp->ch)) {
		wake_unlock(&smd_pkt_devp->pa_wake_lock);
		smd_pkt_devp->wakelock_locked = 0;
		smd_pkt_devp->poll_mode = 0;
		D_READ("%s unlocked smd_pkt_dev id:%d wakelock\n",
			__func__, smd_pkt_devp->i);
	}
	spin_unlock_irqrestore(&smd_pkt_devp->pa_spinlock, flags);
	mutex_unlock(&smd_pkt_devp->ch_lock);

	D_READ("Finished %s on smd_pkt_dev id:%d  %d bytes\n",
		__func__, smd_pkt_devp->i, bytes_read);

	/* check and wakeup read threads waiting on this device */
	check_and_wakeup_reader(smd_pkt_devp);

	return bytes_read;
}

ssize_t smd_pkt_write(struct file *file,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int r = 0, bytes_written;
	struct smd_pkt_dev *smd_pkt_devp;
	DEFINE_WAIT(write_wait);

	smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp) {
		pr_err("%s on NULL smd_pkt_dev\n", __func__);
		return -EINVAL;
	}

	if (!smd_pkt_devp->ch) {
		pr_err("%s on a closed smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		return -EINVAL;
	}

	if (smd_pkt_devp->do_reset_notification || smd_pkt_devp->has_reset) {
		pr_err("%s notifying reset for smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
		/* notify client that a reset occurred */
		return notify_reset(smd_pkt_devp);
	}
	D_WRITE("Begin %s on smd_pkt_dev id:%d data_size %d\n",
		__func__, smd_pkt_devp->i, count);

	mutex_lock(&smd_pkt_devp->tx_lock);
	if (!smd_pkt_devp->blocking_write) {
		if (smd_write_avail(smd_pkt_devp->ch) < count) {
			pr_err("%s: Not enough space in smd_pkt_dev id:%d\n",
				   __func__, smd_pkt_devp->i);
			mutex_unlock(&smd_pkt_devp->tx_lock);
			return -ENOMEM;
		}
	}

	r = smd_write_start(smd_pkt_devp->ch, count);
	if (r < 0) {
		mutex_unlock(&smd_pkt_devp->tx_lock);
		pr_err("%s: Error:%d in smd_pkt_dev id:%d @ smd_write_start\n",
			__func__, r, smd_pkt_devp->i);
		return r;
	}

	bytes_written = 0;
	do {
		prepare_to_wait(&smd_pkt_devp->ch_write_wait_queue,
				&write_wait, TASK_UNINTERRUPTIBLE);
		if (!smd_write_avail(smd_pkt_devp->ch) &&
		    !smd_pkt_devp->has_reset) {
			smd_enable_read_intr(smd_pkt_devp->ch);
			schedule();
		}
		finish_wait(&smd_pkt_devp->ch_write_wait_queue, &write_wait);
		smd_disable_read_intr(smd_pkt_devp->ch);

		if (smd_pkt_devp->has_reset) {
			mutex_unlock(&smd_pkt_devp->tx_lock);
			pr_err("%s notifying reset for smd_pkt_dev id:%d\n",
				__func__, smd_pkt_devp->i);
			return notify_reset(smd_pkt_devp);
		} else {
			r = smd_write_segment(smd_pkt_devp->ch,
					      (void *)(buf + bytes_written),
					      (count - bytes_written), 1);
			if (r < 0) {
				mutex_unlock(&smd_pkt_devp->tx_lock);
				if (smd_pkt_devp->has_reset) {
					pr_err("%s notifying reset for"
					       " smd_pkt_dev id:%d\n",
						__func__, smd_pkt_devp->i);
					return notify_reset(smd_pkt_devp);
				}
				pr_err("%s on smd_pkt_dev id:%d failed r:%d\n",
					__func__, smd_pkt_devp->i, r);
				return r;
			}
			bytes_written += r;
		}
	} while (bytes_written != count);
	smd_write_end(smd_pkt_devp->ch);
	mutex_unlock(&smd_pkt_devp->tx_lock);
	D_WRITE_DUMP_BUFFER("Write: ",
			    (bytes_written > 16 ? 16 : bytes_written), buf);
	D_WRITE("Finished %s on smd_pkt_dev id:%d %d bytes\n",
		__func__, smd_pkt_devp->i, count);

	return count;
}

static unsigned int smd_pkt_poll(struct file *file, poll_table *wait)
{
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned int mask = 0;

	smd_pkt_devp = file->private_data;
	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return POLLERR;
	}

	smd_pkt_devp->poll_mode = 1;
	poll_wait(file, &smd_pkt_devp->ch_read_wait_queue, wait);
	mutex_lock(&smd_pkt_devp->ch_lock);
	if (smd_pkt_devp->has_reset || !smd_pkt_devp->ch) {
		mutex_unlock(&smd_pkt_devp->ch_lock);
		pr_err("%s notifying reset for smd_pkt_dev id:%d\n",
			__func__, smd_pkt_devp->i);
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
		D_READ("%s: packet size is %d in smd_pkt_dev id:%d -"
			" but the data isn't here\n",
			__func__, sz, smd_pkt_devp->i);
		return;
	}

	/* here we have a packet of size sz ready */
	spin_lock_irqsave(&smd_pkt_devp->pa_spinlock, flags);
	wake_lock(&smd_pkt_devp->pa_wake_lock);
	smd_pkt_devp->wakelock_locked = 1;
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

	sz = smd_write_avail(smd_pkt_devp->ch);
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
		if (smd_pkt_devp->i == LOOPBACK_INX)
			schedule_delayed_work(&loopback_work,
					msecs_to_jiffies(1000));
		break;
	}
}

#ifdef CONFIG_ARCH_FSM9XXX
static char *smd_pkt_dev_name[] = {
	"smdcntl1",
	"smdcntl2",
	"smd22",
	"smd_pkt_loopback",
};

static char *smd_ch_name[] = {
	"DATA6_CNTL",
	"DATA7_CNTL",
	"DATA22",
	"LOOPBACK",
};

static uint32_t smd_ch_edge[] = {
	SMD_APPS_QDSP,
	SMD_APPS_QDSP,
	SMD_APPS_QDSP,
	SMD_APPS_QDSP
};
#else
static char *smd_pkt_dev_name[] = {
	"smdcntl0",
	"smdcntl1",
	"smdcntl2",
	"smdcntl3",
	"smdcntl4",
	"smdcntl5",
	"smdcntl6",
	"smdcntl7",
	"smd22",
	"smd_sns_dsps",
	"apr_apps2",
	"smdcntl8",
	"smd_sns_adsp",
	"smd_cxm_qmi",
	"smd_pkt_loopback",
};

static char *smd_ch_name[] = {
	"DATA5_CNTL",
	"DATA6_CNTL",
	"DATA7_CNTL",
	"DATA8_CNTL",
	"DATA9_CNTL",
	"DATA12_CNTL",
	"DATA13_CNTL",
	"DATA14_CNTL",
	"DATA22",
	"SENSOR",
	"apr_apps2",
	"DATA40_CNTL",
	"SENSOR",
	"CXM_QMI_PORT_8064",
	"LOOPBACK",
};

static uint32_t smd_ch_edge[] = {
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_MODEM,
	SMD_APPS_DSPS,
	SMD_APPS_QDSP,
	SMD_APPS_MODEM,
	SMD_APPS_QDSP,
	SMD_APPS_WCNSS,
	SMD_APPS_MODEM,
};
#endif

static int smd_pkt_dummy_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_SMD_PKT_PORTS; i++) {
		if (smd_ch_edge[i] == pdev->id
		    && !strncmp(pdev->name, smd_ch_name[i],
				SMD_MAX_CH_NAME_LEN)
		    && smd_pkt_devp[i]->driver.probe) {
			complete_all(&smd_pkt_devp[i]->ch_allocated);
			D_STATUS("%s allocated SMD ch for smd_pkt_dev id:%d\n",
				 __func__, i);
			break;
		}
	}
	return 0;
}

static uint32_t is_modem_smsm_inited(void)
{
	uint32_t modem_state;
	uint32_t ready_state = (SMSM_INIT | SMSM_SMDINIT);

	modem_state = smsm_get_state(SMSM_MODEM_STATE);
	return (modem_state & ready_state) == ready_state;
}

int smd_pkt_open(struct inode *inode, struct file *file)
{
	int r = 0;
	struct smd_pkt_dev *smd_pkt_devp;
	const char *peripheral = NULL;

	smd_pkt_devp = container_of(inode->i_cdev, struct smd_pkt_dev, cdev);

	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return -EINVAL;
	}
	D_STATUS("Begin %s on smd_pkt_dev id:%d\n", __func__, smd_pkt_devp->i);

	file->private_data = smd_pkt_devp;

	mutex_lock(&smd_pkt_devp->ch_lock);
	if (smd_pkt_devp->ch == 0) {
		wake_lock_init(&smd_pkt_devp->pa_wake_lock, WAKE_LOCK_SUSPEND,
				smd_pkt_dev_name[smd_pkt_devp->i]);
		INIT_WORK(&smd_pkt_devp->packet_arrival_work,
				packet_arrival_worker);
		init_completion(&smd_pkt_devp->ch_allocated);
		smd_pkt_devp->driver.probe = smd_pkt_dummy_probe;
		scnprintf(smd_pkt_devp->pdriver_name, PDRIVER_NAME_MAX_SIZE,
			  "%s", smd_ch_name[smd_pkt_devp->i]);
		smd_pkt_devp->driver.driver.name = smd_pkt_devp->pdriver_name;
		smd_pkt_devp->driver.driver.owner = THIS_MODULE;
		r = platform_driver_register(&smd_pkt_devp->driver);
		if (r) {
			pr_err("%s: %s Platform driver reg. failed\n",
				__func__, smd_ch_name[smd_pkt_devp->i]);
			goto out;
		}

		peripheral = smd_edge_to_subsystem(
				smd_ch_edge[smd_pkt_devp->i]);
		if (peripheral) {
			smd_pkt_devp->pil = pil_get(peripheral);
			if (IS_ERR(smd_pkt_devp->pil)) {
				r = PTR_ERR(smd_pkt_devp->pil);
				pr_err("%s failed on smd_pkt_dev id:%d -"
				       " pil_get failed for %s\n", __func__,
					smd_pkt_devp->i, peripheral);
				goto release_pd;
			}

			/* Wait for the modem SMSM to be inited for the SMD
			** Loopback channel to be allocated at the modem. Since
			** the wait need to be done atmost once, using msleep
			** doesn't degrade the performance. */
			if (!strncmp(smd_ch_name[smd_pkt_devp->i], "LOOPBACK",
						SMD_MAX_CH_NAME_LEN)) {
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
			if (smd_pkt_devp->open_modem_wait) {
				r = wait_for_completion_interruptible_timeout(
					&smd_pkt_devp->ch_allocated,
					msecs_to_jiffies(
						smd_pkt_devp->open_modem_wait
							 * 1000));
				if (r == 0)
					r = -ETIMEDOUT;
				if (r < 0) {
					pr_err("%s: wait on smd_pkt_dev id:%d"
					       " allocation failed rc:%d\n",
						__func__, smd_pkt_devp->i, r);
					goto release_pil;
				}
			}
		}

		r = smd_named_open_on_edge(smd_ch_name[smd_pkt_devp->i],
					   smd_ch_edge[smd_pkt_devp->i],
					   &smd_pkt_devp->ch,
					   smd_pkt_devp,
					   ch_notify);
		if (r < 0) {
			pr_err("%s: %s open failed %d\n", __func__,
			       smd_ch_name[smd_pkt_devp->i], r);
			goto release_pil;
		}

		r = wait_event_interruptible_timeout(
				smd_pkt_devp->ch_opened_wait_queue,
				smd_pkt_devp->is_open, (2 * HZ));
		if (r == 0) {
			r = -ETIMEDOUT;
			/* close the ch to sync smd's state with smd_pkt */
			smd_close(smd_pkt_devp->ch);
			smd_pkt_devp->ch = NULL;
		}

		if (r < 0) {
			pr_err("%s: wait on smd_pkt_dev id:%d OPEN event failed"
			       " rc:%d\n", __func__, smd_pkt_devp->i, r);
		} else if (!smd_pkt_devp->is_open) {
			pr_err("%s: Invalid OPEN event on smd_pkt_dev id:%d\n",
				__func__, smd_pkt_devp->i);
			r = -ENODEV;
		} else {
			smd_disable_read_intr(smd_pkt_devp->ch);
			smd_pkt_devp->ch_size =
				smd_write_avail(smd_pkt_devp->ch);
			r = 0;
			D_STATUS("Finished %s on smd_pkt_dev id:%d\n",
				 __func__, smd_pkt_devp->i);
		}
	}
release_pil:
	if (peripheral && (r < 0))
		pil_put(smd_pkt_devp->pil);

release_pd:
	if (r < 0) {
		platform_driver_unregister(&smd_pkt_devp->driver);
		smd_pkt_devp->driver.probe = NULL;
	}
out:
	if (!smd_pkt_devp->ch)
		wake_lock_destroy(&smd_pkt_devp->pa_wake_lock);

	mutex_unlock(&smd_pkt_devp->ch_lock);


	return r;
}

int smd_pkt_release(struct inode *inode, struct file *file)
{
	int r = 0;
	struct smd_pkt_dev *smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp) {
		pr_err("%s on a NULL device\n", __func__);
		return -EINVAL;
	}
	D_STATUS("Begin %s on smd_pkt_dev id:%d\n",
		 __func__, smd_pkt_devp->i);

	clean_and_signal(smd_pkt_devp);

	mutex_lock(&smd_pkt_devp->ch_lock);
	mutex_lock(&smd_pkt_devp->rx_lock);
	mutex_lock(&smd_pkt_devp->tx_lock);
	if (smd_pkt_devp->ch != 0) {
		r = smd_close(smd_pkt_devp->ch);
		smd_pkt_devp->ch = 0;
		smd_pkt_devp->blocking_write = 0;
		smd_pkt_devp->poll_mode = 0;
		platform_driver_unregister(&smd_pkt_devp->driver);
		smd_pkt_devp->driver.probe = NULL;
		if (smd_pkt_devp->pil)
			pil_put(smd_pkt_devp->pil);
	}
	mutex_unlock(&smd_pkt_devp->tx_lock);
	mutex_unlock(&smd_pkt_devp->rx_lock);
	mutex_unlock(&smd_pkt_devp->ch_lock);

	smd_pkt_devp->has_reset = 0;
	smd_pkt_devp->do_reset_notification = 0;
	smd_pkt_devp->wakelock_locked = 0;
	wake_lock_destroy(&smd_pkt_devp->pa_wake_lock);
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
};

static int __init smd_pkt_init(void)
{
	int i;
	int r;

	r = alloc_chrdev_region(&smd_pkt_number,
			       0,
			       NUM_SMD_PKT_PORTS,
			       DEVICE_NAME);
	if (IS_ERR_VALUE(r)) {
		pr_err("%s: alloc_chrdev_region() failed ret:%i\n",
		       __func__, r);
		goto error0;
	}

	smd_pkt_classp = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(smd_pkt_classp)) {
		pr_err("%s: class_create() failed ENOMEM\n", __func__);
		r = -ENOMEM;
		goto error1;
	}

	for (i = 0; i < NUM_SMD_PKT_PORTS; ++i) {
		smd_pkt_devp[i] = kzalloc(sizeof(struct smd_pkt_dev),
					 GFP_KERNEL);
		if (IS_ERR(smd_pkt_devp[i])) {
			pr_err("%s: kzalloc() failed for smd_pkt_dev id:%d\n",
				__func__, i);
			r = -ENOMEM;
			goto error2;
		}

		smd_pkt_devp[i]->i = i;

		init_waitqueue_head(&smd_pkt_devp[i]->ch_read_wait_queue);
		init_waitqueue_head(&smd_pkt_devp[i]->ch_write_wait_queue);
		smd_pkt_devp[i]->is_open = 0;
		smd_pkt_devp[i]->poll_mode = 0;
		smd_pkt_devp[i]->wakelock_locked = 0;
		init_waitqueue_head(&smd_pkt_devp[i]->ch_opened_wait_queue);

		spin_lock_init(&smd_pkt_devp[i]->pa_spinlock);
		mutex_init(&smd_pkt_devp[i]->ch_lock);
		mutex_init(&smd_pkt_devp[i]->rx_lock);
		mutex_init(&smd_pkt_devp[i]->tx_lock);

		cdev_init(&smd_pkt_devp[i]->cdev, &smd_pkt_fops);
		smd_pkt_devp[i]->cdev.owner = THIS_MODULE;

		r = cdev_add(&smd_pkt_devp[i]->cdev,
			     (smd_pkt_number + i),
			     1);

		if (IS_ERR_VALUE(r)) {
			pr_err("%s: cdev_add() failed for smd_pkt_dev id:%d"
			       " ret:%i\n", __func__, i, r);
			kfree(smd_pkt_devp[i]);
			goto error2;
		}

		smd_pkt_devp[i]->devicep =
			device_create(smd_pkt_classp,
				      NULL,
				      (smd_pkt_number + i),
				      NULL,
				      smd_pkt_dev_name[i]);

		if (IS_ERR(smd_pkt_devp[i]->devicep)) {
			pr_err("%s: device_create() failed for smd_pkt_dev"
			       " id:%d\n", __func__, i);
			r = -ENOMEM;
			cdev_del(&smd_pkt_devp[i]->cdev);
			kfree(smd_pkt_devp[i]);
			goto error2;
		}
		if (device_create_file(smd_pkt_devp[i]->devicep,
					&dev_attr_open_timeout))
			pr_err("%s: unable to create device attr for"
			       " smd_pkt_dev id:%d\n", __func__, i);
	}

	INIT_DELAYED_WORK(&loopback_work, loopback_probe_worker);

	D_STATUS("SMD Packet Port Driver Initialized.\n");
	return 0;

 error2:
	if (i > 0) {
		while (--i >= 0) {
			cdev_del(&smd_pkt_devp[i]->cdev);
			kfree(smd_pkt_devp[i]);
			device_destroy(smd_pkt_classp,
				       MKDEV(MAJOR(smd_pkt_number), i));
		}
	}

	class_destroy(smd_pkt_classp);
 error1:
	unregister_chrdev_region(MAJOR(smd_pkt_number), NUM_SMD_PKT_PORTS);
 error0:
	return r;
}

static void __exit smd_pkt_cleanup(void)
{
	int i;

	for (i = 0; i < NUM_SMD_PKT_PORTS; ++i) {
		cdev_del(&smd_pkt_devp[i]->cdev);
		kfree(smd_pkt_devp[i]);
		device_destroy(smd_pkt_classp,
			       MKDEV(MAJOR(smd_pkt_number), i));
	}

	class_destroy(smd_pkt_classp);

	unregister_chrdev_region(MAJOR(smd_pkt_number), NUM_SMD_PKT_PORTS);
}

module_init(smd_pkt_init);
module_exit(smd_pkt_cleanup);

MODULE_DESCRIPTION("MSM Shared Memory Packet Port");
MODULE_LICENSE("GPL v2");

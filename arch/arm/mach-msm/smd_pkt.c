/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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

#include <mach/msm_smd.h>
#include <mach/peripheral-loader.h>

#include "smd_private.h"
#ifdef CONFIG_ARCH_FSM9XXX
#define NUM_SMD_PKT_PORTS 4
#else
#define NUM_SMD_PKT_PORTS 12
#endif

#define LOOPBACK_INX (NUM_SMD_PKT_PORTS - 1)

#define DEVICE_NAME "smdpkt"

struct smd_pkt_dev {
	struct cdev cdev;
	struct device *devicep;
	void *pil;
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
	int needed_space;
	int is_open;
	unsigned ch_size;
	uint open_modem_wait;

	int has_reset;
	int do_reset_notification;
	struct completion ch_allocated;

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
#define DEBUG

#ifdef DEBUG
#define D_DUMP_BUFFER(prestr, cnt, buf) \
do { \
	if (msm_smd_pkt_debug_mask) \
		print_hex_dump(KERN_DEBUG, prestr, \
				DUMP_PREFIX_NONE, 16, 1, \
				buf, cnt, 1); \
} while (0)
#else
#define D_DUMP_BUFFER(prestr, cnt, buf) do {} while (0)
#endif

#ifdef DEBUG
#define D(x...) if (msm_smd_pkt_debug_mask) printk(x)
#else
#define D(x...) do {} while (0)
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
	return sprintf(buf, "%d\n", smd_pkt_devp[i]->open_modem_wait);
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

	wake_up_interruptible(&smd_pkt_devp->ch_read_wait_queue);
	wake_up_interruptible(&smd_pkt_devp->ch_write_wait_queue);
	wake_up_interruptible(&smd_pkt_devp->ch_opened_wait_queue);
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
		ret = smd_tiocmget(smd_pkt_devp->ch);
		break;
	case TIOCMSET:
		ret = smd_tiocmset(smd_pkt_devp->ch, arg, ~arg);
		break;
	case SMD_PKT_IOCTL_BLOCKING_WRITE:
		ret = get_user(smd_pkt_devp->blocking_write, (int *)arg);
		break;
	default:
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
	struct smd_pkt_dev *smd_pkt_devp;
	struct smd_channel *chl;

	D(KERN_ERR "%s: read %i bytes\n",
	  __func__, count);

	smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp || !smd_pkt_devp->ch)
		return -EINVAL;

	if (smd_pkt_devp->do_reset_notification) {
		/* notify client that a reset occurred */
		return notify_reset(smd_pkt_devp);
	}

	chl = smd_pkt_devp->ch;
wait_for_packet:
	r = wait_event_interruptible(smd_pkt_devp->ch_read_wait_queue,
				     (smd_cur_packet_size(chl) > 0 &&
				      smd_read_avail(chl) >=
				      smd_cur_packet_size(chl)) ||
				     smd_pkt_devp->has_reset);

	if (smd_pkt_devp->has_reset)
		return notify_reset(smd_pkt_devp);

	if (r < 0) {
		/* qualify error message */
		if (r != -ERESTARTSYS) {
			/* we get this anytime a signal comes in */
			printk(KERN_ERR "ERROR:%s:%i:%s: "
			       "wait_event_interruptible ret %i\n",
			       __FILE__,
			       __LINE__,
			       __func__,
			       r
				);
		}
		return r;
	}

	/* Here we have a whole packet waiting for us */

	mutex_lock(&smd_pkt_devp->rx_lock);
	bytes_read = smd_cur_packet_size(smd_pkt_devp->ch);

	D(KERN_ERR "%s: after wait_event_interruptible bytes_read = %i\n",
	  __func__, bytes_read);

	if (bytes_read == 0 ||
	    bytes_read < smd_read_avail(smd_pkt_devp->ch)) {
		D(KERN_ERR "%s: Nothing to read\n", __func__);
		mutex_unlock(&smd_pkt_devp->rx_lock);
		goto wait_for_packet;
	}

	if (bytes_read > count) {
		printk(KERN_ERR "packet size %i > buffer size %i, "
		       "dropping packet!", bytes_read, count);
		smd_read(smd_pkt_devp->ch, 0, bytes_read);
		mutex_unlock(&smd_pkt_devp->rx_lock);
		return -EINVAL;
	}

	if (smd_read_user_buffer(smd_pkt_devp->ch, buf, bytes_read)
	    != bytes_read) {
		mutex_unlock(&smd_pkt_devp->rx_lock);
		if (smd_pkt_devp->has_reset)
			return notify_reset(smd_pkt_devp);

		printk(KERN_ERR "user read: not enough data?!\n");
		return -EINVAL;
	}
	D_DUMP_BUFFER("read: ", bytes_read, buf);
	mutex_unlock(&smd_pkt_devp->rx_lock);

	D(KERN_ERR "%s: just read %i bytes\n",
	  __func__, bytes_read);

	/* check and wakeup read threads waiting on this device */
	check_and_wakeup_reader(smd_pkt_devp);

	return bytes_read;
}

ssize_t smd_pkt_write(struct file *file,
		       const char __user *buf,
		       size_t count,
		       loff_t *ppos)
{
	int r = 0;
	struct smd_pkt_dev *smd_pkt_devp;
	DEFINE_WAIT(write_wait);

	D(KERN_ERR "%s: writting %i bytes\n",
	  __func__, count);

	smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp || !smd_pkt_devp->ch)
		return -EINVAL;

	if (count > smd_pkt_devp->ch_size)
		return -EINVAL;

	if (smd_pkt_devp->do_reset_notification) {
		/* notify client that a reset occurred */
		return notify_reset(smd_pkt_devp);
	}

	if (smd_pkt_devp->blocking_write) {
		for (;;) {
			mutex_lock(&smd_pkt_devp->tx_lock);
			if (smd_pkt_devp->has_reset) {
				smd_disable_read_intr(smd_pkt_devp->ch);
				mutex_unlock(&smd_pkt_devp->tx_lock);
				return notify_reset(smd_pkt_devp);
			}
			if (signal_pending(current)) {
				smd_disable_read_intr(smd_pkt_devp->ch);
				mutex_unlock(&smd_pkt_devp->tx_lock);
				return -ERESTARTSYS;
			}

			prepare_to_wait(&smd_pkt_devp->ch_write_wait_queue,
					&write_wait, TASK_INTERRUPTIBLE);
			smd_enable_read_intr(smd_pkt_devp->ch);
			if (smd_write_avail(smd_pkt_devp->ch) < count) {
				if (!smd_pkt_devp->needed_space ||
				    count < smd_pkt_devp->needed_space)
					smd_pkt_devp->needed_space = count;
				mutex_unlock(&smd_pkt_devp->tx_lock);
				schedule();
			} else
				break;
		}
		finish_wait(&smd_pkt_devp->ch_write_wait_queue, &write_wait);
		smd_disable_read_intr(smd_pkt_devp->ch);
		if (smd_pkt_devp->has_reset) {
			mutex_unlock(&smd_pkt_devp->tx_lock);
			return notify_reset(smd_pkt_devp);
		}
		if (signal_pending(current)) {
			mutex_unlock(&smd_pkt_devp->tx_lock);
			return -ERESTARTSYS;
		}
	} else {
		if (smd_pkt_devp->has_reset)
			return notify_reset(smd_pkt_devp);
		if (signal_pending(current))
			return -ERESTARTSYS;

		mutex_lock(&smd_pkt_devp->tx_lock);
		if (smd_write_avail(smd_pkt_devp->ch) < count) {
			D(KERN_ERR "%s: Not enough space to write\n",
				    __func__);
			mutex_unlock(&smd_pkt_devp->tx_lock);
			return -ENOMEM;
		}
	}

	D_DUMP_BUFFER("write: ", count, buf);

	smd_pkt_devp->needed_space = 0;

	r = smd_write_user_buffer(smd_pkt_devp->ch, buf, count);
	if (r != count) {
		mutex_unlock(&smd_pkt_devp->tx_lock);
		if (smd_pkt_devp->has_reset)
			return notify_reset(smd_pkt_devp);

		printk(KERN_ERR "ERROR:%s:%i:%s: "
		       "smd_write(ch,buf,count = %i) ret %i.\n",
		       __FILE__,
		       __LINE__,
		       __func__,
		       count,
		       r);
		return r;
	}
	mutex_unlock(&smd_pkt_devp->tx_lock);

	D(KERN_ERR "%s: just wrote %i bytes\n",
	       __func__, count);

	return count;
}

static unsigned int smd_pkt_poll(struct file *file, poll_table *wait)
{
	struct smd_pkt_dev *smd_pkt_devp;
	unsigned int mask = 0;

	smd_pkt_devp = file->private_data;
	if (!smd_pkt_devp)
		return POLLERR;

	poll_wait(file, &smd_pkt_devp->ch_read_wait_queue, wait);
	if (smd_read_avail(smd_pkt_devp->ch))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static void check_and_wakeup_reader(struct smd_pkt_dev *smd_pkt_devp)
{
	int sz;

	if (!smd_pkt_devp || !smd_pkt_devp->ch)
		return;

	sz = smd_cur_packet_size(smd_pkt_devp->ch);
	if (sz == 0) {
		D(KERN_ERR "%s: packet size is 0\n", __func__);
		return;
	}
	if (sz > smd_read_avail(smd_pkt_devp->ch)) {
		D(KERN_ERR "%s: packet size is %i - "
		  "the whole packet isn't here\n",
		  __func__, sz);
		return;
	}

	/* here we have a packet of size sz ready */
	wake_up_interruptible(&smd_pkt_devp->ch_read_wait_queue);
	D(KERN_ERR "%s: after wake_up\n", __func__);
}

static void check_and_wakeup_writer(struct smd_pkt_dev *smd_pkt_devp)
{
	int sz;

	if (!smd_pkt_devp || !smd_pkt_devp->ch)
		return;

	sz = smd_write_avail(smd_pkt_devp->ch);
	if (sz >= smd_pkt_devp->needed_space) {
		D(KERN_ERR "%s: %d bytes Write Space available\n",
			    __func__, sz);
		smd_disable_read_intr(smd_pkt_devp->ch);
		wake_up_interruptible(&smd_pkt_devp->ch_write_wait_queue);
	}
}

static void ch_notify(void *priv, unsigned event)
{
	struct smd_pkt_dev *smd_pkt_devp = priv;

	if (smd_pkt_devp->ch == 0)
		return;

	switch (event) {
	case SMD_EVENT_DATA: {
		D(KERN_ERR "%s: data\n", __func__);
		check_and_wakeup_reader(smd_pkt_devp);
		if (smd_pkt_devp->blocking_write)
			check_and_wakeup_writer(smd_pkt_devp);
		D(KERN_ERR "%s: data after check_and_wakeup\n", __func__);
		break;
	}
	case SMD_EVENT_OPEN:
		D(KERN_ERR "%s: smd opened\n",
		  __func__);

		smd_pkt_devp->has_reset = 0;
		smd_pkt_devp->is_open = 1;
		wake_up_interruptible(&smd_pkt_devp->ch_opened_wait_queue);
		break;
	case SMD_EVENT_CLOSE:
		smd_pkt_devp->is_open = 0;
		printk(KERN_ERR "%s: smd closed\n",
		       __func__);

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
	"apr_apps_user",
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
	"apr_apps_user",
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
};
#endif

static int smd_pkt_dummy_probe(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < NUM_SMD_PKT_PORTS; i++) {
		if (!strncmp(pdev->name, smd_ch_name[i], SMD_MAX_CH_NAME_LEN)) {
			complete_all(&smd_pkt_devp[i]->ch_allocated);
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
	char *peripheral = NULL;

	smd_pkt_devp = container_of(inode->i_cdev, struct smd_pkt_dev, cdev);

	if (!smd_pkt_devp)
		return -EINVAL;

	file->private_data = smd_pkt_devp;

	mutex_lock(&smd_pkt_devp->ch_lock);
	if (smd_pkt_devp->ch == 0) {

		if (smd_ch_edge[smd_pkt_devp->i] == SMD_APPS_MODEM)
			peripheral = "modem";
		else if (smd_ch_edge[smd_pkt_devp->i] == SMD_APPS_QDSP)
			peripheral = "q6";

		if (peripheral) {
			smd_pkt_devp->pil = pil_get(peripheral);
			if (IS_ERR(smd_pkt_devp->pil)) {
				r = PTR_ERR(smd_pkt_devp->pil);
				goto out;
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
					pr_err("%s: wait failed for smd port:"
					       " %d\n", __func__, r);
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
		if (r == 0)
			r = -ETIMEDOUT;

		if (r < 0) {
			pr_err("%s: wait failed for smd open: %d\n",
			       __func__, r);
		} else if (!smd_pkt_devp->is_open) {
			pr_err("%s: Invalid open notification\n", __func__);
			r = -ENODEV;
		} else {
			smd_disable_read_intr(smd_pkt_devp->ch);
			smd_pkt_devp->ch_size =
				smd_write_avail(smd_pkt_devp->ch);
			r = 0;
		}
	}
release_pil:
	if (peripheral && (r < 0))
		pil_put(smd_pkt_devp->pil);
out:
	mutex_unlock(&smd_pkt_devp->ch_lock);

	return r;
}

int smd_pkt_release(struct inode *inode, struct file *file)
{
	int r = 0;
	struct smd_pkt_dev *smd_pkt_devp = file->private_data;

	if (!smd_pkt_devp)
		return -EINVAL;

	clean_and_signal(smd_pkt_devp);

	mutex_lock(&smd_pkt_devp->ch_lock);
	if (smd_pkt_devp->ch != 0) {
		r = smd_close(smd_pkt_devp->ch);
		smd_pkt_devp->ch = 0;
		smd_pkt_devp->blocking_write = 0;
		if (smd_pkt_devp->pil)
			pil_put(smd_pkt_devp->pil);
	}
	mutex_unlock(&smd_pkt_devp->ch_lock);

	smd_pkt_devp->has_reset = 0;
	smd_pkt_devp->do_reset_notification = 0;

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
		printk(KERN_ERR "ERROR:%s:%i:%s: "
		       "alloc_chrdev_region() ret %i.\n",
		       __FILE__,
		       __LINE__,
		       __func__,
		       r);
		goto error0;
	}

	smd_pkt_classp = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(smd_pkt_classp)) {
		printk(KERN_ERR "ERROR:%s:%i:%s: "
		       "class_create() ENOMEM\n",
		       __FILE__,
		       __LINE__,
		       __func__);
		r = -ENOMEM;
		goto error1;
	}

	for (i = 0; i < NUM_SMD_PKT_PORTS; ++i) {
		smd_pkt_devp[i] = kzalloc(sizeof(struct smd_pkt_dev),
					 GFP_KERNEL);
		if (IS_ERR(smd_pkt_devp[i])) {
			printk(KERN_ERR "ERROR:%s:%i:%s kmalloc() ENOMEM\n",
			       __FILE__,
			       __LINE__,
			       __func__);
			r = -ENOMEM;
			goto error2;
		}

		smd_pkt_devp[i]->i = i;

		init_waitqueue_head(&smd_pkt_devp[i]->ch_read_wait_queue);
		init_waitqueue_head(&smd_pkt_devp[i]->ch_write_wait_queue);
		smd_pkt_devp[i]->is_open = 0;
		init_waitqueue_head(&smd_pkt_devp[i]->ch_opened_wait_queue);

		mutex_init(&smd_pkt_devp[i]->ch_lock);
		mutex_init(&smd_pkt_devp[i]->rx_lock);
		mutex_init(&smd_pkt_devp[i]->tx_lock);
		init_completion(&smd_pkt_devp[i]->ch_allocated);

		cdev_init(&smd_pkt_devp[i]->cdev, &smd_pkt_fops);
		smd_pkt_devp[i]->cdev.owner = THIS_MODULE;

		r = cdev_add(&smd_pkt_devp[i]->cdev,
			     (smd_pkt_number + i),
			     1);

		if (IS_ERR_VALUE(r)) {
			printk(KERN_ERR "%s:%i:%s: cdev_add() ret %i\n",
			       __FILE__,
			       __LINE__,
			       __func__,
			       r);
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
			printk(KERN_ERR "%s:%i:%s: "
			       "device_create() ENOMEM\n",
			       __FILE__,
			       __LINE__,
			       __func__);
			r = -ENOMEM;
			cdev_del(&smd_pkt_devp[i]->cdev);
			kfree(smd_pkt_devp[i]);
			goto error2;
		}
		if (device_create_file(smd_pkt_devp[i]->devicep,
					&dev_attr_open_timeout))
			pr_err("%s: unable to create device attr on #%d\n",
				__func__, i);

		smd_pkt_devp[i]->driver.probe = smd_pkt_dummy_probe;
		smd_pkt_devp[i]->driver.driver.name = smd_ch_name[i];
		smd_pkt_devp[i]->driver.driver.owner = THIS_MODULE;
		r = platform_driver_register(&smd_pkt_devp[i]->driver);
		if (r)
			goto error2;
	}

	INIT_DELAYED_WORK(&loopback_work, loopback_probe_worker);

	D(KERN_INFO "SMD Packet Port Driver Initialized.\n");
	return 0;

 error2:
	if (i > 0) {
		while (--i >= 0) {
			platform_driver_unregister(&smd_pkt_devp[i]->driver);
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
		platform_driver_unregister(&smd_pkt_devp[i]->driver);
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

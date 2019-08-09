/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/compiler.h>

#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/input.h>
#include "mix_event.h"

static struct class *s_ccci_ge_class;
static dev_t s_ge_status_dev;
struct cdev s_ge_char_dev;

#define SKB_TIME_BUF_LEN 100

///struct skb_time_data {
//	__u16 port;
//	struct timeval time;
//};

struct user_skb_times {
	int count;
	int beg;
	int end;
	//struct skb_time_data times[SKB_TIME_BUF_LEN];
	struct input_event times[SKB_TIME_BUF_LEN];
	wait_queue_head_t wait;
	int pending_event_cnt;

	int user_id;
	int time_interval;
	int once_read_count;

	__be16 dest_port;
	u64 last_skb_time;

} skb_times_ring;

static spinlock_t s_event_update_lock;

static void ccci_util_skbtime_initdata(void)
{
	unsigned long flag;

	spin_lock_irqsave(&s_event_update_lock, flag);

	skb_times_ring.count = 0;
	skb_times_ring.beg = 0;
	skb_times_ring.end = 0;

	skb_times_ring.last_skb_time = 0;

	skb_times_ring.user_id = -1;
	skb_times_ring.time_interval = -1; //5 * 1000000;  //5ms
	skb_times_ring.once_read_count = -1; //all skb times

	skb_times_ring.pending_event_cnt = 0;
	spin_unlock_irqrestore(&s_event_update_lock, flag);

}

static int ccci_util_skbtime_hasdata(void)
{
	unsigned long flag;
	int hasdata;

	spin_lock_irqsave(&s_event_update_lock, flag);

	hasdata = skb_times_ring.count > 0;

	spin_unlock_irqrestore(&s_event_update_lock, flag);

	return hasdata;
}

static int ccci_util_skbtime_getdata(struct input_event *data)
{
	unsigned long flag;

	spin_lock_irqsave(&s_event_update_lock, flag);

	if (skb_times_ring.count > 0) {
		*data = skb_times_ring.times[skb_times_ring.beg];

		if (skb_times_ring.beg == SKB_TIME_BUF_LEN - 1)
			skb_times_ring.beg = 0;
		else
			skb_times_ring.beg++;

		skb_times_ring.count--;

		spin_unlock_irqrestore(&s_event_update_lock, flag);
		return 1;
	}

	spin_unlock_irqrestore(&s_event_update_lock, flag);
	return 0;
}

static void ccci_util_skbtime_adddata(__be16 ipid, int skb_len, __u16 port)
{
	unsigned long flag;
	int index;

	spin_lock_irqsave(&s_event_update_lock, flag);

	index = skb_times_ring.end;
	if (skb_times_ring.count == SKB_TIME_BUF_LEN) { // buff is full
		if (skb_times_ring.beg == SKB_TIME_BUF_LEN - 1) {
			skb_times_ring.beg = 0;
			skb_times_ring.end = 0;
		} else {
			skb_times_ring.beg++;
			skb_times_ring.end++;
		}
	} else { // buff no full
		if (skb_times_ring.end == SKB_TIME_BUF_LEN - 1) // end is tail
			skb_times_ring.end = 0;
		else
			skb_times_ring.end++;

		skb_times_ring.count++;
	}
	//timeval_to_ktime
	do_gettimeofday(&(skb_times_ring.times[index].time));
	//skb_times_ring.times[index].time = ktime_to_timeval(*ktm);
	//skb_times_ring.times[index].time.tv_sec = ktm.
	skb_times_ring.times[index].type = port;
	skb_times_ring.times[index].code = ipid;
	skb_times_ring.times[index].value = skb_len;

	spin_unlock_irqrestore(&s_event_update_lock, flag);
}

void inject_mix_event(struct sk_buff *skb, struct net_device *dev,
		struct iphdr *iph)
{
	if (iph->protocol != IPPROTO_UDP)
		return;

	//if (skb->sk->sk_state != TCP_ESTABLISHED)
	//	return;

	if ((skb_times_ring.user_id == 999999 ||
		 skb_times_ring.user_id == skb->sk->sk_uid.val) &&
		 strncmp(dev->name, "ccmni", 5) == 0) {

		if (skb_times_ring.time_interval > 0 &&
			(sched_clock() - skb_times_ring.last_skb_time)
			< skb_times_ring.time_interval) {
			return;
		}

		skb_times_ring.last_skb_time = sched_clock();

		//ktm = ktime_get();
		ccci_util_skbtime_adddata(ntohs(iph->id),
				ntohs(iph->tot_len), ntohs(udp_hdr(skb)->dest));

		skb_times_ring.pending_event_cnt = 1;
		wake_up_interruptible(&skb_times_ring.wait);
	}
}

static int mix_open(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	nonseekable_open(inode, filp);

	return 0;
}

static int mix_release(struct inode *inode, struct file *filp)
{
	ccci_util_skbtime_initdata();
	wake_up_interruptible(&skb_times_ring.wait);

	return 0;
}

static int ccci_util_analyze_parameter(char *temp_buf, int size)
{
	char *psub, *pname, *pvalue;
	long user_id = -1;
	long time_interval = -1;
	long read_count = -1;

	pname = temp_buf;
	while (true) {
		psub = strchr(pname, '|');

		if (psub) {
			*psub = '\0';
			psub += 1;
		}

		pvalue = strchr(pname, '=');
		if (pvalue) {
			*pvalue = '\0';
			pvalue += 1;
		}

		if (strstr(pname, "user_id")) {
			if (pvalue && *pvalue)
				if (kstrtol(pvalue, 10, &user_id))
					return 0;
		} else if (strstr(pname, "time_interval")) {
			if (pvalue && *pvalue)
				if (kstrtol(pvalue, 10, &time_interval))
					return 0;
		} else if (strstr(pname, "read_count")) {
			if (pvalue && *pvalue)
				if (kstrtol(pvalue, 10, &read_count))
					return 0;
		}

		if (!psub)
			break;

		pname = psub;
	}

	skb_times_ring.user_id = user_id;
	//ns * 1000000 = ms
	skb_times_ring.time_interval = time_interval * 1000000;
	skb_times_ring.once_read_count = read_count;

	return size;
}

static ssize_t mix_write(struct file *filp,
		const char __user *buf, size_t size, loff_t *ppos)
{
	char temp_buf[500];

	if (copy_from_user(temp_buf, buf, size))
		return -EFAULT;
	temp_buf[size] = '\0';

	return ccci_util_analyze_parameter(temp_buf, size);
}

static int copy_skbtime_to_user(char __user *buf, int buf_len)
{
	int offset = 0;
	struct input_event data;
	int readcount = 0;


	while (buf_len >= sizeof(struct input_event)) {
		if (!ccci_util_skbtime_getdata(&data))
			break;

		if (copy_to_user(buf + offset, &data,
						 sizeof(struct input_event))) {
			offset = -EFAULT;
			break; //return -EFAULT;
		}

		buf_len -= sizeof(struct input_event);
		offset += sizeof(struct input_event);

		if (skb_times_ring.once_read_count > 0) {
			readcount++;
			if (readcount >= skb_times_ring.once_read_count)
				break;
		}
	}

	return offset;
}

static ssize_t mix_read(struct file *filp,
		char __user *buf,	size_t size, loff_t *ppos)
{
	int ret;

	/* For Non-block read */
	if (filp->f_flags & O_NONBLOCK) {
		if (ccci_util_skbtime_hasdata() == 0) {
			return -EAGAIN;

		} else {
			ret = copy_skbtime_to_user(buf, (int)size);
			skb_times_ring.pending_event_cnt =
					ccci_util_skbtime_hasdata();
			return ret;
		}
	}

	/* For block read */
	while (1) {
		if (ccci_util_skbtime_hasdata()) {
			ret = copy_skbtime_to_user(buf, (int) size);
			skb_times_ring.pending_event_cnt =
					ccci_util_skbtime_hasdata();
			return ret;
		}

		//skb_times_ring.pending_event_cnt > 0
		//is true:exit, false:suspend
		ret = wait_event_interruptible(skb_times_ring.wait,
				skb_times_ring.pending_event_cnt > 0);
		if (ret)
			return ret;
	}

	return 0;
}

static unsigned int mix_poll(struct file *filp,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &skb_times_ring.wait, wait);

	if (skb_times_ring.pending_event_cnt > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static const struct file_operations mix_event_fops = {
	.owner = THIS_MODULE,
	.open = mix_open,
	.read = mix_read,
	.write = mix_write,
	.unlocked_ioctl = NULL,
	.poll = mix_poll,
	.release = mix_release,
};

int mix_event_init(void)
{
	int ret;

	spin_lock_init(&s_event_update_lock);
	init_waitqueue_head(&skb_times_ring.wait);
	ccci_util_skbtime_initdata();

	ret = alloc_chrdev_region(&s_ge_status_dev, 0, 1, "mix_event");
	if (ret != 0) {
		pr_notice("[mixdev] alloc chrdev fail (%d)\n", ret);
		goto _exit_1;
	}

	cdev_init(&s_ge_char_dev, &mix_event_fops);
	s_ge_char_dev.owner = THIS_MODULE;

	ret = cdev_add(&s_ge_char_dev, s_ge_status_dev, 1);
	if (ret) {
		pr_notice("[mixdev] cdev_add failed\n");
		goto _exit_2;
	}

	/* create a device node in directory /dev/ */
	//dev_n = MKDEV(MAJOR(s_ge_status_dev), 0);
	s_ccci_ge_class = class_create(THIS_MODULE, "mix_event");
	device_create(s_ccci_ge_class, NULL,
			s_ge_status_dev, NULL, "mix_event");

	return 0;

_exit_2:
	cdev_del(&s_ge_char_dev);

_exit_1:
	unregister_chrdev_region(s_ge_status_dev, 1);

	return -1;
}

static int __init __mix_event_init(void)
{
	mix_event_init();

	return 0;
}

subsys_initcall(__mix_event_init);
MODULE_DESCRIPTION("MTK MIX EVENT Driver");
MODULE_LICENSE("GPL");


/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <asm-generic/poll.h>

#include "ramdump.h"

#define RAMDUMP_WAIT_MSECS	120000

struct ramdump_device {
	char name[256];

	unsigned int data_ready;
	unsigned int consumer_present;
	int ramdump_status;

	struct completion ramdump_complete;
	struct miscdevice device;

	wait_queue_head_t dump_wait_q;
	int nsegments;
	struct ramdump_segment *segments;
};

static int ramdump_open(struct inode *inode, struct file *filep)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	rd_dev->consumer_present = 1;
	rd_dev->ramdump_status = 0;
	return 0;
}

static int ramdump_release(struct inode *inode, struct file *filep)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	rd_dev->consumer_present = 0;
	rd_dev->data_ready = 0;
	complete(&rd_dev->ramdump_complete);
	return 0;
}

static unsigned long offset_translate(loff_t user_offset,
		struct ramdump_device *rd_dev, unsigned long *data_left)
{
	int i = 0;

	for (i = 0; i < rd_dev->nsegments; i++)
		if (user_offset >= rd_dev->segments[i].size)
			user_offset -= rd_dev->segments[i].size;
		else
			break;

	if (i == rd_dev->nsegments) {
		pr_debug("Ramdump(%s): offset_translate returning zero\n",
				rd_dev->name);
		*data_left = 0;
		return 0;
	}

	*data_left = rd_dev->segments[i].size - user_offset;

	pr_debug("Ramdump(%s): Returning address: %llx, data_left = %ld\n",
		rd_dev->name, rd_dev->segments[i].address + user_offset,
		*data_left);

	return rd_dev->segments[i].address + user_offset;
}

#define MAX_IOREMAP_SIZE SZ_1M

static int ramdump_read(struct file *filep, char __user *buf, size_t count,
			loff_t *pos)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	void *device_mem = NULL;
	unsigned long data_left = 0;
	unsigned long addr = 0;
	size_t copy_size = 0;
	int ret = 0;

	if (rd_dev->data_ready == 0) {
		pr_err("Ramdump(%s): Read when there's no dump available!",
			rd_dev->name);
		return -EPIPE;
	}

	addr = offset_translate(*pos, rd_dev, &data_left);

	/* EOF check */
	if (data_left == 0) {
		pr_debug("Ramdump(%s): Ramdump complete. %lld bytes read.",
			rd_dev->name, *pos);
		rd_dev->ramdump_status = 0;
		ret = 0;
		goto ramdump_done;
	}

	copy_size = min(count, (size_t)MAX_IOREMAP_SIZE);
	copy_size = min((unsigned long)copy_size, data_left);
	device_mem = ioremap_nocache(addr, copy_size);

	if (device_mem == NULL) {
		pr_err("Ramdump(%s): Unable to ioremap: addr %lx, size %x\n",
			rd_dev->name, addr, copy_size);
		rd_dev->ramdump_status = -1;
		ret = -ENOMEM;
		goto ramdump_done;
	}

	if (copy_to_user(buf, device_mem, copy_size)) {
		pr_err("Ramdump(%s): Couldn't copy all data to user.",
			rd_dev->name);
		iounmap(device_mem);
		rd_dev->ramdump_status = -1;
		ret = -EFAULT;
		goto ramdump_done;
	}

	iounmap(device_mem);
	*pos += copy_size;

	pr_debug("Ramdump(%s): Read %d bytes from address %lx.",
			rd_dev->name, copy_size, addr);

	return copy_size;

ramdump_done:
	rd_dev->data_ready = 0;
	*pos = 0;
	complete(&rd_dev->ramdump_complete);
	return ret;
}

static unsigned int ramdump_poll(struct file *filep,
					struct poll_table_struct *wait)
{
	struct ramdump_device *rd_dev = container_of(filep->private_data,
				struct ramdump_device, device);
	unsigned int mask = 0;

	if (rd_dev->data_ready)
		mask |= (POLLIN | POLLRDNORM);

	poll_wait(filep, &rd_dev->dump_wait_q, wait);
	return mask;
}

const struct file_operations ramdump_file_ops = {
	.open = ramdump_open,
	.release = ramdump_release,
	.read = ramdump_read,
	.poll = ramdump_poll
};

void *create_ramdump_device(const char *dev_name)
{
	int ret;
	struct ramdump_device *rd_dev;

	if (!dev_name) {
		pr_err("%s: Invalid device name.\n", __func__);
		return NULL;
	}

	rd_dev = kzalloc(sizeof(struct ramdump_device), GFP_KERNEL);

	if (!rd_dev) {
		pr_err("%s: Couldn't alloc space for ramdump device!",
			__func__);
		return NULL;
	}

	snprintf(rd_dev->name, ARRAY_SIZE(rd_dev->name), "ramdump_%s",
		 dev_name);

	init_completion(&rd_dev->ramdump_complete);

	rd_dev->device.minor = MISC_DYNAMIC_MINOR;
	rd_dev->device.name = rd_dev->name;
	rd_dev->device.fops = &ramdump_file_ops;

	init_waitqueue_head(&rd_dev->dump_wait_q);

	ret = misc_register(&rd_dev->device);

	if (ret) {
		pr_err("%s: misc_register failed for %s (%d)", __func__,
				dev_name, ret);
		kfree(rd_dev);
		return NULL;
	}

	return (void *)rd_dev;
}

int do_ramdump(void *handle, struct ramdump_segment *segments,
		int nsegments)
{
	int ret, i;
	struct ramdump_device *rd_dev = (struct ramdump_device *)handle;

	if (!rd_dev->consumer_present) {
		pr_err("Ramdump(%s): No consumers. Aborting..\n", rd_dev->name);
		return -EPIPE;
	}

	for (i = 0; i < nsegments; i++)
		segments[i].size = PAGE_ALIGN(segments[i].size);

	rd_dev->segments = segments;
	rd_dev->nsegments = nsegments;

	rd_dev->data_ready = 1;
	rd_dev->ramdump_status = -1;

	INIT_COMPLETION(rd_dev->ramdump_complete);

	/* Tell userspace that the data is ready */
	wake_up(&rd_dev->dump_wait_q);

	/* Wait (with a timeout) to let the ramdump complete */
	ret = wait_for_completion_timeout(&rd_dev->ramdump_complete,
			msecs_to_jiffies(RAMDUMP_WAIT_MSECS));

	if (!ret) {
		pr_err("Ramdump(%s): Timed out waiting for userspace.\n",
			rd_dev->name);
		ret = -EPIPE;
	} else
		ret = (rd_dev->ramdump_status == 0) ? 0 : -EPIPE;

	rd_dev->data_ready = 0;
	return ret;
}

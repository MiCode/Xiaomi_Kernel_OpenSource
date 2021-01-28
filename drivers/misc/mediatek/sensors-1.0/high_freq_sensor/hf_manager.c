/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/bitmap.h>

#include "hf_manager.h"

static DECLARE_BITMAP(sensor_list_bitmap, HIGH_FREQUENCY_SENSOR_MAX);
static LIST_HEAD(hf_manager_list);
static DEFINE_SPINLOCK(hf_fifo_lock);
static DEFINE_MUTEX(hf_manager_list_mtx);
static struct task_struct *hf_manager_kthread_task;
static struct kthread_worker hf_manager_kthread_worker;

static struct coordinate coordinates[] = {
	{ { 1, 1, 1}, {0, 1, 2} },
	{ { -1, 1, 1}, {1, 0, 2} },
	{ { -1, -1, 1}, {0, 1, 2} },
	{ { 1, -1, 1}, {1, 0, 2} },

	{ { -1, 1, -1}, {0, 1, 2} },
	{ { 1, 1, -1}, {1, 0, 2} },
	{ { 1, -1, -1}, {0, 1, 2} },
	{ { -1, -1, -1}, {1, 0, 2} },
};

void coordinate_map(unsigned char direction, int32_t *data)
{
	int32_t temp[3] = {0};

	if (direction >= sizeof(coordinates))
		return;

	temp[coordinates[direction].map[0]] =
		coordinates[direction].sign[0] * data[0];
	temp[coordinates[direction].map[1]] =
		coordinates[direction].sign[1] * data[1];
	temp[coordinates[direction].map[2]] =
		coordinates[direction].sign[2] * data[2];

	data[0] = temp[0];
	data[1] = temp[1];
	data[2] = temp[2];
}

static int hf_manager_report(struct hf_manager_fifo *hf_fifo,
		struct hf_manager_event *event)
{
	unsigned long flags;
	unsigned int next = 0;

	spin_lock_irqsave(&hf_fifo->buffer_lock, flags);
	if (unlikely(hf_fifo->buffull == true)) {
		pr_err_ratelimited("%s buffull, head:%d, tail:%d\n",
			__func__, hf_fifo->head, hf_fifo->tail);
		spin_unlock(&hf_fifo->buffer_lock);
		wake_up_interruptible(&hf_fifo->wait);
		return -1;
	}
	hf_fifo->buffer[hf_fifo->head++] = *event;
	hf_fifo->head &= hf_fifo->bufsize - 1;
	/* remain 1 count */
	next = hf_fifo->head + 1;
	next &= hf_fifo->bufsize - 1;
	if (unlikely(next == hf_fifo->tail))
		hf_fifo->buffull = true;
	spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);

	wake_up_interruptible(&hf_fifo->wait);
	return 0;
}

static void hf_manager_io_schedule(struct hf_manager *manager)
{
	if (!READ_ONCE(manager->io_enabled))
		return;
	if (manager->hf_dev->device_bus == HF_DEVICE_IO_ASYNC)
		tasklet_schedule(&manager->io_work_tasklet);
	else if (manager->hf_dev->device_bus == HF_DEVICE_IO_SYNC)
		kthread_queue_work(&hf_manager_kthread_worker,
			&manager->io_kthread_work);
}

static void hf_manager_io_report(struct hf_manager *manager,
		struct hf_manager_event *event)
{
	unsigned long flags;
	struct hf_manager_fifo *hf_fifo = NULL;

	spin_lock_irqsave(&hf_fifo_lock, flags);
	hf_fifo = manager->hf_fifo;
	if (hf_fifo)
		hf_manager_report(hf_fifo, event);
	spin_unlock_irqrestore(&hf_fifo_lock, flags);
}

static void hf_manager_io_complete(struct hf_manager *manager)
{
	clear_bit(HF_MANAGER_IO_IN_PROGRESS, &(manager->flags));
	if (test_and_clear_bit(HF_MANAGER_IO_READY, &manager->flags))
		hf_manager_io_schedule(manager);
}

static void hf_manager_io_sample(struct hf_manager *manager)
{
	int retval;

	if (!manager->hf_dev || !manager->hf_dev->sample)
		return;

	if (!test_and_set_bit(HF_MANAGER_IO_IN_PROGRESS, &manager->flags)) {
		retval = manager->hf_dev->sample(manager->hf_dev);
		if (retval) {
			clear_bit(HF_MANAGER_IO_IN_PROGRESS,
				  &manager->flags);
			hf_manager_io_schedule(manager);
		}
	}
}

static void hf_manager_io_tasklet(unsigned long data)
{
	struct hf_manager *manager = (struct hf_manager *)data;

	hf_manager_io_sample(manager);
}

static void hf_manager_io_kthread_work(struct kthread_work *work)
{
	struct hf_manager *manager =
		container_of(work, struct hf_manager, io_kthread_work);

	hf_manager_io_sample(manager);
}

static void hf_manager_sched_sample(struct hf_manager *manager)
{
	if (!test_bit(HF_MANAGER_IO_IN_PROGRESS, &manager->flags))
		hf_manager_io_schedule(manager);
	else
		set_bit(HF_MANAGER_IO_READY, &manager->flags);
}

static enum hrtimer_restart hf_manager_io_poll(struct hrtimer *timer)
{
	struct hf_manager *manager =
		(struct hf_manager *)container_of(timer,
			struct hf_manager, io_poll_timer);

	hf_manager_sched_sample(manager);
	hrtimer_forward_now(&manager->io_poll_timer,
		READ_ONCE(manager->io_poll_interval));
	return HRTIMER_RESTART;
}

static void hf_manager_io_interrupt(struct hf_manager *manager)
{
	hf_manager_sched_sample(manager);
}

int hf_manager_create(struct hf_device *device)
{
	unsigned char sensor_id = 0;
	int i = 0;
	int err = 0;
	struct hf_manager *manager = NULL;

	if (!device || !device->dev_name ||
		!device->support_list || !device->support_size)
		return -EFAULT;

	manager = kzalloc(sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->hf_dev = device;
	device->manager = manager;

	clear_bit(HF_MANAGER_IO_IN_PROGRESS, &manager->flags);
	clear_bit(HF_MANAGER_IO_READY, &manager->flags);

	if (device->device_poll == HF_DEVICE_IO_POLLING) {
		hrtimer_init(&manager->io_poll_timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		manager->io_poll_timer.function = hf_manager_io_poll;
	} else if (device->device_poll == HF_DEVICE_IO_INTERRUPT) {
		manager->interrupt = hf_manager_io_interrupt;
	}
	manager->report = hf_manager_io_report;
	manager->complete = hf_manager_io_complete;

	if (device->device_bus == HF_DEVICE_IO_ASYNC)
		tasklet_init(&manager->io_work_tasklet,
			hf_manager_io_tasklet, (unsigned long)manager);
	else if (device->device_bus == HF_DEVICE_IO_SYNC)
		kthread_init_work(&manager->io_kthread_work,
			hf_manager_io_kthread_work);

	INIT_LIST_HEAD(&manager->list);
	mutex_lock(&hf_manager_list_mtx);
	for (i = 0; i < device->support_size; ++i) {
		sensor_id = device->support_list[i];
		if (test_and_set_bit(sensor_id, sensor_list_bitmap)) {
			mutex_unlock(&hf_manager_list_mtx);
			pr_err("%s %s %d repeat\n", __func__,
				device->dev_name, sensor_id);
			err = -EBUSY;
			goto out_err;
		} else
			pr_err("%s %s register %d\n", __func__,
				device->dev_name, sensor_id);
	}
	list_add(&manager->list, &hf_manager_list);
	mutex_unlock(&hf_manager_list_mtx);

	return 0;
out_err:
	kfree(manager);
	return err;
}

int hf_manager_destroy(struct hf_manager *manager)
{
	int i = 0;
	struct hf_device *device = NULL;

	if (!manager || !manager->hf_dev || !manager->hf_dev->support_list)
		return -EFAULT;

	device = manager->hf_dev;
	mutex_lock(&hf_manager_list_mtx);
	for (i = 0; i < device->support_size; ++i) {
		clear_bit(device->support_list[i],
			sensor_list_bitmap);
	}
	list_del(&manager->list);
	mutex_unlock(&hf_manager_list_mtx);
	if (manager->hf_dev->device_bus == HF_DEVICE_IO_ASYNC)
		tasklet_kill(&manager->io_work_tasklet);

	kfree(manager);
	return 0;
}

static int hf_manager_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	unsigned long flags;
	struct hf_manager *manager = NULL;
	struct hf_manager_fifo *hf_fifo = NULL;

	hf_fifo = kzalloc(sizeof(*hf_fifo), GFP_KERNEL);
	if (!hf_fifo) {
		err = -ENOMEM;
		goto err_out;
	}

	hf_fifo->head = 0;
	hf_fifo->tail = 0;
	hf_fifo->bufsize = HF_MANAGER_FIFO_SIZE;
	hf_fifo->buffull = false;
	spin_lock_init(&hf_fifo->buffer_lock);
	init_waitqueue_head(&hf_fifo->wait);
	hf_fifo->buffer =
		kcalloc(hf_fifo->bufsize, sizeof(*hf_fifo->buffer),
			GFP_KERNEL);
	if (!hf_fifo->buffer) {
		err = -ENOMEM;
		goto err_free;
	}

	mutex_lock(&hf_manager_list_mtx);
	spin_lock_irqsave(&hf_fifo_lock, flags);
	list_for_each_entry(manager, &hf_manager_list, list) {
		manager->hf_fifo = hf_fifo;
	}
	spin_unlock_irqrestore(&hf_fifo_lock, flags);
	mutex_unlock(&hf_manager_list_mtx);

	filp->private_data = hf_fifo;
	nonseekable_open(inode, filp);
	return 0;
err_free:
	kfree(hf_fifo);
err_out:
	return err;
}

static int hf_manager_release(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	struct hf_manager *manager = NULL;
	struct hf_manager_fifo *hf_fifo = filp->private_data;

	filp->private_data = NULL;
	mutex_lock(&hf_manager_list_mtx);
	spin_lock_irqsave(&hf_fifo_lock, flags);
	list_for_each_entry(manager, &hf_manager_list, list) {
		manager->hf_fifo = NULL;
	}
	spin_unlock_irqrestore(&hf_fifo_lock, flags);
	mutex_unlock(&hf_manager_list_mtx);
	kfree(hf_fifo->buffer);
	kfree(hf_fifo);
	return 0;
}

static int hf_manager_fetch_next(struct hf_manager_fifo *hf_fifo,
				  struct hf_manager_event *event)
{
	unsigned long flags;
	int have_event;

	spin_lock_irqsave(&hf_fifo->buffer_lock, flags);
	have_event = hf_fifo->head != hf_fifo->tail;
	if (have_event) {
		*event = hf_fifo->buffer[hf_fifo->tail++];
		hf_fifo->tail &= hf_fifo->bufsize - 1;
		hf_fifo->buffull = false;
	}
	spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
	return have_event;
}

static ssize_t hf_manager_read(struct file *filp,
		char __user *buf, size_t count, loff_t *f_pos)
{
	struct hf_manager_fifo *hf_fifo = filp->private_data;
	struct hf_manager_event event;
	size_t read = 0;

	if (count != 0 && count < sizeof(struct hf_manager_event))
		return -EINVAL;

	for (;;) {
		if (hf_fifo->head == hf_fifo->tail)
			return 0;
		if (count == 0)
			break;
		while (read + sizeof(struct hf_manager_event) <= count &&
			hf_manager_fetch_next(hf_fifo, &event)) {
			if (copy_to_user(buf + read,
				&event, sizeof(struct hf_manager_event)))
				return -EFAULT;
			read += sizeof(struct hf_manager_event);
		}
		if (read)
			break;
	}
	return read;
}

static struct hf_manager *hf_manager_find_manager(uint8_t sensor_id)
{
	int i = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;

	list_for_each_entry(manager, &hf_manager_list, list) {
		device = manager->hf_dev;
		if (!device || !device->support_list)
			continue;
		for (i = 0; i < device->support_size; ++i) {
			if (sensor_id == device->support_list[i])
				return manager;
		}
	}
	return NULL;
}

static int hf_manager_drive_device(struct hf_manager_cmd *cmd)
{
	int err = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;

	if (cmd->sensor_id >= HIGH_FREQUENCY_SENSOR_MAX)
		return -EINVAL;

	mutex_lock(&hf_manager_list_mtx);
	if (!test_bit(cmd->sensor_id, sensor_list_bitmap)) {
		pr_err("%s: %d not registered\n", __func__, cmd->sensor_id);
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}
	manager = hf_manager_find_manager(cmd->sensor_id);
	if (!manager) {
		pr_err("%s: no manager finded\n");
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}
	device = manager->hf_dev;
	if (!device || !device->dev_name ||
			!device->batch || !device->enable) {
		pr_err("%s: no hf device or important param finded\n");
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}

	pr_notice("%s: %s: %d,%d,%lld,%lld\n", __func__, device->dev_name,
		cmd->sensor_id, cmd->action, cmd->delay, cmd->latency);

	clear_bit(HF_MANAGER_IO_IN_PROGRESS, &manager->flags);
	clear_bit(HF_MANAGER_IO_READY, &manager->flags);

	if (cmd->action == HF_MANAGER_SENSOR_ENABLE) {
		err = device->batch(device, cmd->sensor_id,
			cmd->delay, cmd->latency);
		err = device->enable(device, cmd->sensor_id, cmd->action);
		WRITE_ONCE(manager->io_enabled, true);
		WRITE_ONCE(manager->io_poll_interval, cmd->delay);
		if (device->device_poll == HF_DEVICE_IO_POLLING &&
			!hrtimer_active(&manager->io_poll_timer))
			hrtimer_start(&manager->io_poll_timer,
				READ_ONCE(manager->io_poll_interval),
					HRTIMER_MODE_REL);
	} else if (cmd->action == HF_MANAGER_SENSOR_DISABLE) {
		err = device->enable(device, cmd->sensor_id, cmd->action);
		WRITE_ONCE(manager->io_enabled, false);
		if (device->device_poll == HF_DEVICE_IO_POLLING &&
			hrtimer_active(&manager->io_poll_timer)) {
			hrtimer_cancel(&manager->io_poll_timer);
			if (device->device_bus == HF_DEVICE_IO_ASYNC)
				tasklet_kill(&manager->io_work_tasklet);
		}
	}
	mutex_unlock(&hf_manager_list_mtx);
	return err;
}

static ssize_t hf_manager_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos)
{
	int err = 0;
	struct hf_manager_cmd cmd;

	memset(&cmd, 0, sizeof(struct hf_manager_cmd));

	if (count != sizeof(struct hf_manager_cmd))
		return -EFAULT;

	if (copy_from_user(&cmd, buf, count))
		return -EFAULT;

	err = hf_manager_drive_device(&cmd);
	return err;
}

static unsigned int hf_manager_poll(struct file *filp,
		struct poll_table_struct *wait)
{
	struct hf_manager_fifo *hf_fifo = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &hf_fifo->wait, wait);

	if (hf_fifo->head != hf_fifo->tail)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static long hf_manager_unlocked_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long hf_manager_compat_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	return 0;
}
#else
#define hf_manager_compat_ioctl NULL
#endif

static ssize_t test_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int err = 0;
	struct hf_manager_cmd cmd;

	memset(&cmd, 0, sizeof(struct hf_manager_cmd));

	if (sscanf(buf, "%d,%d",
		&cmd.sensor_id, &cmd.action) != 2)
		return -EINVAL;

	cmd.delay = 5000000;
	cmd.latency = 0;

	err = hf_manager_drive_device(&cmd);
	return (err < 0) ? err : count;
}

DEVICE_ATTR(test, 0644, NULL, test_store);

static struct attribute *hf_manager_attrs[] = {
	&dev_attr_test.attr,
	NULL
};

static struct attribute_group hf_manager_group = {
	.attrs = hf_manager_attrs
};

static const struct file_operations hf_manager_fops = {
	.owner          = THIS_MODULE,
	.open           = hf_manager_open,
	.release        = hf_manager_release,
	.read           = hf_manager_read,
	.write          = hf_manager_write,
	.poll           = hf_manager_poll,
	.unlocked_ioctl = hf_manager_unlocked_ioctl,
	.compat_ioctl   = hf_manager_compat_ioctl,
};

static int __init hf_manager_init(void)
{
	int major = -1;
	struct class *hf_manager_class;
	struct device *dev;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };

	major = register_chrdev(0, "hf_manager", &hf_manager_fops);
	if (major < 0) {
		pr_err("%s unable to get major %d\n", __func__, major);
		return -1;
	}
	hf_manager_class = class_create(THIS_MODULE, "hf_manager");
	if (IS_ERR(hf_manager_class))
		return PTR_ERR(hf_manager_class);
	dev = device_create(hf_manager_class, NULL, MKDEV(major, 0),
		NULL, "hf_manager");
	if (IS_ERR(dev))
		return -1;

	if (sysfs_create_group(&dev->kobj, &hf_manager_group) < 0)
		return -1;

	kthread_init_worker(&hf_manager_kthread_worker);
	hf_manager_kthread_task = kthread_run(kthread_worker_fn,
			&hf_manager_kthread_worker, "hf_manager");
	if (IS_ERR(hf_manager_kthread_task)) {
		pr_err("%s failed to create kthread\n", __func__);
		return -1;
	}
	sched_setscheduler(hf_manager_kthread_task, SCHED_FIFO, &param);
	return 0;
}
subsys_initcall(hf_manager_init);


MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("high freq sensor manaer driver");
MODULE_LICENSE("GPL");

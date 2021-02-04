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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched_clock.h>
#include <linux/log2.h>

#include "hf_manager.h"

static DECLARE_BITMAP(sensor_list_bitmap, HIGH_FREQUENCY_SENSOR_MAX);
static LIST_HEAD(hf_manager_list);
static DEFINE_MUTEX(hf_manager_list_mtx);
static LIST_HEAD(hf_client_list);
static DEFINE_SPINLOCK(hf_client_list_lock);
static struct sensor_state prev_request[HIGH_FREQUENCY_SENSOR_MAX];

static struct task_struct *hf_manager_kthread_task;
static struct kthread_worker hf_manager_kthread_worker;

static int hf_manager_find_client(struct hf_manager_event *event);

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

static bool filter_event_by_timestamp(struct hf_client_fifo *hf_fifo,
		struct hf_manager_event *event)
{
	if (hf_fifo->last_time_stamp[event->sensor_id] ==
			event->timestamp) {
		return true;
	}
	hf_fifo->last_time_stamp[event->sensor_id] = event->timestamp;
	return false;
}

static int hf_manager_report_event(struct hf_client *client,
		struct hf_manager_event *event)
{
	unsigned long flags;
	unsigned int next = 0;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;

	spin_lock_irqsave(&hf_fifo->buffer_lock, flags);
	if (unlikely(hf_fifo->buffull == true)) {
		pr_err_ratelimited("%s [%s][%d:%d] buffer full, [%d,%d]\n",
			__func__, client->proc_comm, client->leader_pid,
			client->pid, hf_fifo->head, hf_fifo->tail);
		spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
		wake_up_interruptible(&hf_fifo->wait);
		/*
		 * must return -1 when buffer full, tell caller retry
		 * send data some times later.
		 */
		return -1;
	}
	if (unlikely(filter_event_by_timestamp(hf_fifo, event))) {
		pr_err_ratelimited("%s [%s][%d:%d] filterd, [%d,%lld]\n",
			__func__, client->proc_comm, client->leader_pid,
			client->pid, event->sensor_id, event->timestamp);
		spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
		/*
		 * must return 0 when timestamp filtered, tell caller data
		 * already in buffer, don't need send again.
		 */
		return 0;
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
	if (READ_ONCE(manager->hf_dev->device_bus) == HF_DEVICE_IO_ASYNC)
		tasklet_schedule(&manager->io_work_tasklet);
	else if (READ_ONCE(manager->hf_dev->device_bus) == HF_DEVICE_IO_SYNC)
		kthread_queue_work(&hf_manager_kthread_worker,
			&manager->io_kthread_work);
}

static int hf_manager_io_report(struct hf_manager *manager,
		struct hf_manager_event *event)
{
	/* must return 0 when sensor_id exceed and no need to retry */
	if (unlikely(event->sensor_id >= HIGH_FREQUENCY_SENSOR_MAX)) {
		pr_err_ratelimited("%s %d exceed max sensor id\n", __func__,
			event->sensor_id);
		return 0;
	}
	return hf_manager_find_client(event);
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

	manager->io_enabled = false;
	manager->io_poll_interval.tv64 = S64_MAX;

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

	for (i = 0; i < device->support_size; ++i) {
		sensor_id = device->support_list[i];
		if (unlikely(sensor_id >= HIGH_FREQUENCY_SENSOR_MAX)) {
			pr_err("%s %s %d exceed max sensor id\n", __func__,
				device->dev_name, sensor_id);
			err = -EINVAL;
			goto out_err;
		}
		if (test_and_set_bit(sensor_id, sensor_list_bitmap)) {
			pr_err("%s %s %d repeat\n", __func__,
				device->dev_name, sensor_id);
			err = -EBUSY;
			goto out_err;
		} else
			pr_err("%s %s register %d\n", __func__,
				device->dev_name, sensor_id);
	}

	INIT_LIST_HEAD(&manager->list);
	mutex_lock(&hf_manager_list_mtx);
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
	for (i = 0; i < device->support_size; ++i) {
		clear_bit(device->support_list[i],
			sensor_list_bitmap);
	}
	mutex_lock(&hf_manager_list_mtx);
	list_del(&manager->list);
	mutex_unlock(&hf_manager_list_mtx);
	if (manager->hf_dev->device_bus == HF_DEVICE_IO_ASYNC)
		tasklet_kill(&manager->io_work_tasklet);

	kfree(manager);
	return 0;
}

static int hf_manager_find_client(struct hf_manager_event *event)
{
	int err = 0;
	unsigned long flags;
	struct hf_client *client = NULL;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		/* must (err |=), collect all err to decide retry? */
		if (READ_ONCE(client->request[event->sensor_id].action) ==
				HF_MANAGER_SENSOR_ENABLE)
			err |= hf_manager_report_event(client, event);
	}
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

	return err;
}

static struct hf_manager *hf_manager_find_manager(uint8_t sensor_id)
{
	int i = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;

	list_for_each_entry(manager, &hf_manager_list, list) {
		device = READ_ONCE(manager->hf_dev);
		if (!device || !device->support_list)
			continue;
		for (i = 0; i < device->support_size; ++i) {
			if (sensor_id == device->support_list[i])
				return manager;
		}
	}
	return NULL;
}

static void hf_manager_update_client_param(
		struct hf_client *client, struct hf_manager_cmd *cmd)
{
	/* only enable disable update action delay and latency */
	if (cmd->action == HF_MANAGER_SENSOR_ENABLE) {
		client->request[cmd->sensor_id].action = cmd->action;
		client->request[cmd->sensor_id].delay = cmd->delay;
		client->request[cmd->sensor_id].latency = cmd->latency;
		client->request[cmd->sensor_id].start_time = sched_clock();
	} else if (cmd->action == HF_MANAGER_SENSOR_DISABLE) {
		client->request[cmd->sensor_id].action = cmd->action;
		client->request[cmd->sensor_id].delay = S64_MAX;
		client->request[cmd->sensor_id].latency = S64_MAX;
		client->request[cmd->sensor_id].start_time = S64_MAX;
	}
}

static void hf_manager_find_best_param(uint8_t sensor_id,
		uint8_t *action, int64_t *delay, int64_t *latency)
{
	unsigned long flags;
	struct hf_client *client = NULL;
	struct sensor_state *request = NULL;
	uint8_t tmp_action = HF_MANAGER_SENSOR_DISABLE;
	int64_t tmp_delay = S64_MAX;
	int64_t tmp_latency = S64_MAX;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		request = &client->request[sensor_id];
		if (request->action == HF_MANAGER_SENSOR_ENABLE) {
			tmp_action = HF_MANAGER_SENSOR_ENABLE;
			if (request->delay < tmp_delay)
				tmp_delay = request->delay;
			if (request->latency < tmp_latency)
				tmp_latency = request->latency;
		}
	}
	spin_unlock_irqrestore(&hf_client_list_lock, flags);
	*action = tmp_action;
	*delay = tmp_delay;
	*latency = tmp_latency;

	if (tmp_action == HF_MANAGER_SENSOR_ENABLE)
		pr_notice("%s: %d,%d,%lld,%lld\n", __func__,
			sensor_id, tmp_action, tmp_delay, tmp_latency);
	else if (tmp_action == HF_MANAGER_SENSOR_DISABLE)
		pr_notice("%s: %d,%d\n", __func__, sensor_id, tmp_action);
}

static bool device_rebatch(uint8_t sensor_id,
			int64_t best_delay, int64_t best_latency)
{
	if (prev_request[sensor_id].delay != best_delay ||
			prev_request[sensor_id].latency != best_latency) {
		prev_request[sensor_id].delay = best_delay;
		prev_request[sensor_id].latency = best_latency;
		return true;
	}
	return false;
}

static bool device_reenable(uint8_t sensor_id, uint8_t best_action)
{
	if (prev_request[sensor_id].action != best_action) {
		prev_request[sensor_id].action = best_action;
		return true;
	}
	return false;
}

static bool device_redisable(uint8_t sensor_id, uint8_t best_action,
			int64_t best_delay, int64_t best_latency)
{
	if (prev_request[sensor_id].action != best_action) {
		prev_request[sensor_id].action = best_action;
		prev_request[sensor_id].delay = best_delay;
		prev_request[sensor_id].latency = best_latency;
		return true;
	}
	return false;
}

static int hf_manager_device_enable(struct hf_device *device,
				uint8_t sensor_id)
{
	int err = 0;
	struct hf_manager *manager = device->manager;
	uint8_t best_action = HF_MANAGER_SENSOR_DISABLE;
	int64_t best_delay = S64_MAX;
	int64_t best_latency = S64_MAX;

	if (!device->enable || !device->batch)
		return -EINVAL;

	hf_manager_find_best_param(sensor_id, &best_action,
		&best_delay, &best_latency);

	if (best_action == HF_MANAGER_SENSOR_ENABLE) {
		if (device_rebatch(sensor_id, best_delay, best_latency))
			err = device->batch(device, sensor_id,
				best_delay, best_latency);
		if (device_reenable(sensor_id, best_action))
			err = device->enable(device, sensor_id, best_action);
		/* must update io_enabled before hrtimer_start */
		manager->io_enabled = true;
		if (device->device_poll == HF_DEVICE_IO_POLLING &&
				manager->io_poll_interval.tv64 != best_delay) {
			manager->io_poll_interval.tv64 = best_delay;
			hrtimer_start(&manager->io_poll_timer,
				manager->io_poll_interval, HRTIMER_MODE_REL);
		}
	} else if (best_action == HF_MANAGER_SENSOR_DISABLE) {
		if (device_redisable(sensor_id, best_action,
				best_delay, best_latency))
			err = device->enable(device, sensor_id, best_action);
		manager->io_enabled = false;
		if (device->device_poll == HF_DEVICE_IO_POLLING) {
			manager->io_poll_interval.tv64 = best_delay;
			hrtimer_cancel(&manager->io_poll_timer);
			if (device->device_bus == HF_DEVICE_IO_ASYNC)
				tasklet_kill(&manager->io_work_tasklet);
		}
	}
	return err;
}

static int hf_manager_device_flush(struct hf_device *device,
		uint8_t sensor_id)
{
	if (!device->flush)
		return -EINVAL;

	return device->flush(device, sensor_id);
}

static int hf_manager_device_calibration(struct hf_device *device,
		uint8_t sensor_id)
{
	if (device->calibration)
		return device->calibration(device, sensor_id);
	return 0;
}

static int hf_manager_device_config_cali(struct hf_device *device,
		uint8_t sensor_id)
{
	if (device->config_cali)
		return device->config_cali(device, sensor_id);
	return 0;
}

static int hf_manager_device_selftest(struct hf_device *device,
		uint8_t sensor_id)
{
	if (device->selftest)
		return device->selftest(device, sensor_id);
	return 0;
}

static int hf_manager_drive_device(struct hf_client *client,
		struct hf_manager_cmd *cmd)
{
	int err = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;
	uint8_t sensor_id = cmd->sensor_id;

	if (unlikely(sensor_id >= HIGH_FREQUENCY_SENSOR_MAX))
		return -EINVAL;

	mutex_lock(&hf_manager_list_mtx);
	manager = hf_manager_find_manager(sensor_id);
	if (!manager) {
		pr_err("%s: no manager finded\n");
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}
	device = manager->hf_dev;
	if (!device || !device->dev_name) {
		pr_err("%s: no hf device or important param finded\n");
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}

	pr_notice("%s: %s: %d,%d,%lld,%lld\n", __func__, device->dev_name,
		cmd->sensor_id, cmd->action, cmd->delay, cmd->latency);

	switch (cmd->action) {
	case HF_MANAGER_SENSOR_ENABLE:
	case HF_MANAGER_SENSOR_DISABLE:
		hf_manager_update_client_param(client, cmd);
		err = hf_manager_device_enable(device, sensor_id);
		break;
	case HF_MANAGER_SENSOR_FLUSH:
		err = hf_manager_device_flush(device, sensor_id);
		break;
	case HF_MANAGER_SENSOR_CALIBRATION:
		err = hf_manager_device_calibration(device, sensor_id);
		break;
	case HF_MANAGER_SENSOR_CONFIG_CALI:
		err = hf_manager_device_config_cali(device, sensor_id);
		break;
	case HF_MANAGER_SENSOR_SELFTEST:
		err = hf_manager_device_selftest(device, sensor_id);
		break;
	}
	mutex_unlock(&hf_manager_list_mtx);
	return err;
}

static int hf_manager_open(struct inode *inode, struct file *filp)
{
	int err = 0;
	unsigned long flags;
	struct hf_client *client = NULL;
	struct hf_client_fifo *hf_fifo = NULL;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client) {
		err = -ENOMEM;
		goto err_out;
	}

	/* record process id and thread id for debug */
	strlcpy(client->proc_comm, current->comm, sizeof(client->proc_comm));
	client->leader_pid = current->group_leader->pid;
	client->pid = current->pid;

	pr_notice("%s: [%s][%d:%d]\n", __func__, current->comm,
		current->group_leader->pid, current->pid);

	INIT_LIST_HEAD(&client->list);

	hf_fifo = &client->hf_fifo;
	hf_fifo->head = 0;
	hf_fifo->tail = 0;
	hf_fifo->bufsize = roundup_pow_of_two(HF_MANAGER_FIFO_SIZE);
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

	filp->private_data = client;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_add(&client->list, &hf_client_list);
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

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
	struct hf_client *client = filp->private_data;

	pr_notice("%s: [%s][%d:%d]\n", __func__, current->comm,
		current->group_leader->pid, current->pid);

	filp->private_data = NULL;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_del(&client->list);
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

	kfree(client->hf_fifo.buffer);
	kfree(client);
	return 0;
}

static int hf_manager_fetch_next(struct hf_client_fifo *hf_fifo,
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
	struct hf_client *client = filp->private_data;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;
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

static ssize_t hf_manager_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *f_pos)
{
	int err = 0;
	struct hf_manager_cmd cmd;
	struct hf_client *client = filp->private_data;

	memset(&cmd, 0, sizeof(struct hf_manager_cmd));

	if (count != sizeof(struct hf_manager_cmd))
		return -EFAULT;

	if (copy_from_user(&cmd, buf, count))
		return -EFAULT;

	err = hf_manager_drive_device(client, &cmd);
	return err;
}

static unsigned int hf_manager_poll(struct file *filp,
		struct poll_table_struct *wait)
{
	struct hf_client *client = filp->private_data;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;
	unsigned int mask = 0;

	poll_wait(filp, &hf_fifo->wait, wait);

	if (hf_fifo->head != hf_fifo->tail)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static long hf_manager_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	unsigned int sensor_id = 0, result = 0;

	switch (cmd) {
	case HF_MANAGER_SENSOR_REGISTER_STATUS:
		if (size != sizeof(unsigned int))
			return -EINVAL;
		if (copy_from_user(&sensor_id, ubuf, sizeof(sensor_id)))
			return -EFAULT;
		if (unlikely(sensor_id >= HIGH_FREQUENCY_SENSOR_MAX))
			return -EINVAL;
		result = test_bit(sensor_id, sensor_list_bitmap);
		if (copy_to_user(ubuf, &result, sizeof(result)))
			return -EFAULT;
		break;
	}
	return 0;
}

static ssize_t client_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

DEVICE_ATTR(client_info, 0644, client_info_show, NULL);

static struct attribute *hf_manager_attrs[] = {
	&dev_attr_client_info.attr,
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
	.unlocked_ioctl = hf_manager_ioctl,
	.compat_ioctl   = hf_manager_ioctl,
};

static int hf_manager_proc_show(struct seq_file *m, void *v)
{
	int i = 0, sensor_id = 0;
	unsigned long flags;
	struct hf_manager *manager = NULL;
	struct hf_client *client = NULL;
	struct hf_device *device = NULL;

	mutex_lock(&hf_manager_list_mtx);
	list_for_each_entry(manager, &hf_manager_list, list) {
		device = READ_ONCE(manager->hf_dev);
		if (!device || !device->support_list)
			continue;
		seq_printf(m, "manager: param:[%d,%lld]\n",
			manager->io_enabled,
			manager->io_poll_interval);
		seq_printf(m, "device:%s poll:%s bus:%s online\n",
			device->dev_name,
			device->device_poll ? "io_polling" : "io_interrupt",
			device->device_bus ? "io_async" : "io_sync");
		for (i = 0; i < device->support_size; ++i) {
			sensor_id = device->support_list[i];
			seq_printf(m, "support:%d now param:[%d,%lld,%lld]\n",
				sensor_id,
				prev_request[sensor_id].action,
				prev_request[sensor_id].delay,
				prev_request[sensor_id].latency);
		}
	}
	mutex_unlock(&hf_manager_list_mtx);

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		seq_printf(m, "client:%s pid:[%d:%d] online\n",
			client->proc_comm,
			client->leader_pid,
			client->pid);
		for (i = 0; i < HIGH_FREQUENCY_SENSOR_MAX; ++i) {
			if (client->request[i].action ==
				HF_MANAGER_SENSOR_DISABLE)
				continue;
			seq_printf(m, "request:%d param:[%d,%lld,%lld,%lld]\n",
				i,
				client->request[i].action,
				client->request[i].delay,
				client->request[i].latency,
				client->request[i].start_time);
		}
	}
	spin_unlock_irqrestore(&hf_client_list_lock, flags);
	return 0;
}

static int hf_manager_proc_open(struct inode *inode,
		struct file *filp)
{
	return single_open(filp, hf_manager_proc_show, NULL);
}

static const struct file_operations hf_manager_proc_fops = {
	.open           = hf_manager_proc_open,
	.release        = single_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
};


static int __init hf_manager_init(void)
{
	int major = -1, i = 0;
	struct class *hf_manager_class;
	struct device *dev;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };

	for (i = 0; i < HIGH_FREQUENCY_SENSOR_MAX; ++i) {
		prev_request[i].action = HF_MANAGER_SENSOR_DISABLE;
		prev_request[i].delay = S64_MAX;
		prev_request[i].latency = S64_MAX;
		prev_request[i].start_time = S64_MAX;
	}

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

	proc_create("hf_manager", 0644, NULL, &hf_manager_proc_fops);

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

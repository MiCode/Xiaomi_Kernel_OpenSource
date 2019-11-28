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

#define pr_fmt(fmt) "[hf_manager] " fmt

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
#include <uapi/linux/sched/types.h>
#include <linux/sched_clock.h>
#include <linux/log2.h>

#include "hf_manager.h"

static DECLARE_BITMAP(sensor_list_bitmap, SENSOR_TYPE_SENSOR_MAX);
static LIST_HEAD(hf_manager_list);
static DEFINE_MUTEX(hf_manager_list_mtx);
static LIST_HEAD(hf_client_list);
static DEFINE_SPINLOCK(hf_client_list_lock);
static struct sensor_state prev_request[SENSOR_TYPE_SENSOR_MAX];

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

	if (direction >= ARRAY_SIZE(coordinates))
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
	if (hf_fifo->last_time_stamp[event->sensor_type] ==
			event->timestamp) {
		return true;
	}
	hf_fifo->last_time_stamp[event->sensor_type] = event->timestamp;
	return false;
}

static int hf_manager_report_event(struct hf_client *client,
		struct hf_manager_event *event)
{
	unsigned long flags;
	unsigned int next = 0;
	int64_t hang_time = 0;
	const int64_t max_hang_time = 1000000000LL;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;

	spin_lock_irqsave(&hf_fifo->buffer_lock, flags);
	if (unlikely(hf_fifo->buffull == true)) {
		hang_time = ktime_get_boot_ns() - hf_fifo->hang_begin;
		if (hang_time >= max_hang_time) {
			/* reset buffer */
			hf_fifo->buffull = false;
			hf_fifo->head = 0;
			hf_fifo->tail = 0;
			pr_err_ratelimited(
				"%s [%s][%d:%d] hang(%lld) to reset buffer\n",
				__func__, client->proc_comm,
				client->leader_pid, client->pid, hang_time);
		} else {
			pr_err_ratelimited(
				"%s [%s][%d:%d] buffer full, [%d,%lld]\n",
				__func__, client->proc_comm,
				client->leader_pid, client->pid,
				event->sensor_type, event->timestamp);
			spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
			wake_up_interruptible(&hf_fifo->wait);
			/*
			 * must return -1 when buffer full, tell caller retry
			 * send data some times later.
			 */
			return -1;
		}
	}
	/* only data action run filter event */
	if (likely(event->action == DATA_ACTION) &&
			unlikely(filter_event_by_timestamp(hf_fifo, event))) {
		pr_err_ratelimited("%s [%s][%d:%d] filterd, [%d,%lld]\n",
			__func__, client->proc_comm, client->leader_pid,
			client->pid, event->sensor_type, event->timestamp);
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
	if (unlikely(next == hf_fifo->tail)) {
		hf_fifo->buffull = true;
		hf_fifo->hang_begin = ktime_get_boot_ns();
	}
	spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);

	wake_up_interruptible(&hf_fifo->wait);
	return 0;
}

static void hf_manager_io_schedule(struct hf_manager *manager,
		int64_t timestamp)
{
	if (!atomic_read(&manager->io_enabled))
		return;
	set_interrupt_timestamp(manager, timestamp);
	if (READ_ONCE(manager->hf_dev->device_bus) == HF_DEVICE_IO_ASYNC)
		tasklet_schedule(&manager->io_work_tasklet);
	else if (READ_ONCE(manager->hf_dev->device_bus) == HF_DEVICE_IO_SYNC)
		kthread_queue_work(&hf_manager_kthread_worker,
			&manager->io_kthread_work);
}

static int hf_manager_io_report(struct hf_manager *manager,
		struct hf_manager_event *event)
{
	/* must return 0 when sensor_type exceed and no need to retry */
	if (unlikely(event->sensor_type >= SENSOR_TYPE_SENSOR_MAX)) {
		pr_err_ratelimited("%s %d exceed max sensor id\n", __func__,
			event->sensor_type);
		return 0;
	}
	return hf_manager_find_client(event);
}

static void hf_manager_io_complete(struct hf_manager *manager)
{
	clear_bit(HF_MANAGER_IO_IN_PROGRESS, &(manager->flags));
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

static void hf_manager_sched_sample(struct hf_manager *manager,
		int64_t timestamp)
{
	hf_manager_io_schedule(manager, timestamp);
}

static enum hrtimer_restart hf_manager_io_poll(struct hrtimer *timer)
{
	struct hf_manager *manager =
		(struct hf_manager *)container_of(timer,
			struct hf_manager, io_poll_timer);

	hf_manager_sched_sample(manager, ktime_get_boot_ns());
	hrtimer_forward_now(&manager->io_poll_timer,
		ns_to_ktime(atomic64_read(&manager->io_poll_interval)));
	return HRTIMER_RESTART;
}

static void hf_manager_io_interrupt(struct hf_manager *manager,
		int64_t timestamp)
{
	hf_manager_sched_sample(manager, timestamp);
}

int hf_manager_create(struct hf_device *device)
{
	unsigned char sensor_type = 0;
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

	atomic_set(&manager->io_enabled, 0);
	atomic64_set(&manager->io_poll_interval, S64_MAX);

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
		sensor_type = device->support_list[i];
		if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX)) {
			pr_err("%s %s %d exceed max sensor id\n", __func__,
				device->dev_name, sensor_type);
			err = -EINVAL;
			goto out_err;
		}
		if (test_and_set_bit(sensor_type, sensor_list_bitmap)) {
			pr_err("%s %s %d repeat\n", __func__,
				device->dev_name, sensor_type);
			err = -EBUSY;
			goto out_err;
		}
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

static int hf_manager_distinguish_event(struct hf_client *client,
		struct hf_manager_event *event)
{
	int err = 0;
	unsigned long flags;
	struct sensor_state *request = &client->request[event->sensor_type];

	switch (event->action) {
	case DATA_ACTION:
		/* must relay on enable status client requested */
		if (READ_ONCE(request->enable) &&
				(event->timestamp >
					atomic64_read(&request->start_time)))
			err = hf_manager_report_event(client, event);
		break;
	case FLUSH_ACTION:
		/*
		 * flush relay on flush count client requested,
		 * must not relay on enable status.
		 * flush may report both by looper thread and disable thread.
		 * spinlock prevent flush count report more than request.
		 * sequence:
		 * flush = 1
		 * looper thread flush > 0
		 *    looper thread hf_manager_report_event
		 *        disable thread flush > 0
		 *            disable thread hf_manager_report_event
		 * flush complete report 2 times but request is 1.
		 */
		spin_lock_irqsave(&client->request_lock, flags);
		if (atomic_read(&request->flush) > 0) {
			err = hf_manager_report_event(client, event);
			/* return < 0, don't decrease flush count */
			if (err < 0) {
				spin_unlock_irqrestore(&client->request_lock,
					flags);
				return err;
			}
			atomic_dec_if_positive(&request->flush);
		}
		spin_unlock_irqrestore(&client->request_lock, flags);
		break;
	case BIAS_ACTION:
		/* relay on status client requested, don't check return */
		if (READ_ONCE(request->bias))
			hf_manager_report_event(client, event);
		break;
	case CALI_ACTION:
		/* cali on status client requested, don't check return */
		if (READ_ONCE(request->cali))
			hf_manager_report_event(client, event);
		break;
	case TEMP_ACTION:
		/* temp on status  client requested, don't check return */
		if (READ_ONCE(request->temp))
			hf_manager_report_event(client, event);
		break;
	case TEST_ACTION:
		/* test on status client requested, don't check return */
		if (READ_ONCE(request->test))
			hf_manager_report_event(client, event);
		break;
	case RAW_ACTION:
		/* raw on status client requested, don't check return */
		if (READ_ONCE(request->raw))
			hf_manager_report_event(client, event);
		break;
	}
	return err;
}

static int hf_manager_find_client(struct hf_manager_event *event)
{
	int err = 0;
	unsigned long flags;
	struct hf_client *client = NULL;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		/* must (err |=), collect all err to decide retry */
		err |= hf_manager_distinguish_event(client, event);
	}
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

	return err;
}

static struct hf_manager *hf_manager_find_manager(uint8_t sensor_type)
{
	int i = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;

	list_for_each_entry(manager, &hf_manager_list, list) {
		device = READ_ONCE(manager->hf_dev);
		if (!device || !device->support_list)
			continue;
		for (i = 0; i < device->support_size; ++i) {
			if (sensor_type == device->support_list[i])
				return manager;
		}
	}
	return NULL;
}

static void hf_manager_update_client_param(
		struct hf_client *client, struct hf_manager_cmd *cmd)
{
	struct sensor_state *request = &client->request[cmd->sensor_type];

	/* only enable disable update action delay and latency */
	if (cmd->action == HF_MANAGER_SENSOR_ENABLE) {
		if (!request->enable)
			atomic64_set(&request->start_time,
				ktime_get_boot_ns());
		request->enable = true;
		request->delay = cmd->delay;
		request->latency = cmd->latency;
	} else if (cmd->action == HF_MANAGER_SENSOR_DISABLE) {
		atomic64_set(&request->start_time, S64_MAX);
		request->enable = false;
		request->delay = S64_MAX;
		request->latency = S64_MAX;
	}
}

static void hf_manager_find_best_param(uint8_t sensor_type,
		bool *action, int64_t *delay, int64_t *latency)
{
	unsigned long flags;
	struct hf_client *client = NULL;
	struct sensor_state *request = NULL;
	bool tmp_enable = false;
	int64_t tmp_delay = S64_MAX;
	int64_t tmp_latency = S64_MAX;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		request = &client->request[sensor_type];
		if (request->enable) {
			tmp_enable = true;
			if (request->delay < tmp_delay)
				tmp_delay = request->delay;
			if (request->latency < tmp_latency)
				tmp_latency = request->latency;
		}
	}
	spin_unlock_irqrestore(&hf_client_list_lock, flags);
	*action = tmp_enable;
	*delay = tmp_delay;
	*latency = tmp_latency;

#ifdef HF_MANAGER_DEBUG
	if (tmp_enable)
		pr_notice("%s: %d,%d,%lld,%lld\n", __func__,
			sensor_type, tmp_enable, tmp_delay, tmp_latency);
	else
		pr_notice("%s: %d,%d\n", __func__, sensor_type, tmp_enable);
#endif
}

static bool device_rebatch(uint8_t sensor_type,
			int64_t best_delay, int64_t best_latency)
{
	if (prev_request[sensor_type].delay != best_delay ||
			prev_request[sensor_type].latency != best_latency) {
		prev_request[sensor_type].delay = best_delay;
		prev_request[sensor_type].latency = best_latency;
		return true;
	}
	return false;
}

static bool device_reenable(uint8_t sensor_type, bool best_enable)
{
	if (prev_request[sensor_type].enable != best_enable) {
		prev_request[sensor_type].enable = best_enable;
		return true;
	}
	return false;
}

static bool device_redisable(uint8_t sensor_type, bool best_enable,
			int64_t best_delay, int64_t best_latency)
{
	if (prev_request[sensor_type].enable != best_enable) {
		prev_request[sensor_type].enable = best_enable;
		prev_request[sensor_type].delay = best_delay;
		prev_request[sensor_type].latency = best_latency;
		return true;
	}
	return false;
}

static int64_t device_poll_min_interval(struct hf_device *device)
{
	int i = 0, j = 0;
	int64_t interval = S64_MAX;

	for (i = 0; i < device->support_size; ++i) {
		j = device->support_list[i];
		if (prev_request[j].enable) {
			if (prev_request[j].delay < interval)
				interval = prev_request[j].delay;
		}
	}
	return interval;
}

static void device_poll_trigger(struct hf_device *device, bool enable)
{
	int64_t min_interval = S64_MAX;
	struct hf_manager *manager = device->manager;

	BUG_ON(enable && !atomic_read(&manager->io_enabled));
	min_interval = device_poll_min_interval(device);
	BUG_ON(atomic_read(&manager->io_enabled) && min_interval == S64_MAX);
	if (atomic64_read(&manager->io_poll_interval) == min_interval)
		return;
	atomic64_set(&manager->io_poll_interval, min_interval);
	if (atomic_read(&manager->io_enabled))
		hrtimer_start(&manager->io_poll_timer,
			ns_to_ktime(min_interval), HRTIMER_MODE_REL);
	else
		hrtimer_cancel(&manager->io_poll_timer);
}

static int hf_manager_device_enable(struct hf_device *device,
				uint8_t sensor_type)
{
	int err = 0;
	struct hf_manager *manager = device->manager;
	bool best_enable = false;
	int64_t best_delay = S64_MAX;
	int64_t best_latency = S64_MAX;

	if (!device->enable || !device->batch)
		return -EINVAL;

	hf_manager_find_best_param(sensor_type, &best_enable,
		&best_delay, &best_latency);

	if (best_enable) {
		if (device_rebatch(sensor_type, best_delay, best_latency))
			err = device->batch(device, sensor_type,
				best_delay, best_latency);
		if (device_reenable(sensor_type, best_enable)) {
			err = device->enable(device, sensor_type, best_enable);
			/* must update io_enabled before hrtimer_start */
			atomic_inc(&manager->io_enabled);
		}
		if (device->device_poll == HF_DEVICE_IO_POLLING)
			device_poll_trigger(device, best_enable);
	} else {
		if (device_redisable(sensor_type, best_enable,
				best_delay, best_latency)) {
			err = device->enable(device, sensor_type, best_enable);
			atomic_dec_if_positive(&manager->io_enabled);
		}
		if (device->device_poll == HF_DEVICE_IO_POLLING)
			device_poll_trigger(device, best_enable);
		if (device->device_bus == HF_DEVICE_IO_ASYNC &&
				!atomic_read(&manager->io_enabled))
			tasklet_kill(&manager->io_work_tasklet);
	}
	return err;
}

static int hf_manager_device_flush(struct hf_device *device,
		uint8_t sensor_type)
{
	if (!device->flush)
		return -EINVAL;

	return device->flush(device, sensor_type);
}

static int hf_manager_device_calibration(struct hf_device *device,
		uint8_t sensor_type)
{
	if (device->calibration)
		return device->calibration(device, sensor_type);
	return 0;
}

static int hf_manager_device_config_cali(struct hf_device *device,
		uint8_t sensor_type, int32_t *data)
{
	if (device->config_cali)
		return device->config_cali(device, sensor_type, data);
	return 0;
}

static int hf_manager_device_selftest(struct hf_device *device,
		uint8_t sensor_type)
{
	if (device->selftest)
		return device->selftest(device, sensor_type);
	return 0;
}

static int hf_manager_device_rawdata(struct hf_device *device,
		uint8_t sensor_type)
{
	unsigned long flags;
	struct hf_client *client = NULL;
	struct sensor_state *request = NULL;
	bool best_enable = false;

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		request = &client->request[sensor_type];
		if (request->raw)
			best_enable = true;
	}
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

	if (prev_request[sensor_type].raw == best_enable)
		return 0;
	prev_request[sensor_type].raw = best_enable;
	if (device->rawdata)
		return device->rawdata(device, sensor_type, best_enable);
	return 0;
}

static int hf_manager_drive_device(struct hf_client *client,
		struct hf_manager_cmd *cmd)
{
	int err = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;
	uint8_t sensor_type = cmd->sensor_type;

	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;

	mutex_lock(&hf_manager_list_mtx);
	manager = hf_manager_find_manager(sensor_type);
	if (!manager) {
		pr_err("%s: no manager finded\n", __func__);
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}
	device = manager->hf_dev;
	if (!device || !device->dev_name) {
		pr_err("%s: no hf device or important param finded\n",
			__func__);
		mutex_unlock(&hf_manager_list_mtx);
		return -EINVAL;
	}

#ifdef HF_MANAGER_DEBUG
	pr_notice("%s: %s: %d,%d,%lld,%lld\n", __func__, device->dev_name,
		cmd->sensor_type, cmd->action, cmd->delay, cmd->latency);
#endif

	switch (cmd->action) {
	case HF_MANAGER_SENSOR_ENABLE:
	case HF_MANAGER_SENSOR_DISABLE:
		hf_manager_update_client_param(client, cmd);
		err = hf_manager_device_enable(device, sensor_type);
		break;
	case HF_MANAGER_SENSOR_FLUSH:
		atomic_inc(&client->request[sensor_type].flush);
		err = hf_manager_device_flush(device, sensor_type);
		if (err < 0)
			atomic_dec_if_positive(
				&client->request[sensor_type].flush);
		break;
	case HF_MANAGER_SENSOR_ENABLE_CALI:
		err = hf_manager_device_calibration(device, sensor_type);
		break;
	case HF_MANAGER_SENSOR_CONFIG_CALI:
		err = hf_manager_device_config_cali(device,
			sensor_type, cmd->data);
		break;
	case HF_MANAGER_SENSOR_SELFTEST:
		err = hf_manager_device_selftest(device, sensor_type);
		break;
	case HF_MANAGER_SENSOR_RAWDATA:
		client->request[sensor_type].raw =
			cmd->data[0] ? true : false;
		err = hf_manager_device_rawdata(device, sensor_type);
		break;
	}
	mutex_unlock(&hf_manager_list_mtx);
	return err;
}

struct hf_client *hf_client_create(void)
{
	unsigned long flags;
	struct hf_client *client = NULL;
	struct hf_client_fifo *hf_fifo = NULL;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		goto err_out;

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
	hf_fifo->bufsize = roundup_pow_of_two(HF_CLIENT_FIFO_SIZE);
	hf_fifo->buffull = false;
	spin_lock_init(&hf_fifo->buffer_lock);
	init_waitqueue_head(&hf_fifo->wait);
	hf_fifo->buffer =
		kcalloc(hf_fifo->bufsize, sizeof(*hf_fifo->buffer),
			GFP_KERNEL);
	if (!hf_fifo->buffer)
		goto err_free;

	spin_lock_init(&client->request_lock);

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_add(&client->list, &hf_client_list);
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

	return client;
err_free:
	kfree(client);
err_out:
	return NULL;
}

void hf_client_destroy(struct hf_client *client)
{
	unsigned long flags;

	pr_notice("%s: [%s][%d:%d]\n", __func__, current->comm,
		current->group_leader->pid, current->pid);

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_del(&client->list);
	spin_unlock_irqrestore(&hf_client_list_lock, flags);

	kfree(client->hf_fifo.buffer);
	kfree(client);
}

bool hf_client_find_sensor(struct hf_client *client, uint8_t sensor_type)
{
	return test_bit(sensor_type, sensor_list_bitmap);
}

int hf_client_control_sensor(struct hf_client *client,
		struct hf_manager_cmd *cmd)
{
	return hf_manager_drive_device(client, cmd);
}

static int fetch_next(struct hf_client_fifo *hf_fifo,
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

int hf_client_poll_sensor(struct hf_client *client,
		struct hf_manager_event *data, int count)
{
	int read = 0;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;

	wait_event_interruptible(hf_fifo->wait,
		hf_fifo->head != hf_fifo->tail);

	for (;;) {
		if (hf_fifo->head == hf_fifo->tail)
			return 0;
		if (count == 0)
			break;
		while (read <= count &&
			fetch_next(hf_fifo, &data[read])) {
			read++;
		}
		if (read)
			break;
	}
	return read;
}

static int hf_manager_open(struct inode *inode, struct file *filp)
{
	struct hf_client *client = hf_client_create();

	if (!client)
		return -ENOMEM;

	filp->private_data = client;
	nonseekable_open(inode, filp);
	return 0;
}

static int hf_manager_release(struct inode *inode, struct file *filp)
{
	struct hf_client *client = filp->private_data;

	filp->private_data = NULL;
	hf_client_destroy(client);
	return 0;
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
			fetch_next(hf_fifo, &event)) {
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
	struct hf_manager_cmd cmd;
	struct hf_client *client = filp->private_data;

	memset(&cmd, 0, sizeof(struct hf_manager_cmd));

	if (count != sizeof(struct hf_manager_cmd))
		return -EFAULT;

	if (copy_from_user(&cmd, buf, count))
		return -EFAULT;

	return hf_manager_drive_device(client, &cmd);
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
	struct hf_client *client = filp->private_data;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;
	unsigned int sensor_type = 0;
	struct ioctl_packet packet;

	memset(&packet, 0, sizeof(struct ioctl_packet));

	if (size != sizeof(struct ioctl_packet))
		return -EINVAL;
	if (copy_from_user(&packet, ubuf, sizeof(struct ioctl_packet)))
		return -EFAULT;
	sensor_type = packet.sensor_type;
	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;

	switch (cmd) {
	case HF_MANAGER_REQUEST_REGISTER_STATUS:
		packet.status = test_bit(sensor_type, sensor_list_bitmap);
		if (copy_to_user(ubuf, &packet, sizeof(struct ioctl_packet)))
			return -EFAULT;
		break;
	case HF_MANAGER_REQUEST_BIAS_DATA:
		client->request[sensor_type].bias = packet.status;
		break;
	case HF_MANAGER_REQUEST_CALI_DATA:
		client->request[sensor_type].cali = packet.status;
		break;
	case HF_MANAGER_REQUEST_TEMP_DATA:
		client->request[sensor_type].temp = packet.status;
		break;
	case HF_MANAGER_REQUEST_TEST_DATA:
		client->request[sensor_type].test = packet.status;
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
	int i = 0, sensor_type = 0;
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
			atomic_read(&manager->io_enabled),
			(int64_t)atomic64_read(&manager->io_poll_interval));
		seq_printf(m, "device:%s poll:%s bus:%s online\n",
			device->dev_name,
			device->device_poll ? "io_polling" : "io_interrupt",
			device->device_bus ? "io_async" : "io_sync");
		for (i = 0; i < device->support_size; ++i) {
			sensor_type = device->support_list[i];
			seq_printf(m, "support:%d now param:[%d,%lld,%lld]\n",
				sensor_type,
				prev_request[sensor_type].enable,
				prev_request[sensor_type].delay,
				prev_request[sensor_type].latency);
		}
	}
	mutex_unlock(&hf_manager_list_mtx);

	spin_lock_irqsave(&hf_client_list_lock, flags);
	list_for_each_entry(client, &hf_client_list, list) {
		seq_printf(m, "client:%s pid:[%d:%d] online\n",
			client->proc_comm,
			client->leader_pid,
			client->pid);
		for (i = 0; i < SENSOR_TYPE_SENSOR_MAX; ++i) {
			if (!client->request[i].enable)
				continue;
			seq_printf(m, "request:%d param:[%d,%lld,%lld,%lld]\n",
				i,
				client->request[i].enable,
				client->request[i].delay,
				client->request[i].latency,
				(int64_t)atomic64_read(
					&client->request[i].start_time));
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

	for (i = 0; i < SENSOR_TYPE_SENSOR_MAX; ++i) {
		prev_request[i].delay = S64_MAX;
		prev_request[i].latency = S64_MAX;
		atomic64_set(&prev_request[i].start_time, S64_MAX);
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

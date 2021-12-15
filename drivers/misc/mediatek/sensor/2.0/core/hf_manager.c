// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "[hf_manager]" fmt

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


static int major;
static struct class *hf_manager_class;
static struct task_struct *task;

struct coordinate {
	int8_t sign[3];
	uint8_t map[3];
};

static const struct coordinate coordinates[] = {
	{ { 1, 1, 1}, {0, 1, 2} },
	{ { -1, 1, 1}, {1, 0, 2} },
	{ { -1, -1, 1}, {0, 1, 2} },
	{ { 1, -1, 1}, {1, 0, 2} },

	{ { -1, 1, -1}, {0, 1, 2} },
	{ { 1, 1, -1}, {1, 0, 2} },
	{ { 1, -1, -1}, {0, 1, 2} },
	{ { -1, -1, -1}, {1, 0, 2} },
};

static DECLARE_BITMAP(sensor_list_bitmap, SENSOR_TYPE_SENSOR_MAX);
static struct hf_core hfcore;

#define print_s64(l) (((l) == S64_MAX) ? -1 : (l))
static int hf_manager_find_client(struct hf_core *core,
		struct hf_manager_event *event);

static void init_hf_core(struct hf_core *core)
{
	int i = 0;

	mutex_init(&core->manager_lock);
	INIT_LIST_HEAD(&core->manager_list);
	for (i = 0; i < SENSOR_TYPE_SENSOR_MAX; ++i) {
		core->state[i].delay = S64_MAX;
		core->state[i].latency = S64_MAX;
		atomic64_set(&core->state[i].start_time, S64_MAX);
	}

	spin_lock_init(&core->client_lock);
	INIT_LIST_HEAD(&core->client_list);

	mutex_init(&core->device_lock);
	INIT_LIST_HEAD(&core->device_list);

	kthread_init_worker(&core->kworker);
}

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
EXPORT_SYMBOL_GPL(coordinate_map);

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
			pr_err_ratelimited("[%s][%d:%d] buffer reset %lld\n",
				client->proc_comm, client->leader_pid,
				client->ppid, hang_time);
		} else {
			pr_err_ratelimited("[%s][%d:%d] buffer full %d %lld\n",
				client->proc_comm, client->leader_pid,
				client->ppid, event->sensor_type,
				event->timestamp);
			spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
			wake_up_interruptible(&hf_fifo->wait);
			/*
			 * must return -EAGAIN when buffer full,
			 * tell caller retry send data some times later.
			 */
			return -EAGAIN;
		}
	}
	/* only data action run filter event */
	if (likely(event->action == DATA_ACTION) &&
			unlikely(filter_event_by_timestamp(hf_fifo, event))) {
		pr_err_ratelimited("[%s][%d:%d] buffer filter %d %lld\n",
			client->proc_comm, client->leader_pid,
			client->ppid, event->sensor_type, event->timestamp);
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
		if (hf_fifo->hang_begin > hf_fifo->client_active) {
			hang_time = hf_fifo->hang_begin -
				hf_fifo->client_active;
			if (hang_time < max_hang_time)
				hf_fifo->hang_begin = ktime_get_boot_ns();
		} else {
			hf_fifo->hang_begin = ktime_get_boot_ns();
		}
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
		kthread_queue_work(&manager->core->kworker,
			&manager->io_kthread_work);
}

static int hf_manager_io_report(struct hf_manager *manager,
		struct hf_manager_event *event)
{
	/* must return 0 when sensor_type exceed and no need to retry */
	if (unlikely(event->sensor_type >= SENSOR_TYPE_SENSOR_MAX)) {
		pr_err_ratelimited("Report failed, %u exceed max\n",
			event->sensor_type);
		return 0;
	}
	return hf_manager_find_client(manager->core, event);
}

static void hf_manager_io_complete(struct hf_manager *manager)
{
	clear_bit(HF_MANAGER_IO_IN_PROGRESS, &manager->flags);
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

int hf_device_register(struct hf_device *device)
{
	struct hf_core *core = &hfcore;

	INIT_LIST_HEAD(&device->list);
	device->ready = false;
	mutex_lock(&core->device_lock);
	list_add(&device->list, &core->device_list);
	mutex_unlock(&core->device_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(hf_device_register);

void hf_device_unregister(struct hf_device *device)
{
	struct hf_core *core = &hfcore;

	mutex_lock(&core->device_lock);
	list_del(&device->list);
	mutex_unlock(&core->device_lock);
	device->ready = false;
}
EXPORT_SYMBOL_GPL(hf_device_unregister);

int hf_manager_create(struct hf_device *device)
{
	uint8_t sensor_type = 0;
	int i = 0, err = 0;
	uint32_t gain = 0;
	struct hf_manager *manager = NULL;

	if (!device || !device->dev_name ||
			!device->support_list || !device->support_size)
		return -EINVAL;

	manager = kzalloc(sizeof(*manager), GFP_KERNEL);
	if (!manager)
		return -ENOMEM;

	manager->hf_dev = device;
	manager->core = &hfcore;
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
		sensor_type = device->support_list[i].sensor_type;
		gain = device->support_list[i].gain;
		if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX || !gain)) {
			pr_err("Device:%s register failed, %u invalid gain\n",
				device->dev_name, sensor_type);
			err = -EINVAL;
			goto out_err;
		}
		if (test_and_set_bit(sensor_type, sensor_list_bitmap)) {
			pr_err("Device:%s register failed, %u repeat\n",
				device->dev_name, sensor_type);
			err = -EBUSY;
			goto out_err;
		}
	}

	INIT_LIST_HEAD(&manager->list);
	mutex_lock(&manager->core->manager_lock);
	list_add(&manager->list, &manager->core->manager_list);
	mutex_unlock(&manager->core->manager_lock);

	mutex_lock(&manager->core->device_lock);
	manager->hf_dev->ready = true;
	mutex_unlock(&manager->core->device_lock);

	return 0;
out_err:
	kfree(manager);
	device->manager = NULL;
	return err;
}
EXPORT_SYMBOL_GPL(hf_manager_create);

void hf_manager_destroy(struct hf_manager *manager)
{
	uint8_t sensor_type = 0;
	int i = 0;
	struct hf_device *device = NULL;

	if (!manager || !manager->hf_dev || !manager->hf_dev->support_list)
		return;

	device = manager->hf_dev;
	for (i = 0; i < device->support_size; ++i) {
		sensor_type = device->support_list[i].sensor_type;
		if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX)) {
			pr_err("Device:%s unregister failed, %u exceed max\n",
				device->dev_name, sensor_type);
			continue;
		}
		clear_bit(sensor_type, sensor_list_bitmap);
	}
	mutex_lock(&manager->core->manager_lock);
	list_del(&manager->list);
	mutex_unlock(&manager->core->manager_lock);
	if (device->device_poll == HF_DEVICE_IO_POLLING)
		hrtimer_cancel(&manager->io_poll_timer);
	if (device->device_bus == HF_DEVICE_IO_ASYNC)
		tasklet_kill(&manager->io_work_tasklet);
	else if (device->device_bus == HF_DEVICE_IO_SYNC)
		kthread_flush_work(&manager->io_kthread_work);

	while (test_bit(HF_MANAGER_IO_IN_PROGRESS, &manager->flags))
		cpu_relax();

	kfree(manager);
}
EXPORT_SYMBOL_GPL(hf_manager_destroy);

int hf_device_register_manager_create(struct hf_device *device)
{
	int ret = 0;

	ret = hf_device_register(device);
	if (ret < 0)
		return ret;
	return hf_manager_create(device);
}
EXPORT_SYMBOL_GPL(hf_device_register_manager_create);

void hf_device_unregister_manager_destroy(struct hf_device *device)
{
	hf_manager_destroy(device->manager);
	hf_device_unregister(device);
}
EXPORT_SYMBOL_GPL(hf_device_unregister_manager_destroy);

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
	default:
		pr_err("Report %u failed, unknown action %u\n",
			event->sensor_type, event->action);
		/* unknown action must return 0 */
		err = 0;
		break;
	}
	return err;
}

static int hf_manager_find_client(struct hf_core *core,
		struct hf_manager_event *event)
{
	int err = 0;
	unsigned long flags;
	struct hf_client *client = NULL;

	spin_lock_irqsave(&core->client_lock, flags);
	list_for_each_entry(client, &core->client_list, list) {
		/* must (err |=), collect all err to decide retry */
		err |= hf_manager_distinguish_event(client, event);
	}
	spin_unlock_irqrestore(&core->client_lock, flags);

	return err;
}

static struct hf_manager *hf_manager_find_manager(struct hf_core *core,
		uint8_t sensor_type)
{
	int i = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;

	list_for_each_entry(manager, &core->manager_list, list) {
		device = READ_ONCE(manager->hf_dev);
		if (!device || !device->support_list)
			continue;
		for (i = 0; i < device->support_size; ++i) {
			if (sensor_type == device->support_list[i].sensor_type)
				return manager;
		}
	}
	pr_err("Failed to find manager, %u unregistered\n", sensor_type);
	return NULL;
}

static void hf_manager_update_client_param(struct hf_client *client,
		struct hf_manager_cmd *cmd, struct sensor_state *old)
{
	struct hf_manager_batch *batch = (struct hf_manager_batch *)cmd->data;
	struct sensor_state *request = &client->request[cmd->sensor_type];

	/* only enable disable update action delay and latency */
	if (cmd->action == HF_MANAGER_SENSOR_ENABLE) {
		/* save enable delay latency and start_time to old */
		old->enable = request->enable;
		old->delay = request->delay;
		old->latency = request->latency;
		atomic64_set(&old->start_time,
			atomic64_read(&request->start_time));
		/* update new */
		if (!request->enable)
			atomic64_set(&request->start_time,
				ktime_get_boot_ns());
		request->enable = true;
		request->delay = batch->delay;
		request->latency = batch->latency;
	} else if (cmd->action == HF_MANAGER_SENSOR_DISABLE) {
		atomic64_set(&request->start_time, S64_MAX);
		request->enable = false;
		request->delay = S64_MAX;
		request->latency = S64_MAX;
	}
}

static void hf_manager_clear_client_param(struct hf_client *client,
		struct hf_manager_cmd *cmd, struct sensor_state *old)
{
	struct sensor_state *request = &client->request[cmd->sensor_type];

	if (cmd->action == HF_MANAGER_SENSOR_ENABLE) {
		/*
		 * restore enable delay latency and start_time
		 * remember must not restore bias raw etc
		 */
		atomic64_set(&request->start_time,
			atomic64_read(&old->start_time));
		request->enable = old->enable;
		request->delay = old->delay;
		request->latency = old->latency;
	} else if (cmd->action == HF_MANAGER_SENSOR_DISABLE) {
		atomic64_set(&request->start_time, S64_MAX);
		request->enable = false;
		request->delay = S64_MAX;
		request->latency = S64_MAX;
	}
}

static void hf_manager_find_best_param(struct hf_core *core,
		uint8_t sensor_type, bool *action,
		int64_t *delay, int64_t *latency)
{
	unsigned long flags;
	struct hf_client *client = NULL;
	struct sensor_state *request = NULL;
	bool tmp_enable = false;
	int64_t tmp_delay = S64_MAX;
	int64_t tmp_latency = S64_MAX;
	const int64_t max_latency_ns = 2000000000000LL;

	spin_lock_irqsave(&core->client_lock, flags);
	list_for_each_entry(client, &core->client_list, list) {
		request = &client->request[sensor_type];
		if (request->enable) {
			tmp_enable = true;
			if (request->delay < tmp_delay)
				tmp_delay = request->delay;
			if (request->latency < tmp_latency)
				tmp_latency = request->latency;
		}
	}
	spin_unlock_irqrestore(&core->client_lock, flags);
	*action = tmp_enable;
	*delay = tmp_delay > 0 ? tmp_delay : 0;
	tmp_latency = tmp_latency > 0 ? tmp_latency : 0;
	*latency = tmp_latency < max_latency_ns ? tmp_latency : max_latency_ns;

#ifdef HF_MANAGER_DEBUG
	if (tmp_enable)
		pr_notice("Find best command %u %u %lld %lld\n",
			sensor_type, tmp_enable, tmp_delay, tmp_latency);
	else
		pr_notice("Find best command %u %u\n",
			sensor_type, tmp_enable);
#endif
}

static inline bool device_rebatch(struct hf_core *core, uint8_t sensor_type,
			int64_t best_delay, int64_t best_latency)
{
	if (core->state[sensor_type].delay != best_delay ||
			core->state[sensor_type].latency != best_latency) {
		core->state[sensor_type].delay = best_delay;
		core->state[sensor_type].latency = best_latency;
		return true;
	}
	return false;
}

static inline bool device_reenable(struct hf_core *core, uint8_t sensor_type,
		bool best_enable)
{
	if (core->state[sensor_type].enable != best_enable) {
		core->state[sensor_type].enable = best_enable;
		return true;
	}
	return false;
}

static inline bool device_redisable(struct hf_core *core, uint8_t sensor_type,
		bool best_enable, int64_t best_delay, int64_t best_latency)
{
	if (core->state[sensor_type].enable != best_enable) {
		core->state[sensor_type].enable = best_enable;
		core->state[sensor_type].delay = best_delay;
		core->state[sensor_type].latency = best_latency;
		return true;
	}
	return false;
}

static inline void device_request_update(struct hf_core *core,
		uint8_t sensor_type, struct sensor_state *old)
{
	/* save enable delay and latency to old */
	old->enable = core->state[sensor_type].enable;
	old->delay = core->state[sensor_type].delay;
	old->latency = core->state[sensor_type].latency;
}

static inline void device_request_clear(struct hf_core *core,
		uint8_t sensor_type, struct sensor_state *old)
{
	/*
	 * restore enable delay and latency
	 * remember must not restore bias raw etc
	 */
	core->state[sensor_type].enable = old->enable;
	core->state[sensor_type].delay = old->delay;
	core->state[sensor_type].latency = old->latency;
}

static int64_t device_poll_min_interval(struct hf_device *device)
{
	int i = 0;
	uint8_t j = 0;
	int64_t interval = S64_MAX;
	struct hf_core *core = device->manager->core;

	for (i = 0; i < device->support_size; ++i) {
		j = device->support_list[i].sensor_type;
		if (core->state[j].enable) {
			if (core->state[j].delay < interval)
				interval = core->state[j].delay;
		}
	}
	return interval;
}

static void device_poll_trigger(struct hf_device *device, bool enable)
{
	int64_t min_interval = S64_MAX;
	struct hf_manager *manager = device->manager;

	WARN_ON(enable && !atomic_read(&manager->io_enabled));
	min_interval = device_poll_min_interval(device);
	WARN_ON(atomic_read(&manager->io_enabled) && min_interval == S64_MAX);
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
	struct sensor_state old;
	struct hf_manager *manager = device->manager;
	struct hf_core *core = device->manager->core;
	bool best_enable = false;
	int64_t best_delay = S64_MAX;
	int64_t best_latency = S64_MAX;

	if (!device->enable || !device->batch)
		return -EINVAL;

	hf_manager_find_best_param(core, sensor_type, &best_enable,
		&best_delay, &best_latency);

	if (best_enable) {
		device_request_update(core, sensor_type, &old);
		if (device_rebatch(core, sensor_type,
				best_delay, best_latency)) {
			err = device->batch(device, sensor_type,
				best_delay, best_latency);
			/* handle error to return when batch fail */
			if (err < 0) {
				device_request_clear(core, sensor_type, &old);
				return err;
			}
		}
		if (device_reenable(core, sensor_type, best_enable)) {
			/* must update io_enabled before enable */
			atomic_inc(&manager->io_enabled);
			err = device->enable(device, sensor_type, best_enable);
			/* handle error to clear prev request */
			if (err < 0) {
				atomic_dec_if_positive(&manager->io_enabled);
				/*
				 * rebatch success and enable fail.
				 * update prev request from old.
				 */
				device_request_clear(core, sensor_type, &old);
				return err;
			}
		}
		if (device->device_poll == HF_DEVICE_IO_POLLING)
			device_poll_trigger(device, best_enable);
	} else {
		if (device_redisable(core, sensor_type, best_enable,
				best_delay, best_latency)) {
			atomic_dec_if_positive(&manager->io_enabled);
			err = device->enable(device, sensor_type, best_enable);
			/*
			 * disable fail no need to handle error.
			 * run next to update hrtimer or tasklet.
			 */
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
		uint8_t sensor_type, void *data, uint8_t length)
{
	if (device->config_cali)
		return device->config_cali(device, sensor_type, data, length);
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
	int err = 0;
	unsigned long flags;
	struct hf_core *core = device->manager->core;
	struct hf_client *client = NULL;
	struct sensor_state *request = NULL;
	bool best_enable = false;

	spin_lock_irqsave(&core->client_lock, flags);
	list_for_each_entry(client, &core->client_list, list) {
		request = &client->request[sensor_type];
		if (request->raw)
			best_enable = true;
	}
	spin_unlock_irqrestore(&core->client_lock, flags);

	if (!device->rawdata)
		return 0;
	if (core->state[sensor_type].raw == best_enable)
		return 0;
	core->state[sensor_type].raw = best_enable;
	err = device->rawdata(device, sensor_type, best_enable);
	if (err < 0)
		core->state[sensor_type].raw = false;
	return err;
}

static int hf_manager_device_info(struct hf_client *client,
		uint8_t sensor_type, struct sensor_info *info)
{
	int i = 0;
	int ret = 0;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;
	struct sensor_info *si = NULL;

	mutex_lock(&client->core->manager_lock);
	manager = hf_manager_find_manager(client->core, sensor_type);
	if (!manager) {
		ret = -EINVAL;
		goto err_out;
	}
	device = manager->hf_dev;
	if (!device || !device->support_list ||
			!device->support_size) {
		ret = -EINVAL;
		goto err_out;
	}
	for (i = 0; i < device->support_size; ++i) {
		if (device->support_list[i].sensor_type ==
				sensor_type) {
			si = &device->support_list[i];
			break;
		}
	}
	if (!si) {
		ret = -EINVAL;
		goto err_out;
	}
	*info = *si;

err_out:
	mutex_unlock(&client->core->manager_lock);
	return ret;
}

static int hf_manager_custom_cmd(struct hf_client *client,
		uint8_t sensor_type, struct custom_cmd *cust_cmd)
{
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;
	int ret = 0;

	if (cust_cmd->tx_len > sizeof(cust_cmd->data) ||
		cust_cmd->rx_len > sizeof(cust_cmd->data))
		return -EINVAL;

	mutex_lock(&client->core->manager_lock);
	manager = hf_manager_find_manager(client->core, sensor_type);
	if (!manager) {
		ret = -EINVAL;
		goto err_out;
	}
	device = manager->hf_dev;
	if (!device || !device->dev_name) {
		ret = -EINVAL;
		goto err_out;
	}
	if (device->custom_cmd)
		ret = device->custom_cmd(device, sensor_type, cust_cmd);

err_out:
	mutex_unlock(&client->core->manager_lock);
	return ret;
}

static int hf_manager_drive_device(struct hf_client *client,
		struct hf_manager_cmd *cmd)
{
	int err = 0;
	struct sensor_state old;
	struct hf_manager *manager = NULL;
	struct hf_device *device = NULL;
	struct hf_core *core = client->core;
	uint8_t sensor_type = cmd->sensor_type;

	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;

	mutex_lock(&core->manager_lock);
	manager = hf_manager_find_manager(core, sensor_type);
	if (!manager) {
		mutex_unlock(&core->manager_lock);
		return -EINVAL;
	}
	device = manager->hf_dev;
	if (!device || !device->dev_name) {
		mutex_unlock(&core->manager_lock);
		return -EINVAL;
	}

#ifdef HF_MANAGER_DEBUG
	pr_notice("Drive device:%s command %u %u %u\n",
		device->dev_name, cmd->sensor_type, cmd->action,
		cmd->padding);
#endif

	switch (cmd->action) {
	case HF_MANAGER_SENSOR_ENABLE:
	case HF_MANAGER_SENSOR_DISABLE:
		hf_manager_update_client_param(client, cmd, &old);
		err = hf_manager_device_enable(device, sensor_type);
		if (err < 0)
			hf_manager_clear_client_param(client, cmd, &old);
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
			sensor_type, cmd->data, cmd->length);
		break;
	case HF_MANAGER_SENSOR_SELFTEST:
		err = hf_manager_device_selftest(device, sensor_type);
		break;
	case HF_MANAGER_SENSOR_RAWDATA:
		client->request[sensor_type].raw =
			cmd->data[0] ? true : false;
		err = hf_manager_device_rawdata(device, sensor_type);
		if (err < 0)
			client->request[sensor_type].raw = false;
		break;
	default:
		pr_err("Unknown action %u\n", cmd->action);
		err = -EINVAL;
		break;
	}
	mutex_unlock(&core->manager_lock);
	return err;
}

static int hf_manager_get_sensor_info(struct hf_client *client,
		uint8_t sensor_type, struct sensor_info *info)
{
	return hf_manager_device_info(client, sensor_type, info);
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
	client->core = &hfcore;

#ifdef HF_MANAGER_DEBUG
	pr_notice("Client create\n");
#endif

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

	spin_lock_irqsave(&client->core->client_lock, flags);
	list_add(&client->list, &client->core->client_list);
	spin_unlock_irqrestore(&client->core->client_lock, flags);

	return client;
err_free:
	kfree(client);
err_out:
	return NULL;
}
EXPORT_SYMBOL_GPL(hf_client_create);

void hf_client_destroy(struct hf_client *client)
{
	unsigned long flags;

#ifdef HF_MANAGER_DEBUG
	pr_notice("Client destroy\n");
#endif
	spin_lock_irqsave(&client->core->client_lock, flags);
	list_del(&client->list);
	spin_unlock_irqrestore(&client->core->client_lock, flags);

	kfree(client->hf_fifo.buffer);
	kfree(client);
}
EXPORT_SYMBOL_GPL(hf_client_destroy);

int hf_client_find_sensor(struct hf_client *client, uint8_t sensor_type)
{
	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;
	if (!test_bit(sensor_type, sensor_list_bitmap))
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL_GPL(hf_client_find_sensor);

int hf_client_get_sensor_info(struct hf_client *client,
		uint8_t sensor_type, struct sensor_info *info)
{
	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;
	if (!test_bit(sensor_type, sensor_list_bitmap))
		return -EINVAL;
	return hf_manager_device_info(client, sensor_type, info);
}
EXPORT_SYMBOL_GPL(hf_client_get_sensor_info);

int hf_client_request_sensor_cali(struct hf_client *client,
		uint8_t sensor_type, unsigned int cmd, bool status)
{
	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;
	if (!test_bit(sensor_type, sensor_list_bitmap))
		return -EINVAL;
	switch (cmd) {
	case HF_MANAGER_REQUEST_BIAS_DATA:
		client->request[sensor_type].bias = status;
		break;
	case HF_MANAGER_REQUEST_CALI_DATA:
		client->request[sensor_type].cali = status;
		break;
	case HF_MANAGER_REQUEST_TEMP_DATA:
		client->request[sensor_type].temp = status;
		break;
	case HF_MANAGER_REQUEST_TEST_DATA:
		client->request[sensor_type].test = status;
		break;
	default:
		pr_err("Unknown command %u\n", cmd);
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(hf_client_request_sensor_cali);

int hf_client_control_sensor(struct hf_client *client,
		struct hf_manager_cmd *cmd)
{
	return hf_manager_drive_device(client, cmd);
}
EXPORT_SYMBOL_GPL(hf_client_control_sensor);

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
		hf_fifo->client_active = ktime_get_boot_ns();
	}
	spin_unlock_irqrestore(&hf_fifo->buffer_lock, flags);
	return have_event;
}

/* timeout: MAX_SCHEDULE_TIMEOUT or msecs_to_jiffies(ms) */
int hf_client_poll_sensor_timeout(struct hf_client *client,
		struct hf_manager_event *data, int count, long timeout)
{
	long ret = 0;
	int read = 0;
	struct hf_client_fifo *hf_fifo = &client->hf_fifo;

	/* ret must be long to fill timeout(MAX_SCHEDULE_TIMEOUT) */
	ret = wait_event_interruptible_timeout(hf_fifo->wait,
		hf_fifo->head != hf_fifo->tail, timeout);

	if (!ret)
		return -ETIMEDOUT;
	if (ret < 0)
		return ret;

	for (;;) {
		if (hf_fifo->head == hf_fifo->tail)
			return 0;
		if (count == 0)
			break;
		while (read < count &&
			fetch_next(hf_fifo, &data[read])) {
			read++;
		}
		if (read)
			break;
	}
	return read;
}
EXPORT_SYMBOL_GPL(hf_client_poll_sensor_timeout);

int hf_client_custom_cmd(struct hf_client *client,
		uint8_t sensor_type, struct custom_cmd *cust_cmd)
{
	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;
	if (!test_bit(sensor_type, sensor_list_bitmap))
		return -EINVAL;
	return hf_manager_custom_cmd(client, sensor_type, cust_cmd);
}
EXPORT_SYMBOL_GPL(hf_client_custom_cmd);

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
		while (read + sizeof(event) <= count &&
				fetch_next(hf_fifo, &event)) {
			if (copy_to_user(buf + read, &event, sizeof(event)))
				return -EFAULT;
			read += sizeof(event);
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

	memset(&cmd, 0, sizeof(cmd));

	if (count != sizeof(struct hf_manager_cmd))
		return -EINVAL;

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

	client->ppid = current->pid;

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
	uint8_t sensor_type = 0;
	struct ioctl_packet packet;
	struct sensor_info info;
	struct custom_cmd *cust_cmd = NULL;
	struct hf_device *device = NULL;

	memset(&packet, 0, sizeof(packet));

	if (size != sizeof(struct ioctl_packet))
		return -EINVAL;
	if (copy_from_user(&packet, ubuf, sizeof(packet)))
		return -EFAULT;
	sensor_type = packet.sensor_type;
	if (unlikely(sensor_type >= SENSOR_TYPE_SENSOR_MAX))
		return -EINVAL;

	switch (cmd) {
	case HF_MANAGER_REQUEST_REGISTER_STATUS:
		packet.status = test_bit(sensor_type, sensor_list_bitmap);
		if (copy_to_user(ubuf, &packet, sizeof(packet)))
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
	case HF_MANAGER_REQUEST_SENSOR_INFO:
		if (!test_bit(sensor_type, sensor_list_bitmap))
			return -EINVAL;
		memset(&info, 0, sizeof(info));
		if (hf_manager_get_sensor_info(client, sensor_type, &info))
			return -EINVAL;
		if (sizeof(packet.byte) < sizeof(info))
			return -EINVAL;
		memcpy(packet.byte, &info, sizeof(info));
		if (copy_to_user(ubuf, &packet, sizeof(packet)))
			return -EFAULT;
		break;
	case HF_MANAGER_REQUEST_CUST_DATA:
		if (!test_bit(sensor_type, sensor_list_bitmap))
			return -EINVAL;
		if (sizeof(packet.byte) < sizeof(*cust_cmd))
			return -EINVAL;
		cust_cmd = (struct custom_cmd *)packet.byte;
		if (hf_manager_custom_cmd(client, sensor_type, cust_cmd))
			return -EINVAL;
		if (copy_to_user(ubuf, &packet, sizeof(packet)))
			return -EFAULT;
		break;
	case HF_MANAGER_REQUEST_READY_STATUS:
		mutex_lock(&client->core->device_lock);
		packet.status = true;
		list_for_each_entry(device, &client->core->device_list, list) {
			if (!READ_ONCE(device->ready)) {
				pr_err_ratelimited("Device:%s not ready\n",
					device->dev_name);
				packet.status = false;
				break;
			}
		}
		mutex_unlock(&client->core->device_lock);
		if (copy_to_user(ubuf, &packet, sizeof(packet)))
			return -EFAULT;
		break;
	default:
		pr_err("Unknown command %u\n", cmd);
		return -EINVAL;
	}
	return 0;
}

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
	int i = 0, j = 0, k = 0;
	uint8_t sensor_type = 0;
	unsigned long flags;
	struct hf_core *core = (struct hf_core *)m->private;
	struct hf_manager *manager = NULL;
	struct hf_client *client = NULL;
	struct hf_device *device = NULL;
	const unsigned int debug_len = 4096;
	uint8_t *debug_buffer = NULL;

	seq_puts(m, "**************************************************\n");
	seq_puts(m, "Manager List:\n");
	mutex_lock(&core->manager_lock);
	j = 1;
	k = 1;
	list_for_each_entry(manager, &core->manager_list, list) {
		device = READ_ONCE(manager->hf_dev);
		if (!device || !device->support_list)
			continue;
		seq_printf(m, "%d. manager:[%d,%lld]\n", j++,
			atomic_read(&manager->io_enabled),
			print_s64((int64_t)atomic64_read(
				&manager->io_poll_interval)));
		seq_printf(m, " device:%s poll:%s bus:%s online\n",
			device->dev_name,
			device->device_poll ? "io_polling" : "io_interrupt",
			device->device_bus ? "io_async" : "io_sync");
		for (i = 0; i < device->support_size; ++i) {
			sensor_type = device->support_list[i].sensor_type;
			seq_printf(m, "  (%d) type:%u info:[%u,%s,%s]\n",
				k++,
				sensor_type,
				device->support_list[i].gain,
				device->support_list[i].name,
				device->support_list[i].vendor);
		}
	}
	mutex_unlock(&core->manager_lock);

	seq_puts(m, "**************************************************\n");
	seq_puts(m, "Client List:\n");
	spin_lock_irqsave(&core->client_lock, flags);
	j = 1;
	k = 1;
	list_for_each_entry(client, &core->client_list, list) {
		seq_printf(m, "%d. client:%s pid:[%d:%d,%d] online\n",
			j++,
			client->proc_comm,
			client->leader_pid,
			client->pid,
			client->ppid);
		for (i = 0; i < SENSOR_TYPE_SENSOR_MAX; ++i) {
			if (!client->request[i].enable)
				continue;
			seq_printf(m, " (%d) type:%d param:[%lld,%lld,%lld]\n",
				k++,
				i,
				client->request[i].delay,
				client->request[i].latency,
				(int64_t)atomic64_read(
					&client->request[i].start_time));
		}
	}
	spin_unlock_irqrestore(&core->client_lock, flags);

	seq_puts(m, "**************************************************\n");
	seq_puts(m, "Active List:\n");
	mutex_lock(&core->manager_lock);
	j = 1;
	for (i = 0; i < SENSOR_TYPE_SENSOR_MAX; ++i) {
		if (!core->state[i].enable)
			continue;
		seq_printf(m, "%d. type:%d param:[%lld,%lld]\n",
			j++,
			i,
			core->state[i].delay,
			core->state[i].latency);
	}
	mutex_unlock(&core->manager_lock);

	seq_puts(m, "**************************************************\n");
	mutex_lock(&core->manager_lock);
	debug_buffer = kzalloc(debug_len, GFP_KERNEL);
	list_for_each_entry(manager, &core->manager_list, list) {
		device = READ_ONCE(manager->hf_dev);
		if (!device || !device->support_list || !device->debug)
			continue;
		if (device->debug(device, SENSOR_TYPE_INVALID, debug_buffer,
				debug_len) > 0) {
			seq_printf(m, "Debug Sub Module: %s\n",
				device->dev_name);
			seq_printf(m, "%s\n", debug_buffer);
		}
	}
	kfree(debug_buffer);
	mutex_unlock(&core->manager_lock);
	return 0;
}

static int hf_manager_proc_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, hf_manager_proc_show, PDE_DATA(inode));
}

static const struct file_operations hf_manager_proc_fops = {
	.open           = hf_manager_proc_open,
	.release        = single_release,
	.read           = seq_read,
	.llseek         = seq_lseek,
};

static int __init hf_manager_init(void)
{
	int ret;
	struct device *dev;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO / 2 };

	init_hf_core(&hfcore);

	major = register_chrdev(0, "hf_manager", &hf_manager_fops);
	if (major < 0) {
		pr_err("Unable to get major\n");
		ret = major;
		goto err_exit;
	}

	hf_manager_class = class_create(THIS_MODULE, "hf_manager");
	if (IS_ERR(hf_manager_class)) {
		pr_err("Failed to create class\n");
		ret = PTR_ERR(hf_manager_class);
		goto err_chredev;
	}

	dev = device_create(hf_manager_class, NULL, MKDEV(major, 0),
		NULL, "hf_manager");
	if (IS_ERR(dev)) {
		pr_err("Failed to create device\n");
		ret = PTR_ERR(dev);
		goto err_class;
	}

	if (!proc_create_data("hf_manager", 0440, NULL,
			&hf_manager_proc_fops, &hfcore))
		pr_err("Failed to create proc\n");

	task = kthread_run(kthread_worker_fn,
			&hfcore.kworker, "hf_manager");
	if (IS_ERR(task)) {
		pr_err("Failed to create kthread\n");
		ret = PTR_ERR(task);
		goto err_device;
	}
	sched_setscheduler(task, SCHED_FIFO, &param);
	return 0;

err_device:
	device_destroy(hf_manager_class, MKDEV(major, 0));
err_class:
	class_destroy(hf_manager_class);
err_chredev:
	unregister_chrdev(major, "hf_manager");
err_exit:
	return ret;
}

static void __exit hf_manager_exit(void)
{
	kthread_stop(task);
	device_destroy(hf_manager_class, MKDEV(major, 0));
	class_destroy(hf_manager_class);
	unregister_chrdev(major, "hf_manager");
}

subsys_initcall(hf_manager_init);
module_exit(hf_manager_exit);

MODULE_DESCRIPTION("high frequency manager");
MODULE_AUTHOR("Hongxu Zhao <hongxu.zhao@mediatek.com>");
MODULE_LICENSE("GPL v2");

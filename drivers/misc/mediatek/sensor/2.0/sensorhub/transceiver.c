// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "transceiver " fmt

#include <linux/err.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/suspend.h>

#include "ready.h"
#include "sensor_comm.h"
#include "sensor_list.h"
#include "share_memory.h"
#include "timesync.h"
#include "debug.h"
#include "custom_cmd.h"

struct transceiver_config {
	uint8_t length;
	uint8_t data[0] __aligned(4);
};

struct transceiver_state {
	bool enable;
	uint8_t flush;
	struct sensor_comm_batch batch;
	struct transceiver_config *config;
};

struct transceiver_device {
	struct hf_device hf_dev;

	struct timesync_filter filter;

	struct mutex enable_lock;
	struct mutex flush_lock;
	struct mutex config_lock;
	struct transceiver_state state[SENSOR_TYPE_SENSOR_MAX];

	struct sensor_info support_list[SENSOR_TYPE_SENSOR_MAX];
	unsigned int support_size;

	struct share_mem shm_reader;
	struct share_mem_data shm_buffer[8];
	struct share_mem shm_super_reader;
	struct share_mem_super_data shm_super_buffer[4];
	struct wakeup_source *wakeup_src;
	int64_t raw_ts_reverse_debug[SENSOR_TYPE_SENSOR_MAX];
	int64_t comp_ts_reverse_debug[SENSOR_TYPE_SENSOR_MAX];

	atomic_t first_bootup;
	atomic_t normal_wp_dropped;
	atomic_t super_wp_dropped;
	struct task_struct *task;
};

static struct transceiver_device transceiver_dev;
static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ, 10);
DEFINE_SPINLOCK(transceiver_fifo_lock);
DECLARE_COMPLETION(transceiver_done);
DEFINE_KFIFO(transceiver_fifo, uint32_t, 32);
DEFINE_KFIFO(transceiver_super_fifo, uint32_t, 32);

static void transceiver_notify_func(struct sensor_comm_notify *n,
		void *private_data)
{
	uint32_t wp = 0;
	struct transceiver_device *dev = private_data;
	struct data_notify *dnotify = (struct data_notify *)n->value;
	uint64_t start_time = 0, end_time = 0, timeout_ns = 5000000;
	uint64_t wait_spin_lock_end_time = 0, timesync_end_time = 0;
	uint64_t kfifo_end_time = 0, complete_end_time = 0;

	if (n->command != SENS_COMM_NOTIFY_DATA_CMD &&
	    n->command != SENS_COMM_NOTIFY_FULL_CMD)
		return;

	start_time = ktime_get_boottime_ns();
	spin_lock(&transceiver_fifo_lock);
	wait_spin_lock_end_time = ktime_get_boottime_ns();
	timesync_filter_set(&dev->filter,
		dnotify->scp_timestamp, dnotify->scp_archcounter);
	timesync_end_time = ktime_get_boottime_ns();
	if (kfifo_is_full(&transceiver_fifo)) {
		if (kfifo_out(&transceiver_fifo, &wp, 1))
			atomic_inc(&dev->normal_wp_dropped);
	}
	wp = dnotify->write_position;
	kfifo_in(&transceiver_fifo, &wp, 1);
	kfifo_end_time = ktime_get_boottime_ns();
	complete(&transceiver_done);
	complete_end_time = ktime_get_boottime_ns();
	spin_unlock(&transceiver_fifo_lock);
	end_time = ktime_get_boottime_ns();
	if (end_time - start_time > timeout_ns && __ratelimit(&ratelimit))
		printk_deferred("time monitor:%llu, %llu, %llu, %llu, %llu, %llu\n",
			start_time, wait_spin_lock_end_time, timesync_end_time,
			kfifo_end_time, complete_end_time, end_time);
}

static void transceiver_super_notify_func(struct sensor_comm_notify *n,
		void *private_data)
{
	uint32_t wp = 0;
	struct transceiver_device *dev = private_data;
	struct data_notify *dnotify = (struct data_notify *)n->value;

	if (n->command != SENS_COMM_NOTIFY_SUPER_DATA_CMD &&
	    n->command != SENS_COMM_NOTIFY_SUPER_FULL_CMD)
		return;

	spin_lock(&transceiver_fifo_lock);
	timesync_filter_set(&dev->filter,
		dnotify->scp_timestamp, dnotify->scp_archcounter);
	if (kfifo_is_full(&transceiver_super_fifo)) {
		if (kfifo_out(&transceiver_super_fifo, &wp, 1))
			atomic_inc(&dev->super_wp_dropped);
	}
	wp = dnotify->write_position;
	kfifo_in(&transceiver_super_fifo, &wp, 1);
	complete(&transceiver_done);
	spin_unlock(&transceiver_fifo_lock);
}

static bool transceiver_wakeup_check(uint8_t action, uint8_t sensor_type)
{
	/*
	 * oneshot proximity tiledetect should wakeup source when data action
	 */
	if (action == DATA_ACTION && (sensor_type == SENSOR_TYPE_PROXIMITY ||
			sensor_type == SENSOR_TYPE_STEP_DETECTOR ||
			sensor_type == SENSOR_TYPE_SIGNIFICANT_MOTION ||
			sensor_type == SENSOR_TYPE_WAKE_GESTURE ||
			sensor_type == SENSOR_TYPE_GLANCE_GESTURE ||
			sensor_type == SENSOR_TYPE_PICK_UP_GESTURE ||
			sensor_type == SENSOR_TYPE_STATIONARY_DETECT ||
			sensor_type == SENSOR_TYPE_MOTION_DETECT ||
			sensor_type == SENSOR_TYPE_IN_POCKET ||
			sensor_type == SENSOR_TYPE_ANSWER_CALL ||
			sensor_type == SENSOR_TYPE_FLAT))
		return true;

	return false;
}

static void transceiver_copy_config(struct transceiver_config *dst,
		struct hf_manager_event *src, uint8_t bias_len,
		uint8_t cali_len, uint8_t temp_len)
{
	if (dst->length < (bias_len + cali_len + temp_len)) {
		pr_err_ratelimited("can't copy config %u %u %u %u %u %u\n",
			src->sensor_type, src->action, dst->length,
			bias_len, cali_len, temp_len);
		return;
	}
	if (bias_len > sizeof(src->word) ||
		cali_len > sizeof(src->word) ||
			temp_len > sizeof(src->word)) {
		pr_err_ratelimited("can't copy config %u %u %u %u %u\n",
			src->sensor_type, src->action,
			bias_len, cali_len, temp_len);
		return;
	}
	if (src->action == BIAS_ACTION)
		memcpy(dst->data, src->word, bias_len);
	else if (src->action == CALI_ACTION)
		memcpy(dst->data + bias_len, src->word, cali_len);
	else if (src->action == TEMP_ACTION)
		memcpy(dst->data + bias_len + cali_len, src->word, temp_len);
}

static void transceiver_update_config(struct transceiver_device *dev,
		struct hf_manager_event *src)
{
	struct transceiver_config *dst = NULL;

	mutex_lock(&dev->config_lock);
	dst = dev->state[src->sensor_type].config;
	if (!dst) {
		mutex_unlock(&dev->config_lock);
		return;
	}
	switch (src->sensor_type) {
	case SENSOR_TYPE_ACCELEROMETER:
		transceiver_copy_config(dst, src, 12, 12, 0);
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD:
		transceiver_copy_config(dst, src, 12, 24, 0);
		break;
	case SENSOR_TYPE_GYROSCOPE:
		transceiver_copy_config(dst, src, 12, 12, 24);
		break;
	default:
		/*
		 * NOTE: default branch only handle CALI_ACTION.
		 * if you add new sensor type that only cali need store, you
		 * can use default branch.
		 * for example:
		 * SENSOR_TYPE_LIGHT,SENSOR_TYPE_PRESSURE,
		 * SENSOR_TYPE_PROXIMITY, SENSOR_TYPE_SAR, SENSOR_TYPE_OIS
		 * and so on can use this branch.
		 */
		if (src->action == CALI_ACTION &&
				dst->length <= sizeof(src->word))
			transceiver_copy_config(dst, src, 0, dst->length, 0);
		else
			pr_err_ratelimited("can't update config %u %u %u\n",
				src->sensor_type, src->action, dst->length);
		break;
	}
	mutex_unlock(&dev->config_lock);
}

static void transceiver_report(struct transceiver_device *dev,
		struct hf_manager_event *event)
{
	int ret = 0;
	bool need_wakeup = false;
	uint8_t sensor_type = 0, action = 0;
	struct hf_manager *manager = dev->hf_dev.manager;
	struct transceiver_state *state = NULL;

	if (!manager)
		return;

	action = event->action;
	sensor_type = event->sensor_type;
	state = &dev->state[sensor_type];
	need_wakeup = transceiver_wakeup_check(action, sensor_type);

	if (action == BIAS_ACTION || action == CALI_ACTION ||
			action == TEMP_ACTION)
		transceiver_update_config(dev, event);

	do {
		if (action != FLUSH_ACTION) {
			if (need_wakeup)
				__pm_wakeup_event(dev->wakeup_src, 250);
			ret = manager->report(manager, event);
		} else {
			/*
			 * NOTE: only for flush ret = 0 we decrease
			 * ret must reset to 0 for each loop
			 * sequence:
			 *  report thread flush fail ret < 0 then retry
			 *   disable thread report flush success
			 *    report thread try flush = 0 no need send
			 *     report thread while loop because ret < 0
			 */
			ret = 0;
			mutex_lock(&dev->flush_lock);
			if (state->flush > 0) {
				ret = manager->report(manager, event);
				if (!ret)
					state->flush--;
			}
			mutex_unlock(&dev->flush_lock);
		}
		if (ret < 0)
			usleep_range(2000, 4000);
	} while (ret < 0);
}

static int transceiver_translate(struct transceiver_device *dev,
		struct hf_manager_event *dst,
		const struct share_mem_data *src)
{
	int64_t remap_timestamp = 0;

	if (src->sensor_type >= SENSOR_TYPE_SENSOR_MAX ||
			src->action >= MAX_ACTION) {
		pr_err_ratelimited("invalid sensor event %u %u\n",
			src->sensor_type, src->action);
		return -EINVAL;
	}

	remap_timestamp = src->timestamp + timesync_filter_get(&dev->filter);
	if (src->action == DATA_ACTION) {
		dst->timestamp = remap_timestamp;
		dst->sensor_type = src->sensor_type;
		dst->accurancy = src->accurancy;
		dst->action = src->action;
		switch (src->sensor_type) {
		case SENSOR_TYPE_ACCELEROMETER:
		case SENSOR_TYPE_MAGNETIC_FIELD:
		case SENSOR_TYPE_GYROSCOPE:
			dst->word[0] = src->value[0];
			dst->word[1] = src->value[1];
			dst->word[2] = src->value[2];
			break;
		case SENSOR_TYPE_ORIENTATION:
		case SENSOR_TYPE_ROTATION_VECTOR:
		case SENSOR_TYPE_GAME_ROTATION_VECTOR:
		case SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR:
			dst->word[0] = src->value[0];
			dst->word[1] = src->value[1];
			dst->word[2] = src->value[2];
			dst->word[3] = src->value[3];
			break;
		case SENSOR_TYPE_LINEAR_ACCELERATION:
		case SENSOR_TYPE_GRAVITY:
			dst->word[0] = src->value[0];
			dst->word[1] = src->value[1];
			dst->word[2] = src->value[2];
			break;
		case SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
		case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
		case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
			dst->word[0] = src->value[0];
			dst->word[1] = src->value[1];
			dst->word[2] = src->value[2];
			dst->word[3] = src->value[3];
			dst->word[4] = src->value[4];
			dst->word[5] = src->value[5];
			break;
		case SENSOR_TYPE_GYRO_SECONDARY:
			dst->word[0] = src->value[0];
			dst->word[1] = src->value[1];
			dst->word[2] = src->value[2];
			break;
		case SENSOR_TYPE_LIGHT:
		case SENSOR_TYPE_PRESSURE:
		case SENSOR_TYPE_PROXIMITY:
		case SENSOR_TYPE_STEP_COUNTER:
			dst->word[0] = src->value[0];
			break;
		default:
			memcpy(dst->word, src->value,
				min(sizeof(dst->word), sizeof(src->value)));
			break;
		}
	} else if (src->action == FLUSH_ACTION) {
		dst->timestamp = remap_timestamp;
		dst->sensor_type = src->sensor_type;
		dst->action = src->action;
	} else {
		/*
		 * BIAS_ACTION, CALI_ACTION, TEMP_ACTION,
		 * TEST_ACTION and RAW_ACTION
		 */
		dst->timestamp = remap_timestamp;
		dst->sensor_type = src->sensor_type;
		dst->accurancy = src->accurancy;
		dst->action = src->action;
		memcpy(dst->word, src->value,
			min(sizeof(dst->word), sizeof(src->value)));
	}
	return 0;
}

static void transceiver_read(struct transceiver_device *dev,
		uint32_t write_position)
{
	int ret = 0;
	int i = 0;
	struct share_mem *shm = &dev->shm_reader;
	struct share_mem_data *buffer = dev->shm_buffer;
	uint32_t size = sizeof(dev->shm_buffer);
	uint32_t item_size = sizeof(dev->shm_buffer[0]);
	struct hf_manager_event evt;

	ret = share_mem_seek(shm, write_position);
	if (ret < 0) {
		pr_err("%s seek fail %d\n", shm->name, ret);
		return;
	}

	while (1) {
		ret = share_mem_read(shm, buffer, size);
		if (ret < 0 || ret > size) {
			pr_err("%s read fail %d\n", shm->name, ret);
			break;
		}
		if (ret == 0)
			break;
		if (ret % item_size) {
			pr_err("%s times greater fail %d\n", shm->name, ret);
			break;
		}
		for (i = 0; i < (ret / item_size); i++) {
			if (transceiver_translate(dev, &evt, &buffer[i]) < 0)
				continue;
			transceiver_report(dev, &evt);
		}
	}
}

static void transceiver_process(struct transceiver_device *dev)
{
	unsigned int ret = 0;
	uint32_t wp = 0;
	unsigned long flags = 0;

	while (1) {
		spin_lock_irqsave(&transceiver_fifo_lock, flags);
		ret = kfifo_out(&transceiver_fifo, &wp, 1);
		spin_unlock_irqrestore(&transceiver_fifo_lock, flags);
		if (!ret)
			break;
		transceiver_read(dev, wp);
	}
}

static int transceiver_translate_super(struct transceiver_device *dev,
		struct hf_manager_event *dst,
		const struct share_mem_super_data *src)
{
	if (src->sensor_type >= SENSOR_TYPE_SENSOR_MAX ||
			src->action >= MAX_ACTION) {
		pr_err_ratelimited("invalid sensor event %u %u\n",
			src->sensor_type, src->action);
		return -EINVAL;
	}

	dst->timestamp = src->timestamp + timesync_filter_get(&dev->filter);
	dst->sensor_type = src->sensor_type;
	dst->accurancy = src->accurancy;
	dst->action = src->action;
	memcpy(dst->word, src->value,
		min(sizeof(dst->word), sizeof(src->value)));
	return 0;
}

static void transceiver_read_super(struct transceiver_device *dev,
		uint32_t write_position)
{
	int ret = 0;
	int i = 0;
	struct share_mem *shm = &dev->shm_super_reader;
	struct share_mem_super_data *buffer = dev->shm_super_buffer;
	uint32_t size = sizeof(dev->shm_super_buffer);
	uint32_t item_size = sizeof(dev->shm_super_buffer[0]);
	struct hf_manager_event evt;

	ret = share_mem_seek(shm, write_position);
	if (ret < 0) {
		pr_err("%s seek fail %d\n", shm->name, ret);
		return;
	}

	while (1) {
		ret = share_mem_read(shm, buffer, size);
		if (ret < 0 || ret > size) {
			pr_err("%s read fail %d\n", shm->name, ret);
			break;
		}
		if (ret == 0)
			break;
		if (ret % item_size) {
			pr_err("%s times greater fail %d\n", shm->name, ret);
			break;
		}
		for (i = 0; i < (ret / item_size); i++) {
			if (transceiver_translate_super(dev,
					&evt, &buffer[i]) < 0)
				continue;
			transceiver_report(dev, &evt);
		}
	}
}

static void transceiver_process_super(struct transceiver_device *dev)
{
	unsigned int ret = 0;
	uint32_t wp = 0;
	unsigned long flags = 0;

	while (1) {
		spin_lock_irqsave(&transceiver_fifo_lock, flags);
		ret = kfifo_out(&transceiver_super_fifo, &wp, 1);
		spin_unlock_irqrestore(&transceiver_fifo_lock, flags);
		if (!ret)
			break;
		transceiver_read_super(dev, wp);
	}
}

static int transceiver_thread(void *data)
{
	int ret = 0;
	struct transceiver_device *dev = data;
	int32_t normal_wp_dropped = 0, super_wp_dropped = 0;

	while (!kthread_should_stop()) {
		ret = wait_for_completion_interruptible(&transceiver_done);
		if (ret)
			continue;

		normal_wp_dropped = atomic_xchg(&dev->normal_wp_dropped, 0);
		super_wp_dropped = atomic_xchg(&dev->super_wp_dropped, 0);
		if (unlikely(normal_wp_dropped))
			pr_err_ratelimited("drop normal write position:%u\n",
				normal_wp_dropped);
		if (unlikely(super_wp_dropped))
			pr_err_ratelimited("drop super write position:%u\n",
				super_wp_dropped);
		transceiver_process(dev);
		transceiver_process_super(dev);
	}

	return 0;
}

static int transceiver_comm_with(int sensor_type, int cmd,
		void *data, uint8_t length)
{
	int ret = 0;
	struct sensor_comm_ctrl *ctrl = NULL;

	ctrl = kzalloc(sizeof(*ctrl) + length, GFP_KERNEL);
	ctrl->sensor_type = sensor_type;
	ctrl->command = cmd;
	ctrl->length = length;
	if (length)
		memcpy(ctrl->data, data, length);
	ret = sensor_comm_ctrl_send(ctrl, sizeof(*ctrl) + ctrl->length);
	kfree(ctrl);
	return ret;
}

static int transceiver_enable(struct hf_device *hf_dev,
		int sensor_type, int en)
{
	int ret = 0;
	struct transceiver_device *dev = hf_dev->private_data;
	struct transceiver_state *state = NULL;

	state = &dev->state[sensor_type];
	mutex_lock(&dev->enable_lock);
	if (en) {
		ret = transceiver_comm_with(sensor_type,
			SENS_COMM_CTRL_ENABLE_CMD,
			&state->batch, sizeof(state->batch));
		if (ret >= 0)
			state->enable = true;
	} else {
		ret = transceiver_comm_with(sensor_type,
			SENS_COMM_CTRL_DISABLE_CMD, NULL, 0);
		state->batch.delay = S64_MAX;
		state->batch.latency = S64_MAX;
		state->enable = false;
	}
	mutex_unlock(&dev->enable_lock);
	return ret;
}

static int transceiver_batch(struct hf_device *hf_dev,
		int sensor_type, int64_t delay, int64_t latency)
{
	int ret = 0;
	struct transceiver_device *dev = hf_dev->private_data;
	struct transceiver_state *state = NULL;
	struct sensor_comm_batch batch = {.delay = delay, .latency = latency};

	state = &dev->state[sensor_type];
	mutex_lock(&dev->enable_lock);
	if (!state->enable) {
		state->batch = batch;
		mutex_unlock(&dev->enable_lock);
		return ret;
	}

	ret = transceiver_comm_with(sensor_type,
		SENS_COMM_CTRL_ENABLE_CMD, &batch, sizeof(batch));
	if (ret >= 0)
		state->batch = batch;
	mutex_unlock(&dev->enable_lock);
	return ret;
}

static int transceiver_flush(struct hf_device *hf_dev,
		int sensor_type)
{
	int ret = 0;
	struct transceiver_device *dev = hf_dev->private_data;
	struct transceiver_state *state = NULL;

	state = &dev->state[sensor_type];
	mutex_lock(&dev->flush_lock);
	state->flush++;
	mutex_unlock(&dev->flush_lock);
	ret = transceiver_comm_with(sensor_type,
		SENS_COMM_CTRL_FLUSH_CMD, NULL, 0);
	if (ret < 0) {
		mutex_lock(&dev->flush_lock);
		if (state->flush > 0)
			state->flush--;
		mutex_unlock(&dev->flush_lock);
	}
	return ret;
}

static int transceiver_calibration(struct hf_device *hf_dev,
		int sensor_type)
{
	return transceiver_comm_with(sensor_type,
		SENS_COMM_CTRL_CALI_CMD, NULL, 0);
}

static int transceiver_config(struct hf_device *hf_dev,
		int sensor_type, void *data, uint8_t length)
{
	struct transceiver_device *dev = hf_dev->private_data;
	struct transceiver_config *cfg = NULL;

	mutex_lock(&dev->config_lock);
	cfg = dev->state[sensor_type].config;
	if (!cfg) {
		cfg = kzalloc(sizeof(*cfg) + length, GFP_KERNEL);
		if (!cfg) {
			mutex_unlock(&dev->config_lock);
			return -ENOMEM;
		}
		dev->state[sensor_type].config = cfg;
	} else {
		if (cfg->length != length) {
			pr_err("length not equal to prev length\n");
			mutex_unlock(&dev->config_lock);
			return -EINVAL;

		}
	}
	cfg->length = length;
	memcpy(cfg->data, data, length);
	mutex_unlock(&dev->config_lock);

	return transceiver_comm_with(sensor_type,
		SENS_COMM_CTRL_CONFIG_CMD, data, length);
}

static transceiver_selftest(struct hf_device *hf_dev,
		int sensor_type)
{
	return transceiver_comm_with(sensor_type,
		SENS_COMM_CTRL_SELF_TEST_CMD, NULL, 0);
}

static int transceiver_rawdata(struct hf_device *hf_dev,
		int sensor_type, int en)
{
	int ret = 0;

	if (en)
		ret = transceiver_comm_with(sensor_type,
			SENS_COMM_CTRL_ENABLE_RAW_CMD, NULL, 0);
	else
		ret = transceiver_comm_with(sensor_type,
			SENS_COMM_CTRL_DISABLE_RAW_CMD, NULL, 0);
	return ret;
}

static int transceiver_debug(struct hf_device *hfdev, int sensor_type,
		uint8_t *buffer, unsigned int len)
{
	return debug_get_debug(sensor_type, buffer, len);
}

static int transceiver_custom_cmd(struct hf_device *hfdev, int sensor_type,
		struct custom_cmd *cust_cmd)
{
	return custom_cmd_comm_with(sensor_type, cust_cmd);
}

static void transceiver_restore_sensor(struct transceiver_device *dev)
{
	int ret = 0, index = 0, flush = 0;
	uint8_t sensor_type = 0;
	struct transceiver_state *state = NULL;

	/* NOTE: config restore all firstly, don't put in one for loop */
	mutex_lock(&dev->config_lock);
	for (index = 0; index < dev->support_size; index++) {
		sensor_type = dev->support_list[index].sensor_type;
		state = &dev->state[sensor_type];
		if (!state->config)
			continue;
		ret = transceiver_comm_with(sensor_type,
			SENS_COMM_CTRL_CONFIG_CMD,
			state->config->data, state->config->length);
		if (ret < 0)
			pr_err("restore config %u fail %d\n",
			       sensor_type, ret);
	}
	mutex_unlock(&dev->config_lock);
	/* enable restore */
	mutex_lock(&dev->enable_lock);
	for (index = 0; index < dev->support_size; index++) {
		sensor_type = dev->support_list[index].sensor_type;
		state = &dev->state[sensor_type];
		if (!state->enable)
			continue;
		ret = transceiver_comm_with(sensor_type,
			SENS_COMM_CTRL_ENABLE_CMD,
			&state->batch, sizeof(state->batch));
		if (ret < 0)
			pr_err("restore enable %u fail %d\n",
			       sensor_type, ret);
	}
	mutex_unlock(&dev->enable_lock);
	/* flush restore */
	mutex_lock(&dev->flush_lock);
	for (index = 0; index < dev->support_size; index++) {
		sensor_type = dev->support_list[index].sensor_type;
		state = &dev->state[sensor_type];
		flush = state->flush;
		while (flush-- > 0) {
			ret = transceiver_comm_with(sensor_type,
				SENS_COMM_CTRL_FLUSH_CMD, NULL, 0);
			if (ret < 0)
				pr_err("restore flush %u fail %d\n",
				       sensor_type, ret);
		}
	}
	mutex_unlock(&dev->flush_lock);
}

static int transceiver_create_manager(struct transceiver_device *dev)
{
	int ret = 0;
	struct hf_device *hf_dev = &dev->hf_dev;

	memset(dev->support_list, 0, sizeof(dev->support_list));
	ret = sensor_list_get_list(dev->support_list,
		ARRAY_SIZE(dev->support_list));
	if (ret < 0)
		return ret;

	dev->support_size = ret;
	/* refill support_list and support_size then create manager */
	dev->hf_dev.support_list = dev->support_list;
	dev->hf_dev.support_size = dev->support_size;
	return hf_manager_create(hf_dev);
}

static void transceiver_destroy_manager(struct transceiver_device *dev)
{
	hf_manager_destroy(dev->hf_dev.manager);
}

static void transceiver_sensor_bootup(struct transceiver_device *dev)
{
	int ret = 0;

	ret = share_mem_config();
	if (ret < 0) {
		pr_err("share mem config fail %d\n", ret);
		return;
	}
	if (likely(atomic_xchg(&dev->first_bootup, false))) {
		timesync_start();
		ret = transceiver_create_manager(dev);
		if (ret < 0) {
			pr_err("create manager fail %d\n", ret);
			return;
		}
	} else {
		transceiver_restore_sensor(dev);
	}
}

static int transceiver_ready_notifier_call(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	if (event)
		transceiver_sensor_bootup(&transceiver_dev);

	return NOTIFY_DONE;
}

static struct notifier_block transceiver_ready_notifier = {
	.notifier_call = transceiver_ready_notifier_call,
	.priority = READY_HIGHPRI,
};

static int transceiver_pm_notifier_call(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_POST_SUSPEND:
		transceiver_comm_with(SENSOR_TYPE_INVALID,
			SENS_COMM_CTRL_UNMASK_NOTIFY_CMD, NULL, 0);
		timesync_resume();
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		transceiver_comm_with(SENSOR_TYPE_INVALID,
			SENS_COMM_CTRL_MASK_NOTIFY_CMD, NULL, 0);
		timesync_suspend();
		return NOTIFY_DONE;
	default:
		return NOTIFY_OK;
	}
	return NOTIFY_OK;
}

static struct notifier_block transceiver_pm_notifier = {
	.notifier_call = transceiver_pm_notifier_call,
};

static int transceiver_shm_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	unsigned long flags = 0;
	struct transceiver_device *dev = private_data;

	spin_lock_irqsave(&transceiver_fifo_lock, flags);
	kfifo_reset(&transceiver_fifo);
	spin_unlock_irqrestore(&transceiver_fifo_lock, flags);

	dev->shm_reader.name = "trans_r";
	dev->shm_reader.item_size = sizeof(struct share_mem_data);
	dev->shm_reader.buffer_full_detect = false;
	return share_mem_init(&dev->shm_reader, cfg);
}

static int transceiver_shm_super_cfg(struct share_mem_config *cfg,
		void *private_data)
{
	unsigned long flags = 0;
	struct transceiver_device *dev = private_data;

	spin_lock_irqsave(&transceiver_fifo_lock, flags);
	kfifo_reset(&transceiver_super_fifo);
	spin_unlock_irqrestore(&transceiver_fifo_lock, flags);

	dev->shm_super_reader.name = "trans_super_r";
	dev->shm_super_reader.item_size = sizeof(struct share_mem_super_data);
	dev->shm_super_reader.buffer_full_detect = false;
	return share_mem_init(&dev->shm_super_reader, cfg);
}

static int __init transceiver_init(void)
{
	int ret = 0;
	struct transceiver_device *dev = &transceiver_dev;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	mutex_init(&dev->enable_lock);
	mutex_init(&dev->flush_lock);
	mutex_init(&dev->config_lock);
	dev->wakeup_src = wakeup_source_register(NULL, "trans_data");
	if (!dev->wakeup_src) {
		pr_err("trans_data wakeup source register fail\n");
		return -ENOMEM;
	}

	atomic_set(&dev->first_bootup, true);
	atomic_set(&dev->normal_wp_dropped, 0);
	atomic_set(&dev->super_wp_dropped, 0);

	memset(&dev->hf_dev, 0, sizeof(dev->hf_dev));
	dev->hf_dev.dev_name = "transceiver";
	dev->hf_dev.device_poll = HF_DEVICE_IO_INTERRUPT;
	dev->hf_dev.device_bus = HF_DEVICE_IO_ASYNC;
	dev->hf_dev.support_list = dev->support_list;
	dev->hf_dev.support_size = dev->support_size;
	dev->hf_dev.enable = transceiver_enable;
	dev->hf_dev.batch = transceiver_batch;
	dev->hf_dev.flush = transceiver_flush;
	dev->hf_dev.calibration = transceiver_calibration;
	dev->hf_dev.config_cali = transceiver_config;
	dev->hf_dev.selftest = transceiver_selftest;
	dev->hf_dev.rawdata = transceiver_rawdata;
	dev->hf_dev.debug = transceiver_debug;
	dev->hf_dev.custom_cmd = transceiver_custom_cmd;
	dev->hf_dev.private_data = dev;
	ret = hf_device_register(&dev->hf_dev);
	if (ret < 0) {
		pr_err("register hf device fail %d\n", ret);
		return ret;
	}

	ret = sensor_comm_init();
	if (ret < 0) {
		pr_err("sensor comm init fail %d\n", ret);
		goto out_device;
	}

	ret = sensor_list_init();
	if (ret < 0) {
		pr_err("sensor list init fail %d\n", ret);
		goto out_sensor_comm;
	}

	ret = debug_init();
	if (ret < 0) {
		pr_err("debug init fail %d\n", ret);
		goto out_sensor_list;
	}

	ret = custom_cmd_init();
	if (ret < 0) {
		pr_err("custom command init fail %d\n", ret);
		goto out_debug;
	}

	dev->filter.max_diff = 10000000000LL;
	dev->filter.min_diff = 10000000LL;
	dev->filter.bufsize = 16;
	dev->filter.name = "transceiver";
	ret = timesync_filter_init(&dev->filter);
	if (ret < 0) {
		pr_err("timesync filter init fail %d\n", ret);
		goto out_cust_cmd;
	}

	ret = timesync_init();
	if (ret < 0) {
		pr_err("timesync init fail %d\n", ret);
		goto out_timesync_filter;
	}

	ret = register_pm_notifier(&transceiver_pm_notifier);
	if (ret < 0) {
		pr_err("register pm notifier fail %d\n", ret);
		goto out_timesync;
	}

	dev->task = kthread_run(transceiver_thread, dev, "transceiver");
	if (IS_ERR(dev->task)) {
		ret = -ENOMEM;
		pr_err("create thread fail %d\n", ret);
		goto out_pm_notify;
	}
	sched_setscheduler(dev->task, SCHED_FIFO, &param);

	/*
	 * NOTE: handler resgiter must before host ready to avoid lost
	 * share mem config etc.
	 */
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_DATA_CMD,
		transceiver_notify_func, dev);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_FULL_CMD,
		transceiver_notify_func, dev);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_SUPER_DATA_CMD,
		transceiver_super_notify_func, dev);
	sensor_comm_notify_handler_register(SENS_COMM_NOTIFY_SUPER_FULL_CMD,
		transceiver_super_notify_func, dev);
	share_mem_config_handler_register(SHARE_MEM_DATA_PAYLOAD_TYPE,
		transceiver_shm_cfg, dev);
	share_mem_config_handler_register(SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE,
		transceiver_shm_super_cfg, dev);

	/*
	 * NOTE: sensor ready must before host ready to avoid lost ready notify
	 * host ready init must at the end of function.
	 */
	sensor_ready_notifier_chain_register(&transceiver_ready_notifier);
	ret = host_ready_init();
	if (ret < 0) {
		pr_err("host ready init fail %d\n", ret);
		goto out_ready;
	}

	return 0;

out_ready:
	host_ready_exit();
	sensor_ready_notifier_chain_unregister(&transceiver_ready_notifier);
	share_mem_config_handler_unregister(SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE);
	share_mem_config_handler_unregister(SHARE_MEM_DATA_PAYLOAD_TYPE);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_SUPER_FULL_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_SUPER_DATA_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_FULL_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_DATA_CMD);
out_pm_notify:
	unregister_pm_notifier(&transceiver_pm_notifier);
out_timesync:
	timesync_exit();
out_timesync_filter:
	timesync_filter_exit(&dev->filter);
out_cust_cmd:
	custom_cmd_exit();
out_debug:
	debug_exit();
out_sensor_list:
	sensor_list_exit();
out_sensor_comm:
	sensor_comm_exit();
out_device:
	hf_device_unregister(&dev->hf_dev);
	wakeup_source_unregister(dev->wakeup_src);
	return ret;
}

static void __exit transceiver_exit(void)
{
	struct transceiver_device *dev = &transceiver_dev;

	share_mem_config_handler_unregister(SHARE_MEM_SUPER_DATA_PAYLOAD_TYPE);
	share_mem_config_handler_unregister(SHARE_MEM_DATA_PAYLOAD_TYPE);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_SUPER_FULL_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_SUPER_DATA_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_FULL_CMD);
	sensor_comm_notify_handler_unregister(SENS_COMM_NOTIFY_DATA_CMD);
	if (!IS_ERR(dev->task))
		kthread_stop(dev->task);
	unregister_pm_notifier(&transceiver_pm_notifier);
	timesync_exit();
	custom_cmd_exit();
	debug_exit();
	sensor_list_exit();
	sensor_ready_notifier_chain_unregister(&transceiver_ready_notifier);
	host_ready_exit();
	sensor_comm_exit();
	hf_device_unregister(&dev->hf_dev);
	wakeup_source_unregister(dev->wakeup_src);
	transceiver_destroy_manager(dev);
}

module_init(transceiver_init);
module_exit(transceiver_exit);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("transceiver driver");
MODULE_LICENSE("GPL");

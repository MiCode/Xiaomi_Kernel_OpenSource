/*
* Copyright (C) 2015 MediaTek Inc.
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

#include <batch.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

static DEFINE_MUTEX(batch_data_mutex);
static DEFINE_MUTEX(batch_hw_mutex);
static struct batch_context *batch_context_obj;

static struct batch_init_info *batch_init_list[MAX_CHOOSE_BATCH_NUM] = {0};

static int IDToSensorType(int id)
{
	int sensorType;

	switch (id) {
	case ID_ACCELEROMETER:
		sensorType = SENSOR_TYPE_ACCELEROMETER;
		break;
	case ID_MAGNETIC:
		sensorType = SENSOR_TYPE_MAGNETIC_FIELD;
		break;
	case ID_MAGNETIC_UNCALIBRATED:
		sensorType = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
		break;
	case ID_ORIENTATION:
		sensorType = SENSOR_TYPE_ORIENTATION;
		break;
	case ID_GYROSCOPE:
		sensorType = SENSOR_TYPE_GYROSCOPE;
		break;
	case ID_GYROSCOPE_UNCALIBRATED:
		sensorType = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
		break;
	case ID_ROTATION_VECTOR:
		sensorType = SENSOR_TYPE_ROTATION_VECTOR;
		break;
	case ID_GAME_ROTATION_VECTOR:
		sensorType = SENSOR_TYPE_GAME_ROTATION_VECTOR;
		break;
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
		sensorType = SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR;
		break;
	case ID_LINEAR_ACCELERATION:
		sensorType = SENSOR_TYPE_LINEAR_ACCELERATION;
		break;
	case ID_GRAVITY:
		sensorType = SENSOR_TYPE_GRAVITY;
		break;
	case ID_LIGHT:
		sensorType = SENSOR_TYPE_LIGHT;
		break;
	case ID_PROXIMITY:
		sensorType = SENSOR_TYPE_PROXIMITY;
		break;
	case ID_PRESSURE:
		sensorType = SENSOR_TYPE_PRESSURE;
		break;
	case ID_HUMIDITY:
		sensorType = SENSOR_TYPE_HUMIDITY;
		break;
	case ID_TEMPRERATURE:
		sensorType = SENSOR_TYPE_TEMPERATURE;
		break;
	case ID_SIGNIFICANT_MOTION:
		sensorType = SENSOR_TYPE_SIGNIFICANT_MOTION;
		break;
	case ID_STEP_DETECTOR:
		sensorType = SENSOR_TYPE_STEP_DETECTOR;
		break;
	case ID_STEP_COUNTER:
		sensorType = SENSOR_TYPE_STEP_COUNTER;
		break;
	case ID_PEDOMETER:
		sensorType = SENSOR_TYPE_PEDOMETER;
		break;
	case ID_PDR:
		sensorType = SENSOR_TYPE_PDR;
		break;
	case ID_WAKE_GESTURE:
		sensorType = SENSOR_TYPE_WAKE_GESTURE;
		break;
	case ID_PICK_UP_GESTURE:
		sensorType = SENSOR_TYPE_PICK_UP_GESTURE;
		break;
	case ID_GLANCE_GESTURE:
		sensorType = SENSOR_TYPE_GLANCE_GESTURE;
		break;
	case ID_ACTIVITY:
		sensorType = SENSOR_TYPE_ACTIVITY;
		break;
	case ID_TILT_DETECTOR:
		sensorType = SENSOR_TYPE_TILT_DETECTOR;
		break;
	default:
		sensorType = -1;
	}

	return sensorType;
}

static int batch_update_polling_rate(void)
{
	struct batch_context *obj = batch_context_obj;
	int idx = 0;
	int mindelay = 0;
	int onchange_delay = 0;

	for (idx = 0; idx < ID_SENSOR_MAX_HANDLE; idx++) {
		if ((obj->active_sensor & (1ULL << idx)) &&
				(0 != obj->dev_list.data_dev[idx].maxBatchReportLatencyMs)) {
			switch (idx) {
			case ID_LIGHT:
			case ID_PROXIMITY:
			case ID_HUMIDITY:
			case ID_STEP_COUNTER:
			case ID_STEP_DETECTOR:
			case ID_TILT_DETECTOR:
				onchange_delay = obj->dev_list.data_dev[idx].maxBatchReportLatencyMs - 2000;
				break;
			default:
				onchange_delay = obj->dev_list.data_dev[idx].maxBatchReportLatencyMs;
				break;
			}
			if ((onchange_delay < mindelay) || (mindelay == 0))
				atomic_set(&obj->min_timeout_handle, idx);
			mindelay = ((onchange_delay < mindelay) || (mindelay == 0)) ? onchange_delay : mindelay;
		}

	}
	BATCH_LOG("get polling rate min value (%d) !\n", mindelay);
	return mindelay;
}

static int get_fifo_data(struct batch_context *obj)
{

	struct hwm_sensor_data sensor_data = {0};
	int idx, err = 0;
	int fifo_len =  -1;
	int fifo_status =  -1;
	int i = 0;
	int64_t  nt;
	struct timespec time;

	time.tv_sec = 0;
	time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	nt = time.tv_sec*1000000000LL+time.tv_nsec;
	for (i = 0; i <= ID_SENSOR_MAX_HANDLE; i++) {
		obj->timestamp_info[i].num = 1;
		obj->timestamp_info[i].end_t = nt;
	}

	/* BATCH_ERR("fwq!! get_fifo_data +++++++++	!\n"); */
	if ((obj->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
		&& (obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)
		&& (obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_fifo_status) != NULL) {
			mutex_lock(&batch_data_mutex);
			err = obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
				.get_fifo_status(&fifo_len, &fifo_status, 0, obj->timestamp_info);
			if (-1 == fifo_len) {
				/* we use fifo_status */
				if (1 == fifo_status) {
					err = obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data(0, &sensor_data);
					if (err)
						BATCH_LOG("batch get fifoA data error\n");
				}
			} else if (fifo_len >= 0) {
			#ifdef CONFIG_PM_WAKELOCKS
				/* __pm_stay_awake(&(batch_context_obj->read_data_wake_lock)); */
			#else
				wake_lock(&(batch_context_obj->read_data_wake_lock));
			#endif
				obj->numOfDataLeft = fifo_len;
				input_report_rel(obj->idev, EVENT_TYPE_BATCH_READY, fifo_len);
				input_sync(obj->idev);
			} else
				BATCH_LOG("can not handle this fifo, err = %d, fifo_len = %d\n", err, fifo_len);
				mutex_unlock(&batch_data_mutex);
	}

	for (idx = 0; idx < ID_SENSOR_MAX_HANDLE; idx++) {
			/* BATCH_LOG("get data from sensor (%d) !\n", idx); */
		if ((obj->dev_list.ctl_dev[idx].flush == NULL) || (obj->dev_list.data_dev[idx].get_data == NULL))
				continue;

		if ((obj->active_sensor & (1ULL << idx))) {
			do {
					err = obj->dev_list.data_dev[idx].get_data(idx, &sensor_data);
					if (err == 0)
						report_batch_data(obj->idev, &sensor_data);
			} while (err == 0);

		}
	}

	return err;
}

void batch_end(void)
{
	int handle = 0;
	struct batch_context *obj = batch_context_obj;

	mutex_lock(&batch_hw_mutex);
	for (handle = 0; handle < ID_SENSOR_MAX_HANDLE; handle++) {
		if (obj->active_sensor & (1ULL << handle)) {
			report_batch_finish(obj->idev, handle);
		}
	}
	mutex_unlock(&batch_hw_mutex);
}
int  batch_notify(enum BATCH_NOTIFY_TYPE type)
{
	int err = 0;
	struct batch_context *obj = batch_context_obj;

	if (type == TYPE_BATCHFULL) {
		err = get_fifo_data(obj);
		if (err)
			BATCH_LOG("fwq!! get fifo data error !\n");
	}

	if (type == TYPE_BATCHTIMEOUT) {
		err = get_fifo_data(obj);
		if (err)
			BATCH_LOG("fwq!! get fifo data error !\n");
	}
	if (obj->is_polling_run)
		mod_timer(&obj->timer, jiffies + atomic_read(&obj->delay)/(1000/HZ));

	return err;
}

static void batch_work_func(struct work_struct *work)
{
	struct batch_context *obj = batch_context_obj;
	int err;
	/*BATCH_ERR("fwq!! get data from sensor+++++++++  !\n");*/

	if ((obj->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
		&& (obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)
		&& (obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_fifo_status) != NULL
		&& (obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].batch_timeout) != NULL) {
			mutex_lock(&batch_data_mutex);
			err = obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
				.batch_timeout(atomic_read(&obj->min_timeout_handle), SENSOR_TIMEOUT_BATCH_FLUSH);
			mutex_unlock(&batch_data_mutex);
	}
	/*err = get_fifo_data(obj);
	if (err)
		BATCH_LOG("fwq!! get fifo data error !\n");*/

	if (obj->is_polling_run)
		mod_timer(&obj->timer, jiffies + atomic_read(&obj->delay)/(1000/HZ));

	/* BATCH_LOG("fwq!! get data from sensor obj->delay=%d ---------  !\n", atomic_read(&obj->delay)); */
}

static void batch_poll(unsigned long data)
{
	struct batch_context *obj = (struct batch_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}
#if 0
static void report_data_once(int handle)
{
	struct batch_context *obj = batch_context_obj;
	struct hwm_sensor_data sensor_data = {0};
	int err;

	obj->flush_result = 0;

	if ((obj->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
			&& (obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)) {
		obj->flush_result = obj->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush(handle);
		get_fifo_data(obj);
		report_batch_finish(obj->idev, handle);
	}

	if ((obj->dev_list.ctl_dev[handle].flush != NULL) && (obj->dev_list.data_dev[handle].get_data != NULL)) {
		do {
			err = obj->dev_list.data_dev[handle].get_data(handle, &sensor_data);
			/* sensor_data.value_divide = obj->dev_list.data_dev[sensor_data.sensor-1].div; */
			if (err == 0)
				report_batch_data(obj->idev, &sensor_data);
		} while (err == 0);
		report_batch_finish(obj->idev, handle);
		obj->flush_result = obj->dev_list.ctl_dev[handle].flush(handle);
	} else
		BATCH_LOG("batch mode is not support for this sensor!\n");

}
#endif
static struct batch_context *batch_context_alloc_object(void)
{
	struct batch_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	BATCH_LOG("batch_context_alloc_object++++\n");
	if (!obj) {
		BATCH_ERR("Alloc batch object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200); /*5Hz, set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->min_timeout_handle, 0);
	INIT_WORK(&obj->report, batch_work_func);
	init_timer(&obj->timer);
	obj->timer.expires	= jiffies + atomic_read(&obj->delay)/(1000/HZ);
	obj->timer.function	= batch_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->active_sensor = 0;
	obj->div_flag = 0;
	obj->force_wake_upon_fifo_full = SENSORS_BATCH_WAKE_UPON_FIFO_FULL;
	mutex_init(&obj->batch_op_mutex);
	BATCH_LOG("batch_context_alloc_object----\n");
	return obj;
}

static ssize_t batch_store_active(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct batch_context *cxt = NULL;
	int res = 0;
	int handle = 0;
	int en = 0;
	int delay = 0;
	int64_t  nt;
	struct timespec time;

	cxt = batch_context_obj;
	res = sscanf(buf, "%d,%d", &handle, &en);
	if (res != 2)
		BATCH_ERR(" batch_store_active param error: res = %d\n", res);

	BATCH_ERR(" batch_store_active handle=%d ,en=%d\n", handle, en);

	if (handle < 0 || ID_SENSOR_MAX_HANDLE < handle) {
		cxt->batch_result = -1;
		return count;
	} else if (ID_SENSOR_MAX_HANDLE <= handle) {
		cxt->batch_result = 0;
		return count;
	}

	if (2 == en) {
		cxt->div_flag = handle;
		BATCH_LOG(" batch_hal want read %d div\n", handle);
	return count;
	}

	if (cxt->dev_list.data_dev[handle].is_batch_supported == 0) {
		cxt->batch_result = 0;
	}

	mutex_lock(&batch_hw_mutex);
	if (0 == en) {
		if ((cxt->active_sensor & (1ULL << handle)) && (cxt->batch_sensor & (1ULL << handle))) {
			if ((cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_fifo_status) != NULL
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].batch_timeout) != NULL) {
						mutex_lock(&batch_data_mutex);
						/* in practice, this is too aggressive, but guaranteed to be enough
						*to flush empty the fifo. */
						res = cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
							.batch_timeout(handle, SENSOR_DEACTIVE_BATCH_FLUSH);
						/*res = cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
							.batch_timeout((void *)&arg);*/
						mutex_unlock(&batch_data_mutex);
				}
		}
		cxt->active_sensor = cxt->active_sensor & (~(1ULL << handle));
		cxt->batch_sensor = cxt->batch_sensor & (~(1ULL << handle));
	/* L would not do flush at first sensor enable batch mode.
	*So we need to flush sensor data when sensor disabled.
	* Do flush before call enable_hw_batch to make sure flush finish. */
		/*report_data_once(handle);*/
	} else if (1 == en) {
		cxt->active_sensor = cxt->active_sensor | (1ULL << handle);
		time.tv_sec = 0;
		time.tv_nsec = 0;
		get_monotonic_boottime(&time);
		nt = time.tv_sec*1000000000LL+time.tv_nsec;
		cxt->timestamp_info[handle].start_t = nt;
	}

	if (cxt->dev_list.ctl_dev[handle].enable_hw_batch != NULL) {
		/* BATCH_LOG("cxt->dev_list.ctl_dev[%d].enable_hw_batch, %d, %d, %d\n", handle,en,
		*cxt->dev_list.data_dev[handle].samplingPeriodMs, cxt->dev_list
		*.data_dev[handle].maxBatchReportLatencyMs); */
		res = cxt->dev_list.ctl_dev[handle].enable_hw_batch(handle, en, cxt->dev_list.data_dev[handle]
			.flags|cxt->force_wake_upon_fifo_full, (long long)cxt->dev_list
			.data_dev[handle].samplingPeriodMs*1000000,
			(long long)cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs*1000000);
		if (res < 0) {
			cxt->batch_result = -1;
			mutex_unlock(&batch_hw_mutex);
			return count;
		}
	} else if (cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].enable_hw_batch != NULL) {
		/* BATCH_LOG("cxt->dev_list.ctl_dev[%d].enable_hw_batch, %d, %d, %d\n",
		*ID_SENSOR_MAX_HANDLE,en,cxt->dev_list.data_dev[handle].samplingPeriodMs,
		*cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs); */
		res = cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].enable_hw_batch(handle, en,
			cxt->dev_list.data_dev[handle].flags|cxt->force_wake_upon_fifo_full,
			(long long)cxt->dev_list.data_dev[handle].samplingPeriodMs*1000000,
			(long long)cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs*1000000);
		if (res < 0) {
			cxt->batch_result = -1;
			mutex_unlock(&batch_hw_mutex);
			return count;
		}
	}
	mutex_unlock(&batch_hw_mutex);

	delay = batch_update_polling_rate();
	BATCH_LOG("batch_update_polling_rate = %d\n", delay);
	if (delay > 0) {
		cxt->is_polling_run = true;
		atomic_set(&cxt->delay, delay);
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
	} else {
		cxt->is_polling_run = false;
		del_timer_sync(&cxt->timer);
		cancel_work_sync(&cxt->report);
	}
	BATCH_LOG("batch_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t batch_show_active(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int div = 0;
	int count = 0;
	struct batch_context *cxt = NULL;

	cxt = batch_context_obj;
	/* display now enabling sensors of batch mode */
	div = cxt->dev_list.data_dev[cxt->div_flag].div;
	BATCH_LOG("batch %d_div value: %d\n", cxt->div_flag, div);
	count =  snprintf(buf, PAGE_SIZE, "%d\n", div);
	BATCH_LOG("count=%d,buf=%s\n", count, buf);
	return count;
}

static ssize_t batch_store_delay(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	BATCH_ERR(" batch_store_delay not support now\n");
	return count;
}

static ssize_t batch_show_delay(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	BATCH_LOG("batch_show_delay not support now\n");
	return len;
}

static ssize_t batch_store_batch(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int handle, flags = 0;
	long long samplingPeriodNs, maxBatchReportLatencyNs = 0;
	int res, delay = 0;
	struct batch_context *cxt = NULL;
	int64_t  nt;
	struct timespec time;
	int en;
	int normalToBatch = 0;
	int delayChange = 0;
	int timeoutChange = 0;

	cxt = batch_context_obj;
	/*msleep(50);*/
	res = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flags, &samplingPeriodNs, &maxBatchReportLatencyNs);
	if (res != 4)
		BATCH_ERR("batch_store_delay param error: res = %d\n", res);

	BATCH_ERR("batch_store_delay param: handle %d, flag:%d samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
			handle, flags, samplingPeriodNs, maxBatchReportLatencyNs);

	if (handle < 0 || ID_SENSOR_MAX_HANDLE < handle) {
		cxt->batch_result = -1;
		return count;
	} else if (ID_SENSOR_MAX_HANDLE <= handle) {
		cxt->batch_result = 0;
		return count;
	}

	if (cxt->dev_list.data_dev[handle].is_batch_supported == 0) {
		if (maxBatchReportLatencyNs == 0)
			cxt->batch_result = 0;
		else {
			cxt->batch_result = -1;
			return count;
		}
	} else if (flags & SENSORS_BATCH_DRY_RUN) {
		cxt->batch_result = 0;
		return count;
	}

	if ((cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].enable_hw_batch == NULL)
			&& (cxt->dev_list.ctl_dev[handle].enable_hw_batch == NULL)) {
		cxt->batch_result = -1;
		return count;
	}

	mutex_lock(&batch_hw_mutex);

	do_div(maxBatchReportLatencyNs, 1000000);
	do_div(samplingPeriodNs, 1000000);

	/* Change from normal mode to batch mode, update start time. */
	if (cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs == 0 && maxBatchReportLatencyNs != 0)
		normalToBatch = 1;

	if (cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs != 0 && maxBatchReportLatencyNs != 0 &&
		cxt->dev_list.data_dev[handle].samplingPeriodMs != samplingPeriodNs)
		delayChange = 1;

	if (cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs != 0 && maxBatchReportLatencyNs != 0 &&
		cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs != maxBatchReportLatencyNs)
		timeoutChange = 1;

	cxt->dev_list.data_dev[handle].samplingPeriodMs = samplingPeriodNs;
	cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs = maxBatchReportLatencyNs;
	cxt->dev_list.data_dev[handle].flags = flags;

	if (maxBatchReportLatencyNs == 0) {
		/* WE need not flush batch data when change from batch mode to normal mode.
		 * due to batch to normal, normal poll will flush the old batch data
		 */
		cxt->batch_sensor = cxt->batch_sensor & (~(1ULL << handle));
	} else if (maxBatchReportLatencyNs != 0) {
		cxt->batch_sensor = cxt->batch_sensor | ((1ULL << handle));
		/* BATCH_ERR("batch maxBatchReportLatencyNs: %lld, real_batch_sensor_bitmap: %x\n",
			maxBatchReportLatencyNs, cxt->real_batch_sensor_bitmap);*/
		/* Update start time only when change from normal mode to batch mode. */
		if (normalToBatch) {
			time.tv_sec = 0;
			time.tv_nsec = 0;
			get_monotonic_boottime(&time);
			nt = time.tv_sec*1000000000LL+time.tv_nsec;
			cxt->timestamp_info[handle].start_t = nt;
		} else if (delayChange) {
			if ((cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_fifo_status) != NULL
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].batch_timeout) != NULL) {
						mutex_lock(&batch_data_mutex);
						res = cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
							.batch_timeout(handle, SENSOR_DELAYCHANGE_BATCH_FLUSH);
						mutex_unlock(&batch_data_mutex);
				}
		} else if (timeoutChange) {
			if ((cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_fifo_status) != NULL
					&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].batch_timeout) != NULL) {
						mutex_lock(&batch_data_mutex);
						res = cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
							.batch_timeout(handle, SENSOR_TIMEOUTCHANGE_BATCH_FLUSH);
						mutex_unlock(&batch_data_mutex);
				}
		}
	}

	en = (cxt->active_sensor & (1ULL << handle)) ? 1 : 0;
	/* BATCH_LOG("en = %d\n", en); */
	if (cxt->dev_list.ctl_dev[handle].enable_hw_batch != NULL) {
		res = cxt->dev_list.ctl_dev[handle].enable_hw_batch(handle, en, flags|cxt->force_wake_upon_fifo_full,
				(long long)samplingPeriodNs*1000000, (long long)maxBatchReportLatencyNs*1000000);
		if (res < 0) {
			cxt->batch_result = -1;
			mutex_unlock(&batch_hw_mutex);
			return count;
		}
	} else if (cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].enable_hw_batch != NULL) {
		res = cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].enable_hw_batch
			(handle, en, flags|cxt->force_wake_upon_fifo_full,
				(long long)samplingPeriodNs*1000000, (long long)maxBatchReportLatencyNs*1000000);
		if (res < 0) {
			cxt->batch_result = -1;
			mutex_unlock(&batch_hw_mutex);
			return count;
		}
	}

	mutex_unlock(&batch_hw_mutex);

	delay = batch_update_polling_rate();
	BATCH_LOG("delay = %d\n", delay);
	if (delay > 0) {
		cxt->is_polling_run = true;
		atomic_set(&cxt->delay, delay);
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
	} else {
		cxt->is_polling_run = false;
		del_timer_sync(&cxt->timer);
		cancel_work_sync(&cxt->report);
	}

	cxt->batch_result = 0;
	BATCH_LOG("batch_store_batch done\n");
	return count;
}

static ssize_t batch_show_batch(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct batch_context *cxt = NULL;
	int res = 0;

	cxt = batch_context_obj;
	res = cxt->batch_result;
	BATCH_LOG(" batch_show_delay batch result: %d\n", res);
	return snprintf(buf, PAGE_SIZE, "%d\n", res);
}

static ssize_t batch_store_flush(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct batch_context *cxt = NULL;
	int handle;
	int ret;

	cxt = batch_context_obj;

	ret = kstrtoint(buf, 10, &handle);
	if (ret != 0) {
		BATCH_LOG("fwq flush_ err......\n");
		BATCH_ERR("invalid format!!\n");
		return count;
	}

	if (handle < 0 || ID_SENSOR_MAX_HANDLE < handle) {
		BATCH_ERR("invalid handle : %d\n", handle);
		cxt->flush_result = -1;
		return count;
	}
	BATCH_ERR("batch_store_flush, handle:%d\n", handle);
	report_batch_finish(cxt->idev, handle);
	if ((cxt->dev_list.ctl_dev[ID_SENSOR_MAX_HANDLE].flush != NULL)
		&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_data != NULL)
		&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].get_fifo_status) != NULL
		&& (cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE].batch_timeout) != NULL) {
			mutex_lock(&batch_data_mutex);
			ret = cxt->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
				.batch_timeout(handle, SENSOR_FLUSH_FIFO);
			mutex_unlock(&batch_data_mutex);
	}
	/*report_data_once(handle);*//* handle need to use of this function */
	return count;
}

static ssize_t batch_show_flush(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct batch_context *cxt = NULL;
	int res = 0;

	cxt = batch_context_obj;
	res = cxt->flush_result;
	BATCH_LOG(" batch_show_flush flush result: %d\n", res);
	return snprintf(buf, PAGE_SIZE, "%d\n", res);

}

static ssize_t batch_show_devnum(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct batch_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = batch_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5);
}
/*----------------------------------------------------------------*/
static int batch_open(struct inode *node , struct file *fp)
{
	BATCH_FUN(f);
	fp->private_data = NULL;
	return nonseekable_open(node, fp);
}
/*----------------------------------------------------------------------------*/
static int batch_release(struct inode *node, struct file *fp)
{
	BATCH_FUN(f);
	kfree(fp->private_data);
	fp->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long batch_unlocked_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct batch_trans_data batch_sensors_data;
	int i;
	int err;
	struct batch_timestamp_info *pt;
	int handle;

	if (batch_context_obj == NULL) {
		BATCH_ERR("null pointer!!\n");
		return -EINVAL;
	}

	switch (cmd) {
	case BATCH_IO_GET_SENSORS_DATA:
		if (copy_from_user(&batch_sensors_data, argp, sizeof(batch_sensors_data))) {
			BATCH_ERR("copy_from_user fail!!\n");
			return -EFAULT;
		}
		mutex_lock(&batch_data_mutex);
		/* BATCH_ERR("BATCH_IO_GET_SENSORS_DATA1, %d, %d, %d!!\n", batch_sensors_data.numOfDataReturn,
		*batch_sensors_data.numOfDataLeft, batch_context_obj->numOfDataLeft); */

		for (i = 0; i < batch_context_obj->numOfDataLeft && i < batch_sensors_data.numOfDataReturn; i++) {
			err = batch_context_obj->dev_list.data_dev[ID_SENSOR_MAX_HANDLE]
				.get_data(0, &batch_sensors_data.data[i]);
			if (err)
				BATCH_ERR("BATCH_IO_GET_SENSORS_DATA err = %d\n", err);
			else {
				handle = batch_sensors_data.data[i].sensor;

				if (batch_context_obj->dev_list.data_dev[handle].is_timestamp_supported == 0) {
					pt = &batch_context_obj->timestamp_info[handle];
					batch_sensors_data.data[i].time = pt->end_t - pt->start_t;
					if (pt->total_count == 0)
						BATCH_ERR("pt->total_count == 0\n");
					else
						do_div(batch_sensors_data.data[i].time, pt->total_count);

					if (batch_sensors_data.data[i].time > (int64_t)batch_context_obj->dev_list
							.data_dev[handle].samplingPeriodMs*1100000) {
						batch_sensors_data.data[i].time = pt->end_t - (int64_t)
							batch_context_obj->dev_list.data_dev[handle]
							.samplingPeriodMs*1100000*(pt->total_count-1);
						BATCH_LOG("FIFO wrapper around1, %lld, %lld, %lld\n",
								(int64_t)batch_context_obj->dev_list.data_dev[handle]
								.samplingPeriodMs, (int64_t)pt->total_count,
								(int64_t)batch_context_obj->dev_list.data_dev[handle]
								.samplingPeriodMs*1100000*(pt->total_count-1));
						BATCH_LOG("FIFO wrapper around2, %lld, %lld, %lld\n", pt->start_t,
								batch_sensors_data.data[i].time, pt->end_t);
					} else
							batch_sensors_data.data[i].time += pt->start_t;

					pt->start_t = batch_sensors_data.data[i].time;
				}
				batch_sensors_data.data[i].sensor = IDToSensorType(handle);
			}
		}

		batch_sensors_data.numOfDataReturn = i;
		batch_context_obj->numOfDataLeft -= i;
		batch_sensors_data.numOfDataLeft = batch_context_obj->numOfDataLeft;

		if (batch_context_obj->numOfDataLeft == 0) {
		#ifdef CONFIG_PM_WAKELOCKS
			/* __pm_relax(&(batch_context_obj->read_data_wake_lock)); */
		#else
			wake_unlock(&(batch_context_obj->read_data_wake_lock));
		#endif
		}

		/* BATCH_ERR("BATCH_IO_GET_SENSORS_DATA2, %d, %d, %d!!\n",
		*batch_sensors_data.numOfDataReturn, batch_sensors_data.numOfDataLeft,
		*batch_context_obj->numOfDataLeft); */
		mutex_unlock(&batch_data_mutex);
		if (copy_to_user(argp, &batch_sensors_data, sizeof(batch_sensors_data))) {
			BATCH_ERR("copy_to_user fail!!\n");
			return -EFAULT;
		}
		/*BATCH_ERR("\n");
		BATCH_ERR("batch_context_obj->numOfDataLeft : %d\n", batch_context_obj->numOfDataLeft);
		BATCH_ERR("\n");*/

		break;
	default:
		BATCH_ERR("have no this paramenter %d!!\n", cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long batch_compat_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!fp->f_op || !fp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_BATCH_IO_GET_SENSORS_DATA:
		err = fp->f_op->unlocked_ioctl(fp, BATCH_IO_GET_SENSORS_DATA, (unsigned long)arg32);
		break;
	default:
		BATCH_ERR("Unknown cmd %x!!\n", cmd);
		return -ENOIOCTLCMD;
	}

	return err;
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations batch_fops = {
	.open   = batch_open,
	.release = batch_release,
	.unlocked_ioctl = batch_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = batch_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static int batch_misc_init(struct batch_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = BATCH_MISC_DEV_NAME;
	cxt->mdev.fops  = &batch_fops;
	err = misc_register(&cxt->mdev);
	if (err)
		BATCH_ERR("unable to register batch misc device!!\n");

	return err;
}

static void batch_input_destroy(struct batch_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int batch_input_init(struct batch_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = BATCH_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_Z);
	/* input_set_capability(dev, EV_ABS, EVENT_TYPE_SENSORTYPE); */
	input_set_capability(dev, EV_REL, EVENT_TYPE_SENSORTYPE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_VALUE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BATCH_STATUS);
	/* input_set_capability(dev, EV_ABS, EVENT_TYPE_END_FLAG); */
	input_set_capability(dev, EV_REL, EVENT_TYPE_END_FLAG);
	input_set_capability(dev, EV_REL, EVENT_TYPE_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_TIMESTAMP_LO);
	input_set_capability(dev, EV_REL, EVENT_TYPE_BATCH_READY);

	input_set_abs_params(dev, EVENT_TYPE_BATCH_X, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_Y, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_Z, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_STATUS, BATCH_STATUS_MIN, BATCH_STATUS_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BATCH_VALUE, BATCH_VALUE_MIN, BATCH_VALUE_MAX, 0, 0);
	/* input_set_abs_params(dev, EVENT_TYPE_SENSORTYPE, BATCH_TYPE_MIN, BATCH_TYPE_MAX, 0, 0); */
	/* input_set_abs_params(dev, EVENT_TYPE_END_FLAG, BATCH_STATUS_MIN, BATCH_STATUS_MAX, 0, 0); */
	set_bit(EV_REL, dev->evbit);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}


DEVICE_ATTR(batchactive,	 S_IWUSR | S_IRUGO, batch_show_active, batch_store_active);
DEVICE_ATTR(batchdelay,	  S_IWUSR | S_IRUGO, batch_show_delay,  batch_store_delay);
DEVICE_ATTR(batchbatch,	  S_IWUSR | S_IRUGO, batch_show_batch,  batch_store_batch);
DEVICE_ATTR(batchflush,	  S_IWUSR | S_IRUGO, batch_show_flush,  batch_store_flush);
DEVICE_ATTR(batchdevnum,	  S_IWUSR | S_IRUGO, batch_show_devnum,  NULL);


static struct attribute *batch_attributes[] = {
	&dev_attr_batchactive.attr,
	&dev_attr_batchdelay.attr,
	&dev_attr_batchbatch.attr,
	&dev_attr_batchflush.attr,
	&dev_attr_batchdevnum.attr,
	NULL
};

static struct attribute_group batch_attribute_group = {
	.attrs = batch_attributes
};

int batch_register_data_path(int handle, struct batch_data_path *data)
{
	struct batch_context *cxt = NULL;

	cxt = batch_context_obj;
	if (data == NULL) {
		BATCH_ERR("data pointer is null!\n");
		return -1;
	}
	if (handle >= 0 && handle <= (ID_SENSOR_MAX_HANDLE)) {
		cxt->dev_list.data_dev[handle].get_data = data->get_data;
		cxt->dev_list.data_dev[handle].flags = data->flags;
		cxt->dev_list.data_dev[handle].get_fifo_status = data->get_fifo_status;
		cxt->dev_list.data_dev[handle].batch_timeout = data->batch_timeout;
		/* cxt ->dev_list.data_dev[handle].is_batch_supported = data->is_batch_supported; */
		return 0;
	}
	return -1;
}

int batch_register_control_path(int handle, struct batch_control_path *ctl)
{
	struct batch_context *cxt = NULL;

	cxt = batch_context_obj;
	if (ctl == NULL) {
		BATCH_ERR("ctl pointer is null!\n");
		return -1;
	}
	if (handle >= 0 && handle <= (ID_SENSOR_MAX_HANDLE)) {
		cxt->dev_list.ctl_dev[handle].enable_hw_batch = ctl->enable_hw_batch;
		cxt->dev_list.ctl_dev[handle].flush = ctl->flush;
		return 0;
	}
	return -1;
}

int batch_register_support_info(int handle, int support, int div, int timestamp_supported)
{
	struct batch_context *cxt = NULL;

	cxt = batch_context_obj;
	if (cxt == NULL) {
		if (0 == support)
			return 0;
		BATCH_ERR("cxt pointer is null!\n");
		return -1;
	}

	if (handle >= 0 && handle <= (ID_SENSOR_MAX_HANDLE)) {
		cxt->dev_list.data_dev[handle].is_batch_supported = support;
		cxt->dev_list.data_dev[handle].div = div;
		cxt->dev_list.data_dev[handle].is_timestamp_supported = timestamp_supported;
		cxt->dev_list.data_dev[handle].samplingPeriodMs = 200;
		cxt->dev_list.data_dev[handle].maxBatchReportLatencyMs = 0;
		return 0;
	}
	return -1;
}

void report_batch_data(struct input_dev *dev, struct hwm_sensor_data *data)
{
	struct hwm_sensor_data report_data;

	memcpy(&report_data, data, sizeof(struct hwm_sensor_data));

	if (report_data.sensor == ID_ACCELEROMETER || report_data.sensor == ID_MAGNETIC
			|| report_data.sensor == ID_ORIENTATION || report_data.sensor == ID_GYROSCOPE){
		input_report_rel(dev, EVENT_TYPE_SENSORTYPE, IDToSensorType(report_data.sensor));
		input_report_abs(dev, EVENT_TYPE_BATCH_X, report_data.values[0]);
		input_report_abs(dev, EVENT_TYPE_BATCH_Y, report_data.values[1]);
		input_report_abs(dev, EVENT_TYPE_BATCH_Z, report_data.values[2]);
		input_report_abs(dev, EVENT_TYPE_BATCH_STATUS, report_data.status);
		input_report_rel(dev, EVENT_TYPE_TIMESTAMP_HI, (uint32_t) (report_data.time>>32)&0xFFFFFFFF);
		input_report_rel(dev, EVENT_TYPE_TIMESTAMP_LO, (uint32_t)(report_data.time&0xFFFFFFFF));
		input_sync(dev);
	} else {
		input_report_rel(dev, EVENT_TYPE_SENSORTYPE, IDToSensorType(report_data.sensor));
		input_report_abs(dev, EVENT_TYPE_BATCH_VALUE, report_data.values[0]);
		input_report_abs(dev, EVENT_TYPE_BATCH_STATUS, report_data.status);
		input_report_rel(dev, EVENT_TYPE_TIMESTAMP_HI, (uint32_t) (report_data.time>>32)&0xFFFFFFFF);
		input_report_rel(dev, EVENT_TYPE_TIMESTAMP_LO, (uint32_t)(report_data.time&0xFFFFFFFF));
		input_sync(dev);
	}
}

void report_batch_finish(struct input_dev *dev, int handle)
{
	BATCH_LOG("fwq report_batch_finish rel+++++\n");
	input_report_rel(dev, EVENT_TYPE_END_FLAG, 1<<16|handle);
	input_sync(dev);
	BATCH_LOG("fwq report_batch_finish----\n");
}

static int sensorHub_remove(struct platform_device *pdev)
{
	BATCH_LOG("sensorHub_remove\n");
	return 0;
}

static int sensorHub_probe(struct platform_device *pdev)
{
	BATCH_ERR("sensorHub_probe\n");
	return 0;
}

static int sensorHub_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static int sensorHub_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sensorHub_of_match[] = {
	{ .compatible = "mediatek,sensorHub", },
	{},
};
#endif

static struct platform_driver sensorHub_driver = {
	.probe	  = sensorHub_probe,
	.remove	 = sensorHub_remove,
	.suspend    = sensorHub_suspend,
	.resume     = sensorHub_resume,
	.driver = {

		.name  = "sensorHub",
		#ifdef CONFIG_OF
		.of_match_table = sensorHub_of_match,
		#endif
	}
};

static int batch_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	BATCH_LOG(" batch_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_BATCH_NUM; i++) {
		BATCH_LOG(" i=%d\n", i);
		if (0 != batch_init_list[i]) {
			BATCH_LOG(" batch try to init driver %s\n", batch_init_list[i]->name);
			err = batch_init_list[i]->init();
			if (0 == err) {
				BATCH_LOG(" batch real driver %s probe ok\n", batch_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_BATCH_NUM) {
		BATCH_LOG(" batch_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

int  batch_driver_add(struct batch_init_info *obj)
{
	int err = 0;
	int i = 0;

	BATCH_FUN();
	if (!obj) {
		BATCH_ERR("batch driver add fail, batch_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_BATCH_NUM; i++) {
		if (i == 0) {
			BATCH_LOG("register sensorHub driver for the first time\n");
			if (platform_driver_register(&sensorHub_driver))
				BATCH_ERR("failed to register sensorHub driver already exist\n");

		}

		if (NULL == batch_init_list[i]) {
			obj->platform_diver_addr = &sensorHub_driver;
			batch_init_list[i] = obj;
			break;
		}
	}

	if (i >= MAX_CHOOSE_BATCH_NUM) {
		BATCH_LOG("batch driver add err\n");
		err =  -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(batch_driver_add);

static int batch_probe(void)
{

	int err;

	BATCH_LOG("+++++++++++++batch_probe!!\n");

	batch_context_obj = batch_context_alloc_object();
	if (!batch_context_obj) {
		err = -ENOMEM;
		BATCH_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	err = batch_real_driver_init();
	if (err) {
		BATCH_ERR("batch real driver init fail\n");
		goto real_driver_init_fail;
	}

	/* init input dev */
	err = batch_input_init(batch_context_obj);
	if (err) {
		BATCH_ERR("unable to register batch input device!\n");
		goto exit_alloc_input_dev_failed;
	}


	/* add misc dev for sensor hal control cmd */
	err = batch_misc_init(batch_context_obj);
	if (err) {
		BATCH_ERR("unable to register batch misc device!!\n");
		goto exit_err_sysfs;
	}

	err = sysfs_create_group(&batch_context_obj->mdev.this_device->kobj,
			&batch_attribute_group);
	if (err < 0) {
		BATCH_ERR("unable to create batch attribute file\n");
		goto exit_misc_register_failed;
	}

	kobject_uevent(&batch_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	BATCH_LOG("----batch_probe OK !!\n");
	return 0;


exit_misc_register_failed:
	err = misc_deregister(&batch_context_obj->mdev);
	if (err)
		BATCH_ERR("misc_deregister fail: %d\n", err);

exit_err_sysfs:

	if (err) {
		BATCH_ERR("sysfs node creation error\n");
		batch_input_destroy(batch_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(batch_context_obj);
	batch_context_obj = NULL;

exit_alloc_data_failed:
	BATCH_LOG("----batch_probe fail !!!\n");
	return err;
}



static int batch_remove(void)
{
	int err = 0;

	BATCH_FUN(f);
	input_unregister_device(batch_context_obj->idev);
	sysfs_remove_group(&batch_context_obj->idev->dev.kobj,
				&batch_attribute_group);

	err = misc_deregister(&batch_context_obj->mdev);
	if (err)
		BATCH_ERR("misc_deregister fail: %d\n", err);

	kfree(batch_context_obj);

	return 0;
}

static int __init batch_init(void)
{
	BATCH_FUN();

	if (batch_probe()) {
		BATCH_ERR("failed to register batch driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit batch_exit(void)
{
	batch_remove();
	platform_driver_unregister(&sensorHub_driver);
}

late_initcall(batch_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("batch device driver");
MODULE_AUTHOR("Mediatek");


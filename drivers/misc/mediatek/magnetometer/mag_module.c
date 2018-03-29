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

#include "mag.h"
#include "accel.h"

bool mag_success_Flag = false;
EXPORT_SYMBOL(mag_success_Flag);

static int mag_probe(void);
struct mag_context *mag_context_obj = NULL;
static struct mag_init_info *msensor_init_list[MAX_CHOOSE_G_NUM] = {0};

static void initTimer(struct hrtimer *timer, enum hrtimer_restart (*callback)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = callback;
}

static void startTimer(struct hrtimer *timer, int delay_ms, bool first)
{
	struct mag_context *obj = (struct mag_context *)container_of(timer, struct mag_context, hrTimer);

	if (obj == NULL) {
		MAG_ERR("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime = ktime_add_ns(ktime_get(), (int64_t)delay_ms*1000000);
	} else {
		do {
			obj->target_ktime = ktime_add_ns(obj->target_ktime, (int64_t)delay_ms*1000000);
		} while (ktime_to_ns(obj->target_ktime) < ktime_to_ns(ktime_get()));
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}

static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}

static void mag_work_func(struct work_struct *work)
{
	struct mag_context *cxt = NULL;
	struct hwm_sensor_data sensor_data;
	int64_t m_pre_ns, o_pre_ns, cur_ns;
	int64_t delay_ms;
	struct timespec time;
	int err;
	int i;
	int x, y, z, status;

	cxt  = mag_context_obj;
	delay_ms = atomic_read(&cxt->delay);
	memset(&sensor_data, 0, sizeof(sensor_data));
	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	cur_ns = time.tv_sec*1000000000LL+time.tv_nsec;

	for (i = 0; i < MAX_M_V_SENSOR; i++) {
		if (!(cxt->active_data_sensor&(0x01<<i)))
			continue;

		if (ID_M_V_MAGNETIC == i) {
			err = cxt->mag_dev_data.get_data_m(&x, &y, &z, &status);
			if (err) {
				MAG_ERR("get %d data fails!!\n" , i);
				return;
			}
			cxt->drv_data[i].mag_data.values[0] = x;
			cxt->drv_data[i].mag_data.values[1] = y;
			cxt->drv_data[i].mag_data.values[2] = z;
			cxt->drv_data[i].mag_data.status = status;
			m_pre_ns = cxt->drv_data[i].mag_data.time;
			cxt->drv_data[i].mag_data.time = cur_ns;
			if (true ==  cxt->is_first_data_after_enable) {
				m_pre_ns = cur_ns;
				cxt->is_first_data_after_enable = false;
				/* filter -1 value */
				if (MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[0] ||
					MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[1] ||
					MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[2]) {
					MAG_LOG(" read invalid data\n");
					continue;
				}
			}
			while ((cur_ns - m_pre_ns) >= delay_ms*1800000LL) {
				m_pre_ns += delay_ms*1000000LL;
				mag_data_report(MAGNETIC, cxt->drv_data[i].mag_data.values[0],
					cxt->drv_data[i].mag_data.values[1],
					cxt->drv_data[i].mag_data.values[2],
					cxt->drv_data[i].mag_data.status, m_pre_ns);
			}

			mag_data_report(MAGNETIC, cxt->drv_data[i].mag_data.values[0],
				cxt->drv_data[i].mag_data.values[1],
				cxt->drv_data[i].mag_data.values[2],
				cxt->drv_data[i].mag_data.status, cxt->drv_data[i].mag_data.time);

			/* MAG_LOG("mag_type(%d) data[%d,%d,%d]\n" ,i,cxt->drv_data[i].mag_data.values[0], */
		/* cxt->drv_data[i].mag_data.values[1],cxt->drv_data[i].mag_data.values[2]); */
		}

		if (ID_M_V_ORIENTATION == i) {
			err = cxt->mag_dev_data.get_data_o(&x, &y, &z, &status);
			if (err) {
				MAG_ERR("get %d data fails!!\n" , i);
				return;
			}
			cxt->drv_data[i].mag_data.values[0] = x;
			cxt->drv_data[i].mag_data.values[1] = y;
			cxt->drv_data[i].mag_data.values[2] = z;
			cxt->drv_data[i].mag_data.status = status;
			o_pre_ns = cxt->drv_data[i].mag_data.time;
			cxt->drv_data[i].mag_data.time = cur_ns;
			if (true ==  cxt->is_first_data_after_enable) {
				o_pre_ns = cur_ns;
				cxt->is_first_data_after_enable = false;
				/* filter -1 value */
				if (MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[0] ||
				MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[1] ||
				MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[2]) {
					MAG_LOG(" read invalid data\n");
					continue;
				}
			}
			while ((cur_ns - o_pre_ns) >= delay_ms*1800000LL) {
				o_pre_ns += delay_ms*1000000LL;
				mag_data_report(ORIENTATION, cxt->drv_data[i].mag_data.values[0],
					cxt->drv_data[i].mag_data.values[1],
					cxt->drv_data[i].mag_data.values[2],
					cxt->drv_data[i].mag_data.status, o_pre_ns);
			}

			mag_data_report(ORIENTATION, cxt->drv_data[i].mag_data.values[0],
				cxt->drv_data[i].mag_data.values[1],
				cxt->drv_data[i].mag_data.values[2],
				cxt->drv_data[i].mag_data.status, cxt->drv_data[i].mag_data.time);

			/* MAG_LOG("mag_type(%d) data[%d,%d,%d]\n" ,i,cxt->drv_data[i].mag_data.values[0], */
		/* cxt->drv_data[i].mag_data.values[1],cxt->drv_data[i].mag_data.values[2]); */
		}

	}

	if (true == cxt->is_polling_run)
		startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), false);
}

enum hrtimer_restart mag_poll(struct hrtimer *timer)
{
	struct mag_context *obj = (struct mag_context *)container_of(timer, struct mag_context, hrTimer);

	queue_work(obj->mag_workqueue, &obj->report);

	return HRTIMER_NORESTART;
}

static struct mag_context *mag_context_alloc_object(void)
{

	struct mag_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	MAG_LOG("mag_context_alloc_object++++\n");
	if (!obj) {
		MAG_ERR("Alloc magel object error!\n");
		return NULL;
	}

	atomic_set(&obj->delay, 200); /* set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, mag_work_func);
	obj->mag_workqueue = NULL;
	obj->mag_workqueue = create_workqueue("mag_polling");
	if (!obj->mag_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, mag_poll);
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->active_data_sensor = 0;
	obj->active_nodata_sensor = 0;
	obj->is_batch_enable = false;
	mutex_init(&obj->mag_op_mutex);
	MAG_LOG("mag_context_alloc_object----\n");
	return obj;
}
static int mag_enable_data(int handle, int enable)
{
	struct mag_context *cxt = NULL;

	cxt = mag_context_obj;
	if (NULL  == cxt->drv_obj[handle] && NULL == cxt->mag_ctl.m_enable) {
		MAG_ERR("no real mag driver\n");
		return -1;
	}

	if (1 == enable) {
		MAG_LOG("MAG(%d) enable\n", handle);
		cxt->is_first_data_after_enable = true;
		cxt->active_data_sensor |= 1<<handle;
		if (ID_M_V_ORIENTATION == handle) {
			cxt->mag_ctl.o_enable(1);
			cxt->mag_ctl.o_open_report_data(1);
		}
		if (ID_M_V_MAGNETIC == handle) {
			cxt->mag_ctl.m_enable(1);
			cxt->mag_ctl.m_open_report_data(1);
		}

		if ((0 != cxt->active_data_sensor) && (false == cxt->is_polling_run) &&
			(false == cxt->is_batch_enable)) {
			if (false == cxt->mag_ctl.is_report_input_direct) {
				MAG_LOG("MAG(%d)  mod timer\n", handle);
				startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
				cxt->is_polling_run = true;
			}
		}
	}

	if (0 == enable) {
		MAG_LOG("MAG(%d) disable\n", handle);
		cxt->active_data_sensor &= ~(1<<handle);
		if (ID_M_V_ORIENTATION == handle) {
			cxt->mag_ctl.o_enable(0);
			cxt->mag_ctl.o_open_report_data(0);
		}
		if (ID_M_V_MAGNETIC == handle) {
			cxt->mag_ctl.m_enable(0);
			cxt->mag_ctl.m_open_report_data(0);
		}

		if (0 == cxt->active_data_sensor && true == cxt->is_polling_run) {
			if (false == cxt->mag_ctl.is_report_input_direct) {
				MAG_LOG("MAG(%d)  del timer\n", handle);
				cxt->is_polling_run = false;
				smp_mb();/*fo memory barrier*/
				stopTimer(&cxt->hrTimer);
				smp_mb();/*for memory barrier*/
				cancel_work_sync(&cxt->report);
				cxt->drv_data[handle].mag_data.values[0] = MAG_INVALID_VALUE;
				cxt->drv_data[handle].mag_data.values[1] = MAG_INVALID_VALUE;
				cxt->drv_data[handle].mag_data.values[2] = MAG_INVALID_VALUE;
			}
		}

	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t mag_show_magdev(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG("sensor test: mag function!\n");
	return len;
}
/*----------------------------------------------------------------------------*/

static ssize_t mag_store_oactive(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;

	MAG_LOG("mag_store_oactive buf=%s\n", buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (NULL == cxt->mag_ctl.o_enable) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl o-enable NULL\n");
		return count;
	}

	if (!strncmp(buf, "1", 1))
		mag_enable_data(ID_M_V_ORIENTATION, 1);
	else if (!strncmp(buf, "0", 1))
		mag_enable_data(ID_M_V_ORIENTATION, 0);
	else
		MAG_ERR(" mag_store_oactive error !!\n");

	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_oactive done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t mag_show_oactive(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mag_context *cxt = NULL;
	int div = 0;

	cxt = mag_context_obj;
	div = cxt->mag_dev_data.div_o;
	ACC_LOG("acc mag_dev_data o_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t mag_store_active(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;

	MAG_LOG("mag_store_active buf=%s\n", buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (NULL == cxt->mag_ctl.m_enable) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl path is NULL\n");
		return count;
	}

	if (!strncmp(buf, "1", 1))
		mag_enable_data(ID_M_V_MAGNETIC, 1);
	else if (!strncmp(buf, "0", 1))
		mag_enable_data(ID_M_V_MAGNETIC, 0);
	else
		MAG_ERR(" mag_store_active error !!\n");

	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t mag_show_active(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct mag_context *cxt = NULL;
	int div = 0;

	cxt = mag_context_obj;
	div = cxt->mag_dev_data.div_m;
	ACC_LOG("acc mag_dev_data m_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t mag_store_odelay(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int delay = 0;
	int mdelay = 0;
	int ret = 0;
	struct mag_context *cxt = NULL;

	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (NULL == cxt->mag_ctl.o_set_delay) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl o_delay NULL\n");
		return count;
	}
	MAG_LOG(" mag_odelay ++\n");

	ret = kstrtoint(buf, 10, &delay);
	if (ret != 0) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_ERR("invalid format!!\n");
		return count;
	}

	if (false == cxt->mag_ctl.is_report_input_direct) {
		mdelay = (int)delay/1000/1000;
		atomic_set(&mag_context_obj->delay, mdelay);
	}

	cxt->mag_ctl.o_set_delay(delay);
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_odelay %d ns done\n", delay);
	return count;
}

static ssize_t mag_show_odelay(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_delay(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int64_t delay = 0;
	int64_t mdelay = 0;
	int ret = 0;
	struct mag_context *cxt = NULL;

	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (NULL == cxt->mag_ctl.m_set_delay) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl m_delay NULL\n");
		return count;
	}

	MAG_LOG(" mag_delay ++\n");

	ret = kstrtoll(buf, 10, &delay);
	if (ret != 0) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_ERR("invalid format!!\n");
		return count;
	}

	if (false == cxt->mag_ctl.is_report_input_direct) {
		mdelay = delay;
		do_div(mdelay, 1000000);
		atomic_set(&mag_context_obj->delay, mdelay);
	}
	cxt->mag_ctl.m_set_delay(delay);
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_delay %lld ns done\n", delay);
	return count;
}

static ssize_t mag_show_delay(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_batch(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;

	MAG_LOG("mag_store_batch buf=%s\n", buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (cxt->mag_ctl.is_support_batch) {
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			if (true == cxt->is_polling_run) {
				cxt->is_polling_run = false;
				smp_mb();  /* for memory barrier */
				stopTimer(&cxt->hrTimer);
				smp_mb();  /* for memory barrier */
				cancel_work_sync(&cxt->report);
				cxt->drv_data[ID_M_V_MAGNETIC].mag_data.values[0] = MAG_INVALID_VALUE;
				cxt->drv_data[ID_M_V_MAGNETIC].mag_data.values[1] = MAG_INVALID_VALUE;
				cxt->drv_data[ID_M_V_MAGNETIC].mag_data.values[2] = MAG_INVALID_VALUE;
			}
		 } else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			if (false == cxt->is_polling_run) {
				if (false == cxt->mag_ctl.is_report_input_direct) {
					startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
					cxt->is_polling_run = true;
				}
			}
		} else
			MAG_ERR(" mag_store_batch error !!\n");
	} else
		MAG_LOG(" mag_store_batch not supported\n");

	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_batch done: %d\n", cxt->is_batch_enable);
	return count;
}


static ssize_t mag_show_batch(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_flush(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return count;
}
/* need work around again */
static ssize_t mag_show_sensordevnum(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned int devnum;
	int ret;
	struct mag_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = mag_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	ret = sscanf(devname+5, "%d", &devnum);
	return snprintf(buf, PAGE_SIZE, "%d\n", devnum);
}


static ssize_t mag_show_flush(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_obatch(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;

	MAG_LOG("mag_store_obatch buf=%s\n", buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (cxt->mag_ctl.is_support_batch) {
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			if (true == cxt->is_polling_run) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data[ID_M_V_ORIENTATION].mag_data.values[0] = MAG_INVALID_VALUE;
				cxt->drv_data[ID_M_V_ORIENTATION].mag_data.values[1] = MAG_INVALID_VALUE;
				cxt->drv_data[ID_M_V_ORIENTATION].mag_data.values[2] = MAG_INVALID_VALUE;
			}
		 } else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			if (false == cxt->is_polling_run) {
				if (false == cxt->mag_ctl.is_report_input_direct && 0 !=
					(cxt->active_data_sensor&ID_M_V_ORIENTATION)) {
					startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
					cxt->is_polling_run = true;
				}
			}
		} else
			MAG_ERR(" mag_store_obatch error !!\n");
	} else
		MAG_LOG(" mag_store_obatch not supported\n");

	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_obatch done: %d\n", cxt->is_batch_enable);
	return count;

}


static ssize_t mag_show_obatch(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_oflush(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return count;
}


static ssize_t mag_show_oflush(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	MAG_LOG(" not support now\n");
	return len;
}


int mag_attach(int sensor, struct mag_drv_obj *obj)
{
	int err = 0;

	MAG_FUN();
	mag_context_obj->drv_obj[sensor] = kzalloc(sizeof(struct mag_drv_obj), GFP_KERNEL);
	if (mag_context_obj->drv_obj[sensor] == NULL) {
		err = -EPERM;
		MAG_ERR(" mag attatch alloc fail\n");
		return err;
	}

	memcpy(mag_context_obj->drv_obj[sensor], obj, sizeof(*obj));
	if (NULL == mag_context_obj->drv_obj[sensor]) {
		err =  -1;
		MAG_ERR(" mag attatch fail\n");
	}
	return err;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(mag_attach);


static int msensor_remove(struct platform_device *pdev)
{
	MAG_LOG("msensor_remove\n");
	return 0;
}

static int msensor_probe(struct platform_device *pdev)
{
	MAG_LOG("msensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id msensor_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver msensor_driver = {
	.probe	  = msensor_probe,
	.remove	 = msensor_remove,
	.driver = {

		.name  = "msensor",
		#ifdef CONFIG_OF
		.of_match_table = msensor_of_match,
		#endif
	}
};

static int mag_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	MAG_LOG(" mag_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		MAG_LOG(" i=%d\n", i);
		if (0 != msensor_init_list[i]) {
			MAG_LOG(" mag try to init driver %s\n", msensor_init_list[i]->name);
			err = msensor_init_list[i]->init();
			if (0 == err) {
				MAG_LOG(" mag real driver %s probe ok\n", msensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_G_NUM) {
		MAG_LOG(" mag_real_driver_init fail\n");
		err =  -1;
	}
	return err;
}

int mag_driver_add(struct mag_init_info *obj)
{
	int err = 0;
	int i = 0;

	MAG_FUN();
	if (!obj) {
		MAG_ERR("MAG driver add fail, mag_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		if ((i == 0) && (NULL == msensor_init_list[0])) {
			MAG_LOG("register mensor driver for the first time\n");
			if (platform_driver_register(&msensor_driver))
				MAG_ERR("failed to register msensor driver already exist\n");
		}
		if (NULL == msensor_init_list[i]) {
			obj->platform_diver_addr = &msensor_driver;
			msensor_init_list[i] = obj;
			break;
		}
	}

	if (i >= MAX_CHOOSE_G_NUM) {
		MAG_ERR("MAG driver add err\n");
		err =  -1;
	}

	if (mag_success_Flag == false) {
		if (mag_probe()) {
			MAG_ERR("failed to register mag driver\n");
			return -ENODEV;
		}
		mag_success_Flag = true;
	}
	return err;
}
EXPORT_SYMBOL_GPL(mag_driver_add);

static int mag_misc_init(struct mag_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = MAG_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		MAG_ERR("unable to register mag misc device!!\n");

	return err;
}

/*
static void mag_input_destroy(struct mag_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}
*/

static int mag_input_init(struct mag_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = MAG_INPUTDEV_NAME;
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_MAGEL_UPDATE);
	input_set_capability(dev, EV_REL, EVENT_TYPE_MAG_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_MAG_TIMESTAMP_LO);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ORIENT_UPDATE);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ORIENT_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ORIENT_TIMESTAMP_LO);

	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_O_UPDATE);

	input_set_abs_params(dev, EVENT_TYPE_MAGEL_X, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_Y, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_Z, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_STATUS, MAG_STATUS_MIN, MAG_STATUS_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_X, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_Y, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_Z, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_STATUS, MAG_STATUS_MIN, MAG_STATUS_MAX, 0, 0);

	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(magdev,		S_IWUSR | S_IRUGO, mag_show_magdev, NULL);
DEVICE_ATTR(magactive,	 S_IWUSR | S_IRUGO, mag_show_active, mag_store_active);
DEVICE_ATTR(magdelay,	  S_IWUSR | S_IRUGO, mag_show_delay,  mag_store_delay);
DEVICE_ATTR(magoactive,	 S_IWUSR | S_IRUGO, mag_show_oactive, mag_store_oactive);
DEVICE_ATTR(magodelay,	  S_IWUSR | S_IRUGO, mag_show_odelay,  mag_store_odelay);
DEVICE_ATTR(magbatch,	S_IWUSR | S_IRUGO, mag_show_batch,  mag_store_batch);
DEVICE_ATTR(magflush,		S_IWUSR | S_IRUGO, mag_show_flush,  mag_store_flush);
DEVICE_ATTR(magobatch,	S_IWUSR | S_IRUGO, mag_show_obatch,  mag_store_obatch);
DEVICE_ATTR(magoflush,	S_IWUSR | S_IRUGO, mag_show_oflush,  mag_store_oflush);
DEVICE_ATTR(magdevnum,	S_IWUSR | S_IRUGO, mag_show_sensordevnum,  NULL);

static struct attribute *mag_attributes[] = {
	&dev_attr_magdev.attr,
	&dev_attr_magactive.attr,
	&dev_attr_magdelay.attr,
	&dev_attr_magbatch.attr,
	&dev_attr_magflush.attr,
	&dev_attr_magoactive.attr,
	&dev_attr_magodelay.attr,
	&dev_attr_magobatch.attr,
	&dev_attr_magoflush.attr,
	&dev_attr_magdevnum.attr,
	NULL
};

static struct attribute_group mag_attribute_group = {
	.attrs = mag_attributes
};


int mag_register_data_path(struct mag_data_path *data)
{
	struct mag_context *cxt = NULL;

	cxt = mag_context_obj;
	cxt->mag_dev_data.div_m = data->div_m;
	cxt->mag_dev_data.div_o = data->div_o;
	cxt->mag_dev_data.get_data_m = data->get_data_m;
	cxt->mag_dev_data.get_data_o = data->get_data_o;
	cxt->mag_dev_data.get_raw_data = data->get_raw_data;
	MAG_LOG("mag register data path div_o: %d\n", cxt->mag_dev_data.div_o);
	MAG_LOG("mag register data path div_m: %d\n", cxt->mag_dev_data.div_m);

	return 0;
}
EXPORT_SYMBOL_GPL(mag_register_data_path);

int mag_register_control_path(struct mag_control_path *ctl)
{
	struct mag_context *cxt = NULL;
	int err = 0;

	cxt = mag_context_obj;
	cxt->mag_ctl.m_set_delay = ctl->m_set_delay;
	cxt->mag_ctl.m_enable = ctl->m_enable;
	cxt->mag_ctl.m_open_report_data = ctl->m_open_report_data;
	cxt->mag_ctl.o_set_delay = ctl->o_set_delay;
	cxt->mag_ctl.o_open_report_data = ctl->o_open_report_data;
	cxt->mag_ctl.o_enable = ctl->o_enable;
	cxt->mag_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->mag_ctl.is_support_batch = ctl->is_support_batch;
	cxt->mag_ctl.is_use_common_factory = ctl->is_use_common_factory;

	if (NULL == cxt->mag_ctl.m_set_delay || NULL == cxt->mag_ctl.m_enable
		|| NULL == cxt->mag_ctl.m_open_report_data
		|| NULL == cxt->mag_ctl.o_set_delay || NULL == cxt->mag_ctl.o_open_report_data
		|| NULL == cxt->mag_ctl.o_enable) {
		MAG_LOG("mag register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = mag_misc_init(mag_context_obj);
	if (err) {
		MAG_ERR("unable to register mag misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&mag_context_obj->mdev.this_device->kobj,
			&mag_attribute_group);
	if (err < 0) {
		MAG_ERR("unable to create mag attribute file\n");
		return -3;
	}

	kobject_uevent(&mag_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}
EXPORT_SYMBOL_GPL(mag_register_control_path);
static int x1, y1, z1;
static long pc;
static long count;

static int check_repeat_data(int x, int y, int z)
{
	if ((x1 == x) && (y1 == y) && (z1 == z))
		pc++;
	else
		pc = 0;

	x1 = x; y1 = y; z1 = z;

	if (pc > 100) {
		MAG_ERR("Mag sensor output repeat data\n");
		pc = 0;
	}

	return 0;
}

static int check_abnormal_data(int x, int y, int z, int status)
{
	long total;
	struct mag_context *cxt = mag_context_obj;

	total = (x*x + y*y + z*z)/(cxt->mag_dev_data.div_m * cxt->mag_dev_data.div_m);
	if ((total < 100) || (total > 10000)) {
		if (count % 10 == 0)
			MAG_ERR("mag sensor abnormal data: x=%d,y=%d,z=%d, status=%d\n", x, y, z, status);
		count++;
		if (count > 1000)
			count = 0;
	}

	return 0;
}

int mag_data_report(enum MAG_TYPE type, int x, int y, int z, int status, int64_t nt)
{
	/* MAG_LOG("update!valus: %d, %d, %d, %d\n" , x, y, z, status); */
	struct mag_context *cxt = NULL;

	check_repeat_data(x, y, z);
	check_abnormal_data(x, y, z, status);

	cxt = mag_context_obj;
	if (MAGNETIC == type) {
		input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_STATUS, status);
		input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_X, x);
		input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_Y, y);
		input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_Z, z);
		input_report_rel(cxt->idev, EVENT_TYPE_MAGEL_UPDATE, 1);
	    input_report_rel(cxt->idev, EVENT_TYPE_MAG_TIMESTAMP_HI, nt >> 32);
	    input_report_rel(cxt->idev, EVENT_TYPE_MAG_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
		input_sync(cxt->idev);
	}

	if (ORIENTATION == type) {
		input_report_abs(cxt->idev, EVENT_TYPE_O_STATUS, status);
		input_report_abs(cxt->idev, EVENT_TYPE_O_X, x);
		input_report_abs(cxt->idev, EVENT_TYPE_O_Y, y);
		input_report_abs(cxt->idev, EVENT_TYPE_O_Z, z);
		input_report_rel(cxt->idev, EVENT_TYPE_O_UPDATE, 1);
	    input_report_rel(cxt->idev, EVENT_TYPE_ORIENT_TIMESTAMP_HI, nt >> 32);
	    input_report_rel(cxt->idev, EVENT_TYPE_ORIENT_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
		input_sync(cxt->idev);
	}

	return 0;
}

static int mag_probe(void)
{
	int err;

	MAG_LOG("+++++++++++++mag_probe!!\n");
	mag_context_obj = mag_context_alloc_object();
	if (!mag_context_obj) {
		err = -ENOMEM;
		MAG_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real mageleration driver */
	err = mag_real_driver_init();
	if (err) {
		MAG_ERR("mag_real_driver_init fail\n");
		goto real_driver_init_fail;
	}

	err = mag_factory_device_init();
	if (err)
		MAG_ERR("mag_factory_device_init fail\n");
	/* init input dev */
	err = mag_input_init(mag_context_obj);
	if (err) {
		MAG_ERR("unable to register mag input device!\n");
		goto exit_alloc_input_dev_failed;
	}


	MAG_LOG("----magel_probe OK !!\n");
	return 0;

real_driver_init_fail:
exit_alloc_input_dev_failed:
	destroy_workqueue(mag_context_obj->mag_workqueue);
	del_timer(&mag_context_obj->timer);
	kfree(mag_context_obj);
	mag_context_obj = NULL;

exit_alloc_data_failed:

	MAG_ERR("----magel_probe fail !!!\n");
	return err;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAGELEROMETER device driver");
MODULE_AUTHOR("Mediatek");


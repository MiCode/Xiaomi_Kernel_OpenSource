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

#include "inc/gyroscope.h"

struct gyro_context *gyro_context_obj = NULL;
static struct platform_device *pltfm_dev;

static struct gyro_init_info *gyroscope_init_list[MAX_CHOOSE_GYRO_NUM] = {0};

static int64_t getCurNS(void)
{
	int64_t ns;
	struct timespec time;

	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	ns = time.tv_sec * 1000000000LL + time.tv_nsec;

	return ns;
}

static void initTimer(struct hrtimer *timer, enum hrtimer_restart (*callback)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = callback;
}

static void startTimer(struct hrtimer *timer, int delay_ms, bool first)
{
	struct gyro_context *obj = (struct gyro_context *)container_of(timer, struct gyro_context, hrTimer);
	static int count;

	if (obj == NULL) {
		GYRO_ERR("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime = ktime_add_ns(ktime_get(), (int64_t)delay_ms*1000000);
		/* GYRO_LOG("%d, cur_nt = %lld, delay_ms = %d, target_nt = %lld\n", count,
			getCurNT(), delay_ms, ktime_to_us(obj->target_ktime)); */
		count = 0;
	} else {
		do {
			obj->target_ktime = ktime_add_ns(obj->target_ktime, (int64_t)delay_ms*1000000);
		} while (ktime_to_ns(obj->target_ktime) < ktime_to_ns(ktime_get()));
		/* GYRO_LOG("%d, cur_nt = %lld, delay_ms = %d, target_nt = %lld\n", count,
			getCurNT(), delay_ms, ktime_to_us(obj->target_ktime)); */
		count++;
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}

static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}

static void gyro_work_func(struct work_struct *work)
{

	struct gyro_context *cxt = NULL;
	int x, y, z, status;
	int64_t pre_ns, cur_ns;
	int64_t delay_ms;
	int err = 0;

	cxt  = gyro_context_obj;
	delay_ms = atomic_read(&cxt->delay);

	if (NULL == cxt->gyro_data.get_data) {
		GYRO_ERR("gyro driver not register data path\n");
		return;
	}

	cur_ns = getCurNS();

    /* add wake lock to make sure data can be read before system suspend */
	err = cxt->gyro_data.get_data(&x, &y, &z, &status);

	if (err) {
		GYRO_ERR("get gyro data fails!!\n");
		goto gyro_loop;
	} else {
			cxt->drv_data.gyro_data.values[0] = x+cxt->cali_sw[0];
			cxt->drv_data.gyro_data.values[1] = y+cxt->cali_sw[1];
			cxt->drv_data.gyro_data.values[2] = z+cxt->cali_sw[2];
			cxt->drv_data.gyro_data.status = status;
			pre_ns = cxt->drv_data.gyro_data.time;
			cxt->drv_data.gyro_data.time = cur_ns;
	 }

	if (true ==  cxt->is_first_data_after_enable) {
		pre_ns = cur_ns;
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
	    if (GYRO_INVALID_VALUE == cxt->drv_data.gyro_data.values[0] ||
			     GYRO_INVALID_VALUE == cxt->drv_data.gyro_data.values[1] ||
			     GYRO_INVALID_VALUE == cxt->drv_data.gyro_data.values[2]) {
			GYRO_LOG(" read invalid data\n");
			goto gyro_loop;
	    }
	}

	/* GYRO_LOG("gyro data[%d,%d,%d]\n" ,cxt->drv_data.gyro_data.values[0], */
	/* cxt->drv_data.gyro_data.values[1],cxt->drv_data.gyro_data.values[2]); */

	while ((cur_ns - pre_ns) >= delay_ms*1800000LL) {
		pre_ns += delay_ms*1000000LL;
		gyro_data_report(cxt->drv_data.gyro_data.values[0],
			cxt->drv_data.gyro_data.values[1], cxt->drv_data.gyro_data.values[2],
			cxt->drv_data.gyro_data.status, pre_ns);
	}

	gyro_data_report(cxt->drv_data.gyro_data.values[0],
		cxt->drv_data.gyro_data.values[1], cxt->drv_data.gyro_data.values[2],
		cxt->drv_data.gyro_data.status, cxt->drv_data.gyro_data.time);

gyro_loop:
	if (true == cxt->is_polling_run)
		startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), false);
}

enum hrtimer_restart gyro_poll(struct hrtimer *timer)
{
	struct gyro_context *obj = (struct gyro_context *)container_of(timer, struct gyro_context, hrTimer);

	queue_work(obj->gyro_workqueue, &obj->report);

	/* GYRO_LOG("cur_nt = %lld\n", getCurNT()); */

	return HRTIMER_NORESTART;
}

static struct gyro_context *gyro_context_alloc_object(void)
{

	struct gyro_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	GYRO_LOG("gyro_context_alloc_object++++\n");
	if (!obj) {
		GYRO_ERR("Alloc gyro object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200); /*5Hz,  set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, gyro_work_func);
	obj->gyro_workqueue = NULL;
	obj->gyro_workqueue = create_workqueue("gyro_polling");
	if (!obj->gyro_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, gyro_poll);
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	obj->cali_sw[GYRO_AXIS_X] = 0;
	obj->cali_sw[GYRO_AXIS_Y] = 0;
	obj->cali_sw[GYRO_AXIS_Z] = 0;
	mutex_init(&obj->gyro_op_mutex);
	GYRO_LOG("gyro_context_alloc_object----\n");
	return obj;
}

static int gyro_real_enable(int enable)
{
	int err = 0;
	struct gyro_context *cxt = NULL;

	cxt = gyro_context_obj;

	if (1 == enable) {
		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->gyro_ctl.enable_nodata(1);
			if (err) {
				err = cxt->gyro_ctl.enable_nodata(1);
				if (err) {
					err = cxt->gyro_ctl.enable_nodata(1);
					if (err)
						GYRO_ERR("gyro enable(%d) err 3 timers = %d\n", enable, err);
				}
			}
			GYRO_LOG("gyro real enable\n");
		}
	}

	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->gyro_ctl.enable_nodata(0);
			if (err)
				GYRO_ERR("gyro enable(%d) err = %d\n", enable, err);

		GYRO_LOG("gyro real disable\n");
		}
	}

	return err;
}
static int gyro_enable_data(int enable)
{
	struct gyro_context *cxt = NULL;

	cxt = gyro_context_obj;
	if (NULL  == cxt->gyro_ctl.open_report_data) {
		GYRO_ERR("no gyro control path\n");
		return -1;
	}

	if (1 == enable) {
		GYRO_LOG("gyro enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->gyro_ctl.open_report_data(1);
		gyro_real_enable(enable);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->gyro_ctl.is_report_input_direct) {
				startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
				cxt->is_polling_run = true;
			}
		}
	}

	if (0 == enable) {
		GYRO_LOG("gyro disable\n");

		cxt->is_active_data = false;
		cxt->gyro_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->gyro_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
					smp_mb(); /* for memory barrier */
					stopTimer(&cxt->hrTimer);
					smp_mb();/* for memory barrier */
					cancel_work_sync(&cxt->report);
					cxt->drv_data.gyro_data.values[0] = GYRO_INVALID_VALUE;
					cxt->drv_data.gyro_data.values[1] = GYRO_INVALID_VALUE;
					cxt->drv_data.gyro_data.values[2] = GYRO_INVALID_VALUE;
			}
		}
		gyro_real_enable(enable);
	}

	return 0;
}

int gyro_enable_nodata(int enable)
{
	struct gyro_context *cxt = NULL;

	cxt = gyro_context_obj;
	if (NULL  == cxt->gyro_ctl.enable_nodata) {
		GYRO_ERR("gyro_enable_nodata:gyro ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	gyro_real_enable(enable);
	return 0;
}


static ssize_t gyro_show_enable_nodata(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	GYRO_LOG(" not support now\n");
	return len;
}

static ssize_t gyro_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct gyro_context *cxt = NULL;

	GYRO_LOG("gyro_store_enable nodata buf=%s\n", buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if (NULL == cxt->gyro_ctl.enable_nodata) {
		GYRO_LOG("gyro_ctl enable nodata NULL\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		gyro_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		gyro_enable_nodata(0);
	else
		GYRO_ERR(" gyro_store enable nodata cmd error !!\n");

	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	return count;
}

static ssize_t gyro_store_active(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct gyro_context *cxt = NULL;

	GYRO_LOG("gyro_store_active buf=%s\n", buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if (NULL == cxt->gyro_ctl.open_report_data) {
		GYRO_LOG("gyro_ctl enable NULL\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		gyro_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		gyro_enable_data(0);
	else
		GYRO_ERR(" gyro_store_active error !!\n");

	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	GYRO_LOG(" gyro_store_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t gyro_show_active(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct gyro_context *cxt = NULL;
	int div = 0;

	cxt = gyro_context_obj;

	GYRO_LOG("gyro show active not support now\n");
	div = cxt->gyro_data.vender_div;
	GYRO_LOG("gyro vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t gyro_store_delay(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int64_t delay;
	int64_t mdelay = 0;
	int ret = 0;
	struct gyro_context *cxt = NULL;

	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if (NULL == cxt->gyro_ctl.set_delay) {
		GYRO_LOG("gyro_ctl set_delay NULL\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
		return count;
	}

	ret = kstrtoll(buf, 10, &delay);
	if (ret != 0) {
		GYRO_ERR("invalid format!!\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
		return count;
	}

	if (false == cxt->gyro_ctl.is_report_input_direct) {
		mdelay = delay;
		do_div(mdelay, 1000000);
		atomic_set(&gyro_context_obj->delay, mdelay);
	}
	cxt->gyro_ctl.set_delay(delay);
	GYRO_LOG(" gyro_delay %lld ns\n", delay);
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	return count;
}

static ssize_t gyro_show_delay(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int len = 0;

	GYRO_LOG(" not support now\n");
	return len;
}

static ssize_t gyro_store_batch(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct gyro_context *cxt = NULL;

	/* GYRO_LOG("gyro_store_batch buf=%s\n", buf); */
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if (cxt->gyro_ctl.is_support_batch) {
		GYRO_LOG("gyro_store_batch buf=%s\n", buf);
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			if (true == cxt->is_polling_run) {
				cxt->is_polling_run = false;
				smp_mb();  /* for memory barrier */
				stopTimer(&cxt->hrTimer);
				smp_mb();  /* for memory barrier */
				cancel_work_sync(&cxt->report);
				cxt->drv_data.gyro_data.values[0] = GYRO_INVALID_VALUE;
				cxt->drv_data.gyro_data.values[1] = GYRO_INVALID_VALUE;
				cxt->drv_data.gyro_data.values[2] = GYRO_INVALID_VALUE;
			}
		} else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			if (false == cxt->is_polling_run) {
				if (false == cxt->gyro_ctl.is_report_input_direct) {
					startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
					cxt->is_polling_run = true;
				}
			}
		} else
			GYRO_ERR(" gyro_store_batch error !!\n");
	} else
		GYRO_LOG(" gyro_store_batch not support\n");

	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	/* GYRO_LOG(" gyro_store_batch done: %d\n", cxt->is_batch_enable); */

	return count;
}

static ssize_t gyro_show_batch(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t gyro_store_flush(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	/* mutex_lock(&gyro_context_obj->gyro_op_mutex); */
	/* struct gyro_context *devobj = (struct gyro_context*)dev_get_drvdata(dev); */
	/* do read FIFO data function and report data immediately */
	/* mutex_unlock(&gyro_context_obj->gyro_op_mutex); */
	return count;
}

static ssize_t gyro_show_flush(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
/* need work around again */
static ssize_t gyro_show_devnum(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned int devnum;
	const char *devname = NULL;
	int ret = 0;
	struct input_handle *handle;

	list_for_each_entry(handle, &gyro_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	ret = sscanf(devname+5, "%d", &devnum);
	return snprintf(buf, PAGE_SIZE, "%d\n", devnum);
}
static int gyroscope_remove(struct platform_device *pdev)
{
	GYRO_LOG("gyroscope_remove\n");
	return 0;
}

static int gyroscope_probe(struct platform_device *pdev)
{
	GYRO_LOG("gyroscope_probe\n");
	pltfm_dev = pdev;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gyroscope_of_match[] = {
	{ .compatible = "mediatek,gyroscope", },
	{},
};
#endif

static struct platform_driver gyroscope_driver = {
	.probe      = gyroscope_probe,
	.remove     = gyroscope_remove,
	.driver = {
		.name  = "gyroscope",
		#ifdef CONFIG_OF
		.of_match_table = gyroscope_of_match,
		#endif
	}
};

static int gyro_real_driver_init(struct platform_device *pdev)
{
	int i = 0;
	int err = 0;

	GYRO_LOG("gyro_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_GYRO_NUM; i++) {
		GYRO_LOG("i=%d\n", i);
		if (0 != gyroscope_init_list[i]) {
			GYRO_LOG("gyro try to init driver %s\n", gyroscope_init_list[i]->name);
			err = gyroscope_init_list[i]->init(pdev);
			if (0 == err) {
				GYRO_LOG("gyro real driver %s probe ok\n", gyroscope_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_GYRO_NUM) {
		GYRO_LOG(" gyro_real_driver_init fail\n");
		err =  -1;
	}
	return err;
}

int gyro_driver_add(struct gyro_init_info *obj)
{
	int err = 0;
	int i = 0;

	if (!obj) {
		GYRO_ERR("gyro driver add fail, gyro_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_GYRO_NUM; i++) {
		if ((i == 0) && (NULL == gyroscope_init_list[0])) {
			GYRO_LOG("register gyro driver for the first time\n");
			if (platform_driver_register(&gyroscope_driver))
				GYRO_ERR("failed to register gyro driver already exist\n");
		}

	    if (NULL == gyroscope_init_list[i]) {
			obj->platform_diver_addr = &gyroscope_driver;
			gyroscope_init_list[i] = obj;
			break;
	    }
	}

	if (i >= MAX_CHOOSE_GYRO_NUM) {
		GYRO_ERR("gyro driver add err\n");
		err =  -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(gyro_driver_add);

static int gyro_misc_init(struct gyro_context *cxt)
{
	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = GYRO_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		GYRO_ERR("unable to register gyro misc device!!\n");

	return err;
}

/* static void gyro_input_destroy(struct gyro_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
} */

static int gyro_input_init(struct gyro_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GYRO_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GYRO_UPDATE);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GYRO_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GYRO_TIMESTAMP_LO);

	input_set_abs_params(dev, EVENT_TYPE_GYRO_X, GYRO_VALUE_MIN, GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GYRO_Y, GYRO_VALUE_MIN, GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GYRO_Z, GYRO_VALUE_MIN, GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GYRO_STATUS, GYRO_STATUS_MIN, GYRO_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(gyroenablenodata,     S_IWUSR | S_IRUGO, gyro_show_enable_nodata, gyro_store_enable_nodata);
DEVICE_ATTR(gyroactive,     S_IWUSR | S_IRUGO, gyro_show_active, gyro_store_active);
DEVICE_ATTR(gyrodelay,      S_IWUSR | S_IRUGO, gyro_show_delay,  gyro_store_delay);
DEVICE_ATTR(gyrobatch,     S_IWUSR | S_IRUGO, gyro_show_batch, gyro_store_batch);
DEVICE_ATTR(gyroflush,      S_IWUSR | S_IRUGO, gyro_show_flush,  gyro_store_flush);
DEVICE_ATTR(gyrodevnum,      S_IWUSR | S_IRUGO, gyro_show_devnum,  NULL);

static struct attribute *gyro_attributes[] = {
	&dev_attr_gyroenablenodata.attr,
	&dev_attr_gyroactive.attr,
	&dev_attr_gyrodelay.attr,
	&dev_attr_gyrobatch.attr,
	&dev_attr_gyroflush.attr,
	&dev_attr_gyrodevnum.attr,
	NULL
};

static struct attribute_group gyro_attribute_group = {
	.attrs = gyro_attributes
};

int gyro_register_data_path(struct gyro_data_path *data)
{
	struct gyro_context *cxt = NULL;

	cxt = gyro_context_obj;
	cxt->gyro_data.get_data = data->get_data;
	cxt->gyro_data.vender_div = data->vender_div;
	cxt->gyro_data.get_raw_data = data->get_raw_data;
	GYRO_LOG("gyro register data path vender_div: %d\n", cxt->gyro_data.vender_div);
	if (NULL == cxt->gyro_data.get_data) {
		GYRO_LOG("gyro register data path fail\n");
		return -1;
	}
	return 0;
}

int gyro_register_control_path(struct gyro_control_path *ctl)
{
	struct gyro_context *cxt = NULL;
	int err = 0;

	cxt = gyro_context_obj;
	cxt->gyro_ctl.set_delay = ctl->set_delay;
	cxt->gyro_ctl.open_report_data = ctl->open_report_data;
	cxt->gyro_ctl.enable_nodata = ctl->enable_nodata;
	cxt->gyro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->gyro_ctl.gyro_calibration = ctl->gyro_calibration;
	cxt->gyro_ctl.is_use_common_factory = ctl->is_use_common_factory;
	cxt->gyro_ctl.is_report_input_direct = ctl->is_report_input_direct;
	if (NULL == cxt->gyro_ctl.set_delay || NULL == cxt->gyro_ctl.open_report_data
		|| NULL == cxt->gyro_ctl.enable_nodata) {
		GYRO_LOG("gyro register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = gyro_misc_init(gyro_context_obj);
	if (err) {
		GYRO_ERR("unable to register gyro misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&gyro_context_obj->mdev.this_device->kobj,
			&gyro_attribute_group);
	if (err < 0) {
		GYRO_ERR("unable to create gyro attribute file\n");
		return -3;
	}

	kobject_uevent(&gyro_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int x_t = 0;
int y_t = 0;
int z_t = 0;
long pc = 0;

static int check_repeat_data(int x, int y, int z)
{
	if ((x_t == x) && (y_t == y) && (z_t == z))
		pc++;
	else
		pc = 0;

	x_t = x; y_t = y; z_t = z;

	if (pc > 100) {
		GYRO_ERR("Gyro sensor output repeat data\n");
		pc = 0;
	}

	return 0;
}

int gyro_data_report(int x, int y, int z, int status, int64_t nt)
{
	struct gyro_context *cxt = NULL;
	int err = 0;

	check_repeat_data(x, y, z);
	cxt = gyro_context_obj;
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_X, x);
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_Y, y);
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_Z, z);
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_STATUS, status);
	input_report_rel(cxt->idev, EVENT_TYPE_GYRO_UPDATE, 1);
	input_report_rel(cxt->idev, EVENT_TYPE_GYRO_TIMESTAMP_HI, nt >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_GYRO_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
	input_sync(cxt->idev);
	return err;
}

static int gyro_probe(void)
{

	int err;

	GYRO_LOG("+++++++++++++gyro_probe!!\n");

	gyro_context_obj = gyro_context_alloc_object();
	if (!gyro_context_obj) {
		err = -ENOMEM;
		GYRO_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real gyroeleration driver */
	err = gyro_real_driver_init(pltfm_dev);
	if (err) {
		GYRO_ERR("gyro real driver init fail\n");
		goto real_driver_init_fail;
	}

	err = gyro_factory_device_init();
	if (err)
		GYRO_ERR("gyro_factory_device_init fail\n");

	/* init input dev */
	err = gyro_input_init(gyro_context_obj);
	if (err) {
		GYRO_ERR("unable to register gyro input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	GYRO_LOG("----gyro_probe OK !!\n");
	return 0;

	/* Structurally dead code (UNREACHABLE) */
	/* if (err) {
		GYRO_ERR("sysfs node creation error\n");
		gyro_input_destroy(gyro_context_obj);
	} */

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(gyro_context_obj);

exit_alloc_data_failed:
	GYRO_ERR("----gyro_probe fail !!!\n");
	return err;
}

static int gyro_remove(void)
{
	int err = 0;

	input_unregister_device(gyro_context_obj->idev);
	sysfs_remove_group(&gyro_context_obj->idev->dev.kobj,
				&gyro_attribute_group);
	err = misc_deregister(&gyro_context_obj->mdev);
	if (err)
		GYRO_ERR("misc_deregister fail: %d\n", err);

	kfree(gyro_context_obj);

	return 0;
}

static int __init gyro_init(void)
{
	GYRO_LOG("gyro_init\n");

	if (gyro_probe()) {
		GYRO_ERR("failed to register gyro driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit gyro_exit(void)
{
	gyro_remove();
	platform_driver_unregister(&gyroscope_driver);
}

late_initcall(gyro_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GYROSCOPE device driver");
MODULE_AUTHOR("Mediatek");


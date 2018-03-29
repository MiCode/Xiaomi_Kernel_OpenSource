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

#include "inc/accel.h"
#include "inc/accel_factory.h"

struct acc_context *acc_context_obj = NULL;
bool success_Flag = false;
EXPORT_SYMBOL(success_Flag);

static int acc_probe(void);
static struct acc_init_info *gsensor_init_list[MAX_CHOOSE_G_NUM] = { 0 };

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
	struct acc_context *obj = (struct acc_context *)container_of(timer, struct acc_context, hrTimer);
	static int count;

	if (obj == NULL) {
		ACC_ERR("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime = ktime_add_ns(ktime_get(), (int64_t)delay_ms*1000000);
		/* ACC_LOG("%d, cur_nt = %lld, delay_ms = %d, target_nt = %lld\n", count,
			getCurNT(), delay_ms, ktime_to_us(obj->target_ktime)); */
		count = 0;
	} else {
		do {
			obj->target_ktime = ktime_add_ns(obj->target_ktime, (int64_t)delay_ms*1000000);
		} while (ktime_to_ns(obj->target_ktime) < ktime_to_ns(ktime_get()));
		/* ACC_LOG("%d, cur_nt = %lld, delay_ms = %d, target_nt = %lld\n", count,
			getCurNT(), delay_ms, ktime_to_us(obj->target_ktime)); */
		count++;
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}

static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}

static void acc_work_func(struct work_struct *work)
{
	struct acc_context *cxt = NULL;
	int x, y, z, status;
	int64_t pre_ns, cur_ns;
	int64_t delay_ms;
	int err;

	cxt = acc_context_obj;
	delay_ms = atomic_read(&cxt->delay);

	if (NULL == cxt->acc_data.get_data) {
		ACC_ERR("acc driver not register data path\n");
		return;
	}

	cur_ns = getCurNS();

	err = cxt->acc_data.get_data(&x, &y, &z, &status);

	if (err) {
		ACC_ERR("get acc data fails!!\n");
		goto acc_loop;
	} else {
			if (0 == x && 0 == y && 0 == z)
				goto acc_loop;

			cxt->drv_data.acc_data.values[0] = x;
			cxt->drv_data.acc_data.values[1] = y;
			cxt->drv_data.acc_data.values[2] = z;
			cxt->drv_data.acc_data.status = status;
			pre_ns = cxt->drv_data.acc_data.time;
			cxt->drv_data.acc_data.time = cur_ns;
	}

	if (true == cxt->is_first_data_after_enable) {
		pre_ns = cur_ns;
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (ACC_INVALID_VALUE == cxt->drv_data.acc_data.values[0] ||
		    ACC_INVALID_VALUE == cxt->drv_data.acc_data.values[1] ||
		    ACC_INVALID_VALUE == cxt->drv_data.acc_data.values[2]) {
			ACC_LOG(" read invalid data\n");
			goto acc_loop;

		}
	}
	/* report data to input device */
	/* printk("new acc work run....\n"); */
	/* ACC_LOG("acc data[%d,%d,%d]\n" ,cxt->drv_data.acc_data.values[0], */
	/* cxt->drv_data.acc_data.values[1],cxt->drv_data.acc_data.values[2]); */

	while ((cur_ns - pre_ns) >= delay_ms*1800000LL) {
		pre_ns += delay_ms*1000000LL;
		acc_data_report(cxt->drv_data.acc_data.values[0],
			cxt->drv_data.acc_data.values[1], cxt->drv_data.acc_data.values[2],
			cxt->drv_data.acc_data.status, pre_ns);
	}

	acc_data_report(cxt->drv_data.acc_data.values[0],
			cxt->drv_data.acc_data.values[1], cxt->drv_data.acc_data.values[2],
			cxt->drv_data.acc_data.status, cxt->drv_data.acc_data.time);

 acc_loop:
	if (true == cxt->is_polling_run)
		startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), false);
}

enum hrtimer_restart acc_poll(struct hrtimer *timer)
{
	struct acc_context *obj = (struct acc_context *)container_of(timer, struct acc_context, hrTimer);

	queue_work(obj->accel_workqueue, &obj->report);

	/* ACC_LOG("cur_ns = %lld\n", getCurNS()); */

	return HRTIMER_NORESTART;
}

static struct acc_context *acc_context_alloc_object(void)
{

	struct acc_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	ACC_LOG("acc_context_alloc_object++++\n");
	if (!obj) {
		ACC_ERR("Alloc accel object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);	/*5Hz ,  set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, acc_work_func);
	obj->accel_workqueue = NULL;
	obj->accel_workqueue = create_workqueue("accel_polling");
	if (!obj->accel_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, acc_poll);
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->acc_op_mutex);
	obj->is_batch_enable = false;/* for batch mode init */
	obj->cali_sw[ACC_AXIS_X] = 0;
	obj->cali_sw[ACC_AXIS_Y] = 0;
	obj->cali_sw[ACC_AXIS_Z] = 0;
	ACC_LOG("acc_context_alloc_object----\n");
	return obj;
}

static int acc_real_enable(int enable)
{
	int err = 0;
	struct acc_context *cxt = NULL;

	cxt = acc_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->acc_ctl.enable_nodata(1);
			if (err) {
				err = cxt->acc_ctl.enable_nodata(1);
				if (err) {
					err = cxt->acc_ctl.enable_nodata(1);
					if (err)
						ACC_ERR("acc enable(%d) err 3 timers = %d\n",
							enable, err);
				}
			}
			ACC_LOG("acc real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->acc_ctl.enable_nodata(0);
			if (err)
				ACC_ERR("acc enable(%d) err = %d\n", enable, err);
			ACC_LOG("acc real disable\n");
		}

	}

	return err;
}

static int acc_enable_data(int enable)
{
	struct acc_context *cxt = NULL;

	cxt = acc_context_obj;
	if (NULL == cxt->acc_ctl.open_report_data) {
		ACC_ERR("no acc control path\n");
		return -1;
	}

	if (1 == enable) {
		ACC_LOG("ACC enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->acc_ctl.open_report_data(1);
	acc_real_enable(enable);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->acc_ctl.is_report_input_direct) {
				startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		ACC_LOG("ACC disable\n");

		cxt->is_active_data = false;
		cxt->acc_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->acc_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				smp_mb();/* for memory barrier */
				stopTimer(&cxt->hrTimer);
				smp_mb();/* for memory barrier */
				cancel_work_sync(&cxt->report);
				cxt->drv_data.acc_data.values[0] = ACC_INVALID_VALUE;
				cxt->drv_data.acc_data.values[1] = ACC_INVALID_VALUE;
				cxt->drv_data.acc_data.values[2] = ACC_INVALID_VALUE;
			}
		}
	acc_real_enable(enable);
	}
	return 0;
}



int acc_enable_nodata(int enable)
{
	struct acc_context *cxt = NULL;

	cxt = acc_context_obj;
	if (NULL == cxt->acc_ctl.enable_nodata) {
		ACC_ERR("acc_enable_nodata:acc ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;
	acc_real_enable(enable);
	return 0;
}


static ssize_t acc_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ACC_LOG(" not support now\n");
	return len;
}

static ssize_t acc_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
#ifndef CONFIG_MTK_SCP_SENSORHUB_V1
	struct acc_context *cxt = NULL;

	ACC_LOG("acc_store_enable nodata buf=%s\n", buf);
	mutex_lock(&acc_context_obj->acc_op_mutex);
	cxt = acc_context_obj;
	if (NULL == cxt->acc_ctl.enable_nodata) {
		ACC_LOG("acc_ctl enable nodata NULL\n");
		mutex_unlock(&acc_context_obj->acc_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->acc_ctl.enable_nodata(1); */
		acc_enable_nodata(1);
	} else if (!strncmp(buf, "0", 1)) {
		/* cxt->acc_ctl.enable_nodata(0); */
		acc_enable_nodata(0);
	} else {
		ACC_ERR(" acc_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&acc_context_obj->acc_op_mutex);
#endif
	return count;
}

static ssize_t acc_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct acc_context *cxt = NULL;

	ACC_LOG("acc_store_active buf=%s\n", buf);
	mutex_lock(&acc_context_obj->acc_op_mutex);
	cxt = acc_context_obj;
	if (NULL == cxt->acc_ctl.open_report_data) {
		ACC_LOG("acc_ctl enable NULL\n");
		mutex_unlock(&acc_context_obj->acc_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->acc_ctl.enable(1); */
		acc_enable_data(1);

	} else if (!strncmp(buf, "0", 1)) {

		/* cxt->acc_ctl.enable(0); */
		acc_enable_data(0);
	} else {
		ACC_ERR(" acc_store_active error !!\n");
	}
	mutex_unlock(&acc_context_obj->acc_op_mutex);
	ACC_LOG(" acc_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t acc_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct acc_context *cxt = NULL;
	int div = 0;

	cxt = acc_context_obj;
	div = cxt->acc_data.vender_div;
	ACC_LOG("acc vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t acc_store_delay(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int64_t delay = 0;
	int64_t mdelay = 0;
	int ret = 0;
	struct acc_context *cxt = NULL;

	mutex_lock(&acc_context_obj->acc_op_mutex);
	cxt = acc_context_obj;
	if (NULL == cxt->acc_ctl.set_delay) {
		ACC_LOG("acc_ctl set_delay NULL\n");
		mutex_unlock(&acc_context_obj->acc_op_mutex);
		return count;
	}

	ret = kstrtoll(buf, 10, &delay);
	if (ret != 0) {
		ACC_ERR("invalid format!!\n");
		mutex_unlock(&acc_context_obj->acc_op_mutex);
		return count;
	}

	if (false == cxt->acc_ctl.is_report_input_direct) {
		mdelay = delay;
		do_div(mdelay, 1000000);
		atomic_set(&acc_context_obj->delay, mdelay);
	}
	cxt->acc_ctl.set_delay(delay);
	ACC_LOG(" acc_delay %lld ns\n", delay);
	mutex_unlock(&acc_context_obj->acc_op_mutex);
	return count;
}

static ssize_t acc_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ACC_LOG(" not support now\n");
	return len;
}
/* need work around again */
static ssize_t acc_show_sensordevnum(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned int devnum;
	struct acc_context *cxt = NULL;
	const char *devname = NULL;
	int ret = 0;
	struct input_handle *handle;

	cxt = acc_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
	if (strncmp(handle->name, "event", 5) == 0) {
		devname = handle->name;
		break;
	}
	ret = sscanf(devname+5, "%d", &devnum);
	return snprintf(buf, PAGE_SIZE, "%d\n", devnum);
}

static ssize_t acc_store_batch(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct acc_context *cxt = NULL;

	/* ACC_LOG("acc_store_batch buf=%s\n", buf); */
	mutex_lock(&acc_context_obj->acc_op_mutex);
	cxt = acc_context_obj;
	if (cxt->acc_ctl.is_support_batch) {
		ACC_LOG("acc_store_batch buf=%s\n", buf);
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			if (true == cxt->is_polling_run) {
				cxt->is_polling_run = false;
				smp_mb();  /* for memory barrier */
				stopTimer(&cxt->hrTimer);
				smp_mb();  /* for memory barrier */
				cancel_work_sync(&cxt->report);
				cxt->drv_data.acc_data.values[0] = ACC_INVALID_VALUE;
				cxt->drv_data.acc_data.values[1] = ACC_INVALID_VALUE;
				cxt->drv_data.acc_data.values[2] = ACC_INVALID_VALUE;
			}
		} else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			if (false == cxt->is_polling_run) {
				if (false == cxt->acc_ctl.is_report_input_direct && true == cxt->is_active_data) {
					startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), true);
					cxt->is_polling_run = true;
				}
			}
		} else
			ACC_ERR(" acc_store_batch error !!\n");
	} else
		ACC_LOG(" acc_store_batch mot supported\n");

	mutex_unlock(&acc_context_obj->acc_op_mutex);
	/* ACC_LOG(" acc_store_batch done: %d\n", cxt->is_batch_enable); */
	return count;

}
static ssize_t acc_show_batch(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static ssize_t acc_store_flush(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return count;
}

static ssize_t acc_show_flush(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static int gsensor_remove(struct platform_device *pdev)
{
	ACC_LOG("gsensor_remove\n");
	return 0;
}

static int gsensor_probe(struct platform_device *pdev)
{
	ACC_LOG("gsensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gsensor_of_match[] = {
	{ .compatible = "mediatek,gsensor", },
	{},
};
#endif
static struct platform_driver gsensor_driver = {
	.probe = gsensor_probe,
	.remove = gsensor_remove,
	.driver = {
		   .name = "gsensor",
	#ifdef CONFIG_OF
		.of_match_table = gsensor_of_match,
		#endif
		   }
};

static int acc_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	ACC_LOG(" acc_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		ACC_LOG(" i=%d\n", i);
		if (0 != gsensor_init_list[i]) {
			ACC_LOG(" acc try to init driver %s\n", gsensor_init_list[i]->name);
			err = gsensor_init_list[i]->init();
			if (0 == err) {
				ACC_LOG(" acc real driver %s probe ok\n",
					gsensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_G_NUM) {
		ACC_LOG(" acc_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

int acc_driver_add(struct acc_init_info *obj)
{
	int err = 0;
	int i = 0;

	ACC_ERR("yue acc_driver_add +++++++++++\n");
	if (!obj) {
		ACC_ERR("ACC driver add fail, acc_init_info is NULL\n");
		return -1;
	}
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		if ((i == 0) && (NULL == gsensor_init_list[0])) {
			ACC_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&gsensor_driver))
				ACC_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == gsensor_init_list[i]) {
			obj->platform_diver_addr = &gsensor_driver;
			gsensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_G_NUM) {
		ACC_ERR("ACC driver add err\n");
		err = -1;
	}

	if (success_Flag == false) {
		if (acc_probe()) {
			ACC_ERR("failed to register acc driver\n");
			return -ENODEV;
		}
		success_Flag = true;
	}
	return err;
}
EXPORT_SYMBOL_GPL(acc_driver_add);

static int acc_misc_init(struct acc_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = ACC_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		ACC_ERR("unable to register acc misc device!!\n");
	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}


static int acc_input_init(struct acc_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = ACC_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACCEL_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACCEL_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACCEL_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACCEL_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ACCEL_UPDATE);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ACCEL_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ACCEL_TIMESTAMP_LO);

	input_set_abs_params(dev, EVENT_TYPE_ACCEL_X, ACC_VALUE_MIN, ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACCEL_Y, ACC_VALUE_MIN, ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACCEL_Z, ACC_VALUE_MIN, ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACCEL_STATUS, ACC_STATUS_MIN, ACC_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(accenablenodata, S_IWUSR | S_IRUGO, acc_show_enable_nodata, acc_store_enable_nodata);
DEVICE_ATTR(accactive, S_IWUSR | S_IRUGO, acc_show_active, acc_store_active);
DEVICE_ATTR(accdelay, S_IWUSR | S_IRUGO, acc_show_delay, acc_store_delay);
DEVICE_ATTR(accbatch,		S_IWUSR | S_IRUGO, acc_show_batch,  acc_store_batch);
DEVICE_ATTR(accflush,		S_IWUSR | S_IRUGO, acc_show_flush,  acc_store_flush);
DEVICE_ATTR(accdevnum,		S_IWUSR | S_IRUGO, acc_show_sensordevnum,  NULL);

static struct attribute *acc_attributes[] = {
	&dev_attr_accenablenodata.attr,
	&dev_attr_accactive.attr,
	&dev_attr_accdelay.attr,
	&dev_attr_accbatch.attr,
	&dev_attr_accflush.attr,
	&dev_attr_accdevnum.attr,
	NULL
};

static struct attribute_group acc_attribute_group = {
	.attrs = acc_attributes
};

int acc_register_data_path(struct acc_data_path *data)
{
	struct acc_context *cxt = NULL;

	cxt = acc_context_obj;
	cxt->acc_data.get_data = data->get_data;
	cxt->acc_data.get_raw_data = data->get_raw_data;
	cxt->acc_data.vender_div = data->vender_div;
	ACC_LOG("acc register data path vender_div: %d\n", cxt->acc_data.vender_div);
	if (NULL == cxt->acc_data.get_data) {
		ACC_LOG("acc register data path fail\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(acc_register_data_path);

int acc_register_control_path(struct acc_control_path *ctl)
{
	struct acc_context *cxt = NULL;
	int err = 0;

	cxt = acc_context_obj;
	cxt->acc_ctl.set_delay = ctl->set_delay;
	cxt->acc_ctl.open_report_data = ctl->open_report_data;
	cxt->acc_ctl.enable_nodata = ctl->enable_nodata;
	cxt->acc_ctl.is_support_batch = ctl->is_support_batch;
	cxt->acc_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->acc_ctl.acc_calibration = ctl->acc_calibration;

	if (NULL == cxt->acc_ctl.set_delay || NULL == cxt->acc_ctl.open_report_data
	    || NULL == cxt->acc_ctl.enable_nodata) {
		ACC_LOG("acc register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = acc_misc_init(acc_context_obj);
	if (err) {
		ACC_ERR("unable to register acc misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&acc_context_obj->mdev.this_device->kobj, &acc_attribute_group);
	if (err < 0) {
		ACC_ERR("unable to create acc attribute file\n");
		return -3;
	}

	kobject_uevent(&acc_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}
EXPORT_SYMBOL_GPL(acc_register_control_path);

int acc_data_report(int x, int y, int z, int status, int64_t nt)
{
	/* ACC_LOG("+acc_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct acc_context *cxt = NULL;
	int err = 0;

	cxt = acc_context_obj;
	input_report_abs(cxt->idev, EVENT_TYPE_ACCEL_X, x);
	input_report_abs(cxt->idev, EVENT_TYPE_ACCEL_Y, y);
	input_report_abs(cxt->idev, EVENT_TYPE_ACCEL_Z, z);
	input_report_abs(cxt->idev, EVENT_TYPE_ACCEL_STATUS, status);
	input_report_rel(cxt->idev, EVENT_TYPE_ACCEL_UPDATE, 1);
	input_report_rel(cxt->idev, EVENT_TYPE_ACCEL_TIMESTAMP_HI, nt >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_ACCEL_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
	input_sync(cxt->idev);
	return err;
}

static int acc_probe(void)
{

	int err;

	ACC_LOG("+++++++++++++accel_probe!!\n");

	acc_context_obj = acc_context_alloc_object();
	if (!acc_context_obj) {
		err = -ENOMEM;
		ACC_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real acceleration driver */
	err = acc_real_driver_init();
	if (err) {
		ACC_ERR("acc real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init acc common factory mode misc device */
	err = acc_factory_device_init();
	if (err)
		ACC_ERR("acc factory device already registed\n");
	/* init input dev */
	err = acc_input_init(acc_context_obj);
	if (err) {
		ACC_ERR("unable to register acc input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	ACC_LOG("----accel_probe OK !!\n");
	return 0;


 real_driver_init_fail:
 exit_alloc_input_dev_failed:
 	destroy_workqueue(acc_context_obj->accel_workqueue);
	del_timer(&acc_context_obj->timer);
	kfree(acc_context_obj);
	acc_context_obj = NULL;

 exit_alloc_data_failed:


	ACC_ERR("----accel_probe fail !!!\n");
	return err;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACCELEROMETER device driver");
MODULE_AUTHOR("Mediatek");

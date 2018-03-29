/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include "activity.h"

struct act_context *act_context_obj = NULL;


static struct act_init_info *activity_init_list[MAX_CHOOSE_ACT_NUM] = { 0 };	/* modified */

static void act_work_func(struct work_struct *work)
{

	struct act_context *cxt = NULL;
	/* int out_size; */
	struct hwm_sensor_data sensor_data;
	/* u64 data64[6]; //for unify get_data parameter type */
	/* u16 data32[6]; //for hwm_sensor_data.values as int */
	int status;
	int64_t nt;
	struct timespec time;
	int err = 0;
	static int64_t last_time_stamp;
	cxt = act_context_obj;

	if (cxt->act_data.get_data == NULL)
		ACT_ERR("act driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	/* add wake lock to make sure data can be read before system suspend */
	/* initial data */

	err = cxt->act_data.get_data(&sensor_data, &status);

	if (err) {
		ACT_ERR("get act data fails!!\n");
		goto act_loop;
	} else {
		cxt->drv_data.probability[0] = sensor_data.probability[0];
		cxt->drv_data.probability[1] = sensor_data.probability[1];
		cxt->drv_data.probability[2] = sensor_data.probability[2];
		cxt->drv_data.probability[3] = sensor_data.probability[3];
		cxt->drv_data.probability[4] = sensor_data.probability[4];
		cxt->drv_data.probability[5] = sensor_data.probability[5];
		cxt->drv_data.probability[6] = sensor_data.probability[6];
		cxt->drv_data.probability[7] = sensor_data.probability[7];
		cxt->drv_data.probability[8] = sensor_data.probability[8];
		cxt->drv_data.probability[9] = sensor_data.probability[9];
		cxt->drv_data.probability[10] = sensor_data.probability[10];
		cxt->drv_data.probability[11] = sensor_data.probability[11];
		cxt->drv_data.status = status;
		cxt->drv_data.time = sensor_data.time;
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (ACT_INVALID_VALUE == cxt->drv_data.probability[0] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[1] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[2] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[3] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[4] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[5] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[6] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[7] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[8] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[9] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[10] ||
		    ACT_INVALID_VALUE == cxt->drv_data.probability[11]) {
			ACT_LOG(" read invalid data\n");
			goto act_loop;

		}
	}
	/* report data to input devic */
	/* ACT_LOG("act data[%d,%d,%d]\n" ,cxt->drv_data.act_data.probability[0],*/
	/* cxt->drv_data.act_data.probability[1],cxt->drv_data.act_data.probability[2]);*/
	if (last_time_stamp != cxt->drv_data.time) {
		last_time_stamp = cxt->drv_data.time;
		act_data_report(&cxt->drv_data, cxt->drv_data.status);
	}

act_loop:
	if (true == cxt->is_polling_run) {
		{
			mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
		}
	}
}

static void act_poll(unsigned long data)
{
	struct act_context *obj = (struct act_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct act_context *act_context_alloc_object(void)
{
	struct act_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	ACT_LOG("act_context_alloc_object++++\n");
	if (!obj) {
		ACT_ERR("Alloc act object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);	/*5Hz set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, act_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = act_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	mutex_init(&obj->act_op_mutex);
	ACT_LOG("act_context_alloc_object----\n");
	return obj;
}

static int act_real_enable(int enable)
{
	int err = 0;
	struct act_context *cxt = NULL;

	cxt = act_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->act_ctl.enable_nodata(1);
			if (err) {
				err = cxt->act_ctl.enable_nodata(1);
				if (err) {
					err = cxt->act_ctl.enable_nodata(1);
					if (err)
						ACT_ERR("act enable(%d) err 3 timers = %d\n",
							enable, err);
				}
			}
			ACT_LOG("act real enable\n");
		}
	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->act_ctl.enable_nodata(0);
			if (err)
				ACT_ERR("act enable(%d) err = %d\n", enable, err);

			ACT_LOG("act real disable\n");
		}
	}

	return err;
}

static int act_enable_data(int enable)
{
	struct act_context *cxt = NULL;

	cxt = act_context_obj;
	if (NULL == cxt->act_ctl.open_report_data) {
		ACT_ERR("no act control path\n");
		return -1;
	}

	if (1 == enable) {
		ACT_LOG("act enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->act_ctl.open_report_data(1);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->act_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		ACT_LOG("act disable\n");

		cxt->is_active_data = false;
		cxt->act_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->act_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.probability[0] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[1] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[2] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[3] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[4] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[5] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[6] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[7] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[8] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[9] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[10] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[11] = ACT_INVALID_VALUE;
			}
		}

	}
	act_real_enable(enable);
	return 0;
}



int act_enable_nodata(int enable)
{
	struct act_context *cxt = NULL;

	cxt = act_context_obj;
	if (NULL == cxt->act_ctl.enable_nodata) {
		ACT_ERR("act_enable_nodata:act ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	act_real_enable(enable);
	return 0;
}


static ssize_t act_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ACT_LOG(" not support now\n");
	return len;
}

static ssize_t act_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct act_context *cxt = NULL;
	/* int err =0; */
	ACT_LOG("act_store_enable nodata buf=%s\n", buf);
	mutex_lock(&act_context_obj->act_op_mutex);
	cxt = act_context_obj;
	if (NULL == cxt->act_ctl.enable_nodata) {
		ACT_LOG("act_ctl enable nodata NULL\n");
		mutex_unlock(&act_context_obj->act_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		act_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		act_enable_nodata(0);
	else
		ACT_ERR(" act_store enable nodata cmd error !!\n");

	mutex_unlock(&act_context_obj->act_op_mutex);
	return count;
}

static ssize_t act_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct act_context *cxt = NULL;

	ACT_LOG("act_store_active buf=%s\n", buf);
	mutex_lock(&act_context_obj->act_op_mutex);
	cxt = act_context_obj;
	if (NULL == cxt->act_ctl.open_report_data) {
		ACT_LOG("act_ctl enable NULL\n");
		mutex_unlock(&act_context_obj->act_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		act_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		act_enable_data(0);
	else
		ACT_ERR(" act_store_active error !!\n");

	mutex_unlock(&act_context_obj->act_op_mutex);
	ACT_LOG(" act_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t act_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct act_context *cxt = NULL;
	int div = 0;

	cxt = act_context_obj;
	/* int len = 0; */
	ACT_LOG("act show active not support now\n");
	/* div=cxt->act_data.vender_div; */
	ACT_LOG("act vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);

	/* return len; */
}

static ssize_t act_store_delay(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	/* struct act_context *devobj = (struct act_context*)dev_get_drvdata(dev); */
	int delay;
	int mdelay = 0;
	struct act_context *cxt = NULL;
	int err = 0;

	mutex_lock(&act_context_obj->act_op_mutex);
	cxt = act_context_obj;
	if (NULL == cxt->act_ctl.set_delay) {
		ACT_LOG("act_ctl set_delay NULL\n");
		mutex_unlock(&act_context_obj->act_op_mutex);
		return count;
	}

	err = kstrtoint(buf, 10, &delay);
	if (0 != err) {
		ACT_ERR("invalid format!!\n");
		mutex_unlock(&act_context_obj->act_op_mutex);
		return count;
	}

	if (false == cxt->act_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&act_context_obj->delay, mdelay);
	}
	cxt->act_ctl.set_delay(delay);
	ACT_LOG(" act_delay %d ns\n", delay);
	mutex_unlock(&act_context_obj->act_op_mutex);
	return count;
}

static ssize_t act_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ACT_LOG(" not support now\n");
	return len;
}

static ssize_t act_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct act_context *cxt = NULL;
	/* int err =0; */
	ACT_LOG("act_store_batch buf=%s\n", buf);
	mutex_lock(&act_context_obj->act_op_mutex);
	cxt = act_context_obj;
	if (cxt->act_ctl.is_support_batch) {
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			if (true == cxt->is_polling_run) {
				cxt->is_polling_run = false;
				smp_mb();  /* for memory barrier */
				del_timer_sync(&cxt->timer);
				smp_mb();  /* for memory barrier */
				cancel_work_sync(&cxt->report);
				cxt->drv_data.probability[0] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[1] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[2] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[3] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[4] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[5] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[6] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[7] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[8] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[9] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[10] = ACT_INVALID_VALUE;
				cxt->drv_data.probability[11] = ACT_INVALID_VALUE;
			}
		} else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			if (false == cxt->is_polling_run) {
				if (false == cxt->act_ctl.is_report_input_direct && true == cxt->is_active_data) {
					mod_timer(&cxt->timer,
						  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
					cxt->is_polling_run = true;
				}
			}
		} else {
			ACT_ERR(" act_store_batch error !!\n");
		}
	} else {
		ACT_LOG(" act_store_batch not support\n");
	}
	mutex_unlock(&act_context_obj->act_op_mutex);
	ACT_LOG(" act_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t act_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t act_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	/* mutex_lock(&act_context_obj->act_op_mutex); */
	/* struct act_context *devobj = (struct act_context*)dev_get_drvdata(dev); */
	/* do read FIFO data function and report data immediately */
	/* mutex_unlock(&act_context_obj->act_op_mutex); */
	return count;
}

static ssize_t act_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t act_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &act_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int activity_remove(struct platform_device *pdev)
{
	ACT_LOG("activity_remove\n");
	return 0;
}

static int activity_probe(struct platform_device *pdev)
{
	ACT_LOG("activity_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id activity_of_match[] = {
	{.compatible = "mediatek,activity",},
	{},
};
#endif

static struct platform_driver activity_driver = {
	.probe = activity_probe,
	.remove = activity_remove,
	.driver = {
		   .name = "activity",
#ifdef CONFIG_OF
		   .of_match_table = activity_of_match,
#endif
		   }
};

static int act_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	ACT_LOG(" act_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_ACT_NUM; i++) {
		ACT_LOG(" i=%d\n", i);
		if (0 != activity_init_list[i]) {
			ACT_LOG(" act try to init driver %s\n", activity_init_list[i]->name);
			err = activity_init_list[i]->init();
			if (0 == err) {
				ACT_LOG(" act real driver %s probe ok\n",
					activity_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_ACT_NUM) {
		ACT_LOG(" act_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

int act_driver_add(struct act_init_info *obj)
{
	int err = 0;
	int i = 0;

	ACT_FUN(f);

	for (i = 0; i < MAX_CHOOSE_ACT_NUM; i++) {
		if (i == 0) {
			ACT_LOG("register act driver for the first time\n");
			err = platform_driver_register(&activity_driver);
			if (err)
				ACT_ERR("failed to register act driver already exist\n");
		}

		if (NULL == activity_init_list[i]) {
			obj->platform_diver_addr = &activity_driver;
			activity_init_list[i] = obj;
			break;
		}
	}
	if (NULL == activity_init_list[i]) {
		ACT_ERR("act driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(act_driver_add);

static int act_misc_init(struct act_context *cxt)
{

	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = ACT_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		ACT_ERR("unable to register act misc device!!\n");

	return err;
}

static void act_input_destroy(struct act_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int act_input_init(struct act_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = ACT_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_IN_VEHICLE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_ON_BICYCLE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_ON_FOOT);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_STILL);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_UNKNOWN);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_TILTING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_WALKING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_STANDING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_LYING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_RUNNING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_CLIMBING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_SITTING);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_ACT_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ACT_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_ACT_TIMESTAMP_LO);
	input_set_abs_params(dev, EVENT_TYPE_ACT_IN_VEHICLE, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_ON_BICYCLE, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_ON_FOOT, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_STILL, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_UNKNOWN, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_TILTING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_WALKING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_STANDING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_LYING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_RUNNING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_CLIMBING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_SITTING, ACT_VALUE_MIN, ACT_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_ACT_STATUS, ACT_STATUS_MIN, ACT_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(actenablenodata, S_IWUSR | S_IRUGO, act_show_enable_nodata, act_store_enable_nodata);
DEVICE_ATTR(actactive, S_IWUSR | S_IRUGO, act_show_active, act_store_active);
DEVICE_ATTR(actdelay, S_IWUSR | S_IRUGO, act_show_delay, act_store_delay);
DEVICE_ATTR(actbatch, S_IWUSR | S_IRUGO, act_show_batch, act_store_batch);
DEVICE_ATTR(actflush, S_IWUSR | S_IRUGO, act_show_flush, act_store_flush);
DEVICE_ATTR(actdevnum, S_IWUSR | S_IRUGO, act_show_devnum, NULL);

static struct attribute *act_attributes[] = {
	&dev_attr_actenablenodata.attr,
	&dev_attr_actactive.attr,
	&dev_attr_actdelay.attr,
	&dev_attr_actbatch.attr,
	&dev_attr_actflush.attr,
	&dev_attr_actdevnum.attr,
	NULL
};

static struct attribute_group act_attribute_group = {
	.attrs = act_attributes
};

int act_register_data_path(struct act_data_path *data)
{
	struct act_context *cxt = NULL;
	/* int err =0; */
	cxt = act_context_obj;
	cxt->act_data.get_data = data->get_data;
	/* cxt->act_data.vender_div = data->vender_div; */
	/* cxt->act_data.get_raw_data = data->get_raw_data; */
	/* ACT_LOG("act register data path vender_div: %d\n", cxt->act_data.vender_div); */
	if (NULL == cxt->act_data.get_data) {
		ACT_LOG("act register data path fail\n");
		return -1;
	}
	return 0;
}

int act_register_control_path(struct act_control_path *ctl)
{
	struct act_context *cxt = NULL;
	int err = 0;

	cxt = act_context_obj;
	cxt->act_ctl.set_delay = ctl->set_delay;
	cxt->act_ctl.open_report_data = ctl->open_report_data;
	cxt->act_ctl.enable_nodata = ctl->enable_nodata;
	cxt->act_ctl.is_support_batch = ctl->is_support_batch;

	if (NULL == cxt->act_ctl.set_delay || NULL == cxt->act_ctl.open_report_data
	    || NULL == cxt->act_ctl.enable_nodata) {
		ACT_LOG("act register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = act_misc_init(act_context_obj);
	if (err) {
		ACT_ERR("unable to register act misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&act_context_obj->mdev.this_device->kobj, &act_attribute_group);
	if (err < 0) {
		ACT_ERR("unable to create act attribute file\n");
		return -3;
	}

	kobject_uevent(&act_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int act_data_report(struct hwm_sensor_data *data, int status)
{
	struct act_context *cxt = NULL;
	int err = 0;

	cxt = act_context_obj;

	input_report_abs(cxt->idev, EVENT_TYPE_ACT_STILL, data->probability[0]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_STANDING, data->probability[1]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_SITTING, data->probability[2]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_LYING, data->probability[3]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_ON_FOOT, data->probability[4]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_WALKING, data->probability[5]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_RUNNING, data->probability[6]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_CLIMBING, data->probability[7]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_ON_BICYCLE, data->probability[8]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_IN_VEHICLE, data->probability[9]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_TILTING, data->probability[10]);
	input_report_abs(cxt->idev, EVENT_TYPE_ACT_UNKNOWN, data->probability[11]);
	input_report_rel(cxt->idev, EVENT_TYPE_ACT_TIMESTAMP_HI, data->time >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_ACT_TIMESTAMP_LO, data->time & 0xFFFFFFFFLL);
	input_sync(cxt->idev);
	return err;
}

static int act_probe(void)
{
	int err;

	ACT_LOG("+++++++++++++act_probe!!\n");

	act_context_obj = act_context_alloc_object();
	if (!act_context_obj) {
		err = -ENOMEM;
		ACT_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real acteleration driver */
	err = act_real_driver_init();
	if (err) {
		ACT_ERR("act real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* err = act_factory_device_init(); */
	/* if(err) */
	/* { */
	/* ACT_ERR("act_factory_device_init fail\n"); */
	/* } */

	/* init input dev */
	err = act_input_init(act_context_obj);
	if (err) {
		ACT_ERR("unable to register act input device!\n");
		goto exit_alloc_input_dev_failed;
	}
	ACT_LOG("----act_probe OK !!\n");
	return 0;

	/* exit_hwmsen_create_attr_failed: */
	/* exit_misc_register_failed: */

	/* exit_err_sysfs: */

	if (err) {
		ACT_ERR("sysfs node creation error\n");
		act_input_destroy(act_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(act_context_obj);

exit_alloc_data_failed:


	ACT_LOG("----act_probe fail !!!\n");
	return err;
}



static int act_remove(void)
{
	int err = 0;

	ACT_FUN(f);
	input_unregister_device(act_context_obj->idev);
	sysfs_remove_group(&act_context_obj->idev->dev.kobj, &act_attribute_group);

	err = misc_deregister(&act_context_obj->mdev);
	if (err)
		ACT_ERR("misc_deregister fail: %d\n", err);
	kfree(act_context_obj);

	return 0;
}
static int __init act_init(void)
{
	ACT_FUN(f);

	if (act_probe()) {
		ACT_ERR("failed to register act driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit act_exit(void)
{
	act_remove();
	platform_driver_unregister(&activity_driver);
}

late_initcall(act_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITY device driver");
MODULE_AUTHOR("Mediatek");

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

#include "uncali_gyro.h"

static struct uncali_gyro_context *uncali_gyro_context_obj;


static struct uncali_gyro_init_info *uncali_gyrosensor_init_list[MAX_CHOOSE_UNCALI_GYRO_NUM] = { 0 };

static void uncali_gyro_work_func(struct work_struct *work)
{

	struct uncali_gyro_context *cxt = NULL;
	int dat[3], offset[3], status;
	int64_t nt;
	struct timespec time;
	int err;

	cxt = uncali_gyro_context_obj;

	if (NULL == cxt->uncali_gyro_data.get_data)
		UNCALI_GYRO_LOG("uncali_gyro driver not register data path\n");


	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->uncali_gyro_data.get_data(dat, offset, &status);
	if (err) {
		UNCALI_GYRO_ERR("get uncali_gyro data fails!!\n");
		goto uncali_gyro_loop;
	} else {
		{
			if (0 == dat[0] && 0 == dat[1] && 0 == dat[2])
				goto uncali_gyro_loop;

			cxt->drv_data.uncali_gyro_data.values[0] = dat[0];
			cxt->drv_data.uncali_gyro_data.values[1] = dat[1];
			cxt->drv_data.uncali_gyro_data.values[2] = dat[2];
			cxt->drv_data.uncali_gyro_data.values[3] = offset[0];
			cxt->drv_data.uncali_gyro_data.values[4] = offset[1];
			cxt->drv_data.uncali_gyro_data.values[5] = offset[2];
			cxt->drv_data.uncali_gyro_data.status = status;
			cxt->drv_data.uncali_gyro_data.time = nt;

		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (UNCALI_GYRO_INVALID_VALUE == cxt->drv_data.uncali_gyro_data.values[0] ||
		    UNCALI_GYRO_INVALID_VALUE == cxt->drv_data.uncali_gyro_data.values[1] ||
		    UNCALI_GYRO_INVALID_VALUE == cxt->drv_data.uncali_gyro_data.values[2] ||
		    UNCALI_GYRO_INVALID_VALUE == cxt->drv_data.uncali_gyro_data.values[3]
		    ) {
			UNCALI_GYRO_LOG(" read invalid data\n");
			goto uncali_gyro_loop;

		}
	}

	uncali_gyro_data_report(cxt->drv_data.uncali_gyro_data.values,
				cxt->drv_data.uncali_gyro_data.status, nt);

uncali_gyro_loop:
	if (true == cxt->is_polling_run)
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
}

static void uncali_gyro_poll(unsigned long data)
{
	struct uncali_gyro_context *obj = (struct uncali_gyro_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct uncali_gyro_context *uncali_gyro_context_alloc_object(void)
{

	struct uncali_gyro_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	UNCALI_GYRO_LOG("uncali_gyro_context_alloc_object++++\n");
	if (!obj) {
		UNCALI_GYRO_ERR("Alloc uncali_gyro object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->enable, 0);
	INIT_WORK(&obj->report, uncali_gyro_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = uncali_gyro_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->uncali_gyro_op_mutex);
	obj->is_batch_enable = false;	/* for batch mode init */
	UNCALI_GYRO_LOG("uncali_gyro_context_alloc_object----\n");
	return obj;
}

static int uncali_gyro_real_enable(int enable)
{
	int err = 0;
	struct uncali_gyro_context *cxt = NULL;

	cxt = uncali_gyro_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->uncali_gyro_ctl.enable_nodata(1);
			if (err) {
				err = cxt->uncali_gyro_ctl.enable_nodata(1);
				if (err) {
					err = cxt->uncali_gyro_ctl.enable_nodata(1);
					if (err)
						UNCALI_GYRO_ERR
						    ("uncali_gyro enable(%d) err 3 timers = %d\n",
						     enable, err);
				}
			}
			UNCALI_GYRO_LOG("uncali_gyro real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->uncali_gyro_ctl.enable_nodata(0);
			if (err)
				UNCALI_GYRO_ERR("uncali_gyro enable(%d) err = %d\n", enable, err);
			UNCALI_GYRO_LOG("uncali_gyro real disable\n");
		}

	}

	return err;
}

static int uncali_gyro_enable_data(int enable)
{
	struct uncali_gyro_context *cxt = NULL;

	/* int err =0; */
	cxt = uncali_gyro_context_obj;
	if (NULL == cxt->uncali_gyro_ctl.open_report_data) {
		UNCALI_GYRO_ERR("no uncali_gyro control path\n");
		return -1;
	}

	if (1 == enable) {
		UNCALI_GYRO_LOG("UNCALI_GYRO enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->uncali_gyro_ctl.open_report_data(1);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->uncali_gyro_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		UNCALI_GYRO_LOG("UNCALI_GYRO disable\n");

		cxt->is_active_data = false;
		cxt->uncali_gyro_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->uncali_gyro_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.uncali_gyro_data.values[0] =
				    UNCALI_GYRO_INVALID_VALUE;
				cxt->drv_data.uncali_gyro_data.values[1] =
				    UNCALI_GYRO_INVALID_VALUE;
				cxt->drv_data.uncali_gyro_data.values[2] =
				    UNCALI_GYRO_INVALID_VALUE;
			}
		}

	}
	uncali_gyro_real_enable(enable);
	return 0;
}



int uncali_gyro_enable_nodata(int enable)
{
	struct uncali_gyro_context *cxt = NULL;

	/* int err =0; */
	cxt = uncali_gyro_context_obj;
	if (NULL == cxt->uncali_gyro_ctl.enable_nodata) {
		UNCALI_GYRO_ERR("uncali_gyro_enable_nodata:uncali_gyro ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;
	uncali_gyro_real_enable(enable);
	return 0;
}


static ssize_t uncali_gyro_show_enable_nodata(struct device *dev,
					      struct device_attribute *attr, char *buf)
{
	int len = 0;

	UNCALI_GYRO_LOG(" not support now\n");
	return len;
}

static ssize_t uncali_gyro_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct uncali_gyro_context *cxt = NULL;
	/* int err =0; */

	UNCALI_GYRO_LOG("uncali_gyro_store_enable nodata buf=%s\n", buf);
	mutex_lock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);

	cxt = uncali_gyro_context_obj;
	if (NULL == cxt->uncali_gyro_ctl.enable_nodata) {
		UNCALI_GYRO_LOG("uncali_gyro_ctl enable nodata NULL\n");
		mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		uncali_gyro_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		uncali_gyro_enable_nodata(0);
	else
		UNCALI_GYRO_ERR(" uncali_gyro_store enable nodata cmd error !!\n");
	mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);

	return 0;
}

static ssize_t uncali_gyro_store_active(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct uncali_gyro_context *cxt = NULL;
	/* int err =0; */

	UNCALI_GYRO_LOG("uncali_gyro_store_active buf=%s\n", buf);
	mutex_lock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	cxt = uncali_gyro_context_obj;
	if (NULL == cxt->uncali_gyro_ctl.open_report_data) {
		UNCALI_GYRO_LOG("uncali_gyro_ctl enable NULL\n");
		mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		uncali_gyro_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		uncali_gyro_enable_data(0);
	else
		UNCALI_GYRO_ERR(" uncali_gyro_store_active error !!\n");
	mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	UNCALI_GYRO_LOG(" uncali_gyro_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t uncali_gyro_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct uncali_gyro_context *cxt = NULL;
	int div;

	cxt = uncali_gyro_context_obj;
	div = cxt->uncali_gyro_data.vender_div;

	UNCALI_GYRO_LOG("uncali_gyro vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t uncali_gyro_store_delay(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	/* struct uncali_gyro_context *devobj = (struct uncali_gyro_context*)dev_get_drvdata(dev); */
	int delay;
	int mdelay = 0;
	struct uncali_gyro_context *cxt = NULL;

	mutex_lock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	/* int err =0; */
	cxt = uncali_gyro_context_obj;
	if (NULL == cxt->uncali_gyro_ctl.set_delay) {
		UNCALI_GYRO_LOG("uncali_gyro_ctl set_delay NULL\n");
		mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
		return count;
	}

	if (0 != kstrtoint(buf, 10, &delay)) {
		UNCALI_GYRO_ERR("invalid format!!\n");
		mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
		return count;
	}

	if (false == cxt->uncali_gyro_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&uncali_gyro_context_obj->delay, mdelay);
	}
	cxt->uncali_gyro_ctl.set_delay(delay);
	UNCALI_GYRO_LOG(" uncali_gyro_delay %d ns\n", delay);
	mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	return count;
}

static ssize_t uncali_gyro_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	UNCALI_GYRO_LOG(" not support now\n");
	return len;
}

static ssize_t uncali_gyro_show_sensordevnum(struct device *dev,
					     struct device_attribute *attr, char *buf)
{
	struct uncali_gyro_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = uncali_gyro_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}


static ssize_t uncali_gyro_store_batch(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{

	struct uncali_gyro_context *cxt = NULL;

	/* int err =0; */
	UNCALI_GYRO_LOG("uncali_gyro_store_batch buf=%s\n", buf);
	mutex_lock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	cxt = uncali_gyro_context_obj;
	if (cxt->uncali_gyro_ctl.is_support_batch) {
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			/* MTK problem fix - start */
			if (cxt->is_active_data && cxt->is_polling_run) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
			}
			/* MTK problem fix - end */
		} else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			/* MTK problem fix - start */
			if (cxt->is_active_data)
				uncali_gyro_enable_data(true);
			/* MTK problem fix - end */
		} else
			UNCALI_GYRO_ERR(" uncali_gyro_store_batch error !!\n");
	} else {
		UNCALI_GYRO_LOG(" uncali_gyro_store_batch mot supported\n");
	}
	mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	UNCALI_GYRO_LOG(" uncali_gyro_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t uncali_gyro_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t uncali_gyro_store_flush(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	/* struct uncali_gyro_context *devobj = (struct uncali_gyro_context*)dev_get_drvdata(dev); */
	mutex_lock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	/* do read FIFO data function and report data immediately */
	mutex_unlock(&uncali_gyro_context_obj->uncali_gyro_op_mutex);
	return count;
}

static ssize_t uncali_gyro_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int uncali_gyrosensor_remove(struct platform_device *pdev)
{
	UNCALI_GYRO_LOG("uncali_gyrosensor_remove\n");
	return 0;
}

static int uncali_gyrosensor_probe(struct platform_device *pdev)
{
	UNCALI_GYRO_LOG("uncali_gyrosensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id uncali_gyrosensor_of_match[] = {
	{.compatible = "mediatek,uncali_gyro",},
	{},
};
#endif

static struct platform_driver uncali_gyrosensor_driver = {
	.probe = uncali_gyrosensor_probe,
	.remove = uncali_gyrosensor_remove,
	.driver = {

		   .name = "uncali_gyro",
#ifdef CONFIG_OF
		   .of_match_table = uncali_gyrosensor_of_match,
#endif
		   }
};

static int uncali_gyro_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	UNCALI_GYRO_LOG(" uncali_gyro_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_UNCALI_GYRO_NUM; i++) {
		UNCALI_GYRO_LOG(" i=%d\n", i);
		if (0 != uncali_gyrosensor_init_list[i]) {
			UNCALI_GYRO_LOG(" uncali_gyro try to init driver %s\n",
					uncali_gyrosensor_init_list[i]->name);
			err = uncali_gyrosensor_init_list[i]->init();
			if (0 == err) {
				UNCALI_GYRO_LOG(" uncali_gyro real driver %s probe ok\n",
						uncali_gyrosensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_UNCALI_GYRO_NUM) {
		UNCALI_GYRO_LOG(" uncali_gyro_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

static int uncali_gyro_misc_init(struct uncali_gyro_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = UNCALI_GYRO_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		UNCALI_GYRO_ERR("unable to register uncali_gyro misc device!!\n");
	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}

static void uncali_gyro_input_destroy(struct uncali_gyro_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int uncali_gyro_input_init(struct uncali_gyro_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = UNCALI_GYRO_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_GYRO_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_GYRO_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_GYRO_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_GYRO_X_BIAS);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_GYRO_Y_BIAS);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_GYRO_Z_BIAS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_UNCALI_GYRO_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_UNCALI_GYRO_TIMESTAMP_LO);

	input_set_abs_params(dev, EVENT_TYPE_UNCALI_GYRO_X, UNCALI_GYRO_VALUE_MIN,
			     UNCALI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_GYRO_Y, UNCALI_GYRO_VALUE_MIN,
			     UNCALI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_GYRO_Z, UNCALI_GYRO_VALUE_MIN,
			     UNCALI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_GYRO_X_BIAS, UNCALI_GYRO_VALUE_MIN,
			     UNCALI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_GYRO_Y_BIAS, UNCALI_GYRO_VALUE_MIN,
			     UNCALI_GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_GYRO_Z_BIAS, UNCALI_GYRO_VALUE_MIN,
			     UNCALI_GYRO_VALUE_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	input_set_events_per_packet(dev, 32);	/* test */

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(ungyroenablenodata, S_IWUSR | S_IRUGO, uncali_gyro_show_enable_nodata,
	    uncali_gyro_store_enable_nodata);
DEVICE_ATTR(ungyroactive, S_IWUSR | S_IRUGO, uncali_gyro_show_active, uncali_gyro_store_active);
DEVICE_ATTR(ungyrodelay, S_IWUSR | S_IRUGO, uncali_gyro_show_delay, uncali_gyro_store_delay);
DEVICE_ATTR(ungyrobatch, S_IWUSR | S_IRUGO, uncali_gyro_show_batch, uncali_gyro_store_batch);
DEVICE_ATTR(ungyroflush, S_IWUSR | S_IRUGO, uncali_gyro_show_flush, uncali_gyro_store_flush);
DEVICE_ATTR(ungyrodevnum, S_IWUSR | S_IRUGO, uncali_gyro_show_sensordevnum, NULL);

static struct attribute *uncali_gyro_attributes[] = {
	&dev_attr_ungyroenablenodata.attr,
	&dev_attr_ungyroactive.attr,
	&dev_attr_ungyrodelay.attr,
	&dev_attr_ungyrobatch.attr,
	&dev_attr_ungyroflush.attr,
	&dev_attr_ungyrodevnum.attr,
	NULL
};

static struct attribute_group uncali_gyro_attribute_group = {
	.attrs = uncali_gyro_attributes
};

int uncali_gyro_register_data_path(struct uncali_gyro_data_path *data)
{
	struct uncali_gyro_context *cxt = NULL;
	/* int err =0; */
	cxt = uncali_gyro_context_obj;
	cxt->uncali_gyro_data.get_data = data->get_data;
	cxt->uncali_gyro_data.vender_div = data->vender_div;
	UNCALI_GYRO_LOG("uncali_gyro register data path vender_div: %d\n",
			cxt->uncali_gyro_data.vender_div);
	if (NULL == cxt->uncali_gyro_data.get_data) {
		UNCALI_GYRO_LOG("uncali_gyro register data path fail\n");
		return -1;
	}
	return 0;
}

int uncali_gyro_register_control_path(struct uncali_gyro_control_path *ctl)
{
	struct uncali_gyro_context *cxt = NULL;
	int err = 0;

	cxt = uncali_gyro_context_obj;
	cxt->uncali_gyro_ctl.set_delay = ctl->set_delay;
	cxt->uncali_gyro_ctl.open_report_data = ctl->open_report_data;
	cxt->uncali_gyro_ctl.enable_nodata = ctl->enable_nodata;
	cxt->uncali_gyro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->uncali_gyro_ctl.is_report_input_direct = ctl->is_report_input_direct;

	if (NULL == cxt->uncali_gyro_ctl.set_delay || NULL == cxt->uncali_gyro_ctl.open_report_data
	    || NULL == cxt->uncali_gyro_ctl.enable_nodata) {
		UNCALI_GYRO_LOG("uncali_gyro register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = uncali_gyro_misc_init(uncali_gyro_context_obj);
	if (err) {
		UNCALI_GYRO_ERR("unable to register uncali_gyro misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&uncali_gyro_context_obj->mdev.this_device->kobj,
				 &uncali_gyro_attribute_group);
	if (err < 0) {
		UNCALI_GYRO_ERR("unable to create uncali_gyro attribute file\n");
		return -3;
	}

	kobject_uevent(&uncali_gyro_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int uncali_gyro_data_report(int *data, int status, int64_t nt)
{
	/* UNCALI_GYRO_LOG("+uncali_gyro_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct uncali_gyro_context *cxt = NULL;

	cxt = uncali_gyro_context_obj;
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_GYRO_X, data[0]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_GYRO_Y, data[1]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_GYRO_Z, data[2]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_GYRO_X_BIAS, data[3]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_GYRO_Y_BIAS, data[4]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_GYRO_Z_BIAS, data[5]);
	input_report_rel(cxt->idev, EVENT_TYPE_UNCALI_GYRO_UPDATE, 1);
	input_report_rel(cxt->idev, EVENT_TYPE_UNCALI_GYRO_TIMESTAMP_HI, nt >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_UNCALI_GYRO_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
	input_sync(cxt->idev);
	return 0;
}

static int uncali_gyro_probe(void)
{

	int err;

	UNCALI_GYRO_LOG("+++++++++++++uncali_gyro_probe!!\n");

	uncali_gyro_context_obj = uncali_gyro_context_alloc_object();
	if (!uncali_gyro_context_obj) {
		err = -ENOMEM;
		UNCALI_GYRO_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real uncali_gyroeleration driver */
	err = uncali_gyro_real_driver_init();
	if (err) {
		UNCALI_GYRO_ERR("uncali_gyro real driver init fail\n");
		goto real_driver_init_fail;
	}

	/* init input dev */
	err = uncali_gyro_input_init(uncali_gyro_context_obj);
	if (err) {
		UNCALI_GYRO_ERR("unable to register uncali_gyro input device!\n");
		goto exit_alloc_input_dev_failed;
	}


	UNCALI_GYRO_LOG("----uncali_gyro_probe OK !!\n");
	return 0;

	/* exit_hwmsen_create_attr_failed: */
	/* exit_misc_register_failed: */

	/* exit_err_sysfs: */

	if (err) {
		UNCALI_GYRO_ERR("sysfs node creation error\n");
		uncali_gyro_input_destroy(uncali_gyro_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(uncali_gyro_context_obj);

exit_alloc_data_failed:


	UNCALI_GYRO_LOG("----uncali_gyro_probe fail !!!\n");
	return err;
}



static int uncali_gyro_remove(void)
{
	int err = 0;

	UNCALI_GYRO_FUN(f);
	input_unregister_device(uncali_gyro_context_obj->idev);
	sysfs_remove_group(&uncali_gyro_context_obj->idev->dev.kobj, &uncali_gyro_attribute_group);

	err = misc_deregister(&uncali_gyro_context_obj->mdev);
	if (err)
		UNCALI_GYRO_ERR("misc_deregister fail: %d\n", err);
	kfree(uncali_gyro_context_obj);

	return 0;
}
int uncali_gyro_driver_add(struct uncali_gyro_init_info *obj)
{
	int err = 0;
	int i = 0;

	UNCALI_GYRO_FUN();

	for (i = 0; i < MAX_CHOOSE_UNCALI_GYRO_NUM; i++) {
		if ((i == 0) && (NULL == uncali_gyrosensor_init_list[0])) {
			UNCALI_GYRO_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&uncali_gyrosensor_driver))
				UNCALI_GYRO_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == uncali_gyrosensor_init_list[i]) {
			obj->platform_diver_addr = &uncali_gyrosensor_driver;
			uncali_gyrosensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_UNCALI_GYRO_NUM) {
		UNCALI_GYRO_ERR("UNCALI_GYRO driver add err\n");
		err = -1;
	}
	return err;
} EXPORT_SYMBOL_GPL(uncali_gyro_driver_add);

static int __init uncali_gyro_init(void)
{
	UNCALI_GYRO_FUN();

	if (uncali_gyro_probe()) {
		UNCALI_GYRO_ERR("failed to register rv driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit uncali_gyro_exit(void)
{
	uncali_gyro_remove();
	platform_driver_unregister(&uncali_gyrosensor_driver);
}

late_initcall(uncali_gyro_init);
/* module_init(uncali_gyro_init); */
/* module_exit(uncali_gyro_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("UNCALI_GYROCOPE device driver");
MODULE_AUTHOR("Mediatek");

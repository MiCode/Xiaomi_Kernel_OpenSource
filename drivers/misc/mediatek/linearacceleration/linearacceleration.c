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


#include "linearacceleration.h"

static struct la_context *la_context_obj;


static struct la_init_info *linearaccelerationsensor_init_list[MAX_CHOOSE_LA_NUM] = { 0 };

static void la_work_func(struct work_struct *work)
{
	struct la_context *cxt = NULL;
	int x, y, z, status;
	int64_t nt;
	struct timespec time;
	int err;

	LA_FUN();

	cxt = la_context_obj;

	if (NULL == cxt->la_data.get_data)
		LA_LOG("la driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->la_data.get_data(&x, &y, &z, &status);

	if (err) {
		LA_ERR("get la data fails!!\n");
		goto la_loop;
	} else {
		{
			if (0 == x && 0 == y && 0 == z)
				goto la_loop;

			cxt->drv_data.la_data.values[0] = x + cxt->cali_sw[0];
			cxt->drv_data.la_data.values[1] = y + cxt->cali_sw[1];
			cxt->drv_data.la_data.values[2] = z + cxt->cali_sw[2];
			cxt->drv_data.la_data.status = status;
			cxt->drv_data.la_data.time = nt;

		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		if (LA_INVALID_VALUE == cxt->drv_data.la_data.values[0] ||
		    LA_INVALID_VALUE == cxt->drv_data.la_data.values[1] ||
		    LA_INVALID_VALUE == cxt->drv_data.la_data.values[2]) {
			LA_LOG(" read invalid data\n");
			goto la_loop;

		}
	}

	la_data_report(cxt->drv_data.la_data.values[0],
		       cxt->drv_data.la_data.values[1], cxt->drv_data.la_data.values[2],
		       cxt->drv_data.la_data.status, nt);

la_loop:
	if (true == cxt->is_polling_run)
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
}

static void la_poll(unsigned long data)
{
	struct la_context *obj = (struct la_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct la_context *la_context_alloc_object(void)
{

	struct la_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	LA_LOG("la_context_alloc_object++++\n");
	if (!obj) {
		LA_ERR("Alloc linearacceleration object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->enable, 0);
	INIT_WORK(&obj->report, la_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = la_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->la_op_mutex);
	obj->is_batch_enable = false;
	obj->cali_sw[LA_AXIS_X] = 0;
	obj->cali_sw[LA_AXIS_Y] = 0;
	obj->cali_sw[LA_AXIS_Z] = 0;
	LA_LOG("la_context_alloc_object----\n");
	return obj;
}

static int la_real_enable(int enable)
{
	int err = 0;
	struct la_context *cxt = NULL;

	LA_FUN();

	cxt = la_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->la_ctl.enable_nodata(1);
			if (err) {
				err = cxt->la_ctl.enable_nodata(1);
				if (err) {
					err = cxt->la_ctl.enable_nodata(1);
					if (err)
						LA_ERR("la enable(%d) err 3 timers = %d\n", enable,
						       err);
				}
			}
			LA_LOG("la real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->la_ctl.enable_nodata(0);
			if (err)
				LA_ERR("la enable(%d) err = %d\n", enable, err);
			LA_LOG("la real disable\n");
		}

	}

	return err;
}

static int la_enable_data(int enable)
{
	struct la_context *cxt = NULL;

	LA_FUN();

	cxt = la_context_obj;
	if (NULL == cxt->la_ctl.open_report_data) {
		LA_ERR("no la control path\n");
		return -1;
	}

	if (1 == enable) {
		LA_LOG("LA enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->la_ctl.open_report_data(1);
		la_real_enable(enable);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->la_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		LA_LOG("LA disable\n");

		cxt->is_active_data = false;
		cxt->la_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->la_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.la_data.values[0] = LA_INVALID_VALUE;
				cxt->drv_data.la_data.values[1] = LA_INVALID_VALUE;
				cxt->drv_data.la_data.values[2] = LA_INVALID_VALUE;
			}
		}
		la_real_enable(enable);
	}
	return 0;
}



int la_enable_nodata(int enable)
{
	struct la_context *cxt = NULL;

	cxt = la_context_obj;
	if (NULL == cxt->la_ctl.enable_nodata) {
		LA_ERR("la_enable_nodata:la ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;
	la_real_enable(enable);
	return 0;
}


static ssize_t la_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	LA_LOG(" not support now\n");
	return len;
}

static ssize_t la_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct la_context *cxt = NULL;

	LA_LOG("la_store_enable nodata buf=%s\n", buf);
	mutex_lock(&la_context_obj->la_op_mutex);

	cxt = la_context_obj;
	if (NULL == cxt->la_ctl.enable_nodata) {
		LA_LOG("la_ctl enable nodata NULL\n");
		mutex_unlock(&la_context_obj->la_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		la_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		la_enable_nodata(0);
	else
		LA_ERR(" la_store enable nodata cmd error !!\n");
	mutex_unlock(&la_context_obj->la_op_mutex);
	return count;
}

static ssize_t la_store_active(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct la_context *cxt = NULL;

	LA_LOG("la_store_active buf=%s\n", buf);
	mutex_lock(&la_context_obj->la_op_mutex);
	cxt = la_context_obj;
	if (NULL == cxt->la_ctl.open_report_data) {
		LA_LOG("la_ctl enable NULL\n");
		mutex_unlock(&la_context_obj->la_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		la_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		la_enable_data(0);
	else
		LA_ERR(" la_store_active error !!\n");
	mutex_unlock(&la_context_obj->la_op_mutex);
	LA_LOG(" la_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t la_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct la_context *cxt = NULL;
	int div = 0;

	cxt = la_context_obj;
	div = cxt->la_data.vender_div;

	LA_LOG("la vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t la_store_delay(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int delay = 0;
	int mdelay = 0;
	struct la_context *cxt = NULL;

	mutex_lock(&la_context_obj->la_op_mutex);
	cxt = la_context_obj;
	if (NULL == cxt->la_ctl.set_delay) {
		LA_LOG("la_ctl set_delay NULL\n");
		mutex_unlock(&la_context_obj->la_op_mutex);
		return count;
	}

	if (0 != kstrtoint(buf, 10, &delay)) {
		LA_ERR("invalid format!!\n");
		mutex_unlock(&la_context_obj->la_op_mutex);
		return count;
	}

	if (false == cxt->la_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&la_context_obj->delay, mdelay);
	}
	cxt->la_ctl.set_delay(delay);
	LA_LOG(" la_delay %d ns\n", delay);
	mutex_unlock(&la_context_obj->la_op_mutex);
	return count;
}

static ssize_t la_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	LA_LOG(" not support now\n");
	return len;
}

static ssize_t la_show_sensordevnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct la_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = la_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}


static ssize_t la_store_batch(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{

	struct la_context *cxt = NULL;

	LA_LOG("la_store_batch buf=%s\n", buf);
	mutex_lock(&la_context_obj->la_op_mutex);
	cxt = la_context_obj;
	if (cxt->la_ctl.is_support_batch) {
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			if (cxt->is_active_data && cxt->is_polling_run) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
			}
		} else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			if (cxt->is_active_data)
				la_enable_data(true);
		} else
			LA_ERR(" la_store_batch error !!\n");
	} else
		LA_LOG(" la_store_batch mot supported\n");
	mutex_unlock(&la_context_obj->la_op_mutex);
	LA_LOG(" la_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t la_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t la_store_flush(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	return count;
}

static ssize_t la_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int linearaccelerationsensor_remove(struct platform_device *pdev)
{
	LA_LOG("linearaccelerationsensor_remove\n");
	return 0;
}

static int linearaccelerationsensor_probe(struct platform_device *pdev)
{
	LA_LOG("linearaccelerationsensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id linearaccelerationsensor_of_match[] = {
	{.compatible = "mediatek,linearaccel",},
	{},
};
#endif

static struct platform_driver linearaccelerationsensor_driver = {
	.probe = linearaccelerationsensor_probe,
	.remove = linearaccelerationsensor_remove,
	.driver = {

		   .name = "linearaccel",
#ifdef CONFIG_OF
		   .of_match_table = linearaccelerationsensor_of_match,
#endif
		   }
};

static int la_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	LA_LOG(" la_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_LA_NUM; i++) {
		LA_LOG(" i=%d\n", i);
		if (0 != linearaccelerationsensor_init_list[i]) {
			LA_LOG(" la try to init driver %s\n",
			       linearaccelerationsensor_init_list[i]->name);
			err = linearaccelerationsensor_init_list[i]->init();
			if (0 == err) {
				LA_LOG(" la real driver %s probe ok\n",
				       linearaccelerationsensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_LA_NUM) {
		LA_LOG(" la_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

static int la_misc_init(struct la_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = LA_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		LA_ERR("unable to register la misc device!!\n");
	return err;
}

static void la_input_destroy(struct la_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int la_input_init(struct la_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = LA_INPUTDEV_NAME;

	input_set_capability(dev, EV_REL, EVENT_TYPE_LA_X);
	input_set_capability(dev, EV_REL, EVENT_TYPE_LA_Y);
	input_set_capability(dev, EV_REL, EVENT_TYPE_LA_Z);
	input_set_capability(dev, EV_REL, EVENT_TYPE_LA_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_LA_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_LA_TIMESTAMP_LO);
	/*input_set_abs_params(dev, EVENT_TYPE_LA_X, LA_VALUE_MIN, LA_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_LA_Y, LA_VALUE_MIN, LA_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_LA_Z, LA_VALUE_MIN, LA_VALUE_MAX, 0, 0);*/
	input_set_drvdata(dev, cxt);

	input_set_events_per_packet(dev, 32);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(laenablenodata, S_IWUSR | S_IRUGO, la_show_enable_nodata, la_store_enable_nodata);
DEVICE_ATTR(laactive, S_IWUSR | S_IRUGO, la_show_active, la_store_active);
DEVICE_ATTR(ladelay, S_IWUSR | S_IRUGO, la_show_delay, la_store_delay);
DEVICE_ATTR(labatch, S_IWUSR | S_IRUGO, la_show_batch, la_store_batch);
DEVICE_ATTR(laflush, S_IWUSR | S_IRUGO, la_show_flush, la_store_flush);
DEVICE_ATTR(ladevnum, S_IWUSR | S_IRUGO, la_show_sensordevnum, NULL);

static struct attribute *la_attributes[] = {
	&dev_attr_laenablenodata.attr,
	&dev_attr_laactive.attr,
	&dev_attr_ladelay.attr,
	&dev_attr_labatch.attr,
	&dev_attr_laflush.attr,
	&dev_attr_ladevnum.attr,
	NULL
};

static struct attribute_group la_attribute_group = {
	.attrs = la_attributes
};

int la_register_data_path(struct la_data_path *data)
{
	struct la_context *cxt = NULL;

	cxt = la_context_obj;
	cxt->la_data.get_data = data->get_data;
	cxt->la_data.get_raw_data = data->get_raw_data;
	cxt->la_data.vender_div = data->vender_div;
	LA_LOG("la register data path vender_div: %d\n", cxt->la_data.vender_div);
	if (NULL == cxt->la_data.get_data) {
		LA_LOG("la register data path fail\n");
		return -1;
	}
	return 0;
}

int la_register_control_path(struct la_control_path *ctl)
{
	struct la_context *cxt = NULL;
	int err = 0;

	cxt = la_context_obj;
	cxt->la_ctl.set_delay = ctl->set_delay;
	cxt->la_ctl.open_report_data = ctl->open_report_data;
	cxt->la_ctl.enable_nodata = ctl->enable_nodata;
	cxt->la_ctl.is_support_batch = ctl->is_support_batch;
	cxt->la_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->la_ctl.la_calibration = ctl->la_calibration;

	if (NULL == cxt->la_ctl.set_delay || NULL == cxt->la_ctl.open_report_data
	    || NULL == cxt->la_ctl.enable_nodata) {
		LA_LOG("la register control path fail\n");
		return -1;
	}

	err = la_misc_init(la_context_obj);
	if (err) {
		LA_ERR("unable to register la misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&la_context_obj->mdev.this_device->kobj, &la_attribute_group);
	if (err < 0) {
		LA_ERR("unable to create la attribute file\n");
		return -3;
	}

	kobject_uevent(&la_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int la_data_report(int x, int y, int z, int status, int64_t nt)
{
	struct la_context *cxt = NULL;
	int err = 0;

	cxt = la_context_obj;

	/* LA_LOG("la_data_report! %d, %d, %d, %d\n", x, y, z, status); */

	input_report_rel(cxt->idev, EVENT_TYPE_LA_X, x);
	input_report_rel(cxt->idev, EVENT_TYPE_LA_Y, y);
	input_report_rel(cxt->idev, EVENT_TYPE_LA_Z, z);
	input_report_rel(cxt->idev, EVENT_TYPE_LA_STATUS, status);
	input_report_rel(cxt->idev, EVENT_TYPE_LA_TIMESTAMP_HI, nt >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_LA_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
	input_sync(cxt->idev);
	return err;
}

static int la_probe(void)
{

	int err;

	LA_LOG("+++++++++++++linearacceleration_probe!!\n");

	la_context_obj = la_context_alloc_object();
	if (!la_context_obj) {
		err = -ENOMEM;
		LA_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	err = la_real_driver_init();
	if (err) {
		LA_ERR("la real driver init fail\n");
		goto real_driver_init_fail;
	}

	err = la_input_init(la_context_obj);
	if (err) {
		LA_ERR("unable to register la input device!\n");
		goto exit_alloc_input_dev_failed;
	}


	LA_LOG("----linearacceleration_probe OK !!\n");
	return 0;

	if (err) {
		LA_ERR("sysfs node creation error\n");
		la_input_destroy(la_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(la_context_obj);

exit_alloc_data_failed:


	LA_LOG("----linearacceleration_probe fail !!!\n");
	return err;
}



static int la_remove(void)
{
	int err = 0;

	LA_FUN(f);
	input_unregister_device(la_context_obj->idev);
	sysfs_remove_group(&la_context_obj->idev->dev.kobj, &la_attribute_group);

	err = misc_deregister(&la_context_obj->mdev);
	if (err)
		LA_ERR("misc_deregister fail: %d\n", err);
	kfree(la_context_obj);

	return 0;
}
int la_driver_add(struct la_init_info *obj)
{
	int err = 0;
	int i = 0;

	LA_FUN();

	for (i = 0; i < MAX_CHOOSE_LA_NUM; i++) {
		if ((i == 0) && (NULL == linearaccelerationsensor_init_list[0])) {
			LA_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&linearaccelerationsensor_driver))
				LA_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == linearaccelerationsensor_init_list[i]) {
			obj->platform_diver_addr = &linearaccelerationsensor_driver;
			linearaccelerationsensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_LA_NUM) {
		LA_ERR("LA driver add err\n");
		err = -1;
	}
	return err;
}

static int __init la_init(void)
{
	LA_FUN();

	if (la_probe()) {
		LA_ERR("failed to register la driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit la_exit(void)
{
	la_remove();
	platform_driver_unregister(&linearaccelerationsensor_driver);
}

late_initcall(la_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LINEARACCEL device driver");
MODULE_AUTHOR("Mediatek");

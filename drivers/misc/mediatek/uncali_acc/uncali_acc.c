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

#include "uncali_acc.h"

static struct uncali_acc_context *uncali_acc_context_obj;


static struct uncali_acc_init_info *uncali_accsensor_init_list[MAX_CHOOSE_UNCALI_ACC_NUM] = { 0 };

static void uncali_acc_work_func(struct work_struct *work)
{

	struct uncali_acc_context *cxt = NULL;
	int dat[3], offset[3], status;
	int64_t nt;
	struct timespec time;
	int err;

	cxt = uncali_acc_context_obj;

	if (NULL == cxt->uncali_acc_data.get_data)
		UNCALI_ACC_LOG("uncali_acc driver not register data path\n");


	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->uncali_acc_data.get_data(dat, offset, &status);
	if (err) {
		UNCALI_ACC_ERR("get uncali_acc data fails!!\n");
		goto uncali_acc_loop;
	} else {
		{
			if (0 == dat[0] && 0 == dat[1] && 0 == dat[2])
				goto uncali_acc_loop;

			cxt->drv_data.uncali_acc_data.values[0] = dat[0];
			cxt->drv_data.uncali_acc_data.values[1] = dat[1];
			cxt->drv_data.uncali_acc_data.values[2] = dat[2];
			cxt->drv_data.uncali_acc_data.values[3] = offset[0];
			cxt->drv_data.uncali_acc_data.values[4] = offset[1];
			cxt->drv_data.uncali_acc_data.values[5] = offset[2];
			cxt->drv_data.uncali_acc_data.status = status;
			cxt->drv_data.uncali_acc_data.time = nt;

		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (UNCALI_ACC_INVALID_VALUE == cxt->drv_data.uncali_acc_data.values[0] ||
		    UNCALI_ACC_INVALID_VALUE == cxt->drv_data.uncali_acc_data.values[1] ||
		    UNCALI_ACC_INVALID_VALUE == cxt->drv_data.uncali_acc_data.values[2] ||
		    UNCALI_ACC_INVALID_VALUE == cxt->drv_data.uncali_acc_data.values[3]
		    ) {
			UNCALI_ACC_LOG(" read invalid data\n");
			goto uncali_acc_loop;

		}
	}

	uncali_acc_data_report(cxt->drv_data.uncali_acc_data.values,
			       cxt->drv_data.uncali_acc_data.status);

uncali_acc_loop:
	if (true == cxt->is_polling_run)
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
}

static void uncali_acc_poll(unsigned long data)
{
	struct uncali_acc_context *obj = (struct uncali_acc_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct uncali_acc_context *uncali_acc_context_alloc_object(void)
{

	struct uncali_acc_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	UNCALI_ACC_LOG("uncali_acc_context_alloc_object++++\n");
	if (!obj) {
		UNCALI_ACC_ERR("Alloc uncali_acc object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->enable, 0);
	INIT_WORK(&obj->report, uncali_acc_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = uncali_acc_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->uncali_acc_op_mutex);
	obj->is_batch_enable = false;	/* for batch mode init */
	UNCALI_ACC_LOG("uncali_acc_context_alloc_object----\n");
	return obj;
}

static int uncali_acc_real_enable(int enable)
{
	int err = 0;
	struct uncali_acc_context *cxt = NULL;

	cxt = uncali_acc_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->uncali_acc_ctl.enable_nodata(1);
			if (err) {
				err = cxt->uncali_acc_ctl.enable_nodata(1);
				if (err) {
					err = cxt->uncali_acc_ctl.enable_nodata(1);
					if (err)
						UNCALI_ACC_ERR
						    ("uncali_acc enable(%d) err 3 timers = %d\n",
						     enable, err);
				}
			}
			UNCALI_ACC_LOG("uncali_acc real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->uncali_acc_ctl.enable_nodata(0);
			if (err)
				UNCALI_ACC_ERR("uncali_acc enable(%d) err = %d\n", enable, err);
			UNCALI_ACC_LOG("uncali_acc real disable\n");
		}

	}

	return err;
}

static int uncali_acc_enable_data(int enable)
{
	struct uncali_acc_context *cxt = NULL;

	/* int err =0; */
	cxt = uncali_acc_context_obj;
	if (NULL == cxt->uncali_acc_ctl.open_report_data) {
		UNCALI_ACC_ERR("no uncali_acc control path\n");
		return -1;
	}

	if (1 == enable) {
		UNCALI_ACC_LOG("UNCALI_ACC enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->uncali_acc_ctl.open_report_data(1);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->uncali_acc_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		UNCALI_ACC_LOG("UNCALI_ACC disable\n");

		cxt->is_active_data = false;
		cxt->uncali_acc_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->uncali_acc_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.uncali_acc_data.values[0] = UNCALI_ACC_INVALID_VALUE;
				cxt->drv_data.uncali_acc_data.values[1] = UNCALI_ACC_INVALID_VALUE;
				cxt->drv_data.uncali_acc_data.values[2] = UNCALI_ACC_INVALID_VALUE;
			}
		}

	}
	uncali_acc_real_enable(enable);
	return 0;
}



int uncali_acc_enable_nodata(int enable)
{
	struct uncali_acc_context *cxt = NULL;

	/* int err =0; */
	cxt = uncali_acc_context_obj;
	if (NULL == cxt->uncali_acc_ctl.enable_nodata) {
		UNCALI_ACC_ERR("uncali_acc_enable_nodata:uncali_acc ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;
	uncali_acc_real_enable(enable);
	return 0;
}


static ssize_t uncali_acc_show_enable_nodata(struct device *dev,
					     struct device_attribute *attr, char *buf)
{
	int len = 0;

	UNCALI_ACC_LOG(" not support now\n");
	return len;
}

static ssize_t uncali_acc_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t count)
{
	struct uncali_acc_context *cxt = NULL;
	/* int err =0; */

	UNCALI_ACC_LOG("uncali_acc_store_enable nodata buf=%s\n", buf);
	mutex_lock(&uncali_acc_context_obj->uncali_acc_op_mutex);

	cxt = uncali_acc_context_obj;
	if (NULL == cxt->uncali_acc_ctl.enable_nodata) {
		UNCALI_ACC_LOG("uncali_acc_ctl enable nodata NULL\n");
		mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		uncali_acc_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		uncali_acc_enable_nodata(0);
	else
		UNCALI_ACC_ERR(" uncali_acc_store enable nodata cmd error !!\n");
	mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);

	return 0;
}

static ssize_t uncali_acc_store_active(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct uncali_acc_context *cxt = NULL;
	/* int err =0; */

	UNCALI_ACC_LOG("uncali_acc_store_active buf=%s\n", buf);
	mutex_lock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	cxt = uncali_acc_context_obj;
	if (NULL == cxt->uncali_acc_ctl.open_report_data) {
		UNCALI_ACC_LOG("uncali_acc_ctl enable NULL\n");
		mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		uncali_acc_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		uncali_acc_enable_data(0);
	else
		UNCALI_ACC_ERR(" uncali_acc_store_active error !!\n");
	mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	UNCALI_ACC_LOG(" uncali_acc_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t uncali_acc_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct uncali_acc_context *cxt = NULL;
	int div;

	cxt = uncali_acc_context_obj;
	div = cxt->uncali_acc_data.vender_div;

	UNCALI_ACC_LOG("uncali_acc vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t uncali_acc_store_delay(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int delay;
	int mdelay = 0;
	struct uncali_acc_context *cxt = NULL;

	mutex_lock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	cxt = uncali_acc_context_obj;
	if (NULL == cxt->uncali_acc_ctl.set_delay) {
		UNCALI_ACC_LOG("uncali_acc_ctl set_delay NULL\n");
		mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
		return count;
	}

	if (1 != kstrtoint(buf, 10, &delay)) {
		UNCALI_ACC_ERR("invalid format!!\n");
		mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
		return count;
	}

	if (false == cxt->uncali_acc_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&uncali_acc_context_obj->delay, mdelay);
	}
	cxt->uncali_acc_ctl.set_delay(delay);
	UNCALI_ACC_LOG(" uncali_acc_delay %d ns\n", delay);
	mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	return count;
}

static ssize_t uncali_acc_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	UNCALI_ACC_LOG(" not support now\n");
	return len;
}

static ssize_t uncali_acc_show_sensordevnum(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	struct uncali_acc_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = uncali_acc_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}


static ssize_t uncali_acc_store_batch(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{

	struct uncali_acc_context *cxt = NULL;

	UNCALI_ACC_LOG("uncali_acc_store_batch buf=%s\n", buf);
	mutex_lock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	cxt = uncali_acc_context_obj;
	if (cxt->uncali_acc_ctl.is_support_batch) {
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
				uncali_acc_enable_data(true);
		} else
			UNCALI_ACC_ERR(" uncali_acc_store_batch error !!\n");
	} else
		UNCALI_ACC_LOG(" uncali_acc_store_batch mot supported\n");
	mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	UNCALI_ACC_LOG(" uncali_acc_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t uncali_acc_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t uncali_acc_store_flush(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	/* struct uncali_acc_context *devobj = (struct uncali_acc_context*)dev_get_drvdata(dev); */
	mutex_lock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	/* do read FIFO data function and report data immediately */
	mutex_unlock(&uncali_acc_context_obj->uncali_acc_op_mutex);
	return count;
}

static ssize_t uncali_acc_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int uncali_accsensor_remove(struct platform_device *pdev)
{
	UNCALI_ACC_LOG("uncali_accsensor_remove\n");
	return 0;
}

static int uncali_accsensor_probe(struct platform_device *pdev)
{
	UNCALI_ACC_LOG("uncali_accsensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id uncali_accsensor_of_match[] = {
	{.compatible = "mediatek,uncali_accsensor",},
	{},
};
#endif

static struct platform_driver uncali_accsensor_driver = {
	.probe = uncali_accsensor_probe,
	.remove = uncali_accsensor_remove,
	.driver = {

		   .name = "uncali_accsensor",
#ifdef CONFIG_OF
		   .of_match_table = uncali_accsensor_of_match,
#endif
		   }
};

static int uncali_acc_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	UNCALI_ACC_LOG(" uncali_acc_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_UNCALI_ACC_NUM; i++) {
		UNCALI_ACC_LOG(" i=%d\n", i);
		if (0 != uncali_accsensor_init_list[i]) {
			UNCALI_ACC_LOG(" uncali_acc try to init driver %s\n",
				       uncali_accsensor_init_list[i]->name);
			err = uncali_accsensor_init_list[i]->init();
			if (0 == err) {
				UNCALI_ACC_LOG(" uncali_acc real driver %s probe ok\n",
					       uncali_accsensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_UNCALI_ACC_NUM) {
		UNCALI_ACC_LOG(" uncali_acc_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

static int uncali_acc_misc_init(struct uncali_acc_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = UNCALI_ACC_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		UNCALI_ACC_ERR("unable to register uncali_acc misc device!!\n");
	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}

static void uncali_acc_input_destroy(struct uncali_acc_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int uncali_acc_input_init(struct uncali_acc_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = UNCALI_ACC_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_ACC_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_ACC_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_ACC_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_ACC_X_BIAS);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_ACC_Y_BIAS);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_UNCALI_ACC_Z_BIAS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_UNCALI_ACC_STATUS);

	input_set_abs_params(dev, EVENT_TYPE_UNCALI_ACC_X, UNCALI_ACC_VALUE_MIN,
			     UNCALI_ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_ACC_Y, UNCALI_ACC_VALUE_MIN,
			     UNCALI_ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_ACC_Z, UNCALI_ACC_VALUE_MIN,
			     UNCALI_ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_ACC_X_BIAS, UNCALI_ACC_VALUE_MIN,
			     UNCALI_ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_ACC_Y_BIAS, UNCALI_ACC_VALUE_MIN,
			     UNCALI_ACC_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_UNCALI_ACC_Z_BIAS, UNCALI_ACC_VALUE_MIN,
			     UNCALI_ACC_VALUE_MAX, 0, 0);
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

DEVICE_ATTR(unaccenablenodata, S_IWUSR | S_IRUGO, uncali_acc_show_enable_nodata,
	    uncali_acc_store_enable_nodata);
DEVICE_ATTR(unaccactive, S_IWUSR | S_IRUGO, uncali_acc_show_active, uncali_acc_store_active);
DEVICE_ATTR(unaccdelay, S_IWUSR | S_IRUGO, uncali_acc_show_delay, uncali_acc_store_delay);
DEVICE_ATTR(unaccbatch, S_IWUSR | S_IRUGO, uncali_acc_show_batch, uncali_acc_store_batch);
DEVICE_ATTR(unaccflush, S_IWUSR | S_IRUGO, uncali_acc_show_flush, uncali_acc_store_flush);
DEVICE_ATTR(unaccdevnum, S_IWUSR | S_IRUGO, uncali_acc_show_sensordevnum, NULL);

static struct attribute *uncali_acc_attributes[] = {
	&dev_attr_unaccenablenodata.attr,
	&dev_attr_unaccactive.attr,
	&dev_attr_unaccdelay.attr,
	&dev_attr_unaccbatch.attr,
	&dev_attr_unaccflush.attr,
	&dev_attr_unaccdevnum.attr,
	NULL
};

static struct attribute_group uncali_acc_attribute_group = {
	.attrs = uncali_acc_attributes
};

int uncali_acc_register_data_path(struct uncali_acc_data_path *data)
{
	struct uncali_acc_context *cxt = NULL;
	/* int err =0; */
	cxt = uncali_acc_context_obj;
	cxt->uncali_acc_data.get_data = data->get_data;
	cxt->uncali_acc_data.vender_div = data->vender_div;
	UNCALI_ACC_LOG("uncali_acc register data path vender_div: %d\n",
		       cxt->uncali_acc_data.vender_div);
	if (NULL == cxt->uncali_acc_data.get_data) {
		UNCALI_ACC_LOG("uncali_acc register data path fail\n");
		return -1;
	}
	return 0;
}

int uncali_acc_register_control_path(struct uncali_acc_control_path *ctl)
{
	struct uncali_acc_context *cxt = NULL;
	int err = 0;

	cxt = uncali_acc_context_obj;
	cxt->uncali_acc_ctl.set_delay = ctl->set_delay;
	cxt->uncali_acc_ctl.open_report_data = ctl->open_report_data;
	cxt->uncali_acc_ctl.enable_nodata = ctl->enable_nodata;
	cxt->uncali_acc_ctl.is_support_batch = ctl->is_support_batch;
	cxt->uncali_acc_ctl.is_report_input_direct = ctl->is_report_input_direct;

	if (NULL == cxt->uncali_acc_ctl.set_delay || NULL == cxt->uncali_acc_ctl.open_report_data
	    || NULL == cxt->uncali_acc_ctl.enable_nodata) {
		UNCALI_ACC_LOG("uncali_acc register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = uncali_acc_misc_init(uncali_acc_context_obj);
	if (err) {
		UNCALI_ACC_ERR("unable to register uncali_acc misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&uncali_acc_context_obj->mdev.this_device->kobj,
				 &uncali_acc_attribute_group);
	if (err < 0) {
		UNCALI_ACC_ERR("unable to create uncali_acc attribute file\n");
		return -3;
	}

	kobject_uevent(&uncali_acc_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int uncali_acc_data_report(int *data, int status)
{
	/* UNCALI_ACC_LOG("+uncali_acc_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct uncali_acc_context *cxt = NULL;

	cxt = uncali_acc_context_obj;
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_ACC_X, data[0]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_ACC_Y, data[1]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_ACC_Z, data[2]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_ACC_X_BIAS, data[3]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_ACC_Y_BIAS, data[4]);
	input_report_abs(cxt->idev, EVENT_TYPE_UNCALI_ACC_Z_BIAS, data[5]);
	input_report_rel(cxt->idev, EVENT_TYPE_UNCALI_ACC_STATUS, status);
	input_sync(cxt->idev);
	return 0;
}

static int uncali_acc_probe(struct platform_device *pdev)
{

	int err;

	UNCALI_ACC_LOG("+++++++++++++uncali_acc_probe!!\n");

	uncali_acc_context_obj = uncali_acc_context_alloc_object();
	if (!uncali_acc_context_obj) {
		err = -ENOMEM;
		UNCALI_ACC_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real uncali_acceleration driver */
	err = uncali_acc_real_driver_init();
	if (err) {
		UNCALI_ACC_ERR("uncali_acc real driver init fail\n");
		goto real_driver_init_fail;
	}

	/* init input dev */
	err = uncali_acc_input_init(uncali_acc_context_obj);
	if (err) {
		UNCALI_ACC_ERR("unable to register uncali_acc input device!\n");
		goto exit_alloc_input_dev_failed;
	}


	UNCALI_ACC_LOG("----uncali_acc_probe OK !!\n");
	return 0;

	/* exit_hwmsen_create_attr_failed: */
	/* exit_misc_register_failed: */

	/* exit_err_sysfs: */

	if (err) {
		UNCALI_ACC_ERR("sysfs node creation error\n");
		uncali_acc_input_destroy(uncali_acc_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(uncali_acc_context_obj);

exit_alloc_data_failed:


	UNCALI_ACC_LOG("----uncali_acc_probe fail !!!\n");
	return err;
}



static int uncali_acc_remove(struct platform_device *pdev)
{
	int err = 0;

	UNCALI_ACC_FUN(f);
	input_unregister_device(uncali_acc_context_obj->idev);
	sysfs_remove_group(&uncali_acc_context_obj->idev->dev.kobj, &uncali_acc_attribute_group);

	err = misc_deregister(&uncali_acc_context_obj->mdev);
	if (err)
		UNCALI_ACC_ERR("misc_deregister fail: %d\n", err);
	kfree(uncali_acc_context_obj);

	return 0;
}

static int uncali_acc_suspend(struct platform_device *dev, pm_message_t msg)
{
	return 0;
}

static int uncali_acc_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_unacc_pl_of_match[] = {
	{.compatible = "mediatek,m_uncali_acc_pl",},
	{},
};
#endif

static struct platform_driver uncali_acc_driver = {

	.probe = uncali_acc_probe,
	.remove = uncali_acc_remove,
	.suspend = uncali_acc_suspend,
	.resume = uncali_acc_resume,
	.driver = {

		   .name = UNCALI_ACC_PL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = m_unacc_pl_of_match,
#endif
		   }
};

int uncali_acc_driver_add(struct uncali_acc_init_info *obj)
{
	int err = 0;
	int i = 0;

	UNCALI_ACC_FUN();

	for (i = 0; i < MAX_CHOOSE_UNCALI_ACC_NUM; i++) {
		if ((i == 0) && (NULL == uncali_accsensor_init_list[0])) {
			UNCALI_ACC_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&uncali_accsensor_driver))
				UNCALI_ACC_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == uncali_accsensor_init_list[i]) {
			obj->platform_diver_addr = &uncali_accsensor_driver;
			uncali_accsensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_UNCALI_ACC_NUM) {
		UNCALI_ACC_ERR("UNCALI_ACC driver add err\n");
		err = -1;
	}
	return err;
} XPORT_SYMBOL_GPL(uncali_acc_driver_add);

static int __init uncali_acc_init(void)
{
	UNCALI_ACC_FUN();

	if (platform_driver_register(&uncali_acc_driver)) {
		UNCALI_ACC_ERR("failed to register rv driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit uncali_acc_exit(void)
{
	platform_driver_unregister(&uncali_acc_driver);
	platform_driver_unregister(&uncali_accsensor_driver);
}

late_initcall(uncali_acc_init);
/* module_init(uncali_acc_init); */
/* module_exit(uncali_acc_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("UNCALI_ACCCOPE device driver");
MODULE_AUTHOR("Mediatek");

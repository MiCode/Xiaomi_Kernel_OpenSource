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


#include "grv.h"

static struct grv_context *grv_context_obj;


static struct grv_init_info *grvsensor_init_list[MAX_CHOOSE_GRV_NUM] = { 0 };	/* modified */

static void grv_work_func(struct work_struct *work)
{

	struct grv_context *cxt = NULL;
	int x, y, z, scalar, status;
	int64_t nt;
	struct timespec time;
	int err;

	cxt = grv_context_obj;

	if (cxt->grv_data.get_data == NULL)
		GRV_LOG("grv driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->grv_data.get_data(&x, &y, &z, &scalar, &status);

	if (err) {
		GRV_ERR("get grv data fails!!\n");
		goto grv_loop;
	} else {
		{
			if (0 == x && 0 == y && 0 == z)
				goto grv_loop;

			cxt->drv_data.grv_data.values[0] = x;
			cxt->drv_data.grv_data.values[1] = y;
			cxt->drv_data.grv_data.values[2] = z;
			cxt->drv_data.grv_data.values[3] = scalar;
			cxt->drv_data.grv_data.status = status;
			cxt->drv_data.grv_data.time = nt;

		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (GRV_INVALID_VALUE == cxt->drv_data.grv_data.values[0] ||
		    GRV_INVALID_VALUE == cxt->drv_data.grv_data.values[1] ||
		    GRV_INVALID_VALUE == cxt->drv_data.grv_data.values[2] ||
		    GRV_INVALID_VALUE == cxt->drv_data.grv_data.values[3]
		    ) {
			GRV_LOG(" read invalid data\n");
			goto grv_loop;

		}
	}
	/* report data to input device */
	/* printk("new grv work run....\n"); */
	/* GRV_LOG("grv data[%d,%d,%d]\n" ,cxt->drv_data.grv_data.values[0], */
	/* cxt->drv_data.grv_data.values[1],cxt->drv_data.grv_data.values[2]); */

	grv_data_report(cxt->drv_data.grv_data.values[0],
			cxt->drv_data.grv_data.values[1],
			cxt->drv_data.grv_data.values[2],
			cxt->drv_data.grv_data.values[3], cxt->drv_data.grv_data.status,  nt);

grv_loop:
	if (true == cxt->is_polling_run)
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
}

static void grv_poll(unsigned long data)
{
	struct grv_context *obj = (struct grv_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct grv_context *grv_context_alloc_object(void)
{

	struct grv_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	GRV_LOG("grv_context_alloc_object++++\n");
	if (!obj) {
		GRV_ERR("Alloc grv object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);	/*5Hz set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->enable, 0);
	INIT_WORK(&obj->report, grv_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = grv_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->grv_op_mutex);
	obj->is_batch_enable = false;	/* for batch mode init */
	GRV_LOG("grv_context_alloc_object----\n");
	return obj;
}

static int grv_real_enable(int enable)
{
	int err = 0;
	struct grv_context *cxt = NULL;

	cxt = grv_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->grv_ctl.enable_nodata(1);
			if (err) {
				err = cxt->grv_ctl.enable_nodata(1);
				if (err) {
					err = cxt->grv_ctl.enable_nodata(1);
					if (err)
						GRV_ERR("grv enable(%d) err 3 timers = %d\n",
							enable, err);
				}
			}
			GRV_LOG("grv real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->grv_ctl.enable_nodata(0);
			if (err)
				GRV_ERR("grv enable(%d) err = %d\n", enable, err);

			GRV_LOG("grv real disable\n");
		}

	}

	return err;
}

static int grv_enable_data(int enable)
{
	struct grv_context *cxt = NULL;

	/* int err =0; */
	cxt = grv_context_obj;
	if (NULL == cxt->grv_ctl.open_report_data) {
		GRV_ERR("no grv control path\n");
		return -1;
	}

	if (1 == enable) {
		GRV_LOG("GRV enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->grv_ctl.open_report_data(1);
		grv_real_enable(enable);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->grv_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		GRV_LOG("GRV disable\n");

		cxt->is_active_data = false;
		cxt->grv_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->grv_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.grv_data.values[0] = GRV_INVALID_VALUE;
				cxt->drv_data.grv_data.values[1] = GRV_INVALID_VALUE;
				cxt->drv_data.grv_data.values[2] = GRV_INVALID_VALUE;
			}
		}
		grv_real_enable(enable);
	}
	return 0;
}



int grv_enable_nodata(int enable)
{
	struct grv_context *cxt = NULL;

	/* int err =0; */
	cxt = grv_context_obj;
	if (NULL == cxt->grv_ctl.enable_nodata) {
		GRV_ERR("grv_enable_nodata:grv ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	grv_real_enable(enable);
	return 0;
}


static ssize_t grv_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GRV_LOG(" not support now\n");
	return len;
}

static ssize_t grv_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct grv_context *cxt = NULL;
	/* int err =0; */

	GRV_LOG("grv_store_enable nodata buf=%s\n", buf);
	mutex_lock(&grv_context_obj->grv_op_mutex);

	cxt = grv_context_obj;
	if (NULL == cxt->grv_ctl.enable_nodata) {
		GRV_LOG("grv_ctl enable nodata NULL\n");
		mutex_unlock(&grv_context_obj->grv_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->grv_ctl.enable_nodata(1); */
		grv_enable_nodata(1);
	} else if (!strncmp(buf, "0", 1)) {
		/* cxt->grv_ctl.enable_nodata(0); */
		grv_enable_nodata(0);
	} else {
		GRV_ERR(" grv_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&grv_context_obj->grv_op_mutex);

	return 0;
}

static ssize_t grv_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct grv_context *cxt = NULL;
	/* int err =0; */

	GRV_LOG("grv_store_active buf=%s\n", buf);
	mutex_lock(&grv_context_obj->grv_op_mutex);
	cxt = grv_context_obj;
	if (NULL == cxt->grv_ctl.open_report_data) {
		GRV_LOG("grv_ctl enable NULL\n");
		mutex_unlock(&grv_context_obj->grv_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->grv_ctl.enable(1); */
		grv_enable_data(1);

	} else if (!strncmp(buf, "0", 1)) {

		/* cxt->grv_ctl.enable(0); */
		grv_enable_data(0);
	} else {
		GRV_ERR(" grv_store_active error !!\n");
	}
	mutex_unlock(&grv_context_obj->grv_op_mutex);
	GRV_LOG(" grv_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t grv_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct grv_context *cxt = NULL;
	int div;

	cxt = grv_context_obj;
	div = cxt->grv_data.vender_div;

	GRV_LOG("grv vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t grv_store_delay(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	/* struct grv_context *devobj = (struct grv_context*)dev_get_drvdata(dev); */
	int delay;
	int mdelay = 0;
	int err;
	struct grv_context *cxt = NULL;

	mutex_lock(&grv_context_obj->grv_op_mutex);
	/* int err =0; */
	cxt = grv_context_obj;
	if (NULL == cxt->grv_ctl.set_delay) {
		GRV_LOG("grv_ctl set_delay NULL\n");
		mutex_unlock(&grv_context_obj->grv_op_mutex);
		return count;
	}

	err = kstrtoint(buf, 10, &delay);
	if (err != 0) {
		GRV_ERR("invalid format!!\n");
		mutex_unlock(&grv_context_obj->grv_op_mutex);
		return count;
	}

	if (false == cxt->grv_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&grv_context_obj->delay, mdelay);
	}
	cxt->grv_ctl.set_delay(delay);
	GRV_LOG(" grv_delay %d ns\n", delay);
	mutex_unlock(&grv_context_obj->grv_op_mutex);
	return count;
}

static ssize_t grv_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GRV_LOG(" not support now\n");
	return len;
}

static ssize_t grv_show_sensordevnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct grv_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = grv_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}


static ssize_t grv_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{

	struct grv_context *cxt = NULL;

	/* int err =0; */
	GRV_LOG("grv_store_batch buf=%s\n", buf);
	mutex_lock(&grv_context_obj->grv_op_mutex);
	cxt = grv_context_obj;
	if (cxt->grv_ctl.is_support_batch) {
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
				grv_enable_data(true);

			/* MTK problem fix - end */
		} else {
			GRV_ERR(" grv_store_batch error !!\n");
		}
	} else {
		GRV_LOG(" grv_store_batch mot supported\n");
	}
	mutex_unlock(&grv_context_obj->grv_op_mutex);
	GRV_LOG(" grv_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t grv_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t grv_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	/* struct grv_context *devobj = (struct grv_context*)dev_get_drvdata(dev); */
	mutex_lock(&grv_context_obj->grv_op_mutex);
	/* do read FIFO data function and report data immediately */
	mutex_unlock(&grv_context_obj->grv_op_mutex);
	return count;
}

static ssize_t grv_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int grvsensor_remove(struct platform_device *pdev)
{
	GRV_LOG("grvsensor_remove\n");
	return 0;
}

static int grvsensor_probe(struct platform_device *pdev)
{
	GRV_LOG("grvsensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id grvsensor_of_match[] = {
	{.compatible = "mediatek,grv",},
	{},
};
#endif

static struct platform_driver grvsensor_driver = {
	.probe = grvsensor_probe,
	.remove = grvsensor_remove,
	.driver = {
		   .name = "grv",
#ifdef CONFIG_OF
		   .of_match_table = grvsensor_of_match,
#endif
		   }
};

static int grv_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	GRV_LOG(" grv_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_GRV_NUM; i++) {
		GRV_LOG(" i=%d\n", i);
		if (0 != grvsensor_init_list[i]) {
			GRV_LOG(" grv try to init driver %s\n", grvsensor_init_list[i]->name);
			err = grvsensor_init_list[i]->init();
			if (0 == err) {
				GRV_LOG(" grv real driver %s probe ok\n",
					grvsensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_GRV_NUM) {
		GRV_LOG(" grv_real_driver_init fail\n");
		err = -1;
	}
	return err;
}


static int grv_misc_init(struct grv_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = GRV_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		GRV_ERR("unable to register grv misc device!!\n");

	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}

static void grv_input_destroy(struct grv_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int grv_input_init(struct grv_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GRV_INPUTDEV_NAME;

	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_X);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_Y);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_Z);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_SCALAR);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GRV_TIMESTAMP_LO);
	/*input_set_abs_params(dev, EVENT_TYPE_GRV_X, GRV_VALUE_MIN, GRV_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GRV_Y, GRV_VALUE_MIN, GRV_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GRV_Z, GRV_VALUE_MIN, GRV_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GRV_SCALAR, GRV_VALUE_MIN, GRV_VALUE_MAX, 0, 0);*/
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

DEVICE_ATTR(grvenablenodata, S_IWUSR | S_IRUGO, grv_show_enable_nodata, grv_store_enable_nodata);
DEVICE_ATTR(grvactive, S_IWUSR | S_IRUGO, grv_show_active, grv_store_active);
DEVICE_ATTR(grvdelay, S_IWUSR | S_IRUGO, grv_show_delay, grv_store_delay);
DEVICE_ATTR(grvbatch, S_IWUSR | S_IRUGO, grv_show_batch, grv_store_batch);
DEVICE_ATTR(grvflush, S_IWUSR | S_IRUGO, grv_show_flush, grv_store_flush);
DEVICE_ATTR(grvdevnum, S_IWUSR | S_IRUGO, grv_show_sensordevnum, NULL);

static struct attribute *grv_attributes[] = {
	&dev_attr_grvenablenodata.attr,
	&dev_attr_grvactive.attr,
	&dev_attr_grvdelay.attr,
	&dev_attr_grvbatch.attr,
	&dev_attr_grvflush.attr,
	&dev_attr_grvdevnum.attr,
	NULL
};

static struct attribute_group grv_attribute_group = {
	.attrs = grv_attributes
};

int grv_register_data_path(struct grv_data_path *data)
{
	struct grv_context *cxt = NULL;
	/* int err =0; */
	cxt = grv_context_obj;
	cxt->grv_data.get_data = data->get_data;
	cxt->grv_data.vender_div = data->vender_div;
	GRV_LOG("grv register data path vender_div: %d\n", cxt->grv_data.vender_div);
	if (NULL == cxt->grv_data.get_data) {
		GRV_LOG("grv register data path fail\n");
		return -1;
	}
	return 0;
}

int grv_register_control_path(struct grv_control_path *ctl)
{
	struct grv_context *cxt = NULL;

	int err = 0;

	cxt = grv_context_obj;
	cxt->grv_ctl.set_delay = ctl->set_delay;
	cxt->grv_ctl.open_report_data = ctl->open_report_data;
	cxt->grv_ctl.enable_nodata = ctl->enable_nodata;
	cxt->grv_ctl.is_support_batch = ctl->is_support_batch;
	cxt->grv_ctl.is_report_input_direct = ctl->is_report_input_direct;

	if (NULL == cxt->grv_ctl.set_delay || NULL == cxt->grv_ctl.open_report_data
	    || NULL == cxt->grv_ctl.enable_nodata) {
		GRV_LOG("grv register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = grv_misc_init(grv_context_obj);
	if (err) {
		GRV_ERR("unable to register grv misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&grv_context_obj->mdev.this_device->kobj, &grv_attribute_group);
	if (err < 0) {
		GRV_ERR("unable to create grv attribute file\n");
		return -3;
	}

	kobject_uevent(&grv_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int grv_data_report(int x, int y, int z, int scalar, int status, int64_t nt)
{
	/* GRV_LOG("+grv_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct grv_context *cxt = NULL;

	cxt = grv_context_obj;
	input_report_rel(cxt->idev, EVENT_TYPE_GRV_X, x);
	input_report_rel(cxt->idev, EVENT_TYPE_GRV_Y, y);
	input_report_rel(cxt->idev, EVENT_TYPE_GRV_Z, z);
	input_report_rel(cxt->idev, EVENT_TYPE_GRV_SCALAR, scalar);
	input_report_rel(cxt->idev, EVENT_TYPE_GRV_TIMESTAMP_HI, nt >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_GRV_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
	/* input_report_rel(cxt->idev, EVENT_TYPE_GRV_STATUS, status); */
	input_sync(cxt->idev);
	return 0;
}

static int grv_probe(void)
{

	int err;

	GRV_LOG("+++++++++++++grv_probe!!\n");

	grv_context_obj = grv_context_alloc_object();
	if (!grv_context_obj) {
		err = -ENOMEM;
		GRV_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real grveleration driver */
	err = grv_real_driver_init();
	if (err) {
		GRV_ERR("grv real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = grv_input_init(grv_context_obj);
	if (err) {
		GRV_ERR("unable to register grv input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	GRV_LOG("----grv_probe OK !!\n");
	return 0;

	/* exit_hwmsen_create_attr_failed: */
	/* exit_misc_register_failed: */

	/* exit_err_sysfs: */

	if (err) {
		GRV_ERR("sysfs node creation error\n");
		grv_input_destroy(grv_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(grv_context_obj);

exit_alloc_data_failed:


	GRV_LOG("----grv_probe fail !!!\n");
	return err;
}



static int grv_remove(void)
{
	int err = 0;

	GRV_FUN(f);
	input_unregister_device(grv_context_obj->idev);
	sysfs_remove_group(&grv_context_obj->idev->dev.kobj, &grv_attribute_group);
	err = misc_deregister(&grv_context_obj->mdev);
	if (err)
		GRV_ERR("misc_deregister fail: %d\n", err);

	kfree(grv_context_obj);

	return 0;
}
int grv_driver_add(struct grv_init_info *obj)
{
	int err = 0;
	int i = 0;

	GRV_FUN();
	if (!obj) {
		GRV_ERR("GRV driver add fail, grv_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_GRV_NUM; i++) {
		if ((i == 0) && (NULL == grvsensor_init_list[0])) {
			GRV_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&grvsensor_driver))
				GRV_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == grvsensor_init_list[i]) {
			obj->platform_diver_addr = &grvsensor_driver;
			grvsensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_GRV_NUM) {
		GRV_ERR("GRV driver add err\n");
		err = -1;
	}
	return err;
}
static int __init grv_init(void)
{
	GRV_FUN();

	if (grv_probe()) {
		GRV_ERR("failed to register grv driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit grv_exit(void)
{
	grv_remove();
	platform_driver_unregister(&grvsensor_driver);
}

late_initcall(grv_init);
/* module_init(grv_init); */
/* module_exit(grv_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GRVCOPE device driver");
MODULE_AUTHOR("Mediatek");

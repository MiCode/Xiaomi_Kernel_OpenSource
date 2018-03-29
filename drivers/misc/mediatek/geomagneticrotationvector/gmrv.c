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


#include "gmrv.h"

static struct gmrv_context *gmrv_context_obj;


static struct gmrv_init_info *gmrvsensor_init_list[MAX_CHOOSE_GMRV_NUM] = { 0 };	/* modified */

static void gmrv_work_func(struct work_struct *work)
{

	struct gmrv_context *cxt = NULL;
	int x, y, z, scalar, status;
	int64_t nt;
	struct timespec time;
	int err;

	cxt = gmrv_context_obj;

	if (NULL == cxt->gmrv_data.get_data)
		GMRV_LOG("gmrv driver not register data path\n");


	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->gmrv_data.get_data(&x, &y, &z, &scalar, &status);

	if (err) {
		GMRV_ERR("get gmrv data fails!!\n");
		goto gmrv_loop;
	} else {
		{
			if (0 == x && 0 == y && 0 == z)
				goto gmrv_loop;

			cxt->drv_data.gmrv_data.values[0] = x;
			cxt->drv_data.gmrv_data.values[1] = y;
			cxt->drv_data.gmrv_data.values[2] = z;
			cxt->drv_data.gmrv_data.values[3] = scalar;
			cxt->drv_data.gmrv_data.status = status;
			cxt->drv_data.gmrv_data.time = nt;

		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (GMRV_INVALID_VALUE == cxt->drv_data.gmrv_data.values[0] ||
		    GMRV_INVALID_VALUE == cxt->drv_data.gmrv_data.values[1] ||
		    GMRV_INVALID_VALUE == cxt->drv_data.gmrv_data.values[2] ||
		    GMRV_INVALID_VALUE == cxt->drv_data.gmrv_data.values[3]
		    ) {
			GMRV_LOG(" read invalid data\n");
			goto gmrv_loop;

		}
	}
	/* report data to input device */
	/* printk("new gmrv work run....\n"); */
	/* GMRV_LOG("gmrv data[%d,%d,%d]\n" ,cxt->drv_data.gmrv_data.values[0], */
	/* cxt->drv_data.gmrv_data.values[1],cxt->drv_data.gmrv_data.values[2]); */

	gmrv_data_report(cxt->drv_data.gmrv_data.values[0],
			 cxt->drv_data.gmrv_data.values[1],
			 cxt->drv_data.gmrv_data.values[2],
			 cxt->drv_data.gmrv_data.values[3], cxt->drv_data.gmrv_data.status, nt);

gmrv_loop:
	if (true == cxt->is_polling_run)
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
}

static void gmrv_poll(unsigned long data)
{
	struct gmrv_context *obj = (struct gmrv_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct gmrv_context *gmrv_context_alloc_object(void)
{

	struct gmrv_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	GMRV_LOG("gmrv_context_alloc_object++++\n");
	if (!obj) {
		GMRV_ERR("Alloc gmrv object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);	/*5Hz set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->enable, 0);
	INIT_WORK(&obj->report, gmrv_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = gmrv_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->gmrv_op_mutex);
	obj->is_batch_enable = false;	/* for batch mode init */
	GMRV_LOG("gmrv_context_alloc_object----\n");
	return obj;
}

static int gmrv_real_enable(int enable)
{
	int err = 0;
	struct gmrv_context *cxt = NULL;

	cxt = gmrv_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->gmrv_ctl.enable_nodata(1);
			if (err) {
				err = cxt->gmrv_ctl.enable_nodata(1);
				if (err) {
					err = cxt->gmrv_ctl.enable_nodata(1);
					if (err)
						GMRV_ERR("gmrv enable(%d) err 3 timers = %d\n",
							 enable, err);
				}
			}
			GMRV_LOG("gmrv real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->gmrv_ctl.enable_nodata(0);
			if (err)
				GMRV_ERR("gmrv enable(%d) err = %d\n", enable, err);
			GMRV_LOG("gmrv real disable\n");
		}

	}

	return err;
}

static int gmrv_enable_data(int enable)
{
	struct gmrv_context *cxt = NULL;

	/* int err =0; */
	cxt = gmrv_context_obj;
	if (NULL == cxt->gmrv_ctl.open_report_data) {
		GMRV_ERR("no gmrv control path\n");
		return -1;
	}

	if (1 == enable) {
		GMRV_LOG("GMRV enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->gmrv_ctl.open_report_data(1);
		gmrv_real_enable(enable);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->gmrv_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		GMRV_LOG("GMRV disable\n");

		cxt->is_active_data = false;
		cxt->gmrv_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->gmrv_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.gmrv_data.values[0] = GMRV_INVALID_VALUE;
				cxt->drv_data.gmrv_data.values[1] = GMRV_INVALID_VALUE;
				cxt->drv_data.gmrv_data.values[2] = GMRV_INVALID_VALUE;
			}
		}
		gmrv_real_enable(enable);
	}
	return 0;
}



int gmrv_enable_nodata(int enable)
{
	struct gmrv_context *cxt = NULL;

	/* int err =0; */
	cxt = gmrv_context_obj;
	if (NULL == cxt->gmrv_ctl.enable_nodata) {
		GMRV_ERR("gmrv_enable_nodata:gmrv ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	gmrv_real_enable(enable);
	return 0;
}


static ssize_t gmrv_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GMRV_LOG(" not support now\n");
	return len;
}

static ssize_t gmrv_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct gmrv_context *cxt = NULL;
	/* int err =0; */

	GMRV_LOG("gmrv_store_enable nodata buf=%s\n", buf);
	mutex_lock(&gmrv_context_obj->gmrv_op_mutex);

	cxt = gmrv_context_obj;
	if (NULL == cxt->gmrv_ctl.enable_nodata) {
		GMRV_LOG("gmrv_ctl enable nodata NULL\n");
		mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->gmrv_ctl.enable_nodata(1); */
		gmrv_enable_nodata(1);
	} else if (!strncmp(buf, "0", 1)) {
		/* cxt->gmrv_ctl.enable_nodata(0); */
		gmrv_enable_nodata(0);
	} else {
		GMRV_ERR(" gmrv_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);

	return 0;
}

static ssize_t gmrv_store_active(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct gmrv_context *cxt = NULL;
	/* int err =0; */

	GMRV_LOG("gmrv_store_active buf=%s\n", buf);
	mutex_lock(&gmrv_context_obj->gmrv_op_mutex);
	cxt = gmrv_context_obj;
	if (NULL == cxt->gmrv_ctl.open_report_data) {
		GMRV_LOG("gmrv_ctl enable NULL\n");
		mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->gmrv_ctl.enable(1); */
		gmrv_enable_data(1);

	} else if (!strncmp(buf, "0", 1)) {

		/* cxt->gmrv_ctl.enable(0); */
		gmrv_enable_data(0);
	} else {
		GMRV_ERR(" gmrv_store_active error !!\n");
	}
	mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
	GMRV_LOG(" gmrv_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t gmrv_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gmrv_context *cxt = NULL;
	int div;

	cxt = gmrv_context_obj;
	div = cxt->gmrv_data.vender_div;

	GMRV_LOG("gmrv vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t gmrv_store_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	/* struct gmrv_context *devobj = (struct gmrv_context*)dev_get_drvdata(dev); */
	int delay;
	int mdelay = 0;
	struct gmrv_context *cxt = NULL;
	int err;

	mutex_lock(&gmrv_context_obj->gmrv_op_mutex);
	/* int err =0; */
	cxt = gmrv_context_obj;
	if (NULL == cxt->gmrv_ctl.set_delay) {
		GMRV_LOG("gmrv_ctl set_delay NULL\n");
		mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
		return count;
	}

	err = kstrtoint(buf, 10, &delay);
	if (err != 0) {
		GMRV_ERR("invalid format!!\n");
		mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
		return count;
	}

	if (false == cxt->gmrv_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&gmrv_context_obj->delay, mdelay);
	}
	cxt->gmrv_ctl.set_delay(delay);
	GMRV_LOG(" gmrv_delay %d ns\n", delay);
	mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
	return count;
}

static ssize_t gmrv_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GMRV_LOG(" not support now\n");
	return len;
}

static ssize_t gmrv_show_sensordevnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gmrv_context *cxt = NULL;
	const char *devname = NULL;
	struct input_handle *handle;

	cxt = gmrv_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}


static ssize_t gmrv_store_batch(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{

	struct gmrv_context *cxt = NULL;

	/* int err =0; */
	GMRV_LOG("gmrv_store_batch buf=%s\n", buf);
	mutex_lock(&gmrv_context_obj->gmrv_op_mutex);
	cxt = gmrv_context_obj;
	if (cxt->gmrv_ctl.is_support_batch) {
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
				gmrv_enable_data(true);
			/* MTK problem fix - end */
		} else {
			GMRV_ERR(" gmrv_store_batch error !!\n");
		}
	} else {
		GMRV_LOG(" gmrv_store_batch mot supported\n");
	}
	mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
	GMRV_LOG(" gmrv_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t gmrv_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t gmrv_store_flush(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	/* struct gmrv_context *devobj = (struct gmrv_context*)dev_get_drvdata(dev); */
	mutex_lock(&gmrv_context_obj->gmrv_op_mutex);
	/* do read FIFO data function and report data immediately */
	mutex_unlock(&gmrv_context_obj->gmrv_op_mutex);
	return count;
}

static ssize_t gmrv_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int gmrvsensor_remove(struct platform_device *pdev)
{
	GMRV_LOG("gmrvsensor_remove\n");
	return 0;
}

static int gmrvsensor_probe(struct platform_device *pdev)
{
	GMRV_LOG("gmrvsensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gmrvsensor_of_match[] = {
	{.compatible = "mediatek,gmagrotvec",},
	{},
};
#endif

static struct platform_driver gmrvsensor_driver = {
	.probe = gmrvsensor_probe,
	.remove = gmrvsensor_remove,
	.driver = {
		   .name = "gmagrotvec",
#ifdef CONFIG_OF
		   .of_match_table = gmrvsensor_of_match,
#endif
		   }
};

static int gmrv_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	GMRV_LOG(" gmrv_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_GMRV_NUM; i++) {
		GMRV_LOG(" i=%d\n", i);
		if (0 != gmrvsensor_init_list[i]) {
			GMRV_LOG(" gmrv try to init driver %s\n", gmrvsensor_init_list[i]->name);
			err = gmrvsensor_init_list[i]->init();
			if (0 == err) {
				GMRV_LOG(" gmrv real driver %s probe ok\n",
					 gmrvsensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_GMRV_NUM) {
		GMRV_LOG(" gmrv_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

static int gmrv_misc_init(struct gmrv_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = GMRV_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		GMRV_ERR("unable to register gmrv misc device!!\n");

	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}

static void gmrv_input_destroy(struct gmrv_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int gmrv_input_init(struct gmrv_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GMRV_INPUTDEV_NAME;

	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_X);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_Y);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_Z);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_SCALAR);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GMRV_TIMESTAMP_LO);
	/*input_set_abs_params(dev, EVENT_TYPE_GMRV_X, GMRV_VALUE_MIN, GMRV_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GMRV_Y, GMRV_VALUE_MIN, GMRV_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GMRV_Z, GMRV_VALUE_MIN, GMRV_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GMRV_SCALAR, GMRV_VALUE_MIN, GMRV_VALUE_MAX, 0, 0);*/
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

DEVICE_ATTR(gmrvenablenodata, S_IWUSR | S_IRUGO, gmrv_show_enable_nodata, gmrv_store_enable_nodata);
DEVICE_ATTR(gmrvactive, S_IWUSR | S_IRUGO, gmrv_show_active, gmrv_store_active);
DEVICE_ATTR(gmrvdelay, S_IWUSR | S_IRUGO, gmrv_show_delay, gmrv_store_delay);
DEVICE_ATTR(gmrvbatch, S_IWUSR | S_IRUGO, gmrv_show_batch, gmrv_store_batch);
DEVICE_ATTR(gmrvflush, S_IWUSR | S_IRUGO, gmrv_show_flush, gmrv_store_flush);
DEVICE_ATTR(gmrvdevnum, S_IWUSR | S_IRUGO, gmrv_show_sensordevnum, NULL);

static struct attribute *gmrv_attributes[] = {
	&dev_attr_gmrvenablenodata.attr,
	&dev_attr_gmrvactive.attr,
	&dev_attr_gmrvdelay.attr,
	&dev_attr_gmrvbatch.attr,
	&dev_attr_gmrvflush.attr,
	&dev_attr_gmrvdevnum.attr,
	NULL
};

static struct attribute_group gmrv_attribute_group = {
	.attrs = gmrv_attributes
};

int gmrv_register_data_path(struct gmrv_data_path *data)
{
	struct gmrv_context *cxt = NULL;
	/* int err =0; */
	cxt = gmrv_context_obj;
	cxt->gmrv_data.get_data = data->get_data;
	cxt->gmrv_data.vender_div = data->vender_div;
	GMRV_LOG("gmrv register data path vender_div: %d\n", cxt->gmrv_data.vender_div);
	if (NULL == cxt->gmrv_data.get_data) {
		GMRV_LOG("gmrv register data path fail\n");
		return -1;
	}
	return 0;
}

int gmrv_register_control_path(struct gmrv_control_path *ctl)
{
	struct gmrv_context *cxt = NULL;
	int err = 0;

	cxt = gmrv_context_obj;
	cxt->gmrv_ctl.set_delay = ctl->set_delay;
	cxt->gmrv_ctl.open_report_data = ctl->open_report_data;
	cxt->gmrv_ctl.enable_nodata = ctl->enable_nodata;
	cxt->gmrv_ctl.is_support_batch = ctl->is_support_batch;
	cxt->gmrv_ctl.is_report_input_direct = ctl->is_report_input_direct;

	if (NULL == cxt->gmrv_ctl.set_delay || NULL == cxt->gmrv_ctl.open_report_data
	    || NULL == cxt->gmrv_ctl.enable_nodata) {
		GMRV_LOG("gmrv register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = gmrv_misc_init(gmrv_context_obj);
	if (err) {
		GMRV_ERR("unable to register gmrv misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&gmrv_context_obj->mdev.this_device->kobj, &gmrv_attribute_group);
	if (err < 0) {
		GMRV_ERR("unable to create gmrv attribute file\n");
		return -3;
	}

	kobject_uevent(&gmrv_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int gmrv_data_report(int x, int y, int z, int scalar, int status, int64_t nt)
{
	/* GMRV_LOG("+gmrv_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct gmrv_context *cxt = NULL;

	cxt = gmrv_context_obj;
	input_report_rel(cxt->idev, EVENT_TYPE_GMRV_X, x);
	input_report_rel(cxt->idev, EVENT_TYPE_GMRV_Y, y);
	input_report_rel(cxt->idev, EVENT_TYPE_GMRV_Z, z);
	input_report_rel(cxt->idev, EVENT_TYPE_GMRV_SCALAR, scalar);
	input_report_rel(cxt->idev, EVENT_TYPE_GMRV_TIMESTAMP_HI, nt >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_GMRV_TIMESTAMP_LO, nt & 0xFFFFFFFFLL);
	/* input_report_rel(cxt->idev, EVENT_TYPE_GMRV_STATUS, status); */
	input_sync(cxt->idev);
	return 0;
}

static int gmrv_probe(void)
{

	int err;

	GMRV_LOG("+++++++++++++gmrv_probe!!\n");

	gmrv_context_obj = gmrv_context_alloc_object();
	if (!gmrv_context_obj) {
		err = -ENOMEM;
		GMRV_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real gmrveleration driver */
	err = gmrv_real_driver_init();
	if (err) {
		GMRV_ERR("gmrv real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = gmrv_input_init(gmrv_context_obj);
	if (err) {
		GMRV_ERR("unable to register gmrv input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	GMRV_LOG("----gmrv_probe OK !!\n");
	return 0;

	/* exit_hwmsen_create_attr_failed: */
	/* exit_misc_register_failed: */

	/* exit_err_sysfs: */

	if (err) {
		GMRV_ERR("sysfs node creation error\n");
		gmrv_input_destroy(gmrv_context_obj);
	}

real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(gmrv_context_obj);

exit_alloc_data_failed:


	GMRV_LOG("----gmrv_probe fail !!!\n");
	return err;
}



static int gmrv_remove(void)
{
	int err = 0;

	GMRV_FUN(f);
	input_unregister_device(gmrv_context_obj->idev);
	sysfs_remove_group(&gmrv_context_obj->idev->dev.kobj, &gmrv_attribute_group);

	err = misc_deregister(&gmrv_context_obj->mdev);
	if (err)
		GMRV_ERR("misc_deregister fail: %d\n", err);

	kfree(gmrv_context_obj);

	return 0;
}
int gmrv_driver_add(struct gmrv_init_info *obj)
{
	int err = 0;
	int i = 0;

	GMRV_FUN();

	for (i = 0; i < MAX_CHOOSE_GMRV_NUM; i++) {
		if ((i == 0) && (NULL == gmrvsensor_init_list[0])) {
			GMRV_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&gmrvsensor_driver))
				GMRV_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == gmrvsensor_init_list[i]) {
			obj->platform_diver_addr = &gmrvsensor_driver;
			gmrvsensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_GMRV_NUM) {
		GMRV_ERR("GMRV driver add err\n");
		err = -1;
	}
	return err;
}

static int __init gmrv_init(void)
{
	GMRV_FUN();

	if (gmrv_probe()) {
		GMRV_ERR("failed to register gmrv driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit gmrv_exit(void)
{
	gmrv_remove();
	platform_driver_unregister(&gmrvsensor_driver);
}

late_initcall(gmrv_init);
/* module_init(gmrv_init); */
/* module_exit(gmrv_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GMRVCOPE device driver");
MODULE_AUTHOR("Mediatek");

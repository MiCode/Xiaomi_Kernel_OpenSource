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


#include "pedometer.h"

struct pedo_context *pedo_context_obj = NULL;


static struct pedo_init_info *pedometer_init_list[MAX_CHOOSE_PEDO_NUM] = { 0 };

static void pedo_work_func(struct work_struct *work)
{

	struct pedo_context *cxt = NULL;

	struct hwm_sensor_data data;
	static int64_t last_time_stamp;
	int status;
	int64_t nt;
	struct timespec time;
	int err = 0;

	cxt = pedo_context_obj;

	if (NULL == cxt->pedo_data.get_data)
		PEDO_ERR("pedo driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->pedo_data.get_data(&data, &status);
	if (err) {
		PEDO_ERR("get pedo data fails!!\n");
		goto pedo_loop;
	} else {
		cxt->drv_data.pedo_data.values[0] = data.values[0];
		cxt->drv_data.pedo_data.values[1] = data.values[1];
		cxt->drv_data.pedo_data.values[2] = data.values[2];
		cxt->drv_data.pedo_data.values[3] = data.values[3];
		/*PEDO_LOG("pedo values %d,%d,%d,%d\n" ,
		   cxt->drv_data.pedo_data.values[0],
		   cxt->drv_data.pedo_data.values[1],
		   cxt->drv_data.pedo_data.values[2],
		   cxt->drv_data.pedo_data.values[3]); */
		cxt->drv_data.pedo_data.status = status;
		cxt->drv_data.pedo_data.time = data.time;
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		if (PEDO_INVALID_VALUE == cxt->drv_data.pedo_data.values[0] ||
		    PEDO_INVALID_VALUE == cxt->drv_data.pedo_data.values[1] ||
		    PEDO_INVALID_VALUE == cxt->drv_data.pedo_data.values[2] ||
		    PEDO_INVALID_VALUE == cxt->drv_data.pedo_data.values[3]) {
			PEDO_LOG(" read invalid data\n");
			goto pedo_loop;

		}
	}
	/*PEDO_LOG("pedo data %d,%d,%d %d\n" ,cxt->drv_data.pedo_data.values[0],
	   cxt->drv_data.pedo_data.values[1],cxt->drv_data.pedo_data.values[2],cxt->drv_data.pedo_data.values[3]); */
	if (last_time_stamp != cxt->drv_data.pedo_data.time) {
		last_time_stamp = cxt->drv_data.pedo_data.time;
		pedo_data_report(&cxt->drv_data.pedo_data, cxt->drv_data.pedo_data.status);
	}

pedo_loop:
	if (true == cxt->is_polling_run) {
		{
			mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
		}
	}
}

static void pedo_poll(unsigned long data)
{
	struct pedo_context *obj = (struct pedo_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct pedo_context *pedo_context_alloc_object(void)
{
	struct pedo_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	PEDO_LOG("pedo_context_alloc_object++++\n");
	if (!obj) {
		PEDO_ERR("Alloc pedo object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);	/*5Hz */
	atomic_set(&obj->wake, 0);
	atomic_set(&obj->enable, 0);
	INIT_WORK(&obj->report, pedo_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = pedo_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	mutex_init(&obj->pedo_op_mutex);
	PEDO_LOG("pedo_context_alloc_object----\n");
	return obj;
}

static int pedo_real_enable(int enable)
{
	int err = 0;
	struct pedo_context *cxt = NULL;

	cxt = pedo_context_obj;
	if (1 == enable) {
		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->pedo_ctl.enable_nodata(1);
			if (err) {
				err = cxt->pedo_ctl.enable_nodata(1);
				if (err) {
					err = cxt->pedo_ctl.enable_nodata(1);
					if (err)
						PEDO_ERR("pedo enable(%d) err 3 timers = %d\n",
							 enable, err);
				}
			}
			PEDO_LOG("pedo real enable\n");
		}
	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->pedo_ctl.enable_nodata(0);
			if (err)
				PEDO_ERR("pedo enable(%d) err = %d\n", enable, err);
			else
				PEDO_LOG("pedo real disable\n");
		}
	}
	return err;
}

static int pedo_enable_data(int enable)
{
	struct pedo_context *cxt = NULL;

	cxt = pedo_context_obj;
	if (NULL == cxt->pedo_ctl.open_report_data) {
		PEDO_ERR("no pedo control path\n");
		return -1;
	}
	if (1 == enable) {
		PEDO_LOG("pedo enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->pedo_ctl.open_report_data(1);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->pedo_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		PEDO_LOG("pedo disable\n");

		cxt->is_active_data = false;
		cxt->pedo_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->pedo_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.pedo_data.values[0] = PEDO_INVALID_VALUE;
				cxt->drv_data.pedo_data.values[1] = PEDO_INVALID_VALUE;
				cxt->drv_data.pedo_data.values[2] = PEDO_INVALID_VALUE;
				cxt->drv_data.pedo_data.values[3] = PEDO_INVALID_VALUE;
			}
		}

	}
	pedo_real_enable(enable);
	return 0;
}



int pedo_enable_nodata(int enable)
{
	struct pedo_context *cxt = NULL;

	cxt = pedo_context_obj;
	if (NULL == cxt->pedo_ctl.enable_nodata) {
		PEDO_ERR("pedo_enable_nodata:pedo ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;
	pedo_real_enable(enable);
	return 0;
}


static ssize_t pedo_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PEDO_LOG(" not support now\n");
	return len;
}

static ssize_t pedo_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct pedo_context *cxt = NULL;

	PEDO_LOG("pedo_store_enable nodata buf=%s\n", buf);
	mutex_lock(&pedo_context_obj->pedo_op_mutex);
	cxt = pedo_context_obj;
	if (NULL == cxt->pedo_ctl.enable_nodata) {
		PEDO_LOG("pedo_ctl enable nodata NULL\n");
		mutex_unlock(&pedo_context_obj->pedo_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		pedo_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		pedo_enable_nodata(0);
	else
		PEDO_ERR(" pedo_store enable nodata cmd error !!\n");
	mutex_unlock(&pedo_context_obj->pedo_op_mutex);
	return count;
}

static ssize_t pedo_store_active(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct pedo_context *cxt = NULL;

	PEDO_LOG("pedo_store_active buf=%s\n", buf);
	mutex_lock(&pedo_context_obj->pedo_op_mutex);
	cxt = pedo_context_obj;
	if (NULL == cxt->pedo_ctl.open_report_data) {
		PEDO_LOG("pedo_ctl enable NULL\n");
		mutex_unlock(&pedo_context_obj->pedo_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		pedo_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		pedo_enable_data(0);
	else
		PEDO_ERR(" pedo_store_active error !!\n");
	mutex_unlock(&pedo_context_obj->pedo_op_mutex);
	PEDO_LOG(" pedo_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t pedo_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pedo_context *cxt = NULL;
	int div = 0;

	cxt = pedo_context_obj;
	PEDO_LOG("pedo show active not support now\n");
	PEDO_LOG("pedo vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t pedo_store_delay(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int delay;
	int ret;
	int mdelay = 0;
	struct pedo_context *cxt = NULL;

	mutex_lock(&pedo_context_obj->pedo_op_mutex);
	cxt = pedo_context_obj;
	if (NULL == cxt->pedo_ctl.set_delay) {
		PEDO_LOG("pedo_ctl set_delay NULL\n");
		mutex_unlock(&pedo_context_obj->pedo_op_mutex);
		return count;
	}

	ret = kstrtoint(buf, 10, &delay);
	if (0 != ret) {
		PEDO_ERR("invalid format!!\n");
		mutex_unlock(&pedo_context_obj->pedo_op_mutex);
		return count;
	}

	if (false == cxt->pedo_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&pedo_context_obj->delay, mdelay);
	}
	cxt->pedo_ctl.set_delay(delay);
	PEDO_LOG(" pedo_delay %d ns\n", delay);
	mutex_unlock(&pedo_context_obj->pedo_op_mutex);
	return count;
}

static ssize_t pedo_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PEDO_LOG(" not support now\n");
	return len;
}

static ssize_t pedo_store_batch(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pedo_context *cxt = NULL;

	PEDO_LOG("pedo_store_batch buf=%s\n", buf);
	mutex_lock(&pedo_context_obj->pedo_op_mutex);
	cxt = pedo_context_obj;
	if (cxt->pedo_ctl.is_support_batch) {

		if (!strncmp(buf, "1", 1))
			cxt->is_batch_enable = true;
		else if (!strncmp(buf, "0", 1))
			cxt->is_batch_enable = false;
		else
			PEDO_ERR(" pedo_store_batch error !!\n");
	} else
		PEDO_LOG(" pedo_store_batch not support\n");
	mutex_unlock(&pedo_context_obj->pedo_op_mutex);
	PEDO_LOG(" pedo_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t pedo_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t pedo_store_flush(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{

	return count;
}

static ssize_t pedo_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t pedo_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &pedo_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int pedometer_remove(struct platform_device *pdev)
{
	PEDO_LOG("pedometer_remove\n");
	return 0;
}

static int pedometer_probe(struct platform_device *pdev)
{
	PEDO_LOG("pedometer_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pedometer_of_match[] = {
	{.compatible = "mediatek,pedometer",},
	{},
};
#endif

static struct platform_driver pedometer_driver = {
	.probe = pedometer_probe,
	.remove = pedometer_remove,
	.driver = {

		   .name = "pedometer",
#ifdef CONFIG_OF
		   .of_match_table = pedometer_of_match,
#endif
		   }
};

static int pedo_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	PEDO_LOG(" pedo_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_PEDO_NUM; i++) {
		PEDO_LOG(" i=%d\n", i);
		if (0 != pedometer_init_list[i]) {
			PEDO_LOG(" pedo try to init driver %s\n", pedometer_init_list[i]->name);
			err = pedometer_init_list[i]->init();
			if (0 == err) {
				PEDO_LOG(" pedo real driver %s probe ok\n",
					 pedometer_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_PEDO_NUM) {
		PEDO_LOG(" pedo_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

int pedo_driver_add(struct pedo_init_info *obj)
{
	int err = 0;
	int i = 0;

	PEDO_FUN(f);

	for (i = 0; i < MAX_CHOOSE_PEDO_NUM; i++) {
		if (i == 0) {
			PEDO_LOG("register pedo driver for the first time\n");
			if (platform_driver_register(&pedometer_driver))
				PEDO_ERR("failed to register pedo driver already exist\n");
		}

		if (NULL == pedometer_init_list[i]) {
			obj->platform_diver_addr = &pedometer_driver;
			pedometer_init_list[i] = obj;
			break;
		}
	}
	if (NULL == pedometer_init_list[i]) {
		PEDO_ERR("pedo driver add err\n");
		err = -1;
	}

	return err;
}

static int pedo_misc_init(struct pedo_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = PEDO_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		PEDO_ERR("unable to register pedo misc device!!\n");
	return err;
}

static void pedo_input_destroy(struct pedo_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int pedo_input_init(struct pedo_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = PEDO_INPUTDEV_NAME;

	input_set_capability(dev, EV_REL, EVENT_TYPE_PEDO_LENGTH);
	input_set_capability(dev, EV_REL, EVENT_TYPE_PEDO_FREQUENCY);
	input_set_capability(dev, EV_REL, EVENT_TYPE_PEDO_COUNT);
	input_set_capability(dev, EV_REL, EVENT_TYPE_PEDO_DISTANCE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_PEDO_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_PEDO_TIMESTAMP_HI);
	input_set_capability(dev, EV_REL, EVENT_TYPE_PEDO_TIMESTAMP_LO);
	/* input_set_abs_params(dev, EVENT_TYPE_PEDO_LENGTH, PEDO_VALUE_MIN, PEDO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_PEDO_FREQUENCY, PEDO_VALUE_MIN, PEDO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_PEDO_COUNT, PEDO_VALUE_MIN, PEDO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_PEDO_DISTANCE, PEDO_VALUE_MIN, PEDO_VALUE_MAX, 0, 0); */
	input_set_abs_params(dev, EVENT_TYPE_PEDO_STATUS, PEDO_STATUS_MIN, PEDO_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(pedoenablenodata,     S_IWUSR | S_IRUGO, pedo_show_enable_nodata, pedo_store_enable_nodata);
DEVICE_ATTR(pedoactive,     S_IWUSR | S_IRUGO, pedo_show_active, pedo_store_active);
DEVICE_ATTR(pedodelay,      S_IWUSR | S_IRUGO, pedo_show_delay,  pedo_store_delay);
DEVICE_ATTR(pedobatch,     S_IWUSR | S_IRUGO, pedo_show_batch, pedo_store_batch);
DEVICE_ATTR(pedoflush,      S_IWUSR | S_IRUGO, pedo_show_flush,  pedo_store_flush);
DEVICE_ATTR(pedodevnum,      S_IWUSR | S_IRUGO, pedo_show_devnum,  NULL);

static struct attribute *pedo_attributes[] = {
	&dev_attr_pedoenablenodata.attr,
	&dev_attr_pedoactive.attr,
	&dev_attr_pedodelay.attr,
	&dev_attr_pedobatch.attr,
	&dev_attr_pedoflush.attr,
	&dev_attr_pedodevnum.attr,
	NULL
};

static struct attribute_group pedo_attribute_group = {
	.attrs = pedo_attributes
};

int pedo_register_data_path(struct pedo_data_path *data)
{
	struct pedo_context *cxt = NULL;

	cxt = pedo_context_obj;
	cxt->pedo_data.get_data = data->get_data;

	if (NULL == cxt->pedo_data.get_data) {
		PEDO_LOG("pedo register data path fail\n");
		return -1;
	}
	return 0;
}

int pedo_register_control_path(struct pedo_control_path *ctl)
{
	struct pedo_context *cxt = NULL;
	int err = 0;

	cxt = pedo_context_obj;
	cxt->pedo_ctl.set_delay = ctl->set_delay;
	cxt->pedo_ctl.open_report_data = ctl->open_report_data;
	cxt->pedo_ctl.enable_nodata = ctl->enable_nodata;
	cxt->pedo_ctl.is_support_batch = ctl->is_support_batch;

	if (NULL == cxt->pedo_ctl.set_delay || NULL == cxt->pedo_ctl.open_report_data
	    || NULL == cxt->pedo_ctl.enable_nodata) {
		PEDO_LOG("pedo register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = pedo_misc_init(pedo_context_obj);
	if (err) {
		PEDO_ERR("unable to register pedo misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&pedo_context_obj->mdev.this_device->kobj, &pedo_attribute_group);
	if (err < 0) {
		PEDO_ERR("unable to create pedo attribute file\n");
		return -3;
	}

	kobject_uevent(&pedo_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int pedo_data_report(struct hwm_sensor_data *data, int status)
{
	struct pedo_context *cxt = NULL;
	int err = 0;

	/*PEDO_LOG("pedo_data_report! %d, %d, %d, %d\n", data->values[0],
		 data->values[1], data->values[2], data->values[3]);*/
	cxt = pedo_context_obj;
	input_report_rel(cxt->idev, EVENT_TYPE_PEDO_COUNT, data->values[0]);
	input_report_rel(cxt->idev, EVENT_TYPE_PEDO_LENGTH, data->values[1]);
	input_report_rel(cxt->idev, EVENT_TYPE_PEDO_FREQUENCY, data->values[2]);
	input_report_rel(cxt->idev, EVENT_TYPE_PEDO_DISTANCE, data->values[3]);
	input_report_abs(cxt->idev, EVENT_TYPE_PEDO_STATUS, status);
	input_report_rel(cxt->idev, EVENT_TYPE_PEDO_TIMESTAMP_HI, data->time >> 32);
	input_report_rel(cxt->idev, EVENT_TYPE_PEDO_TIMESTAMP_LO, data->time & 0xFFFFFFFFLL);
	input_sync(cxt->idev);
	return err;
}

static int pedo_probe(void)
{

	int err;

	PEDO_LOG("+++++++++++++pedo_probe!!\n");

	pedo_context_obj = pedo_context_alloc_object();
	if (!pedo_context_obj) {
		err = -ENOMEM;
		PEDO_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	err = pedo_real_driver_init();
	if (err) {
		PEDO_ERR("pedo real driver init fail\n");
		goto real_driver_init_fail;
	}

	err = pedo_input_init(pedo_context_obj);
	if (err) {
		PEDO_ERR("unable to register pedo input device!\n");
		goto exit_alloc_input_dev_failed;
	}
	PEDO_LOG("----pedo_probe OK !!\n");
	return 0;
exit_alloc_input_dev_failed:
	pedo_input_destroy(pedo_context_obj);
real_driver_init_fail:
	kfree(pedo_context_obj);
exit_alloc_data_failed:
	PEDO_LOG("----pedo_probe fail !!!\n");
	return err;
}

static int pedo_remove(void)
{
	int err = 0;

	PEDO_FUN(f);
	input_unregister_device(pedo_context_obj->idev);
	sysfs_remove_group(&pedo_context_obj->idev->dev.kobj, &pedo_attribute_group);

	err = misc_deregister(&pedo_context_obj->mdev);
	if (err)
		PEDO_ERR("misc_deregister fail: %d\n", err);
	kfree(pedo_context_obj);
	return 0;
}

static int __init pedo_init(void)
{
	PEDO_FUN(f);

	if (pedo_probe()) {
		PEDO_ERR("failed to register pedo driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit pedo_exit(void)
{
	pedo_remove();
	platform_driver_unregister(&pedometer_driver);
}

late_initcall(pedo_init);
/* module_init(pedo_init); */
/* module_exit(pedo_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PEDOMETER device driver");
MODULE_AUTHOR("Mediatek");

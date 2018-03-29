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

#include "pick_up.h"

static struct pkup_context *pkup_context_obj;

static struct pkup_init_info *pick_up_init = { 0 };	/* modified */



static int resume_enable_status;

static struct pkup_context *pkup_context_alloc_object(void)
{
	struct pkup_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	PKUP_LOG("pkup_context_alloc_object++++\n");
	if (!obj) {
		PKUP_ERR("Alloc pkup object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->pkup_op_mutex);

	PKUP_LOG("pkup_context_alloc_object----\n");
	return obj;
}

int pkup_notify(void)
{
	int err = 0;
	int value = 0;
	struct pkup_context *cxt = NULL;

	cxt = pkup_context_obj;
	PKUP_LOG("pkup_notify++++\n");

	value = 1;
	input_report_rel(cxt->idev, EVENT_TYPE_PKUP_VALUE, value);
	input_sync(cxt->idev);

	return err;
}

static int pkup_real_enable(int enable)
{
	int err = 0;
	struct pkup_context *cxt = NULL;

	cxt = pkup_context_obj;

	if (PKUP_RESUME == enable)
		enable = resume_enable_status;


	if (1 == enable) {
		resume_enable_status = 1;
		if (atomic_read(&(pkup_context_obj->early_suspend)))	/* not allow to enable under suspend */
			return 0;

		if (false == cxt->is_active_data) {
			err = cxt->pkup_ctl.open_report_data(1);
			if (err) {
				err = cxt->pkup_ctl.open_report_data(1);
				if (err) {
					err = cxt->pkup_ctl.open_report_data(1);
					if (err) {
						PKUP_ERR
						    ("enable_pick_up enable(%d) err 3 timers = %d\n",
						     enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			PKUP_LOG("enable_pick_up real enable\n");
		}
	} else if ((0 == enable) || (PKUP_SUSPEND == enable)) {
		if (0 == enable)
			resume_enable_status = 0;
		if (true == cxt->is_active_data) {
			err = cxt->pkup_ctl.open_report_data(0);
			if (err)
				PKUP_ERR("enable_pick_upenable(%d) err = %d\n", enable, err);

			cxt->is_active_data = false;
			PKUP_LOG("enable_pick_up real disable\n");
		}
	}
	return err;
}

int pkup_enable_nodata(int enable)
{
	struct pkup_context *cxt = NULL;

	cxt = pkup_context_obj;
	if (NULL == cxt->pkup_ctl.open_report_data) {
		PKUP_ERR("pkup_enable_nodata:pkup ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	pkup_real_enable(enable);
	return 0;
}

static ssize_t pkup_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pkup_context *cxt = NULL;

	cxt = pkup_context_obj;
	PKUP_LOG("pkup active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata);
}

static ssize_t pkup_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct pkup_context *cxt = NULL;

	PKUP_LOG("pkup_store_enable nodata buf=%s\n", buf);
	mutex_lock(&pkup_context_obj->pkup_op_mutex);
	cxt = pkup_context_obj;
	if (NULL == cxt->pkup_ctl.open_report_data) {
		PKUP_LOG("pkup_ctl enable nodata NULL\n");
		mutex_unlock(&pkup_context_obj->pkup_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		pkup_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		pkup_enable_nodata(0);
	else
		PKUP_ERR(" pkup_store enable nodata cmd error !!\n");

	mutex_unlock(&pkup_context_obj->pkup_op_mutex);
	return count;
}

static ssize_t pkup_store_active(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct pkup_context *cxt = NULL;
	int res = 0;
	int en = 0;

	PKUP_LOG("pkup_store_active buf=%s\n", buf);
	mutex_lock(&pkup_context_obj->pkup_op_mutex);

	cxt = pkup_context_obj;

	res = kstrtoint(buf, 10, &en);
	if (res != 0)
		PKUP_LOG(" pkup_store_active param error: res = %d\n", res);

	PKUP_LOG(" pkup_store_active en=%d\n", en);
	if (1 == en)
		pkup_real_enable(1);
	else if (0 == en)
		pkup_real_enable(0);
	else
		PKUP_ERR(" pkup_store_active error !!\n");

	mutex_unlock(&pkup_context_obj->pkup_op_mutex);
	PKUP_LOG(" pkup_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t pkup_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pkup_context *cxt = NULL;

	cxt = pkup_context_obj;
	PKUP_LOG("pkup active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data);
}

static ssize_t pkup_store_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;

	PKUP_LOG(" not support now\n");
	return len;
}


static ssize_t pkup_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PKUP_LOG(" not support now\n");
	return len;
}


static ssize_t pkup_store_batch(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int len = 0;

	PKUP_LOG(" not support now\n");
	return len;
}

static ssize_t pkup_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PKUP_LOG(" not support now\n");
	return len;
}

static ssize_t pkup_store_flush(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int len = 0;

	PKUP_LOG(" not support now\n");
	return len;
}

static ssize_t pkup_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PKUP_LOG(" not support now\n");
	return len;
}

static ssize_t pkup_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &pkup_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}

	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int pick_up_remove(struct platform_device *pdev)
{
	PKUP_LOG("pick_up_remove\n");
	return 0;
}

static int pick_up_probe(struct platform_device *pdev)
{
	PKUP_LOG("pick_up_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pick_up_of_match[] = {
	{.compatible = "mediatek,pick_up",},
	{},
};
#endif

static struct platform_driver pick_up_driver = {
	.probe = pick_up_probe,
	.remove = pick_up_remove,
	.driver = {
		   .name = "pick_up",
#ifdef CONFIG_OF
		   .of_match_table = pick_up_of_match,
#endif
		   }
};

static int pkup_real_driver_init(void)
{
	int err = 0;

	PKUP_LOG(" pkup_real_driver_init +\n");
	if (0 != pick_up_init) {
		PKUP_LOG(" pkup try to init driver %s\n", pick_up_init->name);
		err = pick_up_init->init();
		if (0 == err)
			PKUP_LOG(" pkup real driver %s probe ok\n", pick_up_init->name);
	}
	return err;
}

int pkup_driver_add(struct pkup_init_info *obj)
{
	int err = 0;

	PKUP_FUN();
	PKUP_LOG("register pick_up driver for the first time\n");
	if (platform_driver_register(&pick_up_driver))
		PKUP_ERR("failed to register gensor driver already exist\n");

	if (NULL == pick_up_init) {
		obj->platform_diver_addr = &pick_up_driver;
		pick_up_init = obj;
	}

	if (NULL == pick_up_init) {
		PKUP_ERR("PKUP driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(pkup_driver_add);

static int pkup_misc_init(struct pkup_context *cxt)
{
	int err = 0;

	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = PKUP_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		PKUP_ERR("unable to register pkup misc device!!\n");

	return err;
}

static void pkup_input_destroy(struct pkup_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int pkup_input_init(struct pkup_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = PKUP_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_PKUP_VALUE);

	input_set_drvdata(dev, cxt);
	set_bit(EV_REL, dev->evbit);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(pkupenablenodata, S_IWUSR | S_IRUGO, pkup_show_enable_nodata, pkup_store_enable_nodata);
DEVICE_ATTR(pkupactive, S_IWUSR | S_IRUGO, pkup_show_active, pkup_store_active);
DEVICE_ATTR(pkupdelay, S_IWUSR | S_IRUGO, pkup_show_delay, pkup_store_delay);
DEVICE_ATTR(pkupbatch, S_IWUSR | S_IRUGO, pkup_show_batch, pkup_store_batch);
DEVICE_ATTR(pkupflush, S_IWUSR | S_IRUGO, pkup_show_flush, pkup_store_flush);
DEVICE_ATTR(pkupdevnum, S_IWUSR | S_IRUGO, pkup_show_devnum, NULL);


static struct attribute *pkup_attributes[] = {
	&dev_attr_pkupenablenodata.attr,
	&dev_attr_pkupactive.attr,
	&dev_attr_pkupdelay.attr,
	&dev_attr_pkupbatch.attr,
	&dev_attr_pkupflush.attr,
	&dev_attr_pkupdevnum.attr,
	NULL
};

static struct attribute_group pkup_attribute_group = {
	.attrs = pkup_attributes
};

int pkup_register_data_path(struct pkup_data_path *data)
{
	struct pkup_context *cxt = NULL;

	cxt = pkup_context_obj;
	cxt->pkup_data.get_data = data->get_data;
	if (NULL == cxt->pkup_data.get_data) {
		PKUP_LOG("pkup register data path fail\n");
		return -1;
	}
	return 0;
}

int pkup_register_control_path(struct pkup_control_path *ctl)
{
	struct pkup_context *cxt = NULL;
	int err = 0;

	cxt = pkup_context_obj;
/* cxt->pkup_ctl.enable = ctl->enable; */
/* cxt->pkup_ctl.enable_nodata = ctl->enable_nodata; */
	cxt->pkup_ctl.open_report_data = ctl->open_report_data;

	if (NULL == cxt->pkup_ctl.open_report_data) {
		PKUP_LOG("pkup register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = pkup_misc_init(pkup_context_obj);
	if (err) {
		PKUP_ERR("unable to register pkup misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&pkup_context_obj->mdev.this_device->kobj, &pkup_attribute_group);
	if (err < 0) {
		PKUP_ERR("unable to create pkup attribute file\n");
		return -3;
	}
	kobject_uevent(&pkup_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}

static int pkup_probe(void)
{
	int err;

	PKUP_LOG("+++++++++++++pkup_probe!!\n");

	pkup_context_obj = pkup_context_alloc_object();
	if (!pkup_context_obj) {
		err = -ENOMEM;
		PKUP_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real pkup driver */
	err = pkup_real_driver_init();
	if (err) {
		PKUP_ERR("pkup real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = pkup_input_init(pkup_context_obj);
	if (err) {
		PKUP_ERR("unable to register pkup input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	PKUP_LOG("----pkup_probe OK !!\n");
	return 0;


	if (err) {
		PKUP_ERR("sysfs node creation error\n");
		pkup_input_destroy(pkup_context_obj);
	}
real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(pkup_context_obj);
exit_alloc_data_failed:
	PKUP_LOG("----pkup_probe fail !!!\n");
	return err;
}

static int pkup_remove(void)
{
	int err = 0;

	PKUP_FUN(f);
	input_unregister_device(pkup_context_obj->idev);
	sysfs_remove_group(&pkup_context_obj->idev->dev.kobj, &pkup_attribute_group);

	err = misc_deregister(&pkup_context_obj->mdev);
	if (err)
		PKUP_ERR("misc_deregister fail: %d\n", err);

	kfree(pkup_context_obj);
	return 0;
}
static int __init pkup_init(void)
{
	PKUP_FUN();

	if (pkup_probe()) {
		PKUP_ERR("failed to register pkup driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit pkup_exit(void)
{
	pkup_remove();
	platform_driver_unregister(&pick_up_driver);
}

late_initcall(pkup_init);
/* module_init(pkup_init); */
/* module_exit(pkup_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PKUP device driver");
MODULE_AUTHOR("Mediatek");

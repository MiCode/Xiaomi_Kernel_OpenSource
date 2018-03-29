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

#include "answer_call.h"

static struct ancall_context *ancall_context_obj;

static struct ancall_init_info *answer_call_init = { 0 };	/* modified */



static int resume_enable_status;
static struct wake_lock ancall_lock;
static void notify_ancall_timeout(unsigned long);

static struct ancall_context *ancall_context_alloc_object(void)
{
	struct ancall_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	ANCALL_LOG("ancall_context_alloc_object++++\n");
	if (!obj) {
		ANCALL_ERR("Alloc ancall object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->ancall_op_mutex);

	ANCALL_LOG("ancall_context_alloc_object----\n");
	return obj;
}

static void notify_ancall_timeout(unsigned long data)
{
	wake_unlock(&ancall_lock);
}

int ancall_notify(void)
{
	int err = 0;
	int value = 0;
	struct ancall_context *cxt = NULL;

	cxt = ancall_context_obj;
	ANCALL_LOG("ancall_notify++++\n");
	pr_warn("ancall_notify\n");
	value = 1;
	input_report_rel(cxt->idev, EVENT_TYPE_ANSWER_CALL_VALUE, value);
	input_sync(cxt->idev);

	wake_lock(&ancall_lock);
	mod_timer(&cxt->notify_timer, jiffies + HZ / 5);

	return err;
}

static int ancall_real_enable(int enable)
{
	int err = 0;
	struct ancall_context *cxt = NULL;

	cxt = ancall_context_obj;

	if (ANSWER_CALL_RESUME == enable)
		enable = resume_enable_status;

	pr_warn("%s : enable=%d\n", __func__, enable);
	if (1 == enable) {
		resume_enable_status = 1;
		pr_warn("%s : suspend=%d\n", __func__, atomic_read(&(ancall_context_obj->early_suspend)));
		if (atomic_read(&(ancall_context_obj->early_suspend)))	/* not allow to enable under suspend */
			return 0;


		if (false == cxt->is_active_data) {
			err = cxt->ancall_ctl.open_report_data(1);
			if (err) {
				err = cxt->ancall_ctl.open_report_data(1);
				if (err) {
					err = cxt->ancall_ctl.open_report_data(1);
					if (err) {
						ANCALL_ERR
						    ("enable_answer_call enable(%d) err 3 timers = %d\n",
						     enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			ANCALL_LOG("enable_answer_call real enable\n");
		}
	} else if ((0 == enable) || (ANSWER_CALL_SUSPEND == enable)) {
		if (0 == enable)
			resume_enable_status = 0;
		if (true == cxt->is_active_data) {
			err = cxt->ancall_ctl.open_report_data(0);
			if (err)
				ANCALL_ERR("enable_answer_callenable(%d) err = %d\n", enable, err);

			cxt->is_active_data = false;
			ANCALL_LOG("enable_answer_call real disable\n");
		}
	}
	return err;
}

int ancall_enable_nodata(int enable)
{
	struct ancall_context *cxt = NULL;

	cxt = ancall_context_obj;
	if (NULL == cxt->ancall_ctl.open_report_data) {
		ANCALL_ERR("ancall_enable_nodata:ancall ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	ancall_real_enable(enable);
	return 0;
}

static ssize_t ancall_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ancall_context *cxt = NULL;

	cxt = ancall_context_obj;
	ANCALL_LOG("ancall active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata);
}

static ssize_t ancall_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct ancall_context *cxt = NULL;

	ANCALL_LOG("ancall_store_enable nodata buf=%s\n", buf);
	mutex_lock(&ancall_context_obj->ancall_op_mutex);
	cxt = ancall_context_obj;
	if (NULL == cxt->ancall_ctl.open_report_data) {
		ANCALL_LOG("ancall_ctl enable nodata NULL\n");
		mutex_unlock(&ancall_context_obj->ancall_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		ancall_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		ancall_enable_nodata(0);
	else
		ANCALL_ERR(" ancall_store enable nodata cmd error !!\n");

	mutex_unlock(&ancall_context_obj->ancall_op_mutex);
	return count;
}

static ssize_t ancall_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ancall_context *cxt = NULL;
	int res = 0;
	int en = 0;

	ANCALL_LOG("ancall_store_active buf=%s\n", buf);
	mutex_lock(&ancall_context_obj->ancall_op_mutex);

	cxt = ancall_context_obj;

	res = kstrtoint(buf, 10, &en);
	if (res != 0)
		ANCALL_LOG(" ancall_store_active param error: res = %d\n", res);

	ANCALL_LOG(" ancall_store_active en=%d\n", en);
	if (1 == en)
		ancall_real_enable(1);
	else if (0 == en)
		ancall_real_enable(0);
	else
		ANCALL_ERR(" ancall_store_active error !!\n");

	mutex_unlock(&ancall_context_obj->ancall_op_mutex);
	ANCALL_LOG(" ancall_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ancall_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ancall_context *cxt = NULL;

	cxt = ancall_context_obj;
	ANCALL_LOG("ancall active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data);
}

static ssize_t ancall_store_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;

	ANCALL_LOG(" not support now\n");
	return len;
}


static ssize_t ancall_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ANCALL_LOG(" not support now\n");
	return len;
}


static ssize_t ancall_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	ANCALL_LOG(" not support now\n");
	return len;
}

static ssize_t ancall_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ANCALL_LOG(" not support now\n");
	return len;
}

static ssize_t ancall_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	ANCALL_LOG(" not support now\n");
	return len;
}

static ssize_t ancall_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	ANCALL_LOG(" not support now\n");
	return len;
}

static ssize_t ancall_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &ancall_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}

	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int answer_call_remove(struct platform_device *pdev)
{
	ANCALL_LOG("answer_call_remove\n");
	return 0;
}

static int answer_call_probe(struct platform_device *pdev)
{
	ANCALL_LOG("answer_call_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id answer_call_of_match[] = {
	{.compatible = "mediatek,answer_call",},
	{},
};
#endif

static struct platform_driver answer_call_driver = {
	.probe = answer_call_probe,
	.remove = answer_call_remove,
	.driver = {
		   .name = "answer_call",
#ifdef CONFIG_OF
		   .of_match_table = answer_call_of_match,
#endif
		   }
};

static int ancall_real_driver_init(void)
{
	int err = 0;

	ANCALL_LOG(" ancall_real_driver_init +\n");
	if (0 != answer_call_init) {
		ANCALL_LOG(" ancall try to init driver %s\n", answer_call_init->name);
		err = answer_call_init->init();
		if (0 == err)
			ANCALL_LOG(" ancall real driver %s probe ok\n", answer_call_init->name);
	}
	wake_lock_init(&ancall_lock, WAKE_LOCK_SUSPEND, "ancall wakelock");
	init_timer(&ancall_context_obj->notify_timer);
	ancall_context_obj->notify_timer.expires = HZ / 5;	/* 200 ms */
	ancall_context_obj->notify_timer.function = notify_ancall_timeout;
	ancall_context_obj->notify_timer.data = (unsigned long)ancall_context_obj;

	return err;
}

int ancall_driver_add(struct ancall_init_info *obj)
{
	int err = 0;

	ANCALL_FUN();
	ANCALL_LOG("register answer_call driver for the first time\n");
	if (platform_driver_register(&answer_call_driver))
		ANCALL_ERR("failed to register gensor driver already exist\n");

	if (NULL == answer_call_init) {
		obj->platform_diver_addr = &answer_call_driver;
		answer_call_init = obj;
	}

	if (NULL == answer_call_init) {
		ANCALL_ERR("ANCALL driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(ancall_driver_add);

static int ancall_misc_init(struct ancall_context *cxt)
{
	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = ANSWERCALL_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		ANCALL_ERR("unable to register ancall misc device!!\n");

	return err;
}

static void ancall_input_destroy(struct ancall_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int ancall_input_init(struct ancall_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = ANSWERCALL_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_ANSWER_CALL_VALUE);

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

DEVICE_ATTR(ancallenablenodata, S_IWUSR | S_IRUGO, ancall_show_enable_nodata, ancall_store_enable_nodata);
DEVICE_ATTR(ancallactive, S_IWUSR | S_IRUGO, ancall_show_active, ancall_store_active);
DEVICE_ATTR(ancalldelay, S_IWUSR | S_IRUGO, ancall_show_delay, ancall_store_delay);
DEVICE_ATTR(ancallbatch, S_IWUSR | S_IRUGO, ancall_show_batch, ancall_store_batch);
DEVICE_ATTR(ancallflush, S_IWUSR | S_IRUGO, ancall_show_flush, ancall_store_flush);
DEVICE_ATTR(ancalldevnum, S_IWUSR | S_IRUGO, ancall_show_devnum, NULL);


static struct attribute *ancall_attributes[] = {
	&dev_attr_ancallenablenodata.attr,
	&dev_attr_ancallactive.attr,
	&dev_attr_ancalldelay.attr,
	&dev_attr_ancallbatch.attr,
	&dev_attr_ancallflush.attr,
	&dev_attr_ancalldevnum.attr,
	NULL
};

static struct attribute_group ancall_attribute_group = {
	.attrs = ancall_attributes
};

int ancall_register_data_path(struct ancall_data_path *data)
{
	struct ancall_context *cxt = NULL;

	cxt = ancall_context_obj;
	cxt->ancall_data.get_data = data->get_data;
	if (NULL == cxt->ancall_data.get_data) {
		ANCALL_LOG("ancall register data path fail\n");
		return -1;
	}
	return 0;
}

int ancall_register_control_path(struct ancall_control_path *ctl)
{
	struct ancall_context *cxt = NULL;
	int err = 0;

	cxt = ancall_context_obj;
/* cxt->ancall_ctl.enable = ctl->enable; */
/* cxt->ancall_ctl.enable_nodata = ctl->enable_nodata; */
	cxt->ancall_ctl.open_report_data = ctl->open_report_data;

	if (NULL == cxt->ancall_ctl.open_report_data) {
		ANCALL_LOG("ancall register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = ancall_misc_init(ancall_context_obj);
	if (err) {
		ANCALL_ERR("unable to register ancall misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&ancall_context_obj->mdev.this_device->kobj, &ancall_attribute_group);
	if (err < 0) {
		ANCALL_ERR("unable to create ancall attribute file\n");
		return -3;
	}
	kobject_uevent(&ancall_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}

static int ancall_probe(void)
{
	int err;

	ANCALL_LOG("+++++++++++++ancall_probe!!\n");

	ancall_context_obj = ancall_context_alloc_object();
	if (!ancall_context_obj) {
		err = -ENOMEM;
		ANCALL_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real ancall driver */
	err = ancall_real_driver_init();
	if (err) {
		ANCALL_ERR("ancall real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = ancall_input_init(ancall_context_obj);
	if (err) {
		ANCALL_ERR("unable to register ancall input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	ANCALL_LOG("----ancall_probe OK !!\n");
	return 0;

	if (err) {
		ANCALL_ERR("sysfs node creation error\n");
		ancall_input_destroy(ancall_context_obj);
	}
real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(ancall_context_obj);
exit_alloc_data_failed:
	ANCALL_LOG("----ancall_probe fail !!!\n");
	return err;
}

static int ancall_remove(void)
{
	int err = 0;

	ANCALL_FUN();
	input_unregister_device(ancall_context_obj->idev);
	sysfs_remove_group(&ancall_context_obj->idev->dev.kobj, &ancall_attribute_group);

	err = misc_deregister(&ancall_context_obj->mdev);
	if (err)
		ANCALL_ERR("misc_deregister fail: %d\n", err);

	kfree(ancall_context_obj);
	return 0;
}
static int __init ancall_init(void)
{
	ANCALL_FUN();

	if (ancall_probe()) {
		ANCALL_ERR("failed to register ancall driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit ancall_exit(void)
{
	ancall_remove();
	platform_driver_unregister(&answer_call_driver);
}

late_initcall(ancall_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ANCALL device driver");
MODULE_AUTHOR("Mediatek");

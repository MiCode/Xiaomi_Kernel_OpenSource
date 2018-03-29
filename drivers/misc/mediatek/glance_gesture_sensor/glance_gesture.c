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

#include "glance_gesture.h"

static struct glg_context *glg_context_obj;

static struct glg_init_info *glance_gesture_init = { 0 };	/* modified */



static int resume_enable_status;
static struct wake_lock glg_lock;
static void notify_glg_timeout(unsigned long);

static struct glg_context *glg_context_alloc_object(void)
{
	struct glg_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	GLG_LOG("glg_context_alloc_object++++\n");
	if (!obj) {
		GLG_ERR("Alloc glg object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->glg_op_mutex);

	GLG_LOG("glg_context_alloc_object----\n");
	return obj;
}

static void notify_glg_timeout(unsigned long data)
{
	wake_unlock(&glg_lock);
}

int glg_notify(void)
{
	int err = 0;
	int value = 0;
	struct glg_context *cxt = NULL;

	cxt = glg_context_obj;
	GLG_LOG("glg_notify++++\n");

	value = 1;
	input_report_rel(cxt->idev, EVENT_TYPE_GLG_VALUE, value);
	input_sync(cxt->idev);

	wake_lock(&glg_lock);
	mod_timer(&cxt->notify_timer, jiffies + HZ / 5);

	return err;
}

static int glg_real_enable(int enable)
{
	int err = 0;
	struct glg_context *cxt = NULL;

	cxt = glg_context_obj;

	if (GLG_RESUME == enable)
		enable = resume_enable_status;

	if (1 == enable) {
		resume_enable_status = 1;
		if (atomic_read(&(glg_context_obj->early_suspend)))	/* not allow to enable under suspend */
			return 0;

		if (false == cxt->is_active_data) {
			err = cxt->glg_ctl.open_report_data(1);
			if (err) {
				err = cxt->glg_ctl.open_report_data(1);
				if (err) {
					err = cxt->glg_ctl.open_report_data(1);
					if (err) {
						GLG_ERR
						    ("enable_glance_gesture enable(%d) err 3 timers = %d\n",
						     enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			GLG_LOG("enable_glance_gesture real enable\n");
		}
	} else if ((0 == enable) || (GLG_SUSPEND == enable)) {
		if (0 == enable)
			resume_enable_status = 0;
		if (true == cxt->is_active_data) {
			err = cxt->glg_ctl.open_report_data(0);
			if (err)
				GLG_ERR("enable_glance_gestureenable(%d) err = %d\n", enable, err);

			cxt->is_active_data = false;
			GLG_LOG("enable_glance_gesture real disable\n");
		}
	}
	return err;
}

int glg_enable_nodata(int enable)
{
	struct glg_context *cxt = NULL;

	cxt = glg_context_obj;
	if (NULL == cxt->glg_ctl.open_report_data) {
		GLG_ERR("glg_enable_nodata:glg ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	glg_real_enable(enable);
	return 0;
}

static ssize_t glg_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct glg_context *cxt = NULL;

	cxt = glg_context_obj;
	GLG_LOG("glg active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata);
}

static ssize_t glg_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct glg_context *cxt = NULL;

	GLG_LOG("glg_store_enable nodata buf=%s\n", buf);
	mutex_lock(&glg_context_obj->glg_op_mutex);
	cxt = glg_context_obj;
	if (NULL == cxt->glg_ctl.open_report_data) {
		GLG_LOG("glg_ctl enable nodata NULL\n");
		mutex_unlock(&glg_context_obj->glg_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		glg_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		glg_enable_nodata(0);
	else
		GLG_ERR(" glg_store enable nodata cmd error !!\n");

	mutex_unlock(&glg_context_obj->glg_op_mutex);
	return count;
}

static ssize_t glg_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct glg_context *cxt = NULL;
	int res = 0;
	int en = 0;

	GLG_LOG("glg_store_active buf=%s\n", buf);
	mutex_lock(&glg_context_obj->glg_op_mutex);

	cxt = glg_context_obj;

	res = kstrtoint(buf, 10, &en);
	if (res != 0)
		GLG_LOG(" glg_store_active param error: res = %d\n", res);

	GLG_LOG(" glg_store_active en=%d\n", en);
	if (1 == en)
		glg_real_enable(1);
	else if (0 == en)
		glg_real_enable(0);
	else
		GLG_ERR(" glg_store_active error !!\n");

	mutex_unlock(&glg_context_obj->glg_op_mutex);
	GLG_LOG(" glg_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t glg_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct glg_context *cxt = NULL;

	cxt = glg_context_obj;
	GLG_LOG("glg active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data);
}

static ssize_t glg_store_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;

	GLG_LOG(" not support now\n");
	return len;
}


static ssize_t glg_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GLG_LOG(" not support now\n");
	return len;
}


static ssize_t glg_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	GLG_LOG(" not support now\n");
	return len;
}

static ssize_t glg_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GLG_LOG(" not support now\n");
	return len;
}

static ssize_t glg_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	GLG_LOG(" not support now\n");
	return len;
}

static ssize_t glg_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GLG_LOG(" not support now\n");
	return len;
}

static ssize_t glg_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &glg_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}

	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int glance_gesture_remove(struct platform_device *pdev)
{
	GLG_LOG("glance_gesture_remove\n");
	return 0;
}

static int glance_gesture_probe(struct platform_device *pdev)
{
	GLG_LOG("glance_gesture_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id glance_gesture_of_match[] = {
	{.compatible = "mediatek,glance_gesture",},
	{},
};
#endif

static struct platform_driver glance_gesture_driver = {
	.probe = glance_gesture_probe,
	.remove = glance_gesture_remove,
	.driver = {
		   .name = "glance_gesture",
#ifdef CONFIG_OF
		   .of_match_table = glance_gesture_of_match,
#endif
		   }
};

static int glg_real_driver_init(void)
{
	int err = 0;

	GLG_LOG(" glg_real_driver_init +\n");
	if (0 != glance_gesture_init) {
		GLG_LOG(" glg try to init driver %s\n", glance_gesture_init->name);
		err = glance_gesture_init->init();
		if (0 == err)
			GLG_LOG(" glg real driver %s probe ok\n", glance_gesture_init->name);
	}
	wake_lock_init(&glg_lock, WAKE_LOCK_SUSPEND, "glg wakelock");
	init_timer(&glg_context_obj->notify_timer);
	glg_context_obj->notify_timer.expires = HZ / 5;	/* 200 ms */
	glg_context_obj->notify_timer.function = notify_glg_timeout;
	glg_context_obj->notify_timer.data = (unsigned long)glg_context_obj;

	return err;
}

int glg_driver_add(struct glg_init_info *obj)
{
	int err = 0;

	GLG_FUN();
	GLG_LOG("register glance_gesture driver for the first time\n");
	if (platform_driver_register(&glance_gesture_driver))
		GLG_ERR("failed to register gensor driver already exist\n");

	if (NULL == glance_gesture_init) {
		obj->platform_diver_addr = &glance_gesture_driver;
		glance_gesture_init = obj;
	}

	if (NULL == glance_gesture_init) {
		GLG_ERR("GLG driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(glg_driver_add);

static int glg_misc_init(struct glg_context *cxt)
{
	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = GLG_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		GLG_ERR("unable to register glg misc device!!\n");

	return err;
}

static void glg_input_destroy(struct glg_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int glg_input_init(struct glg_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GLG_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_GLG_VALUE);

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

DEVICE_ATTR(glgenablenodata, S_IWUSR | S_IRUGO, glg_show_enable_nodata, glg_store_enable_nodata);
DEVICE_ATTR(glgactive, S_IWUSR | S_IRUGO, glg_show_active, glg_store_active);
DEVICE_ATTR(glgdelay, S_IWUSR | S_IRUGO, glg_show_delay, glg_store_delay);
DEVICE_ATTR(glgbatch, S_IWUSR | S_IRUGO, glg_show_batch, glg_store_batch);
DEVICE_ATTR(glgflush, S_IWUSR | S_IRUGO, glg_show_flush, glg_store_flush);
DEVICE_ATTR(glgdevnum, S_IWUSR | S_IRUGO, glg_show_devnum, NULL);


static struct attribute *glg_attributes[] = {
	&dev_attr_glgenablenodata.attr,
	&dev_attr_glgactive.attr,
	&dev_attr_glgdelay.attr,
	&dev_attr_glgbatch.attr,
	&dev_attr_glgflush.attr,
	&dev_attr_glgdevnum.attr,
	NULL
};

static struct attribute_group glg_attribute_group = {
	.attrs = glg_attributes
};

int glg_register_data_path(struct glg_data_path *data)
{
	struct glg_context *cxt = NULL;

	cxt = glg_context_obj;
	cxt->glg_data.get_data = data->get_data;
	if (NULL == cxt->glg_data.get_data) {
		GLG_LOG("glg register data path fail\n");
		return -1;
	}
	return 0;
}

int glg_register_control_path(struct glg_control_path *ctl)
{
	struct glg_context *cxt = NULL;
	int err = 0;

	cxt = glg_context_obj;
/* cxt->glg_ctl.enable = ctl->enable; */
/* cxt->glg_ctl.enable_nodata = ctl->enable_nodata; */
	cxt->glg_ctl.open_report_data = ctl->open_report_data;

	if (NULL == cxt->glg_ctl.open_report_data) {
		GLG_LOG("glg register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = glg_misc_init(glg_context_obj);
	if (err) {
		GLG_ERR("unable to register glg misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&glg_context_obj->mdev.this_device->kobj, &glg_attribute_group);
	if (err < 0) {
		GLG_ERR("unable to create glg attribute file\n");
		return -3;
	}
	kobject_uevent(&glg_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}

static int glg_probe(void)
{
	int err;

	GLG_LOG("+++++++++++++glg_probe!!\n");

	glg_context_obj = glg_context_alloc_object();
	if (!glg_context_obj) {
		err = -ENOMEM;
		GLG_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real glg driver */
	err = glg_real_driver_init();
	if (err) {
		GLG_ERR("glg real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = glg_input_init(glg_context_obj);
	if (err) {
		GLG_ERR("unable to register glg input device!\n");
		goto exit_alloc_input_dev_failed;
	}
	GLG_LOG("----glg_probe OK !!\n");
	return 0;

	if (err) {
		GLG_ERR("sysfs node creation error\n");
		glg_input_destroy(glg_context_obj);
	}
real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(glg_context_obj);
exit_alloc_data_failed:
	GLG_LOG("----glg_probe fail !!!\n");
	return err;
}

static int glg_remove(void)
{
	int err = 0;

	GLG_FUN();
	input_unregister_device(glg_context_obj->idev);
	sysfs_remove_group(&glg_context_obj->idev->dev.kobj, &glg_attribute_group);

	err = misc_deregister(&glg_context_obj->mdev);
	if (err)
		GLG_ERR("misc_deregister fail: %d\n", err);

	kfree(glg_context_obj);
	return 0;
}
static int __init glg_init(void)
{
	GLG_FUN();

	if (glg_probe()) {
		GLG_ERR("failed to register glg driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit glg_exit(void)
{
	glg_remove();
	platform_driver_unregister(&glance_gesture_driver);
}

late_initcall(glg_init);
/* module_init(glg_init); */
/* module_exit(glg_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLG device driver");
MODULE_AUTHOR("Mediatek");

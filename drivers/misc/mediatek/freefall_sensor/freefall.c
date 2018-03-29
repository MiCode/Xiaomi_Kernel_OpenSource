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

#include "freefall.h"

static struct freefall_context *freefall_context_obj;

static struct freefall_init_info *freefall_sensor_init = { 0 };	/* modified */

static void freefall_early_suspend(struct early_suspend *h);
static void freefall_late_resume(struct early_suspend *h);

static int resume_enable_status;
static struct wake_lock freefall_lock;
static void notify_freefall_timeout(unsigned long);

static struct freefall_context *freefall_context_alloc_object(void)
{
	struct freefall_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	FREEFALL_LOG("freefall_context_alloc_object++++\n");
	if (!obj) {
		FREEFALL_ERR("Alloc freefall object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->freefall_op_mutex);

	FREEFALL_LOG("freefall_context_alloc_object----\n");
	return obj;
}

static void notify_freefall_timeout(unsigned long data)
{
	wake_unlock(&freefall_lock);
}

int freefall_notify(void)
{
	int err = 0;
	int value = 0;
	struct freefall_context *cxt = NULL;

	cxt = freefall_context_obj;
	FREEFALL_LOG("freefall_notify++++\n");

	value = 1;
	input_report_rel(cxt->idev, EVENT_TYPE_FREEFALL_VALUE, value);
	input_sync(cxt->idev);

	wake_lock(&freefall_lock);
	mod_timer(&cxt->notify_timer, jiffies + HZ / 5);

	return err;
}

static int freefall_real_enable(int enable)
{
	int err = 0;
	struct freefall_context *cxt = NULL;

	cxt = freefall_context_obj;
	if (FREEFALL_RESUME == enable)
		enable = resume_enable_status;

	if (1 == enable) {
		resume_enable_status = 1;
		if (atomic_read(&(freefall_context_obj->early_suspend)))
			return 0;

		if (false == cxt->is_active_data) {
			err = cxt->freefall_ctl.open_report_data(1);
			if (err) {
				err = cxt->freefall_ctl.open_report_data(1);
				if (err) {
					err = cxt->freefall_ctl.open_report_data(1);
					if (err) {
						GLG_ERR
						    ("enable_glance_gesture enable(%d) err 3 timers = %d\n",
						     enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			FREEFALL_LOG("enable_glance_gesture real enable\n");
		}
	} else if ((0 == enable) || (FREEFALL_SUSPEND == enable)) {
		if (0 == enable)
			resume_enable_status = 0;
		if (true == cxt->is_active_data) {
			err = cxt->freefall_ctl.open_report_data(0);
			if (err)
				GLG_ERR("enable_glance_gestureenable(%d) err = %d\n", enable, err);

			cxt->is_active_data = false;
			FREEFALL_LOG("enable_glance_gesture real disable\n");
		}
	}
	return err;
}

int freefall_enable_nodata(int enable)
{
	struct freefall_context *cxt = NULL;

	cxt = freefall_context_obj;
	if (NULL == cxt->freefall_ctl.open_report_data) {
		GLG_ERR("freefall_enable_nodata:freefall ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;
	if (0 == enable)
		cxt->is_active_nodata = false;

	freefall_real_enable(enable);
	return 0;
}

static ssize_t freefall_show_enable_nodata(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct freefall_context *cxt = NULL;

	cxt = freefall_context_obj;

	FREEFALL_LOG("freefall active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata);
}

static ssize_t freefall_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct freefall_context *cxt = NULL;

	FREEFALL_LOG("freefall_store_enable nodata buf=%s\n", buf);
	mutex_lock(&freefall_context_obj->freefall_op_mutex);
	cxt = freefall_context_obj;
	if (NULL == cxt->freefall_ctl.open_report_data) {
		FREEFALL_LOG("freefall_ctl enable nodata NULL\n");
		mutex_unlock(&freefall_context_obj->freefall_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		freefall_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		freefall_enable_nodata(0);
	else
		GLG_ERR(" freefall_store enable nodata cmd error !!\n");

	mutex_unlock(&freefall_context_obj->freefall_op_mutex);
	return count;
}

static ssize_t freefall_store_active(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct freefall_context *cxt = NULL;
	int res = 0;
	int en = 0;

	FREEFALL_LOG("freefall_store_active buf=%s\n", buf);
	mutex_lock(&freefall_context_obj->freefall_op_mutex);

	cxt = freefall_context_obj;

	res = kstrtoint(buf, 10, &en);
	if (res != 0)
		FREEFALL_LOG(" freefall_store_active param error: res = %d\n", res);

	FREEFALL_LOG(" freefall_store_active en=%d\n", en);
	if (1 == en)
		freefall_real_enable(1);
	else if (0 == en)
		freefall_real_enable(0);
	else
		FREEFALL_ERR(" freefall_store_active error !!\n");

	mutex_unlock(&freefall_context_obj->freefall_op_mutex);
	FREEFALL_LOG(" freefall_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t freefall_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct freefall_context *cxt = NULL;

	cxt = freefall_context_obj;
	FREEFALL_LOG("freefall active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data);
}

static ssize_t freefall_store_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FREEFALL_LOG(" not support now\n");
	return len;
}


static ssize_t freefall_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FREEFALL_LOG(" not support now\n");
	return len;
}


static ssize_t freefall_store_batch(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int len = 0;

	FREEFALL_LOG(" not support now\n");
	return len;
}

static ssize_t freefall_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FREEFALL_LOG(" not support now\n");
	return len;
}

static ssize_t freefall_store_flush(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int len = 0;

	FREEFALL_LOG(" not support now\n");
	return len;
}

static ssize_t freefall_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FREEFALL_LOG(" not support now\n");
	return len;
}

static ssize_t freefall_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &freefall_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int freefall_remove(struct platform_device *pdev)
{
	FREEFALL_LOG("freefall_remove\n");
	return 0;
}

static int freefall_probe(struct platform_device *pdev)
{
	FREEFALL_LOG("freefall_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id freefall_of_match[] = {
	{.compatible = "mediatek,freefall_sensor",},
	{},
};
#endif

static struct platform_driver freefall_driver = {
	.probe = freefall_probe,
	.remove = freefall_remove,
	.driver = {
		   .name = "freefall_sensor",
#ifdef CONFIG_OF
		   .of_match_table = freefall_of_match,
#endif
		   }
};

static int freefall_real_driver_init(void)
{
	int err = 0;

	FREEFALL_LOG(" freefall_real_driver_init +\n");
	if (0 != freefall_sensor_init) {
		FREEFALL_LOG(" freefall try to init driver %s\n", freefall_sensor_init->name);
		err = freefall_sensor_init->init();
		if (0 == err) {
			FREEFALL_LOG(" freefall real driver %s probe ok\n",
				     freefall_sensor_init->name);
		}
	}
	wake_lock_init(&freefall_lock, WAKE_LOCK_SUSPEND, "freefall wakelock");
	init_timer(&freefall_context_obj->notify_timer);
	freefall_context_obj->notify_timer.expires = HZ / 5;	/* 200 ms */
	freefall_context_obj->notify_timer.function = notify_freefall_timeout;
	freefall_context_obj->notify_timer.data = (unsigned long)freefall_context_obj;

	return err;
}

int freefall_driver_add(struct freefall_init_info *obj)
{
	int err = 0;

	FREEFALL_FUN();
	FREEFALL_LOG("register glance_gesture driver for the first time\n");
	if (platform_driver_register(&freefall_driver))
		GLG_ERR("failed to register gensor driver already exist\n");

	if (NULL == freefall_sensor_init) {
		obj->platform_diver_addr = &freefall_driver;
		freefall_sensor_init = obj;
	}

	if (NULL == freefall_sensor_init) {
		GLG_ERR("GLG driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(freefall_driver_add);

static int freefall_misc_init(struct freefall_context *cxt)
{
	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = M_GLG_MISC_MINOR;
	cxt->mdev.name = GLG_MISC_DEV_NAME;
	err = misc_register(&cxt->mdev);
	if (err)
		GLG_ERR("unable to register freefall misc device!!\n");

	return err;
}

static void freefall_input_destroy(struct freefall_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int freefall_input_init(struct freefall_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GLG_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_FREEFALL_VALUE);

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

DEVICE_ATTR(freefallenablenodata, S_IWUSR | S_IRUGO, freefall_show_enable_nodata,
	    freefall_store_enable_nodata);
DEVICE_ATTR(freefallactive, S_IWUSR | S_IRUGO, freefall_show_active, freefall_store_active);
DEVICE_ATTR(freefalldelay, S_IWUSR | S_IRUGO, freefall_show_delay, freefall_store_delay);
DEVICE_ATTR(freefallbatch, S_IWUSR | S_IRUGO, freefall_show_batch, freefall_store_batch);
DEVICE_ATTR(freefallflush, S_IWUSR | S_IRUGO, freefall_show_flush, freefall_store_flush);
DEVICE_ATTR(freefalldevnum, S_IWUSR | S_IRUGO, freefall_show_devnum, NULL);


static struct attribute *freefall_attributes[] = {
	&dev_attr_freefallenablenodata.attr,
	&dev_attr_freefallactive.attr,
	&dev_attr_freefalldelay.attr,
	&dev_attr_freefallbatch.attr,
	&dev_attr_freefallflush.attr,
	&dev_attr_freefalldevnum.attr,
	NULL
};

static struct attribute_group freefall_attribute_group = {
	.attrs = freefall_attributes
};

int freefall_register_data_path(struct freefall_data_path *data)
{
	struct freefall_context *cxt = NULL;

	cxt = freefall_context_obj;
	cxt->freefall_data.get_data = data->get_data;
	if (NULL == cxt->freefall_data.get_data) {
		FREEFALL_LOG("freefall register data path fail\n");
		return -1;
	}
	return 0;
}

int freefall_register_control_path(struct freefall_control_path *ctl)
{
	struct freefall_context *cxt = NULL;
	int err = 0;

	cxt = freefall_context_obj;
/* cxt->freefall_ctl.enable = ctl->enable; */
/* cxt->freefall_ctl.enable_nodata = ctl->enable_nodata; */
	cxt->freefall_ctl.open_report_data = ctl->open_report_data;

	if (NULL == cxt->freefall_ctl.open_report_data) {
		FREEFALL_LOG("freefall register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = freefall_misc_init(freefall_context_obj);
	if (err) {
		GLG_ERR("unable to register freefall misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&freefall_context_obj->mdev.this_device->kobj,
				 &freefall_attribute_group);
	if (err < 0) {
		GLG_ERR("unable to create freefall attribute file\n");
		return -3;
	}
	kobject_uevent(&freefall_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}

static int freefall_probe(struct platform_device *pdev)
{
	int err;

	FREEFALL_LOG("+++++++++++++freefall_probe!!\n");

	freefall_context_obj = freefall_context_alloc_object();
	if (!freefall_context_obj) {
		err = -ENOMEM;
		GLG_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real freefall driver */
	err = freefall_real_driver_init();
	if (err) {
		GLG_ERR("freefall real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = freefall_input_init(freefall_context_obj);
	if (err) {
		GLG_ERR("unable to register freefall input device!\n");
		goto exit_alloc_input_dev_failed;
	}
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
	atomic_set(&(freefall_context_obj->early_suspend), 0);
	freefall_context_obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	    freefall_context_obj->early_drv.suspend = freefall_early_suspend,
	    freefall_context_obj->early_drv.resume = freefall_late_resume,
	    register_early_suspend(&freefall_context_obj->early_drv);
#endif				/* #if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND) */

	FREEFALL_LOG("----freefall_probe OK !!\n");
	return 0;

	if (err) {
		GLG_ERR("sysfs node creation error\n");
		freefall_input_destroy(freefall_context_obj);
	}
real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(freefall_context_obj);
exit_alloc_data_failed:
	FREEFALL_LOG("----freefall_probe fail !!!\n");
	return err;
}

static int freefall_remove(struct platform_device *pdev)
{
	int err = 0;

	GLG_FUN();
	input_unregister_device(freefall_context_obj->idev);
	sysfs_remove_group(&freefall_context_obj->idev->dev.kobj, &freefall_attribute_group);

	err = misc_deregister(&freefall_context_obj->mdev);
	if (err)
		GLG_ERR("misc_deregister fail: %d\n", err);

	kfree(freefall_context_obj);
	return 0;
}

static void freefall_early_suspend(struct early_suspend *h)
{
	atomic_set(&(freefall_context_obj->early_suspend), 1);
	if (!atomic_read(&freefall_context_obj->wake))	/* not wake up, disable in early suspend */
		freefall_real_enable(GLG_SUSPEND);

	FREEFALL_LOG(" freefall_early_suspend ok------->hwm_obj->early_suspend=%d\n",
		     atomic_read(&(freefall_context_obj->early_suspend)));
}

/*----------------------------------------------------------------------------*/
static void freefall_late_resume(struct early_suspend *h)
{
	atomic_set(&(freefall_context_obj->early_suspend), 0);
	if (!atomic_read(&freefall_context_obj->wake) && resume_enable_status)
		freefall_real_enable(GLG_RESUME);

	FREEFALL_LOG(" freefall_late_resume ok------->hwm_obj->early_suspend=%d\n",
		     atomic_read(&(freefall_context_obj->early_suspend)));
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int freefall_suspend(struct platform_device *dev, pm_message_t state)
{
	atomic_set(&(freefall_context_obj->suspend), 1);
	if (!atomic_read(&freefall_context_obj->wake))
		freefall_real_enable(GLG_SUSPEND);

	FREEFALL_LOG(" freefall_suspend ok------->hwm_obj->suspend=%d\n",
		     atomic_read(&(freefall_context_obj->suspend)));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int freefall_resume(struct platform_device *dev)
{
	atomic_set(&(freefall_context_obj->suspend), 0);
	if (!atomic_read(&freefall_context_obj->wake) && resume_enable_status)
		freefall_real_enable(GLG_RESUME);

	FREEFALL_LOG(" freefall_resume ok------->hwm_obj->suspend=%d\n",
		     atomic_read(&(freefall_context_obj->suspend)));
	return 0;
}
#endif				/* #if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND) */

#ifdef CONFIG_OF
static const struct of_device_id m_freefall_pl_of_match[] = {
	{.compatible = "mediatek,m_freefall_pl",},
	{},
};
#endif

static struct platform_driver freefall_driver = {
	.probe = freefall_probe,
	.remove = freefall_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend = freefall_suspend,
	.resume = freefall_resume,
#endif
	.driver = {
		   .name = GLG_FREEFALL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = m_freefall_pl_of_match,
#endif
		   }
};

static int __init freefall_init(void)
{
	GLG_FUN();

	if (platform_driver_register(&freefall_driver)) {
		GLG_ERR("failed to register freefall driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit freefall_exit(void)
{
	platform_driver_unregister(&freefall_driver);
	platform_driver_unregister(&freefall_driver);
}

late_initcall(freefall_init);
/* module_init(freefall_init); */
/* module_exit(freefall_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GLG device driver");
MODULE_AUTHOR("Mediatek");

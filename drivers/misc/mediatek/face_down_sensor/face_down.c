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

#include "face_down.h"

static struct fdn_context *fdn_context_obj;

static struct fdn_init_info *face_down_init = { 0 };	/* modified */

static void fdn_early_suspend(struct early_suspend *h);
static void fdn_late_resume(struct early_suspend *h);

static int resume_enable_status;

static struct fdn_context *fdn_context_alloc_object(void)
{
	struct fdn_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	FDN_LOG("fdn_context_alloc_object++++\n");
	if (!obj) {
		FDN_ERR("Alloc fdn object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->fdn_op_mutex);

	FDN_LOG("fdn_context_alloc_object----\n");
	return obj;
}

int fdn_notify(void)
{
	int err = 0;
	int value = 0;
	struct fdn_context *cxt = NULL;

	cxt = fdn_context_obj;
	FDN_LOG("fdn_notify++++\n");

	value = 1;
	input_report_rel(cxt->idev, EVENT_TYPE_FDN_VALUE, value);
	input_sync(cxt->idev);

	return err;
}

static int fdn_real_enable(int enable)
{
	int err = 0;
	struct fdn_context *cxt = NULL;

	cxt = fdn_context_obj;

	if (FDN_RESUME == enable)
		enable = resume_enable_status;

	if (1 == enable) {
		resume_enable_status = 1;
		if (atomic_read(&(fdn_context_obj->early_suspend)))
			return 0;

		if (false == cxt->is_active_data) {
			err = cxt->fdn_ctl.open_report_data(1);
			if (err) {
				err = cxt->fdn_ctl.open_report_data(1);
				if (err) {
					err = cxt->fdn_ctl.open_report_data(1);
					if (err) {
						FDN_ERR
						    ("enable_face_down enable(%d) err 3 timers = %d\n",
						     enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			FDN_LOG("enable_face_down real enable\n");
		}
	} else if ((0 == enable) || (FDN_SUSPEND == enable)) {
		if (0 == enable)
			resume_enable_status = 0;
		if (true == cxt->is_active_data) {
			err = cxt->fdn_ctl.open_report_data(0);
			if (err)
				FDN_ERR("enable_face_downenable(%d) err = %d\n", enable, err);

			cxt->is_active_data = false;
			FDN_LOG("enable_face_down real disable\n");
		}
	}
	return err;
}

int fdn_enable_nodata(int enable)
{
	struct fdn_context *cxt = NULL;

	cxt = fdn_context_obj;
	if (NULL == cxt->fdn_ctl.open_report_data) {
		FDN_ERR("fdn_enable_nodata:fdn ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	fdn_real_enable(enable);
	return 0;
}

static ssize_t fdn_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fdn_context *cxt = NULL;

	cxt = fdn_context_obj;

	FDN_LOG("fdn active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata);
}

static ssize_t fdn_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct fdn_context *cxt = NULL;

	FDN_LOG("fdn_store_enable nodata buf=%s\n", buf);
	mutex_lock(&fdn_context_obj->fdn_op_mutex);
	cxt = fdn_context_obj;
	if (NULL == cxt->fdn_ctl.open_report_data) {
		FDN_LOG("fdn_ctl enable nodata NULL\n");
		mutex_unlock(&fdn_context_obj->fdn_op_mutex);
		return count;
	}

	if (!strncmp(buf, "1", 1))
		fdn_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		fdn_enable_nodata(0);
	else
		FDN_ERR(" fdn_store enable nodata cmd error !!\n");

	mutex_unlock(&fdn_context_obj->fdn_op_mutex);
	return count;
}

static ssize_t fdn_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct fdn_context *cxt = NULL;
	int res = 0;
	int en = 0;

	FDN_LOG("fdn_store_active buf=%s\n", buf);
	mutex_lock(&fdn_context_obj->fdn_op_mutex);

	cxt = fdn_context_obj;
	res = kstrtoint(buf, 10, &en);
	if (res != 0)
		FDN_LOG(" fdn_store_active param error: res = %d\n", res);

	FDN_LOG(" fdn_store_active en=%d\n", en);

	if (1 == en)
		fdn_real_enable(1);
	else if (0 == en)
		fdn_real_enable(0);
	else
		FDN_ERR(" fdn_store_active error !!\n");

	mutex_unlock(&fdn_context_obj->fdn_op_mutex);
	FDN_LOG(" fdn_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t fdn_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fdn_context *cxt = NULL;

	cxt = fdn_context_obj;
	FDN_LOG("fdn active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data);
}

static ssize_t fdn_store_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FDN_LOG(" not support now\n");
	return len;
}


static ssize_t fdn_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FDN_LOG(" not support now\n");
	return len;
}


static ssize_t fdn_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	FDN_LOG(" not support now\n");
	return len;
}

static ssize_t fdn_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FDN_LOG(" not support now\n");
	return len;
}

static ssize_t fdn_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	FDN_LOG(" not support now\n");
	return len;
}

static ssize_t fdn_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	FDN_LOG(" not support now\n");
	return len;
}

static ssize_t fdn_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *devname = NULL;
	struct input_handle *handle;

	list_for_each_entry(handle, &fdn_context_obj->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}

	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}

static int face_down_remove(struct platform_device *pdev)
{
	FDN_LOG("face_down_remove\n");
	return 0;
}

static int face_down_probe(struct platform_device *pdev)
{
	FDN_LOG("face_down_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id face_down_of_match[] = {
	{.compatible = "mediatek,face_down",},
	{},
};
#endif

static struct platform_driver face_down_driver = {
	.probe = face_down_probe,
	.remove = face_down_remove,
	.driver = {
		   .name = "face_down",
#ifdef CONFIG_OF
		   .of_match_table = face_down_of_match,
#endif
		   }
};

static int fdn_real_driver_init(void)
{
	int err = 0;

	FDN_LOG(" fdn_real_driver_init +\n");
	if (0 != face_down_init) {
		FDN_LOG(" fdn try to init driver %s\n", face_down_init->name);
		err = face_down_init->init();
		if (0 == err)
			FDN_LOG(" fdn real driver %s probe ok\n", face_down_init->name);
	}
	return err;
}

int fdn_driver_add(struct fdn_init_info *obj)
{
	int err = 0;

	FDN_FUN();
	FDN_LOG("register face_down driver for the first time\n");
	if (platform_driver_register(&face_down_driver))
		FDN_ERR("failed to register gensor driver already exist\n");

	if (NULL == face_down_init) {
		obj->platform_diver_addr = &face_down_driver;
		face_down_init = obj;
	}

	if (NULL == face_down_init) {
		FDN_ERR("FDN driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(fdn_driver_add);

static int fdn_misc_init(struct fdn_context *cxt)
{
	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = M_FDN_MISC_MINOR;
	cxt->mdev.name = FDN_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		FDN_ERR("unable to register fdn misc device!!\n");

	return err;
}

static void fdn_input_destroy(struct fdn_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int fdn_input_init(struct fdn_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = FDN_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_FDN_VALUE);

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

DEVICE_ATTR(fdnenablenodata, S_IWUSR | S_IRUGO, fdn_show_enable_nodata, fdn_store_enable_nodata);
DEVICE_ATTR(fdnactive, S_IWUSR | S_IRUGO, fdn_show_active, fdn_store_active);
DEVICE_ATTR(fdndelay, S_IWUSR | S_IRUGO, fdn_show_delay, fdn_store_delay);
DEVICE_ATTR(fdnbatch, S_IWUSR | S_IRUGO, fdn_show_batch, fdn_store_batch);
DEVICE_ATTR(fdnflush, S_IWUSR | S_IRUGO, fdn_show_flush, fdn_store_flush);
DEVICE_ATTR(fdndevnum, S_IWUSR | S_IRUGO, fdn_show_devnum, NULL);


static struct attribute *fdn_attributes[] = {
	&dev_attr_fdnenablenodata.attr,
	&dev_attr_fdnactive.attr,
	&dev_attr_fdndelay.attr,
	&dev_attr_fdnbatch.attr,
	&dev_attr_fdnflush.attr,
	&dev_attr_fdndevnum.attr,
	NULL
};

static struct attribute_group fdn_attribute_group = {
	.attrs = fdn_attributes
};

int fdn_register_data_path(struct fdn_data_path *data)
{
	struct fdn_context *cxt = NULL;

	cxt = fdn_context_obj;
	cxt->fdn_data.get_data = data->get_data;
	if (NULL == cxt->fdn_data.get_data) {
		FDN_LOG("fdn register data path fail\n");
		return -1;
	}
	return 0;
}

int fdn_register_control_path(struct fdn_control_path *ctl)
{
	struct fdn_context *cxt = NULL;
	int err = 0;

	cxt = fdn_context_obj;
/* cxt->fdn_ctl.enable = ctl->enable; */
/* cxt->fdn_ctl.enable_nodata = ctl->enable_nodata; */
	cxt->fdn_ctl.open_report_data = ctl->open_report_data;

	if (NULL == cxt->fdn_ctl.open_report_data) {
		FDN_LOG("fdn register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = fdn_misc_init(fdn_context_obj);
	if (err) {
		FDN_ERR("unable to register fdn misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&fdn_context_obj->mdev.this_device->kobj, &fdn_attribute_group);
	if (err < 0) {
		FDN_ERR("unable to create fdn attribute file\n");
		return -3;
	}
	kobject_uevent(&fdn_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}

static int fdn_probe(struct platform_device *pdev)
{
	int err;

	FDN_LOG("+++++++++++++fdn_probe!!\n");

	fdn_context_obj = fdn_context_alloc_object();
	if (!fdn_context_obj) {
		err = -ENOMEM;
		FDN_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real fdn driver */
	err = fdn_real_driver_init();
	if (err) {
		FDN_ERR("fdn real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = fdn_input_init(fdn_context_obj);
	if (err) {
		FDN_ERR("unable to register fdn input device!\n");
		goto exit_alloc_input_dev_failed;
	}
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
	atomic_set(&(fdn_context_obj->early_suspend), 0);
	fdn_context_obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	    fdn_context_obj->early_drv.suspend = fdn_early_suspend,
	    fdn_context_obj->early_drv.resume = fdn_late_resume,
	    register_early_suspend(&fdn_context_obj->early_drv);
#endif				/* #if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND) */

	FDN_LOG("----fdn_probe OK !!\n");
	return 0;


	if (err) {
		FDN_ERR("sysfs node creation error\n");
		fdn_input_destroy(fdn_context_obj);
	}
real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(fdn_context_obj);
exit_alloc_data_failed:
	FDN_LOG("----fdn_probe fail !!!\n");
	return err;
}

static int fdn_remove(struct platform_device *pdev)
{
	int err = 0;

	FDN_FUN(f);
	input_unregister_device(fdn_context_obj->idev);
	sysfs_remove_group(&fdn_context_obj->idev->dev.kobj, &fdn_attribute_group);

	err = misc_deregister(&fdn_context_obj->mdev);
	if (err)
		FDN_ERR("misc_deregister fail: %d\n", err);

	kfree(fdn_context_obj);
	return 0;
}

static void fdn_early_suspend(struct early_suspend *h)
{
	atomic_set(&(fdn_context_obj->early_suspend), 1);
	if (!atomic_read(&fdn_context_obj->wake))
		fdn_real_enable(FDN_SUSPEND);

	FDN_LOG(" fdn_early_suspend ok------->hwm_obj->early_suspend=%d\n",
		atomic_read(&(fdn_context_obj->early_suspend)));
}

/*----------------------------------------------------------------------------*/
static void fdn_late_resume(struct early_suspend *h)
{
	atomic_set(&(fdn_context_obj->early_suspend), 0);
	if (!atomic_read(&fdn_context_obj->wake) && resume_enable_status)
		fdn_real_enable(FDN_RESUME);

	FDN_LOG(" fdn_late_resume ok------->hwm_obj->early_suspend=%d\n",
		atomic_read(&(fdn_context_obj->early_suspend)));
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int fdn_suspend(struct platform_device *dev, pm_message_t state)
{
	atomic_set(&(fdn_context_obj->suspend), 1);
	if (!atomic_read(&fdn_context_obj->wake))
		fdn_real_enable(FDN_SUSPEND);

	FDN_LOG(" fdn_suspend ok------->hwm_obj->suspend=%d\n",
		atomic_read(&(fdn_context_obj->suspend)));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int fdn_resume(struct platform_device *dev)
{
	atomic_set(&(fdn_context_obj->suspend), 0);
	if (!atomic_read(&fdn_context_obj->wake) && resume_enable_status)
		fdn_real_enable(FDN_RESUME);

	FDN_LOG(" fdn_resume ok------->hwm_obj->suspend=%d\n",
		atomic_read(&(fdn_context_obj->suspend)));
	return 0;
}
#endif				/* #if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND) */

#ifdef CONFIG_OF
static const struct of_device_id m_fdn_pl_of_match[] = {
	{.compatible = "mediatek,m_fdn_pl",},
	{},
};
#endif

static struct platform_driver fdn_driver = {
	.probe = fdn_probe,
	.remove = fdn_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend = fdn_suspend,
	.resume = fdn_resume,
#endif
	.driver = {
		   .name = FDN_PL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = m_fdn_pl_of_match,
#endif
		   }
};

static int __init fdn_init(void)
{
	FDN_FUN();

	if (platform_driver_register(&fdn_driver)) {
		FDN_ERR("failed to register fdn driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit fdn_exit(void)
{
	platform_driver_unregister(&fdn_driver);
	platform_driver_unregister(&face_down_driver);
}

late_initcall(fdn_init);
/* module_init(fdn_init); */
/* module_exit(fdn_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FDN device driver");
MODULE_AUTHOR("Mediatek");

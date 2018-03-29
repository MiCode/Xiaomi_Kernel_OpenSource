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

#include "tilt_detector.h"

static struct tilt_context *tilt_context_obj;

static struct tilt_init_info *tilt_detector_init = { 0 };	/* modified */



static int resume_enable_status;

static struct tilt_context *tilt_context_alloc_object(void)
{
	struct tilt_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	TILT_LOG("tilt_context_alloc_object++++\n");
	if (!obj) {
		TILT_ERR("Alloc tilt object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->tilt_op_mutex);
	obj->is_batch_enable = false;
	TILT_LOG("tilt_context_alloc_object----\n");
	return obj;
}

int tilt_notify(void)
{
	int err = 0;
	struct sensor_event event;
	struct tilt_context *cxt = NULL;

	cxt = tilt_context_obj;
	if (true == cxt->is_active_data) {
		TILT_LOG("tilt_notify++++\n");
		event.flush_action = DATA_ACTION;
		event.word[0] = 1;
		err = sensor_input_event(cxt->mdev.minor, &event);
		if (err < 0)
			TILT_ERR("event buffer full, so drop this data\n");
	}
	return err;
}

int tilt_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(tilt_context_obj->mdev.minor, &event);
	if (err < 0)
		TILT_ERR("event buffer full, so drop this data\n");
	else
		TILT_LOG("flush\n");
	return err;
}

static int tilt_real_enable(int enable)
{
	int err = 0;
	struct tilt_context *cxt = NULL;

	cxt = tilt_context_obj;
	if (TILT_RESUME == enable)
		enable = resume_enable_status;

	if (1 == enable) {
		resume_enable_status = 1;
		if (atomic_read(&(tilt_context_obj->early_suspend)))	/* not allow to enable under suspend */
			return 0;

		if (NULL != cxt->tilt_ctl.set_delay) {
			if (cxt->is_batch_enable == false)
				cxt->tilt_ctl.set_delay(66000000);
		} else {
			TILT_ERR("tilt set delay = NULL\n");
		}
		err = cxt->tilt_ctl.open_report_data(1);
		if (err) {
			err = cxt->tilt_ctl.open_report_data(1);
			if (err) {
				err = cxt->tilt_ctl.open_report_data(1);
				if (err) {
					TILT_ERR
					    ("enable_tilt_detector enable(%d) err 3 timers = %d\n",
					     enable, err);
					return err;
				}
			}
		}
		cxt->is_active_data = true;
		TILT_LOG("enable_tilt_detector real enable\n");

	} else if ((0 == enable) || (TILT_SUSPEND == enable)) {
		if (0 == enable)
			resume_enable_status = 0;
		err = cxt->tilt_ctl.open_report_data(0);
		if (err)
			TILT_ERR("enable_tilt_detectorenable(%d) err = %d\n", enable, err);
		cxt->is_active_data = false;
		TILT_LOG("enable_tilt_detector real disable\n");
	}
	return err;
}

int tilt_enable_nodata(int enable)
{
	struct tilt_context *cxt = NULL;

	cxt = tilt_context_obj;
	if (NULL == cxt->tilt_ctl.open_report_data) {
		TILT_ERR("tilt_enable_nodata:tilt ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	tilt_real_enable(enable);
	return 0;
}

static ssize_t tilt_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tilt_context *cxt = NULL;

	cxt = tilt_context_obj;
	TILT_LOG("tilt active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata);
}

static ssize_t tilt_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct tilt_context *cxt = NULL;

	TILT_LOG("tilt_store_enable nodata buf=%s\n", buf);
	mutex_lock(&tilt_context_obj->tilt_op_mutex);
	cxt = tilt_context_obj;
	if (NULL == cxt->tilt_ctl.open_report_data) {
		TILT_LOG("tilt_ctl enable nodata NULL\n");
		mutex_unlock(&tilt_context_obj->tilt_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		tilt_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		tilt_enable_nodata(0);
	else
		TILT_ERR(" tilt_store enable nodata cmd error !!\n");

	mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	return count;
}

static ssize_t tilt_store_active(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct tilt_context *cxt = NULL;
	int res = 0;
	int en = 0;

	TILT_LOG("tilt_store_active buf=%s\n", buf);
	mutex_lock(&tilt_context_obj->tilt_op_mutex);

	cxt = tilt_context_obj;

	res = kstrtoint(buf, 10, &en);
	if (res != 0)
		TILT_LOG(" tilt_store_active param error: res = %d\n", res);

	TILT_LOG(" tilt_store_active en=%d\n", en);
	if (1 == en)
		tilt_real_enable(1);
	else if (0 == en)
		tilt_real_enable(0);
	else
		TILT_ERR(" tilt_store_active error !!\n");

	mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	TILT_LOG(" tilt_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t tilt_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tilt_context *cxt = NULL;

	cxt = tilt_context_obj;
	TILT_LOG("tilt active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data);
}

static ssize_t tilt_store_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;

	TILT_LOG(" not support now\n");
	return len;
}


static ssize_t tilt_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	TILT_LOG(" not support now\n");
	return len;
}


static ssize_t tilt_store_batch(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int len = 0;
	struct tilt_context *cxt = NULL;
	int handle = 0, flag = 0, err = 0;
	int64_t samplingPeriodNs = 0, maxBatchReportLatencyNs = 0;

	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &samplingPeriodNs, &maxBatchReportLatencyNs);
	if (err != 4)
		TILT_ERR("tilt_store_batch param error: err = %d\n", err);

	TILT_LOG("tilt_store_batch param: handle %d, flag:%d samplingPeriodNs:%lld, maxBatchReportLatencyNs: %lld\n",
			handle, flag, samplingPeriodNs, maxBatchReportLatencyNs);
	mutex_lock(&tilt_context_obj->tilt_op_mutex);
	cxt = tilt_context_obj;
	if (cxt->tilt_ctl.is_support_batch == true) {
		if (maxBatchReportLatencyNs != 0)
			cxt->is_batch_enable = true;
		else if (maxBatchReportLatencyNs == 0)
			cxt->is_batch_enable = false;
		else
			TILT_ERR(" tilt_store_batch error !!\n");
	} else {
		maxBatchReportLatencyNs = 0;
	}
	if (NULL != cxt->tilt_ctl.batch)
		err = cxt->tilt_ctl.batch(flag, samplingPeriodNs, maxBatchReportLatencyNs);
	else
		TILT_ERR("TILT DRIVER OLD ARCHITECTURE DON'T SUPPORT TILT COMMON VERSION BATCH\n");
	if (err < 0)
		TILT_ERR("tilt enable batch err %d\n", err);
	mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	return len;
}

static ssize_t tilt_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	TILT_LOG(" not support now\n");
	return len;
}

static ssize_t tilt_store_flush(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct tilt_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		TILT_ERR("tilt_store_flush param error: err = %d\n", err);

	TILT_ERR("tilt_store_flush param: handle %d\n", handle);

	mutex_lock(&tilt_context_obj->tilt_op_mutex);
	cxt = tilt_context_obj;
	if (NULL != cxt->tilt_ctl.flush)
		err = cxt->tilt_ctl.flush();
	else
		TILT_ERR("TILT DRIVER OLD ARCHITECTURE DON'T SUPPORT TILT COMMON VERSION FLUSH\n");
	if (err < 0)
		TILT_ERR("tilt enable flush err %d\n", err);
	mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	return count;
}

static ssize_t tilt_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	TILT_LOG(" not support now\n");
	return len;
}

static ssize_t tilt_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);	/* TODO: why +5? */
}

static int tilt_detector_remove(struct platform_device *pdev)
{
	TILT_LOG("tilt_detector_remove\n");
	return 0;
}

static int tilt_detector_probe(struct platform_device *pdev)
{
	TILT_LOG("tilt_detector_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tilt_detector_of_match[] = {
	{.compatible = "mediatek,tilt_detector",},
	{},
};
#endif

static struct platform_driver tilt_detector_driver = {
	.probe = tilt_detector_probe,
	.remove = tilt_detector_remove,
	.driver = {
		   .name = "tilt_detector",
#ifdef CONFIG_OF
		   .of_match_table = tilt_detector_of_match,
#endif
		   }
};

static int tilt_real_driver_init(void)
{
	int err = 0;

	TILT_LOG(" tilt_real_driver_init +\n");
	if (0 != tilt_detector_init) {
		TILT_LOG(" tilt try to init driver %s\n", tilt_detector_init->name);
		err = tilt_detector_init->init();
		if (0 == err)
			TILT_LOG(" tilt real driver %s probe ok\n", tilt_detector_init->name);
	}
	return err;
}

int tilt_driver_add(struct tilt_init_info *obj)
{
	int err = 0;

	TILT_FUN();
	TILT_LOG("register tilt_detector driver for the first time\n");
	if (platform_driver_register(&tilt_detector_driver))
		TILT_ERR("failed to register gensor driver already exist\n");

	if (NULL == tilt_detector_init) {
		obj->platform_diver_addr = &tilt_detector_driver;
		tilt_detector_init = obj;
	}

	if (NULL == tilt_detector_init) {
		TILT_ERR("TILT driver add err\n");
		err = -1;
	}

	return err;
} EXPORT_SYMBOL_GPL(tilt_driver_add);
static int tilt_detect_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t tilt_detect_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(tilt_context_obj->mdev.minor, file, buffer, count, ppos);

	return read_cnt;
}

static unsigned int tilt_detect_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(tilt_context_obj->mdev.minor, file, wait);
}

static const struct file_operations tilt_detect_fops = {
	.owner = THIS_MODULE,
	.open = tilt_detect_open,
	.read = tilt_detect_read,
	.poll = tilt_detect_poll,
};

static int tilt_misc_init(struct tilt_context *cxt)
{
	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = ID_TILT_DETECTOR;
	cxt->mdev.name = TILT_MISC_DEV_NAME;
	cxt->mdev.fops = &tilt_detect_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		TILT_ERR("unable to register tilt misc device!!\n");

	return err;
}

static DEVICE_ATTR(tiltenablenodata, S_IWUSR | S_IRUGO, tilt_show_enable_nodata, tilt_store_enable_nodata);
static DEVICE_ATTR(tiltactive, S_IWUSR | S_IRUGO, tilt_show_active, tilt_store_active);
static DEVICE_ATTR(tiltdelay, S_IWUSR | S_IRUGO, tilt_show_delay, tilt_store_delay);
static DEVICE_ATTR(tiltbatch, S_IWUSR | S_IRUGO, tilt_show_batch, tilt_store_batch);
static DEVICE_ATTR(tiltflush, S_IWUSR | S_IRUGO, tilt_show_flush, tilt_store_flush);
static DEVICE_ATTR(tiltdevnum, S_IWUSR | S_IRUGO, tilt_show_devnum, NULL);



static struct attribute *tilt_attributes[] = {
	&dev_attr_tiltenablenodata.attr,
	&dev_attr_tiltactive.attr,
	&dev_attr_tiltdelay.attr,
	&dev_attr_tiltbatch.attr,
	&dev_attr_tiltflush.attr,
	&dev_attr_tiltdevnum.attr,
	NULL
};

static struct attribute_group tilt_attribute_group = {
	.attrs = tilt_attributes
};

int tilt_register_data_path(struct tilt_data_path *data)
{
	struct tilt_context *cxt = NULL;

	cxt = tilt_context_obj;
	cxt->tilt_data.get_data = data->get_data;
	if (NULL == cxt->tilt_data.get_data) {
		TILT_LOG("tilt register data path fail\n");
		return -1;
	}
	return 0;
}

int tilt_register_control_path(struct tilt_control_path *ctl)
{
	struct tilt_context *cxt = NULL;
	int err = 0;

	cxt = tilt_context_obj;
/* cxt->tilt_ctl.enable = ctl->enable; */
/* cxt->tilt_ctl.enable_nodata = ctl->enable_nodata; */
	cxt->tilt_ctl.open_report_data = ctl->open_report_data;
	cxt->tilt_ctl.set_delay = ctl->set_delay;
	cxt->tilt_ctl.batch = ctl->batch;
	cxt->tilt_ctl.flush = ctl->flush;

	if (NULL == cxt->tilt_ctl.open_report_data || NULL == cxt->tilt_ctl.set_delay) {
		TILT_LOG("tilt register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = tilt_misc_init(tilt_context_obj);
	if (err) {
		TILT_ERR("unable to register tilt misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&tilt_context_obj->mdev.this_device->kobj, &tilt_attribute_group);
	if (err < 0) {
		TILT_ERR("unable to create tilt attribute file\n");
		return -3;
	}
	kobject_uevent(&tilt_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}

static int tilt_probe(void)
{
	int err;

	TILT_LOG("+++++++++++++tilt_probe!!\n");
	tilt_context_obj = tilt_context_alloc_object();
	if (!tilt_context_obj) {
		err = -ENOMEM;
		TILT_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real tilt driver */
	err = tilt_real_driver_init();
	if (err) {
		TILT_ERR("tilt real driver init fail\n");
		goto real_driver_init_fail;
	}
	TILT_LOG("----tilt_probe OK !!\n");
	return 0;
real_driver_init_fail:
	kfree(tilt_context_obj);
exit_alloc_data_failed:
	TILT_LOG("----tilt_probe fail !!!\n");
	return err;
}

static int tilt_remove(void)
{
	int err = 0;

	TILT_FUN(f);
	sysfs_remove_group(&tilt_context_obj->mdev.this_device->kobj, &tilt_attribute_group);

	err = sensor_attr_deregister(&tilt_context_obj->mdev);
	if (err)
		TILT_ERR("misc_deregister fail: %d\n", err);

	kfree(tilt_context_obj);
	return 0;
}
static int __init tilt_init(void)
{
	TILT_FUN();

	if (tilt_probe()) {
		TILT_ERR("failed to register tilt driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit tilt_exit(void)
{
	tilt_remove();
	platform_driver_unregister(&tilt_detector_driver);
}

late_initcall(tilt_init);
/* module_init(tilt_init); */
/* module_exit(tilt_exit); */
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TILT device driver");
MODULE_AUTHOR("Mediatek");

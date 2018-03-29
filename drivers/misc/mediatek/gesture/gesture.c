/*
* Copyright (C) 2016 MediaTek Inc.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "gesture.h"

static struct ges_context *ges_context_obj;

static struct ges_init_info *gesture_init[GESTURE_MAX_SUPPORT] = {0};


static struct wake_lock ges_lock;
static void notify_ges_timeout(unsigned long);

static struct ges_context *ges_context_alloc_object(void)
{
	struct ges_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	GESTURE_LOG("ges_context_alloc_object++++\n");
	if (!obj) {
		GESTURE_ERR("Alloc ges object error!\n");
		return NULL;
	}
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->ges_op_mutex);

	GESTURE_LOG("ges_context_alloc_object----\n");
	return obj;
}

static void notify_ges_timeout(unsigned long data)
{
	wake_unlock(&ges_lock);
}

int ges_notify(int handle)
{
	int err = 0;
	int value = 0;
	struct ges_context *cxt = NULL;

	cxt = ges_context_obj;
	GESTURE_LOG("ges_notify++++\n");
	value = 1;

	switch (handle) {
	case ID_IN_POCKET:
		input_report_rel(cxt->idev, EVENT_TYPE_INPK_VALUE, value);
		break;
	case ID_STATIONARY:
		input_report_rel(cxt->idev, EVENT_TYPE_STATIONARY_VALUE, value);
		break;
	default:
		GESTURE_ERR("[%s] don't support handle:%d\n", __func__, handle);
	}
	input_sync(cxt->idev);

	wake_lock(&ges_lock);
	/* Max report rate need 200ms */
	mod_timer(&cxt->notify_timer, jiffies + HZ / 5);

	return err;
}

static int ges_real_enable(int enable, int handle)
{
	int err = 0;
	int index = -1;
	struct ges_context *cxt = NULL;

	cxt = ges_context_obj;
	index = HandleToIndex(handle);

	if (1 == enable) {
		if (false == cxt->ctl_context[index].is_active_data) {
			err = cxt->ctl_context[index].ges_ctl.open_report_data(1);
			if (err) {
				err = cxt->ctl_context[index].ges_ctl.open_report_data(1);
				if (err) {
					err = cxt->ctl_context[index].ges_ctl.open_report_data(1);
					if (err) {
						GESTURE_ERR
						    ("enable_gesture enable(%d) err 3 timers = %d\n",
						     enable, err);
						return err;
					}
				}
			}
			cxt->ctl_context[index].is_active_data = true;
			GESTURE_LOG("enable_gesture real enable\n");
		}
	} else if (0 == enable) {
		if (true == cxt->ctl_context[index].is_active_data) {
			err = cxt->ctl_context[index].ges_ctl.open_report_data(0);
			if (err)
				GESTURE_ERR("enable_gestureenable(%d) err = %d\n", enable, err);

			cxt->ctl_context[index].is_active_data = false;
			GESTURE_LOG("enable_gesture real disable\n");
		}
	}
	return err;
}

int ges_enable_nodata(int enable, int handle)
{
	struct ges_context *cxt = NULL;
	int index;

	index = HandleToIndex(handle);
	cxt = ges_context_obj;
	if (NULL == cxt->ctl_context[index].ges_ctl.open_report_data) {
		GESTURE_ERR("ges_enable_nodata:ges ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->ctl_context[index].is_active_nodata = true;

	if (0 == enable)
		cxt->ctl_context[index].is_active_nodata = false;
	ges_real_enable(enable, handle);
	return 0;
}

static ssize_t ges_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ges_context *cxt = NULL;
	int i;
	int s_len = 0;

	cxt = ges_context_obj;
	for (i = 0; i < GESTURE_MAX_SUPPORT; i++) {
		GESTURE_LOG("ges handle:%d active: %d\n", i, cxt->ctl_context[i].is_active_nodata);
		s_len += snprintf(buf + s_len, PAGE_SIZE, "id:%d, en:%d\n", i, cxt->ctl_context[i].is_active_nodata);
	}
	return s_len;
}

static ssize_t ges_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct ges_context *cxt = NULL;
	int handle, en;
	int err = -1;
	int index = -1;

	GESTURE_LOG("ges_store_enable nodata buf=%s\n", buf);
	mutex_lock(&ges_context_obj->ges_op_mutex);
	cxt = ges_context_obj;

	err = sscanf(buf, "%d : %d", &handle, &en);
	if (err < 0) {
		GESTURE_ERR("[%s] sscanf fail\n", __func__);
		return count;
	}
	GESTURE_LOG("[%s] handle=%d, en=%d\n", __func__, handle, en);
	index = HandleToIndex(handle);

	if (NULL == cxt->ctl_context[index].ges_ctl.open_report_data) {
		GESTURE_LOG("ges_ctl enable nodata NULL\n");
		mutex_unlock(&ges_context_obj->ges_op_mutex);
		return count;
	}
	GESTURE_LOG("[%s] handle=%d, en=%d\n", __func__, handle, en);
	ges_enable_nodata(en, handle);

	/* for debug */
	ges_notify(handle);

	mutex_unlock(&ges_context_obj->ges_op_mutex);
	return count;
}

static ssize_t ges_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ges_context *cxt = NULL;
	int res = 0;
	int en = 0;
	int handle = -1;

	GESTURE_LOG("ges_store_active buf=%s\n", buf);
	mutex_lock(&ges_context_obj->ges_op_mutex);

	cxt = ges_context_obj;

	res = sscanf(buf, "%d : %d", &handle, &en);
	if (res < 0) {
		GESTURE_LOG(" ges_store_active param error: res = %d\n", res);
		return count;
	}
	GESTURE_LOG(" ges_store_active handle=%d, en=%d\n", handle, en);
	if (1 == en)
		ges_real_enable(1, handle);
	else if (0 == en)
		ges_real_enable(0, handle);
	else
		GESTURE_ERR(" ges_store_active error !!\n");

	mutex_unlock(&ges_context_obj->ges_op_mutex);
	GESTURE_LOG(" ges_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t ges_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ges_context *cxt = NULL;
	int i;
	int s_len = 0;

	cxt = ges_context_obj;
	for (i = 0; i < GESTURE_MAX_SUPPORT; i++) {
		GESTURE_LOG("ges handle:%d active: %d\n", i, cxt->ctl_context[i].is_active_data);
		s_len += snprintf(buf + s_len, PAGE_SIZE, "id:%d, en:%d\n", i, cxt->ctl_context[i].is_active_data);
	}
	return s_len;
}

static ssize_t ges_store_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int len = 0;

	GESTURE_LOG(" not support now\n");
	return len;
}


static ssize_t ges_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GESTURE_LOG(" not support now\n");
	return len;
}


static ssize_t ges_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	GESTURE_LOG(" not support now\n");
	return len;
}

static ssize_t ges_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GESTURE_LOG(" not support now\n");
	return len;
}

static ssize_t ges_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int len = 0;

	GESTURE_LOG(" not support now\n");
	return len;
}

static ssize_t ges_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	GESTURE_LOG(" not support now\n");
	return len;
}

static ssize_t ges_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	const char *devname = NULL;

	devname = dev_name(&ges_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);	/* TODO: why +5? */
}


static int ges_real_driver_init(void)
{
	int err = 0, i = 0;

	GESTURE_LOG(" ges_real_driver_init +\n");

	for (i = 0; i < GESTURE_MAX_SUPPORT; i++) {
		if (NULL != gesture_init[i]) {
			GESTURE_LOG(" ges try to init driver %s\n", gesture_init[i]->name);
			err = gesture_init[i]->init();
			if (0 == err)
				GESTURE_LOG(" ges real driver %s probe ok\n", gesture_init[i]->name);
		} else
			continue;
	}

	wake_lock_init(&ges_lock, WAKE_LOCK_SUSPEND, "ges wakelock");
	init_timer(&ges_context_obj->notify_timer);
	ges_context_obj->notify_timer.expires = HZ / 5;	/* 200 ms */
	ges_context_obj->notify_timer.function = notify_ges_timeout;
	ges_context_obj->notify_timer.data = (unsigned long)ges_context_obj;

	return err;
}


int ges_driver_add(struct ges_init_info *obj, int handle)
{
	int err = 0;
	int index = -1;

	GESTURE_LOG("register gesture handle=%d\n", handle);

	if (!obj) {
		GESTURE_ERR("[%s] fail, ges_init_info is NULL\n", __func__);
		return -1;
	}

	index = HandleToIndex(handle);
	if (index < 0) {
		GESTURE_ERR("[%s] invalid index\n", __func__);
		return  -1;
	}

	if (NULL == gesture_init[index])
		gesture_init[index] = obj;

	return err;
}
EXPORT_SYMBOL_GPL(ges_driver_add);

static int ges_misc_init(struct ges_context *cxt)
{
	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = GES_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		GESTURE_ERR("unable to register ges misc device!!\n");

	return err;
}

static void ges_input_destroy(struct ges_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int ges_input_init(struct ges_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GES_INPUTDEV_NAME;
	/*set support event code*/
	input_set_capability(dev, EV_REL, EVENT_TYPE_INPK_VALUE);
	input_set_capability(dev, EV_REL, EVENT_TYPE_STATIONARY_VALUE);

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

DEVICE_ATTR(gesenablenodata, S_IWUSR | S_IRUGO, ges_show_enable_nodata, ges_store_enable_nodata);
DEVICE_ATTR(gesactive, S_IWUSR | S_IRUGO, ges_show_active, ges_store_active);
DEVICE_ATTR(gesdelay, S_IWUSR | S_IRUGO, ges_show_delay, ges_store_delay);
DEVICE_ATTR(gesbatch, S_IWUSR | S_IRUGO, ges_show_batch, ges_store_batch);
DEVICE_ATTR(gesflush, S_IWUSR | S_IRUGO, ges_show_flush, ges_store_flush);
DEVICE_ATTR(gesdevnum, S_IWUSR | S_IRUGO, ges_show_devnum, NULL);


static struct attribute *ges_attributes[] = {
	&dev_attr_gesenablenodata.attr,
	&dev_attr_gesactive.attr,
	&dev_attr_gesdelay.attr,
	&dev_attr_gesbatch.attr,
	&dev_attr_gesflush.attr,
	&dev_attr_gesdevnum.attr,
	NULL
};

static struct attribute_group ges_attribute_group = {
	.attrs = ges_attributes
};

int HandleToIndex(int handle)
{
	GESTURE_LOG("HandleToIndex(%d)\n", handle);
	switch (handle) {
	case ID_IN_POCKET:
		return inpocket;
	case ID_STATIONARY:
		return stationary;
	default:
		GESTURE_LOG("HandleToIndex invalid handle(%d)\n", handle);
	}

	return -1;
}

int ges_register_data_path(struct ges_data_path *data, int handle)
{
	struct ges_context *cxt = NULL;
	int index = -1;

	if (NULL == data || NULL == data->get_data) {
		GESTURE_LOG("ges register data path fail\n");
		return -1;
	}

	index = HandleToIndex(handle);
	if (index < 0) {
		GESTURE_ERR("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = ges_context_obj;
	cxt->ctl_context[index].ges_data.get_data = data->get_data;

	return 0;
}

int ges_register_control_path(struct ges_control_path *ctl, int handle)
{
	struct ges_context *cxt = NULL;
	int index = -1;

	GESTURE_FUN();
	if (NULL == ctl || NULL == ctl->open_report_data) {
		GESTURE_LOG("ges register control path fail\n");
		return -1;
	}

	index = HandleToIndex(handle);
	if (index < 0) {
		GESTURE_ERR("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = ges_context_obj;
	cxt->ctl_context[index].ges_ctl.open_report_data = ctl->open_report_data;

	return 0;
}

static int ges_probe(void)
{
	int err;

	GESTURE_LOG("+++++++++++++ges_probe!!\n");

	ges_context_obj = ges_context_alloc_object();
	if (!ges_context_obj) {
		err = -ENOMEM;
		GESTURE_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real ges driver */
	err = ges_real_driver_init();
	if (err) {
		GESTURE_ERR("ges real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* init input dev */
	err = ges_input_init(ges_context_obj);
	if (err) {
		GESTURE_ERR("unable to register ges input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	/* add misc dev for sensor hal control cmd */
	err = ges_misc_init(ges_context_obj);
	if (err) {
		GESTURE_ERR("unable to register ges misc device!!\n");
		goto exit_alloc_input_dev_failed;
	}
	err = sysfs_create_group(&ges_context_obj->mdev.this_device->kobj, &ges_attribute_group);
	if (err < 0) {
		GESTURE_ERR("unable to create ges attribute file\n");
		goto exit_alloc_input_dev_failed;
	}
	kobject_uevent(&ges_context_obj->mdev.this_device->kobj, KOBJ_ADD);


	GESTURE_LOG("----ges_probe OK !!\n");
	return 0;

	if (err) {
		GESTURE_ERR("sysfs node creation error\n");
		ges_input_destroy(ges_context_obj);
	}
real_driver_init_fail:
exit_alloc_input_dev_failed:
	kfree(ges_context_obj);
exit_alloc_data_failed:
	GESTURE_LOG("----ges_probe fail !!!\n");
	return err;
}

static int ges_remove(void)
{
	int err = 0;

	GESTURE_FUN();
	input_unregister_device(ges_context_obj->idev);
	sysfs_remove_group(&ges_context_obj->idev->dev.kobj, &ges_attribute_group);

	err = misc_deregister(&ges_context_obj->mdev);
	if (err)
		GESTURE_ERR("misc_deregister fail: %d\n", err);

	kfree(ges_context_obj);
	return 0;
}
static int __init ges_init(void)
{
	GESTURE_FUN();

	if (ges_probe()) {
		GESTURE_ERR("failed to register ges driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit ges_exit(void)
{
	ges_remove();
}

late_initcall(ges_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gesture sensor driver");
MODULE_AUTHOR("qiangming.xia@mediatek.com");

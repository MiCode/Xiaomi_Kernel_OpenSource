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

#include "inc/humidity.h"

struct hmdy_context *hmdy_context_obj = NULL;

static struct hmdy_init_info *humidity_init_list[MAX_CHOOSE_HMDY_NUM] = { 0 };

static void hmdy_work_func(struct work_struct *work)
{

	struct hmdy_context *cxt = NULL;
	int value = 0, status = 0;
	int64_t nt = 0;
	struct timespec time;
	int err = 0;

	cxt = hmdy_context_obj;

	if (NULL == cxt->hmdy_data.get_data)
		HMDY_LOG("hmdy driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->hmdy_data.get_data(&value, &status);

	if (err) {
		HMDY_ERR("get hmdy data fails!!\n");
		goto hmdy_loop;
	} else {
		{
			cxt->drv_data.hmdy_data.values[0] = value;
			cxt->drv_data.hmdy_data.status = status;
			cxt->drv_data.hmdy_data.time = nt;
		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		if (HMDY_INVALID_VALUE == cxt->drv_data.hmdy_data.values[0]) {
			HMDY_LOG(" read invalid data\n");
			goto hmdy_loop;
		}
	}


	hmdy_data_report(cxt->drv_data.hmdy_data.values[0], cxt->drv_data.hmdy_data.status);

 hmdy_loop:
	if (true == cxt->is_polling_run) {
		{
			mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
		}
	}
}

static void hmdy_poll(unsigned long data)
{
	struct hmdy_context *obj = (struct hmdy_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct hmdy_context *hmdy_context_alloc_object(void)
{

	struct hmdy_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	HMDY_LOG("hmdy_context_alloc_object++++\n");
	if (!obj) {
		HMDY_ERR("Alloc hmdy object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, hmdy_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = hmdy_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->hmdy_op_mutex);
	obj->is_batch_enable = false;

	HMDY_LOG("hmdy_context_alloc_object----\n");
	return obj;
}

static int hmdy_real_enable(int enable)
{
	int err = 0;
	struct hmdy_context *cxt = NULL;

	cxt = hmdy_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->hmdy_ctl.enable_nodata(1);
			if (err) {
				err = cxt->hmdy_ctl.enable_nodata(1);
				if (err) {
					err = cxt->hmdy_ctl.enable_nodata(1);
					if (err)
						HMDY_ERR("hmdy enable(%d) err 3 timers = %d\n", enable, err);
				}
			}
			HMDY_LOG("hmdy real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->hmdy_ctl.enable_nodata(0);
			if (err)
				HMDY_ERR("hmdy enable(%d) err = %d\n", enable, err);
			HMDY_LOG("hmdy real disable\n");
		}

	}

	return err;
}

static int hmdy_enable_data(int enable)
{
	struct hmdy_context *cxt = NULL;

	cxt = hmdy_context_obj;
	if (NULL == cxt->hmdy_ctl.open_report_data) {
		HMDY_ERR("no hmdy control path\n");
		return -1;
	}

	if (1 == enable) {
		HMDY_LOG("HMDY enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->hmdy_ctl.open_report_data(1);
		hmdy_real_enable(enable);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->hmdy_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		HMDY_LOG("HMDY disable\n");
		cxt->is_active_data = false;
		cxt->hmdy_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->hmdy_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.hmdy_data.values[0] = HMDY_INVALID_VALUE;
			}
		}
		hmdy_real_enable(enable);
	}
	return 0;
}

int hmdy_enable_nodata(int enable)
{
	struct hmdy_context *cxt = NULL;

	cxt = hmdy_context_obj;
	if (NULL == cxt->hmdy_ctl.enable_nodata) {
		HMDY_ERR("hmdy_enable_nodata:hmdy ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;
	hmdy_real_enable(enable);
	return 0;
}

static ssize_t hmdy_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	HMDY_LOG(" not support now\n");
	return len;
}

static ssize_t hmdy_store_enable_nodata(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct hmdy_context *cxt = NULL;

	HMDY_LOG("hmdy_store_enable nodata buf=%s\n", buf);
	mutex_lock(&hmdy_context_obj->hmdy_op_mutex);

	cxt = hmdy_context_obj;
	if (NULL == cxt->hmdy_ctl.enable_nodata) {
		HMDY_LOG("hmdy_ctl enable nodata NULL\n");
		mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		hmdy_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		hmdy_enable_nodata(0);
	else
		HMDY_ERR(" hmdy_store enable nodata cmd error !!\n");
	mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);

	return count;
}

static ssize_t hmdy_store_active(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct hmdy_context *cxt = NULL;

	HMDY_LOG("hmdy_store_active buf=%s\n", buf);
	mutex_lock(&hmdy_context_obj->hmdy_op_mutex);

	cxt = hmdy_context_obj;
	if (NULL == cxt->hmdy_ctl.open_report_data) {
		HMDY_LOG("hmdy_ctl enable NULL\n");
		mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		hmdy_enable_data(1);
	else if (!strncmp(buf, "0", 1))
		hmdy_enable_data(0);
	else
		HMDY_ERR(" hmdy_store_active error !!\n");
	mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
	HMDY_LOG(" hmdy_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t hmdy_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hmdy_context *cxt = NULL;
	int div = 0;

	cxt = hmdy_context_obj;
	div = cxt->hmdy_data.vender_div;

	HMDY_LOG("hmdy vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t hmdy_store_delay(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int delay = 0;
	int mdelay = 0;
	int err = 0;
	struct hmdy_context *cxt = NULL;

	mutex_lock(&hmdy_context_obj->hmdy_op_mutex);

	cxt = hmdy_context_obj;
	if (NULL == cxt->hmdy_ctl.set_delay) {
		HMDY_LOG("hmdy_ctl set_delay NULL\n");
		mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
		return count;
	}
	err = kstrtoint(buf, 10, &delay);
	if (err != 0) {
		HMDY_ERR("invalid format!!\n");
		mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
		return count;
	}

	if (false == cxt->hmdy_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&hmdy_context_obj->delay, mdelay);
	}
	cxt->hmdy_ctl.set_delay(delay);
	HMDY_LOG(" hmdy_delay %d ns\n", delay);
	mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
	return count;

}

static ssize_t hmdy_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	HMDY_LOG("not support now\n");
	return len;
}

static ssize_t hmdy_store_batch(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct hmdy_context *cxt = NULL;

	HMDY_LOG("hmdy_store_batch buf=%s\n", buf);
	mutex_lock(&hmdy_context_obj->hmdy_op_mutex);

	cxt = hmdy_context_obj;

	if (!strncmp(buf, "1", 1)) {
		cxt->is_batch_enable = true;
		if (true == cxt->is_polling_run) {
			cxt->is_polling_run = false;
			del_timer_sync(&cxt->timer);
			cancel_work_sync(&cxt->report);
			cxt->drv_data.hmdy_data.values[0] = HMDY_INVALID_VALUE;
			cxt->drv_data.hmdy_data.values[1] = HMDY_INVALID_VALUE;
			cxt->drv_data.hmdy_data.values[2] = HMDY_INVALID_VALUE;
		}
	} else if (!strncmp(buf, "0", 1)) {
		cxt->is_batch_enable = false;
		if (false == cxt->is_polling_run) {
			if (false == cxt->hmdy_ctl.is_report_input_direct)
				mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
		}
	} else {
		HMDY_ERR(" hmdy_store_batch error !!\n");
	}
	mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
	HMDY_LOG(" hmdy_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t hmdy_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t hmdy_store_flush(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mutex_lock(&hmdy_context_obj->hmdy_op_mutex);
	mutex_unlock(&hmdy_context_obj->hmdy_op_mutex);
	return count;
}

static ssize_t hmdy_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t hmdy_show_devnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int humidity_remove(struct platform_device *pdev)
{
	HMDY_LOG("humidity_remove\n");
	return 0;
}

static int humidity_probe(struct platform_device *pdev)
{
	HMDY_LOG("humidity_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id humidity_of_match[] = {
	{.compatible = "mediatek,humidity",},
	{},
};
#endif

static struct platform_driver humidity_driver = {
	.probe = humidity_probe,
	.remove = humidity_remove,
	.driver = {
		   .name = "humidity",
#ifdef CONFIG_OF
		   .of_match_table = humidity_of_match,
#endif
		   }
};

static int hmdy_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	HMDY_LOG(" hmdy_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_HMDY_NUM; i++) {
		HMDY_LOG(" i=%d\n", i);
		if (0 != humidity_init_list[i]) {
			HMDY_LOG(" hmdy try to init driver %s\n", humidity_init_list[i]->name);
			err = humidity_init_list[i]->init();
			if (0 == err) {
				HMDY_LOG(" hmdy real driver %s probe ok\n", humidity_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_HMDY_NUM) {
		HMDY_LOG(" hmdy_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

int hmdy_driver_add(struct hmdy_init_info *obj)
{
	int err = 0;
	int i = 0;

	HMDY_FUN();
	if (!obj) {
		HMDY_ERR("HMDY driver add fail, hmdy_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_HMDY_NUM; i++) {
		if (i == 0) {
			HMDY_LOG("register humidity driver for the first time\n");
			if (platform_driver_register(&humidity_driver))
				HMDY_ERR("failed to register gensor driver already exist\n");
		}

		if (NULL == humidity_init_list[i]) {
			obj->platform_diver_addr = &humidity_driver;
			humidity_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_HMDY_NUM) {
		HMDY_ERR("HMDY driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(hmdy_driver_add);
static int humidity_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t humidity_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(hmdy_context_obj->mdev.minor, file, buffer, count, ppos);

	return read_cnt;
}

static unsigned int humidity_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(hmdy_context_obj->mdev.minor, file, wait);
}

static const struct file_operations humidity_fops = {
	.owner = THIS_MODULE,
	.open = humidity_open,
	.read = humidity_read,
	.poll = humidity_poll,
};

static int hmdy_misc_init(struct hmdy_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = ID_RELATIVE_HUMIDITY;
	cxt->mdev.name = HMDY_MISC_DEV_NAME;
	cxt->mdev.fops = &humidity_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		HMDY_ERR("unable to register hmdy misc device!!\n");
	return err;
}

DEVICE_ATTR(hmdyenablenodata, S_IWUSR | S_IRUGO, hmdy_show_enable_nodata, hmdy_store_enable_nodata);
DEVICE_ATTR(hmdyactive, S_IWUSR | S_IRUGO, hmdy_show_active, hmdy_store_active);
DEVICE_ATTR(hmdydelay, S_IWUSR | S_IRUGO, hmdy_show_delay, hmdy_store_delay);
DEVICE_ATTR(hmdybatch, S_IWUSR | S_IRUGO, hmdy_show_batch, hmdy_store_batch);
DEVICE_ATTR(hmdyflush, S_IWUSR | S_IRUGO, hmdy_show_flush, hmdy_store_flush);
DEVICE_ATTR(hmdydevnum, S_IWUSR | S_IRUGO, hmdy_show_devnum, NULL);

static struct attribute *hmdy_attributes[] = {
	&dev_attr_hmdyenablenodata.attr,
	&dev_attr_hmdyactive.attr,
	&dev_attr_hmdydelay.attr,
	&dev_attr_hmdybatch.attr,
	&dev_attr_hmdyflush.attr,
	&dev_attr_hmdydevnum.attr,
	NULL
};

static struct attribute_group hmdy_attribute_group = {
	.attrs = hmdy_attributes
};

int hmdy_register_data_path(struct hmdy_data_path *data)
{
	struct hmdy_context *cxt = NULL;

	cxt = hmdy_context_obj;
	cxt->hmdy_data.get_data = data->get_data;
	cxt->hmdy_data.vender_div = data->vender_div;
	cxt->hmdy_data.get_raw_data = data->get_raw_data;
	HMDY_LOG("hmdy register data path vender_div: %d\n", cxt->hmdy_data.vender_div);
	if (NULL == cxt->hmdy_data.get_data) {
		HMDY_LOG("hmdy register data path fail\n");
		return -1;
	}
	return 0;
}

int hmdy_register_control_path(struct hmdy_control_path *ctl)
{
	struct hmdy_context *cxt = NULL;
	int err = 0;

	cxt = hmdy_context_obj;
	cxt->hmdy_ctl.set_delay = ctl->set_delay;
	cxt->hmdy_ctl.open_report_data = ctl->open_report_data;
	cxt->hmdy_ctl.enable_nodata = ctl->enable_nodata;
	cxt->hmdy_ctl.is_support_batch = ctl->is_support_batch;
	cxt->hmdy_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->hmdy_ctl.is_support_batch = ctl->is_support_batch;
	cxt->hmdy_ctl.is_use_common_factory = ctl->is_use_common_factory;

	if (NULL == cxt->hmdy_ctl.set_delay || NULL == cxt->hmdy_ctl.open_report_data
	    || NULL == cxt->hmdy_ctl.enable_nodata) {
		HMDY_LOG("hmdy register control path fail\n");
		return -1;
	}

	err = hmdy_misc_init(hmdy_context_obj);
	if (err) {
		HMDY_ERR("unable to register hmdy misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&hmdy_context_obj->mdev.this_device->kobj, &hmdy_attribute_group);
	if (err < 0) {
		HMDY_ERR("unable to create hmdy attribute file\n");
		return -3;
	}

	kobject_uevent(&hmdy_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int hmdy_data_report(int value, int status)
{
	struct sensor_event event;

	event.word[0] = value;
	event.word[1] = status;
	return sensor_input_event(hmdy_context_obj->mdev.minor, &event);
}

static int hmdy_probe(struct platform_device *pdev)
{

	int err = 0;

	HMDY_LOG("+++++++++++++hmdy_probe!!\n");

	hmdy_context_obj = hmdy_context_alloc_object();
	if (!hmdy_context_obj) {
		err = -ENOMEM;
		HMDY_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	err = hmdy_real_driver_init();
	if (err) {
		HMDY_ERR("hmdy real driver init fail\n");
		goto real_driver_init_fail;
	}
	err = hmdy_factory_device_init();
	if (err)
		HMDY_ERR("hmdy factory device already registed\n");

	HMDY_LOG("----hmdy_probe OK !!\n");
	return 0;

real_driver_init_fail:
	kfree(hmdy_context_obj);
	hmdy_context_obj = NULL;
exit_alloc_data_failed:

	HMDY_LOG("----hmdy_probe fail !!!\n");
	return err;
}

static int hmdy_remove(struct platform_device *pdev)
{
	int err = 0;

	HMDY_FUN(f);

	sysfs_remove_group(&hmdy_context_obj->mdev.this_device->kobj, &hmdy_attribute_group);
	err = sensor_attr_deregister(&hmdy_context_obj->mdev);
	if (err)
		HMDY_ERR("misc_deregister fail: %d\n", err);
	kfree(hmdy_context_obj);

	return 0;
}

static int hmdy_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int hmdy_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_hmdy_pl_of_match[] = {
	{.compatible = "mediatek,m_hmdy_pl",},
	{},
};
#endif

static struct platform_driver hmdy_driver = {
	.probe = hmdy_probe,
	.remove = hmdy_remove,
	.suspend = hmdy_suspend,
	.resume = hmdy_resume,
	.driver = {
		   .name = HMDY_PL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = m_hmdy_pl_of_match,
#endif
		   }
};

static int __init hmdy_init(void)
{
	HMDY_FUN();

	if (platform_driver_register(&hmdy_driver)) {
		HMDY_ERR("failed to register hmdy driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit hmdy_exit(void)
{
	platform_driver_unregister(&hmdy_driver);
	platform_driver_unregister(&humidity_driver);
}

late_initcall(hmdy_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HMDYMETER device driver");
MODULE_AUTHOR("Mediatek");

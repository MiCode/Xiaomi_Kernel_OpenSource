// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#define pr_fmt(fmt) "<MAG> " fmt

#include "inc/mag.h"
#include "sensor_performance.h"
#include <linux/vmalloc.h>

struct mag_context *mag_context_obj /* = NULL*/;
static struct mag_init_info *msensor_init_list[MAX_CHOOSE_G_NUM] = {0};

static void initTimer(struct hrtimer *timer,
		      enum hrtimer_restart (*callback)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = callback;
}

static void startTimer(struct hrtimer *timer, int delay_ms, bool first)
{
	struct mag_context *obj = (struct mag_context *)container_of(timer,
		struct mag_context, hrTimer);

	if (obj == NULL) {
		pr_err("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime =
			ktime_add_ns(ktime_get(), (int64_t)delay_ms * 1000000);
	} else {
		do {
			obj->target_ktime = ktime_add_ns(
				obj->target_ktime, (int64_t)delay_ms * 1000000);
		} while (ktime_to_ns(obj->target_ktime) <
			 ktime_to_ns(ktime_get()));
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}
#ifndef CONFIG_NANOHUB
static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}
#endif
static void mag_work_func(struct work_struct *work)
{
	struct mag_context *cxt = NULL;
	struct hwm_sensor_data sensor_data;
	int64_t m_pre_ns, cur_ns;
	int64_t delay_ms;
	struct timespec time;
	int err;
	int x, y, z, status;

	cxt = mag_context_obj;
	delay_ms = atomic_read(&cxt->delay);
	memset(&sensor_data, 0, sizeof(sensor_data));
	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	cur_ns = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->mag_dev_data.get_data(&x, &y, &z, &status);
	if (err) {
		pr_err("get data fails!!\n");
		return;
	}
	cxt->drv_data.x = x;
	cxt->drv_data.y = y;
	cxt->drv_data.z = z;
	cxt->drv_data.status = status;
	m_pre_ns = cxt->drv_data.timestamp;
	cxt->drv_data.timestamp = cur_ns;
	if (true == cxt->is_first_data_after_enable) {
		m_pre_ns = cur_ns;
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.x == MAG_INVALID_VALUE ||
		    cxt->drv_data.y == MAG_INVALID_VALUE ||
		    cxt->drv_data.z == MAG_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto mag_loop;
		}
	}
	while ((cur_ns - m_pre_ns) >= delay_ms * 1800000LL) {
		struct mag_data tmp_data = cxt->drv_data;

		m_pre_ns += delay_ms * 1000000LL;
		tmp_data.timestamp = m_pre_ns;
		mag_data_report(&tmp_data);
	}

	mag_data_report(&cxt->drv_data);

mag_loop:

	if (true == cxt->is_polling_run)
		startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), false);
}

enum hrtimer_restart mag_poll(struct hrtimer *timer)
{
	struct mag_context *obj = (struct mag_context *)container_of(timer,
		struct mag_context, hrTimer);

	queue_work(obj->mag_workqueue, &obj->report);

	return HRTIMER_NORESTART;
}

static struct mag_context *mag_context_alloc_object(void)
{

	struct mag_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc magel object error!\n");
		return NULL;
	}

	atomic_set(&obj->delay, 200); /* set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, mag_work_func);
	obj->mag_workqueue = NULL;
	obj->mag_workqueue = create_workqueue("mag_polling");
	if (!obj->mag_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, mag_poll);
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	mutex_init(&obj->mag_op_mutex);
	obj->power = 0;
	obj->enable = 0;
	obj->delay_ns = -1;
	obj->latency_ns = -1;
	pr_debug("%s end\n", __func__);
	return obj;
}

#ifndef CONFIG_NANOHUB
static int mag_enable_and_batch(void)
{
	struct mag_context *cxt = mag_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->power == 1 && cxt->enable == 0) {
		pr_debug("MAG disable\n");
		/* stop polling firstly, if needed */
		if (cxt->mag_ctl.is_report_input_direct == false &&
		    cxt->is_polling_run == true) {
			smp_mb(); /* for memory barrier */
			stopTimer(&cxt->hrTimer);
			smp_mb(); /* for memory barrier */
			cancel_work_sync(&cxt->report);
			cxt->drv_data.x = MAG_INVALID_VALUE;
			cxt->drv_data.y = MAG_INVALID_VALUE;
			cxt->drv_data.z = MAG_INVALID_VALUE;
			cxt->is_polling_run = false;
			pr_debug("mag stop polling done\n");
		}
		/* turn off the power */
		err = cxt->mag_ctl.enable(0);
		if (err) {
			pr_err("mag turn off power err = %d\n", err);
			return -1;
		}
		pr_debug("mag turn off power done\n");

		cxt->power = 0;
		cxt->delay_ns = -1;
		pr_debug("MAG disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->power == 0 && cxt->enable == 1) {
		pr_debug("MAG power on\n");
		err = cxt->mag_ctl.enable(1);
		if (err) {
			pr_err("mag turn on power err = %d\n", err);
			return -1;
		}
		pr_debug("mag turn on power done\n");

		cxt->power = 1;
		pr_debug("MAG power on done\n");
	}
	/* rate change */
	if (cxt->power == 1 && cxt->delay_ns >= 0) {
		pr_debug("MAG set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->mag_ctl.is_support_batch)
			err = cxt->mag_ctl.batch(0, cxt->delay_ns,
						 cxt->latency_ns);
		else
			err = cxt->mag_ctl.batch(0, cxt->delay_ns, 0);
		if (err) {
			pr_err("mag set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("mag set ODR, fifo latency done\n");
		/* start polling, if needed */
		if (cxt->mag_ctl.is_report_input_direct == false) {
			uint64_t mdelay = cxt->delay_ns;

			do_div(mdelay, 1000000);
			atomic_set(&cxt->delay, mdelay);
			/* the first sensor start polling timer */
			if (cxt->is_polling_run == false) {
				cxt->is_polling_run = true;
				cxt->is_first_data_after_enable = true;
				startTimer(&cxt->hrTimer,
					   atomic_read(&cxt->delay), true);
			}
			pr_debug("mag set polling delay %d ms\n",
				atomic_read(&cxt->delay));
		}
		pr_debug("MAG batch done\n");
	}
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static ssize_t magdev_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug("sensor test: mag function!\n");
	return len;
}
static ssize_t magactive_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct mag_context *cxt = mag_context_obj;
	int err = 0;

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);

	if (!strncmp(buf, "1", 1))
		cxt->enable = 1;
	else if (!strncmp(buf, "0", 1))
		cxt->enable = 0;
	else {
		pr_err("%s error !!\n", __func__);
		err = -1;
		goto err_out;
	}
#ifdef CONFIG_NANOHUB
	err = cxt->mag_ctl.enable(cxt->enable);
	if (err) {
		pr_err("mag turn on power err = %d\n", err);
		goto err_out;
	}
#else
	err = mag_enable_and_batch();
#endif

err_out:
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	pr_debug("%s done\n", __func__);
	if (err)
		return err;
	else
		return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t magactive_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct mag_context *cxt = NULL;
	int div = 0;

	cxt = mag_context_obj;
	div = cxt->mag_dev_data.div;
	pr_debug("mag mag_dev_data m_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t magbatch_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct mag_context *cxt = mag_context_obj;
	int handle = 0, flag = 0, err = 0;

	pr_debug("%s %s\n", __func__, buf);
	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->delay_ns,
		     &cxt->latency_ns);
	if (err != 4) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return -1;
	}

	mutex_lock(&mag_context_obj->mag_op_mutex);
#ifdef CONFIG_NANOHUB
	if (cxt->mag_ctl.is_support_batch)
		err = cxt->mag_ctl.batch(0, cxt->delay_ns, cxt->latency_ns);
	else
		err = cxt->mag_ctl.batch(0, cxt->delay_ns, 0);
	if (err)
		pr_err("mag set batch(ODR) err %d\n", err);
#else
	err = mag_enable_and_batch();
#endif
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	pr_debug("%s done: %d\n", __func__, cxt->is_batch_enable);
	if (err)
		return err;
	else
		return count;
}

static ssize_t magbatch_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t magflush_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t magflush_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct mag_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (cxt->mag_ctl.flush != NULL)
		err = cxt->mag_ctl.flush();
	else
		pr_debug(
			"MAG DRIVER OLD ARCHITECTURE DON'T SUPPORT ACC COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_err("mag enable flush err %d\n", err);
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t magcali_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t magcali_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;
	int err = 0;
	uint8_t *cali_buf = NULL;

	cali_buf = vzalloc(count);
	if (cali_buf == NULL)
		return -EFAULT;
	memcpy(cali_buf, buf, count);

	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if (cxt->mag_ctl.set_cali != NULL)
		err = cxt->mag_ctl.set_cali(cali_buf, count);
	else
		pr_debug(
			"MAG DRIVER OLD ARCHITECTURE DON'T SUPPORT MAG COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_err("mag set cali err %d\n", err);
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	vfree(cali_buf);
	return count;
}

/* need work around again */
static ssize_t magdevnum_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t maglibinfo_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mag_context *cxt = mag_context_obj;

	if (!buf)
		return -1;
	memcpy(buf, &cxt->mag_ctl.libinfo, sizeof(struct mag_libinfo_t));
	return sizeof(struct mag_libinfo_t);
}

static int msensor_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int msensor_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id msensor_of_match[] = {
	{
		.compatible = "mediatek,msensor",
	},
	{},
};
#endif

static struct platform_driver msensor_driver = {
	.probe = msensor_probe,
	.remove = msensor_remove,
	.driver = {

		.name = "msensor",
#ifdef CONFIG_OF
		.of_match_table = msensor_of_match,
#endif
	}
};

static int mag_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		pr_debug(" i=%d\n", i);
		if (msensor_init_list[i] != 0) {
			pr_debug(" mag try to init driver %s\n",
				msensor_init_list[i]->name);
			err = msensor_init_list[i]->init();
			if (err == 0) {
				pr_debug(" mag real driver %s probe ok\n",
					msensor_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_G_NUM) {
		pr_debug("%s fail\n", __func__);
		err = -1;
	}
	return err;
}

int mag_driver_add(struct mag_init_info *obj)
{
	int err = 0;
	int i = 0;

	pr_debug("%s\n", __func__);
	if (!obj) {
		pr_err("%s fail, mag_init_info is NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		if ((i == 0) && (msensor_init_list[0] == NULL)) {
			pr_debug("register mensor driver for the first time\n");
			if (platform_driver_register(&msensor_driver))
				pr_err(
					"failed to register msensor driver already exist\n");
		}
		if (msensor_init_list[i] == NULL) {
			obj->platform_diver_addr = &msensor_driver;
			msensor_init_list[i] = obj;
			break;
		}
	}

	if (i >= MAX_CHOOSE_G_NUM) {
		pr_err("MAG driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(mag_driver_add);
static int magnetic_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t magnetic_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(mag_context_obj->mdev.minor, file, buffer,
				     count, ppos);

	return read_cnt;
}

static unsigned int magnetic_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(mag_context_obj->mdev.minor, file, wait);
}

static const struct file_operations mag_fops = {
	.owner = THIS_MODULE,
	.open = magnetic_open,
	.read = magnetic_read,
	.poll = magnetic_poll,
};

static int mag_misc_init(struct mag_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = ID_MAGNETIC;
	cxt->mdev.name = MAG_MISC_DEV_NAME;
	cxt->mdev.fops = &mag_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register mag misc device!!\n");

	return err;
}

DEVICE_ATTR_RO(magdev);
DEVICE_ATTR_RW(magactive);
DEVICE_ATTR_RW(magbatch);
DEVICE_ATTR_RW(magflush);
DEVICE_ATTR_RW(magcali);
DEVICE_ATTR_RO(magdevnum);
DEVICE_ATTR_RO(maglibinfo);

static struct attribute *mag_attributes[] = {
	&dev_attr_magdev.attr,
	&dev_attr_magactive.attr,
	&dev_attr_magbatch.attr,
	&dev_attr_magflush.attr,
	&dev_attr_magcali.attr,
	&dev_attr_magdevnum.attr,
	&dev_attr_maglibinfo.attr,
	NULL
};

static struct attribute_group mag_attribute_group = {
	.attrs = mag_attributes
};

int mag_register_data_path(struct mag_data_path *data)
{
	struct mag_context *cxt = NULL;

	cxt = mag_context_obj;
	cxt->mag_dev_data.div = data->div;
	cxt->mag_dev_data.get_data = data->get_data;
	cxt->mag_dev_data.get_raw_data = data->get_raw_data;
	pr_debug("mag register data path div: %d\n", cxt->mag_dev_data.div);

	return 0;
}
EXPORT_SYMBOL_GPL(mag_register_data_path);

int mag_register_control_path(struct mag_control_path *ctl)
{
	struct mag_context *cxt = NULL;
	int err = 0;

	cxt = mag_context_obj;
	cxt->mag_ctl.set_delay = ctl->set_delay;
	cxt->mag_ctl.enable = ctl->enable;
	cxt->mag_ctl.open_report_data = ctl->open_report_data;
	cxt->mag_ctl.batch = ctl->batch;
	cxt->mag_ctl.flush = ctl->flush;
	cxt->mag_ctl.set_cali = ctl->set_cali;
	cxt->mag_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->mag_ctl.is_support_batch = ctl->is_support_batch;
	cxt->mag_ctl.is_use_common_factory = ctl->is_use_common_factory;
	memcpy(cxt->mag_ctl.libinfo.libname, ctl->libinfo.libname,
	       sizeof(cxt->mag_ctl.libinfo.libname));
	cxt->mag_ctl.libinfo.layout = ctl->libinfo.layout;
	cxt->mag_ctl.libinfo.deviceid = ctl->libinfo.deviceid;

	if (cxt->mag_ctl.set_delay == NULL || cxt->mag_ctl.enable == NULL ||
	    cxt->mag_ctl.open_report_data == NULL) {
		pr_debug("mag register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = mag_misc_init(mag_context_obj);
	if (err) {
		pr_err("unable to register mag misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&mag_context_obj->mdev.this_device->kobj,
				 &mag_attribute_group);
	if (err < 0) {
		pr_err("unable to create mag attribute file\n");
		return -3;
	}

	kobject_uevent(&mag_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}
EXPORT_SYMBOL_GPL(mag_register_control_path);

static int x1, y1, z1;
static long pc;

static int check_repeat_data(int x, int y, int z)
{
	if ((x1 == x) && (y1 == y) && (z1 == z))
		pc++;
	else
		pc = 0;

	x1 = x;
	y1 = y;
	z1 = z;

	if (pc > 100) {
		pr_debug("Mag sensor output repeat data\n");
		pc = 0;
	}

	return 0;
}

int mag_data_report(struct mag_data *data)
{
	/* pr_debug("update!valus: %d, %d, %d, %d\n" , x, y, z, status); */
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	check_repeat_data(data->x, data->y, data->z);
	event.flush_action = DATA_ACTION;
	event.status = data->status;
	event.time_stamp = data->timestamp;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;
	event.word[3] = data->reserved[0];
	event.word[4] = data->reserved[1];
	event.word[5] = data->reserved[2];
	event.reserved = data->reserved[0];

	if (event.reserved == 1)
		mark_timestamp(ID_MAGNETIC, DATA_REPORT, ktime_get_boot_ns(),
			       event.time_stamp);
	err = sensor_input_event(mag_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(mag_data_report);

int mag_bias_report(struct mag_data *data)
{
	/* pr_debug("update!valus: %d, %d, %d, %d\n" , x, y, z, status); */
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = BIAS_ACTION;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;

	err = sensor_input_event(mag_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(mag_bias_report);

int mag_cali_report(int32_t *param)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = CALI_ACTION;
	event.word[0] = param[0];
	event.word[1] = param[1];
	event.word[2] = param[2];
	event.word[3] = param[3];
	event.word[4] = param[4];
	event.word[5] = param[5];

	err = sensor_input_event(mag_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(mag_cali_report);

int mag_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	pr_debug_ratelimited("flush\n");
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(mag_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(mag_flush_report);

int mag_info_record(struct mag_libinfo_t *p_mag_info)
{
	struct mag_context *cxt = NULL;
	int err = 0;

	cxt = mag_context_obj;

	memcpy(cxt->mag_ctl.libinfo.libname,
		p_mag_info->libname, sizeof(cxt->mag_ctl.libinfo.libname));
	cxt->mag_ctl.libinfo.layout = p_mag_info->layout;
	cxt->mag_ctl.libinfo.deviceid = p_mag_info->deviceid;

	return err;
}
EXPORT_SYMBOL_GPL(mag_info_record);

int mag_probe(void)
{
	int err;

	pr_debug("%s ++++!!\n", __func__);
	mag_context_obj = mag_context_alloc_object();
	if (!mag_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real mageleration driver */
	err = mag_real_driver_init();
	if (err) {
		pr_err("mag_real_driver_init fail\n");
		goto real_driver_init_fail;
	}

	pr_debug("%s OK !!\n", __func__);
	return 0;

real_driver_init_fail:
	kfree(mag_context_obj);

exit_alloc_data_failed:

	pr_err("%s fail !!!\n", __func__);
	return err;
}
EXPORT_SYMBOL_GPL(mag_probe);

int mag_remove(void)
{
	int err = 0;

	pr_debug("%s\n", __func__);
	sysfs_remove_group(&mag_context_obj->mdev.this_device->kobj,
			   &mag_attribute_group);

	err = sensor_attr_deregister(&mag_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);

	kfree(mag_context_obj);
	platform_driver_unregister(&msensor_driver);

	return 0;
}
EXPORT_SYMBOL_GPL(mag_remove);

static int __init mag_init(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static void __exit mag_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(mag_init);
module_exit(mag_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAGELEROMETER device driver");
MODULE_AUTHOR("Mediatek");

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

#define pr_fmt(fmt) "<ACTIVITY> " fmt

#include "activity.h"

struct act_context *act_context_obj /* = NULL*/;

static struct act_init_info *activity_init_list[MAX_CHOOSE_ACT_NUM] = {0};

static void act_work_func(struct work_struct *work)
{

	struct act_context *cxt = NULL;
	/* int out_size; */
	struct hwm_sensor_data sensor_data;
	/* u64 data64[6]; //for unify get_data parameter type */
	/* u16 data32[6]; //for hwm_sensor_data.values as int */
	int status;
	int64_t nt;
	struct timespec time;
	int err = 0;
	static int64_t last_time_stamp;

	cxt = act_context_obj;
	if (cxt->act_data.get_data == NULL)
		pr_err("act driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	/* add wake lock to make sure data can be read before system suspend */
	/* initial data */

	err = cxt->act_data.get_data(&sensor_data, &status);

	if (err) {
		pr_err("get act data fails!!\n");
		goto act_loop;
	} else {
		cxt->drv_data.probability[0] = sensor_data.probability[0];
		cxt->drv_data.probability[1] = sensor_data.probability[1];
		cxt->drv_data.probability[2] = sensor_data.probability[2];
		cxt->drv_data.probability[3] = sensor_data.probability[3];
		cxt->drv_data.probability[4] = sensor_data.probability[4];
		cxt->drv_data.probability[5] = sensor_data.probability[5];
		cxt->drv_data.probability[6] = sensor_data.probability[6];
		cxt->drv_data.probability[7] = sensor_data.probability[7];
		cxt->drv_data.probability[8] = sensor_data.probability[8];
		cxt->drv_data.probability[9] = sensor_data.probability[9];
		cxt->drv_data.probability[10] = sensor_data.probability[10];
		cxt->drv_data.probability[11] = sensor_data.probability[11];
		cxt->drv_data.status = status;
		cxt->drv_data.time = sensor_data.time;
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.probability[0] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[1] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[2] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[3] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[4] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[5] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[6] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[7] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[8] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[9] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[10] == ACT_INVALID_VALUE ||
		    cxt->drv_data.probability[11] == ACT_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto act_loop;
		}
	}
	/* report data to input devic */
	/* pr_debug("act data[%d,%d,%d]\n"
	 * ,cxt->drv_data.act_data.probability[0],
	 */
	/*cxt->drv_data.act_data.probability[1],
	 *cxt->drv_data.act_data.probability[2]);
	 */
	if (last_time_stamp != cxt->drv_data.time) {
		last_time_stamp = cxt->drv_data.time;
		act_data_report(&cxt->drv_data, cxt->drv_data.status);
	}

act_loop:
	if (true == cxt->is_polling_run) {
		mod_timer(&cxt->timer, jiffies +
				  atomic_read(&cxt->delay) /
					  (1000 / HZ));
	}
}

static void act_poll(unsigned long data)
{
	struct act_context *obj = (struct act_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct act_context *act_context_alloc_object(void)
{
	struct act_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("act_context_alloc_object++++\n");
	if (!obj) {
		pr_err("Alloc act object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200); /*5Hz set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, act_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = act_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	obj->power = 0;
	obj->enable = 0;
	obj->delay_ns = -1;
	obj->latency_ns = -1;
	mutex_init(&obj->act_op_mutex);
	pr_debug("act_context_alloc_object----\n");
	return obj;
}
#ifndef CONFIG_NANOHUB
static int act_enable_and_batch(void)
{
	struct act_context *cxt = act_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->power == 1 && cxt->enable == 0) {
		pr_debug("ACT disable\n");
		/* stop polling firstly, if needed */
		if (cxt->act_ctl.is_report_input_direct == false &&
		    cxt->is_polling_run == true) {
			smp_mb(); /* for memory barrier */
			del_timer_sync(&cxt->timer);
			smp_mb(); /* for memory barrier */
			cancel_work_sync(&cxt->report);
			cxt->drv_data.probability[0] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[1] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[2] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[3] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[4] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[5] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[6] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[7] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[8] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[9] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[10] = ACT_INVALID_VALUE;
			cxt->drv_data.probability[11] = ACT_INVALID_VALUE;
			cxt->is_polling_run = false;
			pr_debug("act stop polling done\n");
		}
		/* turn off the power */
		err = cxt->act_ctl.enable_nodata(0);
		if (err) {
			pr_err("act turn off power err = %d\n", err);
			return -1;
		}
		pr_debug("act turn off power done\n");

		cxt->power = 0;
		cxt->delay_ns = -1;
		pr_debug("ACT disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->power == 0 && cxt->enable == 1) {
		pr_debug("ACT power on\n");
		err = cxt->act_ctl.enable_nodata(1);
		if (err) {
			pr_err("act turn on power err = %d\n", err);
			return -1;
		}
		pr_debug("act turn on power done\n");

		cxt->power = 1;
		pr_debug("ACT power on done\n");
	}
	/* rate change */
	if (cxt->power == 1 && cxt->delay_ns >= 0) {
		pr_debug("ACT set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->act_ctl.is_support_batch)
			err = cxt->act_ctl.batch(0, cxt->delay_ns,
						 cxt->latency_ns);
		else
			err = cxt->act_ctl.batch(0, cxt->delay_ns, 0);
		if (err) {
			pr_err("act set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("act set ODR, fifo latency done\n");
		/* start polling, if needed */
		if (cxt->act_ctl.is_report_input_direct == false) {
			int mdelay = cxt->delay_ns;

			do_div(mdelay, 1000000);
			atomic_set(&cxt->delay, mdelay);
			/* the first sensor start polling timer */
			if (cxt->is_polling_run == false) {
				mod_timer(&cxt->timer,
					  jiffies +
						  atomic_read(&cxt->delay) /
							  (1000 / HZ));
				cxt->is_polling_run = true;
				cxt->is_first_data_after_enable = true;
			}
			pr_debug("act set polling delay %d ms\n",
				atomic_read(&cxt->delay));
		}
		pr_debug("ACT batch done\n");
	}
	return 0;
}
#endif
static ssize_t act_store_active(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct act_context *cxt = act_context_obj;
	int err = -1;

	pr_debug("act_store_active buf=%s\n", buf);
	mutex_lock(&act_context_obj->act_op_mutex);
	if (!strncmp(buf, "1", 1))
		cxt->enable = 1;
	else if (!strncmp(buf, "0", 1))
		cxt->enable = 0;
	else {
		pr_err(" act_store_active error !!\n");
		err = -1;
		goto err_out;
	}
#ifdef CONFIG_NANOHUB
	err = cxt->act_ctl.enable_nodata(cxt->enable);
	if (err) {
		pr_err("act turn on power err = %d\n", err);
		goto err_out;
	}
#else
	err = act_enable_and_batch();
#endif
	pr_debug(" act_store_active done\n");
err_out:
	mutex_unlock(&act_context_obj->act_op_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/
static ssize_t act_show_active(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct act_context *cxt = NULL;
	int div = 0;

	cxt = act_context_obj;
	/* int len = 0; */
	pr_debug("act show active not support now\n");
	/* div=cxt->act_data.vender_div; */
	pr_debug("act show active not support now,vender_div value: %d\n",
		div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);

	/* return len; */
}

static ssize_t act_store_batch(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct act_context *cxt = act_context_obj;
	int handle = 0, flag = 0, err = 0;

	pr_debug(" act_store_batch %s\n", buf);
	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->delay_ns,
		     &cxt->latency_ns);
	if (err != 4) {
		pr_err("act_store_batch param error: err = %d\n", err);
		return -1;
	}
	mutex_lock(&act_context_obj->act_op_mutex);
#ifdef CONFIG_NANOHUB
	if (cxt->act_ctl.is_support_batch)
		err = cxt->act_ctl.batch(0, cxt->delay_ns, cxt->latency_ns);
	else
		err = cxt->act_ctl.batch(0, cxt->delay_ns, 0);
	if (err)
		pr_err("act set batch(ODR) err %d\n", err);
#else
	err = act_enable_and_batch();
#endif

	mutex_unlock(&act_context_obj->act_op_mutex);
	return err;
}

static ssize_t act_show_batch(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t act_store_flush(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct act_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("act_store_flush param error: err = %d\n", err);

	pr_debug("act_store_flush param: handle %d\n", handle);

	mutex_lock(&act_context_obj->act_op_mutex);
	cxt = act_context_obj;
	if (cxt->act_ctl.flush != NULL)
		err = cxt->act_ctl.flush();
	else
		pr_err(
			"ACT DRIVER OLD ARCHITECTURE DON'T SUPPORT ACT COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_err("act enable flush err %d\n", err);
	mutex_unlock(&act_context_obj->act_op_mutex);
	return err;
}

static ssize_t act_show_flush(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t act_show_devnum(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int activity_remove(struct platform_device *pdev)
{
	pr_debug("activity_remove\n");
	return 0;
}

static int activity_probe(struct platform_device *pdev)
{
	pr_debug("activity_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id activity_of_match[] = {
	{
		.compatible = "mediatek,activity",
	},
	{},
};
#endif

static struct platform_driver activity_driver = {
	.probe = activity_probe,
	.remove = activity_remove,
	.driver = {
		.name = "activity",
#ifdef CONFIG_OF
		.of_match_table = activity_of_match,
#endif
	} };

static int act_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	pr_debug(" act_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_ACT_NUM; i++) {
		pr_debug(" i=%d\n", i);
		if (activity_init_list[i] != 0) {
			pr_debug(" act try to init driver %s\n",
				activity_init_list[i]->name);
			err = activity_init_list[i]->init();
			if (err == 0) {
				pr_debug(" act real driver %s probe ok\n",
					activity_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_ACT_NUM) {
		pr_debug(" act_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

int act_driver_add(struct act_init_info *obj)
{
	int err = 0;
	int i = 0;

	pr_debug("%s\n", __func__);

	for (i = 0; i < MAX_CHOOSE_ACT_NUM; i++) {
		if (i == 0) {
			pr_debug("register act driver for the first time\n");
			err = platform_driver_register(&activity_driver);
			if (err)
				pr_err(
					"failed to register act driver already exist\n");
		}

		if (activity_init_list[i] == NULL) {
			obj->platform_diver_addr = &activity_driver;
			activity_init_list[i] = obj;
			break;
		}
	}
	if (activity_init_list[i] == NULL) {
		pr_err("act driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(act_driver_add);
static int activity_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t activity_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(act_context_obj->mdev.minor, file, buffer,
				     count, ppos);

	return read_cnt;
}

static unsigned int activity_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(act_context_obj->mdev.minor, file, wait);
}

static const struct file_operations activity_fops = {
	.owner = THIS_MODULE,
	.open = activity_open,
	.read = activity_read,
	.poll = activity_poll,
};

static int act_misc_init(struct act_context *cxt)
{

	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = ID_ACTIVITY;
	cxt->mdev.name = ACT_MISC_DEV_NAME;
	cxt->mdev.fops = &activity_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register act misc device!!\n");

	return err;
}

DEVICE_ATTR(actactive, 0644, act_show_active, act_store_active);
DEVICE_ATTR(actbatch, 0644, act_show_batch, act_store_batch);
DEVICE_ATTR(actflush, 0644, act_show_flush, act_store_flush);
DEVICE_ATTR(actdevnum, 0644, act_show_devnum, NULL);

static struct attribute *act_attributes[] = {
	&dev_attr_actactive.attr, &dev_attr_actbatch.attr,
	&dev_attr_actflush.attr, &dev_attr_actdevnum.attr, NULL};

static struct attribute_group act_attribute_group = {.attrs = act_attributes};

int act_register_data_path(struct act_data_path *data)
{
	struct act_context *cxt = NULL;
	/* int err =0; */
	cxt = act_context_obj;
	cxt->act_data.get_data = data->get_data;
	/* cxt->act_data.vender_div = data->vender_div; */
	/* cxt->act_data.get_raw_data = data->get_raw_data; */
	/* pr_debug("act register data path vender_div: %d\n",
	 * cxt->act_data.vender_div);
	 */
	if (cxt->act_data.get_data == NULL) {
		pr_debug("act register data path fail\n");
		return -1;
	}
	return 0;
}

int act_register_control_path(struct act_control_path *ctl)
{
	struct act_context *cxt = NULL;
	int err = 0;

	cxt = act_context_obj;
	cxt->act_ctl.set_delay = ctl->set_delay;
	cxt->act_ctl.open_report_data = ctl->open_report_data;
	cxt->act_ctl.enable_nodata = ctl->enable_nodata;
	cxt->act_ctl.batch = ctl->batch;
	cxt->act_ctl.flush = ctl->flush;
	cxt->act_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->act_ctl.is_support_batch = ctl->is_support_batch;

	if (cxt->act_ctl.set_delay == NULL ||
	    cxt->act_ctl.open_report_data == NULL ||
	    cxt->act_ctl.enable_nodata == NULL) {
		pr_debug("act register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = act_misc_init(act_context_obj);
	if (err) {
		pr_err("unable to register act misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&act_context_obj->mdev.this_device->kobj,
				 &act_attribute_group);
	if (err < 0) {
		pr_err("unable to create act attribute file\n");
		return -3;
	}

	kobject_uevent(&act_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int act_data_report(struct hwm_sensor_data *data, int status)
{
	struct sensor_event event;
	int err = 0;

	event.flush_action = DATA_ACTION;
	event.time_stamp = data->time;
	event.byte[0] = data->probability[0];
	event.byte[1] = data->probability[1];
	event.byte[2] = data->probability[2];
	event.byte[3] = data->probability[3];
	event.byte[4] = data->probability[4];
	event.byte[5] = data->probability[5];
	event.byte[6] = data->probability[6];
	event.byte[7] = data->probability[7];
	event.byte[8] = data->probability[8];
	event.byte[9] = data->probability[9];
	event.byte[10] = data->probability[10];
	event.byte[11] = data->probability[11];

	err = sensor_input_event(act_context_obj->mdev.minor, &event);
	if (err < 0)
		pr_err_ratelimited("failed due to event buffer full\n");
	return err;
}

int act_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	pr_debug("flush\n");
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(act_context_obj->mdev.minor, &event);
	if (err < 0)
		pr_err_ratelimited("failed due to event buffer full\n");
	return err;
}

static int act_probe(void)
{
	int err;

	pr_debug("+++++++++++++act_probe!!\n");

	act_context_obj = act_context_alloc_object();
	if (!act_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real acteleration driver */
	err = act_real_driver_init();
	if (err) {
		pr_err("act real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* err = act_factory_device_init(); */
	/* if(err) */
	/* { */
	/* pr_err("act_factory_device_init fail\n"); */
	/* } */

	pr_debug("----act_probe OK !!\n");
	return 0;

real_driver_init_fail:
	kfree(act_context_obj);

exit_alloc_data_failed:

	pr_debug("----act_probe fail !!!\n");
	return err;
}

static int act_remove(void)
{
	int err = 0;

	pr_debug("%s\n", __func__);
	sysfs_remove_group(&act_context_obj->mdev.this_device->kobj,
			   &act_attribute_group);

	err = sensor_attr_deregister(&act_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);
	kfree(act_context_obj);

	return 0;
}
static int __init act_init(void)
{
	pr_debug("%s\n", __func__);

	if (act_probe()) {
		pr_err("failed to register act driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit act_exit(void)
{
	act_remove();
	platform_driver_unregister(&activity_driver);
}
late_initcall(act_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACTIVITY device driver");
MODULE_AUTHOR("Mediatek");

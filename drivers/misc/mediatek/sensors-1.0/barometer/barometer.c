// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "Barometer " fmt

#include "inc/barometer.h"

struct baro_context *baro_context_obj /* = NULL*/;

static void initTimer(struct hrtimer *timer,
		      enum hrtimer_restart (*callback)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = callback;
}

static void startTimer(struct hrtimer *timer, int delay_ms, bool first)
{
	struct baro_context *obj = (struct baro_context *)container_of(timer,
		struct baro_context, hrTimer);

	if (obj == NULL) {
		pr_err("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime =
			ktime_add_ns(ktime_get(), (int64_t)delay_ms * 1000000);

		/* pr_debug("cur_ns = %lld, first_target_ns = %lld\n",
		 *	ktime_to_ns(ktime_get()),
		 * ktime_to_ns(obj->target_ktime));
		 */
	} else {
		do {
			obj->target_ktime = ktime_add_ns(
				obj->target_ktime, (int64_t)delay_ms * 1000000);
		} while (ktime_to_ns(obj->target_ktime) <
			 ktime_to_ns(ktime_get()));

		/* pr_debug("cur_ns = %lld, target_ns = %lld\n",
		 *	ktime_to_ns(ktime_get()),
		 *	ktime_to_ns(obj->target_ktime));
		 */
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}

#if !IS_ENABLED(CONFIG_NANOHUB) || !IS_ENABLED(CONFIG_MTK_BAROHUB)
static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}
#endif
static struct baro_init_info *barometer_init_list[MAX_CHOOSE_BARO_NUM] = {0};

static void baro_work_func(struct work_struct *work)
{

	struct baro_context *cxt = NULL;
	/* hwm_sensor_data sensor_data; */
	int value, status;
	int64_t pre_ns, cur_ns;
	int64_t delay_ms;
	int err;

	cxt = baro_context_obj;
	delay_ms = atomic_read(&cxt->delay);

	if (cxt->baro_data.get_data == NULL) {
		pr_debug("baro driver not register data path\n");
		goto baro_loop;
	}

	cur_ns = ktime_get_boottime_ns();

	/* add wake lock to make sure data can be read before system suspend */
	err = cxt->baro_data.get_data(&value, &status);

	if (err) {
		pr_err("get baro data fails!!\n");
		goto baro_loop;
	} else {
		{
			cxt->drv_data.baro_data.values[0] = value;
			cxt->drv_data.baro_data.status = status;
			pre_ns = cxt->drv_data.baro_data.time;
			cxt->drv_data.baro_data.time = cur_ns;
		}
	}

	if (true == cxt->is_first_data_after_enable) {
		pre_ns = cur_ns;
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.baro_data.values[0] == BARO_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto baro_loop;
		}
	}
	/* report data to input device */
	/*pr_debug("new baro work run....\n"); */
	/*pr_debug("baro data[%d].\n", cxt->drv_data.baro_data.values[0]); */

	while ((cur_ns - pre_ns) >= delay_ms * 1800000LL) {
		pre_ns += delay_ms * 1000000LL;
		baro_data_report(cxt->drv_data.baro_data.values[0],
				 cxt->drv_data.baro_data.status, pre_ns);
	}

	baro_data_report(cxt->drv_data.baro_data.values[0],
			 cxt->drv_data.baro_data.status,
			 cxt->drv_data.baro_data.time);

baro_loop:
	if (true == cxt->is_polling_run) {
		{
			startTimer(&cxt->hrTimer, atomic_read(&cxt->delay),
				   false);
		}
	}
}

enum hrtimer_restart baro_poll(struct hrtimer *timer)
{
	struct baro_context *obj = (struct baro_context *)container_of(timer,
		struct baro_context, hrTimer);

	queue_work(obj->baro_workqueue, &obj->report);

	return HRTIMER_NORESTART;
}

static struct baro_context *baro_context_alloc_object(void)
{

	struct baro_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc baro object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200); /*5Hz set work queue delay time 200 ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, baro_work_func);
	obj->baro_workqueue = NULL;
	obj->baro_workqueue = create_workqueue("baro_polling");
	if (!obj->baro_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, baro_poll);
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->baro_op_mutex);
	obj->is_batch_enable = false; /* for batch mode init */
	obj->power = 0;
	obj->enable = 0;
	obj->delay_ns = -1;
	obj->latency_ns = -1;

	pr_debug("%s end\n", __func__);
	return obj;
}
#if !IS_ENABLED(CONFIG_NANOHUB) || !IS_ENABLED(CONFIG_MTK_BAROHUB)
static int baro_enable_and_batch(void)
{
	struct baro_context *cxt = baro_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->power == 1 && cxt->enable == 0) {
		pr_debug("BARO disable\n");
		/* stop polling firstly, if needed */
		if (cxt->baro_ctl.is_report_input_direct == false &&
		    cxt->is_polling_run == true) {
			smp_mb(); /* for memory barrier */
			stopTimer(&cxt->hrTimer);
			smp_mb(); /* for memory barrier */
			cancel_work_sync(&cxt->report);
			cxt->drv_data.baro_data.values[0] = BARO_INVALID_VALUE;
			cxt->is_polling_run = false;
			pr_debug("baro stop polling done\n");
		}
		/* turn off the power */
		err = cxt->baro_ctl.enable_nodata(0);
		if (err) {
			pr_err("baro turn off power err = %d\n", err);
			return -1;
		}
		pr_debug("baro turn off power done\n");

		cxt->power = 0;
		cxt->delay_ns = -1;
		pr_debug("BARO disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->power == 0 && cxt->enable == 1) {
		pr_debug("BARO power on\n");
		err = cxt->baro_ctl.enable_nodata(1);
		if (err) {
			pr_err("baro turn on power err = %d\n", err);
			return -1;
		}
		pr_debug("baro turn on power done\n");

		cxt->power = 1;
		pr_debug("BARO power on done\n");
	}
	/* rate change */
	if (cxt->power == 1 && cxt->delay_ns >= 0) {
		pr_debug("BARO set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->baro_ctl.is_support_batch)
			err = cxt->baro_ctl.batch(0, cxt->delay_ns,
						  cxt->latency_ns);
		else
			err = cxt->baro_ctl.batch(0, cxt->delay_ns, 0);
		if (err) {
			pr_err("baro set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("baro set ODR, fifo latency done\n");
		/* start polling, if needed */
		if (cxt->baro_ctl.is_report_input_direct == false) {
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
			pr_debug("baro set polling delay %d ms\n",
				 atomic_read(&cxt->delay));
		}
		pr_debug("BARO batch done\n");
	}
	return 0;
}
#endif
static ssize_t baroactive_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct baro_context *cxt = baro_context_obj;
	int err = 0;

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&baro_context_obj->baro_op_mutex);
	if (!strncmp(buf, "1", 1))
		cxt->enable = 1;
	else if (!strncmp(buf, "0", 1))
		cxt->enable = 0;
	else {
		pr_err("%s error !!\n", __func__);
		err = -1;
		goto err_out;
	}
#if IS_ENABLED(CONFIG_NANOHUB) && IS_ENABLED(CONFIG_MTK_BAROHUB)
	err = cxt->baro_ctl.enable_nodata(cxt->enable);
	if (err) {
		pr_err("baro turn on power err = %d\n", err);
		goto err_out;
	}
#else
	err = baro_enable_and_batch();
#endif
err_out:
	mutex_unlock(&baro_context_obj->baro_op_mutex);
	pr_debug("%s done\n", __func__);
	if (err)
		return err;
	else
		return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t baroactive_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct baro_context *cxt = NULL;
	int div;

	cxt = baro_context_obj;
	div = cxt->baro_data.vender_div;

	pr_debug("baro vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t barobatch_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct baro_context *cxt = baro_context_obj;
	int handle = 0, flag = 0, err = 0;

	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->delay_ns,
		     &cxt->latency_ns);
	if (err != 4) {
		pr_err("grav_store_batch param error: err = %d\n", err);
		return -1;
	}

	mutex_lock(&baro_context_obj->baro_op_mutex);
#if IS_ENABLED(CONFIG_NANOHUB) && IS_ENABLED(CONFIG_MTK_BAROHUB)
	if (cxt->baro_ctl.is_support_batch)
		err = cxt->baro_ctl.batch(0, cxt->delay_ns, cxt->latency_ns);
	else
		err = cxt->baro_ctl.batch(0, cxt->delay_ns, 0);
	if (err)
		pr_err("baro set batch(ODR) err %d\n", err);
#else
	err = baro_enable_and_batch();
#endif
	mutex_unlock(&baro_context_obj->baro_op_mutex);
	pr_debug("%s done: %d\n", __func__, cxt->is_batch_enable);
	if (err)
		return err;
	else
		return count;
}

static ssize_t barobatch_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t baroflush_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct baro_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&baro_context_obj->baro_op_mutex);
	cxt = baro_context_obj;
	if (cxt->baro_ctl.flush != NULL)
		err = cxt->baro_ctl.flush();
	else
		pr_err(
			"BARO DRIVER OLD ARCHITECTURE DON'T SUPPORT BARO COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_err("baro enable flush err %d\n", err);
	mutex_unlock(&baro_context_obj->baro_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t baroflush_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t barodevnum_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int barometer_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int barometer_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id barometer_of_match[] = {
	{
		.compatible = "mediatek,barometer",
	},
	{},
};
#endif

static struct platform_driver barometer_driver = {
	.probe = barometer_probe,
	.remove = barometer_remove,
	.driver = {
		.name = "barometer",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = barometer_of_match,
#endif
	}
};

static int baro_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_BARO_NUM; i++) {
		pr_debug(" i=%d\n", i);
		if (barometer_init_list[i] != 0) {
			pr_debug(" baro try to init driver %s\n",
				 barometer_init_list[i]->name);
			err = barometer_init_list[i]->init();
			if (err == 0) {
				pr_debug(" baro real driver %s probe ok\n",
					 barometer_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_BARO_NUM) {
		pr_debug("%s fail\n", __func__);
		err = -1;
	}
	return err;
}

int baro_driver_add(struct baro_init_info *obj)
{
	int err = 0;
	int i = 0;

	pr_debug("%s\n", __func__);
	if (!obj) {
		pr_err("BARO driver add fail, baro_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_BARO_NUM; i++) {
		if (i == 0) {
			pr_debug(
				"register barometer driver for the first time\n");
			if (platform_driver_register(&barometer_driver))
				pr_err(
					"failed to register gensor driver already exist\n");
		}

		if (barometer_init_list[i] == NULL) {
			obj->platform_diver_addr = &barometer_driver;
			barometer_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_BARO_NUM) {
		pr_err("BARO driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(baro_driver_add);

static int pressure_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t pressure_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(baro_context_obj->mdev.minor, file, buffer,
				     count, ppos);

	return read_cnt;
}

static unsigned int pressure_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(baro_context_obj->mdev.minor, file, wait);
}

static const struct file_operations pressure_fops = {
	.owner = THIS_MODULE,
	.open = pressure_open,
	.read = pressure_read,
	.poll = pressure_poll,
};

static int baro_misc_init(struct baro_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = ID_PRESSURE;
	cxt->mdev.name = BARO_MISC_DEV_NAME;
	cxt->mdev.fops = &pressure_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register baro misc device!!\n");

	return err;
}

DEVICE_ATTR_RW(baroactive);
DEVICE_ATTR_RW(barobatch);
DEVICE_ATTR_RW(baroflush);
DEVICE_ATTR_RO(barodevnum);

static struct attribute *baro_attributes[] = {
	&dev_attr_baroactive.attr,
	&dev_attr_barobatch.attr,
	&dev_attr_baroflush.attr,
	&dev_attr_barodevnum.attr,
	NULL
};

static struct attribute_group baro_attribute_group = {
	.attrs = baro_attributes
};

int baro_register_data_path(struct baro_data_path *data)
{
	struct baro_context *cxt = NULL;

	cxt = baro_context_obj;
	cxt->baro_data.get_data = data->get_data;
	cxt->baro_data.vender_div = data->vender_div;
	cxt->baro_data.get_raw_data = data->get_raw_data;
	pr_debug("baro register data path vender_div: %d\n",
		 cxt->baro_data.vender_div);
	if (cxt->baro_data.get_data == NULL) {
		pr_debug("baro register data path fail\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(baro_register_data_path);

int baro_register_control_path(struct baro_control_path *ctl)
{
	struct baro_context *cxt = NULL;
	int err = 0;

	cxt = baro_context_obj;
	cxt->baro_ctl.set_delay = ctl->set_delay;
	cxt->baro_ctl.open_report_data = ctl->open_report_data;
	cxt->baro_ctl.enable_nodata = ctl->enable_nodata;
	cxt->baro_ctl.batch = ctl->batch;
	cxt->baro_ctl.flush = ctl->flush;
	cxt->baro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->baro_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->baro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->baro_ctl.is_use_common_factory = ctl->is_use_common_factory;

	if (cxt->baro_ctl.set_delay == NULL ||
	    cxt->baro_ctl.open_report_data == NULL ||
	    cxt->baro_ctl.enable_nodata == NULL) {
		pr_debug("baro register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = baro_misc_init(baro_context_obj);
	if (err) {
		pr_err("unable to register baro misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&baro_context_obj->mdev.this_device->kobj,
				 &baro_attribute_group);
	if (err < 0) {
		pr_err("unable to create baro attribute file\n");
		return -3;
	}

	kobject_uevent(&baro_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}
EXPORT_SYMBOL_GPL(baro_register_control_path);

int baro_data_report(int value, int status, int64_t nt)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = DATA_ACTION;
	event.time_stamp = nt;
	event.word[0] = value;
	event.status = status;

	err = sensor_input_event(baro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(baro_data_report);

int baro_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	pr_debug_ratelimited("flush\n");
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(baro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(baro_flush_report);

int baro_probe(void)
{
	int err;

	pr_debug("%s+++!!\n", __func__);

	baro_context_obj = baro_context_alloc_object();
	if (!baro_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real baro driver */
	err = baro_real_driver_init();
	if (err) {
		pr_err("baro real driver init fail\n");
		goto real_driver_init_fail;
	}

	pr_debug("%s--- OK !!\n", __func__);
	return 0;

real_driver_init_fail:
	kfree(baro_context_obj);
	baro_context_obj = NULL;
exit_alloc_data_failed:

	pr_debug("%s----fail !!!\n", __func__);
	return err;
}
EXPORT_SYMBOL_GPL(baro_probe);

int baro_remove(void)
{
	int err = 0;

	pr_debug("%s\n", __func__);

	sysfs_remove_group(&baro_context_obj->mdev.this_device->kobj,
			   &baro_attribute_group);

	err = sensor_attr_deregister(&baro_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);
	kfree(baro_context_obj);

	platform_driver_unregister(&barometer_driver);

	return 0;
}
EXPORT_SYMBOL_GPL(baro_remove);

static int __init baro_init(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static void __exit baro_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(baro_init);
module_exit(baro_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BAROMETER device driver");
MODULE_AUTHOR("Mediatek");

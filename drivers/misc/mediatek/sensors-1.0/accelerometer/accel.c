// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<ACCELEROMETER> " fmt
#include "inc/accel.h"
#include "inc/accel_factory.h"
#include "sensor_performance.h"
#include <linux/vmalloc.h>

struct acc_context *acc_context_obj /* = NULL*/;

static struct acc_init_info *gsensor_init_list[MAX_CHOOSE_G_NUM] = {0};

static int64_t getCurNS(void)
{
	int64_t ns;

	ns = ktime_get_boottime_ns();

	return ns;
}

static void initTimer(struct hrtimer *timer,
		      enum hrtimer_restart (*callback)(struct hrtimer *))
{
	hrtimer_init(timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	timer->function = callback;
}

static void startTimer(struct hrtimer *timer, int delay_ms, bool first)
{
	struct acc_context *obj = (struct acc_context *)container_of(timer,
		struct acc_context, hrTimer);
	static int count;

	if (obj == NULL) {
		pr_err("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime =
			ktime_add_ns(ktime_get(), (int64_t)delay_ms * 1000000);
		/* pr_debug("%d, cur_nt = %lld, delay_ms = %d,
		 * target_nt = %lld\n",count, getCurNT(),
		 * delay_ms, ktime_to_us(obj->target_ktime));
		 */
		count = 0;
	} else {
		do {
			obj->target_ktime = ktime_add_ns(
				obj->target_ktime, (int64_t)delay_ms * 1000000);
		} while (ktime_to_ns(obj->target_ktime) <
			 ktime_to_ns(ktime_get()));
		/* pr_debug("%d, cur_nt = %lld, delay_ms = %d,
		 * target_nt = %lld\n",
		 *  count, getCurNT(), delay_ms,
		 * ktime_to_us(obj->target_ktime));
		 */
		count++;
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}

#ifndef CONFIG_NANOHUB
static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}
#endif
static void acc_work_func(struct work_struct *work)
{
	struct acc_context *cxt = NULL;
	int x, y, z, status;
	int64_t pre_ns, cur_ns;
	int64_t delay_ms;
	int err;

	cxt = acc_context_obj;
	delay_ms = atomic_read(&cxt->delay);

	if (cxt->acc_data.get_data == NULL) {
		pr_err("acc driver not register data path\n");
		return;
	}

	cur_ns = getCurNS();

	err = cxt->acc_data.get_data(&x, &y, &z, &status);

	if (err) {
		pr_err("get acc data fails!!\n");
		goto acc_loop;
	} else {
		if (0 == x && 0 == y && 0 == z)
			goto acc_loop;

		cxt->drv_data.x = x;
		cxt->drv_data.y = y;
		cxt->drv_data.z = z;
		cxt->drv_data.status = status;
		pre_ns = cxt->drv_data.timestamp;
		cxt->drv_data.timestamp = cur_ns;
	}

	if (true == cxt->is_first_data_after_enable) {
		pre_ns = cur_ns;
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.x == ACC_INVALID_VALUE ||
		    cxt->drv_data.y == ACC_INVALID_VALUE ||
		    cxt->drv_data.z == ACC_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto acc_loop;
		}
	}
	/* report data to input device */
	/* printk("new acc work run....\n"); */
	/* pr_debug("acc data[%d,%d,%d]\n" ,cxt->drv_data.acc_data.values[0],*/
	/* cxt->drv_data.acc_data.values[1],cxt->drv_data.acc_data.values[2]);*/

	while ((cur_ns - pre_ns) >= delay_ms * 1800000LL) {
		struct acc_data tmp_data = cxt->drv_data;

		pre_ns += delay_ms * 1000000LL;
		tmp_data.timestamp = pre_ns;
		acc_data_report(&tmp_data);
	}

	acc_data_report(&cxt->drv_data);

acc_loop:
	if (true == cxt->is_polling_run)
		startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), false);
}

enum hrtimer_restart acc_poll(struct hrtimer *timer)
{
	struct acc_context *obj = (struct acc_context *)container_of(timer,
		struct acc_context, hrTimer);

	queue_work(obj->accel_workqueue, &obj->report);

	/* pr_debug("cur_ns = %lld\n", getCurNS()); */

	return HRTIMER_NORESTART;
}

static struct acc_context *acc_context_alloc_object(void)
{

	struct acc_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc accel object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200); /*5Hz,set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, acc_work_func);
	obj->accel_workqueue = NULL;
	obj->accel_workqueue = create_workqueue("accel_polling");
	if (!obj->accel_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, acc_poll);
	obj->is_active_nodata = false;
	obj->is_active_data = false;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->acc_op_mutex);
	obj->is_batch_enable = false; /* for batch mode init */
	obj->cali_sw[ACC_AXIS_X] = 0;
	obj->cali_sw[ACC_AXIS_Y] = 0;
	obj->cali_sw[ACC_AXIS_Z] = 0;
	obj->power = 0;
	obj->enable = 0;
	obj->delay_ns = -1;
	obj->latency_ns = -1;
	pr_debug("%s end\n", __func__);
	return obj;
}

#ifndef CONFIG_NANOHUB
static int acc_enable_and_batch(void)
{
	struct acc_context *cxt = acc_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->power == 1 && cxt->enable == 0) {
		pr_debug("ACC disable\n");
		/* stop polling firstly, if needed */
		if (cxt->is_active_data == false &&
		    cxt->acc_ctl.is_report_input_direct == false &&
		    cxt->is_polling_run == true) {
			smp_mb(); /* for memory barrier */
			stopTimer(&cxt->hrTimer);
			smp_mb(); /* for memory barrier */
			cancel_work_sync(&cxt->report);
			cxt->drv_data.x = ACC_INVALID_VALUE;
			cxt->drv_data.y = ACC_INVALID_VALUE;
			cxt->drv_data.z = ACC_INVALID_VALUE;
			cxt->is_polling_run = false;
			pr_debug("acc stop polling done\n");
		}
		/* turn off the power */
		if (cxt->is_active_data == false &&
		    cxt->is_active_nodata == false) {
			err = cxt->acc_ctl.enable_nodata(0);
			if (err) {
				pr_err("acc turn off power err:%d\n", err);
				return -1;
			}
			pr_debug("acc turn off power done\n");
		}

		cxt->power = 0;
		cxt->delay_ns = -1;
		pr_debug("ACC disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->power == 0 && cxt->enable == 1) {
		pr_debug("ACC power on\n");
		if (true == cxt->is_active_data ||
		    true == cxt->is_active_nodata) {
			err = cxt->acc_ctl.enable_nodata(1);
			if (err) {
				pr_err("acc turn on power err = %d\n", err);
				return -1;
			}
			pr_debug("acc turn on power done\n");
		}
		cxt->power = 1;
		pr_debug("ACC power on done\n");
	}
	/* rate change */
	if (cxt->power == 1 && cxt->delay_ns >= 0) {
		pr_debug("ACC set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->acc_ctl.is_support_batch)
			err = cxt->acc_ctl.batch(0, cxt->delay_ns,
						 cxt->latency_ns);
		else
			err = cxt->acc_ctl.batch(0, cxt->delay_ns, 0);
		if (err) {
			pr_err("acc set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("acc set ODR, fifo latency done\n");
		/* start polling, if needed */
		if (cxt->is_active_data == true &&
		    cxt->acc_ctl.is_report_input_direct == false) {
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
			pr_debug("acc set polling delay %d ms\n",
				atomic_read(&cxt->delay));
		}
		pr_debug("ACC batch done\n");
	}
	return 0;
}
#endif
static ssize_t accenablenodata_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
#if !IS_ENABLED(CONFIG_MTK_SCP_SENSORHUB_V1) && !IS_ENABLED(CONFIG_NANOHUB)
	struct acc_context *cxt = acc_context_obj;
	int err = 0;

	pr_debug("acc_store_enable nodata buf=%s\n", buf);
	mutex_lock(&acc_context_obj->acc_op_mutex);
	if (!strncmp(buf, "1", 1)) {
		cxt->enable = 1;
		cxt->is_active_nodata = true;
	} else if (!strncmp(buf, "0", 1)) {
		cxt->enable = 0;
		cxt->is_active_nodata = false;
	} else {
		pr_err(" acc_store enable nodata cmd error !!\n");
		err = -1;
		goto err_out;
	}
	err = acc_enable_and_batch();
err_out:
	mutex_unlock(&acc_context_obj->acc_op_mutex);
	if (err)
		return err;
	else
		return count;
#else
	return count;
#endif
}

static ssize_t accenablenodata_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t accactive_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct acc_context *cxt = acc_context_obj;
	int err = 0;

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&acc_context_obj->acc_op_mutex);
	if (!strncmp(buf, "1", 1)) {
		cxt->enable = 1;
		cxt->is_active_data = true;
	} else if (!strncmp(buf, "0", 1)) {
		cxt->enable = 0;
		cxt->is_active_data = false;
	} else {
		pr_err("%s error !!\n", __func__);
		err = -1;
		goto err_out;
	}
#if IS_ENABLED(CONFIG_NANOHUB)
	if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
		err = cxt->acc_ctl.enable_nodata(1);
		if (err) {
			pr_err("acc turn on power err = %d\n", err);
			goto err_out;
		}
		pr_debug("acc turn on power done\n");
	} else {
		err = cxt->acc_ctl.enable_nodata(0);
		if (err) {
			pr_err("acc turn off power err = %d\n", err);
			goto err_out;
		}
		pr_debug("acc turn off power done\n");
	}
#else
	err = acc_enable_and_batch();
#endif

err_out:
	mutex_unlock(&acc_context_obj->acc_op_mutex);
	if (err)
		return err;
	else
		return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t accactive_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct acc_context *cxt = acc_context_obj;
	int div = 0;

	div = cxt->acc_data.vender_div;
	pr_debug("acc vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

/* need work around again */
static ssize_t accdevnum_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t accbatch_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct acc_context *cxt = acc_context_obj;
	int handle = 0, flag = 0, err = 0;

	pr_debug("%s %s\n", __func__, buf);
	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->delay_ns,
		     &cxt->latency_ns);
	if (err != 4) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return -1;
	}

	mutex_lock(&acc_context_obj->acc_op_mutex);

#if IS_ENABLED(CONFIG_NANOHUB)
	if (cxt->acc_ctl.is_support_batch)
		err = cxt->acc_ctl.batch(0, cxt->delay_ns, cxt->latency_ns);
	else
		err = cxt->acc_ctl.batch(0, cxt->delay_ns, 0);
	if (err)
		pr_err("acc set batch(ODR) err %d\n", err);
#else
	err = acc_enable_and_batch();
#endif

	mutex_unlock(&acc_context_obj->acc_op_mutex);
	if (err)
		return err;
	else
		return count;
}
static ssize_t accbatch_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static ssize_t accflush_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct acc_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&acc_context_obj->acc_op_mutex);
	cxt = acc_context_obj;
	if (cxt->acc_ctl.flush != NULL)
		err = cxt->acc_ctl.flush();
	else
		pr_err("DON'T SUPPORT ACC COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_err("acc enable flush err %d\n", err);
	mutex_unlock(&acc_context_obj->acc_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t accflush_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t acccali_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t acccali_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct acc_context *cxt = NULL;
	int err = 0;
	uint8_t *cali_buf = NULL;

	cali_buf = vzalloc(count);
	if (cali_buf == NULL)
		return -EFAULT;
	memcpy(cali_buf, buf, count);

	mutex_lock(&acc_context_obj->acc_op_mutex);
	cxt = acc_context_obj;
	if (cxt->acc_ctl.set_cali != NULL)
		err = cxt->acc_ctl.set_cali(cali_buf, count);
	else
		pr_err("DON'T SUPPORT ACC COMMONVERSION FLUSH\n");
	if (err < 0)
		pr_err("acc set cali err %d\n", err);
	mutex_unlock(&acc_context_obj->acc_op_mutex);
	vfree(cali_buf);
	return count;
}

static int gsensor_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int gsensor_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gsensor_of_match[] = {
	{
		.compatible = "mediatek,gsensor",
	},
	{},
};
#endif
static struct platform_driver gsensor_driver = {
	.probe = gsensor_probe,
	.remove = gsensor_remove,
	.driver = {
		.name = "gsensor",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = gsensor_of_match,
#endif
	}
};

static int acc_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		pr_debug(" i=%d\n", i);
		if (gsensor_init_list[i] != 0) {
			pr_debug(" acc try to init driver %s\n",
				gsensor_init_list[i]->name);
			err = gsensor_init_list[i]->init();
			if (err == 0) {
				pr_debug(" acc real driver %s probe ok\n",
					gsensor_init_list[i]->name);
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

static int acc_real_driver_uninit(void)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		pr_debug(" i=%d\n", i);
		if (gsensor_init_list[i] != 0) {
			pr_debug(" acc try to init driver %s\n",
				gsensor_init_list[i]->name);
			err = gsensor_init_list[i]->uninit();
			if (err == 0) {
				pr_debug(" acc real driver %s uninit ok\n",
					gsensor_init_list[i]->name);
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

int acc_driver_add(struct acc_init_info *obj)
{
	int err = 0;
	int i = 0;

	if (!obj) {
		pr_err("ACC driver add fail, acc_init_info is NULL\n");
		return -1;
	}
	for (i = 0; i < MAX_CHOOSE_G_NUM; i++) {
		if ((i == 0) && (gsensor_init_list[0] == NULL)) {
			pr_debug("register gensor driver for the first time\n");
			if (platform_driver_register(&gsensor_driver))
				pr_err("failed: driveralready exist\n");
		}

		if (gsensor_init_list[i] == NULL) {
			obj->platform_diver_addr = &gsensor_driver;
			gsensor_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_G_NUM) {
		pr_err("ACC driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(acc_driver_add);

static int accel_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t accel_read(struct file *file, char __user *buffer, size_t count,
			  loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(acc_context_obj->mdev.minor, file, buffer,
				     count, ppos);

	return read_cnt;
}

static unsigned int accel_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(acc_context_obj->mdev.minor, file, wait);
}

static const struct file_operations accel_fops = {
	.owner = THIS_MODULE,
	.open = accel_open,
	.read = accel_read,
	.poll = accel_poll,
};

static int acc_misc_init(struct acc_context *cxt)
{
	int err = 0;

	cxt->mdev.minor = ID_ACCELEROMETER;
	cxt->mdev.name = ACC_MISC_DEV_NAME;
	cxt->mdev.fops = &accel_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register acc misc device!!\n");
	return err;
}

DEVICE_ATTR_RW(accenablenodata);
DEVICE_ATTR_RW(accactive);
DEVICE_ATTR_RW(accbatch);
DEVICE_ATTR_RW(accflush);
DEVICE_ATTR_RW(acccali);
DEVICE_ATTR_RO(accdevnum);

static struct attribute *acc_attributes[] = {
	&dev_attr_accenablenodata.attr,
	&dev_attr_accactive.attr,
	&dev_attr_accbatch.attr,
	&dev_attr_accflush.attr,
	&dev_attr_acccali.attr,
	&dev_attr_accdevnum.attr,
	NULL
};

static struct attribute_group acc_attribute_group = {
	.attrs = acc_attributes
};

int acc_register_data_path(struct acc_data_path *data)
{
	struct acc_context *cxt = NULL;

	cxt = acc_context_obj;
	cxt->acc_data.get_data = data->get_data;
	cxt->acc_data.get_raw_data = data->get_raw_data;
	cxt->acc_data.vender_div = data->vender_div;
	pr_debug("acc register data path vender_div: %d\n",
		cxt->acc_data.vender_div);
	if (cxt->acc_data.get_data == NULL) {
		pr_debug("acc register data path fail\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(acc_register_data_path);

int acc_register_control_path(struct acc_control_path *ctl)
{
	struct acc_context *cxt = NULL;
	int err = 0;

	cxt = acc_context_obj;
	cxt->acc_ctl.enable_nodata = ctl->enable_nodata;
	cxt->acc_ctl.batch = ctl->batch;
	cxt->acc_ctl.flush = ctl->flush;
	cxt->acc_ctl.set_cali = ctl->set_cali;
	cxt->acc_ctl.is_support_batch = ctl->is_support_batch;
	cxt->acc_ctl.is_report_input_direct = ctl->is_report_input_direct;

	if (cxt->acc_ctl.enable_nodata == NULL || cxt->acc_ctl.batch == NULL ||
	    cxt->acc_ctl.flush == NULL) {
		pr_debug("acc register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = acc_misc_init(acc_context_obj);
	if (err) {
		pr_err("unable to register acc misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&acc_context_obj->mdev.this_device->kobj,
				 &acc_attribute_group);
	if (err < 0) {
		pr_err("unable to create acc attribute file\n");
		return -3;
	}

	kobject_uevent(&acc_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}
EXPORT_SYMBOL_GPL(acc_register_control_path);

int acc_data_report(struct acc_data *data)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.time_stamp = data->timestamp;
	event.flush_action = DATA_ACTION;
	event.status = data->status;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;
	event.reserved = data->reserved[0];
	/* pr_err("x:%d,y:%d,z:%d,time:%lld\n", data->x, data->y, data->z,
	 * data->timestamp);
	 */
	if (event.reserved == 1)
		mark_timestamp(ID_ACCELEROMETER, DATA_REPORT,
			       ktime_get_boot_ns(), event.time_stamp);
	err = sensor_input_event(acc_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(acc_data_report);

int acc_bias_report(struct acc_data *data)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = BIAS_ACTION;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;
	/* pr_err("x:%d,y:%d,z:%d,time:%lld\n", x, y, z, nt); */
	err = sensor_input_event(acc_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(acc_bias_report);

int acc_cali_report(struct acc_data *data)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = CALI_ACTION;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;
	/* pr_err("x:%d,y:%d,z:%d,time:%lld\n", x, y, z, nt); */
	err = sensor_input_event(acc_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(acc_cali_report);

int acc_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	pr_debug_ratelimited("flush\n");
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(acc_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(acc_flush_report);

int acc_probe(void)
{

	int err;

	pr_debug("+++++++++++++accel_probe!!\n");

	acc_context_obj = acc_context_alloc_object();
	if (!acc_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real acceleration driver */
	err = acc_real_driver_init();
	if (err) {
		pr_err("acc real driver init fail\n");
		goto real_driver_init_fail;
	}

	pr_debug("----accel_probe OK !!\n");
	return 0;

real_driver_init_fail:
	kfree(acc_context_obj);

exit_alloc_data_failed:

	pr_err("----accel_probe fail !!!\n");
	return err;
}
EXPORT_SYMBOL_GPL(acc_probe);

int acc_remove(void)
{
	int err = 0;

	acc_real_driver_uninit();

	sysfs_remove_group(&acc_context_obj->mdev.this_device->kobj,
			   &acc_attribute_group);

	err = sensor_attr_deregister(&acc_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);
	kfree(acc_context_obj);

	platform_driver_unregister(&gsensor_driver);

	return 0;
}
EXPORT_SYMBOL_GPL(acc_remove);

static int __init acc_init(void)
{
	pr_debug("%s\n", __func__);

	return 0;
}

static void __exit acc_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(acc_init);
module_exit(acc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ACCELEROMETER device driver");
MODULE_AUTHOR("Mediatek");

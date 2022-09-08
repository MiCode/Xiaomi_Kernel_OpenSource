// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<GYROSCOPE> " fmt

#include "inc/gyroscope.h"
#include "sensor_performance.h"
#include <linux/vmalloc.h>

struct gyro_context *gyro_context_obj /* = NULL*/;
static struct platform_device *pltfm_dev;

static struct gyro_init_info *gyroscope_init_list[MAX_CHOOSE_GYRO_NUM] = {0};

static int64_t getCurNS(void)
{
	int64_t ns;

	ns = ktime_get_boottime_ns();

	return ns;

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
	struct gyro_context *obj = (struct gyro_context *)container_of(timer,
		struct gyro_context, hrTimer);
	static int count;

	if (obj == NULL) {
		pr_err("NULL pointer\n");
		return;
	}

	if (first) {
		obj->target_ktime =
			ktime_add_ns(ktime_get(), (int64_t)delay_ms * 1000000);
		count = 0;
	} else {
		do {
			obj->target_ktime = ktime_add_ns(
				obj->target_ktime, (int64_t)delay_ms * 1000000);
		} while (ktime_to_ns(obj->target_ktime) <
			 ktime_to_ns(ktime_get()));
		count++;
	}

	hrtimer_start(timer, obj->target_ktime, HRTIMER_MODE_ABS);
}

#if !IS_ENABLED(CONFIG_NANOHUB)
static void stopTimer(struct hrtimer *timer)
{
	hrtimer_cancel(timer);
}
#endif
static void gyro_work_func(struct work_struct *work)
{

	struct gyro_context *cxt = NULL;
	int x = 0, y = 0, z = 0, status = 0;
	int temperature = -32768; /* =0xFFFF_8000 */
	int64_t pre_ns, cur_ns;
	int64_t delay_ms;
	int err = 0;

	cxt = gyro_context_obj;
	delay_ms = atomic_read(&cxt->delay);

	cur_ns = getCurNS();

	/* gyro driver has register temperature path */
	if (cxt->gyro_data.get_temperature) {
		err = cxt->gyro_data.get_temperature(&temperature);
		if (err)
			pr_info("get gyro temperature fails!!\n");
		else
			cxt->drv_data.temperature = temperature;
	}

	/* add wake lock to make sure data can be read before system suspend */
	if (cxt->gyro_data.get_data != NULL)
		err = cxt->gyro_data.get_data(&x, &y, &z, &status);
	else
		pr_err("gyro driver not register data path\n");

	if (err) {
		pr_err("get gyro data fails!!\n");
		goto gyro_loop;
	} else {
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
		if (cxt->drv_data.x == GYRO_INVALID_VALUE ||
		    cxt->drv_data.y == GYRO_INVALID_VALUE ||
		    cxt->drv_data.z == GYRO_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto gyro_loop;
		}
	}

	/* pr_debug("gyro data[%d,%d,%d]\n" ,cxt->drv_data.gyro_data.values[0],
	 */
	/* cxt->drv_data.gyro_data.values[1],cxt->drv_data.gyro_data.values[2]);
	 */

	while ((cur_ns - pre_ns) >= delay_ms * 1800000LL) {
		struct gyro_data tmp_data = cxt->drv_data;

		pre_ns += delay_ms * 1000000LL;
		tmp_data.timestamp = pre_ns;
		gyro_data_report(&tmp_data);
	}

	gyro_data_report(&cxt->drv_data);

gyro_loop:
	if (true == cxt->is_polling_run)
		startTimer(&cxt->hrTimer, atomic_read(&cxt->delay), false);
}

enum hrtimer_restart gyro_poll(struct hrtimer *timer)
{
	struct gyro_context *obj = (struct gyro_context *)container_of(timer,
		struct gyro_context, hrTimer);

	queue_work(obj->gyro_workqueue, &obj->report);

	/* pr_debug("cur_nt = %lld\n", getCurNT()); */

	return HRTIMER_NORESTART;
}

static struct gyro_context *gyro_context_alloc_object(void)
{

	struct gyro_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc gyro object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200); /*5Hz,  set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, gyro_work_func);
	obj->gyro_workqueue = NULL;
	obj->gyro_workqueue = create_workqueue("gyro_polling");
	if (!obj->gyro_workqueue) {
		kfree(obj);
		return NULL;
	}
	initTimer(&obj->hrTimer, gyro_poll);
	obj->is_active_nodata = false;
	obj->is_active_data = false;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	obj->cali_sw[GYRO_AXIS_X] = 0;
	obj->cali_sw[GYRO_AXIS_Y] = 0;
	obj->cali_sw[GYRO_AXIS_Z] = 0;
	obj->power = 0;
	obj->enable = 0;
	obj->delay_ns = -1;
	obj->latency_ns = -1;
	mutex_init(&obj->gyro_op_mutex);
	pr_debug("%s end\n", __func__);
	return obj;
}

#if !IS_ENABLED(CONFIG_NANOHUB)
static int gyro_enable_and_batch(void)
{
	struct gyro_context *cxt = gyro_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->power == 1 && cxt->enable == 0) {
		pr_debug("GYRO disable\n");
		/* stop polling firstly, if needed */
		if (cxt->is_active_data == false &&
		    cxt->gyro_ctl.is_report_input_direct == false &&
		    cxt->is_polling_run == true) {
			smp_mb(); /* for memory barrier */
			stopTimer(&cxt->hrTimer);
			smp_mb(); /* for memory barrier */
			cancel_work_sync(&cxt->report);
			cxt->drv_data.x = GYRO_INVALID_VALUE;
			cxt->drv_data.y = GYRO_INVALID_VALUE;
			cxt->drv_data.z = GYRO_INVALID_VALUE;
			cxt->drv_data.temperature = 0;
			cxt->is_polling_run = false;
			pr_debug("gyro stop polling done\n");
		}
		/* turn off the power */
		if (cxt->is_active_data == false &&
		    cxt->is_active_nodata == false) {
			err = cxt->gyro_ctl.enable_nodata(0);
			if (err) {
				pr_err("gyro turn off power err = %d\n",
					    err);
				return -1;
			}
			pr_debug("gyro turn off power done\n");
		}

		cxt->power = 0;
		cxt->delay_ns = -1;
		pr_debug("GYRO disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->power == 0 && cxt->enable == 1) {
		pr_debug("GYRO enable\n");
		if (true == cxt->is_active_data ||
		    true == cxt->is_active_nodata) {
			err = cxt->gyro_ctl.enable_nodata(1);
			if (err) {
				pr_err("gyro turn on power err = %d\n",
					    err);
				return -1;
			}
			pr_debug("gyro turn on power done\n");
		}
		cxt->power = 1;
		pr_debug("GYRO enable done\n");
	}
	/* rate change */
	if (cxt->power == 1 && cxt->delay_ns >= 0) {
		pr_debug("GYRO set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->gyro_ctl.is_support_batch)
			err = cxt->gyro_ctl.batch(0, cxt->delay_ns,
						  cxt->latency_ns);
		else
			err = cxt->gyro_ctl.batch(0, cxt->delay_ns, 0);
		if (err) {
			pr_err("gyro set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("gyro set ODR, fifo latency done\n");
		/* start polling, if needed */
		if (cxt->is_active_data == true &&
		    cxt->gyro_ctl.is_report_input_direct == false) {
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
			pr_debug("gyro set polling delay %d ms\n",
				 atomic_read(&cxt->delay));
		}
		pr_debug("GYRO batch done\n");
	}
	return 0;
}
#endif
static ssize_t gyroenablenodata_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t gyroenablenodata_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct gyro_context *cxt = gyro_context_obj;
	int err = 0;

	pr_debug("gyro_store_enable nodata buf=%s\n", buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	if (!strncmp(buf, "1", 1)) {
		cxt->enable = 1;
		cxt->is_active_nodata = true;
	} else if (!strncmp(buf, "0", 1)) {
		cxt->enable = 0;
		cxt->is_active_nodata = false;
	} else {
		pr_info(" gyro_store enable nodata cmd error !!\n");
		err = -1;
		goto err_out;
	}
#if IS_ENABLED(CONFIG_NANOHUB)
	if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
		err = cxt->gyro_ctl.enable_nodata(1);
		if (err) {
			pr_err("gyro turn on power err = %d\n", err);
			goto err_out;
		}
		pr_debug("gyro turn on power done\n");
	} else {
		err = cxt->gyro_ctl.enable_nodata(0);
		if (err) {
			pr_err("gyro turn off power err = %d\n", err);
			goto err_out;
		}
		pr_debug("gyro turn off power done\n");
	}
#else
	err = gyro_enable_and_batch();
#endif

err_out:
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t gyroactive_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct gyro_context *cxt = gyro_context_obj;
	int err = 0;

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
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
		err = cxt->gyro_ctl.enable_nodata(1);
		if (err) {
			pr_err("gyro turn on power err = %d\n", err);
			goto err_out;
		}
		pr_debug("gyro turn on power done\n");
	} else {
		err = cxt->gyro_ctl.enable_nodata(0);
		if (err) {
			pr_err("gyro turn off power err = %d\n", err);
			goto err_out;
		}
		pr_debug("gyro turn off power done\n");
	}
#else
	err = gyro_enable_and_batch();
#endif
err_out:
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	pr_debug("%s done\n", __func__);
	if (err)
		return err;
	else
		return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t gyroactive_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct gyro_context *cxt = NULL;
	int div = 0;

	cxt = gyro_context_obj;

	pr_debug("gyro show active not support now\n");
	div = cxt->gyro_data.vender_div;
	pr_debug("gyro vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t gyrobatch_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct gyro_context *cxt = gyro_context_obj;
	int handle = 0, flag = 0, err = 0;

	pr_debug("%s %s\n", __func__, buf);
	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->delay_ns,
		     &cxt->latency_ns);
	if (err != 4) {
		pr_info("%s param error: err = %d\n", __func__, err);
		return -1;
	}

	mutex_lock(&gyro_context_obj->gyro_op_mutex);
#if IS_ENABLED(CONFIG_NANOHUB)
	if (cxt->gyro_ctl.is_support_batch)
		err = cxt->gyro_ctl.batch(0, cxt->delay_ns, cxt->latency_ns);
	else
		err = cxt->gyro_ctl.batch(0, cxt->delay_ns, 0);
	if (err)
		pr_err("gyro set batch(ODR) err %d\n", err);
#else
	err = gyro_enable_and_batch();
#endif
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t gyrobatch_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t gyroflush_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct gyro_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_info("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if (cxt->gyro_ctl.flush != NULL)
		err = cxt->gyro_ctl.flush();
	else
		pr_info(
			"GYRO DRIVER OLD ARCHITECTURE DON'T SUPPORT GYRO COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_info("gyro enable flush err %d\n", err);
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t gyroflush_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static ssize_t gyrocali_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t gyrocali_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct gyro_context *cxt = NULL;
	int err = 0;
	uint8_t *cali_buf = NULL;

	cali_buf = vzalloc(count);
	if (cali_buf == NULL)
		return -EFAULT;
	memcpy(cali_buf, buf, count);

	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if (cxt->gyro_ctl.set_cali != NULL)
		err = cxt->gyro_ctl.set_cali(cali_buf, count);
	else
		pr_info(
			"GYRO DRIVER OLD ARCHITECTURE DON'T SUPPORT GYRO COMMON VERSION FLUSH\n");
	if (err < 0)
		pr_info("gyro set cali err %d\n", err);
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	vfree(cali_buf);
	return count;
}

/* need work around again */
static ssize_t gyrodevnum_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static int gyroscope_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int gyroscope_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	pltfm_dev = pdev;
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id gyroscope_of_match[] = {
	{
		.compatible = "mediatek,gyroscope",
	},
	{},
};
#endif

static struct platform_driver gyroscope_driver = {
	.probe = gyroscope_probe,
	.remove = gyroscope_remove,
	.driver = {
		.name = "gyroscope",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = gyroscope_of_match,
#endif
	}
};

static int gyro_real_driver_init(struct platform_device *pdev)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_GYRO_NUM; i++) {
		pr_debug("i=%d\n", i);
		if (gyroscope_init_list[i] != 0) {
			pr_debug("gyro try to init driver %s\n",
				 gyroscope_init_list[i]->name);
			err = gyroscope_init_list[i]->init(pdev);
			if (err == 0) {
				pr_debug("gyro real driver %s probe ok\n",
					 gyroscope_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_GYRO_NUM) {
		pr_debug("%s fail\n", __func__);
		err = -1;
	}
	return err;
}

int gyro_driver_add(struct gyro_init_info *obj)
{
	int err = 0;
	int i = 0;

	if (!obj) {
		pr_err("%s fail, gyro_init_info is NULL\n", __func__);
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_GYRO_NUM; i++) {
		if ((i == 0) && (gyroscope_init_list[0] == NULL)) {
			pr_debug("register gyro driver for the first time\n");
			if (platform_driver_register(&gyroscope_driver))
				pr_err(
					"failed to register gyro driver already exist\n");
		}

		if (gyroscope_init_list[i] == NULL) {
			obj->platform_diver_addr = &gyroscope_driver;
			gyroscope_init_list[i] = obj;
			break;
		}
	}

	if (i >= MAX_CHOOSE_GYRO_NUM) {
		pr_err("gyro driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(gyro_driver_add);
static int gyroscope_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t gyroscope_read(struct file *file, char __user *buffer,
			      size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(gyro_context_obj->mdev.minor, file, buffer,
				     count, ppos);

	return read_cnt;
}

static unsigned int gyroscope_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(gyro_context_obj->mdev.minor, file, wait);
}

static const struct file_operations gyroscope_fops = {
	.owner = THIS_MODULE,
	.open = gyroscope_open,
	.read = gyroscope_read,
	.poll = gyroscope_poll,
};

static int gyro_misc_init(struct gyro_context *cxt)
{
	int err = 0;

	cxt->mdev.minor = ID_GYROSCOPE;
	cxt->mdev.name = GYRO_MISC_DEV_NAME;
	cxt->mdev.fops = &gyroscope_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register gyro misc device!!\n");

	return err;
}
DEVICE_ATTR_RW(gyroenablenodata);
DEVICE_ATTR_RW(gyroactive);
DEVICE_ATTR_RW(gyrobatch);
DEVICE_ATTR_RW(gyroflush);
DEVICE_ATTR_RW(gyrocali);
DEVICE_ATTR_RO(gyrodevnum);

static struct attribute *gyro_attributes[] = {
	&dev_attr_gyroenablenodata.attr,
	&dev_attr_gyroactive.attr,
	&dev_attr_gyrobatch.attr,
	&dev_attr_gyroflush.attr,
	&dev_attr_gyrocali.attr,
	&dev_attr_gyrodevnum.attr,
	NULL
};

static struct attribute_group gyro_attribute_group = {
	.attrs = gyro_attributes
};

int gyro_register_data_path(struct gyro_data_path *data)
{
	struct gyro_context *cxt = NULL;

	cxt = gyro_context_obj;
	cxt->gyro_data.get_data = data->get_data;
	cxt->gyro_data.get_temperature = data->get_temperature;
	cxt->gyro_data.vender_div = data->vender_div;
	cxt->gyro_data.get_raw_data = data->get_raw_data;
	pr_debug("gyro register data path vender_div: %d\n",
		 cxt->gyro_data.vender_div);
	if (cxt->gyro_data.get_data == NULL) {
		pr_debug("gyro register data path fail\n");
		return -1;
	}
	if (cxt->gyro_data.get_temperature == NULL)
		pr_debug("gyro not register temperature path\n");
	return 0;
}
EXPORT_SYMBOL_GPL(gyro_register_data_path);

int gyro_register_control_path(struct gyro_control_path *ctl)
{
	struct gyro_context *cxt = NULL;
	int err = 0;

	cxt = gyro_context_obj;
	cxt->gyro_ctl.set_delay = ctl->set_delay;
	cxt->gyro_ctl.open_report_data = ctl->open_report_data;
	cxt->gyro_ctl.enable_nodata = ctl->enable_nodata;
	cxt->gyro_ctl.batch = ctl->batch;
	cxt->gyro_ctl.flush = ctl->flush;
	cxt->gyro_ctl.set_cali = ctl->set_cali;
	cxt->gyro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->gyro_ctl.is_use_common_factory = ctl->is_use_common_factory;
	cxt->gyro_ctl.is_report_input_direct = ctl->is_report_input_direct;
	if (cxt->gyro_ctl.batch == NULL ||
	    cxt->gyro_ctl.open_report_data == NULL ||
	    cxt->gyro_ctl.enable_nodata == NULL) {
		pr_debug("gyro register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = gyro_misc_init(gyro_context_obj);
	if (err) {
		pr_info("unable to register gyro misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&gyro_context_obj->mdev.this_device->kobj,
				 &gyro_attribute_group);
	if (err < 0) {
		pr_info("unable to create gyro attribute file\n");
		return -3;
	}

	kobject_uevent(&gyro_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}
EXPORT_SYMBOL_GPL(gyro_register_control_path);

int x_t /* = 0*/;
int y_t /* = 0*/;
int z_t /* = 0*/;
long pc /* = 0*/;

static int check_repeat_data(int x, int y, int z)
{
	if ((x_t == x) && (y_t == y) && (z_t == z))
		pc++;
	else
		pc = 0;

	x_t = x;
	y_t = y;
	z_t = z;

	if (pc > 100) {
		pr_info("Gyro sensor output repeat data\n");
		pc = 0;
	}

	return 0;
}

int gyro_data_report(struct gyro_data *data)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	check_repeat_data(data->x, data->y, data->z);
	event.time_stamp = data->timestamp;
	event.flush_action = DATA_ACTION;
	event.status = data->status;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;
	event.word[3] = data->temperature;
	event.reserved = data->reserved[0];

	if (event.reserved == 1)
		mark_timestamp(ID_GYROSCOPE, DATA_REPORT, ktime_get_boot_ns(),
			       event.time_stamp);
	err = sensor_input_event(gyro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(gyro_data_report);

int gyro_bias_report(struct gyro_data *data)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = BIAS_ACTION;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;

	err = sensor_input_event(gyro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(gyro_bias_report);

int gyro_cali_report(struct gyro_data *data)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = CALI_ACTION;
	event.word[0] = data->x;
	event.word[1] = data->y;
	event.word[2] = data->z;

	err = sensor_input_event(gyro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(gyro_cali_report);

int gyro_temp_report(int32_t *temp)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = TEMP_ACTION;
	event.word[0] = temp[0];
	event.word[1] = temp[1];
	event.word[2] = temp[2];
	event.word[3] = temp[3];
	event.word[4] = temp[4];
	event.word[5] = temp[5];

	err = sensor_input_event(gyro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(gyro_temp_report);

int gyro_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	pr_debug_ratelimited("flush\n");
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(gyro_context_obj->mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(gyro_flush_report);

int gyro_probe(void)
{

	int err;

	pr_debug("%s +++!!\n", __func__);

	gyro_context_obj = gyro_context_alloc_object();
	if (!gyro_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real gyroeleration driver */
	err = gyro_real_driver_init(pltfm_dev);
	if (err) {
		pr_err("gyro real driver init fail\n");
		goto real_driver_init_fail;
	}
	pr_debug("%s OK !!\n", __func__);
	return 0;

real_driver_init_fail:
	kfree(gyro_context_obj);

exit_alloc_data_failed:
	pr_err("%s--- fail !!!\n", __func__);
	return err;
}
EXPORT_SYMBOL_GPL(gyro_probe);

int gyro_remove(void)
{
	int err = 0;

	sysfs_remove_group(&gyro_context_obj->mdev.this_device->kobj,
			   &gyro_attribute_group);
	err = sensor_attr_deregister(&gyro_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);

	kfree(gyro_context_obj);

	platform_driver_unregister(&gyroscope_driver);

	return 0;
}
EXPORT_SYMBOL_GPL(gyro_remove);

static int __init gyro_init(void)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static void __exit gyro_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(gyro_init);
module_exit(gyro_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GYROSCOPE device driver");
MODULE_AUTHOR("Mediatek");

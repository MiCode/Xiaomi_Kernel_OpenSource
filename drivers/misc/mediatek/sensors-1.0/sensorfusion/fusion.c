// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<FUSION> " fmt

#include "fusion.h"

static struct fusion_context *fusion_context_obj;


static struct fusion_init_info *fusion_init_list[max_fusion_support] = { 0 };

static struct fusion_context *fusion_context_alloc_object(void)
{
	int index = 0;
	struct fusion_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc fusion object error!\n");
		return NULL;
	}
	mutex_init(&obj->fusion_op_mutex);
	for (index = orientation; index < max_fusion_support; ++index) {
		obj->fusion_context[index].is_first_data_after_enable = false;
		obj->fusion_context[index].is_polling_run = false;
		obj->fusion_context[index].is_batch_enable = false;
		obj->fusion_context[index].power = 0;
		obj->fusion_context[index].enable = 0;
		obj->fusion_context[index].delay_ns = -1;
		obj->fusion_context[index].latency_ns = -1;
	}
	pr_debug("%s end\n", __func__);
	return obj;
}

static int handle_to_index(int handle)
{
	int index = -1;

	switch (handle) {
	case ID_ORIENTATION:
		index = orientation;
		break;
	case ID_GAME_ROTATION_VECTOR:
		index = grv;
		break;
	case ID_GEOMAGNETIC_ROTATION_VECTOR:
		index = gmrv;
		break;
	case ID_ROTATION_VECTOR:
		index = rv;
		break;
	case ID_LINEAR_ACCELERATION:
		index = la;
		break;
	case ID_GRAVITY:
		index = grav;
		break;
	case ID_ACCELEROMETER_UNCALIBRATED:
		index = unacc;
		break;
	case ID_GYROSCOPE_UNCALIBRATED:
		index = ungyro;
		break;
	case ID_MAGNETIC_UNCALIBRATED:
		index = unmag;
		break;
	case ID_PDR:
		index = pdr;
		break;
	case ID_GYRO_TEMPERATURE:
		index = ungyro_temperature;
		break;
	default:
		index = -1;
		pr_err("%s invalid handle:%d, index:%d\n", __func__,
			handle, index);
		return index;
	}
	return index;
}

#ifndef CONFIG_NANOHUB
static int fusion_enable_and_batch(int index)
{
	struct fusion_context *cxt = fusion_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->fusion_context[index].power == 1 &&
		cxt->fusion_context[index].enable == 0) {
		pr_debug("FUSION disable\n");
		/* turn off the power */
		err = cxt->fusion_context[index].fusion_ctl.enable_nodata(0);
		if (err) {
			pr_err("fusion turn off power err = %d\n", err);
			return -1;
		}

		cxt->fusion_context[index].power = 0;
		cxt->fusion_context[index].delay_ns = -1;
		return 0;
	}
	/* power off -> power on */
	if (cxt->fusion_context[index].power == 0 &&
		cxt->fusion_context[index].enable == 1) {
		pr_debug("FUSION power on\n");
		err = cxt->fusion_context[index].fusion_ctl.enable_nodata(1);
		if (err) {
			pr_err("fusion turn on power err = %d\n", err);
			return -1;
		}

		cxt->fusion_context[index].power = 1;
	}
	/* rate change */
	if (cxt->fusion_context[index].power == 1 &&
		cxt->fusion_context[index].delay_ns >= 0) {
		pr_debug("FUSION set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->fusion_context[index].fusion_ctl.is_support_batch)
			err = cxt->fusion_context[index].fusion_ctl.batch(0,
				cxt->fusion_context[index].delay_ns,
				cxt->fusion_context[index].latency_ns);
		else
			err = cxt->fusion_context[index].fusion_ctl.batch(0,
				cxt->fusion_context[index].delay_ns, 0);
		if (err) {
			pr_err("fusion set batch(ODR) err %d\n", err);
			return -1;
		}
	}
	return 0;
}
#endif

static ssize_t fusionactive_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fusion_context *cxt = fusion_context_obj;
	int err = 0, handle = -1, en = 0, index = -1;

	err = sscanf(buf, "%d,%d", &handle, &en);
	if (err < 0) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return err;
	}
	pr_debug("%s handle=%d, en=%d\n", __func__, handle, en);
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}

	if (cxt->fusion_context[index].fusion_ctl.enable_nodata == NULL) {
		pr_err("[%s] ctl not registered\n", __func__);
		return -1;
	}

	mutex_lock(&fusion_context_obj->fusion_op_mutex);
	if (en == 1)
		cxt->fusion_context[index].enable = 1;
	else if (en == 0)
		cxt->fusion_context[index].enable = 0;
	else {
		pr_err("%s error !!\n", __func__);
		err = -1;
		goto err_out;
	}

#ifdef CONFIG_NANOHUB
	if (cxt->fusion_context[index].enable == 1) {
		err = cxt->fusion_context[index].fusion_ctl.enable_nodata(1);
		if (err) {
			pr_err("fusion turn on power err = %d\n", err);
			goto err_out;
		}
	} else {
		err = cxt->fusion_context[index].fusion_ctl.enable_nodata(0);
		if (err) {
			pr_err("fusion turn off power err = %d\n", err);
			goto err_out;
		}
	}
#else
	err = fusion_enable_and_batch(index);
#endif
	pr_debug("%s done\n", __func__);
err_out:
	mutex_unlock(&fusion_context_obj->fusion_op_mutex);
	if (err)
		return err;
	else
		return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t fusionactive_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int vendor_div[max_fusion_support];
	int index = 0;
	struct fusion_context *cxt = fusion_context_obj;

	for (index = orientation; index < max_fusion_support; ++index) {
		vendor_div[index] =
			cxt->fusion_context[index].fusion_data.vender_div;
		pr_debug("fusion index:%d vender_div: %d\n",
			index, vendor_div[index]);
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		vendor_div[orientation], vendor_div[grv],
		vendor_div[gmrv], vendor_div[rv],
		vendor_div[la], vendor_div[grav], vendor_div[unacc],
		vendor_div[ungyro], vendor_div[unmag], vendor_div[pdr]);
}

static ssize_t fusiondevnum_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t fusionbatch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fusion_context *cxt = fusion_context_obj;
	int index = -1, handle = 0, flag = 0, err = 0;
	int64_t samplingPeriodNs = 0, maxBatchReportLatencyNs = 0;

	err = sscanf(buf, "%d,%d,%lld,%lld",
		&handle, &flag, &samplingPeriodNs, &maxBatchReportLatencyNs);
	if (err != 4) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return err;
	}
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}

	if (cxt->fusion_context[index].fusion_ctl.batch == NULL) {
		pr_debug("[%s] ctl not registered\n", __func__);
		return -1;
	}

	pr_debug("handle %d, flag:%d, PeriodNs:%lld, LatencyNs: %lld\n",
		handle, flag, samplingPeriodNs, maxBatchReportLatencyNs);

	cxt->fusion_context[index].delay_ns = samplingPeriodNs;
	cxt->fusion_context[index].latency_ns = maxBatchReportLatencyNs;

	mutex_lock(&fusion_context_obj->fusion_op_mutex);
#ifdef CONFIG_NANOHUB
	if (cxt->fusion_context[index].delay_ns >= 0) {
		if (cxt->fusion_context[index].fusion_ctl.is_support_batch)
			err = cxt->fusion_context[index].fusion_ctl.batch(0,
				cxt->fusion_context[index].delay_ns,
				cxt->fusion_context[index].latency_ns);
		else
			err = cxt->fusion_context[index].fusion_ctl.batch(0,
				cxt->fusion_context[index].delay_ns, 0);
		if (err) {
			pr_err("fusion set batch(ODR) err %d\n", err);
			goto err_out;
		}
	} else
		pr_info("batch state no need change\n");
#else
	err = fusion_enable_and_batch(index);
#endif
	pr_debug("%s done\n", __func__);
err_out:
	mutex_unlock(&fusion_context_obj->fusion_op_mutex);

	if (err)
		return err;
	else
		return count;
}

static ssize_t fusionbatch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t fusionflush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fusion_context *cxt = NULL;
	int index = -1, handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&fusion_context_obj->fusion_op_mutex);
	cxt = fusion_context_obj;
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		mutex_unlock(&fusion_context_obj->fusion_op_mutex);
		return  -1;
	}
	if (cxt->fusion_context[index].fusion_ctl.flush != NULL)
		err = cxt->fusion_context[index].fusion_ctl.flush();
	else
		pr_err("FUSION OLD ARCH NOT SUPPORT COMMON VRS FLUSH\n");
	if (err < 0)
		pr_err("fusion enable flush err %d\n", err);
	mutex_unlock(&fusion_context_obj->fusion_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t fusionflush_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static int fusion_real_driver_init(void)
{
	int index = 0;
	int err = -1;

	pr_debug("%s start\n", __func__);
	for (index = 0; index < max_fusion_support; index++) {
		pr_debug("index = %d\n", index);
		if (fusion_init_list[index] != NULL) {
			pr_debug("fusion try to init driver %s\n",
				fusion_init_list[index]->name);
			err = fusion_init_list[index]->init();
			if (err == 0)
				pr_debug("fusion real driver %s probe ok\n",
					fusion_init_list[index]->name);
		}
	}
	return err;
}

static int fusion_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t fusion_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(fusion_context_obj->mdev.minor,
		file, buffer, count, ppos);

	return read_cnt;
}

static unsigned int fusion_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(fusion_context_obj->mdev.minor, file, wait);
}

static const struct file_operations fusion_fops = {
	.owner = THIS_MODULE,
	.open = fusion_open,
	.read = fusion_read,
	.poll = fusion_poll,
};

static int fusion_misc_init(struct fusion_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = ID_GAME_ROTATION_VECTOR;
	cxt->mdev.name = FUSION_MISC_DEV_NAME;
	cxt->mdev.fops = &fusion_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register fusion misc device!!\n");

	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}

DEVICE_ATTR_RW(fusionactive);
DEVICE_ATTR_RW(fusionbatch);
DEVICE_ATTR_RW(fusionflush);
DEVICE_ATTR_RO(fusiondevnum);

static struct attribute *fusion_attributes[] = {
	&dev_attr_fusionactive.attr,
	&dev_attr_fusionbatch.attr,
	&dev_attr_fusionflush.attr,
	&dev_attr_fusiondevnum.attr,
	NULL
};

static struct attribute_group fusion_attribute_group = {
	.attrs = fusion_attributes
};

int fusion_register_data_path(struct fusion_data_path *data, int handle)
{
	int index = -1;
	struct fusion_context *cxt = NULL;

	if (data == NULL) {
		pr_err("fail\n");
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = fusion_context_obj;
	cxt->fusion_context[index].fusion_data.get_data = data->get_data;
	cxt->fusion_context[index].fusion_data.vender_div = data->vender_div;
	pr_debug("fusion handle:%d vender_div: %d\n",
		handle, cxt->fusion_context[index].fusion_data.vender_div);
	return 0;
}

int fusion_register_control_path(struct fusion_control_path *ctl,
	int handle)
{
	struct fusion_context *cxt = NULL;
	int index = -1;

	if (NULL == ctl || NULL == ctl->set_delay
		|| NULL == ctl->open_report_data
		|| NULL == ctl->enable_nodata
		|| NULL == ctl->batch || NULL == ctl->flush) {
		pr_err("fusion handle:%d register control path fail\n",
			handle);
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}

	cxt = fusion_context_obj;
	cxt->fusion_context[index].fusion_ctl.set_delay =
		ctl->set_delay;
	cxt->fusion_context[index].fusion_ctl.open_report_data =
		ctl->open_report_data;
	cxt->fusion_context[index].fusion_ctl.enable_nodata =
		ctl->enable_nodata;
	cxt->fusion_context[index].fusion_ctl.batch = ctl->batch;
	cxt->fusion_context[index].fusion_ctl.flush = ctl->flush;
	cxt->fusion_context[index].fusion_ctl.is_support_batch =
		ctl->is_support_batch;
	cxt->fusion_context[index].fusion_ctl.is_report_input_direct =
		ctl->is_report_input_direct;
	return 0;
}

static int fusion_data_report(int x, int y, int z,
	int scalar, int status, int64_t nt, int handle)
{
	/* pr_debug("+fusion_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = handle;
	event.flush_action = DATA_ACTION;
	event.time_stamp = nt;
	event.status = status;
	event.word[0] = x;
	event.word[1] = y;
	event.word[2] = z;
	event.word[3] = scalar;

	err = sensor_input_event(fusion_context_obj->mdev.minor, &event);
	return err;
}

static int fusion_flush_report(int handle)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));
	pr_debug("flush\n");
	event.handle = handle;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(fusion_context_obj->mdev.minor, &event);
	return err;
}
static int uncali_sensor_data_report(int *data,
	int status, int64_t nt, int handle)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = handle;
	event.flush_action = DATA_ACTION;
	event.time_stamp = nt;
	event.status = status;
	event.word[0] = data[0];
	event.word[1] = data[1];
	event.word[2] = data[2];
	event.word[3] = data[3];
	event.word[4] = data[4];
	event.word[5] = data[5];
	err = sensor_input_event(fusion_context_obj->mdev.minor, &event);
	return err;
}

static int uncali_sensor_flush_report(int handle)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));
	pr_debug_ratelimited("flush handle:%d\n", handle);
	event.handle = handle;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(fusion_context_obj->mdev.minor, &event);
	return err;
}

int rv_data_report(int x, int y, int z, int scalar, int status, int64_t nt)
{
	return fusion_data_report(x, y, z, scalar, status, nt,
		ID_ROTATION_VECTOR);
}
int rv_flush_report(void)
{
	return fusion_flush_report(ID_ROTATION_VECTOR);
}
int grv_data_report(int x, int y, int z, int scalar, int status, int64_t nt)
{
	return fusion_data_report(x, y, z, scalar, status, nt,
		ID_GAME_ROTATION_VECTOR);
}
int grv_flush_report(void)
{
	return fusion_flush_report(ID_GAME_ROTATION_VECTOR);
}
int gmrv_data_report(int x, int y, int z,
	int scalar, int status, int64_t nt)
{
	return fusion_data_report(x, y, z, scalar, status, nt,
		ID_GEOMAGNETIC_ROTATION_VECTOR);
}
int gmrv_flush_report(void)
{
	return fusion_flush_report(ID_GEOMAGNETIC_ROTATION_VECTOR);
}
int grav_data_report(int x, int y, int z, int status, int64_t nt)
{
	return fusion_data_report(x, y, z, 0, status, nt, ID_GRAVITY);
}
int grav_flush_report(void)
{
	return fusion_flush_report(ID_GRAVITY);
}
int la_data_report(int x, int y, int z, int status, int64_t nt)
{
	return fusion_data_report(x, y, z, 0, status, nt,
		ID_LINEAR_ACCELERATION);
}
int la_flush_report(void)
{
	return fusion_flush_report(ID_LINEAR_ACCELERATION);
}
int orientation_data_report(int x, int y, int z, int status, int64_t nt)
{
	return fusion_data_report(x, y, z, 0, status, nt, ID_ORIENTATION);
}
int orientation_flush_report(void)
{
	return fusion_flush_report(ID_ORIENTATION);
}
int uncali_acc_data_report(int *data, int status, int64_t nt)
{
	return uncali_sensor_data_report(data,
		status, nt, ID_ACCELEROMETER_UNCALIBRATED);
}
int uncali_acc_flush_report(void)
{
	return uncali_sensor_flush_report(ID_ACCELEROMETER_UNCALIBRATED);
}
int uncali_gyro_data_report(int *data, int status, int64_t nt)
{
	return uncali_sensor_data_report(data,
		status, nt, ID_GYROSCOPE_UNCALIBRATED);
}
int uncali_gyro_temperature_data_report(int *data, int status, int64_t nt)
{
	return uncali_sensor_data_report(data, status, nt, ID_GYRO_TEMPERATURE);
}
int uncali_gyro_temperature_flush_report(void)
{
	return uncali_sensor_flush_report(ID_GYRO_TEMPERATURE);
}
int uncali_gyro_flush_report(void)
{
	return uncali_sensor_flush_report(ID_GYROSCOPE_UNCALIBRATED);
}
int uncali_mag_data_report(int *data, int status, int64_t nt)
{
	return uncali_sensor_data_report(data,
		status, nt, ID_MAGNETIC_UNCALIBRATED);
}

int uncali_mag_flush_report(void)
{
	return uncali_sensor_flush_report(ID_MAGNETIC_UNCALIBRATED);
}
static int fusion_probe(void)
{
	int err;

	pr_debug("%s+++!!\n", __func__);

	fusion_context_obj = fusion_context_alloc_object();
	if (!fusion_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real fusioneleration driver */
	err = fusion_real_driver_init();
	if (err) {
		pr_err("fusion real driver init fail\n");
		goto real_driver_init_fail;
	}
	/* add misc dev for sensor hal control cmd */
	err = fusion_misc_init(fusion_context_obj);
	if (err) {
		pr_err("unable to register fusion misc device!!\n");
		goto real_driver_init_fail;
	}
	err = sysfs_create_group(&fusion_context_obj->mdev.this_device->kobj,
		&fusion_attribute_group);
	if (err < 0) {
		pr_err("unable to create fusion attribute file\n");
		goto real_driver_init_fail;
	}
	kobject_uevent(&fusion_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	pr_debug("%s--- OK !!\n", __func__);
	return 0;

real_driver_init_fail:
	kfree(fusion_context_obj);
exit_alloc_data_failed:
	pr_debug("%s---- fail !!!\n", __func__);
	return err;
}

static int fusion_remove(void)
{
	int err = 0;

	pr_debug("%s\n", __func__);

	sysfs_remove_group(&fusion_context_obj->mdev.this_device->kobj,
		&fusion_attribute_group);
	err = sensor_attr_deregister(&fusion_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);

	kfree(fusion_context_obj);

	return 0;
}
int fusion_driver_add(struct fusion_init_info *obj, int handle)
{
	int err = 0;
	int index = 0;

	pr_debug("handle:%d\n", handle);
	if (!obj) {
		pr_err("FUSION handle: %d, driver add fail\n", handle);
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return  -1;
	}

	if (fusion_init_list[index] == NULL)
		fusion_init_list[index] = obj;
	else
		pr_err("fusion_init_list handle:%d already exist\n",
			handle);
	return err;
}
static int __init fusion_init(void)
{
	pr_debug("%s\n", __func__);

	if (fusion_probe()) {
		pr_err("failed to register fusion driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit fusion_exit(void)
{
	fusion_remove();
}

late_initcall(fusion_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FUSION device driver");
MODULE_AUTHOR("Mediatek");

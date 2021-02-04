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

#define pr_fmt(fmt) "<SITUATION> " fmt

#include "situation.h"

static struct situation_context *situation_context_obj;

static struct situation_init_info *
	situation_init_list[max_situation_support] = {0};

static struct situation_context *situation_context_alloc_object(void)
{
	struct situation_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	int index;

	pr_debug("situation_context_alloc_object++++\n");
	if (!obj) {
		pr_err("Alloc situ object error!\n");
		return NULL;
	}
	mutex_init(&obj->situation_op_mutex);
	for (index = inpocket; index < max_situation_support; ++index) {
		obj->ctl_context[index].power = 0;
		obj->ctl_context[index].enable = 0;
		obj->ctl_context[index].delay_ns = -1;
		obj->ctl_context[index].latency_ns = -1;
	}

	pr_debug("situation_context_alloc_object----\n");
	return obj;
}

static int handle_to_index(int handle)
{
	int index = -1;

	switch (handle) {
	case ID_IN_POCKET:
		index = inpocket;
		break;
	case ID_STATIONARY_DETECT:
		index = stationary;
		break;
	case ID_WAKE_GESTURE:
		index = wake_gesture;
		break;
	case ID_GLANCE_GESTURE:
		index = glance_gesture;
		break;
	case ID_PICK_UP_GESTURE:
		index = pickup_gesture;
		break;
	case ID_ANSWER_CALL:
		index = answer_call;
		break;
	case ID_MOTION_DETECT:
		index = motion_detect;
		break;
	case ID_DEVICE_ORIENTATION:
		index = device_orientation;
		break;
	case ID_TILT_DETECTOR:
		index = tilt_detector;
		break;
	case ID_FLAT:
		index = flat;
		break;
	case ID_SAR:
		index = sar;
		break;
	default:
		index = -1;
		pr_err("handle_to_index invalid handle:%d,index:%d\n",
			handle, index);
		return index;
	}
	pr_debug("handle_to_index handle:%d, index:%d\n", handle, index);
	return index;
}

int situation_data_report(int handle, uint32_t one_sample_data)
{
	int err = 0, index = -1;
	struct sensor_event event;
	struct situation_context *cxt = situation_context_obj;

	memset(&event, 0, sizeof(struct sensor_event));
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err_ratelimited("[%s] invalid index\n", __func__);
		return -1;
	}

	pr_debug("situation_notify handle:%d, index:%d\n", handle, index);

	event.handle = handle;
	event.flush_action = DATA_ACTION;
	event.word[0] = one_sample_data;
	err = sensor_input_event(situation_context_obj->mdev.minor, &event);
	if (cxt->ctl_context[index].situation_ctl.open_report_data != NULL &&
		cxt->ctl_context[index].situation_ctl.is_support_wake_lock)
		__pm_wakeup_event(&cxt->ws[index], 250);
	return err;
}

int sar_data_report(int32_t value[3])
{
	int err = 0, index = -1;
	struct sensor_event event;
	struct situation_context *cxt = situation_context_obj;

	memset(&event, 0, sizeof(struct sensor_event));

	index = handle_to_index(ID_SAR);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return -1;
	}
	event.handle = ID_SAR;
	event.flush_action = DATA_ACTION;
	event.word[0] = value[0];
	event.word[1] = value[1];
	event.word[2] = value[2];
	err = sensor_input_event(situation_context_obj->mdev.minor, &event);
	if (cxt->ctl_context[index].situation_ctl.open_report_data != NULL &&
		cxt->ctl_context[index].situation_ctl.is_support_wake_lock)
		__pm_wakeup_event(&cxt->ws[index], 250);
	return err;
}

int situation_notify(int handle)
{
	return situation_data_report(handle, 1);
}

int situation_flush_report(int handle)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));
	pr_debug_ratelimited("flush, handle:%d\n", handle);
	event.handle = handle;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(situation_context_obj->mdev.minor, &event);
	return err;
}

#ifndef CONFIG_NANOHUB
static int situation_enable_and_batch(int index)
{
	struct situation_context *cxt = situation_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->ctl_context[index].power == 1 &&
		cxt->ctl_context[index].enable == 0) {
		pr_debug("SITUATION disable\n");
		/* turn off the power */
		err = cxt->ctl_context[index].situation_ctl.open_report_data(0);
		if (err) {
			pr_err("situation turn off power err = %d\n",
				err);
			return -1;
		}
		pr_debug("situation turn off power done\n");

		cxt->ctl_context[index].power = 0;
		cxt->ctl_context[index].delay_ns = -1;
		pr_debug("SITUATION disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->ctl_context[index].power == 0 &&
		cxt->ctl_context[index].enable == 1) {
		pr_debug("SITUATION power on\n");
		err = cxt->ctl_context[index].situation_ctl.open_report_data(1);
		if (err) {
			pr_err("situation turn on power err = %d\n",
				err);
			return -1;
		}
		pr_debug("situation turn on power done\n");

		cxt->ctl_context[index].power = 1;
		pr_debug("SITUATION power on done\n");
	}
	/* rate change */
	if (cxt->ctl_context[index].power == 1 &&
		cxt->ctl_context[index].delay_ns >= 0) {
		pr_debug("SITUATION set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->ctl_context[index].situation_ctl.is_support_batch)
			err = cxt->ctl_context[index].situation_ctl.batch(0,
				cxt->ctl_context[index].delay_ns,
				cxt->ctl_context[index].latency_ns);
		else
			err = cxt->ctl_context[index].situation_ctl.batch(0,
				cxt->ctl_context[index].delay_ns, 0);
		if (err) {
			pr_err("situation set batch(ODR) err %d\n",
				err);
			return -1;
		}
		pr_debug("situation set ODR, fifo latency done\n");
	}
	return 0;
}
#endif

static ssize_t situation_store_active(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct situation_context *cxt = situation_context_obj;
	int err = 0, handle = -1, en = 0, index = -1;

	err = sscanf(buf, "%d : %d", &handle, &en);
	if (err < 0) {
		pr_debug("situation_store_active param error: err = %d\n",
			err);
		return err;
	}
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return -1;
	}
	pr_debug("situation_store_active handle=%d, en=%d\n", handle, en);

	mutex_lock(&situation_context_obj->situation_op_mutex);
	if (en == 1)
		cxt->ctl_context[index].enable = 1;
	else if (en == 0)
		cxt->ctl_context[index].enable = 0;
	else {
		pr_err(" situation_store_active error !!\n");
		err = -1;
		goto err_out;
	}
#ifdef CONFIG_NANOHUB
	if (cxt->ctl_context[index].enable == 1) {
		if (cxt->ctl_context[index].situation_ctl.open_report_data
			== NULL) {
			pr_err("open_report_data() is NULL, %d\n", index);
			goto err_out;
		}
		err = cxt->ctl_context[index].situation_ctl.open_report_data(1);
		if (err) {
			pr_err("situation turn on power err = %d\n", err);
			goto err_out;
		}
	} else {
		if (cxt->ctl_context[index].situation_ctl.open_report_data
			== NULL) {
			pr_err("open_report_data() is NULL, %d\n", index);
			goto err_out;
		}
		err = cxt->ctl_context[index].situation_ctl.open_report_data(0);
		if (err) {
			pr_err("situation turn off power err = %d\n", err);
			goto err_out;
		}
	}
#else
	err = situation_enable_and_batch(index);
#endif
	pr_debug("situation_store_active done\n");
err_out:
	mutex_unlock(&situation_context_obj->situation_op_mutex);
	if (err)
		return err;
	else
		return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t situation_show_active(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct situation_context *cxt = NULL;
	int i;
	int s_len = 0;

	cxt = situation_context_obj;
	for (i = 0; i < max_situation_support; i++) {
		pr_debug("situ handle:%d active: %d\n",
			i, cxt->ctl_context[i].is_active_data);
		s_len += snprintf(buf + s_len, PAGE_SIZE, "id:%d, en:%d\n",
			i, cxt->ctl_context[i].is_active_data);
	}
	return s_len;
}

static ssize_t situation_store_batch(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct situation_context *cxt = situation_context_obj;
	int index = -1, handle = 0, flag = 0, err = 0;
	int64_t samplingPeriodNs = 0, maxBatchReportLatencyNs = 0;

	err = sscanf(buf, "%d,%d,%lld,%lld",
		&handle, &flag, &samplingPeriodNs, &maxBatchReportLatencyNs);
	if (err != 4) {
		pr_err("situation_store_batch param error: err =%d\n",
			err);
		return err;
	}
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	pr_debug("handle %d, flag:%d, Period:%lld, Latency: %lld\n",
		handle, flag, samplingPeriodNs, maxBatchReportLatencyNs);

	cxt->ctl_context[index].delay_ns = samplingPeriodNs;
	cxt->ctl_context[index].latency_ns = maxBatchReportLatencyNs;
	mutex_lock(&situation_context_obj->situation_op_mutex);
#ifdef CONFIG_NANOHUB
	if (cxt->ctl_context[index].delay_ns >= 0) {
		if (cxt->ctl_context[index].situation_ctl.batch == NULL) {
			pr_err("batch() is NULL, %d\n", index);
			goto err_out;
		}
		if (cxt->ctl_context[index].situation_ctl.is_support_batch)
			err = cxt->ctl_context[index].situation_ctl.batch(0,
				cxt->ctl_context[index].delay_ns,
				cxt->ctl_context[index].latency_ns);
		else
			err = cxt->ctl_context[index].situation_ctl.batch(0,
				cxt->ctl_context[index].delay_ns, 0);
		if (err) {
			pr_err("situation set batch(ODR) err %d\n", err);
			goto err_out;
		}
	} else
		pr_info("batch state no need change\n");
#else
	err = situation_enable_and_batch(index);
#endif
	pr_debug("%s done\n", __func__);
err_out:
	mutex_unlock(&situation_context_obj->situation_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t situation_show_batch(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t situation_store_flush(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct situation_context *cxt = NULL;
	int index = -1, handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("situation_store_flush param error: err=%d\n",
		err);

	pr_debug("situation_store_flush param: handle %d\n", handle);

	mutex_lock(&situation_context_obj->situation_op_mutex);
	cxt = situation_context_obj;
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		mutex_unlock(&situation_context_obj->situation_op_mutex);
		return  -1;
	}
	if (cxt->ctl_context[index].situation_ctl.flush != NULL)
		err = cxt->ctl_context[index].situation_ctl.flush();
	else
		pr_err("SITUATION OLD ARCH NOT SUPPORT COMM FLUSH\n");
	if (err < 0)
		pr_err("situation enable flush err %d\n", err);
	mutex_unlock(&situation_context_obj->situation_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t situation_show_flush(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t situation_show_devnum(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);	/* TODO: why +5? */
}


static int situation_real_driver_init(void)
{
	int err = -1, i = 0;

	pr_debug(" situation_real_driver_init +\n");

	for (i = 0; i < max_situation_support; i++) {
		if (situation_init_list[i] != NULL) {
			pr_debug(" situ try to init driver %s\n",
				situation_init_list[i]->name);
			err = situation_init_list[i]->init();
			if (err == 0)
				pr_debug(" situ real driver %s probe ok\n",
				situation_init_list[i]->name);
		} else
			continue;
	}
	return err;
}


int situation_driver_add(struct situation_init_info *obj, int handle)
{
	int err = 0;
	int index = -1;

	pr_debug("register situation handle=%d\n", handle);

	if (!obj) {
		pr_err("[%s] fail, situation_init_info is NULL\n",
			__func__);
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return  -1;
	}

	if (situation_init_list[index] == NULL)
		situation_init_list[index] = obj;

	return err;
}
EXPORT_SYMBOL_GPL(situation_driver_add);
static int situation_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t situation_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(situation_context_obj->mdev.minor,
		file, buffer, count, ppos);

	return read_cnt;
}

static unsigned int situation_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(situation_context_obj->mdev.minor, file, wait);
}

static const struct file_operations situation_fops = {
	.owner = THIS_MODULE,
	.open = situation_open,
	.read = situation_read,
	.poll = situation_poll,
};

static int situation_misc_init(struct situation_context *cxt)
{
	int err = 0;

	cxt->mdev.minor = ID_WAKE_GESTURE; /* MISC_DYNAMIC_MINOR; */
	cxt->mdev.name = SITU_MISC_DEV_NAME;
	cxt->mdev.fops = &situation_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register situ misc device!!\n");

	return err;
}

DEVICE_ATTR(situactive, 0644,
	situation_show_active, situation_store_active);
DEVICE_ATTR(situbatch, 0644, situation_show_batch, situation_store_batch);
DEVICE_ATTR(situflush, 0644, situation_show_flush, situation_store_flush);
DEVICE_ATTR(situdevnum, 0644, situation_show_devnum, NULL);


static struct attribute *situation_attributes[] = {
	&dev_attr_situactive.attr,
	&dev_attr_situbatch.attr,
	&dev_attr_situflush.attr,
	&dev_attr_situdevnum.attr,
	NULL
};

static struct attribute_group situation_attribute_group = {
	.attrs = situation_attributes
};

int situation_register_data_path(struct situation_data_path *data,
	int handle)
{
	struct situation_context *cxt = NULL;
	int index = -1;

	if (NULL == data || NULL == data->get_data) {
		pr_debug("situ register data path fail\n");
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = situation_context_obj;
	cxt->ctl_context[index].situation_data.get_data = data->get_data;

	return 0;
}

int situation_register_control_path(struct situation_control_path *ctl,
	int handle)
{
	struct situation_context *cxt = NULL;
	int index = -1;

	pr_debug("%s\n", __func__);
	if (NULL == ctl || NULL == ctl->open_report_data) {
		pr_debug("situ register control path fail\n");
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = situation_context_obj;
	cxt->ctl_context[index].situation_ctl.open_report_data =
		ctl->open_report_data;
	cxt->ctl_context[index].situation_ctl.batch = ctl->batch;
	cxt->ctl_context[index].situation_ctl.flush = ctl->flush;
	cxt->ctl_context[index].situation_ctl.is_support_wake_lock =
		ctl->is_support_wake_lock;
	cxt->ctl_context[index].situation_ctl.is_support_batch =
		ctl->is_support_batch;

	cxt->wake_lock_name[index] = kzalloc(64, GFP_KERNEL);
	if (!cxt->wake_lock_name[index])
		return -1;
	sprintf(cxt->wake_lock_name[index], "situation_wakelock-%d", index);
	wakeup_source_init(&cxt->ws[index], cxt->wake_lock_name[index]);

	return 0;
}

static int situation_probe(void)
{
	int err;

	pr_debug("+++++++++++++situation_probe!!\n");

	situation_context_obj = situation_context_alloc_object();
	if (!situation_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real situ driver */
	err = situation_real_driver_init();
	if (err) {
		pr_err("situ real driver init fail\n");
		goto real_driver_init_fail;
	}

	/* add misc dev for sensor hal control cmd */
	err = situation_misc_init(situation_context_obj);
	if (err) {
		pr_err("unable to register situ misc device!!\n");
		goto real_driver_init_fail;
	}
	err = sysfs_create_group(&situation_context_obj->mdev.this_device->kobj,
		&situation_attribute_group);
	if (err < 0) {
		pr_err("unable to create situ attribute file\n");
		goto real_driver_init_fail;
	}
	kobject_uevent(&situation_context_obj->mdev.this_device->kobj,
		KOBJ_ADD);


	pr_debug("----situation_probe OK !!\n");
	return 0;

real_driver_init_fail:
	kfree(situation_context_obj);
exit_alloc_data_failed:
	pr_debug("----situation_probe fail !!!\n");
	return err;
}

static int situation_remove(void)
{
	int err = 0;

	pr_debug("%s\n", __func__);
	sysfs_remove_group(&situation_context_obj->mdev.this_device->kobj,
		&situation_attribute_group);

	err = sensor_attr_deregister(&situation_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);

	kfree(situation_context_obj);
	return 0;
}

static int __init situation_init(void)
{
	pr_debug("%s\n", __func__);

	if (situation_probe()) {
		pr_err("failed to register situ driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit situation_exit(void)
{
	situation_remove();
}

late_initcall(situation_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("situation sensor driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");

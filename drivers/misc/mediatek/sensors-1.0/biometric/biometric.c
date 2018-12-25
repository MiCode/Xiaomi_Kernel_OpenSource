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

#include "inc/biometric.h"
#include <linux/kthread.h>

static struct biometric_context *biometric_context_obj;

static struct biometric_init_info
	*biometric_init_list[max_biometric_support] = {0};
static struct task_struct *bio_tsk;
static DECLARE_WAIT_QUEUE_HEAD(bio_thread_wq);

static int64_t getCurNS(void)
{
	int64_t ns;
	struct timespec time;

	time.tv_sec = time.tv_nsec = 0;
	get_monotonic_boottime(&time);
	ns = time.tv_sec * 1000000000LL + time.tv_nsec;

	return ns;
}

static int handle_to_index(int handle)
{
	int index = -1;

	switch (handle) {
	case ID_EKG:
		index = ekg;
		break;
	case ID_PPG1:
		index = ppg1;
		break;
	case ID_PPG2:
		index = ppg2;
		break;
	default:
		index = -1;
		pr_err("handle_to_index invalid handle:%d, index:%d\n",
			handle, index);
		return index;
	}

	return index;
}

static struct biometric_context *biometric_context_alloc_object(void)
{
	struct biometric_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	int index;

	pr_debug("biometric_context_alloc_object++++\n");
	if (!obj) {
		pr_err("Alloc biometric object error!\n");
		return NULL;
	}
	mutex_init(&obj->biometric_op_mutex);
	for (index = 0; index < max_biometric_support; ++index) {
		obj->ctl_context[index].power = 0;
		obj->ctl_context[index].enable = 0;
		obj->ctl_context[index].delay_ns = -1;
		obj->ctl_context[index].latency_ns = -1;
	}

	mutex_init(&obj->biometric_op_mutex);
	pr_debug("biometric_context_alloc_object----\n");
	return obj;
}

int data_report(int handle)
{
	struct sensor_event event;
	struct biometric_context *cxt = biometric_context_obj;
	u32 length;
	int i, err = 0;
	int64_t cur_time;
	int index = handle_to_index(handle);

	cur_time = getCurNS();
	if (cxt->ctl_context[index].read_time != 0) {
		if ((cur_time - cxt->ctl_context[index].read_time) > 300000000)
			pr_err("Timeout, %lld, %lld\n",
				cur_time, cxt->ctl_context[index].read_time);
	}
	cxt->ctl_context[index].read_time = cur_time;

	cxt->ctl_context[index].biometric_data.get_data(
		cxt->ctl_context[index].data,
		cxt->ctl_context[index].amb_data,
		cxt->ctl_context[index].agc_data,
		cxt->ctl_context[index].status, &length);

	for (i = 0; i < length; i++) {
		event.time_stamp = 0; /* data->timestamp; */
		event.handle = handle;
		event.flush_action = DATA_ACTION;
		event.word[0] = cxt->ctl_context[index].data[i];
		event.word[1] = cxt->ctl_context[index].sn;
		event.word[2] = cxt->ctl_context[index].amb_data[i];
		event.word[3] = cxt->ctl_context[index].agc_data[i];
		event.status = cxt->ctl_context[index].status[i];
		cxt->ctl_context[index].sn = (cxt->ctl_context[index].sn + 1) %
			10000000;

		err = sensor_input_event(biometric_context_obj->mdev.minor,
			&event);
		if (err < 0)
			pr_err_ratelimited("failed due to event buffer full\n");
	}
	return err;
}

int biometric_flush_report(int handle)
{
	struct sensor_event event;
	int err = 0;

	pr_debug("flush, handle:%d\n", handle);
	event.handle = handle;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(biometric_context_obj->mdev.minor, &event);
	if (err < 0)
		pr_err_ratelimited("failed due to event buffer full\n");
	return err;
}

static int bio_thread(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	struct biometric_context *cxt = NULL;
	int handle = (int64_t)arg;
	int index;

	cxt = biometric_context_obj;
	index = handle_to_index(handle);

	sched_setscheduler(current, SCHED_FIFO, &param);
	set_current_state(TASK_INTERRUPTIBLE);

	for (;;) {
		if (kthread_should_stop())
			break;
		wait_event_interruptible(bio_thread_wq,
			cxt->ctl_context[index].power);

		data_report(handle);

		msleep_interruptible(20);
	}
	return 0;
}

static int biometric_enable_and_batch(int index)
{
	struct biometric_context *cxt = biometric_context_obj;
	int err;

	/* power on -> power off */
	if (cxt->ctl_context[index].power == 1 &&
		cxt->ctl_context[index].enable == 0) {
		pr_debug("BIOMETRIC disable\n");
		/* turn off the power */
		err = cxt->ctl_context[index].biometric_ctl.open_report_data(0);
		if (err) {
			pr_err("biometric turn off power err = %d\n", err);
			return -1;
		}
		pr_debug("biometric turn off power done\n");

		cxt->ctl_context[index].power = 0;
		cxt->ctl_context[index].delay_ns = -1;
		pr_debug("BIOMETRIC disable done\n");
		return 0;
	}
	/* power off -> power on */
	if (cxt->ctl_context[index].power == 0 &&
		cxt->ctl_context[index].enable == 1) {
		pr_debug("BIOMETRIC power on\n");
		err = cxt->ctl_context[index].biometric_ctl.open_report_data(1);
		if (err) {
			pr_err("biometric turn on power err = %d\n", err);
			return -1;
		}
		pr_debug("biometric turn on power done\n");

		cxt->ctl_context[index].power = 1;
		cxt->ctl_context[index].sn = 0;
		cxt->ctl_context[index].read_time = 0;
		wake_up_interruptible(&bio_thread_wq);
		pr_debug("BIOMETRIC power on done\n");
	}
	/* rate change */
	if (cxt->ctl_context[index].power == 1 &&
		cxt->ctl_context[index].delay_ns >= 0) {
		pr_debug("BIOMETRIC set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->ctl_context[index].biometric_ctl.is_support_batch)
			err = cxt->ctl_context[index].biometric_ctl.batch(0,
				cxt->ctl_context[index].delay_ns,
				cxt->ctl_context[index].latency_ns);
		else
			err = cxt->ctl_context[index].biometric_ctl.batch(0,
				cxt->ctl_context[index].delay_ns, 0);
		if (err) {
			pr_err("biometric set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("BIOMETRIC batch done\n");
	}
	return 0;
}

static ssize_t biometric_store_active(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct biometric_context *cxt = biometric_context_obj;
	int err = 0, handle = -1, en = 0, index = -1;

	err = sscanf(buf, "%d : %d", &handle, &en);
	if (err < 0) {
		pr_debug("biometric_store_active param error: err = %d\n",
			err);
		return err;
	}
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return -1;
	}
	pr_debug("biometric_store_active handle=%d, en=%d\n", handle, en);

	mutex_lock(&biometric_context_obj->biometric_op_mutex);
	if (en == 1)
		cxt->ctl_context[index].enable = 1;
	else if (en == 0)
		cxt->ctl_context[index].enable = 0;
	else {
		pr_err(" biometric_store_active error !!\n");
		err = -1;
		goto err_out;
	}
	err = biometric_enable_and_batch(index);
	pr_debug("biometric_store_active done\n");
err_out:
	mutex_unlock(&biometric_context_obj->biometric_op_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/
static ssize_t biometric_show_active(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct biometric_context *cxt = NULL;
	int i;
	int s_len = 0;

	cxt = biometric_context_obj;
	for (i = 0; i < max_biometric_support; i++) {
		pr_debug("biometric handle:%d active: %d\n",
			i, cxt->ctl_context[i].is_active_data);
		s_len += snprintf(buf + s_len, PAGE_SIZE, "id:%d, en:%d\n",
			i, cxt->ctl_context[i].is_active_data);
	}
	return s_len;
}

static ssize_t biometric_store_batch(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct biometric_context *cxt = biometric_context_obj;
	int index = -1, handle = 0, flag = 0, err = 0;
	int64_t samplingPeriodNs = 0, maxBatchReportLatencyNs = 0;

	err = sscanf(buf, "%d,%d,%lld,%lld", &handle,
		&flag, &samplingPeriodNs, &maxBatchReportLatencyNs);
	if (err != 4) {
		pr_err("biometric_store_batch param error: err = %d\n", err);
		return err;
	}
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	pr_debug("handle %d, flag:%d %s:%lld, %s: %lld\n",
		handle, flag,
		"samplingPeriodNs",
		samplingPeriodNs,
		"maxBatchReportLatencyNs",
		maxBatchReportLatencyNs);
	cxt->ctl_context[index].delay_ns = samplingPeriodNs;
	cxt->ctl_context[index].latency_ns = maxBatchReportLatencyNs;
	mutex_lock(&biometric_context_obj->biometric_op_mutex);
	err = biometric_enable_and_batch(index);
	mutex_unlock(&biometric_context_obj->biometric_op_mutex);
	return err;
}

static ssize_t biometric_show_batch(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int len = 0;

	pr_debug("not support now\n");
	return len;
}

static ssize_t biometric_store_flush(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct biometric_context *cxt = NULL;
	int index = -1, handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("biometric_store_flush param error: err = %d\n", err);

	pr_debug("biometric_store_flush param: handle %d\n", handle);

	mutex_lock(&biometric_context_obj->biometric_op_mutex);
	cxt = biometric_context_obj;
	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		mutex_unlock(&biometric_context_obj->biometric_op_mutex);
		return  -1;
	}
	if (cxt->ctl_context[index].biometric_ctl.flush != NULL)
		err = cxt->ctl_context[index].biometric_ctl.flush();
	else {
		biometric_flush_report(handle);
		pr_err("BIOMETRIC DRIVER OLD ARCHITECTURE DON'T SUPPORT BIOMETRIC COMMON VERSION FLUSH\n");
	}
	if (err < 0)
		pr_err("biometric enable flush err %d\n", err);
	mutex_unlock(&biometric_context_obj->biometric_op_mutex);
	return err;
}

static ssize_t biometric_show_flush(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t biometric_show_devnum(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int biometric_remove(struct platform_device *pdev)
{
	pr_debug("biometric_remove\n");
	return 0;
}

static int biometric_probe(struct platform_device *pdev)
{
	pr_debug("biometric_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id biometric_of_match[] = {
	{ .compatible = "mediatek,biometric", },
	{},
};
#endif
static struct platform_driver biometric_driver = {
	.probe = biometric_probe,
	.remove = biometric_remove,
	.driver = {
		   .name = "biometric",
	#ifdef CONFIG_OF
		.of_match_table = biometric_of_match,
	#endif
	}
};

static int biometric_real_driver_init(void)
{
	int i = 0;
	int err = -1;

	pr_debug(" biometric_real_driver_init +\n");

	for (i = 0; i < max_biometric_support; i++) {
		if (biometric_init_list[i] != NULL) {
			pr_debug("biometric try to init driver %s\n",
				biometric_init_list[i]->name);
			err = biometric_init_list[i]->init();
			if (err == 0)
				pr_debug("biometric real driver %s probe ok\n",
					biometric_init_list[i]->name);
		} else
			continue;
	}
	return err;
}


int biometric_driver_add(struct biometric_init_info *obj, int handle)
{
	int err = 0;
	int index = -1;

	pr_debug("register biometric handle=%d\n", handle);

	if (!obj) {
		pr_err("[%s] fail, biometric_init_info is NULL\n", __func__);
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid index\n", __func__);
		return  -1;
	}

	if (platform_driver_register(&biometric_driver))
		pr_err("failed to register biometric driver already exist\n");

	if (biometric_init_list[index] == NULL) {
		biometric_init_list[index] = obj;
		obj->platform_diver_addr = &biometric_driver;
	}

	return err;
}
EXPORT_SYMBOL_GPL(biometric_driver_add);
static int biometric_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t biometric_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(biometric_context_obj->mdev.minor,
		file, buffer, count, ppos);

	return read_cnt;
}

static unsigned int biometric_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(biometric_context_obj->mdev.minor, file, wait);
}

static const struct file_operations biometric_fops = {
	.owner = THIS_MODULE,
	.open = biometric_open,
	.read = biometric_read,
	.poll = biometric_poll,
};

static int biometric_misc_init(struct biometric_context *cxt)
{
	int err = 0;

	cxt->mdev.minor = ID_EKG;/* MISC_DYNAMIC_MINOR; */
	cxt->mdev.name = BIO_MISC_DEV_NAME;
	cxt->mdev.fops = &biometric_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register biometric misc device!!\n");

	return err;
}

DEVICE_ATTR(bioactive, 0644, biometric_show_active, biometric_store_active);
DEVICE_ATTR(biobatch, 0644, biometric_show_batch, biometric_store_batch);
DEVICE_ATTR(bioflush, 0644, biometric_show_flush, biometric_store_flush);
DEVICE_ATTR(biodevnum, 0644, biometric_show_devnum, NULL);


static struct attribute *biometric_attributes[] = {
	&dev_attr_bioactive.attr,
	&dev_attr_biobatch.attr,
	&dev_attr_bioflush.attr,
	&dev_attr_biodevnum.attr,
	NULL
};

static struct attribute_group biometric_attribute_group = {
	.attrs = biometric_attributes
};

int biometric_register_data_path(struct biometric_data_path *data, int handle)
{
	struct biometric_context *cxt = NULL;
	int index = -1;

	if (NULL == data || NULL == data->get_data) {
		pr_debug("biometric register data path fail\n");
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = biometric_context_obj;
	cxt->ctl_context[index].biometric_data.get_data = data->get_data;

	return 0;
}

int biometric_register_control_path(struct biometric_control_path *ctl,
				     int handle)
{
	struct biometric_context *cxt = NULL;
	int index = -1;

	if (NULL == ctl || NULL == ctl->open_report_data) {
		pr_debug("biometric register control path fail\n");
		return -1;
	}

	index = handle_to_index(handle);
	if (index < 0) {
		pr_err("[%s] invalid handle\n", __func__);
		return -1;
	}
	cxt = biometric_context_obj;
	cxt->ctl_context[index].biometric_ctl.open_report_data =
		ctl->open_report_data;
	cxt->ctl_context[index].biometric_ctl.batch = ctl->batch;
	cxt->ctl_context[index].biometric_ctl.flush = ctl->flush;
	cxt->ctl_context[index].biometric_ctl.is_support_batch =
		ctl->is_support_batch;

	return 0;
}

static int bio_probe(void)
{
	int err;

	pr_debug("+++++++++++++bio_probe!!\n");

	biometric_context_obj = biometric_context_alloc_object();
	if (!biometric_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real biometric driver */
	err = biometric_real_driver_init();
	if (err) {
		pr_err("biometric real driver init fail\n");
		goto real_driver_init_fail;
	}

	init_waitqueue_head(&bio_thread_wq);
	bio_tsk = kthread_run(bio_thread, (void *)ID_EKG, "EKG");
	if (IS_ERR(bio_tsk))
		pr_err("create EKG thread fail\n");

	bio_tsk = kthread_run(bio_thread, (void *)ID_PPG1, "PPG1");
	if (IS_ERR(bio_tsk))
		pr_err("create PPG1 thread fail\n");

	bio_tsk = kthread_run(bio_thread, (void *)ID_PPG2, "PPG2");
	if (IS_ERR(bio_tsk))
		pr_err("create PPG2 thread fail\n");

	/* add misc dev for sensor hal control cmd */
	err = biometric_misc_init(biometric_context_obj);
	if (err) {
		pr_err("unable to register biometric misc device!!\n");
		goto real_driver_init_fail;
	}
	err = sysfs_create_group(&biometric_context_obj->mdev.this_device->kobj,
		&biometric_attribute_group);
	if (err < 0) {
		pr_err("unable to create biometric attribute file\n");
		goto real_driver_init_fail;
	}
	kobject_uevent(&biometric_context_obj->mdev.this_device->kobj,
		KOBJ_ADD);


	pr_debug("----bio_probe OK !!\n");
	return 0;

real_driver_init_fail:
	kfree(biometric_context_obj);
exit_alloc_data_failed:
	pr_debug("----bio_probe fail !!!\n");
	return err;
}

static int bio_remove(void)
{
	int err = 0;

	sysfs_remove_group(&biometric_context_obj->mdev.this_device->kobj,
		&biometric_attribute_group);

	err = sensor_attr_deregister(&biometric_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);

	kfree(biometric_context_obj);
	return 0;
}

static int __init biometric_init(void)
{
	if (bio_probe()) {
		pr_err("failed to register biometric driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit biometric_exit(void)
{
	bio_remove();
	platform_driver_unregister(&biometric_driver);
}

late_initcall(biometric_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("biometric sensor driver");
MODULE_AUTHOR("andrew.yang@mediatek.com");

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<STEP_COUNTER> " fmt

#include "step_counter.h"

static struct step_c_context *step_c_context_obj;
static struct step_c_init_info *
	step_counter_init_list[MAX_CHOOSE_STEP_C_NUM] = { 0 };

static void step_c_work_func(struct work_struct *work)
{

	struct step_c_context *cxt = NULL;
	uint32_t counter;
	uint32_t counter_floor_c;
	/* hwm_sensor_data sensor_data; */
	int status;
	int64_t nt;
	struct timespec time;
	int err = 0;

	cxt = step_c_context_obj;

	if (cxt->step_c_data.get_data == NULL)
		pr_debug("step_c driver not register data path\n");
	if (cxt->step_c_data.get_data_floor_c == NULL)
		pr_debug("floor_c driver not register data path\n");


	status = 0;
	counter = 0;
	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	/* add wake lock to make sure data can be read before system suspend */
	if ((cxt->is_active_data == true) &&
		(cxt->step_c_data.get_data != NULL))
		err = cxt->step_c_data.get_data(&counter, &status);

	if (err) {
		pr_err("get step_c data fails!!\n");
		goto step_c_loop;
	} else {
		{
			cxt->drv_data.counter = counter;
			cxt->drv_data.status = status;
		}
	}

	status = 0;
	counter_floor_c = 0;
	if ((cxt->is_floor_c_active_data == true) &&
		(cxt->step_c_data.get_data_floor_c != NULL))
		err = cxt->step_c_data.get_data_floor_c(&counter_floor_c,
							&status);

	if (err) {
		pr_err("get floor_c data fails!!\n");
		goto step_c_loop;
	} else {
		{
			cxt->drv_data.floor_counter = counter_floor_c;
			cxt->drv_data.floor_c_status = status;
		}
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.counter == STEP_C_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto step_c_loop;

		}
	}

	if (true == cxt->is_first_floor_c_data_after_enable) {
		cxt->is_first_floor_c_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.floor_counter == STEP_C_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto step_c_loop;

		}
	}

	/* report data to input device */
	/*pr_debug("step_c data[%d]\n", cxt->drv_data.counter);*/

	step_c_data_report(cxt->drv_data.counter, cxt->drv_data.status);
	floor_c_data_report(cxt->drv_data.floor_counter,
		cxt->drv_data.floor_c_status);

step_c_loop:
	if (true == cxt->is_polling_run) {
		mod_timer(&cxt->timer,
			jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
	}
}

static void step_c_poll(struct timer_list *t)
{
	struct step_c_context *obj = from_timer(obj, t, timer);

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct step_c_context *step_c_context_alloc_object(void)
{
	struct step_c_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc step_c object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 2000);	/*0.5Hz */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, step_c_work_func);
	timer_setup(&obj->timer, step_c_poll, 0);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->step_c_op_mutex);
	obj->is_step_c_batch_enable = false;	/* for batch mode init */
	obj->is_step_d_batch_enable = false;	/* for batch mode init */

	pr_debug("%s end\n", __func__);
	return obj;
}

int step_notify_t(enum STEP_NOTIFY_TYPE type, int64_t time_stamp)
{
	int err = 0;
	struct step_c_context *cxt = NULL;
	struct sensor_event event;

	memset(&event, 0, sizeof(struct sensor_event));

	cxt = step_c_context_obj;
	event.time_stamp = time_stamp;

	if (type == TYPE_STEP_DETECTOR) {
		event.flush_action = DATA_ACTION;
		event.handle = ID_STEP_DETECTOR;
		event.word[0] = 1;
		err = sensor_input_event(step_c_context_obj->mdev.minor,
			&event);

	}
	if (type == TYPE_SIGNIFICANT) {
		pr_debug("fwq TYPE_SIGNIFICANT notify\n");
		/* cxt->step_c_data.get_data_significant(&value); */
		event.flush_action = DATA_ACTION;
		event.handle = ID_SIGNIFICANT_MOTION;
		event.word[0] = 1;
		err = sensor_input_event(step_c_context_obj->mdev.minor,
			&event);
	}

	return err;
}
int step_notify(enum STEP_NOTIFY_TYPE type)
{
	return step_notify_t(type, 0);
}
static int step_d_real_enable(int enable)
{
	int err = 0;
	unsigned int i = 0;
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	if (enable == 1) {

		for (i = 0; i < 3; i++) {
			err = cxt->step_c_ctl.enable_step_detect(1);
			if (err == 0)
				break;
			else if (i == 2) {
				pr_err("step_d E(%d)err 3 = %d\n",
					enable, err);
			}
		}

		pr_debug("step_d real enable\n");
	}
	if (enable == 0) {

		err = cxt->step_c_ctl.enable_step_detect(0);
		if (err)
			pr_err("step_d enable(%d) err = %d\n",
				enable, err);
		pr_debug("step_d real disable\n");

	}

	return err;
}

static int significant_real_enable(int enable)
{
	int err = 0;
	unsigned int i = 0;
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	if (enable == 1) {

		for (i = 0; i < 3; i++) {
			err = cxt->step_c_ctl.enable_significant(1);
			if (err == 0)
				break;
			else if (i == 2) {
				pr_err("significant E(%d)err 3 = %d\n",
					enable, err);
			}
		}

		pr_debug("enable_significant real enable\n");
	}
	if (enable == 0) {
		err = cxt->step_c_ctl.enable_significant(0);
		if (err)
			pr_err("enable_significantenable(%d) err = %d\n",
				enable, err);
		pr_debug("enable_significant real disable\n");

	}
	return err;
}


static int step_c_real_enable(int enable)
{
	int err = 0;
	unsigned int i = 0;
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	if (enable == 1) {
		if (true == cxt->is_active_data ||
			true == cxt->is_active_nodata) {

			for (i = 0; i < 3; i++) {
				err = cxt->step_c_ctl.enable_nodata(1);
				if (err == 0)
					break;
				else if (i == 2) {
					pr_err("step_c E(%d)err 3 =%d\n",
						enable, err);
				}
			}

			pr_debug("step_c real enable\n");
		}
	}
	if (enable == 0) {
		if (false == cxt->is_active_data &&
			false == cxt->is_active_nodata) {
			err = cxt->step_c_ctl.enable_nodata(0);
			if (err)
				pr_err("step_c enable(%d) err = %d\n",
					enable, err);
			pr_debug("step_c real disable\n");
		}

	}

	return err;
}


static int floor_c_real_enable(int enable)
{
	int err = 0;
	unsigned int i = 0;
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	if (enable == 1) {
		for (i = 0; i < 3; i++) {
			err = cxt->step_c_ctl.enable_floor_c(1);
			if (err == 0)
				break;
			else if (i == 2)
				pr_err("floor_c enable(%d) err 3 = %d\n",
					enable, err);
		}

		pr_debug("floor_c real enable\n");
	}
	if (enable == 0) {
		err = cxt->step_c_ctl.enable_floor_c(0);
		if (err)
			pr_err("floor_c enable(%d) err = %d\n",
				enable, err);
		pr_debug("floor_c real disable\n");

	}

	return err;
}



static int step_c_enable_data(int enable)
{
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	if (cxt->step_c_ctl.open_report_data == NULL) {
		pr_err("no step_c control path\n");
		return -1;
	}

	if (enable == 1) {
		pr_debug("STEP_C enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->step_c_ctl.open_report_data(1);
		if (false == cxt->is_polling_run &&
			cxt->is_step_c_batch_enable == false) {
			if (false == cxt->step_c_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer, jiffies +
					atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (enable == 0) {
		pr_debug("STEP_C disable\n");
		cxt->is_active_data = false;
		cxt->step_c_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->step_c_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.counter = STEP_C_INVALID_VALUE;
			}
		}

	}
	step_c_real_enable(enable);
	return 0;
}


static int floor_c_enable_data(int enable)
{
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;

	if (enable == 1) {
		pr_debug("FLOOR_C enable data\n");
		cxt->is_floor_c_active_data = true;
		cxt->is_first_floor_c_data_after_enable = true;
		floor_c_real_enable(1);
		if (false == cxt->is_polling_run &&
			cxt->is_step_c_batch_enable == false) {
			if (false == cxt->step_c_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer, jiffies +
					atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (enable == 0) {
		pr_debug("FLOOR_C disable\n");
		cxt->is_floor_c_active_data = false;
		floor_c_real_enable(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->step_c_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.floor_counter =
					STEP_C_INVALID_VALUE;
			}
		}

	}
	return 0;
}



int step_c_enable_nodata(int enable)
{
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	if (cxt->step_c_ctl.enable_nodata == NULL) {
		pr_err("%s:step_c ctl path is NULL\n", __func__);
		return -1;
	}

	if (enable == 1)
		cxt->is_active_nodata = true;

	if (enable == 0)
		cxt->is_active_nodata = false;
	step_c_real_enable(enable);
	return 0;
}


static ssize_t step_cenablenodata_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}

static ssize_t step_cenablenodata_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int err = 0;
	struct step_c_context *cxt = NULL;

	pr_debug("step_c_store_enable nodata buf=%s\n", buf);
	mutex_lock(&step_c_context_obj->step_c_op_mutex);
	cxt = step_c_context_obj;
	if (cxt->step_c_ctl.enable_nodata == NULL) {
		pr_debug("step_c_ctl enable nodata NULL\n");
		mutex_unlock(&step_c_context_obj->step_c_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1))
		err = step_c_enable_nodata(1);
	else if (!strncmp(buf, "0", 1))
		err = step_c_enable_nodata(0);
	else
		pr_err(" step_c_store enable nodata cmd error !!\n");
	mutex_unlock(&step_c_context_obj->step_c_op_mutex);
	return err;
}

static ssize_t step_cactive_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct step_c_context *cxt = NULL;
	int res = 0;
	int handle = 0;
	int en = 0;

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&step_c_context_obj->step_c_op_mutex);

	cxt = step_c_context_obj;
	if (cxt->step_c_ctl.open_report_data == NULL) {
		pr_debug("step_c_ctl enable NULL\n");
		mutex_unlock(&step_c_context_obj->step_c_op_mutex);
		return count;
	}
	res = sscanf(buf, "%d,%d", &handle, &en);
	if (res != 2)
		pr_debug("%s param error: res = %d\n", __func__, res);
	pr_debug("%s handle=%d ,en=%d\n", __func__, handle, en);
	switch (handle) {
	case ID_STEP_COUNTER:
		if (en == 1)
			res = step_c_enable_data(1);
		else if (en == 0)
			res = step_c_enable_data(0);
		else
			pr_err("%s error !!\n", __func__);
		break;
	case ID_STEP_DETECTOR:
		if (en == 1)
			res = step_d_real_enable(1);
		else if (en == 0)
			res = step_d_real_enable(0);
		else
			pr_err(" step_d_real_enable error !!\n");
		break;
	case ID_SIGNIFICANT_MOTION:
		if (en == 1)
			res = significant_real_enable(1);
		else if (en == 0)
			res = significant_real_enable(0);
		else
			pr_err(" significant_real_enable error !!\n");
		break;
	case ID_FLOOR_COUNTER:
		if (en == 1)
			res = floor_c_enable_data(1);
		else if (en == 0)
			res = floor_c_enable_data(0);
		else
			pr_err(" fc_real_enable error !!\n");
		break;

	}
	mutex_unlock(&step_c_context_obj->step_c_op_mutex);
	pr_debug("%s done\n", __func__);
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t step_cactive_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct step_c_context *cxt = NULL;
	int div;

	cxt = step_c_context_obj;
	div = cxt->step_c_data.vender_div;
	pr_debug("step_c vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t step_cdelay_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int delay = 0, err = 0;
	int mdelay = 0;
	struct step_c_context *cxt = NULL;

	mutex_lock(&step_c_context_obj->step_c_op_mutex);
	cxt = step_c_context_obj;
	if (cxt->step_c_ctl.step_c_set_delay == NULL) {
		pr_debug("step_c_ctl step_c_set_delay NULL\n");
		mutex_unlock(&step_c_context_obj->step_c_op_mutex);
		return -1;
	}

	if (kstrtoint(buf, 10, &delay) != 0) {
		pr_err("invalid format!!\n");
		mutex_unlock(&step_c_context_obj->step_c_op_mutex);
		return -1;
	}

	if (false == cxt->step_c_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&step_c_context_obj->delay, mdelay);
	}
	err = cxt->step_c_ctl.step_c_set_delay(delay);
	pr_debug(" step_c_delay %d ns\n", delay);
	mutex_unlock(&step_c_context_obj->step_c_op_mutex);
	return err;

}

static ssize_t step_cdelay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	pr_debug(" not support now\n");
	return len;
}


static ssize_t step_cbatch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct step_c_context *cxt = NULL;
	int handle = 0, flag = 0, res = 0;
	int64_t samplingPeriodNs = 0, maxBatchReportLatencyNs = 0;

	res = sscanf(buf, "%d,%d,%lld,%lld",
		&handle, &flag, &samplingPeriodNs, &maxBatchReportLatencyNs);
	if (res != 4)
		pr_err("%s param error: err =%d\n", __func__, res);
	pr_debug("handle %d, flag:%d PeriodNs:%lld, LatencyNs: %lld\n",
		handle, flag, samplingPeriodNs, maxBatchReportLatencyNs);
	mutex_lock(&step_c_context_obj->step_c_op_mutex);
	cxt = step_c_context_obj;
	if (handle == ID_STEP_COUNTER) {
		if (!cxt->step_c_ctl.is_counter_support_batch)
			maxBatchReportLatencyNs = 0;
		if (cxt->step_c_ctl.step_c_batch != NULL)
			res = cxt->step_c_ctl.step_c_batch(flag,
				samplingPeriodNs, maxBatchReportLatencyNs);
		else
			pr_err("SUPPORT STEP COUNTER COM BATCH\n");
		if (res < 0)
			pr_err("step counter enable batch err %d\n",
				res);
	} else if (handle == ID_STEP_DETECTOR) {
		if (!cxt->step_c_ctl.is_detector_support_batch)
			maxBatchReportLatencyNs = 0;
		if (cxt->step_c_ctl.step_d_batch != NULL)
			res = cxt->step_c_ctl.step_d_batch(flag,
				samplingPeriodNs, maxBatchReportLatencyNs);
		else
			pr_err("NOT SUPPORT STEP DETECTOR COM BATCH\n");
		if (res < 0)
			pr_err("step detector enable batch err %d\n",
				res);
	} else if (handle == ID_SIGNIFICANT_MOTION) {
		if (!cxt->step_c_ctl.is_smd_support_batch)
			maxBatchReportLatencyNs = 0;

		if (cxt->step_c_ctl.smd_batch != NULL)
			res = cxt->step_c_ctl.smd_batch(flag,
			samplingPeriodNs, maxBatchReportLatencyNs);
		else
			pr_err("STEP SMD OLD NOT SUPPORT COM BATCH\n");
		if (res < 0)
			pr_err("step smd enable batch err %d\n", res);
	} else if (handle == ID_FLOOR_COUNTER) {
		if (!cxt->step_c_ctl.is_floor_c_support_batch)
			maxBatchReportLatencyNs = 0;

		if (cxt->step_c_ctl.floor_c_batch != NULL)
			res = cxt->step_c_ctl.floor_c_batch(flag,
				samplingPeriodNs, maxBatchReportLatencyNs);
		else
			pr_err("NOT SUPPORT FLOOR COUNT COM BATCH\n");
		if (res < 0)
			pr_err("floor count enable batch err %d\n", res);
	}
	mutex_unlock(&step_c_context_obj->step_c_op_mutex);
	pr_debug("%s done: %d\n", __func__, cxt->is_step_c_batch_enable);
	return res;
}

static ssize_t step_cbatch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t step_cflush_store(struct device *dev,
	struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct step_c_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&step_c_context_obj->step_c_op_mutex);
	cxt = step_c_context_obj;
	if (handle == ID_STEP_COUNTER) {
		if (cxt->step_c_ctl.step_c_flush != NULL)
			err = cxt->step_c_ctl.step_c_flush();
		else
			pr_err("NOT SUPPORT STEP COUNTER COM FLUSH\n");
		if (err < 0)
			pr_err("step counter enable flush err %d\n",
				err);
	} else if (handle == ID_STEP_DETECTOR) {
		if (cxt->step_c_ctl.step_d_flush != NULL)
			err = cxt->step_c_ctl.step_d_flush();
		else
			pr_err("NOT SUPPORT STEP DETECTOR COM FLUSH\n");
		if (err < 0)
			pr_err("step detector enable flush err %d\n",
				err);
	} else if (handle == ID_SIGNIFICANT_MOTION) {
		if (cxt->step_c_ctl.smd_flush != NULL)
			err = cxt->step_c_ctl.smd_flush();
		else
			pr_err("NOT SUPPORT SMD COMMON VERSION FLUSH\n");
		if (err < 0)
			pr_err("smd enable flush err %d\n", err);
	} else if (handle == ID_FLOOR_COUNTER) {
		if (cxt->step_c_ctl.floor_c_flush != NULL)
			err = cxt->step_c_ctl.floor_c_flush();
		else
			pr_err("NOT SUPPORT FLOOR COUNTER COM FLUSH\n");
		if (err < 0)
			pr_err("floor counter enable flush err %d\n",
				err);

	}
	mutex_unlock(&step_c_context_obj->step_c_op_mutex);
	return err;
}

static ssize_t step_cflush_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t step_cdevnum_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int step_counter_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int step_counter_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id step_counter_of_match[] = {
	{.compatible = "mediatek,step_counter",},
	{},
};
#endif

static struct platform_driver step_counter_driver = {
	.probe = step_counter_probe,
	.remove = step_counter_remove,
	.driver = {

		   .name = "step_counter",
#ifdef CONFIG_OF
		   .of_match_table = step_counter_of_match,
#endif
		}
};

static int step_c_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_STEP_C_NUM; i++) {
		pr_debug(" i=%d\n", i);
		if (step_counter_init_list[i] != 0) {
			pr_debug(" step_c try to init driver %s\n",
				   step_counter_init_list[i]->name);
			err = step_counter_init_list[i]->init();
			if (err == 0) {
				pr_debug(" step_c real driver %s probe ok\n",
					   step_counter_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_STEP_C_NUM) {
		pr_debug("%s fail\n", __func__);
		err = -1;
	}
	return err;
}

int step_c_driver_add(struct step_c_init_info *obj)
{
	int err = 0;
	int i = 0;

	pr_debug("%s\n", __func__);

	pr_debug("register step_counter driver for the first time\n");
	if (platform_driver_register(&step_counter_driver))
		pr_err("fail to register gensor driver already exist\n");
	for (i = 0; i < MAX_CHOOSE_STEP_C_NUM; i++) {
		if (step_counter_init_list[i] == NULL) {
			obj->platform_diver_addr = &step_counter_driver;
			step_counter_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_STEP_C_NUM) {
		pr_err("STEP_C driver add err\n");
		err = -1;
	}

	return err;
}

static int step_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t step_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(step_c_context_obj->mdev.minor,
		file, buffer, count, ppos);

	return read_cnt;
}

static unsigned int step_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(step_c_context_obj->mdev.minor, file, wait);
}

static const struct file_operations step_fops = {
	.owner = THIS_MODULE,
	.open = step_open,
	.read = step_read,
	.poll = step_poll,
};

static int step_c_misc_init(struct step_c_context *cxt)
{

	int err = 0;
	/* kernel-3.10\include\linux\Miscdevice.h */
	/* use MISC_DYNAMIC_MINOR exceed 64 */
	cxt->mdev.minor = ID_STEP_COUNTER;
	cxt->mdev.name = STEP_C_MISC_DEV_NAME;
	cxt->mdev.fops = &step_fops;
	err = sensor_attr_register(&cxt->mdev);
	if (err)
		pr_err("unable to register step_c misc device!!\n");
	return err;
}

DEVICE_ATTR_RW(step_cenablenodata);
DEVICE_ATTR_RW(step_cactive);
DEVICE_ATTR_RW(step_cdelay);
DEVICE_ATTR_RW(step_cbatch);
DEVICE_ATTR_RW(step_cflush);
DEVICE_ATTR_RO(step_cdevnum);


static struct attribute *step_c_attributes[] = {
	&dev_attr_step_cenablenodata.attr,
	&dev_attr_step_cactive.attr,
	&dev_attr_step_cdelay.attr,
	&dev_attr_step_cbatch.attr,
	&dev_attr_step_cflush.attr,
	&dev_attr_step_cdevnum.attr,
	NULL
};

static struct attribute_group step_c_attribute_group = {
	.attrs = step_c_attributes
};

int step_c_register_data_path(struct step_c_data_path *data)
{
	struct step_c_context *cxt = NULL;

	cxt = step_c_context_obj;
	cxt->step_c_data.get_data = data->get_data;
	cxt->step_c_data.vender_div = data->vender_div;
	cxt->step_c_data.get_data_significant = data->get_data_significant;
	cxt->step_c_data.get_data_step_d = data->get_data_step_d;
	cxt->step_c_data.get_data_floor_c = data->get_data_floor_c;
	pr_debug("step_c register data path vender_div: %d\n",
		cxt->step_c_data.vender_div);
	if (cxt->step_c_data.get_data == NULL
	    || cxt->step_c_data.get_data_significant == NULL
	    || cxt->step_c_data.get_data_step_d == NULL) {
		pr_debug("step_c register data path fail\n");
		return -1;
	}
	return 0;
}

int step_c_register_control_path(struct step_c_control_path *ctl)
{
	struct step_c_context *cxt = NULL;
	int err = 0;

	cxt = step_c_context_obj;
	cxt->step_c_ctl.step_c_set_delay = ctl->step_c_set_delay;
	cxt->step_c_ctl.step_d_set_delay = ctl->step_d_set_delay;
	cxt->step_c_ctl.floor_c_set_delay = ctl->floor_c_set_delay;
	cxt->step_c_ctl.open_report_data = ctl->open_report_data;
	cxt->step_c_ctl.enable_nodata = ctl->enable_nodata;
	cxt->step_c_ctl.step_c_batch = ctl->step_c_batch;
	cxt->step_c_ctl.step_c_flush = ctl->step_c_flush;
	cxt->step_c_ctl.step_d_batch = ctl->step_d_batch;
	cxt->step_c_ctl.step_d_flush = ctl->step_d_flush;
	cxt->step_c_ctl.smd_batch = ctl->smd_batch;
	cxt->step_c_ctl.smd_flush = ctl->smd_flush;
	cxt->step_c_ctl.floor_c_batch = ctl->floor_c_batch;
	cxt->step_c_ctl.floor_c_flush = ctl->floor_c_flush;
	cxt->step_c_ctl.is_counter_support_batch =
		ctl->is_counter_support_batch;
	cxt->step_c_ctl.is_detector_support_batch =
		ctl->is_detector_support_batch;
	cxt->step_c_ctl.is_smd_support_batch = ctl->is_smd_support_batch;
	cxt->step_c_ctl.is_floor_c_support_batch =
		ctl->is_floor_c_support_batch;
	cxt->step_c_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->step_c_ctl.enable_significant = ctl->enable_significant;
	cxt->step_c_ctl.enable_step_detect = ctl->enable_step_detect;
	cxt->step_c_ctl.enable_floor_c = ctl->enable_floor_c;

	if ((cxt->step_c_ctl.step_c_set_delay == NULL)
		|| (cxt->step_c_ctl.open_report_data == NULL)
		|| (cxt->step_c_ctl.enable_nodata == NULL)
		|| (cxt->step_c_ctl.step_d_set_delay == NULL)
		|| (cxt->step_c_ctl.enable_significant == NULL)
		|| (cxt->step_c_ctl.enable_step_detect == NULL)) {
		pr_debug("step_c register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = step_c_misc_init(step_c_context_obj);
	if (err) {
		pr_err("unable to register step_c misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&step_c_context_obj->mdev.this_device->kobj,
				 &step_c_attribute_group);
	if (err < 0) {
		pr_err("unable to create step_c attribute file\n");
		return -3;
	}

	kobject_uevent(&step_c_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int step_c_data_report_t(uint32_t new_counter, int status, int64_t time_stamp)
{
	int err = 0;
	struct sensor_event event;
	static uint32_t last_step_counter;

	memset(&event, 0, sizeof(struct sensor_event));
	event.time_stamp = time_stamp;
	if (last_step_counter != new_counter) {
		event.flush_action = DATA_ACTION;
		event.handle = ID_STEP_COUNTER;
		event.word[0] = new_counter;
		err = sensor_input_event(step_c_context_obj->mdev.minor,
			&event);
		if (err >= 0)
			last_step_counter = new_counter;
	}
	return err;
}
int step_c_data_report(uint32_t new_counter, int status)
{
	return step_c_data_report_t(new_counter, status, 0);
}

int floor_c_data_report_t(uint32_t new_counter, int status, int64_t time_stamp)
{
	int err = 0;
	struct sensor_event event;
	static uint32_t last_floor_counter;

	memset(&event, 0, sizeof(struct sensor_event));
	event.time_stamp = time_stamp;
	if (last_floor_counter != new_counter) {
		event.flush_action = DATA_ACTION;
		event.handle = ID_FLOOR_COUNTER;
		event.word[0] = new_counter;
		err = sensor_input_event(step_c_context_obj->mdev.minor,
			&event);
		if (err >= 0)
			last_floor_counter = new_counter;
	}
	return err;
}
int floor_c_data_report(uint32_t new_counter, int status)
{
	return floor_c_data_report_t(new_counter, status, 0);
}
int step_c_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = ID_STEP_COUNTER;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(step_c_context_obj->mdev.minor, &event);
	pr_debug_ratelimited("flush\n");
	return err;
}

int step_d_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = ID_STEP_DETECTOR;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(step_c_context_obj->mdev.minor, &event);
	pr_debug_ratelimited("flush\n");
	return err;
}

int smd_flush_report(void)
{
	return 0;
}

int floor_c_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = ID_FLOOR_COUNTER;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(step_c_context_obj->mdev.minor, &event);
	pr_debug_ratelimited("flush\n");
	return err;
}

static int step_c_probe(void)
{

	int err;

	pr_debug("%s+++!!\n", __func__);

	step_c_context_obj = step_c_context_alloc_object();
	if (!step_c_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real step_c driver */
	err = step_c_real_driver_init();
	if (err) {
		pr_err("step_c real driver init fail\n");
		goto real_driver_init_fail;
	}

	pr_debug("%s---- OK !!\n", __func__);
	return 0;
real_driver_init_fail:
	kfree(step_c_context_obj);
exit_alloc_data_failed:
	pr_debug("%s---- fail !!!\n", __func__);
	return err;
}

static int step_c_remove(void)
{

	int err = 0;

	pr_debug("%s\n", __func__);
	sysfs_remove_group(&step_c_context_obj->mdev.this_device->kobj,
		&step_c_attribute_group);

	err = sensor_attr_deregister(&step_c_context_obj->mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);
	kfree(step_c_context_obj);
	platform_driver_unregister(&step_counter_driver);

	return 0;
}

static int __init step_c_init(void)
{
	pr_debug("%s\n", __func__);

	if (step_c_probe()) {
		pr_err("failed to register step_c driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit step_c_exit(void)
{
	step_c_remove();
	platform_driver_unregister(&step_counter_driver);
}

late_initcall(step_c_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("STEP_CMETER device driver");
MODULE_AUTHOR("Mediatek");

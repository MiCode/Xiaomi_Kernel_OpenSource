// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<ALS/PS> " fmt

#include "inc/alsps.h"
#include "inc/aal_control.h"
struct alsps_context *alsps_context_obj /* = NULL*/;
struct platform_device *pltfm_dev;
int last_als_report_data = -1;

/* AAL default delay timer(nano seconds)*/
#define AAL_DELAY 200000000

static struct alsps_init_info *alsps_init_list[MAX_CHOOSE_ALSPS_NUM] = {0};

int als_data_report_t(int value, int status, int64_t time_stamp)
{
	int err = 0;
	struct alsps_context *cxt = NULL;
	struct sensor_event event;

	memset(&event, 0, sizeof(struct sensor_event));

	cxt = alsps_context_obj;
	event.time_stamp = time_stamp;
	/* pr_debug(" +als_data_report! %d, %d\n", value, status); */
	/* force trigger data update after sensor enable. */
	if (cxt->is_get_valid_als_data_after_enable == false) {
		event.handle = ID_LIGHT;
		event.flush_action = DATA_ACTION;
		event.word[0] = value + 1;
		err = sensor_input_event(cxt->als_mdev.minor, &event);
		cxt->is_get_valid_als_data_after_enable = true;
	}
	if (value != last_als_report_data) {
		event.handle = ID_LIGHT;
		event.flush_action = DATA_ACTION;
		event.word[0] = value;
		event.status = status;
		err = sensor_input_event(cxt->als_mdev.minor, &event);
		if (err >= 0)
			last_als_report_data = value;
	}
	return err;
}
EXPORT_SYMBOL_GPL(als_data_report_t);

int als_data_report(int value, int status)
{
	return als_data_report_t(value, status, 0);
}
EXPORT_SYMBOL_GPL(als_data_report);

int als_cali_report(int *value)
{
	int err = 0;
	struct sensor_event event;

	memset(&event, 0, sizeof(struct sensor_event));
	event.handle = ID_LIGHT;
	event.flush_action = CALI_ACTION;
	event.word[0] = value[0];
	err = sensor_input_event(alsps_context_obj->als_mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(als_cali_report);

int als_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = ID_LIGHT;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(alsps_context_obj->als_mdev.minor, &event);
	pr_debug_ratelimited("flush\n");
	return err;
}
EXPORT_SYMBOL_GPL(als_flush_report);

int rgbw_data_report_t(int *value, int64_t time_stamp)
{
	int err = 0;
	struct alsps_context *cxt = alsps_context_obj;
	struct sensor_event event;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = ID_RGBW;
	event.flush_action = DATA_ACTION;
	event.time_stamp = time_stamp;
	event.word[0] = value[0];
	event.word[1] = value[1];
	event.word[2] = value[2];
	event.word[3] = value[3];
	err = sensor_input_event(cxt->als_mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(rgbw_data_report_t);

int rgbw_data_report(int *value)
{
	return rgbw_data_report_t(value, 0);
}
int rgbw_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.handle = ID_RGBW;
	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(alsps_context_obj->als_mdev.minor, &event);
	pr_debug_ratelimited("flush\n");
	return err;
}
EXPORT_SYMBOL_GPL(rgbw_flush_report);

int ps_data_report_t(int value, int status, int64_t time_stamp)
{
	int err = 0;
	struct sensor_event event;

	memset(&event, 0, sizeof(struct sensor_event));

	pr_notice("[ALS/PS]%s! %d, %d\n", __func__, value, status);
	event.flush_action = DATA_ACTION;
	event.time_stamp = time_stamp;
	event.word[0] = value + 1;
	event.status = status;
	err = sensor_input_event(alsps_context_obj->ps_mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(ps_data_report_t);

int ps_data_report(int value, int status)
{
	return ps_data_report_t(value, status, 0);
}
EXPORT_SYMBOL_GPL(ps_data_report);

int ps_cali_report(int *value)
{
	int err = 0;
	struct sensor_event event;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = CALI_ACTION;
	event.word[0] = value[0];
	event.word[1] = value[1];
	err = sensor_input_event(alsps_context_obj->ps_mdev.minor, &event);
	return err;
}
EXPORT_SYMBOL_GPL(ps_cali_report);

int ps_flush_report(void)
{
	struct sensor_event event;
	int err = 0;

	memset(&event, 0, sizeof(struct sensor_event));

	event.flush_action = FLUSH_ACTION;
	err = sensor_input_event(alsps_context_obj->ps_mdev.minor, &event);
	pr_debug_ratelimited("flush\n");
	return err;
}
EXPORT_SYMBOL_GPL(ps_flush_report);

static void als_work_func(struct work_struct *work)
{
	struct alsps_context *cxt = NULL;
	int value, status;
	int64_t nt;
	struct timespec time;
	int err;

	cxt = alsps_context_obj;
	if (cxt->als_data.get_data == NULL) {
		pr_err("alsps driver not register data path\n");
		return;
	}

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;
	/* add wake lock to make sure data can be read before system suspend */
	err = cxt->als_data.get_data(&value, &status);
	if (err) {
		pr_err("get alsps data fails!!\n");
		goto als_loop;
	} else {
		cxt->drv_data.als_data.values[0] = value;
		cxt->drv_data.als_data.status = status;
		cxt->drv_data.als_data.time = nt;
	}

	if (true == cxt->is_als_first_data_after_enable) {
		cxt->is_als_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.als_data.values[0] == ALSPS_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto als_loop;
		}
	}
	/* pr_debug(" als data[%d]\n" , cxt->drv_data.als_data.values[0]); */
	als_data_report(cxt->drv_data.als_data.values[0],
			cxt->drv_data.als_data.status);

als_loop:
	if (true == cxt->is_als_polling_run)
		mod_timer(&cxt->timer_als,
			  jiffies + atomic_read(&cxt->delay_als) / (1000 / HZ));
}

static void ps_work_func(struct work_struct *work)
{

	struct alsps_context *cxt = NULL;
	int value, status;
	int64_t nt;
	struct timespec time;
	int err = 0;

	cxt = alsps_context_obj;
	if (cxt->ps_data.get_data == NULL) {
		pr_err("alsps driver not register data path\n");
		return;
	}

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	/* add wake lock to make sure data can be read before system suspend */
	err = cxt->ps_data.get_data(&value, &status);
	if (err) {
		pr_err("get alsps data fails!!\n");
		goto ps_loop;
	} else {
		cxt->drv_data.ps_data.values[0] = value;
		cxt->drv_data.ps_data.status = status;
		cxt->drv_data.ps_data.time = nt;
	}

	if (true == cxt->is_ps_first_data_after_enable) {
		cxt->is_ps_first_data_after_enable = false;
		/* filter -1 value */
		if (cxt->drv_data.ps_data.values[0] == ALSPS_INVALID_VALUE) {
			pr_debug(" read invalid data\n");
			goto ps_loop;
		}
	}

	if (cxt->is_get_valid_ps_data_after_enable == false) {
		if (cxt->drv_data.ps_data.values[0] != ALSPS_INVALID_VALUE)
			cxt->is_get_valid_ps_data_after_enable = true;
	}

	ps_data_report(cxt->drv_data.ps_data.values[0],
		       cxt->drv_data.ps_data.status);

ps_loop:
	if (true == cxt->is_ps_polling_run) {
		if (cxt->ps_ctl.is_polling_mode ||
		    (cxt->is_get_valid_ps_data_after_enable == false))
			mod_timer(&cxt->timer_ps,
				  jiffies +
					  atomic_read(&cxt->delay_ps) /
						  (1000 / HZ));
	}
}

static void als_poll(struct timer_list *t)
{
	struct alsps_context *obj = from_timer(obj, t, timer_als);

	if ((obj != NULL) && (obj->is_als_polling_run))
		schedule_work(&obj->report_als);
}

static void ps_poll(struct timer_list *t)
{
	struct alsps_context *obj = from_timer(obj, t, timer_ps);

	if (obj != NULL)
		schedule_work(&obj->report_ps);
}

static struct alsps_context *alsps_context_alloc_object(void)
{
	struct alsps_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	pr_debug("%s start\n", __func__);
	if (!obj) {
		pr_err("Alloc alsps object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay_als,
		   200); /*5Hz, set work queue delay time 200ms */
	atomic_set(&obj->delay_ps,
		   200); /* 5Hz,  set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report_als, als_work_func);
	INIT_WORK(&obj->report_ps, ps_work_func);
	timer_setup(&obj->timer_als, als_poll, 0);
	timer_setup(&obj->timer_ps, ps_poll, 0);
	obj->timer_als.expires =
		jiffies + atomic_read(&obj->delay_als) / (1000 / HZ);

	obj->timer_ps.expires =
		jiffies + atomic_read(&obj->delay_ps) / (1000 / HZ);

	obj->is_als_first_data_after_enable = false;
	obj->is_als_polling_run = false;
	obj->is_ps_first_data_after_enable = false;
	obj->is_ps_polling_run = false;
	mutex_init(&obj->alsps_op_mutex);
	obj->is_als_batch_enable = false; /* for batch mode init */
	obj->is_ps_batch_enable = false;  /* for batch mode init */
	obj->als_power = 0;
	obj->als_enable = 0;
	obj->als_delay_ns = -1;
	obj->als_latency_ns = -1;
	obj->ps_power = 0;
	obj->ps_enable = 0;
	obj->ps_delay_ns = -1;
	obj->ps_latency_ns = -1;

	pr_debug("%s end\n", __func__);
	return obj;
}

#if !defined(CONFIG_NANOHUB) || !defined(CONFIG_MTK_ALSPSHUB)
static int als_enable_and_batch(void)
{
	struct alsps_context *cxt = alsps_context_obj;
	int err;

	/* als_power on -> power off */
	if (cxt->als_power == 1 && cxt->als_enable == 0) {
		pr_debug("ALSPS disable\n");
		/* stop polling firstly, if needed */
		if (cxt->als_ctl.is_report_input_direct == false &&
		    cxt->is_als_polling_run == true) {
			smp_mb(); /* for memory barrier */
			del_timer_sync(&cxt->timer_als);
			smp_mb(); /* for memory barrier */
			cancel_work_sync(&cxt->report_als);
			cxt->drv_data.als_data.values[0] = ALSPS_INVALID_VALUE;
			cxt->is_als_polling_run = false;
			pr_debug("als stop polling done\n");
		}
		/* turn off the als_power */
		err = cxt->als_ctl.enable_nodata(0);
		if (err) {
			pr_err("als turn off als_power err = %d\n", err);
			return -1;
		}
		pr_debug("als turn off als_power done\n");

		cxt->als_power = 0;
		cxt->als_delay_ns = -1;
		pr_debug("ALSPS disable done\n");
		return 0;
	}
	/* als_power off -> power on */
	if (cxt->als_power == 0 && cxt->als_enable == 1) {
		pr_debug("ALSPS als_power on\n");
		err = cxt->als_ctl.enable_nodata(1);
		if (err) {
			pr_err("als turn on als_power err = %d\n", err);
			return -1;
		}
		pr_debug("als turn on als_power done\n");

		cxt->als_power = 1;
		pr_debug("ALSPS als_power on done\n");
	}
	/* rate change */
	if (cxt->als_power == 1 && cxt->als_delay_ns >= 0) {
		pr_debug("ALSPS set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->als_ctl.is_support_batch)
			err = cxt->als_ctl.batch(0, cxt->als_delay_ns,
						 cxt->als_latency_ns);
		else
			err = cxt->als_ctl.batch(0, cxt->als_delay_ns, 0);
		if (err) {
			pr_err("als set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("als set ODR, fifo latency done\n");
		/* start polling, if needed */
		if (cxt->als_ctl.is_report_input_direct == false) {
			uint64_t mdelay = cxt->als_delay_ns;

			do_div(mdelay, 1000000);
			/* defaut max polling delay */
			if (mdelay < 10)
				mdelay = 10;
			atomic_set(&cxt->delay_als, mdelay);
			/* the first sensor start polling timer */
			if (cxt->is_als_polling_run == false) {
				mod_timer(&cxt->timer_als,
					  jiffies +
						  atomic_read(&cxt->delay_als) /
							  (1000 / HZ));
				cxt->is_als_polling_run = true;
				cxt->is_als_first_data_after_enable = true;
			}
			pr_debug("als set polling delay %d ms\n",
				  atomic_read(&cxt->delay_als));
		}
		pr_debug("ALSPS batch done\n");
	}
	return 0;
}
#endif

static ssize_t alsactive_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct alsps_context *cxt = alsps_context_obj;
	int err = 0, handle = -1, en = 0;

	err = sscanf(buf, "%d,%d", &handle, &en);
	if (err < 0) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return err;
	}

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&alsps_context_obj->alsps_op_mutex);
	if (handle == ID_LIGHT) {
		if (en) {
			cxt->als_enable = 1;
			last_als_report_data = -1;
		} else if (!en) {
			cxt->als_enable = 0;
		} else {
			pr_err("alsps_store_active error !!\n");
			err = -1;
			goto err_out;
		}
#if defined(CONFIG_NANOHUB) && defined(CONFIG_MTK_ALSPSHUB)
		if (cxt->als_enable) {
			err = cxt->als_ctl.enable_nodata(cxt->als_enable);
			if (err) {
				pr_err("als turn on err = %d\n", err);
				goto err_out;
			}
		} else {
			err = cxt->als_ctl.enable_nodata(cxt->als_enable);
			if (err) {
				pr_err("als turn off err = %d\n", err);
				goto err_out;
			}
		}
#else
		err = als_enable_and_batch();
#endif
	} else if (handle == ID_RGBW) {
		if (en)
			cxt->rgbw_enable = 1;
		else if (!en)
			cxt->rgbw_enable = 0;
		else {
			pr_err("alsps_store_active error !!\n");
			err = -1;
			goto err_out;
		}
#if defined(CONFIG_NANOHUB) && defined(CONFIG_MTK_ALSPSHUB)
		if (cxt->rgbw_enable) {
			err = cxt->als_ctl.rgbw_enable(cxt->rgbw_enable);
			if (err) {
				pr_err("rgbw turn on err = %d\n", err);
				goto err_out;
			}
		} else {
			err = cxt->als_ctl.rgbw_enable(cxt->rgbw_enable);
			if (err) {
				pr_err("rgbw turn off err = %d\n", err);
				goto err_out;
			}
		}
#endif
	}

err_out:
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	pr_debug("%s done\n", __func__);
	if (err)
		return err;
	else
		return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t alsactive_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct alsps_context *cxt = NULL;
	int div = 0;

	cxt = alsps_context_obj;
	div = cxt->als_data.vender_div;
	pr_debug("als vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t alsbatch_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct alsps_context *cxt = alsps_context_obj;
	int handle = 0, flag = 0, err = 0;
	int64_t delay_ns = 0;
	int64_t latency_ns = 0;

	pr_debug("%s %s\n", __func__, buf);
	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->als_delay_ns,
		     &cxt->als_latency_ns);
	if (err != 4) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return -1;
	}

	mutex_lock(&alsps_context_obj->alsps_op_mutex);
	if (handle == ID_LIGHT) {
#if defined(CONFIG_NANOHUB) && defined(CONFIG_MTK_ALSPSHUB)
		if (cxt->als_ctl.is_support_batch)
			err = cxt->als_ctl.batch(0, cxt->als_delay_ns,
				cxt->als_latency_ns);
		else
			err = cxt->als_ctl.batch(0, cxt->als_delay_ns, 0);
#else
		err = als_enable_and_batch();
#endif
	} else if (handle == ID_RGBW) {
		cxt->rgbw_delay_ns = delay_ns;
		cxt->rgbw_latency_ns = latency_ns;
#if defined(CONFIG_NANOHUB) && defined(CONFIG_MTK_ALSPSHUB)
		if (cxt->als_ctl.is_support_batch)
			err = cxt->als_ctl.rgbw_batch(0, cxt->rgbw_delay_ns,
				cxt->rgbw_latency_ns);
		else
			err = cxt->als_ctl.rgbw_batch(0, cxt->rgbw_delay_ns, 0);
#endif
	}
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	pr_debug("%s done: %d\n", __func__, cxt->is_als_batch_enable);
	if (err)
		return err;
	else
		return count;
}

static ssize_t alsbatch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t alsflush_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct alsps_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&alsps_context_obj->alsps_op_mutex);
	cxt = alsps_context_obj;
	if (handle == ID_LIGHT) {
		if (cxt->als_ctl.flush != NULL)
			err = cxt->als_ctl.flush();
		else
			pr_err("DON'T SUPPORT ALS COMMON VERSION FLUSH\n");
		if (err < 0)
			pr_err("als enable flush err %d\n", err);
	} else if (handle == ID_RGBW) {
		if (cxt->als_ctl.rgbw_flush != NULL)
			err = cxt->als_ctl.rgbw_flush();
		else
			pr_err("DON'T SUPPORT RGB COMMON VERSION FLUSH\n");
		if (err < 0)
			pr_err("rgbw enable flush err %d\n", err);
	}
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t alsflush_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
/* need work around again */
static ssize_t alsdevnum_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static ssize_t alscali_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct alsps_context *cxt = NULL;
	int err = 0;
	uint8_t *cali_buf = NULL;

	cali_buf = vzalloc(count);
	if (!cali_buf)
		return -ENOMEM;
	memcpy(cali_buf, buf, count);

	mutex_lock(&alsps_context_obj->alsps_op_mutex);
	cxt = alsps_context_obj;
	if (cxt->als_ctl.set_cali != NULL)
		err = cxt->als_ctl.set_cali(cali_buf, count);
	if (err < 0)
		pr_err("als set cali err %d\n", err);
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	vfree(cali_buf);
	return count;
}

#if !defined(CONFIG_NANOHUB) || !defined(CONFIG_MTK_ALSPSHUB)
static int ps_enable_and_batch(void)
{
	struct alsps_context *cxt = alsps_context_obj;
	int err;

	/* ps_power on -> power off */
	if (cxt->ps_power == 1 && cxt->ps_enable == 0) {
		pr_debug("PS disable\n");
/* stop polling firstly, if needed */
/*
 *		if (cxt->ps_ctl.is_report_input_direct == false
 *			&& cxt->is_ps_polling_run == true) {
 *			smp_mb();// for memory barrier
 *			del_timer_sync(&cxt->timer_ps);
 *			smp_mb();// for memory barrier
 *			cancel_work_sync(&cxt->report_ps);
 *			cxt->drv_data.ps_data.values[0] = ALSPS_INVALID_VALUE;
 *			cxt->is_ps_polling_run = false;
 *			pr_debug("ps stop polling done\n");
 *		}
 */
		/* turn off the ps_power */
		err = cxt->ps_ctl.enable_nodata(0);
		if (err) {
			pr_err("ps turn off ps_power err = %d\n", err);
			return -1;
		}
		pr_debug("ps turn off ps_power done\n");

		cxt->ps_power = 0;
		cxt->ps_delay_ns = -1;
		pr_debug("PS disable done\n");
		return 0;
	}
	/* ps_power off -> power on */
	if (cxt->ps_power == 0 && cxt->ps_enable == 1) {
		pr_debug("PS ps_power on\n");
		err = cxt->ps_ctl.enable_nodata(1);
		if (err) {
			pr_err("ps turn on ps_power err = %d\n", err);
			return -1;
		}
		pr_debug("ps turn on ps_power done\n");

		cxt->ps_power = 1;
		pr_debug("PS ps_power on done\n");
	}
	/* rate change */
	if (cxt->ps_power == 1 && cxt->ps_delay_ns >= 0) {
		pr_debug("PS set batch\n");
		/* set ODR, fifo timeout latency */
		if (cxt->ps_ctl.is_support_batch)
			err = cxt->ps_ctl.batch(0, cxt->ps_delay_ns,
						cxt->ps_latency_ns);
		else
			err = cxt->ps_ctl.batch(0, cxt->ps_delay_ns, 0);
		if (err) {
			pr_err("ps set batch(ODR) err %d\n", err);
			return -1;
		}
		pr_debug("ps set ODR, fifo latency done\n");
/* start polling, if needed */
/*		if (cxt->ps_ctl.is_report_input_direct == false) {
 *			int mdelay = cxt->ps_delay_ns;
 *
 *			do_div(mdelay, 1000000);
 *			atomic_set(&cxt->delay_ps, mdelay);
 *			if (cxt->is_ps_polling_run == false) {
 *				mod_timer(&cxt->timer_ps, jiffies +
 *					atomic_read(&cxt->delay_ps)/(1000/HZ));
 *				cxt->is_ps_polling_run = true;
 *				cxt->is_ps_first_data_after_enable = true;
 *			}
 *		pr_debug("ps delay %d ms\n", atomic_read(&cxt->delay_ps));
 *		} else {
 *			ps_data_report(1, 3);
 *		}
 */
		pr_debug("PS batch done\n");
	}
	return 0;
}
#endif
static ssize_t psactive_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct alsps_context *cxt = alsps_context_obj;
	int err = 0;

	pr_debug("%s buf=%s\n", __func__, buf);
	mutex_lock(&alsps_context_obj->alsps_op_mutex);

	if (!strncmp(buf, "1", 1))
		cxt->ps_enable = 1;
	else if (!strncmp(buf, "0", 1))
		cxt->ps_enable = 0;
	else {
		pr_err("%s error !!\n", __func__);
		err = -1;
		goto err_out;
	}
#if defined(CONFIG_NANOHUB) && defined(CONFIG_MTK_ALSPSHUB)
	err = cxt->ps_ctl.enable_nodata(cxt->ps_enable);
#else
	err = ps_enable_and_batch();
#endif
err_out:
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	pr_debug("%s done\n", __func__);
	if (err)
		return err;
	else
		return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t psactive_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct alsps_context *cxt = NULL;
	int div = 0;

	cxt = alsps_context_obj;
	div = cxt->ps_data.vender_div;
	pr_debug("ps vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t psbatch_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct alsps_context *cxt = alsps_context_obj;
	int handle = 0, flag = 0, err = 0;

	pr_debug("%s %s\n", __func__, buf);
	err = sscanf(buf, "%d,%d,%lld,%lld", &handle, &flag, &cxt->ps_delay_ns,
		     &cxt->ps_latency_ns);
	if (err != 4) {
		pr_err("%s param error: err = %d\n", __func__, err);
		return -1;
	}

	mutex_lock(&alsps_context_obj->alsps_op_mutex);
#if defined(CONFIG_NANOHUB) && defined(CONFIG_MTK_ALSPSHUB)
	if (cxt->ps_ctl.is_support_batch)
		err = cxt->ps_ctl.batch(0, cxt->ps_delay_ns,
					cxt->ps_latency_ns);
	else
		err = cxt->ps_ctl.batch(0, cxt->ps_delay_ns, 0);
#else
	err = ps_enable_and_batch();
#endif
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	pr_debug("%s done: %d\n", __func__, cxt->is_ps_batch_enable);
	if (err)
		return err;
	else
		return count;
}

static ssize_t psbatch_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t psflush_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct alsps_context *cxt = NULL;
	int handle = 0, err = 0;

	err = kstrtoint(buf, 10, &handle);
	if (err != 0)
		pr_err("%s param error: err = %d\n", __func__, err);

	pr_debug("%s param: handle %d\n", __func__, handle);

	mutex_lock(&alsps_context_obj->alsps_op_mutex);
	cxt = alsps_context_obj;
	if (cxt->ps_ctl.flush != NULL)
		err = cxt->ps_ctl.flush();
	if (err < 0)
		pr_err("ps enable flush err %d\n", err);
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	if (err)
		return err;
	else
		return count;
}

static ssize_t psflush_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
/* need work around again */
static ssize_t psdevnum_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t pscali_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct alsps_context *cxt = NULL;
	int err = 0;
	uint8_t *cali_buf = NULL;

	cali_buf = vzalloc(count);
	if (!cali_buf)
		return -ENOMEM;
	memcpy(cali_buf, buf, count);

	mutex_lock(&alsps_context_obj->alsps_op_mutex);
	cxt = alsps_context_obj;
	if (cxt->ps_ctl.set_cali != NULL)
		err = cxt->ps_ctl.set_cali(cali_buf, count);
	if (err < 0)
		pr_err("ps set cali err %d\n", err);
	mutex_unlock(&alsps_context_obj->alsps_op_mutex);
	vfree(cali_buf);
	return count;
}

static int als_ps_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int als_ps_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	pltfm_dev = pdev;
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id als_ps_of_match[] = {
	{
		.compatible = "mediatek,als_ps",
	},
	{},
};
#endif

static struct platform_driver als_ps_driver = {
	.probe = als_ps_probe,
	.remove = als_ps_remove,
	.driver = {

		.name = "als_ps",
#ifdef CONFIG_OF
		.of_match_table = als_ps_of_match,
#endif
	}
};

static int alsps_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	pr_debug("%s start\n", __func__);
	for (i = 0; i < MAX_CHOOSE_ALSPS_NUM; i++) {
		pr_debug("%s i=%d\n", __func__, i);
		if (alsps_init_list[i] != 0) {
			pr_debug(" alsps try to init driver %s\n",
				  alsps_init_list[i]->name);
			err = alsps_init_list[i]->init();
			if (err == 0) {
				pr_debug(" alsps real driver %s probe ok\n",
					  alsps_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_ALSPS_NUM) {
		pr_debug("%s fail\n", __func__);
		err = -1;
	}

	return err;
}

int alsps_driver_add(struct alsps_init_info *obj)
{
	int err = 0;
	int i = 0;

	pr_debug("%s\n", __func__);

	if (!obj) {
		pr_err(
			"ALSPS driver add fail, alsps_init_info is NULL\n");
		return -1;
	}

	for (i = 0; i < MAX_CHOOSE_ALSPS_NUM; i++) {
		if ((i == 0) && (alsps_init_list[0] == NULL)) {
			pr_debug("register alsps driver for the first time\n");
			if (platform_driver_register(&als_ps_driver))
				pr_err(
					"failed to register alsps driver already exist\n");
		}

		if (alsps_init_list[i] == NULL) {
			obj->platform_diver_addr = &als_ps_driver;
			alsps_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_ALSPS_NUM) {
		pr_err("ALSPS driver add err\n");
		err = -1;
	}

	return err;
}
EXPORT_SYMBOL_GPL(alsps_driver_add);
struct platform_device *get_alsps_platformdev(void)
{
	return pltfm_dev;
}

int ps_report_interrupt_data(int value)
{
	struct alsps_context *cxt = NULL;
	/* int err =0; */
	cxt = alsps_context_obj;
	pr_notice("[ALS/PS] [%s]:value=%d\n", __func__, value);
	if (cxt->is_get_valid_ps_data_after_enable == false) {
		if (value != ALSPS_INVALID_VALUE) {
			cxt->is_get_valid_ps_data_after_enable = true;
			smp_mb(); /*for memory barriier*/
			del_timer_sync(&cxt->timer_ps);
			smp_mb(); /*for memory barriier*/
			cancel_work_sync(&cxt->report_ps);
		}
	}

	if (cxt->is_ps_batch_enable == false)
		ps_data_report(value, 3);

	return 0;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(ps_report_interrupt_data);
DEVICE_ATTR_RW(alsactive);
DEVICE_ATTR_RW(alsbatch);
DEVICE_ATTR_RW(alsflush);
DEVICE_ATTR_RO(alsdevnum);
DEVICE_ATTR_WO(alscali);
DEVICE_ATTR_RW(psactive);
DEVICE_ATTR_RW(psbatch);
DEVICE_ATTR_RW(psflush);
DEVICE_ATTR_RO(psdevnum);
DEVICE_ATTR_WO(pscali);

static struct attribute *als_attributes[] = {
	&dev_attr_alsactive.attr,
	&dev_attr_alsbatch.attr,
	&dev_attr_alsflush.attr,
	&dev_attr_alsdevnum.attr,
	&dev_attr_alscali.attr,
	NULL
};

static struct attribute *ps_attributes[] = {
	&dev_attr_psactive.attr,
	&dev_attr_psbatch.attr,
	&dev_attr_psflush.attr,
	&dev_attr_psdevnum.attr,
	&dev_attr_pscali.attr,
	NULL
};

static struct attribute_group als_attribute_group = {
	.attrs = als_attributes
};

static struct attribute_group ps_attribute_group = {
	.attrs = ps_attributes
};
static int light_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t light_read(struct file *file, char __user *buffer, size_t count,
			  loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(alsps_context_obj->als_mdev.minor, file,
				     buffer, count, ppos);

	return read_cnt;
}

static unsigned int light_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(alsps_context_obj->als_mdev.minor, file, wait);
}

static const struct file_operations light_fops = {
	.owner = THIS_MODULE,
	.open = light_open,
	.read = light_read,
	.poll = light_poll,
};

static int als_misc_init(struct alsps_context *cxt)
{
	int err = 0;

	cxt->als_mdev.minor = ID_LIGHT;
	cxt->als_mdev.name = ALS_MISC_DEV_NAME;
	cxt->als_mdev.fops = &light_fops;
	err = sensor_attr_register(&cxt->als_mdev);
	if (err)
		pr_err("unable to register alsps misc device!!\n");

	return err;
}
static int proximity_open(struct inode *inode, struct file *file)
{
	nonseekable_open(inode, file);
	return 0;
}

static ssize_t proximity_read(struct file *file, char __user *buffer,
			      size_t count, loff_t *ppos)
{
	ssize_t read_cnt = 0;

	read_cnt = sensor_event_read(alsps_context_obj->ps_mdev.minor, file,
				     buffer, count, ppos);

	return read_cnt;
}

static unsigned int proximity_poll(struct file *file, poll_table *wait)
{
	return sensor_event_poll(alsps_context_obj->ps_mdev.minor, file, wait);
}

static const struct file_operations proximity_fops = {
	.owner = THIS_MODULE,
	.open = proximity_open,
	.read = proximity_read,
	.poll = proximity_poll,
};

static int ps_misc_init(struct alsps_context *cxt)
{
	int err = 0;

	cxt->ps_mdev.minor = ID_PROXIMITY;
	cxt->ps_mdev.name = PS_MISC_DEV_NAME;
	cxt->ps_mdev.fops = &proximity_fops;
	err = sensor_attr_register(&cxt->ps_mdev);
	if (err)
		pr_err("unable to register alsps misc device!!\n");

	return err;
}

int als_register_data_path(struct als_data_path *data)
{
	struct alsps_context *cxt = NULL;
	/* int err =0; */
	cxt = alsps_context_obj;
	cxt->als_data.get_data = data->get_data;
	cxt->als_data.vender_div = data->vender_div;
	cxt->als_data.als_get_raw_data = data->als_get_raw_data;
	pr_debug("alsps register data path vender_div: %d\n",
		  cxt->als_data.vender_div);
	if (cxt->als_data.get_data == NULL) {
		pr_debug("als register data path fail\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(als_register_data_path);

int ps_register_data_path(struct ps_data_path *data)
{
	struct alsps_context *cxt = NULL;
	/* int err =0; */
	cxt = alsps_context_obj;
	cxt->ps_data.get_data = data->get_data;
	cxt->ps_data.vender_div = data->vender_div;
	cxt->ps_data.ps_get_raw_data = data->ps_get_raw_data;
	pr_debug("alsps register data path vender_div: %d\n",
		  cxt->ps_data.vender_div);
	if (cxt->ps_data.get_data == NULL) {
		pr_debug("ps register data path fail\n");
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(ps_register_data_path);

int als_register_control_path(struct als_control_path *ctl)
{
	struct alsps_context *cxt = NULL;
	int err = 0;

	cxt = alsps_context_obj;
	cxt->als_ctl.set_delay = ctl->set_delay;
	cxt->als_ctl.open_report_data = ctl->open_report_data;
	cxt->als_ctl.enable_nodata = ctl->enable_nodata;
	cxt->als_ctl.batch = ctl->batch;
	cxt->als_ctl.flush = ctl->flush;
	cxt->als_ctl.set_cali = ctl->set_cali;
	cxt->als_ctl.rgbw_enable = ctl->rgbw_enable;
	cxt->als_ctl.rgbw_batch = ctl->rgbw_batch;
	cxt->als_ctl.rgbw_flush = ctl->rgbw_flush;
	cxt->als_ctl.is_support_batch = ctl->is_support_batch;
	cxt->als_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->als_ctl.is_use_common_factory = ctl->is_use_common_factory;

	if (cxt->als_ctl.enable_nodata == NULL || cxt->als_ctl.batch == NULL ||
	    cxt->als_ctl.flush == NULL) {
		pr_debug("als register control path fail\n");
		return -1;
	}

	/* add misc dev for sensor hal control cmd */
	err = als_misc_init(alsps_context_obj);
	if (err) {
		pr_err("unable to register alsps misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&alsps_context_obj->als_mdev.this_device->kobj,
				 &als_attribute_group);
	if (err < 0) {
		pr_err("unable to create alsps attribute file\n");
		return -3;
	}
	kobject_uevent(&alsps_context_obj->als_mdev.this_device->kobj,
		       KOBJ_ADD);
	return 0;
}
EXPORT_SYMBOL_GPL(als_register_control_path);

int ps_register_control_path(struct ps_control_path *ctl)
{
	struct alsps_context *cxt = NULL;
	int err = 0;

	cxt = alsps_context_obj;
	cxt->ps_ctl.set_delay = ctl->set_delay;
	cxt->ps_ctl.open_report_data = ctl->open_report_data;
	cxt->ps_ctl.enable_nodata = ctl->enable_nodata;
	cxt->ps_ctl.batch = ctl->batch;
	cxt->ps_ctl.flush = ctl->flush;
	cxt->ps_ctl.is_support_batch = ctl->is_support_batch;
	cxt->ps_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->ps_ctl.ps_calibration = ctl->ps_calibration;
	cxt->ps_ctl.set_cali = ctl->set_cali;
	cxt->ps_ctl.is_use_common_factory = ctl->is_use_common_factory;
	cxt->ps_ctl.is_polling_mode = ctl->is_polling_mode;

	if (cxt->ps_ctl.enable_nodata == NULL || cxt->ps_ctl.batch == NULL ||
	    cxt->ps_ctl.flush == NULL) {
		pr_debug("ps register control path fail\n");
		return -1;
	}

	err = ps_misc_init(alsps_context_obj);
	if (err) {
		pr_err("unable to register alsps misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&alsps_context_obj->ps_mdev.this_device->kobj,
				 &ps_attribute_group);
	if (err < 0) {
		pr_err("unable to create alsps attribute file\n");
		return -3;
	}
	kobject_uevent(&alsps_context_obj->ps_mdev.this_device->kobj, KOBJ_ADD);
	return 0;
}
EXPORT_SYMBOL_GPL(ps_register_control_path);

/* AAL functions**************************************** */
int alsps_aal_enable(int enable)
{
	return 0;
}

int alsps_aal_get_status(void)
{
	return 0;
}

int alsps_aal_get_data(void)
{
	return 0;
}
/* *************************************************** */

int alsps_probe(void)
{
	int err;

	pr_debug("%s start!!\n", __func__);
	alsps_context_obj = alsps_context_alloc_object();
	if (!alsps_context_obj) {
		err = -ENOMEM;
		pr_err("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	/* init real alspseleration driver */
	err = alsps_real_driver_init();
	if (err) {
		pr_err("alsps real driver init fail\n");
		goto real_driver_init_fail;
	}
	pr_debug("%s OK !!\n", __func__);
	return 0;

real_driver_init_fail:
	kfree(alsps_context_obj);
	alsps_context_obj = NULL;
exit_alloc_data_failed:
	pr_err("%s fail !!!\n", __func__);
	return err;
}
EXPORT_SYMBOL_GPL(alsps_probe);

int alsps_remove(void)
{
	int err = 0;

	pr_debug("%s\n", __func__);
	sysfs_remove_group(&alsps_context_obj->als_mdev.this_device->kobj,
			   &als_attribute_group);
	sysfs_remove_group(&alsps_context_obj->ps_mdev.this_device->kobj,
			   &ps_attribute_group);

	err = sensor_attr_deregister(&alsps_context_obj->als_mdev);
	err = sensor_attr_deregister(&alsps_context_obj->ps_mdev);
	if (err)
		pr_err("misc_deregister fail: %d\n", err);
	kfree(alsps_context_obj);

	platform_driver_unregister(&als_ps_driver);

	return 0;
}
EXPORT_SYMBOL_GPL(alsps_remove);

static int __init alsps_init(void)
{
	pr_debug("%s\n", __func__);
	AAL_init();
	return 0;
}

static void __exit alsps_exit(void)
{
	pr_debug("%s\n", __func__);
	AAL_exit();
}

module_init(alsps_init);
module_exit(alsps_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ALSPS device driver");
MODULE_AUTHOR("Mediatek");

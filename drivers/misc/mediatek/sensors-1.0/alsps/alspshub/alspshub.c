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


#define pr_fmt(fmt) "[ALS/PS] " fmt

#include "alspshub.h"
#include <alsps.h>
#include <hwmsensor.h>
#include <SCP_sensorHub.h>
#include "SCP_power_monitor.h"
#include <linux/pm_wakeup.h>


#define ALSPSHUB_DEV_NAME     "alsps_hub_pl"

struct alspshub_ipi_data {
	struct work_struct init_done_work;
	atomic_t first_ready_after_boot;
	/*misc */
	atomic_t	als_suspend;
	atomic_t	ps_suspend;
	atomic_t	trace;
	atomic_t	scp_init_done;

	/*data */
	u16		als;
	u8		ps;
	int		ps_cali;
	atomic_t	als_cali;
	atomic_t	ps_thd_val_high;
	atomic_t	ps_thd_val_low;
	ulong		enable;
	ulong		pending_intr;
	bool als_factory_enable;
	bool ps_factory_enable;
	bool als_android_enable;
	bool ps_android_enable;
	struct wakeup_source ps_wake_lock;
};

static struct alspshub_ipi_data *obj_ipi_data;
static int ps_get_data(int *value, int *status);

static int alspshub_local_init(void);
static int alspshub_local_remove(void);
static int alspshub_init_flag = -1;
static struct alsps_init_info alspshub_init_info = {
	.name = "alsps_hub",
	.init = alspshub_local_init,
	.uninit = alspshub_local_remove,

};

static DEFINE_MUTEX(alspshub_mutex);
static DEFINE_SPINLOCK(calibration_lock);

enum {
	CMC_BIT_ALS = 1,
	CMC_BIT_PS = 2,
} CMC_BIT;
enum {
	CMC_TRC_ALS_DATA = 0x0001,
	CMC_TRC_PS_DATA = 0x0002,
	CMC_TRC_EINT = 0x0004,
	CMC_TRC_IOCTL = 0x0008,
	CMC_TRC_I2C = 0x0010,
	CMC_TRC_CVT_ALS = 0x0020,
	CMC_TRC_CVT_PS = 0x0040,
	CMC_TRC_DEBUG = 0x8000,
} CMC_TRC;

long alspshub_read_ps(u8 *ps)
{
	long res;
	struct alspshub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data_t;

	res = sensor_get_data_from_hub(ID_PROXIMITY, &data_t);
	if (res < 0) {
		*ps = -1;
		pr_err("sensor_get_data_from_hub fail, (ID: %d)\n",
			ID_PROXIMITY);
		return -1;
	}
	if (data_t.proximity_t.steps < obj->ps_cali)
		*ps = 0;
	else
		*ps = data_t.proximity_t.steps - obj->ps_cali;
	return 0;
}

long alspshub_read_als(u16 *als)
{
	long res = 0;
	struct data_unit_t data_t;

	res = sensor_get_data_from_hub(ID_LIGHT, &data_t);
	if (res < 0) {
		*als = -1;
		pr_err_ratelimited("sensor_get_data_from_hub fail, (ID: %d)\n",
			ID_LIGHT);
		return -1;
	}
	*als = data_t.light;

	return 0;
}

static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj_ipi_data) {
		pr_err("obj_ipi_data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t trace_store(struct device_driver *ddri,
				const char *buf, size_t count)
{
	int trace = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;
	int res = 0;
	int ret = 0;

	if (!obj) {
		pr_err("obj_ipi_data is null!!\n");
		return 0;
	}
	ret = sscanf(buf, "0x%x", &trace);
	if (ret != 1) {
		pr_err("invalid content: '%s', length = %zu\n", buf, count);
		return count;
	}
	atomic_set(&obj->trace, trace);
	res = sensor_set_cmd_to_hub(ID_PROXIMITY,
		CUST_ACTION_SET_TRACE, &trace);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_SET_TRACE);
		return 0;
	}
	return count;
}

static ssize_t als_show(struct device_driver *ddri, char *buf)
{
	int res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj) {
		pr_err("obj_ipi_data is null!!\n");
		return 0;
	}
	res = alspshub_read_als(&obj->als);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", obj->als);
}

static ssize_t ps_show(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj) {
		pr_err("cm3623_obj is null!!\n");
		return 0;
	}
	res = alspshub_read_ps(&obj->ps);
	if (res)
		return snprintf(buf, PAGE_SIZE, "ERROR: %d\n", (int)res);
	else
		return snprintf(buf, PAGE_SIZE, "0x%04X\n", obj->ps);
}

static ssize_t reg_show(struct device_driver *ddri, char *buf)
{
	int res = 0;

	res = sensor_set_cmd_to_hub(ID_PROXIMITY, CUST_ACTION_SHOW_REG, buf);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_SHOW_REG);
		return 0;
	}

	return res;
}

static ssize_t alslv_show(struct device_driver *ddri, char *buf)
{
	int res = 0;

	res = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_SHOW_ALSLV, buf);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_LIGHT, CUST_ACTION_SHOW_ALSLV);
		return 0;
	}

	return res;
}

static ssize_t alsval_show(struct device_driver *ddri, char *buf)
{
	int res = 0;

	res = sensor_set_cmd_to_hub(ID_LIGHT, CUST_ACTION_SHOW_ALSVAL, buf);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_LIGHT, CUST_ACTION_SHOW_ALSVAL);
		return 0;
	}

	return res;
}

static DRIVER_ATTR_RO(als);
static DRIVER_ATTR_RO(ps);
static DRIVER_ATTR_RO(alslv);
static DRIVER_ATTR_RO(alsval);
static DRIVER_ATTR_RW(trace);
static DRIVER_ATTR_RO(reg);
static struct driver_attribute *alspshub_attr_list[] = {
	&driver_attr_als,
	&driver_attr_ps,
	&driver_attr_trace,	/*trace log */
	&driver_attr_alslv,
	&driver_attr_alsval,
	&driver_attr_reg,
};

static int alspshub_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(alspshub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, alspshub_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				alspshub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int alspshub_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(ARRAY_SIZE(alspshub_attr_list));

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, alspshub_attr_list[idx]);

	return err;
}

static void alspshub_init_done_work(struct work_struct *work)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;
	int err = 0;
#ifndef MTK_OLD_FACTORY_CALIBRATION
	int32_t cfg_data[2] = {0};
#endif

	if (atomic_read(&obj->scp_init_done) == 0) {
		pr_err("wait for nvram to set calibration\n");
		return;
	}
	if (atomic_xchg(&obj->first_ready_after_boot, 1) == 0)
		return;
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_PROXIMITY,
		CUST_ACTION_SET_CALI, &obj->ps_cali);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail,(ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_SET_CALI);
#else
	spin_lock(&calibration_lock);
	cfg_data[0] = atomic_read(&obj->ps_thd_val_high);
	cfg_data[1] = atomic_read(&obj->ps_thd_val_low);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_PROXIMITY,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub ps fail\n");

	spin_lock(&calibration_lock);
	cfg_data[0] = atomic_read(&obj->als_cali);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_LIGHT,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub als fail\n");
#endif
}
static int ps_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj)
		return 0;

	if (event->flush_action == FLUSH_ACTION)
		err = ps_flush_report();
	else if (event->flush_action == DATA_ACTION &&
			READ_ONCE(obj->ps_android_enable) == true) {
		__pm_wakeup_event(&obj->ps_wake_lock, msecs_to_jiffies(100));
		err = ps_data_report_t(event->proximity_t.oneshot,
			SENSOR_STATUS_ACCURACY_HIGH,
			(int64_t)event->time_stamp);
	} else if (event->flush_action == CALI_ACTION) {
		spin_lock(&calibration_lock);
		atomic_set(&obj->ps_thd_val_high, event->data[0]);
		atomic_set(&obj->ps_thd_val_low, event->data[1]);
		spin_unlock(&calibration_lock);
		err = ps_cali_report(event->data);
	}
	return err;
}
static int als_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (!obj)
		return 0;

	if (event->flush_action == FLUSH_ACTION)
		err = als_flush_report();
	else if ((event->flush_action == DATA_ACTION) &&
			READ_ONCE(obj->als_android_enable) == true)
		err = als_data_report_t(event->light,
				SENSOR_STATUS_ACCURACY_MEDIUM,
				(int64_t)event->time_stamp);
	else if (event->flush_action == CALI_ACTION) {
		spin_lock(&calibration_lock);
		atomic_set(&obj->als_cali, event->data[0]);
		spin_unlock(&calibration_lock);
		err = als_cali_report(event->data);
	}
	return err;
}

static int rgbw_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;

	if (event->flush_action == FLUSH_ACTION)
		err = rgbw_flush_report();
	else if (event->flush_action == DATA_ACTION)
		err = rgbw_data_report_t(event->data,
			(int64_t)event->time_stamp);
	return err;
}

static int alshub_factory_enable_sensor(bool enable_disable,
				int64_t sample_periods_ms)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (enable_disable == true)
		WRITE_ONCE(obj->als_factory_enable, true);
	else
		WRITE_ONCE(obj->als_factory_enable, false);

	if (enable_disable == true) {
		err = sensor_set_delay_to_hub(ID_LIGHT, sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_LIGHT, enable_disable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	mutex_lock(&alspshub_mutex);
	if (enable_disable)
		set_bit(CMC_BIT_ALS, &obj->enable);
	else
		clear_bit(CMC_BIT_ALS, &obj->enable);
	mutex_unlock(&alspshub_mutex);
	return 0;
}
static int alshub_factory_get_data(int32_t *data)
{
	int err = 0;
	struct data_unit_t data_t;

	err = sensor_get_data_from_hub(ID_LIGHT, &data_t);
	if (err < 0)
		return -1;
	*data = data_t.light;
	return 0;
}
static int alshub_factory_get_raw_data(int32_t *data)
{
	return alshub_factory_get_data(data);
}
static int alshub_factory_enable_calibration(void)
{
	return sensor_calibration_to_hub(ID_LIGHT);
}
static int alshub_factory_clear_cali(void)
{
	return 0;
}
static int alshub_factory_set_cali(int32_t offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;
	int err = 0;
	int32_t cfg_data;

	cfg_data = offset;
	err = sensor_cfg_to_hub(ID_LIGHT,
		(uint8_t *)&cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub fail\n");
	atomic_set(&obj->als_cali, offset);
	als_cali_report(&cfg_data);

	return err;

}
static int alshub_factory_get_cali(int32_t *offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	*offset = atomic_read(&obj->als_cali);
	return 0;
}
static int pshub_factory_enable_sensor(bool enable_disable,
			int64_t sample_periods_ms)
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	if (enable_disable == true) {
		err = sensor_set_delay_to_hub(ID_PROXIMITY, sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_PROXIMITY, enable_disable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	mutex_lock(&alspshub_mutex);
	if (enable_disable)
		set_bit(CMC_BIT_PS, &obj->enable);
	else
		clear_bit(CMC_BIT_PS, &obj->enable);
	mutex_unlock(&alspshub_mutex);
	return 0;
}
static int pshub_factory_get_data(int32_t *data)
{
	int err = 0, status = 0;

	err = ps_get_data(data, &status);
	if (err < 0)
		return -1;
	return 0;
}
static int pshub_factory_get_raw_data(int32_t *data)
{
	int err = 0;
	struct data_unit_t data_t;

	err = sensor_get_data_from_hub(ID_PROXIMITY, &data_t);
	if (err < 0)
		return -1;
	*data = data_t.proximity_t.steps;
	return 0;
}
static int pshub_factory_enable_calibration(void)
{
	return sensor_calibration_to_hub(ID_PROXIMITY);
}
static int pshub_factory_clear_cali(void)
{
#ifdef MTK_OLD_FACTORY_CALIBRATION
	int err = 0;
#endif
	struct alspshub_ipi_data *obj = obj_ipi_data;

	obj->ps_cali = 0;
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_PROXIMITY,
			CUST_ACTION_RESET_CALI, &obj->ps_cali);
	if (err < 0) {
		pr_err("sensor_set_cmd_to_hub fail, (ID: %d),(action: %d)\n",
			ID_PROXIMITY, CUST_ACTION_RESET_CALI);
		return -1;
	}
#endif
	return 0;
}
static int pshub_factory_set_cali(int32_t offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	obj->ps_cali = offset;
	return 0;
}
static int pshub_factory_get_cali(int32_t *offset)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	*offset = obj->ps_cali;
	return 0;
}
static int pshub_factory_set_threshold(int32_t threshold[2])
{
	int err = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;
#ifndef MTK_OLD_FACTORY_CALIBRATION
	int32_t cfg_data[2] = {0};
#endif
	if (threshold[0] < threshold[1] || threshold[0] <= 0 ||
		threshold[1] <= 0) {
		pr_err("PS set threshold fail! invalid value:[%d, %d]\n",
			threshold[0], threshold[1]);
		return -1;
	}

	spin_lock(&calibration_lock);
	atomic_set(&obj->ps_thd_val_high, (threshold[0] + obj->ps_cali));
	atomic_set(&obj->ps_thd_val_low, (threshold[1] + obj->ps_cali));
	spin_unlock(&calibration_lock);
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = sensor_set_cmd_to_hub(ID_PROXIMITY,
		CUST_ACTION_SET_PS_THRESHOLD, threshold);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail, (ID:%d),(action:%d)\n",
			ID_PROXIMITY, CUST_ACTION_SET_PS_THRESHOLD);
#else
	spin_lock(&calibration_lock);
	cfg_data[0] = atomic_read(&obj->ps_thd_val_high);
	cfg_data[1] = atomic_read(&obj->ps_thd_val_low);
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_PROXIMITY,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub fail\n");

	ps_cali_report(cfg_data);
#endif
	return err;
}

static int pshub_factory_get_threshold(int32_t threshold[2])
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	threshold[0] = atomic_read(&obj->ps_thd_val_high) - obj->ps_cali;
	threshold[1] = atomic_read(&obj->ps_thd_val_low) - obj->ps_cali;
	spin_unlock(&calibration_lock);
	return 0;
}

static struct alsps_factory_fops alspshub_factory_fops = {
	.als_enable_sensor = alshub_factory_enable_sensor,
	.als_get_data = alshub_factory_get_data,
	.als_get_raw_data = alshub_factory_get_raw_data,
	.als_enable_calibration = alshub_factory_enable_calibration,
	.als_clear_cali = alshub_factory_clear_cali,
	.als_set_cali = alshub_factory_set_cali,
	.als_get_cali = alshub_factory_get_cali,

	.ps_enable_sensor = pshub_factory_enable_sensor,
	.ps_get_data = pshub_factory_get_data,
	.ps_get_raw_data = pshub_factory_get_raw_data,
	.ps_enable_calibration = pshub_factory_enable_calibration,
	.ps_clear_cali = pshub_factory_clear_cali,
	.ps_set_cali = pshub_factory_set_cali,
	.ps_get_cali = pshub_factory_get_cali,
	.ps_set_threshold = pshub_factory_set_threshold,
	.ps_get_threshold = pshub_factory_get_threshold,
};

static struct alsps_factory_public alspshub_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &alspshub_factory_fops,
};
static int als_open_report_data(int open)
{
	return 0;
}


static int als_enable_nodata(int en)
{
	int res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	pr_debug("obj_ipi_data als enable value = %d\n", en);

	if (en == true)
		WRITE_ONCE(obj->als_android_enable, true);
	else
		WRITE_ONCE(obj->als_android_enable, false);

	res = sensor_enable_to_hub(ID_LIGHT, en);
	if (res < 0) {
		pr_err("%s is failed!!\n", __func__);
		return -1;
	}

	mutex_lock(&alspshub_mutex);
	if (en)
		set_bit(CMC_BIT_ALS, &obj_ipi_data->enable);
	else
		clear_bit(CMC_BIT_ALS, &obj_ipi_data->enable);
	mutex_unlock(&alspshub_mutex);
	return 0;
}

static int als_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	unsigned int delayms = 0;

	delayms = (unsigned int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_LIGHT, delayms);
	if (err) {
		pr_err("%s fail!\n", __func__);
		return err;
	}
	pr_debug("%s (%d)\n", __func__, delayms);
	return 0;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int als_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	als_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_LIGHT, flag,
		samplingPeriodNs, maxBatchReportLatencyNs);
}

static int als_flush(void)
{
	return sensor_flush_to_hub(ID_LIGHT);
}

static int als_set_cali(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	atomic_set(&obj->als_cali, buf[0]);
	spin_unlock(&calibration_lock);
	return sensor_cfg_to_hub(ID_LIGHT, data, count);
}

static int rgbw_enable(int en)
{
	int res = 0;

	res = sensor_enable_to_hub(ID_RGBW, en);
	if (res < 0) {
		pr_err("%s is failed!!\n", __func__);
		return -1;
	}
	return 0;
}

static int rgbw_batch(int flag, int64_t samplingPeriodNs,
		int64_t maxBatchReportLatencyNs)
{
	return sensor_batch_to_hub(ID_RGBW,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int rgbw_flush(void)
{
	return sensor_flush_to_hub(ID_RGBW);
}

static int als_get_data(int *value, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_LIGHT, &data);
	if (err) {
		pr_err("sensor_get_data_from_hub fail!\n");
	} else {
		time_stamp = data.time_stamp;
		*value = data.light;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&obj_ipi_data->trace) & CMC_TRC_PS_DATA)
		pr_debug("value = %d\n", *value);
	return 0;
}

static int ps_open_report_data(int open)
{
	return 0;
}

static int ps_enable_nodata(int en)
{
	int res = 0;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	pr_debug("obj_ipi_data als enable value = %d\n", en);
	if (en == true)
		WRITE_ONCE(obj->ps_android_enable, true);
	else
		WRITE_ONCE(obj->ps_android_enable, false);

	res = sensor_enable_to_hub(ID_PROXIMITY, en);
	if (res < 0) {
		pr_err("als_enable_nodata is failed!!\n");
		return -1;
	}

	mutex_lock(&alspshub_mutex);
	if (en)
		set_bit(CMC_BIT_PS, &obj_ipi_data->enable);
	else
		clear_bit(CMC_BIT_PS, &obj_ipi_data->enable);
	mutex_unlock(&alspshub_mutex);


	return 0;

}

static int ps_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	unsigned int delayms = 0;

	delayms = (unsigned int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_PROXIMITY, delayms);
	if (err < 0) {
		pr_err("%s fail!\n", __func__);
		return err;
	}

	pr_debug("%s (%d)\n", __func__, delayms);
	return 0;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int ps_batch(int flag,
	int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ps_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_PROXIMITY, flag,
		samplingPeriodNs, maxBatchReportLatencyNs);
}

static int ps_flush(void)
{
	return sensor_flush_to_hub(ID_PROXIMITY);
}

static int ps_get_data(int *value, int *status)
{
	int err = 0;
	struct data_unit_t data;
	uint64_t time_stamp = 0;

	err = sensor_get_data_from_hub(ID_PROXIMITY, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!\n");
		*value = -1;
		err = -1;
	} else {
		time_stamp = data.time_stamp;
		*value = data.proximity_t.oneshot;
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	}

	if (atomic_read(&obj_ipi_data->trace) & CMC_TRC_PS_DATA)
		pr_debug("value = %d\n", *value);

	return err;
}

static int ps_set_cali(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;
	struct alspshub_ipi_data *obj = obj_ipi_data;

	spin_lock(&calibration_lock);
	atomic_set(&obj->ps_thd_val_high, buf[0]);
	atomic_set(&obj->ps_thd_val_low, buf[1]);
	spin_unlock(&calibration_lock);
	return sensor_cfg_to_hub(ID_PROXIMITY, data, count);
}

static int scp_ready_event(uint8_t event, void *ptr)
{
	struct alspshub_ipi_data *obj = obj_ipi_data;

	switch (event) {
	case SENSOR_POWER_UP:
	    atomic_set(&obj->scp_init_done, 1);
		schedule_work(&obj->init_done_work);
		break;
	case SENSOR_POWER_DOWN:
	    atomic_set(&obj->scp_init_done, 0);
		break;
	}
	return 0;
}

static struct scp_power_monitor scp_ready_notifier = {
	.name = "alsps",
	.notifier_call = scp_ready_event,
};

static int alspshub_probe(struct platform_device *pdev)
{
	struct alspshub_ipi_data *obj;
	struct platform_driver *paddr =
			alspshub_init_info.platform_diver_addr;

	int err = 0;
	struct als_control_path als_ctl = { 0 };
	struct als_data_path als_data = { 0 };
	struct ps_control_path ps_ctl = { 0 };
	struct ps_data_path ps_data = { 0 };

	pr_debug("%s\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(*obj));
	obj_ipi_data = obj;

	INIT_WORK(&obj->init_done_work, alspshub_init_done_work);

	platform_set_drvdata(pdev, obj);


	atomic_set(&obj->als_suspend, 0);
	atomic_set(&obj->scp_init_done, 0);
	atomic_set(&obj->first_ready_after_boot, 0);

	obj->enable = 0;
	obj->pending_intr = 0;
	obj->ps_cali = 0;
	atomic_set(&obj->ps_thd_val_low, 21);
	atomic_set(&obj->ps_thd_val_high, 28);
	WRITE_ONCE(obj->als_factory_enable, false);
	WRITE_ONCE(obj->als_android_enable, false);
	WRITE_ONCE(obj->ps_factory_enable, false);
	WRITE_ONCE(obj->ps_android_enable, false);

	clear_bit(CMC_BIT_ALS, &obj->enable);
	clear_bit(CMC_BIT_PS, &obj->enable);
	scp_power_monitor_register(&scp_ready_notifier);
	err = scp_sensorHub_data_registration(ID_PROXIMITY, ps_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = scp_sensorHub_data_registration(ID_LIGHT, als_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = scp_sensorHub_data_registration(ID_RGBW, rgbw_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = alsps_factory_device_register(&alspshub_factory_device);
	if (err) {
		pr_err("alsps_factory_device_register register failed\n");
		goto exit_kfree;
	}
	pr_debug("alspshub_misc_device misc_register OK!\n");
	als_ctl.is_use_common_factory = false;
	ps_ctl.is_use_common_factory = false;
	err = alspshub_create_attr(&paddr->driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	als_ctl.open_report_data = als_open_report_data;
	als_ctl.enable_nodata = als_enable_nodata;
	als_ctl.set_delay = als_set_delay;
	als_ctl.batch = als_batch;
	als_ctl.flush = als_flush;
	als_ctl.set_cali = als_set_cali;
	als_ctl.rgbw_enable = rgbw_enable;
	als_ctl.rgbw_batch = rgbw_batch;
	als_ctl.rgbw_flush = rgbw_flush;
	als_ctl.is_report_input_direct = false;

	als_ctl.is_support_batch = false;

	err = als_register_control_path(&als_ctl);
	if (err) {
		pr_err("register fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	als_data.get_data = als_get_data;
	als_data.vender_div = 100;
	err = als_register_data_path(&als_data);
	if (err) {
		pr_err("tregister fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	ps_ctl.open_report_data = ps_open_report_data;
	ps_ctl.enable_nodata = ps_enable_nodata;
	ps_ctl.set_delay = ps_set_delay;
	ps_ctl.batch = ps_batch;
	ps_ctl.flush = ps_flush;
	ps_ctl.set_cali = ps_set_cali;
	ps_ctl.is_report_input_direct = false;

	ps_ctl.is_support_batch = false;

	err = ps_register_control_path(&ps_ctl);
	if (err) {
		pr_err("register fail = %d\n", err);
		goto exit_create_attr_failed;
	}

	ps_data.get_data = ps_get_data;
	ps_data.vender_div = 100;
	err = ps_register_data_path(&ps_data);
	if (err) {
		pr_err("tregister fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	wakeup_source_init(&obj->ps_wake_lock, "ps_wake_lock");

	alspshub_init_flag = 0;
	pr_debug("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	alspshub_delete_attr(&(alspshub_init_info.platform_diver_addr->driver));
exit_kfree:
	kfree(obj);
	obj_ipi_data = NULL;
exit:
	pr_err("%s: err = %d\n", __func__, err);
	alspshub_init_flag = -1;
	return err;
}

static int alspshub_remove(struct platform_device *pdev)
{
	int err = 0;
	struct platform_driver *paddr =
			alspshub_init_info.platform_diver_addr;

	err = alspshub_delete_attr(&paddr->driver);
	if (err)
		pr_err("alspshub_delete_attr fail: %d\n", err);
	alsps_factory_device_deregister(&alspshub_factory_device);
	kfree(platform_get_drvdata(pdev));
	return 0;

}

static int alspshub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int alspshub_resume(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}
static struct platform_device alspshub_device = {
	.name = ALSPSHUB_DEV_NAME,
	.id = -1,
};

static struct platform_driver alspshub_driver = {
	.probe = alspshub_probe,
	.remove = alspshub_remove,
	.suspend = alspshub_suspend,
	.resume = alspshub_resume,
	.driver = {
		.name = ALSPSHUB_DEV_NAME,
	},
};

static int alspshub_local_init(void)
{

	if (platform_driver_register(&alspshub_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == alspshub_init_flag)
		return -1;
	return 0;
}
static int alspshub_local_remove(void)
{

	platform_driver_unregister(&alspshub_driver);
	return 0;
}

static int __init alspshub_init(void)
{
	if (platform_device_register(&alspshub_device)) {
		pr_err("alsps platform device error\n");
		return -1;
	}
	alsps_driver_add(&alspshub_init_info);
	return 0;
}

static void __exit alspshub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(alspshub_init);
module_exit(alspshub_exit);
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_DESCRIPTION("alspshub driver");
MODULE_LICENSE("GPL");

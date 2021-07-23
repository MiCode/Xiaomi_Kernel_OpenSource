/* GYRO_HUB motion sensor driver
 *
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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


#define pr_fmt(fmt) "[GYRO] " fmt

#include <hwmsensor.h>
#include "gyrohub.h"
#include <gyroscope.h>
#include <SCP_sensorHub.h>
#include "SCP_power_monitor.h"

#define XIAOMI_FACTORY_CALIBRATION 1

/* name must different with gsensor gyrohub */
#define GYROHUB_DEV_NAME    "gyro_hub"

static struct gyro_init_info gyrohub_init_info;
struct platform_device *gyroPltFmDev;
static int gyrohub_init_flag = -1;
static DEFINE_SPINLOCK(calibration_lock);

enum GYRO_TRC {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0X08,
	GYRO_TRC_INFO = 0X10,
	GYRO_TRC_DATA = 0X20,
};
struct gyrohub_ipi_data {
	int direction;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest_status;
	int32_t static_cali[GYROHUB_AXES_NUM];
	uint8_t static_cali_status;
	int32_t dynamic_cali[GYROHUB_AXES_NUM];
	int32_t temperature_cali[6];
	struct work_struct init_done_work;
	/*data */
	atomic_t scp_init_done;
	atomic_t first_ready_after_boot;
	bool factory_enable;
	bool android_enable;
	struct completion calibration_done;
	struct completion selftest_done;
};
static struct gyrohub_ipi_data *obj_ipi_data;

static int gyrohub_get_data(int *x, int *y, int *z, int *status);

#ifdef MTK_OLD_FACTORY_CALIBRATION
static int gyrohub_write_rel_calibration(struct gyrohub_ipi_data *obj,
	int dat[GYROHUB_AXES_NUM])
{
	obj->static_cali[GYROHUB_AXIS_X] = dat[GYROHUB_AXIS_X];
	obj->static_cali[GYROHUB_AXIS_Y] = dat[GYROHUB_AXIS_Y];
	obj->static_cali[GYROHUB_AXIS_Z] = dat[GYROHUB_AXIS_Z];


	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("write gyro calibration data  (%5d, %5d, %5d)\n",
			 obj->static_cali[GYROHUB_AXIS_X],
			 obj->static_cali[GYROHUB_AXIS_Y],
			 obj->static_cali[GYROHUB_AXIS_Z]);
	}

	return 0;
}

static int gyrohub_ResetCalibration(void)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	unsigned char buf[2] = {0};
	int err = 0;

	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_RESET_CALI, buf);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail,(ID:%d),(action:%d)\n",
			ID_GYROSCOPE, CUST_ACTION_RESET_CALI);

	memset(obj->static_cali, 0x00, sizeof(obj->static_cali));
	pr_debug("gyro clear cali\n");
	return err;
}

static int gyrohub_ReadCalibration(int dat[GYROHUB_AXES_NUM])
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	dat[GYROHUB_AXIS_X] = obj->static_cali[GYROHUB_AXIS_X];
	dat[GYROHUB_AXIS_Y] = obj->static_cali[GYROHUB_AXIS_Y];
	dat[GYROHUB_AXIS_Z] = obj->static_cali[GYROHUB_AXIS_Z];

	pr_debug("Read gyro calibration data  (%5d, %5d, %5d)\n",
		 dat[GYROHUB_AXIS_X], dat[GYROHUB_AXIS_Y], dat[GYROHUB_AXIS_Z]);
	return 0;
}

static int gyrohub_WriteCalibration_scp(int dat[GYROHUB_AXES_NUM])
{
	int err = 0;

	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SET_CALI, dat);
	if (err < 0)
		pr_err("sensor_set_cmd_to_hub fail,(ID:%d),(action:%d)\n",
			ID_GYROSCOPE, CUST_ACTION_SET_CALI);
	return err;
}

static int gyrohub_WriteCalibration(int dat[GYROHUB_AXES_NUM])
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int err = 0;
	int cali[GYROHUB_AXES_NUM];

	pr_debug("%s\n", __func__);

	if (!obj || !dat) {
		pr_err("null ptr!!\n");
		return -EINVAL;
	}

	err = gyrohub_WriteCalibration_scp(dat);
	if (err < 0) {
		pr_err("gyrohub_WriteCalibration_scp fail\n");
		return -1;
	}

	cali[GYROHUB_AXIS_X] = obj->static_cali[GYROHUB_AXIS_X];
	cali[GYROHUB_AXIS_Y] = obj->static_cali[GYROHUB_AXIS_Y];
	cali[GYROHUB_AXIS_Z] = obj->static_cali[GYROHUB_AXIS_Z];

	cali[GYROHUB_AXIS_X] += dat[GYROHUB_AXIS_X];
	cali[GYROHUB_AXIS_Y] += dat[GYROHUB_AXIS_Y];
	cali[GYROHUB_AXIS_Z] += dat[GYROHUB_AXIS_Z];

	pr_debug("write gyro calibration data (%5d,%5d,%5d)-->(%5d,%5d,%5d)\n",
		 dat[GYROHUB_AXIS_X], dat[GYROHUB_AXIS_Y], dat[GYROHUB_AXIS_Z],
		 cali[GYROHUB_AXIS_X], cali[GYROHUB_AXIS_Y],
		 cali[GYROHUB_AXIS_Z]);

	return gyrohub_write_rel_calibration(obj, cali);
}
#endif

static int gyrohub_SetPowerMode(bool enable)
{
	int err = 0;

	err = sensor_enable_to_hub(ID_GYROSCOPE, enable);
	if (err < 0)
		pr_err("sensor_enable_to_hub fail!\n");

	return err;
}

static int gyrohub_ReadGyroData(char *buf, int bufsize)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	struct data_unit_t data;
	uint64_t time_stamp = 0;
	int gyro[GYROHUB_AXES_NUM];
	int err = 0;
	int status = 0;

	if (atomic_read(&obj->suspend))
		return -3;

	if (buf == NULL)
		return -1;
	err = sensor_get_data_from_hub(ID_GYROSCOPE, &data);
	if (err < 0) {
		pr_err("sensor_get_data_from_hub fail!\n");
		return err;
	}

	time_stamp				= data.time_stamp;
	gyro[GYROHUB_AXIS_X]	= data.gyroscope_t.x;
	gyro[GYROHUB_AXIS_Y]	= data.gyroscope_t.y;
	gyro[GYROHUB_AXIS_Z]	= data.gyroscope_t.z;
	status					= data.gyroscope_t.status;
	sprintf(buf, "%04x %04x %04x %04x",
		gyro[GYROHUB_AXIS_X],
		gyro[GYROHUB_AXIS_Y],
		gyro[GYROHUB_AXIS_Z],
		status);

	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		pr_debug("gsensor data: %s!\n", buf);

	return 0;

}

static int gyrohub_ReadChipInfo(char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30))
		return -1;

	sprintf(buf, "GYROHUB Chip");
	return 0;
}

static int gyrohub_ReadAllReg(char *buf, int bufsize)
{
	int err = 0;

	err = gyrohub_SetPowerMode(true);
	if (err)
		pr_err("Power on mpu6050 error %d!\n", err);
	msleep(50);
	err = sensor_set_cmd_to_hub(ID_GYROSCOPE, CUST_ACTION_SHOW_REG, buf);
	if (err < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID:%d),(action:%d)\n",
			ID_GYROSCOPE, CUST_ACTION_SHOW_REG);
		return 0;
	}
	return 0;
}

static ssize_t chipinfo_show(struct device_driver *ddri, char *buf)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	char strbuf[GYROHUB_BUFSIZE];
	int err = 0;

	if (obj == NULL) {
		pr_err("obj is null!!\n");
		return 0;
	}
	err = gyrohub_ReadAllReg(strbuf, GYROHUB_BUFSIZE);
	if (err < 0) {
		pr_debug("gyrohub_ReadAllReg fail!!\n");
		return 0;
	}
	err = gyrohub_ReadChipInfo(strbuf, GYROHUB_BUFSIZE);
	if (err < 0) {
		pr_debug("gyrohub_ReadChipInfo fail!!\n");
		return 0;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t sensordata_show(struct device_driver *ddri,
	char *buf)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	char strbuf[GYROHUB_BUFSIZE];
	int err = 0;

	if (obj == NULL) {
		pr_err("obj is null!!\n");
		return 0;
	}

	err = gyrohub_ReadGyroData(strbuf, GYROHUB_BUFSIZE);
	if (err < 0) {
		pr_debug("gyrohub_ReadGyroData fail!!\n");
		return 0;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		pr_err(" obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t trace_store(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int trace = 0;
	int res = 0;

	if (obj == NULL) {
		pr_err("obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) != 1) {
		pr_err("invalid content:'%s', length =%zu\n", buf, count);
		return count;
	}

	atomic_set(&obj->trace, trace);
	res = sensor_set_cmd_to_hub(ID_GYROSCOPE,
		CUST_ACTION_SET_TRACE, &trace);
	if (res < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID:%d),(action:%d)\n",
			ID_GYROSCOPE, CUST_ACTION_SET_TRACE);
		return 0;
	}

	return count;
}

static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL) {
		pr_err(" obj is null!!\n");
		return 0;
	}

	return len;
}

static ssize_t orientation_show(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n",
								obj->direction);

	return _tLength;
}

static ssize_t orientation_store(struct device_driver *ddri,
	const char *buf, size_t tCount)
{
	int _nDirection = 0, ret = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (obj == NULL)
		return 0;
	ret = kstrtoint(buf, 10, &_nDirection);

	if (ret != 0) {
		pr_debug("[%s] set direction: %d\n", __func__, _nDirection);
		return tCount;
	}

	obj->direction = _nDirection;
	ret = sensor_set_cmd_to_hub(ID_GYROSCOPE,
		CUST_ACTION_SET_DIRECTION, &_nDirection);
	if (ret < 0) {
		pr_err("sensor_set_cmd_to_hub fail,(ID:%d),(action:%d)\n",
			ID_GYROSCOPE, CUST_ACTION_SET_DIRECTION);
		return 0;
	}

	pr_debug("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static int gyrohub_factory_enable_calibration(void);
static ssize_t test_cali_store(struct device_driver *ddri,
	const char *buf, size_t tCount)
{
	int enable = 0, ret = 0;

	ret = kstrtoint(buf, 10, &enable);
	if (ret != 0) {
		pr_debug("kstrtoint fail\n");
		return 0;
	}
	if (enable == 1)
		gyrohub_factory_enable_calibration();
	return tCount;
}

static DRIVER_ATTR_RO(chipinfo);
static DRIVER_ATTR_RO(sensordata);
static DRIVER_ATTR_RW(trace);
static DRIVER_ATTR_RO(status);
static DRIVER_ATTR_RW(orientation);
static DRIVER_ATTR_WO(test_cali);

static struct driver_attribute *gyrohub_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
	&driver_attr_orientation,
	&driver_attr_test_cali,
};

static int gyrohub_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(gyrohub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, gyrohub_attr_list[idx]);
		if (err != 0) {
			pr_err("driver_create_file (%s) = %d\n",
				gyrohub_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int gyrohub_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(gyrohub_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, gyrohub_attr_list[idx]);

	return err;
}

static void scp_init_work_done(struct work_struct *work)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int err = 0;
#ifndef MTK_OLD_FACTORY_CALIBRATION
	int32_t cfg_data[12] = {0};
#endif

	if (atomic_read(&obj->scp_init_done) == 0) {
		pr_err("scp is not ready to send cmd\n");
		return;
	}
	if (atomic_xchg(&obj->first_ready_after_boot, 1) == 0)
		return;
#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = gyrohub_WriteCalibration_scp(obj->static_cali);
	if (err < 0)
		pr_err("gyrohub_WriteCalibration_scp fail\n");
#else
	spin_lock(&calibration_lock);
	cfg_data[0] = obj->dynamic_cali[0];
	cfg_data[1] = obj->dynamic_cali[1];
	cfg_data[2] = obj->dynamic_cali[2];

	cfg_data[3] = obj->static_cali[0];
	cfg_data[4] = obj->static_cali[1];
	cfg_data[5] = obj->static_cali[2];

	cfg_data[6] = obj->temperature_cali[0];
	cfg_data[7] = obj->temperature_cali[1];
	cfg_data[8] = obj->temperature_cali[2];
	cfg_data[9] = obj->temperature_cali[3];
	cfg_data[10] = obj->temperature_cali[4];
	cfg_data[11] = obj->temperature_cali[5];
	spin_unlock(&calibration_lock);
	err = sensor_cfg_to_hub(ID_GYROSCOPE,
		(uint8_t *)cfg_data, sizeof(cfg_data));
	if (err < 0)
		pr_err("sensor_cfg_to_hub fail\n");
#endif
}

static int gyro_recv_data(struct data_unit_t *event, void *reserved)
{
	int err = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	struct gyro_data data;

	memset(&data, 0, sizeof(struct gyro_data));
	if (event->flush_action == DATA_ACTION &&
		READ_ONCE(obj->android_enable) == true) {
		if (READ_ONCE(obj->android_enable) == false)
			return 0;
		data.x = event->gyroscope_t.x;
		data.y = event->gyroscope_t.y;
		data.z = event->gyroscope_t.z;
		data.status = event->gyroscope_t.status;
		data.timestamp = (int64_t)event->time_stamp;
		data.reserved[0] = event->reserve[0];
		err = gyro_data_report(&data);
	} else if (event->flush_action == FLUSH_ACTION) {
		err = gyro_flush_report();
	} else if (event->flush_action == BIAS_ACTION) {
		data.x = event->gyroscope_t.x_bias;
		data.y = event->gyroscope_t.y_bias;
		data.z = event->gyroscope_t.z_bias;
		err = gyro_bias_report(&data);
		spin_lock(&calibration_lock);
		obj->dynamic_cali[GYROHUB_AXIS_X] = event->gyroscope_t.x_bias;
		obj->dynamic_cali[GYROHUB_AXIS_Y] = event->gyroscope_t.y_bias;
		obj->dynamic_cali[GYROHUB_AXIS_Z] = event->gyroscope_t.z_bias;
		spin_unlock(&calibration_lock);
	} else if (event->flush_action == CALI_ACTION) {
		data.x = event->gyroscope_t.x_bias;
		data.y = event->gyroscope_t.y_bias;
		data.z = event->gyroscope_t.z_bias;
		if (event->gyroscope_t.status == 0)
			err = gyro_cali_report(&data);
		spin_lock(&calibration_lock);
		obj->static_cali[GYROHUB_AXIS_X] = event->gyroscope_t.x_bias;
		obj->static_cali[GYROHUB_AXIS_Y] = event->gyroscope_t.y_bias;
		obj->static_cali[GYROHUB_AXIS_Z] = event->gyroscope_t.z_bias;
		obj->static_cali_status = (uint8_t)event->gyroscope_t.status;
		spin_unlock(&calibration_lock);
		complete(&obj->calibration_done);
	} else if (event->flush_action == TEMP_ACTION) {
		/* temp action occur when gyro disable,
		 *so we always should send data to userspace
		 */
		err = gyro_temp_report(event->data);
		spin_lock(&calibration_lock);
		obj->temperature_cali[0] = event->data[0];
		obj->temperature_cali[1] = event->data[1];
		obj->temperature_cali[2] = event->data[2];
		obj->temperature_cali[3] = event->data[3];
		obj->temperature_cali[4] = event->data[4];
		obj->temperature_cali[5] = event->data[5];
		spin_unlock(&calibration_lock);
	} else if (event->flush_action == TEST_ACTION) {
		atomic_set(&obj->selftest_status, event->gyroscope_t.status);
		complete(&obj->selftest_done);
	}
	return err;
}
static int gyrohub_factory_enable_sensor(bool enabledisable,
	int64_t sample_periods_ms)
{
	int err = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (enabledisable == true)
		WRITE_ONCE(obj->factory_enable, true);
	else
		WRITE_ONCE(obj->factory_enable, false);

	if (enabledisable == true) {
		err = sensor_set_delay_to_hub(ID_GYROSCOPE, sample_periods_ms);
		if (err) {
			pr_err("sensor_set_delay_to_hub failed!\n");
			return -1;
		}
	}
	err = sensor_enable_to_hub(ID_GYROSCOPE, enabledisable);
	if (err) {
		pr_err("sensor_enable_to_hub failed!\n");
		return -1;
	}
	return 0;
}
static int gyrohub_factory_get_data(int32_t data[3], int *status)
{
	int ret = 0;

	ret = gyrohub_get_data(&data[0], &data[1], &data[2], status);
	data[0] = data[0] / 1000;
	data[1] = data[1] / 1000;
	data[2] = data[2] / 1000;

	return ret;
}
static int gyrohub_factory_get_raw_data(int32_t data[3])
{
	pr_debug("%s don't support!\n", __func__);
	return 0;
}
static int gyrohub_factory_enable_calibration(void)
{
#if XIAOMI_FACTORY_CALIBRATION
	int err = 0;
	struct gyro_data data;
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	spin_lock(&calibration_lock);
	data.x = obj->static_cali[0];
	data.y = obj->static_cali[1];
	data.z = obj->static_cali[2];
	spin_unlock(&calibration_lock);
	err = gyro_cali_report(&data);
	return err;
#else
	return sensor_calibration_to_hub(ID_GYROSCOPE);
#endif
}
static int gyrohub_factory_clear_cali(void)
{
#ifdef MTK_OLD_FACTORY_CALIBRATION
	int err = 0;

	err = gyrohub_ResetCalibration();
	if (err) {
		pr_err("gyrohub_ResetCalibration failed!\n");
		return -1;
	}
#endif
	return 0;
}
static int gyrohub_factory_set_cali(int32_t data[3])
{
#if XIAOMI_FACTORY_CALIBRATION
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	int32_t cali_data[6] = {0};

	pr_err("gyrohub_factory_set_cali data: (%d, %d, %d)!\n", data[0], data[1], data[2]);

	spin_lock(&calibration_lock);
	obj->static_cali[0] = data[0];
	obj->static_cali[1] = data[1];
	obj->static_cali[2] = data[2];

	cali_data[3] = data[0];
	cali_data[4] = data[1];
	cali_data[5] = data[2];
	spin_unlock(&calibration_lock);
	return sensor_cfg_to_hub(ID_GYROSCOPE, (uint8_t *)cali_data, sizeof(int32_t) * 6);
#else
#ifdef MTK_OLD_FACTORY_CALIBRATION
	int err = 0;

	err = gyrohub_WriteCalibration(data);
	if (err) {
		pr_err("gyrohub_WriteCalibration failed!\n");
		return -1;
	}
#endif
	return 0;
#endif
}
static int gyrohub_factory_get_cali(int32_t data[3])
{
	int err = 0;
#if XIAOMI_FACTORY_CALIBRATION
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	spin_lock(&calibration_lock);
	data[GYROHUB_AXIS_X] = obj->static_cali[GYROHUB_AXIS_X];
	data[GYROHUB_AXIS_Y] = obj->static_cali[GYROHUB_AXIS_Y];
	data[GYROHUB_AXIS_Z] = obj->static_cali[GYROHUB_AXIS_Z];
	spin_unlock(&calibration_lock);
#else
#ifndef MTK_OLD_FACTORY_CALIBRATION
	struct gyrohub_ipi_data *obj = obj_ipi_data;
	uint8_t status = 0;
#endif

#ifdef MTK_OLD_FACTORY_CALIBRATION
	err = gyrohub_ReadCalibration(data);
	if (err) {
		pr_err("gyrohub_ReadCalibration failed!\n");
		return -1;
	}
#else
	err = wait_for_completion_timeout(&obj->calibration_done,
		msecs_to_jiffies(3000));
	if (!err) {
		pr_err("%s fail!\n", __func__);
		return -1;
	}
	spin_lock(&calibration_lock);
	data[GYROHUB_AXIS_X] = obj->static_cali[GYROHUB_AXIS_X];
	data[GYROHUB_AXIS_Y] = obj->static_cali[GYROHUB_AXIS_Y];
	data[GYROHUB_AXIS_Z] = obj->static_cali[GYROHUB_AXIS_Z];
	status = obj->static_cali_status;
	spin_unlock(&calibration_lock);
	if (status != 0) {
		pr_debug("gyrohub static cali detect shake!\n");
		return -2;
	}
#endif
#endif
	return err;
}
static int gyrohub_factory_do_self_test(void)
{
	int ret = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	ret = sensor_selftest_to_hub(ID_GYROSCOPE);
	if (ret < 0)
		return -1;

	ret = wait_for_completion_timeout(&obj->selftest_done,
					  msecs_to_jiffies(3000));
	if (!ret)
		return -1;
	return atomic_read(&obj->selftest_status);
}

static struct gyro_factory_fops gyrohub_factory_fops = {
	.enable_sensor = gyrohub_factory_enable_sensor,
	.get_data = gyrohub_factory_get_data,
	.get_raw_data = gyrohub_factory_get_raw_data,
	.enable_calibration = gyrohub_factory_enable_calibration,
	.clear_cali = gyrohub_factory_clear_cali,
	.set_cali = gyrohub_factory_set_cali,
	.get_cali = gyrohub_factory_get_cali,
	.do_self_test = gyrohub_factory_do_self_test,
};

static struct gyro_factory_public gyrohub_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &gyrohub_factory_fops,
};

static int gyrohub_open_report_data(int open)
{
	return 0;
}

static int gyrohub_enable_nodata(int en)
{
	int res = 0;
	bool power = false;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	if (en == 1) {
		power = true;
		WRITE_ONCE(obj->android_enable, true);
	}
	if (en == 0) {
		power = false;
		WRITE_ONCE(obj->android_enable, false);
	}

	res = gyrohub_SetPowerMode(power);
	if (res < 0) {
		pr_err("GYROHUB_SetPowerMode fail\n");
		return res;
	}
	pr_debug("%s OK!\n", __func__);
	return 0;

}

static int gyrohub_set_delay(u64 ns)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	int err = 0;
	int value = 0;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	value = (int)ns / 1000 / 1000;
	err = sensor_set_delay_to_hub(ID_GYROSCOPE, value);
	if (err < 0) {
		pr_err("sensor_set_delay_to_hub fail!\n");
		return err;
	}

	pr_debug("gyro_set_delay (%d)\n", value);
	return err;
#elif defined CONFIG_NANOHUB
	return 0;
#else
	return 0;
#endif
}
static int gyrohub_batch(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	gyrohub_set_delay(samplingPeriodNs);
#endif
	return sensor_batch_to_hub(ID_GYROSCOPE,
		flag, samplingPeriodNs, maxBatchReportLatencyNs);
}

static int gyrohub_flush(void)
{
	return sensor_flush_to_hub(ID_GYROSCOPE);
}

static int gyrohub_set_cali(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;
	struct gyrohub_ipi_data *obj = obj_ipi_data;

	pr_err("gyrohub_set_cali data: (%d, %d, %d)!\n", buf[0], buf[1], buf[2]);
	pr_err("gyrohub_set_cali data: (%d, %d, %d)!\n", buf[3], buf[4], buf[5]);

	spin_lock(&calibration_lock);
	obj->dynamic_cali[0] = buf[0];
	obj->dynamic_cali[1] = buf[1];
	obj->dynamic_cali[2] = buf[2];

	obj->static_cali[0] = buf[3];
	obj->static_cali[1] = buf[4];
	obj->static_cali[2] = buf[5];

	obj->temperature_cali[0] = buf[6];
	obj->temperature_cali[1] = buf[7];
	obj->temperature_cali[2] = buf[8];
	obj->temperature_cali[3] = buf[9];
	obj->temperature_cali[4] = buf[10];
	obj->temperature_cali[5] = buf[11];
	spin_unlock(&calibration_lock);
	return sensor_cfg_to_hub(ID_GYROSCOPE, data, count);
}

static int gpio_config(void)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;

	if (gyroPltFmDev == NULL) {
		pr_err("Cannot find gyro device!\n");
		return 0;
	}

	pinctrl = devm_pinctrl_get(&gyroPltFmDev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("Cannot find gyro pinctrl!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		pr_err("Cannot find gyro pinctrl default!\n");
	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		pr_err("Cannot find gyro pinctrl pin_cfg!\n");
		return ret;
	}
	pinctrl_select_state(pinctrl, pins_cfg);

	return 0;
}

static int gyrohub_get_data(int *x, int *y, int *z, int *status)
{
	char buff[GYROHUB_BUFSIZE];
	int err = 0;

	err = gyrohub_ReadGyroData(buff, GYROHUB_BUFSIZE);
	if (err < 0) {
		pr_err("gyrohub_ReadGyroData fail!!\n");
		return -1;
	}
	err = sscanf(buff, "%x %x %x %x", x, y, z, status);
	if (err != 4) {
		pr_err("sscanf fail!!\n");
		return -1;
	}
	return 0;
}
static int scp_ready_event(uint8_t event, void *ptr)
{
	struct gyrohub_ipi_data *obj = obj_ipi_data;

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
	.name = "gyro",
	.notifier_call = scp_ready_event,
};
static int gyrohub_probe(struct platform_device *pdev)
{
	struct gyrohub_ipi_data *obj;
	int err = 0;
	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };

	struct platform_driver *paddr =
					gyrohub_init_info.platform_diver_addr;

	pr_debug("%s\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct gyrohub_ipi_data));

	obj_ipi_data = obj;
	platform_set_drvdata(pdev, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	atomic_set(&obj->first_ready_after_boot, 0);
	atomic_set(&obj->scp_init_done, 0);
	atomic_set(&obj->selftest_status, 0);
	WRITE_ONCE(obj->factory_enable, false);
	WRITE_ONCE(obj->android_enable, false);
	INIT_WORK(&obj->init_done_work, scp_init_work_done);
	init_completion(&obj->calibration_done);
	init_completion(&obj->selftest_done);

	err = gpio_config();
	if (err < 0) {
		pr_err("gpio_config failed\n");
		goto exit_kfree;
	}
	scp_power_monitor_register(&scp_ready_notifier);
	err = scp_sensorHub_data_registration(ID_GYROSCOPE, gyro_recv_data);
	if (err < 0) {
		pr_err("scp_sensorHub_data_registration failed\n");
		goto exit_kfree;
	}
	err = gyro_factory_device_register(&gyrohub_factory_device);
	if (err) {
		pr_err("gyro_factory_device_register fail err = %d\n",
			err);
		goto exit_kfree;
	}
	ctl.is_use_common_factory = true;

	err = gyrohub_create_attr(&paddr->driver);
	if (err) {
		pr_err("gyrohub create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gyrohub_open_report_data;
	ctl.enable_nodata = gyrohub_enable_nodata;
	ctl.set_delay = gyrohub_set_delay;
	ctl.batch = gyrohub_batch;
	ctl.flush = gyrohub_flush;
	ctl.set_cali = gyrohub_set_cali;
#if defined CONFIG_MTK_SCP_SENSORHUB_V1
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = false;
#elif defined CONFIG_NANOHUB
	ctl.is_report_input_direct = true;
	ctl.is_support_batch = true;
#else
#endif

	err = gyro_register_control_path(&ctl);
	if (err) {
		pr_err("register gyro control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = gyrohub_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		pr_err("gyro_register_data_path fail = %d\n", err);
		goto exit_create_attr_failed;
	}
	gyrohub_init_flag = 0;

	pr_debug("%s: OK\n", __func__);
	return 0;
exit_create_attr_failed:
	gyrohub_delete_attr(&(gyrohub_init_info.platform_diver_addr->driver));
exit_kfree:
	kfree(obj);
	obj_ipi_data = NULL;
exit:
	gyrohub_init_flag = -1;
	pr_err("%s: err = %d\n", __func__, err);
	return err;
}

static int gyrohub_remove(struct platform_device *pdev)
{
	int err = 0;
	struct platform_driver *paddr =
				gyrohub_init_info.platform_diver_addr;

	err = gyrohub_delete_attr(&paddr->driver);
	if (err)
		pr_err("gyrohub_delete_attr fail: %d\n", err);

	gyro_factory_device_deregister(&gyrohub_factory_device);

	kfree(platform_get_drvdata(pdev));
	return 0;
}

static int gyrohub_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int gyrohub_resume(struct platform_device *pdev)
{
	return 0;
}
static struct platform_device gyrohub_device = {
	.name = GYROHUB_DEV_NAME,
	.id = -1,
};
static struct platform_driver gyrohub_driver = {
	.driver = {
		   .name = GYROHUB_DEV_NAME,
	},
	.probe = gyrohub_probe,
	.remove = gyrohub_remove,
	.suspend = gyrohub_suspend,
	.resume = gyrohub_resume,
};

static int gyrohub_local_remove(void)
{
	platform_driver_unregister(&gyrohub_driver);
	return 0;
}

static int gyrohub_local_init(struct platform_device *pdev)
{
	gyroPltFmDev = pdev;

	if (platform_driver_register(&gyrohub_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == gyrohub_init_flag)
		return -1;
	return 0;
}
static struct gyro_init_info gyrohub_init_info = {
	.name = "gyrohub",
	.init = gyrohub_local_init,
	.uninit = gyrohub_local_remove,
};

static int __init gyrohub_init(void)
{

	if (platform_device_register(&gyrohub_device)) {
		pr_err("platform device error\n");
		return -1;
	}
	gyro_driver_add(&gyrohub_init_info);

	return 0;
}

static void __exit gyrohub_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(gyrohub_init);
module_exit(gyrohub_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GYROHUB gyroscope driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");

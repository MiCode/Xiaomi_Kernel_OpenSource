/*****************************************************************************
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
 *
 *****************************************************************************/

#include <cust_gyro.h>
#include "mpu6050.h"
#include <gyroscope.h>
#include <hwmsensor.h>

#define INV_GYRO_AUTO_CALI  1

/*----------------------------------------------------------------------------*/
#define MPU6050_DEFAULT_FS		MPU6050_FS_1000
#define MPU6050_DEFAULT_LSB		MPU6050_FS_1000_LSB
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU6050_LOWPASS	/*apply low pass filter on output */
/*----------------------------------------------------------------------------*/
#define MPU6050_AXIS_X          0
#define MPU6050_AXIS_Y          1
#define MPU6050_AXIS_Z          2
#define MPU6050_AXES_NUM        3
#define MPU6050_DATA_LEN        6
#define MPU6050_DEV_NAME        "MPU6050GY"	/* name must different with gsensor mpu6050 */
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu6050_i2c_id[] = {{MPU6050_DEV_NAME, 0}, {} };

int packet_thresh = 75;		/* 600 ms / 8ms/sample */

/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mpu6050_i2c_remove(struct i2c_client *client);
static int mpu6050_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#ifdef CONFIG_PM_SLEEP
static int mpu6050_suspend(struct device *dev);
static int mpu6050_resume(struct device *dev);
#endif
static int mpu6050_local_init(struct platform_device *pdev);
static int  mpu6050_remove(void);
static int gyroscope_init_flag = -1;	/* 0<==>OK -1 <==> fail */
static struct gyro_init_info mpu6050_init_info = {
		.name = "mpu6050GY",
		.init = mpu6050_local_init,
		.uninit = mpu6050_remove,
};

/*----------------------------------------------------------------------------*/
enum {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0X08,
	GYRO_TRC_INFO = 0X10,
	GYRO_TRC_DATA = 0X20,
};
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u8 whole;
	u8 fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
	struct scale_factor scalefactor;
	int sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][MPU6050_AXES_NUM];
	int sum[MPU6050_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct mpu6050_i2c_data {
	struct i2c_client *client;
	struct gyro_hw hw;
	struct hwmsen_convert cvt;

	/*misc */
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[MPU6050_AXES_NUM + 1];

	/*data */
	s8 offset[MPU6050_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[MPU6050_AXES_NUM + 1];

#if defined(CONFIG_MPU6050_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
	/*early suspend */
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_drv;
#endif
#if INV_GYRO_AUTO_CALI == 1
	s16 inv_cali_raw[MPU6050_AXES_NUM + 1];
	s16 temperature;
	struct mutex temperature_mutex;	/* for temperature protection */
	struct mutex raw_data_mutex;	/* for inv_cali_raw[] protection */
#endif
};
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,gyro"},
	{},
};
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops mpu6050_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mpu6050_suspend, mpu6050_resume)
};
#endif
static struct i2c_driver mpu6050gy_i2c_driver = {
	.driver = {
		.name = MPU6050_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		.pm = &mpu6050_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = gyro_of_match,
#endif
	},
	.probe = mpu6050_i2c_probe,
	.remove = mpu6050_i2c_remove,
	.detect = mpu6050_i2c_detect,
	.id_table = mpu6050_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mpu6050_i2c_client;
static struct mpu6050_i2c_data *obj_i2c_data;
static bool sensor_power;

/*----------------------------------------------------------------------------*/
#define MPU6050GY_DEBUG 0
#define GYRO_FLAG					"<GYRO> "
#if MPU6050GY_DEBUG
#define GYRO_DBG(fmt, args...)	pr_debug(GYRO_FLAG fmt, ##args)
#define GYRO_ERR(fmt, args...)	pr_debug(GYRO_FLAG fmt, ##args)
#else
#define GYRO_DBG(fmt, args...)
#define GYRO_ERR(fmt, args...)
#endif

/*----------------------------------------------------------------------------*/


static unsigned int power_on;
#if INV_GYRO_AUTO_CALI == 1
/*
* devpath : "/sys/devices/virtual/invensense_daemon_class/invensense_daemon_device
* class : "/sys/class/invensense_daemon_class"
* inv_mpl_motion :
*	"/sys/class/invensense_daemon_class/invensense_daemon_device/inv_mpl_motion", 1:motion 0:no motion
*	"/sys/devices/virtual/invensense_daemon_class/invensense_daemon_device/inv_mpl_motion", 1:motion 0:no motion
* inv_gyro_data_ready :
*	"/sys/class/invensense_daemon_class/invensense_daemon_device/inv_gyro_data_ready"
*	"/sys/devices/virtual/invensense_daemon_class/invensense_daemon_device/inv_gyro_data_ready"
* inv_gyro_power_state :
*	"/sys/class/invensense_daemon_class/invensense_daemon_device/inv_gyro_power_state"
*	"/sys/devices/virtual/invensense_daemon_class/invensense_daemon_device/inv_gyro_power_state"
*/

#define INV_DAEMON_CLASS_NAME  "invensense_daemon_class"
#define INV_DAEMON_DEVICE_NAME  "invensense_daemon_device"

static struct class *inv_daemon_class;
static struct device *inv_daemon_device;
static int inv_mpl_motion_state;	/* default is 0: no motion */
static int inv_gyro_power_state;
static ssize_t inv_mpl_motion_store(struct device *dev,
				    struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int result;
	unsigned long data;

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	/* if (inv_mpl_motion_state != data) */
	{
		char *envp[2];

		if (data)
			envp[0] = "STATUS=MOTION";
		else
			envp[0] = "STATUS=NOMOTION";
		envp[1] = NULL;
		result = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);

		inv_mpl_motion_state = data;
	}

	return count;
}

static ssize_t inv_mpl_motion_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_mpl_motion_state);
}

static ssize_t inv_gyro_data_ready_store(struct device *dev,
					 struct device_attribute *attr, const char *buf,
					 size_t count)
{
	sysfs_notify(&dev->kobj, NULL, "inv_gyro_data_ready");
	return count;
}

static ssize_t inv_gyro_data_ready_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

static ssize_t inv_gyro_power_state_store(struct device *dev,
					  struct device_attribute *attr, const char *buf,
					  size_t count)
{
	unsigned int result;
	unsigned long data;

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	inv_gyro_power_state = data;

	sysfs_notify(&dev->kobj, NULL, "inv_gyro_power_state");
	return count;
}

static ssize_t inv_gyro_power_state_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_gyro_power_state);
}

static DEVICE_ATTR(inv_mpl_motion, 0644, inv_mpl_motion_show, inv_mpl_motion_store);
static DEVICE_ATTR(inv_gyro_data_ready, 0644, inv_gyro_data_ready_show, inv_gyro_data_ready_store);
static DEVICE_ATTR(inv_gyro_power_state, 0644, inv_gyro_power_state_show, inv_gyro_power_state_store);

static struct device_attribute *inv_daemon_dev_attributes[] = {
	&dev_attr_inv_mpl_motion,
	&dev_attr_inv_gyro_data_ready,
	&dev_attr_inv_gyro_power_state,
};
#endif				/* #if INV_GYRO_AUTO_CALI == 1 */


int MPU6050_gyro_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(MPU6050_gyro_power);

int MPU6050_gyro_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(MPU6050_gyro_mode);

/*----------------------------------------------------------------------------*/
static int MPU6050_write_rel_calibration(struct mpu6050_i2c_data *obj, int dat[MPU6050_AXES_NUM])
{
	obj->cali_sw[MPU6050_AXIS_X] = obj->cvt.sign[MPU6050_AXIS_X]*dat[obj->cvt.map[MPU6050_AXIS_X]];
	obj->cali_sw[MPU6050_AXIS_Y] = obj->cvt.sign[MPU6050_AXIS_Y]*dat[obj->cvt.map[MPU6050_AXIS_Y]];
	obj->cali_sw[MPU6050_AXIS_Z] = obj->cvt.sign[MPU6050_AXIS_Z]*dat[obj->cvt.map[MPU6050_AXIS_Z]];
#if MPU6050GY_DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_DBG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
		 obj->cvt.sign[MPU6050_AXIS_X], obj->cvt.sign[MPU6050_AXIS_Y], obj->cvt.sign[MPU6050_AXIS_Z],
		 dat[MPU6050_AXIS_X], dat[MPU6050_AXIS_Y], dat[MPU6050_AXIS_Z],
		 obj->cvt.map[MPU6050_AXIS_X], obj->cvt.map[MPU6050_AXIS_Y], obj->cvt.map[MPU6050_AXIS_Z]);
		GYRO_DBG("write gyro calibration data  (%5d, %5d, %5d)\n",
		 obj->cali_sw[MPU6050_AXIS_X], obj->cali_sw[MPU6050_AXIS_Y], obj->cali_sw[MPU6050_AXIS_Z]);
	}
#endif
	return 0;
}


/*----------------------------------------------------------------------------*/
static int MPU6050_ResetCalibration(struct i2c_client *client)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadCalibration(struct i2c_client *client, int dat[MPU6050_AXES_NUM])
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*obj->cali_sw[MPU6050_AXIS_X];
	dat[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*obj->cali_sw[MPU6050_AXIS_Y];
	dat[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*obj->cali_sw[MPU6050_AXIS_Z];

#if MPU6050GY_DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_DBG("Read gyro calibration data  (%5d, %5d, %5d)\n",
			dat[MPU6050_AXIS_X], dat[MPU6050_AXIS_Y], dat[MPU6050_AXIS_Z]);
	}
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int MPU6050_WriteCalibration(struct i2c_client *client, int dat[MPU6050_AXES_NUM])
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[MPU6050_AXES_NUM];


	GYRO_FUN();
	if (!obj || !dat) {
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	}
	cali[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*obj->cali_sw[MPU6050_AXIS_X];
	cali[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*obj->cali_sw[MPU6050_AXIS_Y];
	cali[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*obj->cali_sw[MPU6050_AXIS_Z];
		cali[MPU6050_AXIS_X] += dat[MPU6050_AXIS_X];
		cali[MPU6050_AXIS_Y] += dat[MPU6050_AXIS_Y];
		cali[MPU6050_AXIS_Z] += dat[MPU6050_AXIS_Z];
#if MPU6050GY_DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
			GYRO_DBG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
				 dat[MPU6050_AXIS_X], dat[MPU6050_AXIS_Y], dat[MPU6050_AXIS_Z],
				 cali[MPU6050_AXIS_X], cali[MPU6050_AXIS_Y], cali[MPU6050_AXIS_Z]);
		}
#endif
		return MPU6050_write_rel_calibration(obj, cali);

	return err;
}

/*----------------------------------------------------------------------------*/

#if 0
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadStart(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GYRO_FUN();
	if (enable) {
		/* enable xyz gyro in FIFO */
	databuf[0] = (MPU6050_FIFO_GYROX_EN|MPU6050_FIFO_GYROY_EN|MPU6050_FIFO_GYROZ_EN);
	} else {
		/* disable xyz gyro in FIFO */
	databuf[0] = 0;
	}

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_write_block(MPU6050_REG_FIFO_EN, databuf, 0x1);
#else

	databuf[1] = databuf[0];
	databuf[0] = MPU6050_REG_FIFO_EN;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR(" enable xyz gyro in FIFO error,enable: 0x%x!\n", databuf[0]);
		return res;
	}
	GYRO_DBG("%s: enable xyz gyro in FIFO: 0x%x\n", __func__, databuf[0]);
	return 0;
}
#endif

/* ----------------------------------------------------------------------------// */
static int MPU6050_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	if (enable == sensor_power) {
		GYRO_DBG("Sensor power status is newest!\n");
		return 0;
	}
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_read_block(MPU6050_REG_PWR_CTL, databuf, 0x01);
#else
	res = hwmsen_read_byte(client, MPU6050_REG_PWR_CTL, databuf);
#endif
	if (res) {
		GYRO_ERR("read power ctl register err!\n");
		return res;
	}


	databuf[0] &= ~MPU6050_SLEEP;
	if (enable == false) {
		if (MPU6050_gse_mode() == false)
			databuf[0] |= MPU6050_SLEEP;
	} else {
		/* do nothing */
	}

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_write_block(MPU6050_REG_PWR_CTL, databuf, 0x1);
#else
	databuf[1] = databuf[0];
	databuf[0] = MPU6050_REG_PWR_CTL;
	res = i2c_master_send(client, databuf, 0x2);
#endif

	if (res <= 0) {
		GYRO_ERR("set power mode failed!\n");
		return res;
	}
	GYRO_DBG("set power mode ok %d!\n", enable);

	sensor_power = enable;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	/*GYRO_FUN();*/

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = dataformat;
	res = MPU6050_hwmsen_write_block(MPU6050_REG_CFG, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_CFG;
	databuf[1] = dataformat;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0)
		return res;

	/* read sample rate after written for test */
	udelay(500);
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_read_block(MPU6050_REG_CFG, databuf, 0x01);
#else
	res = hwmsen_read_byte(client, MPU6050_REG_CFG, databuf);
#endif
	if (res) {
		GYRO_ERR("read data format register err!\n");
		return res;
	}
	GYRO_DBG("read  data format: 0x%x\n", databuf[0]);

	return 0;
}

static int MPU6050_SetFullScale(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	/*GYRO_FUN();*/

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = dataformat;
	res = MPU6050_hwmsen_write_block(MPU6050_REG_GYRO_CFG, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_GYRO_CFG;
	databuf[1] = dataformat;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0)
		return res;

	/* read sample rate after written for test */
	udelay(500);
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_read_block(MPU6050_REG_GYRO_CFG, databuf, 0x01);
#else
	res = hwmsen_read_byte(client, MPU6050_REG_GYRO_CFG, databuf);
#endif
	if (res) {
		GYRO_ERR("read scale register err!\n");
		return res;
	}
	GYRO_DBG("read scale register: 0x%x\n", databuf[0]);

	return 0;
}


/* set the sample rate */
static int MPU6050_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = { 0 };
	int rate_div = 0;
	int res = 0;

	/*GYRO_FUN();*/
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_read_block(MPU6050_REG_CFG, databuf, 0x01);
#else
	res = hwmsen_read_byte(client, MPU6050_REG_CFG, databuf);
#endif
	if (res) {
		GYRO_ERR("read gyro sample rate register err!\n");
		return res;
	}
	GYRO_DBG("read  gyro sample rate register: 0x%x\n", databuf[0]);

	if ((databuf[0] & 0x07) == 0) {	/* Analog sample rate is 8KHz */
		rate_div = 8 * 1024 / sample_rate - 1;
	} else {	/* 1kHz */
		rate_div = 1024 / sample_rate - 1;
	}

	if (rate_div > 255)	{ /* rate_div: 0 to 255; */
		rate_div = 255;
	} else if (rate_div < 0) {
		rate_div = 0;
	}

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = rate_div;
	res = MPU6050_hwmsen_write_block(MPU6050_REG_SAMRT_DIV, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_SAMRT_DIV;
	databuf[1] = rate_div;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write sample rate register err!\n");
		return res;
	}
	/* read sample div after written for test */
	udelay(500);
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	res = MPU6050_hwmsen_read_block(MPU6050_REG_SAMRT_DIV, databuf, 0x01);
#else
	res = hwmsen_read_byte(client, MPU6050_REG_SAMRT_DIV, databuf);
#endif
	if (res) {
		GYRO_ERR("read gyro sample rate register err!\n");
		return res;
	}
	GYRO_DBG("read  gyro sample rate: 0x%x\n", databuf[0]);

	return 0;
}
/*----------------------------------------------------------------------------*/
#if 0
/*----------------------------------------------------------------------------*/
static int MPU6050_FIFOConfig(struct i2c_client *client, u8 clk)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GYRO_FUN();

	/* use gyro X, Y or Z for clocking */
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = clk;
	res = MPU6050_hwmsen_write_block(MPU6050_REG_PWR_CTL, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_PWR_CTL;
	databuf[1] = clk;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write Power CTRL register err!\n");
		return res;
	}
	GYRO_DBG("MPU6050 use gyro X for clocking OK!\n");

	mdelay(50);

	/* enable xyz gyro in FIFO */
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = (MPU6050_FIFO_GYROX_EN|MPU6050_FIFO_GYROY_EN|MPU6050_FIFO_GYROZ_EN);
	res = MPU6050_hwmsen_write_block(MPU6050_REG_FIFO_EN, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_FIFO_EN;
	databuf[1] = (MPU6050_FIFO_GYROX_EN|MPU6050_FIFO_GYROY_EN|MPU6050_FIFO_GYROZ_EN);
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write Power CTRL register err!\n");
		return res;
	}
	GYRO_DBG("MPU6050 enable xyz gyro in FIFO OK!\n");

	/* disable AUX_VDDIO */
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = MPU6050_AUX_VDDIO_DIS;
	res = MPU6050_hwmsen_write_block(MPU6050_REG_AUX_VDD, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_AUX_VDD;
	databuf[1] = MPU6050_AUX_VDDIO_DIS;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write AUX_VDD register err!\n");
		return res;
	}
	GYRO_DBG("MPU6050 disable AUX_VDDIO OK!\n");

	/* enable FIFO and reset FIFO */
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	databuf[0] = (MPU6050_FIFO_EN | MPU6050_FIFO_RST);
	res = MPU6050_hwmsen_write_block(MPU6050_REG_FIFO_CTL, databuf, 0x1);
#else
	databuf[0] = MPU6050_REG_FIFO_CTL;
	databuf[1] = (MPU6050_FIFO_EN | MPU6050_FIFO_RST);
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write FIFO CTRL register err!\n");
		return res;
	}
	GYRO_DBG("%s OK!\n", __func__);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadFifoData(struct i2c_client *client, s16 *data, int *datalen)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	u8 buf[MPU6050_DATA_LEN] = {0};
	s16 tmp1[MPU6050_AXES_NUM] = {0};
	s16 tmp2[MPU6050_AXES_NUM] = {0};
	int err = 0;
	u8 tmp = 0;
	int packet_cnt = 0;
	int i;

	GYRO_FUN();

	if (client == NULL)
		return -EINVAL;

	/* stop putting data in FIFO */
	MPU6050_ReadStart(client, false);

	/* read data number of bytes in FIFO */
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	err = MPU6050_hwmsen_read_block(MPU6050_REG_FIFO_CNTH, &tmp, 0x01);
#else
	err = hwmsen_read_byte(client, MPU6050_REG_FIFO_CNTH, &tmp);
#endif
	if (err) {
		GYRO_ERR("read data high number of bytes error: %d\n", err);
		return -1;
	}
	packet_cnt = tmp << 8;

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	err = MPU6050_hwmsen_read_block(MPU6050_REG_FIFO_CNTL, &tmp, 0x01);
#else
	err = hwmsen_read_byte(client, MPU6050_REG_FIFO_CNTL, &tmp);
#endif
	if (err) {
		GYRO_ERR("read data low number of bytes error: %d\n", err);
		return -1;
	}
	packet_cnt = (packet_cnt + tmp) / MPU6050_DATA_LEN;

	GYRO_DBG("MPU6050 Read Data packet number OK: %d\n", packet_cnt);

	*datalen = packet_cnt;

	/* Within +-5% range: timing_tolerance * packet_thresh=0.05*75 */
	if (packet_cnt && (abs(packet_thresh - packet_cnt) < 4)) {
		/* read data in FIFO */
		for (i = 0; i < packet_cnt; i++) {
#ifdef MPU6050_ACCESS_BY_GSE_I2C
			if (MPU6050_hwmsen_read_block(MPU6050_REG_FIFO_DATA, buf, MPU6050_DATA_LEN)) {
#else
			if (hwmsen_read_block(client, MPU6050_REG_FIFO_DATA, buf, MPU6050_DATA_LEN)) {
#endif
				GYRO_ERR("MPU6050 read data from FIFO error: %d\n", err);
				return -2;
	    } else
				GYRO_DBG("MPU6050 read Data of diff address from FIFO OK !\n");

			tmp1[MPU6050_AXIS_X] = (s16)((buf[MPU6050_AXIS_X*2+1]) | (buf[MPU6050_AXIS_X*2] << 8));
			tmp1[MPU6050_AXIS_Y] = (s16)((buf[MPU6050_AXIS_Y*2+1]) | (buf[MPU6050_AXIS_Y*2] << 8));
			tmp1[MPU6050_AXIS_Z] = (s16)((buf[MPU6050_AXIS_Z*2+1]) | (buf[MPU6050_AXIS_Z*2] << 8));

	    /* remap coordinate// */
			tmp2[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*tmp1[MPU6050_AXIS_X];
			tmp2[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*tmp1[MPU6050_AXIS_Y];
			tmp2[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*tmp1[MPU6050_AXIS_Z];

			data[3 * i + MPU6050_AXIS_X] = tmp2[MPU6050_AXIS_X];
			data[3 * i + MPU6050_AXIS_Y] = tmp2[MPU6050_AXIS_Y];
			data[3 * i + MPU6050_AXIS_Z] = tmp2[MPU6050_AXIS_Z];

			GYRO_DBG("gyro FIFO packet[%d]:[%04X %04X %04X] => [%5d %5d %5d]\n", i,
				data[3*i + MPU6050_AXIS_X], data[3*i + MPU6050_AXIS_Y], data[3*i + MPU6050_AXIS_Z],
				data[3*i + MPU6050_AXIS_X], data[3*i + MPU6050_AXIS_Y], data[3*i + MPU6050_AXIS_Z]);
		}
	} else {
		GYRO_ERR("MPU6050 Incorrect packet count: %d\n", packet_cnt);
		return -3;
	}

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];
	int data[3];
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);

	if (sensor_power == false) {
		MPU6050_SetPowerMode(client, true);
		msleep(50);
	}

#if INV_GYRO_AUTO_CALI == 1
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	if (MPU6050_hwmsen_read_block(MPU6050_REG_TEMPH, databuf, 2)) {
#else
	if (hwmsen_read_block(client, MPU6050_REG_TEMPH, databuf, 2)) {
#endif
		GYRO_ERR("MPU6050 read temperature data  error\n");
		return -2;
	}

	mutex_lock(&obj->temperature_mutex);
	obj->temperature = ((s16)((databuf[1]) | (databuf[0] << 8)));
	mutex_unlock(&obj->temperature_mutex);

#endif

#ifdef MPU6050_ACCESS_BY_GSE_I2C
	if (MPU6050_hwmsen_read_block(MPU6050_REG_GYRO_XH, databuf, 6)) {
#else
	if (hwmsen_read_block(client, MPU6050_REG_GYRO_XH, databuf, 6)) {
#endif
		GYRO_ERR("MPU6050 read gyroscope data  error\n");
		return -2;
	}

	obj->data[MPU6050_AXIS_X] = ((s16)((databuf[MPU6050_AXIS_X*2+1]) | (databuf[MPU6050_AXIS_X*2] << 8)));
	obj->data[MPU6050_AXIS_Y] = ((s16)((databuf[MPU6050_AXIS_Y*2+1]) | (databuf[MPU6050_AXIS_Y*2] << 8)));
	obj->data[MPU6050_AXIS_Z] = ((s16)((databuf[MPU6050_AXIS_Z*2+1]) | (databuf[MPU6050_AXIS_Z*2] << 8)));
#if MPU6050GY_DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {
		GYRO_DBG("read gyro register: %d, %d, %d, %d, %d, %d",
			databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
		GYRO_DBG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
			obj->data[MPU6050_AXIS_X], obj->data[MPU6050_AXIS_Y], obj->data[MPU6050_AXIS_Z],
			obj->data[MPU6050_AXIS_X], obj->data[MPU6050_AXIS_Y], obj->data[MPU6050_AXIS_Z]);
	}
#endif
#if INV_GYRO_AUTO_CALI == 1
	mutex_lock(&obj->raw_data_mutex);
	/*remap coordinate*/
	obj->inv_cali_raw[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*obj->data[MPU6050_AXIS_X];
	obj->inv_cali_raw[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*obj->data[MPU6050_AXIS_Y];
	obj->inv_cali_raw[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*obj->data[MPU6050_AXIS_Z];
	mutex_unlock(&obj->raw_data_mutex);
#endif
	obj->data[MPU6050_AXIS_X] = obj->data[MPU6050_AXIS_X] + obj->cali_sw[MPU6050_AXIS_X];
	obj->data[MPU6050_AXIS_Y] = obj->data[MPU6050_AXIS_Y] + obj->cali_sw[MPU6050_AXIS_Y];
	obj->data[MPU6050_AXIS_Z] = obj->data[MPU6050_AXIS_Z] + obj->cali_sw[MPU6050_AXIS_Z];

	/*remap coordinate*/
	data[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*obj->data[MPU6050_AXIS_X];
	data[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*obj->data[MPU6050_AXIS_Y];
	data[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*obj->data[MPU6050_AXIS_Z];

	/* Out put the degree/second(o/s) */
	data[MPU6050_AXIS_X] = data[MPU6050_AXIS_X] * MPU6050_FS_MAX_LSB / MPU6050_DEFAULT_LSB;
	data[MPU6050_AXIS_Y] = data[MPU6050_AXIS_Y] * MPU6050_FS_MAX_LSB / MPU6050_DEFAULT_LSB;
	data[MPU6050_AXIS_Z] = data[MPU6050_AXIS_Z] * MPU6050_FS_MAX_LSB / MPU6050_DEFAULT_LSB;

	sprintf(buf, "%04x %04x %04x", data[MPU6050_AXIS_X], data[MPU6050_AXIS_Y], data[MPU6050_AXIS_Z]);

#if MPU6050GY_DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_DBG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
#endif

	return 0;
}
#if 0
/* for factory mode */
static int MPU6050_PROCESS_SMT_DATA(struct i2c_client *client, short *data)
{
	int total_num = 0;
	int retval = 0;
	long xSum = 0;
	long ySum = 0;
	long zSum = 0;
	long xAvg, yAvg, zAvg;
	long xRMS, yRMS, zRMS;
	int i = 0;

	int bias_thresh = 5242; /* 40 dps * 131.072 LSB/dps */
	/* float RMS_thresh = 687.19f; // (.2 dps * 131.072) ^ 2 */
	long RMS_thresh = 68719; /* (.2 dps * 131.072) ^ 2 */

	total_num = data[0];
	retval = data[1];
	GYRO_DBG("MPU6050 read gyro data OK, total number: %d\n", total_num);
	for (i = 0; i < total_num; i++) {
		xSum = xSum + data[MPU6050_AXES_NUM*i + MPU6050_AXIS_X + 2];
		ySum = ySum + data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Y + 2];
		zSum = zSum + data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Z + 2];

		/*
		* FLPLOGD("read gyro data OK: packet_num:%d, [X:%5d, Y:%5d, Z:%5d]\n", i,
		* data[MPU6050_AXES_NUM*i + MPU6050_AXIS_X +2], data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Y +2],
		* data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Z +2]);
		* FLPLOGD("MPU6050 xSum: %5d,  ySum: %5d, zSum: %5d\n", xSum, ySum, zSum);
		*/
	}
	GYRO_DBG("MPU6050 xSum: %5ld,  ySum: %5ld, zSum: %5ld\n", xSum, ySum, zSum);

	if (total_num != 0) {
		xAvg = (xSum / total_num);
		yAvg = (ySum / total_num);
		zAvg = (zSum / total_num);
	} else {
		xAvg = xSum;
		yAvg = ySum;
		zAvg = zSum;
	}

	GYRO_DBG("MPU6050 xAvg: %ld,  yAvg: %ld,  zAvg: %ld\n", xAvg, yAvg, zAvg);

	if (abs(xAvg) > bias_thresh) {
		GYRO_ERR("X-Gyro bias exceeded threshold\n");
		retval |= 1 << 3;
	}
	if (abs(yAvg) >  bias_thresh) {
		GYRO_ERR("Y-Gyro bias exceeded threshold\n");
		retval |= 1 << 4;
	}
	if (abs(zAvg) > bias_thresh) {
		GYRO_ERR("Z-Gyro bias exceeded threshold\n");
		retval |= 1 << 5;
	}

	xRMS = 0;
	yRMS = 0;
	zRMS = 0;

	/* Finally, check RMS */
	for (i = 0; i < total_num; i++) {
		xRMS += (data[MPU6050_AXES_NUM*i + MPU6050_AXIS_X+2]-xAvg)*
			(data[MPU6050_AXES_NUM*i + MPU6050_AXIS_X+2]-xAvg);
		yRMS += (data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Y+2]-yAvg)*
			(data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Y+2]-yAvg);
		zRMS += (data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Z+2]-zAvg)*
			(data[MPU6050_AXES_NUM*i + MPU6050_AXIS_Z+2]-zAvg);
	}

	GYRO_DBG("MPU6050 xRMS: %ld,  yRMS: %ld,  zRMS: %ld\n", xRMS, yRMS, zRMS);
	xRMS = 100*xRMS;
	yRMS = 100*yRMS;
	zRMS = 100*zRMS;

	if (get_boot_mode() == FACTORY_BOOT)
		return retval;
	if (xRMS > RMS_thresh * total_num) {
		GYRO_ERR("X-Gyro RMS exceeded threshold, RMS_thresh: %ld\n", RMS_thresh * total_num);
		retval |= 1 << 6;
	}
	if (yRMS > RMS_thresh * total_num) {
		GYRO_ERR("Y-Gyro RMS exceeded threshold, RMS_thresh: %ld\n", RMS_thresh * total_num);
		retval |= 1 << 7;
	}
	if (zRMS > RMS_thresh * total_num) {
		GYRO_ERR("Z-Gyro RMS exceeded threshold, RMS_thresh: %ld\n", RMS_thresh * total_num);
		retval |= 1 << 8;
	}
	if (xRMS == 0 || yRMS == 0 || zRMS == 0)
		/* If any of the RMS noise value returns zero, then we might have dead gyro or FIFO/register failure */
		retval |= 1 << 9;

	GYRO_DBG("retval %d\n", retval);
	return retval;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SMTReadSensorData(struct i2c_client *client, s16 *buf, int bufsize)
{
/* S16 gyro[MPU6050_AXES_NUM*MPU6050_FIFOSIZE]; */
	int res = 0;
	int i;
	int datalen, total_num = 0;

	GYRO_FUN();

	if (sensor_power == false)
		MPU6050_SetPowerMode(client, true);

	if (buf == NULL)
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	for (i = 0; i < MPU6050_AXES_NUM; i++) {
		res = MPU6050_FIFOConfig(client, (i+1));
		if (res) {
			GYRO_ERR("MPU6050_FIFOConfig error:%d!\n", res);
			return -3;
		}

		/* putting data in FIFO during the delayed 600ms */
		mdelay(600);

		res = MPU6050_ReadFifoData(client, &(buf[total_num+2]), &datalen);
		if (res) {
			if (res == (-3))
				buf[1] = (1 << i);
	    else {
				GYRO_ERR("MPU6050_ReadData error:%d!\n", res);
				return -3;
	    }
		} else {
			buf[0] = datalen;
			total_num += datalen*MPU6050_AXES_NUM;
		}
	}

	GYRO_DBG("gyroscope read data OK, total packet: %d", buf[0]);

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8)*10);

	if ((buf == NULL) || (bufsize <= 30))
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "MPU6050 Chip");
	return 0;
}

#if INV_GYRO_AUTO_CALI == 1
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadGyroDataRaw(struct i2c_client *client, char *buf, int bufsize)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->raw_data_mutex);
	/* return gyro raw LSB in device orientation */
	sprintf(buf, "%x %x %x", obj->inv_cali_raw[MPU6050_AXIS_X],
		obj->inv_cali_raw[MPU6050_AXIS_Y], obj->inv_cali_raw[MPU6050_AXIS_Z]);

#if MPU6050GY_DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_DBG("get gyro raw data packet:[%d %d %d]\n", obj->inv_cali_raw[0],
			obj->inv_cali_raw[1], obj->inv_cali_raw[2]);
#endif
	mutex_unlock(&obj->raw_data_mutex);

	return 0;
}
#if 0
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadTemperature(struct i2c_client *client, char *buf, int bufsize)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->temperature_mutex);
	sprintf(buf, "%x", obj->temperature);

#if MPU6050GY_DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_DBG("get gyro temperature:[%d]\n", obj->temperature);
#endif
	mutex_unlock(&obj->temperature_mutex);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadPowerStatus(struct i2c_client *client, char *buf, int bufsize)
{
#if MPU6050GY_DEBUG
	GYRO_DBG("get gyro PowerStatus:[%d]\n", sensor_power);
#endif

	sprintf(buf, "%x", sensor_power);

	return 0;
}
#endif
#endif

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;
	char strbuf[MPU6050_BUFSIZE];

	if (client == NULL) {
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	MPU6050_ReadChipInfo(client, strbuf, MPU6050_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;
	char strbuf[MPU6050_BUFSIZE];

	if (client == NULL) {
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	MPU6050_ReadGyroData(client, strbuf, MPU6050_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mpu6050_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct mpu6050_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct mpu6050_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
		obj->hw.i2c_num, obj->hw.direction, obj->hw.power_id, obj->hw.power_vol);
	return len;
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,             0444, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           0444, show_sensordata_value,    NULL);
static DRIVER_ATTR(trace,      0644, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               0444, show_status_value,        NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *MPU6050_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
};

/*----------------------------------------------------------------------------*/
static int mpu6050_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(MPU6050_attr_list));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, MPU6050_attr_list[idx]);
		if (err != 0) {
			GYRO_ERR("driver_create_file (%s) = %d\n",
				 MPU6050_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(MPU6050_attr_list));

	if (driver == NULL)
		return -EINVAL;



	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, MPU6050_attr_list[idx]);



	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_gpio_config(void)
{
	/* because we donot use EINT ,to support low power */
	/* config to GPIO input mode + PD */
	/* set   GPIO_MSE_EINT_PIN */
	/*
	* mt_set_gpio_mode(GPIO_GYRO_EINT_PIN, GPIO_GYRO_EINT_PIN_M_GPIO);
	* mt_set_gpio_dir(GPIO_GYRO_EINT_PIN, GPIO_DIR_IN);
	* mt_set_gpio_pull_enable(GPIO_GYRO_EINT_PIN, GPIO_PULL_ENABLE);
	* mt_set_gpio_pull_select(GPIO_GYRO_EINT_PIN, GPIO_PULL_DOWN);
	*/
	return 0;
}
static int mpu6050_init_client(struct i2c_client *client, bool enable)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GYRO_FUN();
	mpu6050_gpio_config();

	res = MPU6050_SetPowerMode(client, true);
	if (res)
		return res;



	/* The range should at least be 17.45 rad/s (ie: ~1000 deg/s). */
	res = MPU6050_SetDataFormat(client, (MPU6050_SYNC_GYROX << MPU6050_EXT_SYNC) |
				    MPU6050_RATE_1K_LPFB_188HZ);

	res = MPU6050_SetFullScale(client, (MPU6050_DEFAULT_FS << MPU6050_FS_RANGE));
	if (res)
		return res;

	/* Set 125HZ sample rate */
	res = MPU6050_SetSampleRate(client, 125);
	if (res)
		return res;


	res = MPU6050_SetPowerMode(client, enable);
	if (res)
		return res;


	GYRO_DBG("%s OK!\n", __func__);

#ifdef CONFIG_MPU6050_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_PM_SLEEP
/*----------------------------------------------------------------------------*/
static int mpu6050_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	atomic_set(&obj->suspend, 1);

	err = MPU6050_SetPowerMode(client, false);
	if (err <= 0)
		return err;

#if INV_GYRO_AUTO_CALI == 1
	inv_gyro_power_state = sensor_power;
	/* inv_gyro_power_state = 0; */
	/* put this in where gyro power is changed, waking up mpu daemon */
	sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_power_state");
#endif

	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	err = mpu6050_init_client(client, false);
	if (err) {
		GYRO_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}

/*----------------------------------------------------------------------------*/
#else				/*CONFIG_HAS_EARLY_SUSPEND is defined */
/*----------------------------------------------------------------------------*/
static void mpu6050_early_suspend(struct early_suspend *h)
{
	struct mpu6050_i2c_data *obj = container_of(h, struct mpu6050_i2c_data, early_drv);
	int err;
	/* u8 databuf[2]; */

	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = MPU6050_SetPowerMode(obj->client, false);
	if (err) {
		GYRO_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;

#if INV_GYRO_AUTO_CALI == 1
	inv_gyro_power_state = sensor_power;
	/* inv_gyro_power_state = 0; */
	/* put this in where gyro power is changed, waking up mpu daemon */
	sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_power_state");
#endif

}

/*----------------------------------------------------------------------------*/
static void mpu6050_late_resume(struct early_suspend *h)
{
	struct mpu6050_i2c_data *obj = container_of(h, struct mpu6050_i2c_data, early_drv);
	int err;

	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return;
	}

	err = mpu6050_init_client(obj->client, false);
	if (err) {
		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return;
	}
	atomic_set(&obj->suspend, 0);

}

/*----------------------------------------------------------------------------*/
#endif				/*CONFIG_HAS_EARLYSUSPEND */
/*----------------------------------------------------------------------------*/


/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int gyroscope_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

static int gyroscope_enable_nodata(int en)
{
	int res = 0;
	int retry = 0;
	bool power = false;

	if (en == 1)
		power = true;

	if (en == 0)
		power = false;


	for (retry = 0; retry < 3; retry++) {
		res = MPU6050_SetPowerMode(obj_i2c_data->client, power);
		if (res == 0)
			break;
	}

	if (res) {
		GYRO_ERR("MPU6050_SetPowerMode fail!\n");
		return -1;
	}
	GYRO_INFO("mpu6050_enable_nodata OK!\n");
	return 0;

}

/*----------------------------------------------------------------------------*/
static int gyroscope_set_delay(u64 ns)
{
	return 0;
}
static int gyroscope_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs/1000/1000;

	GYRO_LOG("mpu6050 gyro set delay = (%d) ok.\n", value);
	return gyroscope_set_delay(samplingPeriodNs);
}
static int gyroscope_flush(void)
{
	return gyro_flush_report();
}
/*----------------------------------------------------------------------------*/
static int gyroscope_get_data(int *x, int *y, int *z, int *status)
{
	char buff[MPU6050_BUFSIZE];
	int ret;

	MPU6050_ReadGyroData(obj_i2c_data->client, buff, MPU6050_BUFSIZE);

	ret = sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int mpu6050g_factory_enable_sensor(bool enabledisable, int64_t sample_periods_ms)
{
	int err = 0;

#if 0
	err = bmi160_gyro_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		GYRO_ERR("%s enable failed!\n", __func__);
		return -1;
	}
	err = bmi160_gyro_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		GYRO_ERR("%s set batch failed!\n", __func__);
		return -1;
	}
#endif
	err = mpu6050_init_client(mpu6050_i2c_client, false);
	if (err)
		GYRO_ERR("%s init_client failed!\n", __func__);
	return 0;
}
static int mpu6050g_factory_get_data(int32_t data[3], int *status)
{
#if 0
	return bmi160_gyro_get_data(&data[0], &data[1], &data[2], status);
#endif
	/* prevent meta calibration timeout */
	if (false == sensor_power) {
		MPU6050_SetPowerMode(mpu6050_i2c_client, true);
		msleep(50);
	}
	/* MPU6050_ReadGyroData(client, strbuf, MPU6050_BUFSIZE); */
	return gyroscope_get_data(&data[0], &data[1], &data[2], status);
}
static int mpu6050g_factory_get_raw_data(int32_t data[3])
{
	char *strbuf;
#if 0
	GYRO_INFO("don't support bmg_factory_get_raw_data!\n");
#endif
	strbuf = kmalloc(MPU6050_BUFSIZE, GFP_KERNEL);
	if (!strbuf) {
		GYRO_ERR("strbuf is null!!\n");
		return -EINVAL;
	}

	MPU6050_ReadGyroDataRaw(mpu6050_i2c_client, strbuf, MPU6050_BUFSIZE);
	if (sscanf(strbuf, "%x %x %x", &data[0], &data[1], &data[2]) != 3)
		GYRO_ERR("sscanf parsing fail\n");

	kfree(strbuf);

	return 0;
}
static int mpu6050g_factory_enable_calibration(void)
{
	return 0;
}
static int mpu6050g_factory_clear_cali(void)
{
	int err = 0;

	/* err = bmg_reset_calibration(obj_data); */
	err = MPU6050_ResetCalibration(mpu6050_i2c_client);
	if (err) {
		GYRO_INFO("mpu6050g_ResetCalibration failed!\n");
		return -1;
	}
	return 0;
}
static int mpu6050g_factory_set_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
#if 0
	cali[BMG_AXIS_X] = data[0] * obj_data->sensitivity / BMI160_FS_250_LSB;
	cali[BMG_AXIS_Y] = data[1] * obj_data->sensitivity / BMI160_FS_250_LSB;
	cali[BMG_AXIS_Z] = data[2] * obj_data->sensitivity / BMI160_FS_250_LSB;
	err = bmg_write_calibration(obj_data, cali);
	if (err) {
		GYRO_INFO("bmg_WriteCalibration failed!\n");
		return -1;
	}
#endif
	cali[MPU6050_AXIS_X] = data[0] * MPU6050_DEFAULT_LSB / MPU6050_FS_MAX_LSB;
	cali[MPU6050_AXIS_Y] = data[1] * MPU6050_DEFAULT_LSB / MPU6050_FS_MAX_LSB;
	cali[MPU6050_AXIS_Z] = data[2] * MPU6050_DEFAULT_LSB / MPU6050_FS_MAX_LSB;
	GYRO_LOG("gyro set cali:[%5d %5d %5d]\n", cali[MPU6050_AXIS_X],
		 cali[MPU6050_AXIS_Y], cali[MPU6050_AXIS_Z]);
	err = MPU6050_WriteCalibration(mpu6050_i2c_client, cali);
	if (err) {
		GYRO_INFO("mpu6050g_WriteCalibration failed!\n");
		return -1;
	}

	return 0;
}
static int mpu6050g_factory_get_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
#if 0
	int raw_offset[BMG_BUFSIZE] = { 0 };

	err = bmg_read_calibration(NULL, cali, raw_offset);
	if (err) {
		GYRO_INFO("bmg_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[BMG_AXIS_X] * BMI160_FS_250_LSB / obj_data->sensitivity;
	data[1] = cali[BMG_AXIS_Y] * BMI160_FS_250_LSB / obj_data->sensitivity;
	data[2] = cali[BMG_AXIS_Z] * BMI160_FS_250_LSB / obj_data->sensitivity;
#endif
	err = MPU6050_ReadCalibration(mpu6050_i2c_client, cali);
	if (err) {
		GYRO_INFO("mpu6050g_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[MPU6050_AXIS_X] * MPU6050_FS_MAX_LSB / MPU6050_DEFAULT_LSB;
	data[1] = cali[MPU6050_AXIS_Y] * MPU6050_FS_MAX_LSB / MPU6050_DEFAULT_LSB;
	data[2] = cali[MPU6050_AXIS_Z] * MPU6050_FS_MAX_LSB / MPU6050_DEFAULT_LSB;

	return 0;
}
static int mpu6050g_factory_do_self_test(void)
{
	return 0;
}

static struct gyro_factory_fops mpu6050g_factory_fops = {
	.enable_sensor = mpu6050g_factory_enable_sensor,
	.get_data = mpu6050g_factory_get_data,
	.get_raw_data = mpu6050g_factory_get_raw_data,
	.enable_calibration = mpu6050g_factory_enable_calibration,
	.clear_cali = mpu6050g_factory_clear_cali,
	.set_cali = mpu6050g_factory_set_cali,
	.get_cali = mpu6050g_factory_get_cali,
	.do_self_test = mpu6050g_factory_do_self_test,
};

static struct gyro_factory_public mpu6050g_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &mpu6050g_factory_fops,
};
/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, MPU6050_DEV_NAME);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client = NULL;
	struct mpu6050_i2c_data *obj = NULL;
	int err = 0;
	struct gyro_control_path ctl = {0};
	struct gyro_data_path data = {0};

	GYRO_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!(obj)) {
		err = -ENOMEM;
		goto exit;
	}

	err = get_gyro_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		GYRO_ERR("get dts info fail\n");
		err = -EFAULT;
		goto exit_init_failed;
	}

	err = hwmsen_get_convert(obj->hw.direction, &obj->cvt);
	if (err) {
		GYRO_ERR("invalid direction: %d\n", obj->hw.direction);
		goto exit_init_failed;
	}


	/*GYRO_DBG("gyro_default_i2c_addr: %x\n", client->addr);*/
#ifdef MPU6050_ACCESS_BY_GSE_I2C
	obj->hw.addr = MPU6050_I2C_SLAVE_ADDR;	/* mtk i2c not allow to probe two same address */
#endif

	GYRO_DBG("gyro_custom_i2c_addr: %x\n", obj->hw.addr);
	if (obj->hw.addr != 0) {
		client->addr = obj->hw.addr >> 1;
		GYRO_INFO("gyro_use_i2c_addr: %x\n", client->addr);
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	mpu6050_i2c_client = new_client;
	err = mpu6050_init_client(new_client, false);
	if (err)
		goto exit_init_failed;

	/* err = misc_register(&mpu6050_device); */
	err = gyro_factory_device_register(&mpu6050g_factory_device);
	if (err) {
		GYRO_ERR("mpu6050_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

	err = mpu6050_create_attr(&(mpu6050_init_info.platform_diver_addr->driver));
	if (err) {
		GYRO_ERR("mpu6050 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gyroscope_open_report_data;
	ctl.enable_nodata = gyroscope_enable_nodata;
	/* ctl.set_delay = gyroscope_set_delay; */
	ctl.batch = gyroscope_batch;
	ctl.flush = gyroscope_flush;
	ctl.is_report_input_direct = false;

	err = gyro_register_control_path(&ctl);
	if (err) {
		GYRO_ERR("register gyro control path err\n");
		goto exit_kfree;
	}

	data.get_data = gyroscope_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		GYRO_ERR("gyro_register_data_path fail = %d\n", err);
		goto exit_kfree;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend = mpu6050_early_suspend,
	obj->early_drv.resume = mpu6050_late_resume, register_early_suspend(&obj->early_drv);
#endif

#if INV_GYRO_AUTO_CALI == 1
	mutex_init(&obj->temperature_mutex);
	mutex_init(&obj->raw_data_mutex);
	{
		int i;
		int result;

		/* create a class to avoid event drop by uevent_ops->filter function (dev_uevent_filter()) */
		inv_daemon_class = class_create(THIS_MODULE, INV_DAEMON_CLASS_NAME);
		if (IS_ERR(inv_daemon_class)) {
			GYRO_ERR("cannot create inv daemon class, %s\n", INV_DAEMON_CLASS_NAME);
			goto exit_class_create_failed;
		}
#if 0
		inv_daemon_device = device_create(inv_daemon_class, NULL,
						  MKDEV(MISC_MAJOR, MISC_DYNAMIC_MINOR), NULL,
						  INV_DAEMON_DEVICE_NAME);
		if (IS_ERR(inv_daemon_device)) {
			GYRO_ERR("cannot create inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
			goto exit_inv_device_create_failed;
		}
#endif

		inv_daemon_device = kzalloc(sizeof(struct device), GFP_KERNEL);
		if (!inv_daemon_device) {
			GYRO_ERR("cannot allocate inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
			goto exit_device_register_failed;
		}
		inv_daemon_device->init_name = INV_DAEMON_DEVICE_NAME;
		inv_daemon_device->class = inv_daemon_class;
		inv_daemon_device->release = (void (*)(struct device *))kfree;
		result = device_register(inv_daemon_device);
		if (result) {
			GYRO_ERR("cannot register inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
			goto exit_device_register_failed;
		}

		result = 0;
		for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++) {
			result =
			    device_create_file(inv_daemon_device, inv_daemon_dev_attributes[i]);
			if (result)
				break;
		}
		if (result) {
			while (--i >= 0)
				device_remove_file(inv_daemon_device, inv_daemon_dev_attributes[i]);
			GYRO_ERR("cannot create inv daemon dev attr.\n");
			goto exit_create_file_failed;
		}
	}
#endif
	gyroscope_init_flag = 0;

	GYRO_DBG("%s: OK\n", __func__);
	return 0;

#if INV_GYRO_AUTO_CALI == 1
exit_create_file_failed:
	device_unregister(inv_daemon_device);
exit_device_register_failed:
	class_destroy(inv_daemon_class);
exit_class_create_failed:
#endif
exit_create_attr_failed:
	/* misc_deregister(&mpu6050_device); */
exit_misc_device_register_failed:
exit_init_failed:
	/* i2c_detach_client(new_client); */
exit_kfree:
	kfree(obj);
exit:
	obj = NULL;
	new_client = NULL;
	obj_i2c_data = NULL;
	mpu6050_i2c_client = NULL;
	GYRO_ERR("%s: err = %d\n", __func__, err);
	gyroscope_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#if INV_GYRO_AUTO_CALI == 1
	{
		int i;

		for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++)
			device_remove_file(inv_daemon_device, inv_daemon_dev_attributes[i]);

		device_unregister(inv_daemon_device);
		class_destroy(inv_daemon_class);
	}
#endif

	err = mpu6050_delete_attr(&(mpu6050_init_info.platform_diver_addr->driver));
	if (err)
		GYRO_ERR("mpu6050_delete_attr fail: %d\n", err);


	err = gyro_factory_device_deregister(&mpu6050g_factory_device);
	if (err)
		GYRO_ERR("misc_deregister fail: %d\n", err);

	mpu6050_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int mpu6050_local_init(struct platform_device *pdev)
{
	if (i2c_add_driver(&mpu6050gy_i2c_driver)) {
		GYRO_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gyroscope_init_flag)
		return -1;

	return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_remove(void)
{
	GYRO_FUN();
	i2c_del_driver(&mpu6050gy_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init mpu6050_init(void)
{
	gyro_driver_add(&mpu6050_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit mpu6050_exit(void)
{
	GYRO_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(mpu6050_init);
module_exit(mpu6050_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6050 gyroscope driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");

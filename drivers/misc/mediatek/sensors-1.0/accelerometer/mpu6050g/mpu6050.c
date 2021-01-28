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

#include <cust_acc.h>
#include "mpu6050.h"
#include <accel.h>
#include <hwmsensor.h>

static DEFINE_MUTEX(mpu6050_i2c_mutex);

/*----------------------------------------------------------------------------*/
/*#define DEBUG 1*/
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU6050_LOWPASS	/*apply low pass filter on output */
#define SW_CALIBRATION
/*----------------------------------------------------------------------------*/
#define MPU6050_AXIS_X          0
#define MPU6050_AXIS_Y          1
#define MPU6050_AXIS_Z          2
#define MPU6050_AXES_NUM        3
#define MPU6050_DATA_LEN        6
/* name must different with gyro mpu6050 */
#define MPU6050_DEV_NAME        "MPU6050G"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu6050_i2c_id[] = {
	{MPU6050_DEV_NAME, 0},
	{}
};

/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int mpu6050_i2c_remove(struct i2c_client *client);
static int mpu6050_i2c_detect(struct i2c_client *client,
	struct i2c_board_info *info);
#ifdef CONFIG_PM_SLEEP
static int mpu6050_suspend(struct device *dev);
static int mpu6050_resume(struct device *dev);
#endif

static int mpu6050_local_init(void);
static int mpu6050_remove(void);
static int gsensor_init_flag = -1; /*0<==>OK -1 <==> fail*/

static struct acc_init_info mpu6050_init_info = {
		.name = "mpu6050g",
		.init = mpu6050_local_init,
		.uninit = mpu6050_remove,
};

/*----------------------------------------------------------------------------*/
enum {
	MPU6050_TRC_FILTER = 0x01,
	MPU6050_TRC_RAWDATA = 0x02,
	MPU6050_TRC_IOCTL = 0x04,
	MPU6050_TRC_CALI = 0X08,
	MPU6050_TRC_INFO = 0X10,
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
	struct acc_hw hw;
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
#if defined(USE_EARLY_SUSPEND)
	struct early_suspend early_drv;
#endif
	u8 bandwidth;
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id accel_of_match[] = {
	{.compatible = "mediatek,gsensor"},
	{},
};
#endif
#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops mpu6050_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mpu6050_suspend, mpu6050_resume)
};
#endif
static struct i2c_driver mpu6050g_i2c_driver = {
	.driver = {
		.name = MPU6050_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		.pm = &mpu6050_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = accel_of_match,
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
static struct GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8] = { 0 };


/*----------------------------------------------------------------------------*/
#define MPU6050G_DEBUG 0
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               pr_debug(GSE_TAG"%s\n", __func__)
#define GSE_ERR(fmt, args...)    pr_debug(GSE_TAG"%s %d : "fmt, \
__func__, __LINE__, ##args)
#if MPU6050G_DEBUG
#define GSE_LOG(fmt, args...)    pr_debug(GSE_TAG fmt, ##args)
#else
#define GSE_LOG(fmt, args...)
#endif
/*----------------------------------------------------------------------------*/
static struct data_resolution mpu6050_data_resolution[] = {
	/*8 combination by {FULL_RES,RANGE} */
	{{0, 6}, 16384},	/*+/-2g  in 16-bit resolution:  0.06 mg/LSB */
	{{0, 12}, 8192},	/*+/-4g  in 16-bit resolution:  0.12 mg/LSB */
	{{0, 24}, 4096},	/*+/-8g  in 16-bit resolution:  0.24 mg/LSB */
	{{0, 5}, 2048},		/*+/-16g in 16-bit resolution:  0.49 mg/LSB */
};
/*----------------------------------------------------------------------------*/
static struct data_resolution mpu6050_offset_resolution = { {0, 5}, 2048 };

static unsigned int power_on;

int MPU6050_gse_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(MPU6050_gse_power);

int MPU6050_gse_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(MPU6050_gse_mode);


/*----------------------------------------------------------------------------*/
static int mpu_i2c_read_block(struct i2c_client *client, u8 addr,
	u8 *data, u8 len)
{
	int err;
	u8 beg = addr;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&mpu6050_i2c_mutex);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;


	if (!client) {
		mutex_unlock(&mpu6050_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&mpu6050_i2c_mutex);
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&mpu6050_i2c_mutex);
	return err;

}

static int mpu_i2c_write_block(struct i2c_client *client, u8 addr,
	u8 *data, u8 len)
{
	/* address also occupies one byte, the max length for write is 7 bytes*/
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&mpu6050_i2c_mutex);
	if (!client) {
		mutex_unlock(&mpu6050_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		mutex_unlock(&mpu6050_i2c_mutex);
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		mutex_unlock(&mpu6050_i2c_mutex);
		GSE_ERR("send command error!!\n");
		return -EFAULT;
	}
	mutex_unlock(&mpu6050_i2c_mutex);
	return err;
}

int MPU6050_hwmsen_read_block(u8 addr, u8 *buf, u8 len)
{
	if (mpu6050_i2c_client == NULL) {
		GSE_ERR("%s null ptr!!\n", __func__);
		return -EINVAL;
	}
	return mpu_i2c_read_block(mpu6050_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(MPU6050_hwmsen_read_block);

int MPU6050_hwmsen_write_block(u8 addr, u8 *buf, u8 len)
{
	if (mpu6050_i2c_client == NULL) {
		GSE_ERR("%s null ptr!!\n", __func__);
		return -EINVAL;
	}
	return mpu_i2c_write_block(mpu6050_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(MPU6050_hwmsen_write_block);

/*----------------------------------------------------------------------------*/
static int MPU6050_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];
	int res = 0;
	/* u8 addr = MPU6050_REG_POWER_CTL; */
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);


	if (enable == sensor_power) {
		GSE_LOG("Sensor power status is newest!\n");
		return 0;
	}

	res = mpu_i2c_read_block(client, MPU6050_REG_POWER_CTL, databuf, 0x1);
	if (res < 0)
		return res;

	databuf[0] &= ~MPU6050_SLEEP;

	if (enable == false) {
		if (MPU6050_gyro_mode() == false)
			databuf[0] |= MPU6050_SLEEP;

	} else {
		/* do nothing */
	}

	res = mpu_i2c_write_block(client, MPU6050_REG_POWER_CTL, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("set power mode failed!\n");
		return res;
	} else if (atomic_read(&obj->trace) & MPU6050_TRC_INFO)
		GSE_LOG("set power mode ok %d!\n", databuf[0]);

	sensor_power = enable;
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetDataResolution(struct mpu6050_i2c_data *obj)
{
	int err;
	u8 dat, reso;

	err = mpu_i2c_read_block(obj->client, MPU6050_REG_DATA_FORMAT, &dat, 1);
	if (err) {
		GSE_ERR("write data format fail!!\n");
		return err;
	}

	/*the data_reso is combined by 3 bits: {FULL_RES, DATA_RANGE} */
	reso = 0x00;
	reso = (dat & MPU6050_RANGE_16G) >> 3;

	if (reso < ARRAY_SIZE(mpu6050_data_resolution)) {
		obj->reso = &mpu6050_data_resolution[reso];
		return 0;
	} else {
		return -EINVAL;
	}
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadData(struct i2c_client *client,
	s16 data[MPU6050_AXES_NUM])
{
	struct mpu6050_i2c_data *priv = i2c_get_clientdata(client);
	u8 buf[MPU6050_DATA_LEN] = { 0 };
	int err = 0;


	if (client == NULL)
		return -EINVAL;

	/* write then burst read */
	mpu_i2c_read_block(client, MPU6050_REG_DATAX0, buf, MPU6050_DATA_LEN);

	data[MPU6050_AXIS_X] = (s16) ((buf[MPU6050_AXIS_X * 2] << 8) |
				      (buf[MPU6050_AXIS_X * 2 + 1]));
	data[MPU6050_AXIS_Y] = (s16) ((buf[MPU6050_AXIS_Y * 2] << 8) |
				      (buf[MPU6050_AXIS_Y * 2 + 1]));
	data[MPU6050_AXIS_Z] = (s16) ((buf[MPU6050_AXIS_Z * 2] << 8) |
				      (buf[MPU6050_AXIS_Z * 2 + 1]));

	if (atomic_read(&priv->trace) & MPU6050_TRC_RAWDATA) {
		GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n",
			data[MPU6050_AXIS_X], data[MPU6050_AXIS_Y],
			data[MPU6050_AXIS_Z], data[MPU6050_AXIS_X],
			data[MPU6050_AXIS_Y], data[MPU6050_AXIS_Z]);
	}
#ifdef CONFIG_MPU6050_LOWPASS
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) &&
			!atomic_read(&priv->suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][MPU6050_AXIS_X] =
					data[MPU6050_AXIS_X];
				priv->fir.raw[priv->fir.num][MPU6050_AXIS_Y] =
					data[MPU6050_AXIS_Y];
				priv->fir.raw[priv->fir.num][MPU6050_AXIS_Z] =
					data[MPU6050_AXIS_Z];
				priv->fir.sum[MPU6050_AXIS_X] +=
					data[MPU6050_AXIS_X];
				priv->fir.sum[MPU6050_AXIS_Y] +=
					data[MPU6050_AXIS_Y];
				priv->fir.sum[MPU6050_AXIS_Z] +=
					data[MPU6050_AXIS_Z];
				if (atomic_read(&priv->trace) &
					MPU6050_TRC_FILTER) {
					GSE_LOG("add [%2d] [%5d %5d %5d] => ",
				priv->fir.num,
				priv->fir.raw[priv->fir.num][MPU6050_AXIS_X],
				priv->fir.raw[priv->fir.num][MPU6050_AXIS_Y],
				priv->fir.raw[priv->fir.num][MPU6050_AXIS_Z]);
					GSE_LOG("[%5d %5d %5d]\n",
					priv->fir.sum[MPU6050_AXIS_X],
					priv->fir.sum[MPU6050_AXIS_Y],
					priv->fir.sum[MPU6050_AXIS_Z]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[MPU6050_AXIS_X] -=
					priv->fir.raw[idx][MPU6050_AXIS_X];
				priv->fir.sum[MPU6050_AXIS_Y] -=
					priv->fir.raw[idx][MPU6050_AXIS_Y];
				priv->fir.sum[MPU6050_AXIS_Z] -=
					priv->fir.raw[idx][MPU6050_AXIS_Z];
				priv->fir.raw[idx][MPU6050_AXIS_X] =
					data[MPU6050_AXIS_X];
				priv->fir.raw[idx][MPU6050_AXIS_Y] =
					data[MPU6050_AXIS_Y];
				priv->fir.raw[idx][MPU6050_AXIS_Z] =
					data[MPU6050_AXIS_Z];
				priv->fir.sum[MPU6050_AXIS_X] +=
					data[MPU6050_AXIS_X];
				priv->fir.sum[MPU6050_AXIS_Y] +=
					data[MPU6050_AXIS_Y];
				priv->fir.sum[MPU6050_AXIS_Z] +=
					data[MPU6050_AXIS_Z];
				priv->fir.idx++;
				data[MPU6050_AXIS_X] =
					priv->fir.sum[MPU6050_AXIS_X]/firlen;
				data[MPU6050_AXIS_Y] =
					priv->fir.sum[MPU6050_AXIS_Y]/firlen;
				data[MPU6050_AXIS_Z] =
					priv->fir.sum[MPU6050_AXIS_Z]/firlen;
				if (atomic_read(&priv->trace) &
					MPU6050_TRC_FILTER)
					GSE_LOG("add [%2d] [%5d %5d %5d] =>",
					idx,
					priv->fir.raw[idx][MPU6050_AXIS_X],
					priv->fir.raw[idx][MPU6050_AXIS_Y],
					priv->fir.raw[idx][MPU6050_AXIS_Z]);
					GSE_LOG("[%5d %5d %5d] : ",
					priv->fir.sum[MPU6050_AXIS_X],
					priv->fir.sum[MPU6050_AXIS_Y],
					priv->fir.sum[MPU6050_AXIS_Z]);

					GSE_LOG("[%5d %5d %5d]\n",
					data[MPU6050_AXIS_X],
					data[MPU6050_AXIS_Y],
					data[MPU6050_AXIS_Z]);
			}
		}
	}
#endif
	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadOffset(struct i2c_client *client,
	s8 ofs[MPU6050_AXES_NUM])
{
	int err = 0;
#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#else
	err = mpu_i2c_read_block(client, MPU6050_REG_OFSX,
		ofs, MPU6050_AXES_NUM);
	if (err)
		GSE_ERR("error: %d\n", err);

#endif
	/* GSE_LOG("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]); */

	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ResetCalibration(struct i2c_client *client)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
	s8 ofs[MPU6050_AXES_NUM] = { 0x00, 0x00, 0x00 };
#endif
	int err = 0;
#ifdef SW_CALIBRATION
	/* do not thing */
#else
	err = hwmsen_write_block(client, MPU6050_REG_OFSX,
		ofs, MPU6050_AXES_NUM);
	if (err)
		GSE_ERR("error: %d\n", err);

#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));

	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadCalibration(struct i2c_client *client,
	int dat[MPU6050_AXES_NUM])
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
	int mul;
#else
	int err;
#endif
#ifdef SW_CALIBRATION
	/* only SW Calibration, disable HW Calibration */
	mul = 0;
#else

	err = MPU6050_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / mpu6050_offset_resolution.sensitivity;
#endif
	dat[obj->cvt.map[MPU6050_AXIS_X]] =
		obj->cvt.sign[MPU6050_AXIS_X] *
		(obj->offset[MPU6050_AXIS_X]*mul +
		obj->cali_sw[MPU6050_AXIS_X]);
	dat[obj->cvt.map[MPU6050_AXIS_Y]] =
		obj->cvt.sign[MPU6050_AXIS_Y] *
		(obj->offset[MPU6050_AXIS_Y]*mul +
		obj->cali_sw[MPU6050_AXIS_Y]);
	dat[obj->cvt.map[MPU6050_AXIS_Z]] =
		obj->cvt.sign[MPU6050_AXIS_Z]*
		(obj->offset[MPU6050_AXIS_Z]*mul +
		obj->cali_sw[MPU6050_AXIS_Z]);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadCalibrationEx(struct i2c_client *client,
	int act[MPU6050_AXES_NUM], int raw[MPU6050_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data */
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
	int mul;
#else
	int err;
#endif
#ifdef SW_CALIBRATION
	/* only SW Calibration, disable HW Calibration */
	mul = 0;
#else

	err = MPU6050_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / mpu6050_offset_resolution.sensitivity;
#endif

	raw[MPU6050_AXIS_X] = obj->offset[MPU6050_AXIS_X]*mul +
	obj->cali_sw[MPU6050_AXIS_X];
	raw[MPU6050_AXIS_Y] = obj->offset[MPU6050_AXIS_Y]*mul +
		obj->cali_sw[MPU6050_AXIS_Y];
	raw[MPU6050_AXIS_Z] = obj->offset[MPU6050_AXIS_Z]*mul +
		obj->cali_sw[MPU6050_AXIS_Z];

	act[obj->cvt.map[MPU6050_AXIS_X]] =
		obj->cvt.sign[MPU6050_AXIS_X] * raw[MPU6050_AXIS_X];
	act[obj->cvt.map[MPU6050_AXIS_Y]] =
		obj->cvt.sign[MPU6050_AXIS_Y] * raw[MPU6050_AXIS_Y];
	act[obj->cvt.map[MPU6050_AXIS_Z]] =
		obj->cvt.sign[MPU6050_AXIS_Z] * raw[MPU6050_AXIS_Z];

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_WriteCalibration(struct i2c_client *client,
	int dat[MPU6050_AXES_NUM])
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[MPU6050_AXES_NUM], raw[MPU6050_AXES_NUM];
#ifndef SW_CALIBRATION
	int lsb = mpu6050_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity / lsb;
#endif
	err = MPU6050_ReadCalibrationEx(client, cali, raw);
	if (err) {	/*offset will be updated in obj->offset */
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

GSE_LOG("OLDOFF:(%+3d %+3d %+3d):(%+3d %+3d %+3d)/(%+3d %+3d %+3d)\n",
		raw[MPU6050_AXIS_X], raw[MPU6050_AXIS_Y],
		raw[MPU6050_AXIS_Z], obj->offset[MPU6050_AXIS_X],
		obj->offset[MPU6050_AXIS_Y], obj->offset[MPU6050_AXIS_Z],
		obj->cali_sw[MPU6050_AXIS_X], obj->cali_sw[MPU6050_AXIS_Y],
		obj->cali_sw[MPU6050_AXIS_Z]);

	/*calculate the real offset expected by caller */
	cali[MPU6050_AXIS_X] += dat[MPU6050_AXIS_X];
	cali[MPU6050_AXIS_Y] += dat[MPU6050_AXIS_Y];
	cali[MPU6050_AXIS_Z] += dat[MPU6050_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
		dat[MPU6050_AXIS_X], dat[MPU6050_AXIS_Y], dat[MPU6050_AXIS_Z]);
#ifdef SW_CALIBRATION
	obj->cali_sw[MPU6050_AXIS_X] = obj->cvt.sign[MPU6050_AXIS_X] *
		(cali[obj->cvt.map[MPU6050_AXIS_X]]);
	obj->cali_sw[MPU6050_AXIS_Y] = obj->cvt.sign[MPU6050_AXIS_Y] *
		(cali[obj->cvt.map[MPU6050_AXIS_Y]]);
	obj->cali_sw[MPU6050_AXIS_Z] = obj->cvt.sign[MPU6050_AXIS_Z] *
		(cali[obj->cvt.map[MPU6050_AXIS_Z]]);
#else

	obj->offset[MPU6050_AXIS_X] =
		(s8)(obj->cvt.sign[MPU6050_AXIS_X] *
		(cali[obj->cvt.map[MPU6050_AXIS_X]])/(divisor));
	obj->offset[MPU6050_AXIS_Y] =
		(s8)(obj->cvt.sign[MPU6050_AXIS_Y] *
		(cali[obj->cvt.map[MPU6050_AXIS_Y]])/(divisor));
	obj->offset[MPU6050_AXIS_Z] =
		(s8)(obj->cvt.sign[MPU6050_AXIS_Z] *
		(cali[obj->cvt.map[MPU6050_AXIS_Z]])/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[MPU6050_AXIS_X] =
		obj->cvt.sign[MPU6050_AXIS_X] *
		(cali[obj->cvt.map[MPU6050_AXIS_X]])%(divisor);
	obj->cali_sw[MPU6050_AXIS_Y] =
		obj->cvt.sign[MPU6050_AXIS_Y] *
		(cali[obj->cvt.map[MPU6050_AXIS_Y]])%(divisor);
	obj->cali_sw[MPU6050_AXIS_Z] =
		obj->cvt.sign[MPU6050_AXIS_Z] *
		(cali[obj->cvt.map[MPU6050_AXIS_Z]])%(divisor);

GSE_LOG("NEWOFF:(%+3d %+3d %+3d):(%+3d %+3d %+3d)/(%+3d %+3d %+3d)\n",
		obj->offset[MPU6050_AXIS_X]*divisor +
		obj->cali_sw[MPU6050_AXIS_X],
		obj->offset[MPU6050_AXIS_Y]*divisor +
		obj->cali_sw[MPU6050_AXIS_Y],
		obj->offset[MPU6050_AXIS_Z]*divisor +
		obj->cali_sw[MPU6050_AXIS_Z],
		obj->offset[MPU6050_AXIS_X], obj->offset[MPU6050_AXIS_Y],
		obj->offset[MPU6050_AXIS_Z], obj->cali_sw[MPU6050_AXIS_X],
		obj->cali_sw[MPU6050_AXIS_Y], obj->cali_sw[MPU6050_AXIS_Z]);

	err = hwmsen_write_block(obj->client, MPU6050_REG_OFSX,
		obj->offset, MPU6050_AXES_NUM);
	if (err) {
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);
	res = mpu_i2c_read_block(client, MPU6050_REG_DEVID, databuf, 0x1);
	if (res < 0)
		goto exit_MPU6050_CheckDeviceID;

	GSE_LOG("%s 0x%x\n", __func__, databuf[0]);
exit_MPU6050_CheckDeviceID:
	if (res < 0)
		return res;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);
	res = mpu_i2c_read_block(client, MPU6050_REG_DATA_FORMAT,
		databuf, 0x1);
	if (res < 0)
		return res;

	/* write */
	databuf[0] = databuf[0] | dataformat;
	res = mpu_i2c_write_block(client, MPU6050_REG_DATA_FORMAT,
		databuf, 0x1);

	if (res < 0)
		return res;
	return MPU6050_SetDataResolution(obj);
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];
	int res = 0;

	if ((obj->bandwidth != bwrate) || (atomic_read(&obj->suspend))) {
		memset(databuf, 0, sizeof(u8)*10);

		/* read */
		res = mpu_i2c_read_block(client, MPU6050_REG_BW_RATE,
			databuf, 0x1);
		if (res < 0)
			return res;

		/* write */
		databuf[0] = databuf[0] | bwrate;
		res = mpu_i2c_write_block(client, MPU6050_REG_BW_RATE,
			databuf, 0x1);

		if (res < 0)
			return res;

		obj->bandwidth = bwrate;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_Dev_Reset(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	/* read */
	res = mpu_i2c_read_block(client, MPU6050_REG_POWER_CTL,
		databuf, 0x1);
	if (res < 0)
		return res;

	/* write */
	databuf[0] = databuf[0] | MPU6050_DEV_RESET;
	res = mpu_i2c_write_block(client, MPU6050_REG_POWER_CTL
		, databuf, 0x1);

	if (res < 0)
		return res;

	do {
		res = mpu_i2c_read_block(client, MPU6050_REG_POWER_CTL,
			databuf, 0x1);
		if (res < 0)
			return res;
		GSE_LOG("[Gsensor] check reset bit");
	} while ((databuf[0]&MPU6050_DEV_RESET) != 0);

	msleep(50);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_Reset(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	/* write */
	databuf[0] = 0x7; /* reset gyro, g-sensor, temperature */
	res = mpu_i2c_write_block(client, MPU6050_REG_RESET, databuf, 0x1);

	if (res < 0)
		return res;

	msleep(20);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[2];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);
	databuf[0] = intenable;
	res = mpu_i2c_write_block(client, MPU6050_REG_INT_ENABLE, databuf, 0x1);

	if (res < 0)
		return res;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_gpio_config(void)
{
	return 0;
}

static int mpu6050_init_client(struct i2c_client *client, int reset_cali)
{
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	mpu6050_gpio_config();

	res = MPU6050_SetPowerMode(client, true);
	if (res) {
		GSE_ERR("set power error\n");
		return res;
	}
	res = MPU6050_CheckDeviceID(client);
	if (res) {
		GSE_ERR("Check ID error\n");
		return res;
	}

	res = MPU6050_SetBWRate(client, MPU6050_BW_184HZ);
	if (res)	{ /* 0x2C->BW=100Hz */
		GSE_ERR("set power error\n");
		return res;
	}

	res = MPU6050_SetDataFormat(client, MPU6050_RANGE_16G);
	if (res)	{ /* 0x2C->BW=100Hz */
		GSE_ERR("set data format error\n");
		return res;
	}

	gsensor_gain.x = gsensor_gain.y =
		gsensor_gain.z = obj->reso->sensitivity;

	res = MPU6050_SetIntEnable(client, 0x00);	/* disable INT */
	if (res) {
		GSE_ERR("mpu6050_SetIntEnable error\n");
		return res;
	}

	if (reset_cali != 0) {
		/*reset calibration only in power on */
		res = MPU6050_ResetCalibration(client);
		if (res)
			return res;

	}
#ifdef CONFIG_MPU6050_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadAllReg(struct i2c_client *client, char *buf, int bufsize)
{
	u8 total_len = 0x5C;	/* (0x75-0x19); */

	u8 addr = 0x19;
	u8 buff[total_len + 1];
	int err = 0;
	int i;


	if (sensor_power == false) {
		err = MPU6050_SetPowerMode(client, true);
		if (err)
			GSE_ERR("Power on mpu6050 error %d!\n", err);
			msleep(50);
	}

	mpu_i2c_read_block(client, addr, buff, total_len);

	for (i = 0; i <= total_len; i++)
		GSE_LOG("MPU6050 reg=0x%x, data=0x%x\n", (addr + i), buff[i]);


	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadChipInfo(struct i2c_client *client,
	char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30))
		return -1;


	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "MPU6050 Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadSensorData(struct i2c_client *client,
	char *buf, int bufsize)
{
	/* (struct mpu6050_i2c_data*)i2c_get_clientdata(client); */
	struct mpu6050_i2c_data *obj = obj_i2c_data;
	int acc[MPU6050_AXES_NUM];
	int res = 0;

	client = obj->client;

	if (atomic_read(&obj->suspend))
		return -3;


	if (buf == NULL)
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	if (sensor_power == false) {
		res = MPU6050_SetPowerMode(client, true);
		if (res)
			GSE_ERR("Power on mpu6050 error %d!\n", res);
			msleep(50);
	}

	res = MPU6050_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	obj->data[MPU6050_AXIS_X] += obj->cali_sw[MPU6050_AXIS_X];
	obj->data[MPU6050_AXIS_Y] += obj->cali_sw[MPU6050_AXIS_Y];
	obj->data[MPU6050_AXIS_Z] += obj->cali_sw[MPU6050_AXIS_Z];

	/*remap coordinate*/
	acc[obj->cvt.map[MPU6050_AXIS_X]] =
	obj->cvt.sign[MPU6050_AXIS_X]*obj->data[MPU6050_AXIS_X];
	acc[obj->cvt.map[MPU6050_AXIS_Y]] =
		obj->cvt.sign[MPU6050_AXIS_Y]*obj->data[MPU6050_AXIS_Y];
	acc[obj->cvt.map[MPU6050_AXIS_Z]] =
		obj->cvt.sign[MPU6050_AXIS_Z]*obj->data[MPU6050_AXIS_Z];

	/* Out put the mg */
	acc[MPU6050_AXIS_X] = acc[MPU6050_AXIS_X] *
	GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	acc[MPU6050_AXIS_Y] = acc[MPU6050_AXIS_Y] *
		GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	acc[MPU6050_AXIS_Z] = acc[MPU6050_AXIS_Z] *
		GRAVITY_EARTH_1000 / obj->reso->sensitivity;

	sprintf(buf, "%04x %04x %04x", acc[MPU6050_AXIS_X],
		acc[MPU6050_AXIS_Y], acc[MPU6050_AXIS_Z]);
	if (atomic_read(&obj->trace) & MPU6050_TRC_IOCTL)
		GSE_LOG("gsensor data: %s!\n", buf);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_ReadRawData(struct i2c_client *client, char *buf)
{
	struct mpu6050_i2c_data *obj =
		(struct mpu6050_i2c_data *)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
		return -EINVAL;

	if (atomic_read(&obj->suspend))
		return -EIO;

	res = MPU6050_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -EIO;
	}
	sprintf(buf, "%04x %04x %04x", obj->data[MPU6050_AXIS_X],
	obj->data[MPU6050_AXIS_Y], obj->data[MPU6050_AXIS_Z]);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
	u8 data;

	res = MPU6050_SetBWRate(client, MPU6050_BW_184HZ);
	if (res)	{ /* 0x2C->BW=100Hz */
		return res;
	}

	res = mpu_i2c_read_block(client, MPU6050_REG_DATA_FORMAT,
		&data, 1);

	if (res)
		return res;


	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_JudgeTestResult(struct i2c_client *client,
	s32 prv[MPU6050_AXES_NUM], s32 nxt[MPU6050_AXES_NUM])
{
	struct criteria {
		int min;
		int max;
	};

	struct criteria self[4][3] = {
		{{0, 540}, {0, 540}, {0, 875} },
		{{0, 270}, {0, 270}, {0, 438} },
		{{0, 135}, {0, 135}, {0, 219} },
		{{0, 67}, {0, 67}, {0, 110} },
	};
	struct criteria (*ptr)[3] = NULL;
	u8 format;
	int res;

	res = mpu_i2c_read_block(client, MPU6050_REG_DATA_FORMAT, &format, 1);
	if (res)
		return res;

	format = format & MPU6050_RANGE_16G;

	switch (format) {
	case MPU6050_RANGE_2G:
		/*GSE_LOG("format use self[0]\n");*/
		ptr = &self[0];
		break;

	case MPU6050_RANGE_4G:
		/*GSE_LOG("format use self[1]\n");*/
		ptr = &self[1];
		break;

	case MPU6050_RANGE_8G:
		/*GSE_LOG("format use self[2]\n");*/
		ptr = &self[2];
		break;

	case MPU6050_RANGE_16G:
		/*GSE_LOG("format use self[3]\n");*/
		ptr = &self[3];
		break;

	default:
		GSE_LOG("format unknown use\n");
		break;
	}

	if (!ptr) {
		GSE_ERR("null pointer\n");
		return -EINVAL;
	}
	GSE_LOG("format=0x%x\n", format);

	GSE_LOG("X diff is %ld\n",
		abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]));
	GSE_LOG("Y diff is %ld\n",
		abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]));
	GSE_LOG("Z diff is %ld\n",
		abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]));


	if ((abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]) >
		(*ptr)[MPU6050_AXIS_X].max) ||
	    (abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]) <
	    (*ptr)[MPU6050_AXIS_X].min)) {
		GSE_ERR("X is over range\n");
		res = -EINVAL;
	}
	if ((abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]) >
		(*ptr)[MPU6050_AXIS_Y].max) ||
	    (abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]) <
	    (*ptr)[MPU6050_AXIS_Y].min)) {
		GSE_ERR("Y is over range\n");
		res = -EINVAL;
	}
	if ((abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]) >
		(*ptr)[MPU6050_AXIS_Z].max) ||
	    (abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]) <
	    (*ptr)[MPU6050_AXIS_Z].min)) {
		GSE_ERR("Z is over range\n");
		res = -EINVAL;
	}
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;
	char strbuf[MPU6050_BUFSIZE];

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	if (sensor_power == false)
		MPU6050_SetPowerMode(client, true);
	msleep(50);


	MPU6050_ReadAllReg(client, strbuf, MPU6050_BUFSIZE);

	MPU6050_ReadChipInfo(client, strbuf, MPU6050_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;
	char strbuf[MPU6050_BUFSIZE];

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	MPU6050_ReadSensorData(client, strbuf, MPU6050_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;
	struct mpu6050_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[MPU6050_AXES_NUM];

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	err = MPU6050_ReadOffset(client, obj->offset);
	if (err)
		return -EINVAL;
	err = MPU6050_ReadCalibration(client, tmp);
	if (err)
		return -EINVAL;

	mul = obj->reso->sensitivity/mpu6050_offset_resolution.sensitivity;
	len += snprintf(buf+len, PAGE_SIZE-len,
		"[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n",
		mul, obj->offset[MPU6050_AXIS_X],
		obj->offset[MPU6050_AXIS_Y], obj->offset[MPU6050_AXIS_Z],
		obj->offset[MPU6050_AXIS_X], obj->offset[MPU6050_AXIS_Y],
		obj->offset[MPU6050_AXIS_Z]);
	len += snprintf(buf+len, PAGE_SIZE-len,
		"[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
		obj->cali_sw[MPU6050_AXIS_X], obj->cali_sw[MPU6050_AXIS_Y],
		obj->cali_sw[MPU6050_AXIS_Z]);

	len += snprintf(buf+len, PAGE_SIZE-len,
		"[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
		obj->offset[MPU6050_AXIS_X] * mul +
		obj->cali_sw[MPU6050_AXIS_X],
		obj->offset[MPU6050_AXIS_Y] * mul +
		obj->cali_sw[MPU6050_AXIS_Y],
		obj->offset[MPU6050_AXIS_Z] * mul +
		obj->cali_sw[MPU6050_AXIS_Z],
		tmp[MPU6050_AXIS_X], tmp[MPU6050_AXIS_Y],
		tmp[MPU6050_AXIS_Z]);
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct i2c_client *client = mpu6050_i2c_client;
	int err, x, y, z;
	int dat[MPU6050_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		err = MPU6050_ResetCalibration(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);

	} else if (sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z) == 3) {
		dat[MPU6050_AXIS_X] = x;
		dat[MPU6050_AXIS_Y] = y;
		dat[MPU6050_AXIS_Z] = z;
		err = MPU6050_WriteCalibration(client, dat);
		if (err)
			GSE_ERR("write calibration err = %d\n", err);

	} else {
		GSE_ERR("invalid format\n");
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	return snprintf(buf, 8, "%s\n", selftestRes);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	/*write anything to this register will trigger the process */
	struct item {
		s16 raw[MPU6050_AXES_NUM];
	};

	struct i2c_client *client = mpu6050_i2c_client;
	int idx, res, num;
	long prv_len, nxt_len;
	struct item *prv = NULL, *nxt = NULL;
	s32 avg_prv[MPU6050_AXES_NUM] = { 0, 0, 0 };
	s32 avg_nxt[MPU6050_AXES_NUM] = { 0, 0, 0 };


	res = kstrtoint(buf, 10, &num);
	if (res != 0) {
		GSE_ERR("parse number fail\n");
		return count;
	} else if (num == 0) {
		GSE_ERR("invalid data count\n");
		return count;
	}
	prv_len = sizeof(*prv) * num;
	nxt_len = sizeof(*nxt) * num;
	prv = kzalloc(prv_len, GFP_KERNEL);
	nxt = kzalloc(prv_len, GFP_KERNEL);
	if (!prv || !nxt)
		goto exit;



	/*GSE_LOG("NORMAL:\n");*/
	MPU6050_SetPowerMode(client, true);
	msleep(50);

	for (idx = 0; idx < num; idx++) {
		res = MPU6050_ReadData(client, prv[idx].raw);
		if (res) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}

		avg_prv[MPU6050_AXIS_X] += prv[idx].raw[MPU6050_AXIS_X];
		avg_prv[MPU6050_AXIS_Y] += prv[idx].raw[MPU6050_AXIS_Y];
		avg_prv[MPU6050_AXIS_Z] += prv[idx].raw[MPU6050_AXIS_Z];
		GSE_LOG("Normal:[%5d %5d %5d]\n",
			prv[idx].raw[MPU6050_AXIS_X],
			prv[idx].raw[MPU6050_AXIS_Y],
			prv[idx].raw[MPU6050_AXIS_Z]);
	}

	avg_prv[MPU6050_AXIS_X] /= num;
	avg_prv[MPU6050_AXIS_Y] /= num;
	avg_prv[MPU6050_AXIS_Z] /= num;

	/*initial setting for self test */
	/*GSE_LOG("SELFTEST:\n");*/
	for (idx = 0; idx < num; idx++) {
		res = MPU6050_ReadData(client, nxt[idx].raw);
		if (res) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}
		avg_nxt[MPU6050_AXIS_X] += nxt[idx].raw[MPU6050_AXIS_X];
		avg_nxt[MPU6050_AXIS_Y] += nxt[idx].raw[MPU6050_AXIS_Y];
		avg_nxt[MPU6050_AXIS_Z] += nxt[idx].raw[MPU6050_AXIS_Z];
		GSE_LOG("SELFTESt: [%5d %5d %5d]\n",
			nxt[idx].raw[MPU6050_AXIS_X],
			nxt[idx].raw[MPU6050_AXIS_Y],
			nxt[idx].raw[MPU6050_AXIS_Z]);
	}

	avg_nxt[MPU6050_AXIS_X] /= num;
	avg_nxt[MPU6050_AXIS_Y] /= num;
	avg_nxt[MPU6050_AXIS_Z] /= num;

	GSE_LOG("X: %5d - %5d = %5d\n",
		avg_nxt[MPU6050_AXIS_X], avg_prv[MPU6050_AXIS_X],
		avg_nxt[MPU6050_AXIS_X] - avg_prv[MPU6050_AXIS_X]);
	GSE_LOG("Y: %5d - %5d = %5d\n",
		avg_nxt[MPU6050_AXIS_Y], avg_prv[MPU6050_AXIS_Y],
		avg_nxt[MPU6050_AXIS_Y] - avg_prv[MPU6050_AXIS_Y]);
	GSE_LOG("Z: %5d - %5d = %5d\n",
		avg_nxt[MPU6050_AXIS_Z], avg_prv[MPU6050_AXIS_Z],
		avg_nxt[MPU6050_AXIS_Z] - avg_prv[MPU6050_AXIS_Z]);

	if (!MPU6050_JudgeTestResult(client, avg_prv, avg_nxt)) {
		GSE_LOG("SELFTEST : PASS\n");
		strcpy(selftestRes, "y");
	} else {
		GSE_ERR("SELFTEST : FAIL\n");
		strcpy(selftestRes, "n");
	}

exit:
	/*restore the setting */
	mpu6050_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050_i2c_client;
	struct mpu6050_i2c_data *obj;

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->selftest));
}

/*----------------------------------------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct mpu6050_i2c_data *obj = obj_i2c_data;
	int tmp;

	if (obj == NULL) {
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}

	if (kstrtoint(buf, 10, &tmp) == 0) {
		if (atomic_read(&obj->selftest) && !tmp) {
			/*enable -> disable */
			mpu6050_init_client(obj->client, 0);
		} else if (!atomic_read(&obj->selftest) && tmp) {
			/*disable -> enable */
			MPU6050_InitSelfTest(obj->client);
		}

		GSE_LOG("selftest: %d => %d\n",
			atomic_read(&obj->selftest), tmp);
			atomic_set(&obj->selftest, tmp);
	} else {
		GSE_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_MPU6050_LOWPASS
	struct i2c_client *client = mpu6050_i2c_client;
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);

	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++)
			GSE_LOG("[%5d %5d %5d]\n",
				obj->fir.raw[idx][MPU6050_AXIS_X],
				obj->fir.raw[idx][MPU6050_AXIS_Y],
				obj->fir.raw[idx][MPU6050_AXIS_Z]);

		GSE_LOG("sum = [%5d %5d %5d]\n",
			obj->fir.sum[MPU6050_AXIS_X],
			obj->fir.sum[MPU6050_AXIS_Y],
			obj->fir.sum[MPU6050_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n",
			obj->fir.sum[MPU6050_AXIS_X]/len,
			obj->fir.sum[MPU6050_AXIS_Y]/len,
			obj->fir.sum[MPU6050_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
#ifdef CONFIG_MPU6050_LOWPASS
	struct i2c_client *client = mpu6050_i2c_client;
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int firlen;

	if (kstrtoint(buf, 10, &firlen) != 0) {
		GSE_ERR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		GSE_ERR("exceeds maximum filter length\n");
	} else {
		atomic_set(&obj->firlen, firlen);
		if (firlen == 0) {
			atomic_set(&obj->fir_en, 0);
		} else {
			memset(&obj->fir, 0x00, sizeof(obj->fir));
			atomic_set(&obj->fir_en, 1);
		}
	}
#endif
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mpu6050_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	struct mpu6050_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (kstrtoint(buf, 16, &trace) == 0)
		atomic_set(&obj->trace, trace);
	else
		GSE_ERR("invalid content: '%s', length = %zu\n", buf, count);


	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct mpu6050_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
		obj->hw.i2c_num, obj->hw.direction,
		obj->hw.power_id, obj->hw.power_vol);
	return len;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, 0644, show_cali_value, store_cali_value);
static DRIVER_ATTR(self, 0644, show_selftest_value, store_selftest_value);
static DRIVER_ATTR(selftest, 0644, show_self_value, store_self_value);
static DRIVER_ATTR(firlen, 0644, show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace, 0644, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *mpu6050_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_cali,	/*show calibration data */
	&driver_attr_self,	/*self test demo */
	&driver_attr_selftest,	/*self control: 0: disable, 1: enable */
	&driver_attr_firlen,	/*filter length: 0: disable, others: enable */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
};

/*----------------------------------------------------------------------------*/
static int mpu6050_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(mpu6050_attr_list));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, mpu6050_attr_list[idx]);
		if (err != 0) {
			GSE_ERR("driver_create_file (%s) = %d\n",
				mpu6050_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(mpu6050_attr_list));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, mpu6050_attr_list[idx]);


	return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void *self, uint32_t command, void *buff_in, int size_in,
		    void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value, sample_delay;
	struct mpu6050_i2c_data *priv = (struct mpu6050_i2c_data *)self;
	struct hwm_sensor_data *gsensor_data;
	char buff[MPU6050_BUFSIZE];


	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;

			if (value <= 5)
				sample_delay = MPU6050_BW_184HZ;
			else if (value <= 10)
				sample_delay = MPU6050_BW_94HZ;
			else
				sample_delay = MPU6050_BW_44HZ;

			GSE_LOG("Set delay parameter value:%d\n", value);

			err = MPU6050_SetBWRate(priv->client, sample_delay);
			if (err)	{ /* 0x2C->BW=100Hz */
				GSE_ERR("Set delay parameter error!\n");
			}

			if (value >= 50) {
				atomic_set(&priv->filter, 0);
			} else {
#if defined(CONFIG_MPU6050_LOWPASS)
				priv->fir.num = 0;
				priv->fir.idx = 0;
				priv->fir.sum[MPU6050_AXIS_X] = 0;
				priv->fir.sum[MPU6050_AXIS_Y] = 0;
				priv->fir.sum[MPU6050_AXIS_Z] = 0;
#endif
				atomic_set(&priv->filter, 1);
			}
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (((value == 0) && (sensor_power == false)) ||
				((value == 1) && (sensor_power == true)))
				GSE_LOG("Gsensor device have updated!\n");
			else
				err = MPU6050_SetPowerMode(priv->client,
					!sensor_power);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
			(size_out < sizeof(struct hwm_sensor_data))) {
			GSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			gsensor_data = (struct hwm_sensor_data *) buff_out;
			err = MPU6050_ReadSensorData(priv->client,
				buff, MPU6050_BUFSIZE);
			if (!err) {
				err = sscanf(buff, "%x %x %x",
					&gsensor_data->values[0],
					&gsensor_data->values[1],
					&gsensor_data->values[2]);
				if (err == 3) {
					gsensor_data->status =
						SENSOR_STATUS_ACCURACY_MEDIUM;
					gsensor_data->value_divide = 1000;
				} else
					GSE_ERR("gsensor invaild para !\n");
			}
		}
		break;
	default:
		GSE_ERR("gsensor no this para %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
#if 0
static int mpu6050_open(struct inode *inode, struct file *file)
{
	file->private_data = mpu6050_i2c_client;

	if (file->private_data == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int mpu6050_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long mpu6050_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long err = 0;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}
		err = file->f_op->unlocked_ioctl(file,
			GSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_READ_SENSORDATA failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err = file->f_op->unlocked_ioctl(file,
			GSENSOR_IOCTL_SET_CALI, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_SET_CALI failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}
		err = file->f_op->unlocked_ioctl(file,
			GSENSOR_IOCTL_GET_CALI, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_GET_CALI failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}
		err = file->f_op->unlocked_ioctl(file,
			GSENSOR_IOCTL_CLR_CALI, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_CLR_CALI failed.");
			return err;
		}
		break;
	default:
		GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif

static long mpu6050_unlocked_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client =
		(struct i2c_client *)file->private_data;
	struct mpu6050_i2c_data *obj =
		(struct mpu6050_i2c_data *)i2c_get_clientdata(client);
	char strbuf[MPU6050_BUFSIZE];
	void __user *data;
	struct SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3];

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
			_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
			_IOC_SIZE(cmd));


	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		mpu6050_init_client(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MPU6050_ReadChipInfo(client,
			strbuf, MPU6050_BUFSIZE);
		if (copy_to_user(data, strbuf,
			strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MPU6050_ReadSensorData(client,
			strbuf, MPU6050_BUFSIZE);
		if (copy_to_user(data, strbuf,
			strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_GAIN:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data, &gsensor_gain,
			sizeof(struct GSENSOR_VECTOR3D))) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (atomic_read(&obj->suspend)) {
			err = -EINVAL;
		} else {
			MPU6050_ReadRawData(client, strbuf);
			if (copy_to_user(data, strbuf,
				strlen(strbuf) + 1)) {
				err = -EFAULT;
				break;
			}
		}
		break;

	case GSENSOR_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data,
			sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			GSE_ERR("Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
			cali[MPU6050_AXIS_X] =
				sensor_data.x * obj->reso->sensitivity /
				GRAVITY_EARTH_1000;
			cali[MPU6050_AXIS_Y] =
				sensor_data.y * obj->reso->sensitivity /
				GRAVITY_EARTH_1000;
			cali[MPU6050_AXIS_Z] =
				sensor_data.z * obj->reso->sensitivity /
				GRAVITY_EARTH_1000;
			err = MPU6050_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = MPU6050_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MPU6050_ReadCalibration(client, cali);
		if (err)
			break;

		sensor_data.x =
			cali[MPU6050_AXIS_X] * GRAVITY_EARTH_1000 /
			obj->reso->sensitivity;
		sensor_data.y =
			cali[MPU6050_AXIS_Y] * GRAVITY_EARTH_1000 /
			obj->reso->sensitivity;
		sensor_data.z =
			cali[MPU6050_AXIS_Z] * GRAVITY_EARTH_1000 /
			obj->reso->sensitivity;
		if (copy_to_user(data, &sensor_data,
			sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;


	default:
		GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}


/*----------------------------------------------------------------------------*/
static const struct file_operations mpu6050_fops = {
	.open = mpu6050_open,
	.release = mpu6050_release,
	.unlocked_ioctl = mpu6050_unlocked_ioctl,
    #ifdef CONFIG_COMPAT
	.compat_ioctl = mpu6050_compat_ioctl,
	#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice mpu6050_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &mpu6050_fops,
};
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_PM_SLEEP
/*----------------------------------------------------------------------------*/
static int mpu6050_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	atomic_set(&obj->suspend, 1);

	err = MPU6050_SetPowerMode(obj->client, false);
	if (err) {
		GSE_ERR("write power control fail!!\n");
		return err;
	}
	GSE_LOG("%s ok\n", __func__);

	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	err = mpu6050_init_client(client, 0);
	if (err) {
		GSE_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);
	GSE_LOG("%s ok\n", __func__);

	return 0;
}

/*----------------------------------------------------------------------------*/
#else				/*CONFIG_HAS_EARLY_SUSPEND is defined */
/*----------------------------------------------------------------------------*/
static void mpu6050_early_suspend(struct early_suspend *h)
{
	struct mpu6050_i2c_data *obj = container_of(h,
		struct mpu6050_i2c_data, early_drv);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);

	err = MPU6050_SetPowerMode(obj->client, false);
	if (err) {
		GSE_ERR("write power control fail!!\n");
		return;
	}

	if (MPU6050_gyro_mode() == false) {
		MPU6050_Dev_Reset(obj->client);
		MPU6050_Reset(obj->client);
	}

	obj->bandwidth = 0;

	sensor_power = false;

}

/*----------------------------------------------------------------------------*/
static void mpu6050_late_resume(struct early_suspend *h)
{
	struct mpu6050_i2c_data *obj = container_of(h,
		struct mpu6050_i2c_data, early_drv);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}

	err = mpu6050_init_client(obj->client, 0);
	if (err) {
		GSE_ERR("initialize client fail!!\n");
		return;
	}
	atomic_set(&obj->suspend, 0);
}
/*----------------------------------------------------------------------------*/
#endif				/*CONFIG_HAS_EARLYSUSPEND */
/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_detect(struct i2c_client *client,
	struct i2c_board_info *info)
{
	strcpy(info->type, MPU6050_DEV_NAME);
	return 0;
}



/*
 * if use  this type of enable ,
 * Gsensor should report inputEvent(x, y, z ,stats, div) to HAL
 */
static int gsensor_open_report_data(int open)
{
	/*
	 * should queuq work to report event
	 * if  is_report_input_direct=true
	 */
	return 0;
}

/*
 * if use  this type of enable ,
 * Gsensor only enabled but not report inputEvent to HAL
 */

static int gsensor_enable_nodata(int en)
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
		GSE_ERR("MPU6050_SetPowerMode fail!\n");
		return -1;
	}
	GSE_LOG("mpu6050_enable_nodata OK!\n");
	return 0;
}

static int gsensor_set_delay(u64 ns)
{
	int value = 0;
	int sample_delay = 0;
	int err;

	value = (int)ns/1000/1000;
	if (value <= 5)
		sample_delay = MPU6050_BW_184HZ;
	else if (value <= 10)
		sample_delay = MPU6050_BW_94HZ;
	else
		sample_delay = MPU6050_BW_44HZ;

	err = MPU6050_SetBWRate(obj_i2c_data->client, sample_delay);
	if (err) {
		GSE_ERR("mpu6050_set_delay Set delay parameter error!\n");
		return -1;
	}
	GSE_LOG("mpu6050_set_delay (%d)\n", value);
	return 0;
}
static int gsensor_batch(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs/1000/1000;

	GSE_LOG("mpu6050 acc set delay = (%d) ok.\n", value);
	return gsensor_set_delay(samplingPeriodNs);
}
static int gsensor_flush(void)
{
	return acc_flush_report();
}
/*----------------------------------------------------------------------------*/
static int gsensor_get_data(int *x, int *y, int *z, int *status)
{
	char buff[MPU6050_BUFSIZE];
	int err;

	MPU6050_ReadSensorData(obj_i2c_data->client,
		buff, MPU6050_BUFSIZE);
	err = sscanf(buff, "%x %x %x", x, y, z);
	if (err == 3)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	else
		GSE_ERR("gsensor invaild para!\n");

	return 0;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_factory_enable_sensor(bool enabledisable,
	int64_t sample_periods_ms)
{
	int err;
#if 0
	err = mpu6050_acc_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		GSE_ERR("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = mpu6050_acc_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		GSE_ERR("%s enable set batch failed!\n", __func__);
		return -1;
	}
#endif
	err = mpu6050_init_client(mpu6050_i2c_client, 0);
	if (err) {
		GSE_ERR("initialize client fail!!\n");
		return -1;
	}
	return 0;
}
static int mpu6050_factory_get_data(int32_t data[3], int *status)
{
	int err = 0;
#if 0
	return mpu6050_acc_get_data(&data[0], &data[1], &data[2], status);
#endif
	if (sensor_power == false) {
		err = MPU6050_SetPowerMode(mpu6050_i2c_client, true);
		if (err)
			GSE_ERR("Power on mpu6050 error %d!\n", err);

		msleep(50);
	}
	/* mpu6050_ReadSensorData(client, strbuf, MPU6050_BUFSIZE); */

	return gsensor_get_data(&data[0], &data[1], &data[2], status);
}
static int mpu6050_factory_get_raw_data(int32_t data[3])
{
	char *strbuf;

	strbuf = kmalloc(MPU6050_BUFSIZE, GFP_KERNEL);
	if (!strbuf) {
		GSE_ERR("strbuf is null!!\n");
		return -EINVAL;
	}

	MPU6050_ReadRawData(mpu6050_i2c_client, strbuf);
	if (sscanf(strbuf, "%x %x %x", &data[0], &data[1], &data[2]) != 3)
		GSE_ERR("error sscanf\n");


	kfree(strbuf);

	return 0;
}
static int mpu6050_factory_enable_calibration(void)
{
	return 0;
}
static int mpu6050_factory_clear_cali(void)
{
	int err = 0;

	err = MPU6050_ResetCalibration(mpu6050_i2c_client);
	if (err) {
		GSE_ERR("mpu6050a_ResetCalibration failed!\n");
		return -1;
	}
	return 0;
}
static int mpu6050_factory_set_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
	struct mpu6050_i2c_data *obj = obj_i2c_data;

	cali[MPU6050_AXIS_X] =
	    data[0] * obj->reso->sensitivity / GRAVITY_EARTH_1000;
	cali[MPU6050_AXIS_Y] =
	    data[1] * obj->reso->sensitivity / GRAVITY_EARTH_1000;
	cali[MPU6050_AXIS_Z] =
	    data[2] * obj->reso->sensitivity / GRAVITY_EARTH_1000;
	err = MPU6050_WriteCalibration(mpu6050_i2c_client, cali);
	if (err) {
		GSE_ERR("mpu6050a_WriteCalibration failed!\n");
		return -1;
	}
	return 0;
}
static int mpu6050_factory_get_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };
	struct mpu6050_i2c_data *obj = obj_i2c_data;

	err = MPU6050_ReadCalibration(mpu6050_i2c_client, cali);
	if (err) {
		GSE_ERR("mpu6050a_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[MPU6050_AXIS_X] * GRAVITY_EARTH_1000 /
		obj->reso->sensitivity;
	data[1] = cali[MPU6050_AXIS_Y] * GRAVITY_EARTH_1000 /
		obj->reso->sensitivity;
	data[2] = cali[MPU6050_AXIS_Z] * GRAVITY_EARTH_1000 /
		obj->reso->sensitivity;
	return 0;
}
static int mpu6050_factory_do_self_test(void)
{
	return 0;
}

static struct accel_factory_fops mpu6050a_factory_fops = {
	.enable_sensor = mpu6050_factory_enable_sensor,
	.get_data = mpu6050_factory_get_data,
	.get_raw_data = mpu6050_factory_get_raw_data,
	.enable_calibration = mpu6050_factory_enable_calibration,
	.clear_cali = mpu6050_factory_clear_cali,
	.set_cali = mpu6050_factory_set_cali,
	.get_cali = mpu6050_factory_get_cali,
	.do_self_test = mpu6050_factory_do_self_test,
};

static struct accel_factory_public mpu6050a_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &mpu6050a_factory_fops,
};
/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct i2c_client *new_client = NULL;
	struct mpu6050_i2c_data *obj = NULL;
	int err = 0;
	struct acc_control_path ctl = {0};
	struct acc_data_path data = {0};

	GSE_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	err = get_accel_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		GSE_ERR("get dts info fail\n");
		err = -EFAULT;
		goto exit_kfree;
	}

	err = hwmsen_get_convert(obj->hw.direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw.direction);
		goto exit_kfree;
	}

	obj_i2c_data = obj;
	obj->client = client;
	/* obj->client->timing = 400; */

	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

#ifdef CONFIG_MPU6050_LOWPASS
	if (obj->hw.firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw.firlen);


	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);

#endif

	mpu6050_i2c_client = new_client;
	MPU6050_Dev_Reset(new_client);
	MPU6050_Reset(new_client);

	err = mpu6050_init_client(new_client, 1);
	if (err)
		goto exit_init_failed;


	/* err = misc_register(&mpu6050_device); */
	err = accel_factory_device_register(&mpu6050a_factory_device);
	if (err) {
		GSE_ERR("acc_factory register failed.\n");
		goto exit_misc_device_register_failed;
	}


	err = mpu6050_create_attr(
		&(mpu6050_init_info.platform_diver_addr->driver));
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = gsensor_open_report_data;
	ctl.enable_nodata = gsensor_enable_nodata;
	/* ctl.set_delay = gsensor_set_delay; */
	ctl.batch = gsensor_batch;
	ctl.flush = gsensor_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw.is_batch_supported;

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = gsensor_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err= %d\n", err);
		goto exit_create_attr_failed;
	}

#ifdef USE_EARLY_SUSPEND
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend = mpu6050_early_suspend,
	obj->early_drv.resume = mpu6050_late_resume,
	register_early_suspend(&obj->early_drv);
#endif

	gsensor_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__);
	return 0;

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
	mpu6050_i2c_client = NULL;
	obj_i2c_data = NULL;
	GSE_ERR("%s: err = %d\n", __func__, err);
	gsensor_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err =  mpu6050_delete_attr(
		&(mpu6050_init_info.platform_diver_addr->driver));
	if (err)
		GSE_ERR("mpu6050_delete_attr fail: %d\n", err);

	/* err = misc_deregister(&mpu6050_device); */
	accel_factory_device_deregister(&mpu6050a_factory_device);

	mpu6050_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/


static int  mpu6050_local_init(void)
{
	if (i2c_add_driver(&mpu6050g_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gsensor_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int mpu6050_remove(void)
{

	/*GSE_FUN();*/
	i2c_del_driver(&mpu6050g_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int __init mpu6050gse_init(void)
{

	acc_driver_add(&mpu6050_init_info);
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit mpu6050gse_exit(void)
{
	GSE_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(mpu6050gse_init);
module_exit(mpu6050gse_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6050 gse driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");

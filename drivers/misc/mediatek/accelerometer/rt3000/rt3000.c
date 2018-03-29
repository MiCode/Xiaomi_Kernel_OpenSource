/*(C) Copyright 2016
 * Richtek <www.richtek.com>
 *
 * RT3000 driver for MT6795
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
/* #include <linux/earlysuspend.h> */
#include <linux/platform_device.h>
#include <asm/atomic.h>

/* #include <mach/mt_typedefs.h> */
/* #include <mach/mt_gpio.h> */
/* #include <mach/mt_pm_ldo.h> */

/* #define POWER_NONE_MACRO MT65XX_POWER_NONE */

#include <cust_acc.h>
#include <hwmsensor.h>
/* #include <linux/hwmsensor.h> */
/* #include <linux/hwmsen_dev.h> */
/* #include <linux/sensors_io.h> */
#include "rt3000.h"
/* #include <linux/hwmsen_helper.h> */

#include <accel.h>
/* #include <linux/batch.h> */
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */


/*----------------------------------------------------------------------------*/
#define GSE_DEBUG
/*----------------------------------------------------------------------------*/

#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define RT3000_AXIS_X          0
#define RT3000_AXIS_Y          1
#define RT3000_AXIS_Z          2
#define RT3000_DATA_LEN        6
#define RT3000_DEV_NAME        "RT3000"
#define RT3000_DEV_DRIVER_VERSION              "1.0.0"
/*----------------------------------------------------------------------------*/

#define RT3000_REGS_LENGTH    (64)

/* Maintain  cust info here */
struct acc_hw accel_cust;
static struct acc_hw *hw = &accel_cust;


/*********/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id rt3000_i2c_id[] = { {RT3000_DEV_NAME, 0}, {} };

/* static struct i2c_board_info __initdata i2c_RT3000={ I2C_BOARD_INFO(RT3000_DEV_NAME, 0x19)}; */


/*----------------------------------------------------------------------------*/
static int rt3000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int rt3000_i2c_remove(struct i2c_client *client);
#if !defined(USE_EARLY_SUSPEND)
static int rt3000_suspend(struct i2c_client *client, pm_message_t msg);
static int rt3000_resume(struct i2c_client *client);
#endif

static int gsensor_local_init(void);
static int gsensor_remove(void);
#ifdef CUSTOM_KERNEL_SENSORHUB
static int gsensor_setup_irq(void);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
static int gsensor_set_delay(u64 ns);



/*----------------------------------------------------------------------------*/
typedef enum {
	RT_TRC_FILTER = 0x01,
	RT_TRC_RAWDATA = 0x02,
	RT_TRC_IOCTL = 0x04,
	RT_TRC_CALI = 0X08,
	RT_TRC_INFO = 0X10,
} RT_TRC;
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
	s16 raw[C_MAX_FIR_LENGTH][RT3000_AXES_NUM];
	int sum[RT3000_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct rt3000_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;
#ifdef CUSTOM_KERNEL_SENSORHUB
	struct work_struct irq_work;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	/*misc */
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[RT3000_AXES_NUM + 1];

	/*data */
	s8 offset[RT3000_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[RT3000_AXES_NUM + 1];

#ifdef CUSTOM_KERNEL_SENSORHUB
	int SCP_init_done;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

#if defined(CONFIG_RT3000_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
	/*early suspend */
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(USE_EARLY_SUSPEND)
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

static struct i2c_driver rt3000_i2c_driver = {
	.driver = {
		   .name = RT3000_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = accel_of_match,
#endif
		   },
	.probe = rt3000_i2c_probe,
	.remove = rt3000_i2c_remove,
#if !defined(USE_EARLY_SUSPEND)
	.suspend = rt3000_suspend,
	.resume = rt3000_resume,
#endif
	.id_table = rt3000_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *rt3000_i2c_client;
static struct rt3000_i2c_data *obj_i2c_data;
static bool sensor_power = true;
static int sensor_suspend;
static struct GSENSOR_VECTOR3D gsensor_gain;
/* static char selftestRes[8]= {0}; */
static DEFINE_MUTEX(gsensor_mutex);
static DEFINE_MUTEX(gsensor_scp_en_mutex);


static bool enable_status;

static int gsensor_init_flag = -1;	/* 0<==>OK -1 <==> fail */
static struct acc_init_info rt3000_init_info = {
	.name = RT3000_DEV_NAME,
	.init = gsensor_local_init,
	.uninit = gsensor_remove,
};

/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_ERR(fmt, args...)    pr_err(GSE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#ifdef GSE_DEBUG
#define GSE_FUN(f)               pr_debug(GSE_TAG"%s\n", __func__)
#define GSE_LOG(fmt, args...)    pr_debug(GSE_TAG fmt, ##args)
#else
#define GSE_FUN(f)
#define GSE_LOG(fmt, args...)
#endif

/*----------------------------------------------------------------------------*/
static struct data_resolution rt3000_data_resolution[] = {
	/* combination by {FULL_RES,RANGE} */
	{{0, 6}, 16384},	/*+/-2g  in 16-bit resolution:  0.06 mg/LSB */
	{{0, 12}, 8192},	/*+/-4g  in 16-bit resolution:  0.12 mg/LSB */
	{{0, 24}, 4096},	/*+/-8g  in 16-bit resolution:  0.24 mg/LSB */
	{{0, 5}, 2048},		/*+/-16g in 16-bit resolution:  0.49 mg/LSB */
};

static struct data_resolution rt3000_offset_resolution = { {0, 5}, 2048 };

/*----------------------------------------------------------------------------*/
static int rt_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr | 0x80;
	int err;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&gsensor_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&gsensor_mutex);
		return -EINVAL;
	}
#if (0)
	else if (len > C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&gsensor_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
#else
	if (len > 1)
		addr = addr | 0x80;
	err = i2c_smbus_read_i2c_block_data(client, addr, len, data);
#endif
	if (err < 0) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&gsensor_mutex);
	return err;

}
EXPORT_SYMBOL(rt_i2c_read_block);
static int rt_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{				/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err;

	err = 0;
	mutex_lock(&gsensor_mutex);
	if (!client) {
		mutex_unlock(&gsensor_mutex);
		return -EINVAL;
	}
#if (0)
	else if (len >= C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&gsensor_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = (len == 1) ? addr : (addr | 0x80);
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}
	err = i2c_master_send(client, buf, num);
#else
	if (len > 1)
		addr = addr | 0x80;
	err = i2c_smbus_write_i2c_block_data(client, addr, len, data);
#endif
	if (err < 0) {
		GSE_ERR("send command error!!\n");
		mutex_unlock(&gsensor_mutex);
		return -EFAULT;
	}
	mutex_unlock(&gsensor_mutex);
	return err;
}
EXPORT_SYMBOL(rt_i2c_write_block);

/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
int RT3000_SCP_SetPowerMode(bool enable, int sensorType)
{
	static bool gsensor_scp_en_status;
	static unsigned int gsensor_scp_en_map;
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;

	mutex_lock(&gsensor_scp_en_mutex);

	if (sensorType >= 32) {
		GSE_ERR("Out of index!\n");
		return -1;
	}

	if (true == enable) {
		gsensor_scp_en_map |= (1 << sensorType);
	} else {
		gsensor_scp_en_map &= ~(1 << sensorType);
	}

	if (0 == gsensor_scp_en_map)
		enable = false;
	else
		enable = true;

	if (gsensor_scp_en_status != enable) {
		gsensor_scp_en_status = enable;

		req.activate_req.sensorType = ID_ACCELEROMETER;
		req.activate_req.action = SENSOR_HUB_ACTIVATE;
		req.activate_req.enable = enable;
		len = sizeof(req.activate_req);
		err = SCP_sensorHub_req_send(&req, &len, 1);
		if (err) {
			GSE_ERR("SCP_sensorHub_req_send fail\n");
		}
	}

	mutex_unlock(&gsensor_scp_en_mutex);

	return err;
}
EXPORT_SYMBOL(RT3000_SCP_SetPowerMode);
#endif

/*--------------------RT3000 power control function----------------------------------*/
static void RT3000_power(struct acc_hw *hw, unsigned int on)
{
/*
#ifdef __USE_LINUX_REGULATOR_FRAMEWORK__
#else
#ifndef FPGA_EARLY_PORTING
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)		// have externel LDO
	{
		GSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			GSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "RT3000"))
			{
				GSE_ERR("power on fails!!\n");
			}
			mdelay(100);
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "RT3000"))
			{
				GSE_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
#endif //#ifndef FPGA_EARLY_PORTING
#endif //__USE_LINUX_REGULATOR_FRAMEWORK__
*/
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int RT3000_SetDataResolution(struct rt3000_i2c_data *obj)
{

/*set g sensor dataresolution here*/

/*RT3000 only can set to 10-bit dataresolution, so do nothing in rt3000 driver here*/

/*end of set dataresolution*/



	/*we set measure range from -2g to +2g in BMA150_SetDataFormat(client, BMA150_RANGE_2G),
	   and set 10-bit dataresolution BMA150_SetDataResolution() */

	/*so rt3000_data_resolution[0] set value as {{ 3, 9}, 256} when declaration, and assign the value to obj->reso here */
/*
	obj->reso = &rt3000_data_resolution[0];
	return 0;
*/
/*if you changed the measure range, for example call: RT3000_SetDataFormat(client, RANGE_4G),
you must set the right value to rt3000_data_resolution*/

	u8 dat, reso;

	if (rt_i2c_read_block(obj->client, RT3000_REG_ADDR_CTRL_REG4, &dat, 0x01)) {
		GSE_LOG("rt3000 read Dataformat failt\n");
		return RT3000_ERR_I2C;
	}
	mdelay(1);
	reso = 0x00;
	reso = (dat & RT3000_CTRL_REG4_RANGE_MASK) >> 4;

	if (reso < sizeof(rt3000_data_resolution) / sizeof(rt3000_data_resolution[0])) {
		obj->reso = &rt3000_data_resolution[reso];
		return 0;
	} else {
		return -EINVAL;
	}
}

/*----------------------------------------------------------------------------*/
static int RT3000_ReadData(struct i2c_client *client, s16 data[RT3000_AXES_NUM])
{
	struct rt3000_i2c_data *priv = i2c_get_clientdata(client);
	int err = 0;
#if 0				/* CUSTOM_KERNEL_SENSORHUB */
	SCP_SENSOR_HUB_DATA req;
	int len;
#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	u8 addr = RT3000_REG_ADDR_OUT_X_L;
	u8 buf[RT3000_DATA_LEN] = { 0 };
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

#if 0				/* CUSTOM_KERNEL_SENSORHUB */
	req.get_data_req.sensorType = ID_ACCELEROMETER;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		GSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}

	if (ID_ACCELEROMETER != req.get_data_rsp.sensorType ||
	    SENSOR_HUB_GET_DATA != req.get_data_rsp.action || 0 != req.get_data_rsp.errCode) {
		GSE_ERR("error : %d\n", req.get_data_rsp.errCode);
		return req.get_data_rsp.errCode;
	}

	len -= offsetof(SCP_SENSOR_HUB_GET_DATA_RSP, int8_Data);

	if (6 == len) {
		data[RT3000_AXIS_X] = req.get_data_rsp.int16_Data[0];
		data[RT3000_AXIS_Y] = req.get_data_rsp.int16_Data[1];
		data[RT3000_AXIS_Z] = req.get_data_rsp.int16_Data[2];
	} else {
		GSE_ERR("data length fail : %d\n", len);
	}

	if (atomic_read(&priv->trace) & RT_TRC_RAWDATA) {
		/* show data */
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (NULL == client) {
		err = -EINVAL;
	} else if ((err = rt_i2c_read_block(client, addr, buf, 0x06)) != 0) {
		GSE_ERR("error: %d\n", err);
	} else {
		data[RT3000_AXIS_X] = *(s16 *) (&buf[RT3000_AXIS_X * 2]);
		/*data[RT3000_AXIS_X] >>=6; */
		data[RT3000_AXIS_Y] = *(s16 *) (&buf[RT3000_AXIS_Y * 2]);
		/*data[RT3000_AXIS_Y] >>=6; */
		data[RT3000_AXIS_Z] = *(s16 *) (&buf[RT3000_AXIS_Z * 2]);
		/*data[RT3000_AXIS_Z] >>=6; */
		if (atomic_read(&priv->trace) & RT_TRC_RAWDATA) {
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d] before\n", data[RT3000_AXIS_X],
				data[RT3000_AXIS_Y], data[RT3000_AXIS_Z], data[RT3000_AXIS_X],
				data[RT3000_AXIS_Y], data[RT3000_AXIS_Z]);
		}

#ifdef CONFIG_RT3000_LOWPASS
		if (atomic_read(&priv->filter)) {
			if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
				int idx, firlen = atomic_read(&priv->firlen);

				if (priv->fir.num < firlen) {
					priv->fir.raw[priv->fir.num][RT3000_AXIS_X] =
					    data[RT3000_AXIS_X];
					priv->fir.raw[priv->fir.num][RT3000_AXIS_Y] =
					    data[RT3000_AXIS_Y];
					priv->fir.raw[priv->fir.num][RT3000_AXIS_Z] =
					    data[RT3000_AXIS_Z];
					priv->fir.sum[RT3000_AXIS_X] += data[RT3000_AXIS_X];
					priv->fir.sum[RT3000_AXIS_Y] += data[RT3000_AXIS_Y];
					priv->fir.sum[RT3000_AXIS_Z] += data[RT3000_AXIS_Z];
					if (atomic_read(&priv->trace) & RT_TRC_FILTER) {
						GSE_LOG
						    ("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n",
						     priv->fir.num,
						     priv->fir.raw[priv->fir.num][RT3000_AXIS_X],
						     priv->fir.raw[priv->fir.num][RT3000_AXIS_Y],
						     priv->fir.raw[priv->fir.num][RT3000_AXIS_Z],
						     priv->fir.sum[RT3000_AXIS_X],
						     priv->fir.sum[RT3000_AXIS_Y],
						     priv->fir.sum[RT3000_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				} else {
					idx = priv->fir.idx % firlen;
					priv->fir.sum[RT3000_AXIS_X] -=
					    priv->fir.raw[idx][RT3000_AXIS_X];
					priv->fir.sum[RT3000_AXIS_Y] -=
					    priv->fir.raw[idx][RT3000_AXIS_Y];
					priv->fir.sum[RT3000_AXIS_Z] -=
					    priv->fir.raw[idx][RT3000_AXIS_Z];
					priv->fir.raw[idx][RT3000_AXIS_X] = data[RT3000_AXIS_X];
					priv->fir.raw[idx][RT3000_AXIS_Y] = data[RT3000_AXIS_Y];
					priv->fir.raw[idx][RT3000_AXIS_Z] = data[RT3000_AXIS_Z];
					priv->fir.sum[RT3000_AXIS_X] += data[RT3000_AXIS_X];
					priv->fir.sum[RT3000_AXIS_Y] += data[RT3000_AXIS_Y];
					priv->fir.sum[RT3000_AXIS_Z] += data[RT3000_AXIS_Z];
					priv->fir.idx++;
					data[RT3000_AXIS_X] = priv->fir.sum[RT3000_AXIS_X] / firlen;
					data[RT3000_AXIS_Y] = priv->fir.sum[RT3000_AXIS_Y] / firlen;
					data[RT3000_AXIS_Z] = priv->fir.sum[RT3000_AXIS_Z] / firlen;
					if (atomic_read(&priv->trace) & RT_TRC_FILTER) {
						GSE_LOG
						    ("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n",
						     idx, priv->fir.raw[idx][RT3000_AXIS_X],
						     priv->fir.raw[idx][RT3000_AXIS_Y],
						     priv->fir.raw[idx][RT3000_AXIS_Z],
						     priv->fir.sum[RT3000_AXIS_X],
						     priv->fir.sum[RT3000_AXIS_Y],
						     priv->fir.sum[RT3000_AXIS_Z],
						     data[RT3000_AXIS_X], data[RT3000_AXIS_Y],
						     data[RT3000_AXIS_Z]);
					}
				}
			}
		}
#endif
	}

	return err;
}

/*----------------------------------------------------------------------------*/

static int RT3000_ReadOffset(struct i2c_client *client, s8 ofs[RT3000_AXES_NUM])
{
	int err;

	err = 0;

	ofs[0] = ofs[1] = ofs[2] = 0x0;

	return err;
}

/*----------------------------------------------------------------------------*/
static int RT3000_ResetCalibration(struct i2c_client *client)
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	RT3000_CUST_DATA *pCustData;
	unsigned int len;
#endif

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (0 != obj->SCP_init_done) {
		pCustData = (RT3000_CUST_DATA *) &data.set_cust_req.custData;

		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		pCustData->resetCali.action = RT3000_CUST_ACTION_RESET_CALI;
		len =
		    offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->resetCali);
		SCP_sensorHub_req_send(&data, &len, 1);
	}
#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;
}

/*----------------------------------------------------------------------------*/
static int RT3000_ReadCalibration(struct i2c_client *client, int dat[RT3000_AXES_NUM])
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int mul;

	GSE_FUN();
	mul = 0;		/* only SW Calibration, disable HW Calibration */


	dat[obj->cvt.map[RT3000_AXIS_X]] =
	    obj->cvt.sign[RT3000_AXIS_X] * (obj->offset[RT3000_AXIS_X] * mul +
					    obj->cali_sw[RT3000_AXIS_X]);
	dat[obj->cvt.map[RT3000_AXIS_Y]] =
	    obj->cvt.sign[RT3000_AXIS_Y] * (obj->offset[RT3000_AXIS_Y] * mul +
					    obj->cali_sw[RT3000_AXIS_Y]);
	dat[obj->cvt.map[RT3000_AXIS_Z]] =
	    obj->cvt.sign[RT3000_AXIS_Z] * (obj->offset[RT3000_AXIS_Z] * mul +
					    obj->cali_sw[RT3000_AXIS_Z]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int RT3000_ReadCalibrationEx(struct i2c_client *client, int act[RT3000_AXES_NUM],
				    int raw[RT3000_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data */
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;

	err = 0;

	mul = 0;		/* only SW Calibration, disable HW Calibration */

	raw[RT3000_AXIS_X] = obj->offset[RT3000_AXIS_X] * mul + obj->cali_sw[RT3000_AXIS_X];
	raw[RT3000_AXIS_Y] = obj->offset[RT3000_AXIS_Y] * mul + obj->cali_sw[RT3000_AXIS_Y];
	raw[RT3000_AXIS_Z] = obj->offset[RT3000_AXIS_Z] * mul + obj->cali_sw[RT3000_AXIS_Z];

	act[obj->cvt.map[RT3000_AXIS_X]] = obj->cvt.sign[RT3000_AXIS_X] * raw[RT3000_AXIS_X];
	act[obj->cvt.map[RT3000_AXIS_Y]] = obj->cvt.sign[RT3000_AXIS_Y] * raw[RT3000_AXIS_Y];
	act[obj->cvt.map[RT3000_AXIS_Z]] = obj->cvt.sign[RT3000_AXIS_Z] * raw[RT3000_AXIS_Z];

	return 0;
}

/*----------------------------------------------------------------------------*/
static int RT3000_WriteCalibration(struct i2c_client *client, int dat[RT3000_AXES_NUM])
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[RT3000_AXES_NUM], raw[RT3000_AXES_NUM];
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	RT3000_CUST_DATA *pCustData;
	unsigned int len;
#endif


	if (0 != (err = RT3000_ReadCalibrationEx(client, cali, raw))) {	/*offset will be updated in obj->offset */
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		raw[RT3000_AXIS_X], raw[RT3000_AXIS_Y], raw[RT3000_AXIS_Z],
		obj->offset[RT3000_AXIS_X], obj->offset[RT3000_AXIS_Y], obj->offset[RT3000_AXIS_Z],
		obj->cali_sw[RT3000_AXIS_X], obj->cali_sw[RT3000_AXIS_Y],
		obj->cali_sw[RT3000_AXIS_Z]);

#ifdef CUSTOM_KERNEL_SENSORHUB
	pCustData = (RT3000_CUST_DATA *) data.set_cust_req.custData;
	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData->setCali.action = RT3000_CUST_ACTION_SET_CALI;
	pCustData->setCali.data[RT3000_AXIS_X] = dat[RT3000_AXIS_X];
	pCustData->setCali.data[RT3000_AXIS_Y] = dat[RT3000_AXIS_Y];
	pCustData->setCali.data[RT3000_AXIS_Z] = dat[RT3000_AXIS_Z];
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);
	SCP_sensorHub_req_send(&data, &len, 1);
#endif

	/*calculate the real offset expected by caller */
	cali[RT3000_AXIS_X] += dat[RT3000_AXIS_X];
	cali[RT3000_AXIS_Y] += dat[RT3000_AXIS_Y];
	cali[RT3000_AXIS_Z] += dat[RT3000_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
		dat[RT3000_AXIS_X], dat[RT3000_AXIS_Y], dat[RT3000_AXIS_Z]);

	obj->cali_sw[RT3000_AXIS_X] =
	    obj->cvt.sign[RT3000_AXIS_X] * (cali[obj->cvt.map[RT3000_AXIS_X]]);
	obj->cali_sw[RT3000_AXIS_Y] =
	    obj->cvt.sign[RT3000_AXIS_Y] * (cali[obj->cvt.map[RT3000_AXIS_Y]]);
	obj->cali_sw[RT3000_AXIS_Z] =
	    obj->cvt.sign[RT3000_AXIS_Z] * (cali[obj->cvt.map[RT3000_AXIS_Z]]);

	mdelay(1);
	return err;
}

/*----------------------------------------------------------------------------*/
static int RT3000_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[2] = { 0 };
	int res = 0;


	res = rt_i2c_read_block(client, RT3000_REG_ADDR_WHO_AM_I, databuf, 0x1);
	if (res < 0) {
		goto exit_RT3000_CheckDeviceID;
	}


	GSE_LOG("RT3000_CheckDeviceID 0x%x done!\n ", databuf[0]);

exit_RT3000_CheckDeviceID:
	if (res < 0) {
		GSE_ERR("RT3000_CheckDeviceID 0x%x failt!\n ", RT3000_ERR_I2C);
		return RT3000_ERR_I2C;
	}
	mdelay(1);
	return RT3000_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int RT3000_SetPowerMode(struct i2c_client *client, bool enable)
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	u8 databuf[2];

	if (rt_i2c_read_block(client, RT3000_REG_ADDR_CTRL_REG1, databuf, 0x01)) {
		GSE_LOG("get power mode failed!\n");
		return RT3000_ERR_I2C;
	}
	mdelay(1);
	databuf[0] &= ~RT3000_CTRL_REG1_ODR_MASK;
	if (enable)
		databuf[0] |= RT3000_CTRL_REG1_ODR_100HZ;
	else
		databuf[0] |= RT3000_CTRL_REG1_ODR_POWER_DOWN;
	res = rt_i2c_write_block(client, RT3000_REG_ADDR_CTRL_REG1, databuf, 0x1);
	if (res < 0) {
		GSE_LOG("set power mode failed!\n");
		return RT3000_ERR_I2C;
	} else if (atomic_read(&obj->trace) & RT_TRC_INFO) {
		GSE_LOG("set power mode ok %d!\n", databuf[1]);
	}


	sensor_power = enable;
	mdelay(1);
	GSE_LOG("leave Sensor power status is sensor_power = %d\n", sensor_power);
	return RT3000_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int RT3000_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10] = { 0 };
	int res = 0;

	if (rt_i2c_read_block(client, RT3000_REG_ADDR_CTRL_REG4, databuf, 0x01)) {
		GSE_LOG("rt3000 read Dataformat failt\n");
		return RT3000_ERR_I2C;
	}
	mdelay(1);
	databuf[0] &= ~RT3000_CTRL_REG4_RANGE_MASK;
	databuf[0] |= dataformat;

	res = rt_i2c_write_block(client, RT3000_REG_ADDR_CTRL_REG4, databuf, 0x1);
	if (res < 0) {
		return RT3000_ERR_I2C;
	}

	GSE_LOG("RT3000_SetDataFormat OK!\n");
	mdelay(1);
	return RT3000_SetDataResolution(obj);
}

/*----------------------------------------------------------------------------*/
static int RT3000_SetODRRate(struct i2c_client *client, u8 odrrate)
{
	u8 databuf[10] = { 0 };
	int res = 0;

	if (rt_i2c_read_block(client, RT3000_REG_ADDR_CTRL_REG1, databuf, 0x01)) {
		GSE_LOG("rt3000 read rate failt\n");
		return RT3000_ERR_I2C;
	}
	mdelay(1);
	databuf[0] &= ~RT3000_CTRL_REG1_ODR_MASK;
	databuf[0] |= odrrate;


	res = rt_i2c_write_block(client, RT3000_REG_ADDR_CTRL_REG1, databuf, 0x1);
	if (res < 0) {
		return RT3000_ERR_I2C;
	}
	mdelay(1);
	GSE_LOG("RT3000_SetODRRate OK!\n");

	return RT3000_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int RT3000_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	int res = 0;

	if (intenable)
		res =
		    hwmsen_write_byte(client, RT3000_REG_ADDR_CTRL_REG3, RT3000_CTRL_REG3_AOI_INT);
	else
		res = hwmsen_write_byte(client, RT3000_REG_ADDR_CTRL_REG3, 0);
	if (res != RT3000_SUCCESS) {
		return res;
	}

	/*for disable interrupt function */
	mdelay(1);
	return RT3000_SUCCESS;
}

static int RT3000_Read_Regs(struct i2c_client *p_i2c_client, u8 *pbUserBuf)
{
	u8 _baData[RT3000_REGS_LENGTH] = { 0 };
	int _nIndex = 0;

	GSE_LOG("[%s]\n", __func__);

	if (NULL == p_i2c_client)
		return (-EINVAL);

	for (_nIndex = 0; _nIndex < RT3000_REGS_LENGTH; _nIndex++) {
		hwmsen_read_block(p_i2c_client, _nIndex, &_baData[_nIndex], 1);

		if (NULL != pbUserBuf)
			pbUserBuf[_nIndex] = _baData[_nIndex];

		GSE_LOG("REG[0x%02X] = 0x%02X\n", _nIndex, _baData[_nIndex]);
	}

	return (0);
}

/*----------------------------------------------------------------------------*/
static int rt3000_init_client(struct i2c_client *client, int reset_cali)
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_FUN();


	res = RT3000_CheckDeviceID(client);
	if (res != RT3000_SUCCESS) {
		return res;
	}
	GSE_LOG("RT3000_CheckDeviceID ok\n");

	res = RT3000_SetODRRate(client, RT3000_CTRL_REG1_ODR_100HZ);
	if (res != RT3000_SUCCESS) {
		return res;
	}
	GSE_LOG("RT3000_SetODRRate OK!\n");

	res = RT3000_SetDataFormat(client, RT3000_CTRL_REG4_RANGE_2G);
	if (res != RT3000_SUCCESS) {
		return res;
	}
	GSE_LOG("RT3000_SetDataFormat OK!\n");

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

#ifdef CUSTOM_KERNEL_SENSORHUB
	res = gsensor_setup_irq();
	if (res != RT3000_SUCCESS) {
		return res;
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	res = RT3000_SetIntEnable(client, 0x00);
	if (res != RT3000_SUCCESS) {
		return res;
	}
	GSE_LOG("RT3000 disable interrupt function!\n");

	res = RT3000_SetPowerMode(client, enable_status);
	if (res != RT3000_SUCCESS) {
		return res;
	}
	GSE_LOG("RT3000_SetPowerMode OK!\n");


	if (0 != reset_cali) {
		/*reset calibration only in power on */
		res = RT3000_ResetCalibration(client);
		if (res != RT3000_SUCCESS) {
			return res;
		}
	}
	GSE_LOG("rt3000_init_client OK!\n");
#ifdef CONFIG_RT3000_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif
	return RT3000_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int RT3000_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((NULL == buf) || (bufsize <= 30)) {
		return -1;
	}

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "RT3000 Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/

static int RT3000_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct rt3000_i2c_data *obj = (struct rt3000_i2c_data *)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[RT3000_AXES_NUM];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);

	if (NULL == buf) {
		return -1;
	}
	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	if (sensor_suspend == 1) {
		GSE_LOG("sensor in suspend read not data!\n");
		return 0;
	}

	if ((res = RT3000_ReadData(client, obj->data)) != 0) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	} else {
#if 0				/* CUSTOM_KERNEL_SENSORHUB */
		acc[RT3000_AXIS_X] = obj->data[RT3000_AXIS_X];
		acc[RT3000_AXIS_Y] = obj->data[RT3000_AXIS_Y];
		acc[RT3000_AXIS_Z] = obj->data[RT3000_AXIS_Z];
		/* data has been calibrated in SCP side. */
#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
		/*
		GSE_LOG("raw data x=%d, y=%d, z=%d\n", obj->data[RT3000_AXIS_X],
			obj->data[RT3000_AXIS_Y], obj->data[RT3000_AXIS_Z]);
		obj->data[RT3000_AXIS_X] += obj->cali_sw[RT3000_AXIS_X];
		obj->data[RT3000_AXIS_Y] += obj->cali_sw[RT3000_AXIS_Y];
		obj->data[RT3000_AXIS_Z] += obj->cali_sw[RT3000_AXIS_Z];

		GSE_LOG("cali_sw x=%d, y=%d, z=%d\n", obj->cali_sw[RT3000_AXIS_X],
			obj->cali_sw[RT3000_AXIS_Y], obj->cali_sw[RT3000_AXIS_Z]);
		*/
		/*remap coordinate */
		acc[obj->cvt.map[RT3000_AXIS_X]] =
		    obj->cvt.sign[RT3000_AXIS_X] * obj->data[RT3000_AXIS_X];
		acc[obj->cvt.map[RT3000_AXIS_Y]] =
		    obj->cvt.sign[RT3000_AXIS_Y] * obj->data[RT3000_AXIS_Y];
		acc[obj->cvt.map[RT3000_AXIS_Z]] =
		    obj->cvt.sign[RT3000_AXIS_Z] * obj->data[RT3000_AXIS_Z];
		/*
		GSE_LOG("cvt x=%d, y=%d, z=%d\n", obj->cvt.sign[RT3000_AXIS_X],
			obj->cvt.sign[RT3000_AXIS_Y], obj->cvt.sign[RT3000_AXIS_Z]);

		GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[RT3000_AXIS_X],
			acc[RT3000_AXIS_Y], acc[RT3000_AXIS_Z]);
		*/

		/* Out put the mg */
		/*
		GSE_LOG("mg acc=%d, GRAVITY=%d, sensityvity=%d\n", acc[RT3000_AXIS_X],
			GRAVITY_EARTH_1000, obj->reso->sensitivity);
		*/
		acc[RT3000_AXIS_X] =
		    acc[RT3000_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[RT3000_AXIS_Y] =
		    acc[RT3000_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[RT3000_AXIS_Z] =
		    acc[RT3000_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */


		sprintf(buf, "%04x %04x %04x", acc[RT3000_AXIS_X], acc[RT3000_AXIS_Y],
			acc[RT3000_AXIS_Z]);
		/*GSE_LOG("gsensor data: %s!\n", buf);*/
		if (atomic_read(&obj->trace) & RT_TRC_IOCTL) {
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int RT3000_ReadRawData(struct i2c_client *client, char *buf)
{
	struct rt3000_i2c_data *obj = (struct rt3000_i2c_data *)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client) {
		return EINVAL;
	}

	if (0 != (res = RT3000_ReadData(client, obj->data))) {
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	} else {
		sprintf(buf, "RT3000_ReadRawData %04x %04x %04x", obj->data[RT3000_AXIS_X],
			obj->data[RT3000_AXIS_Y], obj->data[RT3000_AXIS_Z]);

	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = rt3000_i2c_client;
	char strbuf[RT3000_BUFSIZE];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	RT3000_ReadChipInfo(client, strbuf, RT3000_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = rt3000_i2c_client;
	char strbuf[RT3000_BUFSIZE];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	RT3000_ReadSensorData(client, strbuf, RT3000_BUFSIZE);
	GSE_LOG("rt3000 show_sensordata:\n");
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
#if 1
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = rt3000_i2c_client;
	struct rt3000_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[RT3000_AXES_NUM];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);



	if (0 != (err = RT3000_ReadOffset(client, obj->offset))) {
		return -EINVAL;
	} else if (0 != (err = RT3000_ReadCalibration(client, tmp))) {
		return -EINVAL;
	} else {
		mul = obj->reso->sensitivity / rt3000_offset_resolution.sensitivity;
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,
			     obj->offset[RT3000_AXIS_X], obj->offset[RT3000_AXIS_Y],
			     obj->offset[RT3000_AXIS_Z], obj->offset[RT3000_AXIS_X],
			     obj->offset[RT3000_AXIS_Y], obj->offset[RT3000_AXIS_Z]);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
			     obj->cali_sw[RT3000_AXIS_X], obj->cali_sw[RT3000_AXIS_Y],
			     obj->cali_sw[RT3000_AXIS_Z]);

		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
			     obj->offset[RT3000_AXIS_X] * mul + obj->cali_sw[RT3000_AXIS_X],
			     obj->offset[RT3000_AXIS_Y] * mul + obj->cali_sw[RT3000_AXIS_Y],
			     obj->offset[RT3000_AXIS_Z] * mul + obj->cali_sw[RT3000_AXIS_Z],
			     tmp[RT3000_AXIS_X], tmp[RT3000_AXIS_Y], tmp[RT3000_AXIS_Z]);

		return len;
	}
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = rt3000_i2c_client;
	int err, x, y, z;
	int dat[RT3000_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		if (0 != (err = RT3000_ResetCalibration(client))) {
			GSE_ERR("reset offset err = %d\n", err);
		}
	} else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z)) {
		dat[RT3000_AXIS_X] = x;
		dat[RT3000_AXIS_Y] = y;
		dat[RT3000_AXIS_Z] = z;
		if (0 != (err = RT3000_WriteCalibration(client, dat))) {
			GSE_ERR("write calibration err = %d\n", err);
		}
	} else {
		GSE_ERR("invalid format\n");
	}

	return count;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_RT3000_LOWPASS
	struct i2c_client *client = rt3000_i2c_client;
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);

	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][RT3000_AXIS_X],
				obj->fir.raw[idx][RT3000_AXIS_Y], obj->fir.raw[idx][RT3000_AXIS_Z]);
		}

		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[RT3000_AXIS_X],
			obj->fir.sum[RT3000_AXIS_Y], obj->fir.sum[RT3000_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[RT3000_AXIS_X] / len,
			obj->fir.sum[RT3000_AXIS_Y] / len, obj->fir.sum[RT3000_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_RT3000_LOWPASS
	struct i2c_client *client = rt3000_i2c_client;
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int firlen;

	if (1 != sscanf(buf, "%d", &firlen)) {
		GSE_ERR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		GSE_ERR("exceeds maximum filter length\n");
	} else {
		atomic_set(&obj->firlen, firlen);
		if (NULL == firlen) {
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
	struct rt3000_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct rt3000_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {
		atomic_set(&obj->trace, trace);
	} else {
		GSE_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct rt3000_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id,
				obj->hw->power_vol);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{

	u8 databuf[2];
	u8 addr = RT3000_REG_ADDR_CTRL_REG1;
	struct rt3000_i2c_data *obj = obj_i2c_data;

	if (rt_i2c_read_block(obj->client, addr, databuf, 0x01)) {
		GSE_ERR("read power ctl register err!\n");
		return 1;
	}

	if (sensor_power)
		GSE_LOG("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		GSE_LOG("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return snprintf(buf, PAGE_SIZE, "%x\n", databuf[0]);
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *pbBuf)
{
	ssize_t _tLength = 0;
	/* struct acc_hw   *_ptAccelHw = get_cust_acc_hw(); */
	struct acc_hw *_ptAccelHw = hw;

	GSE_LOG("[%s] default direction: %d\n", __func__, _ptAccelHw->direction);

	_tLength = snprintf(pbBuf, PAGE_SIZE, "default direction = %d\n", _ptAccelHw->direction);

	return (_tLength);
}


static ssize_t store_chip_orientation(struct device_driver *ddri, const char *pbBuf, size_t tCount)
{
	int _nDirection = 0;
	struct rt3000_i2c_data *_pt_i2c_obj = obj_i2c_data;

	if (NULL == _pt_i2c_obj)
		return (0);

	if (1 == sscanf(pbBuf, "%d", &_nDirection)) {
		if (hwmsen_get_convert(_nDirection, &_pt_i2c_obj->cvt))
			GSE_ERR("ERR: fail to set direction\n");
	}

	GSE_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return (tCount);
}


static ssize_t show_register(struct device_driver *ddri, char *buf)
{
	u8 _bIndex = 0;
	u8 _baRegMap[64] = { 0 };
	ssize_t _tLength = 0;

	struct i2c_client *client = rt3000_i2c_client;



	mutex_lock(&gsensor_mutex);
	RT3000_Read_Regs(client, _baRegMap);
	mutex_unlock(&gsensor_mutex);

	for (_bIndex = 0; _bIndex < 64; _bIndex++)
		_tLength +=
		    snprintf((buf + _tLength), (PAGE_SIZE - _tLength), "Reg[0x%02X]: 0x%02X\n",
			     _bIndex, _baRegMap[_bIndex]);


	return (_tLength);
}

/*****************************************
 *** store_regiter_map
 *****************************************/
static ssize_t store_regiter_map(struct device_driver *ddri, const char *buf, size_t count)
{
	/* reserved */
	/* GSE_LOG("[%s] buf[0]: 0x%02X\n", __FUNCTION__, buf[0]); */

	return count;
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IWUSR | S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(firlen, S_IWUSR | S_IRUGO, show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powerstatus, S_IRUGO, show_power_status_value, NULL);
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);
static DRIVER_ATTR(regs, S_IWUSR | S_IRUGO, show_register, store_regiter_map);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *rt3000_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_cali,	/*show calibration data */
	&driver_attr_firlen,	/*filter length: 0: disable, others: enable */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
	&driver_attr_powerstatus,
	&driver_attr_orientation,
	&driver_attr_regs,

};

/*----------------------------------------------------------------------------*/
static int rt3000_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(rt3000_attr_list) / sizeof(rt3000_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		if (0 != (err = driver_create_file(driver, rt3000_attr_list[idx]))) {
			GSE_ERR("driver_create_file (%s) = %d\n", rt3000_attr_list[idx]->attr.name,
				err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int rt3000_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(rt3000_attr_list) / sizeof(rt3000_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}


	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, rt3000_attr_list[idx]);
	}


	return err;
}

/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static void gsensor_irq_work(struct work_struct *work)
{
	struct rt3000_i2c_data *obj = obj_i2c_data;
	struct scp_acc_hw scp_hw;
	RT3000_CUST_DATA *p_cust_data;
	SCP_SENSOR_HUB_DATA data;
	int max_cust_data_size_per_packet;
	int i;
	uint sizeOfCustData;
	uint len;
	char *p = (char *)&scp_hw;

	GSE_FUN();

	scp_hw.i2c_num = obj->hw->i2c_num;
	scp_hw.direction = obj->hw->direction;
	scp_hw.power_id = obj->hw->power_id;
	scp_hw.power_vol = obj->hw->power_vol;
	scp_hw.firlen = obj->hw->firlen;
	memcpy(scp_hw.i2c_addr, obj->hw->i2c_addr, sizeof(obj->hw->i2c_addr));
	scp_hw.power_vio_id = obj->hw->power_vio_id;
	scp_hw.power_vio_vol = obj->hw->power_vio_vol;
	scp_hw.is_batch_supported = obj->hw->is_batch_supported;

	p_cust_data = (RT3000_CUST_DATA *) data.set_cust_req.custData;
	sizeOfCustData = sizeof(scp_hw);
	max_cust_data_size_per_packet =
	    sizeof(data.set_cust_req.custData) - offsetof(RT3000_SET_CUST, data);

	GSE_ERR("sizeOfCustData = %d, max_cust_data_size_per_packet = %d\n", sizeOfCustData,
		max_cust_data_size_per_packet);
	GSE_ERR("offset %d\n", offsetof(RT3000_SET_CUST, data));

	for (i = 0; sizeOfCustData > 0; i++) {
		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		p_cust_data->setCust.action = RT3000_CUST_ACTION_SET_CUST;
		p_cust_data->setCust.part = i;
		if (sizeOfCustData > max_cust_data_size_per_packet) {
			len = max_cust_data_size_per_packet;
		} else {
			len = sizeOfCustData;
		}

		memcpy(p_cust_data->setCust.data, p, len);
		sizeOfCustData -= len;
		p += len;

		GSE_ERR("i= %d, sizeOfCustData = %d, len = %d\n", i, sizeOfCustData, len);
		len +=
		    offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(RT3000_SET_CUST,
									       data);
		GSE_ERR("data.set_cust_req.sensorType= %d\n", data.set_cust_req.sensorType);
		SCP_sensorHub_req_send(&data, &len, 1);

	}
	p_cust_data = (RT3000_CUST_DATA *) &data.set_cust_req.custData;
	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	p_cust_data->resetCali.action = RT3000_CUST_ACTION_RESET_CALI;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(p_cust_data->resetCali);
	SCP_sensorHub_req_send(&data, &len, 1);
	obj->SCP_init_done = 1;
}

/*----------------------------------------------------------------------------*/
static int gsensor_irq_handler(void *data, uint len)
{
	struct rt3000_i2c_data *obj = obj_i2c_data;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	GSE_ERR("gsensor_irq_handler len = %d, type = %d, action = %d, errCode = %d\n", len,
		rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);
	if (!obj) {
		return -1;
	}

	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->irq_work);
			GSE_ERR("OK sensor hub notify\n");
			break;
		default:
			GSE_ERR("Error sensor hub notify\n");
			break;
		}
		break;
	default:
		GSE_ERR("Error sensor hub action\n");
		break;
	}

	return 0;
}

static int gsensor_setup_irq(void)
{
	int err = 0;



	err = SCP_sensorHub_rsp_registration(ID_ACCELEROMETER, gsensor_irq_handler);

	return err;
}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
/******************************************************************************
 * Function Configuration
******************************************************************************/
static int rt3000_open(struct inode *inode, struct file *file)
{
	file->private_data = rt3000_i2c_client;

	if (file->private_data == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int rt3000_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
/* static int rt3000_ioctl(struct inode *inode, struct file *file, unsigned int cmd, */
/* unsigned long arg) */
static long rt3000_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct rt3000_i2c_data *obj = (struct rt3000_i2c_data *)i2c_get_clientdata(client);
	char strbuf[RT3000_BUFSIZE];
	void __user *data;
	struct SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3];

	/* GSE_FUN(f); */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		rt3000_init_client(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		RT3000_ReadChipInfo(client, strbuf, RT3000_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
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
		RT3000_SetPowerMode(client, true);
		RT3000_ReadSensorData(client, strbuf, RT3000_BUFSIZE);
		GSE_LOG("rt3000 IOCTL_READ_SENSORDATA:\n");
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
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

		if (copy_to_user(data, &gsensor_gain, sizeof(struct GSENSOR_VECTOR3D))) {
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
		RT3000_ReadRawData(client, strbuf);
		if (copy_to_user(data, &strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			GSE_ERR("Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
			cali[RT3000_AXIS_X] =
			    sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[RT3000_AXIS_Y] =
			    sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[RT3000_AXIS_Z] =
			    sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			err = RT3000_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = RT3000_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (0 != (err = RT3000_ReadCalibration(client, cali))) {
			break;
		}

		sensor_data.x = cali[RT3000_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.y = cali[RT3000_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.z = cali[RT3000_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
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


#ifdef CONFIG_COMPAT
static long rt3000_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_SENSORDATA,
					       (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_SET_CALI, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_SET_CALI unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_GET_CALI, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_GET_CALI unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_CLR_CALI, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_CLR_CALI unlocked_ioctl failed.");
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
/*----------------------------------------------------------------------------*/
static struct file_operations rt3000_fops = {
	.owner = THIS_MODULE,
	.open = rt3000_open,
	.release = rt3000_release,
	.unlocked_ioctl = rt3000_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = rt3000_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice rt3000_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &rt3000_fops,
};

/*----------------------------------------------------------------------------*/
#if !defined(USE_EARLY_SUSPEND)
/*----------------------------------------------------------------------------*/
static int rt3000_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	mutex_lock(&gsensor_scp_en_mutex);
	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GSE_ERR("null pointer!!\n");
			mutex_unlock(&gsensor_scp_en_mutex);
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
#ifdef CUSTOM_KERNEL_SENSORHUB
		if (0 != (err = RT3000_SCP_SetPowerMode(false, ID_ACCELEROMETER)))
#else
		if (0 != (err = RT3000_SetPowerMode(obj->client, false)))
#endif
		{
			GSE_ERR("write power control fail!!\n");
			mutex_unlock(&gsensor_scp_en_mutex);
			return -EINVAL;
		}
#ifndef CUSTOM_KERNEL_SENSORHUB
		RT3000_power(obj->hw, 0);
#endif
	}
	mutex_unlock(&gsensor_scp_en_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/
static int rt3000_resume(struct i2c_client *client)
{
	struct rt3000_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
#ifndef CUSTOM_KERNEL_SENSORHUB
	RT3000_power(obj->hw, 1);
#endif

#ifndef CUSTOM_KERNEL_SENSORHUB
	if (0 != (err = rt3000_init_client(client, 0)))
#else
	if (0 != (err = RT3000_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER)))
#endif
	{
		GSE_ERR("initialize client fail!!\n");

		return err;
	}
	atomic_set(&obj->suspend, 0);


	return 0;
}

/*----------------------------------------------------------------------------*/
#else				/*CONFIG_HAS_EARLY_SUSPEND is defined */
/*----------------------------------------------------------------------------*/
static void rt3000_early_suspend(struct early_suspend *h)
{
	struct rt3000_i2c_data *obj = container_of(h, struct rt3000_i2c_data, early_drv);
	int err;

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);

	GSE_FUN();
	u8 databuf[2];		/* for debug read power control register to see the value is OK */

	if (rt3000_i2c_read_block(obj->client, RT3000_REG_POWER_CTL, databuf, 0x01)) {
		GSE_ERR("read power ctl register err!\n");
		return RT3000_ERR_I2C;
	}
	if (databuf[0] == 0xff)	/* if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild */
		GSE_LOG("before RT3000_SetPowerMode in suspend databuf = 0x%x\n", databuf[0]);
#ifndef CUSTOM_KERNEL_SENSORHUB
	if ((err = RT3000_SetPowerMode(obj->client, false)))
#else
	if ((err = RT3000_SCP_SetPowerMode(false, ID_ACCELEROMETER)))
#endif
	{
		GSE_ERR("write power control fail!!\n");

		return;
	}
	if (rt3000_i2c_read_block(obj->client, RT3000_REG_POWER_CTL, databuf, 0x01))	/* for debug read power control register to see the value is OK */
	{
		GSE_ERR("read power ctl register err!\n");

		return RT3000_ERR_I2C;
	}
	if (databuf[0] == 0xff)	/* if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild */
		GSE_LOG("after RT3000_SetPowerMode suspend err databuf = 0x%x\n", databuf[0]);
	sensor_suspend = 1;

#ifndef CUSTOM_KERNEL_SENSORHUB
	RT3000_power(obj->hw, 0);
#endif

}

/*----------------------------------------------------------------------------*/
static void rt3000_late_resume(struct early_suspend *h)
{
	struct rt3000_i2c_data *obj = container_of(h, struct rt3000_i2c_data, early_drv);
	int err;

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}
#ifndef CUSTOM_KERNEL_SENSORHUB
	RT3000_power(obj->hw, 1);

#endif
	u8 databuf[2];		/* for debug read power control register to see the value is OK */

	if (rt3000_i2c_read_block(obj->client, RT3000_REG_POWER_CTL, databuf, 0x01)) {
		GSE_ERR("read power ctl register err!\n");

		return RT3000_ERR_I2C;

	}
	if (databuf[0] == 0xff)	/* if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild */

		GSE_LOG("before rt3000_init_client databuf = 0x%x\n", databuf[0]);
#ifndef CUSTOM_KERNEL_SENSORHUB
	if ((err = rt3000_init_client(obj->client, 0)))
#else
	if ((err = RT3000_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER)))
#endif
	{
		GSE_ERR("initialize client fail!!\n");

		return;
	}

	if (rt3000_i2c_read_block(obj->client, RT3000_REG_POWER_CTL, databuf, 0x01))	/* for debug read power control register to see the value is OK */
	{
		GSE_ERR("read power ctl register err!\n");

		return RT3000_ERR_I2C;
	}

	if (databuf[0] == 0xff)	/* if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild */
		GSE_LOG("after rt3000_init_client databuf = 0x%x\n", databuf[0]);
	sensor_suspend = 0;

	atomic_set(&obj->suspend, 0);
}

/*----------------------------------------------------------------------------*/
#endif				/*USE_EARLY_SUSPEND */
/*----------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int gsensor_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/*----------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
static int gsensor_enable_nodata(int en)
{
	int err = 0;



	if (((en == 0) && (sensor_power == false)) || ((en == 1) && (sensor_power == true))) {
		enable_status = sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		enable_status = !sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0) {
#ifdef CUSTOM_KERNEL_SENSORHUB
			err = RT3000_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER);
			if (0 == err) {
				sensor_power = enable_status;
			}
#else
			err = RT3000_SetPowerMode(obj_i2c_data->client, enable_status);
#endif
			GSE_LOG("Gsensor not in suspend RT3000_SetPowerMode!, enable_status = %d\n",
				enable_status);
		} else {
			GSE_LOG
			    ("Gsensor in suspend and can not enable or disable!enable_status = %d\n",
			     enable_status);
		}
	}

	if (err != RT3000_SUCCESS) {
		GSE_ERR("gsensor_enable_nodata fail!\n");
		return -1;
	}

	GSE_ERR("gsensor_enable_nodata OK!\n");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_set_delay(u64 ns)
{
	int err = 0;
	int value;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	int sample_delay;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	value = (int)ns / 1000 / 1000;

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.set_delay_req.sensorType = ID_ACCELEROMETER;
	req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
	req.set_delay_req.delay = value;
	len = sizeof(req.activate_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		GSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}
#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	if (value <= 5) {
		sample_delay = RT3000_CTRL_REG1_ODR_200HZ;
	} else if (value <= 10) {
		sample_delay = RT3000_CTRL_REG1_ODR_100HZ;
	} else {
		sample_delay = RT3000_CTRL_REG1_ODR_50HZ;
	}

	mutex_lock(&gsensor_scp_en_mutex);
	err = RT3000_SetODRRate(obj_i2c_data->client, sample_delay);
	mutex_unlock(&gsensor_scp_en_mutex);
	if (err != RT3000_SUCCESS) {
		GSE_ERR("Set delay parameter error!\n");
		return -1;
	}

	if (value >= 50) {
		atomic_set(&obj_i2c_data->filter, 0);
	} else {
#if defined(CONFIG_RT3000_LOWPASS)
		priv->fir.num = 0;
		priv->fir.idx = 0;
		priv->fir.sum[RT3000_AXIS_X] = 0;
		priv->fir.sum[RT3000_AXIS_Y] = 0;
		priv->fir.sum[RT3000_AXIS_Z] = 0;
		atomic_set(&priv->filter, 1);
#endif
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	GSE_LOG("gsensor_set_delay (%d)\n", value);

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int gsensor_get_data(int *x, int *y, int *z, int *status)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;
#else
	char buff[RT3000_BUFSIZE];
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	req.get_data_req.sensorType = ID_ACCELEROMETER;
	req.get_data_req.action = SENSOR_HUB_GET_DATA;
	len = sizeof(req.get_data_req);
	err = SCP_sensorHub_req_send(&req, &len, 1);
	if (err) {
		GSE_ERR("SCP_sensorHub_req_send!\n");
		return err;
	}

	if (ID_ACCELEROMETER != req.get_data_rsp.sensorType ||
	    SENSOR_HUB_GET_DATA != req.get_data_rsp.action || 0 != req.get_data_rsp.errCode) {
		GSE_ERR("error : %d\n", req.get_data_rsp.errCode);
		return req.get_data_rsp.errCode;
	}

	*x = (int)req.get_data_rsp.int16_Data[0] * GRAVITY_EARTH_1000 / 1000;
	*y = (int)req.get_data_rsp.int16_Data[1] * GRAVITY_EARTH_1000 / 1000;
	*z = (int)req.get_data_rsp.int16_Data[2] * GRAVITY_EARTH_1000 / 1000;
	GSE_ERR("x = %d, y = %d, z = %d\n", *x, *y, *z);

	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
#else
	mutex_lock(&gsensor_scp_en_mutex);
	RT3000_ReadSensorData(obj_i2c_data->client, buff, RT3000_BUFSIZE);
	mutex_unlock(&gsensor_scp_en_mutex);
	/*GSE_LOG("rt3000 get_data:\n");*/
	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int rt3000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct rt3000_i2c_data *obj;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };
	int err = 0;
	int retry = 0;

	GSE_FUN();

	if (!(obj = kzalloc(sizeof(*obj), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct rt3000_i2c_data));

	/* /obj->hw = get_cust_acc_hw(); */
	obj->hw = hw;

	if (0 != (err = hwmsen_get_convert(obj->hw->direction, &obj->cvt))) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}
#ifdef CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&obj->irq_work, gsensor_irq_work);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	obj_i2c_data = obj;
	obj->client = client;
#ifdef FPGA_EARLY_PORTING
	obj->client->timing = 100;
#else
	obj->client->timing = 400;
#endif
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

#ifdef CONFIG_RT3000_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH) {
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	} else {
		atomic_set(&obj->firlen, obj->hw->firlen);
	}

	if (atomic_read(&obj->firlen) > 0) {
		atomic_set(&obj->fir_en, 1);
	}
#endif

	rt3000_i2c_client = new_client;

	for (retry = 0; retry < 3; retry++) {
		if (0 != (err = rt3000_init_client(new_client, 1))) {
			GSE_ERR("rt3000_device init cilent fail time: %d\n", retry);
			continue;
		}
	}
	if (err != 0)
		goto exit_init_failed;


	if (0 != (err = misc_register(&rt3000_device))) {
		GSE_ERR("rt3000_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if (0 != (err = rt3000_create_attr(&rt3000_init_info.platform_diver_addr->driver))) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gsensor_open_report_data;
	ctl.enable_nodata = gsensor_enable_nodata;
	ctl.set_delay = gsensor_set_delay;
	/* ctl.batch = gsensor_set_batch; */
	ctl.is_report_input_direct = false;

#ifdef CUSTOM_KERNEL_SENSORHUB
	ctl.is_support_batch = obj->hw->is_batch_supported;
#else
	ctl.is_support_batch = false;
#endif

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = gsensor_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err\n");
		goto exit_kfree;
	}

	err = batch_register_support_info(ID_ACCELEROMETER, ctl.is_support_batch, 102, 0);	/* divisor is 1000/9.8 */
	if (err) {
		GSE_ERR("register gsensor batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}
#ifdef USE_EARLY_SUSPEND
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	    obj->early_drv.suspend = rt3000_early_suspend,
	    obj->early_drv.resume = rt3000_late_resume, register_early_suspend(&obj->early_drv);
#endif

	gsensor_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&rt3000_device);
exit_misc_device_register_failed:
exit_init_failed:
	/* i2c_detach_client(new_client); */
exit_kfree :
	kfree(obj);
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	gsensor_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int rt3000_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	if (0 != (err = rt3000_delete_attr(&rt3000_init_info.platform_diver_addr->driver))) {
		GSE_ERR("rt3000_delete_attr fail: %d\n", err);
	}

	if (0 != (err = misc_deregister(&rt3000_device))) {
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	rt3000_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_local_init(void)
{
	/* struct acc_hw *hw = get_cust_acc_hw(); */

	GSE_FUN();

	RT3000_power(hw, 1);

	if (i2c_add_driver(&rt3000_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gsensor_init_flag) {
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_remove(void)
{
	/* struct acc_hw *hw = get_cust_acc_hw(); */

	GSE_FUN();
	RT3000_power(hw, 0);
	i2c_del_driver(&rt3000_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init rt3000_init(void)
{
	const char *name = "mediatek,rt3000";

	hw = get_accel_dts_func(name, hw);
	if (!hw)
		GSE_ERR("get dts info fail\n");

	/* GSE_FUN(); */
	/* struct acc_hw *hw = get_cust_acc_hw(); */
	/* GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); */
	/* i2c_register_board_info(hw->i2c_num, &i2c_RT3000, 1); */

	acc_driver_add(&rt3000_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit rt3000_exit(void)
{
	GSE_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(rt3000_init);
module_exit(rt3000_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RT3000 G-Sensor Driver");
MODULE_AUTHOR("Richtek-inc");
MODULE_VERSION(RT3000_DEV_DRIVER_VERSION);

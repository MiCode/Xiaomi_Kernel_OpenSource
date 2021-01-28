/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * VERSION: V1.7
 * HISTORY:
 * V1.0 --- Driver creation
 * V1.1 --- Add share I2C address function
 * V1.2 --- Fix the bug that sometimes sensor is stuck after system resume.
 * V1.3 --- Add FIFO interfaces.
 * V1.4 --- Use basic i2c function to read fifo data instead of i2c DMA mode.
 * V1.5 --- Add compensated value performed by MTK acceleration calibration.
 * V1.6 --- Add walkaround for powermode change.
 * V1.7 --- Add walkaround for fifo.
 * V1.8 --- Add I2C retry in initializition.
 */
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <cust_acc.h>
#include <accel.h>
#include <sensors_io.h>

#include "bma2x2.h"
#define BMA255
/*------------------------------------------------------------------------*/
#define I2C_DRIVERID_BMA2x2 255
/*------------------------------------------------------------------------*/
#define DEBUG 1
/*------------------------------------------------------------------------*/
/*apply low pass filter on output*/
/*#define CONFIG_BMA2x2_LOWPASS*/
#define SW_CALIBRATION
#define CONFIG_I2C_BASIC_FUNCTION
static DECLARE_WAIT_QUEUE_HEAD(uplink_event_flag_wq);
static int CHIP_TYPE;

#define BMA2x2_GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)

#define BMA2x2_SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

/*----------------------------------------------------------------------------*/
#define GSE_TAG			"[Gsensor] "
#define GSE_FUN(f)		pr_info(GSE_TAG"%s\n", __func__)
#define GSE_ERR(fmt, args...)	pr_info(GSE_TAG"%s %d : "fmt,\
				__func__, __LINE__, ##args)
#define GSE_DBG(fmt, args...)	pr_debug(GSE_TAG fmt, ##args)
#define GSE_LOG(fmt, args...)	pr_info(GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id bma2x2_i2c_id[] = {
	{BMA2x2_DEV_NAME, 0},
	{}
};
/*----------------------------------------------------------------------------*/
static int bma2x2_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id);
static int bma2x2_i2c_remove(struct i2c_client *client);
static int BMA2x2_SetPowerMode(struct i2c_client *client, bool enable);
/*----------------------------------------------------------------------------*/
enum {
	BMA_TRC_FILTER	= 0x01,
	BMA_TRC_RAWDATA	= 0x02,
	BMA_TRC_IOCTL	= 0x04,
	BMA_TRC_CALI	= 0X08,
	BMA_TRC_INFO	= 0X10,
} BMA_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u16 whole;
	u16 fraction;
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
	s16 raw[C_MAX_FIR_LENGTH][BMA2x2_AXES_NUM];
	int sum[BMA2x2_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct bma2x2_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;

	/*misc*/
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t in_suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[BMA2x2_AXES_NUM + 1];
	struct mutex lock;

	/*data*/
	/*+1: for 4-byte alignment*/
	s8 offset[BMA2x2_AXES_NUM + 1];
	s16 data[BMA2x2_AXES_NUM + 1];
	u8 fifo_count;
	int IRQ;

#if defined(CONFIG_BMA2x2_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
};

/*----------------------------------------------------------------------------*/


#ifdef CONFIG_PM_SLEEP
static int bma2x2_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_i2c_data *client_data = i2c_get_clientdata(client);

	int err = 0;

	GSE_DBG("suspend function entrance");

	if (!client_data) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	atomic_set(&client_data->in_suspend, 1);

	err = BMA2x2_SetPowerMode(client, false);
	if (err < 0)
		GSE_ERR("BMA2x2_SetPowerMode failed\n");
	else
		GSE_ERR("BMA2x2_SetPowerMode succeed\n");

	return err;
}

static int bma2x2_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_i2c_data *client_data = i2c_get_clientdata(client);

	int err = 0;

	GSE_DBG("resume function entrance");

	if (!client_data) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	err = BMA2x2_SetPowerMode(client, true);
	if (err < 0)
		GSE_ERR("BMA2x2_SetPowerMode failed\n");
	else
		GSE_ERR("BMA2x2_SetPowerMode succeed\n");

	atomic_set(&client_data->in_suspend, 0);

	return err;
}

static const struct dev_pm_ops bma2x2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bma2x2_i2c_suspend, bma2x2_i2c_resume)
};
#endif


#ifdef CONFIG_OF
static const struct of_device_id gsensor_of_match[] = {
	{ .compatible = "mediatek,gsensor", },
	{},
};
#endif

static struct i2c_driver bma2x2_i2c_driver = {
	.driver = {
		.name = BMA2x2_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = gsensor_of_match,
#endif
#ifdef CONFIG_PM_SLEEP
		.pm = &bma2x2_pm_ops,
#endif
	},
	.probe	= bma2x2_i2c_probe,
	.remove	= bma2x2_i2c_remove,
	.id_table = bma2x2_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *bma2x2_i2c_client;
static struct acc_init_info bma2x2_init_info;
static struct bma2x2_i2c_data *obj_i2c_data;
static bool sensor_power = true;
static struct GSENSOR_VECTOR3D gsensor_gain;
static int bma2x2_init_flag = -1; /* 0<==>OK -1 <==> fail */

#define COMPATIABLE_NAME	"mediatek,gsensor"
#define BMA2X2_CAL_FILE		"/data/gsensor_cal_data.bin"
#define CALI_READ_DATA_DELAY_MS	20
#define CALI_READ_DATA_TIMES	20

struct acc_hw accel_cust;
static struct acc_hw *hw = &accel_cust;

/*----------------------------------------------------------------------------*/
static struct data_resolution bma2x2_2g_data_resolution[4] = {
	/*combination by {FULL_RES,RANGE}*/
	/*BMA222E +/-2g  in 8-bit;  { 15, 63} = 15.63;  64 = (2^8)/(2*2)*/
	{ { 15, 63 }, 64 },
	/*BMA250E +/-2g  in 10-bit;  { 3, 91} = 3.91;  256 = (2^10)/(2*2)*/
	{ { 3, 91 }, 256 },
	/*BMA255 +/-2g  in 12-bit;  { 0, 98} = 0.98;  1024 = (2^12)/(2*2)*/
	{{ 0, 98}, 1024},
	/*BMA280 +/-2g  in 14-bit;  { 0, 244} = 0.244;  4096 = (2^14)/(2*2)*/
	{ { 0, 488 }, 4096 },
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct data_resolution bma2x2_4g_data_resolution[4] = {
	/*combination by {FULL_RES,RANGE}*/
	/*BMA222E +/-4g  in 8-bit;  { 31, 25} = 31.25;  32 = (2^8)/(2*4)*/
	{ { 31, 25 }, 32 },
	/*BMA250E +/-4g  in 10-bit;  { 7, 81} = 7.81;  128 = (2^10)/(2*4)*/
	{ { 7, 81 }, 128 },
	/*BMA255 +/-4g  in 12-bit;  { 1, 95} = 1.95;  512 = (2^12)/(2*4)*/
	{ { 1, 95 }, 512 },
	/*BMA280 +/-4g  in 14-bit;  { 0, 488} = 0.488;  2048 = (2^14)/(2*4)*/
	{ { 0, 488 }, 2048 },
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct data_resolution bma2x2_8g_data_resolution[4] = {
	/*combination by {FULL_RES,RANGE}*/
	/*BMA222E +/-8g  in 8-bit;  { 62, 50} = 62.5;  16 = (2^8)/(2*8)*/
	{ { 62, 50 }, 16 },
	/*BMA250E +/-8g  in 10-bit;  { 15, 63} = 15.63;  64 = (2^10)/(2*8)*/
	{ { 15, 63 }, 64 },
	/*BMA255 +/-8g  in 12-bit;  { 3, 91} = 3.91;  256 = (2^12)/(2*8)*/
	{ { 3, 91 }, 256 },
	/*BMA280 +/-8g  in 14-bit;  { 0, 977} = 0.977;  1024 = (2^14)/(2*8)*/
	{ { 0, 977 }, 1024 },
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct data_resolution bma2x2_16g_data_resolution[4] = {
	/*combination by {FULL_RES,RANGE}*/
	/*BMA222E +/-16g  in 8-bit;  { 125, 0} = 125.0;  8 = (2^8)/(2*16)*/
	{ { 125, 0 }, 8 },
	/*BMA250E +/-16g  in 10-bit;  { 31, 25} = 31.25;  32 = (2^10)/(2*16)*/
	{ { 31, 25 }, 32 },
	/*BMA255 +/-16g  in 12-bit;  { 7, 81} = 7.81;  128 = (2^12)/(2*16)*/
	{ { 7, 81 }, 128 },
	/*BMA280 +/-16g  in 14-bit;  { 1, 953} = 1.953;  512 = (2^14)/(2*16)*/
	{ { 1, 953 }, 512 },
};
/*----------------------------------------------------------------------------*/

static struct data_resolution bma2x2_offset_resolution;

/* I2C operation functions */

static int bma_i2c_read_block(struct i2c_client *client,
					u8 addr, u8 *data, u8 len)
{
	u8 reg = addr;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = data,
		}
	};
	int err;

	if (!client)
		return -EINVAL;

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;/*no error*/
	}
	return err;
}

#define I2C_BUFFER_SIZE 256
static int bma_i2c_write_block(struct i2c_client *client, u8 addr,
			       u8 *data, u8 len)
{
	int err = 0, idx = 0, num = 0;
	char buf[32];

	if (!client)
		return -EINVAL;

	/*
	 * because address also occupies one byte,
	 * the maximum length for write is 7 bytes
	 */
	if (len > C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		GSE_ERR("send command error!!\n");
		return -EFAULT;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
static int BMA2x2_SetDataResolution(struct bma2x2_i2c_data *obj, u8 dataformat)
{
	if (CHIP_TYPE >= 0) {
		switch (dataformat) {
		case 0x03:
			/*2g range*/
			obj->reso = &bma2x2_2g_data_resolution[CHIP_TYPE];
			bma2x2_offset_resolution =
				bma2x2_2g_data_resolution[CHIP_TYPE];
			break;
		case 0x05:
			/*4g range*/
			obj->reso = &bma2x2_4g_data_resolution[CHIP_TYPE];
			bma2x2_offset_resolution =
				bma2x2_4g_data_resolution[CHIP_TYPE];
			break;
		case 0x08:
			/*8g range*/
			obj->reso = &bma2x2_8g_data_resolution[CHIP_TYPE];
			bma2x2_offset_resolution =
				bma2x2_8g_data_resolution[CHIP_TYPE];
			break;
		case 0x0C:
			/*16g range*/
			obj->reso = &bma2x2_16g_data_resolution[CHIP_TYPE];
			bma2x2_offset_resolution =
				bma2x2_16g_data_resolution[CHIP_TYPE];
			break;
		default:
			obj->reso = &bma2x2_2g_data_resolution[CHIP_TYPE];
			bma2x2_offset_resolution =
				bma2x2_2g_data_resolution[CHIP_TYPE];
		}

	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadData(struct i2c_client *client, s16 data[BMA2x2_AXES_NUM])
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	u8 addr = BMA2x2_REG_DATAXLOW;
	u8 buf[BMA2x2_DATA_LEN] = {0};
	int err = 0;

	if (!client) {
		err = -EINVAL;
		goto exit;
	}

	if (atomic_read(&obj->in_suspend)) {
		data[BMA2x2_AXIS_X] = 0;
		data[BMA2x2_AXIS_Y] = 0;
		data[BMA2x2_AXIS_Z] = 0;
		goto exit;
	}

#ifdef CONFIG_BMA2x2_LOWPASS
	int num = 0;
	int X = BMA2x2_AXIS_X;
	int Y = BMA2x2_AXIS_Y;
	int Z = BMA2x2_AXIS_Z;
#endif

	mutex_lock(&obj->lock);
	err = bma_i2c_read_block(client, addr, buf, BMA2x2_DATA_LEN);
	mutex_unlock(&obj->lock);
	if (err) {
		GSE_ERR("error: %d\n", err);
		goto exit;
	}

	if (CHIP_TYPE == BMA255_TYPE) {
		/* Convert sensor raw data to 16-bit integer */
		data[BMA2x2_AXIS_X] =
			BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA255_ACC_X_MSB) << BMA255_ACC_X_LSB__LEN);
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] <<
				(sizeof(short) * 8 - (BMA255_ACC_X_LSB__LEN
				+ BMA255_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] >>
				(sizeof(short) * 8 - (BMA255_ACC_X_LSB__LEN
				+ BMA255_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_Y] =
			BMA2x2_GET_BITSLICE(buf[2], BMA255_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[3],
				BMA255_ACC_Y_MSB) << BMA255_ACC_Y_LSB__LEN);
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] <<
				(sizeof(short) * 8 - (BMA255_ACC_Y_LSB__LEN
				+ BMA255_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] >>
				(sizeof(short) * 8 - (BMA255_ACC_Y_LSB__LEN
				+ BMA255_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Z] =
			BMA2x2_GET_BITSLICE(buf[4], BMA255_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[5],
				BMA255_ACC_Z_MSB) << BMA255_ACC_Z_LSB__LEN);
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] <<
				(sizeof(short) * 8 - (BMA255_ACC_Z_LSB__LEN
				+ BMA255_ACC_Z_MSB__LEN));
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] >>
				(sizeof(short) * 8 - (BMA255_ACC_Z_LSB__LEN
				+ BMA255_ACC_Z_MSB__LEN));
	} else if (CHIP_TYPE == BMA222E_TYPE) {
		/* Convert sensor raw data to 16-bit integer */
		data[BMA2x2_AXIS_X] =
			BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA222E_ACC_X_MSB) << BMA222E_ACC_X_LSB__LEN);
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] <<
				(sizeof(short) * 8 - (BMA222E_ACC_X_LSB__LEN
						+ BMA222E_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] >>
				(sizeof(short) * 8 - (BMA222E_ACC_X_LSB__LEN
						+ BMA222E_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_Y] =
			BMA2x2_GET_BITSLICE(buf[2], BMA222E_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[3],
				BMA222E_ACC_Y_MSB) << BMA222E_ACC_Y_LSB__LEN);
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] <<
				(sizeof(short) * 8 - (BMA222E_ACC_Y_LSB__LEN
					+ BMA222E_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] >>
				(sizeof(short) * 8 - (BMA222E_ACC_Y_LSB__LEN
				+ BMA222E_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Z] =
			BMA2x2_GET_BITSLICE(buf[4], BMA222E_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[5],
				BMA222E_ACC_Z_MSB) << BMA222E_ACC_Z_LSB__LEN);
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] <<
				(sizeof(short) * 8 - (BMA222E_ACC_Z_LSB__LEN
					+ BMA222E_ACC_Z_MSB__LEN));
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] >>
				(sizeof(short) * 8 - (BMA222E_ACC_Z_LSB__LEN
						+ BMA222E_ACC_Z_MSB__LEN));
	} else if (CHIP_TYPE == BMA250E_TYPE) {
		/* Convert sensor raw data to 16-bit integer */
		data[BMA2x2_AXIS_X] =
			BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA250E_ACC_X_MSB) << BMA250E_ACC_X_LSB__LEN);
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] <<
				(sizeof(short) * 8 - (BMA250E_ACC_X_LSB__LEN
					+ BMA250E_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] >>
				(sizeof(short) * 8 - (BMA250E_ACC_X_LSB__LEN
				+ BMA250E_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_Y] =
			BMA2x2_GET_BITSLICE(buf[2], BMA250E_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[3],
				BMA250E_ACC_Y_MSB) << BMA250E_ACC_Y_LSB__LEN);
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] <<
				(sizeof(short) * 8 - (BMA250E_ACC_Y_LSB__LEN
				+ BMA250E_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] >>
				(sizeof(short) * 8 - (BMA250E_ACC_Y_LSB__LEN
				+ BMA250E_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Z] =
			BMA2x2_GET_BITSLICE(buf[4], BMA250E_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[5],
				BMA250E_ACC_Z_MSB) << BMA250E_ACC_Z_LSB__LEN);
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] <<
				(sizeof(short) * 8 - (BMA250E_ACC_Z_LSB__LEN
				+ BMA250E_ACC_Z_MSB__LEN));
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] >>
				(sizeof(short) * 8 - (BMA250E_ACC_Z_LSB__LEN
				+ BMA250E_ACC_Z_MSB__LEN));
	} else if (CHIP_TYPE == BMA280_TYPE) {
		/* Convert sensor raw data to 16-bit integer */
		data[BMA2x2_AXIS_X] =
			BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA280_ACC_X_MSB) << BMA280_ACC_X_LSB__LEN);
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] <<
				(sizeof(short) * 8 - (BMA280_ACC_X_LSB__LEN
				+ BMA280_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_X] = data[BMA2x2_AXIS_X] >>
				(sizeof(short) * 8 - (BMA280_ACC_X_LSB__LEN
				+ BMA280_ACC_X_MSB__LEN));
		data[BMA2x2_AXIS_Y] =
			BMA2x2_GET_BITSLICE(buf[2], BMA280_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[3],
				BMA280_ACC_Y_MSB) << BMA280_ACC_Y_LSB__LEN);
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] <<
				(sizeof(short) * 8 - (BMA280_ACC_Y_LSB__LEN
				+ BMA280_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Y] = data[BMA2x2_AXIS_Y] >>
				(sizeof(short) * 8 - (BMA280_ACC_Y_LSB__LEN
				+ BMA280_ACC_Y_MSB__LEN));
		data[BMA2x2_AXIS_Z] =
			BMA2x2_GET_BITSLICE(buf[4], BMA280_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[5],
				BMA280_ACC_Z_MSB) << BMA280_ACC_Z_LSB__LEN);
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] <<
				(sizeof(short) * 8 - (BMA280_ACC_Z_LSB__LEN
				+ BMA280_ACC_Z_MSB__LEN));
		data[BMA2x2_AXIS_Z] = data[BMA2x2_AXIS_Z] >>
				(sizeof(short) * 8 - (BMA280_ACC_Z_LSB__LEN
				+ BMA280_ACC_Z_MSB__LEN));
	}

#ifdef CONFIG_BMA2x2_LOWPASS
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) &&
		    !atomic_read(&priv->in_suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			num = priv->fir.num;
			if (priv->fir.num < firlen) {
				priv->fir.raw[num][BMA2x2_AXIS_X]
					= data[BMA2x2_AXIS_X];
				priv->fir.raw[num][BMA2x2_AXIS_Y]
					= data[BMA2x2_AXIS_Y];
				priv->fir.raw[num][BMA2x2_AXIS_Z]
					= data[BMA2x2_AXIS_Z];
				priv->fir.sum[BMA2x2_AXIS_X] +=
					data[BMA2x2_AXIS_X];
				priv->fir.sum[BMA2x2_AXIS_Y] +=
					data[BMA2x2_AXIS_Y];
				priv->fir.sum[BMA2x2_AXIS_Z] +=
					data[BMA2x2_AXIS_Z];
				if (atomic_read(&priv->trace) &
				    BMA_TRC_FILTER) {
					GSE_DBG(
						"add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n",
						priv->fir.num,
						priv->fir.raw[num][X],
						priv->fir.raw[num][Y],
						priv->fir.raw[num][Z],
						priv->fir.sum[BMA2x2_AXIS_X],
						priv->fir.sum[BMA2x2_AXIS_Y],
						priv->fir.sum[BMA2x2_AXIS_Z]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[BMA2x2_AXIS_X] -=
					priv->fir.raw[idx][BMA2x2_AXIS_X];
				priv->fir.sum[BMA2x2_AXIS_Y] -=
					priv->fir.raw[idx][BMA2x2_AXIS_Y];
				priv->fir.sum[BMA2x2_AXIS_Z] -=
					priv->fir.raw[idx][BMA2x2_AXIS_Z];
				priv->fir.raw[idx][BMA2x2_AXIS_X]
					= data[BMA2x2_AXIS_X];
				priv->fir.raw[idx][BMA2x2_AXIS_Y]
					= data[BMA2x2_AXIS_Y];
				priv->fir.raw[idx][BMA2x2_AXIS_Z]
					= data[BMA2x2_AXIS_Z];
				priv->fir.sum[BMA2x2_AXIS_X] +=
					data[BMA2x2_AXIS_X];
				priv->fir.sum[BMA2x2_AXIS_Y] +=
					data[BMA2x2_AXIS_Y];
				priv->fir.sum[BMA2x2_AXIS_Z] +=
					data[BMA2x2_AXIS_Z];
				priv->fir.idx++;
				data[BMA2x2_AXIS_X] =
					priv->fir.sum[BMA2x2_AXIS_X] / firlen;
				data[BMA2x2_AXIS_Y] =
					priv->fir.sum[BMA2x2_AXIS_Y] / firlen;
				data[BMA2x2_AXIS_Z] =
					priv->fir.sum[BMA2x2_AXIS_Z] / firlen;
				if (atomic_read(&priv->trace) &
				    BMA_TRC_FILTER) {
					GSE_DBG(
						"[%2d][%5d %5d %5d][%5d %5d %5d]:[%5d %5d %5d]\n",
						idx,
						priv->fir.raw[idx][X],
						priv->fir.raw[idx][Y],
						priv->fir.raw[idx][Z],
						priv->fir.sum[BMA2x2_AXIS_X],
						priv->fir.sum[BMA2x2_AXIS_Y],
						priv->fir.sum[BMA2x2_AXIS_Z],
						data[BMA2x2_AXIS_X],
						data[BMA2x2_AXIS_Y],
						data[BMA2x2_AXIS_Z]);
				}
			}
		}
	}
#endif

exit:
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadOffset(struct i2c_client *client, s8 ofs[BMA2x2_AXES_NUM])
{
	int err = 0;
#ifndef SW_CALIBRATION
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
#endif

#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#else
	mutex_lock(&obj->lock);
	err = bma_i2c_read_block(client, BMA2x2_REG_OFSX, ofs, BMA2x2_AXES_NUM);
	mutex_unlock(&obj->lock);
	if (err)
		GSE_ERR("error: %d\n", err);

#endif
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ResetCalibration(struct i2c_client *client)
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

#ifdef SW_CALIBRATION
#else
	err = bma_i2c_write_block(client, BMA2x2_REG_OFSX, ofs, 4);
	if (err)
		GSE_ERR("error: %d\n", err);

#endif
	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadCalibration(struct i2c_client *client,
				  int dat[BMA2x2_AXES_NUM])
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int mul;

#ifdef SW_CALIBRATION
	/*only SW Calibration, disable HW Calibration*/
	mul = 0;
#else
	err = BMA2x2_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / bma2x2_offset_resolution.sensitivity;
#endif

	dat[obj->cvt.map[BMA2x2_AXIS_X]] =
		obj->cvt.sign[BMA2x2_AXIS_X] *
		(obj->offset[BMA2x2_AXIS_X] * mul +
		obj->cali_sw[BMA2x2_AXIS_X]);
	dat[obj->cvt.map[BMA2x2_AXIS_Y]] =
		obj->cvt.sign[BMA2x2_AXIS_Y] *
		(obj->offset[BMA2x2_AXIS_Y] * mul +
		obj->cali_sw[BMA2x2_AXIS_Y]);
	dat[obj->cvt.map[BMA2x2_AXIS_Z]] =
		obj->cvt.sign[BMA2x2_AXIS_Z] *
		(obj->offset[BMA2x2_AXIS_Z] * mul +
		obj->cali_sw[BMA2x2_AXIS_Z]);
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadCalibrationEx(struct i2c_client *client,
			int act[BMA2x2_AXES_NUM], int raw[BMA2x2_AXES_NUM])
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
	int err = 0;
#endif
	int mul;

#ifdef SW_CALIBRATION
	/*only SW Calibration, disable HW Calibration*/
	mul = 0;
#else
	err = BMA2x2_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity /
	      bma2x2_offset_resolution.sensitivity;
#endif

	raw[BMA2x2_AXIS_X] =
		obj->offset[BMA2x2_AXIS_X] * mul + obj->cali_sw[BMA2x2_AXIS_X];
	raw[BMA2x2_AXIS_Y] =
		obj->offset[BMA2x2_AXIS_Y] * mul + obj->cali_sw[BMA2x2_AXIS_Y];
	raw[BMA2x2_AXIS_Z] =
		obj->offset[BMA2x2_AXIS_Z] * mul + obj->cali_sw[BMA2x2_AXIS_Z];

	act[obj->cvt.map[BMA2x2_AXIS_X]] =
		obj->cvt.sign[BMA2x2_AXIS_X] * raw[BMA2x2_AXIS_X];
	act[obj->cvt.map[BMA2x2_AXIS_Y]] =
		obj->cvt.sign[BMA2x2_AXIS_Y] * raw[BMA2x2_AXIS_Y];
	act[obj->cvt.map[BMA2x2_AXIS_Z]] =
		obj->cvt.sign[BMA2x2_AXIS_Z] * raw[BMA2x2_AXIS_Z];
	return 0;
}
/*----------------------------------------------------------------------------*/
static bool gsensor_store_cali_in_file(const char *filename,
					int cali_x, int cali_y, int cali_z)
{
	struct file *cali_file;
	mm_segment_t fs;
	char data_buf[64] = {0};

	cali_file = filp_open(filename, O_CREAT | O_RDWR, 0777);
	if (IS_ERR(cali_file)) {
		GSE_ERR("%s open error! exit!\n", __func__);
		return false;
	}
	fs = get_fs();
	set_fs(get_ds());

	sprintf(data_buf, "%d,%d,%d", cali_x, cali_y, cali_z);
	GSE_LOG("%s sprintf(w_buf!\n", __func__);

	vfs_write(cali_file, data_buf, sizeof(data_buf), &cali_file->f_pos);

	set_fs(fs);

	filp_close(cali_file, NULL);
	GSE_LOG("pass\n");
	return 0;
}

static int BMA2x2_WriteCalibration(struct i2c_client *client,
				   int dat[BMA2x2_AXES_NUM])
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[BMA2x2_AXES_NUM], raw[BMA2x2_AXES_NUM];

	err = BMA2x2_ReadCalibrationEx(client, cali, raw);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_DBG("OLDOFF:(%+3d %+3d %+3d):(%+3d %+3d %+3d)/(%+3d %+3d %+3d)\n",
		raw[BMA2x2_AXIS_X], raw[BMA2x2_AXIS_Y], raw[BMA2x2_AXIS_Z],
		obj->offset[BMA2x2_AXIS_X],
		obj->offset[BMA2x2_AXIS_Y],
		obj->offset[BMA2x2_AXIS_Z],
		obj->cali_sw[BMA2x2_AXIS_X],
		obj->cali_sw[BMA2x2_AXIS_Y],
		obj->cali_sw[BMA2x2_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[BMA2x2_AXIS_X] += dat[BMA2x2_AXIS_X];
	cali[BMA2x2_AXIS_Y] += dat[BMA2x2_AXIS_Y];
	cali[BMA2x2_AXIS_Z] += dat[BMA2x2_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
		dat[BMA2x2_AXIS_X], dat[BMA2x2_AXIS_Y], dat[BMA2x2_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[BMA2x2_AXIS_X] = cali[BMA2x2_AXIS_X];
	obj->cali_sw[BMA2x2_AXIS_Y] = cali[BMA2x2_AXIS_Y];
	obj->cali_sw[BMA2x2_AXIS_Z] = cali[BMA2x2_AXIS_Z];
#else
	obj->offset[BMA2x2_AXIS_X] = (s8)(obj->cvt.sign[BMA2x2_AXIS_X] *
			(cali[obj->cvt.map[BMA2x2_AXIS_X]]) / (divisor));
	obj->offset[BMA2x2_AXIS_Y] = (s8)(obj->cvt.sign[BMA2x2_AXIS_Y] *
			(cali[obj->cvt.map[BMA2x2_AXIS_Y]]) / (divisor));
	obj->offset[BMA2x2_AXIS_Z] = (s8)(obj->cvt.sign[BMA2x2_AXIS_Z] *
			(cali[obj->cvt.map[BMA2x2_AXIS_Z]]) / (divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[BMA2x2_AXIS_X] = obj->cvt.sign[BMA2x2_AXIS_X] *
			(cali[obj->cvt.map[BMA2x2_AXIS_X]]) % (divisor);
	obj->cali_sw[BMA2x2_AXIS_Y] = obj->cvt.sign[BMA2x2_AXIS_Y] *
			(cali[obj->cvt.map[BMA2x2_AXIS_Y]]) % (divisor);
	obj->cali_sw[BMA2x2_AXIS_Z] = obj->cvt.sign[BMA2x2_AXIS_Z] *
			(cali[obj->cvt.map[BMA2x2_AXIS_Z]]) % (divisor);

	GSE_DBG("NEWOFF:(%+3d %+3d %+3d):(%+3d %+3d %+3d)/(%+3d %+3d %+3d)\n",
		obj->offset[BMA2x2_AXIS_X] *
		divisor + obj->cali_sw[BMA2x2_AXIS_X],
		obj->offset[BMA2x2_AXIS_Y] *
		divisor + obj->cali_sw[BMA2x2_AXIS_Y],
		obj->offset[BMA2x2_AXIS_Z] *
		divisor + obj->cali_sw[BMA2x2_AXIS_Z],
		obj->offset[BMA2x2_AXIS_X],
		obj->offset[BMA2x2_AXIS_Y],
		obj->offset[BMA2x2_AXIS_Z],
		obj->cali_sw[BMA2x2_AXIS_X],
		obj->cali_sw[BMA2x2_AXIS_Y],
		obj->cali_sw[BMA2x2_AXIS_Z]);
	err = bma_i2c_write_block(obj->client, BMA2x2_REG_OFSX,
						obj->offset, BMA2x2_AXES_NUM);
	if (err) {
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_CheckDeviceID(struct i2c_client *client)
{
	u8 data;
	int res = 0;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->lock);
	res = bma_i2c_read_block(client, BMA2x2_REG_DEVID, &data, 0x01);
	res = bma_i2c_read_block(client, BMA2x2_REG_DEVID, &data, 0x01);
	mutex_unlock(&obj->lock);
	if (res < 0)
		goto exit_BMA2x2_CheckDeviceID;

	switch (data) {
	case BMA222E_CHIPID:
		CHIP_TYPE = BMA222E_TYPE;
		GSE_LOG("%s %d pass!\n ", __func__, data);
		break;
	case BMA250E_CHIPID:
		CHIP_TYPE = BMA250E_TYPE;
		GSE_LOG("%s %d pass!\n ", __func__, data);
		break;
	case BMA255_CHIPID:
		CHIP_TYPE = BMA255_TYPE;
		GSE_LOG("%s %d pass!\n ", __func__, data);
		break;
	case BMA280_CHIPID:
		CHIP_TYPE = BMA280_TYPE;
		GSE_LOG("%s %d pass!\n ", __func__, data);
		break;
	default:
		CHIP_TYPE = UNKNOWN_TYPE;
		GSE_LOG("%s %d failt!\n ", __func__, data);
		break;
	}

exit_BMA2x2_CheckDeviceID:
	if (res < 0)
		return BMA2x2_ERR_I2C;

	return BMA2x2_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};
	int res = 0;
	u8 temp, temp0, temp1;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	u8 actual_power_mode = 0;
	int count = 0;
	static int num_record;

	if ((enable == sensor_power) && (num_record)) {
		GSE_LOG("Sensor power status is newest!\n");
		return BMA2x2_SUCCESS;
	}
	num_record = 0;
	num_record++;
	mutex_lock(&obj->lock);
	if (enable == true)
		actual_power_mode = BMA2x2_MODE_NORMAL;
	else
		actual_power_mode = BMA2x2_MODE_SUSPEND;

	res = bma_i2c_read_block(client, 0x3E, &temp, 0x1);
	count++;
	while ((count < 3) && (res < 0)) {
		res = bma_i2c_read_block(client, 0x3E, &temp, 0x1);
		msleep(20);
		count++;
	}
	if (res < 0)
		GSE_ERR("read  config failed!\n");
	switch (actual_power_mode) {
	case BMA2x2_MODE_NORMAL:
		databuf[0] = 0x00;
		databuf[1] = 0x00;
		while (count < 10) {
			res = bma_i2c_write_block(client,
				BMA2x2_MODE_CTRL_REG, &databuf[0], 1);
			if (res < 0)
				GSE_ERR("write MODE_CTRL_REG failed!\n");

			res = bma_i2c_write_block(client,
				BMA2x2_LOW_POWER_CTRL_REG, &databuf[1], 1);
			if (res < 0)
				GSE_ERR("write LOW_POWER_CTRL_REG failed!\n");

			res = bma_i2c_read_block(client,
					BMA2x2_MODE_CTRL_REG, &temp0, 0x1);
			if (res < 0)
				GSE_ERR("read MODE_CTRL_REG failed!\n");

			res = bma_i2c_read_block(client,
				BMA2x2_LOW_POWER_CTRL_REG, &temp1, 0x1);
			if (res < 0)
				GSE_ERR("read LOW_POWER_CTRL_REG failed!\n");

			if (temp0 != databuf[0]) {
				GSE_ERR("readback MODE_CTRL failed!\n");
				count++;
				continue;
			} else if (temp1 != databuf[1]) {
				GSE_ERR("readback LOW_POWER_CTRL failed!\n");
				count++;
				continue;
			} else {
				GSE_ERR("configure powermode success\n");
				break;
			}
		}
		break;
	case BMA2x2_MODE_SUSPEND:
		databuf[0] = 0x80;
		databuf[1] = 0x00;
		while (count < 10) {
			res = bma_i2c_write_block(client,
				BMA2x2_LOW_POWER_CTRL_REG, &databuf[1], 1);
			if (res < 0)
				GSE_ERR("write LOW_POWER_CTRL_REG failed!\n");

			res = bma_i2c_write_block(client,
					BMA2x2_MODE_CTRL_REG, &databuf[0], 1);
			if (res < 0)
				GSE_ERR("write BMA2x2_MODE_CTRL_REG failed!\n");

			res = bma_i2c_write_block(client, 0x3E, &temp, 0x1);
			if (res < 0)
				GSE_ERR("write  config failed!\n");

			res = bma_i2c_write_block(client, 0x3E, &temp, 0x1);
			if (res < 0)
				GSE_ERR("write  config failed!\n");

			res = bma_i2c_read_block(client,
					BMA2x2_MODE_CTRL_REG, &temp0, 0x1);
			if (res < 0)
				GSE_ERR("read BMA2x2_MODE_CTRL_REG failed!\n");

			res = bma_i2c_read_block(client,
					BMA2x2_LOW_POWER_CTRL_REG, &temp1, 0x1);
			if (res < 0)
				GSE_ERR("read BLOW_POWER_CTRL_REG failed!\n");

			if (temp0 != databuf[0]) {
				GSE_ERR("readback MODE_CTRL failed!\n");
				count++;
				continue;
			} else if (temp1 != databuf[1]) {
				GSE_ERR("readback LOW_POWER_CTRL failed!\n");
				count++;
				continue;
			} else {
				GSE_ERR("configure powermode success\n");
				break;
			}
		}
		break;
	}

	if (res < 0) {
		GSE_ERR("set power mode failed, res = %d\n", res);
		mutex_unlock(&obj->lock);
		return BMA2x2_ERR_I2C;
	}
	sensor_power = enable;
	mutex_unlock(&obj->lock);
	return BMA2x2_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = {0};
	int res = 0;

	mutex_lock(&obj->lock);
	res = bma_i2c_read_block(client, BMA2x2_RANGE_SEL_REG, &databuf[0], 1);
	databuf[0] = BMA2x2_SET_BITSLICE(databuf[0],
				BMA2x2_RANGE_SEL, dataformat);
	res += bma_i2c_write_block(client,
				   BMA2x2_RANGE_SEL_REG, &databuf[0], 1);
	if (res < 0) {
		GSE_ERR("set data format failed, res = %d\n", res);
		mutex_unlock(&obj->lock);
		return BMA2x2_ERR_I2C;
	}
	mutex_unlock(&obj->lock);
	return BMA2x2_SetDataResolution(obj, dataformat);
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = {0};
	int res = 0;

	mutex_lock(&obj->lock);
	res = bma_i2c_read_block(client, BMA2x2_BANDWIDTH__REG, &databuf[0], 1);
	databuf[0] = BMA2x2_SET_BITSLICE(databuf[0], BMA2x2_BANDWIDTH, bwrate);
	res += bma_i2c_write_block(client, BMA2x2_BANDWIDTH__REG,
			&databuf[0], 1);
	if (res < 0) {
		GSE_ERR("set bandwidth failed, res = %d\n", res);
		mutex_unlock(&obj->lock);
		return BMA2x2_ERR_I2C;
	}
	mutex_unlock(&obj->lock);

	return BMA2x2_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	int res = 0;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->lock);
	res = bma_i2c_write_block(client, BMA2x2_INT_REG_1, &intenable, 0x01);
	if (res < 0) {
		mutex_unlock(&obj->lock);
		return BMA2x2_ERR_I2C;
	}

	res = bma_i2c_write_block(client, BMA2x2_INT_REG_2, &intenable, 0x01);
	if (res < 0) {
		mutex_unlock(&obj->lock);
		return BMA2x2_ERR_I2C;
	}
	mutex_unlock(&obj->lock);
	GSE_LOG("BMA2X2 disable interrupt ...\n");
	return BMA2x2_SUCCESS;
}

static int BMA2x2_DisableTempSensor(struct i2c_client *client)
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	u8 temp = 0;

	mutex_lock(&obj->lock);
	temp = 0xCA;
	res += bma_i2c_write_block(client,
				   0x35, &temp, 1);
	res += bma_i2c_write_block(client,
				   0x35, &temp, 1);
	temp = 0x00;
	res += bma_i2c_write_block(client,
				   0x4F, &temp, 1);
	temp = 0x0A;
	res += bma_i2c_write_block(client,
				   0x35, &temp, 1);
	mutex_unlock(&obj->lock);
	return BMA2x2_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int bma2x2_set_fifo_mode(struct i2c_client *client,
				unsigned char fifo_mode)
{
	int comres = 0;
	unsigned char data = 0;
	struct bma2x2_i2c_data *obj =
		(struct bma2x2_i2c_data *)i2c_get_clientdata(client);

	if (client == NULL || fifo_mode >= 4)
		return BMA2x2_ERR_I2C;


	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_FIFO_MODE__REG, &data, 1);
	if (comres <= 0)
		GSE_ERR("read FIFOconfig0 failed ...\n");
	data = BMA2x2_SET_BITSLICE(data, BMA2x2_FIFO_MODE, fifo_mode);

	comres = bma_i2c_write_block(client, BMA2x2_FIFO_MODE__REG, &data, 1);
	mutex_unlock(&obj->lock);
	if (comres <= 0) {
		GSE_ERR("write FIFOconfig0 failed ...\n");
		return BMA2x2_ERR_I2C;
	} else
		return comres;

}
/*----------------------------------------------------------------------------*/
static int bma2x2_get_fifo_mode(struct i2c_client *client,
				unsigned char *fifo_mode)
{
	int comres = 0;
	unsigned char data;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	if (client == NULL)
		return BMA2x2_ERR_I2C;

	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_FIFO_MODE__REG, &data, 1);
	if (comres <= 0)
		GSE_ERR("read FIFOconfig0 failed ...\n");
	mutex_unlock(&obj->lock);
	*fifo_mode = BMA2x2_GET_BITSLICE(data, BMA2x2_FIFO_MODE);

	return comres;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_init_client(struct i2c_client *client, int reset_cali)
{
	/*try to write FIFO config again so as to clear the fifo data*/
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_LOG("%s\n", __func__);

	/*Check Device ID to avoid second source devcie*/
	res = BMA2x2_CheckDeviceID(client);
	if (res != BMA2x2_SUCCESS)
		return res;

	GSE_LOG("BMA2x2_CheckDeviceID ok\n");

	res = BMA2x2_SetBWRate(client, BMA2x2_BW_62_50HZ);
	if (res != BMA2x2_SUCCESS)
		return res;

	GSE_LOG("BMA2x2_SetBWRate OK!\n");

	res = BMA2x2_SetPowerMode(client, true);
	if (res != BMA2x2_SUCCESS)
		return res;

	GSE_LOG("BMA2x2_SetPowerMode to normal OK!\n");

	res = BMA2x2_DisableTempSensor(client);
	if (res != BMA2x2_SUCCESS)
		return res;

	GSE_LOG("temperature sensor disabled!\n");

	res = BMA2x2_SetDataFormat(client, BMA2x2_RANGE_4G);
	if (res != BMA2x2_SUCCESS)
		return res;

	GSE_LOG("BMA2x2_SetDataFormat OK!\n");

	gsensor_gain.x = obj->reso->sensitivity;
	gsensor_gain.y = obj->reso->sensitivity;
	gsensor_gain.z = obj->reso->sensitivity;

	res = BMA2x2_SetIntEnable(client, 0x00);
	if (res != BMA2x2_SUCCESS)
		return res;

	GSE_LOG("BMA2X2 disable interrupt function!\n");

	res = BMA2x2_SetPowerMode(client, false);
	if (res != BMA2x2_SUCCESS)
		return res;
	GSE_LOG("BMA2x2_SetPowerMode to suspend OK!\n");

	if (reset_cali != 0) {
		/*reset calibration only in power on*/
		res = BMA2x2_ResetCalibration(client);
		if (res != BMA2x2_SUCCESS)
			return res;
	}

	GSE_LOG("%s OK!\n", __func__);
#ifdef CONFIG_BMA2x2_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return BMA2x2_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadChipInfo(struct i2c_client *client,
			       char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30))
		return BMA2x2_ERR_I2C;

	if (client == NULL) {
		*buf = 0;
		return BMA2x2_ERR_CLIENT;
	}

	switch (CHIP_TYPE) {
	case BMA222E_TYPE:
		snprintf(buf, BMA2x2_BUFSIZE, "BMA222E Chip");
		break;
	case BMA250E_TYPE:
		snprintf(buf, BMA2x2_BUFSIZE, "BMA250E Chip");
		break;
	case BMA255_TYPE:
		snprintf(buf, BMA2x2_BUFSIZE, "BMA255 Chip");
		break;
	case BMA280_TYPE:
		snprintf(buf, BMA2x2_BUFSIZE, "BMA280 Chip");
		break;
	default:
		snprintf(buf, BMA2x2_BUFSIZE, "UNKNOWN Chip");
		break;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_CompassReadData(struct i2c_client *client,
				  char *buf, int bufsize)
{
	struct bma2x2_i2c_data *obj =
		(struct bma2x2_i2c_data *)i2c_get_clientdata(client);
	int acc[BMA2x2_AXES_NUM];
	int res = 0;
	s16 databuf[BMA2x2_AXES_NUM];

	if (buf == NULL)
		return BMA2x2_ERR_I2C;

	if (client == NULL) {
		*buf = 0;
		return BMA2x2_ERR_CLIENT;
	}

	if (sensor_power == false) {
		res = BMA2x2_SetPowerMode(client, true);
		if (res)
			GSE_ERR("Power on bma255 error %d!\n", res);

	}
	res = BMA2x2_ReadData(client, databuf);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return BMA2x2_ERR_STATUS;
	}

	/* Add compensated value performed by MTK calibration process*/
	databuf[BMA2x2_AXIS_X] += obj->cali_sw[BMA2x2_AXIS_X];
	databuf[BMA2x2_AXIS_Y] += obj->cali_sw[BMA2x2_AXIS_Y];
	databuf[BMA2x2_AXIS_Z] += obj->cali_sw[BMA2x2_AXIS_Z];

	/*remap coordinate*/
	acc[obj->cvt.map[BMA2x2_AXIS_X]] =
			obj->cvt.sign[BMA2x2_AXIS_X] * databuf[BMA2x2_AXIS_X];
	acc[obj->cvt.map[BMA2x2_AXIS_Y]] =
			obj->cvt.sign[BMA2x2_AXIS_Y] * databuf[BMA2x2_AXIS_Y];
	acc[obj->cvt.map[BMA2x2_AXIS_Z]] =
			obj->cvt.sign[BMA2x2_AXIS_Z] * databuf[BMA2x2_AXIS_Z];

	snprintf(buf, BMA2x2_BUFSIZE, "%d %d %d",
		(s16)acc[BMA2x2_AXIS_X],
		(s16)acc[BMA2x2_AXIS_Y],
		(s16)acc[BMA2x2_AXIS_Z]);

	if (atomic_read(&obj->trace) & BMA_TRC_IOCTL)
		GSE_DBG("gsensor data for compass: %s!\n", buf);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadSensorData(struct i2c_client *client,
				int *pdata_x, int *pdata_y, int *pdata_z)
{
	struct bma2x2_i2c_data *obj =
		(struct bma2x2_i2c_data *) i2c_get_clientdata(client);
	int acc[BMA2x2_AXES_NUM];
	int res = 0;

	s16 databuf[BMA2x2_AXES_NUM];

	if (!pdata_x || !pdata_y || !pdata_z)
		return BMA2x2_ERR_I2C;

	if (client == NULL)
		return BMA2x2_ERR_CLIENT;

	if (sensor_power == false) {
		res = BMA2x2_SetPowerMode(client, true);
		if (res)
			GSE_ERR("Power on bma255 error %d!\n", res);
	}
	res = BMA2x2_ReadData(client, databuf);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return BMA2x2_ERR_STATUS;
	}

	databuf[BMA2x2_AXIS_X] += obj->cali_sw[BMA2x2_AXIS_X];
	databuf[BMA2x2_AXIS_Y] += obj->cali_sw[BMA2x2_AXIS_Y];
	databuf[BMA2x2_AXIS_Z] += obj->cali_sw[BMA2x2_AXIS_Z];

	/*remap coordinate*/
	acc[obj->cvt.map[BMA2x2_AXIS_X]] =
		obj->cvt.sign[BMA2x2_AXIS_X] * databuf[BMA2x2_AXIS_X];
	acc[obj->cvt.map[BMA2x2_AXIS_Y]] =
		obj->cvt.sign[BMA2x2_AXIS_Y] * databuf[BMA2x2_AXIS_Y];
	acc[obj->cvt.map[BMA2x2_AXIS_Z]] =
		obj->cvt.sign[BMA2x2_AXIS_Z] * databuf[BMA2x2_AXIS_Z];

	GSE_DBG("cvt x=%d, y=%d, z=%d\n",
		obj->cvt.sign[BMA2x2_AXIS_X],
		obj->cvt.sign[BMA2x2_AXIS_Y],
		obj->cvt.sign[BMA2x2_AXIS_Z]);

	acc[BMA2x2_AXIS_X] = acc[BMA2x2_AXIS_X] * GRAVITY_EARTH_1000
					/ obj->reso->sensitivity;
	acc[BMA2x2_AXIS_Y] = acc[BMA2x2_AXIS_Y] * GRAVITY_EARTH_1000
					/ obj->reso->sensitivity;
	acc[BMA2x2_AXIS_Z] = acc[BMA2x2_AXIS_Z] * GRAVITY_EARTH_1000
					/ obj->reso->sensitivity;

	GSE_DBG("Mapped gsensor data: %d, %d, %d!\n",
			acc[BMA2x2_AXIS_X],
			acc[BMA2x2_AXIS_Y],
			acc[BMA2x2_AXIS_Z]);

	*pdata_x = acc[BMA2x2_AXIS_X];
	*pdata_y = acc[BMA2x2_AXIS_Y];
	*pdata_z = acc[BMA2x2_AXIS_Z];

	if (atomic_read(&obj->trace) & BMA_TRC_IOCTL)
		GSE_DBG("gsensor data: %04x %04x %04x!\n",
			*pdata_x, *pdata_y, *pdata_z);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA2x2_ReadRawData(struct i2c_client *client, char *buf)
{
	int res = 0;
	s16 databuf[BMA2x2_AXES_NUM];

	if (!buf || !client)
		return -EINVAL;

	res = BMA2x2_ReadData(client, databuf);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -EIO;
	}

	snprintf(buf, BMA2x2_BUFSIZE, "%s %04x %04x %04x",
			__func__,
			databuf[BMA2x2_AXIS_X],
			databuf[BMA2x2_AXIS_Y],
			databuf[BMA2x2_AXIS_Z]);

	return 0;
}
int bma2x2_softreset(void)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	unsigned char temp = BMA2x2_SOFTRESET;
	int ret = 0;

	if (!obj || !obj->client)
		return -EINVAL;

	mutex_lock(&obj->lock);
	ret = bma_i2c_write_block(obj->client, BMA2x2_SOFTRESET_REG, &temp, 1);
	mutex_unlock(&obj->lock);
	msleep(20);
	if (ret < 0)
		GSE_ERR("%s failed", __func__);
	return ret;

}
/*----------------------------------------------------------------------------*/
int bma2x2_selftest(unsigned char *selfresult, unsigned char *diff4x,
		    unsigned char *diff4y, unsigned char *diff4z)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	struct i2c_client *client = NULL;
	short value_n = 0, value_p = 0, diff_x = 0, diff_y = 0, diff_z = 0;
	short X_threshold = 0, Y_threshold = 0, Z_threshold = 0;
	unsigned char Ampl = 0;
	unsigned char temp = BMA2x2_SOFTRESET;
	u8 buf[2] = { 0 };
	int ret = 0;

	if (!obj || !obj->client)
		return -EINVAL;

	client = obj->client;
	mutex_lock(&obj->lock);
	ret = bma_i2c_write_block(client, BMA2x2_SOFTRESET_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_SOFTRESET_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	temp = 0;
	ret = bma_i2c_write_block(client, BMA2x2_MODE_CTRL_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_MODE_CTRL_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	temp = 0;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
#ifdef BMA250E
	temp = BMA2x2_DATA_FORMAT_RANGE_8G;
	X_threshold = BMA250E_SELF_X_THRESHOLD;
	Y_threshold = BMA250E_SELF_Y_THRESHOLD;
	Z_threshold = BMA250E_SELF_Z_THRESHOLD;
#elif defined BMA222E
	temp = BMA2x2_DATA_FORMAT_RANGE_8G;
	X_threshold = BMA222E_SELF_X_THRESHOLD;
	Y_threshold = BMA222E_SELF_Y_THRESHOLD;
	Z_threshold = BMA222E_SELF_Z_THRESHOLD;
#elif defined BMA280
	temp = BMA2x2_DATA_FORMAT_RANGE_4G;
	X_threshold = BMA280_SELF_X_THRESHOLD;
	Y_threshold = BMA280_SELF_Y_THRESHOLD;
	Z_threshold = BMA280_SELF_Z_THRESHOLD;
#elif defined BMA255
	temp = BMA2x2_DATA_FORMAT_RANGE_4G;
	X_threshold = BMA255_SELF_X_THRESHOLD;
	Y_threshold = BMA255_SELF_Y_THRESHOLD;
	Z_threshold = BMA255_SELF_Z_THRESHOLD;
#else
	temp = BMA2x2_DATA_FORMAT_RANGE_4G;
#endif
	ret = bma_i2c_write_block(client, BMA2x2_RANGE_SEL_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_RANGE_SEL_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	Ampl = 0x10;

	/*do the x axis self test*/
	temp = BMA2x2_SELF_TEST_X | BMA2x2_SELF_TEST_NEGATIVE | Ampl;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	ret = bma_i2c_read_block(client, BMA2x2_X_LSB_REG, buf, 2);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_X_LSB_REG N failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
#ifdef BMA250E
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA250E_ACC_X_MSB) << BMA250E_ACC_X_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA250E_ACC_X_LSB__LEN
				+ BMA250E_ACC_X_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA250E_ACC_X_LSB__LEN
				+ BMA250E_ACC_X_MSB__LEN));
#elif defined BMA222E
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA222E_ACC_X_MSB) << BMA222E_ACC_X_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA222E_ACC_X_LSB__LEN
				+ BMA222E_ACC_X_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA222E_ACC_X_LSB__LEN
				+ BMA222E_ACC_X_MSB__LEN));
#elif defined BMA280
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA280_ACC_X_MSB) << BMA280_ACC_X_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA280_ACC_X_LSB__LEN
				+ BMA280_ACC_X_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA280_ACC_X_LSB__LEN
				+ BMA280_ACC_X_MSB__LEN));
#elif defined BMA255
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_X_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA255_ACC_X_MSB) << BMA255_ACC_X_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA255_ACC_X_LSB__LEN
				+ BMA255_ACC_X_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA255_ACC_X_LSB__LEN
				+ BMA255_ACC_X_MSB__LEN));
#else
#endif
	temp = BMA2x2_SELF_TEST_X | BMA2x2_SELF_TEST_POSITIVE | Ampl;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	buf[0] = 0;
	buf[1] = 0;
	ret = bma_i2c_read_block(client, BMA2x2_X_LSB_REG, buf, 2);
	if (ret < 0) {
		GSE_ERR("writes BMA2x2_X_LSB_REG P failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
#ifdef BMA250E
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_X_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA250E_ACC_X_MSB) << BMA250E_ACC_X_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA250E_ACC_X_LSB__LEN
				+ BMA250E_ACC_X_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA250E_ACC_X_LSB__LEN
				+ BMA250E_ACC_X_MSB__LEN));
#elif defined BMA222E
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_X_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA222E_ACC_X_MSB) << BMA222E_ACC_X_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA222E_ACC_X_LSB__LEN
				+ BMA222E_ACC_X_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA222E_ACC_X_LSB__LEN
				+ BMA222E_ACC_X_MSB__LEN));
#elif defined BMA280
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_X_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA280_ACC_X_MSB) << BMA280_ACC_X_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA280_ACC_X_LSB__LEN
				+ BMA280_ACC_X_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA280_ACC_X_LSB__LEN
				+ BMA280_ACC_X_MSB__LEN));
#elif defined BMA255
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_X_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA255_ACC_X_MSB) << BMA255_ACC_X_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA255_ACC_X_LSB__LEN
				+ BMA255_ACC_X_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA255_ACC_X_LSB__LEN
				+ BMA255_ACC_X_MSB__LEN));
#else
#endif
	diff_x = value_n - value_p;

	/*do the y axis self test*/
	temp = BMA2x2_SELF_TEST_Y | BMA2x2_SELF_TEST_NEGATIVE | Ampl;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	buf[0] = 0;
	buf[1] = 0;
	ret = bma_i2c_read_block(client, BMA2x2_Y_LSB_REG, buf, 2);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_Y_LSB_REG N failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
#ifdef BMA250E
	value_n =
		BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_Y_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA250E_ACC_Y_MSB) << BMA250E_ACC_Y_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA250E_ACC_Y_LSB__LEN
				+ BMA250E_ACC_Y_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA250E_ACC_Y_LSB__LEN
				+ BMA250E_ACC_Y_MSB__LEN));
#elif defined BMA222E
	value_n =
		BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_Y_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA222E_ACC_Y_MSB) << BMA222E_ACC_Y_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA222E_ACC_Y_LSB__LEN
				+ BMA222E_ACC_Y_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA222E_ACC_Y_LSB__LEN
				+ BMA222E_ACC_Y_MSB__LEN));
#elif defined BMA280
	value_n =
		BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_Y_LSB)
		| (BMA2x2_GET_BITSLICE(buf[1],
			BMA280_ACC_Y_MSB) << BMA280_ACC_Y_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA280_ACC_Y_LSB__LEN
				+ BMA280_ACC_Y_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA280_ACC_Y_LSB__LEN
				+ BMA280_ACC_Y_MSB__LEN));
#elif defined BMA255
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA255_ACC_Y_MSB) << BMA255_ACC_Y_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA255_ACC_Y_LSB__LEN
				+ BMA255_ACC_Y_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA255_ACC_Y_LSB__LEN
				+ BMA255_ACC_Y_MSB__LEN));
#else
#endif
	temp = BMA2x2_SELF_TEST_Y | BMA2x2_SELF_TEST_POSITIVE | Ampl;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	buf[0] = 0;
	buf[1] = 0;
	ret = bma_i2c_read_block(client, BMA2x2_Y_LSB_REG, buf, 2);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_Y_LSB_REG P failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
#ifdef BMA250E
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA250E_ACC_Y_MSB) << BMA250E_ACC_Y_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA250E_ACC_Y_LSB__LEN
				+ BMA250E_ACC_Y_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA250E_ACC_Y_LSB__LEN
				+ BMA250E_ACC_Y_MSB__LEN));
#elif defined BMA222E
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA222E_ACC_Y_MSB) << BMA222E_ACC_Y_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA222E_ACC_Y_LSB__LEN
				+ BMA222E_ACC_Y_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA222E_ACC_Y_LSB__LEN
				+ BMA222E_ACC_Y_MSB__LEN));
#elif defined BMA280
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA280_ACC_Y_MSB) << BMA280_ACC_Y_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA280_ACC_Y_LSB__LEN
				+ BMA280_ACC_Y_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA280_ACC_Y_LSB__LEN
				+ BMA280_ACC_Y_MSB__LEN));
#elif defined BMA255
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_Y_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA255_ACC_Y_MSB) << BMA255_ACC_Y_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA255_ACC_Y_LSB__LEN
				+ BMA255_ACC_Y_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA255_ACC_Y_LSB__LEN
				+ BMA255_ACC_Y_MSB__LEN));
#else
#endif
	diff_y = value_n - value_p;

	/*do the z axis self test*/
	temp = BMA2x2_SELF_TEST_Z | BMA2x2_SELF_TEST_NEGATIVE | Ampl;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	buf[0] = 0;
	buf[1] = 0;
	ret = bma_i2c_read_block(client, BMA2x2_Z_LSB_REG, buf, 2);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_Z_LSB_REG N failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
#ifdef BMA250E
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA250E_ACC_Z_MSB) << BMA250E_ACC_Z_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA250E_ACC_Z_LSB__LEN
				+ BMA250E_ACC_Z_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA250E_ACC_Z_LSB__LEN
				+ BMA250E_ACC_Z_MSB__LEN));
#elif defined BMA222E
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA222E_ACC_Z_MSB) << BMA222E_ACC_Z_LSB__LEN);
	value_n = value_n <<
		  (sizeof(short) * 8 - (BMA222E_ACC_Z_LSB__LEN
					+ BMA222E_ACC_Z_MSB__LEN));
	value_n = value_n >>
		  (sizeof(short) * 8 - (BMA222E_ACC_Z_LSB__LEN
					+ BMA222E_ACC_Z_MSB__LEN));
#elif defined BMA280
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA280_ACC_Z_MSB) << BMA280_ACC_Z_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA280_ACC_Z_LSB__LEN
				+ BMA280_ACC_Z_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA280_ACC_Z_LSB__LEN
				+ BMA280_ACC_Z_MSB__LEN));
#elif defined BMA255
	value_n = BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA255_ACC_Z_MSB) << BMA255_ACC_Z_LSB__LEN);
	value_n = value_n << (sizeof(short) * 8 - (BMA255_ACC_Z_LSB__LEN
				+ BMA255_ACC_Z_MSB__LEN));
	value_n = value_n >> (sizeof(short) * 8 - (BMA255_ACC_Z_LSB__LEN
				+ BMA255_ACC_Z_MSB__LEN));
#else
#endif

	temp = BMA2x2_SELF_TEST_Z | BMA2x2_SELF_TEST_POSITIVE | Ampl;
	ret = bma_i2c_write_block(client, BMA2x2_PMU_SELF_TEST_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_PMU_SELF_TEST_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	buf[0] = 0;
	buf[1] = 0;
	ret = bma_i2c_read_block(client, BMA2x2_Z_LSB_REG, buf, 2);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_Z_LSB_REG P failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
#ifdef BMA250E
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA250E_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA250E_ACC_Z_MSB) << BMA250E_ACC_Z_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA250E_ACC_Z_LSB__LEN
				+ BMA250E_ACC_Z_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA250E_ACC_Z_LSB__LEN
				+ BMA250E_ACC_Z_MSB__LEN));
#elif defined BMA222E
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA222E_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA222E_ACC_Z_MSB) << BMA222E_ACC_Z_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA222E_ACC_Z_LSB__LEN
				+ BMA222E_ACC_Z_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA222E_ACC_Z_LSB__LEN
				+ BMA222E_ACC_Z_MSB__LEN));
#elif defined BMA280
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA280_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA280_ACC_Z_MSB) << BMA280_ACC_Z_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA280_ACC_Z_LSB__LEN
				+ BMA280_ACC_Z_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA280_ACC_Z_LSB__LEN
				+ BMA280_ACC_Z_MSB__LEN));
#elif defined BMA255
	value_p = BMA2x2_GET_BITSLICE(buf[0], BMA255_ACC_Z_LSB)
			| (BMA2x2_GET_BITSLICE(buf[1],
				BMA255_ACC_Z_MSB) << BMA255_ACC_Z_LSB__LEN);
	value_p = value_p << (sizeof(short) * 8 - (BMA255_ACC_Z_LSB__LEN
				+ BMA255_ACC_Z_MSB__LEN));
	value_p = value_p >> (sizeof(short) * 8 - (BMA255_ACC_Z_LSB__LEN
				+ BMA255_ACC_Z_MSB__LEN));
#else
#endif
	diff_z = value_n - value_p;

	(*selfresult) = 0;
	if (abs(diff_x) < X_threshold) {
		(*selfresult) |= 0x01;
		GSE_ERR("X selftest failed!!\n");
	} else
		GSE_ERR("X selftest succeeded!!\n");

	(*diff4x) = abs(diff_x);
	if (abs(diff_y) < Y_threshold) {
		(*selfresult) |= 0x02;
		GSE_ERR("Y selftest failed!!\n");
	} else
		GSE_ERR("Y selftest succeeded!!\n");

	(*diff4y) = abs(diff_y);
	if (abs(diff_z) < Z_threshold) {
		(*selfresult) |= 0x04;
		GSE_ERR("Z selftest failed!!\n");
	} else
		GSE_ERR("Z selftest succeeded!!\n");

	(*diff4z) = abs(diff_z);
	temp = BMA2x2_SOFTRESET;
	ret = bma_i2c_write_block(client, BMA2x2_SOFTRESET_REG, &temp, 1);
	if (ret < 0) {
		GSE_ERR("write BMA2x2_SOFTRESET_REG failed!!\n");
		mutex_unlock(&obj->lock);
		return ret;
	}
	msleep(20);
	mutex_unlock(&obj->lock);
	return ret;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	if (!client)
		return BMA2x2_ERR_I2C;

	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_EN_LOW_POWER__REG, mode, 1);
	mutex_unlock(&obj->lock);
	*mode  = (*mode) >> 6;

	return comres;
}

/*----------------------------------------------------------------------------*/
static int bma2x2_set_range(struct i2c_client *client, unsigned char range)
{
	int comres = 0;
	unsigned char data[2] = {BMA2x2_RANGE_SEL__REG};
	struct bma2x2_i2c_data *obj =
		(struct bma2x2_i2c_data *)i2c_get_clientdata(client);

	if (!client)
		return BMA2x2_ERR_I2C;

	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_RANGE_SEL__REG, data + 1, 1);
	data[1] = BMA2x2_SET_BITSLICE(data[1], BMA2x2_RANGE_SEL, range);
	comres = i2c_master_send(client, data, 2);
	mutex_unlock(&obj->lock);
	if (comres <= 0)
		return BMA2x2_ERR_I2C;

	BMA2x2_SetDataResolution(obj, range);

	return comres;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_get_range(struct i2c_client *client, unsigned char *range)
{
	int comres = 0;
	unsigned char data;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	if (!client)
		return BMA2x2_ERR_I2C;

	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_RANGE_SEL__REG, &data, 1);
	mutex_unlock(&obj->lock);
	*range = BMA2x2_GET_BITSLICE(data, BMA2x2_RANGE_SEL);

	return comres;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_set_bandwidth(struct i2c_client *client,
				unsigned char bandwidth)
{
	int comres = 0;
	unsigned char data[2] = {BMA2x2_BANDWIDTH__REG};
	struct bma2x2_i2c_data *obj =
		(struct bma2x2_i2c_data *)i2c_get_clientdata(client);

	if (!client)
		return BMA2x2_ERR_I2C;


	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client,
				    BMA2x2_BANDWIDTH__REG, data + 1, 1);

	data[1] = BMA2x2_SET_BITSLICE(data[1],
				      BMA2x2_BANDWIDTH, bandwidth);

	comres = i2c_master_send(client, data, 2);
	mutex_unlock(&obj->lock);
	if (comres <= 0)
		return BMA2x2_ERR_I2C;
	else
		return comres;

}
/*----------------------------------------------------------------------------*/
static int bma2x2_get_bandwidth(struct i2c_client *client,
				unsigned char *bandwidth)
{
	int comres = 0;
	unsigned char data;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	if (!client)
		return BMA2x2_ERR_I2C;

	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_BANDWIDTH__REG, &data, 1);
	mutex_unlock(&obj->lock);
	data = BMA2x2_GET_BITSLICE(data, BMA2x2_BANDWIDTH);

	if (data < 0x08) {
		/*7.81Hz*/
		*bandwidth = 0x08;
	} else if (data > 0x0f) {
		/* 1000Hz*/
		*bandwidth = 0x0f;
	} else {
		*bandwidth = data;
	}
	return comres;
}



static int bma2x2_get_fifo_framecount(struct i2c_client *client,
				      unsigned char *framecount)
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);
	int comres = 0;
	unsigned char data;

	if (!client)
		return BMA2x2_ERR_I2C;

	mutex_lock(&obj->lock);
	comres = bma_i2c_read_block(client, BMA2x2_FIFO_FRAME_COUNTER_S__REG,
				    &data, 1);
	mutex_unlock(&obj->lock);
	*framecount = BMA2x2_GET_BITSLICE(data, BMA2x2_FIFO_FRAME_COUNTER_S);

	return comres;
}
/*----------------------------------------------------------------------------*/
int gsensor_calibration(int period, int count,
			int *cali_x, int *cali_y, int *cali_z)
{
	struct i2c_client *client = bma2x2_i2c_client;

	int num = 0;

	int golden_x = 0;
	int golden_y = 0;
	int golden_z = 512;

	long avg_x = 0;
	long avg_y = 0;
	long avg_z = 0;

	int res = 0;

	s16 databuf[BMA2x2_AXES_NUM];

	if (!cali_x || !cali_y || !cali_z)
		return BMA2x2_ERR_I2C;

	if (client == NULL)
		return BMA2x2_ERR_CLIENT;

	if (sensor_power == false) {
		res = BMA2x2_SetPowerMode(client, true);
		if (res)
			GSE_ERR("Power on bma255 error %d!\n", res);
	}
	res = BMA2x2_ReadData(client, databuf);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return BMA2x2_ERR_STATUS;
	}

	GSE_LOG("BMA2x2_ReadData gsensor data: %d, %d, %d!\n",
			databuf[BMA2x2_AXIS_X],
			databuf[BMA2x2_AXIS_Y],
			databuf[BMA2x2_AXIS_Z]);

	while (num < count) {
		res = BMA2x2_ReadData(client, databuf);
		if (res) {
			GSE_ERR("I2C error: ret value=%d", res);
			return BMA2x2_ERR_STATUS;
		}
		GSE_LOG("read gsensor data 20 times x: %d y: %d z: %d\n",
			databuf[BMA2x2_AXIS_X],
			databuf[BMA2x2_AXIS_Y],
			databuf[BMA2x2_AXIS_Z]);
		avg_x += databuf[BMA2x2_AXIS_X];
		avg_y += databuf[BMA2x2_AXIS_Y];
		avg_z += databuf[BMA2x2_AXIS_Z];

		GSE_LOG("%s tatal++: %ld, %ld, %ld\n", __func__,
			avg_x, avg_y, avg_z);

		num++;
		msleep(period);
	}
	GSE_LOG("%s tatal: %ld, %ld, %ld\n", __func__, avg_x, avg_y, avg_z);

	avg_x /= count;
	avg_y /= count;
	avg_z /= count;
	GSE_LOG("%s average: %ld, %ld, %ld\n", __func__, avg_x, avg_y, avg_z);

	*cali_x = golden_x - avg_x;
	*cali_y = golden_y - avg_y;
	if (avg_z > 0)
		*cali_z = golden_z - avg_z;
	else
		*cali_z = -(avg_z + golden_z);

	GSE_LOG("%s %d, %d, %d\n", __func__, *cali_x, *cali_y, *cali_z);

	return res;
}

/*----------------------------------------------------------------------------*/

static ssize_t chipinfo_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	char strbuf[BMA2x2_BUFSIZE];

	if (client == NULL) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	BMA2x2_ReadChipInfo(client, strbuf, BMA2x2_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}


/*----------------------------------------------------------------------------*/
/* g sensor opmode for compass tilt compensation */
static ssize_t cpsopmode_show(struct device_driver *ddri, char *buf)
{
	unsigned char data;

	if (bma2x2_get_mode(bma2x2_i2c_client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

/*----------------------------------------------------------------------------*/
/* g sensor opmode for compass tilt compensation */
static ssize_t cpsopmode_store(struct device_driver *ddri,
				     const char *buf, size_t count)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (data == BMA2x2_MODE_NORMAL)
		BMA2x2_SetPowerMode(client, true);
	else if (data == BMA2x2_MODE_SUSPEND)
		BMA2x2_SetPowerMode(client, false);
	else
		GSE_ERR("invalid content: '%s'\n", buf);

	return count;
}

/*----------------------------------------------------------------------------*/
/* g sensor range for compass tilt compensation */
static ssize_t cpsrange_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned char data;

	if (bma2x2_get_range(client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", data);
}

/*----------------------------------------------------------------------------*/
/* g sensor range for compass tilt compensation */
static ssize_t cpsrange_store(struct device_driver *ddri,
				    const char *buf, size_t count)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_range(client, (unsigned char) data) < 0)
		GSE_ERR("invalid content: '%s'\n", buf);


	return count;
}
/*----------------------------------------------------------------------------*/
/* g sensor bandwidth for compass tilt compensation */
static ssize_t cpsbandwidth_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned char data;

	if (bma2x2_get_bandwidth(client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", data);

}

/*----------------------------------------------------------------------------*/
/* g sensor bandwidth for compass tilt compensation */
static ssize_t cpsbandwidth_store(struct device_driver *ddri,
					const char *buf, size_t count)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);

	if (error)
		return error;

	if (bma2x2_set_bandwidth(client, (unsigned char) data) < 0)
		GSE_ERR("invalid content: '%s'\n", buf);

	return count;
}

/*----------------------------------------------------------------------------*/
/* g sensor data for compass tilt compensation */
static ssize_t cpsdata_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	char strbuf[BMA2x2_BUFSIZE];

	if (!client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	BMA2x2_CompassReadData(client, strbuf, BMA2x2_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t sensordata_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err = BMA2x2_SUCCESS;
	int data_x = 0;
	int data_y = 0;
	int data_z = 0;

	if (!client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	err = BMA2x2_ReadSensorData(client, &data_x, &data_y, &data_z);
	if (err != BMA2x2_SUCCESS) {
		GSE_ERR("show_sensordata_value() read sensor data error!\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%04x %04x %04x\n",
			data_x, data_y, data_z);
}

/*----------------------------------------------------------------------------*/
static ssize_t cali_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	struct bma2x2_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[BMA2x2_AXES_NUM];

	if (!client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	err = BMA2x2_ReadOffset(client, obj->offset);
	if (err)
		return -EINVAL;

	err = BMA2x2_ReadCalibration(client, tmp);
	if (err)
		return -EINVAL;

	mul = obj->reso->sensitivity
	      / bma2x2_offset_resolution.sensitivity;
	len += snprintf(buf + len, PAGE_SIZE - len,
		"[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n",
		mul,
		obj->offset[BMA2x2_AXIS_X],
		obj->offset[BMA2x2_AXIS_Y],
		obj->offset[BMA2x2_AXIS_Z],
		obj->offset[BMA2x2_AXIS_X],
		obj->offset[BMA2x2_AXIS_Y],
		obj->offset[BMA2x2_AXIS_Z]);
	len += snprintf(buf + len, PAGE_SIZE - len,
		"[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
		obj->cali_sw[BMA2x2_AXIS_X],
		obj->cali_sw[BMA2x2_AXIS_Y],
		obj->cali_sw[BMA2x2_AXIS_Z]);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"[ALL] (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
		obj->offset[BMA2x2_AXIS_X] * mul + obj->cali_sw[BMA2x2_AXIS_X],
		obj->offset[BMA2x2_AXIS_Y] * mul + obj->cali_sw[BMA2x2_AXIS_Y],
		obj->offset[BMA2x2_AXIS_Z] * mul + obj->cali_sw[BMA2x2_AXIS_Z],
		tmp[BMA2x2_AXIS_X], tmp[BMA2x2_AXIS_Y], tmp[BMA2x2_AXIS_Z]);

	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t cali_store(struct device_driver *ddri,
				const char *buf, size_t count)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err, x, y, z;
	int dat[BMA2x2_AXES_NUM];
	int cali_x = 0;
	int cali_y = 0;
	int cali_z = 0;
	int period;
	int times;

	period = CALI_READ_DATA_DELAY_MS;
	times = CALI_READ_DATA_TIMES;

	if (!client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	if (!strncmp(buf, "rst", 3)) {
		err = BMA2x2_ResetCalibration(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);

	} else if (!strncmp(buf, "1", 1)) {
		GSE_LOG("Start Gsensor Calibration\n");

		err = BMA2x2_ResetCalibration(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);

		err = gsensor_calibration(period, times,
					&cali_x, &cali_y, &cali_z);
		if (err != 0)
			GSE_ERR("calibrate acc: %d\n", err);

		dat[BMA2x2_AXIS_X] = cali_x;
		dat[BMA2x2_AXIS_Y] = cali_y;
		dat[BMA2x2_AXIS_Z] = cali_z;

		err = BMA2x2_WriteCalibration(client, dat);
		if (err)
			GSE_ERR("write calibration err = %d\n", err);

		err = gsensor_store_cali_in_file(BMA2X2_CAL_FILE,
			dat[BMA2x2_AXIS_X],
			dat[BMA2x2_AXIS_Y],
			dat[BMA2x2_AXIS_Z]);
		if (err)
			GSE_ERR("write calibration to file err = %d\n", err);

	} else if (sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z) == 3) {
		dat[BMA2x2_AXIS_X] = x;
		dat[BMA2x2_AXIS_Y] = y;
		dat[BMA2x2_AXIS_Z] = z;
		err = BMA2x2_WriteCalibration(client, dat);
		if (err)
			GSE_ERR("write calibration err = %d\n", err);

	} else
		GSE_ERR("invalid format\n");

	return count;
}


/*----------------------------------------------------------------------------*/
static ssize_t firlen_show(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_BMA2x2_LOWPASS
	int X = BMA2x2_AXIS_X;
	struct i2c_client *client = bma2x2_i2c_client;
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(client);

	if (!client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][X],
				obj->fir.raw[idx][BMA2x2_AXIS_Y],
				obj->fir.raw[idx][BMA2x2_AXIS_Z]);
		}

		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[BMA2x2_AXIS_X],
			obj->fir.sum[BMA2x2_AXIS_Y],
			obj->fir.sum[BMA2x2_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[X] / len,
			obj->fir.sum[BMA2x2_AXIS_Y] / len,
			obj->fir.sum[BMA2x2_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t firlen_store(struct device_driver *ddri,
				  const char *buf, size_t count)
{
#ifdef CONFIG_BMA2x2_LOWPASS
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	int firlen;

	if (sscanf(buf, "%11d", &firlen) != 1) {
		GSE_ERR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		GSE_ERR("exceeds maximum filter length\n");
	} else {
		atomic_set(&obj->firlen, firlen);
		if (firlen == NULL) {
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
static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	ssize_t res;

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t trace_store(struct device_driver *ddri,
				 const char *buf, size_t count)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	int trace;

	if (sscanf(buf, "0x%11x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		GSE_ERR("invalid content: '%s'\n", buf);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	ssize_t len = 0;

	if (obj->hw) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num,
				obj->hw->direction,
				obj->hw->power_id,
				obj->hw->power_vol);
	} else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n",
			obj->client->addr, BMA_DRIVER_VERSION);
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t powerstatus_show(struct device_driver *ddri, char *buf)
{
	if (sensor_power)
		GSE_LOG("G sensor is in work mode, sensor_power = %d\n",
			sensor_power);
	else
		GSE_LOG("G sensor is in standby mode, sensor_power = %d\n",
			sensor_power);

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t fifo_mode_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned char data;

	if (bma2x2_get_fifo_mode(client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", data);

}

/*----------------------------------------------------------------------------*/
static ssize_t fifo_mode_store(struct device_driver *ddri,
				     const char *buf, size_t count)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_fifo_mode(client, (unsigned char) data) < 0)
		GSE_ERR("invalid content: '%s'\n", buf);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t fifo_framecount_show(struct device_driver *ddri,
		char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned char data;

	if (bma2x2_get_fifo_framecount(client, &data) < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");
	else
		return snprintf(buf, PAGE_SIZE, "%d\n", data);

}

/*----------------------------------------------------------------------------*/
static ssize_t fifo_framecount_store(struct device_driver *ddri,
		const char *buf, size_t count)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	unsigned long data;
	int error;

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	mutex_lock(&obj->lock);
	obj->fifo_count = (unsigned char)data;
	mutex_unlock(&obj->lock);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t fifo_data_frame_show(struct device_driver *ddri,
		char *buf)
{
	struct bma2x2_i2c_data *obj = i2c_get_clientdata(bma2x2_i2c_client);
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned char fifo_frame_cnt = 0;
	int err = 0;
	int read_cnt = 0;
	int num = 0;
	static char tmp_buf[32 * 6] = {0};
	char *pBuf = NULL;
	unsigned char f_len = 6;/* FIXME: ONLY USE 3-AXIS */

	if (!buf)
		return -EINVAL;


	err = bma2x2_get_fifo_framecount(client, &fifo_frame_cnt);
	if (err < 0)
		return snprintf(buf, PAGE_SIZE, "Read error\n");


	if (fifo_frame_cnt == 0) {
		GSE_ERR("fifo frame count is 0!!!");
		return 0;
	}
#ifdef READFIFOCOUNT
	read_cnt = fifo_frame_cnt;
	pBuf = &tmp_buf[0];
	mutex_lock(&obj->lock);
	while (read_cnt > 0) {
		if (bma_i2c_read_block(client, BMA2x2_FIFO_DATA_OUTPUT_REG,
			pBuf, f_len) < 0) {
			GSE_ERR("[a]fatal error\n");
			num = snprintf(buf, PAGE_SIZE, "Read byte block err\n");
			return num;
		}
		pBuf += f_len;
		read_cnt--;
		GSE_ERR("fifo_frame_cnt %d, f_len %d", fifo_frame_cnt, f_len);
	}
	mutex_unlock(&obj->lock);
	memcpy(buf, tmp_buf, fifo_frame_cnt * f_len);
#else
	if (bma_i2c_dma_read(client, BMA2x2_FIFO_DATA_OUTPUT_REG, buf,
			fifo_frame_cnt * f_len) < 0) {
		GSE_ERR("[a]fatal error\n");
		return snprintf(buf, PAGE_SIZE, "Read byte block error\n");
	}
#endif
	return fifo_frame_cnt * f_len;
}

/*----------------------------------------------------------------------------*/
static ssize_t dump_registers_show(struct device_driver *ddri, char *buf)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	struct i2c_client *client = bma2x2_i2c_client;
	unsigned char temp = 0;
	int count = 0;
	int num = 0;
	int ret = 0;

	if (!buf || !client)
		return -EINVAL;

	mutex_lock(&obj->lock);
	while (count <= MAX_REGISTER_MAP) {
		temp = 0;
		ret = bma_i2c_read_block(client, count, &temp, 1);
		if (ret < 0) {
			num = snprintf(buf, PAGE_SIZE, "Read byte error\n");
			return num;
		}
		num += snprintf(buf + num, PAGE_SIZE, "0x%x\n", temp);
		count++;
	}
	mutex_unlock(&obj->lock);
	return num;
}

/*----------------------------------------------------------------------------*/
static ssize_t softreset_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int ret = 0;
	int num = 0;

	if (!buf || !client)
		return -EINVAL;

	ret = bma2x2_softreset();
	if (ret < 0)
		num = snprintf(buf, PAGE_SIZE, "Write byte block error\n");
	else
		num = snprintf(buf, PAGE_SIZE, "softreset success\n");
	ret = bma2x2_init_client(client, 0);
	if (ret < 0)
		GSE_ERR("bma2x2_init_client failed\n");
	else
		GSE_ERR("bma2x2_init_client succeed\n");

	ret = BMA2x2_SetPowerMode(client, true);
	if (ret < 0)
		GSE_ERR("BMA2x2_SetPowerMode failed\n");
	else
		GSE_ERR("BMA2x2_SetPowerMode succeed\n");

	return num;
}

/*----------------------------------------------------------------------------*/
static ssize_t selftest_show(struct device_driver *ddri, char *buf)
{
	int ret = 0;
	int num = 0;

	int X_failed = 0, Y_failed = 0, Z_failed = 0;
	unsigned char selftestresult = 0;
	unsigned char diff4x = 0, diff4y = 0, diff4z = 0;

	if (!buf)
		return -EINVAL;

	ret = bma2x2_selftest(&selftestresult, &diff4x, &diff4y, &diff4z);
	if (ret < 0) {
		num = snprintf(buf, PAGE_SIZE, "error happened in selftest\n");
		return num;
	}
	if (selftestresult == 0) {
		num = snprintf(buf, PAGE_SIZE,
			"selftest success, diff4x=%d, diff4y=%d, diff4z=%d\n",
			diff4x, diff4y, diff4z);
	} else {
		X_failed = (selftestresult & 0x01) ? 1 : 0;
		Y_failed = (selftestresult & 0x02) ? 1 : 0;
		Z_failed = (selftestresult & 0x04) ? 1 : 0;
		num = snprintf(buf, PAGE_SIZE,
				"selftest failed, X_failed=%d, Y_failed=%d, Z_failed=%d, diff4x=%d, diff4y=%d, diff4z=%d\n",
				X_failed, Y_failed, Z_failed,
				diff4x, diff4y, diff4z);
	}

	return num;
}

/*----------------------------------------------------------------------------*/
static ssize_t gsensortest_show(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err = BMA2x2_SUCCESS;
	int data_x = 0;
	int data_y = 0;
	int data_z = 0;

	if (!client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	err = BMA2x2_ReadSensorData(client, &data_x, &data_y, &data_z);
	if (err != BMA2x2_SUCCESS) {
		GSE_ERR("show_sensordata_value() read sensor data error!\n");
		return 0;
	}
	GSE_LOG("show gsensor data x: %d y: %d z: %d\n",
		data_x, data_y, data_z);

	return snprintf(buf, PAGE_SIZE, "%d %d %d\n",
		(int32_t)data_x, (int32_t)data_y, (int32_t)data_z);
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR_RO(chipinfo);
static DRIVER_ATTR_RO(cpsdata);
static DRIVER_ATTR_RW(cpsopmode);
static DRIVER_ATTR_RW(cpsrange);
static DRIVER_ATTR_RW(cpsbandwidth);
static DRIVER_ATTR_RO(sensordata);
static DRIVER_ATTR_RW(cali);
static DRIVER_ATTR_RW(firlen);
static DRIVER_ATTR_RW(trace);
static DRIVER_ATTR_RO(status);
static DRIVER_ATTR_RO(powerstatus);
static DRIVER_ATTR_RW(fifo_mode);
static DRIVER_ATTR_RW(fifo_framecount);
static DRIVER_ATTR_RO(fifo_data_frame);
static DRIVER_ATTR_RO(dump_registers);
static DRIVER_ATTR_RO(softreset);
static DRIVER_ATTR_RO(selftest);
static DRIVER_ATTR_RO(gsensortest);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *bma2x2_attr_list[] = {
	/*chip information*/
	&driver_attr_chipinfo,
	/*dump sensor data*/
	&driver_attr_sensordata,
	/*show calibration data*/
	&driver_attr_cali,
	/*filter length: 0: disable, others: enable*/
	&driver_attr_firlen,
	/*trace log*/
	&driver_attr_trace,
	&driver_attr_status,
	&driver_attr_powerstatus,
	/*g sensor data for compass tilt compensation*/
	&driver_attr_cpsdata,
	/*g sensor opmode for compass tilt compensation*/
	&driver_attr_cpsopmode,
	/*g sensor range for compass tilt compensation*/
	&driver_attr_cpsrange,
	/*g sensor bandwidth for compass tilt compensation*/
	&driver_attr_cpsbandwidth,
	&driver_attr_fifo_mode,
	&driver_attr_fifo_framecount,
	&driver_attr_fifo_data_frame,
	&driver_attr_dump_registers,
	&driver_attr_softreset,
	&driver_attr_selftest,

	&driver_attr_gsensortest,
};
/*----------------------------------------------------------------------------*/
static int bma2x2_create_attr(struct device_driver *driver)
{
	int num = (int)(ARRAY_SIZE(bma2x2_attr_list));
	int idx, err = 0;

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bma2x2_attr_list[idx]);
		if (err) {
			GSE_ERR("driver_create_file (%s) = %d\n",
				bma2x2_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(bma2x2_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, bma2x2_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
/* if use  this typ of enable ,*/
/* Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int bma2x2_open_report_data(int open)
{
	/*should queuq work to report event if  is_report_input_direct=true*/
	return 0;
}

/* if use  this typ of enable ,*/
/*Gsensor only enabled but not report inputEvent to HAL*/
static int bma2x2_enable_nodata(int en)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err = 0;

	if (((en == 0) && (sensor_power == false))
	    || ((en == 1) && (sensor_power == true))) {
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		err = BMA2x2_SetPowerMode(client, !sensor_power);
	}

	return err;
}

/*
 * not really set the hardware
 */
static int bma2x2_set_delay(u64 ns)
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	int err = 0;

	int value = (int)ns / 1000 / 1000;


	if (err != BMA2x2_SUCCESS)
		GSE_ERR("Set delay parameter error!\n");

	if (value >= 50)
		atomic_set(&obj->filter, 0);
	else {
#if defined(CONFIG_BMA2x2_LOWPASS)
		obj->fir.num = 0;
		obj->fir.idx = 0;
		obj->fir.sum[BMA2x2_AXIS_X] = 0;
		obj->fir.sum[BMA2x2_AXIS_Y] = 0;
		obj->fir.sum[BMA2x2_AXIS_Z] = 0;
		atomic_set(&obj->filter, 1);
#endif
	}

	return 0;
}

static int bma2x2_get_data(int *px, int *py, int *pz, int *status)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err = BMA2x2_SUCCESS;

	/* use acc raw data for gsensor */
	err = BMA2x2_ReadSensorData(client, px, py, pz);
	if (err != BMA2x2_SUCCESS)
		GSE_ERR("%s read sensor data error!\n", __func__);
	else
		*status = SENSOR_STATUS_ACCURACY_HIGH;

	return err;
}

static int gsensor_acc_batch(int flag, int64_t samplingPeriodNs,
					int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs / 1000 / 1000;

	GSE_LOG("gsensor acc set delay = (%d) ok.\n", value);

	return bma2x2_set_delay(samplingPeriodNs);
}

static int gsensor_acc_flush(void)
{
	return acc_flush_report();
}


static int bma2x2_factory_enable_sensor(bool enabledisable,
					int64_t sample_periods_ms)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err;

	err = BMA2x2_SetPowerMode(client, 1);
	if (err) {
		GSE_ERR("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = gsensor_acc_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		GSE_ERR("%s enable set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}


static int bma2x2_factory_get_data(int32_t data[3], int *status)
{
	return bma2x2_get_data(&data[0], &data[1], &data[2], status);
}


static int bma2x2_factory_get_raw_data(int32_t data[3])
{
	struct i2c_client *client = bma2x2_i2c_client;
	char strbuf[BMA2x2_BUFSIZE] = {0};

	BMA2x2_ReadRawData(client, strbuf);
	data[0] = strbuf[0];
	data[1] = strbuf[1];
	data[2] = strbuf[2];

	return 0;
}


static int bma2x2_factory_enable_calibration(void)
{
	return 0;
}

static int bma2x2_factory_clear_cali(void)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err = 0;

	err = BMA2x2_ResetCalibration(client);
	return 0;
}


static int bma2x2_factory_set_cali(int32_t data[3])
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	int err = 0;
	int cali[3] = { 0 };

	cali[BMA2x2_ACC_AXIS_X] =
		data[0] * obj->reso->sensitivity / GRAVITY_EARTH_1000;
	cali[BMA2x2_ACC_AXIS_Y] =
		data[1] * obj->reso->sensitivity / GRAVITY_EARTH_1000;
	cali[BMA2x2_ACC_AXIS_Z] =
		data[2] * obj->reso->sensitivity / GRAVITY_EARTH_1000;
	err = BMA2x2_WriteCalibration(obj->client, cali);
	if (err) {
		GSE_ERR("LSM6DS3H_WriteCalibration failed!\n");
		return -1;
	}
	return 0;
}


static int bma2x2_factory_get_cali(int32_t data[3])
{
	struct bma2x2_i2c_data *obj = obj_i2c_data;
	int err = 0;
	int cali[3] = { 0 };

	err = BMA2x2_ReadCalibration(obj->client, cali);
	if (err) {
		GSE_ERR("BMA2x2_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[BMA2x2_ACC_AXIS_X]
			* GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	data[1] = cali[BMA2x2_ACC_AXIS_X]
			* GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	data[2] = cali[BMA2x2_ACC_AXIS_X]
			* GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	return 0;
}



static int bma2x2_factory_do_self_test(void)
{
	return 0;
}


static struct accel_factory_fops bma2x2_factory_fops = {
	.enable_sensor = bma2x2_factory_enable_sensor,
	.get_data = bma2x2_factory_get_data,
	.get_raw_data = bma2x2_factory_get_raw_data,
	.enable_calibration = bma2x2_factory_enable_calibration,
	.clear_cali = bma2x2_factory_clear_cali,
	.set_cali = bma2x2_factory_set_cali,
	.get_cali = bma2x2_factory_get_cali,
	.do_self_test = bma2x2_factory_do_self_test,
};


static struct accel_factory_public bma2x2_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &bma2x2_factory_fops,
};


/*----------------------------------------------------------------------------*/
static int bma2x2_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	static struct acc_init_info *init = &bma2x2_init_info;
	struct bma2x2_i2c_data *obj;
	struct acc_control_path ctl = {0};
	struct acc_data_path data = {0};
	int err = 0;

	GSE_FUN();
	err = get_accel_dts_func(client->dev.of_node, hw);
	if (err) {
		GSE_ERR("get dts info fail\n");
		err = -EFAULT;
		goto exit;
	}
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}

	client->addr = *hw->i2c_addr;
	obj_i2c_data = obj;
	bma2x2_i2c_client = client;
	obj->client = client;
	i2c_set_clientdata(client, obj);
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->in_suspend, 0);
	mutex_init(&obj->lock);

	err = bma2x2_init_client(client, 1);
	if (err)
		goto exit_init_failed;


	err = accel_factory_device_register(&bma2x2_factory_device);
	if (err) {
		GSE_ERR("accel_factory_device_register failed\n");
		goto exit_factory_device_register_failed;
	}

	err = bma2x2_create_attr(&(init->platform_diver_addr->driver));
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = bma2x2_open_report_data;
	ctl.enable_nodata = bma2x2_enable_nodata;
	ctl.set_delay  = bma2x2_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = false;
	ctl.batch = gsensor_acc_batch;
	ctl.flush = gsensor_acc_flush;

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = bma2x2_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err\n");
		goto exit_kfree;
	}

	bma2x2_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
exit_factory_device_register_failed:
exit_init_failed:
exit_kfree:
	kfree(obj);
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	bma2x2_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int bma2x2_i2c_remove(struct i2c_client *client)
{
	static struct acc_init_info *init = &bma2x2_init_info;
	int err = 0;

	err = bma2x2_delete_attr(&(init->platform_diver_addr->driver));
	if (err)
		GSE_ERR("bma150_delete_attr fail: %d\n", err);


	accel_factory_device_deregister(&bma2x2_factory_device);
	if (err)
		GSE_ERR("accel_factory_device_deregister fail: %d\n", err);


	bma2x2_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int idme_get_gsensorcal_calibration(void)
{
	struct i2c_client *client = bma2x2_i2c_client;
	int err;
	int dat[BMA2x2_AXES_NUM];
	char *gsensor_cal = NULL;
	char *sepstr;
	char *sepdata;

	long data_x;
	long data_y;
	long data_z;
	char buf[64] = {0};

	gsensor_cal = NULL; //idme_get_sensorcal();
	if (gsensor_cal == NULL) {
		GSE_ERR("idme get sensorcal fail!\n");
		return -1;
	}
	GSE_LOG("gsensor_cal %s\n", gsensor_cal);

	strcpy(buf, gsensor_cal);

	sepstr = buf;

	sepdata = strsep(&sepstr, ",");
	if (sepdata == NULL) {
		GSE_ERR("strsep calibration data fail\n");
		return -1;
	}

	GSE_LOG("gsensor_cal x %s\n", sepdata);
	err = kstrtol(sepdata, 10, &data_x);
	if (err) {
		GSE_ERR("calibration data x char to long fail\n");
		return -1;
	}

	sepdata = strsep(&sepstr, ",");
	if (sepdata == NULL) {
		GSE_ERR("strsep calibration data fail\n");
		return -1;
	}
	GSE_LOG("gsensor_cal y %s\n", sepdata);
	err = kstrtol(sepdata, 10, &data_y);
	if (err) {
		GSE_ERR("calibration data y char to long fail\n");
		return -1;
	}

	GSE_LOG("gsensor_cal z %s\n", sepstr);
	err = kstrtol(sepstr, 10, &data_z);

	dat[BMA2x2_AXIS_X] = (int)data_x;
	dat[BMA2x2_AXIS_Y] = (int)data_y;
	dat[BMA2x2_AXIS_Z] = (int)data_z;

	err = BMA2x2_WriteCalibration(client, dat);
	if (err)
		GSE_ERR("write calibration err = %d\n", err);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_local_init(void)
{
	GSE_FUN();

	if (i2c_add_driver(&bma2x2_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return BMA2x2_ERR_I2C;
	}
	if (-1 == bma2x2_init_flag)
		return BMA2x2_ERR_I2C;

	if (idme_get_gsensorcal_calibration() == -1)
		GSE_ERR("idme get gsensor value and calibration fail\n");

	return 0;
}
/*----------------------------------------------------------------------------*/
static int bma2x2_remove(void)
{
	GSE_FUN();
	i2c_del_driver(&bma2x2_i2c_driver);
	return 0;
}

static struct acc_init_info bma2x2_init_info = {
	.name = "bma255",
	.init = bma2x2_local_init,
	.uninit = bma2x2_remove,
};

/*----------------------------------------------------------------------------*/
static int __init bma2x2_init(void)
{
	GSE_FUN();
	acc_driver_add(&bma2x2_init_info);
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit bma2x2_exit(void)
{
	GSE_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(bma2x2_init);
module_exit(bma2x2_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BMA2x2 ACCELEROMETER SENSOR DRIVER");
MODULE_AUTHOR("contact@bosch-sensortec.com");

/* drivers/i2c/chips/mma8653.c - MMA8653 motion sensor driver
 *
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>

#include "upmu_sw.h"
#include "upmu_common.h"
#include "batch.h"

#include <cust_acc.h>
#include "mma8653.h"
#include <accel.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define CONFIG_MMA8653_LOWPASS	/*apply low pass filter on output */
/*----------------------------------------------------------------------------*/
#define MMA8653_AXIS_X          0
#define MMA8653_AXIS_Y          1
#define MMA8653_AXIS_Z          2
#define MMA8653_AXES_NUM        3
#define MMA8653_DATA_LEN        6
#define MMA8653_DEV_NAME        "MMA8653"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mma8653_i2c_id[] = { {MMA8653_DEV_NAME, 0}, {} };

/*the adapter id will be available in customization*/
#define COMPATIABLE_NAME "mediatek,mma8653"

/*----------------------------------------------------------------------------*/
static int mma8653_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mma8653_i2c_remove(struct i2c_client *client);
static int mma8653_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#ifndef CONFIG_USE_EARLY_SUSPEND
static int mma8653_suspend(struct i2c_client *client, pm_message_t msg);
static int mma8653_resume(struct i2c_client *client);
#endif

static int mma8653_local_init(void);
static int mma8653_remove(void);
static int mma8653_init_flag = -1;	/* 0<==>OK -1 <==> fail */
/*----------------------------------------------------------------------------*/
static int MMA8653_SetPowerMode(struct i2c_client *client, bool enable);

/*------------------------------------------------------------------------------*/
typedef enum {
	ADX_TRC_FILTER = 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL = 0x04,
	ADX_TRC_CALI = 0X08,
	ADX_TRC_INFO = 0X10,
	ADX_TRC_REGXYZ = 0X20,
} ADX_TRC;
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
	s16 raw[C_MAX_FIR_LENGTH][MMA8653_AXES_NUM];
	int sum[MMA8653_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
static struct acc_init_info mma8653_init_info = {
	.name = "mma8653",
	.init = mma8653_local_init,
	.uninit = mma8653_remove,
};

/*----------------------------------------------------------------------------*/
struct mma8653_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;

	/*misc */
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[MMA8653_AXES_NUM + 1];

	/*data */
	s8 offset[MMA8653_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[MMA8653_AXES_NUM + 1];

#if defined(CONFIG_MMA8653_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
	/*early suspend */
#ifdef CONFIG_USE_EARLY_SUSPEND
	struct early_suspend early_drv;
#endif
	u8 bandwidth;
};

static const struct of_device_id accel_of_match[] = {
	{.compatible = "mediatek,gsensor"},
	{},
};

/*----------------------------------------------------------------------------*/
static struct i2c_driver mma8653_i2c_driver = {
	.driver = {
		   /*.owner          = THIS_MODULE, */
		   .name = MMA8653_DEV_NAME,
		   .of_match_table = accel_of_match,
		   },
	.probe = mma8653_i2c_probe,
	.remove = mma8653_i2c_remove,
	.detect = mma8653_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = mma8653_suspend,
	.resume = mma8653_resume,
#endif
	.id_table = mma8653_i2c_id,
	/* .address_data = &mma8653_addr_data, */
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mma8653_i2c_client;
/* static struct platform_driver mma8653_gsensor_driver; */
static struct mma8653_i2c_data *obj_i2c_data;
static bool sensor_power;
static int sensor_suspend;
static struct GSENSOR_VECTOR3D gsensor_gain, gsensor_offset;
static char selftestRes[10] = { 0 };

static DEFINE_MUTEX(mma8653_i2c_mutex);
static DEFINE_MUTEX(mma8653_op_mutex);

static bool enable_status;


/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               pr_debug(GSE_TAG"%s\n", __func__)
#define GSE_ERR(fmt, args...)    pr_err(GSE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    pr_debug(GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/

struct acc_hw mma8653_accel_cust;
static struct acc_hw *hw = &mma8653_accel_cust;

static struct data_resolution mma8653_data_resolution[] = {
	/*8 combination by {FULL_RES,RANGE} */
	{{3, 9}, 256},		/*+/-2g  in 10-bit resolution:  3.9 mg/LSB */
	{{7, 8}, 128},		/*+/-4g  in 10-bit resolution:  7.8 mg/LSB */
	{{15, 6}, 64},		/*+/-8g  in 10-bit resolution: 15.6 mg/LSB */
	{{15, 6}, 64},		/*+/-2g  in 8-bit resolution:  3.9 mg/LSB (full-resolution) */
	{{31, 3}, 32},		/*+/-4g  in 8-bit resolution:  3.9 mg/LSB (full-resolution) */
	{{62, 5}, 16},		/*+/-8g  in 8-bit resolution:  3.9 mg/LSB (full-resolution) */
};

/*----------------------------------------------------------------------------*/
static struct data_resolution mma8653_offset_resolution = { {2, 0}, 512 };

/*static int hwmsen_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data)
{
	u8 buf;
	int ret = 0;

#ifdef CONFIG_MTK_I2C_EXTENSION
	client->addr = client->addr & I2C_MASK_FLAG | I2C_WR_FLAG | I2C_RS_FLAG;
#endif
	buf = addr;
	ret = i2c_master_send(client, (const char *)&buf, 1 << 8 | 1);
	if (ret < 0) {
		GSE_ERR("send command error!!\n");
		return -EFAULT;
	}

	*data = buf;
#ifdef CONFIG_MTK_I2C_EXTENSION
	client->addr = client->addr & I2C_MASK_FLAG;
#endif
	return 0;
}
*/
/*----------------------------------------------------------------------------*/
static int mma8653_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
/* add by zero 2015.3.17 */
	u8 buf;
	int ret = 0;

#ifdef CONFIG_MTK_I2C_EXTENSION
	client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_RS_FLAG;
#endif
	buf = addr;
	ret = i2c_master_send(client, (const char *)&buf, len << 8 | 1);
	if (ret < 0) {
		GSE_ERR("send command error!!\n");
		return -EFAULT;
	}

	*data = buf;
#ifdef CONFIG_MTK_I2C_EXTENSION
	client->addr = client->addr & I2C_MASK_FLAG;
#endif
	return 0;
}

static int mma8653_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{				/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&mma8653_i2c_mutex);
	if (!client) {
		mutex_unlock(&mma8653_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&mma8653_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++) {
		buf[num++] = data[idx];
	}

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		GSE_ERR("send command error!!\n");
		mutex_unlock(&mma8653_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&mma8653_i2c_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/

void dumpReg(struct i2c_client *client)
{
	int i = 0;
	u8 addr = 0x00;
	u8 regdata = 0;

	for (i = 0; i < 49; i++) {
		/* dump all */
		mma8653_i2c_read_block(client, addr, &regdata, 0x1);
		GSE_LOG("yucong Reg addr=%x regdata=%x\n", addr, regdata);
		addr++;
		if (addr == 01)
			addr = addr + 0x06;
		if (addr == 0x09)
			addr++;
		if (addr == 0x0A)
			addr++;
	}
}


static void MMA8653_power(struct acc_hw *hw, unsigned int on)
{
/*
	static unsigned int power_on;

	if (hw->power_id != POWER_NONE_MACRO)
	{
		GSE_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on)	{
			GSE_LOG("ignore power control: %d\n", on);
		} else if (on)	{
			if (!hwPowerOn(hw->power_id, hw->power_vol, "MMA8653"))
				GSE_ERR("power on fails!!\n");
		} else {
			if (!hwPowerDown(hw->power_id, "MMA8653"))
				GSE_ERR("power off fail!!\n");
		}
	}
	power_on = on;
*/
}

/*----------------------------------------------------------------------------*/
/* this function here use to set resolution and choose sensitivity */
static int MMA8653_SetDataResolution(struct i2c_client *client, u8 dataresolution)
{
	int err;
	u8 dat, reso;
	u8 databuf[10];
	int res = 0;
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);

	GSE_LOG("fwq set resolution  dataresolution= %d!\n", dataresolution);
	if ((mma8653_i2c_read_block(client, MMA8653_REG_CTL_REG2, databuf, 0x1)) < 0) {
		GSE_ERR("read power ctl register err!\n");
		return -1;
	}
	GSE_LOG("fwq read MMA8653_REG_CTL_REG2 =%x in %s\n", databuf[0], __func__);
	if (dataresolution == MMA8653_12BIT_RES) {
		databuf[0] |= MMA8653_12BIT_RES;
	} else {
		databuf[0] &= (~MMA8653_12BIT_RES);	/* 8 bit resolution */
	}

	res = mma8653_i2c_write_block(client, MMA8653_REG_CTL_REG2, databuf, 0x1);
	if (res < 0) {
		GSE_LOG("set resolution  failed!\n");
		return -1;
	} else {
		GSE_LOG("set resolution mode ok %x!\n", databuf[1]);
	}

	/* choose sensitivity depend on resolution and detect range */
	/* read detect range */
	if ((err = mma8653_i2c_read_block(client, MMA8653_REG_XYZ_DATA_CFG, &dat, 0x1)) < 0) {
		GSE_ERR("read detect range  fail!!\n");
		return err;
	}
	reso = (dataresolution & MMA8653_12BIT_RES) ? (0x00) : (0x03);


	if (dat & MMA8653_RANGE_2G) {
		reso = reso + MMA8653_RANGE_2G;
	}
	if (dat & MMA8653_RANGE_4G) {
		reso = reso + MMA8653_RANGE_4G;
	}
	if (dat & MMA8653_RANGE_8G) {
		reso = reso + MMA8653_RANGE_8G;
	}

	if (reso < sizeof(mma8653_data_resolution) / sizeof(mma8653_data_resolution[0])) {
		obj->reso = &mma8653_data_resolution[reso];
		GSE_LOG("reso=%x!! OK\n", reso);
		return 0;
	} else {
		GSE_ERR("choose sensitivity  fail!!\n");
		return -EINVAL;
	}
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadData(struct i2c_client *client, s16 data[MMA8653_AXES_NUM])
{
	struct mma8653_i2c_data *priv = i2c_get_clientdata(client);
	/* u8 addr = MMA8653_REG_DATAX0; */
	u8 buf[MMA8653_DATA_LEN] = { 0 };
	int err = 0;

	if (NULL == client) {
		err = -EINVAL;
	} else {
		/* hwmsen_read_block(client, addr, buf, 0x06); */
		/* dumpReg(client); */

		/* mma8653_i2c_read_block(client, MMA8653_REG_DATAX0, buf, 0x06); */
		/*****add by zero ***/
		buf[0] = MMA8653_REG_DATAX0;
#ifdef CONFIG_MTK_I2C_EXTENSION
		client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_RS_FLAG;
#endif
		i2c_master_send(client, (const char *)&buf, 6 << 8 | 1);
#ifdef CONFIG_MTK_I2C_EXTENSION
		client->addr = client->addr & I2C_MASK_FLAG;
#endif
		/***end************/

		data[MMA8653_AXIS_X] = (s16) ((buf[MMA8653_AXIS_X * 2] << 8) |
					      (buf[MMA8653_AXIS_X * 2 + 1]));
		data[MMA8653_AXIS_Y] = (s16) ((buf[MMA8653_AXIS_Y * 2] << 8) |
					      (buf[MMA8653_AXIS_Y * 2 + 1]));
		data[MMA8653_AXIS_Z] = (s16) ((buf[MMA8653_AXIS_Z * 2] << 8) |
					      (buf[MMA8653_AXIS_Z * 2 + 1]));

		if (atomic_read(&priv->trace) & ADX_TRC_REGXYZ) {
			GSE_LOG("raw from reg(SR) [%08X %08X %08X] => [%5d %5d %5d]\n",
				data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y], data[MMA8653_AXIS_Z],
				data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y], data[MMA8653_AXIS_Z]);
		}
		/* GSE_LOG("raw from reg(SR) [%08X %08X %08X] => [%5d %5d %5d]\n", data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y], data[MMA8653_AXIS_Z], */
		/* data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y], data[MMA8653_AXIS_Z]); */
		/* add to fix data, refer to datasheet */

		data[MMA8653_AXIS_X] = data[MMA8653_AXIS_X] >> 6;
		data[MMA8653_AXIS_Y] = data[MMA8653_AXIS_Y] >> 6;
		data[MMA8653_AXIS_Z] = data[MMA8653_AXIS_Z] >> 6;

		data[MMA8653_AXIS_X] += priv->cali_sw[MMA8653_AXIS_X];
		data[MMA8653_AXIS_Y] += priv->cali_sw[MMA8653_AXIS_Y];
		data[MMA8653_AXIS_Z] += priv->cali_sw[MMA8653_AXIS_Z];

		if (atomic_read(&priv->trace) & ADX_TRC_RAWDATA) {
			GSE_LOG("raw >>6it:[%08X %08X %08X] => [%5d %5d %5d]\n",
				data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y], data[MMA8653_AXIS_Z],
				data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y], data[MMA8653_AXIS_Z]);
		}
#ifdef CONFIG_MMA8653_LOWPASS
		if (atomic_read(&priv->filter)) {
			if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
				int idx, firlen = atomic_read(&priv->firlen);

				if (priv->fir.num < firlen) {
					priv->fir.raw[priv->fir.num][MMA8653_AXIS_X] =
					    data[MMA8653_AXIS_X];
					priv->fir.raw[priv->fir.num][MMA8653_AXIS_Y] =
					    data[MMA8653_AXIS_Y];
					priv->fir.raw[priv->fir.num][MMA8653_AXIS_Z] =
					    data[MMA8653_AXIS_Z];
					priv->fir.sum[MMA8653_AXIS_X] += data[MMA8653_AXIS_X];
					priv->fir.sum[MMA8653_AXIS_Y] += data[MMA8653_AXIS_Y];
					priv->fir.sum[MMA8653_AXIS_Z] += data[MMA8653_AXIS_Z];
					if (atomic_read(&priv->trace) & ADX_TRC_FILTER) {
						GSE_LOG
						    ("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n",
						     priv->fir.num,
						     priv->fir.raw[priv->fir.num][MMA8653_AXIS_X],
						     priv->fir.raw[priv->fir.num][MMA8653_AXIS_Y],
						     priv->fir.raw[priv->fir.num][MMA8653_AXIS_Z],
						     priv->fir.sum[MMA8653_AXIS_X],
						     priv->fir.sum[MMA8653_AXIS_Y],
						     priv->fir.sum[MMA8653_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				} else {
					idx = priv->fir.idx % firlen;
					priv->fir.sum[MMA8653_AXIS_X] -=
					    priv->fir.raw[idx][MMA8653_AXIS_X];
					priv->fir.sum[MMA8653_AXIS_Y] -=
					    priv->fir.raw[idx][MMA8653_AXIS_Y];
					priv->fir.sum[MMA8653_AXIS_Z] -=
					    priv->fir.raw[idx][MMA8653_AXIS_Z];
					priv->fir.raw[idx][MMA8653_AXIS_X] = data[MMA8653_AXIS_X];
					priv->fir.raw[idx][MMA8653_AXIS_Y] = data[MMA8653_AXIS_Y];
					priv->fir.raw[idx][MMA8653_AXIS_Z] = data[MMA8653_AXIS_Z];
					priv->fir.sum[MMA8653_AXIS_X] += data[MMA8653_AXIS_X];
					priv->fir.sum[MMA8653_AXIS_Y] += data[MMA8653_AXIS_Y];
					priv->fir.sum[MMA8653_AXIS_Z] += data[MMA8653_AXIS_Z];
					priv->fir.idx++;
					data[MMA8653_AXIS_X] =
					    priv->fir.sum[MMA8653_AXIS_X] / firlen;
					data[MMA8653_AXIS_Y] =
					    priv->fir.sum[MMA8653_AXIS_Y] / firlen;
					data[MMA8653_AXIS_Z] =
					    priv->fir.sum[MMA8653_AXIS_Z] / firlen;
					if (atomic_read(&priv->trace) & ADX_TRC_FILTER) {
						GSE_LOG
						    ("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n",
						     idx, priv->fir.raw[idx][MMA8653_AXIS_X],
						     priv->fir.raw[idx][MMA8653_AXIS_Y],
						     priv->fir.raw[idx][MMA8653_AXIS_Z],
						     priv->fir.sum[MMA8653_AXIS_X],
						     priv->fir.sum[MMA8653_AXIS_Y],
						     priv->fir.sum[MMA8653_AXIS_Z],
						     data[MMA8653_AXIS_X], data[MMA8653_AXIS_Y],
						     data[MMA8653_AXIS_Z]);
					}
				}
			}
		}
#endif
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadOffset(struct i2c_client *client, s8 ofs[MMA8653_AXES_NUM])
{
	int err;

	GSE_ERR("fwq read offset+:\n");
	if ((err = mma8653_i2c_read_block(client, MMA8653_REG_OFSX, &ofs[MMA8653_AXIS_X], 0x1)) < 0) {
		GSE_ERR("error: %d\n", err);
	}
	if ((err = mma8653_i2c_read_block(client, MMA8653_REG_OFSY, &ofs[MMA8653_AXIS_Y], 0x1)) < 0) {
		GSE_ERR("error: %d\n", err);
	}
	if ((err = mma8653_i2c_read_block(client, MMA8653_REG_OFSZ, &ofs[MMA8653_AXIS_Z], 0x1)) < 0) {
		GSE_ERR("error: %d\n", err);
	}
	GSE_LOG("fwq read off:  offX=%x ,offY=%x ,offZ=%x\n", ofs[MMA8653_AXIS_X],
		ofs[MMA8653_AXIS_Y], ofs[MMA8653_AXIS_Z]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ResetCalibration(struct i2c_client *client)
{
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	s8 ofs[MMA8653_AXES_NUM] = { 0x00, 0x00, 0x00 };
	int err;

	/* goto standby mode to clear cali */
	MMA8653_SetPowerMode(obj->client, false);
	if ((err = mma8653_i2c_write_block(client, MMA8653_REG_OFSX, ofs, 0x3)) < 0) {
		GSE_ERR("error: %d\n", err);
	}
	MMA8653_SetPowerMode(obj->client, true);
	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return err;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadCalibration(struct i2c_client *client, int dat[MMA8653_AXES_NUM])
{
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;

	if ((err = MMA8653_ReadOffset(client, obj->offset))) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	/* mul = obj->reso->sensitivity/mma8653_offset_resolution.sensitivity; */
	mul = mma8653_offset_resolution.sensitivity / obj->reso->sensitivity;
	dat[obj->cvt.map[MMA8653_AXIS_X]] =
	    obj->cvt.sign[MMA8653_AXIS_X] * (obj->offset[MMA8653_AXIS_X] / mul);
	dat[obj->cvt.map[MMA8653_AXIS_Y]] =
	    obj->cvt.sign[MMA8653_AXIS_Y] * (obj->offset[MMA8653_AXIS_Y] / mul);
	dat[obj->cvt.map[MMA8653_AXIS_Z]] =
	    obj->cvt.sign[MMA8653_AXIS_Z] * (obj->offset[MMA8653_AXIS_Z] / mul);
	GSE_LOG("fwq:read cali offX=%x ,offY=%x ,offZ=%x\n", obj->offset[MMA8653_AXIS_X],
		obj->offset[MMA8653_AXIS_Y], obj->offset[MMA8653_AXIS_Z]);
	/* GSE_LOG("fwq:read cali swX=%x ,swY=%x ,swZ=%x\n",obj->cali_sw[MMA8653_AXIS_X],obj->cali_sw[MMA8653_AXIS_Y],obj->cali_sw[MMA8653_AXIS_Z]); */
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadCalibrationEx(struct i2c_client *client, int act[MMA8653_AXES_NUM],
				     int raw[MMA8653_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data */
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;

	if ((err = MMA8653_ReadOffset(client, obj->offset))) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	/* mul = obj->reso->sensitivity/mma8653_offset_resolution.sensitivity; */
	mul = mma8653_offset_resolution.sensitivity / obj->reso->sensitivity;
	raw[MMA8653_AXIS_X] = obj->offset[MMA8653_AXIS_X] / mul + obj->cali_sw[MMA8653_AXIS_X];
	raw[MMA8653_AXIS_Y] = obj->offset[MMA8653_AXIS_Y] / mul + obj->cali_sw[MMA8653_AXIS_Y];
	raw[MMA8653_AXIS_Z] = obj->offset[MMA8653_AXIS_Z] / mul + obj->cali_sw[MMA8653_AXIS_Z];

	act[obj->cvt.map[MMA8653_AXIS_X]] = obj->cvt.sign[MMA8653_AXIS_X] * raw[MMA8653_AXIS_X];
	act[obj->cvt.map[MMA8653_AXIS_Y]] = obj->cvt.sign[MMA8653_AXIS_Y] * raw[MMA8653_AXIS_Y];
	act[obj->cvt.map[MMA8653_AXIS_Z]] = obj->cvt.sign[MMA8653_AXIS_Z] * raw[MMA8653_AXIS_Z];

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_WriteCalibration(struct i2c_client *client, int dat[MMA8653_AXES_NUM])
{
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
/* u8 testdata=0; */
	int err;
	int cali[MMA8653_AXES_NUM], raw[MMA8653_AXES_NUM];
	int lsb = mma8653_offset_resolution.sensitivity;
/* u8 databuf[2]; */
/* int res = 0; */
	/* int divisor = obj->reso->sensitivity/lsb; */
	int divisor = lsb / obj->reso->sensitivity;

	GSE_LOG("fwq obj->reso->sensitivity=%d\n", obj->reso->sensitivity);
	GSE_LOG("fwq lsb=%d\n", lsb);


	if ((err = MMA8653_ReadCalibrationEx(client, cali, raw))) {	/*offset will be updated in obj->offset */
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		raw[MMA8653_AXIS_X], raw[MMA8653_AXIS_Y], raw[MMA8653_AXIS_Z],
		obj->offset[MMA8653_AXIS_X], obj->offset[MMA8653_AXIS_Y],
		obj->offset[MMA8653_AXIS_Z], obj->cali_sw[MMA8653_AXIS_X],
		obj->cali_sw[MMA8653_AXIS_Y], obj->cali_sw[MMA8653_AXIS_Z]);

	/*calculate the real offset expected by caller */
	cali[MMA8653_AXIS_X] += dat[MMA8653_AXIS_X];
	cali[MMA8653_AXIS_Y] += dat[MMA8653_AXIS_Y];
	cali[MMA8653_AXIS_Z] += dat[MMA8653_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
		dat[MMA8653_AXIS_X], dat[MMA8653_AXIS_Y], dat[MMA8653_AXIS_Z]);

	obj->offset[MMA8653_AXIS_X] =
	    (s8) (obj->cvt.sign[MMA8653_AXIS_X] * (cali[obj->cvt.map[MMA8653_AXIS_X]]) * (divisor));
	obj->offset[MMA8653_AXIS_Y] =
	    (s8) (obj->cvt.sign[MMA8653_AXIS_Y] * (cali[obj->cvt.map[MMA8653_AXIS_Y]]) * (divisor));
	obj->offset[MMA8653_AXIS_Z] =
	    (s8) (obj->cvt.sign[MMA8653_AXIS_Z] * (cali[obj->cvt.map[MMA8653_AXIS_Z]]) * (divisor));

	/*convert software calibration using standard calibration */
	obj->cali_sw[MMA8653_AXIS_X] = 0;	/* obj->cvt.sign[MMA8653_AXIS_X]*(cali[obj->cvt.map[MMA8653_AXIS_X]])%(divisor); */
	obj->cali_sw[MMA8653_AXIS_Y] = 0;	/* obj->cvt.sign[MMA8653_AXIS_Y]*(cali[obj->cvt.map[MMA8653_AXIS_Y]])%(divisor); */
	obj->cali_sw[MMA8653_AXIS_Z] = 0;	/* obj->cvt.sign[MMA8653_AXIS_Z]*(cali[obj->cvt.map[MMA8653_AXIS_Z]])%(divisor); */

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		obj->offset[MMA8653_AXIS_X] + obj->cali_sw[MMA8653_AXIS_X],
		obj->offset[MMA8653_AXIS_Y] + obj->cali_sw[MMA8653_AXIS_Y],
		obj->offset[MMA8653_AXIS_Z] + obj->cali_sw[MMA8653_AXIS_Z],
		obj->offset[MMA8653_AXIS_X], obj->offset[MMA8653_AXIS_Y],
		obj->offset[MMA8653_AXIS_Z], obj->cali_sw[MMA8653_AXIS_X],
		obj->cali_sw[MMA8653_AXIS_Y], obj->cali_sw[MMA8653_AXIS_Z]);
	/*  */
	/* go to standby mode to set cali */
	MMA8653_SetPowerMode(obj->client, false);
	if ((err =
	     hwmsen_write_block(obj->client, MMA8653_REG_OFSX, obj->offset, MMA8653_AXES_NUM))) {
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
	MMA8653_SetPowerMode(obj->client, true);

	return err;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	res = mma8653_i2c_read_block(client, MMA8653_REG_DEVID, databuf, 0x1);
	GSE_LOG("mma8653 id %x!\n", databuf[0]);
	/* res = hwmsen_read_byte_sr(client,MMA8653_REG_DEVID,databuf); */
	if (databuf[0] != MMA8653_FIXED_DEVID) {
		GSE_LOG("mma8653 id %x!\n", databuf[0]);
		return MMA8653_ERR_IDENTIFICATION;
	}

	if (res < 0) {
		return MMA8653_ERR_I2C;
	}

	return MMA8653_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/* normal */
/* High resolution */
/* low noise low power */
/* low power */

/*---------------------------------------------------------------------------*/
static int MMA8653_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];
	int res = 0;
	u8 addr = MMA8653_REG_CTL_REG1;
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);

	GSE_FUN();
	if (enable == sensor_power) {
		GSE_LOG("Sensor power status need not to be set again!!!\n");
		return MMA8653_SUCCESS;
	}

	if ((mma8653_i2c_read_block(client, addr, databuf, 0x1)) < 0) {
		GSE_ERR("read power ctl register err!\n");
		return MMA8653_ERR_I2C;
	}

	databuf[0] &= ~MMA8653_MEASURE_MODE;

	if (enable == true) {
		databuf[0] |= MMA8653_MEASURE_MODE;
	} else {
		/* do nothing */
	}

	res = mma8653_i2c_write_block(client, MMA8653_REG_CTL_REG1, databuf, 0x1);

	if (res < 0) {
		GSE_LOG("fwq set power mode failed!\n");
		return MMA8653_ERR_I2C;
	} else if (atomic_read(&obj->trace) & ADX_TRC_INFO) {
		GSE_LOG("fwq set power mode ok %d!\n", databuf[1]);
	}

	sensor_power = enable;

	return MMA8653_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/* set detect range */

static int MMA8653_SetDataFormat(struct i2c_client *client, u8 dataformat)
{

/* struct mma8653_i2c_data *obj = i2c_get_clientdata(client); */
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	databuf[0] = dataformat;
	res = mma8653_i2c_write_block(client, MMA8653_REG_XYZ_DATA_CFG, databuf, 0x1);

	if (res < 0) {
		return MMA8653_ERR_I2C;
	}

	return 0;

	/* return MMA8653_SetDataResolution(obj,dataformat); */
}

/*----------------------------------------------------------------------------*/
static int MMA8653_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	if ((mma8653_i2c_read_block(client, MMA8653_REG_CTL_REG1, databuf, 0x1)) < 0) {
		GSE_ERR("read power ctl register err!\n");
		return MMA8653_ERR_I2C;
	}
	GSE_LOG("fwq read MMA8653_REG_CTL_REG1 =%x in %s\n", databuf[0], __func__);

	databuf[0] &= 0xC7;	/* clear original  data rate */

	databuf[0] |= bwrate;	/* set data rate */


	res = mma8653_i2c_write_block(client, MMA8653_REG_CTL_REG1, databuf, 0x1);
	if (res < 0) {
		return MMA8653_ERR_I2C;
	}

	return MMA8653_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*static int MMA8653_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	databuf[0] = intenable;

	res = mma8653_i2c_write_block(client, MMA8653_REG_CTL_REG4, databuf, 0x1);
	if (res < 0) {
		return MMA8653_ERR_I2C;
	}

	return MMA8653_SUCCESS;
}
*/
/*----------------------------------------------------------------------------*/
static int MMA8653_Init(struct i2c_client *client, int reset_cali)
{
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GSE_LOG("2010-11-03-11:43 fwq mma8653 addr %x!\n", client->addr);

	res = MMA8653_CheckDeviceID(client);
	if (res < 0) {
		GSE_ERR("fwq mma8653 check id error\n");
		return res;
	}

	res = MMA8653_SetPowerMode(client, false);
	if (res < 0) {
		GSE_ERR("fwq mma8653 set power error\n");
		return res;
	}

	res = MMA8653_SetBWRate(client, MMA8653_BW_100HZ);
	if (res < 0) {
		GSE_ERR("fwq mma8653 set BWRate error\n");
		return res;
	}

	res = MMA8653_SetDataFormat(client, MMA8653_RANGE_2G);
	if (res < 0) {
		GSE_ERR("fwq mma8653 set data format error\n");
		return res;
	}
	/* add by fwq */
	res = MMA8653_SetDataResolution(client, MMA8653_12BIT_RES);
	if (res < 0) {
		GSE_ERR("fwq mma8653 set data reslution error\n");
		return res;
	}
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;
	/*//we do not use interrupt
	   res = MMA8653_SetIntEnable(client, MMA8653_DATA_READY);
	   if(res != MMA8653_SUCCESS)//0x2E->0x80
	   {
	   return res;
	   }
	 */

	if (0 != reset_cali) {
		/*reset calibration only in power on */
		GSE_ERR("fwq mma8653  set cali\n");
		res = MMA8653_ResetCalibration(client);
		if (res < 0) {
			GSE_ERR("fwq mma8653 set cali error\n");
			return res;
		}
	}
#ifdef CONFIG_MMA8653_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif
	GSE_LOG("mma8653 Init OK\n");
	return MMA8653_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "MMA8653 Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct mma8653_i2c_data *obj = (struct mma8653_i2c_data *)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[MMA8653_AXES_NUM];
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
		/* GSE_LOG("sensor in suspend read not data!\n"); */
		return 0;
	}

	res = MMA8653_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	} else {
		obj->data[MMA8653_AXIS_X] += obj->cali_sw[MMA8653_AXIS_X];
		obj->data[MMA8653_AXIS_Y] += obj->cali_sw[MMA8653_AXIS_Y];
		obj->data[MMA8653_AXIS_Z] += obj->cali_sw[MMA8653_AXIS_Z];

		/*remap coordinate */
		acc[obj->cvt.map[MMA8653_AXIS_X]] =
		    obj->cvt.sign[MMA8653_AXIS_X] * obj->data[MMA8653_AXIS_X];
		acc[obj->cvt.map[MMA8653_AXIS_Y]] =
		    obj->cvt.sign[MMA8653_AXIS_Y] * obj->data[MMA8653_AXIS_Y];
		acc[obj->cvt.map[MMA8653_AXIS_Z]] =
		    obj->cvt.sign[MMA8653_AXIS_Z] * obj->data[MMA8653_AXIS_Z];

		/* GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[MMA8653_AXIS_X], acc[MMA8653_AXIS_Y], acc[MMA8653_AXIS_Z]); */

		/* Out put the mg */
		acc[MMA8653_AXIS_X] =
		    acc[MMA8653_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[MMA8653_AXIS_Y] =
		    acc[MMA8653_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[MMA8653_AXIS_Z] =
		    acc[MMA8653_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;


		sprintf(buf, "%04x %04x %04x", acc[MMA8653_AXIS_X], acc[MMA8653_AXIS_Y],
			acc[MMA8653_AXIS_Z]);
		if (atomic_read(&obj->trace) & ADX_TRC_IOCTL) {
			GSE_LOG("gsensor data: %s!\n", buf);
			GSE_LOG("gsensor data:  sensitivity x=%d\n", gsensor_gain.z);
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_ReadRawData(struct i2c_client *client, char *buf)
{
	struct mma8653_i2c_data *obj = (struct mma8653_i2c_data *)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client) {
		return EINVAL;
	}

	if ((res = MMA8653_ReadData(client, obj->data))) {
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	} else {
		sprintf(buf, "%04x %04x %04x", obj->data[MMA8653_AXIS_X],
			obj->data[MMA8653_AXIS_Y], obj->data[MMA8653_AXIS_Z]);

	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
/* u8  data; */
	u8 databuf[10];

	GSE_LOG("fwq init self test\n");
	res = MMA8653_SetBWRate(client, MMA8653_BW_100HZ);
	if (res != MMA8653_SUCCESS)	/*  */
	{
		return res;
	}

	res = MMA8653_SetDataFormat(client, MMA8653_RANGE_2G);
	if (res != MMA8653_SUCCESS)	/* 0x2C->BW=100Hz */
	{
		return res;
	}
	res = MMA8653_SetDataResolution(client, MMA8653_12BIT_RES);
	if (res != MMA8653_SUCCESS) {
		GSE_LOG("fwq mma8653 set data reslution error\n");
		return res;
	}
	/* set self test reg */
	memset(databuf, 0, sizeof(u8) * 10);
	if ((mma8653_i2c_read_block(client, MMA8653_REG_CTL_REG2, databuf, 0x1)) < 0) {
		GSE_ERR("read power ctl register err!\n");
		return MMA8653_ERR_I2C;
	}

	databuf[0] &= ~0x80;	/* clear original */
	databuf[0] |= 0x80;	/* set self test */

	res = mma8653_i2c_write_block(client, MMA8653_REG_CTL_REG2, databuf, 0x1);
	if (res < 0) {
		GSE_LOG("fwq set selftest error\n");
		return MMA8653_ERR_I2C;
	}

	GSE_LOG("fwq init self test OK\n");
	return MMA8653_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MMA8653_JudgeTestResult(struct i2c_client *client, s32 prv[MMA8653_AXES_NUM],
				   s32 nxt[MMA8653_AXES_NUM])
{
	struct criteria {
		int min;
		int max;
	};

	struct criteria self[6][3] = {
		{{-2, 11}, {-2, 16}, {-20, 105} },
		{{-10, 89}, {0, 125}, {0, 819} },
		{{12, 135}, {-135, -12}, {19, 219} },
		{{6, 67}, {-67, -6}, {10, 110} },
		{{6, 67}, {-67, -6}, {10, 110} },
		{{50, 540}, {-540, -50}, {75, 875} },
	};
	struct criteria (*ptr)[3] = NULL;
	u8 detectRage;
	u8 tmp_resolution;
	int res;

	GSE_LOG("fwq judge test result\n");
	if ((res = mma8653_i2c_read_block(client, MMA8653_REG_XYZ_DATA_CFG, &detectRage, 0x1)) < 0)
		return res;
	if ((res = mma8653_i2c_read_block(client, MMA8653_REG_CTL_REG2, &tmp_resolution, 0x1)) < 0)
		return res;

	GSE_LOG("fwq tmp_resolution=%x , detectRage=%x\n", tmp_resolution, detectRage);
	if ((tmp_resolution & MMA8653_12BIT_RES) && (detectRage == 0x00))
		ptr = &self[0];
	else if ((tmp_resolution & MMA8653_12BIT_RES) && (detectRage & MMA8653_RANGE_4G)) {
		ptr = &self[1];
		GSE_LOG("fwq self test choose ptr1\n");
	} else if ((tmp_resolution & MMA8653_12BIT_RES) && (detectRage & MMA8653_RANGE_8G))
		ptr = &self[2];
	else if (detectRage & MMA8653_RANGE_2G)	/* 8 bit resolution */
		ptr = &self[3];
	else if (detectRage & MMA8653_RANGE_4G)	/* 8 bit resolution */
		ptr = &self[4];
	else if (detectRage & MMA8653_RANGE_8G)	/* 8 bit resolution */
		ptr = &self[5];


	if (!ptr) {
		GSE_ERR("null pointer\n");
		GSE_LOG("fwq ptr null\n");
		return -EINVAL;
	}

	if (((nxt[MMA8653_AXIS_X] - prv[MMA8653_AXIS_X]) > (*ptr)[MMA8653_AXIS_X].max) ||
	    ((nxt[MMA8653_AXIS_X] - prv[MMA8653_AXIS_X]) < (*ptr)[MMA8653_AXIS_X].min)) {
		GSE_ERR("X is over range\n");
		res = -EINVAL;
	}
	if (((nxt[MMA8653_AXIS_Y] - prv[MMA8653_AXIS_Y]) > (*ptr)[MMA8653_AXIS_Y].max) ||
	    ((nxt[MMA8653_AXIS_Y] - prv[MMA8653_AXIS_Y]) < (*ptr)[MMA8653_AXIS_Y].min)) {
		GSE_ERR("Y is over range\n");
		res = -EINVAL;
	}
	if (((nxt[MMA8653_AXIS_Z] - prv[MMA8653_AXIS_Z]) > (*ptr)[MMA8653_AXIS_Z].max) ||
	    ((nxt[MMA8653_AXIS_Z] - prv[MMA8653_AXIS_Z]) < (*ptr)[MMA8653_AXIS_Z].min)) {
		GSE_ERR("Z is over range\n");
		res = -EINVAL;
	}
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mma8653_i2c_client;
	char strbuf[MMA8653_BUFSIZE];

	GSE_LOG("fwq show_chipinfo_value\n");

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	MMA8653_ReadChipInfo(client, strbuf, MMA8653_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mma8653_i2c_client;
	char strbuf[MMA8653_BUFSIZE];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	MMA8653_ReadSensorData(client, strbuf, MMA8653_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mma8653_i2c_client;
	struct mma8653_i2c_data *obj;
	int tmp[MMA8653_AXES_NUM];
	int mul, len;

	len = 0;
	GSE_LOG("fwq show_cali_value\n");

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);


	if (MMA8653_ReadOffset(client, obj->offset)) {
		return -EINVAL;
	} else if (MMA8653_ReadCalibration(client, tmp)) {
		return -EINVAL;
	} else {
		mul = obj->reso->sensitivity / mma8653_offset_resolution.sensitivity;
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,
			     obj->offset[MMA8653_AXIS_X], obj->offset[MMA8653_AXIS_Y],
			     obj->offset[MMA8653_AXIS_Z], obj->offset[MMA8653_AXIS_X],
			     obj->offset[MMA8653_AXIS_Y], obj->offset[MMA8653_AXIS_Z]);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
			     obj->cali_sw[MMA8653_AXIS_X], obj->cali_sw[MMA8653_AXIS_Y],
			     obj->cali_sw[MMA8653_AXIS_Z]);

		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
			     obj->offset[MMA8653_AXIS_X] * mul + obj->cali_sw[MMA8653_AXIS_X],
			     obj->offset[MMA8653_AXIS_Y] * mul + obj->cali_sw[MMA8653_AXIS_Y],
			     obj->offset[MMA8653_AXIS_Z] * mul + obj->cali_sw[MMA8653_AXIS_Z],
			     tmp[MMA8653_AXIS_X], tmp[MMA8653_AXIS_Y], tmp[MMA8653_AXIS_Z]);

		return len;
	}
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = mma8653_i2c_client;
	int err, x, y, z;
	int dat[MMA8653_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		err = MMA8653_ResetCalibration(client);
		if (err) {
			GSE_ERR("reset offset err = %d\n", err);
		}
	} else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z)) {
		dat[MMA8653_AXIS_X] = x;
		dat[MMA8653_AXIS_Y] = y;
		dat[MMA8653_AXIS_Z] = z;
		err = MMA8653_WriteCalibration(client, dat);
		if (err) {
			GSE_ERR("write calibration err = %d\n", err);
		}
	} else {
		GSE_ERR("invalid format\n");
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mma8653_i2c_client;
	/* struct mma8653_i2c_data *obj; */
/* int result =0; */
	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	GSE_LOG("fwq  selftestRes value =%s\n", selftestRes);
	return snprintf(buf, 10, "%s\n", selftestRes);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, const char *buf, size_t count)
{				/*write anything to this register will trigger the process */
	struct item {
		s16 raw[MMA8653_AXES_NUM];
	};

	struct i2c_client *client = mma8653_i2c_client;
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	int idx, res, num;
	struct item *prv = NULL, *nxt = NULL;
	s32 avg_prv[MMA8653_AXES_NUM] = { 0, 0, 0 };
	s32 avg_nxt[MMA8653_AXES_NUM] = { 0, 0, 0 };
	u8 databuf[10];

	if (1 != sscanf(buf, "%d", &num)) {
		GSE_ERR("parse number fail\n");
		return count;
	} else if (num == 0) {
		GSE_ERR("invalid data count\n");
		return count;
	}

	prv = kcalloc(num, sizeof(*prv), GFP_KERNEL);
	nxt = kcalloc(num, sizeof(*nxt), GFP_KERNEL);
	if (!prv || !nxt) {
		goto exit;
	}

	res = MMA8653_SetPowerMode(client, true);
	if (res != MMA8653_SUCCESS)	/*  */
	{
		return res;
	}

	GSE_LOG("NORMAL:\n");
	for (idx = 0; idx < num; idx++) {
		if ((res = MMA8653_ReadData(client, prv[idx].raw))) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}

		avg_prv[MMA8653_AXIS_X] += prv[idx].raw[MMA8653_AXIS_X];
		avg_prv[MMA8653_AXIS_Y] += prv[idx].raw[MMA8653_AXIS_Y];
		avg_prv[MMA8653_AXIS_Z] += prv[idx].raw[MMA8653_AXIS_Z];
		GSE_LOG("[%5d %5d %5d]\n", prv[idx].raw[MMA8653_AXIS_X],
			prv[idx].raw[MMA8653_AXIS_Y], prv[idx].raw[MMA8653_AXIS_Z]);
	}

	avg_prv[MMA8653_AXIS_X] /= num;
	avg_prv[MMA8653_AXIS_Y] /= num;
	avg_prv[MMA8653_AXIS_Z] /= num;

	res = MMA8653_SetPowerMode(client, false);
	if (res != MMA8653_SUCCESS)	/*  */
	{
		return res;
	}

	/*initial setting for self test */
	MMA8653_InitSelfTest(client);
	GSE_LOG("SELFTEST:\n");
/*
	MMA8653_ReadData(client, nxt[0].raw);
	GSE_LOG("nxt[0].raw[MMA8653_AXIS_X]: %d\n", nxt[0].raw[MMA8653_AXIS_X]);
	GSE_LOG("nxt[0].raw[MMA8653_AXIS_Y]: %d\n", nxt[0].raw[MMA8653_AXIS_Y]);
	GSE_LOG("nxt[0].raw[MMA8653_AXIS_Z]: %d\n", nxt[0].raw[MMA8653_AXIS_Z]);
	*/
	for (idx = 0; idx < num; idx++) {
		if ((res = MMA8653_ReadData(client, nxt[idx].raw))) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}
		avg_nxt[MMA8653_AXIS_X] += nxt[idx].raw[MMA8653_AXIS_X];
		avg_nxt[MMA8653_AXIS_Y] += nxt[idx].raw[MMA8653_AXIS_Y];
		avg_nxt[MMA8653_AXIS_Z] += nxt[idx].raw[MMA8653_AXIS_Z];
		GSE_LOG("[%5d %5d %5d]\n", nxt[idx].raw[MMA8653_AXIS_X],
			nxt[idx].raw[MMA8653_AXIS_Y], nxt[idx].raw[MMA8653_AXIS_Z]);
	}

	/* softrestet */

	memset(databuf, 0, sizeof(u8) * 10);
	if ((mma8653_i2c_read_block(client, MMA8653_REG_CTL_REG2, databuf, 0x1)) < 0) {
		GSE_ERR("read power ctl2 register err!\n");
		return MMA8653_ERR_I2C;
	}

	databuf[0] &= ~0x40;	/* clear original */
	databuf[0] |= 0x40;

	res = mma8653_i2c_write_block(client, MMA8653_REG_CTL_REG2, databuf, 0x1);
	if (res < 0) {
		GSE_LOG("fwq softrest error\n");
		return MMA8653_ERR_I2C;
	}
	/*  */
	MMA8653_Init(client, 0);

	avg_nxt[MMA8653_AXIS_X] /= num;
	avg_nxt[MMA8653_AXIS_Y] /= num;
	avg_nxt[MMA8653_AXIS_Z] /= num;

	GSE_LOG("X: %5d - %5d = %5d\n", avg_nxt[MMA8653_AXIS_X], avg_prv[MMA8653_AXIS_X],
		avg_nxt[MMA8653_AXIS_X] - avg_prv[MMA8653_AXIS_X]);
	GSE_LOG("Y: %5d - %5d = %5d\n", avg_nxt[MMA8653_AXIS_Y], avg_prv[MMA8653_AXIS_Y],
		avg_nxt[MMA8653_AXIS_Y] - avg_prv[MMA8653_AXIS_Y]);
	GSE_LOG("Z: %5d - %5d = %5d\n", avg_nxt[MMA8653_AXIS_Z], avg_prv[MMA8653_AXIS_Z],
		avg_nxt[MMA8653_AXIS_Z] - avg_prv[MMA8653_AXIS_Z]);

	if (!MMA8653_JudgeTestResult(client, avg_prv, avg_nxt)) {
		GSE_LOG("SELFTEST : PASS\n");
		atomic_set(&obj->selftest, 1);
		strcpy(selftestRes, "y");

	} else {
		GSE_LOG("SELFTEST : FAIL\n");
		atomic_set(&obj->selftest, 0);
		strcpy(selftestRes, "n");
	}

exit:
	/*restore the setting */
	MMA8653_Init(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_MMA8653_LOWPASS
	struct i2c_client *client = mma8653_i2c_client;
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);

	GSE_LOG("fwq show_firlen_value\n");
	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][MMA8653_AXIS_X],
				obj->fir.raw[idx][MMA8653_AXIS_Y],
				obj->fir.raw[idx][MMA8653_AXIS_Z]);
		}

		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[MMA8653_AXIS_X],
			obj->fir.sum[MMA8653_AXIS_Y], obj->fir.sum[MMA8653_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[MMA8653_AXIS_X] / len,
			obj->fir.sum[MMA8653_AXIS_Y] / len, obj->fir.sum[MMA8653_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_MMA8653_LOWPASS
	int firlen;
	struct i2c_client *client = mma8653_i2c_client;
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);

	GSE_LOG("fwq store_firlen_value\n");

	if (1 != sscanf(buf, "%d", &firlen)) {
		GSE_ERR("invallid format\n");
	} else if (firlen > C_MAX_FIR_LENGTH) {
		GSE_ERR("exceeds maximum filter length\n");
	} else {
		atomic_set(&obj->firlen, firlen);
		if (0 == firlen) {
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
	struct mma8653_i2c_data *obj = obj_i2c_data;

	GSE_LOG("fwq show_trace_value\n");
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
	int trace;
	struct mma8653_i2c_data *obj = obj_i2c_data;

	GSE_LOG("fwq store_trace_value\n");
	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GSE_ERR("invalid content: '%s', length = %d\n", buf, (int)count);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct mma8653_i2c_data *obj = obj_i2c_data;
	struct i2c_client *client = mma8653_i2c_client;

	GSE_LOG("fwq show_status_value\n");
	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	dumpReg(client);
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
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(selftest, S_IWUSR | S_IRUGO, show_selftest_value, store_selftest_value);
static DRIVER_ATTR(firlen, S_IWUSR | S_IRUGO, show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *mma8653_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_cali,	/*show calibration data */
	&driver_attr_selftest,	/*self test demo */
	&driver_attr_firlen,	/*filter length: 0: disable, others: enable */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
};

/*----------------------------------------------------------------------------*/
static int mma8653_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(mma8653_attr_list) / sizeof(mma8653_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		if ((err = driver_create_file(driver, mma8653_attr_list[idx]))) {
			GSE_ERR("driver_create_file (%s) = %d\n", mma8653_attr_list[idx]->attr.name,
				err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mma8653_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(mma8653_attr_list) / sizeof(mma8653_attr_list[0]));

	if (driver == NULL) {
		return -EINVAL;
	}


	for (idx = 0; idx < num; idx++) {
		driver_remove_file(driver, mma8653_attr_list[idx]);
	}


	return err;
}

/*----------------------------------------------------------------------------*/
/*
int mma8653_operate(void *self, uint32_t command, void *buff_in, int size_in,
		    void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value, sample_delay;
	struct mma8653_i2c_data *priv = (struct mma8653_i2c_data *)self;
	hwm_sensor_data *gsensor_data;
	char buff[MMA8653_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GSE_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 5) {
				sample_delay = MMA8653_BW_200HZ;
			} else if (value <= 10) {
				sample_delay = MMA8653_BW_100HZ;
			} else {
				sample_delay = MMA8653_BW_50HZ;
			}
			mutex_lock(&mma8653_op_mutex);
			err = MMA8653_SetBWRate(priv->client, MMA8653_BW_100HZ);
			if (err != MMA8653_SUCCESS)
			{
				GSE_ERR("Set delay parameter error!\n");
			}
			mutex_unlock(&mma8653_op_mutex);
			if (value >= 50) {
				atomic_set(&priv->filter, 0);
			} else {
				priv->fir.num = 0;
				priv->fir.idx = 0;
				priv->fir.sum[MMA8653_AXIS_X] = 0;
				priv->fir.sum[MMA8653_AXIS_Y] = 0;
				priv->fir.sum[MMA8653_AXIS_Z] = 0;
				atomic_set(&priv->filter, 1);
			}
		}
		break;

	case SENSOR_ENABLE:
		GSE_LOG("fwq sensor enable gsensor\n");
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GSE_ERR("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			mutex_lock(&mma8653_op_mutex);
			GSE_LOG("Gsensor device enable function enable = %d, sensor_power = %d!\n",
				value, sensor_power);
			if (((value == 0) && (sensor_power == false))
			    || ((value == 1) && (sensor_power == true))) {
				enable_status = sensor_power;
				GSE_LOG("Gsensor device have updated!\n");
			} else {
				enable_status = !sensor_power;
				err = MMA8653_SetPowerMode(priv->client, !sensor_power);
				GSE_LOG
				    ("Gsensor not in suspend MMA8653_SetPowerMode!, enable_status = %d\n",
				     enable_status);
			}
			mutex_unlock(&mma8653_op_mutex);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			GSE_ERR("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			gsensor_data = (hwm_sensor_data *) buff_out;
			MMA8653_ReadSensorData(priv->client, buff, MMA8653_BUFSIZE);
			sscanf(buff, "%x %x %x", &gsensor_data->values[0],
			       &gsensor_data->values[1], &gsensor_data->values[2]);
			gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			gsensor_data->value_divide = 1000;
			 GSE_LOG("X :%d,Y: %d, Z: %d\n",gsensor_data->values[0],gsensor_data->values[1],gsensor_data->values[2]);
		}
		break;
	default:
		GSE_ERR("gsensor operate function no this parameter %d!\n", command);
		err = -1;
		break;
	}

	return err;
}
*/
/******************************************************************************
 * Function Configuration
******************************************************************************/
static int mma8653_open(struct inode *inode, struct file *file)
{
	file->private_data = mma8653_i2c_client;

	if (file->private_data == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int mma8653_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static long mma8653_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct mma8653_i2c_data *obj = (struct mma8653_i2c_data *)i2c_get_clientdata(client);
	char strbuf[MMA8653_BUFSIZE];
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
		/* GSE_LOG("fwq GSENSOR_IOCTL_INIT\n"); */
		MMA8653_Init(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		/* GSE_LOG("fwq GSENSOR_IOCTL_READ_CHIPINFO\n"); */
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MMA8653_ReadChipInfo(client, strbuf, MMA8653_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_SENSORDATA:
		/* GSE_LOG("fwq GSENSOR_IOCTL_READ_SENSORDATA\n"); */
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		MMA8653_SetPowerMode(client, true);
		MMA8653_ReadSensorData(client, strbuf, MMA8653_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_GAIN:
		/* GSE_LOG("fwq GSENSOR_IOCTL_READ_GAIN\n"); */
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

	case GSENSOR_IOCTL_READ_OFFSET:
		/* GSE_LOG("fwq GSENSOR_IOCTL_READ_OFFSET\n"); */
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data, &gsensor_offset, sizeof(struct GSENSOR_VECTOR3D))) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_READ_RAW_DATA:
		/* GSE_LOG("fwq GSENSOR_IOCTL_READ_RAW_DATA\n"); */
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		MMA8653_ReadRawData(client, strbuf);
		if (copy_to_user(data, &strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case GSENSOR_IOCTL_SET_CALI:
		GSE_LOG("zero GSENSOR_IOCTL_SET_CALI\n");
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
			GSE_LOG("fwq going to set cali\n");
			GSE_LOG("zero sensor_data.x = %d sensor_data.y = %d,sensor_data.z = %d\n",
			       sensor_data.x, sensor_data.y, sensor_data.z);
			if (sensor_data.x > 0)
				sensor_data.x = sensor_data.x + 38;
			else if (sensor_data.x < 0)
				sensor_data.x = sensor_data.x - 38;

			if (sensor_data.y > 0)
				sensor_data.y = sensor_data.y + 38;
			else if (sensor_data.y < 0)
				sensor_data.y = sensor_data.y - 38;

			if (sensor_data.z > 0)
				sensor_data.z = sensor_data.z + 38;
			else if (sensor_data.z < 0)
				sensor_data.z = sensor_data.z - 38;
			GSE_LOG("zero sensor_data.x = %d sensor_data.y = %d,sensor_data.z = %d\n",
			       sensor_data.x, sensor_data.y, sensor_data.z);

			cali[MMA8653_AXIS_X] =
			    sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[MMA8653_AXIS_Y] =
			    sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[MMA8653_AXIS_Z] =
			    sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			GSE_LOG("zero cali[MMA8653_AXIS_X] = %d y = %d z = %d\n",
			       cali[MMA8653_AXIS_X], cali[MMA8653_AXIS_Y], cali[MMA8653_AXIS_Z]);
			err = MMA8653_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		/* GSE_LOG("fwq GSENSOR_IOCTL_CLR_CALI!!\n"); */
		err = MMA8653_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		/* GSE_LOG("fwq GSENSOR_IOCTL_GET_CALI\n"); */
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if ((err = MMA8653_ReadCalibration(client, cali))) {
			break;
		}

		sensor_data.x = cali[MMA8653_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.y = cali[MMA8653_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.z = cali[MMA8653_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		GSE_LOG
		    ("zero GSENSOR_IOCTL_GET_CALI sensor_data.x = %d sensor_data.y = %d,sensor_data.z = %d\n",
		     sensor_data.x, sensor_data.y, sensor_data.z);
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
static long mma8653_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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
	case COMPAT_GSENSOR_IOCTL_INIT:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_INIT : , (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_INIT: unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_READ_CHIPINFO:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_CHIPINFO,
					       (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_READ_CHIPINFO unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_READ_GAIN:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_GAIN, (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_READ_GAIN unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_READ_OFFSET:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_OFFSET,
					       (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_READ_OFFSET unlocked_ioctl failed.");
			return err;
		}
		break;
	case COMPAT_GSENSOR_IOCTL_READ_RAW_DATA:
		if (arg32 == NULL) {
			err = -EINVAL;
			break;
		}

		err =
		    file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_RAW_DATA,
					       (unsigned long)arg32);
		if (err) {
			GSE_ERR("GSENSOR_IOCTL_READ_RAW_DATA unlocked_ioctl failed.");
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
static const struct file_operations mma8653_fops = {
	.owner = THIS_MODULE,
	.open = mma8653_open,
	.release = mma8653_release,
	.unlocked_ioctl = mma8653_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mma8653_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice mma8653_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &mma8653_fops,
};

/*----------------------------------------------------------------------------*/
#ifndef CONFIG_USE_EARLY_SUSPEND
/*----------------------------------------------------------------------------*/
static int mma8653_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_FUN();
	mutex_lock(&mma8653_op_mutex);
	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GSE_ERR("null pointer!!\n");
			mutex_unlock(&mma8653_op_mutex);
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		if ((err = MMA8653_SetPowerMode(obj->client, false))) {
			GSE_ERR("write power control fail!!\n");
			mutex_unlock(&mma8653_op_mutex);
			return -EINVAL;
		}
		MMA8653_power(obj->hw, 0);
	}
	mutex_unlock(&mma8653_op_mutex);
	return err;
}

/*----------------------------------------------------------------------------*/
static int mma8653_resume(struct i2c_client *client)
{
	struct mma8653_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	mutex_lock(&mma8653_op_mutex);
	MMA8653_power(obj->hw, 1);

	if ((err = MMA8653_Init(client, 0))) {
		GSE_ERR("initialize client fail!!\n");
		mutex_unlock(&mma8653_op_mutex);
		return err;
	}
	atomic_set(&obj->suspend, 0);
	mutex_unlock(&mma8653_op_mutex);
	return 0;
}

/*----------------------------------------------------------------------------*/
#else				/*CONFIG_HAS_EARLY_SUSPEND is defined */
/*----------------------------------------------------------------------------*/
static void mma8653_early_suspend(struct early_suspend *h)
{
	struct mma8653_i2c_data *obj = container_of(h, struct mma8653_i2c_data, early_drv);
	int err = 0;
	u8 dat = 0;

	GSE_FUN();
	mutex_lock(&mma8653_op_mutex);
	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GSE_ERR("null pointer!!\n");
			mutex_unlock(&mma8653_op_mutex);
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		if ((err = MMA8653_SetPowerMode(obj->client, false))) {
			GSE_ERR("write power control fail!!\n");
			mutex_unlock(&mma8653_op_mutex);
			return -EINVAL;
		}
		MMA8653_power(obj->hw, 0);
	}
	sensor_suspend = 1;
	mutex_unlock(&mma8653_op_mutex);
}

/*----------------------------------------------------------------------------*/
static void mma8653_late_resume(struct early_suspend *h)
{
	struct mma8653_i2c_data *obj = container_of(h, struct mma8653_i2c_data, early_drv);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	mutex_lock(&mma8653_op_mutex);
	MMA8653_power(obj->hw, 1);

	if ((err = MMA8653_Init(client, 0))) {
		GSE_ERR("initialize client fail!!\n");
		mutex_unlock(&mma8653_op_mutex);
		return err;
	}
	sensor_suspend = 0;
	atomic_set(&obj->suspend, 0);
	mutex_unlock(&mma8653_op_mutex);
}

/*----------------------------------------------------------------------------*/
#endif				/*CONFIG_HAS_EARLYSUSPEND */
/*----------------------------------------------------------------------------*/
static int mma8653_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, MMA8653_DEV_NAME);
	return 0;
}

static int gsensor_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}
static int gsensor_enable_nodata(int en)
{
	int err = 0;

	if (((en == 0) && (sensor_power == false)) || ((en == 1) && (sensor_power == true))) {
		enable_status = sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		mutex_lock(&mma8653_op_mutex);
		enable_status = !sensor_power;

		err = MMA8653_SetPowerMode(obj_i2c_data->client, enable_status);
		if (atomic_read(&obj_i2c_data->suspend) == 0)
			GSE_LOG("MMA8653 resume to suspend!, enable_status = %d\n",enable_status);
		else
			GSE_LOG("MMA8653 suspend to resume!, enable_status = %d\n",enable_status);
		mutex_unlock(&mma8653_op_mutex);
	}

	if (err) {
		GSE_ERR("gsensor_enable_nodata fail!\n");
		return -1;
	}

	GSE_ERR("gsensor_enable_nodata OK!\n");
	return 0;
}
static int gsensor_set_delay(u64 ns)
{
	int err = 0;
	int value;
	/*int sample_delay;*/

	value = (int)ns/1000/1000;
/*
	if (value <= 5) {
		sample_delay = MMA8653_BW_200HZ;
	} else if (value <= 10) {
		sample_delay = MMA8653_BW_100HZ;
	} else {
		sample_delay = MMA8653_BW_50HZ;
	}
*/
	mutex_lock(&mma8653_op_mutex);
	err = MMA8653_SetBWRate(obj_i2c_data->client, MMA8653_BW_100HZ);
	if (err != MMA8653_SUCCESS)
		GSE_ERR("Set delay parameter error!\n");
	mutex_unlock(&mma8653_op_mutex);

	if (value >= 50) {
		atomic_set(&obj_i2c_data->filter, 0);
	} else {
		obj_i2c_data->fir.num = 0;
		obj_i2c_data->fir.idx = 0;
		obj_i2c_data->fir.sum[MMA8653_AXIS_X] = 0;
		obj_i2c_data->fir.sum[MMA8653_AXIS_Y] = 0;
		obj_i2c_data->fir.sum[MMA8653_AXIS_Z] = 0;
		atomic_set(&obj_i2c_data->filter, 1);
	}

	return err;
}
static int gsensor_get_data(int* x ,int* y,int* z, int* status)
{
	char buff[MMA8653_BUFSIZE];
	int ret = 0;

	mutex_lock(&mma8653_op_mutex);
	MMA8653_ReadSensorData(obj_i2c_data->client, buff, MMA8653_BUFSIZE);
	mutex_unlock(&mma8653_op_mutex);
	ret = sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	/* GSE_LOG("X :%d,Y: %d, Z: %d\n",x,y,z); */
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mma8653_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mma8653_i2c_data *obj;
	//struct hwmsen_object sobj;
  struct acc_control_path ctl={0};
  struct acc_data_path data={0};
	int err = 0;
	int retry = 0;

	GSE_FUN();

	if (!(obj = kzalloc(sizeof(*obj), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct mma8653_i2c_data));

	obj->hw = hw;

	if ((err = hwmsen_get_convert(obj->hw->direction, &obj->cvt))) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

#ifdef CONFIG_MMA8653_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH) {
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	} else {
		atomic_set(&obj->firlen, obj->hw->firlen);
	}

	if (atomic_read(&obj->firlen) > 0) {
		atomic_set(&obj->fir_en, 1);
	}
#endif

	mma8653_i2c_client = new_client;

	for (retry = 0; retry < 3; retry++) {
		if ((err = MMA8653_Init(new_client, 1))) {
			GSE_ERR("MMA8653_device init cilent fail time: %d\n", retry);
			continue;
		}
		break;
	}

	if (err != 0)
		goto exit_init_failed;


	if ((err = misc_register(&mma8653_device))) {
		GSE_ERR("mma8653_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if ((err = mma8653_create_attr(&(mma8653_init_info.platform_diver_addr->driver)))) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
/*
	sobj.self = obj;
	sobj.polling = 1;
	sobj.sensor_operate = mma8653_operate;
	if ((err = hwmsen_attach(ID_ACCELEROMETER, &sobj))) {
		GSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
*/
	ctl.open_report_data = gsensor_open_report_data;
	ctl.enable_nodata = gsensor_enable_nodata;
	ctl.set_delay  = gsensor_set_delay;
	//ctl.batch = gsensor_set_batch;
	ctl.is_report_input_direct = false;

#ifdef CUSTOM_KERNEL_SENSORHUB
  ctl.is_support_batch = obj->hw->is_batch_supported;
#else
  ctl.is_support_batch = false;
#endif

  err = acc_register_control_path(&ctl);
  if(err) {
      GSE_ERR("register acc control path err\n");
      goto exit_create_attr_failed;
  }

  data.get_data = gsensor_get_data;
  data.vender_div = 1000;
  err = acc_register_data_path(&data);
  if(err) {
      GSE_ERR("register acc data path err\n");
      goto exit_create_attr_failed;
  }

  err = batch_register_support_info(ID_ACCELEROMETER,ctl.is_support_batch, 102, 0); //divisor is 1000/9.8
  if(err) {
    GSE_ERR("register gsensor batch support err = %d\n", err);
     goto exit_create_attr_failed;
   }
#ifdef CONFIG_USE_EARLY_SUSPEND
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2;
	obj->early_drv.suspend = mma8653_early_suspend;
	obj->early_drv.resume = mma8653_late_resume;
	register_early_suspend(&obj->early_drv);
#endif

	GSE_LOG("%s: OK\n", __func__);
	mma8653_init_flag = 0;
	return 0;

exit_create_attr_failed:
	misc_deregister(&mma8653_device);
exit_misc_device_register_failed:
exit_init_failed:
	kfree(obj);
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	mma8653_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int mma8653_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	if ((err = mma8653_delete_attr(&(mma8653_init_info.platform_diver_addr->driver)))) {
		GSE_ERR("mma8653_delete_attr fail: %d\n", err);
	}

	if ((err = misc_deregister(&mma8653_device))) {
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if ((err = hwmsen_detach(ID_ACCELEROMETER)))


		mma8653_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int mma8653_remove(void)
{
	GSE_FUN();
	MMA8653_power(hw, 0);
	i2c_del_driver(&mma8653_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

static int mma8653_local_init(void)
{
	GSE_FUN();

	MMA8653_power(hw, 1);
	if (i2c_add_driver(&mma8653_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == mma8653_init_flag) {
		return -1;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int __init mma8653_init(void)
{
	GSE_FUN();

	hw = get_accel_dts_func(COMPATIABLE_NAME, hw);
	if (!hw)
		GSE_ERR("get dts info fail\n");
	GSE_LOG("%s: i2c_number=%d\n", __func__, hw->i2c_num);
	acc_driver_add(&mma8653_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit mma8653_exit(void)
{
	GSE_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(mma8653_init);
module_exit(mma8653_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MMA8653 I2C driver");
MODULE_AUTHOR("Chunlei.Wang@mediatek.com");

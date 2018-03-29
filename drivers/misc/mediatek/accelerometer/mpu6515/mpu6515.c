/* MPU6515 motion sensor driver
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
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include "mpu6515.h"
#include "mpu6515g.h"
#include "mpu6515a.h"

#include <accel.h>
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

/* #define POWER_NONE_MACRO MT65XX_POWER_NONE */

/*----------------------------------------------------------------------------*/
#define DEBUG 1
/* #define GSENSOR_UT */
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU6515_LOWPASS	/*apply low pass filter on output */
#define SW_CALIBRATION
/* #define USE_EARLY_SUSPEND */
/*----------------------------------------------------------------------------*/
#define MPU6515_AXIS_X          0
#define MPU6515_AXIS_Y          1
#define MPU6515_AXIS_Z          2
#define MPU6515_AXES_NUM        3
#define MPU6515_DATA_LEN        6
#define MPU6515_DUMP_LEN        128
#define MPU6515_DEV_NAME        "MPU6515G"	/* name must different with gyro mpu6515 */
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu6515_i2c_id[] = { {MPU6515_DEV_NAME, 0}, {} };

#ifdef CONFIG_MTK_LEGACY
static struct i2c_board_info i2c_mpu6515 __initdata = {
	I2C_BOARD_INFO(MPU6515_DEV_NAME, (MPU6515_I2C_SLAVE_ADDR >> 1))
};
#endif

/*----------------------------------------------------------------------------*/
static int mpu6515_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mpu6515_i2c_remove(struct i2c_client *client);
static int mpu6515_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int mpu6515_suspend(struct i2c_client *client, pm_message_t msg);
static int mpu6515_resume(struct i2c_client *client);
#endif

static int gsensor_local_init(void);
static int gsensor_remove(void);

#ifdef CUSTOM_KERNEL_SENSORHUB
static int gsensor_setup_irq(void);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
static int gsensor_set_delay(u64 ns);

/* Maintain  cust info here */
struct acc_hw accel_cust;
static struct acc_hw *hw = &accel_cust;

/* For  driver get cust info */
struct acc_hw *get_cust_acc(void)
{
	return &accel_cust;
}

/*----------------------------------------------------------------------------*/
typedef enum {
	MPU6515_TRC_FILTER = 0x01,
	MPU6515_TRC_RAWDATA = 0x02,
	MPU6515_TRC_IOCTL = 0x04,
	MPU6515_TRC_CALI = 0X08,
	MPU6515_TRC_INFO = 0X10,
	MPU6515_TRC_I2C = 0X20,
	MPU6515_TRC_DUMP = 0X1000,
} MPU6515_TRC;
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
	s16 raw[C_MAX_FIR_LENGTH][MPU6515_AXES_NUM];
	int sum[MPU6515_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct mpu6515_i2c_data {
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
	s16 cali_sw[MPU6515_AXES_NUM + 1];

	/*data */
	s8 offset[MPU6515_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[MPU6515_AXES_NUM + 1];

#ifdef CUSTOM_KERNEL_SENSORHUB
	int SCP_init_done;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

#if defined(CONFIG_MPU6515_LOWPASS)
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
static const struct of_device_id acc_of_match[] = {
	{.compatible = "mediatek,GSENSOR"},
	{},
};
#endif

static struct i2c_driver mpu6515_i2c_driver = {
	.driver = {
		   .name = MPU6515_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = acc_of_match,
#endif
		   },
	.probe = mpu6515_i2c_probe,
	.remove = mpu6515_i2c_remove,
	.detect = mpu6515_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend = mpu6515_suspend,
	.resume = mpu6515_resume,
#endif
	.id_table = mpu6515_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mpu6515_i2c_client;
static struct mpu6515_i2c_data *obj_i2c_data;
static bool sensor_power;
/* static bool scp_sensor_power = false; */
static struct GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8] = { 0 };

static DEFINE_MUTEX(gsensor_mutex);
static DEFINE_MUTEX(gsensor_scp_en_mutex);
static bool enable_status;

int gsensor_init_flag = -1;	/* 0<==>OK -1 <==> fail */

static struct acc_init_info mpu6515_init_info = {
	.name = "mpu6515",
	.init = gsensor_local_init,
	.uninit = gsensor_remove,
};

/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               pr_debug(GSE_TAG"%s\n", __func__)
#define GSE_ERR(fmt, args...)    pr_err(GSE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    pr_debug(GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution mpu6515_data_resolution[] = {
	/*8 combination by {FULL_RES,RANGE} */
	{{0, 6}, 16384},	/*+/-2g  in 16-bit resolution:  0.06 mg/LSB */
	{{0, 12}, 8192},	/*+/-4g  in 16-bit resolution:  0.12 mg/LSB */
	{{0, 24}, 4096},	/*+/-8g  in 16-bit resolution:  0.24 mg/LSB */
	{{0, 49}, 2048},	/*+/-16g in 16-bit resolution:  0.49 mg/LSB */
};

/*----------------------------------------------------------------------------*/
static struct data_resolution mpu6515_offset_resolution = { {0, 5}, 2048 };

static unsigned int power_on;

/* extern int MPU6515_gyro_power(void); */
/* extern int MPU6515_gyro_mode(void); */


int MPU6515_gse_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(MPU6515_gse_power);

int MPU6515_gse_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(MPU6515_gse_mode);


int MPU6515_i2c_master_send(u8 *buf, u8 len)
{
#ifndef GSENSOR_UT
	int res = 0;

	if (NULL == mpu6515_i2c_client)
		GSE_ERR("MPU6515_i2c_master_send null ptr!!\n");
	else
		res = i2c_master_send(mpu6515_i2c_client, buf, len);


	return res;
#else
	return 1;
#endif
}
EXPORT_SYMBOL(MPU6515_i2c_master_send);

int MPU6515_i2c_master_recv(u8 *buf, u8 len)
{
#ifndef GSENSOR_UT
	int res = 0;

	if (NULL == mpu6515_i2c_client)
		GSE_ERR("MPU6515_i2c_master_recv null ptr!!\n");
	else
		res = i2c_master_recv(mpu6515_i2c_client, buf, len);


	return res;
#else
	return 1;
#endif
}
EXPORT_SYMBOL(MPU6515_i2c_master_recv);

#ifdef CUSTOM_KERNEL_SENSORHUB
int MPU6515_SCP_SetPowerMode(bool enable, int sensorType)
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

	if (true == enable)
		gsensor_scp_en_map |= (1 << sensorType);
	else
		gsensor_scp_en_map &= ~(1 << sensorType);


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
		if (err)
			GSE_ERR("SCP_sensorHub_req_send fail!\n");
	}

	mutex_unlock(&gsensor_scp_en_mutex);

	return err;
}
EXPORT_SYMBOL(MPU6515_SCP_SetPowerMode);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
/*----------------------------------------------------------------------------*/
static int mpu_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

	data[0] = addr;

	msg.addr = client->addr;
	msg.addr &= I2C_MASK_FLAG;
	msg.addr |= (I2C_WR_FLAG | I2C_RS_FLAG);
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = ((len << 8) | 0x1);
	msg.buf = (char *)data;
#ifdef CONFIG_MTK_I2C_EXTENSION
	/* msg.ext_flag = I2C_WR_FLAG | I2C_RS_FLAG; */
	msg.timing = client->timing;
	msg.ext_flag = client->ext_flag;
#endif
	if (atomic_read(&obj->trace) & MPU6515_TRC_I2C)
		GSE_LOG("i2c msg: (%x %x %x %x %x)\n", msg.addr, msg.flags, msg.len, msg.timing, msg.ext_flag);

	err = i2c_transfer(adap, &msg, 1);

	if (err < 0)
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
	else
		err = 0;

	return err;
}

int MPU6515_hwmsen_read_block(u8 addr, u8 *buf, u8 len)
{
#ifndef GSENSOR_UT
	if (NULL == mpu6515_i2c_client) {
		GSE_ERR("MPU6515_hwmsen_read_block null ptr!!\n");
		return MPU6515_ERR_I2C;
	}
	return mpu_i2c_read_block(mpu6515_i2c_client, addr, buf, len);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(MPU6515_hwmsen_read_block);

int MPU6515_hwmsen_read_byte(u8 addr, u8 *buf)
{
#ifndef GSENSOR_UT
	if (NULL == mpu6515_i2c_client) {
		GSE_ERR("MPU6515_hwmsen_read_byte null ptr!!\n");
		return MPU6515_ERR_I2C;
	}
	return mpu_i2c_read_block(mpu6515_i2c_client, addr, buf, 1);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(MPU6515_hwmsen_read_byte);
/*--------------------mpu6515 power control function----------------------------------*/
static void MPU6515_power(struct acc_hw *hw, unsigned int on)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	static unsigned int power_on;
#if 0
	if (hw->power_id != POWER_NONE_MACRO) {	/* have externel LDO */
		GSE_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on) {	/* power status not change */
			GSE_LOG("ignore power control: %d\n", on);
		} else if (on) {	/* power on */
			if (!hwPowerOn(hw->power_id, hw->power_vol, "MPU6515G"))
				GSE_ERR("power on fails!!\n");
		} else {	/* power off */
			if (!hwPowerDown(hw->power_id, "MPU6515G"))
				GSE_ERR("power off fail!!\n");
		}
	}
#endif
	power_on = on;
#endif
}

/*----------------------------------------------------------------------------*/
static int MPU6515_SetPowerMode(struct i2c_client *client, bool enable)
{
	int res = 0;
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2];

	if (enable == sensor_power) {
		GSE_LOG("Sensor power status is newest!\n");
		return MPU6515_SUCCESS;
	}
#if 0
	databuf[0] = MPU6515_REG_POWER_CTL;
	res = i2c_master_send(client, databuf, 0x1);
	if (res <= 0)
		return MPU6515_ERR_I2C;


	udelay(500);

	databuf[0] = 0x0;
	res = i2c_master_recv(client, databuf, 1);
	if (res <= 0)
		return MPU6515_ERR_I2C;

#else
	if (hwmsen_read_byte(client, MPU6515_REG_POWER_CTL, databuf)) {
		GSE_ERR("read power ctl register err!\n");
		return MPU6515_ERR_I2C;
	}
#endif

	if ((databuf[0] & 0x1f) != 0x1)
		GSE_ERR("MPU6515 PWR_MGMT_1 = %x\n", databuf[0]);

	databuf[0] &= ~MPU6515_SLEEP;

	if (enable == false) {
		if (MPU6515_gyro_mode() == false)
			databuf[0] |= MPU6515_SLEEP;
	} else {
		/* do nothing */
	}
	databuf[1] = databuf[0];
	databuf[0] = MPU6515_REG_POWER_CTL;

	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0) {
		GSE_LOG("set power mode failed!\n");
		return MPU6515_ERR_I2C;
	}

	if (atomic_read(&obj->trace) & MPU6515_TRC_INFO)
		GSE_LOG("set power mode ok %d!\n", databuf[1]);

	sensor_power = enable;
	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_SetDataResolution(struct mpu6515_i2c_data *obj)
{
	int err;
	u8 dat = 0, reso = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	err = mpu_i2c_read_block(obj->client, MPU6515_REG_DATA_FORMAT, &dat, 1);
	if (err) {
		GSE_ERR("write data format fail!!\n");
		return err;
	}

	/*the data_reso is combined by 3 bits: {FULL_RES, DATA_RANGE} */
	reso = 0x00;
	reso = (dat & MPU6515_RANGE_16G) >> 3;

	if (reso < sizeof(mpu6515_data_resolution) / sizeof(mpu6515_data_resolution[0])) {
		obj->reso = &mpu6515_data_resolution[reso];
		return 0;
	} else {
		return -EINVAL;
	}
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadData(struct i2c_client *client, s16 data[MPU6515_AXES_NUM])
{
	struct mpu6515_i2c_data *priv;
	int err = 0;
	u8 buf[MPU6515_DATA_LEN] = { 0 };
	u8 buf_dump[MPU6515_DUMP_LEN] = { 0 };
	int i;
	static int num = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	if (NULL == client)
		return -EINVAL;

	num %= 100;

	priv = i2c_get_clientdata(client);


	{
		/* write then burst read */
		mpu_i2c_read_block(client, MPU6515_REG_DATAX0, buf, MPU6515_DATA_LEN);
		if ((atomic_read(&priv->trace) & MPU6515_TRC_DUMP) && (num == 0)) {
			GSE_LOG("\n== Dump Info ==\n");
			for(i=0;i<MPU6515_DUMP_LEN;i+=8) {
				mpu_i2c_read_block(client, (0x00+i), (buf_dump+i), 8);
				GSE_LOG("[%02X %02X %02X %02X %02X %02X %02X %02X] => [%02X %02X %02X %02X %02X %02X %02X %02X]\n",
					i, i+1, i+2, i+3, i+4, i+5, i+6, i+7,
					buf_dump[i], buf_dump[i+1], buf_dump[i+2], buf_dump[i+3],
					buf_dump[i+4], buf_dump[i+5], buf_dump[i+6], buf_dump[i+7]);
			}
			GSE_LOG("== End Dump Info ==\n\n");
		}
		num++;

		data[MPU6515_AXIS_X] = (s16) ((buf[MPU6515_AXIS_X * 2] << 8) |
					      (buf[MPU6515_AXIS_X * 2 + 1]));
		data[MPU6515_AXIS_Y] = (s16) ((buf[MPU6515_AXIS_Y * 2] << 8) |
					      (buf[MPU6515_AXIS_Y * 2 + 1]));
		data[MPU6515_AXIS_Z] = (s16) ((buf[MPU6515_AXIS_Z * 2] << 8) |
					      (buf[MPU6515_AXIS_Z * 2 + 1]));

		if (atomic_read(&priv->trace) & MPU6515_TRC_RAWDATA) {
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[MPU6515_AXIS_X],
				data[MPU6515_AXIS_Y], data[MPU6515_AXIS_Z], data[MPU6515_AXIS_X],
				data[MPU6515_AXIS_Y], data[MPU6515_AXIS_Z]);
		}
#ifdef CONFIG_MPU6515_LOWPASS
		if (atomic_read(&priv->filter)) {
			if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
				int idx, firlen = atomic_read(&priv->firlen);

				if (priv->fir.num < firlen) {
					priv->fir.raw[priv->fir.num][MPU6515_AXIS_X] =
					    data[MPU6515_AXIS_X];
					priv->fir.raw[priv->fir.num][MPU6515_AXIS_Y] =
					    data[MPU6515_AXIS_Y];
					priv->fir.raw[priv->fir.num][MPU6515_AXIS_Z] =
					    data[MPU6515_AXIS_Z];
					priv->fir.sum[MPU6515_AXIS_X] += data[MPU6515_AXIS_X];
					priv->fir.sum[MPU6515_AXIS_Y] += data[MPU6515_AXIS_Y];
					priv->fir.sum[MPU6515_AXIS_Z] += data[MPU6515_AXIS_Z];
					if (atomic_read(&priv->trace) & MPU6515_TRC_FILTER) {
						GSE_LOG
						    ("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n",
						     priv->fir.num,
						     priv->fir.raw[priv->fir.num][MPU6515_AXIS_X],
						     priv->fir.raw[priv->fir.num][MPU6515_AXIS_Y],
						     priv->fir.raw[priv->fir.num][MPU6515_AXIS_Z],
						     priv->fir.sum[MPU6515_AXIS_X],
						     priv->fir.sum[MPU6515_AXIS_Y],
						     priv->fir.sum[MPU6515_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				} else {
					idx = priv->fir.idx % firlen;
					priv->fir.sum[MPU6515_AXIS_X] -=
					    priv->fir.raw[idx][MPU6515_AXIS_X];
					priv->fir.sum[MPU6515_AXIS_Y] -=
					    priv->fir.raw[idx][MPU6515_AXIS_Y];
					priv->fir.sum[MPU6515_AXIS_Z] -=
					    priv->fir.raw[idx][MPU6515_AXIS_Z];
					priv->fir.raw[idx][MPU6515_AXIS_X] = data[MPU6515_AXIS_X];
					priv->fir.raw[idx][MPU6515_AXIS_Y] = data[MPU6515_AXIS_Y];
					priv->fir.raw[idx][MPU6515_AXIS_Z] = data[MPU6515_AXIS_Z];
					priv->fir.sum[MPU6515_AXIS_X] += data[MPU6515_AXIS_X];
					priv->fir.sum[MPU6515_AXIS_Y] += data[MPU6515_AXIS_Y];
					priv->fir.sum[MPU6515_AXIS_Z] += data[MPU6515_AXIS_Z];
					priv->fir.idx++;
					data[MPU6515_AXIS_X] =
					    priv->fir.sum[MPU6515_AXIS_X] / firlen;
					data[MPU6515_AXIS_Y] =
					    priv->fir.sum[MPU6515_AXIS_Y] / firlen;
					data[MPU6515_AXIS_Z] =
					    priv->fir.sum[MPU6515_AXIS_Z] / firlen;
					if (atomic_read(&priv->trace) & MPU6515_TRC_FILTER) {
						GSE_LOG
						    ("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n",
						     idx, priv->fir.raw[idx][MPU6515_AXIS_X],
						     priv->fir.raw[idx][MPU6515_AXIS_Y],
						     priv->fir.raw[idx][MPU6515_AXIS_Z],
						     priv->fir.sum[MPU6515_AXIS_X],
						     priv->fir.sum[MPU6515_AXIS_Y],
						     priv->fir.sum[MPU6515_AXIS_Z],
						     data[MPU6515_AXIS_X], data[MPU6515_AXIS_Y],
						     data[MPU6515_AXIS_Z]);
					}
				}
			}
		}
#endif
	}

	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadOffset(struct i2c_client *client, s8 ofs[MPU6515_AXES_NUM])
{
	int err = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#else
	err = mpu_i2c_read_block(client, MPU6515_REG_OFSX, ofs, MPU6515_AXES_NUM);
	if (err)
		GSE_ERR("error: %d\n", err);
#endif
	/* GSE_LOG("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]); */

	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ResetCalibration(struct i2c_client *client)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	MPU6515_CUST_DATA *pCustData;
	unsigned int len;
#endif

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	if (0 != obj->SCP_init_done) {
		pCustData = (MPU6515_CUST_DATA *) &data.set_cust_req.custData;

		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		pCustData->resetCali.action = MPU6515_CUST_ACTION_RESET_CALI;
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
static int MPU6515_ReadCalibration(struct i2c_client *client, int dat[MPU6515_AXES_NUM])
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
	int mul;
#else
	int err;
#endif

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

#ifdef SW_CALIBRATION
	mul = 0;		/* only SW Calibration, disable HW Calibration */
#else

	err = MPU6515_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / mpu6515_offset_resolution.sensitivity;
#endif

	dat[obj->cvt.map[MPU6515_AXIS_X]] =
	    obj->cvt.sign[MPU6515_AXIS_X] * (obj->offset[MPU6515_AXIS_X] * mul +
					     obj->cali_sw[MPU6515_AXIS_X]);
	dat[obj->cvt.map[MPU6515_AXIS_Y]] =
	    obj->cvt.sign[MPU6515_AXIS_Y] * (obj->offset[MPU6515_AXIS_Y] * mul +
					     obj->cali_sw[MPU6515_AXIS_Y]);
	dat[obj->cvt.map[MPU6515_AXIS_Z]] =
	    obj->cvt.sign[MPU6515_AXIS_Z] * (obj->offset[MPU6515_AXIS_Z] * mul +
					     obj->cali_sw[MPU6515_AXIS_Z]);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadCalibrationEx(struct i2c_client *client, int act[MPU6515_AXES_NUM],
				     int raw[MPU6515_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data */
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
	int mul;
#else
	int err;
#endif

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

#ifdef SW_CALIBRATION
	mul = 0;		/* only SW Calibration, disable HW Calibration */
#else

	err = MPU6515_ReadOffset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / mpu6515_offset_resolution.sensitivity;
#endif

	raw[MPU6515_AXIS_X] = obj->offset[MPU6515_AXIS_X] * mul + obj->cali_sw[MPU6515_AXIS_X];
	raw[MPU6515_AXIS_Y] = obj->offset[MPU6515_AXIS_Y] * mul + obj->cali_sw[MPU6515_AXIS_Y];
	raw[MPU6515_AXIS_Z] = obj->offset[MPU6515_AXIS_Z] * mul + obj->cali_sw[MPU6515_AXIS_Z];

	act[obj->cvt.map[MPU6515_AXIS_X]] = obj->cvt.sign[MPU6515_AXIS_X] * raw[MPU6515_AXIS_X];
	act[obj->cvt.map[MPU6515_AXIS_Y]] = obj->cvt.sign[MPU6515_AXIS_Y] * raw[MPU6515_AXIS_Y];
	act[obj->cvt.map[MPU6515_AXIS_Z]] = obj->cvt.sign[MPU6515_AXIS_Z] * raw[MPU6515_AXIS_Z];

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_WriteCalibration(struct i2c_client *client, int dat[MPU6515_AXES_NUM])
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[MPU6515_AXES_NUM], raw[MPU6515_AXES_NUM];
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	MPU6515_CUST_DATA *pCustData;
	unsigned int len;
#endif

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	err = MPU6515_ReadCalibrationEx(client, cali, raw);
	if (err) {		/*offset will be updated in obj->offset */
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		raw[MPU6515_AXIS_X], raw[MPU6515_AXIS_Y], raw[MPU6515_AXIS_Z],
		obj->offset[MPU6515_AXIS_X], obj->offset[MPU6515_AXIS_Y],
		obj->offset[MPU6515_AXIS_Z], obj->cali_sw[MPU6515_AXIS_X],
		obj->cali_sw[MPU6515_AXIS_Y], obj->cali_sw[MPU6515_AXIS_Z]);

#ifdef CUSTOM_KERNEL_SENSORHUB
	pCustData = (MPU6515_CUST_DATA *) data.set_cust_req.custData;
	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData->setCali.action = MPU6515_CUST_ACTION_SET_CALI;
	pCustData->setCali.data[MPU6515_AXIS_X] = dat[MPU6515_AXIS_X];
	pCustData->setCali.data[MPU6515_AXIS_Y] = dat[MPU6515_AXIS_Y];
	pCustData->setCali.data[MPU6515_AXIS_Z] = dat[MPU6515_AXIS_Z];
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);
	SCP_sensorHub_req_send(&data, &len, 1);
#endif

	/*calculate the real offset expected by caller */
	cali[MPU6515_AXIS_X] += dat[MPU6515_AXIS_X];
	cali[MPU6515_AXIS_Y] += dat[MPU6515_AXIS_Y];
	cali[MPU6515_AXIS_Z] += dat[MPU6515_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
		dat[MPU6515_AXIS_X], dat[MPU6515_AXIS_Y], dat[MPU6515_AXIS_Z]);

	obj->cali_sw[MPU6515_AXIS_X] =
	    obj->cvt.sign[MPU6515_AXIS_X] * (cali[obj->cvt.map[MPU6515_AXIS_X]]);
	obj->cali_sw[MPU6515_AXIS_Y] =
	    obj->cvt.sign[MPU6515_AXIS_Y] * (cali[obj->cvt.map[MPU6515_AXIS_Y]]);
	obj->cali_sw[MPU6515_AXIS_Z] =
	    obj->cvt.sign[MPU6515_AXIS_Z] * (cali[obj->cvt.map[MPU6515_AXIS_Z]]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_CheckDeviceID(struct i2c_client *client)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];
	int res = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	memset(databuf, 0, sizeof(u8) * 10);
#if 0
	databuf[0] = MPU6515_REG_DEVID;

	res = i2c_master_send(client, databuf, 0x1);
	if (res <= 0) {
		GSE_ERR("i2c_master_send failed : %d\n", res);
		goto exit_MPU6515_CheckDeviceID;
	}

	udelay(500);

	databuf[0] = 0x0;
	res = i2c_master_recv(client, databuf, 0x01);
	if (res <= 0) {
		GSE_ERR("i2c_master_recv failed : %d\n", res);
		goto exit_MPU6515_CheckDeviceID;
	}
#else
	res = hwmsen_read_byte(client, MPU6515_REG_DEVID, databuf);
	if (res) {
		GSE_ERR("read devid register err! %d\n", res);
		goto exit_MPU6515_CheckDeviceID;
	}
#endif

	if (atomic_read(&obj->trace) & MPU6515_TRC_INFO)
		GSE_LOG("MPU6515_CheckDeviceID 0x%x\n", databuf[0]);

exit_MPU6515_CheckDeviceID:
	if (res)
		return MPU6515_ERR_I2C;

	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;

#ifndef GSENSOR_UT
	memset(databuf, 0, sizeof(u8) * 2);

#if 0
	databuf[0] = MPU6515_REG_DATA_FORMAT;
	res = i2c_master_send(client, databuf, 0x1);
	if (res <= 0)
		return MPU6515_ERR_I2C;

	udelay(500);

	databuf[0] = 0x0;
	res = i2c_master_recv(client, databuf, 0x01);
	if (res <= 0)
		return MPU6515_ERR_I2C;
#endif

	GSE_LOG("MPU6515_REG_DATA_FORMAT, reg[%x] read=0x%x, dataformat=0x%x\n", MPU6515_REG_DATA_FORMAT, databuf[0], dataformat);
	/* write */
	databuf[1] = databuf[0] | dataformat;
	databuf[0] = MPU6515_REG_DATA_FORMAT;
	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0)
		return MPU6515_ERR_I2C;

	return MPU6515_SetDataResolution(obj);
#else
	GSE_LOG("dataformat = %d\n", dataformat);
	obj->reso = &mpu6515_data_resolution[3];
#endif
}

/*----------------------------------------------------------------------------*/
static int MPU6515_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];
	int res = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	if ((obj->bandwidth != bwrate) || (atomic_read(&obj->suspend))) {
		memset(databuf, 0, sizeof(u8) * 10);

#if 0
		/* read */
		databuf[0] = MPU6515_REG_BW_RATE;
		res = i2c_master_send(client, databuf, 0x1);
		if (res <= 0)
			return MPU6515_ERR_I2C;


		udelay(500);

		databuf[0] = 0x0;
		res = i2c_master_recv(client, databuf, 0x01);
		if (res <= 0)
			return MPU6515_ERR_I2C;
#endif

    	GSE_LOG("MPU6515_REG_BW_RATE, reg[%x] read=0x%x, bwrate=0x%x\n", MPU6515_REG_BW_RATE, databuf[0], bwrate);
		/* write */
		databuf[1] = databuf[0] | bwrate;
		databuf[0] = MPU6515_REG_BW_RATE;

		res = i2c_master_send(client, databuf, 0x2);

		if (res <= 0)
			return MPU6515_ERR_I2C;


		obj->bandwidth = bwrate;
	}

	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_Dev_Reset(struct i2c_client *client)
{
#ifndef CUSTOM_KERNEL_SENSORHUB
	u8 databuf[10];
	int res = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	memset(databuf, 0, sizeof(u8) * 10);

#if 0
	/* read */
	databuf[0] = MPU6515_REG_POWER_CTL;
	res = i2c_master_send(client, databuf, 0x1);
	if (res <= 0)
		return MPU6515_ERR_I2C;


	udelay(500);

	databuf[0] = 0x0;
	res = i2c_master_recv(client, databuf, 0x01);
	if (res <= 0)
		return MPU6515_ERR_I2C;
#else
	if (hwmsen_read_byte(client, MPU6515_REG_POWER_CTL, databuf)) {
		GSE_ERR("read power ctl register err!\n");
		return MPU6515_ERR_I2C;
	}
#endif


	if ((databuf[0] & 0x1f) != 0x1)
		GSE_ERR("MPU6515 PWR_MGMT_1 = %x\n", databuf[0]);

	/* write */
	databuf[1] = databuf[0] | MPU6515_DEV_RESET;
	databuf[0] = MPU6515_REG_POWER_CTL;

	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0)
		return MPU6515_ERR_I2C;


	do {
#if 0
		databuf[0] = MPU6515_REG_POWER_CTL;
		res = i2c_master_send(client, databuf, 0x1);

		udelay(500);

		databuf[0] = 0x0;
		res = i2c_master_recv(client, databuf, 0x01);
#else
		res = hwmsen_read_byte(client, MPU6515_REG_POWER_CTL, databuf);
#endif

		GSE_LOG("[Gsensor] check reset bit");

	} while ((databuf[0] & MPU6515_DEV_RESET) != 0);

	msleep(50);
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	return MPU6515_SUCCESS;
}


/*----------------------------------------------------------------------------*/
static int MPU6515_Reset(struct i2c_client *client)
{
#ifndef CUSTOM_KERNEL_SENSORHUB
	u8 databuf[10];
	int res = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	/* write */
	databuf[1] = 0x7;	/* reset gyro, g-sensor, temperature */
	databuf[0] = MPU6515_REG_RESET;

	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0)
		return MPU6515_ERR_I2C;


	msleep(20);
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	return MPU6515_SUCCESS;
}


/*----------------------------------------------------------------------------*/
static int MPU6515_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[2];
	int res = 0;

#ifndef GSENSOR_UT

	memset(databuf, 0, sizeof(u8) * 2);
	databuf[0] = MPU6515_REG_INT_ENABLE;
	databuf[1] = intenable;

	res = i2c_master_send(client, databuf, 0x2);

	if (res <= 0)
		return MPU6515_ERR_I2C;

#else
	GSE_FUN();
#endif

	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_gpio_config(void)
{
/* because we donot use EINT to support low power */
/* config to GPIO input mode + PD */

/* set to GPIO_GSE_1_EINT_PIN */
	/*
	   mt_set_gpio_mode(GPIO_GSE_1_EINT_PIN, GPIO_GSE_1_EINT_PIN_M_GPIO);
	   mt_set_gpio_dir(GPIO_GSE_1_EINT_PIN, GPIO_DIR_IN);
	   mt_set_gpio_pull_enable(GPIO_GSE_1_EINT_PIN, GPIO_PULL_ENABLE);
	   mt_set_gpio_pull_select(GPIO_GSE_1_EINT_PIN, GPIO_PULL_DOWN);
	 */
/* set to GPIO_GSE_2_EINT_PIN */
	/*
	   mt_set_gpio_mode(GPIO_GSE_2_EINT_PIN, GPIO_GSE_2_EINT_PIN_M_GPIO);
	   mt_set_gpio_dir(GPIO_GSE_2_EINT_PIN, GPIO_DIR_IN);
	   mt_set_gpio_pull_enable(GPIO_GSE_2_EINT_PIN, GPIO_PULL_ENABLE);
	   mt_set_gpio_pull_select(GPIO_GSE_2_EINT_PIN, GPIO_PULL_DOWN);
	 */
	return 0;
}

static int mpu6515_init_client(struct i2c_client *client, int reset_cali)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	mpu6515_gpio_config();

	res = MPU6515_SetPowerMode(client, true);
	if (res != MPU6515_SUCCESS) {
		GSE_ERR("set power error\n");
		return res;
	}

	res = MPU6515_CheckDeviceID(client);
	if (res != MPU6515_SUCCESS) {
		GSE_ERR("Check ID error\n");
		return res;
	}
	/* res = gsensor_set_delay(5000000); */
	res = MPU6515_SetBWRate(client, MPU6515_BW_184HZ);
	if (res != MPU6515_SUCCESS) {	/* 0x2C->BW=100Hz */
		GSE_ERR("set BWRate error\n");
		return res;
	}

	res = MPU6515_SetDataFormat(client, MPU6515_RANGE_16G);
	if (res != MPU6515_SUCCESS) {	/* 0x2C->BW=100Hz */
		GSE_ERR("set data format error\n");
		return res;
	}

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

#ifdef CUSTOM_KERNEL_SENSORHUB
	res = gsensor_setup_irq();
	if (res != MPU6515_SUCCESS)
		return res;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	res = MPU6515_SetIntEnable(client, 0x00);	/* disable INT */
	if (res != MPU6515_SUCCESS) {
		GSE_ERR("mpu6515_SetIntEnable error\n");
		return res;
	}

	if (0 != reset_cali) {
		/*reset calibration only in power on */
		res = MPU6515_ResetCalibration(client);
		if (res != MPU6515_SUCCESS)
			return res;
	}

	res = MPU6515_SetPowerMode(client, enable_status);
	if (res != MPU6515_SUCCESS) {
		GSE_ERR("set power error\n");
		return res;
	}
#ifdef CONFIG_MPU6515_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadAllReg(struct i2c_client *client, char *buf, int bufsize)
{
	u8 total_len = 0x5C;	/* (0x75-0x19); */

	u8 addr = 0x19;
	u8 buff[total_len + 1];
	int err = 0;
	int i;


	if (sensor_power == false) {
		err = MPU6515_SetPowerMode(client, true);
		if (err)
			GSE_ERR("Power on mpu6515 error %d!\n", err);

		msleep(50);
	}

	mpu_i2c_read_block(client, addr, buff, total_len);

	for (i = 0; i <= total_len; i++)
		GSE_LOG("MPU6515 reg=0x%x, data=0x%x\n", (addr + i), buff[i]);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	memset(databuf, 0, sizeof(u8) * 10);

	if ((NULL == buf) || (bufsize <= 30))
		return -1;


	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "MPU6515 Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct mpu6515_i2c_data *obj = obj_i2c_data;	/* (struct mpu6515_i2c_data*)i2c_get_clientdata(client); */
	int acc[MPU6515_AXES_NUM];
	int res = 0;

	client = obj->client;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	if (atomic_read(&obj->suspend))
		return -3;


	if (NULL == buf)
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	res = MPU6515_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}

	obj->data[MPU6515_AXIS_X] += obj->cali_sw[MPU6515_AXIS_X];
	obj->data[MPU6515_AXIS_Y] += obj->cali_sw[MPU6515_AXIS_Y];
	obj->data[MPU6515_AXIS_Z] += obj->cali_sw[MPU6515_AXIS_Z];

	/*remap coordinate */
	acc[obj->cvt.map[MPU6515_AXIS_X]] =
	    obj->cvt.sign[MPU6515_AXIS_X] * obj->data[MPU6515_AXIS_X];
	acc[obj->cvt.map[MPU6515_AXIS_Y]] =
	    obj->cvt.sign[MPU6515_AXIS_Y] * obj->data[MPU6515_AXIS_Y];
	acc[obj->cvt.map[MPU6515_AXIS_Z]] =
	    obj->cvt.sign[MPU6515_AXIS_Z] * obj->data[MPU6515_AXIS_Z];

	/* Out put the mg */
	acc[MPU6515_AXIS_X] = acc[MPU6515_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	acc[MPU6515_AXIS_Y] = acc[MPU6515_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	acc[MPU6515_AXIS_Z] = acc[MPU6515_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;

	sprintf(buf, "%04x %04x %04x", acc[MPU6515_AXIS_X], acc[MPU6515_AXIS_Y],
		acc[MPU6515_AXIS_Z]);
	if (atomic_read(&obj->trace) & MPU6515_TRC_IOCTL)
		GSE_LOG("gsensor data: %s!\n", buf);


	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadRawData(struct i2c_client *client, char *buf)
{
	struct mpu6515_i2c_data *obj;
	int res = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	if (!buf || !client)
		return -EINVAL;

	obj = (struct mpu6515_i2c_data *)i2c_get_clientdata(client);


	if (atomic_read(&obj->suspend))
		return -EIO;


	res = MPU6515_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -EIO;
	} else {
		sprintf(buf, "%04x %04x %04x", obj->data[MPU6515_AXIS_X],
			obj->data[MPU6515_AXIS_Y], obj->data[MPU6515_AXIS_Z]);
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
	u8 data;

	res = MPU6515_SetBWRate(client, MPU6515_BW_184HZ);
	if (res != MPU6515_SUCCESS)	/* 0x2C->BW=100Hz */
		return res;

	res = mpu_i2c_read_block(client, MPU6515_REG_DATA_FORMAT, &data, 1);

	if (res != MPU6515_SUCCESS)
		return res;


	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_JudgeTestResult(struct i2c_client *client, s32 prv[MPU6515_AXES_NUM],
				   s32 nxt[MPU6515_AXES_NUM])
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

	res = mpu_i2c_read_block(client, MPU6515_REG_DATA_FORMAT, &format, 1);
	if (res)
		return res;

	format = format & MPU6515_RANGE_16G;

	switch (format) {
	case MPU6515_RANGE_2G:
		GSE_LOG("format use self[0]\n");
		ptr = &self[0];
		break;

	case MPU6515_RANGE_4G:
		GSE_LOG("format use self[1]\n");
		ptr = &self[1];
		break;

	case MPU6515_RANGE_8G:
		GSE_LOG("format use self[2]\n");
		ptr = &self[2];
		break;

	case MPU6515_RANGE_16G:
		GSE_LOG("format use self[3]\n");
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

	GSE_LOG("X diff is %ld\n", abs(nxt[MPU6515_AXIS_X] - prv[MPU6515_AXIS_X]));
	GSE_LOG("Y diff is %ld\n", abs(nxt[MPU6515_AXIS_Y] - prv[MPU6515_AXIS_Y]));
	GSE_LOG("Z diff is %ld\n", abs(nxt[MPU6515_AXIS_Z] - prv[MPU6515_AXIS_Z]));


	if ((abs(nxt[MPU6515_AXIS_X] - prv[MPU6515_AXIS_X]) > (*ptr)[MPU6515_AXIS_X].max) ||
	    (abs(nxt[MPU6515_AXIS_X] - prv[MPU6515_AXIS_X]) < (*ptr)[MPU6515_AXIS_X].min)) {
		GSE_ERR("X is over range\n");
		res = -EINVAL;
	}
	if ((abs(nxt[MPU6515_AXIS_Y] - prv[MPU6515_AXIS_Y]) > (*ptr)[MPU6515_AXIS_Y].max) ||
	    (abs(nxt[MPU6515_AXIS_Y] - prv[MPU6515_AXIS_Y]) < (*ptr)[MPU6515_AXIS_Y].min)) {
		GSE_ERR("Y is over range\n");
		res = -EINVAL;
	}
	if ((abs(nxt[MPU6515_AXIS_Z] - prv[MPU6515_AXIS_Z]) > (*ptr)[MPU6515_AXIS_Z].max) ||
	    (abs(nxt[MPU6515_AXIS_Z] - prv[MPU6515_AXIS_Z]) < (*ptr)[MPU6515_AXIS_Z].min)) {
		GSE_ERR("Z is over range\n");
		res = -EINVAL;
	}
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6515_i2c_client;
	/* char strbuf[MPU6515_BUFSIZE]; */
	char *strbuf;
	int ret;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	if (sensor_power == false) {
		MPU6515_SetPowerMode(client, true);
		msleep(50);
	}

	strbuf = kmalloc(MPU6515_BUFSIZE, GFP_KERNEL);
	if (!strbuf) {
		GSE_ERR("strbuf is null!!\n");
		return 0;
	}

	MPU6515_ReadAllReg(client, strbuf, MPU6515_BUFSIZE);

	MPU6515_ReadChipInfo(client, strbuf, MPU6515_BUFSIZE);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
	kfree(strbuf);

	return ret;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6515_i2c_client;
	char strbuf[MPU6515_BUFSIZE];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	MPU6515_ReadSensorData(client, strbuf, MPU6515_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6515_i2c_client;
	struct mpu6515_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[MPU6515_AXES_NUM];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);


	err = MPU6515_ReadOffset(client, obj->offset);
	if (err)
		return -EINVAL;
	err = MPU6515_ReadCalibration(client, tmp);
	if (err)
		return -EINVAL;

	mul = obj->reso->sensitivity / mpu6515_offset_resolution.sensitivity;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,
		     obj->offset[MPU6515_AXIS_X], obj->offset[MPU6515_AXIS_Y],
		     obj->offset[MPU6515_AXIS_Z], obj->offset[MPU6515_AXIS_X],
		     obj->offset[MPU6515_AXIS_Y], obj->offset[MPU6515_AXIS_Z]);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
		     obj->cali_sw[MPU6515_AXIS_X], obj->cali_sw[MPU6515_AXIS_Y],
		     obj->cali_sw[MPU6515_AXIS_Z]);

	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
		     obj->offset[MPU6515_AXIS_X] * mul + obj->cali_sw[MPU6515_AXIS_X],
		     obj->offset[MPU6515_AXIS_Y] * mul + obj->cali_sw[MPU6515_AXIS_Y],
		     obj->offset[MPU6515_AXIS_Z] * mul + obj->cali_sw[MPU6515_AXIS_Z],
		     tmp[MPU6515_AXIS_X], tmp[MPU6515_AXIS_Y], tmp[MPU6515_AXIS_Z]);

	return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = mpu6515_i2c_client;
	int err, x, y, z;
	int dat[MPU6515_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		err = MPU6515_ResetCalibration(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);
	} else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z)) {
		dat[MPU6515_AXIS_X] = x;
		dat[MPU6515_AXIS_Y] = y;
		dat[MPU6515_AXIS_Z] = z;
		err = MPU6515_WriteCalibration(client, dat);
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
	struct i2c_client *client = mpu6515_i2c_client;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	return snprintf(buf, 8, "%s\n", selftestRes);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{				/*write anything to this register will trigger the process */
	struct item {
		s16 raw[MPU6515_AXES_NUM];
	};

	struct i2c_client *client = mpu6515_i2c_client;
	int idx, res, num;
	struct item *prv = NULL, *nxt = NULL;
	s32 avg_prv[MPU6515_AXES_NUM] = { 0, 0, 0 };
	s32 avg_nxt[MPU6515_AXES_NUM] = { 0, 0, 0 };


	if (1 != sscanf(buf, "%d", &num)) {
		GSE_ERR("parse number fail\n");
		return count;
	} else if (num == 0) {
		GSE_ERR("invalid data count\n");
		return count;
	}

	prv = kcalloc(num, sizeof(*prv), GFP_KERNEL);
	nxt = kcalloc(num, sizeof(*nxt), GFP_KERNEL);
	if (!prv || !nxt)
		goto exit;


	GSE_LOG("NORMAL:\n");
	MPU6515_SetPowerMode(client, true);
	msleep(50);

	for (idx = 0; idx < num; idx++) {
		res = MPU6515_ReadData(client, prv[idx].raw);
		if (res) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}

		avg_prv[MPU6515_AXIS_X] += prv[idx].raw[MPU6515_AXIS_X];
		avg_prv[MPU6515_AXIS_Y] += prv[idx].raw[MPU6515_AXIS_Y];
		avg_prv[MPU6515_AXIS_Z] += prv[idx].raw[MPU6515_AXIS_Z];
		GSE_LOG("[%5d %5d %5d]\n", prv[idx].raw[MPU6515_AXIS_X],
			prv[idx].raw[MPU6515_AXIS_Y], prv[idx].raw[MPU6515_AXIS_Z]);
	}

	avg_prv[MPU6515_AXIS_X] /= num;
	avg_prv[MPU6515_AXIS_Y] /= num;
	avg_prv[MPU6515_AXIS_Z] /= num;

	/*initial setting for self test */
	GSE_LOG("SELFTEST:\n");
	for (idx = 0; idx < num; idx++) {
		res = MPU6515_ReadData(client, nxt[idx].raw);
		if (res) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}
		avg_nxt[MPU6515_AXIS_X] += nxt[idx].raw[MPU6515_AXIS_X];
		avg_nxt[MPU6515_AXIS_Y] += nxt[idx].raw[MPU6515_AXIS_Y];
		avg_nxt[MPU6515_AXIS_Z] += nxt[idx].raw[MPU6515_AXIS_Z];
		GSE_LOG("[%5d %5d %5d]\n", nxt[idx].raw[MPU6515_AXIS_X],
			nxt[idx].raw[MPU6515_AXIS_Y], nxt[idx].raw[MPU6515_AXIS_Z]);
	}

	avg_nxt[MPU6515_AXIS_X] /= num;
	avg_nxt[MPU6515_AXIS_Y] /= num;
	avg_nxt[MPU6515_AXIS_Z] /= num;

	GSE_LOG("X: %5d - %5d = %5d\n", avg_nxt[MPU6515_AXIS_X], avg_prv[MPU6515_AXIS_X],
		avg_nxt[MPU6515_AXIS_X] - avg_prv[MPU6515_AXIS_X]);
	GSE_LOG("Y: %5d - %5d = %5d\n", avg_nxt[MPU6515_AXIS_Y], avg_prv[MPU6515_AXIS_Y],
		avg_nxt[MPU6515_AXIS_Y] - avg_prv[MPU6515_AXIS_Y]);
	GSE_LOG("Z: %5d - %5d = %5d\n", avg_nxt[MPU6515_AXIS_Z], avg_prv[MPU6515_AXIS_Z],
		avg_nxt[MPU6515_AXIS_Z] - avg_prv[MPU6515_AXIS_Z]);

	if (!MPU6515_JudgeTestResult(client, avg_prv, avg_nxt)) {
		GSE_LOG("SELFTEST : PASS\n");
		strncpy(selftestRes, "y", sizeof(selftestRes));
	} else {
		GSE_LOG("SELFTEST : FAIL\n");
		strncpy(selftestRes, "n", sizeof(selftestRes));
	}

exit:
	/*restore the setting */
	mpu6515_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6515_i2c_client;
	struct mpu6515_i2c_data *obj;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->selftest));
}

/*----------------------------------------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct mpu6515_i2c_data *obj = obj_i2c_data;
	int tmp;

	if (NULL == obj) {
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}


	if (1 == sscanf(buf, "%d", &tmp)) {
		if (atomic_read(&obj->selftest) && !tmp) {
			/*enable -> disable */
			mpu6515_init_client(obj->client, 0);
		} else if (!atomic_read(&obj->selftest) && tmp) {
			/*disable -> enable */
			MPU6515_InitSelfTest(obj->client);
		}

		GSE_LOG("selftest: %d => %d\n", atomic_read(&obj->selftest), tmp);
		atomic_set(&obj->selftest, tmp);
	} else {
		GSE_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_MPU6515_LOWPASS
	struct i2c_client *client = mpu6515_i2c_client;
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][MPU6515_AXIS_X],
				obj->fir.raw[idx][MPU6515_AXIS_Y],
				obj->fir.raw[idx][MPU6515_AXIS_Z]);
		}

		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[MPU6515_AXIS_X],
			obj->fir.sum[MPU6515_AXIS_Y], obj->fir.sum[MPU6515_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[MPU6515_AXIS_X] / len,
			obj->fir.sum[MPU6515_AXIS_Y] / len, obj->fir.sum[MPU6515_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_MPU6515_LOWPASS
	struct i2c_client *client = mpu6515_i2c_client;
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int firlen;

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
	struct mpu6515_i2c_data *obj = obj_i2c_data;

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
	struct mpu6515_i2c_data *obj = obj_i2c_data;
	int trace;

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
	int err;
	struct mpu6515_i2c_data *obj = obj_i2c_data;
	u8 dat = 0;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	err = mpu_i2c_read_block(obj->client, MPU6515_REG_POWER_CTL, &dat, 1);
	if (err) {
		GSE_ERR("write data format fail!!\n");
		return err;
	}

	if (obj->hw) {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d), %x\n",
				obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id,
				obj->hw->power_vol, dat);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(self, S_IWUSR | S_IRUGO, show_selftest_value, store_selftest_value);
static DRIVER_ATTR(selftest, S_IWUSR | S_IRUGO, show_self_value, store_self_value);
static DRIVER_ATTR(firlen, S_IWUSR | S_IRUGO, show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *mpu6515_attr_list[] = {
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
static int mpu6515_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(mpu6515_attr_list) / sizeof(mpu6515_attr_list[0]));

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, mpu6515_attr_list[idx]);
		if (0 != err) {
			GSE_ERR("driver_create_file (%s) = %d\n", mpu6515_attr_list[idx]->attr.name,
				err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(mpu6515_attr_list) / sizeof(mpu6515_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, mpu6515_attr_list[idx]);


	return err;
}

/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static void gsensor_irq_work(struct work_struct *work)
{
	struct mpu6515_i2c_data *obj = obj_i2c_data;
	struct scp_acc_hw scp_hw;
	MPU6515_CUST_DATA *p_cust_data;
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

	p_cust_data = (MPU6515_CUST_DATA *) data.set_cust_req.custData;
	sizeOfCustData = sizeof(scp_hw);
	max_cust_data_size_per_packet =
	    sizeof(data.set_cust_req.custData) - offsetof(MPU6515_SET_CUST, data);

	for (i = 0; sizeOfCustData > 0; i++) {
		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		p_cust_data->setCust.action = MPU6515_CUST_ACTION_SET_CUST;
		p_cust_data->setCust.part = i;

		if (sizeOfCustData > max_cust_data_size_per_packet)
			len = max_cust_data_size_per_packet;
		else
			len = sizeOfCustData;


		memcpy(p_cust_data->setCust.data, p, len);
		sizeOfCustData -= len;
		p += len;

		len +=
		    offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(MPU6515_SET_CUST,
									       data);
		SCP_sensorHub_req_send(&data, &len, 1);
	}

	p_cust_data = (MPU6515_CUST_DATA *) &data.set_cust_req.custData;

	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	p_cust_data->resetCali.action = MPU6515_CUST_ACTION_RESET_CALI;
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(p_cust_data->resetCali);
	SCP_sensorHub_req_send(&data, &len, 1);

	obj->SCP_init_done = 1;
}

/*----------------------------------------------------------------------------*/
static int gsensor_irq_handler(void *data, uint len)
{
	struct mpu6515_i2c_data *obj = obj_i2c_data;
	SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P) data;

	GSE_FUN();
	GSE_LOG("len = %d, type = %d, action = %d, errCode = %d\n", len, rsp->rsp.sensorType,
		rsp->rsp.action, rsp->rsp.errCode);

	if (!obj)
		return -1;


	switch (rsp->rsp.action) {
	case SENSOR_HUB_NOTIFY:
		switch (rsp->notify_rsp.event) {
		case SCP_INIT_DONE:
			schedule_work(&obj->irq_work);
			/* schedule_delayed_work(&obj->irq_work, HZ); */
			break;
		default:
			GSE_ERR("Error sensor hub notify");
			break;
		}
		break;
	default:
		GSE_ERR("Error sensor hub action");
		break;
	}

	return 0;
}

static int gsensor_setup_irq(void)
{
	int err = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	err = SCP_sensorHub_rsp_registration(ID_ACCELEROMETER, gsensor_irq_handler);

	return err;
}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
/******************************************************************************
 * Function Configuration
******************************************************************************/
static int mpu6515_open(struct inode *inode, struct file *file)
{
	file->private_data = mpu6515_i2c_client;

	if (file->private_data == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int mpu6515_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
static long mpu6515_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct mpu6515_i2c_data *obj = (struct mpu6515_i2c_data *)i2c_get_clientdata(client);
	char strbuf[MPU6515_BUFSIZE];
	void __user *data;
	struct SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3];

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));


	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		mpu6515_init_client(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MPU6515_ReadChipInfo(client, strbuf, MPU6515_BUFSIZE);
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
		mutex_lock(&gsensor_mutex);
		if (sensor_power == false) {
			err = MPU6515_SetPowerMode(client, true);
			if (err)
				GSE_ERR("Power on mpu6515 error %ld!\n", err);

			msleep(50);
		}
		MPU6515_ReadSensorData(client, strbuf, MPU6515_BUFSIZE);
		mutex_unlock(&gsensor_mutex);
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

		if (atomic_read(&obj->suspend)) {
			err = -EINVAL;
		} else {
			MPU6515_ReadRawData(client, strbuf);
			if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
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
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			GSE_ERR("Perform calibration in suspend state!!\n");
			err = -EINVAL;
		} else {
			cali[MPU6515_AXIS_X] =
			    sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[MPU6515_AXIS_Y] =
			    sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[MPU6515_AXIS_Z] =
			    sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			err = MPU6515_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = MPU6515_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MPU6515_ReadCalibration(client, cali);
		if (err)
			break;

		sensor_data.x = cali[MPU6515_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.y = cali[MPU6515_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.z = cali[MPU6515_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
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

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_mpu6515_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	GSE_FUN();

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		GSE_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_GSENSOR_IOCTL_INIT:
	case COMPAT_GSENSOR_IOCTL_READ_CHIPINFO:
	case COMPAT_GSENSOR_IOCTL_READ_GAIN:
	case COMPAT_GSENSOR_IOCTL_READ_RAW_DATA:
	case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
		/* NVRAM will use below ioctl */
	case COMPAT_GSENSOR_IOCTL_SET_CALI:
	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
	case COMPAT_GSENSOR_IOCTL_GET_CALI:{
			GSE_LOG("compat_ion_ioctl : GSENSOR_IOCTL_XXX command is 0x%x\n", cmd);
			return filp->f_op->unlocked_ioctl(filp, cmd,
							  (unsigned long)compat_ptr(arg));
		}
	default:{
			GSE_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
			return -ENOIOCTLCMD;
		}
	}
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations mpu6515_fops = {
	.open = mpu6515_open,
	.release = mpu6515_release,
	.unlocked_ioctl = mpu6515_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_mpu6515_unlocked_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice mpu6515_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &mpu6515_fops,
};

/*----------------------------------------------------------------------------*/
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
/*----------------------------------------------------------------------------*/
static int mpu6515_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & MPU6515_TRC_INFO)
		GSE_FUN();

	if (msg.event == PM_EVENT_SUSPEND) {
		/* mutex_lock(&gsensor_mutex); */
		atomic_set(&obj->suspend, 1);
#ifndef CUSTOM_KERNEL_SENSORHUB
		err = MPU6515_SetPowerMode(obj->client, false);
		if (err)
#else				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
		if (0)		/* (err = MPU6515_SCP_SetPowerMode(false, ID_ACCELEROMETER)))
				   //need not disable g sensor in suspend mode if use sensor hub. */
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
		{
			GSE_ERR("write power control fail!!\n");
			return err;
		}
		/* mutex_unlock(&gsensor_mutex); */
#ifndef CUSTOM_KERNEL_SENSORHUB
		MPU6515_power(obj->hw, 0);
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
		GSE_LOG("mpu6515_suspend ok\n");
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_resume(struct i2c_client *client)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & MPU6515_TRC_INFO)
		GSE_FUN();

#ifndef CUSTOM_KERNEL_SENSORHUB
	MPU6515_power(obj->hw, 1);
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	/* mutex_lock(&gsensor_mutex); */
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = mpu6515_init_client(client, 0);
	if (err)
#else				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	if (0)			/* (err = MPU6515_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER)))
				   //need not disable g sensor in suspend mode if use sensor hub. */
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	{
		GSE_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);
	/* mutex_unlock(&gsensor_mutex); */
	GSE_LOG("mpu6515_resume ok\n");

	return 0;
}

/*----------------------------------------------------------------------------*/
#else				/* #if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND) */
/*----------------------------------------------------------------------------*/
static void mpu6515_early_suspend(struct early_suspend *h)
{
	struct mpu6515_i2c_data *obj = container_of(h, struct mpu6515_i2c_data, early_drv);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}
	/* mutex_lock(&gsensor_mutex); */
	atomic_set(&obj->suspend, 1);
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = MPU6515_SetPowerMode(obj->client, false);
	if (err)
#else				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	err = MPU6515_SCP_SetPowerMode(false, ID_ACCELEROMETER);
	if (err)
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
		{
			GSE_ERR("write power control fail!!\n");
			return;
		}
#ifndef CUSTOM_KERNEL_SENSORHUB
	if (MPU6515_gyro_mode() == false) {
		MPU6515_Dev_Reset(obj->client);
		MPU6515_Reset(obj->client);
		sensor_power = true;
		MPU6515_SetPowerMode(obj->client, false);
	}

	obj->bandwidth = 0;
	/* mutex_unlock(&gsensor_mutex); */

	MPU6515_power(obj->hw, 0);
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
}

/*----------------------------------------------------------------------------*/
static void mpu6515_late_resume(struct early_suspend *h)
{
	struct mpu6515_i2c_data *obj = container_of(h, struct mpu6515_i2c_data, early_drv);
	int err;

	GSE_FUN();

	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}
#ifndef CUSTOM_KERNEL_SENSORHUB
	MPU6515_power(obj->hw, 1);
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	/* mutex_lock(&gsensor_mutex); */
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = mpu6515_init_client(obj->client, 0);
	if (err)
#else				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
	err = MPU6515_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER);
	if (err)
#endif				/* #ifndef CUSTOM_KERNEL_SENSORHUB */
		{
			GSE_ERR("initialize client fail!!\n");
			return;
		}
	atomic_set(&obj->suspend, 0);
	/* mutex_unlock(&gsensor_mutex); */
}

/*----------------------------------------------------------------------------*/
#endif				/* #if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND) */
/*----------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int gsensor_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/*----------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
#ifndef CUSTOM_KERNEL_SENSORHUB
static int gsensor_enable_nodata(int en)
{
	int err = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	mutex_lock(&gsensor_mutex);
	if (((en == 0) && (sensor_power == false)) || ((en == 1) && (sensor_power == true))) {
		enable_status = sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		enable_status = !sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0) {
			err = MPU6515_SetPowerMode(obj_i2c_data->client, enable_status);
			GSE_LOG
			    ("Gsensor not in suspend gsensor_SetPowerMode!, enable_status = %d\n",
			     enable_status);
		} else {
			GSE_LOG
			    ("Gsensor in suspend and can not enable or disable!enable_status = %d\n",
			     enable_status);
		}
	}
	mutex_unlock(&gsensor_mutex);

	if (err != MPU6515_SUCCESS) {
		GSE_ERR("gsensor_enable_nodata fail!\n");
		return -1;
	}

	GSE_LOG("gsensor_enable_nodata OK!!!\n");
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
#ifdef CUSTOM_KERNEL_SENSORHUB
static int scp_gsensor_enable_nodata(int en)
{
	int err = 0;

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	mutex_lock(&gsensor_mutex);
	if (((en == 0) && (scp_sensor_power == false)) || ((en == 1) && (scp_sensor_power == true))) {
		enable_status = scp_sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	} else {
		enable_status = !scp_sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0) {
			err = MPU6515_SCP_SetPowerMode(en, ID_ACCELEROMETER);
			if (0 == err)
				scp_sensor_power = enable_status;

			GSE_LOG
			    ("Gsensor not in suspend gsensor_SetPowerMode!, enable_status = %d\n",
			     scp_sensor_power);
		} else {
			GSE_LOG
			    ("Gsensor in suspend and can not enable or disable!enable_status = %d\n",
			     scp_sensor_power);
		}
	}
	mutex_unlock(&gsensor_mutex);

	if (err != MPU6515_SUCCESS) {
		GSE_ERR("scp_gsensor_enable_nodata fail!\n");
		return -1;
	}

	GSE_LOG("scp_gsensor_enable_nodata OK!!!\n");
	return 0;
}
#endif
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

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

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
	if (value <= 5)
		sample_delay = MPU6515_BW_184HZ;
	else if (value <= 10)
		sample_delay = MPU6515_BW_92HZ;
	else
		sample_delay = MPU6515_BW_41HZ;


	mutex_lock(&gsensor_mutex);
	err = MPU6515_SetBWRate(obj_i2c_data->client, sample_delay);
	mutex_unlock(&gsensor_mutex);
	if (err != MPU6515_SUCCESS) {	/* 0x2C->BW=100Hz */
		GSE_ERR("Set delay parameter error!\n");
		return -1;
	}

	if (value >= 50) {
		atomic_set(&obj_i2c_data->filter, 0);
	} else {
#if defined(CONFIG_MPU6515_LOWPASS)
		obj_i2c_data->fir.num = 0;
		obj_i2c_data->fir.idx = 0;
		obj_i2c_data->fir.sum[MPU6515_AXIS_X] = 0;
		obj_i2c_data->fir.sum[MPU6515_AXIS_Y] = 0;
		obj_i2c_data->fir.sum[MPU6515_AXIS_Z] = 0;
		atomic_set(&obj_i2c_data->filter, 1);
#endif
	}
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	GSE_LOG("gsensor_set_delay (%d)\n", value);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_get_data(int *x, int *y, int *z, int *status)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA req;
	int len;
	int err = 0;
#else
	char buff[MPU6515_BUFSIZE];
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	/* GSE_FUN(); */

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
	/* sscanf(buff, "%x %x %x", req.get_data_rsp.int16_Data[0], req.get_data_rsp.int16_Data[1],
	   req.get_data_rsp.int16_Data[2]); */
	*x = (int)req.get_data_rsp.int16_Data[0] * GRAVITY_EARTH_1000 / 1000;
	*y = (int)req.get_data_rsp.int16_Data[1] * GRAVITY_EARTH_1000 / 1000;
	*z = (int)req.get_data_rsp.int16_Data[2] * GRAVITY_EARTH_1000 / 1000;
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	if (atomic_read(&obj_i2c_data->trace) & MPU6515_TRC_RAWDATA)
		GSE_LOG("x = %d, y = %d, z = %d\n", *x, *y, *z);

#else				/* #ifdef CUSTOM_KERNEL_SENSORHUB */
	mutex_lock(&gsensor_mutex);
	MPU6515_ReadSensorData(obj_i2c_data->client, buff, MPU6515_BUFSIZE);
	mutex_unlock(&gsensor_mutex);
	if (sscanf(buff, "%x %x %x", x, y, z) != 3)
		GSE_ERR("error sscanf\n");

	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strncpy(info->type, MPU6515_DEV_NAME, sizeof(info->type));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mpu6515_i2c_data *obj;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };
	int err = 0;

	GSE_FUN();
	pr_err("mpu6515G i2c probe\n");

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!(obj)) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct mpu6515_i2c_data));

	obj->hw = get_cust_acc();

	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}
#ifdef CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&obj->irq_work, gsensor_irq_work);
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

	obj_i2c_data = obj;
	obj->client = client;
#ifdef CONFIG_FPGA_EARLY_PORTING
	obj->client->timing = 100;
#else
	obj->client->timing = 400;
#endif

	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
#ifdef CUSTOM_KERNEL_SENSORHUB
	obj->SCP_init_done = 0;
#endif				/* #ifdef CUSTOM_KERNEL_SENSORHUB */

#ifdef CONFIG_MPU6515_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw->firlen);


	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);

#endif

	mpu6515_i2c_client = new_client;
	MPU6515_Dev_Reset(new_client);
	MPU6515_Reset(new_client);

	err = mpu6515_init_client(new_client, 1);
	if (err)
		goto exit_init_failed;


	err = misc_register(&mpu6515_device);
	if (err) {
		GSE_ERR("mpu6515_device register failed\n");
		goto exit_misc_device_register_failed;
	}


	err = mpu6515_create_attr(&mpu6515_init_info.platform_diver_addr->driver);
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gsensor_open_report_data;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ctl.enable_nodata = scp_gsensor_enable_nodata;
#else
	ctl.enable_nodata = gsensor_enable_nodata;
#endif
	ctl.set_delay = gsensor_set_delay;
	ctl.is_report_input_direct = false;
#ifdef CUSTOM_KERNEL_SENSORHUB
	ctl.is_support_batch = obj->hw->is_batch_supported;
#else
	ctl.is_support_batch = false;
#endif

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_create_attr_failed;
	}

	data.get_data = gsensor_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err\n");
		goto exit_create_attr_failed;
	}

	err = batch_register_support_info(ID_ACCELEROMETER, ctl.is_support_batch, 102, 0);/* divisor is 1000/9.8 */
	if (err) {
		GSE_ERR("register gsensor batch support err = %d\n", err);
		goto exit_create_attr_failed;
	}
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(USE_EARLY_SUSPEND)
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	    obj->early_drv.suspend = mpu6515_early_suspend,
	    obj->early_drv.resume = mpu6515_late_resume, register_early_suspend(&obj->early_drv);
#endif

	gsensor_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&mpu6515_device);
exit_misc_device_register_failed:
exit_init_failed:
	/* i2c_detach_client(new_client); */
exit_kfree:
	kfree(obj);
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	gsensor_init_flag = -1;
	mpu6515_i2c_client = NULL;
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = mpu6515_delete_attr(&mpu6515_init_info.platform_diver_addr->driver);
	if (err)
		GSE_ERR("mpu6515_delete_attr fail: %d\n", err);

	err = misc_deregister(&mpu6515_device);
	if (err)
		GSE_ERR("misc_deregister fail: %d\n", err);

	mpu6515_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_local_init(void)
{
	struct acc_hw *hw = get_cust_acc();

	GSE_FUN();

	MPU6515_power(hw, 1);
	if (i2c_add_driver(&mpu6515_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == gsensor_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_remove(void)
{
	struct acc_hw *hw = get_cust_acc();

	GSE_FUN();
	MPU6515_power(hw, 0);
	i2c_del_driver(&mpu6515_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init mpu6515gse_init(void)
{
	const char *name = "mediatek,mpu6515a";

	hw = get_accel_dts_func(name, hw);
	if (!hw)
		GSE_ERR("get cust_accel dts info fail\n");

	GSE_LOG("%s: i2c_number=%d\n", __func__, hw->i2c_num);
#ifdef CONFIG_MTK_LEGACY
	i2c_register_board_info(hw->i2c_num, &i2c_mpu6515, 1);
#endif
	acc_driver_add(&mpu6515_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit mpu6515gse_exit(void)
{
	GSE_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(mpu6515gse_init);
module_exit(mpu6515gse_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6515 gse driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");

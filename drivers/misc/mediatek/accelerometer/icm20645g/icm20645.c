/* ICM20645 motion sensor driver
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



#include <cust_acc.h>
#include "icm20645.h"
#include <accel.h>
#include <hwmsensor.h>

static DEFINE_MUTEX(icm20645_i2c_mutex);
/* Maintain  cust info here */
struct acc_hw accel_cust;
static struct acc_hw *hw = &accel_cust;

/* For  driver get cust info */
struct acc_hw *get_cust_acc(void)
{
	return &accel_cust;
}
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_ICM20645_LOWPASS	/*apply low pass filter on output */
#define SW_CALIBRATION
/*----------------------------------------------------------------------------*/
#define ICM20645_AXIS_X          0
#define ICM20645_AXIS_Y          1
#define ICM20645_AXIS_Z          2
#define ICM20645_AXES_NUM        3
#define ICM20645_DATA_LEN        6
#define ICM20645_DEV_NAME        "ICM20645G"	/* name must different with gyro icm20645 */
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id icm20645_i2c_id[] = { {ICM20645_DEV_NAME, 0}, {} };

/*----------------------------------------------------------------------------*/
static int icm20645_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int icm20645_i2c_remove(struct i2c_client *client);
static int icm20645_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#ifndef USE_EARLY_SUSPEND
static int icm20645_suspend(struct i2c_client *client, pm_message_t msg);
static int icm20645_resume(struct i2c_client *client);
#endif

static int icm20645_local_init(void);
static int icm20645_remove(void);
static int icm20645_init_flag = -1;

static struct acc_init_info icm20645_init_info = {
	.name = "icm20645g",
	.init = icm20645_local_init,
	.uninit = icm20645_remove,
};

/*----------------------------------------------------------------------------*/
typedef enum {
	ICM20645_TRC_FILTER = 0x01,
	ICM20645_TRC_RAWDATA = 0x02,
	ICM20645_TRC_IOCTL = 0x04,
	ICM20645_TRC_CALI = 0X08,
	ICM20645_TRC_INFO = 0X10,
} ICM20645_TRC;
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
	s16 raw[C_MAX_FIR_LENGTH][ICM20645_AXES_NUM];
	int sum[ICM20645_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct icm20645_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;

	/*misc */
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[ICM20645_AXES_NUM + 1];

	/*data */
	s8 offset[ICM20645_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s16 data[ICM20645_AXES_NUM + 1];

#if defined(CONFIG_ICM20645_LOWPASS)
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

static struct i2c_driver icm20645_i2c_driver = {
	.probe = icm20645_i2c_probe,
	.remove = icm20645_i2c_remove,
	.detect = icm20645_i2c_detect,
#if !defined(USE_EARLY_SUSPEND)
	.suspend = icm20645_suspend,
	.resume = icm20645_resume,
#endif
	.id_table = icm20645_i2c_id,
	.driver = {
		.name = ICM20645_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = accel_of_match,
#endif
	},
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *icm20645_i2c_client;
static struct icm20645_i2c_data *obj_i2c_data;
static bool sensor_power;
static struct GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8] = { 0 };

/*----------------------------------------------------------------------------*/
#define GSE_TAG						"[Gsensor] "
#define GSE_ERR(fmt, args...)		pr_err(GSE_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define GSE_LOGLEVEL 0

#if ((GSE_LOGLEVEL) >= 0)
#define GSE_DBG(fmt, args...)		pr_debug(GSE_TAG fmt, ##args)
#else
#define GSE_DBG(fmt, args...)
#endif

#if ((GSE_LOGLEVEL) >= 1)
#define GSE_FUN(f)					pr_debug(GSE_TAG"%s\n", __func__)
#else
#define GSE_FUN(f)
#endif


#if ((GSE_LOGLEVEL) >= 2)
#define GSE_LOG(fmt, args...)		pr_debug(GSE_TAG fmt, ##args)
#else
#define GSE_LOG(fmt, args...)
#endif

/*----------------------------------------------------------------------------*/
static struct data_resolution icm20645_data_resolution[] = {
	/*8 combination by {FULL_RES,RANGE} */
	{{0, 6}, 16384},	/*+/-2g  in 16-bit resolution:  0.06 mg/LSB */
	{{0, 12}, 8192},	/*+/-4g  in 16-bit resolution:  0.12 mg/LSB */
	{{0, 24}, 4096},	/*+/-8g  in 16-bit resolution:  0.24 mg/LSB */
	{{0, 5}, 2048},		/*+/-16g in 16-bit resolution:  0.49 mg/LSB */
};

/*----------------------------------------------------------------------------*/
static struct data_resolution icm20645_offset_resolution = { {0, 5}, 2048 };

static unsigned int power_on;

int ICM20645_gse_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(ICM20645_gse_power);

int ICM20645_gse_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(ICM20645_gse_mode);

/*----------------------------------------------------------------------------*/
static int mpu_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	int err = 0;
	u8 beg = addr;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&icm20645_i2c_mutex);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&icm20645_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&icm20645_i2c_mutex);
		GSE_ERR("length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else
		err = 0;
	mutex_unlock(&icm20645_i2c_mutex);
	return err;

}

static int mpu_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{				/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err = 0, idx = 0, num = 0;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&icm20645_i2c_mutex);
	if (!client) {
		mutex_unlock(&icm20645_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		mutex_unlock(&icm20645_i2c_mutex);
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		mutex_unlock(&icm20645_i2c_mutex);
		GSE_ERR("send command error!!\n");
		return -EFAULT;
	}
	mutex_unlock(&icm20645_i2c_mutex);
	return err;
}

int ICM20645_hwmsen_read_block(u8 addr, u8 *buf, u8 len)
{
	if (NULL == icm20645_i2c_client) {
		GSE_ERR("ICM20645_hwmsen_read_block null ptr!!\n");
		return ICM20645_ERR_I2C;
	}
	return mpu_i2c_read_block(icm20645_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(ICM20645_hwmsen_read_block);

int ICM20645_hwmsen_write_block(u8 addr, u8 *buf, u8 len)
{
	if (NULL == icm20645_i2c_client) {
		GSE_ERR("ICM20645_hwmsen_read_block null ptr!!\n");
		return ICM20645_ERR_I2C;
	}
	return mpu_i2c_write_block(icm20645_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(ICM20645_hwmsen_write_block);

static int icm20645_set_bank(struct i2c_client *client, u8 bank)
{
	int res = 0;
	u8 databuf[2];

	databuf[0] = bank;
	res = mpu_i2c_write_block(client, REG_BANK_SEL, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("icm20645_set_bank fail at %x", bank);
		return ICM20645_ERR_I2C;
	}

	return ICM20645_SUCCESS;
}

static int icm20645_turn_on(struct i2c_client *client, u8 status, bool on)
{
	int res = 0;
	u8 databuf[2];

	memset(databuf, 0, sizeof(databuf));
	GSE_DBG("%s: ENTER\n", __func__);
	icm20645_set_bank(client, BANK_SEL_0);
	res = mpu_i2c_read_block(client, ICM20645_REG_POWER_CTL2, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("icm20645_turn_on fail at %x", on);
		return ICM20645_ERR_I2C;
	}
	if (on == true) {
		databuf[0] &= ~status;
		res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL2, databuf, 0x1);
		if (res < 0) {
			GSE_ERR("icm20645_turn_on fail at %x", on);
			return ICM20645_ERR_I2C;
		}
	} else {
		databuf[0] |= status;
		res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL2, databuf, 0x1);
		if (res < 0) {
			GSE_ERR("icm20645_turn_on fail at %x", on);
			return ICM20645_ERR_I2C;
		}
	}
	return ICM20645_SUCCESS;

}

static int icm20645_lp_mode(struct i2c_client *client, bool on)
{

	int res = 0;
	u8 databuf[2];

	memset(databuf, 0, sizeof(databuf));
	icm20645_set_bank(client, BANK_SEL_0);
	GSE_FUN(f);
	/* acc lp config */

	res = mpu_i2c_read_block(client, ICM20645_REG_LP_CONFIG, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("icm20645_lp_mode fail at %x", on);
		return ICM20645_ERR_I2C;
	}
	if (on == true) {
		databuf[0] |= BIT_ACC_LP_EN;
		databuf[0] &= ~BIT_ACC_I2C_MST;
		res = mpu_i2c_write_block(client, ICM20645_REG_LP_CONFIG, databuf, 0x1);
		if (res < 0) {
			GSE_ERR("icm20645_lp_mode fail at %x", on);
			return ICM20645_ERR_I2C;
		}
	} else {
		databuf[0] &= ~BIT_ACC_LP_EN;
		databuf[0] &= ~BIT_ACC_I2C_MST;
		res = mpu_i2c_write_block(client, ICM20645_REG_LP_CONFIG, databuf, 0x1);
		if (res < 0) {
			GSE_ERR("icm20645_lp_mode fail at %x", on);
			return ICM20645_ERR_I2C;
		}
	}
	return ICM20645_SUCCESS;

}


static int ICM20645_Setfilter(struct i2c_client *client, int filter_sample)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GSE_FUN(f);
	icm20645_set_bank(client, BANK_SEL_2);
	res = mpu_i2c_read_block(client, ICM20645_ACC_CONFIG_2, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("ICM20645_ACC_CONFIG_2 fail\n");
		return ICM20645_ERR_I2C;
	}
	databuf[0] = filter_sample;
	res = mpu_i2c_write_block(client, ICM20645_ACC_CONFIG_2, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("ICM20645_ACC_CONFIG_2 err!\n");
		return ICM20645_ERR_I2C;
	}

	icm20645_set_bank(client, BANK_SEL_0);
	return ICM20645_SUCCESS;
}

static int ICM20645_lowest_power_mode(struct i2c_client *client, unsigned int on)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	icm20645_set_bank(client, BANK_SEL_2);
	res = mpu_i2c_read_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("ICM20645_lowest_power_mode1 err!\n");
		return ICM20645_ERR_I2C;
	}
	if (on == true) {
		databuf[0] |= BIT_LP_EN;
		res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
		if (res < 0) {
			GSE_ERR("ICM20645_lowest_power_mode2 err!\n");
			return ICM20645_ERR_I2C;
		}
	} else {
		databuf[0] &= ~BIT_LP_EN;
		res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
		if (res < 0) {
			GSE_ERR("ICM20645_lowest_power_mode3 err!\n");
			return ICM20645_ERR_I2C;
		}
	}

	icm20645_set_bank(client, BANK_SEL_0);
	return ICM20645_SUCCESS;
}

static int ICM20645_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = { 0 };
	u16 rate_div = 0;
	int res = 0;

	rate_div = 1125 / sample_rate - 1;

	GSE_FUN(f);
	icm20645_set_bank(client, BANK_SEL_2);

	databuf[0] = rate_div % 256;

	res = mpu_i2c_write_block(client, ICM20645_REG_SAMRT_DIV2, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("write sample rate register err!\n");
		return ICM20645_ERR_I2C;
	}
	databuf[0] = rate_div / 256;
	res = mpu_i2c_write_block(client, ICM20645_REG_SAMRT_DIV1, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("write sample rate register err!\n");
		return ICM20645_ERR_I2C;
	}


	icm20645_set_bank(client, BANK_SEL_0);
	return ICM20645_SUCCESS;
}

/*--------------------icm20645 power control function----------------------------------*/
static void ICM20645_power(struct acc_hw *hw, unsigned int on)
{
}

/*----------------------------------------------------------------------------*/
static int ICM20645_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];
	int res = 0;
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);

	GSE_FUN(f);
	icm20645_set_bank(client, BANK_SEL_0);

	if (enable == sensor_power) {
		GSE_DBG("Sensor power status is newest!\n");
		return ICM20645_SUCCESS;
	}

	res = mpu_i2c_read_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
	if (res < 0)
		return ICM20645_ERR_I2C;

	databuf[0] &= ~ICM20645_SLEEP;

	if (enable == false) {
		if (ICM20645_gyro_mode() == false) {
			databuf[0] |= ICM20645_SLEEP;
		} else {
			res = ICM20645_lowest_power_mode(obj_i2c_data->client, true);
			if (res != ICM20645_SUCCESS) {
				GSE_ERR("ICM20645_lowest_power_mode error\n");
				return res;
			}
		}
	}

	res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("set power mode failed!\n");
		return ICM20645_ERR_I2C;
	} else if (atomic_read(&obj->trace) & ICM20645_TRC_INFO)
		GSE_DBG("set power mode ok %d!\n", databuf[0]);
	sensor_power = enable;
	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_SetDataResolution(struct icm20645_i2c_data *obj)
{
	int err = 0;
	u8 dat = 0, reso = 0;

	GSE_FUN(f);
	err = mpu_i2c_read_block(obj->client, ICM20645_ACC_CONFIG, &dat, 1);
	if (err) {
		GSE_ERR("write data format fail!!\n");
		return err;
	}

	/*the data_reso is combined by 3 bits: {FULL_RES, DATA_RANGE} */
	reso = 0x00;
	reso = (dat & ICM20645_RANGE_16G) >> 1;

	if (reso < sizeof(icm20645_data_resolution) / sizeof(icm20645_data_resolution[0])) {
		obj->reso = &icm20645_data_resolution[reso];
		return 0;
	} else
		return -EINVAL;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadData(struct i2c_client *client, s16 data[ICM20645_AXES_NUM])
{
	struct icm20645_i2c_data *priv = i2c_get_clientdata(client);
	u8 buf[ICM20645_DATA_LEN] = { 0 };
	int err = 0;

	if (NULL == client)
		return -EINVAL;
	/* write then burst read */
	mpu_i2c_read_block(client, ICM20645_REG_DATAX0, buf, ICM20645_DATA_LEN);

	data[ICM20645_AXIS_X] = (s16) ((buf[ICM20645_AXIS_X * 2] << 8) | (buf[ICM20645_AXIS_X * 2 + 1]));
	data[ICM20645_AXIS_Y] = (s16) ((buf[ICM20645_AXIS_Y * 2] << 8) | (buf[ICM20645_AXIS_Y * 2 + 1]));
	data[ICM20645_AXIS_Z] = (s16) ((buf[ICM20645_AXIS_Z * 2] << 8) | (buf[ICM20645_AXIS_Z * 2 + 1]));

	if (atomic_read(&priv->trace) & ICM20645_TRC_RAWDATA) {
		GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[ICM20645_AXIS_X], data[ICM20645_AXIS_Y],
			data[ICM20645_AXIS_Z], data[ICM20645_AXIS_X], data[ICM20645_AXIS_Y], data[ICM20645_AXIS_Z]);
	}
#ifdef CONFIG_ICM20645_LOWPASS
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][ICM20645_AXIS_X] = data[ICM20645_AXIS_X];
				priv->fir.raw[priv->fir.num][ICM20645_AXIS_Y] = data[ICM20645_AXIS_Y];
				priv->fir.raw[priv->fir.num][ICM20645_AXIS_Z] = data[ICM20645_AXIS_Z];
				priv->fir.sum[ICM20645_AXIS_X] += data[ICM20645_AXIS_X];
				priv->fir.sum[ICM20645_AXIS_Y] += data[ICM20645_AXIS_Y];
				priv->fir.sum[ICM20645_AXIS_Z] += data[ICM20645_AXIS_Z];
				if (atomic_read(&priv->trace) & ICM20645_TRC_FILTER) {
					GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
						priv->fir.raw[priv->fir.num][ICM20645_AXIS_X],
						priv->fir.raw[priv->fir.num][ICM20645_AXIS_Y],
						priv->fir.raw[priv->fir.num][ICM20645_AXIS_Z],
						priv->fir.sum[ICM20645_AXIS_X], priv->fir.sum[ICM20645_AXIS_Y],
						priv->fir.sum[ICM20645_AXIS_Z]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[ICM20645_AXIS_X] -= priv->fir.raw[idx][ICM20645_AXIS_X];
				priv->fir.sum[ICM20645_AXIS_Y] -= priv->fir.raw[idx][ICM20645_AXIS_Y];
				priv->fir.sum[ICM20645_AXIS_Z] -= priv->fir.raw[idx][ICM20645_AXIS_Z];
				priv->fir.raw[idx][ICM20645_AXIS_X] = data[ICM20645_AXIS_X];
				priv->fir.raw[idx][ICM20645_AXIS_Y] = data[ICM20645_AXIS_Y];
				priv->fir.raw[idx][ICM20645_AXIS_Z] = data[ICM20645_AXIS_Z];
				priv->fir.sum[ICM20645_AXIS_X] += data[ICM20645_AXIS_X];
				priv->fir.sum[ICM20645_AXIS_Y] += data[ICM20645_AXIS_Y];
				priv->fir.sum[ICM20645_AXIS_Z] += data[ICM20645_AXIS_Z];
				priv->fir.idx++;
				data[ICM20645_AXIS_X] = priv->fir.sum[ICM20645_AXIS_X] / firlen;
				data[ICM20645_AXIS_Y] = priv->fir.sum[ICM20645_AXIS_Y] / firlen;
				data[ICM20645_AXIS_Z] = priv->fir.sum[ICM20645_AXIS_Z] / firlen;
				if (atomic_read(&priv->trace) & ICM20645_TRC_FILTER) {
					GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][ICM20645_AXIS_X],
						priv->fir.raw[idx][ICM20645_AXIS_Y],
						priv->fir.raw[idx][ICM20645_AXIS_Z], priv->fir.sum[ICM20645_AXIS_X],
						priv->fir.sum[ICM20645_AXIS_Y], priv->fir.sum[ICM20645_AXIS_Z],
						data[ICM20645_AXIS_X], data[ICM20645_AXIS_Y], data[ICM20645_AXIS_Z]);
				}
			}
		}
	}
#endif
	return err;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadOffset(struct i2c_client *client, s8 ofs[ICM20645_AXES_NUM])
{
	int err = 0;
#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#endif

	return err;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ResetCalibration(struct i2c_client *client)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
	s8 ofs[ICM20645_AXES_NUM] = { 0x00, 0x00, 0x00 };
#endif
	int err = 0;

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));

	return err;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadCalibration(struct i2c_client *client, int dat[ICM20645_AXES_NUM])
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
	int mul = 0;
#endif

	dat[obj->cvt.map[ICM20645_AXIS_X]] =
	    obj->cvt.sign[ICM20645_AXIS_X] * (obj->offset[ICM20645_AXIS_X] * mul + obj->cali_sw[ICM20645_AXIS_X]);
	dat[obj->cvt.map[ICM20645_AXIS_Y]] =
	    obj->cvt.sign[ICM20645_AXIS_Y] * (obj->offset[ICM20645_AXIS_Y] * mul + obj->cali_sw[ICM20645_AXIS_Y]);
	dat[obj->cvt.map[ICM20645_AXIS_Z]] =
	    obj->cvt.sign[ICM20645_AXIS_Z] * (obj->offset[ICM20645_AXIS_Z] * mul + obj->cali_sw[ICM20645_AXIS_Z]);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadCalibrationEx(struct i2c_client *client, int act[ICM20645_AXES_NUM], int raw[ICM20645_AXES_NUM])
{
	/*raw: the raw calibration data; act: the actual calibration data */
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
	int mul = 0;
#endif

	raw[ICM20645_AXIS_X] = obj->offset[ICM20645_AXIS_X] * mul + obj->cali_sw[ICM20645_AXIS_X];
	raw[ICM20645_AXIS_Y] = obj->offset[ICM20645_AXIS_Y] * mul + obj->cali_sw[ICM20645_AXIS_Y];
	raw[ICM20645_AXIS_Z] = obj->offset[ICM20645_AXIS_Z] * mul + obj->cali_sw[ICM20645_AXIS_Z];

	act[obj->cvt.map[ICM20645_AXIS_X]] = obj->cvt.sign[ICM20645_AXIS_X] * raw[ICM20645_AXIS_X];
	act[obj->cvt.map[ICM20645_AXIS_Y]] = obj->cvt.sign[ICM20645_AXIS_Y] * raw[ICM20645_AXIS_Y];
	act[obj->cvt.map[ICM20645_AXIS_Z]] = obj->cvt.sign[ICM20645_AXIS_Z] * raw[ICM20645_AXIS_Z];

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_WriteCalibration(struct i2c_client *client, int dat[ICM20645_AXES_NUM])
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[ICM20645_AXES_NUM], raw[ICM20645_AXES_NUM];
#ifndef SW_CALIBRATION
	int lsb = icm20645_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity / lsb;
#endif

	err = ICM20645_ReadCalibrationEx(client, cali, raw);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		raw[ICM20645_AXIS_X], raw[ICM20645_AXIS_Y], raw[ICM20645_AXIS_Z],
		obj->offset[ICM20645_AXIS_X], obj->offset[ICM20645_AXIS_Y], obj->offset[ICM20645_AXIS_Z],
		obj->cali_sw[ICM20645_AXIS_X], obj->cali_sw[ICM20645_AXIS_Y], obj->cali_sw[ICM20645_AXIS_Z]);

	/*calculate the real offset expected by caller */
	cali[ICM20645_AXIS_X] += dat[ICM20645_AXIS_X];
	cali[ICM20645_AXIS_Y] += dat[ICM20645_AXIS_Y];
	cali[ICM20645_AXIS_Z] += dat[ICM20645_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", dat[ICM20645_AXIS_X], dat[ICM20645_AXIS_Y], dat[ICM20645_AXIS_Z]);
#ifdef SW_CALIBRATION
	obj->cali_sw[ICM20645_AXIS_X] = obj->cvt.sign[ICM20645_AXIS_X] * (cali[obj->cvt.map[ICM20645_AXIS_X]]);
	obj->cali_sw[ICM20645_AXIS_Y] = obj->cvt.sign[ICM20645_AXIS_Y] * (cali[obj->cvt.map[ICM20645_AXIS_Y]]);
	obj->cali_sw[ICM20645_AXIS_Z] = obj->cvt.sign[ICM20645_AXIS_Z] * (cali[obj->cvt.map[ICM20645_AXIS_Z]]);
#else

	obj->offset[ICM20645_AXIS_X] =
	    (s8) (obj->cvt.sign[ICM20645_AXIS_X] * (cali[obj->cvt.map[ICM20645_AXIS_X]]) / (divisor));
	obj->offset[ICM20645_AXIS_Y] =
	    (s8) (obj->cvt.sign[ICM20645_AXIS_Y] * (cali[obj->cvt.map[ICM20645_AXIS_Y]]) / (divisor));
	obj->offset[ICM20645_AXIS_Z] =
	    (s8) (obj->cvt.sign[ICM20645_AXIS_Z] * (cali[obj->cvt.map[ICM20645_AXIS_Z]]) / (divisor));

	/*convert software calibration using standard calibration */
	obj->cali_sw[ICM20645_AXIS_X] =
	    obj->cvt.sign[ICM20645_AXIS_X] * (cali[obj->cvt.map[ICM20645_AXIS_X]]) % (divisor);
	obj->cali_sw[ICM20645_AXIS_Y] =
	    obj->cvt.sign[ICM20645_AXIS_Y] * (cali[obj->cvt.map[ICM20645_AXIS_Y]]) % (divisor);
	obj->cali_sw[ICM20645_AXIS_Z] =
	    obj->cvt.sign[ICM20645_AXIS_Z] * (cali[obj->cvt.map[ICM20645_AXIS_Z]]) % (divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n",
		obj->offset[ICM20645_AXIS_X] * divisor + obj->cali_sw[ICM20645_AXIS_X],
		obj->offset[ICM20645_AXIS_Y] * divisor + obj->cali_sw[ICM20645_AXIS_Y],
		obj->offset[ICM20645_AXIS_Z] * divisor + obj->cali_sw[ICM20645_AXIS_Z],
		obj->offset[ICM20645_AXIS_X], obj->offset[ICM20645_AXIS_Y], obj->offset[ICM20645_AXIS_Z],
		obj->cali_sw[ICM20645_AXIS_X], obj->cali_sw[ICM20645_AXIS_Y], obj->cali_sw[ICM20645_AXIS_Z]);
	err = hwmsen_write_block(obj->client, ICM20645_REG_OFSX, obj->offset, ICM20645_AXES_NUM);
	if (err) {
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8) * 10);
	icm20645_set_bank(client, BANK_SEL_0);

	res = mpu_i2c_read_block(client, ICM20645_REG_DEVID, databuf, 0x1);
	if (res < 0)
		goto exit_ICM20645_CheckDeviceID;

	GSE_LOG("ICM20645_CheckDeviceID 0x%x\n", databuf[0]);

exit_ICM20645_CheckDeviceID:
	if (res < 0)
		return ICM20645_ERR_I2C;
	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2];
	int res = 0;

	GSE_FUN(f);

	memset(databuf, 0, sizeof(u8) * 2);
	icm20645_set_bank(client, BANK_SEL_2);

	/* write */
	databuf[0] = (0 | dataformat);
	res = mpu_i2c_write_block(client, ICM20645_ACC_CONFIG, databuf, 0x1);

	if (res < 0)
		return ICM20645_ERR_I2C;
	return ICM20645_SetDataResolution(obj);
}

/*----------------------------------------------------------------------------*/
static int ICM20645_Dev_Reset(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	GSE_FUN(f);
	memset(databuf, 0, sizeof(u8) * 10);
	icm20645_set_bank(client, BANK_SEL_0);

	/* read */
	res = mpu_i2c_read_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
	if (res < 0)
		return ICM20645_ERR_I2C;

	/* write */
	databuf[0] = databuf[0] | ICM20645_DEV_RESET;
	res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);

	if (res < 0) {
		GSE_ERR("ICM20645_Dev_Reset fail\n");
		return ICM20645_ERR_I2C;
	}

	do {
		res = mpu_i2c_read_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
		if (res < 0)
			return ICM20645_ERR_I2C;
		GSE_LOG("[Gsensor] check reset bit");
	} while ((databuf[0] & ICM20645_DEV_RESET) != 0);
	msleep(50);
	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[2];
	int res = 0;

	GSE_FUN(f);

	memset(databuf, 0, sizeof(u8) * 2);
	databuf[0] = intenable;
	res = mpu_i2c_write_block(client, ICM20645_REG_INT_ENABLE, databuf, 0x1);
	if (res < 0)
		return ICM20645_ERR_I2C;

	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int icm20645_gpio_config(void)
{

	/*
	   mt_set_gpio_mode(GPIO_GSE_1_EINT_PIN, GPIO_GSE_1_EINT_PIN_M_GPIO);
	   mt_set_gpio_dir(GPIO_GSE_1_EINT_PIN, GPIO_DIR_IN);
	   mt_set_gpio_pull_enable(GPIO_GSE_1_EINT_PIN, GPIO_PULL_ENABLE);
	   mt_set_gpio_pull_select(GPIO_GSE_1_EINT_PIN, GPIO_PULL_DOWN);
	 */

	/*
	   mt_set_gpio_mode(GPIO_GSE_2_EINT_PIN, GPIO_GSE_2_EINT_PIN_M_GPIO);
	   mt_set_gpio_dir(GPIO_GSE_2_EINT_PIN, GPIO_DIR_IN);
	   mt_set_gpio_pull_enable(GPIO_GSE_2_EINT_PIN, GPIO_PULL_ENABLE);
	   mt_set_gpio_pull_select(GPIO_GSE_2_EINT_PIN, GPIO_PULL_DOWN);
	 */
	return 0;
}

static int ICM20645_selclk(struct i2c_client *client)
{
	int res = 0;
	u8 databuf[2];

	res = mpu_i2c_read_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("ICM20645_REG_POWER_CTL read fail\n");
		return ICM20645_ERR_I2C;
	}
	databuf[0] |= (BIT_CLK_PLL | BIT_TEMP_DIS);
	res = mpu_i2c_write_block(client, ICM20645_REG_POWER_CTL, databuf, 0x1);
	if (res < 0) {
		GSE_ERR("ICM20645_REG_POWER_CTL write fail\n");
		return ICM20645_ERR_I2C;
	}
	return ICM20645_SUCCESS;
}

static int icm20645_init_client(struct i2c_client *client, int reset_cali)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	int retry_fail = 0 ;

	icm20645_gpio_config();
	res = ICM20645_selclk(client);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("ICM20645_selclk error\n");
		return res;
	}
	res = ICM20645_CheckDeviceID(client);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("Check ID error\n");
		return res;
	}
	res = ICM20645_SetPowerMode(client, false);
	for (; retry_fail < 10; retry_fail++) {
		res = ICM20645_SetPowerMode(client, true);
		if (res != ICM20645_SUCCESS) {
			GSE_ERR("set power error\n");
			udelay(5);
			continue;
		}
		if (retry_fail == 9) {
			GSE_ERR("set power error, retry 10 times\n");
			return res;
		}
		break;
	}
	res = icm20645_turn_on(client, BIT_PWR_ACCEL_STBY, true);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("icm20645_turn_on error\n");
		return res;
	}
	res = icm20645_lp_mode(client, true);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("icm20645_lp_mode error\n");
		return res;
	}
	res = ICM20645_lowest_power_mode(client, false);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("ICM20645_lowest_power_mode error\n");
		return res;
	}
	res = ICM20645_SetDataFormat(client, (ACCEL_DLPFCFG | ICM20645_RANGE_16G | ACCEL_FCHOICE));
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("set data format error\n");
		return res;
	}
	res = ICM20645_Setfilter(client, ACCEL_AVGCFG_8X);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("ICM20645_Setfilter error\n");
		return res;
	}
	res = ICM20645_SetSampleRate(client, 125);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("ICM20645_SetSampleRate error\n");
		return res;
	}
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

	res = ICM20645_SetIntEnable(client, 0x00);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("icm20645_SetIntEnable error\n");
		return res;
	}

	if (0 != reset_cali) {
		/*reset calibration only in power on */
		res = ICM20645_ResetCalibration(client);
		if (res != ICM20645_SUCCESS)
			return res;
	}
#ifdef CONFIG_ICM20645_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif
	res = icm20645_turn_on(client, BIT_PWR_ACCEL_STBY, false);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("icm20645_turn_on error\n");
		return res;
	}
	res = ICM20645_lowest_power_mode(client, true);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("ICM20645_lowest_power_mode error\n");
		return res;
	}
	res = ICM20645_SetPowerMode(client, false);
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("set power error\n");
		return res;
	}
	icm20645_set_bank(client, BANK_SEL_0);
	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadAllReg(struct i2c_client *client, char *buf, int bufsize)
{
	u8 total_len = 8;

	u8 addr = 0;
	u8 buff[total_len + 1];
	int err = 0;
	int i, res;

	if (sensor_power == false) {
		err = ICM20645_SetPowerMode(client, true);
		if (err)
			GSE_ERR("Power on mpu6050 error %d!\n", err);
		msleep(50);
	}

	icm20645_set_bank(client, BANK_SEL_0);
	res = mpu_i2c_read_block(client, addr, buff, total_len);
	if (res < 0)
		return ICM20645_ERR_I2C;

	for (i = 0; i <= total_len; i++)
		GSE_ERR("ICM20645 bank0 reg=0x%x, data=0x%x\n", (addr + i), buff[i]);
	icm20645_set_bank(client, BANK_SEL_2);
	addr = 0X10;
	res = mpu_i2c_read_block(client, addr, buff, total_len);
	if (res < 0)
		return ICM20645_ERR_I2C;

	for (i = 0; i <= total_len; i++)
		GSE_ERR("ICM20645 bank2 reg=0x%x, data=0x%x\n", (addr + i), buff[i]);
	icm20645_set_bank(client, BANK_SEL_0);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8) * 10);

	if ((NULL == buf) || (bufsize <= 30))
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "ICM20645 Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct icm20645_i2c_data *obj = obj_i2c_data;
	int acc[ICM20645_AXES_NUM];
	int res = 0;

	client = obj->client;

	if (atomic_read(&obj->suspend))
		return -3;

	if (NULL == buf)
		return -1;
	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	if (sensor_power == false) {
		res = ICM20645_SetPowerMode(client, true);
		res = icm20645_turn_on(client, BIT_PWR_ACCEL_STBY, true);
		if (res)
			GSE_ERR("Power on icm20645 error %d!\n", res);
		msleep(50);
	}
	res = ICM20645_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d\n", res);
		return -3;
	}
	obj->data[ICM20645_AXIS_X] += obj->cali_sw[ICM20645_AXIS_X];
	obj->data[ICM20645_AXIS_Y] += obj->cali_sw[ICM20645_AXIS_Y];
	obj->data[ICM20645_AXIS_Z] += obj->cali_sw[ICM20645_AXIS_Z];

	/*remap coordinate */
	acc[obj->cvt.map[ICM20645_AXIS_X]] = obj->cvt.sign[ICM20645_AXIS_X] * obj->data[ICM20645_AXIS_X];
	acc[obj->cvt.map[ICM20645_AXIS_Y]] = obj->cvt.sign[ICM20645_AXIS_Y] * obj->data[ICM20645_AXIS_Y];
	acc[obj->cvt.map[ICM20645_AXIS_Z]] = obj->cvt.sign[ICM20645_AXIS_Z] * obj->data[ICM20645_AXIS_Z];

	acc[ICM20645_AXIS_X] = acc[ICM20645_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	acc[ICM20645_AXIS_Y] = acc[ICM20645_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	acc[ICM20645_AXIS_Z] = acc[ICM20645_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;

	sprintf(buf, "%04x %04x %04x", acc[ICM20645_AXIS_X], acc[ICM20645_AXIS_Y], acc[ICM20645_AXIS_Z]);
	if (atomic_read(&obj->trace) & ICM20645_TRC_IOCTL)
		GSE_LOG("gsensor data: %s!\n", buf);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadRawData(struct i2c_client *client, char *buf)
{
	struct icm20645_i2c_data *obj = (struct icm20645_i2c_data *)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
		return -2;

	if (atomic_read(&obj->suspend))
		return -3;
	res = ICM20645_ReadData(client, obj->data);
	if (res) {
		GSE_ERR("I2C error: ret value=%d", res);
		return -1;
	}
	sprintf(buf, "%04x %04x %04x", obj->data[ICM20645_AXIS_X],
		obj->data[ICM20645_AXIS_Y], obj->data[ICM20645_AXIS_Z]);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
	u8 data = 0;
#if 0
	res = ICM20645_SetBWRate(client, ICM20645_BW_184HZ);
	if (res != ICM20645_SUCCESS)
		return res;
#endif
	res = mpu_i2c_read_block(client, ICM20645_ACC_CONFIG, &data, 1);

	if (res != ICM20645_SUCCESS)
		return res;

	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_JudgeTestResult(struct i2c_client *client, s32 prv[ICM20645_AXES_NUM], s32 nxt[ICM20645_AXES_NUM])
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
	u8 format = 0;
	int res = 0;

	res = mpu_i2c_read_block(client, ICM20645_ACC_CONFIG, &format, 1);
	if (res)
		return res;

	format = format & ICM20645_RANGE_16G;

	switch (format) {
	case ICM20645_RANGE_2G:
		GSE_LOG("format use self[0]\n");
		ptr = &self[0];
		break;

	case ICM20645_RANGE_4G:
		GSE_LOG("format use self[1]\n");
		ptr = &self[1];
		break;

	case ICM20645_RANGE_8G:
		GSE_LOG("format use self[2]\n");
		ptr = &self[2];
		break;

	case ICM20645_RANGE_16G:
		GSE_LOG("format use self[3]\n");
		ptr = &self[3];
		break;

	default:
		GSE_LOG("invilad case\n");
		break;
	}

	if (!ptr) {
		GSE_ERR("null pointer\n");
		return -EINVAL;
	}
	GSE_LOG("format=0x%x\n", format);

	GSE_LOG("X diff is %ld\n", abs(nxt[ICM20645_AXIS_X] - prv[ICM20645_AXIS_X]));
	GSE_LOG("Y diff is %ld\n", abs(nxt[ICM20645_AXIS_Y] - prv[ICM20645_AXIS_Y]));
	GSE_LOG("Z diff is %ld\n", abs(nxt[ICM20645_AXIS_Z] - prv[ICM20645_AXIS_Z]));

	if ((abs(nxt[ICM20645_AXIS_X] - prv[ICM20645_AXIS_X]) > (*ptr)[ICM20645_AXIS_X].max) ||
	    (abs(nxt[ICM20645_AXIS_X] - prv[ICM20645_AXIS_X]) < (*ptr)[ICM20645_AXIS_X].min)) {
		GSE_ERR("X is over range\n");
		res = -EINVAL;
	}
	if ((abs(nxt[ICM20645_AXIS_Y] - prv[ICM20645_AXIS_Y]) > (*ptr)[ICM20645_AXIS_Y].max) ||
	    (abs(nxt[ICM20645_AXIS_Y] - prv[ICM20645_AXIS_Y]) < (*ptr)[ICM20645_AXIS_Y].min)) {
		GSE_ERR("Y is over range\n");
		res = -EINVAL;
	}
	if ((abs(nxt[ICM20645_AXIS_Z] - prv[ICM20645_AXIS_Z]) > (*ptr)[ICM20645_AXIS_Z].max) ||
	    (abs(nxt[ICM20645_AXIS_Z] - prv[ICM20645_AXIS_Z]) < (*ptr)[ICM20645_AXIS_Z].min)) {
		GSE_ERR("Z is over range\n");
		res = -EINVAL;
	}
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = icm20645_i2c_client;
	char strbuf[ICM20645_BUFSIZE];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	if (sensor_power == false) {
		ICM20645_SetPowerMode(client, true);
		msleep(50);
	}

	ICM20645_ReadAllReg(client, strbuf, ICM20645_BUFSIZE);

	ICM20645_ReadChipInfo(client, strbuf, ICM20645_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = icm20645_i2c_client;
	char strbuf[ICM20645_BUFSIZE];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	ICM20645_ReadSensorData(client, strbuf, ICM20645_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = icm20645_i2c_client;
	struct icm20645_i2c_data *obj;
	int err = 0, len = 0, mul = 0;
	int tmp[ICM20645_AXES_NUM];

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	err = ICM20645_ReadOffset(client, obj->offset);
	if (err)
		return -EINVAL;
	err = ICM20645_ReadCalibration(client, tmp);
	if (err)
		return -EINVAL;
	mul = obj->reso->sensitivity / icm20645_offset_resolution.sensitivity;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n",
		     mul, obj->offset[ICM20645_AXIS_X], obj->offset[ICM20645_AXIS_Y],
		     obj->offset[ICM20645_AXIS_Z], obj->offset[ICM20645_AXIS_X], obj->offset[ICM20645_AXIS_Y],
		     obj->offset[ICM20645_AXIS_Z]);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
		     obj->cali_sw[ICM20645_AXIS_X], obj->cali_sw[ICM20645_AXIS_Y],
		     obj->cali_sw[ICM20645_AXIS_Z]);

	len += snprintf(buf + len, PAGE_SIZE - len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
			obj->offset[ICM20645_AXIS_X] * mul + obj->cali_sw[ICM20645_AXIS_X],
			obj->offset[ICM20645_AXIS_Y] * mul + obj->cali_sw[ICM20645_AXIS_Y],
			obj->offset[ICM20645_AXIS_Z] * mul + obj->cali_sw[ICM20645_AXIS_Z],
			tmp[ICM20645_AXIS_X], tmp[ICM20645_AXIS_Y], tmp[ICM20645_AXIS_Z]);

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = icm20645_i2c_client;
	int err = 0, x = 0, y = 0, z = 0;
	int dat[ICM20645_AXES_NUM];

	if (!strncmp(buf, "rst", 3)) {
		err = ICM20645_ResetCalibration(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);
	} else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z)) {
		dat[ICM20645_AXIS_X] = x;
		dat[ICM20645_AXIS_Y] = y;
		dat[ICM20645_AXIS_Z] = z;
		err = ICM20645_WriteCalibration(client, dat);
		if (err)
			GSE_ERR("write calibration err = %d\n", err);
	} else
		GSE_ERR("invalid format\n");

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = icm20645_i2c_client;

	if (NULL == client) {
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	return snprintf(buf, 8, "%s\n", selftestRes);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{				/*write anything to this register will trigger the process */
	int ret = 0;
	struct item {
		s16 raw[ICM20645_AXES_NUM];
	};

	struct i2c_client *client = icm20645_i2c_client;
	int idx = 0, res = 0, num = 0;
	struct item *prv = NULL, *nxt = NULL;
	s32 avg_prv[ICM20645_AXES_NUM] = { 0, 0, 0 };
	s32 avg_nxt[ICM20645_AXES_NUM] = { 0, 0, 0 };

	ret = kstrtoint(buf, 10, &num);
	if (ret != 0) {
		GSE_ERR("parse number fail\n");
		return count;
	} else if (num == 0) {
		GSE_ERR("invalid data count\n");
		return count;
	}
	prv = kcalloc(num, sizeof(*prv), GFP_KERNEL);
	prv = kcalloc(num, sizeof(*nxt), GFP_KERNEL);
	if (!prv || !nxt)
		goto exit;

	GSE_LOG("NORMAL:\n");
	ICM20645_SetPowerMode(client, true);
	msleep(50);

	for (idx = 0; idx < num; idx++) {
		res = ICM20645_ReadData(client, prv[idx].raw);
		if (res) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}

		avg_prv[ICM20645_AXIS_X] += prv[idx].raw[ICM20645_AXIS_X];
		avg_prv[ICM20645_AXIS_Y] += prv[idx].raw[ICM20645_AXIS_Y];
		avg_prv[ICM20645_AXIS_Z] += prv[idx].raw[ICM20645_AXIS_Z];
		GSE_LOG("[%5d %5d %5d]\n", prv[idx].raw[ICM20645_AXIS_X], prv[idx].raw[ICM20645_AXIS_Y],
			prv[idx].raw[ICM20645_AXIS_Z]);
	}

	avg_prv[ICM20645_AXIS_X] /= num;
	avg_prv[ICM20645_AXIS_Y] /= num;
	avg_prv[ICM20645_AXIS_Z] /= num;

	/*initial setting for self test */
	GSE_LOG("SELFTEST:\n");
	for (idx = 0; idx < num; idx++) {
		res = ICM20645_ReadData(client, nxt[idx].raw);
		if (res) {
			GSE_ERR("read data fail: %d\n", res);
			goto exit;
		}
		avg_nxt[ICM20645_AXIS_X] += nxt[idx].raw[ICM20645_AXIS_X];
		avg_nxt[ICM20645_AXIS_Y] += nxt[idx].raw[ICM20645_AXIS_Y];
		avg_nxt[ICM20645_AXIS_Z] += nxt[idx].raw[ICM20645_AXIS_Z];
		GSE_LOG("[%5d %5d %5d]\n", nxt[idx].raw[ICM20645_AXIS_X], nxt[idx].raw[ICM20645_AXIS_Y],
			nxt[idx].raw[ICM20645_AXIS_Z]);
	}

	avg_nxt[ICM20645_AXIS_X] /= num;
	avg_nxt[ICM20645_AXIS_Y] /= num;
	avg_nxt[ICM20645_AXIS_Z] /= num;

	GSE_LOG("X: %5d - %5d = %5d\n", avg_nxt[ICM20645_AXIS_X], avg_prv[ICM20645_AXIS_X],
		avg_nxt[ICM20645_AXIS_X] - avg_prv[ICM20645_AXIS_X]);
	GSE_LOG("Y: %5d - %5d = %5d\n", avg_nxt[ICM20645_AXIS_Y], avg_prv[ICM20645_AXIS_Y],
		avg_nxt[ICM20645_AXIS_Y] - avg_prv[ICM20645_AXIS_Y]);
	GSE_LOG("Z: %5d - %5d = %5d\n", avg_nxt[ICM20645_AXIS_Z], avg_prv[ICM20645_AXIS_Z],
		avg_nxt[ICM20645_AXIS_Z] - avg_prv[ICM20645_AXIS_Z]);

	if (!ICM20645_JudgeTestResult(client, avg_prv, avg_nxt)) {
		GSE_LOG("SELFTEST : PASS\n");
		strcpy(selftestRes, "y");
	} else {
		GSE_LOG("SELFTEST : FAIL\n");
		strcpy(selftestRes, "n");
	}

 exit:
	/*restore the setting */
	icm20645_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = icm20645_i2c_client;
	struct icm20645_i2c_data *obj;

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
	struct icm20645_i2c_data *obj = obj_i2c_data;
	int tmp = 0;
	int ret = 0;

	if (NULL == obj) {
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}

	ret = kstrtoint(buf, 10, &tmp);

	if (ret == 0) {
		if (atomic_read(&obj->selftest) && !tmp)
			icm20645_init_client(obj->client, 0);
		else if (!atomic_read(&obj->selftest) && tmp)
			ICM20645_InitSelfTest(obj->client);

		GSE_LOG("selftest: %d => %d\n", atomic_read(&obj->selftest), tmp);
		atomic_set(&obj->selftest, tmp);
	} else
		GSE_ERR("invalid content: '%s', length = %zu\n", buf, count);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_ICM20645_LOWPASS
	struct i2c_client *client = icm20645_i2c_client;
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);

	if (atomic_read(&obj->firlen)) {
		int idx, len = atomic_read(&obj->firlen);

		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for (idx = 0; idx < len; idx++) {
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][ICM20645_AXIS_X],
				obj->fir.raw[idx][ICM20645_AXIS_Y], obj->fir.raw[idx][ICM20645_AXIS_Z]);
		}

		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[ICM20645_AXIS_X], obj->fir.sum[ICM20645_AXIS_Y],
			obj->fir.sum[ICM20645_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[ICM20645_AXIS_X] / len,
			obj->fir.sum[ICM20645_AXIS_Y] / len, obj->fir.sum[ICM20645_AXIS_Z] / len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}

/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_ICM20645_LOWPASS
	struct i2c_client *client = icm20645_i2c_client;
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int firlen = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &firlen);
	if (ret != 0)
		GSE_ERR("invallid format\n");
	else if (firlen > C_MAX_FIR_LENGTH)
		GSE_ERR("exceeds maximum filter length\n");
	else {
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
	ssize_t res = 0;
	struct icm20645_i2c_data *obj = obj_i2c_data;

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
	struct icm20645_i2c_data *obj = obj_i2c_data;
	int trace = 0;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GSE_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct icm20645_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct acc_hw *_ptAccelHw = hw;

	GSE_DBG("[%s] default direction: %d\n", __func__, _ptAccelHw->direction);

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n", _ptAccelHw->direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri, const char *buf, size_t tCount)
{
	int _nDirection = 0, ret = 0;
	struct icm20645_i2c_data *_pt_i2c_obj = obj_i2c_data;

	if (NULL == _pt_i2c_obj)
		return 0;
	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret == 0) {
		if (hwmsen_get_convert(_nDirection, &_pt_i2c_obj->cvt))
			GSE_ERR("ERR: fail to set direction\n");
	}

	GSE_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
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
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *icm20645_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_cali,	/*show calibration data */
	&driver_attr_self,	/*self test demo */
	&driver_attr_selftest,	/*self control: 0: disable, 1: enable */
	&driver_attr_firlen,	/*filter length: 0: disable, others: enable */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
	&driver_attr_orientation,
};

/*----------------------------------------------------------------------------*/
static int icm20645_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(icm20645_attr_list) / sizeof(icm20645_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, icm20645_attr_list[idx]);
		if (0 != err) {
			GSE_ERR("driver_create_file (%s) = %d\n", icm20645_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int icm20645_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(icm20645_attr_list) / sizeof(icm20645_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, icm20645_attr_list[idx]);

	return err;
}
static int icm20645_open(struct inode *inode, struct file *file)
{
	file->private_data = icm20645_i2c_client;

	if (file->private_data == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int icm20645_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long icm20645_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
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

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_SET_CALI, (unsigned long)arg32);
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

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_GET_CALI, (unsigned long)arg32);
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

		err = file->f_op->unlocked_ioctl(file, GSENSOR_IOCTL_CLR_CALI, (unsigned long)arg32);
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

static long icm20645_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct icm20645_i2c_data *obj = (struct icm20645_i2c_data *)i2c_get_clientdata(client);
	char strbuf[ICM20645_BUFSIZE];
	void __user *data;
	long err = 0;
	int cali[3];
	struct SENSOR_DATA sensor_data = {0};

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
		icm20645_init_client(client, 0);
		break;

	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		ICM20645_ReadChipInfo(client, strbuf, ICM20645_BUFSIZE);
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

		ICM20645_ReadSensorData(client, strbuf, ICM20645_BUFSIZE);
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
			ICM20645_ReadRawData(client, strbuf);
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
			cali[ICM20645_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[ICM20645_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			cali[ICM20645_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			err = ICM20645_WriteCalibration(client, cali);
		}
		break;

	case GSENSOR_IOCTL_CLR_CALI:
		err = ICM20645_ResetCalibration(client);
		break;

	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = ICM20645_ReadCalibration(client, cali);
		if (err)
			break;

		sensor_data.x = cali[ICM20645_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.y = cali[ICM20645_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		sensor_data.z = cali[ICM20645_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
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

/*----------------------------------------------------------------------------*/
static const struct file_operations icm20645_fops = {
	.open = icm20645_open,
	.release = icm20645_release,
	.unlocked_ioctl = icm20645_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = icm20645_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice icm20645_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &icm20645_fops,
};

/*----------------------------------------------------------------------------*/
#ifndef USE_EARLY_SUSPEND
/*----------------------------------------------------------------------------*/
static int icm20645_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_DBG("%s: ENTER\n", __func__);
	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GSE_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);	
		err = icm20645_turn_on(obj->client, BIT_PWR_ACCEL_STBY, false);
		if (err != ICM20645_SUCCESS) {
			GSE_ERR("icm20645_turn_on error\n");
			return err;
		}
		err = ICM20645_SetPowerMode(obj->client, false);
		if (err) {
			GSE_ERR("write power control fail!!\n");
			return err;
		}
		ICM20645_power(obj->hw, 0);
		GSE_DBG("icm20645_suspend ok\n");
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int icm20645_resume(struct i2c_client *client)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GSE_DBG("%s: ENTER\n", __func__);
	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	ICM20645_power(obj->hw, 1);
	err = icm20645_init_client(client, 0);
	if (err) {
		GSE_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);
	GSE_DBG("icm20645_resume ok\n");

	return 0;
}

/*----------------------------------------------------------------------------*/
#else				/*CONFIG_HAS_EARLY_SUSPEND is defined */
/*----------------------------------------------------------------------------*/
static void icm20645_early_suspend(struct early_suspend *h)
{
	struct icm20645_i2c_data *obj = container_of(h, struct icm20645_i2c_data, early_drv);
	int err = 0;

	GSE_DBG("%s: ENTER\n", __func__);
	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = ICM20645_SetPowerMode(obj->client, false);
	if (err) {
		GSE_ERR("write power control fail!!\n");
		return;
	}

	if (ICM20645_gyro_mode() == false)
		ICM20645_Dev_Reset(obj->client);

	obj->bandwidth = 0;

	sensor_power = false;

	ICM20645_power(obj->hw, 0);
}

/*----------------------------------------------------------------------------*/
static void icm20645_late_resume(struct early_suspend *h)
{
	struct icm20645_i2c_data *obj = container_of(h, struct icm20645_i2c_data, early_drv);
	int err = 0;

	GSE_DBG("%s: ENTER\n", __func__);
	if (obj == NULL) {
		GSE_ERR("null pointer!!\n");
		return;
	}

	ICM20645_power(obj->hw, 1);
	err = icm20645_init_client(obj->client, 0);
	if (err) {
		GSE_ERR("initialize client fail!!\n");
		return;
	}
	atomic_set(&obj->suspend, 0);
}

/*----------------------------------------------------------------------------*/
#endif				/*CONFIG_HAS_EARLYSUSPEND */
/*----------------------------------------------------------------------------*/
static int icm20645_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, ICM20645_DEV_NAME);
	return 0;
}

static int icm20645_open_report_data(int open)
{
	return 0;
}

static int icm20645_enable_nodata(int en)
{
	int res = 0;
	int retry = 0;
	bool power = false;

	if (1 == en)
		power = true;
	if (0 == en)
		power = false;
	if (power == true) {
		for (retry = 0; retry < 3; retry++) {
			res = ICM20645_SetPowerMode(obj_i2c_data->client, true);
			if (res == 0) {
				GSE_LOG("ICM20645_SetPowerMode done\n");
				break;
			}
			GSE_ERR("ICM20645_SetPowerMode fail\n");
		}
		res = icm20645_turn_on(obj_i2c_data->client, BIT_PWR_ACCEL_STBY, true);
		if (res != ICM20645_SUCCESS) {
			GSE_ERR("icm20645_turn_on error\n");
			return res;
		}
		res = ICM20645_lowest_power_mode(obj_i2c_data->client, true);	
		if (res != ICM20645_SUCCESS) {
			GSE_ERR("ICM20645_lowest_power_mode error\n");
			return res;
		}
	} else {
		res = icm20645_turn_on(obj_i2c_data->client, BIT_PWR_ACCEL_STBY, false);
		if (res != ICM20645_SUCCESS) {
			GSE_ERR("icm20645_turn_on error\n");
			return res;
		}
		for (retry = 0; retry < 3; retry++) {
			res = ICM20645_SetPowerMode(obj_i2c_data->client, false);
			if (res == 0) {
				GSE_LOG("ICM20645_SetPowerMode done\n");
				break;
			}
			GSE_LOG("ICM20645_SetPowerMode fail\n");
		}
		
	}
	if (res != ICM20645_SUCCESS) {
		GSE_ERR("ICM20645_SetPowerMode fail!\n");
		return -1;
	}
	GSE_DBG("icm20645_enable_nodata OK!\n");
	return 0;
}

static int icm20645_set_delay(u64 ns)
{
#if 0
	int value = 0;
	int sample_delay = 0;
	int err;

	value = (int)ns / 1000 / 1000;
	if (value <= 5)
		sample_delay = ICM20645_BW_184HZ;
	else if (value <= 10)
		sample_delay = ICM20645_BW_94HZ;
	else
		sample_delay = ICM20645_BW_44HZ;

	err = ICM20645_SetBWRate(obj_i2c_data->client, sample_delay);
	if (err != ICM20645_SUCCESS) {
		GSE_ERR("icm20645_set_delay Set delay parameter error!\n");
		return -1;
	}
	GSE_LOG("icm20645_set_delay (%d)\n", value);
#endif
	return 0;
}

static int icm20645_get_data(int *x, int *y, int *z, int *status)
{
	char buff[ICM20645_BUFSIZE];

	ICM20645_ReadSensorData(obj_i2c_data->client, buff, ICM20645_BUFSIZE);

	if (3 == sscanf(buff, "%x %x %x", x, y, z))
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int icm20645_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct icm20645_i2c_data *obj;
	int err = 0;
	int retry = 0;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };
	GSE_DBG("%s: ENTER\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct icm20645_i2c_data));

	obj->hw = hw;

	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	obj_i2c_data = obj;
	obj->client = client;

	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

#ifdef CONFIG_ICM20645_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw->firlen);

	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);

#endif

	icm20645_i2c_client = new_client;

	for (retry = 0; retry < 10; retry++) {
		if (!ICM20645_Dev_Reset(new_client)) {
			break;
		} else {
			GSE_ERR("icm20645_i2c_probe, ICM20645_Dev_Reset failed\n");
		}
	}

	err = icm20645_init_client(new_client, 1);
	if (err)
		goto exit_init_failed;
	err = misc_register(&icm20645_device);
	if (err) {
		GSE_ERR("icm20645_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	err = icm20645_create_attr(&(icm20645_init_info.platform_diver_addr->driver));
	if (err) {
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = icm20645_open_report_data;
	ctl.enable_nodata = icm20645_enable_nodata;
	ctl.set_delay = icm20645_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = acc_register_control_path(&ctl);
	if (err) {
		GSE_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = icm20645_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		GSE_ERR("register acc data path err= %d\n", err);
		goto exit_kfree;
	}
#ifdef USE_EARLY_SUSPEND
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	    obj->early_drv.suspend = icm20645_early_suspend,
	    obj->early_drv.resume = icm20645_late_resume, register_early_suspend(&obj->early_drv);
#endif
	icm20645_init_flag = 0;

	GSE_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&icm20645_device);
exit_misc_device_register_failed:
exit_init_failed:
	/*i2c_detach_client(new_client);*/
exit_kfree:
	kfree(obj);
exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	icm20645_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int icm20645_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = icm20645_delete_attr(&(icm20645_init_info.platform_diver_addr->driver));
	if (err)
		GSE_ERR("icm20645_delete_attr fail: %d\n", err);
	err = misc_deregister(&icm20645_device);
	if (err)
		GSE_ERR("misc_deregister fail: %d\n", err);
	err = hwmsen_detach(ID_ACCELEROMETER);
	if (err)
		GSE_ERR("hwmsen_detach fail: %d\n", err);

	icm20645_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int icm20645_remove(void)
{
	GSE_DBG("%s: ENTER\n", __func__);
	ICM20645_power(hw, 0);
	i2c_del_driver(&icm20645_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/

static int icm20645_local_init(void)
{
	ICM20645_power(hw, 1);
	if (i2c_add_driver(&icm20645_i2c_driver)) {
		GSE_ERR("add driver error\n");
		return -1;
	}
	if (-1 == icm20645_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init icm20645gse_init(void)
{
	const char *name = "mediatek,icm20645g";

	hw = get_accel_dts_func(name, hw);
	if (!hw)
		GSE_ERR("get dts info fail\n");

	acc_driver_add(&icm20645_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit icm20645gse_exit(void)
{
	GSE_DBG("%s: ENTER\n", __func__);
}

/*----------------------------------------------------------------------------*/
module_init(icm20645gse_init);
module_exit(icm20645gse_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ICM20645 gse driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");

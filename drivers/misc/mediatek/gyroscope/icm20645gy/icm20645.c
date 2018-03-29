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

#include <cust_gyro.h>
#include "icm20645.h"
#include <gyroscope.h>

/*----------------------------------------------------------------------------*/
#define ICM20645_DEFAULT_FS		ICM20645_FS_2000
#define ICM20645_DEFAULT_LSB		ICM20645_FS_2000_LSB
/*---------------------------------------------------------------------------*/
#define DEBUG 0
/*----------------------------------------------------------------------------*/
#define CONFIG_ICM20645_LOWPASS	/*apply low pass filter on output */
/*----------------------------------------------------------------------------*/
#define ICM20645_AXIS_X          0
#define ICM20645_AXIS_Y          1
#define ICM20645_AXIS_Z          2
#define ICM20645_AXES_NUM        3
#define ICM20645_DATA_LEN        6
#define ICM20645_DEV_NAME        "ICM20645GY"	/* name must different with gsensor icm20645 */
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id icm20645_i2c_id[] = { {ICM20645_DEV_NAME, 0}, {} };
struct gyro_hw gyro_cust;
static struct gyro_hw *hw = &gyro_cust;
struct platform_device *gyroPltFmDev;
/* For  driver get cust info */
struct gyro_hw *get_cust_gyro(void)
{
	return &gyro_cust;
}

/*----------------------------------------------------------------------------*/
static int icm20645_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int icm20645_i2c_remove(struct i2c_client *client);
static int icm20645_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#if !defined(CONFIG_HAS_EARLYSUSPEND)
static int icm20645_suspend(struct i2c_client *client, pm_message_t msg);
static int icm20645_resume(struct i2c_client *client);
#endif
static int icm20645_local_init(struct platform_device *pdev);
static int icm20645_remove(void);
static int icm20645_init_flag = -1;
static struct gyro_init_info icm20645_init_info = {
	.name = "icm20645GY",
	.init = icm20645_local_init,
	.uninit = icm20645_remove,
};

/*----------------------------------------------------------------------------*/
enum {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0X08,
	GYRO_TRC_INFO = 0X10,
	GYRO_TRC_DATA = 0X20,
} GYRO_TRC;
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
	struct gyro_hw *hw;
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
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,gyro"},
	{},
};
#endif

static struct i2c_driver icm20645_i2c_driver = {
	.driver = {
		.name = ICM20645_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = gyro_of_match,
#endif

	},
	.probe = icm20645_i2c_probe,
	.remove = icm20645_i2c_remove,
	.detect = icm20645_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = icm20645_suspend,
	.resume = icm20645_resume,
#endif
	.id_table = icm20645_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *icm20645_i2c_client;
static struct icm20645_i2c_data *obj_i2c_data;
static bool sensor_power;

static unsigned int power_on;

int ICM20645_gyro_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(ICM20645_gyro_power);

int ICM20645_gyro_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(ICM20645_gyro_mode);

static int icm20645_set_bank(struct i2c_client *client, u8 bank)
{
	int res;
	u8 databuf[2];

	databuf[0] = bank;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_write_block(REG_BANK_SEL, databuf, 0x01);
	if (res < 0) {
		GYRO_LOG("icm20645_set_bank fail at %x\n", bank);
		return ICM20645_ERR_I2C;
	}
#else
	if (hwmsen_write_byte(client, REG_BANK_SEL, databuf)) {
		GYRO_LOG("icm20645_set_bank fail at %x\n", bank);
		return ICM20645_ERR_I2C;
	}
#endif

	return ICM20645_SUCCESS;
}

static int icm20645_lp_mode(struct i2c_client *client, bool on)
{
	int res;
	u8 databuf[2];

	memset(databuf, 0, sizeof(databuf));
	icm20645_set_bank(client, BANK_SEL_0);

/* gyroscope lp config */
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_read_block(ICM20645_REG_LP_CONFIG, databuf, 0x01);
	if (res < 0) {
		GYRO_LOG("icm20645_gyro_lp_mode fail at %x\n", on);
		return ICM20645_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, ICM20645_REG_LP_CONFIG, databuf)) {
		GYRO_LOG("icm20645_gyro_lp_mode fail at %x\n", on);
		return ICM20645_ERR_I2C;
	}
#endif
	if (on == true) {
		databuf[0] |= BIT_GYRO_LP_EN;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
		res = ICM20645_hwmsen_write_block(ICM20645_REG_LP_CONFIG, databuf, 0x01);
		if (res < 0) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#else
		if (hwmsen_write_byte(client, ICM20645_REG_LP_CONFIG, databuf)) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#endif
	} else {
		databuf[0] &= ~BIT_GYRO_LP_EN;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
		res = ICM20645_hwmsen_write_block(ICM20645_REG_LP_CONFIG, databuf, 0x01);
		if (res < 0) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#else
		if (hwmsen_write_byte(client, ICM20645_REG_LP_CONFIG, databuf)) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#endif
	}
	return ICM20645_SUCCESS;

}

static int icm20645_lowest_power_mode(struct i2c_client *client, bool on)
{
	int res;
	u8 databuf[2];

	memset(databuf, 0, sizeof(databuf));
	icm20645_set_bank(client, BANK_SEL_0);

/* all_chip_lp_config */
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_read_block(ICM20645_REG_PWR_CTL, databuf, 0x01);
	if (res < 0) {
		GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
		return ICM20645_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, ICM20645_REG_PWR_CTL, databuf)) {
		GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
		return ICM20645_ERR_I2C;
	}
#endif

	if (on == true) {
		databuf[0] |= BIT_LP_EN;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
		res = ICM20645_hwmsen_write_block(ICM20645_REG_PWR_CTL, databuf, 0x01);
		if (res < 0) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#else
		if (hwmsen_write_byte(client, ICM20645_REG_PWR_CTL, databuf)) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#endif
	} else {
		databuf[0] &= ~BIT_LP_EN;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
		res = ICM20645_hwmsen_write_block(ICM20645_REG_PWR_CTL, databuf, 0x01);
		if (res < 0) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#else
		if (hwmsen_write_byte(client, ICM20645_REG_PWR_CTL, databuf)) {
			GYRO_LOG("icm20645_lp_mode fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#endif

	}
	return ICM20645_SUCCESS;

}

static int icm20645_turn_on(struct i2c_client *client, u8 status, bool on)
{
	int res;
	u8 databuf[2];

	memset(databuf, 0, sizeof(databuf));
	icm20645_set_bank(client, BANK_SEL_0);
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_read_block(ICM20645_REG_POWER_CTL2, databuf, 0x01);
	if (res < 0) {
		GYRO_LOG("icm20645_turn_on fail at %x\n", on);
		return ICM20645_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, ICM20645_REG_POWER_CTL2, databuf)) {
		GYRO_LOG("icm20645_turn_on fail at %x\n", on);
		return ICM20645_ERR_I2C;
	}
#endif

	if (on == true) {
		databuf[0] &= ~status;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
		res = ICM20645_hwmsen_write_block(ICM20645_REG_POWER_CTL2, databuf, 0x01);
		if (res < 0) {
			GYRO_LOG("icm20645_turn_on fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#else
		if (hwmsen_write_byte(client, ICM20645_REG_POWER_CTL2, databuf)) {
			GYRO_LOG("icm20645_turn_on fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#endif
	} else {
		databuf[0] |= status;
#ifdef ICM20645_ACCESS_BY_GSE_I2C
		res = ICM20645_hwmsen_write_block(ICM20645_REG_POWER_CTL2, databuf, 0x01);
		if (res < 0) {
			GYRO_LOG("icm20645_turn_on fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#else
		if (hwmsen_write_byte(client, ICM20645_REG_POWER_CTL2, databuf)) {
			GYRO_LOG("icm20645_turn_on fail at %x\n", on);
			return ICM20645_ERR_I2C;
		}
#endif

	}
	return ICM20645_SUCCESS;

}
/*--------------------gyroscopy power control function----------------------------------*/
static void ICM20645_power(struct gyro_hw *hw, unsigned int on)
{
}
/*----------------------------------------------------------------------------*/
static int ICM20645_write_rel_calibration(struct icm20645_i2c_data *obj, int dat[ICM20645_AXES_NUM])
{
	obj->cali_sw[ICM20645_AXIS_X] = obj->cvt.sign[ICM20645_AXIS_X] * dat[obj->cvt.map[ICM20645_AXIS_X]];
	obj->cali_sw[ICM20645_AXIS_Y] = obj->cvt.sign[ICM20645_AXIS_Y] * dat[obj->cvt.map[ICM20645_AXIS_Y]];
	obj->cali_sw[ICM20645_AXIS_Z] = obj->cvt.sign[ICM20645_AXIS_Z] * dat[obj->cvt.map[ICM20645_AXIS_Z]];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
			 obj->cvt.sign[ICM20645_AXIS_X], obj->cvt.sign[ICM20645_AXIS_Y], obj->cvt.sign[ICM20645_AXIS_Z],
			 dat[ICM20645_AXIS_X], dat[ICM20645_AXIS_Y], dat[ICM20645_AXIS_Z],
			 obj->cvt.map[ICM20645_AXIS_X], obj->cvt.map[ICM20645_AXIS_Y], obj->cvt.map[ICM20645_AXIS_Z]);
		GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n",
			 obj->cali_sw[ICM20645_AXIS_X], obj->cali_sw[ICM20645_AXIS_Y], obj->cali_sw[ICM20645_AXIS_Z]);
	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ResetCalibration(struct i2c_client *client)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadCalibration(struct i2c_client *client, int dat[ICM20645_AXES_NUM])
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[ICM20645_AXIS_X]] = obj->cvt.sign[ICM20645_AXIS_X] * obj->cali_sw[ICM20645_AXIS_X];
	dat[obj->cvt.map[ICM20645_AXIS_Y]] = obj->cvt.sign[ICM20645_AXIS_Y] * obj->cali_sw[ICM20645_AXIS_Y];
	dat[obj->cvt.map[ICM20645_AXIS_Z]] = obj->cvt.sign[ICM20645_AXIS_Z] * obj->cali_sw[ICM20645_AXIS_Z];

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI)
		GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n",
			 dat[ICM20645_AXIS_X], dat[ICM20645_AXIS_Y], dat[ICM20645_AXIS_Z]);
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int ICM20645_WriteCalibration(struct i2c_client *client, int dat[ICM20645_AXES_NUM])
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[ICM20645_AXES_NUM];

	if (!obj || !dat) {
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	}
	cali[obj->cvt.map[ICM20645_AXIS_X]] = obj->cvt.sign[ICM20645_AXIS_X] * obj->cali_sw[ICM20645_AXIS_X];
	cali[obj->cvt.map[ICM20645_AXIS_Y]] = obj->cvt.sign[ICM20645_AXIS_Y] * obj->cali_sw[ICM20645_AXIS_Y];
	cali[obj->cvt.map[ICM20645_AXIS_Z]] = obj->cvt.sign[ICM20645_AXIS_Z] * obj->cali_sw[ICM20645_AXIS_Z];
	cali[ICM20645_AXIS_X] += dat[ICM20645_AXIS_X];
	cali[ICM20645_AXIS_Y] += dat[ICM20645_AXIS_Y];
	cali[ICM20645_AXIS_Z] += dat[ICM20645_AXIS_Z];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI)
		GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
			 dat[ICM20645_AXIS_X], dat[ICM20645_AXIS_Y], dat[ICM20645_AXIS_Z],
			 cali[ICM20645_AXIS_X], cali[ICM20645_AXIS_Y], cali[ICM20645_AXIS_Z]);
#endif
	return ICM20645_write_rel_calibration(obj, cali);

	return err;
}


static int ICM20645_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	icm20645_set_bank(client, BANK_SEL_0);

	if (enable == sensor_power) {
		GYRO_LOG("Sensor power status is newest!\n");
		return ICM20645_SUCCESS;
	}
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_read_block(ICM20645_REG_PWR_CTL, databuf, 0x01);
	if (res < 0) {
		GYRO_ERR("read power ctl register err!\n");
		return ICM20645_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, ICM20645_REG_PWR_CTL, databuf)) {
		GYRO_ERR("read power ctl register err!\n");
		return ICM20645_ERR_I2C;
	}
#endif

	databuf[0] &= ~ICM20645_SLEEP;
	if (enable == FALSE) {
		if (ICM20645_gse_mode() == false) {
			databuf[0] |= ICM20645_SLEEP;
		} else {
			res = icm20645_lowest_power_mode(client, true);
			if (res != 0) {
				GYRO_ERR("icm20645gy_lowest_power_mode error\n\r");
				return res;
			}
		}
	}

#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_write_block(ICM20645_REG_PWR_CTL, databuf, 0x1);
#else
	databuf[1] = databuf[0];
	databuf[0] = ICM20645_REG_PWR_CTL;
	res = i2c_master_send(client, databuf, 0x2);
#endif

	if (res < 0) {
		GYRO_LOG("set power mode failed!\n");
		return ICM20645_ERR_I2C;
	}
	GYRO_LOG("set power mode ok %d!\n", enable);

	sensor_power = enable;

	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	icm20645_set_bank(client, BANK_SEL_2);

#ifdef ICM20645_ACCESS_BY_GSE_I2C
	databuf[0] = dataformat;
	res = ICM20645_hwmsen_write_block(ICM20645_REG_CFG, databuf, 0x1);
#else
	databuf[0] = ICM20645_REG_CFG;
	databuf[1] = dataformat;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res < 0) {
		GYRO_ERR("ICM20645_SetDataFormat ERR : 0x%x\n", databuf[0]);
		return ICM20645_ERR_I2C;
	}

	udelay(500);
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_read_block(ICM20645_REG_CFG, databuf, 0x01);
	if (res < 0) {
		GYRO_ERR("read data format register err!\n");
		return ICM20645_ERR_I2C;
	}
	GYRO_LOG("read  data format: 0x%x\n", databuf[0]);
#else
	if (hwmsen_read_byte(client, ICM20645_REG_CFG, databuf)) {
		GYRO_ERR("read data format register err!\n");
		return ICM20645_ERR_I2C;
	}
	GYRO_LOG("read  data format: 0x%x\n", databuf[0]);
#endif

	icm20645_set_bank(client, BANK_SEL_0);

	return ICM20645_SUCCESS;
}

static int ICM20645_Setfilter(struct i2c_client *client, int filter_sample)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	icm20645_set_bank(client, BANK_SEL_2);

#ifdef ICM20645_ACCESS_BY_GSE_I2C
	databuf[0] = filter_sample;
	res = ICM20645_hwmsen_write_block(ICM20645_GYRO_CFG2, databuf, 0x1);
#else
	databuf[0] = ICM20645_GYRO_CFG2;
	databuf[1] = filter_sample;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res < 0) {
		GYRO_ERR("write sample rate register err!\n");
		return ICM20645_ERR_I2C;
	}
	icm20645_set_bank(client, BANK_SEL_0);
	return ICM20645_SUCCESS;
}


static int ICM20645_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	if (sample_rate >= 500)
		databuf[0] = 0;
	else
		databuf[0] = 1125 / sample_rate - 1;

	icm20645_set_bank(client, BANK_SEL_2);

#ifdef ICM20645_ACCESS_BY_GSE_I2C
	res = ICM20645_hwmsen_write_block(ICM20645_REG_SAMRT_DIV, databuf, 0x1);
#else
	databuf[1] = databuf[0];
	databuf[0] = ICM20645_REG_SAMRT_DIV;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res < 0) {
		GYRO_ERR("write sample rate register err!\n");
		return ICM20645_ERR_I2C;
	}

	icm20645_set_bank(client, BANK_SEL_0);
	return ICM20645_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ICM20645_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];
	int data[3];
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);

	if (sensor_power == false) {
		ICM20645_SetPowerMode(client, true);
		icm20645_turn_on(client, BIT_PWR_GYRO_STBY, true);
		msleep(50);
	}
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	if (ICM20645_hwmsen_read_block(ICM20645_REG_GYRO_XH, databuf, 6)) {
		GYRO_ERR("ICM20645 read gyroscope data  error\n");
		return -2;
	}
#endif

	obj->data[ICM20645_AXIS_X] =
	    ((s16) ((databuf[ICM20645_AXIS_X * 2 + 1]) | (databuf[ICM20645_AXIS_X * 2] << 8)));
	obj->data[ICM20645_AXIS_Y] =
	    ((s16) ((databuf[ICM20645_AXIS_Y * 2 + 1]) | (databuf[ICM20645_AXIS_Y * 2] << 8)));
	obj->data[ICM20645_AXIS_Z] =
	    ((s16) ((databuf[ICM20645_AXIS_Z * 2 + 1]) | (databuf[ICM20645_AXIS_Z * 2] << 8)));
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {
		GYRO_LOG("read gyro register: %d, %d, %d, %d, %d, %d",
			 databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
		GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
			 obj->data[ICM20645_AXIS_X], obj->data[ICM20645_AXIS_Y], obj->data[ICM20645_AXIS_Z],
			 obj->data[ICM20645_AXIS_X], obj->data[ICM20645_AXIS_Y], obj->data[ICM20645_AXIS_Z]);
	}
#endif
	obj->data[ICM20645_AXIS_X] = obj->data[ICM20645_AXIS_X] + obj->cali_sw[ICM20645_AXIS_X];
	obj->data[ICM20645_AXIS_Y] = obj->data[ICM20645_AXIS_Y] + obj->cali_sw[ICM20645_AXIS_Y];
	obj->data[ICM20645_AXIS_Z] = obj->data[ICM20645_AXIS_Z] + obj->cali_sw[ICM20645_AXIS_Z];

	/*remap coordinate */
	data[obj->cvt.map[ICM20645_AXIS_X]] = obj->cvt.sign[ICM20645_AXIS_X] * obj->data[ICM20645_AXIS_X];
	data[obj->cvt.map[ICM20645_AXIS_Y]] = obj->cvt.sign[ICM20645_AXIS_Y] * obj->data[ICM20645_AXIS_Y];
	data[obj->cvt.map[ICM20645_AXIS_Z]] = obj->cvt.sign[ICM20645_AXIS_Z] * obj->data[ICM20645_AXIS_Z];

	data[ICM20645_AXIS_X] = data[ICM20645_AXIS_X] * ICM20645_FS_MAX_LSB / ICM20645_DEFAULT_LSB;
	data[ICM20645_AXIS_Y] = data[ICM20645_AXIS_Y] * ICM20645_FS_MAX_LSB / ICM20645_DEFAULT_LSB;
	data[ICM20645_AXIS_Z] = data[ICM20645_AXIS_Z] * ICM20645_FS_MAX_LSB / ICM20645_DEFAULT_LSB;


	sprintf(buf, "%04x %04x %04x", data[ICM20645_AXIS_X], data[ICM20645_AXIS_Y], data[ICM20645_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
#endif

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
static int ICM20645_ReadAllReg(struct i2c_client *client, char *buf, int bufsize)
{
	u8 total_len = 8;

	u8 addr = 0;
	u8 buff[total_len + 1];
	int err = 0;
	int i;

	if (sensor_power == FALSE) {
		err = ICM20645_SetPowerMode(client, true);
		if (err)
			GYRO_ERR("Power on mpu6050 error %d!\n", err);
		msleep(50);
	}

	icm20645_set_bank(client, BANK_SEL_0);
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	if (ICM20645_hwmsen_read_block(addr, buff, total_len)) {
		GYRO_ERR("ICM20645_ReadAllReg err!\n");
		return ICM20645_ERR_I2C;
	}
#endif

	for (i = 0; i <= total_len; i++)
		GYRO_ERR("ICM20645 bank0 reg=0x%x, data=0x%x\n", (addr + i), buff[i]);
	icm20645_set_bank(client, BANK_SEL_2);
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	if (ICM20645_hwmsen_read_block(addr, buff, total_len)) {
		GYRO_ERR("ICM20645_ReadAllReg err!\n");
		return ICM20645_ERR_I2C;
	}
#endif

	for (i = 0; i <= total_len; i++)
		GYRO_ERR("ICM20645 bank2 reg=0x%x, data=0x%x\n", (addr + i), buff[i]);
	icm20645_set_bank(client, BANK_SEL_0);

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = icm20645_i2c_client;
	char strbuf[ICM20645_BUFSIZE];

	if (NULL == client) {
		GYRO_ERR("i2c client is null!!\n");
		return 0;
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
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	ICM20645_ReadGyroData(client, strbuf, ICM20645_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct icm20645_i2c_data *obj = obj_i2c_data;

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
	struct icm20645_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct icm20645_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw)
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	return len;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct gyro_hw *_ptAccelHw = hw;

	GYRO_LOG("[%s] default direction: %d\n", __func__, _ptAccelHw->direction);

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
			GYRO_ERR("ERR: fail to set direction\n");
	}

	GYRO_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *ICM20645_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
	&driver_attr_orientation,
};

/*----------------------------------------------------------------------------*/
static int icm20645_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ICM20645_attr_list) / sizeof(ICM20645_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ICM20645_attr_list[idx]);
		if (0 != err) {
			GYRO_ERR("driver_create_file (%s) = %d\n", ICM20645_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int icm20645_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ICM20645_attr_list) / sizeof(ICM20645_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, ICM20645_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int icm20645_gpio_config(void)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;

	pinctrl = devm_pinctrl_get(&gyroPltFmDev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		GYRO_ERR("Cannot find gyro pinctrl!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		GYRO_ERR("Cannot find gyro pinctrl default!\n");
	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		GYRO_ERR("Cannot find gyro pinctrl pin_cfg!\n");
		return ret;
	}
	pinctrl_select_state(pinctrl, pins_cfg);

	return 0;
}

static int icm20645_init_client(struct i2c_client *client, bool enable)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	res = icm20645_gpio_config();
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("icm20645_gpio_config ERR!\n");
		return res;
	}
	res = ICM20645_SetPowerMode(client, true);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("ICM20645_SetPowerMode ERR!\n");
		return res;
	}
	res = icm20645_turn_on(client, BIT_PWR_GYRO_STBY, true);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("icm20645_turn_on ERR!\n");
		return res;
	}
	res = icm20645_lp_mode(client, true);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("icm20645_lp_mode ERR!\n");
		return res;
	}
	res = icm20645_lowest_power_mode(client, false);
	if (res != 0) {
		GYRO_ERR("icm20645gy_lowest_power_mode error\n\r");
		return res;
	}
	res = ICM20645_SetDataFormat(client, (GYRO_DLPFCFG | GYRO_FS_SEL | GYRO_FCHOICE));
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("ICM20645_SetDataFormat ERR!\n");
		return res;
	}
	/* this is used to cts gyroscope measurement test, this case will use 500hz frquency,
	 * so we calibrate sensor in factory use use 500hz and 1x filter to give raw data, if
	 * we use 100hz and 8x filter in factory calibration will lead raw data not accurancy 
	 */
	res = ICM20645_Setfilter(client, GYRO_AVGCFG_1X);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("ICM20645_Setfilter ERR!\n");
		return res;
	}
	res = ICM20645_SetSampleRate(client, 500);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("ICM20645_SetSampleRate ERR!\n");
		return res;
	}
#ifdef CONFIG_ICM20645_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif
	res = icm20645_turn_on(client, BIT_PWR_GYRO_STBY, false);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("icm20645_turn_on ERR!\n");
		return res;
	}
	res = icm20645_lowest_power_mode(client, true);
	if (res != 0) {
		GYRO_ERR("icm20645gy_lowest_power_mode error\n\r");
		return res;
	}
	res = ICM20645_SetPowerMode(client, enable);
	if (res != ICM20645_SUCCESS) {
		GYRO_ERR("ICM20645_SetPowerMode ERR!\n");
		return res;
	}
	icm20645_set_bank(client, BANK_SEL_0);
	GYRO_LOG("icm20645_init_client OK!\n");

	return ICM20645_SUCCESS;
}


static int icm20645_open(struct inode *inode, struct file *file)
{
	file->private_data = icm20645_i2c_client;

	if (file->private_data == NULL) {
		GYRO_ERR("null pointer!!\n");
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
static long icm20645_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	char strbuf[ICM20645_BUFSIZE] = { 0 };
	void __user *data;
	long err = 0;
	struct SENSOR_DATA sensor_data;
	int cali[3];
	int smtRes = 0;
	int copy_cnt = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		GYRO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GYROSCOPE_IOCTL_INIT:
		icm20645_init_client(client, false);
		break;
	case GYROSCOPE_IOCTL_SMT_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		copy_cnt = copy_to_user(data, &smtRes, sizeof(smtRes));
		if (copy_cnt) {
			err = -EFAULT;
			GYRO_ERR("copy gyro data to user failed!\n");
		}
		err = 0;
		break;
	case GYROSCOPE_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		ICM20645_ReadGyroData(client, strbuf, ICM20645_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}

		else {
			cali[ICM20645_AXIS_X] = sensor_data.x * ICM20645_DEFAULT_LSB / ICM20645_FS_MAX_LSB;
			cali[ICM20645_AXIS_Y] = sensor_data.y * ICM20645_DEFAULT_LSB / ICM20645_FS_MAX_LSB;
			cali[ICM20645_AXIS_Z] = sensor_data.z * ICM20645_DEFAULT_LSB / ICM20645_FS_MAX_LSB;
			GYRO_LOG("gyro set cali:[%5d %5d %5d]\n",
				 cali[ICM20645_AXIS_X], cali[ICM20645_AXIS_Y], cali[ICM20645_AXIS_Z]);
			err = ICM20645_WriteCalibration(client, cali);
		}
		break;

	case GYROSCOPE_IOCTL_CLR_CALI:
		err = ICM20645_ResetCalibration(client);
		break;

	case GYROSCOPE_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = ICM20645_ReadCalibration(client, cali);
		if (err)
			break;

		sensor_data.x = cali[ICM20645_AXIS_X] * ICM20645_FS_MAX_LSB / ICM20645_DEFAULT_LSB;
		sensor_data.y = cali[ICM20645_AXIS_Y] * ICM20645_FS_MAX_LSB / ICM20645_DEFAULT_LSB;
		sensor_data.z = cali[ICM20645_AXIS_Z] * ICM20645_FS_MAX_LSB / ICM20645_DEFAULT_LSB;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		GYRO_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

#ifdef CONFIG_COMPAT
static long icm20645_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;


	switch (cmd) {
	case COMPAT_GYROSCOPE_IOCTL_INIT:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.\n");
			return ret;
		}

		break;

	default:
		GYRO_ERR("%s not supported = 0x%04x\n", __func__, cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
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
	.name = "gyroscope",
	.fops = &icm20645_fops,
};

static int icm20645_suspend(struct i2c_client *client, pm_message_t msg)
{
	int err = 0;
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);


	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GYRO_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		err = icm20645_turn_on(client, BIT_PWR_GYRO_STBY, false);
		if (err != ICM20645_SUCCESS) {
			GYRO_ERR("icm20645_turn_on ERR!\n");
			return err;
		}

		err = ICM20645_SetPowerMode(client, false);
		if (err < 0)
			return err;
	}
	return err;
}

static int icm20645_resume(struct i2c_client *client)
{
	struct icm20645_i2c_data *obj = i2c_get_clientdata(client);
	int err;


	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	ICM20645_power(obj->hw, 1);
	err = icm20645_init_client(client, false);
	if (err) {
		GYRO_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}

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
				GYRO_LOG("ICM20645_SetPowerMode done\n");
				break;
			}
			GYRO_LOG("ICM20645_SetPowerMode fail\n");
		}
		res = icm20645_turn_on(obj_i2c_data->client, BIT_PWR_GYRO_STBY, true);
		if (res != ICM20645_SUCCESS) {
			GYRO_ERR("icm20645_turn_on ERR!\n");
			return res;
		}
		res = icm20645_lowest_power_mode(obj_i2c_data->client, true);
		if (res != 0) {
			GYRO_ERR("icm20645gy_lowest_power_mode error\n\r");
			return res;
		}
	} else {
		res = icm20645_turn_on(obj_i2c_data->client, BIT_PWR_GYRO_STBY, false);
		if (res != ICM20645_SUCCESS) {
			GYRO_ERR("icm20645_turn_on ERR!\n");
			return res;
		}
		for (retry = 0; retry < 3; retry++) {
			res = ICM20645_SetPowerMode(obj_i2c_data->client, false);
			if (res == 0) {
				GYRO_LOG("ICM20645_SetPowerMode done\n");
				break;
			}
			GYRO_LOG("ICM20645_SetPowerMode fail\n");
		}
	}

	if (res != ICM20645_SUCCESS) {
		GYRO_LOG("ICM20645_SetPowerMode fail!\n");
		return -1;
	}
	GYRO_LOG("icm20645_enable_nodata OK!\n");
	return 0;

}

static int icm20645_set_delay(u64 ns)
{
	unsigned int hw_rate, delay_t, err;

	delay_t = ns / 1000 / 1000;
	hw_rate = 1000 / delay_t;

	err = ICM20645_SetPowerMode(obj_i2c_data->client, true);
	if (err < 0) {
		GYRO_LOG("ICM20645_SetPowerMode on fail\n\r");
		err = -1;
	}

	err = icm20645_lowest_power_mode(obj_i2c_data->client, false);
	if (err != 0) {
		GYRO_ERR("icm20645gy_lowest_power_mode error\n\r");
		err = -1;
	}

	if (hw_rate >= 500) {
		err = ICM20645_Setfilter(obj_i2c_data->client, GYRO_AVGCFG_1X);
		if (err != ICM20645_SUCCESS) {
			GYRO_ERR("ICM20645_Setfilter ERR!\n");
			return err;
		}
	} else if (hw_rate >= 100) {
		err = ICM20645_Setfilter(obj_i2c_data->client, GYRO_AVGCFG_4X);
		if (err != ICM20645_SUCCESS) {
			GYRO_ERR("ICM20645_Setfilter ERR!\n");
			return err;
		}
	 }

	err = ICM20645_SetSampleRate(obj_i2c_data->client, hw_rate);
	if (err != 0 ) {
		GYRO_ERR("icm20645gy_SetSampleRate error\n\r");
		err = -1;
	}

	err = icm20645_lowest_power_mode(obj_i2c_data->client, true);
	if (err != 0) {
		GYRO_ERR("icm20645gy_lowest_power_mode error\n\r");
		err = -1;
	}

	return 0;
}

static int icm20645_get_data(int *x, int *y, int *z, int *status)
{
	char buff[ICM20645_BUFSIZE];

	ICM20645_ReadGyroData(obj_i2c_data->client, buff, ICM20645_BUFSIZE);

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
	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct icm20645_i2c_data));

	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	GYRO_LOG("gyro_default_i2c_addr: %x\n", client->addr);
#ifdef ICM20645_ACCESS_BY_GSE_I2C
	obj->hw->addr = ICM20645_I2C_SLAVE_ADDR;	/* mtk i2c not allow to probe two same address */
#endif

	GYRO_LOG("gyro_custom_i2c_addr: %x\n", obj->hw->addr);
	if (0 != obj->hw->addr) {
		client->addr = obj->hw->addr >> 1;
		GYRO_LOG("gyro_use_i2c_addr: %x\n", client->addr);
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	icm20645_i2c_client = new_client;
	err = icm20645_init_client(new_client, false);
	if (err)
		goto exit_init_failed;

	err = misc_register(&icm20645_device);
	if (err) {
		GYRO_ERR("icm20645_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
	ctl.is_use_common_factory = false;

	err = icm20645_create_attr(&(icm20645_init_info.platform_diver_addr->driver));
	if (err) {
		GYRO_ERR("icm20645 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = icm20645_open_report_data;
	ctl.enable_nodata = icm20645_enable_nodata;
	ctl.set_delay = icm20645_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = gyro_register_control_path(&ctl);
	if (err) {
		GYRO_ERR("register gyro control path err\n");
		goto exit_kfree;
	}

	data.get_data = icm20645_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		GYRO_ERR("gyro_register_data_path fail = %d\n", err);
		goto exit_kfree;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend = icm20645_early_suspend,
	obj->early_drv.resume = icm20645_late_resume, register_early_suspend(&obj->early_drv);
#endif

	icm20645_init_flag = 0;

	GYRO_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&icm20645_device);
exit_misc_device_register_failed:
exit_init_failed:
	/*i2c_detach_client(new_client);*/
exit_kfree:
	kfree(obj);
exit:
	icm20645_init_flag = -1;
	GYRO_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int icm20645_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = icm20645_delete_attr(&(icm20645_init_info.platform_diver_addr->driver));
	if (err)
		GYRO_ERR("icm20645_delete_attr fail: %d\n", err);

	err = misc_deregister(&icm20645_device);
	if (err)
		GYRO_ERR("misc_deregister fail: %d\n", err);

	icm20645_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int icm20645_remove(void)
{
	ICM20645_power(hw, 0);
	i2c_del_driver(&icm20645_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int icm20645_local_init(struct platform_device *pdev)
{
	gyroPltFmDev = pdev;

	ICM20645_power(hw, 1);
	if (i2c_add_driver(&icm20645_i2c_driver)) {
		GYRO_ERR("add driver error\n");
		return -1;
	}
	if (-1 == icm20645_init_flag)
		return -1;
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init icm20645_init(void)
{
	const char *name = "mediatek,icm20645gy";

	hw = get_gyro_dts_func(name, hw);
	if (!hw)
		GYRO_ERR("get dts info fail\n");
	gyro_driver_add(&icm20645_init_info);

	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit icm20645_exit(void)
{
}

/*----------------------------------------------------------------------------*/
module_init(icm20645_init);
module_exit(icm20645_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ICM20645 gyroscope driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");

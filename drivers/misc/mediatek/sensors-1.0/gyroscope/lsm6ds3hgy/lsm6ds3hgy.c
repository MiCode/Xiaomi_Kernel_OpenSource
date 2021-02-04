/* ST LSM6DS3H Accelerometer and Gyroscope sensor driver on MTK platform
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

#include "lsm6ds3hgy.h"
#include "lsm6ds3hg_API.h"
#include <cust_gyro.h>
#include <gyroscope.h>



#define POWER_NONE_MACRO
#define LSM6DS3H_GYRO_NEW_ARCH



/*---------------------------------------------------------------------------*/
#define DEBUG 1
/* #define GYRO_UT */
/*----------------------------------------------------------------------------*/
#define CONFIG_LSM6DS3H_LOWPASS	/*apply low pass filter on output */
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH         (32)
#define LSM6DS3H_AXIS_X          0
#define LSM6DS3H_AXIS_Y          1
#define LSM6DS3H_AXIS_Z          2

#define LSM6DS3H_GYRO_AXES_NUM       3
#define LSM6DS3H_GYRO_DATA_LEN       6
#define LSM6DS3H_GYRO_DEV_NAME        "LSM6DS3H_GYRO"
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
enum enum_ADX_TRC {
	ADX_TRC_FILTER = 0x01,
	ADX_TRC_RAWDATA = 0x02,
	ADX_TRC_IOCTL = 0x04,
	ADX_TRC_CALI = 0X08,
	ADX_TRC_INFO = 0X10,
};
#define ADX_TRC enum enum_ADX_TRC
/*----------------------------------------------------------------------------*/
enum enum_GYRO_TRC {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0X08,
	GYRO_TRC_INFO = 0X10,
	GYRO_TRC_DATA = 0X20,
};
#define GYRO_TRC enum enum_GYRO_TRC
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
static const struct i2c_device_id lsm6ds3h_gyro_i2c_id[] = {
	{LSM6DS3H_GYRO_DEV_NAME, 0}, {} };


struct gyro_hw gyro_cust;
static struct gyro_hw *hw = &gyro_cust;
/* For  driver get cust info */
struct gyro_hw *get_cust_gyro(void)
{
	return &gyro_cust;
}

/*----------------------------------------------------------------------------*/

struct gyro_data_filter {
	s16 raw[C_MAX_FIR_LENGTH][LSM6DS3H_GYRO_AXES_NUM];
	int sum[LSM6DS3H_GYRO_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct lsm6ds3h_gyro_i2c_data {
	struct i2c_client *client;
	struct gyro_hw *hw;
	struct hwmsen_convert cvt;

	/*misc */
	/*struct data_resolution *reso;*/
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	atomic_t gyro_debounce;	/*debounce time after enabling ps */
	atomic_t gyro_deb_on;	/*indicates if the debounce is on */
	/*the jiffies representing the end of debounce */
	unsigned long gyro_deb_end;

	s32 cali_sw[LSM6DS3H_GYRO_AXES_NUM + 1];

	/*data */

	s8 offset[LSM6DS3H_GYRO_AXES_NUM + 1];	/*+1: for 4-byte alignment */
	s32 data[LSM6DS3H_GYRO_AXES_NUM + 1];
	int sensitivity;

#if defined(CONFIG_LSM6DS3H_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct gyro_data_filter fir;
#endif
};

/*----------------------------------------------------------------------------*/
static bool sensor_power = true;
static bool enable_status;
static int lsm6ds3h_gyro_init_flag = -1;
static struct i2c_client *lsm6ds3h_i2c_client;
static struct platform_driver lsm6ds3h_driver;
static struct lsm6ds3h_gyro_i2c_data *obj_i2c_data;




/*----------------------------------------------------------------------------*/
static int lsm6ds3h_gyro_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
/*static int lsm6ds3h_i2c_detect(struct i2c_client *client,
 * int kind, struct i2c_board_info *info);
 */
static int lsm6ds3h_gyro_i2c_remove(struct i2c_client *client);
static int lsm6ds3h_gyro_init_client(struct i2c_client *client, bool enable);
static int lsm6ds3h_gyro_resume(struct device *dev);
static int lsm6ds3h_gyro_suspend(struct device *dev);
/*----------------------------------------------------------------------------*/

static int lsm6ds3h_gyro_local_init(struct platform_device *pdev);
static int lsm6ds3h_gyro_local_uninit(void);

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,gyro"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops lsm6ds3h_gyro_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(lsm6ds3h_gyro_suspend, lsm6ds3h_gyro_resume)
};
#endif

static struct i2c_driver lsm6ds3h_gyro_i2c_driver = {
	.probe = lsm6ds3h_gyro_i2c_probe,
	.remove = lsm6ds3h_gyro_i2c_remove,
/*      .detect                         = lsm6ds3h_i2c_detect,*/

	.id_table = lsm6ds3h_gyro_i2c_id,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = LSM6DS3H_GYRO_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		   .pm = &lsm6ds3h_gyro_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = gyro_of_match,
#endif
		   },
};

static struct gyro_init_info lsm6ds3h_gyro_init_info = {
	.name = LSM6DS3H_GYRO_DEV_NAME,
	.init = lsm6ds3h_gyro_local_init,
	.uninit = lsm6ds3h_gyro_local_uninit,
};


/*----------------------------------------------------------------------------*/
#if 0
static void LSM6DS3H_dumpReg(struct i2c_client *client)
{
	int i = 0;
	u8 addr = 0x10;
	u8 regdata = 0;

	for (i = 0; i < 25; i++) {
		/*dump all*/
		hwmsen_read_byte(client, addr, &regdata);
		HWM_LOG("Reg addr=%x regdata=%x\n", addr, regdata);
		addr++;
	}
}
#endif
/*--------------------gyroscopy power control function-----------------*/
static void LSM6DS3H_power(struct gyro_hw *hw, unsigned int on)
{
	static unsigned int power_on;

	pr_debug("[gyro] power %s\n", on ? "on" : "off");

#if 0
	if (hw->power_id != POWER_NONE_MACRO) {	/* have externel LDO*/
		pr_debug("[gyro] power %s\n", on ? "on" : "off");
		if (power_on == on)	/* power status not change*/
			pr_debug("[gyro] ignore power control: %d\n", on);
		else if (on) {	/* power on*/
			if (!hwPowerOn(hw->power_id, hw->power_vol, "LSM6DS3H"))
				pr_info("[gyro] power on fails!!\n");
		} else {	/* power off*/
			if (!hwPowerDown(hw->power_id, "LSM6DS3H"))
				pr_info("[gyro] power off fail!!\n");
		}
	}
#endif
	power_on = on;
}

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_gyro_write_rel_calibration(
	struct lsm6ds3h_gyro_i2c_data *obj,
	int dat[LSM6DS3H_GYRO_AXES_NUM])
{
	obj->cali_sw[LSM6DS3H_AXIS_X] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * dat[obj->cvt.map[LSM6DS3H_AXIS_X]];
	obj->cali_sw[LSM6DS3H_AXIS_Y] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * dat[obj->cvt.map[LSM6DS3H_AXIS_Y]];
	obj->cali_sw[LSM6DS3H_AXIS_Z] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * dat[obj->cvt.map[LSM6DS3H_AXIS_Z]];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("[gyro] test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
			obj->cvt.sign[LSM6DS3H_AXIS_X],
			obj->cvt.sign[LSM6DS3H_AXIS_Y],
			obj->cvt.sign[LSM6DS3H_AXIS_Z], dat[LSM6DS3H_AXIS_X],
			dat[LSM6DS3H_AXIS_Y],
			dat[LSM6DS3H_AXIS_Z], obj->cvt.map[LSM6DS3H_AXIS_X],
			obj->cvt.map[LSM6DS3H_AXIS_Y],
			obj->cvt.map[LSM6DS3H_AXIS_Z]);
		pr_debug("[gyro] write gyro calibration data  (%5d, %5d, %5d)\n",
			obj->cali_sw[LSM6DS3H_AXIS_X],
			obj->cali_sw[LSM6DS3H_AXIS_Y],
			obj->cali_sw[LSM6DS3H_AXIS_Z]);
	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_gyro_ResetCalibration(struct i2c_client *client)
{
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_gyro_ReadCalibration(struct i2c_client *client,
	int dat[LSM6DS3H_GYRO_AXES_NUM])
{
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[LSM6DS3H_AXIS_X]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * obj->cali_sw[LSM6DS3H_AXIS_X];
	dat[obj->cvt.map[LSM6DS3H_AXIS_Y]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * obj->cali_sw[LSM6DS3H_AXIS_Y];
	dat[obj->cvt.map[LSM6DS3H_AXIS_Z]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * obj->cali_sw[LSM6DS3H_AXIS_Z];

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("[gyro] Read gyro calibration data  (%5d, %5d, %5d)\n",
			dat[LSM6DS3H_AXIS_X], dat[LSM6DS3H_AXIS_Y],
			dat[LSM6DS3H_AXIS_Z]);
	}
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int LSM6DS3H_gyro_WriteCalibration(struct i2c_client *client,
					  int dat[LSM6DS3H_GYRO_AXES_NUM])
{
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int cali[LSM6DS3H_GYRO_AXES_NUM];

	GYRO_FUN();
	if (!obj || !dat) {
		pr_info("[gyro] null ptr!!\n");
		return -EINVAL;
	}
	cali[obj->cvt.map[LSM6DS3H_AXIS_X]] =
		obj->cvt.sign[LSM6DS3H_AXIS_X] * obj->cali_sw[LSM6DS3H_AXIS_X];
	cali[obj->cvt.map[LSM6DS3H_AXIS_Y]] =
		obj->cvt.sign[LSM6DS3H_AXIS_Y] * obj->cali_sw[LSM6DS3H_AXIS_Y];
	cali[obj->cvt.map[LSM6DS3H_AXIS_Z]] =
		obj->cvt.sign[LSM6DS3H_AXIS_Z] * obj->cali_sw[LSM6DS3H_AXIS_Z];
	cali[LSM6DS3H_AXIS_X] += dat[LSM6DS3H_AXIS_X];
	cali[LSM6DS3H_AXIS_Y] += dat[LSM6DS3H_AXIS_Y];
	cali[LSM6DS3H_AXIS_Z] += dat[LSM6DS3H_AXIS_Z];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("[gyro] write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
			dat[LSM6DS3H_AXIS_X], dat[LSM6DS3H_AXIS_Y],
			dat[LSM6DS3H_AXIS_Z],
			cali[LSM6DS3H_AXIS_X], cali[LSM6DS3H_AXIS_Y],
			cali[LSM6DS3H_AXIS_Z]);
	}
#endif
	return LSM6DS3H_gyro_write_rel_calibration(obj, cali);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int LSM6DS3H_gyro_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;
	struct lsm6ds3h_gyro_i2c_data *priv = obj_i2c_data;

	if (enable == sensor_power) {
		pr_debug("[gyro] Sensor power status is newest!\n");
		return LSM6DS3H_SUCCESS;
	}
#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_read_block(LSM6DS3H_CTRL2_G, databuf, 0x01);
	if (res < 0) {
		pr_info("[gyro] read power ctl register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, LSM6DS3H_CTRL2_G, databuf)) {
		pr_info("[gyro] read lsm6ds3h power ctl register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#endif

	pr_debug("[gyro] LSM6DS3H_CTRL2_G:databuf[0] =  %x!\n", databuf[0]);
	if (true == enable) {
		/*clear lsm6ds3h gyro ODR bits*/
		databuf[0] &= ~LSM6DS3H_GYRO_ODR_MASK;
		/*default set 100HZ for LSM6DS3H gyro */
		databuf[0] |= LSM6DS3H_GYRO_ODR_104HZ;
/*databuf[0] |= LSM6DS3H_GYRO_ODR_208HZ; default set 100HZ for LSM6DS3H gyro */

	} else {
			/*clear lsm6ds3h gyro ODR bits*/
		databuf[0] &= ~LSM6DS3H_GYRO_ODR_MASK;
		databuf[0] |= LSM6DS3H_GYRO_ODR_POWER_DOWN;
	}
#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_write_block(LSM6DS3H_CTRL2_G, databuf, 0x1);
#else
	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3H_CTRL2_G;
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		pr_debug("[gyro] LSM6DS3H set power mode: ODR 100hz failed!\n");
		return LSM6DS3H_ERR_I2C;
	}

	if (enable == true) {
		atomic_set(&priv->gyro_deb_on, 1);
		priv->gyro_deb_end = jiffies + msecs_to_jiffies(
					atomic_read(&priv->gyro_debounce));
	} else
		atomic_set(&priv->gyro_deb_on, 0);

	pr_debug("[gyro] set LSM6DS3H gyro power mode:ODR 100HZ ok %d!\n",
		enable);


	sensor_power = enable;

	return LSM6DS3H_SUCCESS;
}

static int LSM6DS3H_Set_RegInc(struct i2c_client *client, bool inc)
{
	u8 databuf[2] = { 0 };
	int res = 0;
	/*GYRO_FUN();*/

#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_read_block(LSM6DS3H_CTRL3_C, databuf, 0x1);
	if (res < 0) {
		pr_info("[gyro] 1 read LSM6DS3H_CTRL1_XL err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, LSM6DS3H_CTRL3_C, databuf)) {
		pr_info("[gyro] 2 read LSM6DS3H_CTRL1_XL err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#endif
	else
		pr_debug("[gyro] read  LSM6DS3H_CTRL1_XL register: 0x%x\n",
			databuf[0]);

	if (inc) {
		databuf[0] |= LSM6DS3H_CTRL3_C_IFINC;
#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
		res = LSM6DS3H_hwmsen_write_block(LSM6DS3H_CTRL3_C,
			databuf, 0x1);
#else

		databuf[1] = databuf[0];
		databuf[0] = LSM6DS3H_CTRL3_C;

		res = i2c_master_send(client, databuf, 0x2);
#endif
		if (res <= 0) {
			pr_info("[gyro] write full scale register err!\n");
			return LSM6DS3H_ERR_I2C;
		}
	}
	return LSM6DS3H_SUCCESS;
}

static int LSM6DS3H_gyro_SetFullScale(struct i2c_client *client, u8 gyro_fs)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GYRO_FUN();

#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_read_block(LSM6DS3H_CTRL2_G, databuf, 0x1);
	if (res < 0) {
		pr_info("[gyro] read LSM6DS3H_CTRL2_G err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, LSM6DS3H_CTRL2_G, databuf)) {
		pr_info("[gyro] read LSM6DS3H_CTRL2_G err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#endif
	else
		pr_debug("[gyro] read  LSM6DS3H_CTRL2_G register: 0x%x\n",
			databuf[0]);

/*clear ,why clear the bit0???? */
	databuf[0] &= ~LSM6DS3H_GYRO_RANGE_MASK;
	databuf[0] |= gyro_fs;
#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_write_block(LSM6DS3H_CTRL2_G, databuf, 0x1);
#else

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3H_CTRL2_G;

	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		pr_info("[gyro] write full scale register err!\n");
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}

/*----------------------------------------------------------------------------*/

/* set the gyro sample rate*/
static int LSM6DS3H_gyro_SetSampleRate(struct i2c_client *client,
	u8 sample_rate)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GYRO_FUN();

#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_read_block(LSM6DS3H_CTRL2_G, databuf, 0x1);
	if (res < 0) {
		pr_info("[gyro] read gyro data format register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#else
	if (hwmsen_read_byte(client, LSM6DS3H_CTRL2_G, databuf)) {
		pr_info("[gyro] read gyro data format register err!\n");
		return LSM6DS3H_ERR_I2C;
	}
#endif
	else
		pr_debug("[gyro] read  gyro data format register: 0x%x\n",
			databuf[0]);


	databuf[0] &= ~LSM6DS3H_GYRO_ODR_MASK;	/*clear */
	databuf[0] |= sample_rate;
#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	res = LSM6DS3H_hwmsen_write_block(LSM6DS3H_CTRL2_G, databuf, 1);
#else

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3H_CTRL2_G;

	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		pr_info("[gyro] write sample rate register err!\n");
		return LSM6DS3H_ERR_I2C;
	}

	return LSM6DS3H_SUCCESS;
}

s32 LSM6DS3H_gyro_TransfromResolution(s32 rawData)
{
	s64 tempValue;
	uint64_t tempPlusValue = 0;

	tempValue = (s64)(rawData) * LSM6DS3H_GYRO_SENSITIVITY_2000DPS
			* 3141592;
	if (tempValue < 0) {
		tempPlusValue = tempValue * -1;
		do_div(tempPlusValue, (180 * 1000 * 1000));
		tempValue = tempPlusValue * -1;
	} else {
		tempPlusValue = tempValue;
		do_div(tempPlusValue, (180 * 1000 * 1000));
		tempValue = tempPlusValue;
	}
	return (s32) tempValue;
}

#ifdef LSM6DS3H_SENSITIVITY_SCALING
int LSM6DS3H_gyro_scale(int data, int scale)
{
	s64 tempValue;
	uint64_t tempPlusValue = 0;

	tempValue = (s64) (data) * scale;


	if (tempValue < 0) {
		tempPlusValue = tempValue * -1;
		do_div(tempPlusValue, 1000000);
		tempValue = tempPlusValue * -1;
	} else {
		tempPlusValue = tempValue;
		do_div(tempPlusValue, 1000000);
		tempValue = tempPlusValue;
	}


	return (int)tempValue;
}
#endif


/*----------------------------------------------------------------------------*/
static int LSM6DS3H_ReadGyroData(struct i2c_client *client,
	char *buf, int bufsize)
{
	char databuf[6];
	int data[3];
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);

	if (sensor_power == false) {
		/*LSM6DS3H_gyro_SetPowerMode(client, true);*/
		dump_stack();
	}


	if (hwmsen_read_block(client, LSM6DS3H_OUTX_L_G, databuf, 6)) {
		pr_info("[gyro] LSM6DS3H read gyroscope data  error\n");
		return -2;
	}
	obj->data[LSM6DS3H_AXIS_X] =
	    (s16) ((databuf[LSM6DS3H_AXIS_X * 2 + 1] << 8) |
		   (databuf[LSM6DS3H_AXIS_X * 2]));
	obj->data[LSM6DS3H_AXIS_Y] =
	    (s16) ((databuf[LSM6DS3H_AXIS_Y * 2 + 1] << 8) |
		   (databuf[LSM6DS3H_AXIS_Y * 2]));
	obj->data[LSM6DS3H_AXIS_Z] =
	    (s16) ((databuf[LSM6DS3H_AXIS_Z * 2 + 1] << 8) |
		   (databuf[LSM6DS3H_AXIS_Z * 2]));

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {
		pr_debug("[gyro] read gyro register: %x, %x, %x, %x, %x, %x",
			 databuf[0], databuf[1], databuf[2],
			 databuf[3], databuf[4],
			 databuf[5]);
		pr_debug("[gyro] get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
			 obj->data[LSM6DS3H_AXIS_X],
			 obj->data[LSM6DS3H_AXIS_Y],
			 obj->data[LSM6DS3H_AXIS_Z],
			 obj->data[LSM6DS3H_AXIS_X],
			 obj->data[LSM6DS3H_AXIS_Y],
			 obj->data[LSM6DS3H_AXIS_Z]);
		pr_debug("[gyro] get gyro cali data (%5d, %5d, %5d)\n",
			 obj->cali_sw[LSM6DS3H_AXIS_X],
			 obj->cali_sw[LSM6DS3H_AXIS_Y],
			 obj->cali_sw[LSM6DS3H_AXIS_Z]);
	}
#endif
#if 1
/*obj->data is s16, assigned a intermediate multiplication
 * result directly will overflow
 */
	obj->data[LSM6DS3H_AXIS_X] =
	    LSM6DS3H_gyro_TransfromResolution(obj->data[LSM6DS3H_AXIS_X]);
	obj->data[LSM6DS3H_AXIS_Y] =
	    LSM6DS3H_gyro_TransfromResolution(obj->data[LSM6DS3H_AXIS_Y]);
	obj->data[LSM6DS3H_AXIS_Z] =
	    LSM6DS3H_gyro_TransfromResolution(obj->data[LSM6DS3H_AXIS_Z]);

	obj->data[LSM6DS3H_AXIS_X] += (obj->cali_sw[LSM6DS3H_AXIS_X] * 1000);
	obj->data[LSM6DS3H_AXIS_Y] += (obj->cali_sw[LSM6DS3H_AXIS_Y] * 1000);
	obj->data[LSM6DS3H_AXIS_Z] += (obj->cali_sw[LSM6DS3H_AXIS_Z] * 1000);

	/*remap coordinate */
	data[obj->cvt.map[LSM6DS3H_AXIS_X]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_X] * obj->data[LSM6DS3H_AXIS_X];
	data[obj->cvt.map[LSM6DS3H_AXIS_Y]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Y] * obj->data[LSM6DS3H_AXIS_Y];
	data[obj->cvt.map[LSM6DS3H_AXIS_Z]] =
	    obj->cvt.sign[LSM6DS3H_AXIS_Z] * obj->data[LSM6DS3H_AXIS_Z];
#else
	data[LSM6DS3H_AXIS_X] =
	    (s64) (data[LSM6DS3H_AXIS_X])
	    * LSM6DS3H_GYRO_SENSITIVITY_2000DPS * 3142 /
	    (180 * 1000 * 1000);
	data[LSM6DS3H_AXIS_Y] =
	    (s64) (data[LSM6DS3H_AXIS_Y])
	    * LSM6DS3H_GYRO_SENSITIVITY_2000DPS * 3142 /
	    (180 * 1000 * 1000);
	data[LSM6DS3H_AXIS_Z] =
	    (s64) (data[LSM6DS3H_AXIS_Z])
	    * LSM6DS3H_GYRO_SENSITIVITY_2000DPS * 3142 /
	    (180 * 1000 * 1000);
#endif


#ifdef LSM6DS3H_SENSITIVITY_SCALING
	data[LSM6DS3H_AXIS_X] =
	    LSM6DS3H_gyro_scale(data[LSM6DS3H_AXIS_X],
		LSM6DS3H_SENSITIVITY_SCALING_X);
	data[LSM6DS3H_AXIS_Y] =
	    LSM6DS3H_gyro_scale(data[LSM6DS3H_AXIS_Y],
		LSM6DS3H_SENSITIVITY_SCALING_Y);
	data[LSM6DS3H_AXIS_Z] =
	    LSM6DS3H_gyro_scale(data[LSM6DS3H_AXIS_Z],
		LSM6DS3H_SENSITIVITY_SCALING_Z);
#endif
	sprintf(buf, "%x %x %x", data[LSM6DS3H_AXIS_X], data[LSM6DS3H_AXIS_Y],
		data[LSM6DS3H_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		pr_debug("[gyro] get gyro data packet:[%d %d %d]\n",
			data[0], data[1], data[2]);

#endif

	return 0;

}

/*----------------------------------------------------------------------------*/
static int LSM6DS3H_ReadChipInfo(struct i2c_client *client,
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

	sprintf(buf, "LSM6DS3H Chip");
	return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3h_i2c_client;
	char strbuf[LSM6DS3H_BUFSIZE];

	if (client == NULL) {
		pr_info("[gyro] i2c client is null!!\n");
		return 0;
	}

	LSM6DS3H_ReadChipInfo(client, strbuf, LSM6DS3H_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3h_i2c_client;
	char strbuf[LSM6DS3H_BUFSIZE];

	if (client == NULL) {
		pr_info("[gyro] i2c client is null!!\n");
		return 0;
	}

	LSM6DS3H_ReadGyroData(client, strbuf, LSM6DS3H_BUFSIZE);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct lsm6ds3h_gyro_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[gyro] i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri,
	const char *buf, size_t count)
{
	int trace = 0;
	struct lsm6ds3h_gyro_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[gyro] i2c_data obj is null!!\n");
		return count;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		pr_info("[gyro] invalid content: '%s', length = %zu\n",
			buf, count);


	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct lsm6ds3h_gyro_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_info("[gyro] i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction,
				obj->hw->power_id,
				obj->hw->power_vol);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

/*----------------------------------------------------------------------------*/

static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
static DRIVER_ATTR(trace, 0644, show_trace_value, store_trace_value);




/*----------------------------------------------------------------------------*/
static struct driver_attribute *LSM6DS3H_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
};

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_create_attr(struct device_driver *driver)
{
	int idx = 0;
	int err = 0;
	int num = 0;

	num = (int)((sizeof(LSM6DS3H_attr_list))
			/ (sizeof(LSM6DS3H_attr_list[0])));
	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, LSM6DS3H_attr_list[idx]);
		if (err != 0) {
			pr_info("[gyro] driver_create_file (%s) = %d\n",
				    LSM6DS3H_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_delete_attr(struct device_driver *driver)
{
	int idx = 0;
	int err = 0;
	int num = (int)(ARRAY_SIZE(LSM6DS3H_attr_list));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, LSM6DS3H_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_gyro_init_client(struct i2c_client *client, bool enable)
{
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	/*GYRO_FUN();*/
	pr_debug("[gyro]  lsm6ds3h addr %x!\n", client->addr);
	/*res = LSM6DS3H_CheckDeviceID(client);*/
	/*if(res != LSM6DS3H_SUCCESS)*/
	/*{
	 *return res;
	 *}
	 */

	res = LSM6DS3H_gyro_SetPowerMode(client, enable);
	if (res != LSM6DS3H_SUCCESS)
		return res;


	res = LSM6DS3H_Set_RegInc(client, true);
	if (res != LSM6DS3H_SUCCESS)
		return res;

	/*we have only this choice*/
	res = LSM6DS3H_gyro_SetFullScale(client, LSM6DS3H_GYRO_RANGE_2000DPS);
	if (res != LSM6DS3H_SUCCESS)
		return res;


	/* res = LSM6DS3H_gyro_SetSampleRate(client, LSM6DS3H_GYRO_ODR_104HZ);
	 *if (res != LSM6DS3H_SUCCESS)
	 *	return res;
	 */


	pr_debug("[gyro] %s OK!\n", __func__);
	/*acc setting*/


#ifdef CONFIG_LSM6DS3H_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return LSM6DS3H_SUCCESS;
}

/*----------------------------------------------------------------------------*/

static int lsm6ds3h_gyro_open_report_data(int open)
{
	/*should queuq work to report event if  is_report_input_direct=true*/
	return 0;
}

/* if use  this typ of enable,
 * Gsensor only enabled but not report inputEvent to HAL
 */

static int lsm6ds3h_gyro_enable_nodata(int en)
{
	int value = en;
	int err = 0;
	struct lsm6ds3h_gyro_i2c_data *priv = obj_i2c_data;

	if (priv == NULL) {
		pr_info("[gyro] obj_i2c_data is NULL!\n");
		return -1;
	}

	if (value == 1)
		enable_status = true;
	else
		enable_status = false;

	pr_debug("[gyro] enable value=%d, sensor_power =%d\n",
		value, sensor_power);
	if (((value == 0) && (sensor_power == false)) || ((value == 1)
		&& (sensor_power == true)))
		pr_debug("[gyro] Gsensor device have updated!\n");
	else
		err = LSM6DS3H_gyro_SetPowerMode(priv->client, enable_status);


	pr_debug("[gyro] mc3xxx_enable_nodata OK!\n");
	return err;
}

static int lsm6ds3h_gyro_set_delay(u64 ns)
{
	int value = 0;
	int sample_delay;
	int err = 0;
	struct lsm6ds3h_gyro_i2c_data *priv = obj_i2c_data;

	value = (int)ns / 1000 / 1000;

	if (priv == NULL) {
		pr_info("[gyro] obj_i2c_data is NULL!\n");
		return -1;
	}


	if (value <= 5)
		sample_delay = LSM6DS3H_GYRO_ODR_208HZ;
	else if (value <= 10)
		sample_delay = LSM6DS3H_GYRO_ODR_104HZ;
	else
		sample_delay = LSM6DS3H_GYRO_ODR_52HZ;


	err = LSM6DS3H_gyro_SetSampleRate(priv->client, sample_delay);
	if (err != LSM6DS3H_SUCCESS)
		pr_info("[gyro] Set delay parameter error!\n");

#if 0
	if (value >= 50) {
		atomic_set(&priv->filter, 0);
	} else {
		priv->fir.num = 0;
		priv->fir.idx = 0;
		priv->fir.sum[LSM6DS3H_AXIS_X] = 0;
		priv->fir.sum[LSM6DS3H_AXIS_Y] = 0;
		priv->fir.sum[LSM6DS3H_AXIS_Z] = 0;
		atomic_set(&priv->filter, 1);
	}
#endif
	pr_debug("[gyro] set_delay (%d)\n", value);
	return 0;
}

static int gyroscope_batch(int flag, int64_t samplingPeriodNs,
	int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs / 1000 / 1000;

	pr_debug("[gyro] lsm6ds3hg gyro set delay = (%d) ok.\n", value);
	return lsm6ds3h_gyro_set_delay(samplingPeriodNs);
}

static int gyroscope_flush(void)
{
	return gyro_flush_report();
}

static int lsm6ds3h_gyro_get_data(int *x, int *y, int *z, int *status)
{
	char buff[LSM6DS3H_BUFSIZE];
	struct lsm6ds3h_gyro_i2c_data *priv = obj_i2c_data;

#ifdef GYRO_UT
	pr_debug("[gyro] %s (%d)\n", __func__, __LINE__);
#endif

	if (priv == NULL) {
		pr_info("[gyro] obj_i2c_data is NULL!\n");
		return -1;
	}

	if (atomic_read(&priv->gyro_deb_on) == 1) {
		unsigned long endt = priv->gyro_deb_end;

		if (time_after(jiffies, endt))
			atomic_set(&priv->gyro_deb_on, 0);

		if (atomic_read(&priv->gyro_deb_on) == 1)
			return -1;
	}

	memset(buff, 0, sizeof(buff));
	LSM6DS3H_ReadGyroData(priv->client, buff, LSM6DS3H_BUFSIZE);

	if (sscanf(buff, "%x %x %x", x, y, z) != 3)
		pr_debug("[gyro] get data format error\n");
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int lsm6ds3hg_factory_enable_sensor(bool enabledisable,
	int64_t sample_periods_ms)
{
	int err = 0;

#if 0
	err = bmi160_gyro_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		pr_info("[gyro] %s enable failed!\n", __func__);
		return -1;
	}
	err = bmi160_gyro_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_info("[gyro] %s set batch failed!\n", __func__);
		return -1;
	}
#endif
	err = lsm6ds3h_gyro_init_client(lsm6ds3h_i2c_client, enabledisable);
	if (err)
		pr_info("[gyro] %s init_client failed!\n", __func__);
	else
		msleep(350);
	return 0;
}

static int lsm6ds3hg_factory_get_data(int32_t data[3], int *status)
{
	int err = 0;
#if 0
	return bmi160_gyro_get_data(&data[0], &data[1], &data[2], status);
#endif
	if (false == sensor_power) {
		err = LSM6DS3H_gyro_SetPowerMode(lsm6ds3h_i2c_client, true);
		if (err != 0)
			pr_info("[gyro] MPU6515_SetPowerMode fail\n");
		msleep(350);
	}
	err = lsm6ds3h_gyro_get_data(&data[0], &data[1], &data[2], status);
	data[0] = (int32_t) (data[0] / 1000);
	data[1] = (int32_t) (data[1] / 1000);
	data[2] = (int32_t) (data[2] / 1000);
	return err;
}

static int lsm6ds3hg_factory_get_raw_data(int32_t data[3])
{
	char *strbuf;
#if 0
	GYRO_INFO("don't support bmg_factory_get_raw_data!\n");
#endif
	strbuf = kmalloc(LSM6DS3H_BUFSIZE, GFP_KERNEL);
	if (!strbuf)
		return -ENOMEM;

	LSM6DS3H_ReadGyroData(lsm6ds3h_i2c_client, strbuf, LSM6DS3H_BUFSIZE);
	if (sscanf(strbuf, "%x %x %x", &data[0], &data[1], &data[2]) != 3)
		pr_info("[gyro] sscanf parsing fail\n");

	kfree(strbuf);
	data[0] = (int32_t) (data[0] / 1000);
	data[1] = (int32_t) (data[1] / 1000);
	data[2] = (int32_t) (data[2] / 1000);

	return 0;
}

static int lsm6ds3hg_factory_enable_calibration(void)
{
	return 0;
}

static int lsm6ds3hg_factory_clear_cali(void)
{
	int err = 0;

	err = LSM6DS3H_gyro_ResetCalibration(lsm6ds3h_i2c_client);
	if (err) {
		GYRO_INFO("lsm6ds3hg_ResetCalibration failed!\n");
		return -1;
	}
	return 0;
}

static int lsm6ds3hg_factory_set_cali(int32_t data[3])
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
	cali[LSM6DS3H_AXIS_X] = data[0];
	cali[LSM6DS3H_AXIS_Y] = data[1];
	cali[LSM6DS3H_AXIS_Z] = data[2];
	pr_debug("[gyro] gyro set cali:[%5d %5d %5d]\n", cali[LSM6DS3H_AXIS_X],
		 cali[LSM6DS3H_AXIS_Y], cali[LSM6DS3H_AXIS_Z]);
	err = LSM6DS3H_gyro_WriteCalibration(lsm6ds3h_i2c_client, cali);
	if (err) {
		GYRO_INFO("lsm6ds3hg_WriteCalibration failed!\n");
		return -1;
	}

	return 0;
}

static int lsm6ds3hg_factory_get_cali(int32_t data[3])
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
	err = LSM6DS3H_gyro_ReadCalibration(lsm6ds3h_i2c_client, cali);
	if (err) {
		GYRO_INFO("lsm6ds3hg_ReadCalibration failed!\n");
		return -1;
	}
	data[0] = cali[LSM6DS3H_AXIS_X];
	data[1] = cali[LSM6DS3H_AXIS_Y];
	data[2] = cali[LSM6DS3H_AXIS_Z];

	return 0;
}

static int lsm6ds3hg_factory_do_self_test(void)
{
	return 0;
}

static struct gyro_factory_fops lsm6ds3hg_factory_fops = {
	.enable_sensor = lsm6ds3hg_factory_enable_sensor,
	.get_data = lsm6ds3hg_factory_get_data,
	.get_raw_data = lsm6ds3hg_factory_get_raw_data,
	.enable_calibration = lsm6ds3hg_factory_enable_calibration,
	.clear_cali = lsm6ds3hg_factory_clear_cali,
	.set_cali = lsm6ds3hg_factory_set_cali,
	.get_cali = lsm6ds3hg_factory_get_cali,
	.do_self_test = lsm6ds3hg_factory_do_self_test,
};

static struct gyro_factory_public lsm6ds3hg_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &lsm6ds3hg_factory_fops,
};

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#if 0
int LSM6DS3H_gyro_operate(void *self, uint32_t command,
	void *buff_in, int size_in,
	void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct lsm6ds3h_gyro_i2c_data *priv =
		(struct lsm6ds3h_gyro_i2c_data *)self;
	hwm_sensor_data *gyro_data;
	char buff[LSM6DS3H_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_info("[gyro] Set delay parameter error!\n");
			err = -EINVAL;
		} else {

		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_info("[gyro] Enable gyroscope parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value == 1)
				enable_status = true;
			else
				enable_status = false;

			pr_debug("[gyro] enable value=%d, sensor_power =%d\n",
				value, sensor_power);
			if (((value == 0) && (sensor_power == false))
			    || ((value == 1) && (sensor_power == true))) {
				pr_debug("[gyro] Gsensor device have updated!\n");
			} else {
				err = LSM6DS3H_gyro_SetPowerMode(priv->client,
					enable_status);
			}

		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL)
			|| (size_out < sizeof(hwm_sensor_data))) {
			pr_info("[gyro] get gyroscope data parameter error!\n");
			err = -EINVAL;
		} else {
			gyro_data = (hwm_sensor_data *) buff_out;
			LSM6DS3H_ReadGyroData(priv->client, buff,
				LSM6DS3H_BUFSIZE);
			if (sscanf(buff, "%x %x %x", &gyro_data->values[0],
			       &gyro_data->values[1],
			       &gyro_data->values[2]) != 3)
				pr_debug("[gyro] get data format error\n");
			gyro_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			gyro_data->value_divide = 1000;
		}
		break;
	default:
		pr_info("[gyro] gyroscope operate function no this parameter %d!\n",
			command);
		err = -1;
		break;
	}

	return err;
}
#endif

#if 0
/******************************************************************************
 * Function Configuration
 ******************************************************************************/
static int lsm6ds3h_open(struct inode *inode, struct file *file)
{
	file->private_data = lsm6ds3h_i2c_client;

	if (file->private_data == NULL) {
		pr_info("[gyro] null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long lsm6ds3h_gyro_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_GYROSCOPE_IOCTL_INIT:
		if (arg32 == NULL) {
			pr_info("[gyro] invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT,
				(unsigned long)arg32);
		if (ret) {
			pr_info("[gyro] GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			pr_info("[gyro] invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI,
						 (unsigned long)arg32);
		if (ret) {
			pr_info("[gyro] GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			pr_info("[gyro] invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI,
						 (unsigned long)arg32);
		if (ret) {
			pr_info("[gyro] GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			pr_info("[gyro] invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI,
						 (unsigned long)arg32);
		if (ret) {
			pr_info("[gyro] GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			pr_info("[gyro] invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file,
				GYROSCOPE_IOCTL_READ_SENSORDATA,
				(unsigned long)arg32);
		if (ret) {
			pr_info("[gyro] GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}

		break;

	default:
		pr_info("[gyro] %s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;

	}
	return ret;
}
#endif

static long lsm6ds3h_gyro_unlocked_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
/*struct lsm6ds3h_gyro_i2c_data *obj =
 * (struct lsm6ds3h_gyro_i2c_data*)i2c_get_clientdata(client);
 */
	char strbuf[LSM6DS3H_BUFSIZE] = { 0 };
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	struct SENSOR_DATA sensor_data;
	int cali[3] = { 0 };
	int smtRes = 0;
	/*GYRO_FUN();*/

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
			_IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
			_IOC_SIZE(cmd));


	if (err) {
		pr_info("[gyro] access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GYROSCOPE_IOCTL_INIT:
		lsm6ds3h_gyro_init_client(client, false);
		break;

	case GYROSCOPE_IOCTL_SMT_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		pr_debug("[gyro] IOCTL smtRes: %d!\n", smtRes);
		copy_cnt = copy_to_user(data, &smtRes, sizeof(smtRes));

		if (copy_cnt) {
			err = -EFAULT;
			pr_info("[gyro] copy gyro data to user failed!\n");
		}
		pr_debug("[gyro] copy gyro data to user OK: %d!\n", copy_cnt);
		break;


	case GYROSCOPE_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (LSM6DS3H_ReadGyroData(client,
			strbuf, LSM6DS3H_BUFSIZE) == 0) {
			int x, y, z;

/*enlarged 100 times in kernel level, need to shrink it in factory case */
			if (sscanf(strbuf, "%x %x %x", &x, &y, &z) != 3)
				pr_info("[gyro] ioctl readGyro error!\n");
			sprintf(strbuf, "%x %x %x",
				x / 1000, y / 1000, z / 1000);
		}

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
	/* * 180*1000*1000/(LSM6DS3H_GYRO_SENSITIVITY_2000DPS*3142);*/
			cali[LSM6DS3H_AXIS_X] =
				(s64) (sensor_data.x);
	/* * 180*1000*1000/(LSM6DS3H_GYRO_SENSITIVITY_2000DPS*3142);*/
			cali[LSM6DS3H_AXIS_Y] =
				(s64) (sensor_data.y);
	/* * 180*1000*1000/(LSM6DS3H_GYRO_SENSITIVITY_2000DPS*3142);*/
			cali[LSM6DS3H_AXIS_Z] =
				(s64) (sensor_data.z);
			err = LSM6DS3H_gyro_WriteCalibration(client, cali);
		}
		break;

	case GYROSCOPE_IOCTL_CLR_CALI:
		err = LSM6DS3H_gyro_ResetCalibration(client);
		break;

	case GYROSCOPE_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = LSM6DS3H_gyro_ReadCalibration(client, cali);
		if (err)
			break;

	/* * LSM6DS3H_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);*/
		sensor_data.x =
			(s64) (cali[LSM6DS3H_AXIS_X]);
	/* * LSM6DS3H_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);*/
		sensor_data.y =
			(s64) (cali[LSM6DS3H_AXIS_Y]);
	/* * LSM6DS3H_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);*/
		sensor_data.z =
			(s64) (cali[LSM6DS3H_AXIS_Z]);

		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		pr_info("[gyro] unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}


/*----------------------------------------------------------------------------*/
static const struct file_operations lsm6ds3h_gyro_fops = {
	.owner = THIS_MODULE,
	.open = lsm6ds3h_open,
	.release = lsm6ds3h_release,
	.unlocked_ioctl = lsm6ds3h_gyro_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lsm6ds3h_gyro_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice lsm6ds3h_gyro_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &lsm6ds3h_gyro_fops,
};
#endif
/*----------------------------------------------------------------------------*/
static int lsm6ds3h_gyro_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		pr_info("[gyro] null pointer!!\n");
		return -1;
	}
	atomic_set(&obj->suspend, 1);
	err = LSM6DS3H_gyro_SetPowerMode(obj->client, false);
	if (err) {
		pr_info("[gyro] write power control fail!!\n");
		return err;
	}

	sensor_power = false;

	LSM6DS3H_power(obj->hw, 0);

	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_gyro_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lsm6ds3h_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	if (obj == NULL) {
		pr_info("[gyro] null pointer!!\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
		GYRO_FUN();

	LSM6DS3H_power(obj->hw, 1);

	err = LSM6DS3H_gyro_SetPowerMode(obj->client, enable_status);
	if (err) {
		pr_info("[gyro] initialize client fail! err code %d!\n", err);
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}

/*#endif*/
/*----------------------------------------------------------------------------*/
#if 0				/*CONFIG_HAS_EARLY_SUSPEND is defined */
/*----------------------------------------------------------------------------*/
static void lsm6ds3h_gyro_early_suspend(struct early_suspend *h)
{
	struct lsm6ds3h_gyro_i2c_data *obj =
	    container_of(h, struct lsm6ds3h_gyro_i2c_data, early_drv);
	int err;

	GYRO_FUN();

	if (obj == NULL) {
		pr_info("[gyro] null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = LSM6DS3H_gyro_SetPowerMode(obj->client, false);
	if (err) {
		pr_info("[gyro] write power control fail!!\n");
		return;
	}
	if (err <= 0)
		return;


	sensor_power = false;

	LSM6DS3H_power(obj->hw, 0);
}

/*----------------------------------------------------------------------------*/
static void lsm6ds3h_gyro_late_resume(struct early_suspend *h)
{
	struct lsm6ds3h_gyro_i2c_data *obj =
	    container_of(h, struct lsm6ds3h_gyro_i2c_data, early_drv);
	int err;

	GYRO_FUN();

	if (obj == NULL) {
		pr_info("[gyro] null pointer!!\n");
		return;
	}

	LSM6DS3H_power(obj->hw, 1);
	err = LSM6DS3H_gyro_SetPowerMode(obj->client, enable_status);
	if (err) {
		pr_info("[gyro] initialize client fail! err code %d!\n", err);
		return;
	}
	atomic_set(&obj->suspend, 0);
}

/*----------------------------------------------------------------------------*/
#endif				/*CONFIG_EARLYSUSPEND */
/*----------------------------------------------------------------------------*/
#if 0
static int lsm6ds3h_i2c_detect(struct i2c_client *client,
	int kind, struct i2c_board_info *info)
{
	strcpy(info->type, LSM6DS3H_GYRO_DEV_NAME);
	return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int lsm6ds3h_gyro_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;
	struct i2c_client *new_client;
	struct lsm6ds3h_gyro_i2c_data *obj;

	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };

	/*GYRO_FUN();*/
	err = get_gyro_dts_func(client->dev.of_node, hw);
	if (err) {
		pr_info("[gyro] get dts info fail\n");
		err = -EFAULT;
		goto exit;
	}
	LSM6DS3H_power(hw, 1);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct lsm6ds3h_gyro_i2c_data));

	obj->hw = hw;		/*get_cust_gyro_hw();*/

	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		pr_info("[gyro] invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}

	pr_debug("[gyro] gyro_default_i2c_addr: %x\n", client->addr);
#ifdef LSM6DS3H_ACCESS_BY_GSE_I2C
	obj->hw->addr = lsm6ds3h_acc_i2c_client->addr;
#endif

	pr_debug("[gyro] gyro_custom_i2c_addr: %x\n", obj->hw->addr);
	if (obj->hw->addr != 0) {
		client->addr = obj->hw->addr;
		pr_debug("[gyro] gyro_use_i2c_addr: %x\n", client->addr);
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
/*this sensor need about 120ms before data ready*/
	atomic_set(&obj->gyro_debounce, 200);
	atomic_set(&obj->gyro_deb_on, 0);
	obj->gyro_deb_end = 0;



	lsm6ds3h_i2c_client = new_client;
	err = lsm6ds3h_gyro_init_client(new_client, false);
	if (err)
		goto exit_init_failed;

#if 1
	/*err = misc_register(&lsm6ds3h_gyro_device); */
	err = gyro_factory_device_register(&lsm6ds3hg_factory_device);
	if (err) {
		pr_info("[gyro] lsm6ds3h_gyro_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
#endif

	ctl.is_use_common_factory = true;
	err = lsm6ds3h_create_attr(
		&(lsm6ds3h_gyro_init_info.platform_diver_addr->driver));
	if (err) {
		pr_info("[gyro] lsm6ds3h create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.open_report_data = lsm6ds3h_gyro_open_report_data;
	ctl.enable_nodata = lsm6ds3h_gyro_enable_nodata;
	/*ctl.set_delay = lsm6ds3h_gyro_set_delay; */
	ctl.batch = gyroscope_batch;
	ctl.flush = gyroscope_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = gyro_register_control_path(&ctl);
	if (err) {
		pr_info("[gyro] register gyro control path err\n");
		goto exit_kfree;
	}

	data.get_data = lsm6ds3h_gyro_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		pr_info("[gyro] gyro_register_data_path fail = %d\n", err);
		goto exit_kfree;
	}

	lsm6ds3h_gyro_init_flag = 0;

	pr_debug("[gyro] %s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	/*misc_deregister(&lsm6ds3h_gyro_device); */
exit_misc_device_register_failed:
exit_init_failed:
	/*i2c_detach_client(new_client);*/
exit_kfree:
	kfree(obj);
exit:
	lsm6ds3h_gyro_init_flag = -1;
	pr_info("[gyro] %s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3h_gyro_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	LSM6DS3H_power(hw, 0);

	err = lsm6ds3h_delete_attr(&lsm6ds3h_driver.driver);
	if (err)
		pr_info("[gyro] %s fail: %d\n", __func__, err);

#if 1
	/*misc_deregister(&lsm6ds3h_gyro_device); */
	err = gyro_factory_device_deregister(&lsm6ds3hg_factory_device);
#endif

	lsm6ds3h_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/

static int lsm6ds3h_gyro_local_init(struct platform_device *pdev)
{
	GYRO_FUN();


	if (i2c_add_driver(&lsm6ds3h_gyro_i2c_driver)) {
		pr_info("[gyro] add driver error\n");
		return -1;
	}
	if (lsm6ds3h_gyro_init_flag == -1) {
		pr_info("[gyro] %s init failed!\n", __func__);
		return -1;
	}
	return 0;
}

static int lsm6ds3h_gyro_local_uninit(void)
{
	/*struct gyro_hw *gy_hw = get_cust_gyro_hw();*/

	/* GYRO_FUN();*/
	LSM6DS3H_power(hw, 0);
	i2c_del_driver(&lsm6ds3h_gyro_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init lsm6ds3h_gyro_init(void)
{
	/*GYRO_FUN();*/
	gyro_driver_add(&lsm6ds3h_gyro_init_info);

	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit lsm6ds3h_gyro_exit(void)
{
	GYRO_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(lsm6ds3h_gyro_init);
module_exit(lsm6ds3h_gyro_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LSM6DS3Hh gyroscope driver");
MODULE_AUTHOR("Yue.Wu@mediatek.com");






/*------- LSM6DS3H --------------*/

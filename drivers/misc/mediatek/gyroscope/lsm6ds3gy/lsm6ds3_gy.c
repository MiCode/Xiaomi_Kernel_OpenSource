/* ST LSM6DS3 Accelerometer and Gyroscope sensor driver
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
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>

#include "lsm6ds3_gy.h"
#include <linux/kernel.h>
#include <cust_gyro.h>




#define LSM6DS3_GYRO_NEW_ARCH		/*kk and L compatialbe*/


#ifdef LSM6DS3_GYRO_NEW_ARCH
#include <gyroscope.h>
#endif

/*---------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_LSM6DS3_LOWPASS   /*apply low pass filter on output*/
/*----------------------------------------------------------------------------*/
#define LSM6DS3_AXIS_X          0
#define LSM6DS3_AXIS_Y          1
#define LSM6DS3_AXIS_Z          2

#define LSM6DS3_GYRO_AXES_NUM       3
#define LSM6DS3_GYRO_DATA_LEN       6
#define LSM6DS3_GYRO_DEV_NAME        "LSM6DS3_GYRO"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id lsm6ds3_gyro_i2c_id[] = {{LSM6DS3_GYRO_DEV_NAME, 0}, {} };
static struct i2c_board_info __initdata i2c_lsm6ds3_gyro = { I2C_BOARD_INFO(LSM6DS3_GYRO_DEV_NAME, 0x34)}; /*0xD4>>1 is right address*/

/* Maintain  cust info here */
struct gyro_hw gyro_cust;
/* For  driver get cust info */
struct gyro_hw *get_cust_gyro(void)
{
	return &gyro_cust;
}

struct platform_device *gyroPltFmDev;
/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lsm6ds3_gyro_i2c_remove(struct i2c_client *client);
static int LSM6DS3_gyro_init_client(struct i2c_client *client, bool enable);

#ifndef CONFIG_HAS_EARLYSUSPEND
static int lsm6ds3_gyro_resume(struct i2c_client *client);
static int lsm6ds3_gyro_suspend(struct i2c_client *client, pm_message_t msg);
#endif
/*----------------------------------------------------------------------------*/
typedef enum {
		ADX_TRC_FILTER  = 0x01,
		ADX_TRC_RAWDATA = 0x02,
		ADX_TRC_IOCTL   = 0x04,
		ADX_TRC_CALI	= 0X08,
		ADX_TRC_INFO	= 0X10,
} ADX_TRC;
/*----------------------------------------------------------------------------*/
typedef enum {
		GYRO_TRC_FILTER  = 0x01,
		GYRO_TRC_RAWDATA = 0x02,
		GYRO_TRC_IOCTL   = 0x04,
		GYRO_TRC_CALI	= 0X08,
		GYRO_TRC_INFO	= 0X10,
		GYRO_TRC_DATA	= 0X20,
} GYRO_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor {
		u8  whole;
		u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
		struct scale_factor scalefactor;
		int					sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/

struct gyro_data_filter {
		s16 raw[C_MAX_FIR_LENGTH][LSM6DS3_GYRO_AXES_NUM];
		int sum[LSM6DS3_GYRO_AXES_NUM];
		int num;
		int idx;
};
/*----------------------------------------------------------------------------*/
struct lsm6ds3_gyro_i2c_data {
		struct i2c_client *client;
		struct gyro_hw *hw;
		struct hwmsen_convert   cvt;

		/*misc*/
		/*struct data_resolution *reso;*/
		atomic_t                trace;
		atomic_t                suspend;
		atomic_t                selftest;
		atomic_t				filter;
		s16                     cali_sw[LSM6DS3_GYRO_AXES_NUM+1];

		/*data*/

		s8                      offset[LSM6DS3_GYRO_AXES_NUM+1];  /*+1: for 4-byte alignment*/
		s16                     data[LSM6DS3_GYRO_AXES_NUM+1];
	int					sensitivity;

#if defined(CONFIG_LSM6DS3_LOWPASS)
		atomic_t                firlen;
		atomic_t                fir_en;
		struct gyro_data_filter      fir;
#endif
		/*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
		struct early_suspend    early_drv;
#endif
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver lsm6ds3_gyro_i2c_driver = {
		.driver = {
		.owner			= THIS_MODULE,
		.name			= LSM6DS3_GYRO_DEV_NAME,
		},
	.probe				= lsm6ds3_gyro_i2c_probe,
	.remove				= lsm6ds3_gyro_i2c_remove,

#if !defined(CONFIG_HAS_EARLYSUSPEND)
		.suspend			= lsm6ds3_gyro_suspend,
		.resume				= lsm6ds3_gyro_resume,
#endif
	.id_table = lsm6ds3_gyro_i2c_id,
};
#ifdef LSM6DS3_GYRO_NEW_ARCH
static int lsm6ds3_gyro_local_init(struct platform_device *pdev);
static int lsm6ds3_gyro_local_uninit(void);
static int lsm6ds3_gyro_init_flag = -1;
static struct gyro_init_info  lsm6ds3_gyro_init_info = {

	.name	= LSM6DS3_GYRO_DEV_NAME,
	.init	= lsm6ds3_gyro_local_init,
	.uninit	= lsm6ds3_gyro_local_uninit,
};
#endif
/*----------------------------------------------------------------------------*/
static struct i2c_client *lsm6ds3_i2c_client;	/*initial in module_init*/

#ifndef LSM6DS3_GYRO_NEW_ARCH
static struct platform_driver lsm6ds3_driver;
#endif

static struct lsm6ds3_gyro_i2c_data *obj_i2c_data;	/*initial in module_init*/
static bool sensor_power;	/*initial in module_init*/
static bool enable_status;	/*initial in module_init*/


/*--------------------gyroscopy power control function----------------------------------*/
static void LSM6DS3_power(struct gyro_hw *hw, unsigned int on)
{
	static unsigned int power_on;	/*default = 0;*/

#if 0
	if (hw->power_id != POWER_NONE_MACRO) {		/* have externel LDO*/

		GYRO_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on) {	/* power status not change*/

			GYRO_LOG("ignore power control: %d\n", on);
		} else if (on) {	/* power on*/

			if (!hwPowerOn(hw->power_id, hw->power_vol, "LSM6DS3")) {

				GYRO_ERR("power on fails!!\n");
			}
		} else {	/* power off*/

			if (!hwPowerDown(hw->power_id, "LSM6DS3")) {

				GYRO_ERR("power off fail!!\n");
			}
		}
	}
#endif
	power_on = on;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_write_rel_calibration(struct lsm6ds3_gyro_i2c_data *obj, int dat[LSM6DS3_GYRO_AXES_NUM])
{
	obj->cali_sw[LSM6DS3_AXIS_X] = obj->cvt.sign[LSM6DS3_AXIS_X] * dat[obj->cvt.map[LSM6DS3_AXIS_X]];
	obj->cali_sw[LSM6DS3_AXIS_Y] = obj->cvt.sign[LSM6DS3_AXIS_Y] * dat[obj->cvt.map[LSM6DS3_AXIS_Y]];
	obj->cali_sw[LSM6DS3_AXIS_Z] = obj->cvt.sign[LSM6DS3_AXIS_Z] * dat[obj->cvt.map[LSM6DS3_AXIS_Z]];
#if DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {

			GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
				obj->cvt.sign[LSM6DS3_AXIS_X], obj->cvt.sign[LSM6DS3_AXIS_Y], obj->cvt.sign[LSM6DS3_AXIS_Z],
				dat[LSM6DS3_AXIS_X], dat[LSM6DS3_AXIS_Y], dat[LSM6DS3_AXIS_Z],
				obj->cvt.map[LSM6DS3_AXIS_X], obj->cvt.map[LSM6DS3_AXIS_Y], obj->cvt.map[LSM6DS3_AXIS_Z]);
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n",
				obj->cali_sw[LSM6DS3_AXIS_X], obj->cali_sw[LSM6DS3_AXIS_Y], obj->cali_sw[LSM6DS3_AXIS_Z]);
		}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_ResetCalibration(struct i2c_client *client)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_ReadCalibration(struct i2c_client *client, int dat[LSM6DS3_GYRO_AXES_NUM])
{
		struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

		dat[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X]*obj->cali_sw[LSM6DS3_AXIS_X];
		dat[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y]*obj->cali_sw[LSM6DS3_AXIS_Y];
		dat[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z]*obj->cali_sw[LSM6DS3_AXIS_Z];

#if DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {

			GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n",
				dat[LSM6DS3_AXIS_X], dat[LSM6DS3_AXIS_Y], dat[LSM6DS3_AXIS_Z]);
		}
#endif

		return 0;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_WriteCalibration(struct i2c_client *client, int dat[LSM6DS3_GYRO_AXES_NUM])
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[LSM6DS3_GYRO_AXES_NUM];


	GYRO_FUN();
	if (!obj || !dat) {

		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	} else {

		cali[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X] * obj->cali_sw[LSM6DS3_AXIS_X];
		cali[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y] * obj->cali_sw[LSM6DS3_AXIS_Y];
		cali[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z] * obj->cali_sw[LSM6DS3_AXIS_Z];
		cali[LSM6DS3_AXIS_X] += dat[LSM6DS3_AXIS_X];
		cali[LSM6DS3_AXIS_Y] += dat[LSM6DS3_AXIS_Y];
		cali[LSM6DS3_AXIS_Z] += dat[LSM6DS3_AXIS_Z];
#if DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {

			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
				dat[LSM6DS3_AXIS_X], dat[LSM6DS3_AXIS_Y], dat[LSM6DS3_AXIS_Z],
				cali[LSM6DS3_AXIS_X], cali[LSM6DS3_AXIS_Y], cali[LSM6DS3_AXIS_Z]);
		}
#endif
		return LSM6DS3_gyro_write_rel_calibration(obj, cali);
	}

	return err;
}
/*----------------------------------------------------------------------------*/
static int LSM6DS3_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);
	databuf[0] = LSM6DS3_FIXED_DEVID;

	res = hwmsen_read_byte(client, LSM6DS3_WHO_AM_I, databuf);
		GYRO_LOG(" LSM6DS3  id %x!\n", databuf[0]);
	if (databuf[0] != LSM6DS3_FIXED_DEVID) {

		return LSM6DS3_ERR_IDENTIFICATION;
	}

	if (res < 0) {

		return LSM6DS3_ERR_I2C;
	}

	return LSM6DS3_SUCCESS;
}

/*/----------------------------------------------------------------------------/*/
static int LSM6DS3_gyro_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};
	int res = 0;

	if (enable == sensor_power) {

		GYRO_LOG("Sensor power status is newest!\n");
		return LSM6DS3_SUCCESS;
	}

	if (hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf)) {

		GYRO_ERR("read lsm6ds3 power ctl register err!\n");
		return LSM6DS3_ERR_I2C;
	}


	if (true == enable) {

		databuf[0] &= ~LSM6DS3_GYRO_ODR_MASK;/*clear lsm6ds3 gyro ODR bits*/
		databuf[0] |= LSM6DS3_GYRO_ODR_104HZ; /*default set 100HZ for LSM6DS3 gyro*/


	} else {

		/* do nothing*/
		databuf[0] &= ~LSM6DS3_GYRO_ODR_MASK;/*clear lsm6ds3 gyro ODR bits*/
		databuf[0] |= LSM6DS3_GYRO_ODR_POWER_DOWN; /*POWER DOWN*/
	}
	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3_CTRL2_G;
	res = i2c_master_send(client, databuf, 0x2);
	if (res <= 0) {

		GYRO_LOG("LSM6DS3 set power mode: ODR 100hz failed!\n");
		return LSM6DS3_ERR_I2C;
	} else {

		GYRO_LOG("set LSM6DS3 gyro power mode:ODR 100HZ ok %d!\n", enable);
	}


	sensor_power = enable;

	return LSM6DS3_SUCCESS;
}

static int LSM6DS3_Set_RegInc(struct i2c_client *client, bool inc)
{
	u8 databuf[2] = {0};
	int res = 0;
	/*GYRO_FUN();     */

	if (hwmsen_read_byte(client, LSM6DS3_CTRL3_C, databuf)) {

		GYRO_ERR("read LSM6DS3_CTRL1_XL err!\n");
		return LSM6DS3_ERR_I2C;
	} else {

		GYRO_LOG("read  LSM6DS3_CTRL1_XL register: 0x%x\n", databuf[0]);
	}
	if (inc) {

		databuf[0] |= LSM6DS3_CTRL3_C_IFINC;

		databuf[1] = databuf[0];
		databuf[0] = LSM6DS3_CTRL3_C;


		res = i2c_master_send(client, databuf, 0x2);
		if (res <= 0) {

			GYRO_ERR("write full scale register err!\n");
			return LSM6DS3_ERR_I2C;
		}
	}
	return LSM6DS3_SUCCESS;
}

static int LSM6DS3_gyro_SetFullScale(struct i2c_client *client, u8 gyro_fs)
{
	u8 databuf[2] = {0};
	int res = 0;
	GYRO_FUN();

	if (hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf)) {

		GYRO_ERR("read LSM6DS3_CTRL2_G err!\n");
		return LSM6DS3_ERR_I2C;
	} else {

		GYRO_LOG("read  LSM6DS3_CTRL2_G register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= ~LSM6DS3_GYRO_RANGE_MASK;/*clear */
	databuf[0] |= gyro_fs;

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3_CTRL2_G;


	res = i2c_master_send(client, databuf, 0x2);
	if (res <= 0) {

		GYRO_ERR("write full scale register err!\n");
		return LSM6DS3_ERR_I2C;
	}
	return LSM6DS3_SUCCESS;
}

/*----------------------------------------------------------------------------*/

/* set the gyro sample rate*/
static int LSM6DS3_gyro_SetSampleRate(struct i2c_client *client, u8 sample_rate)
{
	u8 databuf[2] = {0};
	int res = 0;
	GYRO_FUN();

	res = LSM6DS3_gyro_SetPowerMode(client, true);	/*set Sample Rate will enable power and should changed power status*/
	if (res != LSM6DS3_SUCCESS) {

		return res;
	}

	if (hwmsen_read_byte(client, LSM6DS3_CTRL2_G, databuf)) {

		GYRO_ERR("read gyro data format register err!\n");
		return LSM6DS3_ERR_I2C;
	} else {

		GYRO_LOG("read  gyro data format register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= ~LSM6DS3_GYRO_ODR_MASK;/*clear */
	databuf[0] |= sample_rate;

	databuf[1] = databuf[0];
	databuf[0] = LSM6DS3_CTRL2_G;


	res = i2c_master_send(client, databuf, 0x2);
	if (res <= 0) {

		GYRO_ERR("write sample rate register err!\n");
		return LSM6DS3_ERR_I2C;
	}

	return LSM6DS3_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];
	int data[3];
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);

	if (sensor_power == false) {

		LSM6DS3_gyro_SetPowerMode(client, true);
	}

	if (hwmsen_read_block(client, LSM6DS3_OUTX_L_G, databuf, 6)) {

		GYRO_ERR("LSM6DS3 read gyroscope data  error\n");
		return -2;
	} else {

		obj->data[LSM6DS3_AXIS_X] = (s16)((databuf[LSM6DS3_AXIS_X*2+1] << 8) | (databuf[LSM6DS3_AXIS_X*2]));
		obj->data[LSM6DS3_AXIS_Y] = (s16)((databuf[LSM6DS3_AXIS_Y*2+1] << 8) | (databuf[LSM6DS3_AXIS_Y*2]));
		obj->data[LSM6DS3_AXIS_Z] = (s16)((databuf[LSM6DS3_AXIS_Z*2+1] << 8) | (databuf[LSM6DS3_AXIS_Z*2]));

#if DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {

			GYRO_LOG("read gyro register: %x, %x, %x, %x, %x, %x",
				databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
			GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
				obj->data[LSM6DS3_AXIS_X], obj->data[LSM6DS3_AXIS_Y], obj->data[LSM6DS3_AXIS_Z],
				obj->data[LSM6DS3_AXIS_X], obj->data[LSM6DS3_AXIS_Y], obj->data[LSM6DS3_AXIS_Z]);
			GYRO_LOG("get gyro cali data (%5d, %5d, %5d)\n",
				obj->cali_sw[LSM6DS3_AXIS_X], obj->cali_sw[LSM6DS3_AXIS_Y], obj->cali_sw[LSM6DS3_AXIS_Z]);
		}
#endif
#if 1
	#if 0
		obj->data[LSM6DS3_AXIS_X] = (long)(obj->data[LSM6DS3_AXIS_X]) * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);
		obj->data[LSM6DS3_AXIS_Y] = (long)(obj->data[LSM6DS3_AXIS_Y]) * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);
		obj->data[LSM6DS3_AXIS_Z] = (long)(obj->data[LSM6DS3_AXIS_Z]) * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);
	#endif
			/*report degree/s */
		obj->data[LSM6DS3_AXIS_X] = (long)(obj->data[LSM6DS3_AXIS_X])*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000/1000;
		obj->data[LSM6DS3_AXIS_Y] = (long)(obj->data[LSM6DS3_AXIS_Y])*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000/1000;
		obj->data[LSM6DS3_AXIS_Z] = (long)(obj->data[LSM6DS3_AXIS_Z])*LSM6DS3_GYRO_SENSITIVITY_2000DPS*131/1000/1000;

		obj->data[LSM6DS3_AXIS_X] += obj->cali_sw[LSM6DS3_AXIS_X];
		obj->data[LSM6DS3_AXIS_Y] += obj->cali_sw[LSM6DS3_AXIS_Y];
		obj->data[LSM6DS3_AXIS_Z] += obj->cali_sw[LSM6DS3_AXIS_Z];

		/*remap coordinate*/
		data[obj->cvt.map[LSM6DS3_AXIS_X]] = obj->cvt.sign[LSM6DS3_AXIS_X] * obj->data[LSM6DS3_AXIS_X];
		data[obj->cvt.map[LSM6DS3_AXIS_Y]] = obj->cvt.sign[LSM6DS3_AXIS_Y] * obj->data[LSM6DS3_AXIS_Y];
		data[obj->cvt.map[LSM6DS3_AXIS_Z]] = obj->cvt.sign[LSM6DS3_AXIS_Z] * obj->data[LSM6DS3_AXIS_Z];
#else
		data[LSM6DS3_AXIS_X] = (s64)(data[LSM6DS3_AXIS_X]) * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);
		data[LSM6DS3_AXIS_Y] = (s64)(data[LSM6DS3_AXIS_Y]) * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);
		data[LSM6DS3_AXIS_Z] = (s64)(data[LSM6DS3_AXIS_Z]) * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);
#endif
	}

	sprintf(buf, "%x %x %x", data[LSM6DS3_AXIS_X], data[LSM6DS3_AXIS_Y], data[LSM6DS3_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA) {

		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
	}
#endif

	return 0;

}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8)*10);

	if ((NULL == buf) || (bufsize <= 30)) {

		return -1;
	}

	if (NULL == client) {

		*buf = 0;
		return -2;
	}

	sprintf(buf, "LSM6DS3 Chip");
	return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
	char strbuf[LSM6DS3_BUFSIZE];
	if (NULL == client) {

		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	LSM6DS3_ReadChipInfo(client, strbuf, LSM6DS3_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm6ds3_i2c_client;
	char strbuf[LSM6DS3_BUFSIZE];

	if (NULL == client) {

		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	LSM6DS3_ReadGyroData(client, strbuf, LSM6DS3_BUFSIZE);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
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
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL) {

		GYRO_ERR("i2c_data obj is null!!\n");
		return count;
	}

	if (1 == sscanf(buf, "0x%x", &trace)) {

		atomic_set(&obj->trace, trace);
	} else {

		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);
	}

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct lsm6ds3_gyro_i2c_data *obj = obj_i2c_data;
	if (obj == NULL) {

		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw) {

		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
			obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	} else {

		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	return len;
}
/*----------------------------------------------------------------------------*/

static DRIVER_ATTR(chipinfo,             S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *LSM6DS3_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
};
/*----------------------------------------------------------------------------*/
static int lsm6ds3_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(LSM6DS3_attr_list)/sizeof(LSM6DS3_attr_list[0]));
	if (driver == NULL) {

		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, LSM6DS3_attr_list[idx]);
		if (0 != err) {

			GYRO_ERR("driver_create_file (%s) = %d\n", LSM6DS3_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int lsm6ds3_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(LSM6DS3_attr_list)/sizeof(LSM6DS3_attr_list[0]));

	if (driver == NULL) {

		return -EINVAL;
	}

	for (idx = 0; idx < num; idx++) {

		driver_remove_file(driver, LSM6DS3_attr_list[idx]);
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int LSM6DS3_gyro_init_client(struct i2c_client *client, bool enable)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	GYRO_LOG("%s lsm6ds3 addr %x!\n", __FUNCTION__, client->addr);

	res = LSM6DS3_CheckDeviceID(client);
	if (res != LSM6DS3_SUCCESS) {

		return res;
	}

	res = LSM6DS3_Set_RegInc(client, true);
	if (res != LSM6DS3_SUCCESS) {

		return res;
	}

	res = LSM6DS3_gyro_SetFullScale(client, LSM6DS3_GYRO_RANGE_2000DPS);/*we have only this choice*/
	if (res != LSM6DS3_SUCCESS) {

		return res;
	}


	res = LSM6DS3_gyro_SetSampleRate(client, LSM6DS3_GYRO_ODR_104HZ);
	if (res != LSM6DS3_SUCCESS) {

		return res;
	}
	res = LSM6DS3_gyro_SetPowerMode(client, enable);
	if (res != LSM6DS3_SUCCESS) {

		return res;
	}

	GYRO_LOG("LSM6DS3_gyro_init_client OK!\n");
	/*acc setting*/


#ifdef CONFIG_LSM6DS3_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return LSM6DS3_SUCCESS;
}
/*----------------------------------------------------------------------------*/
#ifdef LSM6DS3_GYRO_NEW_ARCH
static int lsm6ds3_gyro_open_report_data(int open)
{
		/*should queuq work to report event if  is_report_input_direct=true*/
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL*/

static int lsm6ds3_gyro_enable_nodata(int en)
{
	int value = en;
	int err = 0;
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;

	if (priv == NULL) {

		GYRO_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}

	if (value == 1) {

		enable_status = true;
	} else {

		enable_status = false;
	}
	GYRO_LOG("enable value=%d, sensor_power =%d\n", value, sensor_power);
	if (((value == 0) && (sensor_power == false)) || ((value == 1) && (sensor_power == true))) {

		GYRO_LOG("Gsensor device have updated!\n");
	} else {

		err = LSM6DS3_gyro_SetPowerMode(priv->client, enable_status);
	}

		GYRO_LOG("mc3xxx_enable_nodata OK!\n");
		return err;
}

static int lsm6ds3_gyro_set_delay(u64 ns)
{
		int value = 0;
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;

		value = (int)ns/1000/1000;


	if (priv == NULL) {

		GYRO_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}


		GYRO_LOG("mc3xxx_set_delay (%d), chip only use 1024HZ\n", value);
		return 0;
}

static int lsm6ds3_gyro_get_data(int *x, int *y, int *z, int *status)
{
		char buff[LSM6DS3_BUFSIZE];
	struct lsm6ds3_gyro_i2c_data *priv = obj_i2c_data;

	if (priv == NULL) {

		GYRO_ERR("obj_i2c_data is NULL!\n");
		return -1;
	}
	if (atomic_read(&priv->trace) & GYRO_TRC_DATA) {

		GYRO_LOG("%s (%d),\n", __FUNCTION__, __LINE__);
	}
	memset(buff, 0, sizeof(buff));
	LSM6DS3_ReadGyroData(priv->client, buff, LSM6DS3_BUFSIZE);

	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

		return 0;
}
#endif
#ifndef LSM6DS3_GYRO_NEW_ARCH
/*----------------------------------------------------------------------------*/
int LSM6DS3_gyro_operate(void *self, uint32_t command, void *buff_in, int size_in,
		void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct lsm6ds3_gyro_i2c_data *priv = (struct lsm6ds3_gyro_i2c_data *)self;
	hwm_sensor_data *gyro_data;
	char buff[LSM6DS3_BUFSIZE];

	switch (command) {

	case SENSOR_DELAY:
			if ((buff_in == NULL) || (size_in < sizeof(int))) {

				GYRO_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			} else {


			}
			break;

	case SENSOR_ENABLE:
			if ((buff_in == NULL) || (size_in < sizeof(int))) {

				GYRO_ERR("Enable gyroscope parameter error!\n");
				err = -EINVAL;
			} else {

				value = *(int *)buff_in;
				if (value == 1) {

					enable_status = true;
				} else {

					enable_status = false;
				}
				GYRO_LOG("enable value=%d, sensor_power =%d\n", value, sensor_power);
				if (((value == 0) && (sensor_power == false)) || ((value == 1) && (sensor_power == true))) {

					GYRO_LOG("Gsensor device have updated!\n");
				} else {

					err = LSM6DS3_gyro_SetPowerMode(priv->client, enable_status);
				}

			}
			break;

	case SENSOR_GET_DATA:
			if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {

				GYRO_ERR("get gyroscope data parameter error!\n");
				err = -EINVAL;
			} else {

				gyro_data = (hwm_sensor_data *)buff_out;
				LSM6DS3_ReadGyroData(priv->client, buff, LSM6DS3_BUFSIZE);
				sscanf(buff, "%x %x %x", &gyro_data->values[0],
									&gyro_data->values[1], &gyro_data->values[2]);
				gyro_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				gyro_data->value_divide = 1000;
				if (atomic_read(&priv->trace) & GYRO_TRC_DATA) {

					GYRO_LOG("===>LSM6DS3_gyro_operate x=%d,y=%d,z=%d\n",
						gyro_data->values[0], gyro_data->values[1], gyro_data->values[2]);
				}
			}
			break;
	default:
			GYRO_ERR("gyroscope operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}
#endif
/******************************************************************************
 * Function Configuration
******************************************************************************/
static int lsm6ds3_open(struct inode *inode, struct file *file)
{
	file->private_data = lsm6ds3_i2c_client;

	if (file->private_data == NULL) {

		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int lsm6ds3_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_COMPAT
static long lsm6ds3_gyro_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT,
							(unsigned long)arg32);
			 if (ret) {
				GYRO_ERR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.");
				return ret;
			 }

			 break;

	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
			 if (arg32 == NULL) {

				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI,
							(unsigned long)arg32);
			 if (ret) {
				GYRO_ERR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.");
				return ret;
			 }

			 break;

	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
			 if (arg32 == NULL) {

				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI,
							(unsigned long)arg32);
			 if (ret) {
				GYRO_ERR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.");
				return ret;
			 }

			 break;

	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
			 if (arg32 == NULL) {

				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI,
							(unsigned long)arg32);
			 if (ret) {
				GYRO_ERR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.");
				return ret;
			 }

			 break;

	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
			 if (arg32 == NULL) {

				 GYRO_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA,
							(unsigned long)arg32);
			 if (ret) {
				GYRO_ERR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
				return ret;
			 }

			break;

	default:
			 printk(KERN_ERR "%s not supported = 0x%04x", __FUNCTION__, cmd);
			 return -ENOIOCTLCMD;
			 break;
	}
	return ret;
}
#endif

static long lsm6ds3_gyro_unlocked_ioctl(struct file *file, unsigned int cmd,
			 unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;

	char strbuf[LSM6DS3_BUFSIZE] = {0};
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	struct SENSOR_DATA sensor_data;
	int cali[3] = {0};
	int smtRes = 0;
	/*GYRO_FUN();*/

	if (_IOC_DIR(cmd) & _IOC_READ) {

		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {

		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if (err) {

		GYRO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {

	case GYROSCOPE_IOCTL_INIT:
			LSM6DS3_gyro_init_client(client, false);
			break;

	case GYROSCOPE_IOCTL_SMT_DATA:
			data = (void __user *) arg;
			if (data == NULL) {

				err = -EINVAL;
				break;
			}

			GYRO_LOG("IOCTL smtRes: %d!\n", smtRes);
			copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));

			if (copy_cnt) {

				err = -EFAULT;
				GYRO_ERR("copy gyro data to user failed!\n");
			}
			GYRO_LOG("copy gyro data to user OK: %d!\n", copy_cnt);
			break;


	case GYROSCOPE_IOCTL_READ_SENSORDATA:
			data = (void __user *)arg;
			if (data == NULL) {

				err = -EINVAL;
				break;
			}

			LSM6DS3_ReadGyroData(client, strbuf, LSM6DS3_BUFSIZE);
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
			} else {

				cali[LSM6DS3_AXIS_X] = (s64)(sensor_data.x);/* * 180*1000*1000/(LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142);*/
				cali[LSM6DS3_AXIS_Y] = (s64)(sensor_data.y);/* * 180*1000*1000/(LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142);*/
				cali[LSM6DS3_AXIS_Z] = (s64)(sensor_data.z);/* * 180*1000*1000/(LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142);			*/
				err = LSM6DS3_gyro_WriteCalibration(client, cali);
			}
			break;

	case GYROSCOPE_IOCTL_CLR_CALI:
			err = LSM6DS3_gyro_ResetCalibration(client);
			break;

	case GYROSCOPE_IOCTL_GET_CALI:
			data = (void __user *)arg;
			if (data == NULL) {

				err = -EINVAL;
				break;
			}
			err = LSM6DS3_gyro_ReadCalibration(client, cali);
			if (err) {

				break;
			}

			sensor_data.x = (s64)(cali[LSM6DS3_AXIS_X]);/* * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);*/
			sensor_data.y = (s64)(cali[LSM6DS3_AXIS_Y]);/* * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000);*/
			sensor_data.z = (s64)(cali[LSM6DS3_AXIS_Z]);/* * LSM6DS3_GYRO_SENSITIVITY_2000DPS*3142/(180*1000*1000); */

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

#if 1
/*----------------------------------------------------------------------------*/
static struct file_operations lsm6ds3_gyro_fops = {
	.owner = THIS_MODULE,
	.open = lsm6ds3_open,
	.release = lsm6ds3_release,
	.unlocked_ioctl = lsm6ds3_gyro_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lsm6ds3_gyro_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice lsm6ds3_gyro_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &lsm6ds3_gyro_fops,
};
#endif

/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	GYRO_FUN();

	if (msg.event == PM_EVENT_SUSPEND) {

		if (obj == NULL) {

			GYRO_ERR("null pointer!!\n");
			return -1;
		}
		atomic_set(&obj->suspend, 1);
		err = LSM6DS3_gyro_SetPowerMode(obj->client, false);
		if (err) {

			GYRO_ERR("write power control fail!!\n");
			return err;
		}

		sensor_power = false;

		LSM6DS3_power(obj->hw, 0);

	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_resume(struct i2c_client *client)
{
	struct lsm6ds3_gyro_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	GYRO_FUN();

	if (obj == NULL) {

		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	LSM6DS3_power(obj->hw, 1);

	err = LSM6DS3_gyro_SetPowerMode(obj->client, enable_status);
	if (err) {

		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void lsm6ds3_gyro_early_suspend(struct early_suspend *h)
{
	struct lsm6ds3_gyro_i2c_data *obj = container_of(h, struct lsm6ds3_gyro_i2c_data, early_drv);
	int err;
	GYRO_FUN();

	if (obj == NULL) {

		GYRO_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = LSM6DS3_gyro_SetPowerMode(obj->client, false);
	if (err) {

		GYRO_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;

	LSM6DS3_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void lsm6ds3_gyro_late_resume(struct early_suspend *h)
{
	struct lsm6ds3_gyro_i2c_data *obj = container_of(h, struct lsm6ds3_gyro_i2c_data, early_drv);
	int err;
	GYRO_FUN();

	if (obj == NULL) {

		GYRO_ERR("null pointer!!\n");
		return;
	}

	LSM6DS3_power(obj->hw, 1);
	err = LSM6DS3_gyro_SetPowerMode(obj->client, enable_status);
	if (err) {

		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return;
	}
	atomic_set(&obj->suspend, 0);
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct lsm6ds3_gyro_i2c_data *obj;

#ifdef LSM6DS3_GYRO_NEW_ARCH
	struct gyro_control_path ctl = {0};
		struct gyro_data_path data = {0};
#else
	struct hwmsen_object gyro_sobj;
#endif
	int err = 0;
	GYRO_FUN();
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	if (!obj) {

		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct lsm6ds3_gyro_i2c_data));

	obj->hw = get_cust_gyro();

	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {

		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	client->addr = 0xD4 >> 1;
	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	lsm6ds3_i2c_client = new_client;
	err = LSM6DS3_gyro_init_client(new_client, false);
	if (err) {

		goto exit_init_failed;
	}

#if 1
	err = misc_register(&lsm6ds3_gyro_device);
	if (err) {

		GYRO_ERR("lsm6ds3_gyro_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
#endif

#ifdef LSM6DS3_GYRO_NEW_ARCH
	err = lsm6ds3_create_attr(&(lsm6ds3_gyro_init_info.platform_diver_addr->driver));
#else
	err = lsm6ds3_create_attr(&lsm6ds3_driver.driver);
#endif
	if (err) {

		GYRO_ERR("lsm6ds3 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
#ifndef LSM6DS3_GYRO_NEW_ARCH

	gyro_sobj.self = obj;
		gyro_sobj.polling = 1;
		gyro_sobj.sensor_operate = LSM6DS3_gyro_operate;
	err = hwmsen_attach(ID_GYROSCOPE, &gyro_sobj);
	if (err) {

		GYRO_ERR("hwmsen_attach Gyroscope fail = %d\n", err);
		goto exit_kfree;
	}
#else
		ctl.open_report_data = lsm6ds3_gyro_open_report_data;
		ctl.enable_nodata = lsm6ds3_gyro_enable_nodata;
		ctl.set_delay  = lsm6ds3_gyro_set_delay;
		ctl.is_report_input_direct = false;
		ctl.is_support_batch = obj->hw->is_batch_supported;

		err = gyro_register_control_path(&ctl);
		if (err) {

		GYRO_ERR("register acc control path err\n");
		goto exit_kfree;
	}

		data.get_data = lsm6ds3_gyro_get_data;
		data.vender_div = DEGREE_TO_RAD;
		err = gyro_register_data_path(&data);
		if (err) {

		GYRO_ERR("register acc data path err= %d\n", err);
		goto exit_kfree;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = lsm6ds3_gyro_early_suspend,
	obj->early_drv.resume   = lsm6ds3_gyro_late_resume,
	register_early_suspend(&obj->early_drv);
#endif
#ifdef LSM6DS3_GYRO_NEW_ARCH
	lsm6ds3_gyro_init_flag = 0;
#endif
	GYRO_LOG("%s: OK\n", __func__);
	return 0;

	exit_create_attr_failed:
	misc_deregister(&lsm6ds3_gyro_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	/*i2c_detach_client(new_client);*/
	exit_kfree:
	kfree(obj);
	exit:
#ifdef LSM6DS3_GYRO_NEW_ARCH
	lsm6ds3_gyro_init_flag = -1;
#endif
	GYRO_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_i2c_remove(struct i2c_client *client)
{
	int err = 0;
#ifndef LSM6DS3_GYRO_NEW_ARCH

	err = lsm6ds3_delete_attr(&lsm6ds3_driver.driver);
#else
	err = lsm6ds3_delete_attr(&(lsm6ds3_gyro_init_info.platform_diver_addr->driver));
#endif
	if (err) {

		GYRO_ERR("lsm6ds3_gyro_i2c_remove fail: %d\n", err);
	}

	#if 1
	err = misc_deregister(&lsm6ds3_gyro_device);
	if (err) {

		GYRO_ERR("misc_deregister lsm6ds3_gyro_device fail: %d\n", err);
	}
	#endif

	lsm6ds3_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifndef LSM6DS3_GYRO_NEW_ARCH
static int lsm6ds3_gyro_probe(struct platform_device *pdev)
{
	struct gyro_hw *gy_hw = get_cust_gyro();
	GYRO_FUN();

	LSM6DS3_power(gy_hw, 1);

	if (i2c_add_driver(&lsm6ds3_gyro_i2c_driver)) {

		GYRO_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int lsm6ds3_gyro_remove(struct platform_device *pdev)
{
		struct gyro_hw *gy_hw = get_cust_gyro();

		/*GYRO_FUN();    */
		LSM6DS3_power(gy_hw, 0);
		i2c_del_driver(&lsm6ds3_gyro_i2c_driver);
		return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyroscope_of_match[] = {
	{ .compatible = "mediatek,gyroscope", },
	{},
};
#endif

static struct platform_driver lsm6ds3_driver = {
	.probe		= lsm6ds3_gyro_probe,
	.remove		= lsm6ds3_gyro_remove,
	.driver		= {
			.name	= "gyroscope",
		/*	.owner	= THIS_MODULE,*/
	#ifdef CONFIG_OF
			.of_match_table = gyroscope_of_match,
	#endif

	}
};
#else
static int lsm6ds3_gyro_local_init(struct platform_device *pdev)
{
	struct gyro_hw *gy_hw = get_cust_gyro();
	GYRO_FUN();

	gyroPltFmDev = pdev;

	LSM6DS3_power(gy_hw, 1);

	if (i2c_add_driver(&lsm6ds3_gyro_i2c_driver)) {

		GYRO_ERR("add driver error\n");
		return -1;
	}
	if (lsm6ds3_gyro_init_flag == -1) {

		GYRO_ERR("%s init failed!\n", __FUNCTION__);
		return -1;
	}
	return 0;
}
static int lsm6ds3_gyro_local_uninit(void)
{
		struct gyro_hw *gy_hw = get_cust_gyro();

		GYRO_FUN();
		LSM6DS3_power(gy_hw, 0);
		i2c_del_driver(&lsm6ds3_gyro_i2c_driver);
		return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int __init lsm6ds3_gyro_init(void)
{
	/*GYRO_FUN();*/
	struct gyro_hw *hw = get_cust_gyro();
		GYRO_LOG("%s: i2c_number=%d\n", __func__, hw->i2c_num);
		i2c_register_board_info(hw->i2c_num, &i2c_lsm6ds3_gyro, 1);

	lsm6ds3_i2c_client = NULL;	/*initial in module_init*/

	obj_i2c_data = NULL;	/*initial in module_init*/
	sensor_power = false;	/*initial in module_init*/
	enable_status = false;	/*initial in module_init*/

#ifndef LSM6DS3_GYRO_NEW_ARCH
	if (platform_driver_register(&lsm6ds3_driver)) {

		GYRO_ERR("failed to register driver");
		return -ENODEV;
	}
#else
		gyro_driver_add(&lsm6ds3_gyro_init_info);
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit lsm6ds3_gyro_exit(void)
{
	GYRO_FUN();
#ifndef LSM6DS3_GYRO_NEW_ARCH
	platform_driver_unregister(&lsm6ds3_driver);
#endif
}
/*----------------------------------------------------------------------------*/
module_init(lsm6ds3_gyro_init);
module_exit(lsm6ds3_gyro_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LSM6DS3 Accelerometer and gyroscope driver");
MODULE_AUTHOR("xj.wang@mediatek.com, darren.han@st.com");






/*----------------------------------------------------------------- LSM6DS3 ------------------------------------------------------------------*/

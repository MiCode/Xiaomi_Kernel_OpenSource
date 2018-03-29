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

#include <cust_gyro.h>
#include "mpu6515.h"
#include "mpu6515g.h"
#include "mpu6515a.h"
#include <linux/kernel.h>

/* #include <mach/mt_boot.h> */
#include <mt-plat/mt_boot_common.h>
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#include <gyroscope.h>

#define INV_GYRO_AUTO_CALI  1

/* #define POWER_NONE_MACRO MT65XX_POWER_NONE */
/*----------------------------------------------------------------------------*/
#define MPU6515_DEFAULT_FS		MPU6515_FS_1000
#define MPU6515_DEFAULT_LSB		MPU6515_FS_1000_LSB
/* #define USE_EARLY_SUSPEND */
/*---------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU6515_LOWPASS	/*apply low pass filter on output */
/*----------------------------------------------------------------------------*/
#define MPU6515_AXIS_X          0
#define MPU6515_AXIS_Y          1
#define MPU6515_AXIS_Z          2
#define MPU6515_AXES_NUM        3
#define MPU6515_DATA_LEN        6
#define MPU6515_DEV_NAME        "MPU6515GY"	/* name must different with gsensor mpu6515 */
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu6515_i2c_id[] = { {MPU6515_DEV_NAME, 0}, {} };

#ifdef CONFIG_MTK_LEGACY
static struct i2c_board_info i2c_mpu6515 __initdata = {
I2C_BOARD_INFO(MPU6515_DEV_NAME, (MPU6515_I2C_SLAVE_ADDR >> 1)) };
#endif
int packet_thresh = 75;		/* 600 ms / 8ms/sample */

/* Maintain  cust info here */
struct gyro_hw gyro_cust;
static struct gyro_hw *hw = &gyro_cust;
/* For  driver get cust info */
struct gyro_hw *get_cust_gyro(void)
{
	return &gyro_cust;
}

struct platform_device *gyroPltFmDev;
/*----------------------------------------------------------------------------*/
static int mpu6515_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mpu6515_i2c_remove(struct i2c_client *client);
static int mpu6515_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int mpu6515_suspend(struct i2c_client *client, pm_message_t msg);
static int mpu6515_resume(struct i2c_client *client);
#endif

static int gyroscope_local_init(struct platform_device *pdev);
static int gyroscope_remove(void);
/*----------------------------------------------------------------------------*/
typedef enum {
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
	s16 raw[C_MAX_FIR_LENGTH][MPU6515_AXES_NUM];
	int sum[MPU6515_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct mpu6515_i2c_data {
	struct i2c_client *client;
	struct gyro_hw *hw;
	struct hwmsen_convert cvt;

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

#if defined(CONFIG_MPU6515_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
	/*early suspend */
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(USE_EARLY_SUSPEND)
	struct early_suspend early_drv;
#endif
#if INV_GYRO_AUTO_CALI == 1
	s16 inv_cali_raw[MPU6515_AXES_NUM + 1];
	s16 temperature;
	struct mutex temperature_mutex;	/* for temperature protection */
	struct mutex raw_data_mutex;	/* for inv_cali_raw[] protection */
#endif
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,GYRO"},
	{},
};
#endif

static struct i2c_driver mpu6515_i2c_driver = {
	.driver = {
		   .name = MPU6515_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = gyro_of_match,
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
static DEFINE_MUTEX(gyroscope_mutex);
static bool enable_status;

static int gyroscope_init_flag = -1;	/* 0<==>OK -1 <==> fail */
#ifdef MPU6515_ACCESS_BY_GSE_I2C
extern int gsensor_init_flag;	/* 0<==>OK -1 <==> fail */
#endif

static struct gyro_init_info gyroscope_init_info = {
	.name = "mpu6515",
	.init = gyroscope_local_init,
	.uninit = gyroscope_remove,
};

/*----------------------------------------------------------------------------*/

/* extern int MPU6515_gse_power(void); */
/* extern int MPU6515_gse_mode(void); */
/* extern int MPU6515_SCP_SetPowerMode(bool enable, int sensorType); */

#ifdef MPU6515_ACCESS_BY_GSE_I2C
/* extern int MPU6515_i2c_master_send(u8 *buf, u8 len); */
/* extern int MPU6515_i2c_master_recv(u8 *buf, u8 len); */
/* extern int MPU6515_hwmsen_read_block(u8 addr, u8 *buf, u8 len); */
/* extern int MPU6515_hwmsen_read_byte(u8 addr, u8 *buf); */
#endif

static unsigned int power_on;

int MPU6515_gyro_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(MPU6515_gyro_power);

int MPU6515_gyro_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(MPU6515_gyro_mode);

/*--------------------gyroscopy power control function----------------------------------*/
static void MPU6515_power(struct gyro_hw *hw, unsigned int on)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
#if 0
	if (hw->power_id != POWER_NONE_MACRO) {	/* have externel LDO */
		GYRO_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on) {	/* power status not change */
			GYRO_LOG("ignore power control: %d\n", on);
		} else if (on) {	/* power on */
			if (!hwPowerOn(hw->power_id, hw->power_vol, "MPU6515GY"))
				GYRO_ERR("power on fails!!\n");
		} else {		/* power off */
			if (MPU6515_gse_power() == false) {
				if (!hwPowerDown(hw->power_id, "MPU6515GY"))
					GYRO_ERR("power off fail!!\n");
			}
		}
	}
#endif
	power_on = on;
#endif				/* #ifndef FPGA_EARLY_PORTING */
}

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int MPU6515_write_rel_calibration(struct mpu6515_i2c_data *obj, int dat[MPU6515_AXES_NUM])
{
	obj->cali_sw[MPU6515_AXIS_X] =
	    obj->cvt.sign[MPU6515_AXIS_X] * dat[obj->cvt.map[MPU6515_AXIS_X]];
	obj->cali_sw[MPU6515_AXIS_Y] =
	    obj->cvt.sign[MPU6515_AXIS_Y] * dat[obj->cvt.map[MPU6515_AXIS_Y]];
	obj->cali_sw[MPU6515_AXIS_Z] =
	    obj->cvt.sign[MPU6515_AXIS_Z] * dat[obj->cvt.map[MPU6515_AXIS_Z]];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
			 obj->cvt.sign[MPU6515_AXIS_X], obj->cvt.sign[MPU6515_AXIS_Y],
			 obj->cvt.sign[MPU6515_AXIS_Z], dat[MPU6515_AXIS_X], dat[MPU6515_AXIS_Y],
			 dat[MPU6515_AXIS_Z], obj->cvt.map[MPU6515_AXIS_X],
			 obj->cvt.map[MPU6515_AXIS_Y], obj->cvt.map[MPU6515_AXIS_Z]);
		GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n",
			 obj->cali_sw[MPU6515_AXIS_X], obj->cali_sw[MPU6515_AXIS_Y],
			 obj->cali_sw[MPU6515_AXIS_Z]);
	}
#endif
	return 0;
}


/*----------------------------------------------------------------------------*/
static int MPU6515_ResetCalibration(struct i2c_client *client)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadCalibration(struct i2c_client *client, int dat[MPU6515_AXES_NUM])
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[MPU6515_AXIS_X]] =
	    obj->cvt.sign[MPU6515_AXIS_X] * obj->cali_sw[MPU6515_AXIS_X];
	dat[obj->cvt.map[MPU6515_AXIS_Y]] =
	    obj->cvt.sign[MPU6515_AXIS_Y] * obj->cali_sw[MPU6515_AXIS_Y];
	dat[obj->cvt.map[MPU6515_AXIS_Z]] =
	    obj->cvt.sign[MPU6515_AXIS_Z] * obj->cali_sw[MPU6515_AXIS_Z];

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n",
			 dat[MPU6515_AXIS_X], dat[MPU6515_AXIS_Y], dat[MPU6515_AXIS_Z]);
	}
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int MPU6515_WriteCalibration(struct i2c_client *client, int dat[MPU6515_AXES_NUM])
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[MPU6515_AXES_NUM];


	GYRO_FUN();
	if (!obj || !dat) {
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	} else {
		cali[obj->cvt.map[MPU6515_AXIS_X]] =
		    obj->cvt.sign[MPU6515_AXIS_X] * obj->cali_sw[MPU6515_AXIS_X];
		cali[obj->cvt.map[MPU6515_AXIS_Y]] =
		    obj->cvt.sign[MPU6515_AXIS_Y] * obj->cali_sw[MPU6515_AXIS_Y];
		cali[obj->cvt.map[MPU6515_AXIS_Z]] =
		    obj->cvt.sign[MPU6515_AXIS_Z] * obj->cali_sw[MPU6515_AXIS_Z];
		cali[MPU6515_AXIS_X] += dat[MPU6515_AXIS_X];
		cali[MPU6515_AXIS_Y] += dat[MPU6515_AXIS_Y];
		cali[MPU6515_AXIS_Z] += dat[MPU6515_AXIS_Z];
#if DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
				 dat[MPU6515_AXIS_X], dat[MPU6515_AXIS_Y], dat[MPU6515_AXIS_Z],
				 cali[MPU6515_AXIS_X], cali[MPU6515_AXIS_Y], cali[MPU6515_AXIS_Z]);
		}
#endif
		return MPU6515_write_rel_calibration(obj, cali);
	}

	return err;
}

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int MPU6515_ReadStart(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GYRO_FUN();

	databuf[0] = MPU6515_REG_FIFO_EN;

	if (enable) {
		/* enable xyz gyro in FIFO */
		databuf[1] =
		    (MPU6515_FIFO_GYROX_EN | MPU6515_FIFO_GYROY_EN | MPU6515_FIFO_GYROZ_EN);
	} else {
		/* disable xyz gyro in FIFO */
		databuf[1] = 0;
	}

#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR(" enable xyz gyro in FIFO error,enable: 0x%x!\n", databuf[1]);
		return MPU6515_ERR_I2C;
	}
	GYRO_LOG("MPU6515_ReadStart: enable xyz gyro in FIFO: 0x%x\n", databuf[1]);
	return MPU6515_SUCCESS;
}

/* ----------------------------------------------------------------------------// */
static int MPU6515_SetPWR_MGMT_2(struct i2c_client *client, bool enable)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = { 0 };
	int res = 0;

	if (enable)
		databuf[1] = 0xC0;
	else
		databuf[1] = 0xC7;

	if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
		GYRO_LOG("MPU6515_SetPWR_MGMT_2 : en = %d, reg = %x\n", enable, databuf[1]);

	databuf[0] = MPU6515_REG_PWR_CTL2;
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_LOG("set power mode failed : %d!\n", res);
		return MPU6515_ERR_I2C;
	}
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_byte(MPU6515_REG_PWR_CTL2, databuf))
#else
	if (hwmsen_read_byte(client, MPU6515_REG_PWR_CTL2, databuf))
#endif
		{
			GYRO_ERR("read power ctl 2 register err!\n");
	} else {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("MPU6515_REG_PWR_CTL = %x\n", databuf[0]);
	}

	return MPU6515_SUCCESS;
}

/* ----------------------------------------------------------------------------// */
static int MPU6515_SetPowerMode(struct i2c_client *client, bool enable)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = { 0 };
	int res = 0;

	if (enable == sensor_power) {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("Sensor power status is newest!\n");
		return MPU6515_SUCCESS;
	}
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_byte(MPU6515_REG_PWR_CTL, databuf))
#else
	if (hwmsen_read_byte(client, MPU6515_REG_PWR_CTL, databuf))
#endif
		{
			GYRO_ERR("read power ctl register err!\n");
			return MPU6515_ERR_I2C;
		}

	if ((databuf[0] & 0x1f) != 0x1)
		GYRO_ERR("MPU6515 PWR_MGMT_1 = %x\n", databuf[0]);

	databuf[0] &= ~MPU6515_SLEEP;
	if (enable == FALSE) {
		if (MPU6515_gse_mode() == false)
			databuf[0] |= MPU6515_SLEEP;

	} else {
		/* do nothing */
	}


	databuf[1] = databuf[0];
	databuf[0] = MPU6515_REG_PWR_CTL;

#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif

	if (res <= 0) {
		GYRO_LOG("set power mode failed!\n");
		return MPU6515_ERR_I2C;
	} else {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("set power mode ok %d!\n", enable);
	}

	sensor_power = enable;

	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = { 0 };
	int res = 0;

	if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
		GYRO_FUN();

	databuf[0] = MPU6515_REG_CFG;
	databuf[1] = dataformat;
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0)
		return MPU6515_ERR_I2C;

	/* read sample rate after written for test */
	udelay(500);
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_byte(MPU6515_REG_CFG, databuf))
#else
	if (hwmsen_read_byte(client, MPU6515_REG_CFG, databuf))
#endif
		{
			GYRO_ERR("read data format register err!\n");
			return MPU6515_ERR_I2C;
	} else {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("read  data format: 0x%x\n", databuf[0]);
	}

	return MPU6515_SUCCESS;
}

static int MPU6515_SetFullScale(struct i2c_client *client, u8 dataformat)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = { 0 };
	int res = 0;

	if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
		GYRO_FUN();

	databuf[0] = MPU6515_REG_GYRO_CFG;
	databuf[1] = dataformat;
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0)
		return MPU6515_ERR_I2C;

	/* read sample rate after written for test */
	udelay(500);
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_byte(MPU6515_REG_GYRO_CFG, databuf))
#else
	if (hwmsen_read_byte(client, MPU6515_REG_GYRO_CFG, databuf))
#endif
		{
			GYRO_ERR("read data format register err!\n");
			return MPU6515_ERR_I2C;
	} else {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("read  data format: 0x%x\n", databuf[0]);
	}

	return MPU6515_SUCCESS;
}


/* set the sample rate */
static int MPU6515_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[2] = { 0 };
	int rate_div = 0;
	int res = 0;

	/* GYRO_FUN(); */
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_byte(MPU6515_REG_CFG, databuf))
#else
	if (hwmsen_read_byte(client, MPU6515_REG_CFG, databuf))
#endif
		{
			GYRO_ERR("read gyro data format register err!\n");
			return MPU6515_ERR_I2C;
	} else {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("read  gyro data format register: 0x%x\n", databuf[0]);
	}

	if ((databuf[0] & 0x07) == 0)	/* Analog sample rate is 8KHz */
		rate_div = 8 * 1024 / sample_rate - 1;
	else			/* 1kHz */
		rate_div = 1024 / sample_rate - 1;


	if (rate_div > 255) {	/* rate_div: 0 to 255; */
		rate_div = 255;
	} else if (rate_div < 0) {
		rate_div = 0;
	}

	databuf[0] = MPU6515_REG_SAMRT_DIV;
	databuf[1] = rate_div;
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write sample rate register err!\n");
		return MPU6515_ERR_I2C;
	}
	/* read sample div after written for test */
	udelay(500);
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_byte(MPU6515_REG_SAMRT_DIV, databuf))
#else
	if (hwmsen_read_byte(client, MPU6515_REG_SAMRT_DIV, databuf))
#endif
		{
			GYRO_ERR("read gyro sample rate register err!\n");
			return MPU6515_ERR_I2C;
	} else {
		if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
			GYRO_LOG("read  gyro sample rate: 0x%x\n", databuf[0]);
	}

	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int MPU6515_FIFOConfig(struct i2c_client *client, u8 clk)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	GYRO_FUN();

	/* use gyro X, Y or Z for clocking */
	databuf[0] = MPU6515_REG_PWR_CTL;
	databuf[1] = clk;
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write Power CTRL register err!\n");
		return MPU6515_ERR_I2C;
	}
	GYRO_LOG("MPU6515 use gyro X for clocking OK!\n");

	mdelay(50);

	/* enable xyz gyro in FIFO */
	databuf[0] = MPU6515_REG_FIFO_EN;
	databuf[1] = (MPU6515_FIFO_GYROX_EN | MPU6515_FIFO_GYROY_EN | MPU6515_FIFO_GYROZ_EN);
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write Power CTRL register err!\n");
		return MPU6515_ERR_I2C;
	}
	GYRO_LOG("MPU6515 enable xyz gyro in FIFO OK!\n");

	/* disable AUX_VDDIO */
	databuf[0] = MPU6515_REG_AUX_VDD;
	databuf[1] = MPU6515_AUX_VDDIO_DIS;
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write AUX_VDD register err!\n");
		return MPU6515_ERR_I2C;
	}
	GYRO_LOG("MPU6515 disable AUX_VDDIO OK!\n");

	/* enable FIFO and reset FIFO */
	databuf[0] = MPU6515_REG_FIFO_CTL;
	databuf[1] = (MPU6515_FIFO_EN | MPU6515_FIFO_RST);
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	res = MPU6515_i2c_master_send(databuf, 0x2);
#else
	res = i2c_master_send(client, databuf, 0x2);
#endif
	if (res <= 0) {
		GYRO_ERR("write FIFO CTRL register err!\n");
		return MPU6515_ERR_I2C;
	}

	GYRO_LOG("MPU6515_FIFOConfig OK!\n");
	return MPU6515_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadFifoData(struct i2c_client *client, s16 *data, int *datalen)
{
	struct mpu6515_i2c_data *obj;
	u8 buf[MPU6515_DATA_LEN] = { 0 };
	s16 tmp1[MPU6515_AXES_NUM] = { 0 };
	s16 tmp2[MPU6515_AXES_NUM] = { 0 };
	int err = 0;
	u8 tmp = 0;
	int packet_cnt = 0;
	int i;

	GYRO_FUN();

	if (NULL == client)
		return -EINVAL;

	obj = i2c_get_clientdata(client);

	/* stop putting data in FIFO */
	MPU6515_ReadStart(client, FALSE);

	/* read data number of bytes in FIFO */
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	err = MPU6515_hwmsen_read_byte(MPU6515_REG_FIFO_CNTH, &tmp);
#else
	err = hwmsen_read_byte(client, MPU6515_REG_FIFO_CNTH, &tmp);
#endif
	if (err) {
		GYRO_ERR("read data high number of bytes error: %d\n", err);
		return -1;
	}
	packet_cnt = tmp << 8;

#ifdef MPU6515_ACCESS_BY_GSE_I2C
	err = MPU6515_hwmsen_read_byte(MPU6515_REG_FIFO_CNTL, &tmp);
#else
	err = hwmsen_read_byte(client, MPU6515_REG_FIFO_CNTL, &tmp);
#endif
	if (err) {
		GYRO_ERR("read data low number of bytes error: %d\n", err);
		return -1;
	}
	packet_cnt = (packet_cnt + tmp) / MPU6515_DATA_LEN;

	GYRO_LOG("MPU6515 Read Data packet number OK: %d\n", packet_cnt);

	*datalen = packet_cnt;

	/* Within +-5% range: timing_tolerance * packet_thresh=0.05*75 */
	if (packet_cnt && (abs(packet_thresh - packet_cnt) < 4)) {
		/* read data in FIFO */
		for (i = 0; i < packet_cnt; i++) {
#ifdef MPU6515_ACCESS_BY_GSE_I2C
			if (MPU6515_hwmsen_read_block(MPU6515_REG_FIFO_DATA, buf, MPU6515_DATA_LEN))
#else
			if (hwmsen_read_block(client, MPU6515_REG_FIFO_DATA, buf, MPU6515_DATA_LEN))
#endif
				{
					GYRO_ERR("MPU6515 read data from FIFO error: %d\n", err);
					return -2;
			} else {
				GYRO_LOG("MPU6515 read Data of diff address from FIFO OK !\n");
			}

			tmp1[MPU6515_AXIS_X] =
			    (s16) ((buf[MPU6515_AXIS_X * 2 + 1]) | (buf[MPU6515_AXIS_X * 2] << 8));
			tmp1[MPU6515_AXIS_Y] =
			    (s16) ((buf[MPU6515_AXIS_Y * 2 + 1]) | (buf[MPU6515_AXIS_Y * 2] << 8));
			tmp1[MPU6515_AXIS_Z] =
			    (s16) ((buf[MPU6515_AXIS_Z * 2 + 1]) | (buf[MPU6515_AXIS_Z * 2] << 8));

			/* remap coordinate// */
			tmp2[obj->cvt.map[MPU6515_AXIS_X]] =
			    obj->cvt.sign[MPU6515_AXIS_X] * tmp1[MPU6515_AXIS_X];
			tmp2[obj->cvt.map[MPU6515_AXIS_Y]] =
			    obj->cvt.sign[MPU6515_AXIS_Y] * tmp1[MPU6515_AXIS_Y];
			tmp2[obj->cvt.map[MPU6515_AXIS_Z]] =
			    obj->cvt.sign[MPU6515_AXIS_Z] * tmp1[MPU6515_AXIS_Z];

			data[3 * i + MPU6515_AXIS_X] = tmp2[MPU6515_AXIS_X];
			data[3 * i + MPU6515_AXIS_Y] = tmp2[MPU6515_AXIS_Y];
			data[3 * i + MPU6515_AXIS_Z] = tmp2[MPU6515_AXIS_Z];

			GYRO_LOG("gyro FIFO packet[%d]:[%04X %04X %04X] => [%5d %5d %5d]\n", i,
				 data[3 * i + MPU6515_AXIS_X], data[3 * i + MPU6515_AXIS_Y],
				 data[3 * i + MPU6515_AXIS_Z], data[3 * i + MPU6515_AXIS_X],
				 data[3 * i + MPU6515_AXIS_Y], data[3 * i + MPU6515_AXIS_Z]);
		}

	} else {
		GYRO_ERR("MPU6515 Incorrect packet count: %d\n", packet_cnt);
		return -3;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];
	int data[3];
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

#if INV_GYRO_AUTO_CALI == 1
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_block(MPU6515_REG_TEMPH, databuf, 2))
#else
	if (hwmsen_read_block(client, MPU6515_REG_TEMPH, databuf, 2))
#endif
		{
			GYRO_ERR("MPU6515 read temperature data  error\n");
			return -2;
	} else {
		mutex_lock(&obj->temperature_mutex);
		obj->temperature = ((s16) ((databuf[1]) | (databuf[0] << 8)));
		mutex_unlock(&obj->temperature_mutex);
	}
#endif

#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (MPU6515_hwmsen_read_block(MPU6515_REG_GYRO_XH, databuf, 6))
#else
	if (hwmsen_read_block(client, MPU6515_REG_GYRO_XH, databuf, 6))
#endif
		{
			GYRO_ERR("MPU6515 read gyroscope data  error\n");
			return -2;
	} else {
		obj->data[MPU6515_AXIS_X] =
		    ((s16)
		     ((databuf[MPU6515_AXIS_X * 2 + 1]) | (databuf[MPU6515_AXIS_X * 2] << 8)));
		obj->data[MPU6515_AXIS_Y] =
		    ((s16)
		     ((databuf[MPU6515_AXIS_Y * 2 + 1]) | (databuf[MPU6515_AXIS_Y * 2] << 8)));
		obj->data[MPU6515_AXIS_Z] =
		    ((s16)
		     ((databuf[MPU6515_AXIS_Z * 2 + 1]) | (databuf[MPU6515_AXIS_Z * 2] << 8)));
#if DEBUG
		if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {
			GYRO_LOG("read gyro register: %d, %d, %d, %d, %d, %d",
				 databuf[0], databuf[1], databuf[2], databuf[3], databuf[4],
				 databuf[5]);
			GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
				 obj->data[MPU6515_AXIS_X], obj->data[MPU6515_AXIS_Y],
				 obj->data[MPU6515_AXIS_Z], obj->data[MPU6515_AXIS_X],
				 obj->data[MPU6515_AXIS_Y], obj->data[MPU6515_AXIS_Z]);
		}
#endif
#if INV_GYRO_AUTO_CALI == 1
		mutex_lock(&obj->raw_data_mutex);
		/*remap coordinate */
		obj->inv_cali_raw[obj->cvt.map[MPU6515_AXIS_X]] =
		    obj->cvt.sign[MPU6515_AXIS_X] * obj->data[MPU6515_AXIS_X];
		obj->inv_cali_raw[obj->cvt.map[MPU6515_AXIS_Y]] =
		    obj->cvt.sign[MPU6515_AXIS_Y] * obj->data[MPU6515_AXIS_Y];
		obj->inv_cali_raw[obj->cvt.map[MPU6515_AXIS_Z]] =
		    obj->cvt.sign[MPU6515_AXIS_Z] * obj->data[MPU6515_AXIS_Z];
		mutex_unlock(&obj->raw_data_mutex);
#endif
		obj->data[MPU6515_AXIS_X] =
		    obj->data[MPU6515_AXIS_X] + obj->cali_sw[MPU6515_AXIS_X];
		obj->data[MPU6515_AXIS_Y] =
		    obj->data[MPU6515_AXIS_Y] + obj->cali_sw[MPU6515_AXIS_Y];
		obj->data[MPU6515_AXIS_Z] =
		    obj->data[MPU6515_AXIS_Z] + obj->cali_sw[MPU6515_AXIS_Z];

		/*remap coordinate */
		data[obj->cvt.map[MPU6515_AXIS_X]] =
		    obj->cvt.sign[MPU6515_AXIS_X] * obj->data[MPU6515_AXIS_X];
		data[obj->cvt.map[MPU6515_AXIS_Y]] =
		    obj->cvt.sign[MPU6515_AXIS_Y] * obj->data[MPU6515_AXIS_Y];
		data[obj->cvt.map[MPU6515_AXIS_Z]] =
		    obj->cvt.sign[MPU6515_AXIS_Z] * obj->data[MPU6515_AXIS_Z];

		/* Out put the degree/second(o/s) */
		data[MPU6515_AXIS_X] =
		    data[MPU6515_AXIS_X] * MPU6515_FS_MAX_LSB / MPU6515_DEFAULT_LSB;
		data[MPU6515_AXIS_Y] =
		    data[MPU6515_AXIS_Y] * MPU6515_FS_MAX_LSB / MPU6515_DEFAULT_LSB;
		data[MPU6515_AXIS_Z] =
		    data[MPU6515_AXIS_Z] * MPU6515_FS_MAX_LSB / MPU6515_DEFAULT_LSB;


	}

	sprintf(buf, "%04x %04x %04x", data[MPU6515_AXIS_X], data[MPU6515_AXIS_Y],
		data[MPU6515_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
#endif

	return 0;

}

/* for factory mode */
static int MPU6515_PROCESS_SMT_DATA(struct i2c_client *client, short *data)
{
	int total_num = 0;
	int retval = 0;
	long xSum = 0;
	long ySum = 0;
	long zSum = 0;
	long xAvg, yAvg, zAvg;
	long xRMS, yRMS, zRMS;
	int i = 0;

	int bias_thresh = 5242;	/* 40 dps * 131.072 LSB/dps */
	/* float RMS_thresh = 687.19f; // (.2 dps * 131.072) ^ 2 */
	long RMS_thresh = 68719;	/* (.2 dps * 131.072) ^ 2 */

	total_num = data[0];
	retval = data[1];
	GYRO_LOG("MPU6515 read gyro data OK, total number: %d\n", total_num);
	for (i = 0; i < total_num; i++) {
		xSum = xSum + data[MPU6515_AXES_NUM * i + MPU6515_AXIS_X + 2];
		ySum = ySum + data[MPU6515_AXES_NUM * i + MPU6515_AXIS_Y + 2];
		zSum = zSum + data[MPU6515_AXES_NUM * i + MPU6515_AXIS_Z + 2];

		/*
		   FLPLOGD("read gyro data OK: packet_num:%d, [X:%5d, Y:%5d, Z:%5d]\n", i,
	 data[MPU6515_AXES_NUM*i + MPU6515_AXIS_X +2], data[MPU6515_AXES_NUM*i + MPU6515_AXIS_Y +2],
	 data[MPU6515_AXES_NUM*i + MPU6515_AXIS_Z +2]);
		   FLPLOGD("MPU6515 xSum: %5d,  ySum: %5d, zSum: %5d\n", xSum, ySum, zSum);
		 */

	}
	GYRO_LOG("MPU6515 xSum: %5ld,  ySum: %5ld, zSum: %5ld\n", xSum, ySum, zSum);

	if (total_num != 0) {
		xAvg = (xSum / total_num);
		yAvg = (ySum / total_num);
		zAvg = (zSum / total_num);
	} else {
		xAvg = xSum;
		yAvg = ySum;
		zAvg = zSum;
	}

	GYRO_LOG("MPU6515 xAvg: %ld,  yAvg: %ld,  zAvg: %ld\n", xAvg, yAvg, zAvg);

	if (abs(xAvg) > bias_thresh) {
		GYRO_LOG("X-Gyro bias exceeded threshold\n");
		retval |= 1 << 3;
	}
	if (abs(yAvg) > bias_thresh) {
		GYRO_LOG("Y-Gyro bias exceeded threshold\n");
		retval |= 1 << 4;
	}
	if (abs(zAvg) > bias_thresh) {
		GYRO_LOG("Z-Gyro bias exceeded threshold\n");
		retval |= 1 << 5;
	}

	xRMS = 0;
	yRMS = 0;
	zRMS = 0;

	/* Finally, check RMS */
	for (i = 0; i < total_num; i++) {
		xRMS +=
		    (data[MPU6515_AXES_NUM * i + MPU6515_AXIS_X + 2] -
		     xAvg) * (data[MPU6515_AXES_NUM * i + MPU6515_AXIS_X + 2] - xAvg);
		yRMS +=
		    (data[MPU6515_AXES_NUM * i + MPU6515_AXIS_Y + 2] -
		     yAvg) * (data[MPU6515_AXES_NUM * i + MPU6515_AXIS_Y + 2] - yAvg);
		zRMS +=
		    (data[MPU6515_AXES_NUM * i + MPU6515_AXIS_Z + 2] -
		     zAvg) * (data[MPU6515_AXES_NUM * i + MPU6515_AXIS_Z + 2] - zAvg);
	}

	GYRO_LOG("MPU6515 xRMS: %ld,  yRMS: %ld,  zRMS: %ld\n", xRMS, yRMS, zRMS);
	xRMS = 100 * xRMS;
	yRMS = 100 * yRMS;
	zRMS = 100 * zRMS;

	if (FACTORY_BOOT == get_boot_mode())
		return retval;
	if (xRMS > RMS_thresh * total_num) {
		GYRO_LOG("X-Gyro RMS exceeded threshold, RMS_thresh: %ld\n",
			 RMS_thresh * total_num);
		retval |= 1 << 6;
	}
	if (yRMS > RMS_thresh * total_num) {
		GYRO_LOG("Y-Gyro RMS exceeded threshold, RMS_thresh: %ld\n",
			 RMS_thresh * total_num);
		retval |= 1 << 7;
	}
	if (zRMS > RMS_thresh * total_num) {
		GYRO_LOG("Z-Gyro RMS exceeded threshold, RMS_thresh: %ld\n",
			 RMS_thresh * total_num);
		retval |= 1 << 8;
	}
	if (xRMS == 0 || yRMS == 0 || zRMS == 0)
		/* If any of the RMS noise value returns zero, then we might have dead gyro or FIFO/register failure */
		retval |= 1 << 9;
	GYRO_LOG("retval %d\n", retval);
	return retval;

}



/*----------------------------------------------------------------------------*/
static int MPU6515_SMTReadSensorData(struct i2c_client *client, s16 *buf, int bufsize)
{
	/* S16 gyro[MPU6515_AXES_NUM*MPU6515_FIFOSIZE]; */
	int res = 0;
	int i;
	int datalen, total_num = 0;

	GYRO_FUN();

	if (NULL == buf)
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	MPU6515_SetPWR_MGMT_2(client, true);
	if (sensor_power == false)
		MPU6515_SetPowerMode(client, true);


	for (i = 0; i < MPU6515_AXES_NUM; i++) {
		res = MPU6515_FIFOConfig(client, (i + 1));
		if (res) {
			GYRO_ERR("MPU6515_FIFOConfig error:%d!\n", res);
			return -3;
		}
		/* putting data in FIFO during the delayed 600ms */
		mdelay(600);

		res = MPU6515_ReadFifoData(client, &(buf[total_num + 2]), &datalen);
		if (res) {
			if (res == (-3)) {
				buf[1] = (1 << i);
			} else {
				GYRO_ERR("MPU6515_ReadData error:%d!\n", res);
				return -3;
			}
		} else {
			buf[0] = datalen;
			total_num += datalen * MPU6515_AXES_NUM;
		}
	}

	GYRO_LOG("gyroscope read data OK, total packet: %d", buf[0]);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

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

#if INV_GYRO_AUTO_CALI == 1
/*----------------------------------------------------------------------------*/
static int MPU6515_ReadGyroDataRaw(struct i2c_client *client, char *buf, int bufsize)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->raw_data_mutex);
	/* return gyro raw LSB in device orientation */
	sprintf(buf, "%x %x %x", obj->inv_cali_raw[MPU6515_AXIS_X],
		obj->inv_cali_raw[MPU6515_AXIS_Y], obj->inv_cali_raw[MPU6515_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA) {
		GYRO_LOG("get gyro raw data packet:[%d %d %d]\n", obj->inv_cali_raw[0],
			 obj->inv_cali_raw[1], obj->inv_cali_raw[2]);
	}
#endif
	mutex_unlock(&obj->raw_data_mutex);

	return 0;

}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadTemperature(struct i2c_client *client, char *buf, int bufsize)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->temperature_mutex);
	sprintf(buf, "%x", obj->temperature);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro temperature:[%d]\n", obj->temperature);
#endif
	mutex_unlock(&obj->temperature_mutex);

	return 0;

}

/*----------------------------------------------------------------------------*/
static int MPU6515_ReadPowerStatus(struct i2c_client *client, char *buf, int bufsize)
{
#if DEBUG
	GYRO_LOG("get gyro PowerStatus:[%d]\n", sensor_power);
#endif

	sprintf(buf, "%x", sensor_power);

	return 0;

}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6515_i2c_client;
	/* char strbuf[MPU6515_BUFSIZE]; */
	char *strbuf;
	int ret;

	if (NULL == client) {
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	strbuf = kmalloc(MPU6515_BUFSIZE, GFP_KERNEL);
	if (!strbuf) {
		GYRO_ERR("strbuf is null!!\n");
		return 0;
	}

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
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}

	MPU6515_ReadGyroData(client, strbuf, MPU6515_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mpu6515_i2c_data *obj = obj_i2c_data;

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
	struct mpu6515_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GYRO_ERR("invalid content: '%s', length = %d\n", buf, (int)count);


	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct mpu6515_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
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
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *MPU6515_attr_list[] = {
	&driver_attr_chipinfo,	/*chip information */
	&driver_attr_sensordata,	/*dump sensor data */
	&driver_attr_trace,	/*trace log */
	&driver_attr_status,
};

/*----------------------------------------------------------------------------*/
static int mpu6515_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(MPU6515_attr_list) / sizeof(MPU6515_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, MPU6515_attr_list[idx]);
		if (0 != err) {
			GYRO_ERR("driver_create_file (%s) = %d\n",
				 MPU6515_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(MPU6515_attr_list) / sizeof(MPU6515_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;


	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, MPU6515_attr_list[idx]);


	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_gpio_config(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* because we donot use EINT ,to support low power */
	/* config to GPIO input mode + PD */
	/* set   GPIO_MSE_EINT_PIN */
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;

#ifdef CONFIG_MTK_LEGACY
	mt_set_gpio_mode(GPIO_GYRO_EINT_PIN, GPIO_GYRO_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_GYRO_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_GYRO_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_GYRO_EINT_PIN, GPIO_PULL_DOWN);
#else
	pinctrl = devm_pinctrl_get(&gyroPltFmDev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		GYRO_ERR("Cannot find gyro pinctrl!\n");
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		/* not define default in DTS now, TBD */
		/* GYRO_ERR("Cannot find gyro pinctrl default!\n"); */

	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		GYRO_ERR("Cannot find gyro pinctrl pin_cfg!\n");
		return 0;

	}
	pinctrl_select_state(pinctrl, pins_cfg);
#endif
#endif				/* #ifndef FPGA_EARLY_PORTING */
	return 0;
}

static int mpu6515_init_client(struct i2c_client *client, bool enable)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
		GYRO_FUN();

	mpu6515_gpio_config();

	res = MPU6515_SetPowerMode(client, true);
	if (res != MPU6515_SUCCESS)
		return res;


	/* The range should at least be 17.45 rad/s (ie: ~1000 deg/s). */
	res = MPU6515_SetDataFormat(client, (MPU6515_SYNC_GYROX << MPU6515_EXT_SYNC) |
				    MPU6515_RATE_1K_LPFB_188HZ);

	res = MPU6515_SetFullScale(client, (MPU6515_DEFAULT_FS << MPU6515_FS_RANGE));
	if (res != MPU6515_SUCCESS)
		return res;

	/* Set 125HZ sample rate */
	res = MPU6515_SetSampleRate(client, 200);
	if (res != MPU6515_SUCCESS)
		return res;


	res = MPU6515_SetPWR_MGMT_2(client, enable_status);
	if (res != MPU6515_SUCCESS)
		return res;


	res = MPU6515_SetPowerMode(client, enable_status);
	if (res != MPU6515_SUCCESS)
		return res;


	GYRO_LOG("mpu6515_init_client OK!\n");

#ifdef CONFIG_MPU6515_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return MPU6515_SUCCESS;
}

/******************************************************************************
 * Function Configuration
******************************************************************************/
static int mpu6515_open(struct inode *inode, struct file *file)
{
	file->private_data = mpu6515_i2c_client;

	if (file->private_data == NULL) {
		GYRO_ERR("null pointer!!\n");
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
	char strbuf[MPU6515_BUFSIZE] = { 0 };
	s16 *SMTdata;
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	struct SENSOR_DATA sensor_data;
	int cali[3];
	int smtRes = 0;

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
		mpu6515_init_client(client, false);
		break;

	case GYROSCOPE_IOCTL_SMT_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		SMTdata = kzalloc(sizeof(*SMTdata) * 800, GFP_KERNEL);
		if (SMTdata == NULL) {
			err = -ENOMEM;
			break;
		}
		memset(SMTdata, 0, sizeof(*SMTdata) * 800);
		MPU6515_SMTReadSensorData(client, SMTdata, 800);

		GYRO_LOG("gyroscope read data from kernel OK: SMTdata[0]:%d, copied packet:%d!\n",
			 SMTdata[0], (int)((SMTdata[0] * MPU6515_AXES_NUM + 2) * sizeof(s16) + 1));

		smtRes = MPU6515_PROCESS_SMT_DATA(client, SMTdata);
		GYRO_LOG("ioctl smtRes: %d!\n", smtRes);
		copy_cnt = copy_to_user(data, &smtRes, sizeof(smtRes));
		kfree(SMTdata);
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
		/* prevent meta calibration timeout */
		MPU6515_SetPWR_MGMT_2(client, true);
		if (false == sensor_power) {
			MPU6515_SetPowerMode(client, true);
			msleep(50);
		}
		MPU6515_ReadGyroData(client, strbuf, MPU6515_BUFSIZE);
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
			cali[MPU6515_AXIS_X] =
			    sensor_data.x * MPU6515_DEFAULT_LSB / MPU6515_FS_MAX_LSB;
			cali[MPU6515_AXIS_Y] =
			    sensor_data.y * MPU6515_DEFAULT_LSB / MPU6515_FS_MAX_LSB;
			cali[MPU6515_AXIS_Z] =
			    sensor_data.z * MPU6515_DEFAULT_LSB / MPU6515_FS_MAX_LSB;
			GYRO_LOG("gyro set cali:[%5d %5d %5d]\n", cali[MPU6515_AXIS_X],
				 cali[MPU6515_AXIS_Y], cali[MPU6515_AXIS_Z]);
			err = MPU6515_WriteCalibration(client, cali);
		}
		break;

	case GYROSCOPE_IOCTL_CLR_CALI:
		err = MPU6515_ResetCalibration(client);
		break;

	case GYROSCOPE_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = MPU6515_ReadCalibration(client, cali);
		if (err)
			break;


		sensor_data.x = cali[MPU6515_AXIS_X] * MPU6515_FS_MAX_LSB / MPU6515_DEFAULT_LSB;
		sensor_data.y = cali[MPU6515_AXIS_Y] * MPU6515_FS_MAX_LSB / MPU6515_DEFAULT_LSB;
		sensor_data.z = cali[MPU6515_AXIS_Z] * MPU6515_FS_MAX_LSB / MPU6515_DEFAULT_LSB;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

#if INV_GYRO_AUTO_CALI == 1
	case GYROSCOPE_IOCTL_READ_SENSORDATA_RAW:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MPU6515_ReadGyroDataRaw(client, strbuf, MPU6515_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_READ_TEMPERATURE:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MPU6515_ReadTemperature(client, strbuf, MPU6515_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_GET_POWER_STATUS:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		MPU6515_ReadPowerStatus(client, strbuf, MPU6515_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;
#endif

	default:
		GYRO_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_mpu6515_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	GYRO_FUN();

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		GYRO_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_GYROSCOPE_IOCTL_INIT:
	case COMPAT_GYROSCOPE_IOCTL_SMT_DATA:
	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA_RAW:
	case COMPAT_GYROSCOPE_IOCTL_READ_TEMPERATURE:
	case COMPAT_GYROSCOPE_IOCTL_GET_POWER_STATUS:
	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
		/* NVRAM will use below ioctl */
	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:{
			GYRO_LOG("compat_ion_ioctl : GYROSCOPE_IOCTL_XXX command is 0x%x\n", cmd);
			return filp->f_op->unlocked_ioctl(filp, cmd,
							  (unsigned long)compat_ptr(arg));
		}
	default:{
			GYRO_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
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
	.name = "gyroscope",
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
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & GYRO_TRC_INFO)
		GYRO_FUN();

	if (msg.event == PM_EVENT_SUSPEND) {
		atomic_set(&obj->suspend, 1);
		MPU6515_SetPWR_MGMT_2(client, false);
#ifndef CUSTOM_KERNEL_SENSORHUB
		err = MPU6515_SetPowerMode(client, false);
#else
		err = MPU6515_SCP_SetPowerMode(false, ID_GYROSCOPE);
#endif
		if (err <= 0)
			return err;

	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_resume(struct i2c_client *client)
{
	struct mpu6515_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	MPU6515_power(obj->hw, 1);
	MPU6515_SetPWR_MGMT_2(client, enable_status);
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = mpu6515_init_client(client, false);
#else
	err = MPU6515_SCP_SetPowerMode(enable_status, ID_GYROSCOPE);
#endif
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
static void mpu6515_early_suspend(struct early_suspend *h)
{
	struct mpu6515_i2c_data *obj = container_of(h, struct mpu6515_i2c_data, early_drv);
	int err;
	/* u8 databuf[2]; */
	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	MPU6515_SetPWR_MGMT_2(obj->client, false);
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = MPU6515_SetPowerMode(obj->client, false);
#else
	err = MPU6515_SCP_SetPowerMode(false, ID_GYROSCOPE);
#endif
	if (err) {
		GYRO_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;

	MPU6515_power(obj->hw, 0);
}

/*----------------------------------------------------------------------------*/
static void mpu6515_late_resume(struct early_suspend *h)
{
	struct mpu6515_i2c_data *obj = container_of(h, struct mpu6515_i2c_data, early_drv);
	int err;

	GYRO_FUN();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return;
	}

	MPU6515_power(obj->hw, 1);
	MPU6515_SetPWR_MGMT_2(obj->client, enable_status);
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = mpu6515_init_client(obj->client, false);
#else
	err = MPU6515_SCP_SetPowerMode(enable_status, ID_GYROSCOPE);
#endif
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

/*----------------------------------------------------------------------------*/
/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */
static int gyroscope_enable_nodata(int en)
{
	int err = 0;

#ifdef GSENSOR_UT
	GYRO_FUN();
#endif

	mutex_lock(&gyroscope_mutex);
	if (((en == 0) && (sensor_power == false)) || ((en == 1) && (sensor_power == true))) {
		enable_status = sensor_power;
		GYRO_LOG("Gsensor device have updated!\n");
	} else {
		enable_status = !sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0) {
#ifndef CUSTOM_KERNEL_SENSORHUB
			err = MPU6515_SetPowerMode(obj_i2c_data->client, enable_status);
#else
			err = MPU6515_SCP_SetPowerMode(enable_status, ID_GYROSCOPE);
			if (0 == err)
				sensor_power = enable_status;

#endif
			MPU6515_SetPWR_MGMT_2(obj_i2c_data->client, enable_status);
			GYRO_LOG
			    ("Gsensor not in suspend gsensor_SetPowerMode!, enable_status = %d\n",
			     enable_status);
		} else {
			GYRO_LOG
			    ("Gsensor in suspend and can not enable or disable!enable_status = %d\n",
			     enable_status);
		}
	}
	mutex_unlock(&gyroscope_mutex);

	if (err != MPU6515_SUCCESS) {
		GYRO_ERR("gsensor_enable_nodata fail!\n");
		return -1;
	}

	GYRO_LOG("gsensor_enable_nodata OK!\n");
	return 0;
}

/*----------------------------------------------------------------------------*/
static int gyroscope_set_delay(u64 ns)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int gyroscope_get_data(int *x, int *y, int *z, int *status)
{
	char buff[MPU6515_BUFSIZE];

#ifdef GSENSOR_UT
	GSE_FUN();
#endif

	mutex_lock(&gyroscope_mutex);
	MPU6515_ReadGyroData(obj_i2c_data->client, buff, MPU6515_BUFSIZE);
	mutex_unlock(&gyroscope_mutex);
	if (sscanf(buff, "%x %x %x", x, y, z) != 3)
		GYRO_ERR("sscanf parsing fail\n");
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

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
	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };
	int err = 0;

	GYRO_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!(obj)) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct mpu6515_i2c_data));

	obj->hw = get_cust_gyro();
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit_kfree;
	}


	GYRO_LOG("gyro_default_i2c_addr: %x\n", client->addr);
#ifdef MPU6515_ACCESS_BY_GSE_I2C
	obj->hw->addr = MPU6515_I2C_SLAVE_ADDR;	/* mtk i2c not allow to probe two same address */
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

#ifdef MPU6515_ACCESS_BY_GSE_I2C
	if (-1 == gsensor_init_flag)
		goto exit_init_failed;
#endif

	mpu6515_i2c_client = new_client;
	err = mpu6515_init_client(new_client, false);
	if (err)
		goto exit_init_failed;



	err = misc_register(&mpu6515_device);
	if (err) {
		GYRO_ERR("mpu6515_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

	err = mpu6515_create_attr(&gyroscope_init_info.platform_diver_addr->driver);
	if (err) {
		GYRO_ERR("mpu6515 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = gyroscope_open_report_data;
	ctl.enable_nodata = gyroscope_enable_nodata;
	ctl.set_delay = gyroscope_set_delay;
	ctl.is_report_input_direct = false;

	err = gyro_register_control_path(&ctl);
	if (err) {
		GYRO_ERR("register acc control path err\n");
		goto exit_kfree;
	}

	data.get_data = gyroscope_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		GYRO_ERR("register acc data path err\n");
		goto exit_kfree;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND) && defined(USE_EARLY_SUSPEND)
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	    obj->early_drv.suspend = mpu6515_early_suspend,
	    obj->early_drv.resume = mpu6515_late_resume, register_early_suspend(&obj->early_drv);
#endif

#if INV_GYRO_AUTO_CALI == 1
	mutex_init(&obj->temperature_mutex);
	mutex_init(&obj->raw_data_mutex);
#endif

	gyroscope_init_flag = 0;
	GYRO_LOG("%s: OK\n", __func__);
	return 0;

exit_create_attr_failed:
	misc_deregister(&mpu6515_device);
exit_misc_device_register_failed:
exit_init_failed:
	/* i2c_detach_client(new_client); */
exit_kfree:
	kfree(obj);
exit:
	GYRO_ERR("%s: err = %d\n", __func__, err);
	gyroscope_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu6515_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = mpu6515_delete_attr(&gyroscope_init_info.platform_diver_addr->driver);
	if (err)
		GYRO_ERR("mpu6515_delete_attr fail: %d\n", err);


	err = misc_deregister(&mpu6515_device);
	if (err)
		GYRO_ERR("misc_deregister fail: %d\n", err);


	err = hwmsen_detach(ID_ACCELEROMETER);
	if (err)
		GYRO_ERR("hwmsen_detach fail: %d\n", err);


	mpu6515_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int gyroscope_local_init(struct platform_device *pdev)
{
	struct gyro_hw *hw = get_cust_gyro();

	gyroPltFmDev = pdev;

	GYRO_FUN();
	MPU6515_power(hw, 1);
	if (i2c_add_driver(&mpu6515_i2c_driver)) {
		GYRO_ERR("add driver error\n");
		return -1;
	}

	if (-1 == gyroscope_init_flag)
		return -1;


	return 0;
}

/*----------------------------------------------------------------------------*/
static int gyroscope_remove(void)
{
	struct gyro_hw *hw = get_cust_gyro();

	GYRO_FUN();
	MPU6515_power(hw, 0);
	i2c_del_driver(&mpu6515_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init mpu6515_init(void)
{
	const char *name = "mediatek,mpu6515g";

	hw = get_gyro_dts_func(name, hw);
	if (!hw)
		GYRO_ERR("get cust_gyro dts info fail\n");

	GYRO_LOG("%s: i2c_number=%d\n", __func__, hw->i2c_num);
#ifdef CONFIG_MTK_LEGACY
	i2c_register_board_info(hw->i2c_num, &i2c_mpu6515, 1);
#endif
	gyro_driver_add(&gyroscope_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit mpu6515_exit(void)
{
	GYRO_FUN();
}

/*----------------------------------------------------------------------------*/
module_init(mpu6515_init);
module_exit(mpu6515_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6515 gyroscope driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");

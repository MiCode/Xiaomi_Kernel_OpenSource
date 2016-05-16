/* ltr559.c
 * LTR-On LTR-559 Proxmity and Light sensor driver
 *
 * Copyright (C) 2011 Lite-On Technology Corp (Singapore)
 * Copyright (C) 2016 XiaoMi, Inc.
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
/*
 *  2011-11-18 Thundersoft porting to MSM7x27A platform.
 *  2011-05-01 Lite-On created base driver.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>


#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/input/ltr559.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/hardware_info.h>


#define SENSOR_NAME			"proximity"
#define LTR559_DRV_NAME		"ltr559"
#define LTR559_MANUFAC_ID	0x05

#define VENDOR_NAME				"lite-on"
#define LTR559_SENSOR_NAME		"ltr559als"
#define DRIVER_VERSION		"1.0"

#define ALSPS_LS_ENABLE	_IOW('c', 5, int *)
#define ALSPS_PS_ENABLE	_IOW('c', 6, int *)
#define ALSPS_LSENSOR_LUX_DATA	_IOW('c', 7, int *)
#define ALSPS_PSENSOR_ABS_DISTANCE_DATA	_IOW('c', 8, int *)
#define ALSPS_GET_PS_RAW_DATA_FOR_CALI	_IOW('c', 9, int *)
#define ALSPS_REC_PS_DATA_FOR_CALI	_IOW('c', 10, int *)

#define DYNAMIC_CALIBRATE 1


#if defined(CONFIG_L6140_COMMON) || defined(CONFIG_L6300_COMMON)
#define SYS_AUTHORITY_FOR_CTS
#endif

#ifdef SYS_AUTHORITY_FOR_CTS
#define SYS_AUTHORITY		(S_IRUGO|S_IWUSR|S_IWGRP)
#else
#define SYS_AUTHORITY		(S_IRUGO|S_IWUGO)
#endif



#ifdef DEBUG
#define wing_info(fmt, ...) \
	printk("[wingtech] %s: "pr_fmt(fmt), __FUNCTION__, ##__VA_ARGS__)
#else
#define wing_info(fmt, ...)
#endif

struct ltr559_data {

	struct i2c_client *client;
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;
	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;

	struct wake_lock ps_wakelock;

	/* pinctrl data*/
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;

	struct ltr559_platform_data *platform_data;

	/* regulator data */
	bool power_state;
	struct regulator *vdd;
	struct regulator *vio;

	/* interrupt type is level-style */
	struct mutex lockw;
	struct mutex op_lock;

	struct delayed_work ps_work;
	struct delayed_work als_work;

	u8 ps_open_state;
	u8 als_open_state;

	u16 irq;

	u32 ps_state;
	u32 last_lux;
#if defined(DYNAMIC_CALIBRATE)
	bool cali_update;
	u32 dynamic_noise;
#endif
};
struct ltr559_data *sensor_info;

struct ltr559_reg {
	const char *name;
	u8 addr;
	u16 defval;
	u16 curval;
};

enum ltr559_reg_tbl{
	REG_ALS_CONTR,
	REG_PS_CONTR,
	REG_ALS_PS_STATUS,
	REG_INTERRUPT,
	REG_PS_LED,
	REG_PS_N_PULSES,
	REG_PS_MEAS_RATE,
	REG_ALS_MEAS_RATE,
	REG_MANUFACTURER_ID,
	REG_INTERRUPT_PERSIST,
	REG_PS_THRES_LOW,
	REG_PS_THRES_UP,
	REG_ALS_THRES_LOW,
	REG_ALS_THRES_UP,
	REG_ALS_DATA_CH1,
	REG_ALS_DATA_CH0,
	REG_PS_DATA
};

static int ltr559_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable);
static int ltr559_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable);

#if defined(DYNAMIC_CALIBRATE)
static ssize_t ltr559_ps_dynamic_caliberate_init(struct i2c_client *client, struct sensors_classdev *sensors_cdev);
#endif

static  struct ltr559_reg reg_tbl[] = {
	{
		.name   = "ALS_CONTR",
		.addr   = 0x80,
		.defval = 0x00,
		.curval = 0x0D,
	},
	{
		.name = "PS_CONTR",
		.addr = 0x81,
		.defval = 0x00,
		.curval = 0x03,
	},
	{
		.name = "ALS_PS_STATUS",
		.addr = 0x8c,
		.defval = 0x00,
		.curval = 0x00,
	},
	{
		.name = "INTERRUPT",
		.addr = 0x8f,
		.defval = 0x00,
		.curval = 0x01,
	},

#if defined(CONFIG_L6140_COMMON) || defined(CONFIG_L6300_COMMON)
	{
		.name = "PS_LED",
		.addr = 0x82,
		.defval = 0x7f,
		.curval = 0x1f,
	},
	{
		.name = "PS_N_PULSES",
		.addr = 0x83,
		.defval = 0x01,
		.curval = 0x0f,
	},
#else
	{
		.name = "PS_LED",
		.addr = 0x82,
		.defval = 0x7f,
		.curval = 0x7b,
	},
	{
		.name = "PS_N_PULSES",
		.addr = 0x83,
		.defval = 0x01,
		.curval = 0x02,
	},
#endif
	{
		.name = "PS_MEAS_RATE",
		.addr = 0x84,
		.defval = 0x02,
		.curval = 0x08,
	},
	{
		.name = "ALS_MEAS_RATE",
		.addr = 0x85,


		.defval = 0x03,
		.curval = 0x01,
	},
	{
		.name = "MANUFACTURER_ID",
		.addr = 0x87,
		.defval = 0x05,
		.curval = 0x05,
	},
	{
		.name = "INTERRUPT_PERSIST",
		.addr = 0x9e,
		.defval = 0x00,
		.curval = 0x23,
	},
	{
		.name = "PS_THRES_LOW",
		.addr = 0x92,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "PS_THRES_UP",
		.addr = 0x90,
		.defval = 0x07ff,
		.curval = 0x0000,
	},
	{
		.name = "ALS_THRES_LOW",
		.addr = 0x99,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "ALS_THRES_UP",
		.addr = 0x97,
		.defval = 0xffff,

		.curval = 0x0000,
	},
	{
		.name = "ALS_DATA_CH1",
		.addr = 0x88,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "ALS_DATA_CH0",
		.addr = 0x8a,
		.defval = 0x0000,
		.curval = 0x0000,
	},
	{
		.name = "PS_DATA",
		.addr = 0x8d,
		.defval = 0x0000,
		.curval = 0x0000,
	},
};

static struct sensors_classdev sensors_light_cdev = {
	.name = "liteon-light",
	.vendor = "liteon",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "60000",
	.resolution = "0.0125",
	.sensor_power = "0.20",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.flags = 2,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "liteon-proximity",
	.vendor = "liteon",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.flags = 3,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

#define LTR_I2C_RETRY_COUNT 3

static s32 ltr559_sensor_I2C_Wrtie(const struct i2c_client *client, u8 command, u8 value)
{
	int ret = 0;
	int retry;

	for (retry = 0; retry < LTR_I2C_RETRY_COUNT; retry++) {
		ret = i2c_smbus_write_byte_data(client, command, value);

		if (ret == 0)
			break;

		pr_err("%s, i2c write error, ret = %d\r\n", __func__, ret);
		mdelay(10);
 	}

	if (retry >= LTR_I2C_RETRY_COUNT) {
		pr_err("%s, i2c write retry over %d\n", __func__, LTR_I2C_RETRY_COUNT);
		return -EINVAL;
 	}

	return ret;
}

static s32 ltr559_sensor_I2C_Read(const struct i2c_client *client, u8 command)
{
	int ret = 0;
	int retry;

	for (retry = 0; retry < LTR_I2C_RETRY_COUNT; retry++) {
		ret = i2c_smbus_read_byte_data(client, command);

		if (ret >= 0)
			break;

		pr_err("%s, i2c read error, ret = %d\r\n", __func__, ret);
		mdelay(10);
 	}

	if (retry >= LTR_I2C_RETRY_COUNT) {
		pr_err("%s, i2c read retry over %d\n", __func__, LTR_I2C_RETRY_COUNT);
		return -EINVAL;
 	}

	return ret;
}


#if defined(DYNAMIC_CALIBRATE)
static int ltr559_ps_read(struct i2c_client *client)
{
	int psval_lo, psval_hi, psdata;
	psval_lo = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_0);
	if (psval_lo < 0) {
			psdata = psval_lo;
			goto out;
	}
	psval_hi = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_1);
	if (psval_hi < 0) {
			psdata = psval_hi;
			goto out;
	}

	psdata = ((psval_hi & 7) * 256) + psval_lo;



out:
	return psdata;
}
#endif

static int ltr559_chip_reset(struct i2c_client *client)
{
	int ret;

	ret = ltr559_sensor_I2C_Wrtie(client, LTR559_ALS_CONTR, MODE_ALS_StdBy);
	ret = ltr559_sensor_I2C_Wrtie(client, LTR559_PS_CONTR, MODE_PS_StdBy);
	ret = ltr559_sensor_I2C_Wrtie(client, LTR559_ALS_CONTR, 0x02);
	if (ret < 0)
		printk("%s reset chip fail\n", __func__);

	return ret;
}

static u32 ps_state_last = 0xff;


static void ltr559_set_ps_threshold(struct i2c_client *client, u8 addr, u16 value)
{
	int ret = 0;

	ret = ltr559_sensor_I2C_Wrtie(client, addr, (value & 0xff));
	if (ret < 0) {
		pr_err("%s: i2c write fail L, value =(%d) failed!\n", __func__, value);
		return;
	}
	ret = ltr559_sensor_I2C_Wrtie(client, addr+1, (value >> 8));
	if (ret < 0) {
		pr_err("%s: i2c write fail H, value =(%d) failed!\n", __func__, value);
		return;
	}
}

static int ltr559_ps_enable(struct i2c_client *client, int on)
{
	struct ltr559_data *data = i2c_get_clientdata(client);
	int ret = 0;
	int contr_data;

	if (on) {
		#if defined(DYNAMIC_CALIBRATE)
		if (0 != ltr559_ps_dynamic_caliberate_init(client, &data->ps_cdev)) {
			pr_err("%s, dynamic calibrate fail!", __func__);
			data->dynamic_noise = 800;
			data->platform_data->prox_threshold = 1000;
			data->platform_data->prox_hsyteresis_threshold = 900;
		}
		#endif
		ret = ltr559_sensor_I2C_Wrtie(client, LTR559_PS_CONTR, reg_tbl[REG_PS_CONTR].curval);
		msleep(WAKEUP_DELAY);

		if (ret < 0) {
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return ret;
		}
		contr_data = ltr559_sensor_I2C_Read(client, LTR559_PS_CONTR);
		if (contr_data != reg_tbl[REG_PS_CONTR].curval) {

			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return -EFAULT;
		}

		data->ps_state = 0xff;
		ps_state_last = 0xff;

		#if defined(DYNAMIC_CALIBRATE)
		ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_hsyteresis_threshold);
		ltr559_set_ps_threshold(data->client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold);
		#else
		ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_threshold);
		ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold-1);
		#endif

		wake_lock(&data->ps_wakelock);


	} else {

		wake_unlock(&data->ps_wakelock);

		ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, 0);
		ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, 0x7ff);


		ret = ltr559_sensor_I2C_Wrtie(client, LTR559_PS_CONTR, MODE_PS_StdBy);
		if (ret < 0) {
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return ret;
		}

		contr_data = ltr559_sensor_I2C_Read(client, LTR559_PS_CONTR);
		if (contr_data != reg_tbl[REG_PS_CONTR].defval) {
			pr_err("%s:  enable=(%d) failed!\n", __func__, on);
			return -EFAULT;
		}

		data->ps_state = 0xff;
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
		input_sync(data->input_dev_ps);

	}
	pr_err("%s: enable=(%d) OK\n", __func__, on);
	return ret;
}

/*
 * Absent Light Sensor Congfig
 */
static int ltr559_als_enable(struct i2c_client *client, int on)
{
	struct ltr559_data *data = i2c_get_clientdata(client);
	int ret;

	if (on) {
		ret = ltr559_sensor_I2C_Wrtie(client, LTR559_ALS_CONTR, reg_tbl[REG_ALS_CONTR].curval);
		msleep(WAKEUP_DELAY);
		ret |= ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH0_1);


		schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));

	} else {
		cancel_delayed_work_sync(&data->als_work);
		ret = ltr559_sensor_I2C_Wrtie(client, LTR559_ALS_CONTR, MODE_ALS_StdBy);
	}
	pr_err("%s: enable=(%d) ret=%d\n", __func__, on, ret);
	return ret;
}

static int ltr559_als_read(struct i2c_client *client)
{
	int alsval_ch0_lo, alsval_ch0_hi, alsval_ch0;
	int alsval_ch1_lo, alsval_ch1_hi, alsval_ch1;
	int luxdata;
	int ch1_co, ch0_co, ratio;

	alsval_ch1_lo = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH1_0);
	alsval_ch1_hi = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH1_1);
	if (alsval_ch1_lo < 0 || alsval_ch1_hi < 0)
		return -EPERM;
	alsval_ch1 = (alsval_ch1_hi << 8) + alsval_ch1_lo;

	alsval_ch0_lo = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH0_0);
	alsval_ch0_hi = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH0_1);
	if (alsval_ch0_lo < 0 || alsval_ch0_hi < 0)
		return -EPERM;
	alsval_ch0 = (alsval_ch0_hi << 8) + alsval_ch0_lo;


	if ((alsval_ch0 + alsval_ch1) == 0) {
		pr_err("%s, (alsval_ch0 + alsval_ch1) == 0, retry", __func__);
		msleep(40);
		alsval_ch1_lo = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH1_0);
		alsval_ch1_hi = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH1_1);
		if (alsval_ch1_lo < 0 || alsval_ch1_hi < 0)
			return -EPERM;
		alsval_ch1 = (alsval_ch1_hi << 8) + alsval_ch1_lo;

		alsval_ch0_lo = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH0_0);
		alsval_ch0_hi = ltr559_sensor_I2C_Read(client, LTR559_ALS_DATA_CH0_1);
		if (alsval_ch0_lo < 0 || alsval_ch0_hi < 0)
			return -EPERM;
		alsval_ch0 = (alsval_ch0_hi << 8) + alsval_ch0_lo;

		if ((alsval_ch0 + alsval_ch1) == 0)
			ratio = 1000;
	} else {
		ratio = alsval_ch1 * 1000 / (alsval_ch1 + alsval_ch0);
	}

	if (ratio < 450) {
		ch0_co = 17743;
		ch1_co = -11059;
	} else if ((ratio >= 450) && (ratio < 640)) {
		ch0_co = 42785;
		ch1_co = 19548;
	} else if ((ratio >= 640) && (ratio < 850)) {
		ch0_co = 5926;
		ch1_co = -1185;
	} else if (ratio >= 850) {
		ch0_co = 0;
		ch1_co = 0;
	}
	luxdata = (alsval_ch0 * ch0_co - alsval_ch1 * ch1_co) / 10000;
		return luxdata;
}

static void ltr559_ps_work_func(struct work_struct *work)
{
	struct ltr559_data *data = container_of(work, struct ltr559_data, ps_work.work);
	struct i2c_client *client = data->client;
	int als_ps_status;
	int psval_lo, psval_hi, psdata;
	int i = 0;

#ifdef DEBUG
	static int cnt;
#endif
	mutex_lock(&data->op_lock);

#ifdef DEBUG
	wing_info("ltr559_ps_work_func cnt = %d.\n", cnt++);
#endif

	als_ps_status = ltr559_sensor_I2C_Read(client, LTR559_ALS_PS_STATUS);
	printk("%s ps_open_state=%d, als_ps_status=0x%x\n", __func__, data->ps_open_state, als_ps_status);
	if (als_ps_status < 0)
		goto workout;
	/* Here should check data status, ignore interrupt status. */
	/* Bit 0: PS Data
	 * Bit 1: PS interrupt
	 * Bit 2: ASL Data
	 * Bit 3: ASL interrupt
	 * Bit 4: ASL Gain 0: ALS measurement data is in dynamic range 2 (2 to 64k lux)
	 * 1: ALS measurement data is in dynamic range 1 (0.01 to 320 lux)
	 */

	if ((data->ps_open_state == 1) && (als_ps_status & 0x03)) {

		for (i = 0; i < 5; i++) {
			psval_lo = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_0);
			if (psval_lo < 0) {
				psdata = psval_lo;
				goto workout;
			}
			psval_hi = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_1);
			if (psval_hi < 0) {
				psdata = psval_hi;
				goto workout;
			}
			psdata = ((psval_hi & 7) << 8) | psval_lo;

			if (psdata <= 0) {
				pr_err("%s, ltr559 read data error, psdata = %d!\n", __func__, psdata);
				msleep(10);
			} else if (psdata < 10) {
				msleep(10);
				psval_lo = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_0);
				if (psval_lo < 0) {
					psdata = psval_lo;
					goto workout;
				}
				psval_hi = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_1);
				if (psval_hi < 0) {
					psdata = psval_hi;
					goto workout;
				}
				psdata = ((psval_hi & 7) << 8) | psval_lo;
				if (psdata > 0)
					break;
			} else {
				break;
			}
		}

		printk("%s ps data =%d(0x%x), psval_hi=0x%x, psval_lo=0x%x, data->dynamic_noise = %d\n", __func__, psdata, psdata, psval_hi, psval_lo, data->dynamic_noise);
		if (psdata >= data->platform_data->prox_threshold) {
			data->ps_state = 0;
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, data->platform_data->prox_hsyteresis_threshold);
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, 0x07ff);
		} else if (psdata <= data->platform_data->prox_hsyteresis_threshold) {
			data->ps_state = 1;

#if defined(DYNAMIC_CALIBRATE)
			if ((data->dynamic_noise >= 10 && (((int)data->dynamic_noise - psdata > 30) ||
					(psdata - (int)data->dynamic_noise > 15))) ||
					(ps_state_last == 0xff && (psdata - (int)data->dynamic_noise) < 200)) {

				data->dynamic_noise = psdata;

				if (psdata < 10) {
					data->dynamic_noise = 800;
					data->platform_data->prox_threshold = 1000;
					data->platform_data->prox_hsyteresis_threshold = 900;
				} else if (psdata < 50) {
					data->platform_data->prox_threshold = psdata+25;
					data->platform_data->prox_hsyteresis_threshold = psdata+20;
				} else if (psdata < 100) {
					data->platform_data->prox_threshold = psdata+40;
					data->platform_data->prox_hsyteresis_threshold = psdata+25;
				} else if (psdata < 200) {
					data->platform_data->prox_threshold = psdata+70;
					data->platform_data->prox_hsyteresis_threshold = psdata+40;
				} else if (psdata < 400) {
					data->platform_data->prox_threshold = psdata+100;
					data->platform_data->prox_hsyteresis_threshold = psdata+50;
				} else if (psdata < 800) {
					  data->platform_data->prox_threshold = psdata+200;
					  data->platform_data->prox_hsyteresis_threshold = psdata+100;
				 } else {
					  data->platform_data->prox_threshold = 1000;
					  data->platform_data->prox_hsyteresis_threshold = 900;

					  pr_err("ltr559 the proximity sensor rubber or structure is error!\n");
				 }

				}
#endif

			ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, 0);
			ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, data->platform_data->prox_threshold);
		} else {
			data->ps_state = ps_state_last;
		}

		if (ps_state_last != data->ps_state) {
			printk("%s, %d, %d\n", __func__, data->platform_data->prox_threshold, data->platform_data->prox_hsyteresis_threshold);
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
			input_sync(data->input_dev_ps);
			printk("%s, report ABS_DISTANCE=%s\n", __func__, data->ps_state ? "far" : "near");

			ps_state_last = data->ps_state;
		} else
			printk("%s, ps_state still %s\n", __func__, data->ps_state ? "far" : "near");
	}
workout:
	enable_irq(data->irq);
	mutex_unlock(&data->op_lock);
}

static void ltr559_als_work_func(struct work_struct *work)
{
	struct ltr559_data *data = container_of(work, struct ltr559_data, als_work.work);
	struct i2c_client *client = data->client;
	int als_ps_status;
	int als_data;
	ktime_t	timestamp;
#ifdef DEBUG
	static int cnt;
#endif
	mutex_lock(&data->op_lock);

#ifdef DEBUG
	wing_info("ltr559_als_work_func cnt = %d.\n", cnt++);
#endif
	timestamp = ktime_get();
	if (!data->als_open_state)
		goto workout;

	als_ps_status = ltr559_sensor_I2C_Read(client, LTR559_ALS_PS_STATUS);
	if (als_ps_status < 0)
		goto workout;


	if ((data->als_open_state == 1) && (als_ps_status & 0x04)) {
		als_data = ltr559_als_read(client);
		if (als_data > 50000)
			als_data = 50000;
		if ((als_data >= 0) && (als_data != data->last_lux)) {
			data->last_lux = als_data;
			input_report_abs(data->input_dev_als, ABS_MISC, als_data);
			input_event(data->input_dev_als, EV_SYN, SYN_TIME_SEC,
					ktime_to_timespec(timestamp).tv_sec);
			input_event(data->input_dev_als, EV_SYN, SYN_TIME_NSEC,
					ktime_to_timespec(timestamp).tv_nsec);
			input_sync(data->input_dev_als);
		}
	}

	schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));
workout:
	mutex_unlock(&data->op_lock);
}

static irqreturn_t ltr559_irq_handler(int irq, void *arg)
{
	struct ltr559_data *data = (struct ltr559_data *)arg;

#ifdef DEBUG
	static int cnt;
	wing_info("ltr559_irq_handler cnt = %d.\n", cnt++);
#endif
	if (NULL == data)
		return IRQ_HANDLED;
	disable_irq_nosync(data->irq);
	schedule_delayed_work(&data->ps_work, 0);
	return IRQ_HANDLED;
}

static int ltr559_gpio_irq(struct ltr559_data *data)
{
		struct device_node *np = data->client->dev.of_node;
	 int err = 0;

		data->platform_data->int_gpio = of_get_named_gpio_flags(np, "ltr, irq-gpio", 0, &data->platform_data->irq_gpio_flags);
		if (data->platform_data->int_gpio < 0)
			return -EIO;

		if (gpio_is_valid(data->platform_data->int_gpio)) {
			err = gpio_request(data->platform_data->int_gpio, "ltr559_irq_gpio");
			if (err) {
				printk("%s irq gpio request failed\n", __func__);
				return -EINTR;
			}

			err = gpio_direction_input(data->platform_data->int_gpio);
			if (err) {
				printk("%s set_direction for irq gpio failed\n", __func__);
				return -EIO;
			}
		}

		data->irq = data->client->irq = gpio_to_irq(data->platform_data->int_gpio);

		if (request_irq(data->irq, ltr559_irq_handler, IRQ_TYPE_LEVEL_LOW,
				LTR559_DRV_NAME, data)) {
			printk("%s Could not allocate ltr559_INT !\n", __func__);
			return -EINTR;
	 	}

		irq_set_irq_wake(data->irq, 1);

		printk(KERN_INFO "%s: INT No. %d", __func__, data->irq);
		return 0;
}


static void ltr559_gpio_irq_free(struct ltr559_data *data)
{
	free_irq(data->irq, data);
	gpio_free(data->platform_data->int_gpio);
}

static ssize_t ltr559_show_enable_ps(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->ps_open_state);
}

static ssize_t ltr559_store_enable_ps(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work, then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "enable 559 PS sensor -> %ld\n", val);

	mutex_lock(&data->lockw);
	ltr559_ps_set_enable(&data->ps_cdev, (unsigned int)val);
	mutex_unlock(&data->lockw);

	return size;
}

static ssize_t ltr559_show_enable_als(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", data->als_open_state);
}

static ssize_t ltr559_store_enable_als(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work, then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);

	printk(KERN_INFO "enable 559 ALS sensor -> %ld\n", val);

	mutex_lock(&data->lockw);
	ltr559_als_set_enable(&data->als_cdev, (unsigned int)val);
	mutex_unlock(&data->lockw);
	return size;
}

static ssize_t ltr559_driver_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Chip: %s %s\nVersion: %s\n",
				VENDOR_NAME, LTR559_SENSOR_NAME, DRIVER_VERSION);
}

static ssize_t ltr559_show_debug_regs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 val, high, low;
	int i;
	char *after;
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	after = buf;

	after += sprintf(after, "%-17s%5s%14s%16s\n", "Register Name", "address", "default", "current");
	for (i = 0; i < sizeof(reg_tbl)/sizeof(reg_tbl[0]); i++) {
		if (reg_tbl[i].name == NULL || reg_tbl[i].addr == 0) {
			break;
		}
		if (i < 10) {
			val = ltr559_sensor_I2C_Read(client, reg_tbl[i].addr);
			after += sprintf(after, "%-20s0x%02x\t  0x%02x\t\t  0x%02x\n", reg_tbl[i].name, reg_tbl[i].addr, reg_tbl[i].defval, val);
		} else {
			low = ltr559_sensor_I2C_Read(client, reg_tbl[i].addr);
			high = ltr559_sensor_I2C_Read(client, reg_tbl[i].addr+1);
			after += sprintf(after, "%-20s0x%02x\t0x%04x\t\t0x%04x\n", reg_tbl[i].name, reg_tbl[i].addr, reg_tbl[i].defval, (high << 8) + low);
		}
	}
	after += sprintf(after, "\nYou can echo '0xaa=0xbb' to set the value 0xbb to the register of address 0xaa.\n ");

	return after - buf;
}

static ssize_t ltr559_store_debug_regs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work, then ALS must be enable */
	char *after, direct;
	u8 addr, val;
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	addr = simple_strtoul(buf, &after, 16);
	direct = *after;
	val = simple_strtoul((after+1), &after, 16);

	if (!((addr >= 0x80 && addr <= 0x93)
				|| (addr >= 0x97 && addr <= 0x9e)))
		return -EINVAL;

	mutex_lock(&data->lockw);
	if (direct == '=')
		ltr559_sensor_I2C_Wrtie(client, addr, val);
	else
		printk("%s: register(0x%02x) is: 0x%02x\n", __func__, addr, ltr559_sensor_I2C_Read(client, addr));
	mutex_unlock(&data->lockw);

	return after - buf;
}

static ssize_t ltr559_show_adc_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	u8 high, low;
	char *after;

	after = buf;

	low = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_0);
	high = ltr559_sensor_I2C_Read(client, LTR559_PS_DATA_1);
	if (low < 0 || high < 0)
		after += sprintf(after, "Failed to read PS adc data.\n");
	else
		after += sprintf(after, "%d\n", (high << 8) + low);

	return after - buf;
}

static ssize_t ltr559_show_lux_data(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int lux;
	struct ltr559_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	lux = ltr559_als_read(client);

	return sprintf(buf, "%d\n", lux);
}

static DEVICE_ATTR(debug_regs, SYS_AUTHORITY, ltr559_show_debug_regs,
				ltr559_store_debug_regs);
static DEVICE_ATTR(enable_als_sensor, SYS_AUTHORITY, ltr559_show_enable_als,
				ltr559_store_enable_als);
static DEVICE_ATTR(enable, SYS_AUTHORITY, ltr559_show_enable_ps,
				ltr559_store_enable_ps);
static DEVICE_ATTR(info, S_IRUGO, ltr559_driver_info_show, NULL);
static DEVICE_ATTR(raw_adc, S_IRUGO, ltr559_show_adc_data, NULL);
static DEVICE_ATTR(lux_adc, S_IRUGO, ltr559_show_lux_data, NULL);

static struct attribute *ltr559_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_info.attr,
	&dev_attr_enable_als_sensor.attr,
	&dev_attr_debug_regs.attr,
	&dev_attr_raw_adc.attr,
	&dev_attr_lux_adc.attr,
	NULL,
};

static const struct attribute_group ltr559_attr_group = {
	.attrs = ltr559_attributes,
};

static int ltr559_als_set_poll_delay(struct ltr559_data *data, unsigned long delay)
{
	mutex_lock(&data->op_lock);
	if (delay < 1)
		delay = 1;
	if (delay > 1000)
		delay = 1000;

	if (data->platform_data->als_poll_interval != delay) {
		data->platform_data->als_poll_interval = delay;
	}

	if (!data->als_open_state)
		goto workout;
	pr_info("%s poll_interval=%d\n", __func__, data->platform_data->als_poll_interval);


workout:
	mutex_unlock(&data->op_lock);
	return 0;
}

static int ltr559_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, als_cdev);
	int ret = 0;

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	if (data->als_open_state != enable) {
		ret = ltr559_als_enable(data->client, enable);
		if (ret < 0) {
			pr_err("%s: enable(%d) failed!\n", __func__, enable);
			return -EFAULT;
		}

		data->als_open_state = enable;
	} else {
		pr_err("%s: als_open_state already = %d\n", __func__, data->als_open_state);
	}
	pr_err("%s: enable=(%d), data->als_open_state=%d\n", __func__, enable, data->als_open_state);
	return ret;
}
static int ltr559_als_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, als_cdev);
	ltr559_als_set_poll_delay(data, delay_msec);
	return 0;
}

#if defined(DYNAMIC_CALIBRATE)
static ssize_t ltr559_ps_dynamic_caliberate_init(struct i2c_client *client, struct sensors_classdev *sensors_cdev)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, ps_cdev);
	struct ltr559_platform_data *pdata = data->platform_data;
	int i = 0;
	int ps;
	int data_total = 0;
	int noise = 0;
	int count = 5;
	int max = 0;

	int ret = 0;
	int contr_data;


	if (!data) {
		pr_err("ltr559_data is null!!\n");
		return -EFAULT;
	}


	ltr559_set_ps_threshold(client, LTR559_PS_THRES_LOW_0, 0x0);
	ltr559_set_ps_threshold(client, LTR559_PS_THRES_UP_0, 0x7ff);
	ret = ltr559_sensor_I2C_Wrtie(client, LTR559_PS_CONTR, reg_tbl[REG_PS_CONTR].curval);

	if (ret < 0) {
		pr_err("%s:ps enable failed!\n", __func__);
		return ret;
	}
	msleep(WAKEUP_DELAY);

	contr_data = ltr559_sensor_I2C_Read(client, LTR559_PS_CONTR);
	if (contr_data != reg_tbl[REG_PS_CONTR].curval) {

		ltr559_sensor_I2C_Wrtie(client, LTR559_PS_CONTR, MODE_PS_StdBy);

		pr_err("%s: ps status check failed!\n", __func__);
		return -EFAULT;
	}

	for (i = 0; i < count; i++) {


		msleep(10);

		ps = ltr559_ps_read(data->client);
		if (ps <= 0) {
			pr_err("%s, ltr559 read data error, ps = %d!\n", __func__, ps);
			i--;
			continue;
		}

		if (ps & 0x8000) {
			noise = 0;
			data_total = 0;
			pr_err("ltr559 read data overflow!\n");
			break;
		} else {
			noise = ps;
		}

		data_total += ps;

		if (max++ > 10) {
			pr_err("ltr559 read data error!\n");

		}
	}

	noise = data_total/count;

	if (noise == 0) {
		data->dynamic_noise = 800;
		pdata->prox_threshold = 1000;
		pdata->prox_hsyteresis_threshold = 900;
	} else if (noise < (data->dynamic_noise + 200)) {
		data->dynamic_noise = noise;
		if (noise < 50) {
			pdata->prox_threshold = noise+25;
			pdata->prox_hsyteresis_threshold = noise+20;
		} else if (noise < 100) {
			pdata->prox_threshold = noise+40;
			pdata->prox_hsyteresis_threshold = noise+25;
		} else if (noise < 200) {
			pdata->prox_threshold = noise+70;
			pdata->prox_hsyteresis_threshold = noise+40;
		} else if (noise < 400) {
			pdata->prox_threshold = noise+100;
			pdata->prox_hsyteresis_threshold = noise+50;
		} else if (noise < 800) {
			pdata->prox_threshold = noise+200;
			pdata->prox_hsyteresis_threshold = noise+100;
		} else {
			pdata->prox_threshold = 1000;
			pdata->prox_hsyteresis_threshold = 900;
			pr_err("ltr559 the proximity sensor rubber or structure is error!\n");
		}
	}

	data->cali_update = true;

	printk("%s : noise = %d , thd_val_low = %d , htd_val_high = %d \n", __func__, noise, pdata->prox_hsyteresis_threshold, pdata->prox_threshold);

	ret = ltr559_sensor_I2C_Wrtie(client, LTR559_PS_CONTR, MODE_PS_StdBy);
	if (ret < 0) {
		pr_err("%s: disable failed!\n", __func__);
		return ret;
	}

	contr_data = ltr559_sensor_I2C_Read(client, LTR559_PS_CONTR);
	if (contr_data != reg_tbl[REG_PS_CONTR].defval) {

		pr_err("%s: disable status check failed!\n", __func__);
		return -EFAULT;
	}


	return 0;
}
#endif

/* PS open fops */
static int ps_open(struct inode *inode, struct file *file)
{
	file->private_data = sensor_info;
	return nonseekable_open(inode, file);
}


/* PS release fops */
static int ps_release(struct inode *inode, struct file *file)
{

	return 0;
}

/* PS IOCTL */
static long ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0, val = 0, lux = 0, enable = 0;
	int value;
	int psval_lo, psval_hi;
	u16 buf[2];
	struct ltr559_data *ltr559 = sensor_info;
	printk("%s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {

	case ALSPS_REC_PS_DATA_FOR_CALI:

		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
		break;
		}
		if (val == 0)
			val = 300;
		#if !(DYNAMIC_CALIBRATE)
		ltr559->platform_data->prox_hsyteresis_threshold = val + FAR_THRES_DATA;
		ltr559->platform_data->prox_threshold = val + NEAR_THRES_DATA;
		#endif

		ltr559_set_ps_threshold(ltr559->client, LTR559_PS_THRES_LOW_0, val + FAR_THRES_DATA);
		ltr559_set_ps_threshold(ltr559->client, LTR559_PS_THRES_UP_0, val + NEAR_THRES_DATA);
		printk("ltr559--- nv data = %d\n", val);
		break;
	case ALSPS_GET_PS_RAW_DATA_FOR_CALI:
		psval_lo = ltr559_sensor_I2C_Read(ltr559->client, LTR559_PS_DATA_0);
		if (psval_lo < 0) {
			return psval_lo;
		}
		psval_hi = ltr559_sensor_I2C_Read(ltr559->client, LTR559_PS_DATA_1);
		if (psval_hi < 0) {
			return psval_hi;
		}
		value = ((psval_hi & 7) << 8) | psval_lo;
		buf[0] = value;
		buf[1] = ltr559->ps_state;

		if (copy_to_user((unsigned long __user *)arg, buf, sizeof(buf)))
			return -EFAULT;
		printk("ltr559--- fastmmi rawdata = %d, ltr559->ps_state=%d\n", value, ltr559->ps_state);
		break;
	case ALSPS_LSENSOR_LUX_DATA:
		lux = ltr559_als_read(ltr559->client);
		put_user(lux, (unsigned long __user *)arg);
		printk("ltr559--- lsensor-lux = %d\n", lux);
		break;
	case ALSPS_PSENSOR_ABS_DISTANCE_DATA:
		put_user(ltr559->ps_state, (unsigned long __user *)arg);
		printk("ltr559--- distance_flag = %d\n", ltr559->ps_state);
		break;

	case ALSPS_PS_ENABLE:
		if (get_user(enable, (unsigned long __user *)arg)) {
			rc = -EFAULT;
		break;
		}
		printk("ltr559 ps_enable = %d\n", enable);
		ltr559_ps_set_enable(&ltr559->ps_cdev, (unsigned int)enable);
		break;

	case ALSPS_LS_ENABLE:
		if (get_user(enable, (unsigned long __user *)arg)) {
			rc = -EFAULT;
		break;
		}
		printk("ltr559 ls_enable = %d\n", enable);
		ltr559_als_set_enable(&ltr559->als_cdev, (unsigned int)enable);

		break;
	default:
		pr_err("%s: INVALID COMMAND %d\n",
			__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

		return 0;
}

static const struct file_operations ps_fops = {
	.owner = THIS_MODULE,
	.open = ps_open,
	.release = ps_release,
	.unlocked_ioctl = ps_ioctl,
};

struct miscdevice ps_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "psensor",
	.fops = &ps_fops,
};

static int ltr559_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct ltr559_data *data = container_of(sensors_cdev, struct ltr559_data, ps_cdev);
	int ret = 0;

	printk("%s: enable(%d) start!\n", __func__, enable);

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	ret = ltr559_ps_enable(data->client, enable);
	if (ret < 0) {
		pr_err("%s: enable(%d) failed!\n", __func__, enable);
		return -EFAULT;
	}

	data->ps_open_state = enable;
	pr_err("%s: enable=(%d), data->ps_open_state=%d\n", __func__, enable, data->ps_open_state);
	return ret;
}

static int ltr559_suspend(struct device *dev)
{
	struct ltr559_data *data = dev_get_drvdata(dev);
	int ret = 0;

	printk("%s\n", __func__);
	mutex_lock(&data->lockw);
	ret |= ltr559_als_enable(data->client, 0);
	mutex_unlock(&data->lockw);
	return ret;
}

static int ltr559_resume(struct device *dev)
{
	struct ltr559_data *data = dev_get_drvdata(dev);
	int ret;

	printk("%s\n", __func__);
	mutex_lock(&data->lockw);
	if (data->als_open_state == 1)
		ret = ltr559_als_enable(data->client, 1);
	mutex_unlock(&data->lockw);
	return ret;
}

static int ltr559_check_chip_id(struct i2c_client *client)
{
	int id;

	id = ltr559_sensor_I2C_Read(client, LTR559_MANUFACTURER_ID);
	printk("%s read the  LTR559_MANUFAC_ID is 0x%x\n", __func__, id);
	if (id != LTR559_MANUFAC_ID) {
		return -EINVAL;
	}
	return 0;
}

int ltr559_device_init(struct i2c_client *client)
{
	int retval = 0;
	int i;

	retval = ltr559_sensor_I2C_Wrtie(client, LTR559_ALS_CONTR, 0x02);
	if (retval < 0) {
		pr_err("%s   i2c_smbus_write_byte_data(LTR559_ALS_CONTR, 0x02);  ERROR !!!.\n", __func__);
	}

	msleep(WAKEUP_DELAY);
	for (i = 2; i < sizeof(reg_tbl)/sizeof(reg_tbl[0]); i++) {
		if (reg_tbl[i].name == NULL || reg_tbl[i].addr == 0) {
			break;
		}
		if (reg_tbl[i].defval != reg_tbl[i].curval) {
			if (i < 10) {
				retval = ltr559_sensor_I2C_Wrtie(client, reg_tbl[i].addr, reg_tbl[i].curval);
				pr_info("___CAOYI____  ltr559  write 0x%x to addr:0x%x\n", reg_tbl[i].curval, reg_tbl[i].addr);
				pr_info("___CAOYI____ write is OK(0) or Error (-x) ? = return is %d\n", ltr559_sensor_I2C_Wrtie(client, reg_tbl[i].addr, reg_tbl[i].curval));
			} else {
				retval = ltr559_sensor_I2C_Wrtie(client, reg_tbl[i].addr, reg_tbl[i].curval & 0xff);
				pr_info("___CAOYI____  ltr559  write 0x%x to addr:0x%x\n", reg_tbl[i].curval & 0xff, reg_tbl[i].addr);
				retval = ltr559_sensor_I2C_Wrtie(client, reg_tbl[i].addr + 1, reg_tbl[i].curval >> 8);
				pr_info("___CAOYI____  ltr559  write 0x%x to addr:0x%x\n", reg_tbl[i].curval >> 8, reg_tbl[i].addr);
			}
		}
	}

	pr_info("___CAOYI______ read addr 0x9e is 0x%x\n", ltr559_sensor_I2C_Read(client, 0x9e));
	pr_info("___CAOYI______ read addr 0x90 is 0x%x\n", ltr559_sensor_I2C_Read(client, 0x90));


	pr_info("___CAOYI______||||   read is OK(+x) or Error (-x) ? = return is %d\n", ltr559_sensor_I2C_Read(client, 0x1a));
	pr_info("___CAOYI______||||  write is OK(0) or Error (-x) ? = return is %d\n", ltr559_sensor_I2C_Wrtie(client, 0x1b, 0x88));
	pr_info("___CAOYI______||||  write is OK(0) or Error (-x) ? = return is %d\n", ltr559_sensor_I2C_Wrtie(client, 0x11, 0x88));

	return retval;
}

static int sensor_regulator_configure(struct ltr559_data *data, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0, LTR559_VDD_MAX_UV);
		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0, LTR559_VIO_MAX_UV);
		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev, "Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd, LTR559_VDD_MIN_UV, LTR559_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev, "Regulator set failed vdd rc=%d\n", rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev, "Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio, LTR559_VIO_MIN_UV, LTR559_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev, "Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;
reg_vio_put:
	regulator_put(data->vio);

reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, LTR559_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static int sensor_regulator_power_on(struct ltr559_data *data, bool on)
{
	int rc = 0;

	if (!on) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
			dev_err(&data->client->dev, "Regulator vio re-enabled rc=%d\n", rc);
			/*
			 * Successfully re-enable regulator.
			 * Enter poweron delay and returns error.
			 */
			if (!rc) {
				rc = -EBUSY;
				goto enable_delay;
			}
		}
		return rc;
	} else {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
			return rc;
		}
	}

enable_delay:
	msleep(130);
	dev_dbg(&data->client->dev, "Sensor regulator power on =%d\n", on);
	return rc;
}

static int sensor_platform_hw_power_onoff(struct ltr559_data *data, bool on)
{
	int err = 0;

	if (data->power_state != on) {
		if (on) {
			if (!IS_ERR_OR_NULL(data->pinctrl)) {
				err = pinctrl_select_state(data->pinctrl, data->pin_default);
				if (err) {
					dev_err(&data->client->dev, "Can't select pinctrl state on=%d\n", on);
					goto power_out;
				}
			}

			err = sensor_regulator_configure(data, true);
			if (err) {
				dev_err(&data->client->dev, "unable to configure regulator on=%d\n", on);
				goto power_out;
			}

			err = sensor_regulator_power_on(data, true);
			if (err) {
				dev_err(&data->client->dev, "Can't configure regulator on=%d\n", on);
				goto power_out;
			}

			data->power_state = true;
		} else {
			if (!IS_ERR_OR_NULL(data->pinctrl)) {
				err = pinctrl_select_state(data->pinctrl, data->pin_sleep);
				if (err) {
					dev_err(&data->client->dev, "Can't select pinctrl state on=%d\n", on);
					goto power_out;
				}
			}

			err = sensor_regulator_power_on(data, false);
			if (err) {
				dev_err(&data->client->dev, "Can't configure regulator on=%d\n", on);
				goto power_out;
			}

			err = sensor_regulator_configure(data, false);
			if (err) {
				dev_err(&data->client->dev, "unable to configure regulator on=%d\n", on);
				goto power_out;
			}

			data->power_state = false;
		}
	}
power_out:
	return err;
}

static int ltr559_pinctrl_init(struct ltr559_data *data)
{
	struct i2c_client *client = data->client;

	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(data->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(data->pinctrl);
	}

	data->pin_default =
		pinctrl_lookup_state(data->pinctrl, "default");
	if (IS_ERR_OR_NULL(data->pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(data->pin_default);
	}

	data->pin_sleep =
		pinctrl_lookup_state(data->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(data->pin_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(data->pin_sleep);
	}

	return 0;
}

static int ltr559_parse_dt(struct device *dev, struct ltr559_data *data)
{
	struct ltr559_platform_data *pdata = data->platform_data;
	struct device_node *np = dev->of_node;
	unsigned int tmp;
	int rc = 0;

	/* ps tuning data*/
	rc = of_property_read_u32(np, "ltr, ps-threshold", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps threshold\n");
		return rc;
	}
	pdata->prox_threshold = tmp;

	rc = of_property_read_u32(np, "ltr, ps-hysteresis-threshold", &tmp);
	 if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->prox_hsyteresis_threshold = tmp;

	rc = of_property_read_u32(np, "ltr, als-polling-time", &tmp);
	 if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->als_poll_interval = tmp;

	return 0;
}

int ltr559_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ltr559_data *data;
	struct ltr559_platform_data *pdata;
	int ret = 0;

	wing_info("start...\n");

	/* check i2c*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE | I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		dev_err(&client->dev, "LTR-559ALS functionality check failed.\n");
		return -EIO;
	}

	/* platform data memory allocation*/
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct ltr559_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "No platform data\n");
			return -ENODEV;
		}
	}

	/* data memory allocation */
	data = kzalloc(sizeof(struct ltr559_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit_kfree_pdata;
	}
	data->client = client;
	data->platform_data = pdata;
	sensor_info = data;
	ret = ltr559_parse_dt(&client->dev, data);
	if (ret) {
		dev_err(&client->dev, "can't parse platform data\n");
		ret = -EFAULT;
		goto exit_kfree_data;
	}
	data->dynamic_noise = 2047;
	/* pinctrl initialization */
	ret = ltr559_pinctrl_init(data);
	if (ret) {
		ret = -EFAULT;
		dev_err(&client->dev, "Can't initialize pinctrl\n");
		goto exit_kfree_data;
	}

	/* power initialization */
	ret = sensor_platform_hw_power_onoff(data, true);
	if (ret) {
		ret = -ENOEXEC;
		dev_err(&client->dev, "power on fail\n");
		goto exit_kfree_data;
	}

	wake_lock_init(&data->ps_wakelock, WAKE_LOCK_SUSPEND, "proximity");

	/* set client data as ltr559_data*/
	i2c_set_clientdata(client, data);

	ret = ltr559_check_chip_id(client);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev, "the manufacture id is not match\n");
		goto exit_power_off;
	}

	ret = ltr559_device_init(client);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev, "device init failed\n");
		goto exit_power_off;
	}

	/* request gpio and irq */
	ret = ltr559_gpio_irq(data);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev, "gpio_irq failed\n");
		goto exit_chip_reset;
	}

	/* Register Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}

	set_bit(EV_ABS, data->input_dev_als->evbit);
	set_bit(EV_ABS, data->input_dev_ps->evbit);

	input_set_abs_params(data->input_dev_als, ABS_MISC, 0, 65535, 0, 0);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);

#if defined (CONFIG_SENSORS_BMM050) || defined(CONFIG_L9100_COMMON) || defined(CONFIG_L8150_COMMON1111111)
	data->input_dev_als->name = "ltr559-ls";
	data->input_dev_ps->name = "ltr559-ps";
#else
	data->input_dev_als->name = "light";
	data->input_dev_ps->name = "proximity";
#endif
	data->input_dev_als->id.bustype = BUS_I2C;
	data->input_dev_als->dev.parent = &data->client->dev;
	data->input_dev_ps->id.bustype = BUS_I2C;
	data->input_dev_ps->dev.parent = &data->client->dev;

	input_set_drvdata(data->input_dev_als, data);
	input_set_drvdata(data->input_dev_ps, data);

	ret = input_register_device(data->input_dev_als);
	if (ret) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Unable to register input device als: %s\n", data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	ret = input_register_device(data->input_dev_ps);
	if (ret) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Unable to register input device ps: %s\n", data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}
	printk("%s input device success.\n", __func__);
	ret = misc_register(&ps_misc);
	if (ret < 0) {
		dev_err(&data->client->dev,
		"%s: PS Register Misc Device Fail...\n", __func__);
		goto  exit_free_dev_als;
	}
	/* init delayed works */
	INIT_DELAYED_WORK(&data->ps_work, ltr559_ps_work_func);
	INIT_DELAYED_WORK(&data->als_work, ltr559_als_work_func);

	/* init mutex */
	mutex_init(&data->lockw);
	mutex_init(&data->op_lock);

	/* create sysfs group */
	ret = sysfs_create_group(&client->dev.kobj, &ltr559_attr_group);

#if defined (CONFIG_SENSORS_BMM050) || defined(CONFIG_L9100_COMMON) || defined(DYNAMIC_CALIBRATE)
	ret = sysfs_create_group(&data->input_dev_als->dev.kobj, &ltr559_attr_group);
	ret = sysfs_create_group(&data->input_dev_ps->dev.kobj, &ltr559_attr_group);
#endif

	if (ret) {
		ret = -EROFS;
		dev_err(&client->dev, "Unable to creat sysfs group\n");
		goto exit_unregister_dev_ps;
	}

	/* Register sensors class */
	data->als_cdev = sensors_light_cdev;
	data->als_cdev.sensors_enable = ltr559_als_set_enable;
	data->als_cdev.sensors_poll_delay = ltr559_als_poll_delay;
	data->ps_cdev = sensors_proximity_cdev;
	data->ps_cdev.sensors_enable = ltr559_ps_set_enable;
	data->ps_cdev.sensors_poll_delay = NULL;

	ret = sensors_classdev_register(&client->dev, &data->als_cdev);
	if (ret) {
		ret = -EROFS;
		dev_err(&client->dev, "Unable to register to als sensor class\n");
		goto exit_remove_sysfs_group;
	}

	ret = sensors_classdev_register(&client->dev, &data->ps_cdev);
	if (ret) {
		ret = -EROFS;
		dev_err(&client->dev, "Unable to register to ps sensor class\n");
		goto exit_unregister_als_class;
	}


#if defined(DYNAMIC_CALIBRATE)
	if (0 != ltr559_ps_dynamic_caliberate_init(client, &data->ps_cdev)) {
		pr_err("%s, dynamic calibrate fail!", __func__);
		data->dynamic_noise = 800;
		pdata->prox_threshold = 1000;
		pdata->prox_hsyteresis_threshold = 900;
	}
#endif

	dev_dbg(&client->dev, "probe succece\n");

	wing_info("success.\n");
	return 0;

exit_unregister_als_class:
	sensors_classdev_unregister(&data->als_cdev);
exit_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &ltr559_attr_group);
#if defined (CONFIG_SENSORS_BMM050) || defined(CONFIG_L9100_COMMON) || defined(DYNAMIC_CALIBRATE)
	sysfs_remove_group(&data->input_dev_als->dev.kobj, &ltr559_attr_group);
	sysfs_remove_group(&data->input_dev_ps->dev.kobj, &ltr559_attr_group);
#endif
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
	if (data->input_dev_ps)
		input_free_device(data->input_dev_ps);
exit_free_dev_als:
	if (data->input_dev_als)
		input_free_device(data->input_dev_als);
exit_free_irq:
	ltr559_gpio_irq_free(data);
exit_chip_reset:
	ltr559_chip_reset(client);
exit_power_off:
	sensor_platform_hw_power_onoff(data, false);
	wake_lock_destroy(&data->ps_wakelock);

exit_kfree_pdata:
	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	data->platform_data = NULL;
	pdata = NULL;


exit_kfree_data:
	kfree(data);

	wing_info("failed.\n");
	return ret;
}

static int ltr559_remove(struct i2c_client *client)
{
	struct ltr559_data *data = i2c_get_clientdata(client);
	struct ltr559_platform_data *pdata = data->platform_data;

	if (data == NULL || pdata == NULL)
		return 0;

	ltr559_ps_enable(client, 0);
	ltr559_als_enable(client, 0);
	input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);

	input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);

	ltr559_gpio_irq_free(data);

	sysfs_remove_group(&client->dev.kobj, &ltr559_attr_group);
#if defined (CONFIG_SENSORS_BMM050) || defined(CONFIG_L9100_COMMON) || defined(DYNAMIC_CALIBRATE)
	sysfs_remove_group(&data->input_dev_als->dev.kobj, &ltr559_attr_group);
	sysfs_remove_group(&data->input_dev_ps->dev.kobj, &ltr559_attr_group);
#endif

	cancel_delayed_work_sync(&data->ps_work);
	cancel_delayed_work_sync(&data->als_work);

	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	pdata = NULL;

	wake_lock_destroy(&data->ps_wakelock);

	kfree(data);
	data = NULL;

	return 0;
}

static struct i2c_device_id ltr559_id[] = {
	{"ltr559", 0},
	{}
};

static struct of_device_id ltr_match_table[] = {
	{.compatible = "ltr, ltr559",},
	{},
};

MODULE_DEVICE_TABLE(i2c, ltr559_id);
static SIMPLE_DEV_PM_OPS(ltr559_pm_ops, ltr559_suspend, ltr559_resume);
static struct i2c_driver ltr559_driver = {
	.driver = {
		.name = LTR559_DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &ltr559_pm_ops,
		.of_match_table = ltr_match_table,
	},
	.probe = ltr559_probe,
	.remove = ltr559_remove,
	.id_table = ltr559_id,
};

static int ltr559_driver_init(void)
{
	pr_info("Driver ltr5590 init.\n");
	return i2c_add_driver(&ltr559_driver);
};

static void ltr559_driver_exit(void)
{
	pr_info("Unload ltr559 module...\n");
	i2c_del_driver(&ltr559_driver);
}

module_init(ltr559_driver_init);
module_exit(ltr559_driver_exit);
MODULE_AUTHOR("Lite-On Technology Corp.");
MODULE_DESCRIPTION("Lite-On LTR-559 Proximity and Light Sensor Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");


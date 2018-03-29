/* BOSCH Pressure Sensor Driver
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
 * History: V1.0 --- [2013.02.18]Driver creation
 *          V1.1 --- [2013.03.14]Instead late_resume, use resume to make sure
 *                               driver resume is ealier than processes resume.
 *          V1.2 --- [2013.03.26]Re-write i2c function to fix the bug that
 *                               i2c access error on MT6589 platform.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include "cust_hmdy.h"
#include "hts221.h"
#include "humidity.h"

#define POWER_NONE_MACRO MT65XX_POWER_NONE

static DEFINE_MUTEX(hts221_i2c_mutex);
static DEFINE_MUTEX(hts221_op_mutex);

/* sensor type */
enum SENSOR_TYPE_ENUM {
	HTS221_TYPE = 0x0,

	INVALID_TYPE = 0xff
};

/* power mode */
enum HTS_POWERMODE_ENUM {
	HTS_SUSPEND_MODE = 0,
	HTS_NORMAL_MODE = 1,

	HTS_UNDEFINED_POWERMODE = 0xff
};

/* trace */
enum HTS_TRC {
	HTS_TRC_READ = 0x01,
	HTS_TRC_RAWDATA = 0x02,
	HTS_TRC_IOCTL = 0x04,
	HTS_TRC_FILTER = 0x08,
};

/* s/w filter */
struct data_filter {
	u32 raw[C_MAX_FIR_LENGTH][HTS221_DATA_NUM];
	int sum[HTS221_DATA_NUM];
	int num;
	int idx;
};

/* hts221 calibration */
struct hts221_calibration_data {
	u8 temperature_calibration[2], temperature_calibration2[2];
	int calibX0, calibX1, calibW0, calibW1;
	u16 calibY0, calibY1;
	u16 calibZ0a, calibZ0b, calibZ1a, calibZ1b;
	int calibZ0, calibZ1;
	int h_slope, h_b, t_slope, t_b;
};

/* hts221 i2c client data */
struct hts221_i2c_data {
	struct i2c_client *client;
	struct hmdy_hw *hw;

	/* sensor info */
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	enum HTS_POWERMODE_ENUM power_mode;

	struct hts221_calibration_data hts221_cali;
	/* calculated temperature correction coefficient */
	s32 t_fine;

	/*misc */
	atomic_t trace;
	atomic_t suspend;
	atomic_t filter;

};

#define HTS_TAG                  "[hts221] "
#define HTS_FUN(f)
#define HTS_ERR(fmt, args...) \
	pr_err(HTS_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define HTS_LOG(fmt, args...)    pr_debug(HTS_TAG fmt, ##args)

static struct i2c_driver hts221_i2c_driver;
static struct hts221_i2c_data *obj_i2c_data;
static const struct i2c_device_id hts221_i2c_id[] = {
	{HTS_DEV_NAME, 0},
	{}
};
struct hmdy_hw hmdy_cust;
static struct hmdy_hw *hw = &hmdy_cust;
/* For alsp driver get cust info */
struct hmdy_hw *get_cust_hmdy(void)
{
	return &hmdy_cust;
}
static int hts221_local_init(void);
static int hts221_remove(void);
static int hts221_init_flag = -1;
static struct hmdy_init_info hts221_init_info = {
	.name = "hts221",
	.init = hts221_local_init,
	.uninit = hts221_remove,
};

/* I2C operation functions */
static int hts221_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err = 0;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&hts221_i2c_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&hts221_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		HTS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&hts221_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		HTS_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&hts221_i2c_mutex);
	return err;

}

static int hts221_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{				/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;

	mutex_lock(&hts221_i2c_mutex);

	if (!client) {
		mutex_unlock(&hts221_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		HTS_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&hts221_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		HTS_ERR("send command error!!\n");
		mutex_unlock(&hts221_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&hts221_i2c_mutex);
	return err;
}

static void hts221_power(struct hmdy_hw *hw, unsigned int on)
{

}

static int hts221_check_ID(struct i2c_client *client)
{
	int res = 0;
	u8 buf[2];

	HTS_FUN();

	res = hts221_i2c_read_block(client, REG_WHOAMI_ADDR, buf, 1);
	if (res < 0)
		goto error;
	HTS_LOG("HTS211 device ID is 0X%XH\n", buf[0]);
	return res;

 error:
	HTS_ERR("hts221_check_ID failed 0x%02x,0x%02x: %d\n", buf[0], buf[1], res);
	return res;
}

static int hts221_set_sampling_period(struct i2c_client *client, u8 new_sampling, u8 BDU)
{
	int res = 0;
	u8 buf[2];

	HTS_FUN();

	res = hts221_i2c_read_block(client, REG_CNTRL1_ADDR, buf, 1);
	if (res < 0)
		goto error;

	buf[0] = (buf[0] & HTS221_ODR_MASK) | new_sampling;
	buf[0] = (buf[0] & HTS221_BDU_MASK) | BDU;

	res = hts221_i2c_write_block(client, REG_CNTRL1_ADDR, buf, 1);
	if (res < 0)
		goto error;

	return res;

 error:
	HTS_ERR("update humidity resolution failed 0x%02x,0x%02x: %d\n", buf[0], buf[1], res);
	return res;
}

static int hts221_set_resolution(struct i2c_client *client, u8 new_resolution, u8 mask)
{
	int res = 0;
	u8 buf[2];

	HTS_FUN();

	res = hts221_i2c_read_block(client, REG_AVCONFIG_ADDR, buf, 1);
	if (res < 0)
		goto error;

	buf[0] = (buf[0] & mask) | new_resolution;

	res = hts221_i2c_write_block(client, REG_AVCONFIG_ADDR, buf, 1);
	if (res < 0)
		goto error;
	return res;

 error:
	HTS_ERR("update humidity resolution failed 0x%02x,0x%02x: %d\n", buf[0], buf[1], res);
	return res;
}

static int hts221_get_calibration_data(struct i2c_client *client)
{
	int err = 0;
	u8 humidity_calibration[2], temperature_calibration[2];

	HTS_FUN();
	err = hts221_i2c_read_block(client, REG_0RH_CAL_X_H + 0x80, humidity_calibration, 2);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibX0 = ((s32) ((s16) ((humidity_calibration[1] << 8) |
							   (humidity_calibration[0]))));

	err = hts221_i2c_read_block(client, REG_1RH_CAL_X_H + 0x80, humidity_calibration, 2);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibX1 = ((s32) ((s16) ((humidity_calibration[1] << 8) |
							   (humidity_calibration[0]))));

	err = hts221_i2c_read_block(client, REG_0RH_CAL_Y_H, humidity_calibration, 1);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibY0 = (u16) humidity_calibration[0];

	err = hts221_i2c_read_block(client, REG_1RH_CAL_Y_H, humidity_calibration, 1);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibY1 = (u16) humidity_calibration[0];

	obj_i2c_data->hts221_cali.h_slope =
	    ((obj_i2c_data->hts221_cali.calibY1 -
	      obj_i2c_data->hts221_cali.calibY0) * 8000) / (obj_i2c_data->hts221_cali.calibX1 -
							    obj_i2c_data->hts221_cali.calibX0);
	obj_i2c_data->hts221_cali.h_b =
	    (((obj_i2c_data->hts221_cali.calibX1 * obj_i2c_data->hts221_cali.calibY0) -
	      (obj_i2c_data->hts221_cali.calibX0 * obj_i2c_data->hts221_cali.calibY1)) * 1000) /
	    (obj_i2c_data->hts221_cali.calibX1 - obj_i2c_data->hts221_cali.calibX0);
	obj_i2c_data->hts221_cali.h_b = obj_i2c_data->hts221_cali.h_b * 8;

	err = hts221_i2c_read_block(client, REG_0T_CAL_X_L + 0x80, temperature_calibration, 2);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibW0 = ((s32) ((s16) ((temperature_calibration[1] << 8) |
							   (temperature_calibration[0]))));

	err = hts221_i2c_read_block(client, REG_1T_CAL_X_L + 0x80, temperature_calibration, 2);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibW1 = ((s32) ((s16) ((temperature_calibration[1] << 8) |
							   (temperature_calibration[0]))));

	err = hts221_i2c_read_block(client, REG_0T_CAL_Y_H, temperature_calibration, 1);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibZ0a = (u16) temperature_calibration[0];

	err = hts221_i2c_read_block(client, REG_T1_T0_CAL_Y_H, temperature_calibration, 1);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibZ0b = (u16) (temperature_calibration[0] & (0x3));
	obj_i2c_data->hts221_cali.calibZ0 =
	    ((s32) ((obj_i2c_data->hts221_cali.calibZ0b << 8) | (obj_i2c_data->hts221_cali.calibZ0a)));

	err = hts221_i2c_read_block(client, REG_1T_CAL_Y_H, temperature_calibration, 1);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibZ1a = (u16) temperature_calibration[0];

	err = hts221_i2c_read_block(client, REG_T1_T0_CAL_Y_H, temperature_calibration, 1);
	if (err < 0)
		return err;
	obj_i2c_data->hts221_cali.calibZ1b = (u16) (temperature_calibration[0] & (0xC));
	obj_i2c_data->hts221_cali.calibZ1b = obj_i2c_data->hts221_cali.calibZ1b >> 2;
	obj_i2c_data->hts221_cali.calibZ1 =
	    ((s32) ((obj_i2c_data->hts221_cali.calibZ1b << 8) | (obj_i2c_data->hts221_cali.calibZ1a)));

	obj_i2c_data->hts221_cali.t_slope =
	    ((obj_i2c_data->hts221_cali.calibZ1 -
	      obj_i2c_data->hts221_cali.calibZ0) * 8000) / (obj_i2c_data->hts221_cali.calibW1 -
							    obj_i2c_data->hts221_cali.calibW0);
	obj_i2c_data->hts221_cali.t_b =
	    (((obj_i2c_data->hts221_cali.calibW1 * obj_i2c_data->hts221_cali.calibZ0) -
	      (obj_i2c_data->hts221_cali.calibW0 * obj_i2c_data->hts221_cali.calibZ1)) * 1000) /
	    (obj_i2c_data->hts221_cali.calibW1 - obj_i2c_data->hts221_cali.calibW0);
	obj_i2c_data->hts221_cali.t_b = obj_i2c_data->hts221_cali.t_b * 8;
#ifdef DEBUF
	HTS_LOG("reading calibX0=%X calibX1=%X\n", obj_i2c_data->hts221_cali.calibX0,
		obj_i2c_data->hts221_cali.calibX1);
	HTS_LOG("reading calibX0=%d calibX1=%d\n", obj_i2c_data->hts221_cali.calibX0,
		obj_i2c_data->hts221_cali.calibX1);
	HTS_LOG("reading calibW0=%X calibW1=%X\n", obj_i2c_data->hts221_cali.calibW0,
		obj_i2c_data->hts221_cali.calibW1);
	HTS_LOG("reading calibW0=%d calibW1=%d\n", obj_i2c_data->hts221_cali.calibW0,
		obj_i2c_data->hts221_cali.calibW1);
	HTS_LOG("reading calibY0=%X calibY1=%X\n", obj_i2c_data->hts221_cali.calibY0,
		obj_i2c_data->hts221_cali.calibY1);
	HTS_LOG("reading calibY0=%u calibY1=%u\n", obj_i2c_data->hts221_cali.calibY0,
		obj_i2c_data->hts221_cali.calibY1);
	HTS_LOG("reading calibZ0a=%X calibZ0b=%X calibZ0=%X\n", obj_i2c_data->hts221_cali.calibZ0a,
		obj_i2c_data->hts221_cali.calibZ0b, obj_i2c_data->hts221_cali.calibZ0);
	HTS_LOG("reading calibZ0a=%u calibZ0b=%u calibZ0=%d\n", obj_i2c_data->hts221_cali.calibZ0a,
		obj_i2c_data->hts221_cali.calibZ0b, obj_i2c_data->hts221_cali.calibZ0);
	HTS_LOG("reading calibZ1a=%X calibZ1b=%X calibZ1=%X\n", obj_i2c_data->hts221_cali.calibZ1a,
		obj_i2c_data->hts221_cali.calibZ1b, obj_i2c_data->hts221_cali.calibZ1);
	HTS_LOG("reading calibZ1a=%u calibZ1b=%u calibZ1=%d\n", obj_i2c_data->hts221_cali.calibZ1a,
		obj_i2c_data->hts221_cali.calibZ1b, obj_i2c_data->hts221_cali.calibZ1);
	HTS_LOG("reading t_slope=%X t_b=%X h_slope=%X h_b=%X\n", obj_i2c_data->hts221_cali.t_slope,
		obj_i2c_data->hts221_cali.t_b, obj_i2c_data->hts221_cali.h_slope, obj_i2c_data->hts221_cali.h_b);
	HTS_LOG("reading t_slope=%d t_b=%d h_slope=%d h_b=%d\n", obj_i2c_data->hts221_cali.t_slope,
		obj_i2c_data->hts221_cali.t_b, obj_i2c_data->hts221_cali.h_slope, obj_i2c_data->hts221_cali.h_b);
#endif

	return 0;
}

static int hts221_device_power_off(struct i2c_client *client)
{
	int res = 0;
	u8 buf[2];

	HTS_FUN();
	res = hts221_i2c_read_block(client, REG_CNTRL1_ADDR, buf, 1);
	if (res < 0)
		goto error;

	buf[0] = (buf[0] & HTS221_POWER_MASK) | DISABLE_SENSOR;

	res = hts221_i2c_write_block(client, REG_CNTRL1_ADDR, buf, 1);
	if (res < 0)
		goto error;
	return res;

 error:
	HTS_ERR("humidity power failed 0x%02x,0x%02x: %d\n", buf[0], buf[1], res);
	return res;
}

static int hts221_device_power_on(struct i2c_client *client)
{
	int res = 0;
	u8 buf[2];

	HTS_FUN();
	res = hts221_i2c_read_block(client, REG_CNTRL1_ADDR, buf, 1);
	if (res < 0)
		goto error;

	buf[0] = (buf[0] & HTS221_POWER_MASK) | ENABLE_SENSOR;

	res = hts221_i2c_write_block(client, REG_CNTRL1_ADDR, buf, 1);
	if (res < 0)
		goto error;
	return res;

 error:
	HTS_ERR("humidity power failed 0x%02x,0x%02x: %d\n", buf[0], buf[1], res);
	return res;
}

static int hts221_set_powermode(struct i2c_client *client, enum HTS_POWERMODE_ENUM power_mode)
{
	int err = 0;

	HTS_FUN();
	if (power_mode == HTS_NORMAL_MODE) {
		err = hts221_device_power_on(client);
		if (err < 0)
			HTS_ERR("HTS221 power on fail\n");
	} else {
		err = hts221_device_power_off(client);
		if (err < 0)
			HTS_ERR("HTS221 power off fail\n");
	}
	return 0;
}

/*****************************************************
* Linear interpolation: (x0,y0) (x1,y1) y = ax+b
*
* a = (y1-y0)/(x1-x0)
* b = (x1*y0-x0*y1)/(x1-x0)
*
* result = ((y1-y0)*x+((x1*y0)-(x0*y1)))/(x1-x0)
*
* For Humidity
* (x1,y1) = (H1_T0_OUT, H1_RH)
* (x0,y0) = (H0_T0_OUT, H0_RH)
* x       =  H_OUT
* For Temperature
* (x1,y1) = (T1_OUT, T1_DegC)
* (x0,y0) = (T0_OUT, T0_DegC)
* x       =  T_OUT
******************************************************/
static int hts221_convert(int slope, int b_gen, int *x, int type)
{
	int err = 0;
	int X = 0;

	X = *x;

	*x = ((slope * X) + b_gen);

	if (type == 0)
		*x = (*x) >> 4;	/*for Humidity, m RH */

	else
		*x = (*x) >> 6;	/*for Humidity, m RH */

	return err;
}

static int hts221_start_convert(struct i2c_client *client)
{
	int err = 0;
	u8 buff[2];

	HTS_FUN();
	buff[0] = START_NEW_CONVERT;
	err = hts221_i2c_write_block(client, REG_CNTRL2_ADDR, buff, 1);
	if (err < 0)
		return err;

	return err;
}

/*
*get compensated temperature
*unit:1000 degrees centigrade
*/
static int hts221_get_temperature(struct i2c_client *client, char *buf, int bufsize)
{
	int err = 0;
	u8 temperature_data[2];
	int data_t = 0;

	HTS_FUN();
	err = hts221_i2c_read_block(client, REG_T_OUT_L + 0x80, temperature_data, 2);
	if (err < 0)
		return err;
	data_t = ((s32) ((s16) ((temperature_data[1] << 8) | (temperature_data[0]))));

	err = hts221_convert(obj_i2c_data->hts221_cali.t_slope, obj_i2c_data->hts221_cali.t_b, &data_t, 1);
	if (err < 0)
		return err;
	/*HTS_LOG("hts221 get temperature: %d rH\n", data_t);*/
	sprintf(buf, "%08x", data_t);
	return 0;
}

/*
*get compensated humidity
*unit: 1000 %rH
*/
static int hts221_get_humidity(struct i2c_client *client, char *buf, int bufsize)
{
	int err = 0;
	u8 humidity_data[2];
	int data_h = 0;

	HTS_FUN();
	err = hts221_start_convert(client);
	if (err < 0) {
		HTS_ERR("ERROR\n");
		return err;
	}
	err = hts221_i2c_read_block(client, REG_H_OUT_L + 0x80, humidity_data, 2);
	if (err < 0)
		return err;
	data_h = ((s32) ((s16) ((humidity_data[1] << 8) | (humidity_data[0]))));
	/*HTS_LOG("humidity raw data: 0x%x\n", data_h);*/
	err = hts221_convert(obj_i2c_data->hts221_cali.h_slope, obj_i2c_data->hts221_cali.h_b, &data_h, 0);
	if (err < 0)
		return err;

	/*HTS_LOG("hts221 get humidity: %d rH\n", data_h);*/
	sprintf(buf, "%08x", data_h);
	return 0;
}

/* hts221 setting initialization */
static int hts221_init_client(struct i2c_client *client)
{
	int res = 0;

	HTS_FUN();

	res = hts221_check_ID(client);
	if (res < 0) {
		HTS_ERR("Error reading WHO_AM_I: device is available\n");
		return res;
	}

	res = hts221_set_sampling_period(client, HTS221_ODR_ONE_SHOT, HTS221_BDU);
	if (res < 0) {
		HTS_ERR("Error hts221_set_sampling_period\n");
		return res;
	}
	res = hts221_set_powermode(client, HTS_NORMAL_MODE);
	if (res < 0) {
		HTS_ERR("Error hts221_set_powermode\n");
		return res;
	}

	res = hts221_set_resolution(client, HTS221_H_RESOLUTION_32, HTS221_H_RESOLUTION_MASK);
	if (res < 0) {
		HTS_ERR("Error hts221_set_resolution humidity\n");
		return res;
	}

	res = hts221_set_resolution(client, HTS221_T_RESOLUTION_16, HTS221_T_RESOLUTION_MASK);
	if (res < 0) {
		HTS_ERR("Error hts221_set_resolution humidity\n");
		return res;
	}

	res = hts221_set_powermode(client, HTS_SUSPEND_MODE);
	if (res < 0) {
		HTS_ERR("Error hts221_set_powermode\n");
		return res;
	}

	res = hts221_get_calibration_data(client);
	if (res < 0) {
		HTS_ERR("Error hts221_get_calibration_data\n");
		return res;
	}
	return 0;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct hts221_i2c_data *obj = obj_i2c_data;

	if (NULL == obj) {
		HTS_ERR("hts221 i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", obj->sensor_name);
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct hts221_i2c_data *obj = obj_i2c_data;
	char strbuf[HTS221_BUFSIZE] = "";

	if (NULL == obj) {
		HTS_ERR("hts221 i2c data pointer is null\n");
		return 0;
	}

	hts221_get_humidity(obj->client, strbuf, HTS221_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct hts221_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		HTS_ERR("hts221 i2c data pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct hts221_i2c_data *obj = obj_i2c_data;
	int trace = 0;

	if (obj == NULL) {
		HTS_ERR("i2c_data obj is null\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		HTS_ERR("invalid content: '%s', length = %d\n", buf, (int)count);

	return count;
}

static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct hts221_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		HTS_ERR("hts221 i2c data pointer is null\n");
		return 0;
	}

	if (obj->hw)
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n", obj->client->addr, HTS_DRIVER_VERSION);

	return len;
}

static ssize_t show_power_mode_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct hts221_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		HTS_ERR("hts221 i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%s mode\n",
			obj->power_mode == HTS_NORMAL_MODE ? "normal" : "suspend");

	return len;
}

static ssize_t store_power_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct hts221_i2c_data *obj = obj_i2c_data;
	unsigned long power_mode = 0;
	int err = 0;

	if (obj == NULL) {
		HTS_ERR("hts221 i2c data pointer is null\n");
		return 0;
	}

	err = kstrtoul(buf, 10, &power_mode);

	if (err == 0) {
		err = hts221_set_powermode(obj->client, (enum HTS_POWERMODE_ENUM)(!!(power_mode)));
		if (err)
			return err;
		return count;
	}
	return err;
}

static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powermode, S_IWUSR | S_IRUGO, show_power_mode_value, store_power_mode_value);

static struct driver_attribute *hts221_attr_list[] = {
	&driver_attr_chipinfo,	/* chip information */
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_trace,	/* trace log */
	&driver_attr_status,	/* cust setting */
	&driver_attr_powermode,	/* power mode */
};

static int hts221_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(hts221_attr_list) / sizeof(hts221_attr_list[0]));

	HTS_FUN();

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, hts221_attr_list[idx]);
		if (err) {
			HTS_ERR("driver_create_file (%s) = %d\n", hts221_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int hts221_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(hts221_attr_list) / sizeof(hts221_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, hts221_attr_list[idx]);

	return err;
}

static int hts221_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_i2c_data;

	if (file->private_data == NULL) {
		HTS_ERR("null pointer\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int hts221_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long hts221_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct hts221_i2c_data *obj = (struct hts221_i2c_data *)file->private_data;
	struct i2c_client *client = obj->client;
	char strbuf[HTS221_BUFSIZE];
	u32 dat = 0;
	void __user *data;
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		HTS_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case HUMIDITY_IOCTL_INIT:
		hts221_init_client(client);
		err = hts221_set_powermode(client, HTS_NORMAL_MODE);
		if (err) {
			err = -EFAULT;
			break;
		}
		break;

	case HUMIDITY_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		strcpy(strbuf, obj->sensor_name);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case HUMIDITY_GET_HMDY_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}

		hts221_get_humidity(client, strbuf, HTS221_BUFSIZE);
		err = kstrtoint(strbuf, 16, &dat);
		if (err == 0) {
			if (copy_to_user(data, &dat, sizeof(dat))) {
				err = -EFAULT;
				break;
			}
		}
		break;

	case HUMIDITY_GET_TEMP_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		hts221_get_temperature(client, strbuf, HTS221_BUFSIZE);
		err = kstrtoint(strbuf, 16, &dat);
		if (err == 0) {
			if (copy_to_user(data, &dat, sizeof(dat))) {
				err = -EFAULT;
				break;
			}
		}
		break;

	default:
		HTS_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

static const struct file_operations hts221_fops = {
	.owner = THIS_MODULE,
	.open = hts221_open,
	.release = hts221_release,
	.unlocked_ioctl = hts221_unlocked_ioctl,
};

static struct miscdevice hts221_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "humidity",
	.fops = &hts221_fops,
};

static int hts221_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct hts221_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	HTS_FUN();
	mutex_lock(&hts221_op_mutex);
	if (msg.event == PM_EVENT_SUSPEND) {
		if (NULL == obj) {
			HTS_ERR("null pointer\n");
			mutex_unlock(&hts221_op_mutex);
			return -EINVAL;
		}

		atomic_set(&obj->suspend, 1);
		err = hts221_set_powermode(obj->client, HTS_SUSPEND_MODE);
		if (err) {
			HTS_ERR("hts221 set suspend mode failed, err = %d\n", err);
			mutex_unlock(&hts221_op_mutex);
			return err;
		}
		hts221_power(obj->hw, 0);
	}
	mutex_unlock(&hts221_op_mutex);
	return err;
}

static int hts221_resume(struct i2c_client *client)
{
	struct hts221_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	HTS_FUN();
	mutex_lock(&hts221_op_mutex);
	if (NULL == obj) {
		HTS_ERR("null pointer\n");
		mutex_unlock(&hts221_op_mutex);
		return -EINVAL;
	}

	hts221_power(obj->hw, 1);

	err = hts221_init_client(obj->client);
	if (err) {
		HTS_ERR("initialize client fail\n");
		mutex_unlock(&hts221_op_mutex);
		return err;
	}

	atomic_set(&obj->suspend, 0);
	mutex_unlock(&hts221_op_mutex);
	return 0;
}

static int hts221_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, HTS_DEV_NAME);
	return 0;
}

static int hts221_open_report_data(int open)
{
	return 0;
}

static int hts221_enable_nodata(int en)
{
	int res = 0;
	int retry = 0;
	bool power = false;

	HTS_FUN();
	if (1 == en)
		power = true;
	if (0 == en)
		power = false;

	for (retry = 0; retry < 3; retry++) {
		res = hts221_set_powermode(obj_i2c_data->client, (enum HTS_POWERMODE_ENUM)(!!power));
		if (res == 0) {
			/*HTS_LOG("hts221_set_powermode done\n");*/
			break;
		}
		HTS_ERR("hts221_set_powermode fail\n");
	}

	if (res != 0) {
		HTS_ERR("hts221_set_powermode fail!\n");
		return -1;
	}
	/*HTS_LOG("hts221_set_powermode OK!\n");*/
	return 0;
}

static int hts221_set_delay(u64 ns)
{
	HTS_FUN();
	return 0;
}

static int hts221_get_data(int *value, int *status)
{
	char buff[HTS221_BUFSIZE];
	int err = 0;

	HTS_FUN();
	err = hts221_get_humidity(obj_i2c_data->client, buff, HTS221_BUFSIZE);
	if (err) {
		HTS_ERR("get compensated humidity value failed," "err = %d\n", err);
		return -1;
	}
	err = kstrtoint(buff, 16, value);
	if (err == 0)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int hts221_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct hts221_i2c_data *obj;
	struct hmdy_control_path ctl = { 0 };
	struct hmdy_data_path data = { 0 };
	int err = 0;

	HTS_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	obj->hw = hw;
	obj_i2c_data = obj;
	obj->client = client;
	i2c_set_clientdata(client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	obj->power_mode = HTS_UNDEFINED_POWERMODE;

	err = hts221_init_client(client);
	if (err)
		goto exit_init_client_failed;

	err = misc_register(&hts221_device);
	if (err) {
		HTS_ERR("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	err = hts221_create_attr(&(hts221_init_info.platform_diver_addr->driver));
	if (err) {
		HTS_ERR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = hts221_open_report_data;
	ctl.enable_nodata = hts221_enable_nodata;
	ctl.set_delay = hts221_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = hmdy_register_control_path(&ctl);
	if (err) {
		HTS_ERR("register hmdy control path err\n");
		goto exit_hwmsen_attach_pressure_failed;
	}

	data.get_data = hts221_get_data;
	data.vender_div = 1000;
	err = hmdy_register_data_path(&data);
	if (err) {
		HTS_ERR("hmdy_register_data_path failed, err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}
	err = batch_register_support_info(ID_HUMIDITY, obj->hw->is_batch_supported, data.vender_div, 0);
	if (err) {
		HTS_ERR("register hmdy batch support err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}

	hts221_init_flag = 0;
	HTS_LOG("%s: OK\n", __func__);
	return 0;

exit_hwmsen_attach_pressure_failed:
	hts221_delete_attr(&(hts221_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	misc_deregister(&hts221_device);
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	HTS_ERR("err = %d\n", err);
	hts221_init_flag = -1;
	return err;
}

static int hts221_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	HTS_FUN();

	err = hwmsen_detach(ID_PRESSURE);
	if (err)
		HTS_ERR("hwmsen_detach ID_PRESSURE failed, err = %d\n", err);

	err = hts221_delete_attr(&(hts221_init_info.platform_diver_addr->driver));
	if (err)
		HTS_ERR("hts221_delete_attr failed, err = %d\n", err);

	err = misc_deregister(&hts221_device);
	if (err)
		HTS_ERR("misc_deregister failed, err = %d\n", err);

	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id hmdy_of_match[] = {
	{.compatible = "mediatek,humidity"},
	{},
};
#endif

static struct i2c_driver hts221_i2c_driver = {
	.driver = {
		.name = HTS_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = hmdy_of_match,
#endif

	},
	.probe = hts221_i2c_probe,
	.remove = hts221_i2c_remove,
	.detect = hts221_i2c_detect,
	.suspend = hts221_suspend,
	.resume = hts221_resume,
	.id_table = hts221_i2c_id,
};

static int hts221_remove(void)
{
	HTS_FUN();
	i2c_del_driver(&hts221_i2c_driver);
	return 0;
}

static int hts221_local_init(void)
{
	HTS_FUN();

	if (i2c_add_driver(&hts221_i2c_driver)) {
		HTS_ERR("add driver error\n");
		return -1;
	}
	if (-1 == hts221_init_flag)
		return -1;
	return 0;
}

static int __init hts221_init(void)
{
	const char *name = "mediatek,hts221";

	hw =   get_hmdy_dts_func(name, hw);
	if (!hw)
		HMDY_ERR("get dts info fail\n");
	hmdy_driver_add(&hts221_init_info);

	return 0;
}

static void __exit hts221_exit(void)
{
	HTS_FUN();
}

module_init(hts221_init);
module_exit(hts221_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("HTS221 I2C Driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_VERSION(HTS_DRIVER_VERSION);

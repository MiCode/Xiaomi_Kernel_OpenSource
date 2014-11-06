/*
 * MPU6050 6-axis gyroscope + accelerometer driver
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#include "mpu6050.h"

#define DEBUG_NODE

/*VDD 2.375V-3.46V VLOGIC 1.8V +-5%*/
#define MPU6050_VDD_MIN_UV	2500000
#define MPU6050_VDD_MAX_UV	3400000
#define MPU6050_VLOGIC_MIN_UV	1800000
#define MPU6050_VLOGIC_MAX_UV	1800000
#define MPU6050_VI2C_MIN_UV	1750000
#define MPU6050_VI2C_MAX_UV	1950000

#define MPU6050_ACCEL_MIN_VALUE	-32768
#define MPU6050_ACCEL_MAX_VALUE	32767
#define MPU6050_GYRO_MIN_VALUE	-32768
#define MPU6050_GYRO_MAX_VALUE	32767

/* Limit mininum delay to 10ms as we do not need higher rate so far */
#define MPU6050_ACCEL_MIN_POLL_INTERVAL_MS	10
#define MPU6050_ACCEL_MAX_POLL_INTERVAL_MS	5000
#define MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS	200

#define MPU6050_GYRO_MIN_POLL_INTERVAL_MS	10
#define MPU6050_GYRO_MAX_POLL_INTERVAL_MS	5000
#define MPU6050_GYRO_DEFAULT_POLL_INTERVAL_MS	200

#define MPU6050_RAW_ACCEL_DATA_LEN	6
#define MPU6050_RAW_GYRO_DATA_LEN	6

#define MPU6050_RESET_SLEEP_US	10

#define MPU6050_DEV_NAME_ACCEL	"MPU6050-accel"
#define MPU6050_DEV_NAME_GYRO	"gyroscope"

#define MPU6050_PINCTRL_DEFAULT	"mpu_default"
#define MPU6050_PINCTRL_SUSPEND	"mpu_sleep"

enum mpu6050_place {
	MPU6050_PLACE_PU = 0,
	MPU6050_PLACE_PR = 1,
	MPU6050_PLACE_LD = 2,
	MPU6050_PLACE_LL = 3,
	MPU6050_PLACE_PU_BACK = 4,
	MPU6050_PLACE_PR_BACK = 5,
	MPU6050_PLACE_LD_BACK = 6,
	MPU6050_PLACE_LL_BACK = 7,
	MPU6050_PLACE_UNKNOWN = 8,
	MPU6050_AXIS_REMAP_TAB_SZ = 8
};

struct mpu6050_place_name {
	char name[32];
	enum mpu6050_place place;
};

struct axis_data {
	s16 x;
	s16 y;
	s16 z;
	s16 rx;
	s16 ry;
	s16 rz;
};

/**
 *  struct mpu6050_sensor - Cached chip configuration data.
 *  @client:		I2C client.
 *  @dev:		device structure.
 *  @accel_dev:		accelerometer input device structure.
 *  @gyro_dev:		gyroscope input device structure.
 *  @accel_cdev:		sensor class device structure for accelerometer.
 *  @gyro_cdev:		sensor class device structure for gyroscope.
 *  @pdata:	device platform dependent data.
 *  @accel_poll_work:	accelerometer delay work structure
 *  @gyro_poll_work:	gyroscope delay work structure.
 *  @vlogic:	regulator data for Vlogic and I2C bus pullup.
 *  @vdd:		regulator data for Vdd.
 *  @reg:		notable slave registers.
 *  @cfg:		cached chip configuration data.
 *  @axis:	axis data reading.
 *  @gyro_poll_ms:		gyroscope polling delay.
 *  @accel_poll_ms:	accelerometer polling delay.
 *  @enable_gpio:	enable GPIO.
 *  @use_poll:		use interrupt mode instead of polling data.
 */
struct mpu6050_sensor {
	struct i2c_client *client;
	struct device *dev;
	struct input_dev *accel_dev;
	struct input_dev *gyro_dev;
	struct sensors_classdev accel_cdev;
	struct sensors_classdev gyro_cdev;
	struct mpu6050_platform_data *pdata;
	struct mutex op_lock;
	enum inv_devices chip_type;
	struct delayed_work accel_poll_work;
	struct delayed_work gyro_poll_work;
	struct mpu_reg_map reg;
	struct mpu_chip_config cfg;
	struct axis_data axis;
	u32 gyro_poll_ms;
	u32 accel_poll_ms;
	bool use_poll;
	bool wakeup_en;

	/* power control */
	struct regulator *vlogic;
	struct regulator *vdd;
	struct regulator *vi2c;
	int enable_gpio;
	bool power_enabled;

	/* pinctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;
};

/* Accelerometer information read by HAL */
static struct sensors_classdev mpu6050_acc_cdev = {
	.name = "MPU6050-accel",
	.vendor = "Invensense",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "156.8",	/* m/s^2 */
	.resolution = "0.000598144",	/* m/s^2 */
	.sensor_power = "0.5",	/* 0.5 mA */
	.min_delay = MPU6050_ACCEL_MIN_POLL_INTERVAL_MS * 1000,
	.delay_msec = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

/* gyroscope information read by HAL */
static struct sensors_classdev mpu6050_gyro_cdev = {
	.name = "MPU6050-gyro",
	.vendor = "Invensense",
	.version = 1,
	.handle = SENSORS_GYROSCOPE_HANDLE,
	.type = SENSOR_TYPE_GYROSCOPE,
	.max_range = "34.906586",	/* rad/s */
	.resolution = "0.0010681152",	/* rad/s */
	.sensor_power = "3.6",	/* 3.6 mA */
	.min_delay = MPU6050_GYRO_MIN_POLL_INTERVAL_MS * 1000,
	.delay_msec = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

struct sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};

static const struct sensor_axis_remap
mpu6050_accel_axis_remap_tab[MPU6050_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,     1,      1,      1 }, /* P0 */
	{  1,    0,    2,     1,     -1,      1 }, /* P1 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P2 */
	{  1,    0,    2,    -1,      1,      1 }, /* P3 */

	{  0,    1,    2,    -1,      1,     -1 }, /* P4 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P5 */
	{  0,    1,    2,     1,     -1,     -1 }, /* P6 */
	{  1,    0,    2,     1,      1,     -1 }, /* P7 */
};

static const struct sensor_axis_remap
mpu6050_gyro_axis_remap_tab[MPU6050_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,    -1,      1,     -1 }, /* P0 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P1*/
	{  0,    1,    2,     1,     -1,     -1 }, /* P2 */
	{  1,    0,    2,     1,      1,     -1 }, /* P3 */

	{  0,    1,    2,     1,      1,      1 }, /* P4 */
	{  1,    0,    2,     1,     -1,      1 }, /* P5 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P6 */
	{  1,    0,    2,    -1,      1,      1 }, /* P7 */
};

static const struct mpu6050_place_name
mpu6050_place_name2num[MPU6050_AXIS_REMAP_TAB_SZ] = {
	{"Portrait Up", MPU6050_PLACE_PU},
	{"Landscape Right", MPU6050_PLACE_PR},
	{"Portrait Down", MPU6050_PLACE_LD},
	{"Landscape Left", MPU6050_PLACE_LL},
	{"Portrait Up Back Side", MPU6050_PLACE_PU_BACK},
	{"Landscape Right Back Side", MPU6050_PLACE_PR_BACK},
	{"Portrait Down Back Side", MPU6050_PLACE_LD_BACK},
	{"Landscape Left Back Side", MPU6050_PLACE_LL_BACK},
};

/* Map gyro measurement range setting to number of bit to shift */
static const u8 mpu_gyro_fs_shift[NUM_FSR] = {
	GYRO_SCALE_SHIFT_FS0, /* MPU_FSR_250DPS */
	GYRO_SCALE_SHIFT_FS1, /* MPU_FSR_500DPS */
	GYRO_SCALE_SHIFT_FS2, /* MPU_FSR_1000DPS */
	GYRO_SCALE_SHIFT_FS3, /* MPU_FSR_2000DPS */
};

/* Map accel measurement range setting to number of bit to shift */
static const u8 mpu_accel_fs_shift[NUM_ACCL_FSR] = {
	ACCEL_SCALE_SHIFT_02G, /* ACCEL_FS_02G */
	ACCEL_SCALE_SHIFT_04G, /* ACCEL_FS_04G */
	ACCEL_SCALE_SHIFT_08G, /* ACCEL_FS_08G */
	ACCEL_SCALE_SHIFT_16G, /* ACCEL_FS_16G */
};

/* Function declarations */
static void mpu6050_pinctrl_state(struct mpu6050_sensor *sensor,
			bool active);
static int mpu6050_set_interrupt(struct mpu6050_sensor *sensor,
		const u8 mask, bool on);


static int mpu6050_power_ctl(struct mpu6050_sensor *sensor, bool on)
{
	int rc = 0;

	if (on && (!sensor->power_enabled)) {
		rc = regulator_enable(sensor->vdd);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(sensor->vlogic);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vlogic enable failed rc=%d\n", rc);
			regulator_disable(sensor->vdd);
			return rc;
		}

		if (!IS_ERR_OR_NULL(sensor->vi2c)) {
			rc = regulator_enable(sensor->vi2c);
			if (rc) {
				dev_err(&sensor->client->dev,
					"Regulator vi2c enable failed rc=%d\n",
					rc);
				regulator_disable(sensor->vlogic);
				regulator_disable(sensor->vdd);
				return rc;
			}
		}

		if (gpio_is_valid(sensor->enable_gpio)) {
			udelay(POWER_EN_DELAY_US);
			gpio_set_value(sensor->enable_gpio, 1);
		}
		msleep(POWER_UP_TIME_MS);

		mpu6050_pinctrl_state(sensor, true);

		sensor->power_enabled = true;
	} else if (!on && (sensor->power_enabled)) {
		mpu6050_pinctrl_state(sensor, false);

		if (gpio_is_valid(sensor->enable_gpio)) {
			udelay(POWER_EN_DELAY_US);
			gpio_set_value(sensor->enable_gpio, 0);
			udelay(POWER_EN_DELAY_US);
		}

		rc = regulator_disable(sensor->vdd);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(sensor->vlogic);
		if (rc) {
			dev_err(&sensor->client->dev,
				"Regulator vlogic disable failed rc=%d\n", rc);
			rc = regulator_enable(sensor->vdd);
			return rc;
		}

		if (!IS_ERR_OR_NULL(sensor->vi2c)) {
			rc = regulator_disable(sensor->vi2c);
			if (rc) {
				dev_err(&sensor->client->dev,
					"Regulator vi2c disable failed rc=%d\n",
					rc);
				if (regulator_enable(sensor->vi2c) ||
						regulator_enable(sensor->vdd))
					return -EIO;
			}
		}

		sensor->power_enabled = false;
	} else {
		dev_warn(&sensor->client->dev,
				"Ignore power status change from %d to %d\n",
				on, sensor->power_enabled);
	}
	return rc;
}

static int mpu6050_power_init(struct mpu6050_sensor *sensor)
{
	int ret = 0;

	sensor->vdd = regulator_get(&sensor->client->dev, "vdd");
	if (IS_ERR(sensor->vdd)) {
		ret = PTR_ERR(sensor->vdd);
		dev_err(&sensor->client->dev,
			"Regulator get failed vdd ret=%d\n", ret);
		return ret;
	}

	if (regulator_count_voltages(sensor->vdd) > 0) {
		ret = regulator_set_voltage(sensor->vdd, MPU6050_VDD_MIN_UV,
					   MPU6050_VDD_MAX_UV);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Regulator set_vtg failed vdd ret=%d\n", ret);
			goto reg_vdd_put;
		}
	}

	sensor->vlogic = regulator_get(&sensor->client->dev, "vlogic");
	if (IS_ERR(sensor->vlogic)) {
		ret = PTR_ERR(sensor->vlogic);
		dev_err(&sensor->client->dev,
			"Regulator get failed vlogic ret=%d\n", ret);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(sensor->vlogic) > 0) {
		ret = regulator_set_voltage(sensor->vlogic,
				MPU6050_VLOGIC_MIN_UV,
				MPU6050_VLOGIC_MAX_UV);
		if (ret) {
			dev_err(&sensor->client->dev,
			"Regulator set_vtg failed vlogic ret=%d\n", ret);
			goto reg_vlogic_put;
		}
	}

	sensor->vi2c = regulator_get(&sensor->client->dev, "vi2c");
	if (IS_ERR(sensor->vi2c)) {
		ret = PTR_ERR(sensor->vi2c);
		dev_info(&sensor->client->dev,
			"Regulator get failed vi2c ret=%d\n", ret);
		sensor->vi2c = NULL;
	} else if (regulator_count_voltages(sensor->vi2c) > 0) {
		ret = regulator_set_voltage(sensor->vi2c,
				MPU6050_VI2C_MIN_UV,
				MPU6050_VI2C_MAX_UV);
		if (ret) {
			dev_err(&sensor->client->dev,
			"Regulator set_vtg failed vi2c ret=%d\n", ret);
			goto reg_vi2c_put;
		}
	}

	return 0;

reg_vi2c_put:
	regulator_put(sensor->vi2c);
	if (regulator_count_voltages(sensor->vlogic) > 0)
		regulator_set_voltage(sensor->vlogic, 0, MPU6050_VLOGIC_MAX_UV);
reg_vlogic_put:
	regulator_put(sensor->vlogic);
reg_vdd_set_vtg:
	if (regulator_count_voltages(sensor->vdd) > 0)
		regulator_set_voltage(sensor->vdd, 0, MPU6050_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(sensor->vdd);
	return ret;
}

static int mpu6050_power_deinit(struct mpu6050_sensor *sensor)
{
	int ret = 0;

	if (regulator_count_voltages(sensor->vlogic) > 0)
		regulator_set_voltage(sensor->vlogic, 0, MPU6050_VLOGIC_MAX_UV);
	regulator_put(sensor->vlogic);
	if (regulator_count_voltages(sensor->vdd) > 0)
		regulator_set_voltage(sensor->vdd, 0, MPU6050_VDD_MAX_UV);
	regulator_put(sensor->vdd);
	return ret;
}

/**
 * mpu6050_read_reg - read multiple register data
 * @start_addr: register address read from
 * @buffer: provide register addr and get register
 * @length: length of register
 *
 * Reads the register values in one transaction or returns a negative
 * error code on failure.
 */
static int mpu6050_read_reg(struct i2c_client *client, u8 start_addr,
			       u8 *buffer, int length)
{
	/*
	 * Annoying we can't make this const because the i2c layer doesn't
	 * declare input buffers const.
	 */
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &start_addr,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	return i2c_transfer(client->adapter, msg, 2);
}

/**
 * mpu6050_read_accel_data - get accelerometer data from device
 * @sensor: sensor device instance
 * @data: axis data to update
 *
 * Return the converted X Y and Z data from the sensor device
 */
static void mpu6050_read_accel_data(struct mpu6050_sensor *sensor,
			     struct axis_data *data)
{
	u16 buffer[3];

	mpu6050_read_reg(sensor->client, sensor->reg.raw_accel,
		(u8 *)buffer, MPU6050_RAW_ACCEL_DATA_LEN);
	data->x = be16_to_cpu(buffer[0]);
	data->y = be16_to_cpu(buffer[1]);
	data->z = be16_to_cpu(buffer[2]);
}

/**
 * mpu6050_read_gyro_data - get gyro data from device
 * @sensor: sensor device instance
 * @data: axis data to update
 *
 * Return the converted RX RY and RZ data from the sensor device
 */
static void mpu6050_read_gyro_data(struct mpu6050_sensor *sensor,
			     struct axis_data *data)
{
	u16 buffer[3];

	mpu6050_read_reg(sensor->client, sensor->reg.raw_gyro,
		(u8 *)buffer, MPU6050_RAW_GYRO_DATA_LEN);
	data->rx = be16_to_cpu(buffer[0]);
	data->ry = be16_to_cpu(buffer[1]);
	data->rz = be16_to_cpu(buffer[2]);
}

/**
 * mpu6050_remap_accel_data - remap accelerometer raw data to axis data
 * @data: data needs remap
 * @place: sensor position
 */
static void mpu6050_remap_accel_data(struct axis_data *data, int place)
{
	const struct sensor_axis_remap *remap;
	s16 tmp[3];
	/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MPU6050_AXIS_REMAP_TAB_SZ))
		return;

	remap = &mpu6050_accel_axis_remap_tab[place];

	tmp[0] = data->x;
	tmp[1] = data->y;
	tmp[2] = data->z;
	data->x = tmp[remap->src_x] * remap->sign_x;
	data->y = tmp[remap->src_y] * remap->sign_y;
	data->z = tmp[remap->src_z] * remap->sign_z;

	return;
}

/**
 * mpu6050_remap_gyro_data - remap gyroscope raw data to axis data
 * @data: data needs remap
 * @place: sensor position
 */
static void mpu6050_remap_gyro_data(struct axis_data *data, int place)
{
	const struct sensor_axis_remap *remap;
	s16 tmp[3];
	/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MPU6050_AXIS_REMAP_TAB_SZ))
		return;

	remap = &mpu6050_gyro_axis_remap_tab[place];
	tmp[0] = data->rx;
	tmp[1] = data->ry;
	tmp[2] = data->rz;
	data->rx = tmp[remap->src_x] * remap->sign_x;
	data->ry = tmp[remap->src_y] * remap->sign_y;
	data->rz = tmp[remap->src_z] * remap->sign_z;

	return;
}

/**
 * mpu6050_interrupt_thread - handle an IRQ
 * @irq: interrupt numner
 * @data: the sensor
 *
 * Called by the kernel single threaded after an interrupt occurs. Read
 * the sensor data and generate an input event for it.
 */
static irqreturn_t mpu6050_interrupt_thread(int irq, void *data)
{
	struct mpu6050_sensor *sensor = data;
	u32 shift;

	if (sensor->cfg.accel_enable) {
		mpu6050_read_accel_data(sensor, &sensor->axis);
		mpu6050_remap_accel_data(&sensor->axis, sensor->pdata->place);
		shift = mpu_accel_fs_shift[sensor->cfg.accel_fs];
		input_report_abs(sensor->accel_dev, ABS_X,
			(sensor->axis.x >> shift));
		input_report_abs(sensor->accel_dev, ABS_Y,
			(sensor->axis.y >> shift));
		input_report_abs(sensor->accel_dev, ABS_Z,
			(sensor->axis.z >> shift));
		input_sync(sensor->accel_dev);
	}

	if (sensor->cfg.gyro_enable) {
		mpu6050_read_gyro_data(sensor, &sensor->axis);
		mpu6050_remap_gyro_data(&sensor->axis, sensor->pdata->place);

		shift = mpu_gyro_fs_shift[sensor->cfg.fsr];
		input_report_abs(sensor->gyro_dev, ABS_RX,
			(sensor->axis.rx >> shift));
		input_report_abs(sensor->gyro_dev, ABS_RY,
			(sensor->axis.ry >> shift));
		input_report_abs(sensor->gyro_dev, ABS_RZ,
			(sensor->axis.rz >> shift));
		input_sync(sensor->gyro_dev);
	}

	return IRQ_HANDLED;
}

/**
 * mpu6050_accel_work_fn - polling accelerometer data
 * @work: the work struct
 *
 * Called by the work queue; read sensor data and generate an input
 * event
 */
static void mpu6050_accel_work_fn(struct work_struct *work)
{
	struct mpu6050_sensor *sensor;
	u32 shift;
	ktime_t timestamp;

	sensor = container_of((struct delayed_work *)work,
				struct mpu6050_sensor, accel_poll_work);

	timestamp = ktime_get();
	mpu6050_read_accel_data(sensor, &sensor->axis);
	mpu6050_remap_accel_data(&sensor->axis, sensor->pdata->place);

	shift = mpu_accel_fs_shift[sensor->cfg.accel_fs];
	input_report_abs(sensor->accel_dev, ABS_X,
		(sensor->axis.x >> shift));
	input_report_abs(sensor->accel_dev, ABS_Y,
		(sensor->axis.y >> shift));
	input_report_abs(sensor->accel_dev, ABS_Z,
		(sensor->axis.z >> shift));
	input_event(sensor->accel_dev,
			EV_SYN, SYN_TIME_SEC,
			ktime_to_timespec(timestamp).tv_sec);
	input_event(sensor->accel_dev, EV_SYN,
		SYN_TIME_NSEC,
		ktime_to_timespec(timestamp).tv_nsec);
	input_sync(sensor->accel_dev);

	if (sensor->use_poll)
		schedule_delayed_work(&sensor->accel_poll_work,
			msecs_to_jiffies(sensor->accel_poll_ms));
}

/**
 * mpu6050_gyro_work_fn - polling gyro data
 * @work: the work struct
 *
 * Called by the work queue; read sensor data and generate an input
 * event
 */
static void mpu6050_gyro_work_fn(struct work_struct *work)
{
	struct mpu6050_sensor *sensor;
	u32 shift;
	ktime_t timestamp;

	sensor = container_of((struct delayed_work *)work,
				struct mpu6050_sensor, gyro_poll_work);

	timestamp = ktime_get();
	mpu6050_read_gyro_data(sensor, &sensor->axis);
	mpu6050_remap_gyro_data(&sensor->axis, sensor->pdata->place);

	shift = mpu_gyro_fs_shift[sensor->cfg.fsr];
	input_report_abs(sensor->gyro_dev, ABS_RX,
		(sensor->axis.rx >> shift));
	input_report_abs(sensor->gyro_dev, ABS_RY,
		(sensor->axis.ry >> shift));
	input_report_abs(sensor->gyro_dev, ABS_RZ,
		(sensor->axis.rz >> shift));
	input_event(sensor->gyro_dev,
			EV_SYN, SYN_TIME_SEC,
			ktime_to_timespec(timestamp).tv_sec);
	input_event(sensor->gyro_dev, EV_SYN,
		SYN_TIME_NSEC,
		ktime_to_timespec(timestamp).tv_nsec);
	input_sync(sensor->gyro_dev);

	if (sensor->use_poll)
		schedule_delayed_work(&sensor->gyro_poll_work,
			msecs_to_jiffies(sensor->gyro_poll_ms));
}

/**
 *  mpu6050_set_lpa_freq() - set low power wakeup frequency.
 */
static int mpu6050_set_lpa_freq(struct mpu6050_sensor *sensor, int lpa_freq)
{
	int ret;
	u8 data;

	/* only for MPU6050 with fixed rate, need expend */
	if (INV_MPU6050 == sensor->chip_type) {
		ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_2);
		if (ret < 0)
			return ret;

		data = (u8)ret;
		data &= ~BIT_LPA_FREQ_MASK;
		data |= MPU6050_LPA_5HZ;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_2, data);
		if (ret < 0)
			return ret;
	}
	sensor->cfg.lpa_freq = lpa_freq;

	return 0;
}

static int mpu6050_switch_engine(struct mpu6050_sensor *sensor,
				bool en, u32 mask)
{
	struct mpu_reg_map *reg;
	u8 data, mgmt_1;
	int ret;

	reg = &sensor->reg;
	/*
	 * switch clock needs to be careful. Only when gyro is on, can
	 * clock source be switched to gyro. Otherwise, it must be set to
	 * internal clock
	 */
	mgmt_1 = MPU_CLK_INTERNAL;
	if (BIT_PWR_GYRO_STBY_MASK == mask) {
		ret = i2c_smbus_read_byte_data(sensor->client,
			reg->pwr_mgmt_1);
		if (ret < 0)
			goto error;
		mgmt_1 = (u8)ret;
		mgmt_1 &= ~BIT_CLK_MASK;
	}

	if ((BIT_PWR_GYRO_STBY_MASK == mask) && (!en)) {
		/*
		 * turning off gyro requires switch to internal clock first.
		 * Then turn off gyro engine
		 */
		mgmt_1 |= MPU_CLK_INTERNAL;
		ret = i2c_smbus_write_byte_data(sensor->client,
			reg->pwr_mgmt_1, mgmt_1);
		if (ret < 0)
			goto error;
	}

	ret = i2c_smbus_read_byte_data(sensor->client,
			reg->pwr_mgmt_2);
	if (ret < 0)
		goto error;
	data = (u8)ret;
	if (en)
		data &= (~mask);
	else
		data |= mask;
	ret = i2c_smbus_write_byte_data(sensor->client,
			reg->pwr_mgmt_2, data);
	if (ret < 0)
		goto error;

	if ((BIT_PWR_GYRO_STBY_MASK == mask) && en) {
		/* wait gyro stable */
		msleep(SENSOR_UP_TIME_MS);
		/* after gyro is on & stable, switch internal clock to PLL */
		mgmt_1 |= MPU_CLK_PLL_X;
		ret = i2c_smbus_write_byte_data(sensor->client,
				reg->pwr_mgmt_1, mgmt_1);
		if (ret < 0)
			goto error;
	}

	return 0;

error:
	dev_err(&sensor->client->dev, "Fail to switch MPU engine\n");
	return ret;
}

static int mpu6050_init_engine(struct mpu6050_sensor *sensor)
{
	int ret;

	ret = mpu6050_switch_engine(sensor, false, BIT_PWR_GYRO_STBY_MASK);
	if (ret)
		return ret;

	ret = mpu6050_switch_engine(sensor, false, BIT_PWR_ACCEL_STBY_MASK);
	if (ret)
		return ret;

	return 0;
}

/**
 * mpu6050_set_power_mode - set the power mode
 * @sensor: sensor data structure
 * @power_on: value to switch on/off of power, 1: normal power,
 *    0: low power
 *
 * Put device to normal-power mode or low-power mode.
 */
static int mpu6050_set_power_mode(struct mpu6050_sensor *sensor,
					bool power_on)
{
	struct i2c_client *client = sensor->client;
	s32 ret;
	u8 val;

	ret = i2c_smbus_read_byte_data(client, sensor->reg.pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&client->dev,
				"Fail to read power mode, ret=%d\n", ret);
		return ret;
	}

	if (power_on)
		val = (u8)ret & ~BIT_SLEEP;
	else
		val = (u8)ret | BIT_SLEEP;
	ret = i2c_smbus_write_byte_data(client, sensor->reg.pwr_mgmt_1, val);
	if (ret < 0) {
		dev_err(&client->dev,
				"Fail to write power mode, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int mpu6050_gyro_enable(struct mpu6050_sensor *sensor, bool on)
{
	int ret;
	u8 data;

	if (sensor->cfg.is_asleep) {
		dev_err(&sensor->client->dev,
			"Fail to set gyro state, device is asleep.\n");
		return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to get sensor power state, ret=%d\n", ret);
		return ret;
	}

	data = (u8)ret;
	if (on) {
		ret = mpu6050_switch_engine(sensor, true,
			BIT_PWR_GYRO_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.gyro_enable = 1;

		data &= ~BIT_SLEEP;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1, data);
		if (ret < 0) {
			dev_err(&sensor->client->dev,
				"Fail to set sensor power state, ret=%d\n",
				ret);
			return ret;
		}

		if (!sensor->cfg.int_enabled) {
			ret = mpu6050_set_interrupt(sensor,
				BIT_DATA_RDY_EN, true);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to enable interrupt mode for gyro, ret=%d\n",
					ret);
				return ret;
			}
			enable_irq(sensor->client->irq);
			sensor->cfg.int_enabled = true;
		}

		sensor->cfg.enable = 1;
	} else {
		if (sensor->cfg.int_enabled && !sensor->cfg.accel_enable) {
			ret = mpu6050_set_interrupt(sensor,
				BIT_DATA_RDY_EN, false);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to disable interrupt mode for gyro, ret=%d\n",
					ret);
				return ret;
			}
			disable_irq(sensor->client->irq);
			sensor->cfg.int_enabled = false;
		}

		ret = mpu6050_switch_engine(sensor, false,
			BIT_PWR_GYRO_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.gyro_enable = 0;
		if (!sensor->cfg.accel_enable) {
			data |=  BIT_SLEEP;
			ret = i2c_smbus_write_byte_data(sensor->client,
					sensor->reg.pwr_mgmt_1, data);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to set sensor power state, ret=%d\n",
					ret);
				return ret;
			}
			sensor->cfg.enable = 0;
		}
	}
	return 0;
}

/**
 * mpu6050_restore_context - update the sensor register context
 */

static int mpu6050_restore_context(struct mpu6050_sensor *sensor)
{
	struct mpu_reg_map *reg;
	struct i2c_client *client;
	int ret;
	u8 data, pwr_ctrl;

	client = sensor->client;
	reg = &sensor->reg;

	/* Save power state and wakeup device from sleep */
	ret = i2c_smbus_read_byte_data(client, reg->pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&client->dev, "read power ctrl failed.\n");
		goto exit;
	}
	pwr_ctrl = (u8)ret;

	ret = i2c_smbus_write_byte_data(client, reg->pwr_mgmt_1,
		BIT_WAKEUP_AFTER_RESET);
	if (ret < 0) {
		dev_err(&client->dev, "wakeup sensor failed.\n");
		goto exit;
	}
	ret = i2c_smbus_write_byte_data(client, reg->gyro_config,
			sensor->cfg.fsr << GYRO_CONFIG_FSR_SHIFT);
	if (ret < 0) {
		dev_err(&client->dev, "update fsr failed.\n");
		goto exit;
	}

	ret = i2c_smbus_write_byte_data(client, reg->lpf, sensor->cfg.lpf);
	if (ret < 0) {
		dev_err(&client->dev, "update lpf failed.\n");
		goto exit;
	}

	ret = i2c_smbus_write_byte_data(client, reg->accel_config,
			(sensor->cfg.accel_fs << ACCL_CONFIG_FSR_SHIFT));
	if (ret < 0) {
		dev_err(&client->dev, "update accel_fs failed.\n");
		goto exit;
	}

	ret = i2c_smbus_read_byte_data(client, reg->fifo_en);
	if (ret < 0) {
		dev_err(&client->dev, "read fifo_en failed.\n");
		goto exit;
	}

	data = (u8)ret;

	if (sensor->cfg.accel_fifo_enable) {
		ret = i2c_smbus_write_byte_data(client, reg->fifo_en,
				data |= BIT_ACCEL_FIFO);
		if (ret < 0) {
			dev_err(&client->dev, "write accel_fifo_enabled failed.\n");
			goto exit;
		}
	}

	if (sensor->cfg.gyro_fifo_enable) {
		ret = i2c_smbus_write_byte_data(client, reg->fifo_en,
				data |= BIT_GYRO_FIFO);
		if (ret < 0) {
			dev_err(&client->dev, "write gyro_fifo_enabled failed.\n");
			goto exit;
		}
	}

	/* Accel and Gyro should set to standby by default */
	ret = i2c_smbus_write_byte_data(client, reg->pwr_mgmt_2,
			BITS_PWR_ALL_AXIS_STBY);
	if (ret < 0) {
		dev_err(&client->dev, "set pwr_mgmt_2 failed.\n");
		goto exit;
	}

	ret = mpu6050_set_lpa_freq(sensor, sensor->cfg.lpa_freq);
	if (ret < 0) {
		dev_err(&client->dev, "set lpa_freq failed.\n");
		goto exit;
	}

	ret = i2c_smbus_write_byte_data(client, reg->sample_rate_div,
			sensor->cfg.rate_div);
	if (ret < 0) {
		dev_err(&client->dev, "set sample_rate_div failed.\n");
		goto exit;
	}

	ret = i2c_smbus_write_byte_data(client, reg->int_pin_cfg,
			sensor->cfg.int_pin_cfg);
	if (ret < 0) {
		dev_err(&client->dev, "set int_pin_cfg failed.\n");
		goto exit;
	}

	ret = i2c_smbus_write_byte_data(client, reg->pwr_mgmt_1,
		pwr_ctrl);
	if (ret < 0) {
		dev_err(&client->dev, "write saved power state failed.\n");
		goto exit;
	}

	dev_dbg(&client->dev, "restore context finished\n");

exit:
	return ret;
}

/**
 * mpu6050_reset_chip - reset chip to default state
 */
static void mpu6050_reset_chip(struct mpu6050_sensor *sensor)
{
	struct i2c_client *client;
	int ret, i;

	client = sensor->client;

	ret = i2c_smbus_write_byte_data(client, sensor->reg.pwr_mgmt_1,
			BIT_RESET_ALL);
	if (ret < 0) {
		dev_err(&client->dev, "Reset chip fail!\n");
		goto exit;
	}
	for (i = 0; i < MPU6050_RESET_RETRY_CNT; i++) {
		ret = i2c_smbus_read_byte_data(sensor->client,
					sensor->reg.pwr_mgmt_1);
		if (ret < 0) {
			dev_err(&sensor->client->dev,
				"Fail to get reset state ret=%d\n", ret);
			goto exit;
		}

		if ((ret & BIT_H_RESET) == 0) {
			dev_dbg(&sensor->client->dev,
				"Chip reset success! i=%d\n", i);
			break;
		}

		usleep(MPU6050_RESET_SLEEP_US);
	}

exit:
	return;
}

static int mpu6050_gyro_set_enable(struct mpu6050_sensor *sensor, bool enable)
{
	int ret = 0;

	mutex_lock(&sensor->op_lock);
	if (enable) {
		if (!sensor->power_enabled) {
			ret = mpu6050_power_ctl(sensor, true);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
						"Failed to power up mpu6050\n");
				goto exit;
			}
			ret = mpu6050_restore_context(sensor);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
						"Failed to restore context\n");
				goto exit;
			}
		}

		ret = mpu6050_gyro_enable(sensor, true);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to enable gyro engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}

		if (sensor->use_poll)
			schedule_delayed_work(&sensor->gyro_poll_work,
				msecs_to_jiffies(sensor->gyro_poll_ms));
	} else {
		ret = mpu6050_gyro_enable(sensor, false);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to disable gyro engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}
		if (sensor->use_poll)
			cancel_delayed_work_sync(&sensor->gyro_poll_work);

	}

exit:
	mutex_unlock(&sensor->op_lock);
	return ret;
}

/*
  * Set interrupt enabling bits to enable/disable specific type of interrupt.
  */
static int mpu6050_set_interrupt(struct mpu6050_sensor *sensor,
		const u8 mask, bool on)
{
	int ret;
	u8 data;

	if (sensor->cfg.is_asleep)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.int_enable);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail read interrupt mode. ret=%d\n", ret);
		return ret;
	}

	if (on) {
		data = (u8)ret;
		data |= mask;
	} else {
		data = (u8)ret;
		data &= ~mask;
	}

	ret = i2c_smbus_write_byte_data(sensor->client,
			sensor->reg.int_enable, data);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to set interrupt. ret=%d\n", ret);
		return ret;
	}
	return 0;
}

/*
  * Enable/disable motion detection interrupt.
  */
static int mpu6050_set_motion_det(struct mpu6050_sensor *sensor, bool on)
{
	int ret;

	if (on) {
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.mot_thr, DEFAULT_MOT_THR);
		if (ret < 0)
			goto err_exit;

		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.mot_dur, DEFAULT_MOT_DET_DUR);
		if (ret < 0)
			goto err_exit;

	}

	ret = mpu6050_set_interrupt(sensor, BIT_MOT_EN, on);
	if (ret < 0)
		goto err_exit;

	sensor->cfg.mot_det_on = on;
	/* Use default motion detection delay 4ms */

	return 0;

err_exit:
	dev_err(&sensor->client->dev,
			"Fail to set motion detection. ret=%d\n", ret);
	return ret;
}

/* Update sensor sample rate divider upon accel and gyro polling rate. */
static int mpu6050_config_sample_rate(struct mpu6050_sensor *sensor)
{
	int ret;
	u32 delay_ms;
	u8 div;

	if (sensor->cfg.is_asleep)
		return -EINVAL;

	if (sensor->accel_poll_ms <= sensor->gyro_poll_ms)
		delay_ms = sensor->accel_poll_ms;
	else
		delay_ms = sensor->gyro_poll_ms;

	/* Sample_rate = internal_ODR/(1+SMPLRT_DIV) */
	if ((sensor->cfg.lpf != MPU_DLPF_256HZ_NOLPF2) &&
		(sensor->cfg.lpf != MPU_DLPF_RESERVED)) {
		if (delay_ms > DELAY_MS_MAX_DLPF)
			delay_ms = DELAY_MS_MAX_DLPF;
		if (delay_ms < DELAY_MS_MIN_DLPF)
			delay_ms = DELAY_MS_MIN_DLPF;

		div = (u8)(((ODR_DLPF_ENA * delay_ms) / MSEC_PER_SEC) - 1);
	} else {
		if (delay_ms > DELAY_MS_MAX_NODLPF)
			delay_ms = DELAY_MS_MAX_NODLPF;
		if (delay_ms < DELAY_MS_MIN_NODLPF)
			delay_ms = DELAY_MS_MIN_NODLPF;
		div = (u8)(((ODR_DLPF_DIS * delay_ms) / MSEC_PER_SEC) - 1);
	}

	ret = i2c_smbus_write_byte_data(sensor->client,
		sensor->reg.sample_rate_div, div);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
				"Update sample rate divdier fail, ret=%d\n",
				ret);
		return ret;
	}

	sensor->cfg.rate_div = div;

	return 0;
}

static int mpu6050_gyro_set_poll_delay(struct mpu6050_sensor *sensor,
					unsigned long delay)
{
	int ret;

	mutex_lock(&sensor->op_lock);
	if (delay < MPU6050_GYRO_MIN_POLL_INTERVAL_MS)
		delay = MPU6050_GYRO_MIN_POLL_INTERVAL_MS;
	if (delay > MPU6050_GYRO_MAX_POLL_INTERVAL_MS)
		delay = MPU6050_GYRO_MAX_POLL_INTERVAL_MS;

	sensor->gyro_poll_ms = delay;
	if (sensor->use_poll) {
		cancel_delayed_work_sync(&sensor->gyro_poll_work);
		schedule_delayed_work(&sensor->gyro_poll_work,
				msecs_to_jiffies(sensor->gyro_poll_ms));
	} else {
		ret = mpu6050_config_sample_rate(sensor);
		if (ret < 0)
			dev_err(&sensor->client->dev,
				"Unable to set polling delay for gyro!\n");
	}
	mutex_unlock(&sensor->op_lock);
	return 0;
}

static int mpu6050_gyro_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, gyro_cdev);

	return mpu6050_gyro_set_enable(sensor, enable);
}

static int mpu6050_gyro_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, gyro_cdev);

	return mpu6050_gyro_set_poll_delay(sensor, delay_ms);
}

/**
 * mpu6050_gyro_attr_get_polling_delay - get the sampling rate
 */
static ssize_t mpu6050_gyro_attr_get_polling_delay(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int val;
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	val = sensor ? sensor->gyro_poll_ms : 0;
	return snprintf(buf, 8, "%d\n", val);
}

/**
 * mpu6050_gyro_attr_set_polling_delay - set the sampling rate
 */
static ssize_t mpu6050_gyro_attr_set_polling_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long interval_ms;
	int ret;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	ret = mpu6050_gyro_set_poll_delay(sensor, interval_ms);

	return ret ? -EBUSY : size;
}

static ssize_t mpu6050_gyro_attr_get_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sensor->cfg.gyro_enable);
}

/**
 * mpu6050_gyro_attr_set_enable -
 *    Set/get enable function is just needed by sensor HAL.
 */
static ssize_t mpu6050_gyro_attr_set_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long enable;
	int ret;

	if (kstrtoul(buf, 10, &enable))
		return -EINVAL;

	if (enable)
		ret = mpu6050_gyro_set_enable(sensor, true);
	else
		ret = mpu6050_gyro_set_enable(sensor, false);

	return ret ? -EBUSY : count;
}

static struct device_attribute gyro_attr[] = {
	__ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		mpu6050_gyro_attr_get_polling_delay,
		mpu6050_gyro_attr_set_polling_delay),
	__ATTR(enable, S_IRUGO | S_IWUSR,
		mpu6050_gyro_attr_get_enable,
		mpu6050_gyro_attr_set_enable),
};

static int create_gyro_sysfs_interfaces(struct device *dev)
{
	int i;
	int err;
	for (i = 0; i < ARRAY_SIZE(gyro_attr); i++) {
		err = device_create_file(dev, gyro_attr + i);
		if (err)
			goto error;
	}
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, gyro_attr + i);
	dev_err(dev, "Unable to create interface\n");
	return err;
}

static int remove_gyro_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(gyro_attr); i++)
		device_remove_file(dev, gyro_attr + i);
	return 0;
}

static int mpu6050_accel_enable(struct mpu6050_sensor *sensor, bool on)
{
	int ret;
	u8 data;

	if (sensor->cfg.is_asleep)
		return -EINVAL;

	ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to get sensor power state, ret=%d\n", ret);
		return ret;
	}

	data = (u8)ret;
	if (on) {
		ret = mpu6050_switch_engine(sensor, true,
			BIT_PWR_ACCEL_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.accel_enable = 1;

		data &= ~BIT_SLEEP;
		ret = i2c_smbus_write_byte_data(sensor->client,
				sensor->reg.pwr_mgmt_1, data);
		if (ret < 0) {
			dev_err(&sensor->client->dev,
				"Fail to set sensor power state, ret=%d\n",
				ret);
			return ret;
		}

		if (!sensor->cfg.int_enabled) {
			ret = mpu6050_set_interrupt(sensor,
				BIT_DATA_RDY_EN, true);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to enable interrupt mode for accel, ret=%d\n",
					ret);
				return ret;
			}
			enable_irq(sensor->client->irq);
			sensor->cfg.int_enabled = true;
		}

		sensor->cfg.enable = 1;
	} else {
		if (sensor->cfg.int_enabled && !sensor->cfg.gyro_enable) {
			ret = mpu6050_set_interrupt(sensor,
				BIT_DATA_RDY_EN, false);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to disable interrupt mode for accel, ret=%d\n",
					ret);
				return ret;
			}
			disable_irq(sensor->client->irq);
			sensor->cfg.int_enabled = false;
		}

		ret = mpu6050_switch_engine(sensor, false,
			BIT_PWR_ACCEL_STBY_MASK);
		if (ret)
			return ret;
		sensor->cfg.accel_enable = 0;

		if (!sensor->cfg.gyro_enable) {
			data |=  BIT_SLEEP;
			ret = i2c_smbus_write_byte_data(sensor->client,
					sensor->reg.pwr_mgmt_1, data);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Fail to set sensor power state for accel, ret=%d\n",
					ret);
				return ret;
			}
			sensor->cfg.enable = 0;
		}
	}
	return 0;
}

static int mpu6050_accel_set_enable(struct mpu6050_sensor *sensor, bool enable)
{
	int ret = 0;

	mutex_lock(&sensor->op_lock);
	if (enable) {
		if (!sensor->power_enabled) {
			ret = mpu6050_power_ctl(sensor, true);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Failed to set power up mpu6050");
				goto exit;
			}

			ret = mpu6050_restore_context(sensor);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Failed to restore context");
				goto exit;
			}
		}

		ret = mpu6050_accel_enable(sensor, true);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to enable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}

		if (sensor->use_poll)
			schedule_delayed_work(&sensor->accel_poll_work,
				msecs_to_jiffies(sensor->accel_poll_ms));
	} else {
		if (sensor->use_poll)
			cancel_delayed_work_sync(&sensor->accel_poll_work);

		ret = mpu6050_accel_enable(sensor, false);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to disable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}

	}

exit:
	mutex_unlock(&sensor->op_lock);
	return ret;
}

static int mpu6050_accel_set_poll_delay(struct mpu6050_sensor *sensor,
					unsigned long delay)
{
	int ret;

	mutex_lock(&sensor->op_lock);
	if (delay < MPU6050_ACCEL_MIN_POLL_INTERVAL_MS)
		delay = MPU6050_ACCEL_MIN_POLL_INTERVAL_MS;
	if (delay > MPU6050_ACCEL_MAX_POLL_INTERVAL_MS)
		delay = MPU6050_ACCEL_MAX_POLL_INTERVAL_MS;

	sensor->accel_poll_ms = delay;

	if (sensor->use_poll) {
		cancel_delayed_work_sync(&sensor->accel_poll_work);
		schedule_delayed_work(&sensor->accel_poll_work,
				msecs_to_jiffies(sensor->accel_poll_ms));
	} else {
		ret = mpu6050_config_sample_rate(sensor);
		if (ret < 0)
			dev_err(&sensor->client->dev,
				"Unable to set polling delay for accel!\n");
	}
	mutex_unlock(&sensor->op_lock);
	return 0;
}

static int mpu6050_accel_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	return mpu6050_accel_set_enable(sensor, enable);
}

static int mpu6050_accel_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	return mpu6050_accel_set_poll_delay(sensor, delay_ms);
}

static int mpu6050_accel_cdev_enable_wakeup(
			struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	if (sensor->use_poll)
		return -ENODEV;

	sensor->wakeup_en = enable;
	return 0;
}


/**
 * mpu6050_accel_attr_get_polling_delay - get the sampling rate
 */
static ssize_t mpu6050_accel_attr_get_polling_delay(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int val;
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	val = sensor ? sensor->accel_poll_ms : 0;
	return snprintf(buf, 8, "%d\n", val);
}

/**
 * mpu6050_accel_attr_set_polling_delay - set the sampling rate
 */
static ssize_t mpu6050_accel_attr_set_polling_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long interval_ms;
	int ret;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	ret = mpu6050_accel_set_poll_delay(sensor, interval_ms);

	return ret ? -EBUSY : size;
}

static ssize_t mpu6050_accel_attr_get_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sensor->cfg.accel_enable);
}

/**
 * mpu6050_accel_attr_set_enable -
 *    Set/get enable function is just needed by sensor HAL.
 */

static ssize_t mpu6050_accel_attr_set_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long enable;
	int ret;

	if (kstrtoul(buf, 10, &enable))
		return -EINVAL;

	if (enable)
		ret = mpu6050_accel_set_enable(sensor, true);
	else
		ret = mpu6050_accel_set_enable(sensor, false);

	return ret ? -EBUSY : count;
}

#ifdef DEBUG_NODE
u8 mpu6050_address;
u8 mpu6050_data;

static ssize_t mpu6050_accel_attr_get_reg_addr(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	return snprintf(buf, 8, "%d\n", mpu6050_address);
}

static ssize_t mpu6050_accel_attr_set_reg_addr(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long addr;

	if (kstrtoul(buf, 10, &addr))
		return -EINVAL;
	if ((addr < 0) || (addr > 255))
		return -EINVAL;

	mpu6050_address = addr;
	dev_info(dev, "mpu6050_address =%d\n", mpu6050_address);

	return size;
}

static ssize_t mpu6050_accel_attr_get_data(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_read_byte_data(sensor->client, mpu6050_address);
	dev_info(dev, "read addr(0x%x)=0x%x\n", mpu6050_address, ret);
	if (ret >= 0 && ret <= 255)
		mpu6050_data = ret;
	return snprintf(buf, 8, "0x%x\n", ret);
}

static ssize_t mpu6050_accel_attr_set_data(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long reg_data;

	if (kstrtoul(buf, 10, &reg_data))
		return -EINVAL;
	if ((reg_data < 0) || (reg_data > 255))
		return -EINVAL;

	mpu6050_data = reg_data;
	dev_info(dev, "set mpu6050_data =0x%x\n", mpu6050_data);

	return size;
}
static ssize_t mpu6050_accel_attr_reg_write(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);
	int ret;

	ret = i2c_smbus_write_byte_data(sensor->client,
		mpu6050_address, mpu6050_data);
	dev_info(dev, "write addr(0x%x)<-0x%x ret=%d\n",
		mpu6050_address, mpu6050_data, ret);

	return size;
}

#endif

static struct device_attribute accel_attr[] = {
	__ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		mpu6050_accel_attr_get_polling_delay,
		mpu6050_accel_attr_set_polling_delay),
	__ATTR(enable, S_IRUGO | S_IWUSR,
		mpu6050_accel_attr_get_enable,
		mpu6050_accel_attr_set_enable),
#ifdef DEBUG_NODE
	__ATTR(addr, S_IRUSR | S_IWUSR,
		mpu6050_accel_attr_get_reg_addr,
		mpu6050_accel_attr_set_reg_addr),
	__ATTR(reg, S_IRUSR | S_IWUSR,
		mpu6050_accel_attr_get_data,
		mpu6050_accel_attr_set_data),
	__ATTR(write, S_IWUSR,
		NULL,
		mpu6050_accel_attr_reg_write),
#endif
};

static int create_accel_sysfs_interfaces(struct device *dev)
{
	int i;
	int err;
	for (i = 0; i < ARRAY_SIZE(accel_attr); i++) {
		err = device_create_file(dev, accel_attr + i);
		if (err)
			goto error;
	}
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, accel_attr + i);
	dev_err(dev, "Unable to create interface\n");
	return err;
}

static int remove_accel_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(accel_attr); i++)
		device_remove_file(dev, accel_attr + i);
	return 0;
}

static void setup_mpu6050_reg(struct mpu_reg_map *reg)
{
	reg->sample_rate_div	= REG_SAMPLE_RATE_DIV;
	reg->lpf		= REG_CONFIG;
	reg->fifo_en		= REG_FIFO_EN;
	reg->gyro_config	= REG_GYRO_CONFIG;
	reg->accel_config	= REG_ACCEL_CONFIG;
	reg->mot_thr		= REG_ACCEL_MOT_THR;
	reg->mot_dur		= REG_ACCEL_MOT_DUR;
	reg->fifo_count_h	= REG_FIFO_COUNT_H;
	reg->fifo_r_w		= REG_FIFO_R_W;
	reg->raw_gyro		= REG_RAW_GYRO;
	reg->raw_accel		= REG_RAW_ACCEL;
	reg->temperature	= REG_TEMPERATURE;
	reg->int_pin_cfg	= REG_INT_PIN_CFG;
	reg->int_enable		= REG_INT_ENABLE;
	reg->int_status		= REG_INT_STATUS;
	reg->pwr_mgmt_1		= REG_PWR_MGMT_1;
	reg->pwr_mgmt_2		= REG_PWR_MGMT_2;
};

/**
 * mpu_check_chip_type() - check and setup chip type.
 */
static int mpu_check_chip_type(struct mpu6050_sensor *sensor,
		const struct i2c_device_id *id)
{
	struct i2c_client *client = sensor->client;
	struct mpu_reg_map *reg;
	s32 ret;

	if (!strcmp(id->name, "mpu6050"))
		sensor->chip_type = INV_MPU6050;
	else if (!strcmp(id->name, "mpu6500"))
		sensor->chip_type = INV_MPU6500;
	else if (!strcmp(id->name, "mpu6xxx"))
		sensor->chip_type = INV_MPU6050;
	else
		return -EPERM;

	reg = &sensor->reg;
	setup_mpu6050_reg(reg);

	/* turn off and turn on power to ensure gyro engine is on */

	ret = mpu6050_set_power_mode(sensor, false);
	if (ret)
		return ret;
	ret = mpu6050_set_power_mode(sensor, true);
	if (ret)
		return ret;

	if (!strcmp(id->name, "mpu6xxx")) {
		ret = i2c_smbus_read_byte_data(client,
				REG_WHOAMI);
		if (ret < 0)
			return ret;

		if (ret == MPU6500_ID) {
			sensor->chip_type = INV_MPU6500;
		} else if (ret == MPU6050_ID) {
			sensor->chip_type = INV_MPU6050;
		} else {
			dev_err(&client->dev,
				"Invalid chip ID %d\n", ret);
			return -ENODEV;
		}
	}
	return 0;
}

/**
 *  mpu6050_init_config() - Initialize hardware, disable FIFO.
 *  @indio_dev:	Device driver instance.
 *  Initial configuration:
 *  FSR: +/- 2000DPS
 *  DLPF: 42Hz
 *  FIFO rate: 50Hz
 *  AFS: 2G
 */
static int mpu6050_init_config(struct mpu6050_sensor *sensor)
{
	struct mpu_reg_map *reg;
	struct i2c_client *client;
	s32 ret;
	u8 data;

	if (sensor->cfg.is_asleep)
		return -EINVAL;

	reg = &sensor->reg;
	client = sensor->client;

	mpu6050_reset_chip(sensor);

	memset(&sensor->cfg, 0, sizeof(struct mpu_chip_config));

	/* Wake up from sleep */
	ret = i2c_smbus_write_byte_data(client, reg->pwr_mgmt_1,
		BIT_WAKEUP_AFTER_RESET);
	if (ret < 0)
		return ret;

	/* Gyro full scale range configure */
	ret = i2c_smbus_write_byte_data(client, reg->gyro_config,
		MPU_FSR_2000DPS << GYRO_CONFIG_FSR_SHIFT);
	if (ret < 0)
		return ret;
	sensor->cfg.fsr = MPU_FSR_2000DPS;

	ret = i2c_smbus_write_byte_data(client, reg->lpf, MPU_DLPF_42HZ);
	if (ret < 0)
		return ret;
	sensor->cfg.lpf = MPU_DLPF_42HZ;

	data = (u8)(ODR_DLPF_ENA / INIT_FIFO_RATE - 1);
	ret = i2c_smbus_write_byte_data(client, reg->sample_rate_div, data);
	if (ret < 0)
		return ret;
	sensor->cfg.rate_div = data;

	ret = i2c_smbus_write_byte_data(client, reg->accel_config,
		(ACCEL_FS_02G << ACCL_CONFIG_FSR_SHIFT));
	if (ret < 0)
		return ret;
	sensor->cfg.accel_fs = ACCEL_FS_02G;

	if ((sensor->pdata->int_flags & IRQF_TRIGGER_FALLING) ||
		(sensor->pdata->int_flags & IRQF_TRIGGER_LOW))
		data = BIT_INT_CFG_DEFAULT | BIT_INT_ACTIVE_LOW;
	else
		data = BIT_INT_CFG_DEFAULT;
	ret = i2c_smbus_write_byte_data(client, reg->int_pin_cfg, data);
	if (ret < 0)
		return ret;
	sensor->cfg.int_pin_cfg = data;

	/* Put sensor into sleep mode */
	ret = i2c_smbus_read_byte_data(client,
		sensor->reg.pwr_mgmt_1);
	if (ret < 0)
		return ret;

	data = (u8)ret;
	data |=  BIT_SLEEP;
	ret = i2c_smbus_write_byte_data(client,
		sensor->reg.pwr_mgmt_1, data);
	if (ret < 0)
		return ret;

	sensor->cfg.gyro_enable = 0;
	sensor->cfg.gyro_fifo_enable = 0;
	sensor->cfg.accel_enable = 0;
	sensor->cfg.accel_fifo_enable = 0;

	return 0;
}

static int mpu6050_pinctrl_init(struct mpu6050_sensor *sensor)
{
	struct i2c_client *client = sensor->client;

	sensor->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(sensor->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(sensor->pinctrl);
	}

	sensor->pin_default =
		pinctrl_lookup_state(sensor->pinctrl, MPU6050_PINCTRL_DEFAULT);
	if (IS_ERR_OR_NULL(sensor->pin_default))
		dev_err(&client->dev, "Failed to look up default state\n");

	sensor->pin_sleep =
		pinctrl_lookup_state(sensor->pinctrl, MPU6050_PINCTRL_SUSPEND);
	if (IS_ERR_OR_NULL(sensor->pin_sleep))
		dev_err(&client->dev, "Failed to look up sleep state\n");

	return 0;
}

static void mpu6050_pinctrl_state(struct mpu6050_sensor *sensor,
			bool active)
{
	struct i2c_client *client = sensor->client;
	int ret;

	dev_dbg(&client->dev, "mpu6050_pinctrl_state en=%d\n", active);

	if (active) {
		if (!IS_ERR_OR_NULL(sensor->pin_default)) {
			ret = pinctrl_select_state(sensor->pinctrl,
				sensor->pin_default);
			if (ret)
				dev_err(&client->dev,
					"Error pinctrl_select_state(%s) err:%d\n",
					MPU6050_PINCTRL_DEFAULT, ret);
		}
	} else {
		if (!IS_ERR_OR_NULL(sensor->pin_sleep)) {
			ret = pinctrl_select_state(sensor->pinctrl,
				sensor->pin_sleep);
			if (ret)
				dev_err(&client->dev,
					"Error pinctrl_select_state(%s) err:%d\n",
					MPU6050_PINCTRL_SUSPEND, ret);
		}
	}
	return;
}

#ifdef CONFIG_OF
static int mpu6050_dt_get_place(struct device *dev,
			struct mpu6050_platform_data *pdata)
{
	const char *place_name;
	int rc;
	int i;

	rc = of_property_read_string(dev->of_node, "invn,place", &place_name);
	if (rc) {
		dev_err(dev, "Cannot get place configuration!\n");
		return -EINVAL;
	}

	for (i = 0; i < MPU6050_AXIS_REMAP_TAB_SZ; i++) {
		if (!strcmp(place_name, mpu6050_place_name2num[i].name)) {
			pdata->place = mpu6050_place_name2num[i].place;
			break;
		}
	}
	if (i >= MPU6050_AXIS_REMAP_TAB_SZ) {
		dev_warn(dev, "Invalid place parameter, use default value 0\n");
		pdata->place = 0;
	}

	return 0;
}

static int mpu6050_parse_dt(struct device *dev,
			struct mpu6050_platform_data *pdata)
{
	int rc;

	rc = mpu6050_dt_get_place(dev, pdata);
	if (rc)
		return rc;

	/* check gpio_int later, use polling if gpio_int is invalid. */
	pdata->gpio_int = of_get_named_gpio_flags(dev->of_node,
				"invn,gpio-int", 0, &pdata->int_flags);

	pdata->gpio_en = of_get_named_gpio_flags(dev->of_node,
				"invn,gpio-en", 0, NULL);

	pdata->use_int = of_property_read_bool(dev->of_node,
				"invn,use-interrupt");

	return 0;
}
#else
static int mpu6050_parse_dt(struct device *dev,
			struct mpu6050_platform_data *pdata)
{
	return -EINVAL;
}
#endif

/**
 * mpu6050_probe - device detection callback
 * @client: i2c client of found device
 * @id: id match information
 *
 * The I2C layer calls us when it believes a sensor is present at this
 * address. Probe to see if this is correct and to validate the device.
 *
 * If present install the relevant sysfs interfaces and input device.
 */
static int mpu6050_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct mpu6050_sensor *sensor;
	struct mpu6050_platform_data *pdata;
	int ret;

	ret = i2c_check_functionality(client->adapter,
					 I2C_FUNC_SMBUS_BYTE |
					 I2C_FUNC_SMBUS_BYTE_DATA |
					 I2C_FUNC_I2C);
	if (!ret) {
		dev_err(&client->dev,
			"Required I2C funcationality does not supported\n");
		return -ENODEV;
	}
	sensor = devm_kzalloc(&client->dev, sizeof(struct mpu6050_sensor),
			GFP_KERNEL);
	if (!sensor) {
		dev_err(&client->dev, "Failed to allocate driver data\n");
		return -ENOMEM;
	}

	sensor->client = client;
	sensor->dev = &client->dev;
	i2c_set_clientdata(client, sensor);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct mpu6050_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allcated memory\n");
			ret = -ENOMEM;
			goto err_free_devmem;
		}
		ret = mpu6050_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "Failed to parse device tree\n");
			ret = -EINVAL;
			goto err_free_devmem;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "Cannot get device platform data\n");
		ret = -EINVAL;
		goto err_free_devmem;
	}

	mutex_init(&sensor->op_lock);
	sensor->pdata = pdata;
	sensor->enable_gpio = sensor->pdata->gpio_en;

	ret = mpu6050_pinctrl_init(sensor);
	if (ret) {
		dev_err(&client->dev, "Can't initialize pinctrl\n");
		goto err_free_devmem;
	}

	if (gpio_is_valid(sensor->enable_gpio)) {
		ret = gpio_request(sensor->enable_gpio, "MPU_EN_PM");
		gpio_direction_output(sensor->enable_gpio, 0);
	}
	ret = mpu6050_power_init(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to init regulator\n");
		goto err_free_enable_gpio;
	}
	ret = mpu6050_power_ctl(sensor, true);
	if (ret) {
		dev_err(&client->dev, "Failed to power on device\n");
		goto err_deinit_regulator;
	}

	ret = mpu_check_chip_type(sensor, id);
	if (ret) {
		dev_err(&client->dev, "Cannot get invalid chip type\n");
		goto err_power_off_device;
	}

	ret = mpu6050_init_engine(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to init chip engine\n");
		goto err_power_off_device;
	}

	ret = mpu6050_set_lpa_freq(sensor, MPU6050_LPA_5HZ);
	if (ret) {
		dev_err(&client->dev, "Failed to set lpa frequency\n");
		goto err_power_off_device;
	}

	sensor->cfg.is_asleep = false;
	ret = mpu6050_init_config(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to set default config\n");
		goto err_power_off_device;
	}

	sensor->accel_dev = input_allocate_device();
	if (!sensor->accel_dev) {
		dev_err(&client->dev,
			"Failed to allocate accelerometer input device\n");
		ret = -ENOMEM;
		goto err_power_off_device;
	}

	sensor->gyro_dev = input_allocate_device();
	if (!sensor->gyro_dev) {
		dev_err(&client->dev,
			"Failed to allocate gyroscope input device\n");
		ret = -ENOMEM;
		goto err_free_input_accel;
	}

	sensor->accel_dev->name = MPU6050_DEV_NAME_ACCEL;
	sensor->gyro_dev->name = MPU6050_DEV_NAME_GYRO;
	sensor->accel_dev->id.bustype = BUS_I2C;
	sensor->gyro_dev->id.bustype = BUS_I2C;
	sensor->accel_poll_ms = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS;
	sensor->gyro_poll_ms = MPU6050_GYRO_DEFAULT_POLL_INTERVAL_MS;

	input_set_capability(sensor->accel_dev, EV_ABS, ABS_MISC);
	input_set_capability(sensor->gyro_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(sensor->accel_dev, ABS_X,
			MPU6050_ACCEL_MIN_VALUE, MPU6050_ACCEL_MAX_VALUE,
			0, 0);
	input_set_abs_params(sensor->accel_dev, ABS_Y,
			MPU6050_ACCEL_MIN_VALUE, MPU6050_ACCEL_MAX_VALUE,
			0, 0);
	input_set_abs_params(sensor->accel_dev, ABS_Z,
			MPU6050_ACCEL_MIN_VALUE, MPU6050_ACCEL_MAX_VALUE,
			0, 0);
	input_set_abs_params(sensor->gyro_dev, ABS_RX,
			     MPU6050_GYRO_MIN_VALUE, MPU6050_GYRO_MAX_VALUE,
			     0, 0);
	input_set_abs_params(sensor->gyro_dev, ABS_RY,
			     MPU6050_GYRO_MIN_VALUE, MPU6050_GYRO_MAX_VALUE,
			     0, 0);
	input_set_abs_params(sensor->gyro_dev, ABS_RZ,
			     MPU6050_GYRO_MIN_VALUE, MPU6050_GYRO_MAX_VALUE,
			     0, 0);
	sensor->accel_dev->dev.parent = &client->dev;
	sensor->gyro_dev->dev.parent = &client->dev;
	input_set_drvdata(sensor->accel_dev, sensor);
	input_set_drvdata(sensor->gyro_dev, sensor);

	if ((sensor->pdata->use_int) &&
		gpio_is_valid(sensor->pdata->gpio_int)) {
		sensor->use_poll = 0;

		/* configure interrupt gpio */
		ret = gpio_request(sensor->pdata->gpio_int,
							"mpu_gpio_int");
		if (ret) {
			dev_err(&client->dev,
				"Unable to request interrupt gpio %d\n",
				sensor->pdata->gpio_int);
			goto err_free_input_gyro;
		}

		ret = gpio_direction_input(sensor->pdata->gpio_int);
		if (ret) {
			dev_err(&client->dev,
				"Unable to set direction for gpio %d\n",
				sensor->pdata->gpio_int);
			goto err_free_gpio;
		}
		client->irq = gpio_to_irq(sensor->pdata->gpio_int);

		ret = request_threaded_irq(client->irq,
				     NULL, mpu6050_interrupt_thread,
				     sensor->pdata->int_flags | IRQF_ONESHOT,
				     "mpu6050", sensor);
		if (ret) {
			dev_err(&client->dev,
				"Can't get IRQ %d, error %d\n",
				client->irq, ret);
			client->irq = 0;
			goto err_free_gpio;
		}
		/* Disable interrupt until event is enabled */
		disable_irq(client->irq);
	} else {
		sensor->use_poll = 1;
		INIT_DELAYED_WORK(&sensor->accel_poll_work,
			mpu6050_accel_work_fn);
		INIT_DELAYED_WORK(&sensor->gyro_poll_work,
			mpu6050_gyro_work_fn);
		dev_dbg(&client->dev,
			"Polling mode is enabled. use_int=%d gpio_int=%d",
			sensor->pdata->use_int, sensor->pdata->gpio_int);
	}

	ret = input_register_device(sensor->accel_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_free_irq;
	}
	ret = input_register_device(sensor->gyro_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_unregister_accel;
	}

	ret = create_accel_sysfs_interfaces(&sensor->accel_dev->dev);
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs for accel\n");
		goto err_unregister_gyro;
	}
	ret = create_gyro_sysfs_interfaces(&sensor->gyro_dev->dev);
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs for gyro\n");
		goto err_remove_accel_sysfs;
	}

	sensor->accel_cdev = mpu6050_acc_cdev;
	sensor->accel_cdev.delay_msec = sensor->accel_poll_ms;
	sensor->accel_cdev.sensors_enable = mpu6050_accel_cdev_enable;
	sensor->accel_cdev.sensors_poll_delay = mpu6050_accel_cdev_poll_delay;
	sensor->accel_cdev.sensors_enable_wakeup =
					mpu6050_accel_cdev_enable_wakeup;
	ret = sensors_classdev_register(&client->dev, &sensor->accel_cdev);
	if (ret) {
		dev_err(&client->dev,
			"create accel class device file failed!\n");
		ret = -EINVAL;
		goto err_remove_gyro_sysfs;
	}

	sensor->gyro_cdev = mpu6050_gyro_cdev;
	sensor->gyro_cdev.delay_msec = sensor->gyro_poll_ms;
	sensor->gyro_cdev.sensors_enable = mpu6050_gyro_cdev_enable;
	sensor->gyro_cdev.sensors_poll_delay = mpu6050_gyro_cdev_poll_delay;
	ret = sensors_classdev_register(&client->dev, &sensor->gyro_cdev);
	if (ret) {
		dev_err(&client->dev,
			"create accel class device file failed!\n");
		ret = -EINVAL;
		goto err_remove_accel_cdev;
	}

	ret = mpu6050_power_ctl(sensor, false);
	if (ret) {
		dev_err(&client->dev,
				"Power off mpu6050 failed\n");
		goto err_remove_gyro_cdev;
	}

	return 0;
err_remove_gyro_cdev:
	sensors_classdev_unregister(&sensor->gyro_cdev);
err_remove_accel_cdev:
	 sensors_classdev_unregister(&sensor->accel_cdev);
err_remove_gyro_sysfs:
	remove_accel_sysfs_interfaces(&sensor->gyro_dev->dev);
err_remove_accel_sysfs:
	remove_accel_sysfs_interfaces(&sensor->accel_dev->dev);
err_unregister_gyro:
	input_unregister_device(sensor->gyro_dev);
err_unregister_accel:
	input_unregister_device(sensor->accel_dev);
err_free_irq:
	if (client->irq > 0)
		free_irq(client->irq, sensor);
err_free_gpio:
	if ((sensor->pdata->use_int) &&
		(gpio_is_valid(sensor->pdata->gpio_int)))
		gpio_free(sensor->pdata->gpio_int);
err_free_input_gyro:
	input_free_device(sensor->gyro_dev);
err_free_input_accel:
	input_free_device(sensor->accel_dev);
err_power_off_device:
	mpu6050_power_ctl(sensor, false);
err_deinit_regulator:
	mpu6050_power_deinit(sensor);
err_free_enable_gpio:
	if (gpio_is_valid(sensor->enable_gpio))
		gpio_free(sensor->enable_gpio);
err_free_devmem:
	devm_kfree(&client->dev, sensor);
	dev_err(&client->dev, "Probe device return error%d\n", ret);
	return ret;
}

/**
 * mpu6050_remove - remove a sensor
 * @client: i2c client of sensor being removed
 *
 * Our sensor is going away, clean up the resources.
 */
static int mpu6050_remove(struct i2c_client *client)
{
	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);

	sensors_classdev_unregister(&sensor->accel_cdev);
	sensors_classdev_unregister(&sensor->gyro_cdev);
	remove_gyro_sysfs_interfaces(&sensor->gyro_dev->dev);
	remove_accel_sysfs_interfaces(&sensor->accel_dev->dev);
	input_unregister_device(sensor->gyro_dev);
	input_unregister_device(sensor->accel_dev);
	if (client->irq > 0)
		free_irq(client->irq, sensor);
	if ((sensor->pdata->use_int) &&
		(gpio_is_valid(sensor->pdata->gpio_int)))
		gpio_free(sensor->pdata->gpio_int);
	input_free_device(sensor->gyro_dev);
	input_free_device(sensor->accel_dev);
	mpu6050_power_ctl(sensor, false);
	mpu6050_power_deinit(sensor);
	if (gpio_is_valid(sensor->enable_gpio))
		gpio_free(sensor->enable_gpio);
	devm_kfree(&client->dev, sensor);

	return 0;
}

#ifdef CONFIG_PM
/**
 * mpu6050_suspend - called on device suspend
 * @dev: device being suspended
 *
 * Put the device into sleep mode before we suspend the machine.
 */
static int mpu6050_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&sensor->op_lock);
	if (sensor->cfg.accel_enable && sensor->wakeup_en) {
		/* keep accel on and config motion detection wakeup */
		ret = mpu6050_set_interrupt(sensor,
				BIT_DATA_RDY_EN, false);
		if (ret == 0)
			ret = mpu6050_set_motion_det(sensor, true);
		if (ret == 0) {
			irq_set_irq_wake(client->irq, 1);

			dev_dbg(&client->dev,
				"Enable motion detection success\n");
			goto exit;
		}
		/*  if motion detection config does not success,
		  *  not exit suspend and sensor will be power off.
		  */
	}

	if (!sensor->use_poll) {
		disable_irq(client->irq);
	} else {
		if (sensor->cfg.gyro_enable)
			cancel_delayed_work_sync(&sensor->gyro_poll_work);

		if (sensor->cfg.accel_enable)
			cancel_delayed_work_sync(&sensor->accel_poll_work);
	}

	mpu6050_set_power_mode(sensor, false);
	ret = mpu6050_power_ctl(sensor, false);
	if (ret < 0) {
		dev_err(&client->dev, "Power off mpu6050 failed\n");
		goto exit;
	}

exit:
	mutex_unlock(&sensor->op_lock);
	dev_dbg(&client->dev, "Suspend completed, ret=%d\n", ret);

	return 0;
}

/**
 * mpu6050_resume - called on device resume
 * @dev: device being resumed
 *
 * Put the device into powered mode on resume.
 */
static int mpu6050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);
	int ret = 0;

	if (sensor->cfg.mot_det_on) {
		/* keep accel on and config motion detection wakeup */
		irq_set_irq_wake(client->irq, 0);
		mpu6050_set_motion_det(sensor, false);
		mpu6050_set_interrupt(sensor,
				BIT_DATA_RDY_EN, true);
		dev_dbg(&client->dev, "Disable motion detection success\n");
		goto exit;
	}

	/* Keep sensor power on to prevent bad power state */
	ret = mpu6050_power_ctl(sensor, true);
	if (ret < 0) {
		dev_err(&client->dev, "Power on mpu6050 failed\n");
		goto exit;
	}
	/* Reset sensor to recovery from unexpected state */
	mpu6050_reset_chip(sensor);

	ret = mpu6050_restore_context(sensor);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to restore context\n");
		goto exit;
	}

	/* Enter sleep mode if both accel and gyro are not enabled */
	ret = mpu6050_set_power_mode(sensor, sensor->cfg.enable);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to set power mode enable=%d\n",
					sensor->cfg.enable);
		goto exit;
	}

	if (sensor->cfg.gyro_enable) {
		ret = mpu6050_gyro_enable(sensor, true);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to enable gyro\n");
			goto exit;
		}

		if (sensor->use_poll) {
			schedule_delayed_work(&sensor->gyro_poll_work,
				msecs_to_jiffies(sensor->gyro_poll_ms));
		}
	}

	if (sensor->cfg.accel_enable) {
		ret = mpu6050_accel_enable(sensor, true);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to enable accel\n");
			goto exit;
		}

		if (sensor->use_poll) {
			schedule_delayed_work(&sensor->accel_poll_work,
				msecs_to_jiffies(sensor->accel_poll_ms));
		}
	}

	if (!sensor->use_poll)
		enable_irq(client->irq);

exit:
	dev_dbg(&client->dev, "Resume complete, ret = %d\n", ret);
	return ret;
}
#endif

static UNIVERSAL_DEV_PM_OPS(mpu6050_pm, mpu6050_suspend, mpu6050_resume, NULL);

static const struct i2c_device_id mpu6050_ids[] = {
	{ "mpu6050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_ids);

static const struct of_device_id mpu6050_of_match[] = {
	{ .compatible = "invn,mpu6050", },
	{ },
};
MODULE_DEVICE_TABLE(of, mpu6050_of_match);

static struct i2c_driver mpu6050_i2c_driver = {
	.driver	= {
		.name	= "mpu6050",
		.owner	= THIS_MODULE,
		.pm	= &mpu6050_pm,
		.of_match_table = mpu6050_of_match,
	},
	.probe		= mpu6050_probe,
	.remove		= mpu6050_remove,
	.id_table	= mpu6050_ids,
};

module_i2c_driver(mpu6050_i2c_driver);

MODULE_DESCRIPTION("MPU6050 Tri-axis gyroscope driver");
MODULE_LICENSE("GPL v2");

/*
 * MPU6050 6-axis gyroscope + accelerometer driver
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/miscdevice.h>
#include <linux/hardware_info.h>
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>

#define DEBUG_NODE

#define IS_ODD_NUMBER(x)	(x & 1UL)

/*VDD 2.375V-3.46V VLOGIC 1.8V +-5%*/
#define MPU6050_VDD_MIN_UV	2500000
#define MPU6050_VDD_MAX_UV	3400000
#define MPU6050_VI2C_MIN_UV	1750000
#define MPU6050_VI2C_MAX_UV	1950000

#define MPU6050_ACCEL_MIN_VALUE	-32768
#define MPU6050_ACCEL_MAX_VALUE	32767
#define MPU6050_GYRO_MIN_VALUE	-32768
#define MPU6050_GYRO_MAX_VALUE	32767

#define MPU6050_MAX_EVENT_CNT	170
/* Limit mininum delay to 10ms as we do not need higher rate so far */
#define MPU6050_ACCEL_MIN_POLL_INTERVAL_MS	10
#define MPU6050_ACCEL_MAX_POLL_INTERVAL_MS	5000
#define MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS	200
#define MPU6050_ACCEL_INT_MAX_DELAY			19

#define MPU6050_GYRO_MIN_POLL_INTERVAL_MS	10
#define MPU6050_GYRO_MAX_POLL_INTERVAL_MS	5000
#define MPU6050_GYRO_DEFAULT_POLL_INTERVAL_MS	200
#define MPU6050_GYRO_INT_MAX_DELAY		18

#define MPU6050_RAW_ACCEL_DATA_LEN	6
#define MPU6050_RAW_GYRO_DATA_LEN	6

#define MPU6050_RESET_SLEEP_US	10

#define MPU6050_DEV_NAME_ACCEL	"MPU6050-accel"
#define MPU6050_DEV_NAME_GYRO	"gyroscope"

#define MPU6050_PINCTRL_DEFAULT	"mpu_default"
#define MPU6050_PINCTRL_SUSPEND	"mpu_sleep"

#define GS_GET_RAW_DATA_FOR_CALI	_IOW('c', 9, int *)
#define GS_REC_DATA_FOR_PER	_IOW('c', 10, int *)
#define GYRO_GET_RAW_DATA_FOR_CALI	_IOW('c', 9, int *)
#define GYRO_REC_DATA_FOR_CALI	_IOW('c', 10, int *)

#define MPU6050_AXIS_X		  0
#define MPU6050_AXIS_Y		  1
#define MPU6050_AXIS_Z		  2

#define MPU6050_AXES_NUM		3

#define MPU6050_RANGE_2G			(0x00 << 3)
#define MPU6050_RANGE_4G			(0x01 << 3)
#define MPU6050_RANGE_8G			(0x02 << 3)
#define MPU6050_RANGE_16G			(0x03 << 3)

#define MPU6050_REG_DATA_FORMAT		0x1C

#define MPU6050_RANGE_PN250dps			(0x00 << 3)
#define MPU6050_RANGE_PN500dps			(0x01 << 3)
#define MPU6050_RANGE_PN1000dps			(0x02 << 3)
#define MPU6050_RANGE_PN2000dps			(0x03 << 3)
#define MPU6050_REG_Gyro_DATA_FORMAT		0x1B

/* Gyro Offset Max Value (dps) */
#define DEF_GYRO_OFFSET_MAX			 120
#define DEF_ST_PRECISION				1000
#define DEF_SELFTEST_GYRO_SENS_250		  (32768 / 250)
#define DEF_SELFTEST_GYRO_SENS_500		  (32768 / 500)
#define DEF_SELFTEST_GYRO_SENS_1000		  (32768 / 1000)
#define DEF_SELFTEST_GYRO_SENS_2000		  (32768 / 2000)


#ifdef DEBUG
#define wing_info(fmt, ...) \
	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
#define wing_info(fmt, ...) \
	no_printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif

static int g_has_initconfig;

#ifdef GYRO_DATA_FILTER
struct mpu6050_sensor *globe_sensor;
#define C_MAX_FIR_LENGTH (32)
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][3];
	int sum[3];
	int num;
	int idx;
	int firlen;
};
struct data_filter  gyro_fir;
#endif
#define CAL_SKIP_COUNT	5
#define MPU_ACC_CAL_COUNT	15
#define MPU_ACC_CAL_NUM	(MPU_ACC_CAL_COUNT - CAL_SKIP_COUNT)
#define MPU_ACC_CAL_BUF_SIZE	22
#define RAW_TO_1G	16384
#define MPU_ACC_CAL_DELAY 100	/* ms */
#define POLL_MS_100HZ 10
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

struct cali_data {
	int x;
	int y;
	int z;
	int offset;
	int rx;
	int ry;
	int rz;
	int roffset;
};

/**
 *  struct mpu6050_sensor - Cached chip configuration data
 *  @client:		I2C client
 *  @dev:		device structure
 *  @accel_dev:		accelerometer input device structure
 *  @gyro_dev:		gyroscope input device structure
 *  @accel_cdev:		sensor class device structure for accelerometer
 *  @gyro_cdev:		sensor class device structure for gyroscope
 *  @pdata:	device platform dependent data
 *  @op_lock:	device operation mutex
 *  @chip_type:	sensor hardware model
 *  @accel_poll_work:	accelerometer delay work structur
 *  @gyro_poll_work:	gyroscope delay work structure
 *  @fifo_flush_work:	work structure to flush sensor fifo
 *  @reg:		notable slave registers
 *  @cfg:		cached chip configuration data
 *  @axis:	axis data reading
 *  @gyro_poll_ms:	gyroscope polling delay
 *  @accel_poll_ms:	accelerometer polling delay
 *  @accel_latency_ms:	max latency for accelerometer batching
 *  @gyro_latency_ms:	max latency for gyroscope batching
 *  @accel_en:	accelerometer enabling flag
 *  @gyro_en:	gyroscope enabling flag
 *  @use_poll:		use polling mode instead of  interrupt mode
 *  @motion_det_en:	motion detection wakeup is enabled
 *  @batch_accel:	accelerometer is working on batch mode
 *  @batch_gyro:	gyroscope is working on batch mode
 *  @vlogic:	regulator data for Vlogic
 *  @vdd:	regulator data for Vdd
 *  @vi2c:	I2C bus pullup
 *  @enable_gpio:	enable GPIO
 *  @power_enabled:	flag of device power state
 *  @pinctrl:	pinctrl struct for interrupt pin
 *  @pin_default:	pinctrl default state
 *  @pin_sleep:	pinctrl sleep state
 *  @flush_count:	number of flush
 *  @fifo_start_ns:		timestamp of first fifo data
 */
struct mpu6050_sensor {
	struct i2c_client *client;
	struct device *dev;
	struct hrtimer gyro_timer;
	struct hrtimer accel_timer;
	struct input_dev *accel_dev;
	struct input_dev *gyro_dev;
	struct sensors_classdev accel_cdev;
	struct sensors_classdev gyro_cdev;
	struct mpu6050_platform_data *pdata;
	struct mutex op_lock;
	enum inv_devices chip_type;
	struct workqueue_struct *data_wq;
	struct delayed_work accel_poll_work;
	struct delayed_work gyro_poll_work;
	struct delayed_work fifo_flush_work;
	struct mpu_reg_map reg;
	struct mpu_chip_config cfg;
	struct axis_data axis;
	struct cali_data cali;
	u32 gyro_poll_ms;
	u32 accel_poll_ms;
	u32 accel_latency_ms;
	u32 gyro_latency_ms;
	atomic_t accel_en;
	atomic_t gyro_en;
	bool use_poll;
	bool motion_det_en;
	bool batch_accel;
	bool batch_gyro;

	/* calibration */
	char acc_cal_buf[MPU_ACC_CAL_BUF_SIZE];
	int acc_cal_params[3];
	bool acc_use_cal;

	/* power control */
	struct regulator *vdd;
	struct regulator *vi2c;
	int enable_gpio;
	bool power_enabled;

	/* pinctrl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;

	u32 flush_count;
	u64 fifo_start_ns;
	int gyro_wkp_flag;
	int accel_wkp_flag;
	struct task_struct *gyr_task;
	struct task_struct *accel_task;
	bool gyro_delay_change;
	bool accel_delay_change;
	wait_queue_head_t	gyro_wq;
	wait_queue_head_t	accel_wq;
};
struct mpu6050_sensor *mpu_info;
static int mpu6050_init_config(struct mpu6050_sensor *sensor);

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
	.max_delay = MPU6050_ACCEL_MAX_POLL_INTERVAL_MS,
	.delay_msec = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.max_latency = 0,
	.flags = 0, /* SENSOR_FLAG_CONTINUOUS_MODE */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_enable_wakeup = NULL,
	.sensors_set_latency = NULL,
	.sensors_flush = NULL,
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
	.max_delay = MPU6050_GYRO_MAX_POLL_INTERVAL_MS,
	.delay_msec = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.max_latency = 0,
	.flags = 0, /* SENSOR_FLAG_CONTINUOUS_MODE */
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_enable_wakeup = NULL,
	.sensors_set_latency = NULL,
	.sensors_flush = NULL,
};
static char selftestRes[8] = {0};

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
static int gyro_poll_thread(void *data);
static int accel_poll_thread(void *data);
static void mpu6050_pinctrl_state(struct mpu6050_sensor *sensor,
			bool active);
static int mpu6050_set_interrupt(struct mpu6050_sensor *sensor,
		const u8 mask, bool on);
static int mpu6050_set_fifo(struct mpu6050_sensor *sensor,
					bool en_accel, bool en_gyro);
static void mpu6050_flush_fifo(struct mpu6050_sensor *sensor);
static int mpu6050_config_sample_rate(struct mpu6050_sensor *sensor);
static void mpu6050_acc_data_process(struct mpu6050_sensor *sensor);

static inline void mpu6050_set_fifo_start_time(struct mpu6050_sensor *sensor)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	sensor->fifo_start_ns = timespec_to_ns(&ts);
}

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
		if (!IS_ERR_OR_NULL(sensor->vi2c)) {
			rc = regulator_enable(sensor->vi2c);
			if (rc) {
				dev_err(&sensor->client->dev,
					"Regulator vi2c enable failed rc=%d\n",
					rc);
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
reg_vdd_put:
	regulator_put(sensor->vdd);
	return ret;
}

static int mpu6050_power_deinit(struct mpu6050_sensor *sensor)
{
	int ret = 0;
	if (regulator_count_voltages(sensor->vdd) > 0)
		regulator_set_voltage(sensor->vdd, 0, MPU6050_VDD_MAX_UV);
	regulator_put(sensor->vdd);
	return ret;
}

/**
 * mpu6050_read_reg() - read multiple register data
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

/* I2C Write */
static int8_t I2C_Write(uint8_t *txData, uint8_t length)
{
	int8_t index;

	struct mpu6050_sensor *self_info = mpu_info;
	struct i2c_msg data[] = {
		{
			.addr = self_info->client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	for (index = 0; index < 5; index++) {
		if (i2c_transfer(self_info->client->adapter, data, 1) > 0)
			break;

		usleep(10000);
	}

	if (index >= 5) {
		pr_alert("%s I2C Write Fail !!!!\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mpu6050_write_reg(struct i2c_client *client, u8 start_addr,
				   u8 data, int length)
{
	int ret = 0;
	u8 buf[2];

	buf[0] = start_addr;
	buf[1] = data;

	ret = I2C_Write(buf, 2);
	if (ret < 0) {
		dev_err(&client->dev, "%s | 0x%02X", __func__, buf[0]);
		return -EIO;
	}

		return 0;
}

/**
 * mpu6050_read_accel_data() - get accelerometer data from device
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
 * mpu6050_read_gyro_data() - get gyro data from device
 * @sensor: sensor device instance
 * @data: axis data to update
 *
 * Return the converted RX RY and RZ data from the sensor device
 */
static void mpu6050_read_gyro_data(struct mpu6050_sensor *sensor,
			     struct axis_data *data)
{
	u16 buffer[3];
#ifdef GYRO_DATA_FILTER
	int k;
#endif

	mpu6050_read_reg(sensor->client, sensor->reg.raw_gyro,
		(u8 *)buffer, MPU6050_RAW_GYRO_DATA_LEN);
	data->rx = be16_to_cpu(buffer[0]);
	data->ry = be16_to_cpu(buffer[1]);
	data->rz = be16_to_cpu(buffer[2]);

#ifdef GYRO_DATA_FILTER

	gyro_fir.raw[gyro_fir.idx][0] = be16_to_cpu(buffer[0]);
	gyro_fir.raw[gyro_fir.idx][1] = be16_to_cpu(buffer[1]);
	gyro_fir.raw[gyro_fir.idx][2] = be16_to_cpu(buffer[2]);

	if (gyro_fir.idx >= gyro_fir.firlen-1) {
		gyro_fir.idx = 0;
	} else {
		gyro_fir.idx++;
	}

	if (gyro_fir.num < gyro_fir.firlen) {
		gyro_fir.num++;
	}

	gyro_fir.sum[0]  = 0;
	gyro_fir.sum[1]  = 0;
	gyro_fir.sum[2]  = 0;

	for (k = 0; k < gyro_fir.num; k++) {
		gyro_fir.sum[0] += gyro_fir.raw[k][0];
		gyro_fir.sum[1] += gyro_fir.raw[k][1];
		gyro_fir.sum[2] += gyro_fir.raw[k][2];
	}

	data->rx = gyro_fir.sum[0] / gyro_fir.num;
	data->ry = gyro_fir.sum[1] / gyro_fir.num;
	data->rz = gyro_fir.sum[2] / gyro_fir.num;
#endif
}

/**
 * mpu6050_remap_accel_data() - remap accelerometer raw data to axis data
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
 * mpu6050_remap_gyro_data() - remap gyroscope raw data to axis data
 * @data: data to remap
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
 * mpu6050_read_single_event() - handle one sensor event.
 * @sensor: sensor device instance
 *
 * It only reads one sensor event from sensor register and send it to
 * sensor HAL, FIFO overflow and motion detection interrupt should be
 * handle by seperate function.
 */
static void mpu6050_read_single_event(struct mpu6050_sensor *sensor)
{
	u32 shift;

	if (sensor->cfg.accel_enable) {
		mpu6050_acc_data_process(sensor);
		shift = mpu_accel_fs_shift[sensor->cfg.accel_fs];
		input_report_abs(sensor->accel_dev, ABS_X,
			(sensor->axis.x << shift));
		input_report_abs(sensor->accel_dev, ABS_Y,
			(sensor->axis.y << shift));
		input_report_abs(sensor->accel_dev, ABS_Z,
			(sensor->axis.z << shift));
		input_sync(sensor->accel_dev);
	}

	if (sensor->cfg.gyro_enable) {
		mpu6050_read_gyro_data(sensor, &sensor->axis);
		mpu6050_remap_gyro_data(&sensor->axis,
			sensor->pdata->place);

		shift = mpu_gyro_fs_shift[sensor->cfg.fsr];
		input_report_abs(sensor->gyro_dev, ABS_RX,
			(sensor->axis.rx >> shift));
		input_report_abs(sensor->gyro_dev, ABS_RY,
			(sensor->axis.ry >> shift));
		input_report_abs(sensor->gyro_dev, ABS_RZ,
			(sensor->axis.rz >> shift));
		input_sync(sensor->gyro_dev);
	}
	return;
}

/**
 * mpu6050_interrupt_thread() - handle an IRQ
 * @irq: interrupt number
 * @data: the sensor device
 *
 * Called by the kernel single threaded after an interrupt occurs. Read
 * the sensor data and generate an input event for it.
 */
static irqreturn_t mpu6050_interrupt_thread(int irq, void *data)
{
	struct mpu6050_sensor *sensor = data;
	int ret;

	mutex_lock(&sensor->op_lock);
	ret = i2c_smbus_read_byte_data(sensor->client,
				sensor->reg.int_status);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Get interrupt source fail, ret = %d\n", ret);
		goto exit;
	}
	dev_dbg(&sensor->client->dev, "Interrupt source=0x%x\n", ret);

	if (ret & BIT_FIFO_OVERFLOW)
		mpu6050_flush_fifo(sensor);
	else if (ret & (BIT_MOT_EN | BIT_ZMOT_EN))
		mpu6050_read_single_event(sensor);
	else if (ret & BIT_DATA_RDY_INT)
		mpu6050_read_single_event(sensor);
	else
		dev_info(&sensor->client->dev, "Unknown interrupt 0x%x", ret);

exit:
	mutex_unlock(&sensor->op_lock);
	return IRQ_HANDLED;
}

static void mpu6050_sche_next_flush(struct mpu6050_sensor *sensor)
{
	u32 latency;

	if ((sensor->batch_accel) && (sensor->batch_gyro)) {
		if (sensor->gyro_latency_ms < sensor->accel_latency_ms)
			latency = sensor->gyro_latency_ms;
		else
			latency = sensor->accel_latency_ms;
	} else if (sensor->batch_accel)
		latency = sensor->accel_latency_ms;
	else if (sensor->batch_gyro)
		latency = sensor->gyro_latency_ms;
	else
		latency = 0;

	if (latency != 0)
		queue_delayed_work(sensor->data_wq,
			&sensor->fifo_flush_work,
			msecs_to_jiffies(latency));
	else
		dev_err(&sensor->client->dev,
			"unknown error, accel: en=%d latency=%d gyro: en=%d latency=%d\n",
			sensor->batch_accel,
			sensor->accel_latency_ms,
			sensor->batch_gyro,
			sensor->gyro_latency_ms);

	return;
}

/**
 * mpu6050_fifo_flush_fn() - flush shared sensor FIFO
 * @work: the work struct
 */
static void mpu6050_fifo_flush_fn(struct work_struct *work)
{
	struct mpu6050_sensor *sensor = container_of(
				(struct delayed_work *)work,
				struct mpu6050_sensor, fifo_flush_work);

	mpu6050_flush_fifo(sensor);
	mpu6050_sche_next_flush(sensor);

	return;
}

static enum hrtimer_restart gyro_timer_handle(struct hrtimer *hrtimer)
{
	struct mpu6050_sensor *sensor;
	ktime_t ktime;
	sensor = container_of(hrtimer, struct mpu6050_sensor, gyro_timer);
	ktime = ktime_set(0,
			sensor->gyro_poll_ms * NSEC_PER_MSEC);
	hrtimer_forward_now(&sensor->gyro_timer, ktime);
	sensor->gyro_wkp_flag = 1;
	wake_up_interruptible(&sensor->gyro_wq);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart accel_timer_handle(struct hrtimer *hrtimer)
{
	struct mpu6050_sensor *sensor;
	ktime_t ktime;
	sensor = container_of(hrtimer, struct mpu6050_sensor, accel_timer);
	ktime = ktime_set(0,
			sensor->accel_poll_ms * NSEC_PER_MSEC);
	hrtimer_forward_now(&sensor->accel_timer, ktime);
	sensor->accel_wkp_flag = 1;
	wake_up_interruptible(&sensor->accel_wq);
	return HRTIMER_RESTART;
}

static int gyro_poll_thread(void *data)
{
	struct mpu6050_sensor *sensor = data;
	u32 shift;
	ktime_t timestamp;

	while (1) {
		wait_event_interruptible(sensor->gyro_wq,
			((sensor->gyro_wkp_flag != 0) ||
				kthread_should_stop()));
		sensor->gyro_wkp_flag = 0;

		if (kthread_should_stop())
			break;

		mutex_lock(&sensor->op_lock);
		if (sensor->gyro_delay_change) {
			if (sensor->gyro_poll_ms <= POLL_MS_100HZ)
				set_wake_up_idle(true);
			else
				set_wake_up_idle(false);
			sensor->gyro_delay_change = false;
		}
		mutex_unlock(&sensor->op_lock);

		timestamp = ktime_get_boottime();
		mpu6050_read_gyro_data(sensor, &sensor->axis);
		mpu6050_remap_gyro_data(&sensor->axis, sensor->pdata->place);
		shift = mpu_gyro_fs_shift[sensor->cfg.fsr];
		input_report_abs(sensor->gyro_dev, ABS_RX,
			((sensor->axis.rx  - sensor->cali.rx) >> shift));
		input_report_abs(sensor->gyro_dev, ABS_RY,
			((sensor->axis.ry  - sensor->cali.ry) >> shift));
		input_report_abs(sensor->gyro_dev, ABS_RZ,
			((sensor->axis.rz  - sensor->cali.rz) >> shift));
		input_event(sensor->gyro_dev,
				EV_SYN, SYN_TIME_SEC,
				ktime_to_timespec(timestamp).tv_sec);
		input_event(sensor->gyro_dev, EV_SYN,
			SYN_TIME_NSEC,
			ktime_to_timespec(timestamp).tv_nsec);
		input_sync(sensor->gyro_dev);
	}
	return 0;
}

static int accel_poll_thread(void *data)
{
	struct mpu6050_sensor *sensor = data;
	u32 shift;
	ktime_t timestamp;

	while (1) {
		wait_event_interruptible(sensor->accel_wq,
			((sensor->accel_wkp_flag != 0) ||
				kthread_should_stop()));
		sensor->accel_wkp_flag = 0;

		if (kthread_should_stop())
			break;

		mutex_lock(&sensor->op_lock);
		if (sensor->accel_delay_change) {
			if (sensor->accel_poll_ms <= POLL_MS_100HZ)
				set_wake_up_idle(true);
			else
				set_wake_up_idle(false);
			sensor->accel_delay_change = false;
		}
		mutex_unlock(&sensor->op_lock);

		timestamp = ktime_get_boottime();
		mpu6050_acc_data_process(sensor);
		shift = mpu_accel_fs_shift[sensor->cfg.accel_fs];
		input_report_abs(sensor->accel_dev, ABS_X,
			((sensor->axis.x - sensor->cali.x) << shift));
		input_report_abs(sensor->accel_dev, ABS_Y,
			((sensor->axis.y - sensor->cali.y) << shift));
		input_report_abs(sensor->accel_dev, ABS_Z,
			((sensor->axis.z -  sensor->cali.z) << shift));
		input_event(sensor->accel_dev,
				EV_SYN, SYN_TIME_SEC,
				ktime_to_timespec(timestamp).tv_sec);
		input_event(sensor->accel_dev, EV_SYN,
			SYN_TIME_NSEC,
			ktime_to_timespec(timestamp).tv_nsec);
		input_sync(sensor->accel_dev);
	}

	return 0;
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
 * mpu6050_set_power_mode() - set the power mode
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

		sensor->cfg.enable = 1;
	} else {

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
 * mpu6050_restore_context() - update the sensor register context
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

	ret = i2c_smbus_write_byte_data(client, reg->sample_rate_div,
			sensor->cfg.rate_div);
	if (ret < 0) {
		dev_err(&client->dev, "set sample_rate_div failed.\n");
		goto exit;
	}

	ret = i2c_smbus_read_byte_data(client, reg->fifo_en);
	if (ret < 0) {
		dev_err(&client->dev, "read fifo_en failed.\n");
		goto exit;
	}

	data = (u8)ret;

	if (sensor->cfg.accel_fifo_enable)
		data |= BIT_ACCEL_FIFO;

	if (sensor->cfg.gyro_fifo_enable)
		data |= BIT_GYRO_FIFO;

	if (sensor->cfg.accel_fifo_enable || sensor->cfg.gyro_fifo_enable) {
		ret = i2c_smbus_write_byte_data(client, reg->fifo_en, data);
		if (ret < 0) {
			dev_err(&client->dev, "write fifo_en failed.\n");
			goto exit;
		}
	}

	if (sensor->cfg.cfg_fifo_en) {
		/* Assume DMP and external I2C is not in use*/
		ret = i2c_smbus_write_byte_data(client, reg->user_ctrl,
				BIT_FIFO_EN);
		if (ret < 0) {
			dev_err(&client->dev, "enable FIFO R/W failed.\n");
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
 * mpu6050_reset_chip() - reset chip to default state
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

static int mpu6050_gyro_batching_enable(struct mpu6050_sensor *sensor)
{
	int ret = 0;
	u32 latency;

	if (!sensor->batch_accel) {
		latency = sensor->gyro_latency_ms;
	} else {
		cancel_delayed_work_sync(&sensor->fifo_flush_work);
		if (sensor->accel_latency_ms < sensor->gyro_latency_ms)
			latency = sensor->accel_latency_ms;
		else
			latency = sensor->gyro_latency_ms;
	}
	ret = mpu6050_set_fifo(sensor, sensor->cfg.accel_enable, true);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to enable FIFO for gyro, ret=%d\n", ret);
		return ret;
	}

	if (sensor->use_poll) {
		queue_delayed_work(sensor->data_wq,
			&sensor->fifo_flush_work,
			msecs_to_jiffies(latency));
	} else if (!sensor->cfg.int_enabled) {
		mpu6050_set_interrupt(sensor, BIT_FIFO_OVERFLOW, true);
		enable_irq(sensor->client->irq);
		sensor->cfg.int_enabled = true;
	}

	return ret;
}

static int mpu6050_gyro_batching_disable(struct mpu6050_sensor *sensor)
{
	int ret = 0;
	u32 latency;

	ret = mpu6050_set_fifo(sensor, sensor->cfg.accel_enable, false);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to disable FIFO for accel, ret=%d\n", ret);
		return ret;
	}
	if (!sensor->use_poll) {
		if (sensor->cfg.int_enabled && !sensor->cfg.accel_enable) {
			mpu6050_set_interrupt(sensor,
				BIT_FIFO_OVERFLOW, false);
			disable_irq(sensor->client->irq);
			sensor->cfg.int_enabled = false;
		}
	} else {
		if (!sensor->batch_accel) {
			cancel_delayed_work_sync(&sensor->fifo_flush_work);
		} else if (sensor->gyro_latency_ms <
				sensor->accel_latency_ms) {
			cancel_delayed_work_sync(&sensor->fifo_flush_work);
			latency = sensor->accel_latency_ms;
			queue_delayed_work(sensor->data_wq,
				&sensor->fifo_flush_work,
				msecs_to_jiffies(latency));
		}
	}
	sensor->batch_gyro = false;

	return ret;
}

static int mpu6050_gyro_set_enable(struct mpu6050_sensor *sensor, bool enable)
{
	int ret = 0;

	dev_dbg(&sensor->client->dev,
		"mpu6050_gyro_set_enable enable=%d\n", enable);
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

		ret = mpu6050_config_sample_rate(sensor);
		if (ret < 0)
			dev_info(&sensor->client->dev,
				"Unable to update sampling rate! ret=%d\n",
				ret);

		if (sensor->batch_gyro) {
			ret = mpu6050_gyro_batching_enable(sensor);
			if (ret) {
				dev_err(&sensor->client->dev,
					"Fail to enable gyro batching =%d\n",
					ret);
				ret = -EBUSY;
				goto exit;
			}
		} else {
			ktime_t ktime;
			ktime = ktime_set(0,
					sensor->gyro_poll_ms * NSEC_PER_MSEC);
			hrtimer_start(&sensor->gyro_timer, ktime,
					HRTIMER_MODE_REL);
		}
		atomic_set(&sensor->gyro_en, 1);
	} else {
		atomic_set(&sensor->gyro_en, 0);
		if (sensor->batch_gyro) {
			ret = mpu6050_gyro_batching_disable(sensor);
			if (ret) {
				dev_err(&sensor->client->dev,
					"Fail to enable gyro batching =%d\n",
					ret);
				ret = -EBUSY;
				goto exit;
			}
		} else {
			hrtimer_cancel(&sensor->gyro_timer);
		}
		ret = mpu6050_gyro_enable(sensor, false);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to disable gyro engine ret=%d\n", ret);
			ret = -EBUSY;
			goto exit;
		}

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
	u8 div, saved_pwr;

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

	if (sensor->cfg.rate_div == div)
		return 0;

	ret = i2c_smbus_read_byte_data(sensor->client, sensor->reg.pwr_mgmt_1);
	if (ret < 0)
		goto err_exit;

	saved_pwr = (u8)ret;

	ret = i2c_smbus_write_byte_data(sensor->client, sensor->reg.pwr_mgmt_1,
		(saved_pwr & ~BIT_SLEEP));
	if (ret < 0)
		goto err_exit;

	ret = i2c_smbus_write_byte_data(sensor->client,
		sensor->reg.sample_rate_div, div);
	if (ret < 0)
		goto err_exit;

	ret = i2c_smbus_write_byte_data(sensor->client, sensor->reg.pwr_mgmt_1,
		saved_pwr);
	if (ret < 0)
		goto err_exit;

	sensor->cfg.rate_div = div;

	return 0;
err_exit:
	dev_err(&sensor->client->dev,
		"update sample div failed, div=%d, ret=%d\n",
		div, ret);
	return ret;
}

/*
 * Calculate sample interval according to sample rate.
 * Return sample interval in millisecond.
 */
static inline u64 mpu6050_get_sample_interval(struct mpu6050_sensor *sensor)
{
	u64 interval_ns;

	if ((sensor->cfg.lpf == MPU_DLPF_256HZ_NOLPF2) ||
		(sensor->cfg.lpf == MPU_DLPF_RESERVED)) {
		interval_ns = (sensor->cfg.rate_div + 1) * NSEC_PER_MSEC;
		interval_ns /= 8;
	} else {
		interval_ns = (sensor->cfg.rate_div + 1) * NSEC_PER_MSEC;
	}

	return interval_ns;
}

/**
 * mpu6050_flush_fifo() - flush fifo and send sensor event
 * @sensor: sensor device instance
 * Return 0 on success and returns a negative error code on failure.
 *
 * This function assumes only accel and gyro data will be stored into FIFO
 * and does not check FIFO enabling bits, if other sensor data is stored into
 * FIFO, it will cause confusion.
 */
static void mpu6050_flush_fifo(struct mpu6050_sensor *sensor)
{
	struct i2c_client *client = sensor->client;
	u64 interval_ns, ts_ns, sec;
	int ret, i, ns;
	u16 *buf, cnt;
	u8 shift;

	ret = mpu6050_read_reg(sensor->client, sensor->reg.fifo_count_h,
			(u8 *)&cnt, MPU6050_FIFO_CNT_SIZE);
	if (ret < 0) {
		dev_err(&client->dev, "read FIFO count failed, ret=%d\n", ret);
		return;
	}

	cnt = be16_to_cpu(cnt);
	dev_dbg(&client->dev, "Flush: FIFO count=%d\n", cnt);
	if (cnt == 0)
		return;
	if (cnt > MPU6050_FIFO_SIZE_BYTE || IS_ODD_NUMBER(cnt)) {
		dev_err(&client->dev, "Invalid FIFO count number %d\n", cnt);
		return;
	}

	interval_ns = mpu6050_get_sample_interval(sensor);
	dev_dbg(&client->dev, "interval_ns=%llu, fifo_start_ns=%llu\n",
		interval_ns, sensor->fifo_start_ns);
	ts_ns = sensor->fifo_start_ns + interval_ns;
	mpu6050_set_fifo_start_time(sensor);

	buf = kmalloc(cnt, GFP_KERNEL);
	if (!buf) {
		dev_err(&client->dev,
			"Allocate FIFO buffer error!\n");
		return;
	}

	ret = mpu6050_read_reg(sensor->client, sensor->reg.fifo_r_w,
			(u8 *)buf, cnt);
	if (ret < 0) {
		dev_err(&client->dev, "Read FIFO data error!\n");
		goto exit;
	}

	for (i = 0; i < (cnt >> 1); ts_ns += interval_ns) {
		if (sensor->cfg.accel_fifo_enable) {
			sensor->axis.x = be16_to_cpu(buf[i++]);
			sensor->axis.y = be16_to_cpu(buf[i++]);
			sensor->axis.z = be16_to_cpu(buf[i++]);
			sec = ts_ns;
			ns = do_div(sec, NSEC_PER_SEC);

			mpu6050_remap_accel_data(&sensor->axis,
				sensor->pdata->place);

			shift = mpu_accel_fs_shift[sensor->cfg.accel_fs];
			input_report_abs(sensor->accel_dev, ABS_X,
				(sensor->axis.x << shift));
			input_report_abs(sensor->accel_dev, ABS_Y,
				(sensor->axis.y << shift));
			input_report_abs(sensor->accel_dev, ABS_Z,
				(sensor->axis.z << shift));
			input_event(sensor->accel_dev,
				EV_SYN, SYN_TIME_SEC,
				(int)sec);
			input_event(sensor->accel_dev,
				EV_SYN, SYN_TIME_NSEC,
				(int)ns);
			input_sync(sensor->accel_dev);
		}

		if (sensor->cfg.gyro_fifo_enable) {
			sensor->axis.rx = be16_to_cpu(buf[i++]);
			sensor->axis.ry = be16_to_cpu(buf[i++]);
			sensor->axis.rz = be16_to_cpu(buf[i++]);
			sec = ts_ns;
			ns = do_div(sec, NSEC_PER_SEC);

			mpu6050_remap_gyro_data(&sensor->axis,
				sensor->pdata->place);

			shift = mpu_gyro_fs_shift[sensor->cfg.fsr];
			input_report_abs(sensor->gyro_dev, ABS_RX,
				(sensor->axis.rx >> shift));
			input_report_abs(sensor->gyro_dev, ABS_RY,
				(sensor->axis.ry >> shift));
			input_report_abs(sensor->gyro_dev, ABS_RZ,
				(sensor->axis.rz >> shift));
			input_event(sensor->gyro_dev,
				EV_SYN, SYN_TIME_SEC,
				(int)sec);
			input_event(sensor->gyro_dev,
				EV_SYN, SYN_TIME_NSEC,
				(int)ns);
			input_sync(sensor->gyro_dev);
		}
	}

exit:
	kfree(buf);
	return;
}

/**
 * mpu6050_set_fifo() - Configure and enable sensor FIFO
 * @sensor: sensor device instance
 * @en_accel: buffer accel event to fifo
 * @en_gyro: buffer gyro event to fifo
 * Return 0 on success and returns a negative error code on failure.
 *
 * This function will remove all existing FIFO setting and flush FIFO data,
 * new FIFO setting will be applied after that.
 */
static int mpu6050_set_fifo(struct mpu6050_sensor *sensor,
					bool en_accel, bool en_gyro)
{
	struct i2c_client *client = sensor->client;
	struct mpu_reg_map *reg = &sensor->reg;
	int ret;
	u8 en, user_ctl;

	en = FIFO_DISABLE_ALL;
	ret = i2c_smbus_write_byte_data(client,
			reg->fifo_en, en);
	if (ret < 0)
		goto err_exit;

	mpu6050_flush_fifo(sensor);

	/* Enable sensor output to FIFO */
	if (en_accel)
		en |= BIT_ACCEL_FIFO;

	if (en_gyro)
		en |= BIT_GYRO_FIFO;

	ret = i2c_smbus_write_byte_data(client,
			reg->fifo_en, en);
	if (ret < 0)
		goto err_exit;

	/* Enable/Disable FIFO RW*/
	ret = i2c_smbus_read_byte_data(client,
			reg->user_ctrl);
	if (ret < 0)
		goto err_exit;

	user_ctl = (u8)ret;
	if (en_accel | en_gyro) {
		user_ctl |= BIT_FIFO_EN;
		sensor->cfg.cfg_fifo_en = true;
	} else {
		user_ctl &= ~BIT_FIFO_EN;
		sensor->cfg.cfg_fifo_en = false;
	}

	ret = i2c_smbus_write_byte_data(client,
			reg->user_ctrl, user_ctl);
	if (ret < 0)
		goto err_exit;

	mpu6050_set_fifo_start_time(sensor);
	sensor->cfg.accel_fifo_enable = en_accel;
	sensor->cfg.gyro_fifo_enable = en_gyro;

	return 0;

err_exit:
	dev_err(&client->dev, "Set fifo failed, ret=%d\n", ret);
	return ret;
}

static int mpu6050_gyro_set_poll_delay(struct mpu6050_sensor *sensor,
					unsigned long delay)
{
	int ret;

	dev_dbg(&sensor->client->dev,
		"mpu6050_gyro_set_poll_delay delay=%ld\n", delay);
	if (delay < MPU6050_GYRO_MIN_POLL_INTERVAL_MS)
		delay = MPU6050_GYRO_MIN_POLL_INTERVAL_MS;
	if (delay > MPU6050_GYRO_MAX_POLL_INTERVAL_MS)
		delay = MPU6050_GYRO_MAX_POLL_INTERVAL_MS;

	mutex_lock(&sensor->op_lock);
	if (sensor->gyro_poll_ms == delay)
		goto exit;

	sensor->gyro_delay_change = true;
	sensor->gyro_poll_ms = delay;

	if (!atomic_read(&sensor->gyro_en))
		goto exit;

	if (sensor->use_poll) {
		ktime_t ktime;
		hrtimer_cancel(&sensor->gyro_timer);
		ktime = ktime_set(0,
				sensor->gyro_poll_ms * NSEC_PER_MSEC);
		hrtimer_start(&sensor->gyro_timer, ktime, HRTIMER_MODE_REL);

	} else {
		ret = mpu6050_config_sample_rate(sensor);
		if (ret < 0)
			dev_err(&sensor->client->dev,
				"Unable to set polling delay for gyro!\n");
	}

exit:
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

static int mpu6050_gyro_cdev_flush(struct sensors_classdev *sensors_cdev)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, gyro_cdev);

	mutex_lock(&sensor->op_lock);
	mpu6050_flush_fifo(sensor);
	input_event(sensor->gyro_dev,
		EV_SYN, SYN_CONFIG, sensor->flush_count++);
	input_sync(sensor->gyro_dev);
	mutex_unlock(&sensor->op_lock);
	return 0;
}

static int mpu6050_gyro_cdev_set_latency(struct sensors_classdev *sensors_cdev,
					unsigned int max_latency)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, gyro_cdev);

	mutex_lock(&sensor->op_lock);
	if (max_latency <= sensor->gyro_poll_ms)
		sensor->batch_gyro = false;
	else
		sensor->batch_gyro = true;

	sensor->gyro_latency_ms = max_latency;
	mutex_unlock(&sensor->op_lock);
	return 0;
}

/**
 * mpu6050_gyro_attr_get_polling_delay() - get the sampling rate
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
 * mpu6050_gyro_attr_set_polling_delay() - set the sampling rate
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

static int MPU6050_SetPowerMode(struct i2c_client *iclient)
{
	int i = 0;
	u8  temp_data;
	struct mpu6050_sensor *self_info = mpu_info;
	for (; i <= 107; i++) {
		mpu6050_read_reg(self_info->client, i, &temp_data, 1);
		wing_info("wlg_set_self_test----read 0x%d, %X\n", i, temp_data);
	}
	mpu6050_write_reg(self_info->client, 0X38, 0X00, 1);
	mpu6050_write_reg(self_info->client, 0X23, 0X00, 1);
	mpu6050_write_reg(self_info->client, 0X6A, 0X00, 1);
	mpu6050_write_reg(self_info->client, 0X6A, 0X04, 1);
	mpu6050_write_reg(self_info->client, 0X1A, 0X02, 1);
	mpu6050_write_reg(self_info->client, 0X1D, 0X02, 1);
	mpu6050_write_reg(self_info->client, 0X19, 0X00, 1);
	mpu6050_write_reg(self_info->client, 0X1B, 0X00, 1);
	mpu6050_write_reg(self_info->client, 0X1C, 0X00, 1);
	mpu6050_write_reg(self_info->client, 0X6A, 0X40, 1);
	mpu6050_write_reg(self_info->client, 0X6B, 0X0, 1);
	mpu6050_write_reg(self_info->client, 0X6C, 0X0, 1);
	mpu6050_write_reg(self_info->client, 0X23, 0X78, 1);
	mpu6050_write_reg(self_info->client, 0X23, 0X00, 1);


	return 0;

}

static int MPU6050_JudgeTestResult(struct i2c_client *client, s32 prv[MPU6050_AXES_NUM], s32 nxt[MPU6050_AXES_NUM])
{
	struct criteria {
		int min;
		int max;
	};



	struct criteria gyro_offset[4][3] = {

		{{1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_250)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_250)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_250)} },
		{{1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_500)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_500)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_500)} },
		{{1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_1000)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_1000)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_1000)} },
		{{1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_2000)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_2000)}, {1000, (DEF_GYRO_OFFSET_MAX * DEF_SELFTEST_GYRO_SENS_2000)} },
	};
	struct criteria (*ptr)[3] = NULL;
	u8 format;
	int res;


	if ((res = mpu6050_read_reg(client, MPU6050_REG_Gyro_DATA_FORMAT, &format, 1)) < 0) {
		return res;
	} else {
		res = 0;
	}

	format = format & MPU6050_RANGE_PN2000dps;

	switch (format) {
	case MPU6050_RANGE_PN250dps:
		wing_info("format use gyro_offset[0]\n");
		ptr = &gyro_offset[0];
		break;

	case MPU6050_RANGE_PN500dps:
		wing_info("format use gyro_offset[1]\n");
		ptr = &gyro_offset[1];
		break;

	case MPU6050_RANGE_PN1000dps:
		wing_info("format use gyro_offset[2]\n");
		ptr = &gyro_offset[2];
		break;

	case MPU6050_RANGE_PN2000dps:
		wing_info("format use gyro_offset[3]\n");
		ptr = &gyro_offset[3];
		break;

	default:
		wing_info("format unknow use \n");
		break;
	}

	if (!ptr) {
		wing_info("null pointer\n");
		return -EINVAL;
	}
	wing_info("format=0x%x\n", format);

	wing_info("X diff is %ld\n", abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]));
	wing_info("Y diff is %ld\n", abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]));
	wing_info("Z diff is %ld\n", abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]));

	if (abs(prv[MPU6050_AXIS_X]) > (*ptr)[MPU6050_AXIS_X].max) {
		wing_info("gyro X offset[%X] is over range\n", prv[MPU6050_AXIS_X]);
		res = -EINVAL;
	}

	if (abs(prv[MPU6050_AXIS_Y]) > (*ptr)[MPU6050_AXIS_Y].max) {
		wing_info("gyro Y offset[%X] is over range\n", prv[MPU6050_AXIS_Y]);
		res = -EINVAL;
	}

	if (abs(prv[MPU6050_AXIS_Z]) > (*ptr)[MPU6050_AXIS_Z].max) {
		wing_info("gyro Z offset[%X] is over range\n", prv[MPU6050_AXIS_Z]);
		res = -EINVAL;
	}
#if 1

	if ((abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]) < (*ptr)[MPU6050_AXIS_X].min)) {
		wing_info("X is out of work\n");
		res = -EINVAL;
	}
	if ((abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]) < (*ptr)[MPU6050_AXIS_Y].min)) {
		wing_info("Y is out of work\n");
		res = -EINVAL;
	}
	if ((abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]) < (*ptr)[MPU6050_AXIS_Z].min)) {
		wing_info("Z is out of work\n");
		res = -EINVAL;
	}

#endif
	return res;
}

static ssize_t show_self_value(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mpu6050_sensor *self_info = mpu_info;


	if (NULL == self_info) {
		wing_info("show_self_value is null!!\n");
		return 0;
	}

	return snprintf(buf, 8, "%s\n", selftestRes);
}

/**
 * mpu6050_gyro_attr_set_enable -
 *	Set/get enable function is just needed by sensor HAL.
 */
static ssize_t store_self_value(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	u8 temp_data;
	struct axis_data *self_data;
	struct axis_data *self_data2;
	struct mpu6050_sensor *self_info = mpu_info;
	int idx, num;
	int ret = 0;

	s32 avg_prv[MPU6050_AXES_NUM] = {0, 0, 0};
	s32 avg_nxt[MPU6050_AXES_NUM] = {0, 0, 0};

	if (1 != sscanf(buf, "%d", &num)) {
		wing_info("parse number fail\n");
		return count;
	} else if (num == 0) {
		wing_info("invalid data count\n");
		return count;
	}
	self_data = kzalloc(sizeof(*self_data) * num, GFP_KERNEL);
	self_data2 = kzalloc(sizeof(*self_data2) * num, GFP_KERNEL);
	if (!self_data || !self_data2) {
		goto exit;
	}

	wing_info("NORMAL:\n");
	MPU6050_SetPowerMode(self_info->client);
	msleep(50);

	for (idx = 0; idx < num; idx++) {

		mpu6050_read_gyro_data(self_info, self_data);

		wing_info("x = %d, y = %d, z= %d\n", self_data->rx, self_data->ry, self_data->rz);
		avg_prv[MPU6050_AXIS_X] += self_data->rx;
		avg_prv[MPU6050_AXIS_Y] += self_data->ry;
		avg_prv[MPU6050_AXIS_Z] += self_data->rz;
		wing_info("[%5d %5d %5d]\n", self_data->rx, self_data->ry, self_data->rz);
	}

	avg_prv[MPU6050_AXIS_X] /= num;
	avg_prv[MPU6050_AXIS_Y] /= num;
	avg_prv[MPU6050_AXIS_Z] /= num;

	/*initial setting for self test*/
	wing_info("SELFTEST:\n");

	mpu6050_read_reg(self_info->client, 0X1B, &temp_data, 1);
	wing_info("wlg_set_self_test----read 0x1B	%d\n", temp_data);
	temp_data |= 0xE0;
	ret = i2c_smbus_write_byte_data(self_info->client,
			0X1B, temp_data);
	if (ret < 0)
		return ret;

	msleep(50);

	for (idx = 0; idx < num; idx++) {

		mpu6050_read_gyro_data(self_info, self_data2);

		wing_info("xx = %d, yy = %d, zz= %d\n", self_data2->rx, self_data2->ry, self_data2->rz);
		avg_nxt[MPU6050_AXIS_X] += self_data2->rx;
		avg_nxt[MPU6050_AXIS_Y] += self_data2->ry;
		avg_nxt[MPU6050_AXIS_Z] += self_data2->rz;
		wing_info("[%5d %5d %5d]\n", self_data2->rx, self_data2->ry, self_data2->rz);
	}

	avg_nxt[MPU6050_AXIS_X] /= num;
	avg_nxt[MPU6050_AXIS_Y] /= num;
	avg_nxt[MPU6050_AXIS_Z] /= num;

	wing_info("X: %5d - %5d = %5d \n", avg_nxt[MPU6050_AXIS_X], avg_prv[MPU6050_AXIS_X], avg_nxt[MPU6050_AXIS_X] - avg_prv[MPU6050_AXIS_X]);
	wing_info("Y: %5d - %5d = %5d \n", avg_nxt[MPU6050_AXIS_Y], avg_prv[MPU6050_AXIS_Y], avg_nxt[MPU6050_AXIS_Y] - avg_prv[MPU6050_AXIS_Y]);
	wing_info("Z: %5d - %5d = %5d \n", avg_nxt[MPU6050_AXIS_Z], avg_prv[MPU6050_AXIS_Z], avg_nxt[MPU6050_AXIS_Z] - avg_prv[MPU6050_AXIS_Z]);

	if (!MPU6050_JudgeTestResult(self_info->client, avg_prv, avg_nxt)) {
		wing_info("SELFTEST : PASS\n");
		strcpy(selftestRes, "y");
	} else {
		wing_info("SELFTEST : FAIL\n");
		strcpy(selftestRes, "n");
	}

	exit:

	mpu6050_read_reg(self_info->client, 0X1B, &temp_data, 1);
	temp_data &= 0x1F;
	ret = i2c_smbus_write_byte_data(self_info->client,
			0X1B, temp_data);
	if (ret < 0)
		return ret;

	/*restore the setting*/
	kfree(self_data);
	kfree(self_data2);
	ret = mpu6050_init_config(self_info);
	if (ret) {
		dev_err(self_info->dev, "Failed to set default config\n");
		return ret;
	}
	mpu6050_gyro_enable(self_info, true);
	return count;
}

static ssize_t mpu6050_gyro_attr_get_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mpu6050_sensor *sensor = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sensor->cfg.gyro_enable);
}

/**
 * mpu6050_gyro_attr_set_enable() -
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
static struct device_attribute gyro_self_attr[] = {
	__ATTR(selftest, S_IRUGO | S_IWUSR, show_self_value , store_self_value),
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

		sensor->cfg.enable = 1;
	} else {
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

static int mpu6050_accel_batching_enable(struct mpu6050_sensor *sensor)
{
	int ret = 0;
	u32 latency;

	if (!sensor->batch_gyro) {
		latency = sensor->accel_latency_ms;
	} else {
		cancel_delayed_work_sync(&sensor->fifo_flush_work);
		if (sensor->accel_latency_ms < sensor->gyro_latency_ms)
			latency = sensor->accel_latency_ms;
		else
			latency = sensor->gyro_latency_ms;
	}

	ret = mpu6050_set_fifo(sensor, true, sensor->cfg.gyro_enable);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to enable FIFO for accel, ret=%d\n", ret);
		return ret;
	}

	if (sensor->use_poll) {
		queue_delayed_work(sensor->data_wq,
			&sensor->fifo_flush_work,
			msecs_to_jiffies(latency));
	} else if (!sensor->cfg.int_enabled) {
		mpu6050_set_interrupt(sensor, BIT_FIFO_OVERFLOW, true);
		enable_irq(sensor->client->irq);
		sensor->cfg.int_enabled = true;
	}

	return ret;
}

static int mpu6050_accel_batching_disable(struct mpu6050_sensor *sensor)
{
	int ret = 0;
	u32 latency;

	ret = mpu6050_set_fifo(sensor, false, sensor->cfg.gyro_enable);
	if (ret < 0) {
		dev_err(&sensor->client->dev,
			"Fail to disable FIFO for accel, ret=%d\n", ret);
		return ret;
	}
	if (!sensor->use_poll) {
		if (sensor->cfg.int_enabled && !sensor->cfg.gyro_enable) {
			mpu6050_set_interrupt(sensor,
				BIT_FIFO_OVERFLOW, false);
			disable_irq(sensor->client->irq);
			sensor->cfg.int_enabled = false;
		}
	} else {
		if (!sensor->batch_gyro) {
			cancel_delayed_work_sync(&sensor->fifo_flush_work);
		} else if (sensor->accel_latency_ms <
				sensor->gyro_latency_ms) {
			cancel_delayed_work_sync(&sensor->fifo_flush_work);
			latency = sensor->gyro_latency_ms;
			queue_delayed_work(sensor->data_wq,
				&sensor->fifo_flush_work,
				msecs_to_jiffies(latency));
		}
	}
	sensor->batch_accel = false;

	return ret;
}

static int mpu6050_accel_set_enable(struct mpu6050_sensor *sensor, bool enable)
{
	int ret = 0;

	dev_dbg(&sensor->client->dev,
		"mpu6050_accel_set_enable enable=%d\n", enable);
	if (enable) {
		if (!sensor->power_enabled) {
			ret = mpu6050_power_ctl(sensor, true);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Failed to set power up mpu6050");
				return ret;
			}

			ret = mpu6050_restore_context(sensor);
			if (ret < 0) {
				dev_err(&sensor->client->dev,
					"Failed to restore context");
				return ret;
			}
		}

		ret = mpu6050_accel_enable(sensor, true);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to enable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			return ret;
		}

		ret = mpu6050_config_sample_rate(sensor);
		if (ret < 0)
			dev_info(&sensor->client->dev,
				"Unable to update sampling rate! ret=%d\n",
				ret);

		if (sensor->batch_accel) {
			ret = mpu6050_accel_batching_enable(sensor);
			if (ret) {
				dev_err(&sensor->client->dev,
					"Fail to enable accel batching =%d\n",
					ret);
				ret = -EBUSY;
				return ret;
			}
		} else {
			ktime_t ktime;
			ktime = ktime_set(0,
					sensor->accel_poll_ms * NSEC_PER_MSEC);
			hrtimer_start(&sensor->accel_timer, ktime,
					HRTIMER_MODE_REL);
		}
		atomic_set(&sensor->accel_en, 1);
	} else {
		atomic_set(&sensor->accel_en, 0);
		if (sensor->batch_accel) {
			ret = mpu6050_accel_batching_disable(sensor);
			if (ret) {
				dev_err(&sensor->client->dev,
					"Fail to disable accel batching =%d\n",
					ret);
				ret = -EBUSY;
				return ret;
			}
		} else {
			hrtimer_cancel(&sensor->accel_timer);
		}

		ret = mpu6050_accel_enable(sensor, false);
		if (ret) {
			dev_err(&sensor->client->dev,
				"Fail to disable accel engine ret=%d\n", ret);
			ret = -EBUSY;
			return ret;
		}

	}

	return ret;
}

static int mpu6050_accel_set_poll_delay(struct mpu6050_sensor *sensor,
					unsigned long delay)
{
	int ret;

	dev_dbg(&sensor->client->dev,
		"mpu6050_accel_set_poll_delay delay_ms=%ld\n", delay);
	if (delay < MPU6050_ACCEL_MIN_POLL_INTERVAL_MS)
		delay = MPU6050_ACCEL_MIN_POLL_INTERVAL_MS;
	if (delay > MPU6050_ACCEL_MAX_POLL_INTERVAL_MS)
		delay = MPU6050_ACCEL_MAX_POLL_INTERVAL_MS;

	mutex_lock(&sensor->op_lock);
	if (sensor->accel_poll_ms == delay)
		goto exit;

	sensor->accel_delay_change = true;
	sensor->accel_poll_ms = delay;

	if (!atomic_read(&sensor->accel_en))
		goto exit;


	if (sensor->use_poll) {
		ktime_t ktime;
		hrtimer_cancel(&sensor->accel_timer);
		ktime = ktime_set(0,
				sensor->accel_poll_ms * NSEC_PER_MSEC);
		hrtimer_start(&sensor->accel_timer, ktime, HRTIMER_MODE_REL);
	} else {
		ret = mpu6050_config_sample_rate(sensor);
		if (ret < 0)
			dev_err(&sensor->client->dev,
				"Unable to set polling delay for accel!\n");
	}

exit:
	mutex_unlock(&sensor->op_lock);
	return 0;
}

static int mpu6050_accel_cdev_enable(struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);
	int err;

	mutex_lock(&sensor->op_lock);

	err = mpu6050_accel_set_enable(sensor, enable);

	mutex_unlock(&sensor->op_lock);

	return err;
}

static int mpu6050_accel_cdev_poll_delay(struct sensors_classdev *sensors_cdev,
			unsigned int delay_ms)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	return mpu6050_accel_set_poll_delay(sensor, delay_ms);
}

static int mpu6050_accel_cdev_flush(struct sensors_classdev *sensors_cdev)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	mutex_lock(&sensor->op_lock);
	mpu6050_flush_fifo(sensor);
	mutex_unlock(&sensor->op_lock);
	input_event(sensor->accel_dev,
		EV_SYN, SYN_CONFIG, sensor->flush_count++);
	input_sync(sensor->accel_dev);
	return 0;
}

static int mpu6050_accel_cdev_set_latency(struct sensors_classdev *sensors_cdev,
					unsigned int max_latency)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	mutex_lock(&sensor->op_lock);
	if (max_latency <= sensor->accel_poll_ms) {
		sensor->batch_accel = false;
	} else {
		sensor->batch_accel = true;
	}
	sensor->accel_latency_ms = max_latency;
	mutex_unlock(&sensor->op_lock);
	return 0;
}

static int mpu6050_accel_cdev_enable_wakeup(
			struct sensors_classdev *sensors_cdev,
			unsigned int enable)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	if (sensor->use_poll)
		return -ENODEV;

	sensor->motion_det_en = enable;
	return 0;
}

static int mpu6050_accel_calibration(struct sensors_classdev *sensors_cdev,
		int axis, int apply_now)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);
	int ret;
	bool pre_enable;
	int arry[3] = { 0 };
	int i, delay_ms;

	mutex_lock(&sensor->op_lock);

	if (axis < AXIS_X && axis > AXIS_XYZ) {
		dev_err(&sensor->client->dev,
				"accel calibration cmd error\n");
		ret = -EINVAL;
		goto exit;
	}

	pre_enable = sensor->cfg.accel_enable;
	if (pre_enable)
		mpu6050_accel_set_enable(sensor, false);

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

	delay_ms = sensor->accel_poll_ms;
	sensor->accel_poll_ms = MPU6050_ACCEL_MIN_POLL_INTERVAL_MS;
	ret = mpu6050_config_sample_rate(sensor);
	if (ret < 0)
		dev_info(&sensor->client->dev,
				"Unable to update sampling rate! ret=%d\n",
				ret);

	ret = mpu6050_accel_enable(sensor, true);
	if (ret) {
		dev_err(&sensor->client->dev,
				"Fail to enable accel engine ret=%d\n", ret);
		ret = -EBUSY;
		goto exit;
	}

	for (i = 0; i < MPU_ACC_CAL_COUNT; i++) {
		msleep(MPU_ACC_CAL_DELAY);
		mpu6050_read_accel_data(sensor, &sensor->axis);
		if (i < CAL_SKIP_COUNT)
			continue;
		mpu6050_remap_accel_data(&sensor->axis, sensor->pdata->place);
		arry[0] += sensor->axis.x;
		arry[1] += sensor->axis.y;
		arry[2] += sensor->axis.z;
	}

	arry[0] = arry[0] / (MPU_ACC_CAL_NUM);
	arry[1] = arry[1] / (MPU_ACC_CAL_NUM);
	arry[2] = arry[2] / (MPU_ACC_CAL_NUM);

	switch (axis) {
	case AXIS_X:
		arry[1] = 0;
		arry[2] = 0;
		break;
	case AXIS_Y:
		arry[0] = 0;
		arry[2] = 0;
		break;
	case AXIS_Z:
		arry[0] = 0;
		arry[1] = 0;
		arry[2] -= RAW_TO_1G;
		break;
	case AXIS_XYZ:
		arry[2] -= RAW_TO_1G;
		break;
	default:
		dev_err(&sensor->client->dev,
				"calibrate mpu6050 accel CMD error\n");
		ret = -EINVAL;
		goto exit;
	}

	if (apply_now) {
		sensor->acc_cal_params[0] = arry[0];
		sensor->acc_cal_params[1] = arry[1];
		sensor->acc_cal_params[2] = arry[2];
		sensor->acc_use_cal = true;
	}
	snprintf(sensor->acc_cal_buf, sizeof(sensor->acc_cal_buf),
			"%d,%d,%d", arry[0], arry[1], arry[2]);
	sensors_cdev->params = sensor->acc_cal_buf;

	sensor->accel_poll_ms = delay_ms;
	ret = mpu6050_config_sample_rate(sensor);
	if (ret < 0)
		dev_info(&sensor->client->dev,
				"Unable to update sampling rate! ret=%d\n",
				ret);

	ret = mpu6050_accel_enable(sensor, false);
	if (ret) {
		dev_err(&sensor->client->dev,
				"Fail to disable accel engine ret=%d\n", ret);
		ret = -EBUSY;
		goto exit;
	}
	if (pre_enable)
		mpu6050_accel_set_enable(sensor, true);

exit:
	mutex_unlock(&sensor->op_lock);
	return ret;
}

static int mpu6050_write_accel_cal_params(struct sensors_classdev *sensors_cdev,
		struct cal_result_t *cal_result)
{
	struct mpu6050_sensor *sensor = container_of(sensors_cdev,
			struct mpu6050_sensor, accel_cdev);

	mutex_lock(&sensor->op_lock);

	sensor->acc_cal_params[0] = cal_result->offset_x;
	sensor->acc_cal_params[1] = cal_result->offset_y;
	sensor->acc_cal_params[2] = cal_result->offset_z;
	snprintf(sensor->acc_cal_buf, sizeof(sensor->acc_cal_buf),
			"%d,%d,%d", sensor->acc_cal_params[0],
			sensor->acc_cal_params[1], sensor->acc_cal_params[2]);
	sensors_cdev->params = sensor->acc_cal_buf;
	sensor->acc_use_cal = true;

	mutex_unlock(&sensor->op_lock);
	return 0;
}

static void mpu6050_acc_data_process(struct mpu6050_sensor *sensor)
{
	mpu6050_read_accel_data(sensor, &sensor->axis);
	mpu6050_remap_accel_data(&sensor->axis, sensor->pdata->place);
	if (sensor->acc_use_cal) {
		sensor->axis.x -= sensor->acc_cal_params[0];
		sensor->axis.y -= sensor->acc_cal_params[1];
		sensor->axis.z -= sensor->acc_cal_params[2];
	}
}

/**
 * mpu6050_accel_attr_get_polling_delay() - get the sampling rate
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
 * mpu6050_accel_attr_set_polling_delay() - set the sampling rate
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
 * mpu6050_accel_attr_set_enable() -
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
	reg->user_ctrl		= REG_USER_CTRL;
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



/* GS open fops */
static int gs_open(struct inode *inode, struct file *file)
{

	file->private_data = mpu_info;
	return nonseekable_open(inode, file);
}

/* GS release fops */
static int gs_release(struct inode *inode, struct file *file)
{


	return 0;
}

/* GS IOCTL */
static long gs_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	int rc = 0;
	void __user *argp = (void __user *)arg;
	struct mpu6050_sensor *sensor = file->private_data;

	struct cali_data rawdata;
	struct cali_data calidata;

	switch (cmd) {

	case GS_REC_DATA_FOR_PER:
		if (copy_from_user(&calidata, argp, sizeof(calidata)))
			return -EFAULT;
		if (calidata.x < MPU6050_ACCEL_MIN_VALUE || calidata.x > MPU6050_ACCEL_MAX_VALUE)
			calidata.x = 0;
		if (calidata.y < MPU6050_ACCEL_MIN_VALUE || calidata.y > MPU6050_ACCEL_MAX_VALUE)
			calidata.y = 0;
		if (calidata.z < MPU6050_ACCEL_MIN_VALUE || calidata.z > MPU6050_ACCEL_MAX_VALUE)
			calidata.z = 0;
		sensor->cali.x = calidata.x;
		sensor->cali.y = calidata.y;
		sensor->cali.z = calidata.z;
		printk("xmm gsensor nv cali x=%d, y=%d, z=%d\n", sensor->cali.x, sensor->cali.y, sensor->cali.z);
		break;
	case GS_GET_RAW_DATA_FOR_CALI:
		rawdata.x = sensor->axis.x;
		rawdata.y = sensor->axis.y;
		rawdata.z = sensor->axis.z;
		rawdata.offset = 16384;
		printk("xmm gsensor fastmmi read x=%d, y=%d, z=%d\n", rawdata.x, rawdata.y, rawdata.z);
		if (copy_to_user(argp, &rawdata, sizeof(rawdata))) {
			dev_err(&sensor->client->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;

	default:
		pr_err("%s: INVALID COMMAND %d\n",
				__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations gs_fops = {
	.owner = THIS_MODULE,
	.open = gs_open,
	.release = gs_release,
	.unlocked_ioctl = gs_ioctl
};

static struct miscdevice gs_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &gs_fops
};

# if 1
static int gyro_open(struct inode *inode, struct file *file)
{

	file->private_data = mpu_info;
	return nonseekable_open(inode, file);
}


/* GS release fops */
static int gyro_release(struct inode *inode, struct file *file)
{


	return 0;
}

/* GS IOCTL */
static long gyro_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	int rc = 0;

	void __user *argp = (void __user *)arg;
	struct mpu6050_sensor *sensor = file->private_data;
	struct cali_data gyrorawdata;
	struct cali_data calidata;

	switch (cmd) {

	case GYRO_REC_DATA_FOR_CALI:

		if (copy_from_user(&calidata, argp, sizeof(calidata)))
			return -EFAULT;
		if (calidata.rx < MPU6050_GYRO_MIN_VALUE || calidata.rx > MPU6050_GYRO_MAX_VALUE)
			calidata.rx = 0;
		if (calidata.ry < MPU6050_GYRO_MIN_VALUE || calidata.ry > MPU6050_GYRO_MAX_VALUE)
			calidata.ry = 0;
		if (calidata.rz < MPU6050_GYRO_MIN_VALUE || calidata.rz > MPU6050_GYRO_MAX_VALUE)
			calidata.rz = 0;
		sensor->cali.rx = calidata.rx;
		sensor->cali.ry = calidata.ry;
		sensor->cali.rz = calidata.rz;
		printk("xmm gyro nv cali x=%d, y=%d, z=%d\n", sensor->cali.rx, sensor->cali.ry, sensor->cali.rz);
		break;

	case GYRO_GET_RAW_DATA_FOR_CALI:
		gyrorawdata.rx = sensor->axis.rx;
		gyrorawdata.ry = sensor->axis.ry;
		gyrorawdata.rz = sensor->axis.rz;
		gyrorawdata.roffset = 938;

		if (g_has_initconfig == 0) {
			g_has_initconfig = 1;
			mpu6050_restore_context(mpu_info);
			msleep(20);
		}

		printk("xmm gyro fastmmi read x=%d, y=%d, z=%d\n", gyrorawdata.rx, gyrorawdata.ry, gyrorawdata.rz);
		if (copy_to_user(argp, &gyrorawdata, sizeof(gyrorawdata))) {
			dev_err(&sensor->client->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		break;

	default:
		pr_err("%s: INVALID COMMAND %d\n",
				__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations gyro_fops = {
	.owner = THIS_MODULE,
	.open = gyro_open,
	.release = gyro_release,
	.unlocked_ioctl = gyro_ioctl
};

static struct miscdevice gyro_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyro",
	.fops = &gyro_fops
};

#endif

/**
 * mpu6050_probe() - device detection callback
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

#ifdef GYRO_DATA_FILTER
	memset(&gyro_fir, 0, sizeof(gyro_fir));
	gyro_fir.firlen = 12;
#endif

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

	mpu_info = sensor;

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
	atomic_set(&sensor->accel_en, 0);
	atomic_set(&sensor->gyro_en, 0);
	ret = mpu6050_init_config(sensor);
	if (ret) {
		dev_err(&client->dev, "Failed to set default config\n");
		goto err_power_off_device;
	}

	sensor->accel_dev = devm_input_allocate_device(&client->dev);
	if (!sensor->accel_dev) {
		dev_err(&client->dev,
			"Failed to allocate accelerometer input device\n");
		ret = -ENOMEM;
		goto err_power_off_device;
	}

	sensor->gyro_dev = devm_input_allocate_device(&client->dev);
	if (!sensor->gyro_dev) {
		dev_err(&client->dev,
			"Failed to allocate gyroscope input device\n");
		ret = -ENOMEM;
		goto err_power_off_device;
	}

	sensor->accel_dev->name = MPU6050_DEV_NAME_ACCEL;
	sensor->gyro_dev->name = MPU6050_DEV_NAME_GYRO;
	sensor->accel_dev->id.bustype = BUS_I2C;
	sensor->gyro_dev->id.bustype = BUS_I2C;
	sensor->accel_poll_ms = MPU6050_ACCEL_DEFAULT_POLL_INTERVAL_MS;
	sensor->gyro_poll_ms = MPU6050_GYRO_DEFAULT_POLL_INTERVAL_MS;
	sensor->acc_use_cal = false;

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
			goto err_power_off_device;
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
		dev_dbg(&client->dev,
			"Polling mode is enabled. use_int=%d gpio_int=%d",
			sensor->pdata->use_int, sensor->pdata->gpio_int);
	}
	sensor->data_wq = create_freezable_workqueue("mpu6050_data_work");
	if (!sensor->data_wq) {
		dev_err(&client->dev, "Cannot create workqueue!\n");
		goto err_free_gpio;
	}

	INIT_DELAYED_WORK(&sensor->fifo_flush_work, mpu6050_fifo_flush_fn);

	hrtimer_init(&sensor->gyro_timer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	sensor->gyro_timer.function = gyro_timer_handle;
	hrtimer_init(&sensor->accel_timer, CLOCK_BOOTTIME, HRTIMER_MODE_REL);
	sensor->accel_timer.function = accel_timer_handle;

	init_waitqueue_head(&sensor->gyro_wq);
	init_waitqueue_head(&sensor->accel_wq);
	sensor->gyro_wkp_flag = 0;
	sensor->accel_wkp_flag = 0;

	sensor->gyr_task = kthread_run(gyro_poll_thread, sensor, "sns_gyro");
	sensor->accel_task = kthread_run(accel_poll_thread, sensor,
						"sns_accel");

	ret = input_register_device(sensor->accel_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_destroy_workqueue;
	}
	ret = input_register_device(sensor->gyro_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_destroy_workqueue;
	}

	ret = create_accel_sysfs_interfaces(&sensor->accel_dev->dev);
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs for accel\n");
		goto err_destroy_workqueue;
	}
	ret = create_gyro_sysfs_interfaces(&sensor->gyro_dev->dev);
	device_create_file(&sensor->gyro_dev->dev, gyro_self_attr);
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
	sensor->accel_cdev.fifo_reserved_event_count = 0;
	sensor->accel_cdev.sensors_set_latency = mpu6050_accel_cdev_set_latency;
	sensor->accel_cdev.sensors_flush = mpu6050_accel_cdev_flush;
	sensor->accel_cdev.sensors_calibrate = mpu6050_accel_calibration;
	sensor->accel_cdev.sensors_write_cal_params =
		mpu6050_write_accel_cal_params;
	if ((sensor->pdata->use_int) &&
			gpio_is_valid(sensor->pdata->gpio_int))
		sensor->accel_cdev.max_delay = MPU6050_ACCEL_INT_MAX_DELAY;

	ret = sensors_classdev_register(&sensor->accel_dev->dev,
			&sensor->accel_cdev);
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
	sensor->gyro_cdev.fifo_reserved_event_count = 0;
	sensor->gyro_cdev.sensors_set_latency = mpu6050_gyro_cdev_set_latency;
	sensor->gyro_cdev.sensors_flush = mpu6050_gyro_cdev_flush;
	if ((sensor->pdata->use_int) &&
			gpio_is_valid(sensor->pdata->gpio_int))
		sensor->gyro_cdev.max_delay = MPU6050_GYRO_INT_MAX_DELAY;

	ret = sensors_classdev_register(&sensor->gyro_dev->dev,
			&sensor->gyro_cdev);
	if (ret) {
		dev_err(&client->dev,
			"create accel class device file failed!\n");
		ret = -EINVAL;
		goto err_remove_accel_cdev;
	}

	ret = misc_register(&gs_misc);
	if (ret < 0) {
		return ret;
	}

	ret = misc_register(&gyro_misc);
	if (ret < 0) {
		return ret;
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
err_destroy_workqueue:
	destroy_workqueue(sensor->data_wq);
	if (client->irq > 0)
		free_irq(client->irq, sensor);
	hrtimer_cancel(&sensor->gyro_timer);
	hrtimer_cancel(&sensor->accel_timer);
	kthread_stop(sensor->gyr_task);
	kthread_stop(sensor->accel_task);
err_free_gpio:
	if ((sensor->pdata->use_int) &&
		(gpio_is_valid(sensor->pdata->gpio_int)))
		gpio_free(sensor->pdata->gpio_int);
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
 * mpu6050_remove() - remove a sensor
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
	destroy_workqueue(sensor->data_wq);
	hrtimer_cancel(&sensor->gyro_timer);
	hrtimer_cancel(&sensor->accel_timer);
	kthread_stop(sensor->gyr_task);
	kthread_stop(sensor->accel_task);
	if (client->irq > 0)
		free_irq(client->irq, sensor);
	if ((sensor->pdata->use_int) &&
		(gpio_is_valid(sensor->pdata->gpio_int)))
		gpio_free(sensor->pdata->gpio_int);
	mpu6050_power_ctl(sensor, false);
	mpu6050_power_deinit(sensor);
	if (gpio_is_valid(sensor->enable_gpio))
		gpio_free(sensor->enable_gpio);
	devm_kfree(&client->dev, sensor);

	return 0;
}

#ifdef CONFIG_PM
/**
 * mpu6050_suspend() - called on device suspend
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
	if ((sensor->batch_accel) || (sensor->batch_gyro)) {
		mpu6050_set_interrupt(sensor,
				BIT_FIFO_OVERFLOW, false);
		cancel_delayed_work_sync(&sensor->fifo_flush_work);
		goto exit;
	}
	if (sensor->motion_det_en) {

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
			hrtimer_cancel(&sensor->gyro_timer);

		if (sensor->cfg.accel_enable)
			hrtimer_cancel(&sensor->accel_timer);
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
 * mpu6050_resume() - called on device resume
 * @dev: device being resumed
 *
 * Put the device into powered mode on resume.
 */
static int mpu6050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu6050_sensor *sensor = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&sensor->op_lock);
	if ((sensor->batch_accel) || (sensor->batch_gyro)) {
		mpu6050_set_interrupt(sensor,
				BIT_FIFO_OVERFLOW, true);
		mpu6050_sche_next_flush(sensor);
	}

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
			ktime_t ktime;
			ktime = ktime_set(0,
					sensor->gyro_poll_ms * NSEC_PER_MSEC);
			hrtimer_start(&sensor->gyro_timer, ktime,
					HRTIMER_MODE_REL);

		}
	}

	if (sensor->cfg.accel_enable) {
		ret = mpu6050_accel_enable(sensor, true);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to enable accel\n");
			goto exit;
		}

		if (sensor->use_poll) {
			ktime_t ktime;
			ktime = ktime_set(0,
					sensor->accel_poll_ms * NSEC_PER_MSEC);
			hrtimer_start(&sensor->accel_timer, ktime,
					HRTIMER_MODE_REL);
		}
	}

	if (!sensor->use_poll)
		enable_irq(client->irq);

exit:
	mutex_unlock(&sensor->op_lock);
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

/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/regmap.h>
#include <linux/sensors.h>
#include <linux/uaccess.h>

#define BMM150_REG_CHIP_ID		0x40
#define BMM150_REG_DATA			0x42
#define BMM150_REG_DS			0x48
#define BMM150_REG_CTRL			0x4C
#define BMM150_REG_POWER_CTRL		0x4B

#define BMM150_DEFAULT_INTERVAL_MS	100
#define BMM150_RETRY_COUNT		10

#define BMM150_CHIP_ID			0x32

/* POWER SUPPLY VOLTAGE RANGE */
#define BMM150_VDD_MIN_UV		2000000
#define BMM150_VDD_MAX_UV		3300000
#define BMM150_VIO_MIN_UV		1750000
#define BMM150_VIO_MAX_UV		1950000

#define BMM150_REG_MAGIC		0xFF
#define BMM150_REG_COUNT		0x32

#define BMM150_OVERFLOW_OUTPUT_S32	((s32)(-2147483647-1))
#define BMM150_FLIP_OVERFLOW_ADCVAL	(-4096)
#define BMM150_HALL_OVERFLOW_ADCVAL	(-16384)


#define BMM150_I2C_NAME			"bmm150"

enum {
	OBVERSE_X_AXIS_FORWARD = 0,
	OBVERSE_X_AXIS_RIGHTWARD,
	OBVERSE_X_AXIS_BACKWARD,
	OBVERSE_X_AXIS_LEFTWARD,
	REVERSE_X_AXIS_FORWARD,
	REVERSE_X_AXIS_RIGHTWARD,
	REVERSE_X_AXIS_BACKWARD,
	REVERSE_X_AXIS_LEFTWARD,
	BMM150_DIR_COUNT,
};

enum {
	CMD_WRITE = 0,
	CMD_READ = 1,
};

static char *bmm150_dir[BMM150_DIR_COUNT] = {
	[OBVERSE_X_AXIS_FORWARD] = "obverse-x-axis-forward",
	[OBVERSE_X_AXIS_RIGHTWARD] = "obverse-x-axis-rightward",
	[OBVERSE_X_AXIS_BACKWARD] = "obverse-x-axis-backward",
	[OBVERSE_X_AXIS_LEFTWARD] = "obverse-x-axis-leftward",
	[REVERSE_X_AXIS_FORWARD] = "reverse-x-axis-forward",
	[REVERSE_X_AXIS_RIGHTWARD] = "reverse-x-axis-rightward",
	[REVERSE_X_AXIS_BACKWARD] = "reverse-x-axis-backward",
	[REVERSE_X_AXIS_LEFTWARD] = "reverse-x-axis-leftward",
};

static s8 bmm150_rotation_matrix[BMM150_DIR_COUNT][9] = {
	[OBVERSE_X_AXIS_FORWARD] = {0, -1, 0, 1, 0, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_RIGHTWARD] = {1, 0, 0, 0, 1, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_BACKWARD] = {0, 1, 0, -1, 0, 0, 0, 0, 1},
	[OBVERSE_X_AXIS_LEFTWARD] = {-1, 0, 0, 0, -1, 0, 0, 0, 1},
	[REVERSE_X_AXIS_FORWARD] = {0, 1, 0, 1, 0, 0, 0, 0, -1},
	[REVERSE_X_AXIS_RIGHTWARD] = {1, 0, 0, 0, -1, 0, 0, 0, -1},
	[REVERSE_X_AXIS_BACKWARD] = {0, -1, 0, -1, 0, 0, 0, 0, -1},
	[REVERSE_X_AXIS_LEFTWARD] = {-1, 0, 0, 0, 1, 0, 0, 0, -1},
};

struct bmm150_vec {
	int x;
	int y;
	int z;
};

struct bmm150_data {
	struct mutex		ecompass_lock;
	struct mutex		ops_lock;
	struct workqueue_struct *data_wq;
	struct delayed_work	dwork;
	struct sensors_classdev	cdev;
	struct bmm150_vec	last;

	struct i2c_client	*i2c;
	struct input_dev	*idev;
	struct regulator	*vdd;
	struct regulator	*vio;
	struct regmap		*regmap;

	s8 dig_x1;
	s8 dig_y1;

	s8 dig_x2;
	s8 dig_y2;

	u16 dig_z1;
	s16 dig_z2;
	s16 dig_z3;
	s16 dig_z4;

	u8 dig_xy1;
	s8 dig_xy2;

	u16 dig_xyz1;

	int			dir;
	int			auto_report;
	int			enable;
	int			poll_interval;
	int			power_enabled;
	unsigned long		timeout;

	unsigned int		reg_addr;
};

static struct sensors_classdev sensors_cdev = {
	.name = "bmm150-mag",
	.vendor = "bosch",
	.version = 1,
	.handle = SENSORS_MAGNETIC_FIELD_HANDLE,
	.type = SENSOR_TYPE_MAGNETIC_FIELD,
	.max_range = "1300.0",
	.resolution = "0.0625",
	.sensor_power = "0.5",
	.min_delay = 100000, /* The maximum sample rate is 10Hz currently. */
	.max_delay = 200,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = BMM150_DEFAULT_INTERVAL_MS,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static s32 bmm150_compensate_X(struct bmm150_data *bmm, s16 mdata_x, u16 data_r)
{
	s16 inter_retval = 0;

	if (mdata_x != BMM150_FLIP_OVERFLOW_ADCVAL) {
		inter_retval = ((s16)(((u16)((((s32)bmm->dig_xyz1) << 14) /
			(data_r != 0 ? data_r : bmm->dig_xyz1))) -
			((u16)0x4000)));

		inter_retval = ((s16)((((s32)mdata_x) *
			((((((((s32)bmm->dig_xy2) * ((((s32)inter_retval) *
			((s32)inter_retval)) >> 7)) + (((s32)inter_retval) *
			((s32)(((s16)bmm->dig_xy1) << 7)))) >> 9) +
			((s32)0x100000)) * ((s32)(((s16)bmm->dig_x2) +
			((s16)0xA0)))) >> 12)) >> 13)) +
			(((s16)bmm->dig_x1) << 3);
	} else {
		return BMM150_OVERFLOW_OUTPUT_S32;
	}

	return inter_retval;
}

static s32 bmm150_compensate_Y(struct bmm150_data *bmm, s16 mdata_y, u16 data_r)
{
	s16 inter_retval = 0;
	if (mdata_y != BMM150_FLIP_OVERFLOW_ADCVAL) {
		inter_retval = ((s16)(((u16)((((s32)bmm->dig_xyz1) << 14) /
			(data_r != 0 ?  data_r : bmm->dig_xyz1))) -
			((u16)0x4000)));

		inter_retval = ((s16)((((s32)mdata_y) *
			((((((((s32)bmm->dig_xy2) * ((((s32) inter_retval) *
			((s32)inter_retval)) >> 7)) + (((s32)inter_retval) *
			((s32)(((s16)bmm->dig_xy1) << 7)))) >> 9) +
			((s32)0x100000)) * ((s32)(((s16)bmm->dig_y2) +
			((s16)0xA0)))) >> 12)) >> 13)) +
			(((s16)bmm->dig_y1) << 3);
	} else {
		return BMM150_OVERFLOW_OUTPUT_S32;
	}
	return inter_retval;
}

static s32 bmm150_compensate_Z(struct bmm150_data *bmm, s16 mdata_z, u16 data_r)
{
	s32 retval = 0;

	if (mdata_z != BMM150_HALL_OVERFLOW_ADCVAL) {
		retval = (((((s32)(mdata_z - bmm->dig_z4)) << 15) -
			((((s32)bmm->dig_z3) * ((s32)(((s16)data_r) -
			((s16)bmm->dig_xyz1))))>>2)) / (bmm->dig_z2 +
			((s16)(((((s16)bmm->dig_z1) * ((((s16)data_r) << 1))) +
			(1<<15))>>16))));
	} else {
		retval = BMM150_OVERFLOW_OUTPUT_S32;
	}
	return retval;
}

static int bmm150_read_xyz(struct bmm150_data *bmm,
		struct bmm150_vec *vec)
{
	int count = 0;
	unsigned char data[8];
	unsigned int status;
	struct bmm150_vec tmp;
	int rc = 0;
	u16 raw_r;

	mutex_lock(&bmm->ecompass_lock);

	rc = regmap_read(bmm->regmap, BMM150_REG_DS, &status);
	if (rc) {
		dev_err(&bmm->i2c->dev, "read reg %d failed at %d.(%d)\n",
				BMM150_REG_DS, __LINE__, rc);
		goto exit;

	}

	while ((!(status & 0x01)) && (count < BMM150_RETRY_COUNT)) {
		/* Read MD again*/
		rc = regmap_read(bmm->regmap, BMM150_REG_DS, &status);
		if (rc) {
			dev_err(&bmm->i2c->dev, "read reg %d failed at %d.(%d)\n",
					BMM150_REG_DS, __LINE__, rc);
			goto exit;

		}

		/* Wait more time to get valid data */
		usleep_range(1000, 1500);
		count++;
	}

	if (count >= BMM150_RETRY_COUNT) {
		dev_err(&bmm->i2c->dev, "TM not work!!");
		rc = -EFAULT;
		goto exit;
	}

	/* read xyz raw data */
	rc = regmap_bulk_read(bmm->regmap, BMM150_REG_DATA, data, sizeof(data));
	if (rc) {
		dev_err(&bmm->i2c->dev, "read reg %d failed at %d.(%d)\n",
				BMM150_REG_DS, __LINE__, rc);
		goto exit;
	}

	tmp.x = (s16)(((s16)(s8)data[1]) << 5) | ((u8)data[0] >> 3);
	tmp.y = (s16)(((s16)(s8)data[3]) << 5) | ((u8)data[2] >> 3);
	tmp.z = (s16)(((s16)(s8)data[5]) << 7) | ((u8)data[4] >> 1);
	raw_r = (s16)(((s16)(s8)data[7]) << 6) | ((u8)data[6] >> 2);

	tmp.x = bmm150_compensate_X(bmm, tmp.x, raw_r);
	tmp.y = bmm150_compensate_Y(bmm, tmp.y, raw_r);
	tmp.z = bmm150_compensate_Z(bmm, tmp.z, raw_r);

	dev_dbg(&bmm->i2c->dev, "raw data:%d %d %d %d %d %d",
			data[0], data[1], data[2], data[3], data[4], data[5]);
	dev_dbg(&bmm->i2c->dev, "raw x:%d y:%d z:%d\n", tmp.x, tmp.y, tmp.z);

	vec->x = tmp.x;
	vec->y = tmp.y;
	vec->z = -tmp.z;

exit:
	mutex_unlock(&bmm->ecompass_lock);
	return rc;
}

static void bmm150_poll(struct work_struct *work)
{
	int ret;
	s8 *tmp;
	struct bmm150_vec vec;
	struct bmm150_vec report;
	struct bmm150_data *bmm = container_of((struct delayed_work *)work,
			struct bmm150_data, dwork);

	vec.x = vec.y = vec.z = 0;

	ret = bmm150_read_xyz(bmm, &vec);
	if (ret) {
		dev_warn(&bmm->i2c->dev, "read xyz failed\n");
		goto exit;
	}

	tmp = &bmm150_rotation_matrix[bmm->dir][0];
	report.x = tmp[0] * vec.x + tmp[1] * vec.y + tmp[2] * vec.z;
	report.y = tmp[3] * vec.x + tmp[4] * vec.y + tmp[5] * vec.z;
	report.z = tmp[6] * vec.x + tmp[7] * vec.y + tmp[8] * vec.z;

	input_report_abs(bmm->idev, ABS_X, report.x);
	input_report_abs(bmm->idev, ABS_Y, report.y);
	input_report_abs(bmm->idev, ABS_Z, report.z);
	input_sync(bmm->idev);

exit:
	queue_delayed_work(bmm->data_wq,
			&bmm->dwork,
			msecs_to_jiffies(bmm->poll_interval));
}

static struct input_dev *bmm150_init_input(struct i2c_client *client)
{
	int status;
	struct input_dev *input = NULL;

	input = devm_input_allocate_device(&client->dev);
	if (!input)
		return NULL;

	input->name = "compass";
	input->phys = "bmm150/input0";
	input->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, input->evbit);

	input_set_abs_params(input, ABS_X, -2047, 2047, 0, 0);
	input_set_abs_params(input, ABS_Y, -2047, 2047, 0, 0);
	input_set_abs_params(input, ABS_Z, -2047, 2047, 0, 0);

	input_set_capability(input, EV_REL, REL_X);
	input_set_capability(input, EV_REL, REL_Y);
	input_set_capability(input, EV_REL, REL_Z);

	status = input_register_device(input);
	if (status) {
		dev_err(&client->dev,
			"error registering input device\n");
		return NULL;
	}

	return input;
}

static int bmm150_power_init(struct bmm150_data *data)
{
	int rc;

	data->vdd = devm_regulator_get(&data->i2c->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->i2c->dev,
				"Regualtor get failed vdd rc=%d\n", rc);
		return rc;
	}
	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd,
				BMM150_VDD_MIN_UV, BMM150_VDD_MAX_UV);
		if (rc) {
			dev_err(&data->i2c->dev,
					"Regulator set failed vdd rc=%d\n",
					rc);
			goto exit;
		}
	}

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->i2c->dev,
				"Regulator enable vdd failed rc=%d\n", rc);
		goto exit;
	}
	data->vio = devm_regulator_get(&data->i2c->dev, "vio");
	if (IS_ERR(data->vio)) {
		rc = PTR_ERR(data->vio);
		dev_err(&data->i2c->dev,
				"Regulator get failed vio rc=%d\n", rc);
		goto reg_vdd_set;
	}

	if (regulator_count_voltages(data->vio) > 0) {
		rc = regulator_set_voltage(data->vio,
				BMM150_VIO_MIN_UV, BMM150_VIO_MAX_UV);
		if (rc) {
			dev_err(&data->i2c->dev,
					"Regulator set failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}
	}
	rc = regulator_enable(data->vio);
	if (rc) {
		dev_err(&data->i2c->dev,
				"Regulator enable vio failed rc=%d\n", rc);
		goto reg_vdd_set;
	}

	 /* The minimum time to operate device after VDD valid is 1 ms. */
	usleep_range(1500, 2000);

	data->power_enabled = true;

	return 0;

reg_vdd_set:
	regulator_disable(data->vdd);
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, BMM150_VDD_MAX_UV);
exit:
	return rc;

}

static int bmm150_power_deinit(struct bmm150_data *data)
{
	if (!IS_ERR_OR_NULL(data->vio)) {
		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0,
					BMM150_VIO_MAX_UV);

		regulator_disable(data->vio);
	}

	if (!IS_ERR_OR_NULL(data->vdd)) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0,
					BMM150_VDD_MAX_UV);

		regulator_disable(data->vdd);
	}

	data->power_enabled = false;

	return 0;
}

static int bmm150_power_set(struct bmm150_data *bmm, bool on)
{
	int rc = 0;

	if (!on && bmm->power_enabled) {
		mutex_lock(&bmm->ecompass_lock);

		rc = regulator_disable(bmm->vdd);
		if (rc) {
			dev_err(&bmm->i2c->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			goto err_vdd_disable;
		}

		rc = regulator_disable(bmm->vio);
		if (rc) {
			dev_err(&bmm->i2c->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			goto err_vio_disable;
		}
		bmm->power_enabled = false;

		mutex_unlock(&bmm->ecompass_lock);
		return rc;
	} else if (on && !bmm->power_enabled) {
		mutex_lock(&bmm->ecompass_lock);

		rc = regulator_enable(bmm->vdd);
		if (rc) {
			dev_err(&bmm->i2c->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			goto err_vdd_enable;
		}

		rc = regulator_enable(bmm->vio);
		if (rc) {
			dev_err(&bmm->i2c->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			goto err_vio_enable;
		}
		bmm->power_enabled = true;

		mutex_unlock(&bmm->ecompass_lock);

		/* The minimum time to operate after VDD valid is 10 ms */
		usleep_range(15000, 20000);

		return rc;
	} else {
		dev_warn(&bmm->i2c->dev,
				"Power on=%d. enabled=%d\n",
				on, bmm->power_enabled);
		return rc;
	}

err_vio_enable:
	regulator_disable(bmm->vio);
err_vdd_enable:
	mutex_unlock(&bmm->ecompass_lock);
	return rc;

err_vio_disable:
	if (regulator_enable(bmm->vdd))
		dev_warn(&bmm->i2c->dev, "Regulator vdd enable failed\n");
err_vdd_disable:
	mutex_unlock(&bmm->ecompass_lock);
	return rc;
}

static int bmm150_check_device(struct bmm150_data *bmm)
{
	unsigned int data;
	int rc;

	rc = regmap_read(bmm->regmap, BMM150_REG_CHIP_ID, &data);
	if (rc) {
		dev_err(&bmm->i2c->dev, "read reg %d failed.(%d)\n",
				BMM150_REG_DS, rc);
		return rc;

	}

	if (data != BMM150_CHIP_ID) {
		dev_err(&bmm->i2c->dev, "chip id:0x%x exptected:0x%x\n",
				data, BMM150_CHIP_ID);
		return -ENODEV;
	}

	return 0;
}

static int bmm150_parse_dt(struct i2c_client *client,
		struct bmm150_data *bmm)
{
	struct device_node *np = client->dev.of_node;
	const char *tmp;
	int rc;
	int i;

	rc = of_property_read_string(np, "bmm,dir", &tmp);

	/* does not have a value or the string is not null-terminated */
	if (rc && (rc != -EINVAL)) {
		dev_err(&client->dev, "Unable to read bmm,dir\n");
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(bmm150_dir); i++) {
		if (strcmp(bmm150_dir[i], tmp) == 0)
			break;
	}

	if (i >= ARRAY_SIZE(bmm150_dir)) {
		dev_err(&client->dev, "Invalid bmm,dir property");
		return -EINVAL;
	}

	bmm->dir = i;
	bmm->auto_report = of_property_read_bool(np, "bmm,auto-report");

	return 0;
}

static int bmm150_init_trim(struct bmm150_data *bmm)
{
	int rc;
	u8 tmp[2] = {0, 0};
	unsigned int val;

	rc = regmap_read(bmm->regmap, 0x5D, &val);
	bmm->dig_x1 = val;

	rc |= regmap_read(bmm->regmap, 0x5E, &val);
	bmm->dig_y1 = val;

	rc |= regmap_read(bmm->regmap, 0x64, &val);
	bmm->dig_x2 = val;

	rc |= regmap_read(bmm->regmap, 0x65, &val);
	bmm->dig_y2 = val;

	rc |= regmap_read(bmm->regmap, 0x71, &val);
	bmm->dig_xy1 = val;

	rc |= regmap_read(bmm->regmap, 0x70, &val);
	bmm->dig_xy2 = val;

	rc |= regmap_bulk_read(bmm->regmap, 0x6A, tmp, 2);
	bmm->dig_z1 = (u16)((((u16)((u8)tmp[1])) << 8) | tmp[0]);

	rc |= regmap_bulk_read(bmm->regmap, 0x68, tmp, 2);
	bmm->dig_z2 = (s16)((((s16)((s8)tmp[1])) << 8) | tmp[0]);

	rc |= regmap_bulk_read(bmm->regmap, 0x6E, tmp, 2);
	bmm->dig_z3 = (s16)((((s16)((s8)tmp[1])) << 8) | tmp[0]);

	rc |= regmap_bulk_read(bmm->regmap, 0x62, tmp, 2);
	bmm->dig_z4 = (s16)((((s16)((s8)tmp[1])) << 8) | tmp[0]);

	rc |= regmap_bulk_read(bmm->regmap, 0x6C, tmp, 2);
	tmp[1] = ((tmp[1] & 0x7F) >> 0);
	bmm->dig_xyz1 = (u16)((((u16)((u8)tmp[1])) << 8) | tmp[0]);

	return rc ? -EIO : 0;
}

static int bmm150_init_device(struct bmm150_data *bmm)
{
	int rc;

	rc = regmap_write(bmm->regmap, BMM150_REG_POWER_CTRL, 0x01);
	if (rc) {
		dev_err(&bmm->i2c->dev, "write 0x%x failed",
				BMM150_REG_POWER_CTRL);
		goto exit;
	}

	/* from suspend to sleep */
	usleep_range(3100, 3500);

exit:
	return rc;
}

static int bmm150_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int rc = 0;
	unsigned int config;
	struct bmm150_data *bmm = container_of(sensors_cdev,
			struct bmm150_data, cdev);

	mutex_lock(&bmm->ops_lock);

	if (enable && (!bmm->enable)) {
		rc = bmm150_power_set(bmm, true);
		if (rc) {
			dev_err(&bmm->i2c->dev, "Power up failed\n");
			goto exit;
		}

		rc = bmm150_init_device(bmm);
		if (rc) {
			dev_err(&bmm->i2c->dev, "init device failed\n");
			goto exit;
		}

		rc = regmap_read(bmm->regmap, BMM150_REG_CTRL, &config);
		if (rc) {
			dev_err(&bmm->i2c->dev, "read 0x%x failed\n",
					BMM150_REG_CTRL);
			goto exit;
		}

		rc = regmap_write(bmm->regmap, BMM150_REG_CTRL,
				config & (~0x06));
		if (rc) {
			dev_err(&bmm->i2c->dev, "write 0x%x failed\n",
					BMM150_REG_CTRL);
			goto exit;
		}

		if (bmm->auto_report)
			queue_delayed_work(bmm->data_wq,
				&bmm->dwork,
				msecs_to_jiffies(bmm->poll_interval));
	} else if ((!enable) && bmm->enable) {
		if (bmm->auto_report)
			cancel_delayed_work_sync(&bmm->dwork);

		rc = regmap_read(bmm->regmap, BMM150_REG_CTRL, &config);
		if (rc) {
			dev_err(&bmm->i2c->dev, "read 0x%x failed\n",
					BMM150_REG_CTRL);
			goto exit;
		}

		rc = regmap_write(bmm->regmap, BMM150_REG_CTRL, config | 0x06);
		if (rc) {
			dev_err(&bmm->i2c->dev, "write 0x%x failed\n",
					BMM150_REG_CTRL);
			goto exit;
		}

		if (bmm150_power_set(bmm, false))
			dev_warn(&bmm->i2c->dev, "Power off failed\n");
	} else {
		dev_warn(&bmm->i2c->dev,
				"ignore enable state change from %d to %d\n",
				bmm->enable, enable);
	}

	bmm->enable = enable;

exit:
	mutex_unlock(&bmm->ops_lock);
	return rc;
}

static int bmm150_set_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct bmm150_data *bmm = container_of(sensors_cdev,
			struct bmm150_data, cdev);

	mutex_lock(&bmm->ops_lock);
	if (bmm->poll_interval != delay_msec)
		bmm->poll_interval = delay_msec;

	if (bmm->auto_report && bmm->enable)
		mod_delayed_work(system_wq, &bmm->dwork,
				msecs_to_jiffies(delay_msec));

	mutex_unlock(&bmm->ops_lock);

	return 0;
}

static struct regmap_config bmm150_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static ssize_t bmm150_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmm150_data *di = dev_get_drvdata(dev);
	unsigned int val;
	int rc;
	ssize_t count = 0;
	int i;

	if (di->reg_addr == BMM150_REG_MAGIC) {
		for (i = 0; i < BMM150_REG_COUNT; i++) {
			rc = regmap_read(di->regmap, 0x40 + i,
					&val);
			if (rc) {
				dev_err(&di->i2c->dev, "read %d failed\n",
						0x40 + i);
				break;
			}
			count += snprintf(&buf[count], PAGE_SIZE - count,
					"0x%x: 0x%x\n", 0x40 + i,
					val);
		}
	} else {
		rc = regmap_read(di->regmap, di->reg_addr, &val);
		if (rc) {
			dev_err(&di->i2c->dev, "read %d failed\n",
					di->reg_addr);
			return rc;
		}
		count = snprintf(&buf[count], PAGE_SIZE, "0x%x:0x%x\n",
				di->reg_addr, val);
	}

	return count;
}

static ssize_t bmm150_register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct bmm150_data *di = dev_get_drvdata(dev);
	unsigned int reg;
	unsigned int val;
	unsigned int cmd;
	int rc;

	if (sscanf(buf, "%u %u %u\n", &cmd, &reg, &val) < 2) {
		dev_err(&di->i2c->dev, "argument error\n");
		return -EINVAL;
	}

	if (cmd == CMD_WRITE) {
		rc = regmap_write(di->regmap, reg, val);
		if (rc) {
			dev_err(&di->i2c->dev, "write %d failed\n", reg);
			return rc;
		}
	} else if (cmd == CMD_READ) {
		di->reg_addr = reg;
		dev_dbg(&di->i2c->dev, "register address set to 0x%x\n", reg);
	}

	return size;
}

static DEVICE_ATTR(register, S_IWUSR | S_IRUGO,
		bmm150_register_show,
		bmm150_register_store);

static struct attribute *bmm150_attr[] = {
	&dev_attr_register.attr,
	NULL
};

static const struct attribute_group bmm150_attr_group = {
	.attrs = bmm150_attr,
};

static int bmm150_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int res = 0;
	struct bmm150_data *bmm;

	dev_info(&client->dev, "probing bmm150\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("bmm150 i2c functionality check failed.\n");
		res = -ENODEV;
		goto out;
	}

	bmm = devm_kzalloc(&client->dev, sizeof(struct bmm150_data),
			GFP_KERNEL);
	if (!bmm) {
		dev_err(&client->dev, "memory allocation failed.\n");
		res = -ENOMEM;
		goto out;
	}

	if (client->dev.of_node) {
		res = bmm150_parse_dt(client, bmm);
		if (res) {
			dev_err(&client->dev,
				"Unable to parse platform data.(%d)", res);
			goto out;
		}
	} else {
		bmm->dir = 0;
		bmm->auto_report = 1;
	}

	bmm->i2c = client;
	dev_set_drvdata(&client->dev, bmm);

	mutex_init(&bmm->ecompass_lock);
	mutex_init(&bmm->ops_lock);

	bmm->regmap = devm_regmap_init_i2c(client, &bmm150_regmap_config);
	if (IS_ERR(bmm->regmap)) {
		dev_err(&client->dev, "Init regmap failed.(%ld)",
				PTR_ERR(bmm->regmap));
		res = PTR_ERR(bmm->regmap);
		goto out;
	}

	res = bmm150_power_init(bmm);
	if (res) {
		dev_err(&client->dev, "Power up bmm150 failed\n");
		goto out;
	}

	res = bmm150_init_device(bmm);
	if (res) {
		dev_err(&client->dev, "init device failed\n");
		goto out_init_device;
	}

	res = bmm150_check_device(bmm);
	if (res) {
		dev_err(&client->dev, "Check device failed\n");
		goto out_init_device;
	}

	res = bmm150_init_trim(bmm);
	if (res) {
		dev_err(&client->dev, "init trim failed\n");
		goto out_init_device;
	}

	res = sysfs_create_group(&client->dev.kobj, &bmm150_attr_group);
	if (res) {
		dev_err(&client->dev, "create sysfs failed\n");
		goto out_create_group;
	}

	bmm->idev = bmm150_init_input(client);
	if (!bmm->idev) {
		dev_err(&client->dev, "init input device failed\n");
		res = -ENODEV;
		goto out_init_input;
	}

	bmm->data_wq = NULL;
	if (bmm->auto_report) {
		dev_dbg(&client->dev, "auto report is enabled\n");
		INIT_DELAYED_WORK(&bmm->dwork, bmm150_poll);
		bmm->data_wq =
			create_freezable_workqueue("bmm150_data_work");
		if (!bmm->data_wq) {
			dev_err(&client->dev, "Cannot create workqueue.\n");
			goto out_create_workqueue;
		}
	}

	bmm->cdev = sensors_cdev;
	bmm->cdev.sensors_enable = bmm150_set_enable;
	bmm->cdev.sensors_poll_delay = bmm150_set_poll_delay;
	res = sensors_classdev_register(&client->dev, &bmm->cdev);
	if (res) {
		dev_err(&client->dev, "sensors class register failed.\n");
		goto out_register_classdev;
	}

	res = bmm150_power_set(bmm, false);
	if (res) {
		dev_err(&client->dev, "Power off failed\n");
		goto out_power_set;
	}

	bmm->poll_interval = BMM150_DEFAULT_INTERVAL_MS;

	dev_dbg(&client->dev, "bmm150 successfully probed\n");

	return 0;

out_power_set:
	sensors_classdev_unregister(&bmm->cdev);
out_register_classdev:
	if (bmm->data_wq)
		destroy_workqueue(bmm->data_wq);
out_create_workqueue:
	input_unregister_device(bmm->idev);
out_init_input:
	sysfs_remove_group(&client->dev.kobj, &bmm150_attr_group);
out_create_group:
out_init_device:
	bmm150_power_deinit(bmm);
out:
	return res;
}

static int bmm150_remove(struct i2c_client *client)
{
	struct bmm150_data *bmm = dev_get_drvdata(&client->dev);

	sensors_classdev_unregister(&bmm->cdev);
	if (bmm->data_wq)
		destroy_workqueue(bmm->data_wq);
	bmm150_power_deinit(bmm);

	if (bmm->idev)
		input_unregister_device(bmm->idev);

	sysfs_remove_group(&client->dev.kobj, &bmm150_attr_group);

	return 0;
}

static int bmm150_suspend(struct device *dev)
{
	int res = 0;
	struct bmm150_data *bmm = dev_get_drvdata(dev);

	dev_dbg(dev, "suspended\n");
	mutex_lock(&bmm->ops_lock);

	if (bmm->enable) {
		if (bmm->auto_report)
			cancel_delayed_work_sync(&bmm->dwork);

		res = bmm150_power_set(bmm, false);
		if (res) {
			dev_err(dev, "failed to suspend bmm150\n");
			goto exit;
		}
	}
exit:
	mutex_unlock(&bmm->ops_lock);
	return res;
}

static int bmm150_resume(struct device *dev)
{
	int res = 0;
	unsigned int config;
	struct bmm150_data *bmm = dev_get_drvdata(dev);

	dev_dbg(dev, "resumed\n");

	if (bmm->enable) {
		res = bmm150_power_set(bmm, true);
		if (res) {
			dev_err(&bmm->i2c->dev, "Power enable failed\n");
			goto exit;
		}

		res = bmm150_init_device(bmm);
		if (res) {
			dev_err(&bmm->i2c->dev, "init device failed\n");
			goto exit;
		}

		res = regmap_read(bmm->regmap, BMM150_REG_CTRL, &config);
		if (res) {
			dev_err(&bmm->i2c->dev, "read 0x%x failed\n",
					BMM150_REG_CTRL);
			goto exit;
		}

		res = regmap_write(bmm->regmap, BMM150_REG_CTRL,
				config & (~0x06));
		if (res) {
			dev_err(&bmm->i2c->dev, "write 0x%x failed\n",
					BMM150_REG_CTRL);
			goto exit;
		}


		if (bmm->auto_report)
			queue_delayed_work(bmm->data_wq,
				&bmm->dwork,
				msecs_to_jiffies(bmm->poll_interval));
	}

exit:
	return res;
}

static const struct i2c_device_id bmm150_id[] = {
	{ BMM150_I2C_NAME, 0 },
	{ }
};

static struct of_device_id bmm150_match_table[] = {
	{ .compatible = "bosch,bmm150", },
	{ },
};

static const struct dev_pm_ops bmm150_pm_ops = {
	.suspend = bmm150_suspend,
	.resume = bmm150_resume,
};

static struct i2c_driver bmm150_driver = {
	.probe		= bmm150_probe,
	.remove		= bmm150_remove,
	.id_table	= bmm150_id,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= BMM150_I2C_NAME,
		.of_match_table = bmm150_match_table,
		.pm = &bmm150_pm_ops,
	},
};

module_i2c_driver(bmm150_driver);

MODULE_DESCRIPTION("BOSCH BMM150 Magnetic Sensor Driver");
MODULE_LICENSE("GPL v2");

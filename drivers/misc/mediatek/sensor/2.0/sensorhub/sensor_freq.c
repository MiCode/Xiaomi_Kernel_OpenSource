// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "sensor_freq " fmt

#include <linux/err.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kernel.h>

#include "sensor_freq.h"

static DECLARE_BITMAP(high_freq_tb, SENSOR_TYPE_SENSOR_MAX);
static DECLARE_BITMAP(medium_freq_tb, SENSOR_TYPE_SENSOR_MAX);
static DECLARE_BITMAP(low_freq_tb, SENSOR_TYPE_SENSOR_MAX);

static struct sensor_freq_tb sensor_freq_table[] = {
	{
		.sensor_type = SENSOR_TYPE_OIS,
		.freq_level = HIGH,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_ACCELEROMETER,
		.freq_level = LOW,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_GYROSCOPE,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_ORIENTATION,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_GRAVITY,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_LINEAR_ACCELERATION,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_ROTATION_VECTOR,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_GAME_ROTATION_VECTOR,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
	{
		.sensor_type = SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
		.freq_level = MEDIUM,
		.core_id = 0,
	},
};

static int freq_tb_bitmap_set(uint8_t sensor_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sensor_freq_table); i++) {
		if (sensor_type == sensor_freq_table[i].sensor_type) {
			switch (sensor_freq_table[i].freq_level) {
			case LOW:
				set_bit(sensor_type, low_freq_tb);
				return 0;
			case MEDIUM:
				set_bit(sensor_type, medium_freq_tb);
				return 0;
			case HIGH:
				set_bit(sensor_type, high_freq_tb);
				return 0;
			default:
				pr_err("sensor[%d] freq level err %d\n",
					sensor_type,
					sensor_freq_table[i].freq_level);
				return -EINVAL;
			}
		}
	}

	return -EINVAL;
}

static int freq_tb_bitmap_clear(uint8_t sensor_type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sensor_freq_table); i++) {
		if (sensor_type == sensor_freq_table[i].sensor_type) {
			switch (sensor_freq_table[i].freq_level) {
			case LOW:
				clear_bit(sensor_type, low_freq_tb);
				return 0;
			case MEDIUM:
				clear_bit(sensor_type, medium_freq_tb);
				return 0;
			case HIGH:
				clear_bit(sensor_type, high_freq_tb);
				return 0;
			default:
				pr_err("sensor[%d] freq level err %d\n",
					sensor_type,
					sensor_freq_table[i].freq_level);
				return -EINVAL;
			}
		}
	}

	return -EINVAL;
}

static int set_scp_freq(void)
{
	int ret = 0;

	if (!bitmap_empty(high_freq_tb, SENSOR_TYPE_SENSOR_MAX)) {
		ret = sensor_control_scp(SENS_FEATURE_ID, HIGH_FREQ_VALUE);
		return ret;
	}
	if (!bitmap_empty(medium_freq_tb, SENSOR_TYPE_SENSOR_MAX)) {
		ret = sensor_control_scp(SENS_FEATURE_ID, MEDIUM_FREQ_VALUE);
		return ret;
	}
	if (!bitmap_empty(low_freq_tb, SENSOR_TYPE_SENSOR_MAX))
		ret = sensor_control_scp(SENS_FEATURE_ID, LOW_FREQ_VALUE);
	else
		ret = sensor_control_scp(SENS_FEATURE_ID, 0);

	return ret;
}

int sensor_register_freq(uint8_t sensor_type)
{
	int ret = 0;

	if (!freq_tb_bitmap_set(sensor_type))
		ret = set_scp_freq();

	return ret;
}

int sensor_deregister_freq(uint8_t sensor_type)
{
	int ret = 0;

	if (!freq_tb_bitmap_clear(sensor_type))
		ret = set_scp_freq();

	return ret;
}


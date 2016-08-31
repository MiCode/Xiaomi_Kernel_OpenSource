/*
 * drivers/misc/nct1008.c
 *
 * Driver for NCT1008, temperature monitoring device from ON Semiconductors
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/nct1008.h>
#include <linux/delay.h>
#include <linux/thermal.h>
#include <linux/regulator/consumer.h>

/* Register Addresses used in this module. */
#define LOC_TEMP_RD                  0x00
#define EXT_TEMP_HI_RD               0x01
#define STATUS_RD                    0x02
#define CONFIG_RD                    0x03
#define CONV_RATE_RD                 0x04
#define LOC_TEMP_HI_LIMIT_RD         0x05
#define LOC_TEMP_LO_LIMIT_RD         0x06
#define EXT_TEMP_HI_LIMIT_HI_BYTE_RD 0x07
#define EXT_TEMP_LO_LIMIT_HI_BYTE_RD 0x08
#define CONFIG_WR                    0x09
#define CONV_RATE_WR                 0x0A
#define LOC_TEMP_HI_LIMIT_WR         0x0B
#define LOC_TEMP_LO_LIMIT_WR         0x0C
#define EXT_TEMP_HI_LIMIT_HI_BYTE_WR 0x0D
#define EXT_TEMP_LO_LIMIT_HI_BYTE_WR 0x0E
#define ONE_SHOT                     0x0F
#define EXT_TEMP_LO_RD               0x10
#define OFFSET_WR                    0x11
#define OFFSET_QUARTER_WR            0x12
#define EXT_TEMP_HI_LIMIT_LO_BYTE    0x13
#define EXT_TEMP_LO_LIMIT_LO_BYTE    0x14
/* NOT USED                          0x15 */
/* NOT USED                          0x16 */
/* NOT USED                          0x17 */
/* NOT USED                          0x18 */
#define EXT_THERM_LIMIT_WR           0x19
/* NOT USED                          0x1A */
/* NOT USED                          0x1B */
/* NOT USED                          0x1C */
/* NOT USED                          0x1D */
/* NOT USED                          0x1E */
/* NOT USED                          0x1F */
#define LOC_THERM_LIMIT              0x20
#define THERM_HYSTERESIS             0x21
#define COSECUTIVE_ALERT             0x22

/* Set of register types that are sensor dependant. */
enum nct1008_sensor_reg_types {
	TEMP_HI_LIMIT, TEMP_LO_LIMIT,
	TEMP_HI_LIMIT_RD, TEMP_LO_LIMIT_RD,
	TEMP_HI_LIMIT_WR, TEMP_LO_LIMIT_WR,
	TEMP_RD_LO, TEMP_RD_HI,
	TEMP_RD, TEMP_WR,
	REGS_COUNT /* This has to be the last element! */
};

/* Mapping from register type on a given sensor to hardware specific address. */
static int nct1008_sensor_regs[SENSORS_COUNT][REGS_COUNT] = {
	[LOC] = {
		[TEMP_HI_LIMIT_RD] = LOC_TEMP_HI_LIMIT_RD,
		[TEMP_HI_LIMIT_WR] = LOC_TEMP_HI_LIMIT_WR,
		[TEMP_LO_LIMIT_RD] = LOC_TEMP_LO_LIMIT_RD,
		[TEMP_LO_LIMIT_WR] = LOC_TEMP_LO_LIMIT_WR,
	},
	[EXT] = {
		[TEMP_HI_LIMIT_RD] = EXT_TEMP_HI_LIMIT_HI_BYTE_RD,
		[TEMP_HI_LIMIT_WR] = EXT_TEMP_HI_LIMIT_HI_BYTE_WR,
		[TEMP_LO_LIMIT_RD] = EXT_TEMP_LO_LIMIT_HI_BYTE_RD,
		[TEMP_LO_LIMIT_WR] = EXT_TEMP_LO_LIMIT_HI_BYTE_WR,
		[TEMP_RD_LO] = EXT_TEMP_LO_RD,
		[TEMP_RD_HI] = EXT_TEMP_HI_RD,
	},
};

/* Accessor to the sensor specific registers. */
#define NCT_REG(x, y) nct1008_sensor_regs[x][y]

/* Configuration register bits. */
#define EXTENDED_RANGE_BIT BIT(2)
#define THERM2_BIT         BIT(5)
#define STANDBY_BIT        BIT(6)
#define ALERT_BIT          BIT(7)

/* Status register trip point bits. */
#define EXT_LO_BIT BIT(3) /* External Sensor has tripped 'temp <= LOW' */
#define EXT_HI_BIT BIT(4) /* External Sensor has tripped 'temp > HIGH' */
#define LOC_LO_BIT BIT(5) /* Local Sensor has tripped 'temp <= LOW' */
#define LOC_HI_BIT BIT(6) /* Local Sensor has tripped 'temp > HIGH' */

/* Constants.  */
#define EXTENDED_RANGE_OFFSET 64U
#define STANDARD_RANGE_MAX    127U
#define EXTENDED_RANGE_MAX   (150U + EXTENDED_RANGE_OFFSET)

#define NCT1008_MIN_TEMP       (-64)
#define NCT1008_MAX_TEMP         191
#define NCT1008_MAX_TEMP_MILLI   191750

#define MAX_STR_PRINT            50
#define MAX_CONV_TIME_ONESHOT_MS 52

#define CELSIUS_TO_MILLICELSIUS(x) ((x)*1000)
#define MILLICELSIUS_TO_CELSIUS(x) ((x)/1000)

#define THERM_WARN_RANGE_HIGH_OFFSET	2000
#define THERM_WARN_RANGE_LOW_OFFSET	7000

struct nct1008_adjust_offset_table {
	int temp;
	int offset;
};

struct nct1008_sensor_data {
	struct nct1008_adjust_offset_table offset_table[16];
	struct thermal_zone_device *thz;
	long current_hi_limit;
	long current_lo_limit;
	long temp;
};

struct nct1008_data {
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct i2c_client *client;
	struct nct1008_platform_data plat_data;
	struct mutex mutex;
	u8 config;
	enum nct1008_chip chip;
	struct regulator *nct_reg;
	int conv_period_ms;
	int nct_disabled;
	int stop_workqueue;

	struct nct1008_sensor_data sensors[SENSORS_COUNT];
};

static int conv_period_ms_table[] =
	{16000, 8000, 4000, 2000, 1000, 500, 250, 125, 63, 32, 16};

static void nct1008_setup_shutdown_warning(struct nct1008_data *data);

static inline s16 value_to_temperature(bool extended, u8 value)
{
	return extended ? (s16)(value - EXTENDED_RANGE_OFFSET) : (s16)value;
}

static inline u8 temperature_to_value(bool extended, s16 temp)
{
	return extended ? (u8)(temp + EXTENDED_RANGE_OFFSET) : (u8)temp;
}

static int nct1008_write_reg(struct i2c_client *client, u8 reg, u16 value)
{
	int ret = 0;
	struct nct1008_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	if (data && data->nct_disabled) {
		mutex_unlock(&data->mutex);
		return -ENODEV;
	}

	ret = i2c_smbus_write_byte_data(client, reg, value);
	mutex_unlock(&data->mutex);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int nct1008_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = 0;
	struct nct1008_data *data = i2c_get_clientdata(client);
	mutex_lock(&data->mutex);
	if (data && data->nct_disabled) {
		mutex_unlock(&data->mutex);
		return -ENODEV;
	}

	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&data->mutex);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int nct1008_get_temp_common(int sensor,
					struct nct1008_data *data,
					unsigned long *temp)
{
	struct i2c_client *client = data->client;
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	s16 temp_hi;
	s16 temp_lo;
	long temp_milli = 0;
	int i, off = 0;
	u8 value;
	int ret;

	/* Read External Temp */
	if (sensor == EXT) {
		ret = nct1008_read_reg(client, NCT_REG(sensor, TEMP_RD_LO));
		if (ret < 0)
			return -1;
		else
			value = ret;

		temp_lo = (value >> 6);

		ret = nct1008_read_reg(client, EXT_TEMP_HI_RD);
		if (ret < 0)
			return -1;
		else
			value = ret;

		temp_hi = value_to_temperature(pdata->extended_range, value);

		temp_milli = CELSIUS_TO_MILLICELSIUS(temp_hi) +
			 temp_lo * 250;

		for (i = 0; i < ARRAY_SIZE(data->sensors[sensor].offset_table);
			i++) {
			if (temp_milli < (data->sensors[sensor].
				offset_table[i].temp * 1000)) {
				off = data->sensors[sensor].offset_table[i].
				offset * 1000;
				break;
			}
		}

		temp_milli += off;

	} else if (sensor == LOC) {
		value = nct1008_read_reg(client, LOC_TEMP_RD);
		if (value < 0)
			return -1;
		temp_hi = value_to_temperature(pdata->extended_range, value);
		temp_milli = CELSIUS_TO_MILLICELSIUS(temp_hi);
	}

	*temp = temp_milli;
	data->sensors[sensor].temp = temp_milli;

	return 0;
}

static ssize_t nct1008_show_temp(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	s16 temp1 = 0;
	s16 temp = 0;
	u8 temp2 = 0;
	int value = 0;

	if (!dev || !buf || !attr)
		return -EINVAL;

	value = nct1008_read_reg(client, LOC_TEMP_RD);
	if (value < 0)
		goto error;
	temp1 = value_to_temperature(pdata->extended_range, value);

	value = nct1008_read_reg(client, EXT_TEMP_LO_RD);
	if (value < 0)
		goto error;
	temp2 = (value >> 6);
	value = nct1008_read_reg(client, EXT_TEMP_HI_RD);
	if (value < 0)
		goto error;
	temp = value_to_temperature(pdata->extended_range, value);

	return snprintf(buf, MAX_STR_PRINT, "%d %d.%d\n",
		temp1, temp, temp2 * 25);

error:
	return snprintf(buf, MAX_STR_PRINT,
		"Error read local/ext temperature\n");
}

static ssize_t nct1008_show_temp_overheat(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	int value;
	s16 temp, temp2;

	/* Local temperature h/w shutdown limit */
	value = nct1008_read_reg(client, LOC_THERM_LIMIT);
	if (value < 0)
		goto error;
	temp = value_to_temperature(pdata->extended_range, value);

	/* External temperature h/w shutdown limit */
	value = nct1008_read_reg(client, EXT_THERM_LIMIT_WR);
	if (value < 0)
		goto error;
	temp2 = value_to_temperature(pdata->extended_range, value);

	return snprintf(buf, MAX_STR_PRINT, "%d %d\n", temp, temp2);
error:
	dev_err(dev, "%s: failed to read temperature-overheat "
		"\n", __func__);
	return snprintf(buf, MAX_STR_PRINT, " Rd overheat Error\n");
}

static ssize_t nct1008_set_temp_overheat(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long int num;
	int err;
	u8 temp;
	long curr_temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_data *data = i2c_get_clientdata(client);
	char bufTemp[MAX_STR_PRINT];
	char bufOverheat[MAX_STR_PRINT];
	unsigned int ret;

	if (strict_strtol(buf, 0, &num)) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	if (((int)num < NCT1008_MIN_TEMP) || ((int)num >= NCT1008_MAX_TEMP)) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	/* check for system power down */
	err = nct1008_get_temp_common(EXT, data, &curr_temp);
	if (err)
		goto error;

	curr_temp = MILLICELSIUS_TO_CELSIUS(curr_temp);

	if (curr_temp >= (int)num) {
		ret = nct1008_show_temp(dev, attr, bufTemp);
		ret = nct1008_show_temp_overheat(dev, attr, bufOverheat);
		dev_err(dev, "\nCurrent temp: %s ", bufTemp);
		dev_err(dev, "\nOld overheat limit: %s ", bufOverheat);
		dev_err(dev, "\nReset from overheat: curr temp=%ld, new overheat temp=%d\n\n",
			curr_temp, (int)num);
	}

	/* External temperature h/w shutdown limit */
	temp = temperature_to_value(data->plat_data.extended_range, (s16)num);
	err = nct1008_write_reg(client, EXT_THERM_LIMIT_WR, temp);
	if (err < 0)
		goto error;

	/* Local temperature h/w shutdown limit */
	temp = temperature_to_value(data->plat_data.extended_range, (s16)num);
	err = nct1008_write_reg(client, LOC_THERM_LIMIT, temp);
	if (err < 0)
		goto error;

	data->plat_data.sensors[EXT].shutdown_limit = num;
	nct1008_setup_shutdown_warning(data);

	return count;
error:
	dev_err(dev, " %s: failed to set temperature-overheat\n", __func__);
	return err;
}

static ssize_t nct1008_show_temp_alert(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	int value;
	s16 temp_hi, temp_lo;
	/* External Temperature Throttling hi-limit */
	value = nct1008_read_reg(client, EXT_TEMP_HI_LIMIT_HI_BYTE_RD);
	if (value < 0)
		goto error;
	temp_hi = value_to_temperature(pdata->extended_range, value);

	/* External Temperature Throttling lo-limit */
	value = nct1008_read_reg(client, EXT_TEMP_LO_LIMIT_HI_BYTE_RD);
	if (value < 0)
		goto error;
	temp_lo = value_to_temperature(pdata->extended_range, value);

	return snprintf(buf, MAX_STR_PRINT, "lo:%d hi:%d\n", temp_lo, temp_hi);
error:
	dev_err(dev, "%s: failed to read temperature-alert\n", __func__);
	return snprintf(buf, MAX_STR_PRINT, " Rd alert Error\n");
}

static ssize_t nct1008_set_temp_alert(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long int num;
	int value;
	int err;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;

	if (strict_strtol(buf, 0, &num)) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	if (((int)num < NCT1008_MIN_TEMP) || ((int)num >= NCT1008_MAX_TEMP)) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}

	/* External Temperature Throttling limit */
	value = temperature_to_value(pdata->extended_range, (s16)num);
	err = nct1008_write_reg(client, EXT_TEMP_HI_LIMIT_HI_BYTE_WR, value);
	if (err < 0)
		goto error;

	/* Local Temperature Throttling limit */
	err = nct1008_write_reg(client, LOC_TEMP_HI_LIMIT_WR, value);
	if (err < 0)
		goto error;

	return count;
error:
	dev_err(dev, "%s: failed to set temperature-alert "
		"\n", __func__);
	return err;
}

static ssize_t nct1008_show_sensor_temp(int sensor, struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	s16 temp_value;
	int data = 0;
	int data_lo;

	if (!dev || !buf || !attr)
		return -EINVAL;

	/* When reading the full external temperature value, read the
	 * LSB first. This causes the MSB to be locked (that is, the
	 * ADC does not write to it) until it is read */
	data_lo = nct1008_read_reg(client, NCT_REG(sensor, TEMP_RD_LO));
	if (data_lo < 0) {
		dev_err(&client->dev, "%s: failed to read "
			"ext_temperature, i2c error=%d\n", __func__, data_lo);
		goto error;
	}

	data = nct1008_read_reg(client, NCT_REG(sensor, TEMP_RD_HI));
	if (data < 0) {
		dev_err(&client->dev, "%s: failed to read "
			"ext_temperature, i2c error=%d\n", __func__, data);
		goto error;
	}

	temp_value = value_to_temperature(pdata->extended_range, data);

	return snprintf(buf, MAX_STR_PRINT, "%d.%d\n", temp_value,
		(25 * (data_lo >> 6)));
error:
	return snprintf(buf, MAX_STR_PRINT, "Error read ext temperature\n");
}

static ssize_t pr_reg(struct nct1008_data *nct, char *buf, int max_s,
			const char *reg_name, int offset)
{
	int ret, sz = 0;

	ret = nct1008_read_reg(nct->client, offset);
	if (ret >= 0)
		sz += snprintf(buf + sz, PAGE_SIZE - sz,
				"%20s  0x%02x  0x%02x  0x%02x\n",
				reg_name, nct->client->addr, offset, ret);
	else
		sz += snprintf(buf + sz, PAGE_SIZE - sz,
				"%s: line=%d, i2c ** read error=%d **\n",
				__func__, __LINE__, ret);
	return sz;
}

static ssize_t nct1008_show_regs(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_data *nct = i2c_get_clientdata(client);
	char *name = nct->chip == NCT72 ? "nct72" : "nct1008";
	int sz = 0;

	sz += snprintf(buf + sz, PAGE_SIZE - sz,
		"%s Registers\n", name);
	sz += snprintf(buf + sz, PAGE_SIZE - sz,
		"---------------------------------------\n");
	sz += snprintf(buf + sz, PAGE_SIZE - sz, "%20s  %4s  %4s  %s\n",
		"Register Name       ", "Addr", "Reg", "Value");
	sz += snprintf(buf + sz, PAGE_SIZE - sz, "%20s  %4s  %4s  %s\n",
		"--------------------", "----", "----", "-----");
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Status              ", STATUS_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Configuration       ", CONFIG_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Conversion Rate     ", CONV_RATE_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Hysteresis          ", THERM_HYSTERESIS);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Consecutive Alert   ", COSECUTIVE_ALERT);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Local Temp Value    ", LOC_TEMP_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Local Temp Hi Limit ", LOC_TEMP_HI_LIMIT_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Local Temp Lo Limit ", LOC_TEMP_LO_LIMIT_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Local Therm Limit   ", LOC_THERM_LIMIT);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Value Hi   ", EXT_TEMP_HI_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Value Lo   ", EXT_TEMP_LO_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Hi Limit Hi", EXT_TEMP_HI_LIMIT_HI_BYTE_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Hi Limit Lo", EXT_TEMP_HI_LIMIT_LO_BYTE);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Lo Limit Hi", EXT_TEMP_LO_LIMIT_HI_BYTE_RD);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Lo Limit Lo", EXT_TEMP_LO_LIMIT_LO_BYTE);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Offset Hi  ", OFFSET_WR);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Temp Offset Lo  ", OFFSET_QUARTER_WR);
	sz += pr_reg(nct, buf+sz, PAGE_SIZE-sz,
		"Ext Therm Limit     ", EXT_THERM_LIMIT_WR);

	return sz;
}

static ssize_t nct1008_set_offsets(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_data *nct = i2c_get_clientdata(client);
	int index, temp, off;
	int rv = count;

	strim((char *)buf);
	sscanf(buf, "[%u] %u %d", &index, &temp, &off);

	if (index >= ARRAY_SIZE(nct->sensors[EXT].offset_table)) {
		pr_info("%s: invalid index [%d]\n", __func__, index);
		rv = -EINVAL;
	} else {
		nct->sensors[EXT].offset_table[index].temp = temp;
		nct->sensors[EXT].offset_table[index].offset = off;
	}

	return rv;
}

static ssize_t nct1008_show_offsets(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_data *nct = i2c_get_clientdata(client);
	char *name = nct->chip == NCT72 ? "nct72" : "nct1008";
	int i, sz = 0;

	sz += snprintf(buf + sz, PAGE_SIZE - sz,
				"%s offsets table\n", name);
	sz += snprintf(buf + sz, PAGE_SIZE - sz,
				"%2s  %4s  %s\n", " #", "temp", "offset");
	sz += snprintf(buf + sz, PAGE_SIZE - sz,
				"%2s  %4s  %s\n", "--", "----", "------");

	for (i = 0; i < ARRAY_SIZE(nct->sensors[EXT].offset_table); i++)
		sz += snprintf(buf + sz, PAGE_SIZE - sz,
				"%2d  %4d  %3d\n",
				i, nct->sensors[EXT].offset_table[i].temp,
				nct->sensors[EXT].offset_table[i].offset);
	return sz;
}


/* This function is used by the system to show the temperature. */
static ssize_t nct1008_show_ext_temp(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return nct1008_show_sensor_temp(EXT, dev, attr, buf);
}

static DEVICE_ATTR(temperature, S_IRUGO, nct1008_show_temp, NULL);
static DEVICE_ATTR(temperature_overheat, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		nct1008_show_temp_overheat, nct1008_set_temp_overheat);
static DEVICE_ATTR(temperature_alert, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		nct1008_show_temp_alert, nct1008_set_temp_alert);
static DEVICE_ATTR(ext_temperature, S_IRUGO, nct1008_show_ext_temp, NULL);
static DEVICE_ATTR(registers, S_IRUGO, nct1008_show_regs, NULL);
static DEVICE_ATTR(offsets, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		nct1008_show_offsets, nct1008_set_offsets);

static struct attribute *nct1008_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temperature_overheat.attr,
	&dev_attr_temperature_alert.attr,
	&dev_attr_ext_temperature.attr,
	&dev_attr_registers.attr,
	&dev_attr_offsets.attr,
	NULL
};

static const struct attribute_group nct1008_attr_group = {
	.attrs = nct1008_attributes,
};

static int nct1008_shutdown_warning_get_max_state(
					struct thermal_cooling_device *cdev,
					unsigned long *max_state)
{
	*max_state = 1;
	return 0;
}

static int nct1008_shutdown_warning_get_cur_state(
					struct thermal_cooling_device *cdev,
					unsigned long *cur_state)
{
	struct nct1008_data *data = cdev->devdata;
	long limit = data->plat_data.sensors[EXT].shutdown_limit * 1000;
	unsigned long temp;

	if (nct1008_get_temp_common(EXT, data, &temp))
		return -1;

	if (temp >= (limit - THERM_WARN_RANGE_HIGH_OFFSET))
		*cur_state = 1;
	else
		*cur_state = 0;

	return 0;
}

static int nct1008_shutdown_warning_set_cur_state(
					struct thermal_cooling_device *cdev,
					unsigned long cur_state)
{
	static long temp_sav;
	struct nct1008_data *data = cdev->devdata;
	long limit = data->plat_data.sensors[EXT].shutdown_limit * 1000;
	unsigned long temp;

	if (nct1008_get_temp_common(EXT, data, &temp))
		return -1;

	if ((temp >= (limit - THERM_WARN_RANGE_HIGH_OFFSET)) &&
		(temp != temp_sav)) {
		pr_warn("NCT%s: Warning: Temperature (%ld.%02ldC) is %s SHUTDOWN (%c)\n",
			(data->chip == NCT72) ? "72" : "1008",
			temp / 1000, (temp % 1000) / 10,
			temp > limit ? "above" :
			temp == limit ? "at" : "near",
			temp > temp_sav ? '^' : 'v');
		temp_sav = temp;
	}
	return 0;
}

static struct thermal_cooling_device_ops nct1008_shutdown_warning_ops = {
	.get_max_state = nct1008_shutdown_warning_get_max_state,
	.get_cur_state = nct1008_shutdown_warning_get_cur_state,
	.set_cur_state = nct1008_shutdown_warning_set_cur_state,
};

static int nct1008_thermal_set_limits(int sensor,
				      struct nct1008_data *data,
				      long lo_limit_milli,
				      long hi_limit_milli)
{
	int err;
	u8 value;
	bool extended_range = data->plat_data.extended_range;
	long lo_limit = MILLICELSIUS_TO_CELSIUS(lo_limit_milli);
	long hi_limit = MILLICELSIUS_TO_CELSIUS(hi_limit_milli);

	if (lo_limit >= hi_limit)
		return -EINVAL;

	if (data->sensors[sensor].current_lo_limit != lo_limit) {
		value = temperature_to_value(extended_range, lo_limit);
		pr_debug("%s: set lo_limit %ld\n", __func__, lo_limit);
		err = nct1008_write_reg(data->client,
				NCT_REG(sensor, TEMP_LO_LIMIT_WR), value);
		if (err)
			return err;

		data->sensors[sensor].current_lo_limit = lo_limit;
	}

	if (data->sensors[sensor].current_hi_limit != hi_limit) {
		value = temperature_to_value(extended_range, hi_limit);
		pr_debug("%s: set hi_limit %ld\n", __func__, hi_limit);
		err = nct1008_write_reg(data->client,
				NCT_REG(sensor, TEMP_HI_LIMIT_WR), value);
		if (err)
			return err;

		data->sensors[sensor].current_hi_limit = hi_limit;
	}

	if (sensor == LOC)
		pr_debug("NCT1008: LOC-sensor limits set to %ld - %ld\n",
			lo_limit, hi_limit);

	return 0;
}

#ifdef CONFIG_THERMAL
static void nct1008_update(int sensor, struct nct1008_data *data)
{
	struct thermal_zone_device *thz;
	long low_temp, high_temp;
	struct thermal_trip_info *trip_state;
	long temp, trip_temp, hysteresis_temp;
	int count;

	low_temp = 0, high_temp = NCT1008_MAX_TEMP * 1000;
	thz = data->sensors[sensor].thz;

	if (!thz)
		return;

	thermal_zone_device_update(thz);

	temp = thz->temperature;

	for (count = 0; count < thz->trips; count++) {
		trip_state = &data->plat_data.sensors[sensor].trips[count];
		trip_temp = trip_state->trip_temp;
		hysteresis_temp = trip_temp - trip_state->hysteresis;
		if ((trip_state->trip_type == THERMAL_TRIP_PASSIVE) &&
		    !trip_state->tripped)
			hysteresis_temp = trip_temp;

		if ((trip_temp > temp) && (trip_temp < high_temp))
			high_temp = trip_temp;

		if ((hysteresis_temp < temp) && (hysteresis_temp > low_temp))
			low_temp = hysteresis_temp;
	}

	nct1008_thermal_set_limits(sensor, data, low_temp, high_temp);
}

static int nct1008_ext_get_temp(struct thermal_zone_device *thz,
					unsigned long *temp)
{
	struct nct1008_data *data = thz->devdata;

	return nct1008_get_temp_common(EXT, data, temp);
}

static int nct1008_ext_bind(struct thermal_zone_device *thz,
			    struct thermal_cooling_device *cdev)
{
	struct nct1008_data *data = thz->devdata;
	int i;
	bool bind = false;
	int err;
	long temp;
	struct nct1008_sensor_platform_data *sensor;

	err = nct1008_get_temp_common(EXT, data, &temp);
	if (!err && (temp == NCT1008_MAX_TEMP_MILLI))
		err = -ENXIO; /* potential faulty skin-tdiode connection  */

	sensor = &data->plat_data.sensors[EXT];

	for (i = 0; i < sensor->num_trips; i++) {
		if (err && !strcmp(sensor->trips[i].cdev_type, "skin-balanced"))
			dev_info(&data->client->dev,
				 "No skin-tdiode flex connector found.\n");
		else if (!strcmp(sensor->trips[i].cdev_type,
				cdev->type) &&
			 !thermal_zone_bind_cooling_device(thz, i, cdev,
					sensor->trips[i].upper,
					sensor->trips[i].lower))
			bind = true;
	}

	if (bind)
		nct1008_update(EXT, data);

	return 0;
}


static int nct1008_unbind(int sensor,
				struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	struct nct1008_data *data = thz->devdata;
	int i;

	struct nct1008_sensor_platform_data *sensor_data;

	sensor_data = &data->plat_data.sensors[sensor];

	for (i = 0; i < sensor_data->num_trips; i++) {
		if (!strcmp(sensor_data->trips[i].cdev_type, cdev->type))
			thermal_zone_unbind_cooling_device(thz, i, cdev);
	}
	return 0;
}

/* Helper function that is called in order to unbind external sensor from the
   cooling device. */
static inline int nct1008_ext_unbind(struct thermal_zone_device *thz,
					struct thermal_cooling_device *cdev)
{
	return nct1008_unbind(EXT, thz, cdev);
}

/* Helper function that is called in order to unbind local sensor from the
   cooling device. */
static inline int nct1008_loc_unbind(struct thermal_zone_device *thz,
					struct thermal_cooling_device *cdev)
{
	return nct1008_unbind(LOC, thz, cdev);
}

/* This function reads the temperature value set for the given trip point. */
static int nct1008_get_trip_temp(int sensor,
					struct thermal_zone_device *thz,
					int trip,
					unsigned long *temp)
{
	struct nct1008_data *data = thz->devdata;
	struct thermal_trip_info *trip_state =
		&data->plat_data.sensors[sensor].trips[trip];

	*temp = trip_state->trip_temp;

	if (trip_state->trip_type != THERMAL_TRIP_PASSIVE)
		return 0;

	if (thz->temperature >= *temp) {
		trip_state->tripped = true;
	} else if (trip_state->tripped) {
		*temp -= trip_state->hysteresis;
		if (thz->temperature < *temp)
			trip_state->tripped = false;
	}

	return 0;
}

/* This function reads the temperature value set for the given trip point for
   the local sensor. */
static inline int nct1008_loc_get_trip_temp(struct thermal_zone_device *thz,
						int trip,
						unsigned long *temp)
{
	return nct1008_get_trip_temp(LOC, thz, trip, temp);
}

/* This function reads the temperature value set for the given trip point for
	the remote sensor. */
static inline int nct1008_ext_get_trip_temp(struct thermal_zone_device *thz,
						int trip,
						unsigned long *temp)
{
	return nct1008_get_trip_temp(EXT, thz, trip, temp);
}

/* This function allows setting trip point temperature for the sensor
   specified. */
static int nct1008_set_trip_temp(int sensor,
					struct thermal_zone_device *thz,
					int trip,
					unsigned long temp)
{
	struct nct1008_data *data = thz->devdata;

	data->plat_data.sensors[sensor].trips[trip].trip_temp = temp;
	nct1008_update(sensor, data);
	return 0;
}

/* This function allows setting trip point temperature for the local sensor. */
static inline int nct1008_loc_set_trip_temp(struct thermal_zone_device *thz,
						int trip,
						unsigned long temp)
{
	return nct1008_set_trip_temp(LOC, thz, trip, temp);
}

/* This function allows setting trip point temperature for the external
 * sensor. */
static inline int nct1008_ext_set_trip_temp(struct thermal_zone_device *thz,
						int trip,
						unsigned long temp)
{
	return nct1008_set_trip_temp(EXT, thz, trip, temp);
}

/* This function return the trip point type for the sensor specified. */
static int nct1008_get_trip_type(int sensor,
					struct thermal_zone_device *thz,
					int trip,
					enum thermal_trip_type *type)
{
	struct nct1008_data *data = thz->devdata;

	*type = data->plat_data.sensors[sensor].trips[trip].trip_type;
	return 0;
}

/* This function return the trip point type for the local sensor. */
static inline int nct1008_loc_get_trip_type(struct thermal_zone_device *thz,
						int trip,
						enum thermal_trip_type *type)
{
	return nct1008_get_trip_type(LOC, thz, trip, type);
}

/* This function return the trip point type for the external sensor. */
static inline int nct1008_ext_get_trip_type(struct thermal_zone_device *thz,
						int trip,
						enum thermal_trip_type *type)
{
	return nct1008_get_trip_type(EXT, thz, trip, type);
}

/* This function returns value of trend for the temperature change, depending
   on the trip point type. */
static int nct1008_get_trend(int sensor,
				struct thermal_zone_device *thz,
				int trip,
				enum thermal_trend *trend)
{
	struct nct1008_data *data = thz->devdata;
	struct thermal_trip_info *trip_state;

	trip_state = &data->plat_data.sensors[sensor].trips[trip];

	switch (trip_state->trip_type) {
	case THERMAL_TRIP_ACTIVE:
		if (data->sensors[sensor].temp >= trip_state->trip_temp)
			*trend = THERMAL_TREND_RAISING;
		else
			*trend = THERMAL_TREND_DROPPING;
		break;
	case THERMAL_TRIP_PASSIVE:
		if (data->sensors[sensor].temp > trip_state->trip_temp)
			*trend = THERMAL_TREND_RAISING;
		else
			*trend = THERMAL_TREND_DROPPING;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* Helper function to get trend for the local sensor. */
static inline int nct1008_loc_get_trend(struct thermal_zone_device *thz,
						int trip,
						enum thermal_trend *trend)
{
	return nct1008_get_trend(LOC, thz, trip, trend);
}
/* Helper function to get trend for the external sensor. */
static inline int nct1008_ext_get_trend(struct thermal_zone_device *thz,
						int trip,
						enum thermal_trend *trend)
{
	return nct1008_get_trend(EXT, thz, trip, trend);
}

/* Helper function to get temperature of the local sensor. */
static int nct1008_loc_get_temp(struct thermal_zone_device *thz,
					unsigned long *temp)
{
	struct nct1008_data *data = thz->devdata;

	return nct1008_get_temp_common(LOC, data, temp);
}

/* Helper function to bind local sensor with the cooling device specified. */
static int nct1008_loc_bind(struct thermal_zone_device *thz,
				struct thermal_cooling_device *cdev)
{
	struct nct1008_data *data = thz->devdata;
	int i;
	bool bind = false;
	struct nct1008_sensor_platform_data *sensor_data;

	pr_debug("NCT1008: LOC-sensor bind %s, %s attempt\n",
		thz->type, cdev->type);

	sensor_data = &data->plat_data.sensors[LOC];

	for (i = 0; i < sensor_data->num_trips; i++) {
		if (!strcmp(sensor_data->trips[i].cdev_type, cdev->type) &&
			!thermal_zone_bind_cooling_device(thz, i, cdev,
				sensor_data->trips[i].upper,
				sensor_data->trips[i].lower)){
			bind = true;
			break;
		}
	}

	if (bind) {
		nct1008_update(LOC, data);
		pr_debug("NCT1008: LOC-sensor bind %s, %s done\n",
			thz->type, cdev->type);
	}

	return 0;
}

static struct thermal_zone_device_ops nct_loc_ops = {
	.get_temp = nct1008_loc_get_temp,
	.bind = nct1008_loc_bind,
	.unbind = nct1008_loc_unbind,
	.get_trip_type = nct1008_loc_get_trip_type,
	.get_trip_temp = nct1008_loc_get_trip_temp,
	.set_trip_temp = nct1008_loc_set_trip_temp,
	.get_trend = nct1008_loc_get_trend,
};

static struct thermal_zone_device_ops nct_ext_ops = {
	.get_temp = nct1008_ext_get_temp,
	.bind = nct1008_ext_bind,
	.unbind = nct1008_ext_unbind,
	.get_trip_type = nct1008_ext_get_trip_type,
	.get_trip_temp = nct1008_ext_get_trip_temp,
	.set_trip_temp = nct1008_ext_set_trip_temp,
	.get_trend = nct1008_ext_get_trend,
};
#else
static void nct1008_update(nct1008_sensors sensor, struct nct1008_data *data)
{
}
#endif /* CONFIG_THERMAL */

static int nct1008_enable(struct i2c_client *client)
{
	struct nct1008_data *data = i2c_get_clientdata(client);
	int err;

	err = nct1008_write_reg(client, CONFIG_WR, data->config);
	if (err < 0)
		dev_err(&client->dev, "%s, line=%d, i2c write error=%d\n",
		__func__, __LINE__, err);
	return err;
}

static int nct1008_disable(struct i2c_client *client)
{
	struct nct1008_data *data = i2c_get_clientdata(client);
	int err;

	err = nct1008_write_reg(client, CONFIG_WR,
				data->config | STANDBY_BIT);
	if (err < 0)
		dev_err(&client->dev, "%s, line=%d, i2c write error=%d\n",
		__func__, __LINE__, err);
	return err;
}

static void nct1008_work_func(struct work_struct *work)
{
	struct nct1008_data *data = container_of(work, struct nct1008_data,
						work);
	int err;
	struct timespec ts;
	int intr_status;

	mutex_lock(&data->mutex);
	if (data->stop_workqueue) {
		mutex_unlock(&data->mutex);
		return;
	}
	mutex_unlock(&data->mutex);

	err = nct1008_disable(data->client);
	if (err == -ENODEV)
		return;

	intr_status = nct1008_read_reg(data->client, STATUS_RD);
	pr_debug("NCT1008: interruption (0x%08x)\n", intr_status);

	if (intr_status & (LOC_LO_BIT | LOC_HI_BIT)) {
		pr_debug("NCT1008: LOC-sensor is not within limits\n");
		nct1008_update(LOC, data);
	}

	if (intr_status & (EXT_LO_BIT | EXT_HI_BIT))
		nct1008_update(EXT, data);

	/* Initiate one-shot conversion */
	nct1008_write_reg(data->client, ONE_SHOT, 0x1);

	/* Give hardware necessary time to finish conversion */
	ts = ns_to_timespec(MAX_CONV_TIME_ONESHOT_MS * 1000 * 1000);
	hrtimer_nanosleep(&ts, NULL, HRTIMER_MODE_REL, CLOCK_MONOTONIC);

	nct1008_read_reg(data->client, STATUS_RD);

	nct1008_enable(data->client);

	enable_irq(data->client->irq);
}

static irqreturn_t nct1008_irq(int irq, void *dev_id)
{
	struct nct1008_data *data = dev_id;

	disable_irq_nosync(irq);
	queue_work(data->workqueue, &data->work);

	return IRQ_HANDLED;
}

static void nct1008_power_control(struct nct1008_data *data, bool is_enable)
{
	int ret;
	mutex_lock(&data->mutex);
	if (!data->nct_reg) {
		data->nct_reg = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR_OR_NULL(data->nct_reg)) {
			if (PTR_ERR(data->nct_reg) == -ENODEV)
				dev_info(&data->client->dev,
					"no regulator found for vdd."
					" Assuming vdd is always powered");
			else
				dev_warn(&data->client->dev, "Error [%ld] in "
					"getting the regulator handle for"
					" vdd\n", PTR_ERR(data->nct_reg));
			data->nct_reg = NULL;
			mutex_unlock(&data->mutex);
			return;
		}
	}
	if (is_enable)
		ret = regulator_enable(data->nct_reg);
	else
		ret = regulator_disable(data->nct_reg);

	if (ret < 0)
		dev_err(&data->client->dev, "Error in %s rail vdd_nct%s, "
			"error %d\n", (is_enable) ? "enabling" : "disabling",
			(data->chip == NCT72) ? "72" : "1008",
			ret);
	else
		dev_info(&data->client->dev, "success in %s rail vdd_nct%s\n",
			(is_enable) ? "enabling" : "disabling",
			(data->chip == NCT72) ? "72" : "1008");
	data->nct_disabled = !is_enable;
	mutex_unlock(&data->mutex);
}

static void nct1008_setup_shutdown_warning(struct nct1008_data *data)
{
	static struct thermal_cooling_device *cdev;
	long limit = data->plat_data.sensors[EXT].shutdown_limit * 1000;
	long warn_temp = limit - THERM_WARN_RANGE_HIGH_OFFSET;
	int i;
	struct nct1008_sensor_platform_data *sensor_data;

	if (cdev)
		thermal_cooling_device_unregister(cdev);
	cdev = thermal_cooling_device_register("shutdown_warning", data,
					&nct1008_shutdown_warning_ops);
	if (IS_ERR_OR_NULL(cdev)) {
		cdev = NULL;
		return;
	}

	sensor_data = &data->plat_data.sensors[EXT];

	for (i = 0; i < sensor_data->num_trips; i++) {
		if (!strcmp(sensor_data->trips[i].cdev_type,
						"shutdown_warning")) {
			sensor_data->trips[i].trip_temp = warn_temp;
			nct1008_update(EXT, data);
			break;
		}
	}

	pr_info("NCT%s: Register overheat warning at %ld.%02ldC\n",
			(data->chip == NCT72) ? "72" : "1008",
			warn_temp / 1000, (warn_temp % 1000) / 10);
}

static int nct1008_configure_sensor(struct nct1008_data *data)
{
	struct i2c_client *client = data->client;
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	u8 value;
	s16 temp;
	u8 temp2;
	int ret;

	if (!pdata || !pdata->supported_hwrev)
		return -ENODEV;

	/* Initially place in Standby */
	ret = nct1008_write_reg(client, CONFIG_WR, STANDBY_BIT);
	if (ret)
		goto error;

	/* External temperature h/w shutdown limit. */
	value = temperature_to_value(pdata->extended_range,
					pdata->sensors[EXT].shutdown_limit);
	ret = nct1008_write_reg(client, EXT_THERM_LIMIT_WR, value);
	if (ret)
		goto error;

	/* Local temperature h/w shutdown limit */
	value = temperature_to_value(pdata->extended_range,
					pdata->sensors[LOC].shutdown_limit);
	ret = nct1008_write_reg(client, LOC_THERM_LIMIT, value);
	if (ret)
		goto error;

	/* set extended range mode if needed */
	if (pdata->extended_range)
		data->config |= EXTENDED_RANGE_BIT;
	data->config &= ~(THERM2_BIT | ALERT_BIT);

	ret = nct1008_write_reg(client, CONFIG_WR, data->config);
	if (ret)
		goto error;

	/* Temperature conversion rate */
	ret = nct1008_write_reg(client, CONV_RATE_WR, pdata->conv_rate);
	if (ret)
		goto error;

	data->conv_period_ms = conv_period_ms_table[pdata->conv_rate];

	/* Setup local hi and lo limits. */
	ret = nct1008_write_reg(client, LOC_TEMP_HI_LIMIT_WR, NCT1008_MAX_TEMP);
	if (ret)
		goto error;

	ret = nct1008_write_reg(client, LOC_TEMP_LO_LIMIT_WR, 0);
	if (ret)
		goto error;

	/* Setup external hi and lo limits */
	ret = nct1008_write_reg(client, EXT_TEMP_LO_LIMIT_HI_BYTE_WR, 0);
	if (ret)
		goto error;
	ret = nct1008_write_reg(client, EXT_TEMP_HI_LIMIT_HI_BYTE_WR,
			NCT1008_MAX_TEMP);
	if (ret)
		goto error;

	/* read initial temperature */
	ret = nct1008_read_reg(client, LOC_TEMP_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;

	temp = value_to_temperature(pdata->extended_range, value);
	dev_dbg(&client->dev, "\n initial local temp = %d ", temp);

	ret = nct1008_read_reg(client, EXT_TEMP_LO_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;

	temp2 = (value >> 6);
	ret = nct1008_read_reg(client, EXT_TEMP_HI_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;

	temp = value_to_temperature(pdata->extended_range, value);

	if (temp2 > 0)
		dev_dbg(&client->dev, "\n initial ext temp = %d.%d deg",
				temp, temp2 * 25);
	else
		dev_dbg(&client->dev, "\n initial ext temp = %d.0 deg", temp);

	/* Remote channel offset */
	ret = nct1008_write_reg(client, OFFSET_WR, pdata->offset / 4);
	if (ret < 0)
		goto error;

	/* Remote channel offset fraction (quarters) */
	ret = nct1008_write_reg(client, OFFSET_QUARTER_WR,
					(pdata->offset % 4) << 6);
	if (ret < 0)
		goto error;

	/* Reset current hi/lo limit values with register values */
	ret = nct1008_read_reg(data->client, EXT_TEMP_LO_LIMIT_HI_BYTE_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;
	data->sensors[EXT].current_lo_limit =
		value_to_temperature(pdata->extended_range, value);

	ret = nct1008_read_reg(data->client, EXT_TEMP_HI_LIMIT_HI_BYTE_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;

	data->sensors[EXT].current_hi_limit =
		value_to_temperature(pdata->extended_range, value);

	ret = nct1008_read_reg(data->client, LOC_TEMP_LO_LIMIT_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;

	data->sensors[LOC].current_lo_limit =
		value_to_temperature(pdata->extended_range, value);

	value = nct1008_read_reg(data->client, LOC_TEMP_HI_LIMIT_RD);
	if (ret < 0)
		goto error;
	else
		value = ret;

	data->sensors[LOC].current_hi_limit =
		value_to_temperature(pdata->extended_range, value);

	nct1008_setup_shutdown_warning(data);

	return 0;
error:
	dev_err(&client->dev, "\n exit %s, err=%d ", __func__, ret);
	return ret;
}

static int nct1008_configure_irq(struct nct1008_data *data)
{
	data->workqueue = create_singlethread_workqueue((data->chip == NCT72) \
							? "nct72" : "nct1008");

	INIT_WORK(&data->work, nct1008_work_func);

	if (data->client->irq < 0)
		return 0;
	else
		return request_irq(data->client->irq, nct1008_irq,
			IRQF_TRIGGER_LOW,
			(data->chip == NCT72) ? "nct72" : "nct1008",
			data);
}

/*
 * Manufacturer(OnSemi) recommended sequence for
 * Extended Range mode is as follows
 * 1. Place in Standby
 * 2. Scale the THERM and ALERT limits
 *	appropriately(for Extended Range mode).
 * 3. Enable Extended Range mode.
 *	ALERT mask/THERM2 mode may be done here
 *	as these are not critical
 * 4. Set Conversion Rate as required
 * 5. Take device out of Standby
 */

/*
 * function nct1008_probe takes care of initial configuration
 */
static int nct1008_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct nct1008_data *data;
	int err;
	int i;
	u64 mask = 0;
	char nct_loc_name[THERMAL_NAME_LENGTH];
	char nct_ext_name[THERMAL_NAME_LENGTH];
	struct nct1008_sensor_platform_data *sensor_data;

	data = kzalloc(sizeof(struct nct1008_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	data->chip = id->driver_data;
	memcpy(&data->plat_data, client->dev.platform_data,
		sizeof(struct nct1008_platform_data));
	i2c_set_clientdata(client, data);
	mutex_init(&data->mutex);

	nct1008_power_control(data, true);
	if (!data->nct_reg) {
		/* power up failure */
		err = -EIO;
		goto cleanup;
	}
	/* extended range recommended steps 1 through 4 taken care
	 * in nct1008_configure_sensor function */
	err = nct1008_configure_sensor(data);	/* sensor is in standby */
	if (err < 0) {
		dev_err(&client->dev, "\n error file: %s : %s(), line=%d ",
			__FILE__, __func__, __LINE__);
		goto error;
	}

	err = nct1008_configure_irq(data);
	if (err < 0) {
		dev_err(&client->dev, "\n error file: %s : %s(), line=%d ",
			__FILE__, __func__, __LINE__);
		goto error;
	}
	dev_info(&client->dev, "%s: initialized\n", __func__);

	/* extended range recommended step 5 is in nct1008_enable function */
	err = nct1008_enable(client);		/* sensor is running */
	if (err < 0) {
		dev_err(&client->dev, "Error: %s, line=%d, error=%d\n",
			__func__, __LINE__, err);
		goto error;
	}

	/* register sysfs hooks */
	err = sysfs_create_group(&client->dev.kobj, &nct1008_attr_group);
	if (err < 0) {
		dev_err(&client->dev, "\n sysfs create err=%d ", err);
		goto error;
	}

#ifdef CONFIG_THERMAL
	if (data->plat_data.loc_name) {
		strcpy(nct_loc_name, "Tboard_");
		strcpy(nct_ext_name, "Tdiode_");
		strncat(nct_loc_name, data->plat_data.loc_name,
			(THERMAL_NAME_LENGTH - strlen("Tboard_")) - 1);
		strncat(nct_ext_name, data->plat_data.loc_name,
			(THERMAL_NAME_LENGTH - strlen("Tdiode_")) - 1);
	} else {
		strcpy(nct_loc_name, "Tboard");
		strcpy(nct_ext_name, "Tdiode");
	}

	sensor_data = &data->plat_data.sensors[LOC];

	/* Config for the Local sensor. */
	mask = 0;
	for (i = 0; i < sensor_data->num_trips; i++)
		mask |= ((data->plat_data.sensors[LOC].trips[i].mask > 0) << i);

	data->sensors[LOC].thz = thermal_zone_device_register(
		nct_loc_name,
		sensor_data->num_trips,
		mask,
		data,
		&nct_loc_ops,
		sensor_data->tzp,
		2000,
		0);

	if (IS_ERR_OR_NULL(data->sensors[LOC].thz))
		goto error;

	/* Config for the External sensor. */
	mask = 0;
	for (i = 0; i < data->plat_data.sensors[EXT].num_trips; i++)
		mask |= ((data->plat_data.sensors[EXT].trips[i].mask > 0) << i);

	data->sensors[EXT].thz = thermal_zone_device_register(nct_ext_name,
		data->plat_data.sensors[EXT].num_trips,
		mask,
		data,
		&nct_ext_ops,
		data->plat_data.sensors[EXT].tzp,
		data->plat_data.sensors[EXT].passive_delay,
		data->plat_data.sensors[EXT].polling_delay);

	if (IS_ERR_OR_NULL(data->sensors[EXT].thz)) {
		thermal_zone_device_unregister(data->sensors[LOC].thz);
		data->sensors[LOC].thz = NULL;
		goto error;
	}

	nct1008_update(EXT, data);
	nct1008_update(LOC, data);
#endif
	return 0;

error:
	dev_err(&client->dev, "\n exit %s, err=%d ", __func__, err);
	nct1008_power_control(data, false);
cleanup:
	mutex_destroy(&data->mutex);
	if (data->nct_reg)
		regulator_put(data->nct_reg);
	kfree(data);
	return err;
}

static int nct1008_remove(struct i2c_client *client)
{
	struct nct1008_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	data->stop_workqueue = 1;
	mutex_unlock(&data->mutex);
	cancel_work_sync(&data->work);
	free_irq(data->client->irq, data);
	sysfs_remove_group(&client->dev.kobj, &nct1008_attr_group);
	nct1008_power_control(data, false);
	if (data->nct_reg)
		regulator_put(data->nct_reg);
	mutex_destroy(&data->mutex);
	kfree(data);

	return 0;
}

static void nct1008_shutdown(struct i2c_client *client)
{
	struct nct1008_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	data->stop_workqueue = 1;
	mutex_unlock(&data->mutex);
	cancel_work_sync(&data->work);
	if (client->irq)
		disable_irq(client->irq);

	mutex_lock(&data->mutex);
	data->nct_disabled = 1;
	mutex_unlock(&data->mutex);
}

#ifdef CONFIG_PM_SLEEP
static int nct1008_suspend_powerdown(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int err;
	struct nct1008_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	data->stop_workqueue = 1;
	mutex_unlock(&data->mutex);
	cancel_work_sync(&data->work);
	disable_irq(client->irq);
	err = nct1008_disable(client);
	nct1008_power_control(data, false);
	return err;
}

static int nct1008_suspend_wakeup(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int err;
	struct nct1008_data *data = i2c_get_clientdata(client);
	long temp;
	int sensor_nr;
	struct nct1008_sensor_platform_data *sensor_data;

	for (sensor_nr = 0; sensor_nr < SENSORS_COUNT; sensor_nr++) {
		sensor_data = &data->plat_data.sensors[sensor_nr];

		err = nct1008_get_temp_common(sensor_nr, data, &temp);

		if (err)
			goto error;

		if (temp > sensor_data->suspend_limit_lo)
			err = nct1008_thermal_set_limits(sensor_nr, data,
				sensor_data->suspend_limit_lo,
				NCT1008_MAX_TEMP * 1000);
		else
			err = nct1008_thermal_set_limits(sensor_nr, data,
				NCT1008_MIN_TEMP * 1000,
				sensor_data->suspend_limit_hi);

		if (err)
			goto error;
	}

	/* Enable NCT wake. */
	err = enable_irq_wake(client->irq);
	if (err)
		dev_err(&client->dev, "Error: %s, error=%d. failed to enable NCT wakeup\n",
			__func__, err);
	return err;

error:
	dev_err(&client->dev, "\n error in file=: %s %s() line=%d: "
		"error=%d. Can't set correct LP1 alarm limits or set wakeup irq, "
		"shutting down device", __FILE__, __func__, __LINE__, err);

	return nct1008_suspend_powerdown(dev);
}

static int nct1008_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_data *data = i2c_get_clientdata(client);

	if (data->plat_data.suspend_with_wakeup &&
		data->plat_data.suspend_with_wakeup())
		return nct1008_suspend_wakeup(dev);
	else
		return nct1008_suspend_powerdown(dev);
}


static int nct1008_resume_wakeup(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);

	err = disable_irq_wake(client->irq);
	if (err) {
		dev_err(&client->dev, "Error: %s, error=%d. failed to disable NCT "
				"wakeup\n", __func__, err);
		return err;
	}

	/* NCT wasn't powered down, so IRQ is still enabled. */
	/* Disable it before calling update */
	disable_irq(client->irq);

	return err;
}

static int nct1008_resume_powerdown(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int err = 0;
	struct nct1008_data *data = i2c_get_clientdata(client);

	nct1008_power_control(data, true);
	nct1008_configure_sensor(data);
	err = nct1008_enable(client);
	if (err < 0) {
		dev_err(&client->dev, "Error: %s, error=%d\n",
			__func__, err);
		return err;
	}

	return err;
}

static int nct1008_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int err;
	struct nct1008_data *data = i2c_get_clientdata(client);

	if (data->plat_data.suspend_with_wakeup &&
		data->plat_data.suspend_with_wakeup())
		err = nct1008_resume_wakeup(dev);
	else
		err = nct1008_resume_powerdown(dev);

	if (err)
		return err;

	nct1008_update(LOC, data);
	nct1008_update(EXT, data);
	mutex_lock(&data->mutex);
	data->stop_workqueue = 0;
	mutex_unlock(&data->mutex);
	enable_irq(client->irq);

	return 0;
}

static const struct dev_pm_ops nct1008_pm_ops = {
	.suspend	= nct1008_suspend,
	.resume		= nct1008_resume,
};

#endif

static const struct i2c_device_id nct1008_id[] = {
	{ "nct1008", NCT1008 },
	{ "nct72", NCT72},
	{}
};
MODULE_DEVICE_TABLE(i2c, nct1008_id);

static struct i2c_driver nct1008_driver = {
	.driver = {
		.name	= "nct1008_nct72",
#ifdef CONFIG_PM_SLEEP
		.pm = &nct1008_pm_ops,
#endif
	},
	.probe		= nct1008_probe,
	.remove		= nct1008_remove,
	.id_table	= nct1008_id,
	.shutdown	= nct1008_shutdown,
};

static int __init nct1008_init(void)
{
	return i2c_add_driver(&nct1008_driver);
}

static void __exit nct1008_exit(void)
{
	i2c_del_driver(&nct1008_driver);
}

MODULE_DESCRIPTION("Temperature sensor driver for OnSemi NCT1008/NCT72");
MODULE_LICENSE("GPL");

module_init(nct1008_init);
module_exit(nct1008_exit);

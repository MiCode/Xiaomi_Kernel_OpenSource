/*
 * drivers/misc/nct1008.c
 *
 * Driver for NCT1008, temperature monitoring device from ON Semiconductors
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION. All rights reserved.
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

/* Register Addresses */
#define LOCAL_TEMP_RD			0x00
#define EXT_TEMP_RD_HI			0x01
#define EXT_TEMP_RD_LO			0x10
#define STATUS_RD			0x02
#define CONFIG_RD			0x03

#define LOCAL_TEMP_HI_LIMIT_RD		0x05
#define LOCAL_TEMP_LO_LIMIT_RD		0x06

#define EXT_TEMP_HI_LIMIT_HI_BYTE_RD	0x07
#define EXT_TEMP_LO_LIMIT_HI_BYTE_RD	0x08

#define CONFIG_WR			0x09
#define CONV_RATE_WR			0x0A
#define LOCAL_TEMP_HI_LIMIT_WR		0x0B
#define LOCAL_TEMP_LO_LIMIT_WR		0x0C
#define EXT_TEMP_HI_LIMIT_HI_BYTE_WR	0x0D
#define EXT_TEMP_LO_LIMIT_HI_BYTE_WR	0x0E
#define ONE_SHOT			0x0F
#define OFFSET_WR			0x11
#define OFFSET_QUARTER_WR		0x12
#define EXT_THERM_LIMIT_WR		0x19
#define LOCAL_THERM_LIMIT_WR		0x20
#define THERM_HYSTERESIS_WR		0x21

/* Configuration Register Bits */
#define EXTENDED_RANGE_BIT		BIT(2)
#define THERM2_BIT			BIT(5)
#define STANDBY_BIT			BIT(6)
#define ALERT_BIT			BIT(7)

/* Max Temperature Measurements */
#define EXTENDED_RANGE_OFFSET		64U
#define NCT1008_MIN_TEMP(extended)	(extended ? -64 : 0)
#define NCT1008_MAX_TEMP(extended)	(extended ? 191 : 127)

#define MAX_STR_PRINT 50

#define MAX_CONV_TIME_ONESHOT_MS (52)
#define CELSIUS_TO_MILLICELSIUS(x) ((x)*1000)
#define MILLICELSIUS_TO_CELSIUS(x) ((x)/1000)

struct nct1008_data {
	struct workqueue_struct *workqueue;
	struct work_struct work;
	struct i2c_client *client;
	struct nct1008_platform_data plat_data;
	struct mutex mutex;
	struct dentry *dent;
	u8 config;
	enum nct1008_chip chip;
	struct regulator *nct_reg;
	long current_lo_limit;
	long current_hi_limit;
	int conv_period_ms;
	long etemp;
	int shutdown_complete;

	struct thermal_zone_device *nct_int;
	struct thermal_zone_device *nct_ext;
};

static const struct i2c_device_id nct1008_id[] = {
	{ "nct1008", NCT1008 },
	{ "nct72", NCT72},
	{ "nct218", NCT218 },
	{}
};

static int conv_period_ms_table[] =
	{16000, 8000, 4000, 2000, 1000, 500, 250, 125, 63, 32, 16};

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
	if (data && data->shutdown_complete) {
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
	if (data && data->shutdown_complete) {
		mutex_unlock(&data->mutex);
		return -ENODEV;
	}

	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&data->mutex);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int nct1008_get_temp(struct device *dev, long *etemp, long *itemp)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	s16 temp_local;
	u8 temp_ext_lo;
	s16 temp_ext_hi;
	long temp_ext_milli;
	long temp_local_milli;
	u8 value;

	/* Read Local Temp */
	if (itemp) {
		value = nct1008_read_reg(client, LOCAL_TEMP_RD);
		if (value < 0)
			goto error;
		temp_local = value_to_temperature(pdata->ext_range, value);
		temp_local_milli = CELSIUS_TO_MILLICELSIUS(temp_local);

		*itemp = temp_local_milli;
	}

	/* Read External Temp */
	if (etemp) {
		value = nct1008_read_reg(client, EXT_TEMP_RD_LO);
		if (value < 0)
			goto error;
		temp_ext_lo = (value >> 6);

		value = nct1008_read_reg(client, EXT_TEMP_RD_HI);
		if (value < 0)
			goto error;
		temp_ext_hi = value_to_temperature(pdata->ext_range, value);

		temp_ext_milli = CELSIUS_TO_MILLICELSIUS(temp_ext_hi) +
					temp_ext_lo * 250;

		*etemp = temp_ext_milli;
	}

	return 0;
error:
	dev_err(&client->dev, "\n error in file=: %s %s() line=%d: "
		"error=%d ", __FILE__, __func__, __LINE__, value);
	return value;
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

	value = nct1008_read_reg(client, LOCAL_TEMP_RD);
	if (value < 0)
		goto error;
	temp1 = value_to_temperature(pdata->ext_range, value);

	value = nct1008_read_reg(client, EXT_TEMP_RD_LO);
	if (value < 0)
		goto error;
	temp2 = (value >> 6);
	value = nct1008_read_reg(client, EXT_TEMP_RD_HI);
	if (value < 0)
		goto error;
	temp = value_to_temperature(pdata->ext_range, value);

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
	value = nct1008_read_reg(client, LOCAL_THERM_LIMIT_WR);
	if (value < 0)
		goto error;
	temp = value_to_temperature(pdata->ext_range, value);

	/* External temperature h/w shutdown limit */
	value = nct1008_read_reg(client, EXT_THERM_LIMIT_WR);
	if (value < 0)
		goto error;
	temp2 = value_to_temperature(pdata->ext_range, value);

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
	long currTemp;
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	char bufTemp[MAX_STR_PRINT];
	char bufOverheat[MAX_STR_PRINT];
	unsigned int ret;

	if (strict_strtol(buf, 0, &num)) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	if (((int)num < NCT1008_MIN_TEMP(pdata->ext_range)) ||
			((int)num >= NCT1008_MAX_TEMP(pdata->ext_range))) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}
	/* check for system power down */
	err = nct1008_get_temp(dev, &currTemp, NULL);
	if (err)
		goto error;

	currTemp = MILLICELSIUS_TO_CELSIUS(currTemp);

	if (currTemp >= (int)num) {
		ret = nct1008_show_temp(dev, attr, bufTemp);
		ret = nct1008_show_temp_overheat(dev, attr, bufOverheat);
		dev_err(dev, "\nCurrent temp: %s ", bufTemp);
		dev_err(dev, "\nOld overheat limit: %s ", bufOverheat);
		dev_err(dev, "\nReset from overheat: curr temp=%ld, "
			"new overheat temp=%d\n\n", currTemp, (int)num);
	}

	/* External temperature h/w shutdown limit */
	temp = temperature_to_value(pdata->ext_range, (s16)num);
	err = nct1008_write_reg(client, EXT_THERM_LIMIT_WR, temp);
	if (err < 0)
		goto error;

	/* Local temperature h/w shutdown limit */
	temp = temperature_to_value(pdata->ext_range, (s16)num);
	err = nct1008_write_reg(client, LOCAL_THERM_LIMIT_WR, temp);
	if (err < 0)
		goto error;
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
	temp_hi = value_to_temperature(pdata->ext_range, value);

	/* External Temperature Throttling lo-limit */
	value = nct1008_read_reg(client, EXT_TEMP_LO_LIMIT_HI_BYTE_RD);
	if (value < 0)
		goto error;
	temp_lo = value_to_temperature(pdata->ext_range, value);

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
	if (((int)num < NCT1008_MIN_TEMP(pdata->ext_range)) ||
			((int)num >= NCT1008_MAX_TEMP(pdata->ext_range))) {
		dev_err(dev, "\n file: %s, line=%d return %s() ", __FILE__,
			__LINE__, __func__);
		return -EINVAL;
	}

	/* External Temperature Throttling limit */
	value = temperature_to_value(pdata->ext_range, (s16)num);
	err = nct1008_write_reg(client, EXT_TEMP_HI_LIMIT_HI_BYTE_WR, value);
	if (err < 0)
		goto error;

	/* Local Temperature Throttling limit */
	err = nct1008_write_reg(client, LOCAL_TEMP_HI_LIMIT_WR, value);
	if (err < 0)
		goto error;

	return count;
error:
	dev_err(dev, "%s: failed to set temperature-alert "
		"\n", __func__);
	return err;
}

static ssize_t nct1008_show_ext_temp(struct device *dev,
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
	data_lo = nct1008_read_reg(client, EXT_TEMP_RD_LO);
	if (data_lo < 0) {
		dev_err(&client->dev, "%s: failed to read "
			"ext_temperature, i2c error=%d\n", __func__, data_lo);
		goto error;
	}

	data = nct1008_read_reg(client, EXT_TEMP_RD_HI);
	if (data < 0) {
		dev_err(&client->dev, "%s: failed to read "
			"ext_temperature, i2c error=%d\n", __func__, data);
		goto error;
	}

	temp_value = value_to_temperature(pdata->ext_range, data);

	return snprintf(buf, MAX_STR_PRINT, "%d.%d\n", temp_value,
		(25 * (data_lo >> 6)));
error:
	return snprintf(buf, MAX_STR_PRINT, "Error read ext temperature\n");
}

static DEVICE_ATTR(temperature, S_IRUGO, nct1008_show_temp, NULL);
static DEVICE_ATTR(temperature_overheat, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		nct1008_show_temp_overheat, nct1008_set_temp_overheat);
static DEVICE_ATTR(temperature_alert, (S_IRUGO | (S_IWUSR | S_IWGRP)),
		nct1008_show_temp_alert, nct1008_set_temp_alert);
static DEVICE_ATTR(ext_temperature, S_IRUGO, nct1008_show_ext_temp, NULL);

static struct attribute *nct1008_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_temperature_overheat.attr,
	&dev_attr_temperature_alert.attr,
	&dev_attr_ext_temperature.attr,
	NULL
};

static const struct attribute_group nct1008_attr_group = {
	.attrs = nct1008_attributes,
};

static int nct1008_thermal_set_limits(struct nct1008_data *data,
				      long lo_limit_milli,
				      long hi_limit_milli)
{
	int err;
	u8 value;
	bool extended_range = data->plat_data.ext_range;
	long lo_limit = MILLICELSIUS_TO_CELSIUS(lo_limit_milli);
	long hi_limit = MILLICELSIUS_TO_CELSIUS(hi_limit_milli);

	if (lo_limit >= hi_limit)
		return -EINVAL;

	if (data->current_lo_limit != lo_limit) {
		value = temperature_to_value(extended_range, lo_limit);
		pr_debug("%s: set lo_limit %ld\n", __func__, lo_limit);
		err = nct1008_write_reg(data->client,
				EXT_TEMP_LO_LIMIT_HI_BYTE_WR, value);
		if (err)
			return err;

		data->current_lo_limit = lo_limit;
	}

	if (data->current_hi_limit != hi_limit) {
		value = temperature_to_value(extended_range, hi_limit);
		pr_debug("%s: set hi_limit %ld\n", __func__, hi_limit);
		err = nct1008_write_reg(data->client,
				EXT_TEMP_HI_LIMIT_HI_BYTE_WR, value);
		if (err)
			return err;

		data->current_hi_limit = hi_limit;
	}

	return 0;
}

#ifdef CONFIG_THERMAL
static void nct1008_update(struct nct1008_data *data)
{
	struct thermal_zone_device *thz = data->nct_ext;
	long low_temp = NCT1008_MIN_TEMP(data->plat_data.ext_range) * 1000;
	long high_temp = NCT1008_MAX_TEMP(data->plat_data.ext_range) * 1000;
	struct thermal_trip_info *trip_state;
	long temp, trip_temp, hysteresis_temp;
	int count;
	enum events type = 0;

	if (!thz)
		return;

	thermal_zone_device_update(thz);

	thz->ops->get_temp(thz, &temp);

	for (count = 0; count < thz->trips; count++) {
		trip_state = &data->plat_data.trips[count];
		trip_temp = trip_state->trip_temp;
		hysteresis_temp = trip_temp - trip_state->hysteresis;
		if ((trip_state->trip_type == THERMAL_TRIP_PASSIVE) &&
		    !trip_state->tripped)
			hysteresis_temp = trip_temp;

		if ((trip_temp >= temp) && (trip_temp < high_temp)) {
			high_temp = trip_temp;
			type = THERMAL_AUX1;
		}

		if ((hysteresis_temp < temp) && (hysteresis_temp > low_temp)) {
			low_temp = hysteresis_temp;
			type = THERMAL_AUX0;
		}
	}

	thermal_generate_netlink_event(thz->id, type);
	nct1008_thermal_set_limits(data, low_temp, high_temp);
}

static int nct1008_ext_get_temp(struct thermal_zone_device *thz,
					unsigned long *temp)
{
	struct nct1008_data *data = thz->devdata;
	struct i2c_client *client = data->client;
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	s16 temp_ext_hi;
	s16 temp_ext_lo;
	long temp_ext_milli;
	u8 value;

	/* Read External Temp */
	value = nct1008_read_reg(client, EXT_TEMP_RD_LO);
	if (value < 0)
		return -1;
	temp_ext_lo = (value >> 6);

	value = nct1008_read_reg(client, EXT_TEMP_RD_HI);
	if (value < 0)
		return -1;
	temp_ext_hi = value_to_temperature(pdata->ext_range, value);

	temp_ext_milli = CELSIUS_TO_MILLICELSIUS(temp_ext_hi) +
			 temp_ext_lo * 250;
	*temp = temp_ext_milli;
	data->etemp = temp_ext_milli;

	return 0;
}

static int nct1008_ext_bind(struct thermal_zone_device *thz,
			    struct thermal_cooling_device *cdev)
{
	struct nct1008_data *data = thz->devdata;
	int i;
	bool bind = false;

	for (i = 0; i < data->plat_data.num_trips; i++) {
		if (!strcmp(data->plat_data.trips[i].cdev_type, cdev->type)) {
			thermal_zone_bind_cooling_device(thz, i, cdev,
					data->plat_data.trips[i].upper,
					data->plat_data.trips[i].lower);
			bind = true;
		}
	}

	if (bind)
		nct1008_update(data);

	return 0;
}

static int nct1008_ext_unbind(struct thermal_zone_device *thz,
			      struct thermal_cooling_device *cdev)
{
	struct nct1008_data *data = thz->devdata;
	int i;

	for (i = 0; i < data->plat_data.num_trips; i++) {
		if (!strcmp(data->plat_data.trips[i].cdev_type, cdev->type))
			thermal_zone_unbind_cooling_device(thz, i, cdev);
	}
	return 0;
}

static int nct1008_ext_get_trip_temp(struct thermal_zone_device *thz,
				     int trip,
				     unsigned long *temp)
{
	struct nct1008_data *data = thz->devdata;
	struct thermal_trip_info *trip_state = &data->plat_data.trips[trip];

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

static int nct1008_ext_set_trip_temp(struct thermal_zone_device *thz,
				     int trip,
				     unsigned long temp)
{
	struct nct1008_data *data = thz->devdata;

	data->plat_data.trips[trip].trip_temp = temp;
	nct1008_update(data);
	return 0;
}

static int nct1008_ext_get_trip_type(struct thermal_zone_device *thz,
				     int trip,
				     enum thermal_trip_type *type)
{
	struct nct1008_data *data = thz->devdata;

	*type = data->plat_data.trips[trip].trip_type;
	return 0;
}

static int nct1008_ext_get_trend(struct thermal_zone_device *thz,
				 int trip,
				 enum thermal_trend *trend)
{
	struct nct1008_data *data = thz->devdata;
	struct thermal_trip_info *trip_state;

	trip_state = &data->plat_data.trips[trip];

	switch (trip_state->trip_type) {
	case THERMAL_TRIP_ACTIVE:
		/* aggressive active cooling */
		*trend = THERMAL_TREND_RAISING;
		break;
	case THERMAL_TRIP_PASSIVE:
		if (data->etemp > trip_state->trip_temp)
			*trend = THERMAL_TREND_RAISING;
		else
			*trend = THERMAL_TREND_DROPPING;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int nct1008_int_get_temp(struct thermal_zone_device *thz,
				unsigned long *temp)
{
	struct nct1008_data *data = thz->devdata;
	struct i2c_client *client = data->client;
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	s16 temp_local;
	long temp_local_milli;
	u8 value;

	/* Read Local Temp */
	value = nct1008_read_reg(client, LOCAL_TEMP_RD);
	if (value < 0)
		return -1;
	temp_local = value_to_temperature(pdata->ext_range, value);

	temp_local_milli = CELSIUS_TO_MILLICELSIUS(temp_local);
	*temp = temp_local_milli;

	return 0;
}

static int nct1008_int_bind(struct thermal_zone_device *thz,
			    struct thermal_cooling_device *cdev)
{
	return 0;
}

static int nct1008_int_get_trip_temp(struct thermal_zone_device *thz,
				     int trip,
				     unsigned long *temp)
{
	return -1;
}

static int nct1008_int_get_trip_type(struct thermal_zone_device *thz,
				     int trip,
				     enum thermal_trip_type *type)
{
	return -1;
}

static struct thermal_zone_device_ops nct_int_ops = {
	.get_temp = nct1008_int_get_temp,
	.bind = nct1008_int_bind,
	.unbind = nct1008_int_bind,
	.get_trip_type = nct1008_int_get_trip_type,
	.get_trip_temp = nct1008_int_get_trip_temp,
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
static void nct1008_update(struct nct1008_data *data)
{
}
#endif /* CONFIG_THERMAL */

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
static void print_reg(const char *reg_name, struct seq_file *s,
		int offset)
{
	struct nct1008_data *nct_data = s->private;
	int ret;

	ret = nct1008_read_reg(nct_data->client, offset);
	if (ret >= 0)
		seq_printf(s, "Reg %s Addr = 0x%02x Reg 0x%02x "
		"Value 0x%02x\n", reg_name,
		nct_data->client->addr,
			offset, ret);
	else
		seq_printf(s, "%s: line=%d, i2c read error=%d\n",
		__func__, __LINE__, ret);
}

static int dbg_nct1008_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "NCT Thermal Sensor Registers\n");
	seq_printf(s, "------------------\n");
	print_reg("Local Temp Value    ",     s, 0x00);
	print_reg("Ext Temp Value Hi   ",     s, 0x01);
	print_reg("Status              ",     s, 0x02);
	print_reg("Configuration       ",     s, 0x03);
	print_reg("Conversion Rate     ",     s, 0x04);
	print_reg("Local Temp Hi Limit ",     s, 0x05);
	print_reg("Local Temp Lo Limit ",     s, 0x06);
	print_reg("Ext Temp Hi Limit Hi",     s, 0x07);
	print_reg("Ext Temp Hi Limit Lo",     s, 0x13);
	print_reg("Ext Temp Lo Limit Hi",     s, 0x08);
	print_reg("Ext Temp Lo Limit Lo",     s, 0x14);
	print_reg("Ext Temp Value Lo   ",     s, 0x10);
	print_reg("Ext Temp Offset Hi  ",     s, 0x11);
	print_reg("Ext Temp Offset Lo  ",     s, 0x12);
	print_reg("Ext THERM Limit     ",     s, 0x19);
	print_reg("Local THERM Limit   ",     s, 0x20);
	print_reg("THERM Hysteresis    ",     s, 0x21);
	print_reg("Consecutive ALERT   ",     s, 0x22);
	return 0;
}

static int dbg_nct1008_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_nct1008_show, inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_nct1008_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nct1008_debuginit(struct nct1008_data *nct)
{
	int err = 0;
	struct dentry *d;
	const char *name = nct1008_id[nct->chip].name;

	/* create debugfs by selecting chipid */
	d = debugfs_create_file(name, S_IRUGO, NULL,
		(void *)nct, &debug_fops);

	if ((!d) || IS_ERR(d)) {
		dev_err(&nct->client->dev, "Error: %s debugfs_create_file"
			" returned an error\n", __func__);
		err = -ENOENT;
		goto end;
	}
	if (d == ERR_PTR(-ENODEV)) {
		dev_err(&nct->client->dev, "Error: %s debugfs not supported "
			"error=-ENODEV\n", __func__);
		err = -ENODEV;
	} else {
		nct->dent = d;
	}
end:
	return err;
}
#else
static int nct1008_debuginit(struct nct1008_data *nct)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

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

static int nct1008_within_limits(struct nct1008_data *data)
{
	int intr_status;

	intr_status = nct1008_read_reg(data->client, STATUS_RD);
	if (intr_status < 0)
		return intr_status;

	return !(intr_status & (BIT(3) | BIT(4)));
}

static void nct1008_work_func(struct work_struct *work)
{
	struct nct1008_data *data = container_of(work, struct nct1008_data,
						work);
	int err;
	struct timespec ts;

	err = nct1008_disable(data->client);
	if (err == -ENODEV)
		return;

	if (!nct1008_within_limits(data))
		nct1008_update(data);

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

static int nct1008_power_control(struct nct1008_data *data, bool enable)
{
	int ret;

	if (!data->nct_reg)
		return 0;

	if (enable)
		ret = regulator_enable(data->nct_reg);
	else
		ret = regulator_disable(data->nct_reg);

	if (ret < 0)
		dev_err(&data->client->dev,
			"%s: Failed to %s regulator vdd, %d\n",
			__func__, (enable) ? "enable" : "disable", ret);

	return ret;
}

static int nct1008_configure_sensor(struct nct1008_data *data)
{
	struct i2c_client *client = data->client;
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	u8 value;
	int err;

	if (!pdata || !pdata->supported_hwrev)
		return -ENODEV;

	/* Initially place in Standby */
	err = nct1008_write_reg(client, CONFIG_WR, STANDBY_BIT);
	if (err)
		goto error;

	/* External temperature h/w shutdown limit */
	value = temperature_to_value(pdata->ext_range,
					pdata->shutdown_ext_limit);
	err = nct1008_write_reg(client, EXT_THERM_LIMIT_WR, value);
	if (err)
		goto error;

	/* Local temperature h/w shutdown limit */
	value = temperature_to_value(pdata->ext_range,
					pdata->shutdown_local_limit);
	err = nct1008_write_reg(client, LOCAL_THERM_LIMIT_WR, value);
	if (err)
		goto error;

	/* set extended range mode if needed */
	if (pdata->ext_range)
		data->config |= EXTENDED_RANGE_BIT;
	data->config &= ~(THERM2_BIT | ALERT_BIT);

	err = nct1008_write_reg(client, CONFIG_WR, data->config);
	if (err)
		goto error;

	/* Temperature conversion rate */
	err = nct1008_write_reg(client, CONV_RATE_WR, pdata->conv_rate);
	if (err)
		goto error;

	data->conv_period_ms = conv_period_ms_table[pdata->conv_rate];

	/* Setup local hi and lo limits */
	value = temperature_to_value(pdata->ext_range,
				     NCT1008_MAX_TEMP(pdata->ext_range));
	err = nct1008_write_reg(client, LOCAL_TEMP_HI_LIMIT_WR, value);
	if (err)
		goto error;

	value = temperature_to_value(pdata->ext_range,
				     NCT1008_MIN_TEMP(pdata->ext_range));
	err = nct1008_write_reg(client, LOCAL_TEMP_LO_LIMIT_WR, value);
	if (err)
		goto error;

	/* Setup external hi and lo limits */
	value = temperature_to_value(pdata->ext_range,
				     NCT1008_MAX_TEMP(pdata->ext_range));
	err = nct1008_write_reg(client, EXT_TEMP_HI_LIMIT_HI_BYTE_WR, value);
	if (err)
		goto error;

	value = temperature_to_value(pdata->ext_range,
				     NCT1008_MIN_TEMP(pdata->ext_range));
	err = nct1008_write_reg(client, EXT_TEMP_LO_LIMIT_HI_BYTE_WR, value);
	if (err)
		goto error;

	data->current_hi_limit = NCT1008_MAX_TEMP(pdata->ext_range);
	data->current_lo_limit = NCT1008_MIN_TEMP(pdata->ext_range);

	/* Remote channel offset */
	err = nct1008_write_reg(client, OFFSET_WR, pdata->offset / 4);
	if (err < 0)
		goto error;

	/* Remote channel offset fraction (quarters) */
	err = nct1008_write_reg(client, OFFSET_QUARTER_WR,
					(pdata->offset % 4) << 6);
	if (err < 0)
		goto error;

	return 0;
error:
	dev_err(&client->dev, "\n exit %s, err=%d ", __func__, err);
	return err;
}

static int __devinit nct1008_configure_irq(struct nct1008_data *data)
{
	const char *name = nct1008_id[data->chip].name;

	data->workqueue = create_singlethread_workqueue(name);

	INIT_WORK(&data->work, nct1008_work_func);

	if (data->client->irq < 0)
		return 0;
	else
		return request_irq(data->client->irq, nct1008_irq,
			IRQF_TRIGGER_LOW,
			name,
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
static int __devinit nct1008_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct nct1008_data *data;
	int err;
	int i;
	int mask = 0;
	char nct_int_name[THERMAL_NAME_LENGTH];
	char nct_ext_name[THERMAL_NAME_LENGTH];

	data = kzalloc(sizeof(struct nct1008_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	data->chip = id->driver_data;
	memcpy(&data->plat_data, client->dev.platform_data,
		sizeof(struct nct1008_platform_data));
	i2c_set_clientdata(client, data);
	mutex_init(&data->mutex);

	data->nct_reg = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(data->nct_reg)) {
		dev_warn(&client->dev, "%s: Failed to get regulator vdd, %ld\n",
			 __func__, PTR_ERR(data->nct_reg));
		data->nct_reg = NULL;
	}

	err = nct1008_power_control(data, true);
	if (err < 0)
		goto cleanup;

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

	err = nct1008_debuginit(data);
	if (err < 0)
		err = 0; /* without debugfs we may continue */

#ifdef CONFIG_THERMAL
	for (i = 0; i < data->plat_data.num_trips; i++)
		mask |= (1 << i);

	if (data->plat_data.loc_name) {
		strcpy(nct_int_name, "Tboard_");
		strcpy(nct_ext_name, "Tdiode_");
		strncat(nct_int_name, data->plat_data.loc_name,
			(THERMAL_NAME_LENGTH - strlen("Tboard_")) - 1);
		strncat(nct_ext_name, data->plat_data.loc_name,
			(THERMAL_NAME_LENGTH - strlen("Tdiode_")) - 1);
	} else {
		strcpy(nct_int_name, "Tboard");
		strcpy(nct_ext_name, "Tdiode");
	}

	data->nct_int = thermal_zone_device_register(nct_int_name,
						0,
						0x0,
						data,
						&nct_int_ops,
						NULL,
						2000,
						0);
	if (IS_ERR_OR_NULL(data->nct_int))
		goto error;

	data->nct_ext = thermal_zone_device_register(nct_ext_name,
					data->plat_data.num_trips,
					mask,
					data,
					&nct_ext_ops,
					data->plat_data.tzp,
					data->plat_data.passive_delay,
					0);
	if (IS_ERR_OR_NULL(data->nct_ext)) {
		thermal_zone_device_unregister(data->nct_int);
		data->nct_int = NULL;
		goto error;
	}

	nct1008_update(data);
#endif
	return 0;

error:
	dev_err(&client->dev, "\n exit %s, err=%d ", __func__, err);
	nct1008_power_control(data, false);
cleanup:
	mutex_destroy(&data->mutex);
	kfree(data);
	return err;
}

static int __devexit nct1008_remove(struct i2c_client *client)
{
	struct nct1008_data *data = i2c_get_clientdata(client);

	if (data->dent)
		debugfs_remove(data->dent);

	free_irq(data->client->irq, data);
	cancel_work_sync(&data->work);
	sysfs_remove_group(&client->dev.kobj, &nct1008_attr_group);
	nct1008_power_control(data, false);
	mutex_destroy(&data->mutex);
	kfree(data);

	return 0;
}

static void nct1008_shutdown(struct i2c_client *client)
{
	struct nct1008_data *data = i2c_get_clientdata(client);
	if (client->irq)
		disable_irq(client->irq);

	cancel_work_sync(&data->work);

	mutex_lock(&data->mutex);
	data->shutdown_complete = 1;
	mutex_unlock(&data->mutex);
}

#ifdef CONFIG_PM_SLEEP
static int nct1008_suspend_powerdown(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int err;
	struct nct1008_data *data = i2c_get_clientdata(client);

	disable_irq(client->irq);

	err = nct1008_disable(client);
	if (err < 0) {
		dev_err(&client->dev, "%s: Failed to disable %s, %d\n",
			__func__, client->name, err);
		return err;
	}

	nct1008_power_control(data, false);

	return err;
}

static int nct1008_suspend_wakeup(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct nct1008_data *data = i2c_get_clientdata(client);
	struct nct1008_platform_data *pdata = client->dev.platform_data;
	long ext_temp;
	int err;

	err = nct1008_get_temp(dev, &ext_temp, 0);
	if (err)
		goto error;

	if (ext_temp > data->plat_data.suspend_ext_limit_lo)
		err = nct1008_thermal_set_limits(data,
			data->plat_data.suspend_ext_limit_lo,
			NCT1008_MAX_TEMP(pdata->ext_range) * 1000);
	else
		err = nct1008_thermal_set_limits(data,
			NCT1008_MIN_TEMP(pdata->ext_range) * 1000,
			data->plat_data.suspend_ext_limit_hi);

	if (err)
		goto error;

	/* Enable NCT wake */
	err = enable_irq_wake(client->irq);
	if (err)
		dev_err(&client->dev, "Error: %s, error=%d. failed to enable NCT "
				"wakeup\n", __func__, err);


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

	err = nct1008_power_control(data, true);
	if (err < 0) {
		dev_err(&client->dev, "%s: Failed to enable power, %d\n",
			__func__, err);
		return err;
	}

	err = nct1008_configure_sensor(data);
	if (err < 0) {
		dev_err(&client->dev, "%s: Failed to configure sensor, %d\n",
			__func__, err);
		return err;
	}

	err = nct1008_enable(client);
	if (err < 0)
		dev_err(&client->dev, "%s: Failed to enable %s, %d\n",
			__func__, client->name, err);

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

	nct1008_update(data);
	enable_irq(client->irq);

	return 0;
}

static const struct dev_pm_ops nct1008_pm_ops = {
	.suspend	= nct1008_suspend,
	.resume		= nct1008_resume,
};

#endif

MODULE_DEVICE_TABLE(i2c, nct1008_id);

static struct i2c_driver nct1008_driver = {
	.driver = {
		.name	= "nct_thermal",
#ifdef CONFIG_PM_SLEEP
		.pm = &nct1008_pm_ops,
#endif
	},
	.probe		= nct1008_probe,
	.remove		= __devexit_p(nct1008_remove),
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

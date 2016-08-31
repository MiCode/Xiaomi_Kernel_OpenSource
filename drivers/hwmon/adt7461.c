/*
 * adt7461.c - Linux kernel modules for hardware
 *          monitoring
 * Copyright (C) 2003-2010  Jean Delvare <khali@linux-fr.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/adt7461.h>

#define DRIVER_NAME "adt7461"

/*
 * The ADT7461 registers
 */

#define ADT7461_REG_R_MAN_ID			0xFE
#define ADT7461_REG_R_CHIP_ID			0xFF
#define ADT7461_REG_R_CONFIG1			0x03
#define ADT7461_REG_W_CONFIG1			0x09
#define ADT7461_REG_R_CONVRATE			0x04
#define ADT7461_REG_W_CONVRATE			0x0A
#define ADT7461_REG_R_STATUS			0x02
#define ADT7461_REG_R_LOCAL_TEMP		0x00
#define ADT7461_REG_R_LOCAL_HIGH		0x05
#define ADT7461_REG_W_LOCAL_HIGH		0x0B
#define ADT7461_REG_R_LOCAL_LOW			0x06
#define ADT7461_REG_W_LOCAL_LOW			0x0C
#define ADT7461_REG_R_LOCAL_CRIT		0x20
#define ADT7461_REG_W_LOCAL_CRIT		0x20
#define ADT7461_REG_R_REMOTE_TEMPH		0x01
#define ADT7461_REG_R_REMOTE_TEMPL		0x10
#define ADT7461_REG_R_REMOTE_OFFSH		0x11
#define ADT7461_REG_W_REMOTE_OFFSH		0x11
#define ADT7461_REG_R_REMOTE_OFFSL		0x12
#define ADT7461_REG_W_REMOTE_OFFSL		0x12
#define ADT7461_REG_R_REMOTE_HIGHH		0x07
#define ADT7461_REG_W_REMOTE_HIGHH		0x0D
#define ADT7461_REG_R_REMOTE_HIGHL		0x13
#define ADT7461_REG_W_REMOTE_HIGHL		0x13
#define ADT7461_REG_R_REMOTE_LOWH		0x08
#define ADT7461_REG_W_REMOTE_LOWH		0x0E
#define ADT7461_REG_R_REMOTE_LOWL		0x14
#define ADT7461_REG_W_REMOTE_LOWL		0x14
#define ADT7461_REG_R_REMOTE_CRIT		0x19
#define ADT7461_REG_W_REMOTE_CRIT		0x19
#define ADT7461_REG_R_TCRIT_HYST		0x21
#define ADT7461_REG_W_TCRIT_HYST		0x21

/* Configuration Register Bits */
#define EXTENDED_RANGE_BIT		BIT(2)
#define THERM2_BIT			BIT(5)
#define STANDBY_BIT			BIT(6)
#define ALERT_BIT			BIT(7)

/* Max Temperature Measurements */
#define EXTENDED_RANGE_OFFSET	64U
#define STANDARD_RANGE_MAX		127U
#define EXTENDED_RANGE_MAX		(150U + EXTENDED_RANGE_OFFSET)

/*
 * Device flags
 */
#define ADT7461_FLAG_ADT7461_EXT		0x01	/* ADT7461 extended mode */
#define ADT7461_FLAG_THERM2			0x02	/* Pin 6 as Therm2 */

/*
 * Client data
 */

struct adt7461_data {
	struct work_struct work;
	struct i2c_client *client;
	struct device *hwmon_dev;
	struct mutex update_lock;
	struct regulator *regulator;
	char valid; /* zero until following fields are valid */
	unsigned long last_updated; /* in jiffies */
	int flags;

	u8 config;		/* configuration register value */
	u8 alert_alarms;	/* Which alarm bits trigger ALERT# */

	/* registers values */
	s8 temp8[4];	/* 0: local low limit
			   1: local high limit
			   2: local critical limit
			   3: remote critical limit */
	s16 temp11[5];	/* 0: remote input
			   1: remote low limit
			   2: remote high limit
			   3: remote offset
			   4: local input */
	u8 temp_hyst;
	u8 alarms; /* bitvector */
	void (*alarm_fn)(bool raised);
	int irq_gpio;
};

/*
 * Conversions
 */

static inline int temp_from_s8(s8 val)
{
	return val * 1000;
}

static u8 hyst_to_reg(long val)
{
	if (val <= 0)
		return 0;
	if (val >= 30500)
		return 31;
	return (val + 500) / 1000;
}

/*
 * ADT7461 attempts to write values that are outside the range
 * 0 < temp < 127 are treated as the boundary value.
 *
 * ADT7461 in "extended mode" operation uses unsigned integers offset by
 * 64 (e.g., 0 -> -64 degC).  The range is restricted to -64..191 degC.
 */
static inline int temp_from_u8(struct adt7461_data *data, u8 val)
{
	if (data->flags & ADT7461_FLAG_ADT7461_EXT)
		return (val - 64) * 1000;
	else
		return temp_from_s8(val);
}

static inline int temp_from_u16(struct adt7461_data *data, u16 val)
{
	if (data->flags & ADT7461_FLAG_ADT7461_EXT)
		return (val - 0x4000) / 64 * 250;
	else
		return val / 32 * 125;
}

static u8 temp_to_u8(struct adt7461_data *data, long val)
{
	if (data->flags & ADT7461_FLAG_ADT7461_EXT) {
		if (val <= -64000)
			return 0;
		if (val >= 191000)
			return 0xFF;
		return (val + 500 + 64000) / 1000;
	} else {
		if (val <= 0)
			return 0;
		if (val >= 127000)
			return 127;
		return (val + 500) / 1000;
	}
}

static u16 temp_to_u16(struct adt7461_data *data, long val)
{
	if (data->flags & ADT7461_FLAG_ADT7461_EXT) {
		if (val <= -64000)
			return 0;
		if (val >= 191750)
			return 0xFFC0;
		return (val + 64000 + 125) / 250 * 64;
	} else {
		if (val <= 0)
			return 0;
		if (val >= 127750)
			return 0x7FC0;
		return (val + 125) / 250 * 64;
	}
}

static int adt7461_read_reg(struct i2c_client* client, u8 reg, u8 *value)
{
	int err;

	err = i2c_smbus_read_byte_data(client, reg);
	if (err < 0) {
		pr_err("adt7461_read_reg:Register %#02x read failed (%d)\n",
			 reg, err);
		return err;
	}
	*value = err;

	return 0;
}

static int adt7461_read16(struct i2c_client *client, u8 regh, u8 regl,
			u16 *value)
{
	int err;
	u8 oldh, newh, l;

	/*
	 * There is a trick here. We have to read two registers to have the
	 * sensor temperature, but we have to beware a conversion could occur
	 * inbetween the readings. The datasheet says we should either use
	 * the one-shot conversion register, which we don't want to do
	 * (disables hardware monitoring) or monitor the busy bit, which is
	 * impossible (we can't read the values and monitor that bit at the
	 * exact same time). So the solution used here is to read the high
	 * byte once, then the low byte, then the high byte again. If the new
	 * high byte matches the old one, then we have a valid reading. Else
	 * we have to read the low byte again, and now we believe we have a
	 * correct reading.
	 */
	if ((err = adt7461_read_reg(client, regh, &oldh))
	 || (err = adt7461_read_reg(client, regl, &l))
	 || (err = adt7461_read_reg(client, regh, &newh)))
		return err;
	if (oldh != newh) {
		err = adt7461_read_reg(client, regl, &l);
		if (err)
			return err;
	}
	*value = (newh << 8) | l;

	return 0;
}

static struct adt7461_data *adt7461_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct adt7461_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + HZ / 2 + HZ / 10)
	 || !data->valid) {
		u8 h, l;

		adt7461_read_reg(client, ADT7461_REG_R_LOCAL_LOW, &data->temp8[0]);
		adt7461_read_reg(client, ADT7461_REG_R_LOCAL_HIGH, &data->temp8[1]);
		adt7461_read_reg(client, ADT7461_REG_R_LOCAL_CRIT, &data->temp8[2]);
		adt7461_read_reg(client, ADT7461_REG_R_REMOTE_CRIT, &data->temp8[3]);
		adt7461_read_reg(client, ADT7461_REG_R_TCRIT_HYST, &data->temp_hyst);

		if (adt7461_read_reg(client, ADT7461_REG_R_LOCAL_TEMP, &h) == 0)
			data->temp11[4] = h << 8;

		adt7461_read16(client, ADT7461_REG_R_REMOTE_TEMPH,
				ADT7461_REG_R_REMOTE_TEMPL, &data->temp11[0]);

		if (adt7461_read_reg(client, ADT7461_REG_R_REMOTE_LOWH, &h) == 0) {
			data->temp11[1] = h << 8;
			if (adt7461_read_reg(client, ADT7461_REG_R_REMOTE_LOWL, &l) == 0)
				data->temp11[1] |= l;
		}
		if (adt7461_read_reg(client, ADT7461_REG_R_REMOTE_HIGHH, &h) == 0) {
			data->temp11[2] = h << 8;
			if (adt7461_read_reg(client, ADT7461_REG_R_REMOTE_HIGHL, &l) == 0)
				data->temp11[2] |= l;
		}

		if (adt7461_read_reg(client, ADT7461_REG_R_REMOTE_OFFSH,
					&h) == 0
		 && adt7461_read_reg(client, ADT7461_REG_R_REMOTE_OFFSL,
					&l) == 0)
			data->temp11[3] = (h << 8) | l;
		adt7461_read_reg(client, ADT7461_REG_R_STATUS, &data->alarms);

		/* Re-enable ALERT# output if relevant alarms are all clear */
		if (!(data->flags & ADT7461_FLAG_THERM2)
		 && (data->alarms & data->alert_alarms) == 0) {
			u8 config;

			adt7461_read_reg(client, ADT7461_REG_R_CONFIG1, &config);
			if (config & 0x80) {
				pr_err("adt7461_update_device:Re-enabling ALERT#\n");
				i2c_smbus_write_byte_data(client,
							ADT7461_REG_W_CONFIG1,
							config & ~ALERT_BIT);
			}
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

/*
 * Sysfs stuff
 */

static ssize_t show_temp8(struct device *dev, struct device_attribute *devattr,
			  char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct adt7461_data *data = adt7461_update_device(dev);
	int temp;

	temp = temp_from_u8(data, data->temp8[attr->index]);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t set_temp8(struct device *dev, struct device_attribute *devattr,
			 const char *buf, size_t count)
{
	static const u8 reg[4] = {
		ADT7461_REG_W_LOCAL_LOW,
		ADT7461_REG_W_LOCAL_HIGH,
		ADT7461_REG_W_LOCAL_CRIT,
		ADT7461_REG_W_REMOTE_CRIT,
	};

	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct adt7461_data *data = i2c_get_clientdata(client);
	long val = simple_strtol(buf, NULL, 10);
	int nr = attr->index;

	mutex_lock(&data->update_lock);
	data->temp8[nr] = temp_to_u8(data, val);
	i2c_smbus_write_byte_data(client, reg[nr], data->temp8[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_temp11(struct device *dev, struct device_attribute *devattr,
			  char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct adt7461_data *data = adt7461_update_device(dev);
	int temp;

	temp = temp_from_u16(data, data->temp11[attr->index]);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t set_temp11(struct device *dev, struct device_attribute *devattr,
			  const char *buf, size_t count)
{
	static const u8 reg[6] = {
		ADT7461_REG_W_REMOTE_LOWH,
		ADT7461_REG_W_REMOTE_LOWL,
		ADT7461_REG_W_REMOTE_HIGHH,
		ADT7461_REG_W_REMOTE_HIGHL,
		ADT7461_REG_W_REMOTE_OFFSH,
		ADT7461_REG_W_REMOTE_OFFSL,
	};

	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct i2c_client *client = to_i2c_client(dev);
	struct adt7461_data *data = i2c_get_clientdata(client);
	long val = simple_strtol(buf, NULL, 10);
	int nr = attr->index;

	mutex_lock(&data->update_lock);
	data->temp11[nr] = temp_to_u16(data, val);

	i2c_smbus_write_byte_data(client, reg[(nr - 1) * 2],
				  data->temp11[nr] >> 8);
	i2c_smbus_write_byte_data(client, reg[(nr - 1) * 2 + 1],
				  data->temp11[nr] & 0xff);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_temphyst(struct device *dev,
		struct device_attribute *devattr,
		char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct adt7461_data *data = adt7461_update_device(dev);
	int temp;

	temp = temp_from_u8(data, data->temp8[attr->index]);

	return sprintf(buf, "%d\n", temp - temp_from_s8(data->temp_hyst));
}

static ssize_t set_temphyst(struct device *dev, struct device_attribute *dummy,
			 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct adt7461_data *data = i2c_get_clientdata(client);
	long val = simple_strtol(buf, NULL, 10);
	int temp;

	mutex_lock(&data->update_lock);
	temp = temp_from_u8(data, data->temp8[2]);
	data->temp_hyst = hyst_to_reg(temp - val);
	i2c_smbus_write_byte_data(client, ADT7461_REG_W_TCRIT_HYST,
				  data->temp_hyst);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t show_alarms(struct device *dev, struct device_attribute *dummy,
			 char *buf)
{
	struct adt7461_data *data = adt7461_update_device(dev);
	return sprintf(buf, "%d\n", data->alarms);
}

static ssize_t show_alarm(struct device *dev, struct device_attribute
			  *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct adt7461_data *data = adt7461_update_device(dev);
	int bitnr = attr->index;

	return sprintf(buf, "%d\n", (data->alarms >> bitnr) & 1);
}

static SENSOR_DEVICE_ATTR(temp1_input, S_IRUGO, show_temp11, NULL, 4);
static SENSOR_DEVICE_ATTR(temp2_input, S_IRUGO, show_temp11, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_min, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 0);
static SENSOR_DEVICE_ATTR(temp2_min, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 1);
static SENSOR_DEVICE_ATTR(temp1_max, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 1);
static SENSOR_DEVICE_ATTR(temp2_max, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 2);
static SENSOR_DEVICE_ATTR(temp1_crit, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 2);
static SENSOR_DEVICE_ATTR(temp2_crit, S_IWUSR | S_IRUGO, show_temp8,
	set_temp8, 3);
static SENSOR_DEVICE_ATTR(temp1_crit_hyst, S_IWUSR | S_IRUGO, show_temphyst,
	set_temphyst, 2);
static SENSOR_DEVICE_ATTR(temp2_crit_hyst, S_IRUGO, show_temphyst, NULL, 3);
static SENSOR_DEVICE_ATTR(temp2_offset, S_IWUSR | S_IRUGO, show_temp11,
	set_temp11, 3);

/* Individual alarm files */
static SENSOR_DEVICE_ATTR(temp1_crit_alarm, S_IRUGO, show_alarm, NULL, 0);
static SENSOR_DEVICE_ATTR(temp2_crit_alarm, S_IRUGO, show_alarm, NULL, 1);
static SENSOR_DEVICE_ATTR(temp2_fault, S_IRUGO, show_alarm, NULL, 2);
static SENSOR_DEVICE_ATTR(temp2_min_alarm, S_IRUGO, show_alarm, NULL, 3);
static SENSOR_DEVICE_ATTR(temp2_max_alarm, S_IRUGO, show_alarm, NULL, 4);
static SENSOR_DEVICE_ATTR(temp1_min_alarm, S_IRUGO, show_alarm, NULL, 5);
static SENSOR_DEVICE_ATTR(temp1_max_alarm, S_IRUGO, show_alarm, NULL, 6);
/* Raw alarm file for compatibility */
static DEVICE_ATTR(alarms, S_IRUGO, show_alarms, NULL);

static struct attribute *adt7461_attributes[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp1_min.dev_attr.attr,
	&sensor_dev_attr_temp2_min.dev_attr.attr,
	&sensor_dev_attr_temp1_max.dev_attr.attr,
	&sensor_dev_attr_temp2_max.dev_attr.attr,
	&sensor_dev_attr_temp1_crit.dev_attr.attr,
	&sensor_dev_attr_temp2_crit.dev_attr.attr,
	&sensor_dev_attr_temp1_crit_hyst.dev_attr.attr,
	&sensor_dev_attr_temp2_crit_hyst.dev_attr.attr,

	&sensor_dev_attr_temp1_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_crit_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_fault.dev_attr.attr,
	&sensor_dev_attr_temp2_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp2_max_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_min_alarm.dev_attr.attr,
	&sensor_dev_attr_temp1_max_alarm.dev_attr.attr,
	&dev_attr_alarms.attr,
	NULL
};

static const struct attribute_group adt7461_group = {
	.attrs = adt7461_attributes,
};

static void adt7461_work_func(struct work_struct *work)
{
	struct adt7461_data *data =
			container_of(work, struct adt7461_data, work);

	if (data->alarm_fn) {
		/* Therm2 line is active low */
		data->alarm_fn(!gpio_get_value(data->irq_gpio));
	}
}

static irqreturn_t adt7461_irq(int irq, void *dev_id)
{
	struct adt7461_data *data = dev_id;
	schedule_work(&data->work);

	return IRQ_HANDLED;
}

static void adt7461_regulator_enable(struct i2c_client *client)
{
	struct adt7461_data *data = i2c_get_clientdata(client);

	data->regulator = regulator_get(NULL, "vdd_vcore_temp");
	if (IS_ERR_OR_NULL(data->regulator)) {
		pr_err("adt7461_regulator_enable:Couldn't get regulator vdd_vcore_temp\n");
		data->regulator = NULL;
	} else {
		regulator_enable(data->regulator);
		/* Optimal time to get the regulator turned on
		 * before initializing adt7461 chip*/
		mdelay(5);
	}
}

static void adt7461_regulator_disable(struct i2c_client *client)
{
	struct adt7461_data *data = i2c_get_clientdata(client);
	struct regulator *adt7461_reg = data->regulator;
	int ret;

	if (adt7461_reg) {
		ret = regulator_is_enabled(adt7461_reg);
		if (ret > 0)
			regulator_disable(adt7461_reg);
		regulator_put(adt7461_reg);
	}
	data->regulator = NULL;
}

static void adt7461_enable(struct i2c_client *client)
{
	struct adt7461_data *data = i2c_get_clientdata(client);

	i2c_smbus_write_byte_data(client, ADT7461_REG_W_CONFIG1,
				  data->config & ~STANDBY_BIT);
}

static void adt7461_disable(struct i2c_client *client)
{
	struct adt7461_data *data = i2c_get_clientdata(client);

	i2c_smbus_write_byte_data(client, ADT7461_REG_W_CONFIG1,
				  data->config | STANDBY_BIT);
}

static int adt7461_init_client(struct i2c_client *client)
{
	struct adt7461_data *data = i2c_get_clientdata(client);
	struct adt7461_platform_data *pdata = client->dev.platform_data;
	u8 config = 0;
	u8 value;
	int err;

	if (!pdata || !pdata->supported_hwrev)
		return -ENODEV;

	data->irq_gpio = -1;

	if (pdata->therm2) {
		data->flags |= ADT7461_FLAG_THERM2;

		if (gpio_is_valid(pdata->irq_gpio)) {
			if (!IS_ERR(gpio_request(pdata->irq_gpio, "adt7461"))) {
				gpio_direction_input(pdata->irq_gpio);
				data->irq_gpio = pdata->irq_gpio;
			}
		}
	}

	if (pdata->ext_range)
		data->flags |= ADT7461_FLAG_ADT7461_EXT;

	adt7461_regulator_enable(client);

	/* Start the conversions. */
	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_CONVRATE,
							pdata->conv_rate);
	if (err < 0)
		goto error;

	/* External temperature h/w shutdown limit */
	value = temp_to_u8(data, pdata->shutdown_ext_limit * 1000);
	err = i2c_smbus_write_byte_data(client,
				ADT7461_REG_W_REMOTE_CRIT, value);
	if (err < 0)
		goto error;

	/* Local temperature h/w shutdown limit */
	value = temp_to_u8(data, pdata->shutdown_local_limit * 1000);
	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_LOCAL_CRIT,
								value);
	if (err < 0)
		goto error;

	/* External Temperature Throttling limit */
	value = temp_to_u8(data, pdata->throttling_ext_limit * 1000);
	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_REMOTE_HIGHH,
								value);
	if (err < 0)
		goto error;

	/* Local Temperature Throttling limit */
	value = (data->flags & ADT7461_FLAG_ADT7461_EXT) ?
				EXTENDED_RANGE_MAX : STANDARD_RANGE_MAX;
	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_LOCAL_HIGH,
								value);
	if (err < 0)
		goto error;

	/* Remote channel offset */
	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_REMOTE_OFFSH,
							pdata->offset);
	if (err < 0)
		goto error;

	/* THERM hysteresis */
	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_TCRIT_HYST,
					pdata->hysteresis);
	if (err < 0)
		goto error;

	if (data->flags & ADT7461_FLAG_THERM2) {
		data->alarm_fn = pdata->alarm_fn;
		config = (THERM2_BIT | STANDBY_BIT);
	} else {
		config = (~ALERT_BIT & ~THERM2_BIT & STANDBY_BIT);
	}

	err = i2c_smbus_write_byte_data(client, ADT7461_REG_W_CONFIG1, config);
	if (err < 0)
		goto error;

	data->config = config;
	return 0;

error:
	pr_err("adt7461_init_client:Initialization failed!\n");
	return err;
}

static int adt7461_init_irq(struct adt7461_data *data)
{
	INIT_WORK(&data->work, adt7461_work_func);

	return request_irq(data->client->irq, adt7461_irq, IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING, DRIVER_NAME, data);
}

static int adt7461_probe(struct i2c_client *new_client,
		  const struct i2c_device_id *id)
{
	struct adt7461_data *data;
	int err;

	data = kzalloc(sizeof(struct adt7461_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = new_client;
	i2c_set_clientdata(new_client, data);
	mutex_init(&data->update_lock);

	data->alert_alarms = 0x7c;

	/* Initialize the ADT7461 chip */
	err = adt7461_init_client(new_client);
	if (err < 0)
		goto exit_free;

	if (data->flags & ADT7461_FLAG_THERM2) {
		err = adt7461_init_irq(data);
		if (err < 0)
			goto exit_free;
	}

	/* Register sysfs hooks */
	if ((err = sysfs_create_group(&new_client->dev.kobj, &adt7461_group)))
		goto exit_free;
	if ((err = device_create_file(&new_client->dev,
			&sensor_dev_attr_temp2_offset.dev_attr)))
		goto exit_remove_files;

	data->hwmon_dev = hwmon_device_register(&new_client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove_files;
	}

	adt7461_enable(new_client);
	return 0;

exit_remove_files:
	sysfs_remove_group(&new_client->dev.kobj, &adt7461_group);
exit_free:
	kfree(data);
	return err;
}

static int adt7461_remove(struct i2c_client *client)
{
	struct adt7461_data *data = i2c_get_clientdata(client);

	if (data->flags & ADT7461_FLAG_THERM2) {
		free_irq(client->irq, data);
		cancel_work_sync(&data->work);
	}
	if (gpio_is_valid(data->irq_gpio))
		gpio_free(data->irq_gpio);
	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &adt7461_group);
	device_remove_file(&client->dev,
			&sensor_dev_attr_temp2_offset.dev_attr);
	adt7461_regulator_disable(client);

	kfree(data);
	return 0;
}

static void adt7461_alert(struct i2c_client *client, unsigned int flag)
{
	struct adt7461_data *data = i2c_get_clientdata(client);
	u8 config, alarms;

	adt7461_read_reg(client, ADT7461_REG_R_STATUS, &alarms);
	if ((alarms & 0x7f) == 0) {
		pr_err("adt7461_alert:Everything OK\n");
	} else {
		if (alarms & 0x61)
			pr_err("adt7461_alert:temp%d out of range, please check!\n", 1);
		if (alarms & 0x1a)
			pr_err("adt7461_alert:temp%d out of range, please check!\n", 2);
		if (alarms & 0x04)
			pr_err("adt7461_alert:temp%d diode open, please check!\n", 2);

		/* Disable ALERT# output, because these chips don't implement
		  SMBus alert correctly; they should only hold the alert line
		  low briefly. */
		if (!(data->flags & ADT7461_FLAG_THERM2)
		 && (alarms & data->alert_alarms)) {
			pr_err("adt7461_alert:Disabling ALERT#\n");
			adt7461_read_reg(client, ADT7461_REG_R_CONFIG1, &config);
			i2c_smbus_write_byte_data(client, ADT7461_REG_W_CONFIG1,
					config | ALERT_BIT);
		}
	}
}

#ifdef CONFIG_PM
static int adt7461_suspend(struct i2c_client *client, pm_message_t state)
{
	disable_irq(client->irq);
	adt7461_disable(client);

	return 0;
}

static int adt7461_resume(struct i2c_client *client)
{
	adt7461_enable(client);
	enable_irq(client->irq);

	return 0;
}
#endif

/*
 * Driver data
 */
static const struct i2c_device_id adt7461_id[] = {
	{ DRIVER_NAME, 0 },
};

MODULE_DEVICE_TABLE(i2c, adt7461_id);

static struct i2c_driver adt7461_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= adt7461_probe,
	.remove		= adt7461_remove,
	.alert		= adt7461_alert,
	.id_table	= adt7461_id,
#ifdef CONFIG_PM
	.suspend	= adt7461_suspend,
	.resume		= adt7461_resume,
#endif
};

static int __init sensors_adt7461_init(void)
{
	return i2c_add_driver(&adt7461_driver);
}

static void __exit sensors_adt7461_exit(void)
{
	i2c_del_driver(&adt7461_driver);
}

MODULE_AUTHOR("Jean Delvare <khali@linux-fr.org>");
MODULE_DESCRIPTION("ADT7461 driver");
MODULE_LICENSE("GPL");

module_init(sensors_adt7461_init);
module_exit(sensors_adt7461_exit);

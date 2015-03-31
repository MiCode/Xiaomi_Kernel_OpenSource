/*
 * HID Sensors Driver
 * Copyright (c) 2012, Intel Corporation.
 * Copyright (c) 2013, Movea SA, Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/hid-sensor-hub.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

static int pow_10(unsigned power)
{
	int i;
	int ret = 1;
	for (i = 0; i < power; ++i)
		ret = ret * 10;

	return ret;
}

static inline int mult_pow_10(int a, int power)
{
	if (power < 0)
		return a / pow_10(-power);
	else
		return a * pow_10(power);
}

static void simple_div(int dividend, int divisor, int *whole,
		       int *micro_frac)
{
	int rem;
	int exp = 0;

	*micro_frac = 0;
	if (divisor == 0) {
		*whole = 0;
		return;
	}
	*whole = dividend/divisor;
	rem = dividend % divisor;
	if (rem) {
		while (rem <= divisor) {
			rem *= 10;
			exp++;
		}
		*micro_frac = mult_pow_10(rem / divisor, 6 - exp);
	}
}

static void split_micro_fraction(unsigned int no, int exp, int *val1, int *val2)
{
	*val1 = no / pow_10(exp);
	*val2 = mult_pow_10(no % pow_10(exp), 6 - exp);
}

static void convert_from(s32 value, int exp, int *val1, int *val2)
{
	int sign;

	if (exp >= 0) {
		*val1 = value * pow_10(exp);
		*val2 = 0;
	} else {
		if (value < 0)
			sign = -1;
		else
			sign = 1;
		split_micro_fraction(abs(value), -exp, val1, val2);
		if (*val1)
			*val1 *= sign;
		else
			*val2 *= sign;
	}
}

static s32 convert_to(int exp, int val1, int val2)
{
	s32 value;
	int sign = 1;

	if (val1 < 0 || val2 < 0)
		sign = -1;
	value = mult_pow_10(abs(val1), -exp);
	if (exp < 0)
		value += mult_pow_10(abs(val2), -6 - exp);
	if (sign < 0)
		value = -value;

	return value;
}

static ssize_t hid_sensor_read_string(struct hid_sensor_common *common,
				      u32 attrib_id, const char *def, char *buf)
{
	struct hid_sensor_hub_attribute_info attr;
	s32 values[16];
	char string[16];
	const char *str;
	int i, ret;

	sensor_hub_input_get_attribute_info(common->hsdev, HID_FEATURE_REPORT,
			common->report_id, common->usage_id,
			attrib_id, &attr);

	ret = sensor_hub_get_feature(common->hsdev, common->report_id,
				     attr.index, values, ARRAY_SIZE(values));
	if (ret < 0) {
		if (def == NULL)
			return ret;
		str = def;
	} else {
		for (i = 0; i < ARRAY_SIZE(string); i++)
			string[i] = values[i];
		str = string;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", str);
}

static ssize_t hid_sensor_read_value(struct hid_sensor_common *common,
				     u32 attrib_id, char *buf)
{
	struct hid_sensor_hub_attribute_info attr;
	int val1, val2;
	char sign[2] = { '\0', '\0' };
	int ret;

	sensor_hub_input_get_attribute_info(common->hsdev, HID_FEATURE_REPORT,
			common->report_id, common->usage_id,
			attrib_id, &attr);

	ret = hid_sensor_read_raw_value(common, &attr, &val1, &val2);
	if (ret != IIO_VAL_INT_PLUS_MICRO)
		return -EINVAL;

	if (val2 < 0) {
		sign[0] = '-';
		val2 = -val2;
	}
	return snprintf(buf, PAGE_SIZE, "%s%d.%06u\n", sign, val1, val2);
}

static ssize_t hid_sensor_read_manufacturer(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_string(common,
			HID_USAGE_SENSOR_PROP_SENSOR_MANUFACTURER,
			common->hsdev->hdev->name, buf);
}

static ssize_t hid_sensor_read_serial(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_string(common,
			HID_USAGE_SENSOR_PROP_SENSOR_SERIAL_NUMBER,
			common->hsdev->hdev->uniq, buf);
}

static ssize_t hid_sensor_read_description(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_string(common,
			HID_USAGE_SENSOR_PROP_SENSOR_DESCRIPTION,
			NULL, buf);
}

static ssize_t hid_sensor_read_sampling_min(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_value(common,
			HID_USAGE_SENSOR_PROP_MINIMUM_REPORT_INTERVAL,
			buf);
}

static ssize_t hid_sensor_read_max(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_value(common,
			common->data_id | HID_USAGE_SENSOR_DATA_MOD_MAX,
			buf);
}

static ssize_t hid_sensor_read_min(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_value(common,
			common->data_id | HID_USAGE_SENSOR_DATA_MOD_MIN,
			buf);
}

static ssize_t hid_sensor_read_resolution(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_value(common,
			common->data_id | HID_USAGE_SENSOR_DATA_MOD_RESOLUTION,
			buf);
}

static ssize_t hid_sensor_read_threshold_low(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_value(common,
			common->data_id | HID_USAGE_SENSOR_DATA_MOD_THRESHOLD_LOW,
			buf);
}

static ssize_t hid_sensor_read_threshold_high(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;

	return hid_sensor_read_value(common,
			common->data_id | HID_USAGE_SENSOR_DATA_MOD_THRESHOLD_HIGH,
			buf);
}

static ssize_t hid_sensor_read_period_max(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;
	int val1, val2;
	char sign[2] = { '\0', '\0' };
	int ret;

	ret = hid_sensor_read_raw_value(common, &common->period_max,
					&val1, &val2);
	if (ret != IIO_VAL_INT_PLUS_MICRO)
		return -EINVAL;

	if (val2 < 0) {
		sign[0] = '-';
		val2 = -val2;
	}
	return snprintf(buf, PAGE_SIZE, "%s%d.%06u\n", sign, val1, val2);
}

static ssize_t hid_sensor_write_period_max(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;
	int val1, val2;
	int ret;

	ret = sscanf(buf, "%d.%u", &val1, &val2);
	if (ret != 2)
		return -EINVAL;

	return hid_sensor_write_raw_value(common, &common->period_max,
					  val1, val2);
}

static ssize_t hid_sensor_read_reporting_state(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan, char *buf)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;
	int val1, val2;
	int ret;

	ret = hid_sensor_read_raw_value(common, &common->report_state,
					&val1, &val2);
	if (ret != IIO_VAL_INT_PLUS_MICRO)
		return -EINVAL;

	val1 = hid_sensor_common_enum_read(&common->report_state, val1);

	return snprintf(buf, PAGE_SIZE, "%d\n", val1);
}

static ssize_t hid_sensor_write_reporting_state(struct iio_dev *iio,
		uintptr_t private, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	struct hid_sensor_common *common = (struct hid_sensor_common *)private;
	int val;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1)
		return -EINVAL;

	val = hid_sensor_common_enum_write(&common->report_state, val);
	if (val < common->report_state.logical_minimum ||
			val > common->report_state.logical_maximum)
		return -EINVAL;

	return hid_sensor_write_raw_value(common, &common->report_state, val, 0);
}

static const struct iio_chan_spec_ext_info hid_sensor_ext[] = {
	{
		.name = "reporting_state",
		.shared = true,
		.read = hid_sensor_read_reporting_state,
		.write = hid_sensor_write_reporting_state,
	},
	{
		.name = "manufacturer",
		.shared = true,
		.read = hid_sensor_read_manufacturer,
	},
	{
		.name = "serial",
		.shared = true,
		.read = hid_sensor_read_serial,
	},
	{
		.name = "description",
		.shared = true,
		.read = hid_sensor_read_description,
	},
	{
		.name = "sampling_min",
		.shared = true,
		.read = hid_sensor_read_sampling_min,
	},
	{
		.name = "max",
		.shared = true,
		.read = hid_sensor_read_max,
	},
	{
		.name = "min",
		.shared = true,
		.read = hid_sensor_read_min,
	},
	{
		.name = "resolution",
		.shared = true,
		.read = hid_sensor_read_resolution,
	},
	{
		.name = "threshold_low",
		.shared = true,
		.read = hid_sensor_read_threshold_low,
	},
	{
		.name = "threshold_high",
		.shared = true,
		.read = hid_sensor_read_threshold_high,
	},
	{
		.name = "period_max",
		.shared = true,
		.read = hid_sensor_read_period_max,
		.write = hid_sensor_write_period_max,
	},
	{ }
};

int hid_sensor_read_samp_freq_value(struct hid_sensor_common *st,
				    int *val1, int *val2)
{
	s32 value;
	int exp;
	int ret;

	ret = sensor_hub_get_feature(st->hsdev, st->report_id,
				     st->poll.index, &value, 1);
	if (ret < 0 || value < 0) {
		*val1 = *val2 = 0;
		return -EINVAL;
	}
	switch (st->poll.units) {
	case HID_USAGE_SENSOR_UNITS_SECOND:
		exp = st->poll.unit_expo;
		break;
	case HID_USAGE_SENSOR_UNITS_NOT_SPECIFIED:
		/* default is ms, exp 10^-3 */
		exp = st->poll.unit_expo - 3;
		break;
	default:
		*val1 = *val2 = 0;
		return -EINVAL;
	}
	if (exp > 0) {
		value *= pow_10(exp);
		exp = 0;
	}
	simple_div(pow_10(-exp), value, val1, val2);

	return IIO_VAL_INT_PLUS_MICRO;
}
EXPORT_SYMBOL(hid_sensor_read_samp_freq_value);

int hid_sensor_write_samp_freq_value(struct hid_sensor_common *st,
				     int val1, int val2)
{
	s32 value;
	int exp;
	int ret;

	if (val1 < 0 || val2 < 0)
		return -EINVAL;

	value = val1 * pow_10(6) + val2;
	if (value < 0)
		return -EINVAL;
	if (value) {
		switch (st->poll.units) {
		case HID_USAGE_SENSOR_UNITS_SECOND:
			exp = st->poll.unit_expo;
			break;
		case HID_USAGE_SENSOR_UNITS_NOT_SPECIFIED:
			/* default is ms , exp 10^-3 */
			exp = st->poll.unit_expo - 3;
			break;
		default:
			return -EINVAL;
		}
		if (exp > 6) {
			value *= pow_10(exp - 6);
			exp = 6;
		}
		value = pow_10(6 - exp) / value;
	}
	ret = sensor_hub_set_feature(st->hsdev, st->report_id,
				     st->poll.index, &value, 1);
	if (ret < 0)
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(hid_sensor_write_samp_freq_value);

int hid_sensor_read_raw_value(struct hid_sensor_common *st,
			      const struct hid_sensor_hub_attribute_info *attr,
			      int *val1, int *val2)
{
	s32 value;
	int ret;

	ret = sensor_hub_get_feature(st->hsdev, st->report_id,
				     attr->index, &value, 1);
	if (ret < 0) {
		*val1 = *val2 = 0;
		return -EINVAL;
	}
	convert_from(value, attr->unit_expo, val1, val2);

	return IIO_VAL_INT_PLUS_MICRO;
}
EXPORT_SYMBOL(hid_sensor_read_raw_value);

int hid_sensor_write_raw_value(struct hid_sensor_common *st,
			       const struct hid_sensor_hub_attribute_info *attr,
			       int val1, int val2)
{
	s32 value;
	int ret;

	value = convert_to(attr->unit_expo, val1, val2);
	ret = sensor_hub_set_feature(st->hsdev, st->report_id,
				     attr->index, &value, 1);
	if (ret < 0)
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(hid_sensor_write_raw_value);

int hid_sensor_parse_common(struct hid_sensor_hub_device *hsdev,
			    u32 report_id, u32 usage_id, u32 data_id,
			    struct hid_sensor_common *st)
{
	int i;

	st->hsdev = hsdev;
	st->report_id = report_id;
	st->usage_id = usage_id;
	st->data_id = data_id;

	st->ext_info = kmemdup(hid_sensor_ext, sizeof(hid_sensor_ext), GFP_KERNEL);
	if (st->ext_info)
		for (i = 0; i < ARRAY_SIZE(hid_sensor_ext) - 1; i++)
			st->ext_info[i].private = (uintptr_t)st;

	sensor_hub_input_get_attribute_info(hsdev, HID_FEATURE_REPORT,
			report_id, usage_id,
			HID_USAGE_SENSOR_PROP_REPORT_INTERVAL,
			&st->poll);

	sensor_hub_input_get_attribute_info(hsdev, HID_FEATURE_REPORT,
			report_id, usage_id,
			HID_USAGE_SENSOR_PROP_REPORT_STATE,
			&st->report_state);

	sensor_hub_input_get_attribute_info(hsdev, HID_FEATURE_REPORT,
			report_id, usage_id,
			HID_USAGE_SENSOR_PROP_POWER_STATE,
			&st->power_state);

	sensor_hub_input_get_attribute_info(hsdev, HID_FEATURE_REPORT,
			report_id, usage_id,
			data_id | HID_USAGE_SENSOR_DATA_MOD_SENSITIVITY_ABS,
			&st->sensitivity);

	sensor_hub_input_get_attribute_info(hsdev, HID_FEATURE_REPORT,
			report_id, usage_id,
			data_id | HID_USAGE_SENSOR_DATA_MOD_PERIOD_MAX,
			&st->period_max);

	hid_dbg(hsdev->hdev,
		"common attributes for report id %u, usage_id %x, data_id %x: "
		"poll %x, report_state %x, power_state %x, "
		"sensitivity %x, period_max %x\n",
		report_id, usage_id, data_id,
		st->poll.index, st->report_state.index, st->power_state.index,
		st->sensitivity.index, st->period_max.index);

	hid_dbg(hsdev->hdev,
		"report_state: %d, %d, %u, %u, %d, %d\n",
		st->report_state.units, st->report_state.unit_expo,
		st->report_state.size, st->report_state.count,
		st->report_state.logical_minimum, st->report_state.logical_maximum);
	hid_dbg(hsdev->hdev,
		"power_state: %d, %d, %u, %u, %d, %d\n",
		st->power_state.units, st->power_state.unit_expo,
		st->power_state.size, st->power_state.count,
		st->power_state.logical_minimum, st->power_state.logical_maximum);

	return 0;
}
EXPORT_SYMBOL(hid_sensor_parse_common);

int hid_sensor_write_output_field(struct hid_sensor_common *st,
		const struct hid_sensor_hub_attribute_info *attr,
		int *val1, int *val2)
{
	s32 values[attr->count];
	unsigned i;
	int ret;

	for (i = 0; i < attr->count; i++)
		values[i] = convert_to(attr->unit_expo, val1[i], val2[i]);

	ret = sensor_hub_set_output(st->hsdev, st->report_id,
				    attr->index, values, attr->count);
	if (ret < 0)
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL(hid_sensor_write_output_field);

void hid_sensor_adjust_channel(struct iio_chan_spec *channels,
		int channel, enum iio_chan_type type,
		const struct hid_sensor_common *common,
		const struct hid_sensor_hub_attribute_info *info)
{
	if (info->logical_minimum < 0)
		channels[channel].scan_type.sign = 's';
	else
		channels[channel].scan_type.sign = 'u';
	/* Real storage bits will change based on the report desc. */
	channels[channel].scan_type.realbits = info->size * 8;
	/* Maximum size of a sample to capture is u32 */
	channels[channel].scan_type.storagebits = sizeof(u32) * 8;

	if (channels[channel].type == type)
		channels[channel].ext_info = common->ext_info;
}
EXPORT_SYMBOL(hid_sensor_adjust_channel);

MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@intel.com>");
MODULE_DESCRIPTION("HID Sensor common attribute processing");
MODULE_LICENSE("GPL");

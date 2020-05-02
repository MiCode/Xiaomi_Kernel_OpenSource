// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2018 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "atl_common.h"
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(4,10,0)

static ssize_t atl_hwmon_set_flag(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	struct atl_hw *hw = dev_get_drvdata(dev);
	bool val;
	int ret;

	if (strtobool(buf, &val) < 0)
		return -EINVAL;

	ret = atl_update_thermal_flag(hw, sattr->index, val);

	return ret ?: size;
}

static ssize_t atl_hwmon_show_flag(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	struct atl_hw *hw = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		!!(hw->thermal.flags & BIT(sattr->index)));
}

#define ATL_HWMON_BIT_ATTR(_name, _bit) \
	SENSOR_DEVICE_ATTR(_name, S_IRUGO | S_IWUSR, atl_hwmon_show_flag, \
		atl_hwmon_set_flag, _bit)

static ATL_HWMON_BIT_ATTR(monitor, atl_thermal_monitor_shift);
static ATL_HWMON_BIT_ATTR(throttle, atl_thermal_throttle_shift);
static ATL_HWMON_BIT_ATTR(ignore_lims, atl_thermal_ignore_lims_shift);

static struct attribute *atl_hwmon_attrs[] = {
	&sensor_dev_attr_monitor.dev_attr.attr,
	&sensor_dev_attr_throttle.dev_attr.attr,
	&sensor_dev_attr_ignore_lims.dev_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(atl_hwmon);

static char *atl_hwmon_labels[] = {
	"PHY Temperature",
};

static const uint32_t atl_hwmon_temp_config[] = {
	HWMON_T_INPUT | HWMON_T_LABEL | HWMON_T_CRIT | HWMON_T_MAX |
	HWMON_T_MAX_HYST | HWMON_T_MAX_ALARM,
	0,
};

static const struct hwmon_channel_info atl_hwmon_temp = {
	.type = hwmon_temp,
	.config = atl_hwmon_temp_config,
};

static const struct hwmon_channel_info *atl_hwmon_info[] = {
	&atl_hwmon_temp,
	NULL,
};

static umode_t atl_hwmon_is_visible(const void *p,
	enum hwmon_sensor_types type, uint32_t attr, int channel)
{
	if (type != hwmon_temp)
		return 0;

	switch (attr) {
	case hwmon_temp_input: case hwmon_temp_max_alarm: case hwmon_temp_label:
		return S_IRUGO;

	case hwmon_temp_crit: case hwmon_temp_max: case hwmon_temp_max_hyst:
		return S_IRUGO | S_IWUSR;
	}

	return 0;
}

static int atl_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
	uint32_t attr, int channel, long *val)
{
	struct atl_hw *hw = dev_get_drvdata(dev);
	int temp, ret;

	if (type != hwmon_temp)
		return -EINVAL;

	switch (attr) {
	case hwmon_temp_input:
		ret = hw->mcp.ops->get_phy_temperature(hw, &temp);
		if (ret)
			return ret;

		*val = temp;
		break;

	case hwmon_temp_crit:
		*val = hw->thermal.crit * 1000;
		break;

	case hwmon_temp_max:
		*val = hw->thermal.high * 1000;
		break;

	case hwmon_temp_max_hyst:
		*val = hw->thermal.low * 1000;
		break;

	case hwmon_temp_max_alarm:
		*val = hw->link_state.thermal_throttled;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

static int atl_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
	uint32_t attr, int channel, long val)
{
	struct atl_hw *hw = dev_get_drvdata(dev);
	uint8_t *ptr, old;
	int ret;

	if (type != hwmon_temp)
		return -EINVAL;

	switch (attr) {
	case hwmon_temp_crit:
		ptr = &hw->thermal.crit;
		break;

	case hwmon_temp_max:
		ptr = &hw->thermal.high;
		break;

	case hwmon_temp_max_hyst:
		ptr = &hw->thermal.low;
		break;

	default:
		return -EINVAL;
	}

	old = *ptr;
	*ptr = val / 1000;

	ret = atl_update_thermal(hw);
	if (ret)
		*ptr = old;

	return ret;
}

static int atl_hwmon_read_string(struct device *dev,
	enum hwmon_sensor_types type, u32 attr, int channel, const char **str)
{
	if (type != hwmon_temp || attr != hwmon_temp_label)
		return -EINVAL;

	*str = atl_hwmon_labels[channel];
	return 0;
}

static const struct hwmon_ops atl_hwmon_ops = {
	.is_visible = atl_hwmon_is_visible,
	.read = atl_hwmon_read,
	.read_string = atl_hwmon_read_string,
	.write = atl_hwmon_write,
};

static const struct hwmon_chip_info atl_hwmon = {
	.ops = &atl_hwmon_ops,
	.info = atl_hwmon_info,
};

int atl_hwmon_init(struct atl_nic *nic)
{
	struct device *hwmon_dev;
	struct atl_hw *hw = &nic->hw;

	hwmon_dev = devm_hwmon_device_register_with_info(&hw->pdev->dev,
		nic->ndev->name, hw, &atl_hwmon, atl_hwmon_groups);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

#else

int atl_hwmon_init(struct atl_nic *nic)
{
	return 0;
}

#endif

/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2018 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include "atl_common.h"
#include <linux/hwmon.h>

static char *atl_hwmon_labels[] = {
	"PHY Temperature",
};

static const uint32_t atl_hwmon_temp_config[] = {
	HWMON_T_INPUT | HWMON_T_LABEL,
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
	return type == hwmon_temp ? S_IRUGO : 0;
}

static int atl_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
	uint32_t attr, int channel, long *val)
{
	struct atl_hw *hw = dev_get_drvdata(dev);
	int temp, ret;

	if (type != hwmon_temp || attr != hwmon_temp_input)
		return -EINVAL;

	ret = hw->mcp.ops->get_phy_temperature(hw, &temp);
	if (ret)
		return ret;

	*val = temp;
	return 0;
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
};

static const struct hwmon_chip_info atl_hwmon = {
	.ops = &atl_hwmon_ops,
	.info = atl_hwmon_info,
};

int atl_hwmon_init(struct atl_nic *nic)
{
	struct device *hwmon_dev;

	hwmon_dev = devm_hwmon_device_register_with_info(&nic->hw.pdev->dev,
		nic->ndev->name, &nic->hw, &atl_hwmon, NULL);

	return PTR_ERR_OR_ZERO(hwmon_dev);
}

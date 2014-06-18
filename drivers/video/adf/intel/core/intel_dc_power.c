/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "core/intel_dc_config.h"

int intel_dc_power_init(struct intel_dc_power *power, struct device *dev,
	const struct intel_dc_config *config,
	const struct intel_dc_power_ops *ops)
{
	if (!power || !dev || !ops)
		return -EINVAL;

	memset(power, 0, sizeof(*power));

	power->dev = dev;
	power->config = config;
	power->ops = ops;

	return 0;
}

void intel_dc_power_destroy(struct intel_dc_power *power)
{
	if (power) {
		power->dev = NULL;
		power->config = NULL;
		power->ops = NULL;
	}
}

void intel_dc_power_suspend(struct intel_dc_power *power)
{
	if (power && power->ops && power->ops->suspend)
		power->ops->suspend(power);
}

void intel_dc_power_resume(struct intel_dc_power *power)
{
	if (power && power->ops && power->ops->resume)
		power->ops->resume(power);
}

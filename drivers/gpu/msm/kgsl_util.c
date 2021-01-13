// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */


#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>

#include "kgsl_util.h"

bool kgsl_regulator_disable_wait(struct regulator *reg, u32 timeout)
{
	ktime_t tout = ktime_add_us(ktime_get(), timeout * 1000);

	if (IS_ERR_OR_NULL(reg))
		return true;

	regulator_disable(reg);

	for (;;) {
		if (!regulator_is_enabled(reg))
			return true;

		if (ktime_compare(ktime_get(), tout) > 0)
			return (!regulator_is_enabled(reg));

		usleep_range((100 >> 2) + 1, 100);
	}
}

struct clk *kgsl_of_clk_by_name(struct clk_bulk_data *clks, int count,
		const char *id)
{
	int i;

	for (i = 0; clks && i < count; i++)
		if (!strcmp(clks[i].id, id))
			return clks[i].clk;

	return NULL;
}

int kgsl_regulator_set_voltage(struct device *dev,
		struct regulator *reg, u32 voltage)
{
	int ret;

	if (IS_ERR_OR_NULL(reg))
		return 0;

	ret = regulator_set_voltage(reg, voltage, INT_MAX);
	if (ret)
		dev_err(dev, "Regulator set voltage:%d failed:%d\n", voltage, ret);

	return ret;
}

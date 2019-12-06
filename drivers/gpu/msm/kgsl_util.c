// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/regulator/consumer.h>

#include "kgsl_util.h"

bool kgsl_regulator_disable_wait(struct regulator *reg, u32 timeout)
{
	ktime_t tout = ktime_add_us(ktime_get(), timeout * 1000);

	regulator_disable(reg);

	for (;;) {
		if (!regulator_is_enabled(reg))
			return true;

		if (ktime_compare(ktime_get(), tout) > 0)
			return (!regulator_is_enabled(reg));

		usleep_range((100 >> 2) + 1, 100);
	}
}

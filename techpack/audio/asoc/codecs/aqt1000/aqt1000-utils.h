/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 */

#ifndef __WCD9XXX_UTILS_H__
#define __WCD9XXX_UTILS_H__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/regmap.h>

struct regmap *aqt1000_regmap_init(struct device *dev,
				   const struct regmap_config *config);
#endif

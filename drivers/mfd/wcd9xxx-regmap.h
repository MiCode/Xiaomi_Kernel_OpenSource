/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _WCD9XXX_REGMAP_
#define _WCD9XXX_REGMAP_

#include <linux/regmap.h>
#include <linux/mfd/wcd9xxx/core.h>

#ifdef CONFIG_WCD9335_CODEC
extern struct regmap_config wcd9335_regmap_config;
#endif

static inline struct regmap_config *wcd9xxx_get_regmap_config(int type)
{
	struct regmap_config *regmap_config;

	switch (type) {
#ifdef CONFIG_WCD9335_CODEC
	case WCD9335:
		regmap_config = &wcd9335_regmap_config;
		break;
#endif
	default:
		regmap_config = NULL;
		break;
	};

	return regmap_config;
}

#endif

/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __WCD9XXX_UTILS_H__
#define __WCD9XXX_UTILS_H__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include "pdata.h"
#include "core.h"

struct wcd9xxx_pdata *wcd9xxx_populate_dt_data(struct device *dev);
int wcd9xxx_bringup(struct device *dev);
int wcd9xxx_bringdown(struct device *dev);
struct regmap *wcd9xxx_regmap_init(struct device *dev,
				   const struct regmap_config *config);
int wcd9xxx_reset(struct device *dev);
int wcd9xxx_reset_low(struct device *dev);
int wcd9xxx_get_codec_info(struct device *dev);

typedef int (*codec_bringup_fn)(struct wcd9xxx *);
typedef int (*codec_bringdown_fn)(struct wcd9xxx *);
typedef int (*codec_type_fn)(struct wcd9xxx *,
			     struct wcd9xxx_codec_type *);

codec_bringdown_fn wcd9xxx_bringdown_fn(int type);
codec_bringup_fn wcd9xxx_bringup_fn(int type);
codec_type_fn wcd9xxx_get_codec_info_fn(int type);

#endif

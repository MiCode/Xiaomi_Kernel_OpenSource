/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2018, 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __WCD9XXX_UTILS_H__
#define __WCD9XXX_UTILS_H__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <asoc/pdata.h>
#include <asoc/core.h>

struct wcd9xxx_pdata *wcd9xxx_populate_dt_data(struct device *dev);
int wcd9xxx_bringup(struct device *dev);
int wcd9xxx_bringdown(struct device *dev);
struct regmap *wcd9xxx_regmap_init(struct device *dev,
				   const struct regmap_config *config);
int wcd9xxx_reset(struct device *dev);
int wcd9xxx_reset_low(struct device *dev);
int wcd9xxx_get_codec_info(struct device *dev);

typedef int (*codec_bringup_fn)(struct wcd9xxx *dev);
typedef int (*codec_bringdown_fn)(struct wcd9xxx *dev);
typedef int (*codec_type_fn)(struct wcd9xxx *dev,
			     struct wcd9xxx_codec_type *wcd_type);

codec_bringdown_fn wcd9xxx_bringdown_fn(int type);
codec_bringup_fn wcd9xxx_bringup_fn(int type);
codec_type_fn wcd9xxx_get_codec_info_fn(int type);

#endif

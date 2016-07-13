/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/mfd/wcd9xxx/core.h>

struct wcd9xxx_pdata *wcd9xxx_populate_dt_data(struct device *dev);
int wcd9xxx_bringup(struct device *dev);
int wcd9xxx_bringdown(struct device *dev);
struct regmap *wcd9xxx_regmap_init(struct device *,
				   const struct regmap_config *);
int wcd9xxx_reset(struct device *dev);
int wcd9xxx_reset_low(struct device *dev);
int wcd9xxx_get_codec_info(struct device *dev);

typedef int (*codec_bringup_fn)(struct wcd9xxx *);
typedef int (*codec_bringdown_fn)(struct wcd9xxx *);
typedef int (*codec_type_fn)(struct wcd9xxx *,
			     struct wcd9xxx_codec_type *);

#ifdef CONFIG_WCD934X_CODEC
extern int wcd934x_bringup(struct wcd9xxx *wcd9xxx);
extern int wcd934x_bringdown(struct wcd9xxx *wcd9xxx);
extern int wcd934x_get_codec_info(struct wcd9xxx *,
				  struct wcd9xxx_codec_type *);
#endif

#ifdef CONFIG_WCD9335_CODEC
extern int wcd9335_bringup(struct wcd9xxx *wcd9xxx);
extern int wcd9335_bringdown(struct wcd9xxx *wcd9xxx);
extern int wcd9335_get_codec_info(struct wcd9xxx *,
				  struct wcd9xxx_codec_type *);
#endif

#ifdef CONFIG_WCD9330_CODEC
extern int wcd9330_bringup(struct wcd9xxx *wcd9xxx);
extern int wcd9330_bringdown(struct wcd9xxx *wcd9xxx);
extern int wcd9330_get_codec_info(struct wcd9xxx *,
				  struct wcd9xxx_codec_type *);
#endif

static inline codec_bringdown_fn wcd9xxx_bringdown_fn(int type)
{
	codec_bringdown_fn cdc_bdown_fn;

	switch (type) {
#ifdef CONFIG_WCD934X_CODEC
	case WCD934X:
		cdc_bdown_fn = wcd934x_bringdown;
		break;
#endif
#ifdef CONFIG_WCD9335_CODEC
	case WCD9335:
		cdc_bdown_fn = wcd9335_bringdown;
		break;
#endif
#ifdef CONFIG_WCD9330_CODEC
	case WCD9330:
		cdc_bdown_fn = wcd9330_bringdown;
		break;
#endif
	default:
		cdc_bdown_fn = NULL;
		break;
	}

	return cdc_bdown_fn;
}

static inline codec_bringup_fn wcd9xxx_bringup_fn(int type)
{
	codec_bringup_fn cdc_bup_fn;

	switch (type) {
#ifdef CONFIG_WCD934X_CODEC
	case WCD934X:
		cdc_bup_fn = wcd934x_bringup;
		break;
#endif
#ifdef CONFIG_WCD9335_CODEC
	case WCD9335:
		cdc_bup_fn = wcd9335_bringup;
		break;
#endif
#ifdef CONFIG_WCD9330_CODEC
	case WCD9330:
		cdc_bup_fn = wcd9330_bringup;
		break;
#endif
	default:
		cdc_bup_fn = NULL;
		break;
	}

	return cdc_bup_fn;
}

static inline codec_type_fn wcd9xxx_get_codec_info_fn(int type)
{
	codec_type_fn cdc_type_fn;

	switch (type) {
#ifdef CONFIG_WCD934X_CODEC
	case WCD934X:
		cdc_type_fn = wcd934x_get_codec_info;
		break;
#endif
#ifdef CONFIG_WCD9335_CODEC
	case WCD9335:
		cdc_type_fn = wcd9335_get_codec_info;
		break;
#endif
#ifdef CONFIG_WCD9330_CODEC
	case WCD9330:
		cdc_type_fn = wcd9330_get_codec_info;
		break;
#endif
	default:
		cdc_type_fn = NULL;
		break;
	}

	return cdc_type_fn;
}
#endif


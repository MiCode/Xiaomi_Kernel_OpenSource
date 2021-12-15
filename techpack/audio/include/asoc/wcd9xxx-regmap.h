/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */

#ifndef _WCD9XXX_REGMAP_
#define _WCD9XXX_REGMAP_

#include <linux/regmap.h>
#include "core.h"

typedef int (*regmap_patch_fptr)(struct regmap *regmap, int version);

extern struct regmap_config wcd934x_regmap_config;
extern int wcd934x_regmap_register_patch(struct regmap *regmap,
					 int version);

extern struct regmap_config wcd9335_regmap_config;
extern int wcd9335_regmap_register_patch(struct regmap *regmap,
					 int version);

static inline struct regmap_config *wcd9xxx_get_regmap_config(int type)
{
	struct regmap_config *regmap_config;

	switch (type) {
	case WCD934X:
		regmap_config = &wcd934x_regmap_config;
		break;
	case WCD9335:
		regmap_config = &wcd9335_regmap_config;
		break;
	default:
		regmap_config = NULL;
		break;
	};

	return regmap_config;
}

static inline regmap_patch_fptr wcd9xxx_get_regmap_reg_patch(int type)
{
	regmap_patch_fptr apply_patch;

	switch (type) {
	case WCD9335:
		apply_patch = wcd9335_regmap_register_patch;
		break;
	case WCD934X:
		apply_patch = wcd934x_regmap_register_patch;
		break;
	default:
		apply_patch = NULL;
		break;
	}

	return apply_patch;
}

#endif

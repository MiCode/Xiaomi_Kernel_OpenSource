/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_BL_H_
#define _SPRD_BL_H_

struct sprd_backlight {
	/* pwm backlight parameters */
	struct pwm_device *pwm;
	struct device dev;
	u32 max_level;
	u32 min_level;
	u32 dft_level;
	u32 scale;
	u32 *levels;
	u32 num;

	/* cabc backlight parameters */
	bool cabc_en;
	u32 cabc_level;
	u32 cabc_refer_level;
};

int sprd_cabc_backlight_update(struct backlight_device *bd);

void sprd_backlight_normalize_map(struct backlight_device *bd, u16 *level);

#endif
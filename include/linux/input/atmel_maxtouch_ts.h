/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (c) 2014, The Linux Foundation.  All rights reserved.
 *
 * Linux foundation chooses to take subject only to the GPLv2 license terms,
 * and distributes only under these terms.
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ATMEL_MAXTOUCH_TS_H
#define __LINUX_ATMEL_MAXTOUCH_TS_H

#include <linux/types.h>

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	unsigned long irqflags;
	unsigned long resetflags;
	int gpio_reset;
	int gpio_irq;
	int gpio_i2cmode;
	u8 t19_num_keys;
	const unsigned int *t19_keymap;
	int t15_num_keys;
	const unsigned int *t15_keymap;
	const char *cfg_name;

	const struct mxt_config_info *config_array;
	size_t config_array_size;

	/* touch panel's minimum and maximum coordinates */
	u32 panel_minx;
	u32 panel_maxx;
	u32 panel_miny;
	u32 panel_maxy;

	/* display's minimum and maximum coordinates */
	u32 disp_minx;
	u32 disp_maxx;
	u32 disp_miny;
	u32 disp_maxy;

	int *key_codes;
	u8 bl_addr;

	u8(*read_chg) (void);
	int (*init_hw) (bool);
	int (*power_on) (bool);
};

#endif /* __LINUX_ATMEL_MAXTOUCH_TS_H */

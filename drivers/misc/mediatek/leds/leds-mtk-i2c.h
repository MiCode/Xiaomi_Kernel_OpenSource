/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/****************************************************************************
 * LED DRV functions
 ***************************************************************************/
#include <linux/leds.h>

struct led_conf_info {
	struct led_classdev cdev;
	int led_bits;
	int trans_bits;
	int max_level;
};

int setMaxBrightness(char *name, int percent, bool enable);
int mt_leds_brightness_set(char *name, int level);
int mtk_leds_register_notifier(struct notifier_block *nb);
int mtk_leds_unregister_notifier(struct notifier_block *nb);

extern void disp_pq_notify_backlight_changed(int bl_1024);
extern int enable_met_backlight_tag(void);
extern int output_met_backlight_tag(int level);
extern int _gate_ic_i2c_write_bytes(unsigned char cmd, unsigned char writeData);
extern int _gate_ic_i2c_read_bytes(unsigned char cmd, unsigned char *returnData);
extern void _gate_ic_backlight_power_on(void);
extern void _gate_ic_backlight_set(unsigned int level);





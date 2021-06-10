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
extern struct led_conf_info {
	int level;
	int led_bits;
	int trans_bits;
	int max_level;
	struct led_classdev cdev;
} led_conf_info;

int mtk_leds_register_notifier(struct notifier_block *nb);
int mtk_leds_unregister_notifier(struct notifier_block *nb);
int mt_leds_brightness_set(char *name, int bl_1024);
int setMaxBrightness(char *name, int percent, bool enable);


extern int mtkfb_set_backlight_level(unsigned int level);
extern void disp_pq_notify_backlight_changed(int bl_1024);
extern int enable_met_backlight_tag(void);
extern int output_met_backlight_tag(int level);


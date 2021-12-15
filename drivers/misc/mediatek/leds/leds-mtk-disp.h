// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
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
int mt_leds_max_brightness_set(char *name, int percent, bool enable);


extern int mtkfb_set_backlight_level(unsigned int level);


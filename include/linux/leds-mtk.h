// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#include <linux/leds.h>

enum mtk_leds_events {
	LED_BRIGHTNESS_CHANGED	= 0,
	LED_STATUS_SHUTDOWN	= 1
};

struct led_conf_info {
	int max_hw_brightness;
	int limit_hw_brightness;
	unsigned int aal_enable;
	struct led_classdev cdev;
} led_conf_info;

int mtk_leds_register_notifier(struct notifier_block *nb);
int mtk_leds_unregister_notifier(struct notifier_block *nb);
int mtk_leds_brightness_set(char *name, int bl_1024);
int setMaxBrightness(char *name, int percent, bool enable);

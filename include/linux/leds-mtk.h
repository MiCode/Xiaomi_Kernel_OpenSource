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

enum led_mode {
	MT_LED_MODE_CUST_LCM = 4,
	MT_LED_MODE_CUST_BLS_PWM,
	MT_LED_MODE_CUST_BLS_I2C
};
struct led_conf_info {
	int max_hw_brightness;
	int limit_hw_brightness;
	int min_brightness;
	unsigned int aal_enable;
	struct led_classdev cdev;
	int flags;
	enum led_mode mode;
	int connector_id;
#define LED_MT_BRIGHTNESS_HW_CHANGED	BIT(1)
#define LED_MT_BRIGHTNESS_CHANGED	BIT(2)

#ifdef CONFIG_LEDS_MT_BRIGHTNESS_HW_CHANGED
	int brightness_hw_changed;
	struct kernfs_node	*brightness_hw_changed_kn;
#endif
	};

#ifdef CONFIG_LEDS_MT_BRIGHTNESS_HW_CHANGED
void mtk_leds_notify_brightness_hw_changed(
	struct led_conf_info *led_conf, enum led_brightness brightness);
#else
static inline void mtk_leds_notify_brightness_hw_changed(
	struct led_conf_info *led_conf, enum led_brightness brightness) { }
#endif

int mtk_leds_register_notifier(struct notifier_block *nb);
int mtk_leds_unregister_notifier(struct notifier_block *nb);
int mtk_leds_brightness_set(char *name, int bl_1024);
int setMaxBrightness(char *name, int percent, bool enable);

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#include<linux/leds-mtk.h>

extern struct list_head leds_list;

struct led_debug_info {
	unsigned long long current_t;
	unsigned long long last_t;
	char buffer[4096];
	int count;
};

struct led_desp {
	int index;
	char name[16];
};

struct mt_led_data {
	struct led_desp desp;
	struct led_conf_info	conf;
	int last_brightness;
	int hw_brightness;
	int last_hw_brightness;
	struct led_debug_info debug;
	int (*mtk_hw_brightness_set)(struct mt_led_data *m_data,
		int brightness);
	struct mutex	led_access;
};

int mt_leds_parse_dt(struct mt_led_data *mdev, struct fwnode_handle *fwnode);
int mt_leds_classdev_register(struct device *parent,
					 struct mt_led_data *led_dat);
void mt_leds_classdev_unregister(struct device *parent,
				     struct mt_led_data *led_dat);
int mt_leds_call_notifier(unsigned long action, void *data);

extern int mtkfb_set_backlight_level(unsigned int level);

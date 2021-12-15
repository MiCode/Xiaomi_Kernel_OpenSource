// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
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

int mt_leds_max_brightness_set(char *name, int percent, bool enable);
int mt_leds_brightness_set(char *name, int level);
int mtk_leds_register_notifier(struct notifier_block *nb);
int mtk_leds_unregister_notifier(struct notifier_block *nb);

extern int _gate_ic_i2c_write_bytes(unsigned char cmd, unsigned char writeData);
extern int _gate_ic_i2c_read_bytes(unsigned char cmd, unsigned char *returnData);
extern void _gate_ic_backlight_power_on(void);
extern void _gate_ic_backlight_set(unsigned int level);





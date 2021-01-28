/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _MT_LED_INFO
#define _MT_LED_INFO

#include <linux/leds.h>


#define MT_LED_MAGIC_CODE  (5527890) /* 0x545505 */
#define MT_LED_MAGIC_MASK  (0xffffff)

#define MT_LED_MAGIC_CC_MODE     (1 << 24)
#define MT_LED_MAGIC_PWM_MODE    (1 << 25)
#define MT_LED_MAGIC_BREATH_MODE (1 << 26)

#define MT_LED_CC_MAGIC_CODE    (MT_LED_MAGIC_CC_MODE|MT_LED_MAGIC_CODE)
#define MT_LED_PWM_MAGIC_CODE   (MT_LED_MAGIC_PWM_MODE|MT_LED_MAGIC_CODE)
#define MT_LED_BREATH_MAGIC_CODE   (MT_LED_MAGIC_BREATH_MODE|MT_LED_MAGIC_CODE)

#define MT_LED_ALL_MAGIC_CODE  (MT_LED_MAGIC_CC_MODE|MT_LED_MAGIC_PWM_MODE|\
			MT_LED_MAGIC_BREATH_MODE|MT_LED_MAGIC_CODE)


enum {
	CC_MODE_ATTR_SFSTR,
};

enum {
	PWM_MODE_ATTR_DIM_DUTY,
	PWM_MODE_ATTR_DIM_FREQ,
	PWM_MODE_ATTR_LIST_DUTY,
	PWM_MODE_ATTR_LIST_FREQ,
};

enum {
	BREATH_MODE_ATTR_TR1,
	BREATH_MODE_ATTR_TR2,
	BREATH_MODE_ATTR_TF1,
	BREATH_MODE_ATTR_TF2,
	BREATH_MODE_ATTR_TON,
	BREATH_MODE_ATTR_TOFF,
	BREATH_MODE_ATTR_LIST_TIME,
};

enum {
	MT_LED_CC_MODE,
	MT_LED_PWM_MODE,
	MT_LED_BREATH_MODE,
	MT_LED_MODE_MAX,
};

extern const char *mt_led_trigger_mode_name[MT_LED_MODE_MAX];

struct mt_led_info;

struct mt_led_ops {
	int (*change_mode)(struct led_classdev *led, int mode);
	int (*get_soft_start_step)(struct mt_led_info *info);
	int (*set_soft_start_step)(struct mt_led_info *info, int ns);
	int (*get_pwm_dim_duty)(struct mt_led_info *info);
	int (*set_pwm_dim_duty)(struct mt_led_info *info, int duty);
	int (*get_pwm_dim_freq)(struct mt_led_info *info);
	int (*set_pwm_dim_freq)(struct mt_led_info *info, int freq);
	int (*list_pwm_duty)(struct mt_led_info *info, char *buf);
	int (*list_pwm_freq)(struct mt_led_info *info, char *buf);
	int (*get_breath_tr1)(struct mt_led_info *info);
	int (*get_breath_tr2)(struct mt_led_info *info);
	int (*get_breath_tf1)(struct mt_led_info *info);
	int (*get_breath_tf2)(struct mt_led_info *info);
	int (*get_breath_ton)(struct mt_led_info *info);
	int (*get_breath_toff)(struct mt_led_info *info);
	int (*set_breath_tr1)(struct mt_led_info *info, int time);
	int (*set_breath_tr2)(struct mt_led_info *info, int time);
	int (*set_breath_tf1)(struct mt_led_info *info, int time);
	int (*set_breath_tf2)(struct mt_led_info *info, int time);
	int (*set_breath_ton)(struct mt_led_info *info, int time);
	int (*set_breath_toff)(struct mt_led_info *info, int time);
	int (*list_breath_time)(struct mt_led_info *info, char *buf);
};

struct mt_led_info {
	struct led_classdev led;
	struct mt_led_ops *ops;
	uint32_t magic_code;
};

extern int mt_led_trigger_register(struct mt_led_ops *ops);
extern void mt_led_trigger_unregister(void);

#endif /* _MT_LED_INFO */

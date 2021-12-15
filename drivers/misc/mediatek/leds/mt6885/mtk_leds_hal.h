/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _LEDS_HAL_H
#define _LEDS_HAL_H

#include "mtk_leds_sw.h"

/****************************************************************************
 * LED HAL functions
 ***************************************************************************/
extern void mt_leds_wake_lock_init(void);
extern void mt_led_pwm_disable(int pwm_num);
extern int mt_led_set_pwm(int pwm_num, struct nled_setting *led);
extern int mt_led_blink_pmic(enum mt65xx_led_pmic pmic_type,
			     struct nled_setting *led);
extern int mt_backlight_set_pwm(int pwm_num, u32 level, u32 div,
				struct PWM_config *config_data);
extern int mt_brightness_set_pmic(enum mt65xx_led_pmic pmic_type, u32 level,
				  u32 div);
extern int mt_mt65xx_led_set_cust(struct cust_mt65xx_led *cust, int level);
extern void mt_mt65xx_led_work(struct work_struct *work);
extern void mt_mt65xx_led_set(struct led_classdev *led_cdev,
			      enum led_brightness level);
extern int mt_mt65xx_blink_set(struct led_classdev *led_cdev,
		unsigned long *delay_on, unsigned long *delay_off);

extern struct cust_mt65xx_led *mt_get_cust_led_list(void);

#if defined(CONFIG_BACKLIGHT_SUPPORT_LM3697)
extern int chargepump_set_backlight_level(unsigned int level);
#endif

#endif

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

#ifndef MTK_RTC_H
#define MTK_RTC_H

#include <linux/rtc.h>

enum rtc_gpio_user_t {
	RTC_GPIO_USER_WIFI = 8,
	RTC_GPIO_USER_GPS = 9,
	RTC_GPIO_USER_BT = 10,
	RTC_GPIO_USER_FM = 11,
	RTC_GPIO_USER_PMIC = 12,
};

#ifdef CONFIG_MTK_RTC

/*
 * NOTE:
 * 1. RTC_GPIO always exports 32K enabled
 * by some user even if the phone is powered off
 */

extern void rtc_gpio_enable_32k(enum rtc_gpio_user_t user);
extern void rtc_gpio_disable_32k(enum rtc_gpio_user_t user);
extern void rtc_mark_recovery(void);
extern void rtc_mark_kpoc(void);
extern void rtc_mark_fast(void);
extern void rtc_read_pwron_alarm(struct rtc_wkalrm *alm);
extern int get_rtc_spare_fg_value(void);
extern int set_rtc_spare_fg_value(int val);
extern int get_rtc_spare0_fg_value(void);
extern int set_rtc_spare0_fg_value(int val);
extern bool crystal_exist_status(void);
#else
#define rtc_gpio_enable_32k(user)	({ 0; })
#define rtc_gpio_disable_32k(user)	({ 0; })
#define rtc_mark_recovery()             ({ 0; })
#define rtc_mark_kpoc()                 ({ 0; })
#define rtc_mark_fast()		        ({ 0; })
#define rtc_read_pwron_alarm(alm)	({ 0; })
#define get_rtc_spare_fg_value()	({ 0; })
#define set_rtc_spare_fg_value(val)	({ 0; })
#define get_rtc_spare0_fg_value()		({ 0; })
#define set_rtc_spare0_fg_value(val)	({ 0; })
#define crystal_exist_status()		({ 0; })
#endif/*ifdef CONFIG_MTK_RTC*/

#endif

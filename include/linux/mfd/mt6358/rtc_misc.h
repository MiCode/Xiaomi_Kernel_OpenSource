/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
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

#ifdef CONFIG_MT6358_MISC
void rtc_enable_32k1v8_1(void);
void rtc_disable_32k1v8_1(void);
#else
#define rtc_enable_32k1v8_1()		do {} while (0)
#define rtc_disable_32k1v8_1()		do {} while (0)
#endif

#endif

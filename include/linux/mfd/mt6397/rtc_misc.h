/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6397_RTC_MISC_H__
#define __MT6397_RTC_MISC_H__
#include <linux/types.h>

enum rtc_gpio_user_t {
	RTC_GPIO_USER_WIFI = 8,
	RTC_GPIO_USER_GPS = 9,
	RTC_GPIO_USER_BT = 10,
	RTC_GPIO_USER_FM = 11,
	RTC_GPIO_USER_PMIC = 12,
};

#ifdef CONFIG_MT6397_MISC
extern void mtk_misc_mark_fast(void);
extern void mtk_misc_mark_recovery(void);
extern bool mtk_misc_low_power_detected(void);
extern bool mtk_misc_crystal_exist_status(void);
extern int mtk_misc_set_spare_fg_value(u32 val);
extern u32 mtk_misc_get_spare_fg_value(void);
extern void rtc_gpio_enable_32k(enum rtc_gpio_user_t user);
extern void rtc_gpio_disable_32k(enum rtc_gpio_user_t user);
#else
#define mtk_misc_mark_fast()			do {} while (0)
#define mtk_misc_mark_recovey()			do {} while (0)
#define mtk_misc_low_power_detected()		({ 0; })
#define mtk_misc_crystal_exist_status()		({ 0; })
#define mtk_misc_set_spare_fg_value(val)	({ 0; })
#define mtk_misc_get_spare_fg_value()		({ 0; })
#define rtc_gpio_enable_32k(user)		do {} while (0)
#define rtc_gpio_disable_32k(user)		do {} while (0)
#endif
#endif /* __MT6397_RTC_MISC_H__ */


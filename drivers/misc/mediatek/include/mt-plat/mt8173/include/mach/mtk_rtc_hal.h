/*
 * Copyright (C) 2010 MediaTek, Inc.
 *
 * Author: Terry Chang <terry.chang@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MTK_RTC_HAL_H_
#define _MTK_RTC_HAL_H_

#include <linux/kernel.h>
#include <linux/rtc.h>

#define RTC_GPIO_USER_MASK	(((1U << 13) - 1) & 0xff00)

/* RTC registers */
#define	RTC_BASE					(0xe000)

extern u16 hal_rtc_get_gpio_32k_status(void);
extern void hal_rtc_set_gpio_32k_status(u16 user, bool enable);
extern void hal_rtc_set_abb_32k(u16 enable);
extern void hal_rtc_bbpu_pwdn(void);
extern void hal_rtc_get_pwron_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm);
extern bool hal_rtc_is_lp_irq(void);
extern bool hal_rtc_is_pwron_alarm(struct rtc_time *nowtm, struct rtc_time *tm);
extern void hal_rtc_get_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm);
extern void hal_rtc_set_alarm(struct rtc_time *tm);
extern void hal_rtc_clear_alarm(struct rtc_time *tm);
extern void hal_rtc_set_lp_irq(void);
extern void hal_rtc_save_pwron_time(bool enable, struct rtc_time *tm, bool logo);

#endif

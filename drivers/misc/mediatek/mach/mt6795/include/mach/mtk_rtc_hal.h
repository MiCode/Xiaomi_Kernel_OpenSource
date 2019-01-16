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
#define	RTC_BASE					(0x4000)

extern u16 hal_rtc_get_register_status(const char * cmd);
extern void hal_rtc_set_register_status(const char * cmd, u16 val);
extern void hal_rtc_set_gpio_32k_status(u16 user, bool enable);
extern void hal_rtc_set_writeif(bool enable);
extern void hal_rtc_mark_mode(const char *cmd);
extern u16 hal_rtc_rdwr_uart(u16 *val);
extern void hal_rtc_bbpu_pwdn(void);
extern void hal_rtc_get_pwron_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm);
extern void hal_rtc_set_pwron_alarm(void);
extern bool hal_rtc_is_lp_irq(void);
extern void hal_rtc_reload_power(void);
extern void hal_rtc_get_tick_time(struct rtc_time *tm);
extern void hal_rtc_set_tick_time(struct rtc_time *tm);
extern bool hal_rtc_check_pwron_alarm_rg(struct rtc_time *nowtm, struct rtc_time *tm);
extern void hal_rtc_get_alarm_time(struct rtc_time *tm, struct rtc_wkalrm *alm);
extern void hal_rtc_set_alarm_time(struct rtc_time *tm);
extern void hal_rtc_clear_alarm(struct rtc_time *tm);
extern void hal_rtc_set_lp_irq(void);
extern void hal_rtc_read_rg(void);
extern void hal_rtc_save_pwron_time(bool enable, struct rtc_time *tm, bool logo);

#endif

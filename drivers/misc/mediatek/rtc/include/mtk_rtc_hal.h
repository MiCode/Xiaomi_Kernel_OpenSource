/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (C) 2021 MediaTek Inc.
*/

#ifndef _MTK_RTC_HAL_H_
#define _MTK_RTC_HAL_H_

#include <linux/kernel.h>
#include <linux/rtc.h>

extern u16 hal_rtc_get_gpio_32k_status(void);
extern void hal_rtc_set_gpio_32k_status(u16 user, bool enable);
extern void hal_rtc_set_abb_32k(u16 enable);
extern void hal_rtc_bbpu_pwdn(bool charger_status);
extern void hal_rtc_get_pwron_alarm(struct rtc_time *tm,
	struct rtc_wkalrm *alm);
extern bool hal_rtc_is_lp_irq(void);
extern bool hal_rtc_is_pwron_alarm(struct rtc_time *nowtm, struct rtc_time *tm);
extern void hal_rtc_get_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm);
extern void hal_rtc_set_alarm(struct rtc_time *tm);
extern void hal_rtc_clear_alarm(struct rtc_time *tm);
extern void hal_rtc_set_lp_irq(void);
extern void hal_rtc_save_pwron_time(bool enable,
	struct rtc_time *tm, bool logo);
extern int mmc_charge_shutdown(void);
extern int __init rtc_debug_init(void);
#endif

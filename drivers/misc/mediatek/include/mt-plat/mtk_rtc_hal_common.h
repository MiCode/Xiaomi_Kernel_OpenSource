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

#ifndef MTK_RTC_HAL_COMMON_H
#define MTK_RTC_HAL_COMMON_H

#include <linux/ioctl.h>
#include <linux/rtc.h>
#include <linux/types.h>

enum rtc_spare_enum {
	RTC_FGSOC = 0,
	RTC_ANDROID,
	RTC_FAC_RESET,
	RTC_BYPASS_PWR,
	RTC_PWRON_TIME,
	RTC_FAST_BOOT,
	RTC_KPOC,
	RTC_DEBUG,
	RTC_PWRON_AL,
	RTC_UART,
	RTC_AUTOBOOT,
	RTC_PWRON_LOGO,
	RTC_32K_LESS,
	RTC_LP_DET,
	RTC_FG_INIT,
	RTC_SPAR_NUM
};

enum rtc_reg_set {
	RTC_REG,
	RTC_MASK,
	RTC_SHIFT
};

#ifdef CONFIG_MTK_RTC
extern u16 rtc_read(u16 addr);
extern void rtc_write(u16 addr, u16 data);
extern void rtc_write_trigger(void);
extern void rtc_writeif_unlock(void);
extern void rtc_xosc_write(u16 val, bool reload);
extern void rtc_set_writeif(bool enable);
extern void rtc_bbpu_pwrdown(bool auto_boot);
extern void hal_rtc_set_spare_register(enum rtc_spare_enum cmd, u16 val);
extern u16 hal_rtc_get_spare_register(enum rtc_spare_enum cmd);
extern void hal_rtc_get_tick_time(struct rtc_time *tm);
extern void hal_rtc_set_tick_time(struct rtc_time *tm);
extern void hal_rtc_get_alarm_time(struct rtc_time *tm);
extern void hal_rtc_set_alarm_time(struct rtc_time *tm);
extern void hal_rtc_save_pwron_alarm(void);
extern void hal_rtc_get_pwron_alarm_time(struct rtc_time *tm);
extern void hal_rtc_set_pwron_alarm_time(struct rtc_time *tm);
extern void hal_rtc_read_rg(void);
#ifndef USER_BUILD_KERNEL
extern void rtc_lp_exception(void);
#endif

#else
static inline void hal_rtc_set_spare_register(enum rtc_spare_enum cmd, u16 val)
{
}

static inline u16 hal_rtc_get_spare_register(enum rtc_spare_enum cmd)
{
	return 0;
}
#endif
#endif

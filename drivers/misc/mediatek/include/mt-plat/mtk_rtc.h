/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

extern unsigned long rtc_read_hw_time(void);
extern void rtc_gpio_enable_32k(enum rtc_gpio_user_t user);
extern void rtc_gpio_disable_32k(enum rtc_gpio_user_t user);
extern bool rtc_gpio_32k_status(void);

/* for AUDIOPLL (deprecated) */
extern void rtc_enable_abb_32k(void);
extern void rtc_disable_abb_32k(void);

/* NOTE: used in Sleep driver to workaround Vrtc-Vore level shifter issue */
extern void rtc_enable_writeif(void);
extern void rtc_disable_writeif(void);

extern void rtc_mark_recovery(void);
extern void rtc_mark_kpoc(void);
extern void rtc_mark_fast(void);
extern u16 rtc_rdwr_uart_bits(u16 *val);
extern void rtc_bbpu_power_down(void);
extern void rtc_read_pwron_alarm(struct rtc_wkalrm *alm);
extern int get_rtc_spare_fg_value(void);
extern int set_rtc_spare_fg_value(int val);
extern int get_rtc_spare0_fg_value(void);
extern int set_rtc_spare0_fg_value(int val);
extern void rtc_irq_handler(void);
extern bool crystal_exist_status(void);
extern void mt_power_off(void);
#elif defined(CONFIG_MT6358_MISC)
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
#else/*ifdef CONFIG_MTK_RTC*/
#define rtc_read_hw_time()              ({ 0; })
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
void __attribute__((weak)) rtc_clock_enable(int enable)
{
}
void __attribute__((weak)) rtc_lpsd_restore_al_mask(void)
{
}
void __attribute__((weak)) rtc_reset_bbpu_alarm_status(void)
{
}
#endif

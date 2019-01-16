/*
 * Copyright (C) 2010 MediaTek, Inc.
 *
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <mach/upmu_hw.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <mach/mtk_rtc_hal.h>
#include "mach/mt_rtc_hw.h"
#include <mach/mt_typedefs.h>
#include <mach/mt_pmic_wrap.h>
#if defined CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include <mach/system.h>
#include <mach/mt_boot.h>
#endif
#include <rtc-mt.h>		/* custom file */
#if 0
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt
#endif
/* #include <linux/printk.h> */

#define XLOG_MYTAG	"Power/RTC"

#if 1
#define hal_rtc_xinfo(fmt, args...)		\
	pr_debug(fmt, ##args)

#define hal_rtc_xerror(fmt, args...)	\
	pr_err(fmt, ##args)

#define hal_rtc_xfatal(fmt, args...)	\
	pr_emerg(fmt, ##args)
#else

#define hal_rtc_xinfo(fmt, args...)		\
	xlog_printk(ANDROID_LOG_INFO, XLOG_MYTAG, fmt, ##args)

#define hal_rtc_xerror(fmt, args...)	\
	xlog_printk(ANDROID_LOG_ERROR, XLOG_MYTAG, fmt, ##args)

#define hal_rtc_xfatal(fmt, args...)	\
	xlog_printk(ANDROID_LOG_FATAL, XLOG_MYTAG, fmt, ##args)
#endif
static u16 rtc_read(u16 addr)
{
	u32 rdata = 0;

	pwrap_read((u32) addr, &rdata);
	return (u16) rdata;
}

static void rtc_write(u16 addr, u16 data)
{
	pwrap_write((u32) addr, (u32) data);
}

#define rtc_busy_wait()					\
do {							\
	while (rtc_read(RTC_BBPU) & RTC_BBPU_CBUSY);	\
} while (0)


static void rtc_write_trigger(void)
{
	rtc_write(RTC_WRTGR, 1);
	rtc_busy_wait();
}

static void rtc_writeif_unlock(void)
{
	rtc_write(RTC_PROT, RTC_PROT_UNLOCK1);
	rtc_write_trigger();
	rtc_write(RTC_PROT, RTC_PROT_UNLOCK2);
	rtc_write_trigger();
}

static void hal_rtc_set_spare_fg_value(u16 val)
{
	/* RTC_AL_HOU bit8~14 */
	u16 temp;

	rtc_writeif_unlock();
	temp = rtc_read(RTC_AL_HOU);
	val = (val & (RTC_NEW_SPARE_FG_MASK >> RTC_NEW_SPARE_FG_SHIFT)) << RTC_NEW_SPARE_FG_SHIFT;
	temp = (temp & RTC_AL_HOU_MASK) | val;
	rtc_write(RTC_AL_HOU, temp);
	rtc_write_trigger();
}

static void hal_rtc_set_auto_boot(u16 enable) {
	u16 pdn2;

	if (enable)
		pdn2 = rtc_read(RTC_PDN2) & ~RTC_PDN2_AUTOBOOT; //bit 7 for auto boot;
	else
		pdn2 = rtc_read(RTC_PDN2) | RTC_PDN2_AUTOBOOT;
	rtc_write(RTC_PDN2, pdn2);
rtc_write_trigger();
}

static hal_rtc_set_auto_bit(int val)
{
	if (val == 0) {
		rtc_write(RTC_BBPU, RTC_BBPU_KEY | RTC_BBPU_BBPU | RTC_BBPU_PWREN);
	} else {
		rtc_write(RTC_BBPU, RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_BBPU | RTC_BBPU_PWREN);
	}
	rtc_write_trigger();	
	hal_rtc_xinfo("hal_rtc_set_auto_bit(RTC_BBPU=0x%x)\n", rtc_read(RTC_BBPU));
}

u16 hal_rtc_get_register_status(const char *cmd)
{
	u16 spar0, al_hou, pdn1, con;

	if (!strcmp(cmd, "XTAL")) {
		/*RTC_SPAR0 bit 6        : 32K less bit. True:with 32K, False:Without 32K */
		spar0 = rtc_read(RTC_SPAR0);
		if (spar0 & RTC_SPAR0_32K_LESS)
			return 1;
		else
			return 0;
	} else if (!strcmp(cmd, "LPD")) {
		spar0 = rtc_read(RTC_SPAR0);
		if (spar0 & RTC_SPAR0_LP_DET)
			return 1;
		else
			return 0;
	} else if (!strcmp(cmd, "FG")) {
		/* RTC_AL_HOU bit8~14 */
		al_hou = rtc_read(RTC_AL_HOU);
		al_hou = (al_hou & RTC_NEW_SPARE_FG_MASK) >> RTC_NEW_SPARE_FG_SHIFT;
		return al_hou;
	} else if (!strcmp(cmd, "GPIO")) {
		pdn1 = rtc_read(RTC_PDN1);
		con = rtc_read(RTC_CON);

		hal_rtc_xinfo("RTC_GPIO 32k status(RTC_PDN1=0x%x)(RTC_CON=0x%x)\n", pdn1, con);

		if (con & RTC_CON_F32KOB)
			return 0;
		else
			return 1;
	}

	return 0;
}

void hal_rtc_set_register_status(const char *cmd, u16 val)
{
	if (!strcmp(cmd, "FG")) {
		hal_rtc_set_spare_fg_value(val);
	} else if (!strcmp(cmd, "AUTOBOOT")) {
		hal_rtc_set_auto_boot(val);
	} else if (!strcmp(cmd, "AUTO")) {
		hal_rtc_set_auto_bit(val);	
	}
}

void hal_rtc_set_gpio_32k_status(u16 user, bool enable)
{
	u16 con, pdn1;

	if (enable) {
		pdn1 = rtc_read(RTC_PDN1);
	} else {
		pdn1 = rtc_read(RTC_PDN1) & ~(1U << user);
		rtc_write(RTC_PDN1, pdn1);
		rtc_write_trigger();
	}

	con = rtc_read(RTC_CON);
	if (enable) {
		con &= ~RTC_CON_F32KOB;
	} else {
		if (!(pdn1 & RTC_GPIO_USER_MASK)) {	/* no users */
			con |= RTC_CON_F32KOB;
		}
	}
	rtc_write(RTC_CON, con);
	rtc_write_trigger();

	if (enable) {
		pdn1 |= (1U << user);
		rtc_write(RTC_PDN1, pdn1);
		rtc_write_trigger();
	}
	hal_rtc_xinfo("RTC_GPIO user %d enable = %d 32k (0x%x)\n", user, enable, pdn1);
}

void hal_rtc_set_writeif(bool enable)
{
	if (enable) {
		rtc_writeif_unlock();
	} else {
		rtc_write(RTC_PROT, 0);
		rtc_write_trigger();
	}
}

void hal_rtc_mark_mode(const char *cmd)
{
	u16 pdn1;

	if (!strcmp(cmd, "recv")) {
		pdn1 = rtc_read(RTC_PDN1) & (~RTC_PDN1_RECOVERY_MASK);
		rtc_write(RTC_PDN1, pdn1 | RTC_PDN1_FAC_RESET);
	} else if (!strcmp(cmd, "kpoc")) {
		pdn1 = rtc_read(RTC_PDN1) & (~RTC_PDN1_KPOC);
		rtc_write(RTC_PDN1, pdn1 | RTC_PDN1_KPOC);
	} else if (!strcmp(cmd, "fast")) {
		pdn1 = rtc_read(RTC_PDN1) & (~RTC_PDN1_FAST_BOOT);
		rtc_write(RTC_PDN1, pdn1 | RTC_PDN1_FAST_BOOT);
	}
	rtc_write_trigger();
}

u16 hal_rtc_rdwr_uart(u16 *val)
{
	u16 pdn2;

	if (val) {
		pdn2 = rtc_read(RTC_PDN2) & (~RTC_PDN2_UART_MASK);
		pdn2 |= (*val & (RTC_PDN2_UART_MASK >> RTC_PDN2_UART_SHIFT)) << RTC_PDN2_UART_SHIFT;
		rtc_write(RTC_PDN2, pdn2);
		rtc_write_trigger();
	}
	pdn2 = rtc_read(RTC_PDN2);

	return (pdn2 & RTC_PDN2_UART_MASK) >> RTC_PDN2_UART_SHIFT;
}

void hal_rtc_bbpu_pwdn(void)
{
	u16 bbpu, con;

	rtc_writeif_unlock();
	/* disable 32K export if there are no RTC_GPIO users */
	if (!(rtc_read(RTC_PDN1) & RTC_GPIO_USER_MASK)) {
		con = rtc_read(RTC_CON) | RTC_CON_F32KOB;
		rtc_write(RTC_CON, con);
		rtc_write_trigger();
	}

	/* pull PWRBB low */
	bbpu = RTC_BBPU_KEY | RTC_BBPU_AUTO | RTC_BBPU_PWREN;
	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
}

void hal_rtc_get_pwron_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm)
{
	u16 pdn1, pdn2, spar0, spar1;


	pdn1 = rtc_read(RTC_PDN1);
	pdn2 = rtc_read(RTC_PDN2);
	spar0 = rtc_read(RTC_SPAR0);
	spar1 = rtc_read(RTC_SPAR1);

	alm->enabled = (pdn1 & RTC_PDN1_PWRON_TIME ? (pdn2 & RTC_PDN2_PWRON_LOGO ? 3 : 2) : 0);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);	/* return Power-On Alarm bit */
	tm->tm_year = ((pdn2 & RTC_PDN2_PWRON_YEA_MASK) >> RTC_PDN2_PWRON_YEA_SHIFT);
	tm->tm_mon = ((pdn2 & RTC_PDN2_PWRON_MTH_MASK) >> RTC_PDN2_PWRON_MTH_SHIFT);
	tm->tm_mday = ((spar1 & RTC_SPAR1_PWRON_DOM_MASK) >> RTC_SPAR1_PWRON_DOM_SHIFT);
	tm->tm_hour = ((spar1 & RTC_SPAR1_PWRON_HOU_MASK) >> RTC_SPAR1_PWRON_HOU_SHIFT);
	tm->tm_min = ((spar1 & RTC_SPAR1_PWRON_MIN_MASK) >> RTC_SPAR1_PWRON_MIN_SHIFT);
	tm->tm_sec = ((spar0 & RTC_SPAR0_PWRON_SEC_MASK) >> RTC_SPAR0_PWRON_SEC_SHIFT);
}

void hal_rtc_set_pwron_alarm(void)
{
	rtc_write(RTC_PDN1, rtc_read(RTC_PDN1) & (~RTC_PDN1_PWRON_TIME));
	rtc_write(RTC_PDN2, rtc_read(RTC_PDN2) | RTC_PDN2_PWRON_ALARM);
	rtc_write_trigger();
}

#ifndef USER_BUILD_KERNEL
static void rtc_lp_exception(void)
{
	u16 bbpu, irqsta, irqen, osc32;
	u16 pwrkey1, pwrkey2, prot, con, sec1, sec2;

	bbpu = rtc_read(RTC_BBPU);
	irqsta = rtc_read(RTC_IRQ_STA);
	irqen = rtc_read(RTC_IRQ_EN);
	osc32 = rtc_read(RTC_OSC32CON);
	pwrkey1 = rtc_read(RTC_POWERKEY1);
	pwrkey2 = rtc_read(RTC_POWERKEY2);
	prot = rtc_read(RTC_PROT);
	con = rtc_read(RTC_CON);
	sec1 = rtc_read(RTC_TC_SEC);
	mdelay(2000);
	sec2 = rtc_read(RTC_TC_SEC);

	hal_rtc_xfatal("!!! 32K WAS STOPPED !!!\n"
		       "RTC_BBPU      = 0x%x\n"
		       "RTC_IRQ_STA   = 0x%x\n"
		       "RTC_IRQ_EN    = 0x%x\n"
		       "RTC_OSC32CON  = 0x%x\n"
		       "RTC_POWERKEY1 = 0x%x\n"
		       "RTC_POWERKEY2 = 0x%x\n"
		       "RTC_PROT      = 0x%x\n"
		       "RTC_CON       = 0x%x\n"
		       "RTC_TC_SEC    = %02d\n"
		       "RTC_TC_SEC    = %02d\n",
		       bbpu, irqsta, irqen, osc32, pwrkey1, pwrkey2, prot, con, sec1, sec2);
}
#endif

bool hal_rtc_is_lp_irq(void)
{
	u16 irqsta;

	irqsta = rtc_read(RTC_IRQ_STA);	/* read clear */
	if (unlikely(!(irqsta & RTC_IRQ_STA_AL))) {
#ifndef USER_BUILD_KERNEL
		if (irqsta & RTC_IRQ_STA_LP)
			rtc_lp_exception();
#endif
		return true;
	}

	return false;
}

void hal_rtc_reload_power(void)
{
	/* set AUTO bit because AUTO = 0 when PWREN = 1 and alarm occurs */
	u16 bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_AUTO;

	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
}

static void rtc_get_tick(struct rtc_time *tm)
{
	tm->tm_sec = rtc_read(RTC_TC_SEC);
	tm->tm_min = rtc_read(RTC_TC_MIN);
	tm->tm_hour = rtc_read(RTC_TC_HOU);
	tm->tm_mday = rtc_read(RTC_TC_DOM);
	tm->tm_mon = rtc_read(RTC_TC_MTH);
	tm->tm_year = rtc_read(RTC_TC_YEA);
}

void hal_rtc_get_tick_time(struct rtc_time *tm)
{
	u16 bbpu;

	bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
	rtc_get_tick(tm);
	if (rtc_read(RTC_TC_SEC) < tm->tm_sec) {	/* SEC has carried */
		rtc_get_tick(tm);
	}
}

void hal_rtc_set_tick_time(struct rtc_time *tm)
{
	rtc_write(RTC_TC_YEA, tm->tm_year);
	rtc_write(RTC_TC_MTH, tm->tm_mon);
	rtc_write(RTC_TC_DOM, tm->tm_mday);
	rtc_write(RTC_TC_HOU, tm->tm_hour);
	rtc_write(RTC_TC_MIN, tm->tm_min);
	rtc_write(RTC_TC_SEC, tm->tm_sec);
	rtc_write_trigger();
}

bool hal_rtc_check_pwron_alarm_rg(struct rtc_time *nowtm, struct rtc_time *tm)
{
	u16 pdn1, pdn2, spar0, spar1;

	pdn1 = rtc_read(RTC_PDN1);
	pdn2 = rtc_read(RTC_PDN2);
	spar0 = rtc_read(RTC_SPAR0);
	spar1 = rtc_read(RTC_SPAR1);
	hal_rtc_xinfo("pdn1 = 0x%4x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {	/* power-on time is available */

		hal_rtc_xinfo("pdn1 = 0x%4x\n", pdn1);
		hal_rtc_get_tick_time(nowtm);
		hal_rtc_xinfo("pdn1 = 0x%4x\n", pdn1);
		if (rtc_read(RTC_TC_SEC) < nowtm->tm_sec) {	/* SEC has carried */
			hal_rtc_get_tick_time(nowtm);
		}

		tm->tm_sec = ((spar0 & RTC_SPAR0_PWRON_SEC_MASK) >> RTC_SPAR0_PWRON_SEC_SHIFT);
		tm->tm_min = ((spar1 & RTC_SPAR1_PWRON_MIN_MASK) >> RTC_SPAR1_PWRON_MIN_SHIFT);
		tm->tm_hour = ((spar1 & RTC_SPAR1_PWRON_HOU_MASK) >> RTC_SPAR1_PWRON_HOU_SHIFT);
		tm->tm_mday = ((spar1 & RTC_SPAR1_PWRON_DOM_MASK) >> RTC_SPAR1_PWRON_DOM_SHIFT);
		tm->tm_mon = ((pdn2 & RTC_PDN2_PWRON_MTH_MASK) >> RTC_PDN2_PWRON_MTH_SHIFT);
		tm->tm_year = ((pdn2 & RTC_PDN2_PWRON_YEA_MASK) >> RTC_PDN2_PWRON_YEA_SHIFT);

		return true;
	}

	return false;
}

void hal_rtc_get_alarm_time(struct rtc_time *tm, struct rtc_wkalrm *alm)
{
	u16 irqen, pdn2;

	irqen = rtc_read(RTC_IRQ_EN);
	tm->tm_sec = rtc_read(RTC_AL_SEC) & RTC_AL_SEC_MASK;
	tm->tm_min = rtc_read(RTC_AL_MIN);
	tm->tm_hour = rtc_read(RTC_AL_HOU) & RTC_AL_HOU_MASK;
	tm->tm_mday = rtc_read(RTC_AL_DOM) & RTC_AL_DOM_MASK;
	tm->tm_mon = rtc_read(RTC_AL_MTH) & RTC_AL_MTH_MASK;
	tm->tm_year = rtc_read(RTC_AL_YEA) & RTC_AL_YEA_MASK;
	pdn2 = rtc_read(RTC_PDN2);
	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);	/* return Power-On Alarm bit */
}

void hal_rtc_set_alarm_time(struct rtc_time *tm)
{
	u16 irqen;
	
	hal_rtc_xinfo("read tc time = %04d/%02d/%02d (%d) %02d:%02d:%02d\n",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_wday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	hal_rtc_xinfo("a = %d\n",(rtc_read(RTC_AL_MTH)& (RTC_NEW_SPARE3))|tm->tm_mon);
	hal_rtc_xinfo("b = %d\n",(rtc_read(RTC_AL_DOM)& (RTC_NEW_SPARE1))|tm->tm_mday);
	hal_rtc_xinfo("c = %d\n",(rtc_read(RTC_AL_HOU)& (RTC_NEW_SPARE_FG_MASK))|tm->tm_hour);
	rtc_write(RTC_AL_YEA, (rtc_read(RTC_AL_YEA) & ~(RTC_AL_YEA_MASK)) | (tm->tm_year & RTC_AL_YEA_MASK));
	rtc_write(RTC_AL_MTH, (rtc_read(RTC_AL_MTH) & (RTC_NEW_SPARE3))|tm->tm_mon);
	rtc_write(RTC_AL_DOM, (rtc_read(RTC_AL_DOM) & (RTC_NEW_SPARE1))|tm->tm_mday);
	rtc_write(RTC_AL_HOU, (rtc_read(RTC_AL_HOU) & (RTC_NEW_SPARE_FG_MASK))|tm->tm_hour);
	rtc_write(RTC_AL_MIN, tm->tm_min);
	rtc_write(RTC_AL_SEC, rtc_read(RTC_AL_SEC) & (~RTC_AL_SEC_MASK) | (tm->tm_sec & RTC_AL_SEC_MASK));
	rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);		/* mask DOW */
	rtc_write_trigger();
	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_ONESHOT_AL;
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();
}

void hal_rtc_clear_alarm(struct rtc_time *tm)
{
	u16 irqsta, irqen, pdn2;

	irqen = rtc_read(RTC_IRQ_EN) & ~RTC_IRQ_EN_AL;
	pdn2 = rtc_read(RTC_PDN2) & ~RTC_PDN2_PWRON_ALARM;
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write(RTC_PDN2, pdn2);
	rtc_write_trigger();
	irqsta = rtc_read(RTC_IRQ_STA);		/* read clear */

	rtc_write(RTC_AL_YEA, (rtc_read(RTC_AL_YEA) & ~(RTC_AL_YEA_MASK)) | (tm->tm_year & RTC_AL_YEA_MASK));
	rtc_write(RTC_AL_MTH, (rtc_read(RTC_AL_MTH)&0xff00)|tm->tm_mon);
	rtc_write(RTC_AL_DOM, (rtc_read(RTC_AL_DOM)&0xff00)|tm->tm_mday);
	rtc_write(RTC_AL_HOU, (rtc_read(RTC_AL_HOU)&0xff00)|tm->tm_hour);
	rtc_write(RTC_AL_MIN, tm->tm_min);
	rtc_write(RTC_AL_SEC, rtc_read(RTC_AL_SEC) & (~RTC_AL_SEC_MASK) | (tm->tm_sec & RTC_AL_SEC_MASK));
}

void hal_rtc_set_lp_irq(void)
{
	u16 irqen;

	rtc_writeif_unlock();
#ifndef USER_BUILD_KERNEL
	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_LP;
#else
	irqen = rtc_read(RTC_IRQ_EN) & ~RTC_IRQ_EN_LP;
#endif
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();
}

void hal_rtc_read_rg(void)
{
	u16 irqen, pdn1;

	irqen = rtc_read(RTC_IRQ_EN);
	pdn1 = rtc_read(RTC_PDN1);

	hal_rtc_xinfo("RTC_IRQ_EN = 0x%x, RTC_PDN1 = 0x%x\n", irqen, pdn1);
}

void hal_rtc_save_pwron_time(bool enable, struct rtc_time *tm, bool logo)
{
	u16 pdn1, pdn2, spar0, spar1;

	pdn2 =
	    rtc_read(RTC_PDN2) & ~(RTC_PDN2_PWRON_MTH_MASK | RTC_PDN2_PWRON_YEA_MASK |
				   RTC_PDN2_PWRON_LOGO);
	pdn2 |=
	    (tm->tm_year << RTC_PDN2_PWRON_YEA_SHIFT) | (tm->tm_mon << RTC_PDN2_PWRON_MTH_SHIFT);
	if (logo)
		pdn2 |= RTC_PDN2_PWRON_LOGO;

	spar1 =
	    (tm->tm_mday << RTC_SPAR1_PWRON_DOM_SHIFT) | (tm->
							  tm_hour << RTC_SPAR1_PWRON_HOU_SHIFT) |
	    (tm->tm_min << RTC_SPAR1_PWRON_MIN_SHIFT);
	spar0 = rtc_read(RTC_SPAR0) & ~RTC_SPAR0_PWRON_SEC_MASK;
	spar0 |= tm->tm_sec << RTC_SPAR0_PWRON_SEC_SHIFT;

	rtc_write(RTC_PDN2, pdn2);
	rtc_write(RTC_SPAR1, spar1);
	rtc_write(RTC_SPAR0, spar0);
	if (enable) {
		pdn1 = rtc_read(RTC_PDN1) | RTC_PDN1_PWRON_TIME;
		rtc_write(RTC_PDN1, pdn1);
	} else {
		pdn1 = rtc_read(RTC_PDN1) & ~RTC_PDN1_PWRON_TIME;
		rtc_write(RTC_PDN1, pdn1);
	}
	rtc_write_trigger();
}

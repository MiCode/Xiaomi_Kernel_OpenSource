/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (C) 2021 MediaTek Inc.
*/

#if defined(CONFIG_MTK_RTC)

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
#include <linux/types.h>
#include <linux/sched/clock.h>

#include <mtk_rtc_hal.h>
#include <mtk_rtc_hw.h>
#include <mtk_rtc_hal_common.h>
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
#include <mach/mtk_pmic_wrap.h>
#endif
#define hal_rtc_xinfo(fmt, args...)		\
		pr_notice(fmt, ##args)

u16 rtc_read(u16 addr)
{
	u32 rdata = 0;
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
	pwrap_read((u32) addr, &rdata);
#endif
	return (u16) rdata;

}

void rtc_write(u16 addr, u16 data)
{
#ifdef CONFIG_MTK_PMIC_WRAP_HAL
	pwrap_write((u32) addr, (u32) data);
#endif
}

void rtc_busy_wait(void)
{
	unsigned long long timeout = sched_clock() + 500000000;

	do {
		if ((rtc_read(RTC_BBPU) & RTC_BBPU_CBUSY) == 0)
			break;
		else if (sched_clock() > timeout) {
			pr_err("%s, wait cbusy timeout, %x, %x, %x, %d\n",
				__func__,
				rtc_read(RTC_BBPU), rtc_read(RTC_POWERKEY1),
				rtc_read(RTC_POWERKEY2), rtc_read(RTC_TC_SEC));
			break;
		}
	} while (1);
}

void rtc_write_trigger(void)
{
	rtc_write(RTC_WRTGR, 1);
	rtc_busy_wait();
}

void rtc_writeif_unlock(void)
{
	rtc_write(RTC_PROT, RTC_PROT_UNLOCK1);
	rtc_write_trigger();
	rtc_write(RTC_PROT, RTC_PROT_UNLOCK2);
	rtc_write_trigger();
}

void rtc_xosc_write(u16 val, bool reload)
{
	rtc_write(RTC_OSC32CON, RTC_OSC32CON_UNLOCK1);
	rtc_busy_wait();
	rtc_write(RTC_OSC32CON, RTC_OSC32CON_UNLOCK2);
	rtc_busy_wait();

	rtc_write(RTC_OSC32CON, val);
	rtc_busy_wait();

	if (reload) {
		u16 bbpu;

		bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
		rtc_write(RTC_BBPU, bbpu);
		rtc_write_trigger();
	}
}

void rtc_set_writeif(bool enable)
{
	if (enable) {
		rtc_writeif_unlock();
	} else {
		rtc_write(RTC_PROT, 0);
		rtc_write_trigger();
	}
}

void hal_rtc_set_spare_register(enum rtc_spare_enum cmd, u16 val)
{
	u16 tmp_val;

	if (cmd >= 0 && cmd < RTC_SPAR_NUM) {
		hal_rtc_xinfo("%s: cmd[%d], set rg[0x%x, 0x%x , %d] = 0x%x\n",
				__func__, cmd,
				rtc_spare_reg[cmd][RTC_REG],
				rtc_spare_reg[cmd][RTC_MASK],
				rtc_spare_reg[cmd][RTC_SHIFT], val);

		tmp_val =
		    rtc_read(rtc_spare_reg[cmd][RTC_REG]) &
		    ~(rtc_spare_reg[cmd][RTC_MASK] <<
		      rtc_spare_reg[cmd][RTC_SHIFT]);

		rtc_write(rtc_spare_reg[cmd][RTC_REG],
			  tmp_val | ((val & rtc_spare_reg[cmd][RTC_MASK]) <<
				     rtc_spare_reg[cmd][RTC_SHIFT]));
		rtc_write_trigger();
	}
}

u16 hal_rtc_get_spare_register(enum rtc_spare_enum cmd)
{
	u16 tmp_val;

	if (cmd >= 0 && cmd < RTC_SPAR_NUM) {

		tmp_val = rtc_read(rtc_spare_reg[cmd][RTC_REG]);
		tmp_val =
		    (tmp_val >> rtc_spare_reg[cmd][RTC_SHIFT]) &
		    rtc_spare_reg[cmd][RTC_MASK];

		hal_rtc_xinfo("%s: cmd[%d], get rg[0x%x, 0x%x , %d] = 0x%x\n",
				__func__, cmd,
				rtc_spare_reg[cmd][RTC_REG],
				rtc_spare_reg[cmd][RTC_MASK],
				rtc_spare_reg[cmd][RTC_SHIFT], tmp_val);

		return tmp_val;
	}
	return 0;
}

static void rtc_get_tick(struct rtc_time *tm)
{
	tm->tm_sec = rtc_read(RTC_TC_SEC);
	tm->tm_min = rtc_read(RTC_TC_MIN);
	tm->tm_hour = rtc_read(RTC_TC_HOU);
	tm->tm_mday = rtc_read(RTC_TC_DOM);
	tm->tm_mon = rtc_read(RTC_TC_MTH) & RTC_TC_MTH_MASK;
	tm->tm_year = rtc_read(RTC_TC_YEA);
}

void hal_rtc_get_tick_time(struct rtc_time *tm)
{
	u16 bbpu;

	bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
	rtc_get_tick(tm);
	bbpu = rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
	rtc_write(RTC_BBPU, bbpu);
	rtc_write_trigger();
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

void hal_rtc_get_alarm_time(struct rtc_time *tm)
{
	tm->tm_sec = rtc_read(RTC_AL_SEC) & RTC_AL_SEC_MASK;
	tm->tm_min = rtc_read(RTC_AL_MIN) & RTC_AL_MIN_MASK;
	tm->tm_hour = rtc_read(RTC_AL_HOU) & RTC_AL_HOU_MASK;
	tm->tm_mday = rtc_read(RTC_AL_DOM) & RTC_AL_DOM_MASK;
	tm->tm_mon = rtc_read(RTC_AL_MTH) & RTC_AL_MTH_MASK;
	tm->tm_year = rtc_read(RTC_AL_YEA) & RTC_AL_YEA_MASK;
}

void hal_rtc_set_alarm_time(struct rtc_time *tm)
{
	hal_rtc_xinfo("mon = %d, day = %d, hour = %d\n",
		(rtc_read(RTC_AL_MTH) & ~(RTC_AL_MTH_MASK)) | tm->tm_mon,
		(rtc_read(RTC_AL_DOM) & ~(RTC_AL_DOM_MASK)) | tm->tm_mday,
		(rtc_read(RTC_AL_HOU) & ~(RTC_AL_HOU_MASK)) | tm->tm_hour);

	rtc_write(RTC_AL_YEA,
		  (rtc_read(RTC_AL_YEA) & ~(RTC_AL_YEA_MASK)) |
		  (tm->tm_year & RTC_AL_YEA_MASK));
	rtc_write(RTC_AL_MTH,
		  (rtc_read(RTC_AL_MTH) & ~(RTC_AL_MTH_MASK)) |
		  (tm->tm_mon & RTC_AL_MTH_MASK));
	rtc_write(RTC_AL_DOM,
		  (rtc_read(RTC_AL_DOM) & ~(RTC_AL_DOM_MASK)) |
		  (tm->tm_mday & RTC_AL_DOM_MASK));
	rtc_write(RTC_AL_HOU,
		  (rtc_read(RTC_AL_HOU) & ~(RTC_AL_HOU_MASK)) |
		  (tm->tm_hour & RTC_AL_HOU_MASK));
	rtc_write(RTC_AL_MIN,
		  (rtc_read(RTC_AL_MIN) & ~(RTC_AL_MIN_MASK)) |
		  (tm->tm_min & RTC_AL_MIN_MASK));
	rtc_write(RTC_AL_SEC,
		  (rtc_read(RTC_AL_SEC) & ~(RTC_AL_SEC_MASK)) |
		  (tm->tm_sec & RTC_AL_SEC_MASK));
	rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);	/* mask DOW */
	rtc_write_trigger();
}

void hal_rtc_save_pwron_alarm(void)
{
	rtc_write(RTC_PDN1, rtc_read(RTC_PDN1) & (~RTC_PDN1_PWRON_TIME));
	rtc_write(RTC_PDN2, rtc_read(RTC_PDN2) | RTC_PDN2_PWRON_ALARM);
	rtc_write_trigger();
}

void hal_rtc_get_pwron_alarm_time(struct rtc_time *tm)
{
	tm->tm_sec =
	    (rtc_read(RTC_PWRON_SEC) & RTC_PWRON_SEC_MASK) >>
	    RTC_PWRON_SEC_SHIFT;
	tm->tm_min =
	    (rtc_read(RTC_PWRON_MIN) & RTC_PWRON_MIN_MASK) >>
	    RTC_PWRON_MIN_SHIFT;
	tm->tm_hour =
	    (rtc_read(RTC_PWRON_HOU) & RTC_PWRON_HOU_MASK) >>
	    RTC_PWRON_HOU_SHIFT;
	tm->tm_mday =
	    (rtc_read(RTC_PWRON_DOM) & RTC_PWRON_DOM_MASK) >>
	    RTC_PWRON_DOM_SHIFT;
	tm->tm_mon =
	    (rtc_read(RTC_PWRON_MTH) & RTC_PWRON_MTH_MASK) >>
	    RTC_PWRON_MTH_SHIFT;
	tm->tm_year =
	    (rtc_read(RTC_PWRON_YEA) & RTC_PWRON_YEA_MASK) >>
	    RTC_PWRON_YEA_SHIFT;
}

void hal_rtc_set_pwron_alarm_time(struct rtc_time *tm)
{
	rtc_write(RTC_PWRON_YEA,
		  (rtc_read(RTC_PWRON_YEA) & ~(RTC_PWRON_YEA_MASK))
		  | ((tm->tm_year << RTC_PWRON_YEA_SHIFT) &
		     RTC_PWRON_YEA_MASK));
	rtc_write_trigger();
	rtc_write(RTC_PWRON_MTH,
		  (rtc_read(RTC_PWRON_MTH) & ~(RTC_PWRON_MTH_MASK))
		  | ((tm->tm_mon << RTC_PWRON_MTH_SHIFT) & RTC_PWRON_MTH_MASK));
	rtc_write_trigger();
	rtc_write(RTC_PWRON_DOM,
		  (rtc_read(RTC_PWRON_DOM) & ~(RTC_PWRON_DOM_MASK))
		  | ((tm->tm_mday << RTC_PWRON_DOM_SHIFT) &
		     RTC_PWRON_DOM_MASK));
	rtc_write_trigger();
	rtc_write(RTC_PWRON_HOU,
		  (rtc_read(RTC_PWRON_HOU) & ~(RTC_PWRON_HOU_MASK))
		  | ((tm->tm_hour << RTC_PWRON_HOU_SHIFT) &
		     RTC_PWRON_HOU_MASK));
	rtc_write_trigger();
	rtc_write(RTC_PWRON_MIN,
		  (rtc_read(RTC_PWRON_MIN) & ~(RTC_PWRON_MIN_MASK))
		  | ((tm->tm_min << RTC_PWRON_MIN_SHIFT) & RTC_PWRON_MIN_MASK));
	rtc_write_trigger();
	rtc_write(RTC_PWRON_SEC,
		  (rtc_read(RTC_PWRON_SEC) & ~(RTC_PWRON_SEC_MASK))
		  | ((tm->tm_sec << RTC_PWRON_SEC_SHIFT) & RTC_PWRON_SEC_MASK));
	rtc_write_trigger();
}

void hal_rtc_read_rg(void)
{
	u16 irqen, pdn1;

	irqen = rtc_read(RTC_IRQ_EN);
	pdn1 = rtc_read(RTC_PDN1);

	hal_rtc_xinfo("RTC_IRQ_EN = 0x%x, RTC_PDN1 = 0x%x\n", irqen, pdn1);
}

#ifndef USER_BUILD_KERNEL
void rtc_lp_exception(void)
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

	pr_emerg("!!! 32K WAS STOPPED !!!\n"
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
		bbpu, irqsta, irqen, osc32, pwrkey1, pwrkey2, prot, con, sec1,
		sec2);
}
#endif

#endif				/*#if defined(CONFIG_MTK_RTC) */

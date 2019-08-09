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
#include <linux/types.h>

#include <mtk_rtc_hal_common.h>
#include "mtk_rtc_hw.h"
#include <mach/mtk_pmic_wrap.h>
#include <mtk_boot.h>

#include "include/pmic.h"

/*TODO extern bool pmic_chrdet_status(void);*/

/*
 *	RTC_FGSOC = 0,
 *	RTC_ANDROID,
 *	RTC_RECOVERY,
 *	RTC_FAC_RESET,
 *	RTC_BYPASS_PWR,
 *	RTC_PWRON_TIME,
 *	RTC_FAST_BOOT,
 *	RTC_KPOC,
 *	RTC_DEBUG,
 *	RTC_PWRON_AL,
 *	RTC_UART,
 *	RTC_AUTOBOOT,
 *	RTC_PWRON_LOGO,
 *	RTC_32K_LESS,
 *	RTC_LP_DET,
 *	RTC_SPAR_NUM
 *
 */
/*
 * RTC_PDN1:
 *     bit 0 - 3  : Android bits
 *     bit 4 - 5  : Recovery bits (0x10: factory data reset)
 *     bit 6      : Bypass PWRKEY bit
 *     bit 7      : Power-On Time bit
 *     bit 8      : RTC_GPIO_USER_WIFI bit
 *     bit 9      : RTC_GPIO_USER_GPS bit
 *     bit 10     : RTC_GPIO_USER_BT bit
 *     bit 11     : RTC_GPIO_USER_FM bit
 *     bit 12     : RTC_GPIO_USER_PMIC bit
 *     bit 13     : Fast Boot
 *     bit 14	  : Kernel Power Off Charging
 *     bit 15     : Debug bit
 */
/*
 * RTC_PDN2:
 *     bit 0 - 3 : MTH in power-on time
 *     bit 4	 : Power-On Alarm bit
 *     bit 5 - 6 : UART bits
 *     bit 7	 : autoboot bit
 *     bit 8 - 14: YEA in power-on time
 *     bit 15	 : Power-On Logo bit
 */
/*
 * RTC_SPAR0:
 *     bit 0 - 5 : SEC in power-on time
 *     bit 6	 : 32K less bit. True:with 32K, False:Without 32K
 *     bit 7 - 15: reserved bits
 */

u16 rtc_spare_reg[RTC_SPAR_NUM][3] = {
	{RTC_AL_MTH, 0xff, 8}
	,
	{RTC_PDN1, 0xf, 0}
	,
	{RTC_PDN1, 0x3, 4}
	,
	{RTC_PDN1, 0x1, 6}
	,
	{RTC_PDN1, 0x1, 7}
	,
	{RTC_PDN1, 0x1, 13}
	,
	{RTC_PDN1, 0x1, 14}
	,
	{RTC_PDN1, 0x1, 15}
	,
	{RTC_PDN2, 0x1, 4}
	,
	{RTC_PDN2, 0x3, 5}
	,
	{RTC_PDN2, 0x1, 7}
	,
	{RTC_PDN2, 0x1, 15}
	,
	{RTC_SPAR0, 0x1, 6}
	,
	{RTC_SPAR0, 0x1, 7}
	,
	{RTC_AL_HOU, 0xff, 8}
};

static int rtc_eosc_cali_td = 8;
module_param(rtc_eosc_cali_td, int, 0664);

void hal_rtc_set_abb_32k(u16 enable)
{
	pr_notice("ABB 32k not support\n");
}

u16 hal_rtc_get_gpio_32k_status(void)
{
	u16 con;

	con = rtc_read(RTC_CON);

	pr_notice("RTC_GPIO 32k status(RTC_CON=0x%x)\n", con);

	if (con & RTC_CON_F32KOB)
		return 0;
	else
		return 1;
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
	pr_notice("RTC_GPIO user %d enable = %d 32k (0x%x), RTC_CON = %x\n",
		      user, enable, pdn1, rtc_read(RTC_CON));
}

void rtc_spar_alarm_clear_wait(void)
{
	unsigned long long timeout = sched_clock() + 500000000;

	do {
		if ((rtc_read(RTC_BBPU) & RTC_BBPU_CLR) == 0)
			break;
		else if (sched_clock() > timeout) {
			pr_notice("%s, spar/alarm clear time out, %x,\n",
				__func__, rtc_read(RTC_BBPU));
			break;
		}
	} while (1);
}

void rtc_enable_k_eosc(void)
{
	u16 osc32;
	/* Truning on eosc cali mode clock */
	pmic_config_interface_nolock(PMIC_SCK_TOP_CKPDN_CON0_CLR_ADDR, 1,
				     PMIC_RG_RTC_EOSC32_CK_PDN_MASK,
				     PMIC_RG_RTC_EOSC32_CK_PDN_SHIFT);
	pmic_config_interface_nolock(PMIC_RG_SRCLKEN_IN0_HW_MODE_ADDR, 1,
				     PMIC_RG_SRCLKEN_IN0_HW_MODE_MASK,
				     PMIC_RG_SRCLKEN_IN0_HW_MODE_SHIFT);
	pmic_config_interface_nolock(PMIC_RG_SRCLKEN_IN1_HW_MODE_ADDR, 1,
				     PMIC_RG_SRCLKEN_IN1_HW_MODE_MASK,
				     PMIC_RG_SRCLKEN_IN1_HW_MODE_SHIFT);
	pmic_config_interface_nolock(PMIC_RG_RTC_EOSC32_CK_PDN_ADDR, 0,
				     PMIC_RG_RTC_EOSC32_CK_PDN_MASK,
				     PMIC_RG_RTC_EOSC32_CK_PDN_SHIFT);

	switch (rtc_eosc_cali_td) {
	case 1:
		pmic_config_interface_nolock(PMIC_EOSC_CALI_TD_ADDR, 0x3,
					     PMIC_EOSC_CALI_TD_MASK,
					     PMIC_EOSC_CALI_TD_SHIFT);
		break;
	case 2:
		pmic_config_interface_nolock(PMIC_EOSC_CALI_TD_ADDR, 0x4,
					     PMIC_EOSC_CALI_TD_MASK,
					     PMIC_EOSC_CALI_TD_SHIFT);
		break;
	case 4:
		pmic_config_interface_nolock(PMIC_EOSC_CALI_TD_ADDR, 0x5,
					     PMIC_EOSC_CALI_TD_MASK,
					     PMIC_EOSC_CALI_TD_SHIFT);
		break;
	case 16:
		pmic_config_interface_nolock(PMIC_EOSC_CALI_TD_ADDR, 0x7,
					     PMIC_EOSC_CALI_TD_MASK,
					     PMIC_EOSC_CALI_TD_SHIFT);
		break;
	default:
		pmic_config_interface_nolock(PMIC_EOSC_CALI_TD_ADDR, 0x6,
					     PMIC_EOSC_CALI_TD_MASK,
					     PMIC_EOSC_CALI_TD_SHIFT);
		break;
	}
	/*Switch the DCXO from 32k-less mode to RTC mode,
	 *otherwise, EOSC cali will fail
	 */
	/*RTC mode will have only OFF mode and FPM */
	pmic_config_interface_nolock(PMIC_XO_EN32K_MAN_ADDR, 0,
				     PMIC_XO_EN32K_MAN_MASK,
				     PMIC_XO_EN32K_MAN_SHIFT);
	rtc_write(RTC_BBPU,
		  rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	rtc_write_trigger();
	/* Enable K EOSC mode for normal power off and then plug out battery */
	rtc_write(RTC_AL_YEA,
		  ((rtc_read(RTC_AL_YEA) | RTC_K_EOSC_RSV_0) &
		   (~RTC_K_EOSC_RSV_1)) | RTC_K_EOSC_RSV_2);
	rtc_write_trigger();

	osc32 = rtc_read(RTC_OSC32CON);
	rtc_xosc_write(osc32 | RTC_EMBCK_SRC_SEL, true);
	pr_notice("RTC_enable_k_eosc\n");
}

void rtc_disable_2sec_reboot(void)
{
	u16 reboot;

	reboot =
	    (rtc_read(RTC_AL_SEC) & ~RTC_BBPU_2SEC_EN) & ~RTC_BBPU_AUTO_PDN_SEL;
	rtc_write(RTC_AL_SEC, reboot);
	rtc_write_trigger();
}

void rtc_bbpu_pwrdown(bool auto_boot)
{
	PMIC_POWER_HOLD(0);
}

void hal_rtc_bbpu_pwdn(bool charger_status)
{
	u16 con, bbpu;

	rtc_disable_2sec_reboot();
	rtc_enable_k_eosc();

	/* disable 32K export if there are no RTC_GPIO users */
	if (!(rtc_read(RTC_PDN1) & RTC_GPIO_USER_MASK)) {
		con = rtc_read(RTC_CON) | RTC_CON_F32KOB;
		rtc_write(RTC_CON, con);
		rtc_write_trigger();
	}
	/* lpsd */
	pr_notice("clear lpsd solution\n");
	bbpu = RTC_BBPU_KEY | RTC_BBPU_CLR | RTC_BBPU_PWREN;
	rtc_write(RTC_BBPU, bbpu);

	rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);	/* mask DOW */
	rtc_write_trigger();

	rtc_spar_alarm_clear_wait();

	wk_pmic_enable_sdn_delay();

	rtc_write(RTC_BBPU,
			rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	rtc_write_trigger();
	pr_notice("RTC_AL_MASK= 0x%x RTC_IRQ_EN= 0x%x\n",
			rtc_read(RTC_AL_MASK), rtc_read(RTC_IRQ_EN));
	/* lpsd */
	rtc_bbpu_pwrdown(true);
}

void hal_rtc_get_pwron_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm)
{
	u16 pdn1, pdn2;


	pdn1 = rtc_read(RTC_PDN1);
	pdn2 = rtc_read(RTC_PDN2);

	alm->enabled =
	    (pdn1 & RTC_PDN1_PWRON_TIME ? (pdn2 & RTC_PDN2_PWRON_LOGO ? 3 : 2) :
	     0);
	/* return Power-On Alarm bit */
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);

	hal_rtc_get_alarm_time(tm);
}

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

bool hal_rtc_is_pwron_alarm(struct rtc_time *nowtm, struct rtc_time *tm)
{
	u16 pdn1;

	pdn1 = rtc_read(RTC_PDN1);
	pr_notice("pdn1 = 0x%4x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {	/* power-on time is available */

		pr_notice("pdn1 = 0x%4x\n", pdn1);
		hal_rtc_get_tick_time(nowtm);
		pr_notice("pdn1 = 0x%4x\n", pdn1);
		/* SEC has carried */
		if (rtc_read(RTC_TC_SEC) < nowtm->tm_sec)
			hal_rtc_get_tick_time(nowtm);

		hal_rtc_get_pwron_alarm_time(tm);

		return true;
	}

	return false;
}

void hal_rtc_get_alarm(struct rtc_time *tm, struct rtc_wkalrm *alm)
{
	u16 irqen, pdn2;

	irqen = rtc_read(RTC_IRQ_EN);
	hal_rtc_get_alarm_time(tm);
	pdn2 = rtc_read(RTC_PDN2);
	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	/* return Power-On Alarm bit */
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);
}

void hal_rtc_set_alarm(struct rtc_time *tm)
{
	u16 irqen;

	hal_rtc_set_alarm_time(tm);

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
	irqsta = rtc_read(RTC_IRQ_STA);	/* read clear */

	hal_rtc_set_alarm_time(tm);
}

void hal_rtc_set_lp_irq(void)
{
	u16 irqen;

#ifndef USER_BUILD_KERNEL
	irqen = rtc_read(RTC_IRQ_EN) | RTC_IRQ_EN_LP;
#else
	irqen = rtc_read(RTC_IRQ_EN) & ~RTC_IRQ_EN_LP;
#endif
	rtc_write(RTC_IRQ_EN, irqen);
	rtc_write_trigger();
}

void hal_rtc_save_pwron_time(bool enable, struct rtc_time *tm, bool logo)
{
	u16 pdn1, pdn2;

	hal_rtc_set_pwron_alarm_time(tm);

	if (logo)
		pdn2 = rtc_read(RTC_PDN2) | RTC_PDN2_PWRON_LOGO;
	else
		pdn2 = rtc_read(RTC_PDN2) & ~RTC_PDN2_PWRON_LOGO;

	rtc_write(RTC_PDN2, pdn2);

	if (enable)
		pdn1 = rtc_read(RTC_PDN1) | RTC_PDN1_PWRON_TIME;
	else
		pdn1 = rtc_read(RTC_PDN1) & ~RTC_PDN1_PWRON_TIME;
	rtc_write(RTC_PDN1, pdn1);
	rtc_write_trigger();
}

void rtc_clock_enable(int enable)
{
	if (enable) {
		pmic_config_interface_nolock(PMIC_SCK_TOP_CKPDN_CON0_CLR_ADDR,
					     1, PMIC_RG_RTC_MCLK_PDN_MASK,
					     PMIC_RG_RTC_MCLK_PDN_SHIFT);
		pmic_config_interface_nolock(PMIC_SCK_TOP_CKPDN_CON0_CLR_ADDR,
					     1, PMIC_RG_RTC_32K_CK_PDN_MASK,
					     PMIC_RG_RTC_32K_CK_PDN_SHIFT);
	} else {
		pmic_config_interface_nolock(PMIC_SCK_TOP_CKPDN_CON0_SET_ADDR,
					     1, PMIC_RG_RTC_MCLK_PDN_MASK,
					     PMIC_RG_RTC_MCLK_PDN_SHIFT);
		pmic_config_interface_nolock(PMIC_SCK_TOP_CKPDN_CON0_SET_ADDR,
					     1, PMIC_RG_RTC_32K_CK_PDN_MASK,
					     PMIC_RG_RTC_32K_CK_PDN_SHIFT);
	}
}

void rtc_lpsd_restore_al_mask(void)
{
	pr_notice("rtc_lpsd_restore_al_mask\n");

	rtc_write(RTC_BBPU,
			rtc_read(RTC_BBPU) | RTC_BBPU_KEY | RTC_BBPU_RELOAD);
	rtc_write_trigger();
	pr_notice("1st RTC_AL_MASK = 0x%x\n", rtc_read(RTC_AL_MASK));
	/* mask DOW */
	rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);
	rtc_write_trigger();
	pr_notice("2nd RTC_AL_MASK = 0x%x\n", rtc_read(RTC_AL_MASK));
}

/*
 * Copyright (C) 2018 MediaTek, Inc.
 * Author: Wilma Wu <wilma.wu@mediatek.com>
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

#include <asm/div64.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_wakeup.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <asm/div64.h>
/* For KPOC alarm */
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include "../misc/mediatek/include/mt-plat/mtk_boot_common.h"
#include "../misc/mediatek/include/mt-plat/mtk_reboot.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define RTC_NAME	"mt-rtc"
#define IPIMB


/* we map HW YEA 0 (2000) to 1968 not 1970 because 2000 is the leap year */
#define RTC_MIN_YEAR		1968
#define RTC_BASE_YEAR		1900
#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - RTC_BASE_YEAR)

/* Min, Hour, Dom... register offset to RTC_TC_SEC */
#define RTC_OFFSET_SEC			0
#define RTC_OFFSET_MIN			1
#define RTC_OFFSET_HOUR			2
#define RTC_OFFSET_DOM			3
#define RTC_OFFSET_DOW			4
#define RTC_OFFSET_MTH			5
#define RTC_OFFSET_YEAR			6
#define RTC_OFFSET_COUNT		7

#define RTC_DSN_ID				0x580
#define RTC_BBPU				0x8
#define RTC_IRQ_STA				0xa
#define RTC_IRQ_EN				0xc
#define RTC_AL_MASK				0x10
#define RTC_TC_SEC				0x12
#define RTC_AL_SEC				0x20
#define RTC_AL_MIN				0x22
#define RTC_AL_HOU				0x24
#define RTC_AL_DOM				0x26
#define RTC_AL_DOW				0x28
#define RTC_AL_MTH				0x2a
#define RTC_AL_YEA				0x2c
#define RTC_OSC32CON			0x2e
#define RTC_POWERKEY1			0x30
#define RTC_POWERKEY2			0x32
#define RTC_PDN1				0x34
#define RTC_PDN2				0x36
#define RTC_SPAR0				0x38
#define RTC_SPAR1				0x3a
#define RTC_PROT				0x3c
#define RTC_WRTGR				0x42
#define RTC_CON					0x44

#define RTC_TC_SEC_MASK			0x3f
#define RTC_TC_MIN_MASK			0x3f
#define RTC_TC_HOU_MASK			0x1f
#define RTC_TC_DOM_MASK			0x1f
#define RTC_TC_DOW_MASK			0x7
#define RTC_TC_MTH_MASK			0xf
#define RTC_TC_YEA_MASK			0x7f

#define RTC_AL_SEC_MASK			0x3f
#define RTC_AL_MIN_MASK			0x3f
#define RTC_AL_HOU_MASK			0x1f
#define RTC_AL_DOM_MASK			0x1f
#define RTC_AL_DOW_MASK			0x7
#define RTC_AL_MTH_MASK			0xf
#define RTC_AL_YEA_MASK			0x7f

#define RTC_PWRON_SEC_SHIFT		0x0
#define RTC_PWRON_MIN_SHIFT		0x0
#define RTC_PWRON_HOU_SHIFT		0x6
#define RTC_PWRON_DOM_SHIFT		0xb
#define RTC_PWRON_MTH_SHIFT		0x0
#define RTC_PWRON_YEA_SHIFT		0x8

#define RTC_PWRON_SEC_MASK      (RTC_AL_SEC_MASK << RTC_PWRON_SEC_SHIFT)
#define RTC_PWRON_MIN_MASK      (RTC_AL_MIN_MASK << RTC_PWRON_MIN_SHIFT)
#define RTC_PWRON_HOU_MASK      (RTC_AL_HOU_MASK << RTC_PWRON_HOU_SHIFT)
#define RTC_PWRON_DOM_MASK      (RTC_AL_DOM_MASK << RTC_PWRON_DOM_SHIFT)
#define RTC_PWRON_MTH_MASK      (RTC_AL_MTH_MASK << RTC_PWRON_MTH_SHIFT)
#define RTC_PWRON_YEA_MASK      (RTC_AL_YEA_MASK << RTC_PWRON_YEA_SHIFT)

#define RTC_BBPU_KEY			0x4300
#define RTC_BBPU_CBUSY			BIT(6)
#define RTC_BBPU_RELOAD			BIT(5)
#define RTC_BBPU_AUTO			BIT(3)
#define RTC_BBPU_CLR			BIT(1)
#define RTC_BBPU_PWREN			BIT(0)
#define RTC_BBPU_AL_STA			BIT(7)
#define RTC_BBPU_RESET_AL		BIT(3)
#define RTC_BBPU_RESET_SPAR		BIT(2)

#define RTC_AL_MASK_DOW			BIT(4)

#define RTC_IRQ_EN_LP			BIT(3)
#define RTC_IRQ_EN_ONESHOT		BIT(2)
#define	RTC_IRQ_EN_AL			BIT(0)
#define RTC_IRQ_EN_ONESHOT_AL   (RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_IRQ_STA_LP			BIT(3)
#define RTC_IRQ_STA_AL			BIT(0)

#define RTC_PDN1_PWRON_TIME		BIT(7)
#define RTC_PDN2_PWRON_LOGO		BIT(15)
#define RTC_PDN2_PWRON_ALARM	BIT(4)


static u16 rtc_alarm_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_AL_SEC, RTC_AL_SEC_MASK, 0},
	{RTC_AL_MIN, RTC_AL_MIN_MASK, 0},
	{RTC_AL_HOU, RTC_AL_HOU_MASK, 0},
	{RTC_AL_DOM, RTC_AL_DOM_MASK, 0},
	{RTC_AL_DOW, RTC_AL_DOW_MASK, 0},
	{RTC_AL_MTH, RTC_AL_MTH_MASK, 0},
	{RTC_AL_YEA, RTC_AL_YEA_MASK, 0},
};

static u16 rtc_pwron_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_SPAR0, RTC_PWRON_SEC_MASK, RTC_PWRON_SEC_SHIFT},
	{RTC_SPAR1, RTC_PWRON_MIN_MASK, RTC_PWRON_MIN_SHIFT},
	{RTC_SPAR1, RTC_PWRON_HOU_MASK, RTC_PWRON_HOU_SHIFT},
	{RTC_SPAR1, RTC_PWRON_DOM_MASK, RTC_PWRON_DOM_SHIFT},
	{0, 0, 0},
	{RTC_PDN2, RTC_PWRON_MTH_MASK, RTC_PWRON_MTH_SHIFT},
	{RTC_PDN2, RTC_PWRON_YEA_MASK, RTC_PWRON_YEA_SHIFT},
};

enum rtc_reg_set {
	RTC_REG,
	RTC_MASK,
	RTC_SHIFT
};

enum rtc_irq_sta {
	RTC_NONE,
	RTC_ALSTA,
	RTC_TCSTA,
	RTC_LPSTA,
};

struct mt6358_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	spinlock_t	lock;
	struct regmap		*regmap;
	int			irq;
	u32			addr_base;
	struct work_struct work;
	struct completion comp;
};
static struct mt6358_rtc *mt_rtc;
static struct wakeup_source *mt6358_rtc_suspend_lock;

static int rtc_show_time;
static int rtc_show_alarm = 1;
static int apply_lpsd_solution;
/*for KPOC alarm*/
static bool rtc_pm_notifier_registered;
static bool kpoc_alarm;
static unsigned long rtc_pm_status;
static int alarm1m15s;

module_param(rtc_show_time, int, 0644);
module_param(rtc_show_alarm, int, 0644);



void __attribute__((weak)) arch_reset(char mode, const char *cmd)
{
	pr_info("arch_reset is not ready\n");
}

static int rtc_read(unsigned int reg, unsigned int *val)
{
	return regmap_read(mt_rtc->regmap, mt_rtc->addr_base + reg, val);
}

static int rtc_write(unsigned int reg, unsigned int val)
{
	return regmap_write(mt_rtc->regmap, mt_rtc->addr_base + reg, val);
}

static int rtc_update_bits(unsigned int reg,
		       unsigned int mask, unsigned int val)
{
	return regmap_update_bits(mt_rtc->regmap,
			mt_rtc->addr_base + reg, mask, val);
}

static int rtc_field_read(unsigned int reg,
		       unsigned int mask, unsigned int shift, unsigned int *val)
{
	int ret;
	unsigned int reg_val = 0;

	ret = rtc_read(reg, &reg_val);
	if (ret != 0)
		return ret;

	reg_val &= mask;
	reg_val >>= shift;
	*val = reg_val;

	return ret;
}

#define BULK_WRITE 0
#define BULK_READ 1

static int rtc_bulk_access(int mode, unsigned int reg, void *val,
		     size_t val_count)
{
	if (mode == BULK_WRITE) {
		return regmap_bulk_write(mt_rtc->regmap,
				mt_rtc->addr_base + reg, val, val_count);
	} else if (mode == BULK_READ) {
		return regmap_bulk_read(mt_rtc->regmap,
				mt_rtc->addr_base + reg, val, val_count);
	} else
		return -EPERM;
}

static int rtc_write_trigger(void)
{
	int ret, bbpu = 0;
	unsigned long long timeout = sched_clock() + 500000000;
	u32 pwrkey1 = 0, pwrkey2 = 0, sec = 0;

	ret = rtc_write(RTC_WRTGR, 1);
	if (ret < 0)
		return ret;

	do {
		ret = rtc_read(RTC_BBPU, &bbpu);
		if (ret < 0)
			break;
		if ((bbpu & RTC_BBPU_CBUSY) == 0)
			break;
		else if (sched_clock() > timeout) {
			rtc_read(RTC_BBPU, &bbpu);
			rtc_read(RTC_POWERKEY1, &pwrkey1);
			rtc_read(RTC_POWERKEY2, &pwrkey2);
			rtc_read(RTC_TC_SEC, &sec);
			pr_err("%s, wait cbusy timeout, %x, %x, %x, %d\n",
				__func__, bbpu, pwrkey1, pwrkey2, sec);
			ret = -ETIMEDOUT;
			break;
		}
	} while (1);

	return ret;
}

static int mtk_rtc_read_time(struct rtc_time *tm)
{
	u16 data[RTC_OFFSET_COUNT];
	int ret;
	u32 sec = 0;

	do {

		ret = rtc_bulk_access(BULK_READ, RTC_TC_SEC,
					data, RTC_OFFSET_COUNT);
		if (ret < 0)
			goto exit;
		tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_TC_SEC_MASK;
		tm->tm_min = data[RTC_OFFSET_MIN] & RTC_TC_MIN_MASK;
		tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_TC_HOU_MASK;
		tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_TC_DOM_MASK;
		tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_TC_MTH_MASK;
		tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_TC_YEA_MASK;

		ret = rtc_read(RTC_TC_SEC, &sec);
		if (ret < 0)
			goto exit;

	} while (sec < tm->tm_sec);

	return ret;

exit:
	pr_err("%s error\n", __func__);
	return ret;
}

static int mtk_rtc_set_alarm(struct rtc_time *tm)
{
	int ret, i;
	u16 data[RTC_OFFSET_COUNT];

	data[RTC_OFFSET_SEC] = tm->tm_sec & RTC_AL_SEC_MASK;
	data[RTC_OFFSET_MIN] = tm->tm_min & RTC_AL_MIN_MASK;
	data[RTC_OFFSET_HOUR] = tm->tm_hour & RTC_AL_HOU_MASK;
	data[RTC_OFFSET_DOM] = tm->tm_mday & RTC_AL_DOM_MASK;
	data[RTC_OFFSET_MTH] = tm->tm_mon & RTC_AL_MTH_MASK;
	data[RTC_OFFSET_YEAR] = tm->tm_year & RTC_AL_YEA_MASK;

	for (i = RTC_OFFSET_SEC; i < RTC_OFFSET_COUNT; i++) {
		if (i == RTC_OFFSET_DOW)
			continue;
		ret = rtc_update_bits(rtc_alarm_reg[i][RTC_REG],
					rtc_alarm_reg[i][RTC_MASK], data[i]);
		if (ret < 0)
			goto exit;
	}
	ret = rtc_write(RTC_AL_MASK, RTC_AL_MASK_DOW);	/* mask DOW */
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	ret = rtc_update_bits(RTC_IRQ_EN,
						  RTC_IRQ_EN_ONESHOT_AL,
						  RTC_IRQ_EN_ONESHOT_AL);
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	return ret;
exit:
	pr_err("%s error\n", __func__);
	return ret;
}

bool mtk_rtc_is_pwron_alarm(struct rtc_time *nowtm, struct rtc_time *tm)
{
	u32 pdn1 = 0;
	u32 data[RTC_OFFSET_COUNT] = {0};
	int ret, i;

	ret = rtc_read(RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;
	pr_notice("pdn1 = 0x%4x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {	/* power-on time is available */

		ret = mtk_rtc_read_time(nowtm);
		if (ret < 0)
			goto exit;

		for (i = RTC_OFFSET_SEC; i < RTC_OFFSET_COUNT; i++) {
			if (i == RTC_OFFSET_DOW)
				continue;
			ret = rtc_field_read(rtc_pwron_reg[i][RTC_REG],
					rtc_pwron_reg[i][RTC_MASK],
					rtc_pwron_reg[i][RTC_SHIFT], &data[i]);
			if (ret < 0)
				goto exit;
		}
		tm->tm_sec = data[RTC_OFFSET_SEC];
		tm->tm_min = data[RTC_OFFSET_MIN];
		tm->tm_hour = data[RTC_OFFSET_HOUR];
		tm->tm_mday = data[RTC_OFFSET_DOM];
		tm->tm_mon = data[RTC_OFFSET_MTH];
		tm->tm_year = data[RTC_OFFSET_YEAR];

		return true;
	}
	return false;
exit:
	pr_err("%s error\n", __func__);
	return false;
}

static int mtk_rtc_set_pwron_alarm_time(struct rtc_time *tm)
{
	u16 data[RTC_OFFSET_COUNT];
	int ret, i;

	pr_err("%s\n", __func__);

	data[RTC_OFFSET_SEC] =
		((tm->tm_sec << RTC_PWRON_SEC_SHIFT) & RTC_PWRON_SEC_MASK);
	data[RTC_OFFSET_MIN] =
		((tm->tm_min << RTC_PWRON_MIN_SHIFT) & RTC_PWRON_MIN_MASK);
	data[RTC_OFFSET_HOUR] =
		((tm->tm_hour << RTC_PWRON_HOU_SHIFT) & RTC_PWRON_HOU_MASK);
	data[RTC_OFFSET_DOM] =
		((tm->tm_mday << RTC_PWRON_DOM_SHIFT) & RTC_PWRON_DOM_MASK);
	data[RTC_OFFSET_MTH] =
		((tm->tm_mon << RTC_PWRON_MTH_SHIFT) & RTC_PWRON_MTH_MASK);
	data[RTC_OFFSET_YEAR] =
		((tm->tm_year << RTC_PWRON_YEA_SHIFT) & RTC_PWRON_YEA_MASK);

	for (i = RTC_OFFSET_SEC; i < RTC_OFFSET_COUNT; i++) {
		if (i == RTC_OFFSET_DOW)
			continue;
		ret = rtc_update_bits(rtc_pwron_reg[i][RTC_REG],
					rtc_pwron_reg[i][RTC_MASK], data[i]);
		if (ret < 0)
			goto exit;
		ret = rtc_write_trigger();
		if (ret < 0)
			goto exit;
	}
	return ret;
exit:
	pr_err("%s error\n", __func__);
	return ret;
}

static int mtk_rtc_set_pwron_alarm(bool enable, struct rtc_time *tm, bool logo)
{
	u16 pdn1 = 0, pdn2 = 0;
	int ret;

	ret = mtk_rtc_set_pwron_alarm_time(tm);
	if (ret < 0)
		goto exit;

	if (enable)
		pdn1 = RTC_PDN1_PWRON_TIME;
	ret = rtc_update_bits(RTC_PDN1, RTC_PDN1_PWRON_TIME, pdn1);
	if (ret < 0)
		goto exit;

	if (logo)
		pdn2 = RTC_PDN2_PWRON_LOGO;
	ret = rtc_update_bits(RTC_PDN2, RTC_PDN2_PWRON_LOGO, pdn2);
	if (ret < 0)
		goto exit;

	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	return ret;

exit:
	pr_err("%s error\n", __func__);
	return ret;
}

void rtc_read_pwron_alarm(struct rtc_wkalrm *alm)
{
	struct rtc_time *tm;
	u32 pdn1 = 0, pdn2 = 0;
	u16 data[RTC_OFFSET_COUNT];
	unsigned long flags;
	int ret;

	if (alm == NULL)
		return;

	tm = &alm->time;

	spin_lock_irqsave(&mt_rtc->lock, flags);
	ret = rtc_read(RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;
	ret = rtc_read(RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	alm->enabled = (pdn1 & RTC_PDN1_PWRON_TIME ?
			(pdn2 & RTC_PDN2_PWRON_LOGO ? 3 : 2) : 0);
	/* return Power-On Alarm bit */
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);

	ret = rtc_bulk_access(BULK_READ, RTC_AL_SEC, data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_AL_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_AL_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_AL_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_AL_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_AL_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_AL_YEA_MASK;

	spin_unlock_irqrestore(&mt_rtc->lock, flags);

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon -= 1;

	if (rtc_show_alarm) {
		pr_notice("power-on = %04d/%02d/%02d %02d:%02d:%02d (%d)(%d)\n",
			  tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1,
			  tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
			  alm->enabled, alm->pending);
	}
	return;
exit:
	pr_err("%s error\n", __func__);
}

#ifdef CONFIG_PM

#define PM_DUMMY 0xFFFF

static int rtc_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	pr_notice("%s = %lu\n", __func__, pm_event);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		rtc_pm_status = PM_SUSPEND_PREPARE;
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		rtc_pm_status = PM_POST_SUSPEND;
		break;
	default:
		rtc_pm_status = PM_DUMMY;
		break;
	}

	if (kpoc_alarm) {
		pr_notice("%s trigger reboot\n", __func__);
		complete(&mt_rtc->comp);
		kpoc_alarm = false;
	}
	return NOTIFY_DONE;
}

static struct notifier_block rtc_pm_notifier_func = {
	.notifier_call = rtc_pm_event,
	.priority = 0,
};
#endif /* CONFIG_PM */

static void mtk_rtc_work_queue(struct work_struct *work)
{
	struct mt6358_rtc *rtc = container_of(work, struct mt6358_rtc, work);
	unsigned long ret;
	unsigned int msecs;

	ret = wait_for_completion_timeout(&rtc->comp, msecs_to_jiffies(30000));
	if (!ret) {
		pr_notice("%s timeout\n", __func__);
		BUG_ON(1);
	} else {
		msecs = jiffies_to_msecs(ret);
		pr_notice("%s timeleft= %d\n", __func__, msecs);
		kernel_restart("kpoc");
	}
}

static void mtk_rtc_reboot(void)
{
	__pm_stay_awake(mt6358_rtc_suspend_lock);

	init_completion(&mt_rtc->comp);
	schedule_work_on(cpumask_first(cpu_online_mask), &mt_rtc->work);

	if (!rtc_pm_notifier_registered)
		goto reboot;

	if (rtc_pm_status != PM_SUSPEND_PREPARE)
		goto reboot;

	kpoc_alarm = true;

	pr_notice("%s:wait\n", __func__);
	return;

reboot:
	pr_notice("%s:trigger\n", __func__);
	complete(&mt_rtc->comp);
}

#ifndef USER_BUILD_KERNEL
void mtk_rtc_lp_exception(void)
{
	u32 bbpu = 0, irqsta = 0, irqen = 0, osc32 = 0;
	u32 pwrkey1 = 0, pwrkey2 = 0, prot = 0, con = 0, sec1 = 0, sec2 = 0;

	rtc_read(RTC_BBPU, &bbpu);
	rtc_read(RTC_IRQ_STA, &irqsta);
	rtc_read(RTC_IRQ_EN, &irqen);
	rtc_read(RTC_OSC32CON, &osc32);
	rtc_read(RTC_POWERKEY1, &pwrkey1);
	rtc_read(RTC_POWERKEY2, &pwrkey2);
	rtc_read(RTC_PROT, &prot);
	rtc_read(RTC_CON, &con);
	rtc_read(RTC_TC_SEC, &sec1);
	mdelay(2000);
	rtc_read(RTC_TC_SEC, &sec2);

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

static int mtk_rtc_is_alarm_irq(void)
{
	u32 irqsta = 0, bbpu;
	int ret, val;

	ret = rtc_read(RTC_IRQ_STA, &irqsta);	/* read clear */
	if ((ret == 0) && (irqsta & RTC_IRQ_STA_AL)) {
		bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN;
		rtc_write(RTC_BBPU, bbpu);
		val = rtc_write_trigger();
		if (val < 0)
			pr_notice("%s error\n", __func__);
		return RTC_ALSTA;
	}

#ifndef USER_BUILD_KERNEL
	if ((ret == 0) && (irqsta & RTC_IRQ_STA_LP)) {
		mtk_rtc_lp_exception();
		return RTC_LPSTA;
	}
#endif

	return RTC_NONE;
}

static void mtk_rtc_update_pwron_alarm_flag(void)
{
	int ret;

	ret = rtc_update_bits(RTC_PDN1, RTC_PDN1_PWRON_TIME, 0);
	if (ret < 0)
		goto exit;
	ret = rtc_update_bits(RTC_PDN2,
						  RTC_PDN2_PWRON_ALARM,
						  RTC_PDN2_PWRON_ALARM);
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	return;
exit:
	pr_err("%s error\n", __func__);
}

static void mtk_rtc_reset_bbpu_alarm_status(void)
{
	u32 bbpu;
	int ret;


	if (apply_lpsd_solution) {
		pr_notice("%s:lpsd\n", __func__);
		return;
	}

	bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN | RTC_BBPU_RESET_AL;
	rtc_write(RTC_BBPU, bbpu);
	ret = rtc_write_trigger();
	if (ret < 0)
		pr_err("%s error\n", __func__);

	return;

}

static irqreturn_t mtk_rtc_irq_handler(int irq, void *data)
{
	bool pwron_alm = false, pwron_alarm = false;
	struct rtc_time nowtm, tm;
	int status = RTC_NONE;
	unsigned long flags;

	spin_lock_irqsave(&mt_rtc->lock, flags);

	status = mtk_rtc_is_alarm_irq();

	pr_notice("%s:%d\n", __func__, status);

	if (status == RTC_NONE) {
		spin_unlock_irqrestore(&mt_rtc->lock, flags);
		return IRQ_NONE;
	}

	if (status == RTC_LPSTA) {
		spin_unlock_irqrestore(&mt_rtc->lock, flags);
		return IRQ_HANDLED;
	}

	mtk_rtc_reset_bbpu_alarm_status();

	pwron_alarm = mtk_rtc_is_pwron_alarm(&nowtm, &tm);
	nowtm.tm_year += RTC_MIN_YEAR;
	tm.tm_year += RTC_MIN_YEAR;
	if (pwron_alarm) {
		time64_t now_time, time;

		now_time =
		    mktime(nowtm.tm_year, nowtm.tm_mon, nowtm.tm_mday,
			   nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);

		if (now_time == -1) {
			spin_unlock_irqrestore(&mt_rtc->lock, flags);
			goto out;
		}

		time =
		    mktime(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
			   tm.tm_min, tm.tm_sec);

		if (time == -1) {
			spin_unlock_irqrestore(&mt_rtc->lock, flags);
			goto out;
		}

		/* power on */
		if (now_time >= time - 1 && now_time <= time + 4) {
			if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
			    || get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT) {
				mtk_rtc_reboot();
				spin_unlock_irqrestore(&mt_rtc->lock, flags);
				disable_irq_nosync(mt_rtc->irq);
				goto out;
			} else {
				mtk_rtc_update_pwron_alarm_flag();
				pwron_alm = true;
			}
		} else if (now_time < time) {	/* set power-on alarm */
			time -= 1;
			rtc_time64_to_tm(time, &tm);
			tm.tm_year -= RTC_MIN_YEAR_OFFSET;
			tm.tm_mon += 1;
			mtk_rtc_set_alarm(&tm);
		}
	}
	spin_unlock_irqrestore(&mt_rtc->lock, flags);
out:
	if (mt_rtc->rtc_dev != NULL)
		rtc_update_irq(mt_rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);

	if (rtc_show_alarm)
		pr_notice("%s time is up\n", pwron_alm ? "power-on" : "alarm");

	return IRQ_HANDLED;
}

static int rtc_ops_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned long long time;
	unsigned long flags;
	struct mt6358_rtc *rtc = dev_get_drvdata(dev);
	int ret;

	spin_lock_irqsave(&rtc->lock, flags);
	ret = mtk_rtc_read_time(tm);
	if (ret < 0)
		goto exit;
	spin_unlock_irqrestore(&rtc->lock, flags);

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;
	time = rtc_tm_to_time64(tm);

	do_div(time, 86400);
	time += 4;
	tm->tm_wday = do_div(time,  7);	/* 1970/01/01 is Thursday */

	if (rtc_show_time) {
		pr_notice("read tc time = %04d/%02d/%02d (%d) %02d:%02d:%02d\n",
			  tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1,
			  tm->tm_mday, tm->tm_wday, tm->tm_hour,
			  tm->tm_min, tm->tm_sec);
	}

	return ret;

exit:
	spin_unlock_irqrestore(&rtc->lock, flags);
	pr_err("%s error\n", __func__);
	return ret;
}

static int rtc_ops_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mt6358_rtc *rtc = dev_get_drvdata(dev);
	unsigned long flags;
	u16 data[RTC_OFFSET_COUNT];
	int ret;

	if (tm->tm_year > 195) {
		pr_err("%s: invalid year %04d > 2095\n",
					__func__, tm->tm_year + RTC_BASE_YEAR);
		return -EINVAL;
	}

	pr_notice("set tc time = %04d/%02d/%02d %02d:%02d:%02d\n",
		  tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	spin_lock_irqsave(&rtc->lock, flags);
	ret = rtc_bulk_access(BULK_WRITE, RTC_TC_SEC, data, RTC_OFFSET_COUNT);
		if (ret < 0)
			goto exit;

	ret = rtc_write_trigger();
		if (ret < 0)
			goto exit;
	spin_unlock_irqrestore(&rtc->lock, flags);

	return ret;

exit:
	spin_unlock_irqrestore(&rtc->lock, flags);
	pr_err("%s error\n", __func__);
	return ret;
}

static int rtc_ops_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	unsigned long flags;
	struct rtc_time *tm = &alm->time;
	struct mt6358_rtc *rtc = dev_get_drvdata(dev);
	u32 irqen = 0, pdn2 = 0;
	u16 data[RTC_OFFSET_COUNT];
	int ret;

	spin_lock_irqsave(&rtc->lock, flags);

	ret = rtc_read(RTC_IRQ_EN, &irqen);
	if (ret < 0)
		goto exit;
	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);

	/* return Power-On Alarm bit */
	ret = rtc_read(RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);

	ret = rtc_bulk_access(BULK_READ, RTC_AL_SEC, data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_AL_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_AL_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_AL_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_AL_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_AL_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_AL_YEA_MASK;

	spin_unlock_irqrestore(&rtc->lock, flags);

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

	pr_notice("read al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		  tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	return ret;

exit:
	spin_unlock_irqrestore(&rtc->lock, flags);
	pr_err("%s error\n", __func__);
	return ret;
}

static int rtc_ops_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	unsigned long flags;
	struct rtc_time tm = alm->time;
	ktime_t target;
	struct mt6358_rtc *rtc = dev_get_drvdata(dev);
	u32 irqsta;
	int ret = 0;

	if (tm.tm_year > 195) {
		pr_err("%s: invalid year %04d > 2095\n",
					__func__, tm.tm_year + RTC_BASE_YEAR);
		return -EINVAL;
	}

	if (alm->enabled == 1) {
		/* Add one more second to postpone wake time. */
		target = rtc_tm_to_ktime(tm);
		target = ktime_add_ns(target, NSEC_PER_SEC);
		tm = rtc_ktime_to_tm(target);
	} else if (alm->enabled == 5) {
		/* Power on system 1 minute earlier */
		alarm1m15s = 1;
	}

	tm.tm_year -= RTC_MIN_YEAR_OFFSET;
	tm.tm_mon++;

	pr_notice("set al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		  tm.tm_year + RTC_MIN_YEAR, tm.tm_mon, tm.tm_mday,
		  tm.tm_hour, tm.tm_min, tm.tm_sec, alm->enabled);

	spin_lock_irqsave(&rtc->lock, flags);
	if (alm->enabled == 2) {	/* enable power-on alarm */
		ret = mtk_rtc_set_pwron_alarm(true, &tm, false);
	} else if (alm->enabled == 3 || alm->enabled == 5) {
		/* enable power-on alarm with logo */
		ret = mtk_rtc_set_pwron_alarm(true, &tm, true);
	} else if (alm->enabled == 4) {	/* disable power-on alarm */
		ret = mtk_rtc_set_pwron_alarm(false, &tm, false);
		alarm1m15s = 0;
	}
	if (ret < 0)
		goto exit;

	/* disable alarm and clear Power-On Alarm bit */
	ret = rtc_update_bits(RTC_IRQ_EN, RTC_IRQ_EN_AL, 0);
	if (ret < 0)
		goto exit;
	ret = rtc_update_bits(RTC_PDN2, RTC_PDN2_PWRON_ALARM, 0);
	if (ret < 0)
		goto exit;
	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;
	ret = rtc_read(RTC_IRQ_STA, &irqsta);	/* read clear */
	if (ret < 0)
		goto exit;

	if (alm->enabled)
		ret = mtk_rtc_set_alarm(&tm);
	spin_unlock_irqrestore(&rtc->lock, flags);

	return ret;
exit:
	spin_unlock_irqrestore(&rtc->lock, flags);
	pr_err("%s error\n", __func__);
	return ret;
}

static const struct rtc_class_ops rtc_ops = {
	.read_time = rtc_ops_read_time,
	.set_time = rtc_ops_set_time,
	.read_alarm = rtc_ops_read_alarm,
	.set_alarm = rtc_ops_set_alarm,
};

static void mtk_rtc_set_lp_irq(void)
{
	unsigned int irqen = 0;
	int ret;

#ifndef USER_BUILD_KERNEL
	irqen = RTC_IRQ_EN_LP;
#endif
	ret = rtc_update_bits(RTC_IRQ_EN, RTC_IRQ_EN_LP, irqen);
	if (ret < 0)
		goto exit;

	ret = rtc_write_trigger();
	if (ret < 0)
		goto exit;

	return;
exit:
	pr_err("%s error\n", __func__);
}

static int mtk_rtc_pdrv_probe(struct platform_device *pdev)
{
#ifndef IPIMB
	struct mt6358_chip *mt6358_chip = dev_get_drvdata(pdev->dev.parent);
#endif
	struct mt6358_rtc *rtc;
	unsigned long flags;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6358_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq <= 0)
		return -EINVAL;
	pr_notice("%s: rtc->irq = %d(%d)\n", __func__, rtc->irq,
					platform_get_irq_byname(pdev, "rtc"));

#ifndef IPIMB
	rtc->regmap = mt6358_chip->regmap;
#else
	rtc->regmap = dev_get_regmap(pdev->dev.parent->parent, NULL);
#endif
	if (!rtc->regmap) {
		pr_err("%s: get regmap failed\n", __func__);
		return -ENODEV;
	}

	rtc->dev = &pdev->dev;
	spin_lock_init(&rtc->lock);

	mt_rtc = rtc;
	platform_set_drvdata(pdev, rtc);

	if (of_property_read_u32(pdev->dev.of_node, "base", &rtc->addr_base))
		rtc->addr_base = RTC_DSN_ID;
	pr_notice("%s: rtc->addr_base =0x%x\n", __func__, rtc->addr_base);

	spin_lock_irqsave(&rtc->lock, flags);
	mtk_rtc_set_lp_irq();
	spin_unlock_irqrestore(&rtc->lock, flags);

	ret = request_threaded_irq(rtc->irq, NULL,
				   mtk_rtc_irq_handler,
				   IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				   "mt6358-rtc", rtc);
	if (ret) {
		dev_dbg(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		goto out_dispose_irq;
	}

	device_init_wakeup(&pdev->dev, 1);

	mt6358_rtc_suspend_lock =
		wakeup_source_register(NULL, "mt6358-rtc suspend wakelock");

	/* register rtc device (/dev/rtc0) */
	rtc->rtc_dev = rtc_device_register(RTC_NAME,
					&pdev->dev, &rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		dev_dbg(&pdev->dev, "register rtc device failed\n");
		ret = PTR_ERR(rtc->rtc_dev);
		goto out_free_irq;
	}

	if (of_property_read_bool(pdev->dev.of_node, "apply-lpsd-solution")) {
		apply_lpsd_solution = 1;
		pr_notice("%s: apply_lpsd_solution\n", __func__);
	}

#ifdef CONFIG_PM
	if (register_pm_notifier(&rtc_pm_notifier_func))
		pr_notice("rtc pm failed\n");
	else
		rtc_pm_notifier_registered = true;
#endif /* CONFIG_PM */

	INIT_WORK(&rtc->work, mtk_rtc_work_queue);

	return 0;
out_free_irq:
	free_irq(rtc->irq, rtc->rtc_dev);
out_dispose_irq:
	irq_dispose_mapping(rtc->irq);
	return ret;
}

static int mtk_rtc_pdrv_remove(struct platform_device *pdev)
{
	struct mt6358_rtc *rtc = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc->rtc_dev);

	return 0;
}

static void mtk_rtc_pdrv_shutdown(struct platform_device *pdev)
{
	struct rtc_time rtc_time_now;
	struct rtc_time rtc_time_alarm;
	ktime_t ktime_now;
	ktime_t ktime_alarm;
	bool is_pwron_alarm;

	if (alarm1m15s == 1) {
		is_pwron_alarm = mtk_rtc_is_pwron_alarm(&rtc_time_now,
			&rtc_time_alarm);
		if (is_pwron_alarm) {
			rtc_time_now.tm_year += RTC_MIN_YEAR_OFFSET;
			rtc_time_now.tm_mon--;
			rtc_time_alarm.tm_year += RTC_MIN_YEAR_OFFSET;
			rtc_time_alarm.tm_mon--;
			pr_notice("now = %04d/%02d/%02d %02d:%02d:%02d\n",
				rtc_time_now.tm_year + 1900,
				rtc_time_now.tm_mon + 1,
				rtc_time_now.tm_mday,
				rtc_time_now.tm_hour,
				rtc_time_now.tm_min,
				rtc_time_now.tm_sec);
			pr_notice("alarm = %04d/%02d/%02d %02d:%02d:%02d\n",
				rtc_time_alarm.tm_year + 1900,
				rtc_time_alarm.tm_mon + 1,
				rtc_time_alarm.tm_mday,
				rtc_time_alarm.tm_hour,
				rtc_time_alarm.tm_min,
				rtc_time_alarm.tm_sec);
			ktime_now = rtc_tm_to_ktime(rtc_time_now);
			ktime_alarm = rtc_tm_to_ktime(rtc_time_alarm);
			if (ktime_after(ktime_alarm, ktime_now)) {
				/* alarm has not happened */
				ktime_alarm = ktime_sub_ms(ktime_alarm,
					MSEC_PER_SEC * 60);
				if (ktime_after(ktime_alarm, ktime_now))
					pr_notice("Alarm will happen after 1 minute\n");
				else {
					ktime_alarm = ktime_add_ms(ktime_now,
						MSEC_PER_SEC * 15);
					pr_notice("Alarm will happen in 15 seconds\n");
				}
				rtc_time_alarm = rtc_ktime_to_tm(ktime_alarm);
				pr_notice("new alarm = %04d/%02d/%02d %02d:%02d:%02d\n",
					rtc_time_alarm.tm_year + 1900,
					rtc_time_alarm.tm_mon + 1,
					rtc_time_alarm.tm_mday,
					rtc_time_alarm.tm_hour,
					rtc_time_alarm.tm_min,
					rtc_time_alarm.tm_sec);
				rtc_time_alarm.tm_year -= RTC_MIN_YEAR_OFFSET;
				rtc_time_alarm.tm_mon++;
				mtk_rtc_set_pwron_alarm_time(&rtc_time_alarm);
				mtk_rtc_set_alarm(&rtc_time_alarm);
			} else
				pr_notice("Alarm has happened before\n");
		} else
			pr_notice("No power-off alarm is set\n");
	}

}

static const struct of_device_id mt6358_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6357-rtc", },
	{ .compatible = "mediatek,mt6358-rtc", },
	{ .compatible = "mediatek,mt6359-rtc", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6358_rtc_of_match);

static struct platform_driver mtk_rtc_pdrv = {
	.probe = mtk_rtc_pdrv_probe,
	.remove = mtk_rtc_pdrv_remove,
	.shutdown = mtk_rtc_pdrv_shutdown,
	.driver = {
		   .name = RTC_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt6358_rtc_of_match,
		   },
};

module_platform_driver(mtk_rtc_pdrv);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wilma Wu <wilma.wu@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6358 PMIC");

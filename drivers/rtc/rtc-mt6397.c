// SPDX-License-Identifier: GPL-2.0-only
/*
* Copyright (c) 2014-2015 MediaTek Inc.
* Author: Tianping.Fang <tianping.fang@mediatek.com>
*/

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/mfd/mt6397/rtc.h>
#include <linux/mod_devicetable.h>
#include <linux/nvmem-provider.h>
#include <linux/sched_clock.h>

#ifdef SUPPORT_EOSC_CALI
#include <linux/mfd/mt6359p/registers.h>
#endif

#ifdef SUPPORT_PWR_OFF_ALARM
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include <linux/reboot.h>
#endif


/*debug information*/
static int rtc_show_time;
static int rtc_show_alarm = 1;

module_param(rtc_show_time, int, 0644);
module_param(rtc_show_alarm, int, 0644);


static int mtk_rtc_write_trigger(struct mt6397_rtc *rtc);

static u16 rtc_pwron_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_PWRON_SEC, RTC_PWRON_SEC_MASK, RTC_PWRON_SEC_SHIFT},
	{RTC_PWRON_MIN, RTC_PWRON_MIN_MASK, RTC_PWRON_MIN_SHIFT},
	{RTC_PWRON_HOU, RTC_PWRON_HOU_MASK, RTC_PWRON_HOU_SHIFT},
	{RTC_PWRON_DOM, RTC_PWRON_DOM_MASK, RTC_PWRON_DOM_SHIFT},
	{0, 0, 0},
	{RTC_PWRON_MTH, RTC_PWRON_MTH_MASK, RTC_PWRON_MTH_SHIFT},
	{RTC_PWRON_YEA, RTC_PWRON_YEA_MASK, RTC_PWRON_YEA_SHIFT},
};

static const struct reg_field mtk_rtc_spare_reg_fields[SPARE_RG_MAX] = {
	[SPARE_AL_HOU] = REG_FIELD(RTC_AL_HOU, 8, 15),
	[SPARE_AL_MTH] = REG_FIELD(RTC_AL_MTH, 8, 15),
	[SPARE_SPAR0]  = REG_FIELD(RTC_SPAR0, 0, 7),
#ifdef SUPPORT_PWR_OFF_ALARM
	[SPARE_KPOC]   = REG_FIELD(RTC_PDN1, 14, 14),
#endif
};

#ifdef SUPPORT_EOSC_CALI
static const struct reg_field mt6359_cali_reg_fields[CALI_FILED_MAX] = {
	[RTC_EOSC32_CK_PDN]	= REG_FIELD(MT6359P_SCK_TOP_CKPDN_CON0, 2, 2),
	[EOSC_CALI_TD]		= REG_FIELD(MT6359P_RTC_AL_DOW, 5, 7),
	[RTC_K_EOSC_RSV]	= REG_FIELD(MT6359P_RTC_AL_YEA, 8, 10),
};

static int rtc_eosc_cali_td;
module_param(rtc_eosc_cali_td, int, 0644);


static void mtk_rtc_enable_k_eosc(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	u32 td;

	if (!rtc->cali_is_supported)
		return;

	/* Truning on eosc cali mode clock */
	regmap_field_write(rtc->cali[RTC_EOSC32_CK_PDN], 0);

	if (rtc_eosc_cali_td) {
		dev_notice(dev, "%s: rtc_eosc_cali_td = %d\n",
				__func__, rtc_eosc_cali_td);
		switch (rtc_eosc_cali_td) {
		case 1:
			td = EOSC_CALI_TD_01_SEC;
			break;
		case 2:
			td = EOSC_CALI_TD_02_SEC;
			break;
		case 4:
			td = EOSC_CALI_TD_04_SEC;
			break;
		case 16:
			td = EOSC_CALI_TD_16_SEC;
			break;
		default:
			td = EOSC_CALI_TD_08_SEC;
			break;
		}
		regmap_field_write(rtc->cali[EOSC_CALI_TD], td);
	}

	if (rtc->data->eosc_cali_version == EOSC_CALI_MT6359P_SERIES)
		regmap_field_write(rtc->cali[RTC_K_EOSC_RSV], EOSC_SOL_2);

	mtk_rtc_write_trigger(rtc);
}

static int mtk_rtc_config_eosc_cali(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < CALI_FILED_MAX; i++) {
		rtc->cali[i] = devm_regmap_field_alloc(dev, rtc->regmap,
					rtc->data->cali_reg_fields[i]);
		if (IS_ERR(rtc->cali[i])) {
			dev_err(rtc->rtc_dev->dev.parent,
				"cali regmap field[%d] err= %ld\n",
				i, PTR_ERR(rtc->cali[i]));
			return PTR_ERR(rtc->cali[i]);
		}
	}
	rtc->cali_is_supported = true;

	return 0;
}
#endif

#ifdef SUPPORT_PWR_OFF_ALARM

static u32 bootmode = NORMAL_BOOT;
static struct wakeup_source *mt6397_rtc_suspend_lock;
static bool rtc_pm_notifier_registered;
static bool kpoc_alarm;
static unsigned long rtc_pm_status;


#if IS_ENABLED(CONFIG_PM)

#define PM_DUMMY 0xFFFF

static int rtc_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	struct mt6397_rtc *rtc = container_of(notifier,
		struct mt6397_rtc, pm_nb);

	dev_notice(rtc->rtc_dev->dev.parent, "%s = %lu\n", __func__, pm_event);

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
		dev_notice(rtc->rtc_dev->dev.parent,
				"%s trigger reboot\n", __func__);
		complete(&rtc->comp);
		kpoc_alarm = false;
	}
	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

static void rtc_mark_kpoc(struct mt6397_rtc *rtc)
{
	mutex_lock(&rtc->lock);
	regmap_field_write(rtc->spare[SPARE_KPOC], 1);
	mtk_rtc_write_trigger(rtc);
	mutex_unlock(&rtc->lock);
}

static void mtk_rtc_work_queue(struct work_struct *work)
{
	struct mt6397_rtc *rtc = container_of(work, struct mt6397_rtc, work);
	unsigned long ret;
	unsigned int msecs;

	ret = wait_for_completion_timeout(&rtc->comp, msecs_to_jiffies(30000));
	if (!ret) {
		dev_notice(rtc->rtc_dev->dev.parent, "%s timeout\n", __func__);
		BUG_ON(1);
	} else {
		msecs = jiffies_to_msecs(ret);
		dev_notice(rtc->rtc_dev->dev.parent,
				"%s timeleft= %d\n", __func__, msecs);
		rtc_mark_kpoc(rtc);
		kernel_restart("kpoc");
	}
}

static void mtk_rtc_reboot(struct mt6397_rtc *rtc)
{
	__pm_stay_awake(mt6397_rtc_suspend_lock);

	init_completion(&rtc->comp);
	schedule_work_on(cpumask_first(cpu_online_mask), &rtc->work);

	if (!rtc_pm_notifier_registered)
		goto reboot;

	if (rtc_pm_status != PM_SUSPEND_PREPARE)
		goto reboot;

	kpoc_alarm = true;

	dev_notice(rtc->rtc_dev->dev.parent, "%s:wait\n", __func__);
	return;

reboot:
	dev_notice(rtc->rtc_dev->dev.parent, "%s:trigger\n", __func__);
	complete(&rtc->comp);
}

static void mtk_rtc_update_pwron_alarm_flag(struct mt6397_rtc *rtc)
{
	int ret;

	dev_notice(rtc->rtc_dev->dev.parent, "%s\n", __func__);

	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN1,
				RTC_PDN1_PWRON_TIME, 0);
	if (ret < 0)
		goto exit;

	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN2,
				RTC_PDN2_PWRON_ALARM, RTC_PDN2_PWRON_ALARM);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	return;
exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
}

static int mtk_rtc_restore_alarm(struct mt6397_rtc *rtc, struct rtc_time *tm)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			    data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;
	data[RTC_OFFSET_SEC] = ((data[RTC_OFFSET_SEC] & ~(RTC_AL_SEC_MASK)) |
				(tm->tm_sec & RTC_AL_SEC_MASK));
	data[RTC_OFFSET_MIN] = ((data[RTC_OFFSET_MIN] & ~(RTC_AL_MIN_MASK)) |
				(tm->tm_min & RTC_AL_MIN_MASK));
	data[RTC_OFFSET_HOUR] = ((data[RTC_OFFSET_HOUR] & ~(RTC_AL_HOU_MASK)) |
				(tm->tm_hour & RTC_AL_HOU_MASK));
	data[RTC_OFFSET_DOM] = ((data[RTC_OFFSET_DOM] & ~(RTC_AL_DOM_MASK)) |
				(tm->tm_mday & RTC_AL_DOM_MASK));
	data[RTC_OFFSET_MTH] = ((data[RTC_OFFSET_MTH] & ~(RTC_AL_MTH_MASK)) |
				(tm->tm_mon & RTC_AL_MTH_MASK));
	data[RTC_OFFSET_YEAR] = ((data[RTC_OFFSET_YEAR] & ~(RTC_AL_YEA_MASK)) |
				(tm->tm_year & RTC_AL_YEA_MASK));

	dev_notice(rtc->rtc_dev->dev.parent,
		"restore al time = %04d/%02d/%02d %02d:%02d:%02d\n",
		tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	ret = regmap_bulk_write(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
				data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_AL_MASK,
				RTC_AL_MASK_DOW);
	if (ret < 0)
		goto exit;
	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_IRQ_EN,
				RTC_IRQ_EN_ONESHOT_AL,
				RTC_IRQ_EN_ONESHOT_AL);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	return ret;

exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
	return ret;
}

bool mtk_rtc_is_pwron_alarm(struct mt6397_rtc *rtc,
	struct rtc_time *nowtm, struct rtc_time *tm)
{
	u32 pdn1 = 0, spar1 = 0, pdn2 = 0, spar0 = 0;
	int ret, sec = 0;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	dev_notice(rtc->rtc_dev->dev.parent, "pdn1 = 0x%x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {/* power-on time is available */

		/*get current rtc time*/
		do {
			ret = regmap_bulk_read(rtc->regmap,
						rtc->addr_base + RTC_TC_SEC,
						data, RTC_OFFSET_COUNT);
			if (ret < 0)
				goto exit;
			nowtm->tm_sec = data[RTC_OFFSET_SEC] & RTC_TC_SEC_MASK;
			nowtm->tm_min = data[RTC_OFFSET_MIN] & RTC_TC_MIN_MASK;
			nowtm->tm_hour =
				data[RTC_OFFSET_HOUR] & RTC_TC_HOU_MASK;
			nowtm->tm_mday = data[RTC_OFFSET_DOM] & RTC_TC_DOM_MASK;
			nowtm->tm_mon = data[RTC_OFFSET_MTH] & RTC_TC_MTH_MASK;
			nowtm->tm_year =
				data[RTC_OFFSET_YEAR] & RTC_TC_YEA_MASK;

			ret = regmap_read(rtc->regmap,
					rtc->addr_base + RTC_TC_SEC, &sec);
			if (ret < 0)
				goto exit;
			sec &= RTC_TC_SEC_MASK;

		} while (sec < nowtm->tm_sec);

		dev_notice(rtc->rtc_dev->dev.parent,
			"get now time = %04d/%02d/%02d %02d:%02d:%02d\n",
			nowtm->tm_year + RTC_MIN_YEAR, nowtm->tm_mon,
			nowtm->tm_mday, nowtm->tm_hour,
			nowtm->tm_min, nowtm->tm_sec);

		/*get power on time from SPARE */
		ret = regmap_read(rtc->regmap,
				rtc->addr_base + RTC_SPAR0, &spar0);
		if (ret < 0)
			goto exit;

		ret = regmap_read(rtc->regmap,
					rtc->addr_base + RTC_SPAR1, &spar1);
		if (ret < 0)
			goto exit;

		ret = regmap_read(rtc->regmap,
					rtc->addr_base + RTC_PDN2, &pdn2);
		if (ret < 0)
			goto exit;
		dev_notice(rtc->rtc_dev->dev.parent,
			"spar0=0x%x, spar1=0x%x, pdn2=0x%x\n",
			spar0, spar1, pdn2);

		tm->tm_sec =
			(spar0 & RTC_PWRON_SEC_MASK) >> RTC_PWRON_SEC_SHIFT;
		tm->tm_min =
			(spar1 & RTC_PWRON_MIN_MASK) >> RTC_PWRON_MIN_SHIFT;
		tm->tm_hour =
			(spar1 & RTC_PWRON_HOU_MASK) >> RTC_PWRON_HOU_SHIFT;
		tm->tm_mday =
			(spar1 & RTC_PWRON_DOM_MASK) >> RTC_PWRON_DOM_SHIFT;
		tm->tm_mon =
			(pdn2 & RTC_PWRON_MTH_MASK) >> RTC_PWRON_MTH_SHIFT;
		tm->tm_year =
			(pdn2 & RTC_PWRON_YEA_MASK) >> RTC_PWRON_YEA_SHIFT;

		dev_notice(rtc->rtc_dev->dev.parent,
		"get pwron time = %04d/%02d/%02d %02d:%02d:%02d\n",
		tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

		return true;
	}
	return false;
exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
	return false;
}
#endif

static int mtk_rtc_write_trigger(struct mt6397_rtc *rtc)
{
	int ret;
	u32 data;

	ret = regmap_write(rtc->regmap, rtc->addr_base + rtc->data->wrtgr, 1);
	if (ret < 0)
		return ret;

	ret = regmap_read_poll_timeout(rtc->regmap,
					rtc->addr_base + RTC_BBPU, data,
					!(data & RTC_BBPU_CBUSY),
					MTK_RTC_POLL_DELAY_US,
					MTK_RTC_POLL_TIMEOUT);
	if (ret < 0)
		dev_err(rtc->rtc_dev->dev.parent,
			"failed to write WRTGR: %d\n", ret);

	return ret;
}

static int rtc_nvram_read(void *priv, unsigned int offset, void *val,
							size_t bytes)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(priv);
	unsigned int ival;
	int ret;
	u8 *buf = val;

	mutex_lock(&rtc->lock);

	for (; bytes; bytes--) {
		ret = regmap_field_read(rtc->spare[offset++], &ival);
		if (ret)
			goto out;
		*buf++ = (u8)ival;
	}
out:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int rtc_nvram_write(void *priv, unsigned int offset, void *val,
							size_t bytes)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(priv);
	unsigned int ival;
	int ret;
	u8 *buf = val;

	mutex_lock(&rtc->lock);

	for (; bytes; bytes--) {
		ival = *buf++;
		ret = regmap_field_write(rtc->spare[offset++], ival);
		if (ret)
			goto out;
	}
	mtk_rtc_write_trigger(rtc);
out:
	mutex_unlock(&rtc->lock);
	return ret;
}

static void mtk_rtc_reset_bbpu_alarm_status(struct mt6397_rtc *rtc)
{
	u32 bbpu;
	int ret;

	bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN | RTC_BBPU_RESET_AL;
	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_BBPU, bbpu);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	return;
exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);

}

#ifndef USER_BUILD_KERNEL
void mtk_rtc_lp_exception(struct mt6397_rtc *rtc)
{
	u32 bbpu = 0, irqsta = 0, irqen = 0, osc32 = 0;
	u32 pwrkey1 = 0, pwrkey2 = 0, prot = 0, con = 0, sec1 = 0, sec2 = 0;

	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_BBPU, &bbpu);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_IRQ_STA, &irqsta);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_IRQ_EN, &irqen);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_OSC32CON, &osc32);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_POWERKEY1, &pwrkey1);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_POWERKEY2, &pwrkey2);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_PROT, &prot);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_CON, &con);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_TC_SEC, &sec1);
	mdelay(2000);
	regmap_read(rtc->regmap,
				rtc->addr_base + RTC_TC_SEC, &sec2);

	dev_emerg(rtc->rtc_dev->dev.parent, "!!! 32K WAS STOPPED !!!\n"
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

static int mtk_rtc_is_alarm_irq(struct mt6397_rtc *rtc)
{
	u32 irqsta = 0, bbpu;
	int ret;

	/* read clear */
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if ((ret == 0) && (irqsta & RTC_IRQ_STA_AL)) {
		bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN;
		ret = regmap_write(rtc->regmap,
					rtc->addr_base + RTC_BBPU, bbpu);
		if (ret < 0)
			dev_err(rtc->rtc_dev->dev.parent,
				"%s error\n", __func__);
		mtk_rtc_write_trigger(rtc);

		return RTC_ALSTA;
	}
#ifndef USER_BUILD_KERNEL
	if ((ret == 0) && (irqsta & RTC_IRQ_STA_LP))
		mtk_rtc_lp_exception(rtc);
#endif

	return RTC_NONE;
}

static irqreturn_t mtk_rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6397_rtc *rtc = data;
	bool pwron_alm = false;
	int status = RTC_NONE;
#ifdef SUPPORT_PWR_OFF_ALARM
	bool pwron_alarm = false;
	struct rtc_time nowtm, tm;
#endif

	mutex_lock(&rtc->lock);

	status = mtk_rtc_is_alarm_irq(rtc);

	dev_notice(rtc->rtc_dev->dev.parent, "%s:%d\n", __func__, status);

	if (status == RTC_NONE) {
		mutex_unlock(&rtc->lock);
		return IRQ_NONE;
	}

	if (status == RTC_LPSTA) {
		mutex_unlock(&rtc->lock);
		return IRQ_HANDLED;
	}

	mtk_rtc_reset_bbpu_alarm_status(rtc);

#ifdef SUPPORT_PWR_OFF_ALARM
	pwron_alarm = mtk_rtc_is_pwron_alarm(rtc, &nowtm, &tm);
	nowtm.tm_year += RTC_MIN_YEAR;
	tm.tm_year += RTC_MIN_YEAR;
	if (pwron_alarm) {
		time64_t now_time, time;

		now_time =
		    mktime64(nowtm.tm_year, nowtm.tm_mon, nowtm.tm_mday,
			   nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);

		if (now_time == -1) {
			mutex_unlock(&rtc->lock);
			goto out;
		}

		time =
		    mktime64(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour,
			   tm.tm_min, tm.tm_sec);

		if (time == -1) {
			mutex_unlock(&rtc->lock);
			goto out;
		}

		/* power on */
		if (now_time >= time - 1 && now_time <= time + 4) {
			if (bootmode == KERNEL_POWER_OFF_CHARGING_BOOT ||
				bootmode == LOW_POWER_OFF_CHARGING_BOOT) {
				mtk_rtc_reboot(rtc);
				mutex_unlock(&rtc->lock);
				disable_irq_nosync(rtc->irq);
				goto out;
			} else {
				mtk_rtc_update_pwron_alarm_flag(rtc);
				pwron_alm = true;
			}
		} else if (now_time < time) {	/* set power-on alarm */
			time -= 1;
			rtc_time64_to_tm(time, &tm);
			tm.tm_year -= RTC_MIN_YEAR_OFFSET;
			tm.tm_mon += 1;
			mtk_rtc_restore_alarm(rtc, &tm);
		}
	}
#endif
	mutex_unlock(&rtc->lock);

out:
	if (rtc->rtc_dev != NULL)
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);

	if (rtc_show_alarm)
		dev_notice(rtc->rtc_dev->dev.parent, "%s time is up\n",
					pwron_alm ? "power-on" : "alarm");

	return IRQ_HANDLED;
}

static int __mtk_rtc_read_time(struct mt6397_rtc *rtc,
			       struct rtc_time *tm, int *sec)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_TC_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_TC_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_TC_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_TC_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_TC_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_TC_YEA_MASK;

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC, sec);
	*sec &= RTC_TC_SEC_MASK;
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static void mtk_rtc_set_pwron_time(struct mt6397_rtc *rtc, struct rtc_time *tm)
{
	u32 data[RTC_OFFSET_COUNT];
	int ret, i;

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
		ret = regmap_update_bits(rtc->regmap,
			rtc->addr_base + rtc_pwron_reg[i][RTC_REG],
			rtc_pwron_reg[i][RTC_MASK], data[i]);
		if (ret < 0)
			goto exit;
		mtk_rtc_write_trigger(rtc);
	}
	return;
exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
}

void mtk_rtc_save_pwron_time(struct mt6397_rtc *rtc,
	bool enable, struct rtc_time *tm)
{
	u32 pdn1 = 0;
	int ret;

	/* set power on time */
	mtk_rtc_set_pwron_time(rtc, tm);

	/* update power on alarm related flags */
	if (enable)
		pdn1 = RTC_PDN1_PWRON_TIME;
	ret = regmap_update_bits(rtc->regmap,
				rtc->addr_base + RTC_PDN1,
				RTC_PDN1_PWRON_TIME, pdn1);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);

	return;

exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
}

static int mtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	time64_t time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int days, sec, ret;
	unsigned long long timeout = sched_clock() + 500000000;

	do {
		ret = __mtk_rtc_read_time(rtc, tm, &sec);
		if (ret < 0)
			goto exit;
		if (sched_clock() > timeout) {
			pr_notice("%s, time out\n", __func__);
			break;
		}
	} while (sec < tm->tm_sec);

	/* HW register use 7 bits to store year data, minus
	 * RTC_MIN_YEAR_OFFSET before write year data to register, and plus
	 * RTC_MIN_YEAR_OFFSET back after read year from register
	 */
	tm->tm_year += RTC_MIN_YEAR_OFFSET;

	/* HW register start mon from one, but tm_mon start from zero. */
	tm->tm_mon--;
	time = rtc_tm_to_time64(tm);

	/* rtc_tm_to_time64 covert Gregorian date to seconds since
	 * 01-01-1970 00:00:00, and this date is Thursday.
	 */
	days = div_s64(time, 86400);
	tm->tm_wday = (days + 4) % 7;

	if (rtc_show_time) {
		dev_notice(rtc->rtc_dev->dev.parent,
			"read tc time = %04d/%02d/%02d (%d) %02d:%02d:%02d\n",
			tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1,
			tm->tm_mday, tm->tm_wday, tm->tm_hour,
			tm->tm_min, tm->tm_sec);
	}
exit:
	return ret;
}

static int mtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	dev_notice(rtc->rtc_dev->dev.parent,
			"set tc time = %04d/%02d/%02d %02d:%02d:%02d\n",
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

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_write(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
				data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	/* Time register write to hardware after call trigger function */
	ret = mtk_rtc_write_trigger(rtc);

exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	u32 irqen = 0, pdn2 = 0;
	int ret;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	mutex_lock(&rtc->lock);
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_EN, &irqen);
	if (ret < 0)
		goto err_exit;
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto err_exit;

	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto err_exit;

	alm->enabled = !!(irqen & RTC_IRQ_EN_AL);
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);
	mutex_unlock(&rtc->lock);

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_AL_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_AL_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_AL_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_AL_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_AL_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_AL_YEA_MASK;

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

	dev_notice(rtc->rtc_dev->dev.parent,
		"read al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		 tm->tm_year + RTC_BASE_YEAR, tm->tm_mon + 1, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	return 0;
err_exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

static int mtk_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];
	ktime_t target;

	if (alm->enabled == 1) {
		/* Add one more second to postpone wake time. */
		target = rtc_tm_to_ktime(*tm);
		target = ktime_add_ns(target, NSEC_PER_SEC);
		*tm = rtc_ktime_to_tm(target);
	}

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	dev_notice(rtc->rtc_dev->dev.parent,
		"set al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		  tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	mutex_lock(&rtc->lock);

	switch (alm->enabled) {
	case 3:
		/* enable power-on alarm with logo */
		mtk_rtc_save_pwron_time(rtc, true, tm);
		break;
	case 4:
		/* disable power-on alarm */
		mtk_rtc_save_pwron_time(rtc, false, tm);
		break;
	default:
		break;
	}

	ret = regmap_update_bits(rtc->regmap,
			rtc->addr_base + RTC_PDN2, RTC_PDN2_PWRON_ALARM, 0);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	data[RTC_OFFSET_SEC] = ((data[RTC_OFFSET_SEC] & ~(RTC_AL_SEC_MASK)) |
				(tm->tm_sec & RTC_AL_SEC_MASK));
	data[RTC_OFFSET_MIN] = ((data[RTC_OFFSET_MIN] & ~(RTC_AL_MIN_MASK)) |
				(tm->tm_min & RTC_AL_MIN_MASK));
	data[RTC_OFFSET_HOUR] = ((data[RTC_OFFSET_HOUR] & ~(RTC_AL_HOU_MASK)) |
				(tm->tm_hour & RTC_AL_HOU_MASK));
	data[RTC_OFFSET_DOM] = ((data[RTC_OFFSET_DOM] & ~(RTC_AL_DOM_MASK)) |
				(tm->tm_mday & RTC_AL_DOM_MASK));
	data[RTC_OFFSET_MTH] = ((data[RTC_OFFSET_MTH] & ~(RTC_AL_MTH_MASK)) |
				(tm->tm_mon & RTC_AL_MTH_MASK));
	data[RTC_OFFSET_YEAR] = ((data[RTC_OFFSET_YEAR] & ~(RTC_AL_YEA_MASK)) |
				(tm->tm_year & RTC_AL_YEA_MASK));

	if (alm->enabled) {
		ret = regmap_bulk_write(rtc->regmap,
					rtc->addr_base + RTC_AL_SEC,
					data, RTC_OFFSET_COUNT);
		if (ret < 0)
			goto exit;
		ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_AL_MASK,
				   RTC_AL_MASK_DOW);
		if (ret < 0)
			goto exit;
		ret = regmap_update_bits(rtc->regmap,
					 rtc->addr_base + RTC_IRQ_EN,
					 RTC_IRQ_EN_ONESHOT_AL,
					 RTC_IRQ_EN_ONESHOT_AL);
		if (ret < 0)
			goto exit;
	} else {
		ret = regmap_update_bits(rtc->regmap,
					 rtc->addr_base + RTC_IRQ_EN,
					 RTC_IRQ_EN_ONESHOT_AL, 0);
		if (ret < 0)
			goto exit;
	}

	/* All alarm time register write to hardware after calling
	 * mtk_rtc_write_trigger. This can avoid race condition if alarm
	 * occur happen during writing alarm time register.
	 */
	ret = mtk_rtc_write_trigger(rtc);
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

int alarm_set_power_on(struct device *dev, struct rtc_wkalrm *alm)
{
	int err = 0;
	struct rtc_time tm;
	time64_t now, scheduled;

	err = rtc_valid_tm(&alm->time);
	if (err != 0)
		return err;
	scheduled = rtc_tm_to_time64(&alm->time);

	err = mtk_rtc_read_time(dev, &tm);
	if (err != 0)
		return err;
	now = rtc_tm_to_time64(&tm);

	if (scheduled <= now)
		alm->enabled = 4;
	else
		alm->enabled = 3;

	mtk_rtc_set_alarm(dev, alm);

	return err;
}

static int mtk_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *) arg;
	int err = 0;
	struct rtc_wkalrm alm;

	switch (cmd) {
	case RTC_POFF_ALM_SET:
		if (copy_from_user(&alm.time, uarg, sizeof(alm.time)))
			return -EFAULT;
		err = alarm_set_power_on(dev, &alm);
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct rtc_class_ops mtk_rtc_ops = {
	.ioctl      = mtk_rtc_ioctl,
	.read_time  = mtk_rtc_read_time,
	.set_time   = mtk_rtc_set_time,
	.read_alarm = mtk_rtc_read_alarm,
	.set_alarm  = mtk_rtc_set_alarm,
};

static int mtk_rtc_set_spare(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	struct reg_field tmp[SPARE_RG_MAX];
	int i, ret;
	struct nvmem_config nvmem_cfg = {
		.name = "mtk_rtc_nvmem",
		.word_size = SPARE_REG_WIDTH,
		.stride = 1,
		.size = SPARE_RG_MAX * SPARE_REG_WIDTH,
		.reg_read = rtc_nvram_read,
		.reg_write = rtc_nvram_write,
		.priv = dev,
	};

	memcpy(tmp, rtc->data->spare_reg_fields, sizeof(tmp));

	for (i = 0; i < SPARE_RG_MAX; i++) {
		tmp[i].reg += rtc->addr_base;
		rtc->spare[i] = devm_regmap_field_alloc(rtc->rtc_dev->dev.parent,
							rtc->regmap,
							tmp[i]);
		if (IS_ERR(rtc->spare[i])) {
			dev_err(rtc->rtc_dev->dev.parent,
					"spare regmap field[%d] err= %ld\n",
					i, PTR_ERR(rtc->spare[i]));
			return PTR_ERR(rtc->spare[i]);
		}
	}

	ret = rtc_nvmem_register(rtc->rtc_dev, &nvmem_cfg);
	if (ret)
		dev_err(rtc->rtc_dev->dev.parent, "nvmem register failed\n");

	return ret;
}

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_rtc *rtc;
	int ret;
#ifdef SUPPORT_PWR_OFF_ALARM
	struct device_node *of_chosen = NULL;
	struct tag_bootmode *tag = NULL;
#endif

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6397_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	rtc->addr_base = res->start;

	rtc->data = of_device_get_match_data(&pdev->dev);

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0)
		return rtc->irq;

	rtc->regmap = mt6397_chip->regmap;
	mutex_init(&rtc->lock);

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc_dev))
		return PTR_ERR(rtc->rtc_dev);

#ifdef SUPPORT_PWR_OFF_ALARM
	mt6397_rtc_suspend_lock =
		wakeup_source_register(NULL, "mt6397-rtc suspend wakelock");

	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		of_chosen = of_find_node_by_path("/chosen@0");

	if (of_chosen) {
		tag = (struct tag_bootmode *)of_get_property(
			of_chosen, "atag,boot", NULL);
		if (!tag)
			dev_err(&pdev->dev,
			"%s: failed to get atag,boot\n", __func__);
		else {
			dev_notice(&pdev->dev,
				"%s, bootmode:%d\n", __func__, tag->bootmode);
			bootmode = tag->bootmode;
		}
	} else
		dev_err(&pdev->dev,
			"%s: failed to get /chosen and /chosen@0\n", __func__);

#if IS_ENABLED(CONFIG_PM)
	rtc->pm_nb.notifier_call = rtc_pm_event;
	rtc->pm_nb.priority = 0;
	if (register_pm_notifier(&rtc->pm_nb))
		dev_err(&pdev->dev, "rtc pm faile\n");
	else
		rtc_pm_notifier_registered = true;
#endif /* CONFIG_PM */

	INIT_WORK(&rtc->work, mtk_rtc_work_queue);
#endif

	ret = devm_request_threaded_irq(&pdev->dev, rtc->irq, NULL,
					mtk_rtc_irq_handler_thread,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"mt6397-rtc", rtc);

	if (ret) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		return ret;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc_dev->ops = &mtk_rtc_ops;

	if (rtc->data->spare_reg_fields)
		if (mtk_rtc_set_spare(&pdev->dev))
			dev_err(&pdev->dev, "spare is not supported\n");

#ifdef SUPPORT_EOSC_CALI
	if (rtc->data->cali_reg_fields)
		if (mtk_rtc_config_eosc_cali(&pdev->dev))
			dev_err(&pdev->dev, "config eosc cali failed\n");

#endif

	return rtc_register_device(rtc->rtc_dev);
}

static void mtk_rtc_shutdown(struct platform_device *pdev)
{

#ifdef SUPPORT_EOSC_CALI
	mtk_rtc_enable_k_eosc(&pdev->dev);
#endif
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int mt6397_rtc_suspend(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(rtc->irq);

	return 0;
}

static int mt6397_rtc_resume(struct device *dev)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(rtc->irq);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mt6397_pm_ops, mt6397_rtc_suspend,
			mt6397_rtc_resume);

static const struct mtk_rtc_data mt6358_rtc_data = {
	.wrtgr = RTC_WRTGR_MT6358,
	.spare_reg_fields = mtk_rtc_spare_reg_fields,
};

static const struct mtk_rtc_data mt6397_rtc_data = {
	.wrtgr = RTC_WRTGR_MT6397,
};

static const struct mtk_rtc_data mt6359p_rtc_data = {
	.wrtgr = RTC_WRTGR_MT6358,
	.spare_reg_fields	= mtk_rtc_spare_reg_fields,
#ifdef SUPPORT_EOSC_CALI
	.cali_reg_fields	= mt6359_cali_reg_fields,
	.eosc_cali_version	= EOSC_CALI_MT6359P_SERIES,
#endif
};

static const struct of_device_id mt6397_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6323-rtc", .data = &mt6397_rtc_data },
	{ .compatible = "mediatek,mt6358-rtc", .data = &mt6358_rtc_data },
	{ .compatible = "mediatek,mt6359p-rtc", .data = &mt6359p_rtc_data },
	{ .compatible = "mediatek,mt6397-rtc", .data = &mt6397_rtc_data },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6397_rtc_of_match);

static struct platform_driver mtk_rtc_driver = {
	.driver = {
		.name = "mt6397-rtc",
		.of_match_table = mt6397_rtc_of_match,
		.pm = &mt6397_pm_ops,
	},
	.probe	= mtk_rtc_probe,
	.shutdown = mtk_rtc_shutdown,
};

module_platform_driver(mtk_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6397 PMIC");

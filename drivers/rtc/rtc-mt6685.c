// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Author: Amber Lin <Mw.lin@mediatek.com>
 */

#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/mfd/mt6685/rtc.h>
#include <linux/mfd/mt6685/core.h>
#include <linux/nvmem-provider.h>
#include <linux/sched/clock.h>
#include <linux/spmi.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

#ifdef SUPPORT_EOSC_CALI
#include <linux/mfd/mt6685/registers.h>
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
static int rtc_show_alarm;

module_param(rtc_show_time, int, 0644);
module_param(rtc_show_alarm, int, 0644);


static int mtk_rtc_write_trigger(struct mt6685_rtc *rtc);

static int counter;

void power_on_mclk(struct mt6685_rtc *rtc)
{
	mutex_lock(&rtc->clk_lock);
	/*Write Protection Key to unlock TOP_CKPDN_CON0*/
	regmap_write(rtc->regmap, TOP_DIG_WPK, 0x15);
	regmap_write(rtc->regmap, TOP_DIG_WPK_H, 0x63);
	/*Power on RTC MCLK before write RTC register*/
	regmap_write(rtc->regmap, RG_RTC_MCLK_PDN_CLR, RG_RTC_MCLK_PDN_MASK);
	counter++;
	mdelay(1);
	mutex_unlock(&rtc->clk_lock);
}

static void power_down_mclk(struct mt6685_rtc *rtc)
{
	mutex_lock(&rtc->clk_lock);
	counter--;
	if (counter < 0) {
		//dump_stack();
		pr_info("mclk_counter[%d]\n", counter);
	}
	if (counter == 0) {
		/*Write Protection Key to unlock TOP_CKPDN_CON0*/
		regmap_write(rtc->regmap, TOP_DIG_WPK, 0x15);
		regmap_write(rtc->regmap, TOP_DIG_WPK_H, 0x63);
		/*Power down RTC MCLK after write RTC register*/
		regmap_write(rtc->regmap, RG_RTC_MCLK_PDN_SET, RG_RTC_MCLK_PDN_MASK);
		mdelay(1);
	}
	mutex_unlock(&rtc->clk_lock);
}


static int rtc_bulk_read(struct mt6685_rtc *rtc, unsigned int reg,
				   void *val, size_t val_count)
{
	int ret;

	ret = regmap_bulk_read(rtc->regmap, reg, val, val_count);
	return ret;
}

static int rtc_read(struct mt6685_rtc *rtc, unsigned int reg,
			      unsigned int *val)
{
	rtc_bulk_read(rtc, reg, val, 2);
	return 0;
}

static int rtc_bulk_write(struct mt6685_rtc *rtc, unsigned int reg,
				    const void *val, size_t val_count)
{
	int ret;

	ret = regmap_bulk_write(rtc->regmap, reg, val, val_count);

	return ret;
}

static int rtc_write(struct mt6685_rtc *rtc, unsigned int reg,
			       unsigned int val)
{
	rtc_bulk_write(rtc, reg, &val, 2);
	return 0;
}

static int rtc_update_bits(struct mt6685_rtc *rtc, unsigned int reg,
					  unsigned int mask, unsigned int val)
{
	int ret;
	unsigned int tmp, orig = 0;

	ret = rtc_read(rtc, reg, &orig);
	if (ret != 0)
		return ret;
	tmp = orig & ~mask;
	tmp |= val & mask;
	ret = rtc_write(rtc, reg, tmp);
	return ret;
}

static unsigned int rtc_spare_reg[SPARE_RG_MAX][3] = {
	{RTC_RG_FG2, 0xff, 0},
	{RTC_RG_FG3, 0xff, 0},
	{RTC_SPAR0, 0xff, 0},
#ifdef SUPPORT_PWR_OFF_ALARM
	{RTC_PDN1_H, 0x1, 6},
#endif
};

static int rtc_field_read(struct mt6685_rtc *rtc, enum mtk_rtc_spare_enum cmd)
{
	unsigned int tmp_val = 0;
	int ret;

	if (cmd < SPARE_RG_MAX) {

		ret = rtc_read(rtc, rtc->addr_base + rtc_spare_reg[cmd][RTC_REG], &tmp_val);
		if (ret < 0)
			return ret;
		tmp_val = (tmp_val >> rtc_spare_reg[cmd][RTC_SHIFT]) &
		    rtc_spare_reg[cmd][RTC_MASK];

		pr_info("%s: cmd[%d], get rg[0x%x, 0x%x , %d] = 0x%x\n",
				__func__, cmd,
				rtc_spare_reg[cmd][RTC_REG],
				rtc_spare_reg[cmd][RTC_MASK],
				rtc_spare_reg[cmd][RTC_SHIFT], tmp_val);
		return tmp_val;
	}
	return 0;
}

static int rtc_spare_field_write(struct mt6685_rtc *rtc,
				enum mtk_rtc_spare_enum cmd, unsigned int val)
{
	unsigned int tmp_val = 0;

	if (cmd < SPARE_RG_MAX) {
		pr_info("%s: cmd[%d], set rg[0x%x, 0x%x , %d] = 0x%x\n",
				__func__, cmd,
				rtc_spare_reg[cmd][RTC_REG],
				rtc_spare_reg[cmd][RTC_MASK],
				rtc_spare_reg[cmd][RTC_SHIFT], val);

		rtc_read(rtc, rtc->addr_base + rtc_spare_reg[cmd][RTC_REG], &tmp_val);

		tmp_val = tmp_val & ~(rtc_spare_reg[cmd][RTC_MASK] <<
				rtc_spare_reg[cmd][RTC_SHIFT]);

		if (rtc->data->unlock_version == UNLOCK_MT6685_SERIES) {
			if (rtc_spare_reg[cmd][RTC_REG] == RTC_RG_FG2) {
				rtc_write(rtc, rtc->addr_base + RTC_RG_FG2, 0xaf);
				rtc_write(rtc, rtc->addr_base + RTC_RG_FG2, 0xaf);
				rtc_write(rtc, rtc->addr_base + RTC_RG_FG2, 0x5e);
			}
			if (rtc_spare_reg[cmd][RTC_REG] == RTC_RG_FG3) {
				rtc_write(rtc, rtc->addr_base + RTC_RG_FG3, 0x66);
				rtc_write(rtc, rtc->addr_base + RTC_RG_FG3, 0x66);
				rtc_write(rtc, rtc->addr_base + RTC_RG_FG3, 0xf1);
			}
		}

		rtc_write(rtc, rtc->addr_base + rtc_spare_reg[cmd][RTC_REG],
					tmp_val | ((val & rtc_spare_reg[cmd][RTC_MASK]) <<
						rtc_spare_reg[cmd][RTC_SHIFT]));
	}
	return 0;
}

static u16 rtc_pwron_reg[RTC_OFFSET_COUNT][3] = {
	{RTC_PWRON_SEC, RTC_PWRON_SEC_MASK, RTC_PWRON_SEC_SHIFT},
	{RTC_PWRON_MIN, RTC_PWRON_MIN_MASK, RTC_PWRON_MIN_SHIFT},
	{RTC_PWRON_HOU, RTC_PWRON_HOU_MASK, RTC_PWRON_HOU_SHIFT},
	{RTC_PWRON_DOM, RTC_PWRON_DOM_MASK, RTC_PWRON_DOM_SHIFT},
	{0, 0, 0},
	{RTC_PWRON_MTH, RTC_PWRON_MTH_MASK, RTC_PWRON_MTH_SHIFT},
	{RTC_PWRON_YEA, RTC_PWRON_YEA_MASK, RTC_PWRON_YEA_SHIFT},
};



#ifdef SUPPORT_EOSC_CALI

static int rtc_eosc_cali_td;
module_param(rtc_eosc_cali_td, int, 0644);


static void mtk_rtc_enable_k_eosc(struct device *dev)
{
	struct mt6685_rtc *rtc = dev_get_drvdata(dev);
	u32 td;

	power_on_mclk(rtc);

	if (!rtc->cali_is_supported) {
		power_down_mclk(rtc);
		return;
	}

	/* Truning on eosc cali mode clock */
	rtc_update_bits(rtc, TOP_RTC_EOSC32_CK_PDN, TOP_RTC_EOSC32_CK_PDN_MASK, 0);

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

		rtc_update_bits(rtc, rtc->addr_base + EOSC_CALI_TD, EOSC_CALI_TD_MASK, td);
		mtk_rtc_write_trigger(rtc);
	}
	power_down_mclk(rtc);
}
#endif

#ifdef SUPPORT_PWR_OFF_ALARM

static u32 bootmode = NORMAL_BOOT;
static struct wakeup_source *mt6685_rtc_suspend_lock;
static bool rtc_pm_notifier_registered;
static bool kpoc_alarm;
static unsigned long rtc_pm_status;


#if IS_ENABLED(CONFIG_PM)

#define PM_DUMMY 0xFFFF

static int rtc_pm_event(struct notifier_block *notifier, unsigned long pm_event,
			void *unused)
{
	struct mt6685_rtc *rtc = container_of(notifier,
		struct mt6685_rtc, pm_nb);

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

static void rtc_mark_kpoc(struct mt6685_rtc *rtc)
{
	power_on_mclk(rtc);
	mutex_lock(&rtc->lock);
	rtc_spare_field_write(rtc, SPARE_KPOC, 1);
	mtk_rtc_write_trigger(rtc);
	mutex_unlock(&rtc->lock);
	power_down_mclk(rtc);
}

static void mtk_rtc_work_queue(struct work_struct *work)
{
	struct mt6685_rtc *rtc = container_of(work, struct mt6685_rtc, work);
	unsigned long ret;
	unsigned int msecs;

	ret = wait_for_completion_timeout(&rtc->comp, msecs_to_jiffies(30000));
	if (!ret) {
		dev_notice(rtc->rtc_dev->dev.parent, "%s timeout\n", __func__);
		WARN_ON(1);
	} else {
		msecs = jiffies_to_msecs(ret);
		dev_notice(rtc->rtc_dev->dev.parent,
				"%s timeleft= %d\n", __func__, msecs);
		rtc_mark_kpoc(rtc);
		kernel_restart("kpoc");
	}
}

static void mtk_rtc_reboot(struct mt6685_rtc *rtc)
{
	__pm_stay_awake(mt6685_rtc_suspend_lock);

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

static void mtk_rtc_update_pwron_alarm_flag(struct mt6685_rtc *rtc)
{
	int ret;

	power_on_mclk(rtc);

	dev_notice(rtc->rtc_dev->dev.parent, "%s\n", __func__);

	ret = rtc_update_bits(rtc,
				rtc->addr_base + RTC_PDN1,
				RTC_PDN1_PWRON_TIME, 0);
	if (ret < 0)
		goto exit;

	ret =  rtc_update_bits(rtc,
				rtc->addr_base + RTC_PDN2,
				RTC_PDN2_PWRON_ALARM, RTC_PDN2_PWRON_ALARM);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	power_down_mclk(rtc);
	return;

exit:
	power_down_mclk(rtc);
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
}

static int mtk_rtc_restore_alarm(struct mt6685_rtc *rtc, struct rtc_time *tm)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	power_on_mclk(rtc);

	ret = rtc_bulk_read(rtc, rtc->addr_base + RTC_AL_SEC,
			    data, RTC_OFFSET_COUNT * 2);
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

	ret = rtc_bulk_write(rtc, rtc->addr_base + RTC_AL_SEC,
				data, RTC_OFFSET_COUNT * 2);
	if (ret < 0)
		goto exit;

	ret = rtc_write(rtc, rtc->addr_base + RTC_AL_MASK,
				RTC_AL_MASK_DOW);
	if (ret < 0)
		goto exit;

	ret =  rtc_update_bits(rtc,
				rtc->addr_base + RTC_IRQ_EN,
				RTC_IRQ_EN_AL, RTC_IRQ_EN_AL);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	power_down_mclk(rtc);
	return ret;

exit:
	power_down_mclk(rtc);
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
	return ret;
}

bool mtk_rtc_is_pwron_alarm(struct mt6685_rtc *rtc,
	struct rtc_time *nowtm, struct rtc_time *tm)
{
	u32 pdn1 = 0, spar1 = 0, pdn2 = 0, spar0 = 0;
	int ret, sec = 0;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	ret = rtc_read(rtc, rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	dev_notice(rtc->rtc_dev->dev.parent, "pdn1 = 0x%x\n", pdn1);

	if (pdn1 & RTC_PDN1_PWRON_TIME) {/* power-on time is available */

		/*get current rtc time*/
		do {
			ret = rtc_bulk_read(rtc,
						rtc->addr_base + RTC_TC_SEC,
						data, RTC_OFFSET_COUNT * 2);
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

			ret = rtc_read(rtc,
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
		ret = rtc_read(rtc,
				rtc->addr_base + RTC_SPAR0, &spar0);
		if (ret < 0)
			goto exit;

		ret = rtc_read(rtc,
					rtc->addr_base + RTC_SPAR1, &spar1);
		if (ret < 0)
			goto exit;

		ret = rtc_read(rtc,
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

static int mtk_rtc_write_trigger(struct mt6685_rtc *rtc)
{
	unsigned long timeout = jiffies + HZ;
	int ret;
	u32 data = 0;

	ret = rtc_write(rtc,
			rtc->addr_base + rtc->data->wrtgr, 1);
	if (ret < 0)
		return ret;

	while (1) {
		ret = rtc_read(rtc, rtc->addr_base + RTC_BBPU,
				  &data);
		if (ret < 0)
			break;
		if (!(data & RTC_BBPU_CBUSY))
			break;
		if (time_after(jiffies, timeout)) {
			ret = -ETIMEDOUT;
			break;
		}
		cpu_relax();
	}
	return ret;
}



static void mtk_rtc_reset_bbpu_alarm_status(struct mt6685_rtc *rtc)
{
	u32 bbpu;
	int ret;

	power_on_mclk(rtc);

	bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN | RTC_BBPU_RESET_AL;
	ret = rtc_write(rtc, rtc->addr_base + RTC_BBPU, bbpu);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	power_down_mclk(rtc);
	return;
exit:
	power_down_mclk(rtc);
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);

}

static int mtk_rtc_is_alarm_irq(struct mt6685_rtc *rtc)
{
	u32 irqsta = 0, bbpu = 0, sck = 0;
	int ret;

	power_on_mclk(rtc);

	ret = rtc_read(rtc, rtc->addr_base + RTC_IRQ_STA, &irqsta);/* read clear */

	/*clear SCK_TOP rtc interrupt*/
	rtc_read(rtc, SCK_TOP_INT_STATUS0, &sck);
	rtc_write(rtc, SCK_TOP_INT_STATUS0, sck);

	if ((ret == 0) && (irqsta & RTC_IRQ_STA_AL)) {
		bbpu = RTC_BBPU_KEY | RTC_BBPU_PWREN;
		ret = rtc_write(rtc,
					rtc->addr_base + RTC_BBPU, bbpu);
		if (ret < 0)
			dev_err(rtc->rtc_dev->dev.parent,
				"%s: %d error\n", __func__, __LINE__);

		ret =  rtc_update_bits(rtc,
				rtc->addr_base + RTC_IRQ_EN,
				RTC_IRQ_EN_AL, 0);
		if (ret < 0)
			dev_err(rtc->rtc_dev->dev.parent,
				"%s: %d error\n", __func__, __LINE__);
		mtk_rtc_write_trigger(rtc);
		power_down_mclk(rtc);
		return RTC_ALSTA;
	}
	power_down_mclk(rtc);
	return RTC_NONE;
}

static irqreturn_t mtk_rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6685_rtc *rtc = data;
	bool pwron_alm = false;
	int status = RTC_NONE;
#ifdef SUPPORT_PWR_OFF_ALARM
	bool pwron_alarm = false;
	struct rtc_time nowtm, tm;
#endif

	mutex_lock(&rtc->lock);

	status = mtk_rtc_is_alarm_irq(rtc);

	if (rtc->rtc_dev == NULL) {
		mutex_unlock(&rtc->lock);
		return IRQ_NONE;
	}

	dev_notice(rtc->rtc_dev->dev.parent, "%s:%d\n", __func__, status);

	if (status == RTC_NONE) {
		mutex_unlock(&rtc->lock);
		return IRQ_NONE;
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

static int __mtk_rtc_read_time(struct mt6685_rtc *rtc,
			       struct rtc_time *tm, int *sec)
{
	int ret;
	unsigned int reload = 0;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	power_on_mclk(rtc);

	rtc_read(rtc, rtc->addr_base + RTC_BBPU, &reload);
	reload = reload | RTC_BBPU_KEY | RTC_BBPU_RELOAD;
	rtc_write(rtc, rtc->addr_base + RTC_BBPU, reload);
	mtk_rtc_write_trigger(rtc);
	power_down_mclk(rtc);

	mutex_lock(&rtc->lock);
	ret = rtc_bulk_read(rtc, rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT * 2);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC] & RTC_TC_SEC_MASK;
	tm->tm_min = data[RTC_OFFSET_MIN] & RTC_TC_MIN_MASK;
	tm->tm_hour = data[RTC_OFFSET_HOUR] & RTC_TC_HOU_MASK;
	tm->tm_mday = data[RTC_OFFSET_DOM] & RTC_TC_DOM_MASK;
	tm->tm_mon = data[RTC_OFFSET_MTH] & RTC_TC_MTH_MASK;
	tm->tm_year = data[RTC_OFFSET_YEAR] & RTC_TC_YEA_MASK;

	ret = rtc_read(rtc, rtc->addr_base + RTC_TC_SEC, sec);
	*sec &= RTC_TC_SEC_MASK;
exit:
	mutex_unlock(&rtc->lock);
	return ret;

}

static void mtk_rtc_set_pwron_time(struct mt6685_rtc *rtc, struct rtc_time *tm)
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
		ret =  rtc_update_bits(rtc,
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

void mtk_rtc_save_pwron_time(struct mt6685_rtc *rtc,
	bool enable, struct rtc_time *tm)
{
	u32 pdn1 = 0;
	int ret;

	/* set power on time */
	mtk_rtc_set_pwron_time(rtc, tm);

	/* update power on alarm related flags */
	if (enable)
		pdn1 = RTC_PDN1_PWRON_TIME;
	ret =  rtc_update_bits(rtc,
				rtc->addr_base + RTC_PDN1,
				RTC_PDN1_PWRON_TIME, pdn1);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	return;

exit:
	dev_err(rtc->rtc_dev->dev.parent, "%s error\n", __func__);
	return;
}

static int mtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	time64_t time;
	struct mt6685_rtc *rtc = dev_get_drvdata(dev);
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
	tm->tm_year += RTC_MIN_YEAR_OFFSET;//+110

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

static bool mtk_rtc_check_set_time(struct mt6685_rtc *rtc, struct rtc_time *tm,
	int retry_time, int rtc_time_reg)
{
	int ret, i, j, write_fail = 0, prot_key = 0, hwid = 0, mclk = 0;
	u16 data[RTC_OFFSET_COUNT], latest[RTC_OFFSET_COUNT];

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	for (j = 1; j <= retry_time; j++) {

		write_fail = 0;

		ret = rtc_bulk_read(rtc, rtc->addr_base + rtc_time_reg,
				       latest, RTC_OFFSET_COUNT * 2);
		if (ret < 0)
			return ret;

		for (i = 0; i < RTC_OFFSET_COUNT; i++) {
			if (i == RTC_OFFSET_DOW)
				continue;

			latest[i] = latest[i] & rtc_time_mask[i];
			if (latest[i] != data[i])
				write_fail++;

			if (j == retry_time) {
				ret = rtc_read(rtc, MT6685_HWCID_L, &hwid);
				if (ret < 0)
					return ret;

				ret = rtc_read(rtc, RG_RTC_MCLK_PDN, &mclk);
				if (ret < 0)
					return ret;
				mclk = mclk >> RG_RTC_MCLK_PDN_STA_SHIFT & RG_RTC_MCLK_PDN_STA_MASK;

				ret = rtc_read(rtc, rtc->addr_base + RTC_SPAR_MACRO, &prot_key);
				if (ret < 0)
					return ret;
				prot_key = prot_key >> SPAR_PROT_STAT_SHIFT & SPAR_PROT_STAT_MASK;

				dev_info(rtc->rtc_dev->dev.parent,
				"[HWID 0x%x, MCLK 0x%x, prot key 0x%x] %s write %d, latest %d\n",
				hwid, mclk, prot_key, rtc_time_reg_name[i], data[i], latest[i]);
			}
		}

		if (write_fail > 0)
			mdelay(2);
		else
			break;
	}

	if (write_fail > 0)
		return false;

	return true;
}

static int mtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mt6685_rtc *rtc = dev_get_drvdata(dev);
	int ret, result = 0;
	u16 data[RTC_OFFSET_COUNT];

	power_on_mclk(rtc);

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
	ret = rtc_bulk_write(rtc, rtc->addr_base + RTC_TC_SEC,
				data, RTC_OFFSET_COUNT * 2);
	if (ret < 0)
		goto exit;

	/* Time register write to hardware after call trigger function */
	ret = mtk_rtc_write_trigger(rtc);
	if (ret < 0)
		goto exit;

	result = mtk_rtc_check_set_time(rtc, tm, 2, RTC_TC_SEC);

	if (!result) {
		dev_info(rtc->rtc_dev->dev.parent, "check rtc set time\n");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning("mt6685-rtc", "mt6685-rtc: set tick time failed\n");
#endif
	}

exit:
	mutex_unlock(&rtc->lock);
	power_down_mclk(rtc);
	return ret;
}

static int mtk_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct rtc_time *tm = &alm->time;
	struct mt6685_rtc *rtc = dev_get_drvdata(dev);
	u32 irqen = 0, pdn2 = 0;
	int ret;
	u16 data[RTC_OFFSET_COUNT] = { 0 };

	mutex_lock(&rtc->lock);
	ret = rtc_read(rtc, rtc->addr_base + RTC_IRQ_EN, &irqen);
	if (ret < 0)
		goto err_exit;
	ret = rtc_read(rtc, rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto err_exit;

	ret = rtc_bulk_read(rtc, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT * 2);
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
	struct mt6685_rtc *rtc = dev_get_drvdata(dev);
	int ret, result = 0;
	u16 data[RTC_OFFSET_COUNT];
	ktime_t target;

	power_on_mclk(rtc);

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

	ret = rtc_update_bits(rtc,
			rtc->addr_base + RTC_PDN2, RTC_PDN2_PWRON_ALARM, 0);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(rtc);

	ret = rtc_bulk_read(rtc, rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT * 2);
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
		ret = rtc_bulk_write(rtc,
					rtc->addr_base + RTC_AL_SEC,
					data, RTC_OFFSET_COUNT * 2);
		if (ret < 0)
			goto exit;
		ret = rtc_write(rtc, rtc->addr_base + RTC_AL_MASK,
				   RTC_AL_MASK_DOW);
		if (ret < 0)
			goto exit;

		ret =  rtc_update_bits(rtc,
				rtc->addr_base + RTC_IRQ_EN,
				RTC_IRQ_EN_AL, RTC_IRQ_EN_AL);
		if (ret < 0)
			goto exit;
		} else {
			ret =  rtc_update_bits(rtc,
					rtc->addr_base + RTC_IRQ_EN,
					RTC_IRQ_EN_AL, 0);
			if (ret < 0)
				goto exit;
		}

	/* All alarm time register write to hardware after calling
	 * mtk_rtc_write_trigger. This can avoid race condition if alarm
	 * occur happen during writing alarm time register.
	 */
	ret = mtk_rtc_write_trigger(rtc);
	if (ret < 0)
		goto exit;

	result = mtk_rtc_check_set_time(rtc, tm, 2, RTC_AL_SEC);

	if (!result) {
		dev_info(rtc->rtc_dev->dev.parent, "check rtc set alarm\n");
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning("mt6685-rtc", "mt6685-rtc: set alarm time failed\n");
#endif
	}
exit:
	mutex_unlock(&rtc->lock);
	power_down_mclk(rtc);
	return ret;
}

int rtc_alarm_set_power_on(struct device *dev, struct rtc_wkalrm *alm)
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
		err = rtc_alarm_set_power_on(dev, &alm);
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

int rtc_nvram_read(void *priv, unsigned int offset, void *val,
							size_t bytes)
{
	struct mt6685_rtc *rtc = dev_get_drvdata(priv);
	int ret;
	u8 *buf = val;

	mutex_lock(&rtc->lock);

	for (; bytes; bytes--) {
		ret = rtc_field_read(rtc, (SPARE_FG2 + offset++));
		if (ret < 0)
			goto out;
		else {
			*buf++ = (u8)ret;
			ret = 0;
		}
	}
out:
	mutex_unlock(&rtc->lock);
	return ret;
}

int rtc_nvram_write(void *priv, unsigned int offset, void *val,
							size_t bytes)
{
	struct mt6685_rtc *rtc = dev_get_drvdata(priv);
	int ival;
	int ret;
	u8 *buf = val;

	power_on_mclk(rtc);

	mutex_lock(&rtc->lock);

	for (; bytes; bytes--) {
		ival = *buf++;
		ret = rtc_spare_field_write(rtc, (SPARE_FG2 + offset++), ival);
		if (ret < 0)
			goto out;
	}
	mtk_rtc_write_trigger(rtc);
out:
	mutex_unlock(&rtc->lock);
	power_down_mclk(rtc);
	return ret;
}

static int mtk_rtc_set_spare(struct device *dev)
{
	struct mt6685_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	struct nvmem_config nvmem_cfg = {
		.name = "mtk_rtc_nvmem",
		.word_size = SPARE_REG_WIDTH,
		.stride = 1,
		.size = SPARE_RG_MAX * SPARE_REG_WIDTH,
		.reg_read = rtc_nvram_read,
		.reg_write = rtc_nvram_write,
		.priv = dev,
	};

	ret = rtc_nvmem_register(rtc->rtc_dev, &nvmem_cfg);
	if (ret)
		dev_err(rtc->rtc_dev->dev.parent, "nvmem register failed\n");

	return ret;
}

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct mt6685_rtc *rtc;
	struct device_node *np = pdev->dev.of_node;
	int ret;

#ifdef SUPPORT_PWR_OFF_ALARM
	struct device_node *of_chosen = NULL;
	struct tag_bootmode *tag = NULL;
#endif

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6685_rtc), GFP_KERNEL);
	if (!rtc) {
		//dev_err(&pdev->dev, "devm_kzalloc failed\n");
		return -ENOMEM;
	}

	rtc->data = of_device_get_match_data(&pdev->dev);
	if (!rtc->data) {
		dev_err(&pdev->dev, "of_device_get_match_data failed\n");
		return -ENODEV;
	}

	if (of_property_read_u32(pdev->dev.of_node, "base", &rtc->addr_base))
		rtc->addr_base = RTC_DSN_ID;

	pr_notice("%s: rtc->addr_base =0x%x\n", __func__, rtc->addr_base);

	mutex_init(&rtc->lock);
	mutex_init(&rtc->clk_lock);

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc_dev)) {
		dev_err(&pdev->dev, "Failed devm_rtc_allocate_device: %d\n", rtc->rtc_dev);
		return PTR_ERR(rtc->rtc_dev);
	}

	rtc->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!rtc->regmap) {
		pr_err("%s: get regmap failed\n", __func__);
		return -ENODEV;
	}

#ifdef SUPPORT_PWR_OFF_ALARM
	mt6685_rtc_suspend_lock =
		wakeup_source_register(NULL, "mt6685-rtc suspend wakelock");

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

	/* Obtain interrupt ID from DTS */
	rtc->irq = of_irq_get(np, 0);
	if (rtc->irq < 0) {
		dev_err(&pdev->dev, "Failed to get irq(%d)\n", rtc->irq);
		return rtc->irq;
	}

	ret = devm_request_threaded_irq(&pdev->dev, rtc->irq, NULL,
					mtk_rtc_irq_handler_thread,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					"mt6685-rtc", rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		return ret;
	}

	/*Enable SCK_TOP rtc interrupt*/
	rtc_update_bits(rtc, SCK_TOP_INT_CON0, EN_RTC_INTERRUPT, 1);

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc_dev->ops = &mtk_rtc_ops;

	if (mtk_rtc_set_spare(&pdev->dev))
		dev_err(&pdev->dev, "spare is not supported\n");

	ret = rtc_register_device(rtc->rtc_dev);
	if (ret) {
		dev_err(&pdev->dev, "register rtc device failed\n");
	};

	enable_irq_wake(rtc->irq);

#ifdef SUPPORT_EOSC_CALI
	rtc->cali_is_supported = true;
#endif

	power_on_mclk(rtc);
	power_down_mclk(rtc);
	return 0;
}

static void mtk_rtc_shutdown(struct platform_device *pdev)
{
	struct mt6685_rtc *rtc = dev_get_drvdata(&pdev->dev);

	/*Normal sequence power off when PON falling*/
	rtc_write(rtc, TOP2_ELR1, 1);

#ifdef SUPPORT_EOSC_CALI
	mtk_rtc_enable_k_eosc(&pdev->dev);
#endif
}

static const struct mtk_rtc_data mt6685_rtc_data = {
	.wrtgr = RTC_WRTGR_MT6685,
	.unlock_version = UNLOCK_MT6685_SERIES,
};

static const struct of_device_id mt6685_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6685-rtc", .data = &mt6685_rtc_data },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6685_rtc_of_match);

static struct platform_driver mtk_rtc_driver = {
	.driver = {
		.name = "mt6685-rtc",
		.of_match_table = mt6685_rtc_of_match,
	},
	.probe	= mtk_rtc_probe,
	.shutdown = mtk_rtc_shutdown,
};

module_platform_driver(mtk_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Mw Lin <Mw.Lin@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6685 Clock IC");

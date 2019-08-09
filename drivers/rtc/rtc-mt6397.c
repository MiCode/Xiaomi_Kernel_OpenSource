/*
* Copyright (c) 2014-2015 MediaTek Inc.
* Author: Tianping.Fang <tianping.fang@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/reboot.h>
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
#include "../misc/mediatek/include/mt-plat/mtk_boot_common.h"
#endif

#define RTC_BBPU		0x0000
#define RTC_BBPU_CBUSY		BIT(6)

#define RTC_WRTGR		0x003c

#define RTC_IRQ_STA		0x0002
#define RTC_IRQ_STA_AL		BIT(0)
#define RTC_IRQ_STA_LP		BIT(3)

#define RTC_IRQ_EN		0x0004
#define RTC_IRQ_EN_AL		BIT(0)
#define RTC_IRQ_EN_ONESHOT	BIT(2)
#define RTC_IRQ_EN_LP		BIT(3)
#define RTC_IRQ_EN_ONESHOT_AL	(RTC_IRQ_EN_ONESHOT | RTC_IRQ_EN_AL)

#define RTC_AL_MASK		0x0008
#define RTC_AL_MASK_DOW		BIT(4)

#define RTC_TC_SEC		0x000a
#define RTC_SPAR1		0x0032
#define RTC_AL_SEC		0x0018

/* Min, Hour, Dom... register offset to RTC_TC_SEC */
#define RTC_OFFSET_SEC		0
#define RTC_OFFSET_MIN		1
#define RTC_OFFSET_HOUR		2
#define RTC_OFFSET_DOM		3
#define RTC_OFFSET_DOW		4
#define RTC_OFFSET_MTH		5
#define RTC_OFFSET_YEAR		6
#define RTC_OFFSET_COUNT	7

#define RTC_AL_SEC		0x0018

#define RTC_PDN2		0x002e
#define RTC_PDN1		0x002c
#define RTC_SPAR0		0x0030
#define RTC_PDN2_PWRON_ALARM	BIT(4)
#define RTC_PDN1_PWRON_TIME      BIT(7)
#define RTC_PDN2_PWRON_LOGO     BIT(15)
#define RTC_BBPU_RELOAD			BIT(5)
#define RTC_BBPU_KEY			(0x43 << 8)
#define RTC_PWRON_YEA        RTC_PDN2
#define RTC_PWRON_YEA_MASK     0x7f00
#define RTC_PWRON_YEA_SHIFT     8

#define RTC_PWRON_MTH        RTC_PDN2
#define RTC_PWRON_MTH_MASK     0x000f
#define RTC_PWRON_MTH_SHIFT     0

#define RTC_PWRON_SEC        RTC_SPAR0
#define RTC_PWRON_SEC_MASK     0x003f
#define RTC_PWRON_SEC_SHIFT     0
#define RTC_SPAR0_ALARM_BOOT	BIT(8)


#define RTC_PWRON_MIN        RTC_SPAR1
#define RTC_PWRON_MIN_MASK     0x003f
#define RTC_PWRON_MIN_SHIFT     0

#define RTC_PWRON_HOU        RTC_SPAR1
#define RTC_PWRON_HOU_MASK     0x07c0
#define RTC_PWRON_HOU_SHIFT     6

#define RTC_PWRON_DOM        RTC_SPAR1
#define RTC_PWRON_DOM_MASK     0xf800
#define RTC_PWRON_DOM_SHIFT     11

#define RTC_MIN_YEAR		1968
#define RTC_BASE_YEAR		1900
#define RTC_NUM_YEARS		128
#define RTC_MIN_YEAR_OFFSET	(RTC_MIN_YEAR - RTC_BASE_YEAR)

struct mt6397_rtc {
	struct device		*dev;
	struct rtc_device	*rtc_dev;
	struct mutex		lock;
	struct regmap		*regmap;
	int			irq;
	u32			addr_base;
};

static struct mt6397_rtc *mt_rtc;

static int mtk_rtc_write_trigger(struct mt6397_rtc *rtc)
{
	unsigned long timeout = jiffies + HZ;
	int ret;
	u32 data;

	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_WRTGR, 1);
	if (ret < 0)
		return ret;

	while (1) {
		ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_BBPU,
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

static void _mtk_rtc_save_pwron_alarm(void)
{
	u32 pdn1, pdn2;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	pdn1 &= ~RTC_PDN1_PWRON_TIME;
	pdn2 |= RTC_PDN2_PWRON_ALARM;
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN1, pdn1);
	if (ret < 0)
		goto exit;

	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, pdn2);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);
	return;

exit:
	dev_dbg(mt_rtc->dev,
		"_mtk_rtc_save_pwron_alarm regmap write/read error!!!\n");
}

static void _mtk_rtc_set_alarm(struct rtc_time *tm)
{
	u16 data[RTC_OFFSET_COUNT];
	int ret;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	dev_notice(mt_rtc->dev, "set al time = %04d/%02d/%02d %02d:%02d:%02d\n",
		  tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		  tm->tm_hour, tm->tm_min, tm->tm_sec);

	ret = regmap_bulk_write(mt_rtc->regmap,
				mt_rtc->addr_base + RTC_AL_SEC,
				data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_AL_MASK,
				   RTC_AL_MASK_DOW);
	if (ret < 0)
		goto exit;
	ret = regmap_update_bits(mt_rtc->regmap,
				mt_rtc->addr_base + RTC_IRQ_EN,
				RTC_IRQ_EN_ONESHOT_AL,
				RTC_IRQ_EN_ONESHOT_AL);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	dev_dbg(mt_rtc->dev, "regmap write/read error!!!\n");
}

static void _rtc_get_tick(struct rtc_time *tm)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	ret = regmap_bulk_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];

	return;
exit:
	dev_dbg(mt_rtc->dev, "_rtc_get_tick regmap write/read error!!!\n");
}

static void _mtk_rtc_get_tick_time(struct rtc_time *tm)
{
	u32 bbpu, sec;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_BBPU, &bbpu);
	if (ret < 0)
		goto exit;

	bbpu |= RTC_BBPU_KEY | RTC_BBPU_RELOAD;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_BBPU, bbpu);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	_rtc_get_tick(tm);
	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_TC_SEC, &sec);
	if (ret < 0)
		goto exit;
	if (sec < tm->tm_sec) {	/* SEC has carried */
		_rtc_get_tick(tm);
	}

	return;

exit:
	dev_dbg(mt_rtc->dev,
		"_mtk_rtc_get_tick_time regmap write/read error!!!\n");
}

static void _mtk_rtc_get_pwron_alarm_time(struct rtc_time *tm)
{

	u32 spar1, pdn2, spar0;
	int ret;

	/*RTC_PWRON_SEC == SPAR0 */
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, &spar0);
	if (ret < 0)
		goto exit;
	/*RTC_PWRON_DOM == RTC_PWRON_HOU */
	/*== RTC_PWRON_MIN == RTC_SPAR1*/
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	/*RTC_PWRON_MTH == RTC_PWRON_YEAR== SPAR0 */
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;
	dev_notice(mt_rtc->dev, "spar0=0x%x, spar1=0x%x, pdn2=0x%x!!!\n",
			spar0, spar1, pdn2);

	tm->tm_sec = (spar0 & RTC_PWRON_SEC_MASK) >> RTC_PWRON_SEC_SHIFT;
	tm->tm_min = (spar1 & RTC_PWRON_MIN_MASK) >> RTC_PWRON_MIN_SHIFT;
	tm->tm_hour = (spar1 & RTC_PWRON_HOU_MASK) >> RTC_PWRON_HOU_SHIFT;
	tm->tm_mday = (spar1 & RTC_PWRON_DOM_MASK) >> RTC_PWRON_DOM_SHIFT;
	tm->tm_mon = (pdn2 & RTC_PWRON_MTH_MASK) >> RTC_PWRON_MTH_SHIFT;
	tm->tm_year = (pdn2 & RTC_PWRON_YEA_MASK) >> RTC_PWRON_YEA_SHIFT;
	dev_notice(mt_rtc->dev,
		"year=0x%x,mon=0x%x,mday =0x%x hou=0x%x,min=0x%x,sec=0x%x\n",
		tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	return;

exit:
	dev_dbg(mt_rtc->dev,
		"_mtk_rtc_get_pwron_alarm_time regmap write/read error!!!\n");
}

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
static void _mtk_rtc_set_alarm_boot(void)
{
	u32 spar0;
	int ret;

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, &spar0);
	if (ret < 0)
		goto exit;

	spar0 |= RTC_SPAR0_ALARM_BOOT;
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, spar0);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	dev_dbg(mt_rtc->dev, "regmap write/read error!!!\n");
}
#endif

static bool _mtk_rtc_is_pwron_alarm(struct rtc_time *nowtm,
						struct rtc_time *tm)
{
	u32 pdn1, sec;
	int ret;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	dev_notice(mt_rtc->dev, "pdn1 = 0x%x!!!\n", pdn1);
	/* power-on time is available */
	if (pdn1 & RTC_PDN1_PWRON_TIME) {
		_mtk_rtc_get_tick_time(nowtm);
		ret = regmap_read(mt_rtc->regmap,
				mt_rtc->addr_base + RTC_TC_SEC, &sec);
		if (ret < 0)
			goto exit;
		if (sec < nowtm->tm_sec) {	/* SEC has carried */
			_mtk_rtc_get_tick_time(nowtm);
		}
		_mtk_rtc_get_pwron_alarm_time(tm);
		return true;
	}
	return false;

exit:
	dev_dbg(mt_rtc->dev,
		"_mtk_rtc_is_pwron_alarm regmap write/read error!!!\n");
	return false;
}

#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
static bool _mtk_rtc_is_charging_boot(void)
{
	if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT
		|| get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT)
		return true;
	else
		return false;
}
#endif

static irqreturn_t mtk_rtc_irq_handler_thread(int irq, void *data)
{
	struct mt6397_rtc *rtc = data;
	u32 irqsta, irqen;
	int ret;
	bool pwron_alm = false, pwron_alarm = false;
	struct rtc_time nowtm;
	struct rtc_time tm = {0};

	mutex_lock(&rtc->lock);
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if ((ret >= 0) && (irqsta & RTC_IRQ_STA_AL)) {
		pwron_alarm = _mtk_rtc_is_pwron_alarm(&nowtm, &tm);
		nowtm.tm_year += RTC_MIN_YEAR;
		tm.tm_year += RTC_MIN_YEAR;
		dev_notice(mt_rtc->dev, "[RTC] nowtm = %d/%d/%d %d:%d:%d\n",
			nowtm.tm_year, nowtm.tm_mon, nowtm.tm_mday,
			nowtm.tm_hour, nowtm.tm_min, nowtm.tm_sec);

		if (pwron_alarm) {
			unsigned long now_time, time;

			now_time = mktime(nowtm.tm_year,
							nowtm.tm_mon,
							nowtm.tm_mday,
							nowtm.tm_hour,
							nowtm.tm_min,
							nowtm.tm_sec);
			time = mktime(tm.tm_year,
						tm.tm_mon,
						tm.tm_mday,
						tm.tm_hour,
						tm.tm_min,
						tm.tm_sec);
			/* power on */
			if (now_time >= time - 1 && now_time <= time + 4) {
				#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
				if (_mtk_rtc_is_charging_boot()) {
					dev_notice(mt_rtc->dev, "KPOC alarm!!!\n");
					time += 1;
					rtc_time_to_tm(time, &tm);
					tm.tm_year -= RTC_MIN_YEAR_OFFSET;
					tm.tm_mon += 1;
					/* tm.tm_sec += 1; */
					_mtk_rtc_set_alarm(&tm);
					_mtk_rtc_set_alarm_boot();
					mutex_unlock(&rtc->lock);
					machine_restart(NULL);
				} else {
					_mtk_rtc_save_pwron_alarm();
					pwron_alm = true;
				}
				#else
				_mtk_rtc_save_pwron_alarm();
				pwron_alm = true;
				#endif
			} else if (now_time < time) { /* set power-on alarm */
				tm.tm_year -= RTC_MIN_YEAR;
				dev_notice(mt_rtc->dev,
						"KPOC alarm again!!!\n");
				if (tm.tm_sec == 0) {
					tm.tm_sec = 59;
					tm.tm_min -= 1;
				} else {
					tm.tm_sec -= 1;
				}
			_mtk_rtc_set_alarm(&tm);
			}
		}
		rtc_update_irq(rtc->rtc_dev, 1, RTC_IRQF | RTC_AF);
		irqen = irqsta & ~RTC_IRQ_EN_AL;

		if (regmap_write(rtc->regmap, rtc->addr_base + RTC_IRQ_EN,
				 irqen) < 0)
			mtk_rtc_write_trigger(rtc);
		mutex_unlock(&rtc->lock);

		return IRQ_HANDLED;
	}
	mutex_unlock(&rtc->lock);
	return IRQ_NONE;
}

static int __mtk_rtc_read_time(struct mt6397_rtc *rtc,
			       struct rtc_time *tm, int *sec)
{
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	mutex_lock(&rtc->lock);
	ret = regmap_bulk_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_TC_SEC, sec);
exit:
	mutex_unlock(&rtc->lock);
	return ret;
}

void rtc_read_pwron_alarm(struct rtc_wkalrm *alm)
{
	struct rtc_time *tm;
	u32 pdn1, pdn2;
	int ret;
	u16 data[RTC_OFFSET_COUNT];

	if (alm == NULL)
		return;

	dev_notice(mt_rtc->dev, "rtc_read_pwron_alarm!!!\n");
	tm = &alm->time;
	mutex_lock(&mt_rtc->lock);
	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	ret = regmap_bulk_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_AL_SEC,
			       data, RTC_OFFSET_COUNT);
	if (ret < 0)
		goto exit;
	alm->enabled = (pdn1 & RTC_PDN1_PWRON_TIME ?
			(pdn2 & RTC_PDN2_PWRON_LOGO ? 3 : 2) : 0);
	/* return Power-On Alarm bit */
	alm->pending = !!(pdn2 & RTC_PDN2_PWRON_ALARM);

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];
	mutex_unlock(&mt_rtc->lock);
	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;
	dev_notice(mt_rtc->dev,
			"power-on = %04d/%02d/%02d %02d:%02d:%02d (%d)(%d)\n",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec,
			alm->enabled, alm->pending);
	return;

exit:
	mutex_unlock(&mt_rtc->lock);
	dev_dbg(mt_rtc->dev, "regmap write/read error!!!\n");
}

static void _rtc_set_pwron_alarm_time(struct rtc_time *tm)
{
	u32 spar1, pdn2, spar0;
	int ret;
	u32 tm_year, tm_mon, tm_mday, tm_hour, tm_min, tm_sec;

	dev_notice(mt_rtc->dev, "_rtc_save_pwron_time!!!\n");
	/*RTC_PWRON_YEAR == RTC_PWRON_MTH==PDN2 */
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	/*RTC_PWRON_DOM == RTC_PWRON_HOU */
	/*== RTC_PWRON_MIN == RTC_SPAR1*/
	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, &spar0);
	if (ret < 0)
		goto exit;

	tm_year = (tm->tm_year << RTC_PWRON_YEA_SHIFT) & RTC_PWRON_YEA_MASK;
	tm_mon = (tm->tm_mon << RTC_PWRON_MTH_SHIFT) & RTC_PWRON_MTH_MASK;
	tm_mday = (tm->tm_mday << RTC_PWRON_DOM_SHIFT) & RTC_PWRON_DOM_MASK;
	tm_hour = (tm->tm_hour << RTC_PWRON_HOU_SHIFT) & RTC_PWRON_HOU_MASK;
	tm_min = (tm->tm_min << RTC_PWRON_MIN_SHIFT) & RTC_PWRON_MIN_MASK;
	tm_sec = (tm->tm_sec << RTC_PWRON_SEC_SHIFT) & RTC_PWRON_SEC_MASK;

	tm_year |= pdn2 & ~(RTC_PWRON_YEA_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, tm_year);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;

	tm_mon |= pdn2 & ~(RTC_PWRON_MTH_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_PDN2, tm_mon);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	tm_mday |= spar1 & ~(RTC_PWRON_DOM_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, tm_mday);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	tm_hour |= spar1 & ~(RTC_PWRON_HOU_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, tm_hour);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	ret = regmap_read(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, &spar1);
	if (ret < 0)
		goto exit;

	tm_min |= spar1 & ~(RTC_PWRON_MIN_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR1, tm_min);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);


	tm_sec |= spar0 & ~(RTC_PWRON_SEC_MASK);
	ret = regmap_write(mt_rtc->regmap,
			mt_rtc->addr_base + RTC_SPAR0, tm_sec);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	mutex_unlock(&mt_rtc->lock);
	dev_dbg(mt_rtc->dev, "regmap write/read error!!!\n");

}

void mtk_rtc_save_pwron_time(bool enable,
					struct rtc_time *tm, bool logo)
{
	u32 pdn1, pdn2;
	int ret;

	dev_notice(mt_rtc->dev, "mtk_rtc_save_pwron_time!!!\n");
	_rtc_set_pwron_alarm_time(tm);

	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;
	ret = regmap_read(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, &pdn1);
	if (ret < 0)
		goto exit;

	if (logo)
		pdn2 |= RTC_PDN2_PWRON_LOGO;
	else
		pdn2 &= ~RTC_PDN2_PWRON_LOGO;

	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN2, pdn2);
	if (ret < 0)
		goto exit;

	if (enable)
		pdn1 |= RTC_PDN1_PWRON_TIME;
	else
		pdn1 &= ~RTC_PDN1_PWRON_TIME;
	ret = regmap_write(mt_rtc->regmap, mt_rtc->addr_base + RTC_PDN1, pdn1);
	if (ret < 0)
		goto exit;
	mtk_rtc_write_trigger(mt_rtc);

	return;

exit:
	mutex_unlock(&mt_rtc->lock);
	dev_dbg(mt_rtc->dev, "regmap write/read error!!!\n");

}

static int mtk_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	time64_t time;
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int days, sec, ret;

	do {
		ret = __mtk_rtc_read_time(rtc, tm, &sec);
		if (ret < 0)
			goto exit;
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

exit:
	return ret;
}

static int mtk_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mt6397_rtc *rtc = dev_get_drvdata(dev);
	int ret;
	u16 data[RTC_OFFSET_COUNT];

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
	u32 irqen, pdn2;
	int ret;
	u16 data[RTC_OFFSET_COUNT];

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

	tm->tm_sec = data[RTC_OFFSET_SEC];
	tm->tm_min = data[RTC_OFFSET_MIN];
	tm->tm_hour = data[RTC_OFFSET_HOUR];
	tm->tm_mday = data[RTC_OFFSET_DOM];
	tm->tm_mon = data[RTC_OFFSET_MTH];
	tm->tm_year = data[RTC_OFFSET_YEAR];

	tm->tm_year += RTC_MIN_YEAR_OFFSET;
	tm->tm_mon--;

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
	u32 irqsta, irqen, pdn2;

	tm->tm_year -= RTC_MIN_YEAR_OFFSET;
	tm->tm_mon++;

	data[RTC_OFFSET_SEC] = tm->tm_sec;
	data[RTC_OFFSET_MIN] = tm->tm_min;
	data[RTC_OFFSET_HOUR] = tm->tm_hour;
	data[RTC_OFFSET_DOM] = tm->tm_mday;
	data[RTC_OFFSET_MTH] = tm->tm_mon;
	data[RTC_OFFSET_YEAR] = tm->tm_year;

	dev_notice(rtc->dev,
		"set al time = %04d/%02d/%02d %02d:%02d:%02d (%d)\n",
		tm->tm_year + RTC_MIN_YEAR, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec, alm->enabled);

	mutex_lock(&rtc->lock);
	switch (alm->enabled) {
	case 2:
		/* enable power-on alarm */
		mtk_rtc_save_pwron_time(true, tm, false);
		break;
	case 3:
		/* enable power-on alarm with logo */
		mtk_rtc_save_pwron_time(true, tm, true);
		break;
	case 4:
		/* disable power-on alarm */
		mtk_rtc_save_pwron_time(false, tm, false);
		break;
	default:
		break;
	}

	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_EN, &irqen);
	if (ret < 0)
		goto exit;
	irqen &= ~RTC_IRQ_EN_AL;
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_PDN2, &pdn2);
	if (ret < 0)
		goto exit;
	pdn2 &= ~RTC_PDN2_PWRON_ALARM;

	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_IRQ_EN, irqen);
	if (ret < 0)
		goto exit;

	ret = regmap_write(rtc->regmap, rtc->addr_base + RTC_PDN2, pdn2);
	if (ret < 0)
		goto exit;

	mtk_rtc_write_trigger(rtc);
	ret = regmap_read(rtc->regmap, rtc->addr_base + RTC_IRQ_STA, &irqsta);
	if (ret < 0)
		goto exit;

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

static const struct rtc_class_ops mtk_rtc_ops = {
	.read_time  = mtk_rtc_read_time,
	.set_time   = mtk_rtc_set_time,
	.read_alarm = mtk_rtc_read_alarm,
	.set_alarm  = mtk_rtc_set_alarm,
};

static int mtk_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mt6397_chip *mt6397_chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6397_rtc *rtc;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct mt6397_rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->addr_base = res->start;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	rtc->irq = irq_create_mapping(mt6397_chip->irq_domain, res->start);
	if (rtc->irq <= 0)
		return -EINVAL;

	rtc->regmap = mt6397_chip->regmap;
	rtc->dev = &pdev->dev;
	mutex_init(&rtc->lock);

	mt_rtc = rtc;
	platform_set_drvdata(pdev, rtc);

	ret = request_threaded_irq(rtc->irq, NULL,
				   mtk_rtc_irq_handler_thread,
				   IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
				   "mt6397-rtc", rtc);
	if (ret) {
		dev_dbg(&pdev->dev, "Failed to request alarm IRQ: %d: %d\n",
			rtc->irq, ret);
		goto out_dispose_irq;
	}

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc_dev = rtc_device_register("mt6397-rtc", &pdev->dev,
					   &mtk_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		dev_dbg(&pdev->dev, "register rtc device failed\n");
		ret = PTR_ERR(rtc->rtc_dev);
		goto out_free_irq;
	}

	return 0;

out_free_irq:
	free_irq(rtc->irq, rtc->rtc_dev);
out_dispose_irq:
	irq_dispose_mapping(rtc->irq);
	return ret;
}

static int mtk_rtc_remove(struct platform_device *pdev)
{
	struct mt6397_rtc *rtc = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc->rtc_dev);
	free_irq(rtc->irq, rtc->rtc_dev);
	irq_dispose_mapping(rtc->irq);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
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

static const struct of_device_id mt6397_rtc_of_match[] = {
	{ .compatible = "mediatek,mt6397-rtc", },
	{ .compatible = "mediatek,mt6323-rtc", },
	{ .compatible = "mediatek,mt6392-rtc", },
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
	.remove = mtk_rtc_remove,
};

module_platform_driver(mtk_rtc_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Tianping Fang <tianping.fang@mediatek.com>");
MODULE_DESCRIPTION("RTC Driver for MediaTek MT6397 PMIC");
MODULE_ALIAS("platform:mt6397-rtc");

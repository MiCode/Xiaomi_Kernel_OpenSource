/*
 *  Real Time Clock driver for ams AS3720 PMICs
 *
 *  Copyright (C) 2012 ams AG.
 *
 *  Author: Bernhard Breinbauer <Bernhard.Breinbauer@ams.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/completion.h>
#include <linux/mfd/as3720.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#define AS3720_SET_ALM_RETRIES	5
#define AS3720_SET_TIME_RETRIES	5
#define AS3720_GET_TIME_RETRIES	5

/*
 * Read current time and date in RTC
 */
static int as3720_rtc_readtime(struct device *dev, struct rtc_time *tm)
{
	struct as3720 *as3720 = dev_get_drvdata(dev->parent);
	struct as3720_platform_data *pdata = as3720->dev->platform_data;
	u8 as_sec;
	u8 as_min_array[3];
	int as_min;
	long time, start_time;
	struct rtc_time start_tm;
	int ret;

	as3720_reg_read(as3720, AS3720_RTC_SECOND_REG, &as_sec);
	ret = as3720_block_read(as3720, AS3720_RTC_MINUTE1_REG,
				3, as_min_array);
	if (ret < 0) {
		dev_err(dev, "failed to read time with err:%d\n", ret);
		return ret;
	}
	as_min = (as_min_array[2] << 16)
		| (as_min_array[1] << 8)
		| (as_min_array[0]);
	time = as_min*60 + as_sec;
	start_tm.tm_year = (pdata->rtc_start_year - 1900);
	start_tm.tm_mon = 0;
	start_tm.tm_mday = 1;
	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	rtc_tm_to_time(&start_tm, &start_time);
	time = time + start_time;
	rtc_time_to_tm(time, tm);
	return 0;
}

/*
 * Set current time and date in RTC
 */
static int as3720_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	struct as3720 *as3720 = dev_get_drvdata(dev->parent);
	struct as3720_platform_data *pdata = as3720->dev->platform_data;
	long time, start_time;
	u8 as_sec;
	u8 as_min_array[3];
	int as_min;
	struct rtc_time start_tm;
	int ret;

	/* Write time to RTC */
	rtc_tm_to_time(tm, &time);
	start_tm.tm_year = (pdata->rtc_start_year - 1900);
	start_tm.tm_mon = 0;
	start_tm.tm_mday = 1;
	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	rtc_tm_to_time(&start_tm, &start_time);
	time = time - start_time;
	as_min = time / 60;
	as_sec = time % 60;
	as_min_array[2] = (as_min & 0xFF0000) >> 16;
	as_min_array[1] = (as_min & 0xFF00) >> 8;
	as_min_array[0] = as_min & 0xFF;
	as3720_reg_write(as3720, AS3720_RTC_SECOND_REG, as_sec);
	ret = as3720_block_write(as3720, AS3720_RTC_MINUTE1_REG, 3,
				 as_min_array);
	if (ret < 0) {
		dev_err(dev, "failed to set time with err:%d\n", ret);
		return ret;
	}
	return 0;
}

/*
 * Read alarm time and date in RTC
 */
static int as3720_rtc_readalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct as3720 *as3720 = dev_get_drvdata(dev->parent);
	struct as3720_platform_data *pdata = as3720->dev->platform_data;
	u8 as_sec;
	u8 as_min_array[3];
	int as_min;
	long time, start_time;
	struct rtc_time start_tm;
	int ret;

	as3720_reg_read(as3720, AS3720_RTC_ALARM_SECOND_REG, &as_sec);
	ret = as3720_block_read(as3720, AS3720_RTC_ALARM_MINUTE1_REG,
				3, as_min_array);
	if (ret < 0) {
		dev_err(dev, "failed to read alarm with err:%d\n", ret);
		return ret;
	}
	as_min = (as_min_array[2] << 16)
		| (as_min_array[1] << 8)
		| (as_min_array[0]);
	time = as_min*60 + as_sec;
	start_tm.tm_year = (pdata->rtc_start_year - 1900);
	start_tm.tm_mon = 0;
	start_tm.tm_mday = 1;
	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	rtc_tm_to_time(&start_tm, &start_time);
	time = time + start_time;
	rtc_time_to_tm(time, &alrm->time);
	return 0;
}

static int as3720_rtc_stop_alarm(struct as3720 *as3720)
{
	/* disable rtc alarm interrupt */
	return as3720_set_bits(as3720, AS3720_INTERRUPTMASK2_REG,
			AS3720_IRQ_RTC_ALARM, 1);
}

static int as3720_rtc_start_alarm(struct as3720 *as3720)
{
	/* enable rtc alarm interrupt */
	return as3720_set_bits(as3720, AS3720_INTERRUPTMASK2_REG,
			AS3720_IRQ_RTC_ALARM, 0);
}

static int as3720_rtc_alarm_irq_enable(struct device *dev,
				       unsigned int enabled)
{
	struct as3720 *as3720 = dev_get_drvdata(dev->parent);

	if (enabled)
		return as3720_rtc_start_alarm(as3720);
	else
		return as3720_rtc_stop_alarm(as3720);
}

static int as3720_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct as3720 *as3720 = dev_get_drvdata(dev->parent);
	struct as3720_platform_data *pdata = as3720->dev->platform_data;
	long time, start_time;
	u8 as_sec;
	u8 as_min_array[3];
	int as_min;
	struct rtc_time start_tm;
	int ret;

	/* Write time to RTC */
	rtc_tm_to_time(&alrm->time, &time);
	start_tm.tm_year = (pdata->rtc_start_year - 1900);
	start_tm.tm_mon = 0;
	start_tm.tm_mday = 1;
	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	rtc_tm_to_time(&start_tm, &start_time);
	time = time - start_time;
	as_min = time / 60;
	as_sec = time % 60;
	as_min_array[2] = (as_min & 0xFF0000) >> 16;
	as_min_array[1] = (as_min & 0xFF00) >> 8;
	as_min_array[0] = as_min & 0xFF;

	/* Write time to RTC */
	as3720_reg_write(as3720, AS3720_RTC_ALARM_SECOND_REG, as_sec);
	ret = as3720_block_write(as3720, AS3720_RTC_ALARM_MINUTE1_REG, 3,
				 as_min_array);
	if (ret < 0) {
		dev_err(dev, "failed to set alarm with err:%d\n", ret);
		return ret;
	}
	return 0;
}

static const struct rtc_class_ops as3720_rtc_ops = {
	.read_time = as3720_rtc_readtime,
	.set_time = as3720_rtc_settime,
	.read_alarm = as3720_rtc_readalarm,
	.set_alarm = as3720_rtc_setalarm,
	.alarm_irq_enable = as3720_rtc_alarm_irq_enable,
};

#ifdef CONFIG_PM_SLEEP
static int as3720_rtc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct as3720 *as3720 = dev_get_drvdata(pdev->dev.parent);
	int ret = 0;
	u32 reg;

	as3720_reg_read(as3720, AS3720_INTERRUPTMASK3_REG, &reg);

	if (device_may_wakeup(dev) &&
	    reg & AS3720_IRQ_MASK_RTC_ALARM) {
		ret = as3720_rtc_stop_alarm(as3720);
		if (ret != 0)
			dev_err(dev, "Failed to stop RTC alarm: %d\n",
				ret);
	}

	return ret;
}

static int as3720_rtc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct as3720 *as3720 = dev_get_drvdata(pdev->dev.parent);
	int ret;

	if (as3720->rtc.alarm_enabled) {
		ret = as3720_rtc_start_alarm(as3720);
		if (ret != 0)
			dev_err(dev,
				"Failed to restart RTC alarm: %d\n", ret);
	}

	return 0;
}

#define DEV_PM_OPS	(&as3720_rtc_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif

static int as3720_rtc_probe(struct platform_device *pdev)
{
	struct as3720 *as3720 = dev_get_drvdata(pdev->dev.parent);
	struct as3720_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	struct as3720_rtc *rtc = &as3720->rtc;
	int ret = 0;
	u8 ctrl;

	/* enable the RTC if it's not already enabled */
	as3720_reg_read(as3720, AS3720_RTC_CONTROL_REG, &ctrl);
	if (!(ctrl &  AS3720_RTC_ON_MASK)) {
		dev_info(&pdev->dev, "Starting RTC\n");

		ret = as3720_set_bits(as3720, AS3720_RTC_CONTROL_REG,
				      AS3720_RTC_ON_MASK, AS3720_RTC_ON_MASK);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to enable RTC: %d\n", ret);
			return ret;
		}
	}
	/* enable alarm wakeup */
	as3720_set_bits(as3720, AS3720_RTC_CONTROL_REG,
			AS3720_RTC_ALARM_WAKEUP_EN_MASK,
			AS3720_RTC_ALARM_WAKEUP_EN_MASK);

	device_init_wakeup(&pdev->dev, 1);

	rtc->rtc = rtc_device_register("as3720", &pdev->dev,
					  &as3720_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc)) {
		ret = PTR_ERR(rtc->rtc);
		dev_err(&pdev->dev, "failed to register RTC: %d\n", ret);
		return ret;
	}

	return 0;
}

static int as3720_rtc_remove(struct platform_device *pdev)
{
	struct as3720 *as3720 = platform_get_drvdata(pdev);
	struct as3720_rtc *rtc = &as3720->rtc;

	rtc_device_unregister(rtc->rtc);

	return 0;
}

static const struct dev_pm_ops as3720_rtc_pm_ops = {
	.suspend	= as3720_rtc_suspend,
	.resume		= as3720_rtc_resume,
};

static struct platform_driver as3720_rtc_driver = {
	.probe = as3720_rtc_probe,
	.remove = as3720_rtc_remove,
	.driver = {
		.name = "as3720-rtc",
		.pm = DEV_PM_OPS,
	},
};

module_platform_driver(as3720_rtc_driver);

MODULE_AUTHOR("Bernhard Breinbauer <bernhard.breinbauer@ams.com>");
MODULE_DESCRIPTION("RTC driver for AS3720 PMICs");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:as3720-rtc");

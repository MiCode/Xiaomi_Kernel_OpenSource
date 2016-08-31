/*
 * drivers/rtc/rtc-max77660.c
 * Max77660 RTC driver
 *
 * Copyright 2011-2012, Maxim Integrated Products, Inc.
 * Copyright (c) 2011-2012, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/rtc.h>
#include <linux/mfd/max77660/max77660-core.h>

/* RTC Year base */
#define RTC_YEAR_BASE				100

/* Read/write buffer update time as per datasheet */
#define MAX77660_RD_WR_BUFFER_UPDATE_TIME	300

enum {
	RTC_SEC,
	RTC_MIN,
	RTC_HOUR,
	RTC_WEEKDAY,
	RTC_MONTH,
	RTC_YEAR,
	RTC_MONTHDAY,
	RTC_MAX_BUF
};

struct max77660_rtc {
	struct rtc_device *rtc;
	struct device *dev;
	struct device *parent;
	int irq;
	bool shutdown_ongoing;
	bool alarm1_enabled;
	struct mutex rtc_reg_lock;
};

static void max77660_register_to_time(struct rtc_time *time, u8 *buf)
{
	time->tm_sec = buf[RTC_SEC] & MAX77660_RTC_SEC_MASK;
	time->tm_min = buf[RTC_MIN] & MAX77660_RTC_MIN_MASK;
	time->tm_hour = buf[RTC_HOUR] & MAX77660_RTC_HOUR_MASK;
	time->tm_wday = ffs(buf[RTC_WEEKDAY] & MAX77660_RTC_WEEKDAY_MASK) - 1;
	time->tm_mon = (buf[RTC_MONTH] & MAX77660_RTC_MONTH_MASK) - 1;
	time->tm_year = (buf[RTC_YEAR] & MAX77660_RTC_YEAR_MASK) +
					RTC_YEAR_BASE;
	time->tm_mday = buf[RTC_MONTHDAY] & MAX77660_RTC_MONTHDAY_MASK;
}

static void max77660_time_to_register(struct rtc_time *time, u8 *buf)
{
	buf[RTC_SEC] = time->tm_sec & MAX77660_RTC_SEC_MASK;
	buf[RTC_MIN] = time->tm_min & MAX77660_RTC_MIN_MASK;
	buf[RTC_HOUR] = time->tm_hour & MAX77660_RTC_HOUR_MASK;
	buf[RTC_WEEKDAY] = BIT(time->tm_wday & MAX77660_RTC_WEEKDAY_MASK);
	buf[RTC_MONTH] = (time->tm_mon + 1) & MAX77660_RTC_MONTH_MASK;
	buf[RTC_YEAR] = (time->tm_year - RTC_YEAR_BASE) &
				MAX77660_RTC_YEAR_MASK;
	buf[RTC_MONTHDAY] = time->tm_mday & MAX77660_RTC_MONTHDAY_MASK;
}

static int max77660_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_MAX_BUF];
	int ret;

	mutex_lock(&rtc->rtc_reg_lock);

	/* Update the read buffer */
	ret = max77660_reg_set_bits(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_UPDATE0, MAX77660_RTC_RB_UPDATE_MASK);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_UPDATE0 update failed: %d\n", ret);
		goto out;
	}

	/* Wait for update */
	udelay(MAX77660_RD_WR_BUFFER_UPDATE_TIME);

	ret = max77660_reg_reads(rtc->parent, MAX77660_RTC_SLAVE,
				 MAX77660_RTC_SEC, RTC_MAX_BUF, buf);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_SEC read failed: %d\n", ret);
		goto out;
	}

	max77660_register_to_time(tm, buf);

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n",
		__func__, tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

out:
	mutex_unlock(&rtc->rtc_reg_lock);
	return 0;
}

static int max77660_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_MAX_BUF];
	int ret;

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n",
		__func__, tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	max77660_time_to_register(tm, buf);

	mutex_lock(&rtc->rtc_reg_lock);

	ret = max77660_reg_writes(rtc->parent, MAX77660_RTC_SLAVE,
				MAX77660_RTC_SEC, RTC_MAX_BUF, buf);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_SEC write failed: %d\n", ret);
		goto out;
	}

	/* Update from write buffer */
	ret = max77660_reg_set_bits(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_UPDATE0, MAX77660_RTC_WB_UPDATE_MASK);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_UPDATE0 update failed: %d\n", ret);
		goto out;
	}

	/* Wait for update */
	udelay(MAX77660_RD_WR_BUFFER_UPDATE_TIME);
out:
	mutex_unlock(&rtc->rtc_reg_lock);
	return 0;
}

static int max77660_rtc_alarm_irq_enable(struct device *dev,
		 unsigned int enabled)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	int ret = 0;

	if (rtc->shutdown_ongoing) {
		dev_warn(rtc->dev,
			"device is shutdown: skipping alarm enable\n");
		return -ESHUTDOWN;
	}

	dev_dbg(dev, "%s(): enabled %u\n", __func__, enabled);

	if (enabled)
		ret = max77660_reg_clr_bits(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_IRQ_MASK, MAX77660_RTC_IRQ_ALARM1_MASK);
	else
		ret = max77660_reg_set_bits(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_IRQ_MASK, MAX77660_RTC_IRQ_ALARM1_MASK);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_IRQ_MASK update failed: %d\n", ret);
		return ret;
	}
	rtc->alarm1_enabled = enabled;
	return 0;
}

static int max77660_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_MAX_BUF];
	int ret;

	if (rtc->shutdown_ongoing) {
		dev_warn(rtc->dev,
			"device is shutdown: skipping alarm reading\n");
		return -ESHUTDOWN;
	}

	ret = max77660_reg_reads(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_ALARM_SEC1, RTC_MAX_BUF, buf);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_ALARM1 read failed: %d\n", ret);
		return ret;
	}
	max77660_register_to_time(&alrm->time, buf);
	dev_dbg(dev, "%s() %d %d %d %d %d %d\n", __func__,
		alrm->time.tm_year, alrm->time.tm_mon, alrm->time.tm_mday,
		alrm->time.tm_hour, alrm->time.tm_min, alrm->time.tm_sec);
	alrm->enabled = rtc->alarm1_enabled;
	return 0;
}

static int max77660_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	u8 buf[RTC_MAX_BUF];
	int ret;

	if (rtc->shutdown_ongoing) {
		dev_warn(rtc->dev,
			"device is shutdown: skipping alarm setting\n");
		return -ESHUTDOWN;
	}

	/* Set alarm for sec/min/hour/month/year/day */
	ret = max77660_reg_write(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_AE1, 0x77);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_AE1 write failed: %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n", __func__,
		alrm->time.tm_year, alrm->time.tm_mon, alrm->time.tm_mday,
		alrm->time.tm_hour, alrm->time.tm_min, alrm->time.tm_sec);
	max77660_time_to_register(&alrm->time, buf);
	buf[RTC_WEEKDAY] = 0;

	ret = max77660_reg_writes(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_ALARM_SEC1, RTC_MAX_BUF, buf);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_ALARAM1 write failed: %d\n", ret);
		return ret;
	}
	return max77660_rtc_alarm_irq_enable(dev, alrm->enabled);
}

static irqreturn_t max77660_rtc_irq(int irq, void *data)
{
	struct max77660_rtc *rtc = (struct max77660_rtc *)data;
	u8 status;
	int ret;

	ret = max77660_reg_read(rtc->parent, MAX77660_RTC_SLAVE,
			MAX77660_RTC_IRQ, &status);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_IRQ read failed: %d\n", ret);
		goto out;
	}

	if (!(status & MAX77660_RTC_IRQ_ALARM1_MASK)) {
		dev_err(rtc->dev, "Unkknow RTC irq: status 0x%02x\n", status);
		goto out;
	}
	rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);
out:
	return IRQ_HANDLED;
}

static const struct rtc_class_ops max77660_rtc_ops = {
	.read_time = max77660_rtc_read_time,
	.set_time = max77660_rtc_set_time,
	.read_alarm = max77660_rtc_read_alarm,
	.set_alarm = max77660_rtc_set_alarm,
	.alarm_irq_enable = max77660_rtc_alarm_irq_enable,
};

static int max77660_rtc_preinit(struct max77660_rtc *rtc)
{
	int ret;

	ret = max77660_reg_write(rtc->parent, MAX77660_RTC_SLAVE,
			 MAX77660_RTC_IRQ_MASK, 0xFF);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_IRQ_MASK write failed: %d\n", ret);
		return ret;
	}

	/* Configure Binary mode and 24hour mode */
	ret = max77660_reg_write(rtc->parent, MAX77660_RTC_SLAVE,
			 MAX77660_RTC_CTRL_MODE, MAX77660_RTCCNTLM_MASK);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_CTRL_MODE write failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_write(rtc->parent, MAX77660_RTC_SLAVE,
			 MAX77660_RTC_CTRL, MAX77660_RTCCNTL_HRMODE_24);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_CTRL write failed: %d\n", ret);
		return ret;
	}
	ret = max77660_reg_write(rtc->parent, MAX77660_RTC_SLAVE,
			 MAX77660_RTC_CTRL_MODE, 0x0);
	if (ret < 0) {
		dev_err(rtc->dev, "RTC_CTRL_MODE write failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int max77660_rtc_probe(struct platform_device *pdev)
{
	static struct max77660_rtc *rtc;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc) {
		dev_err(&pdev->dev, "Memory allocation failed for rtc\n");
		return -ENOMEM;
	}

	rtc->shutdown_ongoing = false;
	dev_set_drvdata(&pdev->dev, rtc);
	rtc->dev = &pdev->dev;
	rtc->parent = pdev->dev.parent;

	ret = max77660_rtc_preinit(rtc);
	if (ret < 0) {
		dev_err(&pdev->dev, "RTC pre initilisation failed: %d\n", ret);
		return ret;
	}

	device_init_wakeup(&pdev->dev, 1);

	mutex_init(&rtc->rtc_reg_lock);
	rtc->rtc = rtc_device_register("max77660-rtc", &pdev->dev,
				       &max77660_rtc_ops, THIS_MODULE);
	if (IS_ERR_OR_NULL(rtc->rtc)) {
		dev_err(&pdev->dev, "probe: Failed to register rtc\n");
		ret = PTR_ERR(rtc->rtc);
		goto out;
	}

	rtc->irq = platform_get_irq(pdev, 0);
	ret = request_threaded_irq(rtc->irq, NULL, max77660_rtc_irq,
		   IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(&pdev->dev), rtc);
	if (ret < 0) {
		dev_err(rtc->dev, "request irq %d failed: %dn", rtc->irq, ret);
		goto out_rtc_free;
	}

	return 0;

out_rtc_free:
	rtc_device_unregister(rtc->rtc);
out:
	mutex_destroy(&rtc->rtc_reg_lock);
	return ret;
}

static int max77660_rtc_remove(struct platform_device *pdev)
{
	struct max77660_rtc *rtc = dev_get_drvdata(&pdev->dev);

	free_irq(rtc->irq, rtc);

	rtc_device_unregister(rtc->rtc);
	mutex_destroy(&rtc->rtc_reg_lock);

	return 0;
}

static void max77660_rtc_shutdown(struct platform_device *pdev)
{
	struct max77660_rtc *rtc = dev_get_drvdata(&pdev->dev);

	rtc->shutdown_ongoing = true;
}

#ifdef CONFIG_PM_SLEEP
static int max77660_rtc_suspend(struct device *dev)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		struct rtc_wkalrm alm;

		enable_irq_wake(rtc->irq);

		/* Set RTC can generate the wakeup signal */
		ret = max77660_reg_set_bits(rtc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG2, MAX77660_GLBLCNFG2_RTCWKEN);
		if (ret < 0)
			dev_err(rtc->dev, "RTC wake enable failed: %d\n", ret);

		ret = max77660_rtc_read_alarm(dev, &alm);
		if (!ret) {
			dev_info(dev, "%s() alrm %d time %d %d %d %d %d %d\n",
				__func__, alm.enabled, alm.time.tm_year,
				alm.time.tm_mon, alm.time.tm_mday,
				alm.time.tm_hour, alm.time.tm_min,
				alm.time.tm_sec);
		}
	}
	return 0;
}

static int max77660_rtc_resume(struct device *dev)
{
	struct max77660_rtc *rtc = dev_get_drvdata(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		struct rtc_time tm;

		disable_irq_wake(rtc->irq);

		/* Set RTC can generate the wakeup signal */
		ret = max77660_reg_clr_bits(rtc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG2, MAX77660_GLBLCNFG2_RTCWKEN);
		if (ret < 0)
			dev_err(rtc->dev, "RTC wake enable failed: %d\n", ret);

		ret = max77660_rtc_read_time(dev, &tm);
		if (!ret)
			dev_info(dev, "%s() %d %d %d %d %d %d\n",
				__func__, tm.tm_year, tm.tm_mon, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
	}
	return 0;
};
#endif

static const struct dev_pm_ops max77660_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77660_rtc_suspend, max77660_rtc_resume)
};

static struct platform_driver max77660_rtc_driver = {
	.probe = max77660_rtc_probe,
	.remove = max77660_rtc_remove,
	.driver = {
		.name = "max77660-rtc",
		.owner = THIS_MODULE,
		.pm = &max77660_pm_ops,
	},
	.shutdown = max77660_rtc_shutdown,
};

module_platform_driver(max77660_rtc_driver);

MODULE_DESCRIPTION("max77660 RTC driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Maxim Integrated");
MODULE_ALIAS("platform:max77660-rtc");

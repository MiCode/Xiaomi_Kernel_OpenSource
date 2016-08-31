/*
 * drivers/rtc/rtc_tps6591x.c
 *
 * RTC driver for TI TPS6591x
 *
 * Copyright (c) 2011-2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* #define DEBUG		1 */
/* #define VERBOSE_DEBUG	1 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/tps6591x.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/module.h>

#define RTC_CTRL		0x10
#define RTC_STATUS		0x11
#define RTC_SECONDS_REG		0x0
#define RTC_ALARM		0x8
#define RTC_INT			0x12
#define RTC_RESET_STATUS	0x16
#define RTC_BBCH_REG		0x39

#define RTC_BBCH_SEL		0x02
#define RTC_BBCH_EN     	0x01
#define ENABLE_ALARM_INT 0x8
#define RTC_RESET_VALUE 0x80
#define ALARM_INT_STATUS 0x40

/*
Linux RTC driver refers 1900 as base year in many calculations.
(e.g. refer drivers/rtc/rtc-lib.c)
*/
#define OS_REF_YEAR 1900

/*
	PMU RTC have only 2 nibbles to store year information, so using an offset
	of 100 to set the base year as 2000 for our driver.
*/
#define RTC_YEAR_OFFSET 100

struct tps6591x_rtc {
	unsigned long		epoch_start;
	int			irq;
	struct rtc_device	*rtc;
	bool			irq_en;
};

static int tps6591x_read_regs(struct device *dev, int reg, int len,
	uint8_t *val)
{
	int ret;

	/* dummy read of STATUS_REG as per data sheet */
	ret = tps6591x_reads(dev->parent, RTC_STATUS, 1, val);
	if (ret < 0) {
		dev_err(dev->parent, "\n %s failed reading from RTC_STATUS\n",
			__func__);
		WARN_ON(1);
		return ret;
	}

	ret = tps6591x_reads(dev->parent, reg, len, val);
	if (ret < 0) {
		dev_err(dev->parent, "\n %s failed reading from 0x%02x\n",
			__func__, reg);
		WARN_ON(1);
		return ret;
	}
	return 0;
}

static int tps6591x_write_regs(struct device *dev, int reg, int len,
	uint8_t *val)
{
	int ret;
	ret = tps6591x_writes(dev->parent, reg, len, val);
	if (ret < 0) {
		dev_err(dev->parent, "\n %s failed writing\n", __func__);
		WARN_ON(1);
		return ret;
	}

	return 0;
}

static int tps6591x_rtc_valid_tm(struct rtc_time *tm)
{
	if (tm->tm_year >= (RTC_YEAR_OFFSET + 99)
		|| tm->tm_year < (RTC_YEAR_OFFSET)
		|| tm->tm_mon >= 12
		|| tm->tm_mday < 1
		|| tm->tm_mday > rtc_month_days(tm->tm_mon, tm->tm_year + OS_REF_YEAR)
		|| tm->tm_hour >= 24
		|| tm->tm_min >= 60
		|| tm->tm_sec >= 60)
		return -EINVAL;
	return 0;
}

static u8 dec2bcd(u8 dec)
{
	return ((dec/10)<<4)+(dec%10);
}

static u8 bcd2dec(u8 bcd)
{
	return (bcd >> 4)*10+(bcd & 0xF);
}

static void convert_bcd_to_decimal(u8 *buf, u8 len)
{
	int i = 0;
	for (i = 0; i < len; i++)
		buf[i] = bcd2dec(buf[i]);
}

static void convert_decimal_to_bcd(u8 *buf, u8 len)
{
	int i = 0;
	for (i = 0; i < len; i++)
		buf[i] = dec2bcd(buf[i]);
}

static void print_time(struct device *dev, struct rtc_time *tm)
{
	dev_info(dev, "RTC Time : %d/%d/%d %d:%d:%d\n",
		(tm->tm_mon + 1), tm->tm_mday, (tm->tm_year + OS_REF_YEAR),
		tm->tm_hour, tm->tm_min , tm->tm_sec);
}

static int tps6591x_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u8 buff[7];
	int err;
	err = tps6591x_read_regs(dev, RTC_SECONDS_REG, sizeof(buff), buff);
	if (err < 0) {
		dev_err(dev, "\n %s :: failed to read time\n", __FILE__);
		return err;
	}
	convert_bcd_to_decimal(buff, sizeof(buff));
	tm->tm_sec = buff[0];
	tm->tm_min = buff[1];
	tm->tm_hour = buff[2];
	tm->tm_mday = buff[3];
	tm->tm_mon = buff[4] - 1;
	tm->tm_year = buff[5] + RTC_YEAR_OFFSET;
	tm->tm_wday = buff[6];
	print_time(dev, tm);
	return tps6591x_rtc_valid_tm(tm);
}

static int tps6591x_rtc_stop(struct device *dev)
{
	u8 reg = 0;
	u8 retries = 0;
	int err;
	do {
		err = tps6591x_read_regs(dev, RTC_CTRL, 1, &reg);
		if (err < 0) {
			dev_err(dev->parent, "\n failed to read RTC_CTRL reg\n");
			return err;
		}

		/* clear STOP bit alone */
		reg &= ~0x1;

		err = tps6591x_write_regs(dev, RTC_CTRL, 1, &reg);
		if (err < 0) {
			dev_err(dev->parent, "\n failed to program RTC_CTRL reg\n");
			return err;
		}

		err = tps6591x_read_regs(dev, RTC_STATUS, 1, &reg);
		if (err < 0) {
			dev_err(dev->parent, "\n failed to read RTC_CTRL reg\n");
			return err;
		}
		/* FixMe: Is allowing up to 5 retries sufficient?? */
		if (retries++ == 5) {
			dev_err(dev->parent, "\n failed to stop RTC\n");
			return -EBUSY;
		}
	}	while (reg & 2);
	return 0;
}

static int tps6591x_rtc_start(struct device *dev)
{
	u8 reg = 0;
	u8 retries = 0;
	int err;

	do {
		err = tps6591x_read_regs(dev, RTC_CTRL, 1, &reg);
		if (err < 0) {
			dev_err(dev->parent, "\n failed to read RTC_CTRL reg\n");
			return err;
		}

		/* set STOP bit alone */
		reg |= 0x1;

		err = tps6591x_write_regs(dev, RTC_CTRL, 1, &reg);
		if (err < 0) {
			dev_err(dev->parent, "\n failed to program RTC_CTRL reg\n");
			return err;
		}

		err = tps6591x_read_regs(dev, RTC_STATUS, 1, &reg);
		if (err < 0) {
			dev_err(dev->parent, "\n failed to read RTC_CTRL reg\n");
			return err;
		}
		/* FixMe: Is allowing up to 5 retries sufficient?? */
		if (retries++ == 5) {
			dev_err(dev->parent, "\n failed to stop RTC\n");
			return -EBUSY;
		}
	}	while (!(reg & 2));
	return 0;
}


static int tps6591x_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	u8 buff[7];
	int err;

	err = tps6591x_rtc_valid_tm(tm);
	if (err < 0) {
		dev_err(dev->parent, "\n Invalid Time\n");
		return err;
	}

	buff[0] = tm->tm_sec;
	buff[1] = tm->tm_min;
	buff[2] = tm->tm_hour;
	buff[3] = tm->tm_mday;
	buff[4] = tm->tm_mon + 1;
	buff[5] = tm->tm_year % RTC_YEAR_OFFSET;
	buff[6] = tm->tm_wday;

	print_time(dev, tm);
	convert_decimal_to_bcd(buff, sizeof(buff));
	err = tps6591x_rtc_stop(dev);
	if (err < 0) {
		dev_err(dev->parent, "\n failed to clear RTC_ENABLE\n");
		return err;
	}

	err = tps6591x_write_regs(dev, RTC_SECONDS_REG, sizeof(buff), buff);
	if (err < 0) {
		dev_err(dev->parent, "\n failed to program new time\n");
		return err;
	}

	err = tps6591x_rtc_start(dev);
	if (err < 0) {
		dev_err(dev->parent, "\n failed to set RTC_ENABLE\n");
		return err;
	}

	return 0;
}

static int tps6591x_rtc_alarm_irq_enable(struct device *dev,
					 unsigned int enable)
{
	struct tps6591x_rtc *rtc = dev_get_drvdata(dev);
	u8 reg;
	int err;

	if (rtc->irq == -1)
		return -EIO;

	if (enable) {
		if (rtc->irq_en == true)
			return 0;
		err = tps6591x_read_regs(dev, RTC_INT, 1, &reg);
		if (err)
			return err;
		reg |= 0x8;
		err = tps6591x_write_regs(dev, RTC_INT, 1, &reg);
		if (err)
			return err;
		rtc->irq_en = true;
	} else {
		if (rtc->irq_en == false)
			return 0;
		err = tps6591x_read_regs(dev, RTC_INT, 1, &reg);
		if (err)
			return err;
		reg &= ~0x8;
		err = tps6591x_write_regs(dev, RTC_INT, 1, &reg);
		if (err)
			return err;
		rtc->irq_en = false;
	}
	return 0;
}

static int tps6591x_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct tps6591x_rtc *rtc = dev_get_drvdata(dev);
	unsigned long seconds;
	u8 buff[6];
	int err;
	struct rtc_time tm;

	if (rtc->irq == -1)
		return -EIO;

	err = tps6591x_rtc_valid_tm(&alrm->time);
	if (err < 0) {
		dev_err(dev->parent, "\n Invalid alarm time\n");
		return err;
	}

	dev_info(dev->parent, "\n setting alarm to requested time::\n");
	print_time(dev->parent, &alrm->time);
	rtc_tm_to_time(&alrm->time, &seconds);
	tps6591x_rtc_read_time(dev, &tm);
	rtc_tm_to_time(&tm, &rtc->epoch_start);

	if (WARN_ON(alrm->enabled && (seconds < rtc->epoch_start))) {
		dev_err(dev->parent, "\n can't set alarm to requested time\n");
		return -EINVAL;
	}

	err = tps6591x_rtc_alarm_irq_enable(dev, alrm->enabled);
	if(err) {
		dev_err(dev->parent, "\n can't set alarm irq\n");
		return err;
	}

	buff[0] = alrm->time.tm_sec;
	buff[1] = alrm->time.tm_min;
	buff[2] = alrm->time.tm_hour;
	buff[3] = alrm->time.tm_mday;
	buff[4] = alrm->time.tm_mon + 1;
	buff[5] = alrm->time.tm_year % RTC_YEAR_OFFSET;
	convert_decimal_to_bcd(buff, sizeof(buff));
	err = tps6591x_write_regs(dev, RTC_ALARM, sizeof(buff), buff);
	if (err)
		dev_err(dev->parent, "\n unable to program alarm\n");

	return err;
}

static int tps6591x_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	u8 buff[6];
	int err;

	err = tps6591x_read_regs(dev, RTC_ALARM, sizeof(buff), buff);
	if (err)
		return err;
	convert_bcd_to_decimal(buff, sizeof(buff));

	alrm->time.tm_sec = buff[0];
	alrm->time.tm_min = buff[1];
	alrm->time.tm_hour = buff[2];
	alrm->time.tm_mday = buff[3];
	alrm->time.tm_mon = buff[4] - 1;
	alrm->time.tm_year = buff[5] + RTC_YEAR_OFFSET;

	dev_info(dev->parent, "\n getting alarm time::\n");
	print_time(dev, &alrm->time);

	return 0;
}

static const struct rtc_class_ops tps6591x_rtc_ops = {
	.read_time	= tps6591x_rtc_read_time,
	.set_time	= tps6591x_rtc_set_time,
	.set_alarm	= tps6591x_rtc_set_alarm,
	.read_alarm	= tps6591x_rtc_read_alarm,
	.alarm_irq_enable = tps6591x_rtc_alarm_irq_enable,
};

static irqreturn_t tps6591x_rtc_irq(int irq, void *data)
{
	struct device *dev = data;
	struct tps6591x_rtc *rtc = dev_get_drvdata(dev);
	u8 reg;
	int err;

	/* clear Alarm status bits.*/
	err = tps6591x_read_regs(dev, RTC_STATUS, 1, &reg);
	if (err) {
		dev_err(dev->parent, "unable to read RTC_STATUS reg\n");
		return -EBUSY;
	}

	reg = ALARM_INT_STATUS;
	err = tps6591x_write_regs(dev, RTC_STATUS, 1, &reg);
	if (err) {
		dev_err(dev->parent, "unable to program RTC_STATUS reg\n");
		return -EBUSY;
	}

	rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);
	return IRQ_HANDLED;
}

static int tps6591x_rtc_probe(struct platform_device *pdev)
{
	struct tps6591x_rtc_platform_data *pdata = pdev->dev.platform_data;
	struct tps6591x_rtc *rtc;
	struct rtc_time tm;
	int err;
	u8 reg;

	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);

	if (!rtc)
		return -ENOMEM;

	rtc->irq = -1;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data specified\n");
		return -EINVAL;
	}

	if (pdata->irq < 0)
		dev_err(&pdev->dev, "\n no IRQ specified, wakeup is disabled\n");

	dev_set_drvdata(&pdev->dev, rtc);
	device_init_wakeup(&pdev->dev, 1);
	rtc->rtc = rtc_device_register(pdev->name, &pdev->dev,
				       &tps6591x_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc->rtc)) {
		err = PTR_ERR(rtc->rtc);
		goto fail;
	}

	if ((int)pdev && (int)&pdev->dev)
		err = tps6591x_read_regs(&pdev->dev, RTC_STATUS, 1, &reg);
	else {
		dev_err(&pdev->dev, "\n %s Input params incorrect\n", __func__);
		return -EBUSY;
	}
	if (err) {
		dev_err(&pdev->dev, "\n %s unable to read status\n", __func__);
		return -EBUSY;
	}

	reg = RTC_BBCH_SEL | RTC_BBCH_EN;
	tps6591x_write_regs(&pdev->dev, RTC_BBCH_REG, 1, &reg);
	if (err) {
		dev_err(&pdev->dev, "unable to program Charger reg\n");
		return -EBUSY;
	}

	err = tps6591x_rtc_start(&pdev->dev);
	if (err) {
		dev_err(&pdev->dev, "unable to start RTC\n");
		return -EBUSY;
	}

	tps6591x_rtc_read_time(&pdev->dev, &tm);

	if (tps6591x_rtc_valid_tm(&tm) < 0) {
		if (pdata->time.tm_year < 2000 || pdata->time.tm_year >= 2100) {
			memset(&pdata->time, 0, sizeof(pdata->time));
			pdata->time.tm_year = 2000;
			pdata->time.tm_mday = 1;
		}
		pdata->time.tm_year -= OS_REF_YEAR;
		tps6591x_rtc_set_time(&pdev->dev, &pdata->time);
	}

	reg = ALARM_INT_STATUS;
	err = tps6591x_write_regs(&pdev->dev, RTC_STATUS, 1, &reg);
	if (err) {
		dev_err(&pdev->dev, "unable to program RTC_STATUS reg\n");
		return -EBUSY;
	}

	reg = ENABLE_ALARM_INT;
	tps6591x_write_regs(&pdev->dev, RTC_INT, 1, &reg);
	if (err) {
		dev_err(&pdev->dev, "unable to program Interrupt Mask reg\n");
		return -EBUSY;
	}

	if (pdata && (pdata->irq >= 0)) {
		rtc->irq = pdata->irq;
		err = request_threaded_irq(pdata->irq, NULL, tps6591x_rtc_irq,
					IRQF_ONESHOT, "rtc_tps6591x",
					&pdev->dev);
		if (err) {
			dev_err(&pdev->dev, "request IRQ:%d fail\n", rtc->irq);
			rtc->irq = -1;
		} else {
			device_init_wakeup(&pdev->dev, 1);
			enable_irq_wake(rtc->irq);
		}
	}
	return 0;

fail:
	if (!IS_ERR_OR_NULL(rtc->rtc))
		rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	return err;
}

static int tps6591x_rtc_remove(struct platform_device *pdev)
{
	struct tps6591x_rtc *rtc = dev_get_drvdata(&pdev->dev);

	if (rtc->irq != -1)
		free_irq(rtc->irq, rtc);
	rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	return 0;
}

static struct platform_driver tps6591x_rtc_driver = {
	.driver	= {
		.name	= "rtc_tps6591x",
		.owner	= THIS_MODULE,
	},
	.probe	= tps6591x_rtc_probe,
	.remove	= tps6591x_rtc_remove,
};

static int __init tps6591x_rtc_init(void)
{
	return platform_driver_register(&tps6591x_rtc_driver);
}
module_init(tps6591x_rtc_init);

static void __exit tps6591x_rtc_exit(void)
{
	platform_driver_unregister(&tps6591x_rtc_driver);
}
module_exit(tps6591x_rtc_exit);

MODULE_DESCRIPTION("TI TPS6591x RTC driver");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc_tps6591x");

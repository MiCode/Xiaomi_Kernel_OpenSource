/*
 * drivers/rtc/rtc_ricoh583.c
 *
 * rtc driver for ricoh rc5t583 pmu
 *
 * copyright (c) 2011, nvidia corporation.
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful, but without
 * any warranty; without even the implied warranty of merchantability or
 * fitness for a particular purpose.  see the gnu general public license for
 * more details.
 *
 * you should have received a copy of the gnu general public license along
 * with this program; if not, write to the free software foundation, inc.,
 * 51 franklin street, fifth floor, boston, ma  02110-1301, usa.
 */

/* #define debug		1 */
/* #define verbose_debug	1 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/ricoh583.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/module.h>

#define rtc_ctrl1		0xED
#define rtc_ctrl2		0xEE
#define rtc_seconds_reg		0xE0
#define rtc_alarm_y		0xF0
#define rtc_adjust		0xE7

/*
linux rtc driver refers 1900 as base year in many calculations.
(e.g. refer drivers/rtc/rtc-lib.c)
*/
#define os_ref_year 1900

/*
	pmu rtc have only 2 nibbles to store year information, so using an
	offset of 100 to set the base year as 2000 for our driver.
*/
#define rtc_year_offset 100

struct ricoh583_rtc {
	unsigned long		epoch_start;
	int			irq;
	struct rtc_device	*rtc;
	bool			irq_en;
};

static int ricoh583_read_regs(struct device *dev, int reg, int len,
	uint8_t *val)
{
	int ret;

	ret = ricoh583_bulk_reads(dev->parent, reg, len, val);
	if (ret < 0) {
		dev_err(dev->parent, "\n %s failed reading from 0x%02x\n",
			__func__, reg);
		WARN_ON(1);
	}
	return ret;
}

static int ricoh583_write_regs(struct device *dev, int reg, int len,
	uint8_t *val)
{
	int ret;
	ret = ricoh583_bulk_writes(dev->parent, reg, len, val);
	if (ret < 0) {
		dev_err(dev->parent, "\n %s failed writing\n", __func__);
		WARN_ON(1);
	}

	return ret;
}

static int ricoh583_rtc_valid_tm(struct device *dev, struct rtc_time *tm)
{
	if (tm->tm_year >= (rtc_year_offset + 99)
		|| tm->tm_mon > 12
		|| tm->tm_mday < 1
		|| tm->tm_mday > rtc_month_days(tm->tm_mon,
			tm->tm_year + os_ref_year)
		|| tm->tm_hour >= 24
		|| tm->tm_min >= 60
		|| tm->tm_sec >= 60) {
		dev_err(dev->parent, "\n returning error due to time"
		"%d/%d/%d %d:%d:%d", tm->tm_mon, tm->tm_mday,
		tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
		return -EINVAL;
	}
	return 0;
}

static u8 dec2bcd(u8 dec)
{
	return ((dec/10)<<4)+(dec%10);
}

static u8 bcd2dec(u8 bcd)
{
	return (bcd >> 4)*10+(bcd & 0xf);
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
	dev_info(dev, "rtc-time : %d/%d/%d %d:%d\n",
		(tm->tm_mon + 1), tm->tm_mday, (tm->tm_year + os_ref_year),
		tm->tm_hour, tm->tm_min);
}

static int ricoh583_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u8 buff[7];
	int err;
	err = ricoh583_read_regs(dev, rtc_seconds_reg, sizeof(buff), buff);
	if (err < 0) {
		dev_err(dev, "\n %s :: failed to read time\n", __FILE__);
		return err;
	}
	convert_bcd_to_decimal(buff, sizeof(buff));
	tm->tm_sec  = buff[0];
	tm->tm_min  = buff[1];
	tm->tm_hour = buff[2];
	tm->tm_wday = buff[3];
	tm->tm_mday = buff[4];
	tm->tm_mon  = buff[5] - 1;
	tm->tm_year = buff[6] + rtc_year_offset;
	print_time(dev, tm);
	return ricoh583_rtc_valid_tm(dev, tm);
}

static int ricoh583_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	u8 buff[7];
	int err;

	print_time(dev, tm);
	buff[0] = tm->tm_sec;
	buff[1] = tm->tm_min;
	buff[2] = tm->tm_hour;
	buff[3] = tm->tm_wday;
	buff[4] = tm->tm_mday;
	buff[5] = tm->tm_mon + 1;
	buff[6] = tm->tm_year - rtc_year_offset;

	convert_decimal_to_bcd(buff, sizeof(buff));
	err = ricoh583_write_regs(dev, rtc_seconds_reg, sizeof(buff), buff);
	if (err < 0) {
		dev_err(dev->parent, "\n failed to program new time\n");
		return err;
	}

	return 0;
}
static int ricoh583_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm);

static int ricoh583_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct ricoh583_rtc *rtc = dev_get_drvdata(dev);
	unsigned long seconds;
	u8 buff[5];
	int err;
	struct rtc_time tm;

	if (rtc->irq == -1)
		return -EIO;

	rtc_tm_to_time(&alrm->time, &seconds);
	ricoh583_rtc_read_time(dev, &tm);
	rtc_tm_to_time(&tm, &rtc->epoch_start);
	/*
		work around: As YAL does not provide the seconds register,
		program minute register to next minute, in cases when alarm
		is requested within a minute from the current time.
	*/
	if (seconds - rtc->epoch_start < 60)
		alrm->time.tm_min += 1;
	dev_info(dev->parent, "\n setting alarm to requested time::\n");
	print_time(dev->parent, &alrm->time);

	if (WARN_ON(alrm->enabled && (seconds < rtc->epoch_start))) {
		dev_err(dev->parent, "\n can't set alarm to requested time\n");
		return -EINVAL;
	}

	if (alrm->enabled && !rtc->irq_en)
		rtc->irq_en = true;
	else if (!alrm->enabled && rtc->irq_en)
		rtc->irq_en = false;

	buff[0] = alrm->time.tm_min;
	buff[1] = alrm->time.tm_hour;
	buff[2] = alrm->time.tm_mday;
	buff[3] = alrm->time.tm_mon + 1;
	buff[4] = alrm->time.tm_year - rtc_year_offset;
	convert_decimal_to_bcd(buff, sizeof(buff));
	err = ricoh583_write_regs(dev, rtc_alarm_y, sizeof(buff), buff);
	if (err) {
		dev_err(dev->parent, "\n unable to set alarm\n");
		return -EBUSY;
	}
	buff[0] = 0x20; /* to enable alarm_y */
	buff[1] = 0x20; /* to enable 24-hour format */
	err = ricoh583_write_regs(dev, rtc_ctrl1, 2, buff);
	if (err) {
		dev_err(dev, "failed programming rtc ctrl regs\n");
		return -EBUSY;
	}
return err;
}

static int ricoh583_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	u8 buff[5];
	int err;

	err = ricoh583_read_regs(dev, rtc_alarm_y, sizeof(buff), buff);
	if (err)
		return err;
	convert_bcd_to_decimal(buff, sizeof(buff));

	alrm->time.tm_min  = buff[0];
	alrm->time.tm_hour = buff[1];
	alrm->time.tm_mday = buff[2];
	alrm->time.tm_mon  = buff[3] - 1;
	alrm->time.tm_year = buff[4] + rtc_year_offset;

	dev_info(dev->parent, "\n getting alarm time::\n");
	print_time(dev, &alrm->time);

	return 0;
}

static const struct rtc_class_ops ricoh583_rtc_ops = {
	.read_time	= ricoh583_rtc_read_time,
	.set_time	= ricoh583_rtc_set_time,
	.set_alarm	= ricoh583_rtc_set_alarm,
	.read_alarm	= ricoh583_rtc_read_alarm,
};

static irqreturn_t ricoh583_rtc_irq(int irq, void *data)
{
	struct device *dev = data;
	struct ricoh583_rtc *rtc = dev_get_drvdata(dev);
	u8 reg;
	int err;

	/* clear alarm-Y status bits.*/
	err = ricoh583_read_regs(dev, rtc_ctrl2, 1, &reg);
	if (err) {
		dev_err(dev->parent, "unable to read rtc_ctrl2 reg\n");
		return -EBUSY;
	}
	reg &= ~0x8;
	err = ricoh583_write_regs(dev, rtc_ctrl2, 1, &reg);
	if (err) {
		dev_err(dev->parent, "unable to program rtc_status reg\n");
		return -EBUSY;
	}

	rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);
	return IRQ_HANDLED;
}

static int __devinit ricoh583_rtc_probe(struct platform_device *pdev)
{
	struct ricoh583_rtc_platform_data *pdata = pdev->dev.platform_data;
	struct ricoh583_rtc *rtc;
	struct rtc_time tm;
	int err;
	u8 reg[2];
	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);

	if (!rtc)
		return -ENOMEM;

	rtc->irq = -1;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data specified\n");
		return -EINVAL;
	}

	if (pdata->irq < 0)
		dev_err(&pdev->dev, "\n no irq specified, wakeup is disabled\n");

	dev_set_drvdata(&pdev->dev, rtc);
	device_init_wakeup(&pdev->dev, 1);
	rtc->rtc = rtc_device_register(pdev->name, &pdev->dev,
				       &ricoh583_rtc_ops, THIS_MODULE);

	if (IS_ERR(rtc->rtc)) {
		err = PTR_ERR(rtc->rtc);
		goto fail;
	}
	reg[0] =  0; /* clearing RTC Adjust register */
	err = ricoh583_write_regs(&pdev->dev, rtc_adjust, 1, reg);
	if (err) {
		dev_err(&pdev->dev, "unable to program rtc_adjust reg\n");
		return -EBUSY;
	}

	reg[0] = 0x20; /* to enable alarm_y */
	reg[1] = 0x20; /* to enable 24-hour format */
	err = ricoh583_write_regs(&pdev->dev, rtc_ctrl1, 2, reg);
	if (err) {
		dev_err(&pdev->dev, "failed rtc setup\n");
		return -EBUSY;
	}

	ricoh583_rtc_read_time(&pdev->dev, &tm);
	if (ricoh583_rtc_valid_tm(&pdev->dev, &tm)) {
		if (pdata->time.tm_year < 2000 || pdata->time.tm_year > 2100) {
			memset(&pdata->time, 0, sizeof(pdata->time));
			pdata->time.tm_year = rtc_year_offset;
			pdata->time.tm_mday = 1;
		} else
		pdata->time.tm_year -= os_ref_year;
		ricoh583_rtc_set_time(&pdev->dev, &pdata->time);
	}
	if (pdata && (pdata->irq >= 0)) {
		rtc->irq = pdata->irq;
		err = request_threaded_irq(pdata->irq, NULL, ricoh583_rtc_irq,
					IRQF_ONESHOT, "rtc_ricoh583",
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

static int __devexit ricoh583_rtc_remove(struct platform_device *pdev)
{
	struct ricoh583_rtc *rtc = dev_get_drvdata(&pdev->dev);

	if (rtc->irq != -1)
		free_irq(rtc->irq, rtc);
	rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	return 0;
}

static struct platform_driver ricoh583_rtc_driver = {
	.driver	= {
		.name	= "rtc_ricoh583",
		.owner	= THIS_MODULE,
	},
	.probe	= ricoh583_rtc_probe,
	.remove	= __devexit_p(ricoh583_rtc_remove),
};

static int __init ricoh583_rtc_init(void)
{
	return platform_driver_register(&ricoh583_rtc_driver);
}
module_init(ricoh583_rtc_init);

static void __exit ricoh583_rtc_exit(void)
{
	platform_driver_unregister(&ricoh583_rtc_driver);
}
module_exit(ricoh583_rtc_exit);

MODULE_DESCRIPTION("RICOH PMU ricoh583 RTC driver");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc_ricoh583");

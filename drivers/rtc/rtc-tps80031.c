/*
 * drivers/rtc/rtc_tps80031.c
 *
 * RTC driver for TI TPS80031
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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
#include <linux/mfd/tps80031.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>

#define RTC_CTRL		0x10
#define RTC_STATUS		0x11
#define RTC_SECONDS_REG		0x0
#define RTC_ALARM		0x8
#define RTC_INT			0x12
#define RTC_RESET_STATUS	0x16

#define ENABLE_ALARM_INT 0x8
#define ALARM_INT_STATUS 0x40
#define STOP_RTC 1

/* Power on reset Values of RTC registers */
#define RTC_POR_YEAR 0
#define RTC_POR_MONTH 1
#define RTC_POR_DAY 1

/*
Linux RTC driver refers 19th centaury as base year in many
calculations. (e.g. refer drivers/rtc/rtc-lib.c)
*/
#define OS_REF_YEAR 1900

/*
	PMU RTC have only 2 nibbles to store year information, so using an
	offset of 100 to set the base year as 2000 for our driver.
*/
#define RTC_YEAR_OFFSET 100

struct tps80031_rtc {
	unsigned long		epoch_start;
	int			irq;
	struct rtc_device	*rtc;
	u8 			alarm_irq_enabled;
	int			msecure_gpio;
};

static inline void tps80031_enable_rtc_write(struct device *dev)
{
	struct tps80031_rtc *rtc = dev_get_drvdata(dev);

	if (rtc->msecure_gpio >= 0)
		gpio_set_value(rtc->msecure_gpio, 1);
}

static inline void tps80031_disable_rtc_write(struct device *dev)
{
	struct tps80031_rtc *rtc = dev_get_drvdata(dev);

	if (rtc->msecure_gpio >= 0)
		gpio_set_value(rtc->msecure_gpio, 0);
}

static int tps80031_read_regs(struct device *dev, int reg, int len,
	uint8_t *val)
{
	int ret;

	/* dummy read of STATUS_REG as per data sheet */
	ret = tps80031_reads(dev->parent, 1, RTC_STATUS, 1, val);
	if (ret < 0) {
		dev_err(dev->parent, "failed reading RTC_STATUS\n");
		WARN_ON(1);
		return ret;
	}

	ret = tps80031_reads(dev->parent, 1, reg, len, val);
	if (ret < 0) {
		dev_err(dev->parent, "failed reading from reg %d\n", reg);
		WARN_ON(1);
		return ret;
	}
	return 0;
}

static int tps80031_write_regs(struct device *dev, int reg, int len,
	uint8_t *val)
{
	int ret;

	tps80031_enable_rtc_write(dev);
	ret = tps80031_writes(dev->parent, 1, reg, len, val);
	if (ret < 0) {
		tps80031_disable_rtc_write(dev);
		dev_err(dev->parent, "failed writing reg: %d\n", reg);
		WARN_ON(1);
		return ret;
	}
	tps80031_disable_rtc_write(dev);
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

static int tps80031_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u8 buff[7];
	int err;
	err = tps80031_read_regs(dev, RTC_SECONDS_REG, sizeof(buff), buff);
	if (err < 0) {
		dev_err(dev->parent, "failed reading time\n");
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
	return 0;
}

static int tps80031_rtc_stop(struct device *dev)
{
	int err;

	tps80031_enable_rtc_write(dev);
	err = tps80031_clr_bits(dev->parent, 1, RTC_CTRL, STOP_RTC);
	if (err < 0)
		dev_err(dev->parent, "failed to stop RTC. err: %d\n", err);
	tps80031_disable_rtc_write(dev);
	return err;
}

static int tps80031_rtc_start(struct device *dev)
{
	int err;

	tps80031_enable_rtc_write(dev);
	err = tps80031_set_bits(dev->parent, 1, RTC_CTRL, STOP_RTC);
	if (err < 0)
		dev_err(dev->parent, "failed to start RTC. err: %d\n", err);
	tps80031_disable_rtc_write(dev);
	return err;
}


static int tps80031_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	u8 buff[7];
	int err;

	buff[0] = tm->tm_sec;
	buff[1] = tm->tm_min;
	buff[2] = tm->tm_hour;
	buff[3] = tm->tm_mday;
	buff[4] = tm->tm_mon + 1;
	buff[5] = tm->tm_year % RTC_YEAR_OFFSET;
	buff[6] = tm->tm_wday;

	convert_decimal_to_bcd(buff, sizeof(buff));
	err = tps80031_rtc_stop(dev);
	if (err < 0)
		return err;

	err = tps80031_write_regs(dev, RTC_SECONDS_REG, sizeof(buff), buff);
	if (err < 0) {
		dev_err(dev->parent, "failed to program new time\n");
		return err;
	}

	err = tps80031_rtc_start(dev);
	return err;
}

static int tps80031_rtc_alarm_irq_enable(struct device *dev,
					 unsigned int enable)
{
	struct tps80031_rtc *rtc = dev_get_drvdata(dev);
	int err;
	struct device *p = dev->parent;

	if (rtc->irq == -1)
		return -EIO;

	if (enable) {
		if (rtc->alarm_irq_enabled)
			return 0;

		tps80031_enable_rtc_write(dev);
		err = tps80031_set_bits(p, 1, RTC_INT, ENABLE_ALARM_INT);
		tps80031_disable_rtc_write(dev);
		if (err < 0) {
			dev_err(p, "failed to set ALRM int. err: %d\n", err);
			return err;
		} else
			rtc->alarm_irq_enabled = 1;
	} else {
		if(!rtc->alarm_irq_enabled)
			return 0;
		tps80031_enable_rtc_write(dev);
		err = tps80031_clr_bits(p, 1, RTC_INT, ENABLE_ALARM_INT);
		tps80031_disable_rtc_write(dev);
		if (err < 0) {
			dev_err(p, "failed to clear ALRM int. err: %d\n", err);
			return err;
		} else
			rtc->alarm_irq_enabled = 0;
	}
	return 0;
}

static int tps80031_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct tps80031_rtc *rtc = dev_get_drvdata(dev);
	unsigned long seconds;
	u8 buff[6];
	int err;
	struct rtc_time tm;

	if (rtc->irq == -1)
		return -EIO;

	rtc_tm_to_time(&alrm->time, &seconds);
	tps80031_rtc_read_time(dev, &tm);
	rtc_tm_to_time(&tm, &rtc->epoch_start);

	if (WARN_ON(alrm->enabled && (seconds < rtc->epoch_start))) {
		dev_err(dev->parent, "can't set alarm to requested time\n");
		return -EINVAL;
	}

	buff[0] = alrm->time.tm_sec;
	buff[1] = alrm->time.tm_min;
	buff[2] = alrm->time.tm_hour;
	buff[3] = alrm->time.tm_mday;
	buff[4] = alrm->time.tm_mon + 1;
	buff[5] = alrm->time.tm_year % RTC_YEAR_OFFSET;
	convert_decimal_to_bcd(buff, sizeof(buff));
	err = tps80031_write_regs(dev, RTC_ALARM, sizeof(buff), buff);
	if (err)
		dev_err(dev->parent, "unable to program alarm\n");
	else
		err = tps80031_rtc_alarm_irq_enable(dev, alrm->enabled);

	return err;
}

static int tps80031_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	u8 buff[6];
	int err;

	err = tps80031_read_regs(dev, RTC_ALARM, sizeof(buff), buff);
	if (err)
		return err;
	convert_bcd_to_decimal(buff, sizeof(buff));

	alrm->time.tm_sec = buff[0];
	alrm->time.tm_min = buff[1];
	alrm->time.tm_hour = buff[2];
	alrm->time.tm_mday = buff[3];
	alrm->time.tm_mon = buff[4] - 1;
	alrm->time.tm_year = buff[5] + RTC_YEAR_OFFSET;

	return 0;
}

static const struct rtc_class_ops tps80031_rtc_ops = {
	.read_time = tps80031_rtc_read_time,
	.set_time = tps80031_rtc_set_time,
	.set_alarm = tps80031_rtc_set_alarm,
	.read_alarm = tps80031_rtc_read_alarm,
	.alarm_irq_enable = tps80031_rtc_alarm_irq_enable,
};

static irqreturn_t tps80031_rtc_irq(int irq, void *data)
{
	struct device *dev = data;
	struct tps80031_rtc *rtc = dev_get_drvdata(dev);
	u8 reg;
	int err;

	/* clear Alarm status bits.*/
	err = tps80031_read_regs(dev, RTC_STATUS, 1, &reg);
	if (err) {
		dev_err(dev->parent, "unable to read RTC_STATUS reg\n");
		return -EBUSY;
	}

	tps80031_enable_rtc_write(dev);
	err = tps80031_force_update(dev->parent, 1, RTC_STATUS,
		ALARM_INT_STATUS, ALARM_INT_STATUS);
	tps80031_disable_rtc_write(dev);
	if (err) {
		dev_err(dev->parent, "unable to set Alarm INT\n");
		return -EBUSY;
	}

	rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);
	return IRQ_HANDLED;
}

static int __devinit tps80031_rtc_probe(struct platform_device *pdev)
{
	struct tps80031_platform_data *tps80031_pdata;
	struct tps80031_rtc_platform_data *pdata;
	struct tps80031_rtc *rtc;
	struct rtc_time tm;
	int err;
	u8 reg;

	tps80031_pdata = dev_get_platdata(pdev->dev.parent);
	if (!tps80031_pdata) {
		dev_err(&pdev->dev, "no tps80031 platform_data specified\n");
		return -EINVAL;
	}

	pdata = tps80031_pdata->rtc_pdata;
	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data specified\n");
		return -EINVAL;
	}

	rtc = kzalloc(sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	rtc->irq = -1;
	if (pdata->irq < 0)
		dev_err(&pdev->dev, "no IRQ specified, wakeup is disabled\n");

	rtc->msecure_gpio = -1;
	if (gpio_is_valid(pdata->msecure_gpio)) {
		err = gpio_request(pdata->msecure_gpio, "tps80031 msecure");
		if (err == 0) {
			rtc->msecure_gpio = pdata->msecure_gpio;
			gpio_direction_output(rtc->msecure_gpio, 0);
		} else
			dev_warn(&pdev->dev, "could not get msecure GPIO\n");
	}

	device_init_wakeup(&pdev->dev, 1);
	rtc->rtc = rtc_device_register(pdev->name, &pdev->dev,
				       &tps80031_rtc_ops, THIS_MODULE);
	dev_set_drvdata(&pdev->dev, rtc);

	if (IS_ERR(rtc->rtc)) {
		err = PTR_ERR(rtc->rtc);
		goto fail;
	}

	if ((int)pdev && (int)&pdev->dev)
		err = tps80031_read_regs(&pdev->dev, RTC_STATUS, 1, &reg);
	else {
		dev_err(&pdev->dev, "%s Input params incorrect\n", __func__);
		err = -EBUSY;
		goto fail;
	}

	if (err) {
		dev_err(&pdev->dev, "%s unable to read status\n", __func__);
		err = -EBUSY;
		goto fail;
	}

	/* If RTC have POR values, set time using platform data*/
	tps80031_rtc_read_time(&pdev->dev, &tm);
	if ((tm.tm_year == RTC_YEAR_OFFSET + RTC_POR_YEAR) &&
		(tm.tm_mon == (RTC_POR_MONTH - 1)) &&
		(tm.tm_mday == RTC_POR_DAY)) {
		if (pdata->time.tm_year < 2000 ||
			pdata->time.tm_year > 2100) {
			dev_err(&pdev->dev, "Invalid platform data\n");
			memset(&pdata->time, 0, sizeof(pdata->time));
			pdata->time.tm_year = 2011;
			pdata->time.tm_mday = 1;
		}
		tps80031_rtc_set_time(&pdev->dev, &pdata->time);
	}

	reg = ALARM_INT_STATUS;
	err = tps80031_write_regs(&pdev->dev, RTC_STATUS, 1, &reg);
	if (err) {
		dev_err(&pdev->dev, "unable to program RTC_STATUS reg\n");
		return -EBUSY;
	}

	tps80031_enable_rtc_write(&pdev->dev);
	err = tps80031_set_bits(pdev->dev.parent, 1, RTC_INT, ENABLE_ALARM_INT);
	tps80031_disable_rtc_write(&pdev->dev);
	if (err) {
		dev_err(&pdev->dev, "unable to program Interrupt Mask reg\n");
		err = -EBUSY;
		rtc->alarm_irq_enabled = 0;
		goto fail;
	} else
		rtc->alarm_irq_enabled = 1;

	if (pdata && (pdata->irq >= 0)) {
		rtc->irq = pdata->irq;
		err = request_threaded_irq(pdata->irq, NULL, tps80031_rtc_irq,
					IRQF_ONESHOT, "rtc_tps80031",
					&pdev->dev);
		if (err) {
			dev_err(&pdev->dev, "request IRQ:%d fail\n", rtc->irq);
			rtc->irq = -1;
		} else {
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

static int __devexit tps80031_rtc_remove(struct platform_device *pdev)
{
	struct tps80031_rtc *rtc = dev_get_drvdata(&pdev->dev);

	if (rtc->irq != -1)
		free_irq(rtc->irq, rtc);
	rtc_device_unregister(rtc->rtc);
	kfree(rtc);
	return 0;
}

static struct platform_driver tps80031_rtc_driver = {
	.driver	= {
		.name	= "tps80031-rtc",
		.owner	= THIS_MODULE,
	},
	.probe	= tps80031_rtc_probe,
	.remove	= __devexit_p(tps80031_rtc_remove),
};

static int __init tps80031_rtc_init(void)
{
	return platform_driver_register(&tps80031_rtc_driver);
}
module_init(tps80031_rtc_init);

static void __exit tps80031_rtc_exit(void)
{
	platform_driver_unregister(&tps80031_rtc_driver);
}
module_exit(tps80031_rtc_exit);

MODULE_DESCRIPTION("TI TPS80031 RTC driver");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rtc_tps80031");

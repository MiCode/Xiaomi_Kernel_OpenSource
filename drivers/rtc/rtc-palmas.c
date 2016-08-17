/*
 * rtc-palmas.c -- Palmas Real Time Clock interface
 *
 * Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.
 * Author: Kasoju Mallikarjun <mkasoju@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mfd/palmas.h>

struct palmas_rtc {
	struct rtc_device	*rtc;
	/* To store the list of enabled interrupts */
	unsigned int irqstat;
	unsigned int irq;
};

/* Total number of RTC registers needed to set time*/
#define NUM_TIME_REGS	(PALMAS_YEARS_REG - PALMAS_SECONDS_REG + 1)

int palmas_rtc_read(struct palmas *palmas, unsigned int reg,
				unsigned int *dest)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	return regmap_read(palmas->regmap[RTC_SLAVE], addr, dest);
}

int palmas_rtc_write(struct palmas *palmas, unsigned int reg,
				unsigned int value)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	return regmap_write(palmas->regmap[RTC_SLAVE], addr, value);
}

int palmas_rtc_bulk_read(struct palmas *palmas, unsigned int reg,
				void *val, size_t val_count)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	return regmap_bulk_read(palmas->regmap[RTC_SLAVE], addr,
		val, val_count);
}

int palmas_rtc_bulk_write(struct palmas *palmas, unsigned int reg,
	const void *val, size_t val_count)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	return regmap_bulk_write(palmas->regmap[RTC_SLAVE], addr,
		val, val_count);
}

int palmas_rtc_update_bits(struct palmas *palmas, unsigned int reg,
				unsigned int mask, unsigned int val)
{
	unsigned int addr;
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	return regmap_update_bits(palmas->regmap[RTC_SLAVE], addr, mask, val);
}

static int palmas_rtc_alarm_irq_enable(struct device *dev, unsigned enabled)
{
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	u8 val = 0;

	dev_dbg(dev, "%s(): enabled %u\n", __func__, enabled);

	if (enabled)
		val = PALMAS_RTC_INTERRUPTS_REG_IT_ALARM;

	return palmas_rtc_write(palmas,
		PALMAS_RTC_INTERRUPTS_REG, val);
}

/*
 * Gets current palmas RTC time and date parameters.
 *
 * The RTC's time/alarm representation is not what gmtime(3) requires
 * Linux to use:
 *
 *  - Months are 1..12 vs Linux 0-11
 *  - Years are 0..99 vs Linux 1900..N (we assume 21st century)
 */
static int palmas_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	unsigned char rtc_data[NUM_TIME_REGS];
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	int ret;

	/* Copy RTC counting registers to static registers or latches */
	ret = palmas_rtc_update_bits(palmas, PALMAS_RTC_CTRL_REG,
		PALMAS_RTC_CTRL_REG_GET_TIME, PALMAS_RTC_CTRL_REG_GET_TIME);
	if (ret < 0) {
		dev_err(dev, "RTC CTRL reg update failed with err:%d\n", ret);
		return ret;
	}

	ret = palmas_rtc_bulk_read(palmas, PALMAS_SECONDS_REG,
		rtc_data, NUM_TIME_REGS);
	if (ret < 0) {
		dev_err(dev, "reading from RTC failed with err:%d\n", ret);
		return ret;
	}

	tm->tm_sec = bcd2bin(rtc_data[0]);
	tm->tm_min = bcd2bin(rtc_data[1]);
	tm->tm_hour = bcd2bin(rtc_data[2]);
	tm->tm_mday = bcd2bin(rtc_data[3]);
	tm->tm_mon = bcd2bin(rtc_data[4]) - 1;
	tm->tm_year = bcd2bin(rtc_data[5]) + 100;

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n",
		__func__, tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
	return ret;
}

static int palmas_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	unsigned char rtc_data[NUM_TIME_REGS];
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	int ret;

	rtc_data[0] = bin2bcd(tm->tm_sec);
	rtc_data[1] = bin2bcd(tm->tm_min);
	rtc_data[2] = bin2bcd(tm->tm_hour);
	rtc_data[3] = bin2bcd(tm->tm_mday);
	rtc_data[4] = bin2bcd(tm->tm_mon + 1);
	rtc_data[5] = bin2bcd(tm->tm_year - 100);

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n",
		__func__, tm->tm_year, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);

	/* Stop RTC while updating the RTC time registers */
	ret = palmas_rtc_update_bits(palmas, PALMAS_RTC_CTRL_REG,
		PALMAS_RTC_CTRL_REG_STOP_RTC, 0);
	if (ret < 0) {
		dev_err(dev, "RTC stop failed with err:%d\n", ret);
		return ret;
	}

	/* update all the time registers in one shot */
	ret = palmas_rtc_bulk_write(palmas, PALMAS_SECONDS_REG,
		rtc_data, NUM_TIME_REGS);
	if (ret < 0) {
		dev_err(dev, "rtc_set_time error %d\n", ret);
		return ret;
	}

	/* Start back RTC */
	ret = palmas_rtc_update_bits(palmas, PALMAS_RTC_CTRL_REG,
		PALMAS_RTC_CTRL_REG_STOP_RTC, 1);
	if (ret < 0)
		dev_err(dev, "RTC start failed with err:%d\n", ret);

	return ret;
}

/*
 * Gets current palmas RTC alarm time.
 */
static int palmas_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	unsigned char alarm_data[NUM_TIME_REGS];
	u32 int_val;
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	int ret;

	ret = palmas_rtc_bulk_read(palmas, PALMAS_ALARM_SECONDS_REG,
		alarm_data, NUM_TIME_REGS);
	if (ret < 0) {
		dev_err(dev, "rtc_read_alarm error %d\n", ret);
		return ret;
	}

	alm->time.tm_sec = bcd2bin(alarm_data[0]);
	alm->time.tm_min = bcd2bin(alarm_data[1]);
	alm->time.tm_hour = bcd2bin(alarm_data[2]);
	alm->time.tm_mday = bcd2bin(alarm_data[3]);
	alm->time.tm_mon = bcd2bin(alarm_data[4]) - 1;
	alm->time.tm_year = bcd2bin(alarm_data[5]) + 100;

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n", __func__,
		alm->time.tm_year, alm->time.tm_mon, alm->time.tm_mday,
		alm->time.tm_hour, alm->time.tm_min, alm->time.tm_sec);

	ret = palmas_rtc_read(palmas, PALMAS_RTC_INTERRUPTS_REG,
		&int_val);
	if (ret < 0)
		return ret;

	if (int_val & PALMAS_RTC_INTERRUPTS_REG_IT_ALARM)
		alm->enabled = 1;

	return ret;
}

static int palmas_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	unsigned char alarm_data[NUM_TIME_REGS];
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	int ret;

	dev_dbg(dev, "%s()\n", __func__);
	ret = palmas_rtc_alarm_irq_enable(dev, 0);
	if (ret)
		return ret;

	alarm_data[0] = bin2bcd(alm->time.tm_sec);
	alarm_data[1] = bin2bcd(alm->time.tm_min);
	alarm_data[2] = bin2bcd(alm->time.tm_hour);
	alarm_data[3] = bin2bcd(alm->time.tm_mday);
	alarm_data[4] = bin2bcd(alm->time.tm_mon + 1);
	alarm_data[5] = bin2bcd(alm->time.tm_year - 100);

	dev_dbg(dev, "%s() %d %d %d %d %d %d\n", __func__,
		alm->time.tm_year, alm->time.tm_mon, alm->time.tm_mday,
		alm->time.tm_hour, alm->time.tm_min, alm->time.tm_sec);

	/* update all the alarm registers in one shot */
	ret = palmas_rtc_bulk_write(palmas,
		PALMAS_ALARM_SECONDS_REG, alarm_data, NUM_TIME_REGS);
	if (ret) {
		dev_err(dev, "rtc_set_alarm error %d\n", ret);
		return ret;
	}

	if (alm->enabled)
		ret = palmas_rtc_alarm_irq_enable(dev, 1);

	return ret;
}

static irqreturn_t palmas_rtc_interrupt(int irq, void *rtc)
{
	struct device *dev = rtc;
	unsigned long events = 0;
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	struct palmas_rtc *palmas_rtc = dev_get_drvdata(dev);
	int ret;
	u32 rtc_reg;

	dev_dbg(dev, "RTC ISR\n");

	ret = palmas_rtc_read(palmas, PALMAS_RTC_STATUS_REG,
		&rtc_reg);
	if (ret)
		return IRQ_NONE;

	dev_dbg(dev, "RTC ISR status 0x%02x\n", rtc_reg);

	if (rtc_reg & PALMAS_RTC_STATUS_REG_ALARM)
		events = RTC_IRQF | RTC_AF;

	ret = palmas_rtc_write(palmas, PALMAS_RTC_STATUS_REG,
		rtc_reg);
	if (ret)
		return IRQ_NONE;

	/* Notify RTC core on event */
	rtc_update_irq(palmas_rtc->rtc, 1, events);

	return IRQ_HANDLED;
}

static struct rtc_class_ops palmas_rtc_ops = {
	.read_time	= palmas_rtc_read_time,
	.set_time	= palmas_rtc_set_time,
	.read_alarm	= palmas_rtc_read_alarm,
	.set_alarm	= palmas_rtc_set_alarm,
	.alarm_irq_enable = palmas_rtc_alarm_irq_enable,
};

static int __devinit palmas_rtc_probe(struct platform_device *pdev)
{
	struct palmas *palmas = NULL;
	struct palmas_rtc *palmas_rtc = NULL;
	struct palmas_platform_data *palmas_pdata;
	struct palmas_rtc_platform_data *rtc_pdata = NULL;
	int ret;
	u32 rtc_reg;

	palmas = dev_get_drvdata(pdev->dev.parent);

	palmas_rtc = devm_kzalloc(&pdev->dev, sizeof(struct palmas_rtc),
			GFP_KERNEL);
	if (!palmas_rtc) {
		dev_err(&pdev->dev, "Memory allocation failed.\n");
		return -ENOMEM;
	}

	palmas_pdata = dev_get_platdata(pdev->dev.parent);
	if (palmas_pdata)
		rtc_pdata = palmas_pdata->rtc_pdata;

	palmas->rtc = palmas_rtc;
	if (rtc_pdata && rtc_pdata->enable_charging) {
		int slave;
		unsigned int addr;
		int reg = 0;

		addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE,
				PALMAS_BACKUP_BATTERY_CTRL);
		slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);

		if (rtc_pdata->charging_current_ua < 100)
			reg = PALMAS_BACKUP_BATTERY_CTRL_BBS_BBC_LOW_ICHRG;

		ret = regmap_update_bits(palmas->regmap[slave], addr,
				PALMAS_BACKUP_BATTERY_CTRL_BBS_BBC_LOW_ICHRG,
				reg);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Battery backup control failed, e %d\n", ret);
			return ret;
		};

		ret = regmap_update_bits(palmas->regmap[slave], addr,
				PALMAS_BACKUP_BATTERY_CTRL_BB_CHG_EN,
				PALMAS_BACKUP_BATTERY_CTRL_BB_CHG_EN);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Battery backup charging enable failed, e %d\n",
				ret);
			return ret;
		}
	}

	ret = palmas_rtc_write(palmas, PALMAS_RTC_INTERRUPTS_REG, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "RTC_INTERRUPTS_REG write failed: %d\n",
				ret);
		return ret;
	}

	/* Clear pending interrupts */
	ret = palmas_rtc_read(palmas, PALMAS_RTC_STATUS_REG,
		&rtc_reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "rtc_read_status error %d\n", ret);
		return ret;
	}

	ret = palmas_rtc_write(palmas, PALMAS_RTC_STATUS_REG,
		rtc_reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "rtc_clear_interupt error %d\n", ret);
		return ret;
	}

	dev_dbg(&pdev->dev, "Enabling palmas-RTC.\n");
	rtc_reg = PALMAS_RTC_CTRL_REG_STOP_RTC;
	ret = palmas_rtc_write(palmas, PALMAS_RTC_CTRL_REG,
		rtc_reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "rtc_enable error %d\n", ret);
		return ret;
	}

	palmas_rtc->irq = platform_get_irq(pdev, 0);
	dev_dbg(&pdev->dev, "RTC interrupt %d\n", palmas_rtc->irq);
	ret = request_threaded_irq(palmas_rtc->irq, NULL,
		palmas_rtc_interrupt, IRQF_TRIGGER_LOW | IRQF_ONESHOT |
		IRQF_EARLY_RESUME,
		"palmas-rtc", &pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "IRQ is not free.\n");
		return ret;
	}
	device_init_wakeup(&pdev->dev, 1);
	platform_set_drvdata(pdev, palmas_rtc);

	palmas_rtc->rtc = rtc_device_register(pdev->name, &pdev->dev,
		&palmas_rtc_ops, THIS_MODULE);
	if (IS_ERR(palmas_rtc->rtc)) {
		ret = PTR_ERR(palmas_rtc->rtc);
		free_irq(palmas_rtc->irq, &pdev->dev);
		dev_err(&pdev->dev, "RTC device register: err %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * Disable all palmas RTC module interrupts.
 * Sets status flag to free.
 */
static int __devexit palmas_rtc_remove(struct platform_device *pdev)
{
	/* leave rtc running, but disable irqs */
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);

	palmas_rtc_alarm_irq_enable(&palmas->rtc->rtc->dev, 0);
	free_irq(palmas->irq, &pdev->dev);
	rtc_device_unregister(palmas->rtc->rtc);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int palmas_rtc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);

	if (device_may_wakeup(dev)) {
		int ret;
		struct rtc_wkalrm alm;

		enable_irq_wake(palmas->rtc->irq);
		ret = palmas_rtc_read_alarm(dev, &alm);
		if (!ret)
			dev_info(dev, "%s() alrm %d time %d %d %d %d %d %d\n",
				__func__, alm.enabled,
				alm.time.tm_year, alm.time.tm_mon,
				alm.time.tm_mday, alm.time.tm_hour,
				alm.time.tm_min, alm.time.tm_sec);
	}

	return 0;
}

static int palmas_rtc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);

	if (device_may_wakeup(dev)) {
		struct rtc_time tm;
		int ret;

		disable_irq_wake(palmas->rtc->irq);
		ret = palmas_rtc_read_time(dev, &tm);
		if (!ret)
			dev_info(dev, "%s() %d %d %d %d %d %d\n",
				__func__, tm.tm_year, tm.tm_mon, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	return 0;
}

static const struct dev_pm_ops palmas_rtc_pm_ops = {
	.suspend	= palmas_rtc_suspend,
	.resume		= palmas_rtc_resume,
};

#define DEV_PM_OPS     (&palmas_rtc_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

static struct platform_driver palmas_rtc_driver = {
	.probe		= palmas_rtc_probe,
	.remove		= __devexit_p(palmas_rtc_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "palmas-rtc",
		.pm	= DEV_PM_OPS,
	},
};

module_platform_driver(palmas_rtc_driver);
MODULE_ALIAS("platform:palmas_rtc");
MODULE_AUTHOR("Kasoju Mallikarjun <mkasoju@nvidia.com>");
MODULE_LICENSE("GPL v2");

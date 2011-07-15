/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtc.h>
#include <linux/mfd/pmic8058.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/rtc/rtc-pm8058.h>
#include <linux/pm_runtime.h>

#define PM8058_RTC_CTRL		0x1E8
	#define PM8058_RTC_ENABLE	BIT(7)
	#define PM8058_RTC_ALARM_ENABLE	BIT(1)
	#define PM8058_RTC_ABORT_ENABLE	BIT(0)
#define PM8058_RTC_ALARM_CTRL	0x1E9
	#define PM8058_RTC_ALARM_CLEAR	BIT(0)
#define PM8058_RTC_TEST		0x1F6
#define PM8058_RTC_READ_BASE	0x1EE
#define PM8058_RTC_WRITE_BASE	0x1EA
#define PM8058_RTC_ALARM_BASE	0x1F2

struct pm8058_rtc {
	struct rtc_device *rtc0;
	u8 rtc_ctrl_reg;
	int rtc_irq;
	int rtc_alarm_irq;
	struct pm8058_chip *pm_chip;
};

static int
pm8058_rtc_read_bytes(struct pm8058_rtc *rtc_dd, u8 *rtc_val, int base)
{
	int i, rc;

	/*
	 * Read the 32-bit RTC/Alarm Value.
	 * These values have to be read 8-bit at a time.
	 */
	for (i = 0; i < 4; i++) {
		rc = pm8058_read(rtc_dd->pm_chip, base + i, &rtc_val[i], 1);
		if (rc < 0) {
			pr_err("%s: PM8058 read failed\n", __func__);
			return rc;
		}
	}

	return 0;
}

static int
pm8058_rtc_write_bytes(struct pm8058_rtc *rtc_dd, u8 *rtc_val, int base)
{
	int i, rc;

	/*
	 * Write the 32-bit Value.
	 * These values have to be written 8-bit at a time.
	 */
	for (i = 0; i < 4; i++) {
		rc = pm8058_write(rtc_dd->pm_chip, base + i, &rtc_val[i], 1);
		if (rc < 0) {
			pr_err("%s: PM8058 read failed\n", __func__);
			return rc;
		}
	}

	return 0;
}

/*
 * Steps to write the RTC registers.
 * 1. Disable alarm if enabled.
 * 2. Write 0x00 to LSB.
 * 3. Write Byte[1], Byte[2], Byte[3] then Byte[0].
 * 4. Enable alarm if disabled earlier.
 */
#ifdef CONFIG_RTC_PM8058_WRITE_ENABLE
static int
pm8058_rtc0_set_time(struct device *dev, struct rtc_time *tm)
{
	int rc;
	unsigned long secs = 0;
	u8 value[4], reg = 0, alarm_enabled = 0, ctrl_reg = 0, i;
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);

	ctrl_reg = rtc_dd->rtc_ctrl_reg;

	rtc_tm_to_time(tm, &secs);

	value[0] = secs & 0xFF;
	value[1] = (secs >> 8) & 0xFF;
	value[2] = (secs >> 16) & 0xFF;
	value[3] = (secs >> 24) & 0xFF;

	pr_debug("%s: Seconds value to be written to RTC = %lu\n", __func__,
								secs);
	 /* Disable alarm before updating RTC */
	if (ctrl_reg & PM8058_RTC_ALARM_ENABLE) {
		alarm_enabled = 1;
		ctrl_reg &= ~PM8058_RTC_ALARM_ENABLE;
		rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_CTRL,
							&ctrl_reg, 1);
		if (rc < 0) {
			pr_err("%s: PM8058 write failed\n", __func__);
			return rc;
		}
	}

	/* Write Byte[1], Byte[2], Byte[3], Byte[0] */
	reg = 0;
	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_WRITE_BASE, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	for (i = 1; i < 4; i++) {
		rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_WRITE_BASE + i,
								&value[i], 1);
		if (rc < 0) {
			pr_err("%s:Write to RTC registers failed\n", __func__);
			return rc;
		}
	}

	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_WRITE_BASE,
							&value[0], 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	if (alarm_enabled) {
		ctrl_reg |= PM8058_RTC_ALARM_ENABLE;
		rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_CTRL,
							&ctrl_reg, 1);
		if (rc < 0) {
			pr_err("%s: PM8058 write failed\n", __func__);
			return rc;
		}
	}

	rtc_dd->rtc_ctrl_reg = ctrl_reg;

	return 0;
}
#endif

static int
pm8058_rtc0_read_time(struct device *dev, struct rtc_time *tm)
{
	int rc;
	u8 value[4], reg;
	unsigned long secs = 0;
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);

	rc = pm8058_rtc_read_bytes(rtc_dd, value, PM8058_RTC_READ_BASE);
	if (rc < 0) {
		pr_err("%s: RTC time read failed\n", __func__);
		return rc;
	}

	/*
	 * Read the LSB again and check if there has been a carry over.
	 * If there is, redo the read operation.
	 */
	rc = pm8058_read(rtc_dd->pm_chip, PM8058_RTC_READ_BASE, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 read failed\n", __func__);
		return rc;
	}

	if (unlikely(reg < value[0])) {
		rc = pm8058_rtc_read_bytes(rtc_dd, value,
						PM8058_RTC_READ_BASE);
		if (rc < 0) {
			pr_err("%s: RTC time read failed\n", __func__);
			return rc;
		}
	}

	secs = value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);

	rtc_time_to_tm(secs, tm);

	rc = rtc_valid_tm(tm);
	if (rc < 0) {
		pr_err("%s: Invalid time read from PMIC8058\n", __func__);
		return rc;
	}

	pr_debug("%s: secs = %lu, h::m:s == %d::%d::%d, d/m/y = %d/%d/%d\n",
		 __func__, secs, tm->tm_hour, tm->tm_min, tm->tm_sec,
		tm->tm_mday, tm->tm_mon, tm->tm_year);

	return 0;
}

static int
pm8058_rtc0_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	int rc;
	u8 value[4], reg;
	struct rtc_time rtc_tm;
	unsigned long secs_alarm, secs_rtc;
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);

	reg = rtc_dd->rtc_ctrl_reg;

	/* Check if the alarm is valid */
	rc = rtc_valid_tm(&alarm->time);
	if (rc < 0) {
		pr_err("%s: Alarm time invalid\n", __func__);
		return -EINVAL;
	}

	rtc_tm_to_time(&alarm->time, &secs_alarm);

	/*
	 * Read the current RTC time and verify if the alarm time is in the
	 * past. If yes, return invalid.
	 */
	rc = pm8058_rtc0_read_time(dev, &rtc_tm);
	if (rc) {
		pr_err("%s: Unable to read RTC time\n", __func__);
		return -EINVAL;
	}
	rtc_tm_to_time(&rtc_tm, &secs_rtc);

	if (secs_alarm < secs_rtc) {
		pr_err("%s: Trying to set alarm in the past\n", __func__);
		return -EINVAL;
	}

	value[0] = secs_alarm & 0xFF;
	value[1] = (secs_alarm >> 8) & 0xFF;
	value[2] = (secs_alarm >> 16) & 0xFF;
	value[3] = (secs_alarm >> 24) & 0xFF;

	rc = pm8058_rtc_write_bytes(rtc_dd, value, PM8058_RTC_ALARM_BASE);
	if (rc < 0) {
		pr_err("%s: Alarm could not be set\n", __func__);
		return rc;
	}

	reg = (alarm->enabled) ? (reg | PM8058_RTC_ALARM_ENABLE) :
					(reg & ~PM8058_RTC_ALARM_ENABLE);

	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_CTRL, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	rtc_dd->rtc_ctrl_reg = reg;

	pr_debug("%s: Alarm Set for h:r:s=%d:%d:%d, d/m/y=%d/%d/%d\n",
			__func__, alarm->time.tm_hour, alarm->time.tm_min,
				alarm->time.tm_sec, alarm->time.tm_mday,
				alarm->time.tm_mon, alarm->time.tm_year);

	return 0;
}

static int
pm8058_rtc0_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	int rc;
	u8 value[4], reg;
	unsigned long secs = 0;
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);

	reg = rtc_dd->rtc_ctrl_reg;

	alarm->enabled = !!(reg & PM8058_RTC_ALARM_ENABLE);

	rc = pm8058_rtc_read_bytes(rtc_dd, value,
					PM8058_RTC_ALARM_BASE);
	if (rc < 0) {
		pr_err("%s: RTC alarm time read failed\n", __func__);
		return rc;
	}

	secs = value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);

	rtc_time_to_tm(secs, &alarm->time);

	rc = rtc_valid_tm(&alarm->time);
	if (rc < 0) {
		pr_err("%s: Invalid time read from PMIC8058\n", __func__);
		return rc;
	}

	pr_debug("%s: Alarm set for - h:r:s=%d:%d:%d, d/m/y=%d/%d/%d\n",
			__func__, alarm->time.tm_hour, alarm->time.tm_min,
				alarm->time.tm_sec, alarm->time.tm_mday,
				alarm->time.tm_mon, alarm->time.tm_year);

	return 0;
}


static int
pm8058_rtc0_alarm_irq_enable(struct device *dev, unsigned int enable)
{
	int rc;
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);
	u8 reg;

	reg = rtc_dd->rtc_ctrl_reg;
	reg = (enable) ? (reg | PM8058_RTC_ALARM_ENABLE) :
				(reg & ~PM8058_RTC_ALARM_ENABLE);

	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_CTRL, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	rtc_dd->rtc_ctrl_reg = reg;

	return rc;
}

static struct rtc_class_ops pm8058_rtc0_ops = {
	.read_time	= pm8058_rtc0_read_time,
	.set_alarm	= pm8058_rtc0_set_alarm,
	.read_alarm	= pm8058_rtc0_read_alarm,
	.alarm_irq_enable = pm8058_rtc0_alarm_irq_enable,
};

static irqreturn_t pm8058_alarm_trigger(int irq, void *dev_id)
{
	u8 reg;
	int rc;
	unsigned long events = 0;
	struct pm8058_rtc *rtc_dd = dev_id;

	events = RTC_IRQF | RTC_AF;
	rtc_update_irq(rtc_dd->rtc0, 1, events);

	pr_debug("%s: Alarm Triggered !!\n", __func__);

	/* Clear the alarm enable bit */
	reg = rtc_dd->rtc_ctrl_reg;

	reg &= ~PM8058_RTC_ALARM_ENABLE;
	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_CTRL,
						&reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		goto rtc_alarm_handled;
	}

	rtc_dd->rtc_ctrl_reg = reg;

	/* Clear RTC alarm register */
	rc = pm8058_read(rtc_dd->pm_chip, PM8058_RTC_ALARM_CTRL, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 read failed\n", __func__);
		goto rtc_alarm_handled;
	}

	reg &= ~PM8058_RTC_ALARM_CLEAR;
	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_ALARM_CTRL, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		goto rtc_alarm_handled;
	}

rtc_alarm_handled:
	return IRQ_HANDLED;
}

static int __devinit pm8058_rtc_probe(struct platform_device *pdev)
{
	int rc;
	u8 reg, reg_alarm;
	struct pm8058_rtc *rtc_dd;
	struct pm8058_chip *pm_chip;

	pm_chip = platform_get_drvdata(pdev);
	if (pm_chip == NULL) {
		pr_err("%s: Invalid driver information\n", __func__);
		return -ENXIO;
	}

	rtc_dd = kzalloc(sizeof(*rtc_dd), GFP_KERNEL);
	if (rtc_dd == NULL) {
		pr_err("%s: Unable to allocate memory\n", __func__);
		return -ENOMEM;
	}

	/* Enable runtime PM ops, start in ACTIVE mode */
	rc = pm_runtime_set_active(&pdev->dev);
	if (rc < 0)
		dev_dbg(&pdev->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&pdev->dev);

	rtc_dd->rtc_irq = platform_get_irq(pdev, 0);
	rtc_dd->rtc_alarm_irq = platform_get_irq(pdev, 1);
	if (!rtc_dd->rtc_alarm_irq || !rtc_dd->rtc_irq) {
		pr_err("%s: RTC Alarm IRQ absent\n", __func__);
		rc = -ENXIO;
		goto fail_rtc_enable;
	}

	rtc_dd->pm_chip = pm_chip;

	rc = pm8058_read(pm_chip, PM8058_RTC_CTRL, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 read failed\n", __func__);
		goto fail_rtc_enable;
	}

	/* Enable RTC, ABORT enable and disable alarm */
	reg |= ((PM8058_RTC_ENABLE | PM8058_RTC_ABORT_ENABLE) &
			~PM8058_RTC_ALARM_ENABLE);

	rc = pm8058_write(pm_chip, PM8058_RTC_CTRL, &reg, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		goto fail_rtc_enable;
	}

	/* Clear RTC alarm control register */
	rc = pm8058_read(rtc_dd->pm_chip, PM8058_RTC_ALARM_CTRL,
							&reg_alarm, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 read failed\n", __func__);
		goto fail_rtc_enable;
	}

	reg_alarm &= ~PM8058_RTC_ALARM_CLEAR;
	rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_ALARM_CTRL,
							&reg_alarm, 1);
	if (rc < 0) {
		pr_err("%s: PM8058 write failed\n", __func__);
		goto fail_rtc_enable;
	}

	rtc_dd->rtc_ctrl_reg = reg;

#ifdef CONFIG_RTC_PM8058_WRITE_ENABLE
	pm8058_rtc0_ops.set_time	= pm8058_rtc0_set_time;
#endif

	/* Register the RTC device */
	rtc_dd->rtc0 = rtc_device_register("pm8058_rtc0", &pdev->dev,
				&pm8058_rtc0_ops, THIS_MODULE);
	if (IS_ERR(rtc_dd->rtc0)) {
		pr_err("%s: RTC device registration failed (%ld)\n",
					__func__, PTR_ERR(rtc_dd->rtc0));
		rc = PTR_ERR(rtc_dd->rtc0);
		goto fail_rtc_enable;
	}

	platform_set_drvdata(pdev, rtc_dd);

	/* Request the alarm IRQ */
	rc = request_threaded_irq(rtc_dd->rtc_alarm_irq, NULL,
				 pm8058_alarm_trigger, IRQF_TRIGGER_RISING,
				 "pm8058_rtc_alarm", rtc_dd);
	if (rc < 0) {
		pr_err("%s: Request IRQ failed (%d)\n", __func__, rc);
		goto fail_req_irq;
	}

	device_init_wakeup(&pdev->dev, 1);

	pr_debug("%s: Probe success !!\n", __func__);

	return 0;

fail_req_irq:
	rtc_device_unregister(rtc_dd->rtc0);
fail_rtc_enable:
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	kfree(rtc_dd);
	return rc;
}

#ifdef CONFIG_PM
static int pm8058_rtc_resume(struct device *dev)
{
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(rtc_dd->rtc_alarm_irq);

	return 0;
}

static int pm8058_rtc_suspend(struct device *dev)
{
	struct pm8058_rtc *rtc_dd = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(rtc_dd->rtc_alarm_irq);

	return 0;
}

static struct dev_pm_ops pm8058_rtc_pm_ops = {
	.suspend = pm8058_rtc_suspend,
	.resume = pm8058_rtc_resume,
};
#endif

static int __devexit pm8058_rtc_remove(struct platform_device *pdev)
{
	struct pm8058_rtc *rtc_dd = platform_get_drvdata(pdev);

	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	device_init_wakeup(&pdev->dev, 0);
	free_irq(rtc_dd->rtc_alarm_irq, rtc_dd);
	rtc_device_unregister(rtc_dd->rtc0);
	kfree(rtc_dd);

	return 0;
}

static void pm8058_rtc_shutdown(struct platform_device *pdev)
{
	u8 reg;
	int rc, i;
	bool rtc_alarm_powerup = false;
	struct pm8058_rtc *rtc_dd = platform_get_drvdata(pdev);
	struct pm8058_rtc_platform_data *pdata = pdev->dev.platform_data;

	if (pdata != NULL)
		rtc_alarm_powerup =  pdata->rtc_alarm_powerup;

	if (!rtc_alarm_powerup) {

		dev_dbg(&pdev->dev, "Disabling alarm interrupts\n");

		/* Disable RTC alarms */
		reg = rtc_dd->rtc_ctrl_reg;
		reg &= ~PM8058_RTC_ALARM_ENABLE;
		rc = pm8058_write(rtc_dd->pm_chip, PM8058_RTC_CTRL, &reg, 1);
		if (rc < 0) {
			pr_err("%s: PM8058 write failed\n", __func__);
			return;
		}

		/* Clear Alarm register */
		reg = 0x0;
		for (i = 0; i < 4; i++) {
			rc = pm8058_write(rtc_dd->pm_chip,
					PM8058_RTC_ALARM_BASE + i, &reg, 1);
			if (rc < 0) {
				pr_err("%s: PM8058 write failed\n", __func__);
				return;
			}
		}

	}
}

static struct platform_driver pm8058_rtc_driver = {
	.probe		= pm8058_rtc_probe,
	.remove		= __devexit_p(pm8058_rtc_remove),
	.shutdown	= pm8058_rtc_shutdown,
	.driver	= {
		.name	= "pm8058-rtc",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm8058_rtc_pm_ops,
#endif
	},
};

static int __init pm8058_rtc_init(void)
{
	return platform_driver_register(&pm8058_rtc_driver);
}

static void __exit pm8058_rtc_exit(void)
{
	platform_driver_unregister(&pm8058_rtc_driver);
}

module_init(pm8058_rtc_init);
module_exit(pm8058_rtc_exit);

MODULE_ALIAS("platform:pm8058-rtc");
MODULE_DESCRIPTION("PMIC8058 RTC driver");
MODULE_LICENSE("GPL v2");

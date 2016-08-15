/*
 * An RTC driver for the NVIDIA Tegra 200 series internal RTC.
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 * Copyright (c) 2010 Jon Mayo <jmayo@nvidia.com>
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

/* set to 1 = busy every eight 32kHz clocks during copy of sec+msec to AHB */
#define TEGRA_RTC_REG_BUSY			0x004
#define TEGRA_RTC_REG_SECONDS			0x008
/* when msec is read, the seconds are buffered into shadow seconds. */
#define TEGRA_RTC_REG_SHADOW_SECONDS		0x00c
#define TEGRA_RTC_REG_MILLI_SECONDS		0x010
#define TEGRA_RTC_REG_SECONDS_ALARM0		0x014
#define TEGRA_RTC_REG_SECONDS_ALARM1		0x018
#define TEGRA_RTC_REG_MILLI_SECONDS_ALARM0	0x01c
#define TEGRA_RTC_REG_SECONDS_CDN_ALARM0        0x020
#define TEGRA_RTC_REG_INTR_MASK			0x028
/* write 1 bits to clear status bits */
#define TEGRA_RTC_REG_INTR_STATUS		0x02c

/* bits in INTR_MASK */
#define TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM	(1<<3)
#define TEGRA_RTC_INTR_MASK_MSEC_ALARM		(1<<2)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM1		(1<<1)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM0		(1<<0)

/* bits in INTR_STATUS */
#define TEGRA_RTC_INTR_STATUS_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_STATUS_SEC_CDN_ALARM	(1<<3)
#define TEGRA_RTC_INTR_STATUS_MSEC_ALARM	(1<<2)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM1	(1<<1)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM0	(1<<0)

/* bits in SECONDS_CDN_ALARM0 */
#define TEGRA_RTC_SECONDS_CDN_ALARM0_ENABLE	(1<<31)
#define TEGRA_RTC_SECONDS_CDN_ALARM0_REPEAT	(1<<30)

struct tegra_rtc_info {
	struct platform_device	*pdev;
	struct rtc_device	*rtc_dev;
	void __iomem		*rtc_base; /* NULL if not initialized. */
	int			tegra_rtc_irq; /* alarm and periodic irq */
	spinlock_t		tegra_rtc_lock;
};
struct tegra_rtc_info *info;

/* RTC hardware is busy when it is updating its values over AHB once
 * every eight 32kHz clocks (~250uS).
 * outside of these updates the CPU is free to write.
 * CPU is always free to read.
 */
static inline u32 tegra_rtc_check_busy(struct tegra_rtc_info *info)
{
	return readl(info->rtc_base + TEGRA_RTC_REG_BUSY) & 1;
}

/* Wait for hardware to be ready for writing.
 * This function tries to maximize the amount of time before the next update.
 * It does this by waiting for the RTC to become busy with its periodic update,
 * then returning once the RTC first becomes not busy.
 * This periodic update (where the seconds and milliseconds are copied to the
 * AHB side) occurs every eight 32kHz clocks (~250uS).
 * The behavior of this function allows us to make some assumptions without
 * introducing a race, because 250uS is plenty of time to read/write a value.
 */
static int tegra_rtc_wait_while_busy(struct device *dev)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);

	int retries = 500; /* ~490 us is the worst case, ~250 us is best. */

	/* first wait for the RTC to become busy. this is when it
	 * posts its updated seconds+msec registers to AHB side. */
	while (tegra_rtc_check_busy(info)) {
		if (!retries--)
			goto retry_failed;
		udelay(1);
	}

	/* now we have about 250 us to manipulate registers */
	return 0;

retry_failed:
	dev_err(dev, "write failed:retry count exceeded.\n");
	return -ETIMEDOUT;
}


static int tegra_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned status;
	unsigned long sl_irq_flags;

	tegra_rtc_wait_while_busy(dev);
	spin_lock_irqsave(&info->tegra_rtc_lock, sl_irq_flags);

	/* read the original value, and OR in the flag. */
	status = readl(info->rtc_base + TEGRA_RTC_REG_INTR_MASK);
	if (enabled)
		status |= TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM; /* set it */
	else
		status &= ~TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM; /* clear it */

	writel(status, info->rtc_base + TEGRA_RTC_REG_INTR_MASK);

	spin_unlock_irqrestore(&info->tegra_rtc_lock, sl_irq_flags);

	return 0;
}


void tegra_rtc_set_countdown(int sec)
{
	unsigned long val;
	if (!info)
		return;

	tegra_rtc_wait_while_busy(&info->pdev->dev);
	val = (sec) ? (TEGRA_RTC_SECONDS_CDN_ALARM0_ENABLE | sec) : 0;
	writel(val, info->rtc_base + TEGRA_RTC_REG_SECONDS_CDN_ALARM0);
	dev_vdbg(&info->pdev->dev, "alarm read back as %x\n",
		readl(info->rtc_base + TEGRA_RTC_REG_SECONDS_CDN_ALARM0));

	/* if successfully written and alarm is enabled ... */
	if (sec) {
		tegra_rtc_alarm_irq_enable(&info->pdev->dev, 1);
	} else {
		tegra_rtc_alarm_irq_enable(&info->pdev->dev, 0);
	}

	return;
}

static irqreturn_t tegra_rtc_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned status;
	unsigned long sl_irq_flags;

	status = readl(info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	if (status) {
		/* clear the interrupt masks and status on any irq. */
		tegra_rtc_wait_while_busy(dev);
		spin_lock_irqsave(&info->tegra_rtc_lock, sl_irq_flags);
		writel(0, info->rtc_base + TEGRA_RTC_REG_INTR_MASK);
		writel(status, info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
		spin_unlock_irqrestore(&info->tegra_rtc_lock, sl_irq_flags);
	}

	return IRQ_HANDLED;
}


static int __devinit tegra_rtc_probe(struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	info = kzalloc(sizeof(struct tegra_rtc_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"Unable to allocate resources for device.\n");
		ret = -EBUSY;
		goto err_free_info;
	}

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev,
			"Unable to request mem region for device.\n");
		ret = -EBUSY;
		goto err_free_info;
	}

	info->tegra_rtc_irq = platform_get_irq(pdev, 0);
	if (info->tegra_rtc_irq <= 0) {
		ret = -EBUSY;
		goto err_release_mem_region;
	}

	info->rtc_base = ioremap_nocache(res->start, resource_size(res));
	if (!info->rtc_base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device.\n");
		ret = -EBUSY;
		goto err_release_mem_region;
	}

	/* set context info. */
	info->pdev = pdev;
	spin_lock_init(&info->tegra_rtc_lock);

	platform_set_drvdata(pdev, info);

	/* clear out the hardware. */
	writel(0, info->rtc_base + TEGRA_RTC_REG_SECONDS_CDN_ALARM0);
	writel(0xffffffff, info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	writel(0, info->rtc_base + TEGRA_RTC_REG_INTR_MASK);

	device_init_wakeup(&pdev->dev, 1);

	ret = request_irq(info->tegra_rtc_irq, tegra_rtc_irq_handler,
		IRQF_TRIGGER_HIGH, "rtc alarm", &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev,
			"Unable to request interrupt for device (err=%d).\n",
			ret);
		goto err_dev_unreg;
	}

	dev_notice(&pdev->dev, "Tegra internal Real Time Clock\n");

	return 0;

err_dev_unreg:
	iounmap(info->rtc_base);
err_release_mem_region:
	release_mem_region(res->start, resource_size(res));
err_free_info:
	kfree(info);

	return ret;
}

static int __devexit tegra_rtc_remove(struct platform_device *pdev)
{
	struct tegra_rtc_info *info = platform_get_drvdata(pdev);
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EBUSY;

	free_irq(info->tegra_rtc_irq, &pdev->dev);
	iounmap(info->rtc_base);
	release_mem_region(res->start, resource_size(res));
	kfree(info);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct tegra_rtc_info *info = platform_get_drvdata(pdev);

	tegra_rtc_wait_while_busy(dev);

	/* only use SEC_CDN_ALARM as a wake source. */
	writel(0xffffffff, info->rtc_base + TEGRA_RTC_REG_INTR_STATUS);
	writel(TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM,
		info->rtc_base + TEGRA_RTC_REG_INTR_MASK);

	dev_dbg(dev, "alarm sec = %x\n",
		readl(info->rtc_base + TEGRA_RTC_REG_SECONDS_CDN_ALARM0));

	dev_vdbg(dev, "Suspend (device_may_wakeup=%d) irq:%d\n",
		device_may_wakeup(dev), info->tegra_rtc_irq);

	/* leave the alarms on as a wake source. */
	if (device_may_wakeup(dev))
		enable_irq_wake(info->tegra_rtc_irq);

	return 0;
}

static int tegra_rtc_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tegra_rtc_info *info = platform_get_drvdata(pdev);

	dev_vdbg(dev, "Resume (device_may_wakeup=%d)\n",
		device_may_wakeup(dev));
	/* alarms were left on as a wake source, turn them off. */
	if (device_may_wakeup(dev))
		disable_irq_wake(info->tegra_rtc_irq);

	return 0;
}
#endif

static void tegra_rtc_shutdown(struct platform_device *pdev)
{
	dev_vdbg(&pdev->dev, "disabling interrupts.\n");
	tegra_rtc_alarm_irq_enable(&pdev->dev, 0);
}

MODULE_ALIAS("platform:tegra_rtc");
static struct platform_driver tegra_rtc_driver = {
	.remove		= __devexit_p(tegra_rtc_remove),
	.shutdown	= tegra_rtc_shutdown,
	.driver		= {
		.name	= "tegra_rtc",
		.owner	= THIS_MODULE,
	},
#ifdef CONFIG_PM
	.suspend	= tegra_rtc_suspend,
	.resume		= tegra_rtc_resume,
#endif
};

static int __init tegra_rtc_init(void)
{
	return platform_driver_probe(&tegra_rtc_driver, tegra_rtc_probe);
}
module_init(tegra_rtc_init);

static void __exit tegra_rtc_exit(void)
{
	platform_driver_unregister(&tegra_rtc_driver);
}
module_exit(tegra_rtc_exit);

MODULE_AUTHOR("Jon Mayo <jmayo@nvidia.com>");
MODULE_DESCRIPTION("driver for Tegra internal RTC");
MODULE_LICENSE("GPL");

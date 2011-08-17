/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>

/* Early-suspend level */
#define LED_SUSPEND_LEVEL 1
#endif

#define PDM_DUTY_MAXVAL BIT(16)
#define PDM_DUTY_REFVAL BIT(15)

struct pdm_led_data {
	struct led_classdev cdev;
	void __iomem *perph_base;
	int pdm_offset;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static void msm_led_brightness_set_percent(struct pdm_led_data *led,
						int duty_per)
{
	u16 duty_val;

	duty_val = PDM_DUTY_REFVAL - ((PDM_DUTY_MAXVAL * duty_per) / 100);

	if (!duty_per)
		duty_val--;

	writel_relaxed(duty_val, led->perph_base + led->pdm_offset);
}

static void msm_led_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct pdm_led_data *led =
		container_of(led_cdev, struct pdm_led_data, cdev);

	msm_led_brightness_set_percent(led, (value * 100) / LED_FULL);
}

#ifdef CONFIG_PM_SLEEP
static int msm_led_pdm_suspend(struct device *dev)
{
	struct pdm_led_data *led = dev_get_drvdata(dev);

	msm_led_brightness_set_percent(led, 0);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void msm_led_pdm_early_suspend(struct early_suspend *h)
{
	struct pdm_led_data *led = container_of(h,
			struct pdm_led_data, early_suspend);

	msm_led_pdm_suspend(led->cdev.dev->parent);
}

#endif

static const struct dev_pm_ops msm_led_pdm_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= msm_led_pdm_suspend,
#endif
};
#endif

static int __devinit msm_pdm_led_probe(struct platform_device *pdev)
{
	const struct led_info *pdata = pdev->dev.platform_data;
	struct pdm_led_data *led;
	struct resource *res, *ioregion;
	u32 tcxo_pdm_ctl;
	int rc;

	if (!pdata) {
		pr_err("platform data is invalid\n");
		return -EINVAL;
	}

	if (pdev->id > 2) {
		pr_err("pdm id is invalid\n");
		return -EINVAL;
	}

	led = kzalloc(sizeof(struct pdm_led_data), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	/* Enable runtime PM ops, start in ACTIVE mode */
	rc = pm_runtime_set_active(&pdev->dev);
	if (rc < 0)
		dev_dbg(&pdev->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&pdev->dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("get resource failed\n");
		rc = -EINVAL;
		goto err_get_res;
	}

	ioregion = request_mem_region(res->start, resource_size(res),
						pdev->name);
	if (!ioregion) {
		pr_err("request for mem region failed\n");
		rc = -ENOMEM;
		goto err_get_res;
	}

	led->perph_base = ioremap(res->start, resource_size(res));
	if (!led->perph_base) {
		pr_err("ioremap failed\n");
		rc = -ENOMEM;
		goto err_ioremap;
	}

	/* Pulse Density Modulation(PDM) ids start with 0 and
	 * every PDM register takes 4 bytes
	 */
	led->pdm_offset = ((pdev->id) + 1) * 4;

	/* program tcxo_pdm_ctl register to enable pdm*/
	tcxo_pdm_ctl = readl_relaxed(led->perph_base);
	tcxo_pdm_ctl |= (1 << pdev->id);
	writel_relaxed(tcxo_pdm_ctl, led->perph_base);

	/* Start with LED in off state */
	msm_led_brightness_set_percent(led, 0);

	led->cdev.brightness_set = msm_led_brightness_set;
	led->cdev.name = pdata->name ? : "leds-msm-pdm";

	rc = led_classdev_register(&pdev->dev, &led->cdev);
	if (rc) {
		pr_err("led class registration failed\n");
		goto err_led_reg;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	led->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						LED_SUSPEND_LEVEL;
	led->early_suspend.suspend = msm_led_pdm_early_suspend;
	register_early_suspend(&led->early_suspend);
#endif

	platform_set_drvdata(pdev, led);
	return 0;

err_led_reg:
	iounmap(led->perph_base);
err_ioremap:
	release_mem_region(res->start, resource_size(res));
err_get_res:
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	kfree(led);
	return rc;
}

static int __devexit msm_pdm_led_remove(struct platform_device *pdev)
{
	struct pdm_led_data *led = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&led->early_suspend);
#endif
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	led_classdev_unregister(&led->cdev);
	msm_led_brightness_set_percent(led, 0);
	iounmap(led->perph_base);
	release_mem_region(res->start, resource_size(res));
	kfree(led);

	return 0;
}

static struct platform_driver msm_pdm_led_driver = {
	.probe		= msm_pdm_led_probe,
	.remove		= __devexit_p(msm_pdm_led_remove),
	.driver		= {
		.name	= "leds-msm-pdm",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm	= &msm_led_pdm_pm_ops,
#endif
	},
};

static int __init msm_pdm_led_init(void)
{
	return platform_driver_register(&msm_pdm_led_driver);
}
module_init(msm_pdm_led_init);

static void __exit msm_pdm_led_exit(void)
{
	platform_driver_unregister(&msm_pdm_led_driver);
}
module_exit(msm_pdm_led_exit);

MODULE_DESCRIPTION("MSM PDM LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:leds-msm-pdm");

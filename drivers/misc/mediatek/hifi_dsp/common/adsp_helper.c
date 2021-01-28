/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include "mtk_hifixdsp_common.h"
#include "adsp_helper.h"
#include "adsp_clk.h"
#include "adsp_ipi.h"

#define IPI_WAKEUP_TIME 1000
static struct adsp_chip_info *adsp_info;
struct wakeup_source ipi_wakeup_lock;
unsigned int is_from_suspend;

void *get_adsp_chip_data(void)
{
	return (void *)adsp_info;
}

static int adsp_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	void *data;
	struct adsp_private_data *pri_data;
	struct device *dev = &pdev->dev;

	data = devm_kzalloc(dev, sizeof(struct adsp_chip_info),
				GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	pri_data = devm_kzalloc(dev, sizeof(struct adsp_private_data),
				GFP_KERNEL);
	if (!pri_data) {
		devm_kfree(dev, data);
		return -ENOMEM;
	}

	adsp_info = data;
	adsp_info->data = pri_data;
	adsp_info->data->dev = dev;

	ret = platform_parse_resource(pdev, data);
	if (ret) {
		dev_err(dev, "platform_parse_resource failed.\n");
		goto tail;
	}

	ret = platform_parse_clock(dev, data);
	if (ret) {
		dev_err(dev, "platform_parse_clock failed.\n");
		goto tail;
	}

	ret = adsp_must_setting_early(dev);
	if (ret) {
		dev_err(dev, "adsp_must_setting_early failed.\n");
		goto tail;
	}

	ret = adsp_shared_base_ioremap(pdev, data);
	if (ret) {
		dev_err(dev, "adsp_shared_base_ioremap failed.\n");
		goto tail;
	}

	ret = adsp_ipi_device_init(pdev);
	if (ret) {
		dev_err(dev, "adsp_ipi_device_init failed.\n");
		goto tail;
	}

	ret = adsp_wdt_device_init(pdev);
	if (ret) {
		dev_err(dev, "adsp_wdt_device_init failed.\n");
		goto tail;
	}

	/* device attributes for debugging */
	ret = adsp_create_sys_files(dev);
	if (unlikely(ret != 0))
		dev_err(dev, "[ADSP] create sys-debug files failed.\n");

	/* init wakeup souce */
	wakeup_source_init(&ipi_wakeup_lock, "ipi_wakeup_lock");

tail:
	if (ret) {
		devm_kfree(dev, pri_data);
		devm_kfree(dev, data);
		adsp_info = NULL;
	}
	return ret;
}

static int adsp_device_remove(struct platform_device *pdev)
{
	/* Release ADSP reset-pin */
	hifixdsp_shutdown();

	/* Close ADSP clock and power-domains */
	adsp_clock_power_off(&pdev->dev);
	adsp_pm_unregister_last(&pdev->dev);

	adsp_ipi_device_remove(pdev);
	adsp_wdt_device_remove(pdev);
	adsp_destroy_sys_files(&pdev->dev);

	return 0;
}

static int __maybe_unused adsp_sleep_suspend(struct device *dev)
{
	/* add adsp ipi wake up souce */
	if (hifixdsp_run_status() == 1) {
		pr_notice("audio_ipi_pm_resume is suspend, set is_from_suspend to 1!\n");
		is_from_suspend = 1;
	}
	return 0;
}

static int __maybe_unused adsp_sleep_resume(struct device *dev)
{
	/* add adsp ipi wake up souce */
	if (hifixdsp_run_status() == 1) {
		pr_notice("audio_ipi_pm_resume is resume!\n");
		__pm_wakeup_event(&ipi_wakeup_lock, IPI_WAKEUP_TIME);
	}
	return 0;
}

static int __maybe_unused adsp_runtime_suspend(struct device *dev)
{
	/* nothing */
	return 0;
}

static int __maybe_unused adsp_runtime_resume(struct device *dev)
{
	/* nothing */
	return 0;
}

static const struct of_device_id adsp_of_ids[] = {
	{ .compatible = "mediatek,audio_dsp", },
	{}
};

static const struct dev_pm_ops adsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(adsp_sleep_suspend, adsp_sleep_resume)
	SET_RUNTIME_PM_OPS(adsp_runtime_suspend, adsp_runtime_resume, NULL)
};

static struct platform_driver mtk_adsp_driver = {
	.probe = adsp_device_probe,
	.remove = adsp_device_remove,
	.driver = {
		.name = "adsp",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = adsp_of_ids,
#endif
#ifdef CONFIG_PM
		.pm = &adsp_pm_ops,
#endif
	},
};

/*
 * ADSP driver initialization entry point.
 */
module_platform_driver(mtk_adsp_driver);


static int __init adsp_module_init(void)
{
	int ret = 0;

	pr_debug("[ADSP] %s(+)\n", __func__);

	/*
	 * adsp-module-init()
	 * You can add some initial items as listed below.
	 * But you should pay attention to initialization dependencies.
	 */
	//adsp_trax_init();

	pr_debug("[ADSP] %s(-)\n", __func__);

	return ret;
}

static void __exit adsp_module_exit(void)
{
	/*
	 * adsp-module-uninit()
	 * You can add some un-initial items as listed below.
	 */
}

module_init(adsp_module_init);
module_exit(adsp_module_exit);


MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("dehui.sun@mediatek.com");
MODULE_DESCRIPTION("HIFIxDSP platform driver");


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


static struct adsp_chip_info *adsp_info;


void *get_adsp_chip_data(void)
{
	return (void *)adsp_info;
}

void adsp_schedule_work(struct adsp_work_struct *adsp_ws)
{
	struct adsp_chip_info *adsp = get_adsp_chip_data();

	queue_work(adsp->pri_data->adsp_wq, &adsp_ws->work);
}

static int adsp_system_sleep_suspend(struct device *dev)
{
	/* code will be added later */
	return 0;
}

static int adsp_system_sleep_resume(struct device *dev)
{
	/* code will be added later */
	return 0;
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
	adsp_info->pri_data = pri_data;

	ret = platform_parse_resource(pdev, data);
	if (ret) {
		dev_err(dev, "platform_parse_resource failed.\n");
		goto tail;
	}

	ret = adsp_must_setting_early();
	if (ret) {
		dev_err(dev, "adsp_necessary_early_setting failed.\n");
		goto tail;
	}

	ret = adsp_shared_base_ioremap(pdev, data);
	if (ret) {
		dev_err(dev, "adsp_base_memory_ioremap failed.\n");
		goto tail;
	}

	/* device attributes for debugging */
	ret = adsp_create_sys_files(dev);
	if (unlikely(ret != 0)) {
		dev_err(dev, "[ADSP] create sys-debug files failed.\n");
		goto tail;
	}

	ret = adsp_ipi_device_init(pdev);
	if (unlikely(ret != 0)) {
		dev_err(dev, "[ADSP] adsp ipi init failed.\n");
		goto tail;
	}
tail:
	if (ret) {
		devm_kfree(dev, pri_data);
		devm_kfree(dev, data);
	}
	return ret;
}

static int adsp_device_remove(struct platform_device *pdev)
{
	adsp_ipi_device_remove(pdev);
	adsp_destroy_sys_files(&pdev->dev);
	return 0;
}

static const struct of_device_id adsp_of_ids[] = {
	{ .compatible = "mediatek,audio_dsp", },
	{}
};

static const struct dev_pm_ops adsp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		adsp_system_sleep_suspend,
		adsp_system_sleep_resume)
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
static int __init adsp_platform_init(void)
{
	int ret = 0;

	pr_debug("[ADSP] %s(+)\n", __func__);

	ret = platform_driver_register(&mtk_adsp_driver);
	if (ret) {
		pr_err("[ADSP] Unable to register platform driver!\n");
		goto TAIL;
	}

	pr_debug("[ADSP] %s(-)\n", __func__);
TAIL:
	return ret;
}

static void __exit adsp_platform_exit(void)
{
	platform_driver_unregister(&mtk_adsp_driver);
}

subsys_initcall(adsp_platform_init);
module_exit(adsp_platform_exit);


static int __init adsp_module_init(void)
{
	int ret = 0;
	struct adsp_chip_info *adsp;

	adsp = get_adsp_chip_data();
	if (!adsp) {
		ret = -1;
		goto TAIL;
	}

	pr_debug("[ADSP] %s(+)\n", __func__);

	adsp->pri_data->adsp_wq = create_workqueue("ADSP_WQ");
	if (!adsp->pri_data->adsp_wq) {
		pr_err("[ADSP] fail to create workqueue.\n");
		ret = -ENOMEM;
		goto TAIL;
	}

	/*
	 * adsp-module-init()
	 * You can add some initial items as listed below.
	 * But you should pay attention to initialization dependencies.
	 */
	//adsp_trax_init();

	pr_debug("[ADSP] %s(-)\n", __func__);
TAIL:
	return ret;
}

static void __exit adsp_module_exit(void)
{
	struct adsp_chip_info *adsp;

	adsp = get_adsp_chip_data();
	if (!adsp)
		return;

	flush_workqueue(adsp->pri_data->adsp_wq);
	destroy_workqueue(adsp->pri_data->adsp_wq);
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


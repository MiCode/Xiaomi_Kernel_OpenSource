// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/suspend.h>

static struct regmap *g_regmap;

static irqreturn_t ot_int_handler(int irq, void *data)
{
	int ret, irq_index = (uintptr_t)data;

	pr_info("%s with irq index=%d\n", __func__, irq_index);
	ret = regmap_update_bits(g_regmap,
				 PMIC_RG_RSV_SWREG_ADDR,
				 1 << irq_index,
				 1 << irq_index);
	if (ret)
		pr_info("%s error\n", __func__);
	/*
	 * TODO: After kernel-4.19, pm_mutex change to
	 * system_transition_mutex.
	 */
	if (mutex_trylock(&pm_mutex)) {
		kernel_power_off();
		mutex_unlock(&pm_mutex);
	}
	return IRQ_HANDLED;
}


static int mt635x_ot_debug_probe(struct platform_device *pdev)
{
	struct mt6358_chip *pmic = dev_get_drvdata(pdev->dev.parent);
	int i = 0, virq = 0, num_irqs = 0, ret = 0;

	g_regmap = pmic->regmap;
	if (!g_regmap)
		return -ENODEV;

	num_irqs = platform_irq_count(pdev);
	if (num_irqs <= 0)
		return num_irqs;

	for (i = 0; i < num_irqs; i++) {
		virq = platform_get_irq(pdev, i);
		if (virq <= 0) {
			dev_notice(&pdev->dev, "get virq fail\n");
			continue;
		}
		ret = devm_request_threaded_irq(&pdev->dev, virq, NULL,
						ot_int_handler,
						IRQF_TRIGGER_NONE,
						"PMIC_OT",
						(void *)(uintptr_t)i);
		if (ret < 0)
			dev_notice(&pdev->dev, "request irq fail\n");
	}
	dev_info(&pdev->dev, "%s\n", __func__);

	return ret;
}

static const struct of_device_id mt635x_ot_debug_of_match[] = {
	{
		.compatible = "mediatek,mt635x-ot-debug",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt635x_ot_debug_of_match);

static struct platform_driver mt635x_ot_debug_driver = {
	.driver = {
		.name = "mt635x-ot-debug",
		.of_match_table = mt635x_ot_debug_of_match,
	},
	.probe	= mt635x_ot_debug_probe,
};
module_platform_driver(mt635x_ot_debug_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("PMIC Over Thermal Debug driver for MediaTek MT635x PMIC");

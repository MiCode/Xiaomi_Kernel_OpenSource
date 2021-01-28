// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/regulator/driver.h>

#include "../inc/mt6360_pmu.h"
#include "../inc/mt6360_pmu_chg.h"
#include <charger_class.h>


#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_boot.h>

#define MT6360_PMU_CHG_DRV_VERSION	"1.0.7_MTK"

static int mt6360_pmu_dts_probe(struct platform_device *pdev)
{
	dev_err(&pdev->dev, "mt6360_pmu_dts_probed\n");
	return 0;
}

static int mt6360_pmu_dts_remove(struct platform_device *pdev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_dts_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_dts_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_dts_pm_ops,
			 mt6360_pmu_dts_suspend, mt6360_pmu_dts_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_dts_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_dts", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_dts_of_id);

static const struct platform_device_id mt6360_pmu_dts_id[] = {
	{ "mt6360_pmu_dts", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_chg_id);

static struct platform_driver mt6360_pmu_dts_driver = {
	.driver = {
		.name = "mt6360_pmu_dts",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_dts_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_dts_of_id),
	},
	.probe = mt6360_pmu_dts_probe,
	.remove = mt6360_pmu_dts_remove,
	.id_table = mt6360_pmu_dts_id,
};
module_platform_driver(mt6360_pmu_dts_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU CHG Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MT6360_PMU_CHG_DRV_VERSION);

/*
 * Version Note
 * 1.0.7_MTK
 * (1) Fix Unbalanced enable for MIVR IRQ
 * (2) Sleep 200ms before do another iteration in mt6360_chg_mivr_task_threadfn
 *
 * 1.0.6_MTK
 * (1) Fix the usages of charger power supply
 *
 * 1.0.5_MTK
 * (1) Prevent charger type infromed repeatedly
 *
 * 1.0.4_MTK
 * (1) Mask mivr irq until mivr task has run an iteration
 *
 * 1.0.3_MTK
 * (1) fix zcv adc from 5mV to 1.25mV per step
 * (2) add BC12 initial setting dcd timeout disable when unuse dcd
 *
 * 1.0.2_MTK
 * (1) remove eoc, rechg, te irq for evb with phone load
 * (2) report power supply online with chg type detect done
 * (3) remove unused irq event and status
 * (4) add chg termination irq notifier when safety timer timeout
 *
 * 1.0.1_MTK
 * (1) fix dtsi parse attribute about en_te, en_wdt, aicc_once
 * (2) add charger class get vbus adc interface
 * (3) add initial setting about disable en_sdi, and check batsysuv.
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */

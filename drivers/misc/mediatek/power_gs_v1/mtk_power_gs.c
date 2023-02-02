// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */


#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/proc_fs.h>


#include <aee.h>
//#include <mt-plat/upmu_common.h>

#include "mtk_power_gs.h"

/* #define POWER_GS_DEBUG */
#undef mt_power_gs_dump_suspend
#undef mt_power_gs_dump_dpidle
#undef mt_power_gs_dump_sodi3
#undef mt_power_gs_t_dump_suspend
#undef mt_power_gs_t_dump_dpidle
#undef mt_power_gs_t_dump_sodi3


struct golden _g;

struct regmap *gs_regmap;

bool is_already_snap_shot;

bool slp_chk_golden_suspend = true;
bool slp_chk_golden_dpidle = true;
bool slp_chk_golden_sodi3 = true;
bool slp_chk_golden_diff_mode = true;

/* deprecated, temp used for api argument transfer */
void mt_power_gs_f_dump_suspend(unsigned int dump_flag)
{
	if (slp_chk_golden_suspend)
		mt_power_gs_suspend_compare(dump_flag);
}
void mt_power_gs_t_dump_suspend(int count, ...)
{
	unsigned int p1 = GS_ALL;
	va_list v;

	va_start(v, count);

	if (count)
		p1 = va_arg(v, unsigned int);

	/* if the argument is void, va_arg will get -1 */
	if (p1 > GS_ALL)
		p1 = GS_ALL;

	mt_power_gs_f_dump_suspend(p1);
	va_end(v);
}
EXPORT_SYMBOL(mt_power_gs_t_dump_suspend);
void mt_power_gs_f_dump_dpidle(unsigned int dump_flag)
{
	if (slp_chk_golden_dpidle)
		mt_power_gs_dpidle_compare(dump_flag);
}
void mt_power_gs_t_dump_dpidle(int count, ...)
{
	unsigned int p1 = GS_ALL;
	va_list v;

	va_start(v, count);

	if (count)
		p1 = va_arg(v, unsigned int);

	/* if the argument is void, va_arg will get -1 */
	if (p1 > GS_ALL)
		p1 = GS_ALL;

	mt_power_gs_f_dump_dpidle(p1);
	va_end(v);
}
EXPORT_SYMBOL(mt_power_gs_t_dump_dpidle);
void mt_power_gs_f_dump_sodi3(unsigned int dump_flag)
{
	if (slp_chk_golden_sodi3)
		mt_power_gs_sodi_compare(dump_flag);
}
void mt_power_gs_t_dump_sodi3(int count, ...)
{
	unsigned int p1 = GS_ALL;
	va_list v;

	va_start(v, count);

	if (count)
		p1 = va_arg(v, unsigned int);

	/* if the argument is void, va_arg will get -1 */
	if (p1 > GS_ALL)
		p1 = GS_ALL;

	mt_power_gs_f_dump_sodi3(p1);
	va_end(v);
}
EXPORT_SYMBOL(mt_power_gs_t_dump_sodi3);

void mt_power_gs_dump_suspend(void)
{
	mt_power_gs_f_dump_suspend(GS_ALL);
}
EXPORT_SYMBOL(mt_power_gs_dump_suspend);
void mt_power_gs_dump_dpidle(void)
{
	mt_power_gs_f_dump_dpidle(GS_ALL);
}
EXPORT_SYMBOL(mt_power_gs_dump_dpidle);
void mt_power_gs_dump_sodi3(void)
{
	mt_power_gs_f_dump_sodi3(GS_ALL);
}
EXPORT_SYMBOL(mt_power_gs_dump_sodi3);

int snapshot_golden_setting(const char *func, const unsigned int line)
{
	if (!is_already_snap_shot)
		return _snapshot_golden_setting(&_g, func, line);

	return 0;
}
EXPORT_SYMBOL(snapshot_golden_setting);

static const struct of_device_id power_gs_of_ids[] = {
	{.compatible = "mediatek,power_gs",},
	{}
};

static int __init mt_power_gs_pdrv_probe(struct platform_device *pdev)
{
	struct platform_device *pmic_pdev = NULL;
	struct device_node *pmic_node = NULL;
	struct mt6397_chip *chip = NULL;
	struct device_node *node = NULL;

	pr_info("%s start\n", __func__);
	node = of_find_matching_node(NULL, power_gs_of_ids);
	if (!node) {
		dev_notice(&pdev->dev, "fail to find POWERGS node\n");
		return -1;
	}
	/* get PMIC regmap */
	pmic_node = of_parse_phandle(node, "pmic", 0);
	if (!pmic_node) {
		dev_notice(&pdev->dev, "fail to find pmic node\n");
		return -1;
	}


	pmic_pdev = of_find_device_by_node(pmic_node);
	if (!pmic_pdev) {
		dev_notice(&pdev->dev, "fail to find pmic device or some project no pmic config\n");
		return -1;
	}

	chip = dev_get_drvdata(&(pmic_pdev->dev));
	if (!chip) {
		dev_notice(&pdev->dev, "fail to find pmic drv data\n");
		return -1;
	}

	gs_regmap =  chip->regmap;
	if (IS_ERR_VALUE(gs_regmap)) {
		gs_regmap = NULL;
		dev_notice(&pdev->dev, "get pmic regmap fail\n");
		return -1;
	}
	pr_info("%s success\n", __func__);
	return 0;
}

static int mt_power_gs_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_device mt_power_gs_pdev = {
	.name = "mt-powergs",
	.id = -1,
};

static struct platform_driver mt_power_gs_pdrv __refdata = {
	.probe = mt_power_gs_pdrv_probe,
	.remove = mt_power_gs_pdrv_remove,
	.driver = {
		.name = "powergs",
		.owner = THIS_MODULE,
		.of_match_table = power_gs_of_ids,
	},
};

static void __exit mt_power_gs_exit(void)
{
}

static int __init mt_power_gs_init(void)
{
	int ret = 0;

	pr_info("%s start\n", __func__);
	/* register platform device/driver */
	ret = platform_device_register(&mt_power_gs_pdev);
	if (ret) {
		pr_info("fail to register power gs device @ %s()\n", __func__);
		return -1;
	}

	ret = platform_driver_register(&mt_power_gs_pdrv);
	if (ret) {
		pr_info("fail to register power gs driver @ %s()\n", __func__);
		platform_device_unregister(&mt_power_gs_pdev);
		return -1;
	}
	mt_golden_setting_init();
	pr_info("%s success\n", __func__);
	return 0;
}

module_init(mt_power_gs_init);
module_exit(mt_power_gs_exit);

MODULE_DESCRIPTION("MT Low Power Golden Setting");
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/uaccess.h>

#include "apu_top.h"
#include "apu_top_entry.h"

struct platform_device *this_pdev;
static const struct apupwr_plat_data *pwr_data;
static int aputop_func_sel;

void __register_aputop_post_power_off_cb(
		void (*post_power_off_cb)(void))
{
}
EXPORT_SYMBOL(__register_aputop_post_power_off_cb);

void __register_aputop_post_power_off_sync_cb(
		void (*post_power_off_sync_cb)(void))
{
}
EXPORT_SYMBOL(__register_aputop_post_power_off_sync_cb);

static int device_linker(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *sup_np = NULL;
	struct device_node *con_np = NULL;
	struct device *sup_dev = NULL;
	struct device *con_dev = NULL;
	struct platform_device *sup_pdev = NULL;
	struct platform_device *con_pdev = NULL;
	int con_size = 0, idx = 0;

	con_size = of_count_phandle_with_args(dev->of_node, "consumer", NULL);

	if (con_size > 0) {

		sup_np = dev->of_node;

		for (idx = 0; idx < con_size; idx++) {
			con_np = of_parse_phandle(sup_np, "consumer", idx);

			con_pdev = of_find_device_by_node(con_np);
			con_dev = &con_pdev->dev;
			sup_pdev = of_find_device_by_node(sup_np);
			sup_dev = &sup_pdev->dev;

			if (!(sup_dev &&
				of_node_check_flag(sup_np, OF_POPULATED)
				&& con_dev &&
				of_node_check_flag(con_np, OF_POPULATED))) {

				return -EPROBE_DEFER;
			}

			get_device(sup_dev);
			get_device(con_dev);

			if (!device_link_add(con_dev, sup_dev,
				DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME)) {
				dev_info(dev, "Not linking %pOFP - %pOFP\n",
						con_np, sup_np);
				put_device(sup_dev);
				put_device(con_dev);

				return -EINVAL;
			}

			put_device(sup_dev);
			put_device(con_dev);

		}
	}

	return 0;
}

static int check_pwr_data(void)
{
	if (!pwr_data) {
		pr_info("%s error : has no platform data\n", __func__);
		return -ENODEV;
	}

	return 0;
}

static int aputop_pwr_on_rpm_cb(struct device *dev)
{
	if (check_pwr_data())
		return -ENODEV;

	if (pwr_data->bypass_pwr_on == 1) {
		dev_info(dev, "%s bypass\n", __func__);
		return 0;
	}

	dev_info(dev, "%s %s\n", __func__, pwr_data->plat_name);
	return pwr_data->plat_apu_top_on(dev);
}

static int aputop_pwr_off_rpm_cb(struct device *dev)
{
	if (check_pwr_data())
		return -ENODEV;

	if (pwr_data->bypass_pwr_off == 1) {
		dev_info(dev, "%s bypass\n", __func__);
		return 0;
	}

	dev_info(dev, "%s %s\n", __func__, pwr_data->plat_name);
	return pwr_data->plat_apu_top_off(dev);
}

static int apu_top_probe(struct platform_device *pdev)
{
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	pwr_data = of_device_get_match_data(&pdev->dev);

	if (check_pwr_data())
		return -ENODEV;

	dev_info(&pdev->dev, "%s %s\n", __func__, pwr_data->plat_name);

	this_pdev = pdev;
	g_apupw_drv_ver = 3;

	pm_runtime_enable(&pdev->dev);

	ret = device_linker(pdev);
	if (ret)
		return ret;

	return pwr_data->plat_apu_top_pb(pdev);
}

static int apu_top_remove(struct platform_device *pdev)
{
	if (check_pwr_data())
		return -ENODEV;

	dev_info(&pdev->dev, "%s %s\n", __func__, pwr_data->plat_name);
	pwr_data->plat_apu_top_rm(pdev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id of_match_apu_top[] = {
	{ .compatible = "mt6893,apu_top_3", .data = &mt6893_plat_data},
	{ /* end of list */},
};

static const struct dev_pm_ops mtk_aputop_pm_ops = {
	SET_RUNTIME_PM_OPS(aputop_pwr_off_rpm_cb, aputop_pwr_on_rpm_cb, NULL)
};

static struct platform_driver apu_top_drv = {
	.probe = apu_top_probe,
	.remove = apu_top_remove,
	.driver = {
		.name = "apu_top_3",
		.pm = &mtk_aputop_pm_ops,
		.of_match_table = of_match_apu_top,
	},
};

static int set_aputop_func_param(const char *buf,
		const struct kernel_param *kp)
{
	struct aputop_func_param aputop;
	int result = 0;

	if (check_pwr_data())
		return -ENODEV;

	memset(&aputop, 0, sizeof(struct aputop_func_param));

	result = sscanf(buf, "%d %d %d %d %d",
				&aputop.func_id,
				&aputop.param1, &aputop.param2,
				&aputop.param3, &aputop.param4);

	if (result > 5) {
		pr_notice("%s invalid input: %s, result(%d)\n",
				__func__, buf, result);
		return -EINVAL;
	}

	pr_info(
		"%s (func_id, param1, param2, param3, param4): (%d,%d,%d,%d,%d)\n",
		__func__, aputop.func_id,
		aputop.param1, aputop.param2,
		aputop.param3, aputop.param4);

	return pwr_data->plat_apu_top_func(this_pdev, aputop.func_id, &aputop);
}

static int get_aputop_func_param(char *buf, const struct kernel_param *kp)
{
	if (check_pwr_data())
		return -ENODEV;

	return snprintf(buf, 64, "aputop_func_sel:%d\n", aputop_func_sel);
}

static struct kernel_param_ops aputop_func_ops = {
	.set = set_aputop_func_param,
	.get = get_aputop_func_param,
};

__MODULE_PARM_TYPE(aputop_func_sel, "int");
module_param_cb(aputop_func_sel, &aputop_func_ops, &aputop_func_sel, 0644);
MODULE_PARM_DESC(aputop_func_sel, "trigger apu top func by parameter");

int apu_top_3_init(void)
{
	return platform_driver_register(&apu_top_drv);
}

void apu_top_3_exit(void)
{
	platform_driver_unregister(&apu_top_drv);
}

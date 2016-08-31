/*
 * Clock driver for  Palmas device.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/mfd/palmas.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct palmas_clk32k_info {
	const char *clk_name;
	unsigned int control_reg;
	unsigned int enable_mask;
	unsigned int sleep_mask;
	unsigned int sleep_reqstr_id;
};

struct palmas_clk {
	struct device *dev;
	struct palmas *palmas;
	struct palmas_clk32k_info *clk_info;
};

static struct palmas_clk32k_info palmas_clk32k_info[] = {
	{
		.clk_name = "clk32k_kg",
		.control_reg = PALMAS_CLK32KG_CTRL,
		.enable_mask = PALMAS_CLK32KG_CTRL_MODE_ACTIVE,
		.sleep_mask = PALMAS_CLK32KG_CTRL_MODE_SLEEP,
		.sleep_reqstr_id = PALMAS_EXTERNAL_REQSTR_ID_CLK32KG,
	}, {
		.clk_name = "clk32k_kg_audio",
		.control_reg = PALMAS_CLK32KGAUDIO_CTRL,
		.enable_mask = PALMAS_CLK32KG_CTRL_MODE_ACTIVE,
		.sleep_mask = PALMAS_CLK32KG_CTRL_MODE_SLEEP,
		.sleep_reqstr_id = PALMAS_EXTERNAL_REQSTR_ID_CLK32KGAUDIO,
	},
};

static int palmas_clk_initialise(struct palmas_clk *palmas_clks,
		struct palmas_clk32k_init_data *clk32_idata)
{
	unsigned int control_reg = palmas_clks->clk_info->control_reg;
	int id = palmas_clks->clk_info->sleep_reqstr_id;
	int val = 0;
	int ret;

	if (clk32_idata->enable)
		val = palmas_clks->clk_info->enable_mask;

	ret = palmas_update_bits(palmas_clks->palmas, PALMAS_RESOURCE_BASE,
			control_reg, palmas_clks->clk_info->enable_mask, val);
	if (ret < 0) {
		dev_err(palmas_clks->dev, "Reg 0x%02x update failed, %d\n",
			control_reg, ret);
		return ret;
	}

	if (!clk32_idata->sleep_control) {
		ret = palmas_update_bits(palmas_clks->palmas,
				PALMAS_RESOURCE_BASE, control_reg,
				palmas_clks->clk_info->sleep_mask, 0);
		if (ret < 0)
			dev_err(palmas_clks->dev,
				"Reg 0x%02x update failed, %d\n",
				control_reg, ret);
		return ret;
	}

	id = palmas_clks->clk_info->sleep_reqstr_id;
	ret = palmas_ext_power_req_config(palmas_clks->palmas, id,
				clk32_idata->sleep_control, true);
	if (ret < 0) {
		dev_err(palmas_clks->dev, "Ext control config failed, %d\n",
			ret);
		return ret;
	}

	ret = palmas_update_bits(palmas_clks->palmas, PALMAS_RESOURCE_BASE,
			control_reg, palmas_clks->clk_info->sleep_mask,
			palmas_clks->clk_info->sleep_mask);
	if (ret < 0)
		dev_err(palmas_clks->dev, "Reg 0x%02x update failed, %d\n",
			control_reg, ret);

	return ret;
}

static int palmas_clk_get_clk_data(struct platform_device *pdev,
	struct palmas_clk32k_init_data **clk32_idata, int *num_idata)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *child;
	struct palmas_clk32k_init_data *idata;
	int num_child;
	int num_valid_child;
	int i;
	int ret;
	unsigned int prop;

	*num_idata = 0;
	*clk32_idata = NULL;

	num_child = of_get_child_count(node);
	if (!num_child) {
		dev_warn(&pdev->dev, "No clock child\n");
		return 0;
	}

	idata = devm_kzalloc(&pdev->dev, sizeof(*idata) * num_child,
				GFP_KERNEL);
	if (!idata)
		return -ENOMEM;

	num_valid_child = 0;
	for (i = 0; i < PALMAS_CLOCK32K_NR; ++i) {
		child = of_get_child_by_name(node,
				palmas_clk32k_info[i].clk_name);
		if (!child)
			continue;

		idata[num_valid_child].clk32k_id = i;
		idata[num_valid_child].enable = of_property_read_bool(child,
						"ti,clock-boot-enable");
		ret = of_property_read_u32(child, "ti,external-sleep-control",
						&prop);
		if (!ret && prop < 3)
			idata[num_valid_child].sleep_control = prop;
		num_valid_child++;
	}

	*num_idata = num_valid_child;
	*clk32_idata =  idata;
	return 0;
}

static int palmas_clk_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_platform_data *palmas_pdata;
	struct palmas_clk32k_init_data *clk32_idata = NULL;
	int clk_idata_size = 0;
	struct palmas_clk *palmas_clks;
	int i, ret;

	palmas_pdata = dev_get_platdata(pdev->dev.parent);
	if (palmas_pdata && palmas_pdata->clk32k_init_data) {
		clk_idata_size = palmas_pdata->clk32k_init_data_size;
		clk32_idata = palmas_pdata->clk32k_init_data;
	}

	if (!clk32_idata && pdev->dev.of_node) {
		ret = palmas_clk_get_clk_data(pdev, &clk32_idata,
				&clk_idata_size);
		if (ret < 0)
			return ret;
	}

	if (!clk32_idata)
		dev_info(&pdev->dev, "No clock platform data\n");

	palmas_clks = devm_kzalloc(&pdev->dev, sizeof(*palmas_clks) *
				PALMAS_CLOCK32K_NR, GFP_KERNEL);
	if (!palmas_clks)
		return -ENOMEM;

	platform_set_drvdata(pdev, palmas_clks);

	for (i = 0; i < PALMAS_CLOCK32K_NR; i++) {
		palmas_clks[i].dev = &pdev->dev;
		palmas_clks[i].palmas = palmas;
		palmas_clks[i].clk_info = &palmas_clk32k_info[i];
	}

	for (i = 0; i < clk_idata_size; ++i) {
		ret = palmas_clk_initialise(
				&palmas_clks[clk32_idata[i].clk32k_id],
				&clk32_idata[i]);
		if (ret < 0)
			dev_warn(&pdev->dev, "CLK %d init failed, %d\n",
				clk32_idata[i].clk32k_id, ret);
	}
	return 0;
}

static struct of_device_id of_palmas_clk_match_tbl[] = {
	{ .compatible = "ti,palmas-clk", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_palmas_clk_match_tbl);

static struct platform_driver palmas_clk_driver = {
	.driver = {
		.name = "palmas-clk",
		.owner = THIS_MODULE,
		.of_match_table = of_palmas_clk_match_tbl,
	},
	.probe = palmas_clk_probe,
};

static int __init palmas_clk_init(void)
{
	return platform_driver_register(&palmas_clk_driver);
}
subsys_initcall(palmas_clk_init);

static void __exit palmas_clk_exit(void)
{
	platform_driver_unregister(&palmas_clk_driver);
}
module_exit(palmas_clk_exit);

MODULE_DESCRIPTION("Clock driver for Palmas Series Devices");
MODULE_ALIAS("platform:palmas-clk");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");

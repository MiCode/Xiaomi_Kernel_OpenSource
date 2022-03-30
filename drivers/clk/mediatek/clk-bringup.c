// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>

static int __bring_up_enable(struct platform_device *pdev)
{
	struct clk *clk;
	int clk_con, i;

	clk_con = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	for (i = 0; i < clk_con; i++) {
		clk = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(clk)) {
			long ret = PTR_ERR(clk);

			if (ret == -EPROBE_DEFER)
				pr_notice("clk %d is not ready\n", i);
			else
				pr_notice("get clk %d fail, ret=%d, clk_con=%d\n",
				       i, (int)ret, clk_con);
		} else {
			pr_notice("get clk [%d]: %s ok\n", i,
					__clk_get_name(clk));
			clk_prepare_enable(clk);
		}
	}

	return 0;
}

static int clk_bring_up_probe(struct platform_device *pdev)
{
	return __bring_up_enable(pdev);
}

static int clk_post_ao_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	u32 enabled;

	of_property_read_u32(node, "mediatek,post_ao", &enabled);

	if (enabled != 1) {
		pr_notice("btypass_clk_post_ao\n");
		return 0;
	}

	return __bring_up_enable(pdev);
}

static const struct of_device_id bring_up_id_table[] = {
	{
		.compatible = "mediatek,clk-bring-up",
		.data = clk_bring_up_probe,
	}, {
		.compatible = "mediatek,clk-post-ao",
		.data = clk_post_ao_probe,
	}, {
		/* sentinel */
	}
};

static int bring_up_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static int bring_up_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver bring_up = {
	.probe		= bring_up_probe,
	.remove		= bring_up_remove,
	.driver		= {
		.name	= "bring_up",
		.owner	= THIS_MODULE,
		.of_match_table = bring_up_id_table,
	},
};

module_platform_driver(bring_up);
MODULE_LICENSE("GPL");

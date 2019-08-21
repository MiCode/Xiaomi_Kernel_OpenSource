/* Copyright (c) 2011, 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/clk/msm-clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <soc/qcom/msm-clock-controller.h>

static int dummy_clk_reset(struct clk *clk, enum clk_reset_action action)
{
	return 0;
}

static int dummy_clk_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;
	return 0;
}

static int dummy_clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

static int dummy_clk_set_flags(struct clk *clk, unsigned long flags)
{
	return 0;
}

static unsigned long dummy_clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

static long dummy_clk_round_rate(struct clk *clk, unsigned long rate)
{
	return rate;
}

const struct clk_ops clk_ops_dummy = {
	.reset = dummy_clk_reset,
	.set_rate = dummy_clk_set_rate,
	.set_max_rate = dummy_clk_set_max_rate,
	.set_flags = dummy_clk_set_flags,
	.get_rate = dummy_clk_get_rate,
	.round_rate = dummy_clk_round_rate,
};

struct clk dummy_clk = {
	.dbg_name = "dummy_clk",
	.ops = &clk_ops_dummy,
	CLK_INIT(dummy_clk),
};

static void *dummy_clk_dt_parser(struct device *dev, struct device_node *np)
{
	struct clk *c;

	c = devm_kzalloc(dev, sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	c->ops = &clk_ops_dummy;
	return msmclk_generic_clk_init(dev, np, c);
}
MSMCLK_PARSER(dummy_clk_dt_parser, "qcom,dummy-clk", 0);

static struct clk *of_dummy_get(struct of_phandle_args *clkspec,
				  void *data)
{
	return &dummy_clk;
}

static const struct of_device_id msm_clock_dummy_match_table[] = {
	{ .compatible = "qcom,dummycc" },
	{}
};

static int msm_clock_dummy_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_clk_add_provider(pdev->dev.of_node, of_dummy_get, NULL);
	if (ret)
		return -ENOMEM;

	dev_info(&pdev->dev, "Registered DUMMY provider.\n");
	return ret;
}

static struct platform_driver msm_clock_dummy_driver = {
	.probe = msm_clock_dummy_probe,
	.driver = {
		.name = "clock-dummy",
		.of_match_table = msm_clock_dummy_match_table,
		.owner = THIS_MODULE,
	},
};

int __init msm_dummy_clk_init(void)
{
	return platform_driver_register(&msm_clock_dummy_driver);
}
arch_initcall(msm_dummy_clk_init);

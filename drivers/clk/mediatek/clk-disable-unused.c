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
#include <linux/pm_runtime.h>

#define DUMP_UNUSED_CLKS		1

static const struct of_device_id disable_unused_id_table[] = {
	{ .compatible = "mediatek,clk-disable-unused",},
	{ },
};
MODULE_DEVICE_TABLE(of, disable_unused_id_table);

static int disable_unused_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int clk_con, i = 0;
	int pp1, en1, pp2, en2;
	int retval = 0;

	clk_con = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	if (!clk_con)
		return 0;

	pr_notice("disable_unused dev: %s ongo\n",
					pdev->name);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
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
			pr_notice("disable_unused clk [%d]: %s ongo\n",
					i, __clk_get_name(clk));
#if DUMP_UNUSED_CLKS
			/* enable parent clk first because of clk dependency */
			retval = clk_prepare_enable(clk_get_parent(clk));
			if (retval) {
				pr_info("parent clk of clk %d clk_prepare_enable failed\n", i);
				pr_info("return err code=%d\n", retval);
				return 0;
			}
			/* record clk initial on/off state */
			en1 = __clk_is_enabled(clk);
			pp1 = clk_hw_is_prepared(__clk_get_hw(clk));
#endif

			/* enable then disable clk to turn off unused clks */
			retval = clk_prepare_enable(clk);
			if (retval) {
				pr_info("clk %d clk_prepare_enable failed\n", i);
				pr_info("return err code=%d\n", retval);
				return 0;
			}

			clk_disable_unprepare(clk);

#if DUMP_UNUSED_CLKS
			/* record clk on/off state after disable unused clock  */
			en2 = __clk_is_enabled(clk);
			pp2 = clk_hw_is_prepared(__clk_get_hw(clk));

			/* disable parent clk to resume clk parent previous state */
			clk_disable_unprepare(clk_get_parent(clk));

			/* if clk before/after state is different, then it's been disabled */
			if (pp1 != pp2 || en1 != en2)
				pr_notice("disable_unused clk [%d]: %s done\n",
					i, __clk_get_name(clk));
#endif
		}
	}

	pm_runtime_put_sync(&pdev->dev);

	return 0;
}

static struct platform_driver disable_unused = {
	.probe		= disable_unused_probe,
	.driver		= {
		.name	= "disable_unused",
		.owner	= THIS_MODULE,
		.of_match_table = disable_unused_id_table,
	},
};

static int __init disable_unused_init(void)
{
	return platform_driver_register(&disable_unused);
}

static void __exit disable_unused_exit(void)
{
	platform_driver_unregister(&disable_unused);
}

late_initcall_sync(disable_unused_init);
module_exit(disable_unused_exit);
MODULE_LICENSE("GPL");
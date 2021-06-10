// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/clk.h>
#include <linux/of_platform.h>

#include "sec_clk.h"

#define MOD                         "MASP"

int sec_clk_enable(struct platform_device *dev)
{
#if 0 // mt8167 case
	struct clk *sec_clk;
	struct clk *sec_clk_13m;

	sec_clk = devm_clk_get(&dev->dev, "main");
	sec_clk_13m = devm_clk_get(&dev->dev, "second");

	if (IS_ERR(sec_clk))
		return PTR_ERR(sec_clk);

	if (IS_ERR(sec_clk_13m))
		return PTR_ERR(sec_clk_13m);

	clk_prepare_enable(sec_clk);
	clk_prepare_enable(sec_clk_13m);
	pr_debug("[%s] get hacc clock\n", MOD);
#else
	pr_debug("[%s] Need not to get hacc clock\n", MOD);
#endif

	return 0;
}


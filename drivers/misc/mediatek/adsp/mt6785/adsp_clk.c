/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/clk.h>

#include "adsp_clk.h"
#include "adsp_platform.h"

static uint32_t adsp_clock_count;
DEFINE_SPINLOCK(adsp_clock_spinlock);

struct adsp_clock_attr {
	const char *name;
	struct clk *clock;
};

static struct adsp_clock_attr adsp_clks[ADSP_CLK_NUM] = {
	[CLK_ADSP_INFRA] = {"clk_adsp_infra", NULL},
	[CLK_TOP_ADSP_SEL] = {"clk_top_adsp_sel", NULL},
	[CLK_TOP_CLK26M] = {"clk_adsp_clk26m", NULL},
	[CLK_TOP_MMPLL_D4] = {"clk_top_mmpll_d4", NULL},
	[CLK_TOP_ADSPPLL_D4] = {"clk_top_adsppll_d4", NULL},
	[CLK_TOP_ADSPPLL_D6] = {"clk_top_adsppll_d6", NULL},
};

int adsp_set_top_mux(enum adsp_clk clk)
{
	int ret = 0;

	pr_debug("%s(%x)\n", __func__, clk);

	if (clk >= ADSP_CLK_NUM  || clk < 0)
		return -EINVAL;

	ret = clk_set_parent(adsp_clks[CLK_TOP_ADSP_SEL].clock,
		      adsp_clks[clk].clock);
	if (IS_ERR(&ret))
		pr_err("%s() clk_set_parent(clk_top_adsp_sel-%x) fail %d\n",
		      __func__, clk, ret);
	return ret;
}

void adsp_set_clock_freq(enum adsp_clk clk)
{
	switch (clk) {
	case CLK_TOP_CLK26M:
	case CLK_TOP_MMPLL_D4:
	case CLK_TOP_ADSPPLL_D4:
	case CLK_TOP_ADSPPLL_D6:
		adsp_set_top_mux(clk);
		break;
	default:
		break;
	}
}

/* clock init */
int adsp_clk_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	size_t i;
	struct device *dev = &pdev->dev;

	for (i = 0; i < ARRAY_SIZE(adsp_clks); i++) {
		adsp_clks[i].clock = devm_clk_get(dev, adsp_clks[i].name);
		if (IS_ERR(adsp_clks[i].clock)) {
			ret = PTR_ERR(adsp_clks[i].clock);
			pr_err("%s devm_clk_get %s fail %d\n", __func__,
				   adsp_clks[i].name, ret);
		}
	}

	return ret;
}

/* clock deinit */
void adsp_clk_device_remove(void *dev)
{
	pr_info("%s\n", __func__);
}

int adsp_enable_clock(void)
{
	int ret = 0;
	unsigned long spin_flags;

	pr_info("%s()\n", __func__);
	spin_lock_irqsave(&adsp_clock_spinlock, spin_flags);
	if (++adsp_clock_count == 1)
		/* unable to access adsp sram before set way_en to 1 */
		adsp_way_en_ctrl(1);
	spin_unlock_irqrestore(&adsp_clock_spinlock, spin_flags);

	ret = clk_prepare_enable(adsp_clks[CLK_ADSP_INFRA].clock);
	if (IS_ERR(&ret)) {
		pr_err("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_ADSP_INFRA].name, ret);
		adsp_clock_count--;
		return -EINVAL;
	}

	return 0;
}

void adsp_disable_clock(void)
{
	unsigned long spin_flags;

	pr_info("%s()\n", __func__);
	clk_disable_unprepare(adsp_clks[CLK_ADSP_INFRA].clock);

	spin_lock_irqsave(&adsp_clock_spinlock, spin_flags);
	if (--adsp_clock_count == 0)
		/* unable to access adsp sram before set way_en to 1 */
		adsp_way_en_ctrl(0);
	spin_unlock_irqrestore(&adsp_clock_spinlock, spin_flags);
}

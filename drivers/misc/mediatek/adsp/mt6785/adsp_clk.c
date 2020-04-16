/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/clk.h>
#include "adsp_clk.h"
#include "adsp_helper.h"

struct adsp_clock_attr {
	const char *name;
	bool clk_prepare;
	bool clk_status;
	struct clk *clock;
};

static struct adsp_clock_attr adsp_clks[ADSP_CLK_NUM] = {
	[CLK_ADSP_INFRA] = {"clk_adsp_infra", false, false, NULL},
	[CLK_TOP_ADSP_SEL] = {"clk_top_adsp_sel", false, false, NULL},
	[CLK_ADSP_CLK26M] = {"clk_adsp_clk26m", false, false, NULL},
	[CLK_TOP_MMPLL_D4] = {"clk_top_mmpll_d4", false, false, NULL},
	[CLK_TOP_ADSPPLL_D4] = {"clk_top_adsppll_d4", false, false, NULL},
	[CLK_TOP_ADSPPLL_D6] = {"clk_top_adsppll_d6", false, false, NULL},
};
static uint32_t adsp_clock_count;
DEFINE_SPINLOCK(adsp_clock_spinlock);

int adsp_set_top_mux(enum adsp_clk clk)
{
	int ret = 0;
	struct clk *parent;

	pr_debug("%s(%x)\n", __func__, clk);

	switch (clk) {
	case CLK_ADSP_CLK26M:
		parent = adsp_clks[CLK_ADSP_CLK26M].clock;
		break;
	case CLK_TOP_MMPLL_D4:
		parent = adsp_clks[CLK_TOP_MMPLL_D4].clock;
		break;
	case CLK_TOP_ADSPPLL_D4:
		parent = adsp_clks[CLK_TOP_ADSPPLL_D4].clock;
		break;
	case CLK_TOP_ADSPPLL_D6:
		parent = adsp_clks[CLK_TOP_ADSPPLL_D6].clock;
		break;
	default:
		parent = adsp_clks[CLK_ADSP_CLK26M].clock;
		break;
	}
	ret = clk_set_parent(adsp_clks[CLK_TOP_ADSP_SEL].clock, parent);
	if (IS_ERR(&ret))
		pr_err("[ADSP] %s clk_set_parent(clk_top_adsp_sel-%x) fail %d\n",
		      __func__, clk, ret);

	return ret;
}

/* clock init */
int adsp_clk_device_probe(void *dev)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(adsp_clks); i++) {
		adsp_clks[i].clock = devm_clk_get(dev, adsp_clks[i].name);
		if (IS_ERR(adsp_clks[i].clock)) {
			ret = PTR_ERR(adsp_clks[i].clock);
			pr_err("%s devm_clk_get %s fail %d\n", __func__,
				   adsp_clks[i].name, ret);
		} else {
			adsp_clks[i].clk_status = true;
		}
	}

	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(adsp_clks); i++) {
		if (adsp_clks[i].clk_status) {
			ret = clk_prepare(adsp_clks[i].clock);
			if (ret) {
				pr_err("%s clk_prepare %s fail %d\n", __func__,
					   adsp_clks[i].name, ret);
			} else {
				adsp_clks[i].clk_prepare = true;
			}
		}
	}

	return ret;
}

/* clock deinit */
void adsp_clk_device_remove(void *dev)
{
	size_t i;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(adsp_clks); i++) {
		if (adsp_clks[i].clock && !IS_ERR(adsp_clks[i].clock) &&
			adsp_clks[i].clk_prepare) {
			clk_unprepare(adsp_clks[i].clock);
			adsp_clks[i].clk_prepare = false;
		}
	}
}

int adsp_enable_clock(void)
{
	int ret = 0;
	unsigned long spin_flags;

	if (adsp_clks[CLK_ADSP_INFRA].clk_prepare) {
		spin_lock_irqsave(&adsp_clock_spinlock, spin_flags);
		if (++adsp_clock_count == 1)
			/* unable to access adsp sram before set way_en to 1 */
			adsp_way_en_ctrl(1);
		ret = clk_enable(adsp_clks[CLK_ADSP_INFRA].clock);
		if (IS_ERR(&ret))
			pr_err("%s(), clk_enable %s fail, ret %d\n", __func__,
				adsp_clks[CLK_ADSP_INFRA].name, ret);
		spin_unlock_irqrestore(&adsp_clock_spinlock, spin_flags);
	} else {
		pr_err("%s(), clk %s, not prepared\n",
			__func__, adsp_clks[CLK_ADSP_INFRA].name);
		ret = -EINVAL;
	}

	return ret;
}

void adsp_disable_clock(void)
{
	unsigned long spin_flags;

	if (adsp_clks[CLK_ADSP_INFRA].clk_prepare) {
		spin_lock_irqsave(&adsp_clock_spinlock, spin_flags);
		if (--adsp_clock_count == 0)
			/* unable to access adsp sram before set way_en to 1 */
			adsp_way_en_ctrl(0);
		clk_disable(adsp_clks[CLK_ADSP_INFRA].clock);
		spin_unlock_irqrestore(&adsp_clock_spinlock, spin_flags);
	}
}

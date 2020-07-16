/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "adsp_clk.h"
#include <linux/clk.h>


static uint32_t adsp_clock_count;

struct adsp_clock_attr {
	const char *name;
	struct clk *clock;
};

static struct adsp_clock_attr adsp_clks[ADSP_CLK_NUM] = {
	[CLK_SCP_SYS_ADSP] = {"scp_sys_adsp", NULL},
	[CLK_ADSP_CK_CG] = {"clk_adsp_ck_cg", NULL},
	[CLK_TOP_ADSP_SEL] = {"clk_top_adsp_sel", NULL},
	[CLK_TOP_CLK26M] = {"clk_top_clk26m", NULL},
	[CLK_TOP_ADSPPLL] = {"clk_top_adsppll", NULL},
};

static struct adsp_clock_attr scp_clks[SCP_CLK_NUM] = {
	[CLK_TOP_SCP_SEL] = {"clk_top_scp_sel", NULL},
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
		pr_err("[ADSP] %s clk_set_parent(clk_top_adsp_sel-%x) fail %d\n",
		      __func__, clk, ret);
	return ret;
}

void adsp_set_clock_freq(enum adsp_clk clk)
{
	switch (clk) {
	case CLK_TOP_CLK26M:
	case CLK_TOP_ADSPPLL:
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

	for (i = 0; i < ARRAY_SIZE(scp_clks); i++) {
		scp_clks[i].clock = devm_clk_get(dev, scp_clks[i].name);
		if (IS_ERR(scp_clks[i].clock)) {
			ret = PTR_ERR(scp_clks[i].clock);
			pr_info("%s devm_clk_get %s fail %d\n", __func__,
				   scp_clks[i].name, ret);
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

	pr_debug("%s()\n", __func__);

	/* enable scp clock before access adsp clock cg */
	ret = clk_prepare_enable(scp_clks[CLK_TOP_SCP_SEL].clock);
	if (IS_ERR(&ret)) {
		pr_info("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, scp_clks[CLK_TOP_SCP_SEL].name, ret);
		return -EINVAL;
	}

	ret = clk_prepare_enable(adsp_clks[CLK_SCP_SYS_ADSP].clock);
	if (IS_ERR(&ret)) {
		pr_info("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_SCP_SYS_ADSP].name, ret);
		return -EINVAL;
	}

	ret = clk_prepare_enable(adsp_clks[CLK_ADSP_CK_CG].clock);
	if (IS_ERR(&ret)) {
		pr_err("%s(), clk_prepare_enable %s fail, ret %d\n",
			__func__, adsp_clks[CLK_ADSP_CK_CG].name, ret);
		clk_disable_unprepare(adsp_clks[CLK_SCP_SYS_ADSP].clock);
		return -EINVAL;

	}
	adsp_clock_count++;

	return 0;
}

void adsp_disable_clock(void)
{
	pr_debug("%s()\n", __func__);

	adsp_clock_count--;
	clk_disable_unprepare(adsp_clks[CLK_ADSP_CK_CG].clock);
	clk_disable_unprepare(adsp_clks[CLK_SCP_SYS_ADSP].clock);
	clk_disable_unprepare(scp_clks[CLK_TOP_SCP_SEL].clock);

}

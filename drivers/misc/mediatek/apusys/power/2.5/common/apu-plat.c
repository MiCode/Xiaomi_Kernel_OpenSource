// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/pm_opp.h>

#include "apu_plat.h"
#include "apu_devfreq.h"
#include "apu_log.h"
#include "apu_gov.h"
#include "apu_of.h"
#include "apu_common.h"
#include "apu_rpc.h"

static int apu_opp_init(struct apu_dev *ad)
{
	int ret = 0;

	ret = dev_pm_opp_of_add_table(ad->dev);
	if (ret) {
		aprobe_err(ad->dev, "[%s] get opp table fail, ret = %d\n", ret);
		goto out;
	}

out:
	return ret;
}

static void apu_opp_uninit(struct apu_dev *ad)
{
	dev_pm_opp_of_remove_table(ad->dev);
}

/**
 * apu_clk_init - and obtain a reference to a clock producer.
 * @ad: apu_dev
 *
 * CGs are controlled by apusys.
 * vpu_clks {
 *       pll = <&PLL, NPUPLL>;
 *       top-mux = <&mux_clk, VPU0_TOP_SEL>, <&mux_clk, VPU1_TOP_SEL>,
 *                 <&mux_clk, VPU2_TOP_SEL>;
 *		 sys-mux = <"&mux_clk, DEMO_MUX_CORE">;
 *};
 *
 * Take above vpu clks for example, this function will get phandle, vpu_clks,
 * parsing "pll", "top-mux", "sys-mux" and "cgs".
 * Drivers must assume that the clock source is not enabled.
 *
 * of_apu_clk_get should not be called from within interrupt context.
 */
static int apu_clk_init(struct apu_dev *ad)
{
	int ret = 0;
	struct apu_clk_gp *aclk = ad->aclk;
	struct apu_clk *dst;
	struct dev_pm_opp *opp;
	ulong rate = 0;

	/* clk related setting is necessary */
	if (IS_ERR_OR_NULL(aclk))
		return -ENODEV;

	mutex_init(&aclk->clk_lock);
	aclk->dev = ad->dev;
	ret = of_apu_clk_get(ad->dev, TOPMUX_NODE, &(aclk->top_mux));
	if (ret)
		goto err;
	ret = of_apu_clk_get(ad->dev, SYSMUX_NODE, &(aclk->sys_mux));
	if (ret)
		goto err;
	ret = of_apu_clk_get(ad->dev, SYSMUX_PARENT_NODE, &(aclk->sys_mux->parents));
	if (ret)
		goto err;
	ret = of_apu_clk_get(ad->dev, APMIX_PLL_NODE, &(aclk->apmix_pll));
	if (ret)
		goto err;
	ret = of_apu_clk_get(ad->dev, TOP_PLL_NODE, &(aclk->top_pll));
	if (ret)
		goto err;

	ret = of_apu_cg_get(ad->dev, &(aclk->cg));
	if (ret)
		goto err;

	/* get the slowest frq in opp */
	opp = devfreq_recommended_opp(ad->dev, &rate, 0);
	if (IS_ERR(opp)) {
		aprobe_err(ad->dev, "[%s] no opp for %luMHz\n", TOMHZ(rate));
		return PTR_ERR(opp);
	}
	rate = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);

	/* if there is no default/shutdown freq, take them as slowest opp */
	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "top_mux def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	dst = aclk->sys_mux;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "sys_mux def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "top_pll def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst)) {
		if (!dst->def_freq)
			dst->def_freq = rate;
		if (!dst->shut_freq)
			dst->shut_freq = rate;
		aprobe_info(ad->dev, "apmix_pll def/shut %luMhz/%luMhz\n",
				TOMHZ(dst->def_freq), TOMHZ(dst->shut_freq));
	}

	/* prepare all clks */
	aclk->ops->prepare(aclk);
	return ret;

err:
	of_apu_clk_put(&(aclk->top_mux));
	of_apu_clk_put(&(aclk->sys_mux));
	of_apu_clk_put(&(aclk->top_pll));
	of_apu_clk_put(&(aclk->apmix_pll));
	of_apu_cg_put(&(aclk->cg));

	return ret;
}


static void apu_clk_uninit(struct apu_dev *ad)
{
	struct apu_clk_gp *aclk = NULL;
	struct apu_clk *dst = NULL;

	aclk = ad->aclk;
	aclk->ops->unprepare(ad->aclk);

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	dst = aclk->sys_mux;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	if (!IS_ERR_OR_NULL(aclk->cg))
		of_apu_cg_put(&(aclk->cg));

}

static int apu_regulator_init(struct apu_dev *ad)
{
	int ret = 0, volt = 0;
	struct apu_regulator *dst = NULL;
	struct dev_pm_opp *opp;
	ulong def_freq = 0;

	if (IS_ERR_OR_NULL(ad->argul))
		goto out;

	/* initial regulator gp lock */
	mutex_init(&(ad->argul->rgulgp_lock));

	/* initial individual regulator lock */
	mutex_init(&(ad->argul->rgul->reg_lock));

	ad->argul->dev = ad->dev;
	ad->argul->rgul->dev = ad->dev;

	if (ad->argul->rgul_sup) {
		ad->argul->rgul_sup->dev = ad->dev;
		mutex_init(&(ad->argul->rgul_sup->reg_lock));
	}
	/* get the slowest frq in opp and set it as default frequency */
	opp = devfreq_recommended_opp(ad->dev, &def_freq, 0);
	if (IS_ERR(opp)) {
		aprobe_err(ad->dev, "Failed to find opp for %luMhz\n", TOMHZ(def_freq));
		return PTR_ERR(opp);
	}
	volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	/* get regulator */
	dst = ad->argul->rgul;
	ret = of_apu_regulator_get(ad->dev, dst, volt, def_freq);
	if (ret)
		goto out;

	/* get regulator's supply */
	dst = ad->argul->rgul_sup;
	ret = of_apu_regulator_get(ad->dev, dst, volt, def_freq);
	if (ret)
		goto out;
out:
	return ret;
}

static void apu_regulator_uninit(struct apu_dev *ad)
{
	struct apu_regulator *dst = NULL;

	if (!IS_ERR_OR_NULL(ad->argul)) {
		dst = ad->argul->rgul;
		if (!IS_ERR_OR_NULL(dst))
			of_apu_regulator_put(dst);

		dst = ad->argul->rgul_sup;
		if (!IS_ERR_OR_NULL(dst))
			of_apu_regulator_put(dst);
	}
}

static int apu_devfreq_init(struct apu_dev *ad, struct devfreq_dev_profile *pf)
{
	struct apu_gov_data *pgov_data;
	const char *gov_name;
	u32 val;
	int err = 0;

	pgov_data = devm_kzalloc(ad->dev, sizeof(*pgov_data), GFP_KERNEL);
	if (!pgov_data)
		return -ENOMEM;

	pgov_data->parent = devfreq_get_devfreq_by_phandle(ad->dev, 0);
	/* Have no parent, no need to register parent's boost array */
	if (IS_ERR(pgov_data->parent)) {
		pgov_data->parent = NULL;
		aprobe_warn(ad->dev, "has no devfreq parent\n");
	} else {
		aprobe_info(ad->dev, "devfreq parent name \"%s\"\n",
			    dev_name(pgov_data->parent->dev.parent));
	}

	of_property_read_string(ad->dev->of_node, "gov", &gov_name);
	if (!gov_name) {
		aprobe_err(ad->dev, "failed to get a governor name\n");
		return -EINVAL;
	}
	aprobe_info(ad->dev, "governor name %s\n", gov_name);

	if (!of_property_read_u32(ad->dev->of_node, "depth", &val))
		pgov_data->depth = val;
	else
		aprobe_err(ad->dev, "failed to get depth\n");
	aprobe_info(ad->dev, "depth %d\n", pgov_data->depth);

	ad->devfreq = devm_devfreq_add_device(ad->dev, pf, gov_name, pgov_data);
	if (IS_ERR(ad->devfreq)) {
		err = PTR_ERR(ad->devfreq);
		goto free_passdata;
	}

	ad->opp_div = DIV_ROUND_CLOSEST(BOOST_MAX,
					ad->devfreq->profile->max_state);
	return err;
free_passdata:
	devm_kfree(ad->dev, pgov_data);

	return err;
}

static void apu_devfreq_uninit(struct apu_dev *ad)
{
	struct apu_gov_data *pgov_data = NULL;

	pgov_data = ad->devfreq->data;
	/* remove devfreq device */
	devm_devfreq_remove_device(ad->dev, ad->devfreq);
	devm_kfree(ad->dev, pgov_data);
}

static int apu_misc_init(struct apu_dev *ad)
{
	int ret = 0;

	if (ad->user == APUCONN)
		ret = apu_rpc_init_done(ad);

	return ret;
}

static struct apu_plat_ops apu_plat_driver[] = {
	{
		.name = "mt68xx_platops",
		.init_misc = apu_misc_init,
		.init_opps = apu_opp_init,
		.uninit_opps = apu_opp_uninit,
		.init_clks = apu_clk_init,
		.uninit_clks = apu_clk_uninit,
		.init_rguls = apu_regulator_init,
		.uninit_rguls = apu_regulator_uninit,
		.init_devfreq = apu_devfreq_init,
		.uninit_devfreq = apu_devfreq_uninit,
	},
};

struct apu_plat_ops *apu_plat_get_ops(struct apu_dev *ad, const char *name)
{
	int i = 0;

	if (!name)
		goto out;

	for (i = 0; i < ARRAY_SIZE(apu_plat_driver); i++) {
		if (strcmp(name, apu_plat_driver[i].name) == 0)
			return &apu_plat_driver[i];
	}

	aprobe_err(ad->dev, "[%s] not found platform ops \"%s\"\n", __func__);

out:
	return ERR_PTR(-ENOENT);
}


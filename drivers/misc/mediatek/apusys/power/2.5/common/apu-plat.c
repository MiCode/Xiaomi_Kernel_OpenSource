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

	ret = dev_pm_opp_of_add_table_indexed(ad->dev, 0);
	if (ret)
		goto out;

	ad->oppt = dev_pm_opp_get_opp_table(ad->dev);
	if (IS_ERR_OR_NULL(ad->oppt)) {
		ret = PTR_ERR(ad->oppt);
		aprobe_err(ad->dev, "[%s] get opp table fail, ret = %d\n", __func__, ret);
		goto out;
	}

out:
	return ret;
}

static void apu_opp_uninit(struct apu_dev *ad)
{
	if (!IS_ERR_OR_NULL(ad->oppt))
		dev_pm_opp_put_opp_table(ad->oppt);
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
	ulong rate = 0;

	/* clk related setting is necessary */
	if (IS_ERR_OR_NULL(aclk))
		return -ENODEV;

	mutex_init(&aclk->clk_lock);
	aclk->dev = ad->dev;
	ret = of_apu_clk_get(ad->dev, TOPMUX_NODE, &(aclk->top_mux));
	if (ret)
		goto out;
	ret = of_apu_clk_get(ad->dev, SYSMUX_NODE, &(aclk->sys_mux));
	if (ret)
		goto err_topmux;
	ret = of_apu_clk_get(ad->dev, SYSMUX_PARENT_NODE, &(aclk->sys_mux->parents));
	if (ret)
		goto err_sysmux;
	ret = of_apu_clk_get(ad->dev, APMIX_PLL_NODE, &(aclk->apmix_pll));
	if (ret)
		goto err_sysmux_parrent;
	ret = of_apu_clk_get(ad->dev, TOP_PLL_NODE, &(aclk->top_pll));
	if (ret)
		goto err_apmix_pll;

	ret = of_apu_cg_get(ad->dev, &(aclk->cg));
	if (ret)
		goto err_toppll;

	/* get the slowest frq in opp */
	ret = apu_get_recommend_freq_volt(ad->dev, &rate, NULL, 0);
	if (ret)
		goto err_cg;

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

	return ret;

err_cg:
	of_apu_cg_put(&(aclk->cg));
err_toppll:
	of_apu_clk_put(&(aclk->top_pll));
err_apmix_pll:
	of_apu_clk_put(&(aclk->apmix_pll));
err_sysmux_parrent:
	of_apu_clk_put(&(aclk->sys_mux->parents));
err_sysmux:
	of_apu_clk_put(&(aclk->sys_mux));
err_topmux:
	of_apu_clk_put(&(aclk->top_mux));

out:
	return ret;
}


static void apu_clk_uninit(struct apu_dev *ad)
{
	struct apu_clk_gp *aclk = NULL;
	struct apu_clk *dst = NULL;

	aclk = ad->aclk;

	dst = aclk->top_pll;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	dst = aclk->apmix_pll;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	if (!IS_ERR_OR_NULL(aclk->sys_mux)) {
		dst = aclk->sys_mux->parents;
		if (!IS_ERR_OR_NULL(dst))
			of_apu_clk_put(&dst);
		dst = aclk->sys_mux;
		of_apu_clk_put(&dst);
	}

	dst = aclk->top_mux;
	if (!IS_ERR_OR_NULL(dst))
		of_apu_clk_put(&dst);

	if (!IS_ERR_OR_NULL(aclk->cg))
		of_apu_cg_put(&(aclk->cg));

}

static int apu_regulator_init(struct apu_dev *ad)
{
	int ret = 0;
	unsigned long volt = 0, def_freq = 0;
	struct apu_regulator *dst = NULL;

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
	ret = apu_get_recommend_freq_volt(ad->dev, &def_freq, &volt, 0);
	if (ret)
		goto out;

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

static int apu_devfreq_init(struct apu_dev *ad, struct devfreq_dev_profile *pf, void *data)
{
	struct apu_gov_data *pgov_data;
	const char *gov_name = NULL;
	int err = 0;

	pgov_data = apu_gov_init(ad->dev, pf, &gov_name);
	if (IS_ERR(pgov_data)) {
		err = PTR_ERR(pgov_data);
		goto out;
	}

	ad->df = devm_devfreq_add_device(ad->dev, pf, gov_name, pgov_data);
	if (IS_ERR_OR_NULL(ad->df)) {
		err = PTR_ERR(ad->df);
		goto out;
	}

	err = apu_gov_setup(ad, data);

out:
	return err;
}

static void apu_devfreq_uninit(struct apu_dev *ad)
{
	struct apu_gov_data *pgov_data = NULL;

	pgov_data = ad->df->data;
	apu_gov_unsetup(ad);
	/* remove devfreq device */
	devm_devfreq_remove_device(ad->dev, ad->df);
	devm_kfree(ad->dev, pgov_data);
}

static int apu_misc_init(struct apu_dev *ad)
{
	int ret = 0;
	int boost, opp;
	ulong freq = 0, volt = 0;

	if (ad->user == APUCONN)
		ret = apu_rpc_init_done(ad);

	for (;;) {
		if (apupw_dbg_get_loglvl() < VERBOSE_LVL)
			break;
		if (IS_ERR(dev_pm_opp_find_freq_ceil(ad->dev, &freq)))
			break;
		apu_get_recommend_freq_volt(ad->dev, &freq, &volt, 0);
		opp = apu_freq2opp(ad, freq);
		boost = apu_opp2boost(ad, opp);
		aprobe_info(ad->dev, "[%s] opp/boost/freq/volt %d/%d/%lu/%lu\n",
			    __func__, opp, boost, freq, volt);

		if (opp != apu_volt2opp(ad, volt))
			aprobe_err(ad->dev, "[%s] apu_volt2opp get %d is wrong\n",
				   __func__, apu_volt2opp(ad, volt));

		if (boost != apu_volt2boost(ad, volt))
			aprobe_err(ad->dev, "[%s] apu_volt2boost get %d is wrong\n",
				   __func__, apu_volt2boost(ad, volt));

		if (boost != apu_freq2boost(ad, freq))
			aprobe_err(ad->dev, "[%s] apu_freq2boost get %d is wrong\n",
				   __func__, apu_freq2boost(ad, freq));

		if (freq != apu_opp2freq(ad, opp))
			aprobe_err(ad->dev, "[%s] apu_opp2freq get %d is wrong\n",
				   __func__, apu_opp2freq(ad, opp));
		freq++;
	}
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

	aprobe_err(ad->dev, "[%s] not found platform ops \"%s\"\n", __func__, name);

out:
	return ERR_PTR(-ENOENT);
}


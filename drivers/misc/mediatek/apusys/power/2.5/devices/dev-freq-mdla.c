// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include "apu_plat.h"
#include "apu_common.h"
#include "apu_devfreq.h"
#include "apu_clk.h"
#include "apu_regulator.h"
#include "apu_log.h"
#include "apu_rpc.h"
#include "apu_of.h"
#include "apu_trace.h"

static int devfreq_target(struct device *dev, unsigned long *rate,
				u32 flags)
{
	unsigned long old_rate = 0, volt = 0;
	struct apu_dev *ad = dev_get_drvdata(dev);
	struct apu_clk_ops *clk_ops = NULL;
	struct apu_regulator_ops *regul_ops = NULL;
	int err = 0;

	if (!IS_ERR_OR_NULL(ad->aclk))
		clk_ops = ad->aclk->ops;
	else
		return -EINVAL;

	if (!IS_ERR_OR_NULL(ad->argul))
		regul_ops = ad->argul->ops;

	/* try to get recommend rate and opp, here rate may be changed by thermal */
	err = apu_get_recommend_freq_volt(dev, rate, &volt, flags);
	if (err) {
		advfs_err(dev, "Failed to find opp for %lu KHz\n", *rate / KHZ);
		goto out;
	}

	old_rate = clk_ops->get_rate(ad->aclk);
	if (round_Mhz(*rate, old_rate))
		return 0;

	/* Scaling up? Scale voltage before frequency */
	if (*rate >= old_rate && regul_ops) {
		err = regul_ops->set_voltage(ad->argul, volt, volt);
		if (err)
			goto out;
	}

	/* Change frequency */
	err = clk_ops->set_rate(ad->aclk, *rate);
	if (err)
		goto out;

	/* Scaling down? Scale voltage after frequency */
	if (*rate < old_rate && regul_ops) {
		err = regul_ops->set_voltage(ad->argul, volt, volt);
		if (err)
			goto out;
	}

	/* update power tags */
	apupw_dbg_pwr_tag_update(ad, *rate, volt);

	if (regul_ops)
		advfs_info(dev, "[%s] rate %luMhz volt %dmV\n",
				__func__, TOMHZ(*rate), TOMV(volt));
	else
		advfs_info(dev, "[%s] rate %luMhz\n", __func__, TOMHZ(*rate));

out:
	return err;
}

static int devfreq_curFreq(struct device *dev, unsigned long *freq)
{
	struct apu_dev *ad = dev_get_drvdata(dev);
	struct apu_clk_ops *clk_ops = NULL;

	*freq = 0;
	if (!IS_ERR_OR_NULL(ad->aclk)) {
		clk_ops = ad->aclk->ops;
		*freq = clk_ops->get_rate(ad->aclk);
		/* user DIV_ROUND_CLOSEST to fix clk rate read as 959999695 */
		*freq = DIV_ROUND_CLOSEST(*freq, MHZ) * MHZ;
	}

	if (!*freq)
		advfs_err(dev, "[%s] fail, return %luMHz", TOMHZ(*freq));
	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static int runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct apu_dev *ad = dev_get_drvdata(dev);

	apower_info(dev, "[%s] called\n", __func__);
	ret = devfreq_suspend_device(ad->df);
	if (ret)
		return ret;

	ret = apu_mtcmos_off(ad);
	if (ret)
		return ret;

	if (!IS_ERR_OR_NULL(ad->aclk))
		ad->aclk->ops->disable(ad->aclk);

	if (!IS_ERR_OR_NULL(ad->argul))
		ad->argul->ops->disable(ad->argul);

	/* update rpc/cg trace */
	apupw_dbg_rpc_tag_update(ad);
	return ret;
}

static int runtime_resume(struct device *dev)
{
	int ret = 0;
	struct apu_dev *ad = dev_get_drvdata(dev);

	apower_info(dev, "[%s] called\n", __func__);
	if (!IS_ERR_OR_NULL(ad->argul)) {
		ret = ad->argul->ops->enable(ad->argul);
		if (ret)
			return ret;
	}

	if (!IS_ERR_OR_NULL(ad->aclk)) {
		ret = ad->aclk->ops->enable(ad->aclk);
		if (ret)
			return ret;
	}

	ret = apu_mtcmos_on(ad);
	if (ret)
		return ret;

	if (!IS_ERR_OR_NULL(ad->aclk)) {
		ret = ad->aclk->ops->cg_enable(ad->aclk);
		if (ret)
			return ret;
	}

	/* update rpc/cg trace */
	apupw_dbg_rpc_tag_update(ad);
	return devfreq_resume_device(ad->df);
}

const static struct dev_pm_ops pm_ops = {
	SET_RUNTIME_PM_OPS(runtime_suspend, runtime_resume, NULL)
};

#else
const static struct dev_pm_ops pm_ops = {};
#endif

#define MDLA_PM_OPS (&pm_ops)

static int mdla_devfreq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct devfreq_dev_profile *pf = NULL;
	struct apu_dev *ad = NULL;
	const struct apu_plat_data *apu_data = NULL;
	int err = 0;

	dev_info(&pdev->dev, "%s\n", __func__);
	apu_data = of_device_get_match_data(&pdev->dev);
	if (!apu_data) {
		dev_info(dev, " has no platform data, ret %d\n", err);
		err = -ENODEV;
		goto out;
	}

	ad = devm_kzalloc(dev, sizeof(*ad), GFP_KERNEL);
	if (!ad)
		return -ENOMEM;

	ad->dev = dev;
	ad->user = apu_data->user;
	ad->name = apu_dev_string(apu_data->user);
	/* save apu_dev to dev->driver_data */
	platform_set_drvdata(pdev, ad);
	ad->plat_ops = apu_plat_get_ops(ad, apu_data->plat_ops_name);
	if (IS_ERR_OR_NULL(ad->plat_ops)) {
		err = PTR_ERR(ad->plat_ops);
		goto free_ad;
	}
	ad->aclk = clk_apu_get_clkgp(ad, apu_data->clkgp_name);
	ad->argul = regulator_apu_gp_get(ad, apu_data->rgulgp_name);
	err = ad->plat_ops->init_opps(ad);
	if (err)
		goto free_ad;
	err = ad->plat_ops->init_clks(ad);
	if (err)
		goto uninit_opps;
	err = ad->plat_ops->init_rguls(ad);
	if (err)
		goto uninit_clks;

	pf = devm_kzalloc(dev, sizeof(struct devfreq_dev_profile), GFP_KERNEL);
	if (!pf)
		goto uninit_rguls;

	pf->get_cur_freq = devfreq_curFreq;
	if (apu_data->bypass_target)
		pf->target = devfreq_dummy_target;
	else
		pf->target = devfreq_target;

	err = ad->plat_ops->init_devfreq(ad, pf, (void *)apu_data);
	if (err)
		goto uninit_rguls;

	err = ad->plat_ops->init_misc(ad);
	if (err)
		goto uninit_devfreq;

	/* initial run time power management */
	pm_runtime_enable(dev);

	err = apu_add_devfreq(ad);
	if (err)
		goto uninit_devfreq;

	/* Enumerate child at last, since child need parent's devfreq */
	err = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (!err)
		goto out;
	aprobe_err(dev, "[%s] populate fail, ret = %d\n", __func__, err);
uninit_devfreq:
	ad->plat_ops->uninit_devfreq(ad);
uninit_rguls:
	ad->plat_ops->uninit_rguls(ad);
uninit_clks:
	ad->plat_ops->uninit_clks(ad);
uninit_opps:
	ad->plat_ops->uninit_opps(ad);
free_ad:
	devm_kfree(dev, ad);
out:
	return err;
}



static int mdla_devfreq_remove(struct platform_device *pdev)
{
	struct apu_dev *ad = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __func__);
	of_platform_depopulate(ad->dev);
	/* disable run time power management */
	pm_runtime_disable(ad->dev);
	ad->plat_ops->uninit_rguls(ad);
	ad->plat_ops->uninit_clks(ad);
	ad->plat_ops->uninit_opps(ad);
	/* remove apu_device from list */
	apu_del_devfreq(ad);
	ad->plat_ops->uninit_devfreq(ad);
	return 0;
}

static const struct apu_plat_data mt6873_mdla_data = {
	.user = MDLA,
	.clkgp_name = "mt6873_mdla",
	.rgulgp_name = "mt6873_mdla",
	.plat_ops_name = "mt68xx_platops",
};

static const struct apu_plat_data mt6873_mdla0_data = {
	.user = MDLA0,
	.bypass_target = 1,
	.clkgp_name = "mt6873_mdla0",
	.plat_ops_name = "mt68xx_platops",
};

static const struct apu_plat_data mt688x_mdla_data = {
	.user = MDLA,
	.clkgp_name = "mt688x_mdla",
	.rgulgp_name = "mt688x_mdla",
	.plat_ops_name = "mt68xx_platops",
};

static const struct apu_plat_data mt688x_mdla0_data = {
	.user = MDLA0,
	.bypass_target = 1,
	.clkgp_name = "mt688x_mdla0",
	.plat_ops_name = "mt68xx_platops",
};

static const struct apu_plat_data mt688x_mdla1_data = {
	.user = MDLA1,
	.bypass_target = 1,
	.clkgp_name = "mt688x_mdla1",
	.plat_ops_name = "mt68xx_platops",
};

static const struct of_device_id mdla_devfreq_of_match[] = {
	{ .compatible = "mtk6873,apumdla", .data = &mt6873_mdla_data },
	{ .compatible = "mtk6873,apumdla0", .data = &mt6873_mdla0_data },
	{ .compatible = "mtk688x,apumdla", .data = &mt688x_mdla_data },
	{ .compatible = "mtk688x,apumdla0", .data = &mt688x_mdla0_data },
	{ .compatible = "mtk688x,apumdla1", .data = &mt688x_mdla1_data },
	{ },
};

MODULE_DEVICE_TABLE(of, mdla_devfreq_of_match);

struct platform_driver mdla_devfreq_driver = {
	.probe	= mdla_devfreq_probe,
	.remove	= mdla_devfreq_remove,
	.driver = {
		.name = "mtk68xx,apumdla",
		.pm = MDLA_PM_OPS,
		.of_match_table = mdla_devfreq_of_match,
	},
};


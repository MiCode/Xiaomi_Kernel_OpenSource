// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/devfreq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>

#include "apu_plat.h"
#include "apu_common.h"
#include "apu_devfreq.h"
#include "apu_clk.h"
#include "apu_regulator.h"
#include "apu_log.h"
#include "apu_rpc.h"
#include "apusys_power.h"
#include "apu_of.h"
#include "apu_trace.h"

static int iommu_devfreq_target(struct device *dev,
				unsigned long *rate, u32 flags)
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
	apupw_dbg_pwr_tag_update(ad, *rate, volt);
	advfs_info(dev, "[%s] rate %luMhz volt %dmV\n",
			__func__, TOMHZ(*rate), TOMV(volt));

out:
	return err;
}

static int iommu_devfreq_curFreq(struct device *dev, unsigned long *freq)
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

	if (!IS_ERR_OR_NULL(ad->aclk))
		ad->aclk->ops->disable(ad->aclk);

	/* update rpc/cg trace */
	apupw_dbg_rpc_tag_update(ad);
	return ret;
}

static int runtime_resume(struct device *dev)
{
	int ret = 0;
	struct apu_dev *ad = dev_get_drvdata(dev);

	apower_info(dev, "[%s] called\n", __func__);

	if (!IS_ERR_OR_NULL(ad->aclk)) {
		ret = ad->aclk->ops->enable(ad->aclk);
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

#define IOMMU_PM_OPS (&pm_ops)

static int iommu_devfreq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct devfreq_dev_profile *pf = NULL;
	struct apu_dev *ad = NULL;
	const struct apu_plat_data *apu_data = NULL;
	struct device_node *con_np = NULL;
	int err = 0, con_size = 0, idx;

	dev_info(&pdev->dev, "%s\n", __func__);
	apu_data = of_device_get_match_data(&pdev->dev);
	if (!apu_data) {
		dev_info(dev, " has no platform data, ret %d\n", err);
		err = -ENODEV;
		goto out;
	}

	con_size = of_count_phandle_with_args(dev->of_node, "consumer", NULL);
	if (con_size > 0) {
		for (idx = 0; idx < con_size; idx++) {
			con_np = of_parse_phandle(dev->of_node, "consumer", idx);
			err = of_apu_link(dev, con_np, dev->of_node,
					  DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
			if (err)
				goto out;
		}
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

	pf->get_cur_freq = iommu_devfreq_curFreq;
	pf->target = iommu_devfreq_target;

	err = ad->plat_ops->init_devfreq(ad, pf, (void *)apu_data);
	if (err)
		goto uninit_rguls;

	err = apu_add_devfreq(ad);
	if (err)
		goto uninit_devfreq;

	err = ad->plat_ops->init_misc(ad);
	if (err)
		goto uninit_devfreq;

	/* initial run time power management */
	pm_runtime_enable(dev);

	/* Enumerate child at last, since child need parent's devfreq */
	err = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (!err)
		goto out;

	apower_err(dev, "[%s] populate fail\n", __func__);
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


static int iommu_devfreq_remove(struct platform_device *pdev)
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

static const struct apu_plat_data mt688x_iommu_data = {
	.user = APUIOMMU,
	.clkgp_name = "mt688x_iommu",
	.plat_ops_name = "mt68xx_platops",
};

static const struct of_device_id iommu_devfreq_of_match[] = {
	{ .compatible = "mtk688x,apuiommu", .data = &mt688x_iommu_data},
	{ },
};

MODULE_DEVICE_TABLE(of, iommu_devfreq_of_match);

struct platform_driver iommu_devfreq_driver = {
	.probe	= iommu_devfreq_probe,
	.remove	= iommu_devfreq_remove,
	.driver = {
		.name = "mtk688x,apuiommu",
		.pm = IOMMU_PM_OPS,
		.of_match_table = iommu_devfreq_of_match,
	},
};

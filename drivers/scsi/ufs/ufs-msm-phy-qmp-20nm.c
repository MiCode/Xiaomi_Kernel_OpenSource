/*
 * Copyright (c) 2013-2014, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include <linux/msm-bus.h>

#include "ufshcd.h"
#include "unipro.h"
#include "ufs-msm.h"
#include "ufs-msm-phy.h"
#include "ufs-msm-phy-qmp-20nm.h"

#define UFS_PHY_NAME "ufs_msm_phy_qmp_20nm"

static void ufs_msm_phy_qmp_20nm_phy_calibrate(struct ufs_msm_phy *phy)
{
	struct ufs_msm_phy_calibration *tbl;
	int tbl_size;
	int i;

	tbl_size = ARRAY_SIZE(phy_cal_table_rate_A);
	tbl = phy_cal_table_rate_A;

	/*
	 * calibration according phy_cal_table_rate_A happens
	 * regardless of the rate we intend to work with.
	 * Only in case we would like to work in rate B, we need
	 * to override a subset of registers of phy_cal_table_rate_A
	 * table, with phy_cal_table_rate_B table.
	 */
	for (i = 0; i < tbl_size; i++)
		writel_relaxed(tbl[i].cfg_value, phy->mmio + tbl[i].reg_offset);

	if (UFS_MSM_LIMIT_HS_RATE == PA_HS_MODE_B) {
		tbl = phy_cal_table_rate_B;
		tbl_size = ARRAY_SIZE(phy_cal_table_rate_B);

		for (i = 0; i < tbl_size; i++)
			writel_relaxed(tbl[i].cfg_value,
					phy->mmio + tbl[i].reg_offset);
	}

	/* flush buffered writes */
	mb();
}

static int ufs_msm_phy_qmp_20nm_init(struct phy *generic_phy)
{
	struct ufs_msm_phy_qmp_20nm *phy = phy_get_drvdata(generic_phy);
	struct ufs_msm_phy *phy_common = &phy->common_cfg;
	int err = 0;

	err = ufs_msm_phy_init_clks(generic_phy, phy_common);
	if (err) {
		dev_err(phy_common->dev, "%s: ufs_msm_phy_init_clks() failed %d\n",
			__func__, err);
		goto out;
	}

	err = ufs_msm_phy_init_vregulators(generic_phy, phy_common);
	if (err)
		dev_err(phy_common->dev, "%s: ufs_msm_phy_init_vregulators() failed %d\n",
			__func__, err);

out:
	return err;
}

static
void ufs_msm_phy_qmp_20nm_power_control(struct ufs_msm_phy *phy, bool val)
{
	if (val) {
		writel_relaxed(0x1, phy->mmio + UFS_PHY_POWER_DOWN_CONTROL);
		/*
		 * Before any transactions involving PHY, ensure PHY knows
		 * that it's analog rail is powered ON. This also ensures
		 * that PHY is out of power collapse before enabling the
		 * SIGDET.
		 */
		mb();
		if (phy->quirks &
			MSM_UFS_PHY_DIS_SIGDET_BEFORE_PWR_COLLAPSE) {
			writel_relaxed(0xC0,
				phy->mmio + QSERDES_RX_SIGDET_CNTRL(0));
			writel_relaxed(0xC0,
				phy->mmio + QSERDES_RX_SIGDET_CNTRL(1));
			/*
			 * make sure that SIGDET is enabled before proceeding
			 * further.
			 */
			 mb();
		}
	} else {
		 if (phy->quirks &
			MSM_UFS_PHY_DIS_SIGDET_BEFORE_PWR_COLLAPSE) {
			writel_relaxed(0x0,
				phy->mmio + QSERDES_RX_SIGDET_CNTRL(0));
			writel_relaxed(0x0,
				phy->mmio + QSERDES_RX_SIGDET_CNTRL(1));
			/*
			 * Ensure that SIGDET is disabled before PHY power
			 * collapse
			 */
			mb();
		}
		writel_relaxed(0x0, phy->mmio + UFS_PHY_POWER_DOWN_CONTROL);
		/*
		 * ensure that PHY knows its PHY analog rail is going
		 * to be powered down
		 */
		mb();
	}
}

static
void ufs_msm_phy_qmp_20nm_set_tx_lane_enable(struct ufs_msm_phy *phy, u32 val)
{
	writel_relaxed(val & UFS_PHY_TX_LANE_ENABLE_MASK,
			phy->mmio + UFS_PHY_TX_LANE_ENABLE);
	mb();
}

static inline void ufs_msm_phy_qmp_20nm_start_serdes(struct ufs_msm_phy *phy)
{
	u32 tmp;

	tmp = readl_relaxed(phy->mmio + UFS_PHY_PHY_START);
	tmp &= ~MASK_SERDES_START;
	tmp |= (1 << OFFSET_SERDES_START);
	writel_relaxed(tmp, phy->mmio + UFS_PHY_PHY_START);
	mb();
}

static int ufs_msm_phy_qmp_20nm_is_pcs_ready(struct ufs_msm_phy *phy_common)
{
	int err = 0;
	u32 val;

	err = readl_poll_timeout(phy_common->mmio + UFS_PHY_PCS_READY_STATUS,
			val, (val & MASK_PCS_READY), 10, 1000000);
	if (err)
		dev_err(phy_common->dev, "%s: poll for pcs failed err = %d\n",
			__func__, err);
	return err;
}

static void ufs_msm_phy_qmp_20nm_advertise_quirks(struct phy *generic_phy)
{
	struct ufs_msm_phy_qmp_20nm *phy =  phy_get_drvdata(generic_phy);
	struct ufs_msm_phy *phy_common = &(phy->common_cfg);

	phy_common->quirks = 0;
}

struct phy_ops ufs_msm_phy_qmp_20nm_phy_ops = {
	.init		= ufs_msm_phy_qmp_20nm_init,
	.exit		= ufs_msm_phy_exit,
	.power_on	= ufs_msm_phy_power_on,
	.power_off	= ufs_msm_phy_power_off,
	.advertise_quirks  = ufs_msm_phy_qmp_20nm_advertise_quirks,
	.owner		= THIS_MODULE,
};

struct ufs_msm_phy_specific_ops phy_20nm_ops = {
	.calibrate_phy = ufs_msm_phy_qmp_20nm_phy_calibrate,
	.start_serdes = ufs_msm_phy_qmp_20nm_start_serdes,
	.is_physical_coding_sublayer_ready = ufs_msm_phy_qmp_20nm_is_pcs_ready,
	.set_tx_lane_enable	= ufs_msm_phy_qmp_20nm_set_tx_lane_enable,
	.power_control		= ufs_msm_phy_qmp_20nm_power_control,
};

static int ufs_msm_phy_qmp_20nm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *generic_phy;
	struct ufs_msm_phy_qmp_20nm *phy;
	struct phy_provider *phy_provider;
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(dev, "%s: failed to allocate phy\n", __func__);
		err = -ENOMEM;
		goto out;
	}

	err = ufs_msm_phy_base_init(pdev, &phy->common_cfg);
	if (err) {
		dev_err(dev, "%s: phy base init failed %d\n", __func__, err);
		goto out;
	}

	phy->common_cfg.phy_spec_ops = &phy_20nm_ops;
	phy->common_cfg.cached_regs = NULL;

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		err = PTR_ERR(phy_provider);
		dev_err(dev, "%s: failed to register phy %d\n", __func__, err);
		goto out;
	}

	generic_phy = devm_phy_create(dev, &ufs_msm_phy_qmp_20nm_phy_ops, NULL);
	if (IS_ERR(generic_phy)) {
		devm_of_phy_provider_unregister(dev, phy_provider);
		err =  PTR_ERR(generic_phy);
		dev_err(dev, "%s: failed to create phy %d\n", __func__, err);
		goto out;
	}

	phy->common_cfg.dev = dev;
	phy_set_drvdata(generic_phy, phy);

	strlcpy(phy->common_cfg.name, UFS_PHY_NAME,
				sizeof(phy->common_cfg.name));

out:
	return err;
}

static int ufs_msm_phy_qmp_20nm_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *generic_phy = to_phy(dev);
	struct ufs_msm_phy *ufs_msm_phy = get_ufs_msm_phy(generic_phy);
	int err = 0;

	err = ufs_msm_phy_remove(generic_phy, ufs_msm_phy);
	if (err)
		dev_err(dev, "%s: ufs_msm_phy_remove failed = %d\n",
			__func__, err);

	return err;
}

static const struct of_device_id ufs_msm_phy_qmp_20nm_of_match[] = {
	{.compatible = "qcom,ufs-msm-phy-qmp-20nm"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_msm_phy_qmp_20nm_of_match);

static struct platform_driver ufs_msm_phy_qmp_20nm_driver = {
	.probe = ufs_msm_phy_qmp_20nm_probe,
	.remove = ufs_msm_phy_qmp_20nm_remove,
	.driver = {
		.of_match_table = ufs_msm_phy_qmp_20nm_of_match,
		.name = "ufs_msm_phy_qmp_20nm",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ufs_msm_phy_qmp_20nm_driver);

MODULE_DESCRIPTION("Universal Flash Storage (UFS) MSM PHY QMP 20nm");
MODULE_LICENSE("GPL v2");

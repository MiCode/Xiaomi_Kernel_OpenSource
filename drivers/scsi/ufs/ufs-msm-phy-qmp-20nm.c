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

static int ufs_msm_phy_qmp_20nm_phy_calibrate(struct ufs_msm_phy *ufs_msm_phy)
{
	struct ufs_msm_phy_calibration *tbl_A, *tbl_B;
	int tbl_size_A, tbl_size_B;
	int rate = UFS_MSM_LIMIT_HS_RATE;
	int err;

	tbl_size_A = ARRAY_SIZE(phy_cal_table_rate_A);
	tbl_A = phy_cal_table_rate_A;

	tbl_size_B = ARRAY_SIZE(phy_cal_table_rate_B);
	tbl_B = phy_cal_table_rate_B;

	err = ufs_msm_phy_calibrate(ufs_msm_phy, tbl_A, tbl_size_A,
						tbl_B, tbl_size_B, rate);

	if (err)
		dev_err(ufs_msm_phy->dev, "%s: ufs_msm_phy_calibrate() failed %d\n",
			__func__, err);

	return err;
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
		 * that it's analog rail is powered ON.
		 */
		mb();

		if (phy->quirks &
			MSM_UFS_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE) {
			/*
			 * Give atleast 1us delay after restoring PHY analog
			 * power.
			 */
			usleep_range(1, 2);
			writel_relaxed(0x0A, phy->mmio +
				       QSERDES_COM_SYSCLK_EN_SEL_TXBAND);
			writel_relaxed(0x08, phy->mmio +
				       QSERDES_COM_SYSCLK_EN_SEL_TXBAND);
			/*
			 * Make sure workaround is deactivated before proceeding
			 * with normal PHY operations.
			 */
			mb();
		}
	} else {
		if (phy->quirks &
			MSM_UFS_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE) {
			writel_relaxed(0x0A, phy->mmio +
				       QSERDES_COM_SYSCLK_EN_SEL_TXBAND);
			writel_relaxed(0x02, phy->mmio +
				       QSERDES_COM_SYSCLK_EN_SEL_TXBAND);
			/*
			 * Make sure that above workaround is activated before
			 * PHY analog power collapse.
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

	phy_common->quirks =
		MSM_UFS_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE;
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
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(dev, "%s: failed to allocate phy\n", __func__);
		err = -ENOMEM;
		goto out;
	}

	generic_phy = ufs_msm_phy_generic_probe(pdev, &phy->common_cfg,
				&ufs_msm_phy_qmp_20nm_phy_ops, &phy_20nm_ops);

	if (!generic_phy) {
		dev_err(dev, "%s: ufs_msm_phy_generic_probe() failed\n",
			__func__);
		err = -EIO;
		goto out;
	}

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

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
#include <linux/phy/phy.h>

#include "ufshcd.h"
#include "unipro.h"
#include "ufs-msm.h"
#include "ufs-msm-phy.h"
#include "ufs-msm-phy-qmp-28nm.h"

#define UFS_PHY_NAME "ufs_msm_phy_qmp_28nm"

static
void ufs_msm_phy_qmp_28nm_power_control(struct ufs_msm_phy *phy, bool val)
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
		if (phy->quirks & MSM_UFS_PHY_DIS_SIGDET_BEFORE_PWR_COLLAPSE) {
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
		 if (phy->quirks & MSM_UFS_PHY_DIS_SIGDET_BEFORE_PWR_COLLAPSE) {
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

static int ufs_msm_phy_qmp_28nm_init(struct phy *generic_phy)
{
	struct ufs_msm_phy_qmp_28nm *phy = phy_get_drvdata(generic_phy);
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

static int ufs_msm_phy_qmp_28nm_calibrate(struct ufs_msm_phy *ufs_msm_phy)
{
	struct ufs_msm_phy_calibration *tbl_A, *tbl_B;
	int tbl_size_A, tbl_size_B;
	int rate = UFS_MSM_LIMIT_HS_RATE;
	u8 major = ufs_msm_phy->host_ctrl_rev_major;
	u16 minor = ufs_msm_phy->host_ctrl_rev_minor;
	u16 step = ufs_msm_phy->host_ctrl_rev_step;
	int err;

	if ((major == 0x1) && (minor == 0x001) && (step == 0x0000)) {
		tbl_size_A = ARRAY_SIZE(phy_cal_table_ctrl_1_1_0_rate_A);
		tbl_A = phy_cal_table_ctrl_1_1_0_rate_A;
	} else if ((major == 0x1) && (minor == 0x001) && (step == 0x0001)) {
		tbl_size_A = ARRAY_SIZE(phy_cal_table_ctrl_1_1_1_rate_A);
		tbl_A = phy_cal_table_ctrl_1_1_1_rate_A;
	}

	tbl_B = phy_cal_table_rate_B;
	tbl_size_B = ARRAY_SIZE(phy_cal_table_rate_B);

	err = ufs_msm_phy_calibrate(ufs_msm_phy, tbl_A, tbl_size_A,
			      tbl_B, tbl_size_B, rate);
	if (err)
		dev_err(ufs_msm_phy->dev, "%s: ufs_msm_phy_calibrate() failed %d\n",
			__func__, err);

	return err;
}

static
u32 ufs_msm_phy_qmp_28nm_read_attr(struct ufs_msm_phy *phy_common, u32 attr)

{
	u32 l0, l1;

	writel_relaxed(attr, phy_common->mmio + UFS_PHY_RMMI_ATTRID);
	/* Read attribute value for both lanes */
	writel_relaxed((UFS_PHY_RMMI_CFGRD_L0 | UFS_PHY_RMMI_CFGRD_L1),
		       phy_common->mmio + UFS_PHY_RMMI_ATTR_CTRL);

	l0 = readl_relaxed(phy_common->mmio + UFS_PHY_RMMI_ATTRRDVAL_L0_STATUS);
	l1 = readl_relaxed(phy_common->mmio + UFS_PHY_RMMI_ATTRRDVAL_L1_STATUS);
	/* Both lanes should have the same value for same attribute type */
	if (unlikely(l0 != l1))
		dev_warn(phy_common->dev, "%s: attr 0x%x values are not same for Lane-0 and Lane-1, l0=0x%x, l1=0x%x",
				__func__, attr, l0, l1);

	/* must clear now */
	writel_relaxed(0x00, phy_common->mmio + UFS_PHY_RMMI_ATTR_CTRL);

	return l0;
}

static
void ufs_msm_phy_qmp_28nm_save_configuration(struct ufs_msm_phy *phy_common)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cached_phy_regs); i++)
		cached_phy_regs[i].cfg_value =
			readl_relaxed(phy_common->mmio +
				      cached_phy_regs[i].reg_offset);

	for (i = 0; i < ARRAY_SIZE(cached_phy_attr); i++)
		cached_phy_attr[i].value =
			ufs_msm_phy_qmp_28nm_read_attr(phy_common,
					cached_phy_attr[i].att);
}

static
void ufs_msm_phy_qmp_28nm_set_tx_lane_enable(struct ufs_msm_phy *phy, u32 val)
{
	writel_relaxed(val & UFS_PHY_TX_LANE_ENABLE_MASK,
			phy->mmio + UFS_PHY_TX_LANE_ENABLE);
	mb();
}

static inline void ufs_msm_phy_qmp_28nm_start_serdes(struct ufs_msm_phy *phy)
{
	u32 tmp;

	tmp = readl_relaxed(phy->mmio + UFS_PHY_PHY_START);
	tmp &= ~MASK_SERDES_START;
	tmp |= (1 << OFFSET_SERDES_START);
	writel_relaxed(tmp, phy->mmio + UFS_PHY_PHY_START);
	mb();
}

static void
ufs_msm_phy_qmp_28nm_write_attr(struct phy *generic_phy, u32 attr, u32 val)
{
	struct ufs_msm_phy_qmp_28nm *phy =  phy_get_drvdata(generic_phy);
	struct ufs_msm_phy *phy_common = &(phy->common_cfg);

	writel_relaxed(attr, phy_common->mmio + UFS_PHY_RMMI_ATTRID);
	writel_relaxed(val, phy_common->mmio + UFS_PHY_RMMI_ATTRWRVAL);
	/* update attribute for both lanes */
	writel_relaxed((UFS_PHY_RMMI_CFGWR_L0 | UFS_PHY_RMMI_CFGWR_L1),
		       phy_common->mmio + UFS_PHY_RMMI_ATTR_CTRL);
	if (is_mphy_tx_attr(attr))
		writel_relaxed((UFS_PHY_RMMI_TX_CFGUPDT_L0 |
				UFS_PHY_RMMI_TX_CFGUPDT_L1),
			       phy_common->mmio + UFS_PHY_RMMI_ATTR_CTRL);
	else
		writel_relaxed((UFS_PHY_RMMI_RX_CFGUPDT_L0 |
				UFS_PHY_RMMI_RX_CFGUPDT_L1),
			       phy_common->mmio + UFS_PHY_RMMI_ATTR_CTRL);

	writel_relaxed(0x00, phy_common->mmio + UFS_PHY_RMMI_ATTR_CTRL);
}

static void ufs_msm_phy_qmp_28nm_restore_attrs(struct phy *generic_phy)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cached_phy_attr); i++)
		ufs_msm_phy_qmp_28nm_write_attr(generic_phy,
			cached_phy_attr[i].att, cached_phy_attr[i].value);
}

static int ufs_msm_phy_qmp_28nm_is_pcs_ready(struct ufs_msm_phy *phy_common)
{
	int err = 0;
	u32 val;

	err = readl_poll_timeout(phy_common->mmio + UFS_PHY_PCS_READY_STATUS,
			val, (val & MASK_PCS_READY), 10, 1000000);
	if (err)
		dev_err(phy_common->dev, "%s: phy init failed, %d\n",
			__func__, err);

	return err;
}

static
void ufs_msm_phy_qmp_28nm_advertise_quirks(struct phy *generic_phy)
{
	struct ufs_msm_phy_qmp_28nm *phy =  phy_get_drvdata(generic_phy);
	struct ufs_msm_phy *phy_common = &(phy->common_cfg);

	phy_common->quirks = MSM_UFS_PHY_QUIRK_CFG_RESTORE
				| MSM_UFS_PHY_DIS_SIGDET_BEFORE_PWR_COLLAPSE;
}

static int ufs_msm_phy_qmp_28nm_suspend(struct phy *generic_phy)
{
	struct ufs_msm_phy_qmp_28nm *phy =  phy_get_drvdata(generic_phy);
	struct ufs_msm_phy *phy_common = &(phy->common_cfg);

	ufs_msm_phy_disable_ref_clk(generic_phy);
	ufs_msm_phy_qmp_28nm_power_control(phy_common, false);

	ufs_msm_phy_disable_vreg(generic_phy, &phy_common->vdda_phy);
	ufs_msm_phy_disable_vreg(generic_phy, &phy_common->vdda_pll);

	return 0;
}

static int ufs_msm_phy_qmp_28nm_resume(struct phy *generic_phy)
{
	struct ufs_msm_phy_qmp_28nm *phy = phy_get_drvdata(generic_phy);
	struct ufs_msm_phy *phy_common = &phy->common_cfg;
	int err = 0;

	ufs_msm_phy_qmp_28nm_start_serdes(phy_common);

	ufs_msm_phy_qmp_28nm_restore_attrs(generic_phy);

	err = ufs_msm_phy_qmp_28nm_is_pcs_ready(phy_common);
	if (err)
		dev_err(phy_common->dev, "%s: failed to init phy = %d\n",
			__func__, err);

	return err;
}

struct phy_ops ufs_msm_phy_qmp_28nm_phy_ops = {
	.init		= ufs_msm_phy_qmp_28nm_init,
	.exit		= ufs_msm_phy_exit,
	.power_on	= ufs_msm_phy_power_on,
	.power_off	= ufs_msm_phy_power_off,
	.advertise_quirks  = ufs_msm_phy_qmp_28nm_advertise_quirks,
	.suspend	= ufs_msm_phy_qmp_28nm_suspend,
	.resume		= ufs_msm_phy_qmp_28nm_resume,
	.owner		= THIS_MODULE,
};

struct ufs_msm_phy_specific_ops phy_28nm_ops = {
	.calibrate_phy		= ufs_msm_phy_qmp_28nm_calibrate,
	.start_serdes		= ufs_msm_phy_qmp_28nm_start_serdes,
	.save_configuration	= ufs_msm_phy_qmp_28nm_save_configuration,
	.is_physical_coding_sublayer_ready = ufs_msm_phy_qmp_28nm_is_pcs_ready,
	.set_tx_lane_enable	= ufs_msm_phy_qmp_28nm_set_tx_lane_enable,
	.power_control		= ufs_msm_phy_qmp_28nm_power_control,
};

static int ufs_msm_phy_qmp_28nm_probe(struct platform_device *pdev)
{
	struct ufs_msm_phy_qmp_28nm *phy;
	struct device *dev = &pdev->dev;
	int err = 0;
	struct phy *generic_phy;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		dev_err(dev, "%s: failed to allocate phy\n", __func__);
		goto out;
	}

	phy->common_cfg.cached_regs =
			(struct ufs_msm_phy_calibration *)cached_phy_regs;
	phy->common_cfg.cached_regs_table_size =
				ARRAY_SIZE(cached_phy_regs);

	generic_phy = ufs_msm_phy_generic_probe(pdev, &phy->common_cfg,
				&ufs_msm_phy_qmp_28nm_phy_ops, &phy_28nm_ops);

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

static int ufs_msm_phy_qmp_28nm_remove(struct platform_device *pdev)
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

static const struct of_device_id ufs_msm_phy_qmp_28nm_of_match[] = {
	{.compatible = "qcom,ufs-msm-phy-qmp-28nm"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_msm_phy_qmp_28nm_of_match);

static struct platform_driver ufs_msm_phy_qmp_28nm_driver = {
	.probe = ufs_msm_phy_qmp_28nm_probe,
	.remove = ufs_msm_phy_qmp_28nm_remove,
	.driver = {
		.of_match_table = ufs_msm_phy_qmp_28nm_of_match,
		.name = "ufs_msm_phy_qmp_28nm",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ufs_msm_phy_qmp_28nm_driver);

MODULE_DESCRIPTION("Universal Flash Storage (UFS) MSM PHY QMP 28nm");
MODULE_LICENSE("GPL v2");

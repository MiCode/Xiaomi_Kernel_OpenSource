/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "phy-qcom-ufs-qmp-v4.h"

#define UFS_PHY_NAME "ufs_phy_qmp_v4"

static
int ufs_qcom_phy_qmp_v4_phy_calibrate(struct ufs_qcom_phy *ufs_qcom_phy,
					bool is_rate_B)
{
	u8 major = ufs_qcom_phy->host_ctrl_rev_major;
	u16 minor = ufs_qcom_phy->host_ctrl_rev_minor;
	u16 step = ufs_qcom_phy->host_ctrl_rev_step;

	writel_relaxed(0x01, ufs_qcom_phy->mmio + UFS_PHY_SW_RESET);
	/* Ensure PHY is in reset before writing PHY calibration data */
	wmb();

	/*
	 * Writing PHY calibration in this order:
	 * 1. Write Rate-A calibration first (1-lane mode).
	 * 2. Write 2nd lane configuration if needed.
	 * 3. Write Rate-B calibration overrides
	 */
	ufs_qcom_phy_write_tbl(ufs_qcom_phy, phy_cal_table_rate_A,
			       ARRAY_SIZE(phy_cal_table_rate_A));
	if (ufs_qcom_phy->lanes_per_direction == 2)
		ufs_qcom_phy_write_tbl(ufs_qcom_phy, phy_cal_table_2nd_lane,
				       ARRAY_SIZE(phy_cal_table_2nd_lane));
	if (is_rate_B)
		ufs_qcom_phy_write_tbl(ufs_qcom_phy, phy_cal_table_rate_B,
				       ARRAY_SIZE(phy_cal_table_rate_B));

	if ((major == 0x4) && (minor == 0x000) && (step == 0x0000)) {
		writel_relaxed(0x01, ufs_qcom_phy->mmio +
					QSERDES_RX0_AC_JTAG_ENABLE);
		writel_relaxed(0x01, ufs_qcom_phy->mmio +
					QSERDES_RX0_AC_JTAG_MODE);
		writel_relaxed(0x01, ufs_qcom_phy->mmio +
					QSERDES_RX1_AC_JTAG_ENABLE);
		writel_relaxed(0x01, ufs_qcom_phy->mmio +
					QSERDES_RX1_AC_JTAG_MODE);
	}

	writel_relaxed(0x00, ufs_qcom_phy->mmio + UFS_PHY_SW_RESET);
	/* flush buffered writes */
	wmb();

	return 0;
}

static int ufs_qcom_phy_qmp_v4_init(struct phy *generic_phy)
{
	struct ufs_qcom_phy_qmp_v4 *phy = phy_get_drvdata(generic_phy);
	struct ufs_qcom_phy *phy_common = &phy->common_cfg;
	int err;

	err = ufs_qcom_phy_init_clks(phy_common);
	if (err) {
		dev_err(phy_common->dev, "%s: ufs_qcom_phy_init_clks() failed %d\n",
			__func__, err);
		goto out;
	}

	err = ufs_qcom_phy_init_vregulators(phy_common);
	if (err) {
		dev_err(phy_common->dev, "%s: ufs_qcom_phy_init_vregulators() failed %d\n",
			__func__, err);
		goto out;
	}

out:
	return err;
}

static int ufs_qcom_phy_qmp_v4_exit(struct phy *generic_phy)
{
	return 0;
}

static
void ufs_qcom_phy_qmp_v4_power_control(struct ufs_qcom_phy *phy,
					 bool power_ctrl)
{
	if (!power_ctrl) {
		/* apply analog power collapse */
		writel_relaxed(0x0, phy->mmio + UFS_PHY_POWER_DOWN_CONTROL);
		/*
		 * Make sure that PHY knows its analog rail is going to be
		 * powered OFF.
		 */
		mb();
	} else {
		/* bring PHY out of analog power collapse */
		writel_relaxed(0x1, phy->mmio + UFS_PHY_POWER_DOWN_CONTROL);

		/*
		 * Before any transactions involving PHY, ensure PHY knows
		 * that it's analog rail is powered ON.
		 */
		mb();
	}
}

static inline
void ufs_qcom_phy_qmp_v4_set_tx_lane_enable(struct ufs_qcom_phy *phy, u32 val)
{
	/*
	 * v4 PHY does not have TX_LANE_ENABLE register.
	 * Implement this function so as not to propagate error to caller.
	 */
}

static
void ufs_qcom_phy_qmp_v4_ctrl_rx_linecfg(struct ufs_qcom_phy *phy, bool ctrl)
{
	u32 temp;

	temp = readl_relaxed(phy->mmio + UFS_PHY_LINECFG_DISABLE);

	if (ctrl) /* enable RX LineCfg */
		temp &= ~UFS_PHY_RX_LINECFG_DISABLE_BIT;
	else /* disable RX LineCfg */
		temp |= UFS_PHY_RX_LINECFG_DISABLE_BIT;

	writel_relaxed(temp, phy->mmio + UFS_PHY_LINECFG_DISABLE);
	/* make sure that RX LineCfg config applied before we return */
	mb();
}

static inline void ufs_qcom_phy_qmp_v4_start_serdes(struct ufs_qcom_phy *phy)
{
	u32 tmp;

	tmp = readl_relaxed(phy->mmio + UFS_PHY_PHY_START);
	tmp &= ~MASK_SERDES_START;
	tmp |= (1 << OFFSET_SERDES_START);
	writel_relaxed(tmp, phy->mmio + UFS_PHY_PHY_START);
	/* Ensure register value is committed */
	mb();
}

static int ufs_qcom_phy_qmp_v4_is_pcs_ready(struct ufs_qcom_phy *phy_common)
{
	int err = 0;
	u32 val;

	err = readl_poll_timeout(phy_common->mmio + UFS_PHY_PCS_READY_STATUS,
		val, (val & MASK_PCS_READY), 10, 1000000);
	if (err) {
		dev_err(phy_common->dev, "%s: poll for pcs failed err = %d\n",
			__func__, err);
		goto out;
	}

out:
	return err;
}

static void ufs_qcom_phy_qmp_v4_dbg_register_dump(struct ufs_qcom_phy *phy)
{
	ufs_qcom_phy_dump_regs(phy, COM_BASE, COM_SIZE,
					"PHY QSERDES COM Registers ");
	ufs_qcom_phy_dump_regs(phy, PHY_BASE, PHY_SIZE,
					"PHY Registers ");
	ufs_qcom_phy_dump_regs(phy, RX_BASE(0), RX_SIZE,
					"PHY RX0 Registers ");
	ufs_qcom_phy_dump_regs(phy, TX_BASE(0), TX_SIZE,
					"PHY TX0 Registers ");
	ufs_qcom_phy_dump_regs(phy, RX_BASE(1), RX_SIZE,
					"PHY RX1 Registers ");
	ufs_qcom_phy_dump_regs(phy, TX_BASE(1), TX_SIZE,
					"PHY TX1 Registers ");
}

struct phy_ops ufs_qcom_phy_qmp_v4_phy_ops = {
	.init		= ufs_qcom_phy_qmp_v4_init,
	.exit		= ufs_qcom_phy_qmp_v4_exit,
	.power_on	= ufs_qcom_phy_power_on,
	.power_off	= ufs_qcom_phy_power_off,
	.owner		= THIS_MODULE,
};

struct ufs_qcom_phy_specific_ops phy_v4_ops = {
	.calibrate_phy		= ufs_qcom_phy_qmp_v4_phy_calibrate,
	.start_serdes		= ufs_qcom_phy_qmp_v4_start_serdes,
	.is_physical_coding_sublayer_ready = ufs_qcom_phy_qmp_v4_is_pcs_ready,
	.set_tx_lane_enable	= ufs_qcom_phy_qmp_v4_set_tx_lane_enable,
	.ctrl_rx_linecfg	= ufs_qcom_phy_qmp_v4_ctrl_rx_linecfg,
	.power_control		= ufs_qcom_phy_qmp_v4_power_control,
	.dbg_register_dump	= ufs_qcom_phy_qmp_v4_dbg_register_dump,
};

static int ufs_qcom_phy_qmp_v4_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *generic_phy;
	struct ufs_qcom_phy_qmp_v4 *phy;
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		goto out;
	}

	generic_phy = ufs_qcom_phy_generic_probe(pdev, &phy->common_cfg,
				&ufs_qcom_phy_qmp_v4_phy_ops, &phy_v4_ops);

	if (!generic_phy) {
		dev_err(dev, "%s: ufs_qcom_phy_generic_probe() failed\n",
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

static const struct of_device_id ufs_qcom_phy_qmp_v4_of_match[] = {
	{.compatible = "qcom,ufs-phy-qmp-v4"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_qcom_phy_qmp_v4_of_match);

static struct platform_driver ufs_qcom_phy_qmp_v4_driver = {
	.probe = ufs_qcom_phy_qmp_v4_probe,
	.driver = {
		.of_match_table = ufs_qcom_phy_qmp_v4_of_match,
		.name = "ufs_qcom_phy_qmp_v4",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ufs_qcom_phy_qmp_v4_driver);

MODULE_DESCRIPTION("Universal Flash Storage (UFS) QCOM PHY QMP v4");
MODULE_LICENSE("GPL v2");

/*
 * Copyright (c) 2013-2015, 2018, Linux Foundation. All rights reserved.
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

#include "phy-qcom-ufs-qmp-14nm.h"

#define UFS_PHY_NAME "ufs_phy_qmp_14nm"

static
int ufs_qcom_phy_qmp_14nm_phy_calibrate(struct ufs_qcom_phy *ufs_qcom_phy,
					bool is_rate_B, bool is_g4)
{
	int err;
	int tbl_size_A, tbl_size_B;
	struct ufs_qcom_phy_calibration *tbl_A, *tbl_B;
	u8 major = ufs_qcom_phy->host_ctrl_rev_major;
	u16 minor = ufs_qcom_phy->host_ctrl_rev_minor;
	u16 step = ufs_qcom_phy->host_ctrl_rev_step;

	tbl_size_B = ARRAY_SIZE(phy_cal_table_rate_B);
	tbl_B = phy_cal_table_rate_B;

	if ((major == 0x2) && (minor == 0x000) && (step == 0x0000)) {
		tbl_A = phy_cal_table_rate_A_2_0_0;
		tbl_size_A = ARRAY_SIZE(phy_cal_table_rate_A_2_0_0);
	} else if ((major == 0x2) && (minor == 0x001) && (step == 0x0000)) {
		tbl_A = phy_cal_table_rate_A_2_1_0;
		tbl_size_A = ARRAY_SIZE(phy_cal_table_rate_A_2_1_0);
	} else if ((major == 0x2) && (minor == 0x002) && (step == 0x0000)) {
		tbl_A = phy_cal_table_rate_A_2_2_0;
		tbl_size_A = ARRAY_SIZE(phy_cal_table_rate_A_2_2_0);
		tbl_B = phy_cal_table_rate_B_2_2_0;
		tbl_size_B = ARRAY_SIZE(phy_cal_table_rate_B_2_2_0);
	} else {
		dev_err(ufs_qcom_phy->dev,
			"%s: Unknown UFS-PHY version (major 0x%x minor 0x%x step 0x%x), no calibration values\n",
			__func__, major, minor, step);
		err = -ENODEV;
		goto out;
	}

	err = ufs_qcom_phy_calibrate(ufs_qcom_phy,
				     tbl_A, tbl_size_A,
				     tbl_B, tbl_size_B,
				     is_rate_B);

	if (ufs_qcom_phy->quirks & UFS_QCOM_PHY_QUIRK_VCO_MANUAL_TUNING)
		writel_relaxed(ufs_qcom_phy->vco_tune1_mode1,
			ufs_qcom_phy->mmio + QSERDES_COM_VCO_TUNE1_MODE1);
out:
	if (err)
		dev_err(ufs_qcom_phy->dev,
			"%s: ufs_qcom_phy_calibrate() failed %d\n",
			__func__, err);
	return err;
}

static
void ufs_qcom_phy_qmp_14nm_advertise_quirks(struct ufs_qcom_phy *phy_common)
{
	u8 major = phy_common->host_ctrl_rev_major;
	u16 minor = phy_common->host_ctrl_rev_minor;
	u16 step = phy_common->host_ctrl_rev_step;

	if ((major == 0x2) && (minor == 0x000) && (step == 0x0000))
		phy_common->quirks =
			UFS_QCOM_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE |
			UFS_QCOM_PHY_QUIRK_SVS_MODE |
			UFS_QCOM_PHY_QUIRK_VCO_MANUAL_TUNING;
}

static int ufs_qcom_phy_qmp_14nm_init(struct phy *generic_phy)
{
	struct ufs_qcom_phy_qmp_14nm *phy = phy_get_drvdata(generic_phy);
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

	ufs_qcom_phy_qmp_14nm_advertise_quirks(phy_common);

	if (phy_common->quirks & UFS_QCOM_PHY_QUIRK_VCO_MANUAL_TUNING) {
		phy_common->vco_tune1_mode1 = readl_relaxed(phy_common->mmio +
						QSERDES_COM_VCO_TUNE1_MODE1);
		dev_info(phy_common->dev, "%s: vco_tune1_mode1 0x%x\n",
			__func__, phy_common->vco_tune1_mode1);
	}

out:
	return err;
}

static int ufs_qcom_phy_qmp_14nm_exit(struct phy *generic_phy)
{
	return 0;
}

static
void ufs_qcom_phy_qmp_14nm_power_control(struct ufs_qcom_phy *phy,
					 bool power_ctrl)
{
	bool is_workaround_req = false;

	if (phy->quirks &
	    UFS_QCOM_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE)
		is_workaround_req = true;

	if (!power_ctrl) {
		/* apply PHY analog power collapse */
		if (is_workaround_req) {
			/* assert common reset before analog power collapse */
			writel_relaxed(0x1, phy->mmio + QSERDES_COM_SW_RESET);
			/*
			 * make sure that reset is propogated before analog
			 * power collapse
			 */
			mb();
		}
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
		if (is_workaround_req) {
			/*
			 * de-assert common reset after coming out of analog
			 * power collapse
			 */
			writel_relaxed(0x0, phy->mmio + QSERDES_COM_SW_RESET);
			/* make common reset is de-asserted before proceeding */
			mb();
		}
	}
}

static inline
void ufs_qcom_phy_qmp_14nm_set_tx_lane_enable(struct ufs_qcom_phy *phy, u32 val)
{
	/*
	 * 14nm PHY does not have TX_LANE_ENABLE register.
	 * Implement this function so as not to propagate error to caller.
	 */
}

static
void ufs_qcom_phy_qmp_14nm_ctrl_rx_linecfg(struct ufs_qcom_phy *phy, bool ctrl)
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

static inline void ufs_qcom_phy_qmp_14nm_start_serdes(struct ufs_qcom_phy *phy)
{
	u32 tmp;

	tmp = readl_relaxed(phy->mmio + UFS_PHY_PHY_START);
	tmp &= ~MASK_SERDES_START;
	tmp |= (1 << OFFSET_SERDES_START);
	writel_relaxed(tmp, phy->mmio + UFS_PHY_PHY_START);
	/* Ensure register value is committed */
	mb();
}

static int ufs_qcom_phy_qmp_14nm_is_pcs_ready(struct ufs_qcom_phy *phy_common)
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

	if (phy_common->quirks & UFS_QCOM_PHY_QUIRK_SVS_MODE) {
		int i;

		for (i = 0; i < ARRAY_SIZE(phy_svs_mode_config_2_0_0); i++)
			writel_relaxed(phy_svs_mode_config_2_0_0[i].cfg_value,
				(phy_common->mmio +
				phy_svs_mode_config_2_0_0[i].reg_offset));
		/* apply above configuration immediately */
		mb();
	}

out:
	return err;
}

static const struct phy_ops ufs_qcom_phy_qmp_14nm_phy_ops = {
	.init		= ufs_qcom_phy_qmp_14nm_init,
	.exit		= ufs_qcom_phy_qmp_14nm_exit,
	.power_on	= ufs_qcom_phy_power_on,
	.power_off	= ufs_qcom_phy_power_off,
	.owner		= THIS_MODULE,
};

static struct ufs_qcom_phy_specific_ops phy_14nm_ops = {
	.calibrate_phy		= ufs_qcom_phy_qmp_14nm_phy_calibrate,
	.start_serdes		= ufs_qcom_phy_qmp_14nm_start_serdes,
	.is_physical_coding_sublayer_ready = ufs_qcom_phy_qmp_14nm_is_pcs_ready,
	.set_tx_lane_enable	= ufs_qcom_phy_qmp_14nm_set_tx_lane_enable,
	.ctrl_rx_linecfg	= ufs_qcom_phy_qmp_14nm_ctrl_rx_linecfg,
	.power_control		= ufs_qcom_phy_qmp_14nm_power_control,
};

static int ufs_qcom_phy_qmp_14nm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *generic_phy;
	struct ufs_qcom_phy_qmp_14nm *phy;
	struct ufs_qcom_phy *phy_common;
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		goto out;
	}
	phy_common = &phy->common_cfg;

	generic_phy = ufs_qcom_phy_generic_probe(pdev, phy_common,
				&ufs_qcom_phy_qmp_14nm_phy_ops, &phy_14nm_ops);

	if (!generic_phy) {
		dev_err(dev, "%s: ufs_qcom_phy_generic_probe() failed\n",
			__func__);
		err = -EIO;
		goto out;
	}

	phy_set_drvdata(generic_phy, phy);

	strlcpy(phy_common->name, UFS_PHY_NAME, sizeof(phy_common->name));

out:
	return err;
}

static const struct of_device_id ufs_qcom_phy_qmp_14nm_of_match[] = {
	{.compatible = "qcom,ufs-phy-qmp-14nm"},
	{.compatible = "qcom,msm8996-ufs-phy-qmp-14nm"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_qcom_phy_qmp_14nm_of_match);

static struct platform_driver ufs_qcom_phy_qmp_14nm_driver = {
	.probe = ufs_qcom_phy_qmp_14nm_probe,
	.driver = {
		.of_match_table = ufs_qcom_phy_qmp_14nm_of_match,
		.name = "ufs_qcom_phy_qmp_14nm",
	},
};

module_platform_driver(ufs_qcom_phy_qmp_14nm_driver);

MODULE_DESCRIPTION("Universal Flash Storage (UFS) QCOM PHY QMP 14nm");
MODULE_LICENSE("GPL v2");

/*
 * Copyright (c) 2013-2015, Linux Foundation. All rights reserved.
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
#define UFS_PHY_VDDA_PHY_UV	(925000)

static
int ufs_qcom_phy_qmp_14nm_phy_calibrate(struct ufs_qcom_phy *ufs_qcom_phy,
					bool is_rate_B)
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
			UFS_QCOM_PHY_QUIRK_SVS_MODE;
}

static int ufs_qcom_phy_qmp_14nm_init(struct phy *generic_phy)
{
	struct ufs_qcom_phy_qmp_14nm *phy = phy_get_drvdata(generic_phy);
	struct ufs_qcom_phy *phy_common = &phy->common_cfg;
	int err;

	err = ufs_qcom_phy_init_clks(generic_phy, phy_common);
	if (err) {
		dev_err(phy_common->dev, "%s: ufs_qcom_phy_init_clks() failed %d\n",
			__func__, err);
		goto out;
	}

	err = ufs_qcom_phy_init_vregulators(generic_phy, phy_common);
	if (err) {
		dev_err(phy_common->dev, "%s: ufs_qcom_phy_init_vregulators() failed %d\n",
			__func__, err);
		goto out;
	}
	phy_common->vdda_phy.max_uV = UFS_PHY_VDDA_PHY_UV;
	phy_common->vdda_phy.min_uV = UFS_PHY_VDDA_PHY_UV;

	ufs_qcom_phy_qmp_14nm_advertise_quirks(phy_common);

out:
	return err;
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
/*
 * This additional sequence is required as a workaround for following bug:
 * Due to missing reset in the UFS PHY logic, the pll may not lock following
 * analog power collapse. As a result the common block of the PHY must be put
 * into reset during hibernate entry and taken out of reset during hibernate
 * exit. The following sequence is required to save the calibrated VCO codes.
 * Saving the codes will save substantial time on hibernate exit
 * (<50us vs. 1.7ms).
 */
static void
ufs_qcom_phy_qmp_14nm_save_calibrated_vco_code(struct ufs_qcom_phy *phy)
{
	u32 vco_tune_mode0, vco_tune_mode1;
	u32 temp;

	/* set common debug bus select */
	writel_relaxed(0x02, phy->mmio + QSERDES_COM_DEBUG_BUS_SEL);
	/* apply debug bus select before reading the debug bus registers */
	mb();

	/* vco_tune_mode0[7:0]: Read VCO tuning code for Series A */
	vco_tune_mode0 = readl_relaxed(phy->mmio + QSERDES_COM_DEBUG_BUS0)
			 & 0xFF;

	temp = readl_relaxed(phy->mmio + QSERDES_COM_DEBUG_BUS1);
	/* vco_tune_mode0[9:8]: Read VCO tuning code for Series A */
	vco_tune_mode0 |= (temp & 0x300);
	/* vco_tune_mode1[5:0]: Read VCO tuning code for Series B */
	vco_tune_mode1 = temp & 0x3F;

	temp = readl_relaxed(phy->mmio + QSERDES_COM_DEBUG_BUS2);
	/* vco_tune_mode1[9:6]: Read VCO tuning code for Series B */
	vco_tune_mode1 |= (temp & 0x3C0);

	/* vco_tune_mode0[7:0] */
	writel_relaxed((vco_tune_mode0 & 0xff),
			phy->mmio + QSERDES_COM_VCO_TUNE1_MODE0);
	/* vco_tune_mode0[9:8] */
	writel_relaxed((vco_tune_mode0 & 0x300) >> 8,
			phy->mmio + QSERDES_COM_VCO_TUNE2_MODE0);
	/* vco_tune_mode1[7:0] */
	writel_relaxed((vco_tune_mode1 & 0xff),
			phy->mmio + QSERDES_COM_VCO_TUNE1_MODE1);
	/* vco_tune_mode1[9:8] */
	writel_relaxed((vco_tune_mode1 & 0x300) >> 8,
			phy->mmio + QSERDES_COM_VCO_TUNE2_MODE1);
	/* apply vco tuning codes before enabling the vco bypass mode */
	mb();

	temp = readl_relaxed(phy->mmio + QSERDES_COM_VCO_TUNE_CTRL);
	/*
	 * Bypass the calibrated code and use the stored code for
	 * both A and B series
	 */
	writel_relaxed((temp | 0xC), phy->mmio + QSERDES_COM_VCO_TUNE_CTRL);
	/* apply this configuration before return */
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

	if (phy_common->quirks &
	    UFS_QCOM_PHY_QUIRK_HIBERN8_EXIT_AFTER_PHY_PWR_COLLAPSE)
		ufs_qcom_phy_qmp_14nm_save_calibrated_vco_code(phy_common);

out:
	return err;
}

struct phy_ops ufs_qcom_phy_qmp_14nm_phy_ops = {
	.init		= ufs_qcom_phy_qmp_14nm_init,
	.exit		= ufs_qcom_phy_exit,
	.power_on	= ufs_qcom_phy_power_on,
	.power_off	= ufs_qcom_phy_power_off,
	.owner		= THIS_MODULE,
};

struct ufs_qcom_phy_specific_ops phy_14nm_ops = {
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
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(dev, "%s: failed to allocate phy\n", __func__);
		err = -ENOMEM;
		goto out;
	}

	generic_phy = ufs_qcom_phy_generic_probe(pdev, &phy->common_cfg,
				&ufs_qcom_phy_qmp_14nm_phy_ops, &phy_14nm_ops);

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

static int ufs_qcom_phy_qmp_14nm_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *generic_phy = to_phy(dev);
	struct ufs_qcom_phy *ufs_qcom_phy = get_ufs_qcom_phy(generic_phy);
	int err = 0;

	err = ufs_qcom_phy_remove(generic_phy, ufs_qcom_phy);
	if (err)
		dev_err(dev, "%s: ufs_qcom_phy_remove failed = %d\n",
			__func__, err);

	return err;
}

static const struct of_device_id ufs_qcom_phy_qmp_14nm_of_match[] = {
	{.compatible = "qcom,ufs-phy-qmp-14nm"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_qcom_phy_qmp_14nm_of_match);

static struct platform_driver ufs_qcom_phy_qmp_14nm_driver = {
	.probe = ufs_qcom_phy_qmp_14nm_probe,
	.remove = ufs_qcom_phy_qmp_14nm_remove,
	.driver = {
		.of_match_table = ufs_qcom_phy_qmp_14nm_of_match,
		.name = "ufs_qcom_phy_qmp_14nm",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ufs_qcom_phy_qmp_14nm_driver);

MODULE_DESCRIPTION("Universal Flash Storage (UFS) QCOM PHY QMP 14nm");
MODULE_LICENSE("GPL v2");

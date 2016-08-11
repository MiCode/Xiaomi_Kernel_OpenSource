/*
 * Copyright (c) 2013-2016, Linux Foundation. All rights reserved.
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

#include "phy-qcom-ufs-qrbtc-v2.h"

#define UFS_PHY_NAME "ufs_phy_qrbtc_v2"

static
int ufs_qcom_phy_qrbtc_v2_phy_calibrate(struct ufs_qcom_phy *ufs_qcom_phy,
					bool is_rate_B)
{
	int err;
	int tbl_size_A;
	struct ufs_qcom_phy_calibration *tbl_A;
	struct ufs_qcom_phy_qrbtc_v2 *qrbtc_phy = container_of(ufs_qcom_phy,
				struct ufs_qcom_phy_qrbtc_v2, common_cfg);

	writel_relaxed(0x15f, qrbtc_phy->u11_regs + U11_UFS_RESET_REG_OFFSET);

	/* 50ms are required to stabilize the reset */
	usleep_range(50000, 50100);
	writel_relaxed(0x0, qrbtc_phy->u11_regs + U11_UFS_RESET_REG_OFFSET);

	/* Set R3PC REF CLK */
	writel_relaxed(0x80, qrbtc_phy->u11_regs + U11_QRBTC_CONTROL_OFFSET);


	tbl_A = phy_cal_table_rate_A;
	tbl_size_A = ARRAY_SIZE(phy_cal_table_rate_A);

	err = ufs_qcom_phy_calibrate(ufs_qcom_phy,
				     tbl_A, tbl_size_A,
				     NULL, 0,
				     false);

	if (err)
		dev_err(ufs_qcom_phy->dev,
			"%s: ufs_qcom_phy_calibrate() failed %d\n",
			__func__, err);

	return err;
}

static int
ufs_qcom_phy_qrbtc_v2_is_pcs_ready(struct ufs_qcom_phy *phy_common)
{
	int err = 0;
	u32 val;
	struct ufs_qcom_phy_qrbtc_v2 *qrbtc_phy = container_of(phy_common,
				struct ufs_qcom_phy_qrbtc_v2, common_cfg);

	/*
	 * The value we are polling for is 0x3D which represents the
	 * following masks:
	 * RESET_SM field: 0x5
	 * RESTRIMDONE bit: BIT(3)
	 * PLLLOCK bit: BIT(4)
	 * READY bit: BIT(5)
	 */
	#define QSERDES_COM_RESET_SM_REG_POLL_VAL	0x3D
	err = readl_poll_timeout(phy_common->mmio + QSERDES_COM_RESET_SM,
		val, (val == QSERDES_COM_RESET_SM_REG_POLL_VAL), 10, 1000000);

	if (err)
		dev_err(phy_common->dev, "%s: poll for pcs failed err = %d\n",
			__func__, err);

	writel_relaxed(0x100, qrbtc_phy->u11_regs + U11_QRBTC_TX_CLK_CTRL);

	return err;
}

static void ufs_qcom_phy_qrbtc_v2_start_serdes(struct ufs_qcom_phy *phy)
{
	u32 temp;

	writel_relaxed(0x01, phy->mmio + UFS_PHY_POWER_DOWN_CONTROL_OFFSET);

	temp = readl_relaxed(phy->mmio + UFS_PHY_PHY_START_OFFSET);
	temp |= 0x1;
	writel_relaxed(temp, phy->mmio + UFS_PHY_PHY_START_OFFSET);

	/* Ensure register value is committed */
	mb();
}

static int ufs_qcom_phy_qrbtc_v2_init(struct phy *generic_phy)
{
	return 0;
}

struct phy_ops ufs_qcom_phy_qrbtc_v2_phy_ops = {
	.init		= ufs_qcom_phy_qrbtc_v2_init,
	.exit		= ufs_qcom_phy_exit,
	.owner		= THIS_MODULE,
};

struct ufs_qcom_phy_specific_ops phy_qrbtc_v2_ops = {
	.calibrate_phy		= ufs_qcom_phy_qrbtc_v2_phy_calibrate,
	.start_serdes		= ufs_qcom_phy_qrbtc_v2_start_serdes,
	.is_physical_coding_sublayer_ready =
				ufs_qcom_phy_qrbtc_v2_is_pcs_ready,
};

static int ufs_qcom_phy_qrbtc_v2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy *generic_phy;
	struct ufs_qcom_phy_qrbtc_v2 *phy;
	struct resource *res;
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		goto out;
	}

	generic_phy = ufs_qcom_phy_generic_probe(pdev, &phy->common_cfg,
		&ufs_qcom_phy_qrbtc_v2_phy_ops, &phy_qrbtc_v2_ops);

	if (!generic_phy) {
		dev_err(dev, "%s: ufs_qcom_phy_generic_probe() failed\n",
			__func__);
		err = -EIO;
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "u11_user");
	if (!res) {
		dev_err(dev, "%s: u11_user resource not found\n", __func__);
		err = -EINVAL;
		goto out;
	}

	phy->u11_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR_OR_NULL(phy->u11_regs)) {
		if (IS_ERR(phy->u11_regs)) {
			err = PTR_ERR(phy->u11_regs);
			phy->u11_regs = NULL;
			dev_err(dev, "%s: ioremap for phy_mem resource failed %d\n",
				__func__, err);
		} else {
			dev_err(dev, "%s: ioremap for phy_mem resource failed\n",
				__func__);
			err = -ENOMEM;
		}
		goto out;
	}

	phy_set_drvdata(generic_phy, phy);

	strlcpy(phy->common_cfg.name, UFS_PHY_NAME,
		sizeof(phy->common_cfg.name));

out:
	return err;
}

static int ufs_qcom_phy_qrbtc_v2_remove(struct platform_device *pdev)
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

static const struct of_device_id ufs_qcom_phy_qrbtc_v2_of_match[] = {
	{.compatible = "qcom,ufs-phy-qrbtc-v2"},
	{},
};
MODULE_DEVICE_TABLE(of, ufs_qcom_phy_qrbtc_v2_of_match);

static struct platform_driver ufs_qcom_phy_qrbtc_v2_driver = {
	.probe = ufs_qcom_phy_qrbtc_v2_probe,
	.remove = ufs_qcom_phy_qrbtc_v2_remove,
	.driver = {
		.of_match_table = ufs_qcom_phy_qrbtc_v2_of_match,
		.name = "ufs_qcom_phy_qrbtc_v2",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(ufs_qcom_phy_qrbtc_v2_driver);

MODULE_DESCRIPTION("Universal Flash Storage (UFS) QCOM PHY QRBTC V2");
MODULE_LICENSE("GPL v2");

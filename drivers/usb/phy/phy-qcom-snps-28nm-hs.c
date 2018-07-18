/* Copyright (c) 2009-2018, Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/usb/phy.h>
#include <linux/reset.h>

#include <linux/usb/msm_hsusb_hw.h>

#define MSM_USB_PHY_CSR_BASE			phy->phy_csr_regs
#define USB_HSPHY_3P3_VOL_MIN			3050000 /* uV */
#define USB_HSPHY_3P3_VOL_MAX			3300000 /* uV */
#define USB_HSPHY_3P3_HPM_LOAD			16000	/* uA */

#define USB_HSPHY_1P8_VOL_MIN			1800000 /* uV */
#define USB_HSPHY_1P8_VOL_MAX			1800000 /* uV */
#define USB_HSPHY_1P8_HPM_LOAD			19000	/* uA */

#define USB_HSPHY_MAX_REGULATORS		3

enum regulators {
	MSM_HSPHY_DVDD_REGULATOR,
	MSM_HSPHY_1P8_REGULATOR,
	MSM_HSPHY_3P3_REGULATOR,
	MSM_HSPHY_MAX_REGULATORS = USB_HSPHY_MAX_REGULATORS
};

struct msm_snps_hsphy {
	struct usb_phy		phy;
	void __iomem		*phy_csr_regs;

	struct clk		*phy_csr_clk;
	struct clk		*ref_clk;
	struct reset_control	*phy_reset;
	struct reset_control	*phy_por_reset;

	bool			dpdm_enable;
	struct regulator_dev	*dpdm_rdev;
	struct regulator_desc	dpdm_rdesc;

	int			*phy_init_seq;

	bool			suspended;
	bool			cable_connected;
	bool			clocks_enabled;

	int			voltage_levels[USB_HSPHY_MAX_REGULATORS][3];
	struct regulator_bulk_data	regulator[USB_HSPHY_MAX_REGULATORS];

	struct mutex		phy_lock;
};

static char *override_phy_init;
module_param(override_phy_init, charp, 0644);
MODULE_PARM_DESC(override_phy_init,
		"Override SNPS HS PHY Init Settings");

struct hsphy_reg_val {
	u32 offset;
	u32 val;
	u32 delay;
};

static void msm_snps_hsphy_disable_clocks(struct msm_snps_hsphy *phy)
{
	dev_dbg(phy->phy.dev, "%s: clocks_enabled:%d\n",
			__func__, phy->clocks_enabled);

	if (!phy->clocks_enabled)
		return;

	clk_disable_unprepare(phy->phy_csr_clk);
	clk_disable_unprepare(phy->ref_clk);

	phy->clocks_enabled = false;
}

static void msm_snps_hsphy_enable_clocks(struct msm_snps_hsphy *phy)
{
	dev_dbg(phy->phy.dev, "%s: clocks_enabled:%d\n",
			__func__, phy->clocks_enabled);

	if (phy->clocks_enabled)
		return;

	clk_prepare_enable(phy->ref_clk);
	clk_prepare_enable(phy->phy_csr_clk);

	phy->clocks_enabled = true;
}

static int msm_snps_hsphy_config_regulators(struct msm_snps_hsphy *phy,
								int high)
{
	int min, ret, i;

	min = high ? 1 : 0; /* low or none? */

	for (i = 0; i < USB_HSPHY_MAX_REGULATORS; i++) {
		ret = regulator_set_voltage(phy->regulator[i].consumer,
						phy->voltage_levels[i][min],
						phy->voltage_levels[i][2]);
		if (ret) {
			dev_err(phy->phy.dev, "%s: unable to set voltage for hsusb %s regulator\n",
					__func__, phy->regulator[i].supply);
			return ret;
		}
		dev_dbg(phy->phy.dev, "%s: min_vol:%d max_vol:%d\n",
						phy->regulator[i].supply,
						phy->voltage_levels[i][min],
						phy->voltage_levels[i][2]);
	}

	return 0;
}
static int msm_snps_hsphy_disable_regulators(struct msm_snps_hsphy *phy)
{
	int ret = 0;

	dev_dbg(phy->phy.dev, "%s turn off regulators\n", __func__);

	mutex_lock(&phy->phy_lock);
	ret = regulator_bulk_disable(USB_HSPHY_MAX_REGULATORS, phy->regulator);

	ret = regulator_set_load(
			phy->regulator[MSM_HSPHY_1P8_REGULATOR].consumer, 0);
	if (ret)
		dev_err(phy->phy.dev, "Unable to set (0) HPM for vdda18\n");

	ret = regulator_set_load(
			phy->regulator[MSM_HSPHY_3P3_REGULATOR].consumer, 0);
	if (ret)
		dev_err(phy->phy.dev, "Unable to set (0) HPM for vdda33\n");

	ret = msm_snps_hsphy_config_regulators(phy, 0);

	mutex_unlock(&phy->phy_lock);

	return ret;
}

static int msm_snps_hsphy_enable_regulators(struct msm_snps_hsphy *phy)
{
	int ret = 0;

	dev_dbg(phy->phy.dev, "%s turn on regulators.\n", __func__);

	mutex_lock(&phy->phy_lock);
	ret = msm_snps_hsphy_config_regulators(phy, 1);
	if (ret) {
		mutex_unlock(&phy->phy_lock);
		return ret;
	}

	ret = regulator_set_load(
			phy->regulator[MSM_HSPHY_1P8_REGULATOR].consumer,
			USB_HSPHY_1P8_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda18:%d\n", ret);
		goto unconfig_regulators;
	}

	ret = regulator_set_load(
			phy->regulator[MSM_HSPHY_3P3_REGULATOR].consumer,
			USB_HSPHY_3P3_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda33:%d\n", ret);
		goto unset_1p8_load;
	}

	ret = regulator_bulk_enable(USB_HSPHY_MAX_REGULATORS, phy->regulator);
	if (ret)
		goto unset_3p3_load;

	mutex_unlock(&phy->phy_lock);

	dev_dbg(phy->phy.dev, "%s(): HSUSB PHY's regulators are turned ON.\n",
								__func__);
	return 0;

unset_3p3_load:
	regulator_set_load(phy->regulator[MSM_HSPHY_3P3_REGULATOR].consumer, 0);
unset_1p8_load:
	regulator_set_load(phy->regulator[MSM_HSPHY_1P8_REGULATOR].consumer, 0);
unconfig_regulators:
	msm_snps_hsphy_config_regulators(phy, 0);
	mutex_unlock(&phy->phy_lock);

	return ret;
}

static void msm_snps_hsphy_enter_retention(struct msm_snps_hsphy *phy)
{
	u32 val;

	val = readb_relaxed(USB_PHY_CSR_PHY_CTRL_COMMON0);
	val |= SIDDQ;
	writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL_COMMON0);

	dev_dbg(phy->phy.dev, "PHY is in retention");
}

static void msm_snps_hsphy_exit_retention(struct msm_snps_hsphy *phy)
{
	u32 val;

	val = readb_relaxed(USB_PHY_CSR_PHY_CTRL_COMMON0);
	val &= ~SIDDQ;
	writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL_COMMON0);

	dev_dbg(phy->phy.dev, "PHY is out of retention");
}

static int msm_snps_phy_block_reset(struct msm_snps_hsphy *phy)
{
	int ret;

	msm_snps_hsphy_disable_clocks(phy);

	ret = reset_control_assert(phy->phy_reset);
	if (ret) {
		dev_err(phy->phy.dev, "phy_reset_clk assert failed %d\n",
									ret);
		return ret;
	}

	usleep_range(10, 15);

	ret = reset_control_deassert(phy->phy_reset);
	if (ret) {
		dev_err(phy->phy.dev, "phy_reset_clk deassert failed %d\n",
									ret);
		return ret;
	}

	usleep_range(80, 100);

	msm_snps_hsphy_enable_clocks(phy);

	return 0;
}

static void msm_snps_hsphy_por(struct msm_snps_hsphy *phy)
{
	struct hsphy_reg_val *reg = NULL;
	u32 aseq[20];
	u32 *seq, tmp;

	if (override_phy_init) {
		dev_dbg(phy->phy.dev, "Override HS PHY Init:%s\n",
							override_phy_init);
		get_options(override_phy_init, ARRAY_SIZE(aseq), aseq);
		seq = &aseq[1];
	} else {
		seq = phy->phy_init_seq;
	}

	reg = (struct hsphy_reg_val *)seq;
	if (!reg)
		return;

	while (reg->offset != -1) {
		writeb_relaxed(reg->val,
				phy->phy_csr_regs + reg->offset);

		tmp = readb_relaxed(phy->phy_csr_regs + reg->offset);
		if (tmp != reg->val)
			dev_err(phy->phy.dev, "write:%x to: %x failed\n",
						reg->val, reg->offset);
		if (reg->delay)
			usleep_range(reg->delay, reg->delay + 10);
		reg++;
	}

	/* Ensure that the above parameter overrides is successful. */
	mb();
}

static int msm_snps_hsphy_reset(struct msm_snps_hsphy *phy)
{
	int ret;

	ret = reset_control_assert(phy->phy_por_reset);
	if (ret) {
		dev_err(phy->phy.dev, "phy_por_clk assert failed %d\n", ret);
		return ret;
	}
	/*
	 * The Femto PHY is POR reset in the following scenarios.
	 *
	 * 1. After overriding the parameter registers.
	 * 2. Low power mode exit from PHY retention.
	 *
	 * Ensure that SIDDQ is cleared before bringing the PHY
	 * out of reset.
	 *
	 */
	msm_snps_hsphy_exit_retention(phy);

	/*
	 * As per databook, 10 usec delay is required between
	 * PHY POR assert and de-assert.
	 */
	usleep_range(10, 20);
	ret = reset_control_deassert(phy->phy_por_reset);
	if (ret) {
		pr_err("phy_por_clk de-assert failed %d\n", ret);
		return ret;
	}
	/*
	 * As per databook, it takes 75 usec for PHY to stabilize
	 * after the reset.
	 */
	usleep_range(80, 100);

	/* Ensure that RESET operation is completed. */
	mb();

	return 0;
}

static int msm_snps_hsphy_init(struct usb_phy *uphy)
{
	struct msm_snps_hsphy *phy =
				container_of(uphy, struct msm_snps_hsphy, phy);
	int ret;

	dev_dbg(phy->phy.dev, "%s: Initialize HS PHY\n", __func__);
	ret = msm_snps_hsphy_enable_regulators(phy);
	if (ret)
		return ret;

	ret = msm_snps_phy_block_reset(phy);
	if (ret)
		return ret;

	msm_snps_hsphy_por(phy);

	ret = msm_snps_hsphy_reset(phy);

	return ret;
}

static void msm_snps_hsphy_enable_hv_interrupts(struct msm_snps_hsphy *phy)
{
	u32 val;

	dev_dbg(phy->phy.dev, "%s\n", __func__);
	val = readl_relaxed(USB_PHY_CSR_PHY_CTRL3);
	val |= CLAMP_MPM_DPSE_DMSE_EN_N;
	writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL3);
}

static void msm_snps_hsphy_disable_hv_interrupts(struct msm_snps_hsphy *phy)
{
	u32 val;

	dev_dbg(phy->phy.dev, "%s\n", __func__);
	val = readl_relaxed(USB_PHY_CSR_PHY_CTRL3);
	val &= ~CLAMP_MPM_DPSE_DMSE_EN_N;
	writeb_relaxed(val, USB_PHY_CSR_PHY_CTRL3);
}

static int msm_snps_hsphy_set_suspend(struct usb_phy *uphy, int suspend)
{
	struct msm_snps_hsphy *phy = container_of(uphy,
					struct msm_snps_hsphy, phy);

	dev_dbg(phy->phy.dev, "%s: suspend:%d with phy->suspended:%d\n",
					__func__, suspend, phy->suspended);
	if (phy->suspended == suspend) {
		dev_info(phy->phy.dev, "PHY is already suspended\n");
		return 0;
	}

	if (suspend) {
		if (phy->cable_connected) {
			msm_snps_hsphy_enable_hv_interrupts(phy);
			msm_snps_hsphy_disable_clocks(phy);
		} else {
			msm_snps_hsphy_enter_retention(phy);
			msm_snps_hsphy_disable_clocks(phy);
			msm_snps_hsphy_disable_regulators(phy);
		}

		phy->suspended = true;
	} else {
		if (phy->cable_connected) {
			msm_snps_hsphy_enable_clocks(phy);
			msm_snps_hsphy_disable_hv_interrupts(phy);
		} else {
			msm_snps_hsphy_enable_regulators(phy);
			msm_snps_hsphy_enable_clocks(phy);
			msm_snps_hsphy_exit_retention(phy);
		}

		phy->suspended = false;
	}

	return 0;
}

static int msm_snps_dpdm_regulator_enable(struct regulator_dev *rdev)
{
	int ret = 0;
	struct msm_snps_hsphy *phy = rdev_get_drvdata(rdev);

	if (phy->dpdm_enable) {
		dev_dbg(phy->phy.dev, "%s: DP DM regulator already enabled\n",
								__func__);
		return 0;
	}

	msm_snps_hsphy_enable_regulators(phy);
	phy->dpdm_enable = true;

	return ret;
}

static int msm_snps_dpdm_regulator_disable(struct regulator_dev *rdev)
{
	int ret = 0;
	struct msm_snps_hsphy *phy = rdev_get_drvdata(rdev);

	if (!phy->dpdm_enable) {
		dev_dbg(phy->phy.dev, "%s: DP DM regulator already enabled\n",
								__func__);
		return 0;
	}

	msm_snps_hsphy_disable_regulators(phy);
	phy->dpdm_enable = false;

	return ret;
}

static int msm_snps_dpdm_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct msm_snps_hsphy *phy = rdev_get_drvdata(rdev);

	return phy->dpdm_enable;
}

static struct regulator_ops msm_snps_dpdm_regulator_ops = {
	.enable		= msm_snps_dpdm_regulator_enable,
	.disable	= msm_snps_dpdm_regulator_disable,
	.is_enabled	= msm_snps_dpdm_regulator_is_enabled,
};

static int msm_snps_dpdm_regulator_register(struct msm_snps_hsphy *phy)
{
	struct device *dev = phy->phy.dev;
	struct regulator_config cfg = {};
	struct regulator_init_data *init_data;

	init_data = devm_kzalloc(dev, sizeof(*init_data), GFP_KERNEL);
	if (!init_data)
		return -ENOMEM;

	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	phy->dpdm_rdesc.owner = THIS_MODULE;
	phy->dpdm_rdesc.type = REGULATOR_VOLTAGE;
	phy->dpdm_rdesc.ops = &msm_snps_dpdm_regulator_ops;
	phy->dpdm_rdesc.name = kbasename(dev->of_node->full_name);

	cfg.dev = dev;
	cfg.init_data = init_data;
	cfg.driver_data = phy;
	cfg.of_node = dev->of_node;

	phy->dpdm_rdev = devm_regulator_register(dev, &phy->dpdm_rdesc, &cfg);
	if (IS_ERR(phy->dpdm_rdev))
		return PTR_ERR(phy->dpdm_rdev);

	return 0;
}

static int msm_snps_hsphy_notify_connect(struct usb_phy *uphy,
					enum usb_device_speed speed)
{
	struct msm_snps_hsphy *phy = container_of(uphy,
					struct msm_snps_hsphy, phy);

	phy->cable_connected = true;

	dev_dbg(phy->phy.dev, "PHY: connect notification cable_connected=%d\n",
							phy->cable_connected);
	return 0;
}

static int msm_snps_hsphy_notify_disconnect(struct usb_phy *uphy,
					enum usb_device_speed speed)
{
	struct msm_snps_hsphy *phy = container_of(uphy,
					struct msm_snps_hsphy, phy);

	phy->cable_connected = false;

	dev_dbg(phy->phy.dev, "PHY: connect notification cable_connected=%d\n",
							phy->cable_connected);
	return 0;
}

static int msm_snps_hsphy_probe(struct platform_device *pdev)
{
	struct msm_snps_hsphy	*phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;
	int len = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	dev_dbg(dev, "%s: probe\n", __func__);
	phy->phy_reset = devm_reset_control_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset)) {
		dev_err(dev, "%s failed to get phy_reset %d\n",
						__func__, ret);
		return PTR_ERR(phy->phy_reset);
	}

	phy->phy_por_reset = devm_reset_control_get(dev, "phy_por_reset");
	if (IS_ERR(phy->phy_por_reset)) {
		dev_err(dev, "%s failed to get phy_por_reset %d\n",
						__func__, ret);
		return PTR_ERR(phy->phy_por_reset);
	}

	phy->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(phy->ref_clk)) {
		dev_err(dev, "%s failed to get ref_clk %d\n",
						__func__, ret);
		return PTR_ERR(phy->ref_clk);
	}

	phy->phy_csr_clk = devm_clk_get(dev, "phy_csr_clk");
	if (IS_ERR(phy->phy_csr_clk)) {
		dev_err(dev, "%s failed to get phy_csr_clk %d\n",
						__func__, ret);
		return PTR_ERR(phy->phy_csr_clk);
	}

	res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "phy_csr");

	phy->phy_csr_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->phy_csr_regs)) {
		dev_err(dev, "%s: PHY CSR ioremap failed!\n", __func__);
		return PTR_ERR(phy->phy_csr_regs);
	}

	of_get_property(dev->of_node, "qcom,snps-hs-phy-init-seq", &len);
	if (len) {
		phy->phy_init_seq = devm_kzalloc(dev, len, GFP_KERNEL);
		if (!phy->phy_init_seq)
			return -ENOMEM;
		of_property_read_u32_array(dev->of_node,
				"qcom,snps-hs-phy-init-seq", phy->phy_init_seq,
				(len/sizeof(*phy->phy_init_seq)));
	}

	phy->regulator[MSM_HSPHY_DVDD_REGULATOR].supply = "vdd";
	phy->regulator[MSM_HSPHY_1P8_REGULATOR].supply = "vdda18";
	phy->regulator[MSM_HSPHY_3P3_REGULATOR].supply = "vdda33";

	ret = devm_regulator_bulk_get(dev, USB_HSPHY_MAX_REGULATORS,
							phy->regulator);
	if (ret)
		return ret;

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
		(u32 *) phy->voltage_levels[MSM_HSPHY_DVDD_REGULATOR],
		ARRAY_SIZE(phy->voltage_levels[MSM_HSPHY_DVDD_REGULATOR]));
	if (ret) {
		dev_err(dev, "%s: error reading qcom,vdd-voltage-level property\n",
				__func__);
		return ret;
	}

	phy->voltage_levels[MSM_HSPHY_1P8_REGULATOR][0] = 0;
	phy->voltage_levels[MSM_HSPHY_1P8_REGULATOR][1] = USB_HSPHY_1P8_VOL_MIN;
	phy->voltage_levels[MSM_HSPHY_1P8_REGULATOR][2] = USB_HSPHY_1P8_VOL_MAX;

	phy->voltage_levels[MSM_HSPHY_3P3_REGULATOR][0] = 0;
	phy->voltage_levels[MSM_HSPHY_3P3_REGULATOR][1] = USB_HSPHY_3P3_VOL_MIN;
	phy->voltage_levels[MSM_HSPHY_3P3_REGULATOR][2] = USB_HSPHY_3P3_VOL_MAX;

	platform_set_drvdata(pdev, phy);

	phy->phy.dev			= dev;
	phy->phy.init			= msm_snps_hsphy_init;
	phy->phy.set_suspend		= msm_snps_hsphy_set_suspend;
	phy->phy.notify_connect		= msm_snps_hsphy_notify_connect;
	phy->phy.notify_disconnect	= msm_snps_hsphy_notify_disconnect;

	mutex_init(&phy->phy_lock);
	ret = msm_snps_dpdm_regulator_register(phy);
	if (ret)
		return ret;

	ret = usb_add_phy_dev(&phy->phy);

	return ret;
}

static int msm_snps_hsphy_remove(struct platform_device *pdev)
{
	struct msm_snps_hsphy *phy = platform_get_drvdata(pdev);

	usb_remove_phy(&phy->phy);

	msm_snps_hsphy_disable_clocks(phy);
	msm_snps_hsphy_disable_regulators(phy);

	return 0;
}

static const struct of_device_id msm_usb_hsphy_match[] = {
	{
		.compatible	= "qcom,usb-snps-hsphy",
	},
	{ },
};

static struct platform_driver msm_snps_hsphy_driver = {
	.probe		= msm_snps_hsphy_probe,
	.remove		= msm_snps_hsphy_remove,
	.driver	= {
		.name	= "msm-usb-snps-hsphy",
		.of_match_table = msm_usb_hsphy_match,
	},
};

module_platform_driver(msm_snps_hsphy_driver);

MODULE_DESCRIPTION("MSM USB SNPS HS PHY driver");
MODULE_LICENSE("GPL v2");

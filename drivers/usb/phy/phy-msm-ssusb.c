/*
 * Copyright (c) 2012-2014,2017 The Linux Foundation. All rights reserved.
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
#include <linux/usb/phy.h>
#include <linux/reset.h>

/* SSPHY control registers */
#define SS_PHY_CTRL0			0x6C
#define SS_PHY_CTRL1			0x70
#define SS_PHY_CTRL2			0x74
#define SS_PHY_CTRL4			0x7C

#define PHY_HOST_MODE			BIT(2)
#define PHY_VBUS_VALID_OVERRIDE		BIT(4)

/* SS_PHY_CTRL_REG bits */
#define REF_SS_PHY_EN			BIT(0)
#define LANE0_PWR_PRESENT		BIT(2)
#define SWI_PCS_CLK_SEL			BIT(4)
#define TEST_POWERDOWN			BIT(4)
#define REF_USE_PAD			BIT(6)
#define SS_PHY_RESET			BIT(7)

#define USB_SSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_SSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_SSPHY_1P8_HPM_LOAD		23000	/* uA */

struct msm_ssphy {
	struct usb_phy		phy;
	void __iomem		*base;

	struct clk		*ref_clk;
	struct clk		*cfg_ahb_clk;
	struct clk		*pipe_clk;
	bool			clocks_enabled;

	struct reset_control	*phy_com_reset;
	struct reset_control	*phy_reset;
	struct regulator	*vdd;
	struct regulator	*vdda18;
	bool			suspended;
	int			vdd_levels[3]; /* none, low, high */

	int			power_enabled;
};

static void msm_ssusb_enable_clocks(struct msm_ssphy *phy)
{
	dev_dbg(phy->phy.dev, "%s: clocks_enabled:%d\n",
			__func__, phy->clocks_enabled);

	if (phy->clocks_enabled)
		return;

	clk_prepare_enable(phy->cfg_ahb_clk);
	clk_prepare_enable(phy->ref_clk);
	clk_prepare_enable(phy->pipe_clk);

	phy->clocks_enabled = true;
}

static void msm_ssusb_disable_clocks(struct msm_ssphy *phy)
{
	dev_dbg(phy->phy.dev, "%s: clocks_enabled:%d\n",
			__func__, phy->clocks_enabled);

	if (!phy->clocks_enabled)
		return;

	clk_disable_unprepare(phy->pipe_clk);
	clk_disable_unprepare(phy->ref_clk);
	clk_disable_unprepare(phy->cfg_ahb_clk);

	phy->clocks_enabled = false;
}

static int msm_ssusb_config_vdd(struct msm_ssphy *phy, int high)
{
	int min, ret;

	min = high ? 1 : 0; /* low or none? */
	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[min],
				    phy->vdd_levels[2]);
	if (ret) {
		dev_err(phy->phy.dev, "unable to set voltage for ssusb vdd\n");
		return ret;
	}

	dev_dbg(phy->phy.dev, "%s: min_vol:%d max_vol:%d\n", __func__,
		phy->vdd_levels[min], phy->vdd_levels[2]);
	return ret;
}

static int msm_ssusb_ldo_enable(struct msm_ssphy *phy, int on)
{
	int rc = 0;

	dev_dbg(phy->phy.dev, "reg (%s)\n", on ? "HPM" : "LPM");

	if (phy->power_enabled == on) {
		dev_dbg(phy->phy.dev, "LDOs are already %s\n",
							on ? "ON" : "OFF");
		return 0;
	}

	if (!on)
		goto disable_regulators;

	rc = regulator_set_load(phy->vdda18, USB_SSPHY_1P8_HPM_LOAD);
	if (rc < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda18: %d\n", rc);
		return rc;
	}

	rc = regulator_set_voltage(phy->vdda18, USB_SSPHY_1P8_VOL_MIN,
						USB_SSPHY_1P8_VOL_MAX);
	if (rc) {
		dev_err(phy->phy.dev, "unable to set voltage for vdda18: %d\n",
									rc);
		goto put_vdda18_lpm;
	}

	rc = regulator_enable(phy->vdda18);
	if (rc) {
		dev_err(phy->phy.dev, "Unable to enable vdda18: %d\n", rc);
		goto unset_vdda18;
	}

	phy->power_enabled = 1;

	return 0;

disable_regulators:
	rc = regulator_disable(phy->vdda18);
	if (rc)
		dev_err(phy->phy.dev, "Unable to disable vdda18: %d\n", rc);

unset_vdda18:
	rc = regulator_set_voltage(phy->vdda18, 0, USB_SSPHY_1P8_VOL_MAX);
	if (rc)
		dev_err(phy->phy.dev, "unable to set min voltage for vdda18: %d\n",
									rc);

put_vdda18_lpm:
	rc = regulator_set_load(phy->vdda18, 0);
	if (rc < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdda18: %d\n", rc);


	phy->power_enabled = 0;

	return rc;
}

static void msm_usb_write_readback(void *base, u32 offset,
					const u32 mask, u32 val)
{
	u32 write_val, tmp = readl_relaxed(base + offset);

	tmp &= ~mask;		/* retain other bits */
	write_val = tmp | val;

	writel_relaxed(write_val, base + offset);

	/* Read back to see if val was written */
	tmp = readl_relaxed(base + offset);
	tmp &= mask;		/* clear other bits */

	if (tmp != val)
		pr_err("%s: write: %x to QSCRATCH: %x FAILED\n",
			__func__, val, offset);
}

/* SSPHY Initialization */
static int msm_ssphy_init(struct usb_phy *uphy)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	int rc;

	rc = msm_ssusb_config_vdd(phy, 1);
	if (rc) {
		dev_err(phy->phy.dev, "Unable to config vdd: %d\n", rc);
		return rc;
	}

	msm_ssusb_ldo_enable(phy, 1);

	msm_ssusb_enable_clocks(phy);

	/* Use clk reset, if available; otherwise use SS_PHY_RESET bit */
	if (phy->phy_com_reset) {
		reset_control_assert(phy->phy_com_reset);
		reset_control_assert(phy->phy_reset);
		udelay(10);
		reset_control_deassert(phy->phy_com_reset);
		reset_control_deassert(phy->phy_reset);
	} else {
		msm_usb_write_readback(phy->base, SS_PHY_CTRL1,
						SS_PHY_RESET, SS_PHY_RESET);
		udelay(10); /* 10us required before de-asserting */
		msm_usb_write_readback(phy->base, SS_PHY_CTRL1,
						SS_PHY_RESET, 0);
	}

	writeb_relaxed(SWI_PCS_CLK_SEL, phy->base + SS_PHY_CTRL0);

	msm_usb_write_readback(phy->base, SS_PHY_CTRL4,
					LANE0_PWR_PRESENT, LANE0_PWR_PRESENT);

	writeb_relaxed(REF_SS_PHY_EN, phy->base + SS_PHY_CTRL2);

	return 0;
}

static int msm_ssphy_set_suspend(struct usb_phy *uphy, int suspend)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	int rc;

	dev_dbg(uphy->dev, "%s: phy->suspended:%d suspend:%d", __func__,
					phy->suspended, suspend);

	if (phy->suspended == suspend) {
		dev_dbg(uphy->dev, "PHY is already %s\n",
					suspend ? "suspended" : "resumed");
		return 0;
	}

	if (suspend) {

		msm_usb_write_readback(phy->base, SS_PHY_CTRL2,
					REF_SS_PHY_EN, 0);
		msm_usb_write_readback(phy->base, SS_PHY_CTRL4,
					TEST_POWERDOWN, TEST_POWERDOWN);

		msm_ssusb_disable_clocks(phy);
		msm_ssusb_ldo_enable(phy, 0);
		msm_ssusb_config_vdd(phy, 0);
		phy->suspended = true;
	} else {

		rc = msm_ssusb_config_vdd(phy, 1);
		if (rc)
			return rc;
		msm_ssusb_ldo_enable(phy, 1);
		msm_ssusb_enable_clocks(phy);

		msm_usb_write_readback(phy->base, SS_PHY_CTRL2,
					REF_SS_PHY_EN, REF_SS_PHY_EN);
		msm_usb_write_readback(phy->base, SS_PHY_CTRL4,
					TEST_POWERDOWN, 0);

		phy->suspended = false;
	}

	return 0;
}

static int msm_ssphy_notify_connect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);

	if (uphy->flags & PHY_HOST_MODE)
		return 0;

	if (uphy->flags & PHY_VBUS_VALID_OVERRIDE)
		/* Indicate power present to SS phy */
		msm_usb_write_readback(phy->base, SS_PHY_CTRL4,
					LANE0_PWR_PRESENT, LANE0_PWR_PRESENT);

	return 0;
}

static int msm_ssphy_notify_disconnect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);

	if (uphy->flags & PHY_HOST_MODE)
		return 0;

	if (uphy->flags & PHY_VBUS_VALID_OVERRIDE)
		/* Clear power indication to SS phy */
		msm_usb_write_readback(phy->base, SS_PHY_CTRL4,
					LANE0_PWR_PRESENT, 0);

	return 0;
}

static int msm_ssphy_probe(struct platform_device *pdev)
{
	struct msm_ssphy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory base resource\n");
		return -ENODEV;
	}

	phy->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!phy->base) {
		dev_err(dev, "ioremap failed\n");
		return -ENODEV;
	}

	phy->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(phy->ref_clk)) {
		dev_err(dev, "unable to get ref_clk\n");
		return PTR_ERR(phy->ref_clk);
	}

	phy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb_clk");
	if (IS_ERR(phy->cfg_ahb_clk)) {
		dev_err(dev, "unable to get cfg_ahb_clk\n");
		return PTR_ERR(phy->cfg_ahb_clk);
	}

	phy->pipe_clk = devm_clk_get(dev, "pipe_clk");
	if (IS_ERR(phy->pipe_clk)) {
		dev_err(dev, "unable to get pipe_clk\n");
		return PTR_ERR(phy->pipe_clk);
	}

	phy->phy_com_reset = devm_reset_control_get(dev, "phy_com_reset");
	if (IS_ERR(phy->phy_com_reset)) {
		ret = PTR_ERR(phy->phy_com_reset);
		dev_dbg(dev, "failed to get phy_com_reset\n");
		phy->phy_com_reset = NULL;
	}

	phy->phy_reset = devm_reset_control_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset)) {
		ret = PTR_ERR(phy->phy_reset);
		dev_dbg(dev, "failed to get phy_reset\n");
		phy->phy_reset = NULL;
	}

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		return ret;
	}

	phy->phy.dev = dev;
	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		return PTR_ERR(phy->vdd);
	}

	phy->vdda18 = devm_regulator_get(dev, "vdda18");
	if (IS_ERR(phy->vdda18)) {
		dev_err(dev, "unable to get vdda18 supply\n");
		return PTR_ERR(phy->vdda18);
	}

	ret = msm_ssusb_config_vdd(phy, 1);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to config vdd: %d\n", ret);
		return ret;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable vdd: %d\n", ret);
		goto unconfig_vdd;
	}

	platform_set_drvdata(pdev, phy);

	if (of_property_read_bool(dev->of_node, "qcom,vbus-valid-override"))
		phy->phy.flags |= PHY_VBUS_VALID_OVERRIDE;

	phy->phy.init			= msm_ssphy_init;
	phy->phy.set_suspend		= msm_ssphy_set_suspend;
	phy->phy.notify_connect		= msm_ssphy_notify_connect;
	phy->phy.notify_disconnect	= msm_ssphy_notify_disconnect;
	phy->phy.type			= USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		goto disable_vdd;

	return 0;

disable_vdd:
	regulator_disable(phy->vdd);
unconfig_vdd:
	msm_ssusb_config_vdd(phy, 0);

	return ret;
}

static int msm_ssphy_remove(struct platform_device *pdev)
{
	struct msm_ssphy *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	msm_ssphy_set_suspend(&phy->phy, 0);
	usb_remove_phy(&phy->phy);
	msm_ssphy_set_suspend(&phy->phy, 1);
	regulator_disable(phy->vdd);

	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-ssphy",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_ssphy_driver = {
	.probe		= msm_ssphy_probe,
	.remove		= msm_ssphy_remove,
	.driver = {
		.name	= "msm-usb-ssphy",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_ssphy_driver);

MODULE_DESCRIPTION("MSM USB SS PHY driver");
MODULE_LICENSE("GPL v2");

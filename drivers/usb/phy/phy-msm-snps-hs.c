/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/usb/phy.h>
#include <linux/reset.h>

#define USB2_PHY_USB_PHY_UTMI_CTRL0		(0x3c)
#define SLEEPM					BIT(0)

#define USB2_PHY_USB_PHY_UTMI_CTRL5		(0x50)
#define ATERESET				BIT(0)
#define POR					BIT(1)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0	(0x54)
#define VATESTENB_MASK				(0x3 << 0)
#define RETENABLEN				BIT(3)
#define FSEL_MASK				(0x7 << 4)
#define FSEL_DEFAULT				(0x3 << 4)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1	(0x58)
#define VBUSVLDEXTSEL0				BIT(4)
#define PLLBTUNE				BIT(5)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2	(0x5c)
#define VREGBYPASS				BIT(0)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL1		(0x60)
#define VBUSVLDEXT0				BIT(0)

#define USB2_PHY_USB_PHY_HS_PHY_CTRL2		(0x64)
#define USB2_SUSPEND_N				BIT(2)
#define USB2_SUSPEND_N_SEL			BIT(3)

#define USB2_PHY_USB_PHY_HS_PHY_TEST0		(0x80)
#define TESTDATAIN_MASK				(0xff << 0)

#define USB2_PHY_USB_PHY_HS_PHY_TEST1		(0x84)
#define TESTDATAOUTSEL				BIT(4)
#define TOGGLE_2WR				BIT(6)

#define USB2_PHY_USB_PHY_CFG0			(0x94)
#define UTMI_PHY_CMN_CTRL_OVERRIDE_EN		BIT(1)

#define USB2_PHY_USB_PHY_REFCLK_CTRL		(0xa0)
#define REFCLK_SEL_MASK				(0x3 << 0)
#define REFCLK_SEL_DEFAULT			(0x2 << 0)

#define USB_HSPHY_3P3_VOL_MIN			3050000 /* uV */
#define USB_HSPHY_3P3_VOL_MAX			3300000 /* uV */
#define USB_HSPHY_3P3_HPM_LOAD			16000	/* uA */
#define USB_HSPHY_3P3_VOL_FSHOST		3150000 /* uV */

#define USB_HSPHY_1P8_VOL_MIN			1704000 /* uV */
#define USB_HSPHY_1P8_VOL_MAX			1800000 /* uV */
#define USB_HSPHY_1P8_HPM_LOAD			19000	/* uA */

struct msm_hsphy {
	struct usb_phy		phy;
	void __iomem		*base;

	struct clk		*ref_clk_src;
	struct clk		*cfg_ahb_clk;
	struct reset_control	*phy_reset;

	struct regulator	*vdd;
	struct regulator	*vdda33;
	struct regulator	*vdda18;
	int			vdd_levels[3]; /* none, low, high */

	bool			clocks_enabled;
	bool			power_enabled;
	bool			suspended;
	bool			cable_connected;

	/* emulation targets specific */
	void __iomem		*emu_phy_base;
	int			*emu_init_seq;
	int			emu_init_seq_len;
	int			*emu_dcm_reset_seq;
	int			emu_dcm_reset_seq_len;
};

static void msm_hsphy_enable_clocks(struct msm_hsphy *phy, bool on)
{
	dev_dbg(phy->phy.dev, "%s(): clocks_enabled:%d on:%d\n",
			__func__, phy->clocks_enabled, on);

	if (!phy->clocks_enabled && on) {
		clk_prepare_enable(phy->ref_clk_src);

		if (phy->cfg_ahb_clk)
			clk_prepare_enable(phy->cfg_ahb_clk);

		phy->clocks_enabled = true;
	}

	if (phy->clocks_enabled && !on) {
		if (phy->cfg_ahb_clk)
			clk_disable_unprepare(phy->cfg_ahb_clk);

		clk_disable_unprepare(phy->ref_clk_src);
		phy->clocks_enabled = false;
	}

}
static int msm_hsphy_config_vdd(struct msm_hsphy *phy, int high)
{
	int min, ret;

	min = high ? 1 : 0; /* low or none? */
	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[min],
				    phy->vdd_levels[2]);
	if (ret) {
		dev_err(phy->phy.dev, "unable to set voltage for hsusb vdd\n");
		return ret;
	}

	dev_dbg(phy->phy.dev, "%s: min_vol:%d max_vol:%d\n", __func__,
		phy->vdd_levels[min], phy->vdd_levels[2]);

	return ret;
}

static int msm_hsphy_enable_power(struct msm_hsphy *phy, bool on)
{
	int ret = 0;

	dev_dbg(phy->phy.dev, "%s turn %s regulators. power_enabled:%d\n",
			__func__, on ? "on" : "off", phy->power_enabled);

	if (phy->power_enabled == on) {
		dev_dbg(phy->phy.dev, "PHYs' regulators are already ON.\n");
		return 0;
	}

	if (!on)
		goto disable_vdda33;

	ret = msm_hsphy_config_vdd(phy, true);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to config VDD:%d\n",
							ret);
		goto err_vdd;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable VDD\n");
		goto unconfig_vdd;
	}

	ret = regulator_set_load(phy->vdda18, USB_HSPHY_1P8_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda18:%d\n", ret);
		goto disable_vdd;
	}

	ret = regulator_set_voltage(phy->vdda18, USB_HSPHY_1P8_VOL_MIN,
						USB_HSPHY_1P8_VOL_MAX);
	if (ret) {
		dev_err(phy->phy.dev,
				"Unable to set voltage for vdda18:%d\n", ret);
		goto put_vdda18_lpm;
	}

	ret = regulator_enable(phy->vdda18);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable vdda18:%d\n", ret);
		goto unset_vdda18;
	}

	ret = regulator_set_load(phy->vdda33, USB_HSPHY_3P3_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda33:%d\n", ret);
		goto disable_vdda18;
	}

	ret = regulator_set_voltage(phy->vdda33, USB_HSPHY_3P3_VOL_MIN,
						USB_HSPHY_3P3_VOL_MAX);
	if (ret) {
		dev_err(phy->phy.dev,
				"Unable to set voltage for vdda33:%d\n", ret);
		goto put_vdda33_lpm;
	}

	ret = regulator_enable(phy->vdda33);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable vdda33:%d\n", ret);
		goto unset_vdd33;
	}

	phy->power_enabled = true;

	pr_debug("%s(): HSUSB PHY's regulators are turned ON.\n", __func__);
	return ret;

disable_vdda33:
	ret = regulator_disable(phy->vdda33);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdda33:%d\n", ret);

unset_vdd33:
	ret = regulator_set_voltage(phy->vdda33, 0, USB_HSPHY_3P3_VOL_MAX);
	if (ret)
		dev_err(phy->phy.dev,
			"Unable to set (0) voltage for vdda33:%d\n", ret);

put_vdda33_lpm:
	ret = regulator_set_load(phy->vdda33, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set (0) HPM of vdda33\n");

disable_vdda18:
	ret = regulator_disable(phy->vdda18);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdda18:%d\n", ret);

unset_vdda18:
	ret = regulator_set_voltage(phy->vdda18, 0, USB_HSPHY_1P8_VOL_MAX);
	if (ret)
		dev_err(phy->phy.dev,
			"Unable to set (0) voltage for vdda18:%d\n", ret);

put_vdda18_lpm:
	ret = regulator_set_load(phy->vdda18, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdda18\n");

disable_vdd:
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdd:%d\n",
								ret);

unconfig_vdd:
	ret = msm_hsphy_config_vdd(phy, false);
	if (ret)
		dev_err(phy->phy.dev, "Unable unconfig VDD:%d\n",
								ret);
err_vdd:
	phy->power_enabled = false;
	dev_dbg(phy->phy.dev, "HSUSB PHY's regulators are turned OFF.\n");
	return ret;
}

static void msm_usb_write_readback(void __iomem *base, u32 offset,
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

static void msm_hsphy_reset(struct msm_hsphy *phy)
{
	int ret;

	ret = reset_control_assert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "%s: phy_reset assert failed\n",
								__func__);
	usleep_range(100, 150);

	ret = reset_control_deassert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "%s: phy_reset deassert failed\n",
							__func__);
}

static void hsusb_phy_write_seq(void __iomem *base, u32 *seq, int cnt,
		unsigned long delay)
{
	int i;

	pr_debug("Seq count:%d\n", cnt);
	for (i = 0; i < cnt; i = i+2) {
		pr_debug("write 0x%02x to 0x%02x\n", seq[i], seq[i+1]);
		writel_relaxed(seq[i], base + seq[i+1]);
		if (delay)
			usleep_range(delay, (delay + 2000));
	}
}

static int msm_hsphy_emu_init(struct usb_phy *uphy)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);
	int ret;

	dev_dbg(uphy->dev, "%s\n", __func__);

	ret = msm_hsphy_enable_power(phy, true);
	if (ret)
		return ret;

	msm_hsphy_enable_clocks(phy, true);
	msm_hsphy_reset(phy);

	if (phy->emu_init_seq) {
		hsusb_phy_write_seq(phy->base,
			phy->emu_init_seq,
			phy->emu_init_seq_len, 10000);

		/* Wait for 5ms as per QUSB2 RUMI sequence */
		usleep_range(5000, 7000);

		if (phy->emu_dcm_reset_seq)
			hsusb_phy_write_seq(phy->emu_phy_base,
					phy->emu_dcm_reset_seq,
					phy->emu_dcm_reset_seq_len, 10000);
	}

	return 0;
}

static int msm_hsphy_init(struct usb_phy *uphy)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);
	int ret;

	dev_dbg(uphy->dev, "%s\n", __func__);

	ret = msm_hsphy_enable_power(phy, true);
	if (ret)
		return ret;

	msm_hsphy_enable_clocks(phy, true);
	msm_hsphy_reset(phy);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
	UTMI_PHY_CMN_CTRL_OVERRIDE_EN, UTMI_PHY_CMN_CTRL_OVERRIDE_EN);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL5,
				POR, POR);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0,
				FSEL_MASK, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1,
				PLLBTUNE, PLLBTUNE);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_REFCLK_CTRL,
				REFCLK_SEL_MASK, REFCLK_SEL_DEFAULT);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1,
				VBUSVLDEXTSEL0, VBUSVLDEXTSEL0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL1,
				VBUSVLDEXT0, VBUSVLDEXT0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2,
				VREGBYPASS, VREGBYPASS);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL5,
				ATERESET, ATERESET);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_TEST1,
				TESTDATAOUTSEL, TESTDATAOUTSEL);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_TEST1,
				TOGGLE_2WR, TOGGLE_2WR);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0,
				VATESTENB_MASK, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_TEST0,
				TESTDATAIN_MASK, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL2,
				USB2_SUSPEND_N_SEL, USB2_SUSPEND_N_SEL);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL2,
				USB2_SUSPEND_N, USB2_SUSPEND_N);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL0,
				SLEEPM, SLEEPM);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_UTMI_CTRL5,
				POR, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_HS_PHY_CTRL2,
				USB2_SUSPEND_N_SEL, 0);

	msm_usb_write_readback(phy->base, USB2_PHY_USB_PHY_CFG0,
	UTMI_PHY_CMN_CTRL_OVERRIDE_EN, 0);

	return 0;
}

static int msm_hsphy_set_suspend(struct usb_phy *uphy, int suspend)
{
	return 0;
}

static int msm_hsphy_notify_connect(struct usb_phy *uphy,
				    enum usb_device_speed speed)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	phy->cable_connected = true;

	return 0;
}

static int msm_hsphy_notify_disconnect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	phy->cable_connected = false;

	return 0;
}

static int msm_hsphy_probe(struct platform_device *pdev)
{
	struct msm_hsphy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0, size = 0;


	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		ret = -ENOMEM;
		goto err_ret;
	}

	phy->phy.dev = dev;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"hsusb_phy_base");
	if (!res) {
		dev_err(dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto err_ret;
	}

	phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->base)) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENODEV;
		goto err_ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"emu_phy_base");
	if (res) {
		phy->emu_phy_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(phy->emu_phy_base)) {
			dev_dbg(dev, "couldn't ioremap emu_phy_base\n");
			phy->emu_phy_base = NULL;
		}
	}

	/* ref_clk_src is needed irrespective of SE_CLK or DIFF_CLK usage */
	phy->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(phy->ref_clk_src)) {
		dev_dbg(dev, "clk get failed for ref_clk_src\n");
		ret = PTR_ERR(phy->ref_clk_src);
		return ret;
	}

	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "cfg_ahb_clk") >= 0) {
		phy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb_clk");
		if (IS_ERR(phy->cfg_ahb_clk)) {
			ret = PTR_ERR(phy->cfg_ahb_clk);
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
				"clk get failed for cfg_ahb_clk ret %d\n", ret);
			return ret;
		}
	}

	phy->phy_reset = devm_reset_control_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset))
		return PTR_ERR(phy->phy_reset);

	of_get_property(dev->of_node, "qcom,emu-init-seq", &size);
	if (size) {
		phy->emu_init_seq = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (phy->emu_init_seq) {
			phy->emu_init_seq_len =
				(size / sizeof(*phy->emu_init_seq));
			if (phy->emu_init_seq_len % 2) {
				dev_err(dev, "invalid emu_init_seq_len\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,emu-init-seq",
				phy->emu_init_seq,
				phy->emu_init_seq_len);
		} else {
			dev_dbg(dev,
			"error allocating memory for emu_init_seq\n");
		}
	}

	size = 0;
	of_get_property(dev->of_node, "qcom,emu-dcm-reset-seq", &size);
	if (size) {
		phy->emu_dcm_reset_seq = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (phy->emu_dcm_reset_seq) {
			phy->emu_dcm_reset_seq_len =
				(size / sizeof(*phy->emu_dcm_reset_seq));
			if (phy->emu_dcm_reset_seq_len % 2) {
				dev_err(dev, "invalid emu_dcm_reset_seq_len\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,emu-dcm-reset-seq",
				phy->emu_dcm_reset_seq,
				phy->emu_dcm_reset_seq_len);
		} else {
			dev_dbg(dev,
			"error allocating memory for emu_dcm_reset_seq\n");
		}
	}

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		goto err_ret;
	}


	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		ret = PTR_ERR(phy->vdd);
		goto err_ret;
	}

	phy->vdda33 = devm_regulator_get(dev, "vdda33");
	if (IS_ERR(phy->vdda33)) {
		dev_err(dev, "unable to get vdda33 supply\n");
		ret = PTR_ERR(phy->vdda33);
		goto err_ret;
	}

	phy->vdda18 = devm_regulator_get(dev, "vdda18");
	if (IS_ERR(phy->vdda18)) {
		dev_err(dev, "unable to get vdda18 supply\n");
		ret = PTR_ERR(phy->vdda18);
		goto err_ret;
	}

	platform_set_drvdata(pdev, phy);

	if (phy->emu_init_seq)
		phy->phy.init			= msm_hsphy_emu_init;
	else
		phy->phy.init			= msm_hsphy_init;
	phy->phy.set_suspend		= msm_hsphy_set_suspend;
	phy->phy.notify_connect		= msm_hsphy_notify_connect;
	phy->phy.notify_disconnect	= msm_hsphy_notify_disconnect;
	phy->phy.type			= USB_PHY_TYPE_USB2;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		return ret;

	return 0;

err_ret:
	return ret;
}

static int msm_hsphy_remove(struct platform_device *pdev)
{
	struct msm_hsphy *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	usb_remove_phy(&phy->phy);
	clk_disable_unprepare(phy->ref_clk_src);

	msm_hsphy_enable_clocks(phy, false);
	msm_hsphy_enable_power(phy, false);

	kfree(phy);

	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-hsphy-snps-femto",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_hsphy_driver = {
	.probe		= msm_hsphy_probe,
	.remove		= msm_hsphy_remove,
	.driver = {
		.name	= "msm-usb-hsphy",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_hsphy_driver);

MODULE_DESCRIPTION("MSM USB HS PHY driver");
MODULE_LICENSE("GPL v2");

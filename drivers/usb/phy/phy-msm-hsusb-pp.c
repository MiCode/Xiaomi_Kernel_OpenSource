/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/clk/msm-clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <linux/usb/msm_hsusb.h>

static int override_phy_init;
module_param(override_phy_init, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(override_phy_init, "Override HSPHY Init Seq");


#define PORT_OFFSET(i) ((i == 0) ? 0x0 : ((i == 1) ? 0x6c : 0x88))

/* QSCRATCH register settings differ based on MSM core ver */
#define MSM_CORE_VER_120                0x10020061
#define MSM_CORE_VER_160                0x10060000
#define MSM_CORE_VER_161                0x10060001
#define MSM_CORE_VER_191                0x10090001

#define USB_PHY_UTMI_CTRL5              (0x74)
#define USB_PHY_PARAMETER_OVERRIDE_X0   (0x98)
#define USB_PHY_PARAMETER_OVERRIDE_X1   (0x9C)
#define USB_PHY_PARAMETER_OVERRIDE_X2   (0xA0)
#define USB_PHY_PARAMETER_OVERRIDE_X3   (0xA4)
#define USB_PHY_REFCLK_CTRL             (0xE8)
#define USB_PHY_HS_PHY_CTRL_COMMON0     (0x78)
#define HS_PHY_CTRL_COMMON1             (0x7C)
#define HS_PHY_CTRL1                    (0x8C)

/* QSCRATCH register offsets */

#define ALT_INTERRUPT_EN_REG(i)		(0x20 + PORT_OFFSET(i))
#define HS_PHY_CTRL_COMMON_REG		(0xEC)	/* ver >= MSM_CORE_VER_120 */

/*DWC_USB2_SW*/
#define HS_PHY_CTRL_REG		(0x10)

/* HS_PHY_CTRL_REG bits */

#define VBUSVLDEXT0			BIT(5)
#define VBUSVLDEXTSEL0			BIT(4)
#define UTMI_OTG_VBUS_VALID		BIT(20)
/* Following exist only when core_ver >= MSM_CORE_VER_120 */
#define SW_SESSVLD_SEL			BIT(28)


struct msm_hsphy {
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*qscratch_base;
	void __iomem		*tcsr;
	void __iomem		*csr;
	int			hsphy_init_seq;
	bool			set_pllbtune;
	u32			core_ver;

	struct clk		*sleep_clk;
	bool			sleep_clk_reset;

	struct regulator	*vdd;
	struct regulator	*vdda33;
	struct regulator	*vdda18;
	int			vdd_levels[3]; /* none, low, high */
	u32			lpm_flags;
	bool			suspended;
	bool			vdda_force_on;

	/* Using external VBUS/ID notification */
	bool			ext_vbus_id;
	int			num_ports;
	bool			cable_connected;
};

/* global reference counter between all HSPHY instances */
static atomic_t hsphy_active_count;

static int msm_hsusb_config_vdd(struct msm_hsphy *phy, int high)
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
		pr_err("%s: write: %x to offset(%x) FAILED\n",
			__func__, val, offset);
}

static int msm_hsphy_reset(struct usb_phy *uphy)
{
	int ret;

	/* skip reset if there are other active PHY instances */
	ret = atomic_read(&hsphy_active_count);
	if (ret > 1) {
		dev_dbg(uphy->dev, "skipping reset, inuse count=%d\n", ret);
		return 0;
	}

	return 0;
}

static int msm_hsphy_init(struct usb_phy *uphy)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	msm_hsphy_reset(uphy);
	phy->core_ver = readl_relaxed(phy->qscratch_base);
	writel_relaxed(0x02, phy->base + USB_PHY_UTMI_CTRL5);
	usleep(10);
	writel_relaxed(0x00, phy->base + USB_PHY_UTMI_CTRL5);

	/* Set REFCLCK */
	msm_usb_write_readback(phy->base, USB_PHY_REFCLK_CTRL,
						(7 << 1), (6 << 1));
	msm_usb_write_readback(phy->base, USB_PHY_HS_PHY_CTRL_COMMON0,
						(7 << 4), (7 << 4));

	/*
	 * write HSPHY init value to QSCRATCH reg to set HSPHY parameters like
	 * VBUS valid threshold, disconnect valid threshold, DC voltage level,
	 * preempasis and rise/fall time.
	 */
	if (override_phy_init)
		phy->hsphy_init_seq = override_phy_init;

	if (phy->hsphy_init_seq) {
			writel_relaxed(phy->hsphy_init_seq & 0xFF,
				phy->base + USB_PHY_PARAMETER_OVERRIDE_X0);
			writel_relaxed((phy->hsphy_init_seq & 0x0000FF00) >> 8,
				phy->base + USB_PHY_PARAMETER_OVERRIDE_X1);
			writel_relaxed((phy->hsphy_init_seq & 0x00FF0000) >> 16,
				phy->base + USB_PHY_PARAMETER_OVERRIDE_X2);
			writel_relaxed((phy->hsphy_init_seq & 0xFF000000) >> 24,
				phy->base + USB_PHY_PARAMETER_OVERRIDE_X3);
	}
	return 0;
}

static int msm_hsphy_notify_connect(struct usb_phy *uphy,
				    enum usb_device_speed speed)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	phy->cable_connected = true;

	if (uphy->flags & PHY_HOST_MODE)
		return 0;

	if (!(uphy->flags & PHY_VBUS_VALID_OVERRIDE))
		return 0;

	/* Set External VBUS Valid Select. Set once, can be left on */
	msm_usb_write_readback(phy->base, HS_PHY_CTRL_COMMON1,
					VBUSVLDEXTSEL0,
					VBUSVLDEXTSEL0);

	/* Enable D+ pull-up resistor */
	msm_usb_write_readback(phy->base,
				HS_PHY_CTRL1,
				VBUSVLDEXT0, VBUSVLDEXT0);

	/* Set OTG VBUS Valid from HSPHY to controller */
	msm_usb_write_readback(phy->qscratch_base, HS_PHY_CTRL_REG,
				UTMI_OTG_VBUS_VALID,
				UTMI_OTG_VBUS_VALID);

	/* Indicate value is driven by UTMI_OTG_VBUS_VALID bit */
	msm_usb_write_readback(phy->qscratch_base, HS_PHY_CTRL_REG,
					SW_SESSVLD_SEL, SW_SESSVLD_SEL);

	return 0;
}

static int msm_hsphy_notify_disconnect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_hsphy *phy = container_of(uphy, struct msm_hsphy, phy);

	phy->cable_connected = false;

	if (uphy->flags & PHY_HOST_MODE)
		return 0;

	if (!(uphy->flags & PHY_VBUS_VALID_OVERRIDE))
		return 0;

	/* Clear OTG VBUS Valid to Controller */
	msm_usb_write_readback(phy->qscratch_base, HS_PHY_CTRL_REG,
				UTMI_OTG_VBUS_VALID, 0);

	/* Disable D+ pull-up resistor */
	msm_usb_write_readback(phy->qscratch_base,
					HS_PHY_CTRL1, VBUSVLDEXT0, 0);

	/* Indicate value is no longer driven by UTMI_OTG_VBUS_VALID bit */
	msm_usb_write_readback(phy->qscratch_base, HS_PHY_CTRL_REG,
					SW_SESSVLD_SEL, 0);

	return 0;
}

static int msm_hsphy_probe(struct platform_device *pdev)
{
	struct msm_hsphy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		ret = -ENOMEM;
		goto err_ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "hsphy_base");
	if (!res) {
		dev_err(dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto err_ret;
	}

	phy->base = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!phy->base) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENODEV;
		goto err_ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"qscratch_phy");
	if (!res) {
		dev_err(dev, "missing memory base resource\n");
		ret = -ENODEV;
		goto err_ret;
	}

	phy->qscratch_base = devm_ioremap_nocache(dev, res->start,
							resource_size(res));
	if (!phy->qscratch_base) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENODEV;
		goto err_ret;
	}

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		goto err_ret;
	}

	phy->ext_vbus_id = of_property_read_bool(dev->of_node,
						"qcom,ext-vbus-id");
	phy->phy.dev = dev;

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

	ret = msm_hsusb_config_vdd(phy, 1);
	if (ret) {
		dev_err(dev, "hsusb vdd_dig configuration failed\n");
		goto err_ret;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(dev, "unable to enable the hsusb vdd_dig\n");
		goto unconfig_hs_vdd;
	}

	if (ret) {
		dev_err(dev, "hsusb vreg enable failed\n");
		goto disable_hs_vdd;
	}

	if (of_property_read_u32(dev->of_node, "qcom,hsphy-init",
					&phy->hsphy_init_seq))
		dev_dbg(dev, "unable to read hsphy init seq\n");
	else if (!phy->hsphy_init_seq)
		dev_warn(dev, "hsphy init seq cannot be 0. Using POR value\n");

	if (of_property_read_u32(dev->of_node, "qcom,num-ports",
					&phy->num_ports))
		phy->num_ports = 1;
	else if (phy->num_ports > 3) {
		dev_err(dev, " number of ports more that 3 is not supported\n");
		goto disable_clk;
	}


	/*
	 * If this workaround flag is enabled, the HW requires the 1.8 and 3.x
	 * regulators to be kept ON when entering suspend. The easiest way to
	 * do that is to call regulator_enable() an additional time here,
	 * since it will keep the regulators' reference counts nonzero.
	 */
	phy->vdda_force_on = of_property_read_bool(dev->of_node,
						"qcom,vdda-force-on");
	if (phy->vdda_force_on) {
		if (ret)
			goto disable_clk;
	}

	platform_set_drvdata(pdev, phy);

	if (of_property_read_bool(dev->of_node, "qcom,vbus-valid-override"))
		phy->phy.flags |= PHY_VBUS_VALID_OVERRIDE;

	phy->phy.init			= msm_hsphy_init;
	phy->phy.notify_connect		= msm_hsphy_notify_connect;
	phy->phy.notify_disconnect	= msm_hsphy_notify_disconnect;
	phy->phy.reset			= msm_hsphy_reset;
	phy->phy.type			= USB_PHY_TYPE_USB2;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		goto disable_clk;

	atomic_inc(&hsphy_active_count);
	return 0;

disable_clk:
	clk_disable_unprepare(phy->sleep_clk);
disable_hs_vdd:
	regulator_disable(phy->vdd);
unconfig_hs_vdd:
	msm_hsusb_config_vdd(phy, 0);
err_ret:
	return ret;
}

static int msm_hsphy_remove(struct platform_device *pdev)
{
	struct msm_hsphy *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	usb_remove_phy(&phy->phy);
	clk_disable_unprepare(phy->sleep_clk);


	msm_hsusb_config_vdd(phy, 0);
	if (!phy->suspended)
		atomic_dec(&hsphy_active_count);
	kfree(phy);

	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-hsphy-pp",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_hsphy_driver = {
	.probe		= msm_hsphy_probe,
	.remove		= msm_hsphy_remove,
	.driver = {
		.name	= "msm-usb-hsphy-pp",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_hsphy_driver);

MODULE_DESCRIPTION("MSM USB HS-PP driver");
MODULE_LICENSE("GPL v2");

/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

static int ss_phy_override_deemphasis;
module_param(ss_phy_override_deemphasis, int, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(ss_phy_override_deemphasis, "Override SSPHY demphasis value");

/* QSCRATCH SSPHY control registers */
#define SS_PHY_CTRL_REG			0x30
#define SS_PHY_PARAM_CTRL_1		0x34
#define SS_PHY_PARAM_CTRL_2		0x38
#define SS_CR_PROTOCOL_DATA_IN_REG	0x3C
#define SS_CR_PROTOCOL_DATA_OUT_REG	0x40
#define SS_CR_PROTOCOL_CAP_ADDR_REG	0x44
#define SS_CR_PROTOCOL_CAP_DATA_REG	0x48
#define SS_CR_PROTOCOL_READ_REG		0x4C
#define SS_CR_PROTOCOL_WRITE_REG	0x50

/* SS_PHY_CTRL_REG bits */
#define SS_PHY_RESET			BIT(7)
#define REF_SS_PHY_EN			BIT(8)
#define LANE0_PWR_PRESENT		BIT(24)
#define TEST_POWERDOWN			BIT(26)
#define REF_USE_PAD			BIT(28)

#define USB_SSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_SSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_SSPHY_1P8_HPM_LOAD		23000	/* uA */

struct msm_ssphy {
	struct usb_phy		phy;
	void __iomem		*base;
	struct clk		*core_clk;	/* USB3 master clock */
	struct clk		*com_reset_clk;	/* PHY common block reset */
	struct clk		*reset_clk;	/* SS PHY reset */
	struct regulator	*vdd;
	struct regulator	*vdda18;
	atomic_t		active_count;	/* num of active instances */
	bool			suspended;
	int			vdd_levels[3]; /* none, low, high */
	int			deemphasis_val;
};

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

	if (!on)
		goto disable_regulators;


	rc = regulator_set_optimum_mode(phy->vdda18, USB_SSPHY_1P8_HPM_LOAD);
	if (rc < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda18\n");
		return rc;
	}

	rc = regulator_set_voltage(phy->vdda18, USB_SSPHY_1P8_VOL_MIN,
						USB_SSPHY_1P8_VOL_MAX);
	if (rc) {
		dev_err(phy->phy.dev, "unable to set voltage for vdda18\n");
		goto put_vdda18_lpm;
	}

	rc = regulator_enable(phy->vdda18);
	if (rc) {
		dev_err(phy->phy.dev, "Unable to enable vdda18\n");
		goto unset_vdda18;
	}

	return 0;

disable_regulators:
	rc = regulator_disable(phy->vdda18);
	if (rc)
		dev_err(phy->phy.dev, "Unable to disable vdda18\n");

unset_vdda18:
	rc = regulator_set_voltage(phy->vdda18, 0, USB_SSPHY_1P8_VOL_MAX);
	if (rc)
		dev_err(phy->phy.dev, "unable to set voltage for vdda18\n");

put_vdda18_lpm:
	rc = regulator_set_optimum_mode(phy->vdda18, 0);
	if (rc < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdda18\n");

	return rc < 0 ? rc : 0;
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

/**
 * Write SSPHY register with debug info.
 *
 * @base - base virtual address.
 * @addr - SSPHY address to write.
 * @val - value to write.
 *
 */
static void msm_ssusb_write_phycreg(void *base, u32 addr, u32 val)
{
	writel_relaxed(addr, base + SS_CR_PROTOCOL_DATA_IN_REG);
	writel_relaxed(0x1, base + SS_CR_PROTOCOL_CAP_ADDR_REG);
	while (readl_relaxed(base + SS_CR_PROTOCOL_CAP_ADDR_REG))
		cpu_relax();

	writel_relaxed(val, base + SS_CR_PROTOCOL_DATA_IN_REG);
	writel_relaxed(0x1, base + SS_CR_PROTOCOL_CAP_DATA_REG);
	while (readl_relaxed(base + SS_CR_PROTOCOL_CAP_DATA_REG))
		cpu_relax();

	writel_relaxed(0x1, base + SS_CR_PROTOCOL_WRITE_REG);
	while (readl_relaxed(base + SS_CR_PROTOCOL_WRITE_REG))
		cpu_relax();
}

/**
 * Read SSPHY register with debug info.
 *
 * @base - base virtual address.
 * @addr - SSPHY address to read.
 *
 */
static u32 msm_ssusb_read_phycreg(void *base, u32 addr)
{
	bool first_read = true;

	writel_relaxed(addr, base + SS_CR_PROTOCOL_DATA_IN_REG);
	writel_relaxed(0x1, base + SS_CR_PROTOCOL_CAP_ADDR_REG);
	while (readl_relaxed(base + SS_CR_PROTOCOL_CAP_ADDR_REG))
		cpu_relax();

	/*
	 * Due to hardware bug, first read of SSPHY register might be
	 * incorrect. Hence as workaround, SW should perform SSPHY register
	 * read twice, but use only second read and ignore first read.
	 */
retry:
	writel_relaxed(0x1, base + SS_CR_PROTOCOL_READ_REG);
	while (readl_relaxed(base + SS_CR_PROTOCOL_READ_REG))
		cpu_relax();

	if (first_read) {
		readl_relaxed(base + SS_CR_PROTOCOL_DATA_OUT_REG);
		first_read = false;
		goto retry;
	}

	return readl_relaxed(base + SS_CR_PROTOCOL_DATA_OUT_REG);
}

/* SSPHY Initialization */
static int msm_ssphy_init(struct usb_phy *uphy)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	u32 val;

	/* Ensure clock is on before accessing QSCRATCH registers */
	clk_prepare_enable(phy->core_clk);

	/* read initial value */
	val = readl_relaxed(phy->base + SS_PHY_CTRL_REG);

	/* Use clk reset, if available; otherwise use SS_PHY_RESET bit */
	if (phy->com_reset_clk) {
		clk_reset(phy->com_reset_clk, CLK_RESET_ASSERT);
		clk_reset(phy->reset_clk, CLK_RESET_ASSERT);
		udelay(10); /* 10us required before de-asserting */
		clk_reset(phy->com_reset_clk, CLK_RESET_DEASSERT);
		clk_reset(phy->reset_clk, CLK_RESET_DEASSERT);
	} else {
		writel_relaxed(val | SS_PHY_RESET, phy->base + SS_PHY_CTRL_REG);
		udelay(10); /* 10us required before de-asserting */
		writel_relaxed(val, phy->base + SS_PHY_CTRL_REG);
	}

	/* Use ref_clk from pads and set its parameters */
	val |= REF_USE_PAD;
	writel_relaxed(val, phy->base + SS_PHY_CTRL_REG);
	msleep(30);

	/* Ref clock must be stable now, enable ref clock for HS mode */
	val |= LANE0_PWR_PRESENT | REF_SS_PHY_EN;
	writel_relaxed(val, phy->base + SS_PHY_CTRL_REG);
	usleep_range(2000, 2200);

	clk_disable_unprepare(phy->core_clk);

	return 0;
}

static int msm_ssphy_set_params(struct usb_phy *uphy)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	u32 data = 0;

	/*
	 * WORKAROUND: There is SSPHY suspend bug due to which USB enumerates
	 * in HS mode instead of SS mode. Workaround it by asserting
	 * LANE0.TX_ALT_BLOCK.EN_ALT_BUS to enable TX to use alt bus mode
	 */
	data = msm_ssusb_read_phycreg(phy->base, 0x102D);
	data |= (1 << 7);
	msm_ssusb_write_phycreg(phy->base, 0x102D, data);

	data = msm_ssusb_read_phycreg(phy->base, 0x1010);
	data &= ~0xFF0;
	data |= 0x20;
	msm_ssusb_write_phycreg(phy->base, 0x1010, data);

	/*
	 * Fix RX Equalization setting as follows
	 * LANE0.RX_OVRD_IN_HI. RX_EQ_EN set to 0
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_EN_OVRD set to 1
	 * LANE0.RX_OVRD_IN_HI.RX_EQ set to 3
	 * LANE0.RX_OVRD_IN_HI.RX_EQ_OVRD set to 1
	 */
	data = msm_ssusb_read_phycreg(phy->base, 0x1006);
	data &= ~(1 << 6);
	data |= (1 << 7);
	data &= ~(0x7 << 8);
	data |= (0x3 << 8);
	data |= (0x1 << 11);
	msm_ssusb_write_phycreg(phy->base, 0x1006, data);

	/*
	 * Set EQ and TX launch amplitudes as follows
	 * LANE0.TX_OVRD_DRV_LO.PREEMPH set to 22
	 * LANE0.TX_OVRD_DRV_LO.AMPLITUDE set to 127
	 * LANE0.TX_OVRD_DRV_LO.EN set to 1.
	 */
	data = msm_ssusb_read_phycreg(phy->base, 0x1002);
	data &= ~0x3F80;
	if (ss_phy_override_deemphasis)
		phy->deemphasis_val = ss_phy_override_deemphasis;
	if (phy->deemphasis_val)
		data |= (phy->deemphasis_val << 7);
	else
		data |= (0x16 << 7);
	data &= ~0x7F;
	data |= (0x7F | (1 << 14));
	msm_ssusb_write_phycreg(phy->base, 0x1002, data);

	/*
	 * Set the QSCRATCH SS_PHY_PARAM_CTRL1 parameters as follows
	 * TX_FULL_SWING [26:20] amplitude to 127
	 * TX_DEEMPH_3_5DB [13:8] to 22
	 * LOS_BIAS [2:0] to 0x5
	 */
	msm_usb_write_readback(phy->base, SS_PHY_PARAM_CTRL_1,
				0x07f03f07, 0x07f01605);

	return 0;
}

static int msm_ssphy_post_init(struct usb_phy *uphy)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	u32 val;

	/* read initial value */
	val = readl_relaxed(phy->base + SS_PHY_CTRL_REG);
	val &= ~LANE0_PWR_PRESENT;
	writel_relaxed(val, phy->base + SS_PHY_CTRL_REG);

	return 0;
}

static int msm_ssphy_set_suspend(struct usb_phy *uphy, int suspend)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	void __iomem *base = phy->base;
	int count;

	/* Ensure clock is on before accessing QSCRATCH registers */
	clk_prepare_enable(phy->core_clk);

	if (suspend) {
		count = atomic_dec_return(&phy->active_count);
		if (count > 0 || phy->suspended) {
			dev_dbg(uphy->dev, "Skipping suspend, active_count=%d phy->suspended=%d\n",
					count, phy->suspended);
			goto done;
		}

		if (count < 0) {
			dev_WARN(uphy->dev, "Suspended too many times!  active_count=%d\n",
					count);
			atomic_set(&phy->active_count, 0);
		}

		/* Clear REF_SS_PHY_EN */
		msm_usb_write_readback(base, SS_PHY_CTRL_REG, REF_SS_PHY_EN, 0);
		/* Clear REF_USE_PAD */
		msm_usb_write_readback(base, SS_PHY_CTRL_REG, REF_USE_PAD, 0);
		/* Set TEST_POWERDOWN (enables PHY retention) */
		msm_usb_write_readback(base, SS_PHY_CTRL_REG, TEST_POWERDOWN,
								TEST_POWERDOWN);
		if (phy->com_reset_clk &&
			!(phy->phy.flags & ENABLE_SECONDARY_PHY)) {
			/* leave these asserted until resuming */
			clk_reset(phy->com_reset_clk, CLK_RESET_ASSERT);
			clk_reset(phy->reset_clk, CLK_RESET_ASSERT);
		}

		msm_ssusb_ldo_enable(phy, 0);
		msm_ssusb_config_vdd(phy, 0);
		phy->suspended = true;
	} else {
		count = atomic_inc_return(&phy->active_count);
		if (count > 1 || !phy->suspended) {
			dev_dbg(uphy->dev, "Skipping resume, active_count=%d phy->suspended=%d\n",
					count, phy->suspended);
			goto done;
		}

		phy->suspended = false;
		msm_ssusb_config_vdd(phy, 1);
		msm_ssusb_ldo_enable(phy, 1);

		if (phy->phy.flags & ENABLE_SECONDARY_PHY) {
			dev_err(uphy->dev, "secondary PHY, skipping reset\n");
			goto done;
		}

		if (phy->com_reset_clk) {
			clk_reset(phy->com_reset_clk, CLK_RESET_DEASSERT);
			clk_reset(phy->reset_clk, CLK_RESET_DEASSERT);
		} else {
			/* Assert SS PHY RESET */
			msm_usb_write_readback(base, SS_PHY_CTRL_REG,
						SS_PHY_RESET, SS_PHY_RESET);
		}

		/* Set REF_USE_PAD */
		msm_usb_write_readback(base, SS_PHY_CTRL_REG, REF_USE_PAD,
								REF_USE_PAD);
		/* Set REF_SS_PHY_EN */
		msm_usb_write_readback(base, SS_PHY_CTRL_REG, REF_SS_PHY_EN,
								REF_SS_PHY_EN);
		/* Clear TEST_POWERDOWN */
		msm_usb_write_readback(base, SS_PHY_CTRL_REG, TEST_POWERDOWN,
								0);
		if (!phy->com_reset_clk) {
			udelay(10); /* 10us required before de-asserting */
			msm_usb_write_readback(base, SS_PHY_CTRL_REG,
						SS_PHY_RESET, 0);
		}

		/*
		 * Reinitialize SSPHY parameters as SS_PHY RESET will reset
		 * the internal registers to default values.
		 */
		msm_ssphy_set_params(uphy);
	}

done:
	clk_disable_unprepare(phy->core_clk);
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
		msm_usb_write_readback(phy->base, SS_PHY_CTRL_REG,
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
		msm_usb_write_readback(phy->base, SS_PHY_CTRL_REG,
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

	phy->core_clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(phy->core_clk)) {
		dev_err(dev, "unable to get core_clk\n");
		return PTR_ERR(phy->core_clk);
	}

	phy->com_reset_clk = devm_clk_get(dev, "com_reset_clk");
	if (IS_ERR(phy->com_reset_clk)) {
		dev_dbg(dev, "com_reset_clk unavailable\n");
		phy->com_reset_clk = NULL;
	}

	phy->reset_clk = devm_clk_get(dev, "reset_clk");
	if (IS_ERR(phy->reset_clk)) {
		dev_dbg(dev, "reset_clk unavailable\n");
		phy->reset_clk = NULL;
	}

	if (of_get_property(dev->of_node, "qcom,primary-phy", NULL)) {
		dev_dbg(dev, "secondary HSPHY\n");
		phy->phy.flags |= ENABLE_SECONDARY_PHY;
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
		dev_err(dev, "ssusb vdd_dig configuration failed\n");
		return ret;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(dev, "unable to enable the ssusb vdd_dig\n");
		goto unconfig_ss_vdd;
	}

	ret = msm_ssusb_ldo_enable(phy, 1);
	if (ret) {
		dev_err(dev, "ssusb vreg enable failed\n");
		goto disable_ss_vdd;
	}

	platform_set_drvdata(pdev, phy);

	if (of_property_read_bool(dev->of_node, "qcom,vbus-valid-override"))
		phy->phy.flags |= PHY_VBUS_VALID_OVERRIDE;

	if (of_property_read_u32(dev->of_node, "qcom,deemphasis-value",
						&phy->deemphasis_val))
		dev_dbg(dev, "unable to read ssphy deemphasis value\n");

	phy->phy.init			= msm_ssphy_init;
	phy->phy.set_suspend		= msm_ssphy_set_suspend;
	phy->phy.set_params		= msm_ssphy_set_params;
	phy->phy.post_init		= msm_ssphy_post_init;
	phy->phy.notify_connect		= msm_ssphy_notify_connect;
	phy->phy.notify_disconnect	= msm_ssphy_notify_disconnect;
	phy->phy.type			= USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		goto disable_ss_ldo;

	return 0;

disable_ss_ldo:
	msm_ssusb_ldo_enable(phy, 0);
disable_ss_vdd:
	regulator_disable(phy->vdd);
unconfig_ss_vdd:
	msm_ssusb_config_vdd(phy, 0);

	return ret;
}

static int msm_ssphy_remove(struct platform_device *pdev)
{
	struct msm_ssphy *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	usb_remove_phy(&phy->phy);
	msm_ssusb_ldo_enable(phy, 0);
	regulator_disable(phy->vdd);
	msm_ssusb_config_vdd(phy, 0);
	kfree(phy);

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

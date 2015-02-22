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

struct msm_ssphy {
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*qscratch_base;
	struct clk		*core_clk;	/* USB3 master clock */
	struct clk		*com_reset_clk;	/* PHY common block reset */
	struct clk		*reset_clk;	/* SS PHY reset */
	struct clk		*phy_reset;
	struct clk		*phy_phy_reset;
	struct regulator	*vdd;
	struct regulator	*vdda18;
	atomic_t		active_count;	/* num of active instances */
	bool			suspended;
	int			vdd_levels[3]; /* none, low, high */
	int			deemphasis_val;
};

/*TERMINATION VALS*/
#define TX_TERM_OFFSET_VAL                  0x0
#define TX_DEEMPH_VAL                       0x20
#define TX_AMP_VAL                          0x6E
#define RX_EQ_VAL                           0x2

/*SSPHY BASE*/
#define USB3_PHY_CR_REG_DATA_STATUS0        0x30
#define USB3_PHY_CR_REG_DATA_STATUS1        0x34
#define USB3_PHY_CR_REG_DATA_STATUS2        0x38
#define USB3_PHY_CR_REG_CTRL1               0x60
#define USB3_PHY_CR_REG_CTRL2               0x64
#define USB3_PHY_CR_REG_CTRL3               0x68
#define USB30_QSCRATCH_SS_PHY_CTRL0         0x6C
#define USB30_QSCRATCH_SS_PHY_CTRL2         0x74
#define USB30_QSCRATCH_SS_PHY_CTRL4         0x7C
#define USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1B 0x88
#define USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1C 0x8C
#define USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1D 0x90
#define USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1E 0x94
#define USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1F 0x98

/*QSCARTCH BASE*/
#define GENERAL_CFG_REG                     0x08
#define SS_PHY_CTRL_REG                     0x30

/* SS_PHY_CTRL_REG bits */
#define SS_PHY_RESET                    BIT(7)
#define REF_SS_PHY_EN                   BIT(8)
#define LANE0_PWR_PRESENT               BIT(24)
#define TEST_POWERDOWN                  BIT(26)
#define REF_USE_PAD                     BIT(28)


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

static int read_phy_Creg(struct msm_ssphy *phy,
		void __iomem *ssphy_base, u32 addr)
{
	u16 read_data;

	/* Write the Address */
	writel_relaxed((addr & 0xFF), ssphy_base + USB3_PHY_CR_REG_CTRL2);
	writel_relaxed(((addr>>8) & 0xFF), ssphy_base + USB3_PHY_CR_REG_CTRL3);
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, 1, 1);
	while (readl_relaxed(ssphy_base + USB3_PHY_CR_REG_DATA_STATUS2) == 0)
		cpu_relax();
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, 1, 0);

	/* Issue Read Command */
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1,
							(1 << 2), (1 << 2));
	while (readl_relaxed(ssphy_base + USB3_PHY_CR_REG_DATA_STATUS2) == 0)
		cpu_relax();
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, 1, 0);

	/* Read out the register data */
	read_data = readl_relaxed(ssphy_base +
					USB3_PHY_CR_REG_DATA_STATUS0) & 0xFF;
	read_data |= (readl_relaxed(ssphy_base +
				USB3_PHY_CR_REG_DATA_STATUS1) << 8) & 0xFF00;

	return read_data;
}

static int write_phy_Creg(struct msm_ssphy *phy, void __iomem *ssphy_base,
		u32 addr, u16 data)
{

	/* Write the Address */
	writel_relaxed((addr & 0xFF), ssphy_base + USB3_PHY_CR_REG_CTRL2);
	writel_relaxed(((addr>>8) & 0xFF), ssphy_base + USB3_PHY_CR_REG_CTRL3);
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, 1, 1);
	while (readl_relaxed(ssphy_base + USB3_PHY_CR_REG_DATA_STATUS2) == 0)
		cpu_relax();
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, 1, 0);

	/* Write the Address */
	writel_relaxed((addr & 0xFF), ssphy_base + USB3_PHY_CR_REG_CTRL2);
	writel_relaxed(((addr>>8) & 0xFF), ssphy_base + USB3_PHY_CR_REG_CTRL3);
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1,
							(1 << 1), (1 << 1));
	while (readl_relaxed(ssphy_base + USB3_PHY_CR_REG_DATA_STATUS2) == 0)
		cpu_relax();

	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, (1 << 1), 0);

	/* Issue Write Command */
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1,
							(1 << 3), (1 << 3));
	while (readl_relaxed(ssphy_base + USB3_PHY_CR_REG_DATA_STATUS2) == 0)
		cpu_relax();
	msm_usb_write_readback(ssphy_base, USB3_PHY_CR_REG_CTRL1, (1 << 3), 0);

	return 0;
}

static int msm_ssphy_init(struct usb_phy *uphy)
{
	struct msm_ssphy *phy = container_of(uphy, struct msm_ssphy, phy);
	int ret;
	u32 val;
	u16 tmp;


	/* read initial value */
	val = readl_relaxed(phy->base + SS_PHY_CTRL_REG);

	/* Deassert USB3 PHY reset */
	ret = clk_reset(phy->phy_phy_reset, CLK_RESET_ASSERT);
	if (ret) {
		dev_err(uphy->dev, "phy_phy reset deassert failed\n");
		goto deassert_phy_phy_reset;
	}

	/* Assert USB3 PHY CSR reset */
	ret = clk_reset(phy->phy_reset, CLK_RESET_ASSERT);
	if (ret) {
		dev_err(uphy->dev, "phy_reset clk assert failed\n");
		goto deassert_phy_reset;
	}

	/* Deassert USB3 PHY CSR reset */
	ret = clk_reset(phy->phy_reset, CLK_RESET_DEASSERT);
	if (ret) {
		dev_err(uphy->dev, "phy_reset clk deassert failed\n");
		goto deassert_phy_reset;
	}



	writel_relaxed(0x10, phy->base + USB30_QSCRATCH_SS_PHY_CTRL0);
	writel_relaxed(0x84, phy->base + USB30_QSCRATCH_SS_PHY_CTRL4);
	writel_relaxed(0x01, phy->base + USB30_QSCRATCH_SS_PHY_CTRL2);


	/* Program Tx Termination offset */
	val = readl_relaxed(phy->base + USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1E);
	if (TX_TERM_OFFSET_VAL != val) {
		writel_relaxed(TX_TERM_OFFSET_VAL, phy->base +
					USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1E);
	}

	/* Program Tx De-emphasis */
	val = readl_relaxed(phy->base + USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1B);
	if (TX_DEEMPH_VAL != val) {
		writel_relaxed(TX_DEEMPH_VAL, phy->base +
					USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1B);
		writel_relaxed(TX_DEEMPH_VAL, phy->base +
					USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1C);
	}

	/* Program Tx Amplitude */
	val = readl_relaxed(phy->base + USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1D);
	if (TX_AMP_VAL != val) {
		writel_relaxed(TX_AMP_VAL, phy->base +
					USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1D);
		writel_relaxed(TX_AMP_VAL, phy->base +
					USB30_QSCRATCH_SS_PHY_PARAM_CTRL_1F);
	}

	/* Fixed EQ setting */
	val = read_phy_Creg(phy, phy->base, 0x1006);
	if (val == -1)
		return -EAGAIN;

	tmp = (u16)val;
	if (tmp != RX_EQ_VAL) { /* Values from 1 to 7 */
		val = read_phy_Creg(phy, phy->base, 0x1006);
		if (val == -1)
			return -EAGAIN;

		tmp = (u16)val;
		tmp &= ~((u16) 1 << 6);
		tmp |= ((u16) 1 << 7);
		tmp &= ~((u16) 0x7 << 8);
		tmp |= RX_EQ_VAL << 8;
		tmp |= 1 << 11;

		if (write_phy_Creg(phy, phy->base, 0x1006, tmp))
			return -EAGAIN;
	}

	msm_usb_write_readback(phy->base, USB30_QSCRATCH_SS_PHY_CTRL4,
								(1 << 6), 0);
	usleep(1000);

	/* Deassert USB3 PHY reset */
	ret = clk_reset(phy->phy_phy_reset, CLK_RESET_DEASSERT);
	if (ret) {
		dev_err(uphy->dev, "phy_phy reset deassert failed\n");
		goto deassert_phy_phy_reset;
	}

	return 0;

deassert_phy_phy_reset:
	if (phy->phy_phy_reset)
		clk_reset(phy->phy_phy_reset, CLK_RESET_DEASSERT);
deassert_phy_reset:
	if (phy->phy_reset)
		clk_reset(phy->phy_reset, CLK_RESET_DEASSERT);

	return ret;
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

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ssphy_base");
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

	phy->phy_reset = devm_clk_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset)) {
		dev_err(dev, "failed to get phy_reset\n");
		ret = PTR_ERR(phy->phy_reset);
	}

	phy->phy_phy_reset = devm_clk_get(dev, "phy_phy_reset");
	if (IS_ERR(phy->phy_phy_reset)) {
		phy->phy_phy_reset = NULL;
		dev_dbg(dev, "phy_phy_reset unavailable\n");
	}

	phy->phy.dev = dev;

	platform_set_drvdata(pdev, phy);

	phy->phy.init = msm_ssphy_init;
	phy->phy.notify_connect = msm_ssphy_notify_connect;
	phy->phy.notify_disconnect = msm_ssphy_notify_disconnect;
	phy->phy.type = USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		return ret;

	return 0;
err_ret:
	return ret;
}

static int msm_ssphy_remove(struct platform_device *pdev)
{
	struct msm_ssphy *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	usb_remove_phy(&phy->phy);
	kfree(phy);

	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-ssphy-pp",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_ssphy_driver = {
	.probe		= msm_ssphy_probe,
	.remove		= msm_ssphy_remove,
	.driver = {
		.name	= "msm-usb-ssphy-pp",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_ssphy_driver);

MODULE_DESCRIPTION("MSM USB SS-PP driver");
MODULE_LICENSE("GPL v2");


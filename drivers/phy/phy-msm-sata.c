/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>

/* QSERDES COMMON registers */
#define QSERDES_COM_SYS_CLK_CTRL	0x000
#define QSERDES_COM_PLL_IP_SETI		0x018
#define QSERDES_COM_PLL_CP_SETI		0x024
#define QSERDES_COM_PLL_IP_SETP		0x028
#define QSERDES_COM_PLL_CP_SETP		0x02c
#define QSERDES_COM_SYSCLK_EN_SEL	0x038
#define QSERDES_COM_RESETSM_CNTRL	0x040
#define QSERDES_COM_PLLLOCK_CMP1	0x044
#define QSERDES_COM_PLLLOCK_CMP2	0x048
#define QSERDES_COM_PLLLOCK_CMP3	0x04c
#define QSERDES_COM_PLLLOCK_CMP_EN	0x050
#define QSERDES_COM_DEC_START1		0x064
#define QSERDES_COM_SSC_EN_CENTER	0x06c
#define QSERDES_COM_SSC_ADJ_PER1	0x070
#define QSERDES_COM_SSC_ADJ_PER2	0x074
#define QSERDES_COM_SSC_PER1		0x078
#define QSERDES_COM_SSC_PER2		0x07c
#define QSERDES_COM_SSC_STEP_SIZE1	0x080
#define QSERDES_COM_SSC_STEP_SIZE2	0x084
#define QSERDES_COM_DIV_FRAC_START1	0x098
#define QSERDES_COM_DIV_FRAC_START2	0x09c
#define QSERDES_COM_DIV_FRAC_START3	0x0a0
#define QSERDES_COM_DEC_START2		0x0a4
#define QSERDES_COM_PLL_CRCTRL		0x0ac
#define QSERDES_COM_RESET_SM		0x0bc

/* QSERDES TX registers */
#define QSERDES_TX_BIST_MODE_LANENO	0x100
#define QSERDES_TX_TX_EMP_POST1_LVL	0x108
#define QSERDES_TX_TX_DRV_LVL		0x10c

/* QSERDES RX registers */
#define QSERDES_RX_CDR_CONTROL		0x200
#define QSERDES_RX_CDR_CONTROL2		0x210
#define QSERDES_RX_RX_EQ_GAIN12		0x230
#define QSERDES_RX_PWM_CNTRL1		0x280
#define QSERDES_RX_PWM_CNTRL2		0x284
#define QSERDES_RX_CDR_CONTROL_QUARTER	0x29c

/* SATA PHY registers */
#define SATA_PHY_SERDES_START		0x300
#define SATA_PHY_CMN_PWR_CTRL		0x304
#define SATA_PHY_RX_PWR_CTRL		0x308
#define SATA_PHY_TX_PWR_CTRL		0x30c
#define SATA_PHY_LANE_CTRL1		0x318
#define SATA_PHY_CDR_CTRL0		0x358
#define SATA_PHY_CDR_CTRL1		0x35c
#define SATA_PHY_TX_DRV_WAKEUP		0x360
#define SATA_PHY_CLK_BUF_SETTLING	0x364
#define SATA_PHY_SPDNEG_CFG0		0x370
#define SATA_PHY_SPDNEG_CFG1		0x374
#define SATA_PHY_POW_DWN_CTRL0		0x380
#define SATA_PHY_ALIGNP			0x3a4

#define MAX_PROP_NAME              32
#define VDDA_PHY_MIN_UV            950000
#define VDDA_PHY_MAX_UV            1000000
#define VDDA_PLL_MIN_UV            1800000
#define VDDA_PLL_MAX_UV            1800000

struct msm_sata_phy_vreg {
	const char *name;
	struct regulator *reg;
	int max_uA;
	int min_uV;
	int max_uV;
	bool enabled;
};

struct msm_sata_phy {
	struct device *dev;
	void __iomem *mmio;
	void __iomem *phy_sel;
	struct clk *ref_clk_src;
	struct clk *ref_clk_parent;
	struct clk *ref_clk;
	struct clk *rxoob_clk;
	bool is_ref_clk_enabled;
	bool is_rxoob_clk_enabled;
	struct msm_sata_phy_vreg vdda_pll;
	struct msm_sata_phy_vreg vdda_phy;
	bool is_powered_on;
};

static int msm_sata_enable_phy_rxoob_clk(struct msm_sata_phy *phy)
{
	int err = 0;

	if (phy->is_rxoob_clk_enabled)
		goto out;

	/* set max. 100MHz */
	err = clk_set_rate(phy->rxoob_clk, 100000000);
	if (err) {
		dev_err(phy->dev, "%s: rxoob_clk set rate failed %d\n",
				__func__, err);
		goto out;
	}

	err = clk_prepare_enable(phy->rxoob_clk);
	if (err) {
		dev_err(phy->dev, "%s: rxoob_clk enable failed %d\n",
				__func__, err);
		goto out;
	}

	phy->is_rxoob_clk_enabled = true;
out:
	return err;
}

static void msm_sata_disable_phy_rxoob_clk(struct msm_sata_phy *phy)
{
	if (phy->is_rxoob_clk_enabled) {
		clk_disable_unprepare(phy->rxoob_clk);
		phy->is_rxoob_clk_enabled = false;
	}
}

static int msm_sata_enable_phy_ref_clk(struct msm_sata_phy *phy)
{
	int err = 0;

	if (phy->is_ref_clk_enabled)
		goto out;

	/*
	 * reference clock is propagated in a daisy-chained manner from
	 * source to phy, so ungate them at each stage.
	 */
	err = clk_prepare_enable(phy->ref_clk_src);
	if (err) {
		dev_err(phy->dev, "%s: ref_clk_src enable failed %d\n",
				__func__, err);
		goto out;
	}

	err = clk_prepare_enable(phy->ref_clk_parent);
	if (err) {
		dev_err(phy->dev, "%s: ref_clk_parent enable failed %d\n",
				__func__, err);
		goto out_disable_src;
	}

	err = clk_prepare_enable(phy->ref_clk);
	if (err) {
		dev_err(phy->dev, "%s: ref_clk enable failed %d\n",
				__func__, err);
		goto out_disable_parent;
	}

	phy->is_ref_clk_enabled = true;
	goto out;

out_disable_parent:
	clk_disable_unprepare(phy->ref_clk_parent);
out_disable_src:
	clk_disable_unprepare(phy->ref_clk_src);
out:
	return err;
}

static void msm_sata_disable_phy_ref_clk(struct msm_sata_phy *phy)
{
	if (phy->is_ref_clk_enabled) {
		clk_disable_unprepare(phy->ref_clk);
		clk_disable_unprepare(phy->ref_clk_parent);
		clk_disable_unprepare(phy->ref_clk_src);
		phy->is_ref_clk_enabled = false;
	}
}

static int msm_sata_phy_cfg_vreg(struct device *dev,
				struct msm_sata_phy_vreg *vreg, bool on)
{
	int err = 0;
	struct regulator *reg = vreg->reg;
	const char *name = vreg->name;
	int min_uV, uA_load;

	BUG_ON(!vreg);

	if (regulator_count_voltages(reg) > 0) {
		min_uV = on ? vreg->min_uV : 0;
		err = regulator_set_voltage(reg, min_uV, vreg->max_uV);
		if (err) {
			dev_err(dev, "%s: %s set voltage failed, err=%d\n",
					__func__, name, err);
			goto out;
		}

		uA_load = on ? vreg->max_uA : 0;
		err = regulator_set_optimum_mode(reg, uA_load);
		if (err >= 0) {
			/*
			 * regulator_set_optimum_mode() returns new regulator
			 * mode upon success.
			 */
			err = 0;
		} else {
			dev_err(dev, "%s: %s set optimum mode(uA_load=%d) failed, err=%d\n",
					__func__, name, uA_load, err);
			goto out;
		}
	}
out:
	return err;
}

static int msm_sata_phy_enable_vreg(struct msm_sata_phy *phy,
					struct msm_sata_phy_vreg *vreg)
{
	struct device *dev = phy->dev;
	int err = 0;

	if (!vreg || vreg->enabled)
		goto out;

	err = msm_sata_phy_cfg_vreg(dev, vreg, true);
	if (!err)
		err = regulator_enable(vreg->reg);

	if (!err)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				__func__, vreg->name, err);
out:
	return err;
}

static int msm_sata_phy_disable_vreg(struct msm_sata_phy *phy,
					struct msm_sata_phy_vreg *vreg)
{
	struct device *dev = phy->dev;
	int err = 0;

	if (!vreg || !vreg->enabled)
		goto out;

	err = regulator_disable(vreg->reg);

	if (!err) {
		/* ignore errors on applying disable config */
		msm_sata_phy_cfg_vreg(dev, vreg, false);
		vreg->enabled = false;
	} else {
		dev_err(dev, "%s: %s disable failed, err=%d\n",
				__func__, vreg->name, err);
	}
out:
	return err;
}

static int msm_sata_phy_init_vreg(struct device *dev,
		struct msm_sata_phy_vreg *vreg, const char *name)
{
	int err = 0;
	char prop_name[MAX_PROP_NAME];

	vreg->name = kstrdup(name, GFP_KERNEL);
	if (!vreg->name) {
		err = -ENOMEM;
		goto out;
	}

	vreg->reg = devm_regulator_get(dev, name);
	if (IS_ERR(vreg->reg)) {
		err = PTR_ERR(vreg->reg);
		dev_err(dev, "failed to get %s, %d\n", name, err);
		goto out;
	}

	if (dev->of_node) {
		snprintf(prop_name, MAX_PROP_NAME, "%s-max-microamp", name);
		err = of_property_read_u32(dev->of_node,
					prop_name, &vreg->max_uA);
		if (err && err != -EINVAL) {
			dev_err(dev, "%s: failed to read %s\n",
					__func__, prop_name);
			goto out;
		} else if (err == -EINVAL || !vreg->max_uA) {
			if (regulator_count_voltages(vreg->reg) > 0) {
				dev_err(dev, "%s: %s is mandatory\n",
						__func__, prop_name);
				goto out;
			}
			err = 0;
		}
	}

	if (!strcmp(name, "vdda-pll")) {
		vreg->max_uV = VDDA_PLL_MAX_UV;
		vreg->min_uV = VDDA_PLL_MIN_UV;
	} else if (!strcmp(name, "vdda-phy")) {
		vreg->max_uV = VDDA_PHY_MAX_UV;
		vreg->min_uV = VDDA_PHY_MIN_UV;
	}

out:
	if (err)
		kfree(vreg->name);
	return err;
}

static int msm_sata_phy_clk_get(struct device *dev,
		const char *name, struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		err = PTR_ERR(clk);
		dev_err(dev, "failed to get %s err %d", name, err);
	} else {
		*clk_out = clk;
	}

	return err;
}

static int msm_sata_phy_power_up(struct msm_sata_phy *phy)
{
	int err = 0;
	u32 reg;
	struct device *dev = phy->dev;

	if (phy->phy_sel) {
		/* Select SATA PHY */
		writel_relaxed(0x0, phy->phy_sel);

		/*
		 * SATA PHY must be selected before configuring the PHY.
		 * The phy_sel and phy_mmio may be in different register space
		 * and *_relaxed version doesn't ensure ordering in such case.
		 */
		mb();
	}

	/* SATA PHY powerup sequence */

	/* PWM configurations */
	writel_relaxed(0x08, phy->mmio + QSERDES_RX_PWM_CNTRL1);
	writel_relaxed(0x40, phy->mmio + QSERDES_RX_PWM_CNTRL2);

	/* Configure PHY power control to operate in mission mode */
	writel_relaxed(0x01, phy->mmio + SATA_PHY_POW_DWN_CTRL0);

	/* CDR counter selected between 30-40 */
	writel_relaxed(0x25, phy->mmio + SATA_PHY_CDR_CTRL0);

	/* Wakeup counter values are set to Maximum */
	writel_relaxed(0x0f, phy->mmio + SATA_PHY_CLK_BUF_SETTLING);
	writel_relaxed(0xff, phy->mmio + SATA_PHY_TX_DRV_WAKEUP);
	writel_relaxed(0xff, phy->mmio + SATA_PHY_SPDNEG_CFG0);
	writel_relaxed(0x25, phy->mmio + SATA_PHY_CDR_CTRL0);

	/* PLL register settings */
	writel_relaxed(0xec, phy->mmio + QSERDES_COM_PLL_CRCTRL);
	writel_relaxed(0x01, phy->mmio + QSERDES_COM_PLL_IP_SETI);
	writel_relaxed(0x3f, phy->mmio + QSERDES_COM_PLL_CP_SETI);
	writel_relaxed(0x0f, phy->mmio + QSERDES_COM_PLL_IP_SETP);
	writel_relaxed(0x13, phy->mmio + QSERDES_COM_PLL_CP_SETP);

	/* PCS settings for COMMON, TX, RX paths */
	writel_relaxed(0x5b, phy->mmio + SATA_PHY_CMN_PWR_CTRL);
	writel_relaxed(0x32, phy->mmio + SATA_PHY_TX_PWR_CTRL);
	writel_relaxed(0x83, phy->mmio + SATA_PHY_RX_PWR_CTRL);
	writel_relaxed(0x7b, phy->mmio + SATA_PHY_CMN_PWR_CTRL);

	/* Ref clk frequency select - 19.2Mhz selected */
	writel_relaxed(0x08, phy->mmio + QSERDES_COM_SYSCLK_EN_SEL);
	writel_relaxed(0x06, phy->mmio + QSERDES_COM_SYS_CLK_CTRL);

	/* Decimal and Fractional dividers configuration */
	writel_relaxed(0x9c, phy->mmio + QSERDES_COM_DEC_START1);
	writel_relaxed(0x03, phy->mmio + QSERDES_COM_DEC_START2);
	writel_relaxed(0xff, phy->mmio + QSERDES_COM_DIV_FRAC_START1);
	writel_relaxed(0xff, phy->mmio + QSERDES_COM_DIV_FRAC_START2);
	writel_relaxed(0x13, phy->mmio + QSERDES_COM_DIV_FRAC_START3);

	/* PLL configurations */
	writel_relaxed(0xff, phy->mmio + QSERDES_COM_PLLLOCK_CMP1);
	writel_relaxed(0x7c, phy->mmio + QSERDES_COM_PLLLOCK_CMP2);
	writel_relaxed(0x00, phy->mmio + QSERDES_COM_PLLLOCK_CMP3);
	writel_relaxed(0x01, phy->mmio + QSERDES_COM_PLLLOCK_CMP_EN);

	/* Other Resetsm configurations - FAST_VCO_TUNE */
	writel_relaxed(0x10, phy->mmio + QSERDES_COM_RESETSM_CNTRL);

	/* PI configurations- First Order threshold and Second order gain */
	writel_relaxed(0xeb, phy->mmio + QSERDES_RX_CDR_CONTROL);
	writel_relaxed(0x5a, phy->mmio + QSERDES_RX_CDR_CONTROL2);
	/* Config required only on reference boards with shared PHY */
	if (phy->phy_sel)
		writel_relaxed(0x1a, phy->mmio +
					QSERDES_RX_CDR_CONTROL_QUARTER);

	/* TX configurations */
	/* TX config differences between shared & dedicated PHY */
	if (phy->phy_sel)
		writel_relaxed(0x1f, phy->mmio + QSERDES_TX_TX_DRV_LVL);
	else
		writel_relaxed(0x16, phy->mmio + QSERDES_TX_TX_DRV_LVL);
	writel_relaxed(0x00, phy->mmio + QSERDES_TX_BIST_MODE_LANENO);
	writel_relaxed(0x30, phy->mmio + QSERDES_TX_TX_EMP_POST1_LVL);

	/* RX config differences between shared & dedicated PHY */
	if (!phy->phy_sel) {
		writel_relaxed(0x44, phy->mmio + QSERDES_RX_RX_EQ_GAIN12);
		writel_relaxed(0x01, phy->mmio + SATA_PHY_CDR_CTRL1);
	}

	/* SSC Configurations and Serdes start */
	writel_relaxed(0x00, phy->mmio + QSERDES_COM_SSC_EN_CENTER);
	writel_relaxed(0x31, phy->mmio + QSERDES_COM_SSC_PER1);
	writel_relaxed(0x01, phy->mmio + QSERDES_COM_SSC_PER2);
	writel_relaxed(0x01, phy->mmio + QSERDES_COM_SSC_ADJ_PER1);
	writel_relaxed(0x00, phy->mmio + QSERDES_COM_SSC_ADJ_PER2);
	writel_relaxed(0x3f, phy->mmio + QSERDES_COM_SSC_STEP_SIZE1);
	writel_relaxed(0x05, phy->mmio + QSERDES_COM_SSC_STEP_SIZE2);
	writel_relaxed(0x01, phy->mmio + QSERDES_COM_SSC_EN_CENTER);

	/*
	 * Flush all delayed writes before sleeping to ensure that PHY
	 * configuration is applied.
	 */
	mb();

	/* Sleep for 1ms before starting serdes */
	usleep(1000);

	/* Start serdes */
	writel_relaxed(0x01, phy->mmio + SATA_PHY_SERDES_START);

	/*
	 * Read RESETSM status until SERDES is ready,
	 * timeout after 1 sec
	 */
	err = readl_poll_timeout(phy->mmio + QSERDES_COM_RESET_SM, reg,
			(reg & (1 << 5)), 100, 1000000);
	if (err) {
		dev_err(dev, "%s: poll timeout QSERDES_COM_RESET_SM, status: 0x%x\n",
				__func__, readl_relaxed(phy->mmio +
					QSERDES_COM_RESET_SM));
		goto out;
	}

	/* RX configurations */
	writel_relaxed(0x5f, phy->mmio + SATA_PHY_LANE_CTRL1);
	writel_relaxed(0x43, phy->mmio + SATA_PHY_ALIGNP);

	dev_dbg(dev, "SATA PHY powered up in functional mode\n");
out:
	/* power down PHY in case of failure */
	if (err)
		writel_relaxed(0x0, phy->mmio + SATA_PHY_POW_DWN_CTRL0);

	return err;
}

static int msm_sata_phy_power_off(struct phy *generic_phy)
{
	struct msm_sata_phy *phy = phy_get_drvdata(generic_phy);

	writel_relaxed(0x0, phy->mmio + SATA_PHY_POW_DWN_CTRL0);
	/*
	 * Ensure that the PHY is power down before gating power.
	 * It is possible that PHY regulators might be turned on if
	 * other PHY's share the same regulators.
	 */
	mb();

	msm_sata_disable_phy_rxoob_clk(phy);
	msm_sata_disable_phy_ref_clk(phy);

	msm_sata_phy_disable_vreg(phy, &phy->vdda_pll);
	msm_sata_phy_disable_vreg(phy, &phy->vdda_phy);
	phy->is_powered_on = false;

	return 0;
}

static int msm_sata_phy_power_on(struct phy *generic_phy)
{
	int err;
	struct msm_sata_phy *phy = phy_get_drvdata(generic_phy);

	err = msm_sata_phy_enable_vreg(phy, &phy->vdda_phy);
	if (err)
		goto out;

	/* vdda_pll also enables ref clock LDOs so enable it first */
	err = msm_sata_phy_enable_vreg(phy, &phy->vdda_pll);
	if (err)
		goto out_disable_phy;

	err = msm_sata_enable_phy_ref_clk(phy);
	if (err)
		goto out_disable_pll;

	err = msm_sata_enable_phy_rxoob_clk(phy);
	if (err)
		goto out_disable_ref;

	err = msm_sata_phy_power_up(phy);
	if (err)
		goto out_disable_rxoob;

	phy->is_powered_on = true;
	goto out;

out_disable_rxoob:
	msm_sata_disable_phy_rxoob_clk(phy);
out_disable_ref:
	msm_sata_disable_phy_ref_clk(phy);
out_disable_pll:
	msm_sata_phy_disable_vreg(phy, &phy->vdda_pll);
out_disable_phy:
	msm_sata_phy_disable_vreg(phy, &phy->vdda_phy);
out:
	return err;
}

static int msm_sata_phy_init(struct phy *generic_phy)
{
	int err;
	struct msm_sata_phy *phy = phy_get_drvdata(generic_phy);
	struct device *dev = phy->dev;

	err = msm_sata_phy_clk_get(dev, "ref_clk_src", &phy->ref_clk_src);
	if (err)
		goto out;

	err = msm_sata_phy_clk_get(dev, "ref_clk_parent", &phy->ref_clk_parent);
	if (err)
		goto out;

	err = msm_sata_phy_clk_get(dev, "ref_clk", &phy->ref_clk);
	if (err)
		goto out;

	err = msm_sata_phy_clk_get(dev, "rxoob_clk", &phy->rxoob_clk);
	if (err)
		goto out;

	err = msm_sata_phy_init_vreg(dev, &phy->vdda_pll, "vdda-pll");
	if (err)
		goto out;

	err = msm_sata_phy_init_vreg(dev, &phy->vdda_phy, "vdda-phy");
	if (err)
		goto out;
out:
	return err;
}

static int msm_sata_phy_exit(struct phy *generic_phy)
{
	struct msm_sata_phy *phy = phy_get_drvdata(generic_phy);

	if (phy->is_powered_on)
		msm_sata_phy_power_off(generic_phy);

	return 0;
}

static struct phy_ops msm_sata_phy_ops = {
	.init		= msm_sata_phy_init,
	.exit		= msm_sata_phy_exit,
	.power_on	= msm_sata_phy_power_on,
	.power_off	= msm_sata_phy_power_off,
	.owner		= THIS_MODULE,
};

static int msm_sata_phy_probe(struct platform_device *pdev)
{
	int err = 0;
	struct msm_sata_phy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct phy_provider *phy_provider;
	struct phy *generic_phy;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		dev_err(dev, "%s: failed to allocate phy\n", __func__);
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy_sel");
	phy->phy_sel = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->phy_sel)) {
		err = PTR_ERR(phy->phy_sel);
		/* phy_sel resource is optional */
		phy->phy_sel = 0;
		dev_dbg(dev, "%s: phy select resource get failed %d\n",
				__func__, err);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy_mem");
	phy->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->mmio)) {
		err = PTR_ERR(phy->mmio);
		dev_err(dev, "%s: phy mmio get resource failed %d\n",
				__func__, err);
		goto out;
	}

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		err = PTR_ERR(phy_provider);
		dev_err(dev, "%s: failed to register phy %d\n", __func__, err);
		goto out;
	}

	generic_phy = devm_phy_create(dev, NULL, &msm_sata_phy_ops, NULL);
	if (IS_ERR(generic_phy)) {
		err =  PTR_ERR(generic_phy);
		dev_err(dev, "%s: failed to create phy %d\n", __func__, err);
		goto out;
	}

	phy->dev = dev;
	phy_set_drvdata(generic_phy, phy);

	return 0;
out:
	return err;
}

static const struct of_device_id msm_sata_phy_of_match[] = {
	{ .compatible = "qcom,sataphy" },
	{ },
};
MODULE_DEVICE_TABLE(of, msm_sata_phy_of_match);

static struct platform_driver msm_sata_phy_driver = {
	.probe	= msm_sata_phy_probe,
	.driver = {
		.name	= "msm-sata-phy",
		.owner	= THIS_MODULE,
		.of_match_table	= msm_sata_phy_of_match,
	}
};
module_platform_driver(msm_sata_phy_driver);

MODULE_DESCRIPTION("MSM 6Gbps SATA PHY driver");
MODULE_LICENSE("GPL v2");

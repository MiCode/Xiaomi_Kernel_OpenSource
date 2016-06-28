/*
 * Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/usb/phy.h>
#include <linux/usb/msm_hsusb.h>

#define QUSB2PHY_PWR_CTRL1		0x210
#define PWR_CTRL1_POWR_DOWN		BIT(0)

#define QUSB2PHY_PLL_COMMON_STATUS_ONE	0x1A0
#define CORE_READY_STATUS		BIT(0)

/* In case Efuse register shows zero, use this value */
#define TUNE2_DEFAULT_HIGH_NIBBLE	0xB
#define TUNE2_DEFAULT_LOW_NIBBLE	0x3

/* Get TUNE2's high nibble value read from efuse */
#define TUNE2_HIGH_NIBBLE_VAL(val, pos, mask)	((val >> pos) & mask)

#define QUSB2PHY_INTR_CTRL		0x22C
#define DMSE_INTR_HIGH_SEL              BIT(4)
#define DPSE_INTR_HIGH_SEL              BIT(3)
#define CHG_DET_INTR_EN                 BIT(2)
#define DMSE_INTR_EN                    BIT(1)
#define DPSE_INTR_EN                    BIT(0)

#define QUSB2PHY_INTR_STAT		0x230
#define DMSE_INTERRUPT			BIT(1)
#define DPSE_INTERRUPT			BIT(0)

#define QUSB2PHY_PORT_TUNE2		0x240

#define HS_PHY_CTRL_REG			0x10
#define UTMI_OTG_VBUS_VALID             BIT(20)
#define SW_SESSVLD_SEL                  BIT(28)

#define QUSB2PHY_1P8_VOL_MIN           1800000 /* uV */
#define QUSB2PHY_1P8_VOL_MAX           1800000 /* uV */
#define QUSB2PHY_1P8_HPM_LOAD          30000   /* uA */

#define QUSB2PHY_3P3_VOL_MIN		3075000 /* uV */
#define QUSB2PHY_3P3_VOL_MAX		3200000 /* uV */
#define QUSB2PHY_3P3_HPM_LOAD		30000	/* uA */

unsigned int phy_tune2;
module_param(phy_tune2, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(phy_tune2, "QUSB PHY v2 TUNE2");

struct qusb_phy {
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*qscratch_base;
	void __iomem		*tune2_efuse_reg;

	struct clk		*ref_clk_src;
	struct clk		*ref_clk;
	struct clk		*cfg_ahb_clk;
	struct clk		*phy_reset;

	struct regulator	*vdd;
	struct regulator	*vdda33;
	struct regulator	*vdda18;
	int			vdd_levels[3]; /* none, low, high */
	int			init_seq_len;
	int			*qusb_phy_init_seq;
	int			host_init_seq_len;
	int			*qusb_phy_host_init_seq;

	u32			tune2_val;
	int			tune2_efuse_bit_pos;
	int			tune2_efuse_num_of_bits;

	bool			power_enabled;
	bool			clocks_enabled;
	bool			cable_connected;
	bool			suspended;
	bool			rm_pulldown;

	struct regulator_desc	dpdm_rdesc;
	struct regulator_dev	*dpdm_rdev;

	/* emulation targets specific */
	void __iomem		*emu_phy_base;
	bool			emulation;
	int			*emu_init_seq;
	int			emu_init_seq_len;
	int			*phy_pll_reset_seq;
	int			phy_pll_reset_seq_len;
	int			*emu_dcm_reset_seq;
	int			emu_dcm_reset_seq_len;
};

static void qusb_phy_enable_clocks(struct qusb_phy *qphy, bool on)
{
	dev_dbg(qphy->phy.dev, "%s(): clocks_enabled:%d on:%d\n",
			__func__, qphy->clocks_enabled, on);

	if (!qphy->clocks_enabled && on) {
		clk_prepare_enable(qphy->ref_clk_src);
		clk_prepare_enable(qphy->ref_clk);
		clk_prepare_enable(qphy->cfg_ahb_clk);
		qphy->clocks_enabled = true;
	}

	if (qphy->clocks_enabled && !on) {
		clk_disable_unprepare(qphy->ref_clk);
		clk_disable_unprepare(qphy->ref_clk_src);
		clk_disable_unprepare(qphy->cfg_ahb_clk);
		qphy->clocks_enabled = false;
	}

	dev_dbg(qphy->phy.dev, "%s(): clocks_enabled:%d\n", __func__,
						qphy->clocks_enabled);
}

static int qusb_phy_config_vdd(struct qusb_phy *qphy, int high)
{
	int min, ret;

	min = high ? 1 : 0; /* low or none? */
	ret = regulator_set_voltage(qphy->vdd, qphy->vdd_levels[min],
						qphy->vdd_levels[2]);
	if (ret) {
		dev_err(qphy->phy.dev, "unable to set voltage for qusb vdd\n");
		return ret;
	}

	dev_dbg(qphy->phy.dev, "min_vol:%d max_vol:%d\n",
			qphy->vdd_levels[min], qphy->vdd_levels[2]);
	return ret;
}

static int qusb_phy_enable_power(struct qusb_phy *qphy, bool on,
						bool toggle_vdd)
{
	int ret = 0;

	dev_dbg(qphy->phy.dev, "%s turn %s regulators. power_enabled:%d\n",
			__func__, on ? "on" : "off", qphy->power_enabled);

	if (toggle_vdd && qphy->power_enabled == on) {
		dev_dbg(qphy->phy.dev, "PHYs' regulators are already ON.\n");
		return 0;
	}

	if (!on)
		goto disable_vdda33;

	if (toggle_vdd) {
		ret = qusb_phy_config_vdd(qphy, true);
		if (ret) {
			dev_err(qphy->phy.dev, "Unable to config VDD:%d\n",
								ret);
			goto err_vdd;
		}

		ret = regulator_enable(qphy->vdd);
		if (ret) {
			dev_err(qphy->phy.dev, "Unable to enable VDD\n");
			goto unconfig_vdd;
		}
	}

	ret = regulator_set_load(qphy->vdda18, QUSB2PHY_1P8_HPM_LOAD);
	if (ret < 0) {
		dev_err(qphy->phy.dev, "Unable to set HPM of vdda18:%d\n", ret);
		goto disable_vdd;
	}

	ret = regulator_set_voltage(qphy->vdda18, QUSB2PHY_1P8_VOL_MIN,
						QUSB2PHY_1P8_VOL_MAX);
	if (ret) {
		dev_err(qphy->phy.dev,
				"Unable to set voltage for vdda18:%d\n", ret);
		goto put_vdda18_lpm;
	}

	ret = regulator_enable(qphy->vdda18);
	if (ret) {
		dev_err(qphy->phy.dev, "Unable to enable vdda18:%d\n", ret);
		goto unset_vdda18;
	}

	ret = regulator_set_load(qphy->vdda33, QUSB2PHY_3P3_HPM_LOAD);
	if (ret < 0) {
		dev_err(qphy->phy.dev, "Unable to set HPM of vdda33:%d\n", ret);
		goto disable_vdda18;
	}

	ret = regulator_set_voltage(qphy->vdda33, QUSB2PHY_3P3_VOL_MIN,
						QUSB2PHY_3P3_VOL_MAX);
	if (ret) {
		dev_err(qphy->phy.dev,
				"Unable to set voltage for vdda33:%d\n", ret);
		goto put_vdda33_lpm;
	}

	ret = regulator_enable(qphy->vdda33);
	if (ret) {
		dev_err(qphy->phy.dev, "Unable to enable vdda33:%d\n", ret);
		goto unset_vdd33;
	}

	if (toggle_vdd)
		qphy->power_enabled = true;

	pr_debug("%s(): QUSB PHY's regulators are turned ON.\n", __func__);
	return ret;

disable_vdda33:
	ret = regulator_disable(qphy->vdda33);
	if (ret)
		dev_err(qphy->phy.dev, "Unable to disable vdda33:%d\n", ret);

unset_vdd33:
	ret = regulator_set_voltage(qphy->vdda33, 0, QUSB2PHY_3P3_VOL_MAX);
	if (ret)
		dev_err(qphy->phy.dev,
			"Unable to set (0) voltage for vdda33:%d\n", ret);

put_vdda33_lpm:
	ret = regulator_set_load(qphy->vdda33, 0);
	if (ret < 0)
		dev_err(qphy->phy.dev, "Unable to set (0) HPM of vdda33\n");

disable_vdda18:
	ret = regulator_disable(qphy->vdda18);
	if (ret)
		dev_err(qphy->phy.dev, "Unable to disable vdda18:%d\n", ret);

unset_vdda18:
	ret = regulator_set_voltage(qphy->vdda18, 0, QUSB2PHY_1P8_VOL_MAX);
	if (ret)
		dev_err(qphy->phy.dev,
			"Unable to set (0) voltage for vdda18:%d\n", ret);

put_vdda18_lpm:
	ret = regulator_set_load(qphy->vdda18, 0);
	if (ret < 0)
		dev_err(qphy->phy.dev, "Unable to set LPM of vdda18\n");

disable_vdd:
	if (toggle_vdd) {
		ret = regulator_disable(qphy->vdd);
		if (ret)
			dev_err(qphy->phy.dev, "Unable to disable vdd:%d\n",
								ret);

unconfig_vdd:
		ret = qusb_phy_config_vdd(qphy, false);
		if (ret)
			dev_err(qphy->phy.dev, "Unable unconfig VDD:%d\n",
								ret);
	}
err_vdd:
	if (toggle_vdd)
		qphy->power_enabled = false;
	dev_dbg(qphy->phy.dev, "QUSB PHY's regulators are turned OFF.\n");
	return ret;
}

static int qusb_phy_update_dpdm(struct usb_phy *phy, int value)
{
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);
	int ret = 0;

	dev_dbg(phy->dev, "%s value:%d rm_pulldown:%d\n",
				__func__, value, qphy->rm_pulldown);

	switch (value) {
	case POWER_SUPPLY_DP_DM_DPF_DMF:
		dev_dbg(phy->dev, "POWER_SUPPLY_DP_DM_DPF_DMF\n");
		if (!qphy->rm_pulldown) {
			ret = qusb_phy_enable_power(qphy, true, false);
			if (ret >= 0) {
				qphy->rm_pulldown = true;
				dev_dbg(phy->dev, "DP_DM_F: rm_pulldown:%d\n",
						qphy->rm_pulldown);
			}
		}

		break;

	case POWER_SUPPLY_DP_DM_DPR_DMR:
		dev_dbg(phy->dev, "POWER_SUPPLY_DP_DM_DPR_DMR\n");
		if (qphy->rm_pulldown) {
			ret = qusb_phy_enable_power(qphy, false, false);
			if (ret >= 0) {
				qphy->rm_pulldown = false;
				dev_dbg(phy->dev, "DP_DM_R: rm_pulldown:%d\n",
						qphy->rm_pulldown);
			}
		}
		break;

	default:
		ret = -EINVAL;
		dev_err(phy->dev, "Invalid power supply property(%d)\n", value);
		break;
	}

	return ret;
}

static void qusb_phy_get_tune2_param(struct qusb_phy *qphy)
{
	u8 num_of_bits;
	u32 bit_mask = 1;

	pr_debug("%s(): num_of_bits:%d bit_pos:%d\n", __func__,
				qphy->tune2_efuse_num_of_bits,
				qphy->tune2_efuse_bit_pos);

	/* get bit mask based on number of bits to use with efuse reg */
	if (qphy->tune2_efuse_num_of_bits) {
		num_of_bits = qphy->tune2_efuse_num_of_bits;
		bit_mask = (bit_mask << num_of_bits) - 1;
	}

	/*
	 * Read EFUSE register having TUNE2 parameter's high nibble.
	 * If efuse register shows value as 0x0, then use default value
	 * as 0xB as high nibble. Otherwise use efuse register based
	 * value for this purpose.
	 */
	qphy->tune2_val = readl_relaxed(qphy->tune2_efuse_reg);
	pr_debug("%s(): bit_mask:%d efuse based tune2 value:%d\n",
				__func__, bit_mask, qphy->tune2_val);

	qphy->tune2_val = TUNE2_HIGH_NIBBLE_VAL(qphy->tune2_val,
				qphy->tune2_efuse_bit_pos, bit_mask);

	if (!qphy->tune2_val)
		qphy->tune2_val = TUNE2_DEFAULT_HIGH_NIBBLE;

	/* Get TUNE2 byte value using high and low nibble value */
	qphy->tune2_val = ((qphy->tune2_val << 0x4) |
					TUNE2_DEFAULT_LOW_NIBBLE);
}

static void qusb_phy_write_seq(void __iomem *base, u32 *seq, int cnt,
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

static void qusb_phy_host_init(struct usb_phy *phy)
{
	u8 reg;
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);

	dev_dbg(phy->dev, "%s\n", __func__);

	/* Perform phy reset */
	clk_reset(qphy->phy_reset, CLK_RESET_ASSERT);
	usleep_range(100, 150);
	clk_reset(qphy->phy_reset, CLK_RESET_DEASSERT);

	qusb_phy_write_seq(qphy->base, qphy->qusb_phy_host_init_seq,
			qphy->host_init_seq_len, 0);

	/* Ensure above write is completed before turning ON ref clk */
	wmb();

	/* Require to get phy pll lock successfully */
	usleep_range(150, 160);

	reg = readb_relaxed(qphy->base + QUSB2PHY_PLL_COMMON_STATUS_ONE);
	dev_dbg(phy->dev, "QUSB2PHY_PLL_COMMON_STATUS_ONE:%x\n", reg);
	if (!(reg & CORE_READY_STATUS)) {
		dev_err(phy->dev, "QUSB PHY PLL LOCK fails:%x\n", reg);
		WARN_ON(1);
	}
}

static int qusb_phy_init(struct usb_phy *phy)
{
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);
	int ret;
	u8 reg;

	dev_dbg(phy->dev, "%s\n", __func__);

	ret = qusb_phy_enable_power(qphy, true, true);
	if (ret)
		return ret;

	qusb_phy_enable_clocks(qphy, true);

	/* Perform phy reset */
	clk_reset(qphy->phy_reset, CLK_RESET_ASSERT);
	usleep_range(100, 150);
	clk_reset(qphy->phy_reset, CLK_RESET_DEASSERT);

	if (qphy->emulation) {
		if (qphy->emu_init_seq)
			qusb_phy_write_seq(qphy->emu_phy_base,
				qphy->emu_init_seq, qphy->emu_init_seq_len, 0);

		if (qphy->qusb_phy_init_seq)
			qusb_phy_write_seq(qphy->base, qphy->qusb_phy_init_seq,
					qphy->init_seq_len, 0);

		/* Wait for 5ms as per QUSB2 RUMI sequence */
		usleep_range(5000, 7000);

		if (qphy->phy_pll_reset_seq)
			qusb_phy_write_seq(qphy->base, qphy->phy_pll_reset_seq,
					qphy->phy_pll_reset_seq_len, 10000);

		if (qphy->emu_dcm_reset_seq)
			qusb_phy_write_seq(qphy->emu_phy_base,
					qphy->emu_dcm_reset_seq,
					qphy->emu_dcm_reset_seq_len, 10000);

		return 0;
	}

	/* Disable the PHY */
	writel_relaxed(readl_relaxed(qphy->base + QUSB2PHY_PWR_CTRL1) |
			PWR_CTRL1_POWR_DOWN,
			qphy->base + QUSB2PHY_PWR_CTRL1);

	if (qphy->qusb_phy_init_seq)
		qusb_phy_write_seq(qphy->base, qphy->qusb_phy_init_seq,
				qphy->init_seq_len, 0);
	/*
	 * Check for EFUSE value only if tune2_efuse_reg is available
	 * and try to read EFUSE value only once i.e. not every USB
	 * cable connect case.
	 */
	if (qphy->tune2_efuse_reg) {
		if (!qphy->tune2_val)
			qusb_phy_get_tune2_param(qphy);

		pr_debug("%s(): Programming TUNE2 parameter as:%x\n", __func__,
				qphy->tune2_val);
		writel_relaxed(qphy->tune2_val,
				qphy->base + QUSB2PHY_PORT_TUNE2);
	}

	/* If phy_tune2 modparam set, override tune2 value */
	if (phy_tune2) {
		pr_debug("%s(): (modparam) TUNE2 val:0x%02x\n",
						__func__, phy_tune2);
		writel_relaxed(phy_tune2,
				qphy->base + QUSB2PHY_PORT_TUNE2);
	}

	/* ensure above writes are completed before re-enabling PHY */
	wmb();

	/* Enable the PHY */
	writel_relaxed(readl_relaxed(qphy->base + QUSB2PHY_PWR_CTRL1) &
			~PWR_CTRL1_POWR_DOWN,
			qphy->base + QUSB2PHY_PWR_CTRL1);

	/* Ensure above write is completed before turning ON ref clk */
	wmb();

	/* Require to get phy pll lock successfully */
	usleep_range(150, 160);

	reg = readb_relaxed(qphy->base + QUSB2PHY_PLL_COMMON_STATUS_ONE);
	dev_dbg(phy->dev, "QUSB2PHY_PLL_COMMON_STATUS_ONE:%x\n", reg);
	if (!(reg & CORE_READY_STATUS)) {
		dev_err(phy->dev, "QUSB PHY PLL LOCK fails:%x\n", reg);
		WARN_ON(1);
	}
	return 0;
}

static void qusb_phy_shutdown(struct usb_phy *phy)
{
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);

	dev_dbg(phy->dev, "%s\n", __func__);

	qusb_phy_enable_clocks(qphy, true);

	/* Disable the PHY */
	writel_relaxed(readl_relaxed(qphy->base + QUSB2PHY_PWR_CTRL1) |
			PWR_CTRL1_POWR_DOWN,
			qphy->base + QUSB2PHY_PWR_CTRL1);

	/* Makes sure that above write goes through */
	wmb();

	qusb_phy_enable_clocks(qphy, false);
}
/**
 * Performs QUSB2 PHY suspend/resume functionality.
 *
 * @uphy - usb phy pointer.
 * @suspend - to enable suspend or not. 1 - suspend, 0 - resume
 *
 */
static int qusb_phy_set_suspend(struct usb_phy *phy, int suspend)
{
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);
	u32 linestate = 0, intr_mask = 0;

	if (qphy->suspended && suspend) {
		dev_dbg(phy->dev, "%s: USB PHY is already suspended\n",
			__func__);
		return 0;
	}

	if (suspend) {
		/* Bus suspend case */
		if (qphy->cable_connected ||
			(qphy->phy.flags & PHY_HOST_MODE)) {
			/* Disable all interrupts */
			writel_relaxed(0x00,
				qphy->base + QUSB2PHY_INTR_CTRL);

			linestate = readl_relaxed(qphy->base +
				QUSB2PHY_INTR_STAT);
			/*
			 * D+/D- interrupts are level-triggered, but we are
			 * only interested if the line state changes, so enable
			 * the high/low trigger based on current state. In
			 * other words, enable the triggers _opposite_ of what
			 * the current D+/D- levels are.
			 * e.g. if currently D+ high, D- low (HS 'J'/Suspend),
			 * configure the mask to trigger on D+ low OR D- high
			 */
			intr_mask = DMSE_INTERRUPT | DPSE_INTERRUPT;
			if (!(linestate & DPSE_INTR_EN)) /* D+ low */
				intr_mask |= DPSE_INTR_HIGH_SEL;
			if (!(linestate & DMSE_INTR_EN)) /* D- low */
				intr_mask |= DMSE_INTR_HIGH_SEL;

			writel_relaxed(intr_mask,
				qphy->base + QUSB2PHY_INTR_CTRL);

			/* Makes sure that above write goes through */
			wmb();
			qusb_phy_enable_clocks(qphy, false);
		} else { /* Cable disconnect case */
			/* Disable all interrupts */
			writel_relaxed(0x00,
				qphy->base + QUSB2PHY_INTR_CTRL);

			/* Put PHY into non-driving mode */
			writel_relaxed(0x23,
				qphy->base + QUSB2PHY_PWR_CTRL1);

			/* Makes sure that above write goes through */
			wmb();

			qusb_phy_enable_clocks(qphy, false);
			qusb_phy_enable_power(qphy, false, true);
		}
		qphy->suspended = true;
	} else {
		/* Bus resume case */
		if (qphy->cable_connected ||
			(qphy->phy.flags & PHY_HOST_MODE)) {
			qusb_phy_enable_clocks(qphy, true);
			/* Clear all interrupts on resume */
			writel_relaxed(0x00,
				qphy->base + QUSB2PHY_INTR_CTRL);

			/* Makes sure that above write goes through */
			wmb();
		} else { /* Cable connect case */
			qusb_phy_enable_power(qphy, true, true);
			qusb_phy_enable_clocks(qphy, true);
		}
		qphy->suspended = false;
	}

	return 0;
}

static void qusb_write_readback(void *base, u32 offset,
					const u32 mask, u32 val)
{
	u32 write_val, tmp = readl_relaxed(base + offset);

	tmp &= ~mask; /* retain other bits */
	write_val = tmp | val;

	writel_relaxed(write_val, base + offset);

	/* Read back to see if val was written */
	tmp = readl_relaxed(base + offset);
	tmp &= mask; /* clear other bits */

	if (tmp != val)
		pr_err("%s: write: %x to QSCRATCH: %x FAILED\n",
			__func__, val, offset);
}

static int qusb_phy_notify_connect(struct usb_phy *phy,
					enum usb_device_speed speed)
{
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);

	qphy->cable_connected = true;

	dev_dbg(phy->dev, " cable_connected=%d\n", qphy->cable_connected);

	if (qphy->qusb_phy_host_init_seq && qphy->phy.flags & PHY_HOST_MODE)
		qusb_phy_host_init(phy);

	/* Set OTG VBUS Valid from HSPHY to controller */
	qusb_write_readback(qphy->qscratch_base, HS_PHY_CTRL_REG,
				UTMI_OTG_VBUS_VALID,
				UTMI_OTG_VBUS_VALID);

	/* Indicate value is driven by UTMI_OTG_VBUS_VALID bit */
	qusb_write_readback(qphy->qscratch_base, HS_PHY_CTRL_REG,
				SW_SESSVLD_SEL, SW_SESSVLD_SEL);

	dev_dbg(phy->dev, "QUSB2 phy connect notification\n");
	return 0;
}

static int qusb_phy_notify_disconnect(struct usb_phy *phy,
					enum usb_device_speed speed)
{
	struct qusb_phy *qphy = container_of(phy, struct qusb_phy, phy);

	qphy->cable_connected = false;

	dev_dbg(phy->dev, " cable_connected=%d\n", qphy->cable_connected);

	/* Set OTG VBUS Valid from HSPHY to controller */
	qusb_write_readback(qphy->qscratch_base, HS_PHY_CTRL_REG,
				UTMI_OTG_VBUS_VALID, 0);

	/* Indicate value is driven by UTMI_OTG_VBUS_VALID bit */
	qusb_write_readback(qphy->qscratch_base, HS_PHY_CTRL_REG,
				SW_SESSVLD_SEL, 0);

	dev_dbg(phy->dev, "QUSB2 phy disconnect notification\n");
	return 0;
}

static int qusb_phy_dpdm_regulator_enable(struct regulator_dev *rdev)
{
	struct qusb_phy *qphy = rdev_get_drvdata(rdev);

	dev_dbg(qphy->phy.dev, "%s\n", __func__);
	return qusb_phy_update_dpdm(&qphy->phy, POWER_SUPPLY_DP_DM_DPF_DMF);
}

static int qusb_phy_dpdm_regulator_disable(struct regulator_dev *rdev)
{
	struct qusb_phy *qphy = rdev_get_drvdata(rdev);

	dev_dbg(qphy->phy.dev, "%s\n", __func__);
	return qusb_phy_update_dpdm(&qphy->phy, POWER_SUPPLY_DP_DM_DPR_DMR);
}

static int qusb_phy_dpdm_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qusb_phy *qphy = rdev_get_drvdata(rdev);

	dev_dbg(qphy->phy.dev, "%s qphy->rm_pulldown = %d\n", __func__,
					qphy->rm_pulldown);
	return qphy->rm_pulldown;
}

static struct regulator_ops qusb_phy_dpdm_regulator_ops = {
	.enable		= qusb_phy_dpdm_regulator_enable,
	.disable	= qusb_phy_dpdm_regulator_disable,
	.is_enabled	= qusb_phy_dpdm_regulator_is_enabled,
};

static int qusb_phy_regulator_init(struct qusb_phy *qphy)
{
	struct device *dev = qphy->phy.dev;
	struct regulator_config cfg = {};
	struct regulator_init_data *init_data;

	init_data = devm_kzalloc(dev, sizeof(*init_data), GFP_KERNEL);
	if (!init_data)
		return -ENOMEM;

	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS;
	qphy->dpdm_rdesc.owner = THIS_MODULE;
	qphy->dpdm_rdesc.type = REGULATOR_VOLTAGE;
	qphy->dpdm_rdesc.ops = &qusb_phy_dpdm_regulator_ops;
	qphy->dpdm_rdesc.name = kbasename(dev->of_node->full_name);

	cfg.dev = dev;
	cfg.init_data = init_data;
	cfg.driver_data = qphy;
	cfg.of_node = dev->of_node;

	qphy->dpdm_rdev = devm_regulator_register(dev, &qphy->dpdm_rdesc, &cfg);
	if (IS_ERR(qphy->dpdm_rdev))
		return PTR_ERR(qphy->dpdm_rdev);

	return 0;
}

static int qusb_phy_probe(struct platform_device *pdev)
{
	struct qusb_phy *qphy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0, size = 0;

	qphy = devm_kzalloc(dev, sizeof(*qphy), GFP_KERNEL);
	if (!qphy)
		return -ENOMEM;

	qphy->phy.dev = dev;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"qusb_phy_base");
	qphy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(qphy->base))
		return PTR_ERR(qphy->base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"qscratch_base");
	if (res) {
		qphy->qscratch_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(qphy->qscratch_base)) {
			dev_dbg(dev, "couldn't ioremap qscratch_base\n");
			qphy->qscratch_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"emu_phy_base");
	if (res) {
		qphy->emu_phy_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(qphy->emu_phy_base)) {
			dev_dbg(dev, "couldn't ioremap emu_phy_base\n");
			qphy->emu_phy_base = NULL;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"tune2_efuse_addr");
	if (res) {
		qphy->tune2_efuse_reg = devm_ioremap_nocache(dev, res->start,
							resource_size(res));
		if (!IS_ERR_OR_NULL(qphy->tune2_efuse_reg)) {
			ret = of_property_read_u32(dev->of_node,
					"qcom,tune2-efuse-bit-pos",
					&qphy->tune2_efuse_bit_pos);
			if (!ret) {
				ret = of_property_read_u32(dev->of_node,
						"qcom,tune2-efuse-num-bits",
						&qphy->tune2_efuse_num_of_bits);
			}

			if (ret) {
				dev_err(dev,
				"DT Value for tune2 efuse is invalid.\n");
				return -EINVAL;
			}
		}
	}

	qphy->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(qphy->ref_clk_src))
		dev_dbg(dev, "clk get failed for ref_clk_src\n");

	qphy->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(qphy->ref_clk))
		dev_dbg(dev, "clk get failed for ref_clk\n");
	else
		clk_set_rate(qphy->ref_clk, 19200000);

	qphy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb_clk");
	if (IS_ERR(qphy->cfg_ahb_clk))
		return PTR_ERR(qphy->cfg_ahb_clk);

	qphy->phy_reset = devm_clk_get(dev, "phy_reset");
	if (IS_ERR(qphy->phy_reset))
		return PTR_ERR(qphy->phy_reset);

	qphy->emulation = of_property_read_bool(dev->of_node,
					"qcom,emulation");

	of_get_property(dev->of_node, "qcom,emu-init-seq", &size);
	if (size) {
		qphy->emu_init_seq = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (qphy->emu_init_seq) {
			qphy->emu_init_seq_len =
				(size / sizeof(*qphy->emu_init_seq));
			if (qphy->emu_init_seq_len % 2) {
				dev_err(dev, "invalid emu_init_seq_len\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,qemu-init-seq",
				qphy->emu_init_seq,
				qphy->emu_init_seq_len);
		} else {
			dev_dbg(dev,
			"error allocating memory for emu_init_seq\n");
		}
	}

	of_get_property(dev->of_node, "qcom,phy-pll-reset-seq", &size);
	if (size) {
		qphy->phy_pll_reset_seq = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (qphy->phy_pll_reset_seq) {
			qphy->phy_pll_reset_seq_len =
				(size / sizeof(*qphy->phy_pll_reset_seq));
			if (qphy->phy_pll_reset_seq_len % 2) {
				dev_err(dev, "invalid phy_pll_reset_seq_len\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,phy-pll-reset-seq",
				qphy->phy_pll_reset_seq,
				qphy->phy_pll_reset_seq_len);
		} else {
			dev_dbg(dev,
			"error allocating memory for phy_pll_reset_seq\n");
		}
	}

	of_get_property(dev->of_node, "qcom,emu-dcm-reset-seq", &size);
	if (size) {
		qphy->emu_dcm_reset_seq = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (qphy->emu_dcm_reset_seq) {
			qphy->emu_dcm_reset_seq_len =
				(size / sizeof(*qphy->emu_dcm_reset_seq));
			if (qphy->emu_dcm_reset_seq_len % 2) {
				dev_err(dev, "invalid emu_dcm_reset_seq_len\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,emu-dcm-reset-seq",
				qphy->emu_dcm_reset_seq,
				qphy->emu_dcm_reset_seq_len);
		} else {
			dev_dbg(dev,
			"error allocating memory for emu_dcm_reset_seq\n");
		}
	}

	of_get_property(dev->of_node, "qcom,qusb-phy-init-seq", &size);
	if (size) {
		qphy->qusb_phy_init_seq = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (qphy->qusb_phy_init_seq) {
			qphy->init_seq_len =
				(size / sizeof(*qphy->qusb_phy_init_seq));
			if (qphy->init_seq_len % 2) {
				dev_err(dev, "invalid init_seq_len\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,qusb-phy-init-seq",
				qphy->qusb_phy_init_seq,
				qphy->init_seq_len);
		} else {
			dev_err(dev,
			"error allocating memory for phy_init_seq\n");
		}
	}

	qphy->host_init_seq_len = of_property_count_elems_of_size(dev->of_node,
				"qcom,qusb-phy-host-init-seq",
				sizeof(*qphy->qusb_phy_host_init_seq));
	if (qphy->host_init_seq_len > 0) {
		qphy->qusb_phy_host_init_seq = devm_kcalloc(dev,
					qphy->host_init_seq_len,
					sizeof(*qphy->qusb_phy_host_init_seq),
					GFP_KERNEL);
		if (qphy->qusb_phy_host_init_seq)
			of_property_read_u32_array(dev->of_node,
				"qcom,qusb-phy-host-init-seq",
				qphy->qusb_phy_host_init_seq,
				qphy->host_init_seq_len);
		else
			return -ENOMEM;
	}

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) qphy->vdd_levels,
					 ARRAY_SIZE(qphy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		return ret;
	}

	qphy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(qphy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		return PTR_ERR(qphy->vdd);
	}

	qphy->vdda33 = devm_regulator_get(dev, "vdda33");
	if (IS_ERR(qphy->vdda33)) {
		dev_err(dev, "unable to get vdda33 supply\n");
		return PTR_ERR(qphy->vdda33);
	}

	qphy->vdda18 = devm_regulator_get(dev, "vdda18");
	if (IS_ERR(qphy->vdda18)) {
		dev_err(dev, "unable to get vdda18 supply\n");
		return PTR_ERR(qphy->vdda18);
	}

	platform_set_drvdata(pdev, qphy);

	qphy->phy.label			= "msm-qusb-phy-v2";
	qphy->phy.init			= qusb_phy_init;
	qphy->phy.set_suspend           = qusb_phy_set_suspend;
	qphy->phy.shutdown		= qusb_phy_shutdown;
	qphy->phy.type			= USB_PHY_TYPE_USB2;

	if (qphy->qscratch_base) {
		qphy->phy.notify_connect        = qusb_phy_notify_connect;
		qphy->phy.notify_disconnect     = qusb_phy_notify_disconnect;
	}

	ret = usb_add_phy_dev(&qphy->phy);
	if (ret)
		return ret;

	ret = qusb_phy_regulator_init(qphy);
	if (ret)
		usb_remove_phy(&qphy->phy);

	return ret;
}

static int qusb_phy_remove(struct platform_device *pdev)
{
	struct qusb_phy *qphy = platform_get_drvdata(pdev);

	usb_remove_phy(&qphy->phy);

	if (qphy->clocks_enabled) {
		clk_disable_unprepare(qphy->cfg_ahb_clk);
		clk_disable_unprepare(qphy->ref_clk);
		clk_disable_unprepare(qphy->ref_clk_src);
		qphy->clocks_enabled = false;
	}

	qusb_phy_enable_power(qphy, false, true);

	return 0;
}

static const struct of_device_id qusb_phy_id_table[] = {
	{ .compatible = "qcom,qusb2phy-v2", },
	{ },
};
MODULE_DEVICE_TABLE(of, qusb_phy_id_table);

static struct platform_driver qusb_phy_driver = {
	.probe		= qusb_phy_probe,
	.remove		= qusb_phy_remove,
	.driver = {
		.name	= "msm-qusb-phy-v2",
		.of_match_table = of_match_ptr(qusb_phy_id_table),
	},
};

module_platform_driver(qusb_phy_driver);

MODULE_DESCRIPTION("MSM QUSB2 PHY v2 driver");
MODULE_LICENSE("GPL v2");

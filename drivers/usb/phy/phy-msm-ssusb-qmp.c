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
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>

#define USB_SSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_SSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_SSPHY_1P8_HPM_LOAD		23000	/* uA */

/* QMP PHY register offsets */
#define PCIE_USB3_PHY_SW_RESET			0x600
#define PCIE_USB3_PHY_POWER_DOWN_CONTROL	0x604
#define PCIE_USB3_PHY_START			0x608
#define PCIE_USB3_PHY_AUTONOMOUS_MODE_CTRL	0x6BC
#define PCIE_USB3_PHY_LFPS_RXTERM_IRQ_CLEAR	0x6C0

#define PCIE_USB3_PHY_PCS_STATUS		0x728
#define PHYSTATUS				BIT(6)

#define PCIE_USB3_PHY_REVISION_ID0		0x730
#define PCIE_USB3_PHY_REVISION_ID1		0x734
#define PCIE_USB3_PHY_REVISION_ID2		0x738
#define PCIE_USB3_PHY_REVISION_ID3		0x73C

/* AHB2PHY register offsets */
#define PERIPH_SS_AHB2PHY_TOP_CFG		0x10

#define INIT_MAX_TIME_USEC			1000

/* PCIE_USB3_PHY_AUTONOMOUS_MODE_CTRL bits */
#define ARCVR_DTCT_EN		BIT(0)
#define ALFPS_DTCT_EN		BIT(1)
#define ARCVR_DTCT_EVENT_SEL	BIT(4)

struct qmp_reg_val {
	u32 offset;
	u32 val;
};

/* Use these offsets/values if PCIE_USB3_PHY_REVISION_ID0 == 0 */
static const struct qmp_reg_val qmp_settings_rev0[] = {
	{0x48, 0x08}, /* QSERDES_COM_SYSCLK_EN_SEL_TXBAND */
	{0xA4, 0x82}, /* QSERDES_COM_DEC_START1 */
	{0x104, 0x03}, /* QSERDES_COM_DEC_START2 */
	{0xF8, 0xD5}, /* QSERDES_COM_DIV_FRAC_START1 */
	{0xFC, 0xAA}, /* QSERDES_COM_DIV_FRAC_START2 */
	{0x100, 0x4D}, /* QSERDES_COM_DIV_FRAC_START3 */
	{0x94, 0x11}, /* QSERDES_COM_PLLLOCK_CMP_EN */
	{0x88, 0x2B}, /* QSERDES_COM_PLLLOCK_CMP1 */
	{0x8C, 0x68}, /* QSERDES_COM_PLLLOCK_CMP2 */
	{0x10C, 0x7C}, /* QSERDES_COM_PLL_CRCTRL */
	{0x34, 0x07}, /* QSERDES_COM_PLL_CP_SETI */
	{0x38, 0x1F}, /* QSERDES_COM_PLL_IP_SETP */
	{0x3C, 0x0F}, /* QSERDES_COM_PLL_CP_SETP */
	{0x24, 0x01}, /* QSERDES_COM_PLL_IP_SETI */
	{0x0C, 0x0F}, /* QSERDES_COM_IE_TRIM */
	{0x10, 0x0F}, /* QSERDES_COM_IP_TRIM */
	{0x14, 0x46}, /* QSERDES_COM_PLL_CNTRL */

	/* CDR Settings */
	{0x400, 0xDA}, /* QSERDES_RX_CDR_CONTROL1 */
	{0x404, 0x42}, /* QSERDES_RX_CDR_CONTROL2 */
	{0x41c, 0x75}, /* QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE */

	/* Calibration Settings */
	{0x4C, 0x90}, /* QSERDES_COM_RESETSM_CNTRL */
	{0x50, 0x05}, /* QSERDES_COM_RESETSM_CNTRL2 */

	{0xD8, 0x20}, /* QSERDES_COM_RES_CODE_START_SEG1 */
	{0xE0, 0x77}, /* QSERDES_COM_RES_CODE_CAL_CSR */
	{0xE8, 0x15}, /* QSERDES_COM_RES_TRIM_CONTROL */
	{0x268, 0x02}, /* QSERDES_TX_RCV_DETECT_LVL */
	{0x4F0, 0x67}, /* QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1 */
	{0x4F4, 0x80}, /* QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2 */
	{0x4BC, 0x06}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2 */
	{0x4C0, 0x6C}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3 */
	{0x4C4, 0xA7}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4 */
	{0x4F8, 0x40}, /* QSERDES_RX_SIGDET_ENABLES */
	{0x500, 0x73}, /* QSERDES_RX_SIGDET_CNTRL */
	{0x504, 0x06}, /* QSERDES_RX_SIGDET_DEGLITCH_CNTRL */

	{0x64C, 0x48}, /* PCIE_USB3_PHY_RX_IDLE_DTCT_CNTRL */
	{0xAC, 0x01}, /* QSERDES_COM_SSC_EN_CENTER */
	{0xB0, 0x02}, /* QSERDES_COM_SSC_ADJ_PER1 */
	{0xB8, 0x31}, /* QSERDES_COM_SSC_PER1 */
	{0xBC, 0x01}, /* QSERDES_COM_SSC_PER2 */
	{0xC0, 0x19}, /* QSERDES_COM_SSC_STEP_SIZE1 */
	{0xC4, 0x19}, /* QSERDES_COM_SSC_STEP_SIZE2 */
	{0x654, 0x08}, /* PCIE_USB3_PHY_POWER_STATE_CONFIG2 */
	{0x65C, 0xE5}, /* PCIE_USB3_PHY_RCVR_DTCT_DLY_P1U2_L */
	{0x660, 0x03}, /* PCIE_USB3_PHY_RCVR_DTCT_DLY_P1U2_H */
	{0x6A0, 0x13}, /* PCIE_USB3_PHY_RXEQTRAINING_RUN_TIME */
	{0x66C, 0xFF}, /* PCIE_USB3_PHY_LOCK_DETECT_CONFIG1 */
	{0x674, 0x17}, /* PCIE_USB3_PHY_LOCK_DETECT_CONFIG3 */
	{0x6AC, 0x05}, /* PCIE_USB3_PHY_FLL_CNTRL2 */

	{-1, -1} /* terminating entry */
};

/*
 * Use these offsets/values if PCIE_USB3_PHY_REVISION_ID0 == 1
 * QSERDES_COM registers between 0x58 and 0x14C been moved (added) 8 bytes
 */
static const struct qmp_reg_val qmp_settings_rev1[] = {
	{0x48, 0x08}, /* QSERDES_COM_SYSCLK_EN_SEL_TXBAND */
	{0xAC, 0x82}, /* QSERDES_COM_DEC_START1 */
	{0x10C, 0x03}, /* QSERDES_COM_DEC_START2 */
	{0x100, 0xD5}, /* QSERDES_COM_DIV_FRAC_START1 */
	{0x104, 0xAA}, /* QSERDES_COM_DIV_FRAC_START2 */
	{0x108, 0x4D}, /* QSERDES_COM_DIV_FRAC_START3 */
	{0x9C, 0x11}, /* QSERDES_COM_PLLLOCK_CMP_EN */
	{0x90, 0x2B}, /* QSERDES_COM_PLLLOCK_CMP1 */
	{0x94, 0x68}, /* QSERDES_COM_PLLLOCK_CMP2 */
	{0x114, 0x7C}, /* QSERDES_COM_PLL_CRCTRL */
	{0x34, 0x1F}, /* QSERDES_COM_PLL_CP_SETI */
	{0x38, 0x12}, /* QSERDES_COM_PLL_IP_SETP */
	{0x3C, 0x0F}, /* QSERDES_COM_PLL_CP_SETP */
	{0x24, 0x01}, /* QSERDES_COM_PLL_IP_SETI */
	{0x0C, 0x0F}, /* QSERDES_COM_IE_TRIM */
	{0x10, 0x0F}, /* QSERDES_COM_IP_TRIM */
	{0x14, 0x46}, /* QSERDES_COM_PLL_CNTRL */

	/* CDR Settings */
	{0x41C, 0x75}, /* QSERDEX_RX_UCDR_SO_SATURATION_AND_ENABLE */

	/* Calibration Settings */
	{0x4C, 0x90}, /* QSERDES_COM_RESETSM_CNTRL */
	{0x50, 0x07}, /* QSERDES_COM_RESETSM_CNTRL2 */
	{0x04, 0xE1}, /* QSERDES_COM_PLL_VCOTAIL_EN */

	{0xE0, 0x24}, /* QSERDES_COM_RES_CODE_START_SEG1 */
	{0xE8, 0x77}, /* QSERDES_COM_RES_CODE_CAL_CSR */
	{0xF0, 0x15}, /* QSERDES_COM_RES_TRIM_CONTROL */
	{0x268, 0x02}, /* QSERDES_TX_RCV_DETECT_LVL */
	{0x4F0, 0x67}, /* QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1 */
	{0x4F4, 0x80}, /* QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2 */
	{0x4BC, 0x06}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2 */
	{0x4C0, 0x6C}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3 */
	{0x4C4, 0xA7}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4 */
	{0x4F8, 0x40}, /* QSERDES_RX_SIGDET_ENABLES */
	{0x500, 0x73}, /* QSERDES_RX_SIGDET_CNTRL */
	{0x504, 0x06}, /* QSERDES_RX_SIGDET_DEGLITCH_CNTRL */
	{0x64C, 0x48}, /* PCIE_USB3_PHY_RX_IDLE_DTCT_CNTRL */
	{0xB4, 0x01}, /* QSERDES_COM_SSC_EN_CENTER */
	{0xB8, 0x02}, /* QSERDES_COM_SSC_ADJ_PER1 */
	{0xC0, 0x31}, /* QSERDES_COM_SSC_PER1 */
	{0xC4, 0x01}, /* QSERDES_COM_SSC_PER2 */
	{0xC8, 0x19}, /* QSERDES_COM_SSC_STEP_SIZE1 */
	{0xCC, 0x19}, /* QSERDES_COM_SSC_STEP_SIZE2 */
	{0x654, 0x08}, /* PCIE_USB3_PHY_POWER_STATE_CONFIG2 */
	{0x65C, 0xE5}, /* PCIE_USB3_PHY_RCVR_DTCT_DLY_P1U2_L */
	{0x660, 0x03}, /* PCIE_USB3_PHY_RCVR_DTCT_DLY_P1U2_H */
	{0x6A0, 0x13}, /* PCIE_USB3_PHY_RXEQTRAINING_RUN_TIME */
	{0x66C, 0xFF}, /* PCIE_USB3_PHY_LOCK_DETECT_CONFIG1 */
	{0x674, 0x17}, /* PCIE_USB3_PHY_LOCK_DETECT_CONFIG3 */
	{0x6AC, 0x05}, /* PCIE_USB3_PHY_FLL_CNTRL2 */

	{-1, -1} /* terminating entry */
};

/* Override PLL Calibration */
static const struct qmp_reg_val qmp_override_pll[] = {
	{0x04, 0xE1}, /* QSERDES_COM_PLL_VCOTAIL_EN */
	{0x50, 0x07}, /* QSERDES_COM_RESETSM_CNTRL2 */
	{-1, -1} /* terminating entry */
};

/* Foundry specific settings */
static const struct qmp_reg_val qmp_settings_rev0_misc[] = {
	{0x10C, 0x37}, /* QSERDES_COM_PLL_CRCTRL */
	{0x34, 0x04}, /* QSERDES_COM_PLL_CP_SETI */
	{0x38, 0x32}, /* QSERDES_COM_PLL_IP_SETP */
	{0x3C, 0x05}, /* QSERDES_COM_PLL_CP_SETP */
	{0x500, 0xF7}, /* QSERDES_RX_SIGDET_CNTRL */
	{0x4A8, 0xFF}, /* QSERDES_RX_RX_EQ_GAIN1_LSB */
	{0x6B0, 0xF4}, /* PCIE_USB3_PHY_FLL_CNT_VAL_L */
	{0x6B4, 0x41}, /* PCIE_USB3_PHY_FLL_CNT_VAL_H_TOL */
	{-1, -1} /* terminating entry */
};

/* Vbg related settings */
static const struct qmp_reg_val qmp_settings_rev1_misc[] = {
	{0x0C, 0x03}, /* QSERDES_COM_IE_TRIM */
	{0x10, 0x00}, /* QSERDES_COM_IP_TRIM */
	{0xA0, 0xFF}, /* QSERDES_COM_BGTC */
	{-1, -1} /* terminating entry */
};

struct msm_ssphy_qmp {
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*ahb2phy;
	struct regulator	*vdd;
	struct regulator	*vdda18;
	int			vdd_levels[3]; /* none, low, high */
	struct clk		*ldo_clk;
	struct clk		*aux_clk;
	struct clk		*cfg_ahb_clk;
	struct clk		*pipe_clk;
	struct clk		*phy_com_reset;
	struct clk		*phy_reset;
	struct clk		*phy_phy_reset;
	bool			clk_enabled;
	bool			cable_connected;
	bool			in_suspend;
	bool			override_pll_cal;
	bool			switch_pipe_clk_src;
	bool			emulation;
	bool			misc_config;
};

static inline char *get_cable_status_str(struct msm_ssphy_qmp *phy)
{
	return phy->cable_connected ? "connected" : "disconnected";
}

static void msm_ssusb_qmp_enable_autonomous(struct msm_ssphy_qmp *phy)
{
	u8 val;

	dev_dbg(phy->phy.dev, "enabling QMP autonomous mode with cable %s\n",
			get_cable_status_str(phy));

	/* clear LFPS RXTERM interrupt */
	writeb_relaxed(1, phy->base + PCIE_USB3_PHY_LFPS_RXTERM_IRQ_CLEAR);
	/* flush the previous write before next write */
	wmb();
	writeb_relaxed(0, phy->base + PCIE_USB3_PHY_LFPS_RXTERM_IRQ_CLEAR);

	val = readb_relaxed(phy->base + PCIE_USB3_PHY_AUTONOMOUS_MODE_CTRL);

	val |= ARCVR_DTCT_EN;
	if (phy->cable_connected) {
		val |= ALFPS_DTCT_EN;
		/* Detect detach */
		val &= ~ARCVR_DTCT_EVENT_SEL;
	} else {
		val &= ~ALFPS_DTCT_EN;
		/* Detect attach */
		val |= ARCVR_DTCT_EVENT_SEL;
	}

	writeb_relaxed(val, phy->base + PCIE_USB3_PHY_AUTONOMOUS_MODE_CTRL);
}


static int msm_ssusb_qmp_config_vdd(struct msm_ssphy_qmp *phy, int high)
{
	int min, ret;

	min = high ? 1 : 0; /* low or none? */
	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[min],
				    phy->vdd_levels[2]);
	if (ret) {
		dev_err(phy->phy.dev, "unable to set voltage for ssusb vdd\n");
		return ret;
	}

	dev_dbg(phy->phy.dev, "min_vol:%d max_vol:%d\n",
		phy->vdd_levels[min], phy->vdd_levels[2]);
	return ret;
}

static int msm_ssusb_qmp_ldo_enable(struct msm_ssphy_qmp *phy, int on)
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

static int msm_ssphy_qmp_init_clocks(struct msm_ssphy_qmp *phy)
{
	int ret = 0;

	phy->aux_clk = devm_clk_get(phy->phy.dev, "aux_clk");
	if (IS_ERR(phy->aux_clk)) {
		dev_err(phy->phy.dev, "failed to get aux_clk\n");
		ret = PTR_ERR(phy->aux_clk);
		return ret;
	}
	clk_set_rate(phy->aux_clk, clk_round_rate(phy->aux_clk, ULONG_MAX));
	clk_prepare_enable(phy->aux_clk);

	phy->cfg_ahb_clk = devm_clk_get(phy->phy.dev, "cfg_ahb_clk");
	if (IS_ERR(phy->cfg_ahb_clk)) {
		dev_err(phy->phy.dev, "failed to get cfg_ahb_clk\n");
		ret = PTR_ERR(phy->cfg_ahb_clk);
		goto disable_aux_clk;
	}
	clk_prepare_enable(phy->cfg_ahb_clk);

	phy->pipe_clk = devm_clk_get(phy->phy.dev, "pipe_clk");
	if (IS_ERR(phy->pipe_clk)) {
		dev_err(phy->phy.dev, "failed to get pipe_clk\n");
		ret = PTR_ERR(phy->pipe_clk);
		goto disable_cfg_ahb_clk;
	}

	if (phy->switch_pipe_clk_src) {
		/*
		 * Before PHY is initilized we must first use the xo clock
		 * as the source clock for the gcc_usb3_pipe_clk in 19.2MHz
		 * After PHY initilization we will set the rate again to 125MHz.
		 */
		clk_set_rate(phy->pipe_clk, 19200000);
		clk_prepare_enable(phy->pipe_clk);
	} /* otherwise pipe_clk must be enabled after initialization */

	phy->phy_com_reset = devm_clk_get(phy->phy.dev, "phy_com_reset");
	if (IS_ERR(phy->phy_com_reset)) {
		phy->phy_com_reset = NULL;
		dev_dbg(phy->phy.dev, "failed to get phy_com_reset\n");
	}

	phy->phy_reset = devm_clk_get(phy->phy.dev, "phy_reset");
	if (IS_ERR(phy->phy_reset)) {
		dev_err(phy->phy.dev, "failed to get phy_reset\n");
		ret = PTR_ERR(phy->phy_reset);
		goto disable_pipe_clk;
	}

	phy->phy_phy_reset = devm_clk_get(phy->phy.dev, "phy_phy_reset");
	if (IS_ERR(phy->phy_phy_reset)) {
		phy->phy_phy_reset = NULL;
		dev_dbg(phy->phy.dev, "phy_phy_reset unavailable\n");
	}

	phy->clk_enabled = true;
	return ret;

disable_pipe_clk:
	clk_disable_unprepare(phy->pipe_clk);
disable_cfg_ahb_clk:
	clk_disable_unprepare(phy->cfg_ahb_clk);
disable_aux_clk:
	clk_disable_unprepare(phy->aux_clk);

	return ret;
}

static int configure_phy_regs(struct usb_phy *uphy,
				const struct qmp_reg_val *reg)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);

	if (!reg) {
		dev_err(uphy->dev, "NULL PHY configuration\n");
		return -EINVAL;
	}

	while (reg->offset != -1 && reg->val != -1) {
		writel_relaxed(reg->val, phy->base + reg->offset);
		reg++;
	}
	return 0;
}

/* SSPHY Initialization */
static int msm_ssphy_qmp_init(struct usb_phy *uphy)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);
	int ret;
	unsigned init_timeout_usec = INIT_MAX_TIME_USEC;
	u32 revid;
	const struct qmp_reg_val *reg = NULL, *misc = NULL;

	dev_dbg(uphy->dev, "Initializing QMP phy\n");

	if (phy->emulation)
		return 0;

	if (!phy->clk_enabled) {
		ret = msm_ssphy_qmp_init_clocks(phy);
		if (ret) {
			dev_err(uphy->dev, "failed to init clocks %d\n", ret);
			return ret;
		}
	}

	/* Rev ID is made up each of the LSBs of REVISION_ID[0-3] */
	revid = (readl_relaxed(phy->base +
			PCIE_USB3_PHY_REVISION_ID3) & 0xFF) << 24;
	revid |= (readl_relaxed(phy->base +
			PCIE_USB3_PHY_REVISION_ID2) & 0xFF) << 16;
	revid |= (readl_relaxed(phy->base +
			PCIE_USB3_PHY_REVISION_ID1) & 0xFF) << 8;
	revid |= readl_relaxed(phy->base + PCIE_USB3_PHY_REVISION_ID0) & 0xFF;

	switch (revid) {
	case 0x10000000:
		reg = qmp_settings_rev0;
		misc = qmp_settings_rev0_misc;
		break;
	case 0x10000001:
		reg = qmp_settings_rev1;
		misc = qmp_settings_rev1_misc;
		break;
	default:
		dev_err(uphy->dev, "Unknown revid 0x%x, cannot initialize PHY\n",
			revid);
		return -ENODEV;
	}

	/* Configure AHB2PHY for one wait state reads/writes */
	if (phy->ahb2phy)
		writel_relaxed(0x11, phy->ahb2phy + PERIPH_SS_AHB2PHY_TOP_CFG);

	writel_relaxed(0x01, phy->base + PCIE_USB3_PHY_POWER_DOWN_CONTROL);

	/* Main configuration */
	if (configure_phy_regs(uphy, reg)) {
		dev_err(uphy->dev, "Failed the main PHY configuration\n");
		return ret;
	}

	/* Feature specific configurations */
	if (phy->override_pll_cal) {
		reg = qmp_override_pll;
		if (configure_phy_regs(uphy, reg)) {
			dev_err(uphy->dev,
				"Failed the PHY PLL override configuration\n");
			return ret;
		}
	}
	if (phy->misc_config) {
		configure_phy_regs(uphy, misc);
		dev_err(uphy->dev, "Failed the misc PHY configuration\n");
		return ret;
	}

	writel_relaxed(0x00, phy->base + PCIE_USB3_PHY_SW_RESET);
	writel_relaxed(0x03, phy->base + PCIE_USB3_PHY_START);

	if (!phy->switch_pipe_clk_src)
		/* this clock wasn't enabled before, enable it now */
		clk_prepare_enable(phy->pipe_clk);

	/* Wait for PHY initialization to be done */
	do {
		if (readl_relaxed(phy->base + PCIE_USB3_PHY_PCS_STATUS) &
			PHYSTATUS)
			usleep(1);
		else
			break;
	} while (--init_timeout_usec);

	if (!init_timeout_usec) {
		dev_err(uphy->dev, "QMP PHY initialization timeout\n");
		return -EBUSY;
	};

	/*
	 * After PHY initilization above, the PHY is generating
	 * the usb3_pipe_clk in 125MHz. Therefore now we can (if needed)
	 * switch the gcc_usb3_pipe_clk to 125MHz as well, so the
	 * gcc_usb3_pipe_clk is sourced now from the usb3_pipe3_clk
	 * instead of from the xo clock.
	 */
	if (phy->switch_pipe_clk_src)
		clk_set_rate(phy->pipe_clk, 125000000);

	return 0;
}

static int msm_ssphy_qmp_reset(struct usb_phy *uphy)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);
	int ret;

	dev_dbg(uphy->dev, "Resetting QMP phy\n");

	if (!phy->clk_enabled) {
		ret = msm_ssphy_qmp_init_clocks(phy);
		if (ret) {
			dev_err(uphy->dev, "failed to init clocks %d\n", ret);
			return ret;
		}
	}

	/* Assert USB3 PHY reset */
	if (phy->phy_com_reset) {
		ret = clk_reset(phy->phy_com_reset, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(uphy->dev, "phy_com_reset clk assert failed\n");
			return ret;
		}
	}

	/* Assert USB3 PHY reset */
	if (phy->phy_phy_reset) {
		ret = clk_reset(phy->phy_phy_reset, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(uphy->dev, "phy_phy reset assert failed\n");
			goto deassert_phy_com_reset;
		}
	} else {
		ret = clk_reset(phy->pipe_clk, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(uphy->dev, "pipe_clk reset assert failed\n");
			goto deassert_phy_com_reset;
		}
	}

	/* Assert USB3 PHY CSR reset */
	ret = clk_reset(phy->phy_reset, CLK_RESET_ASSERT);
	if (ret) {
		dev_err(uphy->dev, "phy_reset clk assert failed\n");
		goto deassert_phy_phy_reset;
	}

	/* Deassert USB3 PHY CSR reset */
	ret = clk_reset(phy->phy_reset, CLK_RESET_DEASSERT);
	if (ret) {
		dev_err(uphy->dev, "phy_reset clk deassert failed\n");
		goto deassert_phy_phy_reset;
	}

	/* Deassert USB3 PHY reset */
	if (phy->phy_phy_reset) {
		ret = clk_reset(phy->phy_phy_reset, CLK_RESET_DEASSERT);
		if (ret) {
			dev_err(uphy->dev, "phy_phy reset deassert failed\n");
			goto deassert_phy_com_reset;
		}
	} else {
		ret = clk_reset(phy->pipe_clk, CLK_RESET_DEASSERT);
		if (ret) {
			dev_err(uphy->dev, "pipe_clk reset deassert failed\n");
			goto deassert_phy_com_reset;
		}
	}

	if (phy->phy_com_reset) {
		ret = clk_reset(phy->phy_com_reset, CLK_RESET_DEASSERT);
		if (ret) {
			dev_err(uphy->dev, "phy_com_reset clk deassert failed\n");
			return ret;
		}
	}
	return 0;

deassert_phy_phy_reset:
	if (phy->phy_phy_reset)
		clk_reset(phy->phy_phy_reset, CLK_RESET_DEASSERT);
	else
		clk_reset(phy->pipe_clk, CLK_RESET_DEASSERT);
deassert_phy_com_reset:
	if (phy->phy_com_reset)
		clk_reset(phy->phy_com_reset, CLK_RESET_DEASSERT);

	phy->in_suspend = false;

	return ret;
}

static int msm_ssphy_qmp_set_params(struct usb_phy *uphy)
{
	dev_dbg(uphy->dev, "Setting phy parameters\n");
	return 0;
}

static int msm_ssphy_power_enable(struct msm_ssphy_qmp *phy, bool on)
{
	bool host = phy->phy.flags & PHY_HOST_MODE;
	int ret = 0;

	/*
	 * Turn off the phy's LDOs when cable is disconnected for device mode
	 * with external vbus_id indication.
	 */
	if (!host && !phy->cable_connected) {
		if (on) {
			ret = regulator_enable(phy->vdd);
			if (ret)
				dev_err(phy->phy.dev,
					"regulator_enable(phy->vdd) failed, ret=%d",
					ret);

			ret = msm_ssusb_qmp_ldo_enable(phy, 1);
			if (ret)
				dev_err(phy->phy.dev,
				"msm_ssusb_qmp_ldo_enable(1) failed, ret=%d\n",
				ret);
		} else {
			ret = msm_ssusb_qmp_ldo_enable(phy, 0);
			if (ret)
				dev_err(phy->phy.dev,
					"msm_ssusb_qmp_ldo_enable(0) failed, ret=%d\n",
					ret);

			ret = regulator_disable(phy->vdd);
			if (ret)
				dev_err(phy->phy.dev, "regulator_disable(phy->vdd) failed, ret=%d",
					ret);
		}
	}

	return ret;
}

/**
 * Performs QMP PHY suspend/resume functionality.
 *
 * @uphy - usb phy pointer.
 * @suspend - to enable suspend or not. 1 - suspend, 0 - resume
 *
 */
static int msm_ssphy_qmp_set_suspend(struct usb_phy *uphy, int suspend)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);

	dev_dbg(uphy->dev, "QMP PHY set_suspend for %s called with cable %s\n",
			(suspend ? "suspend" : "resume"),
			get_cable_status_str(phy));
	/*
	 * The dwc3 probe function calls set_suspend on both phys. This prevent
	 * a situation where would try writing to registers without the phy
	 * init function being called first.
	 */
	if (!phy->clk_enabled) {
		dev_dbg(uphy->dev, "clocks not enabled yet\n");
		return -EAGAIN;
	}

	if (phy->cable_connected && phy->in_suspend && suspend) {
		dev_dbg(uphy->dev, "%s: USB PHY is already suspended\n",
			__func__);
		return 0;
	}

	if (suspend) {
		msm_ssusb_qmp_enable_autonomous(phy);
		if (!phy->cable_connected) {
			clk_disable_unprepare(phy->pipe_clk);
			writel_relaxed(0x00,
				phy->base + PCIE_USB3_PHY_POWER_DOWN_CONTROL);
		}
		clk_disable_unprepare(phy->cfg_ahb_clk);
		clk_disable_unprepare(phy->aux_clk);
		phy->in_suspend = true;
		msm_ssphy_power_enable(phy, 0);
		dev_dbg(uphy->dev, "QMP PHY is suspend\n");
	} else {
		msm_ssphy_power_enable(phy, 1);
		clk_prepare_enable(phy->aux_clk);
		clk_prepare_enable(phy->cfg_ahb_clk);
		if (!phy->cable_connected) {
			clk_prepare_enable(phy->pipe_clk);
			writel_relaxed(0x01,
				phy->base + PCIE_USB3_PHY_POWER_DOWN_CONTROL);
		}
		msm_ssusb_qmp_enable_autonomous(phy);
		phy->in_suspend = false;
		dev_dbg(uphy->dev, "QMP PHY is resumed\n");
	}

	return 0;
}

static int msm_ssphy_qmp_notify_connect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);

	dev_dbg(uphy->dev, "QMP phy connect notification\n");
	phy->cable_connected = true;
	dev_dbg(uphy->dev, "cable_connected=%d\n", phy->cable_connected);
	return 0;
}

static int msm_ssphy_qmp_notify_disconnect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);

	dev_dbg(uphy->dev, "QMP phy disconnect notification\n");
	dev_dbg(uphy->dev, " cable_connected=%d\n", phy->cable_connected);
	msm_ssusb_qmp_enable_autonomous(phy);
	phy->cable_connected = false;
	phy->in_suspend = false;
	return 0;
}

static int msm_ssphy_qmp_probe(struct platform_device *pdev)
{
	struct msm_ssphy_qmp *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"qmp_phy_base");
	phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->base))
		return PTR_ERR(phy->base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"qmp_ahb2phy_base");
	phy->ahb2phy = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->ahb2phy)) {
		dev_err(dev, "couldn't find qmp_ahb2phy_base address.\n");
		phy->ahb2phy = NULL;
	}

	phy->emulation = of_property_read_bool(dev->of_node,
						"qcom,emulation");

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		return ret;
	}

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

	ret = msm_ssusb_qmp_config_vdd(phy, 1);
	if (ret) {
		dev_err(dev, "ssusb vdd_dig configuration failed\n");
		return ret;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(dev, "unable to enable the ssusb vdd_dig\n");
		goto unconfig_ss_vdd;
	}

	ret = msm_ssusb_qmp_ldo_enable(phy, 1);
	if (ret) {
		dev_err(dev, "ssusb vreg enable failed\n");
		goto disable_ss_vdd;
	}

	phy->ldo_clk = devm_clk_get(dev, "ldo_clk");
	if (!IS_ERR(phy->ldo_clk))
		clk_prepare_enable(phy->ldo_clk);

	platform_set_drvdata(pdev, phy);

	if (of_property_read_bool(dev->of_node, "qcom,vbus-valid-override"))
		phy->phy.flags |= PHY_VBUS_VALID_OVERRIDE;

	phy->override_pll_cal = of_property_read_bool(dev->of_node,
					"qcom,override-pll-calibration");
	if (phy->override_pll_cal)
		dev_dbg(dev, "Override PHY PLL calibration is enabled.\n");

	phy->misc_config = of_property_read_bool(dev->of_node,
					"qcom,qmp-misc-config");
	if (phy->misc_config)
		dev_dbg(dev, "Miscellaneous configurations are enabled.\n");

	phy->switch_pipe_clk_src = !of_property_read_bool(dev->of_node,
					"qcom,no-pipe-clk-switch");

	phy->phy.dev			= dev;
	phy->phy.init			= msm_ssphy_qmp_init;
	phy->phy.set_suspend		= msm_ssphy_qmp_set_suspend;
	phy->phy.set_params		= msm_ssphy_qmp_set_params;
	phy->phy.notify_connect		= msm_ssphy_qmp_notify_connect;
	phy->phy.notify_disconnect	= msm_ssphy_qmp_notify_disconnect;
	phy->phy.reset			= msm_ssphy_qmp_reset;
	phy->phy.type			= USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		goto disable_ss_ldo;
	return 0;

disable_ss_ldo:
	if (!IS_ERR(phy->ldo_clk))
		clk_disable_unprepare(phy->ldo_clk);
	msm_ssusb_qmp_ldo_enable(phy, 0);
disable_ss_vdd:
	regulator_disable(phy->vdd);
unconfig_ss_vdd:
	msm_ssusb_qmp_config_vdd(phy, 0);

	return ret;
}

static int msm_ssphy_qmp_remove(struct platform_device *pdev)
{
	struct msm_ssphy_qmp *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	usb_remove_phy(&phy->phy);
	if (!IS_ERR(phy->ldo_clk))
		clk_disable_unprepare(phy->ldo_clk);
	msm_ssusb_qmp_ldo_enable(phy, 0);
	regulator_disable(phy->vdd);
	msm_ssusb_qmp_config_vdd(phy, 0);
	clk_disable_unprepare(phy->aux_clk);
	clk_disable_unprepare(phy->cfg_ahb_clk);
	clk_disable_unprepare(phy->pipe_clk);
	kfree(phy);
	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-ssphy-qmp",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_ssphy_qmp_driver = {
	.probe		= msm_ssphy_qmp_probe,
	.remove		= msm_ssphy_qmp_remove,
	.driver = {
		.name	= "msm-usb-ssphy-qmp",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_ssphy_qmp_driver);

MODULE_DESCRIPTION("MSM USB SS QMP PHY driver");
MODULE_LICENSE("GPL v2");

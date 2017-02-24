/*
 * Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/phy.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>
#include <linux/reset.h>

enum ldo_levels {
	VOLTAGE_LEVEL_NONE = 0,
	VOLTAGE_LEVEL_MIN,
	VOLTAGE_LEVEL_MAX,
};

#define INIT_MAX_TIME_USEC			1000

/* default CORE votlage and load values */
#define USB_SSPHY_1P2_VOL_MIN		1200000 /* uV */
#define USB_SSPHY_1P2_VOL_MAX		1200000 /* uV */
#define USB_SSPHY_HPM_LOAD		23000	/* uA */

#define USB_SSPHY_LOAD_DEFAULT		-1

/* USB3PHY_PCIE_USB3_PCS_PCS_STATUS bit */
#define PHYSTATUS				BIT(6)

/* PCIE_USB3_PHY_AUTONOMOUS_MODE_CTRL bits */
#define ARCVR_DTCT_EN		BIT(0)
#define ALFPS_DTCT_EN		BIT(1)
#define ARCVR_DTCT_EVENT_SEL	BIT(4)

/* PCIE_USB3_PHY_PCS_MISC_TYPEC_CTRL bits */

/* 0 - selects Lane A. 1 - selects Lane B */
#define SW_PORTSELECT		BIT(0)
/* port select mux: 1 - sw control. 0 - HW control*/
#define SW_PORTSELECT_MX	BIT(1)

enum qmp_phy_rev_reg {
	USB3_PHY_PCS_STATUS,
	USB3_PHY_AUTONOMOUS_MODE_CTRL,
	USB3_PHY_LFPS_RXTERM_IRQ_CLEAR,
	USB3_PHY_POWER_DOWN_CONTROL,
	USB3_PHY_SW_RESET,
	USB3_PHY_START,
	USB3_PHY_PCS_MISC_TYPEC_CTRL,
	USB3_PHY_REG_MAX,
};

/* reg values to write */
struct qmp_reg_val {
	u32 offset;
	u32 val;
	u32 delay;
};

struct msm_ssphy_qmp {
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*vls_clamp_reg;
	void __iomem		*tcsr_usb3_dp_phymode;

	struct regulator	*vdd;
	int			vdd_levels[3]; /* none, low, high */
	struct regulator	*core_ldo;
	int			core_voltage_levels[3];
	struct regulator	*fpc_redrive_ldo;
	int			redrive_voltage_levels[3];
	int			redrive_load;
	struct clk		*ref_clk_src;
	struct clk		*ref_clk;
	struct clk		*aux_clk;
	struct clk		*cfg_ahb_clk;
	struct clk		*pipe_clk;
	bool			power_enabled;
	struct reset_control	*phy_reset;
	struct reset_control	*phy_phy_reset;

	bool			clk_enabled;
	bool			cable_connected;
	bool			in_suspend;
	bool			emulation;
	unsigned int		*phy_reg; /* revision based offset */
	unsigned int		*qmp_phy_init_seq;
	int			init_seq_len;
	unsigned int		*qmp_phy_reg_offset;
	int			reg_offset_cnt;
};

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-ssphy-qmp",
	},
	{
		.compatible = "qcom,usb-ssphy-qmp-v1",
	},
	{
		.compatible = "qcom,usb-ssphy-qmp-v2",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static inline char *get_cable_status_str(struct msm_ssphy_qmp *phy)
{
	return phy->cable_connected ? "connected" : "disconnected";
}

static void msm_ssusb_qmp_clr_lfps_rxterm_int(struct msm_ssphy_qmp *phy)
{
	writeb_relaxed(1, phy->base +
			phy->phy_reg[USB3_PHY_LFPS_RXTERM_IRQ_CLEAR]);
	/* flush the previous write before next write */
	wmb();
	writeb_relaxed(0, phy->base +
			phy->phy_reg[USB3_PHY_LFPS_RXTERM_IRQ_CLEAR]);
}

static void msm_ssusb_qmp_enable_autonomous(struct msm_ssphy_qmp *phy,
		int enable)
{
	u8 val;
	unsigned int autonomous_mode_offset =
			phy->phy_reg[USB3_PHY_AUTONOMOUS_MODE_CTRL];

	dev_dbg(phy->phy.dev, "enabling QMP autonomous mode with cable %s\n",
			get_cable_status_str(phy));

	if (enable) {
		msm_ssusb_qmp_clr_lfps_rxterm_int(phy);
		if (phy->phy.flags & DEVICE_IN_SS_MODE) {
			val =
			readb_relaxed(phy->base + autonomous_mode_offset);
			val |= ARCVR_DTCT_EN;
			val |= ALFPS_DTCT_EN;
			val &= ~ARCVR_DTCT_EVENT_SEL;
			writeb_relaxed(val, phy->base + autonomous_mode_offset);
		}

		/* clamp phy level shifter to perform autonomous detection */
		writel_relaxed(0x1, phy->vls_clamp_reg);
	} else {
		writel_relaxed(0x0, phy->vls_clamp_reg);
		writeb_relaxed(0, phy->base + autonomous_mode_offset);
		msm_ssusb_qmp_clr_lfps_rxterm_int(phy);
	}
}

static int msm_ldo_enable(struct msm_ssphy_qmp *phy,
		struct regulator *ldo, int *voltage_levels, int load)
{
	int ret = 0;

	dev_dbg(phy->phy.dev,
		"ldo: min_vol:%duV max_vol:%duV\n",
		voltage_levels[VOLTAGE_LEVEL_MIN],
		voltage_levels[VOLTAGE_LEVEL_MAX]);

	if (load > 0) {
		ret = regulator_set_load(ldo, load);
		if (ret < 0)
			return ret;
	}

	ret = regulator_set_voltage(ldo,
			voltage_levels[VOLTAGE_LEVEL_MIN],
			voltage_levels[VOLTAGE_LEVEL_MAX]);
	if (ret)
		return ret;

	ret = regulator_enable(ldo);

	return ret;
}

static int msm_ssusb_qmp_ldo_enable(struct msm_ssphy_qmp *phy, int on)
{
	int min, rc = 0;

	dev_dbg(phy->phy.dev, "reg (%s)\n", on ? "HPM" : "LPM");

	if (phy->power_enabled == on) {
		dev_dbg(phy->phy.dev, "PHYs' regulators status %d\n",
			phy->power_enabled);
		return 0;
	}

	phy->power_enabled = on;

	min = on ? 1 : 0; /* low or none? */

	if (!on)
		goto disable_regulators;

	if (phy->fpc_redrive_ldo) {
		rc = msm_ldo_enable(phy, phy->fpc_redrive_ldo,
				phy->redrive_voltage_levels,
				phy->redrive_load);
		if (rc < 0) {
			dev_err(phy->phy.dev,
				"enable phy->fpc_redrive_ldo failed\n");
			return rc;
		}

		dev_dbg(phy->phy.dev,
			"fpc redrive ldo: min_vol:%duV max_vol:%duV\n",
			phy->redrive_voltage_levels[VOLTAGE_LEVEL_MIN],
			phy->redrive_voltage_levels[VOLTAGE_LEVEL_MAX]);
	}

	rc = msm_ldo_enable(phy, phy->vdd, phy->vdd_levels,
			USB_SSPHY_LOAD_DEFAULT);
	if (rc < 0) {
		dev_err(phy->phy.dev, "enable phy->vdd failed\n");
		goto disable_fpc_redrive;
	}

	dev_dbg(phy->phy.dev,
		"vdd ldo: min_vol:%duV max_vol:%duV\n",
		phy->vdd_levels[VOLTAGE_LEVEL_MIN],
		phy->vdd_levels[VOLTAGE_LEVEL_MAX]);

	rc = msm_ldo_enable(phy, phy->core_ldo, phy->core_voltage_levels,
			USB_SSPHY_HPM_LOAD);
	if (rc < 0) {
		dev_err(phy->phy.dev, "enable phy->core_ldo failed\n");
		goto disable_vdd;
	}

	dev_dbg(phy->phy.dev,
		"core ldo: min_vol:%duV max_vol:%duV\n",
		phy->core_voltage_levels[VOLTAGE_LEVEL_MIN],
		phy->core_voltage_levels[VOLTAGE_LEVEL_MAX]);

	return 0;

disable_regulators:
	rc = regulator_disable(phy->core_ldo);
	if (rc)
		dev_err(phy->phy.dev, "disable phy->core_ldo failed\n");

disable_vdd:
	rc = regulator_disable(phy->vdd);
	if (rc)
		dev_err(phy->phy.dev, "disable phy->vdd failed\n");

disable_fpc_redrive:
	if (phy->fpc_redrive_ldo) {
		rc = regulator_disable(phy->fpc_redrive_ldo);
		if (rc)
			dev_err(phy->phy.dev,
				"disable phy->fpc_redrive_ldo failed\n");
	}

	return rc < 0 ? rc : 0;
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

	while (reg->offset != -1) {
		writel_relaxed(reg->val, phy->base + reg->offset);
		if (reg->delay)
			usleep_range(reg->delay, reg->delay + 10);
		reg++;
	}
	return 0;
}

/* SSPHY Initialization */
static int msm_ssphy_qmp_init(struct usb_phy *uphy)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);
	int ret, val;
	unsigned init_timeout_usec = INIT_MAX_TIME_USEC;
	const struct qmp_reg_val *reg = NULL;

	dev_dbg(uphy->dev, "Initializing QMP phy\n");

	if (phy->emulation)
		return 0;

	ret = msm_ssusb_qmp_ldo_enable(phy, 1);
	if (ret) {
		dev_err(phy->phy.dev,
		"msm_ssusb_qmp_ldo_enable(1) failed, ret=%d\n",
		ret);
		return ret;
	}

	if (!phy->clk_enabled) {
		if (phy->ref_clk_src)
			clk_prepare_enable(phy->ref_clk_src);
		if (phy->ref_clk)
			clk_prepare_enable(phy->ref_clk);
		clk_prepare_enable(phy->aux_clk);
		clk_prepare_enable(phy->cfg_ahb_clk);
		clk_set_rate(phy->pipe_clk, 125000000);
		clk_prepare_enable(phy->pipe_clk);
		phy->clk_enabled = true;
	}

	/* select usb3 phy mode */
	if (phy->tcsr_usb3_dp_phymode)
		writel_relaxed(0x0, phy->tcsr_usb3_dp_phymode);

	writel_relaxed(0x01,
		phy->base + phy->phy_reg[USB3_PHY_POWER_DOWN_CONTROL]);

	/* Make sure that above write completed to get PHY into POWER DOWN */
	mb();

	reg = (struct qmp_reg_val *)phy->qmp_phy_init_seq;

	/* Main configuration */
	ret = configure_phy_regs(uphy, reg);
	if (ret) {
		dev_err(uphy->dev, "Failed the main PHY configuration\n");
		return ret;
	}

	/* perform lane selection */
	val = -EINVAL;
	if (phy->phy.flags & PHY_LANE_A)
		val = SW_PORTSELECT_MX;

	if (phy->phy.flags & PHY_LANE_B)
		val = SW_PORTSELECT | SW_PORTSELECT_MX;

	if (val > 0)
		writel_relaxed(val,
			phy->base + phy->phy_reg[USB3_PHY_PCS_MISC_TYPEC_CTRL]);

	writel_relaxed(0x03, phy->base + phy->phy_reg[USB3_PHY_START]);
	writel_relaxed(0x00, phy->base + phy->phy_reg[USB3_PHY_SW_RESET]);

	/* Make sure above write completed to bring PHY out of reset */
	mb();

	/* Wait for PHY initialization to be done */
	do {
		if (readl_relaxed(phy->base +
			phy->phy_reg[USB3_PHY_PCS_STATUS]) & PHYSTATUS)
			usleep_range(1, 2);
		else
			break;
	} while (--init_timeout_usec);

	if (!init_timeout_usec) {
		dev_err(uphy->dev, "QMP PHY initialization timeout\n");
		dev_err(uphy->dev, "USB3_PHY_PCS_STATUS:%x\n",
				readl_relaxed(phy->base +
					phy->phy_reg[USB3_PHY_PCS_STATUS]));
		return -EBUSY;
	};

	return 0;
}

static int msm_ssphy_qmp_reset(struct usb_phy *uphy)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);
	int ret;

	dev_dbg(uphy->dev, "Resetting QMP phy\n");

	/* Assert USB3 PHY reset */
	ret = reset_control_assert(phy->phy_phy_reset);
	if (ret) {
		dev_err(uphy->dev, "phy_phy_reset assert failed\n");
		goto exit;
	}

	/* Assert USB3 PHY CSR reset */
	ret = reset_control_assert(phy->phy_reset);
	if (ret) {
		dev_err(uphy->dev, "phy_reset assert failed\n");
		goto deassert_phy_phy_reset;
	}

	/* Deassert USB3 PHY CSR reset */
	ret = reset_control_deassert(phy->phy_reset);
	if (ret) {
		dev_err(uphy->dev, "phy_reset deassert failed\n");
		goto deassert_phy_phy_reset;
	}

	/* Deassert USB3 PHY reset */
	ret = reset_control_deassert(phy->phy_phy_reset);
	if (ret) {
		dev_err(uphy->dev, "phy_phy_reset deassert failed\n");
		goto exit;
	}

	return 0;

deassert_phy_phy_reset:
	ret = reset_control_deassert(phy->phy_phy_reset);
	if (ret)
		dev_err(uphy->dev, "phy_phy_reset deassert failed\n");
exit:
	phy->in_suspend = false;

	return ret;
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

	if (phy->in_suspend == suspend) {
		dev_dbg(uphy->dev, "%s: USB PHY is already %s.\n",
			__func__, (suspend ? "suspended" : "resumed"));
		return 0;
	}

	if (suspend) {
		if (!phy->cable_connected)
			writel_relaxed(0x00,
			phy->base + phy->phy_reg[USB3_PHY_POWER_DOWN_CONTROL]);
		else
			msm_ssusb_qmp_enable_autonomous(phy, 1);

		/* Make sure above write completed with PHY */
		wmb();

		clk_disable_unprepare(phy->cfg_ahb_clk);
		clk_disable_unprepare(phy->aux_clk);
		clk_disable_unprepare(phy->pipe_clk);
		if (phy->ref_clk)
			clk_disable_unprepare(phy->ref_clk);
		if (phy->ref_clk_src)
			clk_disable_unprepare(phy->ref_clk_src);
		phy->clk_enabled = false;
		phy->in_suspend = true;
		msm_ssphy_power_enable(phy, 0);
		dev_dbg(uphy->dev, "QMP PHY is suspend\n");
	} else {
		msm_ssphy_power_enable(phy, 1);
		clk_prepare_enable(phy->pipe_clk);
		if (!phy->clk_enabled) {
			if (phy->ref_clk_src)
				clk_prepare_enable(phy->ref_clk_src);
			if (phy->ref_clk)
				clk_prepare_enable(phy->ref_clk);
			clk_prepare_enable(phy->aux_clk);
			clk_prepare_enable(phy->cfg_ahb_clk);
			phy->clk_enabled = true;
		}
		if (!phy->cable_connected) {
			writel_relaxed(0x01,
			phy->base + phy->phy_reg[USB3_PHY_POWER_DOWN_CONTROL]);
		} else  {
			msm_ssusb_qmp_enable_autonomous(phy, 0);
		}

		/* Make sure that above write completed with PHY */
		wmb();

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
	phy->cable_connected = false;
	return 0;
}

static int msm_ssphy_qmp_probe(struct platform_device *pdev)
{
	struct msm_ssphy_qmp *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0, size = 0, len;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->aux_clk = devm_clk_get(dev, "aux_clk");
	if (IS_ERR(phy->aux_clk)) {
		ret = PTR_ERR(phy->aux_clk);
		phy->aux_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get aux_clk\n");
		goto err;
	}

	clk_set_rate(phy->aux_clk, clk_round_rate(phy->aux_clk, ULONG_MAX));

	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "cfg_ahb_clk") >= 0) {
		phy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb_clk");
		if (IS_ERR(phy->cfg_ahb_clk)) {
			ret = PTR_ERR(phy->cfg_ahb_clk);
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
				"failed to get cfg_ahb_clk ret %d\n", ret);
			goto err;
		}
	}

	phy->pipe_clk = devm_clk_get(dev, "pipe_clk");
	if (IS_ERR(phy->pipe_clk)) {
		ret = PTR_ERR(phy->pipe_clk);
		phy->pipe_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get pipe_clk\n");
		goto err;
	}

	phy->phy_reset = devm_reset_control_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset)) {
		ret = PTR_ERR(phy->phy_reset);
		dev_dbg(dev, "failed to get phy_reset\n");
		goto err;
	}

	phy->phy_phy_reset = devm_reset_control_get(dev, "phy_phy_reset");
	if (IS_ERR(phy->phy_phy_reset)) {
		ret = PTR_ERR(phy->phy_phy_reset);
		dev_dbg(dev, "failed to get phy_phy_reset\n");
		goto err;
	}

	of_get_property(dev->of_node, "qcom,qmp-phy-reg-offset", &size);
	if (size) {
		phy->qmp_phy_reg_offset = devm_kzalloc(dev,
						size, GFP_KERNEL);
		if (phy->qmp_phy_reg_offset) {
			phy->reg_offset_cnt =
				(size / sizeof(*phy->qmp_phy_reg_offset));
			if (phy->reg_offset_cnt > USB3_PHY_REG_MAX) {
				dev_err(dev, "invalid reg offset count\n");
				return -EINVAL;
			}

			of_property_read_u32_array(dev->of_node,
				"qcom,qmp-phy-reg-offset",
				phy->qmp_phy_reg_offset,
				phy->reg_offset_cnt);
		} else {
			dev_err(dev, "err mem alloc for qmp_phy_reg_offset\n");
			return -ENOMEM;
		}
		phy->phy_reg = phy->qmp_phy_reg_offset;
	} else {
		dev_err(dev, "err provide qcom,qmp-phy-reg-offset\n");
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"qmp_phy_base");
	if (!res) {
		dev_err(dev, "failed getting qmp_phy_base\n");
		return -ENODEV;
	}
	phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->base)) {
		ret = PTR_ERR(phy->base);
		goto err;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"vls_clamp_reg");
	if (!res) {
		dev_err(dev, "failed getting vls_clamp_reg\n");
		return -ENODEV;
	}
	phy->vls_clamp_reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->vls_clamp_reg)) {
		dev_err(dev, "couldn't find vls_clamp_reg address.\n");
		return PTR_ERR(phy->vls_clamp_reg);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"tcsr_usb3_dp_phymode");
	if (res) {
		phy->tcsr_usb3_dp_phymode = devm_ioremap_resource(dev, res);
		if (IS_ERR(phy->tcsr_usb3_dp_phymode)) {
			dev_err(dev, "err getting tcsr_usb3_dp_phymode addr\n");
			return PTR_ERR(phy->tcsr_usb3_dp_phymode);
		}
	}

	phy->emulation = of_property_read_bool(dev->of_node,
						"qcom,emulation");
	if (!phy->emulation) {
		of_get_property(dev->of_node, "qcom,qmp-phy-init-seq", &size);
		if (size) {
			if (size % sizeof(*phy->qmp_phy_init_seq)) {
				dev_err(dev, "invalid init_seq_len\n");
				return -EINVAL;
			}
			phy->qmp_phy_init_seq = devm_kzalloc(dev,
							size, GFP_KERNEL);
			if (phy->qmp_phy_init_seq) {
				phy->init_seq_len =
					(size / sizeof(*phy->qmp_phy_init_seq));

				of_property_read_u32_array(dev->of_node,
					"qcom,qmp-phy-init-seq",
					phy->qmp_phy_init_seq,
					phy->init_seq_len);
			} else {
				dev_err(dev, "error allocating memory for phy_init_seq\n");
				return -EINVAL;
			}
		} else {
			dev_err(dev, "error need qmp-phy-init-seq\n");
			return -EINVAL;
		}
	}

	/* Set default core voltage values */
	phy->core_voltage_levels[VOLTAGE_LEVEL_NONE] = 0;
	phy->core_voltage_levels[VOLTAGE_LEVEL_MIN] = USB_SSPHY_1P2_VOL_MIN;
	phy->core_voltage_levels[VOLTAGE_LEVEL_MAX] = USB_SSPHY_1P2_VOL_MAX;

	if (of_get_property(dev->of_node, "qcom,core-voltage-level", &len) &&
		len == sizeof(phy->core_voltage_levels)) {
		ret = of_property_read_u32_array(dev->of_node,
				"qcom,core-voltage-level",
				(u32 *)phy->core_voltage_levels,
				len / sizeof(u32));
		if (ret) {
			dev_err(dev, "err qcom,core-voltage-level property\n");
			goto err;
		}
	}

	if (of_get_property(dev->of_node, "qcom,vdd-voltage-level", &len) &&
		len == sizeof(phy->vdd_levels)) {
		ret = of_property_read_u32_array(dev->of_node,
				"qcom,vdd-voltage-level",
				(u32 *) phy->vdd_levels,
				len / sizeof(u32));
		if (ret) {
			dev_err(dev, "err qcom,vdd-voltage-level property\n");
			goto err;
		}
	} else {
		ret = -EINVAL;
		dev_err(dev, "error invalid inputs for vdd-voltage-level\n");
		goto err;
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		ret = PTR_ERR(phy->vdd);
		goto err;
	}

	phy->core_ldo = devm_regulator_get(dev, "core");
	if (IS_ERR(phy->core_ldo)) {
		dev_err(dev, "unable to get core ldo supply\n");
		ret = PTR_ERR(phy->core_ldo);
		goto err;
	}

	phy->fpc_redrive_ldo = devm_regulator_get_optional(dev, "fpc-redrive");
	if (IS_ERR(phy->fpc_redrive_ldo)) {
		phy->fpc_redrive_ldo = NULL;
		dev_dbg(dev, "no FPC re-drive ldo regulator\n");
	} else {
		if (of_get_property(dev->of_node,
				"qcom,redrive-voltage-level", &len) &&
				len == sizeof(phy->redrive_voltage_levels)) {
			ret = of_property_read_u32_array(dev->of_node,
					"qcom,redrive-voltage-level",
					(u32 *) phy->redrive_voltage_levels,
					len / sizeof(u32));
			if (ret) {
				dev_err(dev,
					"err qcom,redrive-voltage-level\n");
				goto err;
			}
		} else {
			ret = -EINVAL;
			dev_err(dev, "err inputs for redrive-voltage-level\n");
			goto err;
		}

		ret = of_property_read_u32(dev->of_node, "qcom,redrive-load",
				&phy->redrive_load);
		if (ret) {
			dev_err(&pdev->dev, "unable to read redrive load\n");
			goto err;
		}

		dev_dbg(dev, "Get FPC re-drive ldo regulator\n");
	}

	phy->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(phy->ref_clk_src))
		phy->ref_clk_src = NULL;
	phy->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(phy->ref_clk))
		phy->ref_clk = NULL;

	platform_set_drvdata(pdev, phy);

	if (of_property_read_bool(dev->of_node, "qcom,vbus-valid-override"))
		phy->phy.flags |= PHY_VBUS_VALID_OVERRIDE;

	phy->phy.dev			= dev;
	phy->phy.init			= msm_ssphy_qmp_init;
	phy->phy.set_suspend		= msm_ssphy_qmp_set_suspend;
	phy->phy.notify_connect		= msm_ssphy_qmp_notify_connect;
	phy->phy.notify_disconnect	= msm_ssphy_qmp_notify_disconnect;
	phy->phy.reset			= msm_ssphy_qmp_reset;
	phy->phy.type			= USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);

err:
	return ret;
}

static int msm_ssphy_qmp_remove(struct platform_device *pdev)
{
	struct msm_ssphy_qmp *phy = platform_get_drvdata(pdev);

	if (!phy)
		return 0;

	usb_remove_phy(&phy->phy);
	if (phy->ref_clk)
		clk_disable_unprepare(phy->ref_clk);
	if (phy->ref_clk_src)
		clk_disable_unprepare(phy->ref_clk_src);
	msm_ssusb_qmp_ldo_enable(phy, 0);
	clk_disable_unprepare(phy->aux_clk);
	clk_disable_unprepare(phy->cfg_ahb_clk);
	clk_disable_unprepare(phy->pipe_clk);
	kfree(phy);
	return 0;
}

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

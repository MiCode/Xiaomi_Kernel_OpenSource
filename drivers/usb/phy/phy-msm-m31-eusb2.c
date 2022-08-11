// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/qcom_scm.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/usb/dwc3-msm.h>
#include <linux/usb/phy.h>
#include <linux/usb/repeater.h>

#define USB_PHY_UTMI_CTRL0		(0x3c)
#define OPMODE_MASK			(0x3 << 3)
#define OPMODE_NONDRIVING		(0x1 << 3)
#define SLEEPM				BIT(0)

#define USB_PHY_UTMI_CTRL5		(0x50)

#define USB_PHY_HS_PHY_CTRL_COMMON0	(0x54)
#define PHY_ENABLE			BIT(0)
#define SIDDQ_SEL			BIT(1)
#define SIDDQ				BIT(2)
#define RETENABLEN			BIT(3)
#define FSEL				(0x7 << 4)
#define FSEL_19_2_MHZ_VAL		(0x0 << 4)
#define FSEL_38_4_MHZ_VAL		(0x4 << 4)

#define USB_PHY_CFG_CTRL_1		(0x58)
#define PHY_CFG_PLL_CPBIAS_CNTRL	(0xfe)
#define PHY_CFG_PLL_CPBIAS_CNTRL_SHIFT	(0x1)

#define USB_PHY_CFG_CTRL_2		(0x5c)
#define PHY_CFG_PLL_FB_DIV_7_0		(0xff)
#define DIV_7_0_19_2_MHZ_VAL		(0x90)
#define DIV_7_0_38_4_MHZ_VAL		(0xc8)

#define USB_PHY_CFG_CTRL_3		(0x60)
#define PHY_CFG_PLL_FB_DIV_11_8		(0xf)
#define DIV_11_8_19_2_MHZ_VAL		(0x1)
#define DIV_11_8_38_4_MHZ_VAL		(0x0)

#define PHY_CFG_PLL_REF_DIV		(0xf << 4)
#define PLL_REF_DIV_VAL			(0x0)

#define USB_PHY_HS_PHY_CTRL2		(0x64)
#define VBUSVLDEXT0			BIT(0)
#define USB2_SUSPEND_N			BIT(2)
#define USB2_SUSPEND_N_SEL		BIT(3)
#define VBUS_DET_EXT_SEL		BIT(4)

#define USB_PHY_CFG_CTRL_4		(0x68)
#define PHY_CFG_PLL_GMP_CNTRL		(0x3)
#define PHY_CFG_PLL_GMP_CNTRL_SHIFT	(0x0)
#define PHY_CFG_PLL_INT_CNTRL		(0xfc)
#define PHY_CFG_PLL_INT_CNTRL_SHIFT	(0x2)

#define USB_PHY_CFG_CTRL_5		(0x6c)
#define PHY_CFG_PLL_PROP_CNTRL		(0x1f)
#define PHY_CFG_PLL_PROP_CNTRL_SHIFT	(0x0)
#define PHY_CFG_PLL_VREF_TUNE		(0x3 << 6)
#define PHY_CFG_PLL_VREF_TUNE_SHIFT	(6)

#define USB_PHY_CFG_CTRL_6		(0x70)
#define PHY_CFG_PLL_VCO_CNTRL		(0x7)
#define PHY_CFG_PLL_VCO_CNTRL_SHIFT	(0x0)

#define USB_PHY_CFG_CTRL_7		(0x74)

#define USB_PHY_CFG_CTRL_8		(0x78)
#define PHY_CFG_TX_FSLS_VREF_TUNE	(0x3)
#define PHY_CFG_TX_FSLS_VREG_BYPASS	BIT(2)
#define PHY_CFG_TX_HS_VREF_TUNE		(0x7 << 3)
#define PHY_CFG_TX_HS_VREF_TUNE_SHIFT	(0x3)
#define PHY_CFG_TX_HS_XV_TUNE		(0x3 << 6)
#define PHY_CFG_TX_HS_XV_TUNE_SHIFT	(6)

#define USB_PHY_CFG_CTRL_9		(0x7c)
#define PHY_CFG_TX_PREEMP_TUNE		(0x7)
#define PHY_CFG_TX_PREEMP_TUNE_SHIFT	(0x0)
#define PHY_CFG_TX_RES_TUNE		(0x3 << 3)
#define PHY_CFG_TX_RES_TUNE_SHIFT	(0x3)
#define PHY_CFG_TX_RISE_TUNE		(0x3 << 5)
#define PHY_CFG_TX_RISE_TUNE_SHIFT	(0x5)
#define PHY_CFG_RCAL_BYPASS		BIT(7)
#define PHY_CFG_RCAL_BYPASS_SHIFT	(0x7)

#define USB_PHY_CFG_CTRL_10		(0x80)

#define USB_PHY_CFG0			(0x94)
#define DATAPATH_CTRL_OVERRIDE_EN	BIT(0)

#define USB_PHY_CFG1			(0x154)
#define USB_PHY_CFG1_PLL_EN		BIT(0)
#define UTMI_PHY_CMN_CTRL0		(0x98)
#define TESTBURNIN			BIT(6)

#define USB_PHY_FSEL_SEL		(0xb8)

/* M31 PHY XCFGI interface registers */
#define USB_PHY_XCFGI_39_32		(0x16c)
#define USB_PHY_XCFGI_71_64		(0x17c)
#define USB_PHY_XCFGI_31_24		(0x168)
#define XCFG_U2_HSTX_SLEW		(0x7)
#define USB_PHY_XCFGI_7_0		(0x15c)
#define XCFG_U2_PLLLOCKTIME		(0x3)

/* EUD CSR field */
#define EUD_EN2				BIT(0)

/* VIOCTL_EUD_DETECT register based EUD_DETECT field */
#define EUD_DETECT			BIT(0)

#define USB_HSPHY_1P2_VOL_MIN		1200000 /* uV */
#define USB_HSPHY_1P2_VOL_MAX		1200000 /* uV */
#define USB_HSPHY_1P2_HPM_LOAD		5905	/* uA */

#define USB_HSPHY_VDD_HPM_LOAD		7757	/* uA */

struct eusb_phy_tbl {
	u32 offset;
	u32 bit_mask;
	u32 val;
};

#define EUSB_PHY_INIT_CFG(o, v, b)	\
	{				\
		.offset = o,		\
		.bit_mask = b,		\
		.val = v,		\
	}

static const struct eusb_phy_tbl m31_eusb_phy_tbl[] = {
	EUSB_PHY_INIT_CFG(USB_PHY_CFG0, BIT(1), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_UTMI_CTRL5, BIT(1), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_CFG1_PLL_EN, BIT(0), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_FSEL_SEL, BIT(0), 1),
};

static const struct eusb_phy_tbl m31_eusb_phy_override_tbl[] = {
	EUSB_PHY_INIT_CFG(USB_PHY_XCFGI_39_32, GENMASK(3, 2), 0),
	EUSB_PHY_INIT_CFG(USB_PHY_XCFGI_71_64, GENMASK(3, 0), 7),
	EUSB_PHY_INIT_CFG(USB_PHY_XCFGI_31_24, GENMASK(2, 0), 0),
	EUSB_PHY_INIT_CFG(USB_PHY_XCFGI_7_0, GENMASK(1, 0), 0),
};

static const struct eusb_phy_tbl m31_eusb_phy_reset_tbl[] = {
	EUSB_PHY_INIT_CFG(USB_PHY_HS_PHY_CTRL2, BIT(3), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_HS_PHY_CTRL2, BIT(2), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_UTMI_CTRL0, BIT(0), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_HS_PHY_CTRL_COMMON0, BIT(1), 1),
	EUSB_PHY_INIT_CFG(USB_PHY_HS_PHY_CTRL_COMMON0, BIT(2), 0),
	EUSB_PHY_INIT_CFG(USB_PHY_UTMI_CTRL5, BIT(1), 0),
	EUSB_PHY_INIT_CFG(USB_PHY_HS_PHY_CTRL2, BIT(3), 0),
	EUSB_PHY_INIT_CFG(USB_PHY_CFG0, BIT(1), 0),
};

struct eusb_phy_cfg {
	const struct eusb_phy_tbl	*init_seq;
	int				init_seq_num;
	const struct eusb_phy_tbl	*override_seq;
	int				override_seq_num;
	const struct eusb_phy_tbl	*reset_seq;
	int				reset_seq_num;
};

static const struct eusb_phy_cfg m31_eusb_phy_cfg = {
	.init_seq		= m31_eusb_phy_tbl,
	.init_seq_num		= ARRAY_SIZE(m31_eusb_phy_tbl),
	.override_seq		= m31_eusb_phy_override_tbl,
	.override_seq_num	= ARRAY_SIZE(m31_eusb_phy_override_tbl),
	.reset_seq		= m31_eusb_phy_reset_tbl,
	.reset_seq_num		= ARRAY_SIZE(m31_eusb_phy_reset_tbl),
};

struct m31_eusb2_phy {
	struct usb_phy		phy;
	void __iomem		*base;

	/* EUD related parameters */
	phys_addr_t		eud_reg;
	void __iomem		*eud_enable_reg;
	void __iomem		*eud_detect_reg;
	bool			re_enable_eud;

	struct clk		*ref_clk_src;
	struct clk		*ref_clk;
	struct reset_control	*phy_reset;

	struct regulator	*vdd;
	struct regulator	*vdda12;
	int			vdd_levels[3]; /* none, low, high */

	bool			clocks_enabled;
	bool			power_enabled;
	bool			suspended;
	bool			cable_connected;
	bool			ref_clk_enable;

	struct power_supply	*usb_psy;
	unsigned int		vbus_draw;
	struct work_struct	vbus_draw_work;

	int			*param_override_seq;
	int			param_override_seq_cnt;

	/* debugfs entries */
	struct dentry		*root;
	u8			xcfgi_39_32;
	u8			xcfgi_71_64;
	u8			xcfgi_31_24;
	u8			xcfgi_7_0;

	struct usb_repeater	*ur;

	const
	struct eusb_phy_cfg	*cfg;
};

static inline bool is_eud_debug_mode_active(struct m31_eusb2_phy *phy)
{
	if (phy->eud_enable_reg &&
		(readl_relaxed(phy->eud_enable_reg) & EUD_EN2))
		return true;

	return false;
}

static void msm_m31_eusb2_phy_clocks(struct m31_eusb2_phy *phy, bool on)
{
	dev_dbg(phy->phy.dev, "clocks_enabled:%d on:%d\n",
			phy->clocks_enabled, on);

	if (phy->clocks_enabled == on)
		return;

	if (on) {
		clk_prepare_enable(phy->ref_clk_src);

		if (phy->ref_clk)
			clk_prepare_enable(phy->ref_clk);
	} else {
		if (phy->ref_clk)
			clk_disable_unprepare(phy->ref_clk);

		clk_disable_unprepare(phy->ref_clk_src);
	}

	phy->clocks_enabled = on;
}

static void msm_m31_eusb2_phy_update_eud_detect(struct m31_eusb2_phy *phy, bool set)
{
	if (set)
		writel_relaxed(EUD_DETECT, phy->eud_detect_reg);
	else
		writel_relaxed(readl_relaxed(phy->eud_detect_reg) & ~EUD_DETECT,
					phy->eud_detect_reg);
}

static int msm_m31_eusb2_phy_power(struct m31_eusb2_phy *phy, bool on)
{
	int ret = 0;

	dev_dbg(phy->phy.dev, "turn %s regulators. power_enabled:%d\n",
			on ? "on" : "off", phy->power_enabled);

	if (phy->power_enabled == on) {
		dev_dbg(phy->phy.dev, "PHYs' regulators are already ON.\n");
		return 0;
	}

	if (!on)
		goto clear_eud_det;

	ret = regulator_set_load(phy->vdd, USB_HSPHY_VDD_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdd:%d\n", ret);
		goto err_vdd;
	}

	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[1],
				    phy->vdd_levels[2]);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to set voltage for hsusb vdd\n");
		goto put_vdd_lpm;
	}

	ret = regulator_enable(phy->vdd);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable VDD\n");
		goto unconfig_vdd;
	}

	ret = regulator_set_load(phy->vdda12, USB_HSPHY_1P2_HPM_LOAD);
	if (ret < 0) {
		dev_err(phy->phy.dev, "Unable to set HPM of vdda12:%d\n", ret);
		goto disable_vdd;
	}

	ret = regulator_set_voltage(phy->vdda12, USB_HSPHY_1P2_VOL_MIN,
						USB_HSPHY_1P2_VOL_MAX);
	if (ret) {
		dev_err(phy->phy.dev,
				"Unable to set voltage for vdda12:%d\n", ret);
		goto put_vdda12_lpm;
	}

	ret = regulator_enable(phy->vdda12);
	if (ret) {
		dev_err(phy->phy.dev, "Unable to enable vdda12:%d\n", ret);
		goto unset_vdda12;
	}

	/* Make sure all the writes are processed before setting EUD_DETECT */
	mb();
	/* Set eud_detect_reg after powering on eUSB PHY rails to bring EUD out of reset */
	msm_m31_eusb2_phy_update_eud_detect(phy, true);

	phy->power_enabled = true;
	dev_dbg(phy->phy.dev, "eUSB2_PHY's regulators are turned ON.\n");
	return ret;

clear_eud_det:
	/* Clear eud_detect_reg to put EUD in reset */
	msm_m31_eusb2_phy_update_eud_detect(phy, false);

	/* Make sure clearing EUD_DETECT is completed before turning off the regulators */
	mb();

	ret = regulator_disable(phy->vdda12);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdda12:%d\n", ret);

unset_vdda12:
	ret = regulator_set_voltage(phy->vdda12, 0, USB_HSPHY_1P2_VOL_MAX);
	if (ret)
		dev_err(phy->phy.dev,
			"Unable to set (0) voltage for vdda12:%d\n", ret);

put_vdda12_lpm:
	ret = regulator_set_load(phy->vdda12, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdda12\n");

disable_vdd:
	ret = regulator_disable(phy->vdd);
	if (ret)
		dev_err(phy->phy.dev, "Unable to disable vdd:%d\n", ret);

unconfig_vdd:
	ret = regulator_set_voltage(phy->vdd, phy->vdd_levels[0],
				    phy->vdd_levels[2]);
	if (ret)
		dev_err(phy->phy.dev, "unable to set voltage for hsusb vdd\n");

put_vdd_lpm:
	ret = regulator_set_load(phy->vdd, 0);
	if (ret < 0)
		dev_err(phy->phy.dev, "Unable to set LPM of vdd\n");

	/* case handling when regulator turning on failed */
	if (!phy->power_enabled)
		return -EINVAL;

err_vdd:
	phy->power_enabled = false;
	dev_dbg(phy->phy.dev, "eusb2_PHY's regulators are turned OFF.\n");
	return ret;
}

static void msm_m31_eusb2_write_readback(void __iomem *base, u32 offset,
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
		pr_err("write: %x to offset: %x FAILED\n", val, offset);
}

static void msm_m31_eusb2_phy_reset(struct m31_eusb2_phy *phy)
{
	int ret;

	ret = reset_control_assert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "phy reset assert failed\n");

	usleep_range(100, 150);

	ret = reset_control_deassert(phy->phy_reset);
	if (ret)
		dev_err(phy->phy.dev, "phy reset deassert failed\n");
}

static void eusb2_phy_write_seq(struct m31_eusb2_phy *phy, u32 *seq, int cnt)
{
	int i;

	dev_dbg(phy->phy.dev, "Seq count:%d\n", cnt);
	for (i = 0; i < cnt; i = i + 2) {
		dev_dbg(phy->phy.dev,
		"write 0x%02x to 0x%02x\n", seq[i], seq[i + 1]);
		writel_relaxed(seq[i], phy->base + seq[i + 1]);
	}
}

static void msm_m31_eusb2_parameter_override(struct m31_eusb2_phy *phy)
{
	const struct eusb_phy_cfg *cfg = phy->cfg;
	const struct eusb_phy_tbl *tbl = cfg->override_seq;

	/* override init sequence using devicetree based values */
	eusb2_phy_write_seq(phy, phy->param_override_seq,
		phy->param_override_seq_cnt);
	/* override tune params using debugfs based values */

	/* USB_PHY_XCFGI_39_32 */
	if (phy->xcfgi_39_32 != 0xFF)
		msm_m31_eusb2_write_readback(phy->base, tbl[0].offset,
				tbl[0].bit_mask, phy->xcfgi_39_32 << 2);
	/* USB_PHY_XCFGI_71_64 */
	if (phy->xcfgi_71_64 != 0xFF)
		msm_m31_eusb2_write_readback(phy->base, tbl[1].offset,
				tbl[1].bit_mask, phy->xcfgi_71_64);
	/* USB_PHY_XCFGI_31_24 */
	if (phy->xcfgi_31_24 != 0xFF)
		msm_m31_eusb2_write_readback(phy->base, tbl[2].offset,
				tbl[2].bit_mask, phy->xcfgi_31_24);
	/* USB_PHY_XCFGI_7_0 */
	if (phy->xcfgi_7_0 != 0xFF)
		msm_m31_eusb2_write_readback(phy->base, tbl[3].offset,
				tbl[3].bit_mask, phy->xcfgi_7_0);

}

static void msm_m31_eusb2_ref_clk_init(struct usb_phy *uphy)
{
	struct m31_eusb2_phy *phy = container_of(uphy, struct m31_eusb2_phy, phy);

	/* setting to 19.2 mHz as per the ref_clk frequency */
	msm_m31_eusb2_write_readback(phy->base, USB_PHY_HS_PHY_CTRL_COMMON0,
						FSEL, FSEL_19_2_MHZ_VAL);

}

static int msm_m31_eusb2_repeater_reset_and_init(struct m31_eusb2_phy *phy)
{
	int ret;

	ret = usb_repeater_powerup(phy->ur);
	if (ret)
		dev_err(phy->phy.dev, "repeater powerup failed.\n");

	ret = usb_repeater_reset(phy->ur, true);
	if (ret)
		dev_err(phy->phy.dev, "repeater reset failed.\n");

	ret = usb_repeater_init(phy->ur);
	if (ret)
		dev_err(phy->phy.dev, "repeater init failed.\n");

	return ret;
}


static void msm_m31_eusb2_phy_write_configs(struct m31_eusb2_phy *phy,
					const struct eusb_phy_tbl tbl[],
					int num)
{
	int i;
	const struct eusb_phy_tbl *t = tbl;

	for (i = 0 ; i < num; i++, t++) {
		dev_dbg(phy->phy.dev, "Offset:%x BitMask:%x Value:%x",
					t->offset, t->bit_mask, t->val);

		/* for dbg, in case the values is greater than the offset */
		BUG_ON((t->val << __ffs(t->bit_mask)) > t->bit_mask);

		msm_m31_eusb2_write_readback(phy->base,
					t->offset, t->bit_mask,
					t->val << __ffs(t->bit_mask));
	}
}

static int msm_m31_eusb2_phy_init(struct usb_phy *uphy)
{
	struct m31_eusb2_phy *phy = container_of(uphy, struct m31_eusb2_phy, phy);
	int ret;
	const struct eusb_phy_cfg *cfg = phy->cfg;

	dev_dbg(uphy->dev, "phy_flags:%x\n", phy->phy.flags);
	if (is_eud_debug_mode_active(phy)) {
		/* if in host mode, disable EUD debug mode */
		if (phy->phy.flags & PHY_HOST_MODE) {
			qcom_scm_io_writel(phy->eud_reg, 0x0);
			phy->re_enable_eud = true;
		} else {
			msm_m31_eusb2_phy_power(phy, true);
			return msm_m31_eusb2_repeater_reset_and_init(phy);
		}
	}

	ret = msm_m31_eusb2_phy_power(phy, true);
	if (ret)
		return ret;

	ret = msm_m31_eusb2_repeater_reset_and_init(phy);
	if (ret) {
		dev_err(phy->phy.dev, "repeater powerup failed.\n");
		return ret;
	}

	msm_m31_eusb2_phy_clocks(phy, true);

	msm_m31_eusb2_phy_reset(phy);

	msm_m31_eusb2_phy_write_configs(phy, cfg->init_seq, cfg->init_seq_num);
	msm_m31_eusb2_ref_clk_init(uphy);
	msm_m31_eusb2_phy_write_configs(phy, cfg->override_seq, cfg->override_seq_num);
	msm_m31_eusb2_parameter_override(phy);
	msm_m31_eusb2_phy_write_configs(phy, cfg->reset_seq, cfg->reset_seq_num);

	return 0;
}

static int msm_m31_eusb2_phy_set_suspend(struct usb_phy *uphy, int suspend)
{
	struct m31_eusb2_phy *phy = container_of(uphy, struct m31_eusb2_phy, phy);

	if (phy->suspended && suspend) {
		dev_dbg(uphy->dev, "USB PHY is already suspended\n");
		return 0;
	}

	dev_dbg(uphy->dev, "phy->flags:0x%x\n", phy->phy.flags);
	if (suspend) {
		/* Bus suspend handling */
		if (phy->cable_connected ||
			(phy->phy.flags & PHY_HOST_MODE)) {
			msm_m31_eusb2_phy_clocks(phy, false);
			/*
			 * Keep the ref_clk for PHY on to detect resume signalling in bus
			 * suspend case. As this vote is suppressible, this will allow XO
			 * shutdown.
			 */
			if (phy->ref_clk && !phy->ref_clk_enable) {
				phy->ref_clk_enable = true;
				clk_prepare_enable(phy->ref_clk);
			}

			goto suspend_exit;
		}

		/* Cable disconnect handling */
		if (phy->re_enable_eud) {
			dev_dbg(uphy->dev, "re-enabling EUD\n");
			qcom_scm_io_writel(phy->eud_reg, 0x1);
			phy->re_enable_eud = false;
		}

		/* With EUD spoof disconnect, keep clk and ldos on */
		if (phy->phy.flags & EUD_SPOOF_DISCONNECT)
			goto suspend_exit;

		if (phy->ref_clk && phy->ref_clk_enable) {
			clk_disable_unprepare(phy->ref_clk);
			phy->ref_clk_enable = false;
		}

		msm_m31_eusb2_phy_clocks(phy, false);
		msm_m31_eusb2_phy_power(phy, false);

		/* Hold repeater into reset after powering down PHY */
		usb_repeater_reset(phy->ur, false);
		usb_repeater_powerdown(phy->ur);
	} else {
		/* Bus resume and cable connect handling */
		msm_m31_eusb2_phy_clocks(phy, true);
	}

suspend_exit:
	phy->suspended = !!suspend;
	return 0;
}

static int msm_m31_eusb2_phy_notify_connect(struct usb_phy *uphy,
				    enum usb_device_speed speed)
{
	struct m31_eusb2_phy *phy = container_of(uphy, struct m31_eusb2_phy, phy);

	phy->cable_connected = true;

	return 0;
}

static int msm_m31_eusb2_phy_notify_disconnect(struct usb_phy *uphy,
				       enum usb_device_speed speed)
{
	struct m31_eusb2_phy *phy = container_of(uphy, struct m31_eusb2_phy, phy);

	phy->cable_connected = false;
	return 0;
}

static void msm_m31_eusb2_phy_vbus_draw_work(struct work_struct *w)
{
	struct m31_eusb2_phy *phy = container_of(w, struct m31_eusb2_phy,
							vbus_draw_work);
	union power_supply_propval val = {0};
	int ret;

	if (!phy->usb_psy) {
		phy->usb_psy = power_supply_get_by_name("usb");
		if (!phy->usb_psy) {
			dev_err(phy->phy.dev, "Could not get usb psy\n");
			return;
		}
	}

	dev_info(phy->phy.dev, "Avail curr from USB = %u\n", phy->vbus_draw);
	/* Set max current limit in uA */
	val.intval = 1000 * phy->vbus_draw;
	ret = power_supply_set_property(phy->usb_psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);
	if (ret) {
		dev_dbg(phy->phy.dev, "Error setting ICL:(%d)\n", ret);
		return;
	}
}

static int msm_m31_eusb2_phy_set_power(struct usb_phy *uphy, unsigned int mA)
{
	struct m31_eusb2_phy *phy = container_of(uphy, struct m31_eusb2_phy, phy);

	phy->vbus_draw = mA;
	schedule_work(&phy->vbus_draw_work);

	return 0;
}

static void msm_m31_eusb2_phy_create_debugfs(struct m31_eusb2_phy *phy)
{
	phy->root = debugfs_create_dir(dev_name(phy->phy.dev), NULL);

	debugfs_create_x8("xcfgi_39_32", 0644, phy->root,
					&phy->xcfgi_39_32);
	phy->xcfgi_39_32 = 0xFF;

	debugfs_create_x8("xcfgi_71_64", 0644, phy->root,
					&phy->xcfgi_71_64);
	phy->xcfgi_71_64 = 0xFF;

	debugfs_create_x8("xcfgi_31_24", 0644, phy->root,
					&phy->xcfgi_31_24);
	phy->xcfgi_31_24 = 0xFF;

	debugfs_create_x8("xcfgi_7_0", 0644, phy->root,
					&phy->xcfgi_7_0);
	phy->xcfgi_7_0 = 0xFF;
}

static int msm_m31_eusb2_phy_probe(struct platform_device *pdev)
{
	struct m31_eusb2_phy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	struct usb_repeater *ur;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		ret = -ENOMEM;
		goto err_ret;
	}

	phy->cfg = of_device_get_match_data(dev);
	if (!phy->cfg) {
		ret = -ENODEV;
		goto err_ret;
	}

	ur = devm_usb_get_repeater_by_phandle(dev, "usb-repeater", 0);
	if (IS_ERR(ur)) {
		dev_dbg(dev, "Repeater not available!\n");
		ret = PTR_ERR(ur);
		goto err_ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"eusb2_phy_base");
	if (!res) {
		dev_err(dev, "missing eusb2phy memory resource\n");
		ret = -ENODEV;
		goto err_ret;
	}

	phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->base)) {
		dev_err(dev, "ioremap failed\n");
		ret = PTR_ERR(phy->base);
		goto err_ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "eud_enable_reg");
	if (res) {
		phy->eud_enable_reg = devm_ioremap_resource(dev, res);
		if (IS_ERR(phy->eud_enable_reg)) {
			ret = PTR_ERR(phy->eud_enable_reg);
			dev_err(dev, "eud_enable_reg ioremap err:%d\n", ret);
			goto err_ret;
		}
		phy->eud_reg = res->start;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "eud_detect_reg");
	if (!res) {
		dev_err(dev, "missing eud_detect register address\n");
		ret = -ENODEV;
		goto err_ret;
	}

	phy->eud_detect_reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->eud_detect_reg)) {
		ret = PTR_ERR(phy->eud_detect_reg);
		dev_err(dev, "eud_detect_reg ioremap err:%d\n", ret);
		goto err_ret;
	}

	phy->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(phy->ref_clk_src)) {
		dev_dbg(dev, "clk get failed for ref_clk_src\n");
		ret = PTR_ERR(phy->ref_clk_src);
		goto err_ret;
	}

	phy->ref_clk = devm_clk_get_optional(dev, "ref_clk");
	if (IS_ERR(phy->ref_clk)) {
		dev_dbg(dev, "clk get failed for ref_clk\n");
		ret = PTR_ERR(phy->ref_clk);
		goto err_ret;
	}

	phy->phy_reset = devm_reset_control_get(dev, "phy_reset");
	if (IS_ERR(phy->phy_reset)) {
		ret = PTR_ERR(phy->phy_reset);
		goto err_ret;
	}

	ret = of_property_read_variable_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels, 0,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret < 2) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		goto err_ret;
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		ret = PTR_ERR(phy->vdd);
		goto err_ret;
	}

	phy->vdda12 = devm_regulator_get(dev, "vdda12");
	if (IS_ERR(phy->vdda12)) {
		dev_err(dev, "unable to get vdda12 supply\n");
		ret = PTR_ERR(phy->vdda12);
		goto err_ret;
	}

	phy->param_override_seq_cnt = of_property_count_elems_of_size(
					dev->of_node, "qcom,param-override-seq",
					sizeof(*phy->param_override_seq));

	if (phy->param_override_seq_cnt > 0) {
		if (phy->param_override_seq_cnt % 2) {
			dev_err(dev, "invalid param_override_seq_len\n");
			ret = -EINVAL;
			goto err_ret;
		}

		phy->param_override_seq = devm_kcalloc(dev,
				phy->param_override_seq_cnt,
				sizeof(*phy->param_override_seq),
				GFP_KERNEL);
		if (!phy->param_override_seq) {
			ret = -ENOMEM;
			goto err_ret;
		}

		ret = of_property_read_variable_u32_array(dev->of_node,
				"qcom,param-override-seq",
				phy->param_override_seq, 0,
				phy->param_override_seq_cnt);
		if (ret % 2) {
			dev_err(dev, "qcom,param-override-seq read failed %d\n",
				ret);
			goto err_ret;
		}
	}

	phy->ur = ur;
	phy->phy.dev = dev;
	platform_set_drvdata(pdev, phy);

	phy->phy.init			= msm_m31_eusb2_phy_init;
	phy->phy.set_suspend		= msm_m31_eusb2_phy_set_suspend;
	phy->phy.notify_connect		= msm_m31_eusb2_phy_notify_connect;
	phy->phy.notify_disconnect	= msm_m31_eusb2_phy_notify_disconnect;
	phy->phy.set_power		= msm_m31_eusb2_phy_set_power;
	phy->phy.type			= USB_PHY_TYPE_USB2;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		goto err_ret;

	INIT_WORK(&phy->vbus_draw_work, msm_m31_eusb2_phy_vbus_draw_work);
	msm_m31_eusb2_phy_create_debugfs(phy);
	/*
	 * EUD may be enabled in boot loader and to keep EUD session alive across
	 * kernel boot till USB phy driver is initialized based on cable status,
	 * keep LDOs on here.
	 */
	if (is_eud_debug_mode_active(phy))
		msm_m31_eusb2_phy_power(phy, true);

	dev_dbg(dev, "M31 Phy Probed");
	return 0;

err_ret:
	return ret;
}

static int msm_m31_eusb2_phy_remove(struct platform_device *pdev)
{
	struct m31_eusb2_phy *phy = platform_get_drvdata(pdev);

	flush_work(&phy->vbus_draw_work);
	if (phy->usb_psy)
		power_supply_put(phy->usb_psy);

	debugfs_remove_recursive(phy->root);
	usb_remove_phy(&phy->phy);
	if (phy->ref_clk)
		clk_disable_unprepare(phy->ref_clk);
	msm_m31_eusb2_phy_clocks(phy, false);
	msm_m31_eusb2_phy_power(phy, false);
	return 0;
}

static const struct of_device_id msm_usb_id_table[] = {
	{
		.compatible = "qcom,usb-m31-eusb2-phy",
		.data = &m31_eusb_phy_cfg,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, msm_usb_id_table);

static struct platform_driver msm_m31_eusb2_phy_driver = {
	.probe		= msm_m31_eusb2_phy_probe,
	.remove		= msm_m31_eusb2_phy_remove,
	.driver = {
		.name	= "msm_m31_eusb2_phy",
		.of_match_table = of_match_ptr(msm_usb_id_table),
	},
};

module_platform_driver(msm_m31_eusb2_phy_driver);
MODULE_DESCRIPTION("MSM USB M31 eUSB2 PHY driver");
MODULE_LICENSE("GPL v2");

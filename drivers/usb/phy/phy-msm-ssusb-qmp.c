/*
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#define INIT_MAX_TIME_USEC			1000

#define USB_SSPHY_1P8_VOL_MIN		1800000 /* uV */
#define USB_SSPHY_1P8_VOL_MAX		1800000 /* uV */
#define USB_SSPHY_1P8_HPM_LOAD		23000	/* uA */


/* USB3PHY_PCIE_USB3_PCS_PCS_STATUS bit */
#define PHYSTATUS				BIT(6)

/* TCSR_PHY_CLK_SCHEME_SEL bit mask */
#define PHY_CLK_SCHEME_SEL BIT(0)

/* PCIE_USB3_PHY_AUTONOMOUS_MODE_CTRL bits */
#define ARCVR_DTCT_EN		BIT(0)
#define ALFPS_DTCT_EN		BIT(1)
#define ARCVR_DTCT_EVENT_SEL	BIT(4)

enum qmp_phy_rev_reg {
	USB3_REVISION_ID0,
	USB3_REVISION_ID1,
	USB3_REVISION_ID2,
	USB3_REVISION_ID3,
	USB3_PHY_PCS_STATUS,
	USB3_PHY_AUTONOMOUS_MODE_CTRL,
	USB3_PHY_LFPS_RXTERM_IRQ_CLEAR,
	USB3_PHY_POWER_DOWN_CONTROL,
	USB3_PHY_SW_RESET,
	USB3_PHY_START,
	USB3_PHY_REG_MAX,
};

/* QMP PHY register offset for rev1 */
unsigned int qmp_phy_rev1[] = {
	[USB3_REVISION_ID0] = 0x730,
	[USB3_REVISION_ID1] = 0x734,
	[USB3_REVISION_ID2] = 0x738,
	[USB3_REVISION_ID3] = 0x73c,
	[USB3_PHY_PCS_STATUS] = 0x728,
	[USB3_PHY_AUTONOMOUS_MODE_CTRL] = 0x6BC,
	[USB3_PHY_LFPS_RXTERM_IRQ_CLEAR] = 0x6C0,
	[USB3_PHY_POWER_DOWN_CONTROL] = 0x604,
	[USB3_PHY_SW_RESET] = 0x600,
	[USB3_PHY_START] = 0x608,
};

/* QMP PHY register offset for rev2 */
unsigned int qmp_phy_rev2[] = {
	[USB3_REVISION_ID0] = 0x788,
	[USB3_REVISION_ID1] = 0x78C,
	[USB3_REVISION_ID2] = 0x790,
	[USB3_REVISION_ID3] = 0x794,
	[USB3_PHY_PCS_STATUS] = 0x77C,
	[USB3_PHY_AUTONOMOUS_MODE_CTRL] = 0x6D4,
	[USB3_PHY_LFPS_RXTERM_IRQ_CLEAR] = 0x6D8,
	[USB3_PHY_POWER_DOWN_CONTROL] = 0x604,
	[USB3_PHY_SW_RESET] = 0x600,
	[USB3_PHY_START] = 0x608,
};

/* reg values to write based on the phy clk scheme selected */
struct qmp_reg_val {
	u32 offset;
	u32 diff_clk_sel_val;
	u32 se_clk_sel_val;
	u32 delay;
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

	{-1, 0x00} /* terminating entry */
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

	{-1, 0x00} /* terminating entry */
};

/* USB3PHY_REVISION_ID3 = 0x20 where register offset is being changed. */
static const struct qmp_reg_val qmp_settings_rev2[] = {
	/* Common block settings */
	{0xAC, 0x14}, /* QSERDES_COM_SYSCLK_EN_SEL */
	{0x34, 0x08}, /* QSERDES_COM_BIAS_EN_CLKBUFLR_EN */
	{0x174, 0x30}, /* QSERDES_COM_CLK_SELECT */
	{0x194, 0x06}, /* QSERDES_COM_CMN_CONFIG */
	{0x19C, 0x01}, /* QSERDES_COM_SVS_MODE_CLK_SEL */
	{0x178, 0x00}, /* QSERDES_COM_HSCLK_SEL */
	{0x70, 0x0F}, /* USB3PHY_QSERDES_COM_BG_TRIM */
	{0x48, 0x0F}, /* USB3PHY_QSERDES_COM_PLL_IVCO */
	{0x3C, 0x04}, /* QSERDES_COM_SYS_CLK_CTRL */

	/* PLL and Loop filter settings */
	{0xD0, 0x82}, /* QSERDES_COM_DEC_START_MODE0 */
	{0xDC, 0x55}, /* QSERDES_COM_DIV_FRAC_START1_MODE0 */
	{0xE0, 0x55}, /* QSERDES_COM_DIV_FRAC_START2_MODE0 */
	{0xE4, 0x03}, /* QSERDES_COM_DIV_FRAC_START3_MODE0 */
	{0x78, 0x0B}, /* QSERDES_COM_CP_CTRL_MODE0 */
	{0x84, 0x16}, /* QSERDES_COM_PLL_RCTRL_MODE0 */
	{0x90, 0x28}, /* QSERDES_COM_PLL_CCTRL_MODE0 */
	{0x108, 0x80}, /*QSERDES_COM_INTEGLOOP_GAIN0_MODE0 */
	{0x124, 0x00}, /* USB3PHY_QSERDES_COM_VCO_TUNE_CTRL */
	{0x4C, 0x15}, /* QSERDES_COM_LOCK_CMP1_MODE0 */
	{0x50, 0x34}, /* QSERDES_COM_LOCK_CMP2_MODE0 */
	{0x54, 0x00}, /* QSERDES_COM_LOCK_CMP3_MODE0 */
	{0x18C, 0x00}, /* QSERDES_COM_CORE_CLK_EN */
	{0xCC, 0x00}, /* QSERDES_COM_LOCK_CMP_CFG */
	{0x128, 0x00}, /* QSERDES_COM_VCO_TUNE_MAP */
	{0x0C, 0x0A}, /* QSERDES_COM_BG_TIMER */

	/* SSC settings */
	{0x10, 0x01}, /* QSERDES_COM_SSC_EN_CENTER */
	{0x1C, 0x31}, /* QSERDES_COM_SSC_PER1 */
	{0x20, 0x01}, /* QSERDES_COM_SSC_PER2 */
	{0x14, 0x00}, /* QSERDES_COM_SSC_ADJ_PER1 */
	{0x18, 0x00}, /* QSERDES_COM_SSC_ADJ_PER2 */
	{0x24, 0xDE}, /* QSERDES_COM_SSC_STEP_SIZE1 */
	{0x28, 0x07}, /* QSERDES_COM_SSC_STEP_SIZE2 */

	/* Rx settings */
	{0x440, 0x0B}, /* QSERDES_RX_UCDR_FASTLOCK_FO_GAIN */
	{0x41C, 0x04}, /* QSERDES_RX_UCDR_SO_GAIN */
	{0x4D8, 0x02}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2 */
	{0x4DC, 0x4C}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3 */
	{0x4E0, 0xBB}, /* QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4 */
	{0x508, 0x77}, /* QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1 */
	{0x50C, 0x80}, /* QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2 */
	{0x514, 0x03}, /* QSERDES_RX_SIGDET_CNTRL */
	{0x518, 0x18}, /* QSERDES_RX_SIGDET_LVL */
	{0x51C, 0x16}, /* QSERDES_RX_SIGDET_DEGLITCH_CNTRL */

	/* TX settings */
	{0x268, 0x45}, /* QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN */
	{0x2AC, 0x12}, /* QSERDES_TX_RCV_DETECT_LVL_2 */
	{0x294, 0x06}, /* QSERDES_TX_LANE_MODE */

	/* FLL settings */
	{0x6C4, 0x03}, /* USB3_PHY_FLL_CNTRL2 */
	{0x6C0, 0x02}, /* USB3_PHY_FLL_CNTRL1 */
	{0x6C8, 0x09}, /* USB3_PHY_FLL_CNT_VAL_L */
	{0x6CC, 0x42}, /* USB3_PHY_FLL_CNT_VAL_H_TOL */
	{0x6D0, 0x85}, /* USB3_PHY_FLL_MAN_CODE */

	/* Lock Det settings */
	{0x680, 0xD1}, /* USB3_PHY_LOCK_DETECT_CONFIG1 */
	{0x684, 0x1F}, /* USB3_PHY_LOCK_DETECT_CONFIG2 */
	{0x688, 0x47}, /* USB3_PHY_LOCK_DETECT_CONFIG3 */
	{0x664, 0x08}, /* USB3_PHY_POWER_STATE_CONFIG2 */

	{-1, 0x00} /* terminating entry */
};

/* Override for QMP PHY revision 2 */
static const struct qmp_reg_val qmp_settings_rev2_misc[] = {
	{0x178, 0x01}, /* QSERDES_COM_HSCLK_SEL */

	/* Rx settings */
	{0x518, 0x1B}, /* QSERDES_RX_SIGDET_LVL */

	/* Res_code settings */
	{0xC4, 0x15}, /* USB3PHY_QSERDES_COM_RESCODE_DIV_NUM */
	{0x1B8, 0x1F}, /* QSERDES_COM_CMN_MISC2 */
	{-1, 0x00} /* terminating entry */
};

/* Override PLL Calibration */
static const struct qmp_reg_val qmp_override_pll[] = {
	{0x04, 0xE1}, /* QSERDES_COM_PLL_VCOTAIL_EN */
	{0x50, 0x07}, /* QSERDES_COM_RESETSM_CNTRL2 */
	{-1, 0x00} /* terminating entry */
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
	{-1, 0x00} /* terminating entry */
};

/* Vbg related settings */
static const struct qmp_reg_val qmp_settings_rev1_misc[] = {
	{0x0C, 0x03}, /* QSERDES_COM_IE_TRIM */
	{0x10, 0x00}, /* QSERDES_COM_IP_TRIM */
	{0xA0, 0xFF}, /* QSERDES_COM_BGTC */
	{-1, 0x00} /* terminating entry */
};

struct msm_ssphy_qmp {
	struct usb_phy		phy;
	void __iomem		*base;
	void __iomem		*vls_clamp_reg;
	void __iomem		*tcsr_phy_clk_scheme_sel;

	struct regulator	*vdd;
	struct regulator	*vdda18;
	int			vdd_levels[3]; /* none, low, high */
	struct clk		*ref_clk_src;
	struct clk		*ref_clk;
	struct clk		*aux_clk;
	struct clk		*cfg_ahb_clk;
	struct clk		*pipe_clk;
	struct clk		*phy_reset;
	struct clk		*phy_phy_reset;
	bool			clk_enabled;
	bool			cable_connected;
	bool			in_suspend;
	bool			override_pll_cal;
	bool			emulation;
	bool			misc_config;
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
		.data = qmp_phy_rev1,
	},
	{
		.compatible = "qcom,usb-ssphy-qmp-v2",
		.data = qmp_phy_rev2,
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

static int configure_phy_regs(struct usb_phy *uphy,
				const struct qmp_reg_val *reg)
{
	struct msm_ssphy_qmp *phy = container_of(uphy, struct msm_ssphy_qmp,
					phy);
	u32 val;
	bool diff_clk_sel = true;

	if (!reg) {
		dev_err(uphy->dev, "NULL PHY configuration\n");
		return -EINVAL;
	}

	if (phy->tcsr_phy_clk_scheme_sel) {
		val = readl_relaxed(phy->tcsr_phy_clk_scheme_sel);
		if (val & PHY_CLK_SCHEME_SEL) {
			pr_debug("%s:Single Ended clk scheme is selected\n",
				__func__);
			diff_clk_sel = false;
		}
	}

	while (reg->offset != -1) {
		writel_relaxed(diff_clk_sel ?
				reg->diff_clk_sel_val : reg->se_clk_sel_val,
				phy->base + reg->offset);
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
	int ret;
	unsigned init_timeout_usec = INIT_MAX_TIME_USEC;
	u32 revid;
	const struct qmp_reg_val *reg = NULL, *misc = NULL, *pll = NULL;

	dev_dbg(uphy->dev, "Initializing QMP phy\n");

	if (phy->emulation)
		return 0;

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

	/* Rev ID is made up each of the LSBs of REVISION_ID[0-3] */
	revid = (readl_relaxed(phy->base +
			phy->phy_reg[USB3_REVISION_ID3]) & 0xFF) << 24;
	revid |= (readl_relaxed(phy->base +
			phy->phy_reg[USB3_REVISION_ID2]) & 0xFF) << 16;
	revid |= (readl_relaxed(phy->base +
			phy->phy_reg[USB3_REVISION_ID1]) & 0xFF) << 8;
	revid |= readl_relaxed(phy->base +
			phy->phy_reg[USB3_REVISION_ID0]) & 0xFF;

	pll = qmp_override_pll;

	switch (revid) {
	case 0x10000000:
		reg = qmp_settings_rev0;
		misc = qmp_settings_rev0_misc;
		break;
	case 0x10000001:
		reg = qmp_settings_rev1;
		misc = qmp_settings_rev1_misc;
		break;
	case 0x20000000:
	case 0x20000001:
		reg = qmp_settings_rev2;
		misc = qmp_settings_rev2_misc;
		break;
	default:
		dev_err(uphy->dev, "Unknown revid 0x%x, cannot initialize PHY\n",
			revid);
		return -ENODEV;
	}

	if (phy->qmp_phy_init_seq)
		reg = (struct qmp_reg_val *)phy->qmp_phy_init_seq;

	writel_relaxed(0x01,
		phy->base + phy->phy_reg[USB3_PHY_POWER_DOWN_CONTROL]);

	/* Make sure that above write completed to get PHY into POWER DOWN */
	mb();

	/* Main configuration */
	ret = configure_phy_regs(uphy, reg);
	if (ret) {
		dev_err(uphy->dev, "Failed the main PHY configuration\n");
		return ret;
	}

	/* Feature specific configurations */
	if (phy->override_pll_cal) {
		ret = configure_phy_regs(uphy, pll);
		if (ret) {
			dev_err(uphy->dev,
				"Failed the PHY PLL override configuration\n");
			return ret;
		}
	}
	if (phy->misc_config) {
		ret = configure_phy_regs(uphy, misc);
		if (ret) {
			dev_err(uphy->dev, "Failed the misc PHY configuration\n");
			return ret;
		}
	}

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
	if (phy->phy_phy_reset) {
		ret = clk_reset(phy->phy_phy_reset, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(uphy->dev, "phy_phy reset assert failed\n");
			goto exit;
		}
	} else {
		ret = clk_reset(phy->pipe_clk, CLK_RESET_ASSERT);
		if (ret) {
			dev_err(uphy->dev, "pipe_clk reset assert failed\n");
			goto exit;
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
			goto exit;
		}
	} else {
		ret = clk_reset(phy->pipe_clk, CLK_RESET_DEASSERT);
		if (ret) {
			dev_err(uphy->dev, "pipe_clk reset deassert failed\n");
			goto exit;
		}
	}

	return 0;

deassert_phy_phy_reset:
	if (phy->phy_phy_reset)
		clk_reset(phy->phy_phy_reset, CLK_RESET_DEASSERT);
	else
		clk_reset(phy->pipe_clk, CLK_RESET_DEASSERT);
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
	int ret = 0, size = 0;
	const struct of_device_id *phy_ver;

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

	phy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb_clk");
	if (IS_ERR(phy->cfg_ahb_clk)) {
		ret = PTR_ERR(phy->cfg_ahb_clk);
		phy->cfg_ahb_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get cfg_ahb_clk\n");
		goto err;
	}

	phy->pipe_clk = devm_clk_get(dev, "pipe_clk");
	if (IS_ERR(phy->pipe_clk)) {
		ret = PTR_ERR(phy->pipe_clk);
		phy->pipe_clk = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get pipe_clk\n");
		goto err;
	}

	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "phy_reset") >= 0) {
		phy->phy_reset = clk_get(&pdev->dev, "phy_reset");
		if (IS_ERR(phy->phy_reset)) {
			ret = PTR_ERR(phy->phy_reset);
			phy->phy_reset = NULL;
			dev_dbg(dev, "failed to get phy_reset\n");
			goto err;
		}
	}

	if (of_property_match_string(pdev->dev.of_node,
				"clock-names", "phy_phy_reset") >= 0) {
		phy->phy_phy_reset = clk_get(dev, "phy_phy_reset");
		if (IS_ERR(phy->phy_phy_reset)) {
			ret = PTR_ERR(phy->phy_phy_reset);
			phy->phy_phy_reset = NULL;
			dev_dbg(dev, "phy_phy_reset unavailable\n");
			goto err;
		}
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
		}
	}

	phy_ver = of_match_device(msm_usb_id_table, &pdev->dev);
	if (phy_ver) {
		dev_dbg(dev, "Found QMP PHY version as:%s.\n",
						phy_ver->compatible);
		if (phy_ver->data) {
			phy->phy_reg = (unsigned int *)phy_ver->data;
		} else if (phy->qmp_phy_reg_offset) {
			phy->phy_reg = phy->qmp_phy_reg_offset;
		} else {
			dev_err(dev,
				"QMP PHY version match but wrong data val.\n");
			ret = -EINVAL;
		}
	} else {
		dev_err(dev, "QMP PHY version mismatch.\n");
		ret = -ENODEV;
	}

	if (ret)
		goto err;

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
			"tcsr_phy_clk_scheme_sel");
	if (res) {
		phy->tcsr_phy_clk_scheme_sel = devm_ioremap_nocache(dev,
				res->start, resource_size(res));
		if (IS_ERR(phy->tcsr_phy_clk_scheme_sel))
			dev_dbg(dev, "err reading tcsr_phy_clk_scheme_sel\n");
	}

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
		}
	}

	phy->emulation = of_property_read_bool(dev->of_node,
						"qcom,emulation");

	ret = of_property_read_u32_array(dev->of_node, "qcom,vdd-voltage-level",
					 (u32 *) phy->vdd_levels,
					 ARRAY_SIZE(phy->vdd_levels));
	if (ret) {
		dev_err(dev, "error reading qcom,vdd-voltage-level property\n");
		goto err;
	}

	phy->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(phy->vdd)) {
		dev_err(dev, "unable to get vdd supply\n");
		ret = PTR_ERR(phy->vdd);
		goto err;
	}

	phy->vdda18 = devm_regulator_get(dev, "vdda18");
	if (IS_ERR(phy->vdda18)) {
		dev_err(dev, "unable to get vdda18 supply\n");
		ret = PTR_ERR(phy->vdda18);
		goto err;
	}

	ret = msm_ssusb_qmp_config_vdd(phy, 1);
	if (ret) {
		dev_err(dev, "ssusb vdd_dig configuration failed\n");
		goto err;
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

	phy->ref_clk_src = devm_clk_get(dev, "ref_clk_src");
	if (IS_ERR(phy->ref_clk_src))
		phy->ref_clk_src = NULL;
	phy->ref_clk = devm_clk_get(dev, "ref_clk");
	if (IS_ERR(phy->ref_clk))
		phy->ref_clk = NULL;

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

	phy->phy.dev			= dev;
	phy->phy.init			= msm_ssphy_qmp_init;
	phy->phy.set_suspend		= msm_ssphy_qmp_set_suspend;
	phy->phy.notify_connect		= msm_ssphy_qmp_notify_connect;
	phy->phy.notify_disconnect	= msm_ssphy_qmp_notify_disconnect;
	phy->phy.reset			= msm_ssphy_qmp_reset;
	phy->phy.type			= USB_PHY_TYPE_USB3;

	ret = usb_add_phy_dev(&phy->phy);
	if (ret)
		goto disable_ss_ldo;
	return 0;

disable_ss_ldo:
	msm_ssusb_qmp_ldo_enable(phy, 0);
disable_ss_vdd:
	regulator_disable(phy->vdd);
unconfig_ss_vdd:
	msm_ssusb_qmp_config_vdd(phy, 0);
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
	regulator_disable(phy->vdd);
	msm_ssusb_qmp_config_vdd(phy, 0);
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

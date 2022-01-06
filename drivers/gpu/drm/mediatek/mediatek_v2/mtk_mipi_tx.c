// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include "mtk_log.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_panel_ext.h"
#include "mtk_dump.h"
#include "mtk_mipi_tx.h"
#include "platform/mtk_drm_6789.h"

#define MIPITX_DSI_CON 0x00
#define RG_DSI_LDOCORE_EN BIT(0)
#define RG_DSI_CKG_LDOOUT_EN BIT(1)
#define RG_DSI_BCLK_SEL (3 << 2)
#define RG_DSI_LD_IDX_SEL (7 << 4)
#define RG_DSI_PHYCLK_SEL (2 << 8)
#define RG_DSI_DSICLK_FREQ_SEL BIT(10)
#define RG_DSI_LPTX_CLMP_EN BIT(11)

#define MIPITX_DSI_CLOCK_LANE 0x04
#define MIPITX_DSI_DATA_LANE0 0x08
#define MIPITX_DSI_DATA_LANE1 0x0c
#define MIPITX_DSI_DATA_LANE2 0x10
#define MIPITX_DSI_DATA_LANE3 0x14
#define RG_DSI_LNTx_LDOOUT_EN BIT(0)
#define RG_DSI_LNTx_CKLANE_EN BIT(1)
#define RG_DSI_LNTx_LPTX_IPLUS1 BIT(2)
#define RG_DSI_LNTx_LPTX_IPLUS2 BIT(3)
#define RG_DSI_LNTx_LPTX_IMINUS BIT(4)
#define RG_DSI_LNTx_LPCD_IPLUS BIT(5)
#define RG_DSI_LNTx_LPCD_IMINUS BIT(6)
#define RG_DSI_LNTx_RT_CODE (0xf << 8)

#define MIPITX_DSI_TOP_CON 0x40
#define RG_DSI_LNT_INTR_EN BIT(0)
#define RG_DSI_LNT_HS_BIAS_EN BIT(1)
#define RG_DSI_LNT_IMP_CAL_EN BIT(2)
#define RG_DSI_LNT_TESTMODE_EN BIT(3)
#define RG_DSI_LNT_IMP_CAL_CODE (0xf << 4)
#define RG_DSI_LNT_AIO_SEL (7 << 8)
#define RG_DSI_PAD_TIE_LOW_EN BIT(11)
#define RG_DSI_DEBUG_INPUT_EN BIT(12)
#define RG_DSI_PRESERVE (7 << 13)

#define MIPITX_DSI_BG_CON 0x44
#define RG_DSI_BG_CORE_EN BIT(0)
#define RG_DSI_BG_CKEN BIT(1)
#define RG_DSI_BG_DIV (0x3 << 2)
#define RG_DSI_BG_FAST_CHARGE BIT(4)
#define RG_DSI_VOUT_MSK (0x3ffff << 5)
#define RG_DSI_V12_SEL (7 << 5)
#define RG_DSI_V10_SEL (7 << 8)
#define RG_DSI_V072_SEL (7 << 11)
#define RG_DSI_V04_SEL (7 << 14)
#define RG_DSI_V032_SEL (7 << 17)
#define RG_DSI_V02_SEL (7 << 20)
#define RG_DSI_BG_R1_TRIM (0xf << 24)
#define RG_DSI_BG_R2_TRIM (0xf << 28)

#define MIPITX_DSI_PLL_CON0 0x50
#define RG_DSI_MPPLL_PLL_EN BIT(0)
#define RG_DSI_MPPLL_DIV_MSK (0x1ff << 1)
#define RG_DSI_MPPLL_PREDIV (3 << 1)
#define RG_DSI_MPPLL_TXDIV0 (3 << 3)
#define RG_DSI_MPPLL_TXDIV1 (3 << 5)
#define RG_DSI_MPPLL_POSDIV (7 << 7)
#define RG_DSI_MPPLL_MONVC_EN BIT(10)
#define RG_DSI_MPPLL_MONREF_EN BIT(11)
#define RG_DSI_MPPLL_VOD_EN BIT(12)

#define MIPITX_DSI_PLL_CON1 0x54
#define RG_DSI_MPPLL_SDM_FRA_EN BIT(0)
#define RG_DSI_MPPLL_SDM_SSC_PH_INIT BIT(1)
#define RG_DSI_MPPLL_SDM_SSC_EN BIT(2)
#define RG_DSI_MPPLL_SDM_SSC_PRD (0xffff << 16)
#define MIPITX_DSI_PLL_CON2 0x58

#define MIPITX_DSI_PLL_TOP 0x64
#define RG_DSI_MPPLL_PRESERVE (0xff << 8)

#define MIPITX_DSI_PLL_PWR 0x68
#define RG_DSI_MPPLL_SDM_PWR_ON BIT(0)
#define RG_DSI_MPPLL_SDM_ISO_EN BIT(1)
#define RG_DSI_MPPLL_SDM_PWR_ACK BIT(8)

#define MIPITX_DSI_SW_CTRL 0x80
#define SW_CTRL_EN BIT(0)

#define MIPITX_DSI_SW_CTRL_CON0 0x84
#define SW_LNTC_LPTX_PRE_OE BIT(0)
#define SW_LNTC_LPTX_OE BIT(1)
#define SW_LNTC_LPTX_P BIT(2)
#define SW_LNTC_LPTX_N BIT(3)
#define SW_LNTC_HSTX_PRE_OE BIT(4)
#define SW_LNTC_HSTX_OE BIT(5)
#define SW_LNTC_HSTX_ZEROCLK BIT(6)
#define SW_LNT0_LPTX_PRE_OE BIT(7)
#define SW_LNT0_LPTX_OE BIT(8)
#define SW_LNT0_LPTX_P BIT(9)
#define SW_LNT0_LPTX_N BIT(10)
#define SW_LNT0_HSTX_PRE_OE BIT(11)
#define SW_LNT0_HSTX_OE BIT(12)
#define SW_LNT0_LPRX_EN BIT(13)
#define SW_LNT1_LPTX_PRE_OE BIT(14)
#define SW_LNT1_LPTX_OE BIT(15)
#define SW_LNT1_LPTX_P BIT(16)
#define SW_LNT1_LPTX_N BIT(17)
#define SW_LNT1_HSTX_PRE_OE BIT(18)
#define SW_LNT1_HSTX_OE BIT(19)
#define SW_LNT2_LPTX_PRE_OE BIT(20)
#define SW_LNT2_LPTX_OE BIT(21)
#define SW_LNT2_LPTX_P BIT(22)
#define SW_LNT2_LPTX_N BIT(23)
#define SW_LNT2_HSTX_PRE_OE BIT(24)
#define SW_LNT2_HSTX_OE BIT(25)

// MIPI_TX_MT6983 reg
#define MIPITX_LANE_CON_MT6983 (0x0004UL)
#define MIPITX_VOLTAGE_SEL_MT6983 (0x0008UL)
#define FLD_RG_DSI_PRD_REF_SEL (0x3f << 0)
#define FLD_RG_DSI_V2I_REF_SEL (0xf << 10)
#define MIPITX_PRESERVED_MT6983 (0x000CUL)

#define RG_DSI_PLL_SDM_PCW_CHG_MT6983 BIT(2)
#define RG_DSI_PLL_EN_MT6983 BIT(0)
#define FLD_RG_DSI_PLL_FBSEL_MT6983 (0x1 << 13)
#define FLD_RG_DSI_PLL_DIV3_EN	(0x1 << 28)
#define MIPITX_D2_SW_CTL_EN_MT6983 (0x015CUL)
#define MIPITX_D0_SW_CTL_EN_MT6983 (0x025CUL)
#define MIPITX_CK_SW_CTL_EN_MT6983 (0x035CUL)
#define MIPITX_D1_SW_CTL_EN_MT6983 (0x045CUL)
#define MIPITX_D3_SW_CTL_EN_MT6983 (0x055CUL)

#define MIPITX_PLL_CON2_MT6983 (0x0034UL)
#define RG_DSI_PLL_SDM_SSC_EN_MT6983 BIT(1)
#define FLD_RG_DSI_PLL_SDM_SSC_PRD_MT6983 (0xffff << 16)
#define MIPITX_PLL_CON3_MT6983 (0x0038UL)
#define FLD_RG_DSI_PLL_SDM_SSC_DELTA1_MT6983 (0xffff << 0)
#define FLD_RG_DSI_PLL_SDM_SSC_DELTA_MT6983 (0xffff << 16)

#define MIPITX_PHY_SEL0_MT6983 (0x0040UL)
#define FLD_MIPI_TX_PHY2_SEL_MT6983 (0xf << 0)
#define FLD_MIPI_TX_CPHY0BC_SEL_MT6983 (0xf << 4)
#define FLD_MIPI_TX_PHY0_SEL_MT6983 (0xf << 8)
#define FLD_MIPI_TX_PHY1AB_SEL_MT6983 (0xf << 12)
#define FLD_MIPI_TX_PHYC_SEL_MT6983 (0xf << 16)
#define FLD_MIPI_TX_CPHY1CA_SEL_MT6983 (0xf << 20)
#define FLD_MIPI_TX_PHY1_SEL_MT6983 (0xf << 24)
#define FLD_MIPI_TX_PHY2BC_SEL_MT6983 (0xf << 28)
#define MIPITX_PHY_SEL1_MT6983 (0x0044UL)
#define FLD_MIPI_TX_PHY3_SEL_MT6983 (0xf << 0)
#define FLD_MIPI_TX_CPHYXXX_SEL_MT6983 (0xf << 4)
#define FLD_MIPI_TX_LPRX_SEL_MT6983 (0xf << 8)
#define FLD_MIPI_TX_LPRX0AB_SEL_MT6983 (0xf << 12)
#define FLD_MIPI_TX_LPRX0BC_SEL_MT6983 (0xf << 16)
#define FLD_MIPI_TX_LPRX0CA_SEL_MT6983 (0xf << 20)
#define FLD_MIPI_TX_CPHY0_HS_SEL_MT6983 (0xf << 24)
#define FLD_MIPI_TX_CPHY1_HS_SEL_MT6983 (0xf << 26)
#define FLD_MIPI_TX_CPHY2_HS_SEL_MT6983 (0xf << 28)
#define FLD_MIPI_TX_CPHY_EN_MT6983 (0x1 << 31)

#define MIPITX_PHY_SEL2_MT6983 (0x0048UL)
#define FLD_MIPI_TX_PHY2_HSDATA_SEL_MT6983 (0xf << 0)
#define FLD_MIPI_TX_CPHY0BC_HSDATA_SEL_MT6983 (0xf << 4)
#define FLD_MIPI_TX_PHY0_HSDATA_SEL_MT6983 (0xf << 8)
#define FLD_MIPI_TX_PHY1AB_HSDATA_SEL_MT6983 (0xf << 12)
#define FLD_MIPI_TX_PHYC_HSDATA_SEL_MT6983 (0xf << 16)
#define FLD_MIPI_TX_CPHY1CA_HSDATA_SEL_MT6983 (0xf << 20)
#define FLD_MIPI_TX_PHY1_HSDATA_SEL_MT6983 (0xf << 24)
#define FLD_MIPI_TX_PHY2BC_HSDATA_SEL_MT6983 (0xf << 28)

/* use to reset DPHY */
#define MIPITX_SW_CTRL_CON4_MT6983 0x50

#define MIPITX_D2P_RTCODE3_0_MT6983 (0x0100UL)
#define MIPITX_D2N_RTCODE3_0_MT6983 (0x0108UL)

#define MIPITX_D2_CKMODE_EN_MT6983 (0x0120UL)
#define MIPITX_D0_CKMODE_EN_MT6983 (0x0220UL)
#define MIPITX_CK_CKMODE_EN_MT6983 (0x0320UL)
#define MIPITX_D1_CKMODE_EN_MT6983 (0x0420UL)
#define MIPITX_D3_CKMODE_EN_MT6983 (0x0520UL)

#define MIPITX_D0_SW_LPTX_PRE_OE_MT6983	(0x0260UL)
#define MIPITX_D0C_SW_LPTX_PRE_OE_MT6983	(0x0280UL)
#define MIPITX_D1_SW_LPTX_PRE_OE_MT6983	(0x0460UL)
#define MIPITX_D1C_SW_LPTX_PRE_OE_MT6983	(0x0480UL)
#define MIPITX_D2_SW_LPTX_PRE_OE_MT6983	(0x0160UL)
#define MIPITX_D2C_SW_LPTX_PRE_OE_MT6983	(0x0180UL)
#define MIPITX_D3_SW_LPTX_PRE_OE_MT6983	(0x0560UL)
#define MIPITX_D3C_SW_LPTX_PRE_OE_MT6983	(0x0580UL)
#define MIPITX_CK_SW_LPTX_PRE_OE_MT6983	(0x0360UL)
#define MIPITX_CKC_SW_LPTX_PRE_OE_MT6983	(0x0380UL)

enum MIPITX_PAD_VALUE {
	PAD_D2P_T0A = 0,
	PAD_D2N_T0B,
	PAD_D0P_T0C,
	PAD_D0N_T1A,
	PAD_CKP_T1B,
	PAD_CKN_T1C,
	PAD_D1P_T2A,
	PAD_D1N_T2B,
	PAD_D3P_T2C,
	PAD_D3N_XXX,
	PAD_NUM
};

inline struct mtk_mipi_tx *mtk_mipi_tx_from_clk_hw(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_mipi_tx, pll_hw);
}

void mtk_mipi_tx_clear_bits(struct mtk_mipi_tx *mipi_tx, u32 offset,
				   u32 bits)
{
	u32 temp = readl(mipi_tx->regs + offset);

	writel(temp & ~bits, mipi_tx->regs + offset);
}

void mtk_mipi_tx_set_bits(struct mtk_mipi_tx *mipi_tx, u32 offset,
				 u32 bits)
{
	u32 temp = readl(mipi_tx->regs + offset);

	writel(temp | bits, mipi_tx->regs + offset);
}

void mtk_mipi_tx_update_bits(struct mtk_mipi_tx *mipi_tx, u32 offset,
				    u32 mask, u32 data)
{
	u32 temp = readl(mipi_tx->regs + offset);

	writel((temp & ~mask) | (data & mask), mipi_tx->regs + offset);
}

unsigned int mtk_mipi_tx_pll_get_rate(struct phy *phy)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int i = 0;
	unsigned int pcw;
	unsigned int prediv;
	unsigned int posdiv;
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	pcw = readl(mipi_tx->regs + MIPITX_PLL_CON0);
	pcw = (pcw >> 24) & 0xff;
	prediv = 1;
	posdiv = DISP_REG_GET_FIELD(FLD_RG_DSI_PLL_POSDIV_,
				    MIPITX_PLL_CON1 + mipi_tx->regs);
	posdiv = (1 << posdiv);
	DDPINFO("%s, pcw: %d, prediv: %d, posdiv: %d", __func__, pcw, prediv,
		posdiv);
	i = prediv * posdiv;
	if (i > 0)
		return 26 * pcw / i;
#endif
	return 0;
}

int mtk_mipi_tx_dump(struct phy *phy)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	int k;

	DDPDUMP("== MIPI REGS:0x%x ==\n", mipi_tx->regs_pa);
	for (k = 0; k < 0x6A0; k += 16) {
		DDPDUMP("0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x\n", k,
			readl(mipi_tx->regs + k),
			readl(mipi_tx->regs + k + 0x4),
			readl(mipi_tx->regs + k + 0x8),
			readl(mipi_tx->regs + k + 0xc));
	}
#endif
	return 0;
}

int mtk_mipi_tx_cphy_lane_config(struct phy *phy,
				 struct mtk_panel_ext *mtk_panel,
				 bool is_master)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	struct mtk_panel_params *params = mtk_panel->params;
	int i = 0;
	enum MIPITX_PHY_LANE_SWAP *swap_base;
	enum MIPITX_PAD_VALUE pad_mapping[MIPITX_PHY_LANE_NUM] = {
		PAD_D2P_T0A, PAD_D0N_T1A, PAD_D1P_T2A,
		PAD_D1P_T2A, PAD_D1P_T2A, PAD_D1P_T2A};

	/* TODO: support dual port MIPI lane_swap */
	swap_base = params->lane_swap[i];
	DDPINFO("%s+\n", __func__);
	DDPDBG("MIPITX Lane Swap Enabled for DSI Port %d\n", i);
	DDPDBG("MIPITX Lane Swap mapping: %d|%d|%d|%d|%d|%d\n",
			swap_base[MIPITX_PHY_LANE_0],
			swap_base[MIPITX_PHY_LANE_1],
			swap_base[MIPITX_PHY_LANE_2],
			swap_base[MIPITX_PHY_LANE_3],
			swap_base[MIPITX_PHY_LANE_CK],
			swap_base[MIPITX_PHY_LANE_RX]);

	/*set lane swap*/
	if (!mtk_panel->params->lane_swap_en) {
		writel(0x65432101, mipi_tx->regs + MIPITX_PHY_SEL0);
		writel(0x24210987, mipi_tx->regs + MIPITX_PHY_SEL1);
		writel(0x68543102, mipi_tx->regs + MIPITX_PHY_SEL2);
		writel(0x00000007, mipi_tx->regs + MIPITX_PHY_SEL3);
		return 0;
	}

	/* ENABLE CPHY*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
			FLD_MIPI_TX_CPHY_EN, 0x1);

	/* CPHY_LANE_T0 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY2_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 4);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_CPHY0BC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY0_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 2) << 12);

	/* CPHY_LANE_T1 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY1AB_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHYC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1) << 20);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_CPHY1CA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 2) << 24);

	/* CPHY_LANE_T2 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY1_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 28);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_PHY2BC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_PHY3_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 2) << 4);

	/* LPRX SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_LPRX0AB_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_LPRX0BC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_LPRX0CA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 2) << 20);

	/* HS SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_CPHY0_HS_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) / 3 << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_CPHY1_HS_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] / 3) << 26);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_CPHY2_HS_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] / 3) << 28);

	/* HS_DATA_SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY2_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 2) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_CPHY0BC_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 4);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY0_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY1AB_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHYC_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_CPHY1CA_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 2) << 20);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY1_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 2) << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY2BC_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 28);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL3,
		FLD_MIPI_TX_PHY3_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1) << 0);

	return 0;
}

int mtk_mipi_tx_cphy_lane_config_mt6983(struct phy *phy,
				 struct mtk_panel_ext *mtk_panel,
				 bool is_master)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	struct mtk_panel_params *params = mtk_panel->params;
	int i = 0;
	enum MIPITX_PHY_LANE_SWAP *swap_base;
	enum MIPITX_PAD_VALUE pad_mapping[MIPITX_PHY_LANE_NUM] = {
		PAD_D2P_T0A, PAD_D0N_T1A, PAD_D1P_T2A,
		PAD_D1P_T2A, PAD_D1P_T2A, PAD_D1P_T2A};

	/* TODO: support dual port MIPI lane_swap */
	swap_base = params->lane_swap[i];
	DDPINFO("%s+\n", __func__);
	DDPDBG("MIPITX Lane Swap Enabled for DSI Port %d\n", i);
	DDPDBG("MIPITX Lane Swap mapping: %d|%d|%d|%d|%d|%d\n",
			swap_base[MIPITX_PHY_LANE_0],
			swap_base[MIPITX_PHY_LANE_1],
			swap_base[MIPITX_PHY_LANE_2],
			swap_base[MIPITX_PHY_LANE_3],
			swap_base[MIPITX_PHY_LANE_CK],
			swap_base[MIPITX_PHY_LANE_RX]);

	/*set lane swap*/
	if (!mtk_panel->params->lane_swap_en) {
		writel(0x76543210, mipi_tx->regs + MIPITX_PHY_SEL0_MT6983);
		writel(0xA4210098, mipi_tx->regs + MIPITX_PHY_SEL1_MT6983);
		writel(0x68543102, mipi_tx->regs + MIPITX_PHY_SEL2_MT6983);
		writel(0x00000007, mipi_tx->regs + MIPITX_PHY_SEL3);
		return 0;
	}

	/* ENABLE CPHY*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
			FLD_MIPI_TX_CPHY_EN_MT6983, 0x1);

	/* CPHY_LANE_T0 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY2_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 4);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_CPHY0BC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY0_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 2) << 12);

	/* CPHY_LANE_T1 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY1AB_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHYC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1) << 20);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_CPHY1CA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 2) << 24);

	/* CPHY_LANE_T2 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY1_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 28);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_PHY2BC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_PHY3_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 2) << 4);

	/* LPRX SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_LPRX0AB_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_LPRX0BC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_LPRX0CA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 2) << 20);

	/* HS SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_CPHY0_HS_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) / 3 << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_CPHY1_HS_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] / 3) << 26);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_CPHY2_HS_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] / 3) << 28);

	/* HS_DATA_SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY2_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 2) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_CPHY0BC_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 4);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY0_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY1AB_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHYC_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_CPHY1CA_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 2) << 20);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY1_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 2) << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY2BC_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 28);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL3,
		FLD_MIPI_TX_PHY3_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1) << 0);

	return 0;
}


int mtk_mipi_tx_dphy_lane_config(struct phy *phy,
				 struct mtk_panel_ext *mtk_panel,
				 bool is_master)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	struct mtk_panel_params *params = mtk_panel->params;
	int j, i = 0;
	enum MIPITX_PHY_LANE_SWAP *swap_base;
	enum MIPITX_PAD_VALUE pad_mapping[MIPITX_PHY_LANE_NUM] = {
		PAD_D0P_T0C, PAD_D1P_T2A, PAD_D2P_T0A,
		PAD_D3P_T2C, PAD_CKP_T1B, PAD_CKP_T1B};

	if (!mtk_panel->params->lane_swap_en) {
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_CK_CKMODE_EN,
				0x1, 0x1);
		return 0;
	}

	/* TODO: support dual port MIPI lane_swap */
	if (is_master)
		swap_base = params->lane_swap[MIPITX_PHY_PORT_0];
	else
		swap_base = params->lane_swap[MIPITX_PHY_PORT_1];
	DDPDBG("MIPITX Lane Swap Enabled for DSI Port %d\n", i);
	DDPDBG("MIPITX Lane Swap mapping: %d|%d|%d|%d|%d|%d\n",
			swap_base[MIPITX_PHY_LANE_0],
			swap_base[MIPITX_PHY_LANE_1],
			swap_base[MIPITX_PHY_LANE_2],
			swap_base[MIPITX_PHY_LANE_3],
			swap_base[MIPITX_PHY_LANE_CK],
			swap_base[MIPITX_PHY_LANE_RX]);

	for (j = MIPITX_PHY_LANE_0; j < MIPITX_PHY_LANE_CK; j++) {
		if (swap_base[j] == MIPITX_PHY_LANE_CK)
			break;
	}
	switch (j) {
	case MIPITX_PHY_LANE_0:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D0_CKMODE_EN,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_1:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D1_CKMODE_EN,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_2:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D2_CKMODE_EN,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_3:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D3_CKMODE_EN,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_CK:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_CK_CKMODE_EN,
				0x1, 0x1);
		break;
	default:
		break;
	}

	/* LANE_0 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY0_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY1AB_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 16);

	/* LANE_1 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY1_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 28);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_PHY2BC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1) << 0);

	/* LANE_2 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHY2_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 4);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_CPHY0BC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1) << 8);

	/* LANE_3 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_PHY3_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_3]]) << 4);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_CPHYXXX_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_3]] + 1) << 8);

	/* CK_LANE */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_PHYC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_CK]]) << 20);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0,
		FLD_MIPI_TX_CPHY1CA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_CK]] + 1) << 24);

	/* LPRX SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_LPRX0AB_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_RX]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1,
		FLD_MIPI_TX_LPRX0BC_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_RX]] + 1) << 16);

	/* HS_DATA_SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY2_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY0_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHYC_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_CK]]) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2,
		FLD_MIPI_TX_PHY1_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL3,
		FLD_MIPI_TX_PHY3_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_3]]) << 0);

	return 0;
}

int mtk_mipi_tx_dphy_lane_config_mt6983(struct phy *phy,
				 struct mtk_panel_ext *mtk_panel,
				 bool is_master)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	struct mtk_panel_params *params = mtk_panel->params;
	int j, i = 0;
	enum MIPITX_PHY_LANE_SWAP *swap_base;
	enum MIPITX_PAD_VALUE pad_mapping[MIPITX_PHY_LANE_NUM] = {
		PAD_D0P_T0C, PAD_D1P_T2A, PAD_D2P_T0A,
		PAD_D3P_T2C, PAD_CKP_T1B, PAD_CKP_T1B};

	if (!mtk_panel->params->lane_swap_en) {
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_CK_CKMODE_EN_MT6983,
				0x1, 0x1);
		return 0;
	}

	/* TODO: support dual port MIPI lane_swap */
	if (is_master)
		swap_base = params->lane_swap[MIPITX_PHY_PORT_0];
	else
		swap_base = params->lane_swap[MIPITX_PHY_PORT_1];
	DDPDBG("MIPITX Lane Swap Enabled for DSI Port %d\n", i);
	DDPDBG("MIPITX Lane Swap mapping: %d|%d|%d|%d|%d|%d\n",
			swap_base[MIPITX_PHY_LANE_0],
			swap_base[MIPITX_PHY_LANE_1],
			swap_base[MIPITX_PHY_LANE_2],
			swap_base[MIPITX_PHY_LANE_3],
			swap_base[MIPITX_PHY_LANE_CK],
			swap_base[MIPITX_PHY_LANE_RX]);

	for (j = MIPITX_PHY_LANE_0; j < MIPITX_PHY_LANE_CK; j++) {
		if (swap_base[j] == MIPITX_PHY_LANE_CK)
			break;
	}
	switch (j) {
	case MIPITX_PHY_LANE_0:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D0_CKMODE_EN_MT6983,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_1:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D1_CKMODE_EN_MT6983,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_2:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D2_CKMODE_EN_MT6983,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_3:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_D3_CKMODE_EN_MT6983,
				0x1, 0x1);
		break;
	case MIPITX_PHY_LANE_CK:
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_CK_CKMODE_EN_MT6983,
				0x1, 0x1);
		break;
	default:
		break;
	}

	/* LANE_0 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY0_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY1AB_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]] + 1) << 12);

	/* LANE_1 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY1_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY2BC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]] + 1) << 28);

	/* LANE_2 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHY2_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_CPHY0BC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]] + 1) << 4);

	/* LANE_3 */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_PHY3_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_3]]) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_CPHYXXX_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_3]] + 1) << 4);

	/* CK_LANE */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_PHYC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_CK]]) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL0_MT6983,
		FLD_MIPI_TX_CPHY1CA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_CK]] + 1) << 20);

	/* LPRX SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_LPRX0AB_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_RX]]) << 12);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL1_MT6983,
		FLD_MIPI_TX_LPRX0BC_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_RX]] + 1) << 16);

	/* HS_DATA_SETTING */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY2_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_2]]) << 0);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY0_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_0]]) << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHYC_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_CK]]) << 16);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL2_MT6983,
		FLD_MIPI_TX_PHY1_HSDATA_SEL_MT6983,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_1]]) << 24);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PHY_SEL3,
		FLD_MIPI_TX_PHY3_HSDATA_SEL,
		(pad_mapping[swap_base[MIPITX_PHY_LANE_3]]) << 0);

	return 0;
}

int mtk_mipi_tx_ssc_en(struct phy *phy, struct mtk_panel_ext *mtk_panel)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	unsigned int data_rate;
	u16 pdelta1, ssc_prd;
	u8 txdiv, div3;

	DDPINFO("%s+\n", __func__);
	if (mtk_panel->params->ssc_enable) {

		data_rate = mtk_panel->params->data_rate;

		if (data_rate >= 6000) {
			txdiv = 1;
			div3  = 1;
		} else if (data_rate >= 3000) {
			txdiv = 2;
			div3  = 1;
		} else if (data_rate >= 2000) {
			txdiv = 1;
			div3  = 3;
		} else if (data_rate >= 1500) {
			txdiv = 4;
			div3  = 1;
		} else if (data_rate >= 1000) {
			txdiv = 2;
			div3  = 3;
		} else if (data_rate >= 750) {
			txdiv = 8;
			div3  = 1;
		} else if (data_rate >= 510) {
			txdiv = 4;
			div3  = 3;
		} else {
			DDPPR_ERR("data rate is too low\n");
			return -EINVAL;
		}

		pdelta1 = data_rate / 2 * txdiv * div3 * 5 / 26 * 262144 / 1000 / 433;

		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON3_MT6983,
						FLD_RG_DSI_PLL_SDM_SSC_DELTA1_MT6983, pdelta1);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON3_MT6983,
						FLD_RG_DSI_PLL_SDM_SSC_DELTA_MT6983, pdelta1 << 16);

		ssc_prd = 0x1b1;//fix
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON2_MT6983,
						FLD_RG_DSI_PLL_SDM_SSC_PRD_MT6983, ssc_prd << 16);
		mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON2_MT6983,
						mipi_tx->driver_data->dsi_ssc_en);

		DDPINFO("set ssc enabled\n");
	}
	return 0;
}

void mtk_mipi_tx_sw_control_en(struct phy *phy, bool en)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	DDPINFO("%s, en=%d\n", __func__, en);
	if (en) {
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en,
			DSI_D0_SW_CTL_EN);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en,
			DSI_D1_SW_CTL_EN);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en,
			DSI_D2_SW_CTL_EN);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en,
			DSI_D3_SW_CTL_EN);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en,
			DSI_CK_SW_CTL_EN);
	} else {
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en,
			DSI_D0_SW_CTL_EN);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en,
			DSI_D1_SW_CTL_EN);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en,
			DSI_D2_SW_CTL_EN);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en,
			DSI_D3_SW_CTL_EN);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en,
			DSI_CK_SW_CTL_EN);
	}
}

void mtk_mipi_tx_pre_oe_config(struct phy *phy, bool en)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	if (en) {
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2c_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0c_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ckc_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1c_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3c_sw_lptx_pre_oe, 1);
	} else {
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d2_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d2c_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d0_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d0c_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->ck_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->ckc_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d1_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d1c_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d3_sw_lptx_pre_oe, 1);
		mtk_mipi_tx_clear_bits(mipi_tx, mipi_tx->driver_data->d3c_sw_lptx_pre_oe, 1);
	}
}

static int mtk_mipi_tx_pll_prepare(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	u8 txdiv, txdiv0, txdiv1;
	u64 pcw;

	dev_dbg(mipi_tx->dev, "prepare: %u Hz\n", mipi_tx->data_rate);

	if (mipi_tx->data_rate >= 500000000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 250000000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 125000000) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate > 62000000) {
		txdiv = 8;
		txdiv0 = 2;
		txdiv1 = 1;
	} else if (mipi_tx->data_rate >= 50000000) {
		txdiv = 16;
		txdiv0 = 2;
		txdiv1 = 2;
	} else {
		return -EINVAL;
	}

	mtk_mipi_tx_update_bits(
		mipi_tx, MIPITX_DSI_BG_CON,
		RG_DSI_VOUT_MSK | RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN,
		(4 << 20) | (4 << 17) | (4 << 14) | (4 << 11) | (4 << 8) |
			(4 << 5) | RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN);

	usleep_range(30, 100);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_DSI_TOP_CON,
				RG_DSI_LNT_IMP_CAL_CODE | RG_DSI_LNT_HS_BIAS_EN,
				(8 << 4) | RG_DSI_LNT_HS_BIAS_EN);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_DSI_CON,
			     RG_DSI_CKG_LDOOUT_EN | RG_DSI_LDOCORE_EN);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_DSI_PLL_PWR,
				RG_DSI_MPPLL_SDM_PWR_ON |
					RG_DSI_MPPLL_SDM_ISO_EN,
				RG_DSI_MPPLL_SDM_PWR_ON);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_PLL_CON0,
			       RG_DSI_MPPLL_PLL_EN);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_DSI_PLL_CON0,
				RG_DSI_MPPLL_TXDIV0 | RG_DSI_MPPLL_TXDIV1 |
					RG_DSI_MPPLL_PREDIV,
				(txdiv0 << 3) | (txdiv1 << 5));

	/*
	 * PLL PCW config
	 * PCW bit 24~30 = integer part of pcw
	 * PCW bit 0~23 = fractional part of pcw
	 * pcw = data_Rate*4*txdiv/(Ref_clk*2);
	 * Post DIV =4, so need data_Rate*4
	 * Ref_clk is 26MHz
	 */
	pcw = div_u64(((u64)mipi_tx->data_rate * 2 * txdiv) << 24, 26000000);
	writel(pcw, mipi_tx->regs + MIPITX_DSI_PLL_CON2);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_DSI_PLL_CON1,
			     RG_DSI_MPPLL_SDM_FRA_EN);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_DSI_PLL_CON0, RG_DSI_MPPLL_PLL_EN);

	usleep_range(20, 100);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_PLL_CON1,
			       RG_DSI_MPPLL_SDM_SSC_EN);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_DSI_PLL_TOP,
				RG_DSI_MPPLL_PRESERVE,
				mipi_tx->driver_data->mppll_preserve);

	return 0;
}

bool mtk_is_mipi_tx_enable(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	u32 tmp = readl(mipi_tx->regs + MIPITX_PLL_CON1);

	return ((tmp & mipi_tx->driver_data->dsi_pll_en) > 0);
}

static unsigned int _dsi_get_pcw_mt6983(unsigned long data_rate,
	unsigned int pcw_ratio)
{
	unsigned int pcw, tmp, pcw_floor, fbksel, div3 = 0;

	if (data_rate >= 6000)
		div3 = 1;
	else if (data_rate >= 3000)
		div3 = 1;
	else if (data_rate >= 2000)
		div3 = 3;
	else if (data_rate >= 1500)
		div3 = 1;
	else if (data_rate >= 1000)
		div3 = 3;
	else if (data_rate >= 750)
		div3 = 1;
	else if (data_rate >= 510)
		div3 = 3;
	else {
		DDPPR_ERR("invalid data rate %u\n");
		return -EINVAL;
	}

	data_rate = data_rate >> 1;
	fbksel = (data_rate * pcw_ratio) >= 3800 ? 2 : 1;

	/**
	 * PCW bit 24~30 = floor(pcw)
	 * PCW bit 16~23 = (pcw - floor(pcw))*256
	 * PCW bit 8~15 = (pcw*256 - floor(pcw)*256)*256
	 * PCW bit 0~7 = (pcw*256*256 - floor(pcw)*256*256)*256
	 */
	pcw = data_rate * pcw_ratio * div3 / fbksel / 26;
	pcw_floor = data_rate * pcw_ratio  * div3 / fbksel % 26;
	tmp = ((pcw & 0xFF) << 24) | (((256 * pcw_floor / 26) & 0xFF) << 16) |
		(((256 * (256 * pcw_floor % 26) / 26) & 0xFF) << 8) |
		((256 * (256 * (256 * pcw_floor % 26) % 26) / 26) & 0xFF);

	return tmp;
}

unsigned int _dsi_get_pcw(unsigned long data_rate,
	unsigned int pcw_ratio)
{
	unsigned int pcw, tmp, pcw_floor;

	/**
	 * PCW bit 24~30 = floor(pcw)
	 * PCW bit 16~23 = (pcw - floor(pcw))*256
	 * PCW bit 8~15 = (pcw*256 - floor(pcw)*256)*256
	 * PCW bit 0~7 = (pcw*256*256 - floor(pcw)*256*256)*256
	 */
	pcw = data_rate * pcw_ratio / 26;
	pcw_floor = data_rate * pcw_ratio % 26;
	tmp = ((pcw & 0xFF) << 24) | (((256 * pcw_floor / 26) & 0xFF) << 16) |
		(((256 * (256 * pcw_floor % 26) / 26) & 0xFF) << 8) |
		((256 * (256 * (256 * pcw_floor % 26) % 26) / 26) & 0xFF);

	return tmp;
}

static int mtk_mipi_tx_pll_prepare_mt6779(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u64 pcw;

	DDPINFO("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	dev_dbg(mipi_tx->dev, "prepare: %u Hz\n", mipi_tx->data_rate);
	if (mipi_tx->data_rate >= 2000000000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 1000000000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 500000000) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate > 250000000) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (mipi_tx->data_rate >= 125000000) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				0);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				0);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				0);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				0);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				0);
	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	pcw = (mipi_tx->data_rate / 1000000) * txdiv / 26;
	tmp = ((pcw & 0xFF) << 24) |
	      (((256 * ((mipi_tx->data_rate / 1000000) * txdiv % 26) / 26) &
		0xFF)
	       << 16) |
	      (((256 *
		 (256 * ((mipi_tx->data_rate / 1000000) * txdiv % 26) % 26) /
		 26) &
		0xFF)
	       << 8) |
	      ((256 *
		(256 *
		 (256 * ((mipi_tx->data_rate / 1000000) * txdiv % 26) % 26) %
		 26) /
		26) &
	       0xFF);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPINFO("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6885(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPINFO("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6983(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	unsigned int div3, div3_en;
	u32 rate;
	unsigned int fbksel;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;
	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);

	if (rate >= 6000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 3000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
		div3 = 3;
		div3_en = 1;
	} else if (rate >= 1500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
		div3 = 3;
		div3_en = 1;
	} else if (rate >= 750) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 510) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
		div3 = 3;
		div3_en = 1;
	} else {
		DDPPR_ERR("data rate is too low\n");
		return -EINVAL;
	}
	if (rate < 2500)
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
			FLD_RG_DSI_PRD_REF_SEL, 0x0);
	else
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
			FLD_RG_DSI_PRD_REF_SEL, 0x4);

	if (rate > 2000)
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
			FLD_RG_DSI_V2I_REF_SEL, 0x4);
	else if (rate > 1200)
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
			FLD_RG_DSI_V2I_REF_SEL, 0x2);
	else
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
			FLD_RG_DSI_V2I_REF_SEL, 0x0);

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED_MT6983);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON_MT6983);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON_MT6983);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	fbksel = ((rate >> 1) * txdiv) >= 3800 ? 2 : 1;
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			FLD_RG_DSI_PLL_FBSEL_MT6983, (fbksel - 1) << 13);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_DIV3_EN, div3_en << 28);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	DDPDBG("%s-\n", __func__);

#endif
	return 0;
}

static int mtk_mipi_tx_pll_cphy_prepare_mt6885(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}
	/*set volate: cphy need 500mV*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
		FLD_RG_DSI_HSTX_LDO_REF_SEL, 0xD << 6);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt << 6);
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	/* step 0 */
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN=0 BG_CORE_EN=1 */
	writel(0x3FFF0088, mipi_tx->regs + MIPITX_LANE_CON);
	//usleep_range(1, 1); /* 1us */
	/* BG_LPF_EN=1 */
	writel(0x3FFF00C8, mipi_tx->regs + MIPITX_LANE_CON);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	DDPDBG("%s-\n", __func__);

	return 0;
}
static int mtk_mipi_tx_pll_cphy_prepare_mt6983(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	unsigned int div3, div3_en;
	u32 rate;
	unsigned int fbksel;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 6000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 3000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
		div3 = 3;
		div3_en = 1;
	} else if (rate >= 1500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
		div3 = 3;
		div3_en = 1;
	} else if (rate >= 750) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
		div3 = 3;
		div3_en = 1;
	} else if (rate >= 450) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
		div3 = 1;
		div3_en = 0;
	} else {
		DDPPR_ERR("data rate is too low\n");
		return -EINVAL;
	}
	/*set volate: cphy need 500mV*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
		FLD_RG_DSI_HSTX_LDO_REF_SEL, 0xD << 6);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL_MT6983,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt << 6);
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED_MT6983);
	/* step 0 */
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN=0 BG_CORE_EN=1 */
	writel(0x3FFF0088, mipi_tx->regs + MIPITX_LANE_CON_MT6983);
	//usleep_range(1, 1); /* 1us */
	/* BG_LPF_EN=1 */
	writel(0x3FFF00C8, mipi_tx->regs + MIPITX_LANE_CON_MT6983);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	fbksel = ((rate >> 1) * txdiv) >= 3800 ? 2 : 1;
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			FLD_RG_DSI_PLL_FBSEL_MT6983, (fbksel - 1) << 13);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_DIV3_EN, div3_en << 28);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	DDPDBG("%s-\n", __func__);
#endif
	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6873(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	if (mipi_volt) {
		/* set mipi_tx voltage */
		DDPMSG(" %s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt << 6);
	}

	DDPDBG("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_cphy_prepare_mt6873(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}
	/*set volate*/
	writel(0x4444236A, mipi_tx->regs + MIPITX_VOLTAGE_SEL);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt<<6);
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	/* step 0 */
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN=0 BG_CORE_EN=1 */
	writel(0x3FFF0088, mipi_tx->regs + MIPITX_LANE_CON);
	//usleep_range(1, 1); /* 1us */
	/* BG_LPF_EN=1 */
	writel(0x3FFF00C8, mipi_tx->regs + MIPITX_LANE_CON);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	DDPDBG("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6853(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPDBG("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6833(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = _dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       RG_DSI_PLL_EN);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPDBG("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6879(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPDBG("%s-\n", __func__);
#endif
	return 0;
}

static int mtk_mipi_tx_pll_cphy_prepare_mt6879(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}
	/*set volate: cphy need 500mV*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
		FLD_RG_DSI_HSTX_LDO_REF_SEL, 0xD << 6);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt << 6);
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	/* step 0 */
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN=0 BG_CORE_EN=1 */
	writel(0x3FFF0088, mipi_tx->regs + MIPITX_LANE_CON);
	//usleep_range(1, 1); /* 1us */
	/* BG_LPF_EN=1 */
	writel(0x3FFF00C8, mipi_tx->regs + MIPITX_LANE_CON);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = _dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       RG_DSI_PLL_EN);

	usleep_range(50, 100);

#endif
	DDPDBG("%s-\n", __func__);

	return 0;
}

static int mtk_mipi_tx_pll_prepare_mt6855(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	usleep_range(500, 600);
	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);

#ifdef IF_ONE
	/* Switch OFF each Lane */
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
	mtk_mipi_tx_update_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, FLD_DSI_SW_CTL_EN,
				1);
#endif

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       mipi_tx->driver_data->dsi_pll_en);

	usleep_range(50, 100);

	/* TODO: should write bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	DDPDBG("%s-\n", __func__);
#endif
	return 0;
}

static int mtk_mipi_tx_pll_cphy_prepare_mt6855(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 rate;

	DDPDBG("%s+\n", __func__);

	/* if mipitx is on, skip it... */
	if (mtk_is_mipi_tx_enable(hw)) {
		DDPINFO("%s: mipitx already on\n", __func__);
		return 0;
	}

	rate = (mipi_tx->data_rate_adpt) ? mipi_tx->data_rate_adpt :
			mipi_tx->data_rate / 1000000;

	dev_dbg(mipi_tx->dev, "prepare: %u MHz\n", rate);
	if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate > 250) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 125) {
		txdiv = 16;
		txdiv0 = 4;
		txdiv1 = 0;
	} else {
		return -EINVAL;
	}
	/*set volate: cphy need 500mV*/
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
		FLD_RG_DSI_HSTX_LDO_REF_SEL, 0xD << 6);

	/* change the mipi_volt */
	if (mipi_volt) {
		DDPMSG("%s+ mipi_volt change: %d\n", __func__, mipi_volt);
		mtk_mipi_tx_update_bits(mipi_tx, MIPITX_VOLTAGE_SEL,
			FLD_RG_DSI_HSTX_LDO_REF_SEL, mipi_volt << 6);
	}

	writel(0x0, mipi_tx->regs + MIPITX_PRESERVED);
	/* step 0 */
	/* BG_LPF_EN / BG_CORE_EN */
	writel(0x00FF12E0, mipi_tx->regs + MIPITX_PLL_CON4);
	/* BG_LPF_EN=0 BG_CORE_EN=1 */
	writel(0x3FFF0088, mipi_tx->regs + MIPITX_LANE_CON);
	//usleep_range(1, 1); /* 1us */
	/* BG_LPF_EN=1 */
	writel(0x3FFF00C8, mipi_tx->regs + MIPITX_LANE_CON);

	/* step 1: SDM_RWR_ON / SDM_ISO_EN */
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_PWR_ON, 1);
	usleep_range(30, 100);
	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_PWR,
				FLD_AD_DSI_PLL_SDM_ISO_EN, 0);

	tmp = _dsi_get_pcw(rate, txdiv);
	writel(tmp, mipi_tx->regs + MIPITX_PLL_CON0);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_PLL_CON1,
			      FLD_RG_DSI_PLL_POSDIV, txdiv0 << 8);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_CON1,
			       RG_DSI_PLL_EN);

	usleep_range(50, 100);

#endif
	DDPDBG("%s-\n", __func__);

	return 0;
}

static void mtk_mipi_tx_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_PLL_CON0,
			       RG_DSI_MPPLL_PLL_EN);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_DSI_PLL_TOP,
				RG_DSI_MPPLL_PRESERVE,
				mipi_tx->driver_data->mppll_preserve);

	mtk_mipi_tx_update_bits(mipi_tx, MIPITX_DSI_PLL_PWR,
				RG_DSI_MPPLL_SDM_ISO_EN |
					RG_DSI_MPPLL_SDM_PWR_ON,
				RG_DSI_MPPLL_SDM_ISO_EN);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_TOP_CON,
			       RG_DSI_LNT_HS_BIAS_EN);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_CON,
			       RG_DSI_CKG_LDOOUT_EN | RG_DSI_LDOCORE_EN);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_BG_CON,
			       RG_DSI_BG_CKEN | RG_DSI_BG_CORE_EN);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_PLL_CON0,
			       RG_DSI_MPPLL_DIV_MSK);
}

static void mtk_mipi_tx_pll_unprepare_mt6779(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPINFO("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, DSI_CK_SW_CTL_EN);

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_unprepare_mt6885(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6885(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);

}

static void mtk_mipi_tx_pll_unprepare_mt6983(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4_MT6983, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON_MT6983);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON_MT6983);

	DDPINFO("%s-\n", __func__);
#endif
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6983(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON_MT6983);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON_MT6983);

	DDPINFO("%s-\n", __func__);
#endif
}

static void mtk_mipi_tx_pll_unprepare_mt6873(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6873(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);

}

static void mtk_mipi_tx_pll_unprepare_mt6853(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_unprepare_mt6833(struct clk_hw *hw)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, mipi_tx->driver_data->dsi_pll_en);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d0_sw_ctl_en, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d1_sw_ctl_en, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d2_sw_ctl_en, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->d3_sw_ctl_en, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, mipi_tx->driver_data->ck_sw_ctl_en, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);

	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_unprepare_mt6879(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D0_SW_CTL_EN, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D1_SW_CTL_EN, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D2_SW_CTL_EN, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D3_SW_CTL_EN, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_CK_SW_CTL_EN, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);
#endif
	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6879(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON);
#endif
	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_unprepare_mt6855(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	/* TODO: should clear bit8 to set SW_ANA_CK_EN here */
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_SW_CTRL_CON4, 1);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

#ifdef IF_ZERO
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D0_SW_CTL_EN, DSI_D0_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D1_SW_CTL_EN, DSI_D1_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D2_SW_CTL_EN, DSI_D2_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_D3_SW_CTL_EN, DSI_D3_SW_CTL_EN);
	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_CK_SW_CTL_EN, DSI_CK_SW_CTL_EN);
#endif

	writel(0x3FFF0180, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0100, mipi_tx->regs + MIPITX_LANE_CON);
#endif
	DDPINFO("%s-\n", __func__);
}

static void mtk_mipi_tx_pll_cphy_unprepare_mt6855(struct clk_hw *hw)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	DDPDBG("%s+\n", __func__);
	dev_dbg(mipi_tx->dev, "cphy unprepare\n");

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_CON1, RG_DSI_PLL_EN);

	mtk_mipi_tx_set_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_ISO_EN);
	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_PLL_PWR, AD_DSI_PLL_SDM_PWR_ON);

	writel(0x3FFF0080, mipi_tx->regs + MIPITX_LANE_CON);
	writel(0x3FFF0000, mipi_tx->regs + MIPITX_LANE_CON);
#endif
	DDPINFO("%s-\n", __func__);
}

void mtk_mipi_tx_pll_rate_set_adpt(struct phy *phy, unsigned long rate)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	mipi_tx->data_rate_adpt = rate;
}

void mtk_mipi_tx_pll_rate_switch_gce(struct phy *phy,
		void *handle, unsigned long rate)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 reg_val;

	DDPINFO("%s+ %lu\n", __func__, rate);

	if (mipi_tx->driver_data->pll_rate_switch_gce) {
		mipi_tx->driver_data->pll_rate_switch_gce(phy, handle, rate);
	} else {
		/* parameter rate should be MHz */
		if (rate >= 2000) {
			txdiv = 1;
			txdiv0 = 0;
			txdiv1 = 0;
		} else if (rate >= 1000) {
			txdiv = 2;
			txdiv0 = 1;
			txdiv1 = 0;
		} else if (rate >= 500) {
			txdiv = 4;
			txdiv0 = 2;
			txdiv1 = 0;
		} else if (rate > 250) {
			txdiv = 8;
			txdiv0 = 3;
			txdiv1 = 0;
		} else if (rate >= 125) {
			txdiv = 16;
			txdiv0 = 4;
			txdiv1 = 0;
		} else {
			return;
		}
		tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);

		cmdq_pkt_write(handle, mipi_tx->cmdq_base,
			mipi_tx->regs_pa + MIPITX_PLL_CON0, tmp, ~0);

		reg_val = readl(mipi_tx->regs + MIPITX_PLL_CON1);

		reg_val = ((reg_val & ~FLD_RG_DSI_PLL_POSDIV) |
			((txdiv0 << 8) & FLD_RG_DSI_PLL_POSDIV));

		reg_val = (reg_val & ~(mipi_tx->driver_data->dsi_pll_sdm_pcw_chg)) |
			(0 & mipi_tx->driver_data->dsi_pll_sdm_pcw_chg);

		cmdq_pkt_write(handle, mipi_tx->cmdq_base,
				mipi_tx->regs_pa + MIPITX_PLL_CON1, reg_val, ~0);

		reg_val = (reg_val & ~(mipi_tx->driver_data->dsi_pll_sdm_pcw_chg)) |
			(1 & mipi_tx->driver_data->dsi_pll_sdm_pcw_chg);

		cmdq_pkt_write(handle, mipi_tx->cmdq_base,
				mipi_tx->regs_pa + MIPITX_PLL_CON1, reg_val, ~0);

		reg_val = (reg_val & ~(mipi_tx->driver_data->dsi_pll_sdm_pcw_chg)) |
			(0 & mipi_tx->driver_data->dsi_pll_sdm_pcw_chg);

		cmdq_pkt_write(handle, mipi_tx->cmdq_base,
				mipi_tx->regs_pa + MIPITX_PLL_CON1, reg_val, ~0);
	}

	DDPINFO("%s-\n", __func__);
}

void mtk_mipi_tx_pll_rate_switch_gce_mt6895(struct phy *phy,
		void *handle, unsigned long rate)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	unsigned int txdiv, txdiv0, txdiv1, tmp;
	u32 reg_val;

	DDPINFO("%s+ %lu\n", __func__, rate);

	/* parameter rate should be MHz */
	if (rate >= 6000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 3000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 2000) {
		txdiv = 1;
		txdiv0 = 0;
		txdiv1 = 0;
	} else if (rate >= 1500) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else if (rate >= 1000) {
		txdiv = 2;
		txdiv0 = 1;
		txdiv1 = 0;
	} else if (rate >= 750) {
		txdiv = 8;
		txdiv0 = 3;
		txdiv1 = 0;
	} else if (rate >= 510) {
		txdiv = 4;
		txdiv0 = 2;
		txdiv1 = 0;
	} else {
		return;
	}
	tmp = mipi_tx->driver_data->dsi_get_pcw(rate, txdiv);

	cmdq_pkt_write(handle, mipi_tx->cmdq_base,
			mipi_tx->regs_pa + MIPITX_PLL_CON0, tmp, ~0);

	reg_val = readl(mipi_tx->regs + MIPITX_PLL_CON1);

	reg_val = ((reg_val & ~FLD_RG_DSI_PLL_POSDIV) |
			((txdiv0 << 8) & FLD_RG_DSI_PLL_POSDIV));

	reg_val = (reg_val & ~(mipi_tx->driver_data->dsi_pll_sdm_pcw_chg)) |
		(0 & mipi_tx->driver_data->dsi_pll_sdm_pcw_chg);

	cmdq_pkt_write(handle, mipi_tx->cmdq_base,
			mipi_tx->regs_pa + MIPITX_PLL_CON1, reg_val, ~0);

	reg_val = (reg_val & ~(mipi_tx->driver_data->dsi_pll_sdm_pcw_chg)) |
		(1 & mipi_tx->driver_data->dsi_pll_sdm_pcw_chg);

	cmdq_pkt_write(handle, mipi_tx->cmdq_base,
			mipi_tx->regs_pa + MIPITX_PLL_CON1, reg_val, ~0);

	reg_val = (reg_val & ~(mipi_tx->driver_data->dsi_pll_sdm_pcw_chg)) |
		(0 & mipi_tx->driver_data->dsi_pll_sdm_pcw_chg);

	cmdq_pkt_write(handle, mipi_tx->cmdq_base,
			mipi_tx->regs_pa + MIPITX_PLL_CON1, reg_val, ~0);

	DDPINFO("%s-\n", __func__);
}


static long mtk_mipi_tx_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				       unsigned long *prate)
{
	return clamp_val(rate, 50000000, 3000000000);
}

static int mtk_mipi_tx_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	dev_dbg(mipi_tx->dev, "set rate: %lu Hz\n", rate);

	mipi_tx->data_rate = rate;

	return 0;
}

static unsigned long mtk_mipi_tx_pll_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	struct mtk_mipi_tx *mipi_tx = mtk_mipi_tx_from_clk_hw(hw);

	return mipi_tx->data_rate;
}

static struct clk_ops mtk_mipi_tx_pll_ops = {
	.unprepare = mtk_mipi_tx_pll_unprepare,
	.round_rate = mtk_mipi_tx_pll_round_rate,
	.set_rate = mtk_mipi_tx_pll_set_rate,
	.recalc_rate = mtk_mipi_tx_pll_recalc_rate,
};

static int mtk_mipi_tx_power_on_signal(struct phy *phy)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	u32 reg;

	for (reg = MIPITX_DSI_CLOCK_LANE; reg <= MIPITX_DSI_DATA_LANE3;
	     reg += 4)
		mtk_mipi_tx_set_bits(mipi_tx, reg, RG_DSI_LNTx_LDOOUT_EN);

	mtk_mipi_tx_clear_bits(mipi_tx, MIPITX_DSI_TOP_CON,
			       RG_DSI_PAD_TIE_LOW_EN);
	return 0;
}

#ifdef MTK_FILL_MIPI_IMPEDANCE
unsigned int rt_code_backup0[2][25];
unsigned int rt_code_backup1[2][25];
unsigned int rt_dem_code_backup0[2][5];
unsigned int rt_dem_code_backup1[2][5];
#define SECOND_PHY_OFFSET  (0x10000UL)

void backup_mipitx_impedance(struct mtk_mipi_tx *mipi_tx)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;

	/* backup mipitx impedance */
	for (i = 0; i < 2; i++) {
		if (i == 0) {
			for (j = 0; j < 5; j++)
				for (k = 0; k < 5; k++) {
					rt_code_backup0[0][j*5 + k] =
						readl(mipi_tx->regs +
						MIPITX_D2P_RTCODE0 +
						k * 0x4 + j * 0x100);
					rt_code_backup1[0][j*5 + k] =
						readl(mipi_tx->regs +
						MIPITX_D2N_RTCODE0 +
						k * 0x4 + j * 0x100);
				}
		} else {
			for (j = 0; j < 5; j++) {
				rt_dem_code_backup0[0][j] =
					readl(mipi_tx->regs +
					MIPITX_D2P_RT_DEM_CODE +
					j * 0x100);
				rt_dem_code_backup1[0][j] =
					readl(mipi_tx->regs +
					MIPITX_D2N_RT_DEM_CODE +
					j * 0x100);
			}
		}
	}

#ifdef IF_ZERO /* Verification log */
	for (i = 0; i < 10; i++) {
		if (i < 5)
			k = i * 0x100;
		else
			k = (i - 5) * 0x100;

		if (i < 5) {
			DDPDUMP("MIPI_TX ");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RTCODE0 + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0x4)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0x8)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0xC)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0x10)));
			DDPDUMP("MIPI_TX2");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RTCODE0 + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0x4)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0x8)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0xC)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0x10)));
		} else {
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RT_DEM_CODE + k)));
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RT_DEM_CODE + k)));
		}
	}
#endif /* mipitx impedance print */
}

static void backup_mipitx_impedance_mt6983(struct mtk_mipi_tx *mipi_tx)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;

	/* backup mipitx impedance */
	for (i = 0; i < 2; i++) {
		if (i == 0) {
			for (j = 0; j < 5; j++)
				for (k = 0; k < 2; k++) {
					rt_code_backup0[0][j*5 + k] =
						readl(mipi_tx->regs +
						MIPITX_D2P_RTCODE3_0_MT6983 +
						k * 0x4 + j * 0x100);
					rt_code_backup1[0][j*5 + k] =
						readl(mipi_tx->regs +
						MIPITX_D2N_RTCODE3_0_MT6983 +
						k * 0x4 + j * 0x100);
				}
		} else {
			for (j = 0; j < 5; j++) {
				rt_dem_code_backup0[0][j] =
					readl(mipi_tx->regs +
					MIPITX_D2P_RT_DEM_CODE +
					j * 0x100);
				rt_dem_code_backup1[0][j] =
					readl(mipi_tx->regs +
					MIPITX_D2N_RT_DEM_CODE +
					j * 0x100);
			}
		}
	}
#ifdef IF_ZERO /* Verification log */
	for (i = 0; i < 10; i++) {
		if (i < 5)
			k = i * 0x100;
		else
			k = (i - 5) * 0x100;

		if (i < 5) {
			DDPDUMP("MIPI_TX ");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RTCODE3_0_MT6983 + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE3_0_MT6983 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE3_0_MT6983 + k + 0x4)),
			DDPDUMP("MIPI_TX2");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RTCODE3_0_MT6983 + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE3_0_MT6983 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE3_0_MT6983 + k + 0x4)),
		} else {
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RT_DEM_CODE + k)));
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RT_DEM_CODE + k)));
		}
	}
#endif /* mipitx impedance print */
}

void refill_mipitx_impedance(struct mtk_mipi_tx *mipi_tx)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;

	/* backup mipitx impedance */
	for (i = 0; i < 2; i++) {
		if (i == 0) {
			for (j = 0; j < 5; j++)
				for (k = 0; k < 5; k++) {
					writel(rt_code_backup0[0][j*5 + k],
						(mipi_tx->regs +
						 MIPITX_D2P_RTCODE0 +
						 k * 0x4 + j * 0x100));
					writel(rt_code_backup1[0][j*5 + k],
						(mipi_tx->regs +
						 MIPITX_D2N_RTCODE0 +
						 k * 0x4 + j * 0x100));
				}
		} else {
			for (j = 0; j < 5; j++) {
				writel(rt_dem_code_backup0[0][j],
					(mipi_tx->regs +
					 MIPITX_D2P_RT_DEM_CODE +
					 j * 0x100));
				writel(rt_dem_code_backup1[0][j],
					(mipi_tx->regs +
					 MIPITX_D2N_RT_DEM_CODE +
					 j * 0x100));
			}
		}
	}

#ifdef IF_ZERO /* Verification log */
	for (i = 0; i < 10; i++) {
		if (i < 5)
			k = i * 0x100;
		else
			k = (i - 5) * 0x100;

		if (i < 5) {
			DDPDUMP("MIPI_TX ");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RTCODE0 + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0x4)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0x8)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0xC)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE0 + k + 0x10)));
			DDPDUMP("MIPI_TX2");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RTCODE0 + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0x4)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0x8)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0xC)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE0 + k + 0x10)));
		} else {
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RT_DEM_CODE + k)));
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RT_DEM_CODE + k)));
		}
	}
#endif /* mipitx impedance print */
}

static void refill_mipitx_impedance_mt6983(struct mtk_mipi_tx *mipi_tx)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;

	/* refill mipitx impedance */
	for (i = 0; i < 2; i++) {
		if (i == 0) {
			for (j = 0; j < 5; j++)
				for (k = 0; k < 2; k++) {
					writel(rt_code_backup0[0][j*5 + k],
						(mipi_tx->regs +
						 MIPITX_D2P_RTCODE3_0_MT6983 +
						 k * 0x4 + j * 0x100));
					writel(rt_code_backup1[0][j*5 + k],
						(mipi_tx->regs +
						 MIPITX_D2N_RTCODE3_0_MT6983 +
						 k * 0x4 + j * 0x100));
				}
		} else {
			for (j = 0; j < 5; j++) {
				writel(rt_dem_code_backup0[0][j],
					(mipi_tx->regs +
					 MIPITX_D2P_RT_DEM_CODE +
					 j * 0x100));
				writel(rt_dem_code_backup1[0][j],
					(mipi_tx->regs +
					 MIPITX_D2N_RT_DEM_CODE +
					 j * 0x100));
			}
		}
	}

#ifdef IF_ZERO /* Verification log */
	for (i = 0; i < 10; i++) {
		if (i < 5)
			k = i * 0x100;
		else
			k = (i - 5) * 0x100;

		if (i < 5) {
			DDPDUMP("MIPI_TX ");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RTCODE3_0_MT6983 + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE3_0_MT6983 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2P_RTCODE3_0_MT6983 + k + 0x4)),
			DDPDUMP("MIPI_TX2");
			DDPDUMP("[0x%08x]:0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RTCODE3_0_MT6983 + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE3_0_MT6983 + k)),
				readl((mipi_tx->regs +
					MIPITX_D2N_RTCODE3_0_MT6983 + k + 0x4)),
		} else {
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2P_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2P_RT_DEM_CODE + k)));
			DDPDUMP("MIPI_TX[0x%08x]: 0x%08x\n",
				mipi_tx->regs_pa +
					MIPITX_D2N_RT_DEM_CODE + k,
				readl((mipi_tx->regs +
					MIPITX_D2N_RT_DEM_CODE + k)));
		}
	}
#endif /* mipitx impedance print */
}

#endif

static int mtk_mipi_tx_power_on(struct phy *phy)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);
	int ret;

	/* Power up core and enable PLL */
	ret = clk_prepare_enable(mipi_tx->pll);
	if (ret < 0)
		return ret;

	/* Enable DSI Lane LDO outputs, disable pad tie low */
	if (mipi_tx->driver_data->power_on_signal)
		mipi_tx->driver_data->power_on_signal(phy);

#ifdef MTK_FILL_MIPI_IMPEDANCE
	mipi_tx->driver_data->refill_mipitx_impedance(mipi_tx);
#endif

	return 0;
}

static int mtk_mipi_tx_power_off(struct phy *phy)
{
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(phy);

	/* Disable PLL and power down core */
	clk_disable_unprepare(mipi_tx->pll);

	return 0;
}

static const struct phy_ops mtk_mipi_tx_ops = {
	.power_on = mtk_mipi_tx_power_on,
	.power_off = mtk_mipi_tx_power_off,
	.owner = THIS_MODULE,
};

static int mtk_mipi_tx_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_mipi_tx *mipi_tx;
	struct resource *mem;
	struct clk *ref_clk;
	const char *ref_clk_name;
	struct clk_init_data clk_init = {
		.ops = &mtk_mipi_tx_pll_ops,
		.num_parents = 1,
		.parent_names = (const char *const *)&ref_clk_name,
		.flags = CLK_SET_RATE_GATE,
	};
	struct phy *phy;
	struct phy_provider *phy_provider;
	int ret;

	DDPINFO("%s+\n", __func__);

	mipi_tx = devm_kzalloc(dev, sizeof(*mipi_tx), GFP_KERNEL);
	if (!mipi_tx)
		return -ENOMEM;
	mipi_tx->driver_data =
		(struct mtk_mipitx_data *)of_device_get_match_data(&pdev->dev);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mipi_tx->regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(mipi_tx->regs)) {
		ret = PTR_ERR(mipi_tx->regs);
		dev_err(dev, "Failed to get memory resource: %d\n", ret);
		return ret;
	}

	mipi_tx->regs_pa = mem->start;

	mipi_tx->cmdq_base = cmdq_register_device(dev);

	ref_clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ref_clk)) {
		ret = PTR_ERR(ref_clk);
		dev_err(dev, "Failed to get reference clock: %d\n", ret);
		return ret;
	}
	ref_clk_name = __clk_get_name(ref_clk);

	ret = of_property_read_string(dev->of_node, "clock-output-names",
				      &clk_init.name);
	if (ret < 0) {
		dev_err(dev, "Failed to read clock-output-names: %d\n", ret);
		return ret;
	}

	mtk_mipi_tx_pll_ops.prepare = mipi_tx->driver_data->pll_prepare;
	mtk_mipi_tx_pll_ops.unprepare = mipi_tx->driver_data->pll_unprepare;

	if (disp_helper_get_stage() ==
						DISP_HELPER_STAGE_NORMAL) {
		mipi_tx->pll_hw.init = &clk_init;
		mipi_tx->pll = devm_clk_register(dev, &mipi_tx->pll_hw);
		if (IS_ERR(mipi_tx->pll)) {
			ret = PTR_ERR(mipi_tx->pll);
			dev_err(dev, "Failed to register PLL: %d\n", ret);
			return ret;
		}
	}

	phy = devm_phy_create(dev, NULL, &mtk_mipi_tx_ops);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		dev_err(dev, "Failed to create MIPI D-PHY: %d\n", ret);
		return ret;
	}
	phy_set_drvdata(phy, mipi_tx);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider)) {
		ret = PTR_ERR(phy_provider);
		return ret;
	}

	mipi_tx->dev = dev;

#ifdef MTK_FILL_MIPI_IMPEDANCE
	mipi_tx->driver_data->backup_mipitx_impedance(mipi_tx);
#endif

	DDPINFO("%s-\n", __func__);

	return of_clk_add_provider(dev->of_node, of_clk_src_simple_get,
				   mipi_tx->pll);
}

static int mtk_mipi_tx_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
	return 0;
}

static const struct mtk_mipitx_data mt2701_mipitx_data = {
	.mppll_preserve = (3 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare,
	.power_on_signal = mtk_mipi_tx_power_on_signal,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6779_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6779,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6779,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6885_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6885,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6885,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6885_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6885,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6885,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6983_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG_MT6983,
	.dsi_pll_en = RG_DSI_PLL_EN_MT6983,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN_MT6983,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN_MT6983,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN_MT6983,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN_MT6983,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE_MT6983,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE_MT6983,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE_MT6983,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE_MT6983,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE_MT6983,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE_MT6983,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE_MT6983,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE_MT6983,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE_MT6983,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE_MT6983,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6983,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6983,
	.dsi_get_pcw = _dsi_get_pcw_mt6983,
	.backup_mipitx_impedance = backup_mipitx_impedance_mt6983,
	.refill_mipitx_impedance = refill_mipitx_impedance_mt6983,
};

static const struct mtk_mipitx_data mt6895_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG_MT6983,
	.dsi_pll_en = RG_DSI_PLL_EN_MT6983,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN_MT6983,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN_MT6983,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN_MT6983,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN_MT6983,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE_MT6983,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE_MT6983,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE_MT6983,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE_MT6983,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE_MT6983,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE_MT6983,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE_MT6983,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE_MT6983,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE_MT6983,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE_MT6983,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6983,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6983,
	.dsi_get_pcw = _dsi_get_pcw_mt6983,
	.backup_mipitx_impedance = backup_mipitx_impedance_mt6983,
	.refill_mipitx_impedance = refill_mipitx_impedance_mt6983,
	.pll_rate_switch_gce = mtk_mipi_tx_pll_rate_switch_gce_mt6895,
};

static const struct mtk_mipitx_data mt6983_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG_MT6983,
	.dsi_pll_en = RG_DSI_PLL_EN_MT6983,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN_MT6983,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN_MT6983,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN_MT6983,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN_MT6983,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE_MT6983,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE_MT6983,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE_MT6983,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE_MT6983,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE_MT6983,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE_MT6983,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE_MT6983,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE_MT6983,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE_MT6983,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE_MT6983,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6983,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6983,
	.dsi_get_pcw = _dsi_get_pcw_mt6983,
	.backup_mipitx_impedance = backup_mipitx_impedance_mt6983,
	.refill_mipitx_impedance = refill_mipitx_impedance_mt6983,
};

static const struct mtk_mipitx_data mt6895_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG_MT6983,
	.dsi_pll_en = RG_DSI_PLL_EN_MT6983,
	.dsi_ssc_en = RG_DSI_PLL_SDM_SSC_EN_MT6983,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN_MT6983,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN_MT6983,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN_MT6983,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN_MT6983,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE_MT6983,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE_MT6983,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE_MT6983,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE_MT6983,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE_MT6983,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE_MT6983,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE_MT6983,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE_MT6983,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE_MT6983,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE_MT6983,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6983,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6983,
	.dsi_get_pcw = _dsi_get_pcw_mt6983,
	.backup_mipitx_impedance = backup_mipitx_impedance_mt6983,
	.refill_mipitx_impedance = refill_mipitx_impedance_mt6983,
};

static const struct mtk_mipitx_data mt6873_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6873,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6873,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6873_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6873,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6873,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6853_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6853,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6853,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6833_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6833,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6833,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6879_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6879,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6879,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6879_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
		.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6879,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6879,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6855_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare_mt6855,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare_mt6855,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct mtk_mipitx_data mt6855_mipitx_cphy_data = {
	.mppll_preserve = (0 << 8),
		.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_cphy_prepare_mt6855,
	.pll_unprepare = mtk_mipi_tx_pll_cphy_unprepare_mt6855,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};


static const struct mtk_mipitx_data mt8173_mipitx_data = {
	.mppll_preserve = (0 << 8),
	.dsi_pll_sdm_pcw_chg = RG_DSI_PLL_SDM_PCW_CHG,
	.dsi_pll_en = RG_DSI_PLL_EN,
	.ck_sw_ctl_en = MIPITX_CK_SW_CTL_EN,
	.d0_sw_ctl_en = MIPITX_D0_SW_CTL_EN,
	.d1_sw_ctl_en = MIPITX_D1_SW_CTL_EN,
	.d2_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d3_sw_ctl_en = MIPITX_D2_SW_CTL_EN,
	.d0_sw_lptx_pre_oe = MIPITX_D0_SW_LPTX_PRE_OE,
	.d0c_sw_lptx_pre_oe = MIPITX_D0C_SW_LPTX_PRE_OE,
	.d1_sw_lptx_pre_oe = MIPITX_D1_SW_LPTX_PRE_OE,
	.d1c_sw_lptx_pre_oe = MIPITX_D1C_SW_LPTX_PRE_OE,
	.d2_sw_lptx_pre_oe = MIPITX_D2_SW_LPTX_PRE_OE,
	.d2c_sw_lptx_pre_oe = MIPITX_D2C_SW_LPTX_PRE_OE,
	.d3_sw_lptx_pre_oe = MIPITX_D3_SW_LPTX_PRE_OE,
	.d3c_sw_lptx_pre_oe = MIPITX_D3C_SW_LPTX_PRE_OE,
	.ck_sw_lptx_pre_oe = MIPITX_CK_SW_LPTX_PRE_OE,
	.ckc_sw_lptx_pre_oe = MIPITX_CKC_SW_LPTX_PRE_OE,
	.pll_prepare = mtk_mipi_tx_pll_prepare,
	.pll_unprepare = mtk_mipi_tx_pll_unprepare,
	.power_on_signal = mtk_mipi_tx_power_on_signal,
	.dsi_get_pcw = _dsi_get_pcw,
	.backup_mipitx_impedance = backup_mipitx_impedance,
	.refill_mipitx_impedance = refill_mipitx_impedance,
};

static const struct of_device_id mtk_mipi_tx_match[] = {
	{.compatible = "mediatek,mt2701-mipi-tx", .data = &mt2701_mipitx_data},
	{.compatible = "mediatek,mt6779-mipi-tx", .data = &mt6779_mipitx_data},
	{.compatible = "mediatek,mt8173-mipi-tx", .data = &mt8173_mipitx_data},
	{.compatible = "mediatek,mt6885-mipi-tx", .data = &mt6885_mipitx_data},
	{.compatible = "mediatek,mt6893-mipi-tx", .data = &mt6885_mipitx_data},
	{.compatible = "mediatek,mt6983-mipi-tx", .data = &mt6983_mipitx_data},
	{.compatible = "mediatek,mt6895-mipi-tx", .data = &mt6895_mipitx_data},
	{.compatible = "mediatek,mt6873-mipi-tx", .data = &mt6873_mipitx_data},
	{.compatible = "mediatek,mt6853-mipi-tx", .data = &mt6853_mipitx_data},
	{.compatible = "mediatek,mt6833-mipi-tx", .data = &mt6833_mipitx_data},
	{.compatible = "mediatek,mt6789-mipi-tx", .data = &mt6789_mipitx_data},
	{.compatible = "mediatek,mt6879-mipi-tx", .data = &mt6879_mipitx_data},
	{.compatible = "mediatek,mt6855-mipi-tx", .data = &mt6855_mipitx_data},
	{.compatible = "mediatek,mt6789-mipi-tx-cphy",
		.data = &mt6879_mipitx_cphy_data},
	{.compatible = "mediatek,mt6873-mipi-tx-cphy",
		.data = &mt6873_mipitx_cphy_data},
	{.compatible = "mediatek,mt6879-mipi-tx-cphy",
		.data = &mt6879_mipitx_cphy_data},
	{.compatible = "mediatek,mt6885-mipi-tx-cphy",
		.data = &mt6885_mipitx_cphy_data},
	{.compatible = "mediatek,mt6893-mipi-tx-cphy",
		.data = &mt6885_mipitx_cphy_data},
	{.compatible = "mediatek,mt6983-mipi-tx-cphy",
		.data = &mt6983_mipitx_cphy_data},
	{.compatible = "mediatek,mt6895-mipi-tx-cphy",
		.data = &mt6895_mipitx_cphy_data},
	{.compatible = "mediatek,mt6855-mipi-tx-cphy",
		.data = &mt6855_mipitx_cphy_data},
	{},
};

struct platform_driver mtk_mipi_tx_driver = {
	.probe = mtk_mipi_tx_probe,
	.remove = mtk_mipi_tx_remove,
	.driver = {

			.name = "mediatek-mipi-tx",
			.of_match_table = mtk_mipi_tx_match,
		},
};

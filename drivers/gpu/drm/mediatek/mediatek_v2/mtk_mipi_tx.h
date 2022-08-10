/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_MIPITX_H_
#define _MTK_DRM_MIPITX_H_

#include <linux/phy/phy.h>
#include <linux/clk-provider.h>
#include "mtk_panel_ext.h"

#define MIPITX_LANE_CON (0x000CUL)
#define MIPITX_VOLTAGE_SEL (0x0010UL)
#define FLD_RG_DSI_HSTX_LDO_REF_SEL (0xf << 6)
#define MIPITX_PRESERVED (0x0014UL)
#define MIPITX_PLL_PWR (0x0028UL)
#define AD_DSI_PLL_SDM_PWR_ON BIT(0)
#define AD_DSI_PLL_SDM_ISO_EN BIT(1)
#define DA_DSI_PLL_SDM_PWR_ACK BIT(8)

#define MIPITX_PLL_CON0 (0x002CUL)
#define MIPITX_PLL_CON1 (0x0030UL)
#define RG_DSI_PLL_SDM_PCW_CHG BIT(0)
#define RG_DSI_PLL_EN BIT(4)
#define FLD_RG_DSI_PLL_POSDIV (0x7 << 8)
#define FLD_RG_DSI_PLL_POSDIV_ REG_FLD_MSB_LSB(10, 8)

#define MIPITX_PLL_CON2 (0x0034UL)
#define RG_DSI_PLL_SDM_SSC_EN BIT(1)

#define MIPITX_PLL_CON3 (0x0038UL)
#define MIPITX_PLL_CON4 (0x003CUL)
#define MIPITX_D2_SW_CTL_EN (0x0144UL)
#define DSI_D2_SW_CTL_EN BIT(0)
#define MIPITX_D0_SW_CTL_EN (0x0244UL)
#define DSI_D0_SW_CTL_EN BIT(0)
#define MIPITX_CK_SW_CTL_EN (0x0344UL)
#define DSI_CK_SW_CTL_EN BIT(0)
#define MIPITX_D1_SW_CTL_EN (0x0444UL)
#define DSI_D1_SW_CTL_EN BIT(0)
#define MIPITX_D3_SW_CTL_EN (0x0544UL)
#define DSI_D3_SW_CTL_EN BIT(0)

#define MIPITX_PHY_SEL0 (0x0040UL)
#define FLD_MIPI_TX_CPHY_EN (0x1 << 0)
#define FLD_MIPI_TX_PHY2_SEL (0xf << 4)
#define FLD_MIPI_TX_CPHY0BC_SEL (0xf << 8)
#define FLD_MIPI_TX_PHY0_SEL (0xf << 12)
#define FLD_MIPI_TX_PHY1AB_SEL (0xf << 16)
#define FLD_MIPI_TX_PHYC_SEL (0xf << 20)
#define FLD_MIPI_TX_CPHY1CA_SEL (0xf << 24)
#define FLD_MIPI_TX_PHY1_SEL (0xf << 28)
#define MIPITX_PHY_SEL1 (0x0044UL)
#define FLD_MIPI_TX_PHY2BC_SEL (0xf << 0)
#define FLD_MIPI_TX_PHY3_SEL (0xf << 4)
#define FLD_MIPI_TX_CPHYXXX_SEL (0xf << 8)
#define FLD_MIPI_TX_LPRX0AB_SEL (0xf << 12)
#define FLD_MIPI_TX_LPRX0BC_SEL (0xf << 16)
#define FLD_MIPI_TX_LPRX0CA_SEL (0xf << 20)
#define FLD_MIPI_TX_CPHY0_HS_SEL (0xf << 24)
#define FLD_MIPI_TX_CPHY1_HS_SEL (0xf << 26)
#define FLD_MIPI_TX_CPHY2_HS_SEL (0xf << 28)

#define MIPITX_PHY_SEL2 (0x0048UL)
#define FLD_MIPI_TX_PHY2_HSDATA_SEL (0xf << 0)
#define FLD_MIPI_TX_CPHY0BC_HSDATA_SEL (0xf << 4)
#define FLD_MIPI_TX_PHY0_HSDATA_SEL (0xf << 8)
#define FLD_MIPI_TX_PHY1AB_HSDATA_SEL (0xf << 12)
#define FLD_MIPI_TX_PHYC_HSDATA_SEL (0xf << 16)
#define FLD_MIPI_TX_CPHY1CA_HSDATA_SEL (0xf << 20)
#define FLD_MIPI_TX_PHY1_HSDATA_SEL (0xf << 24)
#define FLD_MIPI_TX_PHY2BC_HSDATA_SEL (0xf << 28)

#define MIPITX_PHY_SEL3 (0x004CUL)
#define FLD_MIPI_TX_PHY3_HSDATA_SEL (0xf << 0)

/* use to reset DPHY */
#define MIPITX_SW_CTRL_CON4 0x60

#define MIPITX_D2P_RTCODE0 (0x0100UL)
#define MIPITX_D2N_RTCODE0 (0x0114UL)
#define MIPITX_D2P_RT_DEM_CODE (0x01C8UL)
#define MIPITX_D2N_RT_DEM_CODE (0x01CCUL)

#define MIPITX_D2_CKMODE_EN (0x0128UL)
#define MIPITX_D0_CKMODE_EN (0x0228UL)
#define MIPITX_CK_CKMODE_EN (0x0328UL)
#define MIPITX_D1_CKMODE_EN (0x0428UL)
#define MIPITX_D3_CKMODE_EN (0x0528UL)

#define FLD_DSI_SW_CTL_EN BIT(0)
#define FLD_AD_DSI_PLL_SDM_PWR_ON BIT(0)
#define FLD_AD_DSI_PLL_SDM_ISO_EN BIT(1)

#define MIPITX_D0_SW_LPTX_PRE_OE	(0x0248UL)
#define MIPITX_D0C_SW_LPTX_PRE_OE	(0x0268UL)
#define MIPITX_D1_SW_LPTX_PRE_OE	(0x0448UL)
#define MIPITX_D1C_SW_LPTX_PRE_OE	(0x0468UL)
#define MIPITX_D2_SW_LPTX_PRE_OE	(0x0148UL)
#define MIPITX_D2C_SW_LPTX_PRE_OE	(0x0168UL)
#define MIPITX_D3_SW_LPTX_PRE_OE	(0x0548UL)
#define MIPITX_D3C_SW_LPTX_PRE_OE	(0x0568UL)
#define MIPITX_CK_SW_LPTX_PRE_OE	(0x0348UL)
#define MIPITX_CKC_SW_LPTX_PRE_OE	(0x0368UL)

struct mtk_mipi_tx {
	struct device *dev;
	void __iomem *regs;
	resource_size_t regs_pa;
	struct cmdq_base *cmdq_base;
	u32 data_rate;
	u32 data_rate_adpt;
	const struct mtk_mipitx_data *driver_data;
	struct clk_hw pll_hw;
	struct clk *pll;
};

struct mtk_mipitx_data {
	const u32 mppll_preserve;
	const u32 dsi_pll_sdm_pcw_chg;
	const u32 dsi_pll_en;
	const u32 dsi_ssc_en;
	const u32 ck_sw_ctl_en;
	const u32 d0_sw_ctl_en;
	const u32 d1_sw_ctl_en;
	const u32 d2_sw_ctl_en;
	const u32 d3_sw_ctl_en;
	const u32 d0_sw_lptx_pre_oe;
	const u32 d0c_sw_lptx_pre_oe;
	const u32 d1_sw_lptx_pre_oe;
	const u32 d1c_sw_lptx_pre_oe;
	const u32 d2_sw_lptx_pre_oe;
	const u32 d2c_sw_lptx_pre_oe;
	const u32 d3_sw_lptx_pre_oe;
	const u32 d3c_sw_lptx_pre_oe;
	const u32 ck_sw_lptx_pre_oe;
	const u32 ckc_sw_lptx_pre_oe;
	int (*pll_prepare)(struct clk_hw *hw);
	int (*power_on_signal)(struct phy *phy);
	void (*pll_unprepare)(struct clk_hw *hw);
	int (*power_off_signal)(struct phy *phy);
	unsigned int (*dsi_get_pcw)(unsigned long data_rate, unsigned int pcw_ratio);
	void (*backup_mipitx_impedance)(struct mtk_mipi_tx *mipi_tx);
	void (*refill_mipitx_impedance)(struct mtk_mipi_tx *mipi_tx);
	void (*pll_rate_switch_gce)(struct phy *phy, void *handle, unsigned long rate);
	int (*mipi_tx_ssc_en)(struct phy *phy, struct mtk_panel_ext *mtk_panel);
};

struct mtk_panel_ext;
extern unsigned int mipi_volt;

int mtk_mipi_tx_dump(struct phy *phy);
unsigned int mtk_mipi_tx_pll_get_rate(struct phy *phy);
int mtk_mipi_tx_dphy_lane_config(struct phy *phy,
	struct mtk_panel_ext *mtk_panel, bool is_master);
int mtk_mipi_tx_cphy_lane_config(struct phy *phy,
	struct mtk_panel_ext *mtk_panel, bool is_master);
int mtk_mipi_tx_dphy_lane_config_mt6983(struct phy *phy,
	struct mtk_panel_ext *mtk_panel, bool is_master);
int mtk_mipi_tx_cphy_lane_config_mt6983(struct phy *phy,
	struct mtk_panel_ext *mtk_panel, bool is_master);
int mtk_mipi_tx_ssc_en(struct phy *phy,
	struct mtk_panel_ext *mtk_panel);
void mtk_mipi_tx_pll_rate_set_adpt(struct phy *phy, unsigned long rate);
void mtk_mipi_tx_pll_rate_switch_gce(struct phy *phy,
	void *handle, unsigned long rate);

void mtk_mipi_tx_sw_control_en(struct phy *phy, bool en);
void mtk_mipi_tx_pre_oe_config(struct phy *phy, bool en);
inline struct mtk_mipi_tx *mtk_mipi_tx_from_clk_hw(struct clk_hw *hw);
bool mtk_is_mipi_tx_enable(struct clk_hw *hw);
void mtk_mipi_tx_clear_bits(struct mtk_mipi_tx *mipi_tx, u32 offset, u32 bits);
void mtk_mipi_tx_set_bits(struct mtk_mipi_tx *mipi_tx, u32 offset, u32 bits);
void mtk_mipi_tx_update_bits(struct mtk_mipi_tx *mipi_tx, u32 offset,
		u32 mask, u32 data);
unsigned int _dsi_get_pcw(unsigned long data_rate, unsigned int pcw_ratio);
unsigned int _dsi_get_pcw_khz(unsigned long data_rate_khz, unsigned int pcw_ratio);
void backup_mipitx_impedance(struct mtk_mipi_tx *mipi_tx);
void refill_mipitx_impedance(struct mtk_mipi_tx *mipi_tx);
#endif

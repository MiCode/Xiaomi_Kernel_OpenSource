/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_DRM_MIPITX_H_
#define _MTK_DRM_MIPITX_H_

#include <linux/phy/phy.h>

struct mtk_panel_ext;
extern unsigned int mipi_volt;

int mtk_mipi_tx_dump(struct phy *phy);
unsigned int mtk_mipi_tx_pll_get_rate(struct phy *phy);
int mtk_mipi_tx_dphy_lane_config(struct phy *phy,
	struct mtk_panel_ext *mtk_panel, bool is_master);
int mtk_mipi_tx_cphy_lane_config(struct phy *phy,
	struct mtk_panel_ext *mtk_panel, bool is_master);
void mtk_mipi_tx_pll_rate_set_adpt(struct phy *phy, unsigned long rate);
void mtk_mipi_tx_pll_rate_switch_gce(struct phy *phy,
	void *handle, unsigned long rate);

void mtk_mipi_tx_sw_control_en(struct phy *phy, bool en);
void mtk_mipi_tx_pre_oe_config(struct phy *phy, bool en);

#endif

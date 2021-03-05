/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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

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

int mtk_mipi_tx_dump(struct phy *phy);
unsigned int mtk_mipi_tx_pll_get_rate(struct phy *phy);

#endif

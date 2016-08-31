/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_TC358767_DSI2EDP_H
#define __DRIVERS_VIDEO_TEGRA_DC_TC358767_DSI2EDP_H

struct tegra_dsi2edp {
	struct tegra_dc_dsi_data *dsi;
	struct i2c_client *client_i2c;
	struct regmap *regmap;

	struct tegra_dc_mode *mode;

	bool enabled;
	bool i2c_shutdown;

	struct mutex lock;
};

#define DP0_SRC_CTRL	0x06a0
#define DP1_SRC_CTRL	0x07a0
#define SYS_PLL_PARAM	0x0918
#define DP_PHY_CTRL	0x0800
#define DP1_PLL_CTRL	0x0904
#define DP0_PLL_CTRL	0x0900
#define PXL_PLL_PARAM	0x0914
#define PXL_PLL_CTRL	0x0908
#define DP0_AUX_ADDR	0x0668
#define DP0_AUX_CFG1	0x0664
#define DP0_AUX_CFG0	0x0660
#define DP0_AUX_WDATA0	0x066c
#define DP0_SNK_LTCTRL	0x06e4
#define DP0_LTLOOP_CTRL	0x06d8
#define DP0_CTRL	0x0600
#define PPI_TX_RX_TA	0x013c
#define PPI_LPTX_TIME_CNT	0x0114
#define PPI_D0S_CLRSIPOCOUNT	0x0164
#define PPI_D1S_CLRSIPOCOUNT	0x0168
#define PPI_D2S_CLRSIPOCOUNT	0x016c
#define PPI_D3S_CLRSIPOCOUNT	0x0170
#define PPI_LANE_ENABLE		0x0134
#define DSI_LANE_ENABLE		0x0210
#define PPI_STARTPPI		0x0104
#define DSI_STARTDSI		0x0204
#define VIDEO_PATH0_CTRL	0x0450
#define H_TIMING_CTRL01		0x0454
#define H_TIMING_CTRL02		0x0458
#define V_TIMING_CTRL01		0x045c
#define V_TIMING_CTRL02		0x0460
#define VIDEO_FRAME_TIMING_UPLOAD_ENB0	0x0464
#define DP0_VIDEO_SYNC_DELAY	0x0644
#define DP0_TOT_VAL		0x0648
#define DP0_START_VAL		0x064c
#define DP0_ACTIVE_VAL		0x0650
#define DP0_SYNC_VAL		0x0654
#define DP0_MISC		0x0658
#define DP0_AUX_ADDR		0x0668
#define VIDEO_MN_GEN0		0x0610
#define VIDEO_MN_GEN1		0x0614
#define SYS_CTRL		0x0510

#endif

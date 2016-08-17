/*
 * drivers/video/tegra/dc/tc358770_dsi2edp.h
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_VIDEO_TEGRA_DC_TC358770_DSI2EDP_H
#define __DRIVERS_VIDEO_TEGRA_DC_TC358770_DSI2EDP_H

struct tegra_dc_dsi2edp_data {
	struct tegra_dc_dsi_data *dsi;
	struct i2c_client *client_i2c;
	struct regmap *regmap;

	struct tegra_dc_mode *mode;

	bool dsi2edp_enabled;

	struct mutex lock;
};

#define  TC358770_DATA0_DPHY_TX_CTRL              0x0004
#define  TC358770_CLOCK_DPHY_RX_CTRL              0x0020
#define  TC358770_DATA3W_CTRL                     0x0050
#define  TC358770_DSI0_PPI_START                  0x0104
#define  TC358770_DSI0_PPI_LPTXTIMECNT            0x0114
#define  TC358770_DSI0_PPI_LANEENABLE             0x0134
#define  TC358770_DSI0_PPI_TX_RX_TA               0x013C
#define  TC358770_PPI_CLS_ANALOG_TMR              0x0140
#define  TC358770_PPI_D0S_ANALOG_TMR              0x0144
#define  TC358770_PPI_D1S_ANALOG_TMR              0x0148
#define  TC358770_PPI_D2S_ANALOG_TMR              0x014C
#define  TC358770_PPI_D3S_ANALOG_TMR              0x0150
#define  TC358770_DSI0_PPI_D0S_CLRSIPOCOUNT       0x0164
#define  TC358770_DSI0_PPI_D1S_CLRSIPOCOUNT       0x0168
#define  TC358770_DSI0_PPI_D2S_CLRSIPOCOUNT       0x016C
#define  TC358770_DSI0_PPI_D3S_CLRSIPOCOUNT       0x0170
#define  TC358770_DSI0_DSI_START                  0x0204
#define  TC358770_DSI0_DSI_LANEENABLE             0x0210
#define  TC358770_DSI_LANESTATUS0                 0x0214
#define  TC358770_DSI_LANESTATUS1                 0x0218
#define  TC358770_DSI_INTERRUPT_STATUS            0x0220
#define  TC358770_DSI_INTERRUPT_CLEAR             0x0228
#define  TC358770_VIDEO_FRAME_CTRL                0x0450
#define  TC358770_HORIZONTAL_TIME0                0x0454
#define  TC358770_HORIZONTAL_TIME1                0x0458
#define  TC358770_VERTICAL_TIME0                  0x045C
#define  TC358770_VERTICAL_TIME1                  0x0460
#define  TC358770_VIDEO_FRAME_UPDATE_ENABLE       0x0464
#define  TC358770_CMD_CTRL                        0x0480
#define  TC358770_LR_SIZE                         0x0484
#define  TC358770_PG_SIZE                         0x0488
#define  TC358770_RM_PXL                          0x048C
#define  TC358770_HORI_SCLR_HCOEF                 0x0490
#define  TC358770_HORI_SCLR_LCOEF                 0x0494
#define  TC358770_CHIP_ID                         0x0500
#define  TC358770_SYSTEM_CTRL                     0x0510
#define  TC358770_DP_CTRL                         0x0600
#define  TC358770_NVALUE_VIDEO_CLK_REGEN          0x0614
#define  TC358770_MVALUE_AUDIO_CLK                0x0628
#define  TC358770_NVALUE_AUDIO_CLK_REGEN          0x062C
#define  TC358770_VIDEO_FRAME_OUTPUT_DELAY        0x0644
#define  TC358770_VIDEO_FRAME_SIZE                0x0648
#define  TC358770_VIDEO_FRAME_START               0x064C
#define  TC358770_VIDEO_FRAME_ACTIVE_REGION_SIZE  0x0650
#define  TC358770_VIDEO_FRAME_SYNC_WIDTH          0x0654
#define  TC358770_DP_CONFIG                       0x0658
#define  TC358770_AUX_CHANNEL_CONFIG0             0x0660
#define  TC358770_AUX_CHANNEL_CONFIG1             0x0664
#define  TC358770_AUX_CHANNEL_DPCD_ADDR           0x0668
#define  TC358770_AUX_CHANNEL_DPCD_WR_DATA0       0x066C
#define  TC358770_AUX_CHANNEL_DPCD_RD_DATA0       0x067C
#define  TC358770_AUX_CHANNEL_DPCD_RD_DATA1       0x0680
#define  TC358770_AUX_CHANNEL_STATUS              0x068C
#define  TC358770_LINK_TRAINING_CTRL              0x06A0
#define  TC358770_LINK_TRAINING_LOOP_CTRL         0x06D8
#define  TC358770_LINK_TRAINING_STATUS            0x06D0
#define  TC358770_LINK_TRAINING_SINK_CONFIG       0x06E4
#define  TC358770_PHY_CTRL                        0x0800
#define  TC358770_LINK_CLK_PLL_CTRL               0x0900
#define  TC358770_STREAM_CLK_PLL_CTRL             0x0908
#define  TC358770_STREAM_CLK_PLL_PARAM            0x0914
#define  TC358770_SYSTEM_CLK_PARAM                0x0918
#define  TC358770_DSI1_PPI_START                  0x1104
#define  TC358770_DSI1_PPI_LPTXTIMECNT            0x1114
#define  TC358770_DSI1_PPI_LANEENABLE             0x1134
#define  TC358770_DSI1_PPI_TX_RX_TA               0x113C
#define  TC358770_DSI1_PPI_D0S_CLRSIPOCOUNT       0x1164
#define  TC358770_DSI1_PPI_D1S_CLRSIPOCOUNT       0x1168
#define  TC358770_DSI1_PPI_D2S_CLRSIPOCOUNT       0x116C
#define  TC358770_DSI1_PPI_D3S_CLRSIPOCOUNT       0x1170
#define  TC358770_DSI1_DSI_START                  0x1204
#define  TC358770_DSI1_DSI_LANEENABLE             0x1210

#endif

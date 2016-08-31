/*
 * include/dt-bindings/display/tegra-panel.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA_PANEL_H
#define __TEGRA_PANEL_H

#define DEFAULT_FPGA_FREQ_KHZ	160000

#define DSI_VS_0 0
#define DSI_VS_1 1

#define TEGRA_DSI_VIDEO_NONE_BURST_MODE                 0
#define TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END   1
#define TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED         2
#define TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED            3
#define TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED         4
#define TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED           5
#define TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED        6

#define TEGRA_DSI_GANGED_SYMMETRIC_LEFT_RIGHT  1
#define TEGRA_DSI_GANGED_SYMMETRIC_EVEN_ODD    2

#define TEGRA_DSI_PACKET_CMD 0
#define TEGRA_DSI_DELAY_MS 1
#define TEGRA_DSI_GPIO_SET 2
#define TEGRA_DSI_SEND_FRAME 3
#define TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD 4

#define TEGRA_DSI_LINK0 0
#define TEGRA_DSI_LINK1 1

#define TEGRA_DSI_PIXEL_FORMAT_16BIT_P  0
#define TEGRA_DSI_PIXEL_FORMAT_18BIT_P  1
#define TEGRA_DSI_PIXEL_FORMAT_18BIT_NP 2
#define TEGRA_DSI_PIXEL_FORMAT_24BIT_P  3

#define	TEGRA_DSI_VIRTUAL_CHANNEL_0  0
#define	TEGRA_DSI_VIRTUAL_CHANNEL_1  1
#define	TEGRA_DSI_VIRTUAL_CHANNEL_2  2
#define	TEGRA_DSI_VIRTUAL_CHANNEL_3  3

#define	TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE    0
#define	TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE  1

#define	TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS 0
#define	TEGRA_DSI_VIDEO_CLOCK_TX_ONLY    1

#define DSI_GENERIC_LONG_WRITE			0x29
#define DSI_DCS_LONG_WRITE			0x39
#define DSI_GENERIC_SHORT_WRITE_1_PARAMS	0x13
#define DSI_GENERIC_SHORT_WRITE_2_PARAMS	0x23
#define DSI_DCS_WRITE_0_PARAM			0x05
#define DSI_DCS_WRITE_1_PARAM			0x15

#define DSI_DCS_SET_ADDR_MODE			0x36
#define DSI_DCS_EXIT_SLEEP_MODE			0x11
#define DSI_DCS_ENTER_SLEEP_MODE		0x10
#define DSI_DCS_SET_DISPLAY_ON			0x29
#define DSI_DCS_SET_DISPLAY_OFF			0x28
#define DSI_DCS_SET_TEARING_EFFECT_OFF		0x34
#define DSI_DCS_SET_TEARING_EFFECT_ON		0x35
#define DSI_DCS_NO_OP				0x0
#define DSI_NULL_PKT_NO_DATA			0x9
#define DSI_BLANKING_PKT_NO_DATA		0x19

#define PKT_LP		 0x40000000
#define CMD_VS		 0x01
#define CMD_VE		 0x11

#define CMD_HS		 0x21
#define CMD_HE		 0x31

#define CMD_EOT		 0x08
#define CMD_NULL	 0x09
#define CMD_SHORTW	 0x15
#define CMD_BLNK	 0x19
#define CMD_LONGW	 0x39

#define CMD_RGB		 0x00
#define CMD_RGB_16BPP	 0x0E
#define CMD_RGB_18BPP	 0x1E
#define CMD_RGB_18BPPNP  0x2E
#define CMD_RGB_24BPP	 0x3E

#define LINE_STOP 0xff
#define LEN_SHORT 0
#define LEN_HSYNC 1
#define LEN_HBP 2
#define LEN_HACTIVE3 3
#define LEN_HFP 4
#define LEN_HACTIVE5 5


#define TEGRA_DSI_DISABLE 0
#define TEGRA_DSI_ENABLE 1

#define NUMOF_PKT_SEQ	12

#define DSI_INSTANCE_0 0
#define DSI_INSTANCE_1 1

#endif /* __TEGRA_PANEL_H */

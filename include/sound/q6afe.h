/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __Q6AFE_H__
#define __Q6AFE_H__
#include <sound/apr_audio.h>

#define MSM_AFE_MONO		0
#define MSM_AFE_MONO_RIGHT	1
#define MSM_AFE_MONO_LEFT	2
#define MSM_AFE_STEREO		3

enum {
	IDX_PRIMARY_I2S_RX = 0,
	IDX_PRIMARY_I2S_TX = 1,
	IDX_PCM_RX = 2,
	IDX_PCM_TX = 3,
	IDX_SECONDARY_I2S_RX = 4,
	IDX_SECONDARY_I2S_TX = 5,
	IDX_MI2S_RX = 6,
	IDX_MI2S_TX = 7,
	IDX_HDMI_RX = 8,
	IDX_RSVD_2 = 9,
	IDX_RSVD_3 = 10,
	IDX_DIGI_MIC_TX = 11,
	IDX_VOICE_RECORD_RX = 12,
	IDX_VOICE_RECORD_TX = 13,
	IDX_VOICE_PLAYBACK_TX = 14,
	IDX_SLIMBUS_0_RX = 15,
	IDX_SLIMBUS_0_TX = 16,
	IDX_SLIMBUS_1_RX = 17,
	IDX_SLIMBUS_1_TX = 18,
	IDX_SLIMBUS_2_RX = 19,
	IDX_SLIMBUS_2_TX = 20,
	IDX_SLIMBUS_3_RX = 21,
	IDX_SLIMBUS_3_TX = 22,
	IDX_SLIMBUS_4_RX = 23,
	IDX_SLIMBUS_4_TX = 24,
	IDX_INT_BT_SCO_RX = 25,
	IDX_INT_BT_SCO_TX = 26,
	IDX_INT_BT_A2DP_RX = 27,
	IDX_INT_FM_RX = 28,
	IDX_INT_FM_TX = 29,
	AFE_MAX_PORTS
};

int afe_open(u16 port_id, union afe_port_config *afe_config, int rate);
int afe_close(int port_id);
int afe_loopback(u16 enable, u16 rx_port, u16 tx_port);
int afe_sidetone(u16 tx_port_id, u16 rx_port_id, u16 enable, uint16_t gain);
int afe_loopback_gain(u16 port_id, u16 volume);
int afe_validate_port(u16 port_id);
int afe_get_port_index(u16 port_id);
int afe_start_pseudo_port(u16 port_id);
int afe_stop_pseudo_port(u16 port_id);
int afe_port_start_nowait(u16 port_id, union afe_port_config *afe_config,
	u32 rate);
int afe_port_stop_nowait(int port_id);
#endif /* __Q6AFE_H__ */

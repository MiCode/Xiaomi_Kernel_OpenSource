/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#define MSM_AFE_MONO        0
#define MSM_AFE_MONO_RIGHT  1
#define MSM_AFE_MONO_LEFT   2
#define MSM_AFE_STEREO      3
#define MSM_AFE_4CHANNELS   4
#define MSM_AFE_6CHANNELS   6
#define MSM_AFE_8CHANNELS   8

#define MSM_AFE_I2S_FORMAT_LPCM		0
#define MSM_AFE_I2S_FORMAT_COMPR		1
#define MSM_AFE_I2S_FORMAT_IEC60958_LPCM	2
#define MSM_AFE_I2S_FORMAT_IEC60958_COMPR	3

#define MSM_AFE_PORT_TYPE_RX 0
#define MSM_AFE_PORT_TYPE_TX 1

#define RT_PROXY_DAI_001_RX	0xE0
#define RT_PROXY_DAI_001_TX	0xF0
#define RT_PROXY_DAI_002_RX	0xF1
#define RT_PROXY_DAI_002_TX	0xE1
#define VIRTUAL_ID_TO_PORTID(val) ((val & 0xF) | 0x2000)

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
	IDX_RT_PROXY_PORT_001_RX = 30,
	IDX_RT_PROXY_PORT_001_TX = 31,
	IDX_SECONDARY_PCM_RX = 32,
	IDX_SECONDARY_PCM_TX = 33,
	AFE_MAX_PORTS
};

int afe_open(u16 port_id, union afe_port_config *afe_config, int rate);
int afe_close(int port_id);
int afe_loopback(u16 enable, u16 rx_port, u16 tx_port);
int afe_loopback_cfg(u16 enable, u16 dst_port, u16 src_port, u16 mode);
int afe_sidetone(u16 tx_port_id, u16 rx_port_id, u16 enable, uint16_t gain);
int afe_loopback_gain(u16 port_id, u16 volume);
int afe_validate_port(u16 port_id);
int afe_get_port_index(u16 port_id);
int afe_start_pseudo_port(u16 port_id);
int afe_stop_pseudo_port(u16 port_id);
int afe_cmd_memory_map(u32 dma_addr_p, u32 dma_buf_sz);
int afe_cmd_memory_map_nowait(u32 dma_addr_p, u32 dma_buf_sz);
int afe_cmd_memory_unmap(u32 dma_addr_p);
int afe_cmd_memory_unmap_nowait(u32 dma_addr_p);

int afe_register_get_events(u16 port_id,
		void (*cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data);
int afe_unregister_get_events(u16 port_id);
int afe_rt_proxy_port_write(u32 buf_addr_p, int bytes);
int afe_rt_proxy_port_read(u32 buf_addr_p, int bytes);
int afe_port_start_nowait(u16 port_id, union afe_port_config *afe_config,
	u32 rate);
int afe_port_stop_nowait(int port_id);
int afe_apply_gain(u16 port_id, u16 gain);
int afe_q6_interface_prepare(void);
int afe_get_port_type(u16 port_id);
/* if port_id is virtual, convert to physical..
 * if port_id is already physical, return physical
 */
int afe_convert_virtual_to_portid(u16 port_id);

int afe_pseudo_port_start_nowait(u16 port_id);
int afe_pseudo_port_stop_nowait(u16 port_id);
#endif /* __Q6AFE_H__ */

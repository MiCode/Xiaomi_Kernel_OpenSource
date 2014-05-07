/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#ifndef __Q6AFE_V2_H__
#define __Q6AFE_V2_H__
#include <sound/apr_audio-v2.h>
#include <linux/qdsp6v2/rtac.h>

#define IN			0x000
#define OUT			0x001
#define MSM_AFE_MONO        0
#define MSM_AFE_CH_STEREO   1
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
	IDX_AFE_PORT_ID_PRIMARY_PCM_RX = 2,
	IDX_AFE_PORT_ID_PRIMARY_PCM_TX = 3,
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
	IDX_SLIMBUS_5_RX = 25,
	IDX_SLIMBUS_5_TX = 26,
	IDX_INT_BT_SCO_RX = 27,
	IDX_INT_BT_SCO_TX = 28,
	IDX_INT_BT_A2DP_RX = 29,
	IDX_INT_FM_RX = 30,
	IDX_INT_FM_TX = 31,
	IDX_RT_PROXY_PORT_001_RX = 32,
	IDX_RT_PROXY_PORT_001_TX = 33,
	IDX_AFE_PORT_ID_QUATERNARY_MI2S_RX = 34,
	IDX_AFE_PORT_ID_QUATERNARY_MI2S_TX = 35,
	IDX_AFE_PORT_ID_SECONDARY_MI2S_RX = 36,
	IDX_AFE_PORT_ID_SECONDARY_MI2S_TX = 37,
	IDX_AFE_PORT_ID_TERTIARY_MI2S_RX = 38,
	IDX_AFE_PORT_ID_TERTIARY_MI2S_TX = 39,
	IDX_AFE_PORT_ID_PRIMARY_MI2S_RX = 40,
	IDX_AFE_PORT_ID_PRIMARY_MI2S_TX = 41,
	IDX_AFE_PORT_ID_SECONDARY_PCM_RX = 42,
	IDX_AFE_PORT_ID_SECONDARY_PCM_TX = 43,
	IDX_VOICE2_PLAYBACK_TX = 44,
	IDX_SLIMBUS_6_RX = 45,
	IDX_SLIMBUS_6_TX = 46,
	IDX_SPDIF_RX = 47,
	IDX_GLOBAL_CFG,
	IDX_AUDIO_PORT_ID_I2S_RX,
	IDX_AFE_PORT_ID_SECONDARY_MI2S_RX_SD1,
	AFE_MAX_PORTS
};

enum afe_mad_type {
	MAD_HW_NONE = 0x00,
	MAD_HW_AUDIO = 0x01,
	MAD_HW_BEACON = 0x02,
	MAD_HW_ULTRASOUND = 0x04,
	MAD_SW_AUDIO = 0x05,
};

struct afe_audio_buffer {
	dma_addr_t phys;
	void       *data;
	uint32_t   used;
	uint32_t   size;/* size of buffer */
	uint32_t   actual_size; /* actual number of bytes read by DSP */
	struct      ion_handle *handle;
	struct      ion_client *client;
};

struct afe_audio_port_data {
	struct afe_audio_buffer *buf;
	uint32_t	    max_buf_cnt;
	uint32_t	    dsp_buf;
	uint32_t	    cpu_buf;
	struct list_head    mem_map_handle;
	uint32_t	    tmp_hdl;
	/* read or write locks */
	struct mutex	    lock;
	spinlock_t	    dsp_lock;
};

struct afe_audio_client {
	atomic_t	       cmd_state;
	/* Relative or absolute TS */
	uint32_t	       time_flag;
	void		       *priv;
	uint64_t	       time_stamp;
	struct mutex	       cmd_lock;
	/* idx:1 out port, 0: in port*/
	struct afe_audio_port_data port[2];
	wait_queue_head_t      cmd_wait;
	uint32_t               mem_map_handle;
};

struct aanc_data {
	bool aanc_active;
	uint16_t aanc_rx_port;
	uint16_t aanc_tx_port;
	uint32_t aanc_rx_port_sample_rate;
	uint32_t aanc_tx_port_sample_rate;
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
uint32_t afe_req_mmap_handle(struct afe_audio_client *ac);
int afe_memory_map(phys_addr_t dma_addr_p, u32 dma_buf_sz,
		struct afe_audio_client *ac);
int afe_cmd_memory_map(phys_addr_t dma_addr_p, u32 dma_buf_sz);
int afe_cmd_memory_map_nowait(int port_id, phys_addr_t dma_addr_p,
			u32 dma_buf_sz);
int afe_cmd_memory_unmap(u32 dma_addr_p);
int afe_cmd_memory_unmap_nowait(u32 dma_addr_p);
void afe_set_dtmf_gen_rx_portid(u16 rx_port_id, int set);
int afe_dtmf_generate_rx(int64_t duration_in_ms,
			 uint16_t high_freq,
			 uint16_t low_freq, uint16_t gain);
int afe_register_get_events(u16 port_id,
		void (*cb) (uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data);
int afe_unregister_get_events(u16 port_id);
int afe_rt_proxy_port_write(u32 buf_addr_p, u32 mem_map_handle, int bytes);
int afe_rt_proxy_port_read(u32 buf_addr_p, u32 mem_map_handle, int bytes);
int afe_port_start(u16 port_id, union afe_port_config *afe_config,
	u32 rate);
int afe_spk_prot_feed_back_cfg(int src_port, int dst_port,
	int l_ch, int r_ch, u32 enable);
int afe_spk_prot_get_calib_data(struct afe_spkr_prot_get_vi_calib *calib);
int afe_port_stop_nowait(int port_id);
int afe_apply_gain(u16 port_id, u16 gain);
int afe_q6_interface_prepare(void);
int afe_get_port_type(u16 port_id);
int q6afe_audio_client_buf_alloc_contiguous(unsigned int dir,
			struct afe_audio_client *ac,
			unsigned int bufsz,
			unsigned int bufcnt);
struct afe_audio_client *q6afe_audio_client_alloc(void *priv);
int q6afe_audio_client_buf_free_contiguous(unsigned int dir,
			struct afe_audio_client *ac);
void q6afe_audio_client_free(struct afe_audio_client *ac);
/* if port_id is virtual, convert to physical..
 * if port_id is already physical, return physical
 */
int afe_convert_virtual_to_portid(u16 port_id);

int afe_pseudo_port_start_nowait(u16 port_id);
int afe_pseudo_port_stop_nowait(u16 port_id);
int afe_set_lpass_clock(u16 port_id, struct afe_clk_cfg *cfg);
int afe_set_digital_codec_core_clock(u16 port_id,
			struct afe_digital_clk_cfg *cfg);
int afe_set_lpass_internal_digital_codec_clock(u16 port_id,
				struct afe_digital_clk_cfg *cfg);
int q6afe_check_osr_clk_freq(u32 freq);

int afe_send_spdif_clk_cfg(struct afe_param_id_spdif_clk_cfg *cfg,
		u16 port_id);
int afe_send_spdif_ch_status_cfg(struct afe_param_id_spdif_ch_status_cfg
		*ch_status_cfg,	u16 port_id);

int afe_spdif_port_start(u16 port_id, struct afe_spdif_port_config *spdif_port,
		u32 rate);

int afe_turn_onoff_hw_mad(u16 mad_type, u16 mad_enable);
int afe_port_set_mad_type(u16 port_id, enum afe_mad_type mad_type);
enum afe_mad_type afe_port_get_mad_type(u16 port_id);
int afe_set_config(enum afe_config_type config_type, void *config_data,
		   int arg);
void afe_clear_config(enum afe_config_type config);
bool afe_has_config(enum afe_config_type config);

void afe_set_aanc_info(struct aanc_data *aanc_info);
int afe_port_group_set_param(u16 *port_id, int channel_count);
int afe_port_group_enable(u16 enable);
int afe_unmap_rtac_block(uint32_t *mem_map_handle);
int afe_map_rtac_block(struct rtac_cal_block_data *cal_block);
#endif /* __Q6AFE_V2_H__ */

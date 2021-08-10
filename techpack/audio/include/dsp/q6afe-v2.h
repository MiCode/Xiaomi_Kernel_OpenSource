/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */
#ifndef __Q6AFE_V2_H__
#define __Q6AFE_V2_H__
#include <dsp/apr_audio-v2.h>
#include <dsp/rtac.h>

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
#define MSM_AFE_10CHANNELS   10
#define MSM_AFE_12CHANNELS   12
#define MSM_AFE_14CHANNELS   14
#define MSM_AFE_16CHANNELS   16

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
#define RT_PROXY_DAI_003_RX	0xE2
#define RT_PROXY_DAI_003_TX	0xF2
#define VIRTUAL_ID_TO_PORTID(val) ((val & 0xF) | 0x2000)
#define PORTID_TO_IDX(val)	((val & 0xF) >> 1)
#define NUM_RX_PROXY_PORTS	2

#define AFE_CLK_VERSION_V1    1
#define AFE_CLK_VERSION_V2    2

#define AFE_API_VERSION_SUPPORT_SPV3	2
#define AFE_API_VERSION_V3		3
/* for VAD and Island mode */
#define AFE_API_VERSION_V4		4
/* for VAD enable */
#define AFE_API_VERSION_V6		6
/* for Speaker Protection V4 */
#define AFE_API_VERSION_V9		9
/* for external mclk dynamic switch */
#define AFE_API_VERSION_V8		8
#define AFE_API_VERSION_V10		10

#define AFE_SAMPLING_RATE_8KHZ 8000

/* the different modes for data*/
#define BAP_UNICAST          1
#define BAP_BROADCAST        2
#define BAP_BA_SIMULCAST     3


typedef int (*routing_cb)(int port);

enum {
	/* IDX 0->4 */
	IDX_PRIMARY_I2S_RX,
	IDX_PRIMARY_I2S_TX,
	IDX_AFE_PORT_ID_PRIMARY_PCM_RX,
	IDX_AFE_PORT_ID_PRIMARY_PCM_TX,
	IDX_SECONDARY_I2S_RX,
	/* IDX 5->9 */
	IDX_SECONDARY_I2S_TX,
	IDX_MI2S_RX,
	IDX_MI2S_TX,
	IDX_HDMI_RX,
	IDX_RSVD_2,
	/* IDX 10->14 */
	IDX_RSVD_3,
	IDX_DIGI_MIC_TX,
	IDX_VOICE_RECORD_RX,
	IDX_VOICE_RECORD_TX,
	IDX_VOICE_PLAYBACK_TX,
	/* IDX 15->19 */
	IDX_SLIMBUS_0_RX,
	IDX_SLIMBUS_0_TX,
	IDX_SLIMBUS_1_RX,
	IDX_SLIMBUS_1_TX,
	IDX_SLIMBUS_2_RX,
	/* IDX 20->24 */
	IDX_SLIMBUS_2_TX,
	IDX_SLIMBUS_3_RX,
	IDX_SLIMBUS_3_TX,
	IDX_SLIMBUS_4_RX,
	IDX_SLIMBUS_4_TX,
	/* IDX 25->29 */
	IDX_SLIMBUS_5_RX,
	IDX_SLIMBUS_5_TX,
	IDX_INT_BT_SCO_RX,
	IDX_INT_BT_SCO_TX,
	IDX_INT_BT_A2DP_RX,
	/* IDX 30->34 */
	IDX_INT_FM_RX,
	IDX_INT_FM_TX,
	IDX_RT_PROXY_PORT_001_RX,
	IDX_RT_PROXY_PORT_001_TX,
	IDX_AFE_PORT_ID_QUATERNARY_MI2S_RX,
	/* IDX 35->39 */
	IDX_AFE_PORT_ID_QUATERNARY_MI2S_TX,
	IDX_AFE_PORT_ID_SECONDARY_MI2S_RX,
	IDX_AFE_PORT_ID_SECONDARY_MI2S_TX,
	IDX_AFE_PORT_ID_TERTIARY_MI2S_RX,
	IDX_AFE_PORT_ID_TERTIARY_MI2S_TX,
	/* IDX 40->44 */
	IDX_AFE_PORT_ID_PRIMARY_MI2S_RX,
	IDX_AFE_PORT_ID_PRIMARY_MI2S_TX,
	IDX_AFE_PORT_ID_SECONDARY_PCM_RX,
	IDX_AFE_PORT_ID_SECONDARY_PCM_TX,
	IDX_VOICE2_PLAYBACK_TX,
	/* IDX 45->49 */
	IDX_SLIMBUS_6_RX,
	IDX_SLIMBUS_6_TX,
	IDX_PRIMARY_SPDIF_RX,
	IDX_GLOBAL_CFG,
	IDX_AUDIO_PORT_ID_I2S_RX,
	/* IDX 50->54 */
	IDX_AFE_PORT_ID_SECONDARY_MI2S_RX_SD1,
	IDX_AFE_PORT_ID_QUINARY_MI2S_RX,
	IDX_AFE_PORT_ID_QUINARY_MI2S_TX,
	IDX_AFE_PORT_ID_SENARY_MI2S_RX,
	IDX_AFE_PORT_ID_SENARY_MI2S_TX,
	/* IDX 55->118 */
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_0,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_0,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_1,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_1,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_2,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_2,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_3,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_3,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_4,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_4,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_5,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_5,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_6,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_6,
	IDX_AFE_PORT_ID_PRIMARY_TDM_RX_7,
	IDX_AFE_PORT_ID_PRIMARY_TDM_TX_7,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_0,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_0,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_1,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_1,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_2,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_2,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_3,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_3,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_4,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_4,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_5,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_5,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_6,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_6,
	IDX_AFE_PORT_ID_SECONDARY_TDM_RX_7,
	IDX_AFE_PORT_ID_SECONDARY_TDM_TX_7,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_0,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_0,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_1,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_1,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_2,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_2,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_3,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_3,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_4,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_4,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_5,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_5,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_6,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_6,
	IDX_AFE_PORT_ID_TERTIARY_TDM_RX_7,
	IDX_AFE_PORT_ID_TERTIARY_TDM_TX_7,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_0,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_0,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_1,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_1,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_2,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_2,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_3,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_3,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_4,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_4,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_5,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_5,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_6,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_6,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_RX_7,
	IDX_AFE_PORT_ID_QUATERNARY_TDM_TX_7,
	/* IDX 119->122 */
	IDX_SLIMBUS_7_RX,
	IDX_SLIMBUS_7_TX,
	IDX_SLIMBUS_8_RX,
	IDX_SLIMBUS_8_TX,
	/* IDX 123-> 124 */
	IDX_AFE_PORT_ID_USB_RX,
	IDX_AFE_PORT_ID_USB_TX,
	/* IDX 125 */
	IDX_DISPLAY_PORT_RX,
	/* IDX 126-> 129 */
	IDX_AFE_PORT_ID_TERTIARY_PCM_RX,
	IDX_AFE_PORT_ID_TERTIARY_PCM_TX,
	IDX_AFE_PORT_ID_QUATERNARY_PCM_RX,
	IDX_AFE_PORT_ID_QUATERNARY_PCM_TX,
	/* IDX 130-> 143 */
	IDX_AFE_PORT_ID_INT0_MI2S_RX,
	IDX_AFE_PORT_ID_INT0_MI2S_TX,
	IDX_AFE_PORT_ID_INT1_MI2S_RX,
	IDX_AFE_PORT_ID_INT1_MI2S_TX,
	IDX_AFE_PORT_ID_INT2_MI2S_RX,
	IDX_AFE_PORT_ID_INT2_MI2S_TX,
	IDX_AFE_PORT_ID_INT3_MI2S_RX,
	IDX_AFE_PORT_ID_INT3_MI2S_TX,
	IDX_AFE_PORT_ID_INT4_MI2S_RX,
	IDX_AFE_PORT_ID_INT4_MI2S_TX,
	IDX_AFE_PORT_ID_INT5_MI2S_RX,
	IDX_AFE_PORT_ID_INT5_MI2S_TX,
	IDX_AFE_PORT_ID_INT6_MI2S_RX,
	IDX_AFE_PORT_ID_INT6_MI2S_TX,
	/* IDX 144-> 161 */
	IDX_AFE_PORT_ID_QUINARY_PCM_RX,
	IDX_AFE_PORT_ID_QUINARY_PCM_TX,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_0,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_0,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_1,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_1,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_2,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_2,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_3,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_3,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_4,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_4,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_5,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_5,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_6,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_6,
	IDX_AFE_PORT_ID_QUINARY_TDM_RX_7,
	IDX_AFE_PORT_ID_QUINARY_TDM_TX_7,
	/* IDX 162 to 183 */
	IDX_AFE_PORT_ID_WSA_CODEC_DMA_RX_0,
	IDX_AFE_PORT_ID_WSA_CODEC_DMA_TX_0,
	IDX_AFE_PORT_ID_WSA_CODEC_DMA_RX_1,
	IDX_AFE_PORT_ID_WSA_CODEC_DMA_TX_1,
	IDX_AFE_PORT_ID_WSA_CODEC_DMA_TX_2,
	IDX_AFE_PORT_ID_VA_CODEC_DMA_TX_0,
	IDX_AFE_PORT_ID_VA_CODEC_DMA_TX_1,
	IDX_AFE_PORT_ID_VA_CODEC_DMA_TX_2,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_0,
	IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_0,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_1,
	IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_1,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_2,
	IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_2,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_3,
	IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_3,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_4,
	IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_4,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_5,
	IDX_AFE_PORT_ID_TX_CODEC_DMA_TX_5,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_6,
	IDX_AFE_PORT_ID_RX_CODEC_DMA_RX_7,
	/* IDX 184 to 186 */
	IDX_SECONDARY_SPDIF_RX,
	IDX_PRIMARY_SPDIF_TX,
	IDX_SECONDARY_SPDIF_TX,
	/* IDX 187 to 188 */
	IDX_SLIMBUS_9_RX,
	IDX_SLIMBUS_9_TX,
	/* IDX 189 -> 191 */
	IDX_AFE_PORT_ID_SENARY_PCM_RX,
	IDX_AFE_PORT_ID_SENARY_PCM_TX,
	IDX_AFE_LOOPBACK_TX,
	/* IDX 192-> 207 */
	IDX_AFE_PORT_ID_SENARY_TDM_RX_0,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_0,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_1,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_1,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_2,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_2,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_3,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_3,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_4,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_4,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_5,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_5,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_6,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_6,
	IDX_AFE_PORT_ID_SENARY_TDM_RX_7,
	IDX_AFE_PORT_ID_SENARY_TDM_TX_7,
	/* IDX 208-> 209 */
	IDX_AFE_PORT_ID_PRIMARY_META_MI2S_RX,
	IDX_AFE_PORT_ID_SECONDARY_META_MI2S_RX,
	/* IDX 210-> 211 */
	IDX_RT_PROXY_PORT_002_RX,
	IDX_RT_PROXY_PORT_002_TX,
	/* IDX 212 */
	IDX_HDMI_RX_MS,
	IDX_AFE_PORT_ID_PSEUDOPORT_01,
	/* IDX 213-> 228 */
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_0,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_0,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_1,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_1,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_2,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_2,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_3,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_3,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_4,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_4,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_5,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_5,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_6,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_6,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_RX_7,
	IDX_AFE_PORT_ID_SEPTENARY_TDM_TX_7,
	/* IDX 229-> 244 */
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_0,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_0,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_1,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_1,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_2,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_2,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_3,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_3,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_4,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_4,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_5,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_5,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_6,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_6,
	IDX_AFE_PORT_ID_HSIF0_TDM_RX_7,
	IDX_AFE_PORT_ID_HSIF0_TDM_TX_7,
	/* IDX 244-> 259 */
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_0,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_0,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_1,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_1,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_2,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_2,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_3,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_3,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_4,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_4,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_5,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_5,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_6,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_6,
	IDX_AFE_PORT_ID_HSIF1_TDM_RX_7,
	IDX_AFE_PORT_ID_HSIF1_TDM_TX_7,
	/* IDX 260-> 275 */
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_0,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_0,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_1,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_1,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_2,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_2,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_3,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_3,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_4,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_4,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_5,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_5,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_6,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_6,
	IDX_AFE_PORT_ID_HSIF2_TDM_RX_7,
	IDX_AFE_PORT_ID_HSIF2_TDM_TX_7,
	AFE_MAX_PORTS
};

enum afe_mad_type {
	MAD_HW_NONE = 0x00,
	MAD_HW_AUDIO = 0x01,
	MAD_HW_BEACON = 0x02,
	MAD_HW_ULTRASOUND = 0x04,
	MAD_SW_AUDIO = 0x05,
};

enum afe_cal_mode {
	AFE_CAL_MODE_DEFAULT = 0x00,
	AFE_CAL_MODE_NONE,
};

enum afe_vad_cfg_type {
	AFE_VAD_ENABLE = 0x00,
	AFE_VAD_PREROLL,
};

struct vad_config {
	u32 is_enable;
	u32 pre_roll;
};

enum afe_mclk_src_id {
	MCLK_SRC_INT = 0x00,
	MCLK_SRC_EXT_0 = 0x01,
	MCLK_SRC_MAX,
};

enum afe_mclk_freq {
	MCLK_FREQ_MIN = 0,
	MCLK_FREQ_11P2896_MHZ = MCLK_FREQ_MIN,
	MCLK_FREQ_12P288_MHZ,
	MCLK_FREQ_16P384_MHZ,
	MCLK_FREQ_22P5792_MHZ,
	MCLK_FREQ_24P576_MHZ,
	MCLK_FREQ_MAX,
};

#define Q6AFE_EXT_MCLK_FREQ_DEFAULT 0

struct afe_audio_buffer {
	dma_addr_t phys;
	void       *data;
	uint32_t   used;
	uint32_t   size;/* size of buffer */
	uint32_t   actual_size; /* actual number of bytes read by DSP */
	struct     dma_buf *dma_buf;
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
	int level;
};

int afe_open(u16 port_id, union afe_port_config *afe_config, int rate);
int afe_close(int port_id);
int afe_loopback(u16 enable, u16 rx_port, u16 tx_port);
int afe_sidetone_enable(u16 tx_port_id, u16 rx_port_id, bool enable);
int afe_set_display_stream(u16 rx_port_id, u32 stream_idx, u32 ctl_idx);
int afe_loopback_gain(u16 port_id, u16 volume);
int afe_validate_port(u16 port_id);
int afe_get_port_index(u16 port_id);
int afe_get_topology(int port_id);
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
		void (*cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data);
int afe_unregister_get_events(u16 port_id);
int afe_rt_proxy_port_write(phys_addr_t buf_addr_p,
			u32 mem_map_handle, int bytes);
int afe_rt_proxy_port_read(phys_addr_t buf_addr_p,
			u32 mem_map_handle, int bytes, int id);
void afe_set_cal_mode(u16 port_id, enum afe_cal_mode afe_cal_mode);
void afe_set_vad_cfg(u32 vad_enable, u32 preroll_config,
		     u32 port_id);
void afe_set_island_mode_cfg(u16 port_id, u32 enable_flag);
void afe_get_island_mode_cfg(u16 port_id, u32 *enable_flag);
int afe_send_cdc_dma_data_align(u16 port_id, u32 cdc_dma_data_align);
int afe_set_power_mode_cfg(u16 port_id, u32 enable_flag);
int afe_get_power_mode_cfg(u16 port_id, u32 *enable_flag);
int afe_port_start(u16 port_id, union afe_port_config *afe_config,
	u32 rate);
int afe_set_tws_channel_mode(u32 foramt, u16 port_id, u32 channel_mode);
int afe_set_lc3_channel_mode(u32 foramt, u16 port_id, u32 channel_mode);
int afe_port_start_v2(u16 port_id, union afe_port_config *afe_config,
		      u32 rate, u16 afe_in_channels, u16 afe_in_bit_width,
		      struct afe_enc_config *enc_config,
		      struct afe_dec_config *dec_config);
int afe_port_start_v3(u16 port_id, union afe_port_config *afe_config,
		      u32 rate, u16 afe_in_channels, u16 afe_in_bit_width,
		      struct afe_enc_config *enc_config,
		      struct afe_dec_config *dec_config,
		      struct afe_ttp_config *ttp_config);
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
int afe_set_lpass_clock_v2(u16 port_id, struct afe_clk_set *cfg);
void afe_set_cps_config(int src_port,
			struct afe_cps_hw_intf_cfg *cps_config,
			u32 ch_mask);
int afe_set_lpass_clk_cfg(int index, struct afe_clk_set *cfg);
int afe_set_digital_codec_core_clock(u16 port_id,
			struct afe_digital_clk_cfg *cfg);
int afe_set_lpass_internal_digital_codec_clock(u16 port_id,
				struct afe_digital_clk_cfg *cfg);
int afe_enable_lpass_core_shared_clock(u16 port_id, u32 enable);

int q6afe_check_osr_clk_freq(u32 freq);

int afe_send_spdif_clk_cfg(struct afe_param_id_spdif_clk_cfg *cfg,
		u16 port_id);
int afe_send_spdif_ch_status_cfg(struct afe_param_id_spdif_ch_status_cfg
		*ch_status_cfg,	u16 port_id);

int afe_spdif_port_start(u16 port_id, struct afe_spdif_port_config *spdif_port,
		u32 rate);

int afe_spdif_reg_event_cfg(u16 port_id, u16 reg_flag,
		void (*cb)(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv),
		void *private_data);

int afe_turn_onoff_hw_mad(u16 mad_type, u16 mad_enable);
int afe_port_set_mad_type(u16 port_id, enum afe_mad_type mad_type);
enum afe_mad_type afe_port_get_mad_type(u16 port_id);
int afe_set_config(enum afe_config_type config_type, void *config_data,
		   int arg);
void afe_clear_config(enum afe_config_type config);
bool afe_has_config(enum afe_config_type config);

void afe_set_aanc_info(struct aanc_data *aanc_info);
int afe_set_aanc_noise_level(int val);
int afe_port_group_set_param(u16 group_id,
	union afe_port_group_config *afe_group_config);
int afe_port_group_enable(u16 group_id,
	union afe_port_group_config *afe_group_config, u16 enable,
	struct afe_param_id_tdm_lane_cfg *lane_cfg);
int afe_unmap_rtac_block(uint32_t *mem_map_handle);
int afe_map_rtac_block(struct rtac_cal_block_data *cal_block);
int afe_send_slot_mapping_cfg(
	struct afe_param_id_slot_mapping_cfg *slot_mapping_cfg,
	u16 port_id);
int afe_send_custom_tdm_header_cfg(
	struct afe_param_id_custom_tdm_header_cfg *custom_tdm_header_cfg,
	u16 port_id);
int afe_tdm_port_start(u16 port_id, struct afe_tdm_port_config *tdm_port,
		       u32 rate, u16 num_groups);
void afe_set_routing_callback(routing_cb cb);
int afe_port_send_logging_cfg(u16 port_id,
	struct afe_param_id_port_data_log_disable_t *log_disable);
int afe_get_av_dev_drift(struct afe_param_id_dev_timing_stats *timing_stats,
		u16 port);
int afe_get_sp_rx_tmax_xmax_logging_data(
		struct afe_sp_rx_tmax_xmax_logging_param *xt_logging,
		u16 port_id);
int afe_cal_init_hwdep(void *card);
int afe_send_port_island_mode(u16 port_id);
int afe_send_port_power_mode(u16 port_id);
int afe_send_port_vad_cfg_params(u16 port_id);
#ifdef CONFIG_MSM_CSPL
int afe_apr_send_pkt_crus(void *data, int index, int set);
int crus_afe_port_close(u16 port_id);
int crus_afe_port_start(u16 port_id);
#endif
int afe_send_cmd_wakeup_register(void *handle, bool enable);
void afe_register_wakeup_irq_callback(
	void (*afe_cb_wakeup_irq)(void *handle));
int afe_get_doa_tracking_mon(u16 port_id,
	struct doa_tracking_mon_param *doa_tracking_data);
int afe_set_pll_clk_drift(u16 port_id, int32_t set_clk_drift,
			  uint32_t clk_reset);
int afe_set_clk_id(u16 port_id, uint32_t clk_id);
void afe_set_lsm_afe_port_id(int idx, int lsm_port);

enum {
	AFE_LPASS_CORE_HW_BLOCK_ID_NONE,
	AFE_LPASS_CORE_HW_RSVD,
	AFE_LPASS_CORE_HW_BLOCK_ID_AVTIMER,
	AFE_LPASS_CORE_HW_MACRO_BLOCK,
	AFE_LPASS_CORE_HW_DCODEC_BLOCK,
	AFE_LPASS_CORE_HW_VOTE_MAX
};
int afe_set_source_clk(u16 port_id, const char *clk_src);
void afe_set_clk_src_array(const char *clk_src[CLK_SRC_MAX]);
int afe_set_mclk_src_cfg(u16 port_id, uint32_t mclk_src_id, uint32_t mclk_freq);

typedef int (*afe_enable_mclk_and_get_info_cb_func) (void *private_data,
			uint32_t enable, uint32_t mclk_freq,
			struct afe_param_id_clock_set_v2_t *dyn_mclk_cfg);

int afe_register_ext_mclk_cb(afe_enable_mclk_and_get_info_cb_func fn1,
				void *private_data);
void afe_unregister_ext_mclk_cb(void);

#define AFE_LPASS_CORE_HW_BLOCK_ID_NONE                        0
#define AFE_LPASS_CORE_HW_BLOCK_ID_AVTIMER                     2
#define AFE_LPASS_CORE_HW_MACRO_BLOCK                          3

/* Handles audio-video timer (avtimer) and BTSC vote requests from clients.
 */
#define AFE_CMD_REMOTE_LPASS_CORE_HW_VOTE_REQUEST            0x000100f4

struct afe_cmd_remote_lpass_core_hw_vote_request {
	struct apr_hdr hdr;
	uint32_t  hw_block_id;
	/* ID of the hardware block. */
	char client_name[8];
	/* Name of the client. */
} __packed;

#define AFE_CMD_RSP_REMOTE_LPASS_CORE_HW_VOTE_REQUEST        0x000100f5

struct afe_cmd_rsp_remote_lpass_core_hw_vote_request {
	uint32_t client_handle;
	/**< Handle of the client. */
} __packed;

#define AFE_CMD_REMOTE_LPASS_CORE_HW_DEVOTE_REQUEST            0x000100f6

struct afe_cmd_remote_lpass_core_hw_devote_request {
	struct apr_hdr hdr;
	uint32_t  hw_block_id;
	/**< ID of the hardware block.*/

	uint32_t client_handle;
	/**< Handle of the client.*/
} __packed;

int afe_vote_lpass_core_hw(uint32_t hw_block_id, char *client_name,
			uint32_t *client_handle);
int afe_unvote_lpass_core_hw(uint32_t hw_block_id, uint32_t client_handle);
int afe_get_spk_initial_cal(void);
void afe_get_spk_r0(int *spk_r0);
void afe_get_spk_t0(int *spk_t0);
int afe_get_spk_v_vali_flag(void);
void afe_get_spk_v_vali_sts(int *spk_v_vali_sts);
void afe_set_spk_initial_cal(int initial_cal);
void afe_set_spk_v_vali_flag(int v_vali_flag);
int afe_send_data(phys_addr_t buf_addr_p, u32 mem_map_handle, int bytes);
#endif /* __Q6AFE_V2_H__ */

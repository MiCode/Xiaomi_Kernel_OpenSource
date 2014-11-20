/*
 *  controls_v2.h - Intel MID Platform driver header file
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: Ramesh Babu <ramesh.babu.koul@intel.com>
 *  Author: Omair M Abdullah <omair.m.abdullah@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

#ifndef __SST_CONTROLS_V2_H__
#define __SST_CONTROLS_V2_H__

/*
 * This section defines the map for the mixer widgets.
 *
 * Each mixer will be represented by single value and that value will have each
 * bit corresponding to one input
 *
 * Each out_id will correspond to one mixer and one path. Each input will be
 * represented by single bit in the register.
 */

/* mixer register ids here */
#define SST_MIX(x)		(x)

#define SST_MIX_MODEM		SST_MIX(0)
#define SST_MIX_BT		SST_MIX(1)
#define SST_MIX_CODEC0		SST_MIX(2)
#define SST_MIX_CODEC1		SST_MIX(3)
#define SST_MIX_LOOP0		SST_MIX(4)
#define SST_MIX_LOOP1		SST_MIX(5)
#define SST_MIX_LOOP2		SST_MIX(6)
#define SST_MIX_PROBE		SST_MIX(7)
#define SST_MIX_HF_SNS		SST_MIX(8)
#define SST_MIX_HF		SST_MIX(9)
#define SST_MIX_SPEECH		SST_MIX(10)
#define SST_MIX_RXSPEECH	SST_MIX(11)
#define SST_MIX_VOIP		SST_MIX(12)
#define SST_MIX_PCM0		SST_MIX(13)
#define SST_MIX_PCM1		SST_MIX(14)
#define SST_MIX_PCM2		SST_MIX(15)
#define SST_MIX_AWARE		SST_MIX(16)
#define SST_MIX_VAD		SST_MIX(17)
#define SST_MIX_FM		SST_MIX(18)

#define SST_MIX_MEDIA0		SST_MIX(19)
#define SST_MIX_MEDIA1		SST_MIX(20)

#define SST_NUM_MIX		(SST_MIX_MEDIA1 + 1)

#define SST_MIX_SWITCH		(SST_NUM_MIX + 1)
#define SST_OUT_SWITCH		(SST_NUM_MIX + 2)
#define SST_IN_SWITCH		(SST_NUM_MIX + 3)
#define SST_MUX_REG		(SST_NUM_MIX + 4)
#define SST_REG_LAST		(SST_MUX_REG)

/* last entry defines array size */
#define SST_NUM_WIDGETS		(SST_REG_LAST + 1)

/* in each mixer register we will define one bit for each input */
#define SST_MIX_IP(x)		(x)

#define SST_IP_MODEM		SST_MIX_IP(0)
#define SST_IP_BT		SST_MIX_IP(1)
#define SST_IP_CODEC0		SST_MIX_IP(2)
#define SST_IP_CODEC1		SST_MIX_IP(3)
#define SST_IP_LOOP0		SST_MIX_IP(4)
#define SST_IP_LOOP1		SST_MIX_IP(5)
#define SST_IP_LOOP2		SST_MIX_IP(6)
#define SST_IP_PROBE		SST_MIX_IP(7)
#define SST_IP_SIDETONE		SST_MIX_IP(8)
#define SST_IP_TXSPEECH		SST_MIX_IP(9)
#define SST_IP_SPEECH		SST_MIX_IP(10)
#define SST_IP_TONE		SST_MIX_IP(11)
#define SST_IP_VOIP		SST_MIX_IP(12)
#define SST_IP_PCM0		SST_MIX_IP(13)
#define SST_IP_PCM1		SST_MIX_IP(14)
#define SST_IP_LOW_PCM0		SST_MIX_IP(15)
#define SST_IP_FM		SST_MIX_IP(16)
#define SST_IP_MEDIA0		SST_MIX_IP(17)
#define SST_IP_MEDIA1		SST_MIX_IP(18)
#define SST_IP_MEDIA2		SST_MIX_IP(19)
#define SST_IP_MEDIA3		SST_MIX_IP(20)

#define SST_IP_LAST		SST_IP_MEDIA3

#define SST_SWM_INPUT_COUNT	(SST_IP_LAST + 1)
#define SST_CMD_SWM_MAX_INPUTS	6

#define SST_PATH_ID_SHIFT	8
#define SST_DEFAULT_LOCATION_ID	0xFFFF
#define SST_DEFAULT_CELL_NBR	0xFF
#define SST_DEFAULT_MODULE_ID	0xFFFF

/*
 * Audio DSP Path Ids. Specified by the audio DSP FW
 */
enum sst_path_index {
	SST_PATH_INDEX_MODEM_OUT                = (0x00 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_BT_OUT                   = (0x01 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_CODEC_OUT0               = (0x02 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_CODEC_OUT1               = (0x03 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_SPROT_LOOP_OUT           = (0x04 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA_LOOP1_OUT          = (0x05 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA_LOOP2_OUT          = (0x06 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE_OUT                = (0x07 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_HF_SNS_OUT               = (0x08 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_VOICE_UPLINK_REF2	= (0x08 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_HF_OUT                   = (0x09 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_VOICE_UPLINK_REF1	= (0x09 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_SPEECH_OUT               = (0x0A << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_VOICE_UPLINK		= (0x0A << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_RX_SPEECH_OUT            = (0x0B << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_VOICE_DOWNLINK		= (0x0B << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_VOIP_OUT                 = (0x0C << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PCM0_OUT                 = (0x0D << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PCM1_OUT                 = (0x0E << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PCM2_OUT                 = (0x0F << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_AWARE_OUT                = (0x10 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_VAD_OUT                  = (0x11 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_MEDIA0_OUT               = (0x12 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA1_OUT               = (0x13 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_FM_OUT                   = (0x14 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_PROBE1_PIPE_OUT		= (0x15 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE2_PIPE_OUT		= (0x16 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE3_PIPE_OUT		= (0x17 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE4_PIPE_OUT		= (0x18 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE5_PIPE_OUT		= (0x19 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE6_PIPE_OUT		= (0x1A << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE7_PIPE_OUT		= (0x1B << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE8_PIPE_OUT		= (0x1C << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_SIDETONE_OUT		= (0x1D << SST_PATH_ID_SHIFT),

	/* Start of input paths */
	SST_PATH_INDEX_MODEM_IN                 = (0x80 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_BT_IN                    = (0x81 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_CODEC_IN0                = (0x82 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_CODEC_IN1                = (0x83 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_SPROT_LOOP_IN            = (0x84 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA_LOOP1_IN           = (0x85 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA_LOOP2_IN           = (0x86 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_PROBE_IN                 = (0x87 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_SIDETONE_IN              = (0x88 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_TX_SPEECH_IN             = (0x89 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_SPEECH_IN                = (0x8A << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_TONE_IN                  = (0x8B << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_VOIP_IN                  = (0x8C << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_PCM0_IN                  = (0x8D << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PCM1_IN                  = (0x8E << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_MEDIA0_IN                = (0x8F << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA1_IN                = (0x90 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_MEDIA2_IN                = (0x91 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_FM_IN                    = (0x92 << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_PROBE1_PIPE_IN           = (0x93 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE2_PIPE_IN           = (0x94 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE3_PIPE_IN           = (0x95 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE4_PIPE_IN           = (0x96 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE5_PIPE_IN           = (0x97 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE6_PIPE_IN           = (0x98 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE7_PIPE_IN           = (0x99 << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_PROBE8_PIPE_IN           = (0x9A << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_MEDIA3_IN		= (0x9C << SST_PATH_ID_SHIFT),
	SST_PATH_INDEX_LOW_PCM0_IN		= (0x9D << SST_PATH_ID_SHIFT),

	SST_PATH_INDEX_RESERVED                 = (0xFF << SST_PATH_ID_SHIFT),
};

/*
 * switch matrix input path IDs
 */
enum sst_swm_inputs {
	SST_SWM_IN_MODEM	= (SST_PATH_INDEX_MODEM_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_BT		= (SST_PATH_INDEX_BT_IN		  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_CODEC0	= (SST_PATH_INDEX_CODEC_IN0	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_CODEC1	= (SST_PATH_INDEX_CODEC_IN1	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_SPROT_LOOP	= (SST_PATH_INDEX_SPROT_LOOP_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_MEDIA_LOOP1	= (SST_PATH_INDEX_MEDIA_LOOP1_IN  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_MEDIA_LOOP2	= (SST_PATH_INDEX_MEDIA_LOOP2_IN  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_PROBE	= (SST_PATH_INDEX_PROBE_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_SIDETONE	= (SST_PATH_INDEX_SIDETONE_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_TXSPEECH	= (SST_PATH_INDEX_TX_SPEECH_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_SPEECH	= (SST_PATH_INDEX_SPEECH_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_TONE		= (SST_PATH_INDEX_TONE_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_VOIP		= (SST_PATH_INDEX_VOIP_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_PCM0		= (SST_PATH_INDEX_PCM0_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_PCM1		= (SST_PATH_INDEX_PCM1_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_MEDIA0	= (SST_PATH_INDEX_MEDIA0_IN	  | SST_DEFAULT_CELL_NBR), /* Part of Media Mixer */
	SST_SWM_IN_MEDIA1	= (SST_PATH_INDEX_MEDIA1_IN	  | SST_DEFAULT_CELL_NBR), /* Part of Media Mixer */
	SST_SWM_IN_MEDIA2	= (SST_PATH_INDEX_MEDIA2_IN	  | SST_DEFAULT_CELL_NBR), /* Part of Media Mixer */
	SST_SWM_IN_FM		= (SST_PATH_INDEX_FM_IN		  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_MEDIA3	= (SST_PATH_INDEX_MEDIA3_IN	  | SST_DEFAULT_CELL_NBR), /* Part of Media Mixer */
	SST_SWM_IN_LOW_PCM0	= (SST_PATH_INDEX_LOW_PCM0_IN	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_IN_END		= (SST_PATH_INDEX_RESERVED	  | SST_DEFAULT_CELL_NBR)
};

/*
 * switch matrix output path IDs
 */
enum sst_swm_outputs {
	SST_SWM_OUT_MODEM	= (SST_PATH_INDEX_MODEM_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_BT		= (SST_PATH_INDEX_BT_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_CODEC0	= (SST_PATH_INDEX_CODEC_OUT0	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_CODEC1	= (SST_PATH_INDEX_CODEC_OUT1	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_SPROT_LOOP	= (SST_PATH_INDEX_SPROT_LOOP_OUT  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_MEDIA_LOOP1	= (SST_PATH_INDEX_MEDIA_LOOP1_OUT | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_MEDIA_LOOP2	= (SST_PATH_INDEX_MEDIA_LOOP2_OUT | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_PROBE	= (SST_PATH_INDEX_PROBE_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_HF_SNS	= (SST_PATH_INDEX_HF_SNS_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_HF		= (SST_PATH_INDEX_HF_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_SPEECH	= (SST_PATH_INDEX_SPEECH_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_RXSPEECH	= (SST_PATH_INDEX_RX_SPEECH_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_VOIP	= (SST_PATH_INDEX_VOIP_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_PCM0	= (SST_PATH_INDEX_PCM0_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_PCM1	= (SST_PATH_INDEX_PCM1_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_PCM2	= (SST_PATH_INDEX_PCM2_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_AWARE	= (SST_PATH_INDEX_AWARE_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_VAD		= (SST_PATH_INDEX_VAD_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_MEDIA0	= (SST_PATH_INDEX_MEDIA0_OUT	  | SST_DEFAULT_CELL_NBR), /* Part of Media Mixer */
	SST_SWM_OUT_MEDIA1	= (SST_PATH_INDEX_MEDIA1_OUT	  | SST_DEFAULT_CELL_NBR), /* Part of Media Mixer */
	SST_SWM_OUT_FM		= (SST_PATH_INDEX_FM_OUT	  | SST_DEFAULT_CELL_NBR),
	SST_SWM_OUT_END		= (SST_PATH_INDEX_RESERVED	  | SST_DEFAULT_CELL_NBR),
};

enum sst_ipc_msg {
	SST_IPC_IA_CMD = 1,
	SST_IPC_IA_SET_PARAMS,
	SST_IPC_IA_GET_PARAMS,
};

enum sst_cmd_type {
	SST_CMD_BYTES_SET = 1,
	SST_CMD_BYTES_GET = 2,
};

enum sst_task {
	SST_TASK_SBA = 1,
	SST_TASK_FBA_UL,
	SST_TASK_MMX,
	SST_TASK_AWARE,
	SST_TASK_FBA_DL,
};

enum sst_type {
	SST_TYPE_CMD = 1,
	SST_TYPE_PARAMS,
};

enum sst_flag {
	SST_FLAG_BLOCKED = 1,
	SST_FLAG_NONBLOCK,
};

/*
 * Enumeration for indexing the gain cells in VB_SET_GAIN DSP command
 */
enum sst_gain_index {
	/* GAIN IDs for SB task start here */
	SST_GAIN_INDEX_MODEM_OUT,
	SST_GAIN_INDEX_MODEM_IN,
	SST_GAIN_INDEX_BT_OUT,
	SST_GAIN_INDEX_BT_IN,
	SST_GAIN_INDEX_FM_OUT,

	SST_GAIN_INDEX_FM_IN,
	SST_GAIN_INDEX_CODEC_OUT0,
	SST_GAIN_INDEX_CODEC_OUT1,
	SST_GAIN_INDEX_CODEC_IN0,
	SST_GAIN_INDEX_CODEC_IN1,

	SST_GAIN_INDEX_SPROT_LOOP_OUT,
	SST_GAIN_INDEX_MEDIA_LOOP1_OUT,
	SST_GAIN_INDEX_MEDIA_LOOP2_OUT,
	SST_GAIN_INDEX_RX_SPEECH_OUT,
	SST_GAIN_INDEX_TX_SPEECH_IN,

	SST_GAIN_INDEX_SPEECH_OUT,
	SST_GAIN_INDEX_SPEECH_IN,
	SST_GAIN_INDEX_HF_OUT,
	SST_GAIN_INDEX_HF_SNS_OUT,
	SST_GAIN_INDEX_TONE_IN,

	SST_GAIN_INDEX_SIDETONE_IN,
	SST_GAIN_INDEX_PROBE_OUT,
	SST_GAIN_INDEX_PROBE_IN,
	SST_GAIN_INDEX_PCM0_IN_LEFT,
	SST_GAIN_INDEX_PCM0_IN_RIGHT,

	SST_GAIN_INDEX_PCM1_OUT_LEFT,
	SST_GAIN_INDEX_PCM1_OUT_RIGHT,
	SST_GAIN_INDEX_PCM1_IN_LEFT,
	SST_GAIN_INDEX_PCM1_IN_RIGHT,
	SST_GAIN_INDEX_PCM2_OUT_LEFT,

	SST_GAIN_INDEX_PCM2_OUT_RIGHT,
	SST_GAIN_INDEX_VOIP_OUT,
	SST_GAIN_INDEX_VOIP_IN,
	SST_GAIN_INDEX_AWARE_OUT,
	SST_GAIN_INDEX_VAD_OUT,

	/* Gain IDs for FBA task start here */
	SST_GAIN_INDEX_VOICE_UL,

	/* Gain IDs for MMX task start here */
	SST_GAIN_INDEX_MEDIA0_IN_LEFT,
	SST_GAIN_INDEX_MEDIA0_IN_RIGHT,
	SST_GAIN_INDEX_MEDIA1_IN_LEFT,
	SST_GAIN_INDEX_MEDIA1_IN_RIGHT,

	SST_GAIN_INDEX_MEDIA2_IN_LEFT,
	SST_GAIN_INDEX_MEDIA2_IN_RIGHT,

	SST_GAIN_INDEX_GAIN_END
};

/*
 * Audio DSP module IDs specified by FW spec
 * TODO: Update with all modules
 */
enum sst_module_id {
	SST_MODULE_ID_PCM		  = 0x0001,
	SST_MODULE_ID_MP3		  = 0x0002,
	SST_MODULE_ID_MP24		  = 0x0003,
	SST_MODULE_ID_AAC		  = 0x0004,
	SST_MODULE_ID_AACP		  = 0x0005,
	SST_MODULE_ID_EAACP		  = 0x0006,
	SST_MODULE_ID_WMA9		  = 0x0007,
	SST_MODULE_ID_WMA10		  = 0x0008,
	SST_MODULE_ID_WMA10P		  = 0x0009,
	SST_MODULE_ID_RA		  = 0x000A,
	SST_MODULE_ID_DDAC3		  = 0x000B,
	SST_MODULE_ID_TRUE_HD		  = 0x000C,
	SST_MODULE_ID_HD_PLUS		  = 0x000D,

	SST_MODULE_ID_SRC		  = 0x0064,
	SST_MODULE_ID_DOWNMIX		  = 0x0066,
	SST_MODULE_ID_GAIN_CELL		  = 0x0067,
	SST_MODULE_ID_SPROT		  = 0x006D,
	SST_MODULE_ID_BASS_BOOST	  = 0x006E,
	SST_MODULE_ID_STEREO_WDNG	  = 0x006F,
	SST_MODULE_ID_AV_REMOVAL	  = 0x0070,
	SST_MODULE_ID_MIC_EQ		  = 0x0071,
	SST_MODULE_ID_SPL		  = 0x0072,
	SST_MODULE_ID_ALGO_VTSV           = 0x0073,
	SST_MODULE_ID_NR		  = 0x0076,
	SST_MODULE_ID_BWX		  = 0x0077,
	SST_MODULE_ID_DRP		  = 0x0078,
	SST_MODULE_ID_MDRP		  = 0x0079,

	SST_MODULE_ID_ANA		  = 0x007A,
	SST_MODULE_ID_AEC		  = 0x007B,
	SST_MODULE_ID_NR_SNS		  = 0x007C,
	SST_MODULE_ID_SER		  = 0x007D,
	SST_MODULE_ID_AGC		  = 0x007E,

	SST_MODULE_ID_CNI		  = 0x007F,
	SST_MODULE_ID_CONTEXT_ALGO_AWARE  = 0x0080,
	SST_MODULE_ID_FIR_24		  = 0x0081,
	SST_MODULE_ID_IIR_24		  = 0x0082,

	SST_MODULE_ID_ASRC		  = 0x0083,
	SST_MODULE_ID_TONE_GEN		  = 0x0084,
	SST_MODULE_ID_BMF		  = 0x0086,
	SST_MODULE_ID_EDL		  = 0x0087,
	SST_MODULE_ID_GLC		  = 0x0088,

	SST_MODULE_ID_FIR_16		  = 0x0089,
	SST_MODULE_ID_IIR_16		  = 0x008A,
	SST_MODULE_ID_DNR		  = 0x008B,

	SST_MODULE_ID_VIRTUALIZER	  = 0x008C,
	SST_MODULE_ID_VISUALIZATION	  = 0x008D,
	SST_MODULE_ID_LOUDNESS_OPTIMIZER  = 0x008E,
	SST_MODULE_ID_REVERBERATION	  = 0x008F,

	SST_MODULE_ID_CNI_TX		  = 0x0090,
	SST_MODULE_ID_REF_LINE		  = 0x0091,
	SST_MODULE_ID_VOLUME		  = 0x0092,
	SST_MODULE_ID_FILT_DCR		  = 0x0094,
	SST_MODULE_ID_SLV		  = 0x009A,
	SST_MODULE_ID_NLF		  = 0x009B,
	SST_MODULE_ID_TNR		  = 0x009C,
	SST_MODULE_ID_WNR		  = 0x009D,

	SST_MODULE_ID_LOG		  = 0xFF00,

	SST_MODULE_ID_TASK		  = 0xFFFF,
};

enum sst_cmd {
	SBA_IDLE		= 14,
	SBA_VB_SET_SPEECH_PATH	= 26,
	MMX_SET_GAIN		= 33,
	SBA_VB_SET_GAIN		= 33,
	FBA_VB_RX_CNI		= 35,
	MMX_SET_GAIN_TIMECONST	= 36,
	SBA_VB_SET_TIMECONST	= 36,
	FBA_VB_ANA		= 37,
	FBA_VB_SET_FIR		= 38,
	FBA_VB_SET_IIR		= 39,
	SBA_VB_START_TONE	= 41,
	SBA_VB_STOP_TONE	= 42,
	FBA_VB_AEC		= 47,
	FBA_VB_NR_UL		= 48,
	FBA_VB_AGC		= 49,
	FBA_VB_WNR		= 52,
	FBA_VB_SLV		= 53,
	FBA_VB_NR_DL		= 55,
	SBA_PROBE		= 66,
	MMX_PROBE		= 66,
	FBA_VB_SET_BIQUAD_D_C	= 69,
	FBA_VB_DUAL_BAND_COMP	= 70,
	FBA_VB_SNS		= 72,
	FBA_VB_SER		= 78,
	FBA_VB_TX_CNI		= 80,
	SBA_VB_START		= 85,
	FBA_VB_SET_REF_LINE	= 94,
	FBA_VB_SET_DELAY_LINE	= 95,
	FBA_VB_BWX		= 104,
	FBA_VB_GMM		= 105,
	FBA_VB_GLC		= 107,
	FBA_VB_BMF		= 111,
	FBA_VB_DNR		= 113,
	MMX_SET_SWM		= 114,
	SBA_SET_SWM		= 114,
	SBA_SET_MDRP            = 116,
	SBA_HW_SET_SSP		= 117,
	SBA_SET_MEDIA_LOOP_MAP	= 118,
	SBA_SET_MEDIA_PATH	= 119,
	MMX_SET_MEDIA_PATH	= 119,
	FBA_VB_TNR_UL		= 119,
	FBA_VB_TNR_DL		= 121,
	FBA_VB_NLF		= 125,
	SBA_VB_LPRO		= 126,
	FBA_VB_MDRP		= 127,
	SBA_VB_SET_FIR          = 128,
	SBA_VB_SET_IIR          = 129,
	SBA_SET_SSP_SLOT_MAP	= 130,
	AWARE_ENV_CLASS_PARAMS	= 130,
	VAD_ENV_CLASS_PARAMS	= 2049,
};

enum sst_dsp_switch {
	SST_SWITCH_OFF = 0,
	SST_SWITCH_ON = 3,
};

enum sst_path_switch {
	SST_PATH_OFF = 0,
	SST_PATH_ON = 1,
};

enum sst_swm_state {
	SST_SWM_OFF = 0,
	SST_SWM_ON = 3,
};

#define SST_FILL_LOCATION_IDS(dst, cell_idx, pipe_id)		do {	\
		dst.location_id.p.cell_nbr_idx = (cell_idx);		\
		dst.location_id.p.path_id = (pipe_id);			\
	} while (0)
#define SST_FILL_LOCATION_ID(dst, loc_id)				(\
	dst.location_id.f = (loc_id))
#define SST_FILL_MODULE_ID(dst, mod_id)					(\
	dst.module_id = (mod_id))

#define SST_FILL_DESTINATION1(dst, id)				do {	\
		SST_FILL_LOCATION_ID(dst, (id) & 0xFFFF);		\
		SST_FILL_MODULE_ID(dst, ((id) & 0xFFFF0000) >> 16);	\
	} while (0)
#define SST_FILL_DESTINATION2(dst, loc_id, mod_id)		do {	\
		SST_FILL_LOCATION_ID(dst, loc_id);			\
		SST_FILL_MODULE_ID(dst, mod_id);			\
	} while (0)
#define SST_FILL_DESTINATION3(dst, cell_idx, path_id, mod_id)	do {	\
		SST_FILL_LOCATION_IDS(dst, cell_idx, path_id);		\
		SST_FILL_MODULE_ID(dst, mod_id);			\
	} while (0)

#define SST_FILL_DESTINATION(level, dst, ...)				\
	SST_FILL_DESTINATION##level(dst, __VA_ARGS__)
#define SST_FILL_DEFAULT_DESTINATION(dst)				\
	SST_FILL_DESTINATION(2, dst, SST_DEFAULT_LOCATION_ID, SST_DEFAULT_MODULE_ID)

struct sst_destination_id {
	union sst_location_id {
		struct {
			u8 cell_nbr_idx;	/* module index */
			u8 path_id;		/* pipe_id */
		} __packed	p;		/* part */
		u16		f;		/* full */
	} __packed location_id;
	u16	   module_id;
} __packed;

struct sst_dsp_header {
	struct sst_destination_id dst;
	u16 command_id;
	u16 length;
} __packed;

/*
 *
 * Common Commands
 *
 */
struct sst_cmd_generic {
	struct sst_dsp_header header;
} __packed;

struct swm_input_ids {
	struct sst_destination_id input_id;
} __packed;

struct sst_cmd_set_swm {
	struct sst_dsp_header header;
	struct sst_destination_id output_id;
	u16    switch_state;
	u16    nb_inputs;
	struct swm_input_ids input[SST_CMD_SWM_MAX_INPUTS];
} __packed;

struct sst_cmd_set_media_path {
	struct sst_dsp_header header;
	u16    switch_state;
} __packed;

struct pcm_cfg {
		u8 s_length:2;
		u8 rate:3;
		u8 format:3;
} __packed;

struct sst_cmd_set_speech_path {
	struct sst_dsp_header header;
	u16    switch_state;
	struct {
		u16 rsvd:8;
		struct pcm_cfg cfg;
	} config;
} __packed;

struct gain_cell {
	struct sst_destination_id dest;
	s16 cell_gain_left;
	s16 cell_gain_right;
	u16 gain_time_constant;
} __packed;

#define NUM_GAIN_CELLS 1
struct sst_cmd_set_gain_dual {
	struct sst_dsp_header header;
	u16    gain_cell_num;
	struct gain_cell cell_gains[NUM_GAIN_CELLS];
} __packed;

struct sst_cmd_set_params {
	struct sst_destination_id dst;
	u16 command_id;
	char params[0];
} __packed;

/*
 *
 * Media (MMX) commands
 *
 */

/*
 *
 * SBA commands
 *
 */
struct sst_cmd_sba_vb_start {
	struct sst_dsp_header header;
} __packed;

union sba_media_loop_params {
	struct {
		u16 rsvd:8;
		struct pcm_cfg cfg;
	} part;
	u16 full;
} __packed;

struct sst_cmd_sba_set_media_loop_map {
	struct	sst_dsp_header header;
	u16	switch_state;
	union	sba_media_loop_params param;
	u16	map;
} __packed;

struct sst_cmd_tone_stop {
	struct	sst_dsp_header header;
	u16	switch_state;
} __packed;

struct sst_cmd_sba_hw_set_ssp {
	struct sst_dsp_header header;
	u16 selection;			/* 0:SSP0(def), 1:SSP1, 2:SSP2 */

	u16 switch_state;

	u16 nb_bits_per_slots:6;        /* 0-32 bits, 24 (def) */
	u16 nb_slots:4;			/* 0-8: slots per frame  */
	u16 mode:3;			/* 0:Master, 1: Slave  */
	u16 duplex:3;

	u16 active_tx_slot_map:8;       /* Bit map, 0:off, 1:on */
	u16 reserved1:8;

	u16 active_rx_slot_map:8;       /* Bit map 0: Off, 1:On */
	u16 reserved2:8;

	u16 frame_sync_frequency;

	u16 frame_sync_polarity:8;
	u16 data_polarity:8;

	u16 frame_sync_width;           /* 1 to N clocks */
	u16 ssp_protocol:8;
	u16 start_delay:8;		/* Start delay in terms of clock ticks */
} __packed;

#define SST_MAX_TDM_SLOTS 8

struct sst_param_sba_ssp_slot_map {
	struct sst_dsp_header header;

	u16 param_id;
	u16 param_len;
	u16 ssp_index;

	u8 rx_slot_map[SST_MAX_TDM_SLOTS];
	u8 tx_slot_map[SST_MAX_TDM_SLOTS];
} __packed;

enum {
	SST_PROBE_EXTRACTOR = 0,
	SST_PROBE_INJECTOR = 1,
};

struct sst_cmd_probe {
	struct sst_dsp_header header;

	u16 switch_state;
	struct sst_destination_id probe_dst;

	u16 shared_mem:1;
	u16 probe_in:1;
	u16 probe_out:1;
	u16 rsvd_1:13;

	u16 rsvd_2:5;
	u16 probe_mode:2;
	u16 rsvd_3:1;
	struct pcm_cfg cfg;

	u16 sm_buf_id;

	u16 gain[6];
	u16 rsvd_4[9];
} __packed;

struct sst_probe_config {
	const char *name;
	u16 loc_id;
	u16 mod_id;
	u8 task_id;
	struct pcm_cfg cfg;
};

int sst_mix_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int sst_mix_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
#endif

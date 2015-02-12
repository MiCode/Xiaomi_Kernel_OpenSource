/*
 *  atom_controls.h - Intel MID Platform controls header file
 *
 *  Copyright (C) 2014 Intel Corp
 *  Author: Ramesh Babu <ramesh.babu@intel.com>
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

#ifndef __ATOM_CONTROLS_H__
#define __ATOM_CONTROLS_H__

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

#define SST_MIX_PCM3		SST_MIX(21)
#define SST_MIX_PCM4		SST_MIX(22)
#define SST_MIX_HF_SNS_3	SST_MIX(23)
#define SST_MIX_HF_SNS_4	SST_MIX(24)

#define SST_NUM_MIX		(SST_MIX_HF_SNS_4 + 1)

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
#define SST_IP_PCM2		SST_MIX_IP(21)

#define SST_IP_LAST		SST_IP_PCM2

#define SST_SWM_INPUT_COUNT	(SST_IP_LAST + 1)
#define SST_CMD_SWM_MAX_INPUTS	6

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
#endif /* __ATOM_CONTROLS_H__ */

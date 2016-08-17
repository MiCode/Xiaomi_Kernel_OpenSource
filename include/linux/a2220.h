/* include/linux/a2220.h - a2220 voice processor driver
 *
 * Copyright (C) 2009 HTC Corporation.
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

#ifndef __LINUX_A2220_H
#define __LINUX_A2220_H

#include <linux/ioctl.h>

#ifdef CONFIG_USA_OPERATOR_TMO
#define AUDIENCE_BYPASS 1
#endif

#define CONFIG_USA_MODEL_SGH_I727 0

#define CONFIG_VP_A2220
#define CONFIG_USA_MODEL_SGH_I717  1
#ifdef CONFIG_USA_MODEL_SGH_I717
#define AUDIENCE_BYPASS 1
#endif

#ifdef CONFIG_VP_A2220
#define GPIO_SELECT_I2S_AUDIENCE_QTR 124
extern unsigned int get_hw_rev(void);
#endif

int a2220_ioctl2(unsigned int cmd, unsigned long arg);
extern int a2220_port_path_change(unsigned int msg);

extern bool dualmic_enabled;

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

#define A2220_MAX_FW_SIZE	(32*1024)
struct a2220img {
	unsigned char *buf;
	unsigned img_size;
};

enum A2220_PathID {
	A2220_PATH_SUSPEND,
	A2220_PATH_INCALL_RECEIVER_NSON,
	A2220_PATH_INCALL_RECEIVER_NSOFF,
	A2220_PATH_INCALL_HEADSET,
	A2220_PATH_INCALL_SPEAKER,
	A2220_PATH_INCALL_BT,
	A2220_PATH_VR_NO_NS_RECEIVER,
	A2220_PATH_VR_NO_NS_HEADSET,
	A2220_PATH_VR_NO_NS_SPEAKER,
	A2220_PATH_VR_NO_NS_BT,
	A2220_PATH_VR_NS_RECEIVER,
	A2220_PATH_VR_NS_HEADSET,
	A2220_PATH_VR_NS_SPEAKER,
	A2220_PATH_VR_NS_BT,
	A2220_PATH_RECORD_RECEIVER,
	A2220_PATH_RECORD_HEADSET,
	A2220_PATH_RECORD_SPEAKER,
	A2220_PATH_RECORD_BT,
	A2220_PATH_CAMCORDER,
	A2220_PATH_INCALL_TTY,
#ifdef AUDIENCE_BYPASS
	A2220_PATH_BYPASS_MULTIMEDIA
#endif
};

/* noise suppression states */
enum A2220_NS_states {
	A2220_NS_STATE_AUTO,	/* leave mode as selected by driver  */
	A2220_NS_STATE_OFF,	/* disable noise suppression */
	A2220_NS_STATE_CT,	/* force close talk mode */
	A2220_NS_STATE_FT,	/* force far talk mode */
	A2220_NS_NUM_STATES
};

/* indicates if a2220_set_config() performs a full configuration or only
 * a voice processing algorithm configuration */
/* IOCTLs for Audience A2220 */
#define A2220_IOCTL_MAGIC 'u'

#define A2220_BOOTUP_INIT  _IOW(A2220_IOCTL_MAGIC, 0x01, struct a2220img *)
#define A2220_SET_CONFIG   _IOW(A2220_IOCTL_MAGIC, 0x02, enum A2220_PathID)
#define A2220_SET_NS_STATE _IOW(A2220_IOCTL_MAGIC, 0x03, enum A2220_NS_states)

/* For Diag */
#define A2220_SET_MIC_ONOFF	_IOW(A2220_IOCTL_MAGIC, 0x50, unsigned)
#define A2220_SET_MICSEL_ONOFF	_IOW(A2220_IOCTL_MAGIC, 0x51, unsigned)
#define A2220_READ_DATA		_IOR(A2220_IOCTL_MAGIC, 0x52, unsigned)
#define A2220_WRITE_MSG		_IOW(A2220_IOCTL_MAGIC, 0x53, unsigned)
#define A2220_SYNC_CMD		_IO(A2220_IOCTL_MAGIC, 0x54)
#define A2220_SET_CMD_FILE	_IOW(A2220_IOCTL_MAGIC, 0x55, unsigned)

#ifdef __KERNEL__

/* A2220 Command codes */
#define CtrlMode_LAL		0x0001	/* Level Active Low  */
#define CtrlMode_LAH		0x0002	/* Level Active High */
#define CtrlMode_FE		0x0003	/* Falling Edge */
#define CtrlMode_RE		0x0004	/* Rising  Edge */
#define A100_msg_Sync		0x80000000
#define A100_msg_Sync_Ack	0x80000000

#define A100_msg_ReadPortA	0x800B0A07

/* add the port path if needed */
#define A100_msg_PortC_A_PASS   0x805200E2
#define A100_msg_PortA_C_PASS   0x805200C8

#define A100_msg_PortC_D_PASS   0x805200EE
#define A100_msg_PortD_C_PASS   0x805200FB

#define A100_msg_PortB_A_PASS   0x805200D1
#define A100_msg_PortA_B_PASS   0x805200C4

#define A100_msg_Reset		0x8002
#define RESET_IMMEDIATE		0x0000
#define RESET_DELAYED		0x0001

#define A100_msg_BootloadInitiate	0x8003
#define A100_msg_GetDeviceParm		0x800B
#define A100_msg_SetDeviceParmID	0x800C
#define A100_msg_SetDeviceParm		0x800D

/* Get/Set PCM Device Parameter ID List */
/* PCM-0 */
#define PCM0WordLength		0x0100
#define PCM0DelFromFsTx		0x0101
#define PCM0DelFromFsRx		0x0102
#define PCM0LatchEdge		0x0103
#define PCM0Endianness		0x0105
#define PCM0TristateEnable	0x0107

/* PCM-1 */
#define PCM1WordLength		0x0200
#define PCM1DelFromFsTx		0x0201
#define PCM1DelFromFsRx		0x0202
#define PCM1LatchEdge		0x0203
#define PCM1Endianness		0x0205
#define PCM1TristateEnable	0x0207

/* Possible setting values for PCM I/F */
#define PCMWordLength_16bit	0x10	/* Default */
#define PCMWordLength_24bit	0x18
#define PCMWordLength_32bit	0x20
#define PCMLatchEdge_Tx_F_Rx_R	0x00	/* Tx/Rx on falling/rising edge */
#define PCMLatchEdge_Tx_R_Rx_F	0x03	/* Tx/Rx on falling/rising edge */
#define PCMEndianness_Little	0x00
#define PCMEndianness_Big	0x01	/* Default */
#define PCMTristate_Disable	0x00	/* Default */
#define PCMTristate_Enable	0x01

/* Get/Set ADC Device Parameter ID List */
/* ADC-0 */
#define ADC0Gain	0x0300
#define ADC0Rate	0x0301
#define ADC0CutoffFreq	0x0302

/* ADC-1 */
#define ADC1Gain	0x0400
#define ADC1Rate	0x0401
#define ADC1CutoffFreq	0x0402

/* Possible setting values for ADC I/F */
#define ADC_Gain_0db			0x00
#define ADC_Gain_6db			0x01
#define ADC_Gain_12db			0x02
#define ADC_Gain_18db			0x03
#define ADC_Gain_24db			0x04	/* Default */
#define ADC_Gain_30db			0x05
#define ADC_Rate_8kHz			0x00	/* Default */
#define ADC_Rate_16kHz			0x01
#define ADC_CutoffFreq_NO_DC_Filter	0x00
#define ADC_CutoffFreq_59p68Hz		0x01	/* Default */
#define ADC_CutoffFreq_7p46Hz		0x02
#define ADC_CutoffFreq_3p73Hz		0x03

/* Set Power State */
#define A100_msg_Sleep		0x80100001

/* Get/Set Algorithm Parameter command codes list */
#define A100_msg_GetAlgorithmParm	0x8016
#define A100_msg_SetAlgorithmParmID	0x8017
#define A100_msg_SetAlgorithmParm	0x8018

/* Get/Set Algorithm Parameter ID List (Transmit Feature) */
#define AIS_Global_Supression_Level	0x0000
#define Mic_Config			0x0002
#define AEC_Mode			0x0003
#define AEC_CNG				0x0023
#define Output_AGC			0x0004
#define Output_AGC_Target_Level		0x0005
#define Output_AGC_Noise_Floor		0x0006
#define Output_AGC_SNR_Improvement	0x0007
#define Comfort_Noise			0x001A
#define Comfort_Noise_Level		0x001B

/* Get/Set Algorithm Parameter ID List (Receive Feature) */
#define Speaker_Volume			0x0012
#define VEQ_Mode			0x0009
#define VEQ_Max_FarEnd_Limiter_Level	0x000D
#define VEQ_Noise_Estimation_Adj	0x0025
#define Receive_NS			0x000E
#define Receive_NS_Level		0x000F
#define SideTone			0x0015
#define SideTone_Gain			0x0016

/* Audio Path Commands */
/* Get/Set Transmit Digital Input Gain */
#define A100_msg_GetTxDigitalInputGain  0x801A
#define A100_msg_SetTxDigitalInputGain  0x801B

/* Get/Set Receive Digital Input Gain */
#define A100_msg_GetRcvDigitalInputGain 0x8022
#define A100_msg_SetRcvDigitalInputGain 0x8023

/* Get/Set Transmit Digital Output Gain */
#define A100_msg_GetTxDigitalOutputGain 0x801D
#define A100_msg_SetTxDigitalOutputGain 0x8015

/* Bypass */
#define A100_msg_Bypass		0x801C /* 0ff = 0x0000;
on = 0x0001 (Default) */
#define A2220_msg_VP_ON		0x801C0001
#define A2220_msg_VP_OFF	0x801C0000

/* Diagnostic API Commands */
#define A100_msg_GetMicRMS	0x8013
#define A100_msg_GetMicPeak	0x8014
#define DiagPath_Pri_Input_Mic	0x0000
#define DiagPath_Sec_Input_Mic	0x0001
#define DiagPath_Output_Mic	0x0002
#define DiagPath_Far_End_Input	0x0003
#define DiagPath_Far_End_Output	0x0004
#define A100_msg_SwapInputCh	0x8019
#define A100_msg_OutputKnownSig	0x801E

#define A2220_msg_BOOT		0x0001
#define A2220_msg_BOOT_ACK	0x01

/* general definitions */
#define TIMEOUT			20	/* ms */
#define RETRY_CNT		1
#define POLLING_RETRY_CNT	3
#define A2220_ERROR_CODE	0xffff
#define A2220_SLEEP		0
#define A2220_ACTIVE		1
#define A2220_CMD_FIFO_DEPTH	64
#define ERROR			0xffffffff

enum A2220_config_mode {
	A2220_CONFIG_FULL,
	A2220_CONFIG_VP
};

struct a2220_platform_data {
	uint32_t gpio_a2220_micsel;
	uint32_t gpio_a2220_wakeup;
	uint32_t gpio_a2220_reset;
	uint32_t gpio_a2220_int;
	uint32_t gpio_a2220_clk;
	uint32_t gpio_a2220_audience_chip_sel;
};

#endif				/* __KERNEL__ */
#endif				/* __LINUX_A2220_H */

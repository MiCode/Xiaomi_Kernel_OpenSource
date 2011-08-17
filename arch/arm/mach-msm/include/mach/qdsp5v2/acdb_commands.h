/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _MACH_QDSP5_V2_ACDB_COMMANDS_H
#define _MACH_QDSP5_V2_ACDB_COMMANDS_H

#define ACDB_VOICE_NETWORK_ID_DEFAULT		0x00010037
#define ACDB_INITIALISING			0
#define ACDB_READY				1


/* 4KB */
#define ACDB_PAGE_SIZE				0x1000

#define ACDB_CDMA_NB		0x0108b153
#define ACDB_CDMA_WB		0x0108b154
#define ACDB_GSM_NB		0x0108b155
#define ACDB_GSM_WB		0x0108b156
#define ACDB_WCDMA_NB		0x0108b157
#define ACDB_WCDMA_WB		0x0108b158


/* ACDB commands */


/* struct acdb_cmd_install_device */
#define ACDB_INSTALL_DEVICE		0x0108d245

/* struct acdb_cmd_install_device */
#define ACDB_UNINSTALL_DEVICE		0x0108d246

/* struct acdb_cmd_device */
#define ACDB_GET_DEVICE			0x0108bb92

/* struct acdb_cmd_device */
#define ACDB_SET_DEVICE			0x0108bb93

/* struct acdb_cmd_get_device_table */
#define ACDB_GET_DEVICE_TABLE		0x0108bb97

/* struct acdb_cmd_get_device_capabilities */
#define ACDB_GET_DEVICE_CAPABILITIES	0x0108f5ca

/* struct acdb_cmd_get_device_info */
#define ACDB_GET_DEVICE_INFO		0x0108f5cb

/*command to intitialize ACDB based on codec type*/
#define ACDB_CMD_INITIALIZE_FOR_ADIE	0x00011283


/* ACDB Error codes */

#define ACDB_RES_SUCCESS		0
#define ACDB_RES_FAILURE		-1
#define ACDB_RES_BADPARM		-2
#define ACDB_RES_BADSTATE		-3

#define TGTVERS_MSM7x30_BRING_UP	0x00010064



/* Algorithm Aspect IDs */

#define IID_ENABLE_FLAG			0x0108b6b9


#define IID_ENABLE_FLAG_SIZE					1
#define IID_ECHO_CANCELLER_VERSION_SIZE				2
#define IID_ECHO_CANCELLER_MODE_SIZE				2
#define IID_ECHO_CANCELLER_NOISE_SUPPRESSOR_ENABLE_SIZE		1
#define IID_ECHO_CANCELLER_PARAMETERS_SIZE			32
#define IID_ECHO_CANCELLER_NEXTGEN_NB_PARAMETERS_SIZE		(38 * 2)
#define IID_ECHO_CANCELLER_NEXTGEN_WB_PARAMETERS_SIZE		(38 * 2)
#define IID_FLUENCE_PARAMETERS_SIZE				486
#define IID_AFE_VOLUME_CONTROL_SIZE				6
#define IID_GAIN_SIZE						2
#define IID_VOICE_FIR_FILTER_SIZE				14
#define IID_VOICE_IIR_FILTER_SIZE				114
#define IID_RX_DBM_OFFSET_SIZE					2
#define IID_AGC_SIZE						36
#define IID_AVC_SIZE						80

#define IID_AUDIO_IIR_COEFF_SIZE				100
#define IID_MBADRC_PARAMETERS_SIZE				8
#define IID_MBADRC_EXT_BUFF_SIZE				392
#define IID_MBADRC_BAND_CONFIG_SIZE				100
#define IID_QAFX_PARAMETERS_SIZE				2
#define IID_QCONCERT_PARAMETERS_SIZE				2
#define IID_AUDIO_AGC_PARAMETERS_SIZE				42
#define IID_NS_PARAMETERS_SIZE					14

#define IID_ECHO_CANCELLER_VERSION			0x00010042
#define IID_ECHO_CANCELLER_MODE				0x00010043
#define IID_ECHO_CANCELLER_NOISE_SUPPRESSOR_ENABLE	0x00010044
#define IID_ECHO_CANCELLER_PARAMETERS			0x00010045
#define IID_ECHO_CANCELLER_NEXTGEN_NB_PARAMETERS	0x00010046
#define IID_ECHO_CANCELLER_NEXTGEN_WB_PARAMETERS	0x00010047
#define IID_FLUENCE_PARAMETERS				0x00010048
#define IID_AFE_VOLUME_CONTROL				0x00010049
#define IID_GAIN					0x0001004A
#define IID_VOICE_FIR_FILTER				0x0001004B
#define IID_VOICE_IIR_FILTER				0x0001004C
#define IID_AGC						0x0001004E
#define IID_AVC						0x0001004F
#define ABID_SIDETONE_GAIN				0x00010050
#define ABID_TX_VOICE_GAIN				0x00010051
#define ABID_TX_DTMF_GAIN				0x00010052
#define ABID_CODEC_TX_GAIN				0x00010053
#define ABID_HSSD					0x00010054
#define ABID_TX_AGC					0x00010055
#define ABID_TX_VOICE_FIR				0x00010056
#define ABID_TX_VOICE_IIR				0x00010057
#define ABID_ECHO_CANCELLER				0x00010058
#define ABID_ECHO_CANCELLER_NB_LVHF			0x00010059
#define ABID_ECHO_CANCELLER_WB_LVHF			0x0001005A
#define ABID_FLUENCE					0x0001005B
#define ABID_CODEC_RX_GAIN				0x0001005C
#define ABID_RX_DBM_OFFSET				0x0001005D
#define ABID_RX_AGC					0x0001005E
#define ABID_AVC					0x0001005F
#define ABID_RX_VOICE_FIR				0x00010060
#define ABID_RX_VOICE_IIR				0x00010061
#define ABID_AFE_VOL_CTRL				0x00010067


/* AUDIO IDs */
#define ABID_AUDIO_AGC_TX		0x00010068
#define ABID_AUDIO_NS_TX		0x00010069
#define ABID_VOICE_NS			0x0001006A
#define ABID_AUDIO_IIR_TX		0x0001006B
#define ABID_AUDIO_IIR_RX		0x0001006C
#define ABID_AUDIO_MBADRC_RX		0x0001006E
#define ABID_AUDIO_QAFX_RX		0x0001006F
#define ABID_AUDIO_QCONCERT_RX		0x00010070
#define ABID_AUDIO_STF_RX		0x00010071
#define ABID_AUDIO_CALIBRATION_GAIN_RX  0x00011162
#define ABID_AUDIO_CALIBRATION_GAIN_TX  0x00011149
#define ABID_AUDIO_PBE_RX               0x00011197
#define ABID_AUDIO_RMC_TX		0x00011226
#define ABID_AUDIO_FLUENCE_TX		0x00011244


#define IID_AUDIO_AGC_PARAMETERS	0x0001007E
#define IID_NS_PARAMETERS		0x00010072
#define IID_AUDIO_IIR_COEFF		0x00010073
#define IID_MBADRC_EXT_BUFF		0x00010075
#define IID_MBADRC_BAND_CONFIG		0x00010076
#define IID_MBADRC_PARAMETERS		0x00010077
#define IID_QAFX_PARAMETERS		0x00010079
#define IID_QCONCERT_PARAMETERS		0x0001007A
#define IID_STF_COEFF			0x0001007B
#define IID_AUDIO_CALIBRATION_GAIN_RX   0x00011163
#define IID_AUDIO_CALIBRATION_GAIN_TX   0x00011171
#define IID_PBE_CONFIG_PARAMETERS       0x00011198
#define IID_AUDIO_PBE_RX_ENABLE_FLAG    0x00011199
#define IID_AUDIO_RMC_PARAM		0x00011227
#define IID_AUDIO_FLUENCE_TX		0x00011245


#define TOPID_RX_TOPOLOGY_1		0x00010062
#define TOPID_TX_TOPOLOGY_1		0x00010063
#define AFERID_INT_SINK			0x00010065
#define AFERID_INT_SOURCE		0x00010066
#define AFERID_NO_SINK			0x00000000
#define AFERID_NULL_SINK		0x0108ea92


struct acdb_cmd_install_device {
	u32	command_id;
	u32	device_id;
	u32	topology_id;
	u32	afe_routing_id;
	u32	cad_routing_id;		/* see "Sample Rate Bit Mask" below */
	u32	sample_rate_mask;

	/* represents device direction: Tx, Rx (aux pga - loopback) */
	u8	device_type;
	u8	channel_config;		/* Mono or Stereo */
	u32	adie_codec_path_id;
};


struct acdb_cmd_get_device_capabilities {
	u32	command_id;
	u32	total_bytes;	/* Length in bytes allocated for buffer */
	u32	*phys_buf;	/* Physical Address of data */
};


struct acdb_cmd_get_device_info {
	u32	command_id;
	u32	device_id;
	u32	total_bytes;	/* Length in bytes allocated for buffer */
	u32	*phys_buf;	/* Physical Address of data */
};

struct acdb_cmd_device {
	u32	command_id;
	u32	device_id;
	u32	network_id;
	u32	sample_rate_id;		/* Actual sample rate value */
	u32	interface_id;		/* See interface id's above */
	u32	algorithm_block_id;	/* See enumerations above */
	u32	total_bytes;		/* Length in bytes used by buffer */
	u32	*phys_buf;		/* Physical Address of data */
};

struct acdb_cmd_get_device_table {
	u32	command_id;
	u32	device_id;
	u32	network_id;
	u32	sample_rate_id;		/* Actual sample rate value */
	u32	total_bytes;		/* Length in bytes used by buffer */
	u32	*phys_buf;		/* Physical Address of data */
};

struct acdb_result {
	/* This field is populated in response to the */
	/* ACDB_GET_DEVICE_CAPABILITIES command and indicates the total */
	/* devices whose capabilities are copied to the physical memory. */
	u32	total_devices;
	u32	*buf;			/* Physical Address of data */
	u32	used_bytes;		/* The size in bytes of the data */
	u32	result;			/* See ACDB Error codes above */
};

struct acdb_device_capability {
	u32	device_id;
	u32	sample_rate_mask;	/* See "Sample Rate Bit Mask" below */
};

struct acdb_dev_info {
	u32	cad_routing_id;
	u32	sample_rate_mask;	/* See "Sample Rate Bit Mask" below */
	u32	adsp_device_id;		/* QDSP6 device ID */
	u32	device_type;		/* Tx, Rx  (aux pga - loopback) */
	u32	channel_config;		/* Mono or Stereo */
	s32	min_volume;		/* Min volume (mB) */
	s32	max_volume;		/* Max volume (mB) */
};

/*structure is used to intialize ACDB software on modem
based on adie type detected*/
struct acdb_cmd_init_adie {
    u32 command_id;
    u32 adie_type;
};

#define ACDB_CURRENT_ADIE_MODE_UNKNOWN 0
#define ACDB_CURRENT_ADIE_MODE_TIMPANI 1
#define ACDB_CURRENT_ADIE_MODE_MARIMBA 2

/* Sample Rate Bit Mask */

/* AUX PGA devices will have a sample rate mask of 0xFFFFFFFF */
/* 8kHz              0x00000001 */
/* 11.025kHz         0x00000002 */
/* 12kHz             0x00000004 */
/* 16kHz             0x00000008 */
/* 22.5kHz           0x00000010 */
/* 24kHz             0x00000020 */
/* 32kHz             0x00000040 */
/* 44.1kHz           0x00000080 */
/* 48kHz             0x00000100 */


/* Device type enumeration */
enum {
	RX_DEVICE = 1,
	TX_DEVICE,
	AUXPGA_DEVICE,
	DEVICE_TYPE_MAX
};

#ifdef CONFIG_DEBUG_FS
/*These are ABID used for RTC*/
#define ABID_AUDIO_RTC_MBADRC_RX 0x0001118A
#define ABID_AUDIO_RTC_VOLUME_PAN_RX 0x0001118C
#define ABID_AUDIO_RTC_SPA 0x0001118E
#define ABID_AUDIO_RTC_EQUALIZER_PARAMETERS 0x0001119F

/*These are IID used for RTC*/
#define IID_AUDIO_RTC_MBADRC_PARAMETERS 0x0001118B
#define IID_AUDIO_RTC_VOLUME_PAN_PARAMETERS 0x0001118D
#define IID_AUDIO_RTC_SPA_PARAMETERS 0x0001118F
#define IID_AUDIO_RTC_EQUALIZER_PARAMETERS 0x0001119E
#define IID_AUDIO_RTC_AGC_PARAMETERS 0x000111A7
#define IID_AUDIO_RTC_TX_IIR_COEFF 0x000111A8

#endif


#endif


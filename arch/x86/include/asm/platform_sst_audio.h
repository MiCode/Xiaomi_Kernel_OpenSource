/*
 * platform_sst_audio.h:  sst audio platform data header file
 *
 * Copyright (C) 2012 Intel Corporation
 * Author: Jeeja KP <jeeja.kp@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_SST_AUDIO_H_
#define _PLATFORM_SST_AUDIO_H_

#include <linux/sfi.h>

/* The stream map status is used to dynamically assign
 * device-id to a device, for example probe device. If
 * a stream map entry is free for a device then the device-id
 * for that device will be popluated when the device is
 * opened and then the status set to IN_USE. When device
 * is closed, the strm map status is set to FREE again.
 */
enum sst_strm_map_status {
	SST_DEV_MAP_FREE = 0,
	SST_DEV_MAP_IN_USE,
};

/* Device IDs for CTP are same as stream IDs */
enum sst_audio_device_id_ctp {
	SST_PCM_OUT0 = 1,
	SST_PCM_OUT1 = 2,
	SST_COMPRESSED_OUT = 3,
	SST_CAPTURE_IN = 4,
	SST_PROBE_IN = 5,
};

enum sst_audio_task_id_mrfld {
	SST_TASK_ID_NONE = 0,
	SST_TASK_ID_SBA = 1,
	SST_TASK_ID_FBA_UL = 2,
	SST_TASK_ID_MEDIA = 3,
	SST_TASK_ID_AWARE = 4,
	SST_TASK_ID_FBA_DL = 5,
	SST_TASK_ID_MAX = SST_TASK_ID_FBA_DL,
};

/* Device IDs for Merrifield are Pipe IDs,
 * ref: LPE DSP command interface spec v0.75 */
enum sst_audio_device_id_mrfld {
	/* Output pipeline IDs */
	PIPE_ID_OUT_START = 0x0,
	PIPE_MODEM_OUT = 0x0,
	PIPE_BT_OUT = 0x1,
	PIPE_CODEC_OUT0 = 0x2,
	PIPE_CODEC_OUT1 = 0x3,
	PIPE_SPROT_LOOP_OUT = 0x4,
	PIPE_MEDIA_LOOP1_OUT = 0x5,
	PIPE_MEDIA_LOOP2_OUT = 0x6,
	PIPE_PROBE_OUT = 0x7,
	PIPE_HF_SNS_OUT = 0x8, /* VOCIE_UPLINK_REF2 */
	PIPE_HF_OUT = 0x9, /* VOICE_UPLINK_REF1 */
	PIPE_SPEECH_OUT = 0xA, /* VOICE UPLINK */
	PIPE_RxSPEECH_OUT = 0xB, /* VOICE_DOWNLINK */
	PIPE_VOIP_OUT = 0xC,
	PIPE_PCM0_OUT = 0xD,
	PIPE_PCM1_OUT = 0xE,
	PIPE_PCM2_OUT = 0xF,
	PIPE_AWARE_OUT = 0x10,
	PIPE_VAD_OUT = 0x11,
	PIPE_MEDIA0_OUT = 0x12,
	PIPE_MEDIA1_OUT = 0x13,
	PIPE_FM_OUT = 0x14,
	PIPE_PROBE1_OUT = 0x15,
	PIPE_PROBE2_OUT = 0x16,
	PIPE_PROBE3_OUT = 0x17,
	PIPE_PROBE4_OUT = 0x18,
	PIPE_PROBE5_OUT = 0x19,
	PIPE_PROBE6_OUT = 0x1A,
	PIPE_PROBE7_OUT = 0x1B,
	PIPE_PROBE8_OUT = 0x1C,
/* Input Pipeline IDs */
	PIPE_ID_IN_START = 0x80,
	PIPE_MODEM_IN = 0x80,
	PIPE_BT_IN = 0x81,
	PIPE_CODEC_IN0 = 0x82,
	PIPE_CODEC_IN1 = 0x83,
	PIPE_SPROT_LOOP_IN = 0x84,
	PIPE_MEDIA_LOOP1_IN = 0x85,
	PIPE_MEDIA_LOOP2_IN = 0x86,
	PIPE_PROBE_IN = 0x87,
	PIPE_SIDETONE_IN = 0x88,
	PIPE_TxSPEECH_IN = 0x89,
	PIPE_SPEECH_IN = 0x8A,
	PIPE_TONE_IN = 0x8B,
	PIPE_VOIP_IN = 0x8C,
	PIPE_PCM0_IN = 0x8D,
	PIPE_PCM1_IN = 0x8E,
	PIPE_MEDIA0_IN = 0x8F,
	PIPE_MEDIA1_IN = 0x90,
	PIPE_MEDIA2_IN = 0x91,
	PIPE_FM_IN = 0x92,
	PIPE_PROBE1_IN = 0x93,
	PIPE_PROBE2_IN = 0x94,
	PIPE_PROBE3_IN = 0x95,
	PIPE_PROBE4_IN = 0x96,
	PIPE_PROBE5_IN = 0x97,
	PIPE_PROBE6_IN = 0x98,
	PIPE_PROBE7_IN = 0x99,
	PIPE_PROBE8_IN = 0x9A,
	PIPE_MEDIA3_IN = 0x9C,
	PIPE_LOW_PCM0_IN = 0x9D,
	PIPE_RSVD = 0xFF,
};

enum sst_ssp_mode {
	SSP_MODE_MASTER = 0,
	SSP_MODE_SLAVE = 1,
};

enum sst_ssp_pcm_mode {
	SSP_PCM_MODE_NORMAL = 0,
	SSP_PCM_MODE_NETWORK = 1,
};

enum sst_ssp_duplex {
	SSP_DUPLEX = 0,
	SSP_RX = 1,
	SSP_TX = 2,
};

enum sst_ssp_fs_frequency {
	SSP_FS_8_KHZ = 0,
	SSP_FS_16_KHZ = 1,
	SSP_FS_44_1_KHZ = 2,
	SSP_FS_48_KHZ = 3,
};

enum sst_ssp_fs_polarity {
	SSP_FS_ACTIVE_LOW = 0,
	SSP_FS_ACTIVE_HIGH = 1,
};

enum sst_ssp_protocol {
	SSP_MODE_PCM = 0,
	SSP_MODE_I2S = 1,
};

enum sst_ssp_port_id {
	SSP_MODEM = 0,
	SSP_BT = 1,
	SSP_FM = 2,
	SSP_CODEC = 3,
};
/* physical SSP numbers */
enum {
	SST_SSP0 = 0,
	SST_SSP1,
	SST_SSP2,
	SST_SSP_LAST = SST_SSP2,
};

/* physical SSPs */
#define SST_NUM_SSPS		(SST_SSP_LAST + 1)
/* single SSP muxed between pipes */
#define SST_MAX_SSP_MUX		2
/* domains present in each pipe */
#define SST_MAX_SSP_DOMAINS	3

#define SST_SSP_FM_MUX			0
#define SST_SSP_FM_DOMAIN		0
#define SST_SSP_BT_MUX			1

struct sst_ssp_config {
	u8 ssp_id;
	u8 bits_per_slot;
	u8 slots;
	u8 ssp_mode;
	u8 pcm_mode;
	u8 data_polarity;
	u8 duplex;
	u8 ssp_protocol;
	u8 fs_frequency;
	u8 active_slot_map;
	u8 start_delay;
	u16 fs_width;
};

/* The stream map for each platform consists of an array of the below
 * stream map structure. The array index is used as the static stream-id
 * associated with a device and (dev_num,subdev_num,direction) tuple match
 * gives the device_id for the device.
 */
struct sst_dev_stream_map {
	u8 dev_num;
	u8 subdev_num;
	u8 direction;
	u8 device_id;
	u8 task_id;
	u8 status;
};

#define MAX_DESCRIPTOR_SIZE 172

struct sst_dev_effects_map {
	char	uuid[16];
	u16	algo_id;
	char	descriptor[MAX_DESCRIPTOR_SIZE];
};

struct sst_dev_effects_resource_map {
	char  uuid[16];
	unsigned int flags;
	u16 cpuLoad;
	u16 memoryUsage;
};

struct sst_dev_effects {
	struct sst_dev_effects_map *effs_map;
	struct sst_dev_effects_resource_map *effs_res_map;
	unsigned int effs_num_map;
};

struct sst_platform_data {
	/* Intel software platform id*/
	const struct soft_platform_id *spid;
	struct sst_dev_stream_map *pdev_strm_map;
	struct sst_dev_effects pdev_effs;
	unsigned int strm_map_size;
	int mux_shift[SST_NUM_SSPS];
	const int domain_shift[SST_NUM_SSPS][SST_MAX_SSP_MUX];
	const struct sst_ssp_config ssp_config[SST_NUM_SSPS][SST_MAX_SSP_MUX][SST_MAX_SSP_DOMAINS];
	int dfw_enable;
};

int sst_audio_platform_init(int dev_id);

#endif

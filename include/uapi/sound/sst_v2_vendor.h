/*
 *  sst_v2_vendor.h - Intel sst fw private data
 *
 *  Copyright (C) 2014 Intel Corp
 *  Author: Subhransu S. Prusty<subhransu.s.prusty@intel.com>
 *  Author: Mythri P K <mythri.p.k@intel.com>
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
 */

#ifndef __SST_V2_VENDOR_H__
#define __SST_V2_VENDOR_H__

#include <linux/types.h>

#define SST_V2_PLUGIN_VERSION 0x2

/* Default types range from 0~12. type can range from 0 to 0xff
 * SST types start at higher to avoid any overlapping in future */

#define SOC_CONTROL_TYPE_SST_GAIN		100
#define SOC_CONTROL_TYPE_SST_MUTE		101
#define SOC_CONTROL_TYPE_SST_ALGO_PARAMS	102
#define SOC_CONTROL_TYPE_SST_ALGO_BYPASS	103
#define SOC_CONTROL_TYPE_SST_MUX			104
#define SOC_CONTROL_TYPE_SST_MIX			106
#define SOC_CONTROL_TYPE_SST_BYTE           108
#define SOC_CONTROL_TYPE_SST_MODE           109
#define SOC_CONTROL_TYPE_SST_VOICE_MODE           110

/* REVISIT: Define sst kcontrol index */
#define SOC_CONTROL_IO_SST_GAIN\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_GAIN, \
		SOC_CONTROL_TYPE_SST_GAIN, \
		SOC_CONTROL_TYPE_VOLSW)

#define SOC_CONTROL_IO_SST_MUTE\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_GAIN, \
		SOC_CONTROL_TYPE_SST_GAIN, \
		SOC_CONTROL_TYPE_BOOL_EXT)

#define SOC_CONTROL_IO_SST_ALGO_PARAMS\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_ALGO_PARAMS, \
		SOC_CONTROL_TYPE_SST_ALGO_PARAMS, \
		SOC_CONTROL_TYPE_BYTES_EXT)


#define SOC_CONTROL_IO_SST_ALGO_BYPASS\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_ALGO_PARAMS, \
		SOC_CONTROL_TYPE_SST_ALGO_PARAMS, \
		SOC_CONTROL_TYPE_BOOL_EXT)

#define SOC_CONTROL_IO_SST_MIX\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_MIX, \
		SOC_CONTROL_TYPE_SST_MIX, \
		SOC_CONTROL_TYPE_VOLSW)

#define SOC_CONTROL_IO_SST_MUX\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_MUX, \
		SOC_CONTROL_TYPE_SST_MUX, \
		SOC_CONTROL_TYPE_SST_MUX)

#define SOC_CONTROL_IO_SST_BYTE\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_BYTE, \
		SOC_CONTROL_TYPE_SST_BYTE, \
		SOC_CONTROL_TYPE_SST_BYTE)

#define SOC_CONTROL_IO_SST_MODE\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_MODE, \
		SOC_CONTROL_TYPE_SST_MODE, \
		SOC_CONTROL_TYPE_SST_MODE)

#define SOC_CONTROL_IO_SST_VOICE_MODE\
	SOC_CONTROL_ID(SOC_CONTROL_TYPE_SST_VOICE_MODE, \
		SOC_CONTROL_TYPE_SST_VOICE_MODE, \
		SOC_CONTROL_TYPE_SST_VOICE_MODE)

#define SND_SOC_GAIN_CONTROL_NAME 44

/* Event types goes here */
/* Reserve event type 0 for no event handlers */
enum sst_event_types {
	SST_EVENT_TYPE_NONE = 0,
	SST_HOSTLESS_STREAM,
	SST_SET_BE_MODULE,
	SST_SET_MEDIA_PATH,
	SST_SET_MEDIA_LOOP,
	SST_SET_TONE_GEN,
	SST_SET_SPEECH_PATH,
	SST_SET_SWM,
	SST_EVENT_AWARE,
	SST_SET_LINKED_PATH,
	SST_SET_GENERIC_MODULE_EVENT,
	SST_EVENT_VTSV,
};

enum sst_vendor_type {
	SND_SOC_FW_SST_CONTROLS = 0x1000,
	SND_SOC_FW_SST_WIDGETS,
};

enum sst_gain_kcontrol_type {
	SST_GAIN_TLV,
	SST_GAIN_MUTE,
	SST_GAIN_RAMP_DURATION,
};

struct sst_dfw_gain_data {
	__s32 max;
	__s32 min;
	__u32 type;
	__u32 gain_val_index;
	__u32 reserved;	/* reserved */
	__u16 instance_id;
	__u16 module_id;
	__u16 pipe_id;
	__u16 task_id;
	__u16 ramp_duration;
	__s16 l_gain;
	__s16 r_gain;
	__u8 mute;
	__u8 stereo;
	char pname[SND_SOC_GAIN_CONTROL_NAME];
} __attribute__((packed));

enum sst_algo_kcontrol_type {
	SST_ALGO_PARAMS,
	SST_ALGO_BYPASS,
};

struct sst_dfw_algo_data {
	__s32 max;
	__u32 type;
	__u32 reserved; /* reserved */
	__u16 module_id;
	__u16 pipe_id;
	__u16 task_id;
	__u16 cmd_id;
	__u8 bypass;
	char params[0];
	/* params will be in driver's pvt structure */
} __attribute__((packed));

struct sst_dfw_ids {
	__u32 sample_bits;        /* sst_pcm_format->sample_bits */
	__u32 rate_min;           /* sst_pcm_format-> rate_min */
	__u32 rate_max;           /* sst_pcm_format->rate_max */
	__u32 channels_min;       /* sst_pcm_format->channels_min */
	__u32 channels_max;       /* sst_pcm_format->channels_max */
	__u32 reserved;		/* reserved */
	__u16 location_id;
	__u16 module_id;
	__u8  task_id;
	__u8  format;             /* stereo/mono */
	__u8  reg;
	char parent_wname[SND_SOC_GAIN_CONTROL_NAME];
} __attribute__((packed));

#if 0
/* sst_fw_config: FW config data organization
 * For vendor specific:
 *	hdr_xxx->size: data following header in bytes
 *	hdr_xxx->type:
 *		ex: SND_SOC_FW_MIXER, SND_SOC_FW_DAPM_WIDGET,...
 *	hdr_xxx->venodr_type: SND_SOC_FW_SST_CONTROLS, SND_SOC_FW_SST_WIDGETS
 *
 * For Generic:
 *	hdr_xxx->type: generic types for
 *		ex: SND_SOC_FW_MIXER, SND_SOC_FW_DAPM_WIDGET,...
 *
 *	hdr_xxx->vendor_type: 0
 */
struct sst_fw_config {
	struct snd_soc_fw_hdr hdr_controls;
	struct snd_soc_fw_kcontrol num_controls;
	struct snd_soc_fw_gain_control gain_control[];
	struct snd_soc_fw_algo_control algo_control[];
	struct snd_soc_fw_slot_control slot_control[];
	struct snd_soc_fw_mux_control mux_control[];
	struct snd_soc_fw_probe_control probe_control[];

	struct snd_soc_fw_hdr hdr_gen;	/* generic control types */
	struct snd_soc_fw_kcontrol num_gen_control;
	struct snd_soc_fw_mixer_control gen_mixer_control[];

	/* TODO: Add widgets */
	struct snd_soc_fw_hdr hdr_widgets;
	struct snd_soc_fw_dapm_elems num_widgets;
	struct snd_soc_fw_aifin_widget aifin_widget[];

	/* TODO: Add intercon */
} __packed;
#endif

#endif /* __SST_V2_VENDOR_H__ */

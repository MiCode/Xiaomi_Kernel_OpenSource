/*
 *  sst_vendor.h - Intel sst fw private data
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

#ifndef __SST_VENDOR_H__
#define __SST_VENDOR_H__

/* Default types range from 0~12. type can range from 0 to 0xff
 * SST types start at higher to avoid any overlapping in future */

#define SOC_CONTROL_TYPE_SST_GAIN		100
#define SOC_CONTROL_TYPE_SST_MUTE		101
#define SOC_CONTROL_TYPE_SST_ALGO_PARAMS	102
#define SOC_CONTROL_TYPE_SST_ALGO_BYPASS	103

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
#define SND_SOC_GAIN_CONTROL_NAME 44

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
	u8 stereo;
	enum sst_gain_kcontrol_type type;
	u32 gain_val_index;
	s32 max;
	s32 min;
	u16 instance_id;
	u16 module_id;
	u16 pipe_id;
	u16 task_id;
	u16 ramp_duration;
	s16 l_gain;
	s16 r_gain;
	u8 mute;
	char pname[44];
} __packed;

enum sst_algo_kcontrol_type {
	SST_ALGO_PARAMS,
	SST_ALGO_BYPASS,
};

struct sst_dfw_algo_data {
	enum sst_algo_kcontrol_type type;
	s32 max;
	u16 module_id;
	u16 pipe_id;
	u16 task_id;
	u16 cmd_id;
	u8 bypass;
	char params[0];
	/* params will be in driver's pvt structure */
} __packed;

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

#endif

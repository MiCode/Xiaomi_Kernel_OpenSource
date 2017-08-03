/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
 * This file holds USB constants and structures defined
 * by the USB Device Class Definition for Audio Devices in version 3.0.
 * Comments below reference relevant sections of the documents contained
 * in http://www.usb.org/developers/docs/devclass_docs/USB_Audio_v3.0.zip
 */

#ifndef __LINUX_USB_AUDIO_V3_H
#define __LINUX_USB_AUDIO_V3_H

#include <linux/types.h>

#define UAC3_MIXER_UNIT_V3	0x05
#define UAC3_FEATURE_UNIT_V3	0x07
#define UAC3_CLOCK_SOURCE	0x0b

#define BADD_MAXPSIZE_SYNC_MONO_16	0x0060
#define BADD_MAXPSIZE_SYNC_MONO_24	0x0090
#define BADD_MAXPSIZE_SYNC_STEREO_16	0x00c0
#define BADD_MAXPSIZE_SYNC_STEREO_24	0x0120

#define BADD_MAXPSIZE_ASYNC_MONO_16	0x0062
#define BADD_MAXPSIZE_ASYNC_MONO_24	0x0093
#define BADD_MAXPSIZE_ASYNC_STEREO_16	0x00c4
#define BADD_MAXPSIZE_ASYNC_STEREO_24	0x0126

#define BIT_RES_16_BIT		0x10
#define BIT_RES_24_BIT		0x18

#define SUBSLOTSIZE_16_BIT	0x02
#define SUBSLOTSIZE_24_BIT	0x03

#define BADD_SAMPLING_RATE	48000

#define NUM_CHANNELS_MONO	1
#define NUM_CHANNELS_STEREO	2
#define BADD_CH_CONFIG_MONO	0
#define BADD_CH_CONFIG_STEREO	3
#define CLUSTER_ID_MONO		0x0001
#define CLUSTER_ID_STEREO	0x0002

/* A.2 audio function subclass codes */
#define FULL_ADC_3_0		0x01

/* BADD Profile IDs */
#define PROF_GENERIC_IO		0x20
#define PROF_HEADPHONE		0x21
#define PROF_SPEAKER		0x22
#define PROF_MICROPHONE		0x23
#define PROF_HEADSET		0x24
#define PROF_HEADSET_ADAPTER	0x25
#define PROF_SPEAKERPHONE	0x26

/* BADD Entity IDs */
#define BADD_OUT_TERM_ID_BAOF	0x03
#define BADD_OUT_TERM_ID_BAIF	0x06
#define BADD_IN_TERM_ID_BAOF	0x01
#define BADD_IN_TERM_ID_BAIF	0x04
#define BADD_FU_ID_BAOF		0x02
#define BADD_FU_ID_BAIF		0x05
#define BADD_CLOCK_SOURCE	0x09
#define BADD_FU_ID_BAIOF	0x07
#define BADD_MU_ID_BAIOF	0x08

#define UAC_BIDIR_TERMINAL_HEADSET	0x0402
#define UAC_BIDIR_TERMINAL_SPEAKERPHONE	0x0403

#define NUM_BADD_DESCS		7

struct uac3_input_terminal_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bTerminalID;
	__u16 wTerminalType;
	__u8 bAssocTerminal;
	__u8 bCSourceID;
	__u32 bmControls;
	__u16 wClusterDescrID;
	__u16 wExTerminalDescrID;
	__u16 wConnectorsDescrID;
	__u16 wTerminalDescrStr;
} __packed;

#define UAC3_DT_INPUT_TERMINAL_SIZE	0x14

extern struct uac3_input_terminal_descriptor badd_baif_in_term_desc;
extern struct uac3_input_terminal_descriptor badd_baof_in_term_desc;

struct uac3_output_terminal_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bTerminalID;
	__u16 wTerminalType;
	__u8 bAssocTerminal;
	__u8 bSourceID;
	__u8 bCSourceID;
	__u32 bmControls;
	__u16 wExTerminalDescrID;
	__u16 wConnectorsDescrID;
	__u16 wTerminalDescrStr;
} __packed;

#define UAC3_DT_OUTPUT_TERMINAL_SIZE	0x13

extern struct uac3_output_terminal_descriptor badd_baif_out_term_desc;
extern struct uac3_output_terminal_descriptor badd_baof_out_term_desc;

extern __u8 monoControls[];
extern __u8 stereoControls[];
extern __u8 badd_mu_src_ids[];

struct uac3_mixer_unit_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bUnitID;
	__u8 bNrInPins;
	__u8 *baSourceID;
	__u16 wClusterDescrID;
	__u8 bmMixerControls;
	__u32 bmControls;
	__u16 wMixerDescrStr;
} __packed;

#define UAC3_DT_MIXER_UNIT_SIZE		0x10

extern struct uac3_mixer_unit_descriptor badd_baiof_mu_desc;

struct uac3_feature_unit_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bUnitID;
	__u8 bSourceID;
	__u8 *bmaControls;
	__u16 wFeatureDescrStr;
} __packed;

extern struct uac3_feature_unit_descriptor badd_baif_fu_desc;
extern struct uac3_feature_unit_descriptor badd_baof_fu_desc;
extern struct uac3_feature_unit_descriptor badd_baiof_fu_desc;

struct uac3_clock_source_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__u8 bDescriptorSubtype;
	__u8 bClockID;
	__u8 bmAttributes;
	__u32 bmControls;
	__u8 bReferenceTerminal;
	__u16 wClockSourceStr;
} __packed;

#define UAC3_DT_CLOCK_SRC_SIZE		0x0c

extern struct uac3_clock_source_descriptor badd_clock_desc;

extern void *badd_desc_list[];

#endif /* __LINUX_USB_AUDIO_V3_H */

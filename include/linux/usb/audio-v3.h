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

#define NUM_CHANNELS_MONO		1
#define NUM_CHANNELS_STEREO		2
#define BADD_CH_CONFIG_MONO		0
#define BADD_CH_CONFIG_STEREO		3

#define FULL_ADC_PROFILE	0x01

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

#endif /* __LINUX_USB_AUDIO_V3_H */

/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#ifndef __MACH_QDSP6V2_SNDDEV_ECODEC_H
#define __MACH_QDSP6V2_SNDDEV_ECODEC_H
#include <mach/qdsp5v2/audio_def.h>

struct snddev_ecodec_data {
	u32 capability; /* RX or TX */
	const char *name;
	u32 copp_id; /* audpp routing */
	u8 channel_mode;
	u32 conf_pcm_ctl_val;
	u32 conf_aux_codec_intf;
	u32 conf_data_format_padding_val;
};
#endif

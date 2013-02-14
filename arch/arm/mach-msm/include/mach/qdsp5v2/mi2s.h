/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
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
#ifndef _MACH_QDSP5_V2_MI2S_H
#define _MACH_QDSP5_V2_MI2S_H

#define WT_16_BIT 0
#define WT_24_BIT 1
#define WT_32_BIT 2
#define WT_MAX 4

enum mi2s_ret_enum_type {
	MI2S_FALSE = 0,
	MI2S_TRUE
};

#define MI2S_CHAN_MONO_RAW 0
#define MI2S_CHAN_MONO_PACKED 1
#define MI2S_CHAN_STEREO 2
#define MI2S_CHAN_4CHANNELS 3
#define MI2S_CHAN_6CHANNELS 4
#define MI2S_CHAN_8CHANNELS 5
#define MI2S_CHAN_MAX_OUTBOUND_CHANNELS MI2S__CHAN_8CHANNELS

#define MI2S_SD_0    0x01
#define MI2S_SD_1    0x02
#define MI2S_SD_2    0x04
#define MI2S_SD_3    0x08

#define MI2S_SD_LINE_MASK    (MI2S_SD_0 | MI2S_SD_1 | MI2S_SD_2 |  MI2S_SD_3)

bool mi2s_set_hdmi_output_path(uint8_t channels, uint8_t size,
				uint8_t sd_line);

bool mi2s_set_hdmi_input_path(uint8_t channels, uint8_t size, uint8_t sd_line);

bool mi2s_set_codec_output_path(uint8_t channels, uint8_t size);

bool mi2s_set_codec_input_path(uint8_t channels, uint8_t size);

#endif /* #ifndef MI2S_H */

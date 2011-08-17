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
#ifndef __MACH_QDSP5_V2_SNDDEV_MI2S_H
#define __MACH_QDSP5_V2_SNDDEV_MI2S_H

struct snddev_mi2s_data {
	u32 capability; /* RX or TX */
	const char *name;
	u32 copp_id; /* audpp routing */
	u32 acdb_id; /* Audio Cal purpose */
	u8 channel_mode;
	u8 sd_lines;
	void (*route) (void);
	void (*deroute) (void);
	u32 default_sample_rate;
};

int mi2s_config_clk_gpio(void);

int mi2s_config_data_gpio(u32 direction, u8 sd_line_mask);

int mi2s_unconfig_clk_gpio(void);

int mi2s_unconfig_data_gpio(u32 direction, u8 sd_line_mask);

#endif

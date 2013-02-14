/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#ifndef __MACH_QDSP6_V2_SNDDEV_MI2S_H
#define __MACH_QDSP6_V2_SNDDEV_MI2S_H

struct snddev_mi2s_data {
	u32 capability; /* RX or TX */
	const char *name;
	u32 copp_id; /* audpp routing */
	u16 channel_mode;
	u16 sd_lines;
	u32 sample_rate;
};

#define MI2S_SD0 (1 << 0)
#define MI2S_SD1 (1 << 1)
#define MI2S_SD2 (1 << 2)
#define MI2S_SD3 (1 << 3)

#endif

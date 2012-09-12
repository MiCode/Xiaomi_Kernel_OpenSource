/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TI_DRV2667__

#define DRV2667_WAV_SEQ_LEN	11

struct drv2667_pdata {
	const char *name;
	u8 mode;
	/* support one waveform for now */
	u8 wav_seq[DRV2667_WAV_SEQ_LEN];
	u8 gain;
	u8 idle_timeout_ms;
	u32 max_runtime_ms;
};
#endif

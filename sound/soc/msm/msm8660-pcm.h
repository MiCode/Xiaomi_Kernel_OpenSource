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
 */

#ifndef _MSM_PCM_H
#define _MSM_PCM_H

#define USE_CHANNELS_MIN        1
#define USE_CHANNELS_MAX        2
#define NUM_DMAS 9
#define DMASZ	16384
#define MAX_CHANNELS 9

#define MSM_LPA_PHYS   0x28100000
#define MSM_LPA_END    0x2810DFFF


struct msm_audio {
	struct snd_pcm_substream *substream;

	/* data allocated for various buffers */
	char *data;
	dma_addr_t phys;

	unsigned int pcm_size;
	unsigned int pcm_count;
	int enabled;
	int period;
	int dma_ch;
	int period_index;
	int start;
};

extern struct snd_soc_dai msm_cpu_dai[NUM_DMAS];
extern struct snd_soc_platform msm8660_soc_platform;

#endif /*_MSM_PCM_H*/

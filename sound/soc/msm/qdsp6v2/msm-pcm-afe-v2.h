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
#ifndef _MSM_PCM_AFE_H
#define _MSM_PCM_AFE_H
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>


struct pcm_afe_info {
	unsigned long dma_addr;
	struct snd_pcm_substream *substream;
	unsigned int pcm_irq_pos;       /* IRQ position */
	struct mutex lock;
	spinlock_t dsp_lock;
	uint32_t samp_rate;
	uint32_t channel_mode;
	uint8_t start;
	uint32_t dsp_cnt;
	uint32_t buf_phys;
	int32_t mmap_flag;
	int prepared;
	struct hrtimer hrt;
	int poll_time;
};


#define MSM_EXT(xname, fp_info, fp_get, fp_put, addr) \
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER, \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.name = xname, \
	.info = fp_info,\
	.get = fp_get, .put = fp_put, \
	.private_value = addr, \
	}

#endif /*_MSM_PCM_AFE_H*/

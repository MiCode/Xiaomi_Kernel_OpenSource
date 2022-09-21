/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2012,2015-2016 The Linux Foundation. All rights reserved.
 */
#ifndef _MSM_PCM_AFE_H
#define _MSM_PCM_AFE_H
#include <dsp/apr_audio-v2.h>
#include <dsp/q6afe-v2.h>


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
	struct afe_audio_client *audio_client;
	wait_queue_head_t read_wait;
	atomic_t rec_bytes_avail;
	bool reset_event;
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

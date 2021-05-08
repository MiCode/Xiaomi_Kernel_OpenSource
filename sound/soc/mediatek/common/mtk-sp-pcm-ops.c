// SPDX-License-Identifier: GPL-2.0
//
// mtk-sp-pcm-ops.c  --  Mediatek Smart Phone PCM Operation
//
// Copyright (c) 2017 MediaTek Inc.
// Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>

#include <linux/io.h>
#include <linux/module.h>

#include "mtk-sp-pcm-ops.h"
#include "mtk-afe-platform-driver.h"
#include "mtk-base-afe.h"

/* clean buffer that have already been play to HW, to minimize pop when xrun */
#define CLEAR_BUFFER_US 600
int mtk_sp_clean_written_buffer_ack(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int size_per_frame = runtime->frame_bits / 8;
	snd_pcm_uframes_t avail = snd_pcm_playback_avail(runtime);
	unsigned int copy_size = word_size_align(avail * size_per_frame);
	snd_pcm_uframes_t appl_ofs = runtime->control->appl_ptr %
				     runtime->buffer_size;
	unsigned int write_idx = appl_ofs * size_per_frame;
	unsigned int max_clear_buffer_size;

	max_clear_buffer_size = word_size_align(runtime->rate *
						CLEAR_BUFFER_US *
						size_per_frame / 1000000);

	if (copy_size == 0)
		return 0;

	if (copy_size > max_clear_buffer_size)
		copy_size = max_clear_buffer_size;

	if (write_idx + copy_size < runtime->dma_bytes) {
		memset_io(runtime->dma_area + write_idx, 0, copy_size);
	} else {
		unsigned int size_1 = 0, size_2 = 0;

		size_1 = word_size_align(runtime->dma_bytes - write_idx);
		size_2 = word_size_align(copy_size - size_1);

		memset_io(runtime->dma_area + write_idx, 0, size_1);
		memset_io(runtime->dma_area, 0, size_2);
	}

	return 0;
}

MODULE_DESCRIPTION("Mediatek Smart Phone PCM Operation");
MODULE_AUTHOR("Kai Chieh Chuang <kaichieh.chuang@mediatek.com>");
MODULE_LICENSE("GPL v2");


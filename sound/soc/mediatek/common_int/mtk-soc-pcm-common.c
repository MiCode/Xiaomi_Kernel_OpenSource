// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/******************************************************************************
 *
 *
 * Filename:
 * ---------
 *    mtk-soc-pcm-common
 *
 * Project:
 * --------
 *     mtk-soc-pcm-common function
 *
 *
 * Description:
 * ------------
 *   common function
 *
 * Author:
 * -------
 *   Chipeng Chang (MTK02308)
 *
 *---------------------------------------------------------------------------
---
 *

****************************************************************************/

#include "mtk-soc-pcm-common.h"

unsigned long audio_frame_to_bytes(struct snd_pcm_substream *substream,
				   unsigned long count)
{
	unsigned long bytes = count;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_U32_LE)
		bytes = bytes << 2;
	else
		bytes = bytes << 1;

	if (runtime->channels == 2)
		bytes = bytes << 1;
	else if (runtime->channels == 4)
		bytes = bytes << 2;
	else if (runtime->channels != 1)
		bytes = bytes << 3;

	return bytes;
}
EXPORT_SYMBOL(audio_frame_to_bytes);

unsigned long audio_bytes_to_frame(struct snd_pcm_substream *substream,
				   unsigned long bytes)
{
	unsigned long count = bytes;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    runtime->format == SNDRV_PCM_FORMAT_U32_LE)
		count = count >> 2;
	else
		count = count >> 1;

	if (runtime->channels == 2)
		count = count >> 1;
	else if (runtime->channels == 4)
		count = count >> 2;
	else if (runtime->channels != 1)
		count = count >> 3;

	return count;
}
EXPORT_SYMBOL(audio_bytes_to_frame);
MODULE_LICENSE("GPL v2");

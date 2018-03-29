/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/******************************************************************************
*
 *
 * Filename:
 * ---------
 *    mt_soc_pcm_common
 *
 * Project:
 * --------
 *     mt_soc_pcm_common function
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

*******************************************************************************/

#include "mt_soc_pcm_common.h"

unsigned long audio_frame_to_bytes(struct snd_pcm_substream *substream, unsigned long count)
{
	unsigned long bytes = count;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
		bytes = bytes << 2;
	else
		bytes = bytes << 1;

	if (runtime->channels == 2)
		bytes = bytes << 1;
	/* pr_debug("%s bytes = %d count = %d\n",__func__,bytes,count); */
	return bytes;
}


unsigned long audio_bytes_to_frame(struct snd_pcm_substream *substream, unsigned long bytes)
{
	unsigned long count  = bytes;
	struct snd_pcm_runtime *runtime = substream->runtime;

	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE || runtime->format == SNDRV_PCM_FORMAT_U32_LE)
		count = count >> 2;
	else
		count = count >> 1;


	if (runtime->channels == 2)
		count = count >> 1;
	/* pr_debug("%s bytes = %d count = %d\n",__func__,bytes,count); */
	return count;
}

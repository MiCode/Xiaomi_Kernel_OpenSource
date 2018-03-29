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

/* Conventional and unconventional sample rate supported */
const unsigned int soc_fm_supported_sample_rates[3] = {
	32000, 44100, 48000
};

const unsigned int soc_voice_supported_sample_rates[3] = {
	8000, 16000, 32000
};

/* Conventional and unconventional sample rate supported */
const unsigned int soc_normal_supported_sample_rates[9] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

/* Conventional and unconventional sample rate supported */
const unsigned int soc_high_supported_sample_rates[13] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000
};

unsigned long audio_frame_to_bytes(struct snd_pcm_substream *substream, unsigned long count)
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

	/* printk("%s bytes = %d count = %d\n",__func__,bytes,count); */
	return bytes;
}


unsigned long audio_bytes_to_frame(struct snd_pcm_substream *substream, unsigned long bytes)
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

	/* printk("%s bytes = %d count = %d\n",__func__,bytes,count); */
	return count;
}

unsigned long mtk_local_audio_copy_from_user(bool IsSRAM, kal_uint8 *dst, char *src, int len)
{
	unsigned long value;

	if (IsSRAM) {
		/* PRINTK_AUDDRV("mtk_local_audio_copy_from_user SRAM = %d\n", len); */
#ifdef MEMCPY_SINGLE_MODE
		int loopcnt = (len >> 2);
		int remain = len - (loopcnt<<2);
		int i;

		for (i = 0; i < loopcnt; i++) {
			if (copy_from_user(dst+i*4, src+i*4, 4)) {
				pr_err("%s 1, len=%d", __func__, len);
				return 1;
			}
		}
		if (remain) {
			if (copy_from_user(dst+loopcnt*4, src+loopcnt*4, remain)) {
				pr_err("%s 2, len=%d", __func__, len);
				return 1;
			}
		}
#else	/* use burst mode */
		if (copy_from_user(dst, src, len)) {
			pr_err("%s 3, len=%d", __func__, len);
			return 1;
		}
#endif
	} else {
		/* PRINTK_AUDDRV("mtk_local_audio_copy_from_user DRAM = %d\n", len); */
		value = copy_from_user(dst, src, len);
		if (value) {
			pr_err("%s 4, len=%d, length=%ld", __func__, len, value);
			return 1;
		}
	}
	return 0;
}

unsigned long mtk_local_audio_copy_to_user(bool IsSRAM, kal_uint8 *dst, char *src, int len)
{
	if (IsSRAM) {
		/* PRINTK_AUDDRV("mtk_local_audio_copy_to_user SRAM = %d\n", len); */
#ifdef MEMCPY_SINGLE_MODE
		int loopcnt = (len >> 2);
		int remain = len - (loopcnt<<2);
		int i;

		for (i = 0; i < loopcnt; i++) {
			if (copy_to_user(dst+i*4, src+i*4, 4)) {
				pr_err("%s 1, len=%d", __func__, len);
				return 1;
			}
		}
		if (remain) {
			if (copy_to_user(dst+loopcnt*4, src+loopcnt*4, remain)) {
				pr_err("%s 2, len=%d", __func__, len);
				return 1;
			}
		}
#else	/* use burst mode */
		if (copy_to_user(dst, src, len)) {
			pr_err("%s 3, len=%d", __func__, len);
			return 1;
		}
#endif
	} else {
		/* PRINTK_AUDDRV("mtk_local_audio_copy_to_user DRAM = %d\n", len); */
		if (copy_to_user(dst, src, len)) {
			pr_err("%s 4, len=%d", __func__, len);
			return 1;
		}
	}
	return 0;
}


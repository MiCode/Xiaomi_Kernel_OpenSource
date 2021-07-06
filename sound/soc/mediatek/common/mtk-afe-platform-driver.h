/*
 * mtk-afe-platform-driver.h  --  Mediatek afe platform driver definition
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Garlic Tseng <garlic.tseng@mediatek.com>
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

#ifndef _MTK_AFE_PLATFORM_DRIVER_H_
#define _MTK_AFE_PLATFORM_DRIVER_H_

extern const struct snd_pcm_ops mtk_afe_pcm_ops;
extern const struct snd_soc_platform_driver mtk_afe_pcm_platform;

struct mtk_base_afe;
struct snd_pcm;
struct snd_soc_pcm_runtime;
struct snd_soc_platform;


int mtk_afe_pcm_new(struct snd_soc_pcm_runtime *rtd);
void mtk_afe_pcm_free(struct snd_pcm *pcm);

int mtk_afe_combine_sub_dai(struct mtk_base_afe *afe);
int mtk_afe_add_sub_dai_control(struct snd_soc_platform *platform);

unsigned int word_size_align(unsigned int in_size);
#endif


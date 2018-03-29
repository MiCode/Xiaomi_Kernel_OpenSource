/*
 * mtk-afe-contols.h  --  Mediatek Platform driver ALSA contorls
 *
 * Copyright (c) 2016 MediaTek Inc.
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

#ifndef _MTK_AFE_CONTROLS_H_
#define _MTK_AFE_CONTROLS_H_

struct snd_soc_platform;

int mtk_afe_add_controls(struct snd_soc_platform *platform);

#endif

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef _MTK_SP_PLATFORM_DRIVER_H_
#define _MTK_SP_PLATFORM_DRIVER_H_

#include <sound/soc.h>

int mtk_sp_clean_written_buffer_ack(struct snd_pcm_substream *substream);
#endif


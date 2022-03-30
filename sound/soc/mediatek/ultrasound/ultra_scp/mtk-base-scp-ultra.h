/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-base-dsp.h --  Mediatek ADSP dsp base
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */


#ifndef _MTK_SCP_ULTRA_BASE_H_
#define _MTK_SCP_ULTRA_BASE_H_

#include <sound/soc.h>


struct device;
struct snd_pcm_substream;
struct snd_dma_buffer;

#define MAX_PAYLOAD_SIZE (32)

struct ultra_param_config {
	unsigned int rate_in;
	unsigned int rate_out;
	unsigned int channel_in;
	unsigned int channel_out;
	unsigned int format_in;
	unsigned int format_out;
	unsigned int period_in_size;
	unsigned int period_out_size;
	unsigned int target_out_channel;
};

struct ultra_gain_config {
	int mic_gain;
	int receiver_gain;
};


#endif


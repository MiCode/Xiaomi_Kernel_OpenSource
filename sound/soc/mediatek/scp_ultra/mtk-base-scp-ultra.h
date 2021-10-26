/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-base-dsp.h --  Mediatek ADSP dsp base
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */


#ifndef _MTK_SCP_ULTRA_BASE_H_
#define _MTK_SCP_ULTRA_BASE_H_

#include "audio_buf.h"
#include <audio_messenger_ipi.h>
#include "audio_task.h"
#include <sound/soc.h>


struct device;
struct snd_pcm_substream;
struct snd_dma_buffer;

#define MAX_PAYLOAD_SIZE (32)

struct scp_ultra_dump_ops {
	void (*ultra_dump_callback)(struct ipi_msg_t *ipi_msg);
};

struct mtk_base_scp_ultra_dump {
	bool dump_flag;
	struct audio_dsp_dram dump_resv_mem;
	struct scp_ultra_dump_ops *dump_ops;
};

struct mtk_base_scp_ultra_mem {
	struct snd_dma_buffer ultra_dl_dma_buf;
	struct snd_dma_buffer ultra_ul_dma_buf;
	int ultra_ul_memif_id;
	int ultra_dl_memif_id;
};

struct mtk_base_scp_ultra {
	struct device *dev;
	const struct snd_pcm_hardware *mtk_scp_hardware;
	const struct snd_soc_component_driver *component_driver;
	struct mtk_base_scp_ultra_mem ultra_mem;
	struct mtk_base_scp_ultra_dump ultra_dump;
	struct audio_dsp_dram ultra_reserve_dram;
	unsigned int usnd_state;
};

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


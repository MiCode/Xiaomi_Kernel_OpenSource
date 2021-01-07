/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-base-dsp.h --  Mediatek ADSP dsp base
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer ultra_ul_dma_buf;
	struct snd_dma_buffer ultra_dl_dma_buf;
	struct RingBuf platform_ringbuf;
	int ultra_ul_memif_id;
	int ultra_dl_memif_id;
	uint32_t ipi_payload_buf[MAX_PAYLOAD_SIZE];
};

struct mtk_base_scp_ultra {
	struct device *dev;
	const struct snd_pcm_hardware *mtk_dsp_hardware;
	unsigned int num_dai_drivers;
	const struct snd_soc_component_driver *component_driver;
	struct mtk_base_scp_ultra_mem ultra_mem;
	struct mtk_base_scp_ultra_dump ultra_dump;
	struct RingBuf ring_buf_test;
};

#endif


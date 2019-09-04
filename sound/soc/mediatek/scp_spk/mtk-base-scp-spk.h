/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-base-dsp.h --  Mediatek ADSP dsp base
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */


#ifndef _MTK_SCP_SPK_BASE_H_
#define _MTK_SCP_SPK_BASE_H_

#include "audio_buf.h"
#include <audio_messenger_ipi.h>
#include "audio_task.h"
#include <sound/soc.h>


struct device;
struct snd_pcm_substream;
struct snd_dma_buffer;

#define MAX_PAYLOAD_SIZE (32)

struct scp_spk_dump_ops {
	void (*spk_dump_callback)(struct ipi_msg_t *ipi_msg);
};

struct mtk_base_scp_spk_dump {
	bool dump_flag;
	struct audio_dsp_dram dump_resv_mem;
	struct scp_spk_dump_ops *dump_ops;
};

struct mtk_base_scp_spk_mem {
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer platform_dma_buf;
	struct snd_dma_buffer spk_iv_dma_buf;
	struct snd_dma_buffer spk_dl_dma_buf;
	struct snd_dma_buffer spk_md_ul_dma_buf;
	struct RingBuf platform_ringbuf;
	int spk_iv_memif_id;
	int spk_dl_memif_id;
	int spk_md_ul_memif_id;
	uint32_t ipi_payload_buf[MAX_PAYLOAD_SIZE];
	bool is_iv_buf_in_tcm;
};

struct mtk_base_scp_spk {
	struct device *dev;
	const struct snd_pcm_hardware *mtk_dsp_hardware;
	unsigned int num_dai_drivers;
	const struct snd_soc_component_driver *component_driver;
	struct mtk_base_scp_spk_mem spk_mem;
	struct mtk_base_scp_spk_dump spk_dump;
	struct RingBuf ring_buf_test;
};

#endif


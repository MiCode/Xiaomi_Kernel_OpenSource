/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _MTK_SCP_SPK_COMMON_H
#define _MTK_SCP_SPK_COMMON_H

#include <linux/kernel.h>

#ifdef scp_spk_debug
#undef scp_spk_debug
#endif
#if 0 /* debug only. might make performace degrade */
#define scp_spk_debug(x...) pr_info(x)
#else
#define scp_spk_debug(x...)
#endif

struct mtk_base_afe;
struct mtk_base_scp_spk;
struct snd_dma_buffer;
struct snd_pcm_substream;
struct mtk_base_scp_spk_dump;

enum {
	SCP_SPK_STAGE_OFF,
	SCP_SPK_STAGE_NORMAL_PLAYBACK,
	SCP_SPK_STAGE_VOICE_PLAYBACK,
};

enum {
	SCP_SPK_DL_DAI_ID = 0,
	SCP_SPK_IV_DAI_ID,
	SCP_SPK_MDUL_DAI_ID,
	SCP_SPK_DAI_NUM,
};

int audio_set_dsp_afe(struct mtk_base_afe *afe);
struct mtk_base_afe *get_afe_base(void);
int set_scp_spk_base(struct mtk_base_scp_spk *scp_spk);
void *get_scp_spk_base(void);
void *get_ipi_recv_private(void);
void set_ipi_recv_private(void *priv);
void mtk_scp_spk_dump_msg(struct mtk_base_scp_spk_dump *spk_dump);
void mtk_scp_spk_ipi_send(uint8_t  data_type,/* audio_ipi_msg_data_t */
			  uint8_t  ack_type,/* audio_ipi_msg_ack_t */
			  uint16_t msg_id,
			  uint32_t param1, uint32_t param2,
			  char *payload);
unsigned int mtk_scp_spk_pack_payload(uint16_t msg_id, uint32_t param1,
				      uint32_t param2,
				      struct snd_dma_buffer *dma_buffer,
				      struct snd_pcm_substream *substream);
void set_afe_irq_target(int irq_usage, int scp_enable);
#endif


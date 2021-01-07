/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MTK_SCP_ULTRA_COMMON_H
#define _MTK_SCP_ULTRA_COMMON_H

#include <linux/kernel.h>

#ifdef scp_ultra_debug
#undef scp_ultra_debug
#endif
#if 0 /* debug only. might make performace degrade */
#define scp_ultra_debug(x...) pr_info(x)
#else
#define scp_ultra_debug(x...)
#endif

struct mtk_base_afe;
struct mtk_base_scp_ultra;
struct snd_dma_buffer;
struct snd_pcm_substream;
struct mtk_base_scp_ultra_dump;

enum {
	SCP_ULTRA_STAGE_OFF,
	SCP_ULTRA_STAGE_NORMAL_PLAYBACK,
	SCP_ULTRA_STAGE_VOICE_PLAYBACK,
};

enum {
	SCP_ULTRA_DL_DAI_ID = 0,
	SCP_ULTRA_UL_DAI_ID,
	SCP_ULTRA_DAI_NUM,
};

int audio_set_dsp_afe(struct mtk_base_afe *afe);
struct mtk_base_afe *get_afe_base(void);
int set_scp_ultra_base(struct mtk_base_scp_ultra *scp_ultra);
void *get_scp_ultra_base(void);
void *get_ipi_recv_private(void);
void set_ipi_recv_private(void *priv);
void mtk_scp_ultra_dump_msg(struct mtk_base_scp_ultra_dump *ultra_dump);
void mtk_scp_ultra_ipi_send(uint8_t  data_type,/* audio_ipi_msg_data_t */
			  uint8_t  ack_type,/* audio_ipi_msg_ack_t */
			  uint16_t msg_id,
			  uint32_t param1, uint32_t param2,
			  char *payload);
unsigned int mtk_scp_ultra_pack_payload(uint16_t msg_id, uint32_t param1,
				      uint32_t param2,
				      struct snd_dma_buffer *dma_buffer,
				      struct snd_pcm_substream *substream);
void set_afe_irq_target(int irq_usage, int scp_enable);
void set_afe_ul_irq_target(int irq_usage, int scp_enable);
#endif


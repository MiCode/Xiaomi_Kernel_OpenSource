/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-audio-pcm.h --  Mediatek scp audio pcm
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Zhixiong <Zhixiong.Wang@mediatek.com>
 */

#ifndef _MTK_SCP_AUDIO_PCM_H_
#define _MTK_SCP_AUDIO_PCM_H_

#include <audio_messenger_ipi.h>
#include "audio_task.h"
#include "mtk-scp-audio-base.h"

struct device;
struct snd_pcm_substream;
struct snd_soc_dai;
struct snd_soc_dai_driver;
struct mtk_scp_audio_base;
struct gen_pool;

struct mtk_base_afe;
struct ipi_msg_t;
#define AFE_SCP_AUDIO_NAME "SCP_AUDIO_PCM"

extern const struct snd_soc_component_driver mtk_scp_audio_pcm_platform;
extern int scp_audio_dai_register(struct platform_device *pdev,
				  struct mtk_scp_audio_base *scp_audio);
extern void init_scp_spk_process_enable(int enable_flag);

/* function warp playback buffer information send to scp */
int afe_pcm_ipi_to_scp(int command, struct snd_pcm_substream *substream,
		       struct snd_pcm_hw_params *params,
		       struct snd_soc_dai *dai,
		       struct mtk_base_afe *afe);

int scp_set_audio_afe(struct mtk_base_afe *afe);
struct mtk_base_afe *scp_get_audio_afe(void);
struct scp_aud_task_base *get_taskbase_by_daiid(const int daiid);
void scp_audio_pcm_ipi_recv(struct ipi_msg_t *ipi_msg);
void scp_aud_ipi_handler(struct mtk_scp_audio_base *scp_aud,
		     struct ipi_msg_t *ipi_msg);
int get_scene_by_daiid(int id);
int mtk_get_ipi_buf_scene_rv(void);
int send_task_sharemem_to_scp(struct mtk_scp_audio_base *scp_audio, int daiid);
int scp_audio_pcm_recover_event(struct notifier_block *this,
				  unsigned long event,
				  void *ptr);
#endif

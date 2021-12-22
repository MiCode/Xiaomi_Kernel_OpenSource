/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
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

#if defined(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>

#define AUDIO_AEE(message) \
	(aee_kernel_exception_api(__FILE__, \
				  __LINE__, \
				  DB_OPT_FTRACE, message, \
				  "audio assert"))
#else
#define AUDIO_AEE(message) WARN_ON(true)
#endif

/* wake lock relate*/
#define aud_wake_lock_init(dev, name) wakeup_source_register(dev, name)
#define aud_wake_lock_destroy(ws) wakeup_source_destroy(ws)
#define aud_wake_lock(ws) __pm_stay_awake(ws)
#define aud_wake_unlock(ws) __pm_relax(ws)

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

enum {
	ULTRASOUND_TARGET_OUT_CHANNEL_LEFT,
	ULTRASOUND_TARGET_OUT_CHANNEL_RIGHT,
	ULTRASOUND_TARGET_OUT_CHANNEL_BOTH
};

enum {
	HAL_FORMAT_INALID = -1,
	HAL_FORMAT_S16_LE = 0,
	HAL_FORMAT_S32_LE,
	HAL_FORMAT_S8,
	HAL_FORMAT_S24_LE,
	HAL_FORMAT_S32_3LE,
};

enum {
	SCP_ULTRA_STATE_IDLE = -1,
	SCP_ULTRA_STATE_ON,
	SCP_ULTRA_STATE_START,
	SCP_ULTRA_STATE_STOP,
	SCP_ULTRA_STATE_OFF,
	SCP_ULTRA_STATE_RECOVERY,
};

#define DEFAULT_UL_PERIOD_SIZE (480)
#define DEFAULT_DL_PERIOD_SIZE (1024)



struct mtk_base_afe;
struct mtk_base_scp_ultra;
struct snd_dma_buffer;
struct snd_pcm_substream;
struct mtk_base_scp_ultra_dump;

int ultra_set_afe_base(struct mtk_base_afe *afe);
struct mtk_base_afe *ultra_get_afe_base(void);
int set_scp_ultra_base(struct mtk_base_scp_ultra *scp_ultra);
void *get_scp_ultra_base(void);
void mtk_scp_ultra_dump_msg(struct mtk_base_scp_ultra_dump *ultra_dump);
void mtk_scp_ultra_ipi_send(uint8_t data_type, /*audio_ipi_msg_data_t*/
			    uint8_t ack_type, /*audio_ipi_msg_ack_t*/
			    uint16_t msg_id,
			    uint32_t param1,
			    uint32_t param2,
			    char *payload);
void set_afe_dl_irq_target(int scp_enable);
void set_afe_ul_irq_target(int scp_enable);
#endif


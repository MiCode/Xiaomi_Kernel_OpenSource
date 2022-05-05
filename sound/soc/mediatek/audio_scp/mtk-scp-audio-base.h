/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-audio-base.h --  Mediatek scp audio base
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Zhixiong <Zhixiong.Wang@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/genalloc.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "scp_audio_buf.h"
#include <audio_playback_msg_id.h>
#include <audio_ipi_platform.h>
#include <audio_messenger_ipi.h>
#include <../common/mtk-base-afe.h>

#ifndef _MTK_SCP_AUDIO_BASE_H_
#define _MTK_SCP_AUDIO_BASE_H_

#define SND_SCP_AUDIO_DTS_SIZE (4)
#define MAX_PAYLOAD_SIZE (32) /* 32bytes */
#define A2D_SHAREMEM_SIZE (128)
#define D2A_SHAREMEM_SIZE (128)

/*page size*/
#define MIN_SCP_AUD_SHIFT (8)
#define MIN_SCP_AUD_POOL_SIZE (1 << MIN_SCP_AUD_SHIFT)

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

#define SCP_AUD_ASSERT(exp) \
do { \
	if (!(exp)) { \
		pr_notice("ASSERT("#exp") fail: \""  __FILE__ "\", %uL\n", \
		__LINE__); \
		WARN_ON(1); \
	} \
} while (0)

enum {
	SCP_AUD_TASK_SPK_PROCESS_ID = 0,
	SCP_AUD_TASK_DAI_NUM,
};

enum{
	SCP_BUFFER_SHARE_MEM,
	SCP_BUFFER_HW_MEM
};

enum {
	MEMORY_SRAM,
	MEMORY_DRAM,
	MEMORY_SYS_TCM,
};

/* task command param1 */
enum {
	SCP_AUDIO_TASK_PCM_HWPARAM_DL = 0x1,
	SCP_AUDIO_TASK_PCM_HWPARAM_UL = 0x2,
	SCP_AUDIO_TASK_PCM_HWPARAM_REF = 0x4,
};

/* dl consume param2 status */
enum {
	SCP_DL_CONSUME_OK,
	SCP_DL_CONSUME_RESET,
	SCP_DL_CONSUME_UNDERFLOW,
	SCP_UL_READ_RESET,
};

struct mtk_scp_audio_base;

struct scp_aud_ipi_ops {
	void (*ipi_message_callback)(struct ipi_msg_t *ipi_msg);
	void (*ipi_handler)(struct mtk_scp_audio_base *scp_aud,
			    struct ipi_msg_t *ipi_msg);
};

struct mbox_msg {
	int scene_id;
	int msg_id;
	unsigned long long pRead;
};

struct scp_audio_reserve_mem {
	unsigned long long phy_addr;
	unsigned long long va_addr;
	unsigned long long size;
	unsigned char *vir_addr;
};

struct scp_audio_task_attr {
	unsigned int default_enable; /* default setting */
	int dl_memif;
	int ul_memif;
	int ref_memif;
	int feature_id;
	int runtime_enable;
	int ref_runtime_enable;
};

struct scp_aud_task_base {
	struct audio_hw_buffer share_hw_buf;    /* dsp <-> audio data struct */
	struct audio_hw_buffer temp_work_buf; /* working buffer */
	struct audio_hw_buffer afe_hw_buf; /* dsp <-> audio data struct */
	struct RingBuf ring_buf;
	struct snd_pcm_substream *substream;
	struct audio_dsp_dram msg_atod_share_buf;
	struct audio_dsp_dram msg_dtoa_share_buf;
	struct audio_dsp_dram ring_share_buf;
	unsigned char ipi_payload_buf[MAX_PAYLOAD_SIZE];
	//unsigned int dsp_feature_counter;
	int underflowed;
	spinlock_t ringbuf_lock;
};

struct mtk_scp_audio_base {
	struct device *dev;
	const struct snd_pcm_hardware *scp_audio_hardware;
	struct snd_soc_dai_driver *dai_drivers;
	unsigned int num_dai_drivers;
	struct scp_audio_reserve_mem rsv_mem;
	struct scp_aud_task_base task_base[SCP_AUD_TASK_DAI_NUM];
	struct scp_aud_ipi_ops ipi_ops;
	struct gen_pool *genpool;

	int (*runtime_suspend)(struct device *dev);
	int (*runtime_resume)(struct device *dev);

	int (*request_dram_resource)(struct device *dev);
	int (*release_dram_resource)(struct device *dev);
	struct tasklet_struct tasklet;

	bool suspended;
	int dram_resource_counter;
	int dl_memif;
	int ul_memif;
	int ref_memif;
};

void *get_scp_audio_base(void);
#endif

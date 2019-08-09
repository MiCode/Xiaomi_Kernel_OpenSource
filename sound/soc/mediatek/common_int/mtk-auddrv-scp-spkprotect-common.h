/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/****************************************************************************
 *
 *
 * Filename:
 * ---------
 *   mtk-auddrv_scp_spkprotect_common.h
 *
 * Project:
 * --------
 *   None
 *
 * Description:
 * ------------
 *   Audio Spk Protect Kernel Definitions
 *
 * Author:
 * -------
 *   Chipeng Chang
 *
 *---------------------------------------------------------------------------
 *
 ****************************************************************************
 */

#ifndef AUDIO_SPKPROCT_COMMON_H
#define AUDIO_SPKPROCT_COMMON_H

#include "audio_spkprotect_msg_id.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include <linux/kernel.h>

#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
#include <audio_ipi_client_spkprotect.h>
#include <audio_task_manager.h>
#endif

struct spk_dump_ops {
	void (*spk_dump_callback)(struct ipi_msg_t *ipi_msg);
};

struct aud_spk_message {
	uint16_t msg_id;
	uint32_t param1;
	uint32_t param2;
	char *payload;
};

struct scp_spk_reserved_mem_t {
	dma_addr_t phy_addr;
	char *vir_addr;
	uint32_t size;
};

void init_scp_spk_reserved_dram(void);
struct scp_spk_reserved_mem_t *get_scp_spk_reserved_mem(void);
struct scp_spk_reserved_mem_t *get_scp_spk_dump_reserved_mem(void);
void spkproc_service_set_spk_dump_message(struct spk_dump_ops *ops);
void spkproc_service_ipicmd_received(struct ipi_msg_t *ipi_msg);
void spkproc_service_ipicmd_send(
				 uint8_t  data_type,/* audio_ipi_msg_data_t */
				 uint8_t  ack_type,/* audio_ipi_msg_ack_t */
				 uint16_t msg_id,
				 uint32_t param1, uint32_t param2,
				 char *payload);
unsigned int spkproc_ipi_pack_payload(uint16_t msg_id, uint32_t param1,
				      uint32_t param2,
				      struct snd_dma_buffer *bmd_buffer,
				      struct snd_pcm_substream *substream);
uint32_t *spkproc_ipi_get_payload(void);
extern void scp_reset_check(void);
extern atomic_t stop_send_ipi_flag;
extern atomic_t scp_reset_done;
extern bool scp_smartpa_used_flag;
#endif

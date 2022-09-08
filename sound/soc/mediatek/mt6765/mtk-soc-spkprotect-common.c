// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/****************************************************************************
 *
 * Filename:
 * ---------
 *   mt_soc_spkprotect_common.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *---------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

#include "mtk-auddrv-scp-spkprotect-common.h"
#include <linux/compat.h>
#include <scp_helper.h>
#include <scp_ipi.h>
#include <audio_ipi_platform.h>

#if IS_ENABLED(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT)
#include "audio_ipi_client_spkprotect.h"
#include <audio_ipi_client_spkprotect.h>
#include <audio_task_manager.h>
#endif

#define MAX_PAYLOAD_SIZE (32)
uint32_t ipi_payload_buf[MAX_PAYLOAD_SIZE];
static struct aud_spk_message mAud_Spk_Message;
static struct scp_spk_reserved_mem_t scp_spk_reserved_mem;
static struct scp_spk_reserved_mem_t scp_spk_dump_reserved_mem;
static struct spk_dump_ops *mspk_dump_op;

void init_scp_spk_reserved_dram(void)
{
	scp_spk_reserved_mem.phy_addr =
		scp_get_reserve_mem_phys(SPK_PROTECT_MEM_ID);
	scp_spk_reserved_mem.vir_addr =
		(char *)scp_get_reserve_mem_virt(SPK_PROTECT_MEM_ID);
	scp_spk_reserved_mem.size =
		scp_get_reserve_mem_size(SPK_PROTECT_MEM_ID);
	memset_io((void *)scp_spk_reserved_mem.vir_addr, 0,
		  scp_spk_reserved_mem.size);

	scp_spk_dump_reserved_mem.phy_addr =
		scp_get_reserve_mem_phys(SPK_PROTECT_DUMP_MEM_ID);
	scp_spk_dump_reserved_mem.vir_addr =
		(char *)scp_get_reserve_mem_virt(SPK_PROTECT_DUMP_MEM_ID);
	scp_spk_dump_reserved_mem.size =
		scp_get_reserve_mem_size(SPK_PROTECT_DUMP_MEM_ID);
	memset_io((void *)scp_spk_dump_reserved_mem.vir_addr, 0,
		  scp_spk_dump_reserved_mem.size);

	pr_info("reserved dram: pa %p, va %p, size 0x%x, reserved dump dram: pa %p, va %p, size 0x%x\n",
		scp_spk_reserved_mem.phy_addr,
		scp_spk_reserved_mem.vir_addr,
		scp_spk_reserved_mem.size,
		scp_spk_dump_reserved_mem.phy_addr,
		scp_spk_dump_reserved_mem.vir_addr,
		scp_spk_dump_reserved_mem.size);

	AUDIO_ASSERT(scp_spk_reserved_mem.phy_addr <= 0);
	AUDIO_ASSERT(scp_spk_reserved_mem.vir_addr == NULL);
	AUDIO_ASSERT(scp_spk_reserved_mem.size <= 0);
	AUDIO_ASSERT(scp_spk_dump_reserved_mem.phy_addr <= 0);
	AUDIO_ASSERT(scp_spk_dump_reserved_mem.vir_addr == NULL);
	AUDIO_ASSERT(scp_spk_dump_reserved_mem.size <= 0);
}

struct scp_spk_reserved_mem_t *get_scp_spk_reserved_mem(void)
{
	return &scp_spk_reserved_mem;
}

struct scp_spk_reserved_mem_t *get_scp_spk_dump_reserved_mem(void)
{
	return &scp_spk_dump_reserved_mem;
}

void spkproc_service_set_spk_dump_message(struct spk_dump_ops *ops)
{
	pr_debug("%s\n ", __func__);
	if (ops != NULL) {
		mspk_dump_op = ops;
		mspk_dump_op->spk_dump_callback(NULL);
	}
}

void spkproc_service_ipicmd_received(struct ipi_msg_t *ipi_msg)
{
	switch (ipi_msg->msg_id) {
	case SPK_PROTECT_IRQDL:
		mAud_Spk_Message.msg_id = ipi_msg->msg_id;
		mAud_Spk_Message.param1 = ipi_msg->param1;
		mAud_Spk_Message.param2 = ipi_msg->param2;
		mAud_Spk_Message.payload = ipi_msg->payload;
		AudDrv_DSP_IRQ_handler((void *)&mAud_Spk_Message);
		break;
	case SPK_PROTECT_PCMDUMP_OK:
		if (mspk_dump_op->spk_dump_callback != NULL)
			mspk_dump_op->spk_dump_callback(ipi_msg);
		break;
	default:
		break;
	}
}

void spkproc_service_ipicmd_send(uint8_t data_type, uint8_t ack_type,
				 uint16_t msg_id, uint32_t param1,
				 uint32_t param2, char *payload)
{
	struct ipi_msg_t ipi_msg;
	int send_result = 0;


	if (atomic_read(&stop_send_ipi_flag)) {
		pr_err("%s(), scp reset...\n", __func__);
		return;
	}

	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));
	send_result = audio_send_ipi_msg(&ipi_msg,
					  TASK_SCENE_SPEAKER_PROTECTION,
					  AUDIO_IPI_LAYER_TO_DSP,
					  data_type,
					  ack_type,
					  msg_id,
					  param1,
					  param2,
					  (char *)payload);

	if (send_result != 0)
		pr_err("%s(), scp_ipi send fail\n", __func__);
}

uint32_t *spkproc_ipi_get_payload(void)
{
	return ipi_payload_buf;
}

unsigned int spkproc_ipi_pack_payload(uint16_t msg_id, uint32_t param1,
				      uint32_t param2,
				      struct snd_dma_buffer *bmd_buffer,
				      struct snd_pcm_substream *substream)
{
	unsigned int ret = 0;
	/* clean payload data */
	memset_io((void *)ipi_payload_buf, 0,
		  sizeof(uint32_t) * MAX_PAYLOAD_SIZE);
	switch (msg_id) {
	case SPK_PROTECT_PLATMEMPARAM:
		ipi_payload_buf[0] = (uint32_t)(bmd_buffer->addr);
		ipi_payload_buf[1] = (uint32_t)(*bmd_buffer->area);
		ipi_payload_buf[2] = bmd_buffer->bytes;
		ipi_payload_buf[3] = true;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_DLMEMPARAM:
		ipi_payload_buf[0] = (uint32_t)bmd_buffer->addr;
		ipi_payload_buf[1] = (uint32_t)(*bmd_buffer->area);
		ipi_payload_buf[2] = bmd_buffer->bytes;
		ipi_payload_buf[3] = param1;
		ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_PREPARE:
		ipi_payload_buf[0] = (uint32_t)(substream->runtime->format);
		ipi_payload_buf[1] = (uint32_t)(substream->runtime->rate);
		ipi_payload_buf[2] = (uint32_t)(substream->runtime->channels);
		ipi_payload_buf[3] =
			(uint32_t)(substream->runtime->period_size);
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_IVMEMPARAM:
		ipi_payload_buf[0] = (uint32_t)bmd_buffer->addr;
		ipi_payload_buf[1] = (uint32_t)(*bmd_buffer->area);
		ipi_payload_buf[2] = bmd_buffer->bytes;
		ipi_payload_buf[3] = param1;
		ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_DLCOPY:
		ipi_payload_buf[0] = (uint32_t)param1;
		ipi_payload_buf[1] = (uint32_t)param2;
		ret = sizeof(unsigned int) * 2;
		break;
	case SPK_PROTECT_SPEECH_MDFEEDBACKPARAM:
		ipi_payload_buf[0] = (uint32_t)(bmd_buffer->addr);
		ipi_payload_buf[1] = (uint32_t)(*bmd_buffer->area);
		ipi_payload_buf[2] = bmd_buffer->bytes;
		ipi_payload_buf[3] = param1;
		ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_SPEECH_DLMEMPARAM:
		ipi_payload_buf[0] = (uint32_t)bmd_buffer->addr;
		ipi_payload_buf[1] = (uint32_t)(*bmd_buffer->area);
		ipi_payload_buf[2] = bmd_buffer->bytes;
		ipi_payload_buf[3] = param1;
		ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_SPEECH_PREPARE:
		ipi_payload_buf[0] =
				(uint32_t)substream->runtime->format;
		ipi_payload_buf[1] =
			(uint32_t)substream->runtime->rate;
		ipi_payload_buf[2] =
			(uint32_t)substream->runtime->channels;
		ipi_payload_buf[3] =
			(uint32_t)substream->runtime->period_size;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_SPEECH_IVMEMPARAM:
		ipi_payload_buf[0] = (uint32_t)bmd_buffer->addr;
		ipi_payload_buf[1] = (uint32_t)(*bmd_buffer->area);
		ipi_payload_buf[2] = bmd_buffer->bytes;
		ipi_payload_buf[3] = param1;
		ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTTCT_PCMDUMP_ON:
		ipi_payload_buf[0] = param1;
		ipi_payload_buf[1] = param2;
		ret = sizeof(unsigned int) * 2;
		break;
	default:
		pr_debug("%s msg_id not support\n", __func__);
		break;
	}

	return ret;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek SpkPotect");

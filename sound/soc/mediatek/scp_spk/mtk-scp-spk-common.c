// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

#include "mtk-scp-spk-common.h"
#include "mtk-base-scp-spk.h"
#include <sound/soc.h>
#include <linux/device.h>
#include <linux/compat.h>
#include "scp_helper.h"
#include "scp_ipi.h"
#include "audio_ipi_platform.h"
#include "audio_spkprotect_msg_id.h"
#include "audio_ipi_client_spkprotect.h"
#include "audio_task_manager.h"
#include "mtk-base-afe.h"
#include "mtk-scp-spk-platform-mem-control.h"


/* don't use this directly if not necessary */
static struct mtk_base_scp_spk *local_base_scp_spk;
static struct mtk_base_afe *local_scp_spk_afe;

int audio_set_dsp_afe(struct mtk_base_afe *afe)
{
	if (!afe) {
		pr_err("%s(), afe is NULL", __func__);
		return -1;
	}

	local_scp_spk_afe = afe;
	return 0;
}

struct mtk_base_afe *get_afe_base(void)
{
	if (!local_scp_spk_afe)
		pr_err("%s(), local_scp_spk_afe is NULL", __func__);

	return local_scp_spk_afe;
}

int set_scp_spk_base(struct mtk_base_scp_spk *scp_spk)
{
	if (!scp_spk) {
		pr_err("%s(), scp_spk is NULL", __func__);
		return -1;
	}

	local_base_scp_spk = scp_spk;
	return 0;

}

void *get_scp_spk_base(void)
{
	if (!local_base_scp_spk)
		pr_err("%s(), local_base_scp_spk is NULL", __func__);

	return local_base_scp_spk;
}

static void *ipi_recv_private;
void *get_ipi_recv_private(void)
{
	if (!ipi_recv_private)
		pr_err("%s(), ipi_recv_private is NULL", __func__);

	return ipi_recv_private;
}

void set_ipi_recv_private(void *priv)
{
	pr_debug("%s()\n", __func__);

	if (!ipi_recv_private)
		ipi_recv_private = priv;
	else
		pr_err("%s() has been set\n", __func__);
}

void mtk_scp_spk_dump_msg(struct mtk_base_scp_spk_dump *spk_dump)
{
	pr_info("%s()\n", __func__);

	spk_dump->dump_ops->spk_dump_callback = spkprotect_dump_message;
}

void mtk_scp_spk_ipi_send(uint8_t data_type, uint8_t ack_type,
			  uint16_t msg_id, uint32_t param1,
			  uint32_t param2, char *payload)
{
	struct ipi_msg_t ipi_msg;
	int send_result = 0;

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

unsigned int mtk_scp_spk_pack_payload(uint16_t msg_id, uint32_t param1,
				      uint32_t param2,
				      struct snd_dma_buffer *dma_buffer,
				      struct snd_pcm_substream *substream)
{
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	unsigned int ret = 0;

	/* clean payload data */
	memset((void *)spk_mem->ipi_payload_buf, 0,
		sizeof(uint32_t) * MAX_PAYLOAD_SIZE);
	switch (msg_id) {
	case SPK_PROTECT_PLATMEMPARAM:
		spk_mem->ipi_payload_buf[0] = (uint32_t)(dma_buffer->addr);
		spk_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);
		spk_mem->ipi_payload_buf[2] = dma_buffer->bytes;
		spk_mem->ipi_payload_buf[3] = true;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_DLMEMPARAM:
		spk_mem->ipi_payload_buf[0] = (uint32_t)dma_buffer->addr;
		spk_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);
		spk_mem->ipi_payload_buf[2] = dma_buffer->bytes;
		spk_mem->ipi_payload_buf[3] = param1;
		spk_mem->ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_PREPARE:
		spk_mem->ipi_payload_buf[0] =
			(uint32_t)(substream->runtime->format);
		spk_mem->ipi_payload_buf[1] =
			(uint32_t)(substream->runtime->rate);
		spk_mem->ipi_payload_buf[2] =
			(uint32_t)(substream->runtime->channels);
		spk_mem->ipi_payload_buf[3] =
			(uint32_t)(substream->runtime->period_size);
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_IVMEMPARAM:
		spk_mem->ipi_payload_buf[0] = (uint32_t)dma_buffer->addr;
		spk_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);
		spk_mem->ipi_payload_buf[2] = dma_buffer->bytes;
		spk_mem->ipi_payload_buf[3] = param1;
		spk_mem->ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_DLCOPY:
		spk_mem->ipi_payload_buf[0] = (uint32_t)param1;
		spk_mem->ipi_payload_buf[1] = (uint32_t)param2;
		ret = sizeof(unsigned int) * 2;
		break;
	case SPK_PROTECT_SPEECH_MDFEEDBACKPARAM:
		spk_mem->ipi_payload_buf[0] = (uint32_t)(dma_buffer->addr);
		spk_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);
		spk_mem->ipi_payload_buf[2] = dma_buffer->bytes;
		spk_mem->ipi_payload_buf[3] = param1;
		spk_mem->ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_SPEECH_DLMEMPARAM:
		spk_mem->ipi_payload_buf[0] = (uint32_t)dma_buffer->addr;
		spk_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);
		spk_mem->ipi_payload_buf[2] = dma_buffer->bytes;
		spk_mem->ipi_payload_buf[3] = param1;
		spk_mem->ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTECT_SPEECH_PREPARE:
		spk_mem->ipi_payload_buf[0] =
			(uint32_t)substream->runtime->format;
		spk_mem->ipi_payload_buf[1] =
			(uint32_t)substream->runtime->rate;
		spk_mem->ipi_payload_buf[2] =
			(uint32_t)substream->runtime->channels;
		spk_mem->ipi_payload_buf[3] =
			(uint32_t)substream->runtime->period_size;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_SPEECH_IVMEMPARAM:
		spk_mem->ipi_payload_buf[0] = (uint32_t)dma_buffer->addr;
		spk_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);
		spk_mem->ipi_payload_buf[2] = dma_buffer->bytes;
		spk_mem->ipi_payload_buf[3] = param1;
		spk_mem->ipi_payload_buf[4] = param2;
		ret = sizeof(unsigned int) * 5;
		break;
	case SPK_PROTTCT_PCMDUMP_ON:
		spk_mem->ipi_payload_buf[0] = param1;
		spk_mem->ipi_payload_buf[1] = param2;
		ret = sizeof(unsigned int) * 2;
		break;
	default:
		pr_err("%s(), msg_id not support\n", __func__);
		break;
	}

	return ret;
}

void set_afe_irq_target(int irq_usage, int scp_enable)
{
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[scp_spk->spk_mem.spk_dl_memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	int scp_irq_target_shift =
		SCP_IRQ_SHIFT_BIT + irq_data->irq_scp_en_shift;

	if (scp_enable) {
		regmap_update_bits(afe->regmap,
				   irq_data->irq_ap_en_reg,
				   0x1 << irq_data->irq_ap_en_shift,
				   0x0);

		regmap_update_bits(afe->regmap,
				   irq_data->irq_scp_en_reg,
				   0x1 << scp_irq_target_shift,
				   0x1 << scp_irq_target_shift);
	} else {
		regmap_update_bits(afe->regmap,
				   irq_data->irq_scp_en_reg,
				   0x1 << scp_irq_target_shift,
				   0);

		regmap_update_bits(afe->regmap,
				   irq_data->irq_ap_en_reg,
				   0x1 << irq_data->irq_ap_en_shift,
				   0x1 << irq_data->irq_ap_en_shift);
	}
}


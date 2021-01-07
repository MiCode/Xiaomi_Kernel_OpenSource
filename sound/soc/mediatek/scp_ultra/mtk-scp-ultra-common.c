// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.
// Copyright (C) 2020 XiaoMi, Inc.

#include "mtk-scp-ultra-common.h"
#include "mtk-base-scp-ultra.h"
#include <sound/soc.h>
#include <linux/device.h>
#include <linux/compat.h>
#include "scp_helper.h"
#include "scp_ipi.h"
#include "audio_ipi_platform.h"
//#include "audio_ultra_msg_id.h"
#include "ultra_ipi.h"
#include "audio_task_manager.h"
#include "mtk-base-afe.h"
#include "mtk-scp-ultra-platform-mem-control.h"


/* don't use this directly if not necessary */
static struct mtk_base_scp_ultra *local_base_scp_ultra;
static struct mtk_base_afe *local_scp_ultra_afe;

int ultra_set_dsp_afe(struct mtk_base_afe *afe)
{
	if (!afe) {
		pr_err("%s(), afe is NULL", __func__);
		return -1;
	}

	local_scp_ultra_afe = afe;
	return 0;
}

struct mtk_base_afe *ultra_get_afe_base(void)
{
	if (!local_scp_ultra_afe)
		pr_err("%s(), local_scp_ultra_afe is NULL", __func__);

	return local_scp_ultra_afe;
}

int set_scp_ultra_base(struct mtk_base_scp_ultra *scp_ultra)
{
	if (!scp_ultra) {
		pr_err("%s(), scp_ultra is NULL", __func__);
		return -1;
	}

	local_base_scp_ultra = scp_ultra;
	return 0;

}

void *get_scp_ultra_base(void)
{
	if (!local_base_scp_ultra)
		pr_err("%s(), local_base_scp_ultra is NULL", __func__);

	return local_base_scp_ultra;
}

static void *ipi_recv_private;
void *ultra_get_ipi_recv_private(void)
{
	if (!ipi_recv_private)
		pr_info("%s(), ipi_recv_private is NULL", __func__);

	return ipi_recv_private;
}

void ultra_set_ipi_recv_private(void *priv)
{
	pr_debug("%s()\n", __func__);

	if (!ipi_recv_private)
		ipi_recv_private = priv;
	else
		pr_info("%s() has been set\n", __func__);
}

//void mtk_scp_ultra_dump_msg(struct mtk_base_scp_ultra_dump *ultra_dump)
//{
//	pr_info("%s()\n", __func__);

//	ultra_dump->dump_ops->ultra_dump_callback = ultra_dump_message;
//}

//void mtk_scp_ultra_ipi_send(uint8_t data_type, uint8_t ack_type,
//			  uint16_t msg_id, uint32_t param1,
//			  uint32_t param2, char *payload)
//{
//	struct ipi_msg_t ipi_msg;
//	int send_result = 0;

//	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));
//	send_result = audio_send_ipi_msg(&ipi_msg,
//					 TASK_SCENE_OOC_ULTRASOUND,
//					 AUDIO_IPI_LAYER_TO_DSP,
//					 data_type,
//					 ack_type,
//					 msg_id,
//					 param1,
//					 param2,
//					 (char *)payload);

//	if (send_result != 0)
//		pr_err("%s(), scp_ipi send fail\n", __func__);
//}

//unsigned int mtk_scp_ultra_pack_payload(uint16_t msg_id, uint32_t param1,
//				      uint32_t param2,
//				      struct snd_dma_buffer *dma_buffer,
//				      struct snd_pcm_substream *substream)
//{

//	unsigned int ret = 0;
//	struct mtk_base_scp_ultra *scp_ultra;
//	struct mtk_base_scp_ultra_mem *ultra_mem;
//	pr_info("%s(), 1220_0226\n", __func__);

//	scp_ultra = get_scp_ultra_base();
//	if(NULL == scp_ultra)
//	{
//		pr_err("%s(),scp_ultra == null\n", __func__);
//		return 0;
//	}

//	if(NULL == dma_buffer )
//	{
//		pr_err("%s(),dma_buffer == null\n", __func__);
//	}
//	if( NULL == substream)
//	{
//		pr_err("%s(),substream == null\n", __func__);
//	}
//	else
//	{
//		pr_debug("substream != null %p\n", __func__,substream);
//	}

//	ultra_mem = &scp_ultra->ultra_mem;
//	if(NULL == ultra_mem)
//	{
//		pr_err("%s(),ultra_mem == null\n", __func__);
//		return 0;
//	}

//	if(NULL == ultra_mem->ipi_payload_buf)
//	{
//		pr_err("%s(),ipi_payload_buf == null\n", __func__);
//		return 0;
//	}
//	memset((void *)ultra_mem->ipi_payload_buf, 0,
//		sizeof(uint32_t) * MAX_PAYLOAD_SIZE);

//	pr_debug("%s(),msg_id = %d\n", __func__,msg_id);
//	switch (msg_id) {

//	case AUDIO_TASK_USND_MSG_ID_DLMEMPARAM:
//		ultra_mem->ipi_payload_buf[0] = (uint32_t)dma_buffer->addr;
//		if(NULL != dma_buffer->area)
//		ultra_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);

//		ultra_mem->ipi_payload_buf[2] = dma_buffer->bytes;
//		ultra_mem->ipi_payload_buf[3] = param1;
//		ultra_mem->ipi_payload_buf[4] = param2;
//		ret = sizeof(unsigned int) * 5;
//		break;
//	case AUDIO_TASK_USND_MSG_ID_ULMEMPARAM:
//		ultra_mem->ipi_payload_buf[0] = (uint32_t)dma_buffer->addr;

//		if(NULL != dma_buffer->area)
//		ultra_mem->ipi_payload_buf[1] = (uint32_t)(*dma_buffer->area);

//		ultra_mem->ipi_payload_buf[2] = dma_buffer->bytes;
//		ultra_mem->ipi_payload_buf[3] = param1;
//		ultra_mem->ipi_payload_buf[4] = param2;
//		ret = sizeof(unsigned int) * 5;
//		break;

//	case AUDIO_TASK_USND_MSG_ID_PREPARE:
//		ultra_mem->ipi_payload_buf[0] =
//			(uint32_t)(substream->runtime->format);
//		ultra_mem->ipi_payload_buf[1] =
//			(uint32_t)(substream->runtime->rate);
//		ultra_mem->ipi_payload_buf[2] =
//			(uint32_t)(substream->runtime->channels);
//		ultra_mem->ipi_payload_buf[3] =
//			(uint32_t)(substream->runtime->period_size);
//		ret = sizeof(unsigned int) * 4;
//		break;

//	case AUDIO_TASK_USND_MSG_ID_PCMDUMP_ON:
//		ultra_mem->ipi_payload_buf[0] = param1;
//		ultra_mem->ipi_payload_buf[1] = param2;
//		ret = sizeof(unsigned int) * 2;
//		break;

//	case AUDIO_TASK_USND_MSG_ID_IPI_INFO:
//		ultra_mem->ipi_payload_buf[0] = param1;
//		ultra_mem->ipi_payload_buf[1] = param2;
//		ret = sizeof(unsigned int) * 2;
//		break;
//	default:
//		pr_err("%s(), msg_id not support\n", __func__);
//		break;
//	}

//	return ret;
//}

void set_afe_irq_target(int irq_usage, int scp_enable)
{
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[scp_ultra->ultra_mem.ultra_dl_memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	// int scp_irq_target_shift =
		// SCP_IRQ_SHIFT_BIT + irq_data->irq_scp_en_shift;
	int scp_irq_target_shift = irq_data->irq_scp_en_shift;

	if (scp_enable) {
		pr_debug("%s(), ultra_dl_memif_id = %d,irq_data->irq_ap_en_reg:0x%x\n",
				__func__,
				scp_ultra->ultra_mem.ultra_dl_memif_id,
				irq_data->irq_ap_en_reg);
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
void set_afe_ul_irq_target(int irq_usage, int scp_enable)
{
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[scp_ultra->ultra_mem.ultra_ul_memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	// int scp_irq_target_shift =
		// SCP_IRQ_SHIFT_BIT + irq_data->irq_scp_en_shift;
	int scp_irq_target_shift = irq_data->irq_scp_en_shift;

	if (scp_enable) {
		pr_debug("%s(),ul_memif_id=%d,irq_ap_en_reg:0x%x\n",
				__func__,
				scp_ultra->ultra_mem.ultra_ul_memif_id,
				irq_data->irq_ap_en_reg);
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


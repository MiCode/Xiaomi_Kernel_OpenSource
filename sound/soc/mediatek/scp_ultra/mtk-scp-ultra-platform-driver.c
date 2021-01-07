/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-ultra-platform.c --  Mediatek scp ultra platform
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

#include <linux/module.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <linux/spinlock.h>
#include <asm/arch_timer.h>

#include "audio_task_manager.h"
#include "scp_helper.h"
#include "audio_ultra_msg_id.h"
//#include "audio_ipi_client_ultra.h"
#include "audio_task_manager.h"
#include "mtk-scp-ultra-mem-control.h"
#include "mtk-scp-ultra-platform-mem-control.h"
#include "mtk-scp-ultra-platform-driver.h"
#include "mtk-base-scp-ultra.h"
#include "mtk-scp-ultra-common.h"
#include "mtk-base-afe.h"
#include "audio_buf.h"
#include "ultra_ipi.h"
#include "mtk-scp-ultra_dump.h"
#include "scp_feature_define.h"
#include <linux/pm_wakeup.h>


//static DEFINE_SPINLOCK(scp_ultra_ringbuf_lock);

#define GET_SYSTEM_TIMER_CYCLE(void) \
	({ \
		unsigned long long __ret = arch_counter_get_cntvct(); \
		__ret; \
	})
#define ultra_IPIMSG_TIMEOUT (50)
#define ultra_WAITCHECK_INTERVAL_MS (2)
static bool ultra_ipi_wait;
static struct wakeup_source ultra_suspend_lock;
static const char *const mtk_scp_ultra_dump_str[] = {"off", "normal_dump",
						   "split_dump"};


void ultra_ipi_rx_internal(unsigned int msg_id, void *msg_data)
{
	switch (msg_id) {
	case AUDIO_TASK_USND_MSG_ID_PCMDUMP_OK: {
		ultra_dump_message(msg_data);
		break;
	}
	case AUDIO_TASK_USND_MSG_ID_DEBUG:
	{
		pr_debug("%s(), AUDIO_TASK_USND_MSG_ID_DEBUG \r", __func__);
		break;
	}
	default:
		break;
	}
}
bool ultra_ipi_rceive_ack(unsigned int msg_id,
			unsigned int msg_data)
{
	bool result = false;

	switch (msg_id) {
	case AUDIO_TASK_USND_MSG_ID_PCMDUMP_OK:
		result = true;
		break;
	default:
		pr_info("%s(), no relate msg id\r", __func__);
		break;
	}
	return result;
}

static int mtk_scp_ultra_scenario_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	const int scp_ultra_memif_id =
			get_scp_ultra_memif_id(SCP_ULTRA_DL_DAI_ID);
	struct mtk_base_afe *afe = get_afe_base();

	ucontrol->value.integer.value[0] =
				afe->memif[scp_ultra_memif_id].scp_ultra_enable;
	return 0;
}

static int mtk_scp_ultra_scenario_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe *afe = get_afe_base();
	const int scp_ultra_memif_dl_id =
			get_scp_ultra_memif_id(SCP_ULTRA_DL_DAI_ID);
	const int scp_ultra_memif_ul_id =
			get_scp_ultra_memif_id(SCP_ULTRA_UL_DAI_ID);

	dev_info(scp_ultra->dev,
		 "%s(), %ld\n",
		 __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] > 0) {
		afe->memif[scp_ultra_memif_dl_id].scp_ultra_enable = true;
		afe->memif[scp_ultra_memif_ul_id].scp_ultra_enable = true;
	} else {
		afe->memif[scp_ultra_memif_dl_id].scp_ultra_enable = false;
		afe->memif[scp_ultra_memif_ul_id].scp_ultra_enable = false;
	}

	return 0;
}

static int mtk_scp_ultra_scp_dump_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_scp_ultra_dump *ultra_dump = &scp_ultra->ultra_dump;

	dev_dbg(scp_ultra->dev, "%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
			ARRAY_SIZE(mtk_scp_ultra_dump_str)) {
		dev_dbg(scp_ultra->dev, "return -EINVAL\n");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = ultra_dump->dump_flag;
	return 0;
}

static int mtk_scp_ultra_scp_dump_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_scp_ultra_dump *ultra_dump = &scp_ultra->ultra_dump;
	//struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = get_afe_base();
	static int ctrl_val;
	int ret, timeout = 0;
	//unsigned int payload_len = 0;
	unsigned int payload[3];
	bool ret_val;

	dev_dbg(scp_ultra->dev, "%s(), value = %ld, dump_flag = %d\n",
		 __func__,
		 ucontrol->value.integer.value[0],
		 ultra_dump->dump_flag);

	if (ultra_dump->dump_flag == false &&
	    ucontrol->value.integer.value[0] > 0) {
		ctrl_val = ucontrol->value.integer.value[0];
		ultra_dump->dump_flag = true;

		/* scp ultra dump buffer use dram */
		if (afe->request_dram_resource)
			afe->request_dram_resource(afe->dev);

		if (ctrl_val == 1)
			ret = ultra_open_dump_file();
		else {
			dev_dbg(scp_ultra->dev,
				"%s(), value not support, return\n",
				__func__);
			return -1;
		}

		if (ret < 0) {
			dev_dbg(scp_ultra->dev,
				"%s(), open dump file fail, return\n",
				__func__);
			return -1;
		}

		// payload_len = mtk_scp_ultra_pack_payload(
					// AUDIO_TASK_USND_MSG_ID_PCMDUMP_ON,
					// ultra_dump->dump_resv_mem.size,
					// ultra_dump->dump_resv_mem.phy_addr,
					// NULL, NULL);
		// mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD,
				     // AUDIO_IPI_MSG_BYPASS_ACK,
				     // AUDIO_TASK_USND_MSG_ID_PCMDUMP_ON,
				     // payload_len,
				     // ultra_dump->dump_flag,
				     // (char *)ultra_mem->ipi_payload_buf);
		payload[0] = ultra_dump->dump_resv_mem.size;
		payload[1] = ultra_dump->dump_resv_mem.phy_addr;
		payload[2] = ultra_dump->dump_flag;

		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_PCMDUMP_ON,
		   3,
		   &payload[0],
		   ULTRA_IPI_BYPASS_ACK);
		ultra_ipi_wait = true;
	} else if (ultra_dump->dump_flag == true &&
		   ucontrol->value.integer.value[0] == 0) {
		ultra_dump->dump_flag = false;

		while (ultra_ipi_wait) {
			msleep(ultra_WAITCHECK_INTERVAL_MS);
			if (timeout++ >= ultra_IPIMSG_TIMEOUT)
				ultra_ipi_wait = false;
		}

		if (ctrl_val == 1)
			ultra_close_dump_file();
		else {
			dev_dbg(scp_ultra->dev,
				"%s(), value not support, return\n",
				__func__);
			return -1;
		}

		// mtk_scp_ultra_ipi_send(AUDIO_IPI_MSG_ONLY,
				     // AUDIO_IPI_MSG_BYPASS_ACK,
				     // AUDIO_TASK_USND_MSG_ID_PCMDUMP_OFF,
				     // 1, 0, NULL);
		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_PCMDUMP_OFF,
		   0,
		   NULL,
		   ULTRA_IPI_BYPASS_ACK);
		/* scp ultra dump buffer use dram */
		if (afe->release_dram_resource)
			afe->release_dram_resource(afe->dev);

		ctrl_val = ucontrol->value.integer.value[0];
	}
	return 0;
}

static const struct soc_enum mtk_scp_ultra_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mtk_scp_ultra_dump_str),
			    mtk_scp_ultra_dump_str),
};

static const struct snd_kcontrol_new scp_ultra_platform_kcontrols[] = {
	SOC_ENUM_EXT("mtk_scp_ultra_pcm_dump", mtk_scp_ultra_enum[0],
			mtk_scp_ultra_scp_dump_get, mtk_scp_ultra_scp_dump_set),
	SOC_SINGLE_EXT("mtk_scp_ultra_scenario",
			SND_SOC_NOPM, 0, 0x1, 0,
			mtk_scp_ultra_scenario_get,
			mtk_scp_ultra_scenario_set),
};

//static unsigned int scp_ultra_word_size_align(unsigned int in_size)
//{
//	unsigned int align_size;

//	align_size = in_size & 0xFFFFFFE0;
//	return align_size;
//}

//void mtk_scp_ultra_ipi_recv(struct ipi_msg_t *ipi_msg)
//{
//	struct mtk_base_scp_ultra *scp_ultra =
//		(struct mtk_base_scp_ultra *)get_ipi_recv_private();
//	struct mtk_base_scp_ultra_dump *ultra_dump = &scp_ultra->ultra_dump;

//	if (ipi_msg == NULL) {
//		dev_warn(scp_ultra->dev, "%s ipi_msg == NULL\n", __func__);
//		return;
//	}

//	switch (ipi_msg->msg_id) {
//	case AUDIO_TASK_USND_MSG_ID_PCMDUMP_IRQDL:
//		if (scp_ultra->ultra_mem.substream->runtime->status->state
//		    == SNDRV_PCM_STATE_RUNNING) {

//			snd_pcm_period_elapsed(scp_ultra->ultra_mem.substream);
//		} else {
//			dev_warn(scp_ultra->dev, "%s() state error", __func__);
//		}

//		break;
//	case AUDIO_TASK_USND_MSG_ID_PCMDUMP_OK:
//		if (ultra_dump->dump_ops->ultra_dump_callback != NULL)
//			ultra_dump->dump_ops->ultra_dump_callback(ipi_msg);
//		break;
//	default:
//		break;
//	}
//}

static int mtk_scp_ultra_pcm_open(struct snd_pcm_substream *substream)
{
	//int msg_id;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_base_scp_ultra *scp_ultra =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_scp_ultra_dump *ultra_dump = &scp_ultra->ultra_dump;
	//struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_afe_memif *memif;
	struct mtk_base_afe_memif *memiful;
	//unsigned int payload_len = 0;
	unsigned int payload[3];
	bool ret_val;

	//struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	struct mtk_base_afe_irq *irqsul;

	memif = &afe->memif[scp_ultra->ultra_mem.ultra_dl_memif_id];
	memiful = &afe->memif[scp_ultra->ultra_mem.ultra_ul_memif_id];
	irqsul = &afe->irqs[memiful->irq_usage];

	dev_info(scp_ultra->dev, "%s() ul_memif_id: %d, system cycle:%llu\n",
			__func__, scp_ultra->ultra_mem.ultra_dl_memif_id,
			GET_SYSTEM_TIMER_CYCLE());

	memcpy((void *)(&(runtime->hw)), (void *)scp_ultra->mtk_dsp_hardware,
			sizeof(struct snd_pcm_hardware));

	if (scp_ultra->ultra_mem.ultra_dl_memif_id < 0) {
		dev_info(scp_ultra->dev, "%s() ultra_ul_memif_id < 0, return\n",
				__func__);
		return 0;
	}

	dev_dbg(scp_ultra->dev, "%s() dl mem if = %d, ul mem if = %d\n",
			__func__,
			scp_ultra->ultra_mem.ultra_dl_memif_id,
			scp_ultra->ultra_mem.ultra_ul_memif_id);

	__pm_stay_awake(&ultra_suspend_lock);

	scp_register_feature(ULTRA_FEATURE_ID);

	ultra_start_engine_thread();

	set_afe_irq_target(memif->irq_usage, true);
	set_afe_ul_irq_target(memiful->irq_usage, true);

	scp_ultra->ultra_mem.substream = substream;

	// msg_id = AUDIO_TASK_USND_MSG_ID_ON;

	// mtk_scp_ultra_ipi_send(AUDIO_IPI_MSG_ONLY,
				//AUDIO_IPI_MSG_NEED_ACK, msg_id,
				//memiful->irq_usage,
				//irqsul->irq_data->irq_scp_en_reg,
				//NULL);

	// payload_len = mtk_scp_ultra_pack_payload(
				// AUDIO_TASK_USND_MSG_ID_IPI_INFO,
				// ultra_dump->dump_resv_mem.size,
				// ultra_dump->dump_resv_mem.phy_addr,
				// NULL, NULL);
	// mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD,
				 // AUDIO_IPI_MSG_BYPASS_ACK,
				 // AUDIO_TASK_USND_MSG_ID_IPI_INFO,
				 // payload_len,
				 // ultra_dump->dump_flag,
				 // (char *)ultra_mem->ipi_payload_buf);
		payload[0] = memiful->irq_usage;
		payload[1] =  irqsul->irq_data->irq_scp_en_reg;

		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_ON,
		   2,
		   &payload[0],
		   ULTRA_IPI_BYPASS_ACK);

		payload[0] = ultra_dump->dump_resv_mem.size;
		payload[1] = ultra_dump->dump_resv_mem.phy_addr;
		payload[2] = ultra_dump->dump_flag;

		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_IPI_INFO,
		   3,
		   &payload[0],
		   ULTRA_IPI_BYPASS_ACK);
	return 0;
}

static int mtk_scp_ultra_pcm_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct RingBuf *ring_buf = &scp_ultra->ultra_mem.platform_ringbuf;
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe_memif *dl_memif =
		&afe->memif[ultra_mem->ultra_dl_memif_id];
	struct mtk_base_afe_memif *ul_memif =
		&afe->memif[ultra_mem->ultra_ul_memif_id];
	//int irq_id = dl_memif->irq_usage;
	int irq_id = ul_memif->irq_usage;
	bool ret_val;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;

	dev_info(scp_ultra->dev, "%s(), system cycle:%llu\n",
		 __func__, GET_SYSTEM_TIMER_CYCLE());

	RingBuf_Reset(ring_buf);

	/* stop dl memif */
	regmap_update_bits(afe->regmap,
			   dl_memif->data->enable_reg,
			   1 << dl_memif->data->enable_shift,
			   0);

	/* stop ul memif */
	regmap_update_bits(afe->regmap,
			   ul_memif->data->enable_reg,
			   1 << ul_memif->data->enable_shift,
			   0);

	/* stop dl irq */
	regmap_update_bits(afe->regmap,
			   irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   0);

	// mtk_scp_ultra_ipi_send(AUDIO_IPI_MSG_ONLY,
				// AUDIO_IPI_MSG_DIRECT_SEND,
				// AUDIO_TASK_USND_MSG_ID_STOP, 1, 0, NULL);
	ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_STOP,
						   0,
						   NULL,
						   ULTRA_IPI_BYPASS_ACK);
	return 0;
}

static int mtk_scp_ultra_pcm_close(struct snd_pcm_substream *substream)
{
	//int msg_id;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_afe_memif *memif;
	struct mtk_base_afe_memif *memulif;
	int ret = 0;
	bool ret_val;

	dev_info(scp_ultra->dev, "%s() ultra_dl_memif_id:%d, system cycle:%llu\n",
		 __func__, scp_ultra->ultra_mem.ultra_dl_memif_id,
		 GET_SYSTEM_TIMER_CYCLE());

	if (scp_ultra->ultra_mem.ultra_dl_memif_id < 0) {
		dev_info(scp_ultra->dev, "%s() ultra_dl_memif_id < 0, return\n",
			__func__);
		return 0;
	}

	dev_dbg(scp_ultra->dev, "ultra_stop_engine_thread before\n");
	ultra_stop_engine_thread();

	ret = mtk_scp_ultra_pcm_stop(substream);
	memif = &afe->memif[scp_ultra->ultra_mem.ultra_dl_memif_id];
	memulif = &afe->memif[scp_ultra->ultra_mem.ultra_ul_memif_id];
	/* send to task with close information */

	// msg_id = AUDIO_TASK_USND_MSG_ID_OFF;
	// mtk_scp_ultra_ipi_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
			     // msg_id, 0, 0, NULL);

	ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_OFF,
						   0,
						   NULL,
						   ULTRA_IPI_BYPASS_ACK);

	scp_ultra->ultra_mem.substream = NULL;

	set_afe_irq_target(memif->irq_usage, false);
	set_afe_ul_irq_target(memulif->irq_usage, false);

	scp_deregister_feature(ULTRA_FEATURE_ID);
	__pm_relax(&ultra_suspend_lock);

	return 0;
}

static void mtk_scp_ultra_pcm_hw_params_dl(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = get_afe_base();
	//unsigned int payload_len = 0;
	unsigned int payload[3];
	bool ret_val;
	int dl_using_dram =
		afe->memif[ultra_mem->ultra_dl_memif_id].using_sram ? 0 : 1;
	int ul_using_dram =
		afe->memif[ultra_mem->ultra_ul_memif_id].using_sram ? 0 : 1;

	dev_info(scp_ultra->dev, "%s(), system cycle:%llu\n",
			__func__, GET_SYSTEM_TIMER_CYCLE());
	substream->runtime->dma_bytes = params_buffer_bytes(params);
	if (NULL == ultra_mem || NULL == substream || NULL == params) {
		dev_err(scp_ultra->dev, "param == null\n");
		return;
	}
	dev_info(scp_ultra->dev,
			"%s() dl mem=%d, ul mem=%d, dl dram:%d , ul dram:%d\n",
			__func__,
			ultra_mem->ultra_dl_memif_id,
			ultra_mem->ultra_ul_memif_id,
			dl_using_dram,
			ul_using_dram);

	payload[0] = dl_using_dram;
	payload[1] = ultra_mem->ultra_dl_memif_id;
	payload[2] = (unsigned int)(&ultra_mem->ultra_dl_dma_buf);

	ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_DLMEMPARAM,
			3,
			&payload[0],
			ULTRA_IPI_BYPASS_ACK);

	payload[0] = ul_using_dram;
	payload[1] = ultra_mem->ultra_ul_memif_id;
	payload[2] = (unsigned int)(&ultra_mem->ultra_ul_dma_buf);

	ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_ULMEMPARAM,
	   3,
	   &payload[0],
	   ULTRA_IPI_BYPASS_ACK);

	// payload_len = mtk_scp_ultra_pack_payload(
				// AUDIO_TASK_USND_MSG_ID_DLMEMPARAM,
				// dl_using_dram,
				// ultra_mem->ultra_dl_memif_id,
				// &ultra_mem->ultra_dl_dma_buf,
				// substream);
	// mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			// AUDIO_TASK_USND_MSG_ID_DLMEMPARAM, payload_len, 0,
			// (char *)ultra_mem->ipi_payload_buf);

	// payload_len =
		// mtk_scp_ultra_pack_payload(AUDIO_TASK_USND_MSG_ID_ULMEMPARAM,
					// ul_using_dram,
					// ultra_mem->ultra_ul_memif_id,
					// &ultra_mem->ultra_ul_dma_buf,
					// substream);
	// mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			// AUDIO_TASK_USND_MSG_ID_ULMEMPARAM, payload_len, 0,
			// (char *)ultra_mem->ipi_payload_buf);
}

//static void mtk_scp_ultra_pcm_hw_params_ul(
//		struct snd_pcm_substream *substream)
//{
//	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct mtk_base_scp_ultra *scp_ultra =
//			snd_soc_platform_get_drvdata(rtd->platform);
//	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
//	struct mtk_base_afe *afe = get_afe_base();
//	unsigned int payload_len = 0;
//	int dl_using_dram =
//		afe->memif[ultra_mem->ultra_dl_memif_id].using_sram ? 0 : 1;
//	int ul_using_dram =
//		afe->memif[ultra_mem->ultra_ul_memif_id].using_sram ? 0 : 1;
//	dev_info(scp_ultra->dev, "%s(), system cycle:%llu\n",
//		 __func__, GET_SYSTEM_TIMER_CYCLE());

//	payload_len = mtk_scp_ultra_pack_payload(ULTRA_SPEECH_DLMEMPARAM,
//			dl_using_dram,
//			ultra_mem->ultra_dl_memif_id,
//			&ultra_mem->ultra_dl_dma_buf,
//			substream);
//	mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
//			ULTRA_SPEECH_DLMEMPARAM, payload_len, 0,
//			(char *)ultra_mem->ipi_payload_buf);

//	payload_len =
//		mtk_scp_ultra_pack_payload(ultra_PROTECT_SPEECH_IVMEMPARAM,
//				ul_using_dram,
//				ultra_mem->ultra_ul_memif_id,
//				&ultra_mem->ultra_ul_dma_buf,
//				substream);
//	mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
//			ultra_PROTECT_SPEECH_IVMEMPARAM, payload_len, 0,
//			(char *)ultra_mem->ipi_payload_buf);
//}

static int mtk_scp_ultra_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_scp_ultra_pcm_hw_params_dl(substream, params);
	//else
		//mtk_scp_ultra_pcm_hw_params_ul(substream);

	return 0;
}

static int mtk_scp_ultra_ul_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe_memif *ul_memif =
		&afe->memif[ultra_mem->ultra_ul_memif_id];
	int irq_id = ul_memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int counter = runtime->period_size;
	int fs;

	dev_info(scp_ultra->dev, "%s(), counter:%d, stream:%d, system cycle:%llu\n",
		 __func__, counter, substream->stream,
		 GET_SYSTEM_TIMER_CYCLE());

	/* set ul irq counter */
	dev_dbg(scp_ultra->dev, "%s ul irq_data->irq_cnt_reg:%d", __func__,
			irq_data->irq_cnt_reg);
	regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
			   irq_data->irq_cnt_maskbit << irq_data->irq_cnt_shift,
			   counter << irq_data->irq_cnt_shift);

	/* set ul irq fs */
	fs = afe->irq_fs(substream, runtime->rate);

	if (fs < 0)
		return -EINVAL;
	dev_dbg(scp_ultra->dev, "%s ul irq_data->irq_fs_reg:%d",
			__func__, irq_data->irq_fs_reg);
	regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
			  irq_data->irq_fs_maskbit << irq_data->irq_fs_shift,
			  fs << irq_data->irq_fs_shift);

	/* start ul memif */
	dev_dbg(scp_ultra->dev, "%s ul ul_memif->data->enable_reg:%d",
			__func__, ul_memif->data->enable_reg);
	regmap_update_bits(afe->regmap,
			ul_memif->data->enable_reg,
			1 << ul_memif->data->enable_shift,
			1 << ul_memif->data->enable_shift);

	/* start ul irq */
	dev_dbg(scp_ultra->dev, "%s ul irq_data->irq_en_reg:%d",
			__func__, irq_data->irq_en_reg);
	regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			1 << irq_data->irq_en_shift,
			1 << irq_data->irq_en_shift);

	return 0;
}

static int mtk_scp_ultra_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe_memif *dl_memif =
		&afe->memif[ultra_mem->ultra_dl_memif_id];
	struct mtk_base_afe_memif *ul_memif =
		&afe->memif[ultra_mem->ultra_ul_memif_id];
	int irq_id = dl_memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int counter = runtime->period_size;
	int fs;
	bool ret_val;

	dev_info(scp_ultra->dev, "%s(), dl irq cnt:%d, stream:%d, cycle:%llu\n",
			__func__, counter, substream->stream,
			GET_SYSTEM_TIMER_CYCLE());

	/* start ul memif */
	dev_info(scp_ultra->dev, "%s() ul_memif->data->enable_reg:%d",
			__func__, ul_memif->data->enable_reg);

	regmap_update_bits(afe->regmap,
			ul_memif->data->enable_reg,
			1 << ul_memif->data->enable_shift,
			1 << ul_memif->data->enable_shift);

	/* set dl irq counter */
	regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
			irq_data->irq_cnt_maskbit
			<< irq_data->irq_cnt_shift,
			counter << irq_data->irq_cnt_shift);

	/* set dl irq fs */
	fs = afe->irq_fs(substream, runtime->rate);

	if (fs < 0)
		return -EINVAL;

	regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
			irq_data->irq_fs_maskbit
			<< irq_data->irq_fs_shift,
			fs << irq_data->irq_fs_shift);

	/* start dl memif */
	regmap_update_bits(afe->regmap,
			dl_memif->data->enable_reg,
			1 << dl_memif->data->enable_shift,
			1 << dl_memif->data->enable_shift);

	/* start dl irq */
	//regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
	//		   1 << irq_data->irq_en_shift,
	//		   1 << irq_data->irq_en_shift);

	mtk_scp_ultra_ul_pcm_start(substream);

	// mtk_scp_ultra_ipi_send(AUDIO_IPI_MSG_ONLY,
			// AUDIO_IPI_MSG_DIRECT_SEND,
			// AUDIO_TASK_USND_MSG_ID_START, 1, 0, NULL);
	ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_START,
			0,
			NULL,
			ULTRA_IPI_BYPASS_ACK);
	return 0;
}

static int mtk_scp_ultra_pcm_hw_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);
	//int payload_len = 0;
	//int msg_id;
	int ret = 0;
	bool ret_val;
	unsigned int payload[4];

	dev_info(scp_ultra->dev, "%s(), system cycle:%llu\n",
			__func__, GET_SYSTEM_TIMER_CYCLE());

	// msg_id = AUDIO_TASK_USND_MSG_ID_PREPARE;
	// payload_len =
		// mtk_scp_ultra_pack_payload(msg_id, 0, 0,
					 // NULL, substream);
	// mtk_scp_ultra_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     // msg_id, payload_len,
			     // 0,
			     // (char *)scp_ultra->ultra_mem.ipi_payload_buf);
	payload[0] = (unsigned int)(substream->runtime->format);
	payload[1] = (unsigned int)(substream->runtime->rate);
	payload[2] = (unsigned int)(substream->runtime->channels);
	payload[3] = (unsigned int)(substream->runtime->period_size);

	ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_PREPARE,
			4,
			&payload[0],
			ULTRA_IPI_BYPASS_ACK);
	ret = mtk_scp_ultra_pcm_start(substream);
	return 0;
}

static int mtk_scp_ultra_pcm_hw_trigger(struct snd_pcm_substream *substream,
				      int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_scp_ultra_pcm_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_scp_ultra_pcm_stop(substream);
	}
	return -EINVAL;
}

static int mtk_scp_ultra_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(scp_ultra->dev, "%s()\n", __func__);

	snd_soc_add_platform_controls(rtd->platform,
				      scp_ultra_platform_kcontrols,
				      ARRAY_SIZE(scp_ultra_platform_kcontrols));

	ret = mtk_scp_ultra_reserved_dram_init();
	if (ret < 0)
		return ret;
	ultra_ipi_register(ultra_ipi_rx_internal, ultra_ipi_rceive_ack);
	audio_ipi_client_ultra_init();
	wakeup_source_init(&ultra_suspend_lock, "ultra wakelock");
	//mtk_scp_ultra_dump_msg(&scp_ultra->ultra_dump);
	// ret = audio_task_register_callback(TASK_SCENE_OOC_ULTRASOUND,
					   // mtk_scp_ultra_ipi_recv, NULL);

	// if (ret < 0)
		// return ret;

	return ret;
}

static const struct snd_pcm_ops mtk_scp_ultra_pcm_ops = {
	.open = mtk_scp_ultra_pcm_open,
	.close = mtk_scp_ultra_pcm_close,
	.hw_params = mtk_scp_ultra_pcm_hw_params,
	.prepare = mtk_scp_ultra_pcm_hw_prepare,
	.trigger = mtk_scp_ultra_pcm_hw_trigger,
	.ioctl = snd_pcm_lib_ioctl,
};

const struct snd_soc_platform_driver mtk_scp_ultra_pcm_platform = {
	.ops = &mtk_scp_ultra_pcm_ops,
	.pcm_new = mtk_scp_ultra_pcm_new,
};
EXPORT_SYMBOL_GPL(mtk_scp_ultra_pcm_platform);

MODULE_DESCRIPTION("Mediatek scp ultra platform driver");
MODULE_AUTHOR("Youwei Dong <Youwei.Dong@mediatek.com>");
MODULE_LICENSE("GPL v2");


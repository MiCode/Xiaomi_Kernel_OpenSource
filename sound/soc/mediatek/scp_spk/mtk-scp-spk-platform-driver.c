/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-spk-platform.c --  Mediatek scp spk platform
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

#include <linux/module.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <linux/spinlock.h>
#include <asm/arch_timer.h>

#include "audio_task_manager.h"
#include "scp_helper.h"
#include "audio_spkprotect_msg_id.h"
#include "audio_ipi_client_spkprotect.h"
#include "audio_task_manager.h"
#include "mtk-scp-spk-mem-control.h"
#include "mtk-scp-spk-platform-mem-control.h"
#include "mtk-scp-spk-platform-driver.h"
#include "mtk-base-scp-spk.h"
#include "mtk-scp-spk-common.h"
#include "mtk-base-afe.h"
#include "audio_buf.h"

static DEFINE_SPINLOCK(scp_spk_ringbuf_lock);

#define GET_SYSTEM_TIMER_CYCLE(void) \
	({ \
		unsigned long long __ret = arch_counter_get_cntvct(); \
		__ret; \
	})
#define SPK_IPIMSG_TIMEOUT (50)
#define SPK_WAITCHECK_INTERVAL_MS (2)
static bool spk_ipi_wait;
static const char *const mtk_scp_spk_dump_str[] = {"off", "normal_dump",
						   "split_dump"};

static const struct soc_enum mtk_scp_spk_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mtk_scp_spk_dump_str),
			    mtk_scp_spk_dump_str),
};

static int mtk_scp_spk_scp_dump_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_scp_spk_dump *spk_dump = &scp_spk->spk_dump;

	dev_dbg(scp_spk->dev, "%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(mtk_scp_spk_dump_str)) {
		dev_dbg(scp_spk->dev, "return -EINVAL\n");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = spk_dump->dump_flag;
	return 0;
}

static int mtk_scp_spk_scp_dump_set(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_scp_spk_dump *spk_dump = &scp_spk->spk_dump;
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct mtk_base_afe *afe = get_afe_base();
	static int ctrl_val;
	int ret, timeout = 0;
	unsigned int payload_len = 0;

	dev_dbg(scp_spk->dev, "%s(), value = %ld, dump_flag = %d\n",
		 __func__,
		 ucontrol->value.integer.value[0],
		 spk_dump->dump_flag);

	if (spk_dump->dump_flag == false &&
	    ucontrol->value.integer.value[0] > 0) {
		ctrl_val = ucontrol->value.integer.value[0];
		spk_dump->dump_flag = true;

		/* scp spk dump buffer use dram */
		if (afe->request_dram_resource)
			afe->request_dram_resource(afe->dev);

		if (ctrl_val == 1)
			ret = spkprotect_open_dump_file();
		else if (ctrl_val == 2)
			spk_pcm_dump_split_task_enable();
		else {
			dev_dbg(scp_spk->dev,
				"%s(), value not support, return\n",
				__func__);
			return -1;
		}

		if (ret < 0) {
			dev_dbg(scp_spk->dev,
				"%s(), open dump file fail, return\n",
				__func__);
			return -1;
		}

		payload_len = mtk_scp_spk_pack_payload(
					SPK_PROTTCT_PCMDUMP_ON,
					spk_dump->dump_resv_mem.size,
					spk_dump->dump_resv_mem.phy_addr,
					NULL, NULL);
		mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD,
				     AUDIO_IPI_MSG_BYPASS_ACK,
				     SPK_PROTTCT_PCMDUMP_ON,
				     payload_len,
				     spk_dump->dump_flag,
				     (char *)spk_mem->ipi_payload_buf);

		spk_ipi_wait = true;
	} else if (spk_dump->dump_flag == true &&
		   ucontrol->value.integer.value[0] == 0) {
		spk_dump->dump_flag = false;

		while (spk_ipi_wait) {
			msleep(SPK_WAITCHECK_INTERVAL_MS);
			if (timeout++ >= SPK_IPIMSG_TIMEOUT)
				spk_ipi_wait = false;
		}

		if (ctrl_val == 1)
			spkprotect_close_dump_file();
		else if (ctrl_val == 2)
			spk_pcm_dump_split_task_disable();
		else {
			dev_dbg(scp_spk->dev,
				"%s(), value not support, return\n",
				__func__);
			return -1;
		}

		mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_BYPASS_ACK,
				     SPK_PROTTCT_PCMDUMP_OFF,
				     1, 0, NULL);

		/* scp spk dump buffer use dram */
		if (afe->release_dram_resource)
			afe->release_dram_resource(afe->dev);

		ctrl_val = ucontrol->value.integer.value[0];
	}
	return 0;
}

static int mtk_scp_spk_dl_scenario_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	const int scp_spk_memif_id = get_scp_spk_memif_id(SCP_SPK_DL_DAI_ID);
	struct mtk_base_afe *afe = get_afe_base();

	ucontrol->value.integer.value[0] =
				afe->memif[scp_spk_memif_id].scp_spk_enable;
	return 0;
}

static int mtk_scp_spk_dl_scenario_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe *afe = get_afe_base();
	const int scp_spk_memif_id = get_scp_spk_memif_id(SCP_SPK_DL_DAI_ID);

	dev_info(scp_spk->dev,
		 "%s(), %d\n",
		 __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] > 0)
		afe->memif[scp_spk_memif_id].scp_spk_enable = true;
	else
		afe->memif[scp_spk_memif_id].scp_spk_enable = false;

	return 0;
}

static int mtk_scp_spk_iv_scenario_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	const int scp_spk_memif_id = get_scp_spk_memif_id(SCP_SPK_IV_DAI_ID);
	struct mtk_base_afe *afe = get_afe_base();

	ucontrol->value.integer.value[0] =
				afe->memif[scp_spk_memif_id].scp_spk_enable;
	return 0;
}

static int mtk_scp_spk_iv_scenario_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe *afe = get_afe_base();
	const int scp_spk_memif_id = get_scp_spk_memif_id(SCP_SPK_IV_DAI_ID);

	dev_info(scp_spk->dev,
		 "%s(), %d\n",
		 __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] > 0)
		afe->memif[scp_spk_memif_id].scp_spk_enable = true;
	else
		afe->memif[scp_spk_memif_id].scp_spk_enable = false;

	return 0;
}

static int mtk_scp_spk_mdul_scenario_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	const int scp_spk_memif_id = get_scp_spk_memif_id(SCP_SPK_MDUL_DAI_ID);
	struct mtk_base_afe *afe = get_afe_base();

	ucontrol->value.integer.value[0] =
				afe->memif[scp_spk_memif_id].scp_spk_enable;
	return 0;
}

static int mtk_scp_spk_mdul_scenario_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_afe *afe = get_afe_base();
	const int scp_spk_memif_id = get_scp_spk_memif_id(SCP_SPK_MDUL_DAI_ID);

	dev_info(scp_spk->dev,
		 "%s(), %d\n",
		 __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] > 0)
		afe->memif[scp_spk_memif_id].scp_spk_enable = true;
	else
		afe->memif[scp_spk_memif_id].scp_spk_enable = false;

	return 0;
}

static int mtk_scp_spk_set_iv_tcm_buf_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;

	ucontrol->value.integer.value[0] = spk_mem->is_iv_buf_in_tcm;
	return 0;
}

static int mtk_scp_spk_set_iv_tcm_buf_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_spk *scp_spk = snd_soc_component_get_drvdata(cmpnt);

	dev_info(scp_spk->dev,
		 "%s(), %d\n",
		 __func__, ucontrol->value.integer.value[0]);

	if (ucontrol->value.integer.value[0] > 0)
		ret = mtk_scp_spk_allocate_tcm_iv_buf();

	return ret;
}

static const struct snd_kcontrol_new scp_spk_platform_kcontrols[] = {
	SOC_SINGLE_EXT("mtk_scp_spk_set_iv_tcm_buf",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mtk_scp_spk_set_iv_tcm_buf_get,
		       mtk_scp_spk_set_iv_tcm_buf_set),
	SOC_ENUM_EXT("mtk_scp_spk_pcm_dump", mtk_scp_spk_enum[0],
		     mtk_scp_spk_scp_dump_get, mtk_scp_spk_scp_dump_set),
	SOC_SINGLE_EXT("mtk_scp_spk_dl_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mtk_scp_spk_dl_scenario_get,
		       mtk_scp_spk_dl_scenario_set),
	SOC_SINGLE_EXT("mtk_scp_spk_iv_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mtk_scp_spk_iv_scenario_get,
		       mtk_scp_spk_iv_scenario_set),
	SOC_SINGLE_EXT("mtk_scp_spk_mdul_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       mtk_scp_spk_mdul_scenario_get,
		       mtk_scp_spk_mdul_scenario_set),
};

static unsigned int scp_spk_word_size_align(unsigned int in_size)
{
	unsigned int align_size;

	align_size = in_size & 0xFFFFFFE0;
	return align_size;
}

static snd_pcm_uframes_t mtk_scp_spk_pcm_pointer
			 (struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct RingBuf *ring_buf = &spk_mem->platform_ringbuf;
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[spk_mem->spk_dl_memif_id];
	const struct mtk_base_memif_data *memif_data = memif->data;
	int reg_ofs_base, reg_ofs_cur;
	int ret, pcm_consume_bytes, pcm_remap_consume_bytes;
	unsigned int hw_ptr = 0, hw_base = 0, ring_buf_ptr, pcm_hw_ptr;
	unsigned long flags;
	bool underflow = false;

	reg_ofs_base = memif_data->reg_ofs_base;
	reg_ofs_cur = memif_data->reg_ofs_cur;

	ret = regmap_read(afe->regmap, reg_ofs_base, &hw_base);
	if (ret || hw_base == 0) {
		dev_err(scp_spk->dev, "1 %s hw_base err: %d\n",
			__func__, hw_base);
		pcm_consume_bytes = 0;
		pcm_remap_consume_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	ret = regmap_read(afe->regmap, reg_ofs_cur, &hw_ptr);
	if (ret || hw_ptr == 0) {
		dev_err(scp_spk->dev, "2 %s hw_ptr err: %d\n",
			__func__, hw_ptr);
		pcm_consume_bytes = 0;
		pcm_remap_consume_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	spin_lock_irqsave(&scp_spk_ringbuf_lock, flags);

	pcm_hw_ptr = hw_ptr - hw_base;
	ring_buf_ptr = (unsigned int)(ring_buf->pRead - ring_buf->pBufBase);
	if (pcm_hw_ptr >= ring_buf_ptr)
		pcm_consume_bytes = pcm_hw_ptr - ring_buf_ptr;
	else
		pcm_consume_bytes = pcm_hw_ptr - ring_buf_ptr +
				    ring_buf->bufLen;

	pcm_remap_consume_bytes = scp_spk_word_size_align(pcm_consume_bytes);
	scp_spk_debug("%s(), consume_bytes:0x%x(%d), datacount:%d\n",
		      __func__, pcm_remap_consume_bytes,
		      pcm_remap_consume_bytes,
		      ring_buf->datacount);

	if (pcm_remap_consume_bytes > ring_buf->datacount) {
		dev_err(scp_spk->dev, "%s(), underflow\n", __func__);
		underflow = true;
	}
	RingBuf_update_readptr(ring_buf, pcm_remap_consume_bytes);
	ring_buf_ptr = (unsigned int)(ring_buf->pRead - ring_buf->pBufBase);

	spin_unlock_irqrestore(&scp_spk_ringbuf_lock, flags);

POINTER_RETURN_FRAMES:
	if (underflow)
		return -1;

	return bytes_to_frames(substream->runtime, ring_buf_ptr);
}

void mtk_scp_spk_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	struct mtk_base_scp_spk *scp_spk =
		(struct mtk_base_scp_spk *)get_ipi_recv_private();
	struct mtk_base_scp_spk_dump *spk_dump = &scp_spk->spk_dump;

	if (ipi_msg == NULL) {
		dev_warn(scp_spk->dev, "%s ipi_msg == NULL\n", __func__);
		return;
	}

	switch (ipi_msg->msg_id) {
	case SPK_PROTECT_IRQDL:
		if (scp_spk->spk_mem.substream->runtime->status->state
		    == SNDRV_PCM_STATE_RUNNING) {
			/* notify subsream */
			snd_pcm_period_elapsed(scp_spk->spk_mem.substream);
		} else {
			dev_warn(scp_spk->dev, "%s() state error", __func__);
		}

		break;
	case SPK_PROTECT_PCMDUMP_OK:
		if (spk_dump->dump_ops->spk_dump_callback != NULL)
			spk_dump->dump_ops->spk_dump_callback(ipi_msg);
		break;
	default:
		break;
	}
}

static int mtk_scp_spk_pcm_open(struct snd_pcm_substream *substream)
{
	int msg_id;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_base_scp_spk *scp_spk =
		snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_afe_memif *memif;
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];

	dev_info(scp_spk->dev, "%s() spk_dl_memif_id: %d, system cycle:%llu\n",
		 __func__, scp_spk->spk_mem.spk_dl_memif_id,
		 GET_SYSTEM_TIMER_CYCLE());

	memcpy((void *)(&(runtime->hw)), (void *)scp_spk->mtk_dsp_hardware,
	       sizeof(struct snd_pcm_hardware));

	if (scp_spk->spk_mem.spk_dl_memif_id < 0) {
		dev_info(scp_spk->dev, "%s() spk_dl_memif_id < 0, return\n",
			 __func__);
		return 0;
	}

	memif = &afe->memif[scp_spk->spk_mem.spk_dl_memif_id];

	scp_register_feature(SPEAKER_PROTECT_FEATURE_ID);

	set_afe_irq_target(memif->irq_usage, true);

	scp_spk->spk_mem.substream = substream;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		msg_id = SPK_PROTECT_OPEN;
	else
		msg_id = SPK_PROTECT_SPEECH_OPEN;

	/* send to task with open information */
	mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY,
			     AUDIO_IPI_MSG_NEED_ACK, msg_id,
			     memif->irq_usage, irqs->irq_data->irq_scp_en_reg,
			     NULL);

	return 0;
}

static int mtk_scp_spk_pcm_close(struct snd_pcm_substream *substream)
{
	int msg_id;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_afe_memif *memif;

	dev_info(scp_spk->dev, "%s() spk_dl_memif_id:%d, system cycle:%llu\n",
		 __func__, scp_spk->spk_mem.spk_dl_memif_id,
		 GET_SYSTEM_TIMER_CYCLE());

	if (scp_spk->spk_mem.spk_dl_memif_id < 0) {
		dev_info(scp_spk->dev, "%s() spk_dl_memif_id < 0, return\n",
			 __func__);
		return 0;
	}

	memif = &afe->memif[scp_spk->spk_mem.spk_dl_memif_id];

	/* send to task with close information */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		msg_id = SPK_PROTECT_CLOSE;
	else
		msg_id = SPK_PROTECT_SPEECH_CLOSE;

	mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
			     msg_id, 0, 0, NULL);

	scp_spk->spk_mem.substream = NULL;

	set_afe_irq_target(memif->irq_usage, false);

	scp_deregister_feature(SPEAKER_PROTECT_FEATURE_ID);

	return 0;
}

static void mtk_scp_spk_pcm_hw_params_dl(struct snd_pcm_substream *substream,
					 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct mtk_base_afe *afe = get_afe_base();
	struct RingBuf *ring_buf = &spk_mem->platform_ringbuf;
	unsigned int payload_len = 0;
	int dl_using_dram =
		afe->memif[spk_mem->spk_dl_memif_id].using_sram ? 0 : 1;
	int iv_using_dram =
		afe->memif[spk_mem->spk_iv_memif_id].using_sram ? 0 : 1;

	dev_info(scp_spk->dev, "%s(), system cycle:%llu\n",
		 __func__, GET_SYSTEM_TIMER_CYCLE());

	substream->runtime->dma_bytes = params_buffer_bytes(params);
	mtk_scp_spk_allocate_platform_buf(substream->runtime->dma_bytes,
					  &substream->runtime->dma_addr,
					  &substream->runtime->dma_area);

	init_ring_buf(ring_buf,
		      (char *)spk_mem->platform_dma_buf.area,
		      spk_mem->platform_dma_buf.bytes);

	payload_len = mtk_scp_spk_pack_payload(SPK_PROTECT_PLATMEMPARAM, 0, 0,
					       &spk_mem->platform_dma_buf,
					       substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     SPK_PROTECT_PLATMEMPARAM, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);

	payload_len = mtk_scp_spk_pack_payload(SPK_PROTECT_DLMEMPARAM,
					       dl_using_dram,
					       spk_mem->spk_dl_memif_id,
					       &spk_mem->spk_dl_dma_buf,
					       substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     SPK_PROTECT_DLMEMPARAM, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);

	payload_len =
		mtk_scp_spk_pack_payload(SPK_PROTECT_IVMEMPARAM,
					 iv_using_dram,
					 spk_mem->spk_iv_memif_id,
					 &spk_mem->spk_iv_dma_buf,
					 substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     SPK_PROTECT_IVMEMPARAM, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);
}

static void mtk_scp_spk_pcm_hw_params_ul(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct mtk_base_afe *afe = get_afe_base();
	unsigned int payload_len = 0;
	int dl_using_dram =
		afe->memif[spk_mem->spk_dl_memif_id].using_sram ? 0 : 1;
	int iv_using_dram =
		afe->memif[spk_mem->spk_iv_memif_id].using_sram ? 0 : 1;
	int md_ul_using_dram =
		afe->memif[spk_mem->spk_md_ul_memif_id].using_sram ? 0 : 1;

	dev_info(scp_spk->dev, "%s(), system cycle:%llu\n",
		 __func__, GET_SYSTEM_TIMER_CYCLE());

	payload_len = mtk_scp_spk_pack_payload(
				SPK_PROTECT_SPEECH_MDFEEDBACKPARAM,
				md_ul_using_dram,
				spk_mem->spk_md_ul_memif_id,
				&spk_mem->spk_md_ul_dma_buf,
				substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     SPK_PROTECT_SPEECH_MDFEEDBACKPARAM, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);

	payload_len = mtk_scp_spk_pack_payload(SPK_PROTECT_SPEECH_DLMEMPARAM,
					       dl_using_dram,
					       spk_mem->spk_dl_memif_id,
					       &spk_mem->spk_dl_dma_buf,
					       substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     SPK_PROTECT_SPEECH_DLMEMPARAM, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);

	payload_len =
		mtk_scp_spk_pack_payload(SPK_PROTECT_SPEECH_IVMEMPARAM,
					 iv_using_dram,
					 spk_mem->spk_iv_memif_id,
					 &spk_mem->spk_iv_dma_buf,
					 substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     SPK_PROTECT_SPEECH_IVMEMPARAM, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);
}

static int mtk_scp_spk_pcm_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_scp_spk_pcm_hw_params_dl(substream, params);
	else
		mtk_scp_spk_pcm_hw_params_ul(substream);

	return 0;
}

static int mtk_scp_spk_pcm_hw_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	int payload_len = 0;
	int msg_id;

	dev_info(scp_spk->dev, "%s(), system cycle:%llu\n",
		 __func__, GET_SYSTEM_TIMER_CYCLE());

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		msg_id = SPK_PROTECT_PREPARE;
	else
		msg_id = SPK_PROTECT_SPEECH_PREPARE;

	payload_len =
		mtk_scp_spk_pack_payload(msg_id, 0, 0,
					 NULL, substream);
	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
			     msg_id, payload_len,
			     0,
			     (char *)scp_spk->spk_mem.ipi_payload_buf);

	return 0;
}

static int mtk_scp_spk_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct mtk_base_afe_memif *dl_memif =
		&afe->memif[spk_mem->spk_dl_memif_id];
	struct mtk_base_afe_memif *iv_memif =
		&afe->memif[spk_mem->spk_iv_memif_id];
	struct mtk_base_afe_memif *md_ul_memif =
		&afe->memif[spk_mem->spk_md_ul_memif_id];
	int irq_id = dl_memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int counter = runtime->period_size;
	int fs;

	dev_info(scp_spk->dev, "%s(), counter:%d, stream:%d, system cycle:%llu\n",
		 __func__, counter, substream->stream,
		 GET_SYSTEM_TIMER_CYCLE());

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* start md ul memif */
		regmap_update_bits(afe->regmap,
				   md_ul_memif->data->enable_reg,
				   1 << md_ul_memif->data->enable_shift,
				   1 << md_ul_memif->data->enable_shift);
	}

	/* start iv memif */
	regmap_update_bits(afe->regmap,
			   iv_memif->data->enable_reg,
			   1 << iv_memif->data->enable_shift,
			   1 << iv_memif->data->enable_shift);

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
	regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   1 << irq_data->irq_en_shift);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_DIRECT_SEND,
				     SPK_PROTECT_SPEECH_START, 1, 0, NULL);
	} else {
		mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_DIRECT_SEND,
				     SPK_PROTECT_START, 1, 0, NULL);
	}

	return 0;
}

static int mtk_scp_spk_pcm_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct RingBuf *ring_buf = &scp_spk->spk_mem.platform_ringbuf;
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct mtk_base_afe_memif *dl_memif =
		&afe->memif[spk_mem->spk_dl_memif_id];
	struct mtk_base_afe_memif *iv_memif =
		&afe->memif[spk_mem->spk_iv_memif_id];
	struct mtk_base_afe_memif *md_ul_memif =
		&afe->memif[spk_mem->spk_md_ul_memif_id];
	int irq_id = dl_memif->irq_usage;
	struct mtk_base_afe_irq *irqs = &afe->irqs[irq_id];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;

	dev_info(scp_spk->dev, "%s(), system cycle:%llu\n",
		 __func__, GET_SYSTEM_TIMER_CYCLE());

	RingBuf_Reset(ring_buf);

	/* stop dl memif */
	regmap_update_bits(afe->regmap,
			   dl_memif->data->enable_reg,
			   1 << dl_memif->data->enable_shift,
			   0);

	/* stop iv memif */
	regmap_update_bits(afe->regmap,
			   iv_memif->data->enable_reg,
			   1 << iv_memif->data->enable_shift,
			   0);

	/* stop dl irq */
	regmap_update_bits(afe->regmap,
			   irq_data->irq_en_reg,
			   1 << irq_data->irq_en_shift,
			   0);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* start md ul memif */
		regmap_update_bits(afe->regmap,
				   md_ul_memif->data->enable_reg,
				   1 << md_ul_memif->data->enable_shift,
				   0);
		mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_DIRECT_SEND,
				     SPK_PROTECT_SPEECH_STOP, 1, 0, NULL);
	} else {
		mtk_scp_spk_ipi_send(AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_DIRECT_SEND,
				     SPK_PROTECT_STOP, 1, 0, NULL);
	}

	return 0;
}

static int mtk_scp_spk_pcm_hw_trigger(struct snd_pcm_substream *substream,
				      int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_scp_spk_pcm_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_scp_spk_pcm_stop(substream);
	}
	return -EINVAL;
}

static int mtk_scp_spk_pcm_copy(struct snd_pcm_substream *substream,
				int channel,
				snd_pcm_uframes_t pos, void __user *buf,
				snd_pcm_uframes_t count)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct RingBuf *ringbuf = &spk_mem->platform_ringbuf;
	unsigned int payload_len = 0;
	int ack_type = AUDIO_IPI_MSG_DIRECT_SEND;
	int availsize = 0, copy_size = 0, frame_cnt = 0;

	Ringbuf_Check(ringbuf);

	availsize = RingBuf_getFreeSpace(ringbuf);
	copy_size = scp_spk_word_size_align(count); // count: bytes
	frame_cnt = copy_size / snd_pcm_format_size(
				substream->runtime->format,
				substream->runtime->channels);
	scp_spk_debug("%s(), datacount:%d, copy_size:%d, count:%d, frame_cnt:%d\n",
		      __func__, ringbuf->datacount,
		      copy_size, count, frame_cnt);

	if (availsize >= copy_size) {
		RingBuf_copyFromUserLinear(ringbuf, buf, copy_size);
	} else {
		dev_info(scp_spk->dev,
			 "%s() fail copy_size = %d availsize = %d\n",
			 __func__,
			 copy_size, RingBuf_getFreeSpace(ringbuf));
	}
	Ringbuf_Check(ringbuf);

	payload_len = mtk_scp_spk_pack_payload(SPK_PROTECT_DLCOPY, pos,
					       frame_cnt, NULL, substream);
	if (substream->runtime->status->state != SNDRV_PCM_STATE_RUNNING)
		ack_type = AUDIO_IPI_MSG_NEED_ACK;

	mtk_scp_spk_ipi_send(AUDIO_IPI_PAYLOAD, ack_type,
			     SPK_PROTECT_DLCOPY, payload_len, 0,
			     (char *)spk_mem->ipi_payload_buf);

	return 0;
}

static int mtk_scp_spk_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct mtk_base_scp_spk *scp_spk =
			snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(scp_spk->dev, "%s()\n", __func__);

	snd_soc_add_platform_controls(rtd->platform,
				      scp_spk_platform_kcontrols,
				      ARRAY_SIZE(scp_spk_platform_kcontrols));

	ret = mtk_scp_spk_reserved_dram_init();
	if (ret < 0)
		return ret;

	audio_ipi_client_spkprotect_init();
	mtk_scp_spk_dump_msg(&scp_spk->spk_dump);
	ret = audio_task_register_callback(TASK_SCENE_SPEAKER_PROTECTION,
					   mtk_scp_spk_ipi_recv, NULL);

	if (ret < 0)
		return ret;

	return ret;
}

static const struct snd_pcm_ops mtk_scp_spk_pcm_ops = {
	.open = mtk_scp_spk_pcm_open,
	.close = mtk_scp_spk_pcm_close,
	.hw_params = mtk_scp_spk_pcm_hw_params,
	.prepare = mtk_scp_spk_pcm_hw_prepare,
	.trigger = mtk_scp_spk_pcm_hw_trigger,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = mtk_scp_spk_pcm_pointer,
	.copy_user = mtk_scp_spk_pcm_copy,
};

const struct snd_soc_platform_driver mtk_scp_spk_pcm_platform = {
	.ops = &mtk_scp_spk_pcm_ops,
	.pcm_new = mtk_scp_spk_pcm_new,
};
EXPORT_SYMBOL_GPL(mtk_scp_spk_pcm_platform);

MODULE_DESCRIPTION("Mediatek scp spk platform driver");
MODULE_AUTHOR("Shane Chien <Shane.Chien@mediatek.com>");
MODULE_LICENSE("GPL v2");


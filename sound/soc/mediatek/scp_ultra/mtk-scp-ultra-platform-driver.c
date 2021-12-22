/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-ultra-platform.c --  Mediatek scp ultra platform
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <asm/arch_timer.h>
#include <sound/soc.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_runtime.h>

#include "scp_helper.h"
#include "scp_excep.h"
#include "audio_ultra_msg_id.h"
#include "mtk-scp-ultra-mem-control.h"
#include "mtk-scp-ultra-platform-driver.h"
#include "mtk-base-scp-ultra.h"
#include "mtk-scp-ultra-common.h"
#include "mtk-base-afe.h"
#include "ultra_ipi.h"
#include "mtk-scp-ultra_dump.h"
#include "scp_feature_define.h"


//static DEFINE_SPINLOCK(scp_ultra_ringbuf_lock);

#define GET_SYSTEM_TIMER_CYCLE(void) \
	({ \
		unsigned long long __ret = arch_counter_get_cntvct(); \
		__ret; \
	})
#define ultra_IPIMSG_TIMEOUT (50)
#define ultra_WAITCHECK_INTERVAL_MS (2)
static bool ultra_ipi_wait;
static struct wakeup_source *ultra_suspend_lock;
static bool pcm_dump_switch;
static bool pcm_dump_on;
static const char *const mtk_scp_ultra_dump_str[] = {
	"Off",
	"On"};

static struct ultra_param_config param_config = {
	.rate_in = 48000,
	.rate_out = 48000,
	.channel_in = 2,
	.channel_out = 2,
	.format_in = HAL_FORMAT_S16_LE,
	.format_out = HAL_FORMAT_S16_LE,
	.period_in_size = DEFAULT_UL_PERIOD_SIZE,
	.period_out_size = DEFAULT_DL_PERIOD_SIZE,
	.target_out_channel = ULTRASOUND_TARGET_OUT_CHANNEL_LEFT
};

static struct ultra_gain_config gain_config = {
	.mic_gain = 0,
	.receiver_gain = 0
};

/*****************************************************************************
 * SCP Recovery Register
 *****************************************************************************/
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static int usnd_scp_recover_event(struct notifier_block *this,
				  unsigned long event,
				  void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY: {
		pr_info("%s(), SCP_EVENT_READY\n", __func__);
		ultra_SetScpRecoverStatus(false);
		break;
	}
	case SCP_EVENT_STOP:
		ultra_SetScpRecoverStatus(true);
		pr_info("%s(), SCP_EVENT_STOP\n", __func__);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block usnd_scp_recover_notifier = {
	.notifier_call = usnd_scp_recover_event,
};
#endif  /* #ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT */


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
	case AUDIO_TASK_USND_MSG_ID_OFF:
	case AUDIO_TASK_USND_MSG_ID_ON:
	case AUDIO_TASK_USND_MSG_ID_START:
	case AUDIO_TASK_USND_MSG_ID_STOP:
	case AUDIO_TASK_USND_MSG_ID_PCMDUMP_OK:
		result = true;
		break;
	default:
		pr_info("%s(), no relate msg id(%d)\r", __func__, msg_id);
		break;
	}
	return result;
}

static int mtk_scp_ultra_dump_get(struct snd_kcontrol *kcontrol,
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

static int mtk_scp_ultra_dump_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_scp_ultra *scp_ultra =
			snd_soc_component_get_drvdata(cmpnt);
	struct mtk_base_scp_ultra_dump *ultra_dump = &scp_ultra->ultra_dump;
	//struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = ultra_get_afe_base();
	static int ctrl_val;
	int timeout = 0;
	int payload[3];
	bool ret_val;

	dev_dbg(scp_ultra->dev, "%s(), value = %ld, dump_flag = %d\n",
				__func__,
				ucontrol->value.integer.value[0],
				ultra_dump->dump_flag);

	if (ultra_dump->dump_flag == false &&
		ucontrol->value.integer.value[0] > 0) {
		ctrl_val = ucontrol->value.integer.value[0];
		ultra_dump->dump_flag = true;
		pcm_dump_switch = true;

		payload[0] = ultra_dump->dump_resv_mem.size;
		payload[1] = ultra_dump->dump_resv_mem.phy_addr;
		payload[2] = ultra_dump->dump_flag;

		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_PCMDUMP_ON,
					 false,
					 3,
					 &payload[0],
					 ULTRA_IPI_BYPASS_ACK);
		ultra_ipi_wait = true;
	} else if (ultra_dump->dump_flag == true &&
		   ucontrol->value.integer.value[0] == 0) {
		ultra_dump->dump_flag = false;
		pcm_dump_switch = false;

		while (ultra_ipi_wait) {
			msleep(ultra_WAITCHECK_INTERVAL_MS);
			if (timeout++ >= ultra_IPIMSG_TIMEOUT)
				ultra_ipi_wait = false;
		}

		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_PCMDUMP_OFF,
					 false,
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
			    mtk_scp_ultra_dump_str)
};

static int mtk_scp_ultra_param_config_get(struct snd_kcontrol *kcontrol,
					  unsigned int __user *data,
					  unsigned int size)
{
	struct ultra_param_config *out_param_config = &param_config;

	if (copy_to_user(data, out_param_config,
			 sizeof(struct ultra_param_config))) {
		pr_info("%s() copy fail, data=%p, size=%d\n",
			__func__, data, size);
		return -EFAULT;
	}
	return 0;
}

static int mtk_scp_ultra_param_config_set(struct snd_kcontrol *kcontrol,
					  const unsigned int __user *data,
					  unsigned int size)
{
	if (copy_from_user(&param_config,
			   data,
			   sizeof(struct ultra_param_config))) {
		pr_info("%s() copy fail, data=%p, size=%d\n",
			__func__, data, size);
		return -EFAULT;
	}
	return 0;
}

static int mtk_scp_ultra_gain_config_get(struct snd_kcontrol *kcontrol,
					 unsigned int __user *data,
					 unsigned int size)
{
	struct ultra_gain_config *analog_gain_config = &gain_config;

	if (copy_to_user(data,
			 analog_gain_config,
			 sizeof(struct ultra_gain_config))) {
		pr_info("%s() copy fail, data=%p, size=%d\n",
			__func__, data, size);
		return -EFAULT;
	}
	return 0;
}

static int mtk_scp_ultra_gain_config_set(struct snd_kcontrol *kcontrol,
					 const unsigned int __user *data,
					 unsigned int size)
{
	int payload[2];

	if (copy_from_user(&gain_config,
			   data,
			   sizeof(struct ultra_gain_config))) {
		pr_info("%s() copy fail, data=%p, size=%d\n",
			__func__, data, size);
		return -EFAULT;
	}
	payload[0] = gain_config.mic_gain;
	payload[1] = gain_config.receiver_gain;
	pr_info("%s() mic_gain=%d, receiver_gain=%d\n",
		__func__, gain_config.mic_gain, gain_config.receiver_gain);
	ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_ANALOG_GAIN,
		       false,
		       2,
		       &payload[0],
		       ULTRA_IPI_BYPASS_ACK);
	return 0;
}


static int mtk_scp_ultra_engine_state_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();

	ucontrol->value.integer.value[0] = scp_ultra->usnd_state;
	return 0;
}

static int mtk_scp_ultra_engine_state_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_afe *afe = ultra_get_afe_base();
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	int scp_ultra_memif_dl_id;
	int scp_ultra_memif_ul_id;
	int val = ucontrol->value.integer.value[0];
	int payload[7];
	int old_usnd_state = scp_ultra->usnd_state;
	bool ret_val = false;

	if (val < SCP_ULTRA_STATE_IDLE || val > SCP_ULTRA_STATE_RECOVERY) {
		pr_info("%s() unexpected state, ignore\n", __func__);
		return -1;
	}
	scp_ultra->usnd_state = val;

	scp_ultra_memif_dl_id =
		scp_ultra->scp_ultra_dl_memif_id;
	scp_ultra_memif_ul_id =
		scp_ultra->scp_ultra_ul_memif_id;
	ultra_mem->ultra_dl_memif_id = scp_ultra_memif_dl_id;
	ultra_mem->ultra_ul_memif_id = scp_ultra_memif_ul_id;
	pr_info("%s() new state=%d, memdl=%d, memul=%d\n",
		__func__, scp_ultra->usnd_state,
		scp_ultra_memif_dl_id,
		scp_ultra_memif_ul_id);
	switch (scp_ultra->usnd_state) {
	case SCP_ULTRA_STATE_ON:
		scp_register_feature(ULTRA_FEATURE_ID);
		aud_wake_lock(ultra_suspend_lock);

		afe->memif[scp_ultra_memif_dl_id].scp_ultra_enable = true;
		afe->memif[scp_ultra_memif_ul_id].scp_ultra_enable = true;

		payload[0] = param_config.rate_out;
		payload[1] = param_config.channel_out;
		payload[2] = param_config.period_out_size;
		payload[3] = param_config.rate_in;
		payload[4] = param_config.channel_in;
		payload[5] = param_config.period_in_size;
		payload[6] = param_config.target_out_channel;
		ret_val = ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_ON,
					false,
					7,
					&payload[0],
					ULTRA_IPI_NEED_ACK);
		if (ret_val == 0) {
			pr_info("%s() set state on failed\n", __func__);
			scp_ultra->usnd_state = SCP_ULTRA_STATE_IDLE;
			afe->memif[scp_ultra_memif_dl_id].scp_ultra_enable =
				false;
			afe->memif[scp_ultra_memif_ul_id].scp_ultra_enable =
				false;
			scp_deregister_feature(ULTRA_FEATURE_ID);
			aud_wake_unlock(ultra_suspend_lock);
			return -1;
		}
		return 0;

	case SCP_ULTRA_STATE_OFF:
		ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_OFF,
			       false,
			       0,
			       NULL,
			       ULTRA_IPI_NEED_ACK);
		scp_deregister_feature(ULTRA_FEATURE_ID);
		aud_wake_unlock(ultra_suspend_lock);
		return 0;
	case SCP_ULTRA_STATE_START:
		ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_START,
			       false,
			       0,
			       NULL,
			       ULTRA_IPI_NEED_ACK);
		return 0;
	case SCP_ULTRA_STATE_STOP:
		ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_STOP,
			       false,
			       0,
			       NULL,
			       ULTRA_IPI_NEED_ACK);
		return 0;
	case SCP_ULTRA_STATE_RECOVERY:
		if (old_usnd_state == SCP_ULTRA_STATE_OFF ||
		    old_usnd_state == SCP_ULTRA_STATE_IDLE ||
		    old_usnd_state == SCP_ULTRA_STATE_RECOVERY)
			return 0;
		pm_runtime_get_sync(afe->dev);
		if (old_usnd_state == SCP_ULTRA_STATE_START)
			ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_STOP,
				       false,
				       0,
				       NULL,
				       ULTRA_IPI_NEED_ACK);
		ultra_ipi_send(AUDIO_TASK_USND_MSG_ID_OFF,
			       false,
			       0,
			       NULL,
			       ULTRA_IPI_NEED_ACK);
		scp_deregister_feature(ULTRA_FEATURE_ID);
		pm_runtime_put(afe->dev);
		aud_wake_unlock(ultra_suspend_lock);
		return 0;
	default:
		pr_info("%s() err state, ignore\n", __func__);
		return -1;

	}
	return 0;
}


static const struct snd_kcontrol_new ultra_platform_kcontrols[] = {
	SOC_ENUM_EXT("mtk_scp_ultra_pcm_dump",
		     mtk_scp_ultra_enum[0],
		     mtk_scp_ultra_dump_get,
		     mtk_scp_ultra_dump_set),
	SND_SOC_BYTES_TLV("mtk_scp_ultra_param_config",
			  sizeof(struct ultra_param_config),
			  mtk_scp_ultra_param_config_get,
			  mtk_scp_ultra_param_config_set),
	SND_SOC_BYTES_TLV("mtk_scp_ultra_gain_config",
			  sizeof(struct ultra_gain_config),
			  mtk_scp_ultra_gain_config_get,
			  mtk_scp_ultra_gain_config_set),
	SOC_SINGLE_EXT("mtk_scp_ultra_engine_state",
		     SND_SOC_NOPM, 0, 0xff, 0,
		     mtk_scp_ultra_engine_state_get,
		     mtk_scp_ultra_engine_state_set),
};

static int mtk_scp_ultra_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, ULTRA_PCM_NAME);
	struct mtk_base_scp_ultra *scp_ultra =
		snd_soc_component_get_drvdata(component);
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = ultra_get_afe_base();
	int scp_ultra_memif_dl_id =
		scp_ultra->scp_ultra_dl_memif_id;
	int scp_ultra_memif_ul_id =
		scp_ultra->scp_ultra_ul_memif_id;

	ultra_mem->ultra_dl_memif_id = scp_ultra_memif_dl_id;
	ultra_mem->ultra_ul_memif_id = scp_ultra_memif_ul_id;

	memcpy((void *)(&(runtime->hw)), (void *)scp_ultra->mtk_scp_hardware,
			sizeof(struct snd_pcm_hardware));

	if (scp_ultra->ultra_mem.ultra_dl_memif_id < 0) {
		dev_info(scp_ultra->dev, "%s() ultra_ul_memif_id<0, return\n",
			__func__);
		return 0;
	}

	dev_info(scp_ultra->dev, "%s() memif dl=%d,ul=%d\n",
		__func__,
		scp_ultra->ultra_mem.ultra_dl_memif_id,
		scp_ultra->ultra_mem.ultra_ul_memif_id);

	if (pcm_dump_switch) {
		/* scp ultra dump buffer use dram */
		if (afe->request_dram_resource)
			afe->request_dram_resource(afe->dev);
		ultra_start_engine_thread();
		pcm_dump_on = true;
	}

	return 0;
}
static int mtk_scp_ultra_pcm_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, ULTRA_PCM_NAME);
	struct mtk_base_scp_ultra *scp_ultra =
		snd_soc_component_get_drvdata(component);
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = ultra_get_afe_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[ultra_mem->ultra_dl_memif_id];
	struct mtk_base_afe_memif *memiful =
		&afe->memif[ultra_mem->ultra_ul_memif_id];
	int irq_id_dl = memif->irq_usage;
	struct mtk_base_afe_irq *irqs_dl = &afe->irqs[irq_id_dl];
	const struct mtk_base_irq_data *irq_data_dl = irqs_dl->irq_data;
	int irq_id_ul = memiful->irq_usage;
	struct mtk_base_afe_irq *irqs_ul = &afe->irqs[irq_id_ul];
	const struct mtk_base_irq_data *irq_data_ul = irqs_ul->irq_data;
	int counter;

	/* Set dl&ul irq target to scp */
	set_afe_dl_irq_target(true);
	set_afe_ul_irq_target(true);

	/* set dl irq counter */
	counter = param_config.period_out_size;
	regmap_update_bits(afe->regmap, irq_data_dl->irq_cnt_reg,
			irq_data_dl->irq_cnt_maskbit
			<< irq_data_dl->irq_cnt_shift,
			counter << irq_data_dl->irq_cnt_shift);

	/* start dl memif */
	regmap_update_bits(afe->regmap,
			memif->data->enable_reg,
			1 << memif->data->enable_shift,
			1 << memif->data->enable_shift);

	/* set ul irq counter */
	counter = param_config.period_in_size;
	regmap_update_bits(afe->regmap, irq_data_ul->irq_cnt_reg,
			   irq_data_ul->irq_cnt_maskbit
			   << irq_data_ul->irq_cnt_shift,
			   counter << irq_data_ul->irq_cnt_shift);

	/* Start ul memif */
	regmap_update_bits(afe->regmap,
			   memiful->data->enable_reg,
			   1 << memiful->data->enable_shift,
			   1 << memiful->data->enable_shift);

	/* start ul irq */
	regmap_update_bits(afe->regmap, irq_data_ul->irq_en_reg,
			1 << irq_data_ul->irq_en_shift,
			1 << irq_data_ul->irq_en_shift);
	return 0;
}

static int mtk_scp_ultra_pcm_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, ULTRA_PCM_NAME);
	struct mtk_base_scp_ultra *scp_ultra =
		snd_soc_component_get_drvdata(component);
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	struct mtk_base_afe *afe = ultra_get_afe_base();
	struct mtk_base_afe_memif *memif =
		&afe->memif[ultra_mem->ultra_dl_memif_id];
	struct mtk_base_afe_memif *memiful =
		&afe->memif[ultra_mem->ultra_ul_memif_id];
	int irq_id_dl = memif->irq_usage;
	struct mtk_base_afe_irq *irqs_dl = &afe->irqs[irq_id_dl];
	const struct mtk_base_irq_data *irq_data_dl = irqs_dl->irq_data;
	int irq_id_ul = memiful->irq_usage;
	struct mtk_base_afe_irq *irqs_ul = &afe->irqs[irq_id_ul];
	const struct mtk_base_irq_data *irq_data_ul = irqs_ul->irq_data;

	dev_info(scp_ultra->dev, "%s() dl_if=%d,ul_if=%d\n",
		 __func__,
		 ultra_mem->ultra_dl_memif_id,
		 ultra_mem->ultra_ul_memif_id);

	if (scp_ultra->usnd_state == SCP_ULTRA_STATE_OFF) {
		dev_info(scp_ultra->dev, "%s() ignore, state is off\n",
			 __func__);
		return 0;
	}

	/* stop dl memif */
	regmap_update_bits(afe->regmap,
			   memif->data->enable_reg,
			   1 << memif->data->enable_shift,
			   0);
	/* stop ul memif */
	regmap_update_bits(afe->regmap,
			   memiful->data->enable_reg,
			   1 << memiful->data->enable_shift,
			   0);
	/* stop dl irq */
	regmap_update_bits(afe->regmap,
			   irq_data_dl->irq_en_reg,
			   1 << irq_data_dl->irq_en_shift,
			   0);

	/* stop ul irq */
	regmap_update_bits(afe->regmap, irq_data_ul->irq_en_reg,
			   1 << irq_data_ul->irq_en_shift,
			   0);

	/* clear pending dl irq */
	regmap_write(afe->regmap, irq_data_dl->irq_clr_reg,
		     1 << irq_data_dl->irq_clr_shift);

	/* clear pending ul irq */
	regmap_write(afe->regmap, irq_data_ul->irq_clr_reg,
		     1 << irq_data_ul->irq_clr_shift);

	/* Set dl&ul irq to ap */
	set_afe_dl_irq_target(false);
	set_afe_ul_irq_target(false);
	return 0;
}
static int mtk_scp_ultra_pcm_close(struct snd_pcm_substream *substream)
{
	struct mtk_base_afe *afe = ultra_get_afe_base();
	if (pcm_dump_on) {
		/* scp ultra dump buffer use dram */
		if (afe->release_dram_resource)
			afe->release_dram_resource(afe->dev);
		ultra_stop_engine_thread();
		pcm_dump_on = false;
	}

	return 0;
}

static void mtk_scp_ultra_pcm_hw_params_dl(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params)
{
	substream->runtime->dma_bytes = params_buffer_bytes(params);
}

static int mtk_scp_ultra_pcm_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		mtk_scp_ultra_pcm_hw_params_dl(substream, params);
	return 0;
}


static int mtk_scp_ultra_pcm_hw_prepare(struct snd_pcm_substream *substream)
{
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
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, ULTRA_PCM_NAME);
	struct mtk_base_scp_ultra *scp_ultra =
		snd_soc_component_get_drvdata(component);

	dev_info(scp_ultra->dev, "%s()\n", __func__);

	snd_soc_add_component_controls(component,
				      ultra_platform_kcontrols,
				      ARRAY_SIZE(ultra_platform_kcontrols));

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	scp_A_register_notify(&usnd_scp_recover_notifier);
#endif
	ultra_suspend_lock = aud_wake_lock_init(NULL, "ultra wakelock");
	pcm_dump_switch = false;
	scp_ultra->usnd_state = SCP_ULTRA_STATE_IDLE;
	return 0;
}

static const struct snd_pcm_ops mtk_scp_ultra_pcm_ops = {
	.open = mtk_scp_ultra_pcm_open,
	.close = mtk_scp_ultra_pcm_close,
	.hw_params = mtk_scp_ultra_pcm_hw_params,
	.prepare = mtk_scp_ultra_pcm_hw_prepare,
	.trigger = mtk_scp_ultra_pcm_hw_trigger,
	.ioctl = snd_pcm_lib_ioctl,
};

const struct snd_soc_component_driver mtk_scp_ultra_pcm_platform = {
	.name = ULTRA_PCM_NAME,
	.ops = &mtk_scp_ultra_pcm_ops,
	.pcm_new = mtk_scp_ultra_pcm_new,
};
EXPORT_SYMBOL_GPL(mtk_scp_ultra_pcm_platform);

static int ultra_ipi_init(void)
{
	ultra_ipi_register(ultra_ipi_rx_internal, ultra_ipi_rceive_ack);
	audio_ipi_client_ultra_init();
	return 0;
}
late_initcall(ultra_ipi_init);

MODULE_DESCRIPTION("Mediatek scp ultra platform driver");
MODULE_AUTHOR("Youwei Dong <Youwei.Dong@mediatek.com>");
MODULE_LICENSE("GPL v2");


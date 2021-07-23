// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <audio_task_manager.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/mutex.h>

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#include <audio_ipi_platform.h>
#include <audio_messenger_ipi.h>
#else
#include <scp_helper.h>
#endif

#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"
#include "mtk-dsp-platform-driver.h"
#include "mtk-base-afe.h"

static DEFINE_SPINLOCK(dsp_ringbuf_lock);
static DEFINE_MUTEX(adsp_wakelock_lock);

#define IPIMSG_SHARE_MEM (1024)
#define DSP_IRQ_LOOP_COUNT (3)
static int adsp_wakelock_count;
static struct wakeup_source adsp_audio_wakelock;
static int ktv_status;

//#define DEBUG_VERBOSE
//#define DEBUG_VERBOSE_IRQ

static int dsp_primary_default_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PRIMARY_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_primary_default_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PRIMARY_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_deepbuf_default_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_DEEPBUFFER_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_deepbuf_default_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_DEEPBUFFER_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}


static int dsp_voipdl_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_VOIP_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_voipdl_default_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_VOIP_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}


static int dsp_playback_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_playback_default_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_captureul1_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_UL1_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_captureul1_default_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_offload_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_OFFLOAD_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_offload_default_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_OFFLOAD_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_a2dp_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_a2dp_default_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_A2DP_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_dataprovider_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_DATAPROVIDER_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_dataprovider_default_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_DATAPROVIDER_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_call_final_default_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CALL_FINAL_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_call_final_default_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CALL_FINAL_ID,
			      ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_fast_default_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_FAST_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_fast_default_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_FAST_ID, ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_ktv_default_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_KTV_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_ktv_default_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_KTV_ID, ADSP_TASK_ATTR_DEFAULT);
	return 0;
}

static int dsp_capture_raw_default_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_RAW_ID, ADSP_TASK_ATTR_DEFAULT, val);
	return 0;
}

static int dsp_capture_raw_default_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int val = get_task_attr(AUDIO_TASK_CAPTURE_RAW_ID,
				ADSP_TASK_ATTR_DEFAULT);
	if (val > 0)
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int dsp_primary_runtime_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PRIMARY_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}


static int dsp_primary_runtime_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PRIMARY_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_deepbuf_runtime_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_DEEPBUFFER_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_deepbuf_runtime_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_DEEPBUFFER_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}


static int dsp_voipdl_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_VOIP_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_voipdl_runtime_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_VOIP_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_playback_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_playback_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_music_runtime_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_MUSIC_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_music_runtime_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_MUSIC_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}


static int dsp_captureul1_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_UL1_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_captureul1_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}


static int dsp_offload_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_OFFLOAD_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_offload_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_OFFLOAD_ID, ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_a2dp_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_a2dp_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_dataprovider_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_DATAPROVIDER_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_dataprovider_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_DATAPROVIDER_ID,
				ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_fast_runtime_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_FAST_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_fast_runtime_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_FAST_ID, ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_ktv_runtime_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_KTV_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_ktv_runtime_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_KTV_ID, ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int dsp_capture_raw_runtime_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_RAW_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_capture_raw_runtime_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int val = get_task_attr(AUDIO_TASK_CAPTURE_RAW_ID,
				ADSP_TASK_ATTR_RUMTIME);
	if (val > 0)
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int dsp_captureul1_ref_runtime_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_UL1_ID, ADSP_TASK_ATTR_REF_RUNTIME, val);
	return 0;
}

static int dsp_captureul1_ref_runtime_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CAPTURE_UL1_ID, ADSP_TASK_ATTR_REF_RUNTIME);
	return 0;
}

static int dsp_playback_ref_runtime_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_REF_RUNTIME, val);
	return 0;
}

static int dsp_playback_ref_runtime_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_REF_RUNTIME);
	return 0;
}

static int dsp_call_final_ref_runtime_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CALL_FINAL_ID, ADSP_TASK_ATTR_REF_RUNTIME, val);
	return 0;
}

static int dsp_call_final_ref_runtime_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CALL_FINAL_ID, ADSP_TASK_ATTR_REF_RUNTIME);
	return 0;
}

static int dsp_wakelock_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	mutex_lock(&adsp_wakelock_lock);
	if (val) {
		adsp_wakelock_count++;
		if (adsp_wakelock_count == 1)
			aud_wake_lock(&adsp_audio_wakelock);
	} else {
		adsp_wakelock_count--;
		if (adsp_wakelock_count == 0)
			aud_wake_unlock(&adsp_audio_wakelock);
		if (adsp_wakelock_count < 0) {
			pr_info("%s not paired", __func__);
			adsp_wakelock_count = 0;
		}
	}
	mutex_unlock(&adsp_wakelock_lock);
	return 0;
}

static int dsp_wakelock_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = adsp_wakelock_count;
	return 0;
}

static int audio_dsp_version_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(cmpnt);

	if (!dsp)
		return -1;

	dsp->dsp_ver = ucontrol->value.integer.value[0];
	return 0;
}

static int audio_dsp_version_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(cmpnt);

	if (!dsp)
		return -1;

	ucontrol->value.integer.value[0] = dsp->dsp_ver;
	return 0;
}

static int dsp_call_final_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CALL_FINAL_ID, ADSP_TASK_ATTR_RUMTIME, val);
	return 0;
}

static int dsp_call_final_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CALL_FINAL_ID,
			      ADSP_TASK_ATTR_RUMTIME);
	return 0;
}

static int smartpa_swdsp_process_enable_set(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_SMARTPA, val);
	return 0;
}

static int smartpa_swdsp_process_enable_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			      ADSP_TASK_ATTR_SMARTPA);

	return 0;
}

static int ktv_status_set(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ktv_status = ucontrol->value.integer.value[0];
	pr_debug("%s() ktv_status = %d\n", __func__, ktv_status);
	return 0;
}

static int ktv_status_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ktv_status;
	pr_debug("%s() ktv_status = %ld\n", __func__, ktv_status);
	return 0;
}

static const struct snd_kcontrol_new dsp_platform_kcontrols[] = {
	SOC_SINGLE_EXT("dsp_primary_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_primary_default_get, dsp_primary_default_set),
	SOC_SINGLE_EXT("dsp_deepbuf_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_deepbuf_default_get, dsp_deepbuf_default_set),
	SOC_SINGLE_EXT("dsp_voipdl_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_voipdl_default_get, dsp_voipdl_default_set),
	SOC_SINGLE_EXT("dsp_playback_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_playback_default_get, dsp_playback_default_set),
	SOC_SINGLE_EXT("dsp_captureul1_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_captureul1_default_get, dsp_captureul1_default_set),
	SOC_SINGLE_EXT("dsp_offload_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_offload_default_get, dsp_offload_default_set),
	SOC_SINGLE_EXT("dsp_a2dp_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_a2dp_default_get, dsp_a2dp_default_set),
	SOC_SINGLE_EXT("dsp_dataprovider_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_dataprovider_default_get,
		       dsp_dataprovider_default_set),
	SOC_SINGLE_EXT("dsp_call_final_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_call_final_default_get, dsp_call_final_default_set),
	SOC_SINGLE_EXT("dsp_fast_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_fast_default_get, dsp_fast_default_set),
	SOC_SINGLE_EXT("dsp_ktv_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_ktv_default_get, dsp_ktv_default_set),
	SOC_SINGLE_EXT("dsp_captureraw_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_capture_raw_default_get,
		       dsp_capture_raw_default_set),
	SOC_SINGLE_EXT("dsp_primary_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_primary_runtime_get, dsp_primary_runtime_set),
	SOC_SINGLE_EXT("dsp_deepbuf_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_deepbuf_runtime_get, dsp_deepbuf_runtime_set),
	SOC_SINGLE_EXT("dsp_voipdl_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_voipdl_runtime_get, dsp_voipdl_runtime_set),
	SOC_SINGLE_EXT("dsp_playback_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_playback_runtime_get, dsp_playback_runtime_set),
	SOC_SINGLE_EXT("dsp_music_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_music_runtime_get, dsp_music_runtime_set),
	SOC_SINGLE_EXT("dsp_captureul1_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_captureul1_runtime_get, dsp_captureul1_runtime_set),
	SOC_SINGLE_EXT("dsp_offload_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_offload_runtime_get, dsp_offload_runtime_set),
	SOC_SINGLE_EXT("dsp_a2dp_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_a2dp_runtime_get, dsp_a2dp_runtime_set),
	SOC_SINGLE_EXT("dsp_dataprovider_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_dataprovider_runtime_get,
		       dsp_dataprovider_runtime_set),
	SOC_SINGLE_EXT("dsp_fast_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_fast_runtime_get, dsp_fast_runtime_set),
	SOC_SINGLE_EXT("dsp_ktv_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_ktv_runtime_get, dsp_ktv_runtime_set),
	SOC_SINGLE_EXT("dsp_captureraw_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_capture_raw_runtime_get,
		       dsp_capture_raw_runtime_set),
	SOC_SINGLE_EXT("audio_dsp_wakelock", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_wakelock_get, dsp_wakelock_set),
	SOC_SINGLE_EXT("dsp_call_final_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_call_final_runtime_get, dsp_call_final_runtime_set),
	SOC_SINGLE_EXT("dsp_captureul1_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_captureul1_ref_runtime_get, dsp_captureul1_ref_runtime_set),
	SOC_SINGLE_EXT("dsp_playback_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_playback_ref_runtime_get, dsp_playback_ref_runtime_set),
	SOC_SINGLE_EXT("dsp_call_final_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_call_final_ref_runtime_get, dsp_call_final_ref_runtime_set),
	SOC_SINGLE_EXT("audio_dsp_version", SND_SOC_NOPM, 0, 0xff, 0,
		       audio_dsp_version_get, audio_dsp_version_set),
	SOC_SINGLE_EXT("swdsp_smartpa_process_enable", SND_SOC_NOPM, 0, 0xff, 0,
		       smartpa_swdsp_process_enable_get,
		       smartpa_swdsp_process_enable_set),
	SOC_SINGLE_EXT("ktv_status", SND_SOC_NOPM, 0, 0x1, 0,
		       ktv_status_get,
		       ktv_status_set),
};

static unsigned int dsp_word_size_align(unsigned int in_size)
{
	unsigned int align_size;

	align_size = in_size & 0xFFFFFF80;
	return align_size;
}

static int afe_remap_dsp_pointer
		(struct buf_attr dst, struct buf_attr src, int bytes)
{
	int retval = bytes;

	retval = (retval * snd_pcm_format_physical_width(src.format))
		  / snd_pcm_format_physical_width(dst.format);
	retval = (retval * src.rate) / dst.rate;
	retval = (retval * src.channel) / dst.channel;
	return retval;
}

static snd_pcm_uframes_t mtk_dsphw_pcm_pointer_ul
			 (struct snd_pcm_substream *substream)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	int ptr_bytes;

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
#endif

	ptr_bytes = dsp_mem->ring_buf.pWrite - dsp_mem->ring_buf.pBufBase;

	return bytes_to_frames(substream->runtime, ptr_bytes);
}

static snd_pcm_uframes_t mtk_dsphw_pcm_pointer_dl
			 (struct snd_pcm_substream *substream)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	/* afedl id is get from dts */
	int afedlid = get_afememdl_by_afe_taskid(id);
	unsigned long flags;
	unsigned int hw_ptr = 0, hw_base = 0;
	int pcm_ptr_bytes, ret, pcm_remap_ptr_bytes;
	struct mtk_base_afe *afe = get_afe_base();
	struct device *dev = afe->dev;
	struct regmap *regmap = afe->regmap;
	struct mtk_base_afe_memif *memif;
	const struct mtk_base_memif_data *memif_data;
	int reg_ofs_base;
	int reg_ofs_cur;

	if (dsp->dsp_ver)
		goto SYNC_READINDEX;


	if (afedlid >= 0) {
		memif = &afe->memif[afedlid];
		memif_data = memif->data;
		reg_ofs_base = memif_data->reg_ofs_base;
		reg_ofs_cur = memif_data->reg_ofs_cur;
	}

	if (afedlid < 0) {
		pr_info("%s SYNC_READINDEX\n", __func__);
		goto SYNC_READINDEX;
	}

	ret = regmap_read(regmap, reg_ofs_cur, &hw_ptr);
	if (ret || hw_ptr == 0) {
		dev_err(dev, "1 %s hw_ptr err\n", __func__);
		pcm_ptr_bytes = 0;
		pcm_remap_ptr_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	ret = regmap_read(regmap, reg_ofs_base, &hw_base);
	if (ret || hw_base == 0) {
		dev_err(dev, "2 %s hw_ptr err\n", __func__);
		pcm_ptr_bytes = 0;
		pcm_remap_ptr_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	pcm_ptr_bytes = hw_ptr - hw_base;
	pcm_remap_ptr_bytes =
			afe_remap_dsp_pointer(
			dsp_mem->audio_afepcm_buf.aud_buffer.buffer_attr,
			dsp_mem->adsp_buf.aud_buffer.buffer_attr,
			pcm_ptr_bytes);
	pcm_remap_ptr_bytes = dsp_word_size_align(pcm_remap_ptr_bytes);
	if (pcm_remap_ptr_bytes >=
	    dsp_mem->adsp_buf.aud_buffer.buf_bridge.bufLen)
		pr_info("%s pcm_remap_ptr_bytes = %d",
			__func__,
			pcm_remap_ptr_bytes);
	else
		dsp_mem->adsp_buf.aud_buffer.buf_bridge.pRead =
			(dsp_mem->adsp_buf.aud_buffer.buf_bridge.pBufBase +
			 pcm_remap_ptr_bytes);

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s("1 mtk_dsphw_pcm_pointer_dl",
				&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_dsphw_pcm_pointer_dl",
				&dsp_mem->ring_buf);
#endif
	ret = sync_ringbuf_readidx(
		&dsp_mem->ring_buf,
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	if (ret) {
		pr_info("%s sync_ringbuf_readidx underflow\n", __func__);
		return -1;
	}

#ifdef DEBUG_VERBOSE
	pr_info("%s id = %d reg_ofs_base = %d reg_ofs_cur = %d pcm_ptr_bytes = %d pcm_remap_ptr_bytes = %d\n",
		 __func__, id, reg_ofs_base, reg_ofs_cur,
		 pcm_ptr_bytes, pcm_remap_ptr_bytes);
#endif

POINTER_RETURN_FRAMES:
	return bytes_to_frames(substream->runtime, pcm_remap_ptr_bytes);


SYNC_READINDEX:

#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s("SYNC_READINDEX mtk_dsp_dl_handler",
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("SYNC_READINDEX mtk_dsp_dl_handler",
		&dsp_mem->ring_buf);
#endif
	spin_lock_irqsave(&dsp_ringbuf_lock, flags);

	/* handle for underflow */
	if (dsp_mem->underflowed) {
		pr_info("%s id = %d return -1 because underflowed[%d] %d\n",
			__func__, id, dsp_mem->underflowed);
		dsp_mem->underflowed = 0;
		spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);
		return -1;
	}

	pcm_ptr_bytes = (int)(dsp_mem->ring_buf.pRead -
			      dsp_mem->ring_buf.pBufBase);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);
	pcm_remap_ptr_bytes =
		bytes_to_frames(substream->runtime, pcm_ptr_bytes);
#ifdef DEBUG_VERBOSE
	pr_info("%s id = %d pcm_ptr_bytes = %d pcm_remap_ptr_bytes = %d\n",
		 __func__, id, pcm_ptr_bytes, pcm_remap_ptr_bytes);
#endif
	return pcm_remap_ptr_bytes;

}

static snd_pcm_uframes_t mtk_dsphw_pcm_pointer
			 (struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return mtk_dsphw_pcm_pointer_dl(substream);
	else
		return mtk_dsphw_pcm_pointer_ul(substream);

}

static void mtk_dsp_dl_handler(struct mtk_base_dsp *dsp,
			       struct ipi_msg_t *ipi_msg, int id)
{
	if (dsp->dsp_mem[id].substream == NULL) {
		pr_info("%s = substream == NULL\n", __func__);
		goto DSP_IRQ_HANDLER_ERR;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
DSP_IRQ_HANDLER_ERR:
	return;
}

static void mtk_dsp_dl_consume_handler(struct mtk_base_dsp *dsp,
			       struct ipi_msg_t *ipi_msg, int id)
{
	unsigned long flags;
	void *ipi_audio_buf;

	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];

	if (!dsp->dsp_mem[id].substream) {
		pr_info_ratelimited("%s substream NULL id[%d]\n", __func__, id);
		return;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info_ratelimited("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		return;
	}

	/* adsp reset message */
	if (ipi_msg && ipi_msg->param2 == ADSP_DL_CONSUME_RESET) {
		pr_info("%s adsp resert id = %d\n", __func__, id);
		RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);
		/* notify subsream */
		return snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
	}

	/* adsp reset message */
	if (ipi_msg && ipi_msg->param2 == ADSP_DL_CONSUME_UNDERFLOW) {
		pr_info("%s adsp underflowed id = %d\n", __func__, id);
		dsp->dsp_mem[id].underflowed = true;
		/* notify subsream */
		return snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
	}

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	/* upadte for write index*/
	ipi_audio_buf = (void *)dsp_mem->msg_dtoa_share_buf.va_addr;

	memcpy((void *)&dsp_mem->adsp_work_buf, (void *)ipi_audio_buf,
	       sizeof(struct audio_hw_buffer));

	dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge.pRead =
	    dsp->dsp_mem[id].adsp_work_buf.aud_buffer.buf_bridge.pRead;

#ifdef DEBUG_VERBOSE_IRQ
	dump_rbuf_s("dl_consume before sync", &dsp->dsp_mem[id].ring_buf);
#endif

	sync_ringbuf_readidx(
		&dsp->dsp_mem[id].ring_buf,
		&dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge);

	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE_IRQ
	pr_info("%s id = %d\n", __func__, id);
	dump_rbuf_s("dl_consume", &dsp->dsp_mem[id].ring_buf);
#endif
	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
}

static void mtk_dsp_ul_handler(struct mtk_base_dsp *dsp,
			       struct ipi_msg_t *ipi_msg, int id)
{
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	void *ipi_audio_buf;
	unsigned long flags;

	if (!dsp->dsp_mem[id].substream) {
		pr_info("%s substream NULL\n", __func__);
		return;
	}


	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	/* upadte for write index*/
	ipi_audio_buf = (void *)dsp_mem->msg_dtoa_share_buf.va_addr;

	memcpy((void *)&dsp_mem->adsp_work_buf, (void *)ipi_audio_buf,
	       sizeof(struct audio_hw_buffer));

	dsp_mem->adsp_buf.aud_buffer.buf_bridge.pWrite =
		(dsp_mem->adsp_work_buf.aud_buffer.buf_bridge.pWrite);
#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_work_buf.aud_buffer.buf_bridge);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	sync_ringbuf_writeidx(&dsp_mem->ring_buf,
			      &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
#endif

	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
DSP_IRQ_HANDLER_ERR:
	return;
}


void mtk_dsp_handler(struct mtk_base_dsp *dsp,
		     struct ipi_msg_t *ipi_msg)

{
	int id = 0;

	if (!dsp) {
		pr_info("%s dsp NULL", __func__);
		return;
	}

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	id = get_dspdaiid_by_dspscene(ipi_msg->task_scene);

	if (id < 0)
		return;

	if (!is_audio_task_dsp_ready(ipi_msg->task_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return;
	}

	switch (ipi_msg->msg_id) {
	case AUDIO_DSP_TASK_IRQDL:
		mtk_dsp_dl_handler(dsp, ipi_msg, id);
		break;
	case AUDIO_DSP_TASK_IRQUL:
		mtk_dsp_ul_handler(dsp, ipi_msg, id);
		break;
	case AUDIO_DSP_TASK_DL_CONSUME_DATA:
		mtk_dsp_dl_consume_handler(dsp, ipi_msg, id);
	default:
		break;
	}
}

static int mtk_dsp_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	int dsp_feature_id = get_featureid_by_dsp_daiid(id);

	pr_info("%s(), task_id: %d\n", __func__, id);

	memcpy((void *)(&(runtime->hw)), (void *)dsp->mtk_dsp_hardware,
	       sizeof(struct snd_pcm_hardware));


	ret = mtk_dsp_register_feature(dsp_feature_id);
	if (ret) {
		pr_info("%s register feature fail", __func__);
		return -1;
	}

	/* send to task with open information */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_OPEN, 0, 0,
			 NULL);

	dsp->dsp_mem[id].substream = substream;

	return 0;
}

static int mtk_dsp_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	int dsp_feature_id = get_featureid_by_dsp_daiid(id);

	pr_info("%s(), task_id: %d\n", __func__, id);

	/* send to task with close information */
	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_CLOSE, 0, 0,
			 NULL);

	if (ret)
		pr_info("%s ret[%d]\n", __func__, ret);

	mtk_dsp_deregister_feature(dsp_feature_id);

	dsp->dsp_mem[id].substream = NULL;

	return ret;
}

static int mtk_dsp_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	void *ipi_audio_buf; /* dsp <-> audio data struct*/
	int ret = 0;
	struct mtk_base_dsp_mem *dsp_memif = &dsp->dsp_mem[id];

	pr_info("%s(), task_id: %d\n", __func__, id);

	reset_audiobuffer_hw(&dsp->dsp_mem[id].adsp_buf);
	reset_audiobuffer_hw(&dsp->dsp_mem[id].audio_afepcm_buf);
	reset_audiobuffer_hw(&dsp->dsp_mem[id].adsp_work_buf);
	RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);

	dsp->request_dram_resource(dsp->dev);

	/* gen pool related */
	dsp->dsp_mem[id].gen_pool_buffer =
		mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	if (dsp->dsp_mem[id].gen_pool_buffer != NULL) {
		/* if already allocate , free it.*/
		if (substream->dma_buffer.area) {
			ret = mtk_adsp_genpool_free_sharemem_ring
						(&dsp->dsp_mem[id], id);
			if (!ret)
				release_snd_dmabuffer(&substream->dma_buffer);
		}
		if (ret < 0) {
			pr_warn("%s err\n", __func__);
			return -1;
		}

		/* allocate ring buffer wioth share memory */
		ret = mtk_adsp_genpool_allocate_sharemem_ring(
			&dsp->dsp_mem[id], params_buffer_bytes(params), id);

		if (ret < 0) {
			pr_warn("%s err\n", __func__);
			return -1;
		}

	}

#ifdef DEBUG_VERBOSE
	dump_audio_dsp_dram(&dsp->dsp_mem[id].msg_atod_share_buf);
	dump_audio_dsp_dram(&dsp->dsp_mem[id].msg_dtoa_share_buf);
	dump_audio_dsp_dram(&dsp->dsp_mem[id].dsp_ring_share_buf);
#endif
	ret = dsp_dram_to_snd_dmabuffer(&dsp->dsp_mem[id].dsp_ring_share_buf,
					&substream->dma_buffer);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_hw(&dsp->dsp_mem[id].adsp_buf,
				 BUFFER_TYPE_SHARE_MEM);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_memorytype(&dsp->dsp_mem[id].adsp_buf,
					 MEMORY_AUDIO_DRAM);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_attribute(&dsp->dsp_mem[id].adsp_buf,
					substream,
					params,
					afe_get_pcmdir(substream->stream,
					dsp->dsp_mem[id].adsp_buf));
	if (ret < 0)
		goto error;

	memcpy(&dsp->dsp_mem[id].adsp_work_buf, &dsp->dsp_mem[id].adsp_buf,
	       sizeof(struct audio_hw_buffer));
	/* send audio_hw_buffer to SCP side */
	ipi_audio_buf = (void *)dsp->dsp_mem[id].msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp->dsp_mem[id].adsp_buf,
	       sizeof(struct audio_hw_buffer));

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp->dsp_mem[id].ring_buf);
#endif

	/* send to task with hw_param information , buffer and pcm attribute */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWPARAM,
			 sizeof(unsigned int),
			 (unsigned int)
			 dsp_memif->msg_atod_share_buf.phy_addr,
			 (char *)&dsp->dsp_mem[id].msg_atod_share_buf.phy_addr);

	return ret;

error:
	pr_err("%s err\n", __func__);
	return -1;
}

static int mtk_dsp_pcm_hw_free(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	struct gen_pool *gen_pool_dsp;


	gen_pool_dsp = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);

	/* send to task with free status */
	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWFREE, 1, 0,
			 NULL);

	if (ret)
		pr_info("%s ret[%d]\n", __func__, ret);

	if (gen_pool_dsp != NULL && substream->dma_buffer.area) {
		ret = mtk_adsp_genpool_free_sharemem_ring
				(&dsp->dsp_mem[id], id);
		if (!ret)
			release_snd_dmabuffer(&substream->dma_buffer);
	}

	/* release dsp memory */
	ret = reset_audiobuffer_hw(&dsp->dsp_mem[id].adsp_buf);

	dsp->release_dram_resource(dsp->dev);

	return ret;
}

static int mtk_dsp_pcm_hw_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	int id = rtd->cpu_dai->id;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct mtk_base_dsp_mem *dsp_memif = &dsp->dsp_mem[id];
	struct audio_hw_buffer *adsp_buf = &dsp->dsp_mem[id].adsp_buf;

	clear_audiobuffer_hw(adsp_buf);
	RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);
	RingBuf_Bridge_Reset(&adsp_buf->aud_buffer.buf_bridge);
	RingBuf_Bridge_Reset(
		&dsp->dsp_mem[id].adsp_work_buf.aud_buffer.buf_bridge);

	ret = set_audiobuffer_threshold(adsp_buf, substream);
	if (ret < 0)
		pr_warn("%s set_audiobuffer_attribute err\n", __func__);

	pr_info("%s(), task_id: %d start_threshold: %u stop_threshold: %u period_size: %d period_count: %d\n",
		__func__, id,
		adsp_buf->aud_buffer.start_threshold,
		adsp_buf->aud_buffer.stop_threshold,
		adsp_buf->aud_buffer.period_size,
		adsp_buf->aud_buffer.period_count);

	/* send audio_hw_buffer to SCP side */
	ipi_audio_buf = (void *)dsp->dsp_mem[id].msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)adsp_buf,
	       sizeof(struct audio_hw_buffer));

	/* send to task with prepare status */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_PREPARE,
			 sizeof(unsigned int),
			 (unsigned int)
			 dsp_memif->msg_atod_share_buf.phy_addr,
			 (char *)&dsp->dsp_mem[id].msg_atod_share_buf.phy_addr);
	return ret;
}

static int mtk_dsp_start(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;

	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_DIRECT_SEND, AUDIO_DSP_TASK_START,
			       1, 0, NULL);
	return ret;
}
static int mtk_dsp_stop(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;

	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_DIRECT_SEND, AUDIO_DSP_TASK_STOP,
			       1, 0, NULL);

	return ret;
}

static int mtk_dsp_pcm_hw_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);

	dev_info(dsp->dev, "%s cmd %d id = %d\n",
		 __func__, cmd, rtd->cpu_dai->id);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_dsp_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_dsp_stop(substream);
	}
	return -EINVAL;
}

static int mtk_dsp_pcm_copy_dl(struct snd_pcm_substream *substream,
			       int copy_size,
			       struct mtk_base_dsp_mem *dsp_mem,
			       void __user *buf)
{
	int ret = 0, availsize = 0;
	unsigned long flags = 0;
	int ack_type;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct RingBuf *ringbuf = &(dsp_mem->ring_buf);
	struct ringbuf_bridge *buf_bridge =
		&(dsp_mem->adsp_buf.aud_buffer.buf_bridge);


#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	availsize = RingBuf_getFreeSpace(ringbuf);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	if (availsize >= copy_size) {
		RingBuf_copyFromUserLinear(ringbuf, buf, copy_size);
		RingBuf_Bridge_update_writeptr(buf_bridge, copy_size);
	} else {
		pr_info("%s, id = %d, fail copy_size = %d availsize = %d\n",
			__func__, id, copy_size, RingBuf_getFreeSpace(ringbuf));
		return -1;
	}

	/* send audio_hw_buffer to SCP side*/
	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
	       sizeof(struct audio_hw_buffer));

	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dsp_mem->adsp_buf.counter++;

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	if (substream->runtime->status->state != SNDRV_PCM_STATE_RUNNING)
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
	else
		ack_type = AUDIO_IPI_MSG_BYPASS_ACK;
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			ack_type, AUDIO_DSP_TASK_DLCOPY,
			sizeof(unsigned int),
			(unsigned int)dsp_mem->msg_atod_share_buf.phy_addr,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);

	return ret;
}

static int mtk_dsp_pcm_copy_ul(struct snd_pcm_substream *substream,
			       int copy_size,
			       struct mtk_base_dsp_mem *dsp_mem,
			       void __user *buf)
{
	int ret = 0, availsize = 0;
	unsigned long flags = 0;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct RingBuf *ringbuf = &(dsp_mem->ring_buf);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif
	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
			&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	availsize = RingBuf_getDataCount(ringbuf);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	if (availsize < copy_size) {
		pr_info("%s fail copy_size = %d availsize = %d\n", __func__,
			copy_size, RingBuf_getFreeSpace(ringbuf));
		return -1;
	}

	/* get audio_buffer from ring buffer */
	ringbuf_copyto_user_linear(buf, &dsp_mem->ring_buf, copy_size);
	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	sync_bridge_ringbuf_readidx(&dsp_mem->adsp_buf.aud_buffer.buf_bridge,
				    &dsp_mem->ring_buf);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);
	dsp_mem->adsp_buf.counter++;

	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
		sizeof(struct audio_hw_buffer));
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_ULCOPY,
			sizeof(unsigned int),
			(unsigned int)dsp_mem->msg_atod_share_buf.phy_addr,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);

#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s("1 mtk_dsp_ul_handler",
				&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_dsp_ul_handler",
				&dsp_mem->ring_buf);
#endif
	return ret;
}

static int mtk_dsp_pcm_copy(struct snd_pcm_substream *substream,
			    int channel, unsigned long pos,
			    void __user *buf, unsigned long count)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(rtd->platform);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	int ret = 0;

	if (count <= 0) {
		pr_info(
			"error %s channel = %d pos = %lu count = %lu\n",
			__func__, channel, pos, count);
		return -1;
	}

	if (is_audio_task_dsp_ready(get_dspscene_by_dspdaiid(id)) == false) {
		pr_info("%s(), dsp not ready", __func__);
		return -1;
	}

#ifdef DEBUG_VERBOSE
	pr_info(
		"+%s, id = %d, channel = %d pos = %lu count = %lu\n",
		__func__, id, channel, pos, count);
#endif
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mtk_dsp_pcm_copy_dl(substream, count, dsp_mem, buf);
	else
		ret = mtk_dsp_pcm_copy_ul(substream, count, dsp_mem, buf);

	return ret;
}

static int mtk_dsp_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	pr_info("%s()\n", __func__);

	return 0;
}

void audio_irq_handler(int irq, void *data, int core_id)
{
	struct mtk_base_dsp *dsp = (struct mtk_base_dsp *)data;
	unsigned long task_value;
	int dsp_scene, task_id, loop_count;
	unsigned long *pdtoa;

	if (!dsp) {
		pr_info("%s dsp[%p]\n", __func__, dsp);
		goto IRQ_ERROR;
	}
	if (core_id >= ADSP_CORE_TOTAL) {
		pr_info("%s core_id[%d]\n", __func__, core_id);
		goto IRQ_ERROR;
	}
	if (!dsp->core_share_mem.ap_adsp_core_mem[core_id]) {
		pr_info("%s core_id [%d] ap_adsp_core_mem[%p]\n",
			__func__, core_id,
			dsp->core_share_mem.ap_adsp_core_mem[core_id]);
		goto IRQ_ERROR;
	}

#ifdef DEBUG_VERBOSE_IRQ
	pr_info("enter %s\n", __func__);
#endif

	/* using semaphore to sync ap <=> adsp */
	if (get_adsp_semaphore(SEMA_AUDIO))
		pr_info("%s get semaphore fail\n", __func__);
	pdtoa = (unsigned long *)
		&dsp->core_share_mem.ap_adsp_core_mem[core_id]->dtoa_flag;
	loop_count = DSP_IRQ_LOOP_COUNT;
	/* read dram data need mb()  */
	mb();
	do {
		/* valid bits */
		task_value = fls(*pdtoa);
		if (task_value) {
			dsp_scene = task_value - 1;
			task_id = get_dspdaiid_by_dspscene(dsp_scene);
#ifdef DEBUG_VERBOSE_IRQ
			pr_info("+%s flag[%llx] task_id[%d] task_value[%lu]\n",
			__func__, *pdtoa, task_id, task_value);
#endif
			clear_bit(dsp_scene, pdtoa);
			/* read dram data need mb()  */
			mb();
			if (task_id >= 0)
				mtk_dsp_dl_consume_handler(dsp, NULL, task_id);
		}
		loop_count--;
	} while (*pdtoa && task_value && loop_count > 0);
	release_adsp_semaphore(SEMA_AUDIO);
	return;
IRQ_ERROR:
	pr_info("IRQ_ERROR irq[%d] data[%p] core_id[%d] dsp[%p]\n",
		irq, data, core_id, dsp);
}

#ifdef CFG_RECOVERY_SUPPORT
static int audio_send_reset_event(void)
{
	int ret = 0, i;

	for (i = 0; i < TASK_SCENE_SIZE; i++) {
		if ((i == TASK_SCENE_DEEPBUFFER) ||
			(i == TASK_SCENE_VOIP) ||
			(i == TASK_SCENE_PRIMARY) ||
			(i == TASK_SCENE_FAST)) {
			ret = mtk_scp_ipi_send(i, AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_RESET,
			ADSP_EVENT_READY, 0, NULL);
			pr_info("%s scene = %d\n", __func__, i);
		}
	}
	return ret;
}

static int audio_event_receive(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	int ret = 0;

	switch (event) {
	case ADSP_EVENT_STOP:
		pr_info("%s event[%lu]\n", __func__, event);
		break;
	case ADSP_EVENT_READY: {
		audio_send_reset_event();
		pr_info("%s event[%lu]\n", __func__, event);
		break;
	}
	default:
		pr_info("%s event[%lu]\n", __func__, event);
	}
	return ret;
}

static struct notifier_block adsp_audio_notifier = {
	.notifier_call = audio_event_receive,
	.priority = PRIMARY_FEATURE_PRI,
};

#endif

static int mtk_dsp_probe(struct snd_soc_platform *platform)
{
	int ret = 0;
	int id = 0;
	struct mtk_base_dsp *dsp = snd_soc_platform_get_drvdata(platform);

	pr_info("%s\n", __func__);

	aud_wake_lock_init(&adsp_audio_wakelock, "adsp_audio_wakelock");

	snd_soc_add_platform_controls(platform,
				      dsp_platform_kcontrols,
				      ARRAY_SIZE(dsp_platform_kcontrols));

	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++) {
		ret = audio_task_register_callback(get_dspscene_by_dspdaiid(id),
						   mtk_dsp_pcm_ipi_recv, NULL);
		if (ret < 0)
			return ret;
	}

	for (id = 0; id < ADSP_CORE_TOTAL; id++) {
		if (adsp_irq_registration(id, ADSP_IRQ_AUDIO_ID, audio_irq_handler, dsp) < 0)
			pr_info("%s, ADSP_IRQ_AUDIO not supported\n");
	}

	adsp_register_notify(&adsp_audio_notifier);

	return ret;
}

static const struct snd_pcm_ops mtk_dsp_pcm_ops = {
	.open = mtk_dsp_pcm_open,
	.close = mtk_dsp_pcm_close,
	.hw_params = mtk_dsp_pcm_hw_params,
	.hw_free = mtk_dsp_pcm_hw_free,
	.prepare = mtk_dsp_pcm_hw_prepare,
	.trigger = mtk_dsp_pcm_hw_trigger,
	.ioctl = snd_pcm_lib_ioctl,
	.pointer = mtk_dsphw_pcm_pointer,
	.copy_user = mtk_dsp_pcm_copy,
};

const struct snd_soc_platform_driver mtk_dsp_pcm_platform = {
	.probe = mtk_dsp_probe,
	.ops = &mtk_dsp_pcm_ops,
	.pcm_new = mtk_dsp_pcm_new,
};
EXPORT_SYMBOL_GPL(mtk_dsp_pcm_platform);

MODULE_DESCRIPTION("Mediatek dsp platform driver");
MODULE_AUTHOR("chipeng Chang <chipeng.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

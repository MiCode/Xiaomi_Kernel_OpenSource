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
static int adsp_wakelock_count;
static struct wakeup_source *adsp_audio_wakelock;
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

	set_task_attr(AUDIO_TASK_PRIMARY_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}


static int dsp_primary_runtime_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PRIMARY_ID,
			      ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_deepbuf_runtime_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_DEEPBUFFER_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_deepbuf_runtime_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_DEEPBUFFER_ID,
			      ADSP_TASK_ATTR_RUNTIME);
	return 0;
}


static int dsp_voipdl_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_VOIP_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_voipdl_runtime_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_VOIP_ID,
			      ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_playback_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_playback_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			      ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_captureul1_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_UL1_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_captureul1_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CAPTURE_UL1_ID,
			      ADSP_TASK_ATTR_RUNTIME);
	return 0;
}


static int dsp_offload_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_OFFLOAD_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_offload_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_OFFLOAD_ID, ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_a2dp_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_a2dp_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_A2DP_ID, ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_dataprovider_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_DATAPROVIDER_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_dataprovider_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_DATAPROVIDER_ID,
				ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_ktv_runtime_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_KTV_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_ktv_runtime_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_KTV_ID, ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int dsp_capture_raw_runtime_set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CAPTURE_RAW_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_capture_raw_runtime_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int val = get_task_attr(AUDIO_TASK_CAPTURE_RAW_ID,
				ADSP_TASK_ATTR_RUNTIME);
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
			aud_wake_lock(adsp_audio_wakelock);
	} else {
		adsp_wakelock_count--;
		if (adsp_wakelock_count == 0)
			aud_wake_unlock(adsp_audio_wakelock);
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

static int dsp_call_final_runtime_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_CALL_FINAL_ID, ADSP_TASK_ATTR_RUNTIME, val);
	return 0;
}

static int dsp_call_final_runtime_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_CALL_FINAL_ID,
			      ADSP_TASK_ATTR_RUNTIME);
	return 0;
}

static int smartpa_swdsp_process_enable_set(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val = ucontrol->value.integer.value[0];

	pr_debug("%s() enable = %d\n", __func__, val);
	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_SMARTPA, val);
	return 0;
}

static int smartpa_swdsp_process_enable_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			      ADSP_TASK_ATTR_SMARTPA);
	pr_debug("%s() enable = %d\n", __func__,
		ucontrol->value.integer.value[0]);
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
	SOC_SINGLE_EXT("dsp_primary_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_primary_default_get, dsp_primary_default_set),
	SOC_SINGLE_EXT("dsp_deepbuf_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_deepbuf_default_get, dsp_deepbuf_default_set),
	SOC_SINGLE_EXT("dsp_voipdl_default_en", SND_SOC_NOPM, 0, 0x1, 0,
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
	SOC_SINGLE_EXT("dsp_captureul1_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_captureul1_runtime_get, dsp_captureul1_runtime_set),
	SOC_SINGLE_EXT("dsp_offload_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_offload_runtime_get, dsp_offload_runtime_set),
	SOC_SINGLE_EXT("dsp_a2dp_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_a2dp_runtime_get, dsp_a2dp_runtime_set),
	SOC_SINGLE_EXT("dsp_dataprovider_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_dataprovider_runtime_get,
		       dsp_dataprovider_runtime_set),
	SOC_SINGLE_EXT("audio_dsp_wakelock", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_wakelock_get, dsp_wakelock_set),
	SOC_SINGLE_EXT("dsp_call_final_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_call_final_runtime_get, dsp_call_final_runtime_set),
	SOC_SINGLE_EXT("dsp_ktv_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_ktv_runtime_get, dsp_ktv_runtime_set),
	SOC_SINGLE_EXT("dsp_captureul1_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_captureul1_ref_runtime_get, dsp_captureul1_ref_runtime_set),
	SOC_SINGLE_EXT("dsp_playback_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_playback_ref_runtime_get, dsp_playback_ref_runtime_set),
	SOC_SINGLE_EXT("dsp_call_final_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_call_final_ref_runtime_get, dsp_call_final_ref_runtime_set),
	SOC_SINGLE_EXT("dsp_captureraw_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_capture_raw_runtime_get,
		       dsp_capture_raw_runtime_set),
	SOC_SINGLE_EXT("swdsp_smartpa_process_enable", SND_SOC_NOPM, 0, 0xff, 0,
		       smartpa_swdsp_process_enable_get,
		       smartpa_swdsp_process_enable_set),
	SOC_SINGLE_EXT("ktv_status", SND_SOC_NOPM, 0, 0x1, 0,
		       ktv_status_get, ktv_status_set),
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
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
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
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	struct mtk_base_afe *afe = get_afe_base();
	struct mtk_base_afe_memif *memif =
			&afe->memif[get_afememdl_by_afe_taskid(id)];
	const struct mtk_base_memif_data *memif_data = memif->data;
	struct regmap *regmap = afe->regmap;
	struct device *dev = afe->dev;
	int reg_ofs_base = memif_data->reg_ofs_base;
	int reg_ofs_cur = memif_data->reg_ofs_cur;
	unsigned int hw_ptr = 0, hw_base = 0;
	int ret, pcm_ptr_bytes, pcm_remap_ptr_bytes;
	unsigned long flags;

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
	dump_rbuf_bridge_s("1 mtk_dsp_dl_handler",
				&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_dsp_dl_handler",
				&dsp_mem->ring_buf);
#endif
	sync_ringbuf_readidx(
		&dsp_mem->ring_buf,
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE
	pr_info("%s id = %d reg_ofs_base = %d reg_ofs_cur = %d pcm_ptr_bytes = %d pcm_remap_ptr_bytes = %d\n",
		 __func__, id, reg_ofs_base, reg_ofs_cur,
		 pcm_ptr_bytes, pcm_remap_ptr_bytes);
#endif

POINTER_RETURN_FRAMES:
	return bytes_to_frames(substream->runtime, pcm_remap_ptr_bytes);
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

	if (dsp->dsp_mem[id].substream->runtime->status->state
	    != SNDRV_PCM_STATE_RUNNING) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
DSP_IRQ_HANDLER_ERR:
	return;
}

static void mtk_dsp_ul_handler(struct mtk_base_dsp *dsp,
			       struct ipi_msg_t *ipi_msg, int id)
{
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	void *ipi_audio_buf;
	unsigned long flags;

	if (dsp->dsp_mem[id].substream == NULL) {
		pr_info("%s = substream == NULL\n", __func__);
		goto DSP_IRQ_HANDLER_ERR;
	}

	if (dsp->dsp_mem[id].substream->runtime->status->state
	    != SNDRV_PCM_STATE_RUNNING) {
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


static void mtk_dsp_pcm_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	int id = 0;

	struct mtk_base_dsp *dsp =
		(struct mtk_base_dsp *)get_ipi_recv_private();

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	if (dsp == NULL) {
		pr_warn("%s dsp == NULL\n", __func__);
		return;
	}

	id = get_dspdaiid_by_dspscene(ipi_msg->task_scene);

	if (id < 0)
		return;

	if (is_adsp_ready(ADSP_A_ID) != 1) {
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
	default:
		break;
	}
}

static int mtk_dsp_pcm_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);

	int id = rtd->cpu_dai->id;
	int dsp_feature_id = get_featureid_by_dsp_daiid(id);

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
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	int id = rtd->cpu_dai->id;
	int dsp_feature_id = get_featureid_by_dsp_daiid(id);

	pr_info("%s id[%d]\n", __func__, id);

	/* send to task with close information */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_CLOSE, 0, 0,
			 NULL);

	mtk_dsp_deregister_feature(dsp_feature_id);

	dsp->dsp_mem[id].substream = NULL;

	return ret;
}

static int mtk_dsp_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	int id = rtd->cpu_dai->id;
	void *ipi_audio_buf; /* dsp <-> audio data struct*/
	int ret = 0;

	reset_audiobuffer_hw(&dsp->dsp_mem[id].adsp_buf);
	reset_audiobuffer_hw(&dsp->dsp_mem[id].audio_afepcm_buf);
	reset_audiobuffer_hw(&dsp->dsp_mem[id].adsp_work_buf);
	RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);

	dsp->request_dram_resource(dsp->dev);

	/* gen pool related */
	dsp->dsp_mem[id].gen_pool_buffer = mtk_get_adsp_dram_gen_pool(id);
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
			 sizeof(dsp->dsp_mem[id].msg_atod_share_buf.phy_addr),
			 0,
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
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	int id = rtd->cpu_dai->id;
	struct gen_pool *gen_pool_dsp;

	gen_pool_dsp = mtk_get_adsp_dram_gen_pool(id);

	/* send to task with free status */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWFREE, 1, 0,
			 NULL);

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
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);

	int id = rtd->cpu_dai->id;
	void *ipi_audio_buf; /* dsp <-> audio data struct */

	clear_audiobuffer_hw(&dsp->dsp_mem[id].adsp_buf);
	RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);

	ret = set_audiobuffer_threshold(&dsp->dsp_mem[id].adsp_buf, substream);
	if (ret < 0)
		pr_info("%s set_audiobuffer_attribute err\n", __func__);

	/* send audio_hw_buffer to SCP side */
	ipi_audio_buf = (void *)dsp->dsp_mem[id].msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp->dsp_mem[id].adsp_buf,
	       sizeof(struct audio_hw_buffer));

	/* send to task with prepare status */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_PREPARE,
			 sizeof(dsp->dsp_mem[id].msg_atod_share_buf.phy_addr),
			 0,
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

static int copy_count;
static int mtk_dsp_stop(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;

	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_DIRECT_SEND, AUDIO_DSP_TASK_STOP,
			       1, 0, NULL);
	copy_count = 0;
	return ret;
}

static int mtk_dsp_pcm_hw_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);

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
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct RingBuf *ringbuf = &(dsp_mem->ring_buf);
	struct ringbuf_bridge *buf_bridge =
		&(dsp_mem->adsp_buf.aud_buffer.buf_bridge);


#ifdef DEBUG_VERBOSE
	pr_info("%s copy_size = %d availsize = %d\n", __func__,
		 copy_size, RingBuf_getFreeSpace(ringbuf));
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(&dsp_ringbuf_lock, flags);
	availsize = RingBuf_getFreeSpace(ringbuf);

	if (availsize >= copy_size) {
		RingBuf_copyFromUserLinear(ringbuf, buf, copy_size);
		RingBuf_Bridge_update_writeptr(buf_bridge, copy_size);
	} else {
		pr_info("%s fail copy_size = %d availsize = %d\n", __func__,
			 copy_size, RingBuf_getFreeSpace(ringbuf));
	}

	/* send audio_hw_buffer to SCP side*/
	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
	       sizeof(struct audio_hw_buffer));
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dsp_mem->adsp_buf.counter++;

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_DLCOPY,
			sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			0,
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

	if (availsize < copy_size) {
		pr_info("%s fail copy_size = %d availsize = %d\n", __func__,
			copy_size, RingBuf_getFreeSpace(ringbuf));
		spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);
		return -1;
	}

	/* get audio_buffer from ring buffer */
	ringbuf_copyto_user_linear(buf, &dsp_mem->ring_buf, copy_size);
	sync_bridge_ringbuf_readidx(&dsp_mem->adsp_buf.aud_buffer.buf_bridge,
				    &dsp_mem->ring_buf);
	dsp_mem->adsp_buf.counter++;
	spin_unlock_irqrestore(&dsp_ringbuf_lock, flags);

	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
		sizeof(struct audio_hw_buffer));
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_ULCOPY,
			sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			0,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);

#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s("1 mtk_dsp_ul_handler",
				&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_dsp_ul_handler",
				&dsp_mem->ring_buf);
#endif
	return ret;
}


static int mtk_dsp_pcm_copy(struct snd_pcm_substream *substream, int channel,
			    snd_pcm_uframes_t pos, void __user *buf,
			    unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	int ret = 0;

	if (bytes <= 0) {
		pr_info(
			"error %s channel = %d pos = %lu count = %lu bytes = %d\n",
			__func__, channel, pos, bytes, bytes);
		return -1;
	}

	if (is_audio_task_dsp_ready(get_dspscene_by_dspdaiid(id)) == false) {
		pr_info("%s(), dsp not ready", __func__);
		return -1;
	}

#ifdef DEBUG_VERBOSE
	pr_info(
		"+%s channel = %d pos = %lu count = %lu bytes = %d\n",
		__func__, channel, pos, bytes, bytes);
#endif
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mtk_dsp_pcm_copy_dl(substream, bytes, dsp_mem, buf);
	else
		ret = mtk_dsp_pcm_copy_ul(substream, bytes, dsp_mem, buf);

	return ret;
}

static int mtk_dsp_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	return ret;
}

static int mtk_dsp_probe(struct snd_soc_component *component)
{
	int ret = 0, id = 0;

	pr_info("%s\n", __func__);
	adsp_audio_wakelock = aud_wake_lock_init(NULL, "adsp_audio_wakelock");

	if (adsp_audio_wakelock == NULL)
		pr_info("%s init adsp_audio_wakelock error\n", __func__);

	ret = snd_soc_add_component_controls(component,
				      dsp_platform_kcontrols,
				      ARRAY_SIZE(dsp_platform_kcontrols));
	if (!ret)
		pr_info("%s add_component err ret = %d\n", __func__, ret);

	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++)
		ret = audio_task_register_callback(get_dspscene_by_dspdaiid(id),
						   mtk_dsp_pcm_ipi_recv, NULL);
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

const struct snd_soc_component_driver mtk_dsp_pcm_platform = {
	.name = AFE_DSP_NAME,
	.probe = mtk_dsp_probe,
	.ops = &mtk_dsp_pcm_ops,
	.pcm_new = mtk_dsp_pcm_new,
};
EXPORT_SYMBOL_GPL(mtk_dsp_pcm_platform);

MODULE_DESCRIPTION("Mediatek dsp platform driver");
MODULE_AUTHOR("chipeng Chang <chipeng.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");

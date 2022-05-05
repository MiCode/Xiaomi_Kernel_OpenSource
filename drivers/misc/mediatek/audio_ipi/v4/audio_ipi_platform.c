// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <audio_ipi_platform.h>

#include <linux/printk.h>
#include <linux/bug.h>

#include <audio_task.h>

#include <adsp_helper.h>

/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][CHIP] %s(), " fmt "\n", __func__


/*
 * =============================================================================
 *                     by chip
 * =============================================================================
 */

uint32_t audio_get_dsp_id(const uint8_t task)
{
	uint32_t dsp_id = AUDIO_OPENDSP_ID_INVALID;

	switch (task) {
	case TASK_SCENE_VOICE_ULTRASOUND:
	case TASK_SCENE_SPEAKER_PROTECTION:
	case TASK_SCENE_VOW:
	case TASK_SCENE_AUDIO_CONTROLLER_CM4:
		dsp_id = AUDIO_OPENDSP_USE_CM4_A;
		break;
	case TASK_SCENE_PLAYBACK_MP3:
	case TASK_SCENE_PRIMARY:
	case TASK_SCENE_DEEPBUFFER:
	case TASK_SCENE_AUDPLAYBACK:
	case TASK_SCENE_A2DP:
	case TASK_SCENE_BLEDL:
	case TASK_SCENE_BLEENC:
	case TASK_SCENE_DATAPROVIDER:
	case TASK_SCENE_AUD_DAEMON_A:
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A:
	case TASK_SCENE_CALL_FINAL:
	case TASK_SCENE_MUSIC:
	case TASK_SCENE_FAST:
	case TASK_SCENE_FM_ADSP:
	case TASK_SCENE_PHONE_CALL_SUB:
	case TASK_SCENE_BLECALLDL:
	case TASK_SCENE_VOIP:
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		break;
	case TASK_SCENE_PHONE_CALL:
	case TASK_SCENE_RECORD:
	case TASK_SCENE_CAPTURE_UL1:
	case TASK_SCENE_AUD_DAEMON_B:
	case TASK_SCENE_KTV:
	case TASK_SCENE_CAPTURE_RAW:
	case TASK_SCENE_BLEUL:
	case TASK_SCENE_BLEDEC:
	case TASK_SCENE_BLECALLUL:
		if (get_adsp_core_total() > 1)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
		else
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		break;
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B:
		if (get_adsp_core_total() > 1)
			dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
		else {
			pr_notice("task %d not support!!", task);
			dsp_id = AUDIO_OPENDSP_ID_INVALID;
		}
		break;
	case TASK_SCENE_AUDIO_CONTROLLER_RV:
	case TASK_SCENE_RV_SPK_PROCESS:
		dsp_id = AUDIO_OPENDSP_USE_RV_A;
		break;
	default:
		pr_notice("task %d not support!!", task);
		dsp_id = AUDIO_OPENDSP_ID_INVALID;
	}

	return dsp_id;
}
EXPORT_SYMBOL_GPL(audio_get_dsp_id);



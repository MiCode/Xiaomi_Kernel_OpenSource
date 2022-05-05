/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include <linux/types.h>

enum {
	/* scene for library */
	TASK_SCENE_PHONE_CALL           = 0,
	TASK_SCENE_VOICE_ULTRASOUND     = 1,
	TASK_SCENE_PLAYBACK_MP3         = 2,
	TASK_SCENE_RECORD               = 3,
	TASK_SCENE_VOIP                 = 4,
	TASK_SCENE_SPEAKER_PROTECTION   = 5,
	TASK_SCENE_VOW                  = 6,
	TASK_SCENE_PRIMARY              = 7,
	TASK_SCENE_DEEPBUFFER           = 8,
	TASK_SCENE_AUDPLAYBACK          = 9,
	TASK_SCENE_CAPTURE_UL1          = 10,
	TASK_SCENE_A2DP                 = 11,
	TASK_SCENE_DATAPROVIDER         = 12,
	TASK_SCENE_MUSIC                = 13,
	TASK_SCENE_CALL_FINAL           = 14,
	TASK_SCENE_FAST                 = 15,
	TASK_SCENE_KTV                  = 16,
	TASK_SCENE_CAPTURE_RAW          = 17,
	TASK_SCENE_FM_ADSP              = 18,
	TASK_SCENE_PHONE_CALL_SUB       = 19,
	TASK_SCENE_BLEDL                = 20,
	TASK_SCENE_BLEUL                = 21,
	TASK_SCENE_BLEDEC               = 22,
	TASK_SCENE_BLEENC               = 23,
	TASK_SCENE_BLECALLDL            = 24,
	TASK_SCENE_BLECALLUL            = 25,
	TASK_SCENE_RV_SPK_PROCESS       = 26,

	/* control for driver */
	TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A,
	TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B,
	TASK_SCENE_AUD_DAEMON_A,
	TASK_SCENE_AUD_DAEMON_B,
	TASK_SCENE_AUDIO_CONTROLLER_CM4,
	TASK_SCENE_AUDIO_CONTROLLER_RV,
	TASK_SCENE_SIZE,
	TASK_SCENE_INVALID
};
#endif /* end of AUDIO_TASK_H */


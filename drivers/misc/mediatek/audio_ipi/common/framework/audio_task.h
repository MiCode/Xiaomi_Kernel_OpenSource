/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
	TASK_SCENE_CALL_FINAL           = 13,

	/* control for driver */
	TASK_SCENE_AUD_DAEMON,
	TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A,
	TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B,
	TASK_SCENE_AUDIO_CONTROLLER_CM4,
	TASK_SCENE_SIZE,
	TASK_SCENE_INVALID
};



#endif /* end of AUDIO_TASK_H */


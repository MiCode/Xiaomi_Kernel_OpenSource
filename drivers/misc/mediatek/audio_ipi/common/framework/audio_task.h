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
	TASK_SCENE_PHONE_CALL = 0,
	TASK_SCENE_VOICE_ULTRASOUND,
	TASK_SCENE_PLAYBACK_MP3,
	TASK_SCENE_RECORD,
	TASK_SCENE_VOIP,
	TASK_SCENE_SPEAKER_PROTECTION,
	TASK_SCENE_VOW,
	TASK_SCENE_SIZE, /* the size of tasks */
	TASK_SCENE_CONTROLLER = 0xFF
};

typedef uint8_t task_scene_t;


#endif /* end of AUDIO_TASK_H */


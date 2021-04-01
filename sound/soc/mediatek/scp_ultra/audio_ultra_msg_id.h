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

#ifndef AUDIO_ULTRA_MSG_ID_H
#define AUDIO_ULTRA_MSG_ID_H

#define SPK_IPI_MSG_A2D_BASE (0x200)

enum {
	AUDIO_TASK_USND_MSG_ID_OFF = 0x0,
	AUDIO_TASK_USND_MSG_ID_ON = 0x1,
	AUDIO_TASK_USND_MSG_ID_DL_PREPARE,
	AUDIO_TASK_USND_MSG_ID_UL_PREPARE,
	AUDIO_TASK_USND_MSG_ID_MEMPARAM,
	AUDIO_TASK_USND_MSG_ID_START,
	AUDIO_TASK_USND_MSG_ID_STOP,
	AUDIO_TASK_USND_MSG_ID_PCMDUMP_ON,
	AUDIO_TASK_USND_MSG_ID_PCMDUMP_OFF,
	AUDIO_TASK_USND_MSG_ID_PCMDUMP_OK,
	AUDIO_TASK_USND_MSG_ID_IPI_INFO,
	AUDIO_TASK_USND_MSG_ID_ANALOG_GAIN,

	AUDIO_TASK_USND_MSG_ID_DEBUG = 0x88,
	AUDIO_TASK_USND_MSG_ID_PCMDUMP_IRQDL = 0x4444,

};

#endif /* end of AUDIO_ULTRA_MSG_ID_H */


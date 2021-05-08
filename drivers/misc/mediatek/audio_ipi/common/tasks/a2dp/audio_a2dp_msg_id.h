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

#ifndef AUDIO_A2DP_MSG_ID_H
#define AUDIO_A2DP_MSG_ID_H

enum ipi_msg_id_a2dp_t {
	AUDIO_DSP_TASK_A2DP_CODECINFO = 0x800,
	AUDIO_DSP_TASK_A2DP_SUSPEND,
	AUDIO_DSP_TASK_A2DP_START,
	AUDIO_DSP_TASK_A2DP_STOP,
};

#endif // end of AUDIO_A2DP_MSG_ID_H


/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __AUDIO_IPI_PLATFORM_H__
#define __AUDIO_IPI_PLATFORM_H__

#include <linux/types.h>

enum opendsp_id {
	AUDIO_OPENDSP_USE_CM4_A, /* => SCP_A_ID */
	AUDIO_OPENDSP_USE_CM4_B, /* => SCP_B_ID */
	AUDIO_OPENDSP_USE_HIFI3, /* => ADSP_A_ID */
	AUDIO_OPENDSP_USE_HIFI4,
	NUM_OPENDSP_TYPE,
	AUDIO_OPENDSP_ID_INVALID
};

bool audio_opendsp_id_ready(const uint8_t opendsp_id);

bool audio_opendsp_ready(const uint8_t task);
uint32_t audio_get_opendsp_id(const uint8_t task);
uint32_t audio_get_ipi_id(const uint8_t task);

#endif /*__AUDIO_IPI_PLATFORM_H__ */

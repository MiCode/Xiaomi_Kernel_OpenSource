/* Copyright (c) 2012-2013, 2015 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _Q6_AUDIO_H_
#define _Q6_AUDIO_H_

#include <linux/qdsp6v2/apr.h>

enum {
	LEGACY_PCM_MODE = 0,
	LOW_LATENCY_PCM_MODE,
	ULTRA_LOW_LATENCY_PCM_MODE,
	ULL_POST_PROCESSING_PCM_MODE,
};


int q6audio_get_port_index(u16 port_id);

int q6audio_convert_virtual_to_portid(u16 port_id);

int q6audio_validate_port(u16 port_id);

int q6audio_is_digital_pcm_interface(u16 port_id);

int q6audio_get_port_id(u16 port_id);

#endif

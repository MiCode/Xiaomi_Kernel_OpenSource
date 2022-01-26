/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#ifndef _SIA81XX_COMMOMN_H
#define _SIA81XX_COMMOMN_H

#define SIA81XX_DRIVER_VERSION			("1.2.0B")
#define SIA81XX_MAX_CHANNEL_SUPPORT		(8)

enum {
	SIA81XX_CHANNEL_L = 0,
	SIA81XX_CHANNEL_R,
	SIA81XX_CHANNEL_NUM
};

enum {
	AUDIO_SCENE_PLAYBACK = 0,
	AUDIO_SCENE_VOICE,
	AUDIO_SCENE_RECEIVER,
	AUDIO_SCENE_FACTORY,
	AUDIO_SCENE_NUM
};

enum {
	CHIP_TYPE_SIA8101 = 0,
	CHIP_TYPE_SIA8108,
	CHIP_TYPE_SIA8109,
	CHIP_TYPE_SIA8152,
	CHIP_TYPE_SIA8152S,
	CHIP_TYPE_SIA8100,
	CHIP_TYPE_SIA8159,
	// add normal chip type here
	CHIP_TYPE_SIA81X9,
	CHIP_TYPE_SIA8152X,
	CHIP_TYPE_UNKNOWN,
	CHIP_TYPE_INVALID
};

#endif /* _SIA81XX_COMMOMN_H */


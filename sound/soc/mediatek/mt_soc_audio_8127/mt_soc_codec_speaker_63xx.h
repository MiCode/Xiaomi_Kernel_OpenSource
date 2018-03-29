/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef _AUDIO_CODEC_SPEAKER_H
#define _AUDIO_CODEC_SPEAKER_H

void Speaker_ClassD_Open(void);
void Speaker_ClassD_close(void);
void Speaker_ClassAB_Open(void);
void Speaker_ClassAB_close(void);
void Speaker_ReveiverMode_Open(void);
void Speaker_ReveiverMode_close(void);
bool GetSpeakerOcFlag(void);

#endif


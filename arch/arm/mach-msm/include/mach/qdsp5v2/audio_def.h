/* Copyright (c) 2009,2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _MACH_QDSP5_V2_AUDIO_DEF_H
#define _MACH_QDSP5_V2_AUDIO_DEF_H

/* Define sound device capability */
#define SNDDEV_CAP_RX 0x1 /* RX direction */
#define SNDDEV_CAP_TX 0x2 /* TX direction */
#define SNDDEV_CAP_VOICE 0x4 /* Support voice call */
#define SNDDEV_CAP_PLAYBACK 0x8 /* Support playback */
#define SNDDEV_CAP_FM 0x10 /* Support FM radio */
#define SNDDEV_CAP_TTY 0x20 /* Support TTY */
#define SNDDEV_CAP_ANC 0x40 /* Support ANC */
#define SNDDEV_CAP_LB 0x80 /* Loopback */
#define VOC_NB_INDEX	0
#define VOC_WB_INDEX	1
#define VOC_RX_VOL_ARRAY_NUM	2

/* Device volume types . In Current deisgn only one of these are supported. */
#define SNDDEV_DEV_VOL_DIGITAL  0x1  /* Codec Digital volume control */
#define SNDDEV_DEV_VOL_ANALOG   0x2  /* Codec Analog volume control */

#define SIDE_TONE_MASK	0x01

#endif /* _MACH_QDSP5_V2_AUDIO_DEF_H */

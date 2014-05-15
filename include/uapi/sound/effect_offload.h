/*
 *  effect_offload.h - effect offload header definations
 *
 *  Copyright (C) 2013 Intel Corporation
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#ifndef __EFFECT_OFFLOAD_H
#define __EFFECT_OFFLOAD_H

#include <linux/types.h>

#define SNDRV_EFFECT_VERSION SNDRV_PROTOCOL_VERSION(0, 1, 0)

struct snd_effect {
	char uuid[16];  /* effect UUID */
	u32 device;	/* streaming interface for effect insertion */
	u32 pos;	/* position of effect to be placed in effect chain */
	u32 mode;	/* Backend for Global device (Headset/Speaker) */
} __packed;

struct snd_effect_params {
	char uuid[16];
	u32 device;
	u32 size;	/* size of parameter blob */
	u64 buffer_ptr;
} __packed;

struct snd_effect_caps {
	u32 size;	/* size of buffer to read effect descriptors */
	u64 buffer_ptr;
} __packed;

#define SNDRV_CTL_IOCTL_EFFECT_VERSION		_IOR('E', 0x00, int)
#define SNDRV_CTL_IOCTL_EFFECT_CREATE		_IOW('E', 0x01,\
						struct snd_effect)
#define SNDRV_CTL_IOCTL_EFFECT_DESTROY		_IOW('E', 0x02,\
						struct snd_effect)
#define SNDRV_CTL_IOCTL_EFFECT_SET_PARAMS	_IOW('E', 0x03,\
						struct snd_effect_params)
#define SNDRV_CTL_IOCTL_EFFECT_GET_PARAMS	_IOWR('E', 0x04,\
						struct snd_effect_params)
#define SNDRV_CTL_IOCTL_EFFECT_QUERY_NUM	_IOR('E', 0x05, int)
#define SNDRV_CTL_IOCTL_EFFECT_QUERY_CAPS	_IOWR('E', 0x06,\
						struct snd_effect_caps)
#endif

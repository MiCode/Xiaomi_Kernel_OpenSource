/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_COMPR_H
#define _MSM_COMPR_H
#include <sound/apr_audio-v2.h>
#include <sound/q6asm-v2.h>
#include <sound/compress_params.h>
#include <sound/compress_offload.h>
#include <sound/compress_driver.h>

#include "msm-pcm-q6-v2.h"

struct compr_info {
	struct snd_compr_caps compr_cap;
	struct snd_compr_codec_caps codec_caps;
	struct snd_compr_params codec_param;
};

struct compr_audio {
	struct msm_audio prtd;
	struct compr_info info;
	uint32_t codec;
};

#endif /*_MSM_COMPR_H*/

/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef _MSM_PCM_ROUTING_DEVDEP_H_
#define _MSM_PCM_ROUTING_DEVDEP_H_

#include <sound/soc.h>
#include "msm-pcm-routing-v2.h"

#ifdef CONFIG_SND_HWDEP
int msm_pcm_routing_hwdep_new(struct snd_soc_pcm_runtime *runtime,
			      struct msm_pcm_routing_bdai_data *msm_bedais);
void msm_pcm_routing_hwdep_free(struct snd_pcm *pcm);
#else
static inline int msm_pcm_routing_hwdep_new(struct snd_soc_pcm_runtime *runtime,
				struct msm_pcm_routing_bdai_data *msm_bedais)
{
	return 0;
}

static inline void msm_pcm_routing_hwdep_free(struct snd_pcm *pcm)
{
	return;
}
#endif
#endif

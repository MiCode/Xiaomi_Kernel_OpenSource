/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_MDP_DEBUG_H
#define MDSS_MDP_DEBUG_H

#include <linux/msm_mdp.h>
#include <linux/stringify.h>

#include "mdss.h"
#include "mdss_mdp.h"

static inline const char *mdss_mdp_pipetype2str(u32 ptype)
{
	static const char const *strings[] = {
#define PIPE_TYPE(t) [MDSS_MDP_PIPE_TYPE_ ## t] = __stringify(t)
		PIPE_TYPE(VIG),
		PIPE_TYPE(RGB),
		PIPE_TYPE(DMA),
		PIPE_TYPE(CURSOR),
#undef PIPE_TYPE
	};

	if (ptype >= ARRAY_SIZE(strings) || !strings[ptype])
		return "UNKOWN";

	return strings[ptype];
}

static inline const char *mdss_mdp_format2str(u32 format)
{
	static const char const *strings[] = {
#define FORMAT_NAME(f) [MDP_ ## f] = __stringify(f)
		FORMAT_NAME(RGB_565),
		FORMAT_NAME(BGR_565),
		FORMAT_NAME(RGB_888),
		FORMAT_NAME(BGR_888),
		FORMAT_NAME(RGBX_8888),
		FORMAT_NAME(RGBA_8888),
		FORMAT_NAME(ARGB_8888),
		FORMAT_NAME(XRGB_8888),
		FORMAT_NAME(BGRA_8888),
		FORMAT_NAME(BGRX_8888),
		FORMAT_NAME(Y_CBCR_H2V2_VENUS),
		FORMAT_NAME(Y_CBCR_H2V2),
		FORMAT_NAME(Y_CRCB_H2V2),
		FORMAT_NAME(Y_CB_CR_H2V2),
		FORMAT_NAME(Y_CR_CB_H2V2),
		FORMAT_NAME(Y_CR_CB_GH2V2),
		FORMAT_NAME(YCBYCR_H2V1),
		FORMAT_NAME(YCRYCB_H2V1),
#undef FORMAT_NAME
	};

	if (format >= ARRAY_SIZE(strings) || !strings[format])
		return "UNKOWN";

	return strings[format];
}
void mdss_mdp_dump(struct mdss_data_type *mdata);
void mdss_mdp_hw_rev_debug_caps_init(struct mdss_data_type *mdata);


#ifdef CONFIG_DEBUG_FS
int mdss_mdp_debugfs_init(struct mdss_data_type *mdata);
#else
static inline int mdss_mdp_debugfs_init(struct mdss_data_type *mdata)
{
	return 0;
}
#endif

#endif /* MDSS_MDP_DEBUG_H */

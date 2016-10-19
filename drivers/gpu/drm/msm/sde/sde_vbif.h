/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_VBIF_H__
#define __SDE_VBIF_H__

#include "sde_kms.h"

struct sde_vbif_set_ot_params {
	u32 xin_id;
	u32 num;
	u32 width;
	u32 height;
	u32 frame_rate;
	bool rd;
	bool is_wfd;
	u32 vbif_idx;
	u32 clk_ctrl;
};

/**
 * sde_vbif_set_ot_limit - set OT limit for vbif client
 * @sde_kms:	SDE handler
 * @params:	Pointer to OT configuration parameters
 */
void sde_vbif_set_ot_limit(struct sde_kms *sde_kms,
		struct sde_vbif_set_ot_params *params);

#endif /* __SDE_VBIF_H__ */

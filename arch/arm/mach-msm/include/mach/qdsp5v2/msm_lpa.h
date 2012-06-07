/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#ifndef _MACH_QDSP5_V2_MSM_LPA_H
#define _MACH_QDSP5_V2_MSM_LPA_H

struct lpa_mem_config {
	u32 llb_min_addr;
	u32 llb_max_addr;
	u32 sb_min_addr;
	u32 sb_max_addr;
};

struct msm_lpa_platform_data {
	u32 obuf_hlb_size;
	u32 dsp_proc_id;
	u32 app_proc_id;
	struct lpa_mem_config nosb_config; /* no summing  */
	struct lpa_mem_config sb_config; /* summing required */
};

#endif

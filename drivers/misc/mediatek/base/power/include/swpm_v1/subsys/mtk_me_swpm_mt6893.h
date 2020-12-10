/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_ME_SWPM_PLATFORM_H__
#define __MTK_ME_SWPM_PLATFORM_H__

#define ME_SWPM_RESERVED_SIZE 11

/* me power index structure */
struct me_swpm_index {
	unsigned int disp_resolution;
	unsigned int disp_fps;
	unsigned int disp_active;
	unsigned int venc_freq;
	unsigned int venc_active;
	unsigned int venc_fps;
	unsigned int vdec_freq;
	unsigned int vdec_active;
	unsigned int vdec_fps;
	unsigned int mdp_freq;
	unsigned int mdp_active;
};

struct me_swpm_rec_data {
	/* 4(int) * 11 = 44 bytes */
	unsigned int disp_resolution; /* w x h */
	unsigned int disp_fps;
	unsigned int disp_active;
	unsigned int venc_freq;
	unsigned int venc_active;
	unsigned int venc_fps;
	unsigned int vdec_freq;
	unsigned int vdec_active;
	unsigned int vdec_fps;
	unsigned int mdp_freq;
	unsigned int mdp_active;
};



#endif


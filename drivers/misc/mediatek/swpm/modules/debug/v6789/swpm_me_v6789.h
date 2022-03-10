/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __SWPM_ME_V6789_H__
#define __SWPM_ME_V6789_H__

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


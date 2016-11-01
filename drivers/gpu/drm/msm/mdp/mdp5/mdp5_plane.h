/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
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

#ifndef __MDP5_PLANE_H__
#define __MDP5_PLANE_H__

#include "msm_drv.h"

int mdp5_plane_calc_scalex_steps(struct mdp5_kms *mdp5_kms,
		uint32_t pixel_format, uint32_t src, uint32_t dest,
		uint32_t phasex_steps[COMP_MAX]);
int mdp5_plane_calc_scaley_steps(struct mdp5_kms *mdp5_kms,
		uint32_t pixel_format, uint32_t src, uint32_t dest,
		uint32_t phasey_steps[COMP_MAX]);
void mdp5_plane_calc_pixel_ext(const struct mdp_format *format,
		uint32_t src, uint32_t dst, uint32_t phase_step[2],
		int pix_ext_edge1[COMP_MAX], int pix_ext_edge2[COMP_MAX],
		bool horz);
void mdp5_pipe_write_pixel_ext(struct mdp5_kms *mdp5_kms, enum mdp5_pipe pipe,
	const struct mdp_format *format,
	uint32_t src_w, int pe_left[COMP_MAX], int pe_right[COMP_MAX],
	uint32_t src_h, int pe_top[COMP_MAX], int pe_bottom[COMP_MAX]);

#endif /* __MDP5_PLANE_H__ */


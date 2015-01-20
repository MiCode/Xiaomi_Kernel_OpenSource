/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include "type_support.h"
#include "ia_css_bnlm.host.h"

#ifndef IA_CSS_NO_DEBUG
#include "ia_css_debug.h" /* ia_css_debug_dtrace() */
#endif

/* ToDo: add all the parameters of BNLM
 * Presently only few parameters are added, as other parameter names are not
 * clear in KFS and prm files. */

/* Default kernel parameters. */
const struct ia_css_bnlm_config default_bnlm_config = {
	0,
	0,
	0,
	0,
	0
};

void
ia_css_bnlm_encode(
	struct sh_css_isp_bnlm_params *to,
	const struct ia_css_bnlm_config *from,
	size_t size)
{
	(void)size;
	to->rad_enable = from->rad_enable;
	to->rad_x_origin = from->rad_x_origin;
	to->rad_y_origin = from->rad_y_origin;
	to->avg_min_th = from->avg_min_th;
	to->avg_max_th = from->avg_max_th;
}

#ifndef IA_CSS_NO_DEBUG
void
ia_css_bnlm_debug_trace(
	const struct ia_css_bnlm_config *bnlm,
	unsigned level)
{
	if (!bnlm)
		return;

	ia_css_debug_dtrace(level, "BNLM:\n");
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "rad_enable", bnlm->rad_enable);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "rad_x_origin", bnlm->rad_x_origin);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "rad_y_origin", bnlm->rad_y_origin);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "avg_min_th", bnlm->avg_min_th);
	ia_css_debug_dtrace(level, "\t%-32s = %d\n", "avg_max_th", bnlm->avg_max_th);
}
#endif /* IA_CSS_NO_DEBUG */

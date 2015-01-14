/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "type_support.h"
#include "math_support.h"
#include "sh_css_defs.h"
#include "ia_css_types.h"
#include "ia_css_xnr3.host.h"

/* Maximum value for alpha on ISP interface */
#define XNR_MAX_ALPHA  ((1 << (ISP_VEC_ELEMBITS-1)) - 1)

/* Minimum value for sigma on host interface. Lower values translate to
 * max_alpha. */
#define XNR_MIN_SIGMA  (IA_CSS_XNR3_SIGMA_SCALE / 100)

/*
 * Default kernel parameters. In general, default is bypass mode or as close
 * to the ineffective values as possible. Due to the chroma down+upsampling,
 * perfect bypass mode is not possible for xnr3.
 */
const struct ia_css_xnr3_config default_xnr3_config = {
	/* sigma */
	{ 0, 0, 0, 0, 0, 0 },
	/* coring */
	{ 0, 0, 0, 0 }
};

/*
 * Compute an alpha value for the ISP kernel from sigma value on the host
 * parameter interface as: alpha_scale * 1/(sigma/sigma_scale)
 */
static int32_t
compute_alpha(int sigma)
{
	int32_t alpha;
#if defined(XNR_ATE_ROUNDING_BUG)
	int32_t alpha_unscaled;
#else
	int offset = sigma/2;
#endif
	if (sigma < XNR_MIN_SIGMA) {
		alpha = XNR_MAX_ALPHA;
	} else {
#if defined(XNR_ATE_ROUNDING_BUG)
		/* The scale factor for alpha must be the same as on the ISP,
		 * For sigma, it must match the public interface. The code
		 * below mimics the rounding and unintended loss of precision
		 * of the ATE reference code. It computes an unscaled alpha,
		 * rounds down, and then scales it to get the required fixed
		 * point representation. It would have been more precise to
		 * round after scaling. */
		alpha_unscaled = IA_CSS_XNR3_SIGMA_SCALE / sigma;
		alpha = alpha_unscaled * XNR_ALPHA_SCALE_FACTOR;
#else
		alpha = ((IA_CSS_XNR3_SIGMA_SCALE * XNR_ALPHA_SCALE_FACTOR) + offset)/ sigma;
#endif

		if (alpha > XNR_MAX_ALPHA) {
			alpha = XNR_MAX_ALPHA;
		}
	}

	return alpha;
}

/*
 * Compute the scaled coring value for the ISP kernel from the value on the
 * host parameter interface.
 */
static int32_t
compute_coring(int coring)
{
	int32_t isp_scale = XNR_CORING_SCALE_FACTOR;
	int32_t host_scale = IA_CSS_XNR3_CORING_SCALE;
	int32_t offset = host_scale / 2; /* fixed-point 0.5 */

	/* Convert from public host-side scale factor to isp-side scale
	 * factor. */
	return ((coring * isp_scale) + offset) / host_scale;
}

void
ia_css_xnr3_encode(
	struct sh_css_isp_xnr3_params *to,
	const struct ia_css_xnr3_config *from,
	unsigned size)
{
	int kernel_size = XNR_FILTER_SIZE;
	/* The adjust factor is the next power of 2
	   w.r.t. the kernel size*/
	int adjust_factor = ceil_pow2(kernel_size);

	int32_t alpha_y0 = compute_alpha(from->sigma.y0);
	int32_t alpha_y1 = compute_alpha(from->sigma.y1);
	int32_t alpha_u0 = compute_alpha(from->sigma.u0);
	int32_t alpha_u1 = compute_alpha(from->sigma.u1);
	int32_t alpha_v0 = compute_alpha(from->sigma.v0);
	int32_t alpha_v1 = compute_alpha(from->sigma.v1);

	int32_t coring_u0 = compute_coring(from->coring.u0);
	int32_t coring_u1 = compute_coring(from->coring.u1);
	int32_t coring_v0 = compute_coring(from->coring.v0);
	int32_t coring_v1 = compute_coring(from->coring.v1);

	(void)size;

	/* alpha's are represented in qN.5 format */
	to->alpha.y0 = alpha_y0;
	to->alpha.u0 = alpha_u0;
	to->alpha.v0 = alpha_v0;
	to->alpha.ydiff = (alpha_y1 - alpha_y0) * adjust_factor / kernel_size;
	to->alpha.udiff = (alpha_u1 - alpha_u0) * adjust_factor / kernel_size;
	to->alpha.vdiff = (alpha_v1 - alpha_v0) * adjust_factor / kernel_size;

	/* coring parameters are expressed in q1.NN format */
	to->coring.u0 = coring_u0;
	to->coring.v0 = coring_v0;
	to->coring.udiff = (coring_u1 - coring_u0) * adjust_factor / kernel_size;
	to->coring.vdiff = (coring_v1 - coring_v0) * adjust_factor / kernel_size;
}

/* Dummy Function added as the tool expects it*/
void
ia_css_xnr3_debug_dtrace(
	const struct ia_css_xnr3_config *config,
	unsigned level)
{
	(void)config;
	(void)level;
}

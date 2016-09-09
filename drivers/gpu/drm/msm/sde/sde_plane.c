/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <uapi/drm/sde_drm.h>

#include "msm_prop.h"

#include "sde_kms.h"
#include "sde_fence.h"
#include "sde_formats.h"
#include "sde_hw_sspp.h"

#define DECIMATED_DIMENSION(dim, deci) (((dim) + ((1 << (deci)) - 1)) >> (deci))
#define PHASE_STEP_SHIFT	21
#define PHASE_STEP_UNIT_SCALE   ((int) (1 << PHASE_STEP_SHIFT))
#define PHASE_RESIDUAL		15

#define SHARP_STRENGTH_DEFAULT	32
#define SHARP_EDGE_THR_DEFAULT	112
#define SHARP_SMOOTH_THR_DEFAULT	8
#define SHARP_NOISE_THR_DEFAULT	2

#define SDE_NAME_SIZE  12

#define SDE_PLANE_COLOR_FILL_FLAG	BIT(31)

struct sde_plane {
	struct drm_plane base;

	int mmu_id;

	struct mutex lock;

	enum sde_sspp pipe;
	uint32_t features;      /* capabilities from catalog */
	uint32_t nformats;
	uint32_t formats[64];

	struct sde_hw_pipe *pipe_hw;
	struct sde_hw_pipe_cfg pipe_cfg;
	struct sde_hw_pixel_ext pixel_ext;
	struct sde_hw_sharp_cfg sharp_cfg;
	struct sde_hw_scaler3_cfg scaler3_cfg;
	uint32_t color_fill;
	bool is_error;

	struct sde_csc_cfg csc_cfg;
	struct sde_csc_cfg *csc_ptr;

	const struct sde_sspp_sub_blks *pipe_sblk;

	char pipe_name[SDE_NAME_SIZE];

	struct msm_property_info property_info;
	struct msm_property_data property_data[PLANE_PROP_COUNT];
	struct drm_property_blob *blob_info;

	/* debugfs related stuff */
	struct dentry *debugfs_root;
	struct sde_debugfs_regset32 debugfs_src;
	struct sde_debugfs_regset32 debugfs_scaler;
	struct sde_debugfs_regset32 debugfs_csc;
};

#define to_sde_plane(x) container_of(x, struct sde_plane, base)

static bool sde_plane_enabled(struct drm_plane_state *state)
{
	return state && state->fb && state->crtc;
}

/* helper to update a state's input fence pointer from the property */
static void _sde_plane_set_input_fence(struct drm_plane *plane,
		struct sde_plane_state *pstate, uint64_t fd)
{
	if (!plane || !pstate)
		return;

	/* clear previous reference */
	if (pstate->input_fence)
		sde_sync_put(pstate->input_fence);

	/* get fence pointer for later */
	pstate->input_fence = sde_sync_get(fd);

	SDE_DEBUG("0x%llX\n", fd);
}

int sde_plane_wait_input_fence(struct drm_plane *plane, uint32_t wait_ms)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	void *input_fence;
	int ret = -EINVAL;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
	} else if (!plane->state) {
		SDE_ERROR("invalid plane state\n");
	} else {
		psde = to_sde_plane(plane);
		pstate = to_sde_plane_state(plane->state);
		input_fence = pstate->input_fence;

		if (input_fence) {
			ret = sde_sync_wait(input_fence, wait_ms);
			switch (ret) {
			case 0:
				SDE_DEBUG("%s signaled\n", psde->pipe_name);
				break;
			case -ETIME:
				SDE_ERROR("timeout on %s, %ums\n",
						psde->pipe_name, wait_ms);
				psde->is_error = true;
				break;
			default:
				SDE_ERROR("error on %s, %d\n",
						psde->pipe_name, ret);
				psde->is_error = true;
				break;
			}
		} else {
			ret = 0;
		}
	}
	return ret;
}

static void _sde_plane_set_scanout(struct drm_plane *plane,
		struct sde_plane_state *pstate,
		struct sde_hw_pipe_cfg *pipe_cfg,
		struct drm_framebuffer *fb)
{
	struct sde_plane *psde;
	int ret, i;

	if (!plane || !pstate || !pipe_cfg || !fb)
		return;

	psde = to_sde_plane(plane);

	ret = sde_format_populate_layout(psde->mmu_id, fb, &pipe_cfg->layout);
	if (ret) {
		SDE_ERROR("failed to get format layout, error: %d\n", ret);
		return;
	}

	if (sde_plane_get_property(pstate, PLANE_PROP_SRC_CONFIG) &
			BIT(SDE_DRM_DEINTERLACE))
		for (i = 0; i < SDE_MAX_PLANES; ++i)
			pipe_cfg->layout.plane_pitch[i] <<= 1;

	if (psde->pipe_hw && psde->pipe_hw->ops.setup_sourceaddress)
		psde->pipe_hw->ops.setup_sourceaddress(psde->pipe_hw, pipe_cfg);
}

static void _sde_plane_setup_scaler3(struct sde_plane *psde,
		uint32_t src_w, uint32_t src_h, uint32_t dst_w, uint32_t dst_h,
		struct sde_hw_scaler3_cfg *scale_cfg,
		const struct sde_format *fmt,
		uint32_t chroma_subsmpl_h, uint32_t chroma_subsmpl_v)
{
}

/**
 * _sde_plane_setup_scaler2(): Determine default scaler phase steps/filter type
 * @psde: Pointer to SDE plane object
 * @src: Source size
 * @dst: Destination size
 * @phase_steps: Pointer to output array for phase steps
 * @filter: Pointer to output array for filter type
 * @fmt: Pointer to format definition
 * @chroma_subsampling: Subsampling amount for chroma channel
 *
 * Returns: 0 on success
 */
static int _sde_plane_setup_scaler2(struct sde_plane *psde,
		uint32_t src, uint32_t dst, uint32_t *phase_steps,
		enum sde_hw_filter *filter, const struct sde_format *fmt,
		uint32_t chroma_subsampling)
{
	if (!psde || !phase_steps || !filter || !fmt) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	/* calculate phase steps, leave init phase as zero */
	phase_steps[SDE_SSPP_COMP_0] =
		mult_frac(1 << PHASE_STEP_SHIFT, src, dst);
	phase_steps[SDE_SSPP_COMP_1_2] =
		phase_steps[SDE_SSPP_COMP_0] / chroma_subsampling;
	phase_steps[SDE_SSPP_COMP_2] = phase_steps[SDE_SSPP_COMP_1_2];
	phase_steps[SDE_SSPP_COMP_3] = phase_steps[SDE_SSPP_COMP_0];

	/* calculate scaler config, if necessary */
	if (SDE_FORMAT_IS_YUV(fmt) || src != dst) {
		filter[SDE_SSPP_COMP_3] =
			(src <= dst) ? SDE_SCALE_FILTER_BIL :
			SDE_SCALE_FILTER_PCMN;

		if (SDE_FORMAT_IS_YUV(fmt)) {
			filter[SDE_SSPP_COMP_0] = SDE_SCALE_FILTER_CA;
			filter[SDE_SSPP_COMP_1_2] = filter[SDE_SSPP_COMP_3];
		} else {
			filter[SDE_SSPP_COMP_0] = filter[SDE_SSPP_COMP_3];
			filter[SDE_SSPP_COMP_1_2] =
				SDE_SCALE_FILTER_NEAREST;
		}
	} else {
		/* disable scaler */
		SDE_DEBUG("disable scaler\n");
		filter[SDE_SSPP_COMP_0] = SDE_SCALE_FILTER_MAX;
		filter[SDE_SSPP_COMP_1_2] = SDE_SCALE_FILTER_MAX;
		filter[SDE_SSPP_COMP_3] = SDE_SCALE_FILTER_MAX;
	}
	return 0;
}

/**
 * _sde_plane_setup_pixel_ext - determine default pixel extension values
 * @psde: Pointer to SDE plane object
 * @src: Source size
 * @dst: Destination size
 * @decimated_src: Source size after decimation, if any
 * @phase_steps: Pointer to output array for phase steps
 * @out_src: Output array for pixel extension values
 * @out_edge1: Output array for pixel extension first edge
 * @out_edge2: Output array for pixel extension second edge
 * @filter: Pointer to array for filter type
 * @fmt: Pointer to format definition
 * @chroma_subsampling: Subsampling amount for chroma channel
 * @post_compare: Whether to chroma subsampled source size for comparisions
 */
static void _sde_plane_setup_pixel_ext(struct sde_plane *psde,
		uint32_t src, uint32_t dst, uint32_t decimated_src,
		uint32_t *phase_steps, uint32_t *out_src, int *out_edge1,
		int *out_edge2, enum sde_hw_filter *filter,
		const struct sde_format *fmt, uint32_t chroma_subsampling,
		bool post_compare)
{
	int64_t edge1, edge2, caf;
	uint32_t src_work;
	int i, tmp;

	if (psde && phase_steps && out_src && out_edge1 &&
			out_edge2 && filter && fmt) {
		/* handle CAF for YUV formats */
		if (SDE_FORMAT_IS_YUV(fmt) && *filter == SDE_SCALE_FILTER_CA)
			caf = PHASE_STEP_UNIT_SCALE;
		else
			caf = 0;

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			src_work = decimated_src;
			if (i == SDE_SSPP_COMP_1_2 || i == SDE_SSPP_COMP_2)
				src_work /= chroma_subsampling;
			if (post_compare)
				src = src_work;
			if (!SDE_FORMAT_IS_YUV(fmt) && (src == dst)) {
				/* unity */
				edge1 = 0;
				edge2 = 0;
			} else if (dst >= src) {
				/* upscale */
				edge1 = (1 << PHASE_RESIDUAL);
				edge1 -= caf;
				edge2 = (1 << PHASE_RESIDUAL);
				edge2 += (dst - 1) * *(phase_steps + i);
				edge2 -= (src_work - 1) * PHASE_STEP_UNIT_SCALE;
				edge2 += caf;
				edge2 = -(edge2);
			} else {
				/* downscale */
				edge1 = 0;
				edge2 = (dst - 1) * *(phase_steps + i);
				edge2 -= (src_work - 1) * PHASE_STEP_UNIT_SCALE;
				edge2 += *(phase_steps + i);
				edge2 = -(edge2);
			}

			/* only enable CAF for luma plane */
			caf = 0;

			/* populate output arrays */
			*(out_src + i) = src_work;

			/* edge updates taken from __pxl_extn_helper */
			if (edge1 >= 0) {
				tmp = (uint32_t)edge1;
				tmp >>= PHASE_STEP_SHIFT;
				*(out_edge1 + i) = -tmp;
			} else {
				tmp = (uint32_t)(-edge1);
				*(out_edge1 + i) =
					(tmp + PHASE_STEP_UNIT_SCALE - 1) >>
					PHASE_STEP_SHIFT;
			}
			if (edge2 >= 0) {
				tmp = (uint32_t)edge2;
				tmp >>= PHASE_STEP_SHIFT;
				*(out_edge2 + i) = -tmp;
			} else {
				tmp = (uint32_t)(-edge2);
				*(out_edge2 + i) =
					(tmp + PHASE_STEP_UNIT_SCALE - 1) >>
					PHASE_STEP_SHIFT;
			}
		}
	}
}

/**
 * _sde_plane_verify_blob - verify incoming blob is big enough to contain
 *                          sub-structure
 * @blob_ptr: Pointer to start of incoming blob data
 * @blob_size: Size of incoming blob data, in bytes
 * @sub_ptr: Pointer to start of desired sub-structure
 * @sub_size: Required size of sub-structure, in bytes
 */
static int _sde_plane_verify_blob(void *blob_ptr,
		size_t blob_size,
		void *sub_ptr,
		size_t sub_size)
{
	/*
	 * Use the blob size provided by drm to check if there are enough
	 * bytes from the start of versioned sub-structures to the end of
	 * blob data:
	 *
	 * e.g.,
	 * blob_ptr             --> struct blob_data {
	 *                                  uint32_t version;
	 * sub_ptr              -->         struct blob_data_v1 v1;
	 * sub_ptr + sub_size   -->         struct blob_stuff more_stuff;
	 * blob_ptr + blob_size --> };
	 *
	 * It's important to check the actual number of bytes from the start
	 * of the sub-structure to the end of the blob data, and not just rely
	 * on something like,
	 *
	 * sizeof(blob) - sizeof(blob->version) >= sizeof(sub-struct)
	 *
	 * This is because the start of the sub-structure can vary based on
	 * how the compiler pads the overall structure.
	 */
	if (blob_ptr && sub_ptr)
		/* return zero if end of blob >= end of sub-struct */
		return ((unsigned char *)blob_ptr + blob_size) <
			((unsigned char *)sub_ptr + sub_size);
	return -EINVAL;
}

static void _sde_plane_setup_csc(struct sde_plane *psde,
		struct sde_plane_state *pstate,
		const struct sde_format *fmt)
{
	static const struct sde_csc_cfg sde_csc_YUV2RGB_601L = {
		{
			/* S15.16 format */
			0x00012A00, 0x00000000, 0x00019880,
			0x00012A00, 0xFFFF9B80, 0xFFFF3000,
			0x00012A00, 0x00020480, 0x00000000,
		},
		/* signed bias */
		{ 0xfff0, 0xff80, 0xff80,},
		{ 0x0, 0x0, 0x0,},
		/* unsigned clamp */
		{ 0x10, 0xeb, 0x10, 0xf0, 0x10, 0xf0,},
		{ 0x00, 0xff, 0x00, 0xff, 0x00, 0xff,},
	};
	static const struct sde_csc_cfg sde_csc_NOP = {
		{
			/* identity matrix, S15.16 format */
			0x10000, 0x00000, 0x00000,
			0x00000, 0x10000, 0x00000,
			0x00000, 0x00000, 0x10000,
		},
		/* signed bias */
		{ 0x0, 0x0, 0x0,},
		{ 0x0, 0x0, 0x0,},
		/* unsigned clamp */
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
		{ 0x0, 0xff, 0x0, 0xff, 0x0, 0xff,},
	};
	struct sde_drm_csc *csc = NULL;
	size_t csc_size = 0;
	int i;

	if (!psde || !pstate || !fmt) {
		SDE_ERROR("invalid arguments\n");
		return;
	}
	if (!psde->pipe_hw || !psde->pipe_hw->ops.setup_csc)
		return;

	/* check for user space override */
	psde->csc_ptr = NULL;
	csc = msm_property_get_blob(&psde->property_info,
			pstate->property_blobs,
			&csc_size,
			PLANE_PROP_CSC);
	if (csc) {
		/* user space override */
		memcpy(&psde->csc_cfg,
				&sde_csc_NOP,
				sizeof(struct sde_csc_cfg));
		switch (csc->version) {
		case SDE_DRM_CSC_V1:
			if (!_sde_plane_verify_blob(csc,
					csc_size,
					&csc->v1,
					sizeof(struct sde_drm_csc_v1))) {
				for (i = 0; i < SDE_CSC_MATRIX_COEFF_SIZE; ++i)
					psde->csc_cfg.csc_mv[i] =
						csc->v1.ctm_coeff[i] >> 16;
				for (i = 0; i < SDE_CSC_BIAS_SIZE; ++i) {
					psde->csc_cfg.csc_pre_bv[i] =
						csc->v1.pre_bias[i];
					psde->csc_cfg.csc_post_bv[i] =
						csc->v1.post_bias[i];
				}
				for (i = 0; i < SDE_CSC_CLAMP_SIZE; ++i) {
					psde->csc_cfg.csc_pre_lv[i] =
						csc->v1.pre_clamp[i];
					psde->csc_cfg.csc_post_lv[i] =
						csc->v1.post_clamp[i];
				}
				psde->csc_ptr = &psde->csc_cfg;
			}
			break;
		default:
			break;
		}
		if (!psde->csc_ptr)
			SDE_ERROR("invalid csc blob, v%lld\n", csc->version);
	}

	/* revert to kernel default if override not available */
	if (psde->csc_ptr)
		SDE_DEBUG("user blob override for csc\n");
	else if (SDE_FORMAT_IS_YUV(fmt))
		psde->csc_ptr = (struct sde_csc_cfg *)&sde_csc_YUV2RGB_601L;
}

static void _sde_plane_setup_scaler(struct sde_plane *psde,
		const struct sde_format *fmt,
		struct sde_plane_state *pstate)
{
	struct sde_hw_pixel_ext *pe = NULL;
	struct sde_drm_scaler *sc_u = NULL;
	struct sde_drm_scaler_v1 *sc_u1 = NULL;
	size_t sc_u_size = 0;
	uint32_t chroma_subsmpl_h, chroma_subsmpl_v;
	uint32_t tmp;
	int i;

	if (!psde || !fmt)
		return;

	pe = &(psde->pixel_ext);
	memset(pe, 0, sizeof(struct sde_hw_pixel_ext));

	/* get scaler config from user space */
	if (pstate)
		sc_u = msm_property_get_blob(&psde->property_info,
				pstate->property_blobs,
				&sc_u_size,
				PLANE_PROP_SCALER);
	if (sc_u) {
		switch (sc_u->version) {
		case SDE_DRM_SCALER_V1:
			if (!_sde_plane_verify_blob(sc_u,
						sc_u_size,
						&sc_u->v1,
						sizeof(*sc_u1)))
				sc_u1 = &sc_u->v1;
			break;
		default:
			SDE_DEBUG("unrecognized scaler blob v%lld\n",
							sc_u->version);
			break;
		}
	}

	/* decimation */
	if (sc_u1 && (sc_u1->enable & SDE_DRM_SCALER_DECIMATE)) {
		psde->pipe_cfg.horz_decimation = sc_u1->horz_decimate;
		psde->pipe_cfg.vert_decimation = sc_u1->vert_decimate;
	} else {
		psde->pipe_cfg.horz_decimation = 0;
		psde->pipe_cfg.vert_decimation = 0;
	}

	/* don't chroma subsample if decimating */
	chroma_subsmpl_h = psde->pipe_cfg.horz_decimation ? 1 :
		drm_format_horz_chroma_subsampling(fmt->base.pixel_format);
	chroma_subsmpl_v = psde->pipe_cfg.vert_decimation ? 1 :
		drm_format_vert_chroma_subsampling(fmt->base.pixel_format);

	/* update scaler */
	if (psde->features & BIT(SDE_SSPP_SCALER_QSEED3)) {
		if (sc_u1 && (sc_u1->enable & SDE_DRM_SCALER_SCALER_3))
			SDE_DEBUG("SCALER3 blob detected\n");
		else
			_sde_plane_setup_scaler3(psde,
					psde->pipe_cfg.src_rect.w,
					psde->pipe_cfg.src_rect.h,
					psde->pipe_cfg.dst_rect.w,
					psde->pipe_cfg.dst_rect.h,
					&psde->scaler3_cfg, fmt,
					chroma_subsmpl_h, chroma_subsmpl_v);
	} else {
		/* always calculate basic scaler config */
		if (sc_u1 && (sc_u1->enable & SDE_DRM_SCALER_SCALER_2)) {
			/* populate from user space */
			for (i = 0; i < SDE_MAX_PLANES; i++) {
				pe->init_phase_x[i] = sc_u1->init_phase_x[i];
				pe->phase_step_x[i] = sc_u1->phase_step_x[i];
				pe->init_phase_y[i] = sc_u1->init_phase_y[i];
				pe->phase_step_y[i] = sc_u1->phase_step_y[i];

				pe->horz_filter[i] = sc_u1->horz_filter[i];
				pe->vert_filter[i] = sc_u1->vert_filter[i];
			}
		} else {
			/* calculate phase steps */
			_sde_plane_setup_scaler2(psde,
					psde->pipe_cfg.src_rect.w,
					psde->pipe_cfg.dst_rect.w,
					pe->phase_step_x,
					pe->horz_filter, fmt, chroma_subsmpl_h);
			_sde_plane_setup_scaler2(psde,
					psde->pipe_cfg.src_rect.h,
					psde->pipe_cfg.dst_rect.h,
					pe->phase_step_y,
					pe->vert_filter, fmt, chroma_subsmpl_v);
		}
	}

	/* update pixel extensions */
	if (sc_u1 && (sc_u1->enable & SDE_DRM_SCALER_PIX_EXT)) {
		/* populate from user space */
		SDE_DEBUG("pixel ext blob detected\n");
		for (i = 0; i < SDE_MAX_PLANES; i++) {
			pe->num_ext_pxls_left[i] = sc_u1->lr.num_pxls_start[i];
			pe->num_ext_pxls_right[i] = sc_u1->lr.num_pxls_end[i];
			pe->left_ftch[i] = sc_u1->lr.ftch_start[i];
			pe->right_ftch[i] = sc_u1->lr.ftch_end[i];
			pe->left_rpt[i] = sc_u1->lr.rpt_start[i];
			pe->right_rpt[i] = sc_u1->lr.rpt_end[i];
			pe->roi_w[i] = sc_u1->lr.roi[i];

			pe->num_ext_pxls_top[i] = sc_u1->tb.num_pxls_start[i];
			pe->num_ext_pxls_btm[i] = sc_u1->tb.num_pxls_end[i];
			pe->top_ftch[i] = sc_u1->tb.ftch_start[i];
			pe->btm_ftch[i] = sc_u1->tb.ftch_end[i];
			pe->top_rpt[i] = sc_u1->tb.rpt_start[i];
			pe->btm_rpt[i] = sc_u1->tb.rpt_end[i];
			pe->roi_h[i] = sc_u1->tb.roi[i];
		}
	} else {
		/* calculate left/right/top/bottom pixel extensions */
		tmp = DECIMATED_DIMENSION(psde->pipe_cfg.src_rect.w,
				psde->pipe_cfg.horz_decimation);
		if (SDE_FORMAT_IS_YUV(fmt))
			tmp &= ~0x1;
		_sde_plane_setup_pixel_ext(psde, psde->pipe_cfg.src_rect.w,
				psde->pipe_cfg.dst_rect.w, tmp,
				pe->phase_step_x,
				pe->roi_w,
				pe->num_ext_pxls_left,
				pe->num_ext_pxls_right, pe->horz_filter, fmt,
				chroma_subsmpl_h, 0);

		tmp = DECIMATED_DIMENSION(psde->pipe_cfg.src_rect.h,
				psde->pipe_cfg.vert_decimation);
		_sde_plane_setup_pixel_ext(psde, psde->pipe_cfg.src_rect.h,
				psde->pipe_cfg.dst_rect.h, tmp,
				pe->phase_step_y,
				pe->roi_h,
				pe->num_ext_pxls_top,
				pe->num_ext_pxls_btm, pe->vert_filter, fmt,
				chroma_subsmpl_v, 1);

		for (i = 0; i < SDE_MAX_PLANES; i++) {
			if (pe->num_ext_pxls_left[i] >= 0)
				pe->left_rpt[i] =
					pe->num_ext_pxls_left[i];
			else
				pe->left_ftch[i] =
					pe->num_ext_pxls_left[i];

			if (pe->num_ext_pxls_right[i] >= 0)
				pe->right_rpt[i] =
					pe->num_ext_pxls_right[i];
			else
				pe->right_ftch[i] =
					pe->num_ext_pxls_right[i];

			if (pe->num_ext_pxls_top[i] >= 0)
				pe->top_rpt[i] =
					pe->num_ext_pxls_top[i];
			else
				pe->top_ftch[i] =
					pe->num_ext_pxls_top[i];

			if (pe->num_ext_pxls_btm[i] >= 0)
				pe->btm_rpt[i] =
					pe->num_ext_pxls_btm[i];
			else
				pe->btm_ftch[i] =
					pe->num_ext_pxls_btm[i];
		}
	}
}

/**
 * _sde_plane_color_fill - enables color fill on plane
 * @plane:  Pointer to DRM plane object
 * @color:  RGB fill color value, [23..16] Blue, [15..8] Green, [7..0] Red
 * @alpha:  8-bit fill alpha value, 255 selects 100% alpha
 * Returns: 0 on success
 */
static int _sde_plane_color_fill(struct drm_plane *plane,
		uint32_t color, uint32_t alpha)
{
	struct sde_plane *psde;
	const struct sde_format *fmt;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);
	if (!psde->pipe_hw) {
		SDE_ERROR("invalid plane h/w pointer\n");
		return -EINVAL;
	}

	DBG("");

	/*
	 * select fill format to match user property expectation,
	 * h/w only supports RGB variants
	 */
	fmt = sde_get_sde_format(DRM_FORMAT_ABGR8888);

	/* update sspp */
	if (fmt && psde->pipe_hw->ops.setup_solidfill) {
		psde->pipe_hw->ops.setup_solidfill(psde->pipe_hw,
				(color & 0xFFFFFF) | ((alpha & 0xFF) << 24));

		/* override scaler/decimation if solid fill */
		psde->pipe_cfg.src_rect.x = 0;
		psde->pipe_cfg.src_rect.y = 0;
		psde->pipe_cfg.src_rect.w = psde->pipe_cfg.dst_rect.w;
		psde->pipe_cfg.src_rect.h = psde->pipe_cfg.dst_rect.h;

		_sde_plane_setup_scaler(psde, fmt, 0);

		if (psde->pipe_hw->ops.setup_format)
			psde->pipe_hw->ops.setup_format(psde->pipe_hw,
					fmt, SDE_SSPP_SOLID_FILL);

		if (psde->pipe_hw->ops.setup_rects)
			psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
					&psde->pipe_cfg, &psde->pixel_ext);
	}

	return 0;
}

static int _sde_plane_mode_set(struct drm_plane *plane,
				struct drm_plane_state *state)
{
	uint32_t nplanes, src_flags;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	const struct sde_format *fmt;
	struct drm_crtc *crtc;
	struct drm_framebuffer *fb;
	struct sde_rect src, dst;
	bool q16_data = true;

	if (!plane || !plane->state) {
		SDE_ERROR("invalid plane/state\n");
		return -EINVAL;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(plane->state);

	crtc = state->crtc;
	fb = state->fb;
	if (!crtc || !fb) {
		SDE_ERROR("invalid crtc/fb\n");
		return -EINVAL;
	}
	fmt = to_sde_format(msm_framebuffer_format(fb));
	nplanes = fmt->num_planes;

	POPULATE_RECT(&src, state->src_x, state->src_y,
		state->src_w, state->src_h, q16_data);
	POPULATE_RECT(&dst, state->crtc_x, state->crtc_y,
		state->crtc_w, state->crtc_h, !q16_data);

	SDE_DEBUG("%s:FB[%u] %u,%u,%u,%u -> CRTC[%u] %d,%d,%u,%u, %s ubwc %d\n",
			psde->pipe_name,
			fb->base.id, src.x, src.y, src.w, src.h,
			crtc->base.id, dst.x, dst.y, dst.w, dst.h,
			drm_get_format_name(fmt->base.pixel_format),
			SDE_FORMAT_IS_UBWC(fmt));

	/* update format configuration */
	memset(&(psde->pipe_cfg), 0, sizeof(struct sde_hw_pipe_cfg));
	src_flags = 0;

	/* flags */
	SDE_DEBUG("flags 0x%llX, rotation 0x%llX\n",
			sde_plane_get_property(pstate, PLANE_PROP_SRC_CONFIG),
			sde_plane_get_property(pstate, PLANE_PROP_ROTATION));
	if (sde_plane_get_property(pstate, PLANE_PROP_ROTATION) &
		BIT(DRM_REFLECT_X))
		src_flags |= SDE_SSPP_FLIP_LR;
	if (sde_plane_get_property(pstate, PLANE_PROP_ROTATION) &
		BIT(DRM_REFLECT_Y))
		src_flags |= SDE_SSPP_FLIP_UD;
	if (sde_plane_get_property(pstate, PLANE_PROP_SRC_CONFIG) &
		BIT(SDE_DRM_DEINTERLACE)) {
		src.h /= 2;
		src.y  = DIV_ROUND_UP(src.y, 2);
		src.y &= ~0x1;
	}

	psde->pipe_cfg.src_rect = src;
	psde->pipe_cfg.dst_rect = dst;

	/* check for color fill */
	psde->color_fill = (uint32_t)sde_plane_get_property(pstate,
			PLANE_PROP_COLOR_FILL);
	if (psde->color_fill & SDE_PLANE_COLOR_FILL_FLAG)
		/* skip remaining processing on color fill */
		return 0;

	_sde_plane_set_scanout(plane, pstate, &psde->pipe_cfg, fb);

	_sde_plane_setup_scaler(psde, fmt, pstate);

	if (psde->pipe_hw->ops.setup_format)
		psde->pipe_hw->ops.setup_format(psde->pipe_hw,
				fmt, src_flags);
	if (psde->pipe_hw->ops.setup_rects)
		psde->pipe_hw->ops.setup_rects(psde->pipe_hw,
				&psde->pipe_cfg, &psde->pixel_ext);

	/* update sharpening */
	psde->sharp_cfg.strength = SHARP_STRENGTH_DEFAULT;
	psde->sharp_cfg.edge_thr = SHARP_EDGE_THR_DEFAULT;
	psde->sharp_cfg.smooth_thr = SHARP_SMOOTH_THR_DEFAULT;
	psde->sharp_cfg.noise_thr = SHARP_NOISE_THR_DEFAULT;

	if (psde->pipe_hw->ops.setup_sharpening)
		psde->pipe_hw->ops.setup_sharpening(psde->pipe_hw,
			&psde->sharp_cfg);

	/* update csc */
	if (SDE_FORMAT_IS_YUV(fmt))
		_sde_plane_setup_csc(psde, pstate, fmt);
	else
		psde->csc_ptr = 0;

	return 0;
}

static int sde_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb = new_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!new_state->fb)
		return 0;

	SDE_DEBUG("%s: FB[%u]\n", psde->pipe_name, fb->base.id);
	return msm_framebuffer_prepare(fb, psde->mmu_id);
}

static void sde_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state->fb;
	struct sde_plane *psde = to_sde_plane(plane);

	if (!fb)
		return;

	SDE_DEBUG("%s: FB[%u]\n", psde->pipe_name, fb->base.id);
	msm_framebuffer_cleanup(fb, psde->mmu_id);
}

static void _sde_plane_atomic_check_mode_changed(struct sde_plane *psde,
		struct drm_plane_state *state,
		struct drm_plane_state *old_state)
{
	struct sde_plane_state *pstate = to_sde_plane_state(state);

	/* no need to check it again */
	if (pstate->mode_changed)
		return;

	if (!(sde_plane_enabled(state) && sde_plane_enabled(old_state))) {
		SDE_DEBUG("%s: pipe enabling/disabling full modeset required\n",
			psde->pipe_name);
		pstate->mode_changed = true;
	} else if (to_sde_plane_state(old_state)->pending) {
		SDE_DEBUG("%s: still pending\n", psde->pipe_name);
		pstate->mode_changed = true;
	} else if (state->src_w != old_state->src_w ||
		   state->src_h != old_state->src_h ||
		   state->src_x != old_state->src_x ||
		   state->src_y != old_state->src_y) {
		SDE_DEBUG("%s: src rect updated\n", psde->pipe_name);
		pstate->mode_changed = true;
	} else if (state->crtc_w != old_state->crtc_w ||
		   state->crtc_h != old_state->crtc_h ||
		   state->crtc_x != old_state->crtc_x ||
		   state->crtc_y != old_state->crtc_y) {
		SDE_DEBUG("%s: crtc rect updated\n", psde->pipe_name);
		pstate->mode_changed = true;
	} else if (state->fb->pixel_format != old_state->fb->pixel_format) {
		SDE_DEBUG("%s: format change!\n", psde->pipe_name);
		pstate->mode_changed = true;
	} else {
		uint64_t *new_mods = state->fb->modifier;
		uint64_t *old_mods = old_state->fb->modifier;
		uint32_t *new_pitches = state->fb->pitches;
		uint32_t *old_pitches = old_state->fb->pitches;
		uint32_t *new_offset = state->fb->offsets;
		uint32_t *old_offset = old_state->fb->offsets;
		int i;

		for (i = 0; i < ARRAY_SIZE(state->fb->modifier); i++) {
			if (new_mods[i] != old_mods[i]) {
				SDE_DEBUG("%s: format modifiers change\"\
					plane:%d new_mode:%llu old_mode:%llu\n",
					psde->pipe_name, i, new_mods[i],
					old_mods[i]);
				pstate->mode_changed = true;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(state->fb->pitches); i++) {
			if (new_pitches[i] != old_pitches[i]) {
				SDE_DEBUG("%s: pitches change plane:%d\"\
					old_pitches:%u new_pitches:%u\n",
					psde->pipe_name, i, old_pitches[i],
					new_pitches[i]);
				pstate->mode_changed = true;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(state->fb->offsets); i++) {
			if (new_offset[i] != old_offset[i]) {
				SDE_DEBUG("%s: offset change plane:%d\"\
					old_offset:%u new_offset:%u\n",
					psde->pipe_name, i, old_offset[i],
					new_offset[i]);
				pstate->mode_changed = true;
				break;
			}
		}
	}
}

static bool __get_scale_data(struct sde_plane *psde,
	struct sde_plane_state *pstate, struct sde_drm_scaler *sc_u,
	size_t *sc_u_size)
{
	bool valid_flag = false;

	sc_u = msm_property_get_blob(&psde->property_info,
			pstate->property_blobs,
			sc_u_size,
			PLANE_PROP_SCALER);
	if (sc_u) {
		switch (sc_u->version) {
		case SDE_DRM_SCALER_V1:
			if (!_sde_plane_verify_blob(sc_u, *sc_u_size,
				&sc_u->v1, sizeof(struct sde_drm_scaler_v1)))
				valid_flag = true;
			break;
		default:
			SDE_DEBUG("unrecognized scaler blob v%lld\n",
							sc_u->version);
			break;
		}
	}

	return valid_flag;
}

static int sde_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	int ret = 0, valid_scale_data;
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	const struct sde_format *fmt;
	size_t sc_u_size = 0;
	struct sde_drm_scaler *sc_u = NULL;
	struct sde_rect src, dst;
	uint32_t deci_w, deci_h, src_deci_w, src_deci_h;
	uint32_t max_upscale, max_downscale, min_src_size, max_linewidth;
	bool q16_data = true;

	if (!plane || !state) {
		SDE_ERROR("invalid plane/state\n");
		ret = -EINVAL;
		goto exit;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	if (!psde->pipe_sblk) {
		SDE_ERROR("invalid plane catalog\n");
		ret = -EINVAL;
		goto exit;
	}

	valid_scale_data = __get_scale_data(psde, pstate, sc_u, &sc_u_size);
	deci_w = valid_scale_data && sc_u ? sc_u->v1.horz_decimate : 0;
	deci_h = valid_scale_data && sc_u ? sc_u->v1.vert_decimate : 0;

	/* src values are in Q16 fixed point, convert to integer */
	POPULATE_RECT(&src, state->src_x, state->src_y, state->src_w,
		state->src_h, q16_data);
	POPULATE_RECT(&dst, state->crtc_x, state->crtc_y, state->crtc_w,
		state->crtc_h, !q16_data);

	src_deci_w = DECIMATED_DIMENSION(src.w, deci_w);
	src_deci_h = DECIMATED_DIMENSION(src.h, deci_h);

	max_upscale = psde->pipe_sblk->maxupscale;
	max_downscale = psde->pipe_sblk->maxdwnscale;
	max_linewidth = psde->pipe_sblk->maxlinewidth;

	SDE_DEBUG("%s: check (%d -> %d)\n", psde->pipe_name,
		sde_plane_enabled(plane->state), sde_plane_enabled(state));

	if (!sde_plane_enabled(state))
		goto modeset_update;

	fmt = to_sde_format(msm_framebuffer_format(state->fb));

	min_src_size = SDE_FORMAT_IS_YUV(fmt) ? 2 : 1;

	if (SDE_FORMAT_IS_YUV(fmt) &&
		(!(psde->features & SDE_SSPP_SCALER) ||
		 !(psde->features & BIT(SDE_SSPP_CSC)))) {
		SDE_ERROR("plane doesn't have scaler/csc capability for yuv\n");
		ret = -EINVAL;

	/* check src bounds */
	} else if (state->fb->width > MAX_IMG_WIDTH ||
		state->fb->height > MAX_IMG_HEIGHT ||
		src.w < min_src_size || src.h < min_src_size ||
		CHECK_LAYER_BOUNDS(src.x, src.w, state->fb->width) ||
		CHECK_LAYER_BOUNDS(src.y, src.h, state->fb->height)) {
		SDE_ERROR("invalid source (%u, %u) -> (%u, %u)\n",
			src.x, src.y, src.w, src.h);
		ret = -E2BIG;

	/* valid yuv image */
	} else if (SDE_FORMAT_IS_YUV(fmt) && ((src.x & 0x1) || (src.y & 0x1) ||
			 (src.w & 0x1) || (src.h & 0x1))) {
		SDE_ERROR("invalid yuv source (%u, %u) -> (%u, %u)\n",
				src.x, src.y, src.w, src.h);
		ret = -EINVAL;

	/* min dst support */
	} else if (dst.w < 0x1 || dst.h < 0x1) {
		SDE_ERROR("invalid dest rect (%u, %u) -> (%u, %u)\n",
				dst.x, dst.y, dst.w, dst.h);
		ret = -EINVAL;

	/* decimation validation */
	} else if (deci_w || deci_h) {
		if ((deci_w > psde->pipe_sblk->maxhdeciexp) ||
			(deci_h > psde->pipe_sblk->maxvdeciexp)) {
			SDE_ERROR("too much decimation requested\n");
			ret = -EINVAL;
		} else if (fmt->fetch_mode != SDE_FETCH_LINEAR) {
			SDE_ERROR("decimation requires linear fetch\n");
			ret = -EINVAL;
		}

	} else if (!(psde->features & SDE_SSPP_SCALER) &&
		((src.w != dst.w) || (src.h != dst.h))) {
		SDE_ERROR("pipe doesn't support scaling %ux%u->%ux%u\n",
			src.w, src.h, dst.w, dst.h);
		ret = -EINVAL;

	/* check decimated source width */
	} else if (src_deci_w > max_linewidth) {
		SDE_ERROR("invalid source width:%u, deci wid:%u, line wid:%u\n",
				src.w, src_deci_w, max_linewidth);
		ret = -E2BIG;

	/* check max scaler capability */
	} else if (((src_deci_w * max_upscale) < dst.w) ||
		((src_deci_h * max_upscale) < dst.h) ||
		((dst.w * max_downscale) < src_deci_w) ||
		((dst.h * max_downscale) < src_deci_h)) {
		SDE_ERROR("too much scaling requested %ux%u -> %ux%u\n",
			src_deci_w, src_deci_h, dst.w, dst.h);
		ret = -E2BIG;
	}

modeset_update:
	if (!ret)
		_sde_plane_atomic_check_mode_changed(psde, state, plane->state);
exit:
	return ret;
}

/**
 * sde_plane_flush - final plane operations before commit flush
 * @plane: Pointer to drm plane structure
 */
void sde_plane_flush(struct drm_plane *plane)
{
	struct sde_plane *psde;

	if (!plane)
		return;

	psde = to_sde_plane(plane);

	/*
	 * These updates have to be done immediately before the plane flush
	 * timing, and may not be moved to the atomic_update/mode_set functions.
	 */
	if (psde->is_error)
		/* force white frame with 0% alpha pipe output on error */
		_sde_plane_color_fill(plane, 0xFFFFFF, 0x0);
	else if (psde->color_fill & SDE_PLANE_COLOR_FILL_FLAG)
		/* force 100% alpha */
		_sde_plane_color_fill(plane, psde->color_fill, 0xFF);
	else if (psde->pipe_hw && psde->csc_ptr && psde->pipe_hw->ops.setup_csc)
		psde->pipe_hw->ops.setup_csc(psde->pipe_hw, psde->csc_ptr);

	/* flag h/w flush complete */
	if (plane->state)
		to_sde_plane_state(plane->state)->pending = false;
}

static void sde_plane_atomic_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct sde_plane *sde_plane;
	struct drm_plane_state *state;
	struct sde_plane_state *pstate;

	if (!plane || !plane->state) {
		SDE_ERROR("invalid plane/state\n");
		return;
	}

	sde_plane = to_sde_plane(plane);
	sde_plane->is_error = false;
	state = plane->state;
	pstate = to_sde_plane_state(state);

	SDE_DEBUG("%s: update\n", sde_plane->pipe_name);

	if (!sde_plane_enabled(state)) {
		pstate->pending = true;
	} else if (pstate->mode_changed) {
		int ret;

		pstate->pending = true;
		ret = _sde_plane_mode_set(plane, state);
		/* atomic_check should have ensured that this doesn't fail */
		WARN_ON(ret < 0);
	} else {
		_sde_plane_set_scanout(plane, pstate,
				&sde_plane->pipe_cfg, state->fb);
	}
}


/* helper to install properties which are common to planes and crtcs */
static void _sde_plane_install_properties(struct drm_plane *plane,
	u32 max_blendstages)
{
	static const struct drm_prop_enum_list e_blend_op[] = {
		{SDE_DRM_BLEND_OP_NOT_DEFINED,    "not_defined"},
		{SDE_DRM_BLEND_OP_OPAQUE,         "opaque"},
		{SDE_DRM_BLEND_OP_PREMULTIPLIED,  "premultiplied"},
		{SDE_DRM_BLEND_OP_COVERAGE,       "coverage"}
	};
	static const struct drm_prop_enum_list e_src_config[] = {
		{SDE_DRM_DEINTERLACE, "deinterlace"}
	};
	const struct sde_format_extended *format_list;
	struct sde_kms_info *info;
	struct sde_plane *psde = to_sde_plane(plane);
	u32 default_blendstages;

	if (!plane || !psde || !psde->pipe_hw || !psde->pipe_sblk) {
		SDE_ERROR("Invalid argument(s)\n");
		return;
	}

	default_blendstages = plane->type == DRM_PLANE_TYPE_PRIMARY ?
			STAGE_BASE : STAGE0 + drm_plane_index(plane);
	msm_property_install_range(&psde->property_info, "zpos", 0x0, 0,
		max_blendstages, default_blendstages, PLANE_PROP_ZPOS);

	msm_property_install_range(&psde->property_info, "alpha",
		0x0, 0, 255, 255, PLANE_PROP_ALPHA);

	/* linux default file descriptor range on each process */
	msm_property_install_range(&psde->property_info, "input_fence",
		0x0, 0, INR_OPEN_MAX, 0, PLANE_PROP_INPUT_FENCE);

	/* standard properties */
	msm_property_install_rotation(&psde->property_info,
		BIT(DRM_REFLECT_X) | BIT(DRM_REFLECT_Y), PLANE_PROP_ROTATION);

	msm_property_install_enum(&psde->property_info, "blend_op", 0x0, 0,
		e_blend_op, ARRAY_SIZE(e_blend_op), PLANE_PROP_BLEND_OP);

	msm_property_install_enum(&psde->property_info, "src_config", 0x0, 1,
		e_src_config, ARRAY_SIZE(e_src_config), PLANE_PROP_SRC_CONFIG);

	if (psde->pipe_hw->ops.setup_solidfill)
		msm_property_install_range(&psde->property_info, "color_fill",
				0, 0, 0xFFFFFFFF, 0, PLANE_PROP_COLOR_FILL);

	if (psde->features & SDE_SSPP_SCALER)
		msm_property_install_blob(&psde->property_info, "scaler", 0,
			PLANE_PROP_SCALER);

	if (psde->features & BIT(SDE_SSPP_CSC))
		msm_property_install_blob(&psde->property_info, "csc", 0,
			PLANE_PROP_CSC);

	info = kzalloc(sizeof(struct sde_kms_info), GFP_KERNEL);
	if (!info)
		return;

	msm_property_install_blob(&psde->property_info, "capabilities",
		DRM_MODE_PROP_IMMUTABLE, PLANE_PROP_INFO);
	sde_kms_info_reset(info);

	format_list = psde->pipe_sblk->format_list;
	if (format_list) {
		sde_kms_info_start(info, "pixel_formats");
		while (format_list->fourcc_format) {
			sde_kms_info_append_format(info,
					format_list->fourcc_format,
					format_list->modifier);
			++format_list;
		}
		sde_kms_info_stop(info);
	}

	sde_kms_info_add_keyint(info, "max_linewidth",
			psde->pipe_sblk->maxlinewidth);
	sde_kms_info_add_keyint(info, "max_upscale",
			psde->pipe_sblk->maxupscale);
	sde_kms_info_add_keyint(info, "max_downscale",
			psde->pipe_sblk->maxdwnscale);
	sde_kms_info_add_keyint(info, "max_horizontal_deci",
			psde->pipe_sblk->maxhdeciexp);
	sde_kms_info_add_keyint(info, "max_vertical_deci",
			psde->pipe_sblk->maxvdeciexp);
	msm_property_set_blob(&psde->property_info, &psde->blob_info,
			info->data, info->len, PLANE_PROP_INFO);

	kfree(info);
}

static int sde_plane_atomic_set_property(struct drm_plane *plane,
		struct drm_plane_state *state, struct drm_property *property,
		uint64_t val)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	int idx, ret = -EINVAL;

	DBG("");

	if (!plane) {
		SDE_ERROR("invalid plane\n");
	} else if (!state) {
		SDE_ERROR("invalid state\n");
	} else {
		psde = to_sde_plane(plane);
		pstate = to_sde_plane_state(state);
		ret = msm_property_atomic_set(&psde->property_info,
				pstate->property_values, pstate->property_blobs,
				property, val);
		if (!ret) {
			idx = msm_property_index(&psde->property_info,
					property);
			if (idx == PLANE_PROP_INPUT_FENCE)
				_sde_plane_set_input_fence(plane, pstate, val);
		}
	}

	return ret;
}

static int sde_plane_set_property(struct drm_plane *plane,
		struct drm_property *property, uint64_t val)
{
	DBG("");

	return sde_plane_atomic_set_property(plane,
			plane->state, property, val);
}

static int sde_plane_atomic_get_property(struct drm_plane *plane,
		const struct drm_plane_state *state,
		struct drm_property *property, uint64_t *val)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	int ret = -EINVAL;

	DBG("");

	if (!plane) {
		SDE_ERROR("invalid plane\n");
	} else if (!state) {
		SDE_ERROR("invalid state\n");
	} else {
		psde = to_sde_plane(plane);
		pstate = to_sde_plane_state(state);
		ret = msm_property_atomic_get(&psde->property_info,
				pstate->property_values, pstate->property_blobs,
				property, val);
	}

	return ret;
}

static void sde_plane_destroy(struct drm_plane *plane)
{
	struct sde_plane *psde;

	DBG("");

	if (plane) {
		psde = to_sde_plane(plane);

		debugfs_remove_recursive(psde->debugfs_root);

		if (psde->blob_info)
			drm_property_unreference_blob(psde->blob_info);
		msm_property_destroy(&psde->property_info);
		mutex_destroy(&psde->lock);

		drm_plane_helper_disable(plane);

		/* this will destroy the states as well */
		drm_plane_cleanup(plane);

		if (psde->pipe_hw)
			sde_hw_sspp_destroy(psde->pipe_hw);

		kfree(psde);
	}
}

static void sde_plane_destroy_state(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane || !state) {
		SDE_ERROR("invalid plane/state\n");
		return;
	}

	psde = to_sde_plane(plane);
	pstate = to_sde_plane_state(state);

	DBG("");

	/* remove ref count for frame buffers */
	if (state->fb)
		drm_framebuffer_unreference(state->fb);

	/* remove ref count for fence */
	if (pstate->input_fence)
		sde_sync_put(pstate->input_fence);

	/* destroy value helper */
	msm_property_destroy_state(&psde->property_info, pstate,
			pstate->property_values, pstate->property_blobs);
}

static struct drm_plane_state *
sde_plane_duplicate_state(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;
	struct sde_plane_state *old_state;

	if (!plane || !plane->state)
		return NULL;

	old_state = to_sde_plane_state(plane->state);
	psde = to_sde_plane(plane);
	pstate = msm_property_alloc_state(&psde->property_info);
	if (!pstate)
		return NULL;

	DBG("");

	/* duplicate value helper */
	msm_property_duplicate_state(&psde->property_info, old_state, pstate,
			pstate->property_values, pstate->property_blobs);

	/* add ref count for frame buffer */
	if (pstate->base.fb)
		drm_framebuffer_reference(pstate->base.fb);

	/* add ref count for fence */
	if (pstate->input_fence) {
		pstate->input_fence = 0;
		_sde_plane_set_input_fence(plane, pstate, pstate->
				property_values[PLANE_PROP_INPUT_FENCE]);
	}

	pstate->mode_changed = false;
	pstate->pending = false;

	return &pstate->base;
}

static void sde_plane_reset(struct drm_plane *plane)
{
	struct sde_plane *psde;
	struct sde_plane_state *pstate;

	if (!plane) {
		SDE_ERROR("invalid plane\n");
		return;
	}

	psde = to_sde_plane(plane);
	SDE_DEBUG("%s\n", psde->pipe_name);

	/* remove previous state, if present */
	if (plane->state) {
		sde_plane_destroy_state(plane, plane->state);
		plane->state = 0;
	}

	pstate = msm_property_alloc_state(&psde->property_info);
	if (!pstate)
		return;

	/* reset value helper */
	msm_property_reset_state(&psde->property_info, pstate,
			pstate->property_values, pstate->property_blobs);

	pstate->base.plane = plane;

	plane->state = &pstate->base;
}

static const struct drm_plane_funcs sde_plane_funcs = {
		.update_plane = drm_atomic_helper_update_plane,
		.disable_plane = drm_atomic_helper_disable_plane,
		.destroy = sde_plane_destroy,
		.set_property = sde_plane_set_property,
		.atomic_set_property = sde_plane_atomic_set_property,
		.atomic_get_property = sde_plane_atomic_get_property,
		.reset = sde_plane_reset,
		.atomic_duplicate_state = sde_plane_duplicate_state,
		.atomic_destroy_state = sde_plane_destroy_state,
};

static const struct drm_plane_helper_funcs sde_plane_helper_funcs = {
		.prepare_fb = sde_plane_prepare_fb,
		.cleanup_fb = sde_plane_cleanup_fb,
		.atomic_check = sde_plane_atomic_check,
		.atomic_update = sde_plane_atomic_update,
};

enum sde_sspp sde_plane_pipe(struct drm_plane *plane)
{
	struct sde_plane *sde_plane = to_sde_plane(plane);

	return sde_plane->pipe;
}

static void _sde_plane_init_debugfs(struct sde_plane *psde, struct sde_kms *kms)
{
	const struct sde_sspp_sub_blks *sblk = 0;
	const struct sde_sspp_cfg *cfg = 0;

	if (psde && psde->pipe_hw)
		cfg = psde->pipe_hw->cap;
	if (cfg)
		sblk = cfg->sblk;

	if (kms && sblk) {
		/* create overall sub-directory for the pipe */
		psde->debugfs_root =
			debugfs_create_dir(psde->pipe_name,
					sde_debugfs_get_root(kms));
		if (psde->debugfs_root) {
			/* don't error check these */
			debugfs_create_x32("features", S_IRUGO | S_IWUSR,
					psde->debugfs_root, &psde->features);

			/* add register dump support */
			sde_debugfs_setup_regset32(&psde->debugfs_src,
					sblk->src_blk.base + cfg->base,
					sblk->src_blk.len,
					kms->mmio);
			sde_debugfs_create_regset32("src_blk", S_IRUGO,
					psde->debugfs_root, &psde->debugfs_src);

			sde_debugfs_setup_regset32(&psde->debugfs_scaler,
					sblk->scaler_blk.base + cfg->base,
					sblk->scaler_blk.len,
					kms->mmio);
			sde_debugfs_create_regset32("scaler_blk", S_IRUGO,
					psde->debugfs_root,
					&psde->debugfs_scaler);

			sde_debugfs_setup_regset32(&psde->debugfs_csc,
					sblk->csc_blk.base + cfg->base,
					sblk->csc_blk.len,
					kms->mmio);
			sde_debugfs_create_regset32("csc_blk", S_IRUGO,
					psde->debugfs_root, &psde->debugfs_csc);
		}
	}
}

/* initialize plane */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool primary_plane,
		unsigned long possible_crtcs)
{
	struct drm_plane *plane = NULL;
	struct sde_plane *psde;
	struct msm_drm_private *priv;
	struct sde_kms *kms;
	enum drm_plane_type type;
	int ret = -EINVAL, max_blendstages = 255;

	if (!dev) {
		SDE_ERROR("[%u]device is NULL\n", pipe);
		goto exit;
	}

	priv = dev->dev_private;
	if (!priv) {
		SDE_ERROR("[%u]private data is NULL\n", pipe);
		goto exit;
	}

	if (!priv->kms) {
		SDE_ERROR("[%u]invalid KMS reference\n", pipe);
		goto exit;
	}
	kms = to_sde_kms(priv->kms);

	if (!kms->catalog) {
		SDE_ERROR("[%u]invalid catalog reference\n", pipe);
		goto exit;
	}

	/* create and zero local structure */
	psde = kzalloc(sizeof(*psde), GFP_KERNEL);
	if (!psde) {
		SDE_ERROR("[%u]failed to allocate local plane struct\n", pipe);
		ret = -ENOMEM;
		goto exit;
	}

	/* cache local stuff for later */
	plane = &psde->base;
	psde->pipe = pipe;
	psde->mmu_id = kms->mmu_id[MSM_SMMU_DOMAIN_UNSECURE];

	/* initialize underlying h/w driver */
	psde->pipe_hw = sde_hw_sspp_init(pipe, kms->mmio, kms->catalog);
	if (IS_ERR(psde->pipe_hw)) {
		SDE_ERROR("[%u]SSPP init failed\n", pipe);
		ret = PTR_ERR(psde->pipe_hw);
		goto clean_plane;
	} else if (!psde->pipe_hw->cap || !psde->pipe_hw->cap->sblk) {
		SDE_ERROR("[%u]SSPP init returned invalid cfg\n", pipe);
		goto clean_sspp;
	}

	/* cache features mask for later */
	psde->features = psde->pipe_hw->cap->features;
	psde->pipe_sblk = psde->pipe_hw->cap->sblk;
	if (!psde->pipe_sblk) {
		SDE_ERROR("invalid sblk on pipe %d\n", pipe);
		goto clean_sspp;
	}

	if (kms->catalog && kms->catalog->mixer_count && kms->catalog->mixer)
		max_blendstages = kms->catalog->mixer[0].sblk->maxblendstages;

	/* add plane to DRM framework */
	psde->nformats = sde_populate_formats(psde->pipe_sblk->format_list,
			psde->formats,
			0,
			ARRAY_SIZE(psde->formats));

	if (!psde->nformats) {
		SDE_ERROR("[%u]no valid formats for plane\n", pipe);
		goto clean_sspp;
	}

	if (psde->features & BIT(SDE_SSPP_CURSOR))
		type = DRM_PLANE_TYPE_CURSOR;
	else if (primary_plane)
		type = DRM_PLANE_TYPE_PRIMARY;
	else
		type = DRM_PLANE_TYPE_OVERLAY;
	ret = drm_universal_plane_init(dev, plane, possible_crtcs,
			&sde_plane_funcs, psde->formats, psde->nformats, type);
	if (ret)
		goto clean_sspp;

	/* success! finalize initialization */
	drm_plane_helper_add(plane, &sde_plane_helper_funcs);

	msm_property_init(&psde->property_info, &plane->base, dev,
			priv->plane_property, psde->property_data,
			PLANE_PROP_COUNT, PLANE_PROP_BLOBCOUNT,
			sizeof(struct sde_plane_state));

	_sde_plane_install_properties(plane, max_blendstages);

	/* save user friendly pipe name for later */
	snprintf(psde->pipe_name, SDE_NAME_SIZE, "plane%u", plane->base.id);

	mutex_init(&psde->lock);

	_sde_plane_init_debugfs(psde, kms);

	DRM_INFO("[%u]successfully created %s\n", pipe, psde->pipe_name);
	return plane;

clean_sspp:
	if (psde && psde->pipe_hw)
		sde_hw_sspp_destroy(psde->pipe_hw);
clean_plane:
	kfree(psde);
exit:
	return ERR_PTR(ret);
}

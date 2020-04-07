/* Copyright (c) 2012-2018, 2020, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/ion.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/major.h>
#include <media/msm_media_info.h>

#include <linux/dma-buf.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_formats.h"
#include "mdss_debug.h"
#include "mdss_smmu.h"
#include "mdss_panel.h"

#define PHY_ADDR_4G (1ULL<<32)
#define ALIGN_UP(x, align) ((DIV_ROUND_UP((x), (align))) * (align))

void mdss_mdp_format_flag_removal(u32 *table, u32 num, u32 remove_bits)
{
	struct mdss_mdp_format_params *fmt = NULL;
	int i, j;

	if (table == NULL) {
		pr_err("Null table provided\n");
		return;
	}

	for (i = 0; i < num; i++) {
		for (j = 0; j < ARRAY_SIZE(mdss_mdp_format_map); j++) {
			fmt = &mdss_mdp_format_map[i];
			if (table[i] == fmt->format) {
				fmt->flag &= ~remove_bits;
				break;
			}
		}
	}
}

#define SET_BIT(value, bit_num) \
	{ \
		value[bit_num >> 3] |= (1 << (bit_num & 7)); \
	}
static inline void __set_pipes_supported_fmt(struct mdss_mdp_pipe *pipe_list,
		int count, struct mdss_mdp_format_params *fmt)
{
	struct mdss_mdp_pipe *pipe = pipe_list;
	int i, j;

	for (i = 0; i < count; i++, pipe += j)
		for (j = 0; j < pipe->multirect.max_rects; j++)
			SET_BIT(pipe[j].supported_formats, fmt->format);
}

void mdss_mdp_set_supported_formats(struct mdss_data_type *mdata)
{
	struct mdss_mdp_writeback *wb = mdata->wb;
	bool has_tile = mdata->highest_bank_bit && !mdata->has_ubwc;
	bool has_ubwc = mdata->has_ubwc;
	int i;
	int j;

	for (i = 0; i < ARRAY_SIZE(mdss_mdp_format_map); i++) {
		struct mdss_mdp_format_params *fmt = &mdss_mdp_format_map[i];

		if ((fmt->fetch_mode == MDSS_MDP_FETCH_TILE && has_tile) ||
			(fmt->fetch_mode == MDSS_MDP_FETCH_LINEAR)) {
			if (fmt->unpack_dx_format &&
				!test_bit(MDSS_CAPS_10_BIT_SUPPORTED,
				mdata->mdss_caps_map))
				continue;

			__set_pipes_supported_fmt(mdata->vig_pipes,
					mdata->nvig_pipes, fmt);

			if (fmt->flag & VALID_ROT_WB_FORMAT) {
				for (j = 0; j < mdata->nwb; j++)
					SET_BIT(wb[j].supported_input_formats,
							fmt->format);
			}
			if (fmt->flag & VALID_MDP_WB_INTF_FORMAT) {
				for (j = 0; j < mdata->nwb; j++)
					SET_BIT(wb[j].supported_output_formats,
							fmt->format);
			}
			if (fmt->flag & VALID_MDP_CURSOR_FORMAT &&
					mdata->ncursor_pipes) {
				__set_pipes_supported_fmt(mdata->cursor_pipes,
						mdata->ncursor_pipes, fmt);
			}

			if (!fmt->is_yuv) {
				__set_pipes_supported_fmt(mdata->rgb_pipes,
						mdata->nrgb_pipes, fmt);
				__set_pipes_supported_fmt(mdata->dma_pipes,
						mdata->ndma_pipes, fmt);
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(mdss_mdp_format_ubwc_map) && has_ubwc; i++) {
		struct mdss_mdp_format_params *fmt =
			&mdss_mdp_format_ubwc_map[i].mdp_format;

		if (fmt->unpack_dx_format &&
			!test_bit(MDSS_CAPS_10_BIT_SUPPORTED,
			mdata->mdss_caps_map))
			continue;

		__set_pipes_supported_fmt(mdata->vig_pipes,
				mdata->nvig_pipes, fmt);

		if (fmt->flag & VALID_ROT_WB_FORMAT) {
			for (j = 0; j < mdata->nwb; j++)
				SET_BIT(wb[j].supported_input_formats,
						fmt->format);
		}
		if (fmt->flag & VALID_MDP_WB_INTF_FORMAT) {
			for (j = 0; j < mdata->nwb; j++)
				SET_BIT(wb[j].supported_output_formats,
						fmt->format);
		}
		if (fmt->flag & VALID_MDP_CURSOR_FORMAT &&
				mdata->ncursor_pipes) {
			__set_pipes_supported_fmt(mdata->cursor_pipes,
					mdata->ncursor_pipes, fmt);
		}

		if (!fmt->is_yuv) {
			__set_pipes_supported_fmt(mdata->rgb_pipes,
					mdata->nrgb_pipes, fmt);
			__set_pipes_supported_fmt(mdata->dma_pipes,
					mdata->ndma_pipes, fmt);
		}
	}
}

struct mdss_mdp_format_params *mdss_mdp_get_format_params(u32 format)
{
	struct mdss_mdp_format_params *fmt = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int i;
	bool fmt_found = false;

	for (i = 0; i < ARRAY_SIZE(mdss_mdp_format_map); i++) {
		fmt = &mdss_mdp_format_map[i];
		if (format == fmt->format) {
			fmt_found = true;
			break;
		}
	}

	if (!fmt_found) {
		for (i = 0; i < ARRAY_SIZE(mdss_mdp_format_ubwc_map); i++) {
			fmt = &mdss_mdp_format_ubwc_map[i].mdp_format;
			if (format == fmt->format)
				break;
		}
	}

	return (mdss_mdp_is_ubwc_format(fmt) &&
		!mdss_mdp_is_ubwc_supported(mdata)) ? NULL : fmt;
}

int mdss_mdp_get_ubwc_micro_dim(u32 format, u16 *w, u16 *h)
{
	struct mdss_mdp_format_params_ubwc *fmt = NULL;
	bool fmt_found = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(mdss_mdp_format_ubwc_map); i++) {
		fmt = &mdss_mdp_format_ubwc_map[i];
		if (format == fmt->mdp_format.format) {
			fmt_found = true;
			break;
		}
	}

	if (!fmt_found)
		return -EINVAL;

	*w = fmt->micro.tile_width;
	*h = fmt->micro.tile_height;
	return 0;
}

void mdss_mdp_get_v_h_subsample_rate(u8 chroma_sample,
		u8 *v_sample, u8 *h_sample)
{
	switch (chroma_sample) {
	case MDSS_MDP_CHROMA_H2V1:
		*v_sample = 1;
		*h_sample = 2;
		break;
	case MDSS_MDP_CHROMA_H1V2:
		*v_sample = 2;
		*h_sample = 1;
		break;
	case MDSS_MDP_CHROMA_420:
		*v_sample = 2;
		*h_sample = 2;
		break;
	default:
		*v_sample = 1;
		*h_sample = 1;
		break;
	}
}

void mdss_mdp_intersect_rect(struct mdss_rect *res_rect,
	const struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect)
{
	int l = max(dst_rect->x, sci_rect->x);
	int t = max(dst_rect->y, sci_rect->y);
	int r = min((dst_rect->x + dst_rect->w), (sci_rect->x + sci_rect->w));
	int b = min((dst_rect->y + dst_rect->h), (sci_rect->y + sci_rect->h));

	if (r < l || b < t)
		*res_rect = (struct mdss_rect){0, 0, 0, 0};
	else
		*res_rect = (struct mdss_rect){l, t, (r-l), (b-t)};
}

void mdss_mdp_crop_rect(struct mdss_rect *src_rect,
	struct mdss_rect *dst_rect,
	const struct mdss_rect *sci_rect, bool normalize)
{
	struct mdss_rect res;
	mdss_mdp_intersect_rect(&res, dst_rect, sci_rect);

	if (res.w && res.h) {
		if ((res.w != dst_rect->w) || (res.h != dst_rect->h)) {
			src_rect->x = src_rect->x + (res.x - dst_rect->x);
			src_rect->y = src_rect->y + (res.y - dst_rect->y);
			src_rect->w = res.w;
			src_rect->h = res.h;
		}

		/* adjust dest rect based on the sci_rect starting */
		if (normalize) {
			*dst_rect = (struct mdss_rect) {(res.x - sci_rect->x),
					(res.y - sci_rect->y), res.w, res.h};

		/* return the actual cropped intersecting rect */
		} else {
			*dst_rect = (struct mdss_rect) {res.x, res.y,
					res.w, res.h};
		}
	}
}

/*
 * rect_copy_mdp_to_mdss() - copy mdp_rect struct to mdss_rect
 * @mdp  - pointer to mdp_rect, destination of the copy
 * @mdss - pointer to mdss_rect, source of the copy
 */
void rect_copy_mdss_to_mdp(struct mdp_rect *mdp, struct mdss_rect *mdss)
{
	mdp->x = mdss->x;
	mdp->y = mdss->y;
	mdp->w = mdss->w;
	mdp->h = mdss->h;
}

/*
 * rect_copy_mdp_to_mdss() - copy mdp_rect struct to mdss_rect
 * @mdp  - pointer to mdp_rect, source of the copy
 * @mdss - pointer to mdss_rect, destination of the copy
 */
void rect_copy_mdp_to_mdss(struct mdp_rect *mdp, struct mdss_rect *mdss)
{
	mdss->x = mdp->x;
	mdss->y = mdp->y;
	mdss->w = mdp->w;
	mdss->h = mdp->h;
}

/*
 * mdss_rect_cmp() - compares two rects
 * @rect1 - rect value to compare
 * @rect2 - rect value to compare
 *
 * Returns 1 if the rects are same, 0 otherwise.
 */
int mdss_rect_cmp(struct mdss_rect *rect1, struct mdss_rect *rect2)
{
	return rect1->x == rect2->x && rect1->y == rect2->y &&
	       rect1->w == rect2->w && rect1->h == rect2->h;
}

/*
 * mdss_rect_overlap_check() - compare two rects and check if they overlap
 * @rect1 - rect value to compare
 * @rect2 - rect value to compare
 *
 * Returns true if rects overlap, false otherwise.
 */
bool mdss_rect_overlap_check(struct mdss_rect *rect1, struct mdss_rect *rect2)
{
	u32 rect1_left = rect1->x, rect1_right = rect1->x + rect1->w;
	u32 rect1_top = rect1->y, rect1_bottom = rect1->y + rect1->h;
	u32 rect2_left = rect2->x, rect2_right = rect2->x + rect2->w;
	u32 rect2_top = rect2->y, rect2_bottom = rect2->y + rect2->h;

	if ((rect1_right <= rect2_left) ||
	    (rect1_left >= rect2_right) ||
	    (rect1_bottom <= rect2_top) ||
	    (rect1_top >= rect2_bottom))
		return false;

	return true;
}

/*
 * mdss_rect_split() - split roi into two with regards to split-point.
 * @in_roi - input roi, non-split
 * @l_roi  - left roi after split
 * @r_roi  - right roi after split
 *
 * Split input ROI into left and right ROIs with respect to split-point. This
 * is useful during partial update with ping-pong split enabled, where user-land
 * program is aware of only one frame-buffer but physically there are two
 * distinct panels which requires their own ROIs.
 */
void mdss_rect_split(struct mdss_rect *in_roi, struct mdss_rect *l_roi,
	struct mdss_rect *r_roi, u32 splitpoint)
{
	memset(l_roi, 0x0, sizeof(*l_roi));
	memset(r_roi, 0x0, sizeof(*r_roi));

	/* left update needed */
	if (in_roi->x < splitpoint) {
		*l_roi = *in_roi;

		if ((l_roi->x + l_roi->w) >= splitpoint)
			l_roi->w = splitpoint - in_roi->x;
	}

	/* right update needed */
	if ((in_roi->x + in_roi->w) > splitpoint) {
		*r_roi = *in_roi;

		if (in_roi->x < splitpoint) {
			r_roi->x = 0;
			r_roi->w = in_roi->x + in_roi->w - splitpoint;
		} else {
			r_roi->x = in_roi->x - splitpoint;
		}
	}

	pr_debug("left: %d,%d,%d,%d right: %d,%d,%d,%d\n",
		l_roi->x, l_roi->y, l_roi->w, l_roi->h,
		r_roi->x, r_roi->y, r_roi->w, r_roi->h);
}

int mdss_mdp_get_rau_strides(u32 w, u32 h,
			       struct mdss_mdp_format_params *fmt,
			       struct mdss_mdp_plane_sizes *ps)
{
	if (fmt->is_yuv) {
		ps->rau_cnt = DIV_ROUND_UP(w, 64);
		ps->ystride[0] = 64 * 4;
		ps->rau_h[0] = 4;
		ps->rau_h[1] = 2;
		if (fmt->chroma_sample == MDSS_MDP_CHROMA_H1V2)
			ps->ystride[1] = 64 * 2;
		else if (fmt->chroma_sample == MDSS_MDP_CHROMA_H2V1) {
			ps->ystride[1] = 32 * 4;
			ps->rau_h[1] = 4;
		} else
			ps->ystride[1] = 32 * 2;

		/* account for both chroma components */
		ps->ystride[1] <<= 1;
	} else if (fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
		ps->rau_cnt = DIV_ROUND_UP(w, 32);
		ps->ystride[0] = 32 * 4 * fmt->bpp;
		ps->ystride[1] = 0;
		ps->rau_h[0] = 4;
		ps->rau_h[1] = 0;
	} else  {
		pr_err("Invalid format=%d\n", fmt->format);
		return -EINVAL;
	}

	ps->ystride[0] *= ps->rau_cnt;
	ps->ystride[1] *= ps->rau_cnt;
	ps->num_planes = 2;

	pr_debug("BWC rau_cnt=%d strides={%d,%d} heights={%d,%d}\n",
		ps->rau_cnt, ps->ystride[0], ps->ystride[1],
		ps->rau_h[0], ps->rau_h[1]);

	return 0;
}

static int mdss_mdp_get_ubwc_plane_size(struct mdss_mdp_format_params *fmt,
	u32 width, u32 height, struct mdss_mdp_plane_sizes *ps)
{
	int rc = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_format_params_ubwc *fmt_ubwc =
		(struct mdss_mdp_format_params_ubwc *)fmt;

	if (!mdss_mdp_is_ubwc_supported(mdata)) {
		pr_err("ubwc format is not supported for format: %d\n",
			fmt->format);
		return -EINVAL;
	}

	if (fmt->format == MDP_Y_CBCR_H2V2_UBWC ||
		fmt->format == MDP_Y_CBCR_H2V2_TP10_UBWC) {
		uint32_t y_stride_alignment = 0, uv_stride_alignment = 0;
		uint32_t y_height_alignment = 0, uv_height_alignment = 0;
		uint32_t y_tile_width = fmt_ubwc->micro.tile_width;
		uint32_t y_tile_height = fmt_ubwc->micro.tile_height;
		uint32_t uv_tile_width = y_tile_width / 2;
		uint32_t uv_tile_height = y_tile_height;
		uint32_t y_bpp_numer = 1, y_bpp_denom = 1;
		uint32_t uv_bpp_numer = 1, uv_bpp_denom = 1;

		ps->num_planes = 4;
		if (fmt->format == MDP_Y_CBCR_H2V2_UBWC) {
			y_stride_alignment = 128;
			uv_stride_alignment = 64;
			y_height_alignment = 32;
			uv_height_alignment = 32;
			y_bpp_numer = 1;
			uv_bpp_numer = 2;
			y_bpp_denom = 1;
			uv_bpp_denom = 1;
		} else if (fmt->format == MDP_Y_CBCR_H2V2_TP10_UBWC) {
			y_stride_alignment = 192;
			uv_stride_alignment = 96;
			y_height_alignment = 16;
			uv_height_alignment = 16;
			y_bpp_numer = 4;
			uv_bpp_numer = 8;
			y_bpp_denom = 3;
			uv_bpp_denom = 3;
		}

		/* Y bitstream stride and plane size */
		ps->ystride[0] = ALIGN_UP(width, y_stride_alignment);
		ps->ystride[0] = (ps->ystride[0] * y_bpp_numer) / y_bpp_denom;
		ps->plane_size[0] = ALIGN(ps->ystride[0] *
			ALIGN(height, y_height_alignment), 4096);

		/* CbCr bitstream stride and plane size */
		ps->ystride[1] = ALIGN_UP(width / 2, uv_stride_alignment);
		ps->ystride[1] = (ps->ystride[1] * uv_bpp_numer) / uv_bpp_denom;
		ps->plane_size[1] = ALIGN(ps->ystride[1] *
			ALIGN(height / 2, uv_height_alignment), 4096);

		/* Y meta data stride and plane size */
		ps->ystride[2] = ALIGN(DIV_ROUND_UP(width, y_tile_width), 64);
		ps->plane_size[2] = ALIGN(ps->ystride[2] *
			ALIGN(DIV_ROUND_UP(height, y_tile_height), 16), 4096);

		/* CbCr meta data stride and plane size */
		ps->ystride[3] =
			ALIGN(DIV_ROUND_UP(width / 2, uv_tile_width), 64);
		ps->plane_size[3] = ALIGN(ps->ystride[3] * ALIGN(
			DIV_ROUND_UP(height / 2, uv_tile_height), 16), 4096);
	} else if (fmt->format == MDP_RGBA_8888_UBWC ||
		fmt->format == MDP_RGBX_8888_UBWC ||
		fmt->format == MDP_RGB_565_UBWC ||
		fmt->format == MDP_RGBA_1010102_UBWC ||
		fmt->format == MDP_RGBX_1010102_UBWC) {
		uint32_t stride_alignment, bpp, aligned_bitstream_width;

		if (fmt->format == MDP_RGB_565_UBWC) {
			stride_alignment = 128;
			bpp = 2;
		} else {
			stride_alignment = 64;
			bpp = 4;
		}
		ps->num_planes = 2;

		/* RGB bitstream stride and plane size */
		aligned_bitstream_width = ALIGN(width, stride_alignment);
		ps->ystride[0] = aligned_bitstream_width * bpp;
		ps->plane_size[0] = ALIGN(bpp * aligned_bitstream_width *
			ALIGN(height, 16), 4096);

		/* RGB meta data stride and plane size */
		ps->ystride[2] =
			ALIGN(DIV_ROUND_UP(aligned_bitstream_width, 16), 64);
		ps->plane_size[2] = ALIGN(ps->ystride[2] *
			ALIGN(DIV_ROUND_UP(height, 4), 16), 4096);
	} else {
		pr_err("%s: UBWC format not supported for fmt:%d\n",
			__func__, fmt->format);
		rc = -EINVAL;
	}

	return rc;
}

int mdss_mdp_get_plane_sizes(struct mdss_mdp_format_params *fmt, u32 w, u32 h,
	struct mdss_mdp_plane_sizes *ps, u32 bwc_mode, bool rotation)
{
	int i, rc = 0;
	u32 bpp;
	if (ps == NULL)
		return -EINVAL;

	memset(ps, 0, sizeof(struct mdss_mdp_plane_sizes));

	if ((w > MAX_IMG_WIDTH) || (h > MAX_IMG_HEIGHT))
		return -ERANGE;

	bpp = fmt->bpp;

	if (mdss_mdp_is_ubwc_format(fmt)) {
		rc = mdss_mdp_get_ubwc_plane_size(fmt, w, h, ps);
	} else if (bwc_mode) {
		u32 height, meta_size;

		rc = mdss_mdp_get_rau_strides(w, h, fmt, ps);
		if (rc)
			return rc;

		height = DIV_ROUND_UP(h, ps->rau_h[0]);
		meta_size = DIV_ROUND_UP(ps->rau_cnt, 8);
		ps->ystride[1] += meta_size;
		ps->ystride[0] += ps->ystride[1] + meta_size;
		ps->plane_size[0] = ps->ystride[0] * height;

		ps->ystride[1] = 2;
		ps->plane_size[1] = 2 * ps->rau_cnt * height;

		pr_debug("BWC data stride=%d size=%d meta size=%d\n",
			ps->ystride[0], ps->plane_size[0], ps->plane_size[1]);
	} else {
		if (fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
			ps->num_planes = 1;
			ps->plane_size[0] = w * h * bpp;
			ps->ystride[0] = w * bpp;
		} else if (fmt->format == MDP_Y_CBCR_H2V2_VENUS ||
				fmt->format == MDP_Y_CRCB_H2V2_VENUS) {

			int cf = (fmt->format == MDP_Y_CBCR_H2V2_VENUS) ?
					COLOR_FMT_NV12 : COLOR_FMT_NV21;
			ps->num_planes = 2;
			ps->ystride[0] = VENUS_Y_STRIDE(cf, w);
			ps->ystride[1] = VENUS_UV_STRIDE(cf, w);
			ps->plane_size[0] = VENUS_Y_SCANLINES(cf, h) *
				ps->ystride[0];
			ps->plane_size[1] = VENUS_UV_SCANLINES(cf, h) *
				ps->ystride[1];
		} else {
			u8 v_subsample, h_subsample, stride_align, height_align;
			u32 chroma_samp;

			chroma_samp = fmt->chroma_sample;

			mdss_mdp_get_v_h_subsample_rate(chroma_samp,
				&v_subsample, &h_subsample);

			switch (fmt->format) {
			case MDP_Y_CR_CB_GH2V2:
				stride_align = 16;
				height_align = 1;
				break;
			default:
				stride_align = 1;
				height_align = 1;
				break;
			}

			w = w << fmt->unpack_dx_format;

			ps->ystride[0] = ALIGN(w, stride_align);
			ps->ystride[1] = ALIGN(w / h_subsample, stride_align);
			ps->plane_size[0] = ps->ystride[0] *
				ALIGN(h, height_align);
			ps->plane_size[1] = ps->ystride[1] * (h / v_subsample);

			if (fmt->fetch_planes == MDSS_MDP_PLANE_PSEUDO_PLANAR) {
				ps->num_planes = 2;
				ps->plane_size[1] *= 2;
				ps->ystride[1] *= 2;
			} else { /* planar */
				ps->num_planes = 3;
				ps->plane_size[2] = ps->plane_size[1];
				ps->ystride[2] = ps->ystride[1];
			}
		}
	}

	/* Safe to use MAX_PLANES as ps is memset at start of function */
	for (i = 0; i < MAX_PLANES; i++)
		ps->total_size += ps->plane_size[i];

	return rc;
}

static int mdss_mdp_ubwc_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps,
			struct mdss_mdp_format_params *fmt)
{
	int i, inc;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	unsigned long data_size = 0;
	dma_addr_t base_addr;

	if (!mdss_mdp_is_ubwc_supported(mdata)) {
		pr_err("ubwc format is not supported for format: %d\n",
			fmt->format);
		return -ENOTSUPP;
	}

	if (data->p[0].len == ps->plane_size[0])
		goto end;

	/* From this point, assumption is plane 0 is to be divided */
	data_size = data->p[0].len;
	if (data_size < ps->total_size) {
		pr_err("insufficient current mem len=%lu required mem len=%u\n",
		       data_size, ps->total_size);
		return -ENOMEM;
	}

	base_addr = data->p[0].addr;

	if (fmt->format == MDP_Y_CBCR_H2V2_UBWC ||
		fmt->format == MDP_Y_CBCR_H2V2_TP10_UBWC) {
		/************************************************/
		/*      UBWC            **                      */
		/*      buffer          **      MDP PLANE       */
		/*      format          **                      */
		/************************************************/
		/* -------------------  ** -------------------- */
		/* |      Y meta     |  ** |    Y bitstream   | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |    Y bitstream  |  ** |  CbCr bitstream  | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |   Cbcr metadata |  ** |       Y meta     | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |  CbCr bitstream |  ** |     CbCr meta    | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/************************************************/

		/* configure Y bitstream plane */
		data->p[0].addr = base_addr + ps->plane_size[2];
		data->p[0].len = ps->plane_size[0];

		/* configure CbCr bitstream plane */
		data->p[1].addr = base_addr + ps->plane_size[0]
			+ ps->plane_size[2] + ps->plane_size[3];
		data->p[1].len = ps->plane_size[1];

		/* configure Y metadata plane */
		data->p[2].addr = base_addr;
		data->p[2].len = ps->plane_size[2];

		/* configure CbCr metadata plane */
		data->p[3].addr = base_addr + ps->plane_size[0]
			+ ps->plane_size[2];
		data->p[3].len = ps->plane_size[3];
	} else {
		/************************************************/
		/*      UBWC            **                      */
		/*      buffer          **      MDP PLANE       */
		/*      format          **                      */
		/************************************************/
		/* -------------------  ** -------------------- */
		/* |      RGB meta   |  ** |   RGB bitstream  | */
		/* |       data      |  ** |       plane      | */
		/* -------------------  ** -------------------- */
		/* |  RGB bitstream  |  ** |       NONE       | */
		/* |       data      |  ** |                  | */
		/* -------------------  ** -------------------- */
		/*                      ** |     RGB meta     | */
		/*                      ** |       plane      | */
		/*                      ** -------------------- */
		/************************************************/

		/* configure RGB bitstream plane */
		data->p[0].addr = base_addr + ps->plane_size[2];
		data->p[0].len = ps->plane_size[0];

		/* configure RGB metadata plane */
		data->p[2].addr = base_addr;
		data->p[2].len = ps->plane_size[2];
	}
	data->num_planes = ps->num_planes;

end:
	if (data->num_planes != ps->num_planes) {
		pr_err("num_planes don't match: fmt:%d, data:%d, ps:%d\n",
				fmt->format, data->num_planes, ps->num_planes);
		return -EINVAL;
	}

	inc = ((fmt->format == MDP_Y_CBCR_H2V2_UBWC ||
		fmt->format == MDP_Y_CBCR_H2V2_TP10_UBWC) ? 1 : 2);
	for (i = 0; i < MAX_PLANES; i += inc) {
		if (data->p[i].len != ps->plane_size[i]) {
			pr_err("plane:%d fmt:%d, len does not match: data:%lu, ps:%d\n",
					i, fmt->format, data->p[i].len,
					ps->plane_size[i]);
			return -EINVAL;
		}
	}

	return 0;
}

int mdss_mdp_data_check(struct mdss_mdp_data *data,
			struct mdss_mdp_plane_sizes *ps,
			struct mdss_mdp_format_params *fmt)
{
	struct mdss_mdp_img_data *prev, *curr;
	int i;

	if (!ps)
		return 0;

	if (!data || data->num_planes == 0)
		return -ENOMEM;

	if (mdss_mdp_is_ubwc_format(fmt))
		return mdss_mdp_ubwc_data_check(data, ps, fmt);

	pr_debug("srcp0=%pa len=%lu frame_size=%u\n", &data->p[0].addr,
		data->p[0].len, ps->total_size);

	for (i = 0; i < ps->num_planes; i++) {
		curr = &data->p[i];
		if (i >= data->num_planes) {
			u32 psize = ps->plane_size[i-1];
			prev = &data->p[i-1];
			if (prev->len > psize) {
				curr->len = prev->len - psize;
				prev->len = psize;
			}
			curr->addr = prev->addr + psize;
		}
		if (curr->len < ps->plane_size[i]) {
			pr_err("insufficient mem=%lu p=%d len=%u\n",
			       curr->len, i, ps->plane_size[i]);
			return -ENOMEM;
		}
		pr_debug("plane[%d] addr=%pa len=%lu\n", i,
				&curr->addr, curr->len);
	}
	data->num_planes = ps->num_planes;

	return 0;
}

int mdss_mdp_validate_offset_for_ubwc_format(
	struct mdss_mdp_format_params *fmt, u16 x, u16 y)
{
	int ret;
	u16 micro_w, micro_h;

	ret = mdss_mdp_get_ubwc_micro_dim(fmt->format, &micro_w, &micro_h);
	if (ret || !micro_w || !micro_h) {
		pr_err("Could not get valid micro tile dimensions\n");
		return -EINVAL;
	}

	if (x % (micro_w * UBWC_META_MACRO_W_H)) {
		pr_err("x=%d does not align with meta width=%d\n", x,
			micro_w * UBWC_META_MACRO_W_H);
		return -EINVAL;
	}

	if (y % (micro_h * UBWC_META_MACRO_W_H)) {
		pr_err("y=%d does not align with meta height=%d\n", y,
			UBWC_META_MACRO_W_H);
		return -EINVAL;
	}
	return ret;
}

/* x and y are assumednt to be valid, expected to line up with start of tiles */
void mdss_mdp_ubwc_data_calc_offset(struct mdss_mdp_data *data, u16 x, u16 y,
	struct mdss_mdp_plane_sizes *ps, struct mdss_mdp_format_params *fmt)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u16 macro_w, micro_w, micro_h;
	u32 offset;
	int ret;

	if (!mdss_mdp_is_ubwc_supported(mdata)) {
		pr_err("ubwc format is not supported for format: %d\n",
			fmt->format);
		return;
	}

	ret = mdss_mdp_get_ubwc_micro_dim(fmt->format, &micro_w, &micro_h);
	if (ret || !micro_w || !micro_h) {
		pr_err("Could not get valid micro tile dimensions\n");
		return;
	}
	macro_w = 4 * micro_w;

	if (fmt->format == MDP_Y_CBCR_H2V2_UBWC ||
		fmt->format == MDP_Y_CBCR_H2V2_TP10_UBWC) {
		u16 chroma_macro_w = macro_w / 2;
		u16 chroma_micro_w = micro_w / 2;

		/* plane 1 and 3 are chroma, with sub sample of 2 */
		offset = y * ps->ystride[0] +
			(x / macro_w) * 4096;
		if (offset < data->p[0].len) {
			data->p[0].addr += offset;
		} else {
			ret = 1;
			goto done;
		}

		offset = y / 2 * ps->ystride[1] +
			((x / 2) / chroma_macro_w) * 4096;
		if (offset < data->p[1].len) {
			data->p[1].addr += offset;
		} else {
			ret = 2;
			goto done;
		}

		offset = (y / micro_h) * ps->ystride[2] +
			((x / micro_w) / UBWC_META_MACRO_W_H) *
			UBWC_META_BLOCK_SIZE;
		if (offset < data->p[2].len) {
			data->p[2].addr += offset;
		} else {
			ret = 3;
			goto done;
		}

		offset = ((y / 2) / micro_h) * ps->ystride[3] +
			(((x / 2) / chroma_micro_w) / UBWC_META_MACRO_W_H) *
			UBWC_META_BLOCK_SIZE;
		if (offset < data->p[3].len) {
			data->p[3].addr += offset;
		} else {
			ret = 4;
			goto done;
		}

	} else {
		offset = y * ps->ystride[0] +
			(x / macro_w) * 4096;
		if (offset < data->p[0].len) {
			data->p[0].addr += offset;
		} else {
			ret = 1;
			goto done;
		}

		offset = DIV_ROUND_UP(y, micro_h) * ps->ystride[2] +
			((x / micro_w) / UBWC_META_MACRO_W_H) *
			UBWC_META_BLOCK_SIZE;
		if (offset < data->p[2].len) {
			data->p[2].addr += offset;
		} else {
			ret = 3;
			goto done;
		}
	}

done:
	if (ret) {
		WARN(1, "idx %d, offsets:%u too large for buflen%lu\n",
			(ret - 1), offset, data->p[(ret - 1)].len);
	}
}

void mdss_mdp_data_calc_offset(struct mdss_mdp_data *data, u16 x, u16 y,
	struct mdss_mdp_plane_sizes *ps, struct mdss_mdp_format_params *fmt)
{
	if ((x == 0) && (y == 0))
		return;

	if (mdss_mdp_is_ubwc_format(fmt)) {
		mdss_mdp_ubwc_data_calc_offset(data, x, y, ps, fmt);
		return;
	}

	data->p[0].addr += y * ps->ystride[0];

	if (data->num_planes == 1) {
		data->p[0].addr += x * fmt->bpp;
	} else {
		u16 xoff, yoff;
		u8 v_subsample, h_subsample;
		mdss_mdp_get_v_h_subsample_rate(fmt->chroma_sample,
			&v_subsample, &h_subsample);

		xoff = x / h_subsample;
		yoff = y / v_subsample;

		data->p[0].addr += x;
		data->p[1].addr += xoff + (yoff * ps->ystride[1]);
		if (data->num_planes == 2) /* pseudo planar */
			data->p[1].addr += xoff;
		else /* planar */
			data->p[2].addr += xoff + (yoff * ps->ystride[2]);
	}
}

static int mdss_mdp_put_img(struct mdss_mdp_img_data *data, bool rotator,
		int dir)
{
	u32 domain;

	if (data->flags & MDP_MEMORY_ID_TYPE_FB) {
		pr_debug("fb mem buf=0x%pa\n", &data->addr);
		fdput(data->srcp_f);
		memset(&data->srcp_f, 0, sizeof(struct fd));
	} else if (data->srcp_f.file) {
		pr_debug("pmem buf=0x%pa\n", &data->addr);
		memset(&data->srcp_f, 0, sizeof(struct fd));
	} else if (!IS_ERR_OR_NULL(data->srcp_dma_buf)) {
		pr_debug("ion hdl=%pK buf=0x%pa\n", data->srcp_dma_buf,
							&data->addr);
			if (data->mapped) {
				domain = mdss_smmu_get_domain_type(data->flags,
					rotator);
				data->mapped = false;
			}
			if (!data->skip_detach) {
				data->srcp_attachment->dma_map_attrs
						 |= DMA_ATTR_SKIP_CPU_SYNC;
				dma_buf_unmap_attachment(data->srcp_attachment,
					data->srcp_table,
					mdss_smmu_dma_data_direction(dir));
				dma_buf_detach(data->srcp_dma_buf,
						data->srcp_attachment);
				dma_buf_put(data->srcp_dma_buf);
				data->srcp_dma_buf = NULL;
			}
	} else if ((data->flags & MDP_SECURE_DISPLAY_OVERLAY_SESSION) ||
			(data->flags & MDP_SECURE_CAMERA_OVERLAY_SESSION)) {
		/*
		 * skip memory unmapping - secure display and camera uses
		 * physical address which does not require buffer unmapping
		 *
		 * For LT targets in secure display usecase, srcp_dma_buf will
		 * be filled due to map call which will be unmapped above.
		 *
		 */
		pr_debug("free memory handle for secure display/camera content\n");
	} else {
		return -ENOMEM;
	}

	return 0;
}

static int mdss_mdp_get_img(struct msmfb_data *img,
		struct mdss_mdp_img_data *data, struct device *dev,
		bool rotator, int dir)
{
	struct fd f;
	int ret = -EINVAL;
	int fb_num;
	unsigned long *len;
	u32 domain;
	dma_addr_t *start;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	start = &data->addr;
	len = &data->len;
	data->flags |= img->flags;
	data->offset = img->offset;
	if (img->flags & MDP_MEMORY_ID_TYPE_FB) {
		f = fdget(img->memory_id);
		if (f.file == NULL) {
			pr_err("invalid framebuffer file (%d)\n",
					img->memory_id);
			return -EINVAL;
		}
		data->srcp_f = f;

		if (MAJOR(f.file->f_path.dentry->d_inode->i_rdev) == FB_MAJOR) {
			fb_num = MINOR(f.file->f_path.dentry->d_inode->i_rdev);
			ret = mdss_fb_get_phys_info(start, len, fb_num);
			if (ret)
				pr_err("mdss_fb_get_phys_info() failed\n");
		} else {
			pr_err("invalid FB_MAJOR\n");
			ret = -1;
		}
	} else {
		data->srcp_dma_buf = dma_buf_get(img->memory_id);
		if (IS_ERR_OR_NULL(data->srcp_dma_buf)) {
			pr_err("error on ion_import_fd\n");
			ret = PTR_ERR(data->srcp_dma_buf);
			data->srcp_dma_buf = NULL;
			return ret;
		}

		if (mdss_mdp_is_map_needed(mdata, data)) {
			domain = mdss_smmu_get_domain_type(data->flags,
						   rotator);
			data->srcp_attachment =
				mdss_smmu_dma_buf_attach(data->srcp_dma_buf,
							 dev, domain);
			if (IS_ERR_OR_NULL(data->srcp_attachment)) {
				ret = PTR_ERR(data->srcp_attachment);
				pr_err("error during dma buf attach\n");
				goto err_put;
			}


			data->srcp_attachment->dma_map_attrs |=
					DMA_ATTR_DELAYED_UNMAP;

			data->srcp_table =
				dma_buf_map_attachment(data->srcp_attachment,
				mdss_smmu_dma_data_direction(dir));
			if (IS_ERR(data->srcp_table)) {
				ret = PTR_ERR(data->srcp_table);
				goto err_detach;
			}

			data->addr = 0;
			data->len = 0;
			data->mapped = false;
			data->skip_detach = false;
			/* return early, mapping will be done later */
			ret = 0;
			goto done;
		} else {
			struct sg_table *sg_ptr = NULL;

			data->srcp_attachment =
				dma_buf_attach(data->srcp_dma_buf, dev);
			if (IS_ERR(data->srcp_attachment)) {
				ret = PTR_ERR(data->srcp_attachment);
				goto err_put;
			}

			data->srcp_table =
				dma_buf_map_attachment(data->srcp_attachment,
				mdss_smmu_dma_data_direction(dir));
			if (IS_ERR(data->srcp_table)) {
				ret = PTR_ERR(data->srcp_table);
				goto err_detach;
			}

			do {
				sg_ptr = data->srcp_table;
				if (sg_ptr == NULL) {
					pr_err("ion sg table get failed\n");
					ret = -EINVAL;
					break;
				}

				if (sg_ptr->nents != 1) {
					pr_err("ion buffer mapping failed\n");
					ret = -EINVAL;
					break;
				}

				if (((uint64_t)sg_dma_address(sg_ptr->sgl) >=
					PHY_ADDR_4G - sg_ptr->sgl->length)) {
					pr_err("ion buffer mapped size is invalid\n");
					ret = -EINVAL;
					break;
				}

				data->addr = sg_dma_address(sg_ptr->sgl);
				data->len = sg_ptr->sgl->length;
				data->mapped = true;
				ret = 0;
			} while (0);

			return ret;
		}
	}
	if (start && !*start) {
		pr_err("start address is zero!\n");
		mdss_mdp_put_img(data, rotator, dir);
		return -ENOMEM;
	}

	if (!ret && (data->offset < data->len)) {
		data->addr += data->offset;
		data->len -= data->offset;

		pr_debug("mem=%d ihdl=%pK buf=0x%pa len=0x%lx\n",
			img->memory_id, data->srcp_dma_buf,
			&data->addr, data->len);
	} else {
		mdss_mdp_put_img(data, rotator, dir);
		return ret ? : -EOVERFLOW;
	}

	return ret;
err_detach:
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
err_put:
	dma_buf_put(data->srcp_dma_buf);
done:
	return ret;
}

static int mdss_mdp_map_buffer(struct mdss_mdp_img_data *data, bool rotator,
		int dir)
{
	int ret = -EINVAL;
	int domain;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct scatterlist *sg;
	unsigned int i;
	struct sg_table *table;

	if (data->addr && data->len)
		return 0;

	if (!IS_ERR_OR_NULL(data->srcp_dma_buf)) {
		if (mdss_res->mdss_util->iommu_attached() &&
			(mdss_mdp_is_map_needed(mdata, data))) {
			domain = mdss_smmu_get_domain_type(data->flags,
					rotator);
			data->dir = dir;
			data->domain = domain;
			ret = mdss_smmu_map_dma_buf(data->srcp_dma_buf,
					data->srcp_table, domain,
					&data->addr, &data->len, dir);
			if (IS_ERR_VALUE((unsigned long) ret)) {
				pr_err("smmu map dma buf failed: (%d)\n", ret);
				goto err_unmap;
			}
			data->mapped = true;
		} else {
			data->addr = sg_phys(data->srcp_table->sgl);
			data->len = 0;
			table = data->srcp_table;
			for_each_sg(table->sgl, sg, table->nents, i) {
				data->len += sg->length;
			}
			ret = 0;
		}
	}

	if (!data->addr) {
		pr_err("start address is zero!\n");
		mdss_mdp_put_img(data, rotator, dir);
		return -ENOMEM;
	}

	if (!ret && (data->offset < data->len)) {
		data->addr += data->offset;
		data->len -= data->offset;

		pr_debug("ihdl=%pK buf=0x%pa len=0x%lx\n",
			 data->srcp_dma_buf, &data->addr, data->len);
	} else {
		mdss_mdp_put_img(data, rotator, dir);
		return ret ? : -EOVERFLOW;
	}

	return ret;

err_unmap:
	data->srcp_attachment->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;
	dma_buf_unmap_attachment(data->srcp_attachment, data->srcp_table,
		mdss_smmu_dma_data_direction(dir));
	dma_buf_detach(data->srcp_dma_buf, data->srcp_attachment);
	dma_buf_put(data->srcp_dma_buf);
	return ret;
}

static int mdss_mdp_data_get(struct mdss_mdp_data *data,
		struct msmfb_data *planes, int num_planes, u64 flags,
		struct device *dev, bool rotator, int dir)
{
	int i, rc = 0;

	if ((num_planes <= 0) || (num_planes > MAX_PLANES))
		return -EINVAL;

	for (i = 0; i < num_planes; i++) {
		data->p[i].flags = flags;
		rc = mdss_mdp_get_img(&planes[i], &data->p[i], dev, rotator,
				dir);
		if (rc) {
			pr_err("failed to get buf p=%d flags=%llx\n", i, flags);
			while (i > 0) {
				i--;
				mdss_mdp_put_img(&data->p[i], rotator, dir);
			}
			break;
		}
	}

	data->num_planes = i;

	return rc;
}

int mdss_mdp_data_map(struct mdss_mdp_data *data, bool rotator, int dir)
{
	int i, rc = 0;

	if (!data || !data->num_planes || data->num_planes > MAX_PLANES)
		return -EINVAL;

	for (i = 0; i < data->num_planes; i++) {
		rc = mdss_mdp_map_buffer(&data->p[i], rotator, dir);
		if (rc) {
			pr_err("failed to map buf p=%d\n", i);
			while (i > 0) {
				i--;
				mdss_mdp_put_img(&data->p[i], rotator, dir);
			}
			break;
		}
	}

	return rc;
}

void mdss_mdp_data_free(struct mdss_mdp_data *data, bool rotator, int dir)
{
	int i;

	mdss_iommu_ctrl(1);
	for (i = 0; i < data->num_planes && data->p[i].len; i++)
		mdss_mdp_put_img(&data->p[i], rotator, dir);
	memset(&data->p, 0, sizeof(struct mdss_mdp_img_data) * MAX_PLANES);
	mdss_iommu_ctrl(0);

	data->num_planes = 0;
}

int mdss_mdp_data_get_and_validate_size(struct mdss_mdp_data *data,
	struct msmfb_data *planes, int num_planes, u64 flags,
	struct device *dev, bool rotator, int dir,
	struct mdp_layer_buffer *buffer)
{
	struct mdss_mdp_format_params *fmt;
	struct mdss_mdp_plane_sizes ps;
	int ret, i;
	unsigned long total_buf_len = 0;

	fmt = mdss_mdp_get_format_params(buffer->format);
	if (!fmt) {
		pr_err("Format %d not supported\n", buffer->format);
		return -EINVAL;
	}

	ret = mdss_mdp_data_get(data, planes, num_planes,
		flags, dev, rotator, dir);
	if (ret)
		return ret;

	mdss_mdp_get_plane_sizes(fmt, buffer->width, buffer->height, &ps, 0, 0);

	for (i = 0; i < num_planes ; i++) {
		unsigned long plane_len = (data->p[i].srcp_dma_buf) ?
				data->p[i].srcp_dma_buf->size : data->p[i].len;

		if (plane_len < planes[i].offset) {
			pr_err("Offset=%d larger than buffer size=%lu\n",
				planes[i].offset, plane_len);
			ret = -EINVAL;
			goto buf_too_small;
		}
		total_buf_len += plane_len - planes[i].offset;
	}

	if (total_buf_len < ps.total_size) {
		pr_err("Buffer size=%lu, expected size=%d\n", total_buf_len,
			ps.total_size);
		ret = -EINVAL;
		goto buf_too_small;
	}
	return 0;

buf_too_small:
	mdss_mdp_data_free(data, rotator, dir);
	return ret;
}

int mdss_mdp_calc_phase_step(u32 src, u32 dst, u32 *out_phase)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 unit, residue, result;

	if (src == 0 || dst == 0)
		return -EINVAL;

	unit = 1 << PHASE_STEP_SHIFT;
	*out_phase = mult_frac(unit, src, dst);

	/* check if overflow is possible */
	if (mdss_has_quirk(mdata, MDSS_QUIRK_DOWNSCALE_HANG) && src > dst) {
		residue = *out_phase - unit;
		result = (residue * dst) + residue;

		while (result > (unit + (unit >> 1)))
			result -= unit;

		if ((result > residue) && (result < unit))
			return -EOVERFLOW;
	}

	return 0;
}

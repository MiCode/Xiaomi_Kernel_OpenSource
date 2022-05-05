// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */
#include "tile_driver.h"
#include "tile_mdp_func.h"
#include "mtk-mml-color.h"

#ifndef MAX
#define MAX(x, y)   ((x) >= (y) ? (x) : (y))
#endif  // MAX

#ifndef MIN
#define MIN(x, y)   ((x) <= (y) ? (x) : (y))
#endif  // MIN

enum isp_tile_message tile_rdma_init(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map)
{
	struct rdma_tile_data *data = &ptr_func->data->rdma;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* Specific constraints implied by different formats */

	/* In tile constraints
	 * Input Format | Tile Width
	 * -------------|-----------
	 *   Block mode | L
	 *       YUV420 | L
	 *       YUV422 | L * 2
	 * YUV444/RGB/Y | L * 4
	 */
	if (MML_FMT_ARGB_COMPRESS(data->src_fmt)) {
		/* For AFBC mode end x may be extend to block size
		 * and may exceed max tile width 640. So reduce width
		 * to prevent it.
		 */
		ptr_func->in_tile_width = ((data->max_width >> 5) - 1) << 5;
	} else if (MML_FMT_YUV_COMPRESS(data->src_fmt)) {
		ptr_func->in_tile_width = ((data->max_width >> 4) - 1) << 4;
	} else if (MML_FMT_BLOCK(data->src_fmt)) {
		ptr_func->in_tile_width = (data->max_width >> 6) << 6;
	} else if (MML_FMT_YUV420(data->src_fmt) ||
		   MML_FMT_COMPRESS(data->src_fmt)) {
		ptr_func->in_tile_width = data->max_width;
	} else if (MML_FMT_YUV422(data->src_fmt)) {
		ptr_func->in_tile_width = data->max_width * 2;
	} else {
		ptr_func->in_tile_width = data->max_width * 4;
	}

	if (MML_FMT_H_SUBSAMPLE(data->src_fmt)) {
		/* YUV422 or YUV420 */
		/* Tile alignment constraints */
		ptr_func->in_const_x = 2;

		if (MML_FMT_V_SUBSAMPLE(data->src_fmt) &&
		    !MML_FMT_INTERLACED(data->src_fmt)) {
			/* YUV420 */
			ptr_func->in_const_y = 2;
		}
	}

	if (MML_FMT_10BIT_PACKED(data->src_fmt) &&
	    !MML_FMT_COMPRESS(data->src_fmt) &&
	    !MML_FMT_IS_RGB(data->src_fmt) &&
	    !MML_FMT_BLOCK(data->src_fmt)) {
		/* 10-bit packed, not compress, not rgb, not blk */
		ptr_func->in_const_x = 4;
	}

	ptr_func->in_tile_height  = 65535;
	ptr_func->out_tile_height = 65535;

	ptr_func->crop_bias_x = data->crop.left;
	ptr_func->crop_bias_y = data->crop.top;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_hdr_init(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	struct hdr_tile_data *data = &ptr_func->data->hdr;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	ptr_func->in_tile_width   = 8191;
	ptr_func->out_tile_width  = 8191;
	ptr_func->in_tile_height  = 65535;
	ptr_func->out_tile_height = 65535;

	if (!data->relay_mode) {
		ptr_func->type |= TILE_TYPE_CROP_EN;
		ptr_func->in_min_width = data->min_width;
		ptr_func->l_tile_loss = 8;
		ptr_func->r_tile_loss = 8;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_aal_init(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	struct aal_tile_data *data = &ptr_func->data->aal;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	ptr_func->in_tile_width   = data->max_width;
	ptr_func->out_tile_width  = data->max_width;
	/* AAL_TILE_WIDTH > tile > AAL_HIST_MIN_WIDTH for histogram update,
	 * unless AAL_HIST_MIN_WIDTH > frame > AAL_MIN_WIDTH.
	 */
	ptr_func->in_min_width    = MAX(MIN(data->min_hist_width,
					    ptr_func->full_size_x_in),
					data->min_width);
	ptr_func->in_tile_height  = 65535;
	ptr_func->out_tile_height = 65535;
	ptr_func->l_tile_loss     = 8;
	ptr_func->r_tile_loss     = 8;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_prz_init(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	struct rsz_tile_data *data = &ptr_func->data->rsz;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	// drs: C42 downsampler output frame width
	data->c42_out_frame_w = (ptr_func->full_size_x_in + 0x01) & ~0x01;

	// prz
	if (data->ver_scale) {
		/* Line buffer size constraints (H: horizontal; V: vertical)
		 * H. Scale | V. First | V. Scale |  V. Acc.   | Tile Width
		 * ---------|----------|----------|------------|---------------
		 * (INF, 1] |   Yes    | (INF, 1] | 6-tap      | Input = L
		 *          |          | (1, 1/2) | 6/4n-tap   | Input = L
		 *          |          | [1/2, 0) | 4n-tap/src | Input = L / 2
		 * (1, 1/2) |    No    | (inf, 1] | 6-tap      | Output = L
		 *          |          | (1, 1/2) | 6/4n-tap   | Output = L
		 *          |          | [1/2, 0) | 4n-tap/src | Output = L / 2
		 * [1/2, 0) |    No    | (inf, 1] | 6-tap      | Output = L
		 *          |          | (1, 1/2) | 6/4n-tap   | Output = L / 2
		 *          |          | [1/2, 0) | 4n-tap/src | Output = L / 2
		 */
		if (data->ver_first) {
			/* vertical first */
			if (data->ver_algo == SCALER_6_TAPS ||
			    data->ver_cubic_trunc) {
				ptr_func->in_tile_width = data->max_width;
			} else {
				ptr_func->in_tile_width = data->max_width >> 1;
			}
		} else {
			if (data->ver_algo == SCALER_6_TAPS ||
			    data->ver_cubic_trunc) {
				ptr_func->out_tile_width = data->max_width - 2;
				data->prz_out_tile_w = data->max_width;
			} else {
				ptr_func->out_tile_width = (data->max_width >> 1) - 2;
				data->prz_out_tile_w = data->max_width >> 1;
			}
		}
	}

	ptr_func->in_tile_height = 65535;
	ptr_func->out_tile_height = 65535;

	// urs: C24 upsampler input frame width
	data->c24_in_frame_w = (ptr_func->full_size_x_out + 0x01) & ~0x1;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_tdshp_init(struct tile_func_block *ptr_func,
				      struct tile_reg_map *ptr_tile_reg_map)
{
	struct tdshp_tile_data *data = &ptr_func->data->tdshp;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	ptr_func->in_tile_width   = data->max_width;
	ptr_func->out_tile_width  = data->max_width;
	ptr_func->in_tile_height  = 65535;
	ptr_func->out_tile_height = 65535;
	ptr_func->l_tile_loss     = 3;
	ptr_func->r_tile_loss     = 3;
	ptr_func->t_tile_loss     = 2;
	ptr_func->b_tile_loss     = 2;

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_wrot_init(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map)
{
	struct wrot_tile_data *data = &ptr_func->data->wrot;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (data->racing) {
		if (data->rotate == MML_ROT_90 ||
		    data->rotate == MML_ROT_270) {
			ptr_func->out_tile_width  = data->racing_h;
			ptr_func->in_tile_height  = 65535;
			ptr_func->out_tile_height = 65535;
		} else {
			ptr_func->out_tile_width  = data->max_width;
			ptr_func->in_tile_height  = data->racing_h;
			ptr_func->out_tile_height = data->racing_h;
		}
	} else {
		if (data->rotate == MML_ROT_90 ||
		    data->rotate == MML_ROT_270 ||
		    data->flip) {
			/* 90, 270 degrees and flip */
			ptr_func->out_tile_width = data->max_width;
		} else {
			ptr_func->out_tile_width = data->max_width * 2;
		}
		ptr_func->in_tile_height  = 65535;
		ptr_func->out_tile_height = 65535;
	}

	if (MML_FMT_COMPRESS(data->dest_fmt))
		ptr_func->out_tile_width = MIN(128, ptr_func->out_tile_width);

	/* For tile calculation */
	if (MML_FMT_YUV422(data->dest_fmt)) {
		/* To update with rotation */
		if (data->rotate == MML_ROT_90 ||
		    data->rotate == MML_ROT_270) {
			/* 90, 270 degrees & YUV422 */
			ptr_func->out_const_x = 2;
			ptr_func->out_const_y = 2;
		} else {
			ptr_func->out_const_x = 2;
		}
	} else if (MML_FMT_YUV420(data->dest_fmt)) {
		ptr_func->out_const_x = 2;
		ptr_func->out_const_y = 2;
	} else if (data->dest_fmt != MML_FMT_GREY &&
		   !MML_FMT_IS_RGB(data->dest_fmt)) {
		ASSERT(0);
		return MDP_MESSAGE_WROT_INVALID_FORMAT;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_rdma_for(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		ptr_func->out_pos_xs = ptr_func->in_pos_xs - ptr_func->crop_bias_x;
		ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->crop_bias_x;
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		ptr_func->out_pos_ys = ptr_func->in_pos_ys - ptr_func->crop_bias_y;
		ptr_func->out_pos_ye = ptr_func->in_pos_ye - ptr_func->crop_bias_y;
	}

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		if (ptr_func->backward_output_xs_pos >= ptr_func->out_pos_xs) {
			ptr_func->bias_x = ptr_func->backward_output_xs_pos -
					   ptr_func->out_pos_xs;
			ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos)
			ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		if (ptr_func->backward_output_ys_pos >= ptr_func->out_pos_ys) {
			ptr_func->bias_y = ptr_func->backward_output_ys_pos -
					   ptr_func->out_pos_ys;
			ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos)
			ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_crop_for(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		if (ptr_func->backward_output_xs_pos >= ptr_func->out_pos_xs) {
			ptr_func->bias_x = ptr_func->backward_output_xs_pos -
					   ptr_func->out_pos_xs;
			ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos)
			ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		if (ptr_func->backward_output_ys_pos >= ptr_func->out_pos_ys) {
			ptr_func->bias_y = ptr_func->backward_output_ys_pos -
					   ptr_func->out_pos_ys;
			ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos)
			ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_aal_for(struct tile_func_block *ptr_func,
				   struct tile_reg_map *ptr_tile_reg_map)
{
	/* skip frame mode */
	if (ptr_tile_reg_map->first_frame)
		return ISP_MESSAGE_TILE_OK;

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		if (ptr_func->out_tile_width) {
			if (ptr_func->out_pos_xe + 1 >
			    ptr_func->out_pos_xs + ptr_func->out_tile_width) {
				ptr_func->out_pos_xe = ptr_func->out_pos_xs +
						       ptr_func->out_tile_width - 1;
				ptr_func->h_end_flag = false;
			}
		}
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		if (ptr_func->out_tile_height) {
			if (ptr_func->out_pos_ye + 1 >
			    ptr_func->out_pos_ys + ptr_func->out_tile_height)
				ptr_func->out_pos_ye = ptr_func->out_pos_ys +
						       ptr_func->out_tile_height - 1;
		}
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_prz_for(struct tile_func_block *ptr_func,
				   struct tile_reg_map *ptr_tile_reg_map)
{
	s32 C42OutXLeft = 0;
	s32 C42OutXRight = 0;
	s32 C24InXLeft = 0;
	s32 C24InXRight = 0;
	struct rsz_tile_data *data = &ptr_func->data->rsz;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		/* drs: C42 downsampler forward */
		if (data->use_121filter && ptr_func->in_pos_xs > 0) {
			/* Fixed 2 column tile loss for 121 filter */
			C42OutXLeft = ptr_func->in_pos_xs + 2;
		} else {
			C42OutXLeft = ptr_func->in_pos_xs;
		}

		if (ptr_func->in_pos_xe + 1 >= ptr_func->full_size_x_in) {
			C42OutXRight = data->c42_out_frame_w - 1;
		} else {
			C42OutXRight = ptr_func->in_pos_xe;

			/* tile calculation: xe not end position & is odd
			 * HW behavior:
			 *   prz in x size = drs out x size
			 *		   = (drs in x size + 0x01) & ~0x01
			 *   prz out x size = urs in x size
			 *		    = (urs out x size + 0x01) & ~0x01
			 *   can match tile calculation
			 * HW only needs to fill in drs in x size & urs out x size
			 */
			if (!(ptr_func->in_pos_xe & 0x1))
				C42OutXRight -= 1;
		}

		/* prz */
		switch (data->hor_algo) {
		case SCALER_6_TAPS:
			forward_6_taps(C42OutXLeft,	/* C42 out = Scaler input */
					C42OutXRight,	/* C42 out = Scaler input */
					data->c42_out_frame_w - 1,
					data->coeff_step_x,
					data->precision_x,
					data->crop.r.left,
					data->crop.x_sub_px,
					data->c24_in_frame_w - 1,
					2,
					data->prz_back_xs,
					ptr_func->out_cal_order,
					&C24InXLeft,	/* C24 in = Scaler output */
					&C24InXRight,	/* C24 in = Scaler output */
					&ptr_func->bias_x,
					&ptr_func->offset_x,
					&ptr_func->bias_x_c,
					&ptr_func->offset_x_c);
			break;
		case SCALER_SRC_ACC:
			forward_src_acc(C42OutXLeft,	/* C42 out = Scaler input */
					C42OutXRight,	/* C42 out = Scaler input */
					data->c42_out_frame_w - 1,
					data->coeff_step_x,
					data->precision_x,
					data->crop.r.left,
					data->crop.x_sub_px,
					data->c24_in_frame_w - 1,
					2,
					data->prz_back_xs,
					ptr_func->out_cal_order,
					&C24InXLeft,	/* C24 in = Scaler output */
					&C24InXRight,	/* C24 in = Scaler output */
					&ptr_func->bias_x,
					&ptr_func->offset_x,
					&ptr_func->bias_x_c,
					&ptr_func->offset_x_c);
			break;
		case SCALER_CUB_ACC:
			forward_cub_acc(C42OutXLeft,	/* C42 out = Scaler input */
					C42OutXRight,	/* C42 out = Scaler input */
					data->c42_out_frame_w - 1,
					data->coeff_step_x,
					data->precision_x,
					data->crop.r.left,
					data->crop.x_sub_px,
					data->c24_in_frame_w - 1,
					2,
					data->prz_back_xs,
					ptr_func->out_cal_order,
					&C24InXLeft,	/* C24 in = Scaler output */
					&C24InXRight,	/* C24 in = Scaler output */
					&ptr_func->bias_x,
					&ptr_func->offset_x,
					&ptr_func->bias_x_c,
					&ptr_func->offset_x_c);
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}

		C24InXLeft = data->prz_back_xs;

		if (C24InXRight > data->prz_back_xe)
			C24InXRight = data->prz_back_xe;

		/* urs: C24 upsampler forward */
		ptr_func->out_pos_xs = C24InXLeft;
		/* Fixed 1 column tile loss for C24 upsampling while end is even */
		ptr_func->out_pos_xe = C24InXRight - 1;

		if (C24InXRight >= data->c24_in_frame_w - 1)
			ptr_func->out_pos_xe = ptr_func->full_size_x_out - 1;

		if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos)
			ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		/* drs: C42 downsampler forward */
		/* prz */
		switch (data->ver_algo) {
		case SCALER_6_TAPS:
			forward_6_taps(ptr_func->in_pos_ys,
					ptr_func->in_pos_ye,
					ptr_func->full_size_y_in - 1,
					data->coeff_step_y,
					data->precision_y,
					data->crop.r.top,
					data->crop.y_sub_px,
					ptr_func->full_size_y_out - 1,
					ptr_func->out_const_y,
					ptr_func->backward_output_ys_pos,
					ptr_func->out_cal_order,
					&ptr_func->out_pos_ys,
					&ptr_func->out_pos_ye,
					&ptr_func->bias_y,
					&ptr_func->offset_y,
					&ptr_func->bias_y_c,
					&ptr_func->offset_y_c);
			break;
		case SCALER_SRC_ACC:
			forward_src_acc(ptr_func->in_pos_ys,
					ptr_func->in_pos_ye,
					ptr_func->full_size_y_in - 1,
					data->coeff_step_y,
					data->precision_y,
					data->crop.r.top,
					data->crop.y_sub_px,
					ptr_func->full_size_y_out - 1,
					ptr_func->out_const_y,
					ptr_func->backward_output_ys_pos,
					ptr_func->out_cal_order,
					&ptr_func->out_pos_ys,
					&ptr_func->out_pos_ye,
					&ptr_func->bias_y,
					&ptr_func->offset_y,
					&ptr_func->bias_y_c,
					&ptr_func->offset_y_c);
			break;
		case SCALER_CUB_ACC:
			forward_cub_acc(ptr_func->in_pos_ys,
					ptr_func->in_pos_ye,
					ptr_func->full_size_y_in - 1,
					data->coeff_step_y,
					data->precision_y,
					data->crop.r.top,
					data->crop.y_sub_px,
					ptr_func->full_size_y_out - 1,
					ptr_func->out_const_y,
					ptr_func->backward_output_ys_pos,
					ptr_func->out_cal_order,
					&ptr_func->out_pos_ys,
					&ptr_func->out_pos_ye,
					&ptr_func->bias_y,
					&ptr_func->offset_y,
					&ptr_func->bias_y_c,
					&ptr_func->offset_y_c);
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}

		ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;

		if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos)
			ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;

		/* urs: C24 upsampler forward */
	}

	return ISP_MESSAGE_TILE_OK;
}

static enum isp_tile_message tile_wrot_align_out_width(
	struct tile_func_block *ptr_func, const struct wrot_tile_data *data,
	int full_size_x_out)
{
	s32 alignment = 1;
	s32 remain = 0;

	if (MML_FMT_COMPRESS(data->dest_fmt))
		alignment = 32;
	else if (MML_FMT_10BIT_PACKED(data->dest_fmt))
		alignment = 4;

	if (alignment > 1) {
		remain = 0;
		if ((data->rotate == MML_ROT_0 && data->flip) ||
		    (data->rotate == MML_ROT_180 && !data->flip) ||
		    (data->rotate == MML_ROT_270)) {
			/* first tile padding */
			if (ptr_func->out_pos_xs == 0) {
				remain = TILE_MOD(full_size_x_out -
						  ptr_func->out_pos_xe - 1,
						  alignment);
				if (remain)
					remain = alignment - remain;
			} else {
				remain = TILE_MOD(ptr_func->out_pos_xe -
						  ptr_func->out_pos_xs + 1,
						  alignment);
			}
		} else {
			/* last tile padding */
			if (ptr_func->out_pos_xe + 1 < full_size_x_out)
				remain = TILE_MOD(ptr_func->out_pos_xe -
						  ptr_func->out_pos_xs + 1,
						  alignment);
		}
		if (remain)
			ptr_func->out_pos_xe -= remain;
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_wrot_for(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	struct wrot_tile_data *data = &ptr_func->data->wrot;
	s32 remain = 0;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* frame mode */
	if (ptr_tile_reg_map->first_frame) {
		if (data->enable_x_crop &&
		    !ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
			if (ptr_func->min_out_pos_xs > ptr_func->out_pos_xs)
				ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
			if (ptr_func->out_pos_xe > ptr_func->max_out_pos_xe)
				ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
		} else if (data->enable_y_crop &&
			   !ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
			if (ptr_func->min_out_pos_ys > ptr_func->out_pos_ys)
				ptr_func->out_pos_ys = ptr_func->min_out_pos_ys;
			if (ptr_func->out_pos_ye > ptr_func->max_out_pos_ye)
				ptr_func->out_pos_ye = ptr_func->max_out_pos_ye;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		if (ptr_func->backward_output_xs_pos >= ptr_func->out_pos_xs) {
			ptr_func->bias_x = ptr_func->backward_output_xs_pos -
					   ptr_func->out_pos_xs;
			ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (ptr_func->out_pos_xe >= ptr_func->backward_output_xe_pos) {
			ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
		} else {
			/* Check out xe alignment */
			if (ptr_func->out_const_x > 1) {
				remain = TILE_MOD(ptr_func->out_pos_xe + 1,
						  ptr_func->out_const_x);
				if (remain)
					ptr_func->out_pos_xe -= remain;
			}

			/* Check out width alignment */
			tile_wrot_align_out_width(ptr_func, data,
						  ptr_func->full_size_x_out);
		}
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		if (ptr_func->backward_output_ys_pos >= ptr_func->out_pos_ys) {
			ptr_func->bias_y = ptr_func->backward_output_ys_pos -
					   ptr_func->out_pos_ys;
			ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
		} else {
			return MDP_MESSAGE_BACKWARD_START_LESS_THAN_FORWARD;
		}

		if (ptr_func->out_pos_ye >= ptr_func->backward_output_ye_pos) {
			ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
		} else {
			/* Check out ye alignment */
			if (ptr_func->out_const_y > 1) {
				remain = TILE_MOD(ptr_func->out_pos_ye + 1,
						  ptr_func->out_const_y);
				if (remain)
					ptr_func->out_pos_ye -= remain;
			}
		}
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_rdma_back(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map)
{
	struct rdma_tile_data *data = &ptr_func->data->rdma;
	s32 remain = 0, start = 0;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		ptr_func->in_pos_xs = ptr_func->out_pos_xs + ptr_func->crop_bias_x;
		ptr_func->in_pos_xe = ptr_func->out_pos_xe + ptr_func->crop_bias_x;
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		ptr_func->in_pos_ys = ptr_func->out_pos_ys + ptr_func->crop_bias_y;
		ptr_func->in_pos_ye = ptr_func->out_pos_ye + ptr_func->crop_bias_y;
	}

	/* frame mode */
	if (ptr_tile_reg_map->first_frame) {
		/* Specific handle for block format */
		if (ptr_func->in_pos_xe + 1 > data->crop.left + data->crop.width)
			ptr_func->in_pos_xe = data->crop.left + data->crop.width - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment x right in block boundary */
			ptr_func->in_pos_xe = ((1 + (ptr_func->in_pos_xe >>
				data->blk_shift_w)) << data->blk_shift_w) - 1;

			if (ptr_func->in_pos_xe + 1 > ptr_func->full_size_x_in)
				ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
		}

		if (ptr_func->in_const_x > 1) {
			remain = TILE_MOD(ptr_func->in_pos_xe + 1, ptr_func->in_const_x);
			if (remain)
				ptr_func->in_pos_xe += ptr_func->in_const_x - remain;

			remain = TILE_MOD(ptr_func->in_pos_xs, ptr_func->in_const_x);
			if (remain)
				ptr_func->in_pos_xs -= remain;
		}

		if (ptr_func->in_pos_ye + 1 > data->crop.top + data->crop.height)
			ptr_func->in_pos_ye = data->crop.top + data->crop.height - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment y bottom in block boundary */
			ptr_func->in_pos_ye = ((1 + (ptr_func->in_pos_ye >>
				data->blk_shift_h)) << data->blk_shift_h) - 1;

			if (ptr_func->in_pos_ye + 1 > ptr_func->full_size_y_in)
				ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
		}

		if (ptr_func->in_const_y > 1) {
			remain = TILE_MOD(ptr_func->in_pos_ye + 1, ptr_func->in_const_y);
			if (remain)
				ptr_func->in_pos_ye += ptr_func->in_const_y - remain;

			remain = TILE_MOD(ptr_func->in_pos_ys, ptr_func->in_const_y);
			if (remain)
				ptr_func->in_pos_ys -= remain;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	/* Specific handle for block format */
	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		if (ptr_func->in_pos_xe + 1 > data->crop.left + data->crop.width)
			ptr_func->in_pos_xe = data->crop.left + data->crop.width - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment x left in block boundary */
			start = ((ptr_func->in_pos_xs >> data->blk_shift_w) << data->blk_shift_w);

			/* For video block mode, FIFO limit is before crop */
			if (ptr_func->in_pos_xe + 1 > start + ptr_func->in_tile_width)
				ptr_func->in_pos_xe = start + ptr_func->in_tile_width - 1;

			/* Alignment x right in block boundary */
			ptr_func->in_pos_xe = ((1 + (ptr_func->in_pos_xe >>
				data->blk_shift_w)) << data->blk_shift_w) - 1;

			if (ptr_func->in_pos_xe + 1 > ptr_func->full_size_x_in)
				ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
		}

		if (ptr_func->in_const_x > 1) {
			remain = TILE_MOD(ptr_func->in_pos_xe + 1, ptr_func->in_const_x);
			if (remain)
				ptr_func->in_pos_xe += ptr_func->in_const_x - remain;

			remain = TILE_MOD(ptr_func->in_pos_xs, ptr_func->in_const_x);
			if (remain)
				ptr_func->in_pos_xs -= remain;

			if (ptr_func->in_tile_width &&
			    ptr_func->in_pos_xe + 1 >
					ptr_func->in_pos_xs + ptr_func->in_tile_width)
				ptr_func->in_pos_xe =
					ptr_func->in_pos_xs + ptr_func->in_tile_width - 1;
		}
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		if (ptr_func->in_pos_ye + 1 > data->crop.top + data->crop.height)
			ptr_func->in_pos_ye = data->crop.top + data->crop.height - 1;

		if (MML_FMT_BLOCK(data->src_fmt)) {
			/* Alignment y top in block boundary */
			start = ((ptr_func->in_pos_ys >> data->blk_shift_h) << data->blk_shift_h);

			/* For video block mode, FIFO limit is before crop */
			if (ptr_func->in_pos_ye + 1 > start + ptr_func->in_tile_height)
				ptr_func->in_pos_ye = start + ptr_func->in_tile_height - 1;

			/* Alignment y bottom in block boundary */
			ptr_func->in_pos_ye = ((1 + (ptr_func->in_pos_ye >>
				data->blk_shift_h)) << data->blk_shift_h) - 1;

			if (ptr_func->in_pos_ye + 1 > ptr_func->full_size_y_in)
				ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
		}

		if (ptr_func->in_const_y > 1) {
			remain = TILE_MOD(ptr_func->in_pos_ye + 1, ptr_func->in_const_y);
			if (remain)
				ptr_func->in_pos_ye += ptr_func->in_const_y - remain;

			remain = TILE_MOD(ptr_func->in_pos_ys, ptr_func->in_const_y);
			if (remain)
				ptr_func->in_pos_ys -= remain;
		}
	}
	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_prz_back(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	s32 C24InXLeft = 0;
	s32 C24InXRight = 0;
	s32 C42OutXLeft = 0;
	s32 C42OutXRight = 0;
	struct rsz_tile_data *data = &ptr_func->data->rsz;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		/* urs: C24 upsampler backward */
		C24InXLeft = ptr_func->out_pos_xs;

		if (C24InXLeft & 0x1)
			C24InXLeft -= 1;

		if (ptr_func->out_tile_width) {
			if (ptr_func->out_pos_xe + 1 > C24InXLeft + ptr_func->out_tile_width) {
				ptr_func->out_pos_xe = C24InXLeft + ptr_func->out_tile_width - 1;
				ptr_func->h_end_flag = false;
			}
		}

		if (ptr_func->out_pos_xe + 1 >= ptr_func->full_size_x_out) {
			C24InXRight = data->c24_in_frame_w - 1;
		} else {
			/* Fixed 2 column tile loss for C24 upsampling while end is odd */
			C24InXRight = ptr_func->out_pos_xe + 2;

			if (!(ptr_func->out_pos_xe & 0x1))
				C24InXRight -= 1;
		}

		/* prz */
		if (data->prz_out_tile_w && ptr_func->out_tile_width)
			if (C24InXRight + 1 > C24InXLeft + data->prz_out_tile_w)
				C24InXRight = C24InXLeft + data->prz_out_tile_w - 1;

		if (C24InXRight + 1 > data->c24_in_frame_w)
			C24InXRight = data->c24_in_frame_w - 1;
		if (C24InXLeft < 0)
			C24InXLeft = 0;

		switch (data->hor_algo) {
		case SCALER_6_TAPS:
			backward_6_taps(C24InXLeft,	/* C24 in = Scaler output */
					C24InXRight,	/* C24 in = Scaler output */
					data->c24_in_frame_w - 1,
					data->coeff_step_x,
					data->precision_x,
					data->crop.r.left,
					data->crop.x_sub_px,
					data->c42_out_frame_w - 1,
					2,
					&C42OutXLeft,	/* C42 out = Scaler input */
					&C42OutXRight);	/* C42 out = Scaler input */
			break;
		case SCALER_SRC_ACC:
			backward_src_acc(C24InXLeft,	/* C24 in = Scaler output */
					C24InXRight,	/* C24 in = Scaler output */
					data->c24_in_frame_w - 1,
					data->coeff_step_x,
					data->precision_x,
					data->crop.r.left,
					data->crop.x_sub_px,
					data->c42_out_frame_w - 1,
					2,
					&C42OutXLeft,	/* C42 out = Scaler input */
					&C42OutXRight);	/* C42 out = Scaler input */
			break;
		case SCALER_CUB_ACC:
			backward_cub_acc(C24InXLeft,	/* C24 in = Scaler output */
					C24InXRight,	/* C24 in = Scaler output */
					data->c24_in_frame_w - 1,
					data->coeff_step_x,
					data->precision_x,
					data->crop.r.left,
					data->crop.x_sub_px,
					data->c42_out_frame_w - 1,
					2,
					&C42OutXLeft,	/* C42 out = Scaler input */
					&C42OutXRight);	/* C42 out = Scaler input */
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}

		if (ptr_func->in_tile_width)
			if (C42OutXRight + 1 > C42OutXLeft + ptr_func->in_tile_width)
				C42OutXRight = C42OutXLeft + ptr_func->in_tile_width - 1;
		data->prz_back_xs = C24InXLeft;
		data->prz_back_xe = C24InXRight;

		/* drs: C42 downsampler backward */
		ptr_func->in_pos_xs = C42OutXLeft;
		ptr_func->in_pos_xe = C42OutXRight;

		if (data->use_121filter) {
			/* Fixed 2 column tile loss for 121 filter */
			ptr_func->in_pos_xs -= 2;
		}

		if (ptr_func->in_pos_xs < 0)
			ptr_func->in_pos_xs = 0;
		if (ptr_func->in_pos_xe + 1 > ptr_func->full_size_x_in)
			ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		/* urs: C24 upsampler backward */

		/* prz */
		switch (data->ver_algo) {
		case SCALER_6_TAPS:
			backward_6_taps(ptr_func->out_pos_ys,
					ptr_func->out_pos_ye,
					ptr_func->full_size_y_out - 1,
					data->coeff_step_y,
					data->precision_y,
					data->crop.r.top,
					data->crop.y_sub_px,
					ptr_func->full_size_y_in - 1,
					ptr_func->in_const_y,
					&ptr_func->in_pos_ys,
					&ptr_func->in_pos_ye);
			break;
		case SCALER_SRC_ACC:
			backward_src_acc(ptr_func->out_pos_ys,
					ptr_func->out_pos_ye,
					ptr_func->full_size_y_out - 1,
					data->coeff_step_y,
					data->precision_y,
					data->crop.r.top,
					data->crop.y_sub_px,
					ptr_func->full_size_y_in - 1,
					ptr_func->in_const_y,
					&ptr_func->in_pos_ys,
					&ptr_func->in_pos_ye);
			break;
		case SCALER_CUB_ACC:
			backward_cub_acc(ptr_func->out_pos_ys,
					ptr_func->out_pos_ye,
					ptr_func->full_size_y_out - 1,
					data->coeff_step_y,
					data->precision_y,
					data->crop.r.top,
					data->crop.y_sub_px,
					ptr_func->full_size_y_in - 1,
					ptr_func->in_const_y,
					&ptr_func->in_pos_ys,
					&ptr_func->in_pos_ye);
			break;
		default:
			ASSERT(0);
			return MDP_MESSAGE_RESIZER_SCALING_ERROR;
		}

		/* drs: C42 downsampler backward */
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_wrot_back(struct tile_func_block *ptr_func,
				     struct tile_reg_map *ptr_tile_reg_map)
{
	struct wrot_tile_data *data = &ptr_func->data->wrot;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* frame mode */
	if (ptr_tile_reg_map->first_frame) {
		if (data->enable_x_crop &&
		    !ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
			ptr_func->out_pos_xs = data->crop.left;
			ptr_func->out_pos_xe = data->crop.left + data->crop.width - 1;
			ptr_func->in_pos_xs = ptr_func->out_pos_xs;
			ptr_func->in_pos_xe = ptr_func->out_pos_xe;
			ptr_func->min_out_pos_xs = ptr_func->out_pos_xs;
			ptr_func->max_out_pos_xe = ptr_func->out_pos_xe;
		} else if (data->enable_y_crop &&
			   !ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
			ptr_func->out_pos_ys = data->crop.top;
			ptr_func->out_pos_ye = data->crop.top + data->crop.height - 1;
			ptr_func->in_pos_ys = ptr_func->out_pos_ys;
			ptr_func->in_pos_ye = ptr_func->out_pos_ye;
			ptr_func->min_out_pos_ys = ptr_func->out_pos_ys;
			ptr_func->max_out_pos_ye = ptr_func->out_pos_ye;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		int full_size_x_out = ptr_func->full_size_x_out;

		if (data->enable_x_crop) {
			if (ptr_func->valid_h_no == 0) {
				/* first tile */
				ptr_func->out_pos_xs = data->crop.left;
				ptr_func->in_pos_xs = ptr_func->out_pos_xs;
			}
			if (ptr_func->out_tile_width) {
				ptr_func->out_pos_xe = ptr_func->out_pos_xs +
						       ptr_func->out_tile_width - 1;
				ptr_func->in_pos_xe = ptr_func->out_pos_xe;
			}

			full_size_x_out = data->crop.left + data->crop.width;

			if (ptr_func->out_pos_xe + 1 >= full_size_x_out) {
				ptr_func->in_pos_xe = full_size_x_out - 1;
				/* ptr_func->h_end_flag = true; */
			}
		}

		if (data->alpharot) {
			if (ptr_func->out_pos_xe + 1 < full_size_x_out &&
			    ptr_func->out_pos_xe + 9 + 1 > full_size_x_out &&
			    ptr_func->out_pos_xe != ptr_func->out_pos_xs) {
				ptr_func->out_pos_xe = full_size_x_out - 9 - 1;
				ptr_func->in_pos_xe  = full_size_x_out - 9 - 1;

				ptr_func->out_pos_xe = ((ptr_func->out_pos_xe + 1) >> 2 << 2) - 1;
				ptr_func->in_pos_xe  = ((ptr_func->in_pos_xe + 1) >> 2 << 2) - 1;
			}
		}

		/* Check out width alignment */
		tile_wrot_align_out_width(ptr_func, data, full_size_x_out);
	}

	if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
		int full_size_y_out = ptr_func->full_size_y_out;

		if (data->enable_y_crop) {
			if (ptr_func->valid_v_no == 0) {
				/* first tile */
				ptr_func->out_pos_ys = data->crop.top;
				ptr_func->in_pos_ys = ptr_func->out_pos_ys;
			}
			if (ptr_func->out_tile_height) {
				ptr_func->out_pos_ye = ptr_func->out_pos_ys +
						       ptr_func->out_tile_height - 1;
				ptr_func->in_pos_ye = ptr_func->out_pos_ye;
			}

			full_size_y_out = data->crop.top + data->crop.height;

			if (ptr_func->out_pos_ye + 1 >= full_size_y_out) {
				ptr_func->in_pos_ye = full_size_y_out - 1;
				/* ptr_func->v_end_flag = true; */
			}
		}

		/* Check out height alignment */
	}

	return ISP_MESSAGE_TILE_OK;
}

enum isp_tile_message tile_dlo_back(struct tile_func_block *ptr_func,
				    struct tile_reg_map *ptr_tile_reg_map)
{
	struct dlo_tile_data *data = &ptr_func->data->dlo;

	if (unlikely(!data))
		return MDP_MESSAGE_NULL_DATA;

	/* frame mode */
	if (ptr_tile_reg_map->first_frame) {
		if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
			ptr_func->out_pos_xs = data->crop.left;
			ptr_func->out_pos_xe = data->crop.left + data->crop.width - 1;
			ptr_func->in_pos_xs = ptr_func->out_pos_xs;
			ptr_func->in_pos_xe = ptr_func->out_pos_xe;
			ptr_func->min_out_pos_xs = ptr_func->out_pos_xs;
			ptr_func->max_out_pos_xe = ptr_func->out_pos_xe;
			if (ptr_func->in_pos_xs > 0)
				ptr_func->tdr_edge &= ~TILE_EDGE_LEFT_MASK;
		}
		if (!ptr_tile_reg_map->skip_y_cal && !ptr_func->tdr_v_disable_flag) {
			ptr_func->out_pos_ys = data->crop.top;
			ptr_func->out_pos_ye = data->crop.top + data->crop.height - 1;
			ptr_func->in_pos_ys = ptr_func->out_pos_ys;
			ptr_func->in_pos_ye = ptr_func->out_pos_ye;
			ptr_func->min_out_pos_ys = ptr_func->out_pos_ys;
			ptr_func->max_out_pos_ye = ptr_func->out_pos_ye;
			if (ptr_func->in_pos_ys > 0)
				ptr_func->tdr_edge &= ~TILE_EDGE_TOP_MASK;
		}
		return ISP_MESSAGE_TILE_OK;
	}

	if (!ptr_tile_reg_map->skip_x_cal && !ptr_func->tdr_h_disable_flag) {
		int full_size_x_out = ptr_func->full_size_x_out;

		if (data->enable_x_crop) {
			if (ptr_func->valid_h_no == 0) {
				/* first tile */
				ptr_func->out_pos_xs = data->crop.left;
				ptr_func->in_pos_xs = ptr_func->out_pos_xs;
			}

			full_size_x_out = data->crop.left + data->crop.width;

			if (ptr_func->out_pos_xe + 1 >= full_size_x_out) {
				ptr_func->in_pos_xe = full_size_x_out - 1;
				/* ptr_func->h_end_flag = true; */
			}
		}
	}

	return ISP_MESSAGE_TILE_OK;
}


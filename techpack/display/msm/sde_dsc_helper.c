// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
 */

#include "msm_drv.h"
#include "sde_dsc_helper.h"
#include "sde_kms.h"


#define SDE_DSC_PPS_SIZE       128

enum sde_dsc_ratio_type {
	DSC_V11_8BPC_8BPP,
	DSC_V11_10BPC_8BPP,
	DSC_V11_10BPC_10BPP,
	DSC_V11_SCR1_8BPC_8BPP,
	DSC_V11_SCR1_10BPC_8BPP,
	DSC_V11_SCR1_10BPC_10BPP,
	DSC_V12_444_8BPC_8BPP = DSC_V11_SCR1_8BPC_8BPP,
	DSC_V12_444_10BPC_8BPP = DSC_V11_SCR1_10BPC_8BPP,
	DSC_V12_444_10BPC_10BPP = DSC_V11_SCR1_10BPC_10BPP,
	DSC_V12_422_8BPC_7BPP,
	DSC_V12_422_8BPC_8BPP,
	DSC_V12_422_10BPC_7BPP,
	DSC_V12_422_10BPC_10BPP,
	DSC_V12_420_8BPC_6BPP,
	DSC_V12_420_10BPC_6BPP,
	DSC_V12_420_10BPC_7_5BPP,
	DSC_RATIO_TYPE_MAX
};

static u16 sde_dsc_rc_buf_thresh[DSC_NUM_BUF_RANGES - 1] =
	{0x0e, 0x1c, 0x2a, 0x38, 0x46, 0x54,
		0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e};

/*
 * Rate control - Min QP values for each ratio type in sde_dsc_ratio_type
 */
static char sde_dsc_rc_range_min_qp[DSC_RATIO_TYPE_MAX][DSC_NUM_BUF_RANGES] = {
	/* DSC v1.1 */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 17},
	{0, 4, 5, 6, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 15},
	/* DSC v1.1 SCR and DSC v1.2 RGB 444 */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16},
	{0, 4, 5, 6, 7, 7, 7, 7, 7, 7, 9, 9, 9, 11, 15},
	/* DSC v1.2 YUV422 */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 10},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16},
	{0, 4, 9, 9, 11, 11, 11, 11, 11, 11, 13, 13, 13, 17, 20},
	{0, 2, 3, 4, 5, 5, 5, 6, 6, 7, 8, 8, 9, 11, 12},
	/* DSC v1.2 YUV420 */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 10},
	{0, 2, 3, 4, 6, 7, 7, 7, 7, 7, 9, 9, 9, 11, 14},
	{0, 2, 3, 4, 5, 5, 5, 6, 6, 7, 8, 8, 9, 11, 12},
};

/*
 * Rate control - Max QP values for each ratio type in sde_dsc_ratio_type
 */
static char sde_dsc_rc_range_max_qp[DSC_RATIO_TYPE_MAX][DSC_NUM_BUF_RANGES] = {
	/* DSC v1.1 */
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15},
	{4, 8, 9, 10, 11, 11, 11, 12, 13, 14, 15, 16, 17, 17, 19},
	{7, 8, 9, 10, 11, 11, 11, 12, 13, 13, 14, 14, 15, 15, 16},
	/* DSC v1.1 SCR and DSC v1.2 RGB 444 */
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 10, 11, 11, 12, 13},
	{8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17},
	{7, 8, 9, 10, 11, 11, 11, 12, 13, 13, 14, 14, 15, 15, 16},
	/* DSC v1.2 YUV422 */
	{3, 4, 5, 6, 7, 7, 7, 8, 9, 9, 10, 10, 11, 11, 12},
	{2, 4, 5, 6, 7, 7, 7, 8, 8, 9, 9, 9, 9, 10, 11},
	{7, 8, 9, 10, 11, 11, 11, 12, 13, 13, 14, 14, 15, 15, 16},
	{2, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13},
	/* DSC v1.2 YUV420 */
	{2, 4, 5, 6, 7, 7, 7, 8, 8, 9, 9, 9, 9, 10, 12},
	{2, 5, 7, 8, 9, 10, 11, 12, 12, 13, 13, 13, 13, 14, 15},
	{2, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13},
	};

/*
 * Rate control - bpg offset values for each ratio type in sde_dsc_ratio_type
 */
static char sde_dsc_rc_range_bpg[DSC_RATIO_TYPE_MAX][DSC_NUM_BUF_RANGES] = {
	/* DSC v1.1 */
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12, -12, -12},
	/* DSC v1.1 SCR and DSC V1.2 RGB 444 */
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12, -12, -12},
	/* DSC v1.2 YUV422 */
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -10, -12, -12, -12},
	{10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12, -12, -12},
	/* DSC v1.2 YUV420 */
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12},
	{10, 8, 6, 4, 2, 0, -2, -4, -6, -8, -10, -10, -12, -12, -12},
};

/**
 * Maps to lookup the sde_dsc_ratio_type index used in rate control tables
 */
static struct sde_dsc_v1_1_table_index_lut {
	int scr_ver;
	u32 bpc;
	u32 bpp;
	u32 type;
} sde_dsc_v1_1_index_map[] = {
	{0, 8, 8, DSC_V11_8BPC_8BPP},
	{0, 10, 8, DSC_V11_10BPC_8BPP},
	{0, 10, 10, DSC_V11_10BPC_10BPP},

	{1, 8, 8, DSC_V11_SCR1_8BPC_8BPP},
	{1, 10, 8, DSC_V11_SCR1_10BPC_8BPP},
	{1, 10, 10, DSC_V11_SCR1_10BPC_10BPP},
};

static struct sde_dsc_v1_2_table_index_lut {
	u32 fmt;
	u32 bpc;
	u32 bpp;
	u32 type;
} sde_dsc_v1_2_index_map[] = {
	{MSM_CHROMA_444, 8, 8, DSC_V12_444_8BPC_8BPP},
	{MSM_CHROMA_444, 10, 8, DSC_V12_444_10BPC_8BPP},
	{MSM_CHROMA_444, 10, 10, DSC_V12_444_10BPC_10BPP},

	{MSM_CHROMA_422, 8, 7, DSC_V12_422_8BPC_7BPP},
	{MSM_CHROMA_422, 8, 8, DSC_V12_422_8BPC_8BPP},
	{MSM_CHROMA_422, 10, 7, DSC_V12_422_10BPC_7BPP},
	{MSM_CHROMA_422, 10, 10, DSC_V12_422_10BPC_10BPP},

	{MSM_CHROMA_420, 8, 6, DSC_V12_420_8BPC_6BPP},
	{MSM_CHROMA_420, 10, 6, DSC_V12_420_10BPC_6BPP},
};

static int _get_rc_table_index(struct drm_dsc_config *dsc, int scr_ver)
{
	u32 bpp, bpc, i, fmt = MSM_CHROMA_444;

	if (dsc->dsc_version_major != 0x1) {
		SDE_ERROR("unsupported major version %d\n",
				dsc->dsc_version_major);
		return -EINVAL;
	}

	bpc = dsc->bits_per_component;
	bpp = DSC_BPP(*dsc);

	if (dsc->native_422)
		fmt = MSM_CHROMA_422;
	else if (dsc->native_420)
		fmt = MSM_CHROMA_420;

	if (dsc->dsc_version_minor == 0x1) {
		for (i = 0; i < ARRAY_SIZE(sde_dsc_v1_1_index_map); i++) {
			if (bpc == sde_dsc_v1_1_index_map[i].bpc &&
			    bpp == sde_dsc_v1_1_index_map[i].bpp &&
			    scr_ver == sde_dsc_v1_1_index_map[i].scr_ver)
				return sde_dsc_v1_1_index_map[i].type;
		}
	} else if (dsc->dsc_version_minor == 0x2) {
		for (i = 0; i < ARRAY_SIZE(sde_dsc_v1_2_index_map); i++) {
			if (bpc == sde_dsc_v1_2_index_map[i].bpc &&
			    bpp == sde_dsc_v1_2_index_map[i].bpp &&
			    fmt == sde_dsc_v1_2_index_map[i].fmt)
				return sde_dsc_v1_2_index_map[i].type;
		}
	}

	SDE_ERROR("unsupported DSC v%d.%dr%d, bpc:%d, bpp:%d, fmt:0x%x\n",
			dsc->dsc_version_major, dsc->dsc_version_minor,
			scr_ver, bpc, bpp, fmt);
	return -EINVAL;
}

u8 _get_dsc_v1_2_bpg_offset(struct drm_dsc_config *dsc)
{
	u8 bpg_offset = 0;
	u8 uncompressed_bpg_rate;
	u8 bpp = DSC_BPP(*dsc);

	if (dsc->slice_height < 8)
		bpg_offset = 2 * (dsc->slice_height - 1);
	else if (dsc->slice_height < 20)
		bpg_offset = 12;
	else if (dsc->slice_height <= 30)
		bpg_offset = 13;
	else if (dsc->slice_height < 42)
		bpg_offset = 14;
	else
		bpg_offset = 15;

	if (dsc->native_422)
		uncompressed_bpg_rate = 3 * bpp * 4;
	else if (dsc->native_420)
		uncompressed_bpg_rate = 3 * bpp;
	else
		uncompressed_bpg_rate = (3 * bpp + 2) * 3;

	if (bpg_offset < (uncompressed_bpg_rate - (3 * bpp)))
		return bpg_offset;
	else
		return (uncompressed_bpg_rate - (3 * bpp));
}

int sde_dsc_populate_dsc_config(struct drm_dsc_config *dsc, int scr_ver) {
	int bpp, bpc;
	int groups_per_line, groups_total;
	int min_rate_buffer_size;
	int hrd_delay;
	int pre_num_extra_mux_bits, num_extra_mux_bits;
	int slice_bits;
	int data;
	int final_value, final_scale;
	int i, ratio_idx;

	dsc->rc_model_size = 8192;

	if ((dsc->dsc_version_major == 0x1) &&
			(dsc->dsc_version_minor == 0x1)) {
		if (scr_ver == 0x1)
			dsc->first_line_bpg_offset = 15;
		else
			dsc->first_line_bpg_offset = 12;
	} else if (dsc->dsc_version_minor == 0x2) {
		dsc->first_line_bpg_offset = _get_dsc_v1_2_bpg_offset(dsc);
	}

	dsc->rc_edge_factor = 6;
	dsc->rc_tgt_offset_high = 3;
	dsc->rc_tgt_offset_low = 3;
	dsc->simple_422 = 0;
	dsc->convert_rgb = 1;
	dsc->vbr_enable = 0;

	bpp = DSC_BPP(*dsc);
	bpc = dsc->bits_per_component;

	ratio_idx = _get_rc_table_index(dsc, scr_ver);
	if (ratio_idx == -EINVAL)
		return ratio_idx;

	for (i = 0; i < DSC_NUM_BUF_RANGES - 1; i++)
		dsc->rc_buf_thresh[i] = sde_dsc_rc_buf_thresh[i];

	for (i = 0; i < DSC_NUM_BUF_RANGES; i++) {
		dsc->rc_range_params[i].range_min_qp =
			sde_dsc_rc_range_min_qp[ratio_idx][i];
		dsc->rc_range_params[i].range_max_qp =
			sde_dsc_rc_range_max_qp[ratio_idx][i];
		dsc->rc_range_params[i].range_bpg_offset =
			sde_dsc_rc_range_bpg[ratio_idx][i];
	}

	if (bpp == 8) {
		dsc->initial_offset = 6144;
		dsc->initial_xmit_delay = 512;
	} else if (bpp == 10) {
		dsc->initial_offset = 5632;
		dsc->initial_xmit_delay = 410;
	} else {
		dsc->initial_offset = 2048;
		dsc->initial_xmit_delay = 341;
	}

	dsc->line_buf_depth = bpc + 1;

	if (bpc == 8) {
		dsc->flatness_min_qp = 3;
		dsc->flatness_max_qp = 12;
		dsc->rc_quant_incr_limit0 = 11;
		dsc->rc_quant_incr_limit1 = 11;
		dsc->mux_word_size = DSC_MUX_WORD_SIZE_8_10_BPC;
	} else if (bpc == 10) { /* 10bpc */
		dsc->flatness_min_qp = 7;
		dsc->flatness_max_qp = 16;
		dsc->rc_quant_incr_limit0 = 15;
		dsc->rc_quant_incr_limit1 = 15;
		dsc->mux_word_size = DSC_MUX_WORD_SIZE_8_10_BPC;
	} else { /* 12 bpc */
		dsc->flatness_min_qp = 11;
		dsc->flatness_max_qp = 20;
		dsc->rc_quant_incr_limit0 = 19;
		dsc->rc_quant_incr_limit1 = 19;
		dsc->mux_word_size = DSC_MUX_WORD_SIZE_12_BPC;
	}
	if ((dsc->dsc_version_minor == 0x2) && (dsc->native_420)) {
		dsc->second_line_bpg_offset = 12;
		dsc->second_line_offset_adj = 512;
		dsc->nsl_bpg_offset = 2048 *
			(DIV_ROUND_UP(dsc->second_line_bpg_offset,
				(dsc->slice_height - 1)));
	}

	groups_per_line = DIV_ROUND_UP(dsc->slice_width, 3);

	dsc->slice_chunk_size = dsc->slice_width * bpp / 8;
	if ((dsc->slice_width * bpp) % 8)
		dsc->slice_chunk_size++;

	/* rbs-min */
	min_rate_buffer_size =  dsc->rc_model_size - dsc->initial_offset +
			dsc->initial_xmit_delay * bpp +
			groups_per_line * dsc->first_line_bpg_offset;

	hrd_delay = DIV_ROUND_UP(min_rate_buffer_size, bpp);

	dsc->initial_dec_delay = hrd_delay - dsc->initial_xmit_delay;

	dsc->initial_scale_value = 8 * dsc->rc_model_size /
			(dsc->rc_model_size - dsc->initial_offset);

	slice_bits = 8 * dsc->slice_chunk_size * dsc->slice_height;

	groups_total = groups_per_line * dsc->slice_height;

	data = dsc->first_line_bpg_offset * 2048;

	dsc->nfl_bpg_offset = DIV_ROUND_UP(data, (dsc->slice_height - 1));

	pre_num_extra_mux_bits = 3 * (dsc->mux_word_size + (4 * bpc + 4) - 2);

	num_extra_mux_bits = pre_num_extra_mux_bits - (dsc->mux_word_size -
		((slice_bits - pre_num_extra_mux_bits) % dsc->mux_word_size));

	data = 2048 * (dsc->rc_model_size - dsc->initial_offset
		+ num_extra_mux_bits);
	dsc->slice_bpg_offset = DIV_ROUND_UP(data, groups_total);

	data = dsc->initial_xmit_delay * bpp;
	final_value =  dsc->rc_model_size - data + num_extra_mux_bits;

	final_scale = 8 * dsc->rc_model_size /
		(dsc->rc_model_size - final_value);

	dsc->final_offset = final_value;

	data = (final_scale - 9) * (dsc->nfl_bpg_offset +
		dsc->slice_bpg_offset);
	dsc->scale_increment_interval = (2048 * dsc->final_offset) / data;

	dsc->scale_decrement_interval = groups_per_line /
		(dsc->initial_scale_value - 8);

	return 0;
}

int sde_dsc_populate_dsc_private_params(struct msm_display_dsc_info *dsc_info,
		int intf_width)
{
	int  mod_offset;
	int slice_per_pkt, slice_per_intf;
	int bytes_in_slice, total_bytes_per_intf;
	u16 bpp;
	u32 bytes_in_dsc_pair;
	u32 total_bytes_in_dsc_pair;

	if (!dsc_info || !dsc_info->config.slice_width ||
			!dsc_info->config.slice_height ||
			intf_width < dsc_info->config.slice_width) {
		SDE_ERROR("invalid input, intf_width=%d slice_width=%d\n",
			intf_width, dsc_info ? dsc_info->config.slice_width :
			-1);
		return -EINVAL;
	}

	mod_offset = dsc_info->config.slice_width % 3;

	switch (mod_offset) {
	case 0:
		dsc_info->slice_last_group_size = 2;
		break;
	case 1:
		dsc_info->slice_last_group_size = 0;
		break;
	case 2:
		dsc_info->slice_last_group_size = 1;
		break;
	default:
		break;
	}

	dsc_info->det_thresh_flatness =
		2 << (dsc_info->config.bits_per_component - 8);

	slice_per_pkt = dsc_info->slice_per_pkt;
	slice_per_intf = DIV_ROUND_UP(intf_width,
			dsc_info->config.slice_width);

	/*
	 * If slice_per_pkt is greater than slice_per_intf then default to 1.
	 * This can happen during partial update.
	 */
	if (slice_per_pkt > slice_per_intf)
		slice_per_pkt = 1;

	bpp = DSC_BPP(dsc_info->config);
	bytes_in_slice = DIV_ROUND_UP(dsc_info->config.slice_width *
			bpp, 8);
	total_bytes_per_intf = bytes_in_slice * slice_per_intf;

	dsc_info->eol_byte_num = total_bytes_per_intf % 3;
	dsc_info->pclk_per_line =  DIV_ROUND_UP(total_bytes_per_intf, 3);
	dsc_info->bytes_in_slice = bytes_in_slice;
	dsc_info->bytes_per_pkt = bytes_in_slice * slice_per_pkt;
	dsc_info->pkt_per_line = slice_per_intf / slice_per_pkt;

	bytes_in_dsc_pair = DIV_ROUND_UP(bytes_in_slice * 2, 3);
	if (bytes_in_dsc_pair % 8) {
		dsc_info->dsc_4hsmerge_padding = 8 - (bytes_in_dsc_pair % 8);
		total_bytes_in_dsc_pair = bytes_in_dsc_pair +
				dsc_info->dsc_4hsmerge_padding;
		if (total_bytes_in_dsc_pair % 16)
			dsc_info->dsc_4hsmerge_alignment = 16 -
					(total_bytes_in_dsc_pair % 16);
	}

	return 0;
}

int sde_dsc_create_pps_buf_cmd(struct msm_display_dsc_info *dsc_info,
		char *buf, int pps_id, u32 len)
{
	struct drm_dsc_config *dsc = &dsc_info->config;
	char *bp = buf;
	char data;
	u32 i, bpp;

	if (len < SDE_DSC_PPS_SIZE)
		return -EINVAL;

	memset(buf, 0, len);
	/* pps0 */
	*bp++ = (dsc->dsc_version_minor |
			dsc->dsc_version_major << 4);
	*bp++ = (pps_id & 0xff);		/* pps1 */
	bp++;					/* pps2, reserved */

	data = dsc->line_buf_depth & 0x0f;
	data |= ((dsc->bits_per_component & 0xf) << DSC_PPS_BPC_SHIFT);
	*bp++ = data;				/* pps3 */

	bpp = dsc->bits_per_pixel;
	data = (bpp >> DSC_PPS_MSB_SHIFT);
	data &= 0x03;				/* upper two bits */
	data |= ((dsc->block_pred_enable & 0x1) << 5);
	data |= ((dsc->convert_rgb & 0x1) << 4);
	data |= ((dsc->simple_422 & 0x1) << 3);
	data |= ((dsc->vbr_enable & 0x1) << 2);
	*bp++ = data;				/* pps4 */
	*bp++ = (bpp & DSC_PPS_LSB_MASK);	/* pps5 */

	*bp++ = ((dsc->pic_height >> 8) & 0xff); /* pps6 */
	*bp++ = (dsc->pic_height & 0x0ff);	/* pps7 */
	*bp++ = ((dsc->pic_width >> 8) & 0xff);	/* pps8 */
	*bp++ = (dsc->pic_width & 0x0ff);	/* pps9 */

	*bp++ = ((dsc->slice_height >> 8) & 0xff);/* pps10 */
	*bp++ = (dsc->slice_height & 0x0ff);	/* pps11 */
	*bp++ = ((dsc->slice_width >> 8) & 0xff); /* pps12 */
	*bp++ = (dsc->slice_width & 0x0ff);	/* pps13 */

	*bp++ = ((dsc->slice_chunk_size >> 8) & 0xff);/* pps14 */
	*bp++ = (dsc->slice_chunk_size & 0x0ff);	/* pps15 */

	*bp++ = (dsc->initial_xmit_delay >> 8) & 0x3; /* pps16 */
	*bp++ = (dsc->initial_xmit_delay & 0xff);/* pps17 */

	*bp++ = ((dsc->initial_dec_delay >> 8) & 0xff); /* pps18 */
	*bp++ = (dsc->initial_dec_delay & 0xff);/* pps19 */

	bp++;				/* pps20, reserved */

	*bp++ = (dsc->initial_scale_value & 0x3f); /* pps21 */

	*bp++ = ((dsc->scale_increment_interval >> 8) & 0xff); /* pps22 */
	*bp++ = (dsc->scale_increment_interval & 0xff); /* pps23 */

	*bp++ = ((dsc->scale_decrement_interval >> 8) & 0xf); /* pps24 */
	*bp++ = (dsc->scale_decrement_interval & 0x0ff);/* pps25 */

	bp++;					/* pps26, reserved */

	*bp++ = (dsc->first_line_bpg_offset & 0x1f);/* pps27 */

	*bp++ = ((dsc->nfl_bpg_offset >> 8) & 0xff);/* pps28 */
	*bp++ = (dsc->nfl_bpg_offset & 0x0ff);	/* pps29 */
	*bp++ = ((dsc->slice_bpg_offset >> 8) & 0xff);/* pps30 */
	*bp++ = (dsc->slice_bpg_offset & 0x0ff);/* pps31 */

	*bp++ = ((dsc->initial_offset >> 8) & 0xff);/* pps32 */
	*bp++ = (dsc->initial_offset & 0x0ff);	/* pps33 */

	*bp++ = ((dsc->final_offset >> 8) & 0xff);/* pps34 */
	*bp++ = (dsc->final_offset & 0x0ff);	/* pps35 */

	*bp++ = (dsc->flatness_min_qp & 0x1f);	/* pps36 */
	*bp++ = (dsc->flatness_max_qp & 0x1f);	/* pps37 */

	*bp++ = ((dsc->rc_model_size >> 8) & 0xff);/* pps38 */
	*bp++ = (dsc->rc_model_size & 0x0ff);	/* pps39 */

	*bp++ = (dsc->rc_edge_factor & 0x0f);	/* pps40 */

	*bp++ = (dsc->rc_quant_incr_limit0 & 0x1f);	/* pps41 */
	*bp++ = (dsc->rc_quant_incr_limit1 & 0x1f);	/* pps42 */

	data = ((dsc->rc_tgt_offset_high & 0xf) << 4);
	data |= (dsc->rc_tgt_offset_low & 0x0f);
	*bp++ = data;				/* pps43 */

	for (i = 0; i < DSC_NUM_BUF_RANGES - 1; i++)
		*bp++ = (dsc->rc_buf_thresh[i] & 0xff); /* pps44 - pps57 */

	for (i = 0; i < DSC_NUM_BUF_RANGES; i++) {
		/* pps58 - pps87 */
		data = (dsc->rc_range_params[i].range_min_qp & 0x1f);
		data <<= 3;
		data |= ((dsc->rc_range_params[i].range_max_qp >> 2) & 0x07);
		*bp++ = data;
		data = (dsc->rc_range_params[i].range_max_qp & 0x03);
		data <<= 6;
		data |= (dsc->rc_range_params[i].range_bpg_offset & 0x3f);
		*bp++ = data;
	}

	if (dsc->dsc_version_minor == 0x2) {
		if (dsc->native_422)
			data = BIT(0);
		else if (dsc->native_420)
			data = BIT(1);
		*bp++ = data;				/* pps88 */
		*bp++ = dsc->second_line_bpg_offset;	/* pps89 */

		*bp++ = ((dsc->nsl_bpg_offset >> 8) & 0xff);/* pps90 */
		*bp++ = (dsc->nsl_bpg_offset & 0x0ff);	/* pps91 */

		*bp++ = ((dsc->second_line_offset_adj >> 8) & 0xff); /* pps92*/
		*bp++ = (dsc->second_line_offset_adj & 0x0ff);	/* pps93 */

		/* rest bytes are reserved and set to 0 */
	}

	return 0;
}


// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "msm_drv.h"
#include "sde_vdc_helper.h"

enum sde_vdc_profile_type {
	VDC_RGB_444_8BPC_8BPP,
	VDC_RGB_444_8BPC_6BPP,
	VDC_RGB_444_10BPC_10BPP,
	VDC_RGB_444_108BPC_8BPP,
	VDC_RGB_444_10BPC_7BPP,
	VDC_RGB_444_10BPC_6BPP,
	VDC_YUV_422_8BPC_6BPP,
	VDC_YUV_422_8BPC_5BPP,
	VDC_YUV_422_8BPC_4_75BPP,
	VDC_YUV_422_10BPC_8BPP,
	VDC_YUV_422_10BPC_6BPP,
	VDC_YUV_422_10BPC_5_5BPP,
	VDC_YUV_422_10BPC_5PP,
	VDC_PROFILE_MAX
};

static u8 sde_vdc_mppf_bpc_r_y[VDC_PROFILE_MAX] = {
	2, 1, 3, 2, 2, 1, 3, 2, 2, 4, 3, 2, 2};

static u8 sde_vdc_mppf_bpc_g_cb[VDC_PROFILE_MAX] = {
	2, 2, 3, 2, 2, 2, 2, 2, 1, 3, 2, 2, 2};

static u8 sde_vdc_mppf_bpc_b_cr[VDC_PROFILE_MAX] = {
	2, 1, 3, 2, 2, 1, 2, 2, 1, 3, 2, 2, 2};

static u8 sde_vdc_mppf_bpc_y[VDC_PROFILE_MAX] = {
	2, 2, 3, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0};

static u8 sde_vdc_mppf_bpc_co[VDC_PROFILE_MAX] = {
	2, 1, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0};

static u8 sde_vdc_mppf_bpc_cg[VDC_PROFILE_MAX] = {
	2, 1, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0};

static u8 sde_vdc_flat_qp_vf_fbls[VDC_PROFILE_MAX] = {
	20, 24, 24, 24, 24, 24, 24, 24, 24, 8, 20, 18, 24};

static u8 sde_vdc_flat_qp_vf_nfbls[VDC_PROFILE_MAX] = {
	24, 28, 28, 28, 28, 28, 28, 28, 28, 16, 24, 20, 28};

static u8 sde_vdc_flat_qp_sf_fbls[VDC_PROFILE_MAX] = {
	24, 28, 28, 28, 28, 28, 28, 28, 28, 16, 24, 20, 28};

static u8 sde_vdc_flat_qp_sf_nbls[VDC_PROFILE_MAX] = {
	28, 40, 32, 28, 32, 28, 36, 36, 36, 16, 24, 24, 28};

static u16 sde_vdc_flat_qp_lut[VDC_PROFILE_MAX][VDC_FLAT_QP_LUT_SIZE] = {
	{20, 20, 24, 24, 28, 32, 36, 40},
	{24, 24, 28, 32, 36, 40, 40, 40},
	{24, 24, 28, 32, 32, 36, 36, 36},
	{20, 24, 28, 28, 32, 36, 40, 44},
	{20, 24, 28, 32, 32, 36, 36, 40},
	{24, 28, 32, 32, 36, 40, 40, 40},
	{24, 28, 32, 34, 36, 38, 40, 40},
	{24, 28, 32, 36, 40, 42, 44, 44},
	{24, 28, 32, 36, 40, 42, 44, 44},
	{0, 8, 10, 12, 14, 16, 18, 20},
	{12, 16, 20, 20, 20, 24, 24, 28},
	{16, 18, 20, 22, 24, 26, 28, 28},
	{20, 22, 24, 26, 28, 28, 32, 32},
};

static u16 sde_vdc_max_qp_lut[VDC_PROFILE_MAX][VDC_MAX_QP_LUT_SIZE] = {
	{28, 28, 32, 32, 36, 42, 42, 48},
	{32, 32, 36, 40, 44, 48, 48, 52},
	{32, 32, 36, 36, 36, 40, 44, 48},
	{24, 28, 32, 32, 36, 40, 44, 48},
	{28, 28, 32, 32, 36, 42, 42, 48},
	{28, 32, 36, 40, 44, 44, 46, 52},
	{32, 32, 36, 40, 40, 44, 48, 48},
	{32, 32, 36, 40, 44, 48, 50, 52},
	{32, 32, 36, 40, 44, 48, 50, 52},
	{8, 12, 12, 16, 20, 24, 28, 28},
	{18, 20, 22, 24, 28, 30, 32, 40},
	{18, 20, 22, 24, 28, 30, 32, 40},
	{20, 20, 24, 24, 28, 28, 32, 36},
};

static u16 sde_vdc_tar_del_lut[VDC_PROFILE_MAX][VDC_TAR_DEL_LUT_SIZE] = {
	{128, 117, 107, 96, 85, 75, 64, 53, 43, 32, 24, 11, 0, 0, 0, 0},
	{96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0, 0, 0, 0},
	{160, 147, 133, 120, 107, 93, 80, 67, 53, 40, 27, 13, 0, 0, 0, 0},
	{128, 117, 107, 96, 85, 75, 64, 53, 43, 32, 21, 11, 0, 0, 0, 0},
	{112, 103, 93, 84, 75, 95, 56, 47, 37, 28, 19, 9, 0, 0, 0, 0},
	{96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0, 0, 0, 0},
	{96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0, 0, 0, 0},
	{80, 73, 67, 60, 53, 47, 40, 33, 27, 20, 13, 7, 0, 0, 0, 0},
	{76, 70, 63, 57, 51, 44, 38, 32, 25, 19, 13, 6, 0, 0, 0, 0},
	{128, 117, 107, 96, 85, 75, 64, 53, 43, 32, 21, 11, 0, 0, 0, 0},
	{96, 88, 80, 72, 64, 56, 48, 40, 32, 24, 16, 8, 0, 0, 0, 0},
	{88, 81, 73, 66, 59, 51, 44, 37, 29, 22, 15, 7, 0, 0, 0, 0},
	{80, 73, 67, 60, 53, 47, 40, 33, 27, 20, 13, 7, 0, 0, 0, 0},
};

static u16 sde_vdc_lbda_brate_lut[VDC_PROFILE_MAX][VDC_LBDA_BRATE_LUT_SIZE] = {
	{4, 6, 10, 16, 25, 40, 64, 102, 161, 256, 406, 645, 1024, 1625,
	2580, 4095},
	{8, 12, 18, 28, 42, 64, 97, 147, 223, 338, 512, 776, 1176, 1782,
	2702, 4095},
	{16, 23, 34, 48, 70, 102, 147, 213, 308, 446, 645, 933, 1351,
	1955, 2829, 4095},
	{8, 12, 18, 28, 42, 64, 97, 147, 223, 338, 512, 776, 1176, 1782,
	2702, 4095},
	{32, 44, 61, 84, 117, 161, 223, 308, 425, 588, 813, 1123, 1552, 2144,
	2963, 4095},
	{64, 84, 111, 147, 194, 256, 338, 446, 588, 776, 1024, 1351, 1782,
	2352, 3103, 4095},
	{1, 2, 3, 5, 9, 16, 28, 48, 84, 147, 256, 446, 776, 1351, 2352, 4095},
	{4, 6, 10, 16, 25, 40, 64, 102, 161, 256, 406, 645, 1024, 1625,
	2580, 4095},
	{4, 6, 10, 16, 25, 40, 64, 102, 161, 256, 406, 645, 1024, 1625,
	2580, 4095},
	{1, 2, 3, 5, 9, 16, 28, 48, 84, 147, 256, 446, 776, 1351, 2352, 4095},
	{1, 2, 3, 5, 9, 16, 28, 48, 84, 147, 256, 446, 776, 1351, 2352, 4095},
	{1, 2, 3, 5, 9, 16, 28, 48, 84, 147, 256, 446, 776, 1351, 2352, 4095},
	{1, 2, 3, 5, 9, 16, 28, 48, 84, 147, 256, 446, 776, 1351, 2352, 4095},
};

static u16 sde_vdc_lbda_bf_lut[VDC_PROFILE_MAX][VDC_LBDA_BF_LUT_SIZE] = {
	{1, 1, 2, 3, 4, 6, 9, 13, 19, 28, 40, 58, 84, 122, 176, 255},
	{1, 1, 2, 3, 4, 6, 9, 13, 19, 28, 40, 58, 84, 122, 176, 255},
	{1, 1, 2, 3, 4, 6, 9, 13, 19, 28, 40, 58, 84, 122, 176, 255},
	{1, 1, 2, 3, 4, 6, 9, 13, 19, 28, 40, 58, 84, 122, 176, 255},
	{4, 5, 7, 9, 12, 16, 21, 28, 37, 48, 64, 84, 111, 146, 193, 255},
	{1, 1, 1, 2, 3, 4, 6, 9, 14, 21, 32, 48, 73, 111, 168, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
	{1, 1, 1, 1, 2, 3, 4, 6, 10, 16, 25, 40, 64, 101, 161, 255},
};

static int _get_vdc_profile_index(struct msm_display_vdc_info *vdc_info)
{
	int bpp, bpc;
	int rc = -EINVAL;

	bpp = VDC_BPP(vdc_info->bits_per_pixel);
	bpc = vdc_info->bits_per_component;

	if (vdc_info->chroma_format == MSM_CHROMA_444) {
		if ((bpc == 8) && (bpp == 8))
			return VDC_RGB_444_8BPC_8BPP;
		else if ((bpc == 8) && (bpp == 6))
			return VDC_RGB_444_8BPC_6BPP;
		else if ((bpc == 10) && (bpp == 10))
			return VDC_RGB_444_10BPC_10BPP;
		else if ((bpc == 10) && (bpp == 10))
			return VDC_RGB_444_10BPC_10BPP;
		else if ((bpc == 10) && (bpp == 8))
			return VDC_RGB_444_108BPC_8BPP;
		else if ((bpc == 10) && (bpp == 7))
			return VDC_RGB_444_10BPC_7BPP;
		else if ((bpc == 10) && (bpp == 6))
			return VDC_RGB_444_10BPC_6BPP;
	} else if (vdc_info->chroma_format == MSM_CHROMA_422) {
		if ((bpc == 8) && (bpp == 6))
			return VDC_YUV_422_8BPC_6BPP;
		else if ((bpc == 8) && (bpp == 5))
			return VDC_YUV_422_8BPC_5BPP;
		else if ((bpc == 10) && (bpp == 8))
			return VDC_YUV_422_10BPC_8BPP;
		else if ((bpc == 10) && (bpp == 6))
			return VDC_YUV_422_10BPC_6BPP;
		else if ((bpc == 10) && (bpp == 5))
			return VDC_YUV_422_10BPC_5PP;
	}

	pr_err("unsupported bpc:%d, bpp:%d\n", bpc, bpp);

	return rc;
}

static void sde_vdc_dump_lut_params(struct msm_display_vdc_info *vdc_info)
{
	int i;

	pr_debug("vdc_info->mppf_bpc_r_y = %d\n", vdc_info->mppf_bpc_r_y);
	pr_debug("vdc_info->mppf_bpc_g_cb = %d\n", vdc_info->mppf_bpc_g_cb);
	pr_debug("vdc_info->mppf_bpc_b_cr = %d\n", vdc_info->mppf_bpc_b_cr);
	pr_debug("vdc_info->mppf_bpc_y = %d\n", vdc_info->mppf_bpc_y);
	pr_debug("vdc_info->mppf_bpc_co = %d\n", vdc_info->mppf_bpc_co);
	pr_debug("vdc_info->mppf_bpc_cg = %d\n", vdc_info->mppf_bpc_cg);
	pr_debug("vdc_info->flatqp_vf_fbls = %d\n", vdc_info->flatqp_vf_fbls);
	pr_debug("vdc_info->flatqp_vf_nbls = %d\n", vdc_info->flatqp_vf_nbls);
	pr_debug("vdc_info->flatqp_sw_fbls = %d\n", vdc_info->flatqp_sw_fbls);
	pr_debug("vdc_info->flatqp_sw_nbls = %d\n", vdc_info->flatqp_sw_nbls);

	for (i = 0; i < VDC_FLAT_QP_LUT_SIZE; i++)
		pr_debug("vdc_info->flatness_qp_lut[%d] = %d\n",
				 i, vdc_info->flatness_qp_lut[i]);

	for (i = 0; i < VDC_MAX_QP_LUT_SIZE; i++)
		pr_debug("vdc_info->max_qp_lut[%d] = %d\n",
				 i, vdc_info->max_qp_lut[i]);

	for (i = 0; i < VDC_TAR_DEL_LUT_SIZE; i++)
		pr_debug("vdc_info->tar_del_lut[%d] = %d\n",
				 i, vdc_info->tar_del_lut[i]);

	for (i = 0; i < VDC_LBDA_BRATE_LUT_SIZE; i++)
		pr_debug("vdc_info->lbda_brate_lut[%d] = %d\n",
				 i, vdc_info->lbda_brate_lut[i]);

	for (i = 0; i < VDC_LBDA_BF_LUT_SIZE; i++)
		pr_debug("vdc_info->lbda_bf_lut[%d] = %d\n",
				 i, vdc_info->lbda_bf_lut[i]);

	for (i = 0; i < VDC_LBDA_BRATE_REG_SIZE; i++)
		pr_debug("vdc_info->lbda_brate_lut_interp[%d] = %d\n",
				 i, vdc_info->lbda_brate_lut_interp[i]);

	for (i = 0; i < VDC_LBDA_BRATE_REG_SIZE; i++)
		pr_debug("vdc_info->lbda_bf_lut_interp[%d] = %d\n",
				 i, vdc_info->lbda_bf_lut_interp[i]);
}

static void sde_vdc_dump_core_params(struct msm_display_vdc_info *vdc_info)
{
	pr_debug("vdc_info->num_of_active_ss = %d\n",
			 vdc_info->num_of_active_ss);
	pr_debug("vdc_info->chunk_size = %d\n",
			 vdc_info->chunk_size);
	pr_debug("vdc_info->chunk_size_bits = %d\n",
			 vdc_info->chunk_size_bits);
	pr_debug("vdc_info->slice_num_px = %d\n",
			 vdc_info->slice_num_px);
	pr_debug("vdc_info->avg_block_bits = %d\n",
			 vdc_info->avg_block_bits);
	pr_debug("vdc_info->per_chunk_pad_bits = %d\n",
			 vdc_info->per_chunk_pad_bits);
	pr_debug("vdc_info->tot_pad_bits = %d\n",
			 vdc_info->tot_pad_bits);
	pr_debug("vdc_info->rc_stuffing_bits = %d\n",
			 vdc_info->rc_stuffing_bits);
	pr_debug("vdc_info->slice_num_bits = %llu\n",
			vdc_info->slice_num_bits);
	pr_debug("vdc_info->chunk_adj_bits = %d\n",
			 vdc_info->chunk_adj_bits);
	pr_debug("vdc_info->rc_buf_init_size_temp = %d\n",
			 vdc_info->rc_buf_init_size_temp);
	pr_debug("vdc_info->init_tx_delay_temp = %d\n",
			 vdc_info->init_tx_delay_temp);
	pr_debug("vdc_info->rc_buffer_init_size = %d\n",
			 vdc_info->rc_buffer_init_size);
	pr_debug("vdc_info->rc_init_tx_delay = %d\n",
			 vdc_info->rc_init_tx_delay);
	pr_debug("vdc_info->rc_init_tx_delay_px_times = %d\n",
			 vdc_info->rc_init_tx_delay_px_times);
	pr_debug("vdc_info->rc_buffer_max_size = %d\n",
			 vdc_info->rc_buffer_max_size);
	pr_debug("vdc_info->rc_tar_rate_scale_temp_a = %d\n",
			 vdc_info->rc_tar_rate_scale_temp_a);
	pr_debug("vdc_info->rc_tar_rate_scale_temp_b = %d\n",
			 vdc_info->rc_tar_rate_scale_temp_b);
	pr_debug("vdc_info->rc_tar_rate_scale = %d\n",
			 vdc_info->rc_tar_rate_scale);
	pr_debug("vdc_info->rc_target_rate_threshold = %d\n",
			 vdc_info->rc_target_rate_threshold);
	pr_debug("vdc_info->chroma_samples = %d\n",
			 vdc_info->chroma_samples);
	pr_debug("vdc_info->block_max_bits = %d\n",
			 vdc_info->block_max_bits);
	pr_debug("vdc_info->rc_lambda_bitrate_scale = %d\n",
			 vdc_info->rc_lambda_bitrate_scale);
	pr_debug("vdc_info->rc_buffer_fullness_scale = %d\n",
			 vdc_info->rc_buffer_fullness_scale);
	pr_debug("vdc_info->rc_fullness_offset_thresh = %d\n",
			 vdc_info->rc_fullness_offset_thresh);
	pr_debug("vdc_info->ramp_blocks = %d\n",
			 vdc_info->ramp_blocks);
	pr_debug("vdc_info->ramp_bits = %llu\n",
			 vdc_info->ramp_bits);
	pr_debug("vdc_info->rc_fullness_offset_slope = %d\n",
			 vdc_info->rc_fullness_offset_slope);
	pr_debug("vdc_info->num_extra_mux_bits_init = %d\n",
			 vdc_info->num_extra_mux_bits_init);
	pr_debug("vdc_info->extra_crop_bits = %d\n",
			 vdc_info->extra_crop_bits);
	pr_debug("vdc_info->num_extra_mux_bits = %d\n",
			 vdc_info->num_extra_mux_bits);
	pr_debug("vdc_info->mppf_bits_comp_0 = %d\n",
			 vdc_info->mppf_bits_comp_0);
	pr_debug("vdc_info->mppf_bits_comp_1 = %d\n",
			 vdc_info->mppf_bits_comp_1);
	pr_debug("vdc_info->mppf_bits_comp_2 = %d\n",
			 vdc_info->mppf_bits_comp_2);
	pr_debug("vdc_info->min_block_bits = %d\n",
			vdc_info->min_block_bits);
}

static void sde_vdc_dump_ext_core_params(struct msm_display_vdc_info *vdc_info)
{
	pr_debug("vdc_info->input_ssm_out_latency = %d\n",
			 vdc_info->input_ssm_out_latency);
	pr_debug("vdc_info->input_ssm_out_latency_min = %d\n",
			 vdc_info->input_ssm_out_latency_min);
	pr_debug("vdc_info->obuf_latency = %d\n",
			 vdc_info->obuf_latency);
	pr_debug("vdc_info->base_hs_latency = %d\n",
			 vdc_info->base_hs_latency);
	pr_debug("vdc_info->base_hs_latency_pixels = %d\n",
			 vdc_info->base_hs_latency_pixels);
	pr_debug("vdc_info->base_hs_latency_pixels_min = %d\n",
			 vdc_info->base_hs_latency_pixels_min);
	pr_debug("vdc_info->base_initial_lines = %d\n",
			 vdc_info->base_initial_lines);
	pr_debug("vdc_info->base_top_up = %d\n",
			 vdc_info->base_top_up);
	pr_debug("vdc_info->output_rate = %d\n",
			 vdc_info->output_rate);
	pr_debug("vdc_info->output_rate_ratio_100 = %d\n",
			 vdc_info->output_rate_ratio_100);
	pr_debug("vdc_info->burst_accum_pixels = %d\n",
			 vdc_info->burst_accum_pixels);
	pr_debug("vdc_info->ss_initial_lines = %d\n",
			 vdc_info->ss_initial_lines);
	pr_debug("vdc_info->burst_initial_lines = %d\n",
			 vdc_info->burst_initial_lines);
	pr_debug("vdc_info->initial_lines = %d\n",
			 vdc_info->initial_lines);
	pr_debug("vdc_info->obuf_base = %d\n",
			 vdc_info->obuf_base);
	pr_debug("vdc_info->obuf_extra_ss0 = %d\n",
			 vdc_info->obuf_extra_ss0);
	pr_debug("vdc_info->obuf_extra_ss1 = %d\n",
			 vdc_info->obuf_extra_ss1);
	pr_debug("vdc_info->obuf_extra_burst = %d\n",
			 vdc_info->obuf_extra_burst);
	pr_debug("vdc_info->obuf_ss0 = %d\n",
			 vdc_info->obuf_ss0);
	pr_debug("vdc_info->obuf_ss1 = %d\n",
			 vdc_info->obuf_ss1);
	pr_debug("vdc_info->obuf_margin_words = %d\n",
			 vdc_info->obuf_margin_words);

	pr_debug("vdc_info->ob0_max_addr = %d\n",
			 vdc_info->ob0_max_addr);
	pr_debug("vdc_info->ob1_max_addr = %d\n",
			 vdc_info->ob1_max_addr);

	pr_debug("vdc_info->slice_width_orig = %d\n",
			 vdc_info->slice_width_orig);
	pr_debug("vdc_info->r2b0_max_addr = %d\n",
			 vdc_info->r2b0_max_addr);
	pr_debug("vdc_info->r2b1_max_addr = %d\n",
			 vdc_info->r2b1_max_addr);
}

static int sde_vdc_populate_lut_params(struct msm_display_vdc_info *vdc_info)
{
	int bpp, bpc;
	int i, profile_idx;
	int x_0, x_1, lambda, idx_mod;
	int x_0_idx, x_1_idx;
	int idx;

	bpp = VDC_BPP(vdc_info->bits_per_pixel);
	bpc = vdc_info->bits_per_component;

	profile_idx = _get_vdc_profile_index(vdc_info);
	if (profile_idx == -EINVAL) {
		pr_err("no matching profile found\n");
		return profile_idx;
	}

	vdc_info->mppf_bpc_r_y = sde_vdc_mppf_bpc_r_y[profile_idx];
	vdc_info->mppf_bpc_g_cb = sde_vdc_mppf_bpc_g_cb[profile_idx];
	vdc_info->mppf_bpc_b_cr = sde_vdc_mppf_bpc_b_cr[profile_idx];
	vdc_info->mppf_bpc_y = sde_vdc_mppf_bpc_y[profile_idx];
	vdc_info->mppf_bpc_co = sde_vdc_mppf_bpc_co[profile_idx];
	vdc_info->mppf_bpc_cg = sde_vdc_mppf_bpc_cg[profile_idx];
	vdc_info->flatqp_vf_fbls = sde_vdc_flat_qp_vf_fbls[profile_idx];
	vdc_info->flatqp_vf_nbls = sde_vdc_flat_qp_vf_nfbls[profile_idx];
	vdc_info->flatqp_sw_fbls = sde_vdc_flat_qp_sf_fbls[profile_idx];
	vdc_info->flatqp_sw_nbls = sde_vdc_flat_qp_sf_nbls[profile_idx];

	idx = profile_idx;

	for (i = 0; i < VDC_FLAT_QP_LUT_SIZE; i++)
		vdc_info->flatness_qp_lut[i] = sde_vdc_flat_qp_lut[idx][i];

	for (i = 0; i < VDC_MAX_QP_LUT_SIZE; i++)
		vdc_info->max_qp_lut[i] = sde_vdc_max_qp_lut[idx][i];

	for (i = 0; i < VDC_TAR_DEL_LUT_SIZE; i++)
		vdc_info->tar_del_lut[i] = sde_vdc_tar_del_lut[idx][i];

	for (i = 0; i < VDC_LBDA_BRATE_LUT_SIZE; i++)
		vdc_info->lbda_brate_lut[i] = sde_vdc_lbda_brate_lut[idx][i];

	for (i = 0; i < VDC_LBDA_BF_LUT_SIZE; i++)
		vdc_info->lbda_bf_lut[i] = sde_vdc_lbda_bf_lut[idx][i];

	for (i = 0; i < VDC_LBDA_BRATE_REG_SIZE; i++) {
		idx_mod = i & 0x03;

		x_0_idx = i >> 2;
		if (x_0_idx > VDC_LBDA_BRATE_LUT_SIZE - 1)
			x_0_idx = VDC_LBDA_BRATE_LUT_SIZE - 1;

		x_1_idx = (i >> 2) + 1;
		if (x_1_idx > VDC_LBDA_BRATE_LUT_SIZE - 1)
			x_1_idx = VDC_LBDA_BRATE_LUT_SIZE - 1;

		x_0 = vdc_info->lbda_brate_lut[x_0_idx];
		x_1 = vdc_info->lbda_brate_lut[x_1_idx];

		lambda = (((4 - idx_mod) * x_0 + idx_mod * x_1 + 2) >> 2);
		vdc_info->lbda_brate_lut_interp[i] = lambda;

		x_0 = vdc_info->lbda_bf_lut[x_0_idx];
		x_1 = vdc_info->lbda_bf_lut[x_1_idx];

		lambda = (((4 - idx_mod) * x_0 + idx_mod * x_1 + 2) >> 2);
		vdc_info->lbda_bf_lut_interp[i] = lambda;
	}

	sde_vdc_dump_lut_params(vdc_info);
	return 0;
}

static int sde_vdc_populate_core_params(struct msm_display_vdc_info *vdc_info,
	int intf_width)
{
	u16 bpp;
	u16 bpc;
	u32 bpp_codec;
	u64 temp, diff;

	if (!vdc_info)
		return -EINVAL;

	if (!vdc_info->slice_width ||
			!vdc_info->slice_height ||
			intf_width < vdc_info->slice_width) {
		pr_err("invalid input, intf_width=%d slice_width=%d\n",
			intf_width, vdc_info->slice_width);
		return -EINVAL;
	}

	bpp = VDC_BPP(vdc_info->bits_per_pixel);
	bpp_codec = 16 * bpp;
	bpc = vdc_info->bits_per_component;

	vdc_info->num_of_active_ss = intf_width / vdc_info->slice_width;

	temp = vdc_info->slice_width * bpp_codec;
	temp += 15;
	temp >>= 4;
	temp += 7;
	temp >>= 3;
	vdc_info->chunk_size = temp;
	vdc_info->chunk_size_bits = temp * 8;
	vdc_info->slice_num_px = vdc_info->slice_width *
		vdc_info->slice_height;

	/* slice_num_px should be atleast 4096 */
	if (vdc_info->slice_num_px < 4096) {
		pr_err("insufficient slice_num_px:%d\n",
			vdc_info->slice_num_px);
		return -EINVAL;
	}

	vdc_info->avg_block_bits = bpp_codec;

	temp = (16 * vdc_info->chunk_size);
	temp -= (vdc_info->slice_width * 2 * bpp_codec) >> 4;
	vdc_info->per_chunk_pad_bits = temp;

	vdc_info->tot_pad_bits = (vdc_info->avg_block_bits +
		vdc_info->per_chunk_pad_bits - 8);

	vdc_info->rc_stuffing_bits = ((vdc_info->tot_pad_bits + 8) / 9);
	vdc_info->slice_num_bits = (8 * vdc_info->chunk_size *
		vdc_info->slice_height);

	temp = (16 * vdc_info->chunk_size);
	vdc_info->chunk_adj_bits = temp -
		((2 * vdc_info->slice_width * bpp_codec) >> 4);

	if (vdc_info->slice_width <= 720)
		vdc_info->rc_buf_init_size_temp = 4096;
	else if (vdc_info->slice_width <= 2048)
		vdc_info->rc_buf_init_size_temp = 8192;
	else
		vdc_info->rc_buf_init_size_temp = 10752;

	vdc_info->init_tx_delay_temp = (vdc_info->rc_buf_init_size_temp /
		vdc_info->avg_block_bits);

	temp = (vdc_info->init_tx_delay_temp * 16 * bpp_codec);
	vdc_info->rc_buffer_init_size = temp >> 4;

	vdc_info->rc_init_tx_delay = vdc_info->rc_buffer_init_size /
		vdc_info->avg_block_bits;

	vdc_info->rc_init_tx_delay_px_times = vdc_info->rc_init_tx_delay * 16;

	temp = (2 * vdc_info->rc_buffer_init_size);
	temp = temp + (2 * vdc_info->slice_width *
		RC_TARGET_RATE_EXTRA_FTBLS);
	vdc_info->rc_buffer_max_size = temp;

	vdc_info->rc_tar_rate_scale_temp_a = ilog2(vdc_info->slice_num_px) + 1;
	vdc_info->rc_tar_rate_scale_temp_b = ilog2(vdc_info->slice_num_px);

	vdc_info->rc_tar_rate_scale = 1 + vdc_info->rc_tar_rate_scale_temp_a;

	vdc_info->rc_target_rate_threshold = (1 <<
		(vdc_info->rc_tar_rate_scale - 1));

	if (vdc_info->chroma_format == MSM_CHROMA_444)
		vdc_info->chroma_samples = 16;
	else if (vdc_info->chroma_format == MSM_CHROMA_422)
		vdc_info->chroma_samples = 8;
	else
		vdc_info->chroma_samples = 4;

	temp = (2 * vdc_info->chroma_samples) + 16;
	vdc_info->block_max_bits = (temp * bpc) + 7;

	temp = (1 << 12);
	temp += (vdc_info->block_max_bits >> 1);
	temp /= vdc_info->block_max_bits;
	vdc_info->rc_lambda_bitrate_scale = temp;

	temp = (1 << 20);
	temp /= vdc_info->rc_buffer_max_size;
	vdc_info->rc_buffer_fullness_scale = temp;

	vdc_info->rc_fullness_offset_thresh = (vdc_info->slice_height / 6);

	temp = (vdc_info->slice_width >> 3);
	temp = temp * vdc_info->rc_fullness_offset_thresh;
	vdc_info->ramp_blocks = temp;

	temp = (vdc_info->rc_buffer_max_size - vdc_info->rc_buffer_init_size);
	temp = temp << 16;
	vdc_info->ramp_bits = temp;

	temp = div_u64(vdc_info->ramp_bits, (vdc_info->ramp_blocks) ? vdc_info->ramp_blocks : 1);
	vdc_info->rc_fullness_offset_slope = temp;

	temp = (2 * SSM_MAX_SE_SIZE) - 2;
	vdc_info->num_extra_mux_bits_init = temp * 4;

	temp = vdc_info->slice_num_bits - vdc_info->num_extra_mux_bits_init;
	if ((temp % SSM_MAX_SE_SIZE) == 0) {
		vdc_info->extra_crop_bits = 0;
	} else {
		diff = vdc_info->slice_num_bits -
			vdc_info->num_extra_mux_bits_init;
		vdc_info->extra_crop_bits = (SSM_MAX_SE_SIZE -
			(diff % SSM_MAX_SE_SIZE));
	}

	vdc_info->num_extra_mux_bits = vdc_info->num_extra_mux_bits_init -
		vdc_info->extra_crop_bits;

	vdc_info->mppf_bits_comp_0 = 16 * vdc_info->mppf_bpc_r_y;
	vdc_info->mppf_bits_comp_1 = vdc_info->chroma_samples *
		vdc_info->mppf_bpc_g_cb;
	vdc_info->mppf_bits_comp_2 = vdc_info->chroma_samples *
		vdc_info->mppf_bpc_b_cr;

	vdc_info->min_block_bits = 8 + vdc_info->mppf_bits_comp_0 +
		vdc_info->mppf_bits_comp_1 + vdc_info->mppf_bits_comp_2;

	sde_vdc_dump_core_params(vdc_info);
	return 0;
}

void sde_vdc_intf_prog_params(struct msm_display_vdc_info *vdc_info,
	int intf_width)
{
	int slice_per_pkt, slice_per_intf;
	int bytes_in_slice, total_bytes_per_intf;
	u16 bpp;

	slice_per_pkt = vdc_info->slice_per_pkt;
	// is mode->timing.h_active always intf_width?
	slice_per_intf = DIV_ROUND_UP(intf_width,
			vdc_info->slice_width);

	/*
	 * If slice_per_pkt is greater than slice_per_intf then default to 1.
	 * This can happen during partial update.
	 */
	if (slice_per_pkt > slice_per_intf)
		slice_per_pkt = 1;

	bpp = VDC_BPP(vdc_info->bits_per_pixel);
	bytes_in_slice = DIV_ROUND_UP(vdc_info->slice_width *
			bpp, 8);
	total_bytes_per_intf = bytes_in_slice * slice_per_intf;

	vdc_info->eol_byte_num = total_bytes_per_intf % 3;
	vdc_info->pclk_per_line =  DIV_ROUND_UP(total_bytes_per_intf,
			3);
	vdc_info->bytes_in_slice = bytes_in_slice;
	vdc_info->bytes_per_pkt = bytes_in_slice * slice_per_pkt;
	vdc_info->pkt_per_line = slice_per_intf / slice_per_pkt;

	pr_debug("eol_byte_num = %d pclk_per_line = %d\n",
			 vdc_info->eol_byte_num, vdc_info->pclk_per_line);
	pr_debug("bytes_in_slice = %d bytes_per_pkt = %d\n",
			 vdc_info->bytes_in_slice, vdc_info->bytes_per_pkt);
	pr_debug("pkt_per_line = %d\n", vdc_info->pkt_per_line);
}

static void sde_vdc_ext_core_params(struct msm_display_vdc_info *vdc_info,
	int traffic_mode)
{
	int temp;
	int bpp;
	int rc_init_tx_delay_px_times;

	bpp = VDC_BPP(vdc_info->bits_per_pixel);

	vdc_info->min_ssm_delay = SSM_MAX_SE_SIZE;
	vdc_info->max_ssm_delay = SSM_MAX_SE_SIZE + 1;

	vdc_info->input_ssm_out_latency = MAX_PIPELINE_LATENCY +
		(8 * vdc_info->max_ssm_delay);

	vdc_info->input_ssm_out_latency_min = (8 * vdc_info->min_ssm_delay);

	temp = (7 + OUT_BUF_UF_MARGIN);
	temp *= OB_DATA_WIDTH;
	temp += (SSM_MAX_SE_SIZE / 2);

	temp /= (bpp * 2);
	temp += 1;

	vdc_info->obuf_latency = temp;

	temp = vdc_info->input_ssm_out_latency +
		vdc_info->obuf_latency;
	rc_init_tx_delay_px_times = vdc_info->rc_init_tx_delay_px_times;
	rc_init_tx_delay_px_times /= 2;

	vdc_info->base_hs_latency = rc_init_tx_delay_px_times + temp;

	temp = vdc_info->rc_init_tx_delay_px_times;
	temp /= 2;
	temp += vdc_info->input_ssm_out_latency_min;
	vdc_info->base_hs_latency_min = temp;
	vdc_info->base_hs_latency_pixels = (vdc_info->base_hs_latency * 2);

	vdc_info->base_hs_latency_pixels_min = (2 *
			vdc_info->base_hs_latency_min);

	temp = DIV_ROUND_UP(vdc_info->base_hs_latency_pixels,
		vdc_info->slice_width);

	vdc_info->base_initial_lines = temp;

	temp = vdc_info->base_initial_lines * vdc_info->slice_width;
	temp -= vdc_info->base_hs_latency_pixels;
	vdc_info->base_top_up =  temp;

	if (traffic_mode == VDC_TRAFFIC_BURST_MODE)
		vdc_info->output_rate = OUTPUT_DATA_WIDTH;
	else
		vdc_info->output_rate = bpp * 2;

	temp = (bpp * 2 * 100);
	temp /= vdc_info->output_rate;

	vdc_info->output_rate_ratio_100 = temp;

	if (traffic_mode == VDC_TRAFFIC_BURST_MODE) {
		temp = vdc_info->output_rate_ratio_100 *
			vdc_info->slice_width;
		temp *= vdc_info->num_of_active_ss;
		temp /= 100;
		vdc_info->burst_accum_pixels = temp;
	} else {
		vdc_info->burst_accum_pixels = 0;
	}

	if (vdc_info->num_of_active_ss > 1)
		vdc_info->ss_initial_lines = 1;
	else
		vdc_info->ss_initial_lines = 0;

	if (traffic_mode == VDC_TRAFFIC_BURST_MODE) {
		if ((vdc_info->burst_accum_pixels +
			 vdc_info->base_top_up) < vdc_info->slice_width)
			vdc_info->burst_initial_lines = 1;
		else
			vdc_info->burst_initial_lines = 0;
	} else {
		vdc_info->burst_initial_lines = 0;
	}

	vdc_info->initial_lines = 1 + vdc_info->base_initial_lines +
		vdc_info->ss_initial_lines +
		vdc_info->burst_initial_lines;

	temp = (vdc_info->base_initial_lines * vdc_info->slice_width);
	temp -= vdc_info->base_hs_latency_pixels_min;
	temp *= bpp;
	vdc_info->obuf_base = temp;

	if (vdc_info->num_of_active_ss > 1) {
		vdc_info->obuf_extra_ss0 = 2 * vdc_info->chunk_size_bits;
		vdc_info->obuf_extra_ss1 = vdc_info->chunk_size_bits;
	} else {
		vdc_info->obuf_extra_ss0 = 0;
		vdc_info->obuf_extra_ss1 = 0;
	}

	vdc_info->obuf_extra_burst = vdc_info->burst_initial_lines *
		vdc_info->chunk_size_bits;

	vdc_info->obuf_ss0 = vdc_info->rc_buffer_max_size +
		vdc_info->obuf_base + vdc_info->obuf_extra_ss0 +
			vdc_info->obuf_extra_burst;

	vdc_info->obuf_ss1 = vdc_info->rc_buffer_max_size +
		vdc_info->obuf_base + vdc_info->obuf_extra_ss1 +
			vdc_info->obuf_extra_burst;

	temp = OUT_BUF_OF_MARGIN_TC_10 *
		vdc_info->chunk_size_bits;
	temp /= 10;
	temp /= SSM_MAX_SE_SIZE;
	vdc_info->obuf_margin_words = max(OUT_BUF_OF_MARGIN_OB,
		temp);

	if (vdc_info->num_of_active_ss == 2) {
		vdc_info->ob0_max_addr = OB0_RAM_DEPTH - 1;
		vdc_info->ob1_max_addr = OB1_RAM_DEPTH - 1;
	} else {
		vdc_info->ob0_max_addr = (2 * OB1_RAM_DEPTH) - 1;
		vdc_info->ob1_max_addr = 0;
	}

	if (vdc_info->split_panel_enable) {
		temp = vdc_info->frame_width / vdc_info->num_of_active_ss;
		temp /= NUM_ACTIVE_HS;
	} else {
		temp = vdc_info->frame_width / vdc_info->num_of_active_ss;
	}

	vdc_info->slice_width_orig = temp;

	vdc_info->r2b0_max_addr = (MAX_PIXELS_PER_HS_LINE / 4);
	vdc_info->r2b0_max_addr += 3;

	vdc_info->r2b1_max_addr = (MAX_PIXELS_PER_HS_LINE / 4);
	vdc_info->r2b1_max_addr /= 2;
	vdc_info->r2b1_max_addr += 3;

	sde_vdc_dump_ext_core_params(vdc_info);
}

int sde_vdc_populate_config(struct msm_display_vdc_info *vdc_info,
	int intf_width, int traffic_mode)
{
	int ret = 0;

	ret = sde_vdc_populate_core_params(vdc_info, intf_width);
	if (ret) {
		pr_err("failed to populate vdc core params %d\n", ret);
		return ret;
	}

	ret = sde_vdc_populate_lut_params(vdc_info);
	if (ret) {
		pr_err("failed to populate lut params %d\n", ret);
		return ret;
	}

	sde_vdc_intf_prog_params(vdc_info, intf_width);

	sde_vdc_ext_core_params(vdc_info, traffic_mode);

	return ret;
}

int sde_vdc_create_pps_buf_cmd(struct msm_display_vdc_info *vdc_info,
	char *buf, int pps_id, u32 len)
{
	char *bp = buf;
	u32 i;
	u32 slice_num_bits_ub, slice_num_bits_ldw;

	if (len < SDE_VDC_PPS_SIZE)
		return -EINVAL;

	memset(buf, 0, len);
	/* b0 */
	*bp++ = vdc_info->version_major;
	/* b1 */
	*bp++ = vdc_info->version_minor;
	/* b2 */
	*bp++ = vdc_info->version_release;
	/* b3 */
	*bp++ = (pps_id & 0xff);		/* pps1 */
	/* b4-b5 */
	*bp++ = ((vdc_info->frame_width >> 8) & 0xff);
	*bp++ = (vdc_info->frame_width & 0x0ff);
	/* b6-b7 */
	*bp++ = ((vdc_info->frame_height >> 8) & 0xff);
	*bp++ = (vdc_info->frame_height & 0x0ff);
	/* b8-b9 */
	*bp++ = ((vdc_info->slice_width >> 8) & 0xff);
	*bp++ = (vdc_info->slice_width & 0x0ff);
	/* b10-b11 */
	*bp++ = ((vdc_info->slice_height >> 8) & 0xff);
	*bp++ = (vdc_info->slice_height & 0x0ff);
	/* b12-b15 */
	*bp++ = ((vdc_info->slice_num_px >> 24) & 0xff);
	*bp++ = ((vdc_info->slice_num_px >> 16) & 0xff);
	*bp++ = ((vdc_info->slice_num_px >> 8) & 0xff);
	*bp++ = (vdc_info->slice_num_px & 0x0ff);
	/* b16-b17 */
	*bp++ = ((vdc_info->bits_per_pixel >> 8) & 0x3);
	*bp++ = (vdc_info->bits_per_pixel & 0xff);
	/* b18 */
	bp++; /* reserved */
	/* b19 */
	*bp++ = ((((vdc_info->bits_per_component - 8) >> 1) & 0x3) << 4)|
		((vdc_info->source_color_space & 0x3) << 2)|
		(vdc_info->chroma_format & 0x3);
	/* b20-b21 */
	bp++; /* reserved */
	bp++; /* reserved */

	/* b22-b23 */
	*bp++ = ((vdc_info->chunk_size >> 8) & 0xff);
	*bp++ = (vdc_info->chunk_size & 0x0ff);

	/* b24-b25 */
	bp++; /* reserved */
	bp++; /* reserved */

	/* b26-b27 */
	*bp++ = ((vdc_info->rc_buffer_init_size >> 8) & 0xff);
	*bp++ = (vdc_info->rc_buffer_init_size & 0x0ff);

	/* b28 */
	*bp++ = vdc_info->rc_stuffing_bits;

	/* b29 */
	*bp++ = vdc_info->rc_init_tx_delay;

	/* b30-b31 */
	*bp++ = ((vdc_info->rc_buffer_max_size >> 8) & 0xff);
	*bp++ = (vdc_info->rc_buffer_max_size & 0x0ff);

	/* b32-b35 */
	*bp++ = ((vdc_info->rc_target_rate_threshold >> 24) & 0xff);
	*bp++ = ((vdc_info->rc_target_rate_threshold >> 16) & 0xff);
	*bp++ = ((vdc_info->rc_target_rate_threshold >> 8) & 0xff);
	*bp++ = (vdc_info->rc_target_rate_threshold & 0x0ff);

	/* b36 */
	*bp++ = vdc_info->rc_tar_rate_scale;
	/* b37 */
	*bp++ = vdc_info->rc_buffer_fullness_scale;

	/* b38-b39 */
	*bp++ = ((vdc_info->rc_fullness_offset_thresh >> 8) & 0xff);
	*bp++ = (vdc_info->rc_fullness_offset_thresh & 0x0ff);

	/* b40-b42 */
	*bp++ = ((vdc_info->rc_fullness_offset_slope >> 16) & 0xff);
	*bp++ = ((vdc_info->rc_fullness_offset_slope >> 8) & 0xff);
	*bp++ = ((vdc_info->rc_fullness_offset_slope) & 0xff);

	/* b43 */
	*bp++ = (RC_TARGET_RATE_EXTRA_FTBLS & 0x0f);
	/* b44 */
	*bp++ = vdc_info->flatqp_vf_fbls;
	/* b45 */
	*bp++ = vdc_info->flatqp_vf_nbls;
	/* b46 */
	*bp++ = vdc_info->flatqp_sw_fbls;
	/* b47 */
	*bp++ = vdc_info->flatqp_sw_nbls;

	/* b48-b55 */
	for (i = 0; i < VDC_FLAT_QP_LUT_SIZE; i++)
		*bp++ = vdc_info->flatness_qp_lut[i];

	/* b56-b63 */
	for (i = 0; i < VDC_MAX_QP_LUT_SIZE; i++)
		*bp++ = vdc_info->max_qp_lut[i];

	/* b64-b79 */
	for (i = 0; i < VDC_TAR_DEL_LUT_SIZE; i++)
		*bp++ = vdc_info->tar_del_lut[i];

	/* b80 */
	bp++; /* reserved */

	/* b81 */
	*bp++ = (((vdc_info->mppf_bpc_r_y & 0xf) << 4) |
		(vdc_info->mppf_bpc_g_cb & 0xf));
	/* b82 */
	*bp++ = (((vdc_info->mppf_bpc_b_cr & 0xf) << 4) |
		(vdc_info->mppf_bpc_y & 0xf));
	/* b83 */
	*bp++ = (((vdc_info->mppf_bpc_co & 0xf) << 4) |
		(vdc_info->mppf_bpc_cg & 0xf));

	/* b84 */
	bp++; /* reserved */
	/* b85 */
	bp++; /* reserved */
	/* b86 */
	bp++; /* reserved */

	/* b87 */
	*bp++ = SSM_MAX_SE_SIZE;

	/* b88 */
	bp++; /* reserved */
	/* b89 */
	bp++; /* reserved */
	/* b90 */
	bp++; /* reserved */

	/* b91 */
	slice_num_bits_ub = (vdc_info->slice_num_bits >> 32);
	*bp++ = (slice_num_bits_ub & 0x0ff);
	/* b92-b95 */
	slice_num_bits_ldw = (u32)vdc_info->slice_num_bits;
	*bp++ = ((slice_num_bits_ldw >> 24) & 0xff);
	*bp++ = ((slice_num_bits_ldw >> 16) & 0xff);
	*bp++ = ((slice_num_bits_ldw >> 8) & 0xff);
	*bp++ = (slice_num_bits_ldw & 0x0ff);

	/* b96 */
	bp++;
	/* b97 */
	*bp++ = vdc_info->chunk_adj_bits;
	/* b98-b99 */
	*bp++ = ((vdc_info->num_extra_mux_bits >> 8) & 0xff);
	*bp++ = (vdc_info->num_extra_mux_bits & 0x0ff);

	return 0;
}

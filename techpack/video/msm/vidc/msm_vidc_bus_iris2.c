// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_vidc_bus.h"
#include "msm_vidc_internal.h"

static unsigned long __calculate_vpe(struct vidc_bus_vote_data *d)
{
	return 0;
}

static unsigned long __calculate_cvp(struct vidc_bus_vote_data *d)
{
	return 0;
}

static unsigned long __calculate_decoder(struct vidc_bus_vote_data *d)
{
	/*
	 * XXX: Don't fool around with any of the hardcoded numbers unless you
	 * know /exactly/ what you're doing.  Many of these numbers are
	 * measured heuristics and hardcoded numbers taken from the firmware.
	 */
	/* Decoder parameters */
	int width, height, lcu_size, fps, dpb_bpp;
	bool unified_dpb_opb, dpb_compression_enabled = true,
		opb_compression_enabled = false,
		llc_ref_read_l2_cache_enabled = false,
		llc_top_line_buf_enabled = false;
	fp_t dpb_read_compression_factor, dpb_opb_scaling_ratio,
		dpb_write_compression_factor, opb_write_compression_factor,
		qsmmu_bw_overhead_factor;
	bool is_h264_category = true;

	/* Derived parameters */
	int lcu_per_frame, collocated_bytes_per_lcu, tnbr_per_lcu;
	unsigned long bitrate;

	fp_t bins_to_bit_factor, vsp_read_factor, vsp_write_factor,
		dpb_factor, dpb_write_factor, y_bw_no_ubwc_8bpp;
	fp_t y_bw_no_ubwc_10bpp = 0, y_bw_10bpp_p010 = 0,
	     motion_vector_complexity = 0;
	fp_t	dpb_total = 0;

	/* Output parameters */
	struct {
		fp_t vsp_read, vsp_write, collocated_read, collocated_write,
			dpb_read, dpb_write, opb_read, opb_write,
			line_buffer_read, line_buffer_write,
			total;
	} ddr = {0};

	struct {
		fp_t dpb_read, line_buffer_read, line_buffer_write, total;
	} llc = {0};

	unsigned long ret = 0;
	unsigned int integer_part, frac_part;

	width = max(d->input_width, BASELINE_DIMENSIONS.width);
	height = max(d->input_height, BASELINE_DIMENSIONS.height);

	fps = d->fps;

	lcu_size = d->lcu_size;

	dpb_bpp = __bpp(d->color_formats[0], d->sid);

	unified_dpb_opb = d->num_formats == 1;

	dpb_opb_scaling_ratio = fp_div(FP_INT(d->input_width * d->input_height),
		FP_INT(d->output_width * d->output_height));

	opb_compression_enabled = d->num_formats >= 2 &&
		__ubwc(d->color_formats[1]);

	integer_part = Q16_INT(d->compression_ratio);
	frac_part = Q16_FRAC(d->compression_ratio);
	dpb_read_compression_factor = FP(integer_part, frac_part, 100);

	integer_part = Q16_INT(d->complexity_factor);
	frac_part = Q16_FRAC(d->complexity_factor);
	motion_vector_complexity = FP(integer_part, frac_part, 100);

	dpb_write_compression_factor = dpb_read_compression_factor;
	opb_write_compression_factor = opb_compression_enabled ?
		dpb_write_compression_factor : FP_ONE;

	if (d->codec == HAL_VIDEO_CODEC_HEVC ||
		d->codec == HAL_VIDEO_CODEC_VP9) {
		/* H264, VP8, MPEG2 use the same settings */
		/* HEVC, VP9 use the same setting */
		is_h264_category = false;
	}
	if (d->use_sys_cache) {
		llc_ref_read_l2_cache_enabled = true;
		if (is_h264_category)
			llc_top_line_buf_enabled = true;
	}

	/* Derived parameters setup */
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);

	bitrate = DIV_ROUND_UP(d->bitrate, 1000000);

	bins_to_bit_factor = FP_INT(4);

	vsp_write_factor = bins_to_bit_factor;
	vsp_read_factor = bins_to_bit_factor + FP_INT(2);

	collocated_bytes_per_lcu = lcu_size == 16 ? 16 :
				lcu_size == 32 ? 64 : 256;

	dpb_factor = FP(1, 50, 100);
	dpb_write_factor = FP(1, 5, 100);

	tnbr_per_lcu = lcu_size == 16 ? 128 :
		lcu_size == 32 ? 64 : 128;

	/* .... For DDR & LLC  ...... */
	ddr.vsp_read = fp_div(fp_mult(FP_INT(bitrate),
				vsp_read_factor), FP_INT(8));
	ddr.vsp_write = fp_div(fp_mult(FP_INT(bitrate),
				vsp_write_factor), FP_INT(8));

	ddr.collocated_read = fp_div(FP_INT(lcu_per_frame *
			collocated_bytes_per_lcu * fps), FP_INT(bps(1)));
	ddr.collocated_write = ddr.collocated_read;

	y_bw_no_ubwc_8bpp = fp_div(FP_INT(width * height * fps),
		FP_INT(1000 * 1000));

	if (dpb_bpp != 8) {
		y_bw_no_ubwc_10bpp =
			fp_div(fp_mult(y_bw_no_ubwc_8bpp, FP_INT(256)),
				FP_INT(192));
		y_bw_10bpp_p010 = y_bw_no_ubwc_8bpp * 2;
	}

	ddr.dpb_read = dpb_bpp == 8 ? y_bw_no_ubwc_8bpp : y_bw_no_ubwc_10bpp;
	ddr.dpb_read = fp_div(fp_mult(ddr.dpb_read,
			fp_mult(dpb_factor, motion_vector_complexity)),
			dpb_read_compression_factor);

	ddr.dpb_write = dpb_bpp == 8 ? y_bw_no_ubwc_8bpp : y_bw_no_ubwc_10bpp;
	ddr.dpb_write = fp_div(fp_mult(ddr.dpb_write,
			fp_mult(dpb_factor, dpb_write_factor)),
			dpb_write_compression_factor);

	dpb_total = ddr.dpb_read + ddr.dpb_write;

	if (llc_ref_read_l2_cache_enabled) {
		ddr.dpb_read = fp_div(ddr.dpb_read, is_h264_category ?
					FP(1, 30, 100) : FP(1, 14, 100));
		llc.dpb_read = dpb_total - ddr.dpb_write - ddr.dpb_read;
	}

	ddr.opb_read = FP_ZERO;
	ddr.opb_write = unified_dpb_opb ? FP_ZERO : (dpb_bpp == 8 ?
		y_bw_no_ubwc_8bpp : (opb_compression_enabled ?
		y_bw_no_ubwc_10bpp : y_bw_10bpp_p010));
	ddr.opb_write = fp_div(fp_mult(dpb_factor, ddr.opb_write),
		fp_mult(dpb_opb_scaling_ratio, opb_write_compression_factor));

	ddr.line_buffer_read =
		fp_div(FP_INT(tnbr_per_lcu * lcu_per_frame * fps),
			FP_INT(bps(1)));
	ddr.line_buffer_write = ddr.line_buffer_read;
	if (llc_top_line_buf_enabled) {
		llc.line_buffer_read = ddr.line_buffer_read;
		llc.line_buffer_write = ddr.line_buffer_write;
		ddr.line_buffer_write = ddr.line_buffer_read = FP_ZERO;
	}

	ddr.total = ddr.vsp_read + ddr.vsp_write +
		ddr.collocated_read + ddr.collocated_write +
		ddr.dpb_read + ddr.dpb_write +
		ddr.opb_read + ddr.opb_write +
		ddr.line_buffer_read + ddr.line_buffer_write;

	qsmmu_bw_overhead_factor = FP(1, 3, 100);

	ddr.total = fp_mult(ddr.total, qsmmu_bw_overhead_factor);
	llc.total = llc.dpb_read + llc.line_buffer_read +
			llc.line_buffer_write + ddr.total;

	/* Dump all the variables for easier debugging */
	if (msm_vidc_debug & VIDC_BUS) {
		struct dump dump[] = {
		{"DECODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"lcu size", "%d", lcu_size},
		{"dpb bitdepth", "%d", dpb_bpp},
		{"frame rate", "%d", fps},
		{"dpb/opb unified", "%d", unified_dpb_opb},
		{"dpb/opb downscaling ratio", DUMP_FP_FMT,
			dpb_opb_scaling_ratio},
		{"dpb compression", "%d", dpb_compression_enabled},
		{"opb compression", "%d", opb_compression_enabled},
		{"dpb read compression factor", DUMP_FP_FMT,
			dpb_read_compression_factor},
		{"dpb write compression factor", DUMP_FP_FMT,
			dpb_write_compression_factor},
		{"frame width", "%d", width},
		{"frame height", "%d", height},
		{"llc ref read l2 cache enabled", "%d",
			llc_ref_read_l2_cache_enabled},
		{"llc top line buf enabled", "%d",
			llc_top_line_buf_enabled},

		{"DERIVED PARAMETERS (1)", "", DUMP_HEADER_MAGIC},
		{"lcus/frame", "%d", lcu_per_frame},
		{"bitrate (Mbit/sec)", "%d", bitrate},
		{"bins to bit factor", DUMP_FP_FMT, bins_to_bit_factor},
		{"dpb write factor", DUMP_FP_FMT, dpb_write_factor},
		{"vsp read factor", DUMP_FP_FMT, vsp_read_factor},
		{"vsp write factor", DUMP_FP_FMT, vsp_write_factor},
		{"tnbr/lcu", "%d", tnbr_per_lcu},
		{"collocated bytes/LCU", "%d", collocated_bytes_per_lcu},
		{"bw for NV12 8bpc)", DUMP_FP_FMT, y_bw_no_ubwc_8bpp},
		{"bw for NV12 10bpc)", DUMP_FP_FMT, y_bw_no_ubwc_10bpp},

		{"DERIVED PARAMETERS (2)", "", DUMP_HEADER_MAGIC},
		{"mv complexity", DUMP_FP_FMT, motion_vector_complexity},
		{"qsmmu_bw_overhead_factor", DUMP_FP_FMT,
			qsmmu_bw_overhead_factor},

		{"INTERMEDIATE DDR B/W", "", DUMP_HEADER_MAGIC},
		{"vsp read", DUMP_FP_FMT, ddr.vsp_read},
		{"vsp write", DUMP_FP_FMT, ddr.vsp_write},
		{"collocated read", DUMP_FP_FMT, ddr.collocated_read},
		{"collocated write", DUMP_FP_FMT, ddr.collocated_write},
		{"line buffer read", DUMP_FP_FMT, ddr.line_buffer_read},
		{"line buffer write", DUMP_FP_FMT, ddr.line_buffer_write},
		{"opb read", DUMP_FP_FMT, ddr.opb_read},
		{"opb write", DUMP_FP_FMT, ddr.opb_write},
		{"dpb read", DUMP_FP_FMT, ddr.dpb_read},
		{"dpb write", DUMP_FP_FMT, ddr.dpb_write},
		{"dpb total", DUMP_FP_FMT, dpb_total},
		{"INTERMEDIATE LLC B/W", "", DUMP_HEADER_MAGIC},
		{"llc dpb read", DUMP_FP_FMT, llc.dpb_read},
		{"llc line buffer read", DUMP_FP_FMT, llc.line_buffer_read},
		{"llc line buffer write", DUMP_FP_FMT, llc.line_buffer_write},

		};
		__dump(dump, ARRAY_SIZE(dump), d->sid);
	}

	d->calc_bw_ddr = kbps(fp_round(ddr.total));
	d->calc_bw_llcc = kbps(fp_round(llc.total));

	return ret;
}

static unsigned long __calculate_encoder(struct vidc_bus_vote_data *d)
{
	/*
	 * XXX: Don't fool around with any of the hardcoded numbers unless you
	 * know /exactly/ what you're doing.  Many of these numbers are
	 * measured heuristics and hardcoded numbers taken from the firmware.
	 */
	/* Encoder Parameters */
	int width, height, fps, lcu_size, bitrate, lcu_per_frame,
		collocated_bytes_per_lcu, tnbr_per_lcu, dpb_bpp,
		original_color_format, vertical_tile_width, rotation;
	bool work_mode_1, original_compression_enabled,
		low_power, cropping_or_scaling,
		b_frames_enabled = false,
		llc_ref_chroma_cache_enabled = false,
		llc_top_line_buf_enabled = false,
		llc_vpss_rot_line_buf_enabled = false;

	unsigned int bins_to_bit_factor;
	fp_t dpb_compression_factor,
		original_compression_factor,
		original_compression_factor_y,
		y_bw_no_ubwc_8bpp, y_bw_no_ubwc_10bpp = 0, y_bw_10bpp_p010 = 0,
		input_compression_factor,
		downscaling_ratio,
		ref_y_read_bw_factor, ref_cbcr_read_bw_factor,
		recon_write_bw_factor,
		total_ref_read_crcb,
		qsmmu_bw_overhead_factor;
	fp_t integer_part, frac_part;
	unsigned long ret = 0;

	/* Output parameters */
	struct {
		fp_t vsp_read, vsp_write, collocated_read, collocated_write,
			ref_read_y, ref_read_crcb, ref_write,
			ref_write_overlap, orig_read,
			line_buffer_read, line_buffer_write,
			total;
	} ddr = {0};

	struct {
		fp_t ref_read_crcb, line_buffer, total;
	} llc = {0};

	/* Encoder Parameters setup */
	rotation = d->rotation;
	cropping_or_scaling = false;
	vertical_tile_width = 960;
	/*
	 * recon_write_bw_factor varies according to resolution and bit-depth,
	 * here use 1.08(1.075) for worst case.
	 * Similar for ref_y_read_bw_factor, it can reach 1.375 for worst case,
	 * here use 1.3 for average case, and can somewhat balance the
	 * worst case assumption for UBWC CR factors.
	 */
	recon_write_bw_factor = FP(1, 8, 100);
	ref_y_read_bw_factor = FP(1, 30, 100);
	ref_cbcr_read_bw_factor = FP(1, 50, 100);


	/* Derived Parameters */
	fps = d->fps;
	width = max(d->output_width, BASELINE_DIMENSIONS.width);
	height = max(d->output_height, BASELINE_DIMENSIONS.height);
	downscaling_ratio = fp_div(FP_INT(d->input_width * d->input_height),
		FP_INT(d->output_width * d->output_height));
	downscaling_ratio = max(downscaling_ratio, FP_ONE);
	bitrate = d->bitrate > 0 ? DIV_ROUND_UP(d->bitrate, 1000000) :
		__lut(width, height, fps)->bitrate;
	lcu_size = d->lcu_size;
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);
	tnbr_per_lcu = 16;

	dpb_bpp = __bpp(d->color_formats[0], d->sid);

	y_bw_no_ubwc_8bpp = fp_div(FP_INT(width * height * fps),
		FP_INT(1000 * 1000));

	if (dpb_bpp != 8) {
		y_bw_no_ubwc_10bpp = fp_div(fp_mult(y_bw_no_ubwc_8bpp,
			FP_INT(256)), FP_INT(192));
		y_bw_10bpp_p010 = y_bw_no_ubwc_8bpp * 2;
	}

	b_frames_enabled = d->b_frames_enabled;
	original_color_format = d->num_formats >= 1 ?
		d->color_formats[0] : HAL_UNUSED_COLOR;

	original_compression_enabled = __ubwc(original_color_format);

	work_mode_1 = d->work_mode == HFI_WORKMODE_1;
	low_power = d->power_mode == VIDC_POWER_LOW;
	bins_to_bit_factor = 4;

	if (d->use_sys_cache) {
		llc_ref_chroma_cache_enabled = true;
		llc_top_line_buf_enabled = true,
		llc_vpss_rot_line_buf_enabled = true;
	}

	integer_part = Q16_INT(d->compression_ratio);
	frac_part = Q16_FRAC(d->compression_ratio);
	dpb_compression_factor = FP(integer_part, frac_part, 100);

	integer_part = Q16_INT(d->input_cr);
	frac_part = Q16_FRAC(d->input_cr);
	input_compression_factor = FP(integer_part, frac_part, 100);

	original_compression_factor = original_compression_factor_y =
		!original_compression_enabled ? FP_ONE :
		__compression_ratio(__lut(width, height, fps), dpb_bpp);
	/* use input cr if it is valid (not 1), otherwise use lut */
	if (original_compression_enabled &&
		input_compression_factor != FP_ONE) {
		original_compression_factor = input_compression_factor;
		/* Luma usually has lower compression factor than Chroma,
		 * input cf is overall cf, add 1.08 factor for Luma cf
		 */
		original_compression_factor_y =
			input_compression_factor > FP(1, 8, 100) ?
			fp_div(input_compression_factor, FP(1, 8, 100)) :
			input_compression_factor;
	}

	ddr.vsp_read = fp_div(FP_INT(bitrate * bins_to_bit_factor), FP_INT(8));
	ddr.vsp_write = ddr.vsp_read + fp_div(FP_INT(bitrate), FP_INT(8));

	collocated_bytes_per_lcu = lcu_size == 16 ? 16 :
				lcu_size == 32 ? 64 : 256;

	ddr.collocated_read = fp_div(FP_INT(lcu_per_frame *
			collocated_bytes_per_lcu * fps), FP_INT(bps(1)));

	ddr.collocated_write = ddr.collocated_read;

	ddr.ref_read_y = dpb_bpp == 8 ?
		y_bw_no_ubwc_8bpp : y_bw_no_ubwc_10bpp;
	if (b_frames_enabled)
		ddr.ref_read_y = ddr.ref_read_y * 2;
	ddr.ref_read_y = fp_div(ddr.ref_read_y, dpb_compression_factor);

	ddr.ref_read_crcb = fp_mult((ddr.ref_read_y / 2),
		ref_cbcr_read_bw_factor);

	if (width > vertical_tile_width) {
		ddr.ref_read_y = fp_mult(ddr.ref_read_y,
			ref_y_read_bw_factor);
	}

	if (llc_ref_chroma_cache_enabled) {
		total_ref_read_crcb = ddr.ref_read_crcb;
		ddr.ref_read_crcb = fp_div(ddr.ref_read_crcb,
			ref_cbcr_read_bw_factor);
		llc.ref_read_crcb = total_ref_read_crcb - ddr.ref_read_crcb;
	}

	ddr.ref_write = dpb_bpp == 8 ? y_bw_no_ubwc_8bpp : y_bw_no_ubwc_10bpp;
	ddr.ref_write = fp_div(fp_mult(ddr.ref_write, FP(1, 50, 100)),
			dpb_compression_factor);

	if (width > vertical_tile_width) {
		ddr.ref_write_overlap = fp_mult(ddr.ref_write,
			(recon_write_bw_factor - FP_ONE));
		ddr.ref_write = fp_mult(ddr.ref_write, recon_write_bw_factor);
	}

	ddr.orig_read = dpb_bpp == 8 ? y_bw_no_ubwc_8bpp :
		(original_compression_enabled ? y_bw_no_ubwc_10bpp :
		y_bw_10bpp_p010);
	ddr.orig_read = fp_div(fp_mult(fp_mult(ddr.orig_read, FP(1, 50, 100)),
		downscaling_ratio), original_compression_factor);
	if (rotation == 90 || rotation == 270)
		ddr.orig_read *= lcu_size == 32 ? (dpb_bpp == 8 ? 1 : 3) : 2;

	ddr.line_buffer_read =
		fp_div(FP_INT(tnbr_per_lcu * lcu_per_frame * fps),
			FP_INT(bps(1)));

	ddr.line_buffer_write = ddr.line_buffer_read;
	if (llc_top_line_buf_enabled) {
		llc.line_buffer = ddr.line_buffer_read + ddr.line_buffer_write;
		ddr.line_buffer_read = ddr.line_buffer_write = FP_ZERO;
	}

	ddr.total = ddr.vsp_read + ddr.vsp_write +
		ddr.collocated_read + ddr.collocated_write +
		ddr.ref_read_y + ddr.ref_read_crcb +
		ddr.ref_write + ddr.ref_write_overlap +
		ddr.orig_read +
		ddr.line_buffer_read + ddr.line_buffer_write;

	qsmmu_bw_overhead_factor = FP(1, 3, 100);
	ddr.total = fp_mult(ddr.total, qsmmu_bw_overhead_factor);
	llc.total = llc.ref_read_crcb + llc.line_buffer + ddr.total;

	if (msm_vidc_debug & VIDC_BUS) {
		struct dump dump[] = {
		{"ENCODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"width", "%d", width},
		{"height", "%d", height},
		{"fps", "%d", fps},
		{"dpb bitdepth", "%d", dpb_bpp},
		{"input downscaling ratio", DUMP_FP_FMT, downscaling_ratio},
		{"rotation", "%d", rotation},
		{"cropping or scaling", "%d", cropping_or_scaling},
		{"low power mode", "%d", low_power},
		{"work Mode", "%d", work_mode_1},
		{"B frame enabled", "%d", b_frames_enabled},
		{"original frame format", "%#x", original_color_format},
		{"original compression enabled", "%d",
			original_compression_enabled},
		{"dpb compression factor", DUMP_FP_FMT,
			dpb_compression_factor},
		{"input compression factor", DUMP_FP_FMT,
			input_compression_factor},
		{"llc ref chroma cache enabled", DUMP_FP_FMT,
			llc_ref_chroma_cache_enabled},
		{"llc top line buf enabled", DUMP_FP_FMT,
			llc_top_line_buf_enabled},
		{"llc vpss rot line buf enabled ", DUMP_FP_FMT,
			llc_vpss_rot_line_buf_enabled},

		{"DERIVED PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"lcu size", "%d", lcu_size},
		{"bitrate (Mbit/sec)", "%lu", bitrate},
		{"bins to bit factor", "%u", bins_to_bit_factor},
		{"original compression factor", DUMP_FP_FMT,
			original_compression_factor},
		{"original compression factor y", DUMP_FP_FMT,
			original_compression_factor_y},
		{"qsmmu_bw_overhead_factor",
			 DUMP_FP_FMT, qsmmu_bw_overhead_factor},
		{"bw for NV12 8bpc)", DUMP_FP_FMT, y_bw_no_ubwc_8bpp},
		{"bw for NV12 10bpc)", DUMP_FP_FMT, y_bw_no_ubwc_10bpp},

		{"INTERMEDIATE B/W DDR", "", DUMP_HEADER_MAGIC},
		{"vsp read", DUMP_FP_FMT, ddr.vsp_read},
		{"vsp write", DUMP_FP_FMT, ddr.vsp_write},
		{"collocated read", DUMP_FP_FMT, ddr.collocated_read},
		{"collocated write", DUMP_FP_FMT, ddr.collocated_write},
		{"ref read y", DUMP_FP_FMT, ddr.ref_read_y},
		{"ref read crcb", DUMP_FP_FMT, ddr.ref_read_crcb},
		{"ref write", DUMP_FP_FMT, ddr.ref_write},
		{"ref write overlap", DUMP_FP_FMT, ddr.ref_write_overlap},
		{"original read", DUMP_FP_FMT, ddr.orig_read},
		{"line buffer read", DUMP_FP_FMT, ddr.line_buffer_read},
		{"line buffer write", DUMP_FP_FMT, ddr.line_buffer_write},
		{"INTERMEDIATE LLC B/W", "", DUMP_HEADER_MAGIC},
		{"llc ref read crcb", DUMP_FP_FMT, llc.ref_read_crcb},
		{"llc line buffer", DUMP_FP_FMT, llc.line_buffer},
		};
		__dump(dump, ARRAY_SIZE(dump), d->sid);
	}

	d->calc_bw_ddr = kbps(fp_round(ddr.total));
	d->calc_bw_llcc = kbps(fp_round(llc.total));

	return ret;
}

static unsigned long __calculate(struct vidc_bus_vote_data *d)
{
	unsigned long value = 0;

	switch (d->domain) {
	case HAL_VIDEO_DOMAIN_VPE:
		value = __calculate_vpe(d);
		break;
	case HAL_VIDEO_DOMAIN_ENCODER:
		value = __calculate_encoder(d);
		break;
	case HAL_VIDEO_DOMAIN_DECODER:
		value = __calculate_decoder(d);
		break;
	case HAL_VIDEO_DOMAIN_CVP:
		value = __calculate_cvp(d);
		break;
	default:
		s_vpr_e(d->sid, "Unknown Domain %#x", d->domain);
	}

	return value;
}

int calc_bw_iris2(struct vidc_bus_vote_data *vidc_data)
{
	int ret = 0;

	if (!vidc_data)
		return ret;

	ret = __calculate(vidc_data);

	return ret;
}

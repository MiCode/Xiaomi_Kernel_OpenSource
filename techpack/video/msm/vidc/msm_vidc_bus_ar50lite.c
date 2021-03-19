// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include "msm_vidc_bus.h"
#include "msm_vidc_internal.h"

static unsigned long __calculate_encoder(struct vidc_bus_vote_data *d)
{
	/* Encoder Parameters */
	int width, height, fps, bitrate, lcu_size;

	/* Derived Parameter */
	int search_range, lcu_per_frame;
	fp_t y_bw;
	bool is_h264_category = true;
	fp_t orig_read_factor, recon_write_factor,
		ref_y_read_factor, ref_c_read_factor, lb_factor,
		rest_factor, total_read_factor, total_write_factor,
		total_factor, overhead_factor;

	/* Output parameters */
	fp_t orig_read, recon_write,
			ref_y_read, ref_c_read,
			lb_read, lb_write,
			bse_read, bse_write,
			total_read, total_write,
			total;

	unsigned long ret = 0;

	/* Encoder Fixed Parameters */
	overhead_factor = FP(1, 3, 100);
	orig_read_factor = FP(1, 50, 100); /* L + C */
	recon_write_factor = FP(1, 50, 100); /* L + C */
	ref_c_read_factor = FP(0, 75, 100); /* 1.5/2  ( 1.5 Cache efficiency )*/
	lb_factor = FP(1, 25, 100); /* Worst case : HEVC 720p = 1.25 */

	fps = d->fps;
	width = max(d->output_width, BASELINE_DIMENSIONS.width);
	height = max(d->output_height, BASELINE_DIMENSIONS.height);
	bitrate = d->bitrate > 0 ? (d->bitrate + 1000000 - 1) / 1000000 :
		__lut(width, height, fps)->bitrate;
	lcu_size = d->lcu_size;

	/* Derived Parameters Setup*/
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);

	if (d->codec == HAL_VIDEO_CODEC_HEVC ||
		d->codec == HAL_VIDEO_CODEC_VP9) {
		/* H264, VP8, MPEG2 use the same settings */
		/* HEVC, VP9 use the same setting */
		is_h264_category = false;
	}

	search_range = 48;

	y_bw = fp_mult(fp_mult(FP_INT(width), FP_INT(height)), FP_INT(fps));
	y_bw = fp_div(y_bw, FP_INT(1000000));

	ref_y_read_factor = fp_div(FP_INT(search_range * 2), FP_INT(lcu_size));
	ref_y_read_factor = ref_y_read_factor + FP_INT(1);

	rest_factor = FP_INT(bitrate) + fp_div(FP_INT(bitrate), FP_INT(8));
	rest_factor = fp_div(rest_factor, y_bw);

	total_read_factor = fp_div(rest_factor, FP_INT(2)) +
		fp_div(lb_factor, FP_INT(2));
	total_read_factor = total_read_factor + orig_read_factor +
		ref_y_read_factor + ref_c_read_factor;

	total_write_factor = fp_div(rest_factor, FP_INT(2)) +
		fp_div(lb_factor, FP_INT(2));
	total_write_factor = total_write_factor + recon_write_factor;

	total_factor = total_read_factor + total_write_factor;

	orig_read = fp_mult(y_bw, orig_read_factor);
	recon_write = fp_mult(y_bw, recon_write_factor);
	ref_y_read = fp_mult(y_bw, ref_y_read_factor);
	ref_c_read = fp_mult(y_bw, ref_c_read_factor);
	lb_read = fp_div(fp_mult(y_bw, lb_factor), FP_INT(2));
	lb_write = lb_read;
	bse_read = fp_mult(y_bw, fp_div(rest_factor, FP_INT(2)));
	bse_write = bse_read;

	total_read = orig_read + ref_y_read + ref_c_read +
		lb_read + bse_read;
	total_write = recon_write + lb_write + bse_write;

	total = total_read + total_write;
	total = fp_mult(total, overhead_factor);

	if (msm_vidc_debug & VIDC_BUS) {
		struct dump dump[] = {
		{"ENCODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"width", "%d", width},
		{"height", "%d", height},
		{"fps", "%d", fps},
		{"bitrate (Mbit/sec)", "%lu", bitrate},
		{"lcu size", "%d", lcu_size},

		{"DERIVED PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"lcu/frame", "%d", lcu_per_frame},
		{"Y BW", DUMP_FP_FMT, y_bw},
		{"search range", "%d", search_range},
		{"original read factor", DUMP_FP_FMT, orig_read_factor},
		{"recon write factor", DUMP_FP_FMT, recon_write_factor},
		{"ref read Y factor", DUMP_FP_FMT, ref_y_read_factor},
		{"ref read C factor", DUMP_FP_FMT, ref_c_read_factor},
		{"lb factor", DUMP_FP_FMT, lb_factor},
		{"rest factor", DUMP_FP_FMT, rest_factor},
		{"total_read_factor", DUMP_FP_FMT, total_read_factor},
		{"total_write_factor", DUMP_FP_FMT, total_write_factor},
		{"total_factor", DUMP_FP_FMT, total_factor},
		{"overhead_factor", DUMP_FP_FMT, overhead_factor},

		{"INTERMEDIATE B/W DDR", "", DUMP_HEADER_MAGIC},
		{"orig read", DUMP_FP_FMT, orig_read},
		{"recon write", DUMP_FP_FMT, recon_write},
		{"ref read Y", DUMP_FP_FMT, ref_y_read},
		{"ref read C", DUMP_FP_FMT, ref_c_read},
		{"lb read", DUMP_FP_FMT, lb_read},
		{"lb write", DUMP_FP_FMT, lb_write},
		{"bse read", DUMP_FP_FMT, bse_read},
		{"bse write", DUMP_FP_FMT, bse_write},
		{"total read", DUMP_FP_FMT, total_read},
		{"total write", DUMP_FP_FMT, total_write},
		{"total", DUMP_FP_FMT, total},
		};
		__dump(dump, ARRAY_SIZE(dump), d->sid);
	}


	d->calc_bw_ddr = kbps(fp_round(total));

	return ret;
}

static unsigned long __calculate_decoder(struct vidc_bus_vote_data *d)
{
	/* Decoder parameters */
	int width, height, fps, bitrate, lcu_size;

	/* Derived parameters */
	int lcu_per_frame, motion_complexity;
	fp_t y_bw;
	bool is_h264_category = true;
	fp_t recon_write_factor, ref_read_factor, lb_factor,
		rest_factor, opb_factor,
		total_read_factor, total_write_factor,
		total_factor, overhead_factor;

	/* Output parameters */
	fp_t opb_write, recon_write,
			ref_read,
			lb_read, lb_write,
			bse_read, bse_write,
			total_read, total_write,
			total;

	unsigned long ret = 0;

	/* Decoder Fixed Parameters */
	overhead_factor = FP(1, 3, 100);
	recon_write_factor = FP(1, 50, 100); /* L + C */
	opb_factor = FP(1, 50, 100); /* L + C */
	lb_factor = FP(1, 13, 100); /* Worst case : H264 1080p = 1.125 */
	motion_complexity = 5; /* worst case complexity */

	fps = d->fps;
	width = max(d->output_width, BASELINE_DIMENSIONS.width);
	height = max(d->output_height, BASELINE_DIMENSIONS.height);
	bitrate = d->bitrate > 0 ? (d->bitrate + 1000000 - 1) / 1000000 :
		__lut(width, height, fps)->bitrate;
	lcu_size = d->lcu_size;

	/* Derived Parameters Setup*/
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);

	if (d->codec == HAL_VIDEO_CODEC_HEVC ||
		d->codec == HAL_VIDEO_CODEC_VP9) {
		/* H264, VP8, MPEG2 use the same settings */
		/* HEVC, VP9 use the same setting */
		is_h264_category = false;
	}

	y_bw = fp_mult(fp_mult(FP_INT(width), FP_INT(height)), FP_INT(fps));
	y_bw = fp_div(y_bw, FP_INT(1000000));

	ref_read_factor = FP(1, 50, 100); /* L + C */
	ref_read_factor = fp_mult(ref_read_factor, FP_INT(motion_complexity));

	rest_factor = FP_INT(bitrate) + fp_div(FP_INT(bitrate), FP_INT(8));
	rest_factor = fp_div(rest_factor, y_bw);

	total_read_factor = fp_div(rest_factor, FP_INT(2)) +
		fp_div(lb_factor, FP_INT(2));
	total_read_factor = total_read_factor + ref_read_factor;

	total_write_factor = fp_div(rest_factor, FP_INT(2));
	total_write_factor = total_write_factor +
		recon_write_factor + opb_factor;

	total_factor = total_read_factor + total_write_factor;

	recon_write = fp_mult(y_bw, recon_write_factor);
	ref_read = fp_mult(y_bw, ref_read_factor);
	lb_read = fp_div(fp_mult(y_bw, lb_factor), FP_INT(2));
	lb_write = lb_read;
	bse_read = fp_div(fp_mult(y_bw, rest_factor), FP_INT(2));
	bse_write = bse_read;
	opb_write = fp_mult(y_bw, opb_factor);

	total_read = ref_read + lb_read + bse_read;
	total_write = recon_write + lb_write + bse_write + opb_write;

	total = total_read + total_write;
	total = fp_mult(total, overhead_factor);

	if (msm_vidc_debug & VIDC_BUS) {
		struct dump dump[] = {
		{"DECODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"width", "%d", width},
		{"height", "%d", height},
		{"fps", "%d", fps},
		{"bitrate (Mbit/sec)", "%lu", bitrate},
		{"lcu size", "%d", lcu_size},

		{"DERIVED PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"lcu/frame", "%d", lcu_per_frame},
		{"Y BW", DUMP_FP_FMT, y_bw},
		{"motion complexity", "%d", motion_complexity},
		{"recon write factor", DUMP_FP_FMT, recon_write_factor},
		{"ref_read_factor", DUMP_FP_FMT, ref_read_factor},
		{"lb factor", DUMP_FP_FMT, lb_factor},
		{"rest factor", DUMP_FP_FMT, rest_factor},
		{"opb factor", DUMP_FP_FMT, opb_factor},
		{"total_read_factor", DUMP_FP_FMT, total_read_factor},
		{"total_write_factor", DUMP_FP_FMT, total_write_factor},
		{"total_factor", DUMP_FP_FMT, total_factor},
		{"overhead_factor", DUMP_FP_FMT, overhead_factor},

		{"INTERMEDIATE B/W DDR", "", DUMP_HEADER_MAGIC},
		{"recon write", DUMP_FP_FMT, recon_write},
		{"ref read", DUMP_FP_FMT, ref_read},
		{"lb read", DUMP_FP_FMT, lb_read},
		{"lb write", DUMP_FP_FMT, lb_write},
		{"bse read", DUMP_FP_FMT, bse_read},
		{"bse write", DUMP_FP_FMT, bse_write},
		{"opb write", DUMP_FP_FMT, opb_write},
		{"total read", DUMP_FP_FMT, total_read},
		{"total write", DUMP_FP_FMT, total_write},
		{"total", DUMP_FP_FMT, total},
		};
		__dump(dump, ARRAY_SIZE(dump), d->sid);
	}

	d->calc_bw_ddr = kbps(fp_round(total));

	return ret;
}

static unsigned long __calculate(struct vidc_bus_vote_data *d)
{
	unsigned long value = 0;

	switch (d->domain) {
	case HAL_VIDEO_DOMAIN_ENCODER:
		value = __calculate_encoder(d);
		break;
	case HAL_VIDEO_DOMAIN_DECODER:
		value = __calculate_decoder(d);
		break;
	default:
		s_vpr_e(d->sid, "Unknown Domain %#x", d->domain);
	}

	return value;
}

int calc_bw_ar50lt(struct vidc_bus_vote_data *vidc_data)
{
	int ret = 0;

	if (!vidc_data)
		return ret;

	ret = __calculate(vidc_data);

	return ret;
}

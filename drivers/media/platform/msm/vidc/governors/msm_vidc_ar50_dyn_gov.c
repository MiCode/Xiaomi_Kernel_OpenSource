/* Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include "governor.h"
#include "fixedpoint.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "vidc_hfi_api.h"
#define COMPRESSION_RATIO_MAX 5

static bool debug;
module_param(debug, bool, 0644);

enum governor_mode {
	GOVERNOR_DDR,
	GOVERNOR_LLCC,
};

struct governor {
	enum governor_mode mode;
	struct devfreq_governor devfreq_gov;
};

/*
 * Minimum dimensions that the governor is willing to calculate
 * bandwidth for.  This means that anything bandwidth(0, 0) ==
 * bandwidth(BASELINE_DIMENSIONS.width, BASELINE_DIMENSIONS.height)
 */
static const struct {
	int height, width;
} BASELINE_DIMENSIONS = {
	.width = 1280,
	.height = 720,
};

/*
 * These are hardcoded AB values that the governor votes for in certain
 * situations, where a certain bus frequency is desired.  It isn't exactly
 * scalable since different platforms have different bus widths, but we'll
 * deal with that in the future.
 */
static const unsigned long NOMINAL_BW_MBPS = 6000 /* ideally 320 Mhz */,
	SVS_BW_MBPS = 2000 /* ideally 100 Mhz */;

/* converts Mbps to bps (the "b" part can be bits or bytes based on context) */
#define kbps(__mbps) ((__mbps) * 1000)
#define bps(__mbps) (kbps(__mbps) * 1000)

#define GENERATE_COMPRESSION_PROFILE(__bpp, __worst) {              \
	.bpp = __bpp,                                                          \
	.ratio = __worst,                \
}

/*
 * The below table is a structural representation of the following table:
 *  Resolution |    Bitrate |              Compression Ratio          |
 * ............|............|.........................................|
 * Width Height|Average High|Avg_8bpc Worst_8bpc Avg_10bpc Worst_10bpc|
 *  1280    720|      7   14|    1.69       1.28      1.49        1.23|
 *  1920   1080|     20   40|    1.69       1.28      1.49        1.23|
 *  2560   1440|     32   64|     2.2       1.26      1.97        1.22|
 *  3840   2160|     42   84|     2.2       1.26      1.97        1.22|
 *  4096   2160|     44   88|     2.2       1.26      1.97        1.22|
 *  4096   2304|     48   96|     2.2       1.26      1.97        1.22|
 */
static struct lut {
	int frame_size; /* width x height */
	int frame_rate;
	unsigned long bitrate;
	struct {
		int bpp;
		fp_t ratio;
	} compression_ratio[COMPRESSION_RATIO_MAX];
} const LUT[] = {
	{
		.frame_size = 1280 * 720,
		.frame_rate = 30,
		.bitrate = 14,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1280 * 720,
		.frame_rate = 60,
		.bitrate = 22,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1920 * 1088,
		.frame_rate = 30,
		.bitrate = 40,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1920 * 1088,
		.frame_rate = 60,
		.bitrate = 64,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 2560 * 1440,
		.frame_rate = 30,
		.bitrate = 64,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 2560 * 1440,
		.frame_rate = 60,
		.bitrate = 102,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 3840 * 2160,
		.frame_rate = 30,
		.bitrate = 84,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 3840 * 2160,
		.frame_rate = 60,
		.bitrate = 134,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2160,
		.frame_rate = 30,
		.bitrate = 88,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2160,
		.frame_rate = 60,
		.bitrate = 141,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2304,
		.frame_rate = 30,
		.bitrate = 96,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2304,
		.frame_rate = 60,
		.bitrate = 154,
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 22, 100)),
		}
	},
};

static struct lut const *__lut(int width, int height, int fps)
{
	int frame_size = height * width, c = 0;

	do {
		if (LUT[c].frame_size >= frame_size && LUT[c].frame_rate >= fps)
			return &LUT[c];
	} while (++c < ARRAY_SIZE(LUT));

	return &LUT[ARRAY_SIZE(LUT) - 1];
}

static fp_t __compression_ratio(struct lut const *entry, int bpp)
{
	int c = 0;

	for (c = 0; c < COMPRESSION_RATIO_MAX; ++c) {
		if (entry->compression_ratio[c].bpp == bpp)
			return entry->compression_ratio[c].ratio;
	}

	WARN(true, "Shouldn't be here, LUT possibly corrupted?\n");
	return FP_ZERO; /* impossible */
}

#define DUMP_HEADER_MAGIC 0xdeadbeef
#define DUMP_FP_FMT "%FP" /* special format for fp_t */
struct dump {
	char *key;
	char *format;
	size_t val;
};

static void __dump(struct dump dump[], int len)
{
	int c = 0;

	for (c = 0; c < len; ++c) {
		char format_line[128] = "", formatted_line[128] = "";

		if (dump[c].val == DUMP_HEADER_MAGIC) {
			snprintf(formatted_line, sizeof(formatted_line), "%s\n",
					dump[c].key);
		} else {
			bool fp_format = !strcmp(dump[c].format, DUMP_FP_FMT);

			if (!fp_format) {
				snprintf(format_line, sizeof(format_line),
						"    %-35s: %s\n", dump[c].key,
						dump[c].format);
				snprintf(formatted_line, sizeof(formatted_line),
						format_line, dump[c].val);
			} else {
				size_t integer_part, fractional_part;

				integer_part = fp_int(dump[c].val);
				fractional_part = fp_frac(dump[c].val);
				snprintf(formatted_line, sizeof(formatted_line),
						"    %-35s: %zd + %zd/%zd\n",
						dump[c].key, integer_part,
						fractional_part,
						fp_frac_base());


			}
		}

		dprintk(VIDC_DBG, "%s", formatted_line);
	}
}

static unsigned long __calculate_vpe(struct vidc_bus_vote_data *d,
		enum governor_mode gm)
{
	return 0;
}

static bool __ubwc(enum hal_uncompressed_format f)
{
	switch (f) {
	case HAL_COLOR_FORMAT_NV12_UBWC:
	case HAL_COLOR_FORMAT_NV12_TP10_UBWC:
		return true;
	default:
		return false;
	}
}

static int __bpp(enum hal_uncompressed_format f)
{
	switch (f) {
	case HAL_COLOR_FORMAT_NV12:
	case HAL_COLOR_FORMAT_NV21:
	case HAL_COLOR_FORMAT_NV12_UBWC:
		return 8;
	case HAL_COLOR_FORMAT_NV12_TP10_UBWC:
	case HAL_COLOR_FORMAT_P010:
		return 10;
	default:
		dprintk(VIDC_ERR,
				"What's this?  We don't support this colorformat (%x)",
				f);
		return INT_MAX;
	}
}

static unsigned long __calculate_decoder(struct vidc_bus_vote_data *d,
		enum governor_mode gm)
{
	/*
	 * XXX: Don't fool around with any of the hardcoded numbers unless you
	 * know /exactly/ what you're doing.  Many of these numbers are
	 * measured heuristics and hardcoded numbers taken from the firmware.
	 */
	/* Decoder parameters */
	int width, height, lcu_size, dpb_bpp, opb_bpp, fps, opb_factor;
	bool unified_dpb_opb, dpb_compression_enabled, opb_compression_enabled,
		llc_ref_read_l2_cache_enabled = false,
		llc_vpss_ds_line_buf_enabled = false;
	fp_t dpb_opb_scaling_ratio, dpb_read_compression_factor,
		dpb_write_compression_factor, opb_compression_factor,
		qsmmu_bw_overhead_factor, height_ratio;

	/* Derived parameters */
	int lcu_per_frame, tnbr_per_lcu, colocated_bytes_per_lcu;
	unsigned long bitrate;

	fp_t bins_to_bit_factor, dpb_write_factor, ten_bpc_packing_factor,
		ten_bpc_bpp_factor, vsp_read_factor, vsp_write_factor,
		bw_for_1x_8bpc, dpb_bw_for_1x,
		motion_vector_complexity = 0, row_cache_penalty = 0, opb_bw = 0,
		dpb_total = 0;

	/* Output parameters */
	struct {
		fp_t vsp_read, vsp_write, collocated_read, collocated_write,
			line_buffer_read, line_buffer_write, recon_read,
			recon_write, opb_read, opb_write, dpb_read, dpb_write,
			total;
	} ddr = {0};

	struct {
		fp_t dpb_read, opb_read, total;
	} llc = {0};

	unsigned long ret = 0;
	unsigned int integer_part, frac_part;

	width = max(d->input_width, BASELINE_DIMENSIONS.width);
	height = max(d->input_height, BASELINE_DIMENSIONS.height);

	lcu_size = d->lcu_size;

	dpb_bpp = d->num_formats >= 1 ? __bpp(d->color_formats[0]) : INT_MAX;
	opb_bpp = d->num_formats >= 2 ?  __bpp(d->color_formats[1]) : dpb_bpp;

	fps = d->fps;

	unified_dpb_opb = d->num_formats == 1;

	dpb_opb_scaling_ratio = fp_div(FP_INT(
		(int)(d->input_width * d->input_height)),
		FP_INT((int)(d->output_width * d->output_height)));
	height_ratio = fp_div(d->input_height, d->output_height);

	dpb_compression_enabled = d->num_formats >= 1 &&
		__ubwc(d->color_formats[0]);
	opb_compression_enabled = d->num_formats >= 2 &&
		__ubwc(d->color_formats[1]);

	/*
	 * Convert Q16 number into Integer and Fractional part upto 2 places.
	 * Ex : 105752 / 65536 = 1.61; 1.61 in Q16 = 105752;
	 * Integer part =  105752 / 65536 = 1;
	 * Reminder = 105752 - 1 * 65536 = 40216;
	 * Fractional part = 40216 * 100 / 65536 = 61;
	 * Now converto to FP(1, 61, 100) for below code.
	 */

	integer_part = d->compression_ratio >> 16;
	frac_part =
		((d->compression_ratio - (integer_part << 16)) * 100) >> 16;

	dpb_read_compression_factor = FP(integer_part, frac_part, 100);

	integer_part = d->complexity_factor >> 16;
	frac_part =
		((d->complexity_factor - (integer_part << 16)) * 100) >> 16;

	motion_vector_complexity = FP(integer_part, frac_part, 100);

	dpb_write_compression_factor = dpb_read_compression_factor;

	opb_compression_factor = !opb_compression_enabled ? FP_ONE :
				dpb_write_compression_factor;

	llc_ref_read_l2_cache_enabled = llc_vpss_ds_line_buf_enabled = false;
	if (d->use_sys_cache) {
		llc_ref_read_l2_cache_enabled = true;
		llc_vpss_ds_line_buf_enabled = true;
	}

	/* Derived parameters setup */
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);

	bitrate = (d->bitrate + 1000000 - 1) / 1000000;

	bins_to_bit_factor = d->work_mode == VIDC_WORK_MODE_1 ?
		FP_INT(0) : FP_INT(4);

	vsp_read_factor = bins_to_bit_factor + FP_INT(2);

	dpb_write_factor = FP(1, 5, 100);

	ten_bpc_packing_factor = FP(1, 67, 1000);
	ten_bpc_bpp_factor = FP(1, 1, 4);

	vsp_write_factor = bins_to_bit_factor;

	tnbr_per_lcu = lcu_size == 16 ? 128 :
		lcu_size == 32 ? 64 : 128;

	colocated_bytes_per_lcu = lcu_size == 16 ? 16 :
				lcu_size == 32 ? 64 : 256;

	/* ........................................ for DDR */
	ddr.vsp_read = fp_div(fp_mult(FP_INT(bitrate),
				vsp_read_factor), FP_INT(8));
	ddr.vsp_write = fp_div(fp_mult(FP_INT(bitrate),
				vsp_write_factor), FP_INT(8));

	ddr.collocated_read = FP_INT(lcu_per_frame *
			colocated_bytes_per_lcu * fps / bps(1));
	ddr.collocated_write = FP_INT(lcu_per_frame *
			colocated_bytes_per_lcu * fps / bps(1));

	ddr.line_buffer_read = FP_INT(tnbr_per_lcu *
			lcu_per_frame * fps / bps(1));
	ddr.line_buffer_write = ddr.line_buffer_read;

	bw_for_1x_8bpc = fp_div(FP_INT((int)(width * height)), FP_INT(32 * 8));

	bw_for_1x_8bpc = fp_mult(bw_for_1x_8bpc,
		fp_div(FP_INT(((int)(256 * fps))), FP_INT(1000 * 1000)));

	dpb_bw_for_1x = dpb_bpp == 8 ? bw_for_1x_8bpc :
		fp_mult(bw_for_1x_8bpc, fp_mult(ten_bpc_packing_factor,
				ten_bpc_bpp_factor));

	ddr.dpb_read = fp_div(fp_mult(fp_mult(dpb_bw_for_1x,
			motion_vector_complexity), dpb_write_factor),
			dpb_read_compression_factor);

	ddr.dpb_write = fp_div(fp_mult(dpb_bw_for_1x, dpb_write_factor),
		dpb_write_compression_factor);
	dpb_total = ddr.dpb_read + ddr.dpb_write;
	if (llc_ref_read_l2_cache_enabled) {
		row_cache_penalty = FP(1, 30, 100);
		ddr.dpb_read = fp_div(ddr.dpb_read, row_cache_penalty);
		llc.dpb_read = dpb_total - ddr.dpb_write - ddr.dpb_read;
	}

	opb_factor = dpb_bpp == 8 ? 8 : 4;

	ddr.opb_read = unified_dpb_opb ? 0 : opb_compression_enabled ?
		fp_div(fp_mult(fp_div(dpb_bw_for_1x, dpb_opb_scaling_ratio),
		FP_INT(opb_factor)), height_ratio) : 0;
	ddr.opb_write = unified_dpb_opb ? 0 : opb_compression_enabled ?
		ddr.dpb_read : fp_div(fp_div(fp_mult(dpb_bw_for_1x,
		FP(1, 50, 100)), dpb_opb_scaling_ratio),
			opb_compression_factor);

	if (llc_vpss_ds_line_buf_enabled) {
		llc.opb_read = ddr.opb_read;
		ddr.opb_write -= ddr.opb_read;
		ddr.opb_read = 0;
	}
	ddr.total = ddr.vsp_read + ddr.vsp_write +
		ddr.collocated_read + ddr.collocated_write +
		ddr.line_buffer_read + ddr.line_buffer_write +
		ddr.opb_read + ddr.opb_write +
		ddr.dpb_read + ddr.dpb_write;

	qsmmu_bw_overhead_factor = FP(1, 3, 100);

	ddr.total = fp_mult(ddr.total, qsmmu_bw_overhead_factor);
	llc.total = llc.dpb_read + llc.opb_read + ddr.total;

	/* Dump all the variables for easier debugging */
	if (debug) {
		struct dump dump[] = {
		{"DECODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"LCU size", "%d", lcu_size},
		{"DPB bitdepth", "%d", dpb_bpp},
		{"frame rate", "%d", fps},
		{"DPB/OPB unified", "%d", unified_dpb_opb},
		{"DPB/OPB downscaling ratio", DUMP_FP_FMT,
			dpb_opb_scaling_ratio},
		{"DPB compression", "%d", dpb_compression_enabled},
		{"OPB compression", "%d", opb_compression_enabled},
		{"DPB Read compression factor", DUMP_FP_FMT,
			dpb_read_compression_factor},
		{"DPB Write compression factor", DUMP_FP_FMT,
			dpb_write_compression_factor},
		{"OPB compression factor", DUMP_FP_FMT,
			opb_compression_factor},
		{"frame width", "%d", width},
		{"frame height", "%d", height},

		{"DERIVED PARAMETERS (1)", "", DUMP_HEADER_MAGIC},
		{"LCUs/frame", "%d", lcu_per_frame},
		{"bitrate (Mbit/sec)", "%d", bitrate},
		{"bins to bit factor", DUMP_FP_FMT, bins_to_bit_factor},
		{"DPB write factor", DUMP_FP_FMT, dpb_write_factor},
		{"10bpc packing factor", DUMP_FP_FMT,
			ten_bpc_packing_factor},
		{"10bpc,BPP factor", DUMP_FP_FMT, ten_bpc_bpp_factor},
		{"VSP read factor", DUMP_FP_FMT, vsp_read_factor},
		{"VSP write factor", DUMP_FP_FMT, vsp_write_factor},
		{"TNBR/LCU", "%d", tnbr_per_lcu},
		{"colocated bytes/LCU", "%d", colocated_bytes_per_lcu},
		{"B/W for 1x (NV12 8bpc)", DUMP_FP_FMT, bw_for_1x_8bpc},
		{"DPB B/W For 1x (NV12)", DUMP_FP_FMT, dpb_bw_for_1x},

		{"DERIVED PARAMETERS (2)", "", DUMP_HEADER_MAGIC},
		{"MV complexity", DUMP_FP_FMT, motion_vector_complexity},
		{"row cache penalty", DUMP_FP_FMT, row_cache_penalty},
		{"qsmmu_bw_overhead_factor", DUMP_FP_FMT,
			qsmmu_bw_overhead_factor},
		{"OPB B/W (single instance)", DUMP_FP_FMT, opb_bw},

		{"INTERMEDIATE DDR B/W", "", DUMP_HEADER_MAGIC},
		{"VSP read", DUMP_FP_FMT, ddr.vsp_read},
		{"VSP write", DUMP_FP_FMT, ddr.vsp_write},
		{"collocated read", DUMP_FP_FMT, ddr.collocated_read},
		{"collocated write", DUMP_FP_FMT, ddr.collocated_write},
		{"line buffer read", DUMP_FP_FMT, ddr.line_buffer_read},
		{"line buffer write", DUMP_FP_FMT, ddr.line_buffer_write},
		{"recon read", DUMP_FP_FMT, ddr.recon_read},
		{"recon write", DUMP_FP_FMT, ddr.recon_write},
		{"OPB read", DUMP_FP_FMT, ddr.opb_read},
		{"OPB write", DUMP_FP_FMT, ddr.opb_write},
		{"DPB read", DUMP_FP_FMT, ddr.dpb_read},
		{"DPB write", DUMP_FP_FMT, ddr.dpb_write},
		{"LLC DPB read", DUMP_FP_FMT, llc.dpb_read},
		{"LLC OPB read", DUMP_FP_FMT, llc.opb_read},

		};
		__dump(dump, ARRAY_SIZE(dump));
	}

	switch (gm) {
	case GOVERNOR_DDR:
		ret = kbps(fp_round(ddr.total));
		break;
	case GOVERNOR_LLCC:
		ret = kbps(fp_round(llc.total));
		break;
	default:
		dprintk(VIDC_ERR, "%s - Unknown governor\n", __func__);
	}

	return ret;
}

static unsigned long __calculate_encoder(struct vidc_bus_vote_data *d,
		enum governor_mode gm)
{
	/*
	 * XXX: Don't fool around with any of the hardcoded numbers unless you
	 * know /exactly/ what you're doing.  Many of these numbers are
	 * measured heuristics and hardcoded numbers taken from the firmware.
	 */
	/* Encoder Parameters */

	int width, height, fps, dpb_bpp, lcu_per_frame, lcu_size,
		vertical_tile_width, colocated_bytes_per_lcu, bitrate,
		ref_overlap_bw_factor;
	enum hal_uncompressed_format dpb_color_format, original_color_format;
	bool dpb_compression_enabled, original_compression_enabled,
		work_mode_1, low_power, rotation, cropping_or_scaling,
		b_frames_enabled = false,
		llc_dual_core_ref_read_buf_enabled = false,
		llc_top_line_buf_enabled = false,
		llc_ref_chroma_cache_enabled = false;
	fp_t dpb_compression_factor, original_compression_factor,
		input_compression_factor, qsmmu_bw_overhead_factor,
		ref_y_bw_factor, ref_cb_cr_bw_factor, ten_bpc_bpp_factor,
		bw_for_1x_8bpc, dpb_bw_for_1x, ref_cb_cr_read,
		bins_to_bit_factor, ref_y_read,	ten_bpc_packing_factor,
		dpb_write_factor, ref_overlap_bw, llc_ref_y_read,
		llc_ref_cb_cr_read;
	fp_t integer_part, frac_part;
	unsigned long ret = 0;

	/* Output parameters */
	struct {
		fp_t vsp_read, vsp_write, collocated_read, collocated_write,
			line_buffer_read, line_buffer_write, original_read,
			original_write, dpb_read, dpb_write, total;
	} ddr = {0};

	struct {
		fp_t dpb_read, line_buffer, total;
	} llc = {0};

	/* Encoder Parameters setup */
	ten_bpc_packing_factor = FP(1, 67, 1000);
	ten_bpc_bpp_factor = FP(1, 1, 4);
	rotation = false;
	cropping_or_scaling = false;
	vertical_tile_width = 960;
	ref_y_bw_factor = FP(1, 30, 100);
	ref_cb_cr_bw_factor = FP(1, 50, 100);
	dpb_write_factor = FP(1, 8, 100);


	/* Derived Parameters */
	lcu_size = d->lcu_size;
	fps = d->fps;
	b_frames_enabled = d->b_frames_enabled;
	width = max(d->input_width, BASELINE_DIMENSIONS.width);
	height = max(d->input_height, BASELINE_DIMENSIONS.height);
	bitrate = d->bitrate > 0 ? d->bitrate / 1000000 :
		__lut(width, height, fps)->bitrate;
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);

	dpb_color_format = HAL_COLOR_FORMAT_NV12_UBWC;
	original_color_format = d->num_formats >= 1 ?
		d->color_formats[0] : HAL_UNUSED_COLOR;

	dpb_bpp = d->num_formats >= 1 ? __bpp(d->color_formats[0]) : INT_MAX;

	dpb_compression_enabled = __ubwc(dpb_color_format);
	original_compression_enabled = __ubwc(original_color_format);

	work_mode_1 = d->work_mode == VIDC_WORK_MODE_1;
	low_power = d->power_mode == VIDC_POWER_LOW;
	bins_to_bit_factor = work_mode_1 ?
		FP_INT(0) : FP_INT(4);

	if (d->use_sys_cache) {
		llc_dual_core_ref_read_buf_enabled = true;
		llc_ref_chroma_cache_enabled = true;
	}

	/*
	 * Convert Q16 number into Integer and Fractional part upto 2 places.
	 * Ex : 105752 / 65536 = 1.61; 1.61 in Q16 = 105752;
	 * Integer part =  105752 / 65536 = 1;
	 * Reminder = 105752 - 1 * 65536 = 40216;
	 * Fractional part = 40216 * 100 / 65536 = 61;
	 * Now converto to FP(1, 61, 100) for below code.
	 */

	integer_part = d->compression_ratio >> 16;
	frac_part =
		((d->compression_ratio - (integer_part * 65536)) * 100) >> 16;

	dpb_compression_factor = FP(integer_part, frac_part, 100);

	integer_part = d->input_cr >> 16;
	frac_part =
		((d->input_cr - (integer_part * 65536)) * 100) >> 16;

	input_compression_factor = FP(integer_part, frac_part, 100);

	/* use input cr if it is valid (not 1), otherwise use lut */
	original_compression_factor =
		!original_compression_enabled ? FP_ONE :
		input_compression_factor != FP_ONE ? input_compression_factor :
		__compression_ratio(__lut(width, height, fps), dpb_bpp);

	ddr.vsp_read = fp_mult(fp_div(FP_INT(bitrate), FP_INT(8)),
			bins_to_bit_factor);
	ddr.vsp_write = ddr.vsp_read + fp_div(FP_INT(bitrate), FP_INT(8));

	colocated_bytes_per_lcu = lcu_size == 16 ? 16 :
				lcu_size == 32 ? 64 : 256;

	ddr.collocated_read = FP_INT(lcu_per_frame *
			colocated_bytes_per_lcu * fps / bps(1));

	ddr.collocated_write = ddr.collocated_read;

	ddr.line_buffer_read = FP_INT(16 * lcu_per_frame * fps / bps(1));

	ddr.line_buffer_write = ddr.line_buffer_read;

	llc.line_buffer = ddr.line_buffer_read + ddr.line_buffer_write;
	if (llc_top_line_buf_enabled)
		ddr.line_buffer_read = ddr.line_buffer_write = FP_INT(0);

	llc.line_buffer -= (ddr.line_buffer_read + ddr.line_buffer_write);

	bw_for_1x_8bpc = fp_div(FP_INT((int)(width * height)), FP_INT(32 * 8));

	bw_for_1x_8bpc = fp_mult(bw_for_1x_8bpc,
		fp_div(FP_INT(((int)(256 * fps))), FP_INT(1000 * 1000)));

	dpb_bw_for_1x = dpb_bpp == 8 ? bw_for_1x_8bpc :
		fp_mult(bw_for_1x_8bpc, fp_mult(ten_bpc_packing_factor,
			ten_bpc_bpp_factor));

	ddr.original_read = fp_div(fp_mult(FP(1, 50, 100), dpb_bw_for_1x),
		input_compression_factor);

	ddr.original_write = FP_ZERO;

	ref_y_bw_factor =
		width == vertical_tile_width ? FP_INT(1) : ref_y_bw_factor;

	ref_y_read = fp_mult(ref_y_bw_factor, dpb_bw_for_1x);

	ref_y_read = fp_div(ref_y_read, dpb_compression_factor);

	ref_y_read =
		b_frames_enabled ? fp_mult(ref_y_read, FP_INT(2)) : ref_y_read;

	llc_ref_y_read = ref_y_read;
	if (llc_dual_core_ref_read_buf_enabled)
		ref_y_read = fp_div(ref_y_read, FP_INT(2));

	llc_ref_y_read -= ref_y_read;

	ref_cb_cr_read = fp_mult(ref_cb_cr_bw_factor, dpb_bw_for_1x) / 2;

	ref_cb_cr_read = fp_div(ref_cb_cr_read, dpb_compression_factor);

	ref_cb_cr_read =
		b_frames_enabled ? fp_mult(ref_cb_cr_read, FP_INT(2)) :
					ref_cb_cr_read;

	llc_ref_cb_cr_read = ref_cb_cr_read;

	if (llc_ref_chroma_cache_enabled)
		ref_cb_cr_read = fp_div(ref_cb_cr_read, ref_cb_cr_bw_factor);

	if (llc_dual_core_ref_read_buf_enabled)
		ref_cb_cr_read = fp_div(ref_cb_cr_read, FP_INT(2));

	llc_ref_cb_cr_read -= ref_cb_cr_read;

	ddr.dpb_write = fp_mult(dpb_write_factor, dpb_bw_for_1x);

	ddr.dpb_write = fp_mult(ddr.dpb_write, FP(1, 50, 100));

	ddr.dpb_write = fp_div(ddr.dpb_write, dpb_compression_factor);

	ref_overlap_bw_factor =
		width <= vertical_tile_width ? FP_INT(0) : FP_INT(1);

	ref_overlap_bw = fp_mult(ddr.dpb_write, ref_overlap_bw_factor);

	ref_overlap_bw = fp_div(ref_overlap_bw, dpb_write_factor);

	ref_overlap_bw = fp_mult(ref_overlap_bw,
		(dpb_write_factor - FP_INT(1)));

	ddr.dpb_read = ref_y_read + ref_cb_cr_read + ref_overlap_bw;

	llc.dpb_read = llc_ref_y_read + llc_ref_cb_cr_read;

	ddr.total = ddr.vsp_read + ddr.vsp_write +
		ddr.collocated_read + ddr.collocated_write +
		ddr.line_buffer_read + ddr.line_buffer_write +
		ddr.original_read + ddr.original_write +
		ddr.dpb_read + ddr.dpb_write;

	llc.total = llc.dpb_read + llc.line_buffer + ddr.total;

	qsmmu_bw_overhead_factor = FP(1, 3, 100);
	ddr.total = fp_mult(ddr.total, qsmmu_bw_overhead_factor);

	if (debug) {
		struct dump dump[] = {
		{"ENCODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"width", "%d", width},
		{"height", "%d", height},
		{"DPB format", "%#x", dpb_color_format},
		{"original frame format", "%#x", original_color_format},
		{"fps", "%d", fps},
		{"DPB compression enable", "%d", dpb_compression_enabled},
		{"original compression enable", "%d",
			original_compression_enabled},
		{"low power mode", "%d", low_power},
		{"Work Mode", "%d", work_mode_1},
		{"DPB compression factor", DUMP_FP_FMT,
			dpb_compression_factor},
		{"original compression factor", DUMP_FP_FMT,
			original_compression_factor},
		{"rotation", "%d", rotation},
		{"cropping or scaling", "%d", cropping_or_scaling},

		{"DERIVED PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"LCU size", "%d", lcu_size},
		{"bitrate (Mbit/sec)", "%lu", bitrate},
		{"bins to bit factor", DUMP_FP_FMT, bins_to_bit_factor},
		{"qsmmu_bw_overhead_factor",
			 DUMP_FP_FMT, qsmmu_bw_overhead_factor},

		{"INTERMEDIATE B/W DDR", "", DUMP_HEADER_MAGIC},
		{"ref_y_read", DUMP_FP_FMT, ref_y_read},
		{"ref_cb_cr_read", DUMP_FP_FMT, ref_cb_cr_read},
		{"ref_overlap_bw", DUMP_FP_FMT, ref_overlap_bw},
		{"VSP read", DUMP_FP_FMT, ddr.vsp_read},
		{"VSP write", DUMP_FP_FMT, ddr.vsp_write},
		{"collocated read", DUMP_FP_FMT, ddr.collocated_read},
		{"collocated write", DUMP_FP_FMT, ddr.collocated_write},
		{"line buffer read", DUMP_FP_FMT, ddr.line_buffer_read},
		{"line buffer write", DUMP_FP_FMT, ddr.line_buffer_write},
		{"original read", DUMP_FP_FMT, ddr.original_read},
		{"original write", DUMP_FP_FMT, ddr.original_write},
		{"DPB read", DUMP_FP_FMT, ddr.dpb_read},
		{"DPB write", DUMP_FP_FMT, ddr.dpb_write},
		{"LLC DPB read", DUMP_FP_FMT, llc.dpb_read},
		{"LLC Line buffer", DUMP_FP_FMT, llc.line_buffer},
		};
		__dump(dump, ARRAY_SIZE(dump));
	}

	switch (gm) {
	case GOVERNOR_DDR:
		ret = kbps(fp_round(ddr.total));
		break;
	case GOVERNOR_LLCC:
		ret = kbps(fp_round(llc.total));
		break;
	default:
		dprintk(VIDC_ERR, "%s - Unknown governor\n", __func__);
	}

	return ret;
}

static unsigned long __calculate(struct vidc_bus_vote_data *d,
		enum governor_mode gm)
{
	unsigned long (*calc[])(struct vidc_bus_vote_data *,
			enum governor_mode) = {
		[HAL_VIDEO_DOMAIN_VPE] = __calculate_vpe,
		[HAL_VIDEO_DOMAIN_ENCODER] = __calculate_encoder,
		[HAL_VIDEO_DOMAIN_DECODER] = __calculate_decoder,
	};

	if (d->domain >= ARRAY_SIZE(calc)) {
		dprintk(VIDC_ERR, "%s: invalid domain %d\n",
			__func__, d->domain);
		return 0;
	}
	return calc[d->domain](d, gm);
}


static int __get_target_freq(struct devfreq *dev, unsigned long *freq)
{
	unsigned long ab_kbps = 0, c = 0;
	struct devfreq_dev_status stats = {0};
	struct msm_vidc_gov_data *vidc_data = NULL;
	struct governor *gov = NULL;

	if (!dev || !freq)
		return -EINVAL;

	gov = container_of(dev->governor,
			struct governor, devfreq_gov);
	dev->profile->get_dev_status(dev->dev.parent, &stats);
	vidc_data = (struct msm_vidc_gov_data *)stats.private_data;

	if (!vidc_data || !vidc_data->data_count)
		goto exit;

	for (c = 0; c < vidc_data->data_count; ++c) {
		if (vidc_data->data->power_mode == VIDC_POWER_TURBO) {
			ab_kbps = INT_MAX;
			goto exit;
		}
	}

	for (c = 0; c < vidc_data->data_count; ++c)
		ab_kbps += __calculate(&vidc_data->data[c], gov->mode);

exit:
	*freq = clamp(ab_kbps, dev->min_freq, dev->max_freq ?: UINT_MAX);
	trace_msm_vidc_perf_bus_vote(gov->devfreq_gov.name, *freq);
	return 0;
}

static int __event_handler(struct devfreq *devfreq, unsigned int event,
		void *data)
{
	int rc = 0;

	if (!devfreq)
		return -EINVAL;

	switch (event) {
	case DEVFREQ_GOV_START:
		mutex_lock(&devfreq->lock);
		rc = update_devfreq(devfreq);
		mutex_unlock(&devfreq->lock);
		break;
	}

	return rc;
}

static struct governor governors[] = {
	{
		.mode = GOVERNOR_DDR,
		.devfreq_gov = {
			.name = "vidc-ar50-ddr",
			.get_target_freq = __get_target_freq,
			.event_handler = __event_handler,
		},
	},
	{
		.mode = GOVERNOR_LLCC,
		.devfreq_gov = {
			.name = "vidc-ar50-llcc",
			.get_target_freq = __get_target_freq,
			.event_handler = __event_handler,
		},
	},
};

static int __init msm_vidc_ar50_bw_gov_init(void)
{
	int c = 0, rc = 0;

	for (c = 0; c < ARRAY_SIZE(governors); ++c) {
		dprintk(VIDC_DBG, "Adding governor %s\n",
				governors[c].devfreq_gov.name);

		rc = devfreq_add_governor(&governors[c].devfreq_gov);
		if (rc) {
			dprintk(VIDC_ERR, "Error adding governor %s: %d\n",
				governors[c].devfreq_gov.name, rc);
			break;
		}
	}

	return rc;
}
module_init(msm_vidc_ar50_bw_gov_init);

static void __exit msm_vidc_ar50_bw_gov_exit(void)
{
	int c = 0;

	for (c = 0; c < ARRAY_SIZE(governors); ++c) {
		dprintk(VIDC_DBG, "Removing governor %s\n",
				governors[c].devfreq_gov.name);
		devfreq_remove_governor(&governors[c].devfreq_gov);
	}
}
module_exit(msm_vidc_ar50_bw_gov_exit);
MODULE_LICENSE("GPL v2");

/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

static bool debug;
module_param(debug, bool, 0644);

enum governor_mode {
	GOVERNOR_DDR,
	GOVERNOR_VMEM,
	GOVERNOR_VMEM_PLUS,
};

struct governor {
	enum governor_mode mode;
	struct devfreq_governor devfreq_gov;
};

enum scenario {
	SCENARIO_WORST,
	SCENARIO_SUSTAINED_WORST,
	SCENARIO_AVERAGE,
	SCENARIO_MAX,
};

/*
 * Minimum dimensions that the governor is willing to calculate
 * bandwidth for.  This means that anything bandwidth(0, 0) ==
 * bandwidth(BASELINE_DIMENSIONS.width, BASELINE_DIMENSIONS.height)
 */
const struct {
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
const unsigned long NOMINAL_BW_MBPS = 6000 /* ideally 320 Mhz */,
	SVS_BW_MBPS = 2000 /* ideally 100 Mhz */;

/* converts Mbps to bps (the "b" part can be bits or bytes based on context) */
#define kbps(__mbps) ((__mbps) * 1000)
#define bps(__mbps) (kbps(__mbps) * 1000)

#define GENERATE_SCENARIO_PROFILE(__average, __worst) {                        \
	[SCENARIO_AVERAGE] = (__average),                                      \
	[SCENARIO_WORST] =  (__worst),                                         \
	[SCENARIO_SUSTAINED_WORST] = (__worst),                                \
}

#define GENERATE_COMPRESSION_PROFILE(__bpp, __average, __worst) {              \
	.bpp = __bpp,                                                          \
	.ratio = GENERATE_SCENARIO_PROFILE(__average, __worst),                \
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
#define COMPRESSION_RATIO_MAX 2
static struct lut {
	int frame_size; /* width x height */
	unsigned long bitrate[SCENARIO_MAX];
	struct {
		int bpp;
		fp_t ratio[SCENARIO_MAX];
	} compression_ratio[COMPRESSION_RATIO_MAX];
} const LUT[] = {
	{
		.frame_size = 1280 * 720,
		.bitrate = GENERATE_SCENARIO_PROFILE(7, 14),
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 69, 100),
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 49, 100),
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 1920 * 1088,
		.bitrate = GENERATE_SCENARIO_PROFILE(20, 40),
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(1, 69, 100),
					FP(1, 28, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 49, 100),
					FP(1, 23, 100)),
		}
	},
	{
		.frame_size = 2560 * 1440,
		.bitrate = GENERATE_SCENARIO_PROFILE(32, 64),
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(2, 20, 100),
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 97, 100),
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 3840 * 2160,
		.bitrate = GENERATE_SCENARIO_PROFILE(42, 84),
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(2, 20, 100),
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 97, 100),
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2160,
		.bitrate = GENERATE_SCENARIO_PROFILE(44, 88),
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(2, 20, 100),
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 97, 100),
					FP(1, 22, 100)),
		}
	},
	{
		.frame_size = 4096 * 2304,
		.bitrate = GENERATE_SCENARIO_PROFILE(48, 96),
		.compression_ratio = {
			GENERATE_COMPRESSION_PROFILE(8,
					FP(2, 20, 100),
					FP(1, 26, 100)),
			GENERATE_COMPRESSION_PROFILE(10,
					FP(1, 97, 100),
					FP(1, 22, 100)),
		}
	},
};

static struct lut const *__lut(int width, int height)
{
	int frame_size = height * width, c = 0;

	do {
		if (LUT[c].frame_size >= frame_size)
			return &LUT[c];
	} while (++c < ARRAY_SIZE(LUT));

	return &LUT[ARRAY_SIZE(LUT) - 1];
}

static fp_t __compression_ratio(struct lut const *entry, int bpp,
		enum scenario s)
{
	int c = 0;

	for (c = 0; c < COMPRESSION_RATIO_MAX; ++c) {
		if (entry->compression_ratio[c].bpp == bpp)
			return entry->compression_ratio[c].ratio[s];
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
		return 10;
	default:
		dprintk(VIDC_ERR,
				"What's this?  We don't support this colorformat (%x)",
				f);
		return INT_MAX;
	}
}

static unsigned long __calculate_vmem_plus_ab(struct vidc_bus_vote_data *d)
{
	unsigned long i = 0, vmem_plus = 0;

	if (!d->imem_ab_tbl || !d->imem_ab_tbl_size) {
		vmem_plus = 1; /* Vote for the min ab value */
		goto exit;
	}

	/* Pick up vmem frequency based on venus core frequency */
	for (i = 0; i < d->imem_ab_tbl_size; i++) {
		if (d->imem_ab_tbl[i].core_freq == d->core_freq) {
			vmem_plus = d->imem_ab_tbl[i].imem_ab;
			break;
		}
	}

	/* Incase we get an unsupported freq throw a warning
	 * and set ab to the minimum value. */
	if (!vmem_plus) {
		vmem_plus = 1;
		dprintk(VIDC_WARN,
			"could not calculate vmem ab value due to core freq mismatch\n");
		WARN_ON(1);
	}

exit:
	return vmem_plus;
}

static unsigned long __calculate_decoder(struct vidc_bus_vote_data *d,
		enum governor_mode gm) {
	/*
	 * XXX: Don't fool around with any of the hardcoded numbers unless you
	 * know /exactly/ what you're doing.  Many of these numbers are
	 * measured heuristics and hardcoded numbers taken from the firmware.
	 */
	/* Decoder parameters */
	enum scenario scenario;
	int width, height, lcu_size, dpb_bpp, opb_bpp, fps;
	bool unified_dpb_opb, dpb_compression_enabled, opb_compression_enabled;
	fp_t dpb_opb_scaling_ratio, dpb_compression_factor,
		opb_compression_factor, qsmmu_bw_overhead_factor;
	int vmem_size; /* in kB */

	/* Derived parameters */
	int lcu_per_frame, tnbr_per_lcu_10bpc, tnbr_per_lcu_8bpc, tnbr_per_lcu,
		colocated_bytes_per_lcu, vmem_line_buffer, vmem_chroma_cache,
		vmem_luma_cache, vmem_chroma_luma_cache;
	unsigned long bitrate;
	fp_t bins_to_bit_factor, dpb_write_factor, ten_bpc_packing_factor,
		ten_bpc_bpp_factor, vsp_read_factor, vsp_write_factor,
		ocmem_usage_lcu_factor, ref_ocmem_bw_factor_read,
		ref_ocmem_bw_factor_write, bw_for_1x_8bpc, dpb_bw_for_1x,
		motion_vector_complexity, row_cache_penalty, opb_bw;

	/* Output parameters */
	struct {
		fp_t vsp_read, vsp_write, collocated_read, collocated_write,
			line_buffer_read, line_buffer_write, recon_read,
			recon_write, opb_read, opb_write, dpb_read, dpb_write,
			total;
	} ddr, vmem;

	unsigned long ret = 0;

	/* Decoder parameters setup */
	scenario = SCENARIO_WORST;

	width = max(d->width, BASELINE_DIMENSIONS.width);
	height = max(d->height, BASELINE_DIMENSIONS.height);

	lcu_size = 32;

	dpb_bpp = d->num_formats >= 1 ? __bpp(d->color_formats[0]) : INT_MAX;
	opb_bpp = d->num_formats >= 2 ?  __bpp(d->color_formats[1]) : dpb_bpp;

	fps = d->fps;

	unified_dpb_opb = d->num_formats == 1;

	dpb_opb_scaling_ratio = FP_ONE;

	dpb_compression_enabled = d->num_formats >= 1 &&
		__ubwc(d->color_formats[0]);
	opb_compression_enabled = d->num_formats >= 2 &&
		__ubwc(d->color_formats[1]);

	dpb_compression_factor = !dpb_compression_enabled ? FP_ONE :
		__compression_ratio(__lut(width, height), dpb_bpp, scenario);

	opb_compression_factor = !opb_compression_enabled ? FP_ONE :
		__compression_ratio(__lut(width, height), opb_bpp, scenario);

	vmem_size = 512; /* in kB */

	/* Derived parameters setup */
	lcu_per_frame = DIV_ROUND_UP(width, lcu_size) *
		DIV_ROUND_UP(height, lcu_size);

	bitrate = __lut(width, height)->bitrate[scenario];

	bins_to_bit_factor = FP(1, 60, 100);

	dpb_write_factor = scenario == SCENARIO_AVERAGE ?
		FP_ONE : FP(1, 5, 100);

	ten_bpc_packing_factor = FP(1, 67, 1000);
	ten_bpc_bpp_factor = FP(1, 1, 4);

	vsp_read_factor = bins_to_bit_factor + FP_INT(2);
	vsp_write_factor = bins_to_bit_factor;

	tnbr_per_lcu_10bpc = lcu_size == 16 ? 384 + 192 :
				lcu_size == 32 ? 640 + 256 :
						1280 + 384;
	tnbr_per_lcu_8bpc = lcu_size == 16 ? 256 + 192 :
				lcu_size == 32 ? 512 + 256 :
						1024 + 384;
	tnbr_per_lcu = dpb_bpp == 10 ? tnbr_per_lcu_10bpc : tnbr_per_lcu_8bpc;

	colocated_bytes_per_lcu = lcu_size == 16 ? 16 :
				lcu_size == 32 ? 64 : 256;

	ocmem_usage_lcu_factor = lcu_size == 16 ? FP(1, 8, 10) :
				lcu_size == 32 ? FP(1, 2, 10) :
						FP_ONE;
	ref_ocmem_bw_factor_read = vmem_size < 296 ? FP_ZERO :
				vmem_size < 648 ? FP(0, 1, 4) :
						FP(0, 55, 100);
	ref_ocmem_bw_factor_write = vmem_size < 296 ? FP_ZERO :
				vmem_size < 648 ? FP(0, 7, 10) :
						FP(1, 4, 10);

	/* Prelim b/w calculation */
	bw_for_1x_8bpc = fp_mult(FP_INT(width * height * fps),
			fp_mult(FP(1, 50, 100), dpb_write_factor));
	bw_for_1x_8bpc = fp_div(bw_for_1x_8bpc, FP_INT(bps(1)));

	dpb_bw_for_1x = dpb_bpp == 8 ? bw_for_1x_8bpc :
		fp_mult(bw_for_1x_8bpc, fp_mult(ten_bpc_packing_factor,
					ten_bpc_bpp_factor));
	/* VMEM adjustments */
	vmem_line_buffer = tnbr_per_lcu * DIV_ROUND_UP(width, lcu_size) / 1024;
	vmem_chroma_cache = dpb_bpp == 10 ? 176 : 128;
	vmem_luma_cache = dpb_bpp == 10 ? 353 : 256;
	vmem_chroma_luma_cache = vmem_chroma_cache + vmem_luma_cache;

	motion_vector_complexity = scenario == SCENARIO_AVERAGE ?
		FP(2, 66, 100) : FP_INT(4);

	row_cache_penalty = FP_ZERO;
	if (vmem_size < vmem_line_buffer + vmem_chroma_cache)
		row_cache_penalty = fp_mult(FP(0, 5, 100),
				motion_vector_complexity);
	else if (vmem_size < vmem_line_buffer + vmem_luma_cache)
		row_cache_penalty = fp_mult(FP(0, 7, 100),
				motion_vector_complexity);
	else if (vmem_size < vmem_line_buffer + vmem_chroma_cache
			+ vmem_luma_cache)
		row_cache_penalty = fp_mult(FP(0, 3, 100),
				motion_vector_complexity);
	else
		row_cache_penalty = FP_ZERO;


	opb_bw = unified_dpb_opb ? FP_ZERO :
		fp_div(fp_div(bw_for_1x_8bpc, dpb_opb_scaling_ratio),
				opb_compression_factor);

	/* B/W breakdown on a per buffer type basis for VMEM */
	vmem.vsp_read = FP_ZERO;
	vmem.vsp_write = FP_ZERO;

	vmem.collocated_read = FP_ZERO;
	vmem.collocated_write = FP_ZERO;

	vmem.line_buffer_read = FP_INT(tnbr_per_lcu *
			lcu_per_frame * fps / bps(1));
	vmem.line_buffer_write = vmem.line_buffer_read;

	vmem.recon_read = FP_ZERO;
	vmem.recon_write = FP_ZERO;

	vmem.opb_read = FP_ZERO;
	vmem.opb_write = FP_ZERO;

	vmem.dpb_read = fp_mult(ocmem_usage_lcu_factor, fp_mult(
					ref_ocmem_bw_factor_read,
					dpb_bw_for_1x));
	vmem.dpb_write = fp_mult(ocmem_usage_lcu_factor, fp_mult(
					ref_ocmem_bw_factor_write,
					dpb_bw_for_1x));

	vmem.total = vmem.vsp_read + vmem.vsp_write +
		vmem.collocated_read + vmem.collocated_write +
		vmem.line_buffer_read + vmem.line_buffer_write +
		vmem.recon_read + vmem.recon_write +
		vmem.opb_read + vmem.opb_write +
		vmem.dpb_read + vmem.dpb_write;

	/*
	 * Attempt to force VMEM to a certain frequency for 4K
	 */
	if (width * height * fps >= 3840 * 2160 * 60)
		vmem.total = FP_INT(NOMINAL_BW_MBPS);
	else if (width * height * fps >= 3840 * 2160 * 30)
		vmem.total = FP_INT(SVS_BW_MBPS);

	/* ........................................ for DDR */
	ddr.vsp_read = fp_div(fp_mult(FP_INT(bitrate),
				vsp_read_factor), FP_INT(8));
	ddr.vsp_write = fp_div(fp_mult(FP_INT(bitrate),
				vsp_write_factor), FP_INT(8));

	ddr.collocated_read = FP_INT(lcu_per_frame *
			colocated_bytes_per_lcu * fps / bps(1));
	ddr.collocated_write = FP_INT(lcu_per_frame *
			colocated_bytes_per_lcu * fps / bps(1));

	ddr.line_buffer_read = vmem_size ? FP_ZERO : vmem.line_buffer_read;
	ddr.line_buffer_write = vmem_size ? FP_ZERO : vmem.line_buffer_write;

	ddr.recon_read = FP_ZERO;
	ddr.recon_write = fp_div(dpb_bw_for_1x, dpb_compression_factor);

	ddr.opb_read = FP_ZERO;
	ddr.opb_write = opb_bw;

	ddr.dpb_read = fp_div(fp_mult(dpb_bw_for_1x,
				motion_vector_complexity + row_cache_penalty),
			dpb_compression_factor);
	ddr.dpb_write = FP_ZERO;

	ddr.total = ddr.vsp_read + ddr.vsp_write +
		ddr.collocated_read + ddr.collocated_write +
		ddr.line_buffer_read + ddr.line_buffer_write +
		ddr.recon_read + ddr.recon_write +
		ddr.opb_read + ddr.opb_write +
		ddr.dpb_read + ddr.dpb_write;

	qsmmu_bw_overhead_factor = FP(1, 3, 100);
	ddr.total = fp_mult(ddr.total, qsmmu_bw_overhead_factor);

	/* Dump all the variables for easier debugging */
	if (debug) {
		struct dump dump[] = {
		{"DECODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"content", "%d", scenario},
		{"LCU size", "%d", lcu_size},
		{"DPB bitdepth", "%d", dpb_bpp},
		{"frame rate", "%d", fps},
		{"DPB/OPB unified", "%d", unified_dpb_opb},
		{"DPB/OPB downscaling ratio", DUMP_FP_FMT,
			dpb_opb_scaling_ratio},
		{"DPB compression", "%d", dpb_compression_enabled},
		{"OPB compression", "%d", opb_compression_enabled},
		{"DPB compression factor", DUMP_FP_FMT,
			dpb_compression_factor},
		{"OPB compression factor", DUMP_FP_FMT,
			opb_compression_factor},
		{"VMEM size", "%dkB", vmem_size},
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
		{"TNBR/LCU_10bpc", "%d", tnbr_per_lcu_10bpc},
		{"TNBR/LCU_8bpc", "%d", tnbr_per_lcu_8bpc},
		{"TNBR/LCU", "%d", tnbr_per_lcu},
		{"colocated bytes/LCU", "%d", colocated_bytes_per_lcu},
		{"OCMEM usage LCU factor", DUMP_FP_FMT,
			ocmem_usage_lcu_factor},
		{"ref OCMEM b/w factor (read)", DUMP_FP_FMT,
			ref_ocmem_bw_factor_read},
		{"ref OCMEM b/w factor (write)", DUMP_FP_FMT,
			ref_ocmem_bw_factor_write},
		{"B/W for 1x (NV12 8bpc)", DUMP_FP_FMT, bw_for_1x_8bpc},
		{"DPB B/W For 1x (NV12)", DUMP_FP_FMT, dpb_bw_for_1x},

		{"VMEM", "", DUMP_HEADER_MAGIC},
		{"line buffer", "%d", vmem_line_buffer},
		{"chroma cache", "%d", vmem_chroma_cache},
		{"luma cache", "%d", vmem_luma_cache},
		{"luma & chroma cache", "%d", vmem_chroma_luma_cache},

		{"DERIVED PARAMETERS (2)", "", DUMP_HEADER_MAGIC},
		{"MV complexity", DUMP_FP_FMT, motion_vector_complexity},
		{"row cache penalty", DUMP_FP_FMT, row_cache_penalty},
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

		{"INTERMEDIATE VMEM B/W", "", DUMP_HEADER_MAGIC},
		{"VSP read", "%d", vmem.vsp_read},
		{"VSP write", DUMP_FP_FMT, vmem.vsp_write},
		{"collocated read", DUMP_FP_FMT, vmem.collocated_read},
		{"collocated write", DUMP_FP_FMT, vmem.collocated_write},
		{"line buffer read", DUMP_FP_FMT, vmem.line_buffer_read},
		{"line buffer write", DUMP_FP_FMT, vmem.line_buffer_write},
		{"recon read", DUMP_FP_FMT, vmem.recon_read},
		{"recon write", DUMP_FP_FMT, vmem.recon_write},
		{"OPB read", DUMP_FP_FMT, vmem.opb_read},
		{"OPB write", DUMP_FP_FMT, vmem.opb_write},
		{"DPB read", DUMP_FP_FMT, vmem.dpb_read},
		{"DPB write", DUMP_FP_FMT, vmem.dpb_write},
		};
		__dump(dump, ARRAY_SIZE(dump));
	}

	switch (gm) {
	case GOVERNOR_DDR:
		ret = kbps(fp_round(ddr.total));
		break;
	case GOVERNOR_VMEM:
		ret = kbps(fp_round(vmem.total));
		break;
	case GOVERNOR_VMEM_PLUS:
		ret = __calculate_vmem_plus_ab(d);
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
	enum scenario scenario, bitrate_scenario;
	enum hal_video_codec standard;
	int width, height, fps, vmem_size;
	enum hal_uncompressed_format dpb_color_format;
	enum hal_uncompressed_format original_color_format;
	bool dpb_compression_enabled, original_compression_enabled,
		two_stage_encoding, low_power, rotation, cropping_or_scaling;
	fp_t dpb_compression_factor, original_compression_factor,
		qsmmu_bw_overhead_factor;
	bool b_frames_enabled;

	/* Derived Parameters */
	int lcu_size;
	enum gop {
		GOP_IBBP,
		GOP_IPPP,
	} gop;
	unsigned long bitrate;
	fp_t bins_to_bit_factor, chroma_luma_factor_dpb, one_frame_bw_dpb,
		 chroma_luma_factor_original, one_frame_bw_original,
		 line_buffer_size_per_lcu, line_buffer_size, line_buffer_bw,
		 original_vmem_requirement, bw_increase_p, bw_increase_b;
	int collocated_mv_per_lcu, max_transaction_size,
		search_window_size_vertical_p, search_window_factor_p,
		search_window_factor_bw_p, vmem_size_p, available_vmem_p,
		search_window_size_vertical_b, search_window_factor_b,
		search_window_factor_bw_b, vmem_size_b, available_vmem_b;

	/* Output paramaters */
	struct {
		fp_t vsp_read, vsp_write, collocated_read, collocated_write,
			line_buffer_read, line_buffer_write, original_read,
			original_write, dpb_read, dpb_write, total;
	} ddr, vmem;

	unsigned long ret = 0;

	/* Encoder Parameters setup */
	scenario = SCENARIO_WORST;

	standard = d->codec;
	width = max(d->width, BASELINE_DIMENSIONS.width);
	height = max(d->height, BASELINE_DIMENSIONS.height);

	dpb_color_format = HAL_COLOR_FORMAT_NV12_UBWC;
	original_color_format = d->num_formats >= 1 ?
		d->color_formats[0] : HAL_UNUSED_COLOR;

	fps = d->fps;
	bitrate_scenario = SCENARIO_WORST;

	dpb_compression_enabled = __ubwc(dpb_color_format);
	original_compression_enabled = __ubwc(original_color_format);

	two_stage_encoding = false;
	low_power = d->power_mode == VIDC_POWER_LOW;
	b_frames_enabled = false;

	dpb_compression_factor = !dpb_compression_enabled ? FP_ONE :
		__compression_ratio(__lut(width, height),
				__bpp(dpb_color_format), scenario);
	original_compression_factor = !original_compression_enabled ? FP_ONE :
		__compression_ratio(__lut(width, height),
				__bpp(original_color_format), scenario);

	rotation = false;
	cropping_or_scaling = false;
	vmem_size = 512; /* in kB */

	/* Derived Parameters */
	lcu_size = 16;
	gop = b_frames_enabled ? GOP_IBBP : GOP_IPPP;
	bitrate = __lut(width, height)->bitrate[bitrate_scenario];
	bins_to_bit_factor = FP(1, 6, 10);

	/*
	 * FIXME: Minor color format related hack: a lot of the derived params
	 * depend on the YUV bitdepth as a variable.  However, we don't have
	 * appropriate enums defined yet (hence no support).  As a result omit
	 * a lot of the checks (which should look like the snippet below) in
	 * favour of hardcoding.
	 *      dpb_color_format == YUV420 ? 0.5 :
	 *      dpb_color_format == YUV422 ? 1.0 : 2.0
	 * Similar hacks are annotated inline in code with the string "CF hack"
	 * for documentation purposes.
	 */
	chroma_luma_factor_dpb = FP(0, 1, 2);
	one_frame_bw_dpb = fp_mult(FP_ONE + chroma_luma_factor_dpb,
			fp_div(FP_INT(width * height * fps),
				FP_INT(1000 * 1000)));

	chroma_luma_factor_original = FP(0, 1, 2); /* XXX: CF hack */
	one_frame_bw_original = fp_mult(FP_ONE + chroma_luma_factor_original,
			fp_div(FP_INT(width * height * fps),
				FP_INT(1000 * 1000)));

	line_buffer_size_per_lcu = FP_ZERO;
	if (lcu_size == 16)
		line_buffer_size_per_lcu = FP_INT(128) + fp_mult(FP_INT(256),
					FP_ONE /*XXX: CF hack */);
	else
		line_buffer_size_per_lcu = FP_INT(192) + fp_mult(FP_INT(512),
					FP_ONE /*XXX: CF hack */);

	line_buffer_size = fp_div(
			fp_mult(FP_INT(width / lcu_size),
				line_buffer_size_per_lcu),
			FP_INT(1024));
	line_buffer_bw = fp_mult(line_buffer_size,
			fp_div(FP_INT((height / lcu_size /
				(two_stage_encoding ? 2 : 1) - 1) * fps),
				FP_INT(1000)));

	collocated_mv_per_lcu = lcu_size == 16 ? 16 : 64;
	max_transaction_size = 256;

	original_vmem_requirement = FP_INT(3 *
			(two_stage_encoding ? 2 : 1) * lcu_size);
	original_vmem_requirement = fp_mult(original_vmem_requirement,
			(FP_ONE + chroma_luma_factor_original));
	original_vmem_requirement += FP_INT((cropping_or_scaling ? 3 : 0) * 2);
	original_vmem_requirement = fp_mult(original_vmem_requirement,
			FP_INT(max_transaction_size));
	original_vmem_requirement = fp_div(original_vmem_requirement,
			FP_INT(1024));

	search_window_size_vertical_p = low_power ? 32 :
					b_frames_enabled ? 80 :
					width > 2048 ? 64 : 48;
	search_window_factor_p = search_window_size_vertical_p * 2 / lcu_size;
	search_window_factor_bw_p = !two_stage_encoding ?
		search_window_size_vertical_p * 2 / lcu_size + 1 :
		(search_window_size_vertical_p * 2 / lcu_size + 2) / 2;
	vmem_size_p = (search_window_factor_p * width + 128 * 2) *
		lcu_size / 2 / 1024; /* XXX: CF hack */
	bw_increase_p = fp_mult(one_frame_bw_dpb,
			FP_INT(search_window_factor_bw_p - 1) / 3);
	available_vmem_p = min_t(int, 3, (vmem_size - fp_int(line_buffer_size) -
			fp_int(original_vmem_requirement)) / vmem_size_p);

	search_window_size_vertical_b = 48;
	search_window_factor_b = search_window_size_vertical_b * 2 / lcu_size;
	search_window_factor_bw_b = !two_stage_encoding ?
		search_window_size_vertical_b * 2 / lcu_size + 1 :
		(search_window_size_vertical_b * 2 / lcu_size + 2) / 2;
	vmem_size_b = (search_window_factor_b * width + 128 * 2) * lcu_size /
		2 / 1024;
	bw_increase_b = fp_mult(one_frame_bw_dpb,
			FP_INT((search_window_factor_bw_b - 1) / 3));
	available_vmem_b = min_t(int, 6, (vmem_size - fp_int(line_buffer_size) -
			fp_int(original_vmem_requirement)) / vmem_size_b);

	/* Output parameters for DDR */
	ddr.vsp_read = fp_mult(fp_div(FP_INT(bitrate), FP_INT(8)),
			bins_to_bit_factor);
	ddr.vsp_write = ddr.vsp_read + fp_div(FP_INT(bitrate), FP_INT(8));

	ddr.collocated_read = fp_div(FP_INT(DIV_ROUND_UP(width, lcu_size) *
			DIV_ROUND_UP(height, lcu_size) *
			collocated_mv_per_lcu * fps), FP_INT(1000 * 1000));
	ddr.collocated_write = ddr.collocated_read;

	ddr.line_buffer_read = (FP_INT(vmem_size) >= line_buffer_size +
		original_vmem_requirement) ? FP_ZERO : line_buffer_bw;
	ddr.line_buffer_write = ddr.line_buffer_read;

	ddr.original_read = fp_div(one_frame_bw_original,
			original_compression_factor);
	ddr.original_write = FP_ZERO;

	ddr.dpb_read = FP_ZERO;
	if (gop == GOP_IPPP) {
		ddr.dpb_read = one_frame_bw_dpb + fp_mult(bw_increase_p,
			FP_INT(3 - available_vmem_p));
	} else if (scenario == SCENARIO_WORST ||
			scenario == SCENARIO_SUSTAINED_WORST) {
		ddr.dpb_read = fp_mult(one_frame_bw_dpb, FP_INT(2));
		ddr.dpb_read += fp_mult(FP_INT(6 - available_vmem_b),
				bw_increase_b);
	} else {
		fp_t part_p, part_b;

		part_p = one_frame_bw_dpb + fp_mult(bw_increase_p,
				FP_INT(3 - available_vmem_p));
		part_p = fp_div(part_p, FP_INT(3));

		part_b = fp_mult(one_frame_bw_dpb, 2) +
			fp_mult(FP_INT(6 - available_vmem_b), bw_increase_b);
		part_b = fp_mult(part_b, FP(0, 2, 3));

		ddr.dpb_read = part_p + part_b;
	}

	ddr.dpb_read = fp_div(ddr.dpb_read, dpb_compression_factor);
	ddr.dpb_write = fp_div(one_frame_bw_dpb, dpb_compression_factor);

	ddr.total = ddr.vsp_read + ddr.vsp_write +
		ddr.collocated_read + ddr.collocated_write +
		ddr.line_buffer_read + ddr.line_buffer_write +
		ddr.original_read + ddr.original_write +
		ddr.dpb_read + ddr.dpb_write;

	qsmmu_bw_overhead_factor = FP(1, 3, 100);
	ddr.total = fp_mult(ddr.total, qsmmu_bw_overhead_factor);

	/* ................. for VMEM */
	vmem.vsp_read = FP_ZERO;
	vmem.vsp_write = FP_ZERO;

	vmem.collocated_read = FP_ZERO;
	vmem.collocated_write = FP_ZERO;

	vmem.line_buffer_read = line_buffer_bw - ddr.line_buffer_read;
	vmem.line_buffer_write = vmem.line_buffer_read;

	vmem.original_read = FP_INT(vmem_size) >= original_vmem_requirement ?
		ddr.original_read : FP_ZERO;
	vmem.original_write = vmem.original_read;

	vmem.dpb_read = FP_ZERO;
	if (gop == GOP_IPPP) {
		fp_t temp = fp_mult(one_frame_bw_dpb,
			FP_INT(search_window_factor_bw_p * available_vmem_p));
		temp = fp_div(temp, FP_INT(3));

		vmem.dpb_read = temp;
	} else if (scenario != SCENARIO_AVERAGE) {
		fp_t temp = fp_mult(one_frame_bw_dpb, FP_INT(2));

		temp = fp_mult(temp, FP_INT(search_window_factor_bw_b *
					available_vmem_b));
		temp = fp_div(temp, FP_INT(6));

		vmem.dpb_read = temp;
	} else {
		fp_t part_p, part_b;

		part_p = fp_mult(one_frame_bw_dpb, FP_INT(
					search_window_factor_bw_p *
					available_vmem_p));
		part_p = fp_div(part_p, FP_INT(3 * 3));

		part_b = fp_mult(one_frame_bw_dpb, FP_INT(2 *
					search_window_factor_bw_b *
					available_vmem_b));
		part_b = fp_div(part_b, FP_INT(6));
		part_b = fp_mult(part_b, FP(0, 2, 3));

		vmem.dpb_read = part_p + part_b;
	}

	vmem.dpb_write = FP_ZERO;
	if (gop == GOP_IPPP) {
		fp_t temp = fp_mult(one_frame_bw_dpb,
				FP_INT(available_vmem_p));
		temp = fp_div(temp, FP_INT(3));

		vmem.dpb_write = temp;
	} else if (scenario != SCENARIO_AVERAGE) {
		fp_t temp = fp_mult(one_frame_bw_dpb,
				FP_INT(2 * available_vmem_b));
		temp = fp_div(temp, FP_INT(6));

		vmem.dpb_write = temp;
	} else {
		fp_t part_b, part_p;

		part_b = fp_mult(one_frame_bw_dpb, FP_INT(available_vmem_p));
		part_b = fp_div(part_b, FP_INT(9));

		part_p = fp_mult(one_frame_bw_dpb, FP_INT(
					2 * available_vmem_b));
		part_p = fp_div(part_p, FP_INT(6));
		part_b = fp_mult(part_b, FP(0, 2, 3));

		vmem.dpb_write = part_p + part_b;
	}

	vmem.total = vmem.vsp_read + vmem.vsp_write +
		vmem.collocated_read + vmem.collocated_write +
		vmem.line_buffer_read + vmem.line_buffer_write +
		vmem.original_read + vmem.original_write +
		vmem.dpb_read + vmem.dpb_write;

	/*
	 * When in low power mode, attempt to force the VMEM clocks a certain
	 * frequency that DCVS would prefer
	 */
	if (width * height >= 3840 * 2160 && low_power)
		vmem.total = FP_INT(NOMINAL_BW_MBPS);

	if (debug) {
		struct dump dump[] = {
		{"ENCODER PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"scenario", "%d", scenario},
		{"standard", "%#x", standard},
		{"width", "%d", width},
		{"height", "%d", height},
		{"DPB format", "%#x", dpb_color_format},
		{"original frame format", "%#x", original_color_format},
		{"fps", "%d", fps},
		{"target bitrate", "%d", bitrate_scenario},
		{"DPB compression enable", "%d", dpb_compression_enabled},
		{"original compression enable", "%d",
			original_compression_enabled},
		{"two stage encoding", "%d", two_stage_encoding},
		{"low power mode", "%d", low_power},
		{"DPB compression factor", DUMP_FP_FMT,
			dpb_compression_factor},
		{"original compression factor", DUMP_FP_FMT,
			original_compression_factor},
		{"rotation", "%d", rotation},
		{"cropping or scaling", "%d", cropping_or_scaling},
		{"VMEM size (KB)", "%d", vmem_size},

		{"DERIVED PARAMETERS", "", DUMP_HEADER_MAGIC},
		{"LCU size", "%d", lcu_size},
		{"GOB pattern", "%d", gop},
		{"bitrate (Mbit/sec)", "%lu", bitrate},
		{"bins to bit factor", DUMP_FP_FMT, bins_to_bit_factor},
		{"B-frames enabled", "%d", b_frames_enabled},
		{"search window size vertical (B)", "%d",
			search_window_size_vertical_b},
		{"search window factor (B)", "%d", search_window_factor_b},
		{"search window factor BW (B)", "%d",
			search_window_factor_bw_b},
		{"VMEM size (B)", "%d", vmem_size_b},
		{"bw increase (MB/s) (B)", DUMP_FP_FMT, bw_increase_b},
		{"available VMEM (B)", "%d", available_vmem_b},
		{"search window size vertical (P)", "%d",
			search_window_size_vertical_p},
		{"search window factor (P)", "%d", search_window_factor_p},
		{"search window factor BW (P)", "%d",
			search_window_factor_bw_p},
		{"VMEM size (P)", "%d", vmem_size_p},
		{"bw increase (MB/s) (P)", DUMP_FP_FMT, bw_increase_p},
		{"available VMEM (P)", "%d", available_vmem_p},
		{"chroma/luma factor DPB", DUMP_FP_FMT,
			chroma_luma_factor_dpb},
		{"one frame BW DPB (MB/s)", DUMP_FP_FMT, one_frame_bw_dpb},
		{"chroma/Luma factor original", DUMP_FP_FMT,
			chroma_luma_factor_original},
		{"one frame BW original (MB/s)", DUMP_FP_FMT,
			one_frame_bw_original},
		{"line buffer size per LCU", DUMP_FP_FMT,
			line_buffer_size_per_lcu},
		{"line buffer size (KB)", DUMP_FP_FMT, line_buffer_size},
		{"line buffer BW (MB/s)", DUMP_FP_FMT, line_buffer_bw},
		{"collocated MVs per LCU", "%d", collocated_mv_per_lcu},
		{"original VMEM requirement (KB)", DUMP_FP_FMT,
			original_vmem_requirement},

		{"INTERMEDIATE B/W DDR", "", DUMP_HEADER_MAGIC},
		{"VSP read", DUMP_FP_FMT, ddr.vsp_read},
		{"VSP read", DUMP_FP_FMT, ddr.vsp_write},
		{"collocated read", DUMP_FP_FMT, ddr.collocated_read},
		{"collocated read", DUMP_FP_FMT, ddr.collocated_write},
		{"line buffer read", DUMP_FP_FMT, ddr.line_buffer_read},
		{"line buffer read", DUMP_FP_FMT, ddr.line_buffer_write},
		{"original read", DUMP_FP_FMT, ddr.original_read},
		{"original read", DUMP_FP_FMT, ddr.original_write},
		{"DPB read", DUMP_FP_FMT, ddr.dpb_read},
		{"DPB write", DUMP_FP_FMT, ddr.dpb_write},

		{"INTERMEDIATE B/W VMEM", "", DUMP_HEADER_MAGIC},
		{"VSP read", DUMP_FP_FMT, vmem.vsp_read},
		{"VSP read", DUMP_FP_FMT, vmem.vsp_write},
		{"collocated read", DUMP_FP_FMT, vmem.collocated_read},
		{"collocated read", DUMP_FP_FMT, vmem.collocated_write},
		{"line buffer read", DUMP_FP_FMT, vmem.line_buffer_read},
		{"line buffer read", DUMP_FP_FMT, vmem.line_buffer_write},
		{"original read", DUMP_FP_FMT, vmem.original_read},
		{"original read", DUMP_FP_FMT, vmem.original_write},
		{"DPB read", DUMP_FP_FMT, vmem.dpb_read},
		{"DPB write", DUMP_FP_FMT, vmem.dpb_write},
		};
		__dump(dump, ARRAY_SIZE(dump));
	}

	switch (gm) {
	case GOVERNOR_DDR:
		ret = kbps(fp_round(ddr.total));
		break;
	case GOVERNOR_VMEM:
		ret = kbps(fp_round(vmem.total));
		break;
	case GOVERNOR_VMEM_PLUS:
		ret = __calculate_vmem_plus_ab(d);
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

	return calc[d->domain](d, gm);
}


static int __get_target_freq(struct devfreq *dev, unsigned long *freq,
		u32 *flag)
{
	unsigned long ab_kbps = 0, c = 0;
	struct devfreq_dev_status stats = {0};
	struct msm_vidc_gov_data *vidc_data = NULL;
	struct governor *gov = NULL;

	if (!dev || !freq || !flag)
		return -EINVAL;

	gov = container_of(dev->governor,
			struct governor, devfreq_gov);
	dev->profile->get_dev_status(dev->dev.parent, &stats);
	vidc_data = (struct msm_vidc_gov_data *)stats.private_data;

	for (c = 0; c < vidc_data->data_count; ++c) {
		if (vidc_data->data->power_mode == VIDC_POWER_TURBO) {
			*freq = INT_MAX;
			goto exit;
		}
	}

	for (c = 0; c < vidc_data->data_count; ++c)
		ab_kbps += __calculate(&vidc_data->data[c], gov->mode);

	*freq = clamp(ab_kbps, dev->min_freq, dev->max_freq ?: UINT_MAX);
exit:
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
	case DEVFREQ_GOV_RESUME:
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
			.name = "msm-vidc-ddr",
			.get_target_freq = __get_target_freq,
			.event_handler = __event_handler,
		},
	},
	{
		.mode = GOVERNOR_VMEM,
		.devfreq_gov = {
			.name = "msm-vidc-vmem",
			.get_target_freq = __get_target_freq,
			.event_handler = __event_handler,
		},
	},
	{
		.mode = GOVERNOR_VMEM_PLUS,
		.devfreq_gov = {
			.name = "msm-vidc-vmem+",
			.get_target_freq = __get_target_freq,
			.event_handler = __event_handler,
		},
	},
};

static int __init msm_vidc_bw_gov_init(void)
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
module_init(msm_vidc_bw_gov_init);

static void __exit msm_vidc_bw_gov_exit(void)
{
	int c = 0;

	for (c = 0; c < ARRAY_SIZE(governors); ++c) {
		dprintk(VIDC_DBG, "Removing governor %s\n",
				governors[c].devfreq_gov.name);
		devfreq_remove_governor(&governors[c].devfreq_gov);
	}
}
module_exit(msm_vidc_bw_gov_exit);
MODULE_LICENSE("GPL v2");

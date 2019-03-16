// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/of_fdt.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"


#define DDR_TYPE_LPDDR4 0x6
#define DDR_TYPE_LPDDR4X 0x7
#define DDR_TYPE_LPDDR4Y 0x8
#define DDR_TYPE_LPDDR5 0x9

#define CODEC_ENTRY(n, p, vsp, vpp, lp) \
{	\
	.fourcc = n,		\
	.session_type = p,	\
	.vsp_cycles = vsp,	\
	.vpp_cycles = vpp,	\
	.low_power_cycles = lp	\
}

#define EFUSE_ENTRY(sa, s, m, sh, p) \
{	\
	.start_address = sa,		\
	.size = s,	\
	.mask = m,	\
	.shift = sh,	\
	.purpose = p	\
}

#define UBWC_CONFIG(mco, mlo, hbo, bslo, bso, rs, mc, ml, hbb, bsl, bsp) \
{	\
	.override_bit_info.max_channel_override = mco,	\
	.override_bit_info.mal_length_override = mlo,	\
	.override_bit_info.hb_override = hbo,	\
	.override_bit_info.bank_swzl_level_override = bslo,	\
	.override_bit_info.bank_spreading_override = bso,	\
	.override_bit_info.reserved = rs,	\
	.max_channels = mc,	\
	.mal_length = ml,	\
	.highest_bank_bit = hbb,	\
	.bank_swzl_level = bsl,	\
	.bank_spreading = bsp,	\
}

static struct msm_vidc_codec_data default_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 125, 675, 320),
};

/* Update with Kona data */
static struct msm_vidc_codec_data kona_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_TME, MSM_VIDC_ENCODER, 0, 540, 540),
	CODEC_ENTRY(V4L2_PIX_FMT_MPEG2, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP9, MSM_VIDC_DECODER, 10, 200, 200),
};

/* Update with SM6150 data */
static struct msm_vidc_codec_data sm6150_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_TME, MSM_VIDC_ENCODER, 0, 540, 540),
	CODEC_ENTRY(V4L2_PIX_FMT_MPEG2, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP9, MSM_VIDC_DECODER, 50, 200, 200),
};

/* Update with 855 data */
static struct msm_vidc_codec_data sm8150_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_TME, MSM_VIDC_ENCODER, 0, 540, 540),
	CODEC_ENTRY(V4L2_PIX_FMT_MPEG2, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP9, MSM_VIDC_DECODER, 10, 200, 200),
};

static struct msm_vidc_codec_data sdm845_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_TME, MSM_VIDC_ENCODER, 0, 540, 540),
	CODEC_ENTRY(V4L2_PIX_FMT_MPEG2, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP9, MSM_VIDC_DECODER, 50, 200, 200),
};

static struct msm_vidc_codec_data sdm670_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_TME, MSM_VIDC_ENCODER, 0, 540, 540),
	CODEC_ENTRY(V4L2_PIX_FMT_MPEG2, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_VIDC_DECODER, 50, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP9, MSM_VIDC_DECODER, 50, 200, 200),
};

#define ENC     HAL_VIDEO_DOMAIN_ENCODER
#define DEC     HAL_VIDEO_DOMAIN_DECODER
#define H264    HAL_VIDEO_CODEC_H264
#define HEVC    HAL_VIDEO_CODEC_HEVC
#define VP8     HAL_VIDEO_CODEC_VP8
#define VP9     HAL_VIDEO_CODEC_VP9
#define MPEG2   HAL_VIDEO_CODEC_MPEG2
#define DOMAINS_ALL    (HAL_VIDEO_DOMAIN_ENCODER | HAL_VIDEO_DOMAIN_DECODER)
#define CODECS_ALL     (HAL_VIDEO_CODEC_H264 | HAL_VIDEO_CODEC_HEVC | \
			HAL_VIDEO_CODEC_VP8 | HAL_VIDEO_CODEC_VP9 | \
			HAL_VIDEO_CODEC_MPEG2)

static struct msm_vidc_codec default_codecs[] = {
	/* {domain, codec} */
	{DEC, H264}, {DEC, HEVC}, {DEC, VP8}, {DEC, VP9}, {DEC, MPEG2},
	{ENC, H264}, {ENC, HEVC}, {ENC, VP8},
};

static struct msm_vidc_codec_capability kona_capabilities[] = {
	/* {cap_type, min, max, step_size, default_value, domains, codecs} */
	{CAP_FRAME_WIDTH, DOMAINS_ALL, CODECS_ALL, 128, 8192, 1, 1920},
	{CAP_FRAME_HEIGHT, DOMAINS_ALL, CODECS_ALL, 128, 8192, 1, 1080},
	/* (8192 * 4320) / 256 */
	{CAP_MBS_PER_FRAME, DOMAINS_ALL, CODECS_ALL, 1, 138240, 1, 138240},
	/* ((1920 * 1088) / 256) * 960 fps */
	{CAP_MBS_PER_SECOND, DOMAINS_ALL, CODECS_ALL, 1, 7833600, 1, 7833600},
	{CAP_FRAMERATE, DOMAINS_ALL, CODECS_ALL, 1, 960, 1, 30},
	{CAP_BITRATE, DOMAINS_ALL, CODECS_ALL, 1, 220000000, 1, 20000000},
	{CAP_SCALE_X, DOMAINS_ALL, CODECS_ALL, 4096, 65536, 1, 4096},
	{CAP_SCALE_Y, DOMAINS_ALL, CODECS_ALL, 4096, 65536, 1, 4096},
	{CAP_BFRAME, ENC, H264|HEVC, 0, 1, 1, 0},
	{CAP_HIER_P_NUM_ENH_LAYERS, ENC, H264|HEVC, 0, 6, 1, 0},
	{CAP_LTR_COUNT, ENC, H264|HEVC, 0, 6, 1, 0},
	/* ((4096 * 2304) / 256) * 60 fps */
	{CAP_MBS_PER_SECOND_POWER_SAVE, ENC, CODECS_ALL,
		0, 2211840, 1, 2211840},
	{CAP_I_FRAME_QP, ENC, H264|HEVC, 0, 51, 1, 10},
	{CAP_P_FRAME_QP, ENC, H264|HEVC, 0, 51, 1, 20},
	{CAP_B_FRAME_QP, ENC, H264|HEVC, 0, 51, 1, 20},
	{CAP_I_FRAME_QP, ENC, VP8|VP9, 0, 127, 1, 20},
	{CAP_P_FRAME_QP, ENC, VP8|VP9, 0, 127, 1, 40},
	{CAP_B_FRAME_QP, ENC, VP8|VP9, 0, 127, 1, 40},
	/* 10 slices */
	{CAP_SLICE_BYTE, ENC, H264|HEVC, 1, 10, 1, 10},
	{CAP_SLICE_MB, ENC, H264|HEVC, 1, 10, 1, 10},
	{CAP_MAX_VIDEOCORES, DOMAINS_ALL, CODECS_ALL, 0, 1, 1, 1},

	/* VP8 specific */
	{CAP_FRAME_WIDTH, ENC|DEC, VP8, 128, 4096, 1, 1920},
	{CAP_FRAME_HEIGHT, ENC|DEC, VP8, 128, 4096, 1, 1080},
	/* (4096 * 2304) / 256 */
	{CAP_MBS_PER_FRAME, ENC|DEC, VP8, 1, 36864, 1, 8160},
	/* ((4096 * 2304) / 256) * 120 */
	{CAP_MBS_PER_SECOND, ENC|DEC, VP8, 1, 4423680, 1, 244800},
	{CAP_FRAMERATE, ENC|DEC, VP8, 1, 120, 1, 30},
	{CAP_BITRATE, ENC, VP8, 1, 74000000, 1, 20000000},
	{CAP_BITRATE, DEC, VP8, 1, 220000000, 1, 20000000},

	/* Mpeg2 decoder specific */
	{CAP_FRAME_WIDTH, DEC, MPEG2, 128, 1920, 1, 1920},
	{CAP_FRAME_HEIGHT, DEC, MPEG2, 128, 1920, 1, 1080},
	/* (1920 * 1088) / 256 */
	{CAP_MBS_PER_FRAME, DEC, MPEG2, 1, 8160, 1, 8160},
	/* ((1920 * 1088) / 256) * 30*/
	{CAP_MBS_PER_SECOND, DEC, MPEG2, 1, 244800, 1, 244800},
	{CAP_FRAMERATE, DEC, MPEG2, 1, 30, 1, 30},
	{CAP_BITRATE, DEC, MPEG2, 1, 40000000, 1, 20000000},

	/* Secure usecase specific */
	{CAP_SECURE_FRAME_WIDTH, DOMAINS_ALL, CODECS_ALL, 128, 4096, 1, 1920},
	{CAP_SECURE_FRAME_HEIGHT, DOMAINS_ALL, CODECS_ALL, 128, 4096, 1, 1080},
	/* (4096 * 2304) / 256 */
	{CAP_SECURE_MBS_PER_FRAME, DOMAINS_ALL, CODECS_ALL, 1, 36864, 1, 36864},
	{CAP_SECURE_BITRATE, DOMAINS_ALL, CODECS_ALL, 1, 40000000, 1, 20000000},
};

/*
 * Custom conversion coefficients for resolution: 176x144 negative
 * coeffs are converted to s4.9 format
 * (e.g. -22 converted to ((1 << 13) - 22)
 * 3x3 transformation matrix coefficients in s4.9 fixed point format
 */
static u32 vpe_csc_custom_matrix_coeff[HAL_MAX_MATRIX_COEFFS] = {
	470, 8170, 8148, 0, 490, 50, 0, 34, 483
};

/* offset coefficients in s9 fixed point format */
static u32 vpe_csc_custom_bias_coeff[HAL_MAX_BIAS_COEFFS] = {
	34, 0, 4
};

/* clamping value for Y/U/V([min,max] for Y/U/V) */
static u32 vpe_csc_custom_limit_coeff[HAL_MAX_LIMIT_COEFFS] = {
	16, 235, 16, 240, 16, 240
};

static struct msm_vidc_common_data default_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
};

static struct msm_vidc_common_data kona_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,             /*
					 * As per design driver allows 3rd
					 * instance as well since the secure
					 * flags were updated later for the
					 * current instance. Hence total
					 * secure sessions would be
					 * max-secure-instances + 1.
					 */
	},
	{
		.key = "qcom,max-hw-load",
		.value = 3916800,       /*
					 * 1920x1088/256 MB's@480fps. It is more
					 * than any other usecases (ex:
					 * 3840x2160@120fps, 4096x2160@96ps,
					 * 7680x4320@30fps)
					 */
	},
	{
		.key = "qcom,max-hq-mbs-per-frame",
		.value = 34560,		/* 4096x2160 */
	},
	{
		.key = "qcom,max-hq-mbs-per-sec",
		.value = 1036800,	/* 4096x2160@30fps */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-frame",
		.value = 32400, /* 3840x2160/256 */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-sec",
		.value = 1944000, /* 3840x2160/256 MBs@60fps */
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 1500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,domain-cvp",
		.value = 0,
	},
	{
		.key = "qcom,decode-batching",
		.value = 0,
	},
	{
		.key = "qcom,dcvs",
		.value = 1,
	},
	{
		.key = "qcom,fw-cycles",
		.value = 326389,
	},
	{
		.key = "qcom,fw-vpp-cycles",
		.value = 44156,
	},
};

static struct msm_vidc_common_data sm6150_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 5,
	},
	{
		.key = "qcom,max-hw-load",
		.value = 1216800,
	},
	{
		.key = "qcom,max-hq-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-hq-mbs-per-sec",
		.value = 244800,  /* 1920 x 1088 @ 30 fps */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-b-frame-mbs-per-sec",
		.value = 489600,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 1500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,dcvs",
		.value = 1,
	},
	{
		.key = "qcom,fw-cycles",
		.value = 733003,
	},
	{
		.key = "qcom,fw-vpp-cycles",
		.value = 225975,
	},
};

static struct msm_vidc_common_data sm8150_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 2,             /*
					 * As per design driver allows 3rd
					 * instance as well since the secure
					 * flags were updated later for the
					 * current instance. Hence total
					 * secure sessions would be
					 * max-secure-instances + 1.
					 */
	},
	{
		.key = "qcom,max-hw-load",
		.value = 3916800,       /*
					 * 1920x1088/256 MBs@480fps. It is less
					 * any other usecases (ex:
					 * 3840x2160@120fps, 4096x2160@96ps,
					 * 7680x4320@30fps)
					 */
	},
	{
		.key = "qcom,max-hq-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-hq-mbs-per-sec",
		.value = 244800,  /* 1920 x 1088 @ 30 fps */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-b-frame-mbs-per-sec",
		.value = 489600,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 1500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 1000,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,domain-cvp",
		.value = 1,
	},
	{
		.key = "qcom,decode-batching",
		.value = 1,
	},
	{
		.key = "qcom,dcvs",
		.value = 1,
	},
	{
		.key = "qcom,fw-cycles",
		.value = 760000,
	},
	{
		.key = "qcom,fw-vpp-cycles",
		.value = 166667,
	},
};

static struct msm_vidc_common_data sdm845_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-cache-pagetables",
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 5,
	},
	{
		.key = "qcom,max-hw-load",
		.value = 3110400,	/* 4096x2160@90 */
	},
	{
		.key = "qcom,max-hq-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-hq-mbs-per-sec",
		.value = 244800,  /* 1920 x 1088 @ 30 fps */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-b-frame-mbs-per-sec",
		.value = 489600,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 250,
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	},
	{
		.key = "qcom,dcvs",
		.value = 1,
	},
};

static struct msm_vidc_common_data sdm670_common_data_v0[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 5,
	},
	{
		.key = "qcom,max-hw-load",
		.value = 1944000,
	},
	{
		.key = "qcom,max-hq-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-hq-mbs-per-sec",
		.value = 244800,  /* 1920 x 1088 @ 30 fps */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-b-frame-mbs-per-sec",
		.value = 489600,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 250,
	},
	{
		.key = "qcom,dcvs",
		.value = 1,
	},
};

static struct msm_vidc_common_data sdm670_common_data_v1[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 1,
	},
	{
		.key = "qcom,domain-attr-non-fatal-faults",
		.value = 1,
	},
	{
		.key = "qcom,max-secure-instances",
		.value = 5,
	},
	{
		.key = "qcom,max-hw-load",
		.value = 1216800,
	},
	{
		.key = "qcom,max-hq-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-hq-mbs-per-sec",
		.value = 244800,  /* 1920 x 1088 @ 30 fps */
	},
	{
		.key = "qcom,max-b-frame-mbs-per-frame",
		.value = 8160,
	},
	{
		.key = "qcom,max-b-frame-mbs-per-sec",
		.value = 489600,
	},
	{
		.key = "qcom,power-collapse-delay",
		.value = 500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 250,
	},
	{
		.key = "qcom,dcvs",
		.value = 1,
	},
};

static struct msm_vidc_efuse_data sdm670_efuse_data[] = {
	EFUSE_ENTRY(0x007801A0, 4, 0x00008000, 0x0f, SKU_VERSION),
};

/* Default UBWC config for LPDDR5 */
static struct msm_vidc_ubwc_config_data kona_ubwc_data[] = {
	UBWC_CONFIG(1, 1, 1, 0, 0, 0, 8, 32, 15, 0, 0),
};

static struct msm_vidc_platform_data default_data = {
	.codec_data = default_codec_data,
	.codec_data_length =  ARRAY_SIZE(default_codec_data),
	.common_data = default_common_data,
	.common_data_length =  ARRAY_SIZE(default_common_data),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = NULL,
	.efuse_data_length = 0,
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_IRIS2,
	.ubwc_config = 0x0,
};

static struct msm_vidc_platform_data kona_data = {
	.codec_data = kona_codec_data,
	.codec_data_length =  ARRAY_SIZE(kona_codec_data),
	.common_data = kona_common_data,
	.common_data_length =  ARRAY_SIZE(kona_common_data),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = NULL,
	.efuse_data_length = 0,
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_IRIS2,
	.ubwc_config = kona_ubwc_data,
	.codecs = default_codecs,
	.codecs_count = ARRAY_SIZE(default_codecs),
	.codec_caps = kona_capabilities,
	.codec_caps_count = ARRAY_SIZE(kona_capabilities),
};

static struct msm_vidc_platform_data sm6150_data = {
	.codec_data = sm6150_codec_data,
	.codec_data_length =  ARRAY_SIZE(sm6150_codec_data),
	.common_data = sm6150_common_data,
	.common_data_length =  ARRAY_SIZE(sm6150_common_data),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = NULL,
	.efuse_data_length = 0,
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_AR50,
	.ubwc_config = 0x0,
};

static struct msm_vidc_platform_data sm8150_data = {
	.codec_data = sm8150_codec_data,
	.codec_data_length =  ARRAY_SIZE(sm8150_codec_data),
	.common_data = sm8150_common_data,
	.common_data_length =  ARRAY_SIZE(sm8150_common_data),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = NULL,
	.efuse_data_length = 0,
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_IRIS1,
	.ubwc_config = 0x0,
};

static struct msm_vidc_platform_data sdm845_data = {
	.codec_data = sdm845_codec_data,
	.codec_data_length =  ARRAY_SIZE(sdm845_codec_data),
	.common_data = sdm845_common_data,
	.common_data_length =  ARRAY_SIZE(sdm845_common_data),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = NULL,
	.efuse_data_length = 0,
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_AR50,
	.ubwc_config = 0x0,
};

static struct msm_vidc_platform_data sdm670_data = {
	.codec_data = sdm670_codec_data,
	.codec_data_length =  ARRAY_SIZE(sdm670_codec_data),
	.common_data = sdm670_common_data_v0,
	.common_data_length =  ARRAY_SIZE(sdm670_common_data_v0),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = sdm670_efuse_data,
	.efuse_data_length = ARRAY_SIZE(sdm670_efuse_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_AR50,
	.ubwc_config = 0x0,
};

static const struct of_device_id msm_vidc_dt_match[] = {
	{
		.compatible = "qcom,kona-vidc",
		.data = &kona_data,
	},
	{
		.compatible = "qcom,sm6150-vidc",
		.data = &sm6150_data,
	},
	{
		.compatible = "qcom,sm8150-vidc",
		.data = &sm8150_data,
	},
	{
		.compatible = "qcom,sdm845-vidc",
		.data = &sdm845_data,
	},
	{
		.compatible = "qcom,sdm670-vidc",
		.data = &sdm670_data,
	},
	{},
};

MODULE_DEVICE_TABLE(of, msm_vidc_dt_match);

static int msm_vidc_read_efuse(
		struct msm_vidc_platform_data *data, struct device *dev)
{
	void __iomem *base;
	uint32_t i;
	struct msm_vidc_efuse_data *efuse_data = data->efuse_data;
	uint32_t efuse_data_count = data->efuse_data_length;

	for (i = 0; i < efuse_data_count; i++) {

		switch ((efuse_data[i]).purpose) {

		case SKU_VERSION:
			base = devm_ioremap(dev, (efuse_data[i]).start_address,
					(efuse_data[i]).size);
			if (!base) {
				dprintk(VIDC_ERR,
					"failed efuse ioremap: res->start %#x, size %d\n",
					(efuse_data[i]).start_address,
					(efuse_data[i]).size);
					return -EINVAL;
			} else {
				u32 efuse = 0;

				efuse = readl_relaxed(base);
				data->sku_version =
					(efuse & (efuse_data[i]).mask) >>
					(efuse_data[i]).shift;
				dprintk(VIDC_DBG,
					"efuse 0x%x, platform version 0x%x\n",
					efuse, data->sku_version);

				devm_iounmap(dev, base);
			}
			break;

		default:
			break;
		}
	}
	return 0;
}

void *vidc_get_drv_data(struct device *dev)
{
	struct msm_vidc_platform_data *driver_data = NULL;
	const struct of_device_id *match;
	uint32_t ddr_type = DDR_TYPE_LPDDR5;
	int rc = 0;

	if (!IS_ENABLED(CONFIG_OF) || !dev->of_node) {
		driver_data = &default_data;
		goto exit;
	}

	match = of_match_node(msm_vidc_dt_match, dev->of_node);

	if (match)
		driver_data = (struct msm_vidc_platform_data *)match->data;

	if (!of_find_property(dev->of_node, "sku-index", NULL) ||
			!driver_data) {
		goto exit;
	} else if (!strcmp(match->compatible, "qcom,sdm670-vidc")) {
		rc = msm_vidc_read_efuse(driver_data, dev);
		if (rc)
			goto exit;

		if (driver_data->sku_version == SKU_VERSION_1) {
			driver_data->common_data = sdm670_common_data_v1;
			driver_data->common_data_length =
					ARRAY_SIZE(sdm670_common_data_v1);
		}
	}  else if (!strcmp(match->compatible, "qcom,kona-vidc")) {
		ddr_type = of_fdt_get_ddrtype();
		if (ddr_type == -ENOENT) {
			dprintk(VIDC_ERR,
				"Failed to get ddr type, use LPDDR5\n");
		}
		dprintk(VIDC_DBG, "DDR Type %x\n", ddr_type);

		if (driver_data->ubwc_config &&
			(ddr_type == DDR_TYPE_LPDDR4 ||
			ddr_type == DDR_TYPE_LPDDR4X ||
			ddr_type == DDR_TYPE_LPDDR4Y))
			driver_data->ubwc_config->highest_bank_bit = 0xf;
	}

exit:
	return driver_data;
}

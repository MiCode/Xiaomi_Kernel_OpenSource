// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"

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

/*FIXME: hard coded AXI_REG_START_ADDR???*/
#define GCC_VIDEO_AXI_REG_START_ADDR	0x10B024
#define GCC_VIDEO_AXI_REG_SIZE		0xC

static struct msm_cvp_codec_data default_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_CVP_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_CVP_DECODER, 125, 675, 320),
};

/* Update with 855 data */
static struct msm_cvp_codec_data sm8150_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_CVP_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_CVP_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_CVP_ENCODER, 10, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_TME, MSM_CVP_ENCODER, 0, 540, 540),
	CODEC_ENTRY(V4L2_PIX_FMT_MPEG2, MSM_CVP_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_CVP_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_HEVC, MSM_CVP_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP8, MSM_CVP_DECODER, 10, 200, 200),
	CODEC_ENTRY(V4L2_PIX_FMT_VP9, MSM_CVP_DECODER, 10, 200, 200),
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

static struct msm_cvp_common_data default_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
};

static struct msm_cvp_common_data sm8250_common_data[] = {
	{
		.key = "qcom,never-unload-fw",
		.value = 1,
	},
	{
		.key = "qcom,sw-power-collapse",
		.value = 0,
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
		.key = "qcom,max-hq-frames-per-sec",
		.value = 60,
	},
	{
		.key = "qcom,max-b-frame-size",
		.value = 8160,
	},
	{
		.key = "qcom,max-b-frames-per-sec",
		.value = 60,
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
		.key = "qcom,use-devfreq-scale-bus",
		.value = 0,
	},
};

/* Default UBWC config for LPDDR5 */
static struct msm_cvp_ubwc_config_data kona_ubwc_data[] = {
	UBWC_CONFIG(1, 1, 1, 0, 0, 0, 8, 32, 16, 0, 0),
};


static struct msm_cvp_platform_data default_data = {
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
	.gcc_register_base = 0,
	.gcc_register_size = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = 0x0,
};

static struct msm_cvp_platform_data sm8250_data = {
	.codec_data = sm8150_codec_data,
	.codec_data_length =  ARRAY_SIZE(sm8150_codec_data),
	.common_data = sm8250_common_data,
	.common_data_length =  ARRAY_SIZE(sm8250_common_data),
	.csc_data.vpe_csc_custom_bias_coeff = vpe_csc_custom_bias_coeff,
	.csc_data.vpe_csc_custom_matrix_coeff = vpe_csc_custom_matrix_coeff,
	.csc_data.vpe_csc_custom_limit_coeff = vpe_csc_custom_limit_coeff,
	.efuse_data = NULL,
	.efuse_data_length = 0,
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,
};

static const struct of_device_id msm_cvp_dt_match[] = {
	{
		.compatible = "qcom,kona-cvp",
		.data = &sm8250_data,
	},
	{},
};

MODULE_DEVICE_TABLE(of, msm_cvp_dt_match);

void *cvp_get_drv_data(struct device *dev)
{
	struct msm_cvp_platform_data *driver_data;
	const struct of_device_id *match;
	uint32_t ddr_type = DDR_TYPE_LPDDR5;

	driver_data = &default_data;

	if (!IS_ENABLED(CONFIG_OF) || !dev->of_node)
		goto exit;

	match = of_match_node(msm_cvp_dt_match, dev->of_node);

	if (match)
		driver_data = (struct msm_cvp_platform_data *)match->data;

	if (!strcmp(match->compatible, "qcom,kona-cvp")) {
		ddr_type = of_fdt_get_ddrtype();
		if (ddr_type == -ENOENT) {
			dprintk(CVP_ERR,
				"Failed to get ddr type, use LPDDR5\n");
		}

		if (driver_data->ubwc_config &&
			(ddr_type == DDR_TYPE_LPDDR4 ||
			ddr_type == DDR_TYPE_LPDDR4X ||
			ddr_type == DDR_TYPE_LPDDR4Y))
			driver_data->ubwc_config->highest_bank_bit = 15;
	}
exit:
	return driver_data;
}

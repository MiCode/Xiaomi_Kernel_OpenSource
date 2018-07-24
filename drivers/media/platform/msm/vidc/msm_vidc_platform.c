/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"


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

#define GCC_VIDEO_AXI_REG_START_ADDR	0x10B024
#define GCC_VIDEO_AXI_REG_SIZE		0xC

static struct msm_vidc_codec_data default_codec_data[] =  {
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_ENCODER, 125, 675, 320),
	CODEC_ENTRY(V4L2_PIX_FMT_H264, MSM_VIDC_DECODER, 125, 675, 320),
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
		.value = 500,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 250,
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
		.value = 5,
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
	.gcc_register_base = 0,
	.gcc_register_size = 0,
	.vpu_ver = VPU_VERSION_5,
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
	.gcc_register_base = 0,
	.gcc_register_size = 0,
	.vpu_ver = VPU_VERSION_4,
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
	.gcc_register_base = GCC_VIDEO_AXI_REG_START_ADDR,
	.gcc_register_size = GCC_VIDEO_AXI_REG_SIZE,
	.vpu_ver = VPU_VERSION_5,
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
	.gcc_register_base = 0,
	.gcc_register_size = 0,
	.vpu_ver = VPU_VERSION_4,
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
	.gcc_register_base = 0,
	.gcc_register_size = 0,
	.vpu_ver = VPU_VERSION_4,
};

static const struct of_device_id msm_vidc_dt_match[] = {
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
	}

exit:
	return driver_data;
}

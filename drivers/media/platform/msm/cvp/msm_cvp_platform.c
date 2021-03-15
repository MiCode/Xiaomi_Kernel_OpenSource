// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/of_common.h>
#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"

#define DDR_TYPE_LPDDR4 0x6
#define DDR_TYPE_LPDDR4X 0x7
#define DDR_TYPE_LPDDR4Y 0x8
#define DDR_TYPE_LPDDR5 0x9

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
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 2000,
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	}
};

static struct msm_cvp_common_data sm8350_common_data[] = {
	{
		.key = "qcom,auto-pil",
		.value = 1,
	},
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
		.key = "qcom,power-collapse-delay",
		.value = 3000,
	},
	{
		.key = "qcom,hw-resp-timeout",
		.value = 2000,
	},
	{
		.key = "qcom,dsp-resp-timeout",
		.value = 1000
	},
	{
		.key = "qcom,debug-timeout",
		.value = 0,
	}
};


/* Default UBWC config for LPDDR5 */
static struct msm_cvp_ubwc_config_data kona_ubwc_data[] = {
	UBWC_CONFIG(1, 1, 1, 0, 0, 0, 8, 32, 16, 0, 0),
};

/* Default UBWC config for LPDDR5 */
static struct msm_cvp_ubwc_config_data shima_ubwc_data[] = {
	UBWC_CONFIG(1, 1, 1, 0, 0, 0, 8, 32, 15, 0, 0),
};

static struct msm_cvp_platform_data default_data = {
	.common_data = default_common_data,
	.common_data_length =  ARRAY_SIZE(default_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = 0x0,
};

static struct msm_cvp_platform_data sm8250_data = {
	.common_data = sm8250_common_data,
	.common_data_length =  ARRAY_SIZE(sm8250_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,
};

static struct msm_cvp_platform_data sm8350_data = {
	.common_data = sm8350_common_data,
	.common_data_length =  ARRAY_SIZE(sm8350_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = kona_ubwc_data,
};

static struct msm_cvp_platform_data shima_data = {
	.common_data = sm8350_common_data,
	.common_data_length =  ARRAY_SIZE(sm8350_common_data),
	.sku_version = 0,
	.vpu_ver = VPU_VERSION_5,
	.ubwc_config = shima_ubwc_data,
};

static const struct of_device_id msm_cvp_dt_match[] = {
	{
		.compatible = "qcom,shima-cvp",
		.data = &shima_data,
	},
	{
		.compatible = "qcom,lahaina-cvp",
		.data = &sm8350_data,
	},
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

	if (!match)
		return NULL;

	driver_data = (struct msm_cvp_platform_data *)match->data;

	if (!strcmp(match->compatible, "qcom,lahaina-cvp")) {
		ddr_type = of_fdt_get_ddrtype();
		if (ddr_type == -ENOENT) {
			dprintk(CVP_ERR,
				"Failed to get ddr type, use LPDDR5\n");
		}

		if (driver_data->ubwc_config &&
			(ddr_type == DDR_TYPE_LPDDR4 ||
			ddr_type == DDR_TYPE_LPDDR4X))
			driver_data->ubwc_config->highest_bank_bit = 15;
		dprintk(CVP_CORE, "DDR Type 0x%x hbb 0x%x\n",
			ddr_type, driver_data->ubwc_config ?
			driver_data->ubwc_config->highest_bank_bit : -1);
	}

	if (!strcmp(match->compatible, "qcom,shima-cvp")) {
		ddr_type = of_fdt_get_ddrtype();
		if (ddr_type == -ENOENT) {
			dprintk(CVP_ERR,
				"Failed to get ddr type, use LPDDR5\n");
		}

		if (driver_data->ubwc_config &&
			(ddr_type == DDR_TYPE_LPDDR4 ||
			ddr_type == DDR_TYPE_LPDDR4X))
			driver_data->ubwc_config->highest_bank_bit = 14;
		dprintk(CVP_CORE, "DDR Type 0x%x hbb 0x%x\n",
			ddr_type, driver_data->ubwc_config ?
			driver_data->ubwc_config->highest_bank_bit : -1);
	}
exit:
	return driver_data;
}

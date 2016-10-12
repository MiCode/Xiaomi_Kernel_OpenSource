/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/slab.h>
#include <linux/of_address.h>

#include "sde_hw_mdss.h"
#include "sde_hw_catalog.h"
#include "sde_hw_catalog_format.h"
#include "sde_kms.h"

/*************************************************************
 * MACRO DEFINITION
 *************************************************************/

/**
 * Max hardware block in certain hardware. For ex: sspp pipes
 * can have QSEED, pcc, igc, pa, csc, etc. This count is max
 * 12 based on software design. It should be increased if any of the
 * hardware block has more subblocks.
 */
#define MAX_SDE_HW_BLK  12

/* each entry will have register address and bit offset in that register */
#define MAX_BIT_OFFSET 2

/* default line width for sspp */
#define DEFAULT_SDE_LINE_WIDTH 2048

/* max mixer blend stages */
#define DEFAULT_SDE_MIXER_BLENDSTAGES 7

/* max bank bit for macro tile and ubwc format */
#define DEFAULT_SDE_HIGHEST_BANK_BIT 15

/* default hardware block size if dtsi entry is not present */
#define DEFAULT_SDE_HW_BLOCK_LEN 0x100

/* default rects for multi rect case */
#define DEFAULT_SDE_SSPP_MAX_RECTS 1

/* total number of intf - dp, dsi, hdmi */
#define INTF_COUNT			3

#define MAX_SSPP_UPSCALE		20
#define MAX_SSPP_DOWNSCALE		4
#define SSPP_UNITY_SCALE		1

#define MAX_HORZ_DECIMATION		4
#define MAX_VERT_DECIMATION		4

#define MAX_SPLIT_DISPLAY_CTL		2
#define MAX_PP_SPLIT_DISPLAY_CTL	1

#define MDSS_BASE_OFFSET		0x0

#define ROT_LM_OFFSET			3
#define LINE_LM_OFFSET			5
#define LINE_MODE_WB_OFFSET		2

/* maximum XIN halt timeout in usec */
#define VBIF_XIN_HALT_TIMEOUT		0x4000

#define DEFAULT_CREQ_LUT_NRT		0x0
#define DEFAULT_PIXEL_RAM_SIZE		(50 * 1024)

/*************************************************************
 *  DTSI PROPERTY INDEX
 *************************************************************/
enum {
	HW_OFF,
	HW_LEN,
};

enum sde_prop {
	SDE_OFF,
	SDE_LEN,
	SSPP_LINEWIDTH,
	MIXER_LINEWIDTH,
	MIXER_BLEND,
	WB_LINEWIDTH,
	BANK_BIT,
	QSEED_TYPE,
	PANIC_PER_PIPE,
	CDP,
	SRC_SPLIT,
};

enum {
	SSPP_OFF,
	SSPP_SIZE,
	SSPP_TYPE,
	SSPP_XIN,
	SSPP_CLK_CTRL,
	SSPP_CLK_STATUS,
	SSPP_DANGER,
	SSPP_SAFE,
	SSPP_MAX_RECTS,
	SSPP_SCALE_SIZE,
	SSPP_QSEED_OFF,
	SSPP_CSC_OFF,
};

enum {
	INTF_OFF,
	INTF_LEN,
	INTF_PREFETCH,
	INTF_TYPE,
};

enum {
	PP_OFF,
	PP_LEN,
	TE_OFF,
	TE_LEN,
	TE2_OFF,
	TE2_LEN,
	DSC_OFF,
	DSC_LEN,
};

enum {
	DSPP_OFF,
	DSPP_SIZE,
	DSPP_IGC,
	DSPP_PCC,
	DSPP_GC,
	DSPP_PA,
	DSPP_GAMUT,
	DSPP_DITHER,
	DSPP_HIST,
	DSPP_AD,
};

enum {
	MIXER_OFF,
	MIXER_LEN,
	MIXER_GC,
};

enum {
	WB_OFF,
	WB_LEN,
	WB_ID,
	WB_XIN_ID,
};

enum {
	VBIF_OFF,
	VBIF_LEN,
	VBIF_ID,
	VBIF_DEFAULT_OT_RD_LIMIT,
	VBIF_DEFAULT_OT_WR_LIMIT,
	VBIF_DYNAMIC_OT_RD_LIMIT,
	VBIF_DYNAMIC_OT_WR_LIMIT,
};

/*************************************************************
 * dts property definition
 *************************************************************/
enum prop_type {
	PROP_TYPE_BOOL,
	PROP_TYPE_U32,
	PROP_TYPE_U32_ARRAY,
	PROP_TYPE_STRING,
	PROP_TYPE_STRING_ARRAY,
	PROP_TYPE_BIT_OFFSET_ARRAY,
};

struct sde_prop_type {
	/* use property index from enum property for readability purpose */
	u8 id;
	/* it should be property name based on dtsi documentation */
	char *prop_name;
	/**
	 * if property is marked mandatory then it will fail parsing
	 * when property is not present
	 */
	u32  is_mandatory;
	/* property type based on "enum prop_type"  */
	enum prop_type type;
};

/*************************************************************
 * dts property list
 *************************************************************/
static struct sde_prop_type sde_prop[] = {
	{SDE_OFF, "qcom,sde-off", true, PROP_TYPE_U32},
	{SDE_LEN, "qcom,sde-len", false, PROP_TYPE_U32},
	{SSPP_LINEWIDTH, "qcom,sde-sspp-linewidth", false, PROP_TYPE_U32},
	{MIXER_LINEWIDTH, "qcom,sde-mixer-linewidth", false, PROP_TYPE_U32},
	{MIXER_BLEND, "qcom,sde-mixer-blendstages", false, PROP_TYPE_U32},
	{WB_LINEWIDTH, "qcom,sde-wb-linewidth", false, PROP_TYPE_U32},
	{BANK_BIT, "qcom,sde-highest-bank-bit", false, PROP_TYPE_U32},
	{QSEED_TYPE, "qcom,sde-qseed-type", false, PROP_TYPE_STRING},
	{PANIC_PER_PIPE, "qcom,sde-panic-per-pipe", false, PROP_TYPE_BOOL},
	{CDP, "qcom,sde-has-cdp", false, PROP_TYPE_BOOL},
	{SRC_SPLIT, "qcom,sde-has-src-split", false, PROP_TYPE_BOOL},
};

static struct sde_prop_type sspp_prop[] = {
	{SSPP_OFF, "qcom,sde-sspp-off", true, PROP_TYPE_U32_ARRAY},
	{SSPP_SIZE, "qcom,sde-sspp-src-size", false, PROP_TYPE_U32},
	{SSPP_TYPE, "qcom,sde-sspp-type", true, PROP_TYPE_STRING_ARRAY},
	{SSPP_XIN, "qcom,sde-sspp-xin-id", true, PROP_TYPE_U32_ARRAY},
	{SSPP_CLK_CTRL, "qcom,sde-sspp-clk-ctrl", false,
		PROP_TYPE_BIT_OFFSET_ARRAY},
	{SSPP_CLK_STATUS, "qcom,sde-sspp-clk-status", false,
		PROP_TYPE_BIT_OFFSET_ARRAY},
	{SSPP_DANGER, "qcom,sde-sspp-danger-lut", false, PROP_TYPE_U32_ARRAY},
	{SSPP_SAFE, "qcom,sde-sspp-safe-lut", false, PROP_TYPE_U32_ARRAY},
	{SSPP_MAX_RECTS, "qcom,sde-sspp-max-rects", false, PROP_TYPE_U32_ARRAY},
	{SSPP_SCALE_SIZE, "qcom,sde-sspp-scale-size", false, PROP_TYPE_U32},
	{SSPP_QSEED_OFF, "qcom,sde-sspp-qseed-off", false, PROP_TYPE_U32},
	{SSPP_CSC_OFF, "qcom,sde-sspp-csc-off", false, PROP_TYPE_U32},
};

static struct sde_prop_type ctl_prop[] = {
	{HW_OFF, "qcom,sde-ctl-off", true, PROP_TYPE_U32_ARRAY},
	{HW_LEN, "qcom,sde-ctl-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type mixer_prop[] = {
	{MIXER_OFF, "qcom,sde-mixer-off", true, PROP_TYPE_U32_ARRAY},
	{MIXER_LEN, "qcom,sde-mixer-size", false, PROP_TYPE_U32},
	{MIXER_GC, "qcom,sde-has-mixer-gc", false, PROP_TYPE_BOOL},
};

static struct sde_prop_type dspp_prop[] = {
	{DSPP_OFF, "qcom,sde-dspp-off", true, PROP_TYPE_U32_ARRAY},
	{DSPP_SIZE, "qcom,sde-dspp-size", false, PROP_TYPE_U32},
	{DSPP_IGC, "qcom,sde-dspp-igc-off", false, PROP_TYPE_U32},
	{DSPP_PCC, "qcom,sde-dspp-pcc-off", false, PROP_TYPE_U32},
	{DSPP_GC, "qcom,sde-dspp-gc-off", false, PROP_TYPE_U32},
	{DSPP_PA, "qcom,sde-dspp-pa-off", false, PROP_TYPE_U32},
	{DSPP_GAMUT, "qcom,sde-dspp-gamut-off", false, PROP_TYPE_U32},
	{DSPP_DITHER, "qcom,sde-dspp-dither-off", false, PROP_TYPE_U32},
	{DSPP_HIST, "qcom,sde-dspp-hist-off", false, PROP_TYPE_U32},
	{DSPP_AD, "qcom,sde-dspp-ad-off", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type pp_prop[] = {
	{PP_OFF, "qcom,sde-pp-off", true, PROP_TYPE_U32_ARRAY},
	{PP_LEN, "qcom,sde-pp-size", false, PROP_TYPE_U32},
	{TE_OFF, "qcom,sde-te-off", false, PROP_TYPE_U32_ARRAY},
	{TE_LEN, "qcom,sde-te-size", false, PROP_TYPE_U32},
	{TE2_OFF, "qcom,sde-te2-off", false, PROP_TYPE_U32_ARRAY},
	{TE2_LEN, "qcom,sde-te2-size", false, PROP_TYPE_U32},
	{DSC_OFF, "qcom,sde-dsc-off", false, PROP_TYPE_U32_ARRAY},
	{DSC_LEN, "qcom,sde-dsc-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type cdm_prop[] = {
	{HW_OFF, "qcom,sde-cdm-off", false, PROP_TYPE_U32_ARRAY},
	{HW_LEN, "qcom,sde-cdm-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type intf_prop[] = {
	{INTF_OFF, "qcom,sde-intf-off", true, PROP_TYPE_U32_ARRAY},
	{INTF_LEN, "qcom,sde-intf-size", false, PROP_TYPE_U32},
	{INTF_PREFETCH, "qcom,sde-intf-max-prefetch-lines", false,
						PROP_TYPE_U32_ARRAY},
	{INTF_TYPE, "qcom,sde-intf-type", false, PROP_TYPE_STRING_ARRAY},
};

static struct sde_prop_type wb_prop[] = {
	{WB_OFF, "qcom,sde-wb-off", true, PROP_TYPE_U32_ARRAY},
	{WB_LEN, "qcom,sde-wb-size", false, PROP_TYPE_U32},
	{WB_ID, "qcom,sde-wb-id", true, PROP_TYPE_U32_ARRAY},
	{WB_XIN_ID, "qcom,sde-wb-xin-id", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type vbif_prop[] = {
	{VBIF_OFF, "qcom,sde-vbif-off", true, PROP_TYPE_U32_ARRAY},
	{VBIF_LEN, "qcom,sde-vbif-size", false, PROP_TYPE_U32},
	{VBIF_ID, "qcom,sde-vbif-id", false, PROP_TYPE_U32_ARRAY},
	{VBIF_DEFAULT_OT_RD_LIMIT, "qcom,sde-vbif-default-ot-rd-limit", false,
		PROP_TYPE_U32},
	{VBIF_DEFAULT_OT_WR_LIMIT, "qcom,sde-vbif-default-ot-wr-limit", false,
		PROP_TYPE_U32},
	{VBIF_DYNAMIC_OT_RD_LIMIT, "qcom,sde-vbif-dynamic-ot-rd-limit", false,
		PROP_TYPE_U32_ARRAY},
	{VBIF_DYNAMIC_OT_WR_LIMIT, "qcom,sde-vbif-dynamic-ot-wr-limit", false,
		PROP_TYPE_U32_ARRAY},
};

/*************************************************************
 * static API list
 *************************************************************/
static int _parse_dt_u32_handler(struct device_node *np,
	char *prop_name, u32 *offsets, int len, bool mandatory)
{
	int rc = of_property_read_u32_array(np, prop_name, offsets, len);

	if (rc && mandatory)
		SDE_ERROR("mandatory prop: %s u32 array read len:%d\n",
				prop_name, len);
	else if (rc)
		SDE_DEBUG("optional prop: %s u32 array read len:%d\n",
				prop_name, len);

	return rc;
}

static int _parse_dt_bit_offset(struct device_node *np,
	char *prop_name, u32 prop_value[][MAX_BIT_OFFSET],
	u32 count, bool mandatory)
{
	int rc = 0, len, i, j;
	const u32 *arr;

	arr = of_get_property(np, prop_name, &len);
	if (arr) {
		len /= sizeof(u32);
		for (i = 0, j = 0; i < len; j++) {
			prop_value[j][0] = be32_to_cpu(arr[i]);
			i++;
			prop_value[j][1] = be32_to_cpu(arr[i]);
			i++;
		}
	} else {
		if (mandatory) {
			SDE_ERROR("error mandatory property '%s' not found\n",
				prop_name);
			rc = -EINVAL;
		} else {
			SDE_DEBUG("error optional property '%s' not found\n",
				prop_name);
		}
	}

	return rc;
}

static int _validate_dt_entry(struct device_node *np,
	struct sde_prop_type *sde_prop, u32 prop_size, int *prop_count,
	int *off_count)
{
	int rc = 0, i, val;

	*off_count = of_property_count_u32_elems(np, sde_prop[0].prop_name);
	if ((*off_count > MAX_BLOCKS) || (*off_count < 0)) {
		SDE_ERROR("invalid hw offset prop name:%s count:%d\n",
			sde_prop[0].prop_name, *off_count);
		*off_count = 0;
		return sde_prop[0].is_mandatory ? -EINVAL : 0;
	}

	for (i = 0; i < prop_size && i < MAX_BLOCKS; i++) {
		switch (sde_prop[i].type) {
		case PROP_TYPE_U32:
			rc = of_property_read_u32(np, sde_prop[i].prop_name,
				&val);
			break;
		case PROP_TYPE_U32_ARRAY:
			prop_count[i] = of_property_count_u32_elems(np,
				sde_prop[i].prop_name);
			break;
		case PROP_TYPE_STRING_ARRAY:
			prop_count[i] = of_property_count_strings(np,
				sde_prop[i].prop_name);
			break;
		case PROP_TYPE_BIT_OFFSET_ARRAY:
			of_get_property(np, sde_prop[i].prop_name, &val);
			prop_count[i] = val / (MAX_BIT_OFFSET * sizeof(u32));
			break;
		default:
			SDE_DEBUG("invalid property type:%d\n",
							sde_prop[i].type);
			break;
		}
		SDE_DEBUG("prop id:%d prop name:%s prop type:%d \"\
			prop_count:%d\n", i, sde_prop[i].prop_name,
			sde_prop[i].type, prop_count[i]);

		if (rc && sde_prop[i].is_mandatory &&
		   (sde_prop[i].type == PROP_TYPE_U32)) {
			SDE_ERROR("prop:%s not present\n",
						sde_prop[i].prop_name);
			goto end;
		} else if (sde_prop[i].type == PROP_TYPE_U32 ||
			sde_prop[i].type == PROP_TYPE_BOOL) {
			rc = 0;
			continue;
		}

		if ((prop_count[i] != *off_count) && sde_prop[i].is_mandatory) {
			SDE_ERROR("prop:%s count:%d is different compared to \"\
				offset array:%d\n", sde_prop[i].prop_name,
				prop_count[i], *off_count);
			rc = -EINVAL;
			goto end;
		} else if (prop_count[i] != *off_count) {
			SDE_DEBUG("prop:%s count:%d is different compared to \"\
				offset array:%d\n", sde_prop[i].prop_name,
				prop_count[i], *off_count);
			rc = 0;
			prop_count[i] = 0;
		}
	}

end:
	return rc;
}

static int _read_dt_entry(struct device_node *np,
	struct sde_prop_type *sde_prop, u32 prop_size, u32 *prop_count,
	u32 prop_value[][MAX_SDE_HW_BLK],
	u32 bit_value[][MAX_SDE_HW_BLK][MAX_BIT_OFFSET])
{
	int rc = 0, i, j;

	for (i = 0; i < prop_size && i < MAX_BLOCKS; i++) {
		switch (sde_prop[i].type) {
		case PROP_TYPE_U32:
			of_property_read_u32(np, sde_prop[i].prop_name,
				&prop_value[i][0]);
			SDE_DEBUG("prop id:%d prop name:%s prop type:%d \"\
				 value:0x%x\n", i, sde_prop[i].prop_name,
				sde_prop[i].type, prop_value[i][0]);
			break;
		case PROP_TYPE_BOOL:
			prop_value[i][0] =  of_property_read_bool(np,
				sde_prop[i].prop_name);
			SDE_DEBUG("prop id:%d prop name:%s prop type:%d \"\
				value:0x%x\n", i, sde_prop[i].prop_name,
				sde_prop[i].type, prop_value[i][0]);
			break;
		case PROP_TYPE_U32_ARRAY:
			rc = _parse_dt_u32_handler(np, sde_prop[i].prop_name,
				prop_value[i], prop_count[i],
				sde_prop[i].is_mandatory);
			if (rc && sde_prop[i].is_mandatory) {
				SDE_ERROR("%s prop validation success but \"\
					read failed\n", sde_prop[i].prop_name);
				goto end;
			} else {
				/* only for debug purpose */
				SDE_DEBUG("prop id:%d prop name:%s prop \"\
					type:%d", i, sde_prop[i].prop_name,
					sde_prop[i].type);
				for (j = 0; j < prop_count[i]; j++)
					SDE_DEBUG(" value[%d]:0x%x ", j,
							prop_value[i][j]);
				SDE_DEBUG("\n");
			}
			break;
		case PROP_TYPE_BIT_OFFSET_ARRAY:
			rc = _parse_dt_bit_offset(np, sde_prop[i].prop_name,
				bit_value[i], prop_count[i],
				sde_prop[i].is_mandatory);
			if (rc && sde_prop[i].is_mandatory) {
				SDE_ERROR("%s prop validation success but \"\
					read failed\n", sde_prop[i].prop_name);
				goto end;
			} else {
				SDE_DEBUG("prop id:%d prop name:%s prop \"\
					type:%d", i, sde_prop[i].prop_name,
					sde_prop[i].type);
				for (j = 0; j < prop_count[i]; j++)
					SDE_DEBUG(" count[%d]: bit:0x%x \"\
					off:0x%x ", j, bit_value[i][j][0],
					bit_value[i][j][1]);
				SDE_DEBUG("\n");
			}
			break;
		default:
			SDE_DEBUG("invalid property type:%d\n",
							sde_prop[i].type);
			break;
		}
		rc = 0;
	}

end:
	return rc;
}

static void _sde_sspp_setup_vig(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	u32 prop_value[][MAX_SDE_HW_BLK], u32 *vig_count)
{
	if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED2) {
		set_bit(SDE_SSPP_SCALER_QSEED2, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED2;
		sblk->scaler_blk.base = prop_value[SSPP_QSEED_OFF][0];
	 } else if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3) {
		set_bit(SDE_SSPP_SCALER_QSEED3, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED3;
		sblk->scaler_blk.base = prop_value[SSPP_QSEED_OFF][0];
	}

	set_bit(SDE_SSPP_CSC, &sspp->features);
	sblk->csc_blk.base = prop_value[SSPP_CSC_OFF][0];
	sblk->csc_blk.id = SDE_SSPP_CSC;

	sblk->maxupscale = MAX_SSPP_UPSCALE;
	sblk->maxdwnscale = MAX_SSPP_DOWNSCALE;
	sspp->id = SSPP_VIG0 + *vig_count;
	sspp->clk_ctrl = SDE_CLK_CTRL_NONE;
	sblk->format_list = plane_formats_yuv;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	(*vig_count)++;
}

static void _sde_sspp_setup_rgb(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	u32 prop_value[][MAX_SDE_HW_BLK], u32 *rgb_count)
{
	if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED2) {
		set_bit(SDE_SSPP_SCALER_RGB, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED2;
		sblk->scaler_blk.base = prop_value[SSPP_QSEED_OFF][0];
	} else if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3) {
		set_bit(SDE_SSPP_SCALER_RGB, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED3;
		sblk->scaler_blk.base = prop_value[SSPP_QSEED_OFF][0];
	}

	sblk->maxupscale = MAX_SSPP_UPSCALE;
	sblk->maxdwnscale = MAX_SSPP_DOWNSCALE;
	sspp->id = SSPP_RGB0 + *rgb_count;
	sspp->clk_ctrl = SDE_CLK_CTRL_NONE;
	sblk->format_list = plane_formats;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	(*rgb_count)++;
}

static void _sde_sspp_setup_cursor(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	u32 prop_value[][MAX_SDE_HW_BLK], u32 *cursor_count)
{
	set_bit(SDE_SSPP_CURSOR, &sspp->features);
	sblk->maxupscale = SSPP_UNITY_SCALE;
	sblk->maxdwnscale = SSPP_UNITY_SCALE;
	sspp->id = SSPP_CURSOR0 + *cursor_count;
	sspp->clk_ctrl = SDE_CLK_CTRL_NONE;
	sblk->format_list = plane_formats;
	(*cursor_count)++;
}

static void _sde_sspp_setup_dma(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	u32 prop_value[][MAX_SDE_HW_BLK], u32 *dma_count)
{
	sblk->maxupscale = SSPP_UNITY_SCALE;
	sblk->maxdwnscale = SSPP_UNITY_SCALE;
	sspp->id = SSPP_DMA0 + *dma_count;
	sspp->clk_ctrl = SDE_CLK_CTRL_NONE;
	sblk->format_list = plane_formats;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	(*dma_count)++;
}

static int sde_sspp_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], off_count, i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK];
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET];
	const char *type;
	struct sde_sspp_cfg *sspp;
	struct sde_sspp_sub_blks *sblk;
	u32 vig_count = 0, dma_count = 0, rgb_count = 0, cursor_count = 0;
	u32 danger_count = 0, safe_count = 0;

	rc = _validate_dt_entry(np, sspp_prop, ARRAY_SIZE(sspp_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &sspp_prop[SSPP_DANGER], 1,
			&prop_count[SSPP_DANGER], &danger_count);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &sspp_prop[SSPP_SAFE], 1,
			&prop_count[SSPP_SAFE], &safe_count);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, sspp_prop, ARRAY_SIZE(sspp_prop), prop_count,
							prop_value, bit_value);
	if (rc)
		goto end;

	sde_cfg->sspp_count = off_count;

	for (i = 0; i < off_count; i++) {
		sspp = sde_cfg->sspp + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		sspp->sblk = sblk;

		sspp->base = prop_value[SSPP_OFF][i];
		sblk->maxlinewidth = sde_cfg->max_sspp_linewidth;

		set_bit(SDE_SSPP_SRC, &sspp->features);
		sblk->src_blk.id = SDE_SSPP_SRC;

		of_property_read_string_index(np,
				sspp_prop[SSPP_TYPE].prop_name, i, &type);
		if (!strcmp(type, "vig")) {
			_sde_sspp_setup_vig(sde_cfg, sspp, sblk, prop_value,
								&vig_count);
		} else if (!strcmp(type, "rgb")) {
			_sde_sspp_setup_rgb(sde_cfg, sspp, sblk, prop_value,
								&rgb_count);
		} else if (!strcmp(type, "cursor")) {
			_sde_sspp_setup_cursor(sde_cfg, sspp, sblk, prop_value,
								&cursor_count);
		} else if (!strcmp(type, "dma")) {
			_sde_sspp_setup_dma(sde_cfg, sspp, sblk, prop_value,
								&dma_count);
		} else {
			SDE_ERROR("invalid sspp type:%s\n", type);
			rc = -EINVAL;
			goto end;
		}

		sblk->maxhdeciexp = MAX_HORZ_DECIMATION;
		sblk->maxvdeciexp = MAX_VERT_DECIMATION;

		sspp->xin_id = prop_value[SSPP_XIN][i];
		sblk->danger_lut_linear = prop_value[SSPP_DANGER][0];
		sblk->danger_lut_tile = prop_value[SSPP_DANGER][1];
		sblk->danger_lut_nrt = prop_value[SSPP_DANGER][2];
		sblk->safe_lut_linear = prop_value[SSPP_SAFE][0];
		sblk->safe_lut_tile = prop_value[SSPP_SAFE][1];
		sblk->safe_lut_nrt = prop_value[SSPP_SAFE][2];
		sblk->creq_lut_nrt = DEFAULT_CREQ_LUT_NRT;
		sblk->pixel_ram_size = DEFAULT_PIXEL_RAM_SIZE;
		sblk->src_blk.len = prop_value[SSPP_SIZE][0];

		SDE_DEBUG(
			"xin:%d danger:%x/%x/%x safe:%x/%x/%x creq:%x ram:%d\n",
			sspp->xin_id,
			sblk->danger_lut_linear,
			sblk->danger_lut_tile,
			sblk->danger_lut_nrt,
			sblk->safe_lut_linear,
			sblk->safe_lut_tile,
			sblk->safe_lut_nrt,
			sblk->creq_lut_nrt,
			sblk->pixel_ram_size);
	}

end:
	return rc;
}

static int sde_ctl_parse_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { {0} };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
				= { { { 0 } } };
	struct sde_ctl_cfg *ctl;
	u32 off_count;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument input param\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, ctl_prop, ARRAY_SIZE(ctl_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->ctl_count = off_count;

	rc = _read_dt_entry(np, ctl_prop, ARRAY_SIZE(ctl_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		ctl = sde_cfg->ctl + i;
		ctl->base = prop_value[HW_OFF][i];
		ctl->id = CTL_0 + i;

		if (i < MAX_SPLIT_DISPLAY_CTL)
			set_bit(SDE_CTL_SPLIT_DISPLAY, &ctl->features);
		if (i < MAX_PP_SPLIT_DISPLAY_CTL)
			set_bit(SDE_CTL_PINGPONG_SPLIT, &ctl->features);
	}

end:
	return rc;
}

static int sde_mixer_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
				= { { { 0 } } };

	u32 off_count, max_blendstages;
	u32 blend_reg_base[] = {0x20, 0x50, 0x80, 0xb0, 0x230, 0x260, 0x290};
	u32 lm_pair_mask[] = {LM_1, LM_0, LM_5, 0x0, 0x0, LM_2};
	struct sde_lm_cfg *mixer;
	struct sde_lm_sub_blks *sblk;
	int pp_count, dspp_count;
	u32 pp_idx, dspp_idx;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument input param\n");
		rc = -EINVAL;
		goto end;
	}
	max_blendstages = sde_cfg->max_mixer_blendstages;

	rc = _validate_dt_entry(np, mixer_prop, ARRAY_SIZE(mixer_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->mixer_count = off_count;

	rc = _read_dt_entry(np, mixer_prop, ARRAY_SIZE(mixer_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	pp_count = sde_cfg->pingpong_count;
	dspp_count = sde_cfg->dspp_count;

	for (i = 0, pp_idx = 0, dspp_idx = 0; i < off_count; i++) {
		mixer = sde_cfg->mixer + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		mixer->sblk = sblk;

		mixer->base = prop_value[HW_OFF][i];
		mixer->len = prop_value[HW_LEN][0];
		mixer->id = LM_0 + i;
		if (!mixer->len)
			mixer->len = DEFAULT_SDE_HW_BLOCK_LEN;

		if (lm_pair_mask[i])
			mixer->lm_pair_mask = 1 << lm_pair_mask[i];

		sblk->maxblendstages = max_blendstages;
		sblk->maxwidth = sde_cfg->max_mixer_width;
		memcpy(sblk->blendstage_base, blend_reg_base, sizeof(u32) *
			min_t(u32, MAX_BLOCKS, min_t(u32,
			ARRAY_SIZE(blend_reg_base), max_blendstages)));
		if (sde_cfg->has_src_split)
			set_bit(SDE_MIXER_SOURCESPLIT, &mixer->features);
		if (prop_value[MIXER_GC][0])
			set_bit(SDE_MIXER_GC, &mixer->features);

		if ((i < ROT_LM_OFFSET) || (i >= LINE_LM_OFFSET)) {
			mixer->pingpong = pp_count > 0 ? pp_idx + PINGPONG_0
								: PINGPONG_MAX;
			mixer->dspp = dspp_count > 0 ? dspp_idx + DSPP_0
								: DSPP_MAX;
			pp_count--;
			dspp_count--;
			pp_idx++;
			dspp_idx++;
		} else {
			mixer->pingpong = PINGPONG_MAX;
			mixer->dspp = DSPP_MAX;
		}
	}

end:
	return rc;
}

static int sde_intf_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
					= { { { 0 } } };
	u32 off_count;
	u32 dsi_count = 0, none_count = 0, hdmi_count = 0, dp_count = 0;
	const char *type;
	struct sde_intf_cfg *intf;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, intf_prop, ARRAY_SIZE(intf_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->intf_count = off_count;

	rc = _read_dt_entry(np, intf_prop, ARRAY_SIZE(intf_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		intf = sde_cfg->intf + i;
		intf->base = prop_value[INTF_OFF][i];
		intf->len = prop_value[INTF_LEN][0];
		intf->id = INTF_0 + i;
		if (!intf->len)
			intf->len = DEFAULT_SDE_HW_BLOCK_LEN;

		intf->prog_fetch_lines_worst_case =
					prop_value[INTF_PREFETCH][i];

		of_property_read_string_index(np,
				intf_prop[INTF_TYPE].prop_name, i, &type);
		if (!strcmp(type, "dsi")) {
			intf->type = INTF_DSI;
			intf->controller_id = dsi_count;
			dsi_count++;
		} else if (!strcmp(type, "hdmi")) {
			intf->type = INTF_HDMI;
			intf->controller_id = hdmi_count;
			hdmi_count++;
		} else if (!strcmp(type, "dp")) {
			intf->type = INTF_DP;
			intf->controller_id = dp_count;
			dp_count++;
		} else {
			intf->type = INTF_NONE;
			intf->controller_id = none_count;
			none_count++;
		}
	}

end:
	return rc;
}

static int sde_wb_parse_dt(struct device_node *np, struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
					= { { { 0 } } };
	u32 off_count;
	struct sde_wb_cfg *wb;
	struct sde_wb_sub_blocks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, wb_prop, ARRAY_SIZE(wb_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->wb_count = off_count;

	rc = _read_dt_entry(np, wb_prop, ARRAY_SIZE(wb_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		wb = sde_cfg->wb + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		wb->sblk = sblk;

		wb->base = prop_value[WB_OFF][i];
		wb->id = WB_0 + prop_value[WB_ID][i];
		wb->clk_ctrl = SDE_CLK_CTRL_WB0 + prop_value[WB_ID][i];
		wb->xin_id = prop_value[WB_XIN_ID][i];
		wb->vbif_idx = VBIF_NRT;
		wb->len = prop_value[WB_LEN][0];
		wb->format_list = wb2_formats;
		if (!wb->len)
			wb->len = DEFAULT_SDE_HW_BLOCK_LEN;
		sblk->maxlinewidth = sde_cfg->max_wb_linewidth;

		if (wb->id >= LINE_MODE_WB_OFFSET)
			set_bit(SDE_WB_LINE_MODE, &wb->features);
		else
			set_bit(SDE_WB_BLOCK_MODE, &wb->features);
		set_bit(SDE_WB_TRAFFIC_SHAPER, &wb->features);
		set_bit(SDE_WB_YUV_CONFIG, &wb->features);
	}

end:
	return rc;
}

static int sde_dspp_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
				= { { { 0 } } };
	u32 off_count;
	struct sde_dspp_cfg *dspp;
	struct sde_dspp_sub_blks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, dspp_prop, ARRAY_SIZE(dspp_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->dspp_count = off_count;

	rc = _read_dt_entry(np, dspp_prop, ARRAY_SIZE(dspp_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		dspp = sde_cfg->dspp + i;
		dspp->base = prop_value[DSPP_OFF][i];
		dspp->id = DSPP_0 + i;

		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		dspp->sblk = sblk;

		sblk->igc.base = prop_value[DSPP_IGC][0];
		if (sblk->igc.base)
			set_bit(SDE_DSPP_IGC, &dspp->features);

		sblk->pcc.base = prop_value[DSPP_PCC][0];
		if (sblk->pcc.base)
			set_bit(SDE_DSPP_PCC, &dspp->features);

		sblk->gc.base = prop_value[DSPP_GC][0];
		if (sblk->gc.base)
			set_bit(SDE_DSPP_GC, &dspp->features);

		sblk->gamut.base = prop_value[DSPP_GAMUT][0];
		if (sblk->gamut.base)
			set_bit(SDE_DSPP_GAMUT, &dspp->features);

		sblk->dither.base = prop_value[DSPP_DITHER][0];
		if (sblk->dither.base)
			set_bit(SDE_DSPP_DITHER, &dspp->features);

		sblk->hist.base = prop_value[DSPP_HIST][0];
		if (sblk->hist.base)
			set_bit(SDE_DSPP_HIST, &dspp->features);

		sblk->ad.base = prop_value[DSPP_AD][i];
		if (sblk->ad.base)
			set_bit(SDE_DSPP_AD, &dspp->features);
	}

end:
	return rc;
}

static int sde_cdm_parse_dt(struct device_node *np,
				struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
				= { { { 0 } } };
	u32 off_count;
	struct sde_cdm_cfg *cdm;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, cdm_prop, ARRAY_SIZE(cdm_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->cdm_count = off_count;

	rc = _read_dt_entry(np, cdm_prop, ARRAY_SIZE(cdm_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		cdm = sde_cfg->cdm + i;
		cdm->base = prop_value[HW_OFF][i];
		cdm->id = CDM_0 + i;
		cdm->len = prop_value[HW_LEN][0];

		/* intf3 and wb2 for cdm block */
		cdm->wb_connect = sde_cfg->wb_count ? BIT(WB_2) : BIT(31);
		cdm->intf_connect = sde_cfg->intf_count ? BIT(INTF_3) : BIT(31);
	}

end:
	return rc;
}

static int sde_vbif_parse_dt(struct device_node *np,
				struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i, j, k;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
				= { { { 0 } } };
	u32 off_count, vbif_len, rd_len = 0, wr_len = 0;
	struct sde_vbif_cfg *vbif;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, vbif_prop, ARRAY_SIZE(vbif_prop),
			prop_count, &off_count);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_DYNAMIC_OT_RD_LIMIT], 1,
			&prop_count[VBIF_DYNAMIC_OT_RD_LIMIT], &rd_len);
	if (rc)
		goto end;

	rc = _validate_dt_entry(np, &vbif_prop[VBIF_DYNAMIC_OT_WR_LIMIT], 1,
			&prop_count[VBIF_DYNAMIC_OT_WR_LIMIT], &wr_len);
	if (rc)
		goto end;

	sde_cfg->vbif_count = off_count;

	rc = _read_dt_entry(np, vbif_prop, ARRAY_SIZE(vbif_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	vbif_len = prop_value[VBIF_LEN][0];
	if (!vbif_len)
		vbif_len = DEFAULT_SDE_HW_BLOCK_LEN;

	for (i = 0; i < off_count; i++) {
		vbif = sde_cfg->vbif + i;
		vbif->base = prop_value[VBIF_OFF][i];
		vbif->len = vbif_len;
		vbif->id = VBIF_0 + prop_value[VBIF_ID][i];

		SDE_DEBUG("vbif:%d\n", vbif->id - VBIF_0);

		vbif->xin_halt_timeout = VBIF_XIN_HALT_TIMEOUT;

		vbif->default_ot_rd_limit =
				prop_value[VBIF_DEFAULT_OT_RD_LIMIT][0];
		SDE_DEBUG("default_ot_rd_limit=%u\n",
				vbif->default_ot_rd_limit);

		vbif->default_ot_wr_limit =
				prop_value[VBIF_DEFAULT_OT_WR_LIMIT][0];
		SDE_DEBUG("default_ot_wr_limit=%u\n",
				vbif->default_ot_wr_limit);

		vbif->dynamic_ot_rd_tbl.count =
				prop_count[VBIF_DYNAMIC_OT_RD_LIMIT] / 2;
		SDE_DEBUG("dynamic_ot_rd_tbl.count=%u\n",
				vbif->dynamic_ot_rd_tbl.count);
		if (vbif->dynamic_ot_rd_tbl.count) {
			vbif->dynamic_ot_rd_tbl.cfg = kcalloc(
				vbif->dynamic_ot_rd_tbl.count,
				sizeof(struct sde_vbif_dynamic_ot_cfg),
				GFP_KERNEL);
			if (!vbif->dynamic_ot_rd_tbl.cfg) {
				rc = -ENOMEM;
				goto end;
			}
		}

		for (j = 0, k = 0; j < vbif->dynamic_ot_rd_tbl.count; j++) {
			vbif->dynamic_ot_rd_tbl.cfg[j].pps = (u64)
				prop_value[VBIF_DYNAMIC_OT_RD_LIMIT][k++];
			vbif->dynamic_ot_rd_tbl.cfg[j].ot_limit =
				prop_value[VBIF_DYNAMIC_OT_RD_LIMIT][k++];
			SDE_DEBUG("dynamic_ot_rd_tbl[%d].cfg=<%llu %u>\n", j,
				vbif->dynamic_ot_rd_tbl.cfg[j].pps,
				vbif->dynamic_ot_rd_tbl.cfg[j].ot_limit);
		}

		vbif->dynamic_ot_wr_tbl.count =
				prop_count[VBIF_DYNAMIC_OT_WR_LIMIT] / 2;
		SDE_DEBUG("dynamic_ot_wr_tbl.count=%u\n",
				vbif->dynamic_ot_wr_tbl.count);
		if (vbif->dynamic_ot_wr_tbl.count) {
			vbif->dynamic_ot_wr_tbl.cfg = kcalloc(
				vbif->dynamic_ot_wr_tbl.count,
				sizeof(struct sde_vbif_dynamic_ot_cfg),
				GFP_KERNEL);
			if (!vbif->dynamic_ot_wr_tbl.cfg) {
				rc = -ENOMEM;
				goto end;
			}
		}

		for (j = 0, k = 0; j < vbif->dynamic_ot_wr_tbl.count; j++) {
			vbif->dynamic_ot_wr_tbl.cfg[j].pps = (u64)
				prop_value[VBIF_DYNAMIC_OT_WR_LIMIT][k++];
			vbif->dynamic_ot_wr_tbl.cfg[j].ot_limit =
				prop_value[VBIF_DYNAMIC_OT_WR_LIMIT][k++];
			SDE_DEBUG("dynamic_ot_wr_tbl[%d].cfg=<%llu %u>\n", j,
				vbif->dynamic_ot_wr_tbl.cfg[j].pps,
				vbif->dynamic_ot_wr_tbl.cfg[j].ot_limit);
		}

		if (vbif->default_ot_rd_limit || vbif->default_ot_wr_limit ||
				vbif->dynamic_ot_rd_tbl.count ||
				vbif->dynamic_ot_wr_tbl.count)
			set_bit(SDE_VBIF_QOS_OTLIM, &vbif->features);
	}

end:
	return rc;
}

static int sde_pp_parse_dt(struct device_node *np, struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MAX_BLOCKS], i;
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { { 0 } };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
				= { { { 0 } } };
	u32 off_count;
	struct sde_pingpong_cfg *pp;
	struct sde_pingpong_sub_blks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, pp_prop, ARRAY_SIZE(pp_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->pingpong_count = off_count;

	rc = _read_dt_entry(np, pp_prop, ARRAY_SIZE(pp_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		pp = sde_cfg->pingpong + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		pp->sblk = sblk;

		pp->base = prop_value[PP_OFF][i];
		pp->id = PINGPONG_0 + i;
		pp->len = prop_value[PP_LEN][0];

		sblk->te.base = prop_value[TE_OFF][i];
		sblk->te.id = SDE_PINGPONG_TE;
		set_bit(SDE_PINGPONG_TE, &pp->features);

		sblk->te2.base = prop_value[TE2_OFF][i];
		if (sblk->te2.base) {
			sblk->te2.id = SDE_PINGPONG_TE2;
			set_bit(SDE_PINGPONG_TE2, &pp->features);
			set_bit(SDE_PINGPONG_SPLIT, &pp->features);
		}

		sblk->dsc.base = prop_value[DSC_OFF][i];
		if (sblk->dsc.base) {
			sblk->dsc.id = SDE_PINGPONG_DSC;
			set_bit(SDE_PINGPONG_DSC, &pp->features);
		}
	}

end:
	return rc;
}

static int sde_parse_dt(struct device_node *np, struct sde_mdss_cfg *cfg)
{
	int rc, len, prop_count[MAX_BLOCKS];
	u32 prop_value[MAX_BLOCKS][MAX_SDE_HW_BLK] = { {0} };
	u32 bit_value[MAX_BLOCKS][MAX_SDE_HW_BLK][MAX_BIT_OFFSET]
			= { { { 0 } } };
	const char *type;

	if (!cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	rc = _validate_dt_entry(np, sde_prop, ARRAY_SIZE(sde_prop), prop_count,
		&len);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, sde_prop, ARRAY_SIZE(sde_prop), prop_count,
		prop_value, bit_value);
	if (rc)
		goto end;

	cfg->mdss_count = 1;
	cfg->mdss[0].base = MDSS_BASE_OFFSET;
	cfg->mdss[0].id = MDP_TOP;

	cfg->mdp_count = 1;
	cfg->mdp[0].id = MDP_TOP;
	cfg->mdp[0].base = prop_value[SDE_OFF][0];
	cfg->mdp[0].len = prop_value[SDE_LEN][0];
	if (!cfg->mdp[0].len)
		cfg->mdp[0].len = DEFAULT_SDE_HW_BLOCK_LEN;

	cfg->max_sspp_linewidth = prop_value[SSPP_LINEWIDTH][0];
	if (!cfg->max_sspp_linewidth)
		cfg->max_sspp_linewidth = DEFAULT_SDE_LINE_WIDTH;

	cfg->max_mixer_width = prop_value[MIXER_LINEWIDTH][0];
	if (!cfg->max_mixer_width)
		cfg->max_mixer_width = DEFAULT_SDE_LINE_WIDTH;

	cfg->max_mixer_blendstages = prop_value[MIXER_BLEND][0];
	if (!cfg->max_mixer_blendstages)
		cfg->max_mixer_blendstages = DEFAULT_SDE_MIXER_BLENDSTAGES;

	cfg->max_wb_linewidth = prop_value[WB_LINEWIDTH][0];
	if (!cfg->max_wb_linewidth)
		cfg->max_wb_linewidth = DEFAULT_SDE_LINE_WIDTH;

	cfg->mdp[0].highest_bank_bit = prop_value[BANK_BIT][0];
	if (!cfg->mdp[0].highest_bank_bit)
		cfg->mdp[0].highest_bank_bit = DEFAULT_SDE_HIGHEST_BANK_BIT;

	rc = of_property_read_string(np, sde_prop[QSEED_TYPE].prop_name, &type);
	if (!rc && !strcmp(type, "qseedv3"))
		cfg->qseed_type = SDE_SSPP_SCALER_QSEED3;
	else if (!rc && !strcmp(type, "qseedv2"))
		cfg->qseed_type = SDE_SSPP_SCALER_QSEED2;

	cfg->has_src_split = prop_value[SRC_SPLIT][0];
end:
	return rc;
}

static void sde_hardware_caps(struct sde_mdss_cfg *sde_cfg, uint32_t hw_rev)
{
	switch (hw_rev) {
	case SDE_HW_VER_170:
	case SDE_HW_VER_171:
	case SDE_HW_VER_172:
		/* update msm8996 target here */
		break;
	case SDE_HW_VER_300:
	case SDE_HW_VER_400:
		/* update cobalt and skunk target here */
		break;
	}
}

static void sde_hw_catalog_deinit(struct sde_mdss_cfg *sde_cfg)
{
	int i;

	if (!sde_cfg)
		return;

	for (i = 0; i < sde_cfg->sspp_count; i++)
		kfree(sde_cfg->sspp[i].sblk);

	for (i = 0; i < sde_cfg->mixer_count; i++)
		kfree(sde_cfg->mixer[i].sblk);

	for (i = 0; i < sde_cfg->wb_count; i++)
		kfree(sde_cfg->wb[i].sblk);

	for (i = 0; i < sde_cfg->dspp_count; i++)
		kfree(sde_cfg->dspp[i].sblk);

	for (i = 0; i < sde_cfg->pingpong_count; i++)
		kfree(sde_cfg->pingpong[i].sblk);

	for (i = 0; i < sde_cfg->vbif_count; i++) {
		kfree(sde_cfg->vbif[i].dynamic_ot_rd_tbl.cfg);
		kfree(sde_cfg->vbif[i].dynamic_ot_wr_tbl.cfg);
	}
}

/*************************************************************
 * hardware catalog init
 *************************************************************/
struct sde_mdss_cfg *sde_hw_catalog_init(struct drm_device *dev, u32 hw_rev)
{
	int rc;
	struct sde_mdss_cfg *sde_cfg;
	struct device_node *np = dev->dev->of_node;

	sde_cfg = kzalloc(sizeof(*sde_cfg), GFP_KERNEL);
	if (!sde_cfg)
		return ERR_PTR(-ENOMEM);

	sde_cfg->hwversion = hw_rev;

	rc = sde_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_ctl_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_sspp_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_dspp_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_pp_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	/* mixer parsing should be done after dspp and pp for mapping setup */
	rc = sde_mixer_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_intf_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_wb_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	/* cdm parsing should be done after intf and wb for mapping setup */
	rc = sde_cdm_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = sde_vbif_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	sde_hardware_caps(sde_cfg, hw_rev);

	return sde_cfg;

end:
	sde_hw_catalog_deinit(sde_cfg);
	kfree(sde_cfg);
	return NULL;
}

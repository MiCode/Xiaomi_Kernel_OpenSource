/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include <linux/of_platform.h>

#include "sde_hw_mdss.h"
#include "sde_hw_catalog.h"
#include "sde_hw_catalog_format.h"
#include "sde_kms.h"

/*************************************************************
 * MACRO DEFINITION
 *************************************************************/

/**
 * Max hardware block in certain hardware. For ex: sspp pipes
 * can have QSEED, pcc, igc, pa, csc, qos entries, etc. This count is
 * 64 based on software design. It should be increased if any of the
 * hardware block has more subblocks.
 */
#define MAX_SDE_HW_BLK  64

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

/* access property value based on prop_type and hardware index */
#define PROP_VALUE_ACCESS(p, i, j)		((p + i)->value[j])

/*
 * access element within PROP_TYPE_BIT_OFFSET_ARRAYs based on prop_type,
 * hardware index and offset array index
 */
#define PROP_BITVALUE_ACCESS(p, i, j, k)	((p + i)->bit_value[j][k])

/*************************************************************
 *  DTSI PROPERTY INDEX
 *************************************************************/
enum {
	HW_OFF,
	HW_LEN,
	HW_PROP_MAX,
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
	CSC_TYPE,
	PANIC_PER_PIPE,
	CDP,
	SRC_SPLIT,
	SDE_PROP_MAX,
};

enum {
	PERF_MAX_BW_LOW,
	PERF_MAX_BW_HIGH,
	PERF_PROP_MAX,
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
	SSPP_VIG_BLOCKS,
	SSPP_RGB_BLOCKS,
	SSPP_PROP_MAX,
};

enum {
	VIG_QSEED_OFF,
	VIG_QSEED_LEN,
	VIG_CSC_OFF,
	VIG_HSIC_PROP,
	VIG_MEMCOLOR_PROP,
	VIG_PCC_PROP,
	VIG_PROP_MAX,
};

enum {
	RGB_SCALER_OFF,
	RGB_SCALER_LEN,
	RGB_PCC_PROP,
	RGB_PROP_MAX,
};

enum {
	INTF_OFF,
	INTF_LEN,
	INTF_PREFETCH,
	INTF_TYPE,
	INTF_PROP_MAX,
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
	PP_SLAVE,
	PP_PROP_MAX,
};

enum {
	DSPP_OFF,
	DSPP_SIZE,
	DSPP_BLOCKS,
	DSPP_PROP_MAX,
};

enum {
	DSPP_IGC_PROP,
	DSPP_PCC_PROP,
	DSPP_GC_PROP,
	DSPP_HSIC_PROP,
	DSPP_MEMCOLOR_PROP,
	DSPP_SIXZONE_PROP,
	DSPP_GAMUT_PROP,
	DSPP_DITHER_PROP,
	DSPP_HIST_PROP,
	DSPP_VLUT_PROP,
	DSPP_BLOCKS_PROP_MAX,
};

enum {
	AD_OFF,
	AD_VERSION,
	AD_PROP_MAX,
};

enum {
	MIXER_OFF,
	MIXER_LEN,
	MIXER_BLOCKS,
	MIXER_PROP_MAX,
};

enum {
	MIXER_GC_PROP,
	MIXER_BLOCKS_PROP_MAX,
};

enum {
	WB_OFF,
	WB_LEN,
	WB_ID,
	WB_XIN_ID,
	WB_CLK_CTRL,
	WB_PROP_MAX,
};

enum {
	VBIF_OFF,
	VBIF_LEN,
	VBIF_ID,
	VBIF_DEFAULT_OT_RD_LIMIT,
	VBIF_DEFAULT_OT_WR_LIMIT,
	VBIF_DYNAMIC_OT_RD_LIMIT,
	VBIF_DYNAMIC_OT_WR_LIMIT,
	VBIF_PROP_MAX,
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
	PROP_TYPE_NODE,
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

struct sde_prop_value {
	u32 value[MAX_SDE_HW_BLK];
	u32 bit_value[MAX_SDE_HW_BLK][MAX_BIT_OFFSET];
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
	{CSC_TYPE, "qcom,sde-csc-type", false, PROP_TYPE_STRING},
	{PANIC_PER_PIPE, "qcom,sde-panic-per-pipe", false, PROP_TYPE_BOOL},
	{CDP, "qcom,sde-has-cdp", false, PROP_TYPE_BOOL},
	{SRC_SPLIT, "qcom,sde-has-src-split", false, PROP_TYPE_BOOL},
};

static struct sde_prop_type sde_perf_prop[] = {
	{PERF_MAX_BW_LOW, "qcom,sde-max-bw-low-kbps", false, PROP_TYPE_U32},
	{PERF_MAX_BW_HIGH, "qcom,sde-max-bw-high-kbps", false, PROP_TYPE_U32},
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
	{SSPP_VIG_BLOCKS, "qcom,sde-sspp-vig-blocks", false, PROP_TYPE_NODE},
	{SSPP_RGB_BLOCKS, "qcom,sde-sspp-rgb-blocks", false, PROP_TYPE_NODE},
};

static struct sde_prop_type vig_prop[] = {
	{VIG_QSEED_OFF, "qcom,sde-vig-qseed-off", false, PROP_TYPE_U32},
	{VIG_QSEED_LEN, "qcom,sde-vig-qseed-size", false, PROP_TYPE_U32},
	{VIG_CSC_OFF, "qcom,sde-vig-csc-off", false, PROP_TYPE_U32},
	{VIG_HSIC_PROP, "qcom,sde-vig-hsic", false, PROP_TYPE_U32_ARRAY},
	{VIG_MEMCOLOR_PROP, "qcom,sde-vig-memcolor", false,
		PROP_TYPE_U32_ARRAY},
	{VIG_PCC_PROP, "qcom,sde-vig-pcc", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type rgb_prop[] = {
	{RGB_SCALER_OFF, "qcom,sde-rgb-scaler-off", false, PROP_TYPE_U32},
	{RGB_SCALER_LEN, "qcom,sde-rgb-scaler-size", false, PROP_TYPE_U32},
	{RGB_PCC_PROP, "qcom,sde-rgb-pcc", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type ctl_prop[] = {
	{HW_OFF, "qcom,sde-ctl-off", true, PROP_TYPE_U32_ARRAY},
	{HW_LEN, "qcom,sde-ctl-size", false, PROP_TYPE_U32},
};

static struct sde_prop_type mixer_prop[] = {
	{MIXER_OFF, "qcom,sde-mixer-off", true, PROP_TYPE_U32_ARRAY},
	{MIXER_LEN, "qcom,sde-mixer-size", false, PROP_TYPE_U32},
	{MIXER_BLOCKS, "qcom,sde-mixer-blocks", false, PROP_TYPE_NODE},
};

static struct sde_prop_type mixer_blocks_prop[] = {
	{MIXER_GC_PROP, "qcom,sde-mixer-gc", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type dspp_prop[] = {
	{DSPP_OFF, "qcom,sde-dspp-off", true, PROP_TYPE_U32_ARRAY},
	{DSPP_SIZE, "qcom,sde-dspp-size", false, PROP_TYPE_U32},
	{DSPP_BLOCKS, "qcom,sde-dspp-blocks", false, PROP_TYPE_NODE},
};

static struct sde_prop_type dspp_blocks_prop[] = {
	{DSPP_IGC_PROP, "qcom,sde-dspp-igc", false, PROP_TYPE_U32_ARRAY},
	{DSPP_PCC_PROP, "qcom,sde-dspp-pcc", false, PROP_TYPE_U32_ARRAY},
	{DSPP_GC_PROP, "qcom,sde-dspp-gc", false, PROP_TYPE_U32_ARRAY},
	{DSPP_HSIC_PROP, "qcom,sde-dspp-hsic", false, PROP_TYPE_U32_ARRAY},
	{DSPP_MEMCOLOR_PROP, "qcom,sde-dspp-memcolor", false,
		PROP_TYPE_U32_ARRAY},
	{DSPP_SIXZONE_PROP, "qcom,sde-dspp-sixzone", false,
		PROP_TYPE_U32_ARRAY},
	{DSPP_GAMUT_PROP, "qcom,sde-dspp-gamut", false, PROP_TYPE_U32_ARRAY},
	{DSPP_DITHER_PROP, "qcom,sde-dspp-dither", false, PROP_TYPE_U32_ARRAY},
	{DSPP_HIST_PROP, "qcom,sde-dspp-hist", false, PROP_TYPE_U32_ARRAY},
	{DSPP_VLUT_PROP, "qcom,sde-dspp-vlut", false, PROP_TYPE_U32_ARRAY},
};

static struct sde_prop_type ad_prop[] = {
	{AD_OFF, "qcom,sde-dspp-ad-off", false, PROP_TYPE_U32_ARRAY},
	{AD_VERSION, "qcom,sde-dspp-ad-version", false, PROP_TYPE_U32},
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
	{PP_SLAVE, "qcom,sde-pp-slave", false, PROP_TYPE_U32_ARRAY},
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
	{WB_CLK_CTRL, "qcom,sde-wb-clk-ctrl", false,
		PROP_TYPE_BIT_OFFSET_ARRAY},
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

/**
 * _sde_copy_formats   - copy formats from src_list to dst_list
 * @dst_list:          pointer to destination list where to copy formats
 * @dst_list_size:     size of destination list
 * @dst_list_pos:      starting position on the list where to copy formats
 * @src_list:          pointer to source list where to copy formats from
 * @src_list_size:     size of source list
 * Return: number of elements populated
 */
static uint32_t _sde_copy_formats(
		struct sde_format_extended *dst_list,
		uint32_t dst_list_size,
		uint32_t dst_list_pos,
		const struct sde_format_extended *src_list,
		uint32_t src_list_size)
{
	uint32_t cur_pos, i;

	if (!dst_list || !src_list || (dst_list_pos >= (dst_list_size - 1)))
		return 0;

	for (i = 0, cur_pos = dst_list_pos;
		(cur_pos < (dst_list_size - 1)) && (i < src_list_size)
		&& src_list[i].fourcc_format; ++i, ++cur_pos)
		dst_list[cur_pos] = src_list[i];

	dst_list[cur_pos].fourcc_format = 0;

	return i;
}

static int _parse_dt_u32_handler(struct device_node *np,
	char *prop_name, u32 *offsets, int len, bool mandatory)
{
	int rc = -EINVAL;

	if (len > MAX_SDE_HW_BLK) {
		SDE_ERROR(
			"prop: %s tries out of bound access for u32 array read len: %d\n",
				prop_name, len);
		return -E2BIG;
	}

	rc = of_property_read_u32_array(np, prop_name, offsets, len);
	if (rc && mandatory)
		SDE_ERROR("mandatory prop: %s u32 array read len:%d\n",
				prop_name, len);
	else if (rc)
		SDE_DEBUG("optional prop: %s u32 array read len:%d\n",
				prop_name, len);

	return rc;
}

static int _parse_dt_bit_offset(struct device_node *np,
	char *prop_name, struct sde_prop_value *prop_value, u32 prop_index,
	u32 count, bool mandatory)
{
	int rc = 0, len, i, j;
	const u32 *arr;

	arr = of_get_property(np, prop_name, &len);
	if (arr) {
		len /= sizeof(u32);
		len &= ~0x1;

		if (len > (MAX_SDE_HW_BLK * MAX_BIT_OFFSET)) {
			SDE_ERROR(
				"prop: %s len: %d will lead to out of bound access\n",
				prop_name, len / MAX_BIT_OFFSET);
			return -E2BIG;
		}

		for (i = 0, j = 0; i < len; j++) {
			PROP_BITVALUE_ACCESS(prop_value, prop_index, j, 0) =
				be32_to_cpu(arr[i]);
			i++;
			PROP_BITVALUE_ACCESS(prop_value, prop_index, j, 1) =
				be32_to_cpu(arr[i]);
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
	struct device_node *snp = NULL;

	if (off_count) {
		*off_count = of_property_count_u32_elems(np,
				sde_prop[0].prop_name);
		if ((*off_count > MAX_BLOCKS) || (*off_count < 0)) {
			if (sde_prop[0].is_mandatory) {
				SDE_ERROR(
					"invalid hw offset prop name:%s count: %d\n",
					sde_prop[0].prop_name, *off_count);
				rc = -EINVAL;
			}
			*off_count = 0;
			memset(prop_count, 0, sizeof(int) * prop_size);
			return rc;
		}
	}

	for (i = 0; i < prop_size; i++) {
		switch (sde_prop[i].type) {
		case PROP_TYPE_U32:
			rc = of_property_read_u32(np, sde_prop[i].prop_name,
				&val);
			break;
		case PROP_TYPE_U32_ARRAY:
			prop_count[i] = of_property_count_u32_elems(np,
				sde_prop[i].prop_name);
			if (prop_count[i] < 0)
				rc = prop_count[i];
			break;
		case PROP_TYPE_STRING_ARRAY:
			prop_count[i] = of_property_count_strings(np,
				sde_prop[i].prop_name);
			if (prop_count[i] < 0)
				rc = prop_count[i];
			break;
		case PROP_TYPE_BIT_OFFSET_ARRAY:
			of_get_property(np, sde_prop[i].prop_name, &val);
			prop_count[i] = val / (MAX_BIT_OFFSET * sizeof(u32));
			break;
		case PROP_TYPE_NODE:
			snp = of_get_child_by_name(np,
					sde_prop[i].prop_name);
			if (!snp)
				rc = -EINVAL;
			break;
		default:
			SDE_DEBUG("invalid property type:%d\n",
							sde_prop[i].type);
			break;
		}
		SDE_DEBUG(
			"prop id:%d prop name:%s prop type:%d prop_count:%d\n",
			i, sde_prop[i].prop_name,
			sde_prop[i].type, prop_count[i]);

		if (rc && sde_prop[i].is_mandatory &&
		   ((sde_prop[i].type == PROP_TYPE_U32) ||
		    (sde_prop[i].type == PROP_TYPE_NODE))) {
			SDE_ERROR("prop:%s not present\n",
						sde_prop[i].prop_name);
			goto end;
		} else if (sde_prop[i].type == PROP_TYPE_U32 ||
			sde_prop[i].type == PROP_TYPE_BOOL ||
			sde_prop[i].type == PROP_TYPE_NODE) {
			rc = 0;
			continue;
		}

		if (off_count && (prop_count[i] != *off_count) &&
				sde_prop[i].is_mandatory) {
			SDE_ERROR(
				"prop:%s count:%d is different compared to offset array:%d\n",
				sde_prop[i].prop_name,
				prop_count[i], *off_count);
			rc = -EINVAL;
			goto end;
		} else if (off_count && prop_count[i] != *off_count) {
			SDE_DEBUG(
				"prop:%s count:%d is different compared to offset array:%d\n",
				sde_prop[i].prop_name,
				prop_count[i], *off_count);
			rc = 0;
			prop_count[i] = 0;
		}
		if (!off_count && prop_count[i] < 0) {
			prop_count[i] = 0;
			if (sde_prop[i].is_mandatory) {
				SDE_ERROR("prop:%s count:%d is negative\n",
					sde_prop[i].prop_name, prop_count[i]);
				rc = -EINVAL;
			} else {
				rc = 0;
				SDE_DEBUG("prop:%s count:%d is negative\n",
					sde_prop[i].prop_name, prop_count[i]);
			}
		}
	}

end:
	return rc;
}

static int _read_dt_entry(struct device_node *np,
	struct sde_prop_type *sde_prop, u32 prop_size, int *prop_count,
	bool *prop_exists,
	struct sde_prop_value *prop_value)
{
	int rc = 0, i, j;

	for (i = 0; i < prop_size; i++) {
		prop_exists[i] = true;
		switch (sde_prop[i].type) {
		case PROP_TYPE_U32:
			rc = of_property_read_u32(np, sde_prop[i].prop_name,
				&PROP_VALUE_ACCESS(prop_value, i, 0));
			SDE_DEBUG(
				"prop id:%d prop name:%s prop type:%d value:0x%x\n",
				i, sde_prop[i].prop_name,
				sde_prop[i].type,
				PROP_VALUE_ACCESS(prop_value, i, 0));
			if (rc)
				prop_exists[i] = false;
			break;
		case PROP_TYPE_BOOL:
			PROP_VALUE_ACCESS(prop_value, i, 0) =
				of_property_read_bool(np,
					sde_prop[i].prop_name);
			SDE_DEBUG(
				"prop id:%d prop name:%s prop type:%d value:0x%x\n",
				i, sde_prop[i].prop_name,
				sde_prop[i].type,
				PROP_VALUE_ACCESS(prop_value, i, 0));
			break;
		case PROP_TYPE_U32_ARRAY:
			rc = _parse_dt_u32_handler(np, sde_prop[i].prop_name,
				&PROP_VALUE_ACCESS(prop_value, i, 0),
				prop_count[i], sde_prop[i].is_mandatory);
			if (rc && sde_prop[i].is_mandatory) {
				SDE_ERROR(
					"%s prop validation success but read failed\n",
					sde_prop[i].prop_name);
				prop_exists[i] = false;
				goto end;
			} else {
				if (rc)
					prop_exists[i] = false;
				/* only for debug purpose */
				SDE_DEBUG("prop id:%d prop name:%s prop \"\
					type:%d", i, sde_prop[i].prop_name,
					sde_prop[i].type);
				for (j = 0; j < prop_count[i]; j++)
					SDE_DEBUG(" value[%d]:0x%x ", j,
						PROP_VALUE_ACCESS(prop_value, i,
								j));
				SDE_DEBUG("\n");
			}
			break;
		case PROP_TYPE_BIT_OFFSET_ARRAY:
			rc = _parse_dt_bit_offset(np, sde_prop[i].prop_name,
				prop_value, i, prop_count[i],
				sde_prop[i].is_mandatory);
			if (rc && sde_prop[i].is_mandatory) {
				SDE_ERROR(
					"%s prop validation success but read failed\n",
					sde_prop[i].prop_name);
				prop_exists[i] = false;
				goto end;
			} else {
				if (rc)
					prop_exists[i] = false;
				SDE_DEBUG(
					"prop id:%d prop name:%s prop type:%d",
					i, sde_prop[i].prop_name,
					sde_prop[i].type);
				for (j = 0; j < prop_count[i]; j++)
					SDE_DEBUG(
					"count[%d]: bit:0x%x off:0x%x\n", j,
					PROP_BITVALUE_ACCESS(prop_value,
						i, j, 0),
					PROP_BITVALUE_ACCESS(prop_value,
						i, j, 1));
				SDE_DEBUG("\n");
			}
			break;
		case PROP_TYPE_NODE:
			/* Node will be parsed in calling function */
			rc = 0;
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
	bool *prop_exists, struct sde_prop_value *prop_value, u32 *vig_count)
{
	sblk->maxupscale = MAX_SSPP_UPSCALE;
	sblk->maxdwnscale = MAX_SSPP_DOWNSCALE;
	sblk->format_list = plane_formats_yuv;
	sspp->id = SSPP_VIG0 + *vig_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->clk_ctrl = SDE_CLK_CTRL_VIG0 + *vig_count;
	sspp->type = SSPP_TYPE_VIG;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	(*vig_count)++;

	if (!prop_value)
		return;

	if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED2) {
		set_bit(SDE_SSPP_SCALER_QSEED2, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED2;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_OFF, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_LEN, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
				"sspp_scaler%u", sspp->id - SSPP_VIG0);
	} else if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3) {
		set_bit(SDE_SSPP_SCALER_QSEED3, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED3;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_OFF, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			VIG_QSEED_LEN, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_scaler%u", sspp->id - SSPP_VIG0);
	}

	sblk->csc_blk.id = SDE_SSPP_CSC;
	snprintf(sblk->csc_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_csc%u", sspp->id - SSPP_VIG0);
	if (sde_cfg->csc_type == SDE_SSPP_CSC) {
		set_bit(SDE_SSPP_CSC, &sspp->features);
		sblk->csc_blk.base = PROP_VALUE_ACCESS(prop_value,
							VIG_CSC_OFF, 0);
	} else if (sde_cfg->csc_type == SDE_SSPP_CSC_10BIT) {
		set_bit(SDE_SSPP_CSC_10BIT, &sspp->features);
		sblk->csc_blk.base = PROP_VALUE_ACCESS(prop_value,
							VIG_CSC_OFF, 0);
	}

	sblk->hsic_blk.id = SDE_SSPP_HSIC;
	snprintf(sblk->hsic_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_hsic%u", sspp->id - SSPP_VIG0);
	if (prop_exists[VIG_HSIC_PROP]) {
		sblk->hsic_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_HSIC_PROP, 0);
		sblk->hsic_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_HSIC_PROP, 1);
		sblk->hsic_blk.len = 0;
		set_bit(SDE_SSPP_HSIC, &sspp->features);
	}

	sblk->memcolor_blk.id = SDE_SSPP_MEMCOLOR;
	snprintf(sblk->memcolor_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_memcolor%u", sspp->id - SSPP_VIG0);
	if (prop_exists[VIG_MEMCOLOR_PROP]) {
		sblk->memcolor_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_MEMCOLOR_PROP, 0);
		sblk->memcolor_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_MEMCOLOR_PROP, 1);
		sblk->memcolor_blk.len = 0;
		set_bit(SDE_SSPP_MEMCOLOR, &sspp->features);
	}

	sblk->pcc_blk.id = SDE_SSPP_PCC;
	snprintf(sblk->pcc_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_pcc%u", sspp->id - SSPP_VIG0);
	if (prop_exists[VIG_PCC_PROP]) {
		sblk->pcc_blk.base = PROP_VALUE_ACCESS(prop_value,
			VIG_PCC_PROP, 0);
		sblk->pcc_blk.version = PROP_VALUE_ACCESS(prop_value,
			VIG_PCC_PROP, 1);
		sblk->pcc_blk.len = 0;
		set_bit(SDE_SSPP_PCC, &sspp->features);
	}
	snprintf(sspp->name, sizeof(sspp->name), "vig%d", *vig_count-1);
}

static void _sde_sspp_setup_rgb(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	bool *prop_exists, struct sde_prop_value *prop_value, u32 *rgb_count)
{
	sblk->maxupscale = MAX_SSPP_UPSCALE;
	sblk->maxdwnscale = MAX_SSPP_DOWNSCALE;
	sblk->format_list = plane_formats;
	sspp->id = SSPP_RGB0 + *rgb_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->clk_ctrl = SDE_CLK_CTRL_RGB0 + *rgb_count;
	sspp->type = SSPP_TYPE_RGB;
	set_bit(SDE_SSPP_QOS, &sspp->features);
	(*rgb_count)++;

	if (!prop_value)
		return;

	if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED2) {
		set_bit(SDE_SSPP_SCALER_RGB, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED2;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			RGB_SCALER_OFF, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			RGB_SCALER_LEN, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_scaler%u", sspp->id - SSPP_VIG0);
	} else if (sde_cfg->qseed_type == SDE_SSPP_SCALER_QSEED3) {
		set_bit(SDE_SSPP_SCALER_RGB, &sspp->features);
		sblk->scaler_blk.id = SDE_SSPP_SCALER_QSEED3;
		sblk->scaler_blk.base = PROP_VALUE_ACCESS(prop_value,
			RGB_SCALER_LEN, 0);
		sblk->scaler_blk.len = PROP_VALUE_ACCESS(prop_value,
			SSPP_SCALE_SIZE, 0);
		snprintf(sblk->scaler_blk.name, SDE_HW_BLK_NAME_LEN,
			"sspp_scaler%u", sspp->id - SSPP_VIG0);
	}

	sblk->pcc_blk.id = SDE_SSPP_PCC;
	if (prop_exists[RGB_PCC_PROP]) {
		sblk->pcc_blk.base = PROP_VALUE_ACCESS(prop_value,
			RGB_PCC_PROP, 0);
		sblk->pcc_blk.version = PROP_VALUE_ACCESS(prop_value,
			RGB_PCC_PROP, 1);
		sblk->pcc_blk.len = 0;
		set_bit(SDE_SSPP_PCC, &sspp->features);
	}
	snprintf(sspp->name, sizeof(sspp->name), "rgb%d", *rgb_count-1);
}

static void _sde_sspp_setup_cursor(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	struct sde_prop_value *prop_value, u32 *cursor_count)
{
	set_bit(SDE_SSPP_CURSOR, &sspp->features);
	sblk->maxupscale = SSPP_UNITY_SCALE;
	sblk->maxdwnscale = SSPP_UNITY_SCALE;
	sblk->format_list = cursor_formats;
	sspp->id = SSPP_CURSOR0 + *cursor_count;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	sspp->clk_ctrl = SDE_CLK_CTRL_CURSOR0 + *cursor_count;
	sspp->type = SSPP_TYPE_CURSOR;
	(*cursor_count)++;
	snprintf(sspp->name, sizeof(sspp->name), "cursor%d", *cursor_count-1);
}

static void _sde_sspp_setup_dma(struct sde_mdss_cfg *sde_cfg,
	struct sde_sspp_cfg *sspp, struct sde_sspp_sub_blks *sblk,
	struct sde_prop_value *prop_value, u32 *dma_count)
{
	sblk->maxupscale = SSPP_UNITY_SCALE;
	sblk->maxdwnscale = SSPP_UNITY_SCALE;
	sblk->format_list = plane_formats;
	sspp->id = SSPP_DMA0 + *dma_count;
	sspp->clk_ctrl = SDE_CLK_CTRL_DMA0 + *dma_count;
	sspp->type = SSPP_TYPE_DMA;
	snprintf(sspp->name, SDE_HW_BLK_NAME_LEN, "sspp_%u",
			sspp->id - SSPP_VIG0);
	set_bit(SDE_SSPP_QOS, &sspp->features);
	(*dma_count)++;
	snprintf(sspp->name, sizeof(sspp->name), "dma%d", *dma_count-1);
}

static int sde_sspp_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[SSPP_PROP_MAX], off_count, i, j;
	int vig_prop_count[VIG_PROP_MAX], rgb_prop_count[RGB_PROP_MAX];
	bool prop_exists[SSPP_PROP_MAX], vig_prop_exists[VIG_PROP_MAX];
	bool rgb_prop_exists[RGB_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	struct sde_prop_value *vig_prop_value = NULL, *rgb_prop_value = NULL;
	const char *type;
	struct sde_sspp_cfg *sspp;
	struct sde_sspp_sub_blks *sblk;
	u32 vig_count = 0, dma_count = 0, rgb_count = 0, cursor_count = 0;
	u32 danger_count = 0, safe_count = 0;
	struct device_node *snp = NULL;

	prop_value = kzalloc(SSPP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

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
					prop_exists, prop_value);
	if (rc)
		goto end;

	sde_cfg->sspp_count = off_count;

	/* get vig feature dt properties if they exist */
	snp = of_get_child_by_name(np, sspp_prop[SSPP_VIG_BLOCKS].prop_name);
	if (snp) {
		vig_prop_value = kzalloc(VIG_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
		if (!vig_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, vig_prop, ARRAY_SIZE(vig_prop),
			vig_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, vig_prop, ARRAY_SIZE(vig_prop),
				vig_prop_count, vig_prop_exists,
				vig_prop_value);
	}

	/* get rgb feature dt properties if they exist */
	snp = of_get_child_by_name(np, sspp_prop[SSPP_RGB_BLOCKS].prop_name);
	if (snp) {
		rgb_prop_value = kzalloc(RGB_PROP_MAX *
					sizeof(struct sde_prop_value),
					GFP_KERNEL);
		if (!rgb_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, rgb_prop, ARRAY_SIZE(rgb_prop),
			rgb_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, rgb_prop, ARRAY_SIZE(rgb_prop),
				rgb_prop_count, rgb_prop_exists,
				rgb_prop_value);
	}

	for (i = 0; i < off_count; i++) {
		sspp = sde_cfg->sspp + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		sspp->sblk = sblk;

		sspp->base = PROP_VALUE_ACCESS(prop_value, SSPP_OFF, i);
		sspp->len = PROP_VALUE_ACCESS(prop_value, SSPP_SIZE, 0);
		sblk->maxlinewidth = sde_cfg->max_sspp_linewidth;

		set_bit(SDE_SSPP_SRC, &sspp->features);
		sblk->src_blk.id = SDE_SSPP_SRC;

		of_property_read_string_index(np,
				sspp_prop[SSPP_TYPE].prop_name, i, &type);
		if (!strcmp(type, "vig")) {
			_sde_sspp_setup_vig(sde_cfg, sspp, sblk,
				vig_prop_exists, vig_prop_value, &vig_count);
		} else if (!strcmp(type, "rgb")) {
			_sde_sspp_setup_rgb(sde_cfg, sspp, sblk,
				rgb_prop_exists, rgb_prop_value, &rgb_count);
		} else if (!strcmp(type, "cursor")) {
			/* No prop values for cursor pipes */
			_sde_sspp_setup_cursor(sde_cfg, sspp, sblk, NULL,
								&cursor_count);
		} else if (!strcmp(type, "dma")) {
			/* No prop values for DMA pipes */
			_sde_sspp_setup_dma(sde_cfg, sspp, sblk, NULL,
								&dma_count);
		} else {
			SDE_ERROR("invalid sspp type:%s\n", type);
			rc = -EINVAL;
			goto end;
		}

		snprintf(sblk->src_blk.name, SDE_HW_BLK_NAME_LEN, "sspp_src_%u",
				sspp->id - SSPP_VIG0);

		if (sspp->clk_ctrl >= SDE_CLK_CTRL_MAX) {
			SDE_ERROR("%s: invalid clk ctrl: %d\n",
					sblk->src_blk.name, sspp->clk_ctrl);
			rc = -EINVAL;
			goto end;
		}

		sblk->maxhdeciexp = MAX_HORZ_DECIMATION;
		sblk->maxvdeciexp = MAX_VERT_DECIMATION;

		sspp->xin_id = PROP_VALUE_ACCESS(prop_value, SSPP_XIN, i);
		sblk->danger_lut_linear =
			PROP_VALUE_ACCESS(prop_value, SSPP_DANGER, 0);
		sblk->danger_lut_tile =
			PROP_VALUE_ACCESS(prop_value, SSPP_DANGER, 1);
		sblk->danger_lut_nrt =
			PROP_VALUE_ACCESS(prop_value, SSPP_DANGER, 2);
		sblk->safe_lut_linear =
			PROP_VALUE_ACCESS(prop_value, SSPP_SAFE, 0);
		sblk->safe_lut_tile =
			PROP_VALUE_ACCESS(prop_value, SSPP_SAFE, 1);
		sblk->safe_lut_nrt =
			PROP_VALUE_ACCESS(prop_value, SSPP_SAFE, 2);
		sblk->creq_lut_nrt = DEFAULT_CREQ_LUT_NRT;
		sblk->pixel_ram_size = DEFAULT_PIXEL_RAM_SIZE;
		sblk->src_blk.len = PROP_VALUE_ACCESS(prop_value, SSPP_SIZE, 0);

		for (j = 0; j < sde_cfg->mdp_count; j++) {
			sde_cfg->mdp[j].clk_ctrls[sspp->clk_ctrl].reg_off =
				PROP_BITVALUE_ACCESS(prop_value,
						SSPP_CLK_CTRL, i, 0);
			sde_cfg->mdp[j].clk_ctrls[sspp->clk_ctrl].bit_off =
				PROP_BITVALUE_ACCESS(prop_value,
						SSPP_CLK_CTRL, i, 1);
		}

		SDE_DEBUG(
			"xin:%d danger:%x/%x/%x safe:%x/%x/%x creq:%x ram:%d clk%d:%x/%d\n",
			sspp->xin_id,
			sblk->danger_lut_linear,
			sblk->danger_lut_tile,
			sblk->danger_lut_nrt,
			sblk->safe_lut_linear,
			sblk->safe_lut_tile,
			sblk->safe_lut_nrt,
			sblk->creq_lut_nrt,
			sblk->pixel_ram_size,
			sspp->clk_ctrl,
			sde_cfg->mdp[0].clk_ctrls[sspp->clk_ctrl].reg_off,
			sde_cfg->mdp[0].clk_ctrls[sspp->clk_ctrl].bit_off);
	}

end:
	kfree(prop_value);
	kfree(vig_prop_value);
	kfree(rgb_prop_value);
	return rc;
}

static int sde_ctl_parse_dt(struct device_node *np,
		struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[HW_PROP_MAX], i;
	bool prop_exists[HW_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	struct sde_ctl_cfg *ctl;
	u32 off_count;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument input param\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(HW_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, ctl_prop, ARRAY_SIZE(ctl_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->ctl_count = off_count;

	rc = _read_dt_entry(np, ctl_prop, ARRAY_SIZE(ctl_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		ctl = sde_cfg->ctl + i;
		ctl->base = PROP_VALUE_ACCESS(prop_value, HW_OFF, i);
		ctl->len = PROP_VALUE_ACCESS(prop_value, HW_LEN, 0);
		ctl->id = CTL_0 + i;
		snprintf(ctl->name, SDE_HW_BLK_NAME_LEN, "ctl_%u",
				ctl->id - CTL_0);

		if (i < MAX_SPLIT_DISPLAY_CTL)
			set_bit(SDE_CTL_SPLIT_DISPLAY, &ctl->features);
		if (i < MAX_PP_SPLIT_DISPLAY_CTL)
			set_bit(SDE_CTL_PINGPONG_SPLIT, &ctl->features);
	}

end:
	kfree(prop_value);
	return rc;
}

static int sde_mixer_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[MIXER_PROP_MAX], i;
	int blocks_prop_count[MIXER_BLOCKS_PROP_MAX];
	bool prop_exists[MIXER_PROP_MAX];
	bool blocks_prop_exists[MIXER_BLOCKS_PROP_MAX];
	struct sde_prop_value *prop_value = NULL, *blocks_prop_value = NULL;
	u32 off_count, max_blendstages;
	u32 blend_reg_base[] = {0x20, 0x50, 0x80, 0xb0, 0x230, 0x260, 0x290};
	u32 lm_pair_mask[] = {LM_1, LM_0, LM_5, 0x0, 0x0, LM_2};
	struct sde_lm_cfg *mixer;
	struct sde_lm_sub_blks *sblk;
	int pp_count, dspp_count;
	u32 pp_idx, dspp_idx;
	struct device_node *snp = NULL;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument input param\n");
		rc = -EINVAL;
		goto end;
	}
	max_blendstages = sde_cfg->max_mixer_blendstages;

	prop_value = kzalloc(MIXER_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, mixer_prop, ARRAY_SIZE(mixer_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->mixer_count = off_count;

	rc = _read_dt_entry(np, mixer_prop, ARRAY_SIZE(mixer_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	pp_count = sde_cfg->pingpong_count;
	dspp_count = sde_cfg->dspp_count;

	/* get mixer feature dt properties if they exist */
	snp = of_get_child_by_name(np, mixer_prop[MIXER_BLOCKS].prop_name);
	if (snp) {
		blocks_prop_value = kzalloc(MIXER_BLOCKS_PROP_MAX *
				MAX_SDE_HW_BLK * sizeof(struct sde_prop_value),
				GFP_KERNEL);
		if (!blocks_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, mixer_blocks_prop,
			ARRAY_SIZE(mixer_blocks_prop), blocks_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, mixer_blocks_prop,
				ARRAY_SIZE(mixer_blocks_prop),
				blocks_prop_count, blocks_prop_exists,
				blocks_prop_value);
	}

	for (i = 0, pp_idx = 0, dspp_idx = 0; i < off_count; i++) {
		mixer = sde_cfg->mixer + i;
		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		mixer->sblk = sblk;

		mixer->base = PROP_VALUE_ACCESS(prop_value, MIXER_OFF, i);
		mixer->len = PROP_VALUE_ACCESS(prop_value, MIXER_LEN, 0);
		mixer->id = LM_0 + i;
		snprintf(mixer->name, SDE_HW_BLK_NAME_LEN, "lm_%u",
				mixer->id - LM_0);

		if (!prop_exists[MIXER_LEN])
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

		sblk->gc.id = SDE_MIXER_GC;
		if (blocks_prop_value && blocks_prop_exists[MIXER_GC_PROP]) {
			sblk->gc.base = PROP_VALUE_ACCESS(blocks_prop_value,
					MIXER_GC_PROP, 0);
			sblk->gc.version = PROP_VALUE_ACCESS(blocks_prop_value,
					MIXER_GC_PROP, 1);
			sblk->gc.len = 0;
			set_bit(SDE_MIXER_GC, &mixer->features);
		}
	}

end:
	kfree(prop_value);
	kfree(blocks_prop_value);
	return rc;
}

static int sde_intf_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[INTF_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[INTF_PROP_MAX];
	u32 off_count;
	u32 dsi_count = 0, none_count = 0, hdmi_count = 0, dp_count = 0;
	const char *type;
	struct sde_intf_cfg *intf;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(INTF_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, intf_prop, ARRAY_SIZE(intf_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->intf_count = off_count;

	rc = _read_dt_entry(np, intf_prop, ARRAY_SIZE(intf_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		intf = sde_cfg->intf + i;
		intf->base = PROP_VALUE_ACCESS(prop_value, INTF_OFF, i);
		intf->len = PROP_VALUE_ACCESS(prop_value, INTF_LEN, 0);
		intf->id = INTF_0 + i;
		snprintf(intf->name, SDE_HW_BLK_NAME_LEN, "intf_%u",
				intf->id - INTF_0);

		if (!prop_exists[INTF_LEN])
			intf->len = DEFAULT_SDE_HW_BLOCK_LEN;

		intf->prog_fetch_lines_worst_case =
				PROP_VALUE_ACCESS(prop_value, INTF_PREFETCH, i);

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
	kfree(prop_value);
	return rc;
}

static int sde_wb_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[WB_PROP_MAX], i, j;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[WB_PROP_MAX];
	u32 off_count;
	struct sde_wb_cfg *wb;
	struct sde_wb_sub_blocks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(WB_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, wb_prop, ARRAY_SIZE(wb_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->wb_count = off_count;

	rc = _read_dt_entry(np, wb_prop, ARRAY_SIZE(wb_prop), prop_count,
		prop_exists, prop_value);
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

		wb->base = PROP_VALUE_ACCESS(prop_value, WB_OFF, i);
		wb->id = WB_0 + PROP_VALUE_ACCESS(prop_value, WB_ID, i);
		snprintf(wb->name, SDE_HW_BLK_NAME_LEN, "wb_%u",
				wb->id - WB_0);
		wb->clk_ctrl = SDE_CLK_CTRL_WB0 +
			PROP_VALUE_ACCESS(prop_value, WB_ID, i);
		wb->xin_id = PROP_VALUE_ACCESS(prop_value, WB_XIN_ID, i);
		wb->vbif_idx = VBIF_NRT;

		if (wb->clk_ctrl >= SDE_CLK_CTRL_MAX) {
			SDE_ERROR("%s: invalid clk ctrl: %d\n",
					wb->name, wb->clk_ctrl);
			rc = -EINVAL;
			goto end;
		}

		wb->len = PROP_VALUE_ACCESS(prop_value, WB_LEN, 0);
		wb->format_list = wb2_formats;
		if (!prop_exists[WB_LEN])
			wb->len = DEFAULT_SDE_HW_BLOCK_LEN;
		sblk->maxlinewidth = sde_cfg->max_wb_linewidth;

		if (wb->id >= LINE_MODE_WB_OFFSET)
			set_bit(SDE_WB_LINE_MODE, &wb->features);
		else
			set_bit(SDE_WB_BLOCK_MODE, &wb->features);
		set_bit(SDE_WB_TRAFFIC_SHAPER, &wb->features);
		set_bit(SDE_WB_YUV_CONFIG, &wb->features);

		for (j = 0; j < sde_cfg->mdp_count; j++) {
			sde_cfg->mdp[j].clk_ctrls[wb->clk_ctrl].reg_off =
				PROP_BITVALUE_ACCESS(prop_value,
						WB_CLK_CTRL, i, 0);
			sde_cfg->mdp[j].clk_ctrls[wb->clk_ctrl].bit_off =
				PROP_BITVALUE_ACCESS(prop_value,
						WB_CLK_CTRL, i, 1);
		}

		SDE_DEBUG(
			"wb:%d xin:%d vbif:%d clk%d:%x/%d\n",
			wb->id - WB_0,
			wb->xin_id,
			wb->vbif_idx,
			wb->clk_ctrl,
			sde_cfg->mdp[0].clk_ctrls[wb->clk_ctrl].reg_off,
			sde_cfg->mdp[0].clk_ctrls[wb->clk_ctrl].bit_off);
	}

end:
	kfree(prop_value);
	return rc;
}

static void _sde_dspp_setup_blocks(struct sde_mdss_cfg *sde_cfg,
	struct sde_dspp_cfg *dspp, struct sde_dspp_sub_blks *sblk,
	bool *prop_exists, struct sde_prop_value *prop_value)
{
	sblk->igc.id = SDE_DSPP_IGC;
	if (prop_exists[DSPP_IGC_PROP]) {
		sblk->igc.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_IGC_PROP, 0);
		sblk->igc.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_IGC_PROP, 1);
		sblk->igc.len = 0;
		set_bit(SDE_DSPP_IGC, &dspp->features);
	}

	sblk->pcc.id = SDE_DSPP_PCC;
	if (prop_exists[DSPP_PCC_PROP]) {
		sblk->pcc.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_PCC_PROP, 0);
		sblk->pcc.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_PCC_PROP, 1);
		sblk->pcc.len = 0;
		set_bit(SDE_DSPP_PCC, &dspp->features);
	}

	sblk->gc.id = SDE_DSPP_GC;
	if (prop_exists[DSPP_GC_PROP]) {
		sblk->gc.base = PROP_VALUE_ACCESS(prop_value, DSPP_GC_PROP, 0);
		sblk->gc.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_GC_PROP, 1);
		sblk->gc.len = 0;
		set_bit(SDE_DSPP_GC, &dspp->features);
	}

	sblk->gamut.id = SDE_DSPP_GAMUT;
	if (prop_exists[DSPP_GAMUT_PROP]) {
		sblk->gamut.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_GAMUT_PROP, 0);
		sblk->gamut.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_GAMUT_PROP, 1);
		sblk->gamut.len = 0;
		set_bit(SDE_DSPP_GAMUT, &dspp->features);
	}

	sblk->dither.id = SDE_DSPP_DITHER;
	if (prop_exists[DSPP_DITHER_PROP]) {
		sblk->dither.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_DITHER_PROP, 0);
		sblk->dither.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_DITHER_PROP, 1);
		sblk->dither.len = 0;
		set_bit(SDE_DSPP_DITHER, &dspp->features);
	}

	sblk->hist.id = SDE_DSPP_HIST;
	if (prop_exists[DSPP_HIST_PROP]) {
		sblk->hist.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_HIST_PROP, 0);
		sblk->hist.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_HIST_PROP, 1);
		sblk->hist.len = 0;
		set_bit(SDE_DSPP_HIST, &dspp->features);
	}

	sblk->hsic.id = SDE_DSPP_HSIC;
	if (prop_exists[DSPP_HSIC_PROP]) {
		sblk->hsic.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_HSIC_PROP, 0);
		sblk->hsic.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_HSIC_PROP, 1);
		sblk->hsic.len = 0;
		set_bit(SDE_DSPP_HSIC, &dspp->features);
	}

	sblk->memcolor.id = SDE_DSPP_MEMCOLOR;
	if (prop_exists[DSPP_MEMCOLOR_PROP]) {
		sblk->memcolor.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_MEMCOLOR_PROP, 0);
		sblk->memcolor.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_MEMCOLOR_PROP, 1);
		sblk->memcolor.len = 0;
		set_bit(SDE_DSPP_MEMCOLOR, &dspp->features);
	}

	sblk->sixzone.id = SDE_DSPP_SIXZONE;
	if (prop_exists[DSPP_SIXZONE_PROP]) {
		sblk->sixzone.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_SIXZONE_PROP, 0);
		sblk->sixzone.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_SIXZONE_PROP, 1);
		sblk->sixzone.len = 0;
		set_bit(SDE_DSPP_SIXZONE, &dspp->features);
	}

	sblk->vlut.id = SDE_DSPP_VLUT;
	if (prop_exists[DSPP_VLUT_PROP]) {
		sblk->vlut.base = PROP_VALUE_ACCESS(prop_value,
			DSPP_VLUT_PROP, 0);
		sblk->vlut.version = PROP_VALUE_ACCESS(prop_value,
			DSPP_VLUT_PROP, 1);
		sblk->sixzone.len = 0;
		set_bit(SDE_DSPP_VLUT, &dspp->features);
	}
}

static int sde_dspp_parse_dt(struct device_node *np,
						struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[DSPP_PROP_MAX], i;
	int ad_prop_count[AD_PROP_MAX];
	bool prop_exists[DSPP_PROP_MAX], ad_prop_exists[AD_PROP_MAX];
	bool blocks_prop_exists[DSPP_BLOCKS_PROP_MAX];
	struct sde_prop_value *ad_prop_value = NULL;
	int blocks_prop_count[DSPP_BLOCKS_PROP_MAX];
	struct sde_prop_value *prop_value = NULL, *blocks_prop_value = NULL;
	u32 off_count, ad_off_count;
	struct sde_dspp_cfg *dspp;
	struct sde_dspp_sub_blks *sblk;
	struct device_node *snp = NULL;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(DSPP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, dspp_prop, ARRAY_SIZE(dspp_prop),
		prop_count, &off_count);
	if (rc)
		goto end;

	sde_cfg->dspp_count = off_count;

	rc = _read_dt_entry(np, dspp_prop, ARRAY_SIZE(dspp_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	/* Parse AD dtsi entries */
	ad_prop_value = kzalloc(AD_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!ad_prop_value) {
		rc = -ENOMEM;
		goto end;
	}
	rc = _validate_dt_entry(np, ad_prop, ARRAY_SIZE(ad_prop),
		ad_prop_count, &ad_off_count);
	if (rc)
		goto end;
	rc = _read_dt_entry(np, ad_prop, ARRAY_SIZE(ad_prop), ad_prop_count,
		ad_prop_exists, ad_prop_value);
	if (rc)
		goto end;

	/* get DSPP feature dt properties if they exist */
	snp = of_get_child_by_name(np, dspp_prop[DSPP_BLOCKS].prop_name);
	if (snp) {
		blocks_prop_value = kzalloc(DSPP_BLOCKS_PROP_MAX *
				MAX_SDE_HW_BLK * sizeof(struct sde_prop_value),
				GFP_KERNEL);
		if (!blocks_prop_value) {
			rc = -ENOMEM;
			goto end;
		}
		rc = _validate_dt_entry(snp, dspp_blocks_prop,
			ARRAY_SIZE(dspp_blocks_prop), blocks_prop_count, NULL);
		if (rc)
			goto end;
		rc = _read_dt_entry(snp, dspp_blocks_prop,
			ARRAY_SIZE(dspp_blocks_prop), blocks_prop_count,
			blocks_prop_exists, blocks_prop_value);
		if (rc)
			goto end;
	}

	for (i = 0; i < off_count; i++) {
		dspp = sde_cfg->dspp + i;
		dspp->base = PROP_VALUE_ACCESS(prop_value, DSPP_OFF, i);
		dspp->len = PROP_VALUE_ACCESS(prop_value, DSPP_SIZE, 0);
		dspp->id = DSPP_0 + i;
		snprintf(dspp->name, SDE_HW_BLK_NAME_LEN, "dspp_%u",
				dspp->id - DSPP_0);

		sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
		if (!sblk) {
			rc = -ENOMEM;
			/* catalog deinit will release the allocated blocks */
			goto end;
		}
		dspp->sblk = sblk;

		if (blocks_prop_value)
			_sde_dspp_setup_blocks(sde_cfg, dspp, sblk,
					blocks_prop_exists, blocks_prop_value);

		sblk->ad.id = SDE_DSPP_AD;
		if (ad_prop_value && (i < ad_off_count) &&
		    ad_prop_exists[AD_OFF]) {
			sblk->ad.base = PROP_VALUE_ACCESS(ad_prop_value,
				AD_OFF, i);
			sblk->ad.version = PROP_VALUE_ACCESS(ad_prop_value,
				AD_VERSION, 0);
			set_bit(SDE_DSPP_AD, &dspp->features);
		}
	}

end:
	kfree(prop_value);
	kfree(ad_prop_value);
	kfree(blocks_prop_value);
	return rc;
}

static int sde_cdm_parse_dt(struct device_node *np,
				struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[HW_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[HW_PROP_MAX];
	u32 off_count;
	struct sde_cdm_cfg *cdm;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(HW_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, cdm_prop, ARRAY_SIZE(cdm_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->cdm_count = off_count;

	rc = _read_dt_entry(np, cdm_prop, ARRAY_SIZE(cdm_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	for (i = 0; i < off_count; i++) {
		cdm = sde_cfg->cdm + i;
		cdm->base = PROP_VALUE_ACCESS(prop_value, HW_OFF, i);
		cdm->id = CDM_0 + i;
		snprintf(cdm->name, SDE_HW_BLK_NAME_LEN, "cdm_%u",
				cdm->id - CDM_0);
		cdm->len = PROP_VALUE_ACCESS(prop_value, HW_LEN, 0);

		/* intf3 and wb2 for cdm block */
		cdm->wb_connect = sde_cfg->wb_count ? BIT(WB_2) : BIT(31);
		cdm->intf_connect = sde_cfg->intf_count ? BIT(INTF_3) : BIT(31);
	}

end:
	kfree(prop_value);
	return rc;
}

static int sde_vbif_parse_dt(struct device_node *np,
				struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[VBIF_PROP_MAX], i, j, k;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[VBIF_PROP_MAX];
	u32 off_count, vbif_len, rd_len = 0, wr_len = 0;
	struct sde_vbif_cfg *vbif;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(VBIF_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
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
		prop_exists, prop_value);
	if (rc)
		goto end;

	vbif_len = PROP_VALUE_ACCESS(prop_value, VBIF_LEN, 0);
	if (!prop_exists[VBIF_LEN])
		vbif_len = DEFAULT_SDE_HW_BLOCK_LEN;

	for (i = 0; i < off_count; i++) {
		vbif = sde_cfg->vbif + i;
		vbif->base = PROP_VALUE_ACCESS(prop_value, VBIF_OFF, i);
		vbif->len = vbif_len;
		vbif->id = VBIF_0 + PROP_VALUE_ACCESS(prop_value, VBIF_ID, i);
		snprintf(vbif->name, SDE_HW_BLK_NAME_LEN, "vbif_%u",
				vbif->id - VBIF_0);

		SDE_DEBUG("vbif:%d\n", vbif->id - VBIF_0);

		vbif->xin_halt_timeout = VBIF_XIN_HALT_TIMEOUT;

		vbif->default_ot_rd_limit = PROP_VALUE_ACCESS(prop_value,
				VBIF_DEFAULT_OT_RD_LIMIT, 0);
		SDE_DEBUG("default_ot_rd_limit=%u\n",
				vbif->default_ot_rd_limit);

		vbif->default_ot_wr_limit = PROP_VALUE_ACCESS(prop_value,
				VBIF_DEFAULT_OT_WR_LIMIT, 0);
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
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_RD_LIMIT, k++);
			vbif->dynamic_ot_rd_tbl.cfg[j].ot_limit =
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_RD_LIMIT, k++);
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
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_WR_LIMIT, k++);
			vbif->dynamic_ot_wr_tbl.cfg[j].ot_limit =
				PROP_VALUE_ACCESS(prop_value,
				VBIF_DYNAMIC_OT_WR_LIMIT, k++);
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
	kfree(prop_value);
	return rc;
}

static int sde_pp_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *sde_cfg)
{
	int rc, prop_count[PP_PROP_MAX], i;
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[PP_PROP_MAX];
	u32 off_count;
	struct sde_pingpong_cfg *pp;
	struct sde_pingpong_sub_blks *sblk;

	if (!sde_cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(PP_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, pp_prop, ARRAY_SIZE(pp_prop), prop_count,
		&off_count);
	if (rc)
		goto end;

	sde_cfg->pingpong_count = off_count;

	rc = _read_dt_entry(np, pp_prop, ARRAY_SIZE(pp_prop), prop_count,
		prop_exists, prop_value);
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

		pp->base = PROP_VALUE_ACCESS(prop_value, PP_OFF, i);
		pp->id = PINGPONG_0 + i;
		snprintf(pp->name, SDE_HW_BLK_NAME_LEN, "pingpong_%u",
				pp->id - PINGPONG_0);
		pp->len = PROP_VALUE_ACCESS(prop_value, PP_LEN, 0);

		sblk->te.base = PROP_VALUE_ACCESS(prop_value, TE_OFF, i);
		sblk->te.id = SDE_PINGPONG_TE;
		snprintf(sblk->te.name, SDE_HW_BLK_NAME_LEN, "te_%u",
				pp->id - PINGPONG_0);
		set_bit(SDE_PINGPONG_TE, &pp->features);

		sblk->te2.base = PROP_VALUE_ACCESS(prop_value, TE2_OFF, i);
		if (sblk->te2.base) {
			sblk->te2.id = SDE_PINGPONG_TE2;
			snprintf(sblk->te2.name, SDE_HW_BLK_NAME_LEN, "te2_%u",
					pp->id - PINGPONG_0);
			set_bit(SDE_PINGPONG_TE2, &pp->features);
			set_bit(SDE_PINGPONG_SPLIT, &pp->features);
		}

		if (PROP_VALUE_ACCESS(prop_value, PP_SLAVE, i))
			set_bit(SDE_PINGPONG_SLAVE, &pp->features);

		sblk->dsc.base = PROP_VALUE_ACCESS(prop_value, DSC_OFF, i);
		if (sblk->dsc.base) {
			sblk->dsc.id = SDE_PINGPONG_DSC;
			snprintf(sblk->dsc.name, SDE_HW_BLK_NAME_LEN, "dsc_%u",
					sblk->dsc.id - PINGPONG_0);
			set_bit(SDE_PINGPONG_DSC, &pp->features);
		}
	}

end:
	kfree(prop_value);
	return rc;
}

static inline u32 _sde_parse_sspp_id(struct sde_mdss_cfg *cfg,
	const char *name)
{
	int i;

	for (i = 0; i < cfg->sspp_count; i++) {
		if (!strcmp(cfg->sspp[i].name, name))
			return cfg->sspp[i].id;
	}

	return SSPP_NONE;
}

static int _sde_vp_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *cfg)
{
	int rc = 0, i = 0;
	struct device_node *node = NULL;
	struct device_node *root_node = NULL;
	struct sde_vp_cfg *vp;
	struct sde_vp_sub_blks *vp_sub, *vp_sub_next;
	struct property *prop;
	const char *cname;

	root_node = of_get_child_by_name(np, "qcom,sde-plane-id-map");
	if (!root_node) {
		root_node = of_parse_phandle(np, "qcom,sde-plane-id-map", 0);
		if (!root_node) {
			SDE_ERROR("No entry present for qcom,sde-plane-id-map");
			rc = -EINVAL;
			goto end;
		}
	}

	for_each_child_of_node(root_node, node) {
		if (i >= MAX_BLOCKS) {
			SDE_ERROR("num of nodes(%d) is bigger than max(%d)\n",
					i, MAX_BLOCKS);
			rc = -EINVAL;
			goto end;
		}
		cfg->vp_count++;
		vp = &(cfg->vp[i]);
		vp->id = i;
		rc = of_property_read_string(node, "qcom,display-type",
						&(vp->display_type));
		if (rc) {
			SDE_ERROR("failed to read display-type, rc = %d\n", rc);
			goto end;
		}

		rc = of_property_read_string(node, "qcom,plane-type",
						&(vp->plane_type));
		if (rc) {
			SDE_ERROR("failed to read plane-type, rc = %d\n", rc);
			goto end;
		}

		INIT_LIST_HEAD(&vp->sub_blks);
		of_property_for_each_string(node, "qcom,plane-name",
						prop, cname) {
			vp_sub = kzalloc(sizeof(*vp_sub), GFP_KERNEL);
			if (!vp_sub) {
				rc = -ENOMEM;
				goto end;
			}
			vp_sub->sspp_id = _sde_parse_sspp_id(cfg, cname);
			list_add_tail(&vp_sub->pipeid_list, &vp->sub_blks);
		}
		i++;
	}

end:
	if (rc && cfg->vp_count) {
		vp = &(cfg->vp[i]);
		for (i = 0; i < cfg->vp_count; i++) {
			list_for_each_entry_safe(vp_sub, vp_sub_next,
				&vp->sub_blks, pipeid_list) {
				list_del(&vp_sub->pipeid_list);
				kfree(vp_sub);
			}
		}
		memset(&(cfg->vp[0]), 0, sizeof(cfg->vp));
		cfg->vp_count = 0;
	}
	return rc;
}

static int sde_parse_dt(struct device_node *np, struct sde_mdss_cfg *cfg)
{
	int rc, len, prop_count[SDE_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[SDE_PROP_MAX];
	const char *type;

	if (!cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(SDE_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, sde_prop, ARRAY_SIZE(sde_prop), prop_count,
		&len);
	if (rc)
		goto end;

	rc = _read_dt_entry(np, sde_prop, ARRAY_SIZE(sde_prop), prop_count,
		prop_exists, prop_value);
	if (rc)
		goto end;

	cfg->mdss_count = 1;
	cfg->mdss[0].base = MDSS_BASE_OFFSET;
	cfg->mdss[0].id = MDP_TOP;
	snprintf(cfg->mdss[0].name, SDE_HW_BLK_NAME_LEN, "mdss_%u",
			cfg->mdss[0].id - MDP_TOP);

	cfg->mdp_count = 1;
	cfg->mdp[0].id = MDP_TOP;
	snprintf(cfg->mdp[0].name, SDE_HW_BLK_NAME_LEN, "top_%u",
		cfg->mdp[0].id - MDP_TOP);
	cfg->mdp[0].base = PROP_VALUE_ACCESS(prop_value, SDE_OFF, 0);
	cfg->mdp[0].len = PROP_VALUE_ACCESS(prop_value, SDE_LEN, 0);
	if (!prop_exists[SDE_LEN])
		cfg->mdp[0].len = DEFAULT_SDE_HW_BLOCK_LEN;

	cfg->max_sspp_linewidth = PROP_VALUE_ACCESS(prop_value,
			SSPP_LINEWIDTH, 0);
	if (!prop_exists[SSPP_LINEWIDTH])
		cfg->max_sspp_linewidth = DEFAULT_SDE_LINE_WIDTH;

	cfg->max_mixer_width = PROP_VALUE_ACCESS(prop_value,
			MIXER_LINEWIDTH, 0);
	if (!prop_exists[MIXER_LINEWIDTH])
		cfg->max_mixer_width = DEFAULT_SDE_LINE_WIDTH;

	cfg->max_mixer_blendstages = PROP_VALUE_ACCESS(prop_value,
			MIXER_BLEND, 0);
	if (!prop_exists[MIXER_BLEND])
		cfg->max_mixer_blendstages = DEFAULT_SDE_MIXER_BLENDSTAGES;

	cfg->max_wb_linewidth = PROP_VALUE_ACCESS(prop_value, WB_LINEWIDTH, 0);
	if (!prop_exists[WB_LINEWIDTH])
		cfg->max_wb_linewidth = DEFAULT_SDE_LINE_WIDTH;

	cfg->mdp[0].highest_bank_bit = PROP_VALUE_ACCESS(prop_value,
			BANK_BIT, 0);
	if (!prop_exists[BANK_BIT])
		cfg->mdp[0].highest_bank_bit = DEFAULT_SDE_HIGHEST_BANK_BIT;

	rc = of_property_read_string(np, sde_prop[QSEED_TYPE].prop_name, &type);
	if (!rc && !strcmp(type, "qseedv3"))
		cfg->qseed_type = SDE_SSPP_SCALER_QSEED3;
	else if (!rc && !strcmp(type, "qseedv2"))
		cfg->qseed_type = SDE_SSPP_SCALER_QSEED2;
	else if (rc) {
		SDE_DEBUG("qseed property not found\n");
		rc = 0;
	}

	rc = of_property_read_string(np, sde_prop[CSC_TYPE].prop_name, &type);
	if (!rc && !strcmp(type, "csc"))
		cfg->csc_type = SDE_SSPP_CSC;
	else if (!rc && !strcmp(type, "csc-10bit"))
		cfg->csc_type = SDE_SSPP_CSC_10BIT;
	else if (rc) {
		SDE_DEBUG("CSC property not found\n");
		rc = 0;
	}

	cfg->has_src_split = PROP_VALUE_ACCESS(prop_value, SRC_SPLIT, 0);
end:
	kfree(prop_value);
	return rc;
}

static int sde_perf_parse_dt(struct device_node *np,
	struct sde_mdss_cfg *cfg)
{
	int rc, len, prop_count[PERF_PROP_MAX];
	struct sde_prop_value *prop_value = NULL;
	bool prop_exists[PERF_PROP_MAX];

	if (!cfg) {
		SDE_ERROR("invalid argument\n");
		rc = -EINVAL;
		goto end;
	}

	prop_value = kzalloc(PERF_PROP_MAX *
			sizeof(struct sde_prop_value), GFP_KERNEL);
	if (!prop_value) {
		rc = -ENOMEM;
		goto end;
	}

	rc = _validate_dt_entry(np, sde_perf_prop, ARRAY_SIZE(sde_perf_prop),
			prop_count, &len);
	if (rc)
		goto freeprop;

	rc = _read_dt_entry(np, sde_perf_prop, ARRAY_SIZE(sde_perf_prop),
			prop_count, prop_exists, prop_value);
	if (rc)
		goto freeprop;

	cfg->perf.max_bw_low =
			PROP_VALUE_ACCESS(prop_value, PERF_MAX_BW_LOW, 0);
	cfg->perf.max_bw_high =
			PROP_VALUE_ACCESS(prop_value, PERF_MAX_BW_HIGH, 0);

freeprop:
	kfree(prop_value);
end:
	return rc;
}

static int sde_hardware_format_caps(struct sde_mdss_cfg *sde_cfg,
	uint32_t hw_rev)
{
	int i, rc = 0;
	uint32_t dma_list_size, vig_list_size, wb2_list_size;
	uint32_t cursor_list_size = 0;
	struct sde_sspp_sub_blks *sblk;
	uint32_t index = 0;

	if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_300)) {
		cursor_list_size = ARRAY_SIZE(cursor_formats);
		sde_cfg->cursor_formats = kcalloc(cursor_list_size,
			sizeof(struct sde_format_extended), GFP_KERNEL);
		if (!sde_cfg->cursor_formats) {
			rc = -ENOMEM;
			goto end;
		}
		index = _sde_copy_formats(sde_cfg->cursor_formats,
			cursor_list_size, 0, cursor_formats,
			ARRAY_SIZE(cursor_formats));
	}

	dma_list_size = ARRAY_SIZE(plane_formats);
	vig_list_size = ARRAY_SIZE(plane_formats_yuv);
	wb2_list_size = ARRAY_SIZE(wb2_formats);

	dma_list_size += ARRAY_SIZE(rgb_10bit_formats);
	vig_list_size += ARRAY_SIZE(rgb_10bit_formats)
		+ ARRAY_SIZE(tp10_ubwc_formats)
		+ ARRAY_SIZE(p010_formats);
	wb2_list_size += ARRAY_SIZE(rgb_10bit_formats)
		+ ARRAY_SIZE(tp10_ubwc_formats);

	sde_cfg->dma_formats = kcalloc(dma_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->dma_formats) {
		rc = -ENOMEM;
		goto end;
	}

	sde_cfg->vig_formats = kcalloc(vig_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->vig_formats) {
		rc = -ENOMEM;
		goto end;
	}

	sde_cfg->wb_formats = kcalloc(wb2_list_size,
		sizeof(struct sde_format_extended), GFP_KERNEL);
	if (!sde_cfg->wb_formats) {
		SDE_ERROR("failed to allocate wb format list\n");
		rc = -ENOMEM;
		goto end;
	}

	if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_300) ||
		IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_301)) {
		sde_cfg->has_hdr = true;
	}

	index = _sde_copy_formats(sde_cfg->dma_formats, dma_list_size,
		0, plane_formats, ARRAY_SIZE(plane_formats));
	index += _sde_copy_formats(sde_cfg->dma_formats, dma_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));

	index = _sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		0, plane_formats_yuv, ARRAY_SIZE(plane_formats_yuv));
	index += _sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));
	index += _sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		index, p010_formats, ARRAY_SIZE(p010_formats));

	index += _sde_copy_formats(sde_cfg->vig_formats, vig_list_size,
		index, tp10_ubwc_formats,
		ARRAY_SIZE(tp10_ubwc_formats));

	index = _sde_copy_formats(sde_cfg->wb_formats, wb2_list_size,
		0, wb2_formats, ARRAY_SIZE(wb2_formats));
	index += _sde_copy_formats(sde_cfg->wb_formats, wb2_list_size,
		index, rgb_10bit_formats,
		ARRAY_SIZE(rgb_10bit_formats));
	index += _sde_copy_formats(sde_cfg->wb_formats, wb2_list_size,
		index, tp10_ubwc_formats,
		ARRAY_SIZE(tp10_ubwc_formats));

	for (i = 0; i < sde_cfg->sspp_count; ++i) {
		struct sde_sspp_cfg *sspp = &sde_cfg->sspp[i];

		sblk = (struct sde_sspp_sub_blks *)sspp->sblk;
		switch (sspp->type) {
		case SSPP_TYPE_VIG:
			sblk->format_list = sde_cfg->vig_formats;
			break;
		case SSPP_TYPE_CURSOR:
			if (IS_SDE_MAJOR_MINOR_SAME((hw_rev), SDE_HW_VER_300))
				sblk->format_list = sde_cfg->cursor_formats;
			else
				SDE_ERROR("invalid sspp type %d, xin id %d\n",
					sspp->type, sspp->xin_id);
			break;
		case SSPP_TYPE_DMA:
			sblk->format_list = sde_cfg->dma_formats;
			break;
		default:
			SDE_ERROR("invalid sspp type %d\n", sspp->type);
			rc = -EINVAL;
			goto end;
		}
	}

	for (i = 0; i < sde_cfg->wb_count; ++i)
		sde_cfg->wb[i].format_list = sde_cfg->wb_formats;

end:
	return rc;
}

static int sde_hardware_caps(struct sde_mdss_cfg *sde_cfg, uint32_t hw_rev)
{
	int rc = 0;

	switch (hw_rev) {
	case SDE_HW_VER_170:
	case SDE_HW_VER_171:
	case SDE_HW_VER_172:
		/* update msm8996 target here */
		break;
	case SDE_HW_VER_300:
	case SDE_HW_VER_301:
	case SDE_HW_VER_400:
		/* update cobalt and skunk target here */
		rc = sde_hardware_format_caps(sde_cfg, hw_rev);
		break;
	}

	return rc;
}

void sde_hw_catalog_deinit(struct sde_mdss_cfg *sde_cfg)
{
	int i;
	struct sde_vp_sub_blks *vp_sub, *vp_sub_next;

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

	for (i = 0; i < sde_cfg->vp_count; i++) {
		list_for_each_entry_safe(vp_sub, vp_sub_next,
			&sde_cfg->vp[i].sub_blks, pipeid_list) {
			list_del(&vp_sub->pipeid_list);
			kfree(vp_sub);
		}
	}

	kfree(sde_cfg->dma_formats);
	kfree(sde_cfg->cursor_formats);
	kfree(sde_cfg->vig_formats);
	kfree(sde_cfg->wb_formats);

	kfree(sde_cfg);
}

/*************************************************************
 * hardware catalog init
 *************************************************************/
struct sde_mdss_cfg *sde_hw_catalog_init(struct drm_device *dev,
	u32 hw_rev)
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

	rc = sde_perf_parse_dt(np, sde_cfg);
	if (rc)
		goto end;

	rc = _sde_vp_parse_dt(np, sde_cfg);
	if (rc)
		SDE_DEBUG("virtual plane is not supported.\n");

	rc = sde_hardware_caps(sde_cfg, hw_rev);
	if (rc)
		goto end;

	return sde_cfg;

end:
	sde_hw_catalog_deinit(sde_cfg);
	return NULL;
}

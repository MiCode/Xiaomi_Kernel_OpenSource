/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ddp_ovl_wcg.h"
#include "primary_display.h"
#include "disp_lcm.h"
#include "disp_helper.h"
#include "ddp_reg.h"
#include "disp_drv_log.h"

#define CSC_COEF_NUM 9

static u32 sRGB_to_DCI_P3[CSC_COEF_NUM] = {
	215603, 46541, 0,
	8702, 253442, 0,
	4478, 18979, 238687
};

static u32 DCI_P3_to_sRGB[CSC_COEF_NUM] = {
	321111, -58967, 0,
	-11025, 273169, 0,
	-5148, -20614, 287906
};

enum ovl_colorspace {
	OVL_SRGB = 0,
	OVL_P3,
	OVL_CS_NUM,
	OVL_CS_UNKNOWN,
};

enum ovl_transfer {
	OVL_GAMMA2 = 0,
	OVL_GAMMA2_2,
	OVL_LINEAR,
	OVL_GAMMA_NUM,
	OVL_GAMMA_UNKNOWN,
};

static char *ovl_colorspace_str(enum ovl_colorspace cs)
{
	switch (cs) {
	case OVL_SRGB:
		return "OVL_SRGB";
	case OVL_P3:
		return "OVL_P3";
	default:
		break;
	}
	return "unknown ovl colorspace";
}

static char *ovl_transfer_str(enum ovl_transfer xfr)
{
	switch (xfr) {
	case OVL_GAMMA2:
		return "OVL_GAMMA2";
	case OVL_GAMMA2_2:
		return "OVL_GAMMA2_2";
	case OVL_LINEAR:
		return "OVL_LINEAR";
	default:
		break;
	}
	return "unknown ovl transfer";
}

char *lcm_color_mode_str(enum android_color_mode cm)
{
	switch (cm) {
	case HAL_COLOR_MODE_NATIVE:
		return "NATIVE";
	case HAL_COLOR_MODE_STANDARD_BT601_625:
		return "BT601_625";
	case HAL_COLOR_MODE_STANDARD_BT601_625_UNADJUSTED:
		return "BT601_625_UNADJUSTED";
	case HAL_COLOR_MODE_STANDARD_BT601_525:
		return "BT601_525";
	case HAL_COLOR_MODE_STANDARD_BT601_525_UNADJUSTED:
		return "BT601_525_UNADJUSTED";
	case HAL_COLOR_MODE_STANDARD_BT709:
		return "BT709";
	case HAL_COLOR_MODE_DCI_P3:
		return "DCI_P3";
	case HAL_COLOR_MODE_SRGB:
		return "SRGB";
	case HAL_COLOR_MODE_ADOBE_RGB:
		return "ADOBE_RGB";
	case HAL_COLOR_MODE_DISPLAY_P3:
		return "DISPLAY_P3";
	default:
		break;
	}
	return "unknown lcm color mode";
}

static enum android_dataspace ovl_map_lcm_color_mode(enum android_color_mode cm)
{
	enum android_dataspace ds = HAL_DATASPACE_SRGB;

	switch (cm) {
	case HAL_COLOR_MODE_DISPLAY_P3:
		ds = HAL_DATASPACE_DISPLAY_P3;
		break;
	default:
		ds = HAL_DATASPACE_SRGB;
		break;
	}

	return ds;
}

static enum ovl_colorspace ovl_map_cs(enum android_dataspace ds)
{
	enum ovl_colorspace cs = OVL_SRGB;

	switch (ds & HAL_DATASPACE_STANDARD_MASK) {
	case HAL_DATASPACE_STANDARD_DCI_P3:
		cs = OVL_P3;
		break;
	case HAL_DATASPACE_STANDARD_ADOBE_RGB:
		DISP_PR_ERR("%s: ovl get cs ADOBE_RGB\n", __func__);
		/* fall through */
	case HAL_DATASPACE_STANDARD_BT2020:
		DISP_PR_ERR("%s: ovl does not support BT2020\n", __func__);
		/* fall through */
	default:
		cs = OVL_SRGB;
		break;
	}

	return cs;
}

static enum ovl_transfer ovl_map_transfer(enum android_dataspace ds)
{
	enum ovl_transfer xfr = OVL_GAMMA_UNKNOWN;

	switch (ds & HAL_DATASPACE_TRANSFER_MASK) {
	case HAL_DATASPACE_TRANSFER_LINEAR:
		xfr = OVL_LINEAR;
		break;
	case HAL_DATASPACE_TRANSFER_GAMMA2_6:
	case HAL_DATASPACE_TRANSFER_GAMMA2_8:
		DISP_PR_ERR("%s: ovl does not support gamma 2.6/2.8\n",
			    __func__);
		/* fall through */
	case HAL_DATASPACE_TRANSFER_ST2084:
	case HAL_DATASPACE_TRANSFER_HLG:
		DISP_PR_ERR("%s: HDR transfer\n", __func__);
		/* fall through */
	default:
		xfr = OVL_GAMMA2_2;
		break;
	}

	return xfr;
}

static u32 *get_ovl_csc(enum ovl_colorspace in, enum ovl_colorspace out)
{
	static u32 *ovl_csc[OVL_CS_NUM][OVL_CS_NUM];
	static bool inited;

	if (inited)
		goto done;

	ovl_csc[OVL_SRGB][OVL_P3] = sRGB_to_DCI_P3;
	ovl_csc[OVL_P3][OVL_SRGB] = DCI_P3_to_sRGB;

	inited = true;
done:
	return ovl_csc[in][out];
}

bool is_ovl_standard(enum android_dataspace ds)
{
	enum android_dataspace std = ds & HAL_DATASPACE_STANDARD_MASK;
	bool ret = false;

	if (!disp_helper_get_option(DISP_OPT_OVL_WCG) && is_ovl_wcg(ds))
		return ret;

	switch (std) {
	case HAL_DATASPACE_STANDARD_BT2020:
	case HAL_DATASPACE_STANDARD_BT2020_CONSTANT_LUMINANCE:
		ret = false;
		break;
	default:
		ret = true;
		break;
	}
	return ret;
}

bool is_ovl_wcg(enum android_dataspace ds)
{
	bool ret = false;

	switch (ds) {
	case HAL_DATASPACE_V0_SCRGB:
	case HAL_DATASPACE_V0_SCRGB_LINEAR:
	case HAL_DATASPACE_DISPLAY_P3:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

bool frame_has_wcg(struct disp_ddp_path_config *pconfig)
{
	s32 i = 0;
	struct OVL_CONFIG_STRUCT *c = NULL;

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		c = &pconfig->ovl_config[i];
		if (!c->layer_en)
			continue;
		if (is_ovl_wcg(c->ds))
			return true;
	}

	return false;
}

static int ovl_do_csc(struct OVL_CONFIG_STRUCT *c,
		      enum android_dataspace lcm_ds, u32 *wcg_en, void *qhandle)
{
	enum ovl_colorspace in = OVL_SRGB, out = OVL_SRGB;
	unsigned int fld = 0;
	unsigned long reg = 0;
	unsigned long baddr = ovl_base_addr(c->module);
	s32 i = 0;
	u32 *csc = NULL;
	bool en = false;

	in = ovl_map_cs(c->ds);
	out = ovl_map_cs(lcm_ds);
	DISPDBG("%s:L%d:%s->%s\n", __func__, c->layer,
		ovl_colorspace_str(in), ovl_colorspace_str(out));

	en = in != out;
	if (c->ext_layer != -1)
		fld = FLD_ELn_CSC_EN(c->ext_layer);
	else
		fld = FLD_Ln_CSC_EN(c->phy_layer);
	*wcg_en |= REG_FLD_VAL(fld, en);

	if (!en)
		return 0;

	csc = get_ovl_csc(in, out);
	if (!csc) {
		DISP_PR_ERR("%s:L%d:no ovl csc %s to %s, disable csc\n",
			    __func__, c->layer, ovl_colorspace_str(in),
			    ovl_colorspace_str(out));
		*wcg_en &= ~REG_FLD_VAL(fld, en);
		return 0;
	}

	if (c->ext_layer != -1)
		reg = DISP_REG_OVL_ELn_R2R_PARA(c->ext_layer);
	else
		reg = DISP_REG_OVL_Ln_R2R_PARA(c->phy_layer);

	for (i = 0; i < CSC_COEF_NUM; i++)
		DISP_REG_SET(qhandle, baddr + reg + 4 * i, csc[i]);

	return 0;
}

static int ovl_do_transfer(struct OVL_CONFIG_STRUCT *c,
			   enum android_dataspace lcm_ds,
			   u32 *wcg_en, u32 *gamma_sel)
{
	enum ovl_transfer xfr_in = OVL_GAMMA2_2, xfr_out = OVL_GAMMA2_2;
	enum ovl_colorspace cs_in = OVL_CS_UNKNOWN, cs_out = OVL_CS_UNKNOWN;
	unsigned int fld = 0;
	bool en = false;

	xfr_in = ovl_map_transfer(c->ds);
	xfr_out = ovl_map_transfer(lcm_ds);
	cs_in = ovl_map_cs(c->ds);
	cs_out = ovl_map_cs(lcm_ds);

	DISPDBG("%s:L%d:%s->%s\n", __func__, c->layer,
		ovl_transfer_str(xfr_in), ovl_transfer_str(xfr_out));

	en = xfr_in != OVL_LINEAR && (xfr_in != xfr_out || cs_in != cs_out);
	if (c->ext_layer != -1)
		fld = FLD_ELn_IGAMMA_EN(c->ext_layer);
	else
		fld = FLD_Ln_IGAMMA_EN(c->phy_layer);
	*wcg_en |= REG_FLD_VAL(fld, en);

	if (en) {
		if (c->ext_layer != -1)
			fld = FLD_ELn_IGAMMA_SEL(c->ext_layer);
		else
			fld = FLD_Ln_IGAMMA_SEL(c->phy_layer);
		*gamma_sel |= REG_FLD_VAL(fld, xfr_in);
	}

	en = xfr_out != OVL_LINEAR && (xfr_in != xfr_out || cs_in != cs_out);
	if (c->ext_layer != -1)
		fld = FLD_ELn_GAMMA_EN(c->ext_layer);
	else
		fld = FLD_Ln_GAMMA_EN(c->phy_layer);
	*wcg_en |= REG_FLD_VAL(fld, en);

	if (en) {
		if (c->ext_layer != -1)
			fld = FLD_ELn_GAMMA_SEL(c->ext_layer);
		else
			fld = FLD_Ln_GAMMA_SEL(c->phy_layer);
		*gamma_sel |= REG_FLD_VAL(fld, xfr_out);
	}

	return 0;
}

enum android_color_mode ovl_get_lcm_color_mode(struct disp_lcm_handle *plcm)
{
	if (!(plcm && plcm->params))
		return HAL_COLOR_MODE_NATIVE;

	return plcm->params->lcm_color_mode;
}

int ovl_color_manage(enum DISP_MODULE_ENUM module,
		     struct disp_ddp_path_config *pconfig, void *qhandle)
{
	s32 i = 0;
	struct OVL_CONFIG_STRUCT *c = NULL;
	u32 wcg_en = 0, gamma_sel = 0;
	enum android_color_mode lcm_cm;
	enum android_dataspace lcm_ds;
	unsigned long baddr = ovl_base_addr(module);

	if (!disp_helper_get_option(DISP_OPT_OVL_WCG))
		goto done;

	lcm_cm = ovl_get_lcm_color_mode(primary_get_lcm());
	lcm_ds = ovl_map_lcm_color_mode(lcm_cm);

	for (i = 0; i < TOTAL_OVL_LAYER_NUM; i++) {
		c = &pconfig->ovl_config[i];
		if (!c->layer_en)
			continue;
		if (c->module != module)
			continue;

		DISPDBG("%s:L%d:0x%08x->0x%08x\n",
			__func__, c->layer, c->ds, lcm_ds);
		ovl_do_transfer(c, lcm_ds, &wcg_en, &gamma_sel);
		ovl_do_csc(c, lcm_ds, &wcg_en, qhandle);
	}

done:
	DISP_REG_SET(qhandle, baddr + DISP_REG_OVL_WCG_CFG1, wcg_en);
	DISP_REG_SET(qhandle, baddr + DISP_REG_OVL_WCG_CFG2, gamma_sel);

	return 0;
}

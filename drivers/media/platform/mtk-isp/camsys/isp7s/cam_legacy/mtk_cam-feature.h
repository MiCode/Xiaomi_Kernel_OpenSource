/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_FEATURE_H
#define __MTK_CAM_FEATURE_H

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_camera-v4l2-controls.h"

static inline bool mtk_cam_feature_is_mstream_m2m(int feature)
{
	int raw_feature;

	if (!(feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK))
		return false;

	raw_feature = feature & MTK_CAM_FEATURE_HDR_MASK;
	if (raw_feature == MSTREAM_NE_SE ||
			raw_feature == MSTREAM_SE_NE)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_ext_isp_yuv(int feature)
{
	if (feature == EXT_ISP_CUS_2)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_ext_isp(int feature)
{
	if (feature & MTK_CAM_FEATURE_EXT_ISP_MASK)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_mstream(int feature)
{
	int raw_feature;

	if (feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK)
		return false;

	raw_feature = feature & MTK_CAM_FEATURE_HDR_MASK;
	if (raw_feature == MSTREAM_NE_SE || raw_feature == MSTREAM_SE_NE)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_time_shared(int feature)
{
	if (feature & MTK_CAM_FEATURE_TIMESHARE_MASK)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_stagger_m2m(int feature)
{
	int is_hdr;

	is_hdr = feature & MTK_CAM_FEATURE_HDR_MASK;
	if ((feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK) &&
		(is_hdr >= STAGGER_2_EXPOSURE_LE_SE &&
		is_hdr <= STAGGER_3_EXPOSURE_SE_NE_LE))
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_stagger_pure_m2m(int feature)
{
	int is_hdr;

	is_hdr = feature & MTK_CAM_FEATURE_HDR_MASK;
	if ((feature & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK) &&
		(is_hdr >= STAGGER_2_EXPOSURE_LE_SE &&
		is_hdr <= STAGGER_3_EXPOSURE_SE_NE_LE))
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_m2m(int feature)
{
	if (feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK ||
			feature & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_normal_pure_m2m(int feature)
{
	if ((feature & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK) &&
	     (feature & MTK_CAM_FEATURE_HDR_MASK) == 0)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_normal_m2m(int feature)
{
	if ((feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK) &&
	     (feature & MTK_CAM_FEATURE_HDR_MASK) == 0)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_pure_m2m(int feature)
{
	if (feature & MTK_CAM_FEATURE_PURE_OFFLINE_M2M_MASK)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_stagger(int feature)
{
	int is_hdr;

	if (feature & MTK_CAM_FEATURE_OFFLINE_M2M_MASK)
		return false;

	is_hdr = feature & MTK_CAM_FEATURE_HDR_MASK;
	if (is_hdr && is_hdr >= STAGGER_2_EXPOSURE_LE_SE &&
	    is_hdr <= STAGGER_3_EXPOSURE_SE_NE_LE)
		return true;

	return false;
}

static inline bool mtk_cam_feature_is_subsample(int feature)
{
	if (feature & MTK_CAM_FEATURE_SUBSAMPLE_MASK)
		return true;

	return false;
}


static inline bool mtk_cam_feature_change_is_mstream(int feature_change)
{
	if (feature_change & MSTREAM_EXPOSURE_CHANGE)
		return true;

	return false;
}

static inline bool mtk_cam_hw_mode_is_otf(int hw_mode)
{
	return (hw_mode == HW_MODE_ON_THE_FLY);
}

static inline bool mtk_cam_hw_mode_is_dc(int hw_mode)
{
	return (hw_mode == HW_MODE_DIRECT_COUPLED);
}

static inline bool mtk_cam_hw_mode_is_offline(int hw_mode)
{
	return (hw_mode == HW_MODE_OFFLINE);
}

static inline bool mtk_cam_hw_mode_is_m2m(int hw_mode)
{
	return (hw_mode == HW_MODE_M2M);
}

static inline void mtk_cam_scen_update_dbg_str(struct mtk_cam_scen *scen)
{
	switch (scen->id) {
	case MTK_CAM_SCEN_MSTREAM:
	case MTK_CAM_SCEN_ODT_MSTREAM:
		snprintf(scen->dbg_str, 15, "%d:%d:%d", scen->id,
			 scen->scen.mstream.type, scen->scen.mstream.mem_saving);
		break;
	case MTK_CAM_SCEN_SMVR:
		snprintf(scen->dbg_str, 15, "%d:%d", scen->id, scen->scen.smvr.subsample_num);
		break;
	case MTK_CAM_SCEN_STAGGER:
	case MTK_CAM_SCEN_ODT_STAGGER:
	case MTK_CAM_SCEN_M2M_STAGGER:
		snprintf(scen->dbg_str, 15, "%d:%d:%d:%d", scen->id,
			 scen->scen.stagger.type, scen->scen.stagger.max_exp_num,
			 scen->scen.stagger.mem_saving);
		break;
	case MTK_CAM_SCEN_TIMESHARE:
		snprintf(scen->dbg_str, 15, "%d:%d", scen->id, scen->scen.timeshare.group);
		break;
	case MTK_CAM_SCEN_EXT_ISP:
		snprintf(scen->dbg_str, 15, "%d:%d", scen->id, scen->scen.extisp.type);
		break;
	case MTK_CAM_SCEN_CAMSV_RGBW:
	case MTK_CAM_SCEN_STAGGER_RGBW:
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
	default:
		snprintf(scen->dbg_str, 15, "%d", scen->id);
		break;
	}
}

static inline void mtk_cam_scen_init(struct mtk_cam_scen *scen)
{
	if (!scen) {
		pr_info("%s: failed, scen can't be NULL", __func__);
		return;
	}

	scen->id = MTK_CAM_SCEN_NORMAL;
	mtk_cam_scen_update_dbg_str(scen);
}

static inline void
mtk_cam_stagger_feature_to_scen_type(struct mtk_cam_scen_stagger *scen,
				     int feature)
{
	int function_id;

	/* scen_type is determined by the feature V4L2_CID_MTK_CAM_FEATURE */
	function_id = feature & MTK_CAM_FEATURE_HDR_MASK;
	switch (function_id) {
	case 0:
		scen->type = MTK_CAM_STAGGER_1_EXPOSURE;
		break;
	case STAGGER_2_EXPOSURE_LE_SE:
		scen->type = MTK_CAM_STAGGER_2_EXPOSURE_LE_SE;
		break;
	case STAGGER_2_EXPOSURE_SE_LE:
		scen->type = MTK_CAM_STAGGER_2_EXPOSURE_SE_LE;
		break;
	case STAGGER_3_EXPOSURE_LE_NE_SE:
		scen->type = MTK_CAM_STAGGER_3_EXPOSURE_LE_NE_SE;
		break;
	case STAGGER_3_EXPOSURE_SE_NE_LE:
		scen->type = MTK_CAM_STAGGER_3_EXPOSURE_SE_NE_LE;
		break;
	default:
		pr_info("cam-isp:%s:scen_type err, unknown stagger feature(0x%x)",
				__func__, feature);
		break;
	}
}

static inline void
mtk_cam_stagger_feature_to_scen(struct mtk_cam_scen_stagger *scen,
				     int feature_in_res,
				     int feature)
{
	int function_id;
	/* max exp number is determined by the feature in resource */
	function_id = feature_in_res & MTK_CAM_FEATURE_HDR_MASK;
	switch (function_id) {
	case STAGGER_2_EXPOSURE_LE_SE:
	case STAGGER_2_EXPOSURE_SE_LE:
		scen->max_exp_num = 2;
		break;
	case STAGGER_3_EXPOSURE_LE_NE_SE:
	case STAGGER_3_EXPOSURE_SE_NE_LE:
		scen->max_exp_num = 3;
		break;
	default:
		pr_info("cam-isp:%s:max_exp_num err, unknown stagger feature(0x%x, 0x%x)",
			__func__, feature_in_res, feature);
		break;
	}

	mtk_cam_stagger_feature_to_scen_type(scen, feature);

	function_id = feature & MTK_CAM_FEATURE_HDR_MEMORY_SAVING_MASK;
	if (function_id == HDR_MEMORY_SAVING)
		scen->mem_saving = true;
	else
		scen->mem_saving = false;
}

static inline void
mtk_cam_mstream_feature_to_scen_type(struct mtk_cam_scen_mstream *scen,
				     int feature)
{
	int function_id;

	/* scen_type is determined by the feature V4L2_CID_MTK_CAM_FEATURE */
	function_id = feature & MTK_CAM_FEATURE_HDR_MASK;
	switch (function_id) {
	case 0:
		scen->type = MTK_CAM_MSTREAM_1_EXPOSURE;
		break;
	case MSTREAM_NE_SE:
		scen->type = MTK_CAM_MSTREAM_NE_SE;
		break;
	case MSTREAM_SE_NE:
		scen->type = MTK_CAM_MSTREAM_SE_NE;
		break;
	default:
		pr_info("cam-isp:%s:scen_type err, unknown mstream feature(0x%x)",
			__func__, feature);
		break;
	}
}

static inline void
mtk_cam_mstream_feature_to_scen(struct mtk_cam_scen_mstream *scen,
				     int feature_in_res,
				     int feature)
{
	int function_id;

	mtk_cam_mstream_feature_to_scen_type(scen, feature);

	function_id = feature & MTK_CAM_FEATURE_HDR_MEMORY_SAVING_MASK;
	if (function_id == HDR_MEMORY_SAVING)
		scen->mem_saving = true;
	else
		scen->mem_saving = false;
}

static inline bool mtk_cam_scen_is_mstream_types(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_MSTREAM) ||
	       (scen->id == MTK_CAM_SCEN_ODT_MSTREAM);
}

static inline bool mtk_cam_scen_is_stagger_types(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	if (scen->id == MTK_CAM_SCEN_STAGGER ||
		scen->id == MTK_CAM_SCEN_ODT_STAGGER ||
		scen->id == MTK_CAM_SCEN_M2M_STAGGER)
		return true;
	return false;
}

static inline void mtk_cam_scen_update_feature_to_type(struct mtk_cam_scen *scen,
			int feature)
{
	/* only allow stagger and mstream strcut's update */
	if (mtk_cam_scen_is_mstream_types(scen))
		mtk_cam_mstream_feature_to_scen_type(&scen->scen.mstream, feature);
	else if (mtk_cam_scen_is_stagger_types(scen))
		mtk_cam_stagger_feature_to_scen_type(&scen->scen.stagger, feature);

	mtk_cam_scen_update_dbg_str(scen);
}

static inline void mtk_cam_feature_to_scen(struct mtk_cam_scen *scen,
					   int feature_in_res,
					   int feature)
{
	int function_id;

	if (feature_in_res == 0 && feature == 0) {
		scen->id = MTK_CAM_SCEN_NORMAL;
	} else if (mtk_cam_feature_is_stagger(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_STAGGER;
		mtk_cam_stagger_feature_to_scen(&scen->scen.stagger, feature_in_res, feature);
	}  else if (mtk_cam_feature_is_mstream(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_MSTREAM;
		mtk_cam_mstream_feature_to_scen(&scen->scen.mstream, feature_in_res, feature);
	/* ODT */
	} else if (mtk_cam_feature_is_normal_m2m(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_ODT_NORMAL;
	} else if (mtk_cam_feature_is_stagger_m2m(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_ODT_STAGGER;
		mtk_cam_stagger_feature_to_scen(&scen->scen.stagger, feature_in_res, feature);
	} else if (mtk_cam_feature_is_mstream_m2m(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_ODT_MSTREAM;
		mtk_cam_mstream_feature_to_scen(&scen->scen.mstream, feature_in_res, feature);
	/* M2M (VSE, P1B)*/
	} else if (mtk_cam_feature_is_normal_pure_m2m(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_M2M_NORMAL; /* used by VSB, P1B */
	} else if (mtk_cam_feature_is_stagger_pure_m2m(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_M2M_STAGGER; /* used by VSB */
		mtk_cam_stagger_feature_to_scen(&scen->scen.stagger, feature_in_res, feature);
	} else if (mtk_cam_feature_is_subsample(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_SMVR;
		function_id = feature_in_res & MTK_CAM_FEATURE_SUBSAMPLE_MASK;
		switch (function_id) {
		case HIGHFPS_2_SUBSAMPLE:
			scen->scen.smvr.subsample_num = 2;
			break;
		case HIGHFPS_4_SUBSAMPLE:
			scen->scen.smvr.subsample_num = 4;
			break;
		case HIGHFPS_8_SUBSAMPLE:
			scen->scen.smvr.subsample_num = 8;
			break;
		case HIGHFPS_16_SUBSAMPLE:
			scen->scen.smvr.subsample_num = 16;
			break;
		case HIGHFPS_32_SUBSAMPLE:
			scen->scen.smvr.subsample_num = 32;
			break;
		default:
			pr_info("cam-isp:%s:subsample_num err, unknown smvr feature(0x%x, 0x%x)",
				__func__, feature_in_res, feature);
			break;
		}
	} else if (mtk_cam_feature_is_time_shared(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_TIMESHARE;
		function_id = feature_in_res & MTK_CAM_FEATURE_TIMESHARE_MASK;
		switch (function_id) {
		case TIMESHARE_1_GROUP:
			scen->scen.timeshare.group = MTK_CAM_TIMESHARE_GROUP_1;
			break;
		default:
			pr_info("cam-isp:%s:timeshare type err, unknown timeshare feature(0x%x, 0x%x)",
				__func__, feature_in_res, feature);
			break;
		}
	} else if (feature_in_res & WITH_W_CHANNEL) {
		scen->id = MTK_CAM_SCEN_CAMSV_RGBW;
	} else if (mtk_cam_feature_is_ext_isp(feature_in_res)) {
		scen->id = MTK_CAM_SCEN_EXT_ISP;
		function_id = feature_in_res & MTK_CAM_FEATURE_EXT_ISP_MASK;
		switch (function_id) {
		case EXT_ISP_CUS_1:
			scen->scen.extisp.type = MTK_CAM_EXTISP_CUS_1;
			break;
		case EXT_ISP_CUS_2:
			scen->scen.extisp.type = MTK_CAM_EXTISP_CUS_2;
			break;
		case EXT_ISP_CUS_3:
			scen->scen.extisp.type = MTK_CAM_EXTISP_CUS_3;
			break;
		default:
			pr_info("cam-isp:%s:extisp type err, unknown extisp feature(0x%x, 0x%x)",
				__func__, feature_in_res, feature);
			break;
		}
	}

	mtk_cam_scen_update_dbg_str(scen);
}


static inline void
mtk_cam_stagger_scen_type_to_feature(int *feature,
				     struct mtk_cam_scen_stagger *scen)
{
	switch (scen->type) {
	case MTK_CAM_STAGGER_1_EXPOSURE:
		break;
	case MTK_CAM_STAGGER_2_EXPOSURE_LE_SE:
		*feature |= STAGGER_2_EXPOSURE_LE_SE;
		break;
	case MTK_CAM_STAGGER_2_EXPOSURE_SE_LE:
		*feature |= STAGGER_2_EXPOSURE_SE_LE;
		break;
	case MTK_CAM_STAGGER_3_EXPOSURE_LE_NE_SE:
		*feature |= STAGGER_3_EXPOSURE_LE_NE_SE;
		break;
	case MTK_CAM_STAGGER_3_EXPOSURE_SE_NE_LE:
		*feature |= STAGGER_3_EXPOSURE_SE_NE_LE;
		break;
	default:
		pr_info("cam-isp:%s:max_exp_num err, unknown stagger type(%d)",
			__func__, scen->type);
		break;
	}

	if (scen->mem_saving)
		*feature |= HDR_MEMORY_SAVING;
}

static inline void
mtk_cam_mstream_scen_type_to_feature(int *feature,
				     struct mtk_cam_scen_mstream *scen)
{
	switch (scen->type) {
	case MTK_CAM_MSTREAM_1_EXPOSURE:
		break;
	case MTK_CAM_MSTREAM_NE_SE:
		*feature |= MSTREAM_NE_SE;
		break;
	case MTK_CAM_MSTREAM_SE_NE:
		*feature |= MSTREAM_SE_NE;
		break;
	default:
		pr_info("cam-isp:%s:scen_type err, unknown mstream type(%d)",
			__func__, scen->type);
		break;

	}

	if (scen->mem_saving)
		*feature |= HDR_MEMORY_SAVING;
}

static inline void mtk_cam_scen_to_feature(int *feature,
					   struct mtk_cam_scen *scen)
{
	switch (scen->id) {
	case MTK_CAM_SCEN_NORMAL:
		*feature = 0;
		break;
	case MTK_CAM_SCEN_STAGGER:
		mtk_cam_stagger_scen_type_to_feature(feature,
						     &scen->scen.stagger);
		break;
	case MTK_CAM_SCEN_MSTREAM:
		mtk_cam_mstream_scen_type_to_feature(feature,
						     &scen->scen.mstream);
		break;
	case MTK_CAM_SCEN_ODT_NORMAL:
		*feature |= OFFLINE_M2M;
		break;
	case MTK_CAM_SCEN_ODT_STAGGER:
		*feature |= OFFLINE_M2M;
		mtk_cam_stagger_scen_type_to_feature(feature,
						     &scen->scen.stagger);
		break;
	case MTK_CAM_SCEN_ODT_MSTREAM:
		*feature |= OFFLINE_M2M;
		mtk_cam_mstream_scen_type_to_feature(feature,
						     &scen->scen.mstream);
		break;
	case MTK_CAM_SCEN_M2M_NORMAL:
		*feature |= PURE_OFFLINE_M2M;
		break;
	case MTK_CAM_SCEN_M2M_STAGGER:
		*feature |= PURE_OFFLINE_M2M;
		mtk_cam_stagger_scen_type_to_feature(feature,
						     &scen->scen.stagger);
		break;
	case MTK_CAM_SCEN_SMVR:
		*feature |= MTK_CAM_SCEN_SMVR;
		switch (scen->scen.smvr.subsample_num) {
		case 2:
			*feature |= HIGHFPS_2_SUBSAMPLE;
			break;
		case 4:
			*feature |= HIGHFPS_4_SUBSAMPLE;
			break;
		case 8:
			*feature |= HIGHFPS_8_SUBSAMPLE;
			break;
		case 16:
			*feature |= HIGHFPS_16_SUBSAMPLE;
			break;
		case 32:
			*feature |= HIGHFPS_16_SUBSAMPLE;
			break;
		default:
			pr_info("cam-isp:%s:subsample_num err, unknown value(%d)",
				__func__,
				scen->scen.smvr.subsample_num);
			break;
		}
		break;
	case MTK_CAM_SCEN_TIMESHARE:
		*feature |= TIMESHARE_1_GROUP;
		break;
	case MTK_CAM_SCEN_CAMSV_RGBW:
		*feature |= WITH_W_CHANNEL;
		break;
	case MTK_CAM_SCEN_EXT_ISP:
		switch (scen->scen.extisp.type) {
		case MTK_CAM_EXTISP_CUS_1:
			*feature |= EXT_ISP_CUS_1;
			break;
		case MTK_CAM_EXTISP_CUS_2:
			*feature |= EXT_ISP_CUS_2;
			break;
		case MTK_CAM_EXTISP_CUS_3:
			*feature |= EXT_ISP_CUS_3;
			break;
		default:
			pr_info("cam-isp:%s:extisp type err, unknown value(%d)",
				__func__, scen->scen.extisp.type);
			break;
		}
		break;
	default:
		pr_info("cam-isp:%s: unknown or 7.1 not supported scen(0xd)",
			__func__, scen->id);
		break;
	}
}

/* Utilities */
static inline bool mtk_cam_scen_is_mstream_m2m(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_ODT_MSTREAM);
}


static inline bool mtk_cam_scen_is_ext_isp_yuv(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_EXT_ISP &&
		scen->scen.extisp.type == MTK_CAM_EXTISP_CUS_2);
}


static inline bool mtk_cam_scen_is_ext_isp(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_EXT_ISP);
}

static inline bool mtk_cam_scen_is_mstream(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_MSTREAM);
}

static inline bool mtk_cam_scen_is_time_shared(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_TIMESHARE);
}

static inline bool mtk_cam_scen_is_stagger_m2m(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_ODT_STAGGER);
}

static inline bool mtk_cam_scen_is_stagger_pure_m2m(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_M2M_STAGGER);
}

static inline bool mtk_cam_scen_is_odt(struct mtk_cam_scen *scen)
{
	return (scen->id == MTK_CAM_SCEN_ODT_NORMAL) ||
	       (scen->id == MTK_CAM_SCEN_ODT_STAGGER) ||
	       (scen->id == MTK_CAM_SCEN_ODT_MSTREAM);

}

/* is pure offline nad offline */
static inline bool mtk_cam_scen_is_m2m(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_ODT_NORMAL) ||
	       (scen->id == MTK_CAM_SCEN_ODT_STAGGER) ||
	       (scen->id == MTK_CAM_SCEN_ODT_MSTREAM) ||
	       (scen->id == MTK_CAM_SCEN_M2M_NORMAL) ||
	       (scen->id == MTK_CAM_SCEN_M2M_STAGGER);
}

static inline bool mtk_cam_scen_is_normal_pure_m2m(struct mtk_cam_scen *scen)
{
	return scen->id == MTK_CAM_SCEN_M2M_NORMAL;
}

static inline bool mtk_cam_scen_is_normal_m2m(struct mtk_cam_scen *scen)
{
	return scen->id == MTK_CAM_SCEN_ODT_NORMAL;
}

static inline bool mtk_cam_scen_is_pure_m2m(struct mtk_cam_scen *scen)
{
	return (scen->id == MTK_CAM_SCEN_M2M_NORMAL) ||
	       (scen->id == MTK_CAM_SCEN_M2M_STAGGER);
}

static inline bool mtk_cam_scen_is_stagger(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return scen->id == MTK_CAM_SCEN_STAGGER;
}

static inline bool mtk_cam_scen_is_subsample(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_SMVR);
}

static inline bool mtk_cam_scen_is_stagger_2_exp(struct mtk_cam_scen *scen)
{
	return (scen->scen.stagger.type == MTK_CAM_STAGGER_2_EXPOSURE_LE_SE) ||
	       (scen->scen.stagger.type == MTK_CAM_STAGGER_2_EXPOSURE_SE_LE);
}

static inline bool mtk_cam_scen_is_stagger_3_exp(struct mtk_cam_scen *scen)
{
	return (scen->scen.stagger.type == MTK_CAM_STAGGER_3_EXPOSURE_LE_NE_SE) ||
	       (scen->scen.stagger.type == MTK_CAM_STAGGER_3_EXPOSURE_SE_NE_LE);
}

static inline bool mtk_cam_scen_is_hdr(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return mtk_cam_scen_is_mstream_types(scen) ||
	       mtk_cam_scen_is_stagger_types(scen);
}

static inline bool mtk_cam_scen_is_switchable_hdr(struct mtk_cam_scen *scen)
{
	return mtk_cam_scen_is_mstream(scen) ||
	       mtk_cam_scen_is_stagger(scen);
}

static inline int mtk_cam_scen_get_stagger_exp_num(struct mtk_cam_scen *scen)
{
	int exp_num = 1;

	if (mtk_cam_scen_is_stagger_types(scen)) {
		if (mtk_cam_scen_is_stagger_2_exp(scen))
			exp_num = 2;
		else if (mtk_cam_scen_is_stagger_3_exp(scen))
			exp_num = 3;
	}

	return exp_num;
}

static inline bool mtk_cam_scen_type_mstream_is_2_exp(struct mtk_cam_scen_mstream *mstream)
{
		return mstream->type == MTK_CAM_MSTREAM_NE_SE ||
	       mstream->type == MTK_CAM_MSTREAM_SE_NE;
}

static inline bool mtk_cam_scen_is_mstream_2exp_types(struct mtk_cam_scen *scen)
{
		if (!scen)
			return false;
		if (mtk_cam_scen_is_mstream_types(scen) &&
			mtk_cam_scen_type_mstream_is_2_exp(&scen->scen.mstream))
			return true;

		return false;
}

static inline bool mtk_cam_scen_is_mstream_is_2_exp(struct mtk_cam_scen *scen)
{
	return mtk_cam_scen_is_mstream(scen) &&
		mtk_cam_scen_is_mstream_2exp_types(scen);
}

static inline bool mtk_cam_scen_is_2_exp(struct mtk_cam_scen *scen)
{
	if (mtk_cam_scen_is_stagger_types(scen)) {
		if (mtk_cam_scen_is_stagger_2_exp(scen))
			return true;
	}

	if (mtk_cam_scen_is_mstream_2exp_types(scen))
		return true;

	return false;
}

static inline bool mtk_cam_scen_is_3_exp(struct mtk_cam_scen *scen)
{
	if (mtk_cam_scen_is_stagger_types(scen) &&
	    mtk_cam_scen_is_stagger_3_exp(scen))
		return true;

	return false;
}

static inline int mtk_cam_scen_get_max_exp_num(struct mtk_cam_scen *scen)
{
	int exp_num = 1;

	if (mtk_cam_scen_is_stagger_types(scen))
		exp_num = scen->scen.stagger.max_exp_num;
	else if (mtk_cam_scen_is_mstream_types(scen))
		exp_num = 2;

	return exp_num;
}

static inline int mtk_cam_scen_get_exp_num(struct mtk_cam_scen *scen)
{
	int exp_num;

	if (mtk_cam_scen_is_2_exp(scen))
		exp_num = 2;
	else if (mtk_cam_scen_is_3_exp(scen))
		exp_num = 3;
	else
		exp_num = 1;

	return exp_num;
}

static inline int mtk_cam_scen_is_hdr_save_mem(struct mtk_cam_scen *scen)
{
	if (mtk_cam_scen_is_mstream_types(scen))
		return scen->scen.mstream.mem_saving;
	if (mtk_cam_scen_is_stagger_types(scen))
		return scen->scen.stagger.mem_saving;

	return false;
}

static inline bool mtk_cam_scen_is_with_w_channel(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return scen->id == MTK_CAM_SCEN_CAMSV_RGBW;
}

bool mtk_cam_is_hsf(struct mtk_cam_ctx *ctx);
bool mtk_cam_feature_change_is_mstream(int feature_change);
int mtk_cam_get_feature_switch(struct mtk_raw_pipeline *raw_pipe,
			       struct mtk_cam_scen *prev);
bool mtk_cam_hw_is_otf(struct mtk_cam_ctx *ctx);
bool mtk_cam_hw_is_dc(struct mtk_cam_ctx *ctx);
bool mtk_cam_hw_is_offline(struct mtk_cam_ctx *ctx);
bool mtk_cam_hw_is_m2m(struct mtk_cam_ctx *ctx);

bool mtk_cam_is_srt(int hw_mode);

#endif /*__MTK_CAM_FEATURE_H */

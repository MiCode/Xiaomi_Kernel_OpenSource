/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_FEATURE_H
#define __MTK_CAM_FEATURE_H

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_camera-v4l2-controls.h"

/* TODO(MTK): mtk_cam-util.c */
#define snprintf_safe(buf, size, fmt, args...) \
({ \
	int r = snprintf(buf, size, fmt, ## args); \
	if (r < 0 || r > size) { \
		WARN_ON(1); \
		pr_info("%s:%s:%d snprintf failed", \
			__func__, __FILE__, __LINE__); \
	} \
})

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
		snprintf_safe(scen->dbg_str, 15, "%d:%d:%d", scen->id,
			      scen->scen.mstream.type,
			      scen->scen.mstream.mem_saving);
		break;
	case MTK_CAM_SCEN_SMVR:
		snprintf_safe(scen->dbg_str, 15, "%d:%d:%d", scen->id,
			 scen->scen.smvr.subsample_num,
			 scen->scen.smvr.output_first_frame_only);
		break;
	case MTK_CAM_SCEN_NORMAL:
	case MTK_CAM_SCEN_ODT_NORMAL:
	case MTK_CAM_SCEN_M2M_NORMAL:
		snprintf_safe(scen->dbg_str, 15, "%d:%d:%d:%d",
			      scen->scen.normal.max_exp_num,
			      scen->scen.normal.exp_num,
			      scen->scen.normal.w_chn_supported,
			      scen->scen.normal.frame_order);
		break;
	case MTK_CAM_SCEN_TIMESHARE:
		snprintf_safe(scen->dbg_str, 15, "%d:%d", scen->id,
			      scen->scen.timeshare.group);
		break;
	case MTK_CAM_SCEN_EXT_ISP:
		snprintf_safe(scen->dbg_str, 15, "%d:%d", scen->id,
			      scen->scen.extisp.type);
		break;
	case MTK_CAM_SCEN_CAMSV_RGBW:
	default:
		snprintf_safe(scen->dbg_str, 15, "%d", scen->id);
		break;
	}
}

static inline void mtk_cam_scen_init(struct mtk_cam_scen *scen)
{
	if (!scen) {
		pr_info("%s: failed, scen can't be NULL", __func__);
		return;
	}

	memset(scen, 0, sizeof(*scen));

	scen->id = MTK_CAM_SCEN_NORMAL;
	mtk_cam_scen_update_dbg_str(scen);
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
	if (scen->id == MTK_CAM_SCEN_NORMAL ||
		scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
		scen->id == MTK_CAM_SCEN_M2M_NORMAL)
		return (scen->scen.normal.max_exp_num > 1);
	return false;
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
	return scen->id == MTK_CAM_SCEN_ODT_NORMAL &&
		   (scen->scen.normal.max_exp_num > 1);
}

static inline bool mtk_cam_scen_is_stagger_pure_m2m(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_M2M_NORMAL) &&
		   (scen->scen.normal.max_exp_num > 1);
}

static inline bool mtk_cam_scen_is_odt(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_ODT_NORMAL) ||
	       (scen->id == MTK_CAM_SCEN_ODT_MSTREAM);
}

/* is pure offline nad offline */
/* use MTK_CAM_SCEN_M2M_NORMAL or create new one */
static inline bool mtk_cam_scen_is_m2m(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_ODT_NORMAL) ||
	       (scen->id == MTK_CAM_SCEN_ODT_MSTREAM) ||
	       (scen->id == MTK_CAM_SCEN_M2M_NORMAL);
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
	return (scen->id == MTK_CAM_SCEN_M2M_NORMAL);
}

static inline bool mtk_cam_scen_is_sensor_normal(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return scen->id == MTK_CAM_SCEN_NORMAL &&
		scen->scen.normal.max_exp_num == 1;
}

static inline bool mtk_cam_scen_is_sensor_stagger(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return scen->id == MTK_CAM_SCEN_NORMAL &&
		scen->scen.normal.max_exp_num > 1;
}

static inline bool mtk_cam_scen_is_subsample(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return (scen->id == MTK_CAM_SCEN_SMVR);
}

static inline bool mtk_cam_scen_is_subsample_1st_frame_only(struct mtk_cam_scen *scen)
{
	if (!scen || scen->id != MTK_CAM_SCEN_SMVR)
		return false;
	return (scen->scen.smvr.output_first_frame_only != 0);
}


static inline bool mtk_cam_scen_is_stagger_2_exp(struct mtk_cam_scen *scen)
{
	return mtk_cam_scen_is_stagger_types(scen) && scen->scen.normal.exp_num == 2;
}

static inline bool mtk_cam_scen_is_stagger_3_exp(struct mtk_cam_scen *scen)
{
	return mtk_cam_scen_is_stagger_types(scen) && scen->scen.normal.exp_num == 3;
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
	       mtk_cam_scen_is_mstream_m2m(scen) ||
	       mtk_cam_scen_is_sensor_stagger(scen);
}

static inline int mtk_cam_scen_get_stagger_exp_num(struct mtk_cam_scen *scen)
{
	return (mtk_cam_scen_is_stagger_types(scen) ?
				scen->scen.normal.exp_num : 1);
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
	return mtk_cam_scen_is_stagger_3_exp(scen);
}

static inline int mtk_cam_scen_get_max_exp_num(struct mtk_cam_scen *scen)
{
	int exp_num = 1;

	if (mtk_cam_scen_is_stagger_types(scen))
		exp_num = scen->scen.normal.max_exp_num;
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
		return scen->scen.normal.mem_saving;

	return false;
}

static inline bool mtk_cam_scen_is_rgbw_using_camsv(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;
	return scen->id == MTK_CAM_SCEN_CAMSV_RGBW;
}

static inline bool mtk_cam_scen_is_rgbw_supported(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;

	if (scen->id == MTK_CAM_SCEN_NORMAL ||
		scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
		scen->id == MTK_CAM_SCEN_M2M_NORMAL)
		return !!(scen->scen.normal.w_chn_supported);

	return false;
}

static inline bool mtk_cam_scen_is_rgbw_enabled(struct mtk_cam_scen *scen)
{
	if (!scen)
		return false;

	if (scen->id == MTK_CAM_SCEN_NORMAL ||
		scen->id == MTK_CAM_SCEN_ODT_NORMAL ||
		scen->id == MTK_CAM_SCEN_M2M_NORMAL)
		return !!(scen->scen.normal.w_chn_enabled);

	return false;
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

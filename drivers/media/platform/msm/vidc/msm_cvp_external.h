/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_EXTERNAL_H_
#define _MSM_CVP_EXTERNAL_H_

#include <media/msm_media_info.h>
#include <media/msm_cvp_private.h>
#include <media/msm_cvp_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"

#define CVP_DME                                 (24)

#define HFI_COMMON_BASE                         (0)
#define HFI_VIDEO_DOMAIN_CVP                    (HFI_COMMON_BASE + 0x8)
#define HFI_DOMAIN_BASE_COMMON                  (HFI_COMMON_BASE + 0)
#define HFI_DOMAIN_BASE_CVP                     (HFI_COMMON_BASE + 0x04000000)
#define HFI_ARCH_COMMON_OFFSET                  (0)
#define HFI_CMD_START_OFFSET                    (0x00010000)
#define HFI_CMD_SESSION_CVP_START \
		(HFI_DOMAIN_BASE_CVP + HFI_ARCH_COMMON_OFFSET +	\
		HFI_CMD_START_OFFSET + 0x1000)

#define HFI_CMD_SESSION_CVP_DME_FRAME \
		(HFI_CMD_SESSION_CVP_START + 0x03A)
#define HFI_CMD_SESSION_CVP_DME_BASIC_CONFIG \
		(HFI_CMD_SESSION_CVP_START + 0x03B)
#define HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS \
		(HFI_CMD_SESSION_CVP_START + 0x04D)
#define HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS \
		(HFI_CMD_SESSION_CVP_START + 0x050)

#define HFI_DME_OUTPUT_BUFFER_SIZE              (256 * 4)
#define HFI_DME_INTERNAL_PERSIST_2_BUFFER_SIZE  (512 * 1024)
#define HFI_DME_FRAME_CONTEXT_BUFFER_SIZE       (64 * 1024)

enum HFI_COLOR_PLANE_TYPE {
	HFI_COLOR_PLANE_METADATA,
	HFI_COLOR_PLANE_PICDATA,
	HFI_COLOR_PLANE_UV_META,
	HFI_COLOR_PLANE_UV,
	HFI_MAX_COLOR_PLANES
};

static inline bool is_vidc_cvp_enabled(struct msm_vidc_inst *inst)
{
	return !!inst->cvp;
}

static inline bool is_vidc_cvp_allowed(struct msm_vidc_inst *inst)
{
	return false;
}

struct msm_cvp_buffer_type {
	u32 buffer_addr;
	u32 size;
};

struct msm_cvp_session_release_persist_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_kmd_client_data client_data;
	u32 cvp_op;
	struct msm_cvp_buffer_type persist1_buffer;
	struct msm_cvp_buffer_type persist2_buffer;
};

struct msm_cvp_session_set_persist_buffers_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_kmd_client_data client_data;
	u32 cvp_op;
	struct msm_cvp_buffer_type persist1_buffer;
	struct msm_cvp_buffer_type persist2_buffer;
};

struct msm_cvp_dme_frame_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_kmd_client_data client_data;
	u32 skip_mv_calc;
	u32 min_fpx_threshold;
	u32 enable_descriptor_lpf;
	u32 enable_ncc_subpel;
	u32 descmatch_threshold;
	int ncc_robustness_threshold;
	struct msm_cvp_buffer_type fullres_srcbuffer;
	struct msm_cvp_buffer_type src_buffer;
	struct msm_cvp_buffer_type srcframe_contextbuffer;
	struct msm_cvp_buffer_type prsp_buffer;
	struct msm_cvp_buffer_type grid_buffer;
	struct msm_cvp_buffer_type ref_buffer;
	struct msm_cvp_buffer_type refframe_contextbuffer;
	struct msm_cvp_buffer_type videospatialtemporal_statsbuffer;
};

struct msm_cvp_dme_basic_config_packet {
	u32 size;
	u32 packet_type;
	u32 session_id;
	struct cvp_kmd_client_data client_data;
	u32 srcbuffer_format;
	struct cvp_kmd_color_plane_info srcbuffer_planeinfo;
	u32 src_width;
	u32 src_height;
	u32 fullres_width;
	u32 fullres_height;
	u32 fullresbuffer_format;
	struct cvp_kmd_color_plane_info fullresbuffer_planeinfo;
	u32 ds_enable;
	u32 enable_lrme_robustness;
	u32 enable_inlier_tracking;
	u32 override_defaults;
	s32 inlier_step;
	s32 outlier_step;
	s32 follow_globalmotion_step;
	s32 nomv_conveyedinfo_step;
	s32 invalid_transform_step;
	s32 valid_transform_min_confidence_for_updates;
	u32 min_inlier_weight_threshold;
	u32 ncc_threshold;
	u32 min_allowed_tar_var;
	u32 meaningful_ncc_diff;
	u32 robustness_distmap[8];
	u32 ransac_threshold;
};

struct msm_cvp_buf {
	u32 index;
	int fd;
	u32 size;
	u32 offset;
	struct dma_buf *dbuf;
	void *kvaddr;
};

struct msm_cvp_external {
	void *priv;
	u32 session_id;
	u32 width;
	u32 height;
	u32 ds_width;
	u32 ds_height;
	bool downscale;
	u32 framecount;
	u32 buffer_idx;
	struct msm_cvp_buf fullres_buffer;
	struct msm_cvp_buf src_buffer;
	struct msm_cvp_buf ref_buffer;
	struct msm_cvp_buf output_buffer;
	struct msm_cvp_buf context_buffer;
	struct msm_cvp_buf refcontext_buffer;
	struct msm_cvp_buf persist2_buffer;
};

int msm_vidc_cvp_preprocess(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb);
int msm_vidc_cvp_prepare_preprocess(struct msm_vidc_inst *inst);
int msm_vidc_cvp_unprepare_preprocess(struct msm_vidc_inst *inst);

#endif

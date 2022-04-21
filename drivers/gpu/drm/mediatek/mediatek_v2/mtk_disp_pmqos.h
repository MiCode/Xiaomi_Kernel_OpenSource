/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_PMQOS_H_
#define _MTK_DRM_PMQOS_H_

//#include <linux/interconnect-provider.h>
#include "mtk-interconnect-provider.h"
#include <linux/pm_qos.h>

enum DISP_QOS_BW_MODE {
	DISP_BW_NORMAL_MODE = 0,
	DISP_BW_FBDC_MODE,
	DISP_BW_HRT_MODE,
};

#define NO_PENDING_HRT (0xFFFF)
#define OVL_REQ_HRT (0x1)
#define RDMA_REQ_HRT (0x2)

struct drm_crtc;
struct mtk_drm_crtc;
struct mtk_ddp_comp;

struct mtk_drm_qos_ctx {
	unsigned int last_hrt_req;
	unsigned int last_mmclk_req_idx;
	atomic_t last_hrt_idx;
	atomic_t hrt_cond_sig;
	wait_queue_head_t hrt_cond_wq;
};

void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event);
int __mtk_disp_set_module_bw(struct icc_path *request, int comp_id,
			     unsigned int bandwidth, unsigned int bw_mode);
void __mtk_disp_set_module_hrt(struct icc_path *request,
			       unsigned int bandwidth);
int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc,
			unsigned int overlap_num);
void mtk_drm_pan_disp_set_hrt_bw(struct drm_crtc *crtc, const char *caller);
int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode);
int mtk_disp_hrt_cond_init(struct drm_crtc *crtc);
void mtk_drm_mmdvfs_init(struct device *dev);
unsigned int mtk_drm_get_mmclk_step_size(void);
void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, const char *caller);
void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc, unsigned int pixclk,
			const char *caller);
unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller);
unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller);
#endif

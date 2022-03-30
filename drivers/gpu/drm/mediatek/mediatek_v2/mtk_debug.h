/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTKFB_DEBUG_H
#define __MTKFB_DEBUG_H

#define LOGGER_BUFFER_SIZE (16 * 1024)
#define ERROR_BUFFER_COUNT 4
#define FENCE_BUFFER_COUNT 22
#define DEBUG_BUFFER_COUNT 30
#define DUMP_BUFFER_COUNT 10
#define STATUS_BUFFER_COUNT 1
#define _DRM_P_H_
#if defined(CONFIG_MT_ENG_BUILD) || !defined(CONFIG_MTK_GMO_RAM_OPTIMIZE)
#define DEBUG_BUFFER_SIZE                                                      \
	(4096 +                                                                \
	 (ERROR_BUFFER_COUNT + FENCE_BUFFER_COUNT + DEBUG_BUFFER_COUNT +       \
	  DUMP_BUFFER_COUNT + STATUS_BUFFER_COUNT) *                           \
		 LOGGER_BUFFER_SIZE)
#else
#define DEBUG_BUFFER_SIZE 10240
#endif

extern void disp_color_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_ccorr_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_gamma_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_dither_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_aal_set_bypass(struct drm_crtc *crtc, int bypass);
extern void disp_dither_set_color_detect(struct drm_crtc *crtc, int enable);
extern void mtk_trans_gain_to_gamma(struct drm_crtc *crtc,
	unsigned int gain[3], unsigned int bl);

extern unsigned int m_new_pq_persist_property[32];
enum mtk_pq_persist_property {
	DISP_PQ_COLOR_BYPASS,
	DISP_PQ_CCORR_BYPASS,
	DISP_PQ_GAMMA_BYPASS,
	DISP_PQ_DITHER_BYPASS,
	DISP_PQ_AAL_BYPASS,
	DISP_PQ_C3D_BYPASS,
	DISP_PQ_TDSHP_BYPASS,
	DISP_PQ_CCORR_SILKY_BRIGHTNESS,
	DISP_PQ_GAMMA_SILKY_BRIGHTNESS,
	DISP_PQ_DITHER_COLOR_DETECT,
	DISP_PQ_PROPERTY_MAX,
};

int mtk_drm_ioctl_pq_get_persist_property(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

extern int mtk_disp_hrt_bw_dbg(void);

#ifdef _DRM_P_H_
struct disp_rect {
	u32 x;
	u32 y;
	u32 width;
	u32 height;
};
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *drm_dev);
void disp_dbg_deinit(void);
void mtk_drm_cwb_backup_copy_size(void);
int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);
int mtk_dprec_mmp_dump_cwb_buffer(struct drm_crtc *crtc,
	void *buffer, unsigned int buf_idx);
int disp_met_set(void *data, u64 val);
void mtk_drm_idlemgr_kick_ext(const char *source);
unsigned int mtk_dbg_get_lfr_mode_value(void);
unsigned int mtk_dbg_get_lfr_type_value(void);
unsigned int mtk_dbg_get_lfr_enable_value(void);
unsigned int mtk_dbg_get_lfr_update_value(void);
unsigned int mtk_dbg_get_lfr_vse_dis_value(void);
unsigned int mtk_dbg_get_lfr_skip_num_value(void);
unsigned int mtk_dbg_get_lfr_dbg_value(void);
int hrt_lp_switch_get(void);
#endif

#endif

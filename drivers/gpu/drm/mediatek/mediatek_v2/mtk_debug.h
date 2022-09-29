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
	unsigned int gain[3], unsigned int bl, void *param);

extern unsigned int m_new_pq_persist_property[32];
extern unsigned int g_gamma_data_mode;
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

struct cb_data_store {
	struct cmdq_cb_data data;
	struct list_head link;
};
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
void mtk_wakeup_pf_wq(void);
void mtk_drm_cwb_backup_copy_size(void);
int mtk_dprec_mmp_dump_ovl_layer(struct mtk_plane_state *plane_state);
int mtk_dprec_mmp_dump_wdma_layer(struct drm_crtc *crtc,
	struct drm_framebuffer *wb_fb);
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
int mtk_drm_add_cb_data(struct cb_data_store *cb_data, unsigned int crtc_id);
struct cb_data_store *mtk_drm_get_cb_data(unsigned int crtc_id);
void mtk_drm_del_cb_data(struct cmdq_cb_data data, unsigned int crtc_id);
int hrt_lp_switch_get(void);
void debug_dsi(struct drm_crtc *crtc, unsigned int offset, unsigned int mask);
#endif

enum mtk_drm_mml_dbg {
	DISP_MML_DBG_LOG = 0x0001,
	DISP_MML_MMCLK_UNLIMIT = 0x0002,
	DISP_MML_IR_CLEAR = 0x0004,
	MMP_ADDON_CONNECT = 0x1000,
	MMP_ADDON_DISCONNECT = 0x2000,
	MMP_MML_SUBMIT = 0x4000,
	MMP_MML_IDLE = 0x8000,
};

#if IS_ENABLED(CONFIG_MTK_DISP_DEBUG)
struct reg_dbg {
	uint32_t addr;
	uint32_t val;
	uint32_t mask;
};

struct wr_online_dbg {
	struct reg_dbg reg[64];
	uint32_t index;
	uint32_t after_commit;
};

extern struct wr_online_dbg g_wr_reg;
#endif

#endif

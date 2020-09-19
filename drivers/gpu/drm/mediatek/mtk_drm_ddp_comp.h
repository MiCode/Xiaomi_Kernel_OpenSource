/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_DRM_DDP_COMP_H
#define MTK_DRM_DDP_COMP_H

#include <linux/io.h>
#include <linux/kernel.h>
#include "mtk_log.h"
#include "mtk_rect.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_ddp_addon.h"

struct device;
struct device_node;
struct drm_crtc;
struct drm_device;
struct mtk_plane_state;
struct drm_crtc_state;
struct mm_qos_request;

#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))

enum mtk_ddp_comp_type {
	MTK_DISP_OVL,
	MTK_DISP_RDMA,
	MTK_DISP_WDMA,
	MTK_DISP_COLOR,
	MTK_DISP_DITHER,
	MTK_DISP_CCORR,
	MTK_DISP_AAL,
	MTK_DISP_GAMMA,
	MTK_DISP_UFOE,
	MTK_DSI,
	MTK_DPI,
	MTK_DISP_PWM,
	MTK_DISP_MUTEX,
	MTK_DISP_OD,
	MTK_DISP_BLS,
	MTK_DISP_RSZ,
	MTK_DISP_POSTMASK,
	MTK_DMDP_RDMA,
	MTK_DMDP_HDR,
	MTK_DMDP_AAL,
	MTK_DMDP_RSZ,
	MTK_DMDP_TDSHP,
	MTK_DISP_DSC,
	MTK_DP_INTF,
	MTK_DISP_MERGE,
	MTK_DISP_DPTX,
	MTK_DISP_VIRTUAL,
	MTK_DDP_COMP_TYPE_MAX,
};

#define DECLARE_DDP_COMP(EXPR)                                                 \
	EXPR(DDP_COMPONENT_AAL0)                                            \
	EXPR(DDP_COMPONENT_AAL1)                                            \
	EXPR(DDP_COMPONENT_BLS)                                             \
	EXPR(DDP_COMPONENT_CCORR0)                                          \
	EXPR(DDP_COMPONENT_CCORR1)                                          \
	EXPR(DDP_COMPONENT_COLOR0)                                          \
	EXPR(DDP_COMPONENT_COLOR1)                                          \
	EXPR(DDP_COMPONENT_COLOR2)                                          \
	EXPR(DDP_COMPONENT_DITHER0)                                         \
	EXPR(DDP_COMPONENT_DITHER1)                                         \
	EXPR(DDP_COMPONENT_DPI0)                                            \
	EXPR(DDP_COMPONENT_DPI1)                                            \
	EXPR(DDP_COMPONENT_DSI0)                                            \
	EXPR(DDP_COMPONENT_DSI1)                                            \
	EXPR(DDP_COMPONENT_GAMMA0)                                          \
	EXPR(DDP_COMPONENT_GAMMA1)                                          \
	EXPR(DDP_COMPONENT_OD)                                              \
	EXPR(DDP_COMPONENT_OD1)                                             \
	EXPR(DDP_COMPONENT_OVL0)                                            \
	EXPR(DDP_COMPONENT_OVL1)                                            \
	EXPR(DDP_COMPONENT_OVL2)                                            \
	EXPR(DDP_COMPONENT_OVL0_2L)                                         \
	EXPR(DDP_COMPONENT_OVL1_2L)                                         \
	EXPR(DDP_COMPONENT_OVL2_2L)                                         \
	EXPR(DDP_COMPONENT_OVL3_2L)                                         \
	EXPR(DDP_COMPONENT_OVL0_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL1_2L_VIRTUAL0)                                \
	EXPR(DDP_COMPONENT_OVL0_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_OVL1_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_OVL0_OVL0_2L_VIRTUAL0)                           \
	EXPR(DDP_COMPONENT_PWM0)                                            \
	EXPR(DDP_COMPONENT_PWM1)                                            \
	EXPR(DDP_COMPONENT_PWM2)                                            \
	EXPR(DDP_COMPONENT_RDMA0)                                           \
	EXPR(DDP_COMPONENT_RDMA1)                                           \
	EXPR(DDP_COMPONENT_RDMA2)                                           \
	EXPR(DDP_COMPONENT_RDMA3)                                           \
	EXPR(DDP_COMPONENT_RDMA4)                                           \
	EXPR(DDP_COMPONENT_RDMA5)                                           \
	EXPR(DDP_COMPONENT_RDMA0_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA1_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA2_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RSZ0)                                            \
	EXPR(DDP_COMPONENT_RSZ1)                                            \
	EXPR(DDP_COMPONENT_UFOE)                                            \
	EXPR(DDP_COMPONENT_WDMA0)                                           \
	EXPR(DDP_COMPONENT_WDMA1)                                           \
	EXPR(DDP_COMPONENT_UFBC_WDMA0)                                      \
	EXPR(DDP_COMPONENT_WDMA_VIRTUAL0)                                   \
	EXPR(DDP_COMPONENT_WDMA_VIRTUAL1)                                   \
	EXPR(DDP_COMPONENT_POSTMASK0)                                       \
	EXPR(DDP_COMPONENT_POSTMASK1)                                       \
	EXPR(DDP_COMPONENT_DMDP_RDMA0)                                      \
	EXPR(DDP_COMPONENT_DMDP_HDR0)                                       \
	EXPR(DDP_COMPONENT_DMDP_AAL0)                                       \
	EXPR(DDP_COMPONENT_DMDP_RSZ0)                                       \
	EXPR(DDP_COMPONENT_DMDP_TDSHP0)                                     \
	EXPR(DDP_COMPONENT_DSC0)                                            \
	EXPR(DDP_COMPONENT_MERGE0)                                          \
	EXPR(DDP_COMPONENT_DPTX)                                            \
	EXPR(DDP_COMPONENT_DP_INTF0)                                        \
	EXPR(DDP_COMPONENT_RDMA4_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_RDMA5_VIRTUAL0)                                  \
	EXPR(DDP_COMPONENT_MERGE1)                                          \
	EXPR(DDP_COMPONENT_SPR0_VIRTUAL)                                    \
	EXPR(DDP_COMPONENT_CM0)                                          \
	EXPR(DDP_COMPONENT_SPR0)                                          \
	EXPR(DDP_COMPONENT_ID_MAX)

#define DECLARE_NUM(ENUM) ENUM,
#define DECLARE_STR(STR) #STR,

enum mtk_ddp_comp_id { DECLARE_DDP_COMP(DECLARE_NUM) };

#if 0 /* Origin enum define */
enum mtk_ddp_comp_id {
	DDP_COMPONENT_AAL0,
	DDP_COMPONENT_AAL1,
	DDP_COMPONENT_BLS,
	DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_COLOR2,
	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DPI0,
	DDP_COMPONENT_DPI1,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_DSI1,
	DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_OD,
	DDP_COMPONENT_OD1,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL1,
	DDP_COMPONENT_OVL2,
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_PWM0,
	DDP_COMPONENT_PWM1,
	DDP_COMPONENT_PWM2,
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_UFOE,
	DDP_COMPONENT_WDMA0,
	DDP_COMPONENT_WDMA1,
	DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_ID_MAX,
};
#endif

struct mtk_ddp_comp;
struct cmdq_pkt;
enum mtk_ddp_comp_trigger_flag {
	MTK_TRIG_FLAG_TRIGGER,
	MTK_TRIG_FLAG_EOF,
	MTK_TRIG_FLAG_LAYER_REC,
};

enum mtk_ddp_io_cmd {
	REQ_PANEL_EXT,
	MTK_IO_CMD_RDMA_GOLDEN_SETTING,
	MTK_IO_CMD_OVL_GOLDEN_SETTING,
	DSI_START_VDO_MODE,
	DSI_STOP_VDO_MODE,
	ESD_CHECK_READ,
	ESD_CHECK_CMP,
	REQ_ESD_EINT_COMPAT,
	COMP_REG_START,
	CONNECTOR_ENABLE,
	CONNECTOR_DISABLE,
	CONNECTOR_RESET,
	CONNECTOR_READ_EPILOG,
	CONNECTOR_IS_ENABLE,
	CONNECTOR_PANEL_ENABLE,
	CONNECTOR_PANEL_DISABLE,
	OVL_ALL_LAYER_OFF,
	IRQ_LEVEL_ALL,
	IRQ_LEVEL_IDLE,
	DSI_VFP_IDLE_MODE,
	DSI_VFP_DEFAULT_MODE,
	DSI_GET_TIMING,
	DSI_GET_MODE_BY_MAX_VREFRESH,
	PMQOS_SET_BW,
	PMQOS_SET_HRT_BW,
	PMQOS_UPDATE_BW,
	OVL_REPLACE_BOOTUP_MVA,
	BACKUP_INFO_CMP,
	LCM_RESET,
	DSI_SET_BL,
	DSI_SET_BL_AOD,
	DSI_SET_BL_GRP,
	DSI_HBM_SET,
	DSI_HBM_GET_STATE,
	DSI_HBM_GET_WAIT_STATE,
	DSI_HBM_SET_WAIT_STATE,
	DSI_HBM_WAIT,
	LCM_ATA_CHECK,
	DSI_SET_CRTC_AVAIL_MODES,
	DSI_TIMING_CHANGE,
	GET_PANEL_NAME,
	DSI_CHANGE_MODE,
	BACKUP_OVL_STATUS,
	MIPI_HOPPING,
	PANEL_OSC_HOPPING,
	DYN_FPS_INDEX,
	SET_MMCLK_BY_DATARATE,
	GET_FRAME_HRT_BW_BY_DATARATE,
	DSI_SEND_DDIC_CMD,
	DSI_READ_DDIC_CMD,
	DSI_GET_VIRTUAL_HEIGH,
	DSI_GET_VIRTUAL_WIDTH,
	FRAME_DIRTY,
	DSI_LFR_SET,
	DSI_LFR_UPDATE,
	DSI_LFR_STATUS_CHECK,
};

struct golden_setting_context {
	unsigned int is_vdo_mode;
	unsigned int is_dc;
	unsigned int dst_width;
	unsigned int dst_height;
	// add for rdma default goden setting
	unsigned int vrefresh;
};

struct mtk_ddp_config {
	void *pa;
	unsigned int w;
	unsigned int h;
	unsigned int x;
	unsigned int y;
	unsigned int vrefresh;
	unsigned int bpc;
	struct golden_setting_context *p_golden_setting_context;
};

struct mtk_ddp_fb_info {
	unsigned int fb_pa;
	unsigned int fb_mva;
	unsigned int fb_size;
};

struct mtk_ddp_comp_funcs {
	void (*config)(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg,
		       struct cmdq_pkt *handle);
	void (*prepare)(struct mtk_ddp_comp *comp);
	void (*unprepare)(struct mtk_ddp_comp *comp);
	void (*start)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*stop)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*enable_vblank)(struct mtk_ddp_comp *comp, struct drm_crtc *crtc,
			      struct cmdq_pkt *handle);
	void (*disable_vblank)(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle);
	void (*layer_on)(struct mtk_ddp_comp *comp, unsigned int idx,
			 unsigned int ext_idx, struct cmdq_pkt *handle);
	void (*layer_off)(struct mtk_ddp_comp *comp, unsigned int idx,
			  unsigned int ext_idx, struct cmdq_pkt *handle);
	void (*layer_config)(struct mtk_ddp_comp *comp, unsigned int idx,
			     struct mtk_plane_state *state,
			     struct cmdq_pkt *handle);
	void (*gamma_set)(struct mtk_ddp_comp *comp,
			  struct drm_crtc_state *state,
			  struct cmdq_pkt *handle);
	void (*first_cfg)(struct mtk_ddp_comp *comp,
		       struct mtk_ddp_config *cfg, struct cmdq_pkt *handle);
	void (*bypass)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle);
	void (*config_trigger)(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle,
			       enum mtk_ddp_comp_trigger_flag trig_flag);
	void (*addon_config)(struct mtk_ddp_comp *comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *handle);
	int (*io_cmd)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      enum mtk_ddp_io_cmd cmd, void *params);
	int (*user_cmd)(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		      unsigned int cmd, void *params);
	void (*connect)(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			enum mtk_ddp_comp_id next);
	int (*is_busy)(struct mtk_ddp_comp *comp);
};

struct mtk_ddp_comp {
	struct clk *clk;
	void __iomem *regs;
	resource_size_t regs_pa;
	int irq;
	struct device *larb_dev;
	struct device *dev;
	struct mtk_drm_crtc *mtk_crtc;
	u32 larb_id;
	enum mtk_ddp_comp_id id;
	struct drm_framebuffer *fb;
	const struct mtk_ddp_comp_funcs *funcs;
	void *comp_mode;
	struct cmdq_base *cmdq_base;
#if 0
	u8 cmdq_subsys;
#endif
	unsigned int qos_attr;
	struct mm_qos_request qos_req;
	struct mm_qos_request fbdc_qos_req;
	struct mm_qos_request hrt_qos_req;
	bool blank_mode;
	u32 qos_bw;
	u32 fbdc_bw;
	u32 hrt_bw;
};

static inline void mtk_ddp_comp_config(struct mtk_ddp_comp *comp,
				       struct mtk_ddp_config *cfg,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->config && !comp->blank_mode)
		comp->funcs->config(comp, cfg, handle);
}

static inline void mtk_ddp_comp_prepare(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->prepare && !comp->blank_mode)
		comp->funcs->prepare(comp);
}

static inline void mtk_ddp_comp_unprepare(struct mtk_ddp_comp *comp)
{
	if (comp && comp->funcs && comp->funcs->unprepare && !comp->blank_mode)
		comp->funcs->unprepare(comp);
}

static inline void mtk_ddp_comp_start(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->start && !comp->blank_mode)
		comp->funcs->start(comp, handle);
}

static inline void mtk_ddp_comp_stop(struct mtk_ddp_comp *comp,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->stop && !comp->blank_mode)
		comp->funcs->stop(comp, handle);
}

static inline void mtk_ddp_comp_enable_vblank(struct mtk_ddp_comp *comp,
					      struct drm_crtc *crtc,
					      struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->enable_vblank &&
			!comp->blank_mode)
		comp->funcs->enable_vblank(comp, crtc, handle);
}

static inline void mtk_ddp_comp_disable_vblank(struct mtk_ddp_comp *comp,
					       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->disable_vblank &&
			!comp->blank_mode)
		comp->funcs->disable_vblank(comp, handle);
}

static inline void mtk_ddp_comp_layer_on(struct mtk_ddp_comp *comp,
					 unsigned int idx, unsigned int ext_idx,
					 struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_on && !comp->blank_mode)
		comp->funcs->layer_on(comp, idx, ext_idx, handle);
}

static inline void mtk_ddp_comp_layer_off(struct mtk_ddp_comp *comp,
					  unsigned int idx,
					  unsigned int ext_idx,
					  struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_off && !comp->blank_mode)
		comp->funcs->layer_off(comp, idx, ext_idx, handle);
}

static inline void mtk_ddp_comp_layer_config(struct mtk_ddp_comp *comp,
					     unsigned int idx,
					     struct mtk_plane_state *state,
					     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->layer_config &&
			!comp->blank_mode) {
		DDPDBG("[DRM]func:%s, line:%d ==>\n",
			__func__, __LINE__);
		DDPDBG("comp_funcs:0x%p, layer_config:0x%p\n",
			comp->funcs, comp->funcs->layer_config);

		comp->funcs->layer_config(comp, idx, state, handle);
	}
}

static inline void mtk_ddp_gamma_set(struct mtk_ddp_comp *comp,
				     struct drm_crtc_state *state,
				     struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->gamma_set && !comp->blank_mode)
		comp->funcs->gamma_set(comp, state, handle);
}

static inline void mtk_ddp_comp_bypass(struct mtk_ddp_comp *comp,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->bypass && !comp->blank_mode)
		comp->funcs->bypass(comp, handle);
}

static inline void mtk_ddp_comp_first_cfg(struct mtk_ddp_comp *comp,
				       struct mtk_ddp_config *cfg,
				       struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->first_cfg && !comp->blank_mode)
		comp->funcs->first_cfg(comp, cfg, handle);
}

static inline void
mtk_ddp_comp_config_trigger(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			    enum mtk_ddp_comp_trigger_flag flag)
{
	if (comp && comp->funcs && comp->funcs->config_trigger &&
			!comp->blank_mode)
		comp->funcs->config_trigger(comp, handle, flag);
}

static inline void
mtk_ddp_comp_addon_config(struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id prev,
			  enum mtk_ddp_comp_id next,
			  union mtk_addon_config *addon_config,
			  struct cmdq_pkt *handle)
{
	if (comp && comp->funcs && comp->funcs->addon_config &&
			!comp->blank_mode)
		comp->funcs->addon_config(comp, prev, next, addon_config,
				handle);
}

static inline int mtk_ddp_comp_io_cmd(struct mtk_ddp_comp *comp,
				      struct cmdq_pkt *handle,
				      enum mtk_ddp_io_cmd io_cmd, void *params)
{
	int ret = -EINVAL;

	if (comp && comp->funcs && comp->funcs->io_cmd && !comp->blank_mode)
		ret = comp->funcs->io_cmd(comp, handle, io_cmd, params);

	return ret;
}

static inline int
mtk_ddp_comp_is_busy(struct mtk_ddp_comp *comp)
{
	int ret = 0;

	if (comp && comp->funcs && comp->funcs->is_busy && !comp->blank_mode)
		ret = comp->funcs->is_busy(comp);

	return ret;
}

static inline void mtk_ddp_cpu_mask_write(struct mtk_ddp_comp *comp,
					  unsigned int off, unsigned int val,
					  unsigned int mask)
{
	unsigned int v = (readl(comp->regs + off) & (~mask));

	v += (val & mask);
	writel_relaxed(v, comp->regs + off);
}

enum mtk_ddp_comp_id mtk_ddp_comp_get_id(struct device_node *node,
					 enum mtk_ddp_comp_type comp_type);
struct mtk_ddp_comp *mtk_ddp_comp_find_by_id(struct drm_crtc *crtc,
					     enum mtk_ddp_comp_id comp_id);
unsigned int mtk_drm_find_possible_crtc_by_comp(struct drm_device *drm,
						struct mtk_ddp_comp ddp_comp);
int mtk_ddp_comp_init(struct device *dev, struct device_node *comp_node,
		      struct mtk_ddp_comp *comp, enum mtk_ddp_comp_id comp_id,
		      const struct mtk_ddp_comp_funcs *funcs);
int mtk_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);
void mtk_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);
int mtk_ddp_comp_get_type(enum mtk_ddp_comp_id comp_id);
bool mtk_dsi_is_cmd_mode(struct mtk_ddp_comp *comp);
bool mtk_ddp_comp_is_output(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_get_name(struct mtk_ddp_comp *comp, char *buf, int buf_len);
int mtk_ovl_layer_num(struct mtk_ddp_comp *comp);
void mtk_ddp_write(struct mtk_ddp_comp *comp, unsigned int value,
		   unsigned int offset, void *handle);
void mtk_ddp_write_relaxed(struct mtk_ddp_comp *comp, unsigned int value,
			   unsigned int offset, void *handle);
void mtk_ddp_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
			unsigned int offset, unsigned int mask, void *handle);
void mtk_ddp_write_mask_cpu(struct mtk_ddp_comp *comp,
			unsigned int value, unsigned int offset,
			unsigned int mask);
void mtk_ddp_comp_clk_prepare(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_clk_unprepare(struct mtk_ddp_comp *comp);
void mtk_ddp_comp_iommu_enable(struct mtk_ddp_comp *comp,
			       struct cmdq_pkt *handle);
void mt6779_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6885_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6873_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6853_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);
void mt6833_mtk_sodi_config(struct drm_device *drm, enum mtk_ddp_comp_id id,
			    struct cmdq_pkt *handle, void *data);


int mtk_ddp_comp_helper_get_opt(struct mtk_ddp_comp *comp,
				enum MTK_DRM_HELPER_OPT option);
#endif /* MTK_DRM_DDP_COMP_H */

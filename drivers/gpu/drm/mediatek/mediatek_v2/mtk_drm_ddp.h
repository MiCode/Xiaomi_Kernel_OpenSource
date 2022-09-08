/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_DDP_H
#define MTK_DRM_DDP_H

#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

#define DISP_MUTEX_TOTAL (16)
#define DISP_MUTEX_DDP_FIRST (0)
#define DISP_MUTEX_DDP_LAST (5)
#define DISP_MUTEX_DDP_COUNT (DISP_MUTEX_DDP_LAST - DISP_MUTEX_DDP_FIRST + 1)
#define __DISP_MUTEX_INT_MSK ((1 << (DISP_MUTEX_DDP_COUNT)) - 1)
#define DISP_MUTEX_INT_MSK                                                     \
	((__DISP_MUTEX_INT_MSK << DISP_MUTEX_TOTAL) | __DISP_MUTEX_INT_MSK)

/* CHIST path select*/
#define DISP_CHIST0_FROM_RDMA0_POS 0
#define DISP_CHIST1_FROM_RDMA0_POS 1
#define DISP_CHIST0_FROM_POSTMASK0 2
#define DISP_CHIST1_FROM_POSTMASK0 3
#define DISP_CHIST0_FROM_DITHER0   4
#define DISP_CHIST1_FROM_DITHER0   5

#define MUTEX_SOF_SINGLE_MODE 0
#define MUTEX_SOF_DSI0 1
#define MUTEX_SOF_DSI1 2
#define MUTEX_SOF_DPI0 3
#define MUTEX_SOF_DPI1 4

#define DISP_REG_MUTEX_EN(n) (0x20 + 0x20 * (n))
#define DISP_REG_MUTEX(n) (0x24 + 0x20 * (n))
#define DISP_REG_MUTEX_RST(n) (0x28 + 0x20 * (n))
#define DISP_REG_MUTEX_SOF(data, n) (data->mutex_sof_reg + 0x20 * (n))
#define DISP_REG_MUTEX_MOD(data, n) (data->mutex_mod_reg + 0x20 * (n))
#define DISP_REG_MUTEX_MOD2(n) (0x34 + 0x20 * (n))

#define SOF_FLD_MUTEX0_SOF REG_FLD(3, 0)
#define SOF_FLD_MUTEX0_SOF_TIMING REG_FLD(2, 3)
#define SOF_FLD_MUTEX0_SOF_WAIT REG_FLD(1, 5)
#define SOF_FLD_MUTEX0_EOF REG_FLD(3, 6)
#define SOF_FLD_MUTEX0_FOF_TIMING REG_FLD(2, 9)
#define SOF_FLD_MUTEX0_EOF_WAIT REG_FLD(1, 11)

enum mtk_ddp_mutex_sof_id {
	DDP_MUTEX_SOF_SINGLE_MODE,
	DDP_MUTEX_SOF_DSI0,
	DDP_MUTEX_SOF_DSI1,
	DDP_MUTEX_SOF_DPI0,
	DDP_MUTEX_SOF_DPI1,
	DDP_MUTEX_SOF_DSI2,
	DDP_MUTEX_SOF_DSI3,
	DDP_MUTEX_SOF_MAX,
};

struct regmap;
struct device;
struct mtk_disp_mutex;
struct mtk_mmsys_reg_data;

struct dummy_mapping {
	resource_size_t pa_addr;
	void __iomem *addr;
	enum mtk_ddp_comp_id comp_id;
	unsigned int offset;
};

struct mtk_disp_mutex {
	int id;
	bool claimed;
};

struct mtk_disp_ddp_data {
	const unsigned int *mutex_mod;
	const unsigned int *mutex_sof;
	unsigned int mutex_mod_reg;
	unsigned int mutex_sof_reg;
	const unsigned int *dispsys_map;
};

struct mtk_ddp {
	struct device *dev;
	struct clk *clk;
	void __iomem *regs;
	resource_size_t regs_pa;

	unsigned int dispsys_num;
	struct clk *side_clk;
	void __iomem *side_regs;
	resource_size_t side_regs_pa;
	struct mtk_disp_mutex mutex[10];
	const struct mtk_disp_ddp_data *data;
	struct cmdq_base *cmdq_base;
};

struct mtk_mmsys_reg_data {
	unsigned int ovl0_mout_en;
	unsigned int rdma0_sout_sel_in;
	unsigned int rdma0_sout_color0;
	unsigned int rdma1_sout_sel_in;
	unsigned int rdma1_sout_dpi0;
	unsigned int rdma1_sout_dsi0;
	unsigned int dpi0_sel_in;
	unsigned int dpi0_sel_in_rdma1;
	unsigned int *path_sel;
	unsigned int path_sel_size;
	const unsigned int *dispsys_map;
};

#define MT6983_DUMMY_REG_CNT 61
extern struct dummy_mapping mt6983_dispsys_dummy_register[MT6983_DUMMY_REG_CNT];
extern struct dummy_mapping mt6895_dispsys_dummy_register[MT6983_DUMMY_REG_CNT];

#define MT6879_DUMMY_REG_CNT 53
extern struct dummy_mapping mt6879_dispsys_dummy_register[MT6879_DUMMY_REG_CNT];


const struct mtk_mmsys_reg_data *
mtk_ddp_get_mmsys_reg_data(enum mtk_mmsys_id mmsys_id);

void mtk_disp_ultra_offset(void __iomem *config_regs,
			enum mtk_ddp_comp_id comp, bool is_dc);
void mtk_ddp_add_comp_to_path(struct mtk_drm_crtc *mtk_crtc,
			      struct mtk_ddp_comp *comp,
			      enum mtk_ddp_comp_id prev,
			      enum mtk_ddp_comp_id next);
void mtk_ddp_add_comp_to_path_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					enum mtk_ddp_comp_id cur,
					enum mtk_ddp_comp_id next,
					struct cmdq_pkt *handle);
void mtk_ddp_remove_comp_from_path(struct mtk_drm_crtc *mtk_crtc,
				   enum mtk_ddp_comp_id cur,
				   enum mtk_ddp_comp_id next);
void mtk_ddp_remove_comp_from_path_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					     enum mtk_ddp_comp_id cur,
					     enum mtk_ddp_comp_id next,
					     struct cmdq_pkt *handle);

struct mtk_disp_mutex *mtk_disp_mutex_get(struct device *dev, unsigned int id);
int mtk_disp_mutex_prepare(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_add_comp(struct mtk_disp_mutex *mutex,
			     enum mtk_ddp_comp_id id);
void mtk_disp_mutex_add_comp_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
				       enum mtk_ddp_comp_id id, bool is_cmd_mod,
				       struct cmdq_pkt *handle,
				       unsigned int mutex_id);
void mtk_disp_mutex_enable(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_disable(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_remove_comp(struct mtk_disp_mutex *mutex,
				enum mtk_ddp_comp_id id);
void mtk_disp_mutex_remove_comp_with_cmdq(struct mtk_drm_crtc *mtk_crtc,
					  enum mtk_ddp_comp_id id,
					  struct cmdq_pkt *handle,
					  unsigned int mutex_id);
void mtk_disp_mutex_unprepare(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_put(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_acquire(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_release(struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_trigger(struct mtk_disp_mutex *mutex, void *handle);

void mtk_disp_mutex_enable_cmdq(struct mtk_disp_mutex *mutex,
				struct cmdq_pkt *cmdq_handle,
				struct cmdq_base *cmdq_base);
void mtk_disp_mutex_src_set(struct mtk_drm_crtc *mtk_crtc, bool is_cmd_mode);
void mtk_disp_mutex_inten_enable_cmdq(struct mtk_disp_mutex *mutex,
				      void *handle);
void mtk_disp_mutex_inten_disable_cmdq(struct mtk_disp_mutex *mutex,
				       void *handle);

void mutex_dump_reg_mt6885(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6983(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6895(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6879(struct mtk_disp_mutex *mutex);
void mutex_dump_reg_mt6855(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6885(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6983(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6895(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_reg_mt6885(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6879(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6855(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6983(void __iomem *config_regs);
void mmsys_config_dump_reg_mt6895(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6983(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6895(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6885(void __iomem *config_regs);
void mtk_ddp_insert_dsc_prim_MT6885(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6885(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6983(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6983(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_ext_MT6983(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_ext_MT6983(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6895(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6895(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6879(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6879(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_insert_dsc_prim_MT6855(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6855(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_connect_dual_pipe_path(struct mtk_drm_crtc *mtk_crtc,
	struct mtk_disp_mutex *mutex);
void mtk_disp_mutex_submit_sof(struct mtk_disp_mutex *mutex);
void mtk_ddp_dual_pipe_dump(struct mtk_drm_crtc *mtk_crtc);

void mutex_dump_reg_mt6873(struct mtk_disp_mutex *mutex);
void mutex_dump_analysis_mt6873(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_reg_mt6873(void __iomem *config_regs);
void mmsys_config_dump_analysis_mt6873(void __iomem *config_regs);

void mtk_ddp_insert_dsc_prim_MT6873(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6873(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6853(void __iomem *config_regs);
void mutex_dump_analysis_mt6853(struct mtk_disp_mutex *mutex);

void mtk_ddp_insert_dsc_prim_MT6853(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6853(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);

void mmsys_config_dump_analysis_mt6833(void __iomem *config_regs);
void mutex_dump_analysis_mt6833(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6879(void __iomem *config_regs);
void mutex_dump_analysis_mt6879(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6768(void __iomem *config_regs);
void mutex_dump_analysis_mt6768(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6765(void __iomem *config_regs);
void mutex_dump_analysis_mt6765(struct mtk_disp_mutex *mutex);

void mmsys_config_dump_analysis_mt6855(void __iomem *config_regs);
void mutex_dump_analysis_mt6855(struct mtk_disp_mutex *mutex);

char *mtk_ddp_get_mutex_sof_name(unsigned int regval);
#endif /* MTK_DRM_DDP_H */

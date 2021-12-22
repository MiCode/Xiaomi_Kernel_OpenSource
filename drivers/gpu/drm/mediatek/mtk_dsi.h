/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DSI_H
#define __MTK_DSI_H

#ifdef CONFIG_MTK_MT6382_BDG

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <linux/clk.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/component.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_helper.h"
#include "mtk_mipi_tx.h"
#include "mtk_dump.h"
#include "mtk_log.h"
#include "mtk_drm_lowpower.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_arr.h"
#include "mtk_panel_ext.h"

#define AS_UINT32(x) (*(u32 *)((void *)x))
#define CMD_MODE 0
#define SYNC_PULSE_MODE 1
#define SYNC_EVENT_MODE 2
#define BURST_MODE 3

struct t_condition_wq {
	wait_queue_head_t wq;
	atomic_t condition;
};

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct mipi_dsi_host host;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct drm_panel *panel;
	struct mtk_panel_ext *ext;
	struct cmdq_pkt_buffer cmdq_buf;
	struct drm_bridge *bridge;
	struct phy *phy;
	bool is_slave;
	struct mtk_dsi *slave_dsi;
	struct mtk_dsi *master_dsi;

	void __iomem *regs;

	struct clk *engine_clk;
	struct clk *digital_clk;
	struct clk *hs_clk;

	u32 data_rate;
	u32 bdg_data_rate;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int clk_refcnt;
	bool output_en;
	bool doze_enabled;
	u32 irq_data;
	wait_queue_head_t irq_wait_queue;
	struct mtk_dsi_driver_data *driver_data;

	struct t_condition_wq enter_ulps_done;
	struct t_condition_wq exit_ulps_done;
	struct t_condition_wq te_rdy;
	struct t_condition_wq frame_done;
	unsigned int hs_trail;
	unsigned int hs_prpr;
	unsigned int hs_zero;
	unsigned int lpx;
	unsigned int ta_get;
	unsigned int ta_sure;
	unsigned int ta_go;
	unsigned int da_hs_exit;
	unsigned int cont_det;
	unsigned int clk_zero;
	unsigned int clk_hs_prpr;
	unsigned int clk_hs_exit;
	unsigned int clk_hs_post;

	unsigned int vsa;
	unsigned int vbp;
	unsigned int vfp;
	unsigned int hsa_byte;
	unsigned int hbp_byte;
	unsigned int hfp_byte;
#ifdef CONFIG_MTK_MT6382_BDG
	/* for 6382 mipi hopping */
	bool bdg_mipi_hopping_sta;
#endif
	bool mipi_hopping_sta;
	bool panel_osc_hopping_sta;
	unsigned int data_phy_cycle;
	/* for Panel Master dcs read/write */
	struct mipi_dsi_device *dev_for_PM;
};

int mtk_dsi_get_virtual_width(struct mtk_dsi *dsi,
	struct drm_crtc *crtc);
int mtk_dsi_get_virtual_heigh(struct mtk_dsi *dsi,
	struct drm_crtc *crtc);
unsigned int mtk_dsi_default_rate(struct mtk_dsi *dsi);
struct mtk_panel_ext *mtk_dsi_get_panel_ext(struct mtk_ddp_comp *comp);
void mtk_dsi_cmdq_gce(struct mtk_dsi *dsi, struct cmdq_pkt *handle,
				const struct mipi_dsi_msg *msg);
int mtk_dsi_start_vdo_mode(struct mtk_ddp_comp *comp, void *handle);
void mtk_disp_mutex_trigger(struct mtk_disp_mutex *mutex, void *handle);
int mtk_dsi_trigger(struct mtk_ddp_comp *comp, void *handle);
void mtk_output_bdg_enable(struct mtk_dsi *dsi, int force_lcm_update);
unsigned int _dsi_get_pcw(unsigned long data_rate,
	unsigned int pcw_ratio);

#endif
#endif

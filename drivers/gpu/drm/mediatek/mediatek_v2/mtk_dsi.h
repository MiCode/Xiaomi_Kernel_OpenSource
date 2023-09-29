/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DSI_H__
#define __MTK_DSI_H__

#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/clk.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>
#include <video/videomode.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_panel_ext.h"
#include "mi_disp/mi_dsi_panel.h"
#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "mtk-cmdq-ext.h"
#endif

struct mtk_dsi_driver_data {
	const u32 reg_cmdq0_ofs;
	const u32 reg_cmdq1_ofs;
	const u32 reg_vm_cmd_con_ofs;
	const u32 reg_vm_cmd_data0_ofs;
	const u32 reg_vm_cmd_data10_ofs;
	const u32 reg_vm_cmd_data20_ofs;
	const u32 reg_vm_cmd_data30_ofs;
	s32 (*poll_for_idle)(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
	char *esd_eint_compat;
	bool support_shadow;
	bool need_bypass_shadow;
	bool need_wait_fifo;
	const u32 buffer_unit;
	const u32 sram_unit;
	const u32 urgent_lo_fifo_us;
	const u32 urgent_hi_fifo_us;
	bool dsi_buffer;
	bool smi_dbg_disable;
	u32 max_vfp;
	void (*mmclk_by_datarate)(struct mtk_dsi *dsi,
		struct mtk_drm_crtc *mtk_crtc, unsigned int en);
};

enum dsi_porch_type;

s32 mtk_dsi_poll_for_idle(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
irqreturn_t mtk_dsi_irq_status(int irq, void *dev_id);
void mtk_dsi_set_mmclk_by_datarate_V1(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en);
void mtk_dsi_set_mmclk_by_datarate_V2(struct mtk_dsi *dsi,
	struct mtk_drm_crtc *mtk_crtc, unsigned int en);
int mtk_dsi_get_virtual_width(struct mtk_dsi *dsi,
	struct drm_crtc *crtc);
int mtk_dsi_get_virtual_heigh(struct mtk_dsi *dsi,
	struct drm_crtc *crtc);
unsigned int mtk_dsi_default_rate(struct mtk_dsi *dsi);
void mtk_output_bdg_enable(struct mtk_dsi *dsi, int force_lcm_update);
int mtk_dsi_porch_setting(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
			  enum dsi_porch_type type, unsigned int value);
void mtk_dsi_porch_config(struct mtk_dsi *dsi, struct cmdq_pkt *handle);
int mtk_dsi_trigger(struct mtk_ddp_comp *comp, void *handle);

#endif

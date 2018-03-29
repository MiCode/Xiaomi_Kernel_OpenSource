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

#ifndef _MTK_DSI_H_
#define _MTK_DSI_H_

#include <drm/drm_crtc.h>

#include "mtk_drm_ddp_comp.h"

struct phy;

struct mtk_dsi {
	struct mtk_ddp_comp ddp_comp;
	struct device *dev;
	struct drm_encoder encoder;
	struct drm_connector conn;
	struct device_node *panel_node, *device_node;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct phy *phy;

	struct regmap *mmsys_sw_rst_b;
	u32 sw_rst_b;

	struct mipi_dsi_host host;

	void __iomem *regs;

	struct clk *engine_clk;
	struct clk *digital_clk;

	u32 data_rate;
	int ssc_data;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
	struct videomode vm;
	int refcount;
	bool enabled, poweron;
	int irq_num, irq_data;

#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs;
#endif
};

static inline struct mtk_dsi *host_to_dsi(struct mipi_dsi_host *h)
{
	return container_of(h, struct mtk_dsi, host);
}

static inline struct mtk_dsi *encoder_to_dsi(struct drm_encoder *e)
{
	return container_of(e, struct mtk_dsi, encoder);
}

static inline struct mtk_dsi *connector_to_dsi(struct drm_connector *c)
{
	return container_of(c, struct mtk_dsi, conn);
}

void mtk_dsi_dump_registers(struct mtk_dsi *dsi);

#endif

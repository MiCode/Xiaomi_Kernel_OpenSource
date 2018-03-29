/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
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
#ifndef _MTK_HDMI_CTRL_H
#define _MTK_HDMI_CTRL_H

#include <drm/drm_crtc.h>
#include <drm/mediatek/mtk_hdmi_audio.h>
#include <linux/hdmi.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/types.h>

struct clk;
struct device;
struct device_node;
struct i2c_adapter;
struct platform_device;
struct regmap;

enum mtk_hdmi_clk_id {
	MTK_HDMI_CLK_HDMI_PIXEL,
	MTK_HDMI_CLK_HDMI_PLL,
	MTK_HDMI_CLK_AUD_BCLK,
	MTK_HDMI_CLK_AUD_SPDIF,
	MTK_HDMI_CLK_COUNT
};

struct mtk_hdmi {
	struct drm_bridge bridge;
	struct drm_connector conn;
	struct device *dev;
	struct device *cec_dev;
	struct i2c_adapter *ddc_adpt;
	struct clk *clk[MTK_HDMI_CLK_COUNT];
#if defined(CONFIG_DEBUG_FS)
	struct dentry *debugfs;
#endif
	struct platform_device *audio_pdev;
	struct drm_display_mode mode;
	bool dvi_mode;
	int flt_n_5v_gpio;
	int flt_n_5v_irq;
	u32 min_clock;
	u32 max_clock;
	u32 max_hdisplay;
	u32 max_vdisplay;
	u32 ibias;
	u32 ibias_up;
	struct regmap *sys_regmap;
	unsigned int sys_offset;
	void __iomem *regs;
	bool init;
	enum hdmi_colorspace csp;
	bool audio_enable;
	bool output;
	struct hdmi_audio_param aud_param;
};

static inline struct mtk_hdmi *hdmi_ctx_from_bridge(struct drm_bridge *b)
{
	return container_of(b, struct mtk_hdmi, bridge);
}

static inline struct mtk_hdmi *hdmi_ctx_from_conn(struct drm_connector *c)
{
	return container_of(c, struct mtk_hdmi, conn);
}

int mtk_hdmi_output_init(struct mtk_hdmi *hdmi);
int mtk_hdmi_hpd_high(struct mtk_hdmi *hdmi);
int mtk_hdmi_output_set_display_mode(struct mtk_hdmi *hdmi,
				     struct drm_display_mode *mode);
void mtk_hdmi_power_on(struct mtk_hdmi *hdmi);
void mtk_hdmi_power_off(struct mtk_hdmi *hdmi);
void mtk_hdmi_audio_enable(struct mtk_hdmi *hctx);
void mtk_hdmi_audio_disable(struct mtk_hdmi *hctx);
int mtk_hdmi_audio_set_param(struct mtk_hdmi *hctx,
			     struct hdmi_audio_param *param);
int mtk_hdmi_detect_dvi_monitor(struct mtk_hdmi *hctx);
#if defined(CONFIG_DEBUG_FS)
int mtk_drm_hdmi_debugfs_init(struct mtk_hdmi *hdmi);
void mtk_drm_hdmi_debugfs_exit(struct mtk_hdmi *hdmi);
#else
int mtk_drm_hdmi_debugfs_init(struct mtk_hdmi *hdmi)
{
	return 0;
}

void mtk_drm_hdmi_debugfs_exit(struct mtk_hdmi *hdmi)
{
}
#endif /* CONFIG_DEBUG_FS */

extern struct platform_driver mtk_cec_driver;
extern struct platform_driver mtk_hdmi_ddc_driver;
#endif /* _MTK_HDMI_CTRL_H */

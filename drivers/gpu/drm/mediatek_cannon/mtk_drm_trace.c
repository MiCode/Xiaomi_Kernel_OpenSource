/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mtk_drm_trace.h"
#include <linux/trace_events.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_drv.h"

#ifdef DRM_MMPATH
#include "mmpath.h"
#endif

unsigned long mtk_drm_get_tracing_mark(void)
{
	static unsigned long addr;

	if (unlikely(addr == 0))
		addr = kallsyms_lookup_name("tracing_mark_write");

	return addr;
}

static void drm_print_trace(const char *tag, int value)
{
	preempt_disable();
	event_trace_printk(mtk_drm_get_tracing_mark(), "C|%d|%s|%d\n",
		DRM_TRACE_ID, tag, value);
	preempt_enable();
}

void drm_trace_tag_start(const char *tag)
{
	drm_print_trace(tag, 1);
}

void drm_trace_tag_end(const char *tag)
{
	drm_print_trace(tag, 0);
}

void drm_trace_tag_mark(const char *tag)
{
	preempt_disable();
	event_trace_printk(mtk_drm_get_tracing_mark(), "C|%d|%s|%d\n",
		DRM_TRACE_ID, tag, 1);
	event_trace_printk(mtk_drm_get_tracing_mark(), "C|%d|%s|%d\n",
		DRM_TRACE_ID, tag, 0);
	preempt_enable();
}

void mtk_drm_refresh_tag_start(struct mtk_ddp_comp *ddp_comp)
{
	char tag_name[30] = {'\0'};
	int i, j, crtc_idx, met_mode;
	struct mtk_drm_crtc *mtk_crtc = ddp_comp->mtk_crtc;
	bool b_layer_changed = 0;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv;
	int r;

	if (!mtk_crtc)
		return;
	priv = mtk_crtc->base.dev->dev_private;
	met_mode = mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MET);
	if (!met_mode)
		return;

	crtc_idx = drm_crtc_index(&mtk_crtc->base);
	if (mtk_crtc_is_dc_mode(&mtk_crtc->base)) {
		mtk_ddp_comp_io_cmd(ddp_comp, NULL, BACKUP_INFO_CMP,
				    &b_layer_changed);
	} else {
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
			if (comp->id >= DDP_COMPONENT_OVL0 &&
			    comp->id <= DDP_COMPONENT_OVL1_2L) {
				mtk_ddp_comp_io_cmd(comp, NULL, BACKUP_INFO_CMP,
						    &b_layer_changed);
			}
		}
	}

	if (b_layer_changed) {
		r = sprintf(tag_name,
			crtc_idx ? "ExtDispRefresh" : "PrimDispRefresh");
		if (r < 0) {
			/* Handle sprintf() error */
			pr_debug("sprintf error\n");
		}
		preempt_disable();
		event_trace_printk(mtk_drm_get_tracing_mark(), "C|%d|%s|%d\n",
				   DRM_TRACE_FPS_ID, tag_name, 1);
		preempt_enable();
	}
}

void mtk_drm_refresh_tag_end(struct mtk_ddp_comp *ddp_comp)
{
	char tag_name[30] = {'\0'};
	int crtc_idx, met_mode;
	struct mtk_drm_crtc *mtk_crtc = ddp_comp->mtk_crtc;
	struct mtk_drm_private *priv;
	int r;

	if (!mtk_crtc)
		return;
	priv = mtk_crtc->base.dev->dev_private;
	met_mode = mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MET);
	if (!met_mode)
		return;

	crtc_idx = drm_crtc_index(&mtk_crtc->base);
	r = sprintf(tag_name, crtc_idx ? "ExtDispRefresh" : "PrimDispRefresh");
	if (r < 0) {
		/* Handle sprintf() error */
		pr_debug("sprintf error\n");
	}
	preempt_disable();
	event_trace_printk(mtk_drm_get_tracing_mark(), "C|%d|%s|%d\n",
				DRM_TRACE_FPS_ID, tag_name, 0);
	preempt_enable();
}

#ifdef DRM_MMPATH
int get_HWC_gpid(struct mtk_ddp_comp *ddp_comp)
{
	struct drm_crtc *crtc = &ddp_comp->mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	return priv->HWC_gpid;
}

void MMPathTraceOVL2DSI(struct mtk_ddp_comp *ddp_comp)
{
	char str[1300] = "";
	int strlen = sizeof(str), n = 0;

	n += scnprintf(str + n, strlen - n,
		"hw=DISP_OVL0, pid=%d, ", get_HWC_gpid(ddp_comp));

	n = MMPathTraceCrtcPlanes(&ddp_comp->mtk_crtc->base, str, strlen, n);

	n += scnprintf(str + n, strlen - n, "out=DISP_DSI");

	trace_MMPath(str);
}

void MMPathTraceRDMA2DSI(struct mtk_ddp_comp *ddp_comp)
{
	char str[1300] = "";
	int strlen = sizeof(str), n = 0;

	n += scnprintf(str + n, strlen - n,
		"hw=DISP_RDMA0, pid=%d, ", get_HWC_gpid(ddp_comp));

	n = MMPathTraceRDMA(ddp_comp, str, strlen, n);

	n += scnprintf(str + n, strlen - n, "out=DISP_DSI");

	trace_MMPath(str);
}

void MMPathTraceOVL2WDMA(struct mtk_ddp_comp *ddp_comp)
{
	char str[1300] = "";
	int strlen = sizeof(str), n = 0;

	if (drm_crtc_index(&ddp_comp->mtk_crtc->base) == 0)
		n += scnprintf(str + n, strlen - n,
			"hw=DISP_OVL0, pid=%d, ", get_HWC_gpid(ddp_comp));
	else
		n += scnprintf(str + n, strlen - n,
			"hw=DISP_OVL1, pid=%d, ", get_HWC_gpid(ddp_comp));

	n = MMPathTraceCrtcPlanes(&ddp_comp->mtk_crtc->base, str, strlen, n);

	n = MMPathTraceWDMA(ddp_comp, str, strlen, n);

	trace_MMPath(str);
}

void MMPathTraceDRM(struct mtk_ddp_comp *ddp_comp)
{
	struct drm_crtc *crtc = &ddp_comp->mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MMPATH) == 0)
		return;

	if (ddp_comp->id == DDP_COMPONENT_RDMA0) {
		if (mtk_crtc_is_dc_mode(crtc) == 0)
			MMPathTraceOVL2DSI(ddp_comp);
		else
			MMPathTraceRDMA2DSI(ddp_comp);
	} else if (ddp_comp->id == DDP_COMPONENT_WDMA0)
		MMPathTraceOVL2WDMA(ddp_comp);
}
#else
void MMPathTraceDRM(struct mtk_ddp_comp *ddp_comp)
{
}
#endif

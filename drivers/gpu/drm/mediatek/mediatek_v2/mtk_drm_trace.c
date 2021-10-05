// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
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
//#include "mmpath.h"
#endif

#define MTK_DRM_TRACE_MSG_LEN	1024

static noinline int tracing_mark_write(const char *buf)
{
#ifdef CONFIG_TRACING
	trace_puts(buf);
#endif

	return 0;
}

void mtk_drm_print_trace(char *fmt, ...)
{
	char buf[MTK_DRM_TRACE_MSG_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len >= MTK_DRM_TRACE_MSG_LEN) {
		DDPPR_ERR("%s, string size %u exceed limit\n", __func__, len);
		return;
	}

	tracing_mark_write(buf);
}

void drm_trace_tag_start(const char *tag)
{
	mtk_drm_print_trace("C|%d|%s|%d\n", DRM_TRACE_ID, tag, 1);
}

void drm_trace_tag_end(const char *tag)
{
	mtk_drm_print_trace("C|%d|%s|%d\n", DRM_TRACE_ID, tag, 0);
}

void drm_trace_tag_mark(const char *tag)
{
	mtk_drm_print_trace("C|%d|%s|%d\n", DRM_TRACE_ID, tag, 1);
	mtk_drm_print_trace("C|%d|%s|%d\n", DRM_TRACE_ID, tag, 0);
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
		mtk_drm_print_trace("C|%d|%s|%d\n", DRM_TRACE_FPS_ID,
					tag_name, 1);
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
	mtk_drm_print_trace("C|%d|%s|%d\n", DRM_TRACE_FPS_ID, tag_name, 0);
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

	//trace_MMPath(str);
}

void MMPathTraceRDMA2DSI(struct mtk_ddp_comp *ddp_comp)
{
	char str[1300] = "";
	int strlen = sizeof(str), n = 0;

	n += scnprintf(str + n, strlen - n,
		"hw=DISP_RDMA0, pid=%d, ", get_HWC_gpid(ddp_comp));

	n = MMPathTraceRDMA(ddp_comp, str, strlen, n);

	n += scnprintf(str + n, strlen - n, "out=DISP_DSI");

	//trace_MMPath(str);
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

	//trace_MMPath(str);
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

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	int crtc_idx;
	struct mtk_drm_crtc *mtk_crtc = ddp_comp->mtk_crtc;

	if (!mtk_crtc)
		return;

	crtc_idx = drm_crtc_index(&mtk_crtc->base);

	mtk_drm_trace_c("%d|DISP:CRTC-%d-Refresh|%d",
		hwc_pid, crtc_idx, 1);
}

void mtk_drm_refresh_tag_end(struct mtk_ddp_comp *ddp_comp)
{
	int crtc_idx;
	struct mtk_drm_crtc *mtk_crtc = ddp_comp->mtk_crtc;

	if (!mtk_crtc)
		return;

	crtc_idx = drm_crtc_index(&mtk_crtc->base);

	mtk_drm_trace_c("%d|DISP:CRTC-%d-Refresh|%d",
		hwc_pid, crtc_idx, 0);
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

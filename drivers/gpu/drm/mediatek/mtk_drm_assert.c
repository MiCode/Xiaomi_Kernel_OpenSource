/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>
#include <drm/mediatek_drm.h>
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"

#include "mtk_drm_assert.h"
#include "mtk_drm_fbconsole.h"
#include "mtk_drm_mmp.h"

#define RGB888_To_RGB565(x)                                                    \
	((((x)&0xF80000) >> 8) | (((x)&0x00FC00) >> 5) | (((x)&0x0000F8) >> 3))

#define MAKE_TWO_RGB565_COLOR(high, low) (((low) << 16) | (high))

#define DAL_STR_BUF_LEN (1024)
#define DAL_BPP (2)

#define MTK_FB_ALIGNMENT 32

static DEFINE_SEMAPHORE(dal_sem);

static inline int DAL_LOCK(void)
{
	if (down_interruptible(&dal_sem)) {
		DDPPR_ERR("Can't get semaphore in %s\n", __func__);
		return -1;
	}
	return 0;
}

#define DAL_UNLOCK() up(&dal_sem)

struct mtk_drm_gem_obj *dal_buff;
static void *mfc_handle;
char drm_dal_str_buf[DAL_STR_BUF_LEN];
/* DAL layer only surve CRTC0 only */
static int drm_dal_enable;
static struct drm_crtc *dal_crtc;
static void *dal_va;
static u32 dal_pa;

static u32 DAL_GetLayerSize(void)
{
	static unsigned int size;

	if (!size) {
		struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)mfc_handle;

		if (ctxt)
			size = ctxt->fb_width * ctxt->fb_height * DAL_BPP;
		else
			DDPPR_ERR("%s MFC_CONTEXT is NULL\n", __func__);
	}

	return size;
}

int mtk_drm_dal_setscreencolor(enum DAL_COLOR color)
{
	u32 i, size, bg_color, offset;
	struct MFC_CONTEXT *ctxt;
	u32 *addr;

	color = RGB888_To_RGB565(color);
	bg_color = MAKE_TWO_RGB565_COLOR(color, color);

	ctxt = (struct MFC_CONTEXT *)mfc_handle;
	if (!ctxt)
		return -1;
	if (ctxt->screen_color == color)
		return 0;

	offset = MFC_Get_Cursor_Offset(mfc_handle);
	addr = (u32 *)(ctxt->fb_addr + offset);

	size = DAL_GetLayerSize();
	if (!size) {
		DDPPR_ERR("%s wrong fb size\n", __func__);
		return 0;
	}
	size -= offset;

	for (i = 0; i < size / sizeof(u32); ++i)
		*addr++ = bg_color;
	ctxt->screen_color = color;

	return 0;
}

int DAL_SetScreenColor(enum DAL_COLOR color)
{
	uint32_t i;
	uint32_t size;
	uint32_t bg_color;
	struct MFC_CONTEXT *ctxt = NULL;
	uint32_t offset;
	unsigned int *addr;

	if (!mfc_handle)
		return -1;

	color = RGB888_To_RGB565(color);
	bg_color = MAKE_TWO_RGB565_COLOR(color, color);

	ctxt = (struct MFC_CONTEXT *)mfc_handle;
	if (!ctxt)
		return -1;
	if (ctxt->screen_color == color)
		return 0;

	offset = MFC_Get_Cursor_Offset(mfc_handle);
	addr = (unsigned int *)(ctxt->fb_addr + offset);

	size = DAL_GetLayerSize();
	if (!size) {
		DDPPR_ERR("%s wrong fb size\n", __func__);
		return 0;
	}
	size -= offset;

	for (i = 0; i < size / sizeof(uint32_t); ++i)
		*addr++ = bg_color;
	ctxt->screen_color = color;

	return 0;
}
EXPORT_SYMBOL(DAL_SetScreenColor);

int DAL_SetColor(unsigned int fgColor, unsigned int bgColor)
{
	if (!mfc_handle)
		return -1;

	DAL_LOCK();
	MFC_SetColor(mfc_handle, RGB888_To_RGB565(fgColor),
		     RGB888_To_RGB565(bgColor));
	DAL_UNLOCK();

	return 0;
}
EXPORT_SYMBOL(DAL_SetColor);

static struct mtk_ddp_comp *_handle_phy_top_plane(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j, type;
	int lay_num;
	struct drm_plane *plane;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_comp_state comp_state;
	struct mtk_ddp_comp *ovl_comp = NULL;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		type = mtk_ddp_comp_get_type(comp->id);
		if (type == MTK_DISP_OVL)
			ovl_comp = comp;
		else if (type == MTK_DISP_RDMA)
			break;
	}

	if (ovl_comp == NULL) {
		DDPPR_ERR("No proper ovl module in CRTC %s\n",
			  mtk_crtc->base.name);
		return NULL;
	}

	lay_num = mtk_ovl_layer_num(ovl_comp);
	if (lay_num < 0) {
		DDPPR_ERR("invalid layer number:%d\n", lay_num);
		return NULL;
	}

	for (i = 0 ; i < mtk_crtc->layer_nr; ++i) {
		plane = &mtk_crtc->planes[i].base;
		plane_state = to_mtk_plane_state(plane->state);
		comp_state = plane_state->comp_state;

		/*find been cascaded ovl comp */
		if (comp_state.comp_id == ovl_comp->id &&
		    comp_state.lye_id == lay_num - 1) {
			plane_state->pending.enable = false;
			plane_state->pending.dirty = 1;
		}
	}

	return ovl_comp;
}

#ifndef MTK_DRM_FB_LEAK
static void mtk_drm_cmdq_done(struct cmdq_cb_data data)
{
	struct cmdq_pkt *cmdq_handle = data.data;
	cmdq_pkt_destroy(cmdq_handle);
}
#endif

static struct mtk_plane_state *drm_set_dal_plane_state(struct drm_crtc *crtc,
						       bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_plane *plane;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_pending_state *pending;
	struct mtk_ddp_comp *ovl_comp = _handle_phy_top_plane(mtk_crtc);
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)mfc_handle;
	int layer_id = -1;

	if (ovl_comp)
		layer_id = mtk_ovl_layer_num(ovl_comp) - 1;

	if (!ctxt) {
		DDPPR_ERR("%s MFC_CONTEXT is NULL\n", __func__);
		return NULL;
	}
	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return NULL;
	}

	plane = &mtk_crtc->planes[mtk_crtc->layer_nr - 1].base;
	plane_state = to_mtk_plane_state(plane->state);
	pending = &plane_state->pending;

	plane_state->pending.addr = dal_pa;
	plane_state->pending.pitch = ctxt->fb_width * DAL_BPP;
	plane_state->pending.format = DRM_FORMAT_RGB565;
	plane_state->pending.src_x = 0;
	plane_state->pending.src_y = 0;
	plane_state->pending.dst_x = 0;
	plane_state->pending.dst_y = 0;
	plane_state->pending.height = ctxt->fb_height;
	plane_state->pending.width = ctxt->fb_width;
	plane_state->pending.config = 1;
	plane_state->pending.dirty = 1;
	plane_state->pending.enable = !!enable;
	plane_state->pending.is_sec = false;

	pending->prop_val[PLANE_PROP_ALPHA_CON] = 0x1;
	pending->prop_val[PLANE_PROP_PLANE_ALPHA] = 0x80;
	pending->prop_val[PLANE_PROP_COMPRESS] = 0;

	plane_state->comp_state.comp_id = ovl_comp->id;
	plane_state->comp_state.lye_id = layer_id;
	plane_state->comp_state.ext_lye_id = 0;
	plane_state->base.crtc = crtc;

	return plane_state;
}

static void disable_attached_layer(struct drm_crtc *crtc, struct mtk_ddp_comp *ovl_comp,
	int layer_id, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct mtk_drm_private *priv = crtc->dev->dev_private;
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state;
		struct mtk_ddp_comp *comp;
		unsigned int ext_lye_id;

		plane_state = to_mtk_plane_state(plane->state);
		if (i >= OVL_PHY_LAYER_NR && !plane_state->comp_state.comp_id)
			continue;
		comp = priv->ddp_comp[plane_state->comp_state.comp_id];

		if (comp == NULL)
			continue;

		if (comp == ovl_comp && plane_state->comp_state.lye_id &&
				plane_state->comp_state.lye_id == layer_id &&
				plane_state->comp_state.ext_lye_id) {
			DDPINFO("plane %d comp_id %u lye_id %u ext_id %u\n",
				i, plane_state->comp_state.comp_id,
				plane_state->comp_state.lye_id,
				plane_state->comp_state.ext_lye_id);
				ext_lye_id = plane_state->comp_state.ext_lye_id;
		} else {
			continue;
		}

		if (mtk_crtc->is_dual_pipe) {
			struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
			struct mtk_ddp_comp *r_comp;
			unsigned int comp_id = plane_state->comp_state.comp_id;


			r_comp = priv->ddp_comp[dual_pipe_comp_mapping(comp_id)];
			mtk_ddp_comp_layer_off(r_comp, layer_id,
						ext_lye_id, cmdq_handle);
		}
		mtk_ddp_comp_layer_off(comp, layer_id, ext_lye_id, cmdq_handle);
	}
}

int drm_show_dal(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_state *plane_state;
	struct mtk_ddp_comp *ovl_comp = _handle_phy_top_plane(mtk_crtc);
	struct cmdq_pkt *cmdq_handle;
	int layer_id;

	if (ovl_comp == NULL) {
		DDPPR_ERR("%s: can't find ovl comp\n", __func__);
		return 0;
	}
	layer_id = mtk_ovl_layer_num(ovl_comp) - 1;
	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return 0;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	plane_state = drm_set_dal_plane_state(crtc, enable);
	if (!plane_state) {
		DDPPR_ERR("%s: can't set dal plane_state\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	/* set DAL config and trigger display */
	cmdq_handle = mtk_crtc_gce_commit_begin(crtc);

	disable_attached_layer(crtc, ovl_comp, layer_id, cmdq_handle);

	if (mtk_crtc->is_dual_pipe) {
		mtk_crtc_dual_layer_config(mtk_crtc, ovl_comp, layer_id, plane_state, cmdq_handle);
	} else {
		mtk_ddp_comp_layer_config(ovl_comp, layer_id, plane_state, cmdq_handle);
	}

#ifdef MTK_DRM_FB_LEAK
	mtk_crtc_gce_flush(crtc, NULL, cmdq_handle, cmdq_handle);
	cmdq_pkt_wait_complete(cmdq_handle);
	cmdq_pkt_destroy(cmdq_handle);
#else
	mtk_crtc_gce_flush(crtc, mtk_drm_cmdq_done, cmdq_handle, cmdq_handle);
#endif
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
	return 0;
}

void drm_set_dal(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_state *plane_state;
	struct mtk_ddp_comp *ovl_comp = _handle_phy_top_plane(mtk_crtc);
	int layer_id;

	if (ovl_comp == NULL) {
		DDPPR_ERR("%s: can't find ovl comp\n", __func__);
		return;
	}
	layer_id = mtk_ovl_layer_num(ovl_comp) - 1;
	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return;
	}

	plane_state = drm_set_dal_plane_state(crtc, true);
	if (!plane_state) {
		DDPPR_ERR("%s: can't set dal plane_state\n", __func__);
		return;
	}

	disable_attached_layer(crtc, ovl_comp, layer_id, cmdq_handle);

	if (mtk_crtc->is_dual_pipe) {
		mtk_crtc_dual_layer_config(mtk_crtc, ovl_comp, layer_id, plane_state, cmdq_handle);
	} else {
		mtk_ddp_comp_layer_config(ovl_comp, layer_id, plane_state, cmdq_handle);
	}
}

int DAL_Clean(void)
{
	struct MFC_CONTEXT *ctxt = (struct MFC_CONTEXT *)mfc_handle;

	if (!mfc_handle)
		return 0;

	DAL_LOCK();
	if (MFC_ResetCursor(mfc_handle) != MFC_STATUS_OK)
		goto end;

	ctxt->screen_color = 0;
	DAL_SetScreenColor(DAL_COLOR_RED);

	if (drm_dal_enable == 1) {
		drm_dal_enable = 0;
		drm_show_dal(dal_crtc, false);
	}

end:
	DAL_UNLOCK();

	return 0;
}
EXPORT_SYMBOL(DAL_Clean);

int DAL_Printf(const char *fmt, ...)
{
	va_list args;
	u32 i;

	if (!mfc_handle)
		return -1;
	if (!fmt)
		return -1;

	DAL_LOCK();

	va_start(args, fmt);
	i = vsprintf(drm_dal_str_buf, fmt, args);
	va_end(args);

	if (i >= DAL_STR_BUF_LEN) {
		DDPPR_ERR("%s, string size %u exceed limit\n", __func__, i);
		return -1;
	}

	MFC_Print(mfc_handle, drm_dal_str_buf);

	if (drm_dal_enable == 0)
		drm_dal_enable = 1;

#ifdef CONFIG_MTK_DISPLAY_CMDQ
	drm_show_dal(dal_crtc, true);
#endif

	DAL_UNLOCK();

	return 0;
}
EXPORT_SYMBOL(DAL_Printf);

int mtk_drm_dal_enable(void)
{
	return drm_dal_enable;
}

void mtk_drm_assert_fb_init(struct drm_device *dev, u32 width, u32 height)
{
	struct mtk_drm_gem_obj *mtk_gem;
	u32 size = width * height * DAL_BPP;

	mtk_gem = mtk_drm_gem_create(dev, size, true);
	if (IS_ERR(mtk_gem)) {
		DDPINFO("alloc buffer fail\n");
		return;
	}
	//Avoid kmemleak to scan
	kmemleak_ignore(mtk_gem);

	dal_va = mtk_gem->kvaddr;
	dal_pa = mtk_gem->dma_addr;

	MFC_Open(&mfc_handle, mtk_gem->kvaddr, width, height, DAL_BPP,
		 RGB888_To_RGB565(DAL_COLOR_WHITE),
		 RGB888_To_RGB565(DAL_COLOR_RED), mtk_gem->base.filp);
}

int mtk_drm_assert_layer_init(struct drm_crtc *crtc)
{
	if (!dal_pa || !dal_va) {
		DDPPR_ERR("init DAL without proper buffer\n");
		return -1;
	}

	dal_crtc = crtc;
	DAL_SetScreenColor(DAL_COLOR_RED);

	return 0;
}

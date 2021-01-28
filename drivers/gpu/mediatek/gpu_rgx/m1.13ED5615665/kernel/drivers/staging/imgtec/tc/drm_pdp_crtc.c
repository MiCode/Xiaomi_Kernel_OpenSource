/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include "pvr_linux_fence.h"
#include "drm_pdp_drv.h"

#include <linux/version.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>

#include "pvr_dma_resv.h"
#include "drm_pdp_gem.h"

#include "pdp_apollo.h"
#include "pdp_odin.h"
#include "pdp_plato.h"

#include "plato_drv.h"

#if defined(PDP_USE_ATOMIC)
#include <drm/drm_atomic_helper.h>
#endif

#include "kernel_compatibility.h"

enum pdp_crtc_flip_status {
	PDP_CRTC_FLIP_STATUS_NONE = 0,
	PDP_CRTC_FLIP_STATUS_PENDING,
	PDP_CRTC_FLIP_STATUS_DONE,
};

struct pdp_flip_data {
	struct dma_fence_cb base;
	struct drm_crtc *crtc;
	struct dma_fence *wait_fence;
};

/* returns true for ok, false for fail */
static bool pdp_clocks_set(struct drm_crtc *crtc,
				struct drm_display_mode *adjusted_mode)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	bool res;

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN: {
		pdp_odin_set_updates_enabled(crtc->dev->dev,
						pdp_crtc->pdp_reg, false);
		res = pdp_odin_clocks_set(crtc->dev->dev,
				pdp_crtc->pdp_reg, pdp_crtc->pll_reg,
				0,                       /* apollo only */
				pdp_crtc->odn_core_reg,  /* odin only */
				adjusted_mode->hdisplay,
				adjusted_mode->vdisplay);
		pdp_odin_set_updates_enabled(crtc->dev->dev,
					     pdp_crtc->pdp_reg, true);

		break;
	}
	case PDP_VERSION_APOLLO: {
		int clock_in_mhz = adjusted_mode->clock / 1000;

		pdp_apollo_set_updates_enabled(crtc->dev->dev,
					       pdp_crtc->pdp_reg, false);
		res = pdp_apollo_clocks_set(crtc->dev->dev,
				pdp_crtc->pdp_reg, pdp_crtc->pll_reg,
				clock_in_mhz,           /* apollo only */
				NULL,                   /* odin only */
				adjusted_mode->hdisplay,
				adjusted_mode->vdisplay);
		pdp_apollo_set_updates_enabled(crtc->dev->dev,
					       pdp_crtc->pdp_reg, true);

		DRM_DEBUG_DRIVER("pdp clock set to %dMhz\n", clock_in_mhz);

		break;
	}
	case PDP_VERSION_PLATO:
#if defined(SUPPORT_PLATO_DISPLAY)
		plato_enable_pdp_clock(dev_priv->dev->dev->parent);
		res = true;
#else
		DRM_ERROR("Trying to enable plato PDP clock on non-Plato build\n");
		res = false;
#endif
		break;
	default:
		BUG();
	}

	return res;
}

void pdp_crtc_set_plane_enabled(struct drm_crtc *crtc, bool enable)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		pdp_odin_set_plane_enabled(crtc->dev->dev,
					   pdp_crtc->pdp_reg,
					   0, enable);
		break;
	case PDP_VERSION_APOLLO:
		pdp_apollo_set_plane_enabled(crtc->dev->dev,
					     pdp_crtc->pdp_reg,
					     0, enable);
		break;
	case PDP_VERSION_PLATO:
		pdp_plato_set_plane_enabled(crtc->dev->dev,
					    pdp_crtc->pdp_reg,
					    0, enable);
		break;
	default:
		BUG();
	}
}

static void pdp_crtc_set_syncgen_enabled(struct drm_crtc *crtc, bool enable)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		pdp_odin_set_syncgen_enabled(crtc->dev->dev,
					     pdp_crtc->pdp_reg,
					     enable);
		break;
	case PDP_VERSION_APOLLO:
		pdp_apollo_set_syncgen_enabled(crtc->dev->dev,
					       pdp_crtc->pdp_reg,
					       enable);
		break;
	case PDP_VERSION_PLATO:
		pdp_plato_set_syncgen_enabled(crtc->dev->dev,
					      pdp_crtc->pdp_reg,
					      enable);
		break;
	default:
		BUG();
	}
}

static void pdp_crtc_set_enabled(struct drm_crtc *crtc, bool enable)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;

	if (enable) {
		pdp_crtc_set_syncgen_enabled(crtc, enable);
		pdp_crtc_set_plane_enabled(crtc, dev_priv->display_enabled);
		drm_crtc_vblank_on(crtc);
	} else {
		drm_crtc_vblank_off(crtc);
		pdp_crtc_set_plane_enabled(crtc, enable);
		pdp_crtc_set_syncgen_enabled(crtc, enable);
	}
}

static void pdp_crtc_mode_set(struct drm_crtc *crtc,
			      struct drm_display_mode *adjusted_mode)
{
	/*
	 * ht   = horizontal total
	 * hbps = horizontal back porch start
	 * has  = horizontal active start
	 * hlbs = horizontal left border start
	 * hfps = horizontal front porch start
	 * hrbs = horizontal right border start
	 *
	 * vt   = vertical total
	 * vbps = vertical back porch start
	 * vas  = vertical active start
	 * vtbs = vertical top border start
	 * vfps = vertical front porch start
	 * vbbs = vertical bottom border start
	 */
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	uint32_t ht = adjusted_mode->htotal;
	uint32_t hbps = adjusted_mode->hsync_end - adjusted_mode->hsync_start;
	uint32_t has = (adjusted_mode->htotal - adjusted_mode->hsync_start);
	uint32_t hlbs = has;
	uint32_t hfps = (hlbs + adjusted_mode->hdisplay);
	uint32_t hrbs = hfps;
	uint32_t vt = adjusted_mode->vtotal;
	uint32_t vbps = adjusted_mode->vsync_end - adjusted_mode->vsync_start;
	uint32_t vas = (adjusted_mode->vtotal - adjusted_mode->vsync_start);
	uint32_t vtbs = vas;
	uint32_t vfps = (vtbs + adjusted_mode->vdisplay);
	uint32_t vbbs = vfps;
	bool ok;

	ok = pdp_clocks_set(crtc, adjusted_mode);

	if (!ok) {
		dev_info(crtc->dev->dev, "%s failed\n", __func__);
		return;
	}

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		pdp_odin_set_updates_enabled(crtc->dev->dev,
					     pdp_crtc->pdp_reg, false);
		pdp_odin_reset_planes(crtc->dev->dev,
				      pdp_crtc->pdp_reg);
		pdp_odin_mode_set(crtc->dev->dev,
			     pdp_crtc->pdp_reg,
			     adjusted_mode->hdisplay, adjusted_mode->vdisplay,
			     hbps, ht, has,
			     hlbs, hfps, hrbs,
			     vbps, vt, vas,
			     vtbs, vfps, vbbs,
			     adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC,
			     adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC);
		pdp_odin_set_powerdwn_enabled(crtc->dev->dev,
					      pdp_crtc->pdp_reg, false);
		pdp_odin_set_updates_enabled(crtc->dev->dev,
					     pdp_crtc->pdp_reg, true);
		break;
	case PDP_VERSION_APOLLO:
		pdp_apollo_set_updates_enabled(crtc->dev->dev,
					       pdp_crtc->pdp_reg, false);
		pdp_apollo_reset_planes(crtc->dev->dev,
					pdp_crtc->pdp_reg);
		pdp_apollo_mode_set(crtc->dev->dev,
			     pdp_crtc->pdp_reg,
			     adjusted_mode->hdisplay, adjusted_mode->vdisplay,
			     hbps, ht, has,
			     hlbs, hfps, hrbs,
			     vbps, vt, vas,
			     vtbs, vfps, vbbs,
			     adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC,
			     adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC);
		pdp_apollo_set_powerdwn_enabled(crtc->dev->dev,
						pdp_crtc->pdp_reg, false);
		pdp_apollo_set_updates_enabled(crtc->dev->dev,
					       pdp_crtc->pdp_reg, true);
		break;
	case PDP_VERSION_PLATO:
		pdp_plato_mode_set(crtc->dev->dev,
				   pdp_crtc->pdp_reg,
				   adjusted_mode->hdisplay,
				   adjusted_mode->vdisplay,
				   hbps, ht, has,
				   hlbs, hfps, hrbs,
				   vbps, vt, vas,
				   vtbs, vfps, vbbs,
				   adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC,
				   adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC);
		break;
	default:
		BUG();
	}
}


static bool pdp_crtc_helper_mode_fixup(struct drm_crtc *crtc,
					const struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;

	if (dev_priv->version == PDP_VERSION_ODIN
		&& mode->hdisplay == 1920
		&& mode->vdisplay == 1080) {

		/* 1080p 60Hz */
		const int h_total = 2200;
		const int h_active_start = 192;
		const int h_back_porch_start = 44;
		const int v_total = 1125;
		const int v_active_start = 41;
		const int v_back_porch_start = 5;

		adjusted_mode->htotal = h_total;
		adjusted_mode->hsync_start = adjusted_mode->htotal -
						h_active_start;
		adjusted_mode->hsync_end = adjusted_mode->hsync_start +
						h_back_porch_start;
		adjusted_mode->vtotal = v_total;
		adjusted_mode->vsync_start = adjusted_mode->vtotal -
						v_active_start;
		adjusted_mode->vsync_end = adjusted_mode->vsync_start +
						v_back_porch_start;
	}
	return true;
}

static void pdp_crtc_flip_complete(struct drm_crtc *crtc);

#if defined(PDP_USE_ATOMIC)
static void pdp_crtc_helper_mode_set_nofb(struct drm_crtc *crtc)
{
	pdp_crtc_mode_set(crtc, &crtc->state->adjusted_mode);
}

static void pdp_crtc_helper_atomic_flush(struct drm_crtc *crtc,
					 struct drm_crtc_state *old_crtc_state)
{
	struct drm_crtc_state *new_crtc_state = crtc->state;

	if (!new_crtc_state->active || !old_crtc_state->active)
		return;

	if (crtc->state->event) {
		struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
		unsigned long flags;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		pdp_crtc->flip_async = new_crtc_state->async_flip;
#else
		pdp_crtc->flip_async = !!(new_crtc_state->pageflip_flags
					  & DRM_MODE_PAGE_FLIP_ASYNC);
#endif
		if (pdp_crtc->flip_async)
			WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		pdp_crtc->flip_event = crtc->state->event;
		crtc->state->event = NULL;

		atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_DONE);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

		if (pdp_crtc->flip_async)
			pdp_crtc_flip_complete(crtc);
	}
}

static void pdp_crtc_helper_atomic_enable(struct drm_crtc *crtc,
					  struct drm_crtc_state *old_crtc_state)
{
	pdp_crtc_set_enabled(crtc, true);

	if (crtc->state->event) {
		struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
		unsigned long flags;

		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		pdp_crtc->flip_event = crtc->state->event;
		crtc->state->event = NULL;

		atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_DONE);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}

static void pdp_crtc_helper_atomic_disable(struct drm_crtc *crtc,
					   struct drm_crtc_state *old_crtc_state)
{
	pdp_crtc_set_enabled(crtc, false);

	if (crtc->state->event) {
		unsigned long flags;

		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		crtc->state->event = NULL;
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}
}
#else
static void pdp_crtc_helper_dpms(struct drm_crtc *crtc, int mode)
{
}

static void pdp_crtc_helper_prepare(struct drm_crtc *crtc)
{
	pdp_crtc_set_enabled(crtc, false);
}

static void pdp_crtc_helper_commit(struct drm_crtc *crtc)
{
	pdp_crtc_set_enabled(crtc, true);
}

static int pdp_crtc_helper_mode_set_base_atomic(struct drm_crtc *crtc,
						struct drm_framebuffer *fb,
						int x, int y,
						enum mode_set_atomic atomic)
{
	if (x < 0 || y < 0)
		return -EINVAL;

	pdp_plane_set_surface(crtc, crtc->primary, fb,
			      (uint32_t) x, (uint32_t) y);

	return 0;
}

static int pdp_crtc_helper_mode_set_base(struct drm_crtc *crtc,
					 int x, int y,
					 struct drm_framebuffer *old_fb)
{
	if (!crtc->primary->fb) {
		DRM_ERROR("no framebuffer\n");
		return 0;
	}

	return pdp_crtc_helper_mode_set_base_atomic(crtc,
						    crtc->primary->fb,
						    x, y,
						    0);
}

static int pdp_crtc_helper_mode_set(struct drm_crtc *crtc,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode,
				    int x, int y,
				    struct drm_framebuffer *old_fb)
{
	pdp_crtc_mode_set(crtc, adjusted_mode);

	return pdp_crtc_helper_mode_set_base(crtc, x, y, old_fb);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static void pdp_crtc_helper_load_lut(struct drm_crtc *crtc)
{
}
#endif

static void pdp_crtc_helper_disable(struct drm_crtc *crtc)
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	enum pdp_crtc_flip_status status;

	pdp_crtc_set_enabled(crtc, false);

	status = atomic_read(&pdp_crtc->flip_status);
	if (status != PDP_CRTC_FLIP_STATUS_NONE) {
		long lerr;

		lerr = wait_event_timeout(
			pdp_crtc->flip_pending_wait_queue,
			atomic_read(&pdp_crtc->flip_status)
					!= PDP_CRTC_FLIP_STATUS_PENDING,
			30 * HZ);
		if (!lerr)
			DRM_ERROR("Failed to wait for pending flip\n");
		else if (!pdp_crtc->flip_async)
			pdp_crtc_flip_complete(crtc);
	}
}
#endif /* defined(PDP_USE_ATOMIC) */

static void pdp_crtc_destroy(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct pdp_drm_private *dev_priv = dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);

	DRM_DEBUG_DRIVER("[CRTC:%d]\n", crtc->base.id);

	drm_crtc_cleanup(crtc);

	iounmap(pdp_crtc->pll_reg);

	iounmap(pdp_crtc->pdp_reg);
	release_mem_region(pdp_crtc->pdp_reg_phys_base, pdp_crtc->pdp_reg_size);

	kfree(pdp_crtc);
	dev_priv->crtc = NULL;
}

static void pdp_crtc_flip_complete(struct drm_crtc *crtc)
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	/* The flipping process has been completed so reset the flip state */
	atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_NONE);
	pdp_crtc->flip_async = false;

#if !defined(PDP_USE_ATOMIC)
	if (pdp_crtc->flip_data) {
		dma_fence_put(pdp_crtc->flip_data->wait_fence);
		kfree(pdp_crtc->flip_data);
		pdp_crtc->flip_data = NULL;
	}
#endif

	if (pdp_crtc->flip_event) {
		drm_crtc_send_vblank_event(crtc, pdp_crtc->flip_event);
		pdp_crtc->flip_event = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

#if !defined(PDP_USE_ATOMIC)
static void pdp_crtc_flip(struct drm_crtc *crtc)
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	struct drm_framebuffer *old_fb;

	WARN_ON(atomic_read(&to_pdp_crtc(crtc)->flip_status)
			!= PDP_CRTC_FLIP_STATUS_PENDING);

	old_fb = pdp_crtc->old_fb;
	pdp_crtc->old_fb = NULL;

	/*
	 * The graphics stream registers latch on vsync so we can go ahead and
	 * do the flip now.
	 */
	(void) pdp_crtc_helper_mode_set_base(crtc, crtc->x, crtc->y, old_fb);

	atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_DONE);
	wake_up(&pdp_crtc->flip_pending_wait_queue);

	if (pdp_crtc->flip_async)
		pdp_crtc_flip_complete(crtc);
}

static void pdp_crtc_flip_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct pdp_flip_data *flip_data =
		container_of(cb, struct pdp_flip_data, base);

	pdp_crtc_flip(flip_data->crtc);
}

static void pdp_crtc_flip_schedule_cb(struct dma_fence *fence,
				      struct dma_fence_cb *cb)
{
	struct pdp_flip_data *flip_data =
		container_of(cb, struct pdp_flip_data, base);
	int err = 0;

	if (flip_data->wait_fence)
		err = dma_fence_add_callback(flip_data->wait_fence,
					     &flip_data->base,
					     pdp_crtc_flip_cb);

	if (!flip_data->wait_fence || err) {
		if (err && err != -ENOENT)
			DRM_ERROR("flip failed to wait on old buffer\n");
		pdp_crtc_flip_cb(flip_data->wait_fence, &flip_data->base);
	}
}

static int pdp_crtc_flip_schedule(struct drm_crtc *crtc,
				  struct drm_gem_object *obj,
				  struct drm_gem_object *old_obj)
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	struct dma_resv *resv = pdp_gem_get_resv(obj);
	struct dma_resv *old_resv = pdp_gem_get_resv(old_obj);
	struct pdp_flip_data *flip_data;
	struct dma_fence *fence;
	int err;

	flip_data = kmalloc(sizeof(*flip_data), GFP_KERNEL);
	if (!flip_data)
		return -ENOMEM;

	flip_data->crtc = crtc;

	ww_mutex_lock(&old_resv->lock, NULL);
	flip_data->wait_fence =
		dma_fence_get(dma_resv_get_excl(old_resv));

	if (old_resv != resv) {
		ww_mutex_unlock(&old_resv->lock);
		ww_mutex_lock(&resv->lock, NULL);
	}

	fence = dma_fence_get(dma_resv_get_excl(resv));
	ww_mutex_unlock(&resv->lock);

	pdp_crtc->flip_data = flip_data;
	atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_PENDING);

	if (fence) {
		err = dma_fence_add_callback(fence, &flip_data->base,
					     pdp_crtc_flip_schedule_cb);
		dma_fence_put(fence);
		if (err && err != -ENOENT)
			goto err_set_flip_status_none;
	}

	if (!fence || err == -ENOENT) {
		pdp_crtc_flip_schedule_cb(fence, &flip_data->base);
		err = 0;
	}

	return err;

err_set_flip_status_none:
	atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_NONE);
	dma_fence_put(flip_data->wait_fence);
	kfree(flip_data);
	return err;
}

static int pdp_crtc_page_flip(struct drm_crtc *crtc,
			      struct drm_framebuffer *fb,
			      struct drm_pending_vblank_event *event,
			      uint32_t page_flip_flags
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
			      , struct drm_modeset_acquire_ctx *ctx
#endif
			     )
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	struct pdp_framebuffer *pdp_fb = to_pdp_framebuffer(fb);
	struct pdp_framebuffer *pdp_old_fb =
		to_pdp_framebuffer(crtc->primary->fb);
	enum pdp_crtc_flip_status status;
	unsigned long flags;
	int err;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	status = atomic_read(&pdp_crtc->flip_status);
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

	if (status != PDP_CRTC_FLIP_STATUS_NONE)
		return -EBUSY;

	if (!(page_flip_flags & DRM_MODE_PAGE_FLIP_ASYNC)) {
		err = drm_crtc_vblank_get(crtc);
		if (err)
			return err;
	}

	pdp_crtc->old_fb = crtc->primary->fb;
	pdp_crtc->flip_event = event;
	pdp_crtc->flip_async = !!(page_flip_flags & DRM_MODE_PAGE_FLIP_ASYNC);

	/* Set the crtc primary plane to point to the new framebuffer */
	crtc->primary->fb = fb;

	err = pdp_crtc_flip_schedule(crtc, pdp_fb->obj[0], pdp_old_fb->obj[0]);
	if (err) {
		crtc->primary->fb = pdp_crtc->old_fb;
		pdp_crtc->old_fb = NULL;
		pdp_crtc->flip_event = NULL;
		pdp_crtc->flip_async = false;

		DRM_ERROR("failed to schedule flip (err=%d)\n", err);
		goto err_vblank_put;
	}

	return 0;

err_vblank_put:
	if (!(page_flip_flags & DRM_MODE_PAGE_FLIP_ASYNC))
		drm_crtc_vblank_put(crtc);
	return err;
}
#endif /* !defined(PDP_USE_ATOMIC) */

static const struct drm_crtc_helper_funcs pdp_crtc_helper_funcs = {
	.mode_fixup = pdp_crtc_helper_mode_fixup,
#if defined(PDP_USE_ATOMIC)
	.mode_set_nofb = pdp_crtc_helper_mode_set_nofb,
	.atomic_flush = pdp_crtc_helper_atomic_flush,
	.atomic_enable = pdp_crtc_helper_atomic_enable,
	.atomic_disable = pdp_crtc_helper_atomic_disable,
#else
	.dpms = pdp_crtc_helper_dpms,
	.prepare = pdp_crtc_helper_prepare,
	.commit = pdp_crtc_helper_commit,
	.mode_set = pdp_crtc_helper_mode_set,
	.mode_set_base = pdp_crtc_helper_mode_set_base,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
	.load_lut = pdp_crtc_helper_load_lut,
#endif
	.mode_set_base_atomic = pdp_crtc_helper_mode_set_base_atomic,
	.disable = pdp_crtc_helper_disable,
#endif
};

static const struct drm_crtc_funcs pdp_crtc_funcs = {
	.destroy = pdp_crtc_destroy,
#if defined(PDP_USE_ATOMIC)
	.reset = drm_atomic_helper_crtc_reset,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
#else
	.set_config = drm_crtc_helper_set_config,
	.page_flip = pdp_crtc_page_flip,
#endif
};


struct drm_crtc *pdp_crtc_create(struct drm_device *dev, uint32_t number,
				 struct drm_plane *primary_plane)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;
	struct pdp_crtc *pdp_crtc;
	const char *crtc_name = NULL;
	int err;

	pdp_crtc = kzalloc(sizeof(*pdp_crtc), GFP_KERNEL);
	if (!pdp_crtc) {
		err = -ENOMEM;
		goto err_exit;
	}

	init_waitqueue_head(&pdp_crtc->flip_pending_wait_queue);
	atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_NONE);
	pdp_crtc->number = number;

	switch (number) {
	case 0:
	{
		struct resource *regs;

		regs = platform_get_resource_byname(
				    to_platform_device(dev->dev),
				    IORESOURCE_MEM,
				    "pdp-regs");
		if (!regs) {
			DRM_ERROR("missing pdp register info\n");
			err = -ENXIO;
			goto err_crtc_free;
		}

		pdp_crtc->pdp_reg_phys_base = regs->start;
		pdp_crtc->pdp_reg_size = resource_size(regs);

		if (dev_priv->version == PDP_VERSION_ODIN ||
			dev_priv->version == PDP_VERSION_APOLLO) {
			regs = platform_get_resource_byname(
					    to_platform_device(dev->dev),
					    IORESOURCE_MEM,
					    "pll-regs");
			if (!regs) {
				DRM_ERROR("missing pll register info\n");
				err = -ENXIO;
				goto err_crtc_free;
			}

			pdp_crtc->pll_reg_phys_base = regs->start;
			pdp_crtc->pll_reg_size = resource_size(regs);

			pdp_crtc->pll_reg =
				ioremap_nocache(pdp_crtc->pll_reg_phys_base,
						pdp_crtc->pll_reg_size);
			if (!pdp_crtc->pll_reg) {
				DRM_ERROR("failed to map pll registers\n");
				err = -ENOMEM;
				goto err_crtc_free;
			}
		} else if (dev_priv->version == PDP_VERSION_PLATO) {
			regs = platform_get_resource_byname(
				    to_platform_device(dev->dev),
				    IORESOURCE_MEM,
				    PLATO_PDP_RESOURCE_BIF_REGS);
			if (!regs) {
				DRM_ERROR("missing pdp-bif register info\n");
				err = -ENXIO;
				goto err_crtc_free;
			}

			pdp_crtc->pdp_bif_reg_phys_base = regs->start;
			pdp_crtc->pdp_bif_reg_size = resource_size(regs);

			if (!request_mem_region(pdp_crtc->pdp_bif_reg_phys_base,
					pdp_crtc->pdp_bif_reg_size,
					crtc_name)) {
				DRM_ERROR("failed to reserve pdp-bif registers\n");
				err = -EBUSY;
				goto err_crtc_free;
			}

			pdp_crtc->pdp_bif_reg =
				ioremap_nocache(pdp_crtc->pdp_bif_reg_phys_base,
						pdp_crtc->pdp_bif_reg_size);
			if (!pdp_crtc->pdp_bif_reg) {
				DRM_ERROR("failed to map pdp-bif registers\n");
				err = -ENOMEM;
				goto err_iounmap_regs;
			}
		}

		if (dev_priv->version == PDP_VERSION_ODIN) {
			regs = platform_get_resource_byname(
					    to_platform_device(dev->dev),
					    IORESOURCE_MEM,
					    "odn-core");
			if (!regs) {
				DRM_ERROR("missing odn-core info\n");
				err = -ENXIO;
				goto err_crtc_free;
			}

			pdp_crtc->odn_core_phys_base = regs->start;
			pdp_crtc->odn_core_size = resource_size(regs);

			pdp_crtc->odn_core_reg
				= ioremap_nocache(pdp_crtc->odn_core_phys_base,
					  pdp_crtc->odn_core_size);
			if (!pdp_crtc->odn_core_reg) {
				DRM_ERROR("failed to map pdp reset register\n");
				err = -ENOMEM;
				goto err_iounmap_regs;
			}
		}

		crtc_name = "crtc-0";
		break;
	}
	default:
		DRM_ERROR("invalid crtc number %u\n", number);
		err = -EINVAL;
		goto err_crtc_free;
	}

	if (!request_mem_region(pdp_crtc->pdp_reg_phys_base,
				pdp_crtc->pdp_reg_size,
				crtc_name)) {
		DRM_ERROR("failed to reserve pdp registers\n");
		err = -EBUSY;
		goto err_crtc_free;
	}

	pdp_crtc->pdp_reg = ioremap_nocache(pdp_crtc->pdp_reg_phys_base,
							pdp_crtc->pdp_reg_size);
	if (!pdp_crtc->pdp_reg) {
		DRM_ERROR("failed to map pdp registers\n");
		err = -ENOMEM;
		goto err_release_mem_region;
	}

	err = drm_crtc_init_with_planes(dev, &pdp_crtc->base, primary_plane,
					NULL, &pdp_crtc_funcs, NULL);
	if (err) {
		DRM_ERROR("CRTC init with planes failed");
		goto err_iounmap_regs;
	}

	drm_crtc_helper_add(&pdp_crtc->base, &pdp_crtc_helper_funcs);

	DRM_DEBUG_DRIVER("[CRTC:%d]\n", pdp_crtc->base.base.id);

	return &pdp_crtc->base;

err_iounmap_regs:
	iounmap(pdp_crtc->pdp_reg);
	if (pdp_crtc->odn_core_reg)
		iounmap(pdp_crtc->odn_core_reg);
	if (pdp_crtc->pdp_bif_reg)
		iounmap(pdp_crtc->pdp_bif_reg);
err_release_mem_region:
	release_mem_region(pdp_crtc->pdp_reg_phys_base, pdp_crtc->pdp_reg_size);
err_crtc_free:
	kfree(pdp_crtc);
err_exit:
	return ERR_PTR(err);
}

void pdp_crtc_set_vblank_enabled(struct drm_crtc *crtc, bool enable)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		pdp_odin_set_vblank_enabled(crtc->dev->dev,
					    pdp_crtc->pdp_reg,
					    enable);
		break;
	case PDP_VERSION_APOLLO:
		pdp_apollo_set_vblank_enabled(crtc->dev->dev,
					    pdp_crtc->pdp_reg,
					    enable);
		break;
	case PDP_VERSION_PLATO:
		pdp_plato_set_vblank_enabled(crtc->dev->dev,
					     pdp_crtc->pdp_reg,
					     enable);
		break;
	default:
		BUG();
	}
}

void pdp_crtc_irq_handler(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct pdp_drm_private *dev_priv = dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	bool handled;

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		handled = pdp_odin_check_and_clear_vblank(dev->dev,
							  pdp_crtc->pdp_reg);
		break;
	case PDP_VERSION_APOLLO:
		handled = pdp_apollo_check_and_clear_vblank(dev->dev,
							    pdp_crtc->pdp_reg);
		break;
	case PDP_VERSION_PLATO:
		handled = pdp_plato_check_and_clear_vblank(dev->dev,
							   pdp_crtc->pdp_reg);
		break;
	default:
		handled = false;
		break;
	}

	if (handled) {
		enum pdp_crtc_flip_status status;

		drm_handle_vblank(dev, pdp_crtc->number);

		status = atomic_read(&pdp_crtc->flip_status);
		if (status == PDP_CRTC_FLIP_STATUS_DONE) {
			if (!pdp_crtc->flip_async) {
				pdp_crtc_flip_complete(crtc);
#if !defined(PDP_USE_ATOMIC)
				drm_crtc_vblank_put(crtc);
#endif
			}
		}
	}
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
void pdp_crtc_flip_event_cancel(struct drm_crtc *crtc, struct drm_file *file)
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	unsigned long flags;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	if (pdp_crtc->flip_event &&
	    pdp_crtc->flip_event->base.file_priv == file) {
		pdp_crtc->flip_event->base.destroy(&pdp_crtc->flip_event->base);
		pdp_crtc->flip_event = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}
#endif

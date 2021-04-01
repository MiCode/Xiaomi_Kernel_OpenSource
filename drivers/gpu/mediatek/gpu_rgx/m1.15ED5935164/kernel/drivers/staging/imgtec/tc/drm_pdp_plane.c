/*
 * @File
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/version.h>

#include "drm_pdp_drv.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0))
#include <drm/drmP.h>
#endif

#include <drm/drm_plane_helper.h>

#if defined(PDP_USE_ATOMIC)
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#endif

#include <powervr/img_drm_fourcc.h>

#include "drm_pdp_gem.h"
#include "pdp_apollo.h"
#include "pdp_odin.h"
#include "pdp_plato.h"
#include "pfim_defs.h"

#include "kernel_compatibility.h"


#if defined(PDP_USE_ATOMIC)
static int pdp_plane_helper_atomic_check(struct drm_plane *plane,
					 struct drm_plane_state *state)
{
	struct drm_crtc_state *crtc_new_state;

	if (!state->crtc)
		return 0;

	crtc_new_state = drm_atomic_get_new_crtc_state(state->state,
						       state->crtc);

	return drm_atomic_helper_check_plane_state(state, crtc_new_state,
						   DRM_PLANE_HELPER_NO_SCALING,
						   DRM_PLANE_HELPER_NO_SCALING,
						   false, true);
}

static void pdp_plane_helper_atomic_update(struct drm_plane *plane,
					   struct drm_plane_state *old_state)
{
	struct drm_plane_state *plane_state = plane->state;
	struct drm_framebuffer *fb = plane_state->fb;

	if (fb) {
		pdp_plane_set_surface(plane_state->crtc, plane, fb,
				      plane_state->src_x, plane_state->src_y);
	}
}

static const struct drm_plane_helper_funcs pdp_plane_helper_funcs = {
	.prepare_fb =  drm_gem_fb_prepare_fb,
	.atomic_check = pdp_plane_helper_atomic_check,
	.atomic_update = pdp_plane_helper_atomic_update,
};

static const struct drm_plane_funcs pdp_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_primary_helper_destroy,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};
#else
#define pdp_plane_funcs drm_primary_helper_funcs
#endif

struct drm_plane *pdp_plane_create(struct drm_device *dev,
				   enum drm_plane_type type)
{
	struct pdp_drm_private *dev_priv = dev->dev_private;
	struct drm_plane *plane;
	const uint32_t *supported_formats;
	uint32_t num_supported_formats;
	const uint32_t apollo_plato_formats[] = {
		DRM_FORMAT_XRGB8888,
		DRM_FORMAT_ARGB8888,
	};
	const uint32_t odin_formats[] = {
		DRM_FORMAT_XRGB8888,
		DRM_FORMAT_ARGB8888,
		DRM_FORMAT_RGB565,
	};
	int err;

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		supported_formats = odin_formats;
		num_supported_formats = ARRAY_SIZE(odin_formats);
		break;
	case PDP_VERSION_APOLLO:
	case PDP_VERSION_PLATO:
		supported_formats = apollo_plato_formats;
		num_supported_formats = ARRAY_SIZE(apollo_plato_formats);
		break;
	default:
		DRM_ERROR("Unsupported PDP version\n");
		err = -EINVAL;
		goto err_exit;
	}

	plane = kzalloc(sizeof(*plane), GFP_KERNEL);
	if (!plane) {
		err = -ENOMEM;
		goto err_exit;
	}

	err = drm_universal_plane_init(dev, plane, 0, &pdp_plane_funcs,
				       supported_formats,
				       num_supported_formats,
				       NULL, type, NULL);
	if (err)
		goto err_plane_free;

#if defined(PDP_USE_ATOMIC)
	drm_plane_helper_add(plane, &pdp_plane_helper_funcs);
#endif

	DRM_DEBUG_DRIVER("[PLANE:%d]\n", plane->base.id);

	return plane;

err_plane_free:
	kfree(plane);
err_exit:
	return ERR_PTR(err);
}

void pdp_plane_set_surface(struct drm_crtc *crtc, struct drm_plane *plane,
			   struct drm_framebuffer *fb,
			   const uint32_t src_x, const uint32_t src_y)
{
	struct pdp_drm_private *dev_priv = plane->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	struct pdp_framebuffer *pdp_fb = to_pdp_framebuffer(fb);
	unsigned int pitch = fb->pitches[0];
	uint64_t address = pdp_gem_get_dev_addr(pdp_fb->obj[0]);
	uint64_t modifier = 0;
	uint32_t format;
	uint32_t fbc_mode;

	/*
	 * User space specifies 'x' and 'y' and this is used to tell the display
	 * to scan out from part way through a buffer.
	 */
	address += ((src_y * pitch) + (src_x * (pdp_drm_fb_cpp(fb))));

	/*
	 * NOTE: If the buffer dimensions are less than the current mode then
	 * the output will appear in the top left of the screen. This can be
	 * centered by adjusting horizontal active start, right border start,
	 * vertical active start and bottom border start. At this point it's
	 * not entirely clear where this should be done. On the one hand it's
	 * related to pdp_crtc_helper_mode_set but on the other hand there
	 * might not always be a call to pdp_crtc_helper_mode_set. This needs
	 * to be investigated.
	 */
	switch (dev_priv->version) {
	case PDP_VERSION_APOLLO:
		switch (pdp_drm_fb_format(fb)) {
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
			format = 0xE;
			break;
		default:
			DRM_ERROR("unsupported pixel format (format = %d)\n",
				  pdp_drm_fb_format(fb));
			return;
		}

		pdp_apollo_set_surface(plane->dev->dev,
				       pdp_crtc->pdp_reg,
				       0,
				       address,
				       0, 0,
				       fb->width, fb->height, pitch,
				       format,
				       255,
				       false);
		break;
	case PDP_VERSION_ODIN:
		switch (pdp_drm_fb_format(fb)) {
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
			format = ODN_PDP_SURF_PIXFMT_ARGB8888;
			break;
		case DRM_FORMAT_RGB565:
			format = ODN_PDP_SURF_PIXFMT_RGB565;
			break;
		default:
			DRM_ERROR("unsupported pixel format (format = %d)\n",
				  pdp_drm_fb_format(fb));
			return;
		}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
		modifier = fb->modifier;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0))
		modifier = fb->modifier[0];
#endif

		switch (modifier) {
		case DRM_FORMAT_MOD_PVR_FBCDC_8x8_V12:
			fbc_mode = ODIN_PFIM_FBCDC_8X8_V12;
			break;
		case DRM_FORMAT_MOD_PVR_FBCDC_16x4_V12:
			fbc_mode = ODIN_PFIM_FBCDC_16X4_V12;
			break;
		case DRM_FORMAT_MOD_LINEAR:
			fbc_mode = ODIN_PFIM_MOD_LINEAR;
			break;
		default:
			DRM_ERROR("unsupported fbc format (format = %llu)\n",
				  modifier);
			return;
		}

		pdp_odin_set_surface(plane->dev->dev,
				     pdp_crtc->pdp_reg,
				     0,
				     address, fb->offsets[0],
				     0, 0,
				     fb->width, fb->height, pitch,
				     format,
				     255,
				     false,
				     pdp_crtc->pfim_reg, fbc_mode);
		break;
	case PDP_VERSION_PLATO:
		switch (pdp_drm_fb_format(fb)) {
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
			format = PLATO_PDP_PIXEL_FORMAT_ARGB8;
			break;
		default:
			DRM_ERROR("unsupported pixel format (format = %d)\n",
				  pdp_drm_fb_format(fb));
			return;
		}

		pdp_plato_set_surface(crtc->dev->dev,
				      pdp_crtc->pdp_reg,
				      pdp_crtc->pdp_bif_reg,
				      0,
				      address,
				      0, 0,
				      fb->width, fb->height, pitch,
				      format,
				      255,
				      false);
		break;
	default:
			BUG();
	}
}

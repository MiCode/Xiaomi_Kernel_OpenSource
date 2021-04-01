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

#if !defined(__DRM_PDP_DRV_H__)
#define __DRM_PDP_DRV_H__

#include <linux/version.h>
#include <linux/wait.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0))
#include <drm/drm_fourcc.h>
#else
#include <drm/drmP.h>
#endif

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_mm.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
#include <drm/drm_plane.h>
#endif

#include "pdp_common.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)) && \
	!defined(PVR_ANDROID_USE_PDP_LEGACY)
#define PDP_USE_ATOMIC
#endif

struct pdp_gem_context;
enum pdp_crtc_flip_status;
struct pdp_flip_data;
struct pdp_gem_private;

#if !defined(SUPPORT_PLATO_DISPLAY)
struct tc_pdp_platform_data;
#else
struct plato_pdp_platform_data;
#endif

struct pdp_drm_private {
	struct drm_device *dev;
#if defined(CONFIG_DRM_FBDEV_EMULATION)
	struct pdp_fbdev *fbdev;
#endif

	enum pdp_version version;

	/* differentiate Orion from base Odin PDP */
	enum pdp_odin_subversion subversion;

	/* created by pdp_gem_init */
	struct pdp_gem_private	*gem_priv;

	/* preferred output device */
	enum pdp_output_device outdev;
	uint32_t pdp_interrupt;

	/* PDP FBC Decompression module support  */
	bool pfim_capable;

	/* initialised by pdp_modeset_early_init */
	struct drm_plane *plane;
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct drm_encoder *encoder;

	bool display_enabled;
};

struct pdp_crtc {
	struct drm_crtc base;

	uint32_t number;

	resource_size_t pdp_reg_size;
	resource_size_t pdp_reg_phys_base;
	void __iomem *pdp_reg;

	resource_size_t pdp_bif_reg_size;
	resource_size_t pdp_bif_reg_phys_base;
	void __iomem *pdp_bif_reg;

	resource_size_t pll_reg_size;
	resource_size_t pll_reg_phys_base;
	void __iomem *pll_reg;

	resource_size_t odn_core_size; /* needed for odin pdp clk reset */
	resource_size_t odn_core_phys_base;
	void __iomem *odn_core_reg;

	resource_size_t pfim_reg_size;
	resource_size_t pfim_reg_phys_base;
	void __iomem *pfim_reg;

	wait_queue_head_t flip_pending_wait_queue;

	/* Reuse the drm_device event_lock to protect these */
	atomic_t flip_status;
	struct drm_pending_vblank_event *flip_event;
	struct drm_framebuffer *old_fb;
	struct pdp_flip_data *flip_data;
	bool flip_async;
};

#define to_pdp_crtc(crtc) container_of(crtc, struct pdp_crtc, base)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
struct drm_gem_object;

struct pdp_framebuffer {
	struct drm_framebuffer base;
	struct drm_gem_object *obj[1];
};

#define to_pdp_framebuffer(fb) container_of(fb, struct pdp_framebuffer, base)
#define to_drm_framebuffer(fb) (&(fb)->base)
#else
#define pdp_framebuffer drm_framebuffer
#define to_pdp_framebuffer(fb) (fb)
#define to_drm_framebuffer(fb) (fb)
#endif

#if defined(CONFIG_DRM_FBDEV_EMULATION)
struct pdp_fbdev {
	struct drm_fb_helper helper;
	struct pdp_framebuffer fb;
	struct pdp_drm_private *priv;
	u8 preferred_bpp;
};
#endif

static inline u32 pdp_drm_fb_cpp(struct drm_framebuffer *fb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
	return fb->format->cpp[0];
#else
	return fb->bits_per_pixel / 8;
#endif
}

static inline u32 pdp_drm_fb_format(struct drm_framebuffer *fb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
	return fb->format->format;
#else
	return fb->pixel_format;
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 8, 0))
int pdp_debugfs_init(struct drm_minor *minor);
#else
void pdp_debugfs_init(struct drm_minor *minor);
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0))
void pdp_debugfs_cleanup(struct drm_minor *minor);
#endif

struct drm_plane *pdp_plane_create(struct drm_device *dev,
				   enum drm_plane_type type);
void pdp_plane_set_surface(struct drm_crtc *crtc, struct drm_plane *plane,
			   struct drm_framebuffer *fb,
			   const uint32_t src_x, const uint32_t src_y);

struct drm_crtc *pdp_crtc_create(struct drm_device *dev, uint32_t number,
				 struct drm_plane *primary_plane);
void pdp_crtc_set_plane_enabled(struct drm_crtc *crtc, bool enable);
void pdp_crtc_set_vblank_enabled(struct drm_crtc *crtc, bool enable);
void pdp_crtc_irq_handler(struct drm_crtc *crtc);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
void pdp_crtc_flip_event_cancel(struct drm_crtc *crtc, struct drm_file *file);
#endif

struct drm_connector *pdp_dvi_connector_create(struct drm_device *dev);

struct drm_encoder *pdp_tmds_encoder_create(struct drm_device *dev);

int pdp_modeset_early_init(struct pdp_drm_private *dev_priv);
int pdp_modeset_late_init(struct pdp_drm_private *dev_priv);
void pdp_modeset_early_cleanup(struct pdp_drm_private *dev_priv);
void pdp_modeset_late_cleanup(struct pdp_drm_private *dev_priv);

#if defined(CONFIG_DRM_FBDEV_EMULATION)
struct pdp_fbdev *pdp_fbdev_create(struct pdp_drm_private *dev);
void pdp_fbdev_destroy(struct pdp_fbdev *fbdev);
#endif

int pdp_modeset_validate_init(struct pdp_drm_private *dev_priv,
			      struct drm_mode_fb_cmd2 *mode_cmd,
			      struct pdp_framebuffer *pdp_fb,
			      struct drm_gem_object *obj);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
int pdp_enable_vblank(struct drm_crtc *crtc);
void pdp_disable_vblank(struct drm_crtc *crtc);
#endif

#endif /* !defined(__DRM_PDP_DRV_H__) */

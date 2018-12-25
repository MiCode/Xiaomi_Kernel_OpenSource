/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
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
#include "pvr_sw_fence.h"

#include <linux/reservation.h>
#include <linux/version.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>

#include "drm_pdp_drv.h"
#include "drm_pdp_gem.h"

#include "pdp_apollo.h"
#include "pdp_odin.h"

#include "plato_drv.h"
#include "pdp2_regs.h"
#include "pdp2_mmu_regs.h"

#include "kernel_compatibility.h"

#define PDP_STRIDE_SHIFT 4
#define PDP_BASE_ADDR_SHIFT 4
#define PLATO_PDP_STRIDE_SHIFT 5
#define PDP_REDUCED_BLANKING_VEVENT 1

#define PLATO_PDP_PIXEL_FORMAT_G		(0x00)
#define PLATO_PDP_PIXEL_FORMAT_ARGB4	(0x04)
#define PLATO_PDP_PIXEL_FORMAT_ARGB1555	(0x05)
#define PLATO_PDP_PIXEL_FORMAT_RGB8		(0x06)
#define PLATO_PDP_PIXEL_FORMAT_RGB565	(0x07)
#define PLATO_PDP_PIXEL_FORMAT_ARGB8	(0x08)
#define PLATO_PDP_PIXEL_FORMAT_AYUV8	(0x10)
#define PLATO_PDP_PIXEL_FORMAT_YUV10	(0x15)
#define PLATO_PDP_PIXEL_FORMAT_RGBA8	(0x16)

enum pdp_crtc_flip_status {
	PDP_CRTC_FLIP_STATUS_NONE = 0,
	PDP_CRTC_FLIP_STATUS_PENDING,
	PDP_CRTC_FLIP_STATUS_DONE,
};

struct pdp_flip_data {
	struct dma_fence_cb base;
	struct drm_crtc *crtc;
	struct dma_fence *wait_fence;
	struct dma_fence *complete_fence;
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
		/*plato_enable_pdp_clock(dev_priv->dev->dev->parent);*/
		res = true;
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
	uint32_t value;

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
		dev_info(crtc->dev->dev, "Set plane: %s\n",
			enable ? "enable" : "disable");

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1CTRL_OFFSET);
		value = REG_VALUE_SET(value,
				enable ? 0x1 : 0x0,
				PDP_GRPH1CTRL_GRPH1STREN_SHIFT,
				PDP_GRPH1CTRL_GRPH1STREN_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1CTRL_OFFSET,
				value);
		break;
	default:
		BUG();
	}
}

static void pdp_crtc_set_syncgen_enabled(struct drm_crtc *crtc, bool enable)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	uint32_t value;

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
		dev_info(crtc->dev->dev, "Set syncgen: %s\n",
		enable ? "enable" : "disable");

		value = plato_read_reg32(pdp_crtc->pdp_reg,
			PDP_SYNCCTRL_OFFSET);
		/* Starts Sync Generator. */
		value = REG_VALUE_SET(value,
			enable ? 0x1 : 0x0,
			PDP_SYNCCTRL_SYNCACTIVE_SHIFT,
			PDP_SYNCCTRL_SYNCACTIVE_MASK);
		/* Controls polarity of pixel clock: Pixel clock is inverted */
		value = REG_VALUE_SET(value, 0x01,
			PDP_SYNCCTRL_CLKPOL_SHIFT,
			PDP_SYNCCTRL_CLKPOL_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
			PDP_SYNCCTRL_OFFSET,
			value);
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

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0))
static inline unsigned pdp_drm_fb_cpp(struct drm_framebuffer *fb)
{
	return fb->format->cpp[0];
}

static inline u32 pdp_drm_fb_format(struct drm_framebuffer *fb)
{
	return fb->format->format;
}
#else
static inline unsigned pdp_drm_fb_cpp(struct drm_framebuffer *fb)
{
	return fb->bits_per_pixel / 8;
}

static inline u32 pdp_drm_fb_format(struct drm_framebuffer *fb)
{
	return fb->pixel_format;
}
#endif

static int pdp_crtc_helper_mode_set_base_atomic(struct drm_crtc *crtc,
						struct drm_framebuffer *fb,
						int x, int y,
						enum mode_set_atomic atomic)
{
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	struct pdp_framebuffer *pdp_fb = to_pdp_framebuffer(fb);
	unsigned int pitch = fb->pitches[0];
	uint64_t address  = pdp_gem_get_dev_addr(pdp_fb->obj);
	uint32_t value;

	/*
	 * User space specifies 'x' and 'y' and this is used to tell the display
	 * to scan out from part way through a buffer.
	 */
	address += ((y * pitch) + (x * (pdp_drm_fb_cpp(fb))));

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
	case PDP_VERSION_ODIN:
		switch (pdp_drm_fb_format(fb)) {
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
			break;
		default:
			DRM_ERROR("unsupported pixel format (format = %d)\n",
				pdp_drm_fb_format(fb));
			return -1;
		}

		pdp_odin_set_surface(crtc->dev->dev,
			pdp_crtc->pdp_reg,
			0,
			address,
			0, 0,
			fb->width,
			fb->height, pitch,
			ODN_PDP_SURF_PIXFMT_ARGB8888,
			255,
			false);
		break;
	case PDP_VERSION_APOLLO:
		switch (pdp_drm_fb_format(fb)) {
		case DRM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
			break;
		default:
			DRM_ERROR("unsupported pixel format (format = %d)\n",
				pdp_drm_fb_format(fb));
			return -1;
		}

		pdp_apollo_set_surface(crtc->dev->dev,
			pdp_crtc->pdp_reg,
			0,
			address,
			0, 0,
			fb->width, fb->height,
			pitch,
			0xE,
			255,
			false);
		break;
	case PDP_VERSION_PLATO:
		/* Set the frame buffer base address */
		if (address & 0xF) {
			dev_warn(crtc->dev->dev,
				"Warning - the frame buffer address is not aligned\n");
		}
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1BASEADDR_OFFSET,
				address & PDP_GRPH1BASEADDR_GRPH1BASEADDR_MASK);

		/* Write 8 msb of the address to address extension bits in the
		 * PDP MMU control register
		 */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
					 SYS_PLATO_REG_PDP_SIZE +
					 PDP_BIF_ADDRESS_CONTROL_OFFSET);
		value = REG_VALUE_SET(value,
			address >> 32,
			PDP_BIF_ADDRESS_CONTROL_UPPER_ADDRESS_FIXED_SHIFT,
			PDP_BIF_ADDRESS_CONTROL_UPPER_ADDRESS_FIXED_MASK);
		value = REG_VALUE_SET(value,
			0x00,
			PDP_BIF_ADDRESS_CONTROL_MMU_ENABLE_EXT_ADDRESSING_SHIFT,
			PDP_BIF_ADDRESS_CONTROL_MMU_ENABLE_EXT_ADDRESSING_MASK);
		value = REG_VALUE_SET(value,
			0x01,
			PDP_BIF_ADDRESS_CONTROL_MMU_BYPASS_SHIFT,
			PDP_BIF_ADDRESS_CONTROL_MMU_BYPASS_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
			SYS_PLATO_REG_PDP_SIZE + PDP_BIF_ADDRESS_CONTROL_OFFSET,
			value);

		/* Set the framebuffer pixel format */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1SURF_OFFSET);

		switch (pdp_drm_fb_format(fb)) {
		case DRM_FORMAT_ARGB8888:
			value = REG_VALUE_SET(value,
				PLATO_PDP_PIXEL_FORMAT_ARGB8,
				PDP_GRPH1SURF_GRPH1PIXFMT_SHIFT,
				PDP_GRPH1SURF_GRPH1PIXFMT_MASK);
			break;
		default:
			DRM_ERROR("unsupported pixel format (format = %d)\n",
				pdp_drm_fb_format(fb));
		}

		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1SURF_OFFSET, value);
		/*
		 * Set the framebuffer size (this might be smaller than the
		 * resolution)
		 */
		value = REG_VALUE_SET(0,
				fb->width - 1,
				PDP_GRPH1SIZE_GRPH1WIDTH_SHIFT,
				PDP_GRPH1SIZE_GRPH1WIDTH_MASK);
		value = REG_VALUE_SET(value,
				fb->height - 1,
				PDP_GRPH1SIZE_GRPH1HEIGHT_SHIFT,
				PDP_GRPH1SIZE_GRPH1HEIGHT_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1SIZE_OFFSET,
				value);

		/* Set the framebuffer stride in 16byte words */
		value = REG_VALUE_SET(0,
				(pitch >> PLATO_PDP_STRIDE_SHIFT) - 1,
				PDP_GRPH1STRIDE_GRPH1STRIDE_SHIFT,
				PDP_GRPH1STRIDE_GRPH1STRIDE_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_GRPH1STRIDE_OFFSET, value);

		/* Issues with NoC sending interleaved read responses to PDP
		 * require burst to be 1
		 */
		value = REG_VALUE_SET(0,
				0x02,
				PDP_MEMCTRL_MEMREFRESH_SHIFT,
				PDP_MEMCTRL_MEMREFRESH_MASK);
		value = REG_VALUE_SET(value,
				0x01,
				PDP_MEMCTRL_BURSTLEN_SHIFT,
				PDP_MEMCTRL_BURSTLEN_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_MEMCTRL_OFFSET,
				value);
		break;
	default:
		BUG();
	}
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
	uint32_t value;
	bool ok;

	ok = pdp_clocks_set(crtc, adjusted_mode);

	if (!ok) {
		dev_info(crtc->dev->dev, "pdp_crtc_helper_mode_set failed\n");
		return 0;
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
		dev_info(crtc->dev->dev,
			 "setting mode to %dx%d\n",
			 adjusted_mode->hdisplay, adjusted_mode->vdisplay);

		/* Update control */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_REGISTER_UPDATE_CTRL_OFFSET);
		value = REG_VALUE_SET(value, 0x0,
				PDP_REGISTER_UPDATE_CTRL_REGISTERS_VALID_SHIFT,
				PDP_REGISTER_UPDATE_CTRL_REGISTERS_VALID_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_REGISTER_UPDATE_CTRL_OFFSET, value);

		/* Set hsync timings */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_HSYNC1_OFFSET);
		value = REG_VALUE_SET(value,
				hbps,
				PDP_HSYNC1_HBPS_SHIFT,
				PDP_HSYNC1_HBPS_MASK);
		value = REG_VALUE_SET(value,
				ht,
				PDP_HSYNC1_HT_SHIFT,
				PDP_HSYNC1_HT_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_HSYNC1_OFFSET, value);

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_HSYNC2_OFFSET);
		value = REG_VALUE_SET(value,
				has,
				PDP_HSYNC2_HAS_SHIFT,
				PDP_HSYNC2_HAS_MASK);
		value = REG_VALUE_SET(value,
				hlbs,
				PDP_HSYNC2_HLBS_SHIFT,
				PDP_HSYNC2_HLBS_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_HSYNC2_OFFSET, value);

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_HSYNC3_OFFSET);
		value = REG_VALUE_SET(value,
				hfps,
				PDP_HSYNC3_HFPS_SHIFT,
				PDP_HSYNC3_HFPS_MASK);
		value = REG_VALUE_SET(value,
				hrbs,
				PDP_HSYNC3_HRBS_SHIFT,
				PDP_HSYNC3_HRBS_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_HSYNC3_OFFSET, value);

		/* Set vsync timings */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_VSYNC1_OFFSET);
		value = REG_VALUE_SET(value,
				vbps,
				PDP_VSYNC1_VBPS_SHIFT,
				PDP_VSYNC1_VBPS_MASK);
		value = REG_VALUE_SET(value,
				vt,
				PDP_VSYNC1_VT_SHIFT,
				PDP_VSYNC1_VT_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VSYNC1_OFFSET, value);

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_VSYNC2_OFFSET);
		value = REG_VALUE_SET(value,
				vas,
				PDP_VSYNC2_VAS_SHIFT,
				PDP_VSYNC2_VAS_MASK);
		value = REG_VALUE_SET(value,
				vtbs,
				PDP_VSYNC2_VTBS_SHIFT,
				PDP_VSYNC2_VTBS_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VSYNC2_OFFSET, value);

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_VSYNC3_OFFSET);
		value = REG_VALUE_SET(value,
				vfps,
				PDP_VSYNC3_VFPS_SHIFT,
				PDP_VSYNC3_VFPS_MASK);
		value = REG_VALUE_SET(value,
				vbbs,
				PDP_VSYNC3_VBBS_SHIFT,
				PDP_VSYNC3_VBBS_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VSYNC3_OFFSET, value);

		/* Horizontal data enable */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_HDECTRL_OFFSET);
		value = REG_VALUE_SET(value,
				has,
				PDP_HDECTRL_HDES_SHIFT,
				PDP_HDECTRL_HDES_MASK);
		value = REG_VALUE_SET(value,
				hrbs,
				PDP_HDECTRL_HDEF_SHIFT,
				PDP_HDECTRL_HDEF_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_HDECTRL_OFFSET, value);

		/* Vertical data enable */
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_VDECTRL_OFFSET);
		value = REG_VALUE_SET(value,
				vtbs, /* XXX: plato we're setting this to VAS */
				PDP_VDECTRL_VDES_SHIFT,
				PDP_VDECTRL_VDES_MASK);
		value = REG_VALUE_SET(value,
				vfps, /* XXX: plato set to VBBS */
				PDP_VDECTRL_VDEF_SHIFT,
				PDP_VDECTRL_VDEF_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VDECTRL_OFFSET, value);

		/* Vertical event start and vertical fetch start */
		/* XXX: Review this */
		if (pdp_crtc->reduced_blanking == true) {
			value = 0;
			value = REG_VALUE_SET(value,
				vbbs + PDP_REDUCED_BLANKING_VEVENT,
				PDP_VEVENT_VEVENT_SHIFT,
				PDP_VEVENT_VEVENT_MASK);
			value = REG_VALUE_SET(value,
				vbps / 2,
				PDP_VEVENT_VFETCH_SHIFT,
				PDP_VEVENT_VFETCH_MASK);
			plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VEVENT_OFFSET, value);
		} else {
			value = 0;
			value = REG_VALUE_SET(value,
				0,
				PDP_VEVENT_VEVENT_SHIFT,
				PDP_VEVENT_VEVENT_MASK);
			value = REG_VALUE_SET(value,
				vbps,
				PDP_VEVENT_VFETCH_SHIFT,
				PDP_VEVENT_VFETCH_MASK);
			plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VEVENT_OFFSET, value);
		}

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_VEVENT_OFFSET);
		value = REG_VALUE_SET(value,
				vbps,
				PDP_VEVENT_VFETCH_SHIFT,
				PDP_VEVENT_VFETCH_MASK);
		value = REG_VALUE_SET(value,
				vfps,
				PDP_VEVENT_VEVENT_SHIFT,
				PDP_VEVENT_VEVENT_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_VEVENT_OFFSET, value);

		/* Set up polarities of sync/blank */
		value = REG_VALUE_SET(0,
				0x1,
				PDP_SYNCCTRL_BLNKPOL_SHIFT,
				PDP_SYNCCTRL_BLNKPOL_MASK);

		if (adjusted_mode->flags & DRM_MODE_FLAG_NHSYNC)
			value = REG_VALUE_SET(value, 0x1,
				PDP_SYNCCTRL_HSPOL_SHIFT,
				PDP_SYNCCTRL_HSPOL_MASK);

		if (adjusted_mode->flags & DRM_MODE_FLAG_NVSYNC)
			value = REG_VALUE_SET(value, 0x1,
				PDP_SYNCCTRL_VSPOL_SHIFT,
				PDP_SYNCCTRL_VSPOL_MASK);

		plato_write_reg32(pdp_crtc->pdp_reg,
			PDP_SYNCCTRL_OFFSET,
			value);
		break;
	default:
		BUG();
	}
	return pdp_crtc_helper_mode_set_base(crtc, x, y, old_fb);
}

static void pdp_crtc_helper_load_lut(struct drm_crtc *crtc)
{
}

static void pdp_crtc_flip_complete(struct drm_crtc *crtc);

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

	kfree(pdp_crtc->primary_plane);
	kfree(pdp_crtc);
	dev_priv->crtc = NULL;
}

static void pdp_crtc_flip_complete(struct drm_crtc *crtc)
{
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	unsigned long flags;
	struct dma_fence *fence;

	spin_lock_irqsave(&crtc->dev->event_lock, flags);

	/* The flipping process has been completed so reset the flip status */
	atomic_set(&pdp_crtc->flip_status, PDP_CRTC_FLIP_STATUS_NONE);

	fence = pdp_crtc->flip_data->complete_fence;

	dma_fence_put(pdp_crtc->flip_data->wait_fence);
	kfree(pdp_crtc->flip_data);
	pdp_crtc->flip_data = NULL;

	if (pdp_crtc->flip_event) {
		drm_crtc_send_vblank_event(crtc, pdp_crtc->flip_event);
		pdp_crtc->flip_event = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

	WARN_ON(dma_fence_signal(fence));
	dma_fence_put(fence);
}

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
	struct pdp_drm_private *dev_priv = crtc->dev->dev_private;
	struct pdp_crtc *pdp_crtc = to_pdp_crtc(crtc);
	struct reservation_object *resv = pdp_gem_get_resv(obj);
	struct reservation_object *old_resv = pdp_gem_get_resv(old_obj);
	struct pdp_flip_data *flip_data;
	struct dma_fence *fence;
	int err;

	flip_data = kmalloc(sizeof(*flip_data), GFP_KERNEL);
	if (!flip_data)
		return -ENOMEM;

	flip_data->crtc = crtc;

	flip_data->complete_fence = pvr_sw_fence_create(dev_priv->dev_fctx);
	if (!flip_data->complete_fence) {
		err = -ENOMEM;
		goto err_free_fence_data;
	}

	ww_mutex_lock(&old_resv->lock, NULL);
	err = reservation_object_reserve_shared(old_resv);
	if (err) {
		ww_mutex_unlock(&old_resv->lock);
		goto err_complete_fence_put;
	}

	reservation_object_add_shared_fence(old_resv,
					    flip_data->complete_fence);

	flip_data->wait_fence =
		dma_fence_get(reservation_object_get_excl(old_resv));

	if (old_resv != resv) {
		ww_mutex_unlock(&old_resv->lock);
		ww_mutex_lock(&resv->lock, NULL);
	}

	fence = dma_fence_get(reservation_object_get_excl(resv));
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
err_complete_fence_put:
	dma_fence_put(flip_data->complete_fence);
err_free_fence_data:
	kfree(flip_data);
	return err;
}

static int pdp_crtc_page_flip(struct drm_crtc *crtc,
			      struct drm_framebuffer *fb,
			      struct drm_pending_vblank_event *event,
			      uint32_t page_flip_flags)
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

	err = pdp_crtc_flip_schedule(crtc, pdp_fb->obj, pdp_old_fb->obj);
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

static const struct drm_crtc_helper_funcs pdp_crtc_helper_funcs = {
	.dpms = pdp_crtc_helper_dpms,
	.prepare = pdp_crtc_helper_prepare,
	.commit = pdp_crtc_helper_commit,
	.mode_fixup = pdp_crtc_helper_mode_fixup,
	.mode_set = pdp_crtc_helper_mode_set,
	.mode_set_base = pdp_crtc_helper_mode_set_base,
	.load_lut = pdp_crtc_helper_load_lut,
	.mode_set_base_atomic = pdp_crtc_helper_mode_set_base_atomic,
	.disable = pdp_crtc_helper_disable,
};

static const struct drm_crtc_funcs pdp_crtc_funcs = {
	.reset = NULL,
	.cursor_set = NULL,
	.cursor_move = NULL,
	.gamma_set = NULL,
	.destroy = pdp_crtc_destroy,
	.set_config = drm_crtc_helper_set_config,
	.page_flip = pdp_crtc_page_flip,
};


struct drm_crtc *pdp_crtc_create(struct drm_device *dev, uint32_t number)
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

		regs = platform_get_resource_byname(dev->platformdev,
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
			regs = platform_get_resource_byname(dev->platformdev,
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
			regs = platform_get_resource_byname(dev->platformdev,
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
			regs = platform_get_resource_byname(dev->platformdev,
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

	if (dev_priv->version == PDP_VERSION_PLATO) {
		const uint32_t format = DRM_FORMAT_ARGB8888;

		pdp_crtc->primary_plane = kzalloc(
			sizeof(*pdp_crtc->primary_plane),
			GFP_KERNEL);
		if (pdp_crtc->primary_plane == NULL) {
			DRM_ERROR("Failed to allocate primary plane");
			err = -ENOMEM;
			goto err_iounmap_regs;
		}

		err = drm_universal_plane_init(dev, pdp_crtc->primary_plane,
					       0, &drm_primary_helper_funcs,
					       &format,
					       1, DRM_PLANE_TYPE_PRIMARY, NULL);
		if (err) {
			DRM_ERROR("Universal plane init failed!");
			goto err_free_primary_plane;
		}

		err = drm_crtc_init_with_planes(dev, &pdp_crtc->base,
			pdp_crtc->primary_plane, NULL, &pdp_crtc_funcs, NULL);
		if (err) {
			DRM_ERROR("CRTC init with planes failed");
			goto err_free_primary_plane;
		}
	} else {
		drm_crtc_init(dev, &pdp_crtc->base, &pdp_crtc_funcs);
	}

	drm_crtc_helper_add(&pdp_crtc->base, &pdp_crtc_helper_funcs);

	DRM_DEBUG_DRIVER("[CRTC:%d]\n", pdp_crtc->base.base.id);

	return &pdp_crtc->base;

err_free_primary_plane:
	kfree(pdp_crtc->primary_plane);
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
	uint32_t value;

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
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_INTCLR_OFFSET,
				0xFFFFFFFF);

		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_INTENAB_OFFSET);
		value = REG_VALUE_SET(value,
				enable ? 0x1 : 0x0,
				PDP_INTENAB_INTEN_VBLNK0_SHIFT,
				PDP_INTENAB_INTEN_VBLNK0_MASK);
		plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_INTENAB_OFFSET, value);
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
	uint32_t value;

	switch (dev_priv->version) {
	case PDP_VERSION_ODIN:
		if (pdp_odin_check_and_clear_vblank(
			crtc->dev->dev,
			pdp_crtc->pdp_reg)) {

			enum pdp_crtc_flip_status status;

			drm_handle_vblank(dev, pdp_crtc->number);

			status = atomic_read(&pdp_crtc->flip_status);
			if (status == PDP_CRTC_FLIP_STATUS_DONE) {
				if (!pdp_crtc->flip_async)
					pdp_crtc_flip_complete(crtc);
				drm_crtc_vblank_put(crtc);
			}
		}
		break;
	case PDP_VERSION_APOLLO:
		if (pdp_apollo_check_and_clear_vblank(
			crtc->dev->dev,
			pdp_crtc->pdp_reg)) {

			enum pdp_crtc_flip_status status;

			drm_handle_vblank(dev, pdp_crtc->number);

			status = atomic_read(&pdp_crtc->flip_status);
			if (status == PDP_CRTC_FLIP_STATUS_DONE) {
				if (!pdp_crtc->flip_async)
					pdp_crtc_flip_complete(crtc);
				drm_crtc_vblank_put(crtc);
			}
		}
		break;
	case PDP_VERSION_PLATO:
		value = plato_read_reg32(pdp_crtc->pdp_reg,
				PDP_INTSTAT_OFFSET);

		if (REG_VALUE_GET(value,
				PDP_INTSTAT_INTS_VBLNK0_SHIFT,
				PDP_INTSTAT_INTS_VBLNK0_MASK)) {
			enum pdp_crtc_flip_status status;

			plato_write_reg32(pdp_crtc->pdp_reg,
				PDP_INTCLR_OFFSET,
				(1 << PDP_INTCLR_INTCLR_VBLNK0_SHIFT));

			drm_handle_vblank(dev, pdp_crtc->number);

			status = atomic_read(&pdp_crtc->flip_status);
			if (status == PDP_CRTC_FLIP_STATUS_DONE) {
				if (!pdp_crtc->flip_async)
					pdp_crtc_flip_complete(crtc);
				drm_crtc_vblank_put(crtc);
			}
		}
		break;
	default:
		break;
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

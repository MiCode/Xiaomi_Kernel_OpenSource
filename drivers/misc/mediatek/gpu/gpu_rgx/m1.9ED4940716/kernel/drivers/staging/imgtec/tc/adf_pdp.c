/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File           adf_pdp.c
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

/*
 * This is an example ADF display driver for the testchip's PDP output
 */

/* #define SUPPORT_ADF_PDP_FBDEV */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/version.h>

#include <drm/drm_fourcc.h>

#include <video/adf.h>
#include <video/adf_client.h>

#ifdef SUPPORT_ADF_PDP_FBDEV
#include <video/adf_fbdev.h>
#endif

#include PVR_ANDROID_ION_HEADER

/* for sync_fence_put */
#include PVR_ANDROID_SYNC_HEADER

#include "tc_drv.h"
#include "adf_common.h"
#include "debugfs_dma_buf.h"

#include "pdp_odin.h"
#include "pdp_apollo.h"

#include "pvrmodule.h"

#define DRV_NAME "adf_pdp"

#ifndef ADF_PDP_WIDTH
#define ADF_PDP_WIDTH 1280
#endif

#ifndef ADF_PDP_HEIGHT
#define ADF_PDP_HEIGHT 720
#endif

MODULE_DESCRIPTION("TC PDP display driver");

static int pdp_display_width = ADF_PDP_WIDTH;
static int pdp_display_height = ADF_PDP_HEIGHT;
module_param(pdp_display_width, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(pdp_display_width, "PDP display width");
module_param(pdp_display_height, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(pdp_display_height, "PDP display height");

static DEFINE_SPINLOCK(gFlipLock);

struct pdp_timing_data {
	u32 h_display;
	u32 h_back_porch;
	u32 h_total;
	u32 h_active_start;
	u32 h_left_border;
	u32 h_right_border;
	u32 h_front_porch;

	u32 v_display;
	u32 v_back_porch;
	u32 v_total;
	u32 v_active_start;
	u32 v_top_border;
	u32 v_bottom_border;
	u32 v_front_porch;
	u32 v_refresh;
	u32 clock_freq;
};

struct pdp_format {
	u32 drm_format;
	u32 bytes_per_pixel;
	u32 pixfmt_word;
};

/* Apollo mode/format configuration: */

static const struct pdp_timing_data pdp_apollo_supported_modes[] = {
	{
		.h_display       = 640,
		.h_back_porch    = 64,
		.h_total         = 800,
		.h_active_start  = 144,
		.h_left_border   = 144,
		.h_right_border  = 784,
		.h_front_porch   = 784,

		.v_display       = 480,
		.v_back_porch    = 7,
		.v_total         = 497,
		.v_active_start  = 16,
		.v_top_border    = 16,
		.v_bottom_border = 496,
		.v_front_porch   = 496,

		.v_refresh       = 60,
		.clock_freq      = 23856000,
	},
	{
		.h_display       = 800,
		.h_back_porch    = 80,
		.h_total         = 1024,
		.h_active_start  = 192,
		.h_left_border   = 192,
		.h_right_border  = 992,
		.h_front_porch   = 992,

		.v_display       = 600,
		.v_back_porch    = 7,
		.v_total         = 621,
		.v_active_start  = 20,
		.v_top_border    = 20,
		.v_bottom_border = 620,
		.v_front_porch   = 620,

		.v_refresh       = 60,
		.clock_freq      = 38154000,
	},
	{
		.h_display       = 1024,
		.h_back_porch    = 104,
		.h_total         = 1344,
		.h_active_start  = 264,
		.h_left_border   = 264,
		.h_right_border  = 1288,
		.h_front_porch   = 1288,

		.v_display       = 768,
		.v_back_porch    = 7,
		.v_total         = 795,
		.v_active_start  = 26,
		.v_top_border    = 26,
		.v_bottom_border = 794,
		.v_front_porch   = 794,

		.v_refresh       = 59,
		.clock_freq      = 64108000,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 136,
		.h_total         = 1664,
		.h_active_start  = 328,
		.h_left_border   = 328,
		.h_right_border  = 1608,
		.h_front_porch   = 1608,

		.v_display       = 720,
		.v_back_porch    = 7,
		.v_total         = 745,
		.v_active_start  = 24,
		.v_top_border    = 24,
		.v_bottom_border = 744,
		.v_front_porch   = 744,

		.v_refresh       = 59,
		.clock_freq      = 74380000,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 136,
		.h_total         = 1680,
		.h_active_start  = 336,
		.h_left_border   = 336,
		.h_right_border  = 1616,
		.h_front_porch   = 1616,

		.v_display       = 768,
		.v_back_porch    = 7,
		.v_total         = 795,
		.v_active_start  = 26,
		.v_top_border    = 26,
		.v_bottom_border = 794,
		.v_front_porch   = 794,

		.v_refresh       = 59,
		.clock_freq      = 80136000,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 136,
		.h_total         = 1680,
		.h_active_start  = 336,
		.h_left_border   = 336,
		.h_right_border  = 1616,
		.h_front_porch   = 1616,

		.v_display       = 800,
		.v_back_porch    = 7,
		.v_total         = 828,
		.v_active_start  = 27,
		.v_top_border    = 27,
		.v_bottom_border = 827,
		.v_front_porch   = 827,

		.v_refresh       = 59,
		.clock_freq      = 83462000,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 136,
		.h_total         = 1712,
		.h_active_start  = 352,
		.h_left_border   = 352,
		.h_right_border  = 1632,
		.h_front_porch   = 1632,

		.v_display       = 1024,
		.v_back_porch    = 7,
		.v_total         = 1059,
		.v_active_start  = 34,
		.v_top_border    = 34,
		.v_bottom_border = 1058,
		.v_front_porch   = 1058,

		.v_refresh       = 60,
		.clock_freq      = 108780000,
	},
};
#define PDP_APOLLO_MODES_COUNT \
	(sizeof(pdp_apollo_supported_modes) / sizeof(struct pdp_timing_data))

static const u32 pdp_apollo_supported_formats[] = {
	DRM_FORMAT_BGRA8888,
};
#define PDP_APOLLO_FORMAT_COUNT \
	(sizeof(pdp_apollo_supported_formats) / sizeof(u32))

static const struct pdp_format pdp_apollo_format_table[] = {
	{ DRM_FORMAT_BGRA8888, 4, DCPDP_STR1SURF_FORMAT_ARGB8888 },
	{},
};

/* Odin mode/format configuration: */
static const struct pdp_timing_data pdp_odin_supported_modes[] = {
	{
		.h_display       = 640,
		.h_back_porch    = 96,
		.h_total         = 800,
		.h_active_start  = 144,
		.h_left_border   = 144,
		.h_right_border  = 784,
		.h_front_porch   = 784,

		.v_display       = 480,
		.v_back_porch    = 2,
		.v_total         = 525,
		.v_active_start  = 35,
		.v_top_border    = 35,
		.v_bottom_border = 515,
		.v_front_porch   = 515,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 800,
		.h_back_porch    = 128,
		.h_total         = 1056,
		.h_active_start  = 216,
		.h_left_border   = 216,
		.h_right_border  = 1016,
		.h_front_porch   = 1016,

		.v_display       = 600,
		.v_back_porch    = 4,
		.v_total         = 628,
		.v_active_start  = 27,
		.v_top_border    = 27,
		.v_bottom_border = 627,
		.v_front_porch   = 627,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 1024,
		.h_back_porch    = 136,
		.h_total         = 1344,
		.h_active_start  = 296,
		.h_left_border   = 296,
		.h_right_border  = 1320,
		.h_front_porch   = 1320,

		.v_display       = 768,
		.v_back_porch    = 6,
		.v_total         = 806,
		.v_active_start  = 35,
		.v_top_border    = 35,
		.v_bottom_border = 803,
		.v_front_porch   = 803,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 112,
		.h_total         = 1800,
		.h_active_start  = 424,
		.h_left_border   = 424,
		.h_right_border  = 1704,
		.h_front_porch   = 1704,

		.v_display       = 960,
		.v_back_porch    = 3,
		.v_total         = 1000,
		.v_active_start  = 39,
		.v_top_border    = 39,
		.v_bottom_border = 999,
		.v_front_porch   = 999,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 1440,
		.h_back_porch    = 152,
		.h_total         = 1904,
		.h_active_start  = 384,
		.h_left_border   = 384,
		.h_right_border  = 1824,
		.h_front_porch   = 1824,

		.v_display       = 900,
		.v_back_porch    = 6,
		.v_total         = 934,
		.v_active_start  = 31,
		.v_top_border    = 31,
		.v_bottom_border = 931,
		.v_front_porch   = 931,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 112,
		.h_total         = 1688,
		.h_active_start  = 360,
		.h_left_border   = 360,
		.h_right_border  = 1640,
		.h_front_porch   = 1640,

		.v_display       = 1024,
		.v_back_porch    = 3,
		.v_total         = 1066,
		.v_active_start  = 41,
		.v_top_border    = 41,
		.v_bottom_border = 1065,
		.v_front_porch   = 1065,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 1280,
		.h_back_porch    = 40,
		.h_total         = 1650,
		.h_active_start  = 260,
		.h_left_border   = 260,
		.h_right_border  = 1540,
		.h_front_porch   = 1540,

		.v_display       = 720,
		.v_back_porch    = 5,
		.v_total         = 750,
		.v_active_start  = 25,
		.v_top_border    = 25,
		.v_bottom_border = 745,
		.v_front_porch   = 745,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
	{
		.h_display       = 1920,
		.h_back_porch    = 44,
		.h_total         = 2200,
		.h_active_start  = 192,
		.h_left_border   = 192,
		.h_right_border  = 2112,
		.h_front_porch   = 2112,

		.v_display       = 1080,
		.v_back_porch    = 5,
		.v_total         = 1125,
		.v_active_start  = 41,
		.v_top_border    = 41,
		.v_bottom_border = 1121,
		.v_front_porch   = 1121,

		.v_refresh       = 60,
		.clock_freq      = 0,
	},
};
#define PDP_ODIN_MODES_COUNT \
	(sizeof(pdp_odin_supported_modes) / sizeof(struct pdp_timing_data))

static const u32 pdp_odin_supported_formats[] = {
	DRM_FORMAT_BGRA8888,
};
#define  PDP_ODIN_FORMAT_COUNT \
	(sizeof(pdp_odin_supported_formats) / sizeof(u32))

static const struct pdp_format pdp_odin_format_table[] = {
	{ DRM_FORMAT_BGRA8888, 4, ODN_PDP_SURF_PIXFMT_ARGB8888 },
	{},
};

/* Globals based on TC version */
static const struct pdp_timing_data *pdp_supported_modes;
static u32 pdp_supported_modes_count;
static const u32 *pdp_supported_formats;
static const struct pdp_format *pdp_format_table;

/* Callbacks into the TC specific PDP code: */
static void (*pdp_set_updates_enabled)(struct device *dev,
				void __iomem *pdp_reg, bool enable);
static bool (*pdp_clocks_set)(struct device *dev,
				void __iomem *pdp_reg, void __iomem *pll_reg,
				u32 clock_freq,
				void __iomem *odn_core_reg,
				u32 hdisplay, u32 vdisplay);
static void (*pdp_set_syncgen_enabled)(struct device *dev,
				void __iomem *pdp_reg, bool enable);
static void (*pdp_set_powerdwn_enabled)(struct device *dev,
				void __iomem *pdp_reg, bool enable);
static void (*pdp_set_vblank_enabled)(struct device *dev,
				void __iomem *pdp_reg, bool enable);
static bool (*pdp_check_and_clear_vblank)(struct device *dev,
				void __iomem *pdp_reg);
static void (*pdp_reset_planes)(struct device *dev,
				void __iomem *pdp_reg);
static void (*pdp_set_plane_enabled)(struct device *dev,
				void __iomem *pdp_reg,
				u32 plane, bool enable);
static void (*pdp_set_surface)(struct device *dev,
				void __iomem *pdp_reg, u32 address,
				u32 plane,
				u32 posx, u32 posy,
				u32 width, u32 height, u32 stride,
				u32 format,
				u32 alpha,
				bool blend);
static void (*pdp_mode_set)(struct device *dev,
				void __iomem *pdp_reg,
				u32 h_display, u32 v_display,
				u32 hbps, u32 ht, u32 has,
				u32 hlbs, u32 hfps, u32 hrbs,
				u32 vbps, u32 vt, u32 vas,
				u32 vtbs, u32 vfps, u32 vbbs,
				bool nhsync, bool nvsync);

struct adf_pdp_device {
	struct ion_client *ion_client;

	struct adf_device adf_device;
	struct adf_interface adf_interface;
	struct adf_overlay_engine adf_overlay;
#ifdef SUPPORT_ADF_PDP_FBDEV
	struct adf_fbdev adf_fbdev;
#endif

	struct platform_device *pdev;

	enum pdp_version version;

	struct tc_pdp_platform_data *pdata;

	void __iomem *regs;
	resource_size_t regs_size;

	void __iomem *pll_regs;
	resource_size_t pll_regs_size;

	void __iomem *odin_core_regs;
	resource_size_t odin_core_regs_size;

	struct drm_mode_modeinfo *supported_modes;
	int num_supported_modes;

	const struct pdp_timing_data *current_timings;

	atomic_t refcount;

	atomic_t num_validates;
	int num_posts;

	atomic_t vsync_triggered;
	wait_queue_head_t vsync_wait_queue;
	atomic_t requested_vsync_state;
	atomic_t vsync_state;

	/* This is set when the last client has released this device, causing
	 * all outstanding posts to be ignored
	 */
	atomic_t released;

	struct {
		dma_addr_t address;
		u32 posx;
		u32 posy;
		u32 width;
		u32 height;
		u32 stride;
		u32 format;
		u32 alpha;
		bool blend;
	} flip_params;
};

static int pdp_mode_id(struct adf_pdp_device *pdp, u32 height, u32 width)
{
	int i;

	for (i = 0; i < pdp_supported_modes_count; i++) {
		const struct pdp_timing_data *tdata = &pdp_supported_modes[i];

		if (tdata->h_display == width && tdata->v_display == height)
			return i;
	}

	dev_err(&pdp->pdev->dev, "Failed to find matching mode for %dx%d\n",
		width, height);

	return -1;
}

static const struct pdp_timing_data *pdp_timing_data(
	struct adf_pdp_device *pdp, int mode_id)
{
	if (mode_id >= pdp_supported_modes_count || mode_id < 0)
		return NULL;

	return &pdp_supported_modes[mode_id];
}

static void pdp_mode_to_drm_mode(struct adf_pdp_device *pdp, int mode_id,
	struct drm_mode_modeinfo *drm_mode)
{
	const struct pdp_timing_data *pdp_mode = pdp_timing_data(pdp, mode_id);

	BUG_ON(pdp_mode == NULL);
	memset(drm_mode, 0, sizeof(*drm_mode));

	drm_mode->hdisplay = pdp_mode->h_display;
	drm_mode->vdisplay = pdp_mode->v_display;
	drm_mode->vrefresh = pdp_mode->v_refresh;

	adf_modeinfo_set_name(drm_mode);
}

static void pdp_devres_release(struct device *dev, void *res)
{
	/* No extra cleanup needed */
}

static const struct pdp_format *pdp_format(u32 drm_format)
{
	int i;

	for (i = 0; pdp_format_table[i].drm_format != 0; i++) {
		if (pdp_format_table[i].drm_format == drm_format)
			return &pdp_format_table[i];
	}
	BUG();
	return NULL;
}

static bool pdp_vsync_triggered(struct adf_pdp_device *pdp)
{
	return atomic_read(&pdp->vsync_triggered) == 1;
}

static void pdp_enable_vsync(struct adf_pdp_device *pdp)
{
	pdp_set_vblank_enabled(&pdp->pdev->dev,
			       pdp->regs, true);
}

static void pdp_disable_vsync(struct adf_pdp_device *pdp)
{
	pdp_set_vblank_enabled(&pdp->pdev->dev,
			       pdp->regs, false);
}

static void pdp_enable_interrupt(struct adf_pdp_device *pdp)
{
	int err = 0;

	err = tc_enable_interrupt(pdp->pdev->dev.parent,
		TC_INTERRUPT_PDP);
	if (err) {
		dev_err(&pdp->pdev->dev,
			"tc_enable_interrupt failed (%d)\n", err);
	}
}

static void pdp_disable_interrupt(struct adf_pdp_device *pdp)
{
	int err = 0;

	err = tc_disable_interrupt(pdp->pdev->dev.parent,
		TC_INTERRUPT_PDP);
	if (err) {
		dev_err(&pdp->pdev->dev,
			"tc_disable_interrupt failed (%d)\n", err);
	}
}

static void pdp_post(struct adf_device *adf_dev, struct adf_post *cfg,
	void *driver_state)
{
	int num_validates_snapshot = *(int *)driver_state;
	dma_addr_t buf_addr;
	unsigned long flags;

	/* Set vsync wait timeout to 4x expected vsync */
	struct adf_pdp_device *pdp = devres_find(adf_dev->dev,
		pdp_devres_release, NULL, NULL);
	long timeout =
		msecs_to_jiffies((1000 / pdp->current_timings->v_refresh) * 4);

	/* Null-flip handling, used to push buffers off screen during an error
	 * state to stop them blocking subsequent rendering
	 */
	if (cfg->n_bufs == 0 || atomic_read(&pdp->released) == 1)
		goto out_update_num_posts;

	WARN_ON(cfg->n_bufs != 1);
	WARN_ON(cfg->mappings->sg_tables[0]->nents != 1);

	spin_lock_irqsave(&gFlipLock, flags);

	buf_addr = sg_phys(cfg->mappings->sg_tables[0]->sgl);

	debugfs_dma_buf_set(cfg->bufs[0].dma_bufs[0]);

	pdp->flip_params.posx = 0;
	pdp->flip_params.posy = 0;
	pdp->flip_params.alpha = 255;
	pdp->flip_params.blend = false;
	pdp->flip_params.width = cfg->bufs[0].w;
	pdp->flip_params.height = cfg->bufs[0].h;
	pdp->flip_params.stride = cfg->bufs[0].pitch[0];
	pdp->flip_params.format = pdp_format(cfg->bufs[0].format)->pixfmt_word;
	pdp->flip_params.address = buf_addr;

	atomic_set(&pdp->vsync_triggered, 0);

	spin_unlock_irqrestore(&gFlipLock, flags);

	/* Wait until the buffer is on-screen, so we know the previous buffer
	 * has been retired and off-screen.
	 *
	 * If vsync was already off when this post was serviced, we need to
	 * enable the vsync again briefly so the register updates we shadowed
	 * above get applied and we don't signal the fence prematurely. One
	 * vsync afterwards, we'll disable the vsync again.
	 */
	if (!atomic_xchg(&pdp->vsync_state, 1))
		pdp_enable_vsync(pdp);

	if (wait_event_timeout(pdp->vsync_wait_queue,
		pdp_vsync_triggered(pdp), timeout) == 0) {
		dev_err(&pdp->pdev->dev, "Post VSync wait timeout");
		/* Undefined behaviour if this times out */
	}
out_update_num_posts:
	pdp->num_posts = num_validates_snapshot;
}

static bool pdp_supports_event(struct adf_obj *obj, enum adf_event_type type)
{
	switch (obj->type) {
	case ADF_OBJ_INTERFACE:
	{
		switch (type) {
		case ADF_EVENT_VSYNC:
			return true;
		default:
			return false;
		}
	}
	default:
		return false;
	}
}

static void pdp_irq_handler(void *data)
{
	struct adf_pdp_device *pdp = data;
	unsigned long flags;
	bool int_status;

	int_status = pdp_check_and_clear_vblank(&pdp->pdev->dev,
						pdp->regs);
	spin_lock_irqsave(&gFlipLock, flags);

	/* If we're idle, and a vsync disable was requested, do it now.
	 * This code assumes that the HWC will always re-enable vsync
	 * explicitly before posting new configurations.
	 */
	if (atomic_read(&pdp->num_validates) == pdp->num_posts) {
		if (!atomic_read(&pdp->requested_vsync_state)) {
			pdp_disable_vsync(pdp);
			atomic_set(&pdp->vsync_state, 0);
		}
	}

	if (int_status) {
		pdp_set_updates_enabled(&pdp->pdev->dev,
					pdp->regs, false);
		pdp_set_surface(&pdp->pdev->dev,
				pdp->regs,
				0,
				pdp->flip_params.address,
				pdp->flip_params.posx,
				pdp->flip_params.posy,
				pdp->flip_params.width,
				pdp->flip_params.height,
				pdp->flip_params.stride,
				pdp->flip_params.format,
				pdp->flip_params.alpha,
				pdp->flip_params.blend);
		pdp_set_plane_enabled(&pdp->pdev->dev,
				      pdp->regs, 0, true);
		pdp_set_updates_enabled(&pdp->pdev->dev,
					pdp->regs, true);

		adf_vsync_notify(&pdp->adf_interface, ktime_get());
		atomic_set(&pdp->vsync_triggered, 1);
		wake_up(&pdp->vsync_wait_queue);
	}

	spin_unlock_irqrestore(&gFlipLock, flags);
}

static void pdp_set_event(struct adf_obj *obj, enum adf_event_type type,
	bool enabled)
{
	struct adf_pdp_device *pdp;
	bool old;

	switch (type) {
	case ADF_EVENT_VSYNC:
	{
		pdp = devres_find(obj->parent->dev, pdp_devres_release,
				  NULL, NULL);
		atomic_set(&pdp->requested_vsync_state, enabled);
		if (enabled) {
			old = atomic_xchg(&pdp->vsync_state, enabled);
			if (!old)
				pdp_enable_vsync(pdp);
		}
		break;
	}
	default:
		BUG();
	}
}

static int pdp_modeset(struct adf_interface *intf,
	struct drm_mode_modeinfo *mode)
{
	int err = 0;
	struct adf_pdp_device *pdp = devres_find(intf->base.parent->dev,
		pdp_devres_release, NULL, NULL);
	int mode_id = pdp_mode_id(pdp, mode->vdisplay, mode->hdisplay);
	const struct pdp_timing_data *tdata = pdp_timing_data(pdp, mode_id);

	if (!tdata) {
		dev_err(&pdp->pdev->dev, "Failed to find mode for %ux%u\n",
			mode->hdisplay, mode->vdisplay);
		err = -ENXIO;
		goto err_out;
	}
	/* Disable any register reading while updating */
	pdp_set_updates_enabled(&pdp->pdev->dev, pdp->regs, false);

	/* Reset all planes */
	pdp_reset_planes(&pdp->pdev->dev, pdp->regs);
	/* Disable sync gen */
	pdp_set_syncgen_enabled(&pdp->pdev->dev,
				pdp->regs, false);
	/* Set the clocks and mode */
	pdp_clocks_set(&pdp->pdev->dev,
		       pdp->regs, pdp->pll_regs,
		       (tdata->clock_freq + 500000) / 1000000, /* apollo only */
		       pdp->odin_core_regs,                    /* odin only */
		       tdata->h_display, tdata->v_display);
	pdp_mode_set(&pdp->pdev->dev,
		     pdp->regs,
		     tdata->h_display, tdata->v_display,
		     tdata->h_back_porch, tdata->h_total,
		     tdata->h_active_start, tdata->h_left_border,
		     tdata->h_front_porch, tdata->h_right_border,
		     tdata->v_back_porch, tdata->v_total,
		     tdata->v_active_start, tdata->v_top_border,
		     tdata->v_front_porch, tdata->v_bottom_border,
		     true, true);
	/* Re-enable sync gen */
	pdp_set_syncgen_enabled(&pdp->pdev->dev,
				pdp->regs, true);

	/* Enable register reading */
	pdp_set_updates_enabled(&pdp->pdev->dev, pdp->regs, true);

	intf->current_mode = *mode;
	pdp->current_timings = tdata;

err_out:
	return err;
}

static int pdp_blank(struct adf_interface *intf,
	u8 state)
{
	struct adf_pdp_device *pdp = devres_find(intf->base.parent->dev,
		pdp_devres_release, NULL, NULL);

	if (state != DRM_MODE_DPMS_OFF &&
		state != DRM_MODE_DPMS_ON)
		return -EINVAL;

	pdp_set_powerdwn_enabled(&pdp->pdev->dev,
				pdp->regs,
				state == DRM_MODE_DPMS_OFF ? true : false);

	return 0;
}

static int pdp_alloc_simple_buffer(struct adf_interface *intf, u16 w, u16 h,
	u32 format, struct dma_buf **dma_buf, u32 *offset, u32 *pitch)
{
	struct adf_pdp_device *pdp = devres_find(intf->base.parent->dev,
		pdp_devres_release, NULL, NULL);
	int err = 0;
	const struct pdp_format *pformat = pdp_format(format);
	u32 size = w * h * pformat->bytes_per_pixel;
	struct ion_handle *hdl = ion_alloc(pdp->ion_client, size, 0,
		(1 << pdp->pdata->ion_heap_id), 0);
	if (IS_ERR(hdl)) {
		err = PTR_ERR(hdl);
		dev_err(&pdp->pdev->dev, "ion_alloc failed (%d)\n", err);
		goto err_out;
	}
	*dma_buf = ion_share_dma_buf(pdp->ion_client, hdl);
	if (IS_ERR(*dma_buf)) {
		err = PTR_ERR(hdl);
		dev_err(&pdp->pdev->dev,
			"ion_share_dma_buf failed (%d)\n", err);
		goto err_free_buffer;
	}
	*pitch = w * pformat->bytes_per_pixel;
	*offset = 0;
err_free_buffer:
	ion_free(pdp->ion_client, hdl);
err_out:
	return err;
}

static int pdp_describe_simple_post(struct adf_interface *intf,
	struct adf_buffer *fb, void *data, size_t *size)
{
	struct adf_post_ext *post_ext = data;
	static int post_id;

	struct drm_clip_rect full_screen = {
		.x2 = ADF_PDP_WIDTH,
		.y2 = ADF_PDP_HEIGHT,
	};

	/* NOTE: an upstream ADF bug means we can't test *size instead */
	BUG_ON(sizeof(struct adf_post_ext) +
		1 * sizeof(struct adf_buffer_config_ext)
			> ADF_MAX_CUSTOM_DATA_SIZE);

	*size = sizeof(struct adf_post_ext) +
		1 * sizeof(struct adf_buffer_config_ext);

	post_ext->post_id = ++post_id;

	post_ext->bufs_ext[0].crop        = full_screen;
	post_ext->bufs_ext[0].display     = full_screen;
	post_ext->bufs_ext[0].transform   = ADF_BUFFER_TRANSFORM_NONE_EXT;
	post_ext->bufs_ext[0].blend_type  = ADF_BUFFER_BLENDING_PREMULT_EXT;
	post_ext->bufs_ext[0].plane_alpha = 0xff;

	return 0;
}

static int
adf_pdp_open(struct adf_obj *obj, struct inode *inode, struct file *file)
{
	struct adf_device *dev =
		(struct adf_device *)obj->parent;
	struct adf_pdp_device *pdp = devres_find(dev->dev,
		pdp_devres_release, NULL, NULL);
	atomic_inc(&pdp->refcount);
	atomic_set(&pdp->released, 0);
	return 0;
}

static void
adf_pdp_release(struct adf_obj *obj, struct inode *inode, struct file *file)
{
	struct adf_device *dev =
		(struct adf_device *)obj->parent;
	struct adf_pdp_device *pdp = devres_find(dev->dev,
		pdp_devres_release, NULL, NULL);
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 0))
	struct sync_fence *release_fence;
#else
	struct fence *release_fence;
#endif

	if (atomic_dec_return(&pdp->refcount))
		return;

	/* Make sure we have no outstanding posts waiting */
	atomic_set(&pdp->released, 1);
	atomic_set(&pdp->requested_vsync_state, 0);
	atomic_set(&pdp->vsync_triggered, 1);
	wake_up_all(&pdp->vsync_wait_queue);
	/* This special "null" flip works around a problem with ADF
	 * which leaves buffers pinned by the display engine even
	 * after all ADF clients have closed.
	 *
	 * The "null" flip is pipelined like any other. The user won't
	 * be able to unload this module until it has been posted.
	 */
	release_fence = adf_device_post(dev, NULL, 0, NULL, 0, NULL, 0);
	if (IS_ERR_OR_NULL(release_fence)) {
		dev_err(dev->dev,
			"Failed to queue null flip command (err=%d).\n",
			(int)PTR_ERR(release_fence));
		return;
	}

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 5, 0))
	sync_fence_put(release_fence);
#else
	fence_put(release_fence);
#endif
}

static int pdp_validate(struct adf_device *dev, struct adf_post *cfg,
	void **driver_state)
{
	struct adf_pdp_device *pdp = devres_find(dev->dev,
		pdp_devres_release, NULL, NULL);
	int err = adf_img_validate_simple(dev, cfg, driver_state);

	if (err == 0 && cfg->mappings) {
		/* We store a snapshot of num_validates in driver_state at the
		 * time validate was called, which will be passed to the post
		 * function. This snapshot is copied into (i.e. overwrites)
		 * num_posts, rather than simply incrementing num_posts, to
		 * handle cases e.g. during fence timeouts where validates
		 * are called without corresponding posts.
		 */
		int *validates = kmalloc(sizeof(*validates), GFP_KERNEL);
		*validates = atomic_inc_return(&pdp->num_validates);
		*driver_state = validates;
	} else {
		*driver_state = NULL;
	}
	return err;
}

static void pdp_state_free(struct adf_device *dev, void *driver_state)
{
	kfree(driver_state);
}

static struct adf_device_ops adf_pdp_device_ops = {
	.owner = THIS_MODULE,
	.base = {
		.open = adf_pdp_open,
		.release = adf_pdp_release,
		.ioctl = adf_img_ioctl,
	},
	.state_free = pdp_state_free,
	.validate = pdp_validate,
	.post = pdp_post,
};

static struct adf_interface_ops adf_pdp_interface_ops = {
	.base = {
		.supports_event = pdp_supports_event,
		.set_event = pdp_set_event,
	},
	.modeset = pdp_modeset,
	.blank = pdp_blank,
	.alloc_simple_buffer = pdp_alloc_simple_buffer,
	.describe_simple_post = pdp_describe_simple_post,
};

static struct adf_overlay_engine_ops adf_pdp_apollo_overlay_ops = {
	.supported_formats = &pdp_apollo_supported_formats[0],
	.n_supported_formats = PDP_APOLLO_FORMAT_COUNT,
};

static struct adf_overlay_engine_ops adf_pdp_odin_overlay_ops = {
	.supported_formats = &pdp_odin_supported_formats[0],
	.n_supported_formats = PDP_ODIN_FORMAT_COUNT,
};

static struct adf_overlay_engine_ops *adf_pdp_overlay_ops;

#ifdef SUPPORT_ADF_PDP_FBDEV
static struct fb_ops adf_pdp_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = adf_fbdev_open,
	.fb_release = adf_fbdev_release,
	.fb_check_var = adf_fbdev_check_var,
	.fb_set_par = adf_fbdev_set_par,
	.fb_blank = adf_fbdev_blank,
	.fb_pan_display = adf_fbdev_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_mmap = adf_fbdev_mmap,
};
#endif

static int adf_pdp_probe_device(struct platform_device *pdev)
{
	struct adf_pdp_device *pdp;
	int err = 0;
	int i, default_mode_id;
	struct resource *registers;
	struct pci_dev *pci_dev = to_pci_dev(pdev->dev.parent);
	struct tc_pdp_platform_data *pdata = pdev->dev.platform_data;

	pdp = devres_alloc(pdp_devres_release, sizeof(*pdp),
		GFP_KERNEL);
	if (!pdp) {
		err = -ENOMEM;
		goto err_out;
	}
	devres_add(&pdev->dev, pdp);

	pdp->pdata = pdata;
	pdp->pdev = pdev;
	pdp->version = (enum pdp_version) pdp->pdev->id_entry->driver_data;

	err = tc_enable(pdp->pdev->dev.parent);
	if (err) {
		dev_err(&pdev->dev,
			"Failed to enable PDP pci device (%d)\n", err);
		goto err_out;
	}

	atomic_set(&pdp->refcount, 0);
	atomic_set(&pdp->num_validates, 0);
	pdp->num_posts = 0;

	pdp->ion_client = ion_client_create(pdata->ion_device, "adf_pdp");
	if (IS_ERR(pdp->ion_client)) {
		err = PTR_ERR(pdp->ion_client);
		dev_err(&pdev->dev,
			"Failed to create PDP ION client (%d)\n", err);
		goto err_disable_pci;
	}

	registers = platform_get_resource_byname(pdev,
						 IORESOURCE_MEM,
						 "pdp-regs");
	pdp->regs = devm_ioremap_resource(&pdev->dev, registers);
	if (IS_ERR(pdp->regs)) {
		err = PTR_ERR(pdp->regs);
		dev_err(&pdev->dev, "Failed to map PDP registers (%d)\n", err);
		goto err_destroy_ion_client;
	}
	pdp->regs_size = resource_size(registers);

	/* Init some global state based on the TC version */
	switch (pdp->version) {
	case PDP_VERSION_APOLLO: {
		/* Set the global state to apollo */
		pdp_supported_modes = pdp_apollo_supported_modes;
		pdp_supported_modes_count = PDP_APOLLO_MODES_COUNT;
		pdp_supported_formats = pdp_apollo_supported_formats;
		pdp_format_table = pdp_apollo_format_table;
		adf_pdp_overlay_ops = &adf_pdp_apollo_overlay_ops;
		/* Function callbacks */
		pdp_set_updates_enabled = &pdp_apollo_set_updates_enabled;
		pdp_clocks_set = &pdp_apollo_clocks_set;
		pdp_set_syncgen_enabled = &pdp_apollo_set_syncgen_enabled;
		pdp_set_powerdwn_enabled = &pdp_apollo_set_powerdwn_enabled;
		pdp_set_vblank_enabled = &pdp_apollo_set_vblank_enabled;
		pdp_check_and_clear_vblank = &pdp_apollo_check_and_clear_vblank;
		pdp_reset_planes = &pdp_apollo_reset_planes;
		pdp_set_plane_enabled = &pdp_apollo_set_plane_enabled;
		pdp_set_surface = &pdp_apollo_set_surface;
		pdp_mode_set = &pdp_apollo_mode_set;
		/* Get the pll regs */
		registers = platform_get_resource_byname(pdev,
					IORESOURCE_MEM,
					"pll-regs");
		pdp->pll_regs =
		    devm_ioremap_resource(&pdev->dev, registers);
		if (IS_ERR(pdp->pll_regs)) {
			err = PTR_ERR(pdp->pll_regs);
			dev_err(&pdev->dev,
				"Failed to map PLL registers (%d)\n",
				err);
			goto err_destroy_ion_client;
		}

		break;
	}
	case PDP_VERSION_ODIN: {
		/* Set the global state to odin */
		pdp_supported_modes = pdp_odin_supported_modes;
		pdp_supported_modes_count = PDP_ODIN_MODES_COUNT;
		pdp_supported_formats = pdp_odin_supported_formats;
		pdp_format_table = pdp_odin_format_table;
		adf_pdp_overlay_ops = &adf_pdp_odin_overlay_ops;
		/* Function callbacks */
		pdp_set_updates_enabled = &pdp_odin_set_updates_enabled;
		pdp_clocks_set = &pdp_odin_clocks_set;
		pdp_set_syncgen_enabled = &pdp_odin_set_syncgen_enabled;
		pdp_set_powerdwn_enabled = &pdp_odin_set_powerdwn_enabled;
		pdp_set_vblank_enabled = &pdp_odin_set_vblank_enabled;
		pdp_check_and_clear_vblank = &pdp_odin_check_and_clear_vblank;
		pdp_reset_planes = &pdp_odin_reset_planes;
		pdp_set_plane_enabled = &pdp_odin_set_plane_enabled;
		pdp_set_surface = &pdp_odin_set_surface;
		pdp_mode_set = &pdp_odin_mode_set;
		/* Get the pll and odin-core regs. We can't use
		 * devm_ioremap_resource cause on odin bar 0 is
		 * completely managed by 'pdp-regs'. We should probably
		 * fix this by removing this special mapping all
		 * together on both apollo/odin.
		 */
		registers = platform_get_resource_byname(pdev,
				IORESOURCE_MEM,
				"pll-regs");
		pdp->pll_regs =
		    ioremap_nocache(registers->start,
				    resource_size(registers));
		if (!pdp->pll_regs) {
			err = -ENOMEM;
			dev_err(&pdev->dev,
				"Failed to map PLL registers (%d)\n",
				err);
			goto err_destroy_ion_client;
		}
		pdp->pll_regs_size = resource_size(registers);

		registers = platform_get_resource_byname(pdev,
				IORESOURCE_MEM,
				"odn-core");

		pdp->odin_core_regs = ioremap_nocache(registers->start,
					resource_size(registers));
		if (!pdp->odin_core_regs) {
			err = -ENOMEM;
			dev_err(&pdev->dev, "Failed to map odin-core registers (%d)\n",
				err);
			goto err_destroy_ion_client;
		}

		pdp->odin_core_regs_size = resource_size(registers);

		break;
	}
	default:
		BUG(); break;
	}

	err = adf_device_init(&pdp->adf_device, &pdp->pdev->dev,
		&adf_pdp_device_ops, "pdp_device");
	if (err) {
		dev_err(&pdev->dev, "Failed to init ADF device (%d)\n", err);
		goto err_destroy_ion_client;
	}

	err = adf_interface_init(&pdp->adf_interface, &pdp->adf_device,
		ADF_INTF_DVI, 0, ADF_INTF_FLAG_PRIMARY, &adf_pdp_interface_ops,
		"pdp_interface");
	if (err) {
		dev_err(&pdev->dev, "Failed to init ADF interface (%d)\n", err);
		goto err_destroy_adf_device;
	}

	err = adf_overlay_engine_init(&pdp->adf_overlay, &pdp->adf_device,
		adf_pdp_overlay_ops, "pdp_overlay");
	if (err) {
		dev_err(&pdev->dev, "Failed to init ADF overlay (%d)\n", err);
		goto err_destroy_adf_interface;
	}

	err = adf_attachment_allow(&pdp->adf_device, &pdp->adf_overlay,
		&pdp->adf_interface);
	if (err) {
		dev_err(&pdev->dev, "Failed to attach overlay (%d)\n", err);
		goto err_destroy_adf_overlay;
	}

	pdp->num_supported_modes = pdp_supported_modes_count;
	pdp->supported_modes = kzalloc(sizeof(*pdp->supported_modes)
		* pdp->num_supported_modes, GFP_KERNEL);

	if (!pdp->supported_modes) {
		dev_err(&pdev->dev, "Failed to allocate supported modeinfo structs\n");
		err = -ENOMEM;
		goto err_destroy_adf_overlay;
	}

	for (i = 0; i < pdp->num_supported_modes; i++)
		pdp_mode_to_drm_mode(pdp, i, &pdp->supported_modes[i]);

	default_mode_id = pdp_mode_id(pdp, pdp_display_height,
		pdp_display_width);
	if (default_mode_id == -1) {
		default_mode_id = 0;
		dev_err(&pdev->dev, "No modeline found for requested display size (%dx%d)\n",
			pdp_display_width, pdp_display_height);
	}

	/* Initial modeset... */
	err = pdp_modeset(&pdp->adf_interface,
		&pdp->supported_modes[default_mode_id]);
	if (err) {
		dev_err(&pdev->dev, "Initial modeset failed (%d)\n", err);
		goto err_destroy_modelist;
	}

	err = adf_hotplug_notify_connected(&pdp->adf_interface,
		pdp->supported_modes, pdp->num_supported_modes);
	if (err) {
		dev_err(&pdev->dev, "Initial hotplug notify failed (%d)\n",
			err);
		goto err_destroy_modelist;
	}
	err = tc_set_interrupt_handler(pdp->pdev->dev.parent,
					   TC_INTERRUPT_PDP,
					   pdp_irq_handler, pdp);
	if (err) {
		dev_err(&pdev->dev, "Failed to set interrupt handler (%d)\n",
			err);
		goto err_destroy_modelist;
	}

	init_waitqueue_head(&pdp->vsync_wait_queue);
	atomic_set(&pdp->requested_vsync_state, 0);
	atomic_set(&pdp->vsync_state, 0);

	if (debugfs_dma_buf_init("pdp_raw"))
		dev_err(&pdev->dev, "Failed to create debug fs file for raw access\n");

	pdp_enable_interrupt(pdp);

#ifdef SUPPORT_ADF_PDP_FBDEV
	err = adf_fbdev_init(&pdp->adf_fbdev, &pdp->adf_interface,
		&pdp->adf_overlay, pdp_display_width,
		pdp_display_height, DRM_FORMAT_BGRA8888,
		&adf_pdp_fb_ops, "adf_pdp_fb");
	if (err) {
		dev_err(&pdev->dev, "Failed to init ADF fbdev (%d)\n", err);
		goto err_destroy_modelist;
	}
#endif

	return err;
err_destroy_modelist:
	kfree(pdp->supported_modes);
err_destroy_adf_overlay:
	adf_overlay_engine_destroy(&pdp->adf_overlay);
err_destroy_adf_interface:
	adf_interface_destroy(&pdp->adf_interface);
err_destroy_adf_device:
	adf_device_destroy(&pdp->adf_device);
err_destroy_ion_client:
	ion_client_destroy(pdp->ion_client);
err_disable_pci:
	pci_disable_device(pci_dev);
err_out:
	dev_err(&pdev->dev, "Failed to initialise PDP device\n");
	return err;
}

static int adf_pdp_remove_device(struct platform_device *pdev)
{
	int err = 0;
	struct pci_dev *pci_dev = to_pci_dev(pdev->dev.parent);
	struct adf_pdp_device *pdp = devres_find(&pdev->dev, pdp_devres_release,
		NULL, NULL);

	debugfs_dma_buf_deinit();

	/* Reset all streams */
	pdp_reset_planes(&pdp->pdev->dev, pdp->regs);

	pdp_disable_vsync(pdp);
	pdp_disable_interrupt(pdp);
	tc_set_interrupt_handler(pdp->pdev->dev.parent,
				 TC_INTERRUPT_PDP,
				 NULL, NULL);
	kfree(pdp->supported_modes);
#ifdef SUPPORT_ADF_PDP_FBDEV
	adf_fbdev_destroy(&pdp->adf_fbdev);
#endif
	adf_overlay_engine_destroy(&pdp->adf_overlay);
	adf_interface_destroy(&pdp->adf_interface);
	adf_device_destroy(&pdp->adf_device);
	ion_client_destroy(pdp->ion_client);
	pci_disable_device(pci_dev);
	return err;
}

static void adf_pdp_shutdown_device(struct platform_device *pdev)
{
	/* No cleanup needed, all done in remove_device */
}

static struct platform_device_id pdp_platform_device_id_table[] = {
	{ .name = APOLLO_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_APOLLO },
	{ .name = ODN_DEVICE_NAME_PDP, .driver_data = PDP_VERSION_ODIN },
	{ },
};

static struct platform_driver pdp_platform_driver = {
	.probe = adf_pdp_probe_device,
	.remove = adf_pdp_remove_device,
	.shutdown = adf_pdp_shutdown_device,
	.driver = {
		.name = DRV_NAME,
	},
	.id_table = pdp_platform_device_id_table,
};

static int __init adf_pdp_init(void)
{
	return platform_driver_register(&pdp_platform_driver);
}

static void __exit adf_pdp_exit(void)
{
	platform_driver_unregister(&pdp_platform_driver);
}

module_init(adf_pdp_init);
module_exit(adf_pdp_exit);

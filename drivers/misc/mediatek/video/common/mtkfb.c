/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <mt-plat/dma.h>
#include <linux/compat.h>
#include <linux/dma-mapping.h>
#if defined(COMMON_DISP_LOG)
#include "disp_log.h"
#include "disp_debug.h"
#include "disp_recorder.h"
#include "mtkfb_debug.h"
#else
#include "debug.h"
#include "ddp_log.h"
#include "disp_drv_log.h"
#include "display_recorder.h"
#endif

#include "disp_assert_layer.h"
#include "ddp_hal.h"
#include "disp_lcm.h"
#include "mtkfb.h"
#include "mtkfb_console.h"
#include "mtkfb_fence.h"
#include "mtkfb_info.h"
#include "ddp_ovl.h"
#include "disp_drv_platform.h"
#include "primary_display.h"
#include "ddp_dump.h"
#include "fbconfig_kdebug.h"
#include "ddp_manager.h"
#include "mtk_ovl.h"
#include "ion_drv.h"
#include "ddp_drv.h"
#ifdef DISP_GPIO_DTS
#include "disp_dts_gpio.h" /* set gpio via DTS */
#endif
#include "disp_helper.h"

#define ALIGN_TO(x, n)	(((x) + ((n) - 1)) & ~((n) - 1))


/* xuecheng, remove this because we use session now */
/* mtk_dispif_info_t dispif_info[MTKFB_MAX_DISPLAY_COUNT]; */

struct notifier_block pm_nb;
unsigned int EnableVSyncLog = 0;

static u32 MTK_FB_XRES;
static u32 MTK_FB_YRES;
static u32 MTK_FB_BPP;
static u32 MTK_FB_PAGES;
static u32 fb_xres_update;
static u32 fb_yres_update;
static bool no_update;

#define MTK_FB_XRESV (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT))
#define MTK_FB_YRESV (MTK_FB_YRES * MTK_FB_PAGES)	/* For page flipping */
#define MTK_FB_BYPP  ((MTK_FB_BPP + 7) >> 3)
#define MTK_FB_LINE  (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) * MTK_FB_BYPP)
#define MTK_FB_SIZE  (MTK_FB_LINE * MTK_FB_YRES)

#define MTK_FB_SIZEV (MTK_FB_LINE * MTK_FB_YRES * MTK_FB_PAGES)

#define CHECK_RET(expr)			\
	do {				\
		int ret = (expr);	\
		ASSERT(0 == ret);	\
	} while (0)


static size_t mtkfb_log_on = true;
#define MTKFB_LOG(fmt, arg...)					\
	do {							\
		if (mtkfb_log_on)				\
			pr_debug("DISP/MTKFB " fmt, ##arg);	\
	} while (0)

/* always show this debug info while the global debug log is off */
#define MTKFB_LOG_DBG(fmt, arg...) \
	do { \
		if (!mtkfb_log_on) \
			DISPMSG(fmt, ##arg); \
	} while (0)

#define MTKFB_FUNC()	\
	do { \
		if (mtkfb_log_on) \
			DISPMSG("[Func]%s\n", __func__); \
	} while (0)

#define PRNWARN(fmt, args...)  pr_warn("DISP/MTKFB " fmt, ##args)
#define PRNERR(fmt, args...)  pr_err("DISP/MTKFB " fmt, ##args)

void mtkfb_log_enable(int enable)
{
	mtkfb_log_on = enable;
	MTKFB_LOG("mtkfb log %s\n", enable ? "enabled" : "disabled");
}

/* --------------------------------------------------------------------------- */
/* local variables */
/* --------------------------------------------------------------------------- */

unsigned long fb_pa = 0;

static const struct timeval FRAME_INTERVAL = { 0, 30000 };	/* 33ms */

atomic_t has_pending_update = ATOMIC_INIT(0);
struct fb_overlay_layer video_layerInfo;
uint32_t dbr_backup = 0;
uint32_t dbg_backup = 0;
uint32_t dbb_backup = 0;
bool fblayer_dither_needed = false;
bool is_ipoh_bootup = false;
struct fb_info *mtkfb_fbi;
struct fb_overlay_layer fb_layer_context;
mtk_dispif_info_t dispif_info[MTKFB_MAX_DISPLAY_COUNT];
unsigned int FB_LAYER = 2;
bool is_early_suspended = false;
char mtkfb_lcm_name[256] = { 0 };
/**
 * This mutex is used to prevent tearing due to page flipping when adbd is
 * reading the front buffer
 */
DEFINE_SEMAPHORE(sem_flipping);
DEFINE_SEMAPHORE(sem_early_suspend);
DEFINE_SEMAPHORE(sem_overlay_buffer);

DEFINE_MUTEX(OverlaySettingMutex);
atomic_t OverlaySettingDirtyFlag = ATOMIC_INIT(0);
atomic_t OverlaySettingApplied = ATOMIC_INIT(0);
unsigned int PanDispSettingPending = 0;
unsigned int PanDispSettingDirty = 0;
unsigned int PanDispSettingApplied = 0;

DECLARE_WAIT_QUEUE_HEAD(reg_update_wq);

unsigned int need_esd_check = 0;
DECLARE_WAIT_QUEUE_HEAD(esd_check_wq);

/* extern unsigned int disp_running; */
/* extern wait_queue_head_t disp_done_wq; */

DEFINE_MUTEX(ScreenCaptureMutex);

static int sem_flipping_cnt = 1;
static int sem_early_suspend_cnt = 1;
static int vsync_cnt;

/* extern BOOL is_engine_in_suspend_mode; */
/* extern BOOL is_lcm_in_suspend_mode; */

/* --------------------------------------------------------------------------- */
/* local function declarations */
/* --------------------------------------------------------------------------- */

static int init_framebuffer(struct fb_info *info);
static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info *layerInfo);


/* --------------------------------------------------------------------------- */
/* Timer Routines */
/* --------------------------------------------------------------------------- */
unsigned int lcd_fps = 6000;
wait_queue_head_t screen_update_wq;


/*
 * ---------------------------------------------------------------------------
 * fbdev framework callbacks and the ioctl interface
 * ---------------------------------------------------------------------------
 */
/* Called each time the mtkfb device is opened */
static int mtkfb_open(struct fb_info *info, int user)
{
	/* NOT_REFERENCED(info); */
	/* NOT_REFERENCED(user); */
	/* DISPFUNC();*/
	DISPMSG("%s\n", __func__);
	MSG_FUNC_ENTER();
	MSG_FUNC_LEAVE();
	return 0;
}

/* Called when the mtkfb device is closed. We make sure that any pending
 * gfx DMA operations are ended, before we return. */
static int mtkfb_release(struct fb_info *info, int user)
{

	/* NOT_REFERENCED(info); */
	/* NOT_REFERENCED(user); */
	DISPFUNC();

	MSG_FUNC_ENTER();
	MSG_FUNC_LEAVE();
	return 0;
}

/* Store a single color palette entry into a pseudo palette or the hardware
 * palette if one is available. For now we support only 16bpp and thus store
 * the entry only to the pseudo palette.
 */
static int mtkfb_setcolreg(u_int regno, u_int red, u_int green,
			   u_int blue, u_int transp, struct fb_info *info)
{
	int r = 0;
	unsigned bpp, m;

	/* NOT_REFERENCED(transp); */

	MSG_FUNC_ENTER();

	bpp = info->var.bits_per_pixel;
	m = 1 << bpp;
	if (regno >= m) {
		r = -EINVAL;
		goto exit;
	}

	switch (bpp) {
	case 16:
		/* RGB 565 */
		((u32 *) (info->pseudo_palette))[regno] =
		    ((red & 0xF800) | ((green & 0xFC00) >> 5) | ((blue & 0xF800) >> 11));
		break;
	case 32:
		/* ARGB8888 */
		((u32 *) (info->pseudo_palette))[regno] =
		    (0xff000000) |
		    ((red & 0xFF00) << 8) | ((green & 0xFF00)) | ((blue & 0xFF00) >> 8);
		break;

		/* TODO: RGB888, BGR888, ABGR8888 */

	default:
		ASSERT(0);
	}

exit:
	MSG_FUNC_LEAVE();
	return r;
}

int mtkfb_set_backlight_level(unsigned int level)
{
	MTKFB_FUNC();
	DISPMSG("mtkfb_set_backlight_level:%d Start\n", level);
	primary_display_setbacklight(level);
	DISPMSG("mtkfb_set_backlight_level End\n");
	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);

int mtkfb_set_backlight_mode(unsigned int mode)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_flipping)) {
		DISPERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		return -ERESTARTSYS;
	}
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
		DISPERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		sem_flipping_cnt++;
		up(&sem_flipping);
		return -ERESTARTSYS;
	}

	sem_early_suspend_cnt--;
	if (primary_display_is_sleepd())
		goto End;

	/* DISP_SetBacklight_mode(mode); */
End:
	sem_flipping_cnt++;
	sem_early_suspend_cnt++;
	up(&sem_early_suspend);
	up(&sem_flipping);
	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_mode);

int mtkfb_set_backlight_pwm(int div)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_flipping)) {
		DISPERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		return -ERESTARTSYS;
	}
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
		DISPERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		sem_flipping_cnt++;
		up(&sem_flipping);
		return -ERESTARTSYS;
	}
	sem_early_suspend_cnt--;
	if (primary_display_is_sleepd())
		goto End;
	/* DISP_SetPWM(div); */
End:
	sem_flipping_cnt++;
	sem_early_suspend_cnt++;
	up(&sem_early_suspend);
	up(&sem_flipping);
	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_pwm);

int mtkfb_get_backlight_pwm(int div, unsigned int *freq)
{
	/* DISP_GetPWM(div, freq); */
	return 0;
}
EXPORT_SYMBOL(mtkfb_get_backlight_pwm);

void mtkfb_waitVsync(void)
{
	if (primary_display_is_sleepd()) {
		DISPMSG("[MTKFB_VSYNC]:mtkfb has suspend, return directly\n");
		msleep(20);
		return;
	}
	vsync_cnt++;
#ifdef CONFIG_FPGA_EARLY_PORTING
	msleep(20);
#else
	primary_display_wait_for_vsync(NULL);
#endif
	vsync_cnt--;
}
EXPORT_SYMBOL(mtkfb_waitVsync);

static int _convert_fb_layer_to_disp_input(struct fb_overlay_layer *src, disp_input_config *dst)
{
	dst->layer_id = src->layer_id;
	dst->dirty_roi_num = 0;
	if (!src->layer_enable) {
		dst->layer_enable = 0;
		return 0;
	}

	switch (src->src_fmt) {
	case MTK_FB_FORMAT_YUV422:
		dst->src_fmt = DISP_FORMAT_YUV422;
		break;

	case MTK_FB_FORMAT_RGB565:
		dst->src_fmt = DISP_FORMAT_RGB565;
		break;

	case MTK_FB_FORMAT_RGB888:
		dst->src_fmt = DISP_FORMAT_RGB888;
		break;

	case MTK_FB_FORMAT_BGR888:
		dst->src_fmt = DISP_FORMAT_BGR888;
		break;

	case MTK_FB_FORMAT_ARGB8888:
		dst->src_fmt = DISP_FORMAT_ARGB8888;
		break;

	case MTK_FB_FORMAT_ABGR8888:
		dst->src_fmt = DISP_FORMAT_ABGR8888;
		break;

	case MTK_FB_FORMAT_XRGB8888:
		dst->src_fmt = DISP_FORMAT_XRGB8888;
		break;

	case MTK_FB_FORMAT_XBGR8888:
		dst->src_fmt = DISP_FORMAT_XBGR8888;
		break;

	case MTK_FB_FORMAT_UYVY:
		dst->src_fmt = DISP_FORMAT_UYVY;
		break;

	case MTK_FB_FORMAT_BGRA8888:
		dst->src_fmt = DISP_FORMAT_BGRA8888;
		break;

	case MTK_FB_FORMAT_RGBX8888:
		dst->src_fmt = DISP_FORMAT_RGBX8888;
		break;

	default:
		DISPERR("Invalid color format: 0x%x\n", src->src_fmt);
		return -1;
	}

	dst->src_base_addr = src->src_base_addr;
	dst->security = src->security;
	dst->src_phy_addr = src->src_phy_addr;
	DISPMSG("_convert_fb_layer_to_disp_input, dst->addr=0x%08lx\n",
		(unsigned long)(dst->src_phy_addr));

	dst->isTdshp = src->isTdshp;
	dst->next_buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

	/* set Alpha blending */
	dst->alpha = src->alpha;
	if (MTK_FB_FORMAT_ARGB8888 == src->src_fmt || MTK_FB_FORMAT_ABGR8888 == src->src_fmt)
		dst->alpha_enable = true;
	else
		dst->alpha_enable = false;


	/* set src width, src height */
	dst->src_offset_x = src->src_offset_x;
	dst->src_offset_y = src->src_offset_y;
	dst->src_width = src->src_width;
	dst->src_height = src->src_height;
	dst->tgt_offset_x = src->tgt_offset_x;
	dst->tgt_offset_y = src->tgt_offset_y;
	dst->tgt_width = src->tgt_width;
	dst->tgt_height = src->tgt_height;
	if (dst->tgt_width > dst->src_width)
		dst->tgt_width = dst->src_width;
	if (dst->tgt_height > dst->src_height)
		dst->tgt_height = dst->src_height;

	dst->src_pitch = src->src_pitch;

	/* set color key */
	dst->src_color_key = src->src_color_key;
	dst->src_use_color_key = src->src_use_color_key;

	/* data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT */
	dst->layer_enable = src->layer_enable;

#if 1
	DISPMSG("_convert_fb_layer_to_disp_input():id=%u, en=%u, next_idx=%u, vaddr=%p, paddr=%p,\n",
		dst->layer_id, dst->layer_enable, dst->next_buff_idx, dst->src_base_addr, dst->src_phy_addr);
	DISPMSG("src fmt=%u, dst fmt=%u, pitch=%u, xoff=%u, yoff=%u, w=%u, h=%u\n",
		src->src_fmt, dst->src_fmt, dst->src_pitch, dst->src_offset_x,
		dst->src_offset_y, dst->src_width, dst->src_height);
	DISPMSG("_convert_fb_layer_to_disp_input():target xoff=%u, target yoff=%u, target w=%u, target h=%u, aen=%u\n",
		dst->tgt_offset_x, dst->tgt_offset_y, dst->tgt_width, dst->tgt_height,
		dst->alpha_enable);
#endif

	return 0;
}

static int mtkfb_pan_display_impl(struct fb_var_screeninfo *var, struct fb_info *info)
{
	uint32_t offset = 0;
	uint32_t paStart = 0;
	char *vaStart = NULL, *vaEnd = NULL;
	int ret = 0;
	unsigned int src_pitch = 0;
	disp_session_input_config *session_input;
	disp_input_config *input;

	/* DISPFUNC(); */

	if (no_update) {
		DISPMSG("FB_ACTIVATE_NO_UPDATE flag found, ignore mtkfb_pan_display_impl\n");
		no_update = false;
		return ret;
	}

	DISPMSG("pan_display: offset(%u,%u), res(%u,%u), resv(%u,%u)\n",
		var->xoffset, var->yoffset, info->var.xres, info->var.yres, info->var.xres_virtual,
		info->var.yres_virtual);

	info->var.yoffset = var->yoffset;
	offset = var->yoffset * info->fix.line_length;
	paStart = fb_pa + offset;
	vaStart = info->screen_base + offset;
	vaEnd = vaStart + info->var.yres * info->fix.line_length;

	session_input = kzalloc(sizeof(*session_input), GFP_KERNEL);
	if (!session_input)
		ASSERT(0);

	/* pan display use layer 0 */
	input = &session_input->config[0];
	input->layer_id = 0;
	input->src_phy_addr = (void *)((unsigned long)paStart);
	input->src_base_addr = (void *)((unsigned long)vaStart);
	input->layer_id = primary_display_get_option("FB_LAYER");
	input->layer_enable = 1;
	input->src_offset_x = 0;
	input->src_offset_y = 0;
	input->src_width = var->xres;
	input->src_height = var->yres;
	input->tgt_offset_x = 0;
	input->tgt_offset_y = 0;
	input->tgt_width = var->xres;
	input->tgt_height = var->yres;

	switch (var->bits_per_pixel) {
	case 16:
		input->src_fmt = DISP_FORMAT_RGB565;
		break;
	case 24:
		input->src_fmt = DISP_FORMAT_RGB888;
		break;
	case 32:
		input->src_fmt = (0 == var->blue.offset) ? DISP_FORMAT_BGRA8888 : DISP_FORMAT_RGBX8888;
		break;
	default:
		DISPERR("Invalid color format bpp: 0x%d\n", var->bits_per_pixel);
		kfree(session_input);
		return -1;
	}
	input->alpha_enable = false;

	input->alpha = 0xFF;
	input->next_buff_idx = -1;
	src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
	input->src_pitch = src_pitch;

	session_input->config_layer_num++;

	if (!is_DAL_Enabled()) {
		/* disable font layer(layer3) drawed in lk */
		session_input->config[1].layer_id = primary_display_get_option("ASSERT_LAYER");
		session_input->config[1].next_buff_idx = -1;
		session_input->config[1].layer_enable = 0;
		session_input->config_layer_num++;
	}
	ret = primary_display_config_input_multiple(session_input);
	ret = primary_display_trigger(true, NULL, 0);

	kfree(session_input);
	return ret;
}

/**
 * Set fb_info.fix fields and also updates fbdev.
 * When calling this fb_info.var must be set up already.
 */
static void set_fb_fix(struct mtkfb_device *fbdev)
{
	struct fb_info *fbi = fbdev->fb_info;
	struct fb_fix_screeninfo *fix = &fbi->fix;
	struct fb_var_screeninfo *var = &fbi->var;
	struct fb_ops *fbops = fbi->fbops;

	strncpy(fix->id, MTKFB_DRIVER, sizeof(fix->id));
	fix->type = FB_TYPE_PACKED_PIXELS;

	switch (var->bits_per_pixel) {
	case 16:
	case 24:
	case 32:
		fix->visual = FB_VISUAL_TRUECOLOR;
		break;
	case 1:
	case 2:
	case 4:
	case 8:
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	default:
		ASSERT(0);
	}

	fix->accel = FB_ACCEL_NONE;
	fix->line_length = ALIGN_TO(var->xres_virtual, MTK_FB_ALIGNMENT) * var->bits_per_pixel / 8;
	fix->smem_len = fbdev->fb_size_in_byte;
	fix->smem_start = fbdev->fb_pa_base;

	fix->xpanstep = 0;
	fix->ypanstep = 1;

	fbops->fb_fillrect = cfb_fillrect;
	fbops->fb_copyarea = cfb_copyarea;
	fbops->fb_imageblit = cfb_imageblit;
}


/**
 * Check values in var, try to adjust them in case of out of bound values if
 * possible, or return error.
 */
static int mtkfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fbi)
{
	unsigned int bpp;
	unsigned long max_frame_size;
	unsigned long line_size;

	struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;

	/* DISPFUNC(); */

	DISPMSG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u,\n",
		var->xres, var->yres, var->xres_virtual, var->yres_virtual);
	DISPMSG("xoffset=%u, yoffset=%u, bits_per_pixel=%u\n", var->xoffset, var->yoffset, var->bits_per_pixel);

	bpp = var->bits_per_pixel;

	if (bpp != 16 && bpp != 24 && bpp != 32) {
		MTKFB_LOG("[%s]unsupported bpp: %d", __func__, bpp);
		return -1;
	}

	switch (var->rotate) {
	case 0:
	case 180:
		var->xres = MTK_FB_XRES;
		var->yres = MTK_FB_YRES;
		break;
	case 90:
	case 270:
		var->xres = MTK_FB_YRES;
		var->yres = MTK_FB_XRES;
		break;
	default:
		return -1;
	}

	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;

	max_frame_size = fbdev->fb_size_in_byte;
	DISPMSG("fbdev->fb_size_in_byte=0x%08lx\n", fbdev->fb_size_in_byte);
	line_size = var->xres_virtual * bpp / 8;

	if (line_size * var->yres_virtual > max_frame_size) {
		/* Try to keep yres_virtual first */
		line_size = max_frame_size / var->yres_virtual;
		var->xres_virtual = line_size * 8 / bpp;
		if (var->xres_virtual < var->xres) {
			/* Still doesn't fit. Shrink yres_virtual too */
			var->xres_virtual = var->xres;
			line_size = var->xres * bpp / 8;
			var->yres_virtual = max_frame_size / line_size;
		}
	}

	DISPMSG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u,\n",
		var->xres, var->yres, var->xres_virtual, var->yres_virtual);
	DISPMSG("xoffset=%u, yoffset=%u, bits_per_pixel=%u\n", var->xoffset, var->yoffset, var->bits_per_pixel);
	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	DISPMSG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u,\n",
		var->xres, var->yres, var->xres_virtual, var->yres_virtual);
	DISPMSG("xoffset=%u, yoffset=%u, bits_per_pixel=%u\n", var->xoffset, var->yoffset, var->bits_per_pixel);

	if (16 == bpp) {
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
	} else if (24 == bpp) {
		var->red.length = var->green.length = var->blue.length = 8;
		var->transp.length = 0;

		/* Check if format is RGB565 or BGR565 */

		ASSERT(8 == var->green.offset);
		ASSERT(16 == var->red.offset + var->blue.offset);
		ASSERT(16 == var->red.offset || 0 == var->red.offset);
	} else if (32 == bpp) {
		var->red.length = var->green.length = var->blue.length = var->transp.length = 8;

		/* Check if format is ARGB565 or ABGR565 */

		ASSERT(8 == var->green.offset && 24 == var->transp.offset);
		ASSERT(16 == var->red.offset + var->blue.offset);
		ASSERT(16 == var->red.offset || 0 == var->red.offset);
	}

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	if (var->activate & FB_ACTIVATE_NO_UPDATE)
		no_update = true;
	else
		no_update = false;

	var->activate = FB_ACTIVATE_NOW;

	var->height = UINT_MAX;
	var->width = UINT_MAX;
	var->grayscale = 0;
	var->nonstd = 0;

	var->pixclock = UINT_MAX;
	var->left_margin = UINT_MAX;
	var->right_margin = UINT_MAX;
	var->upper_margin = UINT_MAX;
	var->lower_margin = UINT_MAX;
	var->hsync_len = UINT_MAX;
	var->vsync_len = UINT_MAX;

	var->vmode = FB_VMODE_NONINTERLACED;
	var->sync = 0;

	MSG_FUNC_LEAVE();
	return 0;
}



/* Switch to a new mode. The parameters for it has been check already by
 * mtkfb_check_var.
 */
static int mtkfb_set_par(struct fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->var;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;
	struct fb_overlay_layer fb_layer;
	u32 bpp = var->bits_per_pixel;
	disp_session_input_config *session_input;
	disp_input_config *input;

	/* DISPFUNC(); */
	memset(&fb_layer, 0, sizeof(struct fb_overlay_layer));
	switch (bpp) {
	case 16:
		fb_layer.src_fmt = MTK_FB_FORMAT_RGB565;
		fb_layer.src_use_color_key = 1;
		fb_layer.src_color_key = 0xFF000000;
		break;

	case 24:
		fb_layer.src_use_color_key = 1;
		fb_layer.src_fmt = (0 == var->blue.offset) ?  MTK_FB_FORMAT_RGB888 : MTK_FB_FORMAT_BGR888;
		fb_layer.src_color_key = 0xFF000000;
		break;

	case 32:
		fb_layer.src_use_color_key = 0;
		DISPMSG("set_par,var->blue.offset=%d\n", var->blue.offset);
		fb_layer.src_fmt = (0 == var->blue.offset) ?  MTK_FB_FORMAT_BGRA8888 : MTK_FB_FORMAT_RGBX8888;
		fb_layer.src_color_key = 0;
		break;

	default:
		fb_layer.src_fmt = MTK_FB_FORMAT_UNKNOWN;
		DISPERR("[%s]unsupported bpp: %d", __func__, bpp);
		return -1;
	}

	set_fb_fix(fbdev);

	fb_layer.layer_id = primary_display_get_option("FB_LAYER");
	fb_layer.layer_enable = 1;
	fb_layer.src_base_addr = (void *)((unsigned long)fbdev->fb_va_base + var->yoffset * fbi->fix.line_length);
	DISPMSG("fb_pa=0x%08lx, var->yoffset=0x%08x,fbi->fix.line_length=0x%08x\n",
		fb_pa, var->yoffset, fbi->fix.line_length);
	fb_layer.src_phy_addr = (void *)(fb_pa + var->yoffset * fbi->fix.line_length);
	fb_layer.src_direct_link = 0;
	fb_layer.src_offset_x = fb_layer.src_offset_y = 0;
	fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
	fb_layer.src_width = fb_layer.tgt_width = var->xres;
	fb_layer.src_height = fb_layer.tgt_height = var->yres;
	fb_layer.tgt_offset_x = fb_layer.tgt_offset_y = 0;
	fb_layer.alpha = 0xff;
	/* fb_layer.src_color_key = 0; */
	fb_layer.layer_rotation = MTK_FB_ORIENTATION_0;
	fb_layer.layer_type = LAYER_2D;
	DISPMSG("mtkfb_set_par, fb_layer.src_fmt=%x\n", fb_layer.src_fmt);

	session_input = kzalloc(sizeof(*session_input), GFP_KERNEL);
	if (!session_input)
		goto out;

	session_input->config_layer_num = 0;

	if (!isAEEEnabled) {
		/* DISPCHECK("AEE is not enabled, will disable layer 3\n"); */
		input = &session_input->config[session_input->config_layer_num++];
		input->layer_id = primary_display_get_option("ASSERT_LAYER");
		input->layer_enable = 0;
	} else {
		DISPMSG("AEE is enabled, should not disable layer 3\n");
	}

	input = &session_input->config[session_input->config_layer_num++];
	_convert_fb_layer_to_disp_input(&fb_layer, input);
	primary_display_config_input_multiple(session_input);
	kfree(session_input);

out:
	/* backup fb_layer information. */
	memcpy(&fb_layer_context, &fb_layer, sizeof(fb_layer));

	MSG_FUNC_LEAVE();
	return 0;
}


static int mtkfb_soft_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	/* NOT_REFERENCED(info); */
	/* NOT_REFERENCED(cursor); */

	return 0;
}

static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info *layerInfo)
{
	return 0;
}

void mtkfb_dump_layer_info(void)
{
}

static disp_session_input_config session_input;
static int mtkfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	DISP_STATUS ret = 0;
	int r = 0;

	DISPFUNC();
	DISPMSG("mtkfb_ioctl, info=%p, cmd nr=0x%08x, cmd size=0x%08x\n", info,
		 (unsigned int)_IOC_NR(cmd), (unsigned int)_IOC_SIZE(cmd));

	switch (cmd) {
	case MTKFB_GET_FRAMEBUFFER_MVA:
		return copy_to_user(argp, &fb_pa, sizeof(fb_pa)) ? -EFAULT : 0;
		/* remain this for engineer mode dfo multiple resolution */

	case MTKFB_GET_DISPLAY_IF_INFORMATION:
	{
		int displayid = 0;

		if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			return -EFAULT;
		}

		if (displayid > MTKFB_MAX_DISPLAY_COUNT) {
			DISPERR("[FB]: invalid display id:%d\n", displayid);
			return -EFAULT;
		}

		if (displayid == 0) {
			dispif_info[displayid].displayWidth = primary_display_get_width();
			dispif_info[displayid].displayHeight = primary_display_get_height();

			dispif_info[displayid].lcmOriginalWidth =
			    primary_display_get_original_width();
			dispif_info[displayid].lcmOriginalHeight =
			    primary_display_get_original_height();
			dispif_info[displayid].displayMode =
			    primary_display_is_video_mode() ? 0 : 1;
		} else {
			DISPERR("information for displayid: %d is not available now\n",
				displayid);
		}

		if (copy_to_user((void __user *)arg, &(dispif_info[displayid]), sizeof(mtk_dispif_info_t))) {
			MTKFB_LOG("[FB]: copy_to_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		}

		return r;
	}

	case MTKFB_POWEROFF:
	{
		MTKFB_FUNC();
		if (primary_display_is_sleepd()) {
			PRNWARN("[FB Driver] Still in MTKFB_POWEROFF!!!\n");
			return r;
		}

		PRNWARN("[FB Driver] enter MTKFB_POWEROFF\n");
		ret = primary_display_suspend();
		if (ret < 0)
			DISPERR("primary display suspend failed\n");
		PRNWARN("[FB Driver] leave MTKFB_POWEROFF\n");

		is_early_suspended = true; /* no care */
		return r;
	}

	case MTKFB_POWERON:
	{
		MTKFB_FUNC();
		if (primary_display_is_alive()) {
			DISPMSG("[FB Driver] Still in MTKFB_POWERON!!!\n");
			return r;
		}
		PRNWARN("[FB Driver] enter MTKFB_POWERON\n");
		primary_display_resume();
		PRNWARN("[FB Driver] leave MTKFB_POWERON\n");
		is_early_suspended = false; /* no care */
		return r;
	}
	case MTKFB_GET_POWERSTATE:
	{
		int power_state;

		if (primary_display_is_sleepd())
			power_state = 0;
		else
			power_state = 1;

		if (copy_to_user(argp, &power_state, sizeof(power_state))) {
			DISPERR("[FB]: MTKFB_GET_POWERSTATE failed!\n");
			return -EFAULT;
		}

		return 0;
	}

	case MTKFB_CONFIG_IMMEDIATE_UPDATE:
	{
		MTKFB_LOG("[%s] MTKFB_CONFIG_IMMEDIATE_UPDATE, enable = %lu\n", __func__, arg);
		if (down_interruptible(&sem_early_suspend)) {
			MTKFB_LOG("[mtkfb_ioctl] can't get semaphore:%d\n", __LINE__);
			return -ERESTARTSYS;
		}
		sem_early_suspend_cnt--;
		/* DISP_WaitForLCDNotBusy(); */
		/* ret = DISP_ConfigImmediateUpdate((BOOL)arg); */
		/* sem_early_suspend_cnt++; */
		up(&sem_early_suspend);
		return r;
	}

	case MTKFB_CAPTURE_FRAMEBUFFER:
	{
		unsigned long dst_pbuf = 0;
		unsigned long *src_pbuf = 0;
		unsigned int pixel_bpp = info->var.bits_per_pixel / 8;
		unsigned int fbsize = DISP_GetScreenHeight() * DISP_GetScreenWidth() * pixel_bpp;

		if (copy_from_user(&dst_pbuf, (void __user *)arg, sizeof(dst_pbuf))) {
			DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		} else {
			src_pbuf = vmalloc(fbsize);
			if (!src_pbuf) {
				DISPERR("[FB]: vmalloc capture src_pbuf failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				dprec_logger_start(DPREC_LOGGER_WDMA_DUMP, 0, 0);
				primary_display_capture_framebuffer_ovl((unsigned long)src_pbuf,
					MTK_FB_FORMAT_BGRA8888);
				dprec_logger_done(DPREC_LOGGER_WDMA_DUMP, 0, 0);
				if (copy_to_user((unsigned long *)dst_pbuf, src_pbuf, fbsize)) {
					DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
					r = -EFAULT;
				}
				vfree(src_pbuf);
			}
		}

		return r;
	}

	case MTKFB_SLT_AUTO_CAPTURE:
	{
		struct fb_slt_catpure capConfig;
		char *dst_buffer;
		unsigned int fb_size;

		DISPMSG("MTKFB_SLT_AUTO_CAPTURE\n");
		if (copy_from_user(&capConfig, (void __user *)arg, sizeof(capConfig))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		} else {
			dst_buffer = (char *)capConfig.outputBuffer;
			fb_size = DISP_GetScreenWidth() * DISP_GetScreenHeight() * 4;
			if (!capConfig.outputBuffer) {
				DISPERR("[FB]: vmalloc capture outputBuffer failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				capConfig.outputBuffer = vmalloc(fb_size);
				primary_display_capture_framebuffer_ovl((unsigned long)capConfig.outputBuffer,
					capConfig.format);
				if (copy_to_user(dst_buffer, (char *)capConfig.outputBuffer, fb_size)) {
					DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
					r = -EFAULT;
				}
				vfree((char *)capConfig.outputBuffer);
			}
		}

		return r;
	}

	case MTKFB_GET_OVERLAY_LAYER_INFO:
	{
		struct fb_overlay_layer_info layerInfo;

		MTKFB_LOG(" mtkfb_ioctl():MTKFB_GET_OVERLAY_LAYER_INFO\n");

		if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			return -EFAULT;
		}
		if (mtkfb_get_overlay_layer_info(&layerInfo) < 0) {
			MTKFB_LOG("[FB]: Failed to get overlay layer info\n");
			return -EFAULT;
		}
		if (copy_to_user((void __user *)arg, &layerInfo, sizeof(layerInfo))) {
			MTKFB_LOG("[FB]: copy_to_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		}
		return r;
	}
	case MTKFB_SET_OVERLAY_LAYER:
	{
		struct fb_overlay_layer *layerInfo;

		layerInfo = kmalloc(sizeof(*layerInfo), GFP_KERNEL);
		if (!layerInfo)
			return -ENOMEM;

		if (copy_from_user(layerInfo, (void __user *)arg, sizeof(*layerInfo))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		} else {
			disp_input_config *input;

				/* in early suspend mode ,will not update buffer index, info SF by return value */
			if (primary_display_is_sleepd()) {
				DISPMSG
				    ("[FB] error, set overlay in early suspend ,skip!\n");
				kfree(layerInfo);
				return MTKFB_ERROR_IS_EARLY_SUSPEND;
			}
			memset((void *)&session_input, 0, sizeof(session_input));
			if (layerInfo->layer_id >= TOTAL_OVL_LAYER_NUM) {
				DISPERR("MTKFB_SET_OVERLAY_LAYER, layer_id invalid=%d\n",
					 layerInfo->layer_id);
			} else {
				input = &session_input.config[session_input.config_layer_num++];
				_convert_fb_layer_to_disp_input(layerInfo, input);
			}


			primary_display_config_input_multiple(&session_input);
			primary_display_trigger(1, NULL, 0);
		}

		kfree(layerInfo);
		return r;
	}

	case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT:
	{
		DISPMSG("[DDP] mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT\n");
		/* call info dump function here */
		/* mtkfb_dump_layer_info(); */
		return r;
	}

	case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT_AEE:
	{
		DISPMSG("[DDP] mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT\n");
		return r;
	}

	case MTKFB_SET_VIDEO_LAYERS:
	{
		struct mmp_fb_overlay_layers {
			struct fb_overlay_layer Layer0;
			struct fb_overlay_layer Layer1;
			struct fb_overlay_layer Layer2;
			struct fb_overlay_layer Layer3;
		};

		struct fb_overlay_layer *layerInfo;
		int layerInfo_size = sizeof(struct fb_overlay_layer) * VIDEO_LAYER_COUNT;

		MTKFB_LOG(" mtkfb_ioctl():MTKFB_SET_VIDEO_LAYERS\n");
		layerInfo = kmalloc(layerInfo_size, GFP_KERNEL);
		if (!layerInfo)
			return -ENOMEM;

		if (copy_from_user(layerInfo, (void __user *)arg, layerInfo_size)) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		} else {
			int32_t i;
			disp_input_config *input;

			memset((void *)&session_input, 0, sizeof(session_input));

			for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
				if (layerInfo[i].layer_id >= TOTAL_OVL_LAYER_NUM) {
					DISPERR
					    ("MTKFB_SET_VIDEO_LAYERS, layer_id invalid=%d\n",
					     layerInfo[i].layer_id);
					continue;
				}

				input = &session_input.config[session_input.config_layer_num++];
				_convert_fb_layer_to_disp_input(&layerInfo[i], input);
			}
			primary_display_config_input_multiple(&session_input);
			primary_display_trigger(1, NULL, 0);
		}
		kfree(layerInfo);

		return r;
	}

	case MTKFB_TRIG_OVERLAY_OUT:
	{
		MTKFB_LOG(" mtkfb_ioctl():MTKFB_TRIG_OVERLAY_OUT\n");
		primary_display_trigger(1, NULL, 0);
		return 0;
	}

	case MTKFB_META_RESTORE_SCREEN:
	{
		struct fb_var_screeninfo var;

		if (copy_from_user(&var, argp, sizeof(var)))
			return -EFAULT;

		info->var.yoffset = var.yoffset;
		init_framebuffer(info);

		return mtkfb_pan_display_impl(&var, info);
	}


	case MTKFB_GET_DEFAULT_UPDATESPEED:
	{
		unsigned int speed;

		MTKFB_LOG("[MTKFB] get default update speed\n");
		/* DISP_Get_Default_UpdateSpeed(&speed); */

		DISPMSG("[MTKFB EM]MTKFB_GET_DEFAULT_UPDATESPEED is %d\n", speed);
		return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
	}

	case MTKFB_GET_CURR_UPDATESPEED:
	{
		unsigned int speed;

		MTKFB_LOG("[MTKFB] get current update speed\n");
		/* DISP_Get_Current_UpdateSpeed(&speed); */

		DISPMSG("[MTKFB EM]MTKFB_GET_CURR_UPDATESPEED is %d\n", speed);
		return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
	}

	case MTKFB_CHANGE_UPDATESPEED:
	{
		unsigned int speed;

		MTKFB_LOG("[MTKFB] change update speed\n");

		if (copy_from_user(&speed, (void __user *)arg, sizeof(speed))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
			r = -EFAULT;
		} else {
			/* DISP_Change_Update(speed); */

			DISPMSG("[MTKFB EM]MTKFB_CHANGE_UPDATESPEED is %d\n", speed);
		}
		return r;
	}

	case MTKFB_AEE_LAYER_EXIST:
	{
		/* pr_debug("[MTKFB] isAEEEnabled=%d\n", isAEEEnabled); */
		return copy_to_user(argp, &isAEEEnabled,
				    sizeof(isAEEEnabled)) ? -EFAULT : 0;
	}
	case MTKFB_LOCK_FRONT_BUFFER:
		return 0;
	case MTKFB_UNLOCK_FRONT_BUFFER:
		return 0;

	case MTKFB_FACTORY_AUTO_TEST:
	{
		unsigned int result = 0;

		DISPMSG("factory mode: lcm auto test\n");
		result = mtkfb_fm_auto_test();
		return copy_to_user(argp, &result, sizeof(result)) ? -EFAULT : 0;
	}
	case MTKFB_META_SHOW_BOOTLOGO:
		{
			struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
			int i;
			disp_input_config *input;

			DISPMSG("MTKFB_META_SHOW_BOOTLOGO\n");
			memset((void *)&session_input, 0, sizeof(session_input));

			for (i = 0; i < 2; i++) {

				input = &session_input.config[session_input.config_layer_num++];

				input->layer_enable = 1;
				input->src_fmt = DISP_FORMAT_RGBA8888;
				input->src_offset_x = 0;
				input->src_offset_y = 0;
				input->src_width = MTK_FB_XRES;
				input->src_height = MTK_FB_YRES;
				input->tgt_offset_x = 0;
				input->tgt_offset_y = 0;
				input->tgt_width = MTK_FB_XRES;
				input->tgt_height = MTK_FB_YRES;

				input->src_pitch = ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) * 4;
				input->alpha_enable = 1;
				input->alpha = 0xff;
				input->next_buff_idx = -1;
			}

			input = &session_input.config[0];
			input->layer_id = 0;
			input->src_phy_addr = (void *)(fbdev->fb_pa_base);

			input = &session_input.config[1];
			input->layer_id = 3;
			input->src_phy_addr = (void *)(fbdev->fb_pa_base +
						       (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) *
							ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * 4));

			primary_display_config_input_multiple(&session_input);
			primary_display_trigger(1, NULL, 0);

			return 0;
		}
	default:
		DISPMSG("mtkfb_ioctl Not support, info=%p, cmd=0x%08x, arg=0x%lx\n", info,
			 (unsigned int)cmd, arg);
		return -EINVAL;
	}
}

#ifdef CONFIG_COMPAT
static void compat_convert(struct compat_fb_overlay_layer *compat_info,
			   struct fb_overlay_layer *info)
{
	info->layer_id = compat_info->layer_id;
	info->layer_enable = compat_info->layer_enable;
	info->src_base_addr = (void *)((unsigned long)compat_info->src_base_addr);
	info->src_phy_addr = (void *)((unsigned long)compat_info->src_phy_addr);
	info->src_direct_link = compat_info->src_direct_link;
	info->src_fmt = compat_info->src_fmt;
	info->src_use_color_key = compat_info->src_use_color_key;
	info->src_color_key = compat_info->src_color_key;
	info->src_pitch = compat_info->src_pitch;
	info->src_offset_x = compat_info->src_offset_x;
	info->src_offset_y = compat_info->src_offset_y;
	info->src_width = compat_info->src_width;
	info->src_height = compat_info->src_height;
	info->tgt_offset_x = compat_info->tgt_offset_x;
	info->tgt_offset_y = compat_info->tgt_offset_y;
	info->tgt_width = compat_info->tgt_width;
	info->tgt_height = compat_info->tgt_height;
	info->layer_rotation = compat_info->layer_rotation;
	info->layer_type = compat_info->layer_type;
	info->video_rotation = compat_info->video_rotation;

	info->isTdshp = compat_info->isTdshp;
	info->next_buff_idx = compat_info->next_buff_idx;
	info->identity = compat_info->identity;
	info->connected_type = compat_info->connected_type;

	info->security = compat_info->security;
	info->alpha_enable = compat_info->alpha_enable;
	info->alpha = compat_info->alpha;
	info->fence_fd = compat_info->fence_fd;
	info->ion_fd = compat_info->ion_fd;
}

static int mtkfb_compat_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct fb_overlay_layer layerInfo;
	long ret = 0;

	pr_debug("[FB Driver] mtkfb_compat_ioctl, cmd=0x%08x, cmd nr=0x%08x, cmd size=0x%08x\n",
		 cmd, (unsigned int)_IOC_NR(cmd), (unsigned int)_IOC_SIZE(cmd));

	switch (cmd) {
	case COMPAT_MTKFB_GET_FRAMEBUFFER_MVA:
	{
		compat_uint_t __user *data32;
		__u32 data;

		data32 = compat_ptr(arg);
		data = (__u32) fb_pa;
		if (put_user(data, data32)) {
			pr_err("MTKFB_FRAMEBUFFER_MVA failed\n");
			ret = -EFAULT;
		}
		pr_debug("MTKFB_FRAMEBUFFER_MVA success 0x%lx\n", fb_pa);
		return ret;
	}
	case COMPAT_MTKFB_GET_DISPLAY_IF_INFORMATION:
	{
		compat_uint_t __user *data32;
		compat_uint_t displayid = 0;

		data32 = compat_ptr(arg);
		if (get_user(displayid, data32)) {
			pr_err("COMPAT_MTKFB_GET_DISPLAY_IF_INFORMATION failed\n");
			return -EFAULT;
		}
		if (displayid > MTKFB_MAX_DISPLAY_COUNT) {
			pr_err("[FB]: invalid display id:%d\n", displayid);
			return -EFAULT;
		}
		if (displayid == 0) {
			dispif_info[displayid].displayWidth = primary_display_get_width();
			dispif_info[displayid].displayHeight = primary_display_get_height();

			dispif_info[displayid].lcmOriginalWidth =
				primary_display_get_original_width();
			dispif_info[displayid].lcmOriginalHeight =
				primary_display_get_original_height();
			dispif_info[displayid].displayMode =
				primary_display_is_video_mode() ? 0 : 1;
		} else {
			DISPERR("information for displayid: %d is not available now\n",
			displayid);
		}

		if (copy_to_user((void __user *)arg,
			&(dispif_info[displayid]), sizeof(compat_mtk_dispif_info_t))) {
			pr_err("[FB]: copy_to_user failed! line:%d\n", __LINE__);
			return -EFAULT;
		}
		break;
	}
	case COMPAT_MTKFB_POWEROFF:
	{
		ret = mtkfb_ioctl(info, MTKFB_POWEROFF, arg);
		break;
	}
	case COMPAT_MTKFB_POWERON:
	{
		ret = mtkfb_ioctl(info, MTKFB_POWERON, arg);
		break;
	}
	case COMPAT_MTKFB_GET_POWERSTATE:
	{
		compat_uint_t __user *data32;
		int power_state = 0;

		data32 = compat_ptr(arg);
		if (primary_display_is_sleepd())
			power_state = 0;
		else
			power_state = 1;
		if (put_user(power_state, data32)) {
			pr_err("MTKFB_GET_POWERSTATE failed\n");
			ret = -EFAULT;
		}
		pr_debug("MTKFB_GET_POWERSTATE success %d\n", power_state);
		break;
	}
	case COMPAT_MTKFB_CAPTURE_FRAMEBUFFER:
	{
		compat_ulong_t __user *data32;
		unsigned long *pbuf;
		unsigned int pixel_bpp = info->var.bits_per_pixel / 8;
		unsigned int fbsize = DISP_GetScreenHeight() * DISP_GetScreenWidth() * pixel_bpp;
		unsigned long dest;

		data32 = compat_ptr(arg);
		pbuf = compat_alloc_user_space(fbsize);

		if (!pbuf) {
			DISPERR("[FB]: vmalloc capture src_pbuf failed! line:%d\n", __LINE__);
			ret  = -EFAULT;
		} else {
			dprec_logger_start(DPREC_LOGGER_WDMA_DUMP, 0, 0);
			primary_display_capture_framebuffer_ovl((unsigned long)pbuf, MTK_FB_FORMAT_BGRA8888);
			dprec_logger_done(DPREC_LOGGER_WDMA_DUMP, 0, 0);
			ret = get_user(dest, data32);
			if (copy_in_user((unsigned long *)dest, pbuf, fbsize/2)) {
				DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
				ret  = -EFAULT;
			}
		}
		break;
	}
	case COMPAT_MTKFB_TRIG_OVERLAY_OUT:
	{
		arg = (unsigned long)compat_ptr(arg);
		ret = mtkfb_ioctl(info, MTKFB_TRIG_OVERLAY_OUT, arg);
		break;
	}
	case COMPAT_MTKFB_META_RESTORE_SCREEN:
	{
		arg = (unsigned long)compat_ptr(arg);
		ret = mtkfb_ioctl(info, MTKFB_META_RESTORE_SCREEN, arg);
		break;
	}
	case COMPAT_MTKFB_SET_OVERLAY_LAYER:
	{
		struct compat_fb_overlay_layer *compat_layerInfo;

		compat_layerInfo = kmalloc(sizeof(*compat_layerInfo), GFP_KERNEL);
		if (!compat_layerInfo)
			return -ENOMEM;

		MTKFB_LOG(" mtkfb_compat_ioctl():MTKFB_SET_OVERLAY_LAYER\n");

		arg = (unsigned long)compat_ptr(arg);
		if (copy_from_user(compat_layerInfo, (void __user *)arg, sizeof(*compat_layerInfo))) {
			MTKFB_LOG("[FB Driver]: copy_from_user failed! line:%d\n", __LINE__);
			ret = -EFAULT;
		} else {
			disp_input_config *input;

			compat_convert(compat_layerInfo, &layerInfo);

			/* in early suspend mode ,will not update buffer index, info SF by return value */
			if (primary_display_is_sleepd()) {
				pr_debug("[FB Driver] error, set overlay in early suspend ,skip!\n");
				kfree(compat_layerInfo);
				return MTKFB_ERROR_IS_EARLY_SUSPEND;
			}
			memset((void *)&session_input, 0, sizeof(session_input));
			if (layerInfo.layer_id >= TOTAL_OVL_LAYER_NUM) {
				DISPERR
					("COMPAT_MTKFB_SET_OVERLAY_LAYER, layer_id invalid=%d\n",
					 layerInfo.layer_id);
			} else {
				input = &session_input.config[session_input.config_layer_num++];
				_convert_fb_layer_to_disp_input(&layerInfo, input);
			}
			primary_display_config_input_multiple(&session_input);
			/* primary_display_trigger(1, NULL, 0); */
		}
		kfree(compat_layerInfo);
		break;
	}
	case COMPAT_MTKFB_SET_VIDEO_LAYERS:
	{
		struct compat_fb_overlay_layer *compat_layerInfo;
		int compat_layerInfo_size = sizeof(struct compat_fb_overlay_layer) * VIDEO_LAYER_COUNT;

		compat_layerInfo = kmalloc(compat_layerInfo_size, GFP_KERNEL);
		if (!compat_layerInfo)
			return -ENOMEM;

		MTKFB_LOG(" mtkfb_compat_ioctl():MTKFB_SET_VIDEO_LAYERS\n");

		if (copy_from_user(compat_layerInfo, (void __user *)arg, compat_layerInfo_size)) {
			MTKFB_LOG("[FB Driver]: copy_from_user failed! line:%d\n", __LINE__);
			ret = -EFAULT;
		} else {
			int32_t i;
			/* mutex_lock(&OverlaySettingMutex); */
			disp_input_config *input;

			memset((void *)&session_input, 0, sizeof(session_input));

			for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
				compat_convert(&compat_layerInfo[i], &layerInfo);
				if (layerInfo.layer_id >= TOTAL_OVL_LAYER_NUM) {
					DISPERR
					    ("COMPAT_MTKFB_SET_VIDEO_LAYERS, layer_id invalid=%d\n",
					     layerInfo.layer_id);
					continue;
				}
				input =
				    &session_input.config[session_input.config_layer_num++];
				_convert_fb_layer_to_disp_input(&layerInfo, input);
			}
			/* is_ipoh_bootup = false; */
			/* atomic_set(&OverlaySettingDirtyFlag, 1); */
			/* atomic_set(&OverlaySettingApplied, 0); */
			/* mutex_unlock(&OverlaySettingMutex); */
			/* MMProfileLogStructure(MTKFB_MMP_Events.SetOverlayLayers, MMProfileFlagEnd,
						 layerInfo, struct mmp_fb_overlay_layers); */
			primary_display_config_input_multiple(&session_input);
			/* primary_display_trigger(1, NULL, 0); */
		}
		kfree(compat_layerInfo);
		break;
	}
	case COMPAT_MTKFB_AEE_LAYER_EXIST:
	{
		int dal_en = is_DAL_Enabled();
		compat_ulong_t __user *data32;

		data32 = compat_ptr(arg);
		if (put_user(dal_en, data32)) {
			pr_err("MTKFB_GET_POWERSTATE failed\n");
			ret = -EFAULT;
		}
		break;
	}
	case COMPAT_MTKFB_FACTORY_AUTO_TEST:
	{
		unsigned long result = 0;
		compat_ulong_t __user *data32;

		DISPMSG("factory mode: lcm auto test\n");
		result = mtkfb_fm_auto_test();
		data32 = compat_ptr(arg);
		if (put_user(result, data32)) {
			pr_err("MTKFB_GET_POWERSTATE failed\n");
			ret = -EFAULT;
		}
		break;
		/*return copy_to_user(argp, &result, sizeof(result)) ? -EFAULT : 0;*/
	}
	case COMPAT_MTKFB_META_SHOW_BOOTLOGO:
	{
		arg = (unsigned long)compat_ptr(arg);
		ret = mtkfb_ioctl(info, MTKFB_META_SHOW_BOOTLOGO, arg);
		break;
	}
	default:
		/* NOTHING DIFFERENCE with standard ioctl calling */
		arg = (unsigned long)compat_ptr(arg);
		ret = mtkfb_ioctl(info, cmd, arg);
		break;
	}

	return ret;
}
#endif

static int mtkfb_pan_display_proxy(struct fb_var_screeninfo *var, struct fb_info *info)
{
#ifdef CONFIG_MTPROF_APPLAUNCH	/* eng enable, user disable */
	pr_debug("AppLaunch " "mtkfb_pan_display_proxy.\n");
#endif
	return mtkfb_pan_display_impl(var, info);
}

static void mtkfb_blank_suspend(void);
static void mtkfb_blank_resume(void);

#if defined(CONFIG_PM_AUTOSLEEP)
static int mtkfb_blank(int blank_mode, struct fb_info *info)
{
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		mtkfb_blank_resume();
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		mtkfb_blank_suspend();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

/* Callback table for the frame buffer framework. Some of these pointers
 * will be changed according to the current setting of fb_info->accel_flags.
 */
static struct fb_ops mtkfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = mtkfb_open,
	.fb_release = mtkfb_release,
	.fb_setcolreg = mtkfb_setcolreg,
	.fb_pan_display = mtkfb_pan_display_proxy,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_cursor = mtkfb_soft_cursor,
	.fb_check_var = mtkfb_check_var,
	.fb_set_par = mtkfb_set_par,
	.fb_ioctl = mtkfb_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = mtkfb_compat_ioctl,
#endif
#if defined(CONFIG_PM_AUTOSLEEP)
	.fb_blank = mtkfb_blank,
#endif
};

/**
 * ---------------------------------------------------------------------------
 * Sysfs interface
 * ---------------------------------------------------------------------------
 */
static int mtkfb_register_sysfs(struct mtkfb_device *fbdev)
{
	/* NOT_REFERENCED(fbdev); */

	return 0;
}

static void mtkfb_unregister_sysfs(struct mtkfb_device *fbdev)
{
	/* NOT_REFERENCED(fbdev); */
}

/**
 * ---------------------------------------------------------------------------
 * LDM callbacks
 * ---------------------------------------------------------------------------
 */
/* Initialize system fb_info object and set the default video mode.
 * The frame buffer memory already allocated by lcddma_init
 */
static int mtkfb_fbinfo_init(struct fb_info *info)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;
	struct fb_var_screeninfo var;
	int r = 0;

	/*DISPFUNC(); */

	BUG_ON(!fbdev->fb_va_base);
	info->fbops = &mtkfb_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->screen_base = (char *)fbdev->fb_va_base;
	info->screen_size = fbdev->fb_size_in_byte;
	info->pseudo_palette = fbdev->pseudo_palette;

	r = fb_alloc_cmap(&info->cmap, 32, 0);
	if (r != 0)
		DISPERR("unable to allocate color map memory\n");

	/* setup the initial video mode (RGB565) */

	memset(&var, 0, sizeof(var));

	var.xres = MTK_FB_XRES;
	var.yres = MTK_FB_YRES;
	var.xres_virtual = MTK_FB_XRESV;
	var.yres_virtual = MTK_FB_YRESV;
	DISPMSG("FB_XRES=%d, FB_YRES=%d, FB_XRES_V=%d, FB_YRES_V=%d, BPP=%d, FB_PAGES=%d, FB_LINE=%d, FB_SIZEV=%d\n",
		MTK_FB_XRES, MTK_FB_YRES, MTK_FB_XRESV, MTK_FB_YRESV, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE,
		MTK_FB_SIZEV);
	/* use 32 bit framebuffer as default */
	var.bits_per_pixel = 32;

	var.transp.offset = 24;
	var.red.length = 8;
#if 0
	var.red.offset = 16;
	var.red.length = 8;
	var.green.offset = 8;
	var.green.length = 8;
	var.blue.offset = 0;
	var.blue.length = 8;
#else
	var.red.offset = 0;
	var.red.length = 8;
	var.green.offset = 8;
	var.green.length = 8;
	var.blue.offset = 16;
	var.blue.length = 8;
#endif
	var.width = DISP_GetActiveWidth();
	var.height = DISP_GetActiveHeight();
	var.activate = FB_ACTIVATE_NOW;

	r = mtkfb_check_var(&var, info);
	if (r != 0)
		DISPERR("failed to mtkfb_check_var\n");

	info->var = var;

	r = mtkfb_set_par(info);
	if (r != 0)
		DISPERR("failed to mtkfb_set_par\n");

	MSG_FUNC_LEAVE();
	return r;
}

/* Release the fb_info object */
static void mtkfb_fbinfo_cleanup(struct mtkfb_device *fbdev)
{
	MSG_FUNC_ENTER();

	fb_dealloc_cmap(&fbdev->fb_info->cmap);

	MSG_FUNC_LEAVE();
}

/* fast memset for hw test tool */
void DISP_memset_io(volatile void __iomem *dst, int c, size_t count)
{
	u32 qc = (u8)c;

	qc |= qc << 8;
	qc |= qc << 16;

	while (count && !IS_ALIGNED((unsigned long)dst, 8)) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}
	while (count >= 4) {
		__raw_writel(qc, dst);
		dst += 4;
		count -= 4;
	}

	while (count) {
		__raw_writeb(c, dst);
		dst++;
		count--;
	}
}
/* Init frame buffer content as 3 R/G/B color bars for debug */
static int init_framebuffer(struct fb_info *info)
{
	void *buffer = info->screen_base + info->var.yoffset * info->fix.line_length;

	/* clean whole frame buffer as black */
	int size = info->var.xres_virtual * info->var.yres * info->var.bits_per_pixel/8;

	DISP_memset_io(buffer, 0, size);


	return 0;
}


/**
 * Free driver resources. Can be called to rollback an aborted initialization
 * sequence.
 */
static void mtkfb_free_resources(struct mtkfb_device *fbdev, int state)
{
	int r = 0;

	switch (state) {
	case MTKFB_ACTIVE:
		r = unregister_framebuffer(fbdev->fb_info);
		ASSERT(0 == r);
		/* lint -fallthrough */
	case 5:
		mtkfb_unregister_sysfs(fbdev);
		/* lint -fallthrough */
	case 4:
		mtkfb_fbinfo_cleanup(fbdev);
		/* lint -fallthrough */
	case 3:
		/* DISP_CHECK_RET(DISP_Deinit()); */
		/* lint -fallthrough */
	case 2:
#ifndef CONFIG_FPGA_EARLY_PORTING
		dma_free_coherent(0, fbdev->fb_size_in_byte, fbdev->fb_va_base, fbdev->fb_pa_base);
#endif
		/* lint -fallthrough */
	case 1:
		dev_set_drvdata(fbdev->dev, NULL);
		framebuffer_release(fbdev->fb_info);
		/* lint -fallthrough */
	case 0:
		/* nothing to free */
		break;
	default:
		BUG();
	}
}

void disp_get_fb_address(unsigned long *fbVirAddr, unsigned long *fbPhysAddr)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;

	*fbVirAddr = (unsigned long)fbdev->fb_va_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
	*fbPhysAddr = (unsigned long)fbdev->fb_pa_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
}

#ifdef CONFIG_OF
struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];	/* this is the minimum size */
};
unsigned int islcmconnected = 0;
unsigned int is_lcm_inited = 0;
unsigned int vramsize = 0;
phys_addr_t fb_base = 0;
static int is_videofb_parse_done;

static int fb_early_init_dt_get_chosen(unsigned long node, const char *uname, int depth, void *data)
{
	if (depth != 1 || (strcmp(uname, "chosen") != 0 && strcmp(uname, "chosen@0") != 0))
		return 0;

	*(unsigned long *)data = node;
	return 1;
}

static int __parse_tag_videolfb_extra(unsigned long node)
{
	void *prop;
	unsigned long size = 0;
	u32 fb_base_h, fb_base_l;

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-fb_base_h", NULL);
	if (!prop)
		return -1;
	fb_base_h = of_read_number(prop, 1);

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-fb_base_l", NULL);
	if (!prop)
		return -1;
	fb_base_l = of_read_number(prop, 1);

	fb_base = ((u64) fb_base_h << 32) | (u64) fb_base_l;

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-islcmfound", NULL);
	if (!prop)
		return -1;
	islcmconnected = of_read_number(prop, 1);

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-islcm_inited", NULL);
	if (!prop)
		is_lcm_inited = 1;
	else
		is_lcm_inited = of_read_number(prop, 1);

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-fps", NULL);
	if (!prop)
		return -1;
	lcd_fps = of_read_number(prop, 1);
	if (0 == lcd_fps)
		lcd_fps = 6000;

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-vramSize", NULL);
	if (!prop)
		return -1;
	vramsize = of_read_number(prop, 1);

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-fb_base_l", NULL);
	if (!prop)
		return -1;
	fb_base_l = of_read_number(prop, 1);

	prop = (void *)of_get_flat_dt_prop(node, "atag,videolfb-lcmname", (int *)&size);
	if (!prop)
		return -1;
	if (size >= sizeof(mtkfb_lcm_name)) {
		DISPMSG("%s: error to get lcmname size=%ld\n", __func__, size);
		return -1;
	}
	memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char *)mtkfb_lcm_name, prop, sizeof(mtkfb_lcm_name));
	mtkfb_lcm_name[size] = '\0';
	pr_debug("__parse_tag_videolfb_extra done\n");
	return 0;
}

static int __parse_tag_videolfb(unsigned long node)
{
	struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;

	videolfb_tag = (struct tag_videolfb *)of_get_flat_dt_prop(node, "atag,videolfb", (int *)&size);
	if (videolfb_tag) {
		memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
		strcpy((char *)mtkfb_lcm_name, videolfb_tag->lcmname);
		mtkfb_lcm_name[strlen(videolfb_tag->lcmname)] = '\0';

		lcd_fps = videolfb_tag->fps;
		if (0 == lcd_fps)
			lcd_fps = 6000;

		islcmconnected = videolfb_tag->islcmfound;
		vramsize = videolfb_tag->vram;
		fb_base = videolfb_tag->fb_base;
		is_lcm_inited = 1;
		return 0;
	}

	DISPMSG("[DT][videolfb] videolfb_tag not found\n");
	return -1;
}

/* Retrun value: 0: success, 1: fail */
int _parse_tag_videolfb(void)
{
	int ret;
	unsigned long node = 0;

	if (is_videofb_parse_done)
		return 0;
#ifdef MTK_NO_DISP_IN_LK
	DISPMSG("[DT][videolfb] zaikuo, workaround for LK not ready\n"); /* after LK ready, remove this code */
	return 1;
#endif

	if (of_scan_flat_dt(fb_early_init_dt_get_chosen, &node) > 0) {
		ret = __parse_tag_videolfb(node);
		if (ret)
			__parse_tag_videolfb_extra(node);

		if (!ret) {
			is_videofb_parse_done = 1;

			DISPMSG("[DT][videolfb] lcmfound=%d, fps=%d, fb_base=%p, vram=%d, lcmname=%s\n",
			     islcmconnected, lcd_fps, (void *)fb_base, vramsize, mtkfb_lcm_name);
			return 0;
		}

		DISPMSG("[DT][videolfb] videolfb_tag not found\n");
		return 1;
	}

	DISPMSG("[DT][videolfb] of_chosen not found\n");
	return 1;
}

phys_addr_t mtkfb_get_fb_base(void)
{
	_parse_tag_videolfb();
	return fb_base;
}
EXPORT_SYMBOL(mtkfb_get_fb_base);

size_t mtkfb_get_fb_size(void)
{
	_parse_tag_videolfb();
	return vramsize;
}
EXPORT_SYMBOL(mtkfb_get_fb_size);
#endif

char *mtkfb_find_lcm_driver(void)
{

#ifdef CONFIG_OF
	if (1 == _parse_tag_videolfb()) {
		DISPMSG("[mtkfb] not found LCM driver, return NULL\n");
		return NULL;
	}
#else
	{
		char *p, *q;

		p = strstr(saved_command_line, "lcm=");
		/* we can't find lcm string in the command line, the uboot should be old version */
		if (p == NULL)
			return NULL;

		p += 6;
		if ((p - saved_command_line) > strlen(saved_command_line + 1))
			return NULL;

		pr_debug("%s, %s\n", __func__, p);
		q = p;
		while (*q != ' ' && *q != '\0')
			q++;

		memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
		strncpy((char *)mtkfb_lcm_name, (const char *)p, (int)(q - p));
		mtkfb_lcm_name[q - p + 1] = '\0';
	}
#endif
	/* printk("%s, %s\n", __func__, mtkfb_lcm_name); */
	return mtkfb_lcm_name;
}

uint32_t color = 0;
unsigned int mtkfb_fm_auto_test(void)
{
	unsigned int result = 0;
	unsigned int i = 0;
	unsigned long fbVirAddr;
	uint32_t fbsize;
	int r = 0;
	unsigned int *fb_buffer;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	struct fb_var_screeninfo var;

	fbVirAddr = (unsigned long)fbdev->fb_va_base;
	fb_buffer = (unsigned int *)fbVirAddr;

	memcpy(&var, &(mtkfb_fbi->var), sizeof(var));
	var.activate = FB_ACTIVATE_NOW;
	var.bits_per_pixel = 32;
	var.transp.offset = 24;
	var.transp.length = 8;
	var.red.offset = 16;
	var.red.length = 8;
	var.green.offset = 8;
	var.green.length = 8;
	var.blue.offset = 0;
	var.blue.length = 8;

	r = mtkfb_check_var(&var, mtkfb_fbi);
	if (r != 0)
		PRNERR("failed to mtkfb_check_var\n");

	mtkfb_fbi->var = var;

#if 0
	r = mtkfb_set_par(mtkfb_fbi);

	if (r != 0)
		PRNERR("failed to mtkfb_set_par\n");
#endif
	if (color == 0)
		color = 0xFF00FF00;
	fbsize = ALIGN_TO(DISP_GetScreenWidth(), MTK_FB_ALIGNMENT) * DISP_GetScreenHeight() * MTK_FB_PAGES;
	for (i = 0; i < fbsize; i++)
		*fb_buffer++ = color;
#if 0
	if (!primary_display_is_video_mode())
		primary_display_trigger(1, NULL, 0);
#endif
	mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
	msleep(100);

	result = primary_display_lcm_ATA();

	if (result == 0)
		DISPERR("ATA LCM failed\n");
	else
		DISPMSG("ATA LCM passed\n");

	return result;
}


static void _mtkfb_draw_block(unsigned long addr, unsigned int x, unsigned int y, unsigned int w,
			      unsigned int h, unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr + MTK_FB_XRESV * 4 * y + x * 4;

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++)
			mt_reg_sync_writel(color, (start_addr + i * 4 + j * MTK_FB_XRESV * 4));

	}
}

#if 0
static long int get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}
#endif

static int _mtkfb_internal_test(unsigned long va, unsigned int w, unsigned int h)
{
	/* this is for debug, used in bring up day */
	unsigned int i = 0;
	unsigned int color = 0;
	int _internal_test_block_size = 120;

	for (i = 0; i < w * h / _internal_test_block_size / _internal_test_block_size; i++) {
		color = (i & 0x1) * 0xff;
		/* color += ((i&0x2)>>1)*0xff00; */
		/* color += ((i&0x4)>>2)*0xff0000; */
		color += 0xff000000U;
		_mtkfb_draw_block(va, i % (w / _internal_test_block_size) * _internal_test_block_size,
				  i / (w / _internal_test_block_size) * _internal_test_block_size,
				  _internal_test_block_size, _internal_test_block_size, color);
	}
	/* unsigned long ttt = get_current_time_us(); */
	/* for (i = 0; i < 1000; i++) */
	primary_display_trigger(1, NULL, 0);
	/* ttt = get_current_time_us() - ttt; */
	/* DISPMSG("%s, update 1000 times, fps=%2d.%2d\n",
		   __func__, (1000*100/(ttt/1000/1000))/100, (1000*100/(ttt/1000/1000))%100);
	 */
	/* return 0; */

	_internal_test_block_size = 20;
	for (i = 0; i < w * h / _internal_test_block_size / _internal_test_block_size; i++) {
		color = (i & 0x1) * 0xff;
		color += ((i & 0x2) >> 1) * 0xff00;
		color += ((i & 0x4) >> 2) * 0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va, i % (w / _internal_test_block_size) * _internal_test_block_size,
				  i / (w / _internal_test_block_size) * _internal_test_block_size,
				  _internal_test_block_size, _internal_test_block_size, color);
	}
	primary_display_trigger(1, NULL, 0);
	_internal_test_block_size = 30;
	for (i = 0; i < w * h / _internal_test_block_size / _internal_test_block_size; i++) {
		color = (i & 0x1) * 0xff;
		color += ((i & 0x2) >> 1) * 0xff00;
		color += ((i & 0x4) >> 2) * 0xff0000;
		color += 0xff000000U;
		_mtkfb_draw_block(va, i % (w / _internal_test_block_size) * _internal_test_block_size,
				  i / (w / _internal_test_block_size) * _internal_test_block_size,
				  _internal_test_block_size, _internal_test_block_size, color);
	}
	primary_display_trigger(1, NULL, 0);

	return 0;
}

/* used when early porting, test pan display*/
int pan_display_test(int frame_num, int bpp)
{
	int i, j;
	int Bpp = bpp / 8;
	unsigned char *fb_va;
	unsigned long fb_pa;
	unsigned int fb_size;
	int w, h, fb_h;
	int yoffset_max;
	int yoffset;

	mtkfb_fbi->var.yoffset = 0;
	disp_get_fb_address((unsigned long *)&fb_va, &fb_pa);
	fb_size = mtkfb_fbi->fix.smem_len;
	w = mtkfb_fbi->var.xres;
	h = mtkfb_fbi->var.yres;
	fb_h = fb_size / (w * Bpp) - 10;

	DISPMSG("%s: frame_num=%d,bpp=%d, w=%d,h=%d,fb_h=%d\n",
		__func__, frame_num, bpp, w, h, fb_h);

	for (i = 0; i < fb_h; i++)
		for (j = 0; j < w; j++) {
			int x = (i * w + j) * Bpp;

			fb_va[x++] = (i + j) % 256;
			fb_va[x++] = (i + j) % 256;
			fb_va[x++] = (i + j) % 256;
			if (Bpp == 4)
				fb_va[x++] = 255;
		}

	mtkfb_fbi->var.bits_per_pixel = bpp;

	yoffset_max = fb_h - h;
	yoffset = 0;
	for (i = 0; i < frame_num; i++, yoffset += 10) {

		if (yoffset >= yoffset_max)
			yoffset = 0;

		mtkfb_fbi->var.xoffset = 0;
		mtkfb_fbi->var.yoffset = yoffset;
		mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
	}

	return 0;
}

/* #define FPGA_DEBUG_PAN */
#ifdef FPGA_DEBUG_PAN
static int update_test_kthread(void *data)
{
	/* struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE }; */
	/* sched_setscheduler(current, SCHED_RR, &param); */
	unsigned int i = 0, j = 0;
	unsigned long fb_va;
	unsigned long fb_pa;
	unsigned int *fb_start;
	unsigned int fbsize = primary_display_get_height() * primary_display_get_width();

	mtkfb_fbi->var.yoffset = 0;
	disp_get_fb_address(&fb_va, &fb_pa);

	for (;;) {
		if (kthread_should_stop())
			break;
		msleep(1000); /* 2s */
		pr_debug("update test thread work,offset = %d\n", i);

		mtkfb_fbi->var.yoffset = 0;
		disp_get_fb_address(&fb_va, &fb_pa);
		fb_start = (unsigned int *)fb_va;
		for (j = 0; j < fbsize; j++) {
			*fb_start = (0x55) << ((i % 4) * 8);
			fb_start++;
		}
		mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
		i++;
	}

	MTKFB_LOG("exit esd_recovery_kthread()\n");
	return 0;
}
#endif

static int mtkfb_probe(struct device *dev)
{
	struct mtkfb_device *fbdev = NULL;
	struct fb_info *fbi;
	int init_state;
	int r = 0;
#ifdef DISP_GPIO_DTS
	struct platform_device *pdev;
	long dts_gpio_state = 0;
#endif
	/* DISPFUNC(); */
	DISPMSG("%s\n", __func__);

#ifdef CONFIG_OF
	_parse_tag_videolfb();
#else
	{
		char *p = NULL;

		pr_debug("%s, %s\n", __func__, saved_command_line);
		p = strstr(saved_command_line, "fps=");
		if (p == NULL) {
			lcd_fps = 6000;
			pr_debug("[FB driver]can not get fps from uboot\n");
		} else {
			p += 4;
			r = kstrtol(p, 10, &lcd_fps);
			if (r)
				pr_err("DISP/%s: errno %d\n", __func__, r);
			if (0 == lcd_fps)
				lcd_fps = 6000;
		}
	}
#endif

	init_state = 0;

#ifdef DISP_GPIO_DTS
	pdev = to_platform_device(dev);
	/* repo call DTS gpio module, if not necessary, invoke nothing */
	dts_gpio_state = disp_dts_gpio_init_repo(pdev);
	if (dts_gpio_state != 0)
		dev_err(&pdev->dev, "retrieve GPIO DTS failed.");
#endif

	fbi = framebuffer_alloc(sizeof(struct mtkfb_device), dev);
	if (!fbi) {
		DISPERR("unable to allocate memory for device info\n");
		r = -ENOMEM;
		goto cleanup;
	}

	fbdev = (struct mtkfb_device *)fbi->par;
	fbdev->fb_info = fbi;
	fbdev->dev = dev;
	dev_set_drvdata(dev, fbdev);

	{
#ifndef MTK_NO_DISP_IN_LK
#ifdef CONFIG_OF
		/* printk("mtkfb_probe:get FB MEM REG\n"); */
		_parse_tag_videolfb();
		/* printk("mtkfb_probe: fb_pa = %p\n",(void *)fb_base); */

		disp_hal_allocate_framebuffer(fb_base, (fb_base + vramsize - 1),
					      (unsigned long *)&fbdev->fb_va_base, &fb_pa);
		fbdev->fb_pa_base = fb_base;

#else
		struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

		vramsize = res->end - res->start + 1;
		/* ASSERT(DISP_GetVRamSize() <= (res->end - res->start + 1)); */
		disp_hal_allocate_framebuffer(res->start, res->end,
					      (unsigned int *)&fbdev->fb_va_base, &fb_pa);
		fbdev->fb_pa_base = res->start;
#endif
#else
		{
			struct resource res;

			unsigned long fb_mem_addr_pa = 0;
			unsigned long fb_mem_addr_va = 0;

			pr_debug("mtkfb_probe:get FB MEM REG\n");

			if (0 != of_address_to_resource(dev->of_node, 0, &res)) {
				r = -ENOMEM;
				goto cleanup;
			}
			fb_mem_addr_pa = res.start;

			fb_mem_addr_va = (unsigned long)of_iomap(dev->of_node, 0);

			pr_debug("mtkfb_probe: fb_pa = 0x%lx, fb_va = 0x%lx\n", fb_mem_addr_pa,
				 fb_mem_addr_va);

			disp_hal_allocate_framebuffer(res.start, res.end,
						      (unsigned int *)&fbdev->fb_va_base, &fb_pa);
			fbdev->fb_pa_base = res.start;
			fbdev->fb_va_base = fb_mem_addr_va;
		}
#endif
	}
	primary_display_set_frame_buffer_address((unsigned long)fbdev->fb_va_base, fb_pa);

	/* mtkfb should parse lcm name from kernel boot command line */
	primary_display_init(mtkfb_find_lcm_driver(), lcd_fps, is_lcm_inited);

	init_state++; /* 1 */
	MTK_FB_XRES = primary_display_get_width();
	MTK_FB_YRES = primary_display_get_height();
	fb_xres_update = MTK_FB_XRES;
	fb_yres_update = MTK_FB_YRES;

	MTK_FB_BPP = primary_display_get_bpp();
	MTK_FB_PAGES = primary_display_get_pages();
	DISPMSG("MTK_FB_XRES=%d, MTKFB_YRES=%d, MTKFB_BPP=%d, MTK_FB_PAGES=%d, MTKFB_LINE=%d, MTKFB_SIZEV=%d\n",
		  MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	fbdev->fb_size_in_byte = MTK_FB_SIZEV;

	/* Allocate and initialize video frame buffer */
	DISPMSG("[FB Driver] fbdev->fb_pa_base = %p, fbdev->fb_va_base = %p\n",
		  &(fbdev->fb_pa_base), fbdev->fb_va_base);

	if (!fbdev->fb_va_base) {
		DISPERR("unable to allocate memory for frame buffer\n");
		r = -ENOMEM;
		goto cleanup;
	}
	init_state++; /* 2 */

	/* Register to system */

	r = mtkfb_fbinfo_init(fbi);
	if (r) {
		DISPERR("mtkfb_fbinfo_init fail, r = %d\n", r);
		goto cleanup;
	}
	init_state++; /* 4 */
	mtkfb_fbi = fbi;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		/* dal_init should after mtkfb_fbinfo_init, otherwise layer 3 will show dal background color */
		DAL_STATUS ret;
		unsigned long fbVA = (unsigned long)fbdev->fb_va_base;
		unsigned long fbPA = fb_pa;
		/* DAL init here */
		fbVA += DISP_GetFBRamSize();
		fbPA += DISP_GetFBRamSize();
		ret = DAL_Init(fbVA, fbPA);
	}

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		_mtkfb_internal_test((unsigned long)(fbdev->fb_va_base), MTK_FB_XRES, MTK_FB_YRES);


	r = mtkfb_register_sysfs(fbdev);
	if (r) {
		DISPERR("mtkfb_register_sysfs fail, r = %d\n", r);
		goto cleanup;
	}
	init_state++; /* 5 */

	r = register_framebuffer(fbi);
	if (r != 0) {
		DISPERR("register_framebuffer failed\n");
		goto cleanup;
	}

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		primary_display_diagnose();


	/* this function will get fb_heap base address to ion for management frame buffer */
	ion_drv_create_FB_heap(mtkfb_get_fb_base(), mtkfb_get_fb_size() - DAL_GetLayerSize());
	fbdev->state = MTKFB_ACTIVE;

#ifdef FPGA_DEBUG_PAN
	{
		struct task_struct *update_test_task = NULL;

		update_test_task = kthread_create(update_test_kthread, NULL, "update_test_kthread");

		if (IS_ERR(update_test_task))
			MTKFB_LOG("update test task create fail\n");
		else
			wake_up_process(update_test_task);
	}
#endif

	MSG_FUNC_LEAVE();
	return 0;

cleanup:
	mtkfb_free_resources(fbdev, init_state);

	/* printk("mtkfb_probe end\n"); */
	return r;
}

/* Called when the device is being detached from the driver */
static int mtkfb_remove(struct device *dev)
{
	struct mtkfb_device *fbdev = dev_get_drvdata(dev);
	enum mtkfb_state saved_state = fbdev->state;

	MSG_FUNC_ENTER();
	/* FIXME: wait till completion of pending events */

	fbdev->state = MTKFB_DISABLED;
	mtkfb_free_resources(fbdev, saved_state);

	MSG_FUNC_LEAVE();
	return 0;
}

/* PM suspend */
static int mtkfb_suspend(struct device *pdev, pm_message_t mesg)
{
	/* NOT_REFERENCED(pdev); */
	MSG_FUNC_ENTER();
	MTKFB_LOG("[FB Driver] mtkfb_suspend(): 0x%x\n", mesg.event);
	ovl2mem_wait_done();

	MSG_FUNC_LEAVE();
	return 0;
}

bool mtkfb_is_suspend(void)
{
	return primary_display_is_sleepd();
}
EXPORT_SYMBOL(mtkfb_is_suspend);

int mtkfb_ipoh_restore(struct notifier_block *nb, unsigned long val, void *ign)
{
	switch (val) {
	case PM_RESTORE_PREPARE:
		primary_display_ipoh_restore();
		pr_debug("[FB Driver] mtkfb_ipoh_restore PM_RESTORE_PREPARE\n");
		return NOTIFY_DONE;
	case PM_POST_RESTORE:
		primary_display_ipoh_recover();
		pr_debug("[FB Driver] %s pm_event: %lu\n", __func__, val);
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

int mtkfb_ipo_init(void)
{
	pm_nb.notifier_call = mtkfb_ipoh_restore;
	pm_nb.priority = 0;
	register_pm_notifier(&pm_nb);
	return 0;
}

static void mtkfb_shutdown(struct device *pdev)
{
	PRNWARN("[FB Driver] mtkfb_shutdown()\n");
	/* mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF); */
	if (!lcd_fps)
		msleep(30);
	else
		msleep(2 * 100000 / lcd_fps);	/* Delay 2 frames. */

	if (primary_display_is_sleepd()) {
		PRNWARN("mtkfb has been power off\n");
		return;
	}
	primary_display_suspend();
	PRNWARN("[FB Driver] leave mtkfb_shutdown\n");
}

void mtkfb_clear_lcm(void)
{
}

static void mtkfb_blank_suspend(void)
{
	int ret = 0;

	MSG_FUNC_ENTER();

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;

#ifdef CONFIG_SINGLE_PANEL_OUTPUT
	is_early_suspended = true;
#endif

	DISPMSG("[FB Driver] enter early_suspend\n");
#ifdef CONFIG_MTK_LEDS
/* mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF); */
#endif
	msleep(30);

	ret = primary_display_suspend();

	if (ret < 0) {
		DISPERR("primary display suspend failed\n");
		return;
	}
	DISPMSG("[FB Driver] leave early_suspend\n");
}

/* PM resume */
static int mtkfb_resume(struct device *pdev)
{
	/* NOT_REFERENCED(pdev); */
	MSG_FUNC_ENTER();
	MTKFB_LOG("[FB Driver] mtkfb_resume()\n");
	MSG_FUNC_LEAVE();
	return 0;
}

static void mtkfb_blank_resume(void)
{
	int ret = 0;

	MSG_FUNC_ENTER();

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;

	PRNWARN("[FB Driver] enter late_resume\n");

#ifdef CONFIG_SINGLE_PANEL_OUTPUT
	is_early_suspended = false;
#endif

	ret = primary_display_resume();

	if (ret) {
		DISPERR("primary display resume failed\n");
		return;
	}

	PRNWARN("[FB Driver] leave late_resume\n");
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int mtkfb_pm_suspend(struct device *device)
{
	/* pr_debug("calling %s()\n", __func__); */
	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return mtkfb_suspend((struct device *)pdev, PMSG_SUSPEND);
}

int mtkfb_pm_resume(struct device *device)
{
	/* pr_debug("calling %s()\n", __func__); */
	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return mtkfb_resume((struct device *)pdev);
}

int mtkfb_pm_freeze(struct device *device)
{
	primary_display_esd_check_enable(0);
	return 0;
}

int mtkfb_pm_restore_noirq(struct device *device)
{
	DISPMSG("%s: %d\n", __func__, __LINE__);

	is_ipoh_bootup = true;
#ifndef CONFIG_MTK_CLKMGR
	dpmgr_path_power_on(primary_get_dpmgr_handle(), CMDQ_DISABLE);
#endif
	return 0;
}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define mtkfb_pm_suspend NULL
#define mtkfb_pm_resume  NULL
#define mtkfb_pm_restore_noirq NULL
#define mtkfb_pm_freeze NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
static const struct of_device_id mtkfb_of_ids[] = {
	{.compatible = "mediatek,mtkfb",},
	{}
};

const struct dev_pm_ops mtkfb_pm_ops = {
	.suspend = mtkfb_pm_suspend,
	.resume = mtkfb_pm_resume,
	.freeze = mtkfb_pm_freeze,
	.thaw = mtkfb_pm_resume,
	.poweroff = mtkfb_pm_suspend,
	.restore = mtkfb_pm_resume,
	.restore_noirq = mtkfb_pm_restore_noirq,
};

static struct platform_driver mtkfb_driver = {
	.driver = {
		.name = MTKFB_DRIVER,
#ifdef CONFIG_PM
		.pm = &mtkfb_pm_ops,
#endif
		.bus = &platform_bus_type,
		.probe = mtkfb_probe,
		.remove = mtkfb_remove,
		.suspend = mtkfb_suspend,
		.resume = mtkfb_resume,
		.shutdown = mtkfb_shutdown,
		.of_match_table = mtkfb_of_ids,
	},
};

#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mtkfb_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = mtkfb_early_suspend,
	.resume = mtkfb_late_resume,
};
#endif
#endif


int mtkfb_get_debug_state(char *stringbuf, int buf_len)
{
	int len = 0;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;

	unsigned long va = (unsigned long)fbdev->fb_va_base;
	unsigned long mva = (unsigned long)fbdev->fb_pa_base;
	unsigned long pa = fbdev->fb_pa_base;
	unsigned int resv_size = vramsize;

	len += scnprintf(stringbuf + len, buf_len - len,
		      "|--------------------------------------------------------------------------------------|\n");
	/* len += scnprintf(stringbuf+len, buf_len - len, "********MTKFB Driver General Information********\n"); */
	len += scnprintf(stringbuf + len, buf_len - len,
		      "|Framebuffer VA:0x%lx, PA:0x%lx, MVA:0x%lx, Reserved Size:0x%08x|%d\n", va,
		      pa, mva, resv_size, resv_size);
	len += scnprintf(stringbuf + len, buf_len - len, "|xoffset=%d, yoffset=%d\n",
		      mtkfb_fbi->var.xoffset, mtkfb_fbi->var.yoffset);
	len += scnprintf(stringbuf + len, buf_len - len, "|framebuffer line alignment(for gpu)=%d\n",
		      MTK_FB_ALIGNMENT);
	len += scnprintf(stringbuf + len, buf_len - len,
		      "|xres=%d, yres=%d,bpp=%d,pages=%d,linebytes=%d,total size=%d\n", MTK_FB_XRES,
		      MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	/* use extern in case DAL_LOCK is hold, then can't get any debug info */
	len += scnprintf(stringbuf + len, buf_len - len, "|AEE Layer is %s\n",
		      isAEEEnabled ? "enabled" : "disabled");

	return len;
}


/* Register both the driver and the device */
int __init mtkfb_init(void)
{
	int r = 0;

	MSG_FUNC_ENTER();

	if (platform_driver_register(&mtkfb_driver)) {
		PRNERR("failed to register mtkfb driver\n");
		r = -ENODEV;
		goto exit;
	}
#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mtkfb_early_suspend_handler);
#endif
#endif
	/* FIXME: find definition */
	PanelMaster_Init();

	DBG_Init();
	mtkfb_ipo_init();
exit:
	MSG_FUNC_LEAVE();
	return r;
}


static void __exit mtkfb_cleanup(void)
{
	MSG_FUNC_ENTER();

	platform_driver_unregister(&mtkfb_driver);

#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mtkfb_early_suspend_handler);
#endif
#endif

	/* FIXME: find definition of PanelMaster_Deinit */
	/* PanelMaster_Deinit(); */

	DBG_Deinit();

	MSG_FUNC_LEAVE();
}
module_init(mtkfb_init);
module_exit(mtkfb_cleanup);
MODULE_DESCRIPTION("MEDIATEK framebuffer driver");
MODULE_AUTHOR("Xuecheng Zhang <Xuecheng.Zhang@mediatek.com>");
MODULE_LICENSE("GPL");

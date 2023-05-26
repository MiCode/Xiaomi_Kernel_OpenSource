/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <generated/autoconf.h>
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
#include "disp_assert_layer.h"
#include <linux/semaphore.h>
#include <linux/mutex.h>
/* #include <linux/leds-mt65xx.h> */
#include <linux/suspend.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
/* #include <asm/mach-types.h> */
#include <asm/cacheflush.h>
#include <linux/io.h>
#include "ion_drv.h"
/*#include "mt-plat/dma.h"*/
/* #include <mach/irqs.h> */
#include <linux/dma-mapping.h>
#include <linux/compat.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#  include "mt-plat/aee.h"
#endif
#include "mt-plat/mtk_boot.h"
#include "debug.h"
#include "ddp_hal.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "mtkfb.h"
#include "mtkfb_console.h"
#include "mtkfb_fence.h"
#include "mtkfb_info.h"
#include "ddp_ovl.h"
#include "disp_drv_platform.h"
#include "primary_display.h"
#include "ddp_dump.h"
#include "display_recorder.h"
#include "fbconfig_kdebug.h"
#include "mtk_ovl.h"
#include "mtk_boot.h"
#include "disp_helper.h"
#include "compat_mtkfb.h"
#include "disp_dts_gpio.h"
#include "disp_recovery.h"
#include "ddp_clkmgr.h"
#include "ddp_log.h"
#include "ddp_m4u.h"
#include "disp_lowpower.h"

#ifdef MTKFB_SUPPORT_SECOND_DISP
#  include "extd_multi_control.h"
#endif
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
#  include "external_display.h"
#endif

#ifndef _UAPI__ASMARM_SETUP_H
#define _UAPI__ASMARM_SETUP_H
#endif

#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

/* include for mmpath auto gen */
#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "mmpath.h"

/* static variable */
static u32 MTK_FB_XRES;
static u32 MTK_FB_YRES;
static u32 MTK_FB_BPP;
static u32 MTK_FB_PAGES;
static u32 fb_xres_update;
static u32 fb_yres_update;
static size_t mtkfb_log_on = true;

static int sem_flipping_cnt = 1;
static int sem_early_suspend_cnt = 1;
static int vsync_cnt;
static const struct timeval FRAME_INTERVAL = { 0, 30000 };	/* 33ms */
static bool no_update;
static struct disp_session_input_config session_input;
long dts_gpio_state;

/* macro definiton */
#define ALIGN_TO(x, n)  (((x) + ((n) - 1)) & ~((n) - 1))
#define MTK_FB_XRESV (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT))
/* For page flipping */
#define MTK_FB_YRESV (ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * MTK_FB_PAGES)
#define MTK_FB_BYPP  ((MTK_FB_BPP + 7) >> 3)
#define MTK_FB_LINE  (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) * MTK_FB_BYPP)
#define MTK_FB_SIZE  (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT))
#define MTK_FB_SIZEV (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * \
		      MTK_FB_PAGES)
#define ASSERT_LAYER (DDP_OVL_LAYER_MUN-1)
#define DISP_DEFAULT_UI_LAYER_ID (DDP_OVL_LAYER_MUN-1)
#define DISP_CHANGED_UI_LAYER_ID (DDP_OVL_LAYER_MUN-2)
#define NOT_REFERENCED(x)	{ (x) = (x); }

#ifdef CONFIG_MTK_AEE_FEATURE
#  define CHECK_RET(expr)					\
do {								\
	int ret = (expr);					\
	aee_kernel_exception("mtkfb", "[DISP]error:%s,%d",	\
			     __FILE__, __LINE__);		\
} while (0)
#else
#  define CHECK_RET(expr)
#endif

#define MTKFB_LOG(fmt, arg...)					\
	do {							\
		if (mtkfb_log_on)				\
			DISP_LOG_PRINT(ANDROID_LOG_WARN, "MTKFB", fmt, ##arg); \
	} while (0)
/* always show this debug info while the global debug log is off */
#define MTKFB_LOG_DBG(fmt, arg...)				\
	do {							\
		if (!mtkfb_log_on)				\
			DISP_LOG_PRINT(ANDROID_LOG_WARN, "MTKFB", fmt, ##arg); \
	} while (0)

#define MTKFB_FUNC()						\
	do {							\
		if (mtkfb_log_on)				\
			DISP_LOG_PRINT(ANDROID_LOG_INFO, "MTKFB", \
				       "[Func]%s\n", __func__);	\
	} while (0)

#define PRNERR(fmt, args...)					\
		DISP_LOG_PRINT(ANDROID_LOG_INFO, "MTKFB", fmt, ## args)

/* ------------------------------------------------------------ */
/* local variables */
/* ------------------------------------------------------------ */
struct notifier_block pm_nb;
unsigned int EnableVSyncLog;
unsigned long fb_mva;
atomic_t has_pending_update = ATOMIC_INIT(0);
struct fb_overlay_layer video_layerInfo;
UINT32 dbr_backup;
UINT32 dbg_backup;
UINT32 dbb_backup;
bool fblayer_dither_needed;
bool is_ipoh_bootup;
struct fb_info *mtkfb_fbi;
struct fb_overlay_layer fb_layer_context;
struct mtk_dispif_info dispif_info[MTKFB_MAX_DISPLAY_COUNT];
unsigned int FB_LAYER = 2;
bool is_early_suspended = FALSE;
atomic_t OverlaySettingDirtyFlag = ATOMIC_INIT(0);
atomic_t OverlaySettingApplied = ATOMIC_INIT(0);
unsigned int PanDispSettingPending;
unsigned int PanDispSettingDirty;
unsigned int PanDispSettingApplied;
unsigned int need_esd_check;
unsigned int lcd_fps = 6000;
wait_queue_head_t screen_update_wq;
char mtkfb_lcm_name[256] = { 0 };
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
struct fb_info *ext_mtkfb_fb;
unsigned long ext_fb_pa;

unsigned int ext_lcd_fps = 6000;
char ext_mtkfb_lcm_name[256] = { 0 };
#endif

DEFINE_SEMAPHORE(sem_flipping);
DEFINE_SEMAPHORE(sem_early_suspend);
DEFINE_SEMAPHORE(sem_overlay_buffer);

/* ----------------------------------------------------------- */
/* local function declarations */
/* ----------------------------------------------------------- */
static int mtkfb_set_par(struct fb_info *fbi);
static int init_framebuffer(struct fb_info *info);
static int mtkfb_get_overlay_layer_info(
	struct fb_overlay_layer_info *layerInfo);

#ifdef CONFIG_OF
static int _parse_tag_videolfb(void);
#endif

static void mtkfb_late_resume(void);
static void mtkfb_early_suspend(void);

void mtkfb_log_enable(int enable)
{
	mtkfb_log_on = enable;
	MTKFB_LOG("mtkfb log %s\n", enable ? "enabled" : "disabled");
}

/*
 * ----------------------------------------------------------
 * fbdev framework callbacks and the ioctl interface
 * ----------------------------------------------------------
 */
/* Called each time the mtkfb device is opened */
static int mtkfb_open(struct fb_info *info, int user)
{
	NOT_REFERENCED(info);
	NOT_REFERENCED(user);
	DISPFUNC();
	MSG_FUNC_ENTER();
	MSG_FUNC_LEAVE();
	return 0;
}

/* Called when the mtkfb device is closed. We make sure that any pending*/
/* gfx DMA operations are ended, before we return. */
static int mtkfb_release(struct fb_info *info, int user)
{
	NOT_REFERENCED(info);
	NOT_REFERENCED(user);
	DISPFUNC();

	MSG_FUNC_ENTER();
	MSG_FUNC_LEAVE();
	return 0;
}

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
static int mtkfb1_blank(int blank_mode, struct fb_info *info)
{
	pr_debug("%s blank mode :%d\n", __func__, blank_mode);
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		/*
		 * parameter 0 is to all external display should be resumed,
		 * 0x20003 is dual lcm session
		 */
		external_display_resume(0x20003);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		/*
		 * parameter 0 is to all external display should be suspend,
		 * 0x20003 is dual lcm session
		 */
		external_display_suspend(0x20003);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static int mtkfb_blank(int blank_mode, struct fb_info *info)
{
	enum mtkfb_power_mode prev_pm = primary_display_get_power_mode();
	DISPDBG("%s, blank_mode=%d\n", __func__, blank_mode);
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		DISPDBG("%s mtkfb_late_resume\n", __func__);
		if (bypass_blank) {
			DISP_PR_INFO("FB_BLANK_UNBLANK bypass_blank %d\n",
				     bypass_blank);
			break;
		}

		primary_display_set_power_mode(FB_RESUME);
		mtkfb_late_resume();

		debug_print_power_mode_check(prev_pm, FB_RESUME);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
		DISPDBG("%s mtkfb_early_suspend\n", __func__);
		if (bypass_blank) {
			DISP_PR_INFO("FB_BLANK_POWERDOWN bypass_blank %d\n",
				     bypass_blank);
			break;
		}
		if (prev_pm != FB_SUSPEND) {
			primary_display_set_power_mode(FB_SUSPEND);
			mtkfb_early_suspend();
		}

		debug_print_power_mode_check(prev_pm, FB_SUSPEND);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int mtkfb_set_backlight_level(unsigned int level)
{
	bool aal_is_support = disp_aal_is_support();

	MTKFB_FUNC();

	DISPDBG("%s:%d Start\n", __func__, level);

	if (aal_is_support)
		primary_display_setbacklight_nolock(level);
	else
		primary_display_setbacklight(level);

	DISPDBG("%s End\n", __func__);
	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
int mtkfb1_set_backlight_level(unsigned int level)
{
	MTKFB_FUNC();
/*	DISPDBG("mtkfb1_set_backlight_level:%d Start\n", level); */
	external_display_setbacklight(level);
/*	DISPDBG("mtkfb1_set_backlight_level End\n"); */
	return 0;
}
EXPORT_SYMBOL(mtkfb1_set_backlight_level);
#endif

int mtkfb_set_backlight_mode(unsigned int mode)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_flipping)) {
		DISP_PR_ERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		return -ERESTARTSYS;
	}
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
		DISP_PR_ERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		sem_flipping_cnt++;
		up(&sem_flipping);
		return -ERESTARTSYS;
	}

	sem_early_suspend_cnt--;
	if (primary_display_is_sleepd())
		goto end;

	/* DISP_SetBacklight_mode(mode); */
end:
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
		DISP_PR_ERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		return -ERESTARTSYS;
	}
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
		DISP_PR_ERR("[FB Driver] can't get semaphore:%d\n", __LINE__);
		sem_flipping_cnt++;
		up(&sem_flipping);
		return -ERESTARTSYS;
	}
	sem_early_suspend_cnt--;
	if (primary_display_is_sleepd())
		goto end;
	/* DISP_SetPWM(div); */

end:
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

static int _convert_fb_layer_to_disp_input(struct fb_overlay_layer *src,
					   struct disp_input_config *dst)
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

	default:
		DISP_PR_INFO("Invalid color format: 0x%x\n", src->src_fmt);
		return -1;
	}

	dst->src_base_addr = src->src_base_addr;
	dst->security = src->security;
	dst->src_phy_addr = src->src_phy_addr;
	DISPDBG("%s, dst->addr=0x%p\n", __func__, dst->src_phy_addr);

	dst->isTdshp = src->isTdshp;
	dst->next_buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

	/* set Alpha blending */
	dst->alpha = src->alpha;
	if (src->src_fmt == MTK_FB_FORMAT_ARGB8888 ||
		src->src_fmt == MTK_FB_FORMAT_ABGR8888)
		dst->alpha_enable = TRUE;
	else
		dst->alpha_enable = FALSE;


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
	dst->ext_sel_layer = -1;
#if 1
	DISPDBG(
	"%s:id=%u,en=%u,next_idx=%u,vaddr=%p,pa=%p,srcfmt=%u,dstfmt=%u,pitch=%u\n",
		__func__, dst->layer_id, dst->layer_enable, dst->next_buff_idx,
		dst->src_base_addr, dst->src_phy_addr,
		src->src_fmt, dst->src_fmt, dst->src_pitch);
	DISPDBG(
		"%s:src(%u,%u,%ux%u),target(%u,%u,%ux%u),aen=%u\n",
		__func__, dst->src_offset_x, dst->src_offset_y,
		dst->src_width, dst->src_height,
		dst->tgt_offset_x, dst->tgt_offset_y,
		dst->tgt_width, dst->tgt_height, dst->alpha_enable);
#endif

	return 0;
}
/*FPGA show pic*/
static int mtkfb_pan_display_impl(struct fb_var_screeninfo *var,
				  struct fb_info *info)
{
	UINT32 offset = 0;
	UINT32 paStart = 0;
	char *vaStart = NULL, *vaEnd = NULL;
	int ret = 0;
	/* int wait_ret = 0; */
	/* unsigned int layerpitch = 0; */
	unsigned int src_pitch = 0;
	struct disp_session_input_config *session_input;
	struct disp_input_config *input;

	/* DISPFUNC(); */

	if (no_update) {
		DISPMSG("the first time of %s will be ignored\n", __func__);
		return ret;
	}

	DISPCHECK("pan_display: offset(%u,%u), res(%u,%u), resv(%u,%u)\n",
		  var->xoffset, var->yoffset, info->var.xres, info->var.yres,
		  info->var.xres_virtual, info->var.yres_virtual);

	info->var.yoffset = var->yoffset;
	offset = var->yoffset * info->fix.line_length;
	paStart = fb_mva + offset;
	vaStart = info->screen_base + offset;
	vaEnd = vaStart + info->var.yres * info->fix.line_length;

	DISPCHECK("fb dump: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		  *(unsigned int *)vaStart, *(unsigned int *)(vaStart+4),
		  *(unsigned int *)(vaStart+8), *(unsigned int *)(vaStart+0xC));

	session_input = kzalloc(sizeof(*session_input), GFP_KERNEL);
	if (!session_input) {
		DISP_PR_ERR("session input allocate fail\n");
		ASSERT(0);
		return -1;
	}

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
	input->ext_sel_layer = -1;
	switch (var->bits_per_pixel) {
	case 16:
		input->src_fmt = DISP_FORMAT_RGB565;
		break;
	case 24:
		input->src_fmt = DISP_FORMAT_RGB888;
		break;
	case 32:
		input->src_fmt =
			(var->blue.offset == 0) ?
			DISP_FORMAT_BGRA8888 : DISP_FORMAT_RGBX8888;
		break;
	default:
		DISP_PR_INFO("Invalid color format bpp: %d\n",
			     var->bits_per_pixel);
		kfree(session_input);
		return -1;
	}
	input->alpha_enable = FALSE;

	input->alpha = 0xFF;
	input->next_buff_idx = -1;
	src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
	input->src_pitch = src_pitch;
	input->ext_sel_layer = -1;

	session_input->config_layer_num++;

	session_input->setter = SESSION_USER_PANDISP;

	if (!is_DAL_Enabled()) {
		/* disable font layer(layer3) drawed in lk */
		session_input->config[1].layer_id =
				primary_display_get_option("ASSERT_LAYER");
		session_input->config[1].next_buff_idx = -1;
		session_input->config[1].layer_enable = 0;
		session_input->config_layer_num++;
	}
	ret = primary_display_config_input_multiple(session_input);
	ret = primary_display_trigger(TRUE, NULL, 0);

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
		DISP_PR_ERR("set fb fix fail,bits per pixel=%d\n",
			    var->bits_per_pixel);
		return;
	}

	fix->accel = FB_ACCEL_NONE;
	fix->line_length = ALIGN_TO(var->xres_virtual, MTK_FB_ALIGNMENT) *
				var->bits_per_pixel / 8;
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

	DISPCHECK(
	"%s:xres=%u,yres=%u,x_virt=%u,y_virt=%u,xoffset=%u,yoffset=%u,bits_per_pixel=%u)\n",
		  __func__, var->xres, var->yres,
		  var->xres_virtual, var->yres_virtual,
		  var->xoffset, var->yoffset, var->bits_per_pixel);

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
	DISPDBG("fbdev->fb_size_in_byte=0x%08lx\n",
		fbdev->fb_size_in_byte);
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
	DISPDBG(
		"%s:xres=%u,yres=%u,x_virt=%u,y_virl=%u,xoffset=%u,yoffset=%u,bits_per_pixel=%u)\n",
		__func__, var->xres, var->yres,
		var->xres_virtual, var->yres_virtual,
		var->xoffset, var->yoffset, var->bits_per_pixel);
	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	DISPMSG(
		"%s:xres=%u,yres=%u,x_virt=%u,y_virt=%u,xoffset=%u,yoffset=%u,bits_per_pixel=%u)\n",
		__func__, var->xres, var->yres,
		var->xres_virtual, var->yres_virtual,
		var->xoffset, var->yoffset, var->bits_per_pixel);

	if (bpp == 16) {
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
	} else if (bpp == 24) {
		var->red.length = var->green.length = var->blue.length = 8;
		var->transp.length = 0;

		ASSERT(var->green.offset == 8);
		ASSERT((var->red.offset + var->blue.offset) == 16);
		ASSERT((var->red.offset == 16 || var->red.offset == 0));
	} else if (bpp == 32) {
		var->red.length = var->green.length =
				var->blue.length = var->transp.length = 8;

		ASSERT(var->red.offset + var->blue.offset == 16);
		ASSERT((var->red.offset == 16 || var->red.offset == 0));
	}

	var->red.msb_right = var->green.msb_right =
			var->blue.msb_right = var->transp.msb_right = 0;

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

/**
 * Switch to a new mode. The parameters for it has been check already by
 * mtkfb_check_var.
 */
static int mtkfb_set_par(struct fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->var;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;
	struct fb_overlay_layer fb_layer;
	u32 bpp = var->bits_per_pixel;
	struct disp_session_input_config *session_input;
	struct disp_input_config *input;

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
		fb_layer.src_fmt = (var->blue.offset == 0) ?
				MTK_FB_FORMAT_RGB888 : MTK_FB_FORMAT_BGR888;
		fb_layer.src_color_key = 0xFF000000;
		break;

	case 32:
		fb_layer.src_use_color_key = 0;
		DISPDBG("set_par,var->blue.offset=%d\n", var->blue.offset);
		fb_layer.src_fmt = (var->blue.offset == 0) ?
				MTK_FB_FORMAT_ARGB8888 : MTK_FB_FORMAT_ABGR8888;
		fb_layer.src_color_key = 0;
		break;

	default:
		fb_layer.src_fmt = MTK_FB_FORMAT_UNKNOWN;
		DISP_PR_INFO("[%s]unsupported bpp: %d", __func__, bpp);
		return -1;
	}

	set_fb_fix(fbdev);

	fb_layer.layer_id = primary_display_get_option("FB_LAYER");
	fb_layer.layer_enable = 1;
	fb_layer.src_base_addr = (void *)((unsigned long)fbdev->fb_va_base +
					var->yoffset * fbi->fix.line_length);
	DISPDBG("fb_pa=0x%08lx,var->yoffset=%u,fbi->fix.line_length=%u\n",
		fb_mva, var->yoffset, fbi->fix.line_length);
	fb_layer.src_phy_addr = (void *)(fb_mva + var->yoffset *
					 fbi->fix.line_length);
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
	DISPDBG("%s, fb_layer.src_fmt=%x\n", __func__, fb_layer.src_fmt);

	session_input = kzalloc(sizeof(*session_input), GFP_KERNEL);
	if (!session_input)
		goto out;

	session_input->config_layer_num = 0;

	if (!is_DAL_Enabled()) {
		int layer_num;

		DISPCHECK("AEE is not enabled, will disable layer 3\n");
		layer_num = session_input->config_layer_num;
		input =	&session_input->config[layer_num];
		session_input->config_layer_num++;
		input->layer_id = primary_display_get_option("ASSERT_LAYER");
		input->layer_enable = 0;
	} else {
		DISPCHECK("AEE is enabled, should not disable layer 3\n");
	}

	input = &session_input->config[session_input->config_layer_num++];
	_convert_fb_layer_to_disp_input(&fb_layer, input);
	session_input->setter = SESSION_USER_INVALID;
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
	NOT_REFERENCED(info);
	NOT_REFERENCED(cursor);

	return 0;
}

static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info *layerInfo)
{
	return 0;
}

void mtkfb_dump_layer_info(void)
{
}

UINT32 color;

unsigned int mtkfb_fm_auto_test(void)
{
	unsigned int result = 0;
	unsigned int i = 0;
	unsigned long fbVirAddr;
	UINT32 fbsize;
	int ret = 0;
	unsigned int *fb_buffer;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	struct fb_var_screeninfo var;

	int idle_state_backup = disp_helper_get_option(DISP_OPT_IDLE_MGR);

	if (primary_display_is_sleepd()) {
		DISP_PR_INFO("primary display path is already sleep, skip\n");
		return 0;
	}

	if (idle_state_backup) {
		primary_display_idlemgr_kick(__func__, 1);
		enable_idlemgr(0);
	}

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

	ret = mtkfb_check_var(&var, mtkfb_fbi);
	if (ret)
		PRNERR("failed to mtkfb_check_var\n");

	mtkfb_fbi->var = var;

	if (color == 0)
		color = 0xFF00FF00;
	fbsize =
	    ALIGN_TO(DISP_GetScreenWidth(),
		     MTK_FB_ALIGNMENT) * DISP_GetScreenHeight() * MTK_FB_PAGES;
	for (i = 0; i < fbsize; i++)
		*fb_buffer++ = color;

	mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
	msleep(100);

	primary_display_idlemgr_kick(__func__, 1);
	result = primary_display_lcm_ATA();

	if (idle_state_backup)
		enable_idlemgr(1);

	if (result == 0)
		DISP_PR_ERR("ATA LCM failed\n");
	else
		DISPMSG("ATA LCM passed\n");

	return result;
}

int mtkfb_aod_mode_switch(enum mtkfb_aod_power_mode aod_pm)
{
	int ret = 0;
	enum mtkfb_power_mode prev_pm = primary_display_get_power_mode();

	DISPCHECK("AOD: ioctl: %s\n",
		(aod_pm != 0) ? "AOD_DOZE_SUSPEND" : "AOD_DOZE");
	if (!primary_is_aod_supported()) {
		DISPCHECK("AOD: feature not support\n");
		return ret;
	}

	if (aod_pm == MTKFB_AOD_DOZE_SUSPEND) {
		/*
		 * First DOZE to power on dispsys and LCM(low power mode);
		 * then DOZE_SUSPEND to power off dispsys.
		 */
		if (primary_display_is_sleepd() &&
			primary_display_get_lcm_power_state()) {
			primary_display_set_power_mode(DOZE);
			primary_display_resume();

			debug_print_power_mode_check(prev_pm, DOZE);
		}

		primary_display_set_power_mode(DOZE_SUSPEND);
		ret = primary_display_suspend();

		debug_print_power_mode_check(prev_pm, DOZE_SUSPEND);
	} else if (aod_pm == MTKFB_AOD_DOZE) {
		primary_display_set_power_mode(DOZE);
		ret = primary_display_resume();

		debug_print_power_mode_check(prev_pm, DOZE);
	} else {
		DISP_PR_ERR("AOD: error: unknown AOD power mode %d\n", aod_pm);
	}
	if (ret < 0)
		DISP_PR_ERR("AOD: set %s failed\n",
			(aod_pm != MTKFB_AOD_DOZE) ? "AOD_SUSPEND" : "AOD_RESUME");
	return ret;
}

static int mtkfb_ioctl(struct fb_info *info, unsigned int cmd,
		       unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	enum DISP_STATUS stat = 0;
	int ret = 0;

	DISPFUNC();
	/* M: dump debug mmprofile log info */
	DISPDBG("%s: info=%p, cmd nr=0x%08x, cmd size=0x%08x\n", __func__,
		info, (unsigned int)_IOC_NR(cmd), (unsigned int)_IOC_SIZE(cmd));

	switch (cmd) {
	case MTKFB_GET_FRAMEBUFFER_MVA:
		return copy_to_user(argp, &fb_mva, sizeof(fb_mva)) ?
			-EFAULT : 0;

	case MTKFB_GET_DISPLAY_IF_INFORMATION:
	{
		int displayid = 0;

		if (copy_from_user(&displayid, (void __user *)arg,
				   sizeof(displayid))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n",
				  __LINE__);
			return -EFAULT;
		}

		if (displayid == 0) {
			dispif_info[displayid].displayWidth =
						primary_display_get_width();
			dispif_info[displayid].displayHeight =
						primary_display_get_height();

			dispif_info[displayid].lcmOriginalWidth =
				    primary_display_get_original_width();
			dispif_info[displayid].lcmOriginalHeight =
				    primary_display_get_original_height();
			dispif_info[displayid].displayMode =
				    primary_display_is_video_mode() ? 0 : 1;
		} else {
			DISP_PR_ERR(
				"information for displayid: %d is not available now\n",
				    displayid);
			return -EFAULT;
		}

		if (copy_to_user((void __user *)arg, &(dispif_info[displayid]),
				 sizeof(struct mtk_dispif_info))) {
			MTKFB_LOG("[FB]: copy_to_user failed! line:%d\n",
				  __LINE__);
			ret = -EFAULT;
		}

		return ret;
	}
	case MTKFB_SET_AOD_POWER_MODE:
	{
		enum mtkfb_aod_power_mode aod_pm = MTKFB_AOD_POWER_MODE_ERROR;

		aod_pm = (enum mtkfb_aod_power_mode)arg;
		ret = mtkfb_aod_mode_switch(arg);
		break;
	}
	case MTKFB_POWEROFF:
	{
		MTKFB_FUNC();
		if (primary_display_is_sleepd()) {
			DISP_PR_INFO(
				"[FB Driver] is still in MTKFB_POWEROFF!\n");
			return ret;
		}

		DISPDBG("[FB Driver] enter MTKFB_POWEROFF\n");
		/* TODO: remove unnessecary IOCTL
		 * It will call SurfaceFlinger blank before this that
		 * do the same thing, so it is trivial now.
		 */
		/*ret = primary_display_suspend();*/
		if (stat < 0)
			DISP_PR_ERR("primary display suspend failed\n");
		DISPDBG("[FB Driver] leave MTKFB_POWEROFF\n");

		is_early_suspended = TRUE; /* no care */
		return ret;
	}
	case MTKFB_POWERON:
	{
		MTKFB_FUNC();
		if (primary_display_is_alive()) {
			DISP_PR_INFO(
				"[FB Driver] is still in MTKFB_POWERON!\n");
			return ret;
		}
		DISPDBG("[FB Driver] enter MTKFB_POWERON\n");
		/* TODO: remove unnessecary IOCTL
		 * It will call SurfaceFlinger unblank after this that
		 * do the same thing, so it is trivial now.
		 */
		/*primary_display_resume();*/
		DISPDBG("[FB Driver] leave MTKFB_POWERON\n");
		is_early_suspended = FALSE; /* no care */
		return ret;
	}
	case MTKFB_GET_POWERSTATE:
	{
		int power_state;

		if (primary_display_is_sleepd())
			power_state = 0;
		else
			power_state = 1;

		if (copy_to_user(argp, &power_state, sizeof(power_state))) {
			pr_err("MTKFB_GET_POWERSTATE failed\n");
			return -EFAULT;
		}

		return 0;
	}
	case MTKFB_CONFIG_IMMEDIATE_UPDATE:
	{
		MTKFB_LOG("[%s] MTKFB_CONFIG_IMMEDIATE_UPDATE, enable = %lu\n",
			  __func__, arg);
		if (down_interruptible(&sem_early_suspend)) {
			MTKFB_LOG("[%s] can't get semaphore:%d\n",
				  __func__, __LINE__);
			return -ERESTARTSYS;
		}
		sem_early_suspend_cnt--;
		up(&sem_early_suspend);
		return ret;
	}
	case MTKFB_CAPTURE_FRAMEBUFFER:
	{
		return ret;
	}
	case MTKFB_SLT_AUTO_CAPTURE:
	{
		return ret;
	}
	case MTKFB_GET_OVERLAY_LAYER_INFO:
	{
		struct fb_overlay_layer_info layerInfo;

		MTKFB_LOG(
			" %s():MTKFB_GET_OVERLAY_LAYER_INFO\n", __func__);

		if (copy_from_user(&layerInfo, (void __user *)arg,
				   sizeof(layerInfo))) {
			MTKFB_LOG(
				"[FB]: copy_from_user failed! line:%d\n",
				  __LINE__);
			return -EFAULT;
		}
		if (mtkfb_get_overlay_layer_info(&layerInfo) < 0) {
			MTKFB_LOG(
				"[FB]: Failed to get overlay layer info\n");
			return -EFAULT;
		}
		if (copy_to_user((void __user *)arg, &layerInfo,
				 sizeof(layerInfo))) {
			MTKFB_LOG(
				"[FB]: copy_to_user failed! line:%d\n",
				  __LINE__);
			ret = -EFAULT;
		}
		return ret;
	}
	case MTKFB_SET_OVERLAY_LAYER:
	{		/* no function */
		struct fb_overlay_layer *layerInfo;
		struct disp_input_config *input;
		int layer_num;

		DISPMSG(" %s():MTKFB_SET_OVERLAY_LAYER\n", __func__);

		layerInfo = kmalloc(sizeof(*layerInfo), GFP_KERNEL);
		if (!layerInfo)
			return -ENOMEM;

		if (copy_from_user(layerInfo, (void __user *)arg,
				   sizeof(*layerInfo))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n",
				  __LINE__);
			kfree(layerInfo);
			return -EFAULT;
		}

		/*
		 * in early suspend mode, will not update buffer index,
		 * info SF by return value
		 */
		if (primary_display_is_sleepd()) {
			DISP_PR_INFO(
				"[FB] set overlay in early suspend ,skip!\n");
			kfree(layerInfo);
			return MTKFB_ERROR_IS_EARLY_SUSPEND;
		}

		memset((void *)&session_input, 0, sizeof(session_input));
		layer_num = session_input.config_layer_num;
		input = &session_input.config[layer_num];
		session_input.config_layer_num++;
		session_input.setter = SESSION_USER_PANDISP;
		_convert_fb_layer_to_disp_input(layerInfo, input);
		primary_display_config_input_multiple(&session_input);
		primary_display_trigger(1, NULL, 0);

		kfree(layerInfo);
		return ret;
	}
	case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT:
	{
		pr_info("[DDP] %s():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT\n",
			__func__);
		/* call info dump function here */
		/* mtkfb_dump_layer_info(); */
		return ret;
	}
	case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT_AEE:
	{
		pr_info("[DDP] %s():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT\n",
			__func__);
		/* call info dump function here */
		/* mtkfb_dump_layer_info(); */
		return ret;
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
		int layerInfo_size = sizeof(struct fb_overlay_layer) *
							VIDEO_LAYER_COUNT;
		int32_t i;
		struct disp_input_config *input;
		int layer_num;

		DISPMSG(" %s():MTKFB_SET_VIDEO_LAYERS\n", __func__);

		layerInfo = kmalloc(layerInfo_size, GFP_KERNEL);
		if (!layerInfo)
			return -ENOMEM;

		if (copy_from_user(layerInfo, (void __user *)arg,
				   layerInfo_size)) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n",
				  __LINE__);
			kfree(layerInfo);
			return -EFAULT;
		}

		memset((void *)&session_input, 0, sizeof(session_input));
		for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
			if (layerInfo[i].layer_id >= TOTAL_OVL_LAYER_NUM) {
				DISP_PR_INFO(
					"MTKFB_SET_VIDEO_LAYERS, layer_id invalid=%d\n",
					     layerInfo[i].layer_id);
				continue;
			}

			layer_num = session_input.config_layer_num;
			input = &session_input.config[layer_num];
			session_input.config_layer_num++;
			_convert_fb_layer_to_disp_input(&layerInfo[i], input);
		}
		session_input.setter = SESSION_USER_PANDISP;
		primary_display_config_input_multiple(&session_input);
		primary_display_trigger(1, NULL, 0);
		kfree(layerInfo);

		return ret;
	}
	case MTKFB_TRIG_OVERLAY_OUT:
	{
		DISPMSG(" %s():MTKFB_TRIG_OVERLAY_OUT\n", __func__);
		primary_display_trigger(1, NULL, 0);
		return 0;
	}
	case MTKFB_META_RESTORE_SCREEN:
	{
		struct fb_var_screeninfo var;

		if (copy_from_user(&var, argp, sizeof(var)))
			return -EFAULT;

		/* invalidate params from userspace */
		if (var.xres > MTK_FB_XRES ||
			var.yres > MTK_FB_YRES ||
			var.xres_virtual > MTK_FB_XRESV ||
			var.yres_virtual > MTK_FB_YRESV ||
			var.xoffset > MTK_FB_XRES ||
			var.yoffset > MTK_FB_YRESV * (MTK_FB_PAGES - 1))
			return -EFAULT;

		info->var.yoffset = var.yoffset;

		/* check var.yoffset passed by user space */
		if (info->var.yres + info->var.yoffset > info->var.yres_virtual)
			info->var.yoffset = info->var.yres_virtual -
						info->var.yres;

		init_framebuffer(info);

		return mtkfb_pan_display_impl(&var, info);
	}
	case MTKFB_GET_DEFAULT_UPDATESPEED:
	{
		unsigned int speed = 0;

		MTKFB_LOG("[MTKFB] get default update speed\n");

		DISPMSG("[MTKFB EM]MTKFB_GET_DEFAULT_UPDATESPEED is %d\n",
			speed);
		return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
	}
	case MTKFB_GET_CURR_UPDATESPEED:
	{
		unsigned int speed = 0;

		MTKFB_LOG("[MTKFB] get current update speed\n");

		DISPMSG("[MTKFB EM]MTKFB_GET_CURR_UPDATESPEED is %d\n", speed);
		return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
	}
	case MTKFB_CHANGE_UPDATESPEED:
	{
		unsigned int speed = 0;

		MTKFB_LOG("[MTKFB] change update speed\n");

		if (copy_from_user(&speed, (void __user *)arg, sizeof(speed))) {
			MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n",
				  __LINE__);
			ret = -EFAULT;
		} else {
			DISPMSG("[MTKFB EM]MTKFB_CHANGE_UPDATESPEED is %d\n",
				speed);

		}
		return ret;
	}
	case MTKFB_AEE_LAYER_EXIST:
	{
		int dal_en = is_DAL_Enabled();

		return copy_to_user(argp, &dal_en, sizeof(dal_en)) ?
					-EFAULT : 0;
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
		return copy_to_user(argp, &result, sizeof(result)) ?
					-EFAULT : 0;
	}
	case MTKFB_META_SHOW_BOOTLOGO:
	{
		int i, layer_num;
		struct mtkfb_device *fbdev = (struct mtkfb_device *)
						mtkfb_fbi->par;
		struct disp_input_config *input;

		DISPMSG("MTKFB_META_SHOW_BOOTLOGO\n");
		memset((void *)&session_input, 0, sizeof(session_input));

		for (i = 0; i < 2; i++) {
			layer_num = session_input.config_layer_num;
			input = &session_input.config[layer_num];
			session_input.config_layer_num++;

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

			input->src_pitch = ALIGN_TO(MTK_FB_XRES,
						    MTK_FB_ALIGNMENT) * 4;
			input->alpha_enable = 1;
			input->alpha = 0xff;
			input->next_buff_idx = -1;
		}

		input = &session_input.config[0];
		input->layer_id = 0;
		input->src_phy_addr = (void *)(unsigned long)
					(fbdev->fb_pa_base);

		input = &session_input.config[1];
		input->layer_id = 3;
		input->src_phy_addr = (void *)(unsigned long)
				(fbdev->fb_pa_base +
				 (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) *
				  ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * 4));

		session_input.setter = SESSION_USER_PANDISP;
		primary_display_config_input_multiple(&session_input);
		primary_display_trigger(1, NULL, 0);

		return 0;
	}
	default:
		DISP_PR_INFO(
			"%s: Not support:info=0x%p, cmd=0x%08x, arg=0x%08lx\n",
			     __func__, info, (unsigned int)cmd, arg);
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT

static void compat_convert(struct compat_fb_overlay_layer *compat_info,
			   struct fb_overlay_layer *info)
{
	info->layer_id = compat_info->layer_id;
	info->layer_enable = compat_info->layer_enable;
	info->src_base_addr = (void *)((unsigned long)
				       (compat_info->src_base_addr));
	info->src_phy_addr = (void *)((unsigned long)
				      (compat_info->src_phy_addr));
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

static int mtkfb_compat_ioctl(struct fb_info *info, unsigned int cmd,
			      unsigned long arg)
{
	struct fb_overlay_layer layerInfo;
	long ret = 0;

	DISPDBG("[FB Driver] %s: cmd=0x%08x, cmd nr=0x%08x, cmd size=0x%08x\n",
		__func__, cmd, (unsigned int)_IOC_NR(cmd),
		(unsigned int)_IOC_SIZE(cmd));

	switch (cmd) {
	case COMPAT_MTKFB_GET_FRAMEBUFFER_MVA:
	{
		compat_uint_t __user *data32;
		__u32 data;

		data32 = compat_ptr(arg);
		data = (__u32)fb_mva;
		if (put_user(data, data32)) {
			pr_err("MTKFB_FRAMEBUFFER_MVA failed\n");
			ret = -EFAULT;
		}
		DISPDBG("MTKFB_FRAMEBUFFER_MVA success 0x%lx\n", fb_mva);
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
		if (displayid >= MTKFB_MAX_DISPLAY_COUNT) {
			pr_err("[FB]: invalid display id:%d\n", displayid);
			return -EFAULT;
		}
		if (displayid == 0) {
			dispif_info[displayid].displayWidth =
						primary_display_get_width();
			dispif_info[displayid].displayHeight =
						primary_display_get_height();

			dispif_info[displayid].lcmOriginalWidth =
					primary_display_get_original_width();
			dispif_info[displayid].lcmOriginalHeight =
					primary_display_get_original_height();
			dispif_info[displayid].displayMode =
					primary_display_is_video_mode() ? 0 : 1;
		} else {
			DISP_PR_INFO(
				"information for displayid: %d is not available now\n",
				     displayid);
		}

		if (copy_to_user((void __user *)arg, &(dispif_info[displayid]),
				 sizeof(struct compat_mtk_dispif_info))) {
			pr_err("[FB]: copy_to_user failed! line:%d\n",
			       __LINE__);
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
		DISPDBG("MTKFB_GET_POWERSTATE success %d\n", power_state);
		break;
	}
	case COMPAT_MTKFB_CAPTURE_FRAMEBUFFER:
	{
		compat_ulong_t __user *data32;
		unsigned long *pbuf;
		compat_ulong_t l;

		data32 = compat_ptr(arg);
		pbuf = compat_alloc_user_space(sizeof(unsigned long));
		ret = get_user(l, data32);
		ret |= put_user(l, pbuf);
		ret = mtkfb_ioctl(info, MTKFB_CAPTURE_FRAMEBUFFER,
				  (unsigned long)pbuf);
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

		compat_layerInfo = kmalloc(sizeof(*compat_layerInfo),
					   GFP_KERNEL);
		if (!compat_layerInfo)
			return -ENOMEM;

		MTKFB_LOG(
			" %s():MTKFB_SET_OVERLAY_LAYER\n", __func__);

		arg = (unsigned long)compat_ptr(arg);
		if (copy_from_user(compat_layerInfo, (void __user *)arg,
				   sizeof(*compat_layerInfo))) {
			MTKFB_LOG(
				"[FB Driver]: copy_from_user failed! line:%d\n",
				  __LINE__);
			ret = -EFAULT;
		} else {
			struct disp_input_config *input;
			int layer_num;

			compat_convert(compat_layerInfo, &layerInfo);

			/*
			 * in early suspend mode ,will not update buffer index,
			 * info SF by return value
			 */
			if (primary_display_is_sleepd()) {
				DISP_PR_INFO(
					"[FB Driver] set overlay in early suspend ,skip!\n");
				kfree(compat_layerInfo);
				return MTKFB_ERROR_IS_EARLY_SUSPEND;
			}
			memset((void *)&session_input, 0,
			       sizeof(session_input));
			layer_num = session_input.config_layer_num;
			input = &session_input.config[layer_num];
			session_input.config_layer_num++;
			session_input.setter = SESSION_USER_PANDISP;
			_convert_fb_layer_to_disp_input(&layerInfo, input);
			primary_display_config_input_multiple(&session_input);
			/* primary_display_trigger(1, NULL, 0); */
		}
		kfree(compat_layerInfo);
		break;
	}
	case COMPAT_MTKFB_SET_VIDEO_LAYERS:
	{
		int32_t i;
		struct disp_input_config *input;
		struct compat_fb_overlay_layer *compat_layerInfo;
		int layer_num;
		int l_info_size = sizeof(struct compat_fb_overlay_layer) *
							VIDEO_LAYER_COUNT;

		compat_layerInfo = kmalloc(l_info_size, GFP_KERNEL);
		if (!compat_layerInfo)
			return -ENOMEM;

		MTKFB_LOG(
			" %s():MTKFB_SET_VIDEO_LAYERS\n", __func__);

		if (copy_from_user(compat_layerInfo, (void __user *)arg,
				   l_info_size)) {
			MTKFB_LOG(
				"[FB Driver]: copy_from_user failed! line:%d\n",
				  __LINE__);
			kfree(compat_layerInfo);
			return -EFAULT;
		}

		memset((void *)&session_input, 0, sizeof(session_input));
		for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
			compat_convert(&compat_layerInfo[i], &layerInfo);
			layer_num = session_input.config_layer_num;
			input = &session_input.config[layer_num];
			session_input.config_layer_num++;
			_convert_fb_layer_to_disp_input(&layerInfo, input);
		}
		session_input.setter = SESSION_USER_PANDISP;
		primary_display_config_input_multiple(&session_input);
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

static int mtkfb_pan_display_proxy(struct fb_var_screeninfo *var,
				   struct fb_info *info)
{
#ifdef CONFIG_MTPROF_APPLAUNCH	/* eng enable, user disable */
	LOG_PRINT(ANDROID_LOG_INFO, "AppLaunch", "mtkfb_pan_display_proxy.\n");
#endif
	return mtkfb_pan_display_impl(var, info);
}

/*
 * Callback table for the frame buffer framework. Some of these pointers
 * will be changed according to the current setting of fb_info->accel_flags.
 */
static struct fb_ops mtkfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = mtkfb_open,
	.fb_release = mtkfb_release,
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
	.fb_blank = mtkfb_blank,
};

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
/* external_display fb */
static struct fb_ops mtkfb1_ops = {
	.owner = THIS_MODULE,
	.fb_open = NULL,
	.fb_release = NULL,
	.fb_setcolreg = NULL,
	.fb_pan_display = NULL,
	.fb_fillrect = NULL,
	.fb_copyarea = NULL,
	.fb_imageblit = NULL,
	.fb_cursor = NULL,
	.fb_check_var = NULL,
	.fb_set_par = NULL,
	.fb_ioctl = NULL,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = NULL,
#endif
	.fb_blank = mtkfb1_blank,
};
#endif
/*
 * ---------------------------------------------------------------
 * Sysfs interface
 * ---------------------------------------------------------------
 */

static int mtkfb_register_sysfs(struct mtkfb_device *fbdev)
{
	NOT_REFERENCED(fbdev);

	return 0;
}

static void mtkfb_unregister_sysfs(struct mtkfb_device *fbdev)
{
	NOT_REFERENCED(fbdev);
}

/*
 * ----------------------------------------------------------------
 * LDM callbacks
 * ----------------------------------------------------------------
 */
/* Initialize system fb_info object and set the default video mode.
 * The frame buffer memory already allocated by lcddma_init
 */
static int mtkfb_fbinfo_init(struct fb_info *info)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;
	struct fb_var_screeninfo var;
	int ret = 0;

	DISPFUNC();

	ASSERT(fbdev->fb_va_base);
	info->fbops = &mtkfb_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->screen_base = (char *)fbdev->fb_va_base;
	info->screen_size = fbdev->fb_size_in_byte;
	info->pseudo_palette = fbdev->pseudo_palette;

	ret = fb_alloc_cmap(&info->cmap, 32, 0);
	if (ret)
		DISP_PR_ERR(
		"unable to allocate color map memory\n");

	/* setup the initial video mode (RGB565) */

	memset(&var, 0, sizeof(var));

	var.xres = MTK_FB_XRES;
	var.yres = MTK_FB_YRES;
	var.xres_virtual = MTK_FB_XRESV;
	var.yres_virtual = MTK_FB_YRESV;
	DISPMSG(
		"%s var.xres=%d,var.yres=%d,var.xres_virtual=%d,var.yres_virtual=%d\n",
		__func__, var.xres, var.yres, var.xres_virtual,
		var.yres_virtual);
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

	ret = mtkfb_check_var(&var, info);
	if (ret)
		DISP_PR_ERR("failed to mtkfb_check_var\n");

	info->var = var;

	ret = mtkfb_set_par(info);
	if (ret)
		DISP_PR_ERR("failed to mtkfb_set_par\n");

	MSG_FUNC_LEAVE();
	return ret;
}

/* Release the fb_info object */
static void mtkfb_fbinfo_cleanup(struct mtkfb_device *fbdev)
{
	MSG_FUNC_ENTER();

	fb_dealloc_cmap(&fbdev->fb_info->cmap);

	MSG_FUNC_LEAVE();
}

/* Init frame buffer content as 3 R/G/B color bars for debug */
static int init_framebuffer(struct fb_info *info)
{
	void *buffer;
	int size;
	struct fb_var_screeninfo *var = &info->var;

	buffer = info->screen_base + var->yoffset * info->fix.line_length;
	size = var->xres_virtual * var->yres * var->bits_per_pixel / 8;

	memset_io(buffer, 0, size);
	return 0;
}


/**
 * Free driver resources. Can be called to rollback an aborted initialization
 * sequence.
 */
static void mtkfb_free_resources(struct mtkfb_device *fbdev, int state)
{
	int ret = 0;

	switch (state) {
	case MTKFB_ACTIVE:
		ret = unregister_framebuffer(fbdev->fb_info);
		ASSERT(ret == 0);
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
		dma_free_coherent(0, fbdev->fb_size_in_byte, fbdev->fb_va_base,
				  fbdev->fb_pa_base);
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
		DISP_PR_ERR("free resources fail, state=%d\n", state);
		break;
	}
}

void disp_get_fb_address(unsigned long *fbVirAddr, unsigned long *fbPhysAddr)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	int fb_size;

	fb_size = mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
	*fbVirAddr = (unsigned long)fbdev->fb_va_base + fb_size;
	*fbPhysAddr = (unsigned long)fbdev->fb_pa_base + fb_size;
}

static void _mtkfb_draw_block(unsigned long addr, unsigned int x,
			      unsigned int y, unsigned int w, unsigned int h,
			      unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr + MTK_FB_XRESV * 4 * y + x * 4;

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++)
			mt_reg_sync_writel(color, (start_addr + i * 4 +
						   j * MTK_FB_XRESV * 4));
	}
}

char *mtkfb_find_lcm_driver(void)
{
	_parse_tag_videolfb();
	DISPMSG("%s, %s\n", __func__, mtkfb_lcm_name);
	return mtkfb_lcm_name;
}

int _mtkfb_internal_test(unsigned long va, unsigned int w, unsigned int h)
{
	/* this is for debug, used in bring up day */
	unsigned int i = 0;
	unsigned int color = 0;
	int block_sz = 120;

	for (i = 0; i < w * h / block_sz / block_sz; i++) {
		color = (i & 0x1) * 0xff;
#if 0
		color += ((i & 0x2) >> 1) * 0xff00;
		color += ((i & 0x4) >> 2) * 0xff0000;
#endif
		color += 0xff000000U;
		/* color = 0xff000000U; */
		_mtkfb_draw_block(va, i % (w / block_sz) * block_sz,
				  i / (w / block_sz) * block_sz,
				  block_sz, block_sz, color);
	}

	primary_display_trigger(1, NULL, 0);

	DISPFUNC();
	return 0;
}

#ifdef CONFIG_OF
struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];	/* this is the minimum size */
};

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
struct tag_ext_videolfb {
	u64 ext_fb_base;
	u32 ext_islcmfound;
	u32 ext_fps;
	u32 ext_vram;
	char ext_lcmname[1];	/* this is the minimum size */
};
#endif
unsigned int islcmconnected;
unsigned int is_lcm_inited;
unsigned int vramsize;
phys_addr_t fb_base;
static int is_videofb_parse_done;

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
unsigned int ext_islcmconnected;
unsigned int ext_is_lcm_inited;
unsigned int ext_vramsize;
phys_addr_t ext_fb_base;
#endif

static int __parse_tag_videolfb_extra(struct device_node *node)
{
	void *prop;
	unsigned long size = 0;
	u32 fb_base_h, fb_base_l;

	prop = (void *)of_get_property(node, "atag,videolfb-fb_base_h", NULL);
	if (!prop)
		return -1;
	fb_base_h = of_read_number(prop, 1);

	prop = (void *)of_get_property(node, "atag,videolfb-fb_base_l", NULL);
	if (!prop)
		return -1;
	fb_base_l = of_read_number(prop, 1);

	fb_base = ((u64) fb_base_h << 32) | (u64) fb_base_l;

	prop = (void *)of_get_property(node, "atag,videolfb-islcmfound", NULL);
	if (!prop)
		return -1;
	islcmconnected = of_read_number(prop, 1);

	prop = (void *)of_get_property(node, "atag,videolfb-islcm_inited",
				       NULL);
	if (!prop)
		is_lcm_inited = 1;
	else
		is_lcm_inited = of_read_number(prop, 1);

	prop = (void *)of_get_property(node, "atag,videolfb-fps", NULL);
	if (!prop)
		return -1;
	lcd_fps = of_read_number(prop, 1);
	if (lcd_fps == 0)
		lcd_fps = 6000;

	prop = (void *)of_get_property(node, "atag,videolfb-vramSize", NULL);
	if (!prop)
		return -1;
	vramsize = of_read_number(prop, 1);

	prop = (void *)of_get_property(node, "atag,videolfb-fb_base_l", NULL);
	if (!prop)
		return -1;
	fb_base_l = of_read_number(prop, 1);

	prop = (void *)of_get_property(node, "atag,videolfb-lcmname",
				       (int *)&size);
	if (!prop)
		return -1;
	if (size >= sizeof(mtkfb_lcm_name)) {
		DISPCHECK("%s: error to get lcmname size=%ld\n",
			  __func__, size);
		return -1;
	}
	memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char *)mtkfb_lcm_name, prop, sizeof(mtkfb_lcm_name));
	mtkfb_lcm_name[size] = '\0';
	DISPMSG("%s done\n", __func__);
	return 0;
}

int __parse_tag_videolfb(struct device_node *node)
{
	struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;

	videolfb_tag = (struct tag_videolfb *)
			of_get_property(node, "atag,videolfb", (int *)&size);
	if (videolfb_tag) {
		memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
		strncpy((char *)mtkfb_lcm_name, videolfb_tag->lcmname,
			sizeof(mtkfb_lcm_name));
		mtkfb_lcm_name[strlen(videolfb_tag->lcmname)] = '\0';

		lcd_fps = videolfb_tag->fps;
		if (lcd_fps == 0)
			lcd_fps = 6000;

		islcmconnected = videolfb_tag->islcmfound;
		vramsize = videolfb_tag->vram;
		fb_base = videolfb_tag->fb_base;
		is_lcm_inited = 1;

		return 0;
	}

	DISPCHECK("[DT][videolfb] videolfb_tag not found\n");
	return -1;
}

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
int __init __parse_tag_ext_videolfb(unsigned long node)
{
	struct tag_ext_videolfb *tag_ext_videolfb = NULL;
	unsigned long size = 0;
	char *lcm_name;

	tag_ext_videolfb = (struct tag_ext_videolfb *)
				of_get_flat_dt_prop(node, "atag,ext_videolfb",
						    (int *)&size);
	if (tag_ext_videolfb) {
		lcm_name = tag_ext_videolfb->ext_lcmname;
		memset((void *)ext_mtkfb_lcm_name, 0,
			sizeof(ext_mtkfb_lcm_name));
		strcpy((char *)ext_mtkfb_lcm_name, lcm_name);
		ext_mtkfb_lcm_name[strlen(lcm_name)] = '\0';

		ext_lcd_fps = tag_ext_videolfb->ext_fps;
		if (ext_lcd_fps == 0)
			ext_lcd_fps = 6000;

		ext_islcmconnected = tag_ext_videolfb->ext_islcmfound;
		ext_vramsize = tag_ext_videolfb->ext_vram;
		ext_fb_base = tag_ext_videolfb->ext_fb_base;
		ext_is_lcm_inited = 1;

		return 0;
	}

	DISPCHECK("[DT][ext_videolfb] tag_ext_videolfb not found\n");
	return -1;
}
#endif

static int _parse_tag_videolfb(void)
{
	int ret;
	struct device_node *chosen_node;

	DISPCHECK("[DT][videolfb]isvideofb_parse_done = %d\n",
		  is_videofb_parse_done);

	if (is_videofb_parse_done)
		return 0;

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node)
		chosen_node = of_find_node_by_path("/chosen@0");

	if (chosen_node) {
		ret = __parse_tag_videolfb(chosen_node);
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
		ret = __parse_tag_ext_videolfb(chosen_node);
#endif
		if (!ret)
			goto found;
		ret = __parse_tag_videolfb_extra(chosen_node);
		if (!ret)
			goto found;
	} else {
		DISPCHECK("[DT][videolfb] of_chosen not found\n");
	}
	return -1;

found:
	is_videofb_parse_done = 1;
	DISPCHECK("[DT][videolfb] islcmfound = %d\n",
		islcmconnected);
	DISPCHECK("[DT][videolfb] is_lcm_inited = %d\n",
		is_lcm_inited);
	DISPCHECK("[DT][videolfb] fps        = %d\n",
		lcd_fps);
	DISPCHECK("[DT][videolfb] fb_base    = 0x%lx\n",
		(unsigned long)fb_base);
	DISPCHECK("[DT][videolfb] vram       = 0x%x (%d)\n",
		vramsize, vramsize);
	DISPCHECK("[DT][videolfb] lcmname    = %s\n",
		mtkfb_lcm_name);

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	DISPCHECK("[DT][videolfb] ext_islcmfound = %d\n",
		ext_islcmconnected);
	DISPCHECK("[DT][videolfb] ext_is_lcm_inited = %d\n",
		ext_is_lcm_inited);
	DISPCHECK("[DT][videolfb] ext_fps	 = %d\n",
		ext_lcd_fps);
	DISPCHECK("[DT][videolfb] ext_fb_base	 = 0x%lx\n",
		(unsigned long)ext_fb_base);
	DISPCHECK("[DT][videolfb] ext_vram	 = 0x%x (%d)\n",
		ext_vramsize, ext_vramsize);
	DISPCHECK("[DT][videolfb] ext_lcmname	 = %s\n",
		ext_mtkfb_lcm_name);
#endif
	return 0;
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

/* used when early porting, test pan display */
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
		fb_h = fb_size / (ALIGN_TO(w, MTK_FB_ALIGNMENT) * Bpp) - 10;

	DISPMSG("%s: frame_num=%d,bpp=%d, w=%d,h=%d,fb_h=%d\n",
		__func__, frame_num, bpp, w, h, fb_h);

	for (i = 0; i < fb_h; i++)
		for (j = 0; j < w; j++) {
			int x = (i * ALIGN_TO(w, MTK_FB_ALIGNMENT) + j) * Bpp;

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
static struct task_struct *test_task;

static int update_test_kthread(void *data)
{
	unsigned int i = 0, j = 0;
	unsigned long fb_va;
	unsigned long fb_pa;
	unsigned int *fb_start;
	unsigned int fbsize = primary_display_get_height() *
				primary_display_get_width();

	mtkfb_fbi->var.yoffset = 0;
	disp_get_fb_address(&fb_va, &fb_pa);

	for (;;) {

		if (kthread_should_stop())
			break;
		msleep(1000);	/* 2s */
		DISPMSG("update test thread work,offset = %d\n", i);

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

	DISPMSG("exit %s()\n", __func__);
	return 0;
}
#endif

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
static struct fb_info *allocate_fb_by_index(struct device *dev)
{
	struct mtkfb_device *fbdev = NULL;
	struct fb_info *fb_dev = NULL;

	fb_dev = framebuffer_alloc(sizeof(struct mtkfb_device), dev);
	if (!fb_dev) {
		DISP_PR_ERR("unable to allocate memory for device info\n");
		return NULL;
	}

	fbdev = (struct mtkfb_device *)fb_dev->par;
	fbdev->fb_info = fb_dev;
	fbdev->dev = dev;
	/* dev_set_drvdata(dev, fbdev); */

	fb_dev->fbops = &mtkfb1_ops;
	fb_dev->flags = FBINFO_FLAG_DEFAULT;

	return fb_dev;
}
#endif

static int mtkfb_probe(struct platform_device *pdev)
{
	struct mtkfb_device *fbdev = NULL;
	struct fb_info *fbi;
#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	/* external display fb */
	struct fb_info *fb1;
#endif
	int init_state;
	int ret = 0;

#ifdef CONFIG_MTK_IOMMU_V2
	struct ion_client *ion_display_client = NULL;
	struct ion_handle *ion_display_handle = NULL;
	size_t temp_va = 0;
#endif
	/* struct platform_device *pdev; */

	DISPMSG("%s name [%s] = [%s][%p]\n", __func__,
		pdev->name, pdev->dev.init_name, (void *)&pdev->dev);

#ifdef CONFIG_MTK_SMI_EXT
	if (!smi_mm_first_get()) {
		DISPMSG("SMI not start probe\n");
		return -EPROBE_DEFER;
	}
#endif

	_parse_tag_videolfb();

	init_state = 0;

	/* pdev = to_platform_device(dev); */
	/* repo call DTS gpio module, if not necessary, invoke nothing */
	dts_gpio_state = disp_dts_gpio_init_repo(pdev);
	if (dts_gpio_state)
		dev_err(&pdev->dev, "retrieve GPIO DTS failed.");

	fbi = framebuffer_alloc(sizeof(struct mtkfb_device), &(pdev->dev));
	if (!fbi) {
		DISP_PR_ERR("unable to allocate memory for device info\n");
		ret = -ENOMEM;
		goto cleanup;
	}
	mtkfb_fbi = fbi;

	fbdev = (struct mtkfb_device *)fbi->par;
	fbdev->fb_info = fbi;
	fbdev->dev = &(pdev->dev);
	dev_set_drvdata(&(pdev->dev), fbdev);

	DISPMSG("%s: fb_pa = %pa\n", __func__, &fb_base);

	disp_hal_allocate_framebuffer(fb_base, (fb_base + vramsize - 1),
				(unsigned long *)(&fbdev->fb_va_base), &fb_mva);

	fbdev->fb_pa_base = fb_base;

	primary_display_set_frame_buffer_address((unsigned long)
						 (fbdev->fb_va_base),
						 fb_mva, fb_base);
	primary_display_init(mtkfb_find_lcm_driver(), lcd_fps, is_lcm_inited);

	init_state++; /* 1 */
	MTK_FB_XRES = DISP_GetScreenWidth();
	MTK_FB_YRES = DISP_GetScreenHeight();
	fb_xres_update = MTK_FB_XRES;
	fb_yres_update = MTK_FB_YRES;

	MTK_FB_BPP = DISP_GetScreenBpp();
	MTK_FB_PAGES = DISP_GetPages();
	DISPCHECK(
		"MTK_FB_XRES=%d, MTKFB_YRES=%d, MTKFB_BPP=%d, MTK_FB_PAGES=%d, MTKFB_LINE=%d, MTKFB_SIZEV=%d\n",
		  MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES,
		  MTK_FB_LINE, MTK_FB_SIZEV);
	fbdev->fb_size_in_byte = MTK_FB_SIZEV;

	/* allocate and initialize video frame buffer */
	DISPCHECK(
	"[FB Driver] fbdev->fb_pa_base=0x%p,fbdev->fb_va_base=0x%p\n",
		  &(fbdev->fb_pa_base), fbdev->fb_va_base);

	if (!fbdev->fb_va_base) {
		DISP_PR_ERR(
			"unable to allocate memory for frame buffer\n");
		ret = -ENOMEM;
		goto cleanup;
	}
	init_state++; /* 2 */

	ret = mtkfb_fbinfo_init(fbi);
	if (ret) {
		DISP_PR_ERR(
			"mtkfb_fbinfo_init fail, ret = %d\n", ret);
		goto cleanup;
	}
	init_state++; /* 4 */
	DISPMSG("mtkfb_fbinfo_init done\n");

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		/*
		 * dal_init should after mtkfb_fbinfo_init, otherwise layer
		 * 3 will show dal background color
		 */
		enum DAL_STATUS ret;
		unsigned long fbVA = (unsigned long)(fbdev->fb_va_base);
		unsigned long fbMVA = fb_mva;

		/* DAL init here */
		fbVA += DISP_GetFBRamSize();
		fbMVA += DISP_GetFBRamSize();
		ret = DAL_Init(fbVA, fbMVA);
		DISPMSG("DAL_Init done\n");
	}

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		_mtkfb_internal_test((unsigned long)(fbdev->fb_va_base),
				     MTK_FB_XRES, MTK_FB_YRES);

	ret = mtkfb_register_sysfs(fbdev);
	if (ret) {
		DISP_PR_ERR("mtkfb_register_sysfs fail, ret = %d\n", ret);
		goto cleanup;
	}
	init_state++; /* 5 */

	DISPMSG("register_framebuffer start...\n");
	ret = register_framebuffer(fbi);
	if (ret) {
		DISP_PR_ERR("register_framebuffer failed\n");
		goto cleanup;
	}
	DISPMSG("register_framebuffer done\n");

#if defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)
	DISPDBG("%s register fb1\n", __func__);
	/* register external display fb */
	fb1 = allocate_fb_by_index(&pdev->dev);
	register_framebuffer(fb1);
	DISPMSG("register_ext_framebuffer done\n");
#endif

#ifdef FPGA_DEBUG_PAN
	test_task = kthread_create(update_test_kthread, NULL,
				   "update_test_kthread");
	wake_up_process(test_task);
#endif

#if 0
	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		primary_display_diagnose(__func__, __LINE__);
#else
	primary_display_diagnose(__func__, __LINE__);
#endif
/*disp decouple will use this buffer*/
#if 0
	/*
	 * this function will get fb_heap base address to ion
	 * for management frame buffer
	 */
//#ifdef MTK_FB_ION_SUPPORT
	ion_drv_create_FB_heap(mtkfb_get_fb_base(), DISP_GetFBRamSize());
	pr_info("%s DISP_GetFBRamSize size:%d\n",
		__func__, DISP_GetFBRamSize());
#endif
	fbdev->state = MTKFB_ACTIVE;

	MSG_FUNC_LEAVE();
	pr_info("disp driver(2) %s end\n", __func__);
	return 0;

cleanup:
	mtkfb_free_resources(fbdev, init_state);

	pr_info("disp driver(3) %s end\n", __func__);
	return ret;
}

/* Called when the device is being detached from the driver */
static int mtkfb_remove(struct platform_device *pdev)
{
	struct mtkfb_device *fbdev = dev_get_drvdata(&pdev->dev);
	enum mtkfb_state saved_state = fbdev->state;

	MSG_FUNC_ENTER();
	/* FIXME: wait till completion of pending events */

	fbdev->state = MTKFB_DISABLED;
	mtkfb_free_resources(fbdev, saved_state);

	MSG_FUNC_LEAVE();
	return 0;
}

/* PM suspend */
static int mtkfb_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	DISPFUNC();
	NOT_REFERENCED(pdev);
	MSG_FUNC_ENTER();
	MTKFB_LOG("[FB Driver] %s(): 0x%x\n", __func__, mesg.event);
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
	case PM_HIBERNATION_PREPARE:
		DISPDBG("[FB Driver] %s PM_HIBERNATION_PREPARE\n", __func__);
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		primary_display_ipoh_restore();
		DISPDBG("[FB Driver] %s PM_RESTORE_PREPARE\n", __func__);
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		DISPDBG("[FB Driver] %s PM_POST_HIBERNATION\n", __func__);
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

static void mtkfb_shutdown(struct platform_device *pdev)
{
	MTKFB_LOG("[FB Driver] %s\n", __func__);
	/* mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF); */
	if (!lcd_fps)
		msleep(30);
	else
		msleep(2 * 100000 / lcd_fps);	/* Delay 2 frames. */

	if (primary_display_is_sleepd()) {
		MTKFB_LOG("mtkfb has been power off\n");
		return;
	}
	primary_display_set_power_mode(FB_SUSPEND);
	primary_display_suspend();
	MTKFB_LOG("[FB Driver] leave %s\n", __func__);
}

void mtkfb_clear_lcm(void)
{
}

static void mtkfb_early_suspend(void)
{
	int ret = 0;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;

	DISPMSG("%s+\n", __func__);

	ret = primary_display_suspend();

	if (ret < 0) {
		DISP_PR_ERR("primary display suspend failed\n");
		return;
	}

	DISPMSG("%s-\n", __func__);
}

/* PM resume */
static int mtkfb_resume(struct platform_device *pdev)
{
	NOT_REFERENCED(pdev);
	MSG_FUNC_ENTER();
	MTKFB_LOG("[FB Driver] %s()\n", __func__);
	MSG_FUNC_LEAVE();
	return 0;
}

static void mtkfb_late_resume(void)
{
	int ret = 0;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL)
		return;

	DISPMSG("%s+\n", __func__);

	ret = primary_display_resume();

	if (ret) {
		DISP_PR_ERR("primary display resume failed\n");
		return;
	}

	DISPMSG("%s-\n", __func__);

}

#ifdef CONFIG_PM

int mtkfb_pm_suspend(struct device *device)
{
	/* pr_debug("calling %s()\n", __func__); */

	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		disp_aee_db_print("pdev is NULL\n");
		return -1;
	}

	return mtkfb_suspend(pdev, PMSG_SUSPEND);
}

int mtkfb_pm_resume(struct device *device)
{
	/* pr_debug("calling %s()\n", __func__); */

	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		disp_aee_db_print("pdev is NULL\n");
		return -1;
	}

	return mtkfb_resume(pdev);
}

int mtkfb_pm_freeze(struct device *device)
{
	primary_display_esd_check_enable(0);
	return 0;
}

int mtkfb_pm_restore_noirq(struct device *device)
{
	/* disphal_pm_restore_noirq(device); */
	DISPDBG("%s: %d\n", __func__, __LINE__);
	is_ipoh_bootup = true;
#if 0
	if (disp_helper_get_option(DISP_OPT_DYNAMIC_SWITCH_MMSYSCLK))
		ddp_clk_prepare_enable(MM_VENCPLL);
	ddp_clk_prepare_enable(DISP_MTCMOS_CLK);
	ddp_clk_prepare_enable(DISP0_SMI_COMMON);
	ddp_clk_prepare_enable(DISP0_SMI_LARB0);
	ddp_clk_prepare_enable(DISP0_SMI_LARB4);
#else
	dpmgr_path_power_on(primary_get_dpmgr_handle(), CMDQ_DISABLE);
#endif
	DISPDBG("%s: %d\n", __func__, __LINE__);
	return 0;
}

#else /* !CONFIG_PM */

#define mtkfb_pm_suspend	NULL
#define mtkfb_pm_resume		NULL
#define mtkfb_pm_restore_noirq	NULL
#define mtkfb_pm_freeze		NULL

#endif /* CONFIG_PM */


static const struct of_device_id mtkfb_of_ids[] = {
	{.compatible = "mediatek,MTKFB",},
	{}
};

static const struct dev_pm_ops mtkfb_pm_ops = {
	.suspend = mtkfb_pm_suspend,
	.resume = mtkfb_pm_resume,
	.freeze = mtkfb_pm_freeze,
	.thaw = mtkfb_pm_resume,
	.poweroff = mtkfb_pm_suspend,
	.restore = mtkfb_pm_resume,
	.restore_noirq = mtkfb_pm_restore_noirq,
};

static struct platform_driver mtkfb_driver = {
	.probe = mtkfb_probe,
	.remove = mtkfb_remove,
	.suspend = mtkfb_suspend,
	.resume = mtkfb_resume,
	.shutdown = mtkfb_shutdown,
	.driver = {
		.name = MTKFB_DRIVER,
#ifdef CONFIG_PM
		.pm = &mtkfb_pm_ops,
#endif
		.bus = &platform_bus_type,
		.of_match_table = mtkfb_of_ids,
	},
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend mtkfb_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = mtkfb_early_suspend,
	.resume = mtkfb_late_resume,
};
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
			 "|------------------------------------------------------------------|\n");

	len += scnprintf(stringbuf + len, buf_len - len,
			 "|Framebuffer VA:0x%lx, PA:0x%lx, MVA:0x%lx, Reserved Size:0x%08x|%d\n",
			 va, pa, mva, resv_size, resv_size);
	len += scnprintf(stringbuf + len, buf_len - len,
			 "|xoffset=%d, yoffset=%d\n",
			 mtkfb_fbi->var.xoffset, mtkfb_fbi->var.yoffset);
	len += scnprintf(stringbuf + len, buf_len - len,
			 "|framebuffer line alignment(for gpu)=%d\n",
			 MTK_FB_ALIGNMENT);
	len += scnprintf(stringbuf + len, buf_len - len,
			 "|xres=%d, yres=%d,bpp=%d,pages=%d,linebytes=%d,total size=%d\n",
			 MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES,
			 MTK_FB_LINE, MTK_FB_SIZEV);
	/* use extern in case DAL_LOCK is hold, then can't get any debug info */
	len += scnprintf(stringbuf + len, buf_len - len, "|AEE Layer is %s\n",
			 is_DAL_Enabled() ? "enabled" : "disabled");

	return len;
}

/* Register both the driver and the device */
int __init mtkfb_init(void)
{
	int ret = 0;

	MSG_FUNC_ENTER();
	DISPCHECK("%s Enter\n", __func__);
	if (platform_driver_register(&mtkfb_driver)) {
		PRNERR("failed to register mtkfb driver\n");
		ret = -ENODEV;
		goto exit;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mtkfb_early_suspend_handler);
#endif
	PanelMaster_Init();
	DBG_Init();
	mtkfb_ipo_init();
exit:
	MSG_FUNC_LEAVE();
	DISPCHECK("%s LEAVE\n", __func__);
	return ret;
}

static void __exit mtkfb_cleanup(void)
{
	MSG_FUNC_ENTER();

	platform_driver_unregister(&mtkfb_driver);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&mtkfb_early_suspend_handler);
#endif

	PanelMaster_Deinit();
	DBG_Deinit();

	MSG_FUNC_LEAVE();
}


module_init(mtkfb_init);
module_exit(mtkfb_cleanup);

MODULE_DESCRIPTION("MEDIATEK framebuffer driver");
MODULE_AUTHOR("Xuecheng Zhang <Xuecheng.Zhang@mediatek.com>");
MODULE_LICENSE("GPL");

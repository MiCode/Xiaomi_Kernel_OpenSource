/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
/*#include <linux/earlysuspend.h>*/
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include "disp_assert_layer.h"
#include <linux/semaphore.h>
#include <linux/mutex.h>
/*#include <linux/leds-mt65xx.h> */
#include <linux/suspend.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#ifndef CONFIG_ARM64
#include <asm/mach-types.h>
#endif
#include <asm/cacheflush.h>
#include <linux/io.h>

/*#include <mach/dma.h>*/
/*#include <mach/irqs.h>*/
#include <linux/dma-mapping.h>
#include <linux/compat.h>
#include <mt-plat/mt_boot.h>

#include "debug.h"

#include "ddp_hal.h"
#include "disp_drv_log.h"
#include "ddp_log.h"
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
#include "fbconfig_kdebug_rome.h"
#include "compat_mtkfb.h"
#include "mtk_ovl.h"

#if defined(MTK_ALPS_BOX_SUPPORT)
#include "extd_ddp.h"

static bool factory_mode;

#endif
/*#define DDP_UT */
/*#define PAN_DISPLAY_TEST */

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

/* #undef CONFIG_OF */

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

#define MTK_FB_XRESV (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT))
#define MTK_FB_YRESV (ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * MTK_FB_PAGES)	/* For page flipping */
#define MTK_FB_BYPP  ((MTK_FB_BPP + 7) >> 3)
#define MTK_FB_LINE  (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) * MTK_FB_BYPP)
#define MTK_FB_SIZE  (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT))

#define MTK_FB_SIZEV (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * MTK_FB_PAGES)

#define CHECK_RET(expr)    \
	do { \
		int ret = (expr);  \
		ASSERT(0 == ret);  \
	} while (0)


static size_t mtkfb_log_on;
#define MTKFB_MSG(fmt, args...) pr_warn("[MTKFB]"fmt, ##args)
#define MTKFB_DBG(fmt, args...) pr_debug("[MTKFB]"fmt, ##args)
#define MTKFB_ERR(fmt, args...) pr_err("[MTKFB]"fmt, ##args)

#define MTKFB_LOG(fmt, args...) \
		do { \
			if (mtkfb_log_on) \
				pr_err("[MTKFB]"fmt, ##args); \
		} while (0)

#define MTKFB_FUNC()	\
	do { \
		if (mtkfb_log_on) \
			pr_err("[MTKFB Func]%s %d\n", __func__, __LINE__); \
	} while (0)

void mtkfb_log_enable(int enable)
{
	mtkfb_log_on = enable;
	MTKFB_MSG("mtkfb log %s\n", enable ? "enabled" : "disabled");
}

/* --------------------------------------------------------------------------- */
/* local variables */
/* --------------------------------------------------------------------------- */

unsigned long fb_pa = 0;

static const struct timeval FRAME_INTERVAL = { 0, 30000 };	/* 33ms */

atomic_t has_pending_update = ATOMIC_INIT(0);
struct fb_overlay_layer video_layerInfo;
UINT32 dbr_backup = 0;
UINT32 dbg_backup = 0;
UINT32 dbb_backup = 0;
bool fblayer_dither_needed = false;
/* static UINT32 mtkfb_using_layer_type = LAYER_2D; */
/* static bool hwc_force_fb_enabled = true; */
bool is_ipoh_bootup = false;
struct fb_info *mtkfb_fbi;
struct platform_device *mtkfb_fbdev;
struct fb_overlay_layer fb_layer_context;
mtk_dispif_info_t dispif_info[MTKFB_MAX_DISPLAY_COUNT];

/* This mutex is used to prevent tearing due to page flipping when adbd is
   reading the front buffer
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


DEFINE_MUTEX(ScreenCaptureMutex);

bool is_early_suspended = false;
static int sem_flipping_cnt = 1;
static int sem_early_suspend_cnt = 1;
/* static int sem_overlay_buffer_cnt = 1; */
static int vsync_cnt;


/* --------------------------------------------------------------------------- */
/* local function declarations */
/* --------------------------------------------------------------------------- */

static int init_framebuffer(struct fb_info *info);
static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info *layerInfo);
/* static int mtkfb_update_screen(struct fb_info *info); */
/* static void mtkfb_update_screen_impl(void); */
static unsigned int mtkfb_fm_auto_test(void);


/* --------------------------------------------------------------------------- */
/* Timer Routines */
/* --------------------------------------------------------------------------- */
/* static struct task_struct *screen_update_task; */
/* static struct task_struct *esd_recovery_task; */
unsigned int lcd_fps = 6000;
wait_queue_head_t screen_update_wq;


/*
 * ---------------------------------------------------------------------------
 *  mtkfb_set_lcm_inited() will be called in mt6516_board_init()
 * ---------------------------------------------------------------------------
 */
static bool is_lcm_inited;
void mtkfb_set_lcm_inited(bool inited)
{
	is_lcm_inited = inited;
}

/*
 * ---------------------------------------------------------------------------
 * fbdev framework callbacks and the ioctl interface
 * ---------------------------------------------------------------------------
 */
/* Called each time the mtkfb device is opened */
static int mtkfb_open(struct fb_info *info, int user)
{
	/*NOT_REFERENCED(info);
	   NOT_REFERENCED(user); */
	DISPFUNC();
	MSG_FUNC_ENTER();
	MSG_FUNC_LEAVE();
	return 0;
}

/* Called when the mtkfb device is closed. We make sure that any pending
 * gfx DMA operations are ended, before we return. */
static int mtkfb_release(struct fb_info *info, int user)
{

	/*NOT_REFERENCED(info);
	   NOT_REFERENCED(user); */
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

	/*NOT_REFERENCED(transp); */

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

#if 0
static void mtkfb_update_screen_impl(void)
{
	bool down_sem = false;

	MTKFB_FUNC();
	MMProfileLog(MTKFB_MMP_Events.UpdateScreenImpl, MMProfileFlagStart);
	if (down_interruptible(&sem_overlay_buffer)) {
		MTKFB_ERR("[FB Driver] can't get semaphore in mtkfb_update_screen_impl()\n");
	} else {
		down_sem = true;
		sem_overlay_buffer_cnt--;
	}

	/* DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update)); */

	if (down_sem) {
		sem_overlay_buffer_cnt++;
		up(&sem_overlay_buffer);
	}
	MMProfileLog(MTKFB_MMP_Events.UpdateScreenImpl, MMProfileFlagEnd);
}
#endif


#if 0
static int mtkfb_update_screen(struct fb_info *info)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_early_suspend)) {
		MTKFB_ERR("[FB Driver] can't get semaphore in mtkfb_update_screen()\n");
		return -ERESTARTSYS;
	}
	sem_early_suspend_cnt--;
	if (primary_display_is_sleepd())
		goto End;
	mtkfb_update_screen_impl();

End:
	sem_early_suspend_cnt++;
	up(&sem_early_suspend);
	return 0;
}
#endif

/* static unsigned int BL_level; */
/* static BOOL BL_set_level_resume = false; */
int mtkfb_set_backlight_level(unsigned int level)
{
	MTKFB_FUNC();
	MTKFB_LOG("mtkfb_set_backlight_level:%d Start\n", level);
	primary_display_setbacklight(level);
	MTKFB_LOG("mtkfb_set_backlight_level End\n");
	return 0;
}
EXPORT_SYMBOL(mtkfb_set_backlight_level);

int mtkfb_set_backlight_mode(unsigned int mode)
{
	MTKFB_FUNC();
	if (down_interruptible(&sem_flipping)) {
		MTKFB_ERR("can't get semaphore:%d\n", __LINE__);
		return -ERESTARTSYS;
	}
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
		MTKFB_ERR("can't get semaphore:%d\n", __LINE__);
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
		MTKFB_ERR("can't get semaphore:%d\n", __LINE__);
		return -ERESTARTSYS;
	}
	sem_flipping_cnt--;
	if (down_interruptible(&sem_early_suspend)) {
		MTKFB_ERR("can't get semaphore:%d\n", __LINE__);
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
		MTKFB_ERR("mtkfb has suspend, return directly\n");
		msleep(20);
		return;
	}
	vsync_cnt++;
	primary_display_wait_for_vsync(NULL);
	vsync_cnt--;

}
EXPORT_SYMBOL(mtkfb_waitVsync);
/* Used for HQA test */
/*-------------------------------------------------------------
   Note: The using scenario must be
	 1. switch normal mode to factory mode when LCD screen is on
	 2. switch factory mode to normal mode(optional)
-------------------------------------------------------------*/
/* static struct fb_var_screeninfo fbi_var_backup; */
/* static struct fb_fix_screeninfo fbi_fix_backup; */
/* static BOOL need_restore = false; */
static int mtkfb_set_par(struct fb_info *fbi);

static bool first_update;

/* static bool first_enable_esd = true; */

static int _convert_fb_layer_to_disp_input(struct fb_overlay_layer *src,
					   primary_disp_input_config *dst)
{
	unsigned int layerpitch = 0;
	unsigned int layerbpp = 0;

	dst->layer = src->layer_id;

	if (!src->layer_enable) {
		dst->layer_en = 0;
		dst->isDirty = true;
		return 0;
	}

	switch (src->src_fmt) {
	case MTK_FB_FORMAT_YUV422:
		dst->fmt = eYUY2;
		layerpitch = 2;
		layerbpp = 16;
		break;

	case MTK_FB_FORMAT_RGB565:
		dst->fmt = eRGB565;
		layerpitch = 2;
		layerbpp = 16;
		break;

	case MTK_FB_FORMAT_RGB888:
		dst->fmt = eRGB888;
		layerpitch = 3;
		layerbpp = 24;
		break;

	case MTK_FB_FORMAT_BGR888:
		dst->fmt = eBGR888;
		layerpitch = 3;
		layerbpp = 24;
		break;
	case MTK_FB_FORMAT_ARGB8888:
		dst->fmt = eARGB8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_ABGR8888:
		dst->fmt = eABGR8888;
		layerpitch = 4;
		layerbpp = 32;
		break;
	case MTK_FB_FORMAT_BGRA8888:
		dst->fmt = eBGRA8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_RGBA8888:
		dst->fmt = eRGBA8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_XRGB8888:
		dst->fmt = eARGB8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_XBGR8888:
		dst->fmt = eARGB8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_UYVY:
		dst->fmt = eUYVY;
		layerpitch = 2;
		layerbpp = 16;
		break;

	default:
		MTKFB_ERR("Invalid color format: 0x%x\n", src->src_fmt);
		return -1;
	}

	dst->vaddr = (unsigned long)src->src_base_addr;
	dst->security = src->security;
/* set_overlay will not use fence+ion handle */
#if 0
/* #if defined (MTK_FB_ION_SUPPORT) */
	if (src->src_phy_addr != NULL)
		dst->addr = (unsigned int)src->src_phy_addr;
	else
		dst->addr = mtkfb_query_buf_mva(src->layer_id, (unsigned int)src->next_buff_idx);

#else
	dst->addr = (unsigned long)src->src_phy_addr;
#endif
	MTKFB_LOG("_convert_fb_layer_to_disp_input, dst->addr=0x%lx\n", dst->addr);

	dst->isTdshp = src->isTdshp;
	dst->buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

	/* set Alpha blending */
	dst->alpha = src->alpha;
	if (MTK_FB_FORMAT_ARGB8888 == src->src_fmt || MTK_FB_FORMAT_ABGR8888 == src->src_fmt)
		dst->aen = true;
	else
		dst->aen = false;


	/* set src width, src height */
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;
	dst->dst_w = src->tgt_width;
	dst->dst_h = src->tgt_height;
	if (dst->dst_w > dst->src_w)
		dst->dst_w = dst->src_w;
	if (dst->dst_h > dst->src_h)
		dst->dst_h = dst->src_h;

	dst->src_pitch = src->src_pitch * layerpitch;

	/* set color key */
	dst->key = src->src_color_key;
	dst->keyEn = src->src_use_color_key;

	/* data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT */
	dst->layer_en = src->layer_enable;
	dst->isDirty = true;

#if 1
	MTKFB_LOG
	    ("_convert_fb_layer_to_disp_input():id=%u, en=%u, next_idx=%u, vaddr=0x%lx, paddr=0x%x\n",
	     dst->layer, dst->layer_en, dst->buff_idx, (unsigned long)(dst->vaddr),
	     (unsigned int)(dst->addr));
	MTKFB_LOG
		("_convert_fb_layer_to_disp_input(): src fmt=%u, dst fmt=%u, pitch=%u, xoff=%u, yoff=%u, w=%u, h=%u\n",
		src->src_fmt, dst->fmt, dst->src_pitch, dst->src_x,
		dst->src_y, dst->src_w, dst->src_h);
	MTKFB_LOG
	    ("_convert_fb_layer_to_disp_input():target xoff=%u, target yoff=%u, target w=%u, target h=%u, aen=%u\n",
	     dst->dst_x, dst->dst_y, dst->dst_w, dst->dst_h, dst->aen);
#endif
	return 0;
}

#if 0
static int _overlay_info_convert(struct fb_overlay_layer *src, OVL_CONFIG_STRUCT *dst)
{
	unsigned int layerpitch = 0;
	unsigned int layerbpp = 0;

	dst->layer = src->layer_id;

	if (!src->layer_enable) {
		dst->layer_en = 0;
		dst->isDirty = true;
		return 0;
	}

	switch (src->src_fmt) {
	case MTK_FB_FORMAT_YUV422:
		dst->fmt = eYUY2;
		layerpitch = 2;
		layerbpp = 16;
		break;

	case MTK_FB_FORMAT_RGB565:
		dst->fmt = eRGB565;
		layerpitch = 2;
		layerbpp = 16;
		break;

	case MTK_FB_FORMAT_RGB888:
		dst->fmt = eRGB888;
		layerpitch = 3;
		layerbpp = 24;
		break;

	case MTK_FB_FORMAT_BGR888:
		dst->fmt = eBGR888;
		layerpitch = 3;
		layerbpp = 24;
		break;
		/* xuecheng, ?????? */
	case MTK_FB_FORMAT_ARGB8888:
		dst->fmt = eRGBA8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_ABGR8888:
		/* dst->fmt = eABGR8888; */
		dst->fmt = eRGBA8888;
		layerpitch = 4;
		layerbpp = 32;
		break;
	case MTK_FB_FORMAT_XRGB8888:
		dst->fmt = eARGB8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_XBGR8888:
		dst->fmt = eRGBA8888;
		layerpitch = 4;
		layerbpp = 32;
		break;

	case MTK_FB_FORMAT_UYVY:
		dst->fmt = eUYVY;
		layerpitch = 2;
		layerbpp = 16;
		break;

	default:
		MTKFB_ERR("Invalid color format: 0x%x\n", src->src_fmt);
		return -1;
	}

	dst->vaddr = (unsigned int)src->src_base_addr;
	dst->security = src->security;
/* set overlay will not use fence+ion handle */
#if 0
/* #if defined (MTK_FB_ION_SUPPORT) */
	if (src->src_phy_addr != NULL)
		dst->addr = (unsigned int)src->src_phy_addr;
	else
		dst->addr = mtkfb_query_buf_mva(src->layer_id, (unsigned int)src->next_buff_idx);

#else
	dst->addr = (unsigned int)src->src_phy_addr;
#endif
	DISPMSG("_overlay_info_convert, dst->addr=0x%08x\n", dst->addr);
	dst->isTdshp = src->isTdshp;
	dst->buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

	/* set Alpha blending */
	dst->alpha = src->alpha;
	if (MTK_FB_FORMAT_ARGB8888 == src->src_fmt || MTK_FB_FORMAT_ABGR8888 == src->src_fmt)
		dst->aen = true;
	else
		dst->aen = false;


#ifdef ROME_TODO
#error
	/* xuecheng, for slt debug */
	if (!strcmp(current->comm, "display_slt")) {
		dst->aen = false;
		isAEEEnabled = 1;
		DAL_Dynamic_Change_FB_Layer(isAEEEnabled);	/* default_ui_ layer coniig to changed_ui_layer */
	}
#endif

	/* set src width, src height */
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;
	dst->dst_w = src->tgt_width;
	dst->dst_h = src->tgt_height;
	if (dst->dst_w > dst->src_w)
		dst->dst_w = dst->src_w;
	if (dst->dst_h > dst->src_h)
		dst->dst_h = dst->src_h;

	dst->src_pitch = src->src_pitch * layerpitch;
	/* set color key */
	dst->key = src->src_color_key;
	dst->keyEn = src->src_use_color_key;

	/* data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT */
	dst->layer_en = src->layer_enable;
	dst->isDirty = true;

	return 0;
}
#endif

static int mtkfb_pan_display_impl(struct fb_var_screeninfo *var, struct fb_info *info)
{
	UINT32 offset = 0;
	UINT32 paStart = 0;
	char *vaStart = NULL, *vaEnd = NULL;
	int ret = 0;
	/* int wait_ret = 0; */
	unsigned int layerpitch = 0;
	unsigned int src_pitch = 0;
	primary_disp_input_config input;

	/* DISPFUNC(); */

	if (first_update) {
		MTKFB_MSG("the first time of mtkfb_pan_display_impl will be ignored\n");
		first_update = false;
		return ret;
	}

	info->var.yoffset = var->yoffset;
	offset = var->yoffset * info->fix.line_length;
	paStart = fb_pa + offset;
	vaStart = info->screen_base + offset;
	vaEnd = vaStart + info->var.yres * info->fix.line_length;

	MTKFB_DBG
	    ("vaStart0x%x,paStart0x%lx,xoffset=%u, yoffset=%u, xres=%u, yres=%u, xresv=%u, yresv=%u,",
	    var->xoffset, (unsigned long)vaStart, paStart, var->yoffset, info->var.xres, info->var.yres,
	     info->var.xres_virtual, info->var.yres_virtual);
	MTKFB_DBG
		(" blue.offset=%d, green.offset=%d, red.offset=%d, transp.offset=%d\n",
		var->blue.offset, var->green.offset,
	     var->red.offset, var->transp.offset);

	memset((void *)&input, 0, sizeof(primary_disp_input_config));

	input.addr = (unsigned long)paStart;
	input.vaddr = (unsigned long)vaStart;
	input.layer = primary_display_get_option("FB_LAYER");
	input.layer_en = 1;
	input.src_x = 0;
	input.src_y = 0;
	input.src_w = var->xres;
	input.src_h = var->yres;
	input.dst_x = 0;
	input.dst_y = 0;
	input.dst_w = var->xres;
	input.dst_h = var->yres;

	switch (var->bits_per_pixel) {
	case 16:
		input.fmt = eRGB565;
		layerpitch = 2;
		input.aen = false;
		break;
	case 24:
		input.fmt = eRGB888;
		layerpitch = 3;
		input.aen = false;
		break;
	case 32:
		/* this is very useful for color format related debug */
#if 0
		{
			struct mtkfb_device *fbdev = (struct mtkfb_device *)info->par;
			unsigned int va = fbdev->fb_va_base + offset;

			DISPMSG("fb dump: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n", *(unsigned int *)va,
				*(unsigned int *)(va + 4), *(unsigned int *)(va + 8),
				*(unsigned int *)(va + 0xC));

		}
		DISPMSG("pan display, var->blue.offset=%d\n", var->blue.offset);
#endif
		/* home screen use eRGBA8888 */
		input.fmt = (0 == var->blue.offset) ? eBGRA8888 : eRGBA8888;
		/* recovery mode default source data is eRGBA8888, but not notify driver! */
		if (get_boot_mode() == RECOVERY_BOOT)
			input.fmt = eRGBA8888;

		layerpitch = 4;
		input.aen = false;
		break;
	default:
		MTKFB_ERR("Invalid color format bpp: 0x%d\n", var->bits_per_pixel);
		return -1;
	}

	src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
	input.alpha = 0xFF;

	input.buff_idx = -1;
	input.src_pitch = src_pitch * layerpitch;
	input.isDirty = 1;

	ret = primary_display_config_input(&input);
	ret = primary_display_trigger(true, NULL, 0);

#if defined(MTK_ALPS_BOX_SUPPORT)
	MTKFB_LOG("%s ext_disp_config_input\n", __func__);
	if (factory_mode) {
		ret = ext_disp_config_input((ext_disp_input_config *) &input);
		ret = ext_disp_trigger(true, NULL, 0);
	}
#endif
	/* primary_display_diagnose(); */
#ifdef ROME_TODO
#error "need to wait rdma0 done here"
#error "aee dynamic switch, set overlay race condition protection"
#endif

	return ret;
}


/* Set fb_info.fix fields and also updates fbdev.
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


/* Check values in var, try to adjust them in case of out of bound values if
 * possible, or return error.
 */
static int mtkfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fbi)
{
	unsigned int bpp;
	unsigned long max_frame_size;
	unsigned long line_size;

	struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;

	/* DISPFUNC(); */

	MTKFB_LOG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, ",
		  var->xres, var->yres, var->xres_virtual, var->yres_virtual);

	MTKFB_LOG("xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
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
	MTKFB_LOG("fbdev->fb_size_in_byte=0x%lx\n", fbdev->fb_size_in_byte);
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
	MTKFB_LOG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, ",
		  var->xres, var->yres, var->xres_virtual, var->yres_virtual);
	MTKFB_LOG("xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
		var->xoffset, var->yoffset, var->bits_per_pixel);
	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	MTKFB_LOG("mtkfb_check_var, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, ",
		  var->xres, var->yres, var->xres_virtual, var->yres_virtual);
	MTKFB_LOG("xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
		var->xoffset, var->yoffset, var->bits_per_pixel);

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

		/* TODO : recovery mode */
		/*
		   ASSERT(8 == var->green.offset && 24 == var->transp.offset);
		   ASSERT(16 == var->red.offset + var->blue.offset);
		   ASSERT(16 == var->red.offset || 0 == var->red.offset);
		 */
	}

	var->red.msb_right = var->green.msb_right = var->blue.msb_right = var->transp.msb_right = 0;

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

unsigned int FB_LAYER = 2;
#define ASSERT_LAYER    (DDP_OVL_LAYER_MUN-1)
#define DISP_DEFAULT_UI_LAYER_ID (DDP_OVL_LAYER_MUN-1)
#define DISP_CHANGED_UI_LAYER_ID (DDP_OVL_LAYER_MUN-2)

/* Switch to a new mode. The parameters for it has been check already by
 * mtkfb_check_var.
 */
static int mtkfb_set_par(struct fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->var;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;
	struct fb_overlay_layer fb_layer;
	u32 bpp = var->bits_per_pixel;
	primary_disp_input_config temp;

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
		fb_layer.src_fmt = (0 == var->blue.offset) ?
		    MTK_FB_FORMAT_RGB888 : MTK_FB_FORMAT_BGR888;
		fb_layer.src_color_key = 0xFF000000;
		break;

	case 32:
		fb_layer.src_use_color_key = 0;
		MTKFB_LOG("set_par,var->blue.offset=%d\n", var->blue.offset);
		fb_layer.src_fmt = (0 == var->blue.offset) ?
		    MTK_FB_FORMAT_BGRA8888 : MTK_FB_FORMAT_RGBA8888;
		fb_layer.src_color_key = 0;
		break;

	default:
		fb_layer.src_fmt = MTK_FB_FORMAT_UNKNOWN;
		MTKFB_ERR("[%s]unsupported bpp: %d", __func__, bpp);
		return -1;
	}

	/* If the framebuffer format is NOT changed, nothing to do */
	/*  */
	/* if (fb_layer.src_fmt == fbdev->layer_format[primary_display_get_option("FB_LAYER")]) { */
	/* goto Done; */
	/* } */

	/* else, begin change display mode */
	/*  */
	set_fb_fix(fbdev);

	fb_layer.layer_id = primary_display_get_option("FB_LAYER");
	fb_layer.layer_enable = 1;
	fb_layer.src_base_addr =
	    (void *)((unsigned long)fbdev->fb_va_base + var->yoffset * fbi->fix.line_length);
	/*MTKFB_LOG("fb_va=0x%lx\n", fb_layer.src_base_addr); */
	MTKFB_LOG("fb_pa=0x%lx, var->yoffset=0x%08x,fbi->fix.line_length=0x%08x\n", fb_pa,
		  var->yoffset, fbi->fix.line_length);
	fb_layer.src_phy_addr = (void *)(fb_pa + var->yoffset * fbi->fix.line_length);
	fb_layer.src_direct_link = 0;
	fb_layer.src_offset_x = fb_layer.src_offset_y = 0;
/* fb_layer.src_width = fb_layer.tgt_width = fb_layer.src_pitch = var->xres; */
	/* xuecheng, does HWGPU_SUPPORT still in use now? */
#if defined(HWGPU_SUPPORT)
	fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
#else
	if (get_boot_mode() == META_BOOT || get_boot_mode() == FACTORY_BOOT
	    || get_boot_mode() == ADVMETA_BOOT || get_boot_mode() == RECOVERY_BOOT)
		fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
	else
		fb_layer.src_pitch = ALIGN_TO(var->xres, MTK_FB_ALIGNMENT);
#endif
	fb_layer.src_width = fb_layer.tgt_width = var->xres;
	fb_layer.src_height = fb_layer.tgt_height = var->yres;
	fb_layer.tgt_offset_x = fb_layer.tgt_offset_y = 0;
	fb_layer.alpha = 0xFF;

	/* fb_layer.src_color_key = 0; */
	fb_layer.layer_rotation = MTK_FB_ORIENTATION_0;
	fb_layer.layer_type = LAYER_2D;
	MTKFB_LOG("mtkfb_set_par, fb_layer.src_fmt=%x\n", fb_layer.src_fmt);

	if (!isAEEEnabled) {
		MTKFB_LOG("AEE is not enabled, will disable layer 3\n");
		memset((void *)&temp, 0, sizeof(primary_disp_input_config));
		temp.layer = primary_display_get_option("ASSERT_LAYER");
		temp.layer_en = 0;
		temp.isDirty = 1;
		primary_display_config_input(&temp);
	} else {
		MTKFB_LOG("AEE is enabled, should not disable layer 3\n");
	}

	memset((void *)&temp, 0, sizeof(primary_disp_input_config));
	_convert_fb_layer_to_disp_input(&fb_layer, &temp);
	primary_display_config_input(&temp);

	/*trigger OVL config update, so layer 0 is enabled, layer 2/3 is disable which is enabled at lk */
	/*so the font "normal boot" will disapper at here */
	primary_display_trigger(0, NULL, 0);

#if defined(MTK_ALPS_BOX_SUPPORT)
	MTKFB_LOG("%s ext_disp_config_input && ext_disp_trigger factory_mode %d\n", __func__,
		  factory_mode);

	if (factory_mode) {
		ext_disp_config_input((ext_disp_input_config *) &temp);
		ext_disp_trigger(0, NULL, 0);
	}
#endif


	/* backup fb_layer information. */
	memcpy(&fb_layer_context, &fb_layer, sizeof(fb_layer));

/* Done: */
	MSG_FUNC_LEAVE();
	return 0;
}


static int mtkfb_soft_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	/*NOT_REFERENCED(info);
	   NOT_REFERENCED(cursor); */

	return 0;
}

static int mtkfb_get_overlay_layer_info(struct fb_overlay_layer_info *layerInfo)
{
#if 0
	DISP_LAYER_INFO layer;

	if (layerInfo->layer_id >= DDP_OVL_LAYER_MUN)
		return 0;

	layer.id = layerInfo->layer_id;
	/* DISP_GetLayerInfo(&layer); */
	int id = layerInfo->layer_id;

	layer.curr_en = captured_layer_config[id].layer_en;
	layer.next_en = cached_layer_config[id].layer_en;
	layer.hw_en = realtime_layer_config[id].layer_en;
	layer.curr_idx = captured_layer_config[id].buff_idx;
	layer.next_idx = cached_layer_config[id].buff_idx;
	layer.hw_idx = realtime_layer_config[id].buff_idx;
	layer.curr_identity = captured_layer_config[id].identity;
	layer.next_identity = cached_layer_config[id].identity;
	layer.hw_identity = realtime_layer_config[id].identity;
	layer.curr_conn_type = captured_layer_config[id].connected_type;
	layer.next_conn_type = cached_layer_config[id].connected_type;
	layer.hw_conn_type = realtime_layer_config[id].connected_type;
	layerInfo->layer_enabled = layer.hw_en;
	layerInfo->curr_en = layer.curr_en;
	layerInfo->next_en = layer.next_en;
	layerInfo->hw_en = layer.hw_en;
	layerInfo->curr_idx = layer.curr_idx;
	layerInfo->next_idx = layer.next_idx;
	layerInfo->hw_idx = layer.hw_idx;
	layerInfo->curr_identity = layer.curr_identity;
	layerInfo->next_identity = layer.next_identity;
	layerInfo->hw_identity = layer.hw_identity;
	layerInfo->curr_conn_type = layer.curr_conn_type;
	layerInfo->next_conn_type = layer.next_conn_type;
	layerInfo->hw_conn_type = layer.hw_conn_type;
#endif
	return 0;
}


#include <mt-plat/aee.h>
#define mtkfb_aee_print(string, args...) \
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_MMPROFILE_BUFFER, "sf-mtkfb blocked", string, ##args)

void mtkfb_dump_layer_info(void)
{
#if 0

#endif
}


static int mtkfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	DISP_STATUS ret = 0;
	int r = 0;

	DISPFUNC();
	/* / M: dump debug mmprofile log info */
	MMProfileLogEx(MTKFB_MMP_Events.IOCtrl, MMProfileFlagPulse, _IOC_NR(cmd), arg);
	MTKFB_LOG("mtkfb_ioctl, info=0x%lx, cmd=0x%08x, arg=0x%08x\n", (unsigned long)info,
		  (unsigned int)cmd, (unsigned int)arg);

	switch (cmd) {

	case MTKFB_GET_FRAMEBUFFER_MVA:
		return copy_to_user(argp, &fb_pa, sizeof(fb_pa)) ? -EFAULT : 0;
		/* remain this for engineer mode dfo multiple resolution */
#if 1
	case MTKFB_GET_DISPLAY_IF_INFORMATION:
		{
			int displayid = 0;

			if (copy_from_user(&displayid, (void __user *)arg, sizeof(displayid))) {
				MTKFB_ERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
				return -EFAULT;
			}

			if (displayid > MTKFB_MAX_DISPLAY_COUNT) {
				MTKFB_ERR("[FB]: invalid display id:%d\n", displayid);
				return -EFAULT;
			}

			if (displayid == 0) {
				dispif_info[displayid].displayWidth = primary_display_get_width();
				dispif_info[displayid].displayHeight = primary_display_get_height();

				if (2560 == dispif_info[displayid].displayWidth)
					dispif_info[displayid].displayWidth = 2048;
				if (1600 == dispif_info[displayid].displayHeight)
					dispif_info[displayid].displayHeight = 1536;
				dispif_info[displayid].lcmOriginalWidth =
				    primary_display_get_original_width();
				dispif_info[displayid].lcmOriginalHeight =
				    primary_display_get_original_height();
				dispif_info[displayid].displayMode =
				    primary_display_is_video_mode() ? 0 : 1;
			} else {
				MTKFB_ERR("information for displayid: %d is not available now\n",
					  displayid);
			}

			if (copy_to_user
			    ((void __user *)arg, &(dispif_info[displayid]),
			     sizeof(mtk_dispif_info_t))) {
				MTKFB_ERR("copy_to_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			}

			return r;
		}
#endif
	case MTKFB_POWEROFF:
		{
			MTKFB_FUNC();
			if (primary_display_is_sleepd()) {
				MTKFB_ERR("is still in MTKFB_POWEROFF!!!\n");
				return r;
			}

			MTKFB_MSG("enter MTKFB_POWEROFF\n");
			/* cci400_sel_for_ddp(); */
			ret = primary_display_suspend();
			if (ret < 0)
				MTKFB_ERR("primary display suspend failed\n");

			MTKFB_MSG("leave MTKFB_POWEROFF\n");

			is_early_suspended = true;	/* no care */
			return r;
		}

	case MTKFB_POWERON:
		{
			MTKFB_FUNC();
			if (primary_display_is_alive()) {
				MTKFB_ERR("is still in MTKFB_POWERON!!!\n");
				return r;
			}
			MTKFB_MSG("[FB Driver] enter MTKFB_POWERON\n");
			primary_display_resume();
			MTKFB_MSG("[FB Driver] leave MTKFB_POWERON\n");
			is_early_suspended = false;	/* no care */
			return r;
		}
	case MTKFB_GET_POWERSTATE:
		{
			unsigned long power_state;

			if (primary_display_is_sleepd())
				power_state = 0;
			else
				power_state = 1;

			return copy_to_user(argp, &power_state, sizeof(power_state)) ? -EFAULT : 0;
		}

	case MTKFB_CONFIG_IMMEDIATE_UPDATE:
		{
			MTKFB_LOG("[%s] MTKFB_CONFIG_IMMEDIATE_UPDATE, enable = %lu\n", __func__,
				  arg);
			if (down_interruptible(&sem_early_suspend)) {
				MTKFB_ERR("[mtkfb_ioctl] can't get semaphore:%d\n", __LINE__);
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
			unsigned long pbuf = 0;

			if (copy_from_user(&pbuf, (void __user *)arg, sizeof(pbuf))) {
				MTKFB_ERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				dprec_logger_start(DPREC_LOGGER_WDMA_DUMP, 0, 0);
				MTKFB_LOG("pbuf=0x%lx\n", pbuf);
				primary_display_capture_framebuffer_ovl(pbuf, eBGRA8888);
				dprec_logger_done(DPREC_LOGGER_WDMA_DUMP, 0, 0);
			}

			return r;
		}

	case MTKFB_SLT_AUTO_CAPTURE:
		{
			struct fb_slt_catpure capConfig;

			if (copy_from_user(&capConfig, (void __user *)arg, sizeof(capConfig))) {
				MTKFB_ERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				unsigned int format;

				switch (capConfig.format) {
				case MTK_FB_FORMAT_RGB888:
					format = eRGB888;
					break;
				case MTK_FB_FORMAT_BGR888:
					format = eBGR888;
					break;
				case MTK_FB_FORMAT_ARGB8888:
					format = eARGB8888;
					break;
				case MTK_FB_FORMAT_RGB565:
					format = eRGB565;
					break;
				case MTK_FB_FORMAT_UYVY:
					format = eYUV_420_2P_UYVY;
					break;
				case MTK_FB_FORMAT_ABGR8888:
				default:
					format = eABGR8888;
					break;
				}
				primary_display_capture_framebuffer_ovl((unsigned long)
									capConfig.outputBuffer,
									format);
			}

			return r;
		}

#ifdef MTK_FB_OVERLAY_SUPPORT
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
			struct fb_overlay_layer layerInfo;
			primary_disp_input_config input;

			if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
				MTKFB_ERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
				r = -EFAULT;
			} else {
				/* in early suspend mode ,will not update buffer index, info SF by return value */
				if (primary_display_is_sleepd()) {
					MTKFB_ERR("set overlay in early suspend ,skip!\n");
					return MTKFB_ERROR_IS_EARLY_SUSPEND;
				}

				memset((void *)&input, 0, sizeof(primary_disp_input_config));
				_convert_fb_layer_to_disp_input(&layerInfo, &input);
				primary_display_config_input(&input);
				primary_display_trigger(1, NULL, 0);

#if defined(MTK_ALPS_BOX_SUPPORT)
				MTKFB_LOG("%s MTKFB_SET_OVERLAY_LAYER ext_disp_config_input\n",
					  __func__);
				if (factory_mode) {
					ext_disp_config_input((ext_disp_input_config *) &input);
					ext_disp_trigger(true, NULL, 0);
				}
#endif

			}

			return r;
		}

	case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT:
		{
			MTKFB_ERR("mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT\n");
			/* call info dump function here */
			/* mtkfb_dump_layer_info(); */
			return r;
		}

	case MTKFB_ERROR_INDEX_UPDATE_TIMEOUT_AEE:
		{
			MTKFB_ERR("mtkfb_ioctl():MTKFB_ERROR_INDEX_UPDATE_TIMEOUT\n");
			/* call info dump function here */
			/* mtkfb_dump_layer_info(); */
			/* mtkfb_aee_print("surfaceflinger-mtkfb blocked"); */
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

			struct fb_overlay_layer layerInfo[VIDEO_LAYER_COUNT];

			MTKFB_LOG(" mtkfb_ioctl():MTKFB_SET_VIDEO_LAYERS\n");
			MMProfileLog(MTKFB_MMP_Events.SetOverlayLayers, MMProfileFlagStart);

			if (copy_from_user(&layerInfo, (void __user *)arg, sizeof(layerInfo))) {
				MTKFB_LOG("[FB]: copy_from_user failed! line:%d\n", __LINE__);
				MMProfileLogMetaString(MTKFB_MMP_Events.SetOverlayLayers,
						       MMProfileFlagEnd, "Copy_from_user failed!");
				r = -EFAULT;
			} else {
				int32_t i;
				primary_disp_input_config input;
				/* mutex_lock(&OverlaySettingMutex); */
				for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
					memset((void *)&input, 0,
					       sizeof(primary_disp_input_config));
					_convert_fb_layer_to_disp_input(&layerInfo[i], &input);
					primary_display_config_input(&input);

#if defined(MTK_ALPS_BOX_SUPPORT)

					if (factory_mode) {
						MTKFB_LOG("%s  ext_disp_config_input\n", __func__);
						ext_disp_config_input((ext_disp_input_config *) &
								      input);
					}
#endif

				}
				/* is_ipoh_bootup = false; */
				/* atomic_set(&OverlaySettingDirtyFlag, 1); */
				/* atomic_set(&OverlaySettingApplied, 0); */
				/* mutex_unlock(&OverlaySettingMutex); */

				primary_display_trigger(1, NULL, 0);

#if defined(MTK_ALPS_BOX_SUPPORT)
				if (factory_mode) {
					MTKFB_LOG("%s ext_disp_trigger\n", __func__);
					ext_disp_trigger(true, NULL, 0);
				}
#endif

			}

			return r;
		}

	case MTKFB_TRIG_OVERLAY_OUT:
		{
			MTKFB_LOG(" mtkfb_ioctl():MTKFB_TRIG_OVERLAY_OUT\n");
			MMProfileLog(MTKFB_MMP_Events.TrigOverlayOut, MMProfileFlagPulse);
			primary_display_trigger(1, NULL, 0);
#if defined(MTK_ALPS_BOX_SUPPORT)
			MTKFB_LOG(" mtkfb_ioctl():MTKFB_TRIG_OVERLAY_OUT\n");
			if (factory_mode) {
				DISPMSG("%s ext_disp_trigger\n", __func__);
				ext_disp_trigger(true, NULL, 0);
			}
#endif

			/* return mtkfb_update_screen(info); */
			return 0;
		}

#endif				/* MTK_FB_OVERLAY_SUPPORT */

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

			MTKFB_LOG("MTKFB_GET_DEFAULT_UPDATESPEED is %d\n", speed);
			return copy_to_user(argp, &speed, sizeof(speed)) ? -EFAULT : 0;
		}

	case MTKFB_GET_CURR_UPDATESPEED:
		{
			unsigned int speed;

			MTKFB_LOG("[MTKFB] get current update speed\n");
			/* DISP_Get_Current_UpdateSpeed(&speed); */

			MTKFB_LOG("[MTKFB EM]MTKFB_GET_CURR_UPDATESPEED is %d\n", speed);
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

				MTKFB_LOG("[MTKFB EM]MTKFB_CHANGE_UPDATESPEED is %d\n", speed);

			}
			return r;
		}

	case MTKFB_AEE_LAYER_EXIST:
		{
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

			MTKFB_LOG("factory mode: lcm auto test\n");
			result = mtkfb_fm_auto_test();
			return copy_to_user(argp, &result, sizeof(result)) ? -EFAULT : 0;
		}
/* #if defined (MTK_FB_SYNC_SUPPORT) */
	default:
		MTKFB_ERR("mtkfb_ioctl Not support, info=0x%08x, cmd=0x%08x, arg=0x%08x\n",
			  (unsigned int)(unsigned long)info, (unsigned int)cmd, (unsigned int)arg);
		return -EINVAL;
	}
}

#ifdef CONFIG_COMPAT

static void compat_convert(struct compat_fb_overlay_layer *compat_info,
			   struct fb_overlay_layer *info)
{
	info->layer_id = compat_info->layer_id;
	info->layer_enable = compat_info->layer_enable;
	info->src_base_addr = (void *)((unsigned long)(compat_info->src_base_addr));
	info->src_phy_addr = (void *)((unsigned long)(compat_info->src_phy_addr));
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

	pr_debug("[FB Driver] mtkfb_compat_ioctl, cmd=0x%08x, cmd nr=0x%08x, cmd size=0x%08x\n", cmd,
	       (unsigned int)_IOC_NR(cmd), (unsigned int)_IOC_SIZE(cmd));

	switch (cmd) {
	case MTKFB_GET_FRAMEBUFFER_MVA:
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
	case MTKFB_GET_DISPLAY_IF_INFORMATION:
		{
			compat_uint_t __user *data32;
			compat_uint_t displayid = 0;

			data32 = compat_ptr(arg);
			if (get_user(displayid, data32)) {
				pr_err("MTKFB_GET_DISPLAY_IF_INFORMATION failed\n");
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
				&(dispif_info[displayid]), sizeof(mtk_dispif_info_t))) {
				pr_err("[FB]: copy_to_user failed! line:%d\n", __LINE__);
				return -EFAULT;
			}
			break;
		}
	case MTKFB_POWEROFF:
		{
			ret = mtkfb_ioctl(info, MTKFB_POWEROFF, arg);
			break;
		}

	case MTKFB_POWERON:
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
			compat_ulong_t l;

			data32 = compat_ptr(arg);
			pbuf = compat_alloc_user_space(sizeof(unsigned long));
			ret = get_user(l, data32);
			ret |= put_user(l, pbuf);
			primary_display_capture_framebuffer_ovl(*pbuf, eBGRA8888);
			break;
		}
	case MTKFB_TRIG_OVERLAY_OUT:
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
			struct compat_fb_overlay_layer compat_layerInfo;

			MTKFB_LOG(" mtkfb_compat_ioctl():MTKFB_SET_OVERLAY_LAYER\n");

			arg = (unsigned long)compat_ptr(arg);
			if (copy_from_user(&compat_layerInfo, (void __user *)arg, sizeof(compat_layerInfo))) {
				MTKFB_LOG("[FB Driver]: copy_from_user failed! line:%d\n",
					  __LINE__);
				ret = -EFAULT;
			} else {
				primary_disp_input_config input;

				compat_convert(&compat_layerInfo, &layerInfo);

				/* in early suspend mode ,will not update buffer index, info SF by return value */
				if (primary_display_is_sleepd()) {
					pr_err("[FB Driver] error, set overlay in early suspend ,skip!\n");
					return MTKFB_ERROR_IS_EARLY_SUSPEND;
				}

				memset((void *)&input, 0, sizeof(primary_disp_input_config));
				_convert_fb_layer_to_disp_input(&layerInfo, &input);
				primary_display_config_input(&input);
				primary_display_trigger(1, NULL, 0);

			#if defined(MTK_ALPS_BOX_SUPPORT)
				MTKFB_LOG("%s COMPAT_MTKFB_SET_OVERLAY_LAYER ext_disp_config_input\n",
					  __func__);
				if (factory_mode) {
					ext_disp_config_input((ext_disp_input_config *) &input);
					ext_disp_trigger(true, NULL, 0);
				}
			#endif
			}
		}
		break;

	case COMPAT_MTKFB_SET_VIDEO_LAYERS:
		{
			struct compat_fb_overlay_layer compat_layerInfo[VIDEO_LAYER_COUNT];

			MTKFB_LOG(" mtkfb_compat_ioctl():MTKFB_SET_VIDEO_LAYERS\n");

			if (copy_from_user(&compat_layerInfo, (void __user *)arg, sizeof(compat_layerInfo))) {
				MTKFB_LOG("[FB Driver]: copy_from_user failed! line:%d\n",
					  __LINE__);
				ret = -EFAULT;
			} else {
				int32_t i;
				primary_disp_input_config input;

				for (i = 0; i < VIDEO_LAYER_COUNT; ++i) {
					compat_convert(&compat_layerInfo[i], &layerInfo);
					memset((void *)&input, 0, sizeof(primary_disp_input_config));
					_convert_fb_layer_to_disp_input(&layerInfo, &input);
					primary_display_config_input(&input);

				#if defined(MTK_ALPS_BOX_SUPPORT)
					if (factory_mode) {
						MTKFB_LOG("%s  ext_disp_config_input\n", __func__);
						ext_disp_config_input((ext_disp_input_config *) &
								      input);
					}
				#endif
				}
				primary_display_trigger(1, NULL, 0);

			#if defined(MTK_ALPS_BOX_SUPPORT)
				if (factory_mode) {
					MTKFB_LOG("%s ext_disp_trigger\n", __func__);
					ext_disp_trigger(true, NULL, 0);
				}
			#endif
			}
		}
		break;
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
	case MTKFB_META_SHOW_BOOTLOGO:
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
	LOG_PRINT(ANDROID_LOG_INFO, "AppLaunch", "mtkfb_pan_display_proxy.\n");
#endif
	return mtkfb_pan_display_impl(var, info);
}

static void mtkfb_blank_suspend(void);
static void mtkfb_blank_resume(void);

#if defined(CONFIG_PM_AUTOSLEEP)
static int mtkfb_blank(int blank_mode, struct fb_info *info)
{
	if (get_boot_mode() == RECOVERY_BOOT)
		return 0;

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		mtkfb_blank_resume();
	#ifdef CONFIG_MTK_LEDS
	#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
			get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT)
			mt65xx_leds_brightness_set(6, 255);
	#endif
	#endif
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
		break;
	case FB_BLANK_POWERDOWN:
	#ifdef CONFIG_MTK_LEDS
	#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
		if (get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT ||
			get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT)
			mt65xx_leds_brightness_set(6, 0);
	#endif
	#endif
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

/*
 * ---------------------------------------------------------------------------
 * Sysfs interface
 * ---------------------------------------------------------------------------
 */

static int mtkfb_register_sysfs(struct mtkfb_device *fbdev)
{
	/*NOT_REFERENCED(fbdev); */

	return 0;
}

static void mtkfb_unregister_sysfs(struct mtkfb_device *fbdev)
{
	/*NOT_REFERENCED(fbdev); */
}

/*
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

	MTKFB_FUNC();

	BUG_ON(!fbdev->fb_va_base);
	info->fbops = &mtkfb_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->screen_base = (char *)fbdev->fb_va_base;
	info->screen_size = fbdev->fb_size_in_byte;
	info->pseudo_palette = fbdev->pseudo_palette;

	r = fb_alloc_cmap(&info->cmap, 32, 0);
	if (r != 0)
		MTKFB_ERR("unable to allocate color map memory\n");

	/* setup the initial video mode (RGB565) */

	memset(&var, 0, sizeof(var));

	var.xres = MTK_FB_XRES;
	var.yres = MTK_FB_YRES;
	var.xres_virtual = MTK_FB_XRESV;
	var.yres_virtual = MTK_FB_YRESV;
	/* xuecheng, ROME will use 32 bit framebuffer as default */
	var.bits_per_pixel = 32;

	var.transp.offset = 24;
	var.transp.length = 8;
#ifdef CONFIG_MTK_GPU_SUPPORT
	/* default as BGRA, GPU(default data is BGRA too) will not change this value at home screen */
	var.red.offset = 16;
	var.red.length = 8;
	var.green.offset = 8;
	var.green.length = 8;
	var.blue.offset = 0;
	var.blue.length = 8;
#else
	/* 0xRGBA */
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
		MTKFB_ERR("failed to mtkfb_check_var\n");

	info->var = var;

	r = mtkfb_set_par(info);
	if (r != 0)
		MTKFB_ERR("failed to mtkfb_set_par\n");

	MSG_FUNC_LEAVE();
	return r;
}

#ifdef DDP_UT
static int mtkfb_fbinfo_modify(struct fb_info *info, unsigned int fmt)
{
	struct fb_var_screeninfo var;
	int r = 0;

	memcpy(&var, &(info->var), sizeof(var));
	var.activate = FB_ACTIVATE_NOW;
	var.bits_per_pixel = 32;
	var.transp.offset = 24;


	if (fmt == MTK_FB_FORMAT_BGRA8888) {
		/* default as BGRA, GPU(default data is BGRA too) will not change this value at home screen */
		var.red.offset = 16;
		var.red.length = 8;
		var.green.offset = 8;
		var.green.length = 8;
		var.blue.offset = 0;
		var.blue.length = 8;
	} else {
		/* 0xRGBA */
		var.red.offset = 0;
		var.red.length = 8;
		var.green.offset = 8;
		var.green.length = 8;
		var.blue.offset = 16;
		var.blue.length = 8;
	}

	/* var.yoffset = var.yres; */

	r = mtkfb_check_var(&var, info);
	if (r != 0)
		MTKFB_ERR("failed to mtkfb_check_var\n");

	info->var = var;

	r = mtkfb_set_par(info);
	if (r != 0)
		MTKFB_ERR("failed to mtkfb_set_par\n");

	return r;
}
#endif

/* Release the fb_info object */
static void mtkfb_fbinfo_cleanup(struct mtkfb_device *fbdev)
{
	MSG_FUNC_ENTER();

	fb_dealloc_cmap(&fbdev->fb_info->cmap);

	MSG_FUNC_LEAVE();
}

#define RGB565_TO_ARGB8888(x)   \
	((((x) &   0x1F) << 3) |    \
	(((x) &  0x7E0) << 5) |    \
	(((x) & 0xF800) << 8) |    \
	(0xFF << 24))		/* opaque */

/* Init frame buffer content as 3 R/G/B color bars for debug */
static int init_framebuffer(struct fb_info *info)
{
	void *buffer = info->screen_base + info->var.yoffset * info->fix.line_length;

	/* clean the current frame buffer as black */
	/* the ioremap_nocache memory will not support memset/memcpy */
	memset_io(buffer, 0, info->screen_size/MTK_FB_PAGES);

	return 0;
}


/* Free driver resources. Can be called to rollback an aborted initialization
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
		dma_free_coherent(0, fbdev->fb_size_in_byte, fbdev->fb_va_base, fbdev->fb_pa_base);
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

char mtkfb_lcm_name[256] = { 0 };

void disp_get_fb_address(UINT32 *fbVirAddr, UINT32 *fbPhysAddr)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;

	*fbVirAddr =
	    (UINT32) (unsigned long)fbdev->fb_va_base +
	    mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
	*fbPhysAddr =
	    (UINT32) fbdev->fb_pa_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
}

void disp_clear_current_fb_buffer(void)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	void *fb_vaddr = fbdev->fb_va_base + mtkfb_fbi->var.yoffset * mtkfb_fbi->fix.line_length;
	int fb_size = MTK_FB_XRES * MTK_FB_YRES * MTK_FB_BPP / 8;

	memset(fb_vaddr, 0x00, fb_size);
}

#ifdef DDP_UT
static void _mtkfb_draw_block(unsigned long addr, unsigned int x, unsigned int y, unsigned int w,
			      unsigned int h, unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr + MTK_FB_XRESV * 4 * y + x * 4;

	MTKFB_MSG("@(%d,%d)addr=0x%lx, MTK_FB_XRESV=%d, draw_block start addr=0x%lx, w=%d, h=%d\n",
		  x, y, addr, MTK_FB_XRESV, start_addr, w, h);

	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++)
			*(unsigned int *)(start_addr + i * 4 + j * MTK_FB_XRESV * 4) = color;

	}

}
#endif

char *mtkfb_find_lcm_driver(void)
{
	/* BOOL ret = false; */

	char *p;
	char *q;

	p = strstr(saved_command_line, "lcm=");
	if (p == NULL) {
		/* we can't find lcm string in the command line, the uboot should be old version */
		return NULL;
	}

	p += 6;
	if ((p - saved_command_line) > strlen(saved_command_line + 1))
		return NULL;

	MTKFB_LOG("%s, %s\n", __func__, p);
	q = p;
	while (*q != ' ' && *q != '\0')
		q++;

	memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char *)mtkfb_lcm_name, (const char *)p, (int)(q - p));
	mtkfb_lcm_name[q - p + 1] = '\0';

	MTKFB_LOG("%s, %s\n", __func__, mtkfb_lcm_name);
	return mtkfb_lcm_name;
}


#if 0
static long int get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}
#endif

UINT32 color = 0;
unsigned int mtkfb_fm_auto_test(void)
{
	unsigned int result = 0;
	unsigned int i = 0;
	UINT32 fbVirAddr;
	UINT32 fbsize;
	int r = 0;
	unsigned int *fb_buffer;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	struct fb_var_screeninfo var;

	fbVirAddr = (UINT32) (unsigned long)fbdev->fb_va_base;
	fb_buffer = (unsigned int *)(unsigned long)fbVirAddr;

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
		MTKFB_ERR("failed to mtkfb_check_var\n");

	mtkfb_fbi->var = var;

#if 0
	r = mtkfb_set_par(mtkfb_fbi);

	if (r != 0)
		MTKFB_ERR("failed to mtkfb_set_par\n");
#endif
	if (color == 0)
		color = 0xFF00FF00;
	fbsize =
	    ALIGN_TO(DISP_GetScreenWidth(),
		     MTK_FB_ALIGNMENT) * DISP_GetScreenHeight() * MTK_FB_PAGES;
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
		MTKFB_ERR("ATA LCM failed\n");
	else
		MTKFB_LOG("ATA LCM passed\n");


	return result;
}


#ifdef DDP_UT
static int _mtkfb_internal_test(unsigned long va, unsigned int w, unsigned int h, unsigned int fmt)
{
	/* this is for debug, used in bring up day */
	unsigned int i = 0, j, num;
	unsigned int color = 0;
	int _internal_test_block_size = 120;
	unsigned int *pAddr = (unsigned int *)va;

	MTKFB_MSG("UT starts @ va=0x%lx, w=%d, h=%d\n", va, w, h);

	/* mtkfb_fbinfo_init default config as RGBA-->0xAABBGGRR */
	MTKFB_MSG("R\n");
	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			*(pAddr + i * w + j) =
			    (fmt == MTK_FB_FORMAT_BGRA8888) ? 0xFFFF0000U : 0xFF0000FFU;
		}
	}
	primary_display_trigger(1, NULL, 0);
	msleep(1000);

	MTKFB_MSG("G\n");
	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++)
			*(pAddr + i * w + j) = 0xFF00FF00U;
	}
	primary_display_trigger(1, NULL, 0);
	msleep(1000);

	MTKFB_MSG("B\n");
	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			*(pAddr + i * w + j) =
			    (fmt == MTK_FB_FORMAT_BGRA8888) ? 0xFF0000FFU : 0xFFFF0000U;
		}
	}
	primary_display_trigger(1, NULL, 0);
	msleep(1000);

	for (num = 160; num < 200; num += 40) {
		_internal_test_block_size = num;
		for (i = 0; i < w * h / _internal_test_block_size / _internal_test_block_size; i++) {
			color = (i & 0x1) * 0xff;
			color += ((i & 0x2) >> 1) * 0xff00;
			color += ((i & 0x4) >> 2) * 0xff0000;
			color += 0xff000000U;
			_mtkfb_draw_block(va,
					  i % (w / _internal_test_block_size) *
					  _internal_test_block_size,
					  i / (w / _internal_test_block_size) *
					  _internal_test_block_size, _internal_test_block_size,
					  _internal_test_block_size, color);
		}
		primary_display_trigger(1, NULL, 0);
		msleep(1000);
	}

	return 0;
}
#endif

#ifdef CONFIG_OF
struct tag_videolfb {
	u16 lfb_width;
	u16 lfb_height;
	u16 lfb_depth;
	u16 lfb_linelength;
	u32 lfb_base;
	u32 lfb_size;
	u8 red_size;
	u8 red_pos;
	u8 green_size;
	u8 green_pos;
	u8 blue_size;
	u8 blue_pos;
	u8 rsvd_size;
	u8 rsvd_pos;

};
unsigned int islcmconnected = 0;
unsigned int vramsize = 0;
phys_addr_t fb_base = 0;
unsigned long fb_base_va = 0;
static int is_videofb_parse_done;
/* 0: success / 1: fail */
int _parse_tag_videolfb(void)
{
#ifdef CONFIG_FPGA_EARLY_PORTING	/* for the case of no LK */
	struct device_node *np;
	struct resource res;
	unsigned int lcm_frame_size = 0;
	LCM_PARAMS *lcm_param_primary = NULL;

	lcm_param_primary = kzalloc(sizeof(uint8_t *) * sizeof(LCM_PARAMS), GFP_KERNEL);

	if (is_videofb_parse_done)
		return 0;

	np = of_find_compatible_node(NULL, NULL, "mediatek,MTKFB");
	if (np == NULL) {
		MTKFB_ERR("DT <mediatek,MTKFB> is not config\n");
		return 0;
	}

	if (of_address_to_resource(mtkfb_fbdev->dev.of_node, 0, &res)) {
		MTKFB_ERR("mtkfb of_address_to_resource failed\n");
		return 0;
	}
	if (lcm_driver_list[0] == NULL)	/* peeking lcm param before primary_display does lcm_probe */
		MTKFB_ERR("[MTKFB ERROR] there is no lcm found so peeking lcm param fail");
	else
		islcmconnected = 1;
	lcm_driver_list[0]->get_params(lcm_param_primary);
	lcm_frame_size = lcm_param_primary->width * lcm_param_primary->height;

	/* vramsize = ALIGN_TO(480*800*4*3+480*800*2+4096, 0x1000);      //DAL BPP=2 */
	vramsize = ALIGN_TO(lcm_frame_size * 4 * 3 + lcm_frame_size * 2 /* + 4096 */ , 0x100000);
	fb_base = ALIGN_TO(res.end - vramsize, 0x1000);
	MTKFB_LOG("[DT][videolfb] res.end       = 0x%x\n", res.end);
#else
	struct tag_videolfb *videolfb_tag = NULL;
	void *tag_ptr = NULL;

	if (is_videofb_parse_done)
		return 0;


	if (lcm_driver_list[0] == NULL)	/* peeking lcm param before primary_display does lcm_probe */
		MTKFB_ERR("[MTKFB ERROR] there is no lcm found so peeking lcm param fail");
	else
		islcmconnected = 1;

	if (of_chosen) {
		tag_ptr = (void *)of_get_property(of_chosen, "atag,videolfb", NULL);
		videolfb_tag = (struct tag_videolfb *)(tag_ptr + 8);
		if (videolfb_tag) {
			vramsize = videolfb_tag->lfb_size;
			fb_base = videolfb_tag->lfb_base;
		} else {
			MTKFB_ERR("[DT][videolfb] videolfb_tag not found\n");
			return 0;
		}
	} else {
		MTKFB_ERR("[DT][videolfb] of_chosen not found\n");
		return 0;
	}
#endif

	is_videofb_parse_done = 1;
	MTKFB_LOG("[DT][videolfb] islcmfound!! = %d\n", islcmconnected);
	MTKFB_LOG("[DT][videolfb] fps        = %d\n", lcd_fps);
	MTKFB_LOG("[DT][videolfb] fb_base    = 0x%lx\n", (unsigned long)fb_base);
	MTKFB_LOG("[DT][videolfb] vram       = 0x%x\n", vramsize);
	MTKFB_LOG("[DT][videolfb] lcmname    = %s\n", mtkfb_lcm_name);

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

#ifdef PAN_DISPLAY_TEST
static int update_test_kthread(void *data)
{
	/* struct sched_param param = { .sched_priority = RTPM_PRIO_SCRN_UPDATE }; */
	/* sched_setscheduler(current, SCHED_RR, &param); */
	unsigned int i = 0;

	for (;;) {

		if (kthread_should_stop())
			break;
		msleep(2000);	/* 2s */
		MTKFB_MSG("update test thread work,offset = %d\n", i);
		mtkfb_fbi->var.yoffset = (i % 3) * primary_display_get_height();
		i++;
		mtkfb_pan_display_impl(&mtkfb_fbi->var, mtkfb_fbi);
	}


	MTKFB_MSG("exit esd_recovery_kthread()\n");
	return 0;
}
#endif

bool boot_up_with_facotry_mode(void)
{
#if defined(MTK_ALPS_BOX_SUPPORT)
	return factory_mode;
#else
	return 0;
#endif
}


static ssize_t fb_get_panel_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* struct fb_info *fbi = dev_get_drvdata(dev); */
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE,
		"panel_name=%s\n", mtkfb_lcm_name);

	return ret;
}

static ssize_t fb_set_dispparam(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int param;
	sscanf(buf, "0x%x", &param);

	MTKFB_FUNC();
	MTKFB_LOG("fb_set_dispparam:%d Start\n", param);
	primary_display_set_panel_param(param);
	MTKFB_LOG("fb_set_dispparam End\n");

	return size;
}

static DEVICE_ATTR(fb_panel_info, S_IRUGO, fb_get_panel_info, NULL);
static DEVICE_ATTR(fb_dispparam, 0644, NULL, fb_set_dispparam);

static struct attribute *fb_attrs[] = {

	&dev_attr_fb_panel_info.attr,
	&dev_attr_fb_dispparam.attr,

	NULL,
};

static struct attribute_group fb_attr_group = {
	.attrs = fb_attrs,
};

static int fb_create_sysfs(struct fb_info *fbi)
{
	int rc;

	rc = sysfs_create_group(&fbi->dev->kobj, &fb_attr_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);
	return rc;
}

static int mtkfb_probe(struct platform_device *pdev)
{
	struct mtkfb_device *fbdev = NULL;
	struct fb_info *fbi;
	int init_state;
	int r = 0;
	int i = 0;
	int bug_idx = -1;
	struct device_node *larb_node[2];
	struct platform_device *larb_pdev[2];

	DISPFUNC();

	MTKFB_MSG("mtkfb_probe start\n");

	larb_node[0] = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!larb_node[0])
		return -EINVAL;

	larb_node[1] = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 1);
	if (!larb_node[1])
		return -EINVAL;

	larb_pdev[0] = of_find_device_by_node(larb_node[0]);
	of_node_put(larb_node[0]);
	larb_pdev[1] = of_find_device_by_node(larb_node[1]);
	of_node_put(larb_node[1]);

	if ((!larb_pdev[0]) || (!larb_pdev[0]->dev.driver) ||
		(!larb_pdev[0]) || (!larb_pdev[1]->dev.driver)) {
		MTKFB_ERR("mtkfb_probe is earlier than SMI\n");
		return -EPROBE_DEFER;
	}

	if (get_boot_mode() == META_BOOT || get_boot_mode() == FACTORY_BOOT
	    || get_boot_mode() == ADVMETA_BOOT || get_boot_mode() == RECOVERY_BOOT)
		first_update = false;

	init_state = 0;

	mtkfb_fbdev = pdev;

	/* peeking lcm name before primary_display does lcm_probe */
	if (lcm_driver_list[0] == NULL)
		MTKFB_ERR("[MTKFB ERROR] there is no lcm found so peeking lcm param fail");

	memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char *)mtkfb_lcm_name, lcm_driver_list[0]->name, 255);
	mtkfb_lcm_name[strlen(mtkfb_lcm_name)] = '\0';
	lcd_fps = 6000;

#ifdef CONFIG_OF
	_parse_tag_videolfb();
#else
	MTKFB_LOG("%s, %s\n", __func__, saved_command_line);
	p = strstr(saved_command_line, "fps=");
	if (p == NULL) {
		lcd_fps = 6000;
		MTKFB_ERR("[FB driver]can not get fps from uboot\n");
	} else {
		p += 4;
		lcd_fps = kstrtol(p, NULL, 10);
		if (0 == lcd_fps)
			lcd_fps = 6000;
	}
#endif

	fbi = framebuffer_alloc(sizeof(struct mtkfb_device), &pdev->dev);
	if (!fbi) {
		MTKFB_ERR("unable to allocate memory for device info\n");
		r = -ENOMEM;
		goto cleanup;
	}
	mtkfb_fbi = fbi;

	fbdev = (struct mtkfb_device *)fbi->par;
	fbdev->fb_info = fbi;
	fbdev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, fbdev);

	{
#ifdef CONFIG_OF
		MTKFB_LOG("mtkfb_probe:get FB MEM REG\n");
		/* _parse_tag_videolfb(); */
		MTKFB_LOG("mtkfb_probe: fb_pa = %pa\n", &fb_base);

		disp_hal_allocate_framebuffer(fb_base, (fb_base + (unsigned long)vramsize - 1),
					      (unsigned long *)&fbdev->fb_va_base, &fb_pa);
		fb_base_va = (unsigned long)fbdev->fb_va_base;
		fbdev->fb_pa_base = fb_base;
#else
		struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		/* ASSERT(DISP_GetVRamSize() <= (res->end - res->start + 1)); */
		disp_hal_allocate_framebuffer(res->start, res->end,
					      (unsigned long *)&fbdev->fb_va_base, &fb_pa);
		fbdev->fb_pa_base = res->start;
#endif
	}

#ifdef CONFIG_OF
	for (i = 0; i < MM_CLK_NUM; i++) {
		ddp_clk_map[i] = devm_clk_get(&pdev->dev, ddp_get_clk_name(i));
		if (IS_ERR(ddp_clk_map[i]))
			bug_idx = i;
		MTKFB_LOG("***DT|DISPSYS clock|%s ID %d: 0x%lx\n", ddp_get_clk_name(i), i,
			  (unsigned long)ddp_clk_map[i]);
	}
	if (bug_idx >= 0) {
		MTKFB_ERR("DISP get clock error on ddp_clk_map[%d]:%s\n", bug_idx,
			  ddp_get_clk_name(bug_idx));
		BUG_ON(1);
	}
#endif

	/* mtkfb should parse lcm name from kernel boot command line */
	primary_display_set_frame_buffer_address((unsigned long)fbdev->fb_va_base,
						 (unsigned long)fb_pa);
	primary_display_init(pdev, mtkfb_find_lcm_driver(), lcd_fps);

#if defined(MTK_ALPS_BOX_SUPPORT)
	{
		int tmp_boot_mode;

		tmp_boot_mode = get_boot_mode();
		if ((tmp_boot_mode == FACTORY_BOOT) || (tmp_boot_mode == ATE_FACTORY_BOOT)
		    || (tmp_boot_mode == RECOVERY_BOOT)) {
			DISPMSG("%s factory_mode\n", __func__);
			factory_mode = true;
		}

		MTKFB_LOG("%s ext_disp_config_input boot_mode %d factory_mode %d\n", __func__,
			  tmp_boot_mode, factory_mode);

		/*
		   ext_disp_set_frame_buffer_address((unsigned long)fbdev->fb_va_base,
		   (unsigned long)fb_pa);
		 */
		if (factory_mode) {
			dpi_setting_res(0xb);
			ext_disp_init(0, 0, 0x20001);
		}
	}
#endif

	init_state++;		/* 1 */
	MTK_FB_XRES = primary_display_get_width();
	MTK_FB_YRES = primary_display_get_height();
	fb_xres_update = MTK_FB_XRES;
	fb_yres_update = MTK_FB_YRES;

	MTK_FB_BPP = primary_display_get_bpp();
	MTK_FB_PAGES = primary_display_get_pages();
	DISPMSG
	    ("MTK_FB_XRES=%d, MTKFB_YRES=%d, MTKFB_BPP=%d, MTK_FB_PAGES=%d, MTKFB_LINE=%d, MTKFB_SIZEV=%d\n",
	     MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	fbdev->fb_size_in_byte = MTK_FB_SIZEV;

	/* Allocate and initialize video frame buffer */

	DISPMSG("[FB Driver] fbdev->fb_pa_base = %pa, fbdev->fb_va_base = 0x%lx\n",
		&(fbdev->fb_pa_base), (unsigned long)(fbdev->fb_va_base));

	if (!fbdev->fb_va_base) {
		MTKFB_ERR("unable to allocate memory for frame buffer\n");
		r = -ENOMEM;
		goto cleanup;
	}
	init_state++;		/* 2 */

	/* Register to system */

	r = mtkfb_fbinfo_init(fbi);
	if (r) {
		MTKFB_ERR("mtkfb_fbinfo_init fail, r = %d\n", r);
		goto cleanup;
	}
	init_state++;		/* 4 */

	{
		/* dal_init should after mtkfb_fbinfo_init, otherwise layer 3 will show dal background color */
		DAL_STATUS ret;
		unsigned long fbVA = (unsigned long)fbdev->fb_va_base;
		unsigned int fbPA = fb_pa;
		/* / DAL init here */
		fbVA += DISP_GetFBRamSize();
		fbPA += DISP_GetFBRamSize();
		ret = DAL_Init(fbVA, fbPA);
		/* DAL_Printf("===================================\n"); */
		/* DAL_Printf("===================================\n"); */
		/* DAL_Printf("===================================\n"); */
		/* DAL_Printf("===================================\n"); */
	}

#ifdef DDP_UT
	/* check BGRA color format */
	_mtkfb_internal_test((unsigned long)fbdev->fb_va_base, MTK_FB_XRES, MTK_FB_YRES,
			     MTK_FB_FORMAT_BGRA8888);
	/* check RGBA color format */
	mtkfb_fbinfo_modify(fbi, MTK_FB_FORMAT_RGBA8888);
	_mtkfb_internal_test((unsigned long)fbdev->fb_va_base, MTK_FB_XRES, MTK_FB_YRES,
			     MTK_FB_FORMAT_RGBA8888);
	/* change back to BGRA as default */
	mtkfb_fbinfo_modify(fbi, MTK_FB_FORMAT_BGRA8888);
#endif

	r = mtkfb_register_sysfs(fbdev);
	if (r) {
		MTKFB_ERR("mtkfb_register_sysfs fail, r = %d\n", r);
		goto cleanup;
	}
	init_state++;		/* 5 */

	r = register_framebuffer(fbi);
	if (r != 0) {
		MTKFB_ERR("register_framebuffer failed\n");
		goto cleanup;
	}

	/*primary_display_diagnose(); */

	fbdev->state = MTKFB_ACTIVE;
	MTKFB_MSG("mtkfb_probe done\n");

	fb_create_sysfs(fbi);

#ifdef PAN_DISPLAY_TEST
	{
		struct task_struct *update_test_task = NULL;
		void *fb_va = (void *)fbdev->fb_va_base;
		unsigned int fbsize = primary_display_get_height() * primary_display_get_width();

		/* memset(fb_va, 0xFF, fbsize * 4); */
		memset(fb_va + fbsize * 4, 0x55, fbsize * 4);
		memset(fb_va + fbsize * 8, 0xaa, fbsize * 4);
		MTKFB_MSG("memset done\n");

		update_test_task = kthread_create(update_test_kthread, NULL, "update_test_kthread");

		if (IS_ERR(update_test_task))
			MTKFB_ERR("update test task create fail\n");
		else
			wake_up_process(update_test_task);
	}
#endif

	MSG_FUNC_LEAVE();
	return 0;

cleanup:
	mtkfb_free_resources(fbdev, init_state);

	MSG_FUNC_LEAVE();
	return r;
}

/* Called when the device is being detached from the driver */
static int mtkfb_remove(struct platform_device *pdev)
{
	struct mtkfb_device *fbdev = platform_get_drvdata(pdev);
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
	/*NOT_REFERENCED(pdev); */
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
		MTKFB_LOG("[FB Driver] mtkfb_ipoh_restore PM_RESTORE_PREPARE\n");
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
	MTKFB_LOG("[FB Driver] mtkfb_shutdown()\n");
	/* mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF); */
	if (!lcd_fps)
		msleep(30);
	else
		msleep(2 * 100000 / lcd_fps);	/* Delay 2 frames. */

	if (primary_display_is_sleepd()) {
		MTKFB_LOG("mtkfb has been power off\n");
		return;
	}

	/* MTKFB_LOG("[FB Driver] cci400_sel_for_ddp\n"); */
	/* cci400_sel_for_ddp(); */

	primary_display_suspend();
	MTKFB_LOG("[FB Driver] leave mtkfb_shutdown\n");
}

void mtkfb_clear_lcm(void)
{
#if 0
	int i;
	unsigned int layer_status[DDP_OVL_LAYER_MUN] = { 0 };

	mutex_lock(&OverlaySettingMutex);
	for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
		layer_status[i] = cached_layer_config[i].layer_en;
		cached_layer_config[i].layer_en = 0;
		cached_layer_config[i].isDirty = 1;
	}
	atomic_set(&OverlaySettingDirtyFlag, 1);
	atomic_set(&OverlaySettingApplied, 0);
	mutex_unlock(&OverlaySettingMutex);

	/* DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update)); */
	/* DISP_CHECK_RET(DISP_UpdateScreen(0, 0, fb_xres_update, fb_yres_update)); */
	/* DISP_WaitForLCDNotBusy(); */
	mutex_lock(&OverlaySettingMutex);
	for (i = 0; i < DDP_OVL_LAYER_MUN; i++) {
		cached_layer_config[i].layer_en = layer_status[i];
		cached_layer_config[i].isDirty = 1;
	}
	atomic_set(&OverlaySettingDirtyFlag, 1);
	atomic_set(&OverlaySettingApplied, 0);
	mutex_unlock(&OverlaySettingMutex);
#endif
}


static void mtkfb_blank_suspend(void)
{
	int ret = 0;

	MSG_FUNC_ENTER();

	MTKFB_MSG("enter early_suspend\n");
#ifdef CONFIG_MTK_LEDS
/*	mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF);*/
#endif
	msleep(30);

	ret = primary_display_suspend();
	if (ret < 0) {
		MTKFB_ERR("primary display suspend failed\n");
		return;
	}
/*	disp_clear_current_fb_buffer();*/

	MSG_FUNC_LEAVE();
	MTKFB_MSG("leave early_suspend\n");

}


#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_early_suspend(struct early_suspend *h)
{
	int ret = 0;

	MSG_FUNC_ENTER();

	MTKFB_MSG("enter early_suspend\n");
#ifdef CONFIG_MTK_LEDS
	mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD, LED_OFF);
#endif
	msleep(30);

	ret = primary_display_suspend();
	if (ret < 0) {
		MTKFB_ERR("primary display suspend failed\n");
		return;
	}
	disp_clear_current_fb_buffer();

	MSG_FUNC_LEAVE();
	MTKFB_MSG("leave early_suspend\n");

}
#endif
#endif

/* PM resume */
static int mtkfb_resume(struct platform_device *pdev)
{
	/*NOT_REFERENCED(pdev); */
	MSG_FUNC_ENTER();
	MTKFB_LOG("[FB Driver] mtkfb_resume()\n");
	MSG_FUNC_LEAVE();
	return 0;
}

static void mtkfb_blank_resume(void)
{
	int ret = 0;

	MTKFB_MSG("enter late_resume\n");
	MSG_FUNC_ENTER();

	ret = primary_display_resume();
	if (ret) {
		MTKFB_ERR("primary display resume failed\n");
		return;
	}

	MSG_FUNC_LEAVE();
	MTKFB_MSG("leave late_resume\n");
}


#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mtkfb_late_resume(struct early_suspend *h)
{
	int ret = 0;

	MTKFB_MSG("enter late_resume\n");
	MSG_FUNC_ENTER();

	ret = primary_display_resume();
	if (ret) {
		MTKFB_ERR("primary display resume failed\n");
		return;
	}

	MSG_FUNC_LEAVE();
	MTKFB_MSG("leave late_resume\n");
}
#endif
#endif

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int mtkfb_pm_suspend(struct device *device)
{
	/* pr_debug("calling %s()\n", __func__); */

	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return mtkfb_suspend(pdev, PMSG_SUSPEND);
}

int mtkfb_pm_resume(struct device *device)
{
	/* pr_debug("calling %s()\n", __func__); */

	struct platform_device *pdev = to_platform_device(device);

	BUG_ON(pdev == NULL);

	return mtkfb_resume(pdev);
}

int mtkfb_pm_restore_noirq(struct device *device)
{


	/* disphal_pm_restore_noirq(device); */

	is_ipoh_bootup = true;
	return 0;

}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define mtkfb_pm_suspend NULL
#define mtkfb_pm_resume  NULL
#define mtkfb_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
const struct dev_pm_ops mtkfb_pm_ops = {
	.suspend = mtkfb_pm_suspend,
	.resume = mtkfb_pm_resume,
	.freeze = mtkfb_pm_suspend,
	.thaw = mtkfb_pm_resume,
	.poweroff = mtkfb_pm_suspend,
	.restore = mtkfb_pm_resume,
	.restore_noirq = mtkfb_pm_restore_noirq,
};

#ifdef CONFIG_OF
static const struct of_device_id mtkfb_of_ids[] = {
	{.compatible = "mediatek,MTKFB",},
	{}
};
#endif

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
#ifdef CONFIG_OF
		   .owner = THIS_MODULE,
		   .of_match_table = mtkfb_of_ids,
#endif
		   },
};

#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
static const struct early_suspend mtkfb_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = mtkfb_early_suspend,
	.resume = mtkfb_late_resume,
};
#endif
#endif


int mtkfb_get_debug_state(char *stringbuf, int buf_len)
{
#if 0
	int len = 0;
	struct platform_device *pdev = NULL;
	struct mtkfb_device *fbdev = (struct mtkfb_device *)mtkfb_fbi->par;
	struct resource *res;
	unsigned int va;
	unsigned int mva;
	unsigned int pa;
	unsigned int resv_size;

	pdev = to_platform_device(fbdev->dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	va = (UINT32) fbdev->fb_va_base;
	mva = (UINT32) fbdev->fb_pa_base;
	pa = res->start;
	resv_size = res->end - res->start + 1;
#else
	int len = 0;
	unsigned long va;
	unsigned int mva;
	unsigned int pa;
	unsigned int resv_size;


	va = fb_base_va;
	mva = (unsigned int)fb_pa;
	pa = (unsigned int)fb_base;
	resv_size = vramsize;

#endif
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|--------------------------------------------------------------------------------------|\n");
	/* len += scnprintf(stringbuf+len, buf_len - len, "********MTKFB Driver General Information********\n"); */
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|Framebuffer VA:0x%lx, PA:0x%08x, MVA:0x%08x, Reserved Size:0x%08x|%d\n",
		      va, pa, mva, resv_size, resv_size);
	len +=
	    scnprintf(stringbuf + len, buf_len - len, "|xoffset=%d, yoffset=%d\n",
		      mtkfb_fbi->var.xoffset, mtkfb_fbi->var.yoffset);
	len +=
	    scnprintf(stringbuf + len, buf_len - len, "|framebuffer line alignment(for gpu)=%d\n",
		      MTK_FB_ALIGNMENT);
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|xres=%d, yres=%d,bpp=%d,pages=%d,linebytes=%d,total size=%d\n", MTK_FB_XRES,
		      MTK_FB_YRES, MTK_FB_BPP, MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	/* use extern in case DAL_LOCK is hold, then can't get any debug info */
	len +=
	    scnprintf(stringbuf + len, buf_len - len, "|AEE Layer is %s\n",
		      isAEEEnabled ? "enabled" : "disabled");

	return len;
}


/* Register both the driver and the device */
int __init mtkfb_init(void)
{
	int r = 0;

	MSG_FUNC_ENTER();

	MTKFB_MSG("frame buffer driver init\n");

	if (platform_driver_register(&mtkfb_driver)) {
		MTKFB_ERR("failed to register mtkfb driver\n");
		r = -ENODEV;
		goto exit;
	}
#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mtkfb_early_suspend_handler);
#endif
#endif
	PanelMaster_Init();
	DBG_Init();
	mtkfb_ipo_init();
exit:
	MSG_FUNC_LEAVE();
	MTKFB_MSG("frame buffer driver init -- leave\n");
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

	PanelMaster_Deinit();
	DBG_Deinit();

	MSG_FUNC_LEAVE();
}


module_init(mtkfb_init);
module_exit(mtkfb_cleanup);

MODULE_DESCRIPTION("MEDIATEK framebuffer driver");
MODULE_AUTHOR("Xuecheng Zhang <Xuecheng.Zhang@mediatek.com>");
MODULE_LICENSE("GPL");

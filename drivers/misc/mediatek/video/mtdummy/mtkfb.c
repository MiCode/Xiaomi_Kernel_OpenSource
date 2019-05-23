// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*#include <generated/autoconf.h>*/
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
#include <linux/types.h>
#include "mtkfb.h"
#include "mtkfb_info.h"
#include <linux/bug.h>
/* #include <linux/earlysuspend.h> */
/* #include <linux/rtpm_prio.h> */
/* #include "disp_assert_layer.h" */
/* #include <linux/xlog.h> */
/* #include <linux/leds-mt65xx.h> */
/* #include <mach/dma.h> */
/* #include <mach/irqs.h> */
/* #include "mach/mt_boot.h" */

#define ALIGN_TO(x, n)  \
	(((x) + ((n) - 1)) & ~((n) - 1))

#define SCREEN_WIDHT  (1080)
#define SCREEN_HEIGHT  (1920)

#define MTK_FB_ALIGNMENT 32

static u32 MTK_FB_XRES;
static u32 MTK_FB_YRES;
static u32 MTK_FB_BPP;
static u32 MTK_FB_PAGES;
static u32 fb_xres_update;
static u32 fb_yres_update;
#define MTK_FB_XRESV (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT))
#define MTK_FB_YRESV (ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * \
		MTK_FB_PAGES)	/* For page flipping */
#define MTK_FB_BYPP  ((MTK_FB_BPP + 7) >> 3)
#define MTK_FB_LINE  (ALIGN_TO(MTK_FB_XRES, MTK_FB_ALIGNMENT) * MTK_FB_BYPP)
#define MTK_FB_SIZE  (MTK_FB_LINE * ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT))

#define MTK_FB_SIZEV (MTK_FB_LINE * \
		ALIGN_TO(MTK_FB_YRES, MTK_FB_ALIGNMENT) * MTK_FB_PAGES)

/* ------------------------------------------------------------------------- */
/* local variables */
/* ------------------------------------------------------------------------- */

unsigned long fb_pa;
struct fb_info *mtkfb_fbi;
unsigned int lcd_fps = 6000;
char mtkfb_lcm_name[256] = { 0 };

bool is_ipoh_bootup;
bool is_early_suspended;


/* ------------------------------------------------------------------------- */
/* local function declarations */
/* ------------------------------------------------------------------------- */

int mtkfb_get_debug_state(char *stringbuf, int buf_len)
{
	return 0;
}

unsigned int mtkfb_fm_auto_test(void)
{
	return 0;
}

unsigned int DISP_GetVRamSizeBoot(void)
{
	return 0x01800000;
}

/*
 * ---------------------------------------------------------------------------
 * fbdev framework callbacks and the ioctl interface
 * ---------------------------------------------------------------------------
 */
/* Called each time the mtkfb device is opened */
static int mtkfb_open(struct fb_info *info, int user)
{
	return 0;
}

/* Called when the mtkfb device is closed. We make sure that any pending
 * gfx DMA operations are ended, before we return.
 */
static int mtkfb_release(struct fb_info *info, int user)
{
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
	unsigned int bpp, m;

	/* NOT_REFERENCED(transp); */
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
		    ((red & 0xF800) | ((green & 0xFC00) >> 5) |
		    ((blue & 0xF800) >> 11));
		break;
	case 32:
		/* ARGB8888 */
		((u32 *) (info->pseudo_palette))[regno] =
		    (0xff000000) |
		    ((red & 0xFF00) << 8) | ((green & 0xFF00)) |
		    ((blue & 0xFF00) >> 8);
		break;

		/* TODO: RGB888, BGR888, ABGR8888 */

	default:
		return -EINVAL;
	}

exit:
	return r;
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
		return;
	}

	fix->accel = FB_ACCEL_NONE;
	fix->line_length =
		ALIGN_TO(var->xres_virtual, MTK_FB_ALIGNMENT) *
		var->bits_per_pixel / 8;
	fix->smem_len = fbdev->fb_size_in_byte;
	fix->smem_start = fbdev->fb_pa_base;

	fix->xpanstep = 0;
	fix->ypanstep = 1;

	fbops->fb_fillrect = cfb_fillrect;
	fbops->fb_copyarea = cfb_copyarea;
	fbops->fb_imageblit = cfb_imageblit;
}


/* Switch to a new mode. The parameters for it has been check already by
 * mtkfb_check_var.
 */
static int mtkfb_set_par(struct fb_info *fbi)
{
	struct mtkfb_device *fbdev = (struct mtkfb_device *)fbi->par;

	set_fb_fix(fbdev);
	return 0;
}

static int mtkfb_pan_display_impl(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	pr_info("xoffset=%u, yoffset=%u, xres=%u, yres=%u, xresv=%u, yresv=%u\n",
		var->xoffset, var->yoffset, info->var.xres,
		info->var.yres, info->var.xres_virtual, info->var.yres_virtual);
	info->var.yoffset = var->yoffset;
	return 0;
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

	pr_info("%s, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
	       __func__, var->xres, var->yres, var->xres_virtual,
	       var->yres_virtual, var->xoffset, var->yoffset,
	       var->bits_per_pixel);

	bpp = var->bits_per_pixel;

	if (bpp != 16 && bpp != 24 && bpp != 32) {
		pr_info("[%s]unsupported bpp: %d", __func__, bpp);
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
	pr_info("fbdev->fb_size_in_byte=0x%08lx\n", fbdev->fb_size_in_byte);
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
	pr_info("%s, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
	       __func__, var->xres, var->yres, var->xres_virtual,
	       var->yres_virtual, var->xoffset, var->yoffset,
	       var->bits_per_pixel);
	if (var->xres + var->xoffset > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yres + var->yoffset > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	pr_info("%s, xres=%u, yres=%u, xres_virtual=%u, yres_virtual=%u, xoffset=%u, yoffset=%u, bits_per_pixel=%u)\n",
	       __func__, var->xres, var->yres, var->xres_virtual,
	       var->yres_virtual, var->xoffset, var->yoffset,
	       var->bits_per_pixel);

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

		/* Check if format is RGB565 or BGR565 */

		if (!(var->green.offset == 8)) {
			pr_err("%s: green.offset=%d\n", __func__,
				var->green.offset);
			return -1;
		}
		if (!(var->red.offset + var->blue.offset == 16)) {
			pr_err("%s: red.offset=%d, blue.offset=%d\n", __func__,
			       var->green.offset, var->blue.offset);
			return -1;
		}
		if (!(var->red.offset == 16 || var->red.offset == 0)) {
			pr_err("%s: red.offset=%d\n", __func__,
			       var->green.offset);
			return -1;
		}
	} else if (bpp == 32) {
		var->red.length = var->green.length =
			var->blue.length = var->transp.length = 8;

		/* Check if format is ARGB565 or ABGR565 */

		if (!(var->green.offset == 8 && var->transp.offset == 24)) {
			pr_err("%s: green.offset=%d, transp.offset=%d\n",
			       __func__, var->green.offset, var->transp.offset);
			return -1;
		}
		if (!(var->red.offset + var->blue.offset == 16)) {
			pr_err("%s: red.offset=%d, blue.offset=%d\n",
			       __func__, var->red.offset, var->blue.offset);
			return -1;
		}
		if (!(var->red.offset == 16 || var->red.offset == 0)) {
			pr_err("%s: red.offset=%d\n",
			       __func__, var->red.offset);
			return -1;
		}
	}

	var->red.msb_right = var->green.msb_right =
		var->blue.msb_right = var->transp.msb_right = 0;

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

	return 0;
}


static int mtkfb_pan_display_proxy(struct fb_var_screeninfo *var,
	struct fb_info *info)
{
	return mtkfb_pan_display_impl(var, info);
}

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
	.fb_cursor = NULL,
	.fb_check_var = mtkfb_check_var,
	.fb_set_par = mtkfb_set_par,
	.fb_ioctl = NULL,
};


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

	WARN_ON(!fbdev->fb_va_base);
	info->fbops = &mtkfb_ops;
	info->flags = FBINFO_FLAG_DEFAULT;
	info->screen_base = (char *)fbdev->fb_va_base;
	info->screen_size = fbdev->fb_size_in_byte;
	info->pseudo_palette = fbdev->pseudo_palette;

	r = fb_alloc_cmap(&info->cmap, 32, 0);
	if (r != 0)
		pr_info("unable to allocate color map memory\n");

	/* setup the initial video mode (RGB565) */

	memset(&var, 0, sizeof(var));

	var.xres = MTK_FB_XRES;
	var.yres = MTK_FB_YRES;
	var.xres_virtual = MTK_FB_XRESV;
	var.yres_virtual = MTK_FB_YRESV;
	/* use 32 bit framebuffer as default */
	var.bits_per_pixel = 32;

	var.transp.offset = 24;
	var.red.length = 8;
	var.red.offset = 0;
	var.red.length = 8;
	var.green.offset = 8;
	var.green.length = 8;
	var.blue.offset = 16;
	var.blue.length = 8;

	var.width = 0;
	var.height = 0;

	var.activate = FB_ACTIVATE_NOW;

	r = mtkfb_check_var(&var, info);
	if (r != 0)
		pr_info("failed to mtkfb_check_var\n");

	info->var = var;

	r = mtkfb_set_par(info);
	if (r != 0)
		pr_info("failed to mtkfb_set_par\n");
	return r;
}

/* Release the fb_info object */
static void mtkfb_fbinfo_cleanup(struct mtkfb_device *fbdev)
{
	fb_dealloc_cmap(&fbdev->fb_info->cmap);
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
		WARN_ON(r == 0);
		/* lint -fallthrough */
	case 4:
		mtkfb_fbinfo_cleanup(fbdev);
		/* lint -fallthrough */
	case 2:
#ifndef FPGA_EARLY_PORTING
		dma_free_coherent(0, fbdev->fb_size_in_byte,
			fbdev->fb_va_base, fbdev->fb_pa_base);
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
		return;
	}
}

#ifdef CONFIG_OF
struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];	/* this is the minimum size */
};
unsigned int islcmconnected;
unsigned int is_lcm_inited;
unsigned int vramsize;
phys_addr_t fb_base;
static int is_videofb_parse_done;

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

	prop = (void *)of_get_property(node,
		"atag,videolfb-islcm_inited", NULL);
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

	prop = (void *)of_get_property(node,
		"atag,videolfb-lcmname", (int *)&size);
	if (!prop)
		return -1;
	if (size >= sizeof(mtkfb_lcm_name)) {
		pr_info("%s: error to get lcmname size=%ld\n", __func__, size);
		return -1;
	}
	memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
	strncpy((char *)mtkfb_lcm_name, prop, sizeof(mtkfb_lcm_name));
	mtkfb_lcm_name[size] = '\0';
	pr_info("%s done\n", __func__);
	return 0;
}

int __parse_tag_videolfb(struct device_node *node)
{
	struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;

	videolfb_tag =
		(struct tag_videolfb *)of_get_property(node,
			"atag,videolfb", (int *)&size);
	if (videolfb_tag) {
		memset((void *)mtkfb_lcm_name, 0, sizeof(mtkfb_lcm_name));
		strcpy((char *)mtkfb_lcm_name, videolfb_tag->lcmname);
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

	pr_info("[DT][videolfb] videolfb_tag not found\n");
	return -1;
}


static int _parse_tag_videolfb(void)
{
	int ret;
	struct device_node *chosen_node;

	pr_info("[DT][videolfb]isvideofb_parse_done = %d\n",
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
		pr_info("[DT][videolfb] of_chosen not found\n");
	}
	return -1;

found:
	is_videofb_parse_done = 1;
	pr_info("[DT][videolfb] islcmfound = %d\n", islcmconnected);
	pr_info("[DT][videolfb] is_lcm_inited = %d\n", is_lcm_inited);
	pr_info("[DT][videolfb] fps        = %d\n", lcd_fps);
	pr_info("[DT][videolfb] fb_base    = 0x%lx\n", (unsigned long)fb_base);
	pr_info("[DT][videolfb] vram       = 0x%x (%d)\n", vramsize, vramsize);
	pr_info("[DT][videolfb] lcmname    = %s\n", mtkfb_lcm_name);
	return 0;
}

phys_addr_t mtkfb_get_fb_base(void)
{
	_parse_tag_videolfb();
	return fb_base;
}
#endif


int mtkfb_allocate_framebuffer(phys_addr_t pa_start, phys_addr_t pa_end,
				unsigned long *va, unsigned long *mva)
{
	*va = (unsigned long)ioremap_nocache(pa_start, pa_end - pa_start + 1);
	pr_info("disphal_allocate_fb, pa=%pa, va=0x%08lx\n", &pa_start, *va);
	{
		*mva = pa_start & 0xffffffffULL;
	}
	return 0;
}

static int mtkfb_probe(struct platform_device *pdev)
{
	struct mtkfb_device *fbdev = NULL;
	struct fb_info *fbi;
	int init_state;
	int r = 0;

	pr_info("%s begin\n", __func__);

#ifdef CONFIG_OF
	_parse_tag_videolfb();
#endif
	init_state = 0;

	fbi = framebuffer_alloc(sizeof(struct mtkfb_device), &(pdev->dev));
	if (!fbi) {
		pr_info("unable to allocate memory for device info\n");
		r = -ENOMEM;
		goto cleanup;
	}

	fbdev = (struct mtkfb_device *)fbi->par;
	fbdev->fb_info = fbi;
	fbdev->dev = &(pdev->dev);
	dev_set_drvdata(&(pdev->dev), fbdev);

	{

#ifdef CONFIG_OF
		pr_info("%s:get FB MEM REG\n", __func__);
		_parse_tag_videolfb();
		pr_info("%s: fb_pa = 0x%p\n", __func__, (void *)fb_base);

		mtkfb_allocate_framebuffer(fb_base, (fb_base + vramsize - 1),
			(unsigned long *)&fbdev->fb_va_base, &fb_pa);
		fbdev->fb_pa_base = (dma_addr_t) fb_base;
#else
		struct resource *res =
			platform_get_resource(pdev, IORESOURCE_MEM, 0);

		disp_hal_allocate_framebuffer(res->start, res->end,
			(unsigned long *)&fbdev->fb_va_base, &fb_pa);
		fbdev->fb_pa_base = res->start;
#endif
	}

	init_state++;		/* 1 */
	MTK_FB_XRES = SCREEN_WIDHT;
	MTK_FB_YRES = SCREEN_HEIGHT;
	fb_xres_update = MTK_FB_XRES;
	fb_yres_update = MTK_FB_YRES;

	MTK_FB_BPP = 32;
	MTK_FB_PAGES = 3;
	pr_info("MTK_FB_XRES=%d, MTKFB_YRES=%d, MTKFB_BPP=%d, MTK_FB_PAGES=%d, MTKFB_LINE=%d, MTKFB_SIZEV=%d\n",
	     MTK_FB_XRES, MTK_FB_YRES, MTK_FB_BPP,
	     MTK_FB_PAGES, MTK_FB_LINE, MTK_FB_SIZEV);
	fbdev->fb_size_in_byte = MTK_FB_SIZEV;

	/* Allocate and initialize video frame buffer */
	pr_info("[FB Driver] fbdev->fb_pa_base = %p, fbdev->fb_va_base = %lx\n",
	       (void *)fbdev->fb_pa_base, (unsigned long)(fbdev->fb_va_base));

	if (!fbdev->fb_va_base) {
		pr_info("unable to allocate memory for frame buffer\n");
		r = -ENOMEM;
		goto cleanup;
	}
	init_state++;		/* 2 */

	/* Register to system */

	r = mtkfb_fbinfo_init(fbi);
	if (r) {
		pr_info("mtkfb_fbinfo_init fail, r = %d\n", r);
		goto cleanup;
	}
	init_state++;		/* 4 */
	mtkfb_fbi = fbi;
	init_state++;		/* 5 */

	r = register_framebuffer(fbi);
	if (r != 0) {
		pr_info("register_framebuffer failed\n");
		goto cleanup;
	}
	fbdev->state = MTKFB_ACTIVE;
	pr_info("%s end\n", __func__);
	return 0;

cleanup:
	mtkfb_free_resources(fbdev, init_state);

	pr_info("%s end\n", __func__);
	return r;
}


/* Called when the device is being detached from the driver */
static int mtkfb_remove(struct device *dev)
{
	struct mtkfb_device *fbdev = dev_get_drvdata(dev);
	enum mtkfb_state saved_state = fbdev->state;

	fbdev->state = MTKFB_DISABLED;
	mtkfb_free_resources(fbdev, saved_state);
	return 0;
}

int mtkfb_ipo_init(void)
{
	return 0;
}

void mtkfb_clear_lcm(void)
{
	;
}

static const struct of_device_id mtkfb_of_ids[] = {
	{ .compatible = "mediatek,mtkfb", },
	{}
};

static struct platform_driver mtkfb_driver = {
	.probe = mtkfb_probe,
	.driver = {
		.name = MTKFB_DRIVER,
#ifdef CONFIG_PM
		.pm = NULL,
#endif
		.bus = &platform_bus_type,
		.remove = mtkfb_remove,
		.suspend = NULL,
		.resume = NULL,
		.shutdown = NULL,
		.of_match_table = mtkfb_of_ids,
	},
};

/* Register both the driver and the device */
int __init mtkfb_init(void)
{
	int r = 0;

	pr_info("%s init", __func__);

	if (platform_driver_register(&mtkfb_driver)) {
		pr_info("failed to register mtkfb driver\n");
		r = -ENODEV;
	}
	return r;
}

static void __exit mtkfb_cleanup(void)
{
	platform_driver_unregister(&mtkfb_driver);
}


module_init(mtkfb_init);
module_exit(mtkfb_cleanup);

MODULE_DESCRIPTION("MEDIATEK framebuffer driver");
MODULE_AUTHOR("Xuecheng Zhang <Xuecheng.Zhang@mediatek.com>");
MODULE_LICENSE("GPL");

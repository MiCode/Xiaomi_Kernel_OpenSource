/*
 * drivers/video/tegra/fb.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *         Colin Cross <ccross@android.com>
 *         Travis Geiselbrecht <travis@palm.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fb.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/console.h>

#include <asm/atomic.h>

#include <video/tegrafb.h>

#include <mach/dc.h>
#include <mach/fb.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>

#include "host/dev.h"
#include "dc/dc_priv.h"

/* Pad pitch to 256-byte boundary. */
#define TEGRA_LINEAR_PITCH_ALIGNMENT 256

#ifdef CONFIG_COMPAT
#define user_ptr(p) ((void __user *)(__u64)(p))
#else
#define user_ptr(p) (p)
#endif

struct tegra_fb_info {
	struct tegra_dc_win	*win;
	struct platform_device	*ndev;
	struct fb_info		*info;
	bool			valid;

	struct resource		*fb_mem;

	int			xres;
	int			yres;
	int			curr_xoffset;
	int			curr_yoffset;

	struct fb_videomode	mode;
	phys_addr_t		phys_start;
};

/* palette array used by the fbcon */
static u32 pseudo_palette[16];

static int tegra_fb_check_var(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct tegra_fb_info *tegra_fb = info->par;
	struct tegra_dc *dc = tegra_fb->win->dc;
	struct tegra_dc_out_ops *ops = dc->out_ops;
	struct fb_videomode mode;

	if ((var->yres * var->xres * var->bits_per_pixel / 8 * 2) >
	    info->screen_size)
		return -EINVAL;

	/* Apply mode filter for HDMI only -LVDS supports only fix mode */
	if (ops && ops->mode_filter) {
		/* xoffset and yoffset are not preserved by conversion
		 * to fb_videomode */
		__u32 xoffset = var->xoffset;
		__u32 yoffset = var->yoffset;

		fb_var_to_videomode(&mode, var);
		if (!ops->mode_filter(dc, &mode))
			return -EINVAL;

		/* Mode filter may have modified the mode */
		fb_videomode_to_var(var, &mode);

		var->xoffset = xoffset;
		var->yoffset = yoffset;
	}

	/* Double yres_virtual to allow double buffering through pan_display */
	var->yres_virtual = var->yres * 2;

	return 0;
}

static int tegra_fb_set_par(struct fb_info *info)
{
	struct tegra_fb_info *tegra_fb = info->par;
	struct fb_var_screeninfo *var = &info->var;
	struct tegra_dc *dc = tegra_fb->win->dc;

	if (var->bits_per_pixel) {
		/* we only support RGB ordering for now */
		switch (var->bits_per_pixel) {
		case 32:
			var->red.offset = 0;
			var->red.length = 8;
			var->green.offset = 8;
			var->green.length = 8;
			var->blue.offset = 16;
			var->blue.length = 8;
			var->transp.offset = 24;
			var->transp.length = 8;
			tegra_fb->win->fmt = TEGRA_WIN_FMT_R8G8B8A8;
			break;
		case 16:
			var->red.offset = 11;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 0;
			var->blue.length = 5;
			tegra_fb->win->fmt = TEGRA_WIN_FMT_B5G6R5;
			break;

		default:
			return -EINVAL;
		}
		/* if line_length unset, then pad the stride */
		if (!info->fix.line_length) {
			info->fix.line_length = var->xres * var->bits_per_pixel
				/ 8;
			info->fix.line_length = round_up(info->fix.line_length,
						TEGRA_LINEAR_PITCH_ALIGNMENT);
		}
		tegra_fb->win->stride = info->fix.line_length;
		tegra_fb->win->stride_uv = 0;
		tegra_fb->win->phys_addr_u = 0;
		tegra_fb->win->phys_addr_v = 0;
	}

	if (var->pixclock) {
		bool stereo;
		unsigned old_len = 0;
		struct fb_videomode m;
		struct fb_videomode *old_mode = NULL;
		struct tegra_fb_info *tegra_fb = info->par;


		fb_var_to_videomode(&m, var);

		/* Load framebuffer info with new mode details*/
		old_mode = info->mode;
		old_len  = info->fix.line_length;
		memcpy(&tegra_fb->mode, &m, sizeof(tegra_fb->mode));
		info->mode = (struct fb_videomode *)&tegra_fb->mode;
		if (!info->mode) {
			dev_warn(&tegra_fb->ndev->dev, "can't match video mode\n");
			info->mode = old_mode;
			return -EINVAL;
		}

		/* Update fix line_length and window stride as per new mode */
		info->fix.line_length = var->xres * var->bits_per_pixel / 8;
		info->fix.line_length = round_up(info->fix.line_length,
			TEGRA_LINEAR_PITCH_ALIGNMENT);
		tegra_fb->win->stride = info->fix.line_length;

		/*
		 * only enable stereo if the mode supports it and
		 * client requests it
		 */
		stereo = !!(var->vmode & info->mode->vmode &
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
					FB_VMODE_STEREO_FRAME_PACK);
#else
					FB_VMODE_STEREO_LEFT_RIGHT);
#endif

		/* Configure DC with new mode */
		if (tegra_dc_set_fb_mode(dc, info->mode, stereo)) {
			/* Error while configuring DC, fallback to old mode */
			dev_warn(&tegra_fb->ndev->dev, "can't configure dc with mode %ux%u\n",
				info->mode->xres, info->mode->yres);
			info->mode = old_mode;
			info->fix.line_length = old_len;
			tegra_fb->win->stride = old_len;
			return -EINVAL;
		}

		tegra_fb->win->w.full = dfixed_const(info->mode->xres);
		tegra_fb->win->h.full = dfixed_const(info->mode->yres);
		tegra_fb->win->out_w = info->mode->xres;
		tegra_fb->win->out_h = info->mode->yres;
	}
	return 0;
}

static int tegra_fb_setcolreg(unsigned regno, unsigned red, unsigned green,
	unsigned blue, unsigned transp, struct fb_info *info)
{
	struct fb_var_screeninfo *var = &info->var;

	if (info->fix.visual == FB_VISUAL_TRUECOLOR ||
	    info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		u32 v;

		if (regno >= 16)
			return -EINVAL;

		red = (red >> (16 - info->var.red.length));
		green = (green >> (16 - info->var.green.length));
		blue = (blue >> (16 - info->var.blue.length));

		v = (red << var->red.offset) |
			(green << var->green.offset) |
			(blue << var->blue.offset);

		((u32 *)info->pseudo_palette)[regno] = v;
	}

	return 0;
}


static int tegra_fb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	struct tegra_fb_info *tegra_fb = info->par;
	struct tegra_dc *dc = tegra_fb->win->dc;
	int i;
	u16 *red = cmap->red;
	u16 *green = cmap->green;
	u16 *blue = cmap->blue;
	int start = cmap->start;

	if (((unsigned)start > 255) || ((start + cmap->len) > 256))
		return -EINVAL;

	if (info->fix.visual == FB_VISUAL_TRUECOLOR ||
		info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		/*
		 * For now we are considering color schemes with
		 * cmap->len <=16 as special case of basic color
		 * scheme to support fbconsole.But for DirectColor
		 * visuals(like the one we actually have, that include
		 * a HW LUT),the way it's intended to work is that the
		 * actual LUT HW is programmed to the intended values,
		 * even for small color maps like those with 16 or fewer
		 * entries. The pseudo_palette is then programmed to the
		 * identity transform.
		 */
		if (cmap->len <= 16) {
			/* Low-color schemes like fbconsole*/
			u16 *transp = cmap->transp;
			u_int vtransp = 0xffff;

			for (i = 0; i < cmap->len; i++) {
				if (transp)
					vtransp = *transp++;
				if (tegra_fb_setcolreg(start++, *red++,
					*green++, *blue++,
					vtransp, info))
						return -EINVAL;
			}
		} else {
			/* High-color schemes*/
			for (i = 0; i < cmap->len; i++) {
				dc->fb_lut.r[start+i] = *red++ >> 8;
				dc->fb_lut.g[start+i] = *green++ >> 8;
				dc->fb_lut.b[start+i] = *blue++ >> 8;
			}
			tegra_dc_update_lut(dc, -1, -1);
		}
	}
	return 0;
}

static int tegra_fb_blank(int blank, struct fb_info *info)
{
	struct tegra_fb_info *tegra_fb = info->par;
	struct tegra_dc *dc = tegra_fb->win->dc;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		dev_dbg(&tegra_fb->ndev->dev, "unblank\n");
		if (tegra_fb->win->dc->enabled &&
			(tegra_fb->win->flags & TEGRA_WIN_FLAG_ENABLED))
			return 0;
		tegra_fb->win->flags |= TEGRA_WIN_FLAG_ENABLED;
		if (tegra_fb->win->dc->win_blank_saved_flag > 0) {
			*(tegra_fb->win) = tegra_fb->win->dc->win_blank_saved;
			tegra_fb->win->dc->win_blank_saved_flag = 0;
		}
		tegra_dc_enable(tegra_fb->win->dc);
		tegra_dc_update_windows(&tegra_fb->win, 1);
		tegra_dc_sync_windows(&tegra_fb->win, 1);
        tegra_dc_program_bandwidth(dc, true);
		return 0;

	case FB_BLANK_NORMAL:
		dev_dbg(&tegra_fb->ndev->dev, "blank - normal\n");
		/* To pan fb at the unblank */
		if (tegra_fb->win->dc->enabled)
			tegra_fb->curr_xoffset = -1;
		tegra_dc_blank(tegra_fb->win->dc);
		return 0;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		dev_dbg(&tegra_fb->ndev->dev, "blank - powerdown\n");
		/* To pan fb while switching from X */
		if (!tegra_fb->win->dc->suspended && tegra_fb->win->dc->enabled)
			tegra_fb->curr_xoffset = -1;
		tegra_fb->win->dc->win_blank_saved = *(tegra_fb->win);
		tegra_fb->win->dc->win_blank_saved_flag = 1;
		tegra_dc_disable(tegra_fb->win->dc);
		return 0;

	default:
		return -ENOTTY;
	}
}

static int tegra_fb_pan_display(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	struct tegra_fb_info *tegra_fb = info->par;
	char __iomem *flush_start;
	char __iomem *flush_end;
	struct tegra_dc *dc = tegra_fb->win->dc;
	phys_addr_t    addr;

	/*
	 * Do nothing if display parameters are same as current values.
	 */
#if defined(CONFIG_ANDROID)
	if ((var->xoffset == tegra_fb->curr_xoffset) &&
	    (var->yoffset == tegra_fb->curr_yoffset) &&
	    !(var->activate & FB_ACTIVATE_FORCE))
		return 0;
#endif

	if (!tegra_fb->win->cur_handle) {
		flush_start = info->screen_base +
		(var->yoffset * info->fix.line_length);
		flush_end = flush_start + (var->yres * info->fix.line_length);

		info->var.xoffset = var->xoffset;
		info->var.yoffset = var->yoffset;
		/*
		 * Save previous values of xoffset and yoffset so we can
		 * pan display only when needed.
		 */
		tegra_fb->curr_xoffset = var->xoffset;
		tegra_fb->curr_yoffset = var->yoffset;

		addr = tegra_fb->phys_start + (var->yoffset * info->fix.line_length) +
			(var->xoffset * (var->bits_per_pixel/8));

		tegra_fb->win->phys_addr = addr;
		tegra_fb->win->flags = TEGRA_WIN_FLAG_ENABLED;
		tegra_fb->win->flags |= TEGRA_WIN_FLAG_FB;
		tegra_fb->win->virt_addr = info->screen_base;

		tegra_dc_update_windows(&tegra_fb->win, 1);
		tegra_dc_sync_windows(&tegra_fb->win, 1);
        tegra_dc_program_bandwidth(dc, true);
	}

	return 0;
}

static void tegra_fb_fillrect(struct fb_info *info,
			      const struct fb_fillrect *rect)
{
	cfb_fillrect(info, rect);
}

static void tegra_fb_copyarea(struct fb_info *info,
			      const struct fb_copyarea *region)
{
	cfb_copyarea(info, region);
}

static void tegra_fb_imageblit(struct fb_info *info,
			       const struct fb_image *image)
{
	cfb_imageblit(info, image);
}

static int tegra_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct tegra_fb_info *tegra_fb = (struct tegra_fb_info *)info->par;
	struct tegra_dc *dc = tegra_fb->win->dc;
	struct tegra_fb_modedb modedb;
	struct fb_modelist *modelist;
	struct fb_vblank vblank = {};
	struct fb_var_screeninfo *modedb_ptr;
	unsigned i;

	switch (cmd) {
	case FBIO_TEGRA_GET_MODEDB:
		if (copy_from_user(&modedb, (void __user *)arg, sizeof(modedb)))
			return -EFAULT;

		i = 0;
		modedb_ptr = user_ptr(modedb.modedb);
		list_for_each_entry(modelist, &info->modelist, list) {
			struct fb_var_screeninfo var;

			/* fb_videomode_to_var doesn't fill out all the members
			   of fb_var_screeninfo */
			memset(&var, 0x0, sizeof(var));

			fb_videomode_to_var(&var, &modelist->mode);
			var.width = tegra_dc_get_out_width(dc);
			var.height = tegra_dc_get_out_height(dc);

			if (i < modedb.modedb_len) {
				void __user *ptr = &modedb_ptr[i];
				if (copy_to_user(ptr, &var, sizeof(var)))
					return -EFAULT;
			}
			i++;

			if (var.vmode & FB_VMODE_STEREO_MASK) {
				if (i < modedb.modedb_len) {
					void __user *ptr = &modedb_ptr[i];
					var.vmode &= ~FB_VMODE_STEREO_MASK;
					if (copy_to_user(ptr,
						&var, sizeof(var)))
						return -EFAULT;
				}
				i++;
			}
		}

		/*
		 * If modedb_len == 0, return how many modes are
		 * available; otherwise, return how many modes were written.
		 */
		if (modedb.modedb_len == 0)
			modedb.modedb_len = i;
		else
			modedb.modedb_len = min(modedb.modedb_len, i);

		if (copy_to_user((void __user *)arg, &modedb, sizeof(modedb)))
			return -EFAULT;
		break;

	case FBIOGET_VBLANK:
		tegra_dc_get_fbvblank(tegra_fb->win->dc, &vblank);

		if (copy_to_user(
			(void __user *)arg, &vblank, sizeof(vblank)))
			return -EFAULT;
		break;

	case FBIO_WAITFORVSYNC:
		return tegra_dc_wait_for_vsync(tegra_fb->win->dc);

	default:
		return -ENOTTY;
	}

	return 0;
}

int tegra_fb_get_mode(struct tegra_dc *dc) {
	if (!dc->fb->info->mode)
		return -1;
	return dc->fb->info->mode->refresh;
}

int tegra_fb_set_mode(struct tegra_dc *dc, int fps) {
	size_t stereo;
	struct list_head *pos;
	struct fb_videomode *best_mode = NULL;
	int curr_diff = INT_MAX; /* difference of best_mode refresh rate */
	struct fb_modelist *modelist;
	struct fb_info *info = dc->fb->info;

	list_for_each(pos, &info->modelist) {
		struct fb_videomode *mode;

		modelist = list_entry(pos, struct fb_modelist, list);
		mode = &modelist->mode;
		if (fps <= mode->refresh && curr_diff > (mode->refresh - fps)) {
			curr_diff = mode->refresh - fps;
			best_mode = mode;
		}
	}
	if (best_mode) {
		info->mode = best_mode;
		stereo = !!(info->var.vmode & info->mode->vmode &
#ifndef CONFIG_TEGRA_HDMI_74MHZ_LIMIT
				FB_VMODE_STEREO_FRAME_PACK);
#else
				FB_VMODE_STEREO_LEFT_RIGHT);
#endif
		return tegra_dc_set_fb_mode(dc, best_mode, stereo);
	}
	return -EIO;
}

static struct fb_ops tegra_fb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = tegra_fb_check_var,
	.fb_set_par = tegra_fb_set_par,
	.fb_setcmap = tegra_fb_setcmap,
	.fb_blank = tegra_fb_blank,
	.fb_pan_display = tegra_fb_pan_display,
	.fb_fillrect = tegra_fb_fillrect,
	.fb_copyarea = tegra_fb_copyarea,
	.fb_imageblit = tegra_fb_imageblit,
	.fb_ioctl = tegra_fb_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl = tegra_fb_ioctl,
#endif
};

/* Enabling the pan_display by resetting the cache of offset */
void tegra_fb_pan_display_reset(struct tegra_fb_info *fb_info)
{
	fb_info->curr_xoffset = -1;
}

void tegra_fb_update_monspecs(struct tegra_fb_info *fb_info,
			      struct fb_monspecs *specs,
			      bool (*mode_filter)(const struct tegra_dc *dc,
						  struct fb_videomode *mode))
{
	struct fb_event event;
	int i;

	mutex_lock(&fb_info->info->lock);
	fb_destroy_modedb(fb_info->info->monspecs.modedb);

	fb_destroy_modelist(&fb_info->info->modelist);

	if (specs == NULL) {
		struct tegra_dc_mode mode;
		memset(&fb_info->info->monspecs, 0x0,
		       sizeof(fb_info->info->monspecs));
		memset(&mode, 0x0, sizeof(mode));

		/*
		 * reset video mode properties to prevent garbage being displayed on 'mode' device.
		 */
		fb_info->info->mode = (struct fb_videomode*) NULL;

		tegra_dc_set_mode(fb_info->win->dc, &mode);
		mutex_unlock(&fb_info->info->lock);
		return;
	}

	memcpy(&fb_info->info->monspecs, specs,
	       sizeof(fb_info->info->monspecs));
	fb_info->info->mode = specs->modedb;

	for (i = 0; i < specs->modedb_len; i++) {
		if (mode_filter) {
			if (mode_filter(fb_info->win->dc, &specs->modedb[i]))
				fb_add_videomode(&specs->modedb[i],
						 &fb_info->info->modelist);
		} else {
			fb_add_videomode(&specs->modedb[i],
					 &fb_info->info->modelist);
		}
	}

	event.info = fb_info->info;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
	console_lock();
	fb_notifier_call_chain(FB_EVENT_NEW_MODELIST, &event);
	console_unlock();
#else
	fb_notifier_call_chain(FB_EVENT_NEW_MODELIST, &event);
#endif
	mutex_unlock(&fb_info->info->lock);
}

struct tegra_fb_info *tegra_fb_register(struct platform_device *ndev,
					struct tegra_dc *dc,
					struct tegra_fb_data *fb_data,
					struct resource *fb_mem)
{
	struct tegra_dc_win *win;
	struct fb_info *info;
	struct tegra_fb_info *tegra_fb;
	void __iomem *fb_base = NULL;
	phys_addr_t fb_size = 0;
	int ret = 0;
	int mode_idx;
	unsigned stride;
	struct fb_videomode m;

	win = tegra_dc_get_window(dc, fb_data->win);
	if (!win) {
		dev_err(&ndev->dev, "dc does not have a window at index %d\n",
			fb_data->win);
		return ERR_PTR(-ENOENT);
	}

	info = framebuffer_alloc(sizeof(struct tegra_fb_info), &ndev->dev);
	if (!info) {
		ret = -ENOMEM;
		goto err;
	}

	tegra_fb = info->par;
	tegra_fb->win = win;
	tegra_fb->ndev = ndev;
	tegra_fb->fb_mem = fb_mem;
	tegra_fb->xres = fb_data->xres;
	tegra_fb->yres = fb_data->yres;

	if (fb_mem) {
		fb_size = resource_size(fb_mem);
		tegra_fb->phys_start = fb_mem->start;
		fb_base = ioremap_nocache(tegra_fb->phys_start, fb_size);
		if (!fb_base) {
			dev_err(&ndev->dev, "fb can't be mapped\n");
			ret = -EBUSY;
			goto err_free;
		}
		tegra_fb->valid = true;
	}

	info->fix.line_length = fb_data->xres * fb_data->bits_per_pixel / 8;

	stride = tegra_dc_get_stride(dc, 0);
	if (!stride) /* default to pad the stride */
		stride = round_up(info->fix.line_length,
			TEGRA_LINEAR_PITCH_ALIGNMENT);

	info->fbops = &tegra_fb_ops;
	info->pseudo_palette = pseudo_palette;
	info->screen_base = fb_base;
	info->screen_size = fb_size;

	strlcpy(info->fix.id, "tegra_fb", sizeof(info->fix.id));
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.visual	= FB_VISUAL_TRUECOLOR;
	info->fix.xpanstep	= 1;
	info->fix.ypanstep	= 1;
	info->fix.accel		= FB_ACCEL_NONE;
	/* Note:- Use tegra_fb_info.phys_start instead of
	 *        fb_info.fix->smem_start when LPAE is enabled. */
	info->fix.smem_start	= (u32)tegra_fb->phys_start;
	info->fix.smem_len	= fb_size;
	info->fix.line_length = stride;
	INIT_LIST_HEAD(&info->modelist);
	/* pick first mode as the default for initialization */
	tegra_dc_to_fb_videomode(&m, &dc->mode);
	fb_videomode_to_var(&info->var, &m);
	info->var.xres_virtual		= fb_data->xres;
	info->var.yres_virtual		= fb_data->yres * 2;
	info->var.bits_per_pixel	= fb_data->bits_per_pixel;
	info->var.activate		= FB_ACTIVATE_VBL;
	info->var.height		= tegra_dc_get_out_height(dc);
	info->var.width			= tegra_dc_get_out_width(dc);

	win->x.full = dfixed_const(0);
	win->y.full = dfixed_const(0);
	win->w.full = dfixed_const(fb_data->xres);
	win->h.full = dfixed_const(fb_data->yres);
	/* TODO: set to output res dc */
	win->out_x = 0;
	win->out_y = 0;
	win->out_w = fb_data->xres;
	win->out_h = fb_data->yres;
	win->z = 0;
	win->phys_addr = tegra_fb->phys_start;
	win->virt_addr = fb_base;
	win->phys_addr_u = 0;
	win->phys_addr_v = 0;
	win->stride = info->fix.line_length;
	win->stride_uv = 0;
	win->flags = TEGRA_WIN_FLAG_ENABLED;
	win->global_alpha = 0xFF;

	if (fb_mem)
		tegra_fb_set_par(info);

	if (register_framebuffer(info)) {
		dev_err(&ndev->dev, "failed to register framebuffer\n");
		ret = -ENODEV;
		goto err_iounmap_fb;
	}

	tegra_fb->info = info;

	dev_info(&ndev->dev, "probed\n");

	if (fb_data->flags & TEGRA_FB_FLIP_ON_PROBE) {
		tegra_dc_update_windows(&tegra_fb->win, 1);
		tegra_dc_sync_windows(&tegra_fb->win, 1);
        if (dc->enabled)
            tegra_dc_program_bandwidth(dc, true);
	}

	for (mode_idx = 1; mode_idx < dc->out->n_modes; mode_idx++) {
		struct tegra_dc_mode mode = dc->out->modes[mode_idx];
		struct fb_videomode vmode;

		mode.pclk = dc->mode.pclk;

		if (mode.pclk > 1000) {
			tegra_dc_to_fb_videomode(&vmode, &mode);
			fb_add_videomode(&vmode, &info->modelist);
		}
	}

	return tegra_fb;

err_iounmap_fb:
	if (fb_base)
		iounmap(fb_base);
err_free:
	framebuffer_release(info);
err:
	return ERR_PTR(ret);
}

void tegra_fb_unregister(struct tegra_fb_info *fb_info)
{
	struct fb_info *info = fb_info->info;

	unregister_framebuffer(info);

	iounmap(info->screen_base);
	framebuffer_release(info);
}

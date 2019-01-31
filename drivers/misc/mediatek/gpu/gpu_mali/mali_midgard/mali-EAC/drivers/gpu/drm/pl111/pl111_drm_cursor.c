/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_cursor.c
 * Implementation of cursor functions for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include "pl111_clcd_ext.h"
#include "pl111_drm.h"

#define PL111_MAX_CURSOR_WIDTH (64)
#define PL111_MAX_CURSOR_HEIGHT (64)

#define ARGB_2_LBBP_BINARY_THRESHOLD	(1 << 7)
#define ARGB_ALPHA_SHIFT		24
#define ARGB_ALPHA_MASK			(0xff << ARGB_ALPHA_SHIFT)
#define ARGB_RED_SHIFT			16
#define ARGB_RED_MASK			(0xff << ARGB_RED_SHIFT)
#define ARGB_GREEN_SHIFT		8
#define ARGB_GREEN_MASK			(0xff << ARGB_GREEN_SHIFT)
#define ARGB_BLUE_SHIFT			0
#define ARGB_BLUE_MASK			(0xff << ARGB_BLUE_SHIFT)


void pl111_set_cursor_size(enum pl111_cursor_size size)
{
	u32 reg_data = readl(priv.regs + CLCD_CRSR_CONFIG);

	if (size == CURSOR_64X64)
		reg_data |= CRSR_CONFIG_CRSR_SIZE;
	else
		reg_data &= ~CRSR_CONFIG_CRSR_SIZE;

	writel(reg_data, priv.regs + CLCD_CRSR_CONFIG);
}

void pl111_set_cursor_sync(enum pl111_cursor_sync sync)
{
	u32 reg_data = readl(priv.regs + CLCD_CRSR_CONFIG);

	if (sync == CURSOR_SYNC_VSYNC)
		reg_data |= CRSR_CONFIG_CRSR_FRAME_SYNC;
	else
		reg_data &= ~CRSR_CONFIG_CRSR_FRAME_SYNC;

	writel(reg_data, priv.regs + CLCD_CRSR_CONFIG);
}

void pl111_set_cursor(u32 cursor)
{
	u32 reg_data = readl(priv.regs + CLCD_CRSR_CTRL);

	reg_data &= ~(CRSR_CTRL_CRSR_MAX << CRSR_CTRL_CRSR_NUM_SHIFT);
	reg_data |= (cursor & CRSR_CTRL_CRSR_MAX) << CRSR_CTRL_CRSR_NUM_SHIFT;

	writel(reg_data, priv.regs + CLCD_CRSR_CTRL);
}

void pl111_set_cursor_enable(bool enable)
{
	u32 reg_data = readl(priv.regs + CLCD_CRSR_CTRL);

	if (enable)
		reg_data |= CRSR_CTRL_CRSR_ON;
	else
		reg_data &= ~CRSR_CTRL_CRSR_ON;

	writel(reg_data, priv.regs + CLCD_CRSR_CTRL);
}

void pl111_set_cursor_position(u32 x, u32 y)
{
	u32 reg_data = (x & CRSR_XY_MASK) |
			((y & CRSR_XY_MASK) << CRSR_XY_Y_SHIFT);

	writel(reg_data, priv.regs + CLCD_CRSR_XY);
}

void pl111_set_cursor_clipping(u32 x, u32 y)
{
	u32 reg_data;

	/* 
	 * Do not allow setting clipping values larger than
	 * the cursor size since the cursor is already fully hidden
	 * when x,y = PL111_MAX_CURSOR_WIDTH.
	 */
	if (x > PL111_MAX_CURSOR_WIDTH)
		x = PL111_MAX_CURSOR_WIDTH;
	if (y > PL111_MAX_CURSOR_WIDTH)
		y = PL111_MAX_CURSOR_WIDTH;

	reg_data = (x & CRSR_CLIP_MASK) |
			((y & CRSR_CLIP_MASK) << CRSR_CLIP_Y_SHIFT);

	writel(reg_data, priv.regs + CLCD_CRSR_CLIP);
}

void pl111_set_cursor_palette(u32 color0, u32 color1)
{
	writel(color0 & CRSR_PALETTE_MASK, priv.regs + CLCD_CRSR_PALETTE_0);
	writel(color1 & CRSR_PALETTE_MASK, priv.regs + CLCD_CRSR_PALETTE_1);
}

void pl111_cursor_enable(void)
{
	pl111_set_cursor_sync(CURSOR_SYNC_VSYNC);
	pl111_set_cursor_size(CURSOR_64X64);
	pl111_set_cursor_palette(0x0, 0x00ffffff);
	pl111_set_cursor_enable(true);
}

void pl111_cursor_disable(void)
{
	pl111_set_cursor_enable(false);
}

/* shift required to locate pixel into the correct position in
 * a cursor LBBP word, indexed by x mod 16.
 */
static const unsigned char
x_mod_16_to_value_shift[CLCD_CRSR_IMAGE_PIXELS_PER_WORD] = {
	6, 4, 2, 0, 14, 12, 10, 8, 22, 20, 18, 16, 30, 28, 26, 24
};

/* Pack the pixel value into its correct position in the buffer as specified
 * for LBBP */
static inline void
set_lbbp_pixel(uint32_t *buffer, unsigned int x, unsigned int y,
				uint32_t value)
{
	u32 *cursor_ram = priv.regs + CLCD_CRSR_IMAGE;
	uint32_t shift;
	uint32_t data;

	shift = x_mod_16_to_value_shift[x % CLCD_CRSR_IMAGE_PIXELS_PER_WORD];

	/* Get the word containing this pixel */
	cursor_ram = cursor_ram + (x >> CLCD_CRSR_IMAGE_WORDS_PER_LINE) + (y << 2);

	/* Update pixel in cursor RAM */
	data = readl(cursor_ram);
	data &= ~(CLCD_CRSR_LBBP_COLOR_MASK << shift);
	data |= value << shift;
	writel(data, cursor_ram);
}

static u32 pl111_argb_to_lbbp(u32 argb_pix)
{
	u32 lbbp_pix = CLCD_CRSR_LBBP_TRANSPARENT;
	u32 alpha = (argb_pix & ARGB_ALPHA_MASK) >> ARGB_ALPHA_SHIFT;
	u32 red = (argb_pix & ARGB_RED_MASK) >> ARGB_RED_SHIFT;
	u32 green = (argb_pix & ARGB_GREEN_MASK) >> ARGB_GREEN_SHIFT;
	u32 blue =  (argb_pix & ARGB_BLUE_MASK) >> ARGB_BLUE_SHIFT;

	/*
	 * Converting from 8 pixel transparency to binary transparency
	 * it's the best we can achieve.
	 */
	if (alpha & ARGB_2_LBBP_BINARY_THRESHOLD) {
		u32 gray, max, min;

		/*
		 * Convert to gray using the lightness method:
		 * gray = [max(R,G,B) + min(R,G,B)]/2
		 */
		min = min(red, green);
		min = min(min, blue);
		max = max(red, green);
		max = max(max, blue);
		gray = (min + max) >> 1; /* divide by 2 */
		/* Apply binary threshold to the gray value calculated */
		if (gray & ARGB_2_LBBP_BINARY_THRESHOLD)
			lbbp_pix = CLCD_CRSR_LBBP_FOREGROUND;
		else
			lbbp_pix = CLCD_CRSR_LBBP_BACKGROUND;
	}

	return lbbp_pix;
}

/*
 * The PL111 hardware cursor supports only LBBP which is a 2bpp format but
 * the cursor format from userspace is ARGB8888 so we need to convert
 *  to LBBP here.
 */
static void pl111_set_cursor_image(u32 *data)
{
#ifdef ARGB_LBBP_CONVERSION_DEBUG
	/* Add 1 on width to insert trailing NULL */
	char string_cursor[PL111_MAX_CURSOR_WIDTH + 1];
#endif /* ARGB_LBBP_CONVERSION_DEBUG */
	unsigned int x;
	unsigned int y;

	for (y = 0; y < PL111_MAX_CURSOR_HEIGHT; y++) {
		for (x = 0; x < PL111_MAX_CURSOR_WIDTH; x++) {
			u32 value = pl111_argb_to_lbbp(*data);

#ifdef ARGB_LBBP_CONVERSION_DEBUG
			if (value == CLCD_CRSR_LBBP_TRANSPARENT)
				string_cursor[x] = 'T';
			else if (value == CLCD_CRSR_LBBP_FOREGROUND)
				string_cursor[x] = 'F';
			else if (value == CLCD_CRSR_LBBP_INVERSE)
				string_cursor[x] = 'I';
			else
				string_cursor[x] = 'B';

#endif /* ARGB_LBBP_CONVERSION_DEBUG */
			set_lbbp_pixel(data, x, y, value);
			++data;
		}
#ifdef ARGB_LBBP_CONVERSION_DEBUG
		string_cursor[PL111_MAX_CURSOR_WIDTH] = '\0';
		DRM_INFO("%s\n", string_cursor);
#endif /* ARGB_LBBP_CONVERSION_DEBUG */
	}
}

int pl111_crtc_cursor_set(struct drm_crtc *crtc,
			   struct drm_file *file_priv,
			   uint32_t handle,
			   uint32_t width,
			   uint32_t height)
{
	struct drm_gem_object *obj;
	struct pl111_gem_bo *bo;

	DRM_DEBUG_KMS("handle = %u, width = %u, height = %u\n",
		handle, width, height);

	if (!handle) {
		pl111_cursor_disable();
		return 0;
	}

	if ((width != PL111_MAX_CURSOR_WIDTH) ||
	    (height != PL111_MAX_CURSOR_HEIGHT))
		return -EINVAL;

	obj = drm_gem_object_lookup(crtc->dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("Cannot find cursor object for handle = %d\n",
			  handle);
		return -ENOENT;
	}

	/*
	 * We expect a PL111_MAX_CURSOR_WIDTH x PL111_MAX_CURSOR_HEIGHT
	 * ARGB888 buffer object in the input.
	 *
	 */
	if (obj->size < (PL111_MAX_CURSOR_WIDTH * PL111_MAX_CURSOR_HEIGHT * 4)) {
		DRM_ERROR("Cannot set cursor with an obj size = %d\n",
			  obj->size);
		drm_gem_object_unreference_unlocked(obj);
		return -EINVAL;
	}

	bo = PL111_BO_FROM_GEM(obj);
	if (!(bo->type & PL111_BOT_DMA)) {
		DRM_ERROR("Tried to set cursor with non DMA backed obj = %p\n",
			  obj);
		drm_gem_object_unreference_unlocked(obj);
		return -EINVAL;
	}

	pl111_set_cursor_image(bo->backing_data.dma.fb_cpu_addr);

	/*
	 * Since we copy the contents of the buffer to the HW cursor internal
	 * memory this GEM object is not needed anymore.
	 */
	drm_gem_object_unreference_unlocked(obj);

	pl111_cursor_enable();

	return 0;
}

int pl111_crtc_cursor_move(struct drm_crtc *crtc,
			    int x, int y)
{
	int x_clip = 0;
	int y_clip = 0;

	DRM_DEBUG("x %d y %d\n", x, y);

	/*
	 * The cursor image is clipped automatically at the screen limits when
	 * it extends beyond the screen image to the right or bottom but
	 * we must clip it using pl111 HW features for negative values.
	 */
	if (x < 0) {
		x_clip = -x;
		x = 0;
	}
	if (y < 0) {
		y_clip = -y;
		y = 0;
	}

	pl111_set_cursor_clipping(x_clip, y_clip);
	pl111_set_cursor_position(x, y);

	return 0;
}

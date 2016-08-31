/*
 * drivers/video/tegra/dc/ext/cursor.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <video/tegra_dc_ext.h>
#include "tegra_dc_ext_priv.h"

/* ugh */
#include "../dc_priv.h"
#include "../dc_reg.h"

int tegra_dc_ext_get_cursor(struct tegra_dc_ext_user *user)
{
	struct tegra_dc_ext *ext = user->ext;
	int ret = 0;

	mutex_lock(&ext->cursor.lock);

	if (!ext->cursor.user)
		ext->cursor.user = user;
	else if (ext->cursor.user != user)
		ret = -EBUSY;

	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_put_cursor(struct tegra_dc_ext_user *user)
{
	struct tegra_dc_ext *ext = user->ext;
	int ret = 0;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user == user)
		ext->cursor.user = 0;
	else
		ret = -EACCES;

	mutex_unlock(&ext->cursor.lock);

	return ret;
}

static unsigned int set_cursor_start_addr(struct tegra_dc *dc,
					  u32 size, dma_addr_t phys_addr)
{
	unsigned long val;
	int clip_win;

	BUG_ON(phys_addr & ~CURSOR_START_ADDR_MASK);

	switch (size) {
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64:
		val = CURSOR_SIZE_64;
		break;
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_128x128:
		val = CURSOR_SIZE_128;
		break;
	case TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_256x256:
		val = CURSOR_SIZE_256;
		break;
	default:
		val = 0;
	}

	/* Get the cursor clip window number */
	clip_win = CURSOR_CLIP_GET_WINDOW(tegra_dc_readl(dc,
					  DC_DISP_CURSOR_START_ADDR));
	val |= clip_win;
#if defined(CONFIG_TEGRA_DC_64BIT_SUPPORT)
	/* TO DO: check calculation with HW */
	tegra_dc_writel(dc,
		(u32)(CURSOR_START_ADDR_HI(phys_addr)),
		DC_DISP_CURSOR_START_ADDR_HI);
	tegra_dc_writel(dc, (u32)(val |
			CURSOR_START_ADDR_LOW(phys_addr)),
		DC_DISP_CURSOR_START_ADDR);
#else
	tegra_dc_writel(dc,
		val | CURSOR_START_ADDR(((unsigned long) phys_addr)),
		DC_DISP_CURSOR_START_ADDR);
#endif

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra_dc_writel(dc, CURSOR_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, CURSOR_ACT_REQ, DC_CMD_STATE_CONTROL);
	return 0;
#else
	return 1;
#endif
}

static int set_cursor_position(struct tegra_dc *dc, s16 x, s16 y)
{
	tegra_dc_writel(dc, CURSOR_POSITION(x, y), DC_DISP_CURSOR_POSITION);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	tegra_dc_writel(dc, CURSOR_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, CURSOR_ACT_REQ, DC_CMD_STATE_CONTROL);
	return 0;
#else
	return 1;
#endif
}

static int set_cursor_activation_control(struct tegra_dc *dc)
{
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	u32 reg = tegra_dc_readl(dc, DC_CMD_REG_ACT_CONTROL);

	if ((reg & (1 << CURSOR_ACT_CNTR_SEL)) ==
	    (CURSOR_ACT_CNTR_SEL_V << CURSOR_ACT_CNTR_SEL)) {
		reg &= ~(1 << CURSOR_ACT_CNTR_SEL);
		reg |= (CURSOR_ACT_CNTR_SEL_V << CURSOR_ACT_CNTR_SEL);
		tegra_dc_writel(dc, reg, DC_CMD_REG_ACT_CONTROL);
		return 1;
	}
#endif
	return 0;
}

static int set_cursor_enable(struct tegra_dc *dc, bool enable)
{
	u32 val = tegra_dc_readl(dc, DC_DISP_DISP_WIN_OPTIONS);
	if (!!(val & CURSOR_ENABLE) != enable) {
		val &= ~CURSOR_ENABLE;
		if (enable)
			val |= CURSOR_ENABLE;
		tegra_dc_writel(dc, val, DC_DISP_DISP_WIN_OPTIONS);
		return 1;
	}
	return 0;
}

static int set_cursor_blend(struct tegra_dc *dc, u32 format)
{
	u32 val = tegra_dc_readl(dc, DC_DISP_BLEND_CURSOR_CONTROL);

	u32 newval = WINH_CURS_SELECT(0);

	switch (format) {
	case TEGRA_DC_EXT_CURSOR_FORMAT_2BIT_LEGACY:
		newval |= CURSOR_MODE_SELECT(0);
		break;
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_NON_PREMULT_ALPHA:
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_PREMULT_ALPHA:
		newval |= CURSOR_MODE_SELECT(1);
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
		newval |= CURSOR_ALPHA | CURSOR_DST_BLEND_FACTOR_SELECT(2);
		if (format == TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_PREMULT_ALPHA)
			newval |= CURSOR_SRC_BLEND_FACTOR_SELECT(0);
		else
			newval |= CURSOR_SRC_BLEND_FACTOR_SELECT(1);
#endif
		break;
	}

	if (val != newval) {
		tegra_dc_writel(dc, newval, DC_DISP_BLEND_CURSOR_CONTROL);
		return 1;
	}

	return 0;
}

static int set_cursor_fg_bg(struct tegra_dc *dc,
			    struct tegra_dc_ext_cursor_image *args)
{
	int general_update_needed = 0;

	u32 fg = CURSOR_COLOR(args->foreground.r,
			      args->foreground.g,
			      args->foreground.b);
	u32 bg = CURSOR_COLOR(args->background.r,
			      args->background.g,
			      args->background.b);

	if (fg != tegra_dc_readl(dc, DC_DISP_CURSOR_FOREGROUND)) {
		tegra_dc_writel(dc, fg, DC_DISP_CURSOR_FOREGROUND);
		general_update_needed |= 1;
	}

	if (bg != tegra_dc_readl(dc, DC_DISP_CURSOR_BACKGROUND)) {
		tegra_dc_writel(dc, bg, DC_DISP_CURSOR_BACKGROUND);
		general_update_needed |= 1;
	}

	return general_update_needed;
}

static bool check_cursor_size(struct tegra_dc *dc, u32 size)
{
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (size != TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_32x32 &&
	    size !=  TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64)
		return false;
#else
	if (size != TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_32x32 &&
	    size !=  TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_64x64 &&
	    size !=  TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_128x128 &&
	    size !=  TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE_256x256)
		return false;
#endif
	return true;
}

int tegra_dc_ext_set_cursor_image(struct tegra_dc_ext_user *user,
				  struct tegra_dc_ext_cursor_image *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	struct tegra_dc_dmabuf *handle, *old_handle;
	dma_addr_t phys_addr;
	u32 size;
	int ret;
	int need_general_update = 0;
	u32 format = TEGRA_DC_EXT_CURSOR_FORMAT_FLAGS(args->flags);

	if (!user->nvmap)
		return -EFAULT;

	size = TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE(args->flags);

	if (!check_cursor_size(dc, size))
		return -EINVAL;

	switch (format) {
	case TEGRA_DC_EXT_CURSOR_FORMAT_2BIT_LEGACY:
		break;
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_12x_SOC) || \
	defined(CONFIG_ARCH_TEGRA_14x_SOC)
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_NON_PREMULT_ALPHA:
		break;
#endif
#if defined(CONFIG_ARCH_TEGRA_12x_SOC) || defined(CONFIG_ARCH_TEGRA_14x_SOC)
	case TEGRA_DC_EXT_CURSOR_FORMAT_RGBA_PREMULT_ALPHA:
		break;
#endif
	default:
		return -EINVAL;
	}

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	old_handle = ext->cursor.cur_handle;

	ret = tegra_dc_ext_pin_window(user, args->buff_id, &handle, &phys_addr);
	if (ret)
		goto unlock;

	ext->cursor.cur_handle = handle;

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	need_general_update |= set_cursor_start_addr(dc, size, phys_addr);

	need_general_update |= set_cursor_fg_bg(dc, args);

	need_general_update |= set_cursor_blend(dc, format);

	if (need_general_update) {
		tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	tegra_dc_put(dc);
	/* XXX sync here? */

	mutex_unlock(&dc->lock);

	mutex_unlock(&ext->cursor.lock);

	if (old_handle) {
		dma_buf_unmap_attachment(old_handle->attach,
			old_handle->sgt, DMA_TO_DEVICE);
		dma_buf_detach(old_handle->buf, old_handle->attach);
		dma_buf_put(old_handle->buf);
		kfree(old_handle);
	}

	return 0;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_set_cursor(struct tegra_dc_ext_user *user,
			    struct tegra_dc_ext_cursor *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	bool enable;
	int ret;
	int need_general_update = 0;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	enable = !!(args->flags & TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE);

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	need_general_update |= set_cursor_enable(dc, enable);

	need_general_update |= set_cursor_position(dc, args->x, args->y);

	need_general_update |= set_cursor_activation_control(dc);

	if (need_general_update) {
		tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	/* TODO: need to sync here?  hopefully can avoid this, but need to
	 * figure out interaction w/ rest of GENERAL_ACT_REQ */

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	mutex_unlock(&ext->cursor.lock);

	return 0;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_cursor_clip(struct tegra_dc_ext_user *user,
			    int *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	int ret;
	unsigned long reg_val;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	mutex_lock(&dc->lock);
	tegra_dc_get(dc);

	reg_val = tegra_dc_readl(dc, DC_DISP_CURSOR_START_ADDR);
	reg_val &= ~CURSOR_CLIP_SHIFT_BITS(3); /* Clear out the old value */
	tegra_dc_writel(dc, reg_val | CURSOR_CLIP_SHIFT_BITS(*args),
			DC_DISP_CURSOR_START_ADDR);

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	mutex_unlock(&ext->cursor.lock);

	return 0;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_set_cursor_image_low_latency(struct tegra_dc_ext_user *user,
			struct tegra_dc_ext_cursor_image *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	int ret;
	int need_general_update = 0;

	mutex_lock(&ext->cursor.lock);
	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	mutex_lock(&dc->lock);

	tegra_dc_get(dc);

	need_general_update |= set_cursor_fg_bg(dc, args);

	need_general_update |= set_cursor_blend(dc, !!args->mode);

	if (need_general_update) {
		tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	tegra_dc_put(dc);
	mutex_unlock(&dc->lock);

	mutex_unlock(&ext->cursor.lock);

	return 0;

unlock:
	mutex_unlock(&ext->cursor.lock);

	return ret;
}

int tegra_dc_ext_set_cursor_low_latency(struct tegra_dc_ext_user *user,
			struct tegra_dc_ext_cursor_image *args)
{
	struct tegra_dc_ext *ext = user->ext;
	struct tegra_dc *dc = ext->dc;
	u32 size;
	int ret;
	struct tegra_dc_dmabuf *handle, *old_handle;
	dma_addr_t phys_addr;
	bool enable = !!(args->vis & TEGRA_DC_EXT_CURSOR_FLAGS_VISIBLE);
	int need_general_update = 0;

	if (!user->nvmap)
		return -EFAULT;

	size = TEGRA_DC_EXT_CURSOR_IMAGE_FLAGS_SIZE(args->flags);

	if (!check_cursor_size(dc, size))
		return -EINVAL;

	mutex_lock(&ext->cursor.lock);

	if (ext->cursor.user != user) {
		ret = -EACCES;
		goto unlock;
	}

	if (!ext->enabled) {
		ret = -ENXIO;
		goto unlock;
	}

	old_handle = ext->cursor.cur_handle;

	ret = tegra_dc_ext_pin_window(user, args->buff_id, &handle, &phys_addr);

	if (ret)
		goto unlock;

	ext->cursor.cur_handle = handle;

	mutex_lock(&dc->lock);

	tegra_dc_get(dc);

	need_general_update |= set_cursor_start_addr(dc, size, phys_addr);

	need_general_update |= set_cursor_position(dc, args->x, args->y);

	need_general_update |= set_cursor_activation_control(dc);

	need_general_update |= set_cursor_enable(dc, enable);

	if (need_general_update) {
		tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
		tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	}

	tegra_dc_put(dc);

	mutex_unlock(&dc->lock);

	mutex_unlock(&ext->cursor.lock);

	if (old_handle) {
		dma_buf_unmap_attachment(old_handle->attach,
			old_handle->sgt, DMA_TO_DEVICE);
		dma_buf_detach(old_handle->buf, handle->attach);
		dma_buf_put(old_handle->buf);
		kfree(old_handle);
	}
	return 0;

unlock:
	mutex_unlock(&ext->cursor.lock);
	return ret;
}



/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MTK_DRM_FB_H
#define MTK_DRM_FB_H

size_t mtk_fb_get_size(struct drm_framebuffer *fb);
struct drm_gem_object *mtk_fb_get_gem_obj(struct drm_framebuffer *fb);
dma_addr_t mtk_fb_get_dma(struct drm_framebuffer *fb);
int mtk_fb_wait(struct drm_framebuffer *fb);
struct drm_framebuffer *
mtk_drm_mode_fb_create(struct drm_device *dev, struct drm_file *file,
		       const struct drm_mode_fb_cmd2 *cmd);
struct drm_framebuffer *
mtk_drm_framebuffer_create(struct drm_device *dev,
			   const struct drm_mode_fb_cmd2 *mode,
			   struct drm_gem_object *obj);
bool mtk_drm_fb_is_secure(struct drm_framebuffer *fb);
int mtk_fb_get_sec_id(struct drm_framebuffer *fb);
#endif /* MTK_DRM_FB_H */

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

#define FBDEV_BPP	16

struct drm_gem_object *mtk_fb_get_gem_obj(struct drm_framebuffer *fb);
int mtk_fb_wait(struct drm_framebuffer *fb);
struct drm_framebuffer *mtk_drm_mode_fb_create(struct drm_device *dev,
					       struct drm_file *file,
					       struct drm_mode_fb_cmd2 *cmd);

void mtk_drm_mode_output_poll_changed(struct drm_device *dev);
int mtk_fbdev_create(struct drm_device *dev);
void mtk_fbdev_destroy(struct drm_device *dev);

#endif /* MTK_DRM_FB_H */

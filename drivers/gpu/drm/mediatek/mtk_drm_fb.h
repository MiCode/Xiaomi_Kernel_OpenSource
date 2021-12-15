/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

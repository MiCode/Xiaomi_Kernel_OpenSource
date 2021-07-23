/*
 * Copyright (c) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef MTK_DRM_FBDEV_H
#define MTK_DRM_FBDEV_H

struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1]; /* this is the minimum size */
};

#ifdef CONFIG_DRM_FBDEV_EMULATION
int mtk_fbdev_init(struct drm_device *dev);
void mtk_fbdev_fini(struct drm_device *dev);
#else
int mtk_fbdev_init(struct drm_device *dev)
{
	return 0;
}

void mtk_fbdev_fini(struct drm_device *dev)
{
}
#endif /* CONFIG_DRM_FBDEV_EMULATION */

int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
			unsigned int *fps);
bool mtk_drm_lcm_is_connect(void);
int free_fb_buf(void);
#define MTKFB_FACTORY_AUTO_TEST _IOR('O', 25, unsigned long)
int pan_display_test(int frame_num, int bpp);

#endif /* MTK_DRM_FBDEV_H */

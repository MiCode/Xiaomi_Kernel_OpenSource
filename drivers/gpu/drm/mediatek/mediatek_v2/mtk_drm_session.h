/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_SESSION_H_
#define _MTK_DRM_SESSION_H_

#include <drm/mediatek_drm.h>

#define MAX_SESSION_COUNT 4

#define MTK_SESSION_MODE(id) (((id) >> 24) & 0xff)
#define MTK_SESSION_TYPE(id) (((id) >> 16) & 0xff)
#define MTK_SESSION_DEV(id) ((id)&0xff)
#define MAKE_MTK_SESSION(type, dev) (unsigned int)((type) << 16 | (dev))

enum MTK_SESSION_TYPE {
	MTK_SESSION_PRIMARY = 1,
	MTK_SESSION_EXTERNAL = 2,
	MTK_SESSION_MEMORY = 3,
	MTK_SESSION_SP = 4,
};

struct mtk_session_mode_tb {
	unsigned int en;
	unsigned int ddp_mode[MAX_SESSION_COUNT];
};
/**
 * struct mtk_drm_session - MediaTek specific session structure.
 * @session_id:
 */
struct mtk_drm_session {
	unsigned int session_id;
	struct mtk_session_mode_tb mode_tb[MTK_DRM_SESSION_NUM];
};

int mtk_drm_session_create(struct drm_device *dev,
			   struct drm_mtk_session *config);
int mtk_drm_session_destroy(struct drm_device *dev,
			    struct drm_mtk_session *config);

/* create session */
int mtk_drm_session_create_ioctl(struct drm_device *dev, void *data,
				 struct drm_file *file_priv);
/* destroy session */
int mtk_drm_session_destroy_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file_priv);

int mtk_session_get_mode(struct drm_device *dev, struct drm_crtc *crtc);
int mtk_session_set_mode(struct drm_device *dev, unsigned int session_mode);
int mtk_get_session_id(struct drm_crtc *crtc);

#endif

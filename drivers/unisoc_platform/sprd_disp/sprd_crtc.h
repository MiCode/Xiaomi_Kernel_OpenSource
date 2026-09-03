/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_CRTC_H_
#define _SPRD_CRTC_H_

#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_plane.h>
#include <drm/drm_print.h>

#include "sprd_gem.h"

#define to_sprd_crtc(x)			container_of(x, struct sprd_crtc, base)
#define to_sprd_crtc_state(x)		container_of(x, struct sprd_crtc_state, base)

#define BIT_DPU_INT_DONE_		BIT(0)
#define BIT_DPU_INT_TE			BIT(1)
#define BIT_DPU_INT_ERR			BIT(2)
#define BIT_DPU_INT_EDPI_TE		BIT(3)
#define BIT_DPU_INT_UPDATE_DONE		BIT(4)
#define BIT_DPU_INT_VSYNC		BIT(5)
#define BIT_DPU_INT_WB_DONE		BIT(6)
#define BIT_DPU_INT_WB_ERR		BIT(7)

enum sprd_crtc_output_type {
	SPRD_DISPLAY_TYPE_NONE,
	/* RGB or CPU Interface */
	SPRD_DISPLAY_TYPE_LCD,
	/* DisplayPort Interface */
	SPRD_DISPLAY_TYPE_DP,
	/* HDMI Interface */
	SPRD_DISPLAY_TYPE_HDMI,
};

struct sprd_crtc_capability {
	u32 max_layers;
	const u32 *fmts_ptr;
	u32 fmts_cnt;
};

struct sprd_crtc_state {
	struct drm_crtc_state base;
	bool resolution_change;
	bool frame_rate_change;
};

struct sprd_crtc {
	struct drm_crtc base;
	enum sprd_crtc_output_type type;
	const struct sprd_crtc_ops *ops;
	struct sprd_plane *planes;
	u8 pending_planes;
	void *priv;
	bool fps_mode_changed;
	bool sr_mode_changed;
	struct drm_property *resolution_property;
	struct drm_property *frame_rate_property;
};

struct sprd_crtc_ops {
	void (*atomic_enable)(struct sprd_crtc *crtc);
	void (*atomic_disable)(struct sprd_crtc *crtc);
	int (*enable_vblank)(struct sprd_crtc *crtc);
	void (*disable_vblank)(struct sprd_crtc *crtc);
	enum drm_mode_status (*mode_valid)(struct sprd_crtc *crtc,
		const struct drm_display_mode *mode);
	void (*mode_set_nofb)(struct sprd_crtc *crtc);
	int (*atomic_check)(struct sprd_crtc *crtc,
			    struct drm_crtc_state *state);
	void (*atomic_begin)(struct sprd_crtc *crtc);
	void (*atomic_flush)(struct sprd_crtc *crtc);

	void (*prepare_fb)(struct sprd_crtc *crtc,
			  struct drm_plane_state *new_state);
	void (*cleanup_fb)(struct sprd_crtc *crtc,
			   struct drm_plane_state *old_state);
	void (*atomic_update)(struct sprd_crtc *crtc,
			     struct drm_plane *plane);
};

int sprd_crtc_iommu_map(struct device *dev, struct sprd_gem_obj *sprd_gem);
void sprd_crtc_iommu_unmap(struct device *dev, struct sprd_gem_obj *sprd_gem);
void sprd_crtc_wait_last_commit_complete(struct drm_crtc *crtc);
struct sprd_crtc *sprd_crtc_init(struct drm_device *drm,
					struct sprd_plane *planes,
					enum sprd_crtc_output_type type,
					const struct sprd_crtc_ops *ops,
					const char *version,
					u32 corner_size,
					void *priv);
int sprd_drm_set_possible_crtcs(struct drm_encoder *encoder,
		enum sprd_crtc_output_type out_type);

#endif /* _SPRD_CRTC_H_ */

/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_KMS_H__
#define __MSM_KMS_H__

#include <linux/clk.h>
#include <linux/regulator/consumer.h>

#include "msm_drv.h"

#define MAX_PLANE	4

/**
 * Device Private DRM Mode Flags
 * drm_mode->private_flags
 */
/* Connector has interpreted seamless transition request as dynamic fps */
#define MSM_MODE_FLAG_SEAMLESS_DYNAMIC_FPS	(1<<0)
/* Transition to new mode requires a wait-for-vblank before the modeset */
#define MSM_MODE_FLAG_VBLANK_PRE_MODESET	(1<<1)

/* As there are different display controller blocks depending on the
 * snapdragon version, the kms support is split out and the appropriate
 * implementation is loaded at runtime.  The kms module is responsible
 * for constructing the appropriate planes/crtcs/encoders/connectors.
 */
struct msm_kms_funcs {
	/* hw initialization: */
	int (*hw_init)(struct msm_kms *kms);
	int (*postinit)(struct msm_kms *kms);
	/* irq handling: */
	void (*irq_preinstall)(struct msm_kms *kms);
	int (*irq_postinstall)(struct msm_kms *kms);
	void (*irq_uninstall)(struct msm_kms *kms);
	irqreturn_t (*irq)(struct msm_kms *kms);
	int (*enable_vblank)(struct msm_kms *kms, struct drm_crtc *crtc);
	void (*disable_vblank)(struct msm_kms *kms, struct drm_crtc *crtc);
	/* modeset, bracketing atomic_commit(): */
	void (*prepare_fence)(struct msm_kms *kms,
			struct drm_atomic_state *state);
	void (*prepare_commit)(struct msm_kms *kms,
			struct drm_atomic_state *state);
	void (*commit)(struct msm_kms *kms, struct drm_atomic_state *state);
	void (*complete_commit)(struct msm_kms *kms,
			struct drm_atomic_state *state);
	/* functions to wait for atomic commit completed on each CRTC */
	void (*wait_for_crtc_commit_done)(struct msm_kms *kms,
					struct drm_crtc *crtc);
	/* get msm_format w/ optional format modifiers from drm_mode_fb_cmd2 */
	const struct msm_format *(*get_format)(struct msm_kms *kms,
					const uint32_t format,
					const uint64_t *modifiers,
					const uint32_t modifiers_len);
	/* do format checking on format modified through fb_cmd2 modifiers */
	int (*check_modified_format)(const struct msm_kms *kms,
			const struct msm_format *msm_fmt,
			const struct drm_mode_fb_cmd2 *cmd,
			struct drm_gem_object **bos);
	/* misc: */
	long (*round_pixclk)(struct msm_kms *kms, unsigned long rate,
			struct drm_encoder *encoder);
	int (*set_split_display)(struct msm_kms *kms,
			struct drm_encoder *encoder,
			struct drm_encoder *slave_encoder,
			bool is_cmd_mode);
	void (*postopen)(struct msm_kms *kms, struct drm_file *file);
	/* cleanup: */
	void (*preclose)(struct msm_kms *kms, struct drm_file *file);
	void (*postclose)(struct msm_kms *kms, struct drm_file *file);
	void (*lastclose)(struct msm_kms *kms);
	void (*destroy)(struct msm_kms *kms);
};

struct msm_kms {
	const struct msm_kms_funcs *funcs;

	/* irq handling: */
	bool in_irq;
	struct list_head irq_list;    /* list of mdp4_irq */
	uint32_t vblank_mask;         /* irq bits set for userspace vblank */
};

static inline void msm_kms_init(struct msm_kms *kms,
		const struct msm_kms_funcs *funcs)
{
	kms->funcs = funcs;
}

#ifdef CONFIG_DRM_MSM_MDP4
struct msm_kms *mdp4_kms_init(struct drm_device *dev);
#else
static inline
struct msm_kms *mdp4_kms_init(struct drm_device *dev) { return NULL; };
#endif
struct msm_kms *mdp5_kms_init(struct drm_device *dev);
struct msm_kms *sde_kms_init(struct drm_device *dev);

/**
 * Mode Set Utility Functions
 */
static inline bool msm_is_mode_seamless(const struct drm_display_mode *mode)
{
	return (mode->flags & DRM_MODE_FLAG_SEAMLESS);
}

static inline bool msm_is_mode_dynamic_fps(const struct drm_display_mode *mode)
{
	return ((mode->flags & DRM_MODE_FLAG_SEAMLESS) &&
		(mode->private_flags & MSM_MODE_FLAG_SEAMLESS_DYNAMIC_FPS));
}

static inline bool msm_needs_vblank_pre_modeset(
		const struct drm_display_mode *mode)
{
	return (mode->private_flags & MSM_MODE_FLAG_VBLANK_PRE_MODESET);
}

#endif /* __MSM_KMS_H__ */

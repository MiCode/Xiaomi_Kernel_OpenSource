/*
 * Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SDE_ENCODER_PHYS_H__
#define __SDE_ENCODER_PHYS_H__

#include "sde_kms.h"
#include "sde_hw_intf.h"
#include "sde_hw_mdp_ctl.h"

#define MAX_PHYS_ENCODERS_PER_VIRTUAL 4

struct sde_encoder_phys;

struct sde_encoder_virt_ops {
	void (*handle_vblank_virt)(struct drm_encoder *);
};

struct sde_encoder_phys_ops {
	void (*mode_set)(struct sde_encoder_phys *encoder,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode,
			bool splitmode);
	bool (*mode_fixup)(struct sde_encoder_phys *encoder,
			const struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode);
	void (*enable)(struct sde_encoder_phys *encoder);
	void (*disable)(struct sde_encoder_phys *encoder);
	void (*destroy)(struct sde_encoder_phys *encoder);
	void (*get_hw_resources)(struct sde_encoder_phys *encoder,
			struct sde_encoder_hw_resources *hw_res);
	void (*get_vsync_info)(struct sde_encoder_phys *enc,
			struct vsync_info *vsync);
	void (*enable_split_config)(struct sde_encoder_phys *enc,
			bool enable);
};

struct sde_encoder_phys {
	struct drm_encoder *parent;
	struct sde_encoder_virt_ops parent_ops;
	struct sde_encoder_phys_ops phys_ops;
	struct sde_hw_intf *hw_intf;
	struct sde_hw_ctl *hw_ctl;
	struct sde_kms *sde_kms;
	struct drm_display_mode cached_mode;
	bool enabled;
	spinlock_t spin_lock;
};

/**
 * struct sde_encoder_phys_vid - sub-class of sde_encoder_phys to handle video
 *	mode specific operations
 * @base:		Baseclass physical encoder structure
 * @irq_idx:		IRQ interface lookup index
 * @vblank_complete:	for vblank irq synchronization
 */
struct sde_encoder_phys_vid {
	struct sde_encoder_phys base;
	int irq_idx;
	struct completion vblank_complete;
};

struct sde_encoder_virt {
	struct drm_encoder base;
	spinlock_t spin_lock;
	uint32_t bus_scaling_client;

	int num_phys_encs;
	struct sde_encoder_phys *phys_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];

	void (*kms_vblank_callback)(void *);
	void *kms_vblank_callback_data;
};

struct sde_encoder_phys *sde_encoder_phys_vid_init(struct sde_kms *sde_kms,
		enum sde_intf intf_idx,
		enum sde_ctl ctl_idx,
		struct drm_encoder *parent,
		struct sde_encoder_virt_ops
		parent_ops);

#endif /* __sde_encoder_phys_H__ */

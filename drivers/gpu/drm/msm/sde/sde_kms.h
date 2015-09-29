/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDE_KMS_H__
#define __SDE_KMS_H__

#include "msm_drv.h"
#include "msm_kms.h"
#include "mdp/mdp_kms.h"
#include "sde_hw_catalog.h"
#include "sde_hw_mdss.h"

struct sde_kms {
	struct mdp_kms base;
	struct drm_device *dev;
	int rev;
	struct sde_mdss_cfg *catalog;

	struct msm_mmu *mmu;

	/* io/register spaces: */
	void __iomem *mmio, *vbif;

	struct regulator *vdd;
	struct regulator *mmagic;
	struct regulator *venus;

	struct clk *axi_clk;
	struct clk *ahb_clk;
	struct clk *src_clk;
	struct clk *core_clk;
	struct clk *lut_clk;
	struct clk *mmagic_clk;
	struct clk *iommu_clk;
	struct clk *vsync_clk;

	struct {
		unsigned long enabled_mask;
		struct irq_domain *domain;
	} irqcontroller;
};

#define to_sde_kms(x) container_of(x, struct sde_kms, base)

struct sde_plane_state {
	struct drm_plane_state base;

	/* aligned with property */
	uint8_t premultiplied;
	uint8_t zpos;
	uint8_t alpha;

	/* assigned by crtc blender */
	enum sde_stage stage;

	/* some additional transactional status to help us know in the
	 * apply path whether we need to update SMP allocation, and
	 * whether current update is still pending:
	 */
	bool mode_changed : 1;
	bool pending : 1;
};

#define to_sde_plane_state(x) \
		container_of(x, struct sde_plane_state, base)

int sde_disable(struct sde_kms *sde_kms);
int sde_enable(struct sde_kms *sde_kms);

void sde_set_irqmask(struct mdp_kms *mdp_kms, uint32_t irqmask,
		uint32_t old_irqmask);
void sde_irq_preinstall(struct msm_kms *kms);
int sde_irq_postinstall(struct msm_kms *kms);
void sde_irq_uninstall(struct msm_kms *kms);
irqreturn_t sde_irq(struct msm_kms *kms);
int sde_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);
void sde_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);

enum sde_sspp sde_plane_pipe(struct drm_plane *plane);
void sde_plane_install_properties(struct drm_plane *plane,
		struct drm_mode_object *obj);
void sde_plane_set_scanout(struct drm_plane *plane,
		struct drm_framebuffer *fb);
int sde_plane_mode_set(struct drm_plane *plane,
		struct drm_crtc *crtc, struct drm_framebuffer *fb,
		int crtc_x, int crtc_y,
		unsigned int crtc_w, unsigned int crtc_h,
		uint32_t src_x, uint32_t src_y,
		uint32_t src_w, uint32_t src_h);
void sde_plane_complete_flip(struct drm_plane *plane);
struct drm_plane *sde_plane_init(struct drm_device *dev, bool private_plane);

uint32_t sde_crtc_vblank(struct drm_crtc *crtc);

void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc, struct drm_file *file);
void sde_crtc_attach(struct drm_crtc *crtc, struct drm_plane *plane);
void sde_crtc_detach(struct drm_crtc *crtc, struct drm_plane *plane);
struct drm_crtc *sde_crtc_init(struct drm_device *dev,
		struct drm_encoder *encoder,
		struct drm_plane *plane, int id);

struct drm_encoder *sde_encoder_init(struct drm_device *dev, int intf);

int sde_irq_domain_init(struct sde_kms *sde_kms);
int sde_irq_domain_fini(struct sde_kms *sde_kms);

#endif /* __sde_kms_H__ */

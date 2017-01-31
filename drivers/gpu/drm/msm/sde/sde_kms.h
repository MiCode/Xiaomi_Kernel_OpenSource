/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include "sde_hw_mdp_ctl.h"
#include "sde_hw_lm.h"
#include "sde_hw_interrupts.h"

/*
 * struct sde_irq_callback - IRQ callback handlers
 * @func: intr handler
 * @arg: argument for the handler
 */
struct sde_irq_callback {
	void (*func)(void *arg, int irq_idx);
	void *arg;
};

/**
 * struct sde_irq: IRQ structure contains callback registration info
 * @total_irq:    total number of irq_idx obtained from HW interrupts mapping
 * @irq_cb_tbl:   array of IRQ callbacks setting
 * @cb_lock:      callback lock
 */
struct sde_irq {
	u32 total_irqs;
	struct sde_irq_callback *irq_cb_tbl;
	spinlock_t cb_lock;
};

/**
 *  struct sde_hw_res_map : Default resource table identifying default
 *             hw resource map. Primarily used for forcing DSI to use CTL_0/1
 *             and Pingpong 0/1, if the field is set to SDE_NONE means any HW
 *             intstance for that tpye is allowed as long as it is unused.
 */
struct sde_hw_res_map {
	enum sde_intf intf;
	enum sde_lm lm;
	enum sde_pingpong pp;
	enum sde_ctl ctl;
};

/* struct sde_hw_resource_manager : Resource mananger maintains the current
 *                                  platform configuration and manages shared
 *                                  hw resources ex:ctl_path hw driver context
 *                                  is needed by CRTCs/PLANEs/ENCODERs
 * @ctl        : table of control path hw driver contexts allocated
 * @mixer      : list of mixer hw drivers contexts allocated
 * @intr       : pointer to hw interrupt context
 * @res_table  : pointer to default hw_res table for this platform
 * @feature_map :BIT map for default enabled features ex:specifies if PP_SPLIT
 *               is enabled/disabled by defalt for this platform
 */
struct sde_hw_resource_manager {
	struct sde_hw_ctl *ctl[CTL_MAX];
	struct sde_hw_mixer *mixer[LM_MAX];
	struct sde_hw_intr *intr;
	const struct sde_hw_res_map *res_table;
	bool feature_map;
};

struct sde_kms {
	struct msm_kms base;
	struct drm_device *dev;
	int rev;
	struct sde_mdss_cfg *catalog;

	struct msm_mmu *mmu;
	int mmu_id;

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

	struct sde_hw_intr *hw_intr;
	struct sde_irq irq_obj;
	struct sde_hw_resource_manager hw_res;
};

struct vsync_info {
	u32 frame_count;
	u32 line_count;
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

/**
 * HW resource manager functions
 * @sde_rm_acquire_ctl_path : Allocates control path
 * @sde_rm_get_ctl_path     : returns control path driver context for already
 *                           acquired ctl path
 * @sde_rm_release_ctl_path : Frees control path driver context
 * @sde_rm_acquire_mixer   : Allocates mixer hw driver context
 * @sde_rm_get_mixer       : returns mixer context for already
 *                           acquired mixer
 * @sde_rm_release_mixer   : Frees mixer hw driver context
 * @sde_rm_get_hw_res_map  : Returns map for the passed INTF
 */
struct sde_hw_ctl *sde_rm_acquire_ctl_path(struct sde_kms *sde_kms,
		enum sde_ctl idx);
struct sde_hw_ctl *sde_rm_get_ctl_path(struct sde_kms *sde_kms,
		enum sde_ctl idx);
void sde_rm_release_ctl_path(struct sde_kms *sde_kms,
		enum sde_ctl idx);
struct sde_hw_mixer *sde_rm_acquire_mixer(struct sde_kms *sde_kms,
		enum sde_lm idx);
struct sde_hw_mixer *sde_rm_get_mixer(struct sde_kms *sde_kms,
		enum sde_lm idx);
void sde_rm_release_mixer(struct sde_kms *sde_kms,
		enum sde_lm idx);
struct sde_hw_intr *sde_rm_acquire_intr(struct sde_kms *sde_kms);
struct sde_hw_intr *sde_rm_get_intr(struct sde_kms *sde_kms);

const struct sde_hw_res_map *sde_rm_get_res_map(struct sde_kms *sde_kms,
		enum sde_intf idx);

/**
 * IRQ functions
 */
int sde_irq_domain_init(struct sde_kms *sde_kms);
int sde_irq_domain_fini(struct sde_kms *sde_kms);
void sde_irq_preinstall(struct msm_kms *kms);
int sde_irq_postinstall(struct msm_kms *kms);
void sde_irq_uninstall(struct msm_kms *kms);
irqreturn_t sde_irq(struct msm_kms *kms);

/**
 * sde_set_irqmask - IRQ helper function for writing IRQ mask
 *                   to SDE HW interrupt register.
 * @sde_kms:		SDE handle
 * @reg_off:		SDE HW interrupt register offset
 * @irqmask:		IRQ mask
 */
void sde_set_irqmask(
		struct sde_kms *sde_kms,
		uint32_t reg_off,
		uint32_t irqmask);

/**
 * sde_irq_idx_lookup - IRQ helper function for lookup irq_idx from HW
 *                      interrupt mapping table.
 * @sde_kms:		SDE handle
 * @intr_type:		SDE HW interrupt type for lookup
 * @instance_idx:	SDE HW block instance defined in sde_hw_mdss.h
 * @return:		irq_idx or -EINVAL when fail to lookup
 */
int sde_irq_idx_lookup(
		struct sde_kms *sde_kms,
		enum sde_intr_type intr_type,
		uint32_t instance_idx);

/**
 * sde_enable_irq - IRQ helper function for enabling one or more IRQs
 * @sde_kms:		SDE handle
 * @irq_idxs:		Array of irq index
 * @irq_count:		Number of irq_idx provided in the array
 * @return:		0 for success enabling IRQ, otherwise failure
 */
int sde_enable_irq(
		struct sde_kms *sde_kms,
		int *irq_idxs,
		uint32_t irq_count);

/**
 * sde_disable_irq - IRQ helper function for diabling one of more IRQs
 * @sde_kms:		SDE handle
 * @irq_idxs:		Array of irq index
 * @irq_count:		Number of irq_idx provided in the array
 * @return:		0 for success disabling IRQ, otherwise failure
 */
int sde_disable_irq(
		struct sde_kms *sde_kms,
		int *irq_idxs,
		uint32_t irq_count);

/**
 * sde_register_irq_callback - For registering callback function on IRQ
 *                             interrupt
 * @sde_kms:		SDE handle
 * @irq_idx:		irq index
 * @irq_cb:		IRQ callback structure, containing callback function
 *			and argument. Passing NULL for irq_cb will unregister
 *			the callback for the given irq_idx
 * @return:		0 for success registering callback, otherwise failure
 */
int sde_register_irq_callback(
		struct sde_kms *sde_kms,
		int irq_idx,
		struct sde_irq_callback *irq_cb);

/**
 * sde_clear_all_irqs - Clearing all SDE IRQ interrupt status
 * @sde_kms:		SDE handle
 */
void sde_clear_all_irqs(struct sde_kms *sde_kms);

/**
 * sde_disable_all_irqs - Diabling all SDE IRQ interrupt
 * @sde_kms:		SDE handle
 */
void sde_disable_all_irqs(struct sde_kms *sde_kms);

/**
 * Vblank enable/disable functions
 */
int sde_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);
void sde_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);

/**
 * Plane functions
 */
enum sde_sspp sde_plane_pipe(struct drm_plane *plane);
struct drm_plane *sde_plane_init(struct drm_device *dev, uint32_t pipe,
		bool private_plane);

/**
 * CRTC functions
 */
uint32_t sde_crtc_vblank(struct drm_crtc *crtc);
void sde_crtc_wait_for_commit_done(struct drm_crtc *crtc);
void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc, struct drm_file *file);
struct drm_crtc *sde_crtc_init(struct drm_device *dev,
		struct drm_encoder *encoder,
		struct drm_plane *plane, int id);

/**
 * Encoder functions and data types
 */
struct sde_encoder_hw_resources {
	enum sde_intf_mode intfs[INTF_MAX];
	bool pingpongs[PINGPONG_MAX];
	bool ctls[CTL_MAX];
	bool pingpongsplit;
};

void sde_encoder_get_hw_resources(struct drm_encoder *encoder,
		struct sde_encoder_hw_resources *hw_res);
void sde_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*cb)(void *), void *data);
void sde_encoders_init(struct drm_device *dev);
void sde_encoder_get_vsync_info(struct drm_encoder *encoder,
		struct vsync_info *vsync);



#endif /* __sde_kms_H__ */

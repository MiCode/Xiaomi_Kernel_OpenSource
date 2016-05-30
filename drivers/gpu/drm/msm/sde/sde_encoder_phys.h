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
#include "sde_hw_pingpong.h"
#include "sde_hw_ctl.h"
#include "sde_hw_top.h"

/**
 * enum sde_enc_split_role - Role this physical encoder will play in a
 *	split-panel configuration, where one panel is master, and others slaves.
 *	Masters have extra responsibilities, like managing the VBLANK IRQ.
 * @ENC_ROLE_SOLO:	This is the one and only panel. This encoder is master.
 * @ENC_ROLE_MASTER:	This encoder is the master of a split panel config.
 * @ENC_ROLE_SLAVE:	This encoder is not the master of a split panel config.
 */
enum sde_enc_split_role {
	ENC_ROLE_SOLO,
	ENC_ROLE_MASTER,
	ENC_ROLE_SLAVE
};

struct sde_encoder_phys;

/**
 * struct sde_encoder_virt_ops - Interface the containing virtual encoder
 *	provides for the physical encoders to use to callback.
 * @handle_vblank_virt:	Notify virtual encoder of vblank IRQ reception
 *			Note: This is called from IRQ handler context.
 * @handle_ready_for_kickoff:	Notify virtual encoder that this phys encoder
 *				is now ready for the next kickoff.
 */
struct sde_encoder_virt_ops {
	void (*handle_vblank_virt)(struct drm_encoder *);
	void (*handle_ready_for_kickoff)(struct drm_encoder *,
			struct sde_encoder_phys *phys);
};

/**
 * struct sde_encoder_phys_ops - Interface the physical encoders provide to
 *	the containing virtual encoder.
 * @is_master:			Whether this phys_enc is the current master
 *				encoder. Can be switched at enable time. Based
 *				on split_role and current mode (CMD/VID).
 * @mode_fixup:			DRM Call. Fixup a DRM mode.
 * @mode_set:			DRM Call. Set a DRM mode.
 *				This likely caches the mode, for use at enable.
 * @enable:			DRM Call. Enable a DRM mode.
 * @disable:			DRM Call. Disable mode.
 * @destroy:			DRM Call. Destroy and release resources.
 * @get_hw_resources:		Populate the structure with the hardware
 *				resources that this phys_enc is using.
 *				Expect no overlap between phys_encs.
 * @control_vblank_irq		Register/Deregister for VBLANK IRQ
 * @wait_for_commit_done:	Wait for hardware to have flushed the
 *				current pending frames to hardware
 * @prepare_for_kickoff:	Do any work necessary prior to a kickoff
 *				and report whether need to wait before
 *				triggering the next kickoff
 *				(ie for previous tx to complete)
 * @handle_post_kickoff:	Do any work necessary post-kickoff work
 */

struct sde_encoder_phys_ops {
	bool (*is_master)(struct sde_encoder_phys *encoder);
	bool (*mode_fixup)(struct sde_encoder_phys *encoder,
			const struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode);
	void (*mode_set)(struct sde_encoder_phys *encoder,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode);
	void (*enable)(struct sde_encoder_phys *encoder);
	void (*disable)(struct sde_encoder_phys *encoder);
	void (*destroy)(struct sde_encoder_phys *encoder);
	void (*get_hw_resources)(struct sde_encoder_phys *encoder,
			struct sde_encoder_hw_resources *hw_res);
	int (*control_vblank_irq)(struct sde_encoder_phys *enc, bool enable);
	int (*wait_for_commit_done)(struct sde_encoder_phys *phys_enc);
	void (*prepare_for_kickoff)(struct sde_encoder_phys *phys_enc,
			bool *wait_until_ready);
	void (*handle_post_kickoff)(struct sde_encoder_phys *phys_enc);
};

/**
 * enum sde_enc_enable_state - current enabled state of the physical encoder
 * @SDE_ENC_DISABLED:	Encoder is disabled
 * @SDE_ENC_ENABLING:	Encoder transitioning to enabled
 *			Events bounding transition are encoder type specific
 * @SDE_ENC_ENABLED:	Encoder is enabled
 */
enum sde_enc_enable_state {
	SDE_ENC_DISABLED,
	SDE_ENC_ENABLING,
	SDE_ENC_ENABLED
};

/**
 * struct sde_encoder_phys - physical encoder that drives a single INTF block
 *	tied to a specific panel / sub-panel. Abstract type, sub-classed by
 *	phys_vid or phys_cmd for video mode or command mode encs respectively.
 * @parent:		Pointer to the containing virtual encoder
 * @ops:		Operations exposed to the virtual encoder
 * @parent_ops:		Callbacks exposed by the parent to the phys_enc
 * @hw_mdptop:		Hardware interface to the top registers
 * @hw_ctl:		Hardware interface to the ctl registers
 * @sde_kms:		Pointer to the sde_kms top level
 * @cached_mode:	DRM mode cached at mode_set time, acted on in enable
 * @enabled:		Whether the encoder has enabled and running a mode
 * @split_role:		Role to play in a split-panel configuration
 * @spin_lock:		Lock for IRQ purposes
 * @mode_3d:		3D mux configuration
 * @enable_state:	Enable state tracking
 */
struct sde_encoder_phys {
	struct drm_encoder *parent;
	struct sde_encoder_phys_ops ops;
	struct sde_encoder_virt_ops parent_ops;
	struct sde_hw_mdp *hw_mdptop;
	struct sde_hw_ctl *hw_ctl;
	struct sde_kms *sde_kms;
	struct drm_display_mode cached_mode;
	enum sde_enc_split_role split_role;
	spinlock_t spin_lock;
	enum sde_3d_blend_mode mode_3d;
	enum sde_enc_enable_state enable_state;
};

/**
 * struct sde_encoder_phys_vid - sub-class of sde_encoder_phys to handle video
 *	mode specific operations
 * @base:	Baseclass physical encoder structure
 * @irq_idx:	IRQ interface lookup index
 * @hw_intf:	Hardware interface to the intf registers
 * @vblank_completion:	Completion event signaled on reception of the vsync irq
 */
struct sde_encoder_phys_vid {
	struct sde_encoder_phys base;
	int irq_idx;
	struct sde_hw_intf *hw_intf;
	struct completion vblank_completion;
};

/**
 * struct sde_encoder_phys_cmd - sub-class of sde_encoder_phys to handle command
 *	mode specific operations
 * @base:	Baseclass physical encoder structure
 * @intf_idx:	Intf Block index used by this phys encoder
 * @stream_sel:	Stream selection for multi-stream interfaces
 * @hw_pp:	Hardware interface to the ping pong registers
 * @pp_rd_ptr_irq_idx:	IRQ signifying panel's frame read pointer
 *			For CMD encoders, VBLANK is driven by the PP RD Done IRQ
 * @pp_tx_done_irq_idx:	IRQ signifying frame transmission to panel complete
 * @pp_tx_done_wq:	Wait queue that tracks when a commit is flushed
 *			to hardware after the reception of pp_done
 *			Used to prevent back to back commits
 * @pending_cnt:	Atomic counter tracking the number of kickoffs vs.
 *			the number of pp_done irqs. Should hover between 0-2
 *			Incremented when a new kickoff is scheduled
 *			Decremented in pp_done irq
 */
struct sde_encoder_phys_cmd {
	struct sde_encoder_phys base;
	int intf_idx;
	int stream_sel;
	struct sde_hw_pingpong *hw_pp;
	int pp_rd_ptr_irq_idx;
	int pp_tx_done_irq_idx;
	wait_queue_head_t pp_tx_done_wq;
	atomic_t pending_cnt;
};

/**
 * sde_encoder_phys_vid_init - Construct a new video mode physical encoder
 * @sde_kms:		Pointer to the sde_kms top level
 * @intf_idx:		Interface index this phys_enc will control
 * @ctl_idx:		Control index this phys_enc requires
 * @split_role:		Role to play in a split-panel configuration
 * @parent:		Pointer to the containing virtual encoder
 * @parent_ops:		Callbacks exposed by the parent to the phys_enc
 * Return: Error code or newly allocated encoder
 */
struct sde_encoder_phys *sde_encoder_phys_vid_init(
		struct sde_kms *sde_kms,
		enum sde_intf intf_idx,
		enum sde_ctl ctl_idx,
		enum sde_enc_split_role split_role,
		struct drm_encoder *parent,
		struct sde_encoder_virt_ops parent_ops);

/**
 * sde_encoder_phys_cmd_init - Construct a new command mode physical encoder
 * @sde_kms:		Pointer to the sde_kms top level
 * @intf_idx:		Interface index this phys_enc will control
 * @pp_idx:		PingPong index this phys_enc will control
 * @ctl_idx:		Control index this phys_enc requires
 * @split_role:		Role to play in a split-panel configuration
 * @parent:		Pointer to the containing virtual encoder
 * @parent_ops:		Callbacks exposed by the parent to the phys_enc
 * Return: Error code or newly allocated encoder
 */
struct sde_encoder_phys *sde_encoder_phys_cmd_init(
		struct sde_kms *sde_kms,
		enum sde_intf intf_idx,
		enum sde_pingpong pp_idx,
		enum sde_ctl ctl_idx,
		enum sde_enc_split_role split_role,
		struct drm_encoder *parent,
		struct sde_encoder_virt_ops parent_ops);



#endif /* __sde_encoder_phys_H__ */

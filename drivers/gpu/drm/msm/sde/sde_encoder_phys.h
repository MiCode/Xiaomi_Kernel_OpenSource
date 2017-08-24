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

#include <linux/jiffies.h>

#include "sde_kms.h"
#include "sde_hw_intf.h"
#include "sde_hw_pingpong.h"
#include "sde_hw_ctl.h"
#include "sde_hw_top.h"
#include "sde_hw_wb.h"
#include "sde_hw_cdm.h"
#include "sde_encoder.h"
#include "sde_connector.h"

#define SDE_ENCODER_NAME_MAX	16

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KICKOFF_TIMEOUT_MS		84
#define KICKOFF_TIMEOUT_JIFFIES		msecs_to_jiffies(KICKOFF_TIMEOUT_MS)

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
 * @handle_underrun_virt: Notify virtual encoder of underrun IRQ reception
 *			Note: This is called from IRQ handler context.
 * @handle_frame_done:	Notify virtual encoder that this phys encoder
 *			completes last request frame.
 */
struct sde_encoder_virt_ops {
	void (*handle_vblank_virt)(struct drm_encoder *,
			struct sde_encoder_phys *phys);
	void (*handle_underrun_virt)(struct drm_encoder *,
			struct sde_encoder_phys *phys);
	void (*handle_frame_done)(struct drm_encoder *,
			struct sde_encoder_phys *phys, u32 event);
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
 * @atomic_check:		DRM Call. Atomic check new DRM state.
 * @destroy:			DRM Call. Destroy and release resources.
 * @get_hw_resources:		Populate the structure with the hardware
 *				resources that this phys_enc is using.
 *				Expect no overlap between phys_encs.
 * @control_vblank_irq		Register/Deregister for VBLANK IRQ
 * @wait_for_commit_done:	Wait for hardware to have flushed the
 *				current pending frames to hardware
 * @prepare_for_kickoff:	Do any work necessary prior to a kickoff
 *				For CMD encoder, may wait for previous tx done
 * @handle_post_kickoff:	Do any work necessary post-kickoff work
 * @trigger_start:		Process start event on physical encoder
 * @needs_single_flush:		Whether encoder slaves need to be flushed
 * @setup_misr:		Sets up MISR, enable and disables based on sysfs
 * @collect_misr:		Collects MISR data on frame update
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
	int (*atomic_check)(struct sde_encoder_phys *encoder,
			    struct drm_crtc_state *crtc_state,
			    struct drm_connector_state *conn_state);
	void (*destroy)(struct sde_encoder_phys *encoder);
	void (*get_hw_resources)(struct sde_encoder_phys *encoder,
			struct sde_encoder_hw_resources *hw_res,
			struct drm_connector_state *conn_state);
	int (*control_vblank_irq)(struct sde_encoder_phys *enc, bool enable);
	int (*wait_for_commit_done)(struct sde_encoder_phys *phys_enc);
	void (*prepare_for_kickoff)(struct sde_encoder_phys *phys_enc);
	void (*handle_post_kickoff)(struct sde_encoder_phys *phys_enc);
	void (*trigger_start)(struct sde_encoder_phys *phys_enc);
	bool (*needs_single_flush)(struct sde_encoder_phys *phys_enc);

	void (*setup_misr)(struct sde_encoder_phys *phys_encs,
			struct sde_misr_params *misr_map);
	void (*collect_misr)(struct sde_encoder_phys *phys_enc,
			struct sde_misr_params *misr_map);
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
 * enum sde_intr_idx - sde encoder interrupt index
 * @INTR_IDX_VSYNC:    Vsync interrupt for video mode panel
 * @INTR_IDX_PINGPONG: Pingpong done interrupt for cmd mode panel
 * @INTR_IDX_UNDERRUN: Underrun interrupt for video and cmd mode panel
 * @INTR_IDX_RDPTR:    Readpointer done interrupt for cmd mode panel
 */
enum sde_intr_idx {
	INTR_IDX_VSYNC,
	INTR_IDX_PINGPONG,
	INTR_IDX_UNDERRUN,
	INTR_IDX_RDPTR,
	INTR_IDX_MAX,
};

/**
 * struct sde_encoder_phys - physical encoder that drives a single INTF block
 *	tied to a specific panel / sub-panel. Abstract type, sub-classed by
 *	phys_vid or phys_cmd for video mode or command mode encs respectively.
 * @parent:		Pointer to the containing virtual encoder
 * @connector:		If a mode is set, cached pointer to the active connector
 * @ops:		Operations exposed to the virtual encoder
 * @parent_ops:		Callbacks exposed by the parent to the phys_enc
 * @hw_mdptop:		Hardware interface to the top registers
 * @hw_ctl:		Hardware interface to the ctl registers
 * @hw_cdm:		Hardware interface to the cdm registers
 * @cdm_cfg:		Chroma-down hardware configuration
 * @hw_pp:		Hardware interface to the ping pong registers
 * @sde_kms:		Pointer to the sde_kms top level
 * @cached_mode:	DRM mode cached at mode_set time, acted on in enable
 * @misr_map:		Interface for setting and collecting MISR data
 * @enabled:		Whether the encoder has enabled and running a mode
 * @split_role:		Role to play in a split-panel configuration
 * @intf_mode:		Interface mode
 * @intf_idx:		Interface index on sde hardware
 * @enc_cdm_csc:	Cached CSC type of CDM block
 * @enc_spinlock:	Virtual-Encoder-Wide Spin Lock for IRQ purposes
 * @enable_state:	Enable state tracking
 * @vblank_refcount:	Reference count of vblank request
 * @vsync_cnt:		Vsync count for the physical encoder
 * @underrun_cnt:	Underrun count for the physical encoder
 * @pending_kickoff_cnt:	Atomic counter tracking the number of kickoffs
 *				vs. the number of done/vblank irqs. Should hover
 *				between 0-2 Incremented when a new kickoff is
 *				scheduled. Decremented in irq handler
 * @pending_kickoff_wq:		Wait queue for blocking until kickoff completes
 */
struct sde_encoder_phys {
	struct drm_encoder *parent;
	struct drm_connector *connector;
	struct sde_encoder_phys_ops ops;
	struct sde_encoder_virt_ops parent_ops;
	struct sde_hw_mdp *hw_mdptop;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_cdm *hw_cdm;
	struct sde_hw_cdm_cfg cdm_cfg;
	struct sde_hw_pingpong *hw_pp;
	struct sde_kms *sde_kms;
	struct drm_display_mode cached_mode;
	struct sde_misr_params *misr_map;
	enum sde_enc_split_role split_role;
	enum sde_intf_mode intf_mode;
	enum sde_intf intf_idx;
	enum sde_csc_type enc_cdm_csc;
	spinlock_t *enc_spinlock;
	enum sde_enc_enable_state enable_state;
	atomic_t vblank_refcount;
	atomic_t vsync_cnt;
	atomic_t underrun_cnt;
	atomic_t pending_kickoff_cnt;
	wait_queue_head_t pending_kickoff_wq;
};

static inline int sde_encoder_phys_inc_pending(struct sde_encoder_phys *phys)
{
	return atomic_inc_return(&phys->pending_kickoff_cnt);
}

/**
 * struct sde_encoder_phys_vid - sub-class of sde_encoder_phys to handle video
 *	mode specific operations
 * @base:	Baseclass physical encoder structure
 * @irq_idx:	IRQ interface lookup index
 * @irq_cb:	interrupt callback
 * @hw_intf:	Hardware interface to the intf registers
 */
struct sde_encoder_phys_vid {
	struct sde_encoder_phys base;
	int irq_idx[INTR_IDX_MAX];
	struct sde_irq_callback irq_cb[INTR_IDX_MAX];
	struct sde_hw_intf *hw_intf;
};

/**
 * struct sde_encoder_phys_cmd - sub-class of sde_encoder_phys to handle command
 *	mode specific operations
 * @base:	Baseclass physical encoder structure
 * @intf_idx:	Intf Block index used by this phys encoder
 * @stream_sel:	Stream selection for multi-stream interfaces
 * @pp_rd_ptr_irq_idx:	IRQ signifying panel's frame read pointer
 *			For CMD encoders, VBLANK is driven by the PP RD Done IRQ
 * @pp_tx_done_irq_idx:	IRQ signifying frame transmission to panel complete
 * @irq_cb:	interrupt callback
 */
struct sde_encoder_phys_cmd {
	struct sde_encoder_phys base;
	int intf_idx;
	int stream_sel;
	int irq_idx[INTR_IDX_MAX];
	struct sde_irq_callback irq_cb[INTR_IDX_MAX];
};

/**
 * struct sde_encoder_phys_wb - sub-class of sde_encoder_phys to handle
 *	writeback specific operations
 * @base:		Baseclass physical encoder structure
 * @hw_wb:		Hardware interface to the wb registers
 * @irq_idx:		IRQ interface lookup index
 * @wbdone_timeout:	Timeout value for writeback done in msec
 * @bypass_irqreg:	Bypass irq register/unregister if non-zero
 * @wbdone_complete:	for wbdone irq synchronization
 * @wb_cfg:		Writeback hardware configuration
 * @intf_cfg:		Interface hardware configuration
 * @wb_roi:		Writeback region-of-interest
 * @wb_fmt:		Writeback pixel format
 * @frame_count:	Counter of completed writeback operations
 * @kickoff_count:	Counter of issued writeback operations
 * @aspace:		address space identifier for non-secure/secure domain
 * @wb_dev:		Pointer to writeback device
 * @start_time:		Start time of writeback latest request
 * @end_time:		End time of writeback latest request
 * @wb_name:		Name of this writeback device
 * @debugfs_root:	Root entry of writeback debugfs
 */
struct sde_encoder_phys_wb {
	struct sde_encoder_phys base;
	struct sde_hw_wb *hw_wb;
	int irq_idx;
	struct sde_irq_callback irq_cb;
	u32 wbdone_timeout;
	u32 bypass_irqreg;
	struct completion wbdone_complete;
	struct sde_hw_wb_cfg wb_cfg;
	struct sde_hw_intf_cfg intf_cfg;
	struct sde_rect wb_roi;
	const struct sde_format *wb_fmt;
	u32 frame_count;
	u32 kickoff_count;
	struct msm_gem_address_space *aspace[SDE_IOMMU_DOMAIN_MAX];
	struct sde_wb_device *wb_dev;
	ktime_t start_time;
	ktime_t end_time;
#ifdef CONFIG_DEBUG_FS
	char wb_name[SDE_ENCODER_NAME_MAX];
	struct dentry *debugfs_root;
#endif
};

/**
 * struct sde_enc_phys_init_params - initialization parameters for phys encs
 * @sde_kms:		Pointer to the sde_kms top level
 * @parent:		Pointer to the containing virtual encoder
 * @parent_ops:		Callbacks exposed by the parent to the phys_enc
 * @split_role:		Role to play in a split-panel configuration
 * @intf_idx:		Interface index this phys_enc will control
 * @wb_idx:		Writeback index this phys_enc will control
 * @enc_spinlock:	Virtual-Encoder-Wide Spin Lock for IRQ purposes
 */
struct sde_enc_phys_init_params {
	struct sde_kms *sde_kms;
	struct drm_encoder *parent;
	struct sde_encoder_virt_ops parent_ops;
	enum sde_enc_split_role split_role;
	enum sde_intf intf_idx;
	enum sde_wb wb_idx;
	spinlock_t *enc_spinlock;
};

/**
 * sde_encoder_phys_vid_init - Construct a new video mode physical encoder
 * @p:	Pointer to init params structure
 * Return: Error code or newly allocated encoder
 */
struct sde_encoder_phys *sde_encoder_phys_vid_init(
		struct sde_enc_phys_init_params *p);

/**
 * sde_encoder_phys_cmd_init - Construct a new command mode physical encoder
 * @p:	Pointer to init params structure
 * Return: Error code or newly allocated encoder
 */
struct sde_encoder_phys *sde_encoder_phys_cmd_init(
		struct sde_enc_phys_init_params *p);

/**
 * sde_encoder_phys_wb_init - Construct a new writeback physical encoder
 * @p:	Pointer to init params structure
 * Return: Error code or newly allocated encoder
 */
#ifdef CONFIG_DRM_SDE_WB
struct sde_encoder_phys *sde_encoder_phys_wb_init(
		struct sde_enc_phys_init_params *p);
#else
static inline
struct sde_encoder_phys *sde_encoder_phys_wb_init(
		struct sde_enc_phys_init_params *p)
{
	return NULL;
}
#endif

void sde_encoder_phys_setup_cdm(struct sde_encoder_phys *phys_enc,
		const struct sde_format *format, u32 output_type,
		struct sde_rect *roi);

/**
 * sde_encoder_helper_trigger_start - control start helper function
 *	This helper function may be optionally specified by physical
 *	encoders if they require ctl_start triggering.
 * @phys_enc: Pointer to physical encoder structure
 */
void sde_encoder_helper_trigger_start(struct sde_encoder_phys *phys_enc);

/**
 * sde_encoder_helper_wait_event_timeout - wait for event with timeout
 *	taking into account that jiffies may jump between reads leading to
 *	incorrectly detected timeouts. Prevent failure in this scenario by
 *	making sure that elapsed time during wait is valid.
 * @drm_id: drm object id for logging
 * @hw_id: hw instance id for logging
 * @wq: wait queue structure
 * @cnt: atomic counter to wait on
 * @timeout_ms: timeout value in milliseconds
 */
int sde_encoder_helper_wait_event_timeout(
		int32_t drm_id,
		int32_t hw_id,
		wait_queue_head_t *wq,
		atomic_t *cnt,
		s64 timeout_ms);


static inline enum sde_3d_blend_mode sde_encoder_helper_get_3d_blend_mode(
		struct sde_encoder_phys *phys_enc)
{
	enum sde_rm_topology_name topology;

	topology = sde_connector_get_topology_name(phys_enc->connector);
	if (phys_enc->split_role == ENC_ROLE_SOLO &&
			topology == SDE_RM_TOPOLOGY_DUALPIPEMERGE)
		return BLEND_3D_H_ROW_INT;

	return BLEND_3D_NONE;
}

/**
 * sde_encoder_helper_split_config - split display configuration helper function
 *	This helper function may be used by physical encoders to configure
 *	the split display related registers.
 * @phys_enc: Pointer to physical encoder structure
 * @interface: enum sde_intf setting
 */
void sde_encoder_helper_split_config(
		struct sde_encoder_phys *phys_enc,
		enum sde_intf interface);

#endif /* __sde_encoder_phys_H__ */

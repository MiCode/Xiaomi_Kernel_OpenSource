/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_COLOR_PROCESSING_H
#define _SDE_COLOR_PROCESSING_H
#include "sde_hw_mdss.h"
#include <drm/drm_crtc.h>

struct sde_irq_callback;

/*
 * PA MEMORY COLOR types
 * @MEMCOLOR_SKIN          Skin memory color type
 * @MEMCOLOR_SKY           Sky memory color type
 * @MEMCOLOR_FOLIAGE       Foliage memory color type
 */
enum sde_memcolor_type {
	MEMCOLOR_SKIN = 0,
	MEMCOLOR_SKY,
	MEMCOLOR_FOLIAGE,
	MEMCOLOR_MAX
};

/*
 * PA HISTOGRAM modes
 * @HIST_DISABLED          Histogram disabled
 * @HIST_ENABLED           Histogram enabled
 */
enum sde_hist_modes {
	HIST_DISABLED,
	HIST_ENABLED
};

/**
 * struct drm_prop_enum_list - drm structure for creating enum property and
 *                             enumerating values
 */
static const struct drm_prop_enum_list sde_hist_modes[] = {
	{HIST_DISABLED, "hist_off"},
	{HIST_ENABLED, "hist_on"},
};

/*
 * LTM HISTOGRAM modes
 * @LTM_HIST_DISABLED          Histogram disabled
 * @LTM_HIST_ENABLED           Histogram enabled
 */
enum ltm_hist_modes {
	LTM_HIST_DISABLED,
	LTM_HIST_ENABLED
};

/**
 * struct drm_prop_enum_list - drm structure for creating enum property and
 *                             enumerating values
 */
static const struct drm_prop_enum_list sde_ltm_hist_modes[] = {
	{LTM_HIST_DISABLED, "ltm_hist_off"},
	{LTM_HIST_ENABLED, "ltm_hist_on"},
};

/**
 * sde_cp_crtc_features - list of color processing crtc features
 */
enum sde_cp_crtc_features {
	/* Append new DSPP features before SDE_CP_CRTC_DSPP_MAX */
	/* DSPP Features start */
	SDE_CP_CRTC_DSPP_IGC,
	SDE_CP_CRTC_DSPP_PCC,
	SDE_CP_CRTC_DSPP_GC,
	SDE_CP_CRTC_DSPP_HSIC,
	SDE_CP_CRTC_DSPP_MEMCOL_SKIN,
	SDE_CP_CRTC_DSPP_MEMCOL_SKY,
	SDE_CP_CRTC_DSPP_MEMCOL_FOLIAGE,
	SDE_CP_CRTC_DSPP_MEMCOL_PROT,
	SDE_CP_CRTC_DSPP_SIXZONE,
	SDE_CP_CRTC_DSPP_GAMUT,
	SDE_CP_CRTC_DSPP_DITHER,
	SDE_CP_CRTC_DSPP_HIST_CTRL,
	SDE_CP_CRTC_DSPP_HIST_IRQ,
	SDE_CP_CRTC_DSPP_AD,
	SDE_CP_CRTC_DSPP_VLUT,
	SDE_CP_CRTC_DSPP_AD_MODE,
	SDE_CP_CRTC_DSPP_AD_INIT,
	SDE_CP_CRTC_DSPP_AD_CFG,
	SDE_CP_CRTC_DSPP_AD_INPUT,
	SDE_CP_CRTC_DSPP_AD_ASSERTIVENESS,
	SDE_CP_CRTC_DSPP_AD_BACKLIGHT,
	SDE_CP_CRTC_DSPP_AD_STRENGTH,
	SDE_CP_CRTC_DSPP_AD_ROI,
	SDE_CP_CRTC_DSPP_LTM,
	SDE_CP_CRTC_DSPP_LTM_INIT,
	SDE_CP_CRTC_DSPP_LTM_ROI,
	SDE_CP_CRTC_DSPP_LTM_HIST_CTL,
	SDE_CP_CRTC_DSPP_LTM_HIST_THRESH,
	SDE_CP_CRTC_DSPP_LTM_SET_BUF,
	SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF,
	SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF2,
	SDE_CP_CRTC_DSPP_LTM_QUEUE_BUF3,
	SDE_CP_CRTC_DSPP_LTM_VLUT,
	SDE_CP_CRTC_DSPP_SB,
	SDE_CP_CRTC_DSPP_RC_MASK,
	SDE_CP_CRTC_DSPP_SPR_INIT,
	SDE_CP_CRTC_DSPP_DEMURA_INIT,
	SDE_CP_CRTC_DSPP_DEMURA_BACKLIGHT,
	SDE_CP_CRTC_DSPP_DEMURA_BOOT_PLANE,
	SDE_CP_CRTC_DSPP_MAX,
	/* DSPP features end */

	/* Append new LM features before SDE_CP_CRTC_MAX_FEATURES */
	/* LM feature start*/
	SDE_CP_CRTC_LM_GC,
	/* LM feature end*/

	SDE_CP_CRTC_MAX_FEATURES,
};

/**
 * struct sde_cp_crtc_property_state: struct to define the property states.
 * @prop: pointer to drm property
 * @prop_val: value of the property
 * @cp_node: pointer to cp feature info payload
 */
struct sde_cp_crtc_property_state {
	struct drm_property *prop;
	uint64_t prop_val;
	void *cp_node;
};

/**
 * struct sde_cp_crtc_range_prop_payload: struct to define range prop payload.
 * @addr: pointer to payload
 * @len: length of the property
 */

struct sde_cp_crtc_range_prop_payload {
	u64 addr;
	u32 len;
};

/**
 * struct sde_cp_crtc_skip_blend_plane: struct to define skip blend plane configuration
 * @valid_plane: flag to indicate plane is valid
 * @plane: plane that has been enabled and skipped blending
 * @width: plane width
 * @height: plane height
 */

struct sde_cp_crtc_skip_blend_plane {
	bool valid_plane;
	enum sde_sspp plane;
	u32 width;
	u32 height;
};

/**
 * sde_cp_crtc_init(): Initialize color processing lists for a crtc.
 *                     Should be called during crtc initialization.
 * @crtc:  Pointer to sde_crtc.
 */
void sde_cp_crtc_init(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_install_properties(): Installs the color processing
 *                                properties for a crtc.
 *                                Should be called during crtc initialization.
 * @crtc:  Pointer to crtc.
 */
void sde_cp_crtc_install_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_destroy_properties: Destroys color processing
 *                                            properties for a crtc.
 * should be called during crtc de-initialization.
 * @crtc:  Pointer to crtc.
 */
void sde_cp_crtc_destroy_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_refresh_status_properties(): Updates color processing
 *                                    properties reflecting the status
 *                                    of the crtc.
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_refresh_status_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_set_property: Set a color processing property
 *                                      for a crtc.
 *                                      Should be during atomic set property.
 * @crtc: Pointer to crtc.
 * @cstate: sde crtc property state holding list of dirty props.
 * @property: Property that needs to enabled/disabled.
 * @val: Value of property.
 */
int sde_cp_crtc_set_property(struct drm_crtc *crtc,
		struct drm_crtc_state *state,
		struct drm_property *property, uint64_t val);
/**
 * sde_cp_crtc_check_properties: Verify color processing properties for a crtc.
 *                               Should be called during atomic check call.
 * @crtc: Pointer to crtc.
 * @state: Pointer to crtc state.
 * @returns: 0 on success, non-zero otherwise
 */
int sde_cp_crtc_check_properties(struct drm_crtc *crtc,
					struct drm_crtc_state *state);

/**
 * sde_cp_crtc_apply_properties: Enable/disable properties
 *                               for a crtc.
 *                               Should be called during atomic commit call.
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_apply_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_get_property: Get value of color processing property
 *                                      for a crtc.
 *                                      Should be during atomic get property.
 * @crtc: Pointer to crtc.
 * @property: Property that needs to enabled/disabled.
 * @val: Value of property.
 *
 */
int sde_cp_crtc_get_property(struct drm_crtc *crtc,
				struct drm_property *property, uint64_t *val);

/**
 * sde_cp_crtc_suspend: Suspend the crtc features
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_suspend(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_resume: Resume the crtc features
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_resume(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_clear: Clear the active list and dirty list of crtc features
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_clear(struct drm_crtc *crtc);

/**
 * sde_cp_ad_interrupt: Api to enable/disable ad interrupt
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable interrupt.
 * @irq: Pointer to irq callback
 */
int sde_cp_ad_interrupt(struct drm_crtc *crtc, bool en,
		struct sde_irq_callback *irq);

/**
 * sde_cp_crtc_pre_ipc: Handle color processing features
 *                      before entering IPC
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_pre_ipc(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_post_ipc: Handle color processing features
 *                       after exiting IPC
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_post_ipc(struct drm_crtc *crtc);

/**
 * sde_cp_hist_interrupt: Api to enable/disable histogram interrupt
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable interrupt.
 * @irq: Pointer to irq callback
 */
int sde_cp_hist_interrupt(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq);

/**
 * sde_cp_ltm_hist_interrupt: API to enable/disable LTM hist interrupt
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable interrupt.
 * @irq: Pointer to irq callback
 */
int sde_cp_ltm_hist_interrupt(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq);

/**
 * sde_cp_ltm_wb_pb_interrupt: API to enable/disable LTM wb_pb interrupt
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable interrupt.
 * @irq: Pointer to irq callback
 */
int sde_cp_ltm_wb_pb_interrupt(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq);

/**
 * sde_cp_ltm_off_event_handler: API to enable/disable LTM off notification
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable notification.
 * @irq: Pointer to irq callback
 */
int sde_cp_ltm_off_event_handler(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq);

/**
 * sde_cp_crtc_res_change: API to handle LM resolution changes
 * @crtc_drm: Pointer to crtc.
 */
void sde_cp_crtc_res_change(struct drm_crtc *crtc_drm);

/**
 * sde_cp_crtc_vm_primary_handoff: Properly handoff CRTC color mode features
 * when switching from primary VM to trusted VM
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_vm_primary_handoff(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_enable(): enable color processing info in the crtc.
 *                     Should be called during crtc enable.
 * @crtc:  Pointer to drm_crtc.
 */
void sde_cp_crtc_enable(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_disable(): disable color processing info in the crtc.
 *                     Should be called during crtc disable.
 * @crtc:  Pointer to drm_crtc.
 */
void sde_cp_crtc_disable(struct drm_crtc *crtc);

/**
 * sde_cp_clear_state_info(): clear color processing info in the state.
 * @state:  Pointer to drm_crtc_state.
 */
void sde_cp_clear_state_info(struct drm_crtc_state *state);

/**
 * sde_cp_duplicate_state_info(): duplicate the cp state information.
 * Function should be called only from sde_crtc_duplicate_state.
 * @old_state:  Pointer to old drm_crtc_state.
 * @state: Pointer to current drm_crtc_state.
 */
void sde_cp_duplicate_state_info(struct drm_crtc_state *old_state,
				struct drm_crtc_state *state);
/**
 * sde_cp_disable_features(): disable cp features
 * @crtc: Pointer to drm_crtc.
 */
void sde_cp_disable_features(struct drm_crtc *crtc);
/**
 * sde_cp_set_skip_blend_plane_info(): set/clear the skip blend plane
 * @crtc: Pointer to drm_crtc.
 * @skip_blend: Pointer to sde_cp_crtc_skip_blend_plane
 */

void sde_cp_set_skip_blend_plane_info(struct drm_crtc *crtc,
		struct sde_cp_crtc_skip_blend_plane *skip_blend);

#endif /*_SDE_COLOR_PROCESSING_H */

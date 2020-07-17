/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _SDE_AD4_H_
#define _SDE_AD4_H_

#include <drm/drm_mode.h>
#include <drm/drm_property.h>
#include "sde_hw_dspp.h"

/**
 * enum ad4_modes - ad4 modes supported by driver
 */
enum ad4_modes {
	AD4_OFF,
	AD4_AUTO_STRENGTH,
	AD4_CALIBRATION,
	AD4_MANUAL,
};

/**
 * struct drm_prop_enum_list - drm structure for creating enum property and
 *                             enumerating values
 */
static const struct drm_prop_enum_list ad4_modes[] = {
	{AD4_OFF, "off"},
	{AD4_AUTO_STRENGTH, "auto_strength_mode"},
	{AD4_CALIBRATION, "calibration_mode"},
	{AD4_MANUAL, "manual_mode"},
};

/**
 * enum ad_property - properties that can be set for ad
 */
enum ad_property {
	AD_MODE,
	AD_INIT,
	AD_CFG,
	AD_INPUT,
	AD_SUSPEND,
	AD_ASSERTIVE,
	AD_BACKLIGHT,
	AD_STRENGTH,
	AD_ROI,
	AD_IPC_SUSPEND,
	AD_IPC_RESUME,
	AD_IPC_RESET,
	AD_PROPMAX,
};

/**
 * enum ad_intr_resp_property - ad4 interrupt response enum
 */
enum ad_intr_resp_property {
	AD4_IN_OUT_BACKLIGHT,
	AD4_RESPMAX,
};

/**
 * struct sde_ad_hw_cfg - structure for setting the ad properties
 * @prop: enum of ad property
 * @hw_cfg: payload for the prop being set.
 */
struct sde_ad_hw_cfg {
	enum ad_property prop;
	struct sde_hw_cp_cfg *hw_cfg;
};

/**
 * sde_validate_dspp_ad4() - api to validate if ad property is allowed for
 *                           the display with allocated dspp/mixers.
 * @dspp: pointer to dspp info structure.
 * @prop: pointer to u32 pointing to ad property
 */
int sde_validate_dspp_ad4(struct sde_hw_dspp *dspp, u32 *prop);

/**
 * sde_setup_dspp_ad4 - api to apply the ad property, sde_validate_dspp_ad4
 *                      should be called before call this function
 * @dspp: pointer to dspp info structure.
 * @cfg: pointer to struct sde_ad_hw_cfg
 */
void sde_setup_dspp_ad4(struct sde_hw_dspp *dspp, void *cfg);

/**
 * sde_read_intr_resp_ad4 - api to get ad4 interrupt status for event
 * @dspp: pointer to dspp object
 * @event: event for which response is needed
 * @resp_in: read ad4 input value of event requested
 * @resp_out: read ad4 output value of event requested
 */
void sde_read_intr_resp_ad4(struct sde_hw_dspp *dspp, u32 event,
			u32 *resp_in, u32 *resp_out);

#endif /* _SDE_AD4_H_ */

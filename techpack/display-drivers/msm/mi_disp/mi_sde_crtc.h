/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_SDE_CRTC_H_
#define _MI_SDE_CRTC_H_

#include "sde_crtc.h"

void mi_sde_crtc_update_layer_state(struct sde_crtc_state *cstate);

void mi_sde_crtc_install_properties(struct msm_property_info *property_info);

#endif /* _MI_SDE_CRTC_H_ */


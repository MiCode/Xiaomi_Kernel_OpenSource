/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _EDRM_KMS_H_
#define _EDRM_KMS_H_

#include <drm/drmP.h>
#include "msm_kms.h"

#define MAX_CTRLS_PER_DISPLAY 2

struct msm_edrm_display {
	int display_id;
	int ctl_id;
	int intf_id;
	int encoder_type;
	int connector_type;
	struct drm_display_mode mode;
	int ctl_off;
	int lm_off;
	int plane_cnt;
};

struct msm_edrm_kms {
	struct msm_kms base;
	struct drm_device *dev;
	struct drm_device *master_dev;
	struct msm_gem_address_space *aspace;

	struct msm_edrm_display display[MAX_ENCODERS];
	int display_count;

	int plane_id[MAX_PLANES];
	int plane_count;

	/* when this flag is set, the next lastclose() will trigger
	 * handoff eDRM resource to main kernel.
	 */
	bool handoff_flag;
	bool lk_running_flag;
};

struct msm_kms *msm_edrm_kms_init(struct drm_device *dev);

#define to_edrm_kms(x) container_of(x, struct msm_edrm_kms, base)

#endif /* _EDRM_KMS_H_ */

/**
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
 */
#ifndef EDRM_SPLASH_H_
#define EDRM_SPLASH_H_

#define SPLASH_STATUS_NOT_START 0
#define SPLASH_STATUS_RUNNING 1
#define SPLASH_STATUS_STOP    2

/* APIs for early splash handoff functions */

/**
 * edrm_splash_get_lk_status
 *
 * Get early display status to set the status flag.
 */
int edrm_splash_get_lk_status(struct msm_kms *kms);

/**
 * edrm_display_acquire
 *
 * Update main DRM that eDRM is active and eDRM display resource is being used.
 */
void edrm_display_acquire(struct msm_kms *kms);

/**
 * edrm_splash_get_lk_status
 *
 * Update main DRM that eDRM is active and eDRM display resource no longer
 * being use.  Main DRM can claim back the resource anytime.
 */
void edrm_display_release(struct msm_kms *kms);

#endif

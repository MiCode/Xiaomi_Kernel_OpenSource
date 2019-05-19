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
#include <linux/of_address.h>
#include <linux/debugfs.h>
#include <linux/memblock.h>
#include <soc/qcom/early_domain.h>
#include "msm_drv.h"
#include "sde_kms.h"
#include "edrm_kms.h"
#include "sde_splash.h"
#include "edrm_splash.h"

/* scratch registers */
#define SCRATCH_REGISTER_0		0x014
#define SCRATCH_REGISTER_1		0x018
#define SCRATCH_REGISTER_2		0x01C
#define SCRATCH_REGISTER_3		0x020

#define SDE_RUNNING_VALUE			0xC001CAFE
#define SDE_LK_STOP_VALUE	0xDEADDEAD
#define SDE_EXIT_VALUE		0xDEADBEEF
#define SDE_LK_IMMEDIATE_STOP_VALUE	0xFEFEFEFE

/*
 * Below function will indicate early display exited or not started.
 */
int edrm_splash_get_lk_status(struct msm_kms *kms)
{
	if (get_early_service_status(EARLY_DISPLAY))
		return SPLASH_STATUS_RUNNING;
	else
		return SPLASH_STATUS_NOT_START;
}


/*
 * Below function will indicate early display started.
 */
void edrm_display_acquire(struct msm_kms *kms)
{
	struct msm_edrm_kms *edrm_kms = to_edrm_kms(kms);
	struct sde_kms *master_kms;
	struct sde_splash_info *master_sinfo;
	struct msm_drm_private *master_priv =
			edrm_kms->master_dev->dev_private;

	master_kms = to_sde_kms(master_priv->kms);
	master_sinfo = &master_kms->splash_info;
	master_sinfo->early_display_enabled = true;
}

/*
 * Below function will indicate early display exited or not started.
 */
void edrm_display_release(struct msm_kms *kms)
{
	struct msm_edrm_kms *edrm_kms = to_edrm_kms(kms);
	struct sde_kms *master_kms;
	struct sde_splash_info *master_sinfo;
	struct msm_drm_private *master_priv =
			edrm_kms->master_dev->dev_private;

	master_kms = to_sde_kms(master_priv->kms);
	master_sinfo = &master_kms->splash_info;
	master_sinfo->early_display_enabled = false;
}

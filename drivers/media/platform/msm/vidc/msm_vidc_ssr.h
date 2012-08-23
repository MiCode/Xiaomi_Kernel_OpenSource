/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_VIDC_SSR__
#define __MSM_VIDC_SSR__

#include <../ramdump.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "msm_vidc_debug.h"
#include "vidc_hal_api.h"
int msm_vidc_ssr_init(struct msm_vidc_core *core);
int msm_vidc_ssr_uninit(struct msm_vidc_core *core);

#endif

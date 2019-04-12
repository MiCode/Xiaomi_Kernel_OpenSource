/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * CVP driver functions shared with video driver.
 */

#ifndef _MSM_CVP_VIDC_H_
#define _MSM_CVP_VIDC_H_
#include <uapi/media/msm_cvp_private.h>

#ifdef CONFIG_MSM_CVP_V4L2
void *msm_cvp_open(int core_id, int session_type);
int msm_cvp_close(void *instance);
int msm_cvp_private(void *cvp_inst, unsigned int cmd, struct cvp_kmd_arg *arg);
#else
static inline void *msm_cvp_open(int core_id, int session_type)
{
	return NULL;
}
static inline int msm_cvp_close(void *instance)
{
	return -EINVAL;
}
static inline int msm_cvp_private(void *cvp_inst, unsigned int cmd,
		struct cvp_kmd_arg *arg)
{
	return -EINVAL;
}
#endif

#endif


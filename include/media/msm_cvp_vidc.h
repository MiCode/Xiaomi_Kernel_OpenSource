/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * CVP driver functions shared with video driver.
 */

#ifndef _MSM_CVP_VIDC_H_
#define _MSM_CVP_VIDC_H_
#include <uapi/media/msm_cvp_private.h>

/**
 * struct cvp_kmd_usecase_desc - store generic usecase
 *                              description
 * @fullres_width:  process width of full resolution frame
 * @fullres_height:   process height of full resolution frame
 * @downscale_width:   width of downscaled frame
 * @downscale_height:   height of downscaled frame
 * @is_downscale:   is downscaling enabled in pipeline
 * @fps:   frame rate
 * @op_rate:   stream operation rate
 * @colorfmt:   format based on msm_media_info.h
 * @reserved[16]: for future use
 */
struct cvp_kmd_usecase_desc {
	unsigned int fullres_width;
	unsigned int fullres_height;
	unsigned int downscale_width;
	unsigned int downscale_height;
	unsigned int is_downscale;
	unsigned int fps;
	unsigned int op_rate;
	unsigned int colorfmt;
	int reserved[16];
};

#define VIDEO_NONREALTIME 1
#define VIDEO_REALTIME 5

#if IS_ENABLED(CONFIG_MSM_CVP)
void *msm_cvp_open(int core_id, int session_type);
int msm_cvp_close(void *instance);
int msm_cvp_private(void *cvp_inst, unsigned int cmd, struct cvp_kmd_arg *arg);
int msm_cvp_est_cycles(struct cvp_kmd_usecase_desc *cvp_desc,
	struct cvp_kmd_request_power *cvp_voting);

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
static inline int msm_cvp_est_cycles(struct cvp_kmd_usecase_desc *cvp_desc,
	struct cvp_kmd_request_power *cvp_voting)
{
	return -EINVAL;
}
#endif

#endif


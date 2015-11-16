/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_DCVS_H_
#define _MSM_VIDC_DCVS_H_
#include "msm_vidc_internal.h"

/* Low threshold for encoder dcvs */
#define DCVS_ENC_LOW_THR 4
/* High threshold for encoder dcvs */
#define DCVS_ENC_HIGH_THR 9
/* extra o/p buffers in case of encoder dcvs */
#define DCVS_ENC_EXTRA_OUTPUT_BUFFERS 2
/* Default threshold to reduce the core frequency */
#define DCVS_NOMINAL_THRESHOLD 8
/* Default threshold to increase the core frequency */
#define DCVS_TURBO_THRESHOLD 4

#define DCVS_SVS_LOAD NUM_MBS_PER_SEC(1088, 1920, 30)
#define DCVS_NOMINAL_LOAD NUM_MBS_PER_SEC(1088, 1920, 60)

#define DIM_1080P30 NUM_MBS_PER_SEC(1088, 1920, 30)
#define DIM_2K30 NUM_MBS_PER_SEC(1440, 2560, 30)
#define DIM_1080P60 NUM_MBS_PER_SEC(1088, 1920, 60)

/* Considering one safeguard buffer */
#define DCVS_BUFFER_SAFEGUARD 1
/* Supported DCVS MBs per frame */
#define DCVS_MIN_SUPPORTED_MBPERFRAME NUM_MBS_PER_FRAME(2160, 3840)
#define DCVS_2K_MBPERFRAME NUM_MBS_PER_FRAME(1440, 2560)

void msm_dcvs_init(struct msm_vidc_inst *inst);
void msm_dcvs_init_load(struct msm_vidc_inst *inst);
bool msm_dcvs_enc_check(struct msm_vidc_inst *inst);
void msm_dcvs_monitor_buffer(struct msm_vidc_inst *inst);
void msm_dcvs_check_and_scale_clocks(struct msm_vidc_inst *inst, bool is_etb);
int  msm_dcvs_get_extra_buff_count(struct msm_vidc_inst *inst);
void msm_dcvs_set_buff_req_handled(struct msm_vidc_inst *inst);
void msm_dcvs_enc_set_power_save_mode(struct msm_vidc_inst *inst,
		bool is_power_save_mode);
#endif

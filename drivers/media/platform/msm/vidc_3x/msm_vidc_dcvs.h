/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2015, 2018 The Linux Foundation. All rights reserved.
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

#ifndef _MSM_VIDC_DCVS_H_
#define _MSM_VIDC_DCVS_H_
#include "msm_vidc_internal.h"

/* Low threshold for encoder dcvs */
#define DCVS_ENC_LOW_THR 4
/* High threshold for encoder dcvs */
#define DCVS_ENC_HIGH_THR 9
/* extra o/p buffers in case of encoder dcvs */
#define DCVS_ENC_EXTRA_OUTPUT_BUFFERS 2
/* extra o/p buffers in case of decoder dcvs */
#define DCVS_DEC_EXTRA_OUTPUT_BUFFERS 4
/* Default threshold to reduce the core frequency */
#define DCVS_NOMINAL_THRESHOLD 8
/* Default threshold to increase the core frequency */
#define DCVS_TURBO_THRESHOLD 4

/* Considering one safeguard buffer */
#define DCVS_BUFFER_SAFEGUARD (DCVS_DEC_EXTRA_OUTPUT_BUFFERS - 1)

void msm_dcvs_init(struct msm_vidc_inst *inst);
void msm_dcvs_init_load(struct msm_vidc_inst *inst);
void msm_dcvs_monitor_buffer(struct msm_vidc_inst *inst);
void msm_dcvs_check_and_scale_clocks(struct msm_vidc_inst *inst, bool is_etb);
int  msm_dcvs_get_extra_buff_count(struct msm_vidc_inst *inst);
void msm_dcvs_enc_set_power_save_mode(struct msm_vidc_inst *inst,
		bool is_power_save_mode);
#endif

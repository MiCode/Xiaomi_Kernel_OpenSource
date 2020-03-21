/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_CVP_CORE_H_
#define _MSM_CVP_CORE_H_

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include <linux/refcount.h>
#include <media/msm_cvp_private.h>
#include <media/msm_cvp_vidc.h>
#include "msm_cvp_buf.h"
#include "msm_cvp_synx.h"

enum core_id {
	MSM_CORE_CVP = 0,
	MSM_CVP_CORES_MAX,
};

enum session_type {
	MSM_CVP_USER = 1,
	MSM_CVP_KERNEL,
	MSM_CVP_BOOT,
	MSM_CVP_UNKNOWN,
	MSM_CVP_MAX_DEVICES = MSM_CVP_UNKNOWN,
};

void *msm_cvp_open(int core_id, int session_type);
int msm_cvp_close(void *instance);
int msm_cvp_suspend(int core_id);
int msm_cvp_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
int msm_cvp_private(void *cvp_inst, unsigned int cmd,
		struct cvp_kmd_arg *arg);
int msm_cvp_est_cycles(struct cvp_kmd_usecase_desc *cvp_desc,
		struct cvp_kmd_request_power *cvp_voting);

#endif

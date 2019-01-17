/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_V4L2_PRIVATE_H_
#define _MSM_V4L2_PRIVATE_H_

#include <media/msm_cvp_private.h>
#include "msm_cvp_debug.h"

long msm_cvp_v4l2_private(struct file *file,
		unsigned int cmd, unsigned long arg);

#endif

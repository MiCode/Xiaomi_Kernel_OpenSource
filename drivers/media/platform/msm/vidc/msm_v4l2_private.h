/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _MSM_V4L2_PRIVATE_H_
#define _MSM_V4L2_PRIVATE_H_

#include <media/msm_vidc_private.h>
#include "msm_vidc_debug.h"

long msm_v4l2_private(struct file *file, unsigned int cmd, unsigned long arg);

#endif

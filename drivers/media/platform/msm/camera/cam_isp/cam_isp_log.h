/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_ISP_LOG_H_
#define _CAM_ISP_LOG_H_

#include <linux/kernel.h>

#define ISP_TRACE_ENABLE			1

#if (ISP_TRACE_ENABLE == 1)
	#define ISP_TRACE(args...)		trace_printk(args)
#else
	#define ISP_TRACE(arg...)
#endif

#endif /* __CAM_ISP_LOG_H__ */

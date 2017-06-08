/* Copyright (c) 2017, The Linux Foundataion. All rights reserved.
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

#ifndef _CAM_DEBUG_UTIL_H_
#define _CAM_DEBUG_UTIL_H_

#define DEFAULT     0xFFFF
#define CAM_CDM    (1 << 0)
#define CAM_CORE   (1 << 1)
#define CAM_CPAS   (1 << 2)
#define CAM_ISP    (1 << 3)
#define CAM_CRM    (1 << 4)
#define CAM_SENSOR (1 << 5)
#define CAM_SMMU   (1 << 6)
#define CAM_SYNC   (1 << 7)
#define CAM_ICP    (1 << 8)
#define CAM_JPEG   (1 << 9)
#define CAM_FD     (1 << 10)
#define CAM_LRME   (1 << 11)

#define GROUP      DEFAULT
#define TRACE_ON   0

#define CAM_ERR(__module, fmt, args...)                                      \
	do { if (GROUP & __module) {                                         \
		if (TRACE_ON)                                                \
			trace_printk(fmt, ##args);                           \
		else                                                         \
			pr_err(fmt, ##args);                                 \
	} } while (0)

#define CAM_WARN(__module, fmt, args...)                                     \
	do { if (GROUP & __module) {                                         \
		if (TRACE_ON)                                                \
			trace_printk(fmt, ##args);                           \
		else                                                         \
			pr_warn(fmt, ##args);                                \
	} } while (0)

#define CAM_INFO(__module, fmt, args...)                                     \
	do { if (GROUP & __module) {                                         \
		if (TRACE_ON)                                                \
			trace_printk(fmt, ##args);                           \
		else                                                         \
			pr_info(fmt, ##args);                                \
	} } while (0)

#endif /* _CAM_DEBUG_UTIL_H_ */

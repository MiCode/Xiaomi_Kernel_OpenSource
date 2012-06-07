/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#ifndef __ARCH_ARM_MACH_MSM_DEBUG_MM_H_
#define __ARCH_ARM_MACH_MSM_DEBUG_MM_H_

#include <linux/string.h>

/* The below macro removes the directory path name and retains only the
 * file name to avoid long path names in log messages that comes as
 * part of __FILE__ to compiler.
 */
#define __MM_FILE__ strrchr(__FILE__, '/') ? (strrchr(__FILE__, '/')+1) : \
	__FILE__

#define MM_DBG(fmt, args...) pr_debug("[%s] " fmt,\
		__func__, ##args)

#define MM_INFO(fmt, args...) pr_info("[%s:%s] " fmt,\
	       __MM_FILE__, __func__, ##args)

#define MM_ERR(fmt, args...) pr_err("[%s:%s] " fmt,\
	       __MM_FILE__, __func__, ##args)
#endif /* __ARCH_ARM_MACH_MSM_DEBUG_MM_H_ */

/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#ifndef __MALI_KERNEL_LINUX_H__
#define __MALI_KERNEL_LINUX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/cdev.h>     /* character device definitions */
#include <linux/idr.h>
#include <linux/rbtree.h>
#include "mali_kernel_license.h"
#include "mali_osk_types.h"
#include <linux/version.h>

extern struct platform_device *mali_platform_device;

/* After 3.19.0 kenrel droped CONFIG_PM_RUNTIME define,define by ourself */
#if defined(CONFIG_PM) && LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define CONFIG_PM_RUNTIME 1
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MALI_KERNEL_LINUX_H__ */

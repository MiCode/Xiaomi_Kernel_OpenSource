/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __PLATFORM_SUPPORT_H_INCLUDED__
#define __PLATFORM_SUPPORT_H_INCLUDED__

/**
* @file
* Platform specific includes and functionality.
*/

#if defined(_MSC_VER)
/*
 * Put here everything _MSC_VER specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#include "hrt/defs.h"
#include "storage_class.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

STORAGE_CLASS_INLINE void
hrt_sleep(void)
{
	/* Empty for now. Polling is not used in many places */
}

/* Ignore warning 4505: Unreferenced local function has been removed    *
 * Ignore warning 4324: structure was padded due to __declspec(align()) */
#pragma warning(disable : 4505 4324)

#define CSS_ALIGN(d, a) _declspec(align(a)) d
#define inline      __inline
#define __func__    __FUNCTION__

#define snprintf(buffer, size, ...) \
	_snprintf_s(buffer, size, size, __VA_ARGS__)

#elif defined(__HIVECC)
/*
 * Put here everything __HIVECC specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#include <string.h>
#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))

#elif defined(__KERNEL__)
#include "storage_class.h"
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/string.h>

/* For definition of hrt_sleep() */
#include <hrt/hive_isp_css_custom_host_hrt.h>

#define UINT16_MAX USHRT_MAX
#define UINT32_MAX UINT_MAX
#define UCHAR_MAX  (255)

#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))

/*
 * Put here everything __KERNEL__ specific not covered in
 * "assert_support.h", "math_support.h", etc
 */

#elif defined(__GNUC__)
/*
 * Put here everything __GNUC__ specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#include "hrt/host.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define CSS_ALIGN(d, a) d __attribute__((aligned(a)))

#else /* default is for the FIST environment */
/*
 * Put here everything FIST specific not covered in
 * "assert_support.h", "math_support.h", etc
 */
#include <string.h>
#endif

#endif /* __PLATFORM_SUPPORT_H_INCLUDED__ */

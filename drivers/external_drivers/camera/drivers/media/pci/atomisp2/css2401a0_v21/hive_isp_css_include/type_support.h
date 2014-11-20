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

#ifndef __TYPE_SUPPORT_H_INCLUDED__
#define __TYPE_SUPPORT_H_INCLUDED__

/**
* @file
* Platform specific types.
*
* Per the DLI spec, types are in "type_support.h" and
* "platform_support.h" is for unclassified/to be refactored
* platform specific definitions.
*/

#define IA_CSS_UINT8_T_BITS						8
#define IA_CSS_UINT16_T_BITS					16
#define IA_CSS_UINT32_T_BITS					32
#define IA_CSS_INT32_T_BITS						32
#define IA_CSS_UINT64_T_BITS					64

#if defined(_MSC_VER)
#include <stdint.h>
/* For ATE compilation define the bool */
#if defined(_ATE_)
#define bool int
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#if defined(_M_X64)
#define HOST_ADDRESS(x) (unsigned long long)(x)
#else
#define HOST_ADDRESS(x) (unsigned long)(x)
#endif

#elif defined(__HIVECC)
#define __INDIRECT_STDINT_INCLUDE
#include <stdint/stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#elif defined(__KERNEL__)

#define CHAR_BIT (8)

#include <linux/types.h>
#include <linux/limits.h>
#include <linux/errno.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#elif defined(__GNUC__)
#define __STDC_LIMIT_MACROS 1
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#else /* default is for the FIST environment */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#endif

#endif /* __TYPE_SUPPORT_H_INCLUDED__ */

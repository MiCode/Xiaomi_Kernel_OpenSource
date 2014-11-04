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

#ifndef __ERROR_SUPPORT_H_INCLUDED__
#define __ERROR_SUPPORT_H_INCLUDED__

#if defined(_MSC_VER)
#include <errno.h>
/*
 * Put here everything _MSC_VER specific not covered in
 * "errno.h"
 */
#define EINVAL  22
#define EBADE   52
#define ENODATA 61
#define ENOTCONN 107
#define ENOTSUP 252
#define ENOBUFS 233


#elif defined(__HIVECC)
#include <errno.h>
/*
 * Put here everything __HIVECC specific not covered in
 * "errno.h"
 */

#elif defined(__KERNEL__)
#include <linux/errno.h>
/*
 * Put here everything __KERNEL__ specific not covered in
 * "errno.h"
 */
#define ENOTSUP 252

#elif defined(__GNUC__)
#include <errno.h>
/*
 * Put here everything __GNUC__ specific not covered in
 * "errno.h"
 */

#else /* default is for the FIST environment */
#include <errno.h>
/*
 * Put here everything FIST specific not covered in
 * "errno.h"
 */

#endif

#define verifexit(cond,error_tag)  \
do {                               \
	if (!(cond)){              \
		goto EXIT;         \
	}                          \
} while(0)

#define verifjmpexit(cond)         \
do {                               \
	if (!(cond)){              \
		goto EXIT;         \
	}                          \
} while(0)

#endif /* __ERROR_SUPPORT_H_INCLUDED__ */

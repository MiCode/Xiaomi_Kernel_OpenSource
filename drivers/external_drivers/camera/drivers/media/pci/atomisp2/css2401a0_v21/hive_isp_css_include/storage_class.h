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

#ifndef __STORAGE_CLASS_H_INCLUDED__
#define __STORAGE_CLASS_H_INCLUDED__

/**
* @file
* Platform specific includes and functionality.
*/

#define STORAGE_CLASS_EXTERN extern

#if defined(_MSC_VER)
#define STORAGE_CLASS_INLINE static __inline
#elif defined(__HIVECC)
#define STORAGE_CLASS_INLINE static inline
#else
#define STORAGE_CLASS_INLINE static inline
#endif

#define STORAGE_CLASS_EXTERN_DATA extern const
#define STORAGE_CLASS_INLINE_DATA static const

#endif /* __STORAGE_CLASS_H_INCLUDED__ */

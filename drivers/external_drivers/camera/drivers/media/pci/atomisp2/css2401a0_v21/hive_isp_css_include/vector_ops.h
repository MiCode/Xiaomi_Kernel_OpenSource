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

#ifndef __VECTOR_OPS_H_INCLUDED__
#define __VECTOR_OPS_H_INCLUDED__

#include "storage_class.h"

#include "vector_ops_local.h"

#ifndef __INLINE_VECTOR_OPS__
#define STORAGE_CLASS_VECTOR_OPS_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_VECTOR_OPS_C 
#include "vector_ops_public.h"
#else  /* __INLINE_VECTOR_OPS__ */
#define STORAGE_CLASS_VECTOR_OPS_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_VECTOR_OPS_C STORAGE_CLASS_INLINE
#include "vector_ops_private.h"
#endif /* __INLINE_VECTOR_OPS__ */

#endif /* __VECTOR_OPS_H_INCLUDED__ */

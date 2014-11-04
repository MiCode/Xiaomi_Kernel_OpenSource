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

#ifndef __CPU_MEM_SUPPORT_H_INCLUDED__
#define __CPU_MEM_SUPPORT_H_INCLUDED__

#if defined (__KERNEL__)
#include <linux/string.h> /* memset */
#else
#include <string.h> /* memset */
#endif

#include "sh_css_internal.h" /* sh_css_malloc and sh_css_free */

static inline void*
ia_css_cpu_mem_alloc(unsigned int size)
{
	return sh_css_malloc(size);
}

static inline void*
ia_css_cpu_mem_copy(void* dst, const void* src, unsigned int size)
{
	if(!src || !dst)
		return NULL;

	return memcpy(dst, src, size);
}

static inline void*
ia_css_cpu_mem_set_zero(void* dst, unsigned int size)
{
	if(!dst)
		return NULL;

	return memset(dst, 0, size);
}

static inline void
ia_css_cpu_mem_free(void* ptr)
{
	if(!ptr)
		return;

	sh_css_free(ptr);
}

#endif /* __CPU_MEM_SUPPORT_H_INCLUDED__ */

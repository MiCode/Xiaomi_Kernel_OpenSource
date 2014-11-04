/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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

#ifndef _SH_CSS_REFCOUNT_H_
#define _SH_CSS_REFCOUNT_H_

#include "sh_css.h"
#include "sh_css_binary.h"
#include "sh_css_internal.h"

#define PARAM_SET_POOL  ((int32_t)0xCAFE0001)
#define PARAM_BUFFER    ((int32_t)0xCAFE0002)
#define FREE_BUF_CACHE  ((int32_t)0xCAFE0003)

enum sh_css_err sh_css_refcount_init(void);

void sh_css_refcount_uninit(void);

hrt_vaddress sh_css_refcount_alloc(
	int32_t id, const size_t size, const uint16_t attribute);

hrt_vaddress sh_css_refcount_retain(int32_t id, hrt_vaddress ptr);

bool sh_css_refcount_release(int32_t id, hrt_vaddress ptr);

bool sh_css_refcount_is_single(hrt_vaddress ptr);

int32_t sh_css_refcount_get_id(hrt_vaddress ptr);

void sh_css_refcount_clear(
	int32_t id, void (*clear_func)(hrt_vaddress ptr));

int sh_css_refcount_used(void);

#endif

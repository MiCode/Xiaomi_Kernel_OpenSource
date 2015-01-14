/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
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

#ifndef __ISP_CONFIG_H_INCLUDED__
#define __ISP_CONFIG_H_INCLUDED__

#ifdef ISP2600
#include "isp2600_config.h"
#elif defined(ISP2601)
#include "isp2601_config.h"
#elif defined(ISP2500)
#include "isp2500_config.h"
#elif defined(ISP2400) || defined(ISP2401)
#include "isp2400_config.h"
#else
#error "Please define a core {ISP2400, ISP2401, ISP2500, ISP2600, ISP2601}"
#endif

#endif /* __ISP_CONFIG_H_INCLUDED__ */

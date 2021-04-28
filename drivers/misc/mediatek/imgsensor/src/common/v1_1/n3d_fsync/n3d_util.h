/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __N3D_UTIL_H__
#define __N3D_UTIL_H__

#define PFX "SeninfN3D"

#define LOG_D(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_W(format, args...) pr_warn(PFX "[%s] " format, __func__, ##args)
#define LOG_E(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)

#endif


// SPDX-License-Identifier: GPL-2.0-only
/*
 * common.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __DT_BINDINGS_DEBUG_COMMON_H__
#define __DT_BINDINGS_DEBUG_COMMON_H__

#define ENABLE		1
#define DISABLE		0
#define MON_WRITE	1
#define MON_READ	2
#define MON_WRITEREAD	(MON_WRITE | MON_READ)
#define MON_OUTSIDE	0
#define MON_INSIDE	1

#define USERID		1
#define MPUID		0

#endif
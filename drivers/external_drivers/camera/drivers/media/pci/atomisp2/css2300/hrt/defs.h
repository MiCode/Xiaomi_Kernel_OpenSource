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

#ifndef _HRT_DEFS_H_
#define _HRT_DEFS_H_

#ifndef HRTCAT
#define _HRTCAT(m, n)     m##n
#define HRTCAT(m, n)      _HRTCAT(m, n)
#endif

#ifndef HRTSTR
#define _HRTSTR(x)   #x
#define HRTSTR(x)    _HRTSTR(x)
#endif

#ifndef HRTMIN
#define HRTMIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef HRTMAX
#define HRTMAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#endif /* _HRT_DEFS_H_ */

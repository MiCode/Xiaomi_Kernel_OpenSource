/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_QOS_PREFETCH_COMMON_H__
#define __MTK_QOS_PREFETCH_COMMON_H__

#ifdef QOS_PREFETCH_SUPPORT
extern void qos_prefetch_tick(int cpu);
#else
__weak void qos_prefetch_tick(int cpu) {}
#endif

#endif

/* Copyright (c) 2010-2011, 2013 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

/* The MSM Hardware supports multiple flavors of physical memory.
 * This file captures hardware specific information of these types.
*/

#ifndef __ASM_ARCH_MSM_MEMTYPES_H
#define __ASM_ARCH_MSM_MEMTYPES_H

#include <mach/memory.h>
#include <linux/init.h>

int __init dt_scan_for_memory_reserve(unsigned long node, const char *uname,
					int depth, void *data);
int __init dt_scan_for_memory_hole(unsigned long node, const char *uname,
					int depth, void *data);
void adjust_meminfo(unsigned long start, unsigned long size);
#endif

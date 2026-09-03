// SPDX-License-Identifier: GPL-2.0-only
/*
 * sysdump.h - Unisoc platform header
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

#ifndef __SPRD_PLATFORM_SYSDUMP_H
#define __SPRD_PLATFORM_SYSDUMP_H

/* the SPRD_SYSDUMP_MAGIC indicates the ramdisk addr,
 * the ramdisk add maybe different in different boards
 * just for backup.
 */
#ifndef CONFIG_X86_64
#define SPRD_SYSDUMP_MAGIC      0x85500000
#else
#define SPRD_SYSDUMP_MAGIC      0x3B800000
#endif

#define SPRD_SYSDUMP_RESERVED	"sysdumpinfo-mem"

struct sysdump_mem {
	unsigned long paddr;
	unsigned long vaddr;
	unsigned long soff;
	unsigned long size;
	unsigned long type;
};

enum sysdump_type {
	SYSDUMP_RAM,
	SYSDUMP_MODEM,
	SYSDUMP_IOMEM,
};

#if IS_ENABLED(CONFIG_UNISOC_SIPC)
void sysdump_callback_register(int (*callback)(u8 dst));
void sysdump_callback_unregister(void);
#endif

#ifdef CONFIG_ARM
#include "sysdump32.h"

/* refer to sprd_wdh.c */
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
		(void *)(kaddr) < (void *)high_memory && \
		pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

#ifdef CONFIG_ARM64
#include "sysdump64.h"

/* refer to sprd_wdh.c */
#define sprd_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET)
#endif

#ifdef CONFIG_X86_64
#include "sysdump_x86_64.h"
#endif

#endif

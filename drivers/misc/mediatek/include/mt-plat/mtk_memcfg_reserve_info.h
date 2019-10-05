/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_MEMCFG_RESERVE_INFO__
#define __MTK_MEMCFG_RESERVE_INFO__

#if defined(CONFIG_MTK_MEMCFG)
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/of_reserved_mem.h>

#define MAX_RESERVED_REGIONS	40
#define RESERVED_NOMAP 1
#define RESERVED_MAP 0

struct kernel_reserve_meminfo {
	unsigned long long total;
	unsigned long long available;
	unsigned long long kernel_code;
	unsigned long long rwdata;
	unsigned long long rodata;
	unsigned long long init;
	unsigned long long bss;
	unsigned long long reserved;
#ifdef CONFIG_HIGHMEM
	unsigned long long highmem;
#endif
};

extern struct kernel_reserve_meminfo kernel_reserve_meminfo;

struct reserved_mem_ext {
	const char	*name;
	unsigned long long base;
	unsigned long long size;
	int		nomap;
};

int __init mtk_memcfg_reserve_info_init(struct proc_dir_entry *mtk_memcfg_dir);
int memcfg_get_reserve_info(struct reserved_mem_ext *reserved_mem, int count);
int memcfg_remove_free_mem(struct reserved_mem_ext *reserved_mem, int count);
void clear_reserve(struct reserved_mem_ext *rmem, int count, const char *name);
int reserved_mem_ext_compare(const void *p1, const void *p2);
int freed_reserved_memory_compare(const void *p1, const void *p2);
void mtk_memcfg_record_freed_reserved(phys_addr_t start, phys_addr_t end);
#else

#define mtk_memcfg_record_freed_reserved(start, end) do {} while (0)

#endif

#endif /* end of __MTK_MEMCFG_RESERVE_INFO__ */

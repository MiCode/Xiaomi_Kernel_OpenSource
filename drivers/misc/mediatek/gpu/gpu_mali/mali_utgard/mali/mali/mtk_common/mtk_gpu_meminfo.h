/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef ENABLE_MTK_MEMINFO
#define MTK_MEMINFO_SIZE 50

typedef struct {
	int pid;
	int used_pages;
} mtk_gpu_meminfo_type;

void mtk_gpu_meminfo_init(void);
void mtk_gpu_meminfo_remove(void);
void mtk_gpu_meminfo_reset(void);
void mtk_gpu_meminfo_set(ssize_t index, int pid, int used_pages);

bool mtk_dump_mali_memory_usage(void);
unsigned int mtk_get_mali_memory_usage(void);
#endif /* ENABLE_MTK_MEMINFO */

/*
 * drivers/misc/tegra-profiler/mmap.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __QUADD_MMAP_H
#define __QUADD_MMAP_H

#include <linux/types.h>

void quadd_process_mmap(struct vm_area_struct *vma, pid_t pid);
int quadd_get_current_mmap(pid_t pid);

#endif  /* __QUADD_MMAP_H */

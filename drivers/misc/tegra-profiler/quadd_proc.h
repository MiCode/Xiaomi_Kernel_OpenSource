/*
 * drivers/misc/tegra-profiler/quadd_proc.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __QUADD_PROC_H
#define __QUADD_PROC_H

struct quadd_ctx;

#ifdef CONFIG_PROC_FS
void quadd_proc_init(struct quadd_ctx *context);
void quadd_proc_deinit(void);
#else
static inline void quadd_proc_init(struct quadd_ctx *context)
{
}
static inline void quadd_proc_deinit(void)
{
}
#endif

#endif  /* __QUADD_PROC_H */

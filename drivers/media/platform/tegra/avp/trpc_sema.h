/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *   Dima Zavin <dima@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARM_MACH_TEGRA_RPC_SEMA_H
#define __ARM_MACH_TEGRA_RPC_SEMA_H

#include <linux/types.h>
#include <linux/fs.h>

struct tegra_sema_info;

struct tegra_sema_info *trpc_sema_get_from_fd(int fd);
void trpc_sema_put(struct tegra_sema_info *sema);
int __init trpc_sema_init(void);

#endif

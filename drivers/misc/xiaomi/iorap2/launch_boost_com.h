/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MI_LB_COM_H_
#define _MI_LB_COM_H_

#include <linux/ctype.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>

extern struct lb_manager *g_manager;
extern atomic_t collecting_;

static inline void package_info_get(struct package_info *package)
{
	atomic_inc(&package->users);
}

static inline void *lb_zmalloc(size_t size, gfp_t flags)
{
    void *ptr;

    ptr = kzalloc(size, flags);

    return ptr;
}

static inline void lb_kfree(void *ptr, size_t size)
{
    if(ptr)
        kfree(ptr);
}

bool package_info_put(struct package_info *package);
int mi_lb_add_package_info(struct package_info *package);
struct package_info *mi_lb_get_package_info(struct lb_owner *owner, bool move_to_head);
bool mi_lb_is_in_package_list(struct package_info *package);
struct package_info *mi_lb_get_oldest_package_info(void);
#endif


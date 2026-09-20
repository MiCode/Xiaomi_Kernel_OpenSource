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

#ifndef _MI_LB_CLEAR_H_
#define _MI_LB_CLEAR_H_

#include <linux/ctype.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>

bool package_info_do_destroy(struct package_info *package);
int mi_lb_dealwith_clear_cmd(struct lb_owner *owner);
void free_one_fileinfo(struct package_info *package, struct file_info *file_entry);
#endif


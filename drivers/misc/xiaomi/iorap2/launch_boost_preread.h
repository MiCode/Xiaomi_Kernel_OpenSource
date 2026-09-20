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

#ifndef _MI_LB_PREREAD_H_
#define _MI_LB_PREREAD_H_

#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

int mi_lb_dealwith_preread_size_cmd(struct lb_app_preread_info_size *app_preread_info_size);
int mi_lb_dealwith_preread_info_cmd(struct lb_app_preread_info *app_preread_info);
#ifdef MI_LB_PREREAD
int mi_lb_init_preread(void);
int mi_lb_dealwith_preread_cmd(struct lb_owner *owner);
#endif
void dump_app_preread_info(struct lb_app_preread_info *app_preread_info);
#endif

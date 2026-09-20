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

#ifndef _MI_LB_FLUSH_H_
#define _MI_LB_FLUSH_H_

#include <linux/ctype.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>

int mi_lb_dealwith_get_app_names(struct lb_app_list *app_list);

#endif


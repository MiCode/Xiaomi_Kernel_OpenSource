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

#ifndef _MI_LB_DEBUG_H_
#define _MI_LB_DEBUG_H_

#include <linux/ctype.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/tracepoint.h>

#include <linux/string.h>
#include <linux/kernel.h>

#include  "mi_lb_def.h"

extern struct lb_owner debug_owner;
extern bool g_only_collect;
extern bool g_atrace_enable;
extern bool g_collect_specify_filetype;
extern bool g_collect_pinfile;
extern bool g_debug_verbose;
//#define LB_DEBUG_COLLECT 1

int mi_lb_dbg_init(void);
#endif
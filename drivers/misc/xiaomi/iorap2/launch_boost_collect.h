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

#ifndef _MI_LB_COLLECT_H_
#define _MI_LB_COLLECT_H_

#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

void mi_lb_page_collect_on_readfile(void *unused,
                                struct file *file, loff_t pos, size_t size);
void mi_lb_page_collect_on_pagefault(void *unused, struct file *file,
                                        pgoff_t first_pgoff,
                                        pgoff_t last_pgoff,
                                        vm_fault_t ret);

int mi_lb_dealwith_collect_cmd(enum APP_LAUNCH_CMD cmd, struct lb_owner *owner);

int mi_lb_init_manager(void);
int mi_lb_exit_manager(void);

#endif


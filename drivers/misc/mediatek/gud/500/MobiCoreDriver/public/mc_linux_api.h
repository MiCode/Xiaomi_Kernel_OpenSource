/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MC_LINUX_API_H_
#define _MC_LINUX_API_H_

#include <linux/types.h>

/*
 * Manage dynamically switch TEE worker threads/TEE affinity to big core only.
 * Or default core affinity.
 */
#if defined(BIG_CORE_SWITCH_AFFINITY_MASK)
void set_tee_worker_threads_on_big_core(bool big_core);
#endif

#endif /* _MC_LINUX_API_H_ */

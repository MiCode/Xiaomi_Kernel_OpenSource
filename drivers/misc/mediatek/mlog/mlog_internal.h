/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MLOG_INTERNAL_H
#define _MLOG_INTERNAL_H

#include <linux/printk.h>

#define MLOG_DEBUG

#ifdef MLOG_DEBUG
#define MLOG_PRINTK(args...)    pr_debug(args)
#else
#define MLOG_PRINTK(args...)    do { } while (0)
#endif

#endif

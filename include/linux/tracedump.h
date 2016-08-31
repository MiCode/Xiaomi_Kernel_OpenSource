/*
 * include/linux/tracedump.h
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _LINUX_KERNEL_TRACEDUMP_H
#define _LINUX_KERNEL_TRACEDUMP_H

/* tracedump
 * This module provides additional mechanisms for retreiving tracing data.
 * For details on configurations, parameters and usage, see tracedump.txt.
 */

#define TD_NO_PRINT 0
#define TD_PRINT_CONSOLE 1
#define TD_PRINT_USER 2

/* Dump the tracer to console */
int tracedump_dump(size_t max_out);

/* Dumping functions */
int tracedump_init(void);
ssize_t tracedump_all(int print_to);
ssize_t tracedump_next(size_t max_out, int print_to);
int tracedump_reset(void);
int tracedump_deinit(void);

#endif /* _LINUX_KERNEL_TRACEDUMP_H */

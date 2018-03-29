/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#if !defined(__MRDUMP_MINI_H__)
int mrdump_mini_init(void);
extern raw_spinlock_t logbuf_lock;
extern unsigned long *stack_trace;
extern void get_kernel_log_buffer(unsigned long *addr, unsigned long *size, unsigned long *start);
/* extern void get_android_log_buffer(unsigned long *addr, unsigned long *size, unsigned long *start, int type); */
extern struct ram_console_buffer *ram_console_buffer;
#endif

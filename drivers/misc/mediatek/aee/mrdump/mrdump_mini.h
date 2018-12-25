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
struct mrdump_mini_extra_misc {
	void (*dump_func)(unsigned long *, unsigned long *);
	const char *dump_name;
	unsigned long max_size;
};
#ifdef CONFIG_MODULES
static char modules_info_buf[MODULES_INFO_BUF_SIZE];
extern int save_modules(char *mbuf, int mbufsize);
#endif
int mrdump_mini_init(void);
extern raw_spinlock_t logbuf_lock;
extern unsigned long *stack_trace;
extern void get_kernel_log_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start);
extern void get_hang_detect_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start);
#if defined(CONFIG_TRUSTY_LOG)
extern void get_gz_log_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start);
#endif
extern struct ram_console_buffer *ram_console_buffer;
#endif

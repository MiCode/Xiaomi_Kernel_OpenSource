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

#ifndef __RAM_CONSOLE_H__
#define __RAM_CONSOLE_H__
extern int card_dump_func_write(unsigned char *buf, unsigned int len, unsigned long long offset,
				int dev);
#ifdef CONFIG_MTPROF
extern int boot_finish;
#endif
extern struct file *expdb_open(void);
#ifdef CONFIG_PSTORE
extern void pstore_bconsole_write(struct console *con, const char *s, unsigned c);
#endif
extern u32 scp_dump_pc(void);
extern u32 scp_dump_lr(void);
#endif

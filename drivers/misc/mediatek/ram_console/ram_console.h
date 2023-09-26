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
#define RAM_CONSOLE_EXP_TYPE_MAGIC 0xaeedead0
#define RAM_CONSOLE_EXP_TYPE_DEC(exp_type) \
	((exp_type ^ RAM_CONSOLE_EXP_TYPE_MAGIC) < 16 ? \
	 exp_type ^ RAM_CONSOLE_EXP_TYPE_MAGIC : exp_type)
#define RAM_CONSOLE_DRAM_OFF 0X1000
#ifdef CONFIG_MTPROF
extern int boot_finish;
#endif
#ifdef CONFIG_PSTORE
extern void pstore_bconsole_write(struct console *con, const char *s,
					unsigned int c);
#endif
extern struct pstore_info *psinfo;
extern void	pstore_record_init(struct pstore_record *record,
				   struct pstore_info *psi);
extern u32 scp_dump_pc(void);
extern u32 scp_dump_lr(void);
#endif

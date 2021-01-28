/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MBOOT_PARAMS_INTERNAL_H__
#define __MBOOT_PARAMS_INTERNAL_H__
#define MBOOT_PARAMS_EXP_TYPE_MAGIC 0xaeedead0
#define MBOOT_PARAMS_EXP_TYPE_DEC(exp_type) \
	((exp_type ^ MBOOT_PARAMS_EXP_TYPE_MAGIC) < 16 ? \
	 exp_type ^ MBOOT_PARAMS_EXP_TYPE_MAGIC : exp_type)

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

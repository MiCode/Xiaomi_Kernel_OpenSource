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
#ifndef __MTK_HOOKS_H__
#define __MTK_HOOKS_H__

extern int __weak arm_undefinstr_retry(struct pt_regs *regs,
		unsigned int instr);
extern void __weak ioremap_debug_hook_func(phys_addr_t phys_addr,
		size_t size, pgprot_t prot);
extern int __weak mem_fault_debug_hook(struct pt_regs *regs);

#endif /* __MTK_HOOKS_H__ */

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if !defined(__MRDUMP_MINI_H__)
#define __MRDUMP_MINI_H__

extern void get_mbootlog_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start);
extern void aee_rr_get_desc_info(unsigned long *addr, unsigned long *size,
		unsigned long *start);
extern void mrdump_mini_set_addr_size(unsigned int addr, unsigned int size);
extern void mrdump_mini_ke_cpu_regs(struct pt_regs *regs);
extern void mrdump_mini_add_misc_pa(unsigned long va, unsigned long pa,
		unsigned long size, unsigned long start, char *name);
extern void *remap_lowmem(phys_addr_t start, phys_addr_t size);

#ifdef CONFIG_MODULES
void load_ko_addr_list(struct module *module);
void unload_ko_addr_list(struct module *module);
#endif

#endif

/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MINIDUMP_H
#define __MINIDUMP_H

#define MAX_NAME_LENGTH		16
/* md_region -  Minidump table entry
 * @name:	Entry name, Minidump will dump binary with this name.
 * @id:		Entry ID, used only for SDI dumps.
 * @virt_addr:  Address of the entry.
 * @phys_addr:	Physical address of the entry to dump.
 * @size:	Number of byte to dump from @address location
 *		it should be 4 byte aligned.
 */
struct md_region {
	char	name[MAX_NAME_LENGTH];
	u32	id;
	u64	virt_addr;
	u64	phys_addr;
	u64	size;
};

/* Register an entry in Minidump table
 * Returns:
 *	Zero: on successful addition
 *	Negetive error number on failures
 */
#ifdef CONFIG_MINIDUMP
extern int msm_minidump_add_region(const struct md_region *entry);
extern bool minidump_enabled;
#else
static inline int msm_minidump_add_region(const struct md_region *entry)
{
	return -ENODEV;
}
static inline bool msm_minidump_enabled(void) { return false; }
#endif
#ifdef CONFIG_COMMON_LOG
extern void dump_stack_minidump(u64 sp);
#else
static inline void dump_stack_minidump(u64 sp) {}
#endif
#endif

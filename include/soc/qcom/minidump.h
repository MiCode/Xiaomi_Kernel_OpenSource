/* Copyright (c) 2017-18 The Linux Foundation. All rights reserved.
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

#define MAX_NAME_LENGTH		12
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

#ifdef CONFIG_QCOM_MINIDUMP
/*
 * Register an entry in Minidump table
 * Returns:
 *	region number: entry position in minidump table.
 *	Negetive error number on failures.
 */
extern int msm_minidump_add_region(const struct md_region *entry);
/*
 * Update registered region address in Minidump table.
 * It does not hold any locks, so strictly serialize the region updates.
 * Returns:
 *	Zero: on successfully update
 *	Negetive error number on failures.
 */
extern int msm_minidump_update_region(int regno, const struct md_region *entry);
extern bool msm_minidump_enabled(void);
extern void dump_stack_minidump(u64 sp);
#else
static inline int msm_minidump_add_region(const struct md_region *entry)
{
	/* Return quietly, if minidump is not supported */
	return 0;
}
static inline bool msm_minidump_enabled(void) { return false; }
static inline void dump_stack_minidump(u64 sp) {}
#endif
#endif

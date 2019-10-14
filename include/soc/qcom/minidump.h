/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

/* Register an entry in Minidump table
 * Returns:
 *	Zero: on successful addition
 *	Negetive error number on failures
 */
#ifdef CONFIG_QCOM_MINIDUMP
extern int msm_minidump_add_region(const struct md_region *entry);
extern int msm_minidump_remove_region(const struct md_region *entry);
extern bool msm_minidump_enabled(void);
extern void dump_stack_minidump(u64 sp);
#else
static inline int msm_minidump_add_region(const struct md_region *entry)
{
	/* Return quietly, if minidump is not supported */
	return 0;
}
static inline int msm_minidump_remove_region(const struct md_region *entry)
{
	return 0;
}
static inline bool msm_minidump_enabled(void) { return false; }
static inline void dump_stack_minidump(u64 sp) {}
#endif
#endif

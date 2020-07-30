/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MINIDUMP_H
#define __MINIDUMP_H

#include <linux/types.h>

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

/*
 * Register an entry in Minidump table
 * Returns:
 *	region number: entry position in minidump table.
 *	Negative error number on failures.
 */
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
extern struct seq_buf *md_meminfo_seq_buf;
extern struct seq_buf *md_slabinfo_seq_buf;

extern int msm_minidump_add_region(const struct md_region *entry);
extern int msm_minidump_remove_region(const struct md_region *entry);
/*
 * Update registered region address in Minidump table.
 * It does not hold any locks, so strictly serialize the region updates.
 * Returns:
 *	Zero: on successfully update
 *	Negetive error number on failures.
 */
extern int msm_minidump_update_region(int regno, const struct md_region *entry);
extern bool msm_minidump_enabled(void);
extern struct md_region *md_get_region(char *name);
extern void dump_stack_minidump(u64 sp);
extern void md_dump_meminfo(void);
extern void md_dump_slabinfo(void);
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
static inline struct md_region *md_get_region(char *name) { return NULL; }
static inline void dump_stack_minidump(u64 sp) {}
static inline void add_trace_event(char *buf, size_t size) {}
static inline void md_dump_meminfo(void) {}
static inline void md_dump_slabinfo(void) {}
#endif
#ifdef CONFIG_QCOM_MINIDUMP_FTRACE
extern void minidump_add_trace_event(char *buf, size_t size);
#else
static inline void minidump_add_trace_event(char *buf, size_t size) {}
#endif
#ifdef CONFIG_PAGE_OWNER
extern size_t md_pageowner_dump_size;
extern char *md_pageowner_dump_addr;

extern bool is_page_owner_enabled(void);
extern void md_dump_pageowner(void);
#else
static inline void md_dump_pageowner(void) {}
static inline bool is_page_owner_enabled(void) { return false; }
#endif
#ifdef CONFIG_SLUB_DEBUG
extern size_t md_slabowner_dump_size;
extern char *md_slabowner_dump_addr;

extern bool is_slub_debug_enabled(void);
extern void md_dump_slabowner(void);
#else
static inline void md_dump_slabowner(void) {}
static inline bool is_slub_debug_enabled(void) { return false; }
#endif
#endif

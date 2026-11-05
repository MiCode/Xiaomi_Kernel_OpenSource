/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MINIDUMP_MEMORY_H
#define _MINIDUMP_MEMORY_H

#define MD_MEMINFO_PAGES	1
#define MD_SLABINFO_PAGES	8

int md_register_panic_entries(int num_pages, char *name,
				      struct seq_buf **global_buf);

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_MEMORY_INFO
int md_minidump_memory_init(void);
void md_dump_memory(void);
#else
static inline int md_minidump_memory_init(void)
{
	return 0;
}
static inline void md_dump_memory(void) {}
#endif

#endif /*_MEM_BUF_EXPORTER_H */

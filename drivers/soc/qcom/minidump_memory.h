/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define MD_MEMINFO_PAGES	1
#define MD_SLABINFO_PAGES	8

void md_dump_meminfo(struct seq_buf *m);
#ifdef CONFIG_SLUB_DEBUG
void md_dump_slabinfo(struct seq_buf *m);
#else
static inline void md_dump_slabinfo(struct seq_buf *m) {}
#endif

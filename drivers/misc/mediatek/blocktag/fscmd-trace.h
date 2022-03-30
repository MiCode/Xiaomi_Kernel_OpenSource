/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _FSCMD_F2FS_SYNC_FS_H
#define _FSCMD_F2FS_SYNC_FS_H

#include <linux/types.h>

#include "mtk_blocktag.h"

void fscmd_trace_sys_enter(void *data,
		struct pt_regs *regs, long id);

void fscmd_trace_sys_exit(void *data,
		struct pt_regs *regs, long ret);

void fscmd_trace_f2fs_write_checkpoint(void *data,
		struct super_block *sb,
		int reason, char *msg);

void fscmd_trace_f2fs_gc_begin(void *data,
		struct super_block *sb, bool sync, bool background,
		long long dirty_nodes, long long dirty_dents,
		long long dirty_imeta, unsigned int free_sec,
		unsigned int free_seg, int reserved_seg,
		unsigned int prefree_seg);

void fscmd_trace_f2fs_gc_end(void *data,
	struct super_block *sb, int ret, int seg_freed,
	int sec_freed, long long dirty_nodes,
	long long dirty_dents, long long dirty_imeta,
	unsigned int free_sec, unsigned int free_seg,
	int reserved_seg, unsigned int prefree_seg);

void mtk_fscmd_show(char **buff, unsigned long *size,
	struct seq_file *seq);

int mtk_fscmd_init(void);
#endif //_FSCMD_F2FS_SYNC_FS_H

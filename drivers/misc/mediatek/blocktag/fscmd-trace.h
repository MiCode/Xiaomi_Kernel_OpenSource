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

void mtk_fscmd_show(char **buff, unsigned long *size,
	struct seq_file *seq);

int mtk_fscmd_init(void);
#endif //_FSCMD_F2FS_SYNC_FS_H

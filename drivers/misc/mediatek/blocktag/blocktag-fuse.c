// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/fuse.h>

#include "blocktag-fuse.h"

#define MTK_FUSE_CONN_EXT_CNT 3
static struct mtk_fuse_conn_ext fc_exts[MTK_FUSE_CONN_EXT_CNT];

void mtk_fuse_init(void)
{
	memset(fc_exts, 0,
	       sizeof(struct mtk_fuse_conn_ext) * MTK_FUSE_CONN_EXT_CNT);
}

void mtk_fuse_deinit(void)
{
	memset(fc_exts, 0,
	       sizeof(struct mtk_fuse_conn_ext) * MTK_FUSE_CONN_EXT_CNT);
}

void mtk_fuse_init_reply(void *data, struct fuse_mount *fm,
			 struct fuse_init_in *in_arg,
			 struct fuse_init_out *out_arg, int error)
{
	int i;

	for (i = 0; i < MTK_FUSE_CONN_EXT_CNT; i++) {
		if (fc_exts[i].fc)
			continue;
		fc_exts[i].fc = fm->fc;
		fc_exts[i].daemon_pid = task_tgid_nr(current);
	}
}

static inline void mtk_fuse_trace_request(void *data, struct fuse_mount *fm,
					  struct fuse_args *args)
{
	struct mtk_fuse_conn_ext *fc_ext = NULL;
	int i;

	for (i = 0; i < MTK_FUSE_CONN_EXT_CNT; i++) {
		if (fc_exts[i].fc && fc_exts[i].fc == fm->fc) {
			fc_ext = &fc_exts[i];
			break;
		}
	}
	if (!fc_ext)
		return;
	if (args->opcode != FUSE_CREATE && args->opcode != FUSE_OPEN
		&& args->opcode != FUSE_RELEASE && args->opcode != FUSE_MKNOD)
		return;
	if (fc_ext->daemon_pid == task_tgid_nr(current)
		&& strstr(current->comm, "Thread-")) {
		pr_warn("%s: opcode[%d] is from fusedaemon[%s] with nodeio=%llu\n",
			__func__, args->opcode, current->comm, args->nodeid);
		WARN_ON(1);
	}
}

void mtk_fuse_simple_request(void *data, struct fuse_mount *fm,
			     struct fuse_args *args)
{
	mtk_fuse_trace_request(data, fm, args);
}

void mtk_fuse_simple_background(void *data, struct fuse_mount *fm,
				struct fuse_args *args)
{
	mtk_fuse_trace_request(data, fm, args);
}

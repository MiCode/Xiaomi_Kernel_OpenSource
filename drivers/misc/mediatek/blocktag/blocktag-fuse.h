/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef __BLOCKTAG_FUSE_H
#define __BLOCKTAG_FUSE_H

#include <linux/types.h>
#include "fuse_i.h"

struct mtk_fuse_conn_ext {
		struct fuse_conn *fc;
		pid_t daemon_pid;
};

#if IS_ENABLED(CONFIG_MTK_FUSE_DEBUG)

void mtk_fuse_init_reply(void *data, struct fuse_mount *fm,
		struct fuse_init_in *in_arg, struct fuse_init_out *out_arg,
		int error);

void mtk_fuse_simple_request(void *data, struct fuse_mount *fm,
		struct fuse_args *args);

void mtk_fuse_simple_background(void *data, struct fuse_mount *fm,
		struct fuse_args *args);

void mtk_fuse_init(void);

void mtk_fuse_deinit(void);
#else

#define mtk_fuse_init_reply(...)
#define mtk_fuse_simple_request(...)
#define mtk_fuse_simple_background(...)
#define mtk_fuse_init(...)
#define mtk_fuse_deinit(...)

#endif /* CONFIG_MTK_FUSE_DEBUG */
#endif /* __BLOCKTAG_FUSE_H */

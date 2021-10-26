/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-afe-debug.h  --  Mediatek AFE Debug definition
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef _MTK_AFE_DEBUG_H_
#define _MTK_AFE_DEBUG_H_

#include <linux/debugfs.h>

struct mtk_afe_debug_cmd {
	const char *cmd;
	void (*fn)(struct file *file, void *arg);
};

#define MTK_AFE_DBG_CMD(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

/* debugfs ops */
int mtk_afe_debugfs_open(struct inode *inode, struct file *file);
ssize_t mtk_afe_debugfs_write(struct file *f, const char __user *buf,
			      size_t count, loff_t *offset);

/* debug function */
void mtk_afe_debug_write_reg(struct file *file, void *arg);

#endif

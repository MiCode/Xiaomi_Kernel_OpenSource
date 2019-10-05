/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MLOG_DUMP_H
#define _MLOG_DUMP_H

extern void mlog_init_procfs(void);
extern int mlog_print_fmt(struct seq_file *m);
extern void mlog_doopen(void);
extern int mlog_doread(char __user *buf, size_t len);
extern int mlog_unread(void);

extern int dmlog_open(struct inode *inode, struct file *file);
extern int dmlog_release(struct inode *inode, struct file *file);
extern ssize_t dmlog_read(struct file *file, char __user *buf, size_t len,
		loff_t *ppos);

extern wait_queue_head_t mlog_wait;

#endif

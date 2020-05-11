/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONN_MD_DBG_H_
#define __CONN_MD_DBG_H_

typedef int (*conn_md_dev_dbg_func) (int par1, int par2, int par3);

extern int conn_md_dbg_init(void);
extern int conn_md_test(void);
ssize_t conn_md_dbg_read(struct file *filp, char __user *buf,
			size_t count, loff_t *f_pos);
ssize_t conn_md_dbg_write(struct file *filp, const char __user *buf,
			size_t count, loff_t *f_pos);

#endif/*__CONN_MD_DBG_H_*/

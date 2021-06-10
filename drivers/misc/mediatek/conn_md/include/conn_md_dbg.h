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

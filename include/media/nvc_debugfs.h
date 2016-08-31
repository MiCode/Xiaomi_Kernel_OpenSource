/*
* nvc_debugfs.h
*
* Copyright (c) 2013, NVIDIA, All Rights Reserved.
*
* This file is licensed under the terms of the GNU General Public License
* version 2. This program is licensed "as is" without any warranty of any
* kind, whether express or implied.
*/

#ifndef __NVC_DEBUGFS_H__
#define __NVC_DEBUGFS_H__

#include <linux/i2c.h>

struct nvc_debugfs_info {
	const char *name;
	struct dentry *debugfs_root;
	u16 i2c_reg;
	u16 i2c_addr_limit;
	struct i2c_client *i2c_client;
	int (*i2c_rd8)(struct i2c_client*, u16, u8*);
	int (*i2c_wr8)(struct i2c_client*, u16, u8);
};

int nvc_debugfs_init(struct nvc_debugfs_info *info);
int nvc_debugfs_remove(struct nvc_debugfs_info *info);

#endif /* __NVC_DEBUGFS_H__ */

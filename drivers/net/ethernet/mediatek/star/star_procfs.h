/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _STAR_PROCFS_H_
#define _STAR_PROCFS_H_

#include <linux/proc_fs.h>

struct star_proc_file {
	const char * const name;
	const struct file_operations *fops;
};

struct star_procfs {
	struct net_device *ndev;
	struct proc_dir_entry *root;
	struct proc_dir_entry **entry;
};

int star_init_procfs(void);
void star_exit_procfs(void);

#endif /* _STAR_PROCFS_H_ */


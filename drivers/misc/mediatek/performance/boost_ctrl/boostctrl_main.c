/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/proc_fs.h>

#include "boost_ctrl.h"

int init_boostctrl(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *bstctrl_root = NULL;

	pr_debug("__init init_boostctrl\n");


	bstctrl_root = proc_mkdir("boost_ctrl", parent);

	cpu_ctrl_init(bstctrl_root);

	dram_ctrl_init(bstctrl_root);

	eas_ctrl_init(bstctrl_root);

	topo_ctrl_init(bstctrl_root);

	return 0;
}

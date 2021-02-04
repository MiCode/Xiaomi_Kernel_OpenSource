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

#include "tchbst.h"

int init_tchbst(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *tchbst_root = NULL;

	pr_debug("__init init_tchbst\n");

	/*create touch root procfs*/
	tchbst_root = proc_mkdir("tchbst", parent);

#ifdef CONFIG_MTK_PERFMGR_TOUCH_BOOST
	/*initial kernel touch parameter*/
	init_ktch(tchbst_root);
#endif
#if defined(CONFIG_MTK_FPSGO) || defined(CONFIG_MTK_FPSGO_V3)
	/*initial user touch parameter*/
	init_utch(tchbst_root);
#endif
	return 0;
}

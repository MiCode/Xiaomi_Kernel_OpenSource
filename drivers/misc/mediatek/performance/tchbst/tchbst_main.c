// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/proc_fs.h>

#include "tchbst.h"

int init_tchbst(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *tchbst_root = NULL;

	pr_debug("__init %s\n", __func__);

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

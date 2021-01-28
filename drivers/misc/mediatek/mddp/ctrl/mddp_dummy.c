// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>

static int __init mddp_init(void)
{
	return 0;
}

static void __exit mddp_exit(void)
{
}
module_init(mddp_init);
module_exit(mddp_exit);

MODULE_LICENSE("GPL v2");

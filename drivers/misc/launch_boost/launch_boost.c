
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024-2024 MI. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>

static int __init lb_init(void)
{
        return 0;
}

static void __exit lb_exit(void)
{
}

MODULE_LICENSE("GPL v2");
module_init(lb_init);
module_exit(lb_exit);

// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>
#include <linux/init.h>
#include "reclaim.h"

static int __init mm_reclaim_init(void)
{	kshrink_lruvec_init();
	kshrink_slabd_async_init();
	unisoc_enhance_reclaim_init();
	shrink_anon_init();
	return 0;
}

static void __exit mm_reclaim_exit(void)
{
	kshrink_lruvec_exit();
	kshrink_slabd_async_exit();
	unisoc_enhance_reclaim_exit();
	shrink_anon_exit();
}

module_init(mm_reclaim_init);
module_exit(mm_reclaim_exit);
MODULE_LICENSE("GPL v2");

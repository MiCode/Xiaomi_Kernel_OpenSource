// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) "MKP: " fmt

#include <linux/types.h> // for list_head
#include <linux/module.h> // module_layout
#include <linux/init.h> // rodata_enable support
#include <linux/mutex.h>
#include <linux/kernel.h> // round_up

#ifdef DEMO_MKP
#include "mkp_demo.h"
#endif

#include "mkp.h"

static int __init mkp_init(void)
{
	int ret = 0;

	pr_info("%s:%d start\n", __func__, __LINE__);
#ifdef DEMO_MKP
	ret = mkp_demo_init();
#endif

	if (ret)
		pr_info("%s: failed, ret: %d\n", __func__, ret);

	pr_info("%s:%d done\n", __func__, __LINE__);
	return ret;
}
module_init(mkp_init);

static void  __exit mkp_exit(void)
{
	/*
	 * vendor hook cannot unregister, please check vendor_hook.h
	 */
	pr_info("%s:%d\n", __func__, __LINE__);
}
module_exit(mkp_exit);

MODULE_AUTHOR("<Kuan-Ying.Lee@mediatek.com>");
MODULE_DESCRIPTION("MediaTek MKP Driver");
MODULE_LICENSE("GPL");

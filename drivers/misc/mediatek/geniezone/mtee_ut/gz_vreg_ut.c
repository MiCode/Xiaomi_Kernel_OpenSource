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

#include "gz_vreg_ut.h"
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#define KREE_DEBUG(fmt...) pr_info("[SM_kUT]" fmt)
#define KREE_INFO(fmt...) pr_info("[SM_kUT]" fmt)
#define KREE_ERR(fmt...) pr_info("[SM_kUT][ERR]" fmt)

/*vreg: the same setting in gz*/
#define test_vreg_src_pa  0x1070A000  /*can modify*/
#define test_vreg_basic_size  0x1000  /*can modify*/

#define VREG_BASE_dyn_map (test_vreg_src_pa + 1 * test_vreg_basic_size)

#define WORD_WIDTH 4 /*32-bit*/

DEFINE_MUTEX(vreg_ut_mutex);

int gz_test_vreg_main(void)
{
	void __iomem *io;
	uint32_t v;

	KREE_INFO("==> %s run\n", __func__);

	io = ioremap_wc(VREG_BASE_dyn_map, test_vreg_basic_size);
	if (!io) {
		KREE_ERR("[%s] ioremap_wc fail\n", __func__);
		goto out;
	}

	writel(0x2, io + (2 * WORD_WIDTH));

	v = readl(io + (2 * WORD_WIDTH));
	KREE_INFO("[%s] read[0x%llx]=0x%x\n", __func__,
	(uint64_t) (io + (2 * WORD_WIDTH)), v);

	if (io)
		iounmap(io);

out:

	return 0;
}

int gz_test_vreg(void *args)
{
	mutex_lock(&vreg_ut_mutex);

	gz_test_vreg_main();

	mutex_unlock(&vreg_ut_mutex);

	return 0;
}


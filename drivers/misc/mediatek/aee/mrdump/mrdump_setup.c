/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <mrdump.h>
#include <asm/memory.h>
#include <mtk_wd_api.h>
#include "mrdump_private.h"

static void mrdump_hw_enable(bool enabled)
{
	if (enabled) {
		mrdump_cblock->enabled = MRDUMP_ENABLE_COOKIE;
		pr_info("%s: mrdump enabled!\n", __func__);
	} else {
		mrdump_cblock->enabled = 0;
		pr_info("%s: mrdump disabled!\n", __func__);
	}
	__inner_flush_dcache_all();
}

static void mrdump_reboot(void)
{
	aee_exception_reboot();
}

const struct mrdump_platform mrdump_v1_platform = {
	.hw_enable = mrdump_hw_enable,
	.reboot = mrdump_reboot
};

int __init mrdump_init(void)
{
	mrdump_cblock_init();
	return mrdump_platform_init(&mrdump_v1_platform);
}

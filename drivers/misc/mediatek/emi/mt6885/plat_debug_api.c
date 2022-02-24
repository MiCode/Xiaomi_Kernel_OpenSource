/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mtk_io.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#include <mt-plat/mtk_ram_console.h>
#endif

#include "mt_emi.h"

void plat_debug_api_init(void)
{
}

void dump_emi_outstanding(void)
{
}

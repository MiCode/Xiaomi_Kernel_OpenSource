// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_reservedmem.h>
#endif

/* TODO: independent in after project */
#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>
#include <swpm_module.h>
#include <swpm_v6985.h>

#include <smap_v6985.h>

/* share sram for smap data */
static struct share_data_smap *share_data_ref_smap;

int smap_get_data(unsigned int idx)
{
	if (!share_data_ref_smap)
		return -1;

	if (idx == 0)
		return share_data_ref_smap->latest_cnt_0;
	else
		return share_data_ref_smap->latest_cnt_1;
}

/*
 * TODO: independent in after project,
 * currently must be called(load module) after swpm init
 */
int smap_v6985_init(void)
{
	unsigned int offset;

	/* get sbuf offset */
	offset = swpm_set_and_get_cmd(0, 0, SMAP_GET_ADDR, SMAP_CMD_TYPE);

	/* get sram address */
	share_data_ref_smap = (struct share_data_smap *)sspm_sbuf_get(offset);

	/* exception control for illegal sbuf request */
	if (!share_data_ref_smap) {
		pr_notice("share sram offset fail\n");
		return -1;
	}

	share_data_ref_smap->latest_cnt_0 = 0;
	share_data_ref_smap->latest_cnt_1 = 0;

	return 0;
}


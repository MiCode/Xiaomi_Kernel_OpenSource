/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include "adsp_helper.h"
#include "adsp_bus_monitor.h"

/**
 * TIMEOUT_VALUE = value * unit(15 cycle count) / clk_src
 */
#define BUS_MON_STAGE1_TIMEOUT_VALUE    (0x01DA0A2B) /* 1.00 sec @ 466MHz */
#define BUS_MON_STAGE2_TIMEOUT_VALUE    (0x03B41456) /* 2.00 sec @ 466MHz */

#define ADSP_BUS_MON_STATE              (ADSP_BUS_MON_BACKUP_BASE)
#define ADSP_BUS_MON_1ST_STAGE_BASE     (ADSP_BUS_MON_BACKUP_BASE + sizeof(u32))
#define ADSP_BUS_MON_2ND_STAGE_BASE     (ADSP_BUS_MON_1ST_STAGE_BASE + \
					 sizeof(struct bus_monitor_cblk))
#define ADSP_BUS_MON_BACKUP_SIZE        (sizeof(u32) + \
					 2 * sizeof(struct bus_monitor_cblk))

int adsp_bus_monitor_init(void)
{
	/* Clear bus monitor register backup in DTCM */
	memset_io((void *)ADSP_BUS_MON_BACKUP_BASE, 0,
		  (size_t)ADSP_BUS_MON_BACKUP_SIZE);

	/* initialize bus monitor */
	writel(BUS_MON_STAGE1_TIMEOUT_VALUE, ADSP_BUS_DBG_TIMER_CON0);
	writel(BUS_MON_STAGE2_TIMEOUT_VALUE, ADSP_BUS_DBG_TIMER_CON1);
	writel(0x0, ADSP_BUS_DBG_WP);
	writel(0x0, ADSP_BUS_DBG_WP_MASK);
	writel(0x00002037, ADSP_BUS_DBG_CON); /* timeout control */
	writel(STAGE_RUN, ADSP_BUS_MON_STATE);

	return 0;
}

bool is_adsp_bus_monitor_alert(void)
{
	return readl(ADSP_BUS_MON_STATE) > STAGE_RUN;
}

static void adsp_bus_monitor_stage_info(void *addr)
{
	int i = 0;
	struct bus_monitor_cblk cblk;

	memcpy_fromio(&cblk, addr, sizeof(struct bus_monitor_cblk));

	pr_info("BUS_DBG_CON = 0x%08x", cblk.ctrl);
	pr_info("TIMER_CON0 = 0x%08x, TIMER_CON01 = 0x%08x",
		cblk.timer_ctrl[0], cblk.timer_ctrl[1]);

	for (i = 0; i < 8; i++) {
		if (!cblk.r_tracks[i] && !cblk.w_tracks[i])
			continue;

		pr_info("R_TRACK[%d] = 0x%08x, W_TRACK[%d] = 0x%08x",
			i, cblk.r_tracks[i],
			i, cblk.w_tracks[i]);
	}
}

void adsp_bus_monitor_dump(void)
{
	pr_info("%s(), BUS_MON_1ST_STAGE BACKUP", __func__);
	adsp_bus_monitor_stage_info((void *)ADSP_BUS_MON_1ST_STAGE_BASE);

	pr_info("%s(), BUS_MON_2ST_STAGE BACKUP", __func__);
	adsp_bus_monitor_stage_info((void *)ADSP_BUS_MON_2ND_STAGE_BASE);
}

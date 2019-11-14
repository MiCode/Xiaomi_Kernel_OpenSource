/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6873_SUSPEND_H__
#define __MT6873_SUSPEND_H__

/**********************************************************
 * MD sleep status
 **********************************************************/
struct md_sleep_status {
	u64 sleep_wall_clk;
	u64 sleep_cnt;
	u64 sleep_cnt_reserve;
	u64 sleep_time;
};

int mt6873_model_suspend_init(void);

extern void gpio_dump_regs(void);
extern void pll_if_on(void);
extern void subsys_if_on(void);

#endif

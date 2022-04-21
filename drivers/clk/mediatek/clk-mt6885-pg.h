/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLK_MT6893_PG_H
#define __DRV_CLK_MT6893_PG_H

enum subsys_id {
	SYS_MD1 = 0,
	SYS_CONN = 1,
	SYS_MFG0 = 2,
	SYS_MFG1 = 3,
	SYS_MFG2 = 4,
	SYS_MFG3 = 5,
	SYS_MFG4 = 6,
	SYS_MFG5 = 7,
	SYS_MFG6 = 8,
	SYS_ISP = 9,
	SYS_ISP2 = 10,
	SYS_IPE = 11,
	SYS_VDE = 12,
	SYS_VDE2 = 13,
	SYS_VEN = 14,
	SYS_VEN_CORE1 = 15,
	SYS_MDP = 16,
	SYS_DIS = 17,
	SYS_AUDIO = 18,
	SYS_ADSP = 19,
	SYS_CAM = 20,
	SYS_CAM_RAWA = 21,
	SYS_CAM_RAWB = 22,
	SYS_CAM_RAWC = 23,
	SYS_DP_TX = 24,
	SYS_VPU = 25,

	NR_SYSS = 26,
};

struct pg_callbacks {
	struct list_head list;
	void (*before_off)(enum subsys_id sys);
	void (*after_on)(enum subsys_id sys);
	void (*debug_dump)(enum subsys_id sys);
};

/* register new pg_callbacks and return previous pg_callbacks. */
extern struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb);
void mtk_check_subsys_swcg(enum subsys_id id);

#endif/* __DRV_CLK_MT6893_PG_H */

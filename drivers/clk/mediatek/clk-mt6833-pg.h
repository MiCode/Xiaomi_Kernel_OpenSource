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

#ifndef __DRV_CLK_MT6833_PG_H
#define __DRV_CLK_MT6833_PG_H

enum subsys_id {
	SYS_MD1 = 0,
	SYS_CONN = 1,
	SYS_MFG0 = 2,
	SYS_MFG1 = 3,
	SYS_MFG2 = 4,
	SYS_MFG3 = 5,
	SYS_ISP = 6,
	SYS_ISP2 = 7,
	SYS_IPE = 8,
	SYS_VDE = 9,
	SYS_VEN = 10,
	SYS_DIS = 11,
	SYS_AUDIO = 12,
	SYS_CAM = 13,
	SYS_CAM_RAWA = 14,
	SYS_CAM_RAWB = 15,
	NR_SYSS = 16,
};

enum mtcmos_op {
	MTCMOS_BUS_PROT = 0,
	MTCMOS_PWR = 1,
};

struct pg_callbacks {
	struct list_head list;
	void (*before_off)(enum subsys_id sys);
	void (*after_on)(enum subsys_id sys);
	void (*debug_dump)(enum subsys_id sys);
};

/* register new pg_callbacks and return previous pg_callbacks. */
extern struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb);
extern void subsys_if_on(void);
extern void mtcmos_force_off(void);
extern void mtk_check_subsys_swcg(enum subsys_id id);

#endif/* __DRV_CLK_MT6758_PG_H */

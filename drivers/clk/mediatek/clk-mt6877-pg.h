/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __DRV_CLK_MT6877_PG_H
#define __DRV_CLK_MT6877_PG_H

enum subsys_id {
	SYS_MD = 0,
	SYS_CONN = 1,
	SYS_ISP0 = 2,
	SYS_ISP1 = 3,
	SYS_IPE = 4,
	SYS_VDEC = 5,
	SYS_VENC = 6,
	SYS_DISP = 7,
	SYS_AUDIO = 8,
	SYS_ADSP_DORMANT = 9,
	SYS_APU = 10,
	SYS_CAM = 11,
	SYS_CAM_RAWA = 12,
	SYS_CAM_RAWB = 13,
	SYS_CSI = 14,
	SYS_MFG0 = 15,
	SYS_MFG1 = 16,
	SYS_MFG2 = 17,
	SYS_MFG3 = 18,
	SYS_MFG4 = 19,
	SYS_MFG5 = 20,
	NR_SYSS = 21,
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
extern void mtcmos_force_off(void);
extern void mtk_check_subsys_swcg(enum subsys_id id);

#endif/* __DRV_CLK_MT6877_PG_H */

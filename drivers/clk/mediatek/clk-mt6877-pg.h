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
	SYS_MFG0 = 2,
	SYS_MFG1 = 3,
	SYS_MFG2 = 4,
	SYS_MFG3 = 5,
	SYS_MFG4 = 6,
	SYS_MFG5 = 7,
	SYS_ISP0 = 8,
	SYS_ISP1 = 9,
	SYS_IPE = 10,
	SYS_VDEC = 11,
	SYS_VENC = 12,
	SYS_DISP = 13,
	SYS_AUDIO = 14,
	SYS_ADSP_DORMANT = 15,
	SYS_CAM = 16,
	SYS_CAM_RAWA = 17,
	SYS_CAM_RAWB = 18,
	SYS_CSI = 19,
	SYS_APU = 20,
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

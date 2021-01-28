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

#ifndef __DRV_CLK_MT6771_PG_H
#define __DRV_CLK_MT6771_PG_H

enum subsys_id {
	SYS_MD1 = 0,
	SYS_CONN = 1,
	SYS_DIS = 2,
	SYS_MFG = 3,
	SYS_ISP = 4,
	SYS_VEN = 5,
	SYS_MFG_ASYNC = 6,
	SYS_AUDIO = 7,
	SYS_CAM = 8,
	SYS_MFG_CORE1 = 9,
	SYS_MFG_CORE0 = 10,
	SYS_VDE = 11,
	SYS_VPU_TOP = 12,
	SYS_VPU_CORE0_DORMANT = 13,
	SYS_VPU_CORE0_SHUTDOWN = 14,
	SYS_VPU_CORE1_DORMANT = 15,
	SYS_VPU_CORE1_SHUTDOWN = 16,
	SYS_VPU_CORE2_DORMANT = 17,
	SYS_VPU_CORE2_SHUTDOWN = 18,
	SYS_MFG_2D = 19,
	NR_SYSS = 20,
};

struct pg_callbacks {
	void (*before_off)(enum subsys_id sys);
	void (*after_on)(enum subsys_id sys);
};

/* register new pg_callbacks and return previous pg_callbacks. */
extern struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb);
extern int spm_topaxi_protect(unsigned int mask_value, int en);
extern int spm_topaxi_protect_1(unsigned int mask_value, int en);
extern int cam_mtcmos_patch(int on);
extern void mfg_way_en(int way_en);
extern void check_mm0_clk_sts(void);
extern void check_img_clk_sts(void);
extern void check_ven_clk_sts(void);
extern void check_cam_clk_sts(void);
extern void cam_mtcmos_check(void);
/*ram console api*/
/*
 *[0] bus protect reg
 *[1] pwr_status
 *[2] pwr_status 2
 *[others] local function use
 */
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif
/*extern void dump_emi_MM(void);*/
/*extern void dump_emi_latency(void);*/
#endif				/* __DRV_CLK_MT6771_PG_H */

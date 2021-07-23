/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __DRV_CLK_MT6785_PG_H
#define __DRV_CLK_MT6785_PG_H

enum subsys_id {
	SYS_MD1 = 0,
	SYS_CONN = 1,
	SYS_DIS = 2,
	SYS_VEN = 3,
	SYS_VDE = 4,
	SYS_CAM = 5,
	SYS_ISP = 6,
	SYS_AUDIO = 7,
	SYS_MFG0 = 8,
	SYS_MFG1 = 9,
	SYS_MFG2 = 10,
	SYS_MFG3 = 11,
	SYS_MFG4 = 12,
	SYS_VPU_VCORE_DORMANT = 13,
	SYS_VPU_VCORE_SHUTDOWN = 14,
	SYS_VPU_CONN_DORMANT = 15,
	SYS_VPU_CONN_SHUTDOWN = 16,
	SYS_VPU_CORE0_DORMANT = 17,
	SYS_VPU_CORE0_SHUTDOWN = 18,
	SYS_VPU_CORE1_DORMANT = 19,
	SYS_VPU_CORE1_SHUTDOWN = 20,
	SYS_VPU_CORE2_DORMANT = 21,
	SYS_VPU_CORE2_SHUTDOWN = 22,
	SYS_MFG5 = 23,
	NR_SYSS = 24,
};

struct pg_callbacks {
	struct list_head list;
	void (*before_off)(enum subsys_id sys);
	void (*after_on)(enum subsys_id sys);
	void (*debug_dump)(enum subsys_id sys);
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
extern unsigned int mt_get_abist_freq(unsigned int ID);
extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern void print_enabled_clks_once(void);
extern void mtk_wcn_cmb_stub_clock_fail_dump(void);
extern unsigned int cam_if_on(void);
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
#endif				/* __DRV_CLK_MT6785_PG_H */

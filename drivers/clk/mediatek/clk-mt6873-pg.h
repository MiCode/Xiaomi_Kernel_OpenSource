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

#ifndef __DRV_CLK_MT6873_PG_H
#define __DRV_CLK_MT6873_PG_H

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
	//SYS_VEN_CORE1 = 15,
	SYS_MDP = 15,
	SYS_DIS = 16,
	SYS_AUDIO = 17,
	SYS_ADSP = 18,
	SYS_CAM = 19,
	SYS_CAM_RAWA = 20,
	SYS_CAM_RAWB = 21,
	SYS_CAM_RAWC = 22,
	SYS_DP_TX = 23,
	SYS_VPU = 24,
	SYS_MSDC = 25,
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
extern int spm_topaxi_protect(unsigned int mask_value, int en);
extern int spm_topaxi_protect_1(unsigned int mask_value, int en);
extern int cam_mtcmos_patch(int on);
extern void check_mm0_clk_sts(void);
extern void check_img_clk_sts(void);
extern void check_ipe_clk_sts(void);
extern void check_ven_clk_sts(void);
extern void check_cam_clk_sts(void);
extern unsigned int mt_get_abist_freq(unsigned int ID);
extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern void print_enabled_clks_once(void);
extern void mtk_wcn_cmb_stub_clock_fail_dump(void);
extern int get_sw_req_vcore_opp(void);
void enable_subsys_hwcg(enum subsys_id id);
void mtk_check_subsys_swcg(enum subsys_id id);

/*
 * Resident in clkdbg-mt6873.c
 * For debug use.
 */
void print_subsys_reg(char *subsys_name);

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

#endif				/* __DRV_CLK_MT6873_PG_H */

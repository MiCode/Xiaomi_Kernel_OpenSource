/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __DRV_CLK_MT6781_PG_H
#define __DRV_CLK_MT6781_PG_H

enum subsys_id {
	SYS_MD1 = 0,
	SYS_CONN = 1,
	SYS_DIS = 2,
	SYS_VEN = 3,
	SYS_VDE = 4,
	SYS_CAM = 5,
	SYS_ISP = 6,
	SYS_MFG0 = 7,
	SYS_MFG1 = 8,
	SYS_MFG2 = 9,
	SYS_MFG3 = 10,
	SYS_ISP2 = 11,
	SYS_IPE = 12,
	SYS_CAM_RAWA = 13,
	SYS_CAM_RAWB = 14,
	SYS_CSI = 15,
	NR_SYSS = 16,
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
extern void mtk_check_subsys_swcg(enum subsys_id id);
/*
 * Resident in clkdbg-mt6781.c
 * For debug use.
 */
extern void print_subsys_reg(char *subsys_name);
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
#endif				/* __DRV_CLK_MT6781_PG_H */

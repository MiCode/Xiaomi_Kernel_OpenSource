// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
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
extern int spm_topaxi_protect(unsigned int mask_value, int en);
extern int spm_topaxi_protect_1(unsigned int mask_value, int en);
extern int cam_mtcmos_patch(int on);
extern void check_mm0_clk_sts(void);
extern void check_img_clk_sts(void);
extern void check_ipe_clk_sts(void);
extern void check_ven_clk_sts(void);
extern void check_cam_clk_sts(void);
void enable_subsys_hwcg(enum subsys_id id);
void mtk_check_subsys_swcg(enum subsys_id id);

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

/* for cam debug only */
extern void __iomem *cam_base, *cam_rawa_base, *cam_rawb_base, *cam_rawc_base;
extern void __iomem *spm_base_debug;
extern void mtk_ccf_cam_debug(const char *str1, const char *str2,
							const char *str3);
/* for cam debug only */



#endif				/* __DRV_CLK_MT6893_PG_H */

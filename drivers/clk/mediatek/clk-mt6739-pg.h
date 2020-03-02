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

#ifndef __DRV_CLK_MT6739_PG_H
#define __DRV_CLK_MT6739_PG_H

enum subsys_id {
	SYS_MFG0 = 0,
	SYS_MFG1 = 1,
	SYS_MD1 = 2,
	SYS_CONN = 3,
	SYS_MM0 = 4,
	SYS_ISP = 5,
	SYS_VEN = 6,
	NR_SYSS = 7,
};

struct pg_callbacks {
	struct list_head list;
	void (*before_off)(enum subsys_id sys);
	void (*after_on)(enum subsys_id sys);
};

/* register new pg_callbacks and return previous pg_callbacks. */
extern struct pg_callbacks *register_pg_callback(struct pg_callbacks *pgcb);

#if 0
extern void switch_mfg_clk(int src);
#endif
extern void subsys_if_on(void);
extern void mtcmos_force_off(void);

/*new arch*/
extern void mm0_mtcmos_patch(int on);
extern void ven_mtcmos_patch(void);
extern void isp_mtcmos_patch(int on);
extern void check_ven_clk_sts(void);
extern void check_mm0_clk_sts(void);
extern void check_img_clk_sts(void);
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
#endif/* __DRV_CLK_MT6758_PG_H */

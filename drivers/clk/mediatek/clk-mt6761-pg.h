/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLK_MT6761_PG_H
#define __DRV_CLK_MT6761_PG_H

enum subsys_id {
	SYS_MD1 = 0,
	SYS_CONN = 1,
	SYS_DPY = 2,
	SYS_DIS = 3,
	SYS_MFG = 4,
	SYS_IFR = 5,
	SYS_MFG_CORE0 = 6,
	SYS_MFG_ASYNC = 7,
	SYS_CAM = 8,
	SYS_VCODEC = 9,
	NR_SYSS = 10,
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
extern int spm_topaxi_protect(unsigned int mask_value, int en);

extern void subsys_if_on(void);
extern void mtcmos_force_off(void);

extern unsigned int mt_get_ckgen_freq(unsigned int ID);
/*extern void aee_sram_printk(const char *fmt, ...);*/
/*ram console api*/
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif

extern void print_enabled_clks_once(void);
#endif/* __DRV_CLK_MT6758_PG_H */

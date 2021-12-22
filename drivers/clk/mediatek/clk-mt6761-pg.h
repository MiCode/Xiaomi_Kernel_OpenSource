/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLK_MT6761_PG_H
#define __DRV_CLK_MT6761_PG_H

static DEFINE_SPINLOCK(mtcmos_ops_lock);

#define mtk_mtcmos_lock(flags)	spin_lock_irqsave(&mtcmos_ops_lock, flags)
#define mtk_mtcmos_unlock(flags)	\
	spin_unlock_irqrestore(&mtcmos_ops_lock, flags)

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

/*ram console api*/
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_clk(int id, u32 val);
#endif

extern void print_enabled_clks_once(void);
extern int mtk_is_mtcmos_enable(void);
#endif/* __DRV_CLK_MT6758_PG_H */

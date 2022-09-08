/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef _MT_CLKMGR_6765_H
#define _MT_CLKMGR_6765_H

extern void mtk_set_cg_disable(unsigned int disable);
extern void mtk_set_mtcmos_disable(unsigned int disable);
extern unsigned int mt_get_abist_freq(unsigned int ID);
extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern unsigned int __clk_get_enable_count(struct clk *clk);
extern struct clk *__clk_lookup(const char *name);
#endif

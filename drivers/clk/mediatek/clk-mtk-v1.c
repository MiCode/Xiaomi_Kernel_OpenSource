// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>

#include "clk-mtk-v1.h"

#if !defined(MT_CCF_DEBUG) || !defined(MT_CCF_BRINGUP)
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP	0
#endif

static DEFINE_SPINLOCK(clk_ops_lock);
static DEFINE_SPINLOCK(mtcmos_ops_lock);

spinlock_t *get_mtk_clk_lock(void)
{
	return &clk_ops_lock;
}

spinlock_t *get_mtk_mtcmos_lock(void)
{
	return &mtcmos_ops_lock;
}


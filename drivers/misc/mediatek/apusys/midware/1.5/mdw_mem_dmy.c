// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>
#include "mdw_cmn.h"
#include "mdw_mem_cmn.h"

static int mdw_mem_dmy_op(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_init(void)
{
	return 0;
}

static void mdw_mem_dmy_exit(void)
{
}

static struct mdw_mem_ops dmy_ops = {
	.init = mdw_mem_dmy_init,
	.exit = mdw_mem_dmy_exit,
	.alloc = mdw_mem_dmy_op,
	.free = mdw_mem_dmy_op,
	.flush = mdw_mem_dmy_op,
	.invalidate = mdw_mem_dmy_op,
	.map_kva = mdw_mem_dmy_op,
	.unmap_kva = mdw_mem_dmy_op,
	.map_iova = mdw_mem_dmy_op,
	.unmap_iova = mdw_mem_dmy_op,
};

struct mdw_mem_ops *mdw_mops_dmy(void)
{
	return &dmy_ops;
}


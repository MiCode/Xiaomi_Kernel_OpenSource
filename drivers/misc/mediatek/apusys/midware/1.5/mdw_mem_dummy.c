// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/slab.h>

#include "mdw_mem_dummy.h"
#include "memory_mgt.h"

static int mdw_mem_dmy_map_kva(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_map_iova(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_unmap_iova(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_unmap_kva(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_alloc(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_free(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_import(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_unimport(struct apusys_kmem *mem)
{
	return -ENOMEM;
}

static int mdw_mem_dmy_flush(struct apusys_kmem *mem)
{
	return -ENOMEM;

}

static int mdw_mem_dmy_invalidate(struct apusys_kmem *mem)
{
	return -ENOMEM;

}

static void mdw_mem_dmy_destroy(void)
{
}

struct mdw_mem_ops mem_dmy_ops = {
	.alloc = mdw_mem_dmy_alloc,
	.free = mdw_mem_dmy_free,
	.import = mdw_mem_dmy_import,
	.unimport = mdw_mem_dmy_unimport,
	.flush = mdw_mem_dmy_flush,
	.invalidate = mdw_mem_dmy_invalidate,
	.map_kva = mdw_mem_dmy_map_kva,
	.unmap_kva = mdw_mem_dmy_unmap_kva,
	.map_iova = mdw_mem_dmy_map_iova,
	.unmap_iova = mdw_mem_dmy_unmap_iova,
	.destroy = mdw_mem_dmy_destroy,
};

struct mdw_mem_ops *mdw_mem_dmy_init(void)
{
	return &mem_dmy_ops;
}

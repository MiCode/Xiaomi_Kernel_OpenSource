/*
 * Copyright 2018 GoldenRiver Technologies Co., Ltd. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/io.h>
#include <linux/genalloc.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/trusty_shm.h>
#include <linux/trusty/smcall.h>
#include "trusty-link-shbuf.h"

static struct gen_pool *gpool;
static struct device *gdev; /* trusty dev */

struct trusty_shm_ops {
	void *(*alloc)(size_t size, gfp_t gfp_mask); /* alloc_pages_exact */
	void (*free)(void *vaddr, size_t size); /* free_pages_exact */
	/* arch/arm64/include/asm/memory.h */
	void *(*phys_to_virt)(phys_addr_t x);
	phys_addr_t (*virt_to_phys)(const volatile void *x);
};

static void *default_shm_alloc(size_t sz, gfp_t gfp)
{
	return alloc_pages_exact(sz, gfp);
}

static void default_shm_free(void *va, size_t sz)
{
	free_pages_exact(va, sz);
}

static void *gpool_shm_alloc(size_t size, gfp_t gfp)
{
	unsigned long vaddr;

	BUG_ON(!gpool);
	BUG_ON(!size);

	vaddr = gen_pool_alloc(gpool, size);
	if (!vaddr) {
		dev_err(gdev, "%s: alloc shm failed, request 0x%zx avail 0x%zx\n",
				__func__, size, gen_pool_avail(gpool));
		return NULL;
	}

	if (gfp & __GFP_ZERO)
		memset((void *)vaddr, 0, size);

	/* WARN if other gfp set ? */

	dev_dbg(gdev, "%s: 0x%zx alloced, avail 0x%zx\n",
			__func__, size, gen_pool_avail(gpool));

	return (void *)vaddr;
}

static void gpool_shm_free(void *vaddr, size_t size)
{
	BUG_ON(!gpool);
	BUG_ON(!vaddr);
	BUG_ON(!size);

	gen_pool_free(gpool, (uintptr_t)vaddr, size);

	dev_dbg(gdev, "%s: 0x%zx freed, avail 0x%zx\n",
			__func__, size, gen_pool_avail(gpool));
}

static void *gpool_shm_phys_to_virt(phys_addr_t paddr)
{
	struct gen_pool_chunk *chunk;
	size_t chunk_size = 0;
	unsigned long vaddr = 0;

	BUG_ON(!gpool);

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &gpool->chunks, next_chunk) {
		chunk_size = chunk->end_addr - chunk->start_addr + 1;
		/* check addr is within chunk boundary */
		if (paddr >= chunk->phys_addr &&
				paddr <= (chunk->phys_addr + chunk_size)) {
			vaddr = chunk->start_addr + (paddr - chunk->phys_addr);
			break;
		}
	}
	rcu_read_unlock();

	return (void *)vaddr;
}

static phys_addr_t gpool_shm_virt_to_phys(const volatile void *vaddr)
{
	BUG_ON(!gpool);

	return gen_pool_virt_to_phys(gpool, (uintptr_t)vaddr);
}

static struct trusty_shm_ops shm_ops = {
	.alloc = default_shm_alloc,
	.free = default_shm_free,
	.virt_to_phys = virt_to_phys,
	.phys_to_virt = phys_to_virt,
};

void *trusty_shm_alloc(size_t size, gfp_t gfp)
{
	return shm_ops.alloc(size, gfp);
}

void trusty_shm_free(void *vaddr, size_t size)
{
	shm_ops.free(vaddr, size);
}

void *trusty_shm_phys_to_virt(unsigned long paddr)
{
	return shm_ops.phys_to_virt(paddr);
}

phys_addr_t trusty_shm_virt_to_phys(void *vaddr)
{
	return shm_ops.virt_to_phys(vaddr);
}

int trusty_shm_init_pool(struct device *dev)
{
	int rc;
	unsigned long shm_pa = 0;
	unsigned long shm_va = 0;
	unsigned long shm_size = 0;
	unsigned long shm_ioremap_pa = 0; /* for hee pa fixup */
	struct link_shbuf_data *shbuf_dev = NULL;

	gdev = dev;
	shbuf_dev = trusty_get_link_shbuf_device(0);

	if (shbuf_dev) {
		shm_pa = shbuf_dev->buffer.start + shbuf_dev->ramconsole_size;
		shm_size = resource_size(&shbuf_dev->buffer) - shbuf_dev->ramconsole_size;
		shm_va = (uintptr_t)shbuf_dev->base + shbuf_dev->ramconsole_size;
		shm_ioremap_pa = page_to_phys(virt_to_page(shm_va));

		dev_info(dev, "%s: pa %lx size %lx va %lx remap_pa %lx\n",
				__func__, shm_pa, shm_size, shm_va, shm_ioremap_pa);

		gpool = gen_pool_create(3 /* 8 bytes aligned */, -1);
		if (!gpool) {
			dev_err(dev, "%s: gen_pool_create failed\n", __func__);
			goto err_pool_create;
		}

		gen_pool_set_algo(gpool, gen_pool_best_fit, NULL);
		rc = gen_pool_add_virt(gpool, shm_va, shm_pa, shm_size, -1);
		if (rc < 0) {
			dev_err(dev, "%s: gen_pool_add_virt failed (%d)\n",
					__func__, rc);
			gen_pool_destroy(gpool);
			goto err_pool_add_virt;
		}

		shm_ops.alloc = gpool_shm_alloc;
		shm_ops.free = gpool_shm_free;
		shm_ops.virt_to_phys = gpool_shm_virt_to_phys;
		shm_ops.phys_to_virt = gpool_shm_phys_to_virt;
		/*
		 * ioremap va is not in kernel linear map, so we need
		 * to pass the offset between linear and non-linear
		 * mapping to hee for fixing up.
		 */
		trusty_fast_call32(dev, SMC_FC_IOREMAP_PA_INFO,
				(u32)shm_ioremap_pa,
				(u32)(shm_ioremap_pa >> 32),
				(u32)shm_size);

		dev_info(dev, "shm available, use shm alloc/free\n");
		return 0;
	}

	/* fallback to alloc_pages_exact / free_pages_exact */
err_pool_add_virt:
	if (gpool) {
		gen_pool_destroy(gpool);
		gpool = NULL;
	}
err_pool_create:
	dev_info(dev, "shm not available, fallback to default alloc/free\n");
	return 0;
}

void trusty_shm_destroy_pool(struct device *dev)
{
	if (gpool)
		gen_pool_destroy(gpool);
}
